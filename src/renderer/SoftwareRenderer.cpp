#include "../core/FrameProfile.h"
#include "SoftwareRenderer.h"
#include "Projection.h"
#include "GpuRenderer.h"
#include "../vr/VrRuntime.h"
#include "FrameWorker.h"
#include "MinimapCache.h"
#include "../world/ModelIds.h"
#include <fstream>
#include "../core/PackedResource.h"
#include "../core/Math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <set>
#include <map>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "../ThirdParty/stb/stb_image.h"

namespace retro {
namespace {
constexpr float FOV = 75.0f * kPi / 180.0f;
// Native atlases with crisp texel filtering in the shader: PSX texels stay sharp
// up close and mip-filtered at range. tex_up 2/4 restores the bilinear preview.
int g_texturePresentScale = 1;
bool g_crispTexels = true;
bool g_postProcessing = true; // GPU post pass: occlusion, haze, bloom, grade
bool g_pixelLighting = true; // GPU per-pixel lamps with cube shadow maps
bool g_temporalAA = true; // windowed temporal anti-aliasing
bool g_tiltShift = true; // windowed tilt-shift stylisation
int g_detailLayer = 1; // close-up detail grain on per-pixel-lit surfaces (2/3: debug views)
bool g_postOcclusionView = false;
std::uint32_t rgb(int r,int g,int b){return 0xff000000u | (std::uint32_t(r)<<16) | (std::uint32_t(g)<<8) | std::uint32_t(b); }
// Surface-set and structure finishes, resources 2100-2124 in the order of
// tools/library/surfaces.py: metres per repeat, derived normal strength,
// gloss, and the share of albedo that glows above a luma floor.
struct SurfaceFinish {float repeat,normal,gloss,glow=0,glowFloor=0;};
constexpr int FirstSurfaceFinish=2100;
constexpr SurfaceFinish surfaceFinishes[]={
 {2.4f,.9f,.08f},{2.f,.7f,.12f},{2.4f,.6f,.06f},            // concrete wall, floor, ceiling
 {1.8f,1.f,.08f},{2.f,.8f,.1f},{3.f,.8f,.06f},              // painted brick
 {1.f,.6f,.36f},{2.4f,.6f,.26f},{1.8f,.5f,.04f},            // clinic tile
 {3.f,.7f,.3f},{2.1f,.6f,.3f},{1.8f,.4f,.1f,.45f,.6f},      // lab panel; luminous ceiling panels
 {3.f,1.3f,.1f},{2.5f,1.1f,.04f},{3.f,1.2f,.06f},           // rock
 {2.f,1.f,.24f},{2.f,1.f,.04f},{2.f,.9f,.2f},               // corrugated
 {2.4f,1.f,.06f},{2.f,1.1f,.04f},                           // mine timber walls, gravel (rock ceiling)
 {3.f,1.1f,.08f},{3.f,1.f,.1f},{2.4f,1.2f,.06f},            // ancient stone
 {1.5f,1.2f,.3f},{1.2f,1.f,.45f,.7f,.2f}};                  // resonance growth (34), crystal (35)
constexpr int GrowthFinish=23,CrystalFinish=24;
static_assert(std::size(surfaceFinishes)==CrystalFinish+1);
std::vector<std::string> wrapUiText(std::string remaining,size_t columns,size_t limit=3){
 std::vector<std::string> lines;columns=std::max(size_t(1),columns);
 while(!remaining.empty()&&lines.size()<limit){size_t count=std::min(columns,remaining.size());
  if(count<remaining.size()){auto space=remaining.rfind(' ',count);if(space!=std::string::npos&&space>0)count=space;}
  lines.push_back(remaining.substr(0,count));remaining.erase(0,count);while(!remaining.empty()&&remaining.front()==' ')remaining.erase(0,1);
 }
 if(!remaining.empty()&&!lines.empty()&&columns>=3){auto&last=lines.back();last.resize(std::min(last.size(),columns-3));last+="...";}
 return lines;
}
const char* terminalHeader(const Terminal& terminal){
 if(terminal.reactorAction==1)return "C:\\DEPTHWORKS\\CONTROL> R03.EXE";
 if(terminal.reactorAction)return "C:\\DEPTHWORKS\\CONTROL> COOLANT.EXE";
 if(terminal.activateState)return "C:\\DEPTHWORKS\\CONTROL> LOCAL.EXE";
 if(terminal.control)return "C:\\DEPTHWORKS\\CONTROL> DISPATCH.EXE";
 return "C:\\DEPTHWORKS\\LOGS> TYPE SHIFT.LOG";
}
const char* terminalStatus(const Game& game,const Terminal& terminal){
 if(terminal.activateState){
  if(terminal.requireState&&!game.state(terminal.requireState))return "LOCKED / SEE REQUIRED STEP ABOVE";
  if(game.state(terminal.activateState))return terminal.toggleState?"STATUS: MACHINERY PAUSED / USE CONTROL TO RESUME":"STATUS: COMMAND ACCEPTED";
  return "STATUS: LOCAL CONTROL READY";
 }
 if(terminal.reactorAction){switch(game.world().reactorStage()){
  case World::ReactorStage::NoDisk:return "NEXT: TAKE AUTH FLOPPY FROM UPPER WORKBENCH";
  case World::ReactorStage::DiskHeld:return "NEXT: INSERT AUTH FLOPPY AT THE R-03 COMPUTER";
  case World::ReactorStage::DiskLoaded:return "NEXT: PRIME P-01 FEED ON LOWER FLOOR";
  case World::ReactorStage::FeedPrimed:return "NEXT: PRIME P-02 RETURN ON UPPER FLOOR";
  case World::ReactorStage::ReturnPrimed:return "NEXT: RESET INTERLOCK AT THE R-03 COMPUTER";
  case World::ReactorStage::Released:return "STATUS: CIRCULATION ACCEPTED / BULKHEAD RELEASED";
 }}
 if(terminal.control)return game.world().hasLift()?"STATUS: LIFT DISPATCH / FOLLOW CAB PROCEDURE":game.world().controlReleased()?"STATUS: GANTRY CONTROL RELEASED":"STATUS: GANTRY CONTROL READY";
 return "STATUS: SHIFT LOG / READ ONLY";
}
std::uint32_t sampleBilinear(const std::vector<std::uint32_t>& pixels,int width,int height,float x,float y){
 x=std::clamp(x,0.f,float(width-1));y=std::clamp(y,0.f,float(height-1));
 int x0=int(x),y0=int(y),x1=std::min(width-1,x0+1),y1=std::min(height-1,y0+1);
 float fx=x-x0,fy=y-y0;
 auto mix=[](std::uint32_t a,std::uint32_t b,float t){std::uint32_t out=0;for(int c=0;c<4;++c){int shift=c*8;auto av=(a>>shift)&255u,bv=(b>>shift)&255u;out|=unsigned(av*(1-t)+bv*t)<<shift;}return out;};
 auto a=mix(pixels[size_t(y0*width+x0)],pixels[size_t(y0*width+x1)],fx);
 auto b=mix(pixels[size_t(y1*width+x0)],pixels[size_t(y1*width+x1)],fx);
 return mix(a,b,fy);
}
}

const std::array<std::array<unsigned char,5>, 46> glyphs = {{
{{0x7,0x5,0x5,0x5,0x7}},{{0x2,0x6,0x2,0x2,0x7}},{{0x7,0x1,0x7,0x4,0x7}},{{0x7,0x1,0x7,0x1,0x7}},{{0x5,0x5,0x7,0x1,0x1}},
{{0x7,0x4,0x7,0x1,0x7}},{{0x7,0x4,0x7,0x5,0x7}},{{0x7,0x1,0x2,0x2,0x2}},{{0x7,0x5,0x7,0x5,0x7}},{{0x7,0x5,0x7,0x1,0x7}},
{{0x2,0x5,0x7,0x5,0x5}},{{0x6,0x5,0x6,0x5,0x6}},{{0x3,0x4,0x4,0x4,0x3}},{{0x6,0x5,0x5,0x5,0x6}},{{0x7,0x4,0x6,0x4,0x7}},{{0x7,0x4,0x6,0x4,0x4}},
{{0x3,0x4,0x5,0x5,0x3}},{{0x5,0x5,0x7,0x5,0x5}},{{0x7,0x2,0x2,0x2,0x7}},{{0x1,0x1,0x1,0x5,0x2}},{{0x5,0x5,0x6,0x5,0x5}},{{0x4,0x4,0x4,0x4,0x7}},
{{0x5,0x7,0x7,0x5,0x5}},{{0x5,0x7,0x7,0x7,0x5}},{{0x2,0x5,0x5,0x5,0x2}},{{0x6,0x5,0x6,0x4,0x4}},{{0x2,0x5,0x5,0x3,0x1}},{{0x6,0x5,0x6,0x5,0x5}},
{{0x3,0x4,0x2,0x1,0x6}},{{0x7,0x2,0x2,0x2,0x2}},{{0x5,0x5,0x5,0x5,0x7}},{{0x5,0x5,0x5,0x5,0x2}},{{0x5,0x5,0x7,0x7,0x5}},{{0x5,0x5,0x2,0x5,0x5}},
{{0x5,0x5,0x2,0x2,0x2}},{{0x7,0x1,0x2,0x4,0x7}},{{0x0,0x0,0x0,0x0}},{{0x0,0x2,0x0,0x2,0x0}},
{{0x1,0x1,0x2,0x4,0x4}},{{0x0,0x0,0x7,0x0,0x0}},{{0x0,0x0,0x0,0x0,0x2}},{{0x4,0x2,0x1,0x2,0x4}},
{{0x1,0x2,0x4,0x2,0x1}},{{0x5,0x1,0x2,0x4,0x5}},{{0x1,0x2,0x2,0x2,0x1}},{{0x4,0x2,0x2,0x2,0x4}}
}};
int gi(char c){if(c>='0'&&c<='9')return c-'0'; if(c>='A'&&c<='Z')return 10+c-'A'; if(c==' ')return 36;const char* extra="/-.><%()";auto found=std::strchr(extra,c);return found?38+int(found-extra):37; }

SoftwareRenderer::~SoftwareRenderer()=default;
void SoftwareRenderer::setTexturePresentScale(int scale){
 if(scale!=1&&scale!=2&&scale!=4)return;
 g_texturePresentScale=scale;
}
int SoftwareRenderer::texturePresentScale(){return g_texturePresentScale;}
void SoftwareRenderer::setCrispTexels(bool crisp){g_crispTexels=crisp;}
bool SoftwareRenderer::crispTexels(){return g_crispTexels;}
void SoftwareRenderer::setPostProcessing(bool enabled,bool occlusionView){g_postProcessing=enabled;g_postOcclusionView=occlusionView;}
bool SoftwareRenderer::postProcessing(){return g_postProcessing;}
void SoftwareRenderer::setPixelLighting(bool enabled){g_pixelLighting=enabled;}
bool SoftwareRenderer::pixelLighting(){return g_pixelLighting;}
void SoftwareRenderer::setTemporalAA(bool enabled){g_temporalAA=enabled;}
void SoftwareRenderer::setTiltShift(bool enabled){g_tiltShift=enabled;}
bool SoftwareRenderer::tiltShift(){return g_tiltShift;}
bool SoftwareRenderer::temporalAA(){return g_temporalAA;}
void SoftwareRenderer::setDetailLayer(int mode){g_detailLayer=mode;}
int SoftwareRenderer::detailLayer(){return g_detailLayer;}
bool SoftwareRenderer::enableHardware(void* window){
 if(m_gpu)return true;
 // Delay-load the system Vulkan loader so unsupported machines can still run.
 static HMODULE loader=LoadLibraryW(L"vulkan-1.dll");
 if(!loader){m_gpuName="Software (Vulkan loader unavailable)";std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';return false;}
 try{m_gpu=std::make_unique<GpuRenderer>(static_cast<HWND>(window));
  for(const auto*texture:{&m_ashfallSky,&m_coastSky,&m_coastWater,&m_muzzleFlash,&m_pumpTexture,&m_compressorTexture,&m_pipeTexture,&m_gateTexture,&m_pressureWall,&m_pressureFloor,&m_pressureMetal,&m_transferSign,&m_pumpSign,&m_controlSign,&m_surfaceSign,  &m_liftSign,&m_liftDispatch,&m_wall,&m_floor,&m_metal,&m_serviceFloor,&m_serviceCeiling,&m_officeCarpet,&m_arms,&m_weaponTexture,&m_enemyTexture,&m_waspTexture,&m_bruteTexture,&m_wingTexture,&m_medkitTexture,&m_shellsTexture,&m_barrelTexture,&m_crateTexture,&m_concrete,&m_bulkhead,    &m_hazard,&m_chemicalSign,  &m_signRust,&m_panelMetal,&m_routePaint,&m_redPaint,&m_terminalTexture,&m_cautionSign})m_gpu->prepare(*texture);
  for(const auto*texture:{&m_blood,&m_wardenTexture,&m_mutantTexture,&m_snatcherTexture,&m_chimeraTexture,&m_consoleTexture,&m_feedSign,&m_returnSign,  &m_terrainDirt,&m_terrainRock,&m_coastSand,&m_coastRock,&m_framePaint,&m_yellowSteel,&m_redSteel,&m_paleSteel,&m_panelSteel})m_gpu->prepare(*texture);for(const auto&texture:m_bloodVariants)m_gpu->prepare(texture);
  for(const auto&texture:m_hazmatTextures)m_gpu->prepare(texture);
  // Worker/Witness materials are large too. Deferring their mip construction
  // until the first visible limb caused quarter-second gameplay stalls.
  for(const auto&texture:m_workerTextures)m_gpu->prepare(texture);
  for(const auto&texture:m_staffTextures)m_gpu->prepare(texture);
  if(!m_logo.pixels.empty())m_gpu->prepare(m_logo);
  m_gpu->prepare(m_droneTexture);m_gpu->prepare(m_handgunTexture);
  for(const auto&texture:m_paperProps)m_gpu->prepare(texture);
  for(const auto&role:m_cleanupTextures)for(const auto&texture:role)m_gpu->prepare(texture);
  for(const auto&texture:m_clutterTextures)m_gpu->prepare(texture);for(const auto&entry:m_facilityTextures)m_gpu->prepare(entry.second);
  for(const auto&texture:m_surfaceTextures)m_gpu->prepare(texture);
  for(uint32_t color:{0xffd1f1dau,0xffdf9849u,0xff53aec4u,0xff343834u,0xffb84728u,0xff302c27u}){Texture paint{1,1,{color}};m_gpu->prepare(paint);}
  m_animationWorker=std::make_unique<FrameWorker>();m_gpuName="Vulkan / "+m_gpu->adapter();std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';return true;}
 catch(const std::exception&e){m_gpu.reset();m_gpuName="Software fallback: "+std::string(e.what());std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';return false;}
}
std::uint64_t SoftwareRenderer::vrMirrorFrames()const{return m_gpu?m_gpu->mirrorFrames():0;}
std::array<std::uint64_t,4> SoftwareRenderer::atlasUploadStatistics()const{return m_gpu?m_gpu->uploadStatistics():std::array<std::uint64_t,4>{};}
bool SoftwareRenderer::hardwarePresentsWindow()const{return m_gpu&&(m_gpu->hasSurface()||m_gpu->vrActive());}
SoftwareRenderer::SoftwareRenderer(int w,int h):m_width(w),m_height(h),m_pixels(size_t(w*h)),m_depth(size_t(w),9999.f),m_zbuffer(size_t(w*h),9999.f){m_wall=loadTexture(101);m_floor=loadTexture(102);m_metal=loadTexture(103);m_arms=loadTexture(106);m_weaponTexture=loadTexture(112);m_enemyTexture=loadTexture(113);m_waspTexture=loadTexture(115);m_bruteTexture=loadTexture(117);m_wingTexture=loadTexture(118);
 const char* materialNames[]={"wall_6","wall_7","wall_8","wall_5","floor_1","ceiling_1","vent_1","lamp_1_on","door_1","generator_1","metal_4","metal_3","metal_6","wall_box_2","stairs_1"};
 for(int i=0;i<15;++i)m_facilityTextures.emplace(materialNames[i],loadTexture(172+i));
 m_serviceCeiling=m_facilityTextures.at("ceiling_1");
 auto liftCeiling=[](std::uint32_t&pixel){for(int shift:{0,8,16}){auto channel=(pixel>>shift)&255u;pixel=(pixel&~(255u<<shift))|(std::min(255u,unsigned(channel*1.8f))<<shift);}};
 for(auto&pixel:m_serviceCeiling.pixels)liftCeiling(pixel);for(auto&level:m_serviceCeiling.mips)for(auto&pixel:level)liftCeiling(pixel);
 deriveSurfaceNormal(m_serviceCeiling,.75f);m_serviceCeiling.glossStrength=.24f;
 m_consoleTexture=loadTexture(241);m_wardenTexture=loadTexture(243);m_mutantTexture=loadTexture(273);m_snatcherTexture=loadTexture(2127);m_chimeraTexture=loadTexture(2130);
 for(auto& texture:m_hazmatTextures)texture=loadTexture(246);
 for(int i=0;i<3;++i){m_workerTextures[i]=loadTexture(279+i);deriveSurfaceNormal(m_workerTextures[i],.35f);m_workerTextures[i].glossStrength=.05f;}
 for(int i=0;i<6;++i){auto&texture=m_staffTextures[size_t(i)];texture=loadTexture(i<4?696+i*4:712+(i-4));deriveSurfaceNormal(texture,.35f);texture.glossStrength=i==5?.2f:.05f;}
 for(int role=0;role<6;++role){for(int i=0;i<2;++i){auto&texture=m_cleanupTextures[role][i];texture=loadTexture(305+role*5+i);deriveSurfaceNormal(texture,.25f);texture.glossStrength=.12f;}
   auto joints=loadResource(340+role);uint32_t corners=0;if(joints.size()<104||std::memcmp(joints.data(),"RMC2",4))throw std::runtime_error("Invalid cleanup attachment calibration");std::memcpy(&corners,joints.data()+100,4);
   const Mesh*meshes[]={&m_cleanupMesh,&m_cleanupEngineer,&m_cleanupSharpshooter,&m_cleanupHeavy,&m_cleanupMedic,&m_cleanupRusher};const auto&mesh=*meshes[role];if(corners!=mesh.triangles.size()*3||joints.size()!=104+size_t(corners)*4)throw std::runtime_error("Cleanup attachment topology mismatch");
   std::memcpy(m_cleanupArms[role].data(),joints.data()+4,72);std::memcpy(m_cleanupPalms[role].data(),joints.data()+76,24);m_cleanupWeights[role].resize(corners);std::memcpy(m_cleanupWeights[role].data(),joints.data()+104,size_t(corners)*4);for(const auto&weight:m_cleanupWeights[role])if(weight[3]>1)throw std::runtime_error("Invalid cleanup arm side");}
 m_handgunTexture=loadTexture(338);deriveSurfaceNormal(m_handgunTexture,.3f);m_handgunTexture.glossStrength=.25f;
 m_droneTexture=loadTexture(335);deriveSurfaceNormal(m_droneTexture,.3f);m_droneTexture.glossStrength=.2f;
 {auto emission=loadTexture(336);if(emission.width!=m_droneTexture.width||emission.height!=m_droneTexture.height)throw std::runtime_error("Drone emission dimensions mismatch");m_droneTexture.emission=std::move(emission.pixels);}
 {Texture missing{64,64,std::vector<std::uint32_t>(64*64)};for(int y=0;y<64;++y)for(int x=0;x<64;++x)missing.pixels[size_t(y*64+x)]=((x/8+y/8)&1)?0xff000000u:0xffff00ffu;prepareDecal(missing,false);m_missingTexture=std::move(missing);}
 m_ironTexture=loadTexture(349);attachNormal(m_ironTexture,350,true,0);m_ironTexture.glossStrength=.22f;
 m_darkMetalTexture=loadTexture(351);attachNormal(m_darkMetalTexture,352,true,0);m_darkMetalTexture.glossStrength=.3f;
 m_rubberTexture=loadTexture(353);attachNormal(m_rubberTexture,354,true,0);m_rubberTexture.glossStrength=.06f;
 m_leatherTexture=loadTexture(355);deriveSurfaceNormal(m_leatherTexture,.25f);m_leatherTexture.glossStrength=.12f;
 m_idFabricTexture=loadTexture(356);m_medkitCaseTexture=loadTexture(357);m_medkitLabelTexture=loadTexture(358);
 m_glassTexture=[](){const int n=128;Texture t{n,n,std::vector<std::uint32_t>(size_t(n*n))};
  auto hash=[](int x,int y,unsigned k){unsigned h=unsigned(x)*73856093u^unsigned(y)*19349663u^k;h=(h^(h>>13))*1274126177u;return float((h>>24)&255u)/255.f;};
  auto smooth=[&](float x,float y,unsigned k,int cell){int x0=int(std::floor(x/cell)),y0=int(std::floor(y/cell));float fx=x/cell-x0,fy=y/cell-y0;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
   auto at=[&](int i,int j){return hash((i%(n/cell)+n/cell)%(n/cell),(j%(n/cell)+n/cell)%(n/cell),k);};
   return at(x0,y0)*(1-fx)*(1-fy)+at(x0+1,y0)*fx*(1-fy)+at(x0,y0+1)*(1-fx)*fy+at(x0+1,y0+1)*fx*fy;};
  for(int y=0;y<n;++y)for(int x=0;x<n;++x){
   float dust=smooth(float(x),float(y),0x51u,32)*.6f+smooth(float(x),float(y),0x77u,8)*.4f;
   float settle=float(y)/n;dust*=.55f+.6f*settle;
   // Soft, even film of grime that gathers toward the bottom edge; faint
   // broad wipe marks; no bright specks (they read as stars, not dust).
   float wipe=std::fabs(std::sin((float(x)*.03f+std::sin(float(y)*.02f)*1.2f)));float smear=std::pow(1.f-wipe,10.f)*smooth(float(x),float(y),0x91u,32)*.5f;
   float a=std::clamp(.14f+dust*.1f+smear*.12f+settle*settle*.1f,0.f,.5f);
   float l=std::clamp(.55f+dust*.12f+smear*.1f,0.f,1.f);
   unsigned r=unsigned(l*170.f),g=unsigned(l*190.f),b=unsigned(l*196.f);
   t.pixels[size_t(y*n+x)]=(unsigned(a*255.f)<<24)|(r<<16)|(g<<8)|b;}
  t.transparent=true;t.glossStrength=.9f;return t;}();
 prepareDecal(m_glassTexture,false);
 m_blood=loadTexture(249);for(auto&pixel:m_blood.pixels)if((pixel&0xffffffu)<0x100000u)pixel=0;prepareDecal(m_blood);
 for(int species=0;species<int(creatureVisuals.size());++species){auto&variant=m_bloodVariants[size_t(species)];variant=m_blood;const bool xenoblood=creatureVisuals[size_t(species)].xenoblood;const int tr=xenoblood?128:74,tg=xenoblood?191:12,tb=xenoblood?48:16;
  for(auto&pixel:variant.pixels){float intensity=float(std::max({(pixel>>16)&255u,(pixel>>8)&255u,pixel&255u}))/255.f;auto r=std::uint32_t(tr*intensity),g=std::uint32_t(tg*intensity),b=std::uint32_t(tb*intensity);pixel=(pixel&0xff000000u)|(r<<16)|(g<<8)|b;}prepareDecal(variant);
 }
 m_facilityTextures.emplace("pc_1",loadTexture(192));m_facilityTextures.emplace("keyboard_1",loadTexture(193));
 m_facilityTextures.emplace("machinery_mx_1",loadTexture(143));
 m_facilityTextures.emplace("transformer_box_hr_2",loadTexture(261));
 m_facilityTextures.emplace("metal_hr_6_1",loadTexture(262));
  m_facilityTextures.emplace("wood_1",loadTexture(266));
  m_facilityTextures.emplace("wooden_crate_8",loadTexture(276));
 m_facilityTextures.emplace("van_3",loadTexture(268));
 m_facilityTextures.emplace("metal_2_1",loadTexture(269));
 m_facilityTextures.emplace("vent_4",loadTexture(287));
 m_facilityTextures.emplace("wooden_crate_hx_2_1",loadTexture(289));
 m_facilityTextures.emplace("electronics_etx_part_1",loadTexture(291));
 m_facilityTextures.emplace("keypad_1",loadTexture(293));
 // OFFICE pack models bind by their OBJ usemtl names.
 const char* officeMaterials[]={"desk_2","computer","tv","office_chair_black","file_cabinet_large","file_cabinet","desk_phone","mug","watercooler","coffee_machine","bookcase","Binders"};
 for(int i=0;i<12;++i)m_facilityTextures.emplace(officeMaterials[i],loadTexture(374+i));
 // Mega Pack industrial set pieces bind by their OBJ usemtl names.
 const char* industrialMaterials[]={"metal_hr_1","metal_hr_2","metal_hr_1_dark_smooth","metal_floor_mx_1","grate_hm_1_1","steel_beams_hr_1","pipe_mp_1","valve_1"};
 for(int i=0;i<8;++i)m_facilityTextures.emplace(industrialMaterials[i],loadTexture(407+i));
 // Freight set pieces (models 47-55) from the same library, also by usemtl.
 const char* freightMaterials[]={"shipping_containers_mx_1","metal_barrel_mp_1","vending_machine_1","planks_hr_2","Road_Blocks","concrete_hr_1","gas_cylinder_mx_1"};
 for(int i=0;i<7;++i)m_facilityTextures.emplace(freightMaterials[i],loadTexture(436+i));
 // Metro pack (models 56-61): the subway car and station furniture. Material
 // names carry a metro_ prefix so they never collide with other packs.
 const char* metroMaterials[]={"metro_Door","metro_Floor","metro_Trains","metro_Trains_01","metro_metal","metro_metal_01","metro_metal_02","metro_metal_03","metro_metal_04","metro_seats","metro_automatic_ticket_gate","metro_ticket_machine","metro_trash_can","metro_metro_map","metro_Chair"};
 for(int i=0;i<15;++i)m_facilityTextures.emplace(metroMaterials[i],loadTexture(449+i));
 m_facilityTextures.emplace("metro_Glass",m_glassTexture);
 m_facilityTextures.emplace("metro_Vending_machine",loadTexture(465));
 // Library imports (tools/library/register.py): "material resource" per line.
 {auto text=loadResource(466);std::istringstream lines(std::string(text.begin(),text.end()));std::string material;int resource=0;
  while(lines>>material>>resource)m_facilityTextures.emplace(material,loadTexture(resource));}
 for(int area=0;area<3;++area){m_areaWalls[size_t(area)]=loadTexture(415+area*2);attachNormal(m_areaWalls[size_t(area)],416+area*2,true,0);
  m_areaFloors[size_t(area)]=loadTexture(421+area*2);attachNormal(m_areaFloors[size_t(area)],422+area*2,true,0);}
 m_areaWalls[1].glossStrength=.2f;m_areaWalls[2].glossStrength=.16f;
 // Lit screens: desk CRTs show a green terminal, CCTV sets a camera feed.
 for(auto [material,resource]:{std::pair{"computer",394},std::pair{"tv",395}}){auto&screen=m_facilityTextures.at(material);auto emission=loadTexture(resource);
  if(emission.width!=screen.width||emission.height!=screen.height)throw std::runtime_error("Office screen emission dimensions mismatch");screen.emission=std::move(emission.pixels);}
 m_officeWall=loadTexture(386);attachNormal(m_officeWall,387,true,0);
 m_dropCeiling=loadTexture(388);attachNormal(m_dropCeiling,389,true,0);
 m_securityCarpet=loadTexture(390);attachNormal(m_securityCarpet,391,true,0);
 m_officeTile=loadTexture(392);attachNormal(m_officeTile,393,true,0);m_officeTile.glossStrength=.18f;
 const char* forkliftMaterials[]={"fl_tire.bmp_fl_tire.png","fl_back.bmp_fl_back.png","fl_front.bmp_fl_front.png","fl_grey.bmp_fl_grey.png","fl_lift.bmp_fl_lift.png","fl_top.bmp_fl_top.png","fl_side.bmp_fl_side.png","fl_gate.bmp_fl_gate.png"};
 for(int i=0;i<8;++i)m_facilityTextures.emplace(forkliftMaterials[i],loadTexture(295+i));
 // A pale painted shipping finish retains the supplied atlas's grain,
 // seams and black shipment stamp; the embedded source pixels stay intact.
 {auto&packing=m_facilityTextures.at("wooden_crate_hx_2_1");for(auto&pixel:packing.pixels){
   unsigned gray=((pixel>>16&255)*30+(pixel>>8&255)*59+(pixel&255)*11)/100;
   float variation=(float(gray)-92.f)*.82f;
   auto channel=[&](int base){return unsigned(std::clamp(int(base+variation),0,255));};
   if(gray>30)pixel=(pixel&0xff000000u)|(channel(184)<<16)|(channel(191)<<8)|channel(176);
  }prepareDecal(packing,false);}
 {auto&console=m_facilityTextures.at("electronics_etx_part_1");auto emission=loadTexture(292);
  if(console.width!=emission.width||console.height!=emission.height)throw std::runtime_error("Console emission dimensions mismatch");
  console.emission=std::move(emission.pixels);
  // The CRT face is a separate material in the supplied mesh. Place native
  // green status graphics inside its UV island, preserving the bezel atlas.
  auto screen=console;screen.emission.assign(screen.pixels.size(),0);
  for(int y=327;y<=370;++y)for(int x=377;x<=441;++x)screen.pixels[size_t(y*screen.width+x)]=rgb(8,19+(y%2)*2,14);
  auto mark=[&](int x,int y,std::uint32_t color){size_t i=size_t(y*screen.width+x);screen.pixels[i]=color;screen.emission[i]=color;};
  auto label=[&](int x,int y,const char*str){for(;*str;++str,x+=4){auto glyph=glyphs[size_t(gi(*str))];for(int row=0;row<5;++row)for(int col=0;col<3;++col)if(glyph[row]&(1<<(2-col)))mark(x+col,y+row,rgb(69,178,117));}};
  label(382,332,"SYSTEM 02");label(382,343,"LINK READY");
  for(int row=0;row<3;++row)for(int x=382;x<428-row*9;++x)mark(x,354+row*4,rgb(32,107,73));
  m_facilityTextures.emplace("electronics_etx_part_1_screen",std::move(screen));
 }
 m_officeCarpet=loadTexture(270);deriveSurfaceNormal(m_officeCarpet,.6f);m_officeCarpet.glossStrength=.04f;
 {auto emission=loadTexture(194);auto&lamp=m_facilityTextures.at("lamp_1_on");if(emission.width!=lamp.width||emission.height!=lamp.height)throw std::runtime_error("Lamp emission dimensions mismatch");lamp.emission=std::move(emission.pixels);}
 m_weaponTexture.glossStrength=.42f;m_arms.glossStrength=.06f;deriveSurfaceNormal(m_weaponTexture,.65f);
 m_barrelTexture=loadTexture(122);m_crateTexture=loadTexture(124);m_concrete=loadTexture(125);deriveSurfaceNormal(m_concrete,1.35f);m_bulkhead=loadTexture(126);m_bulkhead.glossStrength=.48f;
 for(int i=0;i<6;++i)m_clutterTextures[i]=loadTexture(152+i*2);
 m_medkitTexture=loadTexture(134);m_shellsTexture=loadTexture(136);
 m_terminalTexture=loadTexture(137);m_cautionSign=loadTexture(138);prepareDecal(m_cautionSign);
 m_muzzleFlash=loadTexture(139);m_muzzleFlash.additive=true;prepareDecal(m_muzzleFlash);
 m_pumpTexture=loadTexture(141);m_compressorTexture=loadTexture(143);m_pipeTexture=loadTexture(145);m_gateTexture=loadTexture(147);
 // Industrial painted steel catches broad fixture highlights without reading as chrome.
 m_pumpTexture.glossStrength=.28f;m_compressorTexture.glossStrength=.36f;
 m_pipeTexture.glossStrength=.42f;m_gateTexture.glossStrength=.32f;
 m_pressureWall=loadTexture(148);m_pressureFloor=loadTexture(149);deriveSurfaceNormal(m_pressureFloor,2.6f);m_pressureMetal=loadTexture(150);
 // Worn paint/concrete remain dielectrics. Smoothness tunes GGX lobe width;
 // keep opaque values below .5 because surface.z also carries blend flags.
 m_wall.glossStrength=.16f;m_floor.glossStrength=.24f;
 m_pressureWall.glossStrength=.20f;m_pressureFloor.glossStrength=.32f;
 m_pressureMetal.glossStrength=.44f;m_concrete.glossStrength=.08f;
 // Physical response is shared by every use of the imported material.
 for(auto&[name,texture]:m_facilityTextures){
  bool wood=name=="wood_1"||name=="wooden_crate_8"||name=="wooden_crate_hx_2_1";
  // This supplied crate atlas averages only 31/255 red. Calibrate its wood
  // albedo before linear lighting; increasing room ambient hid every shadow.
  if(name=="wooden_crate_8"||name=="wood_1"){for(auto&pixel:texture.pixels)for(int shift:{0,8,16}){auto channel=(pixel>>shift)&255u;pixel=(pixel&~(255u<<shift))|(std::min(255u,unsigned(channel*2.2f))<<shift);}prepareDecal(texture,false);}

  // The research-receiving specimen crate and holding cage ship with metal
  // atlases averaging 25-64/255; lit like any other albedo they fall to a flat
  // black silhouette in a lit room. Same calibration as the wood above.
  // The wire-mesh fences and cell bars (mean 13-27/255) get the same lift.
  float steelLift=name=="lab_specimen_crate_metal_crate_3"||name=="med_holding_cage_metal_hr_2"||name=="med_holding_cage_metal_floor_mx_1"?2.6f:name=="mine_mesh_fence_fence"||name=="mine_mesh_doorway_fence"?3.5f:name.starts_with("med_cell_bars")||name=="lab_cell_gate_metal_4"?2.6f:0.f;
  if(steelLift>0.f){for(auto&pixel:texture.pixels)for(int shift:{0,8,16}){auto channel=(pixel>>shift)&255u;pixel=(pixel&~(255u<<shift))|(std::min(255u,unsigned(channel*steelLift))<<shift);}prepareDecal(texture,false);}

  bool wall=name.starts_with("wall_")&&name!="wall_box_2";
  texture.glossStrength=wood?.06f:wall?.10f:name=="pc_1"?.20f:.32f;
  deriveSurfaceNormal(texture,name=="fl_tire.bmp_fl_tire.png"?.18f:wood?.65f:wall?.8f:.45f);
  if(name=="fl_tire.bmp_fl_tire.png")texture.glossStrength=.02f;
  if(name=="electronics_etx_part_1_screen")texture.glossStrength=.34f;
 }

 {auto&computer=m_facilityTextures.at("pc_1");
  if(computer.width!=348||computer.height!=307)throw std::runtime_error("Computer CRT atlas dimensions mismatch");
  // Mesh 191's screen is polygon 86: sampled atlas coordinates
  // x21.38..152.62, y22.72..120.46. No other polygon overlaps this island.
  // Leave its dark rim, bezel, keyboard and casing pixels exactly as supplied.
  // Add phosphor after deriving the original glass/case normals: glyphs must
  // emit light from the display, rather than becoming embossed surface relief.
  computer.emission.assign(computer.pixels.size(),0);
  for(int y=25;y<=117;++y)for(int x=24;x<=149;++x){size_t index=size_t(y*computer.width+x);computer.pixels[index]=rgb(4,13+y%2,9);computer.emission[index]=rgb(1,4+y%2,2);}
  auto mark=[&](int x,int y,std::uint32_t color,std::uint32_t glow){if(x<24||x>149||y<25||y>117)return;size_t index=size_t(y*computer.width+x);computer.pixels[index]=color;computer.emission[index]=glow;};
  auto label=[&](int x,int y,const char* str,int scale,std::uint32_t color,std::uint32_t glow){for(;*str;++str,x+=4*scale){auto glyph=glyphs[size_t(gi(*str))];for(int row=0;row<5;++row)for(int col=0;col<3;++col)if(glyph[row]&(1<<(2-col)))for(int j=0;j<scale;++j)for(int i=0;i<scale;++i)mark(x+col*scale+i,y+row*scale+j,color,glow);}};
  auto green=rgb(38,132,78),emission=rgb(19,93,49),dim=rgb(23,84,49),dimEmission=rgb(9,45,23);
  label(30,30,"DEPTHWORKS",2,rgb(59,178,103),rgb(34,128,68));
  for(int x=30;x<=143;++x)mark(x,43,dim,dimEmission);
  label(30,48,"LOCAL RECORDS",1,green,emission);label(30,58,"> TERMINAL READY",1,green,emission);
  label(34,73,"01 OPERATIONS",1,dim,dimEmission);label(34,83,"02 MAINTENANCE",1,dim,dimEmission);label(34,93,"03 TRANSFER LOG",1,dim,dimEmission);
  for(int x=30;x<=143;++x)mark(x,104,dim,dimEmission);
  label(30,109,">",1,green,emission);for(int y=109;y<114;++y)for(int x=38;x<41;++x)mark(x,y,green,emission);
  prepareDecal(computer,false);
 }

 m_ashfallSky=loadTexture(252);if(std::abs(m_ashfallSky.width*3-m_ashfallSky.height*4)<=4)m_ashfallSky.clampEdges=true;else prepareDecal(m_ashfallSky,false);
 m_terrainDirt=loadTexture(253);attachNormal(m_terrainDirt,254,true,0);
 m_terrainRock=loadTexture(255);attachNormal(m_terrainRock,256);
 // Coastal materials retain the source's coarse texture and normal relief,
 // but shift its albedo toward pale stone and sand without new packed assets.
 auto coastalTint=[](Texture& texture,int red,int green,int blue){
  auto remap=[&](std::uint32_t pixel){int gray=int(((pixel>>16)&255)*.30f+((pixel>>8)&255)*.58f+(pixel&255)*.12f);
   float variation=(gray-100.f)*.60f;auto channel=[&](int base){return std::uint32_t(std::clamp(int(base+variation),0,255));};
   return (pixel&0xff000000u)|(channel(red)<<16)|(channel(green)<<8)|channel(blue);
  };
  for(auto& pixel:texture.pixels)pixel=remap(pixel);
  for(auto& mip:texture.mips)for(auto& pixel:mip)pixel=remap(pixel);
 };
 m_coastRock=m_terrainRock;coastalTint(m_coastRock,205,199,185);
 m_coastSand=m_terrainDirt;coastalTint(m_coastSand,195,181,152);
 // A lightweight wraparound panorama: bright coastal haze with broad cloud
 // forms, generated here so the same sky ships inside the standalone EXE.
 m_coastSky={512,256,std::vector<std::uint32_t>(512*256)};
 auto hash=[](int i,int j)->float{
  unsigned n=unsigned(i*374761393+j*668265263);n=(n^(n>>13))*1274126177u;
  return float(n&2047u)/2047.f;
 };
 auto vnoise=[&](float px,float py){
  int ix=int(std::floor(px)),iy=int(std::floor(py));
  float fx=px-float(ix),fy=py-float(iy);
  fx=fx*fx*(3.f-2.f*fx);fy=fy*fy*(3.f-2.f*fy);
  float bl=hash(ix,iy),br=hash(ix+1,iy),tl=hash(ix,iy+1),tr=hash(ix+1,iy+1);
  return (bl*(1.f-fx)+br*fx)*(1.f-fy)+(tl*(1.f-fx)+tr*fx)*fy;
 };
 for(int y=0;y<256;++y)for(int x=0;x<512;++x){
  float v=float(y)/255.f,lon=float(x)/512.f*2.f*kPi;
  float skyH=std::clamp((.5f-v)*2.f,0.f,1.f);
  float skyR=185.f+(52.f-185.f)*std::pow(skyH,.75f);
  float skyG=212.f+(125.f-212.f)*std::pow(skyH,.75f);
  float skyB=242.f+(220.f-242.f)*std::pow(skyH,.75f);
  if(v>.5f){
   float below=std::clamp((v-.5f)*4.f,0.f,1.f);
   skyR=skyR*(1.f-below)+165.f*below;skyG=skyG*(1.f-below)+195.f*below;skyB=skyB*(1.f-below)+222.f*below;
  }
  float elev=(.5f-v)*kPi*.5f;
  Point3 dir{std::cos(elev)*std::cos(lon),std::cos(elev)*std::sin(lon),std::sin(elev)};
  Point3 sun{std::cos(.65f)*std::cos(1.8f),std::cos(.65f)*std::sin(1.8f),std::sin(.65f)};
  float cosTheta=std::max(0.f,dir.x*sun.x+dir.y*sun.y+dir.z*sun.z);
  float aureole=std::pow(cosTheta,14.f)*.45f+std::pow(cosTheta,64.f)*.65f;
  skyR+=aureole*80.f;skyG+=aureole*65.f;skyB+=aureole*35.f;
  if(v<.48f){
   float planeDist=.20f/std::max(.5f-v,.04f);
   float cx=lon*2.2f+dir.x*planeDist*.35f,cy=dir.y*planeDist*.35f;
   float n=vnoise(cx*1.8f,cy*1.8f)*.58f+vnoise(cx*3.6f,cy*3.6f)*.28f+vnoise(cx*7.2f,cy*7.2f)*.14f;
   float cloud=std::clamp((n-.47f)*4.2f,0.f,1.f)*std::clamp((.48f-v)*6.f,0.f,1.f);
   if(cloud>0.f){
    float lit=std::clamp(.65f+cosTheta*.35f+(n-.47f)*.8f,0.f,1.f);
    float cr=180.f+lit*70.f,cg=198.f+lit*54.f,cb=225.f+lit*30.f;
    skyR=skyR*(1.f-cloud)+cr*cloud;skyG=skyG*(1.f-cloud)+cg*cloud;skyB=skyB*(1.f-cloud)+cb*cloud;
   }
  }
  int ir=std::clamp(int(skyR),0,255),ig=std::clamp(int(skyG),0,255),ib=std::clamp(int(skyB),0,255);
  m_coastSky.pixels[size_t(y*512+x)]=rgb(ir,ig,ib);
 }
 prepareDecal(m_coastSky,false);
 m_water=loadTexture(250);attachNormal(m_water,251,true,0);m_water.transparent=true;m_water.glossStrength=.92f;auto coolantTint=[](uint32_t pixel){return 0xd6000000u|((pixel>>16&255)*90/100<<16)|((pixel>>8&255)*92/100<<8)|((pixel&255)*80/100);};for(auto& pixel:m_water.pixels)pixel=coolantTint(pixel);for(auto& mip:m_water.mips)for(auto& pixel:mip)pixel=coolantTint(pixel);attachNormal(m_wall,187);attachNormal(m_pressureWall,188);attachNormal(m_bulkhead,189);attachNormal(m_floor,190,true,0);
 m_serviceFloor=m_floor;
 auto liftServiceFloor=[](std::uint32_t&pixel){for(int shift:{0,8,16}){auto channel=(pixel>>shift)&255u;pixel=(pixel&~(255u<<shift))|(std::min(255u,unsigned(channel*1.45f))<<shift);}};
 for(auto&pixel:m_serviceFloor.pixels)liftServiceFloor(pixel);for(auto&level:m_serviceFloor.mips)for(auto&pixel:level)liftServiceFloor(pixel);
 m_serviceFloor.glossStrength=.42f;
 m_coastWater=loadTexture(250);attachNormal(m_coastWater,251,true,0);m_coastWater.transparent=true;m_coastWater.glossStrength=.90f;
 auto seaTint=[](std::uint32_t pixel){int gray=int(((pixel>>16)&255)*.30f+((pixel>>8)&255)*.59f+(pixel&255)*.11f);int delta=(gray-100)/3;
  return 0x58000000u|(std::uint32_t(std::clamp(32+delta,0,255))<<16)|(std::uint32_t(std::clamp(92+delta,0,255))<<8)|std::uint32_t(std::clamp(148+delta,0,255));};
for(auto&pixel:m_coastWater.pixels)pixel=seaTint(pixel);for(auto&mip:m_coastWater.mips)for(auto&pixel:mip)pixel=seaTint(pixel);
 m_hazard=loadTexture(127);m_chemicalSign=loadTexture(128);m_signRust=loadTexture(131);m_panelMetal=loadTexture(132);
 // Keep the source steel's rust, scratches and tonal variation. Compressing
 // a dark panel into a narrow blue tint made structural beams look untextured.
 m_framePaint=m_pressureMetal;
 // Charcoal protective finish keeps rack uprights distinct from cargo and
 // concrete. Preserve the oxide/scratch variation instead of flattening it.
 for(auto& pixel:m_framePaint.pixels){
  unsigned r=(pixel>>16)&255,g=(pixel>>8)&255,b=pixel&255;
  unsigned gray=(r*30+g*59+b*11)/100;
  pixel=0xff000000u|((unsigned(r*.22f+gray*.24f))<<16)|((unsigned(g*.25f+gray*.26f))<<8)|unsigned(b*.25f+gray*.26f);
 }
 deriveSurfaceNormal(m_framePaint,.65f);m_framePaint.glossStrength=.18f;
 // Opaque worn industrial paint. Broad housings need real material coverage;
 // the transparent floor-marking decals are unsuitable for their surfaces.
 // The panel-metal source is almost uniform (std dev 2.5/255), so paint over
 // it came out one flat tone. Take the grain from the rusted pressure-works
 // plate instead, as a deviation from its mean so overall brightness is kept.
 auto paintedSteel=[&](unsigned pigment){
  Texture result=m_pressureMetal;
  for(auto& pixel:result.pixels){
   float grain=float(((pixel>>16)&255)+((pixel>>8)&255)+(pixel&255))/765.f;
   unsigned value=0xff000000u;
   for(int channel=0;channel<3;++channel){
    float paint=float((pigment>>(channel*8))&255)*std::clamp(.46f+(grain-.52f)*2.2f,.2f,.95f);
    float exposed=float((pixel>>(channel*8))&255)*.5f;
    value|=unsigned(std::clamp(paint*.9f+exposed*.1f,0.f,255.f))<<(channel*8);
   }
   pixel=value;
  }
  result.mips.clear();result.normalLevels.clear();result.relief.clear();
  prepareDecal(result,true);deriveSurfaceNormal(result,1.f);result.glossStrength=.34f;
  return result;
 };
 m_yellowSteel=paintedSteel(0xffb79b39u);m_redSteel=paintedSteel(0xffa34330u);m_paleSteel=paintedSteel(0xffb4bcb1u);
 // Structure material 2 (ducts, enclosures, housings) was the bare near-black
 // panel metal; give it the same worn grey-green paint.
 m_panelSteel=paintedSteel(0xff6d7a70u);
 for(auto*decal:{&m_hazard,&m_chemicalSign})prepareDecal(*decal);
 m_routePaint=makePaint(0xffb99348u);m_redPaint=makePaint(0xff954732u);
 m_transferSign=makeSign("PRESSURE WORKS","TRANSFER / 02",0xffa7a766u);m_pumpSign=makeSign("PUMP HALL","HIGH PRESSURE",0xffa7a766u);
 m_controlSign=makeSign("CONTROL GALLERY","SWITCHGEAR",0xffa7a766u);m_surfaceSign=makeSign("SURFACE LIFT","EXTRACTION",0xffa7a766u);
 m_liftSign=makeSign("FREIGHT / 03","MAX LOAD 4000 KG",0xffd7ac64u);m_liftDispatch=makeSign("SURFACE / UP","DISPATCH CONTROL",0xff9fceaeu);
 m_feedSign=makeSign("FEED","P-01",0xffd7ac64u);m_returnSign=makeSign("RETURN","P-02",0xff53aec4u);
 for(int kind=0;kind<3;++kind)m_paperProps[size_t(kind)]=makePaperProp(Readable(kind+1));
 m_noticeSigns[0]=makeSign("FREIGHT SERVICES","ACCESS SUSPENDED",0xffa7a766u);m_noticeSigns[1]=makeSign("PRIMARY UTILITIES","SERVICE ACCESS",0xffa7a766u);
 m_serviceAreaSigns[0]=makeSign("SERVICE STORES","SPARES / MAINTENANCE",0xff53aec4u);
 m_serviceAreaSigns[1]=makeSign("PRIMARY UTILITIES","PERMIT REQUIRED",0xffd7ac64u);
 m_serviceAreaSigns[2]=makeSign("SALVAGE DISPATCH","WASTE HANDLING",0xff9fceaeu);
 // Surface sets (World.h Surface) and structure finishes 26-35.
 static_assert(std::tuple_size_v<decltype(m_surfaceTextures)> ==std::size(surfaceFinishes));
 for(size_t i=0;i<m_surfaceTextures.size();++i)m_surfaceTextures[i]=loadSurfaceFinish(int(i));
 auto surface=[&](int set,int wall,int floor,int ceiling){m_surfaceSets[size_t(set)]={&m_surfaceTextures[size_t(wall)],&m_surfaceTextures[size_t(floor)],&m_surfaceTextures[size_t(ceiling)],
  {surfaceFinishes[wall].repeat,surfaceFinishes[wall].repeat},surfaceFinishes[floor].repeat,surfaceFinishes[ceiling].repeat};};
 surface(SurfaceConcrete,0,1,2);surface(SurfaceBrick,3,4,5);surface(SurfaceClinicTile,6,7,8);surface(SurfaceLabPanel,9,10,11);
 surface(SurfaceRock,12,13,14);surface(SurfaceCorrugated,15,16,17);surface(SurfaceMineTimber,18,19,14);surface(SurfaceStone,20,21,22);
 // Pressure and office plaster are the finishes maps already use, at their repeats.
 m_surfaceSets[SurfacePressure]={&m_pressureWall,&m_pressureFloor,&m_facilityTextures.at("ceiling_1"),{2,3},.5f,.5f};
 m_surfaceSets[SurfaceOfficePlaster]={&m_officeWall,&m_securityCarpet,&m_dropCeiling,{1,1},1,1};
 m_logo=loadEmbeddedPng(2);m_logo.clampEdges=true;
}
SoftwareRenderer::Texture SoftwareRenderer::loadSurfaceFinish(int index){
 const auto& finish=surfaceFinishes[size_t(index)];
 auto texture=loadTexture(FirstSurfaceFinish+index);
 if(texture.width!=256||texture.height!=256)throw std::runtime_error("Surface finish "+std::to_string(FirstSurfaceFinish+index)+" must be 256 x 256");
 deriveSurfaceNormal(texture,finish.normal);texture.glossStrength=finish.gloss;
 // The lab ceiling glows through its diffuser panels, crystal from inside its
 // facets: emission follows the albedo above a luma floor, like material 25.
 if(finish.glow>0){texture.emission.resize(texture.pixels.size());
  for(size_t i=0;i<texture.pixels.size();++i){auto p=texture.pixels[i];float r=float((p>>16)&255),g=float((p>>8)&255),b=float(p&255);
   float k=finish.glow*std::clamp(((r*.3f+g*.59f+b*.11f)/255.f-finish.glowFloor)/(1-finish.glowFloor),0.f,1.f);
   texture.emission[i]=0xff000000u|(unsigned(r*k)<<16)|(unsigned(g*k)<<8)|unsigned(b*k);}}
 return texture;
}
const SoftwareRenderer::Texture* SoftwareRenderer::finishMaterial(int material)const{
 switch(material){
  case 26:return m_surfaceSets[SurfaceRock].wall;
  case 27:return m_surfaceSets[SurfaceClinicTile].wall;
  case 28:return m_surfaceSets[SurfaceLabPanel].wall;
  case 29:return m_surfaceSets[SurfaceCorrugated].wall;
  case 31:return m_surfaceSets[SurfaceBrick].wall;
  case 32:return m_surfaceSets[SurfaceMineTimber].wall;
  case 33:return m_surfaceSets[SurfaceStone].wall;
  case 34:return &m_surfaceTextures[GrowthFinish];
  case 35:return &m_surfaceTextures[CrystalFinish];
 }
 return nullptr;
}
Vec2 SoftwareRenderer::finishRepeat(const Texture& texture)const{
 for(size_t i=0;i<m_surfaceTextures.size();++i)if(&texture==&m_surfaceTextures[i])return {surfaceFinishes[i].repeat,surfaceFinishes[i].repeat};
 return {};
}
const SoftwareRenderer::Texture& SoftwareRenderer::facilityTexture(int mesh,int part)const{
 if(mesh<0||mesh>=int(m_facilityMeshes.size()))throw std::runtime_error("Invalid facility mesh index "+std::to_string(mesh));
 const auto& model=m_facilityMeshes[size_t(mesh)];
 if(part>=0&&part<int(model.materialNames.size())){
  const auto& name=model.materialNames[size_t(part)];
  auto found=m_facilityTextures.find(name);if(found!=m_facilityTextures.end())return found->second;
 }
 // The three service-map FBX files each ship with one documented texture, but
 // their internal material labels are exporter metadata and are not guaranteed
 // to match the PNG stem. Bind those meshes by authored asset identity instead
 // of crashing when ufbx reports a different material name.
 static constexpr const char* serviceTexture[]={"machinery_mx_1","transformer_box_hr_2","metal_hr_6_1"};
 if(mesh>=12&&mesh<=14)return m_facilityTextures.at(serviceTexture[mesh-12]);
 if(mesh==15||mesh==18||mesh==19)return m_facilityTextures.at("wood_1");
 if(mesh==16)return m_facilityTextures.at("van_3");
 if(mesh==17)return m_facilityTextures.at("wooden_crate_8");
 if(mesh==20)return m_facilityTextures.at("vent_4");
 if(mesh==21)return m_facilityTextures.at("wooden_crate_hx_2_1");
 // Blender's FBX includes an untextured lever material named "Material".
 // Its handle and housing use the supplied console atlas, while the screen
 // and keypad materials bind separately through their source names above.
 if(mesh==22)return m_facilityTextures.at("electronics_etx_part_1");
 // Unresolved materials draw as the missing-texture checker instead of
 // dropping the GPU session; each one is listed once for replacement.
 std::string material=part>=0&&part<int(model.materialNames.size())?model.materialNames[size_t(part)]:"<invalid part>";
 static std::mutex reportedLock;static std::set<std::string> reported;
 {std::lock_guard lock(reportedLock);if(reported.insert(std::to_string(mesh)+"/"+material).second)std::ofstream("RawMetal-missing-textures.txt",std::ios::app)<<"facility mesh "<<mesh<<", part "<<part<<", material "<<material<<'\n';}
 return m_missingTexture;
}
SoftwareRenderer::Texture SoftwareRenderer::makePaint(std::uint32_t color){
 Texture paint{64,64,std::vector<std::uint32_t>(64*64)};
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto rust=sample(m_signRust,x/64.f,y/64.f);unsigned wear=(rust>>16)&255;auto p=shade(color,.58f+wear/200.f);paint.pixels[y*64+x]=(p&0xffffffu)|(wear>97?0u:0xff000000u);}
 prepareDecal(paint,false);return paint;
}
void SoftwareRenderer::prepareDecal(Texture&texture,bool clampEdges){
 texture.clampEdges=clampEdges;texture.mips.clear();int width=texture.width,height=texture.height;auto pixels=texture.pixels;
 while(width>1||height>1){int nextWidth=std::max(1,width/2),nextHeight=std::max(1,height/2);std::vector<std::uint32_t> next(size_t(nextWidth*nextHeight));
  for(int y=0;y<nextHeight;++y)for(int x=0;x<nextWidth;++x){unsigned channels[4]{};for(int j=0;j<2;++j)for(int i=0;i<2;++i){auto p=pixels[std::min(height-1,y*2+j)*width+std::min(width-1,x*2+i)];for(int c=0;c<4;++c)channels[c]+=(p>>(c*8))&255;}
   next[y*nextWidth+x]=(channels[0]/4)|((channels[1]/4)<<8)|((channels[2]/4)<<16)|((channels[3]/4)<<24);
  }texture.mips.push_back(next);pixels=std::move(next);width=nextWidth;height=nextHeight;
 }
}
// Material: 0 body, 1 gloves, 2 boots, 3 respirator (Scene3D's civilian parts).
const SoftwareRenderer::Texture& SoftwareRenderer::civilianTexture(int body,int material)const{
 if(body<=0)return m_workerTextures[size_t(std::clamp(material,0,2))];
 if(body<5)return m_staffTextures[size_t(body-1)];
 return material==1||material==2?m_workerTextures[size_t(material)]:m_staffTextures[material==3?5:4];
}
SoftwareRenderer::Texture SoftwareRenderer::loadTexture(int id){
 auto resource=loadResource(id);
 Texture t; int channels=0;
 bool raw=resource.size()>=12&&!std::memcmp(resource.data(),"RMT1",4);
 if(raw){std::memcpy(&t.width,resource.data()+4,4);std::memcpy(&t.height,resource.data()+8,4);
  if(t.width<=0||t.height<=0||uint64_t(t.width)*t.height*4!=resource.size()-12)throw std::runtime_error("Invalid packed texture dimensions");}
 auto pixels=raw?resource.data()+12:stbi_load_from_memory(resource.data(),int(resource.size()),&t.width,&t.height,&channels,4);
 if(!pixels)throw std::runtime_error("Invalid embedded texture");
 t.pixels.resize(size_t(t.width)*size_t(t.height));
 for(size_t i=0;i<t.pixels.size();++i)t.pixels[i]=(std::uint32_t(pixels[i*4+3])<<24)|(rgb(pixels[i*4],pixels[i*4+1],pixels[i*4+2])&0xffffffu);
 if(!raw)stbi_image_free(pixels); return t;
}
SoftwareRenderer::Texture SoftwareRenderer::loadEmbeddedPng(int id){
 // Branding assets ride as raw RCDATA beside the ICON — not RMZ1 pack slots.
 auto module=GetModuleHandleW(nullptr);auto res=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
 if(!res)throw std::runtime_error("Missing embedded branding PNG");
 auto handle=LoadResource(module,res);auto bytes=static_cast<const unsigned char*>(LockResource(handle));auto size=SizeofResource(module,res);
 if(!bytes||!size)throw std::runtime_error("Empty embedded branding PNG");
 Texture t;int channels=0;auto pixels=stbi_load_from_memory(bytes,int(size),&t.width,&t.height,&channels,4);
 if(!pixels||t.width<=0||t.height<=0)throw std::runtime_error("Invalid embedded branding PNG");
 t.pixels.resize(size_t(t.width)*size_t(t.height));
 for(size_t i=0;i<t.pixels.size();++i)t.pixels[i]=(std::uint32_t(pixels[i*4+3])<<24)|(rgb(pixels[i*4],pixels[i*4+1],pixels[i*4+2])&0xffffffu);
 stbi_image_free(pixels);return t;
}
std::uint32_t SoftwareRenderer::sample(const Texture&t,float u,float v,float lod){
 if(t.clampEdges){u=std::clamp(u,0.f,1.f);v=std::clamp(v,0.f,1.f);}else{u-=std::floor(u);v-=std::floor(v);}
 if(t.mips.empty())return t.pixels[size_t(std::min(t.height-1,int(v*t.height))*t.width+std::min(t.width-1,int(u*t.width)))];
 lod=std::clamp(lod,0.f,float(t.mips.size()));int level=int(lod);
 auto fetch=[&](int index){int width=std::max(1,t.width>>index),height=std::max(1,t.height>>index);auto&pixels=index?t.mips[index-1]:t.pixels;return pixels[size_t(std::min(height-1,int(v*height))*width+std::min(width-1,int(u*width)))];};
 auto a=fetch(level);if(size_t(level)==t.mips.size())return a;auto b=fetch(level+1);float blend=lod-level;std::uint32_t result=0;
 for(int channel=0;channel<4;++channel){int shift=channel*8;auto value=unsigned(((a>>shift)&255)*(1-blend)+((b>>shift)&255)*blend);result|=value<<shift;}return result;
}
void SoftwareRenderer::drawSky(const Game& game){
 const auto& world=game.world();
 if(!world.outdoors()||std::strcmp(world.skyboxId(),"brutal_wasteland")!=0)return;
 const auto& sky=world.coast()?m_coastSky:m_ashfallSky;
 if(sky.pixels.empty())return;
 // Support the two common single-image sky formats used by the supplied pack:
 // a 4x3 horizontal cube cross, or an equirectangular panorama. The sky is
 // camera-centred and goes through the normal triangle path, so software and
 // Vulkan render the same environment without a second texture implementation.
 constexpr float radius=80.f;const auto& player=m_view;Point3 eye{player.pos.x,player.pos.y,player.z+player.eye};
 auto point=[&](Point3 direction,float u,float v){
  auto camera=cameraPoint(eye+direction*radius,game);
  return MeshVertex{camera,u,v,1.f+std::max(0.f,camera.z)*.018f};
 };
 if(std::abs(sky.width*3-sky.height*4)<=4){
  struct Face{int column,row;Point3 tl,tr,br,bl;};
  // Standard horizontal cross, remapped from Y-up cubemap coordinates to the
  // game's Z-up world: up / left-front-right-back / down.
  const Face faces[]={
   {1,0,{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}},
   {0,1,{-1,-1,1},{-1,1,1},{-1,1,-1},{-1,-1,-1}},
   {1,1,{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1}},
   {2,1,{1,1,1},{1,-1,1},{1,-1,-1},{1,1,-1}},
   {3,1,{1,-1,1},{-1,-1,1},{-1,-1,-1},{1,-1,-1}},
   {1,2,{-1,1,-1},{1,1,-1},{1,-1,-1},{-1,-1,-1}}
  };
  const float du=1.f/4,dv=1.f/3;
  for(const auto& face:faces){
   float u0=face.column*du,u1=(face.column+1)*du,v0=face.row*dv,v1=(face.row+1)*dv;
   auto a=point(face.tl,u0,v0),b=point(face.tr,u1,v0),c=point(face.br,u1,v1),d=point(face.bl,u0,v1);
   triangle3D(a,b,c,sky,1.f);triangle3D(a,c,d,sky,1.f);
  }
  return;
 }
 constexpr int slices=32,bands=16;
 for(int y=0;y<bands;++y){
  float v0=float(y)/bands,v1=float(y+1)/bands;
  float lat0=kPi*.5f-v0*kPi,lat1=kPi*.5f-v1*kPi;
  for(int x=0;x<slices;++x){
   float u0=float(x)/slices,u1=float(x+1)/slices;
   float lon0=(u0-.5f)*2*kPi,lon1=(u1-.5f)*2*kPi;
   auto direction=[](float longitude,float latitude){float ring=std::cos(latitude);return Point3{ring*std::cos(longitude),ring*std::sin(longitude),std::sin(latitude)};};
   auto a=point(direction(lon0,lat0),u0,v0),b=point(direction(lon1,lat0),u1,v0);
   auto c=point(direction(lon1,lat1),u1,v1),d=point(direction(lon0,lat1),u0,v1);
   triangle3D(a,b,c,sky,1.f);triangle3D(a,c,d,sky,1.f);
  }
 }
}
void SoftwareRenderer::clear(std::uint32_t c){
 // A Vulkan window frame clears the scene attachment in its render pass. The
 // CPU pixel buffer is only the transparent HUD overlay and is cleared just
 // before that overlay is drawn, so clearing it to the scene background here
 // is a redundant full-frame write. Keep the clear for captures and software.
 if(m_gpuFrame&&hardwarePresentsWindow())return;
 std::fill(m_pixels.begin(),m_pixels.end(),c);
}
void SoftwareRenderer::put(int x,int y,std::uint32_t c){if(x>=0&&y>=0&&x<m_width&&y<m_height)m_pixels[size_t(y*m_width+x)]=c;}
void SoftwareRenderer::rect(int x,int y,int w,int h,std::uint32_t c){
 if(w<=0||h<=0)return;
 int left=std::max(0,x),top=std::max(0,y);
 int right=int(std::min<int64_t>(m_width,int64_t(x)+w)),bottom=int(std::min<int64_t>(m_height,int64_t(y)+h));
 if(left>=right||top>=bottom)return;
 for(int row=top;row<bottom;++row)std::fill_n(m_pixels.data()+size_t(row)*m_width+left,right-left,c);
}
std::uint32_t SoftwareRenderer::shade(std::uint32_t c,float s)const{int r=int(((c>>16)&255)*s),g=int(((c>>8)&255)*s),b=int((c&255)*s);return rgb(std::clamp(r,0,255),std::clamp(g,0,255),std::clamp(b,0,255));}
void SoftwareRenderer::text(int x,int y,const char* s,std::uint32_t c,int sc){for(;*s;++s,x+=4*sc){auto g=glyphs[size_t(gi(char(std::toupper((unsigned char)*s))))];for(int yy=0;yy<5;++yy)for(int xx=0;xx<3;++xx)if(g[size_t(yy)]&(1<<(2-xx)))rect(x+xx*sc,y+yy*sc,sc,sc,c);}}
// UI type: installed Windows faces rasterised once per size into grayscale
// coverage, then alpha-blended. The HUD texture's alpha reaches the
// composite pass, so translucent panels and soft glyph edges survive.
namespace {
struct UiGlyph{int advance=0;std::vector<std::uint8_t> alpha;};
struct UiFace{int width=0,height=0;std::array<UiGlyph,96> glyphs;};
const UiFace& uiFace(int face,int size){
 static std::map<int,UiFace> faces;static std::mutex guard;std::lock_guard lock(guard);
 if(auto found=faces.find(face*1024+size);found!=faces.end())return found->second;
 auto& out=faces[face*1024+size];
 static const wchar_t* names[]={L"Bahnschrift SemiBold SemiConden",L"Bahnschrift Light SemiCondensed",L"Impact",L"Bahnschrift SemiCondensed"};
 HDC dc=CreateCompatibleDC(nullptr);
 HFONT font=CreateFontW(-size,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,names[std::clamp(face,0,3)]);
 auto oldFont=SelectObject(dc,font);TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);
 out.height=metrics.tmHeight;out.width=size*2+8;
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=out.width;info.bmiHeader.biHeight=-out.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto oldBitmap=SelectObject(dc,bitmap);
 SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
 for(int c=32;c<128;++c){
  std::memset(bits,0,size_t(out.width)*out.height*4);wchar_t ch=wchar_t(c);
  SIZE extent{};GetTextExtentPoint32W(dc,&ch,1,&extent);TextOutW(dc,4,0,&ch,1);GdiFlush();
  auto& glyph=out.glyphs[size_t(c-32)];glyph.advance=extent.cx;glyph.alpha.resize(size_t(out.width)*out.height);
  auto* pixels=static_cast<const std::uint32_t*>(bits);for(size_t i=0;i<glyph.alpha.size();++i)glyph.alpha[i]=std::uint8_t((pixels[i]>>8)&255);
 }
 SelectObject(dc,oldBitmap);SelectObject(dc,oldFont);DeleteObject(bitmap);DeleteObject(font);DeleteDC(dc);
 return out;
}
unsigned uiGlyphIndex(char c){unsigned v=(unsigned char)c;return v<32||v>127?'?'-32:v-32;}
}
int SoftwareRenderer::uiTextWidth(const char* s,int face,int size,int tracking)const{auto&f=uiFace(face,size);int w=0;for(;*s;++s)w+=f.glyphs[uiGlyphIndex(*s)].advance+tracking;return w;}
int SoftwareRenderer::uiText(int x,int y,const char* s,std::uint32_t c,int face,int size,float alpha,int tracking){
 auto&f=uiFace(face,size);int pen=x;
 for(;*s;++s){auto&g=f.glyphs[uiGlyphIndex(*s)];
  for(int yy=0;yy<f.height;++yy)for(int xx=0;xx<f.width;++xx)if(auto cover=g.alpha[size_t(yy)*f.width+xx])blend(pen-4+xx,y+yy,c,alpha*cover/255.f);
  pen+=g.advance+tracking;}
 return pen-x;
}
// Drop-in for the 5-pixel font on screens that moved to UI type.
void SoftwareRenderer::label(int x,int y,const char* s,std::uint32_t c,int scale){
 if(scale>=3)uiText(x,y-7,s,c,0,26);else if(scale==2)uiText(x,y-4,s,c,0,17);else uiText(x,y-3,s,c,0,11,1,1);
}
void SoftwareRenderer::blend(int x,int y,std::uint32_t c,float alpha){
 if(alpha<=0||x<0||y<0||x>=m_width||y>=m_height)return;alpha=std::min(alpha,1.f);
 auto& d=m_pixels[size_t(y)*m_width+x];float below=float(d>>24)/255.f*(1-alpha),out=alpha+below;
 auto channel=[&](int shift){return std::uint32_t(std::clamp((float((c>>shift)&255)*alpha+float((d>>shift)&255)*below)/std::max(out,1e-4f),0.f,255.f)+.5f);};
 d=(std::uint32_t(out*255.f+.5f)<<24)|(channel(16)<<16)|(channel(8)<<8)|channel(0);
}
void SoftwareRenderer::fill(int x,int y,int w,int h,std::uint32_t c,float alpha){
 if(alpha<=0)return;alpha=std::min(alpha,1.f);
 const int x0=std::max(0,x),x1=std::min(m_width,x+w),y0=std::max(0,y),y1=std::min(m_height,y+h);if(x0>=x1)return;
 // What blend() writes for c at full alpha over anything, or at any alpha over the
 // cleared (transparent black) HUD canvas, without its per-pixel float maths.
 const std::uint32_t over=(std::uint32_t(alpha*255.f+.5f)<<24)|(c&0xffffffu);const bool opaque=alpha>=1.f,exact=alpha>=1e-4f;
 for(int yy=y0;yy<y1;++yy){auto*row=m_pixels.data()+size_t(yy)*m_width;
  for(int xx=x0;xx<x1;++xx){auto&d=row[xx];if(opaque||(d==0&&exact))d=over;else blend(xx,yy,c,alpha);}}
}
void SoftwareRenderer::gradient(int x,int y,int w,int h,std::uint32_t c,float from,float to,bool horizontal){
 for(int yy=std::max(0,y);yy<std::min(m_height,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(m_width,x+w);++xx){
  float t=horizontal?float(xx-x)/std::max(1,w-1):float(yy-y)/std::max(1,h-1);blend(xx,yy,c,from+(to-from)*t);}
}
void SoftwareRenderer::frame(int x,int y,int w,int h,std::uint32_t c,float alpha){fill(x,y,w,1,c,alpha);fill(x,y+h-1,w,1,c,alpha);fill(x,y+1,1,h-2,c,alpha);fill(x+w-1,y+1,1,h-2,c,alpha);}
// Smoked glass with a hairline edge and red corner ticks.
void SoftwareRenderer::uiPanel(int x,int y,int w,int h){
 fill(x,y,w,h,rgb(6,5,5),.78f);frame(x,y,w,h,rgb(236,228,214),.10f);
 const auto red=rgb(214,32,26);fill(x,y,6,1,red,1);fill(x,y,1,6,red,1);fill(x+w-6,y+h-1,6,1,red,1);fill(x+w-1,y+h-6,1,6,red,1);
}
// Facility signage: an enamelled graphite steel plate with a bevelled rim,
// corner rivets and a safety-colour band, lettered in the UI's condensed face
// (auto-fitted), with light chipping at the edges and grime toward the bottom.
SoftwareRenderer::Texture SoftwareRenderer::makeSign(const char* title,const char* subtitle,std::uint32_t accent){
 constexpr int W=512,H=144;
 Texture sign{W,H,std::vector<std::uint32_t>(size_t(W)*H)};
 auto channel=[](std::uint32_t c,int shift){return float((c>>shift)&255);};
 auto mixColour=[&](std::uint32_t a,std::uint32_t b,float t){t=std::clamp(t,0.f,1.f);
  return rgb(int(channel(a,16)+(channel(b,16)-channel(a,16))*t),int(channel(a,8)+(channel(b,8)-channel(a,8))*t),int(channel(a,0)+(channel(b,0)-channel(a,0))*t));};
 auto hash=[](int x,int y,unsigned seed){unsigned h=unsigned(x)*374761393u+unsigned(y)*668265263u+seed*2246822519u;h=(h^(h>>13))*1274126177u;return float((h^(h>>16))&0xffffu)/65535.f;};
 auto noise=[&](float x,float y,unsigned seed){int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=x-ix,fy=y-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
  float a=hash(ix,iy,seed),b=hash(ix+1,iy,seed),c=hash(ix,iy+1,seed),d=hash(ix+1,iy+1,seed);return a+(b-a)*fx+(c-a)*fy+(a-b-c+d)*fx*fy;};
 unsigned seed=0;for(const char* c=title;*c;++c)seed=seed*31u+unsigned(char(*c));
 const std::uint32_t plate=rgb(36,39,42),plateHi=rgb(52,56,60),rim=rgb(18,19,21),bevel=rgb(88,92,96);
 const int band=30;
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){
  int edge=std::min({x,y,W-1-x,H-1-y});
  float grain=noise(x*.9f,y*.06f,seed)*.06f+noise(x*.05f,y*.05f,seed+7)*.08f;
  std::uint32_t c=mixColour(plate,plateHi,(1.f-y/float(H))*.55f+grain);
  if(x>=6&&x<6+band&&edge>=6)c=mixColour(accent,rgb(0,0,0),.08f+grain);
  if(x==6+band||x==7+band)c=rim;
  if(edge<6)c=edge<2?rim:edge<4?mixColour(bevel,plate,y/float(H)):plateHi;
  // Grime pooling toward the bottom edge, and chips where the enamel wore.
  float grime=std::clamp((y/float(H)-.6f)*2.2f,0.f,1.f)*noise(x*.08f,y*.2f,seed+3);
  c=mixColour(c,rgb(20,17,14),grime*.45f);
  float chip=noise(x*.22f,y*.22f,seed+11);
  if(edge<14&&chip>.86f-edge*.004f)c=mixColour(rgb(96,70,48),rgb(60,44,32),noise(x*.5f,y*.5f,seed+5));
  sign.pixels[size_t(y)*W+x]=c;
 }
 // Lettering: the condensed UI face, sized to fit, blended over the enamel.
 auto letter=[&](const char* text,int face,int size,int x,int y,std::uint32_t colour,int tracking){
  const auto& f=uiFace(face,size);int pen=x;
  for(const char* c=text;*c;++c){const auto& g=f.glyphs[uiGlyphIndex(*c)];
   for(int yy=0;yy<f.height;++yy)for(int xx=0;xx<f.width;++xx){int px=pen-4+xx,py=y+yy;if(px<0||py<0||px>=W||py>=H)continue;
    float a=g.alpha[size_t(yy)*f.width+xx]/255.f;if(a<=0)continue;
    // Worn paint: lettering thins where the enamel chipped.
    a*=1.f-std::max(0.f,noise(px*.3f,py*.3f,seed+13)-.8f)*3.f;
    auto& d=sign.pixels[size_t(py)*W+px];d=mixColour(d,colour,a);}
   pen+=g.advance+tracking;}
 };
 const int textLeft=6+band+22,textWidth=W-textLeft-22;
 int titleSize=64;while(titleSize>24&&uiTextWidth(title,0,titleSize,2)>textWidth)titleSize-=2;
 int subtitleSize=26;while(subtitleSize>14&&uiTextWidth(subtitle,0,subtitleSize,2)>textWidth)subtitleSize-=1;
 const auto& titleFace=uiFace(0,titleSize);
 int titleY=std::max(8,(H-titleFace.height-uiFace(0,subtitleSize).height)/2-6);
 letter(title,0,titleSize,textLeft,titleY,rgb(236,232,222),2);
 letter(subtitle,0,subtitleSize,textLeft+2,titleY+titleFace.height-4,mixColour(accent,rgb(236,232,222),.35f),2);
 for(int rx:{14+band,W-14})for(int ry:{14,H-15})for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx){
  float d=std::sqrt(float(dx*dx+dy*dy));if(d>4.2f)continue;
  sign.pixels[size_t(ry+dy)*W+rx+dx]=mixColour(rgb(120,124,128),rgb(30,32,35),(dx+dy+6)/12.f*(d/4.2f)*1.4f);
 }
 prepareDecal(sign);return sign;
}


void SoftwareRenderer::wornPanel(int x,int y,int width,int height,bool recess,bool materialPanel){
 if(m_vrRendering){rect(x,y,width,height,0xff041008u);rect(x+1,y+1,width-2,height-2,0xff000000u);return;}
 rect(x+2,y+3,width,height,rgb(5,4,3));
 const auto&texture=materialPanel?(recess?m_panelMetal:m_signRust):m_metal;
 std::array<int,6> key{x,y,width,height,int(recess),int(materialPanel)};
 PanelRaster* retained=nullptr;bool reuse=false;
 if(m_panelCaching&&width>0&&height>0&&int64_t(width)*height<=65536){
  for(auto&entry:m_panelRasters)if(entry.key==key&&entry.textureGeneration==texture.generation.value&&!entry.pixels.empty()){retained=&entry;reuse=true;break;}
  if(!retained){retained=&m_panelRasters[m_nextPanelRaster++%m_panelRasters.size()];retained->key=key;retained->textureGeneration=texture.generation.value;retained->pixels.resize(size_t(width)*height);}
 }
 if(!reuse){
 for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
  unsigned grain=unsigned((xx+x)*7349)^unsigned((yy+y)*19391);grain=(grain^(grain>>7))*1597;
  auto c=materialPanel?sample(recess?m_panelMetal:m_signRust,(xx+x)/256.f,(yy+y)/180.f):sample(m_metal,(xx+x)/91.f,(yy+y)/67.f);
  float light=materialPanel?(recess?1.15f:.85f):(recess?.27f:.54f);light+=(grain%11)*.007f;
  int edge=std::min({xx,yy,width-1-xx,height-1-yy});
  if(edge==0)c=rgb(24,17,12),light=1;
  else if(edge==1)c=yy<height/2?rgb(113,96,71):rgb(27,22,17),light=1;
  else if(edge<4&&grain%9==0)c=rgb(126,81,42),light=.9f;
  auto pixel=shade(c,light);if(retained)retained->pixels[size_t(yy)*width+xx]=pixel;else put(x+xx,y+yy,pixel);
 }
 }
 if(retained){int left=std::max(0,x),right=std::min(m_width,x+width);
  if(right>left)for(int yy=std::max(0,y);yy<std::min(m_height,y+height);++yy)
   std::copy_n(retained->pixels.data()+size_t(yy-y)*width+left-x,right-left,m_pixels.data()+size_t(yy)*m_width+left);
 }
 if(width>40&&height>20){for(int dx:{5,width-6})for(int dy:{5,height-6}){
  rect(x+dx-2,y+dy-2,5,5,rgb(17,13,9));rect(x+dx-1,y+dy-1,3,3,rgb(96,86,69));
  rect(x+dx-1,y+dy,3,1,rgb(31,25,20));put(x+dx-1,y+dy-1,rgb(159,140,101));
 }}
}
void SoftwareRenderer::drawHud(const Game& game){
 const auto&p=game.player();
 const auto paper=rgb(236,228,214),muted=rgb(146,138,128),amber=rgb(232,160,60),red=rgb(214,32,26);
 const char* zone=game.world().zoneName(p.pos,p.z);
 const char* mapLabel=zone?zone:game.world().custom()?game.world().customMapName():!game.world().campaign()?"ASHFALL / SURFACE":game.level()>=10?CampaignMapNames[game.level()]:game.level()==9?"18 WASTE HANDLING":game.level()==8?"17 UTILITY JUNCTION":game.level()==7?"16 PUMP ANNEX":game.level()==6?"15 CABLE VAULTS":game.level()==5?"14 COOLANT RETURN":CampaignMapNames[game.level()]; // Maps 0-4 name every spot through zones.
 const auto bone=rgb(236,228,214),black=rgb(0,0,0);
 // Shut in a locker the leaves are all there is to see, but for the band of
 // vent holes punched through them: Scene3D draws the real leaves from inside
 // as it does while they swing (the door textures' alpha cuts the holes), so
 // the frame they meet and every frame after are the same pixels. All that is
 // added here, once they have met, is what the men outside make of the player.
 const bool locker=game.hiddenLocker()>=0;
 if(game.lockerShutView()>=1&&game.lockerViewFixture()>=0){
  const int index=game.lockerViewFixture();const auto&f=game.world().fixtures()[size_t(index)];const auto&v=m_view;
  const float c=std::cos(f.yaw),s=std::sin(f.yaw);const Vec2 out{c,-s};
  const float cy=std::cos(v.angle),sy2=std::sin(v.angle),cp=std::cos(v.pitch/140.f),sp=std::sin(v.pitch/140.f),focal=Projection{float(m_width)/m_height}.focal(float(m_width));
  const Point3 eye{v.pos.x,v.pos.y,v.z+v.eye},forward{cy*cp,sy2*cp,sp},right{-sy2,cy,0},up{-cy*sp,-sy2*sp,cp};
  const Vec2 plane=f.position+out*(f.width*.5f-.01f);const float cover=1;
 // What the men outside make of the player still reads through the steel: a
  // man at the doors is the one thing a hider must be able to see.
  if(cover>=.5f)for(size_t i=0;i<game.npcActors().size();++i){const auto&npc=game.npcActors()[i];if(npc.level!=game.level())continue;
   const auto card=game.guardCard(i);if(card.kind==Game::GuardCardKind::None)continue;
   const Vec2 at=npc.pose.position;if((at.x-plane.x)*out.x+(at.y-plane.y)*out.y<.1f||lengthSq(at-plane)>36.f)continue;
   const Point3 head{at.x,at.y,npc.pose.z+npc.track().scale*1.12f+.1f},d{head.x-eye.x,head.y-eye.y,head.z-eye.z};
   const float depth=d.x*forward.x+d.y*forward.y+d.z*forward.z;if(depth<.05f)continue;
   const int sx=std::clamp(int(m_width*.5f+(d.x*right.x+d.y*right.y+d.z*right.z)/depth*focal),12,m_width-12),sy=std::clamp(int(m_height*.5f-(d.x*up.x+d.y*up.y+d.z*up.z)/depth*focal),16,m_height-40);
   const bool bang=card.kind==Game::GuardCardKind::Alert;const char* mark=bang?"!":"?";
   const auto tone=bang||card.kind==Game::GuardCardKind::Hunting?rgb(255,58,36):rgb(255,std::uint32_t(208-104*std::clamp(card.fill,0.f,1.f)),64);
   uiText(sx-uiTextWidth(mark,0,26,0)/2,sy,mark,tone,0,26,cover);}
 }
 // Shut in the HUD is down to what hiding needs: the hint, the radar and the
 // alarm line. Vitals, the gun and the objective drain away over the fifth of
 // a second the leaves take to meet and come back as they fly open; notices
 // and title cards wait until the player steps out.
 const float hud=game.lockerViewFixture()>=0?1-game.lockerShutView():1.f;const bool shut=hud<=.5f;
 char b[80];
 if(hud>0){gradient(0,0,m_width,46,black,.55f*hud,0.f,false);
 std::string place(mapLabel);for(auto&c:place)c=char(std::toupper((unsigned char)c));fill(14,13,3,13,red,hud);uiText(22,10,place.c_str(),bone,0,15,hud,2);
 std::snprintf(b,sizeof(b),"%d CONTACTS REMAIN",game.enemiesRemaining());if(!game.enemies().empty())uiText(22,28,b,muted,0,10,hud,1);
 if(game.world().hasLift()&&game.world().liftPhase()!=World::LiftPhase::Crashed)uiText(22,41,game.world().liftStatus(),amber,0,10,hud,1);}
 // Compact map reveals nearby contacts and a fixed extraction marker.
 const int mx=m_width-57,my=8;
 fill(mx-4,my-4,56,56,rgb(6,5,5),.7f);frame(mx-4,my-4,56,56,bone,.14f);fill(mx-4,my-4,5,1,red,1);fill(mx-4,my-4,1,5,red,1);
 static thread_local MinimapCache minimap;const auto&occupancy=minimap.cells(game.world());
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(occupancy[size_t(y*24+x)])fill(mx+x*2,my+y*2,2,2,bone,game.world().tile(x,y)=='#'?.32f:.14f);
 for(auto&e:game.enemies())if(e.alive&&lengthSq(e.pos-p.pos)<36)rect(mx+int(e.pos.x*2),my+int(e.pos.y*2),2,2,red);
 // Once the CCTV patch is made, the Cleanup detail in this chunk: a dot by
 // what each man makes of you (the yellow of the "?" over his head, so it
 // does not read as your own marker), a faint wedge as far and wide as he sees.
 // The headset draws no minimap yet, so in VR the patch shows nothing.
 for(const auto&c:game.radarContacts()){const auto tone=c.state==GuardState::Alert?red:c.state==GuardState::Patrol?paper:rgb(255,208,64);
  const int dx=mx+int(c.position.x*2),dy=my+int(c.position.y*2);const float ox=float(dx)+1,oy=float(dy)+1,reach=c.range*2;
  auto line=[&](float x0,float y0,float x1,float y1){int steps=std::max(1,int(std::ceil(std::max(std::fabs(x1-x0),std::fabs(y1-y0)))));
   for(int i=1;i<=steps;++i){int x=int(std::floor(x0+(x1-x0)*i/steps)),y=int(std::floor(y0+(y1-y0)*i/steps));if(x>=mx-3&&y>=my-3&&x<=mx+50&&y<=my+50)blend(x,y,tone,.35f);}};
  auto arc=[&](float from,float a){line(ox+std::sin(from)*reach,oy+std::cos(from)*reach,ox+std::sin(a)*reach,oy+std::cos(a)*reach);};
  line(ox,oy,ox+std::sin(c.yaw-c.cone)*reach,oy+std::cos(c.yaw-c.cone)*reach);line(ox,oy,ox+std::sin(c.yaw+c.cone)*reach,oy+std::cos(c.yaw+c.cone)*reach);
  for(int i=0;i<6;++i)arc(c.yaw-c.cone+c.cone*float(i)/3,c.yaw-c.cone+c.cone*float(i+1)/3);
  rect(dx,dy,2,2,tone);}
 auto exit=game.world().exitPoint();rect(mx+int(exit.x*2),my+int(exit.y*2),2,2,game.enemiesRemaining()==0?rgb(225,183,70):muted);
 rect(mx+int(p.pos.x*2)-1,my+int(p.pos.y*2)-1,3,3,amber);
 rect(mx+int((p.pos.x+std::cos(p.angle)*2)*2),my+int((p.pos.y+std::sin(p.angle)*2)*2),1,1,paper);
 // Status: type over a floor shadow, no slab. Vitals left, weapon right.
 if(hud>0){gradient(0,m_height-74,m_width,74,black,0.f,.8f*hud,false);
 const bool critical=p.health<30;const auto vital=critical?red:bone;
 std::snprintf(b,sizeof(b),"%d",std::max(0,int(p.health)));int healthWidth=uiText(22,m_height-52,b,vital,0,40,hud);
 int barX=30+healthWidth;uiText(barX,m_height-41,"HEALTH",muted,0,10,hud,2);
 fill(barX,m_height-25,100,4,bone,.15f*hud);fill(barX,m_height-25,int(std::clamp(p.health,0.f,100.f)),4,critical?red:bone,(critical&&(int(game.elapsed()*4)&1)?.5f:1.f)*hud);
 // How lit you are: five pips under the health bar. Guards make out a lit
 // shape from across the room and a dark one only close up.
 {const int lit=int(std::lround(game.playerExposure()*5.f));for(int pip=0;pip<5;++pip)fill(barX+pip*21,m_height-16,18,3,amber,(pip<lit?.85f:.15f)*hud);}
 for(int i=1;i<10;++i)fill(barX+i*10,m_height-25,1,4,black,.7f*hud);
 if(game.unarmed()){const char* hands=game.guarding()?"GUARD":"FISTS";uiText(m_width-24-uiTextWidth(hands,0,30,1),m_height-44,hands,bone,0,30,hud,1);uiText(m_width-24-uiTextWidth("UNARMED",0,10,2),m_height-56,"UNARMED",muted,0,10,hud,2);}
 else{std::snprintf(b,sizeof(b),"%02d",p.loaded);int loadedWidth=uiTextWidth(b,0,40);int reserveX=m_width-24-uiTextWidth("/ 00",0,16);
  uiText(reserveX-6-loadedWidth,m_height-52,b,p.loaded==0?red:bone,0,40,hud);
  std::snprintf(b,sizeof(b),"/ %02d",std::max(0,p.ammo-p.loaded));uiText(reserveX,m_height-31,b,muted,0,16,hud);
  uiText(reserveX,m_height-46,"12 GA",muted,0,10,hud,2);
  for(int i=0;i<8;++i)fill(reserveX-6-loadedWidth-12-i*6,m_height-34,3,10,i<p.loaded?amber:bone,(i<p.loaded?1.f:.15f)*hud);
  if(p.loaded==0)uiText(m_width/2-uiTextWidth("RELOAD  [R]",0,12,2)/2,m_height/2+24,"RELOAD  [R]",red,0,12,hud,2);}
 std::snprintf(b,sizeof(b),"PURGED  %02d / %02d",game.kills(),int(game.enemies().size()));if(!game.enemies().empty())uiText(m_width/2-uiTextWidth(b,0,10,2)/2,m_height-20,b,muted,0,10,.8f*hud,2);
 if(game.hasFlashlight()){float jam=game.flashlightJam();const char* light=!game.state(stateId("flashlight_on"))?"LIGHT OFF":jam>=Game::FlashlightJamTime?"NO POWER":jam>0?"LIGHT FAILING":"LIGHT ON";uiText(m_width/2-uiTextWidth(light,0,10,2)/2,m_height-34,light,game.flashlightOn()?amber:muted,0,10,.8f*hud,2);}}
 auto cross=game.hitFlash()>0?red:paper;int cx=m_width/2,cy=m_height/2;
 if(!locker){fill(cx-7,cy,4,1,cross,.85f);fill(cx+4,cy,4,1,cross,.85f);fill(cx,cy-7,1,4,cross,.85f);fill(cx,cy+4,1,4,cross,.85f);blend(cx,cy,cross,.6f);}
 if(auto target=shut?nullptr:game.targetEnemy()){
  int x=m_width/2-65,y=m_height-60;
  uiText(x,y-6,target->name(),paper,0,11,1,1);
  fill(x,y+8,130,3,bone,.15f);fill(x,y+8,int(130*std::max(0.f,target->hp)/target->maxHp),3,target->windup>0?amber:red,1);
  if(target->windup>0)uiText(cx-uiTextWidth("INCOMING",0,11,2)/2,cy-26,"INCOMING",amber,0,11,1,2);
 }
 bool transferReleased=false;
 for(const auto& door:game.world().doors())if(door.transfer&&(door.requireEnemiesClear||door.requireControl||door.requireState)&&!game.doorLocked(door))transferReleased=true;
 if(shut)transferReleased=false;
 if(transferReleased){const char* m="TRANSFER INTERLOCK RELEASED";int w=uiTextWidth(m,0,11,2);gradient(cx-w/2-30,86,w/2+30,16,black,0.f,.7f,true);gradient(cx,86,w/2+30,16,black,.7f,0.f,true);uiText(cx-w/2,87,m,amber,0,11,1,2);}
 if(auto goal=game.activeObjectiveText();!goal.empty()&&!game.dead()&&!game.won()&&hud>0){std::string line(goal);int w=std::min(m_width-140,uiTextWidth(line.c_str(),1,13));int left=cx-w/2;
  gradient(left-40,60,w/2+40,20,black,0.f,.7f*hud,true);gradient(cx,60,w/2+40,20,black,.7f*hud,0.f,true);
  uiText(cx-uiTextWidth("OBJECTIVE",0,9,3)/2,50,"OBJECTIVE",red,0,9,hud,3);uiText(left,62,line.c_str(),bone,1,13,hud);}
 // Under the objective: ALERT while a guard here has you, EVASION and its
 // clock while his squad still hunts you out of sight, then their CAUTION
 // clock running down while they keep looking.
 if(int alarm=game.guardAlarm();alarm&&!game.dead()&&!game.won()){
  const bool evasion=alarm==Game::AlarmEvasion;const int left=evasion?game.guardEvasion():alarm;
  if(alarm==Game::AlarmAlert)std::snprintf(b,sizeof(b),"ALERT");else{int s=(left+999)/1000;std::snprintf(b,sizeof(b),evasion?"EVASION  %d:%02d":"CAUTION  %d:%02d",s/60,s%60);}
  int w=uiTextWidth(b,0,13,3),y=transferReleased?108:88;if(game.areaTitleAlpha()>0&&!locker)y=std::max(y,m_height/6+std::max(22,m_height/14)+12);
  gradient(cx-w/2-30,y-3,w/2+30,20,black,0.f,.7f,true);gradient(cx,y-3,w/2+30,20,black,.7f,0.f,true);
  // A full red bar while they have you; the red-orange one drains with the
  // hunt's clock, the amber one with Caution's.
  const auto tone=alarm==Game::AlarmAlert?rgb(244,62,46):evasion?rgb(255,112,36):amber;uiText(cx-w/2,y,b,tone,0,13,alarm==Game::AlarmAlert&&(int(game.elapsed()*3)&1)?.75f:1.f,3);
  fill(cx-w/2,y+16,w,2,tone,alarm==Game::AlarmAlert?.85f:.18f);
  if(evasion)fill(cx-w/2,y+16,int(int64_t(w)*left/std::max(1,game.guardEvasionPeak())),2,tone,.85f);else if(alarm>0)fill(cx-w/2,y+16,w*alarm/Game::CautionMs,2,amber,.85f);
 }
 // Major-area title card: thin, wide-set capitals across the top with a soft
 // glow, fading in and out the first time the player enters the area.
 if(float a=game.areaTitleAlpha();a>0&&!game.dead()&&!locker){const char* title=game.areaTitle().c_str();
  int size=std::max(22,m_height/14),track=size/3,w=uiTextWidth(title,1,size,track),x=cx-w/2,y=m_height/6;
  for(auto [dx,dy]:{std::pair{-2,0},{2,0},{0,-2},{0,2}})uiText(x+dx,y+dy,title,bone,1,size,a*.08f,track);
  uiText(x+1,y+2,title,black,1,size,a*.45f,track);uiText(x,y,title,rgb(250,248,242),1,size,a,track);}
 if(game.dead()||game.won()){fill(0,0,m_width,m_height,black,game.dead()?.55f:.35f);gradient(0,cy-50,m_width,50,black,0.f,.8f,false);gradient(0,cy,m_width,50,black,.8f,0.f,false);
  const char* headline=game.won()?"SECTOR CLEARED":"YOU DIED";int w=uiTextWidth(headline,2,64,4);uiText(cx-w/2+3,cy-40,headline,black,2,64,.8f,4);uiText(cx-w/2,cy-42,headline,game.won()?amber:red,2,64,1,4);
  const char* hint="PRESS ESC TO RESTART";uiText(cx-uiTextWidth(hint,0,11,3)/2,cy+30,hint,paper,0,11,.85f,3);}
 if(game.damageFlash()>0){auto tint=rgb(150,37,25);rect(0,0,m_width,2,tint);rect(0,0,2,m_height,tint);rect(m_width-2,0,2,m_height,tint);}
 if(shut){}
 else if(game.audioMuted())uiText(22,54,"AUDIO MUTED  [M]",muted,0,10,1,1);
 else if(!game.musicEnabled())uiText(22,54,"MUSIC OFF  [N]",muted,0,10,1,1);
 auto promptPanel=[&](int bottom,const char* message,std::uint32_t color){
  int maxWidth=std::max(24,m_width-120);auto rows=wrapUiText(message,size_t(std::max(1,(maxWidth-28)/6)));
  int longest=0;for(const auto&line:rows)longest=std::max(longest,uiTextWidth(line.c_str(),1,13));
  int width=std::min(maxWidth,longest+28),height=8+int(rows.size())*15,left=cx-width/2,top=bottom-height;
  fill(left,top,width,height,rgb(6,5,5),.72f);fill(left,top,2,height,color==paper?red:color,1);
  for(size_t i=0;i<rows.size();++i)uiText(left+14,top+3+int(i)*15,rows[i].c_str(),color,1,13);
 };
 // A notice never sits on a locker's vent band (BODY STOWED, said facing the
 // doors just shut on him): the band of the nearest locker ahead is projected
 // and the panel goes above it, or below it if there is no room above.
 auto offVents=[&](int bottom,int height){
  const auto&v=m_view;const float ca=std::cos(v.angle),sa=std::sin(v.angle),cp=std::cos(v.pitch/140.f),sp=std::sin(v.pitch/140.f),focal=Projection{float(m_width)/m_height}.focal(float(m_width));
  const Point3 eye{v.pos.x,v.pos.y,v.z+v.eye},forward{ca*cp,sa*cp,sp},right{-sa,ca,0},up{-ca*sp,-sa*sp,cp};
  for(const auto&f:game.world().fixtures()){if(f.model!=model::psx_locker||!f.visible||lengthSq(f.position-v.pos)>9.f)continue;
   const Vec2 out{std::cos(f.yaw),-std::sin(f.yaw)},side{-out.y,out.x},plane=f.position+out*(f.width*.5f);const float sz=f.depth/.9f,sy=f.height/2.2969f,floor=game.world().floorHeight(f.position.x,f.position.y)+f.base;
   float top=1e9f,low=-1e9f,left=1e9f,rightmost=-1e9f;bool ahead=true;
   for(float across:{-.45f*sz,.45f*sz})for(float h:{1.31f,1.385f}){const Vec2 p=plane+side*across;const Point3 d{p.x-eye.x,p.y-eye.y,floor+h*sy-eye.z};
    const float depth=d.x*forward.x+d.y*forward.y+d.z*forward.z;if(depth<.1f){ahead=false;break;}
    const float x=m_width*.5f+(d.x*right.x+d.y*right.y+d.z*right.z)/depth*focal,y=m_height*.5f-(d.x*up.x+d.y*up.y+d.z*up.z)/depth*focal;
    top=std::min(top,y);low=std::max(low,y);left=std::min(left,x);rightmost=std::max(rightmost,x);}
   if(!ahead||rightmost<m_width*.3f||left>m_width*.7f||low<bottom-height-6||top>bottom+6)continue;
   bottom=top-8-height>60?int(top)-8:std::min(m_height-8,int(low)+10+height);}
  return bottom;};
 if(game.pickupNoticeTime()>0&&!shut)promptPanel(offVents(m_height-65,23),game.pickupNotice().c_str(),paper);
 if(auto caption=game.actorCaption();!caption.empty()&&game.logTime()<=0)promptPanel(m_height-112,std::string(caption).c_str(),paper);
 else if(auto pickup=shut?nullptr:game.nearbyPickup()){
  const char* message=pickup->kind==Pickup::Kind::Ammo?"12 GA / 16 SHELLS":p.health>=100?"FIRST AID / HEALTH FULL":"FIRST AID / 35 HEALTH";
  promptPanel(m_height-65,message,amber);
 }
 // Shut in, the hint sits low on the steel under the vent band, off the
 // holes and the floor in front of the doors that they show.
 if(auto hint=game.interactionHint();hint&&game.logTime()<=0)promptPanel(shut&&locker?m_height-10:m_height-88,hint,paper);
 drawTerminal(game);
}
// A terminal open on screen: the personnel computer full screen, or the
// local-control panel. The desktop HUD and the VR panel both draw it.
// Lying in the world a sheet is too small to read: stock, and lines of writing
// that end ragged the way real lines do.
SoftwareRenderer::Texture SoftwareRenderer::makePaperProp(Readable kind){
 const bool sticky=kind==Readable::Sticky,letter=kind==Readable::Letter;
 const int W=sticky?32:64,H=sticky?32:88;
 Texture sheet{W,H,std::vector<std::uint32_t>(size_t(W)*H)};
 const std::uint32_t stock=sticky?rgb(233,208,92):letter?rgb(232,228,214):rgb(226,222,204);
 const std::uint32_t ink=sticky?rgb(30,30,30):letter?rgb(70,70,74):rgb(52,66,120);
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){
  std::uint32_t c=stock;
  if(!letter&&!sticky&&y>8&&(y-10)%6==0)c=rgb(170,186,210);
  const int pitch=sticky?7:6,first=sticky?6:8,line=(y-first)/pitch,lineTop=first+line*pitch;
  const bool writing=y>=first&&y-lineTop<2&&line<(sticky?3:12);
  const unsigned h=unsigned(line+1)*2654435761u;const int end=W-6-int((h>>8)%unsigned(W/3));
  if(writing&&x>=5&&x<end&&(x*7+line*3)%11!=0)c=ink;
  sheet.pixels[size_t(y)*W+x]=c;
 }
 prepareDecal(sheet);return sheet;
}
// Paper is read in the hand, not on a screen: the sheet fills the middle of the
// view with the room dimmed behind it. A printout is typed, a note or log book
// is in pen on ruled paper, a sticky note is in marker.
void SoftwareRenderer::drawPaper(const Game& game,int index){
 const auto& paper=game.world().terminals()[size_t(index)];
 const bool letter=paper.readable==Readable::Letter,sticky=paper.readable==Readable::Sticky;
 fill(0,0,m_width,m_height,rgb(0,0,0),.62f);
 const int sheetH=std::max(160,std::min(m_height-80,sticky?300:470)),sheetW=sticky?sheetH:int(sheetH*(letter?.71f:.74f));
 const int left=(m_width-sheetW)/2,top=std::max(12,(m_height-sheetH)/2-14);
 const std::uint32_t stock=sticky?rgb(233,208,92):letter?rgb(232,228,214):rgb(226,222,204);
 fill(left+6,top+8,sheetW,sheetH,rgb(0,0,0),.45f);
 fill(left,top,sheetW,sheetH,stock,1.f);
 // A printout was folded to fit a pocket; every sheet darkens toward its edges.
 if(letter)fill(left,top+sheetH/3,sheetW,1,rgb(180,176,160),.8f);
 for(int ring=0;ring<6;++ring)frame(left+ring,top+ring,sheetW-ring*2,sheetH-ring*2,rgb(120,110,90),.05f*(6-ring));
 const int face=letter?3:sticky?0:1,size=letter?15:sticky?24:19,lineH=letter?20:sticky?30:26;
 const int margin=sticky?22:letter?32:54,width=sheetW-margin-(sticky?22:28);
 const std::uint32_t ink=letter?rgb(38,38,42):sticky?rgb(24,24,24):rgb(30,44,104);
 int row=top+(sticky?24:letter?36:38);
 if(!letter&&!sticky){ // ruled notepad: a red margin and blue rules under each line
  fill(left+margin-12,top,2,sheetH,rgb(200,120,120),.8f);
  for(int y=row+size+4;y<top+sheetH-8;y+=lineH)fill(left+6,y,sheetW-12,1,rgb(150,170,200),.7f);
 }
 auto wrap=[&](const std::string& text,int textSize){
  std::vector<std::string> rows;std::string line,word;
  auto place=[&]{if(word.empty())return;const std::string next=line.empty()?word:line+' '+word;
   if(!line.empty()&&uiTextWidth(next.c_str(),face,textSize)>width){rows.push_back(line);line=word;}else line=next;word.clear();};
  for(char c:text){if(c==' ')place();else word+=c;}
  place();if(!line.empty())rows.push_back(line);return rows;
 };
 // Pen does not keep a straight margin; type and marker do.
 auto write=[&](const std::string& text,int textSize){for(const auto& line:wrap(text,textSize)){if(row>top+sheetH-lineH)return;
   const int drift=letter||sticky?0:(row*7)%5-2;uiText(left+margin+drift,row,line.c_str(),ink,face,textSize);row+=lineH;}};
 write(paper.title,size+4);row+=lineH/2;
 write(paper.line1,size);row+=lineH/3;
 write(paper.line2,size);
 uiText(left,top+sheetH+12,m_vrRendering?"GRIP / PUT IT DOWN":"E / PUT IT DOWN",rgb(214,208,190),0,16);
}
void SoftwareRenderer::drawTerminal(const Game& game){
 const int cx=m_width/2;
 if(game.logTime()>0&&game.activeLog()>=0&&game.world().terminals()[size_t(game.activeLog())].paper()){drawPaper(game,game.activeLog());return;}
 if(game.logTime()>0&&game.activeLog()>=0&&!game.world().terminals()[size_t(game.activeLog())].control&&game.world().terminals()[size_t(game.activeLog())].reactorAction<2){
  drawComputer(game,game.activeLog());
 }else if(game.logTime()>0&&game.activeLog()>=0){auto&log=game.world().terminals()[size_t(game.activeLog())];
  const int left=cx-180,top=48,width=360,height=154;
  wornPanel(left,top,width,height,false,true);
  // Inset screen inside the existing metal housing, with a recessed bevel.
  rect(left+8,top+8,width-16,height-16,rgb(18,22,20));
  rect(left+9,top+9,width-18,1,rgb(0,0,0));rect(left+9,top+9,1,height-18,rgb(0,0,0));
  rect(left+10,top+10,width-20,height-20,rgb(2,5,3));
  rect(left+10,top+height-11,width-20,1,rgb(76,83,78));rect(left+width-11,top+10,1,height-20,rgb(76,83,78));
  const auto green=rgb(87,225,123),bright=rgb(144,255,166),dimGreen=rgb(47,136,76);
  text(left+18,top+17,terminalHeader(log),dimGreen);
  text(left+18,top+31,log.title,bright);rect(left+18,top+43,width-36,1,rgb(24,66,36));
  int row=top+51;
  auto lines=[&](const char* value){std::string remaining=value;while(!remaining.empty()&&row<top+114){size_t n=std::min(size_t(52),remaining.size());if(n<remaining.size()){auto space=remaining.rfind(' ',n);if(space!=std::string::npos&&space>0)n=space;}text(left+18,row,remaining.substr(0,n).c_str(),green);remaining.erase(0,n);while(!remaining.empty()&&remaining.front()==' ')remaining.erase(0,1);row+=11;}};
  lines(log.line1);row+=3;lines(log.line2);
  text(left+18,top+117,terminalStatus(game,log),green);
  text(left+18,top+132,"> E TO CLOSE TERMINAL _",bright);
 }
}

// Personnel computers open full screen: the CRT front from the OFFICE atlas,
// with a Depthworks OS desktop on its glass. Messages that play at this
// terminal's position form the conversation in the main window.
void SoftwareRenderer::drawComputer(const Game& game,int index){
 const auto& log=game.world().terminals()[size_t(index)];
 const auto& atlas=m_facilityTextures.at("computer");
 constexpr int CropW=129,CropH=112,GlassX=14,GlassY=17,GlassW=99,GlassH=79,Scale=3;
 const int left=(m_width-CropW*Scale)/2,top=std::max(0,(m_height-CropH*Scale)/2);
 rect(0,0,m_width,m_height,0xb4000000u);
 for(int y=0;y<CropH*Scale;++y)for(int x=0;x<CropW*Scale;++x){
  int px=left+x,py=top+y;if(px<0||py<0||px>=m_width||py>=m_height)continue;
  int sx=std::min(atlas.width-1,x/Scale),sy=std::min(atlas.height-1,y/Scale);
  m_pixels[size_t(py)*m_width+px]=0xff000000u|(atlas.pixels[size_t(sy)*atlas.width+sx]&0xffffffu);
 }
 const int sx=left+GlassX*Scale,sy=top+GlassY*Scale,sw=GlassW*Scale,sh=GlassH*Scale;
 const auto glass=rgb(5,13,9),bar=rgb(18,64,36),panel=rgb(9,26,16),green=rgb(87,225,123),bright=rgb(150,255,172),dim=rgb(47,136,76);
 rect(sx,sy,sw,sh,glass);for(int y=sy;y<sy+sh;y+=2)rect(sx,y,sw,1,rgb(3,9,6));
 auto right=[&](int x,int y,const std::string& value,std::uint32_t color){text(x-int(value.size())*4,y,value.c_str(),color);};
 rect(sx,sy,sw,11,bar);text(sx+4,sy+3,"DEPTHWORKS SECUREOS 3.1",bright);right(sx+sw-4,sy+3,"LOCAL / NIGHT SHIFT",green);
 auto messages=game.terminalMessages(index);
 const int menuW=58;rect(sx,sy+11,menuW,sh-22,panel);
 const char* items[]={"CCTV","LOGS","MAIL","CHAT"};int selected=messages.empty()?1:3;
 for(int i=0;i<4;++i){int y=sy+18+i*12;if(i==selected){rect(sx+2,y-2,menuW-4,10,bar);text(sx+6,y,"> ",bright);}text(sx+14,y,items[i],i==selected?bright:dim);}
 const int wx=sx+menuW+4,wy=sy+15,ww=sw-menuW-8,wh=sh-30;
 rect(wx,wy,ww,wh,dim);rect(wx+1,wy+1,ww-2,wh-2,glass);rect(wx+1,wy+1,ww-2,9,bar);
 std::string title=messages.empty()?std::string(log.title):std::string("DIRECT LINE / ")+[&]{std::string who=messages.front()->caption;auto colon=who.find(':');return colon==std::string::npos?std::string("CHAT"):who.substr(0,colon);}();
 text(wx+4,wy+3,wrapUiText(title,size_t((ww-8)/4),1).front().c_str(),bright);
 const size_t columns=size_t((ww-10)/4);const int lineH=8,bodyTop=wy+14,bodyRows=(wh-18)/lineH;
 struct Line{std::string text;std::uint32_t color;int indent;};std::vector<Line> body;
 if(messages.empty()){
  for(const char* value:{log.line1,log.line2}){for(auto& line:wrapUiText(value,columns,6))body.push_back({line,green,0});body.push_back({"",green,0});}
 }else for(const auto* cue:messages){
  bool own=cue->caption.rfind("YOU:",0)==0;bool first=true;
  for(auto& line:wrapUiText(cue->caption,columns-(own?3:0),4)){body.push_back({line,first?(own?dim:bright):(own?dim:green),own?12:0});first=false;}
 }
 // The HUD font has no underscore: the remote end's typing cursor is a block.
 bool typing=game.terminalConversationActive(index);if(typing)body.push_back({"",bright,0});
 int start=std::max(0,int(body.size())-bodyRows),row=0;
 for(int i=start;i<int(body.size());++i,++row)text(wx+5+body[size_t(i)].indent,bodyTop+row*lineH,body[size_t(i)].text.c_str(),body[size_t(i)].color);
 if(typing&&int(game.elapsed()*2)%2==0)rect(wx+5,bodyTop+(row-1)*lineH,4,6,bright);
 rect(sx,sy+sh-11,sw,11,bar);text(sx+4,sy+sh-8,terminalStatus(game,log),green);right(sx+sw-4,sy+sh-8,"E / CLOSE",bright);
}

void SoftwareRenderer::drawVrStatus(const Game&game){
 rect(0,0,m_width,m_height,0xff000000u);constexpr auto green=0xff69ff8au;char data[100];
 if(game.logTime()>0&&game.activeLog()>=0){const auto&log=game.world().terminals()[game.activeLog()];
  text(24,20,log.paper()?(log.readable==Readable::Letter?"PAPER / PRINTED":log.readable==Readable::Sticky?"PAPER / STICKY NOTE":"PAPER / HANDWRITTEN"):log.reactorAction||log.activateState||log.control?"TERMINAL / LOCAL CONTROL":"TERMINAL / SHIFT LOG",green,3);
  text(24,60,log.title,green,2);int row=95;auto lines=[&](const char*value){for(const auto&line:wrapUiText(value,70,4)){if(row>=250)break;text(24,row,line.c_str(),green,2);row+=22;}};
  lines(log.line1);row+=12;lines(log.line2);if(!log.paper())text(24,270,terminalStatus(game,log),green);text(24,307,log.paper()?"PUT IT DOWN":"CLOSE TERMINAL",green,3);return;
 }
 text(24,20,"DEPTHWORKS / FIELD TERMINAL",green,3);rect(24,46,592,2,green);
 std::snprintf(data,sizeof(data),"HEALTH %03d",int(game.player().health));text(24,67,data,green,5);
 rect(24,106,592,16,0xff082510u);rect(24,106,int(592*std::clamp(game.player().health/100.f,0.f,1.f)),16,green);
 std::snprintf(data,sizeof(data),"SHELLS %02d / %02d",game.player().loaded,game.player().ammo);text(24,142,data,green,4);
 std::snprintf(data,sizeof(data),"CONTACTS %02d / PURGED %02d",game.enemiesRemaining(),game.kills());text(24,185,data,green,3);
 text(24,222,game.world().custom()?game.world().customMapName():game.world().campaign()?CampaignMapNames[game.level()]:"ASHFALL EXCLUSION ZONE",green,2);text(24,245,game.weaponEquipped()?"SHOTGUN EQUIPPED":"EMPTY HANDS",green,2);
 if(auto hint=game.interactionHint())text(24,272,hint,green);if(game.pickupNoticeTime()>0)text(290,245,game.pickupNotice().c_str(),green);if(auto caption=game.actorCaption();!caption.empty())text(24,260,std::string(caption).c_str(),green);
 // Map and contacts remain available on the device, never across the headset view.
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(game.world().solid(x+.5f,y+.5f))rect(530+x*3,180+y*3,3,3,0xff0b3417u);const auto&p=game.player();rect(530+int(p.pos.x*3),180+int(p.pos.y*3),3,3,green);for(const auto&e:game.enemies())if(e.alive&&lengthSq(e.pos-p.pos)<36)rect(530+int(e.pos.x*3),180+int(e.pos.y*3),3,3,green);auto exit=game.world().exitPoint();rect(530+int(exit.x*3),180+int(exit.y*3),4,4,green);
 rect(24,292,266,50,0xff082510u);text(42,307,"INVENTORY",green,3);rect(334,292,282,50,0xff082510u);text(354,307,"MENU / SAVE",green,3);
}
// Alyx-style VR presentation: no HUD across the view. Small textures, drawn on
// the renderer's own canvas and copied out, for the displays that ride on the
// hands and float ahead; all additive, so black is clear glass and text glows.
void SoftwareRenderer::prepareVrDisplays(const Game& game){
 auto stamp=[&](Texture& texture,int width,int height,const auto& draw){
  std::fill(m_pixels.begin(),m_pixels.end(),0xff000000u);draw();
  if(texture.pixels.size()!=size_t(width*height)){texture={width,height,std::vector<std::uint32_t>(size_t(width*height))};texture.clampEdges=true;texture.additive=true;}
  for(int y=0;y<height;++y)std::copy_n(m_pixels.begin()+size_t(y)*size_t(m_width),width,texture.pixels.begin()+size_t(y)*size_t(width));
  m_gpu->updateDynamic(texture);
 };
 const auto glass=rgb(150,225,255),dim=rgb(24,44,54),warn=rgb(255,96,60);
 // Health: ten cells on the back of the left hand, red below 30.
 const int health=int(std::clamp(game.player().health,0.f,100.f));
 if(health!=m_vrHealthShown){m_vrHealthShown=health;stamp(m_vrHealthTexture,128,24,[&]{const int lit=(health+9)/10;for(int i=0;i<10;++i)rect(4+i*12,5,10,14,i<lit?(health<30?warn:glass):dim);});}
 // The load in the gun, and the reserve beside it.
 const int load=game.player().loaded,reserve=std::max(0,game.player().ammo-load);
 const int ammoKey=load*1000+reserve+(game.reloading()?500:0);
 if(ammoKey!=m_vrAmmoShown){m_vrAmmoShown=ammoKey;stamp(m_vrAmmoTexture,128,64,[&]{char text[16];std::snprintf(text,sizeof text,"%d",load);uiText(10,4,text,load?glass:warn,0,44);
  if(game.reloading())uiText(62,8,"LOADING",rgb(255,190,90),0,14,1,1);std::snprintf(text,sizeof text,"%d",reserve);uiText(74,30,text,rgb(90,140,160),0,20);});}
 // Area titles fade in and out as on the desktop.
 const float titleAlpha=game.areaTitleAlpha();
 if(std::fabs(titleAlpha-m_vrTitleShown)>.01f){m_vrTitleShown=titleAlpha;stamp(m_vrTitleTexture,512,64,[&]{if(titleAlpha<=0)return;const char* title=game.areaTitle().c_str();const int size=34,track=10,width=uiTextWidth(title,1,size,track);uiText(256-width/2,10,title,rgb(250,248,242),1,size,titleAlpha,track);});}
 // Subtitles, or a new objective for six seconds.
 std::string objective(game.activeObjectiveText());
 if(objective!=m_vrObjective){m_vrObjective=objective;m_vrObjectiveTime=objective.empty()?0.f:6.f;}
 m_vrObjectiveTime=std::max(0.f,m_vrObjectiveTime-1.f/90.f);
 std::string caption(game.actorCaption());if(caption.empty()&&m_vrObjectiveTime>0)caption="OBJECTIVE: "+m_vrObjective;
 // With nothing else to say, the strip carries the guards' alarm: ALERT, the Evasion clock, or the Caution clock.
 const int alarm=caption.empty()&&!game.dead()&&!game.won()?game.guardAlarm():0;
 if(alarm==Game::AlarmAlert)caption="ALERT";else if(alarm){const bool evasion=alarm==Game::AlarmEvasion;const int s=((evasion?game.guardEvasion():alarm)+999)/1000;char clock[24];std::snprintf(clock,sizeof clock,evasion?"EVASION  %d:%02d":"CAUTION  %d:%02d",s/60,s%60);caption=clock;}
 if(caption!=m_vrCaptionShown){m_vrCaptionShown=caption;const auto tone=alarm==Game::AlarmAlert?rgb(244,62,46):alarm==Game::AlarmEvasion?rgb(255,112,36):alarm>0?rgb(232,160,60):rgb(235,235,230);stamp(m_vrCaptionTexture,512,72,[&]{int y=4;for(const auto& row:wrapUiText(caption,size_t(64),3)){const int width=uiTextWidth(row.c_str(),1,18);uiText(256-width/2,y,row.c_str(),tone,1,18);y+=22;}});}
}
// A save is being rebuilt on a worker: the frame goes dark under a bar that
// fills as its chunks are built, instead of the window hanging.
void SoftwareRenderer::drawLoading(const Game& game){
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0);
 if(!hardwarePresentsWindow())for(auto&p:m_pixels)p=shade(p,.1f);
 fill(0,0,m_width,m_height,black,.82f);
 const int w=std::min(420,m_width-80),x=(m_width-w)/2,y=m_height/2+10;
 const char* label="LOADING";uiText((m_width-uiTextWidth(label,0,14))/2,y-34,label,bone,0,14);
 fill(x,y,w,6,bone,.12f);fill(x,y,std::max(2,int(float(w)*game.loadProgress())),6,red,1);
 const int percent=int(game.loadProgress()*100.f+.5f);char text[16];std::snprintf(text,sizeof text,"%d%%",percent);
 uiText((m_width-uiTextWidth(text,3,11))/2,y+18,text,dim,3,11);
}
void SoftwareRenderer::drawTitle(const Game& game){
 static unsigned tick=0;++tick;const bool blink=(tick/30)%2==0;
 if(!hardwarePresentsWindow())for(auto&p:m_pixels)p=shade(p,.17f);
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0),amber=rgb(232,160,60);
 // Smoke the left of the frame so the menu reads over the dimmed scene.
 gradient(0,0,m_width*3/4,m_height,black,.94f,0.f,true);
 gradient(0,0,m_width,64,black,.75f,0.f,false);gradient(0,m_height-72,m_width,72,black,0.f,.9f,false);
 for(int y=0;y<m_height;y+=2)fill(0,y,m_width,1,black,.16f);
 for(int i=0;i<700;++i){unsigned h=unsigned(i)*2654435761u^tick*40503u;h^=h>>15;h*=2246822519u;h^=h>>13;blend(int(h%unsigned(m_width)),int((h>>10)%unsigned(m_height)),(h>>5)&1?bone:black,.09f);}
 // Header strip.
 fill(40,25,5,5,red,1);uiText(52,20,"DEPTHWORKS INDUSTRIAL  //  SITE 9 EXTRACTION COMPLEX",dim,0,11,1,1);
 const char* clock="SHIFT 03 / 03:14 AM";uiText(m_width-40-uiTextWidth(clock,3,11),20,clock,dim,3,11);
 fill(40,38,m_width-80,1,bone,.10f);
 // Wordmark: a red plate misregistered under bone type, torn every few seconds.
 const char* mark="DEPTHWORKS";const int markX=36,markY=44;
 uiText(markX+3,markY+2,mark,red,2,74,.85f,2);uiText(markX,markY,mark,bone,2,74,1,2);
 if(tick>60&&(tick/7)%23==0){unsigned h=tick*747796405u;int band=markY+12+int(h%50),shift=6+int((h>>8)%14);
  for(int y=band;y<std::min(m_height,band+4+int((h>>4)%6));++y){auto row=m_pixels.begin()+size_t(y)*m_width;std::rotate(row,row+m_width-shift,row+m_width);}}
 fill(40,134,4,16,red,1);
 uiText(52,133,game.customMapsOpen()?"CUSTOM MAP ARCHIVE":"CONTAINMENT FAILURE",red,0,15,1,3);
 uiText(52,153,game.customMapsOpen()?"Drop campaign .txt files in /custom maps":"Something broke loose below the foundry. Nobody is coming.",dim,1,13);
 const char* normalLabels[]={"NEW GAME","CUSTOM MAPS","LOAD GAME","SETTINGS","QUIT"};
 for(int row=0;row<game.titleRows();++row){
  int y=TitleMenuLayout::Y+row*TitleMenuLayout::RowHeight;bool selected=row==game.titleSelection();
  std::string customLabel=game.customMapsOpen()?game.customMenuLabel(row):std::string{};
  const char* label=game.customMapsOpen()?customLabel.c_str():normalLabels[row];
  if(selected){gradient(TitleMenuLayout::X-14,y,TitleMenuLayout::Width+14,23,red,.55f,0.f,true);fill(TitleMenuLayout::X-14,y,3,23,red,1);}
  char index[4];std::snprintf(index,sizeof(index),"%02d",row+1);
  uiText(TitleMenuLayout::X,y+6,index,selected?bone:dim,3,10,selected?1.f:.55f);
  uiText(TitleMenuLayout::X+24,y,label,selected?bone:dim,0,19,selected?1.f:.8f,1);
  if(selected)uiText(TitleMenuLayout::X+TitleMenuLayout::Width-16,y+1,">",bone,0,19,.8f);
 }
 // Facility status readout.
 const int sx=m_width-218,sy=196;
 uiText(sx,sy,"FACILITY STATUS",dim,0,11,1,2);fill(sx,sy+16,178,1,bone,.14f);
 const char* keys[]={"MAIN POWER","SERVICE LIFT","CONTAINMENT","PERSONNEL"};const char* values[]={"DEGRADED","EMERGENCY","FAILED","UNACCOUNTED"};
 for(int i=0;i<4;++i){uiText(sx,sy+24+i*17,keys[i],dim,3,11,.85f);uiText(sx+178-uiTextWidth(values[i],3,11),sy+24+i*17,values[i],i<2?amber:red,3,11);}
 if(blink)fill(sx,sy+101,6,6,red,1);uiText(sx+12,sy+96,"ALARM ACTIVE / ALL LEVELS",red,0,11,blink?1.f:.55f,1);
 // Footer: key caps and build tag.
 fill(40,m_height-36,m_width-80,1,bone,.10f);
 int kx=40;auto cap=[&](const char*key,const char*what){int w=uiTextWidth(key,3,10)+10;frame(kx,m_height-27,w,15,bone,.35f);uiText(kx+5,m_height-26,key,bone,3,10);kx+=w+6;kx+=uiText(kx,m_height-27,what,dim,0,11,1,1)+16;};
 cap("ENTER","SELECT");cap("ESC","BACK");cap("ARROWS","NAVIGATE");
 const char* build="V0.9.0  PLAYTEST BUILD";uiText(m_width-40-uiTextWidth(build,0,11,2),m_height-27,build,red,0,11,1,2);
}

void SoftwareRenderer::drawSettings(const Game& game){
 if(!hardwarePresentsWindow())for(auto&pixel:m_pixels)pixel=shade(pixel,.25f);
 constexpr int x=MenuLayout::X,y=MenuLayout::Y;
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0);
 fill(0,0,m_width,m_height,black,.45f);
 fill(x,y,MenuLayout::Width,MenuLayout::Height,rgb(6,5,5),.9f);frame(x,y,MenuLayout::Width,MenuLayout::Height,bone,.10f);fill(x,y,MenuLayout::Width,2,red,1);
 auto page=game.menuPage();bool settingsPage=page==Game::MenuPage::Settings;
 const char* title=settingsPage?(game.menuFromTitle()?"SETTINGS":"PAUSED"):page==Game::MenuPage::Save?"SAVE GAME":page==Game::MenuPage::Load?"LOAD GAME":page==Game::MenuPage::Overwrite?"CONFIRM OVERWRITE":page==Game::MenuPage::ConfirmLoad?"CONFIRM LOAD":"CONFIRM RESTART";
 uiText(x+16,y+9,title,bone,0,20,1,2);
 const char* subtitle=settingsPage?(game.menuFromTitle()?"AUDIO / CONTROLS":"SETTINGS / SAVED GAMES"):(game.menuFromTitle()?"SELECT SAVE SLOT":"GAMEPLAY IS PAUSED");
 uiText(x+MenuLayout::Width-16-uiTextWidth(subtitle,0,10,1),y+16,subtitle,red,0,10,1,1);
 const char* labels[]={"RESUME","MASTER VOLUME","MUSIC VOLUME","EFFECTS VOLUME","MOUSE SENSITIVITY","INVERT MOUSE Y","DEPTH OF FIELD","LENS EFFECTS","RESTART CURRENT GAME...","SAVE GAME...","LOAD GAME...",game.menuFromTitle()?"QUIT GAME":"QUIT TO TITLE"};
 auto&settings=game.settings();
 for(int row=0;row<game.menuRows();++row){int top=MenuLayout::RowTop+row*MenuLayout::RowHeight;bool selected=row==game.menuSelection();
  if(selected){gradient(x+10,top,MenuLayout::Width-20,19,red,.5f,0.f,true);fill(x+10,top,2,19,red,1);}
  else fill(x+10,top+18,MenuLayout::Width-20,1,bone,.05f);
  const char* label=settingsPage?(row==0&&game.menuFromTitle()?"BACK TO TITLE":labels[row]):page==Game::MenuPage::Save?(row==3?(game.menuFromTitle()?"BACK TO TITLE":"BACK"):game.slotLabel(row).c_str()):page==Game::MenuPage::Load?(row==4?(game.menuFromTitle()?"BACK TO TITLE":"BACK"):row==3?game.checkpointLabel().c_str():game.slotLabel(row).c_str()):(row==0?"CANCEL":page==Game::MenuPage::Overwrite?"OVERWRITE SAVED GAME":page==Game::MenuPage::ConfirmLoad?"LOAD / REPLACE CURRENT PROGRESS":"RESTART / DISCARD CURRENT PROGRESS");
  uiText(x+20,top+2,label,selected?bone:dim,0,13,1,1);
  if(settingsPage&&((row>=1&&row<=4)||row==6||row==7)){float value=row==1?settings.master:row==2?settings.music:row==3?settings.effects:row==4?settings.sensitivity:row==6?settings.depthOfField:settings.lensEffects;
   float normalized=std::clamp(row==4?(value-.2f)/2.8f:row>=6?value*.5f:value,0.f,1.f);int knob=MenuLayout::SliderX+int(normalized*MenuLayout::SliderWidth);
   fill(MenuLayout::SliderX,top+9,MenuLayout::SliderWidth,1,bone,.22f);fill(MenuLayout::SliderX,top+8,knob-MenuLayout::SliderX,3,selected?red:bone,selected?1.f:.55f);
   fill(knob-1,top+4,3,11,bone,1);
   char valueText[16];if(row==4)std::snprintf(valueText,sizeof(valueText),"%.1fX",value);else if(row>=6)std::snprintf(valueText,sizeof(valueText),"%d%%",int(value*100+.5f));else std::snprintf(valueText,sizeof(valueText),"%d",int(value*100+.5f));
   uiText(MenuLayout::SliderX+82,top+3,valueText,bone,3,11);
  }
  if(settingsPage&&row==5)uiText(MenuLayout::SliderX,top+3,settings.invertMouse?"ON":"OFF",settings.invertMouse?red:bone,3,11);
 }
 if(!settingsPage){uiText(x+16,y+150,game.menuMessage().c_str(),red,0,11,1,1);if(page==Game::MenuPage::Overwrite)uiText(x+16,y+172,"The previous slot will be replaced.",dim,1,12);if(page==Game::MenuPage::ConfirmLoad||page==Game::MenuPage::ConfirmRestart)uiText(x+16,y+172,"Unsaved progress will be lost.",dim,1,12);}
 fill(x+10,y+MenuLayout::Height-28,MenuLayout::Width-20,1,bone,.10f);
 uiText(x+16,y+MenuLayout::Height-22,"CLICK / ARROWS / ENTER",dim,0,10,1,1);
 const char* back=settingsPage?(game.menuFromTitle()?"ESC  TITLE":"ESC  RESUME"):"ESC  BACK";uiText(x+MenuLayout::Width-16-uiTextWidth(back,0,10,1),y+MenuLayout::Height-22,back,red,0,10,1,1);
}
void SoftwareRenderer::drawInventory(const Game& game){
 if(!hardwarePresentsWindow())for(auto&pixel:m_pixels)pixel=shade(pixel,.22f);
 const auto paper=rgb(236,228,214),amber=rgb(214,32,26),muted=rgb(146,138,128);
 int x=48,y=28,w=m_width-96,h=m_height-56;fill(0,0,m_width,m_height,rgb(0,0,0),.4f);uiPanel(x,y,w,h);fill(x,y,w,2,rgb(214,32,26),1);
 label(x+16,y+12,"FIELD INVENTORY",paper,2);label(x+w-112,y+15,"I / CLOSE",muted);
 auto section=[&](int sx,int sy,int sw,int sh,const char* title){uiPanel(sx,sy,sw,sh);label(sx+8,sy+8,title,amber);};
 section(x+14,y+36,260,72,"PRIMARY WEAPON");section(x+14,y+116,260,72,"SECONDARY / MELEE");section(x+14,y+196,260,72,"EQUIPMENT / KEY ITEMS");
 section(x+286,y+36,w-300,h-50,"STORAGE");
 // Orthographic thumbnails use the same textured meshes as the world items.
 auto icon=[&](Mesh&mesh,const Texture&texture,int ix,int iy,int iw,int ih,bool gun){
  auto center=(mesh.minimum+mesh.maximum)*.5f;
  auto project=[&](Point3 p){p=p-center;return gun?Point3{-p.z,p.y,p.x}:Point3{p.x*.94f+p.z*.34f,p.y,p.z*.94f-p.x*.34f};};
  float rx=.001f,ry=.001f;for(auto&t:mesh.triangles)for(auto&v:t.v){auto p=project(v.p);rx=std::max(rx,std::fabs(p.x));ry=std::max(ry,std::fabs(p.y));}
  float scale=std::min((iw-4)/(2*rx),(ih-4)/(2*ry));std::vector<float> depth(iw*ih,1e9f);
  for(auto&t:mesh.triangles){Point3 p[3];for(int k=0;k<3;++k){p[k]=project(t.v[k].p);p[k].x=iw*.5f+p[k].x*scale;p[k].y=ih*.5f-p[k].y*scale;}
   float det=(p[1].y-p[2].y)*(p[0].x-p[2].x)+(p[2].x-p[1].x)*(p[0].y-p[2].y);if(std::fabs(det)<.001f)continue;
   int left=std::max(0,int(std::floor(std::min({p[0].x,p[1].x,p[2].x})))),right=std::min(iw-1,int(std::ceil(std::max({p[0].x,p[1].x,p[2].x}))));
   int top=std::max(0,int(std::floor(std::min({p[0].y,p[1].y,p[2].y})))),bottom=std::min(ih-1,int(std::ceil(std::max({p[0].y,p[1].y,p[2].y}))));
   for(int py=top;py<=bottom;++py)for(int px=left;px<=right;++px){float a=((p[1].y-p[2].y)*(px+.5f-p[2].x)+(p[2].x-p[1].x)*(py+.5f-p[2].y))/det,b=((p[2].y-p[0].y)*(px+.5f-p[2].x)+(p[0].x-p[2].x)*(py+.5f-p[2].y))/det,c=1-a-b;
    if(a<0||b<0||c<0)continue;float z=a*p[0].z+b*p[1].z+c*p[2].z;if(z>=depth[py*iw+px])continue;depth[py*iw+px]=z;
    put(ix+px,iy+py,sample(texture,a*t.v[0].u+b*t.v[1].u+c*t.v[2].u,a*t.v[0].v+b*t.v[1].v+c*t.v[2].v));
   }
  }
 };
 if(game.weaponEquipped())icon(m_weaponMesh,m_weaponTexture,76,84,228,34,true);
 label(76,123,game.weaponEquipped()?"SHOTGUN / CLICK TO SELECT":"EMPTY / CLICK TO EQUIP SHOTGUN",paper);
 label(76,173,"FISTS / CLICK TO HOLSTER",paper);label(76,192,"RIGHT CLICK IN WORLD TO GUARD",muted);
 label(76,258,"ARMOR: EMPTY",muted);
 int keyY=273,shown=0;for(const auto&item:game.questItems()){if(shown>=2)break;std::string name=std::string(Game::questItemName(item.id))+(item.count>1?" X"+std::to_string(item.count):"");label(76,keyY+shown*11,name.c_str(),paper);++shown;}
 if(int(game.questItems().size())>shown){std::string more="+ "+std::to_string(int(game.questItems().size())-shown)+" MORE KEY ITEMS";label(76,keyY+shown*11,more.c_str(),muted);}
 for(int row=0;row<5;++row)for(int col=0;col<6;++col){fill(350+col*34,94+row*29,31,26,rgb(236,228,214),.05f);frame(350+col*34,94+row*29,31,26,rgb(236,228,214),.10f);}
 int occupied=0;for(int item=0;item<3;++item){if((item==0&&game.weaponEquipped())||(item==1&&game.player().ammo==0)||(item==2&&game.medkits()==0))continue;
  int cell=game.itemCell(item),ix=350+cell%6*34,iy=94+cell/6*29,iw=item==0?133:item==1?31:65;occupied+=item==0?8:item==1?2:4;
  fill(ix,iy,iw,55,rgb(30,26,24),.85f);if(game.selectedItem()==item){rect(ix,iy,iw,1,amber);rect(ix,iy,1,55,amber);rect(ix+iw-1,iy,1,55,amber);rect(ix,iy+54,iw,1,amber);}
  icon(item==0?m_weaponMesh:item==1?m_shellsMesh:m_medkitMesh,item==0?m_weaponTexture:item==1?m_shellsTexture:m_medkitTexture,ix+3,iy+3,iw-6,39,item==0);
  char count[20];std::snprintf(count,sizeof(count),"%d",item==0?1:item==1?game.player().ammo:game.medkits());label(ix+5,iy+45,count,paper);
 }
 char status[40];std::snprintf(status,sizeof(status),"%d / 30 CELLS  HP %d",occupied,int(game.player().health));label(350,250,status,muted);
 const char* names[]={"SHOTGUN / 4 X 2","12 GA SHELLS / 1 X 2","FIRST AID / 2 X 2"};label(350,267,game.selectedItem()<0?"CLICK AN ITEM TO SELECT":names[game.selectedItem()],paper);
 fill(350,280,224,28,rgb(214,32,26),.18f);label(358,291,game.selectedItem()==0?"E / ENTER: EQUIP OR STOW":game.selectedItem()==2?"E / ENTER: HEAL 35 HP":"AMMO IS USED BY THE SHOTGUN",amber);
 label(64,301,"SELECT ITEM THEN EMPTY CELL TO MOVE",muted);label(64,316,"I / ESC CLOSE   CLICK PRIMARY TO EQUIP",amber);
}
void SoftwareRenderer::drawConsole(const Game& game){
 rect(0,0,m_width,152,rgb(10,14,16));rect(0,150,m_width,2,rgb(202,150,67));
 text(12,9,"DEPTHWORKS / DEVELOPER CONSOLE",rgb(218,172,89),2);
 auto&log=game.consoleLog();size_t first=log.size()>9?log.size()-9:0;
 int y=28;for(size_t i=first;i<log.size();++i,y+=11)text(12,y,log[i].substr(0,150).c_str(),rgb(188,204,196));
 text(12,132,("> "+game.consoleLine()+"_").c_str(),rgb(245,212,142));
}
void SoftwareRenderer::render(const Game& game){auto start=std::chrono::steady_clock::now();auto afterSubmission=start;m_view=game.view();
 m_cleanupReachError=m_cleanupFingerError=m_cleanupBarrelError=0;
 m_cleanupEdgeStretch=1;
 bool directPresentation=hardwarePresentsWindow();
 if(m_lightingWorld!=game.worldId()||m_lightingSession!=game.sessionRevision()){
  m_chunkLighting={};m_chunkNormalLighting={};m_chunkLightCells={};m_chunkLightCounts={};m_chunkLightSourceKeys={};
  if(m_gpu)m_gpu->clearStaticCaches();
  m_lightingWorld=game.worldId();m_lightingSession=game.sessionRevision();
 }
 int logicalWidth=m_width,logicalHeight=m_height;
 bool nativeTarget=directPresentation&&m_gpu&&(m_gpu->hasSurface()||m_gpu->vrActive());
 int fullWidth=logicalWidth,fullHeight=logicalHeight;
 if(nativeTarget){auto extent=m_gpu->surfaceExtent();m_width=std::max(1,int(extent.first*game.renderScale()));m_height=std::max(1,int(extent.second*game.renderScale()));}
 bool scaled=!nativeTarget&&game.renderScale()<1;
 int sceneWidth=m_width,sceneHeight=m_height;
 if(scaled){m_width=int(fullWidth*game.renderScale());m_height=int(fullHeight*game.renderScale());m_scenePixels.resize(size_t(m_width*m_height));m_sceneZ.resize(size_t(m_width*m_height));m_pixels.swap(m_scenePixels);m_zbuffer.swap(m_sceneZ);}
 auto scene=[&]{bool parallel=!m_vrRendering&&!game.titleScreen()&&m_gpuFrame&&m_animationWorker&&!game.holdingClutter();m_poseReady=false;
  if(parallel)m_animationWorker->start([&]{prepareViewModel(game);});
  try{
   const float background=1.f-game.darkness();clear(rgb(int(12*background),int(16*background),int(18*background)));
   {perf::Scope profile(perf::Sky);drawSky(game);}
   drawScene(game);for(int level=0;level<game.chunkCount();++level)if(level!=game.level()&&game.chunkResident(level))drawScene(game,false,level);
   if(parallel){perf::Scope profile(perf::PoseWait);m_animationWorker->wait();m_poseReady=true;}if(m_vrRendering&&!game.titleScreen())drawTrackedHands(game);else if(!game.titleScreen()&&!m_environmentInspection&&((game.hiddenLocker()<0&&!game.lockerViewLocked())||game.lockerHands()))drawViewModel(game);m_poseReady=false;
  }catch(...){if(parallel)m_animationWorker->wait();m_poseReady=false;throw;}
 };
 if(m_gpu){try{if(m_vrReuseScene)m_gpu->beginReuse();else m_gpu->begin(m_width,m_height);auto origin=game.chunkOffset(game.level());float muzzleFlash=game.muzzleFlash();m_gpu->setView(m_view.pos.x+origin.x,m_view.pos.y+origin.y,m_view.z+m_view.eye,m_view.angle,m_view.pitch/140.f,float(m_width)/std::max(1,m_height),game.flashlightOn(),muzzleFlash,game.elapsed());
  std::array<float,16> fogLights{};std::array<float,4> fogPowers{};std::array<float,4> distances{144.f,144.f,144.f,144.f};
  const auto&world=game.world();if(!world.outdoors())for(const auto&lamp:world.lights()){
   if(lamp.mount==WorldLightMount::Wall||lamp.intensity<=0)continue;
   // A switched-off lamp keeps its slot at zero power, so shafts never swap.
   float dx=lamp.position.x-m_view.pos.x,dy=lamp.position.y-m_view.pos.y,d2=dx*dx+dy*dy;
   float floor=world.floorHeight(lamp.position.x,lamp.position.y)+.08f;
   float top=std::min(lamp.z,world.clearanceAbove(lamp.position.x,lamp.position.y,floor)-.04f);
   float bottom=std::max(floor,top-4.5f);
   float ahead=dx*std::cos(m_view.angle)+dy*std::sin(m_view.angle);
   float eyeZ=m_view.z+m_view.eye;
   // Distance only: a facing test swapped lamps in and out of the four post
   // slots while turning, popping lamp shadows and glow.
   if(top-bottom<.8f||floor>eyeZ+2.f||top<eyeZ-2.f||d2>=distances[3])continue;(void)ahead;
   int slot=3;while(slot>0&&d2<distances[slot-1]){distances[slot]=distances[slot-1];fogPowers[slot]=fogPowers[slot-1];for(int c=0;c<4;++c)fogLights[slot*4+c]=fogLights[(slot-1)*4+c];--slot;}
   distances[slot]=d2;fogPowers[slot]=lamp.intensity*game.lampPower(lamp);fogLights[slot*4]=lamp.position.x+origin.x;fogLights[slot*4+1]=lamp.position.y+origin.y;fogLights[slot*4+2]=top;fogLights[slot*4+3]=bottom;
  }
  // Per-pixel lamps: every resident lamp in world space, in stable order so
  // shadow maps are reused until the set itself changes.
  // Animated authored lighting needs the per-frame lamp buffer even when the
  // diagnostic vertex-bake option is selected. Keep this decision independent
  // of power/state so switching never invalidates geometry or shadows.
  bool pixelLighting=g_pixelLighting;
  for(int level=0;level<game.chunkCount()&&!pixelLighting;++level){
   if(level!=game.level()&&!game.chunkResident(level))continue;
   const auto&chunk=game.chunkWorld(level);
   pixelLighting=!chunk.darkZones().empty()||std::any_of(chunk.lights().begin(),chunk.lights().end(),[](const WorldLight&light){return light.switchable();});
  }
  if(pixelLighting!=m_pixelLighting){m_pixelLighting=pixelLighting;m_chunkLighting={};m_chunkNormalLighting={};m_gpu->clearStaticCaches();}
  // Switchable lamps keep their full intensity in the list (a stable key and
  // shadow maps) and their power goes in per frame beside it.
  {std::vector<std::array<float,7>> lamps;std::vector<float> powers;
   for(int level=0;level<game.chunkCount();++level){if(level!=game.level()&&!game.chunkResident(level))continue;
    const auto&chunk=game.chunkWorld(level);auto offset=game.chunkOffset(level);
    for(const auto&lamp:chunk.lights()){if((chunk.hasLift()&&&lamp==&chunk.lights().back())||lamp.intensity<=0)continue;auto e=lamp.emitter();lamps.push_back({e.x+offset.x,e.y+offset.y,e.z,lamp.range,lamp.intensity,lamp.kelvin,lamp.tint});powers.push_back(game.lampPower(lamp));}
   }
   if(lamps.size()>256){Point3 eye{m_view.pos.x+origin.x,m_view.pos.y+origin.y,m_view.z+m_view.eye};
    std::vector<std::pair<float,size_t>> order;for(size_t i=0;i<lamps.size();++i){float dx=lamps[i][0]-eye.x,dy=lamps[i][1]-eye.y,dz=lamps[i][2]-eye.z;order.push_back({dx*dx+dy*dy+dz*dz,i});}
    std::nth_element(order.begin(),order.begin()+256,order.end());order.resize(256);std::sort(order.begin(),order.end(),[](const auto&a,const auto&b){return a.second<b.second;});
    std::vector<std::array<float,7>> nearest;std::vector<float> nearestPowers;for(auto&entry:order){nearest.push_back(lamps[entry.second]);nearestPowers.push_back(powers[entry.second]);}lamps.swap(nearest);powers.swap(nearestPowers);}
   m_gpu->setLamps(lamps,m_pixelLighting,g_detailLayer);m_gpu->setLampPowers(powers);
  }
  // Sun cascades: three view-frustum slices, each a texel-snapped square in
  // the light's frame so the shadow edge does not crawl as the camera moves.
  {std::array<float,56> sunData{};const auto sun=world.sun();sunData[7]=world.wetness();
   if(sun.intensity>0){
    sunData[0]=sun.direction.x;sunData[1]=sun.direction.y;sunData[2]=sun.direction.z;sunData[3]=sun.intensity;sunData[4]=sun.r;sunData[5]=sun.g;sunData[6]=sun.b;
    auto cross=[](Point3 a,Point3 b){return Point3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};Point3 forward=sun.direction*-1.f,right=cross(forward,Point3{0,0,1});float rl=std::sqrt(right.x*right.x+right.y*right.y+right.z*right.z);right=right*(1.f/std::max(rl,1e-4f));Point3 up=cross(right,forward);
    const auto&p=m_view;float cy=std::cos(p.angle),sy=std::sin(p.angle),cp=std::cos(p.pitch/140.f),sp=std::sin(p.pitch/140.f);
    Point3 eye{p.pos.x+origin.x,p.pos.y+origin.y,p.z+p.eye},viewForward{cy*cp,sy*cp,sp};float aspect=float(m_width)/std::max(1,m_height);
    const float splits[4]={.3f,7.f,22.f,70.f};
    for(int c=0;c<3;++c){float n=splits[c],f=splits[c+1],cd=(n+f)*.5f;Point3 centre=eye+viewForward*cd;
     float hx=f/1.3f,hy=f/(1.3f*aspect),r=std::sqrt((f-cd)*(f-cd)+hx*hx+hy*hy),texel=2.f*r/2048.f;
     auto dot=[](Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
     float sr=dot(centre,right),su=dot(centre,up);centre=centre-right*(sr-std::floor(sr/texel)*texel)-up*(su-std::floor(su/texel)*texel);
     float* k=sunData.data()+8+c*16;k[0]=centre.x;k[1]=centre.y;k[2]=centre.z;k[3]=texel;k[4]=right.x;k[5]=right.y;k[6]=right.z;k[7]=1.f/r;k[8]=up.x;k[9]=up.y;k[10]=up.z;k[11]=1.f/r;k[12]=forward.x;k[13]=forward.y;k[14]=forward.z;k[15]=1.f/90.f;
    }
   }
   m_gpu->setSun(sunData);}
  m_gpu->setLens(game.settings().depthOfField,game.settings().lensEffects);
  m_gpu->setFogLights(fogLights,fogPowers);m_gpu->setCrispTexels(g_crispTexels);m_gpu->setTemporalAA(g_temporalAA);m_gpu->setTiltShift(g_tiltShift);m_gpu->setPostProcessing(g_postProcessing,g_postOcclusionView);
  std::array<float,4> atmosphere=world.outdoors()?std::array<float,4>{.43f,.52f,.62f,.010f}:std::array<float,4>{.11f,.15f,.18f,.008f};
  if(world.zoneSky())atmosphere={.55f,.64f,.74f,.0035f};
  m_gpu->setSkyStyle(world.zoneSky()?1:0);
  if(world.outdoors()&&std::strcmp(world.skyboxId(),"brutal_wasteland")==0)atmosphere=world.coast()?std::array<float,4>{.62f,.74f,.85f,.006f}:std::array<float,4>{.39f,.39f,.40f,.012f};
  if(world.waterSurface(m_view.pos.x,m_view.pos.y)>m_view.z+m_view.eye+.03f)atmosphere={.10f,.20f,.22f,.085f};
  // True darkness: haze goes black (it still swallows distance), the ambient
  // fill and exposure lift go, and a shot lights the room out to 12 m.
  const float darkness=game.darkness();for(int c=0;c<3;++c)atmosphere[size_t(c)]*=1.f-darkness;
  m_gpu->setDarkness(darkness,.045f-.033f*darkness,game.weaponFlashes());
  m_gpu->setAtmosphere(atmosphere);m_gpu->setActorEmitCaching(m_actorEmitCaching);m_gpuFrame=true;auto a=std::chrono::steady_clock::now();perf::costs[perf::RenderSetup]+=std::chrono::duration<double,std::milli>(a-start).count();if(!m_vrReuseScene)scene();auto b=std::chrono::steady_clock::now();m_gpu->finish(m_pixels);auto c=std::chrono::steady_clock::now();afterSubmission=c;m_sceneMs=std::chrono::duration<double,std::milli>(b-a).count();m_submitMs=std::chrono::duration<double,std::milli>(c-b).count();m_gpuFrame=false;}
  catch(const std::exception&e){if(m_vrRendering)throw;m_gpuFrame=false;m_gpu.reset();m_gpuName="Software fallback: "+std::string(e.what());std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';
   // Mid-session fallbacks are rare and look like a sudden quality drop; keep an appended record for diagnosis.
   std::ofstream("RawMetal-renderer-events.txt",std::ios::app)<<"Vulkan frame failed, falling back to software: "<<e.what()<<" (map "<<game.level()<<" at "<<game.player().pos.x<<','<<game.player().pos.y<<")\n";m_width=logicalWidth;m_height=logicalHeight;m_pixels.resize(size_t(m_width*m_height));m_zbuffer.resize(size_t(m_width*m_height));directPresentation=false;nativeTarget=false;scene();}}
 else scene();
 bool underwater=!game.titleScreen()&&game.world().waterSurface(m_view.pos.x,m_view.pos.y)>m_view.z+m_view.eye+.03f;
 if(!directPresentation&&underwater){
  const auto source=m_pixels;
  for(int y=0;y<m_height;++y){int wobble=int(std::sin(game.elapsed()*7.f+y*.055f)*2.f);
   for(int x=0;x<m_width;++x){auto c=source[size_t(y*m_width+std::clamp(x+wobble,0,m_width-1))];
    unsigned r=(c>>16&255u)*4/10,g=(c>>8&255u)*8/10,b=(c&255u)*9/10;
    m_pixels[size_t(y*m_width+x)]=0xff000000u|(r<<16)|(g<<8)|b;
   }
  }
 }
 // CPU twin of the composite alarm wash for captures and the software path.
 if(float alarm=!directPresentation?game.alarmPulse()*.55f:0.f;alarm>0){
  const float r=1+.45f*alarm,g=1-.70f*alarm,bl=1-.76f*alarm;const unsigned lift=unsigned(9*alarm);
  for(auto&c:m_pixels){unsigned cr=std::min(255u,unsigned((c>>16&255u)*r)+lift),cg=unsigned((c>>8&255u)*g),cb=unsigned((c&255u)*bl);c=(c&0xff000000u)|(cr<<16)|(cg<<8)|cb;}
 }
 if(scaled){int scaledWidth=m_width,scaledHeight=m_height;m_width=fullWidth;m_height=fullHeight;m_pixels.swap(m_scenePixels);m_zbuffer.swap(m_sceneZ);
  for(int y=0;y<m_height;++y)for(int x=0;x<m_width;++x)m_pixels[size_t(y*m_width+x)]=m_scenePixels[size_t((y*scaledHeight/m_height)*scaledWidth+x*scaledWidth/m_width)];}
 if(nativeTarget){m_width=logicalWidth;m_height=logicalHeight;}
 if(directPresentation)std::fill(m_pixels.begin(),m_pixels.end(),0u);
 if(!m_environmentInspection){if(game.titleScreen())drawTitle(game);else {if(m_vrRendering){std::fill(m_pixels.begin(),m_pixels.end(),0xff000000u);drawTerminal(game);}else drawHud(game);if(game.consoleOpen())drawConsole(game);else if(game.paused())drawSettings(game);else if(game.inventoryOpen())drawInventory(game);}if(game.loading())drawLoading(game);}
 if(m_vrRendering){auto [x,y]=VrRuntime::active()->pointer();if(x>=0&&y>=0){rect(x-4,y-4,9,9,0xff101010u);rect(x-3,y-1,7,3,0xfff4f4f0u);rect(x-1,y-3,3,7,0xfff4f4f0u);}}
 auto overlayEnd=std::chrono::steady_clock::now();perf::costs[perf::Overlay]+=std::chrono::duration<double,std::milli>(overlayEnd-afterSubmission).count();float ms=std::chrono::duration<float,std::milli>(overlayEnd-start).count();m_frameMs=m_frameMs==0?ms:m_frameMs*.9f+ms*.1f;
 if(game.showFps()){char info[96];std::snprintf(info,sizeof(info),"%s %dX%d RENDER %.1F MS / %.0F FPS",m_gpu?"VULKAN":"CPU",sceneWidth,sceneHeight,m_frameMs,1000.f/std::max(.01f,m_frameMs));text(12,m_height-50,info,rgb(225,200,130));}
 m_presentMs=0;
 if(directPresentation){auto presentStart=std::chrono::steady_clock::now();float sceneDim=game.loading()?.08f:game.titleScreen()?.17f:game.paused()?.25f:game.inventoryOpen()?.22f:1.f;float shotKick=std::max(std::clamp(1.f-game.shotAge()/.10f,0.f,1.f)*(game.weaponKick()>.85f&&!game.unarmed()?1.f:0.f),game.damageFlash()*.65f);m_gpu->present(m_pixels.data(),m_width,m_height,underwater,sceneDim,game.damageFlash(),shotKick,game.alarmPulse());m_presentMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-presentStart).count();}
}
}








namespace retro {
void SoftwareRenderer::saveSunCascades(const char*prefix){if(m_gpu)m_gpu->saveSunCascades(prefix);}
std::uint64_t SoftwareRenderer::shadowFacesDrawn()const{return m_gpu?m_gpu->shadowFacesDrawn():0;}
bool SoftwareRenderer::saveSceneCapture(const char*path){
 if(!m_gpu)return false;std::vector<uint32_t> pixels;std::vector<float> depth;int width=0,height=0;m_gpu->captureScene(pixels,width,height,&depth);
 // Planar depth beside the image (raw float32), for offline composite previews.
 if(!depth.empty()){std::ofstream depthFile(std::string(path)+".depth",std::ios::binary);depthFile.write(reinterpret_cast<const char*>(depth.data()),std::streamsize(depth.size()*sizeof(float)));}
 std::ofstream file(path,std::ios::binary);file<<"P6\n"<<width<<" "<<height<<"\n255\n";for(auto p:pixels){char rgb[]={char(p>>16),char(p>>8),char(p)};file.write(rgb,3);}return true;
}
void SoftwareRenderer::saveVrEye(const char*path){std::vector<uint32_t> pixels;m_gpu->captureVrEye(pixels);auto [width,height]=m_gpu->surfaceExtent();std::ofstream file(path,std::ios::binary);file<<"P6\n"<<width<<" "<<height<<"\n255\n";for(auto p:pixels){char rgb[]={char(p>>16),char(p>>8),char(p)};file.write(rgb,3);}}
void SoftwareRenderer::renderVrEye(const Game&game,int eye){
 if(!m_gpu||!m_gpu->vrActive())throw std::runtime_error("VR requires the Vulkan renderer");
 m_vrRendering=true;
 try{m_gpu->setVrEye(eye,VrRuntime::active()->eye(eye,game).clip);if(eye==0){
  if(!game.titleScreen())prepareVrDisplays(game);
  const auto&hands=VrRuntime::active()->hands();for(int side=0;side<2;++side)m_vrHandMeshes[side]=hands[side].tracked?m_armsMesh.trackedHand(side==0,hands[side].curls):std::vector<MeshTriangle>{};}m_vrReuseScene=eye==1;render(game);m_vrReuseScene=false;}
 catch(...){m_vrRendering=false;m_vrReuseScene=false;throw;}
 m_vrRendering=false;
}}

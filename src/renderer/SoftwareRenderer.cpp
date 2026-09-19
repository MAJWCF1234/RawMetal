#include "SoftwareRenderer.h"
#include "GpuRenderer.h"
#include "FrameWorker.h"
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
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/stb/stb_image.h"

namespace retro {
namespace {
constexpr float FOV = 75.0f * kPi / 180.0f;
std::uint32_t rgb(int r,int g,int b){ return 0xff000000u | (std::uint32_t(r)<<16) | (std::uint32_t(g)<<8) | std::uint32_t(b); }

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
int gi(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='A'&&c<='Z')return 10+c-'A'; if(c==' ')return 36;const char* extra="/-.><%()";auto found=std::strchr(extra,c);return found?38+int(found-extra):37; }
}

SoftwareRenderer::~SoftwareRenderer()=default;
bool SoftwareRenderer::enableHardware(){
 if(m_gpu)return true;
 // Delay-load the system Vulkan loader so unsupported machines can still run.
 static HMODULE loader=LoadLibraryW(L"vulkan-1.dll");
 if(!loader){m_gpuName="Software (Vulkan loader unavailable)";std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';return false;}
 try{m_gpu=std::make_unique<GpuRenderer>();
  for(const auto*texture:{&m_muzzleFlash,&m_pumpTexture,&m_compressorTexture,&m_pipeTexture,&m_gateTexture,&m_pressureWall,&m_pressureFloor,&m_pressureMetal,&m_transferSign,&m_pumpSign,&m_controlSign,&m_surfaceSign,&m_gantrySign,&m_reactorSign,&m_liftSign,&m_liftDispatch,&m_wall,&m_floor,&m_metal,&m_arms,&m_weaponTexture,&m_enemyTexture,&m_waspTexture,&m_bruteTexture,&m_wingTexture,&m_medkitTexture,&m_shellsTexture,&m_barrelTexture,&m_crateTexture,&m_concrete,&m_bulkhead,&m_intakeSign,&m_processingSign,&m_containmentSign,&m_exitSign,&m_hazard,&m_chemicalSign,&m_machineSign,&m_confinedSign,&m_signRust,&m_panelMetal,&m_routePaint,&m_redPaint,&m_terminalTexture,&m_cautionSign,&m_serviceSign})m_gpu->prepare(*texture);
  for(const auto*texture:{&m_blood,&m_wardenTexture,&m_consoleTexture,&m_feedSign,&m_returnSign,&m_diskSign,&m_authSign})m_gpu->prepare(*texture);
  for(const auto&texture:m_hazmatTextures)m_gpu->prepare(texture);
  for(const auto&texture:m_clutterTextures)m_gpu->prepare(texture);for(const auto&entry:m_facilityTextures)m_gpu->prepare(entry.second);
  for(uint32_t color:{0xffd1f1dau,0xffdf9849u,0xff53aec4u,0xff343834u,0xffb84728u,0xff302c27u}){Texture paint{1,1,{color}};m_gpu->prepare(paint);}
  m_animationWorker=std::make_unique<FrameWorker>();m_gpuName="Vulkan / "+m_gpu->adapter();std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';return true;}
 catch(const std::exception&e){m_gpu.reset();m_gpuName="Software fallback: "+std::string(e.what());std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';return false;}
}
SoftwareRenderer::SoftwareRenderer(int w,int h):m_width(w),m_height(h),m_pixels(size_t(w*h)),m_depth(size_t(w),9999.f),m_zbuffer(size_t(w*h),9999.f){m_wall=loadTexture(101);m_floor=loadTexture(102);m_metal=loadTexture(103);m_arms=loadTexture(106);m_weaponTexture=loadTexture(112);m_enemyTexture=loadTexture(113);m_waspTexture=loadTexture(115);m_bruteTexture=loadTexture(117);m_wingTexture=loadTexture(118);
 const char* materialNames[]={"wall_6","wall_7","wall_8","wall_5","floor_1","ceiling_1","vent_1","lamp_1_on","door_1","generator_1","metal_4","metal_3","metal_6","wall_box_2","stairs_1"};
 for(int i=0;i<15;++i)m_facilityTextures.emplace(materialNames[i],loadTexture(172+i));
 m_consoleTexture=loadTexture(241);m_wardenTexture=loadTexture(243);
 for(auto& texture:m_hazmatTextures)texture=loadTexture(246);
 m_blood=loadTexture(249);for(auto&pixel:m_blood.pixels)if((pixel&0xffffffu)<0x100000u)pixel=0;prepareDecal(m_blood);
 m_facilityTextures.emplace("pc_1",loadTexture(192));m_facilityTextures.emplace("keyboard_1",loadTexture(193));
 {auto emission=loadTexture(194);auto&lamp=m_facilityTextures.at("lamp_1_on");if(emission.width!=lamp.width||emission.height!=lamp.height)throw std::runtime_error("Lamp emission dimensions mismatch");lamp.emission=std::move(emission.pixels);}
 m_barrelTexture=loadTexture(122);m_crateTexture=loadTexture(124);m_concrete=loadTexture(125);m_bulkhead=loadTexture(126);
 for(int i=0;i<6;++i)m_clutterTextures[i]=loadTexture(152+i*2);
 m_medkitTexture=loadTexture(134);m_shellsTexture=loadTexture(136);
 m_terminalTexture=loadTexture(137);m_cautionSign=loadTexture(138);prepareDecal(m_cautionSign);
 m_muzzleFlash=loadTexture(139);m_muzzleFlash.additive=true;prepareDecal(m_muzzleFlash);
 m_pumpTexture=loadTexture(141);m_compressorTexture=loadTexture(143);m_pipeTexture=loadTexture(145);m_gateTexture=loadTexture(147);
 m_pressureWall=loadTexture(148);m_pressureFloor=loadTexture(149);m_pressureMetal=loadTexture(150);
 attachNormal(m_wall,187);attachNormal(m_pressureWall,188);attachNormal(m_bulkhead,189);attachNormal(m_floor,190);
 m_hazard=loadTexture(127);m_chemicalSign=loadTexture(128);m_machineSign=loadTexture(129);m_confinedSign=loadTexture(130);m_signRust=loadTexture(131);m_panelMetal=loadTexture(132);
 for(auto*decal:{&m_hazard,&m_chemicalSign,&m_machineSign,&m_confinedSign})prepareDecal(*decal);
 m_routePaint=makePaint(0xffb99348u);m_redPaint=makePaint(0xff954732u);
 m_intakeSign=makeSign("01 / INTAKE","FREIGHT ACCESS",0xffca994du);m_processingSign=makeSign("02 / FOUNDRY","KEEP CLEAR",0xffd9984cu);
 m_containmentSign=makeSign("03 / CONTAINMENT","BIOHAZARD",0xffb45c42u);m_exitSign=makeSign("DISPATCH","AUTHORIZED PERSONNEL",0xff83baa7u);
 m_serviceSign=makeSign("SERVICE 04","LOW CLEARANCE",0xffca994du);
 m_transferSign=makeSign("PRESSURE WORKS","TRANSFER / 02",0xffa7a766u);m_pumpSign=makeSign("PUMP HALL","HIGH PRESSURE",0xffa7a766u);
 m_controlSign=makeSign("CONTROL GALLERY","SWITCHGEAR",0xffa7a766u);m_surfaceSign=makeSign("SURFACE LIFT","EXTRACTION",0xffa7a766u);m_gantrySign=makeSign("TURBINE GANTRY","TRANSFER / 03",0xffa7a766u);
 m_reactorSign=makeSign("REACTOR CORE","CONTAINMENT BREACH",0xffbf583eu);
 m_liftSign=makeSign("FREIGHT / 03","MAX LOAD 4000 KG",0xffd7ac64u);m_liftDispatch=makeSign("SURFACE / UP","DISPATCH CONTROL",0xff9fceaeu);
 m_feedSign=makeSign("FEED","P-01",0xffd7ac64u);m_returnSign=makeSign("RETURN","P-02",0xff53aec4u);
 m_diskSign=makeSign("MAINTENANCE","SERVICE BENCH",0xffd7ac64u);m_authSign=makeSign("CONTROL","R-03",0xff53aec4u);
}
SoftwareRenderer::Texture SoftwareRenderer::makeSign(const char* title,const char* subtitle,std::uint32_t accent){
 Texture sign{256,80,std::vector<std::uint32_t>(256*80)};
 for(int y=0;y<80;++y)for(int x=0;x<256;++x){auto rust=sample(m_signRust,x/256.f,y/80.f),metal=sample(m_panelMetal,x/256.f,y/80.f);int edge=std::min({x,y,255-x,79-y});
  unsigned wear=(rust>>16)&255;float light=.65f+wear/180.f;
  auto color=edge<4?shade(rust,1.1f):shade(metal,light);
  if(edge>=5&&edge<8)color=wear>77?rust:shade(accent,.55f+wear/230.f);
  if(edge<13&&wear>83)color=rust;
  sign.pixels[y*256+x]=color;
 }
 auto label=[&](int x,int y,const char* value,int scale,std::uint32_t color){for(;*value;++value,x+=4*scale){auto glyph=glyphs[size_t(gi(*value))];for(int yy=0;yy<5;++yy)for(int xx=0;xx<3;++xx)if(glyph[yy]&(1<<(2-xx)))for(int j=0;j<scale;++j)for(int i=0;i<scale;++i){int px=x+xx*scale+i,py=y+yy*scale+j;if(px>=244)continue;auto rust=sample(m_signRust,px/256.f,py/80.f);unsigned wear=(rust>>16)&255;
   if(wear<106)sign.pixels[py*256+px]=shade(color,.64f+wear/220.f);
  }}};
 label(18,21,title,3,accent);label(18,51,subtitle,2,0xffbcb393u);
 for(int x:{11,244})for(int y:{11,68})for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)sign.pixels[(y+dy)*256+x+dx]=dy==0?0xff171713u:dy<0?0xff929180u:0xff3b3a31u;
 prepareDecal(sign);return sign;
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
std::uint32_t SoftwareRenderer::sample(const Texture&t,float u,float v,float lod){
 if(t.clampEdges){u=std::clamp(u,0.f,1.f);v=std::clamp(v,0.f,1.f);}else{u-=std::floor(u);v-=std::floor(v);}
 if(t.mips.empty())return t.pixels[size_t(std::min(t.height-1,int(v*t.height))*t.width+std::min(t.width-1,int(u*t.width)))];
 lod=std::clamp(lod,0.f,float(t.mips.size()));int level=int(lod);
 auto fetch=[&](int index){int width=std::max(1,t.width>>index),height=std::max(1,t.height>>index);auto&pixels=index?t.mips[index-1]:t.pixels;return pixels[size_t(std::min(height-1,int(v*height))*width+std::min(width-1,int(u*width)))];};
 auto a=fetch(level);if(size_t(level)==t.mips.size())return a;auto b=fetch(level+1);float blend=lod-level;std::uint32_t result=0;
 for(int channel=0;channel<4;++channel){int shift=channel*8;auto value=unsigned(((a>>shift)&255)*(1-blend)+((b>>shift)&255)*blend);result|=value<<shift;}return result;
}
void SoftwareRenderer::clear(std::uint32_t c){std::fill(m_pixels.begin(),m_pixels.end(),c);}
void SoftwareRenderer::put(int x,int y,std::uint32_t c){if(x>=0&&y>=0&&x<m_width&&y<m_height)m_pixels[size_t(y*m_width+x)]=c;}
void SoftwareRenderer::rect(int x,int y,int w,int h,std::uint32_t c){for(int yy=std::max(0,y);yy<std::min(m_height,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(m_width,x+w);++xx)put(xx,yy,c);}
std::uint32_t SoftwareRenderer::shade(std::uint32_t c,float s)const{int r=int(((c>>16)&255)*s),g=int(((c>>8)&255)*s),b=int((c&255)*s);return rgb(std::clamp(r,0,255),std::clamp(g,0,255),std::clamp(b,0,255));}
void SoftwareRenderer::text(int x,int y,const char* s,std::uint32_t c,int sc){for(;*s;++s,x+=4*sc){auto g=glyphs[size_t(gi(char(std::toupper((unsigned char)*s))))];for(int yy=0;yy<5;++yy)for(int xx=0;xx<3;++xx)if(g[size_t(yy)]&(1<<(2-xx)))rect(x+xx*sc,y+yy*sc,sc,sc,c);}}


void SoftwareRenderer::wornPanel(int x,int y,int width,int height,bool recess,bool materialPanel){
 rect(x+2,y+3,width,height,rgb(5,4,3));
 for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
  unsigned grain=unsigned((xx+x)*7349)^unsigned((yy+y)*19391);grain=(grain^(grain>>7))*1597;
  auto c=materialPanel?sample(recess?m_panelMetal:m_signRust,(xx+x)/256.f,(yy+y)/180.f):sample(m_metal,(xx+x)/91.f,(yy+y)/67.f);
  float light=materialPanel?(recess?1.15f:.85f):(recess?.27f:.54f);light+=(grain%11)*.007f;
  int edge=std::min({xx,yy,width-1-xx,height-1-yy});
  if(edge==0)c=rgb(24,17,12),light=1;
  else if(edge==1)c=yy<height/2?rgb(113,96,71):rgb(27,22,17),light=1;
  else if(edge<4&&grain%9==0)c=rgb(126,81,42),light=.9f;
  put(x+xx,y+yy,shade(c,light));
 }
 if(width>40&&height>20){for(int dx:{5,width-6})for(int dy:{5,height-6}){
  rect(x+dx-2,y+dy-2,5,5,rgb(17,13,9));rect(x+dx-1,y+dy-1,3,3,rgb(96,86,69));
  rect(x+dx-1,y+dy,3,1,rgb(31,25,20));put(x+dx-1,y+dy-1,rgb(159,140,101));
 }}
}
void SoftwareRenderer::drawHud(const Game& game){
 const auto&p=game.player();
 const auto paper=rgb(222,206,164),muted=rgb(159,139,105),amber=rgb(210,145,54),red=rgb(180,55,36);
 const int sector=int(p.pos.y)/8;
 wornPanel(8,8,176,29);rect(17,12,151,12,rgb(24,18,13));
 text(19,14,game.level()==3?(p.z<-4?"10 REACTOR COMPLEX":"09 SURFACE LIFT"):game.level()==2?(p.z>2.5f?"08 UPPER GANTRY":"07 TURBINE HALL"):game.level()==1?(p.pos.y<7?"04 RECEIVING":p.pos.y<17?"05 PUMP HALL":"06 CONTROL"):(sector==0?"01  INTAKE":sector==1?"02  FOUNDRY":"03 CONTAINMENT"),paper,2);
 if(game.level()==3&&game.world().liftPhase()!=World::LiftPhase::Crashed)text(19,32,game.world().liftStatus(),amber);
 char b[80];std::snprintf(b,sizeof(b),"%d CONTACTS REMAIN",game.enemiesRemaining());text(17,27,b,muted);
 // Compact map reveals nearby contacts and a fixed extraction marker.
 const int mx=m_width-57,my=8;
 wornPanel(mx-5,my-5,58,58);rect(mx-1,my-1,50,50,rgb(19,18,10));
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(game.world().solid(x+.5f,y+.5f))rect(mx+x*2,my+y*2,2,2,game.world().tile(x,y)=='#'?rgb(85,78,45):rgb(58,52,35));
 for(auto&e:game.enemies())if(e.alive&&lengthSq(e.pos-p.pos)<36)rect(mx+int(e.pos.x*2),my+int(e.pos.y*2),2,2,red);
 auto exit=game.world().exitPoint();rect(mx+int(exit.x*2),my+int(exit.y*2),2,2,game.enemiesRemaining()==0?rgb(225,183,70):muted);
 rect(mx+int(p.pos.x*2)-1,my+int(p.pos.y*2)-1,3,3,amber);
 rect(mx+int((p.pos.x+std::cos(p.angle)*2)*2),my+int((p.pos.y+std::sin(p.angle)*2)*2),1,1,paper);
 wornPanel(0,m_height-39,m_width,39);
 for(int i=0;i<m_width;i+=9)rect(i,m_height-39,4,2,(i/9)%2?rgb(31,24,15):rgb(157,110,42));
 wornPanel(7,m_height-34,114,30,true);wornPanel(130,m_height-34,104,30,true);wornPanel(240,m_height-34,111,30,true);wornPanel(359,m_height-34,m_width-365,30,true);
 text(12,m_height-29,"HEALTH",muted);std::snprintf(b,sizeof(b),"%03d",int(p.health));text(12,m_height-20,b,p.health<30?red:paper,3);
 for(int i=0;i<10;++i){rect(62+i*5,m_height-16,4,11,rgb(10,7,5));if(p.health>i*10){rect(63+i*5,m_height-15,2,8,rgb(158,57,33));put(63+i*5,m_height-15,rgb(215,115,54));}}
 text(139,m_height-29,game.unarmed()?"UNARMED":"12 GA / TUBE-RESERVE",muted);std::snprintf(b,sizeof(b),"%02d/%02d",p.loaded,std::max(0,p.ammo-p.loaded));text(139,m_height-20,game.unarmed()?(game.guarding()?"GUARD":"FISTS"):b,amber,game.unarmed()?2:3);
 text(247,m_height-29,"PURGE",muted);std::snprintf(b,sizeof(b),"%02d / %02d",game.kills(),int(game.enemies().size()));text(247,m_height-19,b,paper,2);
 text(m_width-103,m_height-27,"RAWMETAL",paper,2);text(m_width-103,m_height-12,"R / RELOAD",muted);
 auto cross=game.hitFlash()>0?red:paper;int cx=m_width/2,cy=m_height/2;
 rect(cx-6,cy,3,1,cross);rect(cx+4,cy,3,1,cross);rect(cx,cy-6,1,3,cross);rect(cx,cy+4,1,3,cross);
 if(auto target=game.targetEnemy()){
  int x=m_width/2-65,y=m_height-60;
  wornPanel(x-4,y-4,138,20,true);text(x,y,target->name(),paper);
  rect(x,y+8,130,5,rgb(10,7,5));rect(x+1,y+9,int(128*std::max(0.f,target->hp)/target->maxHp),3,target->windup>0?amber:red);
  if(target->windup>0)text(cx-22,cy-22,"INCOMING",amber);
 }
 if(game.enemiesRemaining()==0&&(game.level()<2||game.world().controlReleased())){wornPanel(cx-70,42,140,14,true);text(cx-62,47,"TRANSFER INTERLOCK RELEASED",amber);}
 if(game.dead()||game.won()){wornPanel(cx-100,cy-26,200,51);text(cx-68,cy-15,game.won()?"SECTOR CLEARED":"SIGNAL LOST",game.won()?amber:red,2);text(cx-63,cy+7,"ESC / RESTART GAME",paper);}
 if(game.damageFlash()>0){auto tint=rgb(150,37,25);rect(0,0,m_width,2,tint);rect(0,0,2,m_height,tint);rect(m_width-2,0,2,m_height,tint);}
 if(game.audioMuted())text(10,43,"AUDIO MUTED / M",muted);
 else if(!game.musicEnabled())text(10,43,"MUSIC OFF / N",muted);
 if(game.pickupNoticeTime()>0){wornPanel(cx-88,m_height-81,176,16,true);text(cx-79,m_height-76,game.pickupNotice().c_str(),paper);}
 else if(auto pickup=game.nearbyPickup()){
  const char* message=pickup->kind==Pickup::Kind::Ammo?"12 GA / 16 SHELLS":p.health>=100?"FIRST AID / HEALTH FULL":"FIRST AID / 35 HEALTH";
  wornPanel(cx-88,m_height-81,176,16,true);text(cx-79,m_height-76,message,amber);
 }
 if(auto hint=game.interactionHint()){wornPanel(cx-85,m_height-104,170,16,true);text(cx-75,m_height-99,hint,paper);}
 if(game.logTime()>0&&game.activeLog()>=0){auto&log=game.world().terminals()[size_t(game.activeLog())];
  wornPanel(cx-155,48,310,68,false,true);text(cx-141,59,log.title,amber,2);text(cx-141,84,log.line1,paper);text(cx-141,98,log.line2,paper);
 }
}

void SoftwareRenderer::drawSettings(const Game& game){
 for(auto&pixel:m_pixels)pixel=shade(pixel,.25f);
 constexpr int x=MenuLayout::X,y=MenuLayout::Y;
 const auto paper=rgb(222,206,164),amber=rgb(210,145,54),muted=rgb(159,139,105);
 wornPanel(x,y,MenuLayout::Width,MenuLayout::Height,false,true);
 auto page=game.menuPage();bool settingsPage=page==Game::MenuPage::Settings;
 const char* title=settingsPage?"RAWMETAL / PAUSED":page==Game::MenuPage::Save?"SAVE GAME":page==Game::MenuPage::Load?"LOAD GAME":page==Game::MenuPage::Overwrite?"CONFIRM OVERWRITE":page==Game::MenuPage::ConfirmLoad?"CONFIRM LOAD":"CONFIRM RESTART";
 text(x+15,y+12,title,paper,2);text(x+15,y+29,settingsPage?"SETTINGS / SAVED GAMES":"GAMEPLAY IS PAUSED",amber);
 const char* labels[]={"RESUME","MASTER VOLUME","MUSIC VOLUME","EFFECTS VOLUME","MOUSE SENSITIVITY","INVERT MOUSE Y","RESTART CURRENT GAME...","SAVE GAME...","LOAD GAME...","QUIT GAME"};
 auto&settings=game.settings();
 for(int row=0;row<game.menuRows();++row){int top=MenuLayout::RowTop+row*MenuLayout::RowHeight;bool selected=row==game.menuSelection();
  wornPanel(x+12,top,MenuLayout::Width-24,19,true,true);
  if(selected){rect(x+13,top+2,2,15,amber);rect(x+17,top+2,MenuLayout::Width-35,1,rgb(101,72,32));}
  const char* label=settingsPage?labels[row]:(page==Game::MenuPage::Save||page==Game::MenuPage::Load)?(row==3?"BACK":game.slotLabel(row).c_str()):(row==0?"CANCEL":page==Game::MenuPage::Overwrite?"OVERWRITE SAVED GAME":page==Game::MenuPage::ConfirmLoad?"LOAD / REPLACE CURRENT PROGRESS":"RESTART / DISCARD CURRENT PROGRESS");
  text(x+23,top+7,label,selected?paper:muted);
  if(settingsPage&&row>=1&&row<=4){float value=row==1?settings.master:row==2?settings.music:row==3?settings.effects:settings.sensitivity;
   float normalized=row==4?(value-.2f)/2.8f:value;
   rect(MenuLayout::SliderX-3,top+7,MenuLayout::SliderWidth+6,5,rgb(8,7,5));
   rect(MenuLayout::SliderX,top+8,MenuLayout::SliderWidth,1,rgb(116,97,67));
   for(int tick=0;tick<=10;++tick)rect(MenuLayout::SliderX+tick*MenuLayout::SliderWidth/10,top+13,1,tick%5==0?3:2,muted);
   int knob=MenuLayout::SliderX+int(normalized*MenuLayout::SliderWidth);rect(knob-5,top+2,11,16,rgb(13,12,9));rect(knob-4,top+3,9,13,selected?amber:muted);rect(knob-4,top+3,9,2,paper);rect(knob+4,top+5,1,11,rgb(70,51,31));
   for(int grip=-2;grip<=2;grip+=2)rect(knob+grip,top+7,1,5,rgb(53,47,34));
   char valueText[16];if(row==4)std::snprintf(valueText,sizeof(valueText),"%.1fX",value);else std::snprintf(valueText,sizeof(valueText),"%d",int(value*100+.5f));text(MenuLayout::SliderX+83,top+7,valueText,paper);
  }
  if(settingsPage&&row==5)text(MenuLayout::SliderX,top+7,settings.invertMouse?"ON":"OFF",paper);
 }
 if(!settingsPage){text(x+15,y+153,game.menuMessage().c_str(),amber);if(page==Game::MenuPage::Overwrite)text(x+15,y+177,"THE PREVIOUS SLOT WILL BE REPLACED",muted);if(page==Game::MenuPage::ConfirmLoad||page==Game::MenuPage::ConfirmRestart)text(x+15,y+177,"UNSAVED PROGRESS WILL BE LOST",muted);}
 text(x+15,y+MenuLayout::Height-23,"CLICK / ARROWS / ENTER",muted);text(x+15,y+MenuLayout::Height-13,settingsPage?"ESC TO RESUME":"ESC TO GO BACK",amber);
}
void SoftwareRenderer::drawInventory(const Game& game){
 for(auto&pixel:m_pixels)pixel=shade(pixel,.22f);
 const auto paper=rgb(222,206,164),amber=rgb(210,145,54),muted=rgb(159,139,105);
 int x=48,y=28,w=m_width-96,h=m_height-56;wornPanel(x,y,w,h,false,true);
 text(x+16,y+12,"FIELD INVENTORY",paper,2);text(x+w-112,y+15,"I / CLOSE",muted);
 auto section=[&](int sx,int sy,int sw,int sh,const char* title){wornPanel(sx,sy,sw,sh,true,true);text(sx+8,sy+8,title,amber);};
 section(x+14,y+36,260,72,"PRIMARY WEAPON");section(x+14,y+116,260,72,"SECONDARY / MELEE");section(x+14,y+196,260,44,"EQUIPMENT");
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
 text(76,123,game.weaponEquipped()?"SHOTGUN / CLICK TO SELECT":"EMPTY / CLICK TO EQUIP SHOTGUN",paper);
 text(76,173,"FISTS / CLICK TO HOLSTER",paper);text(76,192,"RIGHT CLICK IN WORLD TO GUARD",muted);
 text(76,258,"ARMOR: EMPTY    TOOL: EMPTY",muted);
 for(int row=0;row<5;++row)for(int col=0;col<6;++col)wornPanel(350+col*34,94+row*29,31,26,true);
 int occupied=0;for(int item=0;item<3;++item){if((item==0&&game.weaponEquipped())||(item==1&&game.player().ammo==0)||(item==2&&game.medkits()==0))continue;
  int cell=game.itemCell(item),ix=350+cell%6*34,iy=94+cell/6*29,iw=item==0?133:item==1?31:65;occupied+=item==0?8:item==1?2:4;
  wornPanel(ix,iy,iw,55,true);if(game.selectedItem()==item){rect(ix,iy,iw,1,amber);rect(ix,iy,1,55,amber);rect(ix+iw-1,iy,1,55,amber);rect(ix,iy+54,iw,1,amber);}
  icon(item==0?m_weaponMesh:item==1?m_shellsMesh:m_medkitMesh,item==0?m_weaponTexture:item==1?m_shellsTexture:m_medkitTexture,ix+3,iy+3,iw-6,39,item==0);
  char count[20];std::snprintf(count,sizeof(count),"%d",item==0?1:item==1?game.player().ammo:game.medkits());text(ix+5,iy+45,count,paper);
 }
 char status[40];std::snprintf(status,sizeof(status),"%d / 30 CELLS  HP %d",occupied,int(game.player().health));text(350,250,status,muted);
 const char* names[]={"SHOTGUN / 4 X 2","12 GA SHELLS / 1 X 2","FIRST AID / 2 X 2"};text(350,267,game.selectedItem()<0?"CLICK AN ITEM TO SELECT":names[game.selectedItem()],paper);
 wornPanel(350,280,224,28,true);text(358,291,game.selectedItem()==0?"E / ENTER: EQUIP OR STOW":game.selectedItem()==2?"E / ENTER: HEAL 35 HP":"AMMO IS USED BY THE SHOTGUN",amber);
 text(64,301,"SELECT ITEM THEN EMPTY CELL TO MOVE",muted);text(64,316,"I / ESC CLOSE   CLICK PRIMARY TO EQUIP",amber);
}
void SoftwareRenderer::drawConsole(const Game& game){
 rect(0,0,m_width,152,rgb(10,14,16));rect(0,150,m_width,2,rgb(202,150,67));
 text(12,9,"RAWMETAL / DEVELOPER CONSOLE",rgb(218,172,89),2);
 auto&log=game.consoleLog();size_t first=log.size()>9?log.size()-9:0;
 int y=28;for(size_t i=first;i<log.size();++i,y+=11)text(12,y,log[i].substr(0,150).c_str(),rgb(188,204,196));
 text(12,132,("> "+game.consoleLine()+"_").c_str(),rgb(245,212,142));
}
void SoftwareRenderer::render(const Game& game){auto start=std::chrono::steady_clock::now();
 int fullWidth=m_width,fullHeight=m_height;bool scaled=game.renderScale()<1;
 if(scaled){m_width=int(fullWidth*game.renderScale());m_height=int(fullHeight*game.renderScale());m_scenePixels.resize(size_t(m_width*m_height));m_sceneZ.resize(size_t(m_width*m_height));m_pixels.swap(m_scenePixels);m_zbuffer.swap(m_sceneZ);}
 auto scene=[&]{bool parallel=m_gpuFrame&&m_animationWorker&&!game.holdingClutter();m_poseReady=false;
  if(parallel)m_animationWorker->start([&]{prepareViewModel(game);});
  try{clear(rgb(12,16,18));drawScene(game);for(int level=0;level<Game::ChunkCount;++level)if(level!=game.level()&&game.chunkResident(level)){auto neighbor=game.chunkView(level);drawScene(neighbor,false);}
   if(parallel){m_animationWorker->wait();m_poseReady=true;}drawViewModel(game);m_poseReady=false;
  }catch(...){if(parallel)m_animationWorker->wait();m_poseReady=false;throw;}
 };
 if(m_gpu){try{m_gpu->begin(m_width,m_height);m_gpuFrame=true;auto a=std::chrono::steady_clock::now();scene();auto b=std::chrono::steady_clock::now();m_gpu->finish(m_pixels);auto c=std::chrono::steady_clock::now();m_sceneMs=std::chrono::duration<double,std::milli>(b-a).count();m_submitMs=std::chrono::duration<double,std::milli>(c-b).count();m_gpuFrame=false;}
  catch(const std::exception&e){m_gpuFrame=false;m_gpu.reset();m_gpuName="Software fallback: "+std::string(e.what());std::ofstream("RawMetal-renderer.txt")<<m_gpuName<<'\n';scene();}}
 else scene();
 if(scaled){int sceneWidth=m_width,sceneHeight=m_height;m_width=fullWidth;m_height=fullHeight;m_pixels.swap(m_scenePixels);m_zbuffer.swap(m_sceneZ);
  for(int y=0;y<m_height;++y)for(int x=0;x<m_width;++x)m_pixels[size_t(y*m_width+x)]=m_scenePixels[size_t((y*sceneHeight/m_height)*sceneWidth+x*sceneWidth/m_width)];}
 drawHud(game);if(game.consoleOpen())drawConsole(game);else if(game.paused())drawSettings(game);else if(game.inventoryOpen())drawInventory(game);
 float ms=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();m_frameMs=m_frameMs==0?ms:m_frameMs*.9f+ms*.1f;
 if(game.showFps()){char info[96];std::snprintf(info,sizeof(info),"%s %dX%d RENDER %.1F MS / %.0F FPS",m_gpu?"VULKAN":"CPU",int(fullWidth*game.renderScale()),int(fullHeight*game.renderScale()),m_frameMs,1000.f/std::max(.01f,m_frameMs));text(12,m_height-50,info,rgb(225,200,130));}
}
}








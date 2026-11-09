#include "EditorShots.h"
#ifdef _WIN32
#include "../world/CustomCampaign.h"
#include "../game/Game.h"
#include "../renderer/SoftwareRenderer.h"
#include "../renderer/Projection.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <memory>

namespace retro {
int testEditorLiveRendering(){
 std::ofstream report("native-editor-live-render-test.txt");
 try{
  auto text=[](std::string record){std::string out="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Live render regression|0\nMAP|0|Room|0|0|3.5|8.5|0|0|4.5|0.7|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
   for(int y=0;y<24;++y)out+="ROW|0|0|"+std::to_string(y)+"|"+(y==0||y==23?std::string(24,'#'):"#"+std::string(22,'.')+"#")+'\n';
   return out+"LIGHT|0|6|8|4|0|0|2|15|4000\n"+record+"\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";};
  auto campaign=[&](const std::string& record){return loadCustomCampaignText(text(record),"live-render.rmc");};
  auto renderer=std::make_unique<SoftwareRenderer>(DisplayWidth,DisplayHeight);renderer->setEnvironmentInspection(true);
  if(!renderer->enableHardware(nullptr))throw std::runtime_error("Offscreen native renderer initialization failed");
  auto initial=campaign("FIXTURE|0|6|7|8.5|0|2|1|1.5|0|1");auto owned=std::make_unique<Game>(initial);auto& scene=*owned;scene.update({},1.f/60);
  auto camera=scene.player();camera.angle=0;camera.pitch=-12;scene.restoreEditorCamera(camera);
  auto draw=[&]{for(int i=0;i<8;++i)renderer->render(scene);
    return std::vector<std::uint32_t>(renderer->pixels(),renderer->pixels()+DisplayWidth*DisplayHeight);};
  auto difference=[](const auto& a,const auto& b){size_t changed=0;for(size_t i=0;i<a.size();++i){
    auto p=a[i],q=b[i];int d=std::abs(int((p>>16)&255)-int((q>>16)&255))+std::abs(int((p>>8)&255)-int((q>>8)&255))+std::abs(int(p&255)-int(q&255));if(d>18)++changed;}return changed;};
  auto before=draw();auto oldBuilds=renderer->staticGeometryBuilds();auto revision=scene.sessionRevision();
  scene.adoptEditorCampaign(campaign("FIXTURE|0|6|7|11.5|0|2|1|1.5|0|1"));auto moved=draw();
  if(scene.sessionRevision()==revision||renderer->staticGeometryBuilds()<=oldBuilds||difference(before,moved)<100)
    throw std::runtime_error("Moving a native fixture left cached geometry/pixels unchanged");
  report<<"PASS: fixture movement rebuilds cached geometry and changes "<<difference(before,moved)<<" rendered pixels\n";
  scene.adoptEditorCampaign(campaign(""));auto removed=draw();if(difference(moved,removed)<100)throw std::runtime_error("Deleted fixture remains in the native picture");
  report<<"PASS: fixture deletion changes the same live renderer's picture\n";
  scene.adoptEditorCampaign(campaign("CLUTTER|0|7|7|8.5|0|0|6|2|1|1.5|0|0|"));before=draw();
  scene.adoptEditorCampaign(campaign("CLUTTER|0|7|7|11.5|0|0|6|2|1|1.5|0|0|"));moved=draw();
  if(scene.clutter().empty()||std::abs(scene.clutter()[0].pos.y-11.5f)>.001f||difference(before,moved)<100)
    throw std::runtime_error("Loose prop movement did not reach the live native picture");
  report<<"PASS: loose prop movement changes native physics pose and "<<difference(before,moved)<<" rendered pixels\n";
  EditorFloorView view{-1000,1000,1};view.mask.fill(0xffffffu);renderer->setEditorView({view},{});auto opaque=draw();auto builds=renderer->staticGeometryBuilds();
  view.opacity=.5f;renderer->setEditorView({view},{});auto ghost=draw();
  if(difference(opaque,ghost)<500)throw std::runtime_error("Layer opacity has no effect in the native 3D renderer");
  view.opacity=1;view.mask.fill(0);renderer->setEditorView({view},{});auto masked=draw();
  if(difference(opaque,masked)<500)throw std::runtime_error("Layer masks do not hide native 3D geometry");
  view.mask.fill(0xffffffu);renderer->setEditorView({view},{});before=draw();
  renderer->setEditorView({view},{{7,8,0,7,9,0,4},{7,8,0,7,8,2,1},{7,8,2,7,9,2,1}});auto guides=draw();
  if(difference(before,guides)<30)throw std::runtime_error("Selection and hover guides are absent from the native picture");
  if(renderer->staticGeometryBuilds()!=builds)throw std::runtime_error("Layer preview controls rebuild geometry instead of updating the live view");
  report<<"PASS: native GPU opacity, masks and 3D selection/hover overlays change pixels without rebuilding geometry\n";
  renderer->setEditorView({},{});return 0;
 }catch(const std::exception& error){report<<"FAIL: "<<error.what()<<'\n';return 1;}
}
// One engine process renders the whole list offscreen; the editor caches the
// results, so the tiles appear once and are instant afterwards.
int runEditorShots(const std::filesystem::path& list){
 constexpr int width=DisplayWidth,height=DisplayHeight;
 auto renderer=std::make_unique<SoftwareRenderer>(width,height);
 renderer->setEnvironmentInspection(true);
 if(!renderer->enableHardware(nullptr))return 36;
 std::ifstream in(list);std::string line;int made=0,failed=0;
 std::unique_ptr<Game> scene;
 while(std::getline(in,line)){
  std::vector<std::string> f;std::stringstream split(line);std::string part;
  while(std::getline(split,part,'|'))f.push_back(part);
  if(f.size()<7)continue;
  // An optional eighth field asks for a bigger square (review pictures); tiles are 96.
  const int tile=f.size()>7?std::clamp(std::stoi(f[7]),32,std::min(width,height)):96;
  try{
   const std::filesystem::path pack=std::filesystem::u8path(f[0]),out=std::filesystem::u8path(f[1]);
   auto loaded=loadCustomCampaignFile(pack);
   auto campaign=std::make_shared<CustomCampaign>(*loaded);campaign->startMap=0;
   // One session, each picture adopted into it the way the editor's own live
   // preview rebuilds a room: the renderer's caches follow the change.
   if(scene)scene->adoptEditorCampaign(campaign);
   else{scene=std::make_unique<Game>(campaign);scene->update({},1.f/60);}
   auto at=scene->player();
   at.pos.x=std::stof(f[2]);at.pos.y=std::stof(f[3]);at.crouched=at.prone=false;at.eye=Player::StandingEye;
   at.z=std::stof(f[4])-at.eye;at.angle=std::stof(f[5])*3.14159265f/180.f;at.pitch=std::stof(f[6]);
   scene->restoreEditorCamera(at);
   for(int i=0;i<8;++i)renderer->render(*scene);
   // The middle square, box-filtered down to one tile.
   const int side=std::min(width,height),x0=(width-side)/2,y0=(height-side)/2;
   std::vector<unsigned char> rgb(size_t(tile)*tile*3);
   for(int ty=0;ty<tile;++ty)for(int tx=0;tx<tile;++tx){
    const int sx0=x0+tx*side/tile,sx1=x0+(tx+1)*side/tile,sy0=y0+ty*side/tile,sy1=y0+(ty+1)*side/tile;
    unsigned r=0,g=0,b=0,n=0;
    for(int y=sy0;y<std::max(sy1,sy0+1);++y)for(int x=sx0;x<std::max(sx1,sx0+1);++x){auto p=renderer->pixels()[size_t(y)*width+x];r+=(p>>16)&255;g+=(p>>8)&255;b+=p&255;++n;}
    auto* o=&rgb[(size_t(ty)*tile+tx)*3];o[0]=char(r/n);o[1]=char(g/n);o[2]=char(b/n);
   }
   auto temporary=out;temporary+=".tmp";
   {std::ofstream file(temporary,std::ios::binary);file<<"P6\n"<<tile<<' '<<tile<<"\n255\n";file.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));}
   std::error_code ec;std::filesystem::rename(temporary,out,ec);
   ++made;
  }catch(...){++failed;}
 }
 std::ofstream(list.wstring()+L".done")<<made<<' '<<failed<<'\n';
 return 0;
}
}
#endif

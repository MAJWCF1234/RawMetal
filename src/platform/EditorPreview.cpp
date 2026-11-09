#include "EditorPreview.h"
#ifdef _WIN32
#include "Win32Window.h"
#include "../world/CustomCampaign.h"
#include "../renderer/SoftwareRenderer.h"
#include "../audio/AudioEngine.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>

namespace retro {
int validateEditorCampaign(const std::filesystem::path& path){
 auto campaign=loadCustomCampaignFile(path);
 for(size_t i=0;i<campaign->maps.size();++i){World validated(int(i),campaign->maps[i]);}
 std::cout<<"OK "<<campaign->maps.size()<<'\n';return 0;
}
int runEditorPreview(const EditorPreviewOptions& options){
 if(options.pack.empty()||options.control.empty()||options.status.empty())throw std::runtime_error("Editor preview requires pack, control and status files");
 constexpr int width=DisplayWidth,height=DisplayHeight;
 Win32Window window(width,height,L"Raw Metal engine viewport",options.parent,!options.hidden);
 if(!window.valid())throw std::runtime_error("Cannot create engine preview child window");
 auto renderer=std::make_unique<SoftwareRenderer>(width,height);
 renderer->setEnvironmentInspection(true);
 // Test captures use the same offscreen Vulkan path as native map shots.
 if(!renderer->enableHardware(options.capture.empty()&&!options.hidden?window.handle():nullptr))throw std::runtime_error("Cannot initialize engine preview renderer");
 // Frozen authoring and hidden checks do not need a sound device or decode
 // the full soundtrack. Initialize native audio only when explicitly requested.
 std::unique_ptr<AudioEngine> audio;
 std::unique_ptr<Game> scene;
 long long revision=-1;int selected=-1,attemptedMap=-1;bool playing=false,valid=false;std::string loadError;unsigned long long frame=0;
 auto report=[&](const std::string& state,const std::string& detail=""){
  auto temporary=options.status;temporary+=L".tmp";
  {std::ofstream out(temporary);out<<state<<' '<<revision<<' '<<selected<<' '<<int(playing)<<' '<<frame<<'\n'<<detail;}
  MoveFileExW(temporary.c_str(),options.status.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
 };
 auto last=std::chrono::steady_clock::now(),lastReport=last;
 int lastWidth=-1,lastHeight=-1;
 while(window.pump()){
  if(options.parent){if(!IsWindow(options.parent))break;RECT rc{};GetClientRect(options.parent,&rc);if(rc.right!=lastWidth||rc.bottom!=lastHeight){lastWidth=rc.right;lastHeight=rc.bottom;SetWindowPos(window.handle(),nullptr,0,0,std::max(1L,rc.right),std::max(1L,rc.bottom),SWP_NOZORDER|SWP_NOACTIVATE);}}
  long long requested=revision;int map=std::max(0,selected),play=playing?1:0,stop=0,audition=0;
  {std::ifstream control(options.control);if(control){control>>requested>>map>>play>>stop;control>>audition;}}
  if(stop)break;
  if(requested!=revision||map!=attemptedMap){
   attemptedMap=map;
   // Keep the last valid scene on an invalid edit; expose the actual parser error.
   try{
    auto loaded=loadCustomCampaignFile(options.pack);
    if(map<0||map>=int(loaded->maps.size()))throw std::runtime_error("Preview map is outside this campaign");
    auto campaign=std::make_shared<CustomCampaign>(*loaded);campaign->startMap=map;
    auto next=std::make_unique<Game>(campaign);
    if(scene&&selected==map)next->restoreEditorCamera(scene->player());
    wchar_t local[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    if(n>0&&n<32768)next->loadSettings(std::wstring(local)+L"\\RawMetal\\settings.ini");
    next->update({},1.f/60);
    scene=std::move(next);selected=map;revision=requested;valid=true;loadError.clear();report("READY");
   }catch(const std::exception& error){revision=requested;valid=false;loadError=error.what();report("ERROR",loadError);}
  }
  auto now=std::chrono::steady_clock::now();float dt=std::min(.05f,std::chrono::duration<float>(now-last).count());last=now;
  playing=play!=0&&valid;
  if(scene){
   if(playing){bool menu=scene->paused()||scene->inventoryOpen()||scene->talking()||scene->consoleOpen();auto input=window.input(menu);if(input.escape){playing=false;input={};window.setMenu(true);{std::ofstream control(options.control);control<<revision<<' '<<selected<<" 0 0 "<<audition<<'\n';}if(options.parent)PostMessageW(GetAncestor(options.parent,GA_ROOT),WM_APP+31,0,0);}else scene->update(input,dt);}
   else window.setMenu(true);
   const bool hostFocused=options.parent&&GetForegroundWindow()==GetAncestor(options.parent,GA_ROOT);
   const bool audible=!options.hidden&&((playing&&window.focused())||(audition&&hostFocused));
   if(audible&&!audio)audio=std::make_unique<AudioEngine>();
   if(audio)audio->update(*scene,audible);
   renderer->render(*scene);if(!renderer->hardwarePresentsWindow())window.present(renderer->pixels(),width,height);
   ++frame;
   if(now-lastReport>std::chrono::milliseconds(250)){report(valid?"READY":"ERROR",loadError);lastReport=now;}
   if(options.frames>0&&frame>=unsigned(options.frames))break;
  }
  Sleep(playing?1:16);
 }
 window.setMenu(true);
 if(!options.capture.empty()&&scene){std::ofstream out(options.capture,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
 report("STOPPED");return scene?0:8;
}
}
#endif

namespace retro {
int testLayerEditingExport(const std::filesystem::path& file){
 std::ofstream report(file.wstring()+L".layer-test.txt");
 try{
  auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
  auto loaded=loadCustomCampaignFile(file);require(loaded->maps.size()==1,"Layer regression export must contain one map");
  const auto& authored=*loaded->maps[0];World world(0,loaded->maps[0]);
  require(authored.layers.size()>=2,"The exported upper floor is missing");
  float deck=authored.layers[1].elevation;
  require(std::abs(world.supportBelow(12.5f,8.5f,deck+.05f)-deck)<.03f,"The saved catwalk has no native collision support");
  require(world.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The exported catwalk still lacks standing clearance");
  require(!world.fits(9.5f,8.5f,deck,Player::StandingHeight,false),"The exported upper wall has no collision");
  auto low=std::make_shared<AuthoredMapData>(authored);low->definition.ceiling=deck+1.f;World cramped(0,low);
  require(!cramped.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The regression did not reproduce the original low roof");
  require(std::any_of(world.structures().begin(),world.structures().end(),[](const auto& s){return s.layerSurface==1;}),"The game did not construct native upper-floor walls");
  report<<"PASS: editor export loads directly; catwalk support, standing clearance, low-roof reproduction and upper-wall collision\n";
  auto walker=std::make_unique<Game>(loaded);auto start=walker->player();start.angle=0;walker->restoreEditorCamera(start);
  require(std::abs(start.z-deck)<.03f,"The saved player start is not on the catwalk");
  InputState walk{};walk.forward=true;for(int frame=0;frame<60;++frame)walker->update(walk,1.f/60.f);
  require(walker->player().pos.x>start.pos.x+1.f&&std::abs(walker->player().z-deck)<.08f&&!walker->player().crouched,"The player could not walk upright along the exported catwalk");
  report<<"PASS: actual player simulation walks upright on the exported catwalk without falling or crouching\n";
  auto roofPack=loadCustomCampaignFile(file.parent_path()/L"roof-edit.rmc");World roofWorld(0,roofPack->maps[0]);
  require(roofWorld.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The editor-raised explicit roof still blocks the standing player");
  auto lowRoof=std::make_shared<AuthoredMapData>(*roofPack->maps[0]);require(!lowRoof->structures.empty(),"The exported explicit roof is missing");
  auto& slab=lowRoof->structures.back();require(std::abs(slab.top-slab.bottom-.25f)<.001f,"Export changed explicit roof thickness");slab.bottom=4.5f;slab.top=4.75f;World lowRoofWorld(0,lowRoof);
  require(!lowRoofWorld.fits(12.5f,8.5f,deck,Player::StandingHeight,false),"The explicit-roof check did not reproduce blocked headroom");
  report<<"PASS: separately exported thin-roof repair preserves slab thickness and restores real standing clearance\n";
  auto original=std::make_shared<CustomCampaign>(*loaded),taller=std::make_shared<CustomCampaign>(*loaded);
  std::ifstream input(file);std::string line,edited;while(std::getline(input,line)){if(line.starts_with("MAP|0|")){std::vector<std::string> fields;std::istringstream split(line);std::string field;while(std::getline(split,field,'|'))fields.push_back(field);fields[9]=std::to_string(authored.definition.ceiling+2.f);line.clear();for(const auto& value:fields){if(!line.empty())line+='|';line+=value;}}edited+=line+'\n';}
  taller=std::make_shared<CustomCampaign>(*loadCustomCampaignText(edited,"taller-export.rmc"));
  auto before=std::make_unique<Game>(original),after=std::make_unique<Game>(taller);
  before->update({},1.f/60);after->update({},1.f/60);
  auto camera=before->player();camera.pos={3.5f,8.5f};camera.z=deck;camera.angle=0;camera.pitch=0;camera.eye=Player::StandingEye;
  before->restoreEditorCamera(camera);after->restoreEditorCamera(camera);
  report<<"Native roof heights: "<<before->world().definition().ceiling<<" / "<<after->world().definition().ceiling<<"; camera z "<<camera.z<<" eye "<<camera.eye<<"\n";
  auto renderer=std::make_unique<SoftwareRenderer>(DisplayWidth,DisplayHeight);renderer->setEnvironmentInspection(true);
  require(renderer->enableHardware(nullptr),"Native UV verification requires the offscreen Vulkan renderer");
  int previousDetail=SoftwareRenderer::detailLayer();SoftwareRenderer::setDetailLayer(4);
  auto draw=[&](const Game& scene){for(int i=0;i<6;++i)renderer->render(scene);return std::vector<std::uint32_t>(renderer->pixels(),renderer->pixels()+DisplayWidth*DisplayHeight);};
  auto a=draw(*before),b=draw(*after);SoftwareRenderer::setDetailLayer(previousDetail);
  int total=0,matching=0,changed=0;
  auto difference=[](std::uint32_t p,std::uint32_t q){return std::abs(int((p>>16)&255)-int((q>>16)&255))+std::abs(int((p>>8)&255)-int((q>>8)&255))+std::abs(int(p&255)-int(q&255));};
  for(size_t i=0;i<a.size();++i)changed+=difference(a[i],b[i])>18;
  for(int y=DisplayHeight/2+10;y<DisplayHeight-35;++y)for(int x=DisplayWidth/2-24;x<DisplayWidth/2+24;++x){++total;matching+=difference(a[size_t(y)*DisplayWidth+x],b[size_t(y)*DisplayWidth+x])<=12;}
  report<<"UV comparison: "<<matching<<'/'<<total<<" lower-wall pixels match, "<<changed<<" geometry pixels differ\n";
  // UV debugging may give the roof and wall the same periodic values. Check
  // the normal material render independently to prove the height changed.
  SoftwareRenderer::setDetailLayer(0);auto materialA=draw(*before),materialB=draw(*after);SoftwareRenderer::setDetailLayer(previousDetail);
  int materialChanged=0;for(size_t i=0;i<materialA.size();++i)materialChanged+=difference(materialA[i],materialB[i])>18;
  report<<"Material geometry pixels changed: "<<materialChanged<<"\n";
  auto savePicture=[&](const auto& pixels,const wchar_t* suffix){std::ofstream out(file.wstring()+suffix,std::ios::binary);out<<"P6\n"<<DisplayWidth<<' '<<DisplayHeight<<"\n255\n";for(auto pixel:pixels){char rgb[]={char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}};savePicture(materialA,L".before.ppm");savePicture(materialB,L".after.ppm");
  require(materialChanged>100,"Height edit left the native rendered geometry unchanged");
  require(total>100&&matching>total*.94f,"Raising a wall stretched its lower material UV scale");
  report<<"PASS: native rendered UV density stays stable under wall-height edits ("<<matching<<'/'<<total<<" lower-wall pixels match; "<<changed<<" geometry pixels change)\n";
  return 0;
 }catch(const std::exception& error){report<<"FAIL: "<<error.what()<<'\n';return 1;}
}
}
namespace retro {
int testCampaignEditingExport(const std::filesystem::path& file){
 std::ofstream report(file.wstring()+L".campaign-test.txt");
 try{auto require=[](bool okay,const char* message){if(!okay)throw std::runtime_error(message);};auto loaded=loadCustomCampaignFile(file);
  require(loaded->maps.size()==6,"Campaign regression needs the four joined edges and one distant room");
  for(int edge=0;edge<4;++edge){auto game=std::make_unique<Game>(loaded);auto at=game->player();
   at.pos={edge==2?.8f:edge==3?23.2f:12.5f,edge==0?.8f:edge==1?23.2f:12.5f};at.angle=edge==0?-1.5707963f:edge==1?1.5707963f:edge==2?3.1415926f:0;at.z=0;game->restoreEditorCamera(at);
   InputState walk{};walk.forward=true;for(int i=0;i<60;++i)game->update(walk,1.f/60.f);
   require(game->level()==edge+1,"The player could not cross a joined map edge in the game");}
  report<<"PASS: actual player simulation crosses all four saved campaign joins\n";
  auto game=std::make_unique<Game>(loaded);auto renderer=std::make_unique<SoftwareRenderer>(DisplayWidth,DisplayHeight);renderer->setEnvironmentInspection(true);require(renderer->enableHardware(nullptr),"Offscreen native renderer unavailable");
  for(int i=0;i<3;++i)renderer->render(*game);auto chunks=renderer->renderedChunks();
  require(std::find(chunks.begin(),chunks.end(),0)!=chunks.end(),"Active map was culled");
  for(int edge=1;edge<=4;++edge)require(std::find(chunks.begin(),chunks.end(),edge)!=chunks.end(),"A nearby joined map was culled");
  require(std::find(chunks.begin(),chunks.end(),5)==chunks.end(),"A distant map was still submitted to the renderer");
  report<<"PASS: native rendering draws active and adjoining maps while excluding the distant map\n";
  auto garage=loadCustomCampaignFile(file.parent_path()/L"parking-garage.rmc");World garageWorld(0,garage->maps[0]);
  require(garageWorld.supportBelow(6.5f,12.f,3.51f)<3.f,"The painted garage ramp opening still contains a hidden deck");
  bool concrete=false,yellow=false;for(const auto& p:garage->maps[0]->structures){concrete|=p.material==15;yellow|=p.material==18;}require(concrete&&yellow,"Concrete or the local yellow stripe disappeared from the exported garage");
  for(float startZ:{0.f,3.5f}){auto driver=std::make_unique<Game>(garage);auto at=driver->player();at.pos={startZ<1?6.5f:10.5f,4.4f};at.z=startZ;at.angle=1.5707963f;driver->restoreEditorCamera(at);InputState walk{};walk.forward=true;
   for(int i=0;i<420;++i)driver->update(walk,1.f/60.f);report<<"Garage ramp from "<<startZ<<": player "<<driver->player().pos.x<<','<<driver->player().pos.y<<','<<driver->player().z<<'\n';require(driver->player().pos.y>19.3f&&driver->player().z>startZ+3.3f,"The player cannot climb the garage ramp and cross its deck landing");}
  report<<"PASS: concrete and yellow stripe remain native; erased ramp gaps stay open; player walks both garage ramps onto their landings\n";return 0;
 }catch(const std::exception& error){report<<"FAIL: "<<error.what()<<'\n';return 1;}
}
}

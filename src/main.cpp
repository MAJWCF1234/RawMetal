#include "renderer/RenderProfile.h"
#include "core/FrameProfile.h"
#include "core/NeuralTexture.h"
#ifdef _WIN32
#include "game/Game.h"
#include "game/ai/DecisionClock.h"
#include "game/ai/WalkQueryCache.h"
#include "game/ai/StateLookupHints.h"
#include "world/CollisionQueryBatch.h"
#include "world/TerrainSurfaceCache.h"
#include "renderer/MinimapCache.h"
#include "world/CustomCampaign.h"
#include "vr/VrRuntime.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <iostream>
#include <cstdlib>
#include <map>
#include "world/CampaignMaps.h"
#pragma comment(lib,"shell32.lib")
#include "renderer/SoftwareRenderer.h"
#include "platform/Win32Window.h"
#include "audio/AudioEngine.h"
#include <chrono>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <queue>
#include <exception>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

// Own argv strings for the lifetime of every dispatch, including exception handling.
struct CommandArguments {
 std::vector<std::wstring> values;
 explicit CommandArguments(const wchar_t* line=GetCommandLineW()){int count=0;auto argv=CommandLineToArgvW(line,&count);if(!argv)throw std::runtime_error("Cannot parse command line");for(int i=1;i<count;++i)values.emplace_back(argv[i]);LocalFree(argv);}
 const wchar_t* find(std::wstring_view flag)const{
  for(const auto& value:values)if(value==flag||(flag.back()==L'='?value.starts_with(flag):(value.starts_with(flag)&&value.size()>flag.size()&&value[flag.size()]==L'=')))return value.c_str();
  return nullptr;
 }
 bool automated()const{for(const auto& value:values){auto flag=value.substr(0,value.find(L'='));for(auto marker:{L"test",L"shot",L"audit",L"inspection",L"video",L"dump",L"performance",L"benchmark",L"profile",L"--map-",L"--residency"})if(flag.find(marker)!=std::wstring::npos)return true;}return false;}
};
static const CommandArguments& commandArguments(){static const CommandArguments args;return args;}
static void applyRenderSwitches(bool withoutPost=false,bool withoutPixel=false){
 const auto& args=commandArguments();
 retro::SoftwareRenderer::setPostProcessing(!args.find(L"--nopost")&&(args.find(L"--post-ao")||!withoutPost),args.find(L"--post-ao")!=nullptr);
 retro::SoftwareRenderer::setPixelLighting(!withoutPixel&&!args.find(L"--nopixel"));
}
__declspec(noinline) static int neuralTextureRuntimeTest(int W,int H,bool vulkan){
 std::ofstream report("neural-texture-runtime-test.txt");
 auto before=retro::neuralTextureReconstructionCount();auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);
 if(vulkan&&!renderer->enableHardware())return 36;
 auto scene=retro::Game::exclusionInspection(2);renderer->render(scene);
 auto loaded=retro::neuralTextureReconstructionCount();
 for(int frame=0;frame<240;++frame){retro::InputState input{};input.mouseDx=3;scene.update(input,1.f/60);renderer->render(scene);}
 auto afterFrames=retro::neuralTextureReconstructionCount();int scale=retro::SoftwareRenderer::texturePresentScale();
 retro::SoftwareRenderer::setTexturePresentScale(scale==1?2:1);renderer->render(scene);retro::SoftwareRenderer::setTexturePresentScale(scale);renderer->render(scene);
 auto afterAtlasChanges=retro::neuralTextureReconstructionCount();
 report<<"Startup neural reconstructions: "<<loaded-before<<"\nAdditional reconstructions across 240 frames: "<<afterFrames-loaded<<"\nAdditional reconstructions across atlas scale changes: "<<afterAtlasChanges-afterFrames<<"\n";
 bool passed=loaded>before&&afterFrames==loaded&&afterAtlasChanges==afterFrames;report<<(passed?"PASS":"FAIL")<<'\n';return passed?0:75;
}
__declspec(noinline) static int vrSmokeTest(int W,int H,bool inspect=false){
     std::ofstream report("vr-smoke-test.txt");retro::VrRuntime runtime;std::string error;if(!runtime.initialize(error)){report<<"UNAVAILABLE: "<<error;return 61;}
     retro::Win32Window mirror(W,H,L"Depthworks VR spectator test");auto scene=retro::Game::mapInspection({11.3f,3.5f},1.570796f);auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware(mirror.handle())){report<<renderer->hardwareName();return 36;}
     auto start=std::chrono::steady_clock::now();int submitted=0;if(inspect){scene.equipWeapon(retro::WeaponId::Shotgun);retro::perf::enabled=true;retro::perf::costs={};}int limit=inspect?1350:12;double waitMs=0,updateMs=0,leftMs=0,rightMs=0;std::string poses;
     auto lap=[](std::chrono::steady_clock::time_point& t){auto now=std::chrono::steady_clock::now();double ms=std::chrono::duration<double,std::milli>(now-t).count();t=now;return ms;};
     while(submitted<limit&&std::chrono::steady_clock::now()-start<std::chrono::seconds(inspect?30:15)){mirror.pump();retro::InputState input{};auto t=std::chrono::steady_clock::now();if(!runtime.update(scene,input,1.f/90))break;if(!runtime.trackingValid())continue;waitMs+=lap(t);scene.update(input,1.f/90);updateMs+=lap(t);renderer->renderVrEye(scene,0);leftMs+=lap(t);renderer->renderVrEye(scene,1);rightMs+=lap(t);++submitted;if(inspect&&submitted%225==1)poses+="frame "+std::to_string(submitted)+"\n"+runtime.poseReport();if(inspect&&submitted==1){renderer->saveVrEye("vr-eye.ppm");std::ofstream("vr-inspection-ready.txt")<<"ready";}if(inspect&&submitted%225==0)renderer->saveVrEye(("vr-eye-"+std::to_string(submitted/225)+".ppm").c_str());}
     auto extent=runtime.extent();report<<"Desktop mirror frames: "<<renderer->vrMirrorFrames()<<"\n";report<<renderer->hardwareName()<<"\nEye resolution: "<<extent.first<<" x "<<extent.second<<"\nStereo frames submitted: "<<submitted<<"\nMenu panel: 2 metres away / 1.4 metres wide\n";for(int hand=0;hand<2;++hand)report<<"Hand "<<hand<<": tracked="<<runtime.hands()[hand].tracked<<", articulated="<<runtime.hands()[hand].articulated<<"\n";if(submitted){report<<"Per stereo frame (ms): poses/wait "<<waitMs/submitted<<", game "<<updateMs/submitted<<", left eye "<<leftMs/submitted<<", right eye "<<rightMs/submitted<<"\n";for(int i=0;i<retro::perf::Count;++i)if(retro::perf::costs[size_t(i)]>0)report<<"  "<<retro::perf::names[i]<<" "<<retro::perf::costs[size_t(i)]/submitted<<"\n";report<<poses;}report.flush();return submitted==limit&&renderer->vrMirrorFrames()>0?0:62;

}
__declspec(noinline) static int cableLayoutInspection(int width,int height){
 auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(width,height);auto& renderer=*rendererStorage;if(!renderer.enableHardware())return 36;
 struct View{const char*name;retro::Vec2 p;float z,yaw,pitch;};
 constexpr std::array<View,6> views{{
  {"cable-entry",{5.5f,5.5f},-9,.65f,-5},
  {"cable-trench",{13,6.8f},-9,retro::kPi*.5f,-8},
  {"cable-breaker",{17.8f,11},-9,0,-4},
  {"cable-south-route",{13,16.5f},-9,-retro::kPi*.5f,-5},
  {"cable-terminal-remote",{18.6f,11.9f},-9,0,-3},
  // Inspect the maintenance terminal head-on from the open west aisle.
  {"cable-terminal-maintenance",{6.1f,6.4f},-9,0,-4}
 }};
 auto capture=[&](const std::string&name,const retro::Game&scene){renderer.render(scene);
  std::ofstream frame(name+".ppm",std::ios::binary);frame<<"P6\n"<<width<<' '<<height<<"\n255\n";
  for(int i=0;i<width*height;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}
 };
 for(const auto&view:views){
  capture(view.name,retro::Game::mapInspection(view.p,view.yaw,view.pitch,6,false,view.z,true));
  capture(std::string(view.name)+"-gameplay",retro::Game::mapInspection(view.p,view.yaw,view.pitch,6,false,view.z,false));
 }
 return 0;
}
__declspec(noinline) static int gpuActorPerformance(){
 using Clock=std::chrono::steady_clock;
 retro::Win32Window window(retro::DisplayWidth,retro::DisplayHeight,L"Depthworks actor comparison");if(!window.valid())return 1;
 ShowWindow(static_cast<HWND>(window.handle()),SW_HIDE);
 auto renderer=std::make_unique<retro::SoftwareRenderer>(retro::DisplayWidth,retro::DisplayHeight);if(!renderer->enableHardware(window.handle()))return 36;
 auto game=retro::Game::exclusionInspection(12);retro::AudioEngine audio(true);retro::perf::enabled=false;
 std::ofstream report("gpu-actor-performance.txt"),csv("gpu-actor-performance.csv");
 report<<renderer->hardwareName()<<" / paired native renders of the same simulation state, alternating order\n";
 csv<<"frame,cpu_scene,cpu_submit,cpu_present,cpu_render,gpu_scene,gpu_submit,gpu_present,gpu_render\n";
 std::array<std::array<std::vector<double>,4>,2> samples;
 for(int frame=-30;frame<720&&window.pump();++frame){
  retro::InputState input{};input.mouseDx=11;input.mouseDy=std::sin(frame*.13f)*1.7f;game.update(input,1.f/60);audio.update(game,false);
  std::array<std::array<double,4>,2> timings{};
  for(int pass=0;pass<2;++pass){int mode=(pass+(frame&1))&1;renderer->setGpuActorPreparation(mode!=0);auto start=Clock::now();renderer->render(game);auto stages=renderer->frameStages();
   timings[mode]={stages[0],stages[1],stages[2],std::chrono::duration<double,std::milli>(Clock::now()-start).count()};
  }
  if(frame<0)continue;
  csv<<frame;for(int mode=0;mode<2;++mode)for(int stage=0;stage<4;++stage){samples[mode][stage].push_back(timings[mode][stage]);csv<<','<<timings[mode][stage];}csv<<'\n';
 }
 for(int mode=0;mode<2;++mode)for(int stage=0;stage<4;++stage){auto&values=samples[mode][stage];if(values.size()!=720)return 1;std::sort(values.begin(),values.end());
  report<<(mode?"GPU ":"CPU ")<<std::array<const char*,4>{"scene","submit","present","render"}[stage]<<" avg/p95/p99_ms="<<std::accumulate(values.begin(),values.end(),0.)/values.size()<<'/'<<values[values.size()*95/100]<<'/'<<values[values.size()*99/100]<<'\n';
 }
 report<<"final_health="<<game.player().health<<"\n";
 return renderer->hardwareActive()?0:36;
}
// Windowed-path repro: walk the Security Office corridors into walls and
// corners for many frames, at a native-sized hidden window, and report any
// frame where Vulkan drops to the software fallback or the frame time spikes.
__declspec(noinline) static int approachTest(){
 retro::Win32Window window(1920,1080,L"Depthworks approach test");if(!window.valid())return 1;ShowWindow(static_cast<HWND>(window.handle()),SW_HIDE);
 auto renderer=std::make_unique<retro::SoftwareRenderer>(1920,1080);if(!renderer->enableHardware(window.handle()))return 36;
 std::ofstream report("approach-test.txt");report<<renderer->hardwareName()<<'\n';
 auto game=commandArguments().find(L"--foundry")?retro::Game::mapInspection({3.5f,4.5f},0,-6,0,false,0,true):retro::Game::prologueInspection(2);bool ok=true;int frame=0;std::filesystem::create_directories(".build/approach");
 struct Leg{float yaw;int frames;bool forward;};
 // Face each corridor wall and walk into it, then back off, then a corner.
 for(auto leg:{Leg{0.f,90,true},Leg{3.14159f,60,true},Leg{1.5708f,120,true},Leg{-1.5708f,120,true},Leg{0.8f,120,true},Leg{2.4f,120,true},Leg{0.f,30,false}}){
  for(int i=0;i<leg.frames&&window.pump();++i,++frame){
   retro::InputState input{};input.forward=leg.forward;input.mouseDx=std::sin(frame*.21f)*6.f;input.mouseDy=std::cos(frame*.17f)*2.f;
   auto&player=const_cast<retro::Player&>(game.player());if(i==0)player.angle=leg.yaw;
   game.update(input,1.f/60);renderer->render(game);
   if(!renderer->hardwareActive()){report<<"frame "<<frame<<": "<<renderer->hardwareName()<<" at "<<player.pos.x<<','<<player.pos.y<<" yaw "<<player.angle<<'\n';ok=false;break;}
   if(frame%15==0)renderer->saveSceneCapture((".build/approach/frame-"+std::to_string(frame)+".ppm").c_str());
   if(frame%30==0)report<<"frame "<<frame<<" pos "<<player.pos.x<<','<<player.pos.y<<" yaw "<<player.angle<<" frame_ms "<<renderer->frameStages()[0]+renderer->frameStages()[1]<<'\n';
  }
  if(!ok)break;
 }
 report<<(ok?"PASS":"FAIL")<<": Vulkan survives walking into Security Office walls\n";return ok?0:21;
}
__declspec(noinline) static int ashfallPerformance(bool yardOnly=false,bool sustained=false,bool cpuActors=false,bool uncachedAi=false,bool fullRateAi=false,bool deepProfile=false){
 using Clock=std::chrono::steady_clock;auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
 retro::Win32Window window(retro::DisplayWidth,retro::DisplayHeight,L"Depthworks Ashfall frame profile");if(!window.valid())return 1;ShowWindow(static_cast<HWND>(window.handle()),SW_HIDE);
 auto renderer=std::make_unique<retro::SoftwareRenderer>(retro::DisplayWidth,retro::DisplayHeight);if(!renderer->enableHardware(window.handle()))return 36;
 renderer->setGpuActorPreparation(!cpuActors);
 renderer->setPanelCaching(commandArguments().find(L"--uncached-panels")==nullptr);
 renderer->setActorEmitCaching(commandArguments().find(L"--uncached-actor-emit")==nullptr);
 const int measured=sustained?3600:240;
 retro::AudioEngine audio(sustained);std::ofstream report("ashfall-performance.txt"),csv("ashfall-performance.csv");
 report<<renderer->hardwareName()<<" / native window / "<<retro::DisplayWidth<<'x'<<retro::DisplayHeight<<" / 30 simulated warmup + "<<measured<<" measured frames\nCPU wall timings, including native window presentation; not GPU timestamps.\n";
 if(sustained)report<<"Sustained lab: 60 simulated seconds; extended player health; live muted device mixer; deep instrumentation "<<(deepProfile?"enabled":"disabled")<<". Device="<<audio.available()<<'\n';
 csv<<"scene,frame,total_ms,simulation_ms,audio_ms,scene_ms,submit_ms,present_ms,health";for(auto name:retro::perf::names)csv<<","<<name;csv<<",static_builds,atlas_images,atlas_submissions,atlas_bytes,actor_lighting,actor_mesh,actor_triangle_sample_ms,route_queries,route_hits,terrain_queries,terrain_hits,cpu_growth_frames,gpu_buffer_growths,minimap_queries,minimap_rebuilds,state_queries,state_hint_hits\n";
 struct Case{const char*name;int kind;};
 for(auto test:{Case{"exclusion-arrival",0},Case{"exclusion-yard",2},Case{"exclusion-lure",1},Case{"ashfall-terrain",-1},Case{"campaign-receiving",-2},Case{"campaign-reactor",-3}}){
  if((yardOnly||sustained)&&test.kind!=2)continue;auto start=Clock::now();auto game=test.kind>=0?retro::Game::exclusionInspection(sustained?12:test.kind):test.kind==-1?retro::Game(retro::WorldId::Ashfall):test.kind==-2?retro::Game::mapInspection({1.4f,20},0,0,14,false,-25,false):retro::Game::mapInspection({18,17},retro::kPi*.5f,0,3,false,-9,false);
  game.setAiQueryCaching(!uncachedAi);
  game.setAiDecisionScheduling(!fullRateAi);
  game.setNavAuthority(commandArguments().find(L"--lattice-walk-probes")==nullptr);
  report<<test.name<<" construction_ms="<<ms(start,Clock::now())<<" ai_query_cache="<<!uncachedAi<<" nav_authority="<<(commandArguments().find(L"--lattice-walk-probes")==nullptr)<<'\n';std::array<std::vector<double>,6> samples;
  for(int frame=-30;frame<measured&&window.pump();++frame){retro::InputState input{};input.mouseDx=11;input.mouseDy=std::sin(frame*.13f)*1.7f;
   if(frame==0){retro::CollisionQueryBatch::resetStatistics();retro::ai::DecisionClock::resetStatistics();retro::ai::WalkQueryCache::resetStatistics();retro::TerrainSurfaceCache::resetStatistics();retro::ai::StateLookupHints::resetStatistics();retro::MinimapCache::resetStatistics();retro::perf::cpuGrowthFrames=retro::perf::gpuBufferGrowths=0;}
   retro::perf::enabled=!sustained||deepProfile;retro::perf::costs.fill(0);retro::perf::actorCosts.fill(0);retro::perf::actorTriangleCounter=0;auto a=Clock::now();game.update(input,1.f/60);auto b=Clock::now();audio.update(game,!sustained);auto c=Clock::now();renderer->render(game);auto d=Clock::now();auto stages=renderer->frameStages();
   if(frame<0){if(frame==-30)report<<"first_frame_ms="<<ms(a,d)<<'\n';continue;}
   std::array<double,6> values={ms(a,d),ms(a,b),ms(b,c),stages[0],stages[1],stages[2]};
   csv<<test.name<<','<<frame;for(auto value:values)csv<<','<<value;csv<<','<<game.player().health;for(auto value:retro::perf::costs)csv<<','<<value;auto uploads=renderer->atlasUploadStatistics();auto terrain=retro::TerrainSurfaceCache::statistics();auto minimap=retro::MinimapCache::statistics();csv<<','<<renderer->staticGeometryBuilds()<<','<<uploads[0]<<','<<uploads[1]<<','<<uploads[3]<<','<<retro::perf::actorCosts[0]<<','<<retro::perf::actorCosts[1]<<','<<retro::perf::actorCosts[2]<<','<<retro::ai::WalkQueryCache::statistics.queries<<','<<retro::ai::WalkQueryCache::statistics.hits<<','<<terrain.queries<<','<<terrain.hits<<','<<retro::perf::cpuGrowthFrames<<','<<retro::perf::gpuBufferGrowths<<','<<minimap.queries<<','<<minimap.rebuilds<<','<<retro::ai::StateLookupHints::statistics.queries<<','<<retro::ai::StateLookupHints::statistics.hits<<'\n';
   for(size_t i=0;i<values.size();++i)samples[i].push_back(values[i]);
  }
  const char* names[]={"total","simulation","audio","scene","submit","present"};
  for(size_t i=0;i<samples.size();++i){auto&v=samples[i];if(v.size()!=size_t(measured))return 1;std::sort(v.begin(),v.end());report<<names[i]<<" avg/p50/p95/p99/max_ms="<<std::accumulate(v.begin(),v.end(),0.)/v.size()<<'/'<<v[v.size()/2]<<'/'<<v[v.size()*95/100]<<'/'<<v[v.size()*99/100]<<'/'<<v.back()<<'\n';}
  auto queryStats=retro::CollisionQueryBatch::statistics();report<<"ai_queries="<<queryStats.queries<<" hits="<<queryStats.hits<<'\n';
  auto routeStats=retro::ai::WalkQueryCache::statistics;report<<"route_queries="<<routeStats.queries<<" hits="<<routeStats.hits<<" invalidations="<<routeStats.invalidations<<'\n';
  auto terrainStats=retro::TerrainSurfaceCache::statistics();report<<"terrain_queries="<<terrainStats.queries<<" hits="<<terrainStats.hits<<" cpu_growth_frames="<<retro::perf::cpuGrowthFrames<<" gpu_buffer_growths="<<retro::perf::gpuBufferGrowths<<'\n';
  auto stateStats=retro::ai::StateLookupHints::statistics;report<<"state_queries="<<stateStats.queries<<" hint_hits="<<stateStats.hits<<'\n';
  auto minimapStats=retro::MinimapCache::statistics();report<<"minimap_queries="<<minimapStats.queries<<" rebuilds="<<minimapStats.rebuilds<<'\n';
  for(int lane=0;lane<2;++lane){auto stats=retro::ai::DecisionClock::statistics[size_t(lane)];report<<(lane?"planning":"sensing")<<" polls="<<stats.polls<<" runs="<<stats.runs<<" wakes="<<stats.wakes<<'\n';}
  report<<"final_health="<<game.player().health<<"\n\n";report.flush();csv.flush();
 }
 return 0;
}
static int cableWindowPerformance(){
 retro::Win32Window window(retro::DisplayWidth,retro::DisplayHeight,L"RawMetal Map 6 Frame Test");
 if(!window.valid())return 1;
 auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(retro::DisplayWidth,retro::DisplayHeight);auto& renderer=*rendererStorage;
 if(!renderer.enableHardware(window.handle()))return 36;
 auto game=retro::Game::mapInspection({13.f,6.8f},retro::kPi*.5f,-8.f,6,false,-9.f,false);
 for(int i=0;i<30&&window.pump();++i){game.update({},1.f/60.f);renderer.render(game);}
 std::vector<double> frames;frames.reserve(120);
 for(int i=0;i<120&&window.pump();++i){retro::InputState look{};look.mouseDx=2.f;look.mouseDy=std::sin(i*.13f);
  game.update(look,1.f/60.f);auto start=std::chrono::steady_clock::now();renderer.render(game);
  frames.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
 if(frames.empty())return 1;std::sort(frames.begin(),frames.end());
 std::ofstream report("diagnostics/pixel-compare/cable-vaults-layout-pass/cable-window-performance.txt");
 report<<renderer.hardwareName()<<" / native window / "<<retro::DisplayWidth<<'x'<<retro::DisplayHeight<<" / async presentation\n"
       <<"Map 6 active sump sweep, 30 warmup + "<<frames.size()<<" measured frames\n"
       <<"CPU frame submission avg/p50/p95/max: "
       <<std::accumulate(frames.begin(),frames.end(),0.0)/frames.size()<<" / "<<frames[frames.size()/2]<<" / "<<frames[frames.size()*95/100]<<" / "<<frames.back()<<" ms\n"
       <<"This times the real windowed submission path; the offscreen readback benchmark is reported separately.\n";
 return 0;
}
__declspec(noinline) static int freightInspection(int W,int H){
 auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware())return 36;
 std::filesystem::create_directories("diagnostics/freight-district");
 std::ofstream("diagnostics/freight-district/model-report.txt")<<renderer->modelReport();
 for(int level=10;level<retro::FreightDistrictEnd;++level){
  const auto& chunk=retro::CampaignChunks[level];
  auto scene=retro::Game::mapInspection(chunk.playerStart,retro::kPi*.5f,-8,level,true,chunk.spawnHeight,true);
  renderer->render(scene);std::ofstream frame("diagnostics/freight-district/map-"+std::to_string(level)+".ppm",std::ios::binary);
  frame<<"P6\n"<<W<<' '<<H<<"\n255\n";
  for(int i=0;i<W*H;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}
 }
 return 0;
}
__declspec(noinline) static int freightDetailInspection(int W,int H){
 auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware())return 36;
 std::filesystem::create_directories("diagnostics/freight-details");
 struct View{const char* name;int level;retro::Vec2 p;float z,yaw,pitch;};
 const View views[]={
  {"freight-service-corridor",10,{4.4f,3},-12,retro::kPi*.5f,8},
  {"freight-equipment-bay",10,{15,9},-12,.65f,-8},
  {"freight-service-ceiling",10,{3.8f,9.3f},-12,retro::kPi*.5f,100},
  {"receiving-inspection-island",14,{12,10},-25,.65f,-4},
  {"warehouse-vehicle-bay",16,{13.5f,3},-25,1.05f,-8},
  {"warehouse-arrival",16,{12,2},-25,.85f,18},
  {"warehouse-forklift-profile",16,{9.3f,4},-25,.37f,-10},
  {"warehouse-dispatch",16,{10,11},-25,2.1f,-8},
  {"warehouse-wall-services",17,{20.8f,3},-25,retro::kPi*.5f,18},
  {"receiving-van",14,{15,3},-25,.80f,-3},
  {"intake-pallets",15,{10,9},-25,2.55f,-18},
  {"warehouse-cargo",16,{6,3},-25,1.85f,-4},
  {"manifest-office",20,{7,3},-7,1.9f,-18},
  {"manifest-terminal-front",20,{5.5f,5},-7,retro::kPi*.5f,-35},
  {"platform-board",25,{12,4.5f},-25,retro::kPi*.5f,12},
  {"depot-engine",27,{12,7},-25,.55f,4},
  {"inspection-chassis",28,{17,3},-28,retro::kPi*.5f,12},
  {"depot-gantry",29,{20.8f,8},-19,retro::kPi,-90},
  {"warehouse-break-corner",16,{7,16},-25,retro::kPi*.5f,-8},
  {"warehouse-chemical-hold",17,{14,18},-25,0,-6},
  {"manifest-records-bays",20,{12,10},-7,0,-5},
  {"platform-benches",25,{12,8},-25,1.f,-12},
  {"tunnel-service-bypass",30,{9,10},-25,retro::kPi*.5f,-4}
 };
 std::ofstream cacheReport("diagnostics/freight-details/static-cache.txt");cacheReport<<renderer->hardwareName()<<" / repeated fixed-camera screenshots\n";
 for(const auto& view:views){
  auto scene=retro::Game::mapInspection(view.p,view.yaw,view.pitch,view.level,true,view.z,true);
  renderer->render(scene);auto builds=renderer->staticGeometryBuilds();
  renderer->render(scene);renderer->render(scene);
  auto rebuilds=renderer->staticGeometryBuilds()-builds;cacheReport<<view.name<<": static rebuilds "<<rebuilds<<'\n';if(rebuilds)return 35;
  std::ofstream frame(std::string("diagnostics/freight-details/")+view.name+".ppm",std::ios::binary);frame<<"P6\n"<<W<<' '<<H<<"\n255\n";
  for(int i=0;i<W*H;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}
 }
 return 0;
}
__declspec(noinline) static int freightArtInspection(int W,int H){
 auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware())return 36;
 renderer->setEnvironmentInspection(true);
 struct View{const char* name;int level;retro::Vec2 p;float z,yaw,pitch;};
 const View views[]={
  {"freight-service-corridor",10,{4.4f,3},-12,retro::kPi*.5f,8},
  {"freight-equipment-bay",10,{15,9},-12,.65f,-8},
  {"receiving-inspection-island",14,{12,10},-25,.65f,-4},
  {"freight-service-ceiling",10,{3.8f,9.3f},-12,retro::kPi*.5f,100},
  {"warehouse-vehicle-bay",16,{13.5f,3},-25,1.05f,-8},
  {"warehouse-arrival",16,{12,2},-25,.85f,18},
  {"warehouse-forklift-profile",16,{9.3f,4},-25,.37f,-10},
  {"warehouse-dispatch",16,{10,11},-25,2.1f,-8},
  {"transfer-stair-descent",22,{6.6f,3},-7,1.15f,-25},
  {"sorting-pusher",23,{12,3},-25,retro::kPi*.5f,3},
  {"yard-cradle",24,{4,13},-25,retro::kPi*.5f,10},
  {"warehouse-wall-services",17,{20.8f,3},-25,retro::kPi*.5f,18},
  {"intake-bay",15,{12,9},-25,retro::kPi*.5f,12},
  {"warehouse-canyon",16,{12,3},-25,retro::kPi*.5f,32},
  {"warehouse-aisle",16,{6,3},-25,1.85f,-4},
  {"pit-workshop",28,{12,2},-28,1.25f,18},
  {"pit-chassis",28,{17,3},-28,retro::kPi*.5f,12},
  {"pit-toolbay",28,{20.5f,16},-25,-retro::kPi*.5f,-4},
  {"depot-railhead",27,{12,3},-25,1.0f,-4},
  {"pit-frame",28,{14,4},-25,1.1f,-8},
  {"manifest-wing",20,{7,3},-7,1.9f,-18},
  {"manifest-lookback",20,{4.5f,3},-7,-retro::kPi*.5f,-4},
  {"manifest-canyon-view",20,{4.5f,.65f},-7,-retro::kPi*.5f,-32},
  {"manifest-records",20,{8.5f,9},-7,2.6f,-12},
  {"platform-axis",25,{12,3},-25,retro::kPi*.5f,0},
  {"bore-axis",30,{12,2},-25,retro::kPi*.5f,0},
  {"bore-cables",30,{18.5f,4},-25,1.05f,-8},
  {"junction-signal-bay",31,{5,13.2f},-25,-1.95f,-10},
  {"junction-signal-console",31,{3.25f,11.65f},-25,-retro::kPi*.5f,-15},
  {"legacy-pressure",1,{3.5f,3.5f},0,.8f,-6},
  {"legacy-gantry",2,{7.5f,12.5f},0,.8f,-6},
  {"legacy-reactor",3,{3.5f,3.5f},0,.8f,-6},
  {"legacy-service",4,{6.5f,4},-9,retro::kPi*.5f,-6},
  {"legacy-coolant",5,{12,5},-9,retro::kPi*.5f,-6},
  {"legacy-foundry",0,{3.5f,4.5f},0,.8f,-5},
  {"legacy-cable",6,{13,6.8f},-9,retro::kPi*.5f,-8},
  {"legacy-pump",7,{6.5f,5.5f},-12,retro::kPi*.5f,-6},
  {"legacy-waste",9,{15,6.5f},-12,retro::kPi*.5f,-8}
 };
 for(const auto&v:views){auto scene=retro::Game::mapInspection(v.p,v.yaw,v.pitch,v.level,true,v.z,true);renderer->render(scene);
  std::ofstream frame(std::string(v.name)+".ppm",std::ios::binary);frame<<"P6\n"<<W<<' '<<H<<"\n255\n";
  for(int i=0;i<W*H;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}
}return 0;
}
__declspec(noinline) static int freightActorInspection(int W,int H){
 auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
 for(int age:{1,2200,4200,6300,6800,9700,10600,12500})for(int view=0;view<2;++view){
  auto scene=retro::Game::mapInspection(view?retro::Vec2{12.5f,13}:retro::Vec2{7.6f,9.5f},view?.7f:.69f,view?-15.f:-62.f,13,true,view?-25.f:-19.f,true);
  scene.setState(retro::stateId("receiving_worker_encounter_ms"),1);
  for(int time=0;time<age;time+=10)scene.updateMechanisms(.01f);
  renderer->render(scene);
  std::ofstream out("worker-"+std::to_string(age)+(view?"-close.ppm":"-balcony.ppm"),std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
 }return 0;
}
// Maps 0-2 (Foundry, Pressure Works, Turbine Gantry): five views each.
__declspec(noinline) static int earlyMapsInspection(int W,int H){
     auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories(".build/early-maps");
     struct View{const char* name;int level;retro::Vec2 p;float z,yaw,pitch;bool creatures=false;};
     static constexpr std::array<View,17> views{{
      {"foundry-creature",0,{5.5f,4.9f},0,0,-10,true},{"pressure-creature",1,{4.5f,11.5f},0,retro::kPi*.5f,-10,true},
      {"foundry-spawn",0,{3.5f,4.5f},0,0,-4},{"foundry-furnace",0,{7,11.5f},0,0,-6},{"foundry-hall",0,{12.5f,14.5f},0,-retro::kPi*.5f,-4},
      {"foundry-south",0,{4.5f,19},0,0,-4},{"foundry-east",0,{20.5f,12},0,retro::kPi,-4},
      {"pressure-entry",1,{3.5f,3.5f},0,retro::kPi*.5f,-6},{"pressure-pumps",1,{6,12.5f},0,0,-6},{"pressure-east",1,{19,13},0,retro::kPi,-4},
      {"pressure-south",1,{5,20},0,0,-4},{"pressure-north",1,{12,4},0,0,-4},
      {"gantry-entry",2,{3,4},0,0,-6},{"gantry-west",2,{2.5f,12.5f},0,-.2f,-6},{"gantry-core",2,{12.5f,15.5f},0,-retro::kPi*.5f,-6},
      {"gantry-east",2,{20,15.5f},0,retro::kPi,-4},{"gantry-catwalk",2,{8.5f,2.5f},3,retro::kPi*.5f,-14}
     }};
     for(const auto& view:views){
      auto scene=retro::Game::mapInspection(view.p,view.yaw,view.pitch,view.level,false,view.z,!view.creatures);renderer->render(scene);
      std::ofstream frame(std::string(".build/early-maps/")+view.name+".ppm",std::ios::binary);frame<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}
     }
     return 0;
}
__declspec(noinline) static int campaignInspection(int W,int H,bool lockersOnly=false,bool cableLockersOnly=false){
     auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware())return 36;
     struct View{const char* name;int level;retro::Vec2 p;float z,yaw,pitch;bool openDoors=false;};
     static constexpr std::array<View,26> views{{
      {"cable-entry",6,{5.5f,5.5f},-9,.65f,-5},{"cable-trench",6,{13,6.8f},-9,retro::kPi*.5f,-8},
      {"cable-breaker",6,{17.8f,11},-9,0,-4},{"annex-entry",7,{10,8},-9,-2.35f,-10},
      {"annex-lower",7,{6.5f,5.5f},-12,retro::kPi*.5f,-6},{"annex-upper",7,{18.2f,18.2f},-4,0,-8},
      {"junction-bridge",8,{5,8},-4,.35f,-12},{"waste-deck",9,{8,6.5f},-9,.85f,-12},
      {"waste-press",9,{15,6.5f},-12,retro::kPi*.5f,-8},
      {"waste-compactor",9,{11.f,6.5f},-12,.65f,-8},
      {"annex-support-under",7,{17.2f,6.5f},-12,0,-3},
      {"annex-support-deck",7,{21.5f,6.5f},-4,retro::kPi,0},
      {"junction-support-under",8,{12,7.5f},-9,0,-3},
      {"junction-service-bays",8,{11.f,14.f},-9,.55f,-4},
      {"junction-bridge-lamps",8,{13,7},-4,0,0},
      {"annex-pump-control",7,{10.25f,10.5f},-12,0,-8},
      {"annex-operator-room",7,{20.4f,18.3f},-4,.85f,-10},
      {"junction-distribution",8,{11.3f,9.5f},-9,1.f,-8},
      {"junction-stores",8,{3.8f,6.1f},-9,-retro::kPi*.5f,-10},
      {"junction-utilities",8,{3.1f,18.6f},-9,retro::kPi*.5f,-8,true},
      {"junction-freight",8,{20.5f,3.3f},-4,-retro::kPi*.5f,-8,true},
      {"waste-receiving",9,{8.8f,10.1f},-12,retro::kPi,-30},
      {"waste-safe-press",9,{12,7},-12,.65f,-8},
      {"waste-dispatch",9,{20.2f,19.2f},-12,retro::kPi*.5f,-8},
      {"waste-lockers",9,{4.1f,9.2f},-9,2.5f,-4},
      {"cable-west-lockers",6,{7.1f,20.f},-9,retro::kPi,-4}
     }};
     for(const auto& view:views){
      if(lockersOnly&&std::string_view(view.name)!="waste-lockers")continue;
      if(cableLockersOnly&&std::string_view(view.name)!="cable-west-lockers")continue;
      auto scene=retro::Game::mapInspection(view.p,view.yaw,view.pitch,view.level,view.openDoors,view.z,true);renderer->render(scene);
      std::ofstream frame(std::string(view.name)+".ppm",std::ios::binary);frame<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}
     }return 0;
    }
__declspec(noinline) static int windowPerformance(const wchar_t* commandLine,int W,int H){
        const bool doorsOnly=commandArguments().find(L"--door-performance-window")!=nullptr;
        const bool freightOnly=commandArguments().find(L"--freight-performance-window")!=nullptr;
        const bool servicesOnly=doorsOnly||freightOnly||commandArguments().find(L"--service-performance-window")!=nullptr;
        retro::Win32Window benchWindow(W,H,L"RawMetal Vulkan Performance");if(!benchWindow.valid())return 1;
        auto assetStart=std::chrono::steady_clock::now();
        auto benchRendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& benchRenderer=*benchRendererStorage;
        double assetMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-assetStart).count();
        auto hardwareStart=std::chrono::steady_clock::now();if(!benchRenderer.enableHardware(benchWindow.handle()))return 36;
        double hardwareMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-hardwareStart).count();
        std::ofstream report(doorsOnly?"door-performance-window.txt":freightOnly?"freight-performance-window.txt":servicesOnly?"service-performance-window.txt":"performance-window.txt");
        RECT benchmarkClient{};GetClientRect(static_cast<HWND>(benchWindow.handle()),&benchmarkClient);
        report<<"Presentation: "<<benchmarkClient.right<<"x"<<benchmarkClient.bottom<<" / 30 warmup frames per scene / "<<(doorsOnly?240:120)<<" measured frames\n";
        report<<benchRenderer.hardwareName()<<" / native Win32 swapchain / async present (2 frames in flight)\n";
        report<<"Present mode: IMMEDIATE preferred (uncapped), MAILBOX fallback, FIFO last resort\n\n";
        report<<"Renderer asset construction: "<<assetMs<<" ms; hardware prepare: "<<hardwareMs<<" ms (excluded from warm timing)\n";
        // Same scenes as --performance-test but using the real windowed present path.
        struct Scene{const char* name;retro::Game game;int frames;bool simulate;bool sweep;};
        std::vector<Scene> scenes;
        if(!servicesOnly){
        scenes.push_back({"Foundry turn",retro::Game::mapInspection({3.5f,4.5f},0,0,0,false,0,true),120,false,true});
        scenes.push_back({"Gantry turn",retro::Game::mapInspection({7.5f,12.5f},0,0,2,false,0,true),120,false,true});
        scenes.push_back({"Ashfall terrain + streaming",retro::Game(retro::WorldId::Ashfall),120,true,true});
        scenes.push_back({"Lift entry turn",retro::Game::mapInspection({3.5f,2},retro::kPi*.5f,0,3,false,0,true),120,false,true});
        scenes.push_back({"Hazmat settling",retro::Game::hazmatInspection(3),120,true,false});
        scenes.push_back({"Ascent window",retro::Game::liftInspection(5,3),120,true,false});
        scenes.push_back({"Reactor balcony turn",retro::Game::liftInspection(retro::World::LiftRideComplete,2),120,false,true});
        scenes.push_back({"Reactor active AI",retro::Game::mapInspection({18,17},retro::kPi*.5f,0,3,false,-9,false),120,true,true});
        }
        for(int level=servicesOnly?7:6;level<10;++level){
            const auto& def=retro::chunkDefinition(retro::WorldId::Campaign,level);
            scenes.push_back({nullptr,retro::Game::mapInspection(def.playerStart,.6f,0,level,false,def.spawnHeight,false),120,true,true});
        }
        std::vector<std::string> sceneNames{"Foundry turn","Gantry turn","Ashfall terrain + streaming","Lift entry turn","Hazmat settling","Ascent window","Reactor balcony turn","Reactor active AI","Utility chapter 7","Utility chapter 8","Utility chapter 9","Utility chapter 10"};
        if(servicesOnly)sceneNames={"Pump Annex","Utility Junction","Waste Handling"};
        if(freightOnly){
            scenes.clear();sceneNames.clear();
            for(int level:{13,14,16,20,25,27,28,29}){
                const auto& def=retro::CampaignChunks[level];
                scenes.push_back({nullptr,retro::Game::mapInspection(def.playerStart,.6f,-8,level,true,def.spawnHeight,false),120,true,true});
                sceneNames.push_back(retro::CampaignMapNames[level]);
            }
        }
        if(doorsOnly){
            scenes.clear();sceneNames.clear();
            for(int level:{0,2,6,7,8,9}){
                auto world=std::make_unique<retro::World>(level);if(world->doors().empty())continue;const auto& door=world->doors().front();
                retro::Vec2 camera{(door.left+door.right)*.5f,door.y+2.f};
                scenes.push_back({nullptr,retro::Game::mapInspection(camera,-retro::kPi*.5f,0,level,false,world->floorHeight(camera.x,camera.y)+door.z,true),240,false,false});
                sceneNames.push_back("Door cycle / map "+std::to_string(level));
            }
            report<<"Door diagnostic: fixed camera, all leaves cycle closed/open/closed; no AI simulation.\n";
        }
        bool doorCacheStable=true;
        for(int s=0;s<int(scenes.size());++s){
            auto& sc=scenes[s];
            const char* sname=sc.name?sc.name:sceneNames[s].c_str();
            // Update window title so we can see progress.
            std::wstring title=L"RawMetal Benchmark [";title+=std::to_wstring(s+1);title+=L"/";title+=std::to_wstring(scenes.size());title+=L"] ";
            for(const char* c=sname;*c;++c)title+=wchar_t(*c);
            SetWindowTextW(static_cast<HWND>(benchWindow.handle()),title.c_str());
            // Warm static caches and GPU pipelines before collecting frame times.
            for(int warm=0;warm<30;++warm){auto warmStart=std::chrono::steady_clock::now();benchRenderer.render(sc.game);if(warm==0){
                double coldMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-warmStart).count();
                report<<sname<<" first render: "<<coldMs<<" ms (cache construction; excluded from warm timing)\n";
                if(s==0)report<<"Total cold renderer assets + hardware prepare + first frame: "<<assetMs+hardwareMs+coldMs<<" ms\n";
                auto uploads=benchRenderer.atlasUploadStatistics();report<<"  atlas totals: "<<uploads[0]<<" images / "<<uploads[1]<<" transfer submissions / "<<uploads[2]<<" staging allocations / "<<uploads[3]<<" payload bytes\n";
            }}
            auto buildsBefore=benchRenderer.staticGeometryBuilds();
            std::vector<double> frameMs;frameMs.reserve(sc.frames);
            std::array<double,3> stageTotal{};
            auto wallStart=std::chrono::steady_clock::now();
            for(int frame=0;frame<sc.frames&&benchWindow.pump();++frame){
                retro::InputState input{};
                if(sc.sweep){input.mouseDx=11.f;input.mouseDy=std::sin(frame*.13f)*1.7f;}
                auto begin=std::chrono::steady_clock::now();
                if(doorsOnly){
                    // Only the inspection game is mutated: gameplay gates and streaming
                    // are covered by the campaign/streaming tests separately.
                    auto& world=const_cast<retro::World&>(sc.game.world());
                    float pose=frame<120?float(frame)/119.f:float(239-frame)/119.f;
                    for(int door=0;door<int(world.doors().size());++door)world.setDoor(door,pose,frame<120);
                }
                if(sc.simulate||sc.sweep)sc.game.update(input,1.f/60.f);
                benchRenderer.render(sc.game);
                double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                frameMs.push_back(elapsed);auto stages=benchRenderer.frameStages();for(int i=0;i<3;++i)stageTotal[i]+=stages[i];
                if(elapsed>25)report<<"  slow frame "<<frame<<": total "<<elapsed<<" / scene "<<stages[0]<<" / submit "<<stages[1]<<" / present "<<stages[2]<<" ms\n";
            }
            double wallMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-wallStart).count();
            if(frameMs.size()!=size_t(sc.frames)){report<<sname<<": window closed early\n";return 1;}
            std::sort(frameMs.begin(),frameMs.end());
            double avg=std::accumulate(frameMs.begin(),frameMs.end(),0.0)/frameMs.size();
            double p95=frameMs[frameMs.size()*95/100];
            double p99=frameMs[frameMs.size()*99/100];
            double peak=frameMs.back();
            double wallFps=frameMs.size()*1000.0/wallMs;
            report<<sname<<":\n";
            report<<"  avg scene/submit/present: "<<stageTotal[0]/sc.frames<<" / "<<stageTotal[1]/sc.frames<<" / "<<stageTotal[2]/sc.frames<<" ms\n";
            report<<"  wall-clock FPS: "<<wallFps<<" (actual throughput)\n";
            report<<"  avg frame time: "<<avg<<" ms = "<<1000.0/avg<<" FPS\n";
            report<<"  p95 frame time: "<<p95<<" ms = "<<1000.0/p95<<" FPS\n";
            report<<"  p99 frame time: "<<p99<<" ms\n";
            report<<"  max frame time: "<<peak<<" ms = "<<1000.0/peak<<" FPS\n\n";
            if(doorsOnly||freightOnly){auto rebuilds=benchRenderer.staticGeometryBuilds()-buildsBefore;doorCacheStable&=rebuilds==0;report<<"  static map rebuilds during motion: "<<rebuilds<<"\n\n";}
            report.flush();
        }
        report<<"Done.\n";
        return (doorsOnly||freightOnly)&&!doorCacheStable?35:0;
}
// Switchable lamps and true darkness, rendered (frames in .build/darkness,
// numbers in darkness-render-test.txt): a gated lamp off at load, on and off
// again by its state (power only: no static rebuild or shadow redraw), a dark
// zone near black, the same view lit by one shot, and the torch jammed then back.
static bool darknessTestMode(bool software){
 if(!retro::Game::testDarkness())return false;
 constexpr int W=640,H=360;std::ofstream report("darkness-render-test.txt");bool ok=true;
 auto check=[&](bool pass,const char*name){ok&=pass;report<<(pass?"PASS ":"FAIL ")<<name<<'\n';report.flush();};
 auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);const bool vulkan=!software&&renderer->enableHardware();
 if(!software&&!vulkan){report<<"Vulkan required for cache/shadow evidence: "<<renderer->hardwareName()<<'\n';return false;}
 report<<(vulkan?"Vulkan ":"Software renderer ")<<renderer->hardwareName()<<'\n';renderer->setEnvironmentInspection(true);
 // A 16 m room with crates and barrels, a gated ceiling lamp, a gated failing
 // wall tube and a dormant Warden ahead; all of it a jamming dark zone.
 std::string text="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Darkness render fixture|0\nMAP|0|Room|0|0|12|18.5|0|0|3.6|0.3|industrial_night|0|0|0|0|0|0|0|0|-1.5708|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row){std::string cells(24,'#');if(row>=3&&row<21)for(int x=4;x<20;++x)cells[size_t(x)]='.';
  if(row==8){cells[6]=cells[7]='C';cells[17]='B';}if(row==13){cells[5]='C';cells[18]=cells[17]='B';}if(row==6)cells[10]=cells[11]=cells[12]='C';
  text+="ROW|0|0|"+std::to_string(row)+"|"+cells+"\n";}
 text+="LIGHT|0|12|10|3.3|0|0|1.5|11|3400|0|test_lamp|1|0||||\nLIGHT|0|4|15|2|1|0|1|7|5600|0.3|test_lamp|1|0||||0.5\nDARK|0|4|3|20|21|-10|10|test_dark|1|0|1\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";
 std::shared_ptr<const retro::CustomCampaign> parsed;
 try{parsed=retro::loadCustomCampaignText(text,"darkness-render-fixture.txt");}catch(const std::exception&e){report<<"fixture: "<<e.what()<<'\n';return false;}
 auto map=std::make_shared<retro::AuthoredMapData>(*parsed->maps[0]);map->creatureSpawns.push_back({retro::CreatureKind::Warden,{15.f,11.5f},-999.f,1.f,0.f,retro::stateId("darkness_fixture_never")});
 auto campaign=std::make_shared<retro::CustomCampaign>(*parsed);campaign->maps[0]=map;retro::Game game(campaign);
 std::filesystem::create_directories(".build/darkness");
 auto frame=[&](const char*name){renderer->render(game);if(vulkan)check(renderer->hardwareActive(),"Rendered evidence stays on Vulkan");const auto*pixels=renderer->pixels();double sum=0,creatureSum=0;unsigned creaturePeak=0;
  std::ofstream out(std::string(".build/darkness/")+name+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
  for(int i=0;i<W*H;++i){auto p=pixels[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);sum+=.2126*double((p>>16)&255)+.7152*double((p>>8)&255)+.0722*double(p&255);}
  for(int y=160;y<235;++y)for(int x=475;x<520;++x){auto p=pixels[y*W+x];double l=.2126*double((p>>16)&255)+.7152*double((p>>8)&255)+.0722*double(p&255);creatureSum+=l;creaturePeak=std::max(creaturePeak,unsigned(l));}
  if(std::strcmp(name,"3-dark")==0)check(creatureSum/(45*75)<.1,"The creature has no self-lit floor in darkness");
  if(std::strcmp(name,"4-dark-muzzle-flash")==0)check(creatureSum/(45*75)>1&&creaturePeak>20,"The shot reveals the creature above the floor (numeric ROI)");
  double mean=sum/(W*H);report<<"creature ROI mean "<<creatureSum/(45*75)<<", peak "<<creaturePeak<<'\n';report<<name<<": mean luminance "<<mean<<" / 255, darkness "<<game.darkness()<<", torch "<<game.flashlightOn()<<'\n';return mean;};
 auto step=[&](int frames,retro::InputState first={}){for(int i=0;i<frames;++i){game.update(first,1.f/60.f);first={};}};
 step(1);double loadOff=frame("0-load-lamp-off");
 auto builds=renderer->staticGeometryBuilds();auto faces=renderer->shadowFacesDrawn();
 game.setState(retro::stateId("test_lamp"),1);step(1);double on=frame("1-lamp-on");
 game.setState(retro::stateId("test_lamp"),0);step(1);double off=frame("2-lamp-off");
 check(on>off*1.25&&on>off+.5,"The gated lamp lights the room when its state is set and goes out when cleared");
 check(std::fabs(loadOff-off)<std::max(1.5,off*.15),"A lamp off at load is dark on its very first frame");
 check(renderer->staticGeometryBuilds()==builds&&renderer->shadowFacesDrawn()==faces,"Switching a lamp changes power only: no static rebuild, no shadow redraw");
 game.setState(retro::stateId("test_dark"),1);step(30);double dark=frame("3-dark");
 check(game.darkness()==1&&dark<1.0,"Inside an active dark zone the room renders black");
 game.setState(retro::stateId("test_lamp"),1);step(1);double darkLamp=frame("3b-dark-lamp-on");
 check(darkLamp>dark+1,"Lamps still illuminate an active dark zone");
 game.setState(retro::stateId("test_lamp"),0);step(1);
 retro::InputState fire{};fire.fire=true;step(1,fire);double flash=frame("4-dark-muzzle-flash");
 check(flash>6&&flash>dark*8,"One shot lights the room and the creature for an instant");
 step(30);double after=frame("5-dark-after-flash");check(after<1.0,"The room falls black again after the flash");
 renderer->setEnvironmentInspection(false);step(60);step(1,fire);frame("6-dark-flash-viewmodel");step(60);frame("6b-dark-viewmodel");renderer->setEnvironmentInspection(true);
 game.giveQuestItem(retro::Game::Flashlight);game.setState(retro::stateId("flashlight_on"),1);step(120);double jammed=frame("7-flashlight-jammed");
 check(!game.flashlightOn()&&jammed<1.0,"The jammed torch gives no light inside the zone");
 game.setState(retro::stateId("test_dark"),0);step(30);
 game.setState(retro::stateId("flashlight_on"),0);double torchOff=frame("8a-zone-left-torch-off");
 std::vector<std::uint32_t> torchBaseline(renderer->pixels(),renderer->pixels()+W*H);
 game.setState(retro::stateId("flashlight_on"),1);double torch=frame("8-flashlight-restored");
 // The CPU path has no HDR shoulder; compare the illuminated pixels as well
 // as the mean so the beam must visibly brighten a meaningful part of the room.
 int torchPixels=0;double torchPeak=0;for(int i=0;i<W*H;++i){auto a=torchBaseline[size_t(i)],b=renderer->pixels()[i];double delta=.2126*(int((b>>16)&255)-int((a>>16)&255))+.7152*(int((b>>8)&255)-int((a>>8)&255))+.0722*(int(b&255)-int(a&255));torchPixels+=delta>5;torchPeak=std::max(torchPeak,delta);}
 report<<"restored torch: "<<torchPixels<<" pixels brighten by >5/255, peak delta "<<torchPeak<<'\n';
 check(game.flashlightOn()&&torch>torchOff&&torchPixels>256&&torchPeak>20,"Switching the zone off brings the torch back");
 game.setState(retro::stateId("test_lamp"),1);step(1);frame("9-lamp-on-torch");
 report<<"static geometry builds "<<builds<<" -> "<<renderer->staticGeometryBuilds()<<", lamp shadow faces "<<faces<<" -> "<<renderer->shadowFacesDrawn()<<'\n';
 check(renderer->staticGeometryBuilds()==builds&&renderer->shadowFacesDrawn()==faces,"Darkness, flashes and lamp power never rebuild static caches or shadows");
 report<<(ok?"ALL PASSED":"FAILED")<<'\n';return ok;
}
static bool darknessTest(){return darknessTestMode(false);}
// Crashes leave RawMetal-crash.txt: the exception and its offset in the
// executable, which the linker map (.build/RawMetal.map) resolves to a function.
static LONG WINAPI writeCrashReport(EXCEPTION_POINTERS* info){
    auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto address=reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress);
    if(FILE* file=std::fopen("RawMetal-crash.txt","w")){
        std::fprintf(file,"exception 0x%08lx at RawMetal.exe+0x%llx\n",info->ExceptionRecord->ExceptionCode,static_cast<unsigned long long>(address-base));
        // Return addresses on the stack near the fault, for a rough call chain.
        auto* stack=reinterpret_cast<std::uintptr_t*>(info->ContextRecord->Rsp);
        for(int i=0;i<256;++i){auto value=stack[i];if(value>base&&value<base+0x4000000)std::fprintf(file,"  stack RawMetal.exe+0x%llx\n",static_cast<unsigned long long>(value-base));}
        std::fclose(file);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static bool commandArgumentsTest(){
 std::ofstream report("command-arguments-test.txt");bool ok=true;
 CommandArguments args(L"RawMetal.exe --nopost \"--map-shots=folder with spaces/views.txt\" --zone-shots=\"another path/list.txt\" --each-test=movement --nopixel");
 auto check=[&](bool value,const char* label){ok&=value;report<<(value?"PASS ":"FAIL ")<<label<<'\n';};
 check(args.find(L"--map-shots=")&&std::wstring(args.find(L"--map-shots=")+12)==L"folder with spaces/views.txt","quoted map list value");
 check(args.find(L"--zone-shots=")&&std::wstring(args.find(L"--zone-shots=")+13)==L"another path/list.txt","quoted zone list value");
 check(args.find(L"--each-test=")&&std::wstring(args.find(L"--each-test=")+12)==L"movement","value consumes one argument");
 check(args.find(L"--nopost")&&args.find(L"--nopixel"),"switches anywhere");
 check(!args.find(L"--map-shot=")&&!args.find(L"--pixel"),"exact flag boundaries");
 for(auto flag:{L"--zone-shots=a",L"--creature-video=1",L"--vr-hand-dump",L"--map-shots=a",L"--map-shot=0",L"--map-audit=0"}){CommandArguments automation((std::wstring(L"RawMetal.exe ")+flag).c_str());check(automation.automated(),"automation suppresses dialog");}
 check(!CommandArguments(L"RawMetal.exe --vulkan --vr").automated(),"interactive startup retains error dialog");
 return ok;
}
static bool contentIntegrityTest(){
 std::ofstream report("content-integrity-test.txt");int problems=0;
 for(auto name:{"receiving_cleanup","receiving_breach"})report<<"HASH "<<name<<' '<<retro::stateId(name)<<'\n';
 auto inspect=[&](const std::string& world,int count,auto load){
  std::map<retro::StateId,std::string> events;
  for(int level=0;level<count;++level){try{auto w=load(level);std::map<retro::StateId,size_t> timers;
   for(size_t i=0;i<w.scriptEvents().size();++i){auto id=w.scriptEvents()[i].id;std::string record="map "+std::to_string(level)+" scriptEvent["+std::to_string(i)+"]";auto [it,unique]=events.emplace(id,record);if(!unique){++problems;report<<world<<" duplicate event id="<<id<<" "<<it->second<<" and "<<record<<'\n';}}
   for(size_t i=0;i<w.timedSequences().size();++i){auto id=w.timedSequences()[i].timerState;auto [it,unique]=timers.emplace(id,i);if(!unique){++problems;report<<world<<" map "<<level<<" duplicate timerState="<<id<<" timedSequence["<<it->second<<"] and ["<<i<<"]\n";}}
  }catch(const std::exception& e){++problems;report<<world<<" map "<<level<<" load error: "<<e.what()<<'\n';}}
 };
 for(auto id:{retro::WorldId::Campaign,retro::WorldId::Ashfall})inspect(id==retro::WorldId::Campaign?"Campaign":"Ashfall",retro::worldChunkCount(id),[&](int level){return retro::World(level,id);});
 for(auto load:{retro::loadEmbeddedPrologueCampaign,retro::loadEmbeddedExclusionZoneCampaign})try{auto campaign=load();inspect(campaign->name,int(campaign->maps.size()),[&](int level){return retro::World(level,campaign->maps[size_t(level)]);});}catch(const std::exception& e){++problems;report<<"Built-in campaign load error: "<<e.what()<<'\n';}
 report<<(problems?"FAIL ":"PASS ")<<problems<<" integrity problems\n";return problems==0;
}
static bool loadValidationTest(){
 std::ofstream report("load-validation-test.txt");bool ok=true;
 auto base=[](){auto m=std::make_shared<retro::AuthoredMapData>();m->name="Validation fixture";retro::AuthoredLayerData layer{};layer.name="floor";for(auto& row:layer.rows)row=std::string(24,'.');m->layers.push_back(layer);return m;};
 auto rejected=[&](const char* name,auto mutate){auto map=base();mutate(*map);try{retro::World w(7,map);report<<"FAIL accepted "<<name<<'\n';ok=false;}catch(const std::exception& e){std::string message=e.what();bool named=message.find("map 7")!=std::string::npos&&message.find("[0]")!=std::string::npos;ok&=named;report<<(named?"PASS ":"FAIL unindexed ")<<name<<": "<<message;}};
 rejected("door bounds",[](auto& m){retro::Door d;d.left=5;d.right=4;m.doors.push_back(d);});
 rejected("door maxOpen",[](auto& m){retro::Door d;d.left=1;d.right=2;d.maxOpen=0;m.doors.push_back(d);});
 rejected("structure",[](auto& m){m.structures.push_back({1,1,1,2,3,2});});
 rejected("compactor",[](auto& m){retro::Compactor c{};c.period=0;m.compactors.push_back(c);});
 rejected("fixture core",[](auto& m){retro::Fixture f{};f.core=1.1f;m.fixtures.push_back(f);});
 rejected("sequence duration",[](auto& m){retro::TimedSequence t;t.durationMs=0;m.timedSequences.push_back(t);});
 rejected("sequence cue",[](auto& m){retro::TimedSequence t;t.timerState=1;t.finishState=2;m.timedSequences.push_back(t);retro::SequenceCue c;c.timerState=1;c.timeMs=t.durationMs+1;m.sequenceCues.push_back(c);});
 rejected("sequence finish",[](auto& m){retro::TimedSequence t;t.finishAtMs=t.durationMs+1;m.timedSequences.push_back(t);});
 rejected("sequence interval",[](auto& m){retro::TimedSequence t;t.soundIntervalMs=-1;m.timedSequences.push_back(t);});
 rejected("event id",[](auto& m){retro::ScriptEvent e;e.x2=e.y2=24;m.scriptEvents.push_back(e);});
 rejected("event region",[](auto& m){retro::ScriptEvent e;e.id=1;e.requireState=2;m.scriptEvents.push_back(e);});
 try{auto m=base();retro::Door d;d.left=0;d.right=24;m->doors.push_back(d);retro::TimedSequence t;t.timerState=1;t.finishState=2;t.finishAtMs=1;m->timedSequences.push_back(t);retro::SequenceCue c;c.timerState=1;c.timeMs=t.durationMs;m->sequenceCues.push_back(c);retro::World w(7,m);report<<"PASS boundary values\n";}catch(const std::exception& e){ok=false;report<<"FAIL boundary values "<<e.what()<<'\n';}
 // Exercise the custom parser as well as direct C++ authored records.
 std::string custom="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Validation|0\nMAP|0|Validation|0|0|2|2|0|0|8|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|floor|0|0\n";
 for(int row=0;row<24;++row)custom+="ROW|0|0|"+std::to_string(row)+"|........................\n";
 for(auto record:{"TERRAIN|1|-10|2|34|0.45","TERRAIN|1|16|3|34|0.45","DOOR|0|3|2|4|0|0|0|0|0|-1","STRUCT|0|1|1|1|2|0|2|0|0","COMPACTOR|0|1|1|2|2|0|2|0"}){
  try{retro::loadCustomCampaignText(custom+record+"\n--- CUSTOM_CAMPAIGN_DATA_END ---\n","validation.txt");ok=false;report<<"FAIL parser accepted "<<record<<'\n';}catch(const std::exception& e){report<<"PASS parser rejected "<<record<<": "<<e.what()<<'\n';}
 }
 // Actor visuals are appended: the last one parses, the value past it does not.
 for(int visual:{int(retro::LastActorVisual),int(retro::LastActorVisual)+1}){
  std::string record="ACTOR|0|staff_clock|"+std::to_string(visual)+"|1.7|0|0|0";bool last=visual==int(retro::LastActorVisual);
  const std::string body="TIMED_SEQUENCE|0|staff_clock|staff_clock_done|0|0|24|24|-100|100|60000|60000|0|0|0|0|1|1\n"+record+"\nACTOR_KEY|0|0|0|5|4.5|0|0|4|1\nACTOR_KEY|0|0|60000|5|4.5|0|0|4|1";
  try{auto parsed=retro::loadCustomCampaignText(custom+body+"\n--- CUSTOM_CAMPAIGN_DATA_END ---\n","validation.txt");bool staff=last&&parsed->maps.at(0)->actorTracks.at(0).visual==retro::ActorVisual::LabTech;ok&=staff;report<<(staff?"PASS":"FAIL")<<" parser accepted "<<record<<'\n';}
  catch(const std::exception& e){ok&=!last;report<<(last?"FAIL":"PASS")<<" parser rejected "<<record<<": "<<e.what()<<'\n';}
 }
 report<<(ok?"PASS":"FAIL")<<'\n';return ok;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR commandLine,int){
    SetUnhandledExceptionFilter(writeCrashReport);
    try {
    retro::TerrainSurfaceCache::enabled=commandArguments().find(L"--uncached-terrain-surfaces")==nullptr;
    retro::MinimapCache::enabled=commandArguments().find(L"--uncached-minimap")==nullptr;
    // Keep renderer storage on the heap throughout this entry point. The
    // many mutually exclusive inspection branches otherwise reserve their
    // large renderer objects in one native stack frame before dispatch.
    constexpr int W=retro::DisplayWidth,H=retro::DisplayHeight;
    // Render A/B switches apply to every mode below, inspections included.
    applyRenderSwitches();
    if(commandArguments().find(L"--no-impostors"))retro::SoftwareRenderer::s_impostors=false;
    if(commandArguments().find(L"--smooth"))retro::SoftwareRenderer::setCrispTexels(false); // A/B: plain bilinear texels
    if(commandArguments().find(L"--notilt"))retro::SoftwareRenderer::setTiltShift(false); // A/B: no tilt-shift
    if(commandArguments().find(L"--notaa"))retro::SoftwareRenderer::setTemporalAA(false); // A/B: MSAA only
    if(commandArguments().find(L"--nodetail"))retro::SoftwareRenderer::setDetailLayer(0); // A/B: no close-up grain
    if(commandArguments().find(L"--detail-field"))retro::SoftwareRenderer::setDetailLayer(2); // debug: raw detail field
    if(commandArguments().find(L"--detail-normal"))retro::SoftwareRenderer::setDetailLayer(3); // debug: detail normal
    if(commandArguments().find(L"--detail-uv"))retro::SoftwareRenderer::setDetailLayer(4); // debug: texel coordinate sawtooth
    if(commandArguments().find(L"--detail-mag"))retro::SoftwareRenderer::setDetailLayer(5); // debug: uv magnitude and surface class
    if(commandArguments().find(L"--detail-nmap"))retro::SoftwareRenderer::setDetailLayer(6); // debug: normal map sample before detail
    if(commandArguments().find(L"--detail-ppt"))retro::SoftwareRenderer::setDetailLayer(7); // debug: pixels per texel and detail cell coordinate
    if(commandArguments().find(L"--detail-sun"))retro::SoftwareRenderer::setDetailLayer(8); // debug: sun visibility, N.L, cascade
    if(commandArguments().find(L"--detail-light"))retro::SoftwareRenderer::setDetailLayer(9); // debug: ambient (r), lamp (g), not per-pixel lit (b)
    if(commandArguments().find(L"--minimap-cache-test"))return retro::MinimapCache::test()?0:35;
    if(commandArguments().find(L"--neural-texture-runtime-test"))return neuralTextureRuntimeTest(W,H,commandArguments().find(L"--vulkan")!=nullptr);
    if(commandArguments().find(L"--stealth-test")){bool stealth=retro::Game::testStealth(),caution=retro::Game::testCaution();return stealth&&caution?0:21;}
    if(commandArguments().find(L"--exclusion-test"))return retro::Game::testExclusion()?0:21;
    if(commandArguments().find(L"--exclusion-authoring-test"))return retro::Game::testExclusionAuthoring()?0:21;
    if(commandArguments().find(L"--prologue-test"))return retro::Game::testPrologue()?0:21;
    if(commandArguments().find(L"--ui-inspection")){
     auto renderer=std::make_unique<retro::SoftwareRenderer>(W,H);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories(".build/ui-preview");
     for(int view=0;view<6;++view){auto scene=retro::Game::uiInspection(view);renderer->render(scene);std::ofstream out(".build/ui-preview/view-"+std::to_string(view)+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(auto arg=commandArguments().find(L"--map-audit=")){
     int level=_wtoi(arg+12);try{return retro::Game::auditMap(level)?0:21;}
     catch(const std::exception& e){std::ofstream("map-audit-"+std::to_string(level)+".txt")<<"ERROR loading map "<<level<<": "<<e.what()<<'\n';throw;}
    }
    if(commandArguments().find(L"--map-tour")){
     // Authored viewpoints through a rebuilt map: x, y, eye z (floor), yaw (deg), pitch.
     struct View{float x,y,z,yaw,pitch;};
     int level=0;if(auto arg=commandArguments().find(L"--map-tour="))level=_wtoi(arg+11);
     static const std::vector<View> tours[4]={
      {{3.5f,4.5f,0,0,0},{8,2.2f,0,60,-6},{12,5.5f,0,90,-4},{21.5f,6,0,180,-6},{11,8.6f,0,90,-8},{4.5f,12,0,0,-6},{8,15.5f,0,-60,-4},{21,8.9f,2.6f,180,-18},{20.5f,15.5f,0,-90,0},{2,15,0,90,-4},{2.5f,18,0,0,-4},{8,21.5f,0,-90,-4},{17,18,0,45,-6},{21.5f,19.5f,0,90,-4}},
      {},{},{}};
     const auto& views=tours[std::clamp(level,0,3)];
     auto renderer=std::make_unique<retro::SoftwareRenderer>(960,540);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories(".build/tour");
     for(size_t i=0;i<views.size();++i){const auto&v=views[i];
      auto scene=retro::Game::mapInspection({v.x,v.y},v.yaw*retro::kPi/180.f,v.pitch,level,true,v.z,true);
      renderer->render(scene);
      std::ofstream out(".build/tour/view-"+std::to_string(level)+"-"+std::to_string(i)+".ppm",std::ios::binary);out<<"P6\n"<<960<<' '<<540<<"\n255\n";
      for(int k=0;k<960*540;++k){auto px=renderer->pixels()[k];char rgb[]={char(px>>16),char(px>>8),char(px)};out.write(rgb,3);}
     }
     return 0;
    }
    if(auto arg=commandArguments().find(L"--residency=")){
     // Which chunks stream in at a map's spawn: the streaming footprint.
     int level=_wtoi(arg+12);const auto& def=retro::chunkDefinition(retro::WorldId::Campaign,level);
     auto game=retro::Game::mapInspection(def.playerStart,0,0,level,false,def.spawnHeight,true);std::ofstream out("residency.txt");
     for(int other=0;other<game.chunkCount();++other)if(other==level||game.chunkResident(other))out<<other<<' ';
     out<<'\n';return 0;
    }
    if(auto arg=commandArguments().find(L"--map-dump=")){
     // Every collision and render primitive of a map, for inspection.
     int level=_wtoi(arg+11);auto game=retro::Game::mapInspection({12,12},0,0,level,false,0,true);const auto& w=game.world();
     std::ofstream out("map-dump-"+std::to_string(level)+".txt");
     for(const auto& s:w.structures())out<<"S "<<s.x1<<' '<<s.y1<<' '<<s.x2<<' '<<s.y2<<" z "<<s.bottom<<' '<<s.top<<" rail "<<s.rail<<" mat "<<s.material<<'\n';
     for(const auto& f:w.fixtures())out<<"F "<<f.model<<" at "<<f.position.x<<' '<<f.position.y<<" base "<<f.base<<" size "<<f.width<<' '<<f.depth<<' '<<f.height<<" yaw "<<f.yaw<<" solid "<<f.solid<<'\n';
     for(const auto& l:w.lights())out<<"L "<<l.position.x<<' '<<l.position.y<<" z "<<l.z<<" mount "<<int(l.mount)<<" yaw "<<l.yaw<<'\n';
     for(const auto& p:w.pipes())out<<"P "<<p.start.x<<' '<<p.start.y<<" -> "<<p.end.x<<' '<<p.end.y<<" z "<<p.z<<" r "<<p.radius<<" endZ "<<p.endZ<<'\n';
     for(const auto& d:w.doors())out<<"D "<<d.left<<'-'<<d.right<<" y "<<d.y<<" z "<<d.z<<" transfer "<<d.transfer<<" entry "<<d.entry<<" swing "<<d.swinging<<'\n';
     for(const auto& t:w.terminals())out<<"T "<<t.position.x<<' '<<t.position.y<<" z "<<t.z<<" '"<<t.title<<"'\n";
     for(size_t i=0;i<w.layers().size();++i){out<<"LAYER "<<w.layers()[i].name<<" z "<<w.layers()[i].elevation<<'\n';for(auto& r:w.layers()[i].rows)out<<r<<'\n';}
     return 0;
    }
    if(auto arg=commandArguments().find(L"--map-shot=")){
     // One inspection frame: --map-shot=level,x,y,feet,yaw(deg),pitch[,creatures]
     int level=0,creatures=0;float x=0,y=0,z=0,yaw=0,pitch=0;
     swscanf_s(arg+11,L"%d,%f,%f,%f,%f,%f,%d",&level,&x,&y,&z,&yaw,&pitch,&creatures);
     auto renderer=std::make_unique<retro::SoftwareRenderer>(960,540);if(!renderer->enableHardware())return 36;
     auto scene=retro::Game::mapInspection({x,y},yaw*retro::kPi/180.f,pitch,level,true,z,creatures==0);
     renderer->render(scene);std::filesystem::create_directories(".build/shot");
     std::ofstream out(".build/shot/shot.ppm",std::ios::binary);out<<"P6\n"<<960<<' '<<540<<"\n255\n";
     for(int k=0;k<960*540;++k){auto px=renderer->pixels()[k];char rgb[]={char(px>>16),char(px>>8),char(px)};out.write(rgb,3);}
     return 0;
    }
    if(auto arg=commandArguments().find(L"--map-shots=")){
     // Many inspection frames: each line "name level x y feet yaw(deg) pitch".
     // An optional trailing "fire N" fires the gun once and advances N frames;
     // "wait N" just advances N frames (title cards, timed sequences);
     // "set a,b,c" switches those states on first (powered plant, solved puzzles).
     // "caution N" puts the guard squads of that chunk on Caution, then waits N frames.
     // --clean-shots leaves out the hands and HUD.
     std::wstring listPath(arg+12);std::ifstream list{std::filesystem::path(listPath)};std::string line;
     if(!list){std::ofstream("map-shots.txt")<<"ERROR: cannot open list file: "<<std::filesystem::path(listPath).string()<<'\n';return 21;}
     std::ofstream("map-shots.txt")<<"List: "<<std::filesystem::path(listPath).string()<<'\n';
     auto renderer=std::make_unique<retro::SoftwareRenderer>(960,540);if(!renderer->enableHardware())return 36;
     renderer->setEnvironmentInspection(commandArguments().find(L"--clean-shots")!=nullptr);
     std::filesystem::create_directories(".build/shot");
     size_t captures=0;
     while(std::getline(list,line)){
      std::istringstream fields(line);std::string name,action;int level=0,frames=0;float x=0,y=0,z=0,yaw=0,pitch=0;
      if(!(fields>>name>>level>>x>>y>>z>>yaw>>pitch))continue;fields>>action;
      auto scene=retro::Game::mapInspection({x,y},yaw*retro::kPi/180.f,pitch,level,true,z,true);
      if(action=="set"){std::string states;fields>>states;std::istringstream each(states);std::string state;
       while(std::getline(each,state,','))scene.setState(retro::stateId(state));action="wait";frames=2;}
      else fields>>frames;
      if(action=="caution"){for(size_t i=0;i<scene.npcActors().size();++i)if(scene.npcActors()[i].level==scene.level())scene.startGuardCaution(i);action="wait";}
      if(action=="fire"){retro::InputState fire{};fire.fire=true;scene.update(fire,.02f);for(int i=0;i<frames;++i)scene.update({},1.f/60.f);}
      else{scene.stowWeapon();
       // "hide N": E at the locker in front, then N frames looking out of it.
       if(action=="hide"){retro::InputState use{};use.use=true;scene.update(use,1.f/60.f);action="wait";}
       if(action=="wait")for(int i=0;i<frames;++i)scene.update({},1.f/60.f);}
      renderer->render(scene);++captures;
      std::ofstream out(".build/shot/"+name+".ppm",std::ios::binary);out<<"P6\n"<<960<<' '<<540<<"\n255\n";
      for(int k=0;k<960*540;++k){auto px=renderer->pixels()[k];char rgb[]={char(px>>16),char(px>>8),char(px)};out.write(rgb,3);}
     }
     std::ofstream("map-shots.txt",std::ios::app)<<"Rendered "<<captures<<" views; post="<<retro::SoftwareRenderer::postProcessing()<<" pixel="<<retro::SoftwareRenderer::pixelLighting()<<'\n';
     return 0;
    }
    if(auto arg=commandArguments().find(L"--map-sweep=")){
     // Every reachable 3 m block of a map (map-views-N.txt from --map-audit),
     // photographed facing north, east, south and west.
     int level=_wtoi(arg+12);std::ifstream list("map-views-"+std::to_string(level)+".txt");
     if(!list){std::ofstream("map-sweep.txt")<<"ERROR: cannot open list file map-views-"<<level<<".txt\n";return 21;}
     auto renderer=std::make_unique<retro::SoftwareRenderer>(640,360);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories(".build/sweep");float x,y,z;int index=0;
     while(list>>x>>y>>z){
      for(int facing=0;facing<4;++facing){
       auto scene=retro::Game::mapInspection({x,y},facing*retro::kPi*.5f,-4,level,true,z,true);
       renderer->render(scene);
       std::ofstream out(".build/sweep/"+std::to_string(level)+"-"+std::to_string(index)+"-"+std::to_string(facing)+".ppm",std::ios::binary);out<<"P6\n"<<640<<' '<<360<<"\n255\n";
       for(int k=0;k<640*360;++k){auto px=renderer->pixels()[k];char rgb[]={char(px>>16),char(px>>8),char(px)};out.write(rgb,3);}
      }
      std::ofstream(".build/sweep/"+std::to_string(level)+"-"+std::to_string(index)+".txt")<<x<<' '<<y<<' '<<z;
      ++index;
     }
     return 0;
    }
    if(commandArguments().find(L"--prologue-inspection")){
     const int width=commandArguments().find(L"--native-capture")?1920:960,height=commandArguments().find(L"--native-capture")?1080:540;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories(".build/prologue-preview");
     for(int stage=0;stage<11;++stage){auto scene=retro::Game::prologueInspection(stage);renderer->render(scene);std::ofstream out(".build/prologue-preview/view-"+std::to_string(stage)+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(auto arg=commandArguments().find(L"--zone-shots=")){
     // Exclusion Zone views: each line "name map x y yaw(deg) pitch [lift m]".
     // --no-impostors renders the same views with every real mesh (A/B for
     // Painterly Horizon LOD); the atlases are written beside the views.
     std::ifstream list{std::filesystem::path(arg+13)};std::string line;
     if(!list){std::ofstream("zone-shots.txt")<<"ERROR: cannot open list file: "<<std::filesystem::path(arg+13).string()<<'\n';return 21;}
     std::ofstream("zone-shots.txt")<<"List: "<<std::filesystem::path(arg+13).string()<<'\n';
     auto renderer=std::make_unique<retro::SoftwareRenderer>(960,540);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories(".build/shot");
     while(std::getline(list,line)){
      std::istringstream fields(line);std::string name;int level=0;float x=0,y=0,yaw=0,pitch=0,lift=0;
      if(!(fields>>name>>level>>x>>y>>yaw>>pitch))continue;fields>>lift;
      auto scene=retro::Game::zoneInspection(level,{x,y},yaw,pitch,lift);renderer->render(scene);
      std::ofstream out(".build/shot/"+name+".ppm",std::ios::binary);out<<"P6\n"<<960<<' '<<540<<"\n255\n";
      for(int k=0;k<960*540;++k){auto px=renderer->pixels()[k];char rgb[]={char(px>>16),char(px>>8),char(px)};out.write(rgb,3);}
     }
     // Steady-state frame cost of each view (static caches warm), for A/B runs.
     {std::ifstream again{std::filesystem::path(arg+13)};if(!again){std::ofstream("zone-shots.txt",std::ios::app)<<"ERROR: cannot reopen list file for timings\n";return 21;}std::ofstream timing(std::string(".build/shot/timing")+(retro::SoftwareRenderer::s_impostors?"":"-real")+".txt");
      while(std::getline(again,line)){std::istringstream fields(line);std::string name;int level=0;float x=0,y=0,yaw=0,pitch=0,lift=0;
       if(!(fields>>name>>level>>x>>y>>yaw>>pitch))continue;fields>>lift;auto scene=retro::Game::zoneInspection(level,{x,y},yaw,pitch,lift);
       for(int i=0;i<5;++i)renderer->render(scene);auto start=std::chrono::steady_clock::now();std::array<double,3> stages{};
       for(int i=0;i<40;++i){renderer->render(scene);auto s=renderer->frameStages();for(int k=0;k<3;++k)stages[size_t(k)]+=s[size_t(k)]/40;}
       timing<<name<<' '<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/40<<" ms (scene "<<stages[0]<<", gpu "<<stages[1]<<", present "<<stages[2]<<")\n";
       // --perf-variants: the same view with the post pass, then per-pixel
       // lighting (lamps and sun cascades), switched off, to find the cost.
       if(commandArguments().find(L"--perf-variants"))for(int variant=0;variant<2;++variant){
        applyRenderSwitches(variant==0,variant==1);
        for(int i=0;i<5;++i)renderer->render(scene);double gpu=0;for(int i=0;i<20;++i){renderer->render(scene);gpu+=renderer->frameStages()[1]/20;}
        timing<<"  without "<<(variant?"pixel lighting":"post")<<" (post="<<retro::SoftwareRenderer::postProcessing()<<", pixel="<<retro::SoftwareRenderer::pixelLighting()<<"): gpu "<<gpu<<'\n';
        applyRenderSwitches();}}}
     renderer->saveImpostors(".build/shot/impostor");
     return 0;
    }
    if(commandArguments().find(L"--exclusion-inspection")){
     constexpr int width=960,height=540;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;
     const bool dumpCascades=commandArguments().find(L"--sun-cascades")!=nullptr;
     std::filesystem::create_directories(".build/exclusion-preview");std::ofstream report(".build/exclusion-preview/runtime.txt");report<<renderer->hardwareName()<<'\n';
     for(int stage:{0,1,2,3,4,5,6,7,8,9,10,11,13,14,15,16}){auto scene=retro::Game::exclusionInspection(stage);renderer->render(scene);if(dumpCascades&&stage==13)renderer->saveSunCascades(".build/exclusion-preview/cascade");std::ofstream out(".build/exclusion-preview/view-"+std::to_string(stage)+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}report<<"view "<<stage<<" health="<<scene.player().health<<" npcs="<<scene.npcActors().size()<<" gripError="<<renderer->gripError()<<'\n';}
     return 0;
    }
    if(commandArguments().find(L"--guard-combat-test"))return retro::Game::testGuardCombat()?0:21;
    if(commandArguments().find(L"--terrain-query-test"))return retro::World::testCollisionCandidates()?0:21;
    if(commandArguments().find(L"--ai-query-test"))return retro::Game::testAiQueryBatch()?0:21;
    if(commandArguments().find(L"--ai-decision-test"))return retro::Game::testAiDecisionClock()?0:21;
    if(commandArguments().find(L"--guard-tactics-test"))return retro::Game::testGuardTactics()?0:21;
    if(commandArguments().find(L"--guard-policy-test"))return retro::Game::testGuardPolicy()?0:21;
    if(commandArguments().find(L"--guard-defense-test"))return retro::Game::testGuardDefense()?0:21;
    if(commandArguments().find(L"--guard-pursuit-test"))return retro::Game::testGuardPursuit()?0:21;
    if(commandArguments().find(L"--guard-actions-test"))return retro::Game::testGuardActions()?0:21;
    if(commandArguments().find(L"--guard-roles-test"))return retro::Game::testGuardRoles()?0:21;
    if(commandArguments().find(L"--guard-table-test"))return retro::Game::testGuardTable()?0:21;
    if(commandArguments().find(L"--nav-graph-test"))return retro::Game::testNavGraph()?0:21;
    if(commandArguments().find(L"--creature-behaviour-test"))return retro::Game::testCreatureBehaviour()?0:21;
    if(commandArguments().find(L"--engine-regression-test"))return retro::Game::testEngineRegression()?0:21;
    if(commandArguments().find(L"--creature-schedule-test"))return retro::Game::testCreatureSchedule()?0:21;
    if(commandArguments().find(L"--civilian-schedule-test"))return retro::Game::testCivilianSchedule()?0:21;
    if(commandArguments().find(L"--guard-patient-test"))return retro::Game::testGuardPatient()?0:21;
    if(commandArguments().find(L"--guard-cover-test"))return retro::Game::testGuardCover()?0:21;
    if(commandArguments().find(L"--guard-schedule-test"))return retro::Game::testGuardSchedule()?0:21;
    if(commandArguments().find(L"--guard-danger-test"))return retro::Game::testGuardDanger()?0:21;
    if(commandArguments().find(L"--guard-danger-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
     std::filesystem::create_directories("diagnostics/guard-danger-20261011");const char*names[]={"danger-ready","danger-escape","danger-clear"};
     for(int stage=0;stage<3;++stage){auto scene=retro::Game::guardDangerInspection(stage);renderer->render(scene);std::ofstream out(std::string("diagnostics/guard-danger-20261011/")+names[stage]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(commandArguments().find(L"--engineer-drone-test"))return retro::Game::testEngineerDrones()?0:21;
    if(commandArguments().find(L"--squad-support-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
     std::filesystem::create_directories("diagnostics/squad-support-20261012");const char*names[]={"patient-withdraw","medic-kit","patient-resume"};
     for(int stage=0;stage<3;++stage){auto scene=retro::Game::squadSupportInspection(stage);renderer->render(scene);std::ofstream out(std::string("diagnostics/squad-support-20261012/")+names[stage]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(commandArguments().find(L"--engineer-support-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
     std::filesystem::create_directories("diagnostics/engineer-support-20261012");const char*names[]={"terminal-deploy","five-drone-fleet","terminal-maintenance","pistol-support","pad-equip","pad-stow"};
     for(int stage=0;stage<6;++stage){auto scene=retro::Game::engineerSupportInspection(stage);renderer->render(scene);std::ofstream out(std::string("diagnostics/engineer-support-20261012/")+names[stage]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(commandArguments().find(L"--tactical-grenade-test"))return retro::Game::testTacticalGrenades()?0:21;
    if(commandArguments().find(L"--kill-squad-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
     std::filesystem::create_directories("diagnostics/kill-squad-20261011");std::ofstream report("diagnostics/kill-squad-20261011/playground-runtime.txt");report<<renderer->hardwareName()<<'\n';const char*names[]={"cleanup-ready","cleanup-grenade","cleanup-fire","cleanup-pursuit","cleanup-engineer-throw","cleanup-assault-close","cleanup-sharpshooter-close","cleanup-heavy-close","cleanup-medic-close","cleanup-rusher-close","cleanup-drone","cleanup-medkit-opening","cleanup-medkit-treatment","cleanup-medkit-healed"};
     int firstStage=0,lastStage=13;if(auto flag=commandArguments().find(L"--kill-squad-stage=")){wchar_t*end=nullptr;long selected=std::wcstol(flag+19,&end,10);if(end==flag+19||selected<0||selected>13)return 21;firstStage=lastStage=int(selected);}
     for(int stage=firstStage;stage<=lastStage;++stage){auto scene=retro::Game::killSquadInspection(stage);report<<names[stage]<<": PASS health="<<scene.player().health<<" grenades="<<scene.tacticalGrenades().size()<<" drones="<<scene.engineerDrones().size();
      for(size_t actor=0;actor<scene.npcActors().size();++actor){const auto&npc=scene.npcActors()[actor];if(npc.homeLevel!=1||npc.track().ai.mode!=retro::ActorAiMode::Guard)continue;
       if(npc.track().ai.role==retro::GuardSquadRole::Medic)report<<" treatment="<<scene.npcWeaponPose(actor).treatment;
       if(npc.track().ai.role==retro::GuardSquadRole::Heavy)report<<" patientDamage="<<scene.state(npc.track().damageState);
       report<<" npc"<<actor<<"="<<npc.level<<'/'<<npc.pose.position.x<<','<<npc.pose.position.y<<" role="<<int(npc.track().ai.role)<<" clip="<<npc.pose.clip<<" phase="<<npc.pose.phase;}
      report<<'\n';report.flush();renderer->render(scene);std::ofstream out(std::string("diagnostics/kill-squad-20261011/")+names[stage]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(commandArguments().find(L"--civilian-inspection")){
     // The Worker and the research staff, front and side, through drawActor:
     // each baked clip at a mid phase, death at its end, and armed.
     constexpr int width=960,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
     const std::string directory="diagnostics/civilian-models";std::filesystem::create_directories(directory);
     const char*bodies[]={"worker","scientist","senior-scientist","female-scientist","female-doctor","lab-tech"};
     struct Pose{const char*name;int clip;float phase;};constexpr Pose poses[]={{"idle",0,.3f},{"walk",1,.25f},{"punch",2,.5f},{"hit",3,.5f},{"death",4,1.f},{"crouch",5,.3f},{"armed",-1,.3f}};
     for(int body=0;body<6;++body)for(const auto&pose:poses)for(int view=0;view<2;++view){auto scene=retro::Game::civilianInspection(body,pose.clip,pose.phase,view);renderer->render(scene);
      std::ofstream out(directory+"/"+bodies[body]+"-"+pose.name+(view?"-side":"-front")+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";
      for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(commandArguments().find(L"--cleanup-layer-inspection")){
     constexpr int width=960,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
     int inspectionStage=0;if(auto flag=commandArguments().find(L"--cleanup-layer-stage="))inspectionStage=int(std::wcstol(flag+22,nullptr,10));if(inspectionStage<0||inspectionStage>3)return 21;renderer->setCleanupInspectionStage(inspectionStage);
     std::string inspectionDirectory="diagnostics/animation-repair-20261013/native";if(inspectionStage)inspectionDirectory+="-stage-"+std::to_string(inspectionStage);std::filesystem::create_directories(inspectionDirectory);
     const char*roles[]={"assault","engineer","sharpshooter","heavy","medic","rusher"};const char*poses[]={"idle","walk","walk-wrap","aim-up","aim-down","aim-left","aim-right","crouch","fire","reload","support","death","crouch-aim-up","crouch-aim-down","low-ready","crouch-walk"};
     auto filter=[&](const wchar_t*prefix,int maximum){auto flag=commandArguments().find(prefix);if(!flag)return -1;auto value=flag+std::wcslen(prefix);wchar_t*end=nullptr;long selected=std::wcstol(value,&end,10);return end==value||selected<0||selected>maximum?-2:int(selected);};
     int roleFilter=filter(L"--cleanup-layer-role=",5),sampleFilter=filter(L"--cleanup-layer-sample=",15);if(roleFilter<-1||sampleFilter<-1)return 21;
     float phase=-1;if(auto flag=commandArguments().find(L"--cleanup-layer-phase=")){wchar_t*end=nullptr;phase=float(std::wcstod(flag+22,&end));if(end==flag+22||!std::isfinite(phase)||phase<0||phase>1)return 21;inspectionDirectory+="-phase-"+std::to_string(phase);std::filesystem::create_directories(inspectionDirectory);}
     std::string reportName=inspectionDirectory+"/attachments";if(roleFilter>=0)reportName+="-role-"+std::to_string(roleFilter);if(sampleFilter>=0)reportName+="-sample-"+std::to_string(sampleFilter);
     std::ofstream report(reportName+".csv");report<<"role,pose,view,wristReachError,fingerEdgeError,barrelError,wholeMeshEdgeStretch\n";bool passed=true;
     for(int role=0;role<6;++role)for(int sample=0;sample<16;++sample)for(int view=0;view<2;++view){if(roleFilter>=0&&role!=roleFilter||sampleFilter>=0&&sample!=sampleFilter)continue;auto scene=retro::Game::cleanupLayerInspection(role,sample,view,phase);renderer->render(scene);
      auto errors=renderer->cleanupAttachmentErrors();report<<roles[role]<<','<<poses[sample]<<','<<view<<','<<errors[0]<<','<<errors[1]<<','<<errors[2]<<','<<errors[3]<<'\n';report.flush();passed&=errors[0]<.02f&&errors[1]<.00002f&&errors[2]<.002f&&errors[3]<8.f;
      auto name=inspectionDirectory+"/"+roles[role]+"-"+poses[sample]+(view?"-side":"-front")+".ppm";std::ofstream out(name,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";
      for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return passed?0:47;
    }
    if(commandArguments().find(L"--guard-policy-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;renderer->setEnvironmentInspection(true);
     std::filesystem::create_directories("diagnostics/guard-policy-20261011");std::ofstream report("diagnostics/guard-policy-20261011/playground-runtime.txt");report<<renderer->hardwareName()<<'\n';const char*names[]={"squad-activated","squad-alert","squad-fire","squad-reload-cover"};
     for(int stage=0;stage<4;++stage){auto scene=retro::Game::guardPolicyInspection(stage);report<<names[stage]<<": PASS health="<<scene.player().health<<" target-damage="<<scene.state("ash_target_damage");for(size_t i=0;i<scene.npcActors().size();++i){const auto&npc=scene.npcActors()[i];auto weapon=scene.npcWeaponPose(i);report<<" npc"<<i<<"="<<int(scene.guardState(i))<<'/'<<npc.pose.position.x<<','<<npc.pose.position.y<<" flash="<<weapon.flash;}report<<'\n';
      renderer->render(scene);std::ofstream out(std::string("diagnostics/guard-policy-20261011/")+names[stage]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(commandArguments().find(L"--guard-tactics-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;
     renderer->setEnvironmentInspection(true);
     std::filesystem::create_directories("diagnostics/guard-tactics-20261011");std::ofstream report("diagnostics/guard-tactics-20261011/playground-runtime.txt");report<<renderer->hardwareName()<<'\n';const char*names[]={"guard-covered","guard-reposition","guard-clear-fire"};
     for(int stage=0;stage<3;++stage){auto scene=retro::Game::guardTacticsInspection(stage);const auto&npc=scene.npcActors().at(0);auto weapon=scene.npcWeaponPose(0);bool passed=weapon.armed&&(stage<2?scene.player().health==100:scene.player().health==82&&weapon.flash>0);report<<names[stage]<<": "<<(passed?"PASS":"FAIL")<<" health="<<scene.player().health<<" position="<<npc.pose.position.x<<','<<npc.pose.position.y<<" raise="<<weapon.raise<<" flash="<<weapon.flash<<'\n';if(!passed)return 21;
      renderer->render(scene);std::ofstream out(std::string("diagnostics/guard-tactics-20261011/")+names[stage]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(commandArguments().find(L"--guard-combat-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories("diagnostics/guard-combat-20261011");std::ofstream report("diagnostics/guard-combat-20261011/playground-runtime.txt");report<<renderer->hardwareName()<<'\n';const char*names[]={"guard-unarmed-control","guard-low-ready","guard-aim","guard-fire","guard-fire-side"};
     for(int stage=-1;stage<4;++stage){auto scene=retro::Game::guardCombatInspection(stage);auto weapon=scene.npcWeaponPose(0);bool passed=stage<0?!weapon.armed:weapon.armed&&(stage<2||weapon.flash>0&&scene.player().health==82);report<<names[stage+1]<<": "<<(passed?"PASS":"FAIL")<<" health="<<scene.player().health<<" raise="<<weapon.raise<<" flash="<<weapon.flash<<" muzzle="<<weapon.muzzle.x<<','<<weapon.muzzle.y<<','<<weapon.muzzle.z<<'\n';if(!passed)return 21;
      renderer->render(scene);std::ofstream out(std::string("diagnostics/guard-combat-20261011/")+names[stage+1]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(commandArguments().find(L"--guard-inspection")){
     constexpr int width=1280,height=720;auto renderer=std::make_unique<retro::SoftwareRenderer>(width,height);if(!renderer->enableHardware())return 36;
     std::filesystem::create_directories("diagnostics/guard-pass-20261010");std::ofstream report("diagnostics/guard-pass-20261010/playground-runtime.txt");report<<renderer->hardwareName()<<'\n';
     const char*names[]={"guard-patrol","guard-alert","guard-investigate"};
     for(int stage=0;stage<3;++stage){auto scene=retro::Game::guardInspection(stage);auto expected=stage==0?retro::GuardState::Patrol:stage==1?retro::GuardState::Alert:retro::GuardState::Investigate;
      bool passed=scene.npcActors().size()>=1&&scene.guardState(0)==expected;report<<names[stage]<<": "<<(passed?"PASS":"FAIL")<<" state="<<int(scene.guardState(0))<<" pose="<<scene.npcActors()[0].pose.position.x<<','<<scene.npcActors()[0].pose.position.y<<'\n';if(!passed)return 21;
      renderer->render(scene);std::ofstream out(std::string("diagnostics/guard-pass-20261010/")+names[stage]+".ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";
      for(int i=0;i<width*height;++i){auto p=renderer->pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }report<<"Actual custom pack: complete patrol with hidden player, recognition, distraction: PASS\n";return 0;
    }
    if(commandArguments().find(L"--renderer-window-inspection")){
     retro::Win32Window window(W,H,L"RawMetal / Renderer Inspection");if(!window.valid())return 1;
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!renderer.enableHardware(window.handle()))return 36;
     auto scene=commandArguments().find(L"--fixture")?retro::Game::mapInspection({4.5f,6.5f},retro::kPi*.5f,-14,0,false,0,true):retro::Game::mapInspection({6.8f,12},.25f,12,18,false,-13,true);
     std::filesystem::remove("window-inspection.ready");
     for(int frame=0;frame<720&&window.pump();++frame){renderer.render(scene);if(frame==30)std::ofstream("window-inspection.ready")<<reinterpret_cast<std::uintptr_t>(window.handle());Sleep(16);}
     return 0;
    }
    if(commandArguments().find(L"--barrel-inspection")){
     constexpr int inspectW=1280,inspectH=720;auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(inspectW,inspectH);auto& renderer=*rendererStorage;
     if(commandArguments().find(L"--vulkan")&&!renderer.enableHardware())return 36;
     for(const auto&phase:std::array<std::pair<float,const char*>,3>{{{.06f,"barrel-fireball"},{.25f,"barrel-flames"},{.70f,"barrel-smoke"}}}){
      auto scene=retro::Game::barrelInspection(phase.first);renderer.render(scene);
      std::ofstream out(std::string(phase.second)+".ppm",std::ios::binary);out<<"P6\n"<<inspectW<<' '<<inspectH<<"\n255\n";
      for(int i=0;i<inspectW*inspectH;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }
     return 0;
    }
    if(commandArguments().find(L"--shading-inspection")){
     constexpr int inspectW=1920,inspectH=1080;auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(inspectW,inspectH);auto& renderer=*rendererStorage;
     if(commandArguments().find(L"--vulkan")&&!renderer.enableHardware())return 36;
     struct View{retro::Vec2 position;float yaw,pitch;int level;float z;const char*name;};
     const View views[]={
      {{3.5f,5.35f},0,0,0,-999.f,"corridor-light"},
      {{3.5f,5.35f},retro::kPi,130.f,0,-999.f,"fixture-shaft"},
      {{4.5f,6.5f},retro::kPi*.5f,-14.f,0,0.f,"fixture-depth"},
      {{8.5f,9.f},0,0,5,-9.4f,"coolant-depth"},
      {{6.8f,12.f},.25f,12.f,18,-13.f,"warehouse-d-block"}
     };
     for(const auto&view:views){auto scene=retro::Game::mapInspection(view.position,view.yaw,view.pitch,view.level,false,view.z,true);renderer.render(scene);
      std::ofstream out(std::string(view.name)+".ppm",std::ios::binary);out<<"P6\n"<<inspectW<<' '<<inspectH<<"\n255\n";
      for(int i=0;i<inspectW*inspectH;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(commandArguments().find(L"--coast-inspection")){
     constexpr int testW=1280,testH=720;auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(testW,testH);auto& renderer=*rendererStorage;
     if(commandArguments().find(L"--vulkan")&&!renderer.enableHardware())return 36;
     struct View{int level;retro::Vec2 local;float yaw,pitch;const char*name;};
     const View views[]={
      {13,{8.f,12.f},0,-10.f,"coast-from-shelf"},
      {13,{12.f,12.f},0,-22.f,"coast-underwater-terrain"},
      {13,{19.f,12.f},retro::kPi,-8.f,"coast-from-water"},
      {7,{21.f,12.f},0,0.f,"coast-from-ashfall"},
       {13,{10.f,12.f},0.f,-8.f,"coast-towards-ocean"}};
     for(const auto&view:views){auto scene=retro::Game::mapInspection(view.local,view.yaw,view.pitch,view.level,false,view.local.x>18.f&&view.level==13?4.f:-999.f,true,retro::WorldId::Ashfall);renderer.render(scene);
      std::ofstream out(std::string(view.name)+".ppm",std::ios::binary);out<<"P6\n"<<testW<<' '<<testH<<"\n255\n";
      for(int i=0;i<testW*testH;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(commandArguments().find(L"--terrain-seam-inspection")){
     constexpr int testW=1920,testH=1080;auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(testW,testH);auto& renderer=*rendererStorage;
     if(commandArguments().find(L"--vulkan")&&!renderer.enableHardware())return 36;
     struct View{int level;retro::Vec2 local;float yaw;const char*name;};
     const View views[]={
      {0,{23.5f,12.f},0,"east-into-east-chunk"},{1,{.5f,12.f},retro::kPi,"west-into-west-chunk"},
      {1,{23.5f,12.f},0,"east-into-next-chunk"},{2,{.5f,12.f},retro::kPi,"west-into-previous-chunk"},
      {0,{12.f,23.5f},retro::kPi*.5f,"north-into-south-chunk"},{4,{12.f,.5f},-retro::kPi*.5f,"south-into-north-chunk"},
      {4,{23.5f,12.f},0,"row-east-into-east-chunk"},{5,{.5f,12.f},retro::kPi,"row-west-into-west-chunk"}};
     for(const auto&view:views){auto scene=retro::Game::mapInspection(view.local,view.yaw,0,view.level,false,-999,true,retro::WorldId::Ashfall);renderer.render(scene);
      std::ofstream out(std::string(view.name)+".ppm",std::ios::binary);out<<"P6\n"<<testW<<' '<<testH<<"\n255\n";
      for(int i=0;i<testW*testH;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return 0;
    }
    if(commandArguments().find(L"--world-isolation-test")||commandArguments().find(L"--ashfall-inspection")){
     if(commandArguments().find(L"--world-isolation-test"))return retro::Game::testWorldIsolation()?0:44;
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;
     if(commandArguments().find(L"--vulkan")&&!renderer.enableHardware())return 36;
     for(int level=0;level<retro::worldChunkCount(retro::WorldId::Ashfall);++level){
      auto spawn=retro::chunkDefinition(retro::WorldId::Ashfall,level).playerStart;
      auto scene=retro::Game::mapInspection(spawn,.6f,0,level,false,-999,true,retro::WorldId::Ashfall);renderer.render(scene);
      std::ofstream out("isolated-world-"+std::to_string(level)+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }
     return 0;
    }
    if(commandArguments().find(L"--water-wall-inspection")){
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!commandArguments().find(L"--software")&&!renderer.enableHardware())return 36;
     for(int view=0;view<10;++view){auto scene=view<8?retro::Game::mapInspection({8,6},view*retro::kPi*.25f,0,3,false,0,true):retro::Game::mapInspection({9,7.8f},retro::kPi*.5f,-55,5,false,-9,true);
      if(view==9)scene.update({},.05f);renderer.render(scene);
      std::ofstream out("water-wall-"+std::to_string(view)+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(commandArguments().find(L"--darkness-test-software"))return darknessTestMode(true)?0:48;
    if(commandArguments().find(L"--darkness-test"))return darknessTest()?0:48;
    if(commandArguments().find(L"--flashlight-test")){
     if(!retro::Game::testFlashlight())return 43;
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;
     if(!commandArguments().find(L"--software")&&!renderer.enableHardware())return 36;
     auto scene=retro::Game::mapInspection({6.5f,1.5f},1.4f,0,4,false,-9,true);
     std::vector<std::uint32_t> before;std::ofstream report("flashlight-test.txt");
     for(int on=0;on<2;++on){if(on){scene.giveQuestItem(retro::Game::Flashlight);retro::InputState key{};key.flashlight=true;scene.update(key,.01f);}
      for(int warm=0;warm<8;++warm)renderer.render(scene);
      auto start=std::chrono::steady_clock::now();for(int frame=0;frame<20;++frame)renderer.render(scene);
      report<<"light "<<on<<" ms/frame "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20<<'\n';
      std::ofstream out(on?"flashlight-on.ppm":"flashlight-off.ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
      if(!on)before.assign(renderer.pixels(),renderer.pixels()+W*H);
     }
     int brighter=0;for(int y=60;y<H-60;++y)for(int x=80;x<W-80;++x){int i=y*W+x;auto a=before[i],b=renderer.pixels()[i];if(int(b&255)+int((b>>8)&255)+int((b>>16)&255)>int(a&255)+int((a>>8)&255)+int((a>>16)&255)+12)++brighter;}
     report<<"Brighter scene pixels: "<<brighter<<'\n';return brighter>100?0:43;
    }
    if(commandArguments().find(L"--campaign-extension-test"))return retro::Game::testCampaignExtension()?0:46;
    if(commandArguments().find(L"--cable-layout-inspection"))return cableLayoutInspection(W,H);
    if(commandArguments().find(L"--cable-performance-test"))return retro::SoftwareRenderer::testCablePerformance()?0:35;
    if(commandArguments().find(L"--cable-window-performance-test"))return cableWindowPerformance();
    if(commandArguments().find(L"--freight-inspection"))return freightInspection(W,H);
    if(commandArguments().find(L"--freight-detail-inspection"))return freightDetailInspection(commandArguments().find(L"--native-capture")?1920:W,commandArguments().find(L"--native-capture")?1080:H);
    if(commandArguments().find(L"--freight-art-inspection"))return freightArtInspection(commandArguments().find(L"--native-capture")?1920:W,commandArguments().find(L"--native-capture")?1080:H);
    if(commandArguments().find(L"--freight-actor-inspection"))return freightActorInspection(W,H);
    if(commandArguments().find(L"--cable-locker-inspection"))return campaignInspection(W,H,false,true);
    if(commandArguments().find(L"--early-maps-inspection"))return earlyMapsInspection(960,540);
    if(commandArguments().find(L"--campaign-inspection"))return campaignInspection(W,H);
    if(commandArguments().find(L"--waste-locker-inspection"))return campaignInspection(W,H,true);
    if(commandArguments().find(L"--megamap-inspection")){
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!commandArguments().find(L"--software")&&!renderer.enableHardware())return 36;
     std::ofstream report("megamap-inspection.txt");bool ok=true;
     for(int level:{4,5}){retro::World w(level);for(auto&s:w.structures()){bool valid=s.bottom>=-9.01f&&s.top<=-4.79f;ok&=valid;report<<"map "<<level<<" structure "<<s.x1<<','<<s.y1<<" z "<<s.bottom<<".."<<s.top<<" in room "<<valid<<'\n';}
      bool blocks=level==4?!w.fits(8.9f,6,-9,1):!w.fits(17.35f,22,-9,1);ok&=blocks;report<<"collision present "<<blocks<<'\n';}
     // Exercise the real standing hull, step height, door swing and water bed.
     // A fixed -9 flood-fill incorrectly rejects raised decks and ramps.
     ok&=retro::Game::testServiceMaps();
     struct View{std::string name;int level;retro::Vec2 p;float yaw,pitch;};
     std::vector<View> views={{"gallery-entry",4,{6.5f,1.5f},1.4f,0},{"gallery-bays",4,{11.f,12.f},2.65f,-8},{"gallery-seam",4,{12.f,22.f},retro::kPi*.5f,0},{"coolant-seam",5,{12.f,2.f},-retro::kPi*.5f,0},{"coolant-pools",5,{12.f,6.5f},retro::kPi*.5f,-14},{"coolant-equipment",5,{18.f,12.f},.9f,-6},{"coolant-bulkhead",5,{19.5f,21.3f},retro::kPi*.5f,0}};
     for(int level:{4,5})for(int angle=0;angle<8;++angle)views.push_back({std::string(level==4?"gallery":"coolant")+"-sweep-"+std::to_string(angle),level,{12,12},angle*retro::kPi*.25f,-8});
     views.push_back({"gallery-workbay",4,{6.5f,9.5f},-2.1f,-6});
     views.push_back({"coolant-riser",5,{19.f,20.f},-.8f,0});
     for(auto v:views){
      auto scene=retro::Game::mapInspection(v.p,v.yaw,v.pitch,v.level,false,-9,true);renderer.render(scene);
      auto start=std::chrono::steady_clock::now();for(int frame=0;frame<12;++frame)renderer.render(scene);
      report<<v.name<<" warm render ms/frame "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/12<<'\n';
      std::ofstream out(v.name+".ppm",std::ios::binary);out<<"P6\n"<<W<<' '<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }
     return ok&&retro::Game::testStreaming()?0:42;
    }
    if(commandArguments().find(L"--hazmat-test")){
     bool passed=retro::Game::testHazmat();auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;renderer.enableHardware();
     for(int view=0;view<3;++view){auto scene=retro::Game::hazmatInspection(view);renderer.render(scene);std::ofstream out("hazmat-"+std::to_string(view)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     return passed?0:41;
    }
    if(commandArguments().find(L"--stalker-test")){
     if(!retro::SoftwareRenderer::testCreatureAnimation()||!retro::Game::testAI())return 40;
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!renderer.enableHardware())return 36;
     for(int view=0;view<2;++view)for(int clip=0;clip<5;++clip)for(int frame=0;frame<5;++frame){auto scene=retro::Game::stalkerInspection(clip,.01f+frame*.245f,view);renderer.render(scene);
      std::ofstream out(std::string(view?"stalker-side-":"stalker-")+std::to_string(clip)+"-"+std::to_string(frame)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(auto arg=commandArguments().find(L"--creature-inspection=")){
     // Any kind's five clips at four phases, front and side: creature-K-clip-frame[-side].ppm.
     const int kind=_wtoi(arg+22);if(kind<0||kind>int(retro::CreatureKind::Chimera))return 1;
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!renderer.enableHardware())return 36;
     for(int view=0;view<2;++view)for(int clip=0;clip<5;++clip)for(int frame=0;frame<4;++frame){
      auto scene=retro::Game::stalkerInspection(clip,.05f+frame*.3f,view,retro::Enemy::Kind(kind));renderer.render(scene);
      std::ofstream out("creature-"+std::to_string(kind)+"-"+std::to_string(clip)+"-"+std::to_string(frame)+(view?"-side":"")+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
      for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(auto arg=commandArguments().find(L"--creature-video=")){
     // Every clip of one kind on a single 30 fps timeline, front and side, at
     // the timing the game plays it: creature-video/K-front-NNNN.ppm and
     // K-side-NNNN.ppm, with creature-video/K-timeline.txt naming each frame's clip.
     const int kind=_wtoi(arg+17);if(kind<0||kind>int(retro::CreatureKind::Chimera))return 1;
     const auto& traits=retro::creatureTraits(retro::CreatureKind(kind));
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!renderer.enableHardware())return 36;
     renderer.setEnvironmentInspection(true);std::filesystem::create_directories("creature-video");
     retro::Game views[2]={retro::Game::creaturePreview(retro::Enemy::Kind(kind),0),retro::Game::creaturePreview(retro::Enemy::Kind(kind),1)};
     struct Segment{const char* name;int clip;float seconds;};
     const float attack=traits.windup+.25f;
     const float pain=1.f/traits.painDecay;
     const Segment timeline[]={{"IDLE",0,traits.idleSeconds*1.5f},{"MOVE",1,2.5f},{"ATTACK",2,attack},{"ATTACK",2,attack},{"PAIN",3,pain},{"PAIN",3,pain},{"DEATH",4,traits.deathSeconds+.8f}};
     std::ofstream names("creature-video/"+std::to_string(kind)+"-timeline.txt");int frame=0;
     for(const auto& s:timeline)for(int f=0;f<int(s.seconds*30);++f,++frame){const float t=f/30.f;float phase=0;
      if(s.clip==0)phase=std::fmod(t/traits.idleSeconds,1.f);
      else if(s.clip==1)phase=std::fmod(t*traits.speed*traits.gaitPerMetre/(2*retro::kPi),1.f);
      else if(s.clip==2)phase=t<traits.windup?.4f*t/traits.windup:std::min(1.f,.4f+.6f*(t-traits.windup)/.25f);
      else if(s.clip==3)phase=std::min(1.f,t/pain);
      else phase=std::min(1.f,t/traits.deathSeconds);
      names<<frame<<' '<<s.name<<'\n';
      for(int v=0;v<2;++v){views[v].posePreviewCreature(s.clip,phase);renderer.render(views[v]);char file[64];std::snprintf(file,sizeof(file),"creature-video/%d-%s-%04d.ppm",kind,v?"side":"front",frame);
       std::ofstream out(file,std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
     }
     return 0;
    }
    if(commandArguments().find(L"--hearing-test"))return retro::Game::testHearing()?0:21;
    if(commandArguments().find(L"--guard-ai-test"))return retro::Game::testGuardAI()?0:21;
    if(commandArguments().find(L"--friendly-ai-test"))return retro::Game::testFriendlyAI()?0:21;
    if(commandArguments().find(L"--physics-ai-test"))return !retro::Game::testMovement()?19:!retro::Game::testAI()?21:!retro::Game::testClutter()?22:0;
    // --terminal-inspection=LEVEL,TITLE: the first terminal or paper on that map
    // whose title contains TITLE, rendered open as the player reads it.
    if(auto arg=commandArguments().find(L"--terminal-inspection=")){
     const std::wstring value(arg+22);const auto comma=value.find(L',');if(comma==std::wstring::npos)return 2;
     const int level=_wtoi(value.substr(0,comma).c_str());std::string title;for(auto c:value.substr(comma+1))title+=char(c);
     auto probe=retro::Game::mapInspection({12,12},0,0,level,false,-999,false);int index=-1;
     for(size_t i=0;i<probe.world().terminals().size()&&index<0;++i)if(std::string(probe.world().terminals()[i].title).find(title)!=std::string::npos)index=int(i);
     if(index<0)return 2;
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!renderer.enableHardware())return 36;
     auto scene=retro::Game::terminalInspection(level,index);renderer.render(scene);
     std::filesystem::create_directories(".build/shot");std::ofstream out(".build/shot/terminal-"+std::to_string(level)+"-"+std::to_string(index)+".ppm",std::ios::binary);
     out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}return 0;
    }
    if(commandArguments().find(L"--freight-repair-inspection")){
     auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!renderer.enableHardware())return 36;
     const char* names[]={"repair-van","repair-machinery","repair-cage","repair-intake-header","repair-terminal-warehouse","repair-terminal-legacy","repair-van-left","repair-van-right"};
     for(int view=0;view<8;++view){auto scene=view==0?retro::Game::mapInspection({15,3},.8f,-3,14,true,-25,true):view==1?retro::Game::mapInspection({12,3},retro::kPi*.5f,-8,11,true,-12,true):view==2?retro::Game::mapInspection({12,7},retro::kPi*.5f,-15,12,true,-12,true):view==3?retro::Game::mapInspection({12,21},retro::kPi*.5f,55,15,true,-25,true):view==6?retro::Game::mapInspection({15.5f,8},0,-12,14,true,-25,true):view==7?retro::Game::mapInspection({22.5f,8},retro::kPi,-12,14,true,-25,true):retro::Game::terminalInspection(view==4?16:0);
      renderer.render(scene);std::ofstream out(std::string(names[view])+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
     }return 0;
    }
    if(commandArguments().find(L"--mutant-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!renderer.enableHardware())return 49;
        const char* names[]={"mutant-baseline","mutant-walk","mutant-lunge","mutant-death","mutant-warehouse","mutant-depot","mutant-death-baseline"};
        for(int view=0;view<7;++view){auto scene=view==0?retro::Game::mapInspection({3.5f,4.5f},0,0,0,false,0,true):view==3?retro::Game::stalkerInspection(4,.87f,0,retro::Enemy::Kind::Mutant):view==4?retro::Game::mapInspection({12,14.5f},-retro::kPi*.5f,-3,17,true,-25,false):view==5?retro::Game::mapInspection({17.3f,19},0,0,27,true,-25,false):view==6?retro::Game::mapInspection({18.6f,18.5f},0,-35,3,true,-9,true):retro::Game::validationScene(retro::Enemy::Kind::Mutant,-1.f,view==2?.22f:0.f);
         renderer.render(scene);std::ofstream out(std::string(names[view])+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
         for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
        }return 0;
    }
    if(commandArguments().find(L"--warden-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;renderer.enableHardware();
        for(int view=0;view<2;++view){
         auto scene=view==0?retro::Game::validationScene(retro::Enemy::Kind::Warden,-1,.9f):retro::Game::mapInspection({19.2f,18.5f},0,0,3,true,-9,false);renderer.render(scene);
         std::ofstream out(view==0?"warden.ppm":"warden-reactor.ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
         for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
        }return 0;
    }
    if(commandArguments().find(L"--lift-test"))return retro::Game::testLift()?0:32;
    if(commandArguments().find(L"--reactor-test"))return retro::Game::testReactor()?0:37;
    if(commandArguments().find(L"--save-test"))return retro::Game::testSaves()?0:38;
    if(commandArguments().find(L"--npc-voice-test"))return retro::Game::testNpcVoices()?0:13;
    if(commandArguments().find(L"--shaft-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!commandArguments().find(L"--software"))renderer.enableHardware();int index=0;
        for(float seconds:{0.f,9.f,17.f,25.f,29.f,35.f,38.7f,39.6f,42.f,48.f}){auto scene=retro::Game::liftInspection(seconds,8);renderer.render(scene);std::ofstream out("shaft-"+std::to_string(index++)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
        return 0;
    }
    if(commandArguments().find(L"--plant-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!commandArguments().find(L"--software"))renderer.enableHardware();
        auto save=[&](const char* name,const retro::Game& scene){renderer.render(scene);std::ofstream out(name,std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}};
        save("plant-feed.ppm",retro::Game::mapInspection({6.3f,16.95f},retro::kPi*.5f,5,3,false,-9,true));
        save("plant-return.ppm",retro::Game::liftInspection(48,7));
        save("plant-arrival.ppm",retro::Game::mapInspection({10.2f,6.8f},1.2f,0,3,false,0,true));
        save("plant-freight.ppm",retro::Game::mapInspection({3.1f,8.15f},0,-10,3,false,0,true));
        save("plant-freight-low.ppm",retro::Game::mapInspection({3.4f,7.1f},.55f,-35,3,false,0,true));
        save("plant-winch.ppm",retro::Game::mapInspection({15.3f,9.4f},.7f,0,3,false,0,true));
        save("plant-cab.ppm",retro::Game::liftInspection(0,8));return 0;
    }
    if(commandArguments().find(L"--repair-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!commandArguments().find(L"--software"))renderer.enableHardware();
        auto save=[&](const char* name,const retro::Game& scene){renderer.render(scene);std::ofstream out(name,std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}};
        save("repair-wall.ppm",retro::Game::mapInspection({17,20},0,0,3,false,-9,true));
        save("repair-stairs.ppm",retro::Game::mapInspection({21.8f,11.5f},2.04f,15,3,false,-9,true));
        save("repair-stairs-top.ppm",retro::Game::mapInspection({21.8f,18.5f},-2.1f,-35,3,false,-6,true));
        save("repair-sign.ppm",retro::Game::liftInspection(0,3));
        auto menu=retro::Game::liftInspection(retro::World::LiftRideComplete);menu.setSaveDirectory(L"inspection-empty-saves");retro::InputState input{};input.escape=true;menu.update(input,.01f);save("repair-pause.ppm",menu);
        auto click=[&](int row){menu.update({},.01f);retro::InputState i{};i.fire=true;i.pointerX=retro::MenuLayout::X+30;i.pointerY=retro::MenuLayout::RowTop+row*retro::MenuLayout::RowHeight+7;menu.update(i,.01f);};
        click(7);save("repair-save.ppm",menu);click(3);click(8);save("repair-load.ppm",menu);click(0);save("repair-confirm.ppm",menu);return 0;
    }
    if(commandArguments().find(L"--console-test"))return retro::Game::testConsole()?0:34;
    if(commandArguments().find(L"--controls-test"))return retro::Game::testCombat()&&retro::Game::testSettings()?0:39;
    if(commandArguments().find(L"--presentation-resize-test"))return retro::SoftwareRenderer::testPresentationResize()?0:36;
    if(commandArguments().find(L"--gpu-actor-test"))return retro::SoftwareRenderer::testGpuActors()?0:35;
    if(commandArguments().find(L"--gpu-actor-performance"))return gpuActorPerformance();
    if(commandArguments().find(L"--visibility-test"))return retro::SoftwareRenderer::testVisibility()?0:35;
    if(commandArguments().find(L"--performance-test"))return retro::SoftwareRenderer::testPerformance()?0:35;
    if(commandArguments().find(L"--lab-profile"))return ashfallPerformance(true,true,false,false,false,true);
    if(commandArguments().find(L"--lab-performance"))return ashfallPerformance(true,true,commandArguments().find(L"--cpu-actor-preparation")!=nullptr,commandArguments().find(L"--uncached-ai-queries")!=nullptr,commandArguments().find(L"--full-rate-ai-decisions")!=nullptr);
    if(commandArguments().find(L"--approach-test"))return approachTest();
    if(commandArguments().find(L"--ashfall-performance")||commandArguments().find(L"--ashfall-performance-yard"))return ashfallPerformance(commandArguments().find(L"--ashfall-performance-yard")!=nullptr,false,commandArguments().find(L"--cpu-actor-preparation")!=nullptr,commandArguments().find(L"--uncached-ai-queries")!=nullptr,commandArguments().find(L"--full-rate-ai-decisions")!=nullptr);
    if(commandArguments().find(L"--performance-window")||commandArguments().find(L"--service-performance-window")||commandArguments().find(L"--door-performance-window")||commandArguments().find(L"--freight-performance-window"))return windowPerformance(commandLine,W,H);
    if(commandArguments().find(L"--vulkan-test"))return retro::SoftwareRenderer::testHardware()?0:36;
    if(commandArguments().find(L"--cleanup-split-test"))return retro::Mesh::testCleanupTankAim()?0:47;
    if(commandArguments().find(L"--lift-audio-test"))return retro::AudioEngine::testLiftMix()?0:33;
    if(commandArguments().find(L"--lift-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;if(!commandArguments().find(L"--software"))renderer.enableHardware();int index=0;
        for(float seconds:{0.f,7.f,21.f,34.f,48.f,48.f,48.f,48.f,48.f,48.f,48.f}){auto scene=retro::Game::liftInspection(seconds,index>=7?index-3:index>=5?index-4:index==1?3:0);renderer.render(scene);
            std::ofstream out("lift-"+std::to_string(index++)+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";
            for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}}
        return 0;
    }
    if(commandArguments().find(L"--audio-test"))return retro::AudioEngine::test()?0:13;
    if(commandArguments().find(L"--audio-device-test"))return retro::AudioEngine::testDevice()?0:15;
    if(commandArguments().find(L"--environment-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;
        auto save=[&](const std::string&name){std::ofstream out(name+".ppm",std::ios::binary);out<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}};
        const retro::Vec2 centers[]={{16.f,2.5f},{4.f,10.5f},{11.5f,11.f},{11.f,2.5f}};
        for(int target=0;target<4;++target)for(int side=0;side<4;++side){float a=side*retro::kPi*.5f+.3f;auto center=centers[target];auto position=center+retro::Vec2{std::cos(a),std::sin(a)}*2.8f;
            auto scene=retro::Game::mapInspection(position,a+retro::kPi,-30,target<2?0:target-1,true,0,true);renderer.render(scene);save("machine-"+std::to_string(target)+"-"+std::to_string(side));}
        for(int side=0;side<2;++side){auto scene=retro::Game::mapInspection({4.5f,side?10.5f:6.5f},side?-retro::kPi*.5f:retro::kPi*.5f,0,0,false,0,true);renderer.render(scene);save("square-door-"+std::to_string(side));}
        retro::World world;bool inspected[4]{};const retro::Vec2 normals[]={{-1,0},{1,0},{0,-1},{0,1}};
        for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(world.tile(x,y)=='#'&&(x*3+y)%9==0)for(int side=0;side<4;++side){auto n=normals[side];if(inspected[side]||world.tile(x+int(n.x),y+int(n.y))=='#')continue;
            auto face=retro::Vec2{x+.5f,y+.5f}+n*.5f;if(!world.wallSpaceFree(face,{-n.y,n.x},.68f,.65f,1.33f))continue;auto position=face+n*1.4f+retro::Vec2{-n.y,n.x}*.25f;if(!world.fits(position.x,position.y,0,1))continue;
            auto delta=face-position;auto scene=retro::Game::mapInspection(position,std::atan2(delta.y,delta.x),20,0,false,0,true);renderer.render(scene);save("wall-vent-"+std::to_string(side));inspected[side]=true;}
        for(int level=0;level<3;++level){retro::World map(level);int count=0;for(auto&light:map.lights()){
            auto position=light.position+retro::Vec2{0,1.4f};float feet=map.floorHeight(position.x,position.y);if(!map.fits(position.x,position.y,feet,1))continue;
            auto scene=retro::Game::mapInspection(position,-retro::kPi*.5f,std::atan2(light.z-feet-.78f,1.4f)*140,level,false,feet,true);renderer.render(scene);save("ceiling-light-"+std::to_string(level)+"-"+std::to_string(count));if(++count==2)break;}}
        return 0;
    }
    if(commandArguments().find(L"--service-inspection")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;
        if(commandArguments().find(L"--vulkan")&&!renderer.enableHardware())return 36;
        struct View{int level;retro::Vec2 position;float angle;bool openDoors=false;};
        const View views[]={
            {4,{6.5f,2.5f},1.0f},{4,{12.f,7.f},1.57f},{4,{12.f,16.f},0.f},
            {5,{12.f,2.5f},1.57f},{5,{11.5f,9.f},0.f},{5,{12.f,15.f},0.f},
            {5,{19.75f,19.2f},1.57f},{5,{19.75f,19.2f},1.57f,true}};
        for(int i=0;i<int(std::size(views));++i){const auto&v=views[i];
            auto scene=retro::Game::mapInspection(v.position,v.angle,0,v.level,v.openDoors,-9,true);
            renderer.render(scene);std::ofstream out("service-"+std::to_string(i)+".ppm",std::ios::binary);
            out<<"P6\n"<<W<<" "<<H<<"\n255\n";
            for(int p=0;p<W*H;++p){auto color=renderer.pixels()[p];char rgb[]={char(color>>16),char(color>>8),char(color)};out.write(rgb,3);}
        }
        return 0;
    }
    if(commandArguments().find(L"--chapter-sequence-test"))return retro::Game::testChapterSequences()?0:63;
    if(commandArguments().find(L"--chapter-route-test"))return retro::Game::testChapterRoute()?0:62;
    if(commandArguments().find(L"--service-map-test"))return retro::Game::testServiceMaps()?0:45;
    if(commandArguments().find(L"--mechanisms-test"))return retro::Game::testMechanisms()?0:39;
    if(commandArguments().find(L"--campaign-map-test")||commandArguments().find(L"--freight-district-test"))return retro::Game::testCampaignMaps()?0:37;
    if(commandArguments().find(L"--streaming-test"))return retro::Game::testStreaming()?0:26;
    if(commandArguments().find(L"--render-benchmark")){
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;std::ofstream report("render-benchmark.txt");
        const retro::Vec2 positions[]={{3.5f,4.5f},{7.5f,12.5f},{20.5f,11.5f}};
        for(auto position:positions){auto scene=retro::Game::mapInspection(position,0);renderer.render(scene);auto start=std::chrono::steady_clock::now();
            for(int i=0;i<30;++i)renderer.render(scene);
            auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/30;
            report<<position.x<<", "<<position.y<<": "<<ms<<" ms/frame (render only)\n";
        }return 0;
    }
    if(commandArguments().find(L"--command-arguments-test"))return commandArgumentsTest()?0:21;
    if(commandArguments().find(L"--content-integrity-test"))return contentIntegrityTest()?0:21;
    if(commandArguments().find(L"--load-validation-test"))return loadValidationTest()?0:21;
    if(commandArguments().find(L"--each-test")){
        // The smoke chain's game tests, every one run independently: a failure
        // early in the chain must not hide the result of everything after it.
        const std::pair<const char*,bool(*)()> tests[]={
         {"projection",retro::SoftwareRenderer::testProjection},{"command-arguments",commandArgumentsTest},{"content-integrity",contentIntegrityTest},{"load-validation",loadValidationTest},
         {"hazmat",retro::Game::testHazmat},{"creature-animation",retro::SoftwareRenderer::testCreatureAnimation},{"cleanup-tank-aim",retro::Mesh::testCleanupTankAim},
         {"combat",retro::Game::testCombat},{"weapon-motion",retro::Game::testWeaponMotion},{"audio-events",retro::Game::testAudioEvents},
         {"settings",retro::Game::testSettings},{"inventory",retro::Game::testInventory},{"flashlight",retro::Game::testFlashlight},
         {"radar",retro::Game::testRadar},
         {"pickups",retro::Game::testPickups},{"movement",retro::Game::testMovement},{"progression",retro::Game::testProgression},
         {"ai",retro::Game::testAI},{"creature-behaviour",retro::Game::testCreatureBehaviour},{"gantry",retro::Game::testGantry},{"lift",retro::Game::testLift},{"reactor",retro::Game::testReactor},
         {"engine-regression",retro::Game::testEngineRegression},{"saves",retro::Game::testSaves},{"systems",retro::Game::testSystems},{"console",retro::Game::testConsole},
         {"lift-mix",retro::AudioEngine::testLiftMix},{"clutter",retro::Game::testClutter},{"service-maps",retro::Game::testServiceMaps},
         {"streaming",retro::Game::testStreaming},{"unarmed",retro::Game::testUnarmed},{"npc-voices",retro::Game::testNpcVoices},
         {"mechanisms",retro::Game::testMechanisms},{"campaign-maps",retro::Game::testCampaignMaps},{"crates",retro::Game::testCrates},
         {"chapter-route",retro::Game::testChapterRoute},{"chapter-sequence",retro::Game::testChapterSequences},{"darkness",darknessTest},
         {"caution",retro::Game::testCaution},
         {"lockers",retro::Game::testLockers}};
        std::ofstream report("each-test.txt");int failed=0;
        const wchar_t* only=commandArguments().find(L"--each-test=");
        for(const auto& [name,run]:tests){
         if(only){std::wstring wanted(only+12);if(std::wstring(name,name+std::strlen(name))!=wanted)continue;}
         auto start=std::chrono::steady_clock::now();bool ok=false;
         try{ok=run();}catch(const std::exception& e){report<<name<<" threw "<<e.what()<<'\n';}
         failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<name<<" "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"s\n";report.flush();
        }
        report<<(failed?"FAILED ":"ALL PASSED ")<<failed<<'\n';std::cout<<(failed?"FAILED ":"ALL PASSED ")<<failed<<'\n';return failed?1:0;
    }
    if(commandArguments().find(L"--smoke-test")){
        retro::Game game;
        game.loadCustomCampaignDirectory((std::filesystem::current_path()/L"custom maps").wstring());
        auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;
        if(commandArguments().find(L"--vulkan")&&!renderer.enableHardware())return 36;
        std::ofstream("model-report.txt")<<renderer.modelReport();
        if(!renderer.validate3D())return 7;
        if(!retro::Game::testHazmat())return 41;
        if(!retro::SoftwareRenderer::testCreatureAnimation())return 40;
        if(!retro::Mesh::testCleanupTankAim())return 47;
        if(!retro::Game::testCombat())return 9;
        if(!retro::Game::testWeaponMotion())return 11;
        if(!retro::Game::testAudioEvents())return 12;
        if(!retro::Game::testSettings())return 16;
        if(!retro::Game::testInventory())return 31;
        if(!retro::Game::testFlashlight())return 43;
        if(!retro::Game::testPickups())return 18;
        if(!retro::Game::testMovement())return 19;
        if(!retro::Game::testProgression())return 20;
        if(!retro::Game::testAI())return 21;
        if(!retro::Game::testGantry())return 24;
        if(!retro::Game::testLift())return 32;
        if(!retro::Game::testReactor())return 37;
        if(!retro::Game::testSaves())return 38;
        if(!retro::Game::testSystems())return 42;
        if(!retro::Game::testConsole())return 34;
        if(!retro::AudioEngine::testLiftMix())return 33;
        if(!retro::Game::testClutter())return 25;
        if(!retro::Game::testServiceMaps())return 45;
        if(!retro::Game::testStreaming())return 26;
        if(!retro::Game::testUnarmed())return 22;
        {auto wide=retro::Win32Window::viewport(1280,600),normal=retro::Win32Window::viewport(960,540);
         if(wide.left!=320||wide.top!=120||wide.right-wide.left!=640||wide.bottom-wide.top!=360||normal.left!=160||normal.top!=90||normal.right!=800||normal.bottom!=450)return 17;
        }
        if(!retro::AudioEngine::test())return 13;
        if(!retro::Game::testNpcVoices())return 13;
        for(int model=0;model<27;++model){
            renderer.previewModel(model,1.1f);
            std::ofstream preview("model-"+std::to_string(model)+".ppm",std::ios::binary);preview<<"P6\n"<<W<<" "<<H<<"\n255\n";
            for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};preview.write(rgb,3);}
        }
        {
            retro::Game animation=retro::Game::validationScene(retro::Enemy::Kind::Huntsman,3);
            float maxError=0;
            for(int frame=0;frame<150;++frame){retro::InputState motion{};
                motion.mouseDx=frame<80?std::sin(frame*.17f)*80.f:0;
                motion.mouseDy=frame<80?std::cos(frame*.13f)*40.f:0;
                motion.fire=frame==35;motion.jump=frame==65;
                animation.update(motion,1.f/60.f);renderer.render(animation);maxError=std::max(maxError,renderer.gripError());
                if(frame==36||frame==48||frame==80){std::ofstream shot("weapon-motion-"+std::to_string(frame)+".ppm",std::ios::binary);shot<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};shot.write(rgb,3);}}
            }
            std::ofstream("attachment-test.txt")<<"150 frames: recoil, sway, jump and settle\nMaximum wrist-to-socket error: "<<maxError<<" metres\n";
            if(maxError>.025f)return 10;
        }
        bool reachable[24][24]{};
        std::queue<std::pair<int,int>> pending;
        pending.push({3,4});reachable[4][3]=true;
        while(!pending.empty()){
            auto [x,y]=pending.front();pending.pop();
            const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
            for(int d=0;d<4;++d){int nx=x+dx[d],ny=y+dy[d];
                if(nx>=0&&ny>=0&&nx<24&&ny<24&&!reachable[ny][nx]&&!game.world().solid(nx+.5f,ny+.5f)){
                    reachable[ny][nx]=true;pending.push({nx,ny});
                }
            }
        }
        auto reached=[&](retro::Vec2 p){int x=int(p.x),y=int(p.y);return x>=0&&y>=0&&x<24&&y<24&&reachable[y][x];};
        for(const auto& e:game.enemies())if(!reached(e.pos))return 3;
        for(const auto& p:game.pickups())if(!reached(p.pos))return 4;
        if(!reachable[22][21])return 5;
        int walkable=0;for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(!game.world().solid(x+.5f,y+.5f)){if(!reachable[y][x]){std::ofstream("map-test.txt")<<"Isolated open tile "<<x<<","<<y;return 14;}++walkable;}
        std::ofstream("map-test.txt")<<"All "<<walkable<<" walkable tiles connected; every enemy, pickup and extraction reachable.\n";
        retro::InputState input{};
        for(int frame=0;frame<120;++frame)game.update(input,1.f/60.f);
        renderer.render(game);
        std::ofstream out("smoke-frame.ppm",std::ios::binary);
        out<<"P6\n"<<W<<" "<<H<<"\n255\n";
        for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
        auto saveFrame=[&](const char* name){std::ofstream frame(name,std::ios::binary);frame<<"P6\n"<<W<<" "<<H<<"\n255\n";for(int i=0;i<W*H;++i){auto p=renderer.pixels()[i];char rgb[]={char(p>>16),char(p>>8),char(p)};frame.write(rgb,3);}};
        {auto impact=retro::Game::mapInspection({3.5f,6.5f},retro::kPi,0,0,false,0,true);renderer.render(impact);saveFrame("impact-before.ppm");retro::InputState fire{};fire.fire=true;impact.update(fire,.02f);if(impact.bulletImpacts().empty())return 47;for(auto&mark:impact.bulletImpacts())if(retro::length(mark.pos-impact.player().pos)<.3f)return 47;for(int i=0;i<24;++i)impact.update({},1.f/60.f);renderer.render(impact);saveFrame("impact-inspection.ppm");}
        {auto basin=retro::Game::mapInspection({8.5f,9.f},0,0,5,false,-9.4f,true);retro::InputState duck{};duck.crouch=true;for(int i=0;i<30;++i)basin.update(duck,1.f/60.f);renderer.render(basin);saveFrame("underwater-inspection.ppm");}
        {auto title=game;title.showTitleScreen();renderer.render(title);saveFrame("title-menu.ppm");}
        {auto inventory=game;retro::InputState open{};open.inventory=true;inventory.update(open,.01f);renderer.render(inventory);saveFrame("inventory-menu.ppm");}
        {auto console=game;retro::InputState consoleInput{};consoleInput.console=true;console.update(consoleInput,.01f);consoleInput={};consoleInput.textInput="maps\rmap reactor\r";console.update(consoleInput,.01f);renderer.render(console);saveFrame("developer-console.ppm");}
        for(int kind=0;kind<7;++kind)for(int stage=0;stage<3;++stage){auto scene=retro::Game::clutterInspection(kind,stage==0?0:stage==1?.3f:5.f);renderer.render(scene);saveFrame(("clutter-tumble-"+std::to_string(kind)+"-"+std::to_string(stage)+".ppm").c_str());}
        {const retro::Vec2 positions[]={{7.5f,4.5f},{4.8f,17.4f},{6.5f,20.5f},{6.5f,3.5f},{16.5f,11.5f},{17.5f,9.5f},{21.5f,20.5f}};
           const float yaw[]={2.3f,1.570796f,-.7f,.2f,-1.5f,0,1.570796f},pitch[]={50,45,-45,-45,-30,-90,15};
         for(int i=0;i<7;++i){auto scene=retro::Game::mapInspection(positions[i],yaw[i],pitch[i],2,true,i>=2&&i<=5?3.f:0.f);renderer.render(scene);saveFrame(("gantry-view-"+std::to_string(i)+".ppm").c_str());}
          auto lift=retro::Game::mapInspection({11.3f,3.5f},1.570796f);retro::InputState use{};use.use=true;lift.update(use,.01f);renderer.render(lift);saveFrame("clutter-held.ppm");
        }
        {std::ofstream placement("placement-test.txt");
         for(int level=0;level<2;++level){retro::World world(level);
          for(auto& prop:world.props()){
           float base=world.floorHeight(prop.position.x,prop.position.y);
           for(float dx:{-prop.halfSize.x,0.f,prop.halfSize.x})for(float dy:{-prop.halfSize.y,0.f,prop.halfSize.y}){
            float x=prop.position.x+dx,y=prop.position.y+dy;
            if(std::fabs(world.floorHeight(x,y)-base)>.005f||world.tile(int(x),int(y))=='#'){placement<<"Unsupported prop "<<prop.kind<<" in chunk "<<level;return 23;}
           }
          }
         }placement<<"All machinery footprints rest on continuous floor; no wall intersections: PASS\n";
         const retro::Vec2 positions[]={{20.5f,20.f},{22.5f,21.7f},{3.5f,1.5f},{19.5f,8.2f},{18.5f,10.6f},{22.f,12.f},{8.5f,12.5f}};
         const float yaw[]={1.25f,2.24f,-1.570796f,1.3f,0,-2.53f,3.14159f};
         const float pitch[]={65,115,75,40,10,-15,-35};
         for(int i=0;i<7;++i){auto scene=retro::Game::mapInspection(positions[i],yaw[i],pitch[i],i<2?0:1,true);renderer.render(scene);saveFrame(("placement-"+std::to_string(i)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{3.5f,1.5f},{9.f,9.f},{16.5f,13.f},{20.5f,12.f},{15.f,20.5f}};
         const float angles[]={.7f,.7f,-.3f,2.8f,.15f};
         for(int i=0;i<5;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i],-12,1);renderer.render(scene);saveFrame(("pressureworks-"+std::to_string(i)+".ppm").c_str());}
         auto outward=retro::Game::mapInspection({21.5f,22.8f},1.570796f,0,0,true);renderer.render(outward);saveFrame("chunk-seam-out.ppm");
         auto returning=retro::Game::mapInspection({3.5f,1.3f},-1.570796f,0,1,true);renderer.render(returning);saveFrame("chunk-seam-back.ppm");
        }
        {auto menuGame=game;retro::InputState menuInput{};menuInput.escape=true;menuGame.update(menuInput,.02f);renderer.render(menuGame);saveFrame("settings-menu.ppm");
         retro::InputState drag{};drag.fire=true;drag.pointerX=retro::MenuLayout::SliderX+15;drag.pointerY=retro::MenuLayout::RowTop+retro::MenuLayout::RowHeight+9;menuGame.update(drag,.02f);renderer.render(menuGame);saveFrame("slider-low.ppm");
         drag.pointerX=retro::MenuLayout::SliderX+60;drag.pointerY+=50;menuGame.update(drag,.02f);renderer.render(menuGame);saveFrame("slider-high.ppm");
        }
        {for(int mode:{1,2,3})for(int stage=0;stage<4;++stage){auto pose=retro::Game::weaponInspection(mode,mode==2?stage*.028f:stage==0?10.f:stage*.12f);renderer.render(pose);saveFrame(("weapon-pass-"+std::to_string(mode)+"-"+std::to_string(stage)+".ppm").c_str());}
         auto fists=retro::Game::weaponInspection(1,.16f);for(int angle=0;angle<4;++angle){renderer.inspectRig(fists,angle*1.570796f,.25f);saveFrame(("fist-rig-"+std::to_string(angle)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{2.7f,6.8f},{2.7f,10.2f},{19.f,7.f},{18.7f,10.5f},{11.f,15.f},{9.7f,17.5f}};const float yaw[]={1.55f,-1.55f,1.35f,-1.1f,1.6f,-.75f};
         for(int i=0;i<6;++i){auto scene=retro::Game::mapInspection(positions[i],yaw[i],18);renderer.render(scene);saveFrame(("door-inspection-"+std::to_string(i)+".ppm").c_str());}
        }
        {auto health=retro::Game::mapInspection({12.f,5.5f},0,-40);renderer.render(health);saveFrame("pickup-health.ppm");
         auto ammo=retro::Game::mapInspection({4.5f,6.2f},1.570796f,-45);renderer.render(ammo);saveFrame("pickup-ammo.ppm");}
        {const retro::Vec2 positions[]={{4.5f,5.3f},{5.8f,5.4f},{4.5f,1.5f},{7.5f,6.2f},{4.5f,7.2f},{21.5f,21.f}};
         const float angles[]={1.570796f,2.062f,1.570796f,1.570796f,1.570796f,1.570796f},pitch[]={50,45,20,28,-75,35};
         for(int i=0;i<6;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i],pitch[i]);renderer.render(scene);saveFrame(("signage-"+std::to_string(i)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{3.5f,4.5f},{7.5f,12.5f},{20.5f,17.5f},{4.5f,6.5f}};const float angles[]={0,0,1.570796f,1.570796f};
         for(int i=0;i<4;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i]);renderer.render(scene);saveFrame(("map-"+std::to_string(i)+".ppm").c_str());}
        }
        {const retro::Vec2 positions[]={{20.5f,15.7f},{20.5f,11.5f},{9.3f,14.5f},{13.5f,14.5f},{4.5f,7.f}};
         const float angles[]={-1.570796f,1.570796f,0,-1.570796f,1.570796f},pitch[]={0,-20,0,35,20};
         for(int i=0;i<5;++i){auto scene=retro::Game::mapInspection(positions[i],angles[i],pitch[i]);renderer.render(scene);saveFrame(("client-map-"+std::to_string(i)+".ppm").c_str());
          if(i==4){retro::InputState use{};use.use=true;scene.update(use,.02f);use.use=false;for(int frame=0;frame<150;++frame)scene.update(use,1.f/120.f);renderer.render(scene);saveFrame("bulkhead-open.ppm");}
         }
         auto log=retro::Game::mapInspection({2.2f,4.2f},1.570796f);retro::InputState use{};use.use=true;log.update(use,.02f);renderer.render(log);saveFrame("shift-log.ppm");
        }
        {auto rigScene=retro::Game::validationScene(retro::Enemy::Kind::Huntsman,3);
         for(int pose=0;pose<2;++pose){if(pose){retro::InputState trigger{};trigger.fire=true;rigScene.update(trigger,1.f/60.f);}
          const float yaw[]={0,1.570796f,-1.570796f,0,3.14159f,.7f},pitch[]={0,0,0,1.20f,0,.5f};
          for(int angle=0;angle<6;++angle){renderer.inspectRig(rigScene,yaw[angle],pitch[angle]);saveFrame(("rig-"+std::to_string(pose)+"-"+std::to_string(angle)+".ppm").c_str());}
         }
        }
        for(int kind=0;kind<3;++kind)for(int state=0;state<4;++state){auto scene=retro::Game::validationScene(static_cast<retro::Enemy::Kind>(kind),state==2?.35f:state==3?2.5f:-1.f,state==1?.4f:0);
            renderer.render(scene);saveFrame(("encounter-"+std::to_string(kind)+"-"+std::to_string(state)+".ppm").c_str());
        }
        {auto scene=retro::Game::mapInspection({3.5f,5.35f},0,0,0,false,-999,true);renderer.render(scene);saveFrame("beam-inspection.ppm");}
        input.fire=true;game.update(input,1.f/60.f);renderer.render(game);saveFrame("firing-frame.ppm");
        input.fire=false;input.mouseDy=-90;input.jump=true;game.update(input,1.f/60.f);input.mouseDy=0;input.jump=false;
        for(int i=0;i<16;++i)game.update(input,1.f/60.f);
        renderer.render(game);saveFrame("jump-look-frame.ppm");
        return out && game.player().health>0 ? 0 : 2;
    }
    if(commandArguments().find(L"--vr-inspection-test"))return vrSmokeTest(W,H,true);
    if(commandArguments().find(L"--vr-smoke-test"))return vrSmokeTest(W,H);
    if(commandArguments().find(L"--vr-hand-dump")){
     // The tracked-hand mesh open and closed, as OBJ files for offline inspection.
     retro::Mesh arms(111);int file=0;
     for(bool left:{false,true})for(float curl:{0.f,1.f}){std::ofstream obj(std::string("vr-hand-")+(left?"left":"right")+(curl>0?"-closed":"-open")+".obj");int n=0;
      for(const auto&t:arms.trackedHand(left,{curl,curl,curl,curl,curl})){for(const auto&v:t.v)obj<<"v "<<v.p.x<<' '<<v.p.y<<' '<<v.p.z<<'\n';obj<<"f "<<n+1<<' '<<n+2<<' '<<n+3<<'\n';n+=3;}++file;}
     {retro::Mesh weapon(109);std::ofstream obj("vr-weapon.obj");int n=0;for(const auto&t:weapon.triangles){for(const auto&v:t.v)obj<<"v "<<v.p.x<<' '<<v.p.y<<' '<<v.p.z<<'\n';obj<<"f "<<n+1<<' '<<n+2<<' '<<n+3<<'\n';n+=3;}}
     return file==4?0:1;}
    if(commandArguments().find(L"--vr-test")){bool passed=retro::VrRuntime::testMath()&&retro::Mesh::testVrHands()&&retro::Game::testVrMelee()&&retro::Game::testVrInteraction();std::ofstream("vr-test.txt")<<(passed?"PASS: tracking recovery, foregrip support, coordinate axes, rigid-view inversion, wrist clicks, shoulder slot, articulated hands, physical melee and controller interaction\n":"FAIL\n");return passed?0:60;}
    bool vrMode=commandArguments().find(L"--vr")!=nullptr;
    if(!*commandLine){TASKDIALOG_BUTTON buttons[]={{100,L"Play on PC"},{101,L"Play in VR"}};TASKDIALOGCONFIG dialog{};dialog.cbSize=sizeof(dialog);dialog.pszWindowTitle=L"Depthworks";dialog.pszMainInstruction=L"How would you like to play?";dialog.pszContent=L"VR requires SteamVR and a connected headset.";dialog.cButtons=2;dialog.pButtons=buttons;dialog.nDefaultButton=100;dialog.dwCommonButtons=TDCBF_CANCEL_BUTTON;int choice=0;HRESULT result=TaskDialogIndirect(&dialog,&choice,nullptr,nullptr);if(FAILED(result))throw std::runtime_error("Could not open the PC/VR launch chooser");if(choice==IDCANCEL)return 0;vrMode=choice==101;}
    std::unique_ptr<retro::VrRuntime> vrRuntime;
    if(vrMode){vrRuntime=std::make_unique<retro::VrRuntime>();std::string error;if(!vrRuntime->initialize(error))throw std::runtime_error(error);}
    retro::Win32Window window(W,H,L"Depthworks");
    if(!window.valid()) return 1;
    retro::Game game;if(commandArguments().find(L"--dev"))game.setDeveloperMode(true);
    game.loadCustomCampaignDirectory((std::filesystem::current_path()/L"custom maps").wstring());
    wchar_t settingsFolder[32768]{};DWORD settingsLength=GetEnvironmentVariableW(L"LOCALAPPDATA",settingsFolder,32768);
    bool exclusionZone=commandArguments().find(L"--exclusion-zone")!=nullptr||commandArguments().find(L"--guard-lab")!=nullptr;
    bool directStart=commandArguments().find(L"--surface-lift")!=nullptr||exclusionZone;
    if(exclusionZone){auto path=std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt";game=retro::Game(std::filesystem::exists(path)?retro::loadCustomCampaignFile(path):retro::loadEmbeddedExclusionZoneCampaign());}
    else if(commandArguments().find(L"--surface-lift"))game=retro::Game::mapInspection({3.5f,2.f},retro::kPi*.5f,0,3,false,0);
    std::wstring settingsPath=settingsLength>0&&settingsLength<32768?std::wstring(settingsFolder)+L"\\RawMetal\\settings.ini":L"";
    if(!settingsPath.empty())game.loadSettings(settingsPath);
    if(settingsLength>0&&settingsLength<32768)game.setSaveDirectory(std::wstring(settingsFolder)+L"\\RawMetal\\saves");
    // Loads build the saved game on a worker so the window keeps answering Windows.
    game.setAsyncLoads(true);
    if(!directStart)game.showTitleScreen();
    auto rendererStorage=std::make_unique<retro::SoftwareRenderer>(W,H);auto& renderer=*rendererStorage;
    retro::AudioEngine audio;
    renderer.setGpuActorPreparation(commandArguments().find(L"--cpu-actor-preparation")==nullptr);
    game.setAiQueryCaching(commandArguments().find(L"--uncached-ai-queries")==nullptr);
    game.setAiDecisionScheduling(commandArguments().find(L"--full-rate-ai-decisions")==nullptr);
    game.setNavAuthority(commandArguments().find(L"--lattice-walk-probes")==nullptr);
    if(vrMode){if(!renderer.enableHardware(window.handle()))throw std::runtime_error("SteamVR requires Vulkan hardware rendering: "+renderer.hardwareName());}else if(!commandArguments().find(L"--software"))renderer.enableHardware(window.handle());
    std::ofstream("RawMetal-audio.txt")<<(audio.available()?"Stereo audio device opened. ":"No audio output device could be opened. ")<<int(retro::Sound::Count)<<" embedded samples loaded.";
    using clock=std::chrono::steady_clock; auto last=clock::now(); float titleTimer=0;
    while(window.pump()){
        auto now=clock::now(); float dt=std::chrono::duration<float>(now-last).count(); last=now;
        bool wasPaused=game.paused();bool menuOpen=game.titleScreen()||wasPaused||game.inventoryOpen()||game.consoleOpen();auto input=window.input(menuOpen||vrMode);if(vrRuntime&&!vrRuntime->update(game,input,dt))break;if(vrRuntime&&!vrRuntime->trackingValid())continue;game.update(input,dt);window.setMenu(game.titleScreen()||game.paused()||game.inventoryOpen()||game.consoleOpen());
        if(wasPaused&&!game.paused()&&!settingsPath.empty())game.saveSettings(settingsPath);
        if(game.quitRequested())break;
        audio.update(game,vrMode||window.focused()); if(vrRuntime){renderer.renderVrEye(game,0);renderer.renderVrEye(game,1);}else renderer.render(game); if(!renderer.hardwarePresentsWindow())window.present(renderer.pixels(),renderer.width(),renderer.height());
        titleTimer+=dt; if(titleTimer>.25f){titleTimer=0; wchar_t t[128];if(game.titleScreen())std::swprintf(t,128,L"Depthworks");else std::swprintf(t,128,L"Depthworks | HP %.0f | Shells %d | Monsters %d",game.player().health,game.player().ammo,game.enemiesRemaining());window.setCaption(t);}
    }
    if(!settingsPath.empty())game.saveSettings(settingsPath);
    return 0;
    }catch(const std::exception& error){
        std::ofstream("RawMetal-error.txt")<<error.what();
        // Automated runs report through the exit code and RawMetal-error.txt; a
        // modal dialog would hang the test runner.
        bool automated=commandArguments().automated();
        if(automated)std::cerr<<error.what()<<'\n';
        if(!automated)MessageBoxA(nullptr,error.what(),"Depthworks could not start",MB_OK|MB_ICONERROR);
        return 8;
    }
}
#else
#include <iostream>
#include <cstdlib>
int main(){std::cout<<"This demo targets Win32. Build it on Windows with MSVC or MinGW.\n";return 0;}
#endif

#include "SoftwareRenderer.h"
#include "GpuRenderer.h"
#include "FrameWorker.h"
#include "../audio/AudioEngine.h"
#include <chrono>
#include <fstream>
#include <numeric>
#include <algorithm>
namespace retro {
bool SoftwareRenderer::testCreatureAnimation(){
 Mesh mesh(242);std::ofstream report("stalker-animation-test.txt");
 for(int clip=0;clip<5;++clip){mesh.poseCreature(clip,0);auto first=mesh.triangles;float movement=0,deformation=0;
  for(float phase:{.25f,.5f,.75f,1.f}){mesh.poseCreature(clip,phase);
   for(size_t i=0;i<first.size();++i)for(int v=0;v<3;++v){auto p=mesh.triangles[i].v[v].p,d=p-first[i].v[v].p;
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.y<-.002f)return false;
    movement=std::max(movement,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));
    auto a=p-mesh.triangles[0].v[0].p,b=first[i].v[v].p-first[0].v[0].p;
    deformation=std::max(deformation,std::fabs(std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z)-std::sqrt(b.x*b.x+b.y*b.y+b.z*b.z)));
   }
  }
  movement*=1.8f/(mesh.maximum.y-mesh.minimum.y);report<<"clip "<<clip<<" maximum vertex motion "<<movement<<" gameplay metres, non-rigid deformation "<<deformation<<'\n';if(!std::isfinite(movement)||movement<.001f||movement>4||deformation<.001f)return false;
 }
 report<<"PASS: five independently deforming skeletal clips, matching topology\n";return true;
}
bool SoftwareRenderer::testHardware(){
 SoftwareRenderer renderer(128,72);std::ofstream report("vulkan-test.txt");if(!renderer.enableHardware()){report<<renderer.hardwareName()<<'\n';return false;}report<<renderer.hardwareName()<<'\n';bool passed=true;
 auto check=[&](bool condition,const char* label){report<<label<<": "<<(condition?"PASS":"FAIL")<<'\n';passed&=condition;};
 Texture red{1,1,{0xffff0000u}},blue{1,1,{0xff0000ffu}},transparent{1,1,{0x00ffffffu}},emissive{1,1,{0xff000000u}},normal{1,1,{0xffffffffu}};
 emissive.emission={0xffffffffu};normal.normalLevels={{{1,0,0}}};NormalLighting lights;lights.directions[0]={1,0,0};lights.weights[0]=1;
 auto begin=[&]{renderer.m_gpu->begin(128,72);renderer.m_gpuFrame=true;};
 auto triangle=[&](const Texture&t,float z,float light=1,const NormalLighting* nl=nullptr){renderer.triangle3D({{-.3f,-.3f,z},0,0},{{.3f,-.3f,z},1,0},{{0,.3f,z},.5f,1},t,light,nl);};
 auto finish=[&]{renderer.m_gpu->finish(renderer.m_pixels);renderer.m_gpuFrame=false;return renderer.m_pixels[36*128+64]&0xffffffu;};
 begin();triangle(red,1);triangle(blue,2);auto pixel=finish();check((pixel&0xff0000u)>0xf00000u&&(pixel&255)==0,"Nearest surface wins depth test");
 begin();triangle(transparent,1);triangle(blue,2);pixel=finish();check((pixel&255)>240&&(pixel&0xff0000u)==0,"Alpha cutout keeps geometry behind visible");
 begin();triangle(emissive,1,0);check(finish()==0xffffffu,"Emission survives zero ambient illumination");
 renderer.m_emissionScale=.1f;begin();triangle(emissive,1,0);pixel=finish();check((pixel&255)>30&&(pixel&255)<50,"Emergency lamp emission dims on the GPU");renderer.m_emissionScale=1.f;
 begin();triangle(normal,1,.5f);auto flat=finish();begin();triangle(normal,1,.5f,&lights);auto relief=finish();check((relief&255)>(flat&255),"Authored normal map affects hardware lighting");
 begin();triangle(red,1);renderer.m_gpu->clearDepth();triangle(blue,2);pixel=finish();check((pixel&255)>240,"View-model depth range remains independent");
 begin();renderer.triangle3D({{-.3f,-.1f,-.2f},0,0},{{.3f,-.1f,1},1,0},{{0,.3f,1},.5f,1},red,1);finish();size_t coverage=0;for(auto p:renderer.m_pixels)coverage+=(p&0xffffffu)!=0x0c1012u;check(coverage>100,"Near-plane clipping keeps crossing geometry");
 renderer.m_shadowBudgetLimit=10000000;
 for(int mode:{1,2,3}){auto scene=Game::weaponInspection(mode,.16f);renderer.m_animationWorker=std::make_unique<FrameWorker>();renderer.render(scene);auto parallel=renderer.m_pixels;renderer.m_animationWorker.reset();renderer.render(scene);check(parallel==renderer.m_pixels,"Parallel arm pose matches serial reference");check(renderer.gripError()<.025f,"Hardware weapon grip remains attached");}
 check(renderer.hardwareActive(),"Hardware remains active without fallback");report<<(passed?"PASS":"FAIL")<<": Vulkan material / depth / clipping checks\n";return passed;
}
bool SoftwareRenderer::testPerformance(){
 using Clock=std::chrono::steady_clock;std::ofstream report("performance-test.txt");bool passed=true;
 auto measure=[&](const char* name,Game game,int frames,bool simulate,bool sweep){
  SoftwareRenderer renderer(DisplayWidth,DisplayHeight);AudioEngine audio(false);std::vector<double> timings;double updateMax=0,audioMax=0,renderMax=0;
  if(!renderer.enableHardware()){report<<renderer.hardwareName()<<'\n';passed=false;return;}
  report<<renderer.hardwareName()<<" / full-resolution "<<DisplayWidth<<'x'<<DisplayHeight<<'\n';
  auto startup=Clock::now();renderer.render(game);report<<"Initial material upload / first frame: "<<std::chrono::duration<double,std::milli>(Clock::now()-startup).count()<<" ms (startup, excluded)\n";
  for(int i=0;i<frames;++i){auto begin=Clock::now();InputState input{};
   if(sweep){input.mouseDx=11.f;input.mouseDy=std::sin(i*.13f)*1.7f;}
   if(simulate||sweep)game.update(input,1.f/60);auto updated=Clock::now();audio.update(game);auto mixed=Clock::now();renderer.render(game);auto rendered=Clock::now();
   updateMax=std::max(updateMax,std::chrono::duration<double,std::milli>(updated-begin).count());audioMax=std::max(audioMax,std::chrono::duration<double,std::milli>(mixed-updated).count());renderMax=std::max(renderMax,std::chrono::duration<double,std::milli>(rendered-mixed).count());
   timings.push_back(std::chrono::duration<double,std::milli>(Clock::now()-begin).count());
   if(timings.back()>50)report<<"Slow frame "<<i<<" phase "<<int(game.world().liftPhase())<<" time "<<game.world().liftPhaseTime()<<" height "<<game.world().liftHeight()<<" scene "<<renderer.m_sceneMs<<" GPU submission/readback "<<renderer.m_submitMs<<" ms\n";
   passed&=renderer.hardwareActive();
  }
  std::sort(timings.begin(),timings.end());double mean=std::accumulate(timings.begin(),timings.end(),0.)/frames,peak=timings.back(),p95=timings[frames*95/100];
  report<<name<<": avg "<<mean<<" ms, p95 "<<p95<<" ms, max "<<peak<<" ms, worst "<<1000/peak<<" FPS; >50ms "<<std::count_if(timings.begin(),timings.end(),[](double t){return t>50;})<<'/'<<frames<<'\n';report.flush();
  report<<"  Stage maxima: update "<<updateMax<<", audio "<<audioMax<<", render "<<renderMax<<" ms\n";report.flush();passed&=peak<=50;
 };
 measure("Foundry turn",Game::mapInspection({3.5f,4.5f},0,0,0,false,0,true),120,false,true);
 measure("Gantry turn",Game::mapInspection({7.5f,12.5f},0,0,2,false,0,true),120,false,true);
 measure("Lift entry turn",Game::mapInspection({3.5f,2},kPi*.5f,0,3,false,0,true),120,false,true);
 measure("Hazmat impact and settling",Game::hazmatInspection(3),120,true,false);
 measure("Ascent window",Game::liftInspection(5,3),240,true,false);
 measure("Jam and six-floor fall",Game::liftInspection(35,3),600,true,true);
 measure("Reactor balcony turn",Game::liftInspection(World::LiftRideComplete,2),180,false,true);
 auto combat=Game::mapInspection({18,17},kPi*.5f,0,3,false,-9,false);
 measure("Reactor active AI",combat,180,true,true);
 // Compare exact output with deck occlusion disabled. Geometry remains present;
 // the optimization must not change what is visible through shaft openings.
 SoftwareRenderer reference(DisplayWidth,DisplayHeight);reference.enableHardware();reference.m_shadowBudgetLimit=10000000;
 for(int view=0;view<6;++view){auto scene=view<3?Game::liftInspection(float(view)*14,3):view==3?Game::liftInspection(World::LiftRideComplete,2):Game::mapInspection({20,15},kPi*.5f,view==4?65.f:-65.f,3,false,-7.5f,true);
  reference.m_visibilityCulling=true;reference.render(scene);auto optimized=reference.m_pixels;
  reference.m_visibilityCulling=false;reference.render(scene);size_t changed=0;
  for(size_t i=0;i<optimized.size();++i)changed+=optimized[i]!=reference.m_pixels[i];
  report<<"Culling/reference view "<<view<<": "<<changed<<" changed pixels\n";report.flush();passed&=changed==0;
 }
 report<<(passed?"PASS":"FAIL")<<": 50 ms total update/audio/render budget; conservative culling equivalence.\n";return passed;
}
}

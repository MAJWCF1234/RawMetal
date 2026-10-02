#include "SoftwareRenderer.h"
#include "GpuRenderer.h"
#include "FrameWorker.h"
#include "../platform/Win32Window.h"
#include "../audio/AudioEngine.h"
#include <chrono>
#include <fstream>
#include <numeric>
#include <algorithm>
namespace retro {
__declspec(noinline) bool SoftwareRenderer::testPresentationResize(){
 Win32Window window(DisplayWidth,DisplayHeight,L"RawMetal presentation check");if(!window.valid())return false;
 auto storage=std::make_unique<SoftwareRenderer>(DisplayWidth,DisplayHeight);auto&renderer=*storage;if(!renderer.enableHardware(window.handle()))return false;
 auto scene=Game::mapInspection({3.5f,4.5f},0,0,0,false,0,true);std::ofstream report("presentation-resize-test.txt");
 for(auto size:{std::array<int,2>{1280,720},std::array<int,2>{960,540},std::array<int,2>{1280,720}}){
  if(!SetWindowPos(static_cast<HWND>(window.handle()),nullptr,0,0,size[0],size[1],SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))return false;
  for(int frame=0;frame<8;++frame){if(!window.pump())return false;renderer.render(scene);if(!renderer.hardwareActive())return false;}
  auto actual=renderer.m_gpu->surfaceExtent();report<<"Requested "<<size[0]<<'x'<<size[1]<<"; actual "<<actual.first<<'x'<<actual.second<<'\n';
  if(actual.first!=size[0]||actual.second!=size[1])return false;
 }
 ShowWindow(static_cast<HWND>(window.handle()),SW_MINIMIZE);for(int i=0;i<3;++i){window.pump();renderer.render(scene);}
 ShowWindow(static_cast<HWND>(window.handle()),SW_RESTORE);for(int i=0;i<8;++i){window.pump();renderer.render(scene);}
 bool passed=renderer.hardwareActive();report<<(passed?"PASS":"FAIL")<<": repeated resize, minimize/restore and Vulkan presentation\n";return passed;
}
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
 report<<"PASS: five independently deforming skeletal clips, matching topology\n";
 Mesh mutant(272,"",274);std::ofstream mutantReport("mutant-animation-test.txt");
 for(int clip=0;clip<5;++clip){mutant.poseCreature(clip,0);auto first=mutant.triangles;float motion=0;
  for(float phase:{.25f,.5f,.75f,1.f}){mutant.poseCreature(clip,phase);for(size_t i=0;i<first.size();++i)for(int v=0;v<3;++v){auto p=mutant.triangles[i].v[v].p,d=p-first[i].v[v].p;
   if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;motion=std::max(motion,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));}}
  mutantReport<<"clip "<<clip<<" vertex motion "<<motion<<" metres\n";if(motion<.001f||motion>4)return false;
 }
 mutantReport<<"PASS: supplied rig clips and grounded death pose interpolate with matching topology\n";
 Mesh worker(277,"",278);std::ofstream workerReport("worker-animation-test.txt");
 for(int clip=0;clip<6;++clip){worker.poseCreature(clip,0);auto first=worker.triangles;float motion=0;
  for(float phase:{.25f,.5f,.75f,1.f}){worker.poseCreature(clip,phase);auto hand=worker.poseAnchor(0);if(!std::isfinite(hand.y)||hand.y<-.002f)return false;for(size_t i=0;i<first.size();++i)for(int v=0;v<3;++v){auto p=worker.triangles[i].v[v].p,d=p-first[i].v[v].p;if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.y<-.002f)return false;motion=std::max(motion,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));}}
  workerReport<<"clip "<<clip<<" skeletal vertex motion "<<motion<<'\n';if(motion<.001f||motion>4)return false;
 }
 workerReport<<"PASS: six skeletal clips, grounded poses and interpolated hand attachments\n";return true;
}
__declspec(noinline) bool SoftwareRenderer::testHardware(){
 auto storage=std::make_unique<SoftwareRenderer>(128,72);auto&renderer=*storage;std::ofstream report("vulkan-test.txt");if(!renderer.enableHardware()){report<<renderer.hardwareName()<<'\n';return false;}report<<renderer.hardwareName()<<'\n';bool passed=true;
 auto check=[&](bool condition,const char* label){report<<label<<": "<<(condition?"PASS":"FAIL")<<'\n';passed&=condition;};
 Texture red{1,1,{0xffff0000u}},blue{1,1,{0xff0000ffu}},transparent{1,1,{0x00ffffffu}},emissive{1,1,{0xff000000u}},normal{1,1,{0xffffffffu}};
 emissive.emission={0xffffffffu};normal.normalLevels={{{1,0,0}}};NormalLighting lights;lights.directions[0]={1,0,0};lights.weights[0]=1;
 auto begin=[&]{renderer.m_gpu->begin(128,72);renderer.m_gpu->setView(0,0,0,0,0,128.f/72.f,false,0);renderer.m_gpuFrame=true;};
 auto triangle=[&](const Texture&t,float z,float light=1,const NormalLighting* nl=nullptr){renderer.triangle3D({{-.3f,-.3f,z},0,0},{{.3f,-.3f,z},1,0},{{0,.3f,z},.5f,1},t,light,nl);};
 auto finish=[&]{renderer.m_gpu->finish(renderer.m_pixels);renderer.m_gpuFrame=false;return renderer.m_pixels[36*128+64]&0xffffffu;};
 auto redChannel=[](std::uint32_t p){return int((p>>16)&255);};
 auto blueChannel=[](std::uint32_t p){return int(p&255);};
 begin();triangle(red,1);triangle(blue,2);auto pixel=finish();check(redChannel(pixel)>80&&blueChannel(pixel)<redChannel(pixel)/4,"Nearest surface wins depth test");
 begin();triangle(transparent,1);triangle(blue,2);pixel=finish();check((pixel&255)>215&&(pixel&0xff0000u)==0,"Alpha cutout keeps geometry behind visible");
 Texture liquid{1,1,{0xc4ff0000u}};liquid.transparent=true;
 begin();triangle(liquid,1);triangle(blue,2);pixel=finish();
 // The HDR path blends linear radiance before tone mapping, so the blue bed
 // contributes more display-space blue than the former gamma-space blend.
 report<<"HDR water blend RGB: "<<((pixel>>16)&255)<<", "<<((pixel>>8)&255)<<", "<<(pixel&255)<<'\n';
 check(((pixel>>16)&255)>150&&(pixel&255)>110&&(pixel&255)<200&&((pixel>>16)&255)>(pixel&255)+35,"Linear HDR water alpha blends the visible bed before tone mapping");
 begin();triangle(emissive,1,0);auto brightEmission=finish();check(blueChannel(brightEmission)>150,"Emission survives zero ambient illumination");
 renderer.m_emissionScale=.1f;begin();triangle(emissive,1,0);pixel=finish();
 // Match the headless capture and window composite exposure. Display-encoded
 // values are nonlinear: 10% radiance is not 10% pixel value.
 auto expectedEmission=[](float scale){float linear=1.6f*scale*1.05f;float mapped=std::clamp((linear*(2.51f*linear+.03f))/(linear*(2.43f*linear+.59f)+.14f),0.f,1.f);return int(std::round(std::pow(mapped,1.f/2.2f)*255.f));};
 report<<"Emission display values: full "<<blueChannel(brightEmission)<<", dim "<<blueChannel(pixel)<<", expected "<<expectedEmission(.1f)<<'\n';
 check(std::abs(blueChannel(pixel)-expectedEmission(.1f))<=2&&blueChannel(pixel)<blueChannel(brightEmission),"Emergency lamp emission matches linear dimming through display transform");renderer.m_emissionScale=1.f;
 begin();triangle(normal,1,.5f);auto flat=finish();begin();triangle(normal,1,.5f,&lights);auto relief=finish();check((relief&255)>(flat&255),"Authored normal map affects hardware lighting");
 // A black dielectric isolates specular response from diffuse light. This also
 // verifies flat-normal materials receive the same BRDF as authored neutral maps.
 Texture roughDielectric{1,1,{0xff000000u}},smoothDielectric{1,1,{0xff000000u}},neutralDielectric{1,1,{0xff000000u}};
 roughDielectric.glossStrength=.05f;smoothDielectric.glossStrength=.48f;neutralDielectric.glossStrength=.48f;
 neutralDielectric.normalLevels={{{0,0,1}}};NormalLighting frontal;frontal.directions[0]={0,0,-1};frontal.weights[0]=1;
 begin();triangle(roughDielectric,1,1,&frontal);auto roughHighlight=finish();
 begin();triangle(smoothDielectric,1,1,&frontal);auto smoothHighlight=finish();
 begin();triangle(neutralDielectric,1,1,&frontal);auto neutralHighlight=finish();
 report<<"GGX highlight values: rough "<<blueChannel(roughHighlight)<<", smooth "<<blueChannel(smoothHighlight)<<", neutral normal "<<blueChannel(neutralHighlight)<<'\n';
 check(blueChannel(smoothHighlight)>blueChannel(roughHighlight)+3,"GGX smoothness narrows and brightens dielectric highlights");
 check(std::abs(blueChannel(neutralHighlight)-blueChannel(smoothHighlight))<=1,"Flat-normal GGX matches authored neutral normal map");
 // A plane first baked from behind must retain its physical lit hemisphere
 // when viewed from the front. The former eye-facing bake erased this highlight.
 NormalLighting backBake;backBake.directions[0]={0,0,1};backBake.weights[0]=1;backBake.surfaceNormal={0,0,1};
 begin();renderer.m_gpu->setView(2,0,0,kPi,0,128.f/72.f,false,0);renderer.m_gpu->beginStaticCache(999,1);triangle(neutralDielectric,1,1,&backBake);renderer.m_gpu->endStaticCache();finish();
 begin();renderer.m_gpu->beginStaticCache(999,1);auto cachedHemisphere=finish();
 begin();renderer.triangle3D({{.3f,-.3f,1},0,0},{{-.3f,-.3f,1},1,0},{{0,.3f,1},.5f,1},neutralDielectric,1,&frontal);auto freshHemisphere=finish();
 report<<"Opposite-side bake GGX: cached "<<blueChannel(cachedHemisphere)<<", fresh "<<blueChannel(freshHemisphere)<<'\n';
 check(std::abs(blueChannel(cachedHemisphere)-blueChannel(freshHemisphere))<=1&&blueChannel(cachedHemisphere)>30,"Static bake keeps physical normals when the first camera is behind a surface");
 begin();triangle(red,1);renderer.m_gpu->clearDepth();triangle(blue,2);pixel=finish();check(blueChannel(pixel)>80&&redChannel(pixel)<blueChannel(pixel)/4,"View-model depth range remains independent");
 begin();renderer.triangle3D({{-.3f,-.1f,-.2f},0,0},{{.3f,-.1f,1},1,0},{{0,.3f,1},.5f,1},red,1);finish();size_t coverage=0;for(auto p:renderer.m_pixels)coverage+=(p&0xffffffu)!=0x0c1012u;check(coverage>100,"Near-plane clipping keeps crossing geometry");
 renderer.m_shadowBudgetLimit=10000000;
 check(Mesh::testAttachedSkinning(),"Single final skin pass matches redundant IK skinning exactly across six poses");
 check(World::testCollisionCandidates(),"Spatial collision candidates match full scans across ten maps and fixture boundaries");
 for(int mode:{1,2,3}){auto scene=Game::weaponInspection(mode,.16f);renderer.m_animationWorker=std::make_unique<FrameWorker>();renderer.render(scene);auto parallel=renderer.m_pixels;renderer.m_animationWorker.reset();renderer.render(scene);check(parallel==renderer.m_pixels,"Parallel arm pose matches serial reference");check(renderer.gripError()<.025f,"Hardware weapon grip remains attached");}
 World warehouse(16);
 check(warehouse.lightRayClear({12,3},-23,{12,15},-23),"Warehouse open aisle transmits fixture light");
 check(!warehouse.lightRayClear({1.5f,5.08f},-20,{2.5f,5.08f},-20),"Thin rack post casts an exact shadow");
 check(warehouse.lightRayClear({1.5f,5.4f},-20,{2.5f,5.4f},-20),"Open space beside the rack post stays lit");
 check(!warehouse.lightRayClear({5.5f,10},-23,{5.5f,10},-21),"Upper deck blocks light across storeys");
 check(warehouse.lightRayClear({5.5f,10.5f},-21.7f,{6.5f,10.5f},-21.7f),"Guardrail gaps transmit light instead of using the player hull envelope");
 check(!warehouse.fits(5.97f,10.5f,-21.7f,.015f,false,true),"Optical guardrail query remains separate from movement collision");
 auto cachedScene=Game::mapInspection({3.5f,4.5f},0,18,0,false,0,true);renderer.render(cachedScene);auto staticFrame=renderer.m_pixels;double cacheBuildMs=renderer.m_sceneMs;auto cacheHits=renderer.m_gpu->staticCacheHits();renderer.render(cachedScene);double cacheReuseMs=renderer.m_sceneMs;report<<"Static map scene pass: build "<<cacheBuildMs<<" ms, cached "<<cacheReuseMs<<" ms\n";check(renderer.m_gpu->staticCacheHits()>cacheHits,"Static chunk VBO is reused on the next frame");check(staticFrame==renderer.m_pixels,"Cached static chunk keeps the same rendered frame at nonzero pitch");InputState turn{};turn.mouseDx=80;turn.mouseDy=12;cachedScene.update(turn,1.f/60.f);renderer.render(cachedScene);auto turnedFrame=renderer.m_pixels;cacheHits=renderer.m_gpu->staticCacheHits();renderer.render(cachedScene);check(renderer.m_gpu->staticCacheHits()>cacheHits,"Static chunk VBO survives a camera turn");check(turnedFrame==renderer.m_pixels&&turnedFrame!=staticFrame,"Cached geometry transforms correctly after a camera turn");
 // Baked shading must not depend on the camera used for first load. View B
 // through A's reused VBO must match a fresh renderer that starts directly at B.
 auto fresh=std::make_unique<SoftwareRenderer>(128,72);if(!fresh->enableHardware())return false;
 auto bakeScene=Game::mapInspection({6,3},1.85f,-4,16,false,-25,true);renderer.render(bakeScene);
 // Move only the inspection camera, preserving the world and VBO cache key.
 auto&inspectionPlayer=const_cast<Player&>(bakeScene.player());inspectionPlayer.pos={16.5f,15.f};inspectionPlayer.angle=-2.1f;inspectionPlayer.pitch=5;
 renderer.render(bakeScene);auto oppositeFrame=renderer.m_pixels;fresh->render(bakeScene);size_t bakeCameraMismatch=0;double bakeCameraError=0;
 for(size_t i=0;i<oppositeFrame.size();++i){auto a=oppositeFrame[i],b=fresh->m_pixels[i];int peak=0;for(int shift:{0,8,16}){int difference=std::abs(int((a>>shift)&255)-int((b>>shift)&255));peak=std::max(peak,difference);bakeCameraError+=difference;}bakeCameraMismatch+=peak>4;}
 report<<"Different initial camera: pixels differing by >4/255 "<<bakeCameraMismatch<<", mean channel error "<<bakeCameraError/(turnedFrame.size()*3)<<'\n';
 check(bakeCameraMismatch<turnedFrame.size()/100&&bakeCameraError/(turnedFrame.size()*3)<.5,"Physical cached normals are independent of the first-load camera");
 for(int level:{5,6,7,9}){
  const auto&def=chunkDefinition(WorldId::Campaign,level);auto services=Game::mapInspection(def.playerStart,.6f,-10,level,false,def.spawnHeight,true);
  renderer.m_gpu->clearStaticCaches();renderer.m_cacheFixedServices=true;renderer.render(services);auto serviceCold=renderer.m_pixels;renderer.render(services);
  check(serviceCold==renderer.m_pixels,"Fixed service VBO remains identical on cache reuse");
  renderer.m_cacheFixedServices=false;renderer.render(services);size_t changedServices=0;double serviceError=0;
  for(size_t i=0;i<serviceCold.size();++i){int peak=0;for(int shift:{0,8,16}){int d=std::abs(int((serviceCold[i]>>shift)&255)-int((renderer.m_pixels[i]>>shift)&255));peak=std::max(peak,d);serviceError+=d;}changedServices+=peak>4;}
  report<<"Service cache/reference map "<<level<<": "<<changedServices<<" pixels >4/255; mean error "<<serviceError/(serviceCold.size()*3)<<'\n';
  check(changedServices<serviceCold.size()/100&&serviceError/(serviceCold.size()*3)<.5,"Fixed service cache preserves reference shading and visibility");
 }
 renderer.m_cacheFixedServices=true;
 auto reliefScene=Game::mapInspection({3.5f,4.5f},0,18,0,false,0,true);float reliefScale=renderer.m_wall.parallaxScale;renderer.m_wall.parallaxScale=0;renderer.m_gpu->clearStaticCaches();renderer.render(reliefScene);auto flatWall=renderer.m_pixels;renderer.m_wall.parallaxScale=reliefScale;renderer.m_gpu->clearStaticCaches();renderer.render(reliefScene);size_t changed=0;for(size_t i=0;i<flatWall.size();++i)changed+=flatWall[i]!=renderer.m_pixels[i];report<<"Parallax material changed pixels at 128x72: "<<changed<<'\n';check(changed>8,"Nearby wall relief changes the Vulkan image");
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
 measure("Ashfall terrain and streaming turn",Game(WorldId::Ashfall),120,true,true);
 measure("Lift entry turn",Game::mapInspection({3.5f,2},kPi*.5f,0,3,false,0,true),120,false,true);
 measure("Hazmat impact and settling",Game::hazmatInspection(3),120,true,false);
 measure("Ascent window",Game::liftInspection(5,3),240,true,false);
 measure("Jam and six-floor fall",Game::liftInspection(35,3),600,true,true);
 measure("Reactor balcony turn",Game::liftInspection(World::LiftRideComplete,2),180,false,true);
 auto combat=Game::mapInspection({18,17},kPi*.5f,0,3,false,-9,false);
 measure("Reactor active AI",combat,180,true,true);
 for(int level=6;level<10;++level){const auto& def=chunkDefinition(WorldId::Campaign,level);
  std::string name="Utility chapter "+std::to_string(level+1)+" turn";
  measure(name.c_str(),Game::mapInspection(def.playerStart,.6f,0,level,false,def.spawnHeight,false),120,true,true);
 }
 // Compare exact output with deck occlusion disabled. Geometry remains present;
 // the optimization must not change what is visible through shaft openings.
 SoftwareRenderer reference(DisplayWidth,DisplayHeight);reference.enableHardware();reference.m_shadowBudgetLimit=10000000;
 for(int view=0;view<6;++view){auto scene=view<3?Game::liftInspection(float(view)*14,3):view==3?Game::liftInspection(World::LiftRideComplete,2):Game::mapInspection({20,15},kPi*.5f,view==4?65.f:-65.f,3,false,-7.5f,true);
  // Warm the static chunk cache before comparing the two visibility modes;
  // the first render also builds lighting and is a different workload.
  reference.m_visibilityCulling=true;reference.render(scene);auto coldFrame=reference.m_pixels;reference.render(scene);auto optimized=reference.m_pixels;
  size_t coldDifference=0;for(size_t i=0;i<optimized.size();++i)coldDifference+=optimized[i]!=coldFrame[i];report<<"Cold/warm view "<<view<<": "<<coldDifference<<" changed pixels\n";passed&=coldDifference==0;
  reference.m_visibilityCulling=false;reference.render(scene);size_t changed=0;
  for(size_t i=0;i<optimized.size();++i)changed+=optimized[i]!=reference.m_pixels[i];
  report<<"Culling/reference view "<<view<<": "<<changed<<" changed pixels\n";report.flush();passed&=changed==0;
 }
 report<<(passed?"PASS":"FAIL")<<": 50 ms total update/audio/render budget; conservative culling equivalence.\n";return passed;
}
bool SoftwareRenderer::testCablePerformance(){
 using Clock=std::chrono::steady_clock;std::ofstream report("diagnostics/pixel-compare/cable-vaults-layout-pass/cable-performance.txt");
 Game game=Game::mapInspection({13.f,6.8f},kPi*.5f,-8.f,6,false,-9.f,false);
 SoftwareRenderer renderer(DisplayWidth,DisplayHeight);if(!renderer.enableHardware()){report<<renderer.hardwareName()<<"\n";return false;}
 std::vector<double> wall,scene,submit;wall.reserve(90);scene.reserve(90);submit.reserve(90);
 for(int frame=0;frame<100;++frame){InputState look{};look.mouseDx=frame<10?0.f:2.f;look.mouseDy=std::sin(frame*.13f);game.update(look,1.f/60.f);
  auto start=Clock::now();renderer.render(game);double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
  if(frame>=10){wall.push_back(elapsed);scene.push_back(renderer.m_sceneMs);submit.push_back(renderer.m_submitMs);}
 }
 auto stats=[](std::vector<double>&values){std::sort(values.begin(),values.end());return std::array<double,3>{std::accumulate(values.begin(),values.end(),0.0)/values.size(),values[values.size()*95/100],values.back()};};
 auto w=stats(wall),s=stats(scene),g=stats(submit);
 report<<renderer.hardwareName()<<" / "<<DisplayWidth<<'x'<<DisplayHeight<<" / active Map 6 sump sweep\n"
       <<"wall avg/p95/max: "<<w[0]<<" / "<<w[1]<<" / "<<w[2]<<" ms\n"
       <<"scene-build avg/p95/max: "<<s[0]<<" / "<<s[1]<<" / "<<s[2]<<" ms\n"
       <<"GPU submit+readback avg/p95/max: "<<g[0]<<" / "<<g[1]<<" / "<<g[2]<<" ms\n";
 return w[1]<=50.f;
}
}

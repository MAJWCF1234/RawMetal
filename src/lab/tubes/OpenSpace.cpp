// Open space: what the renderer and the streamer spend on a big outdoor world, and where it goes.
//
// Fixtures open-*.lab: the Ashfall zone, and a generated world of N x N stitched 24 m chunks
// (`world stress`, lab/StressZone.cpp: forest, rocks, ruins, wrecks, props). The camera stands still
// and pans, or (key `walk` metres a second) the player runs in a straight line across chunk
// boundaries. Key `view` sets the view distance in metres (Game::setViewDistance) when it exists.
//
// Per view, after the frame settles, medians over the frames of: the game's own frame time (update plus
// render, without the lab's headless readback), CPU scene build, command recording, the wait for the
// GPU, the GPU's pass times, draws, triangles drawn from the static caches / per-frame / shadow, chunks
// resident and drawn; the cold frame (every cache built). A walk adds the worst frame over the median
// (a hitch is a frame over twice the median) and the frames over 16.6 ms.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "core/FrameProfile.h"
#include "renderer/GpuRenderer.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
namespace retro::lab {
namespace {
struct Rig {std::map<std::string,std::vector<double>> samples;std::vector<double> play;double coldRender=0,coldScene=0;bool hadPerf=false,hadLeaf=true;std::uint64_t builds0=0,builds1=0;};
std::map<const Scene*,std::unique_ptr<Rig>> rigs;
Rig& rig(Scene& s){auto& r=rigs[&s];if(!r)r=std::make_unique<Rig>();return *r;}
double median(std::vector<double> v){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[v.size()/2];}
constexpr int Warm=4;
}
static const bool registered=add({
 .name="open-space",.summary="frame time, draws, triangles and chunk counts of a big outdoor world (the Ashfall zone, a generated 4-13 chunk square forest) standing and walking across chunk boundaries",
 .fixtures={"open-*"},
 .setup=[](Scene& s){Rig& r=rig(s);r=Rig{};auto& g=s.g();
  if(auto* gpu=LabAccess::gpu(*s.renderer))gpu->clearStaticCaches();
  r.hadPerf=perf::enabled;r.hadLeaf=perf::leafTimers;perf::enabled=true;perf::leafTimers=false;r.builds0=s.renderer->staticGeometryBuilds();
  s.renderer->setBuildBudget(s.fixture->number("walk",0)>0?8:0); // a standing view bakes everything; a walk streams on the game's budget
  LabAccess::setGod(g,true);
  for(int i=0;i<240;++i)g.update({},1.f/60); // a world that has settled: props asleep, creatures where they stand
  if(s.fixture->number("walk",0)>0)LabAccess::playerCamera(g);
  if(s.fixture->number("lift",0)>0)LabAccess::player(g).z+=s.fixture->number("lift",0); // a raised look across the world
  if(s.fixture->number("view",0)>0)g.setViewDistance(s.fixture->number("view",0));},
 .step=[](Scene& s,float dt){const float walk=s.fixture->number("walk",0);auto& p=LabAccess::player(s.g());
  if(walk>0&&s.frame>0){p.pos.x+=std::cos(p.angle)*walk*dt;p.pos.y+=std::sin(p.angle)*walk*dt;}
  else if(s.frame>0)p.angle+=.0105f;
  perf::costs.fill(0);perf::streamTriangles.fill(0);},
 .capture=[](Scene& s,const Frame& f){Rig& r=rig(s);auto& R=*s.renderer;auto* gpu=LabAccess::gpu(R);
  if(s.frame==0){r.coldScene=R.frameStages()[0];r.coldRender=f.renderMs;r.builds1=R.staticGeometryBuilds();return;}
  auto add=[&](const std::string& k,double v){r.samples[k].push_back(v);};
  double readback=0;if(gpu)readback=gpu->finishTimes()[2];
  const double play=f.updateMs+f.renderMs-readback;
  if(s.frame<Warm)return;
  r.play.push_back(play);add("play_ms",play);add("update_ms",f.updateMs);add("render_ms",f.renderMs);add("scene_ms",R.frameStages()[0]);add("submit_ms",R.frameStages()[1]);
  for(int c=0;c<perf::Count;++c)add(std::string("perf_")+perf::names[c]+"_ms",perf::costs[size_t(c)]);
  if(gpu){const auto d=gpu->drawStatistics();using G=GpuRenderer;
   add("draws",double(d[G::Draws]));add("caches_drawn",double(d[G::CachesDrawn]));add("cached_tris",double(d[G::CachedTriangles]));add("dynamic_tris",double(d[G::DynamicTriangles]));
   add("shadow_draws",double(d[G::ShadowDraws]));add("shadow_tris",double(d[G::ShadowTriangles]));add("upload_kb",double(d[G::DynamicBytes]+d[G::ActorBytes])/1024.0);
   const auto t=gpu->finishTimes();add("record_ms",t[0]);add("gpu_wait_ms",t[1]);}
  int resident=0;for(int level=0;level<s.g().chunkCount();++level)resident+=s.g().chunkResident(level);add("chunks_resident",resident);
  add("chunks_drawn",double(R.renderedChunks().size()));},
 .metrics=[](Scene& s,Report& rep){Rig& r=rig(s);
  for(const auto& [k,v]:r.samples){const bool ms=k.size()>3&&k.compare(k.size()-3,3,"_ms")==0;rep.metric(k,median(v),ms?"ms":"");}
  rep.metric("cold_render_ms",r.coldRender,"ms");rep.metric("cold_scene_ms",r.coldScene,"ms");rep.metric("cache_builds_cold",double(r.builds1-r.builds0));
  rep.metric("chunks",double(s.g().chunkCount()));
  auto play=r.play;const double med=median(play);std::sort(play.begin(),play.end());
  if(!play.empty()){rep.metric("play_p95_ms",play[size_t(double(play.size()-1)*.95)],"ms");rep.metric("play_max_ms",play.back(),"ms");
   int hitches=0,slow=0;std::string where;for(size_t k=0;k<r.play.size();++k){const double v=r.play[k];if(v>2*med){++hitches;if(where.size()<120)where+=" #"+std::to_string(k+Warm)+"="+std::to_string(int(v));}slow+=v>16.6;}rep.metric("hitch_frames",hitches);rep.metric("frames_over_16_6ms",slow);
   if(s.fixture->number("walk",0)>0)rep.check("walking across chunk boundaries: no frame over twice the median",hitches==0,std::to_string(hitches)+where+" frames over "+std::to_string(2*med)+" ms, worst "+std::to_string(play.back())+" ms");}
  if(s.fixture->number("budget",0)>0)rep.check("the median frame holds the budget",med<=s.fixture->number("budget",0),std::to_string(med)+" ms against "+std::to_string(s.fixture->number("budget",0))+" ms");},
 .teardown=[](Scene& s){s.renderer->setBuildBudget(8);auto it=rigs.find(&s);if(it!=rigs.end()){perf::enabled=it->second->hadPerf;perf::leafTimers=it->second->hadLeaf;rigs.erase(it);}}});
}

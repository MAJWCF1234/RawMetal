// Instancing: what the renderer spends to draw busy scenes, where it goes, and the creature stream.
//
// The Vulkan path keeps the static world (walls, terrain, library fixtures and foliage, resting
// props by 4 m cell, fixture parts, lamp housings, pipes, breakables) in static caches: world-space
// triangles baked once on the CPU, lit per vertex, and drawn each frame as one draw per (cache,
// material). What moves (awake props, shards, leads, switchable lamps, particles) is transformed on
// the CPU and uploaded every frame. People go through the instanced actor stream (one instance a
// triangle, one light record a body); creatures now do too (SoftwareRenderer::setCreatureStream,
// knob creature_stream), where before each face was transformed, lit and uploaded on the CPU.
//
// Per view, after the frame settles, the median over the frames of: draws, descriptor, pipeline and
// vertex-buffer binds, static caches drawn, triangles drawn from static caches / uploaded this frame
// / in the actor stream, shadow draws and triangles, KB uploaded; CPU scene build, command recording,
// the wait for the GPU and the headless readback (ms; the readback is the lab's, the game presents);
// the GPU's own pass times (timestamps); the renderer's cost scopes; and which scope filled the
// per-frame stream. The cold frame (frame 0: every static cache built) gives the bake cost.
//
// Old against new, on every sampled frame: the same moment drawn again with the creature stream off
// (old_* metrics, the CPU path) and compared pixel for pixel with the new frame: the largest channel
// difference, the mean, and the share of pixels that differ at all and by more than 8 of 255.
//
// Fixtures: inst-*.lab. The camera pans 0.6 degrees a frame so the views are not one frozen frame.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "core/FrameProfile.h"
#include "renderer/GpuRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
namespace retro::lab {
namespace {
struct Rig {
 std::map<std::string,std::vector<double>> samples;double coldScene=0,coldRender=0;bool hadPerf=false,hadLeaf=true;
 std::uint64_t buildsBefore=0,buildsAfterCold=0,buildsEnd=0;
 int maxDiff=0;double diffSum=0,anySum=0,bigSum=0;int compared=0;
};
std::map<const Scene*,std::unique_ptr<Rig>> rigs;
Rig& rig(Scene& s){auto& r=rigs[&s];if(!r)r=std::make_unique<Rig>();return *r;}
double median(std::vector<double> v){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[v.size()/2];}
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
constexpr int Warm=3; // frames left to settle (lamp shadows, probes) before sampling

// One render's numbers, under a prefix ("" the new path, "old_" the CPU creature path).
void sample(Rig& r,SoftwareRenderer& R,const std::string& prefix,double renderMs){
 auto add=[&](const std::string& k,double v){r.samples[prefix+k].push_back(v);};
 const auto stages=R.frameStages();add("render_ms",renderMs);add("scene_ms",stages[0]);add("submit_ms",stages[1]);
 // The renderer's own cost scopes (core/FrameProfile.h): CPU scene parts and GPU passes.
 for(const auto c:{perf::Scene,perf::Actor,perf::Prop,perf::Air,perf::Sky,perf::GpuShadows,perf::GpuScene,perf::GpuOcclusion,perf::GpuShafts,perf::GpuCompose})
  add(std::string("perf_")+perf::names[c]+"_ms",perf::costs[c]);
 // Who fills the per-frame stream: triangles uploaded while each scope was innermost.
 for(const auto c:{perf::Scene,perf::Actor,perf::Prop,perf::Air})add(std::string("stream_")+perf::names[c]+"_tris",double(perf::streamTriangles[size_t(c)+1]));
 if(auto* gpu=LabAccess::gpu(R)){const auto d=gpu->drawStatistics();using G=GpuRenderer;
  add("draws",double(d[G::Draws]));add("descriptor_binds",double(d[G::DescriptorBinds]));add("pipeline_binds",double(d[G::PipelineBinds]));add("buffer_binds",double(d[G::BufferBinds]));
  add("caches_drawn",double(d[G::CachesDrawn]));add("cached_tris",double(d[G::CachedTriangles]));add("dynamic_tris",double(d[G::DynamicTriangles]));add("actor_tris",double(d[G::ActorTriangles]));
  add("shadow_draws",double(d[G::ShadowDraws]));add("shadow_tris",double(d[G::ShadowTriangles]));add("upload_kb",double(d[G::DynamicBytes]+d[G::ActorBytes])/1024.0);
  const auto t=gpu->finishTimes();add("record_ms",t[0]);add("gpu_wait_ms",t[1]);add("readback_ms",t[2]);}
}
void zero(){perf::costs.fill(0);perf::streamTriangles.fill(0);}
double timed(SoftwareRenderer& R,Game& g){zero();const auto t=std::chrono::steady_clock::now();R.render(g);return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();}

const bool instancing=add({
 .name="instancing",.summary="draw calls, triangles (static cache / per-frame upload / actor stream) and frame time in busy scenes (Ashfall forest, Chapter 2, the Starter Map); creatures in the actor stream against the CPU path, pixel for pixel",
 .fixtures={"inst-*"},
 .setup=[](Scene& s){Rig& r=rig(s);r=Rig{};if(auto* gpu=LabAccess::gpu(*s.renderer)){gpu->clearStaticCaches();gpu->clearActorEmitCache();}
  r.hadPerf=perf::enabled;r.hadLeaf=perf::leafTimers;perf::enabled=true;perf::leafTimers=false;r.buildsBefore=s.renderer->staticGeometryBuilds();s.renderer->setCreatureStream(true);},
 .step=[](Scene& s,float){if(s.frame>0)LabAccess::player(s.g()).angle+=.0105f;zero();},
 .capture=[](Scene& s,const Frame& f){Rig& r=rig(s);auto& R=*s.renderer;
  if(s.frame==0){r.coldScene=R.frameStages()[0];r.coldRender=f.renderMs;r.buildsAfterCold=R.staticGeometryBuilds();return;}
  if(s.frame<Warm)return;
  sample(r,R,"",f.renderMs);
  if(!R.hardwareActive()){if(s.last())r.buildsEnd=R.staticGeometryBuilds();return;}
  // The same moment, creatures on the CPU path, then the new path again (the frame the lab keeps).
  const std::vector<std::uint32_t> now(f.pixels,f.pixels+size_t(f.width)*f.height);
  R.setCreatureStream(false);const double oldMs=timed(R,s.g());sample(r,R,"old_",oldMs);
  const std::vector<std::uint32_t> before(R.pixels(),R.pixels()+now.size());R.setCreatureStream(true);
  long any=0,big=0;double sum=0;int worst=0;std::vector<std::uint32_t> diff(now.size());
  for(size_t i=0;i<now.size();++i){int m=0;for(int c=0;c<24;c+=8){const int d=std::abs(int((now[i]>>c)&255u)-int((before[i]>>c)&255u));m=std::max(m,d);sum+=d;}
   worst=std::max(worst,m);any+=m>0;big+=m>8;const std::uint32_t a=std::uint32_t(std::min(255,m*8));diff[i]=0xff000000u|(a<<16)|(a<<8)|a;}
  r.maxDiff=std::max(r.maxDiff,worst);r.diffSum+=sum/(3.0*double(now.size()));r.anySum+=double(any)/double(now.size());r.bigSum+=double(big)/double(now.size());++r.compared;
  if(s.last()){s.shots.push_back({"creatures on the CPU path (old)",f.width,f.height,before});s.shots.push_back({"old against new, difference x8",f.width,f.height,std::move(diff)});}
  R.render(s.g());
  if(s.last())r.buildsEnd=R.staticGeometryBuilds();},
 .metrics=[](Scene& s,Report& rep){Rig& r=rig(s);Game& g=s.g();const World& w=g.world();
  // Census of what the view holds (the current map; streaming neighbours draw too).
  int library=0;for(const auto& f:w.fixtures())if(f.model>=LabAccess::firstLibraryModel())++library;
  int awake=0,resting=0;for(const auto& c:LabAccess::clutter(g)){if(c.broken)continue;(c.model>=0&&c.sleeping?resting:awake)++;}
  int creatures=0;for(int level=0;level<g.chunkCount();++level)for(const auto& e:g.chunkEnemies(level))creatures+=e.visible();
  rep.metric("fixtures",double(w.fixtures().size()));rep.metric("library_fixtures",library);rep.metric("props_resting",resting);rep.metric("props_awake",awake);rep.metric("creatures",creatures);
  rep.metric("cold_scene_ms",r.coldScene,"ms");rep.metric("cold_render_ms",r.coldRender,"ms");
  rep.metric("cold_static_builds",double(r.buildsAfterCold-r.buildsBefore));rep.metric("warm_static_builds",double(r.buildsEnd-r.buildsAfterCold));
  for(const auto& [k,v]:r.samples){const bool ms=k.size()>3&&k.compare(k.size()-3,3,"_ms")==0;rep.metric(k,median(v),ms?"ms":"");}
  if(!s.renderer->hardwareActive()){rep.check("the software path renders the view",median(r.samples["render_ms"])>0);return;}
  rep.check("the scene draws through the static caches",median(r.samples["cached_tris"])>0,std::to_string(int(median(r.samples["cached_tris"])))+" cached triangles");
  rep.check("a warm frame rebuilds no chunk cache",r.buildsEnd==r.buildsAfterCold,std::to_string(r.buildsEnd-r.buildsAfterCold)+" rebuilds after frame 0");
  const double n=std::max(1,r.compared),mean=r.diffSum/n,anyShare=r.anySum/n,bigShare=r.bigSum/n;
  rep.metric("pixel_max_diff",r.maxDiff,"of 255");rep.metric("pixel_mean_diff",mean,"of 255");rep.metric("pixel_share_differing",anyShare,"share");rep.metric("pixel_share_over_8",bigShare,"share");
  const double moved=median(r.samples["old_dynamic_tris"])-median(r.samples["dynamic_tris"]);rep.metric("creature_tris_moved",moved);
  rep.check("the creature stream draws what the CPU path drew (mean under 0.05 of 255, under 0.1% of pixels off by more than 8)",mean<.05&&bigShare<.001,
   "max "+std::to_string(r.maxDiff)+", mean "+fixed(mean,4)+", differing "+fixed(anyShare*100,3)+"%, over 8: "+fixed(bigShare*100,4)+"%");
  rep.check("the creature stream takes creature faces off the per-frame upload",moved>=0,fixed(moved,0)+" triangles");},
 .teardown=[](Scene& s){s.renderer->setCreatureStream(true);auto it=rigs.find(&s);if(it!=rigs.end()){perf::enabled=it->second->hadPerf;perf::leafTimers=it->second->hadLeaf;rigs.erase(it);}}});
}
}

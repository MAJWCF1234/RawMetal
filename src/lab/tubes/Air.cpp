// The air (renderer/Particles: steam, smoke, fire, sparks, a grenade's trail and
// blast) and the light probes (renderer/LightProbes: the bounce a room's lamps
// leave, crystal glow, reflection probes on a polished floor). Friends: smoke is
// lit by the probes, and drifts through doors.
//
// Both systems are being polished in the main tree. Each tube is compiled where
// its system exists (LabAccess.h finds the header); elsewhere `particles` falls
// back to the map's plain emitters and `probes` is not registered.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include <cmath>
#include <cstdio>
#ifdef RAWMETAL_LAB_HAS_PARTICLES
#include "world/CampaignKit.h"
#endif
namespace retro::lab {
namespace {
std::string fixed(float v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
Vec2 pairAt(const Fixture& f,const char* key,Vec2 fallback){Vec2 v=fallback;const auto t=f.value(key);if(!t.empty())std::sscanf(t.c_str(),"%f %f",&v.x,&v.y);return v;}
// Mean luminance (0..255) in a screen box.
double luminance(const SoftwareRenderer& r,int x0,int y0,int x1,int y1){double sum=0;int n=0;const auto* p=r.pixels();
 for(int y=std::max(0,y0);y<std::min(r.height(),y1);++y)for(int x=std::max(0,x0);x<std::min(r.width(),x1);++x){auto c=p[size_t(y)*r.width()+x];sum+=.2126*((c>>16)&255)+.7152*((c>>8)&255)+.0722*(c&255);++n;}
 return n?sum/n:0;}

// ------------------------------------------------------------------ particles
const bool particles=add({
 .name="particles",.summary="the air: steam leaks and jets, smoke, fire, a grenade's trail and blast; budgets, nothing inside walls, cost per frame",
 .friends={"probes","doors"},.fixtures={"air-room","grenade-86"},
 .author=[](Scene& s,AuthoredMapData& m){
  if(s.fixture->value("air","room")!="room")return;
  const Vec2 o=pairAt(*s.fixture,"air-at",{8.f,12.f});
  #ifdef RAWMETAL_LAB_HAS_PARTICLES
  kit::steamLeak(m,{o.x,o.y+2.5f},1.4f,{.9f,-.2f,.1f},tuned("particles.steam",.7f));kit::steamJet(m,{o.x+.5f,o.y-3.f},.9f,{0,0,1.8f});
  kit::smokeColumn(m,{o.x+4.f,o.y+3.f},.1f,2.4f,tuned("particles.smoke",.8f));kit::fire(m,{o.x+7.5f,o.y+3.5f},0.f,.35f,.7f);
  #else
  m.emitters.push_back(ParticleEmitter{{o.x,o.y+2.5f},1.4f,{.4f,-.1f}});m.emitters.push_back(ParticleEmitter{{o.x+4.f,o.y+3.f},.1f,{.05f,.02f}});
  #endif
  },
 .setup=[](Scene& s){
  #ifdef RAWMETAL_LAB_HAS_PARTICLES
  // A grenade: thrown from beside the camera, lies 1 s, goes off (as --each-test=particles films it).
  const float dist=s.fixture->number("grenade",0);if(dist<=0)return;Game& g=s.g();const Player& p=g.player();const World& w=g.world();
  const Vec2 at=p.pos+Vec2{std::cos(p.angle),std::sin(p.angle)}*dist;const float floor=w.supportBelow(at.x,at.y,p.z+1.f),thrown=g.elapsed()+.1f,flight=.6f,fuse=1.f;
  const Point3 from{p.pos.x+std::cos(p.angle+.5f)*.8f,p.pos.y+std::sin(p.angle+.5f)*.8f,p.z+1.5f};
  const Point3 v{(at.x-from.x)/flight,(at.y-from.y)/flight,(floor+.06f-from.z+4.9f*flight*flight)/flight};
  auto& mem=LabAccess::particleMemory(*s.renderer);mem.extra.throws={{from,v,thrown,fuse,g.level()}};mem.extra.blasts={{at,floor+.55f,thrown+fuse,g.level()}};
  s.points["grenade"]={{at.x,at.y,floor}};
  #endif
  },
 .capture=[](Scene& s,const Frame& f){
  #ifdef RAWMETAL_LAB_HAS_PARTICLES
  if(const auto* st=LabAccess::particleStats(*s.renderer)){
   s.board["air.frames"]+=1;s.board["air.particles"]+=st->particles;s.board["air.ms"]+=st->ms;
   s.board["air.max_blended"]=std::max(s.board["air.max_blended"],double(st->blended));s.board["air.max_glowing"]=std::max(s.board["air.max_glowing"],double(st->glowing));
   s.board["air.max_particles"]=std::max(s.board["air.max_particles"],double(st->particles));s.board["air.inside"]+=st->inside;s.board["air.thinned"]+=st->thinned;s.board["air.sources"]=st->sources;}
  #endif
  // Motion: how much of the frame the air moves from one frame to the next (the room itself is still).
  static thread_local std::vector<std::uint32_t> previous;const size_t n=size_t(f.width)*f.height;
  if(previous.size()==n&&f.index>0){size_t moved=0;for(size_t i=0;i<n;++i){const auto a=previous[i],b=f.pixels[i];int m=0;for(int k=0;k<24;k+=8)m=std::max(m,std::abs(int((a>>k)&255)-int((b>>k)&255)));moved+=m>12;}
   s.board["air.motion"]=std::max(s.board["air.motion"],double(moved)/double(n));}
  previous.assign(f.pixels,f.pixels+n);},
 .metrics=[](Scene& s,Report& r){
  r.metric("motion",s.board["air.motion"],"share");r.check("the air moves",s.board["air.motion"]>.002,fixed(float(s.board["air.motion"]*100),2)+"% of the frame changes frame to frame at most");
  #ifdef RAWMETAL_LAB_HAS_PARTICLES
  const double frames=std::max(1.0,s.board["air.frames"]);
  r.metric("sources",s.board["air.sources"]);r.metric("particles_mean",s.board["air.particles"]/frames);r.metric("particles_max",s.board["air.max_particles"]);
  r.metric("blended_max",s.board["air.max_blended"]);r.metric("glowing_max",s.board["air.max_glowing"]);r.metric("air_ms_mean",s.board["air.ms"]/frames,"ms");r.metric("held_out_of_walls",s.board["air.inside"]); // ParticleStats::inside: particles not drawn because they were in a wallr.metric("thinned",s.board["air.thinned"]);
  r.check("the air is drawn",s.board["air.max_particles"]>0,fixed(float(s.board["air.max_particles"]),0)+" particles at most");
  r.check("the blended budget holds",s.board["air.max_blended"]<=Particles::BlendedBudget,fixed(float(s.board["air.max_blended"]),0)+" of "+std::to_string(Particles::BlendedBudget));
  r.check("the glow budget holds",s.board["air.max_glowing"]<=Particles::GlowBudget,fixed(float(s.board["air.max_glowing"]),0)+" of "+std::to_string(Particles::GlowBudget));
  #endif
  },
 .reporter=[](Scene& s,Overlay& o){
  const World& w=s.g().world();int i=0;
  for(const auto& e:w.particleEmitters()){const Point3 p{e.position.x,e.position.y,e.z};o.cross(p,.2f,0x60e0ff);
   #ifdef RAWMETAL_LAB_HAS_PARTICLES
   o.line(p,p+Point3{e.drift.x,e.drift.y,e.rise}*.5f,0x60e0ff);o.label(p,"EMITTER "+std::to_string(i)+" X"+std::to_string(e.count),0x60e0ff);
   #else
   o.label(p,"EMITTER "+std::to_string(i),0x60e0ff);
   #endif
   ++i;}
  for(const auto& g:s.points["grenade"]){o.cross(g,.3f,0xff8020);o.label(g,"GRENADE",0xff8020);}
  #ifdef RAWMETAL_LAB_HAS_PARTICLES
  if(const auto* st=LabAccess::particleStats(*s.renderer))o.text(4,4,"AIR "+std::to_string(st->particles)+" BLEND "+std::to_string(st->blended)+"/"+std::to_string(Particles::BlendedBudget)+" GLOW "+std::to_string(st->glowing)+" THIN "+std::to_string(st->thinned)+" CULL "+std::to_string(st->culled)+" IN "+std::to_string(st->inside),0x60e0ff);
  #endif
  },
 .teardown=[](Scene& s){
  #ifdef RAWMETAL_LAB_HAS_PARTICLES
  auto& mem=LabAccess::particleMemory(*s.renderer);mem.extra.throws.clear();mem.extra.blasts.clear();
  #endif
  (void)s;}});

#ifdef RAWMETAL_LAB_HAS_PROBES
// ------------------------------------------------------------------ probes
// A/B: the last frame drawn with the probes and without, in the boxes a fixture names
// (probe-box X0 Y0 X1 Y1, screen pixels; default the middle third), plus the probes' own debug view.
const bool probes=add({
 .name="probes",.summary="light probes: the bounce in a dark corner, crystal glow, reflections on a polished floor (A/B with --no-probes, --detail-probes view)",
 .friends={"particles"},.fixtures={"probe-dark-corner","probe-crystal","probe-polish"},
 .author=[](Scene& s,AuthoredMapData& m){
  const auto t=s.fixture->value("polish-floor");if(t.empty())return;float x0,y0,x1,y1;if(std::sscanf(t.c_str(),"%f %f %f %f",&x0,&y0,&x1,&y1)==4)m.structures.push_back({x0,y0,x1,y1,0.f,.04f,false,50});},
 .setup=[](Scene& s){SoftwareRenderer::setLightProbes(true);},
 .capture=[](Scene& s,const Frame& f){
  if(!s.last())return;auto& r=*s.renderer;int x0=f.width/3,y0=f.height/3,x1=2*f.width/3,y1=2*f.height/3;
  const auto box=s.fixture->value("probe-box");if(!box.empty())std::sscanf(box.c_str(),"%d %d %d %d",&x0,&y0,&x1,&y1);
  s.board["probes.box"]=1;s.board["probes.x0"]=x0;s.board["probes.y0"]=y0;s.board["probes.x1"]=x1;s.board["probes.y1"]=y1;
  const double with=luminance(r,x0,y0,x1,y1);double blue=0;{const auto* p=r.pixels();double b=0,red=0;for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){auto c=p[size_t(y)*f.width+x];b+=c&255;red+=(c>>16)&255;}blue=b/std::max(1.0,red);}
  SoftwareRenderer::setLightProbes(false);r.render(s.g());const double without=luminance(r,x0,y0,x1,y1);s.shoot("probes off");
  SoftwareRenderer::setLightProbes(true);
  const int layer=SoftwareRenderer::detailLayer();SoftwareRenderer::setDetailLayer(10);r.render(s.g());s.shoot("probe fill (--detail-probes)");
  SoftwareRenderer::setDetailLayer(11);r.render(s.g());s.shoot("finish (--detail-finish)");SoftwareRenderer::setDetailLayer(layer);r.render(s.g());
  s.board["probes.with"]=with;s.board["probes.without"]=without;s.board["probes.blue_red"]=blue;
  if(auto bake=LabAccess::probes(r,s.g().level())){s.board["probes.open"]=bake->volume.openProbes;s.board["probes.bake_ms"]=bake->volume.milliseconds;s.board["probes.reflections"]=double(bake->reflections.size());}},
 .metrics=[](Scene& s,Report& r){
  const double with=s.board["probes.with"],without=s.board["probes.without"];
  r.metric("lum_with",with);r.metric("lum_without",without);r.metric("lift",with-without);r.metric("blue_red",s.board["probes.blue_red"]);
  r.metric("open_probes",s.board["probes.open"]);r.metric("bake_ms",s.board["probes.bake_ms"],"ms");r.metric("reflection_probes",s.board["probes.reflections"]);
  r.check("the map has baked probes",s.board["probes.open"]>0,fixed(float(s.board["probes.open"]),0)+" open probes");
  const auto expect=s.fixture->value("probe-expect","lift");
  if(expect=="lift")r.check("the probes lift the dark corner without flooding it",with>without+.5&&with<without+60,fixed(float(without),1)+" -> "+fixed(float(with),1));
  if(expect=="cold")r.check("the crystal lights the floor cold (blue over red)",s.board["probes.blue_red"]>1.05,"blue/red "+fixed(float(s.board["probes.blue_red"]),2));
  if(expect=="mirror")r.check("the polished floor shows the room (reflection probes change it)",std::fabs(with-without)>.5&&s.board["probes.reflections"]>0,fixed(float(without),1)+" -> "+fixed(float(with),1));},
 .reporter=[](Scene& s,Overlay& o){
  auto bake=LabAccess::probes(*s.renderer,s.g().level());if(!bake)return;const auto& v=bake->volume;const Point3 eye{s.g().player().pos.x,s.g().player().pos.y,s.g().player().z};
  for(int z=0;z<v.nz;++z)for(int y=0;y<v.ny;++y)for(int x=0;x<v.nx;++x){const size_t i=v.index(x,y,z);if(i>=v.valid.size()||v.valid[i]!=1)continue;const Point3 p=v.position(x,y,z);
   if(std::hypot(p.x-eye.x,p.y-eye.y)>9.f)continue;const auto a=v.ambient(p);auto c=[](float f){return std::uint32_t(std::clamp(std::sqrt(std::max(0.f,f))*255.f*2.f,0.f,255.f));};
   o.point(p,(c(a[0])<<16)|(c(a[1])<<8)|c(a[2])|0x202020,2);}
  for(const auto& rp:bake->reflections){o.box(rp.lo,rp.hi,0xffe040);o.cross(rp.position,.25f,0xffe040);}
  if(s.board.count("probes.box"))for(int k=0;k<4;++k){const float x0=float(s.board["probes.x0"]),y0=float(s.board["probes.y0"]),x1=float(s.board["probes.x1"]),y1=float(s.board["probes.y1"]);
   o.lines.push_back({{x0,y0,0},{x1,y0,0},0xffffff,false});o.lines.push_back({{x1,y0,0},{x1,y1,0},0xffffff,false});o.lines.push_back({{x1,y1,0},{x0,y1,0},0xffffff,false});o.lines.push_back({{x0,y1,0},{x0,y0,0},0xffffff,false});}},
 .with={{"particles",[](Scene& s,Report& r){
  // Smoke lit by the probes: the air is drawn with them on as with them off (same sources), and still moves.
  r.check("the air still moves with the probes in",s.board["air.motion"]>.002||!s.has("particles"));}}},
 .teardown=[](Scene&){SoftwareRenderer::setLightProbes(true);},
 .needsGpu=true});
#endif
}
}

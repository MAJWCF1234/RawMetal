// Plant physbones (renderer/Foliage.h): a plant near the camera is walked through, then shot. Measured:
// how far its chain bends while someone is in it and how fast it springs back and sleeps, how it flinches
// at a round, how many plants are stepped and what that costs a frame, and, from a fixed observer, how
// many pixels the GPU bending moves (the same moment drawn with every plant held still): once for the
// wind alone, once with the plant pushed. Pictures: still / wind, still / pushed, the flinch.
//
// Fixture keys: plant-height MIN MAX (the plant to pick, nearest the camera), walk-from (frame), shoot-at (frame).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Run {int fixture=-1,slot=-1;Vec2 at{},from{},to{},eye{};float eyeZ=0,yaw=0,pitch=0,height=1,radius=.5f,base=0;
 int shotKind=-1;float shotShort=0,pushTip=0,leftAt=-1,backAt=-1,shotAt=-1,shotTip=0,shotBack=-1,windChanged=-1,pushChanged=-1,flinchChanged=-1;
 std::vector<double> costs;double ms=0,msMost=0;int frames=0,awakeMost=0,lookedMost=0;bool inside=false;};
std::map<const Scene*,Run>& runs(){static std::map<const Scene*,Run> r;return r;}
int walkFrom(const Scene& s){return int(s.fixture->number("walk-from",40));}
int shootAt(const Scene& s){return int(s.fixture->number("shoot-at",420));}
constexpr float WalkSpeed=1.2f,WalkHalf=2.4f;
// The observer's view of the plant, drawn with the plants as they are and held still: the share of pixels that differ.
double observe(Scene& s,Run& r,const char* still,const char* moving){
 Game& g=s.g();Player& p=LabAccess::player(g);const Player was=p;auto& plants=LabAccess::plants(*s.renderer);
 LabAccess::setCamera(g,r.eye,r.eyeZ,r.yaw,r.pitch);plants.still=true;s.renderer->render(g);s.shoot(still);
 const std::vector<std::uint32_t> a(s.renderer->pixels(),s.renderer->pixels()+size_t(s.renderer->width())*s.renderer->height());
 plants.still=false;s.renderer->render(g);s.shoot(moving);size_t changed=0;const auto* b=s.renderer->pixels();
 for(size_t i=0;i<a.size();++i){int d=0;for(int k=0;k<24;k+=8)d+=std::abs(int((a[i]>>k)&255)-int((b[i]>>k)&255));changed+=d>24;}
 p=was;LabAccess::setCamera(g,p.pos,p.z,p.angle,p.pitch);return a.empty()?0:100.0*double(changed)/double(a.size());}
const bool plants=add({
 .name="plants",.summary="plant physbones: wind on the GPU for every cutout plant, a spring per plant near the eye that bends away from whoever walks through it, flinches at a round and springs back (Foliage.h, scene.vert)",
 .fixtures={"plants-grove","plants-zone"},
 .setup=[](Scene& s){
  auto& r=runs()[&s];r=Run{};Game& g=s.g();const World& w=g.chunkWorld(g.level());const Player& p=g.player();
  float lo=.35f,hi=1.6f;std::sscanf(s.fixture->value("plant-height",".35 1.6").c_str(),"%f %f",&lo,&hi);float best=1e9f;
  for(size_t i=0;i<w.fixtures().size();++i){const auto& f=w.fixtures()[i];if(!f.visible||f.height<lo||f.height>hi||!LabAccess::plantModel(*s.renderer,f.model))continue;
   const float d=std::hypot(f.position.x-p.pos.x,f.position.y-p.pos.y);if(d<best&&d>2.f){best=d;r.fixture=int(i);}}
  if(r.fixture<0)return;const auto& f=w.fixtures()[size_t(r.fixture)];r.at=f.position;r.height=f.height;r.radius=std::max(f.width,f.depth)*.5f;r.base=w.floorHeight(f.position.x,f.position.y)+f.base;
  // The observer stands 4 m off the plant on the camera's side (where the fixture put the camera, when that is nearer), turned to it.
  r.eye=p.pos;if(best>6.f){const Vec2 back=normalized(p.pos-r.at);r.eye=r.at+back*4.f;best=4.f;}r.eyeZ=w.supportBelow(r.eye.x,r.eye.y,r.base+2.f);
  r.yaw=std::atan2(r.at.y-r.eye.y,r.at.x-r.eye.x);r.pitch=-std::atan2(r.eyeZ+1.42f-(r.base+r.height*.5f),best)*140.f;
  const Vec2 across{-std::sin(r.yaw),std::cos(r.yaw)};r.from=r.at-across*WalkHalf;r.to=r.at+across*WalkHalf;
  LabAccess::setCamera(g,r.eye,r.eyeZ,r.yaw,r.pitch);},
 .step=[](Scene& s,float){
  auto& r=runs()[&s];if(r.fixture<0)return;Game& g=s.g();const int since=s.frame-walkFrom(s);
  // The walker: from one side of the plant through it to the other at a walk, then he stands and shoots it.
  if(since>=0&&s.frame<shootAt(s)){const float t=std::min(1.f,float(since)/60.f*WalkSpeed/(2*WalkHalf));const Vec2 at=r.from+(r.to-r.from)*t;
   const float face=s.frame>=shootAt(s)-2?std::atan2(r.at.y-at.y,r.at.x-at.x):std::atan2(r.to.y-r.from.y,r.to.x-r.from.x);LabAccess::setCamera(g,at,g.world().supportBelow(at.x,at.y,r.base+1.f),face,-25);}
  // The round comes from 1.5 m off the plant on the observer's side.
  if(s.frame==shootAt(s)){const Vec2 gun=r.at+(r.eye-r.at)*.375f;LabAccess::setCamera(g,gun,g.world().supportBelow(gun.x,gun.y,r.base+1.f),r.yaw,r.pitch);ShotHit hit{};LabAccess::fireAt(g,{r.at.x,r.at.y,r.base+r.height*.6f},&hit);r.shotAt=s.time;r.shotKind=int(hit.kind);r.shotShort=std::hypot(r.at.x-gun.x,r.at.y-gun.y)-hit.distance;}},
 .capture=[](Scene& s,const Frame&){
  auto& r=runs()[&s];if(r.fixture<0)return;Game& g=s.g();auto& plants=LabAccess::plants(*s.renderer);
  if(r.slot<0)r.slot=plants.slotOf(g.level(),r.fixture);
  const auto& st=plants.stats();r.ms+=st.ms;if(s.frame>10){r.msMost=std::max(r.msMost,st.ms);r.costs.push_back(st.ms);}++r.frames;r.awakeMost=std::max(r.awakeMost,st.awake);r.lookedMost=std::max(r.lookedMost,st.looked);
  const Point3 tip=plants.tip(r.slot);const float bend=std::hypot(tip.x,tip.y);const Player& p=g.player();const float d=std::hypot(p.pos.x-r.at.x,p.pos.y-r.at.y);
  const bool inside=s.frame>=walkFrom(s)&&d<r.radius+.3f;if(inside)r.pushTip=std::max(r.pushTip,bend);
  if(r.inside&&!inside&&r.leftAt<0)r.leftAt=s.time;r.inside=inside;
  if(r.leftAt>=0&&r.backAt<0&&s.frame<shootAt(s)&&bend<.01f&&!plants.awake(r.slot))r.backAt=s.time;
  if(r.shotAt>=0){if(s.time-r.shotAt<.5f)r.shotTip=std::max(r.shotTip,bend);if(r.shotBack<0&&s.time-r.shotAt>.05f&&bend<.01f&&!plants.awake(r.slot))r.shotBack=s.time;}
  // The pictures (and the pixels the bending moves), from the observer.
  if(s.frame==walkFrom(s)-10)r.windChanged=observe(s,r,"held still (before)","in the wind (before)");
  const int middle=walkFrom(s)+int(WalkHalf/WalkSpeed*60.f);
  if(s.frame==middle)r.pushChanged=observe(s,r,"held still (walked through)","pushed (walked through)");
  if(s.frame==shootAt(s)+4)r.flinchChanged=observe(s,r,"held still (shot)","flinching (shot)");},
 .metrics=[](Scene& s,Report& rep){
  auto& r=runs()[&s];rep.check("a plant near the camera to walk through",r.fixture>=0&&r.slot>=0,r.fixture<0?"none":"fixture "+std::to_string(r.fixture)+" slot "+std::to_string(r.slot));if(r.fixture<0||r.slot<0)return;
  auto& plants=LabAccess::plants(*s.renderer);
  rep.metric("plant_height_m",r.height,"m");rep.metric("plants",plants.stats().plants);rep.metric("awake_most",r.awakeMost);rep.metric("looked_most",r.lookedMost);
  rep.metric("update_ms_mean",r.frames?r.ms/r.frames:0,"ms");rep.metric("update_ms_most",r.msMost,"ms");
  rep.metric("push_tip_m",r.pushTip,"m");rep.metric("spring_back_s",r.backAt>=0&&r.leftAt>=0?r.backAt-r.leftAt:-1,"s");rep.metric("flinch_tip_m",r.shotTip,"m");rep.metric("shot_hit_kind",r.shotKind);rep.metric("shot_stopped_short_m",r.shotShort,"m");rep.metric("flinch_back_s",r.shotBack>=0?r.shotBack-r.shotAt:-1,"s");
  rep.metric("wind_pixels_pct",r.windChanged,"%");rep.metric("push_pixels_pct",r.pushChanged,"%");rep.metric("flinch_pixels_pct",r.flinchChanged,"%");
  rep.check("walked through, it bends away (tip 15% of its height or more)",r.pushTip>=.15f*r.height,fixed(r.pushTip,2)+" m of "+fixed(r.height,2)+" m");
  rep.check("left alone, it springs back and sleeps within 2.5 s",r.backAt>=0&&r.leftAt>=0&&r.backAt-r.leftAt<=2.5f,r.backAt<0?"still moving":fixed(r.backAt-r.leftAt,2)+" s");
  rep.check("a round through it makes it flinch (tip 4 cm or more)",r.shotTip>=.04f,fixed(r.shotTip*100,1)+" cm");
  rep.check("and it settles again within 3 s",r.shotBack>=0&&r.shotBack-r.shotAt<=3.f,r.shotBack<0?"still moving":fixed(r.shotBack-r.shotAt,2)+" s");
  rep.check("the wind moves the plants on screen",r.windChanged>.05,fixed(r.windChanged,2)+"% of pixels");
  rep.check("the push moves more of it than the wind alone",r.pushChanged>r.windChanged,fixed(r.pushChanged,2)+"% vs "+fixed(r.windChanged,2)+"%");
  std::sort(r.costs.begin(),r.costs.end());const double p95=r.costs.empty()?0:r.costs[size_t(double(r.costs.size()-1)*.95)];rep.metric("update_ms_p95",p95,"ms");
  rep.check("stepping the plants costs under 0.15 ms a frame (mean) and 0.3 ms (95th percentile)",r.frames&&r.ms/r.frames<.15&&p95<.3,fixed(r.frames?r.ms/r.frames:0,3)+" / "+fixed(p95,3)+" ms");},
 .reporter=[](Scene& s,Overlay& o){auto& r=runs()[&s];if(r.fixture<0)return;const Point3 root{r.at.x,r.at.y,r.base};auto& plants=LabAccess::plants(*s.renderer);const Point3 tip=plants.tip(r.slot);
  o.line(root,{root.x+tip.x,root.y+tip.y,root.z+r.height},plants.awake(r.slot)?0xffa020:0x40ff60);o.label({root.x,root.y,root.z+r.height+.2f},plants.awake(r.slot)?"AWAKE":"ASLEEP",0xffffff);},
 .teardown=[](Scene& s){LabAccess::plants(*s.renderer).still=false;runs().erase(&s);},.needsGpu=true});
}
}

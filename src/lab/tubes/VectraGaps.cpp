// The gaps a review left between Vectra and Half-Life: Alyx, each closed and checked on the bare core (no game).
// vectra-gaps.txt says what each did.
//  joint friction and detents: a cabinet door nudged on its hinge stops where it is left (and glides on without the
//    friction); a drawer pushed nearly shut is caught and latched shut, a gentle pull leaves it, a firm one opens it.
//  seams (in the game's World): a crate and a bottle slid across the joins of a platform's boxes and onto a ramp's foot
//    neither rise off what is under them nor gain vertical speed past the slope's (RM_GAPS=seams runs this part alone,
//    RM_GAPS_TRACE=1 prints the ramp runs frame by frame, 2 with their world manifolds).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropBodies.h"
#include "game/physics/Vectra.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include "world/Primitives.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
namespace retro::lab {
namespace {
using namespace vectra;
std::string f3(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// A space with a floor at z = 0.
struct World {
 Space space;std::deque<Shape> shapes;std::vector<std::unique_ptr<Joint>> joints;
 World(){space.worldContacts=[](Space& sp,int i,float margin,std::vector<Manifold>& out){const Body& body=sp.bodies[size_t(i)];
   for(int part=0;part<body.shape->count;++part){std::array<V3,8> f{};const int k=supportFeature(body,part,{0,0,-1},f);Manifold m;m.a=i;m.b=-1;m.partA=part;m.normal={0,0,-1};m.material=Material::mix(body.material,{.6f,.5f,0,.004f});
    for(int j=0;j<k&&m.count<4;++j){const float sep=f[size_t(j)].z;if(sep>margin)continue;auto& c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=f[size_t(j)];c.separation=sep;c.localA=body.q.unrotate(c.p-body.x);c.localB=c.p;c.feature=std::uint32_t(j);}
    if(m.count)out.push_back(m);}};}
 int box(V3 at,V3 half,float mass,Quat q={}){shapes.emplace_back();Shape& s=shapes.back();s.add(PartKind::Box,{},half);s.finish();Body b;b.shape=&s;b.q=q;b.x=at;b.setMass(mass);b.updateInertia();b.material={.6f,.5f,.2f,.002f};b.id=int(space.bodies.size());space.bodies.push_back(b);return int(space.bodies.size())-1;}
 Joint& add(JointKind kind,int a,int b){joints.push_back(std::make_unique<Joint>());auto& j=*joints.back();j.kind=kind;j.a=a;j.b=b;space.constraints.push_back(&j);return j;}
};
using Check=std::function<void(bool,const std::string&)>;
// ---- Seams: props slid across the world's joins (game: the World's own contacts, RigidClutter.cpp) ----
// A platform of three 1 m STRUCT boxes in a row (their tops one plane, 0.3 m up), and a ramp rising 0.6 m over 3 m to +y
// from the floor. A crate and a bottle on its side (its axis along the way it goes) are slid along the platform across
// both joins, and up the ramp's foot. Each frame: the most it rises (its lowest point over what is under it) and its
// vertical speed past what the surface under it asks (on the ramp, its speed up the slope times the slope).
void seams(std::ostream& out,const Check& check){
 constexpr float RampY0=10.5f,RampRun=3,RampRise=.6f;
 auto slopeAt=[&](float y){return std::clamp((y-RampY0)/RampRun,0.f,1.f)*RampRise;};
 struct Run {float rise=0,vz=0,went=0;};
 auto slide=[&](int model,float scale,bool ramp,bool lying)->Run{
  auto m=regression::room();m->definition.playerStart={3,3};
  for(int k=0;k<3;++k)m->structures.push_back({6.f+float(k),9.4f,7.f+float(k),10.6f,0,.3f,false,40});
  expandPrimitive({"ramp",14,RampY0+RampRun*.5f,0,1.6f,RampRun,RampRise,0,40,true},m->structures);
  m->structures.push_back({13.2f,RampY0+RampRun,14.8f,RampY0+RampRun+2,0,RampRise,false,40});
  const float pitch=lying?kPi*.5f:0.f;
  if(ramp)kit::propPose(*m,model,14,9.7f,0,lying?kPi*.5f:0.f,pitch,0,scale);else kit::propPose(*m,model,6.35f,10,.3f,0,pitch,0,scale);
  Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};auto& list=LabAccess::clutter(g);Run r;
  int prop=-1;for(size_t i=0;i<list.size();++i)if(list[i].model==model)prop=int(i);if(prop<0)return {1e9f,1e9f,0};
  // Settle a moment, then send it off.
  for(int i=0;i<20;++i)g.update({},1.f/60);
  {auto& c=list[size_t(prop)];c.velocity=ramp?Vec2{0,4.5f}:Vec2{5.5f,0};c.vz=0;c.spin=c.pitchSpeed=c.rollSpeed=0;c.sleeping=false;c.restTime=0;}
  const Vec2 start=list[size_t(prop)].pos;
  for(int i=0;i<90;++i){g.update({},1.f/60);const auto& c=list[size_t(prop)];vectra::Body b;vectra::toBody(c,b);V3 low{0,0,1e9f};for(int p=0;p<b.shape->count;++p){const V3 q=vectra::support(b,p,{0,0,-1});if(q.z<low.z)low=q;}
   if(!ramp){if(c.pos.x>8.8f||c.pos.x<6.2f)break;   // on the platform, short of its end
    r.rise=std::max(r.rise,low.z-.3f);r.vz=std::max(r.vz,std::fabs(c.vz));}
   else{if(c.pos.y>RampY0+RampRun-.3f)break;    // short of the crest (over it, a thing going up is thrown, as it should be)
    r.rise=std::max(r.rise,low.z-slopeAt(low.y));   // its lowest point over the true slope (or floor) under it
    const float onSlope=c.pos.y+c.extent()[1]>RampY0?RampRise/RampRun*c.velocity.y:0.f;   // once its front is on the slope
    r.vz=std::max(r.vz,c.vz-onSlope);
    if(std::getenv("RM_GAPS_TRACE"))out<<"  y "<<f3(c.pos.y)<<" z "<<f3(c.z)<<" low "<<f3(low.y)<<","<<f3(low.z*1000,1)<<" slope "<<f3(slopeAt(low.y)*1000,1)<<" vy "<<f3(c.velocity.y)<<" vz "<<f3(c.vz)<<" roll "<<f3(c.roll)<<" pitch "<<f3(c.pitch)<<'\n';
    if(std::getenv("RM_GAPS_TRACE")&&std::string(std::getenv("RM_GAPS_TRACE"))=="2")for(const auto& m:LabAccess::rigidStats(g).manifoldsSeen){if(m.count==0||m.b>=0)continue;
     out<<"    M g"<<m.partB<<" n "<<f3(m.normal.x,2)<<","<<f3(m.normal.y,2)<<","<<f3(m.normal.z,2);for(int k=0;k<m.count;++k)out<<" ("<<f3(m.points[size_t(k)].p.y,3)<<","<<f3(m.points[size_t(k)].p.z,3)<<" s"<<f3(m.points[size_t(k)].separation*1000,1)<<" i"<<f3(m.points[size_t(k)].normalImpulse,2)<<")";out<<'\n';}}}
  r.went=length(list[size_t(prop)].pos-start);return r;};
 const Run crate=slide(model::wooden_crate,.5f,false,false),bottle=slide(model::glass_bottle,1,false,true),crateUp=slide(model::wooden_crate,.5f,true,false),bottleUp=slide(model::glass_bottle,1,true,true);
 out<<"across the platform's two joins: the crate rose "<<f3(crate.rise*1000,2)<<" mm, its vertical speed at most "<<f3(crate.vz,3)<<" m/s ("<<f3(crate.went,2)<<" m); the bottle "<<f3(bottle.rise*1000,2)<<" mm, "<<f3(bottle.vz,3)<<" m/s ("<<f3(bottle.went,2)<<" m)\n";
 out<<"up the ramp's foot: the crate's lowest point rose "<<f3(crateUp.rise*1000,2)<<" mm over the slope, its vertical speed past the slope's at most "<<f3(crateUp.vz,3)<<" m/s ("<<f3(crateUp.went,2)<<" m); the bottle "<<f3(bottleUp.rise*1000,2)<<" mm, "<<f3(bottleUp.vz,3)<<" m/s ("<<f3(bottleUp.went,2)<<" m)\n";
 check(crate.rise<.004f&&crate.vz<.08f&&bottle.rise<.004f&&bottle.vz<.08f&&crate.went>1.7f&&bottle.went>1.7f,"seams: a crate and a bottle slid across the joins of a platform's boxes do not jump");
 check(crateUp.rise<.01f&&crateUp.vz<.15f&&bottleUp.rise<.01f&&bottleUp.vz<.15f&&crateUp.went>1.1f&&bottleUp.went>1.1f,"seams: a crate and a bottle slid onto a ramp's foot go up it without jumping");}
const bool registered=add({.name="vectra-gaps",.summary="Vectra against Alyx's bar: hinge and slider friction and detents, seams",.standalone=[]{
 std::ofstream out("vectra-gaps.txt");bool ok=true;
 auto check=[&](bool pass,const std::string& what){out<<(pass?"PASS ":"FAIL ")<<what<<'\n';out.flush();ok&=pass;};
 const char* only=std::getenv("RM_GAPS");auto want=[&](const char* part){return !only||std::string(only).find(part)!=std::string::npos;};
 if(want("seams")){out<<"---- seams ----\n";seams(out,check);}
 if(want("joints")){
 // ---- Friction and detents on hinges and sliders ----
 out<<"---- joint friction and detents ----\n";
 for(float hz:{60.f,120.f}){const float dt=1/hz;auto steps=[&](float s){return int(s*hz+.5f);};const std::string at=" @"+std::to_string(int(hz))+" Hz";
  // A 6 kg cabinet door, 0.5 m wide and 0.8 high, on an upright pin at its edge (stops at +-100 degrees), nudged at
  // 1.2 rad/s: with 1.5 N m of friction on its pin, and with none.
  float stopped[2]{},later[2]{};
  for(int f=0;f<2;++f){World w;const int d=w.box({.25f,0,.5f},{.25f,.01f,.4f},6);auto& j=w.add(JointKind::Hinge,d,-1);j.attach(w.space,{0,0,.5f},{0,0,1});j.limited=true;j.lower=-1.75f;j.upper=1.75f;
   if(f==0)j.frictionTorque=1.5f;w.space.bodies[size_t(d)].w={0,0,1.2f};w.space.bodies[size_t(d)].v={0,.3f,0};   // turning about its pin
   for(int i=0;i<steps(1.5f);++i)w.space.step(dt);stopped[f]=std::fabs(j.angle(w.space));for(int i=0;i<steps(1.5f);++i)w.space.step(dt);later[f]=std::fabs(j.angle(w.space));}
  out<<"a cabinet door nudged at 1.2 rad/s: with friction it turned "<<f3(stopped[0]*57.3,1)<<" deg and stayed there ("<<f3((later[0]-stopped[0])*57.3,3)<<" deg more in 1.5 s); without, "<<f3(later[1]*57.3,1)<<" deg\n";
  check(stopped[0]>.1f&&stopped[0]<.45f&&std::fabs(later[0]-stopped[0])<.002f&&later[1]>1.5f,"hinge friction: a cabinet door nudged gently stops where it is left; without friction it glides to its stop"+at);
  // A 5 kg drawer on a level rail (its axis the way it slides shut; travel 0 shut, out to 0.35 m) with 4 N of friction,
  // 12 cm out and pushed shut at 0.41 m/s: friction alone stops it about 1.5 cm short; a detent at 0 (catch 2.5 cm)
  // pulls it shut. Then a 20 N pull for a second leaves it shut (its latch holds 60 N); a 150 N pull opens it.
  float shut[2]{};float gentle=0,firm=0;
  for(int f=0;f<2;++f){World w;const int d=w.box({0,0,1},{.2f,.25f,.075f},5);auto& j=w.add(JointKind::Slider,d,-1);j.attach(w.space,{0,0,1},{1,0,0});j.limited=true;j.lower=0;j.upper=.35f;j.frictionTorque=4;
   if(f==0){j.detents[0]=0;j.detentCount=1;j.detentRange=.025f;j.detentForce=60;}
   Body& b=w.space.bodies[size_t(d)];b.x.x=-.12f;b.v={.41f,0,0};
   for(int i=0;i<steps(2);++i)w.space.step(dt);shut[f]=j.angle(w.space);
   if(f==0){for(int i=0;i<steps(1);++i){{auto& B=w.space.bodies[size_t(d)];B.force={-20,0,0};B.sleeping=false;B.sleepTime=0;}w.space.step(dt);gentle=std::max(gentle,j.angle(w.space));}
    for(int i=0;i<steps(1);++i){{auto& B=w.space.bodies[size_t(d)];B.force={-150,0,0};B.sleeping=false;B.sleepTime=0;}w.space.step(dt);}firm=j.angle(w.space);}}
  out<<"a drawer pushed nearly shut: with a detent it ends "<<f3(shut[0]*1000,2)<<" mm out, without "<<f3(shut[1]*1000,2)<<" mm; a 20 N pull draws it "<<f3(gentle*1000,2)<<" mm, a 150 N pull "<<f3(firm*1000,1)<<" mm\n";
  check(std::fabs(shut[0])<.002f&&shut[1]>.006f,"slider detent: a drawer pushed closed is caught and latches shut (friction alone leaves it short)"+at);
  check(gentle<.008f&&firm>.05f,"slider latch: a gentle pull leaves a latched drawer shut, a firm one takes it out"+at);}}
 return ok;}});
}
}

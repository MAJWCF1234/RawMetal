// The gaps a review left between Vectra and Half-Life: Alyx, each closed and checked on the bare core (no game).
// vectra-gaps.txt says what each did.
//  joint friction and detents: a cabinet door nudged on its hinge stops where it is left (and glides on without the
//    friction); a drawer pushed nearly shut is caught and latched shut, a gentle pull leaves it, a firm one opens it.
//  seams (in the game's World): a crate and a bottle slid across the joins of a platform's boxes and onto a ramp's foot
//    neither rise off what is under them nor gain vertical speed past the slope's (RM_GAPS=seams runs this part alone,
//    RM_GAPS_TRACE=1 prints the ramp runs frame by frame, 2 with their world manifolds).
//  water (in the game): a sealed drum dropped in upright bobs, tips onto its side and settles; a crate rides at its
//    density's draft; a plank dropped in on its end ends lying flat (before: the drum stayed upright, the plank on end).
//  breaking (in the game): a crate smashed by a fast blow throws its planks along it; a spinning crate's pieces go on at
//    their places' velocities; a board nailed across a doorway holds a nudge and snaps under a heavy shove.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/Buoyancy.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropBodies.h"
#include "game/physics/Vectra.h"
#include "world/CampaignKit.h"
#include "world/EntityTraits.h"
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
// ---- Water (game: RigidClutter.cpp's water on Vectra bodies) ----
// A deep pool (x 3-11, y 6-14, bed 1.15 m under the surface). A sealed drum (density 220) dropped in nearly upright bobs,
// tips onto its side and settles there; a wooden crate rides at its density's draft; a 2 m plank dropped standing on end
// ends lying flat.
void water(std::ostream& out,const Check& check){
 constexpr float Bed=-1.2f,Surface=-.05f;
 auto pool=[&]{auto m=regression::room();m->definition.playerStart={2,2};m->waterVolumes.push_back({3,6,11,14,Bed,Surface});return regression::campaign(m);};
 auto make=[](int model,Vec2 at,float z){Clutter c;c.kind=7;c.model=model;c.dims=Game::propTraits(model).size;c.pos=at;c.z=z;c.sleeping=false;return c;};
 auto up=[](const Clutter& c){vectra::Body b;vectra::toBody(c,b);return std::fabs(b.q.rotate({0,0,1}).z);};   // its own z against the world's
 struct Run {float tilt=0,bob=0,settleSpin=0,settleVz=0,draft=0;bool afloat=false;};
 auto drop=[&](Clutter c,float seconds)->Run{Game g(pool());LabAccess::player(g).pos={2,2};LabAccess::clutter(g)={c};Run r;float lo=1e9f,hi=-1e9f,entered=-1;
  for(int i=0;i<int(seconds*60);++i){g.update({},1.f/60);const auto& d=LabAccess::clutter(g)[0];const float t=float(i+1)/60;
   if(entered<0&&d.z<Surface)entered=t;if(entered>=0&&t<entered+2){lo=std::min(lo,d.z);hi=std::max(hi,d.z);}
   if(t>seconds-1){r.settleSpin=std::max(r.settleSpin,len(vectra::propAngular(d)));r.settleVz=std::max(r.settleVz,std::fabs(d.vz));}}
  const auto& d=LabAccess::clutter(g)[0];r.tilt=std::acos(std::clamp(up(d),0.f,1.f));r.bob=hi-lo;r.draft=(Surface-d.z)/std::max(.01f,d.extent()[2]*2);r.afloat=d.z<Surface&&d.z+d.extent()[2]*2>Surface;return r;};
 // The drum: sealed and empty (density 220), dropped 30 cm over the surface leaning 5 degrees.
 Clutter drum=make(model::ind_barrel,{7,10},Surface+.3f);{auto t=std::make_shared<EntityTraits>();t->density=220;t->floats=1;drum.traits=t;}drum.roll=.09f;
 const Run d=drop(drum,8);
 out<<"a sealed drum dropped in upright: it went "<<f3(d.bob*100,1)<<" cm up and down in its first 2 s in, ended "<<f3(d.tilt*57.3f,1)<<" deg over (on its side is 90), afloat "<<d.afloat<<"; in its last second it turned at most "<<f3(d.settleSpin,3)<<" rad/s and rose or fell at most "<<f3(d.settleVz,3)<<" m/s\n";
 check(d.afloat&&d.bob>.03f&&d.tilt>1.3f&&d.settleSpin<.5f&&d.settleVz<.08f,"water: a drum dropped in bobs, tips onto its side and settles there afloat");
 Clutter crate=make(model::wooden_crate,{7,10},Surface+.3f);const float density=buoyancy::density(crate);const Run c=drop(crate,8);
 out<<"a wooden crate: draft "<<f3(c.draft,3)<<" of its height (its density "<<f3(density,0)<<" says "<<f3(density/1000,3)<<"), "<<f3(c.tilt*57.3f,1)<<" deg off a face\n";
 check(c.afloat&&std::fabs(c.draft-density/1000)<.1f&&c.tilt<.15f,"water: a crate floats level at the depth its density says");
 Clutter plank=make(model::scrap_bridge_plank,{7,10},Surface+.2f);plank.roll=kPi*.5f-.15f;   // its 2 m length nearly upright
 const Run p=drop(plank,10);
 out<<"a 2 m plank dropped in standing on its end: it ends "<<f3(p.tilt*57.3f,1)<<" deg off lying flat, afloat "<<p.afloat<<"\n";
 check(p.afloat&&p.tilt<.2f,"water: a plank dropped in on its end ends lying flat");}
// ---- Breaking (game: Game::breakClutter, PropJoints.cpp) ----
// A wooden crate (it has authored gibs) struck hard at its top edge and broken: its planks go off along the blow. A
// wooden crate spun at 8 rad/s and broken where it stands: each piece goes on at the velocity its place had (the side at
// -y going +x, the side at +y going -x), not popping out alike. A board nailed across a doorway (a weld to the room,
// breaks 600 N): a 2 kg box at 1 m/s leaves it; the 30 kg crate at 6 m/s tears it off.
void breaking(std::ostream& out,const Check& check){
 auto heavy=[](Clutter& c,float mass){auto t=std::make_shared<EntityTraits>();t->mass=mass;c.traits=t;};
 auto pieces=[](const std::vector<Clutter>& list,size_t from){std::vector<const Clutter*> p;for(size_t i=from;i<list.size();++i)if(!list[i].broken)p.push_back(&list[i]);return p;};
 // The smash: a 50 N s blow (a sledgehammer's, a shotgun's at the muzzle) on a wooden crate's face at its top edge, as a
 // melee hit or a round gives it (knockClutter), then broken by it (damageClutter's break): its planks go on along the
 // blow, the top ones faster (the blow's turn).
 {auto m=regression::room();m->definition.playerStart={2,2};kit::prop(*m,model::wooden_crate,10,10,0,0,.6f);Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};auto& list=LabAccess::clutter(g);
  for(int i=0;i<10;++i)g.update({},1.f/60);const auto c=list[0];const size_t whole=list.size();const float top=c.z+c.height();
  LabAccess::knock(g,0,{c.pos.x-c.extent()[0],c.pos.y,top-.02f},{50,0,0});const float speed=list[0].velocity.x;LabAccess::breakClutter(g,0,{1,0,0});
  V3 mean{};int n=0;float high=0,low=0;int nh=0,nl=0;const float mid=c.z+c.height()*.5f;
  for(const auto* q:pieces(list,whole)){mean=mean+V3{q->velocity.x,q->velocity.y,0};++n;if(q->z+q->extent()[2]>mid+.03f){high+=q->velocity.x;++nh;}else if(q->z+q->extent()[2]<mid-.03f){low+=q->velocity.x;++nl;}}
  if(n)mean=mean*(1.f/float(n));if(nh)high/=float(nh);if(nl)low/=float(nl);
  out<<"a 50 N s blow on a wooden crate's top edge ("<<f3(c.mass(),1)<<" kg, its middle sent at "<<f3(speed,2)<<" m/s) and it broken: "<<n<<" pieces going on at "<<f3(mean.x,2)<<","<<f3(mean.y,2)<<" m/s on average, the top ones "<<f3(high,2)<<", the bottom ones "<<f3(low,2)<<"\n";
  check(n>=3&&mean.x>speed*.8f&&std::fabs(mean.y)<.5f*mean.x&&high>low+.5f,"breaking: a crate smashed by a fast blow throws its planks along the blow, the struck side's fastest");}
 // The spin.
 {auto m=regression::room();m->definition.playerStart={2,2};kit::prop(*m,model::wooden_crate,10,10,0,0,.6f);Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};auto& list=LabAccess::clutter(g);
  for(int i=0;i<10;++i)g.update({},1.f/60);auto& c=list[0];vectra::setPropAngular(c,{0,0,8});c.velocity={};c.vz=0;c.sleeping=false;const size_t whole=list.size();const Vec2 at=c.pos;const float depth=c.dims[1];
  LabAccess::breakClutter(g,0,{0,0,0});float minus=0,plus=0;int nm=0,np=0;
  for(const auto* q:pieces(list,whole)){if(q->pos.y<at.y-.08f){minus+=q->velocity.x;++nm;}else if(q->pos.y>at.y+.08f){plus+=q->velocity.x;++np;}}
  if(nm)minus/=float(nm);if(np)plus/=float(np);
  out<<"a wooden crate spun at 8 rad/s and broken: its pieces at -y go "<<f3(minus,2)<<" m/s along x, at +y "<<f3(plus,2)<<" (the spin gives them about +-"<<f3(8*depth*.3f,2)<<")\n";
  check(nm>0&&np>0&&minus-plus>1.5f,"breaking: the pieces of a spinning crate go on at the velocity their place had");}
 // The nailed board.
 float pull[2]{};bool snapped[2]{};
 for(int heavyRam=0;heavyRam<2;++heavyRam){auto m=regression::room();m->definition.playerStart={2,2};
  kit::prop(*m,model::scrap_bridge_plank,10,10,.05f,1.2f,.03f,.15f,0);   // the board, standing across x
  PropJoint j;j.kind=PropJointKind::Weld;j.propA=0;j.propB=-1;j.at={10,10,.125f};j.to=j.at;j.breakForce=600;m->joints.push_back(j);
  kit::prop(*m,heavyRam?model::wooden_crate:model::prop_cardboard_box,10,8.6f,0,0,heavyRam?.5f:.4f);
  Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};auto& list=LabAccess::clutter(g);for(int i=0;i<10;++i)g.update({},1.f/60);
  auto& r=list[1];heavy(r,heavyRam?30.f:2.f);r.velocity={0,heavyRam?6.f:1.f};r.sleeping=false;r.restTime=0;
  for(int i=0;i<90;++i){g.update({},1.f/60);const auto& st=g.world().jointStates();if(!st.empty()){pull[heavyRam]=std::max(pull[heavyRam],st[0].force);snapped[heavyRam]|=st[0].broken;}}}
 out<<"a board nailed across a doorway (breaks at 600 N): a 2 kg box at 1 m/s pulled it with "<<f3(pull[0],0)<<" N, it "<<(snapped[0]?"snapped":"held")<<"; a 30 kg crate at 6 m/s: "<<(snapped[1]?"it snapped":"it held, "+f3(pull[1],0)+" N")<<"\n";
 check(!snapped[0]&&snapped[1],"breaking: a nailed board holds against a nudge and snaps off under a heavy shove");}
const bool registered=add({.name="vectra-gaps",.summary="Vectra against Alyx's bar: hinge and slider friction and detents, seams, water, breaking",.standalone=[]{
 std::ofstream out("vectra-gaps.txt");bool ok=true;
 auto check=[&](bool pass,const std::string& what){out<<(pass?"PASS ":"FAIL ")<<what<<'\n';out.flush();ok&=pass;};
 const char* only=std::getenv("RM_GAPS");auto want=[&](const char* part){return !only||std::string(only).find(part)!=std::string::npos;};
 if(want("seams")){out<<"---- seams ----\n";seams(out,check);}
 if(want("breaking")){out<<"---- breaking ----"<<std::endl;breaking(out,check);}
 if(want("water")){out<<"---- water ----\n";water(out,check);}
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

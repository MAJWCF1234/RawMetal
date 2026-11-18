// The review's fixes to Vectra, each on the bare core (planes for the world) where it can be, and in the game
// where the game is what changed. Each writes <tube>.txt with its numbers.
//  vectra-drive: a 2 m plank driven (Space::drive) to turn its end through a doorframe's jamb: capped, it is
//    levered no harder than it is pulled and rests against the jamb, still; set outright (the old drive), it shook
//    against it or went through.
//  vectra-grip: the same crate shoved at 3 m/s across concrete, clinic tile, carpet and a puddle (World surface
//    materials, mixed with the prop's in Space::prepare): it slides furthest on the wet.
//  vectra-stack: a 100 kg drum set on a 1 kg crate, and dropped on it: with mass scaling on resting contacts
//    (Settings::shockNormal) it stays put on the crate, neither sinking into it nor springing off it; without, the
//    numbers it had.
//  vectra-scale: 400 resting props and 20 fast shards through them: the cost of a step.
//  vectra-scrape: a crate shoved across the floor raises its scrape level (Clutter::scrapeLevel, the AudioEngine's
//    loop) while it slides, silent once it rests; a drum rolled along the floor raises its roll level instead.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/Vectra.h"
#include "world/ModelIds.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <string>
namespace retro::lab {
namespace {
using namespace vectra;
std::string f3(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// A world of planes: each keeps bodies on the side its normal points to (dot(n, p) >= d).
struct Plane {V3 n;float d;Material material{.6f,.5f,0,.004f};};
struct World {
 Space space;std::deque<Shape> shapes;std::vector<Plane> planes;
 World(){planes.push_back({{0,0,1},0});
  space.worldContacts=[this](Space& sp,int i,float margin,std::vector<Manifold>& out){const Body& body=sp.bodies[size_t(i)];
   for(int pi=0;pi<int(planes.size());++pi){const Plane& pl=planes[size_t(pi)];
    for(int part=0;part<body.shape->count;++part){std::array<V3,8> f{};const int k=supportFeature(body,part,neg(pl.n),f);Manifold m;m.a=i;m.b=-1;m.partA=part;m.partB=pi;m.normal=neg(pl.n);
     m.material=Material::mix(body.material,pl.material);
     for(int j=0;j<k&&m.count<4;++j){const float sep=dot(pl.n,f[size_t(j)])-pl.d;if(sep>margin)continue;auto& c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=f[size_t(j)];c.separation=sep;c.localA=body.q.unrotate(c.p-body.x);c.localB=c.p;c.feature=std::uint32_t(j);}
     if(m.count)out.push_back(m);}}};}
 int add(PartKind kind,V3 at,V3 half,float mass,Quat q={},Material mat={.6f,.5f,.2f,.002f}){shapes.emplace_back();Shape& s=shapes.back();s.add(kind,{},half);s.finish();Body b;b.shape=&s;b.q=q;b.x=at;b.setMass(mass);b.updateInertia();b.material=mat;b.id=int(space.bodies.size());space.bodies.push_back(b);return int(space.bodies.size())-1;}
 // How far the body's part reaches past a plane (into it): its deepest point's depth, 0 when clear.
 float into(int i,int plane)const{const Body& b=space.bodies[size_t(i)];const Plane& pl=planes[size_t(plane)];float worst=0;for(int p=0;p<b.shape->count;++p)worst=std::max(worst,pl.d-dot(pl.n,support(b,p,neg(pl.n))));return worst;}
};
struct Out {std::ofstream file;bool ok=true;explicit Out(const char* name):file(name){}
 void check(bool pass,const std::string& what){file<<(pass?"PASS ":"FAIL ")<<what<<'\n';file.flush();ok&=pass;}};

// ---- 1. The drive's turn held to a torque ----
struct DriveRun {float into=0,shake=0,meanSpin=0,flips=0;};
DriveRun drivePlank(float maxTorque){
 World w;w.planes.push_back({{-1,0,0},-.6f});   // the jamb: nothing past x = 0.6
 const int p=w.add(PartKind::Box,{0,0,1.2f},{1.f,.1f,.025f},6,Quat::axisAngle({0,0,1},kPi*.5f));
 const float dt=1.f/120,mass=6;DriveRun r;float lastZ=0;int samples=0;
 for(int i=0;i<int(3/dt);++i){
  // Held at the hand as Physgloves would: 8 Hz both ways, 60 m/s^2 of pull on its mass.
  w.space.drive(p,{0,0,1.2f},Quat{},8,8,dt,mass*60,maxTorque);w.space.step(dt);
  if(i*dt>2){const Body& b=w.space.bodies[size_t(p)];r.into=std::max(r.into,w.into(p,1));r.shake=std::max(r.shake,len(b.w));r.meanSpin+=len(b.w);++samples;
   if(b.w.z*lastZ<0&&std::fabs(b.w.z)>.02f)++r.flips;if(std::fabs(b.w.z)>.02f)lastZ=b.w.z;}}
 r.meanSpin/=float(std::max(1,samples));return r;}

const bool driveTube=add({.name="vectra-drive",.summary="Space::drive holds the turn it asks for to a torque: a driven plank levered into a doorframe rests against it, still, instead of shaking or going through",.standalone=[]{
 Out out("vectra-drive.txt");
 const DriveRun capped=drivePlank(-1),old=drivePlank(1e30f);
 for(auto [name,r]:{std::pair{"capped (maxForce times its reach)",capped},std::pair{"set outright (the old drive)",old}})
  out.file<<name<<": into the jamb "<<f3(r.into*1000,2)<<" mm, spin in the last second most "<<f3(r.shake,3)<<" rad/s, mean "<<f3(r.meanSpin,4)<<", turned back "<<r.flips<<" times\n";
 out.check(capped.into<.003f,"a driven plank pressed into a jamb stays out of it (under 3 mm)");
 out.check(capped.shake<.1f&&capped.flips<=2,"and rests against it without shaking (spin under 0.1 rad/s, no back-and-forth)");
 return out.ok;}});

// ---- 2. The floor's own material ----
struct Slide {float distance=0,time=0;bool asleep=false;};
Slide slideOn(int floor){
 auto m=regression::room();m->definition.playerStart={2,2};
 if(floor==1)m->floorSurface=SurfaceClinicTile;
 if(floor==2)m->definition.floorMaterial=FloorMaterial::OfficeCarpet;
 if(floor==3)m->wetPatches.push_back({4,6,20,10,1,true,false});
 Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};
 Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims=Game::propTraits(c.model).size;c.pos={6,8};c.z=0;c.sleeping=false;c.velocity={3,0};
 LabAccess::clutter(g)={c};Slide r;
 for(int i=0;i<240;++i){g.update({},1.f/60);const auto& k=g.clutter()[0];r.distance=k.pos.x-6;if(k.sleeping&&!r.asleep){r.asleep=true;r.time=float(i+1)/60;}}
 return r;}
const bool gripTube=add({.name="vectra-grip",.summary="the floor's own material against a prop's (World surface materials into Space::prepare): a shoved crate slides further on a puddle than on concrete",.standalone=[]{
 Out out("vectra-grip.txt");const char* names[]={"concrete","clinic tile","carpet","a puddle"};Slide s[4];
 for(int f=0;f<4;++f){s[f]=slideOn(f);out.file<<names[f]<<": slid "<<f3(s[f].distance,3)<<" m"<<(s[f].asleep?", at rest at "+f3(s[f].time,2)+" s":", still going")<<"\n";}
 out.check(s[3].distance>s[0].distance*1.4f,"a crate shoved at 3 m/s slides further on a puddle than on concrete (by 40%)");
 out.check(s[2].distance<s[0].distance,"and less far on carpet");
 out.check(s[0].asleep&&s[3].asleep,"and comes to rest on both");
 return out.ok;}});

// ---- 4. A heavy body on a light one ----
struct Stack {float sink=0,lift=0,bounce=0,hop=0,drift=0,floorSink=0;bool asleep=false;};
Stack drumOnCrate(float shock,float drop){
 World w;w.space.settings.shockNormal=shock;
 const int crate=w.add(PartKind::Box,{0,0,.2f},{.2f,.2f,.2f},1,{},{.62f,.45f,.3f,.002f});
 const int drum=w.add(PartKind::Cylinder,{0,0,.4f+.44f+drop},{.29f,.29f,.44f},100,{},{.45f,.38f,.32f,.001f});
 Stack r;const float dt=1.f/120;bool landed=drop<=0;
 for(int i=0;i<int(3/dt);++i){w.space.step(dt);const Body& c=w.space.bodies[size_t(crate)];const Body& d=w.space.bodies[size_t(drum)];
  const float gap=(d.x.z-.44f)-(c.x.z+.2f);
  if(!landed&&gap<.002f)landed=true;
  if(landed){r.sink=std::max(r.sink,-gap);if(i*dt>.5f)r.lift=std::max(r.lift,gap);r.bounce=std::max(r.bounce,d.v.z);r.hop=std::max(r.hop,gap);}
  if(std::getenv("RM_STACK_TRACE")&&drop>0&&i<60){static std::ofstream tr("vectra-stack-trace.txt");tr<<(shock>0?"shock ":"plain ")<<i<<" gap "<<f3(gap*1000,2)<<" drum vz "<<f3(d.v.z,3)<<" crate z "<<f3(c.x.z,4)<<" vz "<<f3(c.v.z,3)<<"\n";}
  r.floorSink=std::max(r.floorSink,w.into(crate,0));r.drift=std::max(r.drift,std::hypot(d.x.x,d.x.y));}
 r.asleep=w.space.bodies[size_t(drum)].sleeping;return r;}
const bool stackTube=add({.name="vectra-stack",.summary="a 100 kg drum on a 1 kg crate stays put (the shock pass: Settings::shockNormal), set down or dropped on it",.standalone=[]{
 Out out("vectra-stack.txt");Stack s[2][2];
 for(int shock=0;shock<2;++shock)for(int drop=0;drop<2;++drop){s[shock][drop]=drumOnCrate(shock?.7f:0.f,drop?.1f:0.f);const auto& r=s[shock][drop];
  out.file<<(shock?"shock pass":"no shock pass")<<(drop?", dropped 10 cm":", set down")<<": sinks into the crate "<<f3(r.sink*1000,2)<<" mm, lifts off it after "<<f3(r.lift*1000,2)<<" mm, rises at most "<<f3(r.bounce,3)<<" m/s, hops "<<f3(r.hop*1000,1)<<" mm off it, crate into the floor "<<f3(r.floorSink*1000,2)<<" mm, drifts "<<f3(r.drift*1000,2)<<" mm"<<(r.asleep?", asleep":", awake")<<"\n";}
 for(int drop=0;drop<2;++drop){const auto& r=s[1][drop];const std::string how=drop?" (dropped 10 cm)":" (set down)";
  out.check(r.sink<.004f&&r.floorSink<.004f,"the drum does not squash the crate: under 4 mm into it or the floor"+how);
  // (Dropped, it may hop as its restitution says, a few millimetres: never spring off or walk away.)
  out.check(r.lift<.002f&&r.hop<(drop?.01f:.002f)&&r.drift<(drop?.01f:.005f)&&r.asleep,"and stays put on it: no spring off (a hop under "+std::string(drop?"1 cm":"2 mm")+"), no drift, asleep"+how);}
 return out.ok;}});

// ---- 5. At scale ----
// The scene from scratch each time, five times, and the quickest of each (the machine's other work only slows a run).
struct Scale {double rest=1e9,busy=1e9,world=0,pair=0,solve=0,other=0,worst=0;int asleep=0,woke=0,pairs=0;};
Scale scaleRun(){World w;Scale r;
 for(int i=0;i<20;++i)for(int j=0;j<20;++j)w.add(PartKind::Box,{float(i)*.5f,float(j)*.5f,.15f},{.15f,.15f,.15f},5);
 const float dt=1.f/120;for(int i=0;i<60;++i)w.space.step(dt);
 for(const auto& b:w.space.bodies)r.asleep+=b.sleeping;
 using Clock=std::chrono::steady_clock;auto ms=[](Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();};
 {const auto t=Clock::now();for(int i=0;i<240;++i)w.space.step(dt);r.rest=ms(t)/240;}
 for(int k=0;k<20;++k){const int s=w.add(PartKind::Box,{-1.f,.25f+float(k)*.45f,.45f},{.03f,.02f,.004f},.05f);auto& b=w.space.bodies[size_t(s)];b.v={22.f,float(k%3)-1.f,1.5f};b.w={0,30,7};}
 double busy=0;
 for(int i=0;i<90;++i){const auto t=Clock::now();w.space.step(dt);const double m=ms(t);busy+=m;r.worst=std::max(r.worst,m);r.world+=w.space.worldMs/90;r.pair+=w.space.pairMs/90;r.solve+=w.space.solveMs/90;r.pairs+=w.space.pairsTested;}
 r.busy=busy/90;r.other=r.busy-r.world-r.pair-r.solve;r.pairs/=90;
 for(int i=0;i<400;++i)r.woke+=!w.space.bodies[size_t(i)].sleeping;return r;}
const bool scaleTube=add({.name="vectra-scale",.summary="the cost of a step with 400 resting props and 20 fast shards through them",.standalone=[]{
 Out out("vectra-scale.txt");Scale best;
 for(int k=0;k<5;++k){const Scale r=scaleRun();if(k==0){best=r;continue;}best.rest=std::min(best.rest,r.rest);if(r.busy<best.busy){const double rest=best.rest;best=r;best.rest=rest;}}
 out.file<<"400 props, "<<best.asleep<<" asleep after half a second (the quickest of five runs)\n";
 out.file<<"at rest: "<<f3(best.rest,4)<<" ms a step\n";
 out.file<<"20 shards through them (90 steps): "<<f3(best.busy,4)<<" ms a step (worst "<<f3(best.worst,3)<<"): world "<<f3(best.world,4)<<", pairs "<<f3(best.pair,4)<<", solve "<<f3(best.solve,4)<<", the rest (continuous collision, islands) "<<f3(best.other,4)<<"; "<<best.pairs<<" part pairs a step; "<<best.woke<<" props woken\n";
 out.check(best.asleep==400,"the 400 props settle and sleep");
 return out.ok;}});

// ---- 6. Scrape and roll ----
struct Rub {float scrape=0,roll=0,endScrape=1,endRoll=1,travel=0;bool asleep=false;std::vector<float> trace;Point3 pose{};float dims=0;};
Rub rub(int model,bool onSide,Vec2 v){
 auto m=regression::room();m->definition.playerStart={2,2};Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};
 Clutter c;c.kind=7;c.model=model;c.dims=Game::propTraits(model).size;c.pos={6,8};c.z=0;c.sleeping=false;if(onSide)c.roll=kPi*.5f;
 LabAccess::clutter(g)={c};for(int i=0;i<20;++i)g.update({},1.f/60);   // settled where it stands
 LabAccess::clutter(g)[0].velocity=v;LabAccess::clutter(g)[0].sleeping=false;if(onSide)LabAccess::clutter(g)[0].pitchSpeed=v.x/std::max(.1f,g.clutter()[0].dims[0]*.5f);   // rolled: spun as it goes
const float x0=g.clutter()[0].pos.x;Rub r;r.dims=g.clutter()[0].dims[0];r.pose={g.clutter()[0].pitch,g.clutter()[0].roll,g.clutter()[0].z};
 for(int i=0;i<300;++i){g.update({},1.f/60);const auto& k=g.clutter()[0];r.scrape=std::max(r.scrape,k.scrapeLevel);r.roll=std::max(r.roll,k.rollLevel);if(i%10==0)r.trace.push_back(onSide?k.rollLevel:k.scrapeLevel);}
 const auto& k=g.clutter()[0];r.endScrape=k.scrapeLevel;r.endRoll=k.rollLevel;r.asleep=k.sleeping;r.travel=k.pos.x-x0;return r;}
const bool scrapeTube=add({.name="vectra-scrape",.summary="a shoved crate raises its scrape level (Manifold::slideSpeed into Clutter::scrapeLevel, the AudioEngine's loop) that stops at rest; a rolled drum its roll level",.standalone=[]{
 Out out("vectra-scrape.txt");
 const Rub crate=rub(model::wooden_crate,false,{3,0}),drum=rub(model::ind_barrel,true,{2.5f,0});
 for(auto [name,r]:{std::pair{"crate shoved at 3 m/s",crate},std::pair{"drum on its side rolled at 2.5 m/s",drum}}){
  out.file<<name<<" (dims "<<f3(r.dims,2)<<", pitch "<<f3(r.pose.x,2)<<", roll "<<f3(r.pose.y,2)<<", z "<<f3(r.pose.z,2)<<"): went "<<f3(r.travel,2)<<" m; scrape at most "<<f3(r.scrape,3)<<", roll at most "<<f3(r.roll,3)<<"; at the end scrape "<<f3(r.endScrape,3)<<", roll "<<f3(r.endRoll,3)<<(r.asleep?", asleep":", awake")<<"\n  level each 1/6 s:";
  for(float t:r.trace)out.file<<' '<<f3(t,2);out.file<<"\n";}
 out.check(crate.scrape>.3f&&crate.roll<crate.scrape*.5f,"a shoved crate scrapes (its scrape level over 0.3, its roll under half that)");
 out.check(crate.asleep&&crate.endScrape==0&&crate.endRoll==0,"and falls silent when it comes to rest");
 out.check(drum.roll>.2f&&drum.roll>drum.scrape,"a drum rolled on its side rolls more than it scrapes");
 out.check(drum.endRoll<.02f||!drum.asleep,"and its roll stops when it does");
 return out.ok;}});
}
}

// Vectra's joints on the bare core (no game): a floor plane, boxes, and each kind of joint put through what it
// is for, at 60 and 144 steps a second. joints.txt says what each did.
//  hinge: a plank swings on it and its pin holds; limits stop it; a motor holds a speed; a servo an angle.
//  ball: a pendulum's pin holds; a cone and a twist limit hold against a hard swing and spin.
//  slider: a box falls along its rail to its stop, stays on its line when shoved; a servo holds it up.
//  weld: a shelf welded to the wall and a box welded to its end barely sag.
//  distance, rope, spring: a rod keeps its length; a rope holds a crate and goes slack when it is thrown up;
//    a spring settles where its stiffness says; a winch reels a crate up and stalls on one too heavy.
//  pulley: the heavier side goes down and the rope keeps its length; even weights stay; its motor lifts one.
//  breaking: a rope holding 20 kg snaps when 20 kg more lands on it, and says so once.
//  mass ratio: a 1 kg lamp on a 50 kg cabinet; a 50 kg weight under two 1 kg links: no stretch, no shake.
//  heavy load: a bridge of eight planks takes five 20 kg crates dropped on it and holds.
//  rebuilt: a joint built afresh each step and warm started by its key moves as one kept between steps.
//  position correction: a 40 kg door struck near its edge by a 30 kg crate keeps its pin within a few mm; a welded pair
//    struck end on by 60 kg at 15 m/s stays welded (each uncorrected, then corrected).
#include "lab/Lab.h"
#include "game/physics/Vectra.h"
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <functional>
#include <memory>
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
 float topSpeed()const{float v=0;for(const auto& b:space.bodies)v=std::max(v,len(b.v));return v;}
};
struct Run {float dt=1.f/60;int steps(float seconds)const{return int(seconds/dt+.5f);}};
const bool registered=add({.name="joints",.summary="Vectra's joints on the bare core: hinge, ball, slider, weld, distance, rope, spring, pulley, motors, limits, breaking, mass ratios, heavy loads, warm starting by key",.standalone=[]{
 std::ofstream out("joints.txt");bool ok=true;
 auto check=[&](bool pass,const std::string& what){out<<(pass?"PASS ":"FAIL ")<<what<<'\n';out.flush();ok&=pass;};
 for(float hz:{60.f,144.f}){const Run r{1/hz};const std::string at=" @"+std::to_string(int(hz))+" Hz";out<<"---- "<<int(hz)<<" steps a second ----\n";
  // Hinge: a 1 m plank pinned at one end to the world about y, let go level.
  {World w;const int p=w.box({.5f,0,2},{.5f,.05f,.02f},10);auto& j=w.add(JointKind::Hinge,p,-1);j.attach(w.space,{0,0,2},{0,1,0});
   float pin=0,off=0,lowest=0;for(int i=0;i<r.steps(3);++i){w.space.step(r.dt);pin=std::max(pin,len(j.worldA(w.space)-j.worldB(w.space)));const Body& b=w.space.bodies[size_t(p)];
    const V3 y=b.q.rotate({0,1,0});off=std::max(off,std::acos(std::clamp(std::fabs(y.y),0.f,1.f)));lowest=std::min(lowest,j.angle(w.space));}
   out<<"hinge free: pin drift "<<f3(pin*1000,2)<<" mm, off its axis "<<f3(off*57.3,2)<<" deg, swung to "<<f3(lowest*57.3,1)<<" deg\n";
   check(pin<.004f&&off<.02f&&std::fabs(lowest)>1.4f,"hinge: a plank swings down on its pin, the pin and the axis hold"+at);}
  {World w;const int p=w.box({.5f,0,2},{.5f,.05f,.02f},10);auto& j=w.add(JointKind::Hinge,p,-1);j.attach(w.space,{0,0,2},{0,1,0});j.limited=true;j.lower=-.5f;j.upper=.5f;
   float worst=0;for(int i=0;i<r.steps(3);++i){w.space.step(r.dt);worst=std::max(worst,std::fabs(j.angle(w.space)));}const float end=j.angle(w.space);
   out<<"hinge limited to 0.5 rad: furthest "<<f3(worst,3)<<" rad, rests at "<<f3(end,3)<<"\n";check(worst<.56f&&std::fabs(std::fabs(end)-.5f)<.03f,"hinge: its limit stops it"+at);}
  {World w;const int p=w.box({0,0,.3f},{.4f,.4f,.05f},20);auto& j=w.add(JointKind::Hinge,p,-1);j.attach(w.space,{0,0,.3f},{0,0,1});j.motor=JointMotor::Speed;j.motorSpeed=2;j.motorMaxForce=200;
   for(int i=0;i<r.steps(1.5f);++i)w.space.step(r.dt);const float spin=w.space.bodies[size_t(p)].w.z;
   j.motor=JointMotor::Servo;j.motorTarget=1;j.motorSpeed=3;for(int i=0;i<r.steps(.1f);++i)w.space.step(r.dt);
   // The servo from where it is now: the angle wraps, so set the target from it.
   j.motorTarget=std::remainder(j.angle(w.space)+1.f,2*kPi);for(int i=0;i<r.steps(2);++i)w.space.step(r.dt);const float miss=std::fabs(std::remainder(j.angle(w.space)-j.motorTarget,2*kPi));
   out<<"hinge motor: turntable at "<<f3(spin,3)<<" rad/s (asked 2); servo misses its angle by "<<f3(miss,4)<<" rad\n";check(std::fabs(std::fabs(spin)-2)<.05f&&miss<.02f,"hinge: a motor holds its speed, a servo its angle"+at);}
  {World w;const int p=w.box({.6f,0,2},{.1f,.1f,.6f},5,Quat::axisAngle({0,1,0},kPi*.5f));auto& j=w.add(JointKind::Hinge,p,-1);j.attach(w.space,{0,0,2},{0,1,0});
   j.motor=JointMotor::Servo;j.motorTarget=0;j.motorSpeed=4;j.motorMaxForce=10;   // too weak to hold a 5 kg arm level (42 N m)
   for(int i=0;i<r.steps(2);++i)w.space.step(r.dt);const float sag=j.angle(w.space);out<<"hinge servo too weak: the arm sags to "<<f3(sag*57.3,1)<<" deg\n";check(std::fabs(sag)>.6f,"hinge: a motor holds no more than its force"+at);}
  // Ball: a pendulum pinned at its top to the world.
  {World w;const int p=w.box({0,0,1.5f},{.1f,.1f,.25f},4);auto& j=w.add(JointKind::Ball,p,-1);j.attach(w.space,{0,0,1.75f},{0,0,-1});
   w.space.bodies[size_t(p)].v={3,1,0};float pin=0;for(int i=0;i<r.steps(3);++i){w.space.step(r.dt);pin=std::max(pin,len(j.worldA(w.space)-j.worldB(w.space)));}
   out<<"ball free: pin drift "<<f3(pin*1000,2)<<" mm\n";check(pin<.004f,"ball: a pendulum swings on its pin and the pin holds"+at);}
  {float cone[2]{},tw=0;for(int dir=0;dir<2;++dir){World w;const int p=w.box({0,0,1.5f},{.1f,.1f,.25f},4);auto& j=w.add(JointKind::Ball,p,-1);j.attach(w.space,{0,0,1.75f},{0,0,-1});
    j.coneX=.4f;j.coneY=.25f;j.twistLimited=true;j.twistLower=-.3f;j.twistUpper=.3f;w.space.bodies[size_t(p)].v=dir?V3{0,4,0}:V3{4,0,0};w.space.bodies[size_t(p)].w={0,0,6};
    for(int i=0;i<r.steps(2);++i){w.space.step(r.dt);cone[dir]=std::max(cone[dir],j.swing(w.space));tw=std::max(tw,std::fabs(j.twist(w.space)));}}
   out<<"ball limited: swung to "<<f3(cone[0],3)<<" rad toward x (cone 0.4), "<<f3(cone[1],3)<<" toward y (0.25), twisted "<<f3(tw,3)<<" rad (limit 0.3)\n";
   check(cone[0]<.46f&&cone[0]>.33f&&cone[1]<.31f&&cone[1]>.2f&&tw<.35f,"ball: an elliptical cone and the twist limit hold against a hard swing and spin"+at);}
  // Slider: a box on a vertical rail to the world, its stop 0.5 m down.
  {World w;const int p=w.box({0,0,2},{.15f,.15f,.15f},6);auto& j=w.add(JointKind::Slider,p,-1);j.attach(w.space,{0,0,2},{0,0,1});j.limited=true;j.lower=0;j.upper=.5f;
   float off=0;for(int i=0;i<r.steps(2);++i){if(i==r.steps(1))w.space.bodies[size_t(p)].v={2,0,0};w.space.step(r.dt);const V3 d=j.worldB(w.space)-j.worldA(w.space);off=std::max(off,std::sqrt(d.x*d.x+d.y*d.y));}
   const float travel=j.angle(w.space);float tilt=std::acos(std::clamp(w.space.bodies[size_t(p)].q.w,-1.f,1.f))*2;
   j.motor=JointMotor::Servo;j.motorTarget=.2f;j.motorSpeed=1;j.motorMaxForce=300;for(int i=0;i<r.steps(2);++i)w.space.step(r.dt);const float held=j.angle(w.space);
   out<<"slider: fell to "<<f3(travel,3)<<" m (stop 0.5), off its line "<<f3(off*1000,2)<<" mm, turned "<<f3(tilt*57.3,2)<<" deg; servo holds it at "<<f3(held,3)<<" (asked 0.2)\n";
   check(std::fabs(travel-.5f)<.01f&&off<.004f&&tilt<.02f&&std::fabs(held-.2f)<.02f,"slider: a box runs down its rail to its stop, keeps its line when shoved, a servo lifts it"+at);}
  // Weld: a shelf welded to the wall (the world), a box welded to its far end.
  {World w;const int s=w.box({.5f,0,1.5f},{.5f,.15f,.02f},4);const int b=w.box({.9f,0,1.62f},{.1f,.1f,.1f},6);auto& j1=w.add(JointKind::Weld,s,-1);j1.attach(w.space,{0,0,1.5f});auto& j2=w.add(JointKind::Weld,b,s);j2.attach(w.space,{.9f,0,1.52f});
   for(int i=0;i<r.steps(2);++i)w.space.step(r.dt);const float sag=1.62f-w.space.bodies[size_t(b)].x.z;const float bend=std::acos(std::clamp(std::fabs(w.space.bodies[size_t(s)].q.w),0.f,1.f))*2;
   out<<"weld: the box on the shelf's end sagged "<<f3(sag*1000,2)<<" mm, the shelf turned "<<f3(bend*57.3,3)<<" deg\n";check(sag<.01f&&bend<.015f,"weld: a welded shelf and what is welded to it hold"+at);}
  // Distance: a rod pendulum.
  {World w;const int p=w.box({1,0,2},{.08f,.08f,.08f},3);auto& j=w.add(JointKind::Distance,p,-1);j.attachEnds(w.space,{1,0,2},{0,0,2});
   float worst=0;for(int i=0;i<r.steps(3);++i){w.space.step(r.dt);worst=std::max(worst,std::fabs(j.angle(w.space)-1));}
   out<<"distance: the rod's length off by "<<f3(worst*1000,2)<<" mm at most\n";check(worst<.01f,"distance: a rod keeps its length through a swing"+at);}
  // Rope: a crate hung a metre under the ceiling; thrown up, the rope goes slack.
  {World w;const int p=w.box({0,0,2},{.2f,.2f,.2f},20);auto& j=w.add(JointKind::Rope,p,-1);j.attachEnds(w.space,{0,0,2.2f},{0,0,3.2f});
   float stretch=0;for(int i=0;i<r.steps(2);++i){w.space.step(r.dt);stretch=std::max(stretch,j.angle(w.space)-j.length);}
   w.space.wake(p);w.space.bodies[size_t(p)].v={0,0,3};float slack=1e9f,pullSlack=1e9f;for(int i=0;i<r.steps(.3f);++i){w.space.step(r.dt);slack=std::min(slack,j.angle(w.space));pullSlack=std::min(pullSlack,j.force);}
   out<<"rope: stretched "<<f3(stretch*1000,2)<<" mm under 20 kg; thrown up, its length went to "<<f3(slack,3)<<" m with a pull of "<<f3(pullSlack,1)<<" N\n";
   check(stretch<.01f&&slack<.85f&&pullSlack<1,"rope: it holds a crate at its length and goes slack when the crate is thrown up"+at);}
  // Spring: a 4 kg box on a 2 Hz spring settles g / (2 pi f)^2 below its rest length.
  {World w;const int p=w.box({0,0,2},{.1f,.1f,.1f},4);auto& j=w.add(JointKind::Spring,p,-1);j.attachEnds(w.space,{0,0,2.1f},{0,0,3.1f});j.springHertz=2;j.springDamping=.5f;
   for(int i=0;i<r.steps(5);++i)w.space.step(r.dt);const float want=14.f/((2*kPi*2)*(2*kPi*2)),got=j.angle(w.space)-1;
   out<<"spring: settled "<<f3(got*1000,1)<<" mm past its length (stiffness says "<<f3(want*1000,1)<<")\n";check(std::fabs(got-want)<want*.1f,"spring: a load settles where the spring's stiffness says"+at);}
  // Winch: a rope reeling in at 0.5 m/s lifts a 20 kg crate; one of 80 kg stalls it (its most is 600 N).
  {for(float mass:{20.f,80.f}){World w;const int p=w.box({0,0,.2f},{.2f,.2f,.2f},mass);auto& j=w.add(JointKind::Rope,p,-1);j.attachEnds(w.space,{0,0,.4f},{0,0,3});
    j.motor=JointMotor::Speed;j.motorSpeed=.5f;j.motorMaxForce=600;j.limited=true;j.lower=.3f;j.upper=4;for(int i=0;i<r.steps(2);++i)w.space.step(r.dt);const float rose=w.space.bodies[size_t(p)].x.z-.2f;
    out<<"winch with "<<int(mass)<<" kg: rose "<<f3(rose,3)<<" m in 2 s\n";if(mass<50)check(rose>.85f&&rose<1.1f,"winch: a rope reeling in lifts its crate"+at);else check(rose<.1f,"winch: it stalls on a load past its force"+at);}}
  // Pulley: 10 kg against 5 kg over two wheels; then 8 kg against 8 kg; then a motor on the even pair.
  {World w;const int a=w.box({-1,0,1.5f},{.15f,.15f,.15f},10),b=w.box({1,0,1.5f},{.15f,.15f,.15f},5);auto& j=w.add(JointKind::Pulley,a,b);j.attachPulley(w.space,{-1,0,1.65f},{-1,0,3},{1,0,1.65f},{1,0,3});
   float over=0;for(int i=0;i<r.steps(.6f);++i){w.space.step(r.dt);over=std::max(over,j.angle(w.space)-j.length);}const float fell=1.5f-w.space.bodies[size_t(a)].x.z,rose=w.space.bodies[size_t(b)].x.z-1.5f;
   out<<"pulley 10 kg vs 5 kg: the heavy side went down "<<f3(fell,3)<<" m, the light up "<<f3(rose,3)<<" m, the rope over its length by "<<f3(over*1000,2)<<" mm\n";
   check(fell>.4f&&std::fabs(fell-rose)<.02f&&over<.01f,"pulley: the heavier side goes down, the other up as far, the rope keeps its length"+at);}
  {World w;const int a=w.box({-1,0,1.5f},{.15f,.15f,.15f},8),b=w.box({1,0,1.5f},{.15f,.15f,.15f},8);auto& j=w.add(JointKind::Pulley,a,b);j.attachPulley(w.space,{-1,0,1.65f},{-1,0,3},{1,0,1.65f},{1,0,3});
   for(int i=0;i<r.steps(1);++i)w.space.step(r.dt);const float still=std::fabs(w.space.bodies[size_t(a)].x.z-1.5f);
   j.motor=JointMotor::Speed;j.motorSpeed=.4f;j.motorMaxForce=200;for(int i=0;i<r.steps(1);++i)w.space.step(r.dt);const float lifted=w.space.bodies[size_t(a)].x.z-1.5f;
   out<<"pulley 8 kg vs 8 kg: moved "<<f3(still*1000,2)<<" mm; its motor lifted A "<<f3(lifted,3)<<" m in 1 s (asked 0.4 m/s)\n";check(still<.005f&&lifted>.3f&&lifted<.5f,"pulley: even weights stay; its motor winds one side up"+at);}
  // Breaking: a rope that holds 20 kg (280 N) snaps at 500 N when 20 kg more lands on the crate.
  {World w;const int p=w.box({0,0,2},{.25f,.25f,.2f},20);auto& j=w.add(JointKind::Rope,p,-1);j.attachEnds(w.space,{0,0,2.2f},{0,0,3.2f});j.breakForce=500;
   for(int i=0;i<r.steps(1);++i)w.space.step(r.dt);const bool held=!j.broken;const int lid=w.box({0,0,2.9f},{.2f,.2f,.1f},20);(void)lid;int events=0;float snapped=0;
   for(int i=0;i<r.steps(1.5f);++i){w.space.step(r.dt);events+=int(w.space.jointEvents.size());if(!w.space.jointEvents.empty())snapped=w.space.jointEvents[0].force;}
   const float fell=2-w.space.bodies[size_t(p)].x.z;out<<"breaking: held 20 kg "<<(held?"yes":"no")<<"; snapped at "<<f3(snapped,0)<<" N, "<<events<<" event(s); the crate fell "<<f3(fell,2)<<" m\n";
   check(held&&j.broken&&events==1&&fell>1.5f,"breaking: a rope holds its load, snaps once past its force, and lets go"+at);}
  // Mass ratio: a 1 kg lamp on an arm of a 50 kg cabinet; a 50 kg weight under two 1 kg links.
  {World w;const int c=w.box({0,0,.9f},{.3f,.25f,.9f},50);const int l=w.box({.5f,0,1.3f},{.08f,.08f,.12f},1);auto& j=w.add(JointKind::Ball,l,c);j.attach(w.space,{.5f,0,1.42f},{0,0,-1});
   w.space.bodies[size_t(l)].v={0,2,0};float pin=0;for(int i=0;i<r.steps(4);++i){w.space.step(r.dt);pin=std::max(pin,len(j.worldA(w.space)-j.worldB(w.space)));}
   const float cab=len(w.space.bodies[size_t(c)].v);out<<"1 kg lamp on a 50 kg cabinet: pin drift "<<f3(pin*1000,2)<<" mm, the cabinet's speed at the end "<<f3(cab*1000,2)<<" mm/s\n";
   check(pin<.004f&&cab<.02f,"mass ratio: a light lamp swings on a heavy cabinet, the pin holds, the cabinet stands"+at);}
  {World w;const int l1=w.box({0,0,2.8f},{.03f,.03f,.1f},1),l2=w.box({0,0,2.6f},{.03f,.03f,.1f},1),wt=w.box({0,0,2.3f},{.2f,.2f,.2f},50);
   auto& j0=w.add(JointKind::Ball,l1,-1);j0.attach(w.space,{0,0,2.9f});auto& j1=w.add(JointKind::Ball,l2,l1);j1.attach(w.space,{0,0,2.7f});auto& j2=w.add(JointKind::Ball,wt,l2);j2.attach(w.space,{0,0,2.5f});
   float stretch=0,shake=0;for(int i=0;i<r.steps(4);++i){if(i==r.steps(1))w.space.bodies[size_t(wt)].v={1.5f,0,0};w.space.step(r.dt);
    for(auto* j:{&j0,&j1,&j2})stretch=std::max(stretch,len(j->worldA(w.space)-j->worldB(w.space)));if(i>r.steps(3.5f))shake=std::max(shake,len(w.space.bodies[size_t(l1)].w));}
   out<<"50 kg under two 1 kg links: the worst pin gap "<<f3(stretch*1000,2)<<" mm, the top link's spin in the last half second "<<f3(shake,3)<<" rad/s\n";
   check(stretch<.02f&&shake<3,"mass ratio: a heavy weight on light links hangs and swings without stretching apart or shaking"+at);}
  // Heavy load: a bridge of eight 2 kg planks hinged end to end between two posts; five 20 kg crates dropped on it.
  {World w;std::vector<int> planks;std::vector<V3> ends{{0,0,1}};const float half=1.9f,depth=std::sqrt(2.f*2.f-half*half);
   for(int k=1;k<=8;++k){const float t=float(k)/8;ends.push_back({2*half*t,0,1-depth*(1-std::fabs(2*t-1))});}
   for(int k=0;k<8;++k){const V3 a=ends[size_t(k)],b=ends[size_t(k+1)];const V3 d=b-a;planks.push_back(w.box((a+b)*.5f,{len(d)*.5f-.01f,.4f,.025f},2,Quat::axisAngle({0,1,0},-std::atan2(d.z,d.x))));}
   std::vector<Joint*> js;for(int k=0;k<=8;++k){const int a=k<8?planks[size_t(k)]:planks[7],b=k==0||k==8?-1:planks[size_t(k-1)];auto& j=w.add(JointKind::Hinge,a,b);j.attach(w.space,ends[size_t(k)],{0,1,0});js.push_back(&j);}
   for(int c=0;c<5;++c)w.box({.5f+.7f*float(c),0,1.3f+.2f*float(c)},{.2f,.2f,.2f},20);
   const float before=w.space.bodies[size_t(planks[4])].x.z;float gap=0,top=0;for(int i=0;i<r.steps(4);++i){w.space.step(r.dt);top=std::max(top,w.topSpeed());for(auto* j:js)gap=std::max(gap,len(j->worldA(w.space)-j->worldB(w.space)));}
   float rest=0,held=0;for(int i=0;i<r.steps(.5f);++i){w.space.step(r.dt);rest=std::max(rest,w.topSpeed());for(auto* j:js)held=std::max(held,len(j->worldA(w.space)-j->worldB(w.space)));}const float sag=before-w.space.bodies[size_t(planks[4])].x.z;
   out<<"bridge under five 20 kg crates: the worst pin gap "<<f3(gap*1000,2)<<" mm as they land, "<<f3(held*1000,2)<<" mm once they lie on it, the fastest anything went "<<f3(top,2)<<" m/s, the middle went down "<<f3(sag,3)<<" m, at the end nothing faster than "<<f3(rest,3)<<" m/s\n";
   check(gap<.06f&&held<.015f&&top<9&&rest<.2f,"heavy load: a plank bridge takes five dropped crates (ten times a plank's mass each) and holds without exploding"+at);}
  // Position correction (Joint::positionCorrection) under a hard blow: a 40 kg door on one hinge struck near its edge by a
  // 30 kg crate at 12 m/s; a welded pair of 10 kg boxes struck end on by a 60 kg box at 15 m/s. Off, then on.
  {float doorGap[2]{},weldGap[2]{},weldTurn[2]{},doorTop[2]{};
   for(int on=0;on<2;++on){
    {World w;const int d=w.box({.5f,0,1.05f},{.5f,.02f,1},40);auto& j=w.add(JointKind::Hinge,d,-1);j.attach(w.space,{0,0,1.05f},{0,0,1});j.positionCorrection=on;
     const int c=w.box({.85f,1.2f,1.4f},{.2f,.2f,.2f},30);w.space.bodies[size_t(c)].v={0,-12,0};w.space.bodies[size_t(c)].gravity={};
     for(int i=0;i<r.steps(1);++i){w.space.step(r.dt);doorGap[on]=std::max(doorGap[on],len(j.worldA(w.space)-j.worldB(w.space)));doorTop[on]=std::max(doorTop[on],len(w.space.bodies[size_t(d)].w));}}
    {World w;const int a=w.box({0,0,.15f},{.15f,.15f,.15f},10),b=w.box({.3f,0,.15f},{.15f,.15f,.15f},10);auto& j=w.add(JointKind::Weld,b,a);j.attach(w.space,{.15f,0,.15f});j.positionCorrection=on;
     const int h=w.box({-1.2f,0,.2f},{.2f,.2f,.2f},60);w.space.bodies[size_t(h)].v={15,0,0};
     for(int i=0;i<r.steps(1);++i){w.space.step(r.dt);weldGap[on]=std::max(weldGap[on],len(j.worldA(w.space)-j.worldB(w.space)));
      const Quat rel=(w.space.bodies[size_t(b)].q*j.frameB)*(w.space.bodies[size_t(a)].q*j.frameA).conj();weldTurn[on]=std::max(weldTurn[on],2*std::acos(std::clamp(std::fabs(rel.w),0.f,1.f)));}}}
   out<<"a 40 kg door struck near its edge by a 30 kg crate at 12 m/s: its pin opened "<<f3(doorGap[0]*1000,2)<<" mm uncorrected, "<<f3(doorGap[1]*1000,2)<<" mm corrected (spinning at most "<<f3(doorTop[0],2)<<" and "<<f3(doorTop[1],2)<<" rad/s)\n";
   out<<"a welded pair struck end on by 60 kg at 15 m/s: the weld opened "<<f3(weldGap[0]*1000,2)<<" mm and turned "<<f3(weldTurn[0]*57.3,2)<<" deg uncorrected, "<<f3(weldGap[1]*1000,2)<<" mm and "<<f3(weldTurn[1]*57.3,2)<<" deg corrected\n";
   check(doorGap[1]<.004f&&doorGap[1]<=doorGap[0]&&doorTop[1]<doorTop[0]*1.2f+.1f,"position correction: a heavy door struck hard keeps its pin together within a few mm"+at);
   check(weldGap[1]<.004f&&weldTurn[1]<.02f&&weldGap[1]<=weldGap[0],"position correction: a welded pair struck hard stays welded"+at);}
  // Rebuilt each step and warm started by key, against kept: the same motion.
  {float end[2]{};for(int kept=0;kept<2;++kept){World w;const int p=w.box({.6f,0,2},{.1f,.1f,.1f},5);Joint keep;keep.kind=JointKind::Ball;keep.a=p;keep.b=-1;keep.attach(w.space,{0,0,2.6f});keep.key=7;
    for(int i=0;i<r.steps(2);++i){Joint fresh=keep;w.space.constraints={kept?&keep:&fresh};w.space.step(r.dt);}end[kept]=w.space.bodies[size_t(p)].x.x;}
   out<<"rebuilt by key "<<f3(end[0],5)<<" m, kept "<<f3(end[1],5)<<" m\n";check(std::fabs(end[0]-end[1])<1e-4f,"warm starting: a joint rebuilt each step finds its impulses by key"+at);}
 }
 // ---- Landing and waking (60 Hz) ----
 out<<"---- landing and waking ----\n";const float dt=1.f/60;
 // A 0.4 m crate 6 cm over the floor going down at 3 m/s (within its speculative reach): where its bottom is when
 // it first stops going down. And one 5 mm over a frictionless floor, going down at 3 m/s and along at 6 (it lands
 // flat within the step, and nothing but its bounce could turn it): its spin after that step, against the same landing without a bounce.
 struct Landing {float bottom=1e9f;V3 w{};};
 auto land=[&](V3 v,float restitution,float over,int at,float friction=.6f){World w;const int p=w.box({0,0,.2f+over},{.2f,.2f,.2f},20);Body& b=w.space.bodies[size_t(p)];b.v=v;b.material.restitution=restitution;b.material.staticFriction=friction;b.material.dynamicFriction=friction*.8f;
  Landing L;for(int i=0;i<60;++i){w.space.step(dt);const Body& c=w.space.bodies[size_t(p)];if(i==at||(at<0&&c.v.z>=0)){L.bottom=c.x.z-.2f;L.w=c.w;break;}}return L;};
 {const Landing drop=land({0,0,-3},.2f,.06f,-1),thrown=land({6,0,-3},.2f,.005f,0,0),dead=land({6,0,-3},0,.005f,0,0);const float bounceSpin=len(thrown.w-dead.w);
  out<<"a crate falling at 3 m/s from 6 cm: turned back with its bottom "<<f3(drop.bottom*1000,2)<<" mm off the floor; landing flat and sliding at 6 m/s on a frictionless floor: its bounce turned it by "<<f3(bounceSpin,3)<<" rad/s (spinning "<<f3(len(thrown.w),3)<<", "<<f3(len(dead.w),3)<<" without a bounce)\n";
  check(std::fabs(drop.bottom)<.004f,"landing: a crate falling at 3 m/s onto the floor is stopped at the floor, not in the air short of it");
  check(bounceSpin<.05f,"landing: a crate thrown down and along onto a frictionless floor gets no spin from its bounce");}
 // A held (kinematic) block carried at 1 m/s with a sleeping box hung under it on a ball joint: the box wakes and follows.
 {World w;const int k=w.box({0,0,2},{.1f,.1f,.1f},1);Body& kb=w.space.bodies[size_t(k)];kb.kinematic=true;kb.setMass(0);kb.gravity={};kb.updateInertia();
  const int p=w.box({0,0,1.6f},{.1f,.1f,.1f},5);auto& j=w.add(JointKind::Ball,p,k);j.attach(w.space,{0,0,1.8f});
  w.space.bodies[size_t(p)].sleeping=true;w.space.bodies[size_t(k)].v={1,0,0};float gap=0;
  for(int i=0;i<60;++i){Body& K=w.space.bodies[size_t(k)];K.x=K.x+K.v*dt;w.space.step(dt);gap=std::max(gap,len(j.worldA(w.space)-j.worldB(w.space)));}
  const Body& b=w.space.bodies[size_t(p)];out<<"a sleeping box hung from a held block carried 1 m: "<<(b.sleeping?"still asleep":"awake")<<", went "<<f3(b.x.x,3)<<" m, the pin opened "<<f3(gap*1000,2)<<" mm at most\n";
  check(!b.sleeping&&b.x.x>.8f&&gap<.03f,"waking: a held body carried on a joint wakes its sleeping partner, which follows");}
 return ok;}});
}
}

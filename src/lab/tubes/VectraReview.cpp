// The review's fixes to Vectra, each on the bare core (planes for the world) where it can be, and in the game
// where the game is what changed. Each writes <tube>.txt with its numbers.
//  vectra-drive: a 2 m plank driven (Space::drive) to turn its end through a doorframe's jamb: capped, it is
//    levered no harder than it is pulled and rests against the jamb, still; set outright (the old drive), it shook
//    against it or went through.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/Vectra.h"
#include "world/ModelIds.h"
#include <chrono>
#include <cmath>
#include <cstdio>
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
}
}

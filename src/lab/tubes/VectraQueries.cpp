// Vectra's queries and continuous collision (game/physics/Queries.h, PropQueries.h).
//
// vectra-queries: what is struck, in the shapes as they stand.
//  - the core: a ray, a sphere cast, a box cast and an overlap against a box turned 45 degrees and a cylinder,
//    each against the distance worked by hand;
//  - gunfire (Game::traceShot): a crate turned 45 degrees lets a round past its visual corner (its old box
//    stopped it) and stops one into its face; a round through the gap under a wooden chair's seat passes it
//    (neither hit nor passage), one down into its seat meets it;
//  - shielding (Game::shieldBlow): a crate held up takes a blow from in front, not one from the side or from
//    45 degrees off it; a mug held up takes none;
// vectra-ccd: what fast bodies meet within one step.
//  - the core: a plank spinning at 60 rad/s beside a 3 cm post (the post as the world, by Space::sweep, and as
//    a fixed body) never turns through it; two thin boards flying at each other at 25 m/s each meet;
//  - jointed islands (60 and 120 steps a second): a six-box ragdoll thrown at 15 m/s at a 3 cm wall, a door slammed
//    at 15 rad/s into a 3 cm post (the world's and a fixed body), a lamp whipped on its chain at 20 m/s at a 3 cm pole:
//    none goes through, and their joints stay together;
//  - the probes' bound (Settings::ccdProbes, ccdBudget): a blast of twenty crates in a pen of 3 cm walls, on the core and
//    in the game, and one fast 1 m crate, bounded and the old way: the world sweeps and their time, and none gets out;
//  - the game: two crates thrown at each other at 25 m/s meet and neither passes the other; a bottle thrown
//    at 30 m/s at a 3 cm wall never gets past it whole.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropQueries.h"
#include "core/Ray.h"
#include "core/Tune.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <memory>
namespace retro::lab {
namespace {
using namespace vectra;
std::string f3(double v){char b[32];std::snprintf(b,sizeof b,"%.3f",v);return b;}
Clutter prop(int model,Vec2 at,float z,float yaw=0){Clutter c;c.kind=7;c.model=model;c.dims=Game::propTraits(model).size;c.pos=at;c.z=z;c.yaw=yaw;c.sleeping=true;return c;}
Clutter weighing(Clutter c,float kg,float health=-1){auto t=std::make_shared<EntityTraits>();t->mass=kg;t->health=health;c.traits=t;return c;}
// Did the shot meet prop `index` (stopped on it, or passed through it)?
bool struck(const ShotHit& h,int index){if(h.kind==ShotKind::Prop&&h.index==index)return true;for(const auto& p:h.passed)if(p.kind==2&&p.index==index)return true;return false;}
// The old test: the prop's world box (Clutter::extent) about its middle.
bool boxStruck(const Clutter& c,Point3 o,Point3 d,float range){const auto e=c.extent();const Point3 m{c.pos.x,c.pos.y,c.z+e[2]};return rayBoxFraction(o,d*range,m-Point3{e[0],e[1],e[2]},m+Point3{e[0],e[1],e[2]})<1;}

const bool queries=add({.name="vectra-queries",.summary="rays, shape casts and overlaps against turned and compound shapes; gunfire past a turned crate's corner and under a chair's seat; shielding by geometry",.standalone=[]{
 std::ofstream r("vectra-queries.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){r<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;r.flush();};
 // ---- The core ----
 {Shape box;box.add(PartKind::Box,{},{.2f,.2f,.2f});box.finish();Shape can;can.add(PartKind::Cylinder,{},{.1f,.1f,.3f});can.finish();
  Body b;b.shape=&box;b.q=Quat::axisAngle({0,0,1},kPi/4);Body c;c.shape=&can;
  const float corner=.2f*std::sqrt(2.f);QueryHit h;
  const bool face=rayCast(b,{-2,0,.1f},{1,0,0},5,h);const float faceAt=h.distance;
  r<<"CORE\n  box turned 45 deg (half 0.2): ray along x hits its edge at "<<f3(faceAt)<<" (by hand "<<f3(2-corner)<<")\n";
  check(face&&std::fabs(faceAt-(2-corner))<1e-3f,"core: a ray meets a turned box's leading edge where it is");
  const V3 d=norm(V3{1,1,0}),side=norm(V3{1,-1,0});
  const bool past=rayCast(b,V3{0,0,.1f}-d*2+side*.24f,d,5,h),into=rayCast(b,V3{0,0,.1f}-d*2,d,5,h);const float intoAt=h.distance;const V3 intoN=h.normal;
  check(!past&&into&&std::fabs(intoAt-1.8f)<1e-3f&&dot(intoN,d)<-.999f,"core: a ray along a turned box's face 4 cm outside it misses; one into the face meets it at 1.8 m, its normal against the ray");
  const bool swept=sphereCast(b,{-2,0,.1f},.1f,{1,0,0},5,h);const float sweptAt=h.distance;
  const bool boxed=boxCast(b,{-2,0,.1f},{.1f,.1f,.1f},Quat{},{1,0,0},5,h);const float boxedAt=h.distance;
  r<<"  sphere (r 0.1) cast along x meets it at "<<f3(sweptAt)<<", a 0.2 m box at "<<f3(boxedAt)<<" (by hand "<<f3(2-corner-.1f)<<")\n";
  check(swept&&boxed&&std::fabs(sweptAt-(2-corner-.1f))<2e-3f&&std::fabs(boxedAt-(2-corner-.1f))<2e-3f,"core: a sphere cast and a box cast meet the turned box's edge where it is");
  const bool round=rayCast(c,{-1,.09f,0},{1,0,0},5,h);const float roundAt=h.distance;
  const bool corner2=rayCast(c,V3{0,0,0}-d*2+side*.12f,d,5,h),cap=rayCast(c,{.05f,.05f,1},{0,0,-1},5,h);const float capAt=h.distance;
  r<<"  cylinder (r 0.1, half 0.3): ray at y 0.09 meets its round at "<<f3(roundAt)<<" (by hand "<<f3(1-std::sqrt(.01f-.0081f))<<"), down onto its cap at "<<f3(capAt)<<"\n";
  check(round&&std::fabs(roundAt-(1-std::sqrt(.01f-.0081f)))<1e-3f&&!corner2&&std::fabs(capAt-.7f)<1e-3f,"core: a cylinder is round to a ray (one past where its box's corner would be misses), flat on its caps");
  const bool cylCast=sphereCast(c,{-1,0,0},.05f,{1,0,0},5,h);const float cylAt=h.distance;
  check(cylCast&&std::fabs(cylAt-.85f)<2e-3f,"core: a sphere cast meets a cylinder's round ("+f3(cylAt)+" m, by hand 0.850)");
  check(overlapSphere(b,{.3f,0,0},.1f)&&!overlapSphere(b,{.4f,0,0},.1f)&&overlapBox(b,{.35f,0,0},{.1f,.1f,.1f},Quat{})&&!overlapBox(b,{.35f,.3f,0},{.05f,.05f,.05f},Quat{}),"core: overlaps of a sphere and a box with the turned box, by its edge");
  // Many bodies: the nearest, and every one along the way.
  std::vector<Body> list{b,c};list[1].x={1,0,0};std::vector<QueryHit> all;
  const bool nearest=rayCast(list,{-2,0,.1f},{1,0,0},5,h);const int n=rayCastAll(list,{-2,0,.1f},{1,0,0},5,all);
  check(nearest&&h.body==0&&n==2&&all[0].body==0&&all[1].body==1&&std::fabs(all[1].distance-2.9f)<1e-3f,"core: over a list of bodies the nearest is the box, and every hit comes in order");
  // A cast cut short (its advancement given one step, as a grazing one runs out of them): a sphere passing 5 mm
  // wide of the cylinder is no hit; one straight at a flat face, done in its one step, is.
  {Body flat;flat.shape=&box;QueryHit g;const bool wide=sphereCast(c,{-1,.155f,0},.05f,{1,0,0},5,g),wideFull=wide;castIterations=1;
   const bool cut=sphereCast(c,{-1,.155f,0},.05f,{1,0,0},5,g),face2=sphereCast(flat,{-2,0,0},.1f,{1,0,0},5,g);const float faceAt2=g.distance;castIterations=64;
   r<<"  a sphere cast 5 mm wide of the cylinder: "<<(wideFull?"hit":"no hit")<<"; cut to one step: "<<(cut?"HIT":"no hit")<<"; straight at a box's face in one step: "<<(face2?"hit at "+f3(faceAt2):"no hit")<<"\n";
   check(!wideFull&&!cut&&face2&&std::fabs(faceAt2-1.7f)<2e-3f,"core: a cast that runs out of steps before it closes in reports no hit (one that has closed in still does)");}}
 // ---- Gunfire ----
 {auto m=regression::room();m->definition.playerStart={3,3};Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};
  auto& list=LabAccess::clutter(g);list.clear();
  list.push_back(weighing(prop(model::wooden_crate,{10,8},0,kPi/4),20));   // heavy: it stops what meets it
  list.push_back(prop(model::wooden_chair,{10,12},0));
  const Clutter crate=list[0],chair=list[1];const V3 mid=propCentre(crate);const auto s=crate.size();const float hy=s[1]*.5f;
  const Point3 d{std::cos(kPi/4),std::sin(kPi/4),0},side{-std::sin(kPi/4),std::cos(kPi/4),0};
  const Point3 pastFrom=mid-d*2.f+side*(hy*1.15f),intoFrom=mid-d*2.f;
  const auto past=LabAccess::trace(g,pastFrom,d,6),into=LabAccess::trace(g,intoFrom,d,6);
  r<<"\nGUNFIRE\n  a 20 kg crate turned 45 deg (half width "<<f3(hy)<<"): a round along its side "<<f3(hy*.15f)<<" m outside it "<<(struck(past,0)?"meets it":"passes")<<" (its old box: "<<(boxStruck(crate,pastFrom,d,6)?"stopped it":"passed")<<"); one into its face "<<(into.kind==ShotKind::Prop&&into.index==0?"stops at "+f3(into.distance)+" m":"does not stop")<<"\n";
  check(!struck(past,0)&&boxStruck(crate,pastFrom,d,6),"gunfire: a round past a turned crate's visual corner passes it (its old box would have stopped it)");
  check(into.kind==ShotKind::Prop&&into.index==0&&std::fabs(into.distance-(2-s[0]*.5f))<.01f,"gunfire: a round into the turned crate's face stops on it, at its face");
  const V3 seat=propCentre(chair);const float h=chair.size()[2];
  const Point3 gapFrom{seat.x-2,seat.y,chair.z+h*.2f},gapDir{1,0,0},topFrom{seat.x,seat.y,chair.z+h*.47f+1};
  const auto gap=LabAccess::trace(g,gapFrom,gapDir,4),top=LabAccess::trace(g,topFrom,{0,0,-1},1.2f);
  r<<"  a wooden chair: a round through the gap under its seat "<<(struck(gap,1)?"meets it":"passes")<<" (its old box: "<<(boxStruck(chair,gapFrom,gapDir,4)?"met it":"passed")<<"); one down onto its seat "<<(struck(top,1)?"meets it":"passes")<<"\n";
  check(!struck(gap,1)&&boxStruck(chair,gapFrom,gapDir,4),"gunfire: a round through the gap under a chair's seat passes the chair (its old box would have met it)");
  check(struck(top,1),"gunfire: a round down onto the chair's seat meets it");}
 // ---- Shielding ----
 {auto shieldWith=[&](int model,float kg,Vec2 from,bool& ok2)->bool{auto m=regression::room();m->definition.playerStart={3,3};Game g(regression::campaign(m));LabAccess::setGod(g,true);
   auto& p=LabAccess::player(g);p.pos={8.6f,8};p.angle=0;p.pitch=0;LabAccess::clutter(g)={weighing(prop(model,{9.4f,8},0),kg,0)};
   ok2=LabAccess::grab(g)&&LabAccess::held(g)==0;for(int i=0;i<20;++i)g.update({},1.f/60);ok2&=LabAccess::held(g)==0;
   return LabAccess::shieldBlow(g,10,LabAccess::player(g).pos+from);};
  bool heldA=false,heldB=false,heldC=false,heldD=false;
  const bool front=shieldWith(model::wooden_crate,20,{2,0},heldA),beside=shieldWith(model::wooden_crate,20,{0,2},heldB),off=shieldWith(model::wooden_crate,20,{1.41f,1.41f},heldC),mug=shieldWith(model::mug,.4f,{2,0},heldD);
  r<<"\nSHIELDING (a 20 kg crate held up in front; blows from 2 m)\n  from in front: "<<(front?"on the crate":"on him")<<"; from his side: "<<(beside?"on the crate":"on him")<<"; from 45 deg off: "<<(off?"on the crate":"on him")<<" (the old 60 degree cone took it); a mug held up, from in front: "<<(mug?"on the mug":"on him")<<"\n";
  check(heldA&&heldB&&heldC&&heldD,"shielding: the props are held");
  check(front,"shielding: a blow into the held crate lands on the crate");
  check(!beside&&!off,"shielding: a blow from a side the held crate does not cover (90 and 45 degrees off) lands on the player");
  check(!mug,"shielding: a mug is not a shield");}
 return ok;}});

// ---- Continuous collision ----
// A plank (1.2 x 0.03 x 0.2 m) spinning at 60 rad/s about its middle, its broad face leading (3 cm through), beside a
// 3 cm post 0.45 m out (0.5 rad a step: with its own 3 cm it would turn clean through the post within one step).
// Watched from the plank: the post's middle in its frame. Within its length, the post going from one face's side
// to the other's is a turn through it; nearer its middle plane than the two halves (3 cm) is a post in it.
struct Spin {int through=0;float deepest=0;bool struck=false;V3 endV{},endW{};long long sweeps=0;};
Spin spin(bool world,float speed=60,float dt=1.f/120,int probes=-1){
 Space space;space.settings.maxAngular=std::max(space.settings.maxAngular,speed*1.5f);if(probes>=0){space.settings.ccdProbes=probes;space.settings.ccdBudget=probes?space.settings.ccdBudget:0;}Shape plank;plank.add(PartKind::Box,{},{.6f,.015f,.1f});plank.finish();Shape post;post.add(PartKind::Box,{},{.015f,.015f,.5f});post.finish();
 Body p;p.shape=&plank;p.material.restitution=.2f;p.linearDamping=p.angularDamping=0;p.gravity={};p.setMass(3);p.q=Quat::axisAngle({0,0,1},-.6f);p.w={0,0,speed};p.updateInertia();space.bodies.push_back(p);
 const V3 at{.45f,0,0},lo=at-V3{.015f,.015f,.5f},hi=at+V3{.015f,.015f,.5f};
 if(world)space.sweep=[&](Space&,int,V3 a,V3 b,V3& n)->float{return rayBoxFraction(a,b-a,lo,hi,&n);};
 else{Body s;s.shape=&post;s.kinematic=true;s.setMass(0);s.x=at;space.bodies.push_back(s);}
 Spin out;auto local=[&]{const auto& B=space.bodies[0];return B.q.unrotate(at-B.x);};V3 last=local();
 for(int i=0;i<int(1/dt+.5f);++i){space.step(dt);out.sweeps+=space.ccdSweeps;const V3 l=local();const auto& B=space.bodies[0];
  if(std::fabs(l.x)<.585f&&std::fabs(last.x)<.585f&&(l.y>0)!=(last.y>0))++out.through;
  if(std::fabs(l.x)<.585f)out.deepest=std::max(out.deepest,.03f-std::fabs(l.y));
  out.struck|=B.w.z<speed*.5f;last=l;out.endV=B.v;out.endW=B.w;
  if(std::getenv("RM_SPIN_TRACE")){static std::ofstream tr("spin-trace.txt");tr<<(world?"W ":"B ")<<i<<" post at "<<l.x<<","<<l.y<<" w "<<B.w.x<<","<<B.w.y<<","<<B.w.z<<" x "<<B.x.x<<","<<B.x.y<<","<<B.x.z<<" ccd "<<B.ccd<<" events "<<space.events.size()<<std::endl;}}
 return out;}
// Two thin boards (0.3 x 0.3 x 0.02 m) flying face on at each other at 25 m/s each, 0.32 m apart: beyond the
// speculative reach, they would cross in one step.
bool boards(float& gapAfter,float& va,float& vb){
 Space space;Shape board;board.add(PartKind::Box,{},{.01f,.15f,.15f});board.finish();
 for(int k=0;k<2;++k){Body b;b.shape=&board;b.gravity={};b.linearDamping=b.angularDamping=0;b.setMass(2);b.x={k?.17f:-.17f,0,1};b.v={k?-25.f:25.f,0,0};b.updateInertia();space.bodies.push_back(b);}
 bool crossed=false;for(int i=0;i<30;++i){space.step(1.f/120);crossed|=space.bodies[0].x.x>space.bodies[1].x.x;}
 gapAfter=space.bodies[1].x.x-space.bodies[0].x.x;va=space.bodies[0].v.x;vb=space.bodies[1].v.x;return !crossed;}
// Jointed islands: a floor at z 0 and a thin box (wall or post) as the world, met as the game's World meets a fixture: its
// face on the side a body's middle is on (a body through it is pushed on out of its far side), its upright corners against
// the body's own shape (RigidClutter's corner test), and the sweep.
struct Jointed {
 // A box of the world. mode 1: a wall (its face and its corners); 2: a post (its corners).
 struct Solid {V3 lo,hi;int mode;};
 Space space;std::deque<Shape> shapes;std::vector<std::unique_ptr<Joint>> joints;std::vector<Solid> solids;
 // mode 0: the floor alone (the box is swept, never met by contacts).
 Jointed(V3 wallLo,V3 wallHi,int mode){solids.push_back({wallLo,wallHi,mode});
  // (Bodies a joint joins do not meet, as a ragdoll's sections do not: a contact at the pivot fights the joint.)
  space.filter=[this](const Space&,int a,int b){for(const auto& j:joints)if((j->a==a&&j->b==b)||(j->a==b&&j->b==a))return false;return true;};
  space.sweep=[this](Space&,int,V3 a,V3 b,V3& n)->float{float best=1;for(const auto& s:solids){V3 nn;const float f=rayBoxFraction(a,b-a,s.lo,s.hi,&nn);if(f<best){best=f;n=nn;}}return best;};
  space.worldContacts=[this](Space& sp,int i,float margin,std::vector<Manifold>& out){const Body& body=sp.bodies[size_t(i)];
   for(int part=0;part<body.shape->count;++part){
    auto emit=[&](V3 d,V3 normal,int partB,auto&& gap,auto&& inside){std::array<V3,8> f{};const int k=supportFeature(body,part,d,f);Manifold m;m.a=i;m.b=-1;m.partA=part;m.partB=partB;m.normal=normal;m.material=Material::mix(body.material,{.6f,.5f,0,.004f});
     for(int j=0;j<k&&m.count<4;++j){const V3 p=f[size_t(j)];const float sep=gap(p);if(sep>margin||!inside(p))continue;auto& c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=p;c.separation=sep;c.localA=body.q.unrotate(p-body.x);c.localB=p;c.feature=std::uint32_t(j);}
     if(m.count)out.push_back(m);};
    emit({0,0,-1},{0,0,-1},0,[](V3 p){return p.z;},[](V3){return true;});
    for(int si=0;si<int(solids.size());++si){const Solid& s=solids[size_t(si)];if(s.mode==0)continue;const V3 lo=s.lo,hi=s.hi;
     V3 plo,phi;partBounds(body,part,plo,phi);if(plo.x>hi.x+margin||phi.x<lo.x-margin||plo.y>hi.y+margin||phi.y<lo.y-margin)continue;
     // Its face toward the body's middle (the axis the middle is furthest outside the box along).
     const V3 mid=(lo+hi)*.5f,half=(hi-lo)*.5f;int axis=0;float most=-1e30f;for(int a=0;a<2;++a){const float o=std::fabs(comp(body.x-mid,a))-comp(half,a);if(o>most){most=o;axis=a;}}
     const float side=comp(body.x-mid,axis)<0?-1.f:1.f;V3 d{};if(axis==0)d.x=-side;else d.y=-side;   // d: into the box from that side
     auto within=[&](V3 p){for(int a=0;a<3;++a){if(a==axis)continue;if(comp(p,a)<comp(lo,a)-.01f||comp(p,a)>comp(hi,a)+.01f)return false;}return true;};
     if(s.mode==1)emit(d,d,1+si*16+axis*2+(side>0),[&](V3 p){return side<0?comp(lo,axis)-comp(p,axis):comp(p,axis)-comp(hi,axis);},within);
     // Its upright corners against the body's shape, at three heights over the body's own (as the game asks).
     Manifold m;m.a=i;m.b=-1;m.partA=part;m.partB=8+si*16;m.material=Material::mix(body.material,{.6f,.5f,0,.004f});V3 sum{};
     const float z0=std::max(lo.z,plo.z),z1=std::min(hi.z,phi.z);if(z1<z0)continue;
     for(int c=0;c<4;++c)for(float z:{z0+.01f,(z0+z1)*.5f,z1-.01f}){const V3 p{c&1?hi.x:lo.x,c&2?hi.y:lo.y,z};V3 surface,normal;float dd;
      if(!nearestPoint(body,part,p,surface,normal,dd)||dd>=margin||m.count>=4)continue;auto& k=m.points[size_t(m.count++)];k=ContactPoint{};k.p=p;k.separation=dd;k.localA=body.q.unrotate(p-body.x);k.localB=p;sum=sum+normal;}
     if(m.count){m.normal=norm(sum);out.push_back(m);}}}};}
 int box(V3 at,V3 half,float mass,Quat q={}){shapes.emplace_back();Shape& s=shapes.back();s.add(PartKind::Box,{},half);s.finish();Body b;b.shape=&s;b.q=q;b.x=at;b.setMass(mass);b.updateInertia();b.material={.6f,.5f,.2f,.002f};b.id=int(space.bodies.size());space.bodies.push_back(b);return int(space.bodies.size())-1;}
 Joint& add(JointKind kind,int a,int b,V3 at,V3 axis={0,0,1}){joints.push_back(std::make_unique<Joint>());auto& j=*joints.back();j.kind=kind;j.a=a;j.b=b;space.constraints.push_back(&j);j.attach(space,at,axis);return j;}
 float worstPin()const{float w=0;for(const auto& j:joints)w=std::max(w,len(j->worldA(space)-j->worldB(space)));return w;}
};
struct Through {bool passed=false;float worstPin=0,endPin=0;int events=0;};
// RM_JOINTED_TRACE=1: each step's bodies and impacts to jointed-trace.txt.
void trace(const char* what,const Jointed& w,int i,float dt){if(!std::getenv("RM_JOINTED_TRACE"))return;static std::ofstream tr("jointed-trace.txt");
 tr<<what<<" "<<int(1/dt+.5f)<<" Hz step "<<i<<" pin "<<w.worstPin()<<"\n";for(const auto& b:w.space.bodies)tr<<"   x "<<b.x.x<<","<<b.x.y<<","<<b.x.z<<" v "<<len(b.v)<<" w "<<len(b.w)<<" ccd "<<b.ccd<<"\n";
 for(const auto& e:w.space.events)tr<<"   event "<<e.a<<"-"<<e.b<<" at "<<e.point.x<<","<<e.point.y<<","<<e.point.z<<" n "<<e.normal.x<<","<<e.normal.y<<","<<e.normal.z<<" approach "<<e.approach<<" impulse "<<e.impulse<<"\n";tr.flush();}
// A ragdoll of six boxes on ball joints (a 30 kg torso, head, arms, legs; 75 kg) thrown at 15 m/s, tumbling, at a 3 cm wall
// 1.5 m off: through it is any section's middle past the wall's middle.
Through ragdollAtWall(float dt){
 Jointed w({1.5f,-3,0},{1.53f,3,3},1);
 const int torso=w.box({0,0,1.1f},{.1f,.175f,.25f},30),head=w.box({0,0,1.5f},{.1f,.1f,.1f},5);
 const int armL=w.box({0,.25f,1.1f},{.045f,.045f,.22f},4),armR=w.box({0,-.25f,1.1f},{.045f,.045f,.22f},4);
 const int legL=w.box({0,.09f,.45f},{.06f,.06f,.38f},16),legR=w.box({0,-.09f,.45f},{.06f,.06f,.38f},16);
 w.add(JointKind::Ball,head,torso,{0,0,1.38f});w.add(JointKind::Ball,armL,torso,{0,.22f,1.33f});w.add(JointKind::Ball,armR,torso,{0,-.22f,1.33f});
 w.add(JointKind::Ball,legL,torso,{0,.09f,.85f});w.add(JointKind::Ball,legR,torso,{0,-.09f,.85f});
 // (A corpse's settings, Carcass::stepVectra: eight substeps, six joint passes, its joints locked at 240 Hz, its sections damped.)
 w.space.settings.substeps=8;w.space.settings.jointIterations=6;w.space.settings.shockNormal=0;for(auto& j:w.joints){j->lockHertz=240;j->lockDamping=1;}
 for(auto& b:w.space.bodies){b.v={15,0,1};b.w={0,3,2};b.linearDamping=.1f;b.angularDamping=1;}
 Through r;for(int i=0;i<int(1.5f/dt+.5f);++i){w.space.step(dt);r.events+=int(w.space.events.size());for(const auto& b:w.space.bodies)r.passed|=b.x.x>1.515f;r.worstPin=std::max(r.worstPin,w.worstPin());
  trace("ragdoll",w,i,dt);}
 r.endPin=w.worstPin();return r;}
// A 20 kg door (1 x 2 m, 4 cm thick) on a hinge to the world, slammed at 15 rad/s (its edge at 15 m/s), a 3 cm post 0.8 m
// out along its way: through it is the post's middle going from one face's side to the other's within the door's width.
Through doorAtPost(float dt,bool postAsBody){
 const V3 post{.8f*std::cos(.5f),.8f*std::sin(.5f),0};const V3 plo=post+V3{-.015f,-.015f,0},phi=post+V3{.015f,.015f,2.2f};
 Jointed w(postAsBody?V3{50,50,0}:plo,postAsBody?V3{50.03f,50.03f,1}:phi,postAsBody?0:2);
 const int door=w.box({.5f,0,1.05f},{.5f,.02f,1},20);auto& hinge=w.add(JointKind::Hinge,door,-1,{0,0,1.05f},{0,0,1});(void)hinge;
 if(postAsBody){w.shapes.emplace_back();Shape& s=w.shapes.back();s.add(PartKind::Box,{},{.015f,.015f,1.1f});s.finish();Body b;b.shape=&s;b.kinematic=true;b.setMass(0);b.x=post+V3{0,0,1.1f};b.updateInertia();b.id=int(w.space.bodies.size());w.space.bodies.push_back(b);}
 w.space.bodies[size_t(door)].w={0,0,15};w.space.settings.maxAngular=60;
 Through r;auto local=[&]{const Body& d=w.space.bodies[size_t(door)];return d.q.unrotate(V3{post.x,post.y,1}-d.x);};V3 last=local();
 for(int i=0;i<int(1.5f/dt+.5f);++i){w.space.step(dt);r.events+=int(w.space.events.size());const V3 l=local();
  if(std::fabs(l.x)<.5f&&std::fabs(last.x)<.5f&&(l.y>0)!=(last.y>0))r.passed=true;last=l;r.worstPin=std::max(r.worstPin,w.worstPin());}
 r.endPin=w.worstPin();return r;}
// A 2 kg cage lamp (10 cm) on a chain of four 15 cm links (0.3 kg, 2 cm thick) from the ceiling, whipped at 20 m/s at a
// 3 cm pole 0.35 m out (the world's, or a fixed body): through it is any of them past the pole's middle while in line with it.
Through lampAtPole(float dt,bool poleAsBody){
 const V3 plo{.335f,-.015f,.5f},phi{.365f,.015f,2.98f};Jointed w(poleAsBody?V3{50,50,0}:plo,poleAsBody?V3{50.03f,50.03f,1}:phi,poleAsBody?0:2);std::vector<int> chain;
 for(int k=0;k<4;++k)chain.push_back(w.box({0,0,3-.075f-.15f*float(k)},{.01f,.01f,.075f},.3f));
 const int lamp=w.box({0,0,2.32f},{.05f,.05f,.08f},2);
 w.add(JointKind::Ball,chain[0],-1,{0,0,3});for(int k=1;k<4;++k)w.add(JointKind::Ball,chain[size_t(k)],chain[size_t(k-1)],{0,0,3-.15f*float(k)});w.add(JointKind::Ball,lamp,chain[3],{0,0,2.4f});
 if(poleAsBody){w.shapes.emplace_back();Shape& s=w.shapes.back();s.add(PartKind::Box,{},{.015f,.015f,1.24f});s.finish();Body b;b.shape=&s;b.kinematic=true;b.setMass(0);b.x={.35f,0,1.74f};b.updateInertia();b.id=int(w.space.bodies.size());w.space.bodies.push_back(b);}
 for(int k=0;k<4;++k)w.space.bodies[size_t(chain[size_t(k)])].v={20*float(k+1)/5,0,0};w.space.bodies[size_t(lamp)].v={20,0,0};
 Through r;for(int i=0;i<int(1.5f/dt+.5f);++i){w.space.step(dt);r.events+=int(w.space.events.size());
  for(int k=0;k<5;++k){const Body& b=w.space.bodies[size_t(k)];r.passed|=b.x.x>.35f&&std::fabs(b.x.y)<.04f;}r.worstPin=std::max(r.worstPin,w.worstPin());trace(poleAsBody?"lamp-body":"lamp-world",w,i,dt);}
 r.endPin=w.worstPin();return r;}
// The probes' bound (Settings::ccdProbes, ccdBudget; 0 and 0: the old way, every 2.5 cm through every piece of the turn).
// A blast: twenty crates of 0.3 to 1 m launched out from the middle of a 10 m pen of 3 cm walls at 8 to 20 m/s, turning at
// up to 15 rad/s, four 3 cm posts in their way, half a second at 120 steps a second. The world sweeps asked and their time.
struct Blast {long long sweeps=0;int worst=0;double ms=0,worstMs=0;int escaped=0;};
Blast blast(int probes,int budget){
 const float in=5;Jointed w({-in-.03f,-in-.03f,0},{-in,in+.03f,3},1);
 w.solids.push_back({{in,-in-.03f,0},{in+.03f,in+.03f,3},1});w.solids.push_back({{-in,-in-.03f,0},{in,-in,3},1});w.solids.push_back({{-in,in,0},{in,in+.03f,3},1});
 for(V3 p:{V3{3.6f,1.2f,0},V3{-3.6f,-1.2f,0},V3{1.2f,-3.6f,0},V3{-1.2f,3.6f,0}})w.solids.push_back({p-V3{.015f,.015f,0},p+V3{.015f,.015f,2.5f},2});
 w.space.settings.ccdProbes=probes;w.space.settings.ccdBudget=budget;
 for(int k=0;k<20;++k){const float s=.3f+.7f*float(k%7)/6.f,x=-2.2f+1.1f*float(k%5),y=-1.65f+1.1f*float(k/5);const int c=w.box({x,y,s*.5f+.01f},{s*.5f,s*.5f,s*.5f},20*s*s*s+2);
  Body& b=w.space.bodies[size_t(c)];const V3 out=norm(V3{x,y,0},{1,0,0});const float speed=8+12*float((k*7)%11)/10.f;b.v=out*speed+V3{0,0,2+float(k%3)};b.w={float(k%5)-2,float(k%3)*4-4,15*float((k*3)%5)/4.f-7.5f};}
 Blast r;const float dt=1.f/120;
 for(int i=0;i<60;++i){w.space.step(dt);r.sweeps+=w.space.ccdSweeps;r.worst=std::max(r.worst,w.space.ccdSweeps);r.ms+=w.space.ccdMs;r.worstMs=std::max(r.worstMs,w.space.ccdMs);}
 for(const auto& b:w.space.bodies)r.escaped+=std::fabs(b.x.x)>in||std::fabs(b.x.y)>in;
 return r;}
// One 1 m crate thrown at 35 m/s, turning at 10 rad/s (fast for its size: a quarter of it a step), at a wall 2 m off: the sweeps of its first step.
int oneCrate(int probes,int budget){Jointed w({2,-3,0},{2.03f,3,3},1);w.space.settings.ccdProbes=probes;w.space.settings.ccdBudget=budget;
 const int c=w.box({0,0,.6f},{.5f,.5f,.5f},40);w.space.bodies[size_t(c)].v={35,0,0};w.space.bodies[size_t(c)].w={0,0,10};w.space.step(1.f/120);return w.space.ccdSweeps;}

const bool ccd=add({.name="vectra-ccd",.summary="continuous collision: a spinning plank beside a 3 cm post, thin boards and crates thrown at each other at 25 m/s, a bottle at 30 m/s at a 3 cm wall",.standalone=[]{
 std::ofstream r("vectra-ccd.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){r<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;r.flush();};
 {const Spin w=spin(true),b=spin(false);
  auto say=[&](const char* what,const Spin& s){r<<"  "<<what<<": turned through it "<<s.through<<" times, deepest in it "<<f3(s.deepest*1000)<<" mm; after 1 s going "<<f3(len(s.endV))<<" m/s, spinning "<<f3(s.endW.z)<<" rad/s about its axis\n";};
  r<<"CORE: a plank spinning at 60 rad/s (0.5 rad a step) beside a 3 cm post\n";say("the post as the world (Space::sweep)",w);say("the post as a fixed body",b);
  check(w.struck&&w.through==0&&w.deepest<.005f,"ccd: a plank spinning at 60 rad/s does not turn through a 3 cm post (the world's, by Space::sweep)");
  check(b.struck&&b.through==0&&b.deepest<.005f,"ccd: a plank spinning at 60 rad/s does not turn through a 3 cm post (a fixed body)");
  // A quarter turn a step and no going at all (every point's way through the step then points behind it).
  const Spin q=spin(true,kPi*.5f*60,1.f/60);say("a quarter turn a step (94 rad/s at 60 Hz), the post as the world",q);
  check(q.struck&&q.through==0&&q.deepest<.005f,"ccd: a plank turning a quarter turn a step, not going, is stopped by a 3 cm post");
  const Spin wOld=spin(true,60,1.f/120,0),qOld=spin(true,kPi*.5f*60,1.f/60,0);
  r<<"  world sweeps over the second: spinning at 60 rad/s "<<w.sweeps<<" (unbounded "<<wOld.sweeps<<"), a quarter turn a step "<<q.sweeps<<" (unbounded "<<qOld.sweeps<<")\n";}
 // The probes' bound: a blast of twenty crates, and one 1 m crate.
 {const Blast now=blast(vectra::Settings{}.ccdProbes,vectra::Settings{}.ccdBudget),old=blast(0,0);const int one=oneCrate(vectra::Settings{}.ccdProbes,vectra::Settings{}.ccdBudget),oneOld=oneCrate(0,0);
  r<<"\nPROBES (Settings::ccdProbes "<<vectra::Settings{}.ccdProbes<<", ccdBudget "<<vectra::Settings{}.ccdBudget<<"; unbounded: every 2.5 cm along every edge, every piece of the turn)\n";
  auto say=[&](const char* what,const Blast& b){r<<"  "<<what<<": "<<b.sweeps<<" world sweeps in 0.5 s (worst step "<<b.worst<<"), "<<f3(b.ms)<<" ms in all (worst step "<<f3(b.worstMs)<<" ms), "<<b.escaped<<" crates out of the pen\n";};
  say("twenty crates launched by a blast, bounded",now);say("the same, unbounded",old);
  r<<"  a 1 m crate thrown at 35 m/s turning at 10 rad/s: "<<one<<" sweeps in its first step (unbounded "<<oneOld<<")\n";
  check(now.escaped==0&&old.escaped==0,"ccd: no crate of a blast gets out of a pen of 3 cm walls, bounded or not");
  check(now.worst<=vectra::Settings{}.ccdBudget+vectra::Settings{}.ccdProbes*8&&now.sweeps*4<old.sweeps,"ccd: a blast's world sweeps keep to the step's budget, a quarter of the unbounded ones or fewer");
  check(one*4<oneOld,"ccd: a fast 1 m crate asks a quarter of the unbounded sweeps or fewer");
  // In the game: the same blast in the regression room against the World (its own sweeps), bounded and (the knobs
  // props.ccd_probes 0, props.ccd_budget 0, from a tune file of this tube's own) unbounded.
  struct GameBlast {long long sweeps=0;int worst=0;double ms=0,worstMs=0;int out=0;};
  auto gameBlast=[&](bool unbounded){GameBlast b;std::string saved;{auto& tf=TuneFile::instance();std::lock_guard<std::mutex> hold(tf.lock);saved=tf.path;}
   if(unbounded){std::ofstream("vectra-ccd-unbounded.txt")<<"props.ccd_probes 0\nprops.ccd_budget 0\n";tuneFile("vectra-ccd-unbounded.txt");}
   forceLegacyProps(0);auto m=regression::room();m->definition.playerStart={2,2};kit::wall(*m,6,4,6.03f,14,0,3);kit::wall(*m,16,4,16.03f,14,0,3);kit::wall(*m,6,3.97f,16.03f,4,0,3);kit::wall(*m,6,14,16.03f,14.03f,0,3);
   Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};auto& list=LabAccess::clutter(g);list.clear();
   for(int k=0;k<20;++k){const float s=.3f+.7f*float(k%7)/6.f,x=11-2.2f+1.1f*float(k%5),y=9-1.65f+1.1f*float(k/5);auto c=weighing(prop(model::wooden_crate,{x,y},0),20*s*s*s+2,-1);c.dims={s,s,s};
    const float speed=8+12*float((k*7)%11)/10.f;const Point3 d=Point3{x-11,y-9,0}*(1/std::max(.01f,std::hypot(x-11,y-9)));c.velocity={d.x*speed,d.y*speed};c.vz=2+float(k%3);c.spin=15*float((k*3)%5)/4.f-7.5f;c.projectile=true;c.sleeping=false;list.push_back(c);}
   for(int i=0;i<30;++i){g.update({},1.f/60);const auto& st=LabAccess::rigidStats(g);b.sweeps+=st.ccdSweeps;b.worst=std::max(b.worst,st.ccdSweeps);b.ms+=st.ccdMs;b.worstMs=std::max(b.worstMs,st.ccdMs);}
   for(const auto& c:LabAccess::clutter(g))if(c.model>=0&&!c.broken)b.out+=c.pos.x<6||c.pos.x>16.03f||c.pos.y<3.97f||c.pos.y>14.03f;
   forceLegacyProps(-1);if(unbounded)tuneFile(saved);return b;};
  const GameBlast gNow=gameBlast(false),gOld=gameBlast(true);
  auto sayG=[&](const char* what,const GameBlast& b){r<<"  "<<what<<": "<<b.sweeps<<" world sweeps in 0.5 s (worst frame "<<b.worst<<"), "<<f3(b.ms)<<" ms in all (worst frame "<<f3(b.worstMs)<<" ms), "<<b.out<<" crates out of the pen\n";};
  sayG("in the game, twenty crates launched in a pen of 3 cm walls, bounded",gNow);sayG("the same, unbounded",gOld);
  check(gNow.out==0&&gNow.sweeps*4<gOld.sweeps,"ccd: in the game a blast of twenty crates stays in its pen with a quarter of the unbounded sweeps or fewer");}
 {float gap,va,vb;const bool met=boards(gap,va,vb);
  r<<"  two 2 cm boards at 25 m/s each, 0.32 m apart: "<<(met?"meet":"cross")<<"; after, 0.25 s on: "<<f3(gap)<<" m apart, going "<<f3(va)<<" and "<<f3(vb)<<" m/s\n";
  check(met&&gap>0&&va<0&&vb>0,"ccd: two thin boards flying at each other at 25 m/s meet and go back apart");}
 // Jointed islands: swept together, put back together (RM_CCD=off shows them go through).
 {r<<"\nJOINTED ISLANDS (60 and 120 steps a second)\n";
  auto say=[&](const std::string& what,const Through& t){r<<"  "<<what<<": "<<(t.passed?"WENT THROUGH":"held")<<", "<<t.events<<" impact events, the worst pin gap "<<f3(t.worstPin*1000)<<" mm, at the end "<<f3(t.endPin*1000)<<" mm\n";};
  for(float hz:{60.f,120.f}){const std::string at=" @"+std::to_string(int(hz))+" Hz";
   const Through rag=ragdollAtWall(1/hz),doorW=doorAtPost(1/hz,false),doorB=doorAtPost(1/hz,true),lamp=lampAtPole(1/hz,false),lampB=lampAtPole(1/hz,true);
   say("a ragdoll thrown at 15 m/s at a 3 cm wall"+at,rag);say("a door slammed at 15 rad/s into a 3 cm post (the world's)"+at,doorW);say("the same door into a 3 cm post (a fixed body)"+at,doorB);say("a lamp on a chain whipped at 20 m/s at a 3 cm pole (the world's)"+at,lamp);say("the same lamp at a 3 cm pole (a fixed body)"+at,lampB);
   check(!rag.passed&&rag.worstPin<.03f&&rag.endPin<.005f,"ccd: a ragdoll thrown at 15 m/s at a 3 cm wall stays on its side, its joints intact"+at);
   check(!doorW.passed&&!doorB.passed&&doorW.worstPin<.01f&&doorB.worstPin<.01f,"ccd: a door slammed into a 3 cm post never turns through it, its hinge pin intact"+at);
   check(!lamp.passed&&!lampB.passed&&lamp.worstPin<.03f&&lampB.worstPin<.03f&&lamp.endPin<.005f&&lampB.endPin<.005f,"ccd: a lamp whipped on its chain at a 3 cm pole never goes through it, the chain intact"+at);}}
 // The game: crates and a bottle (the door scene's 3 cm wall at x 15).
 {forceLegacyProps(0);auto m=regression::room();m->definition.playerStart={3,3};Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};auto& list=LabAccess::clutter(g);list.clear();
  for(int k=0;k<2;++k){auto c=prop(model::wooden_crate,{k?11.5f:8.5f,8},.4f);c.dims={.3f,.3f,.3f};c.velocity={k?-25.f:25.f,0};c.vz=0;c.projectile=true;c.sleeping=false;list.push_back(c);}
  bool crossed=false;float closest=1e9f;int frames=0;float firstA=0,firstB=0;bool turned=false;
  for(int i=0;i<60;++i){g.update({},1.f/60);const auto& L=LabAccess::clutter(g);crossed|=L[0].pos.x>L[1].pos.x;closest=std::min(closest,L[1].pos.x-L[0].pos.x);
   if(!turned&&(L[0].velocity.x<5||L[1].velocity.x>-5)){turned=true;frames=i+1;firstA=L[0].velocity.x;firstB=L[1].velocity.x;}}
  r<<"\nGAME\n  two 0.3 m crates thrown at each other at 25 m/s, 3 m apart: "<<(crossed?"one passed the other":"neither passes the other")<<", nearest middles "<<f3(closest)<<" m; they met at frame "<<frames<<" (going "<<f3(firstA)<<" and "<<f3(firstB)<<" m/s after)\n";
  check(!crossed&&turned&&closest>.25f,"ccd: two crates thrown at each other at 25 m/s both collide (neither passes or goes into the other)");
  forceLegacyProps(-1);}
 {forceLegacyProps(0);auto m=regression::room();m->definition.playerStart={3,3};kit::wall(*m,15,4,15.03f,10,0,3);Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};auto& list=LabAccess::clutter(g);list.clear();
  auto c=prop(model::glass_bottle,{13,6},.6f);c.velocity={30,0};c.vz=.5f;c.projectile=true;c.sleeping=false;list.push_back(c);
  bool past=false;float farthest=0;for(int i=0;i<60;++i){g.update({},1.f/60);const auto& b=LabAccess::clutter(g)[0];if(!b.broken&&b.model>=0){farthest=std::max(farthest,b.pos.x);past|=b.pos.x>15.03f;}}
  const auto& b=LabAccess::clutter(g)[0];
  r<<"  a bottle thrown at 30 m/s at a 3 cm wall (x 15): got to x "<<f3(farthest)<<(b.broken?", broke":"")<<"; "<<(past?"PASSED IT":"stopped")<<"\n";
  check(!past,"ccd: a bottle thrown at 30 m/s at a 3 cm wall never gets past it whole");
  forceLegacyProps(-1);}
 return ok;}});
}
}

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
//  - the game: two crates thrown at each other at 25 m/s meet and neither passes the other; a bottle thrown
//    at 30 m/s at a 3 cm wall never gets past it whole.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropQueries.h"
#include "core/Ray.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
struct Spin {int through=0;float deepest=0;bool struck=false;V3 endV{},endW{};};
Spin spin(bool world,float speed=60,float dt=1.f/120){
 Space space;space.settings.maxAngular=std::max(space.settings.maxAngular,speed*1.5f);Shape plank;plank.add(PartKind::Box,{},{.6f,.015f,.1f});plank.finish();Shape post;post.add(PartKind::Box,{},{.015f,.015f,.5f});post.finish();
 Body p;p.shape=&plank;p.material.restitution=.2f;p.linearDamping=p.angularDamping=0;p.gravity={};p.setMass(3);p.q=Quat::axisAngle({0,0,1},-.6f);p.w={0,0,speed};p.updateInertia();space.bodies.push_back(p);
 const V3 at{.45f,0,0},lo=at-V3{.015f,.015f,.5f},hi=at+V3{.015f,.015f,.5f};
 if(world)space.sweep=[&](Space&,int,V3 a,V3 b,V3& n)->float{return rayBoxFraction(a,b-a,lo,hi,&n);};
 else{Body s;s.shape=&post;s.kinematic=true;s.setMass(0);s.x=at;space.bodies.push_back(s);}
 Spin out;auto local=[&]{const auto& B=space.bodies[0];return B.q.unrotate(at-B.x);};V3 last=local();
 for(int i=0;i<int(1/dt+.5f);++i){space.step(dt);const V3 l=local();const auto& B=space.bodies[0];
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

const bool ccd=add({.name="vectra-ccd",.summary="continuous collision: a spinning plank beside a 3 cm post, thin boards and crates thrown at each other at 25 m/s, a bottle at 30 m/s at a 3 cm wall",.standalone=[]{
 std::ofstream r("vectra-ccd.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){r<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;r.flush();};
 {const Spin w=spin(true),b=spin(false);
  auto say=[&](const char* what,const Spin& s){r<<"  "<<what<<": turned through it "<<s.through<<" times, deepest in it "<<f3(s.deepest*1000)<<" mm; after 1 s going "<<f3(len(s.endV))<<" m/s, spinning "<<f3(s.endW.z)<<" rad/s about its axis\n";};
  r<<"CORE: a plank spinning at 60 rad/s (0.5 rad a step) beside a 3 cm post\n";say("the post as the world (Space::sweep)",w);say("the post as a fixed body",b);
  check(w.struck&&w.through==0&&w.deepest<.005f,"ccd: a plank spinning at 60 rad/s does not turn through a 3 cm post (the world's, by Space::sweep)");
  check(b.struck&&b.through==0&&b.deepest<.005f,"ccd: a plank spinning at 60 rad/s does not turn through a 3 cm post (a fixed body)");
  // A quarter turn a step and no going at all (every point's way through the step then points behind it).
  const Spin q=spin(true,kPi*.5f*60,1.f/60);say("a quarter turn a step (94 rad/s at 60 Hz), the post as the world",q);
  check(q.struck&&q.through==0&&q.deepest<.005f,"ccd: a plank turning a quarter turn a step, not going, is stopped by a 3 cm post");}
 {float gap,va,vb;const bool met=boards(gap,va,vb);
  r<<"  two 2 cm boards at 25 m/s each, 0.32 m apart: "<<(met?"meet":"cross")<<"; after, 0.25 s on: "<<f3(gap)<<" m apart, going "<<f3(va)<<" and "<<f3(vb)<<" m/s\n";
  check(met&&gap>0&&va<0&&vb>0,"ccd: two thin boards flying at each other at 25 m/s meet and go back apart");}
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

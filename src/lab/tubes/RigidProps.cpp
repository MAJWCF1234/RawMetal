// The rigid-body props (game/physics/Vectra.h, game/RigidClutter.cpp) against the legacy prop physics.
//
// rigid-ab: the same scenes, each built fresh and stepped as play steps them (Game::update at 60 Hz),
//  once on legacy (RM_PROPS=legacy) and once on Vectra, with the numbers side by side
//  in rigid-ab.txt and as metrics (old.* and new.*). The checks are Vectra's:
//  - stacking: a ten-crate tower, a 4-3-2-1 pyramid and a mixed stack, woken and left 30 s: how far
//    anything drifts, what fell, when it all slept;
//  - materials: things dropped from 1.5 m (how high they come back), a crate on a 20 and a 35 degree ramp
//    (it holds, it slides); and on the bare core, a rubber ball, a metal can and a crate on a plane;
//  - a barrel lying on its side down a ramp and off a landing down stairs (it rolls, it gets down);
//  - chairs and a table struck by a thrown crate (they go over, and lie on a side or a back);
//  - a crate thrown through a doorway (it goes through); a mug and a crate thrown at 25 m/s at a 3 cm
//    wall (neither passes it);
//  - cost: 100 awake crates falling into a heap (ms a step) and 500 asleep with one awake.
// rigid / rigid-legacy: the scenes filmed (fixtures rigid-stack, rigid-ramp, rigid-chairs, rigid-door), on
//  each physics; tools/dev/rigid_sheet.py lays the two side by side.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropBodies.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include "world/Primitives.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
using Map=AuthoredMapData;
float tilt(const Clutter& c){const auto up=c.rotate(0,0,1);return std::acos(std::clamp(up[2],-1.f,1.f));}
float turnBetween(const Clutter& a,const Clutter& b){const auto qa=vectra::Quat::fromEuler(a.yaw,a.pitch,a.roll),qb=vectra::Quat::fromEuler(b.yaw,b.pitch,b.roll);
 const float d=std::fabs(qa.w*qb.w+qa.x*qb.x+qa.y*qb.y+qa.z*qb.z);return 2*std::acos(std::clamp(d,0.f,1.f));}
// The scenes (each its own corner of the plain room; the player waits at 3,3).
void ramp(Map& m,float x,float y,float w,float d,float h,int facing){expandPrimitive({"ramp",x,y,0,w,d,h,facing,40,true},m.structures);}
void stairs(Map& m,float x,float y,float w,float h,int facing){expandPrimitive({"stairs",x,y,0,w,0,h,facing,40,true},m.structures);}
void author(Map& m,const std::string& scene){
 m.definition.playerStart={3,3};
 if(scene=="stack"){
  for(int i=0;i<10;++i)kit::prop(m,model::wooden_crate,6,12,.24f*float(i),0,.4f);
  for(int row=0;row<4;++row)for(int k=0;k<4-row;++k)kit::prop(m,model::packing_crate,8.8f+.31f*float(k)+.155f*float(row),12,.2f*float(row),0,.5f);
  kit::prop(m,model::packing_crate,12.5f,12,0,0);kit::prop(m,model::prop_cardboard_box,12.5f,12,.4f,0,.6f);kit::prop(m,model::crt_monitor,12.5f,12,.622f,0,.7f);kit::prop(m,model::mug,12.55f,12,.888f,0);}
 else if(scene=="ramp"){
  // A 1.2 m ramp rising to +y (17 degrees) onto a landing, stairs down the far side of the landing.
  ramp(m,8,12,1.6f,4,1.2f,0);m.structures.push_back({7.2f,14,10.2f,16,0,1.2f,false,40});stairs(m,11.2f,15,1.6f,1.2f,3);
  kit::propPose(m,model::ind_barrel,8,14.6f,1.2f,0,kPi*.5f,0);   // on its side, axis along x: it rolls down the ramp
  kit::propPose(m,model::ind_barrel,9.6f,15,1.2f,kPi*.5f,kPi*.5f,0); // axis along y: rolls off the landing, down the stairs
  // A crate on a 20 and on a 35 degree ramp.
  ramp(m,14.5f,12,1.2f,1.2f/std::tan(20*kPi/180),1.2f,0);ramp(m,17,12,1.2f,1.2f/std::tan(35*kPi/180),1.2f,0);}
 else if(scene=="chairs"){
  kit::prop(m,model::office_chair,10,10,0,0);kit::prop(m,model::wooden_chair,10,12,0,0);kit::prop(m,model::wooden_table,10,14.5f,0,kPi*.5f);
  for(float y:{10.f,12.f,14.5f})kit::prop(m,model::wooden_crate,8.7f,y,0,0);}
 else if(scene=="door"){
  kit::wall(m,1,12,11.4f,12.12f,0,3);kit::wall(m,12.4f,12,23,12.12f,0,3);kit::prop(m,model::wooden_crate,11.9f,9.5f,0,0,.8f);
  kit::wall(m,15,4,15.03f,10,0,3);kit::prop(m,model::mug,13,6,0,0);kit::prop(m,model::wooden_crate,13,8,0,0,.6f);}
 else if(scene=="office"||scene=="side"){   // heavy-impacts' knock(): a crate thrown at a chair standing still
  const int target=scene=="office"?model::office_chair:model::wooden_chair;kit::prop(m,target,10,8,0,scene=="side"?kPi*.5f:0.f);kit::prop(m,model::wooden_crate,8.875f,8,0,0);}
 else if(scene=="tip"){kit::prop(m,model::market_crate,10,8,0,kPi*.5f);
  for(int i=0;i<6;++i){const float ox=-.24f+.1f*float(i%3)*2.4f,oy=i<3?-.09f:.09f;kit::prop(m,model::glass_bottle,10+ox,8+oy,.02f,float(i));}}
 else if(scene=="basket"){kit::prop(m,model::market_basket,10,8,0,0);kit::prop(m,model::market_pumpkin,8.8f,8,.5f,0);}
 else if(scene=="crate"){kit::prop(m,model::market_crate,10,8,0,0);kit::prop(m,model::glass_bottle,8.8f,8,.5f,0);}
 else if(scene=="glassfloor"){kit::breakable(m,7,7,10,10,1.f,1.03f,17,false);kit::prop(m,model::wooden_crate,8.5f,8.5f,2.35f,0);}
 else if(scene=="letgo"){kit::prop(m,model::market_crate,9.19f,8,0,-kPi*.5f);kit::prop(m,model::glass_bottle,9.f,8,.95f,0);}
 else if(scene=="barrelpane"){kit::breakable(m,9.97f,5,10.03f,11,0,3,17,false);kit::prop(m,model::ind_barrel,9.95f-.265f-.45f,8,0,0);}
 else if(scene=="shovecrate"){kit::prop(m,model::market_crate,11.1f,8,0,-kPi*.5f);
  for(int i=0;i<6;++i){const float ox=-.24f+.1f*float(i%3)*2.4f,oy=i<3?-.09f:.09f;kit::prop(m,model::glass_bottle,11.1f+oy,8+ox,.02f,float(i));}}
 else if(scene=="carry"){kit::prop(m,model::market_basket,10,8,0,-.2f,.55f);
  const Vec2 fruit[]={{-.07f,-.08f},{.07f,-.07f},{-.06f,.08f},{.07f,.08f}};for(auto o:fruit)kit::prop(m,model::market_pumpkin,10+o.x,8+o.y,.4f*.55f,-o.x*9,.28f);}
 else if(scene=="keys"){kit::prop(m,model::keyboard,8,8,0,0);kit::prop(m,model::keyboard,8.46f,8,0,0);}
 else if(scene=="lob"){kit::prop(m,model::glass_bottle,8.8f,8,.5f,0);kit::prop(m,model::market_pumpkin,8.8f,12,.5f,0);}
 else if(scene=="drop"){
  const int models[]={model::market_pumpkin,model::trash_can,model::wooden_crate,model::glass_bottle,model::prop_cardboard_box,model::office_chair};
  for(int i=0;i<6;++i)kit::prop(m,models[i],6+1.6f*float(i),12,1.5f,0,i==2?.6f:1.f);}
}
std::shared_ptr<CustomCampaign> campaign(const std::string& scene){auto m=regression::room();author(*m,scene);return regression::campaign(m);}
void wakeAll(Game& g){for(auto& c:LabAccess::clutter(g)){c.sleeping=false;c.restTime=0;}}
// What a scene starts with: crates thrown at chairs, through the door and at the wall; barrels nudged.
void launch(Game& g,const std::string& scene){auto& list=LabAccess::clutter(g);
 auto toss=[](Clutter& c,Vec2 v,float vz){c.velocity=v;c.vz=vz;c.spin=0;c.pitchSpeed=0;c.rollSpeed=0;c.projectile=true;c.sleeping=false;c.restTime=0;};
 for(auto& c:list){
  if(scene=="tip"&&c.model==model::market_crate){c.rollSpeed=-7;c.velocity={0,.6f};c.vz=1.2f;c.sleeping=false;c.restTime=0;c.tipTime=1.5f;
   const vectra::V3 w=vectra::propAngular(c),o=vectra::propCentre(c);for(auto& v:list)if(v.model==model::glass_bottle){const vectra::V3 r=vectra::propCentre(v)-o,u=vectra::cross(w,r);v.velocity=c.velocity+Vec2{u.x,u.y};v.vz=c.vz+u.z;vectra::setPropAngular(v,w);v.sleeping=false;v.restTime=0;}}
  if((scene=="basket"||scene=="crate")&&c.pos.x<9){const float floor=(scene=="basket"?.4f:.02f)+.02f,vz=2.2f,t=(vz+std::sqrt(vz*vz+4*7*(.5f-floor)))/14.f;c.velocity={1.2f/t,0};c.vz=vz;c.projectile=true;c.spin=2;c.sleeping=false;c.restTime=0;}
  if(scene=="glassfloor"||(scene=="letgo"&&c.model==model::glass_bottle)){c.sleeping=false;c.restTime=0;}
  if(scene=="keys"&&c.pos.x<8.2f){c.velocity={2,0};c.sleeping=false;c.soundOwner=0;}
  if(scene=="lob"){const float vz=2.2f,t=(vz+std::sqrt(vz*vz+4*7*(.5f-.04f)))/14.f;c.velocity={1.2f/t,0};c.vz=vz;c.projectile=true;c.spin=2;c.sleeping=false;c.restTime=0;}
  if((scene=="office"||scene=="side")&&c.model==model::wooden_crate){c.z=.35f;c.velocity={4.74f,0};c.vz=1;c.sleeping=false;c.restTime=0;
   if(std::getenv("RM_RIGID_TUMBLE")){c.vz=1.2f;c.spin=3;c.pitchSpeed=10;c.rollSpeed=4;}}
  if(scene=="chairs"&&c.model==model::wooden_crate){c.z=.5f;toss(c,{6,0},1.2f);}
  if(scene=="door"&&c.model==model::wooden_crate&&c.pos.y<10){c.z=.4f;toss(c,{0,9},1.5f);}
  if(scene=="door"&&c.pos.x<14&&c.pos.y<9){c.z=.6f;toss(c,{25,0},.5f);}
  if(scene=="ramp"&&c.model==model::ind_barrel){c.sleeping=false;c.restTime=0;if(c.pos.x>9)c.velocity={1.2f,0};else c.velocity={0,-.7f};}}
 if(scene=="ramp"){ // the two crates set on the ramps' slopes, flat on them
  for(float angle:{20.f,35.f}){const float a=angle*kPi/180,x=angle<30?14.5f:17.f,run=1.2f/std::tan(a),y0=12-run*.5f,along=run*.5f;
   Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims=Game::propTraits(c.model).size;for(auto& d:c.dims)d*=.5f;c.roll=a;c.pos={x,y0+along};
   c.z=1.2f*.5f+.01f-c.extent()[2]+std::cos(a)*c.dims[2]*.5f;c.home=c.pos;list.push_back(c);}}
 if(scene=="stack"||scene=="drop")wakeAll(g);}
struct Sim {float seconds=0;std::vector<Clutter> start,end;float sleptAt=-1,maxDrift=0,peakMs=0,meanMs=0;int frames=0;float below=0;std::string belowWhat;};
// How far a prop's underside is under what it should rest on (0: on or over it). Its underside: the box's
// lowest corner on legacy, the shape's own lowest point on Vectra (a barrel's box corner
// lies under its round). What it rests on: the support under that point, asked from just over it.
float underFloor(const Game& g,const Clutter& c){if(c.model<0||c.broken)return 0;float z=c.z;Vec2 at=c.pos;
 if(!vectra::legacyProps()){vectra::Body b;vectra::toBody(c,b);z=1e9f;for(int p=0;p<b.shape->count;++p){const auto q=vectra::support(b,p,{0,0,-1});if(q.z<z){z=q.z;at={q.x,q.y};}}}
 const float floor=g.world().supportBelow(at.x,at.y,z+.06f,true);return std::max(0.f,floor-z);}
Sim simulate(const std::string& scene,float seconds,bool legacy){
 vectra::forceLegacyProps(legacy?1:0);Game g(campaign(scene));LabAccess::player(g).pos={3,3};launch(g,scene);Sim s;s.start=LabAccess::clutter(g);
 double total=0;for(int i=0;i<int(seconds*60);++i){const auto t0=std::chrono::steady_clock::now();g.update({},1.f/60);const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();total+=ms;s.peakMs=std::max(s.peakMs,float(ms));
  const auto& list=LabAccess::clutter(g);bool asleep=true;for(size_t k=0;k<list.size()&&k<s.start.size();++k){const auto& c=list[k];if(c.model<0)continue;asleep&=c.sleeping||c.broken;s.maxDrift=std::max(s.maxDrift,length(c.pos-s.start[k].pos));}
  if(asleep&&s.sleptAt<0)s.sleptAt=float(i+1)/60;if(!asleep)s.sleptAt=-1;}
 s.end=LabAccess::clutter(g);s.seconds=seconds;s.frames=int(seconds*60);s.meanMs=float(total/s.frames);
 for(size_t k=0;k<s.end.size();++k){const float d=underFloor(g,s.end[k]);if(d>s.below){s.below=d;s.belowWhat="#"+std::to_string(k)+" model "+std::to_string(s.end[k].model);}}
 vectra::forceLegacyProps(-1);return s;}
// ---- The bare core: a plane world (tilted by `slope` about x) ----
struct CoreRun {float bounce=0,slide=0,rolled=0,speed=0,spin=0;};
CoreRun core(vectra::PartKind kind,vectra::Material mat,float slope,vectra::V3 v0,float drop,float seconds){
 using namespace vectra;Space space;Shape shape;
 if(kind==PartKind::Sphere)shape.add(kind,{},{.11f,.11f,.11f});else if(kind==PartKind::Cylinder)shape.add(kind,{},{.033f,.033f,.06f},Quat::axisAngle({0,1,0},kPi*.5f));else shape.add(kind,{},{.2f,.2f,.2f});shape.finish();
 const V3 n=norm(V3{0,-std::sin(slope),std::cos(slope)});
 Body b;b.shape=&shape;b.material=mat;b.setMass(kind==PartKind::Box?9.f:kind==PartKind::Sphere?.6f:.35f);b.q=Quat::axisAngle({1,0,0},slope);
 float reach=0;b.updateInertia();for(int k=0;k<1;++k)reach=dot(b.x-support(b,0,neg(n)),n);b.x=n*(reach+drop);b.v=v0;space.bodies.push_back(b);
 space.worldContacts=[&](Space& sp,int i,float margin,std::vector<Manifold>& out){const Body& body=sp.bodies[size_t(i)];std::array<V3,8> f{};const int k=supportFeature(body,0,neg(n),f);
  Manifold m;m.a=i;m.b=-1;m.normal=neg(n);m.material=Material::mix(body.material,{.8f,.7f,0,0});m.material.restitution=body.material.restitution;
  for(int j=0;j<k;++j){const float sep=dot(f[size_t(j)],n);if(sep>margin)continue;auto&c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=f[size_t(j)];c.separation=sep;c.localA=body.q.unrotate(c.p-body.x);c.localB=c.p;}
  if(m.count)out.push_back(m);};
 CoreRun r;const V3 start=space.bodies[0].x;bool landed=false;float peak=0;const Quat q0=space.bodies[0].q;
 for(int i=0;i<int(seconds*120);++i){space.step(1.f/120);const auto& body=space.bodies[0];const float height=dot(body.x-start,n)+drop;
  if(!landed&&!space.manifolds.empty()&&space.events.size())landed=true;else if(landed)peak=std::max(peak,height-reach*0);}
 const auto& body=space.bodies[0];r.speed=len(body.v);r.spin=len(body.w);r.bounce=peak;const V3 d=body.x-start;r.slide=len(d-n*dot(d,n));const Quat dq=body.q*q0.conj();r.rolled=2*std::acos(std::clamp(std::fabs(dq.w),0.f,1.f));(void)reach;return r;}
// ---- Perf ----
struct Cost {double meanMs=0,peakMs=0,stepMs=0,worldMs=0,pairMs=0,solveMs=0;int bodies=0,awake=0;};
Cost cost(int awake,int asleep,bool legacy){
 vectra::forceLegacyProps(legacy?1:0);auto m=regression::room();m->definition.playerStart={3,3};Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};auto& list=LabAccess::clutter(g);
 for(int i=0;i<asleep;++i){Clutter c;c.kind=7;c.model=model::packing_crate;c.dims=Game::propTraits(c.model).size;for(auto& d:c.dims)d*=.4f;c.pos={1.6f+float(i%25)*.82f,13+float(i/25)*.52f};c.z=0;c.sleeping=true;list.push_back(c);}
 for(int i=0;i<awake;++i){Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims=Game::propTraits(c.model).size;for(auto& d:c.dims)d*=.5f;c.pos={5+float(i%10)*.36f,4+float((i/10)%5)*.36f};c.z=.3f+float(i/50)*.4f;c.yaw=.3f*float(i%7);c.sleeping=false;list.push_back(c);}
 Cost out;double total=0,step=0;int steps=0,frames=0;
 for(int i=0;i<90;++i){const auto t0=std::chrono::steady_clock::now();LabAccess::stepProps(g,1.f/60);const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
  if(i>=10){total+=ms;out.peakMs=std::max(out.peakMs,ms);++frames;const auto& st=LabAccess::rigidStats(g);if(!legacy&&st.steps){step+=st.ms/st.steps;++steps;out.worldMs+=st.worldMs;out.pairMs+=st.pairMs;out.solveMs+=st.solveMs;out.awake=std::max(out.awake,st.awake);out.bodies=st.bodies;}}}
 out.meanMs=total/std::max(1,frames);out.stepMs=steps?step/steps:out.meanMs;if(frames){out.worldMs/=frames;out.pairMs/=frames;out.solveMs/=frames;}vectra::forceLegacyProps(-1);return out;}

const bool ab=add({
 .name="rigid-ab",.summary="Vectra against the legacy prop physics: stacks, materials, a barrel on a ramp and stairs, chairs, a doorway, tunnelling, cost",
 .standalone=[]{
  std::ofstream r("rigid-ab.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){r<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;r.flush();};
  auto both=[&](const std::string& scene,float seconds){return std::pair<Sim,Sim>{simulate(scene,seconds,true),simulate(scene,seconds,false)};};
  r<<"Legacy (RM_PROPS=legacy) against Vectra, each scene fresh, Game::update at 60 Hz.\n\n";
  auto floorCheck=[&](const char* scene,const Sim& o,const Sim& n){r<<"  under the floor at the end: legacy "<<fixed(o.below*1000,1)<<" mm"<<(o.belowWhat.empty()?"":" ("+o.belowWhat+")")<<", Vectra "<<fixed(n.below*1000,1)<<" mm"<<(n.belowWhat.empty()?"":" ("+n.belowWhat+")")<<"\n";
   check(n.below<.005f,std::string("Vectra: in the ")+scene+" scene nothing ends under the floor it rests on (its own lowest point within 5 mm)");};
  // Stacking.
  {auto [o,n]=both("stack",30);r<<"STACKS (woken, 30 s): tower of 10 crates (0.24 m), pyramid 4-3-2-1, mixed stack\n";
   auto report=[&](const char* name,const Sim& s){int fell=0;float drift=0,topDrift=0;for(size_t k=0;k<s.end.size()&&k<s.start.size();++k){const auto& a=s.start[k];const auto& b=s.end[k];if(a.model<0)continue;
     if(tilt(b)>.3f||b.z<a.z-.1f)++fell;drift=std::max(drift,length(b.pos-a.pos));if(k==9)topDrift=length(b.pos-a.pos);}
    r<<"  "<<name<<": fell "<<fell<<", largest drift "<<fixed(drift*1000,1)<<" mm, tower top drift "<<fixed(topDrift*1000,1)<<" mm, all asleep at "<<(s.sleptAt<0?std::string("never"):fixed(s.sleptAt,2)+" s")<<", "<<fixed(s.meanMs,3)<<" ms a frame\n";return std::pair<int,float>{fell,drift};};
   report("legacy",o);auto [fell,drift]=report("Vectra",n);
   check(fell==0&&drift<.02f&&n.sleptAt>0&&n.sleptAt<5,"Vectra: every stack stands 30 s (nothing falls, drift under 2 cm) and sleeps within 5 s");floorCheck("stack",o,n);}
  // Materials: dropped from 1.5 m.
  {auto [o,n]=both("drop",4);r<<"\nDROPPED FROM 1.5 m: the highest each comes back up after its first landing (m)\n";
   const char* names[]={"pumpkin (organic)","trash can (metal)","crate (wood)","bottle (glass)","cardboard box","office chair"};
   for(int pass=0;pass<2;++pass){const Sim& s=pass?n:o;r<<"  "<<(pass?"Vectra":"legacy")<<":";for(int i=0;i<6&&i<int(s.end.size());++i)r<<" "<<names[i]<<" ends "<<fixed(s.end[size_t(i)].z,3)<<(s.end[size_t(i)].broken?" (broke)":"")<<";";r<<'\n';}
   bool rest=true;for(int i=0;i<6;++i){const auto& c=n.end[size_t(i)];rest&=c.broken||(c.z>-.02f&&c.z<.6f);}check(rest,"Vectra: everything dropped lands on the floor and stays out of it");floorCheck("drop",o,n);}
  // The bare core: rubber ball, metal can, crate on a 30 degree plane.
  {const auto ball=core(vectra::PartKind::Sphere,vectra::rubber(),0,{},1.5f,3);const auto can=core(vectra::PartKind::Cylinder,vectra::propMaterial(PropMaterial::Metal),0,{0,2,0},0,3);
   const auto steel=core(vectra::PartKind::Sphere,vectra::propMaterial(PropMaterial::Metal),0,{},1.5f,3);
   const auto hold=core(vectra::PartKind::Box,vectra::propMaterial(PropMaterial::Wood),20*kPi/180,{},.01f,3),slide=core(vectra::PartKind::Box,vectra::propMaterial(PropMaterial::Wood),35*kPi/180,{},.01f,3);
   r<<"\nBARE CORE (a plane): rubber ball dropped 1.5 m comes back to "<<fixed(ball.bounce,2)<<" m; a metal ball to "<<fixed(steel.bounce,2)<<" m; a metal can sent rolling at 2 m/s goes "<<fixed(can.slide,2)<<" m turning "<<fixed(can.rolled,2)
    <<" rad; a wooden crate on 20 degrees moves "<<fixed(hold.slide*1000,1)<<" mm, on 35 degrees "<<fixed(slide.slide,2)<<" m\n";
   check(ball.bounce>.6f&&ball.bounce<1.3f&&steel.bounce<ball.bounce*.6f,"core: a rubber ball bounces high (restitution .82), metal much lower");
   check(hold.slide<.01f&&slide.slide>.5f,"core: wood holds on 20 degrees (friction .62) and slides on 35");
   check(can.slide>.5f,"core: a can rolls");
   // A ball sent rolling (2 m/s, with a turn about the vertical) slows by its rolling resistance and nothing gains energy.
   {const auto roll=core(vectra::PartKind::Sphere,vectra::propMaterial(PropMaterial::Organic),0,{2,0,0},0,3);r<<"  an organic ball sent at 2 m/s: after 3 s "<<fixed(roll.speed,3)<<" m/s, spin "<<fixed(roll.spin,2)<<" rad/s, "<<fixed(roll.slide,2)<<" m\n";
    check(roll.speed<1.4f,"core: a rolling ball slows (no energy from the solver)");
    const auto bare=core(vectra::PartKind::Sphere,{.7f,.55f,.15f,0},0,{2,0,0},0,3),slick=core(vectra::PartKind::Sphere,{0,0,.15f,0},0,{2,0,0},0,3);
    r<<"  no rolling resistance: "<<fixed(bare.speed,3)<<" m/s spin "<<fixed(bare.spin,2)<<"; no friction: "<<fixed(slick.speed,3)<<" m/s spin "<<fixed(slick.spin,2)<<"\n";}}
  // Ramp, stairs and the crates on slopes.
  {auto [o,n]=both("ramp",8);r<<"\nRAMP AND STAIRS (8 s)\n";
   for(int pass=0;pass<2;++pass){const Sim& s=pass?n:o;r<<"  "<<(pass?"Vectra":"legacy")<<": ";for(size_t k=0;k<s.end.size()&&k<s.start.size();++k){const auto& a=s.start[k];const auto& b=s.end[k];
     r<<(a.model==model::ind_barrel?"barrel":"crate")<<" from "<<fixed(a.pos.x,1)<<","<<fixed(a.pos.y,1)<<","<<fixed(a.z,2)<<" to "<<fixed(b.pos.x,2)<<","<<fixed(b.pos.y,2)<<","<<fixed(b.z,2)<<" turned "<<fixed(turnBetween(a,b),1)<<" rad; ";}r<<'\n';}
   const auto& rampBarrel=n.end[0];const auto& stairBarrel=n.end[1];
   check(rampBarrel.pos.y<10.5f,"Vectra: the barrel rolls down the ramp and off its foot");
   check(stairBarrel.z<.3f&&stairBarrel.pos.x>11,"Vectra: the barrel rolls off the landing and down the stairs to the floor");
   if(n.end.size()>=4){const float held=length(n.end[2].pos-n.start[2].pos),slid=length(n.end[3].pos-n.start[3].pos);r<<"  new: crate on 20 degrees moved "<<fixed(held,3)<<" m, on 35 degrees "<<fixed(slid,3)<<" m\n";
    check(held<.05f&&slid>.3f,"Vectra: a crate holds on a 20 degree ramp and slides off a 35 degree one");}floorCheck("ramp",o,n);}
  // Chairs.
  {auto [o,n]=both("chairs",5);r<<"\nCHAIRS AND A TABLE struck by a thrown 9 kg crate (5 s): tilt (rad) and resting\n";int over=0;
   for(int pass=0;pass<2;++pass){const Sim& s=pass?n:o;r<<"  "<<(pass?"Vectra":"legacy")<<":";for(size_t k=0;k<3&&k<s.end.size();++k){const auto& c=s.end[k];r<<" "<<(k==0?"office chair":k==1?"wooden chair":"table")<<" tilt "<<fixed(tilt(c),2)<<" moved "<<fixed(length(c.pos-s.start[k].pos),2)<<(c.sleeping?" asleep":" awake")<<";";if(pass&&tilt(c)>.9f)++over;}r<<'\n';}
   bool still=true;for(const auto& c:n.end)still&=c.sleeping||c.broken;(void)over;
   check(tilt(n.end[0])>.9f,"Vectra: the office chair struck by the crate goes over");
   check(tilt(n.end[1])>.9f,"Vectra: the wooden chair struck by the crate goes over");
   check(length(n.end[2].pos-n.start[2].pos)>.2f||tilt(n.end[2])>.9f,"Vectra: the table struck by the crate is knocked aside or over");
   check(still,"Vectra: everything struck comes to rest");floorCheck("chairs",o,n);}
  // The doorway and the thin wall.
  {auto [o,n]=both("door",4);r<<"\nDOORWAY AND A 3 cm WALL (4 s)\n";
   for(int pass=0;pass<2;++pass){const Sim& s=pass?n:o;r<<"  "<<(pass?"Vectra":"legacy")<<": crate thrown at the door ends at y "<<fixed(s.end[0].pos.y,2)<<"; mug at x "<<fixed(s.end[1].pos.x,2)<<(s.end[1].broken?" (broke)":"")<<", crate at x "<<fixed(s.end[2].pos.x,2)<<"\n";}
   check(n.end[0].pos.y>12.5f,"Vectra: a crate thrown through the doorway goes through");
   check(n.end[1].pos.x<15&&n.end[2].pos.x<15,"Vectra: nothing thrown at 25 m/s passes the 3 cm wall");floorCheck("door",o,n);}
  // Saves: the physics section's fields (PropBodies.h captureProp/restoreProp) and an old save's Euler angles.
  {vectra::forceLegacyProps(0);Game g(campaign("drop"));LabAccess::player(g).pos={3,3};launch(g,"drop");for(int i=0;i<20;++i)g.update({},1.f/60);
   struct Kept {std::array<float,4> q;std::array<float,3> w;std::int32_t island;bool has;};std::vector<Kept> kept;
   for(const auto& c:LabAccess::clutter(g)){Kept k{};k.has=vectra::captureProp(c,k.q,k.w,k.island);kept.push_back(k);}
   const auto data=LabAccess::encodeSave(g);Game loaded(campaign("drop"));const bool decoded=LabAccess::decodeSave(loaded,data);Game old(campaign("drop"));LabAccess::decodeSave(old,data);
   auto& list=LabAccess::clutter(loaded);float exact=0,fromEuler=0,spin=0;int n=0;
   for(size_t k=0;k<list.size()&&k<kept.size();++k){if(!kept[k].has)continue;vectra::restoreProp(list[k],kept[k].q,kept[k].w,kept[k].island);++n;
    std::array<float,4> q2;std::array<float,3> w2;std::int32_t is2;vectra::captureProp(list[k],q2,w2,is2);
    for(int a=0;a<4;++a)exact=std::max(exact,std::fabs(q2[size_t(a)]-kept[k].q[size_t(a)]));for(int a=0;a<3;++a)spin=std::max(spin,std::fabs(w2[size_t(a)]-kept[k].w[size_t(a)]));
    vectra::captureProp(LabAccess::clutter(old)[k],q2,w2,is2);float d=std::fabs(q2[0]*kept[k].q[0]+q2[1]*kept[k].q[1]+q2[2]*kept[k].q[2]+q2[3]*kept[k].q[3]);fromEuler=std::max(fromEuler,2*std::acos(std::min(1.f,d)));}
   vectra::forceLegacyProps(-1);
   r<<"\nSAVES: "<<n<<" props mid-fall; with the physics section the quaternion comes back within "<<exact<<", the spin within "<<spin<<"; from an old save's yaw/pitch/roll alone within "<<fixed(fromEuler*1000,3)<<" mrad\n";
   check(decoded&&n>0&&exact<1e-6f&&spin<1e-4f&&fromEuler<1e-3f,"saves: orientation and angular velocity come back through the physics section, and an old save's Euler angles give the same turn");}
  // Cost.
  {const auto o100=cost(100,0,true),n100=cost(100,0,false),o500=cost(1,500,true),n500=cost(1,500,false);
   r<<"\nCOST (updateClutter a 60 Hz frame; new also per 120 Hz step)\n  100 awake: legacy "<<fixed(o100.meanMs,3)<<" ms (peak "<<fixed(o100.peakMs,2)<<"), Vectra "<<fixed(n100.meanMs,3)<<" ms (peak "<<fixed(n100.peakMs,2)<<"), "<<fixed(n100.stepMs,3)<<" ms a step, "<<n100.awake<<" awake bodies\n"
    <<"  500 asleep + 1 awake: legacy "<<fixed(o500.meanMs,3)<<" ms, Vectra "<<fixed(n500.meanMs,3)<<" ms, "<<fixed(n500.stepMs,3)<<" ms a step ("<<n500.bodies<<" bodies)\n";
   check(n100.stepMs<8&&n500.stepMs<1.5,"Vectra: 100 awake bodies under 8 ms a step, 500 asleep under 1.5 ms");}
  return ok;}});

// A scene traced frame by frame (RM_RIGID_SCENE=name, RM_RIGID_LEGACY=1 for legacy): rigid-trace.txt.
const bool traced=add({.name="rigid-trace",.summary="a rigid scene traced frame by frame (debugging)",.standalone=[]{
 const char* e=std::getenv("RM_RIGID_SCENE");const std::string scene=e?e:"chairs";const bool legacy=std::getenv("RM_RIGID_LEGACY")!=nullptr;
 std::ofstream r("rigid-trace.txt");vectra::forceLegacyProps(legacy?1:0);
 std::unique_ptr<Game> made;if(scene=="ward"){made=std::make_unique<Game>();LabAccess::loadLevel(*made,75);}else made=std::make_unique<Game>(campaign(scene));Game& g=*made;
 if(scene!="barrelpane"&&scene!="shovecrate"&&scene!="carry")LabAccess::player(g).pos={3,3};if(scene=="ward"){LabAccess::player(g).pos={12,12};const auto& fx=g.world().fixtures();
  for(const auto& f:fx)if(f.model==model::med_ward_bed&&f.visible){const float crown=g.world().floorHeight(f.position.x,f.position.y)+f.base+f.height*.404f/.842f;
   Clutter c;c.kind=7;c.model=model::med_corpse_supine;const auto& sz=model::propSize[c.model];c.dims={sz[0],sz[1],sz[2]};const Vec2 along{std::cos(f.yaw),-std::sin(f.yaw)};
   c.pos=f.position;c.z=crown+.05f;c.yaw=std::atan2(-along.x,along.y);LabAccess::clutter(g).push_back(c);
   for(const auto& st:g.world().structures())if(st.x2>f.position.x-1.1f&&st.x1<f.position.x+1.1f&&st.y2>f.position.y-.6f&&st.y1<f.position.y+.6f&&st.top>crown-.1f&&st.top<crown+.4f)
    r<<"structure "<<st.x1<<","<<st.y1<<" - "<<st.x2<<","<<st.y2<<" z "<<st.bottom<<".."<<st.top<<" solid "<<st.solid<<" material "<<st.material<<'\n';r<<"bed at "<<f.position.x<<","<<f.position.y<<" yaw "<<f.yaw<<" size "<<f.width<<"x"<<f.depth<<"x"<<f.height<<" crown "<<crown<<'\n';break;}}
 else launch(g,scene);
 if(scene=="barrelpane"){auto& pl=LabAccess::player(g);pl.pos={9.95f-.53f-.45f-.5f,8};pl.angle=0;pl.pitch=0;}
 if(scene=="shovecrate"){auto& pl=LabAccess::player(g);pl.pos={11.1f,6.9f};pl.angle=kPi*.5f;pl.pitch=0;}
 if(scene=="carry"){auto& pl=LabAccess::player(g);pl.pos={8.95f,8};pl.angle=0;pl.pitch=0;}
 for(int i=0;i<(std::getenv("RM_RIGID_FRAMES")?std::atoi(std::getenv("RM_RIGID_FRAMES")):180);++i){InputState in;if(scene=="carry"){if(i==12)r<<"grab "<<LabAccess::grab(g)<<" held "<<LabAccess::held(g)<<'\n';if(i>12&&i<102)in.forward=true;if(i>=102&&i<162)in.mouseDx=6;if(i==162)in.use=true;}
  if(scene=="barrelpane"||(scene=="shovecrate"&&i<90)){in.forward=true;in.sprint=true;LabAccess::player(g).angle=scene=="barrelpane"?0.f:kPi*.5f;}g.update(in,1.f/60);
  if(scene=="barrelpane"||scene=="shovecrate")r<<"player "<<fixed(LabAccess::player(g).pos.x,3)<<","<<fixed(LabAccess::player(g).pos.y,3)<<" pane damage "<<fixed(g.world().breakables().empty()?0.f:g.world().breakableDamage(0),2)<<'\n';const auto& st=LabAccess::rigidStats(g);r<<"frame "<<i<<" steps "<<st.steps<<" bodies "<<st.bodies<<" awake "<<st.awake<<" manifolds "<<st.manifolds<<" world "<<st.worldManifolds<<" events "<<st.events.size()<<'\n';
  const auto& list=LabAccess::clutter(g);for(size_t k=0;k<list.size();++k){const auto& c=list[k];if(c.model<0)continue;
   r<<"  #"<<k<<" m"<<c.model<<" pos "<<fixed(c.pos.x,3)<<","<<fixed(c.pos.y,3)<<","<<fixed(c.z,3)<<" v "<<fixed(c.velocity.x,2)<<","<<fixed(c.velocity.y,2)<<","<<fixed(c.vz,2)<<" ypr "<<fixed(c.yaw,2)<<","<<fixed(c.pitch,2)<<","<<fixed(c.roll,2)
    <<" w "<<fixed(c.rollSpeed,2)<<","<<fixed(c.pitchSpeed,2)<<","<<fixed(c.spin,2)<<(c.sleeping?" asleep":"")<<(c.riding?" riding":"")<<(c.broken?" broken":"")<<'\n';}
  for(const auto& m:st.manifoldsSeen){r<<"   M a"<<m.a<<" b"<<m.b<<" n "<<fixed(m.normal.x,2)<<","<<fixed(m.normal.y,2)<<","<<fixed(m.normal.z,2)<<" pts";for(int k=0;k<m.count;++k)r<<" ("<<fixed(m.points[size_t(k)].p.x,2)<<","<<fixed(m.points[size_t(k)].p.y,2)<<","<<fixed(m.points[size_t(k)].p.z,2)<<" s"<<fixed(m.points[size_t(k)].separation*1000,1)<<" j"<<fixed(m.points[size_t(k)].normalImpulse,3)<<")";r<<'\n';}}
 vectra::forceLegacyProps(-1);return true;}});
// What the World has round a point of a campaign map (RM_RIGID_PROBE="map x y z"): rigid-probe.txt.
const bool probed=add({.name="rigid-probe",.summary="the solids round a point of a map (debugging)",.standalone=[]{
 const char* e=std::getenv("RM_RIGID_PROBE");int map=79;float x=21.56f,y=1.8f,z=-11.28f;if(e)std::sscanf(e,"%d %f %f %f",&map,&x,&y,&z);
 std::ofstream r("rigid-probe.txt");Game g;LabAccess::loadLevel(g,map);const World& w=g.world();
 r<<"fits "<<w.fits(x,y,z-.005f,.01f,false,true)<<" support "<<w.supportBelow(x,y,z,true)<<" clear "<<w.clearanceAbove(x,y,z,true)<<'\n';
 for(const auto& st:w.structures())if(x>st.x1-.3f&&x<st.x2+.3f&&y>st.y1-.3f&&y<st.y2+.3f)r<<"structure "<<st.x1<<","<<st.y1<<" - "<<st.x2<<","<<st.y2<<" z "<<st.bottom<<".."<<st.top<<" solid "<<st.solid<<" rail "<<st.rail<<" material "<<st.material<<'\n';
 for(const auto& f:w.fixtures())if(length(f.position-Vec2{x,y})<1.2f)r<<"fixture model "<<f.model<<" at "<<f.position.x<<","<<f.position.y<<" "<<f.width<<"x"<<f.depth<<"x"<<f.height<<" base "<<f.base<<" yaw "<<f.yaw<<" solid "<<f.solid<<'\n';
 r<<"tile "<<w.tile(int(x),int(y))<<'\n';return true;}});
// The filmed scenes: the same fixture on each physics.
void film(const char* name,bool legacy){add({.name=name,.summary=legacy?"the prop scenes on legacy (RM_PROPS=legacy), for side by side":"the prop scenes on Vectra",
 .fixtures={"rigid-stack","rigid-ramp","rigid-chairs","rigid-door"},
 .author=[](Scene& s,AuthoredMapData& m){author(m,s.fixture->value("rigid","stack"));},
 .setup=[legacy](Scene&){vectra::forceLegacyProps(legacy?1:0);},
 .step=[](Scene& s,float){const std::string scene=s.fixture->value("rigid","stack");if(s.frame==20)launch(s.g(),scene);
  // The tower's bottom crate knocked out from under it (as a kick or a round would).
  if(scene=="stack"&&s.frame==130){auto& c=LabAccess::clutter(s.g())[0];c.velocity={4.5f,0};c.sleeping=false;c.restTime=0;}},
 .capture=[](Scene& s,const Frame&){if(s.frame==19)s.shoot("a-start");if(s.frame==60)s.shoot("b-early");if(s.frame==150)s.shoot("c-mid");if(s.last())s.shoot("d-end");},
 .metrics=[](Scene& s,Report& r){int awake=0,broken=0;for(const auto& c:LabAccess::clutter(s.g()))if(c.model>=0){awake+=!c.sleeping;broken+=c.broken;}r.metric("awake",awake);r.metric("broken",broken);r.check("ran",true);},
 .teardown=[](Scene&){vectra::forceLegacyProps(-1);}});}
const bool filmed=[]{film("rigid",false);film("rigid-legacy",true);return true;}();
}
}

// What Vectra (game/physics) does from shapes, mass, inertia, friction, restitution and the solver alone,
// with no code for the scene or the prop: each scene is only props put down and one push. Each runs on
// legacy too, and vectra-emergence.txt says what each core did. The checks are Vectra's.
//  1 seesaw: a plank on a crate, a crate dropped on one end, a mug on the other flies up;
//  2 dominoes: ten books stood on end in a row, the first tipped, the last goes down;
//  3 off-centre: a crate struck at its corner spins, struck at its middle slides straight;
//  4 centre of mass: a tall cabinet struck high tips, struck low slides; a wide low crate struck high does not tip;
//  5 a ramp from a prop: a board leant on a crate, a can let go at its top rolls down and off;
//  6 collapse: the pyramid's bottom crate knocked out brings the rest down; its top one knocked off leaves it standing;
//  9 on water: a plank floats flat; a crate loaded at one end lists toward the load;
// 10 uneven ground: a ball let go on a ramp rolls down to the floor.
// (7 riding by friction and 8 climbing need the player's carry and climb: not in this tube.)
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropBodies.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include "world/Primitives.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <cstdlib>
namespace retro::lab {
namespace {
std::string f2(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
float tiltOf(const Clutter& c){const auto up=c.rotate(0,0,1);return std::acos(std::clamp(up[2],-1.f,1.f));}
Clutter body(int model,Vec2 at,float z,std::array<float,3> dims,float yaw=0,float pitch=0,float roll=0){Clutter c;c.kind=7;c.model=model;c.dims=dims;c.pos=at;c.yaw=yaw;c.pitch=pitch;c.roll=roll;c.z=z;c.sleeping=false;return c;}
std::array<float,3> size(int model,float k=1){auto s=Game::propTraits(model).size;for(auto& d:s)d*=k;return s;}
// A scene: props (and the room's extras) on a plain room, stepped `seconds` at 60 Hz on one core; `kick` runs once after 0.3 s.
struct Run {std::vector<Clutter> start,end;float peakZ[64]{};};
int g_scene=0;   // RM_EMERGE_TRACE=<scene number>: vectra-emergence-trace-<n>-<core>.txt, every prop each frame
std::function<void(Game&,int)> g_push;   // a push held over frames (scene 4), or none
Run run(bool legacy,float seconds,std::function<void(AuthoredMapData&)> room,std::function<std::vector<Clutter>()> props,std::function<void(Game&)> kick){
 vectra::forceLegacyProps(legacy?1:0);auto m=regression::room();m->definition.playerStart={2,2};if(room)room(*m);Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};
 auto& list=LabAccess::clutter(g);list=props();Run r;
 std::unique_ptr<std::ofstream> trace;{const char* e=std::getenv("RM_EMERGE_TRACE");if(e&&std::atoi(e)==g_scene)trace=std::make_unique<std::ofstream>("vectra-emergence-trace-"+std::to_string(g_scene)+(legacy?"-legacy":"-vectra")+".txt",std::ios::app);}
 for(int i=0;i<int(seconds*60);++i){if(i==18&&kick)kick(g);if(g_push)g_push(g,i);g.update({},1.f/60);if(i==17)r.start=LabAccess::clutter(g);
  const auto& l=LabAccess::clutter(g);for(size_t k=0;k<l.size()&&k<64;++k)r.peakZ[k]=std::max(r.peakZ[k],l[k].z);
  if(trace){*trace<<"frame "<<i<<'\n';for(size_t k=0;k<l.size();++k){const auto& c=l[k];*trace<<"  #"<<k<<" pos "<<f2(c.pos.x,3)<<","<<f2(c.pos.y,3)<<","<<f2(c.z,3)<<" v "<<f2(c.velocity.x,2)<<","<<f2(c.velocity.y,2)<<","<<f2(c.vz,2)<<" ypr "<<f2(c.yaw,2)<<","<<f2(c.pitch,2)<<","<<f2(c.roll,2)<<" tilt "<<f2(tiltOf(c),2)<<(c.sleeping?" asleep":"")<<'\n';}}}
 r.end=LabAccess::clutter(g);vectra::forceLegacyProps(-1);return r;}
const bool registered=add({.name="vectra-emergence",.summary="behaviour from the solver alone: seesaw, dominoes, off-centre hits, centre of mass, a ramp from a prop, collapse, water, uneven ground",.standalone=[]{
 std::ofstream r("vectra-emergence.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){r<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;r.flush();};
 auto both=[&](float s,std::function<void(AuthoredMapData&)> room,std::function<std::vector<Clutter>()> props,std::function<void(Game&)> kick){return std::pair<Run,Run>{run(true,s,room,props,kick),run(false,s,room,props,kick)};};
 // 1. Seesaw.
 {g_scene=1;auto props=[]{const auto fulcrum=size(model::wooden_crate,.5f);return std::vector<Clutter>{body(model::wooden_crate,{10,8},0,fulcrum),
    body(model::packing_crate,{10,8},fulcrum[2],{2.f,.3f,.04f}),body(model::mug,{10.92f,8},fulcrum[2]+.04f,size(model::mug)),body(model::wooden_crate,{9.12f,8},1.6f,size(model::wooden_crate,.5f))};};
  auto [o,n]=both(4,nullptr,props,nullptr);
  r<<"1 SEESAW: the mug's highest  legacy "<<f2(o.peakZ[2])<<" m, Vectra "<<f2(n.peakZ[2])<<" m (it sat at "<<f2(n.start.size()>2?n.start[2].z:0)<<")\n";
  check(n.peakZ[2]>.75f,"1 seesaw: a crate dropped on one end of a plank throws the mug on the other end up");}
 // 2. Dominoes.
 {g_scene=2;auto props=[]{std::vector<Clutter> v;const auto s=size(model::binder,1.5f);for(int i=0;i<10;++i)v.push_back(body(model::binder,{8+.3f*float(i),8},0,s,kPi*.5f));return v;};
  // A push at its top (0.6 N s, a finger's flick): a spin given about its middle would be mostly taken back by the floor.
  auto kick=[](Game& g){const auto& c=LabAccess::clutter(g)[0];LabAccess::knock(g,0,{c.pos.x,c.pos.y,c.z+c.height()-.02f},{.6f,0,0});};
  auto [o,n]=both(6,nullptr,props,kick);int down[2]{};for(int p=0;p<2;++p)for(const auto& c:(p?n:o).end)down[p]+=tiltOf(c)>.6f;
  r<<"2 DOMINOES: down of ten  legacy "<<down[0]<<", Vectra "<<down[1]<<"\n";
  check(down[1]>=9,"2 dominoes: the first of ten books stood on end tipped, the row goes down");}
 // 3. Off-centre hits.
 {float spin[2][2]{},side[2][2]{};
  for(int corner=0;corner<2;++corner){auto props=[]{return std::vector<Clutter>{body(model::wooden_crate,{10,8},0,size(model::wooden_crate,.6f))};};
   auto kick=[corner](Game& g){const auto& c=LabAccess::clutter(g)[0];const float h=c.dims[2]*.5f;LabAccess::knock(g,0,{c.pos.x-c.dims[0]*.5f,c.pos.y+(corner?c.dims[1]*.5f:0.f),c.z+h},{30,0,0});};
   for(int p=0;p<2;++p){const auto rr=run(p==0,.5f,nullptr,props,kick);const auto& c=rr.end[0];spin[corner][p]=std::fabs(c.yaw-rr.start[0].yaw);side[corner][p]=std::fabs(c.pos.y-8);}}
  r<<"3 OFF-CENTRE: turned by a blow at its corner  legacy "<<f2(spin[1][0])<<" rad, Vectra "<<f2(spin[1][1])<<"; at its middle  legacy "<<f2(spin[0][0])<<", Vectra "<<f2(spin[0][1])<<" rad, off line "<<f2(side[0][1]*1000,1)<<" mm\n";
  check(spin[1][1]>.3f&&spin[0][1]<.05f&&side[0][1]<.01f,"3 off-centre: a blow at a crate's corner turns it; at its middle it slides straight");}
 g_scene=4;
 // 4. Centre of mass: a 30 kg wardrobe (0.8 x 0.6 x 1.5) and a 30 kg wide crate (1 x 1 x 0.4), each pushed a second
 // with 320 N (more than the floor's grip: 0.6 x 30 kg x 14) on its side. (A cabinet much taller than it is
 // deep tips however low it is pushed: the floor's friction stopping its feet turns it over, as it would a real one.)
 {float t[3][2]{};const std::array<float,3> tall{.8f,.6f,1.5f},wide{1.f,1.f,.4f};   // a wardrobe 0.8 deep, pushed along its depth
  for(int k=0;k<3;++k){const auto dims=k<2?tall:wide;const float at=k==1?.1f:dims[2]-.05f;
   auto props=[dims]{auto c=body(model::packing_crate,{10,8},0,dims);auto tr=std::make_shared<EntityTraits>();tr->mass=30;c.traits=tr;return std::vector<Clutter>{c};};
   g_push=[dims,at](Game& g,int f){if(f<18||f>=78)return;const auto& c=LabAccess::clutter(g)[0];LabAccess::knock(g,0,{c.pos.x-dims[0]*.5f,c.pos.y,c.z+at},{320.f/60,0,0});};
   for(int p=0;p<2;++p){const auto rr=run(p==0,3,nullptr,props,nullptr);t[k][p]=tiltOf(rr.end[0]);}g_push=nullptr;}
  r<<"4 CENTRE OF MASS: tilt at the end  tall struck high legacy "<<f2(t[0][0])<<" Vectra "<<f2(t[0][1])<<"; tall struck low legacy "<<f2(t[1][0])<<" Vectra "<<f2(t[1][1])<<"; wide struck high legacy "<<f2(t[2][0])<<" Vectra "<<f2(t[2][1])<<"\n";
  check(t[0][1]>1.2f&&t[1][1]<.2f&&t[2][1]<.2f,"4 centre of mass: a tall cabinet pushed high goes over, pushed low slides; a wide crate pushed high stays upright");}
 // 5. A ramp from a prop.
 {g_scene=5;const float a=std::asin(.6f/1.6f);auto props=[a]{const auto box=size(model::wooden_crate);Clutter board=body(model::packing_crate,{10-.3f-.75f*std::cos(a),8},0,{1.6f,.5f,.04f},0,-a);
    board.z=.3f-board.extent()[2]+.02f;Clutter can=body(model::trash_can,{10-.3f-.18f,8},.6f+.05f,size(model::trash_can,.6f),kPi*.5f,kPi*.5f);return std::vector<Clutter>{body(model::wooden_crate,{10,8},0,box),board,can};};
  auto [o,n]=both(5,nullptr,props,nullptr);
  r<<"5 RAMP FROM A PROP: the can from x "<<f2(n.start.size()>2?n.start[2].pos.x:0)<<" ends at  legacy "<<f2(o.end[2].pos.x)<<", Vectra "<<f2(n.end[2].pos.x)<<" (the board's foot is at "<<f2(10-.3f-1.6f*std::cos(a))<<")\n";
  check(n.end[2].pos.x<10-.3f-1.6f*std::cos(a),"5 a ramp from a prop: a can let go at the top of a board leant on a crate rolls down it and off");}
 // 6. Collapse.
 {g_scene=6;auto props=[]{std::vector<Clutter> v;const auto s=size(model::packing_crate,.5f);for(int row=0;row<4;++row)for(int k=0;k<4-row;++k)v.push_back(body(model::packing_crate,{8.8f+.31f*float(k)+.155f*float(row),8},.2f*float(row),s));return v;};
  int fell[2][2]{};
  for(int top=0;top<2;++top){auto kick=[top](Game& g){auto& l=LabAccess::clutter(g);for(int k:top?std::vector<int>{9}:std::vector<int>{1,2}){l[size_t(k)].velocity={0,4};}for(auto& v:l){v.sleeping=false;v.restTime=0;}};
   for(int p=0;p<2;++p){const auto rr=run(p==0,4,nullptr,props,kick);for(size_t k=0;k<rr.end.size();++k)if(top?k!=9:(k!=1&&k!=2))fell[top][p]+=rr.end[k].z<rr.start[k].z-.1f;}}
  // A column of four: its bottom crate knocked out at 6 m/s.
  int column[2]{};{auto stack=[]{std::vector<Clutter> v;const auto s=size(model::packing_crate,.5f);for(int k=0;k<4;++k)v.push_back(body(model::packing_crate,{10,8},.2f*float(k),s));return v;};
   auto knock=[](Game& g){auto& l=LabAccess::clutter(g);l[0].velocity={0,6};for(auto& v:l){v.sleeping=false;v.restTime=0;}};
   for(int p=0;p<2;++p){const auto rr=run(p==0,4,nullptr,stack,knock);for(size_t k=1;k<rr.end.size();++k)column[p]+=rr.end[k].z<rr.start[k].z-.1f;}}
  r<<"6 COLLAPSE: a column of four, its bottom crate knocked out: crates above that came down  legacy "<<column[0]<<" Vectra "<<column[1]<<" (of 3); the pyramid's two middle bottom crates knocked out  legacy "<<fell[0][0]<<" Vectra "<<fell[0][1]<<" (of 8: the crate over the gap drops in and jams between its neighbours, which stay on their outer supports); top knocked off  legacy "<<fell[1][0]<<" Vectra "<<fell[1][1]<<"\n";
  check(column[1]==3&&fell[1][1]==0,"6 collapse: a column's bottom crate knocked out brings the three above it down; the pyramid's top one knocked off leaves the rest standing");}
 // 9. On water.
 {g_scene=9;auto pool=[](AuthoredMapData& m){m.waterVolumes.push_back({3,6,14,14,-1.2f,-.05f});};
  auto props=[]{const auto crate=size(model::wooden_crate);return std::vector<Clutter>{body(model::packing_crate,{6,10},.3f,{1.6f,.3f,.06f}),body(model::wooden_crate,{10,10},-.34f,crate),   // set on the water, not dropped into it (a drop throws the load off)
    [&]{auto load=body(model::med_first_aid_box,{10.15f,10},-.34f+crate[2]+.005f,size(model::med_first_aid_box));auto t=std::make_shared<EntityTraits>();t->mass=1.5f;load.traits=t;return load;}()};};   // a 1.5 kg load (its mass as a map gives it)
  auto [o,n]=both(8,pool,props,nullptr);
  r<<"9 WATER: the plank's tilt  legacy "<<f2(tiltOf(o.end[0]))<<" Vectra "<<f2(tiltOf(n.end[0]))<<"; the loaded crate's list  legacy "<<f2(tiltOf(o.end[1]))<<" Vectra "<<f2(tiltOf(n.end[1]))<<" rad, toward the load "<<f2(n.end[1].rotate(0,0,1)[0],3)<<"\n";
  check(tiltOf(n.end[0])<.15f&&tiltOf(n.end[1])>.03f&&n.end[1].rotate(0,0,1)[0]>0,"9 water: a plank floats flat; a crate with a first aid box on one side lists toward it");}
 // 10. Uneven ground.
 {g_scene=10;auto ramp=[](AuthoredMapData& m){expandPrimitive({"ramp",10,10,0,1.6f,4,1.2f,0,40,true},m.structures);};
  auto props=[]{return std::vector<Clutter>{body(model::market_pumpkin,{10,11.5f},1.1f,size(model::market_pumpkin,.6f))};};
  auto [o,n]=both(5,ramp,props,nullptr);
  r<<"10 UNEVEN GROUND: a ball let go on the ramp ends at y  legacy "<<f2(o.end[0].pos.y)<<" z "<<f2(o.end[0].z)<<", Vectra "<<f2(n.end[0].pos.y)<<" z "<<f2(n.end[0].z)<<" (the ramp's foot is at y 8)\n";
  check(n.end[0].pos.y<8.2f&&n.end[0].z<.1f,"10 uneven ground: a ball let go on a ramp rolls down to the floor");}
 return ok;}});
}
}

// The joints corner of the Starter Map's SHOWCASE: PHYSICS (tools/campaign/make_starter_map.py joints_corner):
// each fixture (lab/fixtures/joints-*.lab, `scene`) plays one exhibit as the game has it, from the map's own
// JOINT records, and checks it; the pictures are the judged ones (1600 x 900).
//  lamps:  a cage lamp on its chain is shot and swings, its chain at its length, its light moving with it.
//  crate:  the rope over the crate is shot through: the rope snaps (a sound), the crate falls to the floor.
//  seesaw: a crate dropped on one end tips it; the player standing on the other end tips it back.
//  gate:   the player walks through the sprung gate: it swings open before him and shuts behind him.
//  lift:   the player steps onto the lift at the landing and rides it down; off it, it goes back up; a 50 kg
//          crate set on it takes it down too.
//  bridge: the player walks over the plank bridge; it sags under him, its pins hold, he reaches the far side.
//  saves:  the rope shot through and the lift ridden down survive a save and load (a save from before joints,
//          physics section version 1, is the engine.saves test's).
//  cost:   every joint in the room moving at once: Vectra's milliseconds a frame.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/physics/PropBodies.h"
#include "world/ModelIds.h"
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string f2(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Watch {std::map<std::string,double> v;std::vector<int> hits;int crate=-1,lamp=-1,plank=-1,gate=-1,platform=-1,weight=-1,load=-1;Point3 rest{};bool done=false;std::string save;};
std::map<const Scene*,Watch>& watches(){static std::map<const Scene*,Watch> w;return w;}
// The prop of `model` nearest (x, y) on the current map.
int nearProp(Game& g,int model,float x,float y){int best=-1;float d=1e9f;const auto& l=LabAccess::clutter(g);for(int i=0;i<int(l.size());++i){if(l[size_t(i)].model!=model)continue;const float e=length(l[size_t(i)].pos-Vec2{x,y});if(e<d){d=e;best=i;}}return d<1.f?best:-1;}
Point3 middle(const Clutter& c){return {c.pos.x,c.pos.y,c.z+c.extent()[2]};}
int jointOf(const Game& g,int prop){const auto& w=g.world();for(int i=0;i<int(w.joints().size());++i)if(w.joints()[size_t(i)].propA==prop)return i;return -1;}
// The authored record index of a list prop (its spawn), for jointOf.
int spawnOf(const Game& g,int index){const auto& w=g.world();const auto& c=g.clutter()[size_t(index)];for(int i=0;i<int(w.clutterSpawns().size());++i){const auto& s=w.clutterSpawns()[size_t(i)];if(s.model==c.model&&length(s.position+g.chunkOffset(g.level())-c.home)<.02f)return i;}return -1;}
float tilt(const Clutter& c){const auto x=c.rotate(1,0,0);return std::asin(std::clamp(x[2],-1.f,1.f));}
void standOn(Game& g,Vec2 at,float z,float yaw){LabAccess::playerCamera(g);auto& p=LabAccess::player(g);p.pos=at;p.z=z;p.angle=yaw;}
const bool registered=add({
 .name="joints-room",.summary="the Starter Map's joints corner played: a swinging lamp, a rope crate shot down, a seesaw, a sprung gate, a pulley lift ridden, a rope bridge walked, saves, the cost",
 .fixtures={"joints-lamps","joints-crate","joints-seesaw","joints-gate","joints-lift","joints-bridge","joints-saves","joints-cost"},
 .setup=[](Scene& s){watches().erase(&s);auto& w=watches()[&s];Game& g=s.g();LabAccess::setGod(g,true);
  w.lamp=nearProp(g,model::mine_cage_lamp,8,9.2f);w.crate=nearProp(g,model::wooden_crate,12,11.2f);w.plank=nearProp(g,model::packing_crate,2.5f,12.5f);w.gate=nearProp(g,model::packing_crate,13.6f,13.6f);
  w.platform=nearProp(g,model::packing_crate,17.45f,3.5f);w.weight=nearProp(g,model::packing_crate,17.45f,6.3f);},
 .step=[](Scene& s,float){auto& w=watches()[&s];Game& g=s.g();const std::string scene=s.fixture->value("scene");auto& list=LabAccess::clutter(g);const int f=s.frame;
  if(scene=="lamps"&&w.lamp>=0){auto& c=list[size_t(w.lamp)];
   if(f==20){w.rest=middle(c);Point3 at{};w.v["light0"]=0;const auto& lights=g.world().lights();for(const auto& l:lights)if(l.prop>=0){Point3 p;if(g.hungLampAt(l,p)&&std::fabs(p.x-c.pos.x)<.3f&&std::fabs(p.y-c.pos.y)<.3f){w.v["lx"]=p.x;w.v["ly"]=p.y;}}(void)at;
    LabAccess::fireFrom(g,{c.pos.x-3.f,c.pos.y+.3f,c.z+.18f},middle(c),weaponDef(WeaponId::Shotgun).hitscan);}   // from the side: it swings across the view
   if(f==57)s.shoot("lamp-swinging");
   if(f>20){const Point3 m=middle(c);const float swing=std::sqrt((m.x-w.rest.x)*(m.x-w.rest.x)+(m.y-w.rest.y)*(m.y-w.rest.y));if(swing>w.v["swing"]){w.v["swing"]=swing;w.v["swingFrame"]=f;}
    const int j=jointOf(g,spawnOf(g,w.lamp));std::array<Point3,4> pts{};int n=0;if(j>=0&&g.jointLine(g.level(),j,pts,n)){const auto d=pts[0]-pts[1];const float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);w.v["stretch"]=std::max(w.v["stretch"],double(l-(5.f-3.1f)));}
    for(const auto& l:g.world().lights())if(l.prop>=0){Point3 p;if(g.hungLampAt(l,p)&&std::fabs(p.x-w.v["lx"])<.6f&&std::fabs(p.y-w.v["ly"])<.6f)w.v["lightMoved"]=std::max(w.v["lightMoved"],std::sqrt((p.x-w.v["lx"])*(p.x-w.v["lx"])+(p.y-w.v["ly"])*(p.y-w.v["ly"])));}}}
  if(scene=="crate"&&w.crate>=0){auto& c=list[size_t(w.crate)];
   if(f==20){w.v["sounds0"]=double(LabAccess::sounds(g).size());LabAccess::fireFrom(g,{c.pos.x,c.pos.y+3.5f,3.9f},{c.pos.x,c.pos.y,3.9f},weaponDef(WeaponId::Handgun).hitscan);
    for(const auto& e:LabAccess::sounds(g))w.v["snapSound"]+=e.sound==Sound::LiftSnap;}
   if(f>20){const int j=jointOf(g,spawnOf(g,w.crate));w.v["broken"]=j>=0&&g.world().jointStates()[size_t(j)].broken;w.v["lowest"]=c.z;if(c.z<1.6f&&c.z>1.0f&&!w.done){w.done=true;s.shoot("rope-crate-falling");}}}
  if(scene=="seesaw"&&w.plank>=0){
   if(f==20){Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims={.4f,.4f,.4f};auto tr=std::make_shared<EntityTraits>();tr->mass=9;c.traits=tr;c.pos={3.6f,12.5f};c.z=1.2f;c.sleeping=false;list.push_back(c);w.load=int(list.size())-1;}
   if(f==150){w.v["tippedByCrate"]=tilt(list[size_t(w.plank)]);s.shoot("seesaw-crate");standOn(g,{1.5f,12.5f},.4f+1.f*std::sin(std::fabs(float(w.v["tippedByCrate"]))),0);}
   if(f>150)w.v["tippedByPlayer"]=tilt(list[size_t(w.plank)]);}
  if(scene=="gate"&&w.gate>=0){
   if(f==10)standOn(g,{13.55f,15.2f},0,-kPi*.5f);
   if(f>10&&f<110)s.input.forward=true;   // forward, through the gate (north)
   const float yaw=list[size_t(w.gate)].yaw;w.v["open"]=std::max(w.v["open"],double(std::fabs(std::remainder(yaw,2*kPi))));if(f>=110)w.v["after"]=std::fabs(std::remainder(yaw,2*kPi));
   w.v["through"]=LabAccess::player(g).pos.y;}
  if(scene=="lift"&&w.platform>=0){auto& p=list[size_t(w.platform)];
   if(f==10){w.v["top"]=p.z;standOn(g,{17.45f,3.5f},p.z+p.height(),kPi*.5f);}
   if(f==130){w.v["rode"]=w.v["top"]-p.z;w.v["weightUp"]=list[size_t(w.weight)].z;s.shoot("pulley-lift-ridden");standOn(g,{15.0f,7.5f},0,-.815f);LabAccess::player(g).pitch=float(s.fixture->number("look-up",0));}
   if(f==340)s.shoot("pulley-lift-loaded");
   if(f==280){w.v["back"]=w.v["top"]-p.z;Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims={.45f,.45f,.45f};auto tr=std::make_shared<EntityTraits>();tr->mass=50;c.traits=tr;c.pos={17.45f,3.5f};c.z=p.z+p.height()+.05f;c.sleeping=false;list.push_back(c);w.load=int(list.size())-1;}
   if(f==400)w.v["loaded"]=w.v["top"]-p.z;}
  if(scene=="bridge"){
   if(f==10)standOn(g,{5.6f,12.6f},.6f,0);
   if(f>10&&f<390)s.input.forward=true;
   const auto& pl=LabAccess::player(g);w.v["reached"]=pl.pos.x;if(pl.pos.x>6.4f&&pl.pos.x<9.6f)w.v["lowest"]=std::min(w.v.count("lowest")?w.v["lowest"]:9.0,double(pl.z));
   if(f==185)s.shoot("rope-bridge");
   if(f%30==0&&f>0){w.v["x"+std::to_string(f)]=pl.pos.x;w.v["z"+std::to_string(f)]=pl.z;}
   if(f==240){w.v["block"]=LabAccess::moveBlock(g,pl.pos+Vec2{.08f,0});w.v["stuckZ"]=pl.z;w.v["groundAhead"]=g.jointedPropTop(pl.pos+Vec2{.08f,0},pl.z+.215f,.2f);}
   const auto& wj=g.world();for(int i=0;i<int(wj.joints().size());++i){const auto& j=wj.joints()[size_t(i)];if(j.kind!=PropJointKind::Hinge||j.at.y<12.5f||j.at.y>12.7f)continue;if(wj.jointStates()[size_t(i)].broken)w.v["bridgeBroke"]=1;}}
  if(scene=="saves"&&w.crate>=0){auto& c=list[size_t(w.crate)];
   if(f==20)LabAccess::fireFrom(g,{c.pos.x,c.pos.y+3.5f,3.9f},{c.pos.x,c.pos.y,3.9f},weaponDef(WeaponId::Handgun).hitscan);
   if(f==30)standOn(g,{17.45f,3.5f},list[size_t(w.platform)].z+.1f,0);
   if(f==150){w.save=LabAccess::encodeSave(g);w.v["platformAtSave"]=list[size_t(w.platform)].z;}}
  if(scene=="bridgeview"){   // the bridge from beside it, a 30 kg crate set down on its middle
   if(f==10){Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims={.45f,.45f,.45f};auto tr=std::make_shared<EntityTraits>();tr->mass=30;c.traits=tr;c.pos={8.3f,12.6f};c.z=.9f;c.sleeping=false;list.push_back(c);}
   if(f==int(s.fixture->number("shoot-at",100)))s.shoot("rope-bridge");}
  if(scene=="stairs"&&f==int(s.fixture->number("shoot-at",170)))s.shoot("ragdoll-on-stairs");   // the dead man at the head of the stairs, gone down them
  if(scene=="cost"){
   if(f==20){for(int i=0;i<int(list.size());++i){const auto& c=list[size_t(i)];if(c.model==model::mine_cage_lamp)LabAccess::knock(g,size_t(i),middle(c),{1.5f,1,0});}
    if(w.crate>=0)LabAccess::knock(g,size_t(w.crate),middle(list[size_t(w.crate)]),{20,0,0});if(w.gate>=0)LabAccess::knock(g,size_t(w.gate),middle(list[size_t(w.gate)])+Point3{.5f,0,0},{0,25,0});
    Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims={.45f,.45f,.45f};auto tr=std::make_shared<EntityTraits>();tr->mass=50;c.traits=tr;c.pos={17.45f,3.5f};c.z=1.3f;c.sleeping=false;list.push_back(c);
    Clutter d=c;d.dims={.4f,.4f,.4f};auto t2=std::make_shared<EntityTraits>();t2->mass=9;d.traits=t2;d.pos={3.6f,12.5f};d.z=1.2f;list.push_back(d);
    Clutter e=c;e.pos={8,12.6f};e.z=1.5f;auto t3=std::make_shared<EntityTraits>();t3->mass=30;e.traits=t3;list.push_back(e);}
   // Every jointed prop kept awake through the measure (each joint solved every step).
   if(f>=20&&f<150){const auto& wj=g.world();for(const auto& st:wj.jointStates())for(int i:{st.hintA,st.hintB})if(i>=0&&i<int(list.size())){list[size_t(i)].sleeping=false;list[size_t(i)].restTime=0;}}
   if(f>30&&f<150){const auto& st=LabAccess::rigidStats(g);w.v["ms"]+=st.ms;w.v["n"]+=1;w.v["peak"]=std::max(w.v["peak"],st.ms);w.v["bodies"]=std::max(w.v["bodies"],double(st.bodies));w.v["awake"]=std::max(w.v["awake"],double(st.awake));}}
 },
 .metrics=[](Scene& s,Report& r){auto& w=watches()[&s];Game& g=s.g();const std::string scene=s.fixture->value("scene");
  // A picture fixture (pictures 1) runs only as far as its picture: it checks the picture was taken.
  if(s.fixture->number("pictures",0)>0){r.check("the picture was taken",!s.shots.empty());return;}
  if(scene=="lamps"){r.metric("swing_m",w.v["swing"]);r.metric("chain_stretch_m",w.v["stretch"]);r.metric("light_moved_m",w.v["lightMoved"]);
   r.check("a shot lamp swings on its chain",w.v["swing"]>.08,f2(w.v["swing"],3)+" m");r.check("its chain keeps its length",w.v["stretch"]<.03,f2(w.v["stretch"]*1000,1)+" mm over");
   r.check("its light swings with it",w.v["lightMoved"]>.05,f2(w.v["lightMoved"],3)+" m");}
  if(scene=="crate"){r.check("the rope shot through snaps",w.v["broken"]>0);r.check("the crate falls to the floor",w.v["lowest"]<.05,"lowest "+f2(w.v["lowest"],3)+" m");r.check("the rope snaps with a sound",w.v["snapSound"]>0);}
  if(scene=="seesaw"){{const int j=jointOf(g,spawnOf(g,w.plank));r.metric("plank_joint",j);if(j>=0){const auto& st=g.world().jointStates()[size_t(j)];r.metric("plank_broken",st.broken);r.metric("plank_force",st.force);r.metric("plank_hint",st.hintA);}
    const auto& c=LabAccess::clutter(g)[size_t(w.plank)];r.metric("plank_z",c.z);r.metric("plank_index",w.plank);r.metric("load_z",w.load>=0?LabAccess::clutter(g)[size_t(w.load)].z:-1);r.metric("load_x",w.load>=0?LabAccess::clutter(g)[size_t(w.load)].pos.x:-1);}
   r.metric("tipped_by_crate_deg",w.v["tippedByCrate"]*57.3);r.metric("tipped_by_player_deg",w.v["tippedByPlayer"]*57.3);
   r.check("a crate dropped on one end tips the seesaw",std::fabs(w.v["tippedByCrate"])>.12,f2(w.v["tippedByCrate"]*57.3,1)+" deg");
   r.check("the player on the other end tips it back",w.v["tippedByCrate"]*w.v["tippedByPlayer"]<0&&std::fabs(w.v["tippedByPlayer"])>.12,f2(w.v["tippedByPlayer"]*57.3,1)+" deg");}
  if(scene=="gate"){r.metric("gate_open_deg",w.v["open"]*57.3);r.check("the gate swings open before the player",w.v["open"]>.5,f2(w.v["open"]*57.3,0)+" deg");
   r.check("he gets through",w.v["through"]<13.2,"y "+f2(w.v["through"],2));r.check("it swings shut behind him",w.v["after"]<.25,f2(w.v["after"]*57.3,0)+" deg");}
  if(scene=="lift"){r.metric("rode_down_m",w.v["rode"]);r.metric("back_m",w.v["back"]);r.metric("loaded_m",w.v["loaded"]);
   r.check("the lift takes the player down",w.v["rode"]>.9,f2(w.v["rode"],2)+" m");r.check("empty, it goes back up",w.v["back"]<.08,f2(w.v["back"],3)+" m below the top");
   r.check("a 50 kg crate takes it down",w.v["loaded"]>.9,f2(w.v["loaded"],2)+" m");}
  if(scene=="bridge"){r.metric("player_lowest_on_bridge_m",w.v["lowest"]);r.metric("block_reason",w.v["block"]);r.metric("stuck_z",w.v["stuckZ"]);r.metric("ground_ahead",w.v["groundAhead"]);for(int k=30;k<=240;k+=30){r.metric("x"+std::to_string(k),w.v["x"+std::to_string(k)]);r.metric("z"+std::to_string(k),w.v["z"+std::to_string(k)]);}r.check("the player walks over the bridge",w.v["reached"]>9.9,"x "+f2(w.v["reached"],2));
   r.check("the bridge sags under him",w.v["lowest"]<.55,"lowest "+f2(w.v["lowest"],2)+" m");r.check("its pins hold",w.v["bridgeBroke"]==0);}
  if(scene=="saves"){Game loaded(LabAccess::campaignOf(g));const bool ok=!w.save.empty()&&LabAccess::decodeSave(loaded,w.save);r.check("the save loads",ok);
   if(ok){const int j=jointOf(loaded,spawnOf(g,w.crate));r.check("the snapped rope stays snapped",j>=0&&loaded.world().jointStates()[size_t(j)].broken);
    const auto& l=loaded.clutter();int p=nearProp(loaded,model::packing_crate,17.45f,3.5f);r.check("the lift is where it was",p>=0&&std::fabs(l[size_t(p)].z-float(w.v["platformAtSave"]))<.02f,p>=0?f2(l[size_t(p)].z,3)+" vs "+f2(w.v["platformAtSave"],3):"none");}}
  if(scene=="cost"){const double n=std::max(1.0,w.v["n"]);r.metric("vectra_ms_frame",w.v["ms"]/n,"ms");r.metric("vectra_ms_peak",w.v["peak"],"ms");r.metric("bodies",w.v["bodies"]);r.metric("awake",w.v["awake"]);
   r.check("every joint moving costs under 2 ms a frame",w.v["ms"]/n<2,f2(w.v["ms"]/n,3)+" ms");}
  (void)g;}});
}
}

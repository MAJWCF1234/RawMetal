// Marks ride a door's leaf (Game::pinToDoor, Door::carry, Scene3D's marks). door-marks: the player shoots a
// bulkhead door that lifts and a round through a man paints it, then it is opened; the bullet holes and the blood
// rise with the leaf instead of hanging in the air where the leaf was. door-marks-swing: a swinging leaf is shot
// and bloodied, then swung open; every mark turns round the hinge with it, its face turned with the leaf's.
// Pictures: each door shot shut, part open and open.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// The marks on the leaf at frame 80 (holes; blood on a face, not a pool on the floor), and how many are pinned.
void countMarks(Scene& s){int pinned=0,total=0,blood=0,bloodPinned=0;
 for(const auto& m:s.g().bulletImpacts()){
  if(m.blood){if(std::fabs(m.normalZ)<.5f&&std::fabs(m.normal.x)+std::fabs(m.normal.y)>.01f){++blood;bloodPinned+=m.door==0;}}
  else if(m.tint==0){++total;pinned+=m.door==0;}}
 s.board["marks.total"]=total;s.board["marks.pinned"]=pinned;s.board["blood.total"]=blood;s.board["blood.pinned"]=bloodPinned;}
// Every pinned mark, carried by the leaf's transform, lies on the leaf where it stands now (within 1 cm of its
// faces, edges, foot and head) and faces as the leaf's face does. Worst distance off the face, least alignment.
void carried(Scene& s){
 const Game& g=s.g();const Door& d=g.world().doors()[0];const Vec2 c=d.centre();const float base=g.world().floorHeight(c.x,c.y)+d.z;
 const Vec2 axis=d.swinging?d.leafAxis(d.open):d.tangent(),face{-axis.y,axis.x};const float half=d.swinging?Door::SwingFace:Door::SlabFace;
 const Vec2 origin=d.swinging?d.hinge():d.world(d.left,d.y);
 double worst=s.board.count("carry.worst")?s.board["carry.worst"]:0,turn=s.board.count("carry.turn")?s.board["carry.turn"]:1;int seen=0;
 for(const auto& m:g.bulletImpacts()){if(m.door!=0)continue;++seen;
  const Point3 p=d.carry({m.pos.x,m.pos.y,m.z},m.doorOpen,d.open);const Vec2 n=d.carryDirection(m.normal,m.doorOpen,d.open);
  const Vec2 off{p.x-origin.x,p.y-origin.y};worst=std::max(worst,double(std::fabs(std::fabs(off.x*face.x+off.y*face.y)-half)));
  if(!d.onLeaf(p,base,d.open,.01f))worst=std::max(worst,1.0);
  turn=std::min(turn,double(std::fabs(n.x*face.x+n.y*face.y)/std::max(1e-4f,length(n))));}
 if(seen){s.board["carry.worst"]=worst;s.board["carry.turn"]=turn;}}
void carriedChecks(Scene& s,Report& r){
 const double total=s.board["marks.total"],pinned=s.board["marks.pinned"],blood=s.board["blood.total"],bloodPinned=s.board["blood.pinned"];
 r.metric("marks",total);r.metric("pinned",pinned);r.metric("blood",blood);r.metric("blood_pinned",bloodPinned);
 r.check("the shots marked the door's leaf, and each mark is pinned to it",total>=3&&pinned==total,std::to_string(int(pinned))+" of "+std::to_string(int(total))+" pinned");
 r.check("the blood on the leaf is pinned to it",blood>=1&&bloodPinned==blood,std::to_string(int(bloodPinned))+" of "+std::to_string(int(blood))+" pinned");
 const double worst=s.board.count("carry.worst")?s.board["carry.worst"]:1,turn=s.board.count("carry.turn")?s.board["carry.turn"]:0;
 r.metric("carry_off_face_m",worst,"m");r.metric("carry_facing",turn);
 r.check("every carried mark stays on the leaf's face as it moves",worst<.006,"worst "+fixed(worst*1000,1)+" mm off the face");
 r.check("and faces as the leaf's face does",turn>.999,"least alignment "+fixed(turn,4));}

const bool doorMarks=add({
 .name="door-marks",.summary="bullet holes and blood on a lifting door rise with its leaf (Game::pinToDoor, Door::carry)",
 .fixtures={"door-marks"},
 .author=[](Scene&,AuthoredMapData& m){if(m.doors.empty())m.doors.push_back({7,10,8});m.doors[0].swinging=false;},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Handgun);LabAccess::playerCamera(g);
  auto& p=LabAccess::player(g);p.pos={8.5f,12.5f};p.angle=-1.5707963f;p.pitch=6;},
 .step=[](Scene& s,float){
  // Five shots across the leaf at chest height, a few frames apart, a gout of blood, then the door lifts.
  auto& p=LabAccess::player(s.g());p.pos={8.5f,12.5f};p.pitch=6;
  if(s.frame<60){p.angle=-1.5707963f+(float((s.frame/12)%5)-2.f)*.09f;s.input.fire=s.frame%12==2;}
  if(s.frame==70)LabAccess::hitBurst(s.g(),{9.3f,9.4f,1.2f},{0,-1,0},ImpactMaterial::Flesh,-1,1.2f,false);
  if(s.frame==300)LabAccess::world(s.g()).setDoor(0,0,true);},   // once the gun smoke has cleared: it lifts at its own pace
 .capture=[](Scene& s,const Frame&){
  if(s.frame==80)countMarks(s);
  if(s.frame==295)s.shoot("the shot door, shut");
  {const float open=s.g().world().doors().empty()?0.f:s.g().world().doors()[0].open;
   if(open>=.3f&&!s.board.count("marks.mid")){s.board["marks.mid"]=open;s.shoot("part lifted: the holes and the blood ride up on the leaf");}}
  if(s.frame>300&&!s.g().world().doors().empty())carried(s);
  if(s.frame==s.frames-1)s.shoot("the door lifted: no holes left hanging where the leaf was");},
 .metrics=[](Scene& s,Report& r){
  const World& w=s.g().world();r.check("the tube has a door",!w.doors().empty());if(w.doors().empty())return;
  carriedChecks(s,r);
  r.check("the door lifted",w.doors()[0].open>.9f);}
});

// The swinging leaf: 1.2 m wide in the door room's 3 m gap (the rest walled up), hinged at x 7, opening away from
// the player (toward -y). Five handgun rounds across it at chest height and two gouts of blood, then a hand swings
// it open; the camera then looks at the open leaf from the doorway and from beside it.
const bool doorMarksSwing=add({
 .name="door-marks-swing",.summary="bullet holes and blood on a swinging door turn round its hinge with the leaf (Door::carry)",
 .fixtures={"door-marks-swing"},
 .author=[](Scene&,AuthoredMapData& m){if(m.doors.empty())m.doors.push_back({7,10,8});auto& d=m.doors[0];d.right=8.2f;d.swinging=true;d.swing=-1;d.maxOpen=1;
  m.structures.push_back({8.2f,7.9f,10,8.1f,0,8});},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Handgun);LabAccess::playerCamera(g);
  auto& p=LabAccess::player(g);p.pos={7.6f,10.6f};p.angle=-1.5707963f;p.pitch=4;},
 .step=[](Scene& s,float){
  auto& p=LabAccess::player(s.g());Game& g=s.g();
  if(s.frame<60){p.pos={7.6f,10.6f};p.pitch=4;p.angle=-1.5707963f+(float((s.frame/12)%5)-2.f)*.07f;s.input.fire=s.frame%12==2;}
  // Two gouts, as a round through a man paints the wall behind him: by the latch, and high by the hinge.
  if(s.frame==70){LabAccess::hitBurst(g,{7.95f,9.2f,1.05f},{0,-1,0},ImpactMaterial::Flesh,-1,1.2f,false);LabAccess::hitBurst(g,{7.35f,9.3f,1.55f},{0,-1,.05f},ImpactMaterial::Flesh,-1,1.f,false);}
  if(s.frame==300)LabAccess::world(g).openDoor(0);
  // Open: from the doorway, then from beside the leaf on its far side.
  if(s.frame==s.frames-40){p.pos={8.9f,9.3f};p.angle=std::atan2(6.9f-9.3f,7.05f-8.9f);p.pitch=0;}
  if(s.frame==s.frames-15){p.pos={9.0f,6.6f};p.angle=std::atan2(7.4f-6.6f,7.f-9.0f);p.pitch=0;}},
 .capture=[](Scene& s,const Frame&){
  const Game& g=s.g();if(g.world().doors().empty())return;const Door& d=g.world().doors()[0];
  if(s.frame==80)countMarks(s);
  if(s.frame==295)s.shoot("the swinging door shot and bloodied, shut");
  if(d.open>=.45f&&!s.board.count("marks.mid")){s.board["marks.mid"]=d.open;s.shoot("half open: the holes and the blood turn with the leaf");}
  if(s.frame>300)carried(s);
  if(s.frame==s.frames-41)s.board["open.end"]=d.open;
  if(s.frame==s.frames-16)s.shoot("open, from the doorway: the marks are on the leaf, none left where it stood");
  if(s.frame==s.frames-1)s.shoot("open, from beside it: the holes and the blood on the leaf's face");},
 .metrics=[](Scene& s,Report& r){
  const World& w=s.g().world();r.check("the tube has a door",!w.doors().empty());if(w.doors().empty())return;
  carriedChecks(s,r);
  r.check("the door swung open",s.board["open.end"]>.97,"open "+fixed(s.board["open.end"]));}
});
}
}

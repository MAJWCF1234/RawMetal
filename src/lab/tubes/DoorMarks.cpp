// Marks ride a lifting door (Game::pinToDoor, Scene3D's marks): the player shoots a bulkhead door that lifts,
// then opens it; the bullet holes (and blood, pinned the same way) rise with the leaf instead of hanging in the
// air where the leaf was. Pictures: the shot door shut, and lifted.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
namespace retro::lab {
namespace {
const bool doorMarks=add({
 .name="door-marks",.summary="bullet holes and blood on a lifting door rise with its leaf (Game::pinToDoor)",
 .fixtures={"door-marks"},
 .author=[](Scene&,AuthoredMapData& m){if(m.doors.empty())m.doors.push_back({7,10,8});m.doors[0].swinging=false;},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Handgun);LabAccess::playerCamera(g);
  auto& p=LabAccess::player(g);p.pos={8.5f,12.5f};p.angle=-1.5707963f;p.pitch=6;},
 .step=[](Scene& s,float){
  // Five shots across the leaf at chest height, a few frames apart, then the door is opened.
  auto& p=LabAccess::player(s.g());p.pos={8.5f,12.5f};p.pitch=6;
  if(s.frame<60){p.angle=-1.5707963f+(float((s.frame/12)%5)-2.f)*.09f;s.input.fire=s.frame%12==2;}
  if(s.frame==300)LabAccess::world(s.g()).setDoor(0,1,true);},   // once the gun smoke has cleared
 .capture=[](Scene& s,const Frame&){
  if(s.frame==295)s.shoot("the shot door, shut");
  {const float open=s.g().world().doors().empty()?0.f:s.g().world().doors()[0].open;
   if(open>=.2f&&!s.board.count("marks.mid")){s.board["marks.mid"]=open;s.shoot("half lifted: the holes ride up on the leaf");}}
  if(s.frame==s.frames-1)s.shoot("the door lifted: no holes left hanging where the leaf was");
  if(s.frame==80){int pinned=0,total=0;for(const auto& m:s.g().bulletImpacts())if(!m.blood&&m.tint==0){++total;pinned+=m.door==0;}s.board["marks.total"]=total;s.board["marks.pinned"]=pinned;}},
 .metrics=[](Scene& s,Report& r){
  const World& w=s.g().world();r.check("the tube has a door",!w.doors().empty());if(w.doors().empty())return;
  const double total=s.board["marks.total"],pinned=s.board["marks.pinned"];r.metric("marks",total);r.metric("pinned",pinned);
  r.check("the shots marked the door's leaf, and each mark is pinned to it",total>=3&&pinned==total,std::to_string(int(pinned))+" of "+std::to_string(int(total))+" pinned");
  r.check("the door lifted",w.doors()[0].open>.9f);}
});
}
}

// The heavy door, mesh door and ticket gate kits in the Starter Map's Kits Gallery: each door follows its state, so
// shut at first, opened when the state is set (a terminal's toggle), the swinging ticket gate easing off its latch.
// One picture per fixture (a camera at each).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
#include <cstring>
namespace retro::lab {
namespace {
const StateId Heavy=stateId("heavy_door"),Mesh=stateId("mesh_door"),Ticket=stateId("ticket_paid");
float openOf(const World& w,const char* label){for(const auto& d:w.doors())if(d.label==label)return d.open;return -1.f;}
const bool kitDoors=add({
 .name="kit-doors",.summary="the heavy door, mesh door and ticket gate kits open on their state (Door::follow)",
 .fixtures={"kitdoors-*"},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);},
 .step=[](Scene& s,float){if(s.frame==60){s.g().setState(Heavy,1);s.g().setState(Mesh,1);s.g().setState(Ticket,1);}},
 .capture=[](Scene& s,const Frame&){const World& w=s.g().world();
  if(s.frame==50){s.board["h0"]=openOf(w,"HEAVY DOOR");s.board["m0"]=openOf(w,"GATE");s.board["t0"]=openOf(w,"TICKET GATE");}
  if(s.frame==s.frames-1){s.board["h1"]=openOf(w,"HEAVY DOOR");s.board["m1"]=openOf(w,"GATE");s.board["t1"]=openOf(w,"TICKET GATE");s.shoot("the door, open");}},
 .metrics=[](Scene& s,Report& r){
  r.check("the kits' doors are all there",s.board["h0"]>=0&&s.board["m0"]>=0&&s.board["t0"]>=0);
  r.check("shut at first",s.board["h0"]<.02&&s.board["m0"]<.02&&s.board["t0"]<.02);
  r.check("each opens on its state",s.board["h1"]>.9&&s.board["m1"]>.9&&s.board["t1"]>.5,
   "heavy "+std::to_string(s.board["h1"])+", mesh "+std::to_string(s.board["m1"])+", ticket "+std::to_string(s.board["t1"]));}
});
}
}

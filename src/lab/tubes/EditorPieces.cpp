// Editor pieces a walker goes into or through: the shower stall, the blast-door frame and the mine entrance.
// Each is set down as the Builder sets it, walked into on foot, and photographed with and without the debug
// layer's collider wireframes.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "renderer/DebugLayer.h"
#include "world/ModelIds.h"
#include <cmath>
namespace retro::lab {
namespace {
constexpr float Pi=3.14159265f;
constexpr int Leg=240;   // frames a leg
struct Walk {const char* name;Vec2 from;float yaw;int stop;Vec2 photo;float photoYaw,photoPitch;};
// The stall (turned as the Shower stall kit turns it), then the frame, then the mine entrance (its low end north).
const Walk walks[]={
 {"shower stall",{5.f,10.4f},-Pi*.5f,150,{6.2f,10.9f},-2.0f,-18.f},
 {"blast door frame",{11.5f,11.f},-Pi*.5f,150,{11.5f,11.2f},-Pi*.5f,-8.f},
 {"mine entrance",{18.f,2.6f},Pi*.5f,230,{18.f,1.4f},Pi*.5f,0.f}};
const bool pieces=add({
 .name="editor-pieces",.summary="the shower stall, blast-door frame and mine entrance can be walked into and through",
 .fixtures={"editor-pieces"},
 .author=[](Scene&,AuthoredMapData& m){
  m.fixtures.push_back({model::bath_stall_open,{5,8},0,2.13f,1.542f,2.4f,-Pi*.5f,true});
  m.fixtures.push_back({model::blast_door_frame,{11.5f,8},0,2.997f,.56f,2.47f,0,true});
  m.fixtures.push_back({model::mine_entrance,{18,9},0,3.86f,8.276f,4.82f,0,true});},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);debuglayer::visible=0;},
 .step=[](Scene& s,float){
  const int i=s.frame/Leg,t=s.frame%Leg;if(i>=3)return;const Walk& w=walks[i];auto& p=LabAccess::player(s.g());
  debuglayer::visible=0;
  if(t<60){   // two stills from outside: as drawn, then its collision
   p.pos=w.photo;p.z=0;p.angle=w.photoYaw;p.pitch=w.photoPitch;if(t>=30)debuglayer::visible=debuglayer::Colliders;return;}
  if(t==60){p.pos=w.from;p.z=0;p.angle=w.yaw;p.pitch=0;}
  s.input.forward=t>=70&&t<w.stop;},
 .capture=[](Scene& s,const Frame&){
  const int i=s.frame/Leg,t=s.frame%Leg;if(i>=3)return;const auto& p=s.g().player();const std::string n=walks[i].name;
  if(t==28)s.shoot(n);
  if(t==58)s.shoot(n+": collision");
  if(t==walks[i].stop+5){s.shoot(n+": walked in");s.board[n+".x"]=p.pos.x;s.board[n+".y"]=p.pos.y;s.board[n+".z"]=p.z;}},
 .metrics=[](Scene& s,Report& r){
  r.check("the walker gets inside the shower stall",std::fabs(s.board["shower stall.y"]-8)<.9&&std::fabs(s.board["shower stall.x"]-5)<1.,"y "+std::to_string(s.board["shower stall.y"]));
  r.check("the walker goes through the blast-door frame",s.board["blast door frame.y"]<7.4,"y "+std::to_string(s.board["blast door frame.y"]));
  r.check("the walker climbs into the mine entrance",s.board["mine entrance.y"]>10&&s.board["mine entrance.z"]>.8,
   "y "+std::to_string(s.board["mine entrance.y"])+", z "+std::to_string(s.board["mine entrance.z"]));},
 .teardown=[](Scene&){debuglayer::visible=0;}
});
// One kit in the Starter Map's Kits Gallery per fixture: as drawn, then with its collision.
const bool piecePhotos=add({
 .name="editor-kit-photos",.summary="the shower, rebar, cracked slab and fluorescent bank kits as the Starter Map places them",
 .fixtures={"editorkit-*"},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);debuglayer::visible=0;},
 .step=[](Scene& s,float){debuglayer::visible=s.frame>=60?debuglayer::Colliders:0u;},
 .capture=[](Scene& s,const Frame&){if(s.frame==55)s.shoot("as drawn");if(s.last())s.shoot("collision");},
 .teardown=[](Scene&){debuglayer::visible=0;}
});
}
}

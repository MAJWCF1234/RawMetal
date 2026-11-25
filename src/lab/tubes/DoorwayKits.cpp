// The Doorway and Wide doorway kits in the Starter Map's Kits Gallery (the parts opened by
// tools/library/open_doorways.py with a working swinging DOOR): the player walks into the shut door and is
// stopped, presses E, and walks through the opening. Pictures: shut, opening, its collision, through.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "renderer/DebugLayer.h"
#include <cmath>
namespace retro::lab {
namespace {
const Door* kitDoor(const World& w,Vec2 at){for(const auto& d:w.doors())if(d.swinging&&std::hypot(d.centre().x-at.x,d.centre().y-at.y)<.6f)return &d;return nullptr;}
Vec2 doorAt(const Scene& s){return {s.fixture->number("door-x",14.5f),s.fixture->number("door-y",14.5f)};}
const bool doorwayKits=add({
 .name="doorway-kits",.summary="the Doorway kits' doors stop a walker shut, open on E and let them through",
 .fixtures={"doorway-single","doorway-wide"},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);debuglayer::visible=0;
  auto& p=LabAccess::player(s.g());const Vec2 d=doorAt(s);p.pos={d.x,d.y-1.9f};p.angle=1.5707963f;p.pitch=0;},
 .step=[](Scene& s,float){const int f=s.frame;   // up to the shut door, back for a look, up again, E, back, through
  s.input.forward=(f>=10&&f<70)||(f>=115&&f<150)||(f>=290&&f<410);s.input.back=(f>=80&&f<105)||(f>=170&&f<195);s.input.use=f==155;
  debuglayer::visible=f>=250&&f<275?debuglayer::Colliders:0u;},
 .capture=[](Scene& s,const Frame&){const auto& p=s.g().player();const Vec2 at=doorAt(s);const Door* d=kitDoor(s.g().world(),at);
  if(s.frame==69){s.board["blocked"]=p.pos.y-at.y;s.board["shut"]=d?d->open:-1;}
  if(s.frame==110)s.shoot("walked into the shut door");
  if(s.frame==235){s.board["open"]=d?d->open:-1;s.shoot("the door, open");}
  if(s.frame==268)s.shoot("open: the collision");
  if(s.frame==s.frames-1)s.board["end"]=p.pos.y-at.y;},
 .metrics=[](Scene& s,Report& r){
  r.check("the shut door stops the walker",s.board["shut"]>=0&&s.board["shut"]<.02&&s.board["blocked"]<0,std::to_string(s.board["blocked"]));
  r.check("E opens it",s.board["open"]>.8,std::to_string(s.board["open"]));
  r.check("the walker goes through the opening",s.board["end"]>.8,std::to_string(s.board["end"]));},
 .teardown=[](Scene&){debuglayer::visible=0;}
});
}
}

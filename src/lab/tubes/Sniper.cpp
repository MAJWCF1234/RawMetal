// The sniper rifle: carried at the hip, then brought to the eye (the view narrows to a quarter and a ring closes in with
// a fine cross), then a shot down the room. Pictures: hip, scoped, after the shot.
// Checks: the scope blend reaches 1 while aimed; the lens narrows with it; it falls back at the hip; the shot leaves a mark.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <algorithm>
#include <cstdio>
namespace retro::lab {
namespace {
const bool sniper=add({
 .name="sniper",.summary="the sniper rifle: hip, scope at the eye, one bolt-action shot",
 .fixtures={"sniper"},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);g.grantSniper(5);g.grantSniper(15);LabAccess::equip(g,WeaponId::Sniper);LabAccess::playerCamera(g);
  auto& p=LabAccess::player(g);p.pos={8.f,22.f};p.angle=-1.5707963f;p.pitch=0;},
 .step=[](Scene& s,float){auto& p=LabAccess::player(s.g());p.pos={8.f,22.f};p.angle=-1.5707963f;p.pitch=0;
  s.input.guard=s.frame>=90&&s.frame<300;s.input.fire=s.frame==170;s.input.reload=s.frame==320;},
 .capture=[](Scene& s,const Frame&){const float scope=s.g().scopeBlend();
  if(s.frame==60)s.shoot("the sniper rifle at the hip");
  if(s.frame==93){s.board["irons"]=scope;s.shoot("the rifle coming up");}
  if(s.frame==106)s.shoot("at the shoulder: down the tube");
  if(s.frame==150){s.board["scope"]=scope;s.shoot("through the scope: the view narrows, the ring closes in");}
  if(s.frame==185){s.board["marks"]=double(s.g().bulletImpacts().size());}
  if(s.frame==240)s.shoot("the bolt being worked");
  if(s.frame==315){s.board["scope_end"]=scope;}
  // Every second frame of the bolt and of the reload (30 fps) for the film: labelled v and the frame number.
  if(((s.frame>=172&&s.frame<=285)||(s.frame>=318&&s.frame<=545))&&s.frame%2==0){char l[16];std::snprintf(l,sizeof l,"v%04d",s.frame);s.shoot(l);}},
 .metrics=[](Scene& s,Report& r){
  r.check("the scope is fully up while aimed",s.board["scope"]>.95,std::to_string(s.board["scope"]));
  r.check("the scope view does not close before the rifle is at the shoulder",s.board["irons"]<.1);
  r.check("the shot leaves a mark",s.board["marks"]>=1);
  r.check("the scope comes down again",s.board["scope_end"]<.1);}
});
}
}

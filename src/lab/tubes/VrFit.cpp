// VR body fit (Avatar.h AvatarFit, VrRuntime avatar()): players of different real heights and reaches drive the same
// Claire, who never changes size (as in VRChat). lab/fixtures/vr-fit-*.lab, `height` (cm) and `arms` (the player's
// shoulder-to-wrist reach over their height). Each player puts the headset on at a desk (the old trap: the height
// read there made a tall player a giant whose arms tore off the hands), stands, then reaches ahead at full stretch,
// holds the arms out wide ahead and reaches one hand overhead. Pictures through her eyes (what the player sees) and
// of her arms from outside.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "vr/VrRuntime.h"
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
namespace retro::lab {
namespace {
struct Rig {std::unique_ptr<VrRuntime> runtime;VrRuntime::VrScript pose;std::map<std::string,double> v;};
std::map<const Scene*,Rig>& rigs(){static std::map<const Scene*,Rig> r;return r;}
Point3 unit(Point3 v){const float l=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return l>1e-6f?v*(1/l):Point3{};}
float len(Point3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
std::string f2(double v){char b[32];std::snprintf(b,sizeof b,"%.3f",v);return b;}
constexpr float DeskHead=.80f;
// The player's own body, tracking space (x right, y up, -z ahead), from their height (m) and reach ratio.
struct Body {float eye,reach;Point3 shoulder(int side)const{const float h=eye/.933f;return {(side?1.f:-1.f)*.115f*h,eye-.115f*h,.03f*h};}};
Body body(const Scene& s){const float h=std::stof(s.fixture->value("height"))/100.f,arms=std::stof(s.fixture->value("arms"));return {h*.933f,h*arms};}
}
static const bool registered=add({
 .name="vr-fit",.summary="VR body fit: short, average and tall players (one long-armed) drive the same Claire; headset put on at a desk, then full-stretch reaches, arms out, one overhead; her size never changes and her hands stay on the arms",
 .fixtures={"vr-fit-short","vr-fit-average","vr-fit-tall"},
 .setup=[](Scene& s){rigs().erase(&s);auto& rig=rigs()[&s];rig.runtime=std::make_unique<VrRuntime>();Game& g=s.g();LabAccess::setGod(g,true);LabAccess::playerCamera(g);
  LabAccess::equip(g,WeaponId::Fists);auto& p=LabAccess::player(g);p.angle=0;p.pitch=0;rig.v["worstGap"]=0;rig.v["worstFitGap"]=0;},
 .step=[](Scene& s,float dt){auto& rig=rigs()[&s];Game& g=s.g();auto& r=*rig.runtime;auto& pose=rig.pose;const int f=s.frame;const Body b=body(s);
  pose.velocity={};pose.grip={false,false};pose.trigger={false,false};pose.headForward=f>=240?Point3{0,.9f,-1}:f>=180?Point3{0,-.15f,-1}:Point3{0,-.35f,-1};
  const Point3 head{0,f<30?DeskHead:b.eye,0};pose.head=head;
  // The hands: where the fist closes, each at its distance along a direction from the player's own shoulder.
  auto put=[&](int side,Point3 dir,float stretch,Point3 fingers,Point3 channel){const Point3 at=f<30?head+Point3{(side?.2f:-.2f),-.3f,-.2f}:b.shoulder(side)+unit(dir)*(b.reach*stretch);
   pose.joints[size_t(side)]=VrRuntime::scriptHand(at,fingers,channel,side==0);};
  if(f<120){for(int side=0;side<2;++side)put(side,{side?.15f:-.15f,-1,-.1f},.85f,{0,-1,0},{0,0,-1});}
  else if(f<180){for(int side=0;side<2;++side)put(side,{side?.12f:-.12f,-.1f,-1},1.f,{0,0,-1},{0,1,0});}
  else if(f<240){for(int side=0;side<2;++side)put(side,{side?.75f:-.75f,-.05f,-.65f},1.f,{side?.75f:-.75f,0,-.65f},{0,1,0});}
  else{put(1,{.15f,1,-.45f},1.f,{0,1,-.45f},{0,0,-1});put(0,{-.1f,-.15f,-1},1.f,{0,0,-1},{0,1,0});}
  r.script(g,s.input,pose,dt);
  const auto fit=r.bodyFit();
  if(f==110){rig.v["eye"]=fit.eye;rig.v["scale"]=fit.scale;rig.v["standingEye"]=s.input.vrEye;}
  // Attached: once each pose has settled (its last third of a second), every tracked hand's drawn wrist (the solve) on its fitted target, and the fitted
  // target on the drawn hand within reach (her arm reaches it).
  if((f>=160&&f<180)||(f>=220&&f<240)||(f>=280&&f<300)){const AvatarPose fitted=r.avatar(g),tracked=r.trackedAvatar(g);const AvatarSolve solved=solveAvatar(fitted,0);
   for(int side=0;side<2;++side){rig.v["worstGap"]=std::max(rig.v["worstGap"],double(len(solved.wrist[size_t(side)]-fitted.hands[size_t(side)].wrist)));
    rig.v["worstFitGap"]=std::max(rig.v["worstFitGap"],double(len(fitted.hands[size_t(side)].wrist-tracked.hands[size_t(side)].wrist)));
    rig.v["arm"]=std::max(rig.v["arm"],double(len(solved.wrist[size_t(side)]-solved.shoulder[size_t(side)])));}}
  if(f==s.frames-1){rig.v["armFit"]=fit.armFit;rig.v["reach"]=fit.reach;}},
 .capture=[](Scene& s,const Frame&){const int f=s.frame;if(!s.renderer)return;
  const std::string who=s.fixture->value("height")+"cm";
  auto front=[&](const std::string& label){Player look=s.g().view();const Vec2 me=look.pos;look.pos=me+Vec2{2.4f,0};look.eye=1.05f;look.angle=std::atan2(me.y-look.pos.y,me.x-look.pos.x);look.pitch=-4.f;
   s.renderer->renderVrScripted(s.g(),&look);s.shoot("vr-fit-"+who+"-"+label);};
  auto side=[&](const std::string& label){Player look=s.g().view();const Vec2 me=look.pos;look.pos=me+Vec2{.5f,-2.2f};look.eye=1.05f;look.angle=std::atan2(me.y-look.pos.y,me.x-look.pos.x);look.pitch=-4.f;
   s.renderer->renderVrScripted(s.g(),&look);s.shoot("vr-fit-"+who+"-"+label+"-side");};
  // What the player sees in the headset (the same for every height: that is the point), and her arms from the side.
  auto eye=[&](const std::string& label){s.renderer->renderVrScripted(s.g());s.shoot("vr-fit-"+who+"-"+label+"-eye");};
  if(f==20)eye("desk");
  if(f==175){eye("reach");side("reach");}
  if(f==235){eye("arms-out");front("arms-out");}
  if(f==s.frames-1){eye("overhead");side("overhead");}},
 .metrics=[](Scene& s,Report& r){auto& v=rigs()[&s].v;const Body b=body(s);
  // What the old fit made of the same player: the eye read once at the desk, the reach left as tracked.
  const float oldScale=std::clamp(Player::StandingEye/std::clamp(DeskHead,1.f,2.1f),.7f,1.25f);
  r.metric("old_eye_m",Player::StandingEye+(b.eye-DeskHead)*oldScale,"m");r.metric("old_hand_off_arm_m",std::max(0.f,b.reach*oldScale-AvatarArmReach),"m");
  r.metric("eye_learned_m",v["eye"],"m");r.metric("tracking_scale",v["scale"]);r.metric("standing_eye_m",v["standingEye"],"m");
  r.metric("arm_fit",v["armFit"]);r.metric("reach_learned_m",v["reach"],"m");r.metric("worst_hand_off_arm_m",v["worstGap"],"m");r.metric("longest_arm_m",v["arm"],"m");r.metric("worst_fit_shift_m",v["worstFitGap"],"m");
  r.check("the desk start is outgrown: the eye learned is the player's own",std::fabs(v["eye"]-b.eye)<.01,f2(v["eye"]));
  r.check("she stands at her own eye height whoever plays (her size never changes)",std::fabs(v["standingEye"]-Player::StandingEye)<.02,f2(v["standingEye"]));
  r.check("her hands never leave her arms (drawn wrist on its target)",v["worstGap"]<.005,f2(v["worstGap"]));
  r.check("her arm never stretches past its own length",v["arm"]<=AvatarArmReach+1e-3,f2(v["arm"]));
  r.check("the fit moves a hand no more than it must (under 14 cm)",v["worstFitGap"]<.14,f2(v["worstFitGap"]));},
 .teardown=[](Scene& s){rigs().erase(&s);}});
}

// VR guns in scripted hands (no headset): lab/fixtures/vr-guns-*.lab, `scene`. Each runs VrRuntime::script with
// hand poses built in tracking space, as SteamVR would hand them over, through Game::update, and takes pictures
// of the VR drawing (SoftwareRenderer::renderVrScripted: her body, arms, the gun in the hand, a magazine or shell
// in the other) through the scripted head.
//  shotgun:  the shotgun up at the eye with the support hand on the fore-end (held as a Touch controller is held
//            under a fore-end: upright, knuckles along the wood); fired, then pumped back and forward.
//  magazine: the handgun's magazine dropped with the button, a fresh one taken from the belt pouch and pushed part
//            way up the well (picture), then home.
//  shell:    the shotgun canted to show its belly, a shell taken from the pouch and part way into the loading port
//            (picture), then in.
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
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 spin(Point3 v,Point3 k,float t){k=unit(k);return v*std::cos(t)+cross(k,v)*std::sin(t)+k*(dot(k,v)*(1-std::cos(t)));}
const Point3 Head{0,Player::StandingEye,0};
// The gun hand's joints so the gun (gunFrame's raise undone) points `muzzle` with its top `top`, the fist at `hold`.
std::array<Point3,4> gunHand(Point3 hold,Point3 muzzle,Point3 top,bool leftHand){
 const Point3 back=unit(muzzle*-1.f);Point3 up=unit(top-back*dot(top,back));const float c=std::cos(VrRuntime::GunPitch),s=std::sin(VrRuntime::GunPitch);
 const Point3 b0=back*c+up*s,u0=up*c-back*s;return VrRuntime::scriptHand(hold,b0*-1.f,u0,leftHand);}
// The gun the gun hand holds (gunHand's frame, tracking space): a point given in it (right, up, back from the
// grip), and its axes. Worked from the scripted pose itself, not read back from the drawing, so a support hand
// placed by it does not chase a gun it has itself turned.
struct GunPose{Point3 hold,right,up,back;Point3 at(Point3 p)const{return hold+right*p.x+up*p.y+back*p.z;}};
GunPose gunPose(Point3 hold,Point3 muzzle,Point3 top){const Point3 back=unit(muzzle*-1.f);const Point3 up=unit(top-back*dot(top,back));return {hold,unit(cross(up,back)),up,back};}
std::string f2(double v){char b[32];std::snprintf(b,sizeof b,"%.2f",v);return b;}
}
static const bool registered=add({
 .name="vr-guns",.summary="VR guns in scripted hands: the shotgun two-handed (fore-end snap, fire, pump), a pistol magazine from the belt pouch part way up the well, a shell part way into the loading port; pictures of the VR drawing through the scripted head",
 .fixtures={"vr-guns-shotgun","vr-guns-magazine","vr-guns-shell"},
 .setup=[](Scene& s){rigs().erase(&s);auto& rig=rigs()[&s];rig.runtime=std::make_unique<VrRuntime>();Game& g=s.g();LabAccess::setGod(g,true);LabAccess::playerCamera(g);
  auto& p=LabAccess::player(g);p.angle=0;p.pitch=0;const std::string scene=s.fixture->value("scene");
  const WeaponId gun=scene=="magazine"?WeaponId::Handgun:WeaponId::Shotgun;LabAccess::equip(g,gun);
  p.pool(gun)=gun==WeaponId::Handgun?AmmoPool{36,9}:AmmoPool{24,scene=="shell"?2:6};
  rig.pose.head=Head;rig.pose.headForward={0,-.25f,-1};
  rig.pose.joints[1]=gunHand(Head+Point3{.12f,-.3f,-.32f},{0,0,-1},{0,1,0},false);rig.pose.joints[0]=VrRuntime::scriptHand(Head+Point3{-.25f,-.6f,-.1f},{0,0,-1},{0,1,0},true);},
 .step=[](Scene& s,float dt){auto& rig=rigs()[&s];Game& g=s.g();auto& r=*rig.runtime;auto& pose=rig.pose;const int f=s.frame;const std::string scene=s.fixture->value("scene");
  pose.trigger={false,false};pose.reload=false;pose.velocity={};
  if(scene=="shotgun"){
   // Up at the eye, the support hand under the fore-end (knuckles along the wood, handle upright), gripping.
   const Point3 hold=Head+Point3{.09f,-.12f,-.17f},muzzle=unit(Point3{-.16f,.01f,-1});pose.headForward=unit(Point3{-.08f,-.22f,-1});
   pose.joints[1]=gunHand(hold,muzzle,{0,1,0},false);const GunPose gun=gunPose(hold,muzzle,{0,1,0});
   if(f>=40){const Point3 wood=gun.at({0,-.012f,-.38f});float back=0;if(f>=72&&f<84)back=.0075f*float(std::min(f-71,12));if(f>=84)back=.0075f*float(std::max(0,12-(f-83)));
    pose.joints[0]=VrRuntime::scriptHand(wood+gun.back*back,muzzle,{0,1,0},true);pose.grip[0]=true;pose.velocity[0]=f>=72&&f<84?gun.back*.45f:f>=84&&f<96?gun.back*-.45f:Point3{};}
   pose.trigger[1]=f==60;}
  else if(scene=="magazine"){
   // Muzzle up and the gun canted over to the left so the well faces the eye, as a magazine is put in.
   const Point3 hold=Head+Point3{.01f,-.32f,-.26f},muzzle=unit(Point3{-.2f,.75f,-1}),top=spin(Point3{0,1,0},muzzle,1.05f);pose.headForward=unit(Point3{0,-.75f,-1});
   pose.joints[1]=gunHand(hold,muzzle,top,false);const GunPose gun=gunPose(hold,muzzle,top);
   pose.reload=f==30;
   if(f>=40&&f<52){pose.joints[0]=VrRuntime::scriptHand(Head+Point3{-.17f,-.54f,-.04f}+Point3{0,0,-.06f},{0,-1,-.3f},{0,0,-1},true);pose.grip[0]=f>=44;}
   if(f>=52){const auto geo=Game::vrGunGeometry(WeaponId::Handgun);const float depth=f<90?.055f:std::max(0.f,.055f-.006f*float(f-89));
    const Point3 base=gun.at(geo.magwell-geo.magAxis*depth);
    // The support fist under the grip, its fist's channel up the magazine, knuckles across the gun's left side.
    pose.joints[0]=VrRuntime::scriptHand(base,gun.right*-.4f+gun.back*-1.f,gun.up*geo.magAxis.y+gun.back*geo.magAxis.z,true);pose.grip[0]=true;}}
  else{
   // Canted left to show the belly; the head looks down at the loading port.
   const Point3 hold=Head+Point3{.04f,-.4f,-.24f},muzzle=unit(Point3{-.35f,.15f,-1});pose.headForward=unit(Point3{0,-.85f,-.8f});
   pose.joints[1]=gunHand(hold,muzzle,spin(Point3{0,1,0},muzzle,1.15f),false);const GunPose gun=gunPose(hold,muzzle,spin(Point3{0,1,0},muzzle,1.15f));
   if(f>=40&&f<52){pose.joints[0]=VrRuntime::scriptHand(Head+Point3{-.17f,-.54f,-.04f}+Point3{0,0,-.06f},{0,-1,-.3f},{0,0,-1},true);pose.grip[0]=f>=44;}
   if(f>=52){const auto port=Game::vrGunGeometry(WeaponId::Shotgun).loadingPort;const float away=f<90?.05f:std::max(0.f,.05f-.008f*float(f-89));
    const Point3 at=gun.at(port)-gun.up*away;
    pose.joints[0]=VrRuntime::scriptHand(at,gun.up,gun.back*-1.f,true);pose.grip[0]=true;}}
  r.script(g,s.input,pose,dt);
  // Measures, by frame.
  const auto view=g.vrGunView();
  if(scene=="shotgun"){if(f==55){rig.v["snapped"]=r.hands()[0].snapped&&g.vrSupportGrip();}
   if(f==70){rig.v["fired"]=g.player().pool(WeaponId::Shotgun).loaded==5;rig.v["waits"]=!g.vrGunReady();}
   if(f==83){rig.v["pumpBack"]=g.vrPumpOffset();}
   if(f==99){rig.v["chambered"]=g.vrGunReady();}}
  if(scene!="shotgun"&&view.seat>=0&&rig.v.count("firstSeat")==0)rig.v["firstSeat"]=f;
  if(scene=="magazine"&&!view.magOut&&rig.v.count("seatedAt")==0&&f>40)rig.v["seatedAt"]=f;
  if(scene=="magazine"){if(f==36)rig.v["magOut"]=view.magOut&&g.player().pool(WeaponId::Handgun).loaded==1;if(f==50)rig.v["took"]=view.held==1;if(f==80){rig.v["seat"]=view.seat;rig.v["held"]=view.held;}
   if(f==s.frames-1)rig.v["seated"]=!view.magOut&&g.player().pool(WeaponId::Handgun).loaded==12;}
  if(scene=="shell"){if(f==50)rig.v["took"]=view.held==2;if(f==80){rig.v["seat"]=view.seat;rig.v["held"]=view.held;}if(f==s.frames-1)rig.v["loaded"]=g.player().pool(WeaponId::Shotgun).loaded;}},
 .capture=[](Scene& s,const Frame&){const std::string scene=s.fixture->value("scene");const int f=s.frame;
  const bool take=(scene=="shotgun"&&(f==55||f==83))||(scene!="shotgun"&&f==80);
  if(take&&s.renderer){const std::string label=scene+(scene=="shotgun"?(f==55?"-two-hands":"-pumped"):"-mid");s.renderer->renderVrScripted(s.g());s.shoot("vr-eye-"+label);
   // An onlooker a metre off her gun side and ahead, at chest height, looking back at her hands.
   Player look=s.g().view();const Vec2 me=look.pos;look.pos=me+Vec2{.75f,.55f};look.eye=1.25f;look.angle=std::atan2(me.y-look.pos.y,me.x-look.pos.x)-.15f;look.pitch=-18.f;
   s.renderer->renderVrScripted(s.g(),&look);s.shoot("vr-side-"+label);}
  // The dropped magazine where it came to rest, seen from a metre off.
  if(scene=="magazine"&&f==s.frames-6&&s.renderer&&s.g().vrGunView().dropped){const Point3 at=s.g().vrGunView().dropAt;Player look=s.g().view();
   look.pos={at.x-.7f,at.y+.6f};look.eye=.9f;const Point3 eye{look.pos.x,look.pos.y,look.z+look.eye};look.angle=std::atan2(at.y-look.pos.y,at.x-look.pos.x);
   look.pitch=140.f*std::atan2(at.z-eye.z,std::sqrt((at.x-eye.x)*(at.x-eye.x)+(at.y-eye.y)*(at.y-eye.y)));
   s.renderer->renderVrScripted(s.g(),&look);s.shoot("vr-side-magazine-dropped");}},
 .metrics=[](Scene& s,Report& r){auto& v=rigs()[&s].v;const std::string scene=s.fixture->value("scene");
  if(scene=="shotgun"){r.check("the support hand snaps to the fore-end",v["snapped"]>0);r.check("fired two-handed it waits for the pump",v["fired"]>0&&v["waits"]>0);
   r.metric("pump_back_m",v["pumpBack"],"m");r.check("the pump slides back with the hand",v["pumpBack"]>.07,f2(v["pumpBack"]));r.check("forward again it is chambered",v["chambered"]>0);}
  if(scene=="magazine"){r.check("the button drops the magazine",v["magOut"]>0);r.check("the pouch gives a magazine",v["took"]>0);
   r.metric("seat",v["seat"]);r.metric("first_seat_frame",v["firstSeat"]);r.metric("seated_frame",v["seatedAt"]);r.check("part way up the well at the picture",v["held"]==1&&v["seat"]>.2&&v["seat"]<.8,f2(v["seat"]));r.check("pushed home it seats twelve",v["seated"]>0);}
  if(scene=="shell"){r.check("the pouch gives a shell",v["took"]>0);r.metric("seat",v["seat"]);r.check("part way into the port at the picture",v["held"]==2&&v["seat"]>.2&&v["seat"]<.8,f2(v["seat"]));
   r.check("pushed in it adds one",v["loaded"]==3,f2(v["loaded"]));}},
 .teardown=[](Scene& s){rigs().erase(&s);}});
}

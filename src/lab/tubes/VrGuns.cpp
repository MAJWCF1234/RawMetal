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
//  mac:      the MAC-11 held out in both hands (the support hand wrapping the gun hand), a burst with the trigger
//            held (pictures in hand and mid-burst).
//  macreload: the MAC-11 run dry (its knob back), the magazine dropped with the button, a fresh one from the pouch
//            part way up the grip (picture), home, then the support hand pulling the charging knob (picture).
//  machip:   the shotgun in hand and the MAC-11 riding the gun hand's hip (picture from beside her).
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
std::array<Point3,4> gunHand(Point3 hold,Point3 muzzle,Point3 top,bool leftHand,float pitch=VrRuntime::GunPitch){
 const Point3 back=unit(muzzle*-1.f);Point3 up=unit(top-back*dot(top,back));const float c=std::cos(pitch),s=std::sin(pitch);
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
 .fixtures={"vr-guns-shotgun","vr-guns-magazine","vr-guns-shell","vr-guns-mac","vr-guns-macreload","vr-guns-machip"},
 .setup=[](Scene& s){rigs().erase(&s);auto& rig=rigs()[&s];rig.runtime=std::make_unique<VrRuntime>();Game& g=s.g();LabAccess::setGod(g,true);LabAccess::playerCamera(g);
  auto& p=LabAccess::player(g);p.angle=0;p.pitch=0;const std::string scene=s.fixture->value("scene");
  const bool mac=scene=="mac"||scene=="macreload";
  const WeaponId gun=scene=="magazine"?WeaponId::Handgun:mac?WeaponId::MachinePistol:WeaponId::Shotgun;
  if(scene=="machip")LabAccess::equip(g,WeaponId::MachinePistol);   // drawn once, so it rides the hip with the shotgun out
  LabAccess::equip(g,gun);
  p.pool(gun)=gun==WeaponId::Handgun?AmmoPool{36,9}:gun==WeaponId::MachinePistol?AmmoPool{96,scene=="macreload"?0:32}:AmmoPool{24,scene=="shell"?2:6};
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
  else if(scene=="mac"){
   // Held out at the chest, a little low of the eye, the support hand wrapped round the gun hand; a held burst.
   const float pitch=VrRuntime::gunPitch(WeaponId::MachinePistol);const Point3 hold=Head+Point3{.06f,-.24f,-.36f},muzzle=unit(Point3{-.12f,.12f,-1});pose.headForward=unit(Point3{-.05f,-.3f,-1});
   pose.joints[1]=gunHand(hold,muzzle,{0,1,0},false,pitch);
   if(f>=20){pose.joints[0]=VrRuntime::scriptHand(hold+Point3{-.05f,-.03f,0},{.4f,0,-1},{0,1,0},true);pose.grip[0]=true;}
   pose.trigger[1]=f>=50&&f<80;}
  else if(scene=="macreload"){
   // Muzzle up and canted over to the left so the well and the knob face the eye.
   const float pitch=VrRuntime::gunPitch(WeaponId::MachinePistol);
   const Point3 hold=Head+Point3{.01f,-.34f,-.28f},muzzle=unit(Point3{-.2f,.6f,-1}),top=spin(Point3{0,1,0},muzzle,.95f);pose.headForward=unit(Point3{0,-.7f,-1});
   pose.joints[1]=gunHand(hold,muzzle,top,false,pitch);const GunPose gun=gunPose(hold,muzzle,top);
   pose.reload=f==30;const auto geo=Game::vrGunGeometry(WeaponId::MachinePistol);
   if(f>=40&&f<52){pose.joints[0]=VrRuntime::scriptHand(Head+Point3{-.17f,-.54f,-.04f}+Point3{0,0,-.06f},{0,-1,-.3f},{0,0,-1},true);pose.grip[0]=f>=44;}
   if(f>=52&&f<104){const float depth=f<90?.10f:std::max(0.f,.10f-.012f*float(f-89));
    const Point3 base=gun.at(geo.magwell-geo.magAxis*depth);
    pose.joints[0]=VrRuntime::scriptHand(base,gun.right*-.4f+gun.back*-1.f,gun.up*geo.magAxis.y+gun.back*geo.magAxis.z,true);pose.grip[0]=f<100;}
   // The charging knob: taken from above, pulled 3 cm back toward the face, let go.
   if(f>=104){const float back=f<108?0.f:std::min(.03f,.006f*float(f-107));const Point3 knob=gun.at(geo.slideRear+Point3{0,.012f,0})+gun.back*back;
    pose.joints[0]=VrRuntime::scriptHand(knob,gun.right*-1.f+gun.back*-.3f,gun.up*-.2f+gun.back*-1.f,true);pose.grip[0]=f>=106&&f<120;}}
  else if(scene=="machip"){
   // The shotgun up, the MAC-11 at the hip.
   const Point3 hold=Head+Point3{.12f,-.3f,-.32f};pose.joints[1]=gunHand(hold,{0,0,-1},{0,1,0},false);}
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
  if((scene=="magazine"||scene=="shell")&&view.seat>=0&&rig.v.count("firstSeat")==0)rig.v["firstSeat"]=f;
  if(scene=="magazine"&&!view.magOut&&rig.v.count("seatedAt")==0&&f>40)rig.v["seatedAt"]=f;
  if(scene=="magazine"){if(f==36)rig.v["magOut"]=view.magOut&&g.player().pool(WeaponId::Handgun).loaded==1;if(f==50)rig.v["took"]=view.held==1;if(f==80){rig.v["seat"]=view.seat;rig.v["held"]=view.held;}
   if(f==s.frames-1)rig.v["seated"]=!view.magOut&&g.player().pool(WeaponId::Handgun).loaded==12;}
  if(scene=="mac"){if(f==45){rig.v["support"]=g.vrSupportGrip();rig.v["ready"]=g.vrGunReady();}if(f==49)rig.v["before"]=g.player().pool(WeaponId::MachinePistol).loaded;
   if(f>=50&&f<80)rig.v["peak"]=std::max(rig.v["peak"],double(g.vrRecoil().rise));
   if(f==85){rig.v["shots"]=rig.v["before"]-g.player().pool(WeaponId::MachinePistol).loaded;}}
  if(scene=="macreload"){if(f==20)rig.v["locked"]=view.slide>.03f&&!g.vrGunReady();if(f==36)rig.v["magOut"]=view.magOut&&view.dropped;if(f==50)rig.v["took"]=view.held==1;if(f==80){rig.v["seat"]=view.seat;rig.v["held"]=view.held;}
   if(f==101)rig.v["seated"]=!view.magOut&&g.player().pool(WeaponId::MachinePistol).loaded==32&&!g.vrGunReady();
   if(f==112)rig.v["knob"]=view.slide;if(f==s.frames-1)rig.v["chambered"]=g.vrGunReady();}
  if(scene=="machip"&&f==25)rig.v["hip"]=r.avatar(g).hip;
  if(scene=="shell"){if(f==50)rig.v["took"]=view.held==2;if(f==80){rig.v["seat"]=view.seat;rig.v["held"]=view.held;}if(f==s.frames-1)rig.v["loaded"]=g.player().pool(WeaponId::Shotgun).loaded;}},
 .capture=[](Scene& s,const Frame&){const std::string scene=s.fixture->value("scene");const int f=s.frame;
  const bool take=(scene=="shotgun"&&(f==55||f==83))||((scene=="magazine"||scene=="shell"||scene=="macreload")&&f==80)||(scene=="mac"&&(f==45||f==62))||(scene=="macreload"&&f==112);
  if(take&&s.renderer){const std::string label=scene+(scene=="shotgun"?(f==55?"-two-hands":"-pumped"):scene=="mac"?(f==45?"-hand":"-burst"):scene=="macreload"&&f==112?"-knob":"-mid");s.renderer->renderVrScripted(s.g());s.shoot("vr-eye-"+label);
   // An onlooker a metre off her gun side and ahead, at chest height, looking back at her hands.
   Player look=s.g().view();const Vec2 me=look.pos;look.pos=me+Vec2{.75f,.55f};look.eye=1.25f;look.angle=std::atan2(me.y-look.pos.y,me.x-look.pos.x)-.15f;look.pitch=-18.f;
   s.renderer->renderVrScripted(s.g(),&look);s.shoot("vr-side-"+label);}
  // The MAC-11 on her hip: from her gun side, a little ahead, at the belt's height.
  if(scene=="machip"&&f==30&&s.renderer){Player look=s.g().view();const Vec2 me=look.pos;const float a=look.angle;
   look.pos=me+Vec2{std::cos(a)*.55f+std::cos(a-1.5708f)*.75f,std::sin(a)*.55f+std::sin(a-1.5708f)*.75f};look.eye=1.0f;look.angle=std::atan2(me.y-look.pos.y,me.x-look.pos.x);look.pitch=-12.f;
   s.renderer->renderVrScripted(s.g(),&look);s.shoot("vr-side-machip");s.renderer->renderVrScripted(s.g());s.shoot("vr-eye-machip");}
  // The dropped magazine where it came to rest, seen from a metre off.
  if((scene=="magazine"||scene=="macreload")&&f==s.frames-6&&s.renderer&&s.g().vrGunView().dropped){const Point3 at=s.g().vrGunView().dropAt;Player look=s.g().view();
   look.pos={at.x-.7f,at.y+.6f};look.eye=.9f;const Point3 eye{look.pos.x,look.pos.y,look.z+look.eye};look.angle=std::atan2(at.y-look.pos.y,at.x-look.pos.x);
   look.pitch=140.f*std::atan2(at.z-eye.z,std::sqrt((at.x-eye.x)*(at.x-eye.x)+(at.y-eye.y)*(at.y-eye.y)));
   s.renderer->renderVrScripted(s.g(),&look);s.shoot("vr-side-"+scene+"-dropped");}},
 .metrics=[](Scene& s,Report& r){auto& v=rigs()[&s].v;const std::string scene=s.fixture->value("scene");
  if(scene=="shotgun"){r.check("the support hand snaps to the fore-end",v["snapped"]>0);r.check("fired two-handed it waits for the pump",v["fired"]>0&&v["waits"]>0);
   r.metric("pump_back_m",v["pumpBack"],"m");r.check("the pump slides back with the hand",v["pumpBack"]>.07,f2(v["pumpBack"]));r.check("forward again it is chambered",v["chambered"]>0);}
  if(scene=="magazine"){r.check("the button drops the magazine",v["magOut"]>0);r.check("the pouch gives a magazine",v["took"]>0);
   r.metric("seat",v["seat"]);r.metric("first_seat_frame",v["firstSeat"]);r.metric("seated_frame",v["seatedAt"]);r.check("part way up the well at the picture",v["held"]==1&&v["seat"]>.2&&v["seat"]<.8,f2(v["seat"]));r.check("pushed home it seats twelve",v["seated"]>0);}
  if(scene=="mac"){r.check("the support hand wraps the MAC-11; chambered",v["support"]>0&&v["ready"]>0);r.metric("burst_rounds",v["shots"]);r.metric("burst_rise_deg",v["peak"]*57.3,"deg");
   r.check("half a second held fires a burst at 18 a second",v["shots"]>=8&&v["shots"]<=10,f2(v["shots"]));r.check("the burst rides the gun up a little, two-handed",v["peak"]>.01&&v["peak"]<.12,f2(v["peak"]*57.3));}
  if(scene=="macreload"){r.check("run dry its knob is back",v["locked"]>0);r.check("the button drops the magazine",v["magOut"]>0);r.check("the pouch gives a magazine",v["took"]>0);
   r.metric("seat",v["seat"]);r.check("part way up the grip at the picture",v["held"]==1&&v["seat"]>.2&&v["seat"]<.8,f2(v["seat"]));r.check("pushed home it seats 32; the knob still back",v["seated"]>0);
   r.metric("knob_m",v["knob"],"m");r.check("the support hand pulls the knob back",v["knob"]>.02,f2(v["knob"]));r.check("let go, it is chambered",v["chambered"]>0);}
  if(scene=="machip")r.check("the MAC-11 rides the hip with the shotgun out",v["hip"]==int(WeaponId::MachinePistol)+1);
  if(scene=="shell"){r.check("the pouch gives a shell",v["took"]>0);r.metric("seat",v["seat"]);r.check("part way into the port at the picture",v["held"]==2&&v["seat"]>.2&&v["seat"]<.8,f2(v["seat"]));
   r.check("pushed in it adds one",v["loaded"]==3,f2(v["loaded"]));}},
 .teardown=[](Scene& s){rigs().erase(&s);}});
}

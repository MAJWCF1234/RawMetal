// VR guns by hand (Game.h "VR guns by hand"; docs/Systems/vr-playtest.md). Only VR play runs any of this.
//  - The handgun: the reload button drops the magazine (a chambered round stays in); the support hand grips at the
//    belt pouch for a fresh one and slides it up into the grip until it seats, which loads it from the pool as the
//    desktop reload does. An empty gun locks its slide back; the button again (the slide release) or the support
//    hand pulling the slide back 2 cm and letting go chambers a round.
//  - The shotgun: the support hand takes one shell at a time from the pouch and pushes it into the loading port
//    under the receiver. Fired with the support hand on the fore-end it waits for the pump: back 7 cm throws the
//    spent case out of the ejection port (Game::ejectCasing, with the hand's speed), forward again chambers the next.
//    Fired one-handed it cycles itself. Feeding an empty gun leaves its chamber empty until a pump.
//  - The MAC-11 reloads as the handgun does (the button drops the magazine, a fresh one from the pouch slides up into
//    the grip), but it has no bolt release: run dry its charging knob stays back, and the support hand takes the knob
//    on top of the receiver, pulls it back 2 cm and lets go. Held, the trigger fires it at its full-auto rate; each
//    round throws a .380 case from the port on the receiver's right (Game::ejectCasing, with the gun hand's speed),
//    kicks the gun lightly and quickly, and asks a lighter, shorter pulse than the handgun's.
//  - Recoil is on the gun in the hand (VrRuntime turns the drawn and aimed gun by it), never on the view: a kick
//    back and a muzzle rise on stiff, damped springs, half as much with two hands on the gun.
//  - Each event asks the controllers for a haptic pulse (m_vrHaptics, played by VrRuntime) and plays its sound at the
//    gun or the hand, in the world.
#include "Game.h"
#include <cmath>
#include <ostream>
#include <string>
namespace retro {
namespace {
float dotv(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float lenv(Point3 v){return std::sqrt(dotv(v,v));}
Point3 unitv(Point3 v,Point3 fallback={1,0,0}){const float l=lenv(v);return l>1e-6f?v*(1/l):fallback;}
// The guns' working points as the VR hands draw them (measured from medic-handgun.obj, mac11.fbx and the Remington by
// SoftwareRenderer::measureVrGun; the renderer sets them again from the loaded meshes).
Game::VrGunGeometry g_vrPistol{{0,.0433f,-.1705f},{0,.0635f,.0093f},{0,.0622f,-.1613f},{.0142f,.0524f,-.0446f},{0,-.0748f,.0110f},{0,.9848f,-.1739f},{0,.0496f,.0276f},{0,-.0748f,.0110f},.1148f};
Game::VrGunGeometry g_vrMac{{0,.0515f,-.1632f},{0,.0897f,.0781f},{0,.0859f,-.1167f},{.0251f,.0635f,-.0356f},{0,-.1690f,-.0010f},{0,1,.0099f},{0,.0779f,-.0724f},{0,-.1690f,-.0010f},.2086f};
Game::VrGunGeometry g_vrShotgun{{0,.0595f,-.5550f},{0,.0870f,-.0645f},{0,.0844f,-.5371f},{.0378f,.0595f,-.1345f},{0,.0189f,-.1545f},{0,1,0},{0,.0189f,-.1545f},{0,.0189f,-.1545f},.07f};
constexpr float PumpBack=.07f,PumpHome=.025f,RackPull=.02f,RackReach=.06f,MagReach=.05f,ShellReach=.075f,ShellHome=.025f;
}
void Game::setVrGunGeometry(WeaponId id,const VrGunGeometry& g){(id==WeaponId::Handgun?g_vrPistol:id==WeaponId::MachinePistol?g_vrMac:g_vrShotgun)=g;}
Game::VrGunGeometry Game::vrGunGeometry(WeaponId id){return id==WeaponId::Handgun?g_vrPistol:id==WeaponId::MachinePistol?g_vrMac:g_vrShotgun;}
void Game::vrShotLine(const VrGunGeometry& g,Point3 grip,Point3 right,Point3 up,Point3 back,Point3& origin,Point3& direction){
 auto at=[&](Point3 p){return grip+right*p.x+up*p.y+back*p.z;};
 origin=at(g.muzzle);const Point3 rear=at(g.rearSight),front=at(g.frontSight);
 const Point3 sight=unitv(front-rear,back*-1.f);
 direction=unitv(rear+sight*VrZero-origin,back*-1.f);
}
void Game::soundAt(Sound sound,Point3 at,float gain,float pitch){m_sounds.push_back({sound,{at.x,at.y},gain,pitch,false,17,at.z});}
void Game::vrEject(int kind,Point3 at,Point3 velocity){m_vrEjected.push_back({kind,at,velocity});if(m_vrEjected.size()>16)m_vrEjected.erase(m_vrEjected.begin());ejectCasing(kind,at,velocity);}
bool Game::vrGunReady()const{
 if(vrMagazineGun(m_equippedWeapon))return m_vrGun.gun!=m_equippedWeapon||(m_vrGun.chambered&&!m_vrGun.slideBack&&!m_vrGun.racking);
 if(m_equippedWeapon==WeaponId::Shotgun)return !m_vrChamberEmpty&&!m_vrPumpBack;
 return true;}
bool Game::vrRackZone(Point3 fist)const{
 if(!m_vrGunIn.valid||!vrMagazineGun(m_equippedWeapon)||m_vrGun.gun!=m_equippedWeapon||(m_vrGun.chambered&&!m_vrGun.slideBack)||m_vrGun.held)return false;
 const auto g=vrGunGeometry(m_equippedWeapon);const auto&i=m_vrGunIn;const Point3 rear=i.grip+i.right*g.slideRear.x+i.up*g.slideRear.y+i.back*g.slideRear.z;
 return lenv(fist-rear)<RackReach;}
Game::VrGunView Game::vrGunView()const{
 VrGunView v;v.held=m_vrGun.held;v.seat=m_vrGun.seat;v.depth=m_vrGun.depth;
 // The slide's (or the MAC-11's knob's) travel back: locked open, or pulled by the support hand.
 if(vrMagazineGun(m_equippedWeapon)&&m_vrGun.gun==m_equippedWeapon){const bool mac=m_equippedWeapon==WeaponId::MachinePistol;v.magOut=m_vrGun.magOut;
  v.slide=std::max(m_vrGun.slideBack?(mac?.04f:.026f):0.f,m_vrGun.racking?std::min(m_vrGun.rack,mac?.04f:.03f):0.f);}
 v.dropped=m_vrGun.dropped;v.dropGun=m_vrGun.dropGun;v.dropAt=m_vrGun.dropAt;v.dropRight=m_vrGun.dropRight;v.dropUp=m_vrGun.dropUp;v.dropBack=m_vrGun.dropBack;
 return v;}
void Game::vrReloadButton(){
 if(dead()||m_won||!m_weaponEquipped||unarmed()||!vrMagazineGun(m_equippedWeapon)||m_vrGun.gun!=m_equippedWeapon||m_switchTarget!=WeaponId::Fists)return;
 auto&rounds=m_player.pool(m_equippedWeapon);const auto&cycle=equippedWeaponDef().cycle;const auto&i=m_vrGunIn;const auto g=vrGunGeometry(m_equippedWeapon);
 auto at=[&](Point3 p){return i.valid?i.grip+i.right*p.x+i.up*p.y+i.back*p.z:m_vrAimPosition;};
 // The slide release (the handgun's; the MAC-11 has none): a loaded magazine in and the slide locked open.
 if(m_equippedWeapon==WeaponId::Handgun&&m_vrGun.slideBack&&!m_vrGun.magOut&&rounds.loaded>0){m_vrGun.slideBack=false;m_vrGun.chambered=true;soundAt(cycle.rackSound,at(g.slideRear),.6f);vrHaptic(1,.03f,180,.55f,"slide");return;}
 if(m_vrGun.magOut)return;
 // The magazine drops out: its rounds go back to the pool (total is kept), a chambered round stays in.
 m_vrGun.magOut=true;rounds.loaded=m_vrGun.chambered&&rounds.loaded>0?1:0;
 const Point3 base=at(g.magwell),axis=i.valid?unitv(i.right*g.magAxis.x+i.up*g.magAxis.y+i.back*g.magAxis.z,{0,0,1}):Point3{0,0,1};
 m_vrGun.dropped=true;m_vrGun.dropRest=false;m_vrGun.dropAge=0;m_vrGun.dropGun=m_equippedWeapon;m_vrGun.dropAt=base+axis*(g.magLength*.5f);
 m_vrGun.dropVelocity=(i.valid?i.velocity[1]:Point3{})-axis*1.2f;
 m_vrGun.dropRight=i.valid?i.right:Point3{0,1,0};m_vrGun.dropUp=i.valid?i.up:Point3{0,0,1};m_vrGun.dropBack=i.valid?i.back:Point3{-1,0,0};
 soundAt(cycle.magOutSound,base,.5f);vrHaptic(1,.025f,140,.3f,"magazine out");}
void Game::vrShotFired(){
 const auto&i=m_vrGunIn;const bool pistol=m_equippedWeapon==WeaponId::Handgun,mac=m_equippedWeapon==WeaponId::MachinePistol,two=m_vrSupportGrip;
 // Recoil: the gun kicks back and its muzzle rises in the hand; two hands on it take half. The MAC-11's .380 rounds
 // kick little but come eighteen a second: each a third of the handgun's rise, so a held burst rides a few degrees up.
 m_vrGun.kickBackV+=(mac?.4f:pistol?.9f:1.5f)*(two?.55f:1.f);m_vrGun.kickRiseV+=(mac?2.4f:pistol?7.5f:11.f)*(two?.5f:1.f);
 // A short, light buzz per MAC-11 round (shorter than its 55 ms interval, so each shot is felt apart).
 if(mac){vrHaptic(1,.022f,230.f,.6f,"shot");if(two)vrHaptic(0,.018f,230.f,.35f,"shot");}
 else{vrHaptic(1,pistol?.05f:.08f,pistol?180.f:120.f,1.f,"shot");if(two)vrHaptic(0,pistol?.04f:.07f,pistol?180.f:120.f,pistol?.5f:.75f,"shot");}
 auto&rounds=m_player.pool(m_equippedWeapon);const auto&g=vrGunGeometry(m_equippedWeapon);
 auto at=[&](Point3 p){return i.valid?i.grip+i.right*p.x+i.up*p.y+i.back*p.z:m_vrAimPosition;};
 const Point3 throwOut=i.valid?i.right*1.6f+i.up*1.1f+i.back*.35f+i.velocity[1]:Point3{0,0,1.2f};
 if(pistol||mac){
  // The slide (the MAC-11's bolt) cycles: the case flies from the port; the next round comes up, or the slide
  // locks open on an empty magazine (with no magazine in it closes on nothing).
  vrEject(pistol?1:2,at(g.port),throwOut);
  m_vrGun.chambered=!m_vrGun.magOut&&rounds.loaded>0;m_vrGun.slideBack=!m_vrGun.magOut&&rounds.loaded<=0;return;}
 if(m_equippedWeapon!=WeaponId::Shotgun)return;
 // The shotgun: a spent case in the chamber until the pump (or, fired one-handed, its own cycle a moment later).
 m_vrGun.spent=true;m_vrChamberEmpty=true;m_vrPumpBack=false;m_vrGun.cycleAt=two?-1.f:m_elapsed+.12f;}
void Game::updateVrGun(const InputState& input,float dt){
 auto&v=m_vrGun;const auto&i=m_vrGunIn;
 const bool firearm=playerFirearm(m_equippedWeapon),magGun=vrMagazineGun(m_equippedWeapon);
 auto&rounds=m_player.pool(m_equippedWeapon);const auto&cycle=equippedWeaponDef().cycle;
 // A new gun in the hand: its state from its pool (a loaded gun has a round chambered; an empty handgun is open).
 if(v.gun!=m_equippedWeapon){v.gun=m_equippedWeapon;v.held=0;v.magOut=false;v.racking=false;v.rack=v.rackMax=0;v.seat=-1;v.depth=0;v.spent=false;v.cycleAt=-1;
  v.chambered=firearm&&rounds.loaded>0;v.slideBack=magGun&&rounds.loaded<=0;
  m_vrChamberEmpty=m_equippedWeapon==WeaponId::Shotgun&&rounds.loaded<=0;m_vrPumpBack=false;v.wasSupport=false;}
 if(rounds.loaded<=0)v.chambered=false;
 // The recoil springs (substepped: stiff, damped a little under critical, back to rest within a third of a second).
 {const int steps=std::max(1,int(std::ceil(dt/.004f)));const float h=dt/float(steps);
  for(int k=0;k<steps;++k){v.kickRiseV+=(-600.f*v.kickRise-37.f*v.kickRiseV)*h;v.kickRise+=v.kickRiseV*h;
   v.kickBackV+=(-900.f*v.kickBack-48.f*v.kickBackV)*h;v.kickBack+=v.kickBackV*h;}
  if(std::fabs(v.kickRise)<1e-5f&&std::fabs(v.kickRiseV)<1e-4f)v.kickRise=v.kickRiseV=0;if(std::fabs(v.kickBack)<1e-6f&&std::fabs(v.kickBackV)<1e-5f)v.kickBack=v.kickBackV=0;}
 // A dropped magazine falls to the floor and lies there a while.
 if(v.dropped){v.dropAge+=dt;if(!v.dropRest){v.dropVelocity.z-=9.8f*dt;v.dropAt=v.dropAt+v.dropVelocity*dt;const float floor=m_world.floorHeight(v.dropAt.x,v.dropAt.y)+.012f;
   if(v.dropAt.z<floor){v.dropAt.z=floor;v.dropRest=true;
    // It lies on its side: its right face up, its length along the floor (the frame keeps the gun frame's mirrored handedness).
    v.dropBack=unitv({v.dropBack.x,v.dropBack.y,0},{1,0,0});v.dropRight={0,0,1};v.dropUp={-v.dropBack.y,v.dropBack.x,0};soundAt(Sound::PistolDry,v.dropAt,.25f,.7f);}}
  if(v.dropAge>20)v.dropped=false;}
 const bool armed=firearm&&m_weaponEquipped&&!unarmed()&&m_switchTarget==WeaponId::Fists;
 auto at=[&](Point3 p){return i.grip+i.right*p.x+i.up*p.y+i.back*p.z;};
 const Point3 muzzleWay=i.valid?i.back*-1.f:unitv(m_vrAimDirection);
 const Point3 origin=i.valid?i.grip:m_vrAimPosition,support=i.valid?i.fist[0]:m_vrHandPosition[0];
 const bool grip=i.valid?i.grips[0]:input.vrGrip[0],rising=grip&&!v.prevGrip;v.prevGrip=grip;
 if(!grip){v.spentGrip=false;if(v.held){v.held=0;v.seat=-1;}}   // let go of a magazine or shell: it goes back in the pouch
 if(!armed){v.held=0;v.seat=-1;v.racking=false;}
 // The pouch: the support hand takes a magazine for an emptied grip, or a shell for a tube with room.
 if(armed&&i.valid&&rising&&i.pouch&&!v.held&&!v.spentGrip){
  if(magGun&&v.magOut&&rounds.total>rounds.loaded){v.held=1;vrHaptic(0,.015f,120,.25f,"pouch");}
  else if(m_equippedWeapon==WeaponId::Shotgun&&rounds.loaded<cycle.tubeCapacity&&rounds.total>rounds.loaded){v.held=2;vrHaptic(0,.015f,120,.25f,"pouch");}}
 v.seat=-1;v.depth=0;
 // A magazine up into the grip: on the well's line (within 5 cm of it) it slides with the hand; home, it seats.
 if(armed&&i.valid&&v.held==1&&magGun&&v.magOut){
  const auto g=vrGunGeometry(m_equippedWeapon);const Point3 base=at(g.magwell),axis=unitv(i.right*g.magAxis.x+i.up*g.magAxis.y+i.back*g.magAxis.z,i.up);
  const Point3 d=support-base;const float below=-dotv(d,axis),side=lenv(d+axis*below);
  if(below>-.01f&&below<g.magLength+.05f&&side<MagReach){v.seat=std::clamp(1-below/g.magLength,0.f,1.f);v.depth=std::max(0.f,below);
   if(below<=.012f){v.magOut=false;v.held=0;v.spentGrip=true;v.seat=-1;v.depth=0;rounds.loaded=std::min(cycle.tubeCapacity,rounds.total);
    soundAt(cycle.magInSound,base,.6f);vrHaptic(0,.03f,160,.5f,"magazine seated");vrHaptic(1,.03f,160,.4f,"magazine seated");}}}
 // A shell into the loading port: near it the shell is lined up with the tube; at it, it goes in.
 if(armed&&i.valid&&v.held==2&&m_equippedWeapon==WeaponId::Shotgun){
  const Point3 port=at(g_vrShotgun.loadingPort);const float d=lenv(support-port);
  if(d<ShellReach){v.seat=std::clamp(1-(d-ShellHome)/(ShellReach-ShellHome),0.f,1.f);
   if(d<ShellHome){const bool empty=rounds.loaded<=0;if(rounds.loaded<cycle.tubeCapacity&&rounds.total>rounds.loaded)++rounds.loaded;
    if(empty){m_vrChamberEmpty=true;v.spent=false;}   // into an empty gun: the tube has it, the chamber waits for a pump
    v.held=0;v.spentGrip=true;v.seat=-1;soundAt(Sound::Pickup,port,.4f,1.25f);vrHaptic(0,.02f,200,.4f,"shell");}}}
 // The slide by hand (the MAC-11: its charging knob): taken at its rear (when nothing is chambered), pulled 2 cm back
 // and let go, it chambers.
 if(armed&&i.valid&&magGun){const auto g=vrGunGeometry(m_equippedWeapon);const float travel=m_equippedWeapon==WeaponId::MachinePistol?.045f:.035f;
  if(rising&&!v.racking&&!v.held&&!v.spentGrip&&vrRackZone(support)){v.racking=true;v.rackFrom=support;v.rack=v.rackMax=0;}
  if(v.racking){v.rack=std::clamp(dotv(support-v.rackFrom,i.back),0.f,travel);v.rackMax=std::max(v.rackMax,v.rack);
   if(!grip||(v.rackMax>=RackPull&&v.rack<.005f)){
    if(v.rackMax>=RackPull){v.slideBack=false;v.chambered=!v.magOut&&rounds.loaded>0;soundAt(cycle.rackSound,at(g.slideRear),.6f);vrHaptic(0,.03f,180,.55f,"slide");vrHaptic(1,.02f,180,.3f,"slide");}
    v.racking=false;v.rack=v.rackMax=0;if(grip)v.spentGrip=true;}}}
 else v.racking=false;
 // The pump: the support hand's place along the bore from the grip. Taking the fore-end sets where the wood rests;
 // a hand that slides forward on the wood moves that on. Back 7 cm throws out a spent case, forward again chambers.
 m_vrPumpAlong=dotv(support-origin,muzzleWay);
 if(armed&&m_equippedWeapon==WeaponId::Shotgun&&m_vrSupportGrip&&!v.held){
  if(!v.wasSupport){m_vrPumpRest=m_vrPumpAlong;m_vrPumpBack=false;}
  if(!m_vrPumpBack&&m_vrPumpAlong>m_vrPumpRest)m_vrPumpRest=m_vrPumpAlong;
  const float pulled=m_vrPumpRest-m_vrPumpAlong;const Point3 hand=i.valid?support:origin;
  if(!m_vrPumpBack&&pulled>PumpBack){m_vrPumpBack=true;
   if(v.spent){v.spent=false;vrEject(0,i.valid?at(g_vrShotgun.port):origin,i.valid?i.right*1.5f+i.up*.8f+i.velocity[0]:Point3{0,0,1});}
   soundAt(Sound::Pump,hand,.55f,.92f);vrHaptic(0,.025f,150,.45f,"pump back");}
  else if(m_vrPumpBack&&pulled<PumpHome){m_vrPumpBack=false;if(m_vrChamberEmpty&&rounds.loaded>0)m_vrChamberEmpty=false;
   soundAt(Sound::Pump,hand,.9f);vrHaptic(0,.03f,150,.5f,"pump forward");}
  v.wasSupport=true;}
 else{v.wasSupport=false;m_vrPumpBack=false;m_vrPumpRest=m_vrPumpAlong;}
 // Fired one-handed, the shotgun cycles itself: the case out, the next shell in.
 if(v.cycleAt>0&&m_elapsed>=v.cycleAt){v.cycleAt=-1;if(v.spent&&m_equippedWeapon==WeaponId::Shotgun){v.spent=false;
   vrEject(0,i.valid?at(g_vrShotgun.port):origin,i.valid?i.right*1.5f+i.up*.8f+i.velocity[1]:Point3{0,0,1});m_vrChamberEmpty=rounds.loaded<=0;soundAt(Sound::Pump,i.valid?i.grip:origin,.65f);}}
 // A pull on a gun that is not ready (nothing chambered, the pump not worked) clicks.
 if(armed&&input.fire&&!m_previousFire&&!vrGunReady()&&m_holster<cycle.raiseHolster){soundAt(cycle.emptySound,i.valid?i.grip:origin,.5f);vrHaptic(1,.012f,220,.2f,"dry");}
}
Game Game::vrGunScene(WeaponId gun,AmmoPool rounds,bool leftHanded){
 auto g=validationScene(Enemy::Kind::Huntsman,3);g.m_enemies.clear();g.grantHandgun(0);if(gun==WeaponId::MachinePistol)g.grantMachinePistol(0);g.m_player.pool(gun)=rounds;g.equipWeapon(gun);g.m_settings.vrLeftHanded=leftHanded;
 for(int k=0;k<24;++k)g.update({},1.f/60);return g;}
// --vr-test / engine.vr: the hand reloads, the pump, the recoil and the haptic pulses, from scripted hands.
bool Game::testVrGuns(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto has=[](const Game& g,const char* cause,int hand=-1){for(const auto&h:g.m_vrHaptics)if(std::string(h.cause)==cause&&(hand<0||h.hand==hand))return true;return false;};
 // A gun held level ahead of her at chest height: grip, right, up, back (game space: she faces +x, her right is +y).
 auto scene=[&](WeaponId gun,AmmoPool rounds){return vrGunScene(gun,rounds);};
 struct Hands{Point3 support{},supportVelocity{};bool grip=false,pouch=false,fire=false,reload=false,two=false;};
 auto frame=[&](Game& g,const Hands& h){const Point3 grip{g.player().pos.x+.35f,g.player().pos.y+.05f,g.player().z+1.25f};
  VrGunInput in;in.valid=true;in.grip=grip;in.right={0,1,0};in.up={0,0,1};in.back={-1,0,0};in.fist={h.support,grip};in.velocity={h.supportVelocity,Point3{}};in.grips={h.grip,true};in.pouch=h.pouch;
  g.setVrGunInput(in);Point3 o,d;vrShotLine(vrGunGeometry(g.m_equippedWeapon),in.grip,in.right,in.up,in.back,o,d);g.setVrAim(o,d,true,h.two);
  InputState s;s.vrTracked=true;s.vrYaw=g.player().angle;s.vrPitch=0;s.vrEye=g.player().eye;s.fire=h.fire;s.reload=h.reload;s.vrGrip={h.grip,true};g.update(s,1.f/60);return in;};
 auto local=[](const VrGunInput& in,Point3 p){return in.grip+in.right*p.x+in.up*p.y+in.back*p.z;};
 // --- The handgun: drop, the pouch, seat ------------------------------------------------------------------------
 {auto g=scene(WeaponId::Handgun,{30,5});Hands h;h.support={g.player().pos.x+.2f,g.player().pos.y-.3f,g.player().z+.9f};
  auto in=frame(g,h);check(g.vrGunReady()&&g.m_vrGun.chambered&&!g.m_vrGun.magOut,"a loaded handgun comes up chambered");
  h.reload=true;in=frame(g,h);h.reload=false;frame(g,h);
  check(g.m_vrGun.magOut&&g.player().pool(WeaponId::Handgun)==AmmoPool{30,1}&&g.vrGunReady()&&has(g,"magazine out",1)&&g.vrGunView().dropped,"the button drops the magazine: its rounds back to the pool, the chambered one still in, the magazine falls");
  for(int k=0;k<120;++k)frame(g,h);check(g.vrGunView().dropped&&std::fabs(g.vrGunView().dropAt.z-(g.world().floorHeight(g.vrGunView().dropAt.x,g.vrGunView().dropAt.y)+.012f))<.005f,"the dropped magazine lands on the floor");
  h.pouch=true;h.grip=true;frame(g,h);check(g.vrGunView().held==1&&has(g,"pouch",0),"gripping at the belt pouch takes a fresh magazine");
  h.pouch=false;const auto&geo=vrGunGeometry(WeaponId::Handgun);const Point3 axis=unitv(in.right*geo.magAxis.x+in.up*geo.magAxis.y+in.back*geo.magAxis.z);const Point3 base=local(in,geo.magwell);
  h.support=base-axis*.2f;frame(g,h);const bool far=g.vrGunView().seat<0;
  h.support=base-axis*.06f+in.right*.01f;frame(g,h);const auto mid=g.vrGunView();
  check(far&&mid.held==1&&mid.seat>.3f&&mid.seat<.7f&&std::fabs(mid.depth-.06f)<.012f&&g.m_vrGun.magOut,"under the grip, on the well's line, it is part way in (seat "+std::to_string(mid.seat)+")");
  g.m_vrHaptics.clear();h.support=base-axis*.005f;frame(g,h);
  check(!g.m_vrGun.magOut&&g.vrGunView().held==0&&g.player().pool(WeaponId::Handgun)==AmmoPool{30,12}&&has(g,"magazine seated",0)&&has(g,"magazine seated",1),"pushed home it seats: twelve loaded from the pool of thirty (both hands feel it)");
  frame(g,h);check(g.vrGunView().held==0,"the hand that seated it holds nothing until it opens");
  h.support=base-axis*.2f;h.grip=false;frame(g,h);h.pouch=true;h.grip=true;frame(g,h);check(g.vrGunView().held==0,"with a magazine in, the pouch gives no other");}
 // --- The handgun run dry: the slide locks; the release, and the rack by hand ----------------------------------
 {auto g=scene(WeaponId::Handgun,{14,2});Hands h;h.support={g.player().pos.x+.2f,g.player().pos.y-.3f,g.player().z+.9f};frame(g,h);
  for(int k=0;k<40;++k){h.fire=(k%10)<2;frame(g,h);}
  check(g.player().pool(WeaponId::Handgun)==AmmoPool{12,0}&&g.m_vrGun.slideBack&&!g.vrGunReady()&&g.vrGunView().slide>.02f,"the last round locks the slide back");
  check(g.m_vrEjected.size()==2&&g.m_vrEjected.back().kind==1,"each pistol shot throws a 9 mm case (ejectCasing)");
  g.m_vrHaptics.clear();h.fire=true;frame(g,h);h.fire=false;frame(g,h);check(has(g,"dry",1)&&g.player().pool(WeaponId::Handgun).total==12,"a pull on the open, empty gun only clicks");
  h.reload=true;auto in=frame(g,h);h.reload=false;frame(g,h);
  const auto&geo=vrGunGeometry(WeaponId::Handgun);const Point3 axis=unitv(in.right*geo.magAxis.x+in.up*geo.magAxis.y+in.back*geo.magAxis.z);const Point3 base=local(in,geo.magwell);
  h.pouch=true;h.grip=true;frame(g,h);h.pouch=false;h.support=base-axis*.05f;frame(g,h);h.support=base;frame(g,h);
  check(!g.m_vrGun.magOut&&g.player().pool(WeaponId::Handgun)==AmmoPool{12,12}&&g.m_vrGun.slideBack&&!g.vrGunReady(),"a fresh magazine in an open gun: twelve in, still open");
  g.m_vrHaptics.clear();h.grip=false;frame(g,h);h.reload=true;frame(g,h);h.reload=false;frame(g,h);
  check(!g.m_vrGun.slideBack&&g.vrGunReady()&&has(g,"slide",1),"the button again releases the slide: a round chambered");
  // The same gun open again, racked by hand instead.
  g.m_vrGun.slideBack=true;g.m_vrGun.chambered=false;const Point3 rear=local(in,geo.slideRear);
  h.support=rear+in.up*.01f;h.grip=true;frame(g,h);const bool took=g.vrRacking();
  h.support=rear+in.back*.03f;frame(g,h);const float pulled=g.vrGunView().slide;g.m_vrHaptics.clear();h.grip=false;frame(g,h);
  check(took&&pulled>.025f&&!g.m_vrGun.slideBack&&g.vrGunReady()&&has(g,"slide",0),"or the support hand takes the slide, pulls it back 3 cm and lets go: chambered");
  h.support=rear+in.up*.01f;h.grip=true;frame(g,h);check(!g.vrRacking(),"with a round chambered the slide is not taken (the hand holds the gun instead)");}
 // --- The shotgun: shells one at a time, the pump ---------------------------------------------------------------
 {auto g=scene(WeaponId::Shotgun,{20,2});Hands h;h.support={g.player().pos.x+.2f,g.player().pos.y-.3f,g.player().z+.9f};auto in=frame(g,h);
  const Point3 port=local(in,vrGunGeometry(WeaponId::Shotgun).loadingPort);int inserted=0;bool each=true,mid=false;
  for(int s=0;s<4;++s){h.pouch=true;h.grip=true;frame(g,h);h.pouch=false;each&=g.vrGunView().held==2;
   h.support=port-in.up*.05f;frame(g,h);mid|=g.vrGunView().seat>0&&g.vrGunView().seat<1;const int before=g.player().pool(WeaponId::Shotgun).loaded;
   h.support=port;frame(g,h);inserted+=g.player().pool(WeaponId::Shotgun).loaded-before;each&=g.player().pool(WeaponId::Shotgun).loaded==before+(before<6?1:0);
   frame(g,h);each&=g.vrGunView().held==0;h.grip=false;h.support=port-in.up*.3f;frame(g,h);}
  check(each&&inserted==4&&mid&&g.player().pool(WeaponId::Shotgun)==AmmoPool{20,6}&&has(g,"shell",0),"each shell from the pouch pushed into the port adds one, up to the tube's six ("+std::to_string(inserted)+" in)");
  h.pouch=true;h.grip=true;frame(g,h);check(g.vrGunView().held==0,"a full tube: the pouch gives no shell");h.grip=false;h.pouch=false;frame(g,h);
  // Fired with the support hand on the fore-end: it waits for the pump.
  const Point3 wood=in.grip+in.back*-.36f;h.support=wood;h.grip=true;h.two=true;frame(g,h);frame(g,h);g.m_vrHaptics.clear();
  h.fire=true;frame(g,h);h.fire=false;for(int k=0;k<20;++k)frame(g,h);
  check(g.player().pool(WeaponId::Shotgun)==AmmoPool{19,5}&&!g.vrGunReady()&&g.m_vrGun.spent&&g.m_vrEjected.empty()&&has(g,"shot",1)&&has(g,"shot",0),"fired two-handed it holds the spent case and waits for the pump (both hands feel the shot)");
  h.fire=true;frame(g,h);h.fire=false;frame(g,h);check(g.player().pool(WeaponId::Shotgun).loaded==5,"no second shot before the pump");
  g.m_vrHaptics.clear();h.supportVelocity=in.back*1.2f;for(int k=1;k<=6;++k){h.support=wood+in.back*(.015f*float(k));frame(g,h);}
  const bool ejected=g.m_vrEjected.size()==1&&g.m_vrEjected[0].kind==0;const auto e=ejected?g.m_vrEjected[0]:VrEject{};
  const Point3 portAt=local(in,vrGunGeometry(WeaponId::Shotgun).port);
  check(ejected&&lenv(e.at-portAt)<1e-3f&&dotv(e.velocity,in.right)>1.f&&dotv(e.velocity,in.back)>.8f&&has(g,"pump back",0)&&g.vrPumpOffset()>.07f,"back 9 cm: the case is thrown from the ejection port (world space), out to the right and with the hand's backward speed");
  check(!g.vrGunReady(),"pumped back, not yet chambered");
  h.supportVelocity=in.back*-1.2f;for(int k=5;k>=0;--k){h.support=wood+in.back*(.015f*float(k));frame(g,h);}
  check(g.vrGunReady()&&has(g,"pump forward",0)&&g.vrPumpOffset()<.01f,"forward again: chambered");
  // One-handed (support hand off) it fires and cycles itself.
  h.two=false;h.grip=false;h.support={g.player().pos.x+.2f,g.player().pos.y-.3f,g.player().z+.9f};for(int k=0;k<40;++k)frame(g,h);g.m_vrEjected.clear();
  h.fire=true;frame(g,h);h.fire=false;for(int k=0;k<20;++k)frame(g,h);
  check(g.player().pool(WeaponId::Shotgun).loaded==4&&g.vrGunReady()&&g.m_vrEjected.size()==1,"fired one-handed it cycles itself and throws the case");
  // Shells into an empty gun need a pump before it fires.
  g.m_player.pool(WeaponId::Shotgun)={10,0};g.m_vrChamberEmpty=true;g.m_vrGun.spent=false;
  h.pouch=true;h.grip=true;frame(g,h);h.pouch=false;h.support=port;frame(g,h);h.grip=false;frame(g,h);
  check(g.player().pool(WeaponId::Shotgun).loaded==1&&!g.vrGunReady(),"a shell into an empty gun waits in the tube until a pump");
  h.support=wood;h.grip=true;h.two=true;frame(g,h);frame(g,h);for(int k=1;k<=6;++k){h.support=wood+in.back*(.015f*float(k));frame(g,h);}for(int k=5;k>=0;--k){h.support=wood+in.back*(.015f*float(k));frame(g,h);}
  check(g.vrGunReady(),"one pump chambers it");}
 // --- The MAC-11: full auto, a light pulse and a .380 case a round, the hand reload, the charging knob ----------
 {auto g=scene(WeaponId::MachinePistol,{96,32});Hands h;h.support={g.player().pos.x+.2f,g.player().pos.y-.3f,g.player().z+.9f};auto in=frame(g,h);
  const auto&geo=vrGunGeometry(WeaponId::MachinePistol);const float rate=1/playerWeaponDef(WeaponId::MachinePistol).cycle.fireCooldown;
  check(g.vrGunReady()&&g.m_vrGun.chambered&&!g.m_vrGun.magOut,"a loaded MAC-11 comes up chambered");
  // A short burst: every round throws one .380 case from the port, through ejectCasing once (the desktop shot skips it).
  g.m_vrHaptics.clear();g.m_vrEjected.clear();const size_t casesBefore=g.m_casings.size();h.fire=true;for(int k=0;k<12;++k)frame(g,h);h.fire=false;frame(g,h);
  int shots=32-g.player().pool(WeaponId::MachinePistol).loaded;bool atPort=!g.m_vrEjected.empty();
  for(const auto&e:g.m_vrEjected)atPort&=e.kind==2&&lenv(e.at-local(in,geo.port))<1e-3f&&dotv(e.velocity,in.right)>1.f;
  check(shots>=3&&int(g.m_vrEjected.size())==shots&&int(g.m_casings.size()-casesBefore)==shots&&atPort,"each round throws one .380 case out of the port on the right ("+std::to_string(shots)+" rounds, "+std::to_string(g.m_casings.size()-casesBefore)+" cases)");
  // A second with the trigger held: rounds at the gun's rate, a light, short pulse for each.
  for(int k=0;k<30;++k)frame(g,h);g.m_vrHaptics.clear();const int before=g.player().pool(WeaponId::MachinePistol).loaded;
  h.fire=true;float peak=0;for(int k=0;k<60;++k){frame(g,h);peak=std::max(peak,g.vrRecoil().rise);}h.fire=false;frame(g,h);
  shots=before-g.player().pool(WeaponId::MachinePistol).loaded;int pulses=0;bool light=true;
  for(const auto&p:g.m_vrHaptics)if(std::string(p.cause)=="shot"&&p.hand==1){++pulses;light&=p.amplitude<.9f&&p.amplitude>=.4f&&p.seconds<1/rate;}
  check(std::abs(float(shots)-rate)<=1.5f,"held for a second it fires "+std::to_string(shots)+" rounds (its rate is "+std::to_string(int(rate+.5f))+" a second)");
  check(pulses==shots&&light,"a pulse per round ("+std::to_string(pulses)+"), lighter than the handgun's and shorter than the interval between rounds");
  check(peak>.02f&&peak<.2f&&g.player().pitch==0.f,"the burst rides the gun up "+std::to_string(int(peak*57.3f+.5f))+" degrees in the hand; the view stays");
  for(int k=0;k<40;++k)frame(g,h);check(std::fabs(g.vrRecoil().rise)<.005f,"let go, the gun settles back in the hand");
  // Run dry: the knob stays back. The button drops the magazine; the pouch gives a fresh one; seated, 32 from the pool.
  h.fire=true;for(int k=0;k<60;++k)frame(g,h);h.fire=false;frame(g,h);
  const int pool=g.player().pool(WeaponId::MachinePistol).total;
  check(g.player().pool(WeaponId::MachinePistol).loaded==0&&g.m_vrGun.slideBack&&!g.vrGunReady()&&g.vrGunView().slide>.03f,"the last round leaves the charging knob back");
  h.reload=true;frame(g,h);h.reload=false;frame(g,h);
  check(g.m_vrGun.magOut&&g.vrGunView().dropped&&g.vrGunView().dropGun==WeaponId::MachinePistol&&has(g,"magazine out",1),"the button drops the long magazine");
  h.pouch=true;h.grip=true;frame(g,h);h.pouch=false;check(g.vrGunView().held==1,"the belt pouch gives a fresh MAC-11 magazine");
  const Point3 axis=unitv(in.right*geo.magAxis.x+in.up*geo.magAxis.y+in.back*geo.magAxis.z);const Point3 base=local(in,geo.magwell);
  h.support=base-axis*.1f;frame(g,h);const auto mid=g.vrGunView();h.support=base-axis*.005f;g.m_vrHaptics.clear();frame(g,h);
  check(mid.held==1&&mid.seat>.3f&&mid.seat<.7f&&!g.m_vrGun.magOut&&g.player().pool(WeaponId::MachinePistol)==AmmoPool{pool,std::min(32,pool)}&&has(g,"magazine seated",0),"slid up the grip it seats: "+std::to_string(g.player().pool(WeaponId::MachinePistol).loaded)+" from the .380 pool (part way at seat "+std::to_string(mid.seat).substr(0,4)+")");
  check(g.m_vrGun.slideBack&&!g.vrGunReady(),"the knob is still back: not ready");
  h.grip=false;frame(g,h);h.reload=true;frame(g,h);h.reload=false;frame(g,h);
  check(g.m_vrGun.magOut&&g.m_vrGun.slideBack,"the button is no bolt release: it drops the magazine again");
  h.pouch=true;h.grip=true;frame(g,h);h.pouch=false;h.support=base-axis*.05f;frame(g,h);h.support=base;frame(g,h);h.grip=false;h.support=base-axis*.2f;frame(g,h);
  // The charging knob by hand: taken on top of the receiver, pulled back 3 cm and let go.
  const Point3 knob=local(in,geo.slideRear);h.support=knob+in.up*.01f;h.grip=true;frame(g,h);const bool took=g.vrRacking();
  h.support=knob+in.back*.03f;frame(g,h);const float pulled=g.vrGunView().slide;g.m_vrHaptics.clear();h.grip=false;frame(g,h);
  check(!g.m_vrGun.magOut&&took&&pulled>.025f&&!g.m_vrGun.slideBack&&g.vrGunReady()&&has(g,"slide",0),"the support hand pulls the charging knob back and lets go: chambered");
  g.m_vrEjected.clear();h.fire=true;frame(g,h);h.fire=false;frame(g,h);check(g.m_vrEjected.size()==1,"and it fires");}
 // --- Recoil on the gun in the hand, never the view --------------------------------------------------------------
 for(WeaponId gun:{WeaponId::Handgun,WeaponId::Shotgun}){auto g=scene(gun,{20,6});Hands h;h.support={g.player().pos.x+.2f,g.player().pos.y-.3f,g.player().z+.9f};frame(g,h);g.m_vrHaptics.clear();
  const float yaw=g.player().angle,pitch=g.player().pitch;h.fire=true;frame(g,h);h.fire=false;float peak=0,back=0,settled=-1;
  for(int k=1;k<=60;++k){frame(g,h);const auto r=g.vrRecoil();peak=std::max(peak,r.rise);back=std::max(back,r.back);if(settled<0&&k>3&&std::fabs(r.rise)<.005f&&std::fabs(r.back)<.001f)settled=float(k)/60;}
  const std::string name=gun==WeaponId::Handgun?"handgun":"shotgun";
  check(peak>.06f&&back>.008f&&settled>0&&settled<=.35f,name+": the shot kicks the gun "+std::to_string(int(peak*57.3f+.5f))+" degrees up and "+std::to_string(int(back*1000+.5f))+" mm back, back in the hand within "+std::to_string(int(settled*1000+.5f))+" ms");
  check(g.player().angle==yaw&&g.player().pitch==pitch,name+": the view does not move with the recoil");
  // The same gun fired again with two hands on it (the shotgun pumped first: it waited for one after the shot).
  if(gun==WeaponId::Shotgun){g.m_vrChamberEmpty=false;g.m_vrGun.spent=false;g.m_vrGun.cycleAt=-1;}
  for(int k=0;k<20;++k)frame(g,h);h.two=true;h.fire=true;frame(g,h);h.fire=false;float peak2=0;for(int k=0;k<30;++k){frame(g,h);peak2=std::max(peak2,g.vrRecoil().rise);}
  check(peak2>.02f&&peak2<peak*.7f,name+": two hands on it take about half the muzzle rise");
  // The haptic pulses asked for the two shots: one sharp pulse in the gun hand each, the support hand's on the second.
  int gunPulses=0,supportPulses=0;bool sharp=true;for(const auto&p:g.m_vrHaptics)if(std::string(p.cause)=="shot"){(p.hand==1?gunPulses:supportPulses)++;if(p.hand==1)sharp&=p.amplitude>=.9f;}
  check(gunPulses==2&&supportPulses==1&&sharp,name+": each shot asks a full-strength pulse of the gun hand, and of the support hand when it is on the gun");}
 {Game g;g.vrHaptic(1,.05f,180,1,"shot");const auto taken=g.takeVrHaptics();check(taken.size()==1&&g.vrHaptics().empty(),"VrRuntime takes the pulses asked (the list empties)");
  report<<"INFO: haptic events: shot (gun hand 1.0, support .5-.75 with two hands; the MAC-11 .6 for 22 ms a round, support .35), magazine out .3, pouch .25, magazine seated .5/.4, slide .55, pump back .45, pump forward .5, shell .4, dry .2; draw .35 and holster .3 (VrRuntime)\n";}
 return ok;
}
}

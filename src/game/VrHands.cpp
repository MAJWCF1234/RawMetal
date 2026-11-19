// VR hands as bodies (Vectra): each tracked hand that holds nothing is a kinematic sphere in the prop solver, so
// it pushes what it meets by contact, and its drawn place (the proxy) is held out of every prop.
//  - The hand's body sits where the drawn hand is, moving toward the tracked hand at no more than 2.5 m/s: a
//    swat moves a light thing at most that fast, so a hand never launches what it brushes.
//  - Props of 12 kg and up, pinned and jointed-to-the-room ones are left out of its contacts: the drawn hand
//    stops at their surface and they do not move (a hand does not shove a drum or a filing cabinet about).
//  - After each step the drawn hand goes on toward the tracked one 2 cm at a time, set out of each prop it would
//    be inside along that prop's nearest face: it never passes into one or through a thin one.
//  - The renderer draws each hand at its proxy (Scene3D drawTrackedHands, Game::vrHandOffset).
#include "Game.h"
#include "physics/PropBodies.h"
#include "../world/ModelIds.h"
#include <cmath>
#include <sstream>
namespace retro {
namespace {
constexpr float HandRadius=.045f,HandPush=2.5f,HeavyHand=12.f,Stride=.02f;
float lengthOf(Point3 d){return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
}
bool Game::vrHandFree(int hand)const{
 if(!m_vrInputActive||m_noclip||m_inspectionCamera||hand<0||hand>1||!m_vrHandValid[size_t(hand)])return false;
 return !(holdingClutter()&&(hand==m_vrCarryHand||hand==m_vrSecondHand));}
bool Game::vrHandsNear(const std::vector<Clutter>& list)const{
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand))continue;const Point3 p=m_vrHandPosition[size_t(hand)],q=m_vrHandProxySet[size_t(hand)]?m_vrHandProxy[size_t(hand)]:p;
  for(const auto&c:list){if(c.model<0||c.broken)continue;const auto e=c.extent();const float r=HandRadius+.25f;
   for(const Point3 at:{p,q})if(std::fabs(at.x-c.pos.x)<e[0]+r&&std::fabs(at.y-c.pos.y)<e[1]+r&&at.z>c.z-r&&at.z<c.z+2*e[2]+r)return true;}}
 return false;}
void Game::addVrHandBodies(vectra::Space& space,std::vector<Clutter>& list,float h){
 using namespace vectra;static Shape ball=[]{Shape s;s.add(PartKind::Sphere,{},{HandRadius,HandRadius,HandRadius});s.finish();return s;}();(void)list;
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand))continue;auto&proxy=m_vrHandProxy[size_t(hand)];if(!m_vrHandProxySet[size_t(hand)]){proxy=m_vrHandPosition[size_t(hand)];m_vrHandProxySet[size_t(hand)]=true;}
  Body b;b.shape=&ball;b.x={proxy.x,proxy.y,proxy.z};b.kinematic=true;b.setMass(0);b.gravity={};b.material={.8f,.7f,0,.002f};b.sleeping=false;b.sleepTime=0;
  const Point3 to=m_vrHandPosition[size_t(hand)]-proxy;V3 v=V3{to.x,to.y,to.z}*(1/std::max(h,1e-4f));const float s=len(v);if(s>HandPush)v=v*(HandPush/s);b.v=v;
  // What it is about to touch wakes (a kinematic body does not wake a sleeper by contact alone).
  if(s>.02f)for(auto&o:space.bodies)if(o.user>=0&&o.sleeping&&vrHandMeets(o)&&o.shape&&lenSq(o.x-b.x)<(o.shape->bound+HandRadius+.05f)*(o.shape->bound+HandRadius+.05f)){o.sleeping=false;o.sleepTime=0;}
  b.user=VrHandBody-hand;b.id=(1<<19)-40-hand;space.bodies.push_back(b);}}
// --vr-test / engine.vr: the hardening of the VR hands, the two-hand release and the Physgloves lock.
bool Game::testVrHardening(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const char* what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 // Two hands letting go a few frames apart: thrown with both, not handed to the last hand for a frame.
 {float grace=0;int a=twoHandRelease(false,true,grace,1.f/90),b=twoHandRelease(false,false,grace,1.f/90);check(a==0&&b==3,"both hands opening a frame apart throw the prop with both");
  grace=0;int last=0;for(int i=0;i<30&&!last;++i)last=twoHandRelease(false,true,grace,1.f/90);check(last==2,"the carry hand letting go alone hands it to the other after the grace");
  grace=0;last=0;for(int i=0;i<30&&!last;++i)last=twoHandRelease(true,false,grace,1.f/90);check(last==1,"the second hand letting go alone leaves it in the carry hand");
  grace=0;twoHandRelease(false,true,grace,.05f);check(twoHandRelease(true,true,grace,.05f)==0&&grace==0,"a hand closing again within the grace keeps both");}
 // A pinned prop is not taken by a VR hand.
 {auto g=validationScene(Enemy::Kind::Huntsman);g.m_enemies.clear();g.m_clutter={{{4.2f,4.5f},{},.56f}};g.m_clutter[0].kind=2;g.m_clutter[0].pinned=true;
  g.setVrHand(0,{4.2f,4.5f,.7f},{1,0,0},true);g.setVrHand(1,{}, {},false);check(!g.interactClutter()&&!g.holdingClutter(),"a bolted-down prop stays where it is");
  g.m_clutter[0].pinned=false;check(g.interactClutter()&&g.holdingClutter(),"loose, the same prop is taken");}
 // The Physgloves lock: not the gun hand while a gun is drawn, not the hand on the fore-end.
 {auto g=validationScene(Enemy::Kind::Huntsman);g.m_vrInputActive=true;g.m_weaponEquipped=true;g.m_equippedWeapon=WeaponId::Shotgun;g.m_holster=0;g.m_switchTarget=WeaponId::Fists;g.m_switchRaise=false;
  const bool gunHand=g.physgloveHandFree(1),offHand=g.physgloveHandFree(0);g.m_vrSupportGrip=true;const bool support=g.physgloveHandFree(0);
  g.m_holster=1;g.m_vrSupportGrip=false;check(!gunHand&&offHand&&!support&&g.physgloveHandFree(1),"the Physgloves lock with a free hand only");}
 // A tracked hand that jumps (tracking regained) is drawn there at once, not dragged through what lies between.
 {auto g=validationScene(Enemy::Kind::Huntsman);g.m_enemies.clear();g.setVrHand(0,{4.f,4.5f,1.f},{1,0,0},true);g.setVrHand(1,{}, {},false);
  vectra::Space space;g.addVrHandBodies(space,g.m_clutter,1.f/90);g.setVrHand(0,{7.f,4.5f,1.f},{1,0,0},true);g.settleVrHands(space,-1);
  check(lengthOf(g.vrHandProxy(0)-Point3{7.f,4.5f,1.f})<1e-4f&&!g.vrHandBlocked(0),"a hand that jumps 3 m is drawn where it is");}
 return ok;
}
bool Game::vrHandMeets(const vectra::Body& other)const{return !other.kinematic&&other.invMass>=1/HeavyHand;}
void Game::settleVrHands(const vectra::Space& space,int held){
 using namespace vectra;
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand)||!m_vrHandProxySet[size_t(hand)])continue;auto&proxy=m_vrHandProxy[size_t(hand)];const Point3 goal=m_vrHandPosition[size_t(hand)];
  // A jump of the tracked hand (tracking regained, a recalibration, a new map) is no stroke through the props
  // between: the drawn hand goes straight there.
  if(lengthOf(goal-proxy)>.6f){proxy=goal;m_vrHandBlocked[size_t(hand)]=false;continue;}
  Point3 way=goal-proxy;const float d=lengthOf(way);const int n=std::max(1,int(std::ceil(d/Stride)));bool blocked=false;
  for(int k=0;k<n;++k){Point3 p=proxy+way*(1.f/float(n));
   for(const auto&b:space.bodies){if(b.user<0||b.user==held||!b.shape)continue;
    if(std::fabs(b.x.x-p.x)>b.shape->bound+HandRadius||std::fabs(b.x.y-p.y)>b.shape->bound+HandRadius||std::fabs(b.x.z-p.z)>b.shape->bound+HandRadius)continue;
    for(int part=0;part<b.shape->count;++part){V3 surface,normal;float distance;if(!nearestPoint(b,part,{p.x,p.y,p.z},surface,normal,distance))continue;
     if(distance<HandRadius){const float push=HandRadius-distance+.001f;p=p+Point3{normal.x,normal.y,normal.z}*push;blocked=true;}}}
   proxy=p;}
  m_vrHandBlocked[size_t(hand)]=blocked||lengthOf(goal-proxy)>.01f;}}
void Game::followVrHands(){
 // A frame with no prop step near a hand: the drawn hand is where the tracked one is.
 for(int hand=0;hand<2;++hand){if(!vrHandFree(hand)){m_vrHandProxySet[size_t(hand)]=false;m_vrHandBlocked[size_t(hand)]=false;continue;}
  if(!m_vrHandStepped){m_vrHandProxy[size_t(hand)]=m_vrHandPosition[size_t(hand)];m_vrHandProxySet[size_t(hand)]=true;m_vrHandBlocked[size_t(hand)]=false;}}
 m_vrHandStepped=false;}
// --vr-test: held props in VR from scripted hand poses (no headset).
//  - One hand holds a crate; the hand turns 90 degrees about each axis in 10 degree steps: the crate's own axes turn
//    exactly as the hand's (within 2 degrees) and its middle stays where it was in the hand (1 cm).
//  - Two hands hold it; both turn together 90 degrees about each axis round the crate's middle: the same.
//  - With map (VrRuntime: tracking space into game space, the mirror), the poses are authored in tracking space and
//    mapped, and the crate must turn as the mirror of the tracking turn (M R M^T), not by R itself.
//  - Grip down takes, grip up drops; the trigger punts at the punt's speed along the hand; a drawn gun's trigger does not.
bool Game::testVrHeldProps(std::ostream& report,Point3(*map)(Point3)){
 using namespace vectra;bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 const char* space=map?" (tracking poses through the mirror)":"";
 auto toGame=[&](V3 v){if(!map)return v;const Point3 p=map({v.x,v.y,v.z});return V3{p.x,p.y,p.z};};
 // map is orthogonal: its inverse is its transpose.
 auto toSource=[&](V3 v){if(!map)return v;const V3 cx=toGame({1,0,0}),cy=toGame({0,1,0}),cz=toGame({0,0,1});return V3{dot(cx,v),dot(cy,v),dot(cz,v)};};
 const V3 f0=map?V3{0,0,-1}:V3{1,0,0},u0=map?V3{0,1,0}:V3{0,0,1};
 auto pt=[](V3 v){return Point3{v.x,v.y,v.z};};
 auto degrees=[](V3 a,V3 b){return std::acos(std::clamp(dot(norm(a),norm(b)),-1.f,1.f))*57.2958f;};
 auto scene=[&](){auto g=validationScene(Enemy::Kind::Huntsman);g.m_enemies.clear();Clutter c;c.kind=7;c.model=model::wooden_crate;c.dims={.5f,.4f,.36f};c.pos={4.3f,4.5f};c.z=1.f;
  g.m_clutter={c};g.m_player.pos={3.2f,4.5f};g.m_player.angle=0;return g;};
 const char* axisName[3]={"x","y","z"};
 for(int hands=1;hands<=2;++hands)for(int k=0;k<3;++k){
  auto g=scene();const V3 centre=propCentre(g.m_clutter[0]);const float dt=1.f/90;InputState in{};in.vrTracked=true;
  // Hand places about the middle, in the source space: one hand just behind it; two at its two ends (along source y).
  const V3 side=norm(toSource({0,1,0}));
  std::array<V3,2> at{};if(hands==1)at[1]=toSource(V3{-.12f,0,0});else{at[1]=side*-.27f;at[0]=side*.27f;}
  auto pose=[&](const Quat& r){for(int h=0;h<2;++h){const bool used=hands==2||h==1;
   g.setVrHand(h,pt(centre+toGame(r.rotate(at[size_t(h)]))),pt(toGame(r.rotate(f0))),used,{},pt(toGame(r.rotate(u0))));}};
  pose(Quat{});in.vrGrip[1]=true;g.updateInteraction(in,dt);g.updateClutter(in,dt);
  if(hands==2){in.vrGrip[0]=true;g.updateInteraction(in,dt);g.updateClutter(in,dt);}
  const bool took=g.holdingClutter()&&(hands==1||g.m_vrSecondHand==0);
  const Quat q0=propOrientation(g.m_clutter[0]);const V3 c0=propCentre(g.m_clutter[0]);
  V3 axis{};(&axis.x)[k]=1;float worst=0,drift=0;
  for(int step=1;step<=9;++step){const Quat r=Quat::axisAngle(axis,float(step)*10.f*kPi/180);pose(r);g.updateInteraction(in,dt);g.updateClutter(in,dt);if(!g.holdingClutter())break;
   const Quat q=propOrientation(g.m_clutter[0]);
   for(V3 local:{V3{1,0,0},V3{0,1,0},V3{0,0,1}}){const V3 want=toGame(r.rotate(toSource(q0.rotate(local))));worst=std::max(worst,degrees(q.rotate(local),want));}
   const V3 wantCentre=centre+toGame(r.rotate(toSource(c0-centre)));drift=std::max(drift,len(propCentre(g.m_clutter[0])-wantCentre));}
  std::ostringstream what;what<<(hands==1?"one hand":"two hands")<<" turned 90 degrees about "<<axisName[k]<<space<<": the crate turns with "<<(hands==1?"it":"them")<<" (worst "<<worst<<" deg, middle off "<<drift*100<<" cm"<<(took?"":", not taken")<<(g.holdingClutter()?"":", dropped")<<")";
  check(took&&g.holdingClutter()&&worst<2.f&&drift<.01f,what.str());}
 if(map)return ok;
 // Grip down takes, grip up drops (no toggle: the same hand opening lets go; a press with no prop near takes nothing).
 {auto g=scene();InputState in{};in.vrTracked=true;const V3 c=propCentre(g.m_clutter[0]);g.setVrHand(0,pt(c+V3{-.1f,0,0}),{1,0,0},true);g.setVrHand(1,{3.2f,3.f,1.f},{1,0,0},true);
  in.vrGrip[1]=true;in.use=true;g.updateInteraction(in,.01f);g.updateInteraction(in,.01f);const bool farHand=!g.holdingClutter();
  in.vrGrip[1]=false;g.updateInteraction(in,.01f);in.vrGrip[0]=true;g.updateInteraction(in,.01f);g.updateClutter(in,.01f);const bool taken=g.holdingClutter()&&g.m_vrCarryHand==0;
  g.updateInteraction(in,.01f);g.updateClutter(in,.01f);const bool kept=g.holdingClutter();
  in.vrGrip[0]=false;in.use=false;g.updateInteraction(in,.01f);const bool dropped=!g.holdingClutter();
  g.updateInteraction(in,.01f);g.updateClutter(in,.01f);const bool stays=!g.holdingClutter();
  check(farHand&&taken&&kept&&dropped&&stays,"grip down takes the crate, grip held keeps it, grip up drops it and it stays dropped (a far hand's grip takes nothing)");}
 // The trigger punts: the holding hand's, or the empty gun hand's; along the holding hand's pointing at the punt speed.
 for(int which=0;which<3;++which){auto g=scene();InputState in{};in.vrTracked=true;const V3 c=propCentre(g.m_clutter[0]);const V3 dir=norm({.8f,.3f,.2f});
  g.setVrHand(0,pt(c+V3{-.1f,0,0}),pt(dir),true);g.setVrHand(1,{3.4f,4.2f,1.1f},{1,0,0},true);g.equipWeapon(WeaponId::Fists);
  in.vrGrip[0]=true;g.updateInteraction(in,.01f);g.updateClutter(in,1.f/240);const bool taken=g.holdingClutter();const float mass=g.m_clutter[0].mass();
  if(which==2){g.m_weaponEquipped=true;g.m_equippedWeapon=WeaponId::Handgun;g.m_holster=0;g.m_switchTarget=WeaponId::Fists;}
  in.vrTrigger[which==0?0:1]=true;g.updateInteraction(in,1.f/240);g.updateClutter(in,1.f/240);const auto& p=g.m_clutter[0];
  const V3 v{p.velocity.x,p.velocity.y,p.vz};const float want=PuntSpeed*std::clamp(std::sqrt(2.5f/mass),.35f,1.f),got=len(v);
  std::ostringstream what;
  if(which<2){what<<(which==0?"the holding hand's trigger":"the empty gun hand's trigger")<<" punts the "<<mass<<" kg crate along the hand at "<<got<<" m/s (want "<<want<<", 3%)";
   check(taken&&!g.holdingClutter()&&std::fabs(got-want)<.03f*want&&degrees(v,dir)<3.f&&p.projectile,what.str());}
  else{what<<"with a gun drawn the gun hand's trigger fires and leaves the crate in the other hand";check(taken&&g.holdingClutter(),what.str());}}
 // A second press of the same trigger while held does not punt twice (the press, not the hold).
 {auto g=scene();InputState in{};in.vrTracked=true;const V3 c=propCentre(g.m_clutter[0]);g.setVrHand(0,pt(c+V3{-.1f,0,0}),{1,0,0},true);g.setVrHand(1,{}, {},false);
  in.vrTrigger[0]=true;in.vrGrip[0]=true;g.updateInteraction(in,.01f);g.updateClutter(in,.01f);
  check(g.holdingClutter(),"a trigger already held as the hand closes does not punt what it takes");}
 return ok;
}
}

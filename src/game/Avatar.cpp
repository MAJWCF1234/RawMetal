// The VR avatar's solve (Avatar.h, docs/Systems/avatar.md): a pure function of an AvatarPose and a time.
#include "Avatar.h"
#include "Game.h"
#include <cmath>
#include <cstring>
#include <ostream>
namespace retro {
namespace {
float len(Point3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
Point3 unit(Point3 v,Point3 fallback={1,0,0}){const float l=len(v);return l>1e-6f?v*(1/l):fallback;}
float dot(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float wrap(float a){while(a>kPi)a-=2*kPi;while(a<-kPi)a+=2*kPi;return a;}
Point3 ahead(float yaw){return {std::cos(yaw),std::sin(yaw),0};}
// Her left, as the body is drawn (Scene3D: side = (sin, -cos)): the map's angle grows clockwise.
Point3 leftOf(float yaw){return {std::sin(yaw),-std::cos(yaw),0};}
constexpr Point3 Up{0,0,1};
}
AvatarSolve solveAvatar(const AvatarPose& p,float time){
 AvatarSolve s;
 s.bodyForward=ahead(p.bodyYaw);s.bodyLeft=leftOf(p.bodyYaw);
 // The torso turns part of the way toward the head (the hips stay), and leans forward when the head is lower
 // than standing without a crouch (a stoop) and a little when looking down.
 s.twist=std::clamp(wrap(p.headYaw-p.bodyYaw),-.7f,.7f)*.5f;
 const float stoop=p.crouched||p.prone?0.f:std::clamp((Player::StandingEye-p.eye)/.45f,0.f,1.f);
 s.lean=stoop*.6f+std::max(0.f,-p.headPitch)*.25f;
 const float ty=p.bodyYaw+s.twist;const Point3 f=ahead(ty);
 s.torsoLeft=leftOf(ty);s.torsoForward=f*std::cos(s.lean)-Up*std::sin(s.lean);s.torsoUp=Up*std::cos(s.lean)+f*std::sin(s.lean);
 // The neck from the head (8 cm behind and 13 cm below the eyes, in the head's own frame), the shoulders under it.
 const float hp=std::clamp(p.headPitch,-1.4f,1.4f);const Point3 hf=ahead(p.headYaw)*std::cos(hp)+Up*std::sin(hp),hu=Up*std::cos(hp)-ahead(p.headYaw)*std::sin(hp);
 s.neck=p.head-hf*.08f-hu*.13f;s.shoulders=s.neck-s.torsoUp*.06f;
 s.shoulder[0]=s.shoulders+s.torsoLeft*AvatarShoulderHalf;s.shoulder[1]=s.shoulders-s.torsoLeft*AvatarShoulderHalf;
 // The arms: each wrist where its hand is, pulled in to the arm's reach; the frame squared.
 const int g=p.gunSide&1,o=1-g;
 for(int h=0;h<2;++h){const auto&hand=p.hands[size_t(h)];Point3 w=hand.tracked?hand.wrist:s.shoulder[size_t(h)]+s.torsoForward*.12f-s.torsoUp*.45f;
  const Point3 d=w-s.shoulder[size_t(h)];const float l=len(d);s.reached[size_t(h)]=hand.tracked&&l<=AvatarArmReach;
  if(l>AvatarArmReach)w=s.shoulder[size_t(h)]+d*(AvatarArmReach/l);
  const Point3 fw=unit(hand.tracked?hand.forward:s.torsoForward*.3f-s.torsoUp);Point3 pa=hand.tracked?hand.palm:(h?s.torsoLeft:s.torsoLeft*-1.f);pa=unit(pa-fw*dot(pa,fw),s.torsoLeft);
  s.wrist[size_t(h)]=w;s.forward[size_t(h)]=fw;s.palm[size_t(h)]=pa;s.curls[size_t(h)]=hand.curls;}
 // The fingers on what they hold: the gun hand round its grip (the index on the trigger as far as the player's
 // own index is), the other hand round the fore-end.
 if(p.inHand==3||p.inHand==4)s.curls[size_t(g)]={.35f,std::clamp(p.hands[size_t(g)].curls[1],.25f,.5f),.85f,.88f,.9f};
 else if(p.inHand==2)s.curls[size_t(g)]={.55f,std::clamp(p.hands[size_t(g)].curls[1],.3f,.55f),.85f,.95f,.85f};
 if(p.support&&p.inHand==2)s.curls[size_t(o)]={.45f,.75f,.85f,.9f,.9f};
 if(p.support&&(p.inHand==3||p.inHand==4))s.curls[size_t(o)]={.5f,.8f,.85f,.9f,.9f};   // round the gun hand's fingers
 // A reload: the other hand goes down to the shells (or the magazine) at its hip and up to the gun, three times
 // for the shotgun's shells, once for a magazine.
 if(p.reload>0&&p.inHand){const int cycles=p.inHand==2?3:1;const float t=std::fmod(p.reload*float(cycles),1.f),u=t<.35f?0.f:t<.75f?(t-.35f)/.4f:1.f,ease=u*u*(3-2*u);
  const Point3 offSide=o==0?s.bodyLeft:s.bodyLeft*-1.f;const Point3 hip=p.root+Up*(p.eye*.62f)+offSide*.17f+s.bodyForward*.04f;
  const Point3 port=s.wrist[size_t(g)]+s.torsoForward*.11f-Up*.04f+(offSide*-1.f)*.02f;
  s.wrist[size_t(o)]=hip+(port-hip)*ease;s.forward[size_t(o)]=unit(s.torsoForward+offSide*-.4f);s.palm[size_t(o)]=unit(Up-s.forward[size_t(o)]*dot(Up,s.forward[size_t(o)]),s.torsoLeft);
  s.curls[size_t(o)]={.5f,.6f,.6f,.65f,.65f};s.reached[size_t(o)]=true;}
 // The legs, as the desktop body chooses them (Scene3D): 0 idle, 1-4 run, 5 sprint, 6 kneel, 7-10 crouch-walk,
 // 11 falling, 13 walk, 14/15 prone, 16-18 walk back/left/right, 19 crawl back; directions against the hips.
 const float speed=length(p.velocity);const Vec2 bf{s.bodyForward.x,s.bodyForward.y},right{-s.bodyLeft.x,-s.bodyLeft.y};
 const float aheadV=dot(p.velocity,bf),acrossV=dot(p.velocity,right),gait=std::fmod(std::fmod(p.gait/(2*kPi),1.f)+1.f,1.f);
 const int direction=std::fabs(aheadV)>=std::fabs(acrossV)?(aheadV>=0?0:1):(acrossV>=0?3:2);
 int clip=0;float phase=std::fmod(time/2.13f,1.f);
 if(!p.grounded){clip=11;phase=std::fmod(time,1.f);}
 else if(p.prone){if(speed<.2f)clip=14;else{clip=direction==1?19:15;phase=gait;}}
 else if(p.crouched){if(speed<.25f)clip=6;else{clip=7+direction;phase=gait;}}
 else if(speed>=.25f){phase=gait;clip=speed<2.f?(direction==0?13:15+direction):1+direction;}
 s.legsClip=clip;s.legsPhase=phase<0?phase+1:phase;
 // The holsters: the long gun across the back between the shoulder blades, the pistol at the gun hand's hip.
 s.backSlot=s.neck-s.torsoForward*.15f-s.torsoUp*.24f;
 s.hipSlot=p.root+Up*(p.eye*.62f)+(g?s.bodyLeft*-1.f:s.bodyLeft)*.19f+s.bodyForward*.02f;
 // The pouch: the belt on the other side, a little ahead of the hip (where the support hand drops to reload).
 s.pouchSlot=p.root+Up*(p.eye*.62f)+(g?s.bodyLeft:s.bodyLeft*-1.f)*.17f+s.bodyForward*.06f;
 return s;
}
int avatarSlotAt(const AvatarPose& p,Point3 hand){
 const AvatarSolve s=solveAvatar(p,0);
 // Over a shoulder, behind the head (the shoulder slot's old box, turned with the hips).
 const Point3 o=hand-p.head;const float behind=o.x*s.bodyForward.x+o.y*s.bodyForward.y,side=o.x*s.bodyLeft.x+o.y*s.bodyLeft.y;
 if(behind<-.1f&&behind>-.65f&&std::fabs(side)<.55f&&o.z>-.45f&&o.z<.3f)return 1;
 if(len(hand-s.hipSlot)<.2f)return 2;
 return 0;
}
bool avatarPouchAt(const AvatarPose& p,Point3 wrist){return len(wrist-solveAvatar(p,0).pouchSlot)<.2f;}
void AvatarFit::observeHead(float y,bool headsetWorn,bool seated,float dt){
 if(!std::isfinite(y)||!std::isfinite(dt))return;
 if(!headsetWorn){worn=false;riseTime=0;return;}
 if(!worn){worn=true;eye=y;reach=0;riseTime=0;stretchTime={};return;}   // put on (again): whoever wears it now
 if(eye<=0){eye=y;return;}
 // Seated, the head is meant to stay low: standing up for a moment must not move her eye.
 if(seated||y<=eye+Rise){riseTime=0;return;}
 riseLow=riseTime>0?std::min(riseLow,y):y;riseTime+=dt;
 if(riseTime>=RiseHold){eye=riseLow;riseTime=0;reach=0;stretchTime={};}   // a new standing eye: the reach is read again against it
}
void AvatarFit::observeReach(int hand,float distance,float scale,float dt){
 const size_t h=size_t(hand&1);if(!std::isfinite(distance)||!(scale>0)||!std::isfinite(dt))return;
 const float now=std::max(reach,AvatarArmReach/scale),most=AvatarArmReach*AvatarReachMost/scale;
 // Past the most she can be fitted to (or a hand a metre and a half off: lost tracking) teaches nothing more.
 if(distance<=now+.004f||distance>1.5f){stretchTime[h]=0;return;}
 stretchLow[h]=stretchTime[h]>0?std::min(stretchLow[h],distance):distance;stretchTime[h]+=dt;
 if(stretchTime[h]>=StretchHold){reach=std::min(stretchLow[h],most);stretchTime[h]=0;}
}
float avatarEyeForHeight(int heightCm){return float(heightCm)*.00933f;}
float avatarArmFit(const AvatarFit& fit,float scale,int setting){
 if(setting==1)return 1.1f;if(setting==2)return 1.f;if(setting==3)return .87f;
 return AvatarArmReach/std::clamp(fit.reach*scale,AvatarArmReach,AvatarArmReach*AvatarReachMost);
}
bool testAvatarFit(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const char* what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 constexpr float dt=1.f/90;auto hold=[&](AvatarFit& f,float y,float seconds,bool worn=true,bool seated=false){for(float t=0;t<seconds;t+=dt)f.observeHead(y,worn,seated,dt);};
 {AvatarFit f;hold(f,.82f,2.f,false);check(f.eye==0,"a headset on the desk, not worn: nothing is learned");
  hold(f,.82f,.5f);check(std::fabs(f.eye-.82f)<1e-5f,"put on at the desk: the first worn sample seeds the eye");
  hold(f,1.64f,.5f);check(std::fabs(f.eye-.82f)<1e-5f,"stood up half a second: not yet");
  hold(f,1.64f,.7f);check(std::fabs(f.eye-1.64f)<1e-5f,"held a second higher: the eye rises to the real standing eye");
  hold(f,1.1f,5.f);check(std::fabs(f.eye-1.64f)<1e-5f,"ducking for five seconds does not lower it");
  {AvatarFit g=f;for(int i=0;i<200;++i)g.observeHead(i%3==0?1.75f:1.64f,true,false,dt);check(std::fabs(g.eye-1.64f)<1e-5f,"a head bobbing up for single frames does not raise it");}
  hold(f,1.64f,.1f,false);hold(f,1.2f,.1f);check(std::fabs(f.eye-1.2f)<1e-5f&&f.reach==0,"taken off and put on again: it starts over");}
 {AvatarFit f;hold(f,1.40f,.2f);hold(f,1.5f,.3f);hold(f,1.47f,.9f);check(std::fabs(f.eye-1.47f)<1e-5f,"a stooped start rises to the lowest height held, not the highest");}
 {AvatarFit f;hold(f,1.1f,.2f,true,true);hold(f,1.7f,3.f,true,true);check(std::fabs(f.eye-1.1f)<1e-5f,"seated: standing up does not move it");}
 {AvatarFit f;f.eye=1.64f;f.worn=true;const float scale=1.42f/1.64f;
  check(avatarArmFit(f,scale,0)==1.f,"unfitted: the hands as tracked");
  for(float t=0;t<1;t+=dt)f.observeReach(1,.30f,scale,dt);check(f.reach==0,"a hand inside her reach teaches nothing");
  f.observeReach(1,1.2f,scale,dt);for(float t=0;t<.5f;t+=dt)f.observeReach(1,.5f,scale,dt);check(f.reach==0,"a one-frame tracking jump teaches nothing");
  for(float t=0;t<.3f;t+=dt)f.observeReach(1,.70f,scale,dt);check(std::fabs(f.reach-.70f)<1e-4f,"a long reach held a fifth of a second is learned");
  check(std::fabs(.70f*scale*avatarArmFit(f,scale,0)-AvatarArmReach)<1e-4f,"the player's full stretch, scaled to her height and fitted, is exactly her arm");
  for(float t=0;t<.3f;t+=dt)f.observeReach(0,1.3f,scale,dt);check(std::fabs(avatarArmFit(f,scale,0)-1/AvatarReachMost)<1e-4f,"no reach is drawn in by more than AvatarReachMost");
  check(avatarArmFit(f,scale,1)>1&&avatarArmFit(f,scale,2)==1&&avatarArmFit(f,scale,3)<1,"ARM LENGTH short, average and long override the fit");
  f.recalibrate(1.6f);check(f.reach==0&&f.eye==1.6f,"RECALIBRATE HEIGHT starts the reach over");}
 check(std::fabs(avatarEyeForHeight(175)-1.633f)<.005f,"175 cm tall: the eyes at about 1.63 m");
 // Her body stays her size: a fitted wrist drawn in toward the shoulder is in her reach and the solve leaves it there.
 {AvatarPose p;p.active=true;p.head={0,0,1.42f};p.root={};p.headYaw=p.bodyYaw=0;p.hands[1].tracked=true;p.hands[1].wrist={.5f,.1f,1.2f};p.hands[1].forward={1,0,0};p.hands[1].palm={0,1,0};
  const AvatarSolve raw=solveAvatar(p,0);const Point3 sh=raw.shoulder[1];const float d=len(p.hands[1].wrist-sh);
  p.hands[1].wrist=avatarFitWrist(sh,p.hands[1].wrist,AvatarArmReach/d);const AvatarSolve fitted=solveAvatar(p,0);
  check(!raw.reached[1]&&fitted.reached[1]&&len(fitted.wrist[1]-p.hands[1].wrist)<1e-4f&&len(fitted.shoulder[1]-sh)<1e-6f,"a reach past her arm, fitted, is her full stretch; her shoulders do not move");}
 return ok;
}
bool testAvatar(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const char* what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto near=[](Point3 a,Point3 b,float e=1e-4f){return len(a-b)<e;};
 auto det=[](Point3 a,Point3 b,Point3 c){return a.x*(b.y*c.z-b.z*c.y)-a.y*(b.x*c.z-b.z*c.x)+a.z*(b.x*c.y-b.y*c.x);};
 AvatarPose p;p.active=true;p.head={5,5,1.42f};p.root={5,5,0};p.eye=1.42f;p.headYaw=p.bodyYaw=.4f;
 for(int h=0;h<2;++h){auto&hand=p.hands[size_t(h)];hand.tracked=true;hand.wrist=p.head+ahead(.4f)*.35f+leftOf(.4f)*(h?-.2f:.2f)-Up*.35f;hand.forward=ahead(.4f);hand.palm=h?leftOf(.4f):leftOf(.4f)*-1.f;hand.curls={.2f,.3f,.4f,.5f,.6f};}
 const AvatarSolve s=solveAvatar(p,1.25f);
 // The torso frame is orthonormal and maps onto the rig's axes.
 check(std::fabs(len(s.torsoForward)-1)<1e-4f&&std::fabs(len(s.torsoUp)-1)<1e-4f&&std::fabs(dot(s.torsoForward,s.torsoUp))<1e-4f&&std::fabs(dot(s.torsoLeft,s.torsoUp))<1e-4f,"torso frame orthonormal");
 check(near(s.rig(s.torsoLeft),{1,0,0})&&near(s.rig(s.torsoUp),{0,1,0})&&near(s.rig(s.torsoForward),{0,0,1}),"rig axes: x her left, y up, z ahead");
 // The mirror chain: a frame built in tracking space (right-handed), mapped to game space as VrRuntime maps it
 // (rotateTracking, determinant -1), then into the rig, is the same frame as tracking space turned straight into
 // the rig (x right -> -x, y up, -z ahead -> z): a proper rotation, so a hand is never drawn inside out.
 {const float yaw=.4f,c=std::cos(yaw),sn=std::sin(yaw);auto toGame=[&](Point3 v){return Point3{-v.z*c-v.x*sn,-v.z*sn+v.x*c,v.y};};
  AvatarPose q=p;q.headYaw=q.bodyYaw=yaw;q.headPitch=0;const AvatarSolve r=solveAvatar(q,0);bool same=true,proper=true;
  for(int k=0;k<24;++k){const float a=.37f*k,b=.61f*k;Point3 x=unit({std::cos(a)*std::cos(b),std::sin(b),std::sin(a)*std::cos(b)}),y=unit(Point3{-std::sin(a),.3f*std::cos(b),std::cos(a)});y=unit(y-x*dot(x,y));
   const Point3 z{x.y*y.z-x.z*y.y,x.z*y.x-x.x*y.z,x.x*y.y-x.y*y.x};
   const Point3 rx=r.rig(toGame(x)),ry=r.rig(toGame(y)),rz=r.rig(toGame(z));
   same&=near(rx,{-x.x,x.y,-x.z},1e-4f)&&near(ry,{-y.x,y.y,-y.z},1e-4f);proper&=det(rx,ry,rz)>.999f;}
  check(same&&proper,"tracking frame -> game (mirror) -> rig (mirror) is a proper rotation of the tracking frame");}
 // Reach: a wrist within reach stays, one past it is pulled in along the arm.
 check(s.reached[0]&&s.reached[1]&&near(s.wrist[0],p.hands[0].wrist)&&near(s.wrist[1],p.hands[1].wrist),"wrists within reach are where the hands are");
 {AvatarPose q=p;q.hands[1].wrist=p.head+ahead(.4f)*1.4f;const AvatarSolve r=solveAvatar(q,0);
  check(!r.reached[1]&&std::fabs(len(r.wrist[1]-r.shoulder[1])-AvatarArmReach)<1e-4f,"a hand past the reach is held at the arm's length");}
 // The shoulders sit under the head, her right on her right.
 check(dot(s.shoulder[0]-s.shoulder[1],s.torsoLeft)>.3f&&s.shoulders.z<p.head.z-.12f&&s.shoulders.z>p.head.z-.3f,"shoulders under the head, left on the left");
 // Purity: the same pose and time give the same solve.
 {const AvatarSolve a=solveAvatar(p,2.5f),b=solveAvatar(p,2.5f);check(std::memcmp(&a,&b,sizeof a)==0,"the solve is a pure function of the pose and the time");}
 // Legs.
 {AvatarPose q=p;q.velocity={std::cos(.4f)*1.2f,std::sin(.4f)*1.2f};check(solveAvatar(q,0).legsClip==13,"walking ahead: the walk");
  q.velocity={-std::cos(.4f)*3.f,-std::sin(.4f)*3.f};check(solveAvatar(q,0).legsClip==2,"running back: the back-run");
  q.velocity={};q.crouched=true;check(solveAvatar(q,0).legsClip==6,"crouched still: the kneel");
  q.crouched=false;q.grounded=false;check(solveAvatar(q,0).legsClip==11,"in the air: falling");}
 // Stoop and twist.
 {AvatarPose q=p;q.eye=1.f;check(solveAvatar(q,0).lean>.5f,"a lowered head without a crouch leans the torso");
  q=p;q.headYaw=p.bodyYaw+2.f;const auto r=solveAvatar(q,0);check(r.twist>.3f&&r.twist<=.35001f,"the torso turns part way toward the head, no further than 20 degrees");}
 // Slots.
 {const Point3 overRight=p.head-ahead(.4f)*.25f-leftOf(.4f)*.15f+Up*.05f,overLeft=p.head-ahead(.4f)*.25f+leftOf(.4f)*.15f;
  check(avatarSlotAt(p,overRight)==1&&avatarSlotAt(p,overLeft)==1,"over either shoulder: the back");
  check(avatarSlotAt(p,s.hipSlot+Up*.05f)==2&&avatarSlotAt(p,p.hands[1].wrist)==0,"at the right hip: the hip; ahead: neither");
  AvatarPose lefty=p;lefty.gunSide=0;const auto r=solveAvatar(lefty,0);check(dot(r.hipSlot-p.root,r.bodyLeft)>.15f&&avatarSlotAt(lefty,s.hipSlot)==0,"left-handed: the hip slot is on her left");
  check(avatarPouchAt(p,s.pouchSlot+Up*.04f)&&dot(s.pouchSlot-p.root,s.bodyLeft)>.15f&&!avatarPouchAt(p,s.hipSlot)&&avatarSlotAt(p,s.pouchSlot)==0&&avatarPouchAt(lefty,r.pouchSlot)&&dot(r.pouchSlot-p.root,r.bodyLeft)<-.15f,"the belt pouch is on the support hand's hip, apart from the holster (and swaps left-handed)");}
 // Fingers on what they hold.
 {AvatarPose q=p;q.inHand=3;check(solveAvatar(q,0).curls[1][2]>=.85f&&solveAvatar(q,0).curls[0][2]==p.hands[0].curls[2],"the pistol closes the gun hand only");
  q.inHand=2;q.support=true;const auto r=solveAvatar(q,0);check(r.curls[0][1]>=.75f,"the support hand wraps the fore-end");
  {AvatarPose two=p;two.inHand=3;two.support=true;check(solveAvatar(two,0).curls[0][1]>=.75f,"two-handed on the pistol, the support hand closes round the gun hand");}
  q.reload=.5f;const auto u=solveAvatar(q,0);check(len(u.wrist[0]-r.wrist[0])>.05f,"a reload takes the support hand off the fore-end");}
 return ok;
}
}

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
 if(p.inHand==3)s.curls[size_t(g)]={.35f,std::clamp(p.hands[size_t(g)].curls[1],.25f,.5f),.85f,.88f,.9f};
 else if(p.inHand==2)s.curls[size_t(g)]={.55f,std::clamp(p.hands[size_t(g)].curls[1],.3f,.55f),.85f,.95f,.85f};
 if(p.support&&p.inHand==2)s.curls[size_t(o)]={.45f,.75f,.85f,.9f,.9f};
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
  AvatarPose lefty=p;lefty.gunSide=0;const auto r=solveAvatar(lefty,0);check(dot(r.hipSlot-p.root,r.bodyLeft)>.15f&&avatarSlotAt(lefty,s.hipSlot)==0,"left-handed: the hip slot is on her left");}
 // Fingers on what they hold.
 {AvatarPose q=p;q.inHand=3;check(solveAvatar(q,0).curls[1][2]>=.85f&&solveAvatar(q,0).curls[0][2]==p.hands[0].curls[2],"the pistol closes the gun hand only");
  q.inHand=2;q.support=true;const auto r=solveAvatar(q,0);check(r.curls[0][1]>=.75f,"the support hand wraps the fore-end");
  q.reload=.5f;const auto u=solveAvatar(q,0);check(len(u.wrist[0]-r.wrist[0])>.05f,"a reload takes the support hand off the fore-end");}
 return ok;
}
}

#pragma once
// The skinbot walking. No walk is authored: each foot stays where it was put until the body has carried its
// comfortable place too far off, a leg may only lift while the legs beside it stand, a step lands on whatever is under
// the place the foot will be wanted, and the body rides at the height and lean of the feet that carry it. The gait,
// the climbing and the turning on the spot all come out of those rules (after TheCymaera's minecraft-spider).
// World positions are metres, z up; the heading yaw is the fixtures' (0 faces +y, a quarter turn faces +x).
#include "SkinbotRig.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <functional>
#include <ostream>
#include <string>

namespace retro::skinbot {
struct W {float x=0,y=0,z=0;};
// The ground under x,y: the highest surface at or below `from`.
using Ground=std::function<float(float x,float y,float from)>;
using BodyClear=std::function<bool(W)>;
using FootClear=std::function<bool(W,W)>;

struct Foot {W at{},from{},to{};float progress=1;
 float time=.30f,lift=.34f;   // this step's own length and height; a slip is a step with no lift
 float hang=0,hangAt=2;       // a hesitation: the blade waits (hang s) in the air at this progress
 bool planted()const{return progress>=1;}};

// The curse: it is not used to being alive. All of it is drawn from one seeded stream, so a walk can be repeated.
struct Curse {
 bool on=true;std::uint32_t seed=0x5c1fb07u;
 float quiver=0,sag=0,sagSpeed=0,sway=0,swaySpeed=0;     // a tremble that dies away; a knee giving and its recovery
 float freeze=0,rush=0,nextRush=4.5f,sinceSlip=3,clock=0;
 int slips=0,hesitations=0,freezes=0;
 // The head: where it is turned (radians, from the body), how fast it is swinging, and its twitches.
 float headYaw=0,headPitch=0,headRoll=0,yawSpeed=0,pitchSpeed=0,rollSpeed=0,wantYaw=0,wantPitch=0,wantRoll=0,glance=1,twitch=2;
 float breath=0,breathPhase=0,breathHold=0;
 float next(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return float(seed%100000u)/100000.f;}
 float between(float a,float b){return a+(b-a)*next();}
};

class Walker {
public:
 static constexpr float Scale=2.4f/1.474f;          // 2.4 m to the top of its head
 static constexpr float Speed=1.16f,RushSpeed=1.85f,TurnRate=1.7f;
 static constexpr float StepTime=.25f,StepLift=.43f;
 static constexpr float ChargeSpeed=3.65f,ChargeStepTime=.105f,ChargeStepLift=.30f,ChargeTrigger=.22f,ChargeAcceleration=11.f;
 static constexpr float Trigger=.30f,Settle=.10f,Ahead=.48f;  // m off its place before it steps (walking, standing); lead, in steps
 static constexpr float Climb=.99f;                 // the tallest thing a foot is set up onto (m)
 Curse curse{};
 bool watching=false;W watched{};                   // something it has noticed: the head goes to it
 void watch(W at){watching=true;watched=at;}
 void lookAway(){watching=false;}
 float x=0,y=0,z=0,yaw=0,pitch=0,roll=0;            // z: the ground the body rides over
 std::array<Foot,LegCount> feet{};
 W goal{};bool going=false;                          // where it is pointed: the Director's laser
 float velocityX=0,velocityY=0,turning=0;

 void place(float px,float py,float heading,const Ground& ground,float from){
  recovery={};impactRush=false;
  x=px;y=py;yaw=heading;z=ground(px,py,from);pitch=roll=0;going=false;velocityX=velocityY=turning=0;{const bool on=curse.on;const auto seed=curse.seed;curse={};curse.on=on;curse.seed=seed;}watching=false;
  for(int leg=0;leg<LegCount;++leg){W h=home(leg);h.z=ground(h.x,h.y,z+Climb);feet[size_t(leg)]={h,h,h,1};}
 }
 void pointAt(float gx,float gy){goal={gx,gy,0};going=true;}
 bool arrived()const{return !going;}
 bool catching()const{return recovery.time>0;}
 bool charging()const{return impactRush&&curse.rush>0;}
 // A co-op guest draws the host's motion and never advances these timers locally.
 void setRemoteMotion(bool moving,bool windup,bool catchStep,bool rush,bool charge){
  going=moving;
  curse.freeze=windup?1.f:0.f;
  curse.rush=rush?1.f:0.f;
  recovery.time=catchStep?1.f:0.f;
  impactRush=charge;
 }
 void catchImpact(float dx,float dy,float strength,float rush){
  const float length=std::hypot(dx,dy);
  if(length<.001f)return;
  recovery={{dx/length,dy/length,0},{},strength,.65f,rush,-1};
  curse.freeze=curse.rush=0;impactRush=false;
 }

 // A model point of leg-free body, or of a posed leg, leaned as the body is (model space in, model space out).
 V lean(V p)const{
  const V c{0,.7f,0};V d=p-c;
  const float cp=std::cos(pitch),sp=std::sin(pitch);d={d.x,d.y*cp+d.z*sp,-d.y*sp+d.z*cp};   // nose up for a positive pitch
  const float rr=roll+shakeRoll(),cr=std::cos(rr),sr=std::sin(rr);d={d.x*cr-d.y*sr,d.x*sr+d.y*cr,d.z};   // right side up for a positive roll
  return c+d+V{0,shakeHeight(),0};
 }
 V unlean(V p)const{
  const V c{0,.7f,0};V d=p-V{0,shakeHeight(),0}-c;
  const float rr=roll+shakeRoll(),cr=std::cos(rr),sr=std::sin(rr);d={d.x*cr+d.y*sr,-d.x*sr+d.y*cr,d.z};
  const float cp=std::cos(pitch),sp=std::sin(pitch);d={d.x,d.y*cp-d.z*sp,d.y*sp+d.z*cp};
  return c+d;
 }
 float shakeRoll()const{return curse.sway+curse.quiver*(.025f*std::sin(curse.clock*47.f)+.012f*std::sin(curse.clock*83.f));}
 float shakeHeight()const{return curse.sag+curse.quiver*.009f*std::sin(curse.clock*61.f);}
 // The skin of the head, neck and chest: bent smoothly (nothing here is rigid), the eyes and teeth with the head.
 // Model space, before the body's lean. The shoulders are behind the bend and do not move.
 V skin(V p)const{
  const V neck{0,1.17f,.33f},axis{0,.5f,.866f};
  const V d=p-neck;const float along=d.y*axis.y+d.z*axis.z;
  float w=std::clamp((along+.02f)/.20f,0.f,1.f);w=w*w*(3-2*w);
  const V chest{0,.98f,.14f};const float apart=length(p-chest);
  if(apart<.42f){const float k=1-apart/.42f;p=p+(p-chest)*(curse.breath*k*k*(3-2*k));}
  if(w<=0)return p;
  V q=p-neck;
  const float a=curse.headRoll*w,ca=std::cos(a),sa=std::sin(a);                     // cocked to one side, about the head's own line
  {const V ax=axis;const float dot=q.x*ax.x+q.y*ax.y+q.z*ax.z;const V cr{ax.y*q.z-ax.z*q.y,ax.z*q.x-ax.x*q.z,ax.x*q.y-ax.y*q.x};q=q*ca+cr*sa+ax*(dot*(1-ca));}
  const float b=curse.headPitch*w,cb=std::cos(b),sb=std::sin(b);q={q.x,q.y*cb+q.z*sb,-q.y*sb+q.z*cb};   // up for a positive pitch
  const float t=curse.headYaw*w,ct=std::cos(t),st=std::sin(t);q={q.x*ct+q.z*st,q.y,-q.x*st+q.z*ct};     // toward +x for a positive turn
  return neck+q;
 }
 W toWorld(V m)const{const float c=std::cos(yaw),s=std::sin(yaw),mx=m.x*Scale,mz=m.z*Scale;return {x+mx*c+mz*s,y-mx*s+mz*c,z+m.y*Scale};}
 V toModel(W w)const{const float c=std::cos(yaw),s=std::sin(yaw),dx=w.x-x,dy=w.y-y;return {(dx*c-dy*s)/Scale,(w.z-z)/Scale,(dx*s+dy*c)/Scale};}
 Pose legPose(int leg)const{return solveSkinbotLeg(leg,unlean(toModel(feet[size_t(leg)].at)));}
 // Where leg's foot wants to be, standing: its modelled place under the body as it is.
 W home(int leg)const{const V m=joint(leg,3);W w=toWorld({m.x,0,m.z});return w;}

 void update(float dt,const Ground& ground,const BodyClear& clear={},const W* waypoint=nullptr,const FootClear& footClear={}){
  dt=std::min(dt,.05f);if(dt<=0)return;
  // The body: turn toward the goal, walk once roughly facing it, and only as fast as its feet can keep up.
  think(dt);
  const bool charge=charging();
  float wantSpeed=0,wantTurn=0;
  if(going&&!catching()){const float dx=goal.x-x,dy=goal.y-y,left=std::hypot(dx,dy);
   if(left<.25f)going=false;
   else{const W target=waypoint?*waypoint:goal;const float tx=target.x-x,ty=target.y-y;
    const float off=std::remainder(std::atan2(tx,ty)-yaw,6.2831853f);wantTurn=std::clamp(off*2.5f,-TurnRate,TurnRate);
    wantSpeed=Speed*std::clamp(1.2f-std::fabs(off),0.f,1.f)*std::clamp(std::hypot(tx,ty)/.8f,.3f,1.f);}}
  float strain=0;for(int leg=0;leg<LegCount;++leg)if(feet[size_t(leg)].planted()){const W h=home(leg);strain=std::max(strain,std::hypot(h.x-feet[size_t(leg)].at.x,h.y-feet[size_t(leg)].at.y));}
  const float keep=std::clamp((.75f-strain)/.3f,0.f,1.f);   // a foot far behind holds the body back until it has stepped
  turning+=std::clamp(wantTurn*keep-turning,-5*dt,5*dt);yaw=std::remainder(yaw+turning*dt,6.2831853f);
  if(curse.freeze>0){wantSpeed=0;wantTurn=0;velocityX=velocityY=turning=0;}          // dead still, all at once
  else if(curse.rush>0)wantSpeed*=(charge?ChargeSpeed:RushSpeed)/Speed;
  const float recoil=catching()?recovery.strength*.9f*std::max(0.f,(recovery.time-.27f)/.38f):0.f;
  const float vx=std::sin(yaw)*wantSpeed*keep+recoil*recovery.direction.x;
  const float vy=std::cos(yaw)*wantSpeed*keep+recoil*recovery.direction.y;
  const float acceleration=(charge?ChargeAcceleration:3.f)*dt;
  velocityX+=std::clamp(vx-velocityX,-acceleration,acceleration);velocityY+=std::clamp(vy-velocityY,-acceleration,acceleration);
  const W next{x+velocityX*dt,y+velocityY*dt,z};
  if(!clear||clear(next)){x=next.x;y=next.y;}
  else velocityX=velocityY=0;
  const bool moving=std::hypot(velocityX,velocityY)>.08f||std::fabs(turning)>.15f;
  // The feet: those in the air swing on; then the standing ones that want to move may lift, the worst off first.
  for(int leg=0;leg<LegCount;++leg){Foot& f=feet[size_t(leg)];if(f.planted())continue;
   if(curse.freeze>0)continue;                                      // held exactly where it was, mid step
   if(catching())f.hang=0;
   if(f.hang>0&&f.progress>=f.hangAt){f.hang-=dt;                   // it is not sure: the blade waits over the floor, shaking
    const float t=curse.clock*55.f+float(leg)*2.f;f.at.x+=std::sin(t)*.35f*dt;f.at.y+=std::cos(t*1.3f)*.35f*dt;f.at.z+=std::sin(t*.9f)*.25f*dt;
    if(f.hang>0)continue;}
   const float s=std::min(1.f,f.progress+dt/f.time),e=s*s*(3-2*s);
   const float over=std::max(f.from.z,f.to.z)+f.lift;
   const W nextFoot{f.from.x+(f.to.x-f.from.x)*e,f.from.y+(f.to.y-f.from.y)*e,
                    s<.5f?f.from.z+(over-f.from.z)*(1-(1-2*s)*(1-2*s)):f.to.z+(over-f.to.z)*(1-(2*s-1)*(2*s-1))};
   if(footClear&&!footClear(f.at,nextFoot)){
    // Something moved into a swing. Set the blade down on this side and take a new step from there.
    W land=f.at;land.z=ground(land.x,land.y,land.z+.03f);
    if(footClear(f.at,land)){f.from=f.at;f.to=land;f.progress=0;f.time=.16f;f.lift=f.hang=0;}
    continue;
   }
   f.progress=s;f.at=nextFoot;
   if(f.planted()){f.at=f.to;if(curse.on&&f.lift>0&&!catching())landed(leg,ground);}}
  const bool bracing=catching();
  if(bracing)recover(dt,ground,footClear);
  const float stepThreshold=moving?(charge?ChargeTrigger:Trigger):Settle;
  std::array<std::pair<float,int>,LegCount> wants{};
  for(int leg=0;leg<LegCount;++leg){const Foot& f=feet[size_t(leg)];const W h=home(leg);
   float off=std::hypot(h.x-f.at.x,h.y-f.at.y);if(!legPose(leg).reached)off+=.5f;
   wants[size_t(leg)]={f.planted()&&off>stepThreshold?off:0.f,leg};}
  std::sort(wants.begin(),wants.end(),[](const auto&a,const auto&b){return a.first>b.first;});
  for(const auto& [off,leg]:wants){if(bracing||off<=0||curse.freeze>0||!mayLift(leg,moving))continue;
   Foot& f=feet[size_t(leg)];f.from=f.at;f.hang=0;f.hangAt=2;
   f.time=charge?ChargeStepTime:StepTime;f.lift=charge?ChargeStepLift:StepLift;
   if(curse.on){f.time*=curse.between(.85f,1.19f)*(!charge&&curse.rush>0?.68f:1.f);f.lift*=curse.between(.9f,1.19f);}
   // Land where its place will be when the step is done, and a little further, so it is not left behind at once.
   const float lead=(charge?f.time:StepTime)*(1+Ahead);const float keepYaw=yaw,keepX=x,keepY=y;
   x+=velocityX*lead;y+=velocityY*lead;yaw+=turning*lead;W land=home(leg);x=keepX;y=keepY;yaw=keepYaw;
   land.z=ground(land.x,land.y,z+Climb);
   // Too high to set a foot on (a wall, a pillar): try short of it, back toward the body.
   for(int back=1;back<=3&&land.z>z+Climb-.02f;++back){const float k=1-.25f*back;land.x=x+(land.x-x)*k;land.y=y+(land.y-y)*k;land.z=ground(land.x,land.y,z+Climb);}
   if(footClear){
    for(int back=1;back<=3&&!footClear(f.at,land);++back){
     land.x=(land.x+f.at.x)*.5f;land.y=(land.y+f.at.y)*.5f;land.z=ground(land.x,land.y,z+Climb);
    }
    if(!footClear(f.at,land))continue;
   }
   // High enough to clear whatever lies across the step (a low wall between the two places).
   {float tallest=std::max(f.from.z,land.z);for(int k=1;k<8;++k){const float t=float(k)/8;tallest=std::max(tallest,ground(f.from.x+(land.x-f.from.x)*t,f.from.y+(land.y-f.from.y)*t,z+Climb));}
    f.lift+=std::max(0.f,tallest-std::max(f.from.z,land.z));}
   if(land.z>f.from.z+.14f)f.time*=.85f;
   else if(curse.on&&curse.rush<=0&&curse.next()<.055f){
    f.hang=curse.between(.07f,.15f);f.hangAt=curse.between(.35f,.55f);++curse.hesitations;
    curse.quiver=std::min(1.f,curse.quiver+.35f);
   }
   f.to=land;f.progress=0;}
  // The body rides its feet: their mean height, leaning with the front-to-back and side-to-side differences.
  auto h=[&](int leg){const Foot& f=feet[size_t(leg)];return f.planted()?f.at.z:f.to.z;};
  const float mean=(h(0)+h(1)+h(2)+h(3))*.25f;
  const float lengthways=std::fabs(joint(ArmL,3).z-joint(RearL,3).z)*Scale,across=std::fabs(joint(ArmR,3).x-joint(ArmL,3).x)*Scale;
  const float wantPitch=std::clamp(std::atan2((h(ArmL)+h(ArmR)-h(RearL)-h(RearR))*.5f,lengthways),-.45f,.45f);
  const float wantRoll=std::clamp(std::atan2((h(ArmR)+h(RearR)-h(ArmL)-h(RearL))*.5f,across),-.3f,.3f);
  const float ease=1-std::exp(-dt*(mean>z?8.f:6.f));
  z+=(mean-z)*ease;pitch+=(wantPitch-pitch)*ease;roll+=(wantRoll-roll)*ease;
 }
 int airborne()const{int n=0;for(const auto& f:feet)n+=!f.planted();return n;}
 static bool beside(int a,int b){   // front pair, back pair, and each side's front and back
  auto pair=[&](int p,int q){return (a==p&&b==q)||(a==q&&b==p);};
  return pair(ArmL,ArmR)||pair(RearL,RearR)||pair(ArmL,RearL)||pair(ArmR,RearR);
 }
private:
 struct Recovery {
  W direction{},landing{};
  float strength=0,time=0,rush=0;
  int leg=-1;
 } recovery;
 bool impactRush=false;

 // Distance to the edges of the convex support area. Negative means the weight is outside it.
 float supportMargin(W weight,int shifted=-1,W landing={})const{
  std::array<W,LegCount> contact{};
  for(int i=0;i<LegCount;++i)contact[size_t(i)]=i==shifted?landing:feet[size_t(i)].at;
  float margin=1e6f;
  for(int i=0;i<LegCount;++i)for(int j=i+1;j<LegCount;++j){
   const auto a=contact[size_t(i)],b=contact[size_t(j)];
   const float dx=b.x-a.x,dy=b.y-a.y,span=std::hypot(dx,dy);
   if(span<.001f)continue;
   const auto side=[&](W p){return dx*(p.y-a.y)-dy*(p.x-a.x);};
   bool left=false,right=false;
   for(const auto p:contact){const float sign=side(p);left|=sign>.001f;right|=sign<-.001f;}
   if(left!=right)margin=std::min(margin,(left?1.f:-1.f)*side(weight)/span);
  }
  return margin;
 }

 void recover(float dt,const Ground& ground,const FootClear& clear){
  // Let the other moving blades finish, then keep three planted while one catches the load.
  recovery.time=std::max(.001f,recovery.time-dt);
  if(airborne()!=0){
   // A prop can trap a blade mid-swing. Let the normal gait resume without a charge.
   if(recovery.time<=.001f)recovery={};
   return;
  }
  if(recovery.leg>=0){
   const auto p=feet[size_t(recovery.leg)].at;
   if(std::hypot(p.x-recovery.landing.x,p.y-recovery.landing.y)<.05f&&std::fabs(p.z-recovery.landing.z)<.05f){
    curse.rush=recovery.rush;impactRush=true;recovery={};return;
   }
   recovery.leg=-1;
  }

  const auto dir=recovery.direction;
  const float reach=.95f+.85f*recovery.strength;
  const W weight{x+dir.x*reach,y+dir.y*reach,z};
  const float margin=supportMargin(weight);
  if(margin>=.04f||recovery.time<=.001f){
   // If it never found a safe catch, it drops the charge and goes back to its usual gait.
   if(margin>=.04f){curse.rush=recovery.rush;impactRush=true;}
   recovery={};return;
  }
  if(recovery.time<.19f)return;
  float best=margin+.025f;
  for(int leg=0;leg<LegCount;++leg){
   const W start=feet[size_t(leg)].at;
   for(const float stride:{.46f,.32f,.19f})for(const float side:{0.f,-.24f,.24f}){
    W land{start.x+dir.x*stride-dir.y*side,start.y+dir.y*stride+dir.x*side,0};
    land.z=ground(land.x,land.y,z+.46f);
    if(std::fabs(land.z-start.z)>.46f||!solveSkinbotLeg(leg,unlean(toModel(land))).reached)continue;
    const float score=supportMargin(weight,leg,land)-.1f*std::fabs(side);
    if(score<=best)continue;
    if(clear&&!clear(start,land))continue;
    best=score;recovery.leg=leg;recovery.landing=land;
   }
  }
  if(recovery.leg<0)return;
  auto& foot=feet[size_t(recovery.leg)];
  foot.from=foot.at;foot.to=recovery.landing;foot.progress=0;foot.time=.18f;foot.lift=.23f;foot.hang=0;foot.hangAt=2;
 }

 // A blade just set down: the weight comes onto it and it trembles; now and then it does not hold.
 void landed(int leg,const Ground& ground){
  curse.quiver=std::min(1.f,curse.quiver+curse.between(.08f,.3f));
  if(charging()||curse.sinceSlip<3.5f||curse.next()>.09f)return;
  for(int other=0;other<LegCount;++other)if(other!=leg&&!feet[size_t(other)].planted()&&beside(leg,other))return;
  if(airborne()>=1)return;
  // It skids out and back from where it was set: a step along the ground, no lift.
  Foot& f=feet[size_t(leg)];const float side=(leg==ArmR||leg==RearR)?1.f:-1.f,c=std::cos(yaw),s=std::sin(yaw),out=curse.between(.18f,.34f),back=curse.between(.05f,.22f);
  W to{f.at.x+side*c*out-s*back,f.at.y-side*s*out-c*back,0};to.z=ground(to.x,to.y,f.at.z+.05f);
  if(std::fabs(to.z-f.at.z)>.2f)return;                           // not off an edge
  f.from=f.at;f.to=to;f.progress=0;f.time=.17f;f.lift=0;f.hang=0;f.hangAt=2;
  curse.sinceSlip=0;++curse.slips;curse.quiver=1;
  curse.sagSpeed=-1.1f;curse.swaySpeed=-side*1.3f;                // the body drops that way, catches itself, overcorrects
  curse.yawSpeed+=side*2.5f;curse.rollSpeed-=side*3.f;
 }
 // The head and the nerves.
 void think(float dt){
  Curse& c=curse;c.clock+=dt;c.sinceSlip+=dt;
  if(!c.on){c.quiver=c.sag=c.sway=c.headRoll=0;c.freeze=c.rush=0;}
  c.quiver*=std::exp(-dt*3.2f);
  auto spring=[dt](float& at,float& speed,float want,float stiff,float damp){speed+=((want-at)*stiff-speed*damp)*dt;at+=speed*dt;};
  spring(c.sag,c.sagSpeed,0,90,7);spring(c.sway,c.swaySpeed,0,55,5);
  c.sag=std::clamp(c.sag,-.16f,.07f);c.sway=std::clamp(c.sway,-.16f,.16f);
  if(!catching()&&c.freeze>0){c.freeze-=dt;if(c.freeze<=0){impactRush=false;c.rush=c.between(1.3f,2.2f);}}
  else if(!catching()&&c.rush>0)c.rush-=dt;
  else if(!catching()&&c.on&&going){c.nextRush-=dt;if(c.nextRush<=0){c.freeze=c.between(.10f,.20f);c.nextRush=c.between(5.f,10.f);++c.freezes;}}
  // Where the head wants to be: on what it watches, or on one thing after another.
  if(watching){const float dx=watched.x-x,dy=watched.y-y,dz=watched.z-(z+2.f);
   c.wantYaw=std::clamp(std::remainder(std::atan2(dx,dy)-yaw,6.2831853f),-1.25f,1.25f);c.wantPitch=std::clamp(std::atan2(dz,std::max(.5f,std::hypot(dx,dy)))+.35f,-.55f,.55f);}
  c.glance-=dt;if(c.glance<=0){c.glance=c.between(.7f,3.2f);
   if(!watching){c.wantYaw=c.between(-.85f,.85f);c.wantPitch=c.between(-.35f,.3f);}
   if(c.on)c.wantRoll=c.next()<.3f?c.between(-.55f,.55f):0.f;}
  c.twitch-=dt;if(c.on&&c.twitch<=0){c.twitch=c.between(1.2f,5.f);c.yawSpeed+=c.between(-5.f,5.f);c.rollSpeed+=c.between(-7.f,7.f);c.pitchSpeed+=c.between(-2.f,2.f);}
  // A stiff, underdamped neck: it snaps to a thing and overshoots.
  spring(c.headYaw,c.yawSpeed,c.wantYaw,c.freeze>0?150.f:62.f,c.freeze>0?15.f:10.f);
  spring(c.headPitch,c.pitchSpeed,c.wantPitch,55,10);spring(c.headRoll,c.rollSpeed,c.wantRoll,70,9);
  c.headYaw=std::clamp(c.headYaw,-1.4f,1.4f);c.headPitch=std::clamp(c.headPitch,-.65f,.65f);c.headRoll=std::clamp(c.headRoll,-.75f,.75f);
  // Breathing it has learnt by watching: uneven, sometimes forgotten.
  if(c.breathHold>0)c.breathHold-=dt;
  else{const float before=c.breathPhase;c.breathPhase+=dt*2.3f;if(c.on&&std::floor(c.breathPhase/6.2831853f)>std::floor(before/6.2831853f)&&c.next()<.28f)c.breathHold=c.between(.6f,2.2f);}
  c.breath=c.freeze>0?c.breath:.055f*std::max(0.f,std::sin(c.breathPhase));
 }
 bool mayLift(int leg,bool moving)const{
  int up=0;for(int other=0;other<LegCount;++other){if(feet[size_t(other)].planted())continue;++up;if(beside(leg,other))return false;}
  return up<(moving?2:1);   // standing, it shifts one foot at a time
 }
};

// A walk on a floor with a 0.3 m slab and a 0.5 m crate lane in the way: what must hold for it to look like walking.
inline bool testSkinbotWalk(std::ostream& report){
 bool all=true;auto check=[&](bool ok,const std::string& what){report<<(ok?"PASS ":"FAIL ")<<what<<'\n';all&=ok;};
 for(const bool obstacles:{false,true}){
  const Ground ground=[&](float gx,float gy,float from){float top=0;
   if(obstacles){if(gy>3.5f&&gy<7.2f)top=.3f;if(gx>-.2f&&gx<.9f&&gy>8.4f&&gy<9.4f)top=.5f;}
   return top<=from?top:0.f;};
  Walker w;w.place(0,0,0,ground,1);
  float headFar=0,peakRush=0,peakWalk=0,longestPause=0,sinceRush=1;
  const std::array<W,3> route{W{0,11,0},W{5,11,0},W{5,1,0}};
  size_t leg=0;w.pointAt(route[0].x,route[0].y);
  int frames=0,slid=0,unsupported=0,adjacent=0,stretched=0,under=0,steps=0,onSlab=0;float highest=0,leanest=0;
  std::array<Foot,LegCount> last=w.feet;
  while(frames<60*60&&leg<route.size()){
   w.update(1.f/60,ground);++frames;
   if(w.arrived()&&++leg<route.size())w.pointAt(route[leg].x,route[leg].y);
   for(int i=0;i<LegCount;++i){const Foot& f=w.feet[size_t(i)];const Foot& was=last[size_t(i)];
    if(f.planted()&&was.planted()&&std::hypot(f.at.x-was.at.x,f.at.y-was.at.y)>.0001f)++slid;
    if(!f.planted()&&was.planted())++steps;
    if(f.planted()){if(f.at.z<ground(f.at.x,f.at.y,f.at.z+.01f)-.001f)++under;if(f.at.z>.2f)++onSlab;
     const V want=w.unlean(w.toModel(f.at));if(length(w.legPose(i).at[3]-want)*Walker::Scale>.12f)++stretched;}
    for(int j=i+1;j<LegCount;++j)if(!f.planted()&&!w.feet[size_t(j)].planted()&&Walker::beside(i,j))++adjacent;}
   if(w.airborne()>2)++unsupported;
   headFar=std::max(headFar,std::fabs(w.curse.headYaw));
   const float pace=std::hypot(w.velocityX,w.velocityY);
   if(w.curse.rush>0){peakRush=std::max(peakRush,pace);sinceRush=0;}
   else{sinceRush+=1.f/60;if(sinceRush>.35f)peakWalk=std::max(peakWalk,pace);}
   longestPause=std::max(longestPause,w.curse.freeze);
   if(!std::isfinite(w.curse.headYaw)||!std::isfinite(w.curse.sag)||std::fabs(w.curse.sag)>.17f)++under;
   highest=std::max(highest,w.z);leanest=std::max(leanest,std::fabs(w.pitch));last=w.feet;
  }
  const std::string name=obstacles?"over a slab and a crate: ":"on a flat floor: ";
  check(peakRush>peakWalk+.25f&&longestPause<=.2f,name+"bursts are faster than the prowl ("+
        std::to_string(peakWalk)+" to "+std::to_string(peakRush)+" m/s), brief wind-up ("+std::to_string(longestPause)+" s)");
  check(headFar>.4f,name+"the head turns on its own ("+std::to_string(headFar*57.3f)+" degrees at most)");
  {Walker t;t.place(0,0,0,ground,1);const V shoulder=joint(ArmR,0),crown{0,1.45f,.55f};t.curse.headYaw=1;
   check(length(t.skin(shoulder)-shoulder)<.002f&&length(t.skin(crown)-crown)>.08f,name+"turning the head bends the skin and leaves the shoulders where they bolt on");}
  check(leg==route.size(),name+"it walks the whole route and turns its corners ("+std::to_string(frames/60.f)+" s, "+std::to_string(steps)+" steps)");
  check(slid==0,name+"a planted foot never slides ("+std::to_string(slid)+")");
  check(unsupported==0&&adjacent==0,name+"two feet always stand, never two neighbours in the air");
  check(under==0,name+"no foot ends inside the ground");
  check(stretched<frames*LegCount/50,name+"planted blades stay on their spots (off by over 12 cm in "+std::to_string(stretched)+" of "+std::to_string(frames*LegCount)+" foot-frames)");
  if(obstacles)check(onSlab>30&&highest>.22f&&leanest>.04f,name+"feet are set on top, and the body rises ("+std::to_string(highest)+" m) and leans ("+std::to_string(leanest*57.3f)+" degrees)");
  else check(highest<.01f&&leanest<.03f,name+"the body rides level");
 }
 report<<(all?"PASS":"FAIL")<<'\n';return all;
}
}

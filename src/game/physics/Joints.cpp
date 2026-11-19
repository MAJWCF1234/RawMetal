// Vectra's joints (Vectra.h, Joint). Each is rows between two bodies' joint frames, solved in the same TGS-soft
// substeps as the contacts and before them (Catto's Box2D v3, taken to 3D):
//  - the locked rows (the anchors together, an axis held, a turn held) are springs of Settings::jointHertz
//    with its damping, push-out capped by the softness alone; relaxed without the bias like the contacts;
//  - limits are one-sided rows: a gap is closed within the substep (speculative), a pass is pushed out softly;
//  - springs are soft rows of their own stiffness; motors are velocity rows held to their force;
//  - every row is warm started from what it carried (a joint built afresh each step finds it by key).
// Measures are taken from the frames each substep (the angle about a hinge from the swing-twist split of the
// relative turn; the cone from the two axes), the effective masses once a step.
#include "Vectra.h"
#include <algorithm>
#include <cmath>
namespace retro::vectra {
namespace {
struct Soft {float biasRate=0,massScale=1,impulseScale=0;};
Soft makeSoft(float hertz,float zeta,float h){if(hertz<=0)return {0,1,0};const float omega=2*kPi*hertz,a1=2*zeta+h*omega,a2=h*omega*a1,a3=1/(1+a2);return {omega/a1,a2*a3,a3};}
M3 skewOf(V3 a){M3 m=M3::zero();m.m[0][1]=-a.z;m.m[0][2]=a.y;m.m[1][0]=a.z;m.m[1][2]=-a.x;m.m[2][0]=-a.y;m.m[2][1]=a.x;return m;}
// The rotation vector (axis times angle) of a unit quaternion, the short way round.
V3 rotationOf(Quat q){if(q.w<0)q={-q.w,-q.x,-q.y,-q.z};const float s=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z);if(s<1e-7f)return V3{q.x,q.y,q.z}*2.f;return V3{q.x,q.y,q.z}*(2*std::atan2(s,q.w)/s);}
// The turn taking +z to `axis`.
Quat fromZ(V3 axis){axis=norm(axis);const float d=axis.z;if(d<-.99999f)return {0,1,0,0};const V3 c=cross({0,0,1},axis);return Quat{1+d,c.x,c.y,c.z}.normalized();}
Body& ground(){static Body g=[]{Body b;b.invMass=0;b.invInertia=M3::zero();b.invInertiaLocal=M3::zero();return b;}();g.x={};g.q={};g.v={};g.w={};return g;}
Body& bodyAt(Space& s,int i){return i>=0?s.bodies[size_t(i)]:ground();}
const Body& bodyAt(const Space& s,int i){return i>=0?s.bodies[size_t(i)]:ground();}
float effective(float mA,const M3& iA,V3 rA,float mB,const M3& iB,V3 rB,V3 u){const V3 ca=cross(rA,u),cb=cross(rB,u);const float k=mA+mB+dot(ca,iA*ca)+dot(cb,iB*cb);return k>0?1/k:0;}
float angular(const M3& iA,const M3& iB,V3 n){const float k=dot(n,(iA+iB)*n);return k>0?1/k:0;}
// The pose of the joint's two frames now: anchors (world) and the arms to them, and the frames' turns.
struct Pose {V3 rA{},rB{},pA{},pB{};Quat qA{},qB{};V3 zA{},zB{};};
Pose poseOf(const Joint& j,const Body& A,const Body& B,bool world){Pose p;p.rA=A.q.rotate(j.anchorA);p.pA=A.x+p.rA;
 if(world){p.rB={};p.pB=j.anchorB;p.qB=j.frameB;}else{p.rB=B.q.rotate(j.anchorB);p.pB=B.x+p.rB;p.qB=B.q*j.frameB;}
 p.qA=A.q*j.frameA;p.zA=p.qA.rotate({0,0,1});p.zB=p.qB.rotate({0,0,1});return p;}
// The turn of B's frame about z relative to A's (swing-twist split of the relative turn), in (-pi, pi].
float twistOf(const Pose& p){Quat r=p.qA.conj()*p.qB;if(r.w<0)r={-r.w,-r.x,-r.y,-r.z};return 2*std::atan2(r.z,r.w);}
// The swing-twist split of a relative turn r (r = swing * twist about z): the swing's turn vector's x and y parts and
// the twist, as a rig measures a joint (Carcass's bend, side and twist).
V3 splitOf(Quat r){if(r.w<0)r={-r.w,-r.x,-r.y,-r.z};const float l=std::sqrt(r.w*r.w+r.z*r.z);const Quat t=l>1e-9f?Quat{r.w/l,0,0,r.z/l}:Quat{};
 const V3 sw=rotationOf(r*t.conj());return {sw.x,sw.y,2*std::atan2(t.z,t.w)};}
// The split now, and the world axis along which each of its parts changes at a unit rate of B's turn against A's
// (a central difference over a small turn of B's frame in A's: the rows' exact rates at any bend).
struct Split {V3 f{};V3 axis[3]{};};
Split splitRows(const Pose& p){Split out;const Quat r=p.qA.conj()*p.qB;out.f=splitOf(r);constexpr float d=2e-3f;V3 col[3];
 for(int k=0;k<3;++k){const V3 e{k==0?d*.5f:0.f,k==1?d*.5f:0.f,k==2?d*.5f:0.f};const V3 g=splitOf(Quat{1,e.x,e.y,e.z}.normalized()*r),h=splitOf(Quat{1,-e.x,-e.y,-e.z}.normalized()*r);
  V3 df=(g-h)*(.5f/d);if(std::fabs(g.z-h.z)>kPi)df.z=std::remainder(g.z-h.z,2*kPi)*(.5f/d);col[k]=df;}
 for(int i=0;i<3;++i)out.axis[i]=p.qA.rotate(V3{comp(col[0],i),comp(col[1],i),comp(col[2],i)});
 return out;}
Quat turned(const Quat& q,V3 by){const float a=len(by);return a>1e-9f?(Quat::axisAngle(by*(1/a),a)*q).normalized():q;}
// A one-sided row: value C must stay >= 0, Cdot its rate along the row (the impulse pushes it up).
float oneSided(float C,float Cdot,float mass,float& accumulated,const Soft& lock,float inv,bool useBias){
 float bias=0,ms=1,is=0;if(C>0)bias=C*inv;else if(useBias){bias=C*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
 const float impulse=-mass*ms*(Cdot+bias)-is*accumulated;const float next=std::max(accumulated+impulse,0.f);const float d=next-accumulated;accumulated=next;return d;}
}
const char* jointKindName(JointKind k){switch(k){case JointKind::Hinge:return "hinge";case JointKind::Ball:return "ball";case JointKind::Slider:return "slider";case JointKind::Weld:return "weld";case JointKind::Distance:return "distance";case JointKind::Rope:return "rope";case JointKind::Spring:return "spring";case JointKind::Pulley:return "pulley";}return "?";}

void Joint::attach(const Body& A,const Body* B,V3 at,V3 axis){const Quat f=fromZ(axis);
 anchorA=A.q.unrotate(at-A.x);frameA=A.q.conj()*f;
 if(!B){anchorB=at;frameB=f;}else{anchorB=B->q.unrotate(at-B->x);frameB=B->q.conj()*f;}}
void Joint::attachEnds(const Body& A,const Body* B,V3 atA,V3 atB,bool keepLength){anchorA=A.q.unrotate(atA-A.x);frameA=A.q.conj();
 if(!B){anchorB=atB;frameB={};}else{anchorB=B->q.unrotate(atB-B->x);frameB=B->q.conj();}
 if(!keepLength)length=len(atB-atA);}
void Joint::attach(const Space& s,V3 at,V3 axis){attach(bodyAt(s,a),b<0?nullptr:&bodyAt(s,b),at,axis);}
void Joint::attachEnds(const Space& s,V3 atA,V3 atB,bool keepLength){attachEnds(bodyAt(s,a),b<0?nullptr:&bodyAt(s,b),atA,atB,keepLength);}
void Joint::attachPulley(const Space& s,V3 atA,V3 overA,V3 atB,V3 overB,float r){kind=JointKind::Pulley;attachEnds(s,atA,atB,true);groundA=overA;groundB=overB;ratio=r;length=len(atA-overA)+r*len(atB-overB);}
V3 Joint::worldA(const Space& s)const{const Body& A=bodyAt(s,a);return A.x+A.q.rotate(anchorA);}
V3 Joint::worldB(const Space& s)const{if(b<0)return anchorB;const Body& B=bodyAt(s,b);return B.x+B.q.rotate(anchorB);}
float Joint::angle(const Space& s)const{const Pose p=poseOf(*this,bodyAt(s,a),bodyAt(s,b),b<0);
 switch(kind){case JointKind::Hinge:return twistOf(p);case JointKind::Slider:return dot(p.pB-p.pA,p.zA);
  case JointKind::Pulley:return len(p.pA-groundA)+ratio*len(p.pB-groundB);default:return len(p.pB-p.pA);}}
float Joint::twist(const Space& s)const{return twistOf(poseOf(*this,bodyAt(s,a),bodyAt(s,b),b<0));}
V3 Joint::swingTwist(const Space& s)const{const Pose p=poseOf(*this,bodyAt(s,a),bodyAt(s,b),b<0);return splitOf(p.qA.conj()*p.qB);}
float Joint::swing(const Space& s)const{const Pose p=poseOf(*this,bodyAt(s,a),bodyAt(s,b),b<0);return std::atan2(len(cross(p.zA,p.zB)),dot(p.zA,p.zB));}

void Joint::prepare(Space& s,float h){prep.h=h;Body& A=bodyAt(s,a);Body& B=bodyAt(s,b);
 const bool ma=A.movable(),mb=b>=0&&B.movable();prep.mA=ma?A.invMass:0;prep.mB=mb?B.invMass:0;prep.iA=ma?A.invInertia:M3::zero();prep.iB=mb?B.invInertia:M3::zero();
 prep.turnMass=(prep.iA+prep.iB).inverse();   // (the anchors' effective mass is worked out as they turn: solve's point)
 // Warm starting across rebuilt spaces.
 if(key){auto it=std::lower_bound(s.jointCache.begin(),s.jointCache.end(),key,[](const auto& e,std::uint64_t k){return e.first<k;});
  if(it!=s.jointCache.end()&&it->first==key){const auto& c=it->second;linearImpulse=c.linear;angularImpulse=c.angular;lowerImpulse=c.lower;upperImpulse=c.upper;motorImpulse=c.motor;springImpulse=c.spring;axialImpulse=c.axial;coneImpulse=c.cone;twistLowerImpulse=c.twistLower;twistUpperImpulse=c.twistUpper;for(int k=0;k<4;++k)swingImpulse[k]=c.swing[k];frictionImpulse=c.friction;detentImpulse=c.detent;}}
 if(broken){linearImpulse=angularImpulse={};lowerImpulse=upperImpulse=motorImpulse=springImpulse=axialImpulse=coneImpulse=twistLowerImpulse=twistUpperImpulse=frictionImpulse=detentImpulse=0;for(float&k:swingImpulse)k=0;}
 // A winch reels its rope at the motor's speed while the pull is under its force (last step's pull).
 if(kind==JointKind::Rope&&motor!=JointMotor::Off&&!broken){const float dt=h*float(std::max(1,s.settings.substeps));
  float rate=motor==JointMotor::Speed?-motorSpeed:std::clamp((motorTarget-length)*servoGain,-std::fabs(motorSpeed),std::fabs(motorSpeed));
  if(rate<0&&motorMaxForce>0&&force>motorMaxForce)rate=0;   // stalled: the load is more than it can lift
  length+=rate*dt;if(limited)length=std::clamp(length,lower,upper);length=std::max(length,.01f);}
}
void Joint::warmStart(Space& s){if(broken)return;Body& A=bodyAt(s,a);Body& B=bodyAt(s,b);const bool ma=A.movable(),mb=b>=0&&B.movable();if(!ma&&!mb)return;
 const Pose p=poseOf(*this,A,B,b<0);
 V3 P=linearImpulse,L=angularImpulse;V3 rA=p.rA;   // the linear impulse at A's arm (the slider's at B's anchor)
 switch(kind){
  case JointKind::Hinge:L=L+p.zA*(lowerImpulse-upperImpulse+motorImpulse+springImpulse+frictionImpulse+detentImpulse);break;
  case JointKind::Slider:{rA=p.pB-A.x;P=P+p.zA*(lowerImpulse-upperImpulse+motorImpulse+springImpulse+frictionImpulse+detentImpulse);break;}
  case JointKind::Ball:if(swingLimited){const Split sp=splitRows(p);L=L+sp.axis[0]*(swingImpulse[0]-swingImpulse[1])+sp.axis[1]*(swingImpulse[2]-swingImpulse[3]);
    if(twistLimited)L=L+sp.axis[2]*(twistLowerImpulse-twistUpperImpulse);break;}else{const V3 c=cross(p.zA,p.zB);const float l=len(c);if(l>1e-6f)L=L-c*(coneImpulse/l);const V3 n=norm(p.zA+p.zB,p.zA);L=L+n*(twistLowerImpulse-twistUpperImpulse);break;}
  case JointKind::Distance:case JointKind::Rope:case JointKind::Spring:{const V3 u=norm(p.pB-p.pA);P=P+u*(axialImpulse+springImpulse+lowerImpulse-upperImpulse);break;}
  case JointKind::Pulley:{const V3 uA=norm(p.pA-groundA),uB=norm(p.pB-groundB);const float t=upperImpulse,m=motorImpulse;
   const V3 PA=uA*(m-t),PB=uB*(-ratio*t-m);   // the tension pulls each end toward its wheel; the motor winds A's side in
   if(ma){A.v=A.v+PA*A.invMass;A.w=A.w+A.invInertia*cross(p.rA,PA);}if(mb){B.v=B.v+PB*B.invMass;B.w=B.w+B.invInertia*cross(p.rB,PB);}return;}
  default:break;}
 if(ma){A.v=A.v-P*A.invMass;A.w=A.w-A.invInertia*(cross(rA,P)+L);}
 if(mb){B.v=B.v+P*B.invMass;B.w=B.w+B.invInertia*(cross(p.rB,P)+L);}}
void Joint::solve(Space& s,float h,bool useBias){if(broken)return;Body& A=bodyAt(s,a);Body& B=bodyAt(s,b);const bool ma=A.movable(),mb=b>=0&&B.movable();if(!ma&&!mb)return;
 const float inv=1/h,mA=prep.mA,mB=prep.mB;const M3& iA=prep.iA;const M3& iB=prep.iB;
 const Soft lock=makeSoft(std::min(lockHertz>0?lockHertz:s.settings.jointHertz,.25f*inv),lockDamping>0?lockDamping:s.settings.jointDamping,h);
 Pose p=poseOf(*this,A,B,b<0);
 auto applyAt=[&](V3 P,V3 rA,V3 rB){if(ma){A.v=A.v-P*mA;A.w=A.w-iA*cross(rA,P);}if(mb){B.v=B.v+P*mB;B.w=B.w+iB*cross(rB,P);}};
 auto applyTurn=[&](V3 L){if(ma)A.w=A.w-iA*L;if(mb)B.w=B.w+iB*L;};
 auto relW=[&]{return B.w-A.w;};
 auto relV=[&](V3 rA,V3 rB){return (B.v+cross(B.w,rB))-(A.v+cross(A.w,rA));};
 // The anchors together (hinge, ball, weld). Its effective mass from the arms as they are now, as the impulse is applied
 // at them: the step's first arms on a limb turning fast (a ragdoll's forearm flung against a wall turns half a radian in
 // a step) made the row overshoot more each pass, and the body blew apart to infinity.
 auto point=[&]{const V3 C=p.pB-p.pA;const V3 dv=relV(p.rA,p.rB);V3 bias{};float ms=1,is=0;if(useBias){bias=C*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
  const M3 sa=skewOf(p.rA),sb=skewOf(p.rB);const float m=mA+mB;const M3 K=(M3::diagonal({m,m,m})+sa*iA*sa.transposed()+sb*iB*sb.transposed()).inverse();
  const V3 P=(K*(dv+bias))*-ms-linearImpulse*is;linearImpulse=linearImpulse+P;applyAt(P,p.rA,p.rB);};
 // The whole turn held (weld, slider).
 auto turn=[&]{const V3 e=rotationOf(p.qB*p.qA.conj());V3 bias{};float ms=1,is=0;if(useBias){bias=e*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
  const V3 L=(prep.turnMass*(relW()+bias))*-ms-angularImpulse*is;angularImpulse=angularImpulse+L;applyTurn(L);};
 // A spring, a motor and limits on a value along a row: `rate` is its speed now, `mass` the row's, `push(d)` applies d along it.
 auto axis=[&](float value,float rate,float mass,auto&& push,auto&& again){
  if(springHertz>0){const Soft sp=makeSoft(std::min(springHertz,.25f*inv),springDamping,h);const float C=value-springTarget;
   const float d=-mass*sp.massScale*(rate+sp.biasRate*C)-sp.impulseScale*springImpulse;springImpulse+=d;push(d);rate=again();}
  // A detent: within its range of the nearest catch, a soft spring to it, holding with at most detentForce. Out of range,
  // what it was warm started with is taken back out (it holds nothing there).
  if(detentCount>0&&detentHertz>0){float C=1e30f;for(int k=0;k<std::min(detentCount,4);++k){const float d=kind==JointKind::Hinge?std::remainder(value-detents[size_t(k)],2*kPi):value-detents[size_t(k)];if(std::fabs(d)<std::fabs(C))C=d;}
   if(std::fabs(C)<detentRange){const Soft sp=makeSoft(std::min(detentHertz,.25f*inv),1.f,h);float next=detentImpulse-mass*sp.massScale*(rate+sp.biasRate*C)-sp.impulseScale*detentImpulse;
    if(detentForce>0)next=std::clamp(next,-detentForce*h,detentForce*h);const float d=next-detentImpulse;detentImpulse=next;push(d);rate=again();}
   else if(detentImpulse!=0){push(-detentImpulse);detentImpulse=0;rate=again();}}
  if(motor!=JointMotor::Off){const float miss=kind==JointKind::Hinge?std::remainder(motorTarget-value,2*kPi):motorTarget-value;   // a hinge's angle goes round
   const float want=motor==JointMotor::Speed?motorSpeed:std::clamp(miss*servoGain,-std::fabs(motorSpeed),std::fabs(motorSpeed));
   const float cap=motorMaxForce*h,old=motorImpulse;motorImpulse=std::clamp(old-mass*(rate-want),-cap,cap);const float d=motorImpulse-old;push(d);rate=again();}
  // Friction on the axis (a door's pin, a drawer's runners): the rate taken out, the impulse held to its force.
  if(frictionTorque>0){const float cap=frictionTorque*h,old=frictionImpulse;frictionImpulse=std::clamp(old-mass*rate,-cap,cap);push(frictionImpulse-old);rate=again();}
  if(limited){{const float d=oneSided(value-lower,rate,mass,lowerImpulse,lock,inv,useBias);push(d);rate=again();}
   {const float d=oneSided(upper-value,-rate,mass,upperImpulse,lock,inv,useBias);push(-d);}}};
 const int iterations=std::max(1,s.settings.jointIterations);
 for(int it=0;it<iterations;++it){if(it)p=poseOf(*this,A,B,b<0);
  switch(kind){
   case JointKind::Hinge:{const V3 n=p.zA;const float mass=angular(iA,iB,n);
    axis(twistOf(p),dot(relW(),n),mass,[&](float d){applyTurn(n*d);},[&]{return dot(relW(),n);});
    // z of B held on z of A: the two rows across the axis, solved together.
    {V3 t1,t2;basis(n,t1,t2);const V3 c=cross(p.zA,p.zB);const float sl=len(c);const V3 e=sl>1e-7f?c*(std::atan2(sl,dot(p.zA,p.zB))/sl):c;
     const M3 K=iA+iB;const float k11=dot(t1,K*t1),k12=dot(t1,K*t2),k22=dot(t2,K*t2);const float det=k11*k22-k12*k12;
     if(det>1e-12f){const V3 w=relW();float b1=dot(w,t1),b2=dot(w,t2);float ms=1,is=0;if(useBias){b1+=dot(e,t1)*lock.biasRate;b2+=dot(e,t2)*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
      const float x1=(k22*b1-k12*b2)/det,x2=(k11*b2-k12*b1)/det;const V3 acc=angularImpulse;const V3 L=(t1*x1+t2*x2)*-ms-(t1*dot(acc,t1)+t2*dot(acc,t2))*is;angularImpulse=angularImpulse+L;applyTurn(L);}}
    point();break;}
   case JointKind::Ball:{
    // Friction on the turn: the relative spin taken out, the impulse held to the friction's torque.
    if(frictionTorque>0){const V3 old=angularImpulse;V3 next=old-prep.turnMass*relW();const float cap=frictionTorque*h,l=len(next);if(l>cap)next=next*(cap/l);angularImpulse=next;applyTurn(next-old);}
    if(swingLimited){const Split sp=splitRows(p);
     // Each part of the split between its limits: rows along the part's own rate.
     auto limit=[&](V3 axis,float value,float lo,float hi,float& li,float& ui){const float mass=angular(iA,iB,axis);if(mass<=0)return;
      {const float d=oneSided(value-lo,dot(relW(),axis),mass,li,lock,inv,useBias);applyTurn(axis*d);}
      {const float d=oneSided(hi-value,-dot(relW(),axis),mass,ui,lock,inv,useBias);applyTurn(axis*-d);}};
     limit(sp.axis[0],sp.f.x,swingLower.x,swingUpper.x,swingImpulse[0],swingImpulse[1]);
     limit(sp.axis[1],sp.f.y,swingLower.y,swingUpper.y,swingImpulse[2],swingImpulse[3]);
     if(twistLimited)limit(sp.axis[2],sp.f.z,twistLower,twistUpper,twistLowerImpulse,twistUpperImpulse);
     point();break;}
    if(twistLimited){const V3 n=norm(p.zA+p.zB,p.zA);const float mass=angular(iA,iB,n),tw=twistOf(p);
     {const float d=oneSided(tw-twistLower,dot(relW(),n),mass,twistLowerImpulse,lock,inv,useBias);applyTurn(n*d);}
     {const float d=oneSided(twistUpper-tw,-dot(relW(),n),mass,twistUpperImpulse,lock,inv,useBias);applyTurn(n*-d);}}
    if(coneX>=0&&coneY>=0){const V3 c=cross(p.zA,p.zB);const float sl=len(c);
     if(sl>1e-6f){const V3 u=c*(1/sl);const float phi=std::atan2(sl,dot(p.zA,p.zB));
      // The cone's reach toward where B's axis leans (an ellipse of coneX toward A's x and coneY toward its y).
      const V3 d=p.qA.unrotate(p.zB);const float dl=std::sqrt(d.x*d.x+d.y*d.y);const float ca=dl>1e-6f?d.x/dl:1,sa=dl>1e-6f?d.y/dl:0;
      const float ex=std::max(coneX,1e-3f),ey=std::max(coneY,1e-3f);const float reach=1/std::sqrt((ca/ex)*(ca/ex)+(sa/ey)*(sa/ey));
      const float dd=oneSided(reach-phi,-dot(relW(),u),angular(iA,iB,u),coneImpulse,lock,inv,useBias);applyTurn(u*-dd);}
     else coneImpulse=0;}
    point();break;}
   case JointKind::Weld:turn();point();break;
   case JointKind::Slider:{const V3 n=p.zA;
    // Along the axis: B's anchor against A's line, the arm on A reaching to it.
    const V3 d=p.pB-p.pA;const V3 rA=p.rA+d;const float mass=effective(mA,iA,rA,mB,iB,p.rB,n);
    axis(dot(d,n),dot(relV(rA,p.rB),n),mass,[&](float x){applyAt(n*x,rA,p.rB);},[&]{return dot(relV(rA,p.rB),n);});
    turn();
    // Across it: the two rows held.
    {V3 t1,t2;basis(n,t1,t2);const V3 dn=p.pB-p.pA;const V3 ra=p.rA+dn;
     for(int r=0;r<2;++r){const V3 t=r?t2:t1;const float m=effective(mA,iA,ra,mB,iB,p.rB,t);const float C=dot(dn,t);float bias=0,ms=1,is=0;if(useBias){bias=C*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
      const float acc=dot(linearImpulse,t);const float x=-m*ms*(dot(relV(ra,p.rB),t)+bias)-is*acc;linearImpulse=linearImpulse+t*x;applyAt(t*x,ra,p.rB);}}
    break;}
   case JointKind::Distance:case JointKind::Rope:case JointKind::Spring:{const V3 d=p.pB-p.pA;const float L=len(d);const V3 u=L>1e-6f?d*(1/L):p.zA;const float mass=effective(mA,iA,p.rA,mB,iB,p.rB,u);
    auto rate=[&]{return dot(relV(p.rA,p.rB),u);};
    if(kind==JointKind::Rope){const float x=oneSided(length-L,-rate(),mass,upperImpulse,lock,inv,useBias);applyAt(u*-x,p.rA,p.rB);}
    else if(kind==JointKind::Distance){float bias=0,ms=1,is=0;if(useBias){bias=(L-length)*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
     const float x=-mass*ms*(rate()+bias)-is*axialImpulse;axialImpulse+=x;applyAt(u*x,p.rA,p.rB);}
    else{if(springHertz>0){const Soft sp=makeSoft(std::min(springHertz,.25f*inv),springDamping,h);const float x=-mass*sp.massScale*(rate()+sp.biasRate*(L-length))-sp.impulseScale*springImpulse;springImpulse+=x;applyAt(u*x,p.rA,p.rB);}
     if(limited){{const float x=oneSided(L-lower,rate(),mass,lowerImpulse,lock,inv,useBias);applyAt(u*x,p.rA,p.rB);}{const float x=oneSided(upper-L,-rate(),mass,upperImpulse,lock,inv,useBias);applyAt(u*-x,p.rA,p.rB);}}}
    break;}
   case JointKind::Pulley:{const V3 dA=p.pA-groundA,dB=p.pB-groundB;const float LA=len(dA),LB=len(dB);const V3 uA=LA>1e-6f?dA*(1/LA):V3{0,0,-1},uB=LB>1e-6f?dB*(1/LB):V3{0,0,-1};
    const V3 ca=cross(p.rA,uA),cb=cross(p.rB,uB);
    auto push=[&](float onA,float onB){const V3 PA=uA*onA,PB=uB*onB;if(ma){A.v=A.v+PA*mA;A.w=A.w+iA*cross(p.rA,PA);}if(mb){B.v=B.v+PB*mB;B.w=B.w+iB*cross(p.rB,PB);}};
    auto rateA=[&]{return dot(A.v+cross(A.w,p.rA),uA);};auto rateB=[&]{return dot(B.v+cross(B.w,p.rB),uB);};
    if(motor!=JointMotor::Off){const float k=mA+dot(ca,iA*ca)+mB+dot(cb,iB*cb);const float mass=k>0?1/k:0;
     // The wheel: lengthA shrinking at the motor's speed is A's end coming up and B's going down.
     const float want=2*(motor==JointMotor::Speed?-motorSpeed:std::clamp((motorTarget-LA)*servoGain,-std::fabs(motorSpeed),std::fabs(motorSpeed)));
     const float cap=motorMaxForce*h,old=motorImpulse;motorImpulse=std::clamp(old-mass*(rateA()-rateB()-want),-cap,cap);const float dm=motorImpulse-old;push(dm,-dm);}
    {const float k=mA+dot(ca,iA*ca)+ratio*ratio*(mB+dot(cb,iB*cb));const float mass=k>0?1/k:0;
     const float C=length-LA-ratio*LB,Cdot=-(rateA()+ratio*rateB());const float x=oneSided(C,Cdot,mass,upperImpulse,lock,inv,useBias);push(-x,-ratio*x);}
    break;}
  }}
}
void Joint::finish(Space& s,float h){
 // What it carried: the pull of its linear rows and the twist of its turning rows, over the substep.
 const float inv=1/std::max(h,1e-6f);
 float pull=len(linearImpulse)+std::fabs(axialImpulse)+std::fabs(springImpulse);float twistNow=len(angularImpulse);
 switch(kind){case JointKind::Hinge:case JointKind::Ball:twistNow+=std::fabs(lowerImpulse-upperImpulse+motorImpulse+springImpulse+frictionImpulse+detentImpulse)+std::fabs(coneImpulse)+std::fabs(twistLowerImpulse-twistUpperImpulse)+std::fabs(swingImpulse[0]-swingImpulse[1])+std::fabs(swingImpulse[2]-swingImpulse[3]);break;
  case JointKind::Slider:pull+=std::fabs(lowerImpulse-upperImpulse+motorImpulse+frictionImpulse+detentImpulse);break;
  case JointKind::Rope:case JointKind::Pulley:pull+=std::fabs(upperImpulse)+std::fabs(motorImpulse);break;
  case JointKind::Spring:pull+=std::fabs(lowerImpulse)+std::fabs(upperImpulse);break;default:break;}
 force=broken?0:pull*inv;torque=broken?0:twistNow*inv;
 if(!broken&&((breakForce>0&&force>breakForce)||(breakTorque>0&&torque>breakTorque))){broken=true;s.jointEvents.push_back({this,force,torque,worldA(s)});
  linearImpulse=angularImpulse={};lowerImpulse=upperImpulse=motorImpulse=springImpulse=axialImpulse=coneImpulse=twistLowerImpulse=twistUpperImpulse=frictionImpulse=detentImpulse=0;for(float&k:swingImpulse)k=0;}
 if(key){Space::JointCached c{linearImpulse,angularImpulse,lowerImpulse,upperImpulse,motorImpulse,springImpulse,axialImpulse,coneImpulse,twistLowerImpulse,twistUpperImpulse};for(int k=0;k<4;++k)c.swing[k]=swingImpulse[k];c.friction=frictionImpulse;c.detent=detentImpulse;
  auto it=std::lower_bound(s.jointCache.begin(),s.jointCache.end(),key,[](const auto& e,std::uint64_t k){return e.first<k;});
  if(it!=s.jointCache.end()&&it->first==key)it->second=c;else s.jointCache.insert(it,{key,c});}
}
namespace {
// Position correction for the kinds that hold an axis, a turn or a length (hinge, weld, slider, distance, rope): what the
// substeps left open put back by moving the bodies, each by its share (its inverse mass and inertia), velocities left to
// the rows. Each pass is held to Settings::projectMaxLinear and projectMaxAngular, so one wrenched far open closes over a
// few steps rather than being thrown through what is beside it. A held (kinematic) partner is not corrected against: its
// pull is the joint's rows' and the caller's (a prop dragged off its chain gives, joints.tear).
void projectHeld(Joint& j,Space& s,Body& A,Body& B,bool ma,bool mb){
 const float mA=ma?A.invMass:0,mB=mb?B.invMass:0;const M3 iA=ma?A.invInertia:M3::zero(),iB=mb?B.invInertia:M3::zero();
 const float maxL=s.settings.projectMaxLinear,maxA=s.settings.projectMaxAngular,slack=s.settings.projectSlack;
 auto capped=[](V3 v,float most){const float l=len(v);return l>most&&l>0?v*(most/l):v;};
 // A turn L between them: A by -iA L, B by iB L (their frames' relative turn changes by (iA + iB) L).
 auto turnBoth=[&](V3 L){if(ma)A.q=turned(A.q,iA*L*-1.f);if(mb)B.q=turned(B.q,iB*L);};
 // A push P at the arms: A by -P, B by +P, each turned by its arm.
 auto pushBoth=[&](V3 P,V3 rA,V3 rB){if(ma){A.x=A.x-P*mA;A.q=turned(A.q,iA*cross(rA,P)*-1.f);}if(mb){B.x=B.x+P*mB;B.q=turned(B.q,iB*cross(rB,P));}};
 const bool world=j.b<0;
 // The turn: the whole of it (weld, slider), or z of B on z of A (hinge), then the hinge's angle past its limits.
 if(j.kind==JointKind::Weld||j.kind==JointKind::Slider){const Pose p=poseOf(j,A,B,world);const V3 e=capped(rotationOf(p.qB*p.qA.conj()),maxA);
  if(lenSq(e)>1e-12f){const M3 K=iA+iB;float tr=K.m[0][0]+K.m[1][1]+K.m[2][2];if(tr>0){M3 Kr=K;for(int r=0;r<3;++r)Kr.m[r][r]+=1e-6f*tr;turnBoth(Kr.inverse()*e*-1.f);}}}
 if(j.kind==JointKind::Hinge){const Pose p=poseOf(j,A,B,world);const V3 c=cross(p.zA,p.zB);const float sl=len(c);
  if(sl>1e-7f){const V3 e=capped(c*(std::atan2(sl,dot(p.zA,p.zB))/sl),maxA);V3 t1,t2;basis(p.zA,t1,t2);const M3 K=iA+iB;
   const float k11=dot(t1,K*t1),k12=dot(t1,K*t2),k22=dot(t2,K*t2),det=k11*k22-k12*k12;
   if(det>1e-12f){const float b1=dot(e,t1),b2=dot(e,t2),x1=(k22*b1-k12*b2)/det,x2=(k11*b2-k12*b1)/det;turnBoth((t1*x1+t2*x2)*-1.f);}}
  if(j.limited){const Pose q=poseOf(j,A,B,world);const float an=twistOf(q);const float over=an<j.lower-slack?an-(j.lower-slack):an>j.upper+slack?an-(j.upper+slack):0.f;
   if(over!=0){const float m=angular(iA,iB,q.zA);if(m>0)turnBoth(q.zA*(-std::clamp(over,-maxA,maxA)*m));}}}
 // The anchors: together (hinge, weld), B's on A's line and within its travel (slider), at the length (distance), no
 // further than it (rope).
 {const Pose p=poseOf(j,A,B,world);const V3 d=p.pB-p.pA;
  if(j.kind==JointKind::Hinge||j.kind==JointKind::Weld){if(lenSq(d)<1e-10f)return;const V3 C=capped(d,maxL);
   const M3 sa=skewOf(p.rA),sb=skewOf(p.rB);const M3 K=M3::diagonal({mA+mB,mA+mB,mA+mB})+sa*iA*sa.transposed()+sb*iB*sb.transposed();pushBoth(K.inverse()*neg(C),p.rA,p.rB);}
  else if(j.kind==JointKind::Slider){const V3 n=p.zA;V3 t1,t2;basis(n,t1,t2);const V3 rA=p.rA+d;
   for(int r=0;r<3;++r){const V3 u=r==0?t1:r==1?t2:n;float C=dot(d,u);
    if(r==2){if(!j.limited)break;C=C<j.lower?C-j.lower:C>j.upper?C-j.upper:0.f;}
    if(std::fabs(C)<1e-5f)continue;C=std::clamp(C,-maxL,maxL);const float m=effective(mA,iA,rA,mB,iB,p.rB,u);pushBoth(u*(-C*m),rA,p.rB);}}
  else if(j.kind==JointKind::Distance||j.kind==JointKind::Rope){const float L=len(d);if(L<1e-6f)return;const V3 u=d*(1/L);
   float C=L-j.length;if(j.kind==JointKind::Rope)C=std::max(0.f,C);if(std::fabs(C)<1e-5f)return;C=std::clamp(C,-maxL,maxL);
   const float m=effective(mA,iA,p.rA,mB,iB,p.rB,u);pushBoth(u*(-C*m),p.rA,p.rB);}}}
}
// Position correction (`positionCorrection`). A ball: each part of the split past its limit turned back, then the anchors
// brought together, each body moving by its share (its inverse mass and inertia). The others: projectHeld above. Velocities
// are left to the rows. Never against a held (kinematic) partner (above).
void Joint::project(Space& s){if(!positionCorrection||broken)return;Body& A=bodyAt(s,a);Body& B=bodyAt(s,b);const bool ma=A.movable(),mb=b>=0&&B.movable();if(!ma&&!mb)return;
 if(A.kinematic||(b>=0&&B.kinematic))return;
 if(kind!=JointKind::Ball){if(kind==JointKind::Spring||kind==JointKind::Pulley)return;projectHeld(*this,s,A,B,ma,mb);return;}
 const float mA=ma?A.invMass:0,mB=mb?B.invMass:0;const M3 iA=ma?A.invInertia:M3::zero(),iB=mb?B.invInertia:M3::zero();
 if(swingLimited){const float Slack=s.settings.projectSlack;
  // Each body turned about the joint's pivot, not its own middle, so the anchors stay together as the limit is put back
  // (turned about their middles, the anchors' own correction would turn them half back). Its share by its inertia about
  // the pivot.
  auto aboutPivot=[](const Body& X,bool movable,V3 r){if(!movable||X.invMass<=0)return M3::zero();M3 P=X.invInertia.inverse();const float m=1/X.invMass,rr=dot(r,r);
   for(int i=0;i<3;++i)for(int j=0;j<3;++j)P.m[i][j]+=m*((i==j?rr:0.f)-comp(r,i)*comp(r,j));return P.inverse();};
  auto turnAbout=[](Body& X,V3 by,V3 pivot){const float an=len(by);if(an<1e-9f)return;const Quat t=Quat::axisAngle(by*(1/an),an);X.q=(t*X.q).normalized();X.x=pivot+t.rotate(X.x-pivot);};
  // The parts past their limits put back together (their rows couple at a wide swing: one by one they would undo each other).
  for(int pass=0;pass<2;++pass){const Pose p=poseOf(*this,A,B,b<0);const Split sp=splitRows(p);
   const float lo[3]{swingLower.x,swingLower.y,twistLower},hi[3]{swingUpper.x,swingUpper.y,twistUpper};float over[3]{0,0,0};bool any=false;
   for(int i=0;i<3;++i){if(i==2&&!twistLimited)continue;const float v=comp(sp.f,i);over[i]=v<lo[i]-Slack?v-(lo[i]-Slack):v>hi[i]+Slack?v-(hi[i]+Slack):0.f;any|=over[i]!=0;}
   if(!any)break;
   const V3 pivot=(p.pA+p.pB)*.5f;const M3 pA=aboutPivot(A,ma,pivot-A.x),pB=b>=0?aboutPivot(B,mb,pivot-B.x):M3::zero();
   const M3 W=pA+pB;M3 K;float trace=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c){K.m[r][c]=over[r]!=0&&over[c]!=0?dot(sp.axis[r],W*sp.axis[c]):(r==c?1.f:0.f);if(r==c)trace+=K.m[r][c];}
   for(int r=0;r<3;++r)K.m[r][r]+=1e-6f*trace;
   const V3 lam=K.inverse()*V3{-over[0],-over[1],-over[2]};
   const V3 L=sp.axis[0]*(over[0]!=0?lam.x:0.f)+sp.axis[1]*(over[1]!=0?lam.y:0.f)+sp.axis[2]*(over[2]!=0?lam.z:0.f);
   if(ma)turnAbout(A,pA*L*-1.f,pivot);if(mb)turnAbout(B,pB*L,pivot);}}
 const Pose p=poseOf(*this,A,B,b<0);const V3 C=p.pB-p.pA;if(lenSq(C)<1e-8f)return;
 const M3 sa=skewOf(p.rA),sb=skewOf(p.rB);const M3 K=M3::diagonal({mA+mB,mA+mB,mA+mB})+sa*iA*sa.transposed()+sb*iB*sb.transposed();const V3 P=K.inverse()*neg(C);
 if(ma){A.x=A.x-P*mA;A.q=turned(A.q,iA*cross(p.rA,P)*-1.f);}
 if(mb){B.x=B.x+P*mB;B.q=turned(B.q,iB*cross(p.rB,P));}}
bool Joint::driving(const Space& s)const{if(broken||motor==JointMotor::Off)return false;
 if(motor==JointMotor::Speed){if(motorSpeed==0)return false;if(kind==JointKind::Rope&&limited)return motorSpeed>0?length>lower+1e-4f:length<upper-1e-4f;return true;}
 const float now=kind==JointKind::Rope?length:angle(s);const float miss=kind==JointKind::Hinge?std::remainder(motorTarget-now,2*kPi):motorTarget-now;return std::fabs(miss)>.004f;}
// The mattress's hinge: its old fields as a Joint's.
void Hinge::prepare(Space& s,float h){kind=JointKind::Hinge;
 const Quat f=fromZ(axisA);frameA=f;frameB=rest.conj()*f;
 springHertz=hertz;springDamping=damping;springTarget=0;limited=limit>0;lower=-limit;upper=limit;
 Joint::prepare(s,h);}
}

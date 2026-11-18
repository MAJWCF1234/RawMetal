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
float Joint::swing(const Space& s)const{const Pose p=poseOf(*this,bodyAt(s,a),bodyAt(s,b),b<0);return std::atan2(len(cross(p.zA,p.zB)),dot(p.zA,p.zB));}

void Joint::prepare(Space& s,float h){prep.h=h;Body& A=bodyAt(s,a);Body& B=bodyAt(s,b);
 const bool ma=A.movable(),mb=b>=0&&B.movable();prep.mA=ma?A.invMass:0;prep.mB=mb?B.invMass:0;prep.iA=ma?A.invInertia:M3::zero();prep.iB=mb?B.invInertia:M3::zero();
 const Pose p=poseOf(*this,A,B,b<0);
 const M3 sa=skewOf(p.rA),sb=skewOf(p.rB);const float m=prep.mA+prep.mB;
 prep.pointMass=(M3::diagonal({m,m,m})+sa*prep.iA*sa.transposed()+sb*prep.iB*sb.transposed()).inverse();
 prep.turnMass=(prep.iA+prep.iB).inverse();
 // Warm starting across rebuilt spaces.
 if(key){auto it=std::lower_bound(s.jointCache.begin(),s.jointCache.end(),key,[](const auto& e,std::uint64_t k){return e.first<k;});
  if(it!=s.jointCache.end()&&it->first==key){const auto& c=it->second;linearImpulse=c.linear;angularImpulse=c.angular;lowerImpulse=c.lower;upperImpulse=c.upper;motorImpulse=c.motor;springImpulse=c.spring;axialImpulse=c.axial;coneImpulse=c.cone;twistLowerImpulse=c.twistLower;twistUpperImpulse=c.twistUpper;}}
 if(broken){linearImpulse=angularImpulse={};lowerImpulse=upperImpulse=motorImpulse=springImpulse=axialImpulse=coneImpulse=twistLowerImpulse=twistUpperImpulse=0;}
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
  case JointKind::Hinge:L=L+p.zA*(lowerImpulse-upperImpulse+motorImpulse+springImpulse);break;
  case JointKind::Slider:{rA=p.pB-A.x;P=P+p.zA*(lowerImpulse-upperImpulse+motorImpulse+springImpulse);break;}
  case JointKind::Ball:{const V3 c=cross(p.zA,p.zB);const float l=len(c);if(l>1e-6f)L=L-c*(coneImpulse/l);const V3 n=norm(p.zA+p.zB,p.zA);L=L+n*(twistLowerImpulse-twistUpperImpulse);break;}
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
 // The anchors together (hinge, ball, weld).
 auto point=[&]{const V3 C=p.pB-p.pA;const V3 dv=relV(p.rA,p.rB);V3 bias{};float ms=1,is=0;if(useBias){bias=C*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
  const V3 P=(prep.pointMass*(dv+bias))*-ms-linearImpulse*is;linearImpulse=linearImpulse+P;applyAt(P,p.rA,p.rB);};
 // The whole turn held (weld, slider).
 auto turn=[&]{const V3 e=rotationOf(p.qB*p.qA.conj());V3 bias{};float ms=1,is=0;if(useBias){bias=e*lock.biasRate;ms=lock.massScale;is=lock.impulseScale;}
  const V3 L=(prep.turnMass*(relW()+bias))*-ms-angularImpulse*is;angularImpulse=angularImpulse+L;applyTurn(L);};
 // A spring, a motor and limits on a value along a row: `rate` is its speed now, `mass` the row's, `push(d)` applies d along it.
 auto axis=[&](float value,float rate,float mass,auto&& push,auto&& again){
  if(springHertz>0){const Soft sp=makeSoft(std::min(springHertz,.25f*inv),springDamping,h);const float C=value-springTarget;
   const float d=-mass*sp.massScale*(rate+sp.biasRate*C)-sp.impulseScale*springImpulse;springImpulse+=d;push(d);rate=again();}
  if(motor!=JointMotor::Off){const float miss=kind==JointKind::Hinge?std::remainder(motorTarget-value,2*kPi):motorTarget-value;   // a hinge's angle goes round
   const float want=motor==JointMotor::Speed?motorSpeed:std::clamp(miss*servoGain,-std::fabs(motorSpeed),std::fabs(motorSpeed));
   const float cap=motorMaxForce*h,old=motorImpulse;motorImpulse=std::clamp(old-mass*(rate-want),-cap,cap);const float d=motorImpulse-old;push(d);rate=again();}
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
 switch(kind){case JointKind::Hinge:case JointKind::Ball:twistNow+=std::fabs(lowerImpulse-upperImpulse+motorImpulse+springImpulse)+std::fabs(coneImpulse)+std::fabs(twistLowerImpulse-twistUpperImpulse);break;
  case JointKind::Slider:pull+=std::fabs(lowerImpulse-upperImpulse+motorImpulse);break;
  case JointKind::Rope:case JointKind::Pulley:pull+=std::fabs(upperImpulse)+std::fabs(motorImpulse);break;
  case JointKind::Spring:pull+=std::fabs(lowerImpulse)+std::fabs(upperImpulse);break;default:break;}
 force=broken?0:pull*inv;torque=broken?0:twistNow*inv;
 if(!broken&&((breakForce>0&&force>breakForce)||(breakTorque>0&&torque>breakTorque))){broken=true;s.jointEvents.push_back({this,force,torque,worldA(s)});
  linearImpulse=angularImpulse={};lowerImpulse=upperImpulse=motorImpulse=springImpulse=axialImpulse=coneImpulse=twistLowerImpulse=twistUpperImpulse=0;}
 if(key){Space::JointCached c{linearImpulse,angularImpulse,lowerImpulse,upperImpulse,motorImpulse,springImpulse,axialImpulse,coneImpulse,twistLowerImpulse,twistUpperImpulse};
  auto it=std::lower_bound(s.jointCache.begin(),s.jointCache.end(),key,[](const auto& e,std::uint64_t k){return e.first<k;});
  if(it!=s.jointCache.end()&&it->first==key)it->second=c;else s.jointCache.insert(it,{key,c});}
}
bool Joint::driving(const Space& s)const{if(broken||motor==JointMotor::Off)return false;
 if(motor==JointMotor::Speed){if(motorSpeed==0)return false;if(kind==JointKind::Rope&&limited)return motorSpeed>0?length>lower+1e-4f:length<upper-1e-4f;return true;}
 const float now=kind==JointKind::Rope?length:angle(s);const float miss=kind==JointKind::Hinge?std::remainder(motorTarget-now,2*kPi):motorTarget-now;return std::fabs(miss)>.004f;}
// The mattress's hinge: its old fields as a Joint's.
void Hinge::prepare(Space& s,float h){kind=JointKind::Hinge;
 const Quat f=fromZ(axisA);frameA=f;frameB=rest.conj()*f;
 springHertz=hertz;springDamping=damping;springTarget=0;limited=limit>0;lower=-limit;upper=limit;
 Joint::prepare(s,h);}
}

// Two-hand VR carry: a large prop (a crate, a chair, anything 0.45 m across or 4 kg and up) already in one hand is
// taken by the other hand closing on it within 12 cm of its surface. From then the prop follows both hands:
//  - each hand keeps the place it took on the prop (in the prop's own frame, at the grab);
//  - the prop turns by the turn that carries the line between the hands at the grab onto the line between them
//    now (lift one hand and it tilts, swing them round and it turns), then about that line by how far the hands'
//    mean pointing has rolled about it (both wrists tipped forward tip it over);
//  - its middle is set so the midpoint of the two grips lies at the midpoint of the hands.
// The pose is kept only where the prop fits the world (a wall stops it, it is not dropped); it is kinematic in the
// solver at the mean of the two hands' velocities (carryVelocity), so it shoves what it meets and, let go of with
// both hands at once, is thrown at that speed.
#include "Game.h"
#include "physics/PropBodies.h"
#include <cmath>
namespace retro {
namespace {
using vectra::V3;using vectra::Quat;
V3 v3(Point3 p){return {p.x,p.y,p.z};}
// The shortest turn taking unit a onto unit b.
Quat arc(V3 a,V3 b){const float d=vectra::dot(a,b);if(d<-.9999f){V3 t,u;vectra::basis(a,t,u);return Quat::axisAngle(t,kPi);}const V3 c=vectra::cross(a,b);return Quat{1+d,c.x,c.y,c.z}.normalized();}
// The part of v at right angles to unit axis, unit (fallback when v lies along it).
V3 across(V3 v,V3 axis,V3 fallback){const V3 p=v-axis*vectra::dot(v,axis);const float l=vectra::len(p);return l>1e-3f?p*(1/l):fallback;}
}
int Game::twoHandRelease(bool carryGrip,bool secondGrip,float& grace,float dt){
 if(carryGrip&&secondGrip){grace=0;return 0;}
 if(!carryGrip&&!secondGrip){grace=0;return 3;}
 grace+=dt;if(grace<TwoHandGrace)return 0;
 grace=0;return carryGrip?1:2;}
Point3 Game::carryVelocity()const{
 if(m_vrInputActive&&m_vrHandValid[size_t(m_vrCarryHand)]){Point3 v=m_vrHandVelocity[size_t(m_vrCarryHand)];
  if(m_vrSecondHand>=0&&m_vrHandValid[size_t(m_vrSecondHand)])v=(v+m_vrHandVelocity[size_t(m_vrSecondHand)])*.5f;return v;}
 return {m_carryHandVelocity.x,m_carryHandVelocity.y,m_carryHandVz};}
bool Game::grabSecondHand(int hand){
 if(!holdingClutter()||hand<0||hand>1||hand==m_vrCarryHand||!m_vrHandValid[size_t(hand)]||!m_vrHandValid[size_t(m_vrCarryHand)])return false;
 auto&c=m_clutter[size_t(m_heldClutter)];if(c.model<0||vectra::chained(c))return false;const auto d=c.size();if(std::max({d[0],d[1],d[2]})<.45f&&c.mass()<4)return false;
 // Near its surface: within 12 cm of its own shape.
 vectra::Body b;vectra::toBody(c,b);const V3 h=v3(m_vrHandPosition[size_t(hand)]);float nearest=1e9f;
 for(int p=0;p<b.shape->count;++p){V3 s,n;float dist;if(vectra::nearestPoint(b,p,h,s,n,dist))nearest=std::min(nearest,dist);}
 if(nearest>.12f)return false;
 const Quat q=vectra::propOrientation(c);const V3 centre=vectra::propCentre(c);
 for(int k:{m_vrCarryHand,hand}){const V3 local=q.unrotate(v3(m_vrHandPosition[size_t(k)])-centre);m_vrGripLocal[size_t(k)]={local.x,local.y,local.z};}
 m_vrGripTurn={q.w,q.x,q.y,q.z};
 const V3 axis=vectra::norm(v3(m_vrHandPosition[size_t(hand)])-v3(m_vrHandPosition[size_t(m_vrCarryHand)]),{1,0,0});
 const V3 mean=v3(m_vrHandDirection[size_t(hand)])+v3(m_vrHandDirection[size_t(m_vrCarryHand)]);
 V3 t,u;vectra::basis(axis,t,u);const V3 up=across(mean,axis,u);
 m_vrGripAxis={axis.x,axis.y,axis.z};m_vrGripUp={up.x,up.y,up.z};m_vrSecondHand=hand;sound(Sound::Pickup,.18f,.7f);return true;}
bool Game::updateTwoHandCarry(const InputState&,Clutter& c){
 const int a=m_vrCarryHand,b=m_vrSecondHand;if(b<0||!m_vrHandValid[size_t(a)]||!m_vrHandValid[size_t(b)]){m_vrSecondHand=-1;return false;}
 const Clutter previous=c;
 const V3 ha=v3(m_vrHandPosition[size_t(a)]),hb=v3(m_vrHandPosition[size_t(b)]);
 const V3 axis0{m_vrGripAxis.x,m_vrGripAxis.y,m_vrGripAxis.z},axis=vectra::norm(hb-ha,axis0);
 const Quat swing=arc(axis0,axis);
 const V3 up0=swing.rotate({m_vrGripUp.x,m_vrGripUp.y,m_vrGripUp.z});
 const V3 up=across(v3(m_vrHandDirection[size_t(a)])+v3(m_vrHandDirection[size_t(b)]),axis,up0);
 const float roll=std::atan2(vectra::dot(vectra::cross(up0,up),axis),vectra::dot(up0,up));
 const Quat q=(Quat::axisAngle(axis,roll)*swing*Quat{m_vrGripTurn[0],m_vrGripTurn[1],m_vrGripTurn[2],m_vrGripTurn[3]}).normalized();
 const Point3 gl=(m_vrGripLocal[size_t(a)]+m_vrGripLocal[size_t(b)])*.5f;
 const V3 centre=(ha+hb)*.5f-q.rotate(v3(gl));
 // The pose, as the solver writes one (PropBodies fromBody): its quaternion, the Euler angles from it, its box's middle.
 c.orient={q.w,q.x,q.y,q.z};q.toEuler(c.yaw,c.pitch,c.roll);c.orientEuler={c.yaw,c.pitch,c.roll};c.pos={centre.x,centre.y};c.z=centre.z-c.extent()[2];
 const float h=c.height();
 const bool fits=c.everyOutlinePoint([&](Vec2 offset){auto p=c.pos+offset;const auto&w=worldAt(p);return w.fits(p.x,p.y,c.z,h,true,true)&&!w.doorBlocks(p.x,p.y,c.z,h);});
 if(!fits){c=previous;return true;}
 if(c.mass()>=8)pushBodiesFromCarried(c,previous);
 c.velocity={};c.vz=c.pitchSpeed=c.rollSpeed=c.spin=0;c.sleeping=false;c.restTime=0;
 return true;}
}

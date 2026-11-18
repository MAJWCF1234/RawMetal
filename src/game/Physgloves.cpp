// Physgloves: point at a loose prop in reach, flick, and it pops toward the hand on a ballistic arc that lands
// where the hand is; caught inside a generous window, or it falls on as anything thrown does.
//  - Aim: desktop, the look (the eye and the crosshair); VR, each hand's pointing. The prop nearest the line,
//    within 7 m, seen from the eye or the hand, not pinned, not in the hand. What lies in a container or
//    hangs on a joint is as good a target as anything: the pull wakes it, and a rope it hangs from holds it,
//    so a hung crate swings out toward the hand on its rope.
//  - Flick: desktop, G, or hold the middle mouse button empty-handed (the look stays put on the target) and flick
//    the mouse back; VR, close the hand while pointing (it locks on) and flick the wrist back toward you.
//  - The arc: the speed that takes the prop's middle to the catch point in a flight time that grows with the
//    distance (0.42 to 0.95 s) under the solver's own gravity (14 m/s^2). Up to 6 kg it flies the whole way;
//    heavier, the speed is cut by 6 / mass: a 9 kg crate hops part way, a 35 kg drum lurches a hand's width.
//  - The catch: desktop, its middle comes within 0.55 m of the hold point while the window is open (flight +
//    0.3 s); VR, within 0.45 m of a closed hand. Missed (the player turned away, walked off, or the hand was
//    open), it goes on as it was going and lands where it lands.
//  - The target is drawn a little brighter, pulsing (Scene3D: drawClutter), so you see what you would pull.
#include "Game.h"
#include "physics/PropBodies.h"
#include "Containers.h"
#include "../world/ModelIds.h"
#include <cmath>
namespace retro {
namespace {
constexpr float Reach=7.f,FlyMass=6.f,DeskCatch=.55f,VrCatch=.45f,FlickPixels=16.f,VrFlick=1.3f;
Point3 middleOf(const Clutter& c){return {c.pos.x,c.pos.y,c.z+c.height()*.5f};}
float lengthOf(Point3 d){return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
}
Point3 Game::physgloveCatchPoint(int hand,const Clutter& c)const{
 if(m_vrInputActive&&hand>=0&&hand<2&&m_vrHandValid[size_t(hand)]){const auto p=m_vrHandPosition[size_t(hand)],d=m_vrHandDirection[size_t(hand)];return {p.x+d.x*.12f,p.y+d.y*.12f,p.z+d.z*.12f};}
 // As the carry holds it: before the eye at its reach, along the look.
 const float reach=c.model>=0?std::max(.9f,.5f+c.footprint()*.5f):.9f,pitch=m_player.pitch/140.f;
 return {m_player.pos.x+std::cos(m_player.angle)*reach*std::cos(pitch),m_player.pos.y+std::sin(m_player.angle)*reach*std::cos(pitch),m_player.z+m_player.eye+reach*std::sin(pitch)};
}
// TODO(integration): main's Vectra query API (physics/PropQueries.h rayCastProp / sphereCastProp) can replace the cone
// test below with a cast against each prop's own shape, and the eye-visibility check with one against props too.
int Game::physgloveAim(int hand)const{
 Point3 from,dir;
 if(m_vrInputActive){if(hand<0||hand>1||!m_vrHandValid[size_t(hand)])return -1;from=m_vrHandPosition[size_t(hand)];dir=m_vrHandDirection[size_t(hand)];}
 else{const float pitch=m_player.pitch/140.f;from={m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye};dir={std::cos(m_player.angle)*std::cos(pitch),std::sin(m_player.angle)*std::cos(pitch),std::sin(pitch)};}
 const float n=lengthOf(dir);if(n<1e-4f)return -1;dir=dir*(1/n);
 int best=-1;float score=1e9f;
 for(int i=0;i<int(m_clutter.size());++i){const auto&c=m_clutter[size_t(i)];
  if(i==m_heldClutter||i==m_physglove.flying||c.model<0||c.broken||c.pinned||dormantEntity(c.z))continue;
  const Point3 m=middleOf(c),d=m-from;const float dist=lengthOf(d),along=d.x*dir.x+d.y*dir.y+d.z*dir.z;if(dist<.35f||dist>Reach||along<=.2f)continue;
  // Off the line by no more than its own half size and a cone of about 6 degrees (a mug at 6 m is still found).
  const float off=std::sqrt(std::max(0.f,dist*dist-along*along)),allowed=.12f+.1f*dist+c.footprint()*.5f;if(off>allowed)continue;
  const float s=off/dist+dist*.004f;if(s>=score)continue;
  Point3 normal;if(clutterPathFraction(from,m,normal)<.999f)continue;
  // Seen from the eye too (a VR hand round a corner does not pull what the eye cannot see).
  if(m_vrInputActive&&clutterPathFraction({m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye},m,normal)<.999f)continue;
  best=i;score=s;}
 return best;
}
bool Game::physglovePull(int index,int hand){
 if(index<0||index>=int(m_clutter.size())||index==m_heldClutter)return false;auto&c=m_clutter[size_t(index)];if(c.broken||c.pinned||c.model<0)return false;
 auto&s=m_physglove;
 // Lying in an open container: first popped straight up, clear of its rim (the arc from inside would meet its
 // wall); the arc is launched from the top of the hop (updatePhysgloves).
 s.popRim=-1e9f;for(const auto&box:m_clutter)if(&box!=&c&&containers::holds(box,c)){s.popRim=std::max(s.popRim,box.z+containers::inner(box).rim+.05f);}
 knockClutter(m_level,size_t(index),middleOf(c),{});
 s.flying=index;s.flyingHand=hand;s.flight=0;s.target=-1;s.locked=false;++s.pulls;c.riding=false;c.soundOwner=-1;
 sound(Sound::PunchSwing,.32f,1.45f);
 if(s.popRim>c.z&&c.mass()<=FlyMass){const float g=-vectra::Body{}.gravity.z;c.velocity={};c.vz=std::sqrt(2*g*(s.popRim-c.z))+.4f;c.sleeping=false;c.restTime=0;c.bodyImpactCooldown=1.5f;s.popping=true;s.window=1.5f;s.lurch=false;return true;}
 s.popping=false;physgloveLaunch(index,hand);
 // On a rope or a hinge it is tugged, not caught: the joint holds it (a hung crate swings out toward the hand), at no
 // more than 2 m/s, a tug its rope takes without snapping.
 for(const auto&[a,b]:jointedProps(m_level,m_clutter))if(a==index||b==index){s.lurch=true;const float v=std::sqrt(c.velocity.x*c.velocity.x+c.velocity.y*c.velocity.y+c.vz*c.vz);
  if(v>2.f){c.velocity=c.velocity*(2.f/v);c.vz*=2.f/v;}break;}
 return true;
}
void Game::physgloveLaunch(int index,int hand){
 auto&c=m_clutter[size_t(index)];auto&s=m_physglove;
 const Point3 from=middleOf(c),to=physgloveCatchPoint(hand,c),d=to-from;const float dist=lengthOf(d);
 const float T=std::clamp(.42f+.075f*dist,.42f,.95f),g=-vectra::Body{}.gravity.z;
 Point3 v{d.x/T,d.y/T,d.z/T+.5f*g*T};
 // The pull is an impulse the gloves can give: what is light flies, what is heavy lurches.
 const float mass=c.mass(),share=mass<=FlyMass?1.f:FlyMass/mass;v=v*share;
 // (Its slight drag over the flight: a tenth of a second's worth of the body's linear damping, given back.)
 v=v*(1+vectra::Body{}.linearDamping*T);
 c.velocity={v.x,v.y};c.vz=v.z;
 if(c.model>=0&&!vectra::legacyProps())vectra::setPropAngular(c,vectra::propAngular(c)*.2f);else{c.spin*=.2f;c.pitchSpeed*=.2f;c.rollSpeed*=.2f;}
 c.sleeping=false;c.restTime=0;c.riding=false;c.projectile=false;c.soundOwner=-1;c.bodyImpactCooldown=T+.5f;   // (it does not strike the one who pulled it)
 s.flight=0;s.window=T+.3f;s.lurch=share<.6f;s.aimedAt=to;
}
void Game::updatePhysgloves(const InputState& input,float dt){
 auto&s=m_physglove;
 // A prop on its way: caught at the hand inside the window, else let go.
 if(s.flying>=0){
  if(s.flying>=int(m_clutter.size())||m_clutter[size_t(s.flying)].broken||s.flying==m_heldClutter){if(s.flying<int(m_clutter.size())&&m_clutter[size_t(s.flying)].broken&&!s.lurch)++s.misses;s.flying=-1;}   // (broken on landing: missed)
  else if(s.popping){auto&c=m_clutter[size_t(s.flying)];s.flight+=dt;
   // Over the rim (or at the top of its hop): off to the hand.
   if(c.z>=s.popRim||c.vz<=.3f){s.popping=false;physgloveLaunch(s.flying,s.flyingHand);}else if(s.flight>s.window)s.flying=-1;}
  else{auto&c=m_clutter[size_t(s.flying)];s.flight+=dt;const int hand=s.flyingHand;
   const Point3 at=physgloveCatchPoint(hand,c),gap=middleOf(c)-at;const float d=lengthOf(gap);
   const bool open=!holdingClutter()&&c.mass()<=25&&!s.lurch;
   const bool vr=m_vrInputActive&&hand>=0&&hand<2;const bool closed=!vr||(input.vrGrip[size_t(hand)]&&m_vrHandValid[size_t(hand)]);
   if(open&&closed&&s.flight>.08f&&d<(vr?VrCatch:DeskCatch)){
    m_heldClutter=s.flying;if(vr)m_vrCarryHand=hand;c.velocity={};c.vz=0;c.spin=c.pitchSpeed=c.rollSpeed=0;c.projectile=false;c.sleeping=false;c.restTime=0;
    m_carryHandVelocity={};m_carryHandVz=0;sound(Sound::Pickup,.3f,.85f);++s.catches;s.flying=-1;}
   else if(s.flight>=s.window){if(!s.lurch)++s.misses;s.flying=-1;}}}
 const bool held=holdingClutter();
 if(m_vrInputActive&&input.vrTracked){
  for(int hand=0;hand<2;++hand){const bool grip=input.vrGrip[size_t(hand)],pressed=grip&&!s.grip[size_t(hand)];s.grip[size_t(hand)]=grip;
   // A hand closed on nothing in reach while pointing at a prop locks onto it.
   if(pressed&&!held&&!s.locked&&s.flying<0){const int t=physgloveAim(hand);if(t>=0){s.locked=true;s.hand=hand;s.target=t;}}
   if(s.locked&&s.hand==hand&&!grip)s.locked=false;}
  if(held){s.locked=false;s.target=-1;return;}
  if(s.locked){if(s.target<0||s.target>=int(m_clutter.size())||m_clutter[size_t(s.target)].broken){s.locked=false;return;}
   // The flick: the wrist snapped back toward the face, or up.
   const auto p=m_vrHandPosition[size_t(s.hand)],v=m_vrHandVelocity[size_t(s.hand)];const Point3 head{m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye};
   Point3 back=head-p;const float l=lengthOf(back);if(l>1e-3f)back=back*(1/l);
   if(v.x*back.x+v.y*back.y+v.z*back.z>VrFlick||v.z>VrFlick*1.4f)physglovePull(s.target,s.hand);
   return;}
  s.hand=m_vrHandValid[1]?1:0;s.target=s.flying<0?physgloveAim(s.hand):-1;return;}
 // Desktop.
 const bool key=input.physglove&&!s.key;s.key=input.physglove;
 if(held){s.locked=false;s.target=-1;return;}
 if(input.rotateProp){if(!s.locked&&s.flying<0){const int t=physgloveAim(-1);if(t>=0){s.locked=true;s.target=t;}}}
 else s.locked=false;
 if(s.locked){if(s.target<0||s.target>=int(m_clutter.size())||m_clutter[size_t(s.target)].broken){s.locked=false;return;}
  if(input.mouseDy<-FlickPixels||key)physglovePull(s.target,-1);return;}
 s.target=s.flying<0?physgloveAim(-1):-1;
 if(key&&s.target>=0)physglovePull(s.target,-1);
 (void)dt;
}
}

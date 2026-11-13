// Doors in play: the hand kept on a leaf, a shove, the sounds a leaf makes as
// it moves, and the creatures that beat a door down. The motion itself (a
// leaf's weight, its stop and its latch) is World::updateDoors.
#include "Game.h"
#include "../core/FrameProfile.h"
#include <cmath>
namespace retro {
namespace {
// E held past this keeps the hand on the leaf; it eases at CreepSpeed while held.
constexpr float HoldDelay=.28f,CreepSpeed=.3f;
}
void Game::updateDoorPlay(const InputState& input,float dt){
 m_doorRattle=std::max(0.f,m_doorRattle-dt);
 const int doors=int(m_world.doors().size());
 // The hand on the leaf: a tap swings it all the way; held, the leaf eases
 // under the hand and stays where it is let go (ajar, to look through).
 if(m_doorHand>=0){
  if(m_doorHand>=doors||!input.use||input.vrTracked){
   if(m_doorHand<doors&&m_doorHandTime>=HoldDelay&&m_world.doors()[size_t(m_doorHand)].swinging)m_world.restDoor(m_doorHand,m_world.doors()[size_t(m_doorHand)].open);
   m_doorHand=-1;
  }else{const auto&d=m_world.doors()[size_t(m_doorHand)];m_doorHandTime+=dt;
   if(m_doorHandTime>=HoldDelay&&d.swinging&&!d.broken){const float from=d.rest>=0?d.rest:d.open;m_world.restDoor(m_doorHand,from+(d.opening?1.f:-1.f)*CreepSpeed*dt);}}
 }
 // A shut leaf is latched: it wants the handle (E), and a locked one gives the
 // handle a rattle when walked into. One standing ajar is not: walking into it
 // from the side it opens away from pushes it open, at a walk on its way, at a
 // run shoved so it bangs off its stop. From the other side it would swing into
 // him: that wants the handle too.
 if(!input.vrTracked&&m_doorHand<0&&!draggingBody()){
  const Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)},right{-forward.y,forward.x};Vec2 wish{};
  if(input.forward)wish+=forward;if(input.back)wish+=forward*-1;if(input.right)wish+=right;if(input.left)wish+=right*-1;
  if(lengthSq(wish)>0){wish=normalized(wish);
   const float pace=input.sprint&&!m_player.crouched?2.6f:m_player.crouched?.75f:1.35f;
   for(int i=0;i<doors;++i){const auto&d=m_world.doors()[size_t(i)];
    if(!d.swinging||d.broken||d.entry||d.transfer||d.open>.35f||d.coasting||(d.opening&&d.velocity>.2f))continue;
    const float side=d.across(m_player.pos)-d.y;
    if(!d.spans(m_player.pos,.12f)||std::fabs(side)>.56f||side*d.swing>0)continue;
    const Vec2 c=d.centre();if(std::fabs(m_player.z-(m_world.floorHeight(c.x,c.y)+d.z))>.5f)continue;
    if(dot(wish,d.normal())*d.swing<.55f)continue;
    if(d.open<.02f){if(doorLocked(d)&&m_doorRattle<=0){m_sounds.push_back({Sound::LockerLatch,c,.38f,.7f,true,7,m_player.z+1.f});m_doorRattle=1.2f;}continue;}
    m_world.shoveDoor(i,pace);
   }
  }
 }
 // Each leaf's sounds, by what it is made of: the enamelled steel leaf, the
 // lighter painted panel, the grille. A slam carries; the latch and the creak
 // are heard close by. Creatures and guards hear them as they hear the player.
 for(int i=0;i<doors;++i){const int hit=m_world.takeDoorHit(i);if(!hit)continue;const auto&d=m_world.doors()[size_t(i)];
  const Vec2 c=d.swinging?d.hinge()+d.leafAxis(d.open)*(d.width()*.5f):d.centre();const float z=m_world.floorHeight(d.centre().x,d.centre().y)+d.z+1.1f;
  const float pitch=d.leaf==1?.72f:d.leaf==2?1.12f:.9f;
  auto emit=[&](Sound s,float gain,float p,float heard){SoundEvent e{s,c,gain,p,true,heard>8?14.f:8.f,z};e.aiRadius=heard;m_sounds.push_back(e);};
  if(hit&Door::HitStart)emit(Sound::LockerCreak,.2f,pitch*.85f,2.f);
  if(hit&Door::HitStop)emit(Sound::LockerShut,.26f,pitch*.82f,3.f);
  if(hit&Door::HitLatch)emit(Sound::LockerLatch,.42f,pitch,3.5f);
  if(hit&Door::HitSlam){emit(Sound::LockerBang,.95f,pitch*.8f,16.f);emit(Sound::LockerShut,.5f,pitch*.7f,16.f);}
 }
}
// A Brute, a Chimera or the Machine hunting the far side of a door that gives
// (Door::strength) beats it until it does: a blow each time its arm is ready,
// heard well beyond the room. A door that never gives (strength 0) it does not
// try; it goes round, as it always has.
bool Game::batterDoor(Enemy& e,Vec2 goal){
 if(!battersDoors(e.kind)||e.awareness<=0||e.attackCooldown>0||e.windup>0||e.strike>0)return false;
 for(int i=0;i<int(m_world.doors().size());++i){const auto&d=m_world.doors()[size_t(i)];
  if(!d.breakable()||d.broken||d.open>.3f)continue;
  const float off=d.across(e.pos)-d.y,toward=d.across(goal)-d.y;
  if(!d.spans(e.pos,-.1f)||std::fabs(off)>1.25f||off*toward>=0)continue;
  const Vec2 c=d.centre();const float base=m_world.floorHeight(c.x,c.y)+d.z;if(std::fabs(e.z-base)>.6f)continue;
  const Vec2 at=d.nearestOnLine(e.pos,.2f),to=at-e.pos;e.heading=std::atan2(to.y,to.x);
  const auto& traits=creatureTraits(e.kind);e.attackCooldown=std::max(1.1f,traits.cooldown);
  const bool push=off*d.swing<0,broke=m_world.batterDoor(i,traits.damage,push);
  // Its own blows are marked as its own (-2-index): every other creature and guard hears the door go,
  // but it does not take the din at the leaf for where its quarry went and stop beating.
  const int self=-2-int(&e-m_enemies.data());
  SoundEvent bang{Sound::LockerBang,c,1.f,broke?.62f:.74f,true,18,base+1.2f};bang.aiRadius=18;bang.owner=self;m_sounds.push_back(bang);
  if(broke){SoundEvent tear{Sound::OneMetalBang,c,1.f,.8f,true,20,base+1.f};tear.aiRadius=20;tear.owner=self;m_sounds.push_back(tear);}
  return true;
 }
 return false;
}
}

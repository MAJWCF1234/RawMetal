#include "Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace retro {
int Game::nearestRearHuman()const{
 Vec2 gaze{std::cos(m_player.angle),std::sin(m_player.angle)};
 int found=-1;float best=1.35f;
 for(int i=0;i<int(m_humans.size());++i){
  const auto&h=m_humans[i];if(!h.conscious()||std::fabs(h.z-m_player.z)>1.1f)continue;
  Vec2 toward=h.pos-m_player.pos;float d=length(toward);
  Vec2 actorForward{std::cos(h.yaw),std::sin(h.yaw)};
  // The player must approach from behind and be facing the target.
  if(d>=best||d<.01f||dot(gaze,toward)/d<.60f||dot(actorForward,toward)/d<.55f)continue;
  if(!m_world.rayClear(m_player.pos,m_player.z+1.f,h.pos,h.z+1.f))continue;
  best=d;found=i;
 }
 return found;
}
void Game::releaseHuman(bool lethal){
 if(m_heldHuman<0||m_heldHuman>=int(m_humans.size())){m_heldHuman=-1;return;}
 auto&h=m_humans[m_heldHuman];
 if(lethal){h.mind=Human::Mind::Dead;h.health=0;h.downTimer=0;setState(stateId("cleanup_security_lethal"),1);}
 else {h.mind=Human::Mind::Unconscious;h.downTimer=45.f;h.alert=0;}
 m_heldHuman=-1;m_pickupNotice=lethal?"HOSTILE NEUTRALIZED":"HOSTAGE RELEASED";m_pickupNoticeTime=2.f;
 sound(Sound::PunchHit,.45f,.75f);
}
bool Game::interactHuman(const InputState&input){
 if(m_heldHuman>=0){releaseHuman(false);return true;}
 int i=nearestRearHuman();if(i<0)return false;
 auto&h=m_humans[i];
 if(input.crouch){
  h.mind=Human::Mind::Unconscious;h.downTimer=45.f;h.alert=0;h.health=std::max(1.f,h.health);
  m_pickupNotice="SILENT TAKEDOWN / UNCONSCIOUS";
 }else{
  h.mind=Human::Mind::Grappled;h.alert=0;m_heldHuman=i;
  m_pickupNotice="HUMAN SHIELD / E RELEASE / R FINISH";
 }
 m_pickupNoticeTime=3.f;sound(Sound::PunchHit,.24f,.7f);
 return true;
}
void Game::updateHumans(const InputState& input,float dt){
 if(m_heldHuman>=0&&m_heldHuman<int(m_humans.size())){
  auto&h=m_humans[m_heldHuman];
  if(input.reload&&!m_previousReload){releaseHuman(true);}
  else{
   Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
   h.pos=m_player.pos+forward*.52f;h.z=m_player.z;
   h.yaw=m_player.angle;h.walkPhase+=dt*3.f;
  }
 }
 for(int i=0;i<int(m_humans.size());++i){
  auto&h=m_humans[i];if(i==m_heldHuman||h.mind==Human::Mind::Dead)continue;
  if(h.mind==Human::Mind::Unconscious){h.downTimer=std::max(0.f,h.downTimer-dt);if(h.downTimer==0){h.mind=Human::Mind::Suspicious;h.alert=2.f;}continue;}
  Vec2 toward=m_player.pos-h.pos;float dist=length(toward);
  bool sameFloor=std::fabs(h.z-m_player.z)<1.25f;
  Vec2 facing{std::cos(h.yaw),std::sin(h.yaw)};
  bool canSee=sameFloor&&dist<12.f&&dist>.001f&&dot(facing,toward)/dist>.42f&&m_world.rayClear(h.pos,h.z+1.2f,m_player.pos,m_player.z+m_player.eye);
  // Guards recognize an unassigned colleague; they react with suspicion before firing.
  if(canSee&&h.role==Human::Role::Guard){h.alert=std::min(10.f,h.alert+dt*(m_player.crouched?.65f:1.8f));h.target=m_player.pos;
   if(h.alert>3.f)h.mind=Human::Mind::Combat;else h.mind=Human::Mind::Suspicious;}
  for(const auto&noise:m_sounds){
   bool gunshot=noise.sound==Sound::Shot;
   bool thrown=noise.sound==Sound::JunkMetal||noise.sound==Sound::JunkGlass||noise.sound==Sound::JunkSoft;
   if(!gunshot&&!thrown)continue;
   Vec2 source=noise.spatial?noise.position:m_player.pos;
   if(lengthSq(source-h.pos)>(gunshot?196.f:64.f))continue;
   h.target=source;h.search=3.5f;
   if(h.mind!=Human::Mind::Combat)h.mind=Human::Mind::Investigate;
  }
  h.shotTimer=std::max(0.f,h.shotTimer-dt);
  if(h.mind==Human::Mind::Combat&&h.role==Human::Role::Guard){
   if(canSee&&dist<11.f&&h.shotTimer<=0){h.yaw=std::atan2(toward.y,toward.x);h.shotTimer=1.15f;
    sound(Sound::Shot,.55f,1.12f);
    bool shielded=hasHumanShield()&&m_heldHuman!=i;
    if(shielded){auto&shield=m_humans[m_heldHuman];shield.health=std::max(0.f,shield.health-36.f);if(shield.health==0){shield.mind=Human::Mind::Dead;m_heldHuman=-1;}}
    else receiveDamage(9.f,h.pos);
   }
   continue;
  }
  if(h.mind==Human::Mind::Investigate||h.mind==Human::Mind::Suspicious){
   Vec2 delta=h.target-h.pos;float d=length(delta);
   if(d>.25f&&h.mind==Human::Mind::Investigate){
    Vec2 step=delta/d*std::min(d,dt*1.2f);Vec2 next=h.pos+step;
    if(m_world.fits(next.x,next.y,h.z,1.6f)&&!m_world.doorBlocks(next.x,next.y,h.z,1.6f)){
     h.pos=next;h.walkPhase+=dt*5.f;
    }
    h.yaw=std::atan2(delta.y,delta.x);
   }
   if(!canSee&&h.search>0)h.search=std::max(0.f,h.search-dt);
   if(!canSee&&h.search==0){h.alert=std::max(0.f,h.alert-dt);if(h.alert==0)h.mind=Human::Mind::Idle;}
  }
 }
}
bool Game::testHumanStealth(){
 Game g;g.m_humans.clear();g.m_player.pos={4.5f,4.5f};g.m_player.z=0;g.m_player.angle=0;
 Human guard;guard.role=Human::Role::Guard;guard.armed=true;guard.pos={5.4f,4.5f};guard.home=guard.pos;guard.yaw=0;g.m_humans.push_back(guard);
 InputState crouch{};crouch.crouch=true;
 if(g.nearestRearHuman()!=0||!g.interactHuman(crouch)||g.m_humans[0].mind!=Human::Mind::Unconscious)return false;
 g.m_humans[0]=guard;InputState upright{};
 if(!g.interactHuman(upright)||!g.hasHumanShield())return false;
 g.releaseHuman(false);if(g.hasHumanShield()||g.m_humans[0].mind!=Human::Mind::Unconscious)return false;
 g.m_humans[0]=guard;g.m_humans[0].pos={7.5f,4.5f};g.m_humans[0].yaw=kPi;
 g.m_sounds.push_back({Sound::JunkMetal,{6.5f,4.5f},1.f,1.f,true});g.updateHumans({},.05f);
 if(g.m_humans[0].mind!=Human::Mind::Investigate)return false;
 std::ofstream("human-stealth-test.txt")<<"PASS: rear-only crouch knockout, grapple, nonlethal release and physics sound investigation\n";
 return true;
}
}

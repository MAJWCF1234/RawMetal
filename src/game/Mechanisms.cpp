#include "Game.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
namespace retro {
namespace {
constexpr float TerminalVoiceRadius=1.5f;
// Integer hash to [0,1): lamp flicker and torch sputter are pure functions of
// time, so every view, capture and replay of a moment agrees.
float lightNoise(std::uint32_t h){h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return float(h&0xffffffu)/16777216.f;}
std::uint32_t timeSlot(double t,double rate){double slot=std::fmod(std::floor(t*rate),4294967296.0);if(slot<0)slot+=4294967296.0;return std::uint32_t(slot);}
}
bool Game::lampSwitchedOn(const WorldLight& lamp)const{bool on=!lamp.enabledState||state(lamp.enabledState)==lamp.enabledValue;return lamp.invert?!on:on;}
float Game::lampPower(const WorldLight& lamp)const{
 if(!lampSwitchedOn(lamp))return 0.f;
 if(lamp.period>0){double t=std::fmod(double(m_elapsed)+lamp.phase,double(lamp.period));if(t<0)t+=lamp.period;if(t>=lamp.onTime)return 0.f;}
 if(lamp.flicker<=0)return 1.f;
 // A failing tube: mostly on, with stretches of rapid off-blinks between dark,
 // a buzzing dim and full, brownout holds, and at high flicker whole drop-outs.
 // The lamp's own position seeds it, so neighbours never fail in step.
 const float f=lamp.flicker;
 std::uint32_t seed=std::bit_cast<std::uint32_t>(lamp.position.x)*73856093u^std::bit_cast<std::uint32_t>(lamp.position.y)*19349663u^std::bit_cast<std::uint32_t>(lamp.z)*83492791u;
 double t=double(m_elapsed)+lamp.phase;
 std::uint32_t stretch=timeSlot(t,2.5);float chance=lightNoise(seed^stretch*0x9e3779b9u);
 if(chance<f*.45f){std::uint32_t blink=timeSlot(t,20.0);float b=lightNoise(seed*0x85ebca6bu^blink);
  return b<.4f?0.f:b<.7f?.18f+.3f*lightNoise(blink^seed^0x27d4eb2fu):1.f;}
 if(chance<f*.6f)return .4f+.25f*lightNoise(seed^stretch*0x165667b1u);
 if(chance>1.f-f*f*.12f)return 0.f;
 return 1.f;
}
bool Game::darkZoneActive(const DarkZone& zone)const{bool active=!zone.state||state(zone.state)==zone.value;return zone.invert?!active:active;}
float Game::darknessTarget(bool* jamming)const{
 float eye=m_player.z+m_player.eye;bool dark=false,jam=false;
 for(const auto& zone:m_world.darkZones())
  if(m_player.pos.x>=zone.x1&&m_player.pos.x<zone.x2&&m_player.pos.y>=zone.y1&&m_player.pos.y<zone.y2&&eye>=zone.bottom&&eye<zone.top&&darkZoneActive(zone)){dark=true;jam|=zone.jamFlashlight;}
 if(jamming)*jamming=jam;return dark?1.f:0.f;
}
// Before the first update after a load (inspection frames, a restored save)
// the target applies at once: a dark room never opens with a lit frame.
float Game::darkness()const{return m_darknessSettled?m_darkness:darknessTarget(nullptr);}
float Game::flashlightJam()const{if(m_darknessSettled)return m_flashlightJam;bool jam=false;darknessTarget(&jam);return jam?FlashlightJamTime:0.f;}
bool Game::flashlightJammed()const{
 float jam=flashlightJam();if(jam<=0)return false;if(jam>=FlashlightJamTime)return true;
 // The beam stutters, dropping out more often until it dies.
 float progress=jam/FlashlightJamTime;return lightNoise(timeSlot(m_elapsed,18.0)*0x9e3779b9u)<.2f+.7f*progress*progress;
}
void Game::updateDarkness(float dt){
 bool jamming=false;float target=darknessTarget(&jamming);
 if(!m_darknessSettled){m_darkness=target;m_flashlightJam=jamming?FlashlightJamTime:0.f;m_darknessSettled=true;return;}
 float jam=m_flashlightJam;m_darkness+=std::clamp(target-m_darkness,-dt/.3f,dt/.3f);
 m_flashlightJam=jamming?std::min(std::max(m_flashlightJam+dt,1e-3f),FlashlightJamTime):0.f;
 if(!hasFlashlight()||!state(stateId("flashlight_on")))return;
 const char* notice=jam<=0&&m_flashlightJam>0?"FLASHLIGHT / FAILING":jam<FlashlightJamTime&&m_flashlightJam>=FlashlightJamTime?"FLASHLIGHT / NO POWER":jam>0&&m_flashlightJam<=0?"FLASHLIGHT / ON":nullptr;
 if(notice){m_pickupNotice=notice;m_pickupNoticeTime=1.5f;}
}
float Game::muzzleFlash()const{
 // Only a real shot: the kick is set to 1 with it and falls 3/s as it ages
 // (a finished reload also restarts the shot clock, with a small kick).
 float age=m_shotAge,dark=darkness(),length=.05f+.07f*dark;
 if(unarmed()||age<0||m_weaponKick<=0||m_weaponKick+3.f*age<.97f)return 0.f;
 if(age<length)return (1.f-age/length)*(1.f+1.6f*dark);
 float tail=(age-length)/.2f;return tail<1?dark*.16f*(1-tail)*(1-tail):0.f;
}
std::array<std::array<float,4>,2> Game::weaponFlashes()const{
 std::array<std::array<float,4>,2> flashes{};const float dark=darkness();if(dark<=0)return flashes;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto& npc=m_friendlyActors[i];
  if(!chunkResident(npc.level)||!state(friendlyKey(i,"weapon_flash")))continue;
  auto weapon=npcWeaponPose(i);if(weapon.flash<=0)continue;auto origin=chunkOffset(npc.level);
  std::array<float,4> light{weapon.muzzle.x+origin.x,weapon.muzzle.y+origin.y,weapon.muzzle.z,weapon.flash*1.6f*dark};
  auto& weakest=flashes[0][3]<=flashes[1][3]?flashes[0]:flashes[1];if(light[3]>weakest[3])weakest=light;
 }
 return flashes;
}
std::vector<const SequenceCue*> Game::terminalMessages(int terminal)const{
 std::vector<const SequenceCue*> result;if(terminal<0||terminal>=int(m_world.terminals().size()))return result;
 const auto& at=m_world.terminals()[size_t(terminal)].position;const auto& sequences=m_world.timedSequences();
 auto order=[&](StateId timer){for(size_t i=0;i<sequences.size();++i)if(sequences[i].timerState==timer)return int(i);return int(sequences.size());};
 for(const auto& cue:m_world.sequenceCues())if(!cue.caption.empty()&&length(cue.position-at)<TerminalVoiceRadius&&state(cue.timerState)>cue.timeMs)result.push_back(&cue);
 std::stable_sort(result.begin(),result.end(),[&](const SequenceCue*a,const SequenceCue*b){int oa=order(a->timerState),ob=order(b->timerState);return oa!=ob?oa<ob:a->timeMs<b->timeMs;});
 return result;
}
// A timer's clock in ms. A finished one-shot rests at durationMs in its saved
// state; for captions and conversations its clock keeps counting from the end
// (after a load, a finished one counts as long over).
int Game::sequenceClock(const World& world,StateId timer)const{
 int clock=state(timer);
 for(const auto& s:world.timedSequences())if(s.timerState==timer){
  if(!s.loop&&clock>=s.durationMs){auto found=m_sequenceEnded.find(timer);clock=found==m_sequenceEnded.end()?1<<29:s.durationMs+int((m_elapsed-found->second)*1000);}
  break;}
 return clock;
}
bool Game::terminalConversationActive(int terminal)const{
 if(terminal<0||terminal>=int(m_world.terminals().size()))return false;
 const auto& at=m_world.terminals()[size_t(terminal)].position;
 for(const auto& cue:m_world.sequenceCues()){int age=sequenceClock(m_world,cue.timerState);
  if(!cue.caption.empty()&&length(cue.position-at)<TerminalVoiceRadius&&age>0&&age<=cue.timeMs+3000)return true;}
 return false;
}
std::string_view Game::actorCaption()const{
 for(int level=0;level<chunkCount();++level){if(!chunkResident(level))continue;const auto& world=level==m_level?m_world:m_chunks[level].world;
  for(const auto& cue:world.sequenceCues()){int clock=sequenceClock(world,cue.timerState),age=clock-cue.timeMs;if(!cue.caption.empty()&&clock>0&&age>=0&&age<2500)return cue.caption;} // A timer at 0 has not started.
 }return {};
}
float Game::alarmPulse()const{
 if(m_titleScreen)return 0;float pulse=0;
 for(const auto& sequence:m_world.timedSequences()){
  if(sequence.tickSound!=Sound::Alarm||sequence.soundIntervalMs<=0)continue;
  int age=state(sequence.timerState);if(age<=0||age>=sequence.soundUntilMs)continue;
  float decay=1.f-float(age%sequence.soundIntervalMs)/float(sequence.soundIntervalMs);pulse=std::max(pulse,decay*decay);
 }
 return pulse;
}
void Game::updateMechanisms(float dt){
 if(dt>0)m_npcNavBudget=4;
 const auto& lift=m_world.cargoLift();
 if(lift.upper>lift.lower){
  float old=m_world.cargoLiftHeight();
  float height=lift.lower+state(lift.positionState)*.001f;
  bool aboard=m_world.insideCargoLift(m_player.pos.x,m_player.pos.y)&&std::fabs(m_player.z-old)<.1f;
  if(state(lift.releaseState)&&aboard)setState(lift.downState,1);
  // The manual release is an actuator, not an invisible permission followed
  // by another E press at the gate. Open authored gates tied to that release.
  if(state(lift.releaseState)&&!state(lift.downState))for(int i=0;i<int(m_world.doors().size());++i)
   if(m_world.doors()[i].requireState==lift.releaseState)m_world.openDoor(i);
  float target=state(lift.downState)?lift.lower:state(lift.callState)?lift.upper:lift.lower;
  height=std::clamp(height+std::clamp(target-height,-lift.speed*dt,lift.speed*dt),lift.lower,lift.upper);
  m_world.setCargoLiftHeight(height);
  if(aboard&&dt>0){m_player.z+=height-old;m_player.verticalVelocity=0;m_player.grounded=true;}
  setState(lift.positionState,int(std::round((height-lift.lower)*1000)));
  if(height>=lift.upper-.01f)setState(lift.arrivedState,1);
  if(state(lift.downState)&&height<=lift.lower+.01f)setState(lift.descendedState,1);
 }
 for(int level=0;level<chunkCount();++level){if(!chunkResident(level))continue;
 auto& world=level==m_level?m_world:m_chunks[level].world;
 for(const auto& sequence:world.timedSequences()){
  int age=state(sequence.timerState);
  bool seen=true;
  if(age==0&&sequence.sightActor>=0){const auto& actor=world.actorTracks()[sequence.sightActor];auto pose=world.actorPose(sequence.sightActor);Vec2 delta=pose.position-m_player.pos;float height=pose.z+actor.scale*.65f-m_player.z-m_player.eye,range=std::sqrt(dot(delta,delta)+height*height),pitch=m_player.pitch/140.f;
   float alignment=range>0?(delta.x*std::cos(m_player.angle)*std::cos(pitch)+delta.y*std::sin(m_player.angle)*std::cos(pitch)+height*std::sin(pitch))/range:1;
   seen=range<=sequence.sightDistance&&alignment>.92f&&world.rayClear(m_player.pos,m_player.z+m_player.eye,pose.position,pose.z+actor.scale*.65f);
  }
  if(age==0&&seen&&level==m_level&&m_player.pos.x>=sequence.x1&&m_player.pos.x<=sequence.x2&&m_player.pos.y>=sequence.y1&&m_player.pos.y<=sequence.y2&&m_player.z>=sequence.bottom&&m_player.z<=sequence.top)age=1;
  if(age<=0)continue;
  // A finished one-shot rests at its end: nothing more fires (a cue at the
  // very end would otherwise replay every frame).
  if(!sequence.loop&&age>=sequence.durationMs)continue;
  int advanced=age+int(std::round(dt*1000));bool wrapped=sequence.loop&&advanced>sequence.durationMs;
  int next=sequence.loop?1+(advanced-1)%sequence.durationMs:std::min(sequence.durationMs,advanced);setState(sequence.timerState,next);
  if(sequence.soundIntervalMs>0&&age/sequence.soundIntervalMs!=next/sequence.soundIntervalMs&&next<sequence.soundUntilMs){
   Vec2 position{(sequence.x1+sequence.x2)*.5f,(sequence.y1+sequence.y2)*.5f};float z=world.floorHeight(position.x,position.y)+.04f;
   for(size_t i=0;i<world.actorTracks().size();++i)if(world.actorTracks()[i].timerState==sequence.timerState){auto pose=world.actorPose(i);position=pose.position;z=pose.z+.04f;break;}
   if(sequence.soundAt.x>=0){position=sequence.soundAt;z=std::isfinite(sequence.soundZ)?sequence.soundZ:world.floorHeight(position.x,position.y)+.04f;}
   m_sounds.push_back({sequence.tickSound,position+chunkOffset(level)-chunkOffset(m_level),sequence.gain,sequence.pitch,true,17,z});
  }
  // The clock starts at 1 (0 means not started), so a cue or finish at 0 or 1
  // is due on the first frame; a one-shot's last frame fires a cue at its end.
  bool ended=!sequence.loop&&next>=sequence.durationMs;
  int finishAt=std::max(sequence.finishAtMs,2);
  if((age<finishAt&&advanced>=finishAt)||wrapped)setState(sequence.finishState,1);
  if(!sequence.loop&&age<sequence.durationMs&&next>=sequence.durationMs)m_sequenceEnded[sequence.timerState]=m_elapsed;
  for(const auto& cue:world.sequenceCues()){if(cue.timerState!=sequence.timerState)continue;int at=std::max(cue.timeMs,1);
   if(!((age<=at&&(advanced>at||(ended&&at==sequence.durationMs)))||(wrapped&&next>at)))continue;
   auto position=chunkOffset(level)+cue.position-chunkOffset(m_level);
   m_sounds.push_back({cue.sound,position,cue.gain,cue.pitch,true,35,cue.z});m_sounds.back().aiRadius=cue.aiRadius;
   if(!cue.voice.empty())for(size_t clip=0;clip<NpcVoiceClips.size();++clip)if(cue.voice==NpcVoiceClips[clip].line){
    m_sounds.push_back({Sound::WorkerDying,position,cue.gain,1,true,35,cue.z,int(clip),0,2});break;}
  }
 }
 for(size_t i=0;i<world.actorTracks().size();++i){const auto& track=world.actorTracks()[i];auto old=world.actorPose(i),pose=sampleActor(track,state(track.timerState));world.setActorPose(i,pose);
  if(autonomousActor(track)||scriptedHandoffActive(level,i)){world.setActorPose(i,old);continue;}
  if(!track.idleKeys.empty()){
   auto idleState=actorTrackState(track.timerState,i,"idle_ms");int idle=state(idleState)+int(std::round(dt*1000));idle%=track.idleKeys.back().timeMs;setState(idleState,idle);
   int age=state(track.timerState);
   if(age<track.idleUntilMs)pose=sampleActorKeys(track.idleKeys,idle,true);
   else if(age<track.approachUntilMs){
    auto captured=actorTrackState(track.timerState,i,"approach_saved");
    if(!state(captured)){auto start=dt>0?old:sampleActorKeys(track.idleKeys,idle,true);setState(actorTrackState(track.timerState,i,"start_x"),int(std::round(start.position.x*1000)));setState(actorTrackState(track.timerState,i,"start_y"),int(std::round(start.position.y*1000)));setState(actorTrackState(track.timerState,i,"start_z"),int(std::round(start.z*1000)));setState(captured,1);}
    Vec2 start{state(actorTrackState(track.timerState,i,"start_x"))*.001f,state(actorTrackState(track.timerState,i,"start_y"))*.001f};float startZ=state(actorTrackState(track.timerState,i,"start_z"))*.001f;
    auto destination=sampleActor(track,track.approachUntilMs);float blend=float(age-track.idleUntilMs)/float(track.approachUntilMs-track.idleUntilMs);pose.position=start+(destination.position-start)*blend;pose.z=startZ+(destination.z-startZ)*blend;auto delta=destination.position-start;pose.yaw=std::atan2(delta.x,delta.y);pose.clip=1;pose.phase=std::fmod(float(age-track.idleUntilMs)/650.f,1.f);
   }
   world.setActorPose(i,pose);
  }
  if(pose.lookAtActor>=0&&dt>0){pose.yaw=old.yaw;world.setActorPose(i,pose);}
  if(track.deadState&&state(track.deadState)){pose.position={state(actorPositionState(track.deadState,0))*.001f,state(actorPositionState(track.deadState,1))*.001f};pose.z=state(actorPositionState(track.deadState,2))*.001f;pose.clip=4;pose.phase=1;pose.lookAtActor=-1;world.setActorPose(i,pose);}
  pose=friendlyAwareness(world,level,track,level,i,old,pose,dt);world.setActorPose(i,pose);
  if(dt>0&&level==m_level&&track.platform&&std::fabs(m_player.z-old.z)<.04f&&std::fabs(m_player.pos.x-old.position.x)<track.footprint.x*.5f&&std::fabs(m_player.pos.y-old.position.y)<track.footprint.y*.5f){
   auto target=m_player.pos+pose.position-old.position;float z=m_player.z+pose.z-old.z;
   if(hullFits(target,z,m_player.hullHeight())){m_player.pos=target;m_player.z=z;m_player.verticalVelocity=0;m_player.grounded=true;}
  }
 }
 for(size_t i=0;i<world.actorTracks().size();++i){auto pose=world.actorPose(i);if(pose.lookAtActor>=0){auto target=world.actorPose(pose.lookAtActor);auto delta=target.position-pose.position;if(dot(delta,delta)>.0025f){float yaw=std::atan2(delta.x,delta.y);pose.yaw=dt>0?pose.yaw+std::clamp(std::remainder(yaw-pose.yaw,2*kPi),-8*dt,8*dt):yaw;world.setActorPose(i,pose);}}}
 }
 updateFriendlyActors(dt);
}
}

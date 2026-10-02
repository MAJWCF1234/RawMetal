#include "Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
namespace retro {
std::string_view Game::actorCaption()const{
 for(int level=0;level<chunkCount();++level){if(!chunkResident(level))continue;const auto& world=level==m_level?m_world:m_chunks[level].world;
  for(const auto& cue:world.sequenceCues()){int age=state(cue.timerState)-cue.timeMs;if(!cue.caption.empty()&&age>=0&&age<2500)return cue.caption;}
 }return {};
}
void Game::updateMechanisms(float dt){
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
  if(age==0&&level==m_level&&m_player.pos.x>=sequence.x1&&m_player.pos.x<=sequence.x2&&m_player.pos.y>=sequence.y1&&m_player.pos.y<=sequence.y2&&m_player.z>=sequence.bottom&&m_player.z<=sequence.top)age=1;
  if(age<=0)continue;
  int advanced=age+int(std::round(dt*1000));bool wrapped=sequence.loop&&advanced>sequence.durationMs;
  int next=sequence.loop?1+(advanced-1)%sequence.durationMs:std::min(sequence.durationMs,advanced);setState(sequence.timerState,next);
  if(sequence.soundIntervalMs>0&&age/sequence.soundIntervalMs!=next/sequence.soundIntervalMs&&next<sequence.soundUntilMs)sound(sequence.tickSound,sequence.gain,sequence.pitch);
  if((age<sequence.finishAtMs&&advanced>=sequence.finishAtMs)||wrapped)setState(sequence.finishState,1);
  for(const auto& cue:world.sequenceCues())if(cue.timerState==sequence.timerState&&((age<=cue.timeMs&&advanced>cue.timeMs)||(wrapped&&next>cue.timeMs))){
   auto position=chunkOffset(level)+cue.position-chunkOffset(m_level);
   m_sounds.push_back({cue.sound,position,cue.gain,cue.pitch,true,35});
  }
 }
 for(size_t i=0;i<world.actorTracks().size();++i){const auto& track=world.actorTracks()[i];auto old=world.actorPose(i),pose=sampleActor(track,state(track.timerState));world.setActorPose(i,pose);
  if(track.deadState&&state(track.deadState)){pose.position={state(actorPositionState(track.deadState,0))*.001f,state(actorPositionState(track.deadState,1))*.001f};pose.z=state(actorPositionState(track.deadState,2))*.001f;pose.clip=4;pose.phase=1;world.setActorPose(i,pose);}
  if(dt>0&&level==m_level&&track.platform&&std::fabs(m_player.z-old.z)<.04f&&std::fabs(m_player.pos.x-old.position.x)<track.footprint.x*.5f&&std::fabs(m_player.pos.y-old.position.y)<track.footprint.y*.5f){
   auto target=m_player.pos+pose.position-old.position;float z=m_player.z+pose.z-old.z;
   if(hullFits(target,z,m_player.hullHeight())){m_player.pos=target;m_player.z=z;m_player.verticalVelocity=0;m_player.grounded=true;}
  }
 }
 }
}
}

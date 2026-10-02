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
  bool seen=true;
  if(age==0&&sequence.sightActor>=0){const auto& actor=world.actorTracks()[sequence.sightActor];auto pose=world.actorPose(sequence.sightActor);Vec2 delta=pose.position-m_player.pos;float height=pose.z+actor.scale*.65f-m_player.z-m_player.eye,range=std::sqrt(dot(delta,delta)+height*height),pitch=m_player.pitch/140.f;
   float alignment=range>0?(delta.x*std::cos(m_player.angle)*std::cos(pitch)+delta.y*std::sin(m_player.angle)*std::cos(pitch)+height*std::sin(pitch))/range:1;
   seen=range<=sequence.sightDistance&&alignment>.92f&&world.rayClear(m_player.pos,m_player.z+m_player.eye,pose.position,pose.z+actor.scale*.65f);
  }
  if(age==0&&seen&&level==m_level&&m_player.pos.x>=sequence.x1&&m_player.pos.x<=sequence.x2&&m_player.pos.y>=sequence.y1&&m_player.pos.y<=sequence.y2&&m_player.z>=sequence.bottom&&m_player.z<=sequence.top)age=1;
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
  if(dt>0&&level==m_level&&track.platform&&std::fabs(m_player.z-old.z)<.04f&&std::fabs(m_player.pos.x-old.position.x)<track.footprint.x*.5f&&std::fabs(m_player.pos.y-old.position.y)<track.footprint.y*.5f){
   auto target=m_player.pos+pose.position-old.position;float z=m_player.z+pose.z-old.z;
   if(hullFits(target,z,m_player.hullHeight())){m_player.pos=target;m_player.z=z;m_player.verticalVelocity=0;m_player.grounded=true;}
  }
 }
 for(size_t i=0;i<world.actorTracks().size();++i){auto pose=world.actorPose(i);if(pose.lookAtActor>=0){auto target=world.actorPose(pose.lookAtActor);auto delta=target.position-pose.position;if(dot(delta,delta)>.0025f){float yaw=std::atan2(delta.x,delta.y);pose.yaw=dt>0?pose.yaw+std::clamp(std::remainder(yaw-pose.yaw,2*kPi),-8*dt,8*dt):yaw;world.setActorPose(i,pose);}}}
 }
}
}

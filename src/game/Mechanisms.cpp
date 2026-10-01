#include "Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
namespace retro {
void Game::updateMechanisms(float dt){
 const auto& lift=m_world.cargoLift();
 if(lift.upper>lift.lower){
  float old=m_world.cargoLiftHeight();
  float height=lift.lower+state(lift.positionState)*.001f;
  bool aboard=m_world.insideCargoLift(m_player.pos.x,m_player.pos.y)&&std::fabs(m_player.z-old)<.1f;
  if(state(lift.releaseState)&&aboard)setState(lift.downState,1);
  float target=state(lift.downState)?lift.lower:state(lift.callState)?lift.upper:lift.lower;
  height=std::clamp(height+std::clamp(target-height,-lift.speed*dt,lift.speed*dt),lift.lower,lift.upper);
  m_world.setCargoLiftHeight(height);
  if(aboard&&dt>0){m_player.z+=height-old;m_player.verticalVelocity=0;m_player.grounded=true;}
  setState(lift.positionState,int(std::round((height-lift.lower)*1000)));
  if(height>=lift.upper-.01f)setState(lift.arrivedState,1);
  if(state(lift.downState)&&height<=lift.lower+.01f)setState(lift.descendedState,1);
 }
 for(const auto& sequence:m_world.timedSequences()){
  int age=state(sequence.timerState);
  if(age==0&&m_player.pos.x>=sequence.x1&&m_player.pos.x<=sequence.x2&&m_player.pos.y>=sequence.y1&&m_player.pos.y<=sequence.y2&&m_player.z>=sequence.bottom&&m_player.z<=sequence.top)age=1;
  if(age<=0)continue;
  int next=std::min(sequence.durationMs,age+int(std::round(dt*1000)));setState(sequence.timerState,next);
  if(sequence.soundIntervalMs>0&&age/sequence.soundIntervalMs!=next/sequence.soundIntervalMs&&next<sequence.soundUntilMs)sound(sequence.tickSound,sequence.gain,sequence.pitch);
  if(age<sequence.finishAtMs&&next>=sequence.finishAtMs)setState(sequence.finishState,1);
 }
}
}

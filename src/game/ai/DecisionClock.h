#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace retro::ai {
// Simulation-time deadlines, never wall clocks. A wake runs immediately without
// shifting the periodic deadline; actors retain their stagger after a shot.
struct DecisionClock {
 struct State {int remaining=0,elapsed=0,ready=0;};
 struct Step {bool due;int elapsed;};
 struct Statistics {uint64_t polls=0,runs=0,wakes=0;};
 inline static thread_local std::array<Statistics,2> statistics{};
 static Step advance(State&state,int ms,int period,int phase,bool wake,int lane){
  auto&stats=statistics[size_t(lane)];++stats.polls;
  state.elapsed=std::min(1000,state.elapsed+ms);
  state.remaining-=ms;bool due=!state.ready||wake||state.remaining<=0;
  if(!due)return {false,0};
  ++stats.runs;if(wake)++stats.wakes;
  if(!state.ready){state.ready=1;state.remaining=1+phase%period;}
  else if(state.remaining<=0)state.remaining=period-(-state.remaining%period);
  int elapsed=state.elapsed;state.elapsed=0;return {true,elapsed};
 }
 static void resetStatistics(){statistics={};}
};
}

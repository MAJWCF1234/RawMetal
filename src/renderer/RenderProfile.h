#pragma once
#include "../core/FrameProfile.h"
namespace retro::perf {
inline thread_local std::array<double,3> actorCosts{};
inline thread_local bool actorMeshActive=false;
inline thread_local unsigned actorTriangleCounter=0;
struct ActorScope {
 bool active;size_t index;std::chrono::steady_clock::time_point start;
 explicit ActorScope(size_t slot):active(enabled),index(slot){if(active){start=std::chrono::steady_clock::now();if(index==1)actorMeshActive=true;}}
 ~ActorScope(){if(active){actorCosts[index]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(index==1)actorMeshActive=false;}}
};
struct ActorTriangleScope {
 bool active;std::chrono::steady_clock::time_point start;
 ActorTriangleScope():active(enabled&&actorMeshActive&&((actorTriangleCounter++&63u)==0)){if(active)start=std::chrono::steady_clock::now();}
 ~ActorTriangleScope(){if(active)actorCosts[2]+=64*std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
};
}

#include "../world/ScriptDefinition.h"
#include <algorithm>
#include <cmath>
namespace retro {
ActorPose sampleActor(const ActorTrack& track,int ageMs){
 return sampleActorKeys(track.keys,ageMs,track.loop);
}
ActorPose sampleActorKeys(const std::vector<ActorKey>& keys,int ageMs,bool loop){
 if(keys.empty())return {};
 if(loop&&keys.back().timeMs>0)ageMs%=keys.back().timeMs;
 auto next=std::upper_bound(keys.begin(),keys.end(),ageMs,[](int age,const ActorKey& key){return age<key.timeMs;});
 const auto& a=next==keys.begin()?keys.front():*(next-1);
 const auto& b=next==keys.end()?a:*next;
 float t=b.timeMs>a.timeMs?std::clamp(float(ageMs-a.timeMs)/float(b.timeMs-a.timeMs),0.f,1.f):0;
 float angle=std::remainder(b.yaw-a.yaw,6.283185307f);
 float phase=a.clip==b.clip?a.phase+(b.phase-a.phase)*t:a.phase+t*(1-a.phase);
 if(a.phase>1||b.phase>1)phase-=std::floor(phase);
 return {a.position+(b.position-a.position)*t,a.z+(b.z-a.z)*t,a.yaw+angle*t,a.clip,phase,a.lookAtActor};
}
}

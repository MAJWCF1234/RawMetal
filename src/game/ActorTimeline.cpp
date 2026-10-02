#include "../world/ScriptDefinition.h"
#include <algorithm>
#include <cmath>
namespace retro {
ActorPose sampleActor(const ActorTrack& track,int ageMs){
 if(track.keys.empty())return {};
 if(track.loop&&track.keys.back().timeMs>0)ageMs%=track.keys.back().timeMs;
 auto next=std::upper_bound(track.keys.begin(),track.keys.end(),ageMs,[](int age,const ActorKey& key){return age<key.timeMs;});
 const auto& a=next==track.keys.begin()?track.keys.front():*(next-1);
 const auto& b=next==track.keys.end()?a:*next;
 float t=b.timeMs>a.timeMs?std::clamp(float(ageMs-a.timeMs)/float(b.timeMs-a.timeMs),0.f,1.f):0;
 float angle=std::remainder(b.yaw-a.yaw,6.283185307f);
 return {a.position+(b.position-a.position)*t,a.z+(b.z-a.z)*t,a.yaw+angle*t,a.clip,a.clip==b.clip?a.phase+(b.phase-a.phase)*t:a.phase+t*(1-a.phase)};
}
}

// The Analysis chapter kit (maps 79-84), shared by CampaignChapterAnalysis.cpp
// (79, 80, 83, 84) and CampaignChapterAnalysisLabs.cpp (81, 82) so the two files
// can be authored and built side by side.
#pragma once
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include "ModelIds.h"
namespace retro::an {
using namespace kit;
inline constexpr float anS=0,anN=kPi,anE=kPi*.5f,anW=-kPi*.5f;
inline void anFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
// A pack model at its own proportions (times s).
inline void anModel(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];anFix(m,model,x,y,z,v[0]*s,v[1]*s,v[2]*s,yaw,solid);
}
// The hum of a live pool: AnomalyHum every 10 s from the box, for an hour once the player
// first stands in it (the water chapter's live pools, the same way).
inline void anHum(Map& m,const char* timer,float x1,float y1,float x2,float y2,float bottom,float top,float gain=.6f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_done");
 s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;s.bottom=bottom;s.top=top;s.durationMs=3600000;s.finishAtMs=3600000;
 s.soundIntervalMs=10000;s.soundUntilMs=3000000;s.tickSound=Sound::AnomalyHum;s.gain=gain;s.pitch=1;m.timedSequences.push_back(s);
}
}

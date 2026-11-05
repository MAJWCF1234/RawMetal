#pragma once
#include "../../core/FrameProfile.h"
#include "../../world/World.h"
#include <algorithm>
#include <cmath>
namespace retro {
// How fast an observer makes the player out, against its full rate (1): within
// 5 m nothing saves them; beyond it distance (to .3 at the end of sight), a
// crouch (x.6), keeping still (x.7; a sprint x1.4), the edge of view (x.5,
// coneDot the facing dot to them) and shadow (x.35 unlit, by lightWeight: 1 a
// guard or a sight hunter, 0 a creature that hunts by ear). Guards and
// creatures make the player out by this one formula.
inline float aiRecognitionRate(float distance,float sightRange,float coneDot,float sightDot,bool crouched,float speed,float exposure,float lightWeight=1){
 if(distance<=5.f)return 1.f;
 float rate=1.f-.7f*std::clamp((distance-5.f)/std::max(1.f,sightRange-5.f),0.f,1.f);
 if(crouched)rate*=.6f;
 if(speed<.3f)rate*=.7f;else if(speed>4.2f)rate*=1.4f;
 if(coneDot<(sightDot+1.f)*.5f)rate*=.5f;
 return rate*std::lerp(1.f,.35f+.65f*std::min(1.f,exposure*1.6f),lightWeight);
}
// Shared visibility and hearing for enemies and friendly actor reactions.
inline bool aiCanSee(const World& world,Vec2 observer,float eye,Vec2 forward,Vec2 target,float targetZ,float range,float facingDot,float closeRange=0){
 perf::count(perf::AiCanSee);
 auto delta=target-observer;float distanceSq=lengthSq(delta),height=targetZ-eye;
 if(distanceSq+height*height>=range*range)return false;
 if(distanceSq>closeRange*closeRange&&dot(normalized(delta),forward)<=facingDot)return false;
 return world.rayClear(observer,eye,target,targetZ);
}
inline float aiSoundStrength(Vec2 observer,float eye,Vec2 source,float sourceZ,float radius,float gain,bool loud,bool clear){
 if(radius<=0||gain<=0)return 0;
 float distance=std::sqrt(lengthSq(source-observer)+(sourceZ-eye)*(sourceZ-eye));
 if(distance>=radius)return 0;
 if(!clear)radius*=loud?.7f:.35f;
 return distance<radius?gain*(1.f-distance/radius):0.f;
}
inline float aiSoundStrength(const World& world,Vec2 observer,float eye,Vec2 source,float sourceZ,float radius,float gain,bool loud){
 float dz=sourceZ-eye;if(radius<=0||gain<=0||lengthSq(source-observer)+dz*dz>=radius*radius)return 0;
 return aiSoundStrength(observer,eye,source,sourceZ,radius,gain,loud,world.rayClear(observer,eye,source,sourceZ));
}
}

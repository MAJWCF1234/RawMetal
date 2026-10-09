#pragma once
#include "../world/World.h"
namespace retro {
// Shared visibility and hearing for enemies and friendly actor reactions.
inline bool aiCanSee(const World& world,Vec2 observer,float eye,Vec2 forward,Vec2 target,float targetZ,float range,float facingDot,float closeRange=0){
 auto delta=target-observer;float distanceSq=lengthSq(delta),height=targetZ-eye;
 if(distanceSq+height*height>=range*range)return false;
 if(distanceSq>closeRange*closeRange&&dot(normalized(delta),forward)<=facingDot)return false;
 return world.rayClear(observer,eye,target,targetZ);
}
inline float aiSoundStrength(const World& world,Vec2 observer,float eye,Vec2 source,float sourceZ,float radius,float gain,bool loud){
 if(radius<=0||gain<=0)return 0;
 float distance=std::sqrt(lengthSq(source-observer)+(sourceZ-eye)*(sourceZ-eye));
 if(distance>=radius)return 0;
 if(!world.rayClear(observer,eye,source,sourceZ))radius*=loud?.7f:.35f;
 return distance<radius?gain*(1.f-distance/radius):0.f;
}
}

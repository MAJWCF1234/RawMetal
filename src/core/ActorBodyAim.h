#pragma once
#include "Math.h"

namespace retro {
// Actor-local +Y is up and +Z is forward. Physics and split animation must
// use the same limited spine rotation when fitting a weapon to its shoulder.
struct ActorBodyAimRotation {
 float cy,sy,cp,sp;
 ActorBodyAimRotation(float yaw,float pitch){
  yaw=std::clamp(yaw,-.70f,.70f)*.85f;pitch=std::clamp(pitch,-.45f,.55f)*.25f;
  cy=std::cos(yaw);sy=std::sin(yaw);cp=std::cos(pitch);sp=std::sin(pitch);
 }
 Point3 operator()(Point3 local)const{
  Point3 raised{local.x,local.y*cp+local.z*sp,-local.y*sp+local.z*cp};
  return {raised.x*cy+raised.z*sy,raised.y,-raised.x*sy+raised.z*cy};
 }
};
inline Point3 actorBodyAim(Point3 local,float yaw,float pitch){
 return ActorBodyAimRotation(yaw,pitch)(local);
}
}

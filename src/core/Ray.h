#pragma once
#include "Math.h"

namespace retro {
// Nearest entry along the finite segment origin + delta * t. A fraction of
// one means no hit before the endpoint; a solid containing the origin hits at
// zero. The normal always faces out of the entered slab.
inline float rayBoxFraction(Point3 origin,Point3 delta,Point3 lo,Point3 hi,Point3* normal=nullptr){
 if(normal)*normal={};
 if(!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z)||!std::isfinite(delta.x)||!std::isfinite(delta.y)||!std::isfinite(delta.z)||lo.x>hi.x||lo.y>hi.y||lo.z>hi.z)return 1.f;
 float nearT=0,farT=1;Point3 entry{};
 auto slab=[&](float p,float v,float lower,float upper,Point3 negative,Point3 positive){
  if(std::fabs(v)<1.e-8f)return p>=lower&&p<=upper;
  float first=(lower-p)/v,last=(upper-p)/v;Point3 face=negative;
  if(first>last){std::swap(first,last);face=positive;}
  if(first>nearT){nearT=first;entry=face;}farT=std::min(farT,last);return nearT<=farT;
 };
 if(!slab(origin.x,delta.x,lo.x,hi.x,{-1,0,0},{1,0,0})||!slab(origin.y,delta.y,lo.y,hi.y,{0,-1,0},{0,1,0})||!slab(origin.z,delta.z,lo.z,hi.z,{0,0,-1},{0,0,1})||farT<0||nearT>=1)return 1.f;
 if(nearT==0){
  // An origin in/on a solid still needs a usable impact normal.
  if(std::fabs(delta.x)>=std::fabs(delta.y)&&std::fabs(delta.x)>=std::fabs(delta.z))entry={delta.x>0?-1.f:1.f,0,0};
  else if(std::fabs(delta.y)>=std::fabs(delta.z))entry={0,delta.y>0?-1.f:1.f,0};
  else entry={0,0,delta.z>0?-1.f:1.f};
 }
 if(normal)*normal=entry;return nearT;
}
}

#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

namespace creation {
// Authoring clearances use native dimensions and the engine's yaw convention.
// This is a placement query, not a renderer or an alternate map format.
struct Box {
  float x,y,w,d,bottom,top,yaw=0;
  float halfX() const {return (std::abs(std::cos(yaw))*w+std::abs(std::sin(yaw))*d)*.5f;}
  float halfY() const {return (std::abs(std::sin(yaw))*w+std::abs(std::cos(yaw))*d)*.5f;}
};
inline bool overlaps(const Box& a,const Box& b,float gap=0){
  if(a.top<=b.bottom+.001f||b.top<=a.bottom+.001f)return false;
  const float axes[][2]={{std::cos(a.yaw),-std::sin(a.yaw)},{std::sin(a.yaw),std::cos(a.yaw)},
                        {std::cos(b.yaw),-std::sin(b.yaw)},{std::sin(b.yaw),std::cos(b.yaw)}};
  for(const auto& axis:axes){
    auto extent=[&](const Box& r){return .5f*(r.w*std::abs(axis[0]*std::cos(r.yaw)-axis[1]*std::sin(r.yaw))+
                                            r.d*std::abs(axis[0]*std::sin(r.yaw)+axis[1]*std::cos(r.yaw)));};
    if(std::abs((a.x-b.x)*axis[0]+(a.y-b.y)*axis[1])>=extent(a)+extent(b)+gap-.0001f)return false;
  }
  return true;
}
inline float wallYaw(int model,int side){
  // Imported models face +Y at yaw zero; the engine's original models face -Y.
  constexpr float yaw[]={0,3.14159265359f,1.57079632679f,-1.57079632679f};
  return std::remainder(yaw[side]+(model<63?3.14159265359f:0),6.28318530718f);
}
// Subtract an axis-aligned wall volume while keeping the old structure on
// every other side, including floors above/below the active editing floor.
inline std::vector<Box> subtract(const Box& a,const Box& cut){
  float x0=a.x-a.w*.5f,x1=a.x+a.w*.5f,y0=a.y-a.d*.5f,y1=a.y+a.d*.5f;
  float ix0=std::max(x0,cut.x-cut.w*.5f),ix1=std::min(x1,cut.x+cut.w*.5f);
  float iy0=std::max(y0,cut.y-cut.d*.5f),iy1=std::min(y1,cut.y+cut.d*.5f);
  float iz0=std::max(a.bottom,cut.bottom),iz1=std::min(a.top,cut.top);
  if(ix1<=ix0+.0001f||iy1<=iy0+.0001f||iz1<=iz0+.0001f)return {a};
  std::vector<Box> out;auto add=[&](float l,float r,float n,float s,float b,float t){
    if(r-l>.0001f&&s-n>.0001f&&t-b>.0001f)out.push_back({(l+r)*.5f,(n+s)*.5f,r-l,s-n,b,t});};
  add(x0,ix0,y0,y1,a.bottom,a.top);add(ix1,x1,y0,y1,a.bottom,a.top);
  add(ix0,ix1,y0,iy0,a.bottom,a.top);add(ix0,ix1,iy1,y1,a.bottom,a.top);
  add(ix0,ix1,iy0,iy1,a.bottom,iz0);add(ix0,ix1,iy0,iy1,iz1,a.top);
  return out;
}
}

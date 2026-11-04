#pragma once
#include "../core/Math.h"
#include <algorithm>
#include <span>
#include <vector>

namespace retro {
struct SurfaceBox {Point3 low,high;int owner;};
struct SurfacePatch {float u0=0,v0=0,u1=1,v1=1;};
// The visible rectangles of an axis-aligned face. Buried and shared faces are
// removed; a later authored skin owns coincident exterior faces. UVs stay in the
// original face's coordinates, so trimming cannot stretch or shift its texture.
inline std::vector<SurfacePatch> exposedBoxFace(Point3 a,Point3 b,Point3 d,int owner,std::span<const SurfaceBox> boxes){
 auto component=[](Point3 p,int axis){return axis==0?p.x:axis==1?p.y:p.z;};
 auto u=b-a,v=d-a;Point3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};int axis=std::fabs(n.x)>.000001f?0:std::fabs(n.y)>.000001f?1:2;
 if(std::fabs(component(n,axis))<.000001f)return {{}};
 int ua=std::fabs(u.x)>.000001f?0:std::fabs(u.y)>.000001f?1:2,va=std::fabs(v.x)>.000001f?0:std::fabs(v.y)>.000001f?1:2;
 // Sloped, cylindrical and mesh faces retain their original geometry.
 if(ua==va||std::fabs(component(u,axis))>.000001f||std::fabs(component(v,axis))>.000001f)return {{}};
 if(std::fabs(u.x)+std::fabs(u.y)+std::fabs(u.z)-std::fabs(component(u,ua))>.000001f||
    std::fabs(v.x)+std::fabs(v.y)+std::fabs(v.z)-std::fabs(component(v,va))>.000001f)return {{}};
 float plane=component(a,axis),out=component(n,axis),du=component(u,ua),dv=component(v,va);
 // Grid wall quads and box quads use opposite winding on their side faces.
 // The owning volume determines the physical exterior in either case.
 for(const auto&box:boxes)if(box.owner==owner){
  if(std::fabs(plane-component(box.low,axis))<.0001f)out=-1;
  else if(std::fabs(plane-component(box.high,axis))<.0001f)out=1;
  break;
 }
 std::vector<SurfacePatch> patches{{}};
 for(const auto&box:boxes){
  float lo=component(box.low,axis),hi=component(box.high,axis);constexpr float epsilon=.0001f;
  // A dado/paint split is still one solid: omit the two caps inside it.
  if(box.owner==owner){if(plane>lo+epsilon&&plane<hi-epsilon)return {};continue;}
  bool buried=out>0?lo<=plane+epsilon&&hi>plane+epsilon:hi>=plane-epsilon&&lo<plane-epsilon;
  bool coincident=box.owner>owner&&(out>0?std::fabs(hi-plane)<=epsilon&&lo<plane-epsilon:std::fabs(lo-plane)<=epsilon&&hi>plane+epsilon);
  if(!buried&&!coincident)continue;
  float u0=(component(box.low,ua)-component(a,ua))/du,u1=(component(box.high,ua)-component(a,ua))/du;
  float v0=(component(box.low,va)-component(a,va))/dv,v1=(component(box.high,va)-component(a,va))/dv;
  if(u0>u1)std::swap(u0,u1);if(v0>v1)std::swap(v0,v1);
  if(u1<=0||u0>=1||v1<=0||v0>=1)continue;
  std::vector<SurfacePatch> next;
  for(auto p:patches){float l=std::max(p.u0,u0),r=std::min(p.u1,u1),bottom=std::max(p.v0,v0),top=std::min(p.v1,v1);
   if(r-l<epsilon||top-bottom<epsilon){next.push_back(p);continue;}
   if(l-p.u0>epsilon)next.push_back({p.u0,p.v0,l,p.v1});
   if(p.u1-r>epsilon)next.push_back({r,p.v0,p.u1,p.v1});
   if(bottom-p.v0>epsilon)next.push_back({l,p.v0,r,bottom});
   if(p.v1-top>epsilon)next.push_back({l,top,r,p.v1});
  }
  patches.swap(next);if(patches.empty())break;
 }
 return patches;
}
}

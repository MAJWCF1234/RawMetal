#pragma once
#include <cmath>
namespace retro {
// Horizontal field of view is fixed; vertical field follows the render target.
struct Projection {
 float aspect;
 static constexpr float horizontal=1.3f, nearPlane=.06f;
 constexpr float vertical()const{return horizontal*aspect;}
 constexpr float focal(float width)const{return width*(horizontal*.5f);}
 unsigned outside(float x,float y,float z)const{
  return (z<nearPlane?1u:0u)|(z+x*horizontal<0?2u:0u)|(z-x*horizontal<0?4u:0u)|(z+y*vertical()<0?8u:0u)|(z-y*vertical()<0?16u:0u);
 }
};
}

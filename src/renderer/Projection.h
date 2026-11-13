#pragma once
#include <cmath>
namespace retro {
// Horizontal field of view: the game's own is 1.3 (about 75 degrees across); the vertical
// field follows the render target. Only a film take (--film's lens, the Campaign Builder's
// Filmmaker preview) changes it, through setFieldOfView; play always draws at 1.3.
struct Projection {
 float aspect;
 static constexpr float gameHorizontal=1.3f, nearPlane=.06f;
 static inline float horizontal=gameHorizontal;
 float vertical()const{return horizontal*aspect;}
 float focal(float width)const{return width*(horizontal*.5f);}
 // degrees across the frame; anything outside 5..170 (0 among them) is the game's own lens.
 static void setFieldOfView(float degrees){horizontal=degrees>=5.f&&degrees<=170.f?1.f/std::tan(degrees*3.14159265f/360.f):gameHorizontal;}
 static float fieldOfView(){return 360.f/3.14159265f*std::atan(1.f/horizontal);}
 unsigned outside(float x,float y,float z)const{
  return (z<nearPlane?1u:0u)|(z+x*horizontal<0?2u:0u)|(z-x*horizontal<0?4u:0u)|(z+y*vertical()<0?8u:0u)|(z-y*vertical()<0?16u:0u);
 }
};
}

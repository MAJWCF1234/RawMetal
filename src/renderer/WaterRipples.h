#pragma once
// Rings on standing water. Each WaterSplash the game keeps (a round, a thing or a body falling in, a
// step, a wake; WaterFx.cpp) sends rings out over the surface: a few close rings for a fall, one
// small one for a step, spreading and fading over a second or two, any number at once. Scene3D
// cuts the water's surface finer where a ring runs and lifts and tilts it there (the ring's height
// moves the mesh, its slope the normal the lamps and the reflection see).
#include "../game/Game.h"
#include <vector>
namespace retro::ripples {
struct Ring {float x=0,y=0,born=0,amplitude=0,speed=.4f,wavelength=.14f,life=2;};
struct Field {
 std::vector<Ring> rings;float now=0;
 // Metres over the still surface, and its slope (dh/dx, dh/dy).
 float height(float x,float y)const;
 Vec2 slope(float x,float y)const;
 // Whether any ring's band reaches into the rectangle, and the box round every ring (false if none).
 bool touches(float x1,float y1,float x2,float y2)const;
 bool bounds(float& x1,float& y1,float& x2,float& y2)const;
 bool empty()const{return rings.empty();}
};
// The rings alive on `level` at `now` (newest kept when there are more than `most`).
Field gather(const std::vector<WaterSplash>& splashes,int level,float now,size_t most=40);
// The band a ring is in at `age`: its radius and the half width of the train of waves round it.
float radius(const Ring& r,float age);
float halfWidth(const Ring& r,float age);
// A basin's surface (its rectangle) cut for drawing: still cells drawn as before, and cells a ring
// crosses cut into `fine` x `fine` (at most `budget` small cells in all; coarser beyond).
struct Cut {struct Cell {float x1,y1,x2,y2;};std::vector<Cell> still,moving;int fine=8;};
Cut cut(const Field& field,float left,float near,float right,float far,float coarse=.5f,int fine=16,int budget=14000);
}

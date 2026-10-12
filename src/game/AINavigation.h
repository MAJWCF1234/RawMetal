#pragma once
#include "../world/World.h"
#include "../core/Ray.h"
namespace retro {
inline bool aiStructureSegment(const World& world,Vec2 start,float feet,Vec2 goal,float height){
 for(const auto&s:world.structures()){
  // Low supports may be stepped onto by the support solver. Taller solid
  // obstacles require a swept footprint, including between sample points.
  if(s.top<=feet+.265f||s.bottom>=feet+height-.005f)continue;
  if(rayBoxFraction({start.x,start.y,feet},{goal.x-start.x,goal.y-start.y,0},
    {s.x1-.1999f,s.y1-.1999f,s.bottom-height+.005f},{s.x2+.1999f,s.y2+.1999f,s.top-.025f})<1)return false;
 }return true;
}
bool aiWalkSegment(const World& world,Vec2 start,float feet,Vec2 goal,float height,float* endFeet=nullptr);
Vec2 aiWalkWaypoint(const World& world,Vec2 start,float feet,Vec2 goal,float goalFeet,float height);
}

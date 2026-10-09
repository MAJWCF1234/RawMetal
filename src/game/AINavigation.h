#pragma once
#include "../world/World.h"
namespace retro {
bool aiWalkSegment(const World& world,Vec2 start,float feet,Vec2 goal,float height,float* endFeet=nullptr);
Vec2 aiWalkWaypoint(const World& world,Vec2 start,float feet,Vec2 goal,float goalFeet,float height);
}

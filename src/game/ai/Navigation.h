#include "../../core/FrameProfile.h"
#pragma once
#include "../../world/World.h"
#include "../../core/Ray.h"
namespace retro {
// Footprint is ±0.2. Sample pitches at/above that keep footprints overlapping;
// structureSegmentClear owns solids between samples. GoldSrc paid one hull move
// per step, not an 8 cm density lattice along every AI probe.
inline float aiWalkSamplePitch(bool terrain){return terrain?.16f:.28f;}
inline bool aiStructureSegment(const World& world,Vec2 start,float feet,Vec2 goal,float height,bool indexed=true){
 perf::Scope queryProfile(perf::Structure);
 return world.structureSegmentClear(start,feet,goal,height,indexed);
}
bool aiWalkSegment(const World& world,Vec2 start,float feet,Vec2 goal,float height,float* endFeet=nullptr);
Vec2 aiWalkWaypoint(const World& world,Vec2 start,float feet,Vec2 goal,float goalFeet,float height);
}

#pragma once
// Props in standing water (WATER records; Clutter.cpp's simulateClutter calls step each substep).
// What floats and what sinks goes by density: the prop's mass (PropTraits) over its box, held to its
// material's range (a crate is mostly air but floats deep as wood does; a metal can floods and sinks).
// The water lifts by the share of the prop under the surface, so a float rides at its density's
// depth and bobs about it; drag (linear and by the square of the speed, larger for a broad light
// thing) slows a throw and lets a sinker down slowly to the bed. Afloat, a swell rocks it and
// tilts it a few degrees and a slow drift carries it about. All of it follows from position,
// velocity and the game clock, so a save keeps a floating prop as it keeps any other.
#include "Game.h"
namespace retro::buoyancy {
inline constexpr float WaterDensity=1000.f,Gravity=14.f;
float density(const Clutter& c);        // kg/m^3, as the water takes it
bool floats(const Clutter& c);          // lighter than water
float drag(const Clutter& c);           // the quadratic drag (per metre) of its cross-section on its mass
struct Result {
 float immersed=0;     // the share of its height under the surface (0 dry)
 bool afloat=false;    // floats and the water is deep enough to carry it
 bool entered=false;   // its base went under the surface during the last substep, falling fast
 float speed=0;        // its speed then
};
// One substep's water forces (gravity is already applied): lift, drag, swell, righting and drift.
// `surface` is the water over its base (World::waterSurface, -1000 dry), `bed` what it would rest on.
Result step(Clutter& c,float surface,float bed,float now,int seed,float dt);
// How hard a thing of `mass` meeting the water at `speed` splashes (WaterSplash::power, .25-3).
inline float splashPower(float mass,float speed){const float energy=.5f*mass*speed*speed;return std::clamp(std::sqrt(std::max(0.f,energy))/3.5f,.25f,3.f);}
}

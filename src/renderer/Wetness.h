#pragma once
// Where a map is wet. A floor is dry unless something wets it: the edges round its WATER, the floor
// under drips and spray and round steam leaks (EMITTER kinds 6, 7, 2, a plume indoors; steam
// HAZARDs), floor drains (med_floor_drain fixtures), authored WET patches, and outdoors the open sky
// when it rains. The map's wetness value (World::wetness) scales all of that: a damp map has bigger,
// wetter patches and seeps along the foot of its walls; it never coats a whole floor.
//
// The mask is baked with the map's light probes (LightProbes.h ProbeBake), over the probes' 24 x 24 m
// at a quarter metre: r the floor's wetness, g where puddles gather (low spots, drains, a pool's
// splash), b wet streaks down a wall under a source, a the height of the floor the wet lies on (in
// tenths of a metre above the volume's origin), so a deck, a table or a crate over a wet floor stays
// dry. scene.frag reads it per pixel (GpuRenderer's wetness atlas, the probe volume's slot), the CPU
// rasteriser per vertex.
#include "world/World.h"
#include <array>
#include <cstdint>
#include <vector>

namespace retro {
struct WetnessMask {
 static constexpr int Size=96;static constexpr float Texel=.25f;
 float originZ=0;
 // 0xAABBGGRR per texel, row y then column x, map-local metres (x+.5) * Texel.
 std::vector<std::uint32_t> texels;
 bool empty()const{return texels.empty();}
 // Bilinear, as the shader samples it: wet, puddle, streak (0-1) and the floor's height (metres).
 std::array<float,4> at(float x,float y)const;
 // The wetness a floor at height z here takes (0 dry): its wet where z is the mask's floor.
 float floorWet(float x,float y,float z)const;
};
WetnessMask bakeWetness(const World& world,float originZ);
// Rain on what sees the sky, outdoors (0 indoors): the map's wetness, or rainOverride when the lab
// sets one (0 dry, 1 pouring).
inline float& rainOverride(){static float v=-1;return v;}
float outdoorRain(const World& world);
}

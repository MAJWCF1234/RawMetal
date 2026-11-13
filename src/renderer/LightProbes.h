#pragma once
#include "world/World.h"
#include "renderer/LookTuning.h"
#include "renderer/Wetness.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace retro {
// Light probes: the soft light a map's own lamps leave in the air once it has
// come off the walls, floors and ceilings (one bounce, the surfaces' colour in
// it), the cold glow of resonance crystal, and outdoors how much sky each point
// sees. A grid of probes one metre apart over the whole map, each an ambient
// cube (six faces: the light arriving at a surface facing +x, -x, +y, -y, +z,
// -z), is baked when a map comes in, from a copy of it on a worker thread.
// scene.frag reads it per pixel (trilinear, half a cell out along the normal so
// a face never reads the probes behind it), actors and the CPU rasteriser read
// it per vertex. A room with no lit lamp has nothing to bounce and stays dark.

// A self-lit thing the bake treats as a small light: resonance crystal. `tone`
// is linear rgb at unit luminance, `power` in WorldLight::falloff units.
struct ProbeEmitter {Point3 position{},tone{};float power=0,range=0,radius=.2f;};
// The colour a surface returns of the light on it, by which way it faces.
struct ProbeSurfaces {Point3 floor{.30f,.29f,.27f},wall{.33f,.32f,.30f},ceiling{.28f,.28f,.28f};};

struct IrradianceVolume {
 static constexpr int Faces=6,Stride=Faces*4,Width=24,Depth=48;
 // Probe (i,j,k) stands at origin + ((i+.5)*cell, (j+.5)*cell, (k+.5)*cellZ), map-local.
 Point3 origin{};float cell=1.f,cellZ=1.f;int nx=0,ny=0,nz=0;
 // Per probe, per face: irradiance over pi (rgb, so a surface's albedo times it is the light it
 // returns) and sky openness (outdoors the share of that face's hemisphere that sees sky; 1 indoors).
 std::vector<float> data;
 // 1 baked in open air, 2 inside a solid and filled from open neighbours, 0 left empty.
 std::vector<std::uint8_t> valid;
 double milliseconds=0;std::uint64_t rays=0;int openProbes=0;int threads=1;
 bool empty()const{return data.empty();}
 size_t index(int x,int y,int z)const{return (size_t(z)*size_t(ny)+size_t(y))*size_t(nx)+size_t(x);}
 Point3 position(int x,int y,int z)const{return {origin.x+(x+.5f)*cell,origin.y+(y+.5f)*cell,origin.z+(z+.5f)*cellZ};}
 // Irradiance (rgb) and sky openness (a) for a surface at p facing n, exactly as scene.frag reads
 // the uploaded volume: trilinear between probe centres, clamped at the grid's edge.
 std::array<float,4> sample(Point3 p,Point3 n)const;
 // The same for a point in the air (no surface): the six faces averaged.
 std::array<float,4> ambient(Point3 p)const;
};

// A room's reflection probe: where its six faces are captured from and the box the reflected ray
// is clipped to (parallax correction), map-local.
struct ReflectionProbe {Point3 position{},lo{},hi{};};

struct ProbeBakeInput {
 // Power (0..1) each of the world's lights is baked at; missing entries bake at full power.
 std::vector<float> lampPower;
 std::vector<ProbeEmitter> emitters;
 ProbeSurfaces surfaces;
 // Worker threads (0: the machine's cores less one, at most eight).
 int threads=0;
};
// wet: where the map is wet (Wetness.h), baked with the probes over the same 24 x 24 m.
struct ProbeBake {IrradianceVolume volume;std::vector<ReflectionProbe> reflections;WetnessMask wet;};

// Scale from a probe's stored value to the fill light scene.frag adds (in the lamps' units), and the
// soft shoulder both renderers put on it so a room of many lamps fills and never floods.
inline constexpr float ProbeGain=3.1f;
inline float probeShoulder(float fill){const float ceiling=lookFillCeiling();return ceiling*(1.f-std::exp(-fill/ceiling));}

ProbeBake bakeLightProbes(const World& world,const ProbeBakeInput& input);
// Resonance crystal fixtures (res_crystal_l/m/s/wall) as cold emitters.
std::vector<ProbeEmitter> crystalEmitters(const World& world);
// --each-test=light-probes
bool testLightProbes();
}

#pragma once
#include "SoftwareRenderer.h"
#include <functional>
#include <iosfwd>
#include <memory>
#include <unordered_map>
#include <vector>

namespace retro {
// The air's own stuff: smoke, steam, dust, debris, drips and spray, fire and embers, toxic gas and the
// glow round live current, for every source at once (Particles.cpp). Sources are the map's EMITTER
// records (World::particleEmitters), its Steam, Fire, Toxic, Pressure, FallingDebris and Electricity
// hazards, and what the game throws: rounds striking solids (dust that hangs and grit that falls),
// shots (gun smoke), blasts and grenades (rolling smoke, a dust ring along the floor, debris that
// bounces, dust shaken from the ceiling, embers). Sparks are not drawn here: an EMITTER of kind Sparks
// throws its showers through the electric system (Game::electricScene), whose sparks bounce off the room
// and light it.
//
// Nothing about a particle is stored but what the room did to it: each one is worked out from its
// source, its index and the game clock, so the same moment draws the same frame, whatever was asked
// before. The memory keeps when a state gate last opened and closed, where a falling chunk lands and
// how far each patch of air is from a wall (the pool of what collision found), all of it rebuilt the
// same way if it is forgotten.
//
// Every card is held off walls, floors and ceilings by the clearance round it (shrunk, lifted and
// thinned where it would cut deep into one) and fades where it nears what lies behind it (the
// depth already drawn there, on either back end), so no card shows a hard line through a surface.
// Smoke, steam and dust take the room's light at each corner, more from a lamp behind them
// (they glow in a lamp's shaft); fire, embers and the glow round current light themselves. All the
// blended cards share one atlas and are drawn far to near; the self-lit ones share another.
// The blended atlas is a flipbook: each kind of air in four shapes, each shape in six frames of its
// life (a puff that rolls and opens out, a jet that frays into wisps, a fireball that burns down to
// soot), and a card moves through them as it ages, blended frame to frame. A card carries its own
// opacity and fades where it nears what lies behind it (SoftwareRenderer::CardLook), so it may lie
// into a wall or the floor without a hard line.
// A frame never draws more than its budget: past ten metres a source thins out (what is left drawn
// larger and thicker, so the far air keeps its body), and the far ones thin first when the budget
// runs short.
struct ParticleStats {int sources=0,particles=0,blended=0,glowing=0,thinned=0,culled=0,inside=0,clearanceProbes=0;float ms=0;};
// A thrown thing's flight as the frames saw it (its smoke trail is born along it).
struct ParticleCrumb {float time=0;Point3 at{};};
struct ParticleMemory;
struct Particles {
 using Texture=SoftwareRenderer::Texture;
 using CardLook=SoftwareRenderer::CardLook;
 static constexpr int BlendedBudget=2200,GlowBudget=900;
 // The blended atlas's rows (smoke, soot, steam, dust, toxic gas, mist, the glow round current, a
 // fireball's burn, the solid bits) of 64-texel cells.
 static constexpr int AtlasRows=9,AtlasCell=64;
 // A card: four corners (each with its atlas uv and its light), the atlas, the card's light scale and
 // how it is laid on (opacity, soft edge, flipbook blend; the glow atlas ignores it).
 using Card=std::function<void(const MeshVertex*,const Texture&,float,const CardLook&)>;
 // The room's light at a point on a surface facing `normal` (Scene3D's illumination for moving things).
 using Light=std::function<float(Point3,Point3)>;
 // Whether a box (low and high corners) can be seen at all: in view and not hidden behind an occluder.
 using Visible=std::function<bool(Point3,Point3)>;
 struct View {Point3 eye{},forward{1,0,0};float pixel=.001f;int level=0;float now=0;};
 // What a test or an inspection adds to the game's own events (renderer-side, like m_goreBursts).
 // A throw: a grenade leaving `from` at `velocity` at `time`, coming to rest on the floor (and smoking
 // there) until `fuse` seconds on.
 struct Throw {Point3 from{},velocity{};float time=0,fuse=2.4f;int level=0;};
 struct Extra {std::vector<BarrelExplosion> blasts;std::vector<HitBurst> hits;std::vector<ShotRay> shots;std::vector<Throw> throws;};
 static ParticleStats draw(ParticleMemory& memory,const Game& game,const View& view,const Card& card,const Light& light,const Visible& visible,const Extra* extra=nullptr);
 // Whether a point is clear of every solid (no card is drawn round a centre that is not).
 static bool open(const World& w,Point3 p);
 static const Texture& blendedAtlas();
 static const Texture& glowAtlas();
};
// What the air remembers between frames: when each state gate last opened and closed, the flights of
// thrown things once the room has had its say, how far each 20 cm of air is from a solid (by level), the
// events a test adds, and the last frame's count.
struct ParticleMemory {
 struct GateMemory {bool seen=false,open=false;float openedAt=-1e9f,closedAt=-1e9f,earlier=-1e9f;};
 std::unordered_map<std::uint64_t,GateMemory> gates;
 // A ballistic particle's flight: up to three arcs, then at rest (a drop splashes where it lands).
 struct Path {std::array<Point3,3> from{},velocity{};std::array<float,3> start{};int arcs=1;Point3 rest{};float restAt=1e9f;bool splash=false;};
 std::unordered_map<std::uint64_t,Path> paths;
 // A cell's clearance: how far it has been looked, whether a solid stopped the look (or it is inside one: -1),
 // and how far its middle is from the nearest solid box (-1: not asked yet).
 struct Room {float value=0;bool settled=false;float box=-1;};
 // And the map's solid boxes (structures, wall tiles, solid fixtures) binned by metre, for the exact distance
 // to a thin beam or rail the probes could pass either side of.
 struct Clearance {std::uint64_t signature=0;std::unordered_map<std::uint64_t,Room> cells;std::vector<std::array<float,6>> boxes;std::vector<std::vector<std::uint32_t>> bins;};
 std::unordered_map<int,Clearance> clearance;
 // Each map's own sources (its emitters and hazards), worked out once (by level, with the map's signature).
 std::unordered_map<int,std::pair<std::uint64_t,std::shared_ptr<const void>>> fixed;
 // The game's thrown grenades as the frames saw them (by level), for their trails.
 std::unordered_map<int,std::vector<std::vector<ParticleCrumb>>> trails;
 Particles::Extra extra;ParticleStats last;
};
std::shared_ptr<ParticleMemory> makeParticleMemory();
// The blasts a test or an inspection added (the scene flashes them as it does the game's own).
const std::vector<BarrelExplosion>& particleExtraBlasts(const ParticleMemory& memory);
// --each-test=particles: determinism, the budget, nothing drawn inside a wall, the EMITTER record
// round trip, every Chapter 1 and 2 map dressed within its limits, the cost of a dense room, and both
// back ends drawing it. RM_PARTICLE_FRAMES=<folder> also writes frame strips there.
bool testParticles();
}

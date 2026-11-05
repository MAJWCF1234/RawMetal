#pragma once
#include "../world/World.h"
#include <iosfwd>
#include <vector>

namespace retro {
// Electricity as it is seen and felt. Every source -- a live lead's torn end and its crackles, an
// Electricity hazard, a round striking a solid, anything calling Game::sparkBurst or strikeArc --
// becomes the same few things: showers of sparks with physics, arcs struck like lightning between
// real surfaces, and the light both throw. Game::electricScene gathers them for a chunk; the
// renderer draws them (renderer/ElectricFx.cpp) and Game::weaponFlashes lights the room from the
// same scene, so what is drawn and what lights the room keep step.
namespace electric {
// A shower thrown at game time `time` from `at` (chunk-local) in a cone round `along` (`spread` its
// half angle, radians): `count` sparks at about `speed` m/s, `heat` 0-1 how white they leave, the last
// `embers` of them slow, fat and long-lived; `smoke` a wisp of ozone at the break. `body` is a prop's
// box the sparks may not pass through (the cell a clip came off), used when `bodied`.
struct Burst {Point3 at{},along{0,0,1};float time=0,speed=3.f,spread=.8f,heat=1;unsigned seed=1;int count=24,embers=3,level=0;bool smoke=false,bodied=false;LeadBody body{};};
// An arc from `from` to `to`, both on something that conducts, or `open`: a brush of current into the
// air off `from` when nothing is in reach. Struck afresh StrikeRate times a second, each strike a new
// shape from `seed`; `bow` lifts its middle (current crawling over a floor or water), `width` scales
// it, `resonance` gives it the violet fringe of an arc off a cell of resonance ore. A timed arc (Game::
// strikeArc) burns until `until`. `solid`: it lands on metal or masonry, which spits sparks back.
struct Arc {Point3 from{},to{};unsigned seed=1;float strength=1,width=1,bow=0,until=1e30f;int level=0;bool open=false,resonance=false,solid=false;};
// A transient light: position (chunk-local in a Scene, world in Game::weaponFlashes), strength on the
// muzzle flash's scale, linear tone at unit luminance (WorldLight::tone) and reach in metres. `muzzle`:
// a gun's flash (an NPC's), lit as the player's is; the rest are sparks and arcs, which the CPU
// rasteriser (no tone curve) lights more gently, under a shoulder.
struct Flash {Point3 at{};float strength=0;Point3 tone{1,1,1};float reach=12;bool muzzle=false;};
struct Scene {std::vector<Burst> bursts;std::vector<Arc> arcs;std::vector<Flash> flashes;};
// A spark as simulated: where, how fast, heat 1 white-hot .. 0 out, ember or spark, settled.
struct Spark {Point3 p{},v{};float heat=0,size=1;bool ember=false,resting=false;};
// One strike of an arc as drawn: a piece of the channel or a fork, its width (1 at the root, tapering).
struct Piece {Point3 a,b;float width;};
// Hard budgets for a frame, all sources together: sparks simulated and drawn, bolts drawn.
constexpr int SparkBudget=320,BoltBudget=24;
constexpr float StrikeRate=24,SimStep=1.f/120;
// What of it the game shows: 2 drawn and lit (always, but for the performance test's paired frames),
// 1 drawn without its light, 0 nothing worked out at all.
inline int shown=2;
// The sparks of `b` alive at `now`, simulated on fixed SimStep steps from its time against the world's
// solids, floors and water (and its body): the same world and burst give the same sparks whatever
// the frame rate or the order times are asked in. Appends at most `limit`; returns how many.
int sparks(const World& w,const Burst& b,float now,std::vector<Spark>& out,int limit);
// How long a burst's last ember can glow (nothing of it is drawn after).
float burstLife(const Burst& b);
// Forget every simulated burst and cached hazard arc (tests: the next ask starts from scratch).
void resetCaches();
// How bright arc `a` burns at `now`: 0 between strikes, 0-1 flickering through one.
float strike(const Arc& a,float now);
// The strike under way at `now`: the main channel from `from` to `to` by recursive midpoint
// displacement with forks off it (or a brush of short forks when open). `detail` 2-6 levels.
void bolt(const Arc& a,float now,int detail,std::vector<Piece>& out);
// Where current at `from` would jump within `reach`: the nearest solid surface, floor or water.
bool conductor(const World& w,Point3 from,float reach,unsigned seed,Point3& to);
// A shower's warm pop and an arc's blue-white strike as light (strength 0 when out).
Flash flash(const Burst& b,float now);
Flash flash(const Arc& a,float now);
// The arcs an Electricity hazard (index `index` in `w`) is striking at `now`: current crawling between
// points of the floor or water in its box and up to the metal beside it, in short busy spells; with
// `sparks`, the showers thrown where a spell's arc lands on metal (timed from the spell's start: the
// game records them as events, Game::updateElectricity, so they fly on after the spell is over).
void hazardArcs(const World& w,const Hazard& h,int index,float now,std::vector<Arc>& out,std::vector<Burst>* sparks=nullptr);
// Physics, arcs and determinism (engine-regression).
bool test(std::ostream& report);
}
}

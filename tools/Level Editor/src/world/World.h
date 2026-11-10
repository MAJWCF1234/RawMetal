#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <cstdint>
#include "../core/Math.h"
#include "WorldDefinition.h"
#include "ScriptDefinition.h"

namespace retro {
struct Door {
 float left=0,right=0,y=0,open=0;bool opening=false,transfer=false,entry=false;float z=0;
 // Authored requirements are independent of whether a door connects chunks.
 bool requireEnemiesClear=false,requireControl=false;
 StateId requireState=0;int requireValue=1;
 bool swinging=false;
 // A swinging leaf opens toward +y (1) or -y (-1) from its hinge at `left`.
 float swing=1.f;
 // A notice plate on the header: CampaignChunkCount+n is notice n (a door that
 // says why it is shut); -1, none. Doors carry no area names.
 int sign=-1;
 float maxOpen=1.f;
};
struct WorldProp {int kind;Vec2 position;float height,footprint,yaw;Vec2 halfSize;float base=0;};
// core: the fraction of the footprint that collides when solid (a tree blocks
// at its trunk, not across its canopy); 1 for ordinary furniture and plant.
// visible=false: a collision-only copy, standing in a neighbouring map for the
// part of a large fixture that crosses the map border.
struct Fixture {int model;Vec2 position;float base,width,depth,height,yaw;bool solid=false;float core=1;bool visible=true;};
// Bare: a light with no housing (a fire, a lamp head that already has its model).
enum class WorldLightMount { Ceiling, Wall, Bare };
struct WorldLight {
 Vec2 position;float z;
 // Ceiling records retain the original source and fixture placement. For a
 // wall lamp, position/z is the backing centre and yaw points into the room.
 WorldLightMount mount=WorldLightMount::Ceiling;
 float yaw=0,intensity=1,range=12;
 // Colour temperature (K, Planck blackbody) and a green(+)/magenta(-) tint
 // for discharge tubes. 3400 K, 0 is a neutral warm incandescent.
 float kelvin=3400,tint=0;
 // A colour gel (linear RGB filter, each 0..1): red emergency light, green
 // resonance glow, monitor blue. White leaves the blackbody untouched.
 Point3 gel{1,1,1};
 // Linear RGB at unit luminance: Planck's blackbody sampled at 610, 550 and
 // 465 nm, the tint, then the gel. A gel keeps the lamp's brightness (CPU
 // bakes stay scalar) but no channel climbs past 2 (or the bare lamp's own
 // peak), so a saturated blue or red cannot blow out under the shoulder.
 static Point3 tone(float kelvin,float tint,Point3 gel){
  const double lambda[3]={610e-9,550e-9,465e-9},filter[3]={gel.x,gel.y,gel.z};double c[3],bare=0,luma=0,peak=0;
  for(int i=0;i<3;++i){c[i]=1./(std::pow(lambda[i],5.)*(std::exp(1.4388e-2/(lambda[i]*std::max(double(kelvin),800.)))-1.));}
  c[1]*=1.+tint*.35;c[2]*=1.-tint*.1;double y=c[0]*.2126+c[1]*.7152+c[2]*.0722;for(double&v:c)v/=std::max(y,1e-300);
  for(int i=0;i<3;++i){bare=std::max(bare,c[i]);c[i]*=std::clamp(filter[i],0.,1.);luma+=c[i]*(i==0?.2126:i==1?.7152:.0722);peak=std::max(peak,c[i]);}
  double scale=luma>1e-9?std::min(1./luma,std::max(2.,bare)/peak):0.;return {float(c[0]*scale),float(c[1]*scale),float(c[2]*scale)};
 }
 Point3 tone()const{return tone(kelvin,tint,gel);}
 // Switching, mirroring Hazard: lit while (enabledState==0 or its state equals
 // enabledValue) xor invert, and, with a period, inside the on-window. flicker
 // (0..1) adds irregular dropouts: a failing tube or a browned-out circuit.
 StateId enabledState=0;int enabledValue=1;bool invert=false;
 float period=0,onTime=0,phase=0,flicker=0;
 // A ceiling lamp's housing: -1 is the original square facility lamp; any
 // other fixture model is drawn flush under its mounting surface at this
 // size, turned by yaw (a clinic's recessed fluorescent, say).
 int fitting=-1;float fittingWidth=0,fittingDepth=0,fittingHeight=0;
  bool shadows=true;int prop=-1,propLevel=-1;bool cargoLift=false;
 // Power can change while the map runs (Game::lampPower), so renderers keep
 // this lamp out of their baked caches and apply its power every frame.
  bool switchable()const{return enabledState||invert||period>0||flicker>0||prop>=0||cargoLift;}
 Point3 emitter()const{
  if(mount==WorldLightMount::Wall)return {position.x+std::cos(yaw)*.16f,position.y+std::sin(yaw)*.16f,z};
  return {position.x,position.y,z};
 }
 Point3 shadowTarget()const{auto p=emitter();if(mount==WorldLightMount::Ceiling)p.z-=.04f;return p;}
 float falloff(float squaredDistance)const{
  float radiusSquared=range*range;if(squaredDistance>=radiusSquared)return 0.f;
  float t=1.f-squaredDistance/radiusSquared;
  return intensity*3.2f*t*t/(1.f+squaredDistance*.12f);
 }
 // Beam profile toward `out` (unit, lamp to receiver). A ceiling fitting
 // throws its light down: pools on the floor, scallops on the walls under
 // it, the ceiling only the spill round the housing. A wall lamp lights the
 // room in front of it; a bare light shines every way. scene.frag and
 // shadow.frag mirror this (lampBeam), keyed by beamCode().
 static float smooth01(float x){x=std::clamp(x,0.f,1.f);return x*x*(3.f-2.f*x);}
 float beam(Point3 out)const{
  if(mount==WorldLightMount::Ceiling)return .2f+.8f*smooth01((-out.z-.42f)/.45f);
  if(mount==WorldLightMount::Wall)return .12f+.88f*smooth01((out.x*std::cos(yaw)+out.y*std::sin(yaw)+.25f)/.75f);
  return 1.f;
 }
 // falloff and beam together; toLamp runs from the receiver to emitter().
 float reach(Point3 toLamp,float squaredDistance)const{
  float f=falloff(squaredDistance);if(f<=0||squaredDistance<1e-6f)return f;
  return f*beam(toLamp*(-1.f/std::sqrt(squaredDistance)));
 }
 // The beam for the GPU lamp record: 1 every way, 2 a ceiling fitting,
 // 4 + yaw (wrapped to 0..2 pi) a wall lamp facing yaw.
 float beamCode()const{
  if(mount==WorldLightMount::Ceiling)return 2.f;
  if(mount==WorldLightMount::Wall){float a=std::fmod(yaw,6.2831853f);return 4.f+(a<0?a+6.2831853f:a);}
  return 1.f;
 }
};
// Outdoor worlds carry one directional light. Direction points toward the
// sun (z up); intensity 0 means no sun. Colour is linear.
struct SunLight {Point3 direction{};float intensity=0,r=1,g=1,b=1;};
// Authored overhead services, with absolute elevations. Kept above standing clearance.
// Materials: 0 service pipe, 1 rusted metal, 2 panel steel, 3 yellow steel,
// 4 red steel, 5 pale steel, 6 dark finish. An explicit endZ
// supplies equipment/bracing geometry without automatic ceiling hangers.
struct PipeRun {Vec2 start,end;float z,radius,endZ=-999;int material=0;};
// An electrical lead (LEAD record, kit::lead): a cable plugged in at `anchor`
// (a cabinet's terminals, a machine's posts) whose far end is clipped to the
// carryable prop this map's clutter spawn `prop` becomes, at `offset` in the
// prop's own frame (x across its width, y its depth, z up from its base); or
// tied to the fixed point `end` (prop<0); or already torn (`loose`: it hangs
// from its anchor, its tail toward `end`). `length` is the cable's own, so the
// slack is what it has over the span. The clip tears when the prop pulls past
// length+TearMargin, leaves the map or breaks: a `live` lead arcs as it goes,
// and tearing sets `state`. Absolute heights; material is a PipeRun finish
// (0-6) or a cable's insulation: 7 black, 8 red.
// A test lead (kit::testLead, LEAD's panel|socket|terminal) has a plug at each end. Its
// `anchor` is a socket on the instrument fixture `panel` (socketPort, from leadPorts()) and
// `socket` the way the plug and its boot hold the cable out of it; `lamp` is the panel's
// pilot, a light that dies when any lead on the panel tears. Its `offset` is the terminal
// post terminalPort on its prop, and `terminal` the post's axis (the prop's frame). The plug
// there is a right-angle banana plug: its pin down the post, its head PlugHead above the post's
// top and its boot leaving level, swivelled round the post toward where the cable runs, so it
// turns with the prop; torn, it stays on the cable's end and swings with its weight. Zero
// socket or terminal: a bare post or clip, as before.
// A free physics chain (CHAIN record, kit::chain) is the same Verlet cable with
// `chain` set: no plugs or circuit. It hangs from `anchor`, or when `creature`
// is a CREATURE spawn index (>=0) from that creature's live body at `offset` in
// its facing frame (x right, y forward, z up from its feet) — the same axes the
// enemy mesh uses. `loose` is always true for a chain.
struct Lead {Point3 anchor{},end{},offset{};int prop=-1;bool loose=false;float length=1,radius=.006f;int material=7;bool live=false;StateId state=0;
 Point3 socket{},terminal{};int panel=-1,socketPort=-1,terminalPort=-1,lamp=-1;int creature=-1;bool chain=false;
 bool socketed()const{return socket.x!=0||socket.y!=0||socket.z!=0;}bool plugged()const{return terminal.x!=0||terminal.y!=0||terminal.z!=0;}
 static constexpr float TearMargin=.3f,PlugHead=.018f;};
// An empty suit hung by its hood from a hook (absolute), its front toward `facing`. Game
// hangs the hazmat ragdoll from it (Ragdoll::hang): limp, swaying when brushed or shot.
struct HungSuit {Point3 hook{};float facing=0;};
// Where test leads connect, by model: an instrument's sockets, a cell's terminal posts, a
// panel's pilot lens ("lamp"). In the model's own frame at its catalog size (x across its
// width, y its depth, z up from its base); `out` is the way a seated plug leaves the port.
struct LeadPort {int model;std::string_view name;Point3 at,out;};
std::span<const LeadPort> leadPorts();
int leadPort(int model,std::string_view name);
// A port on fixture `f` in the world, the fixture's base standing on `floor`; `out` gets its way out.
Point3 fixturePort(const Fixture& f,const LeadPort& port,float floor,Point3* out=nullptr);
// A port on a prop spawned at `width` x `depth` x `height`, in the prop's frame (a lead's offset).
Point3 propPort(int model,float width,float depth,float height,const LeadPort& port);
// A lead's cable as simulated: verlet points from the anchor out (transient,
// re-hung on load) and whether its clip has torn (saved). tornAt is the game
// time it tore (its sparks), clutter the prop's last index in its chunk list,
// plugUp the axis of the post a seated plug stands on (zero off its post).
// `anchorOverride` / `liveAnchor`: a creature-hung chain's moving mount; when set,
// Verlet pins to `liveAnchor` instead of the authored Lead::anchor.
struct LeadRope {std::vector<Point3> points,previous;bool torn=false,sleeping=false;float rest=0,tornAt=-1e9f;int clutter=-1;Point3 plugUp{};bool anchorOverride=false;Point3 liveAnchor{};};
// A test lead's right-angle plug at its cable's far end, as drawn: the tip its pin stands out of,
// the axis up its body (the pin down it) and the way its boot leaves toward the cable. Seated, on
// its post; torn, on the cable's end, hanging pin down, and laid on its side as the end comes to
// rest on `support` (the height under it), lifted so no part of it sinks into what it lies on.
struct LeadPlug {Point3 tip{},up{},side{};};
LeadPlug leadPlug(const Lead& l,const LeadRope& r,float support);
// The lowest point of that plug: its pin, moulded body and boot about the lead's `radius`.
float leadPlugLowest(const LeadPlug& p,float radius);
// The prop a lead is clipped to, as its cable meets it: the prop's box (centre,
// unit axes, half sizes). The cable stays out of it and leaves the clip outward
// through the face the clip is on, held off it as a plug's boot holds a lead.
struct LeadBody {Point3 centre{},axis[3]{};float half[3]{};};
// A lead's cable as it is drawn: the Catmull-Rom curve through its points, `t` (0-1) of the
// way along the piece from point k to point k+1.
inline Point3 leadCurve(const std::vector<Point3>& p,size_t k,float t){const size_t n=p.size();const auto&p0=p[k?k-1:0];const auto&p1=p[k];const auto&p2=p[std::min(k+1,n-1)];const auto&p3=p[std::min(k+2,n-1)];const float t2=t*t,t3=t2*t;
 return p0*(-.5f*t3+t2-.5f*t)+p1*(1.5f*t3-2.5f*t2+1)+p2*(-1.5f*t3+2*t2+.5f*t)+p3*(.5f*t3-.5f*t2);}
// How deep `p` lies inside the body's box (0 or less: outside it).
inline float leadBodyDepth(const LeadBody& b,Point3 p){const auto d=p-b.centre;float least=1e9f;for(int j=0;j<3;++j)least=std::min(least,b.half[j]-std::fabs(d.x*b.axis[j].x+d.y*b.axis[j].y+d.z*b.axis[j].z));return least;}
// The body as its cable meets it at `clip`: the face the clip is on (the one it lies nearest,
// as a share of the box; `face` gets its axis, `side` -1 or 1 which end) is brought down to
// the clip's own level, as for a plug on a lid below the top of the prop's box. The cable may
// lie at the level it is plugged in at, never below it.
inline LeadBody leadBodyAtClip(LeadBody b,Point3 clip,int* face=nullptr,float* side=nullptr){
 const auto d=clip-b.centre;int f=2;float most=-1,s=1,q=0;
 for(int j=0;j<3;++j){const float v=d.x*b.axis[j].x+d.y*b.axis[j].y+d.z*b.axis[j].z,share=std::fabs(v)/std::max(b.half[j],1e-3f);if(share>most){most=share;f=j;s=v<0?-1.f:1.f;q=std::fabs(v);}}
 if(q<b.half[f]){const float cut=(b.half[f]-q)*.5f;b.half[f]-=cut;b.centre=b.centre-b.axis[f]*(s*cut);}
 if(face)*face=f;if(side)*side=s;return b;}
// A torn live lead's arcing, by seconds since it tore: the zap, then the
// crackles of the lead_arc clip it plays (measured from its envelope), then it
// is dead. The sparks and the sound keep time together. 1 the zap, .6 a spit.
// `since` gets the seconds into the burst under way.
inline float leadArc(float age,float* since=nullptr){constexpr float bursts[][2]={{0,.26f},{.40f,.52f},{.60f,1.12f},{1.30f,1.52f},{1.60f,1.68f},{1.90f,1.98f},{2.15f,2.28f},{2.45f,2.52f}};
 for(const auto& b:bursts)if(age>=b[0]&&age<b[1]){if(since)*since=age-b[0];return b[0]==0?1.f:.6f;}return 0.f;}
// scale and health (0 = the kind's default) make variants and bosses. A
// non-zero wakeState keeps the creature dormant and deaf until that state is
// set: patients on their beds, a nest that wakes when the lights fail.
struct CreatureSpawn {CreatureKind kind;Vec2 position;float z=-999;float scale=1,health=0;StateId wakeState=0;};
struct PickupSpawn {Vec2 position;PickupKind kind;float z=-999;};
struct Breakable {float x1=0,y1=0,x2=0,y2=0,bottom=0,top=0;int material=17;float health=0;bool holds=false;StateId state=0,trigger=0;int fixture=-1;unsigned part=0;};
struct ShotTrace {int blocking=-1;std::array<int,2> passed{-1,-1};std::array<float,2> passedAt{1,1};std::array<Point3,2> passedNormal{};};
inline constexpr int ClutterModelKind=7;
struct ClutterSpawn {int kind;Vec2 position;float z=-999,yaw=0;int model=-1;float width=0,depth=0,height=0,pitch=0,roll=0;StateId breakState=0;};
// One authored liquid basin controls the bed, physics surface and drawn mesh.
struct WaterVolume {float x1,y1,x2,y2,bed,surface;};
// Content specifies emitters; rendering does not decide their positions from chunk IDs.
struct ParticleEmitter {Vec2 position;float z;Vec2 drift;float rise=1.05f,rate=.55f,radius=.07f,growth=.2f;int count=7;};
struct Hazard {
 enum class Kind {Electricity,Steam,Crusher,Toxic,Fire,FallingDebris,Pressure,Anomaly};
 Kind kind=Kind::Electricity;float x1=0,y1=0,x2=0,y2=0,bottom=-100,top=100,damagePerSecond=0;
 std::uint32_t enabledFlag=0;int enabledValue=1;bool invertFlag=false;
 float period=0,onTime=0,phase=0;
};
// What a readable thing is read on. A screen is one of the plant's computers;
// the rest is paper, read in the hand: a typed letter or printout, a note or
// log book in pen, a sticky note in marker.
enum class Readable { Screen, Letter, Note, Sticky };
struct Terminal {Vec2 position;const char* title;const char* line1;const char* line2;float z=0;bool control=false;int reactorAction=0;StateId activateState=0;bool toggleState=false;float yaw=kPi;StateId requireState=0;
 // Paper lies on its bench, or is taped to the wall it faces.
 Readable readable=Readable::Screen;bool taped=false;float surface=-1;
 bool paper()const{return readable!=Readable::Screen;}
 // Paper that brings its own bench (not taped up, not lying on a desk).
 bool paperOnBench()const{return paper()&&!taped&&surface<0;}};
struct CargoLift {
 float x1=0,y1=0,x2=0,y2=0,lower=0,upper=0,speed=1.f;
 StateId callState=0,releaseState=0,positionState=0,downState=0,arrivedState=0,descendedState=0;
};
struct Compactor {float x1,y1,x2,y2,bed,raised,period=9;StateId stopState=0;};
// Shapes other than a box are drawn as their true geometry inside x1..top,
// non-solid; PRIM records give them collision-proxy boxes (Primitives.h).
// `facing` is the quarter turn they rise or open toward: 0 +y, 1 +x, 2 -y, 3 -x.
enum StructureShape:std::uint8_t {ShapeBox,ShapeRamp,ShapeColumn,ShapeArch,ShapeVault,ShapeDome,ShapeStrut,ShapeRoundel};
struct Structure {float x1,y1,x2,y2,bottom,top;bool rail=false;int material=0;bool solid=true;std::uint8_t shape=ShapeBox,facing=0,layerSurface=0;};
// A structure that only collides: the solid core of something drawn in finer
// pieces (a steel column's flanges and web), never rendered.
inline constexpr int CollisionProxyMaterial=30;
// Terrain source points are one-metre solid/air voxels, Minecraft-style.
// Surface Nets removes the cube faces and emits the engine-native low-poly skin;
// collision queries the same voxel density field.
struct TerrainVertex {float x=0,y=0,z=0,u=0,v=0;};
// Shared terrain material IDs, independent of campaign/custom world identity.
enum TerrainMaterial : std::uint8_t { TerrainSoil, TerrainRock };
struct TerrainTriangle {TerrainVertex a,b,c;std::uint8_t material=TerrainSoil;};
struct Span {float floor,ceiling;uint16_t flags=0;};
using MapRows = std::array<std::string_view,24>;
struct MapLayer {
    std::string_view name;
    float elevation,thickness;
    MapRows rows;
};
struct Staircase {
    float x1,y1,x2,y2,bottom,top;
    int steps;
    bool alongY,ascending;
    bool openUnderside=false;
    float treadThickness=.12f;
    bool sideRails=false;
};

// Shared geometry records for built-in and runtime-authored maps. Loading these
// records never changes the world's campaign identity. CustomCampaign parses
// the editor's .txt payload into the same representation.
struct AuthoredLayerData {
    std::string name;
    float elevation=0,thickness=0;
    std::array<std::string,24> rows{};
};
struct AuthoredTerminalData {
    Vec2 position{};
    std::string title,line1,line2;
    float z=0;
    bool control=false;
    int reactorAction=0;
    StateId activateState=0;
    bool toggleState=false;
    StateId requireState=0;
    float yaw=kPi;
    Readable readable=Readable::Screen;
    // Paper only: taped to the wall it faces at an absolute height, instead of
    // lying on its bench.
    bool taped=false;
    // Paper only: lying on furniture already there (a desk) whose top is this
    // far above the floor; -1 gives the sheet a bench of its own.
    float surface=-1;
};
struct AuthoredSign {
    Vec2 position{};
    float z=0,width=3,height=.9f,yaw=kPi;
    std::string title,subtitle;
    std::uint32_t accent=0xffca994du;
};
// A named region of a map (HUD location label). The first zone containing
// the player wins; maps without zones show the map name. A zone with a title
// announces a major area: the first time the player enters it the title
// card fades in across the top. A title-only zone has no name. A zone that
// sets a haze gives its box its own air (World::haze); a haze-only zone has
// neither name nor title.
struct AuthoredZone {
    std::string name;
    float x1=0,y1=0,x2=24,y2=24,bottom=-1000,top=1000;
    std::string title;
    Haze haze=EnvironmentHaze;
};
struct AuthoredNavHint {
    Vec2 position{};
    float z=0;
    int hint=0; // 0 Path, 1 Cover — matches ai::NodeHint
};
// Campaign-authored outdoor terrain (TERRAIN / TERRAIN_FLAT records). A seeded
// fractal heightfield in world space, so every stitched chunk samples one
// continuous surface. Flats level pads, roads and building sites, blending
// back into the hills over `blend` metres. Heights are 1 m voxel tops.
struct TerrainFlat {float x1=0,y1=0,x2=0,y2=0,height=2,blend=4;};
// Ground cover painted over authored terrain: one texture kind per rectangle
// (world metres), with a noisy border `edge` metres wide.
enum GroundKind:int {GroundShortGrass,GroundLeafyGrass,GroundClover,GroundWild,GroundDry,GroundLitter,GroundGravel,GroundMud,GroundKinds,GroundMeadow=GroundKinds};
struct GroundPaint {float x1=0,y1=0,x2=0,y2=0;int kind=GroundMeadow;float edge=1.5f;};
struct TerrainShape {
    std::uint32_t seed=1;
    float base=2,amplitude=3,scale=36,roughness=.45f;
    std::vector<TerrainFlat> flats;
    // Any GROUND record turns on painted cover: a noise meadow of grass, clover,
    // wild and dry ground everywhere, then the rectangles in order.
    std::vector<GroundPaint> ground;
    // HORIZON: unreachable land around the playable maps, rising `rise` metres
    // over `depth` metres and wooded every `spacing` metres with these models
    // (drawn only as Painterly Horizon LOD impostors).
    float horizonDepth=0,horizonSpacing=6,horizonRise=0;std::vector<int> horizonModels;
};
// Surface sets for a map's layer walls, floor and ceiling (-1 keeps the
// legacy choice by chunk index). Structure materials 26-29 and 31-35 are the
// matching box finishes: 26 rock, 27 clinic tile, 28 lab panel, 29 corrugated
// sheet, 31 brick, 32 mine timber, 33 ancient stone, 34 resonance growth,
// 35 crystal (self-lit), 36-38 painted clinic walls with a teal, green or
// maroon dado band at the floor, 39 ward curtain fabric. Material 3, the wall
// finish, follows wallSurface.
enum Surface:int {SurfaceDefault=-1,SurfaceConcrete,SurfacePressure,SurfaceBrick,SurfaceClinicTile,SurfaceLabPanel,SurfaceRock,SurfaceCorrugated,SurfaceOfficePlaster,SurfaceMineTimber,SurfaceStone,SurfacePaintedTeal,SurfacePaintedGreen,SurfacePaintedMaroon,SurfaceCount};
// Where the lights are truly out: inside an active zone the light floor, haze
// and exposure lift go to black, the flashlight dies (jamFlashlight) and the
// muzzle flash is the only light. Active while (state==0 or its state equals
// value) xor invert.
struct DarkZone {float x1=0,y1=0,x2=24,y2=24,bottom=-1000,top=1000;StateId state=0;int value=1;bool invert=false;bool jamFlashlight=true;};
// A heavy crate the player can shove: it slides along the axis it was pushed
// until something stops it, and comes to rest on a floor cell's centre. z is
// the floor it stands on; it is a solid box of side `size` you can climb onto.
struct PushCrate {Vec2 position{};float z=0,size=1.f;Vec2 home{},slide{},target{};};
// A floor plate a crate can be slid onto: its state is 1 while a crate rests there.
struct CratePlate {Vec2 position{};StateId state=0;};
// An ash storm cycle for a whole campaign (STORM record): calm, a warning, the
// storm itself, then calm again. Out in the open during the storm the player
// takes `damage` a second; `ambientAsh` (0-1) is the ash that falls when calm.
struct StormCycle {int calmMs=0,warningMs=0,stormMs=0;float damage=0,ambientAsh=0;bool active()const{return calmMs>0&&stormMs>0;}};
// A roof the storm cannot reach (SHELTER record), in map metres; on terrain its
// bottom and top are heights above the ground at its centre.
struct Shelter {float x1=0,y1=0,x2=0,y2=0,bottom=-1000,top=1000;};
struct AuthoredMapData {
    std::string name;
    std::string skybox="industrial_night";
    std::shared_ptr<const TerrainShape> terrain;
    std::vector<ParticleEmitter> emitters;
    ChunkDefinition definition{};
    // Adjacent views can retain another hall without joining its traversal group.
    std::vector<int> visibleResidencyGroups;
    bool openNorth=false,openSouth=false,openWest=false,openEast=false;
    std::vector<AuthoredLayerData> layers;
    std::vector<Door> doors;
    std::vector<WorldProp> props;
    std::vector<Fixture> fixtures;
    std::vector<PipeRun> pipes;
    std::vector<Lead> leads;
    std::vector<HungSuit> hungSuits;
    std::vector<WorldLight> lights;
    std::vector<CreatureSpawn> creatureSpawns;
    std::vector<PickupSpawn> pickupSpawns;
    std::vector<ClutterSpawn> clutterSpawns;
    std::vector<Breakable> breakables;
    std::vector<WaterVolume> waterVolumes;
    std::vector<Hazard> hazards;
    std::vector<Compactor> compactors;
    std::vector<Structure> structures;
    std::vector<Staircase> stairs;
    std::vector<AuthoredTerminalData> terminals;
    std::vector<AuthoredSign> signs;
    CargoLift cargoLift;
    std::vector<TimedSequence> timedSequences;
    std::vector<ActorTrack> actorTracks;
    std::vector<SequenceCue> sequenceCues;
    // Conversations the actors here open with E (ActorTrack::talk).
    std::vector<DialogueNode> dialogue;
    std::vector<ScriptEvent> scriptEvents;
    std::vector<ObjectiveText> objectiveTexts;
    std::vector<AuthoredNavHint> navHints;
    std::vector<AuthoredZone> zones;
    std::vector<PushCrate> pushCrates;
    std::vector<CratePlate> cratePlates;
    // Set by a lever or terminal: every crate on the map goes back home.
    StateId crateReset=0;
    std::vector<DarkZone> darkZones;
    std::vector<Shelter> shelters;
    int wallSurface=SurfaceDefault,floorSurface=SurfaceDefault,ceilingSurface=SurfaceDefault;
};
// Seat test lead `l` by name: its anchor in socket `socket` of fixture `panel` (or left as it
// is with no panel), its offset on terminal `terminal` of its prop (or left with none). The
// first lead on a panel adds the panel's pilot lamp, lit until any lead on it tears. Returns
// what is wrong (no such fixture, prop or port) or an empty string.
std::string seatTestLead(AuthoredMapData& m,Lead& l,int panel,std::string_view socket,std::string_view terminal);
struct CustomCampaign {
    std::string name;
    std::string sourceFile;
    std::uint64_t key=0;
    int startMap=0;
    StormCycle storm;
    std::vector<std::shared_ptr<const AuthoredMapData>> maps;
};

class World {
public:
    static constexpr int Width = 24;
    static constexpr int Height = 24;
    // The toy Ashfall world proves the same volumetric terrain representation
    // intended for the later Dark Below cavern chapter.
    static constexpr int TerrainMinZ = -12;
    static constexpr int TerrainMaxZ = 20;

    explicit World(int level=0,WorldId id=WorldId::Campaign);
    World(int level,std::shared_ptr<const AuthoredMapData> customMap);
    const std::vector<MapLayer>& layers()const{return m_layers;}
    int level()const{return m_level;}
    WorldId worldId()const{return m_worldId;}
    bool campaign()const{return m_worldId==WorldId::Campaign;}
    bool custom()const{return m_worldId==WorldId::Custom;}
    // A legacy hard-coded campaign chunk. Authored maps (including rebuilt
    // campaign slots) carry their own content, so legacy special cases skip them.
    bool campaignChunk(int index)const{return campaign()&&m_level==index&&!m_mapData;}
    bool authored()const{return m_mapData!=nullptr;}
    // Which definition this World's geometry was built from, for memos keyed on
    // its collision (ai::collisionSnapshot): the authored map it was loaded from
    // (the same across a chunk re-made on streaming, even where terrain settles
    // a private copy of it), or none for a built-in chunk, with world and slot.
    // An address names its map only while that map lives; a memo kept longer
    // than the World that keyed it must hold or check its owner too.
    struct DefinitionIdentity {const AuthoredMapData* map=nullptr;WorldId world=WorldId::Campaign;int level=0;bool operator==(const DefinitionIdentity&)const=default;};
    DefinitionIdentity definitionIdentity()const{return {m_definitionSource,m_worldId,m_level};}
    const ChunkDefinition& definition()const{return m_mapData?m_mapData->definition:chunkDefinition(m_worldId,m_level);}
    std::span<const int> visibleResidencyGroups()const{return m_mapData?std::span<const int>(m_mapData->visibleResidencyGroups):std::span<const int>{};}
    bool outdoors()const{return definition().environment==Environment::Outdoor;}
    bool hasLift()const{return definition().lift;}
    // Authored location name at a point (HUD), or nullptr when no zone covers it.
    const char* zoneName(Vec2 p,float z)const{
     if(!m_mapData)return nullptr;
     for(const auto& zone:m_mapData->zones)if(!zone.name.empty()&&p.x>=zone.x1&&p.x<zone.x2&&p.y>=zone.y1&&p.y<zone.y2&&z>=zone.bottom&&z<zone.top)return zone.name.c_str();
     return nullptr;
    }
    // The major-area title at a point, or nullptr.
    const char* areaTitle(Vec2 p,float z)const{
     if(!m_mapData)return nullptr;
     for(const auto& zone:m_mapData->zones)if(!zone.title.empty()&&p.x>=zone.x1&&p.x<zone.x2&&p.y>=zone.y1&&p.y<zone.y2&&z>=zone.bottom&&z<zone.top)return zone.title.c_str();
     return nullptr;
    }
    // The air at a point: the first zone there that sets a haze, else the
    // map's own, else the environment's (interior, outdoor, or by sky).
    Haze haze(Vec2 p,float z)const;
    // The map's surface sets (Surface values); SurfaceDefault keeps the legacy finish.
    int wallSurface()const{return m_mapData?m_mapData->wallSurface:SurfaceDefault;}
    int floorSurface()const{return m_mapData?m_mapData->floorSurface:SurfaceDefault;}
    int ceilingSurface()const{return m_mapData?m_mapData->ceilingSurface:SurfaceDefault;}
    const std::vector<DarkZone>& darkZones()const{static const std::vector<DarkZone> empty;return m_mapData?m_mapData->darkZones:empty;}
    bool horrorMode()const{return m_worldId==WorldId::Ashfall;}
    bool coast()const{return horrorMode()&&m_level>=12;}
    // The open-zone sky: procedural broken overcast, a warm sun with shafts
    // and low steam over the ground, on the same authored terrain.
    bool zoneSky()const{return m_mapData&&m_mapData->skybox=="zone_overcast";}
    // Painted ground cover at a world position, or -1 for the default terrain look.
    int groundKind(float worldX,float worldY)const;
    // The authored terrain shape, or null; and its continuation past the zone
    // edge (`outside` metres beyond it), rising toward the horizon hills.
    const TerrainShape* terrainShape()const{return m_exclusionTerrain&&m_mapData?m_mapData->terrain.get():nullptr;}
    float outskirtHeight(float worldX,float worldY,float outside)const;
    const char* skyboxId()const{return m_mapData?m_mapData->skybox.c_str():horrorMode()?"brutal_wasteland":"industrial_night";}
    const char* customMapName()const{return m_mapData?m_mapData->name.c_str():"";}
    bool openNorthBoundary()const{return m_openNorthBoundary;}
    bool openSouthBoundary()const{return m_openSouthBoundary;}
    bool openWestBoundary()const{return m_openWestBoundary;}
    bool openEastBoundary()const{return m_openEastBoundary;}
    // Where the way on lies: the transfer door, else the open seam onward.
    Vec2 exitPoint()const{
     for(const auto& door:m_doors)if(door.transfer)return {(door.left+door.right)*.5f,std::min(door.y+1.f,23.5f)};
     // An open seam: the opening nearest the middle of that edge (seams need
     // not be centred).
     auto open=[&](bool alongX,int fixed,Vec2 fallback){for(int d=0;d<12;++d)for(int k:{12+d,11-d}){int x=alongX?k:fixed,y=alongX?fixed:k;
      if(k>=0&&k<24&&tile(x,y)!='#')return alongX?Vec2{k+.5f,fixed==23?23.f:1.f}:Vec2{fixed==23?23.f:1.f,k+.5f};}return fallback;};
     if(m_openSouthBoundary)return open(true,23,{12.f,23.f});if(m_openEastBoundary)return open(false,23,{23.f,12.f});
     if(m_openNorthBoundary)return open(true,0,{12.f,1.f});if(m_openWestBoundary)return open(false,0,{1.f,12.f});
     return {21.5f,22.5f};}
    const std::vector<WorldProp>& props()const{return m_props;}
    const std::vector<Fixture>& fixtures()const{return m_fixtures;}
    const std::vector<AuthoredSign>& signs()const{static const std::vector<AuthoredSign> empty;return m_mapData?m_mapData->signs:empty;}
    const std::vector<AuthoredZone>& zones()const{static const std::vector<AuthoredZone> empty;return m_mapData?m_mapData->zones:empty;}
    const std::vector<PipeRun>& pipes()const{return m_pipes;}
    bool wallSpaceFree(Vec2 center,Vec2 along,float width,float bottom,float top)const;
    const std::vector<WorldLight>& lights()const{return m_lights;}
    const std::vector<CreatureSpawn>& creatureSpawns()const{return m_creatureSpawns;}
    const std::vector<PickupSpawn>& pickupSpawns()const{return m_pickupSpawns;}
    const std::vector<ClutterSpawn>& clutterSpawns()const{return m_clutterSpawns;}
    const std::vector<WaterVolume>& waterVolumes()const{return m_waterVolumes;}
    const std::vector<ParticleEmitter>& particleEmitters()const{return m_particleEmitters;}
    const std::vector<Shelter>& shelters()const{static const std::vector<Shelter> none;return m_mapData?m_mapData->shelters:none;}
    const std::vector<ScriptEvent>& scriptEvents()const{return m_scriptEvents;}
    const std::vector<Hazard>& hazards()const{return m_hazards;}
    const std::vector<Compactor>& compactors()const{return m_compactors;}
    const std::vector<Breakable>& breakables()const{static const std::vector<Breakable> none;return m_mapData?m_mapData->breakables:none;}
    // Electrical leads and their cables as simulated. Game steps them with the
    // clutter physics and decides when a clip tears; the cable itself lives here.
    const std::vector<Lead>& leads()const{static const std::vector<Lead> none;return m_mapData?m_mapData->leads:none;}
    const std::vector<HungSuit>& hungSuits()const{static const std::vector<HungSuit> none;return m_mapData?m_mapData->hungSuits:none;}
    const std::vector<LeadRope>& leadRopes()const{return m_leadRopes;}
    // Hang lead i's cable at rest from its anchor to `end` (its clip or fixed
    // end) and let it settle asleep; `body` is the prop it is clipped to (null:
    // none), which the cable hangs clear of. Limp (end null) it is only laid
    // over what is under it, awake: stepping settles it (Game::settleLeads).
    void hangLead(int i,const Point3* end,const LeadBody* body=nullptr);
    // One step of lead i's cable: gravity, its length, the floor and solids
    // under it and its clipped prop; `end` pins its far end (null: the end is free).
    void stepLead(int i,float dt,const Point3* end,const LeadBody* body=nullptr);
    // How far any point of lead i's cable moves in `seconds` of play from where it lies (a copy
    // is stepped, the world untouched): a cable at rest stays within millimetres, one left in mid-air falls.
    float leadUnrest(int i,float seconds,const Point3* end,const LeadBody* body=nullptr)const;
    bool fixtureHasParts(int i)const{for(const auto&b:breakables())if(b.fixture==i)return true;return false;}
    unsigned fixtureParts(int i)const{unsigned mask=~0u;for(int j=0;j<int(breakables().size());++j)if(breakables()[j].fixture==i&&!breakableIntact(j))mask&=~breakables()[j].part;return mask;}
    int breakableStructure(int i)const{return m_breakableFirst+i;}
    bool isBreakableStructure(int i)const{return i>=m_breakableFirst&&i<m_breakableFirst+int(breakables().size());}
    bool breakableIntact(int i)const;
    float breakableDamage(int i)const{return m_breakableDamage.at(i);}
    int breakableAt(float x,float y,float z,float margin=.03f)const;
    bool damageBreakable(int i,float amount);
    void ageBreakables(float dt);
    float breakableOpacity(int i)const{return breakables()[i].holds?std::clamp(21.f-m_breakableAge.at(i),0.f,1.f):1.f;}
    const std::vector<std::uint16_t>& brokenBreakables()const{return m_brokenBreakables;}
    const std::vector<Structure>& structures()const{return m_structures;}
    const std::vector<TerrainTriangle>& terrain()const{return m_terrain;}
    bool hasTerrain()const{return !m_terrain.empty();}
    // How wet floors are (0 dry .. 1 flooded): damp sheen everywhere, puddles
    // on level ground. Carpet never puddles; the Foundry runs hot and dry.
    float wetness()const{
     if(definition().wetness>=0)return definition().wetness;
     if(definition().floorMaterial==FloorMaterial::OfficeCarpet)return 0.f;
     if(zoneSky())return .38f;
     if(outdoors())return .55f;
     if(campaign()&&level()==0)return .12f;
     if(campaign()&&level()==1)return .65f;
     return .3f;
    }
    // Low overcast sun for the Ashfall exterior; indoor worlds have none.
    SunLight sun()const{if(!outdoors())return {};SunLight s;Point3 d{.50f,.30f,.55f};
     if(zoneSky()){d={-.62f,.48f,.42f};float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);s.direction={d.x/l,d.y/l,d.z/l};s.intensity=2.6f;s.r=1.f;s.g=.86f;s.b=.66f;return s;}float l=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);s.direction={d.x/l,d.y/l,d.z/l};s.intensity=2.1f;s.r=1.f;s.g=.92f;s.b=.80f;return s;}
    std::vector<Span> spansAt(int x,int y)const;
    // Reuse result storage for repeated cell queries.
    void spansAt(int x,int y,std::vector<Span>& result)const;
    float supportBelow(float x,float y,float feet,bool cavities=false)const;
    float structureSupportBelow(float x,float y,float radius,float feet)const;
    float clearanceAbove(float x,float y,float feet,bool cavities=false)const;
    float fixtureSurface(const Fixture& f,float x,float y,float feet,bool ceiling)const;
    bool fits(float x,float y,float feet,float height,bool dynamic=true,bool shelfCavities=false)const;
    // A heavy crate's box overlaps this column (skipping crate `except`).
    bool crateBlocks(float x,float y,float feet,float height,int except=-1)const;
    const std::vector<PushCrate>& pushCrates()const{return m_pushCrates;}
    const std::vector<CratePlate>& cratePlates()const{return m_cratePlates;}
    StateId crateReset()const{return m_mapData?m_mapData->crateReset:0;}
    const CargoLift& cargoLift()const{return m_cargoLift;}
    const std::vector<TimedSequence>& timedSequences()const{static const std::vector<TimedSequence> empty;return m_mapData?m_mapData->timedSequences:empty;}
    const std::vector<ActorTrack>& actorTracks()const{static const std::vector<ActorTrack> empty;return m_mapData?m_mapData->actorTracks:empty;}
    const ActorPose& actorPose(size_t index)const{return m_actorPoses.at(index);}
    void setActorPose(size_t index,ActorPose pose){m_actorPoses.at(index)=pose;}
    const std::vector<SequenceCue>& sequenceCues()const{static const std::vector<SequenceCue> empty;return m_mapData?m_mapData->sequenceCues:empty;}
    float cargoLiftHeight()const{return m_cargoLiftHeight;}
    void setCargoLiftHeight(float z);
    bool insideCargoLift(float x,float y)const{return m_cargoLift.x2>m_cargoLift.x1&&x>=m_cargoLift.x1&&x<m_cargoLift.x2&&y>=m_cargoLift.y1&&y<m_cargoLift.y2;}
    bool railBlocksHull(float x,float y,float radius,float feet,float height)const;
    bool structureBlocksHull(float x,float y,float radius,float feet,float height)const;
    bool structureSegmentClear(Vec2 start,float feet,Vec2 goal,float height,bool indexed=true,float radius=.20f)const;
    float wallHeight(int x,int y)const;
    bool controlReleased()const{return m_controlReleased;}
    void releaseControl(){m_controlReleased=true;}
    enum class LiftPhase { Ready, Ascending, Jammed, Falling, Caught, Crashed };
    static constexpr float LiftRideComplete=48.f;
    LiftPhase liftPhase()const{return m_liftPhase;}
    float liftHeight()const{return m_liftHeight;}
    float waterSurface(float x,float y)const;
    float liftPhaseTime()const{return m_liftTimer;}
    bool insideLift(float x,float y)const{return hasLift()&&x>=10&&x<14&&y>=10&&y<14;}
    bool liftMoving()const{return m_liftPhase!=LiftPhase::Ready&&m_liftPhase!=LiftPhase::Crashed;}
    bool startLift();
    void updateLift(float dt);
    void restoreLift(const World& saved);
    // A rebuilt chunk takes back what play changed: lift, reactor and broken crates.
    void restoreDynamic(const World& saved){restoreLift(saved);if(saved.m_leadRopes.size()==m_leadRopes.size())m_leadRopes=saved.m_leadRopes;m_breakableDamage=saved.m_breakableDamage;m_breakableAge=saved.m_breakableAge;for(auto i:saved.m_brokenBreakables)removeBreakable(i);if(saved.m_pushCrates.size()==m_pushCrates.size())m_pushCrates=saved.m_pushCrates;if(!saved.m_destroyedTiles.empty()){m_destroyedTiles=saved.m_destroyedTiles;m_lightSolids.clear();m_lightNodes.clear();}}
    const char* liftStatus()const;
    float liftLampPower()const;
    float liftMotorGain()const;
    enum class ReactorStage { NoDisk, DiskHeld, DiskLoaded, FeedPrimed, ReturnPrimed, Released };
    ReactorStage reactorStage()const{return m_reactorStage;}
    static Vec2 reactorDiskPosition(){return {6.f,21.f};}
    static constexpr float ReactorDiskZ=-5.215f; // On the upper workshop bench.
    bool takeReactorDisk();
    void useReactorTerminal(int action);
    // A stitched reactor chunk mirrors the lift chunk's puzzle state, so its
    // terminals read and its interlocks open from one source of truth.
    void syncReactor(const World& source);

    char tile(int x, int y) const;
    bool destroyTile(int x,int y);
    bool solid(float x, float y) const;
    bool isExit(float x, float y) const;
    // Legacy grating rule for the hard-coded chunks; authored maps lay their own floors.
    bool metalFloor(int x,int)const{return !outdoors()&&!m_mapData&&x>=7&&x<=16;}
    std::vector<Vec2> machines()const;
    float floorHeight(float x,float y)const;
    // What a shot struck at a point on a solid surface (level-local) with that surface's normal.
    ImpactMaterial impactMaterial(Point3 point,Point3 normal)const;
    float supportHeight(float x,float y,bool dynamic=true,bool shelfCavities=false)const;
    float ceilingHeight(float x,float y)const;
    // Headroom over the tile floor. A door's header caps it only for someone
    // below that header (a deck or catwalk over the door line passes above it).
    float clearanceHeight(float x,float y,float feet=-1e9f)const;
    bool rayClear(Vec2 a,float az,Vec2 b,float bz,bool doors=true,bool dynamic=true,bool shelfCavities=false)const;
    // Static optical visibility: exact solid intersections, separate from player hull clearance.
    bool lightRayClear(Vec2 a,float az,Vec2 b,float bz)const;
    // Nearest physical shot obstruction on a finite 3D segment. Actor hulls
    // are resolved by Game; this includes static and moving world geometry.
    float shotFraction(Point3 from,Point3 to,Point3&normal,ShotTrace* trace=nullptr)const;
    static bool testCollisionCandidates();
    bool doorBlocks(float x,float y,float feet,float height)const;
    // While a plan is alive, the doors it marks count as standing at maxOpen to
    // doorBlocks, so route planning for an actor that operates doors passes them.
    struct DoorPlan {
     const World& world;const std::vector<char>* previous;
     DoorPlan(const World& w,const std::vector<char>& open):world(w),previous(w.m_doorPlan.open){w.m_doorPlan.open=&open;}
     ~DoorPlan(){world.m_doorPlan.open=previous;}
     DoorPlan(const DoorPlan&)=delete;DoorPlan& operator=(const DoorPlan&)=delete;
    };
    bool doorPlanActive()const{return m_doorPlan.open!=nullptr;}
    // The live plan's door marks, or null: memos keyed on doorBlocks key on them.
    const std::vector<char>* doorPlanDoors()const{return m_doorPlan.open;}
    bool navigable(int x,int y,int nx,int ny,float height=1.f)const;
    void updateDoors(float dt,bool includeTransfers=true);
    bool openDoor(int index);
    bool toggleDoor(int index);
    void restoreDoors(const std::vector<Door>& doors){m_doors=doors;}
    void setDoor(int index,float open,bool opening){m_doors.at(index).open=open;m_doors.at(index).opening=opening;}
    void unloadGeometry(){m_fixtureRotation.clear();m_collisionCells={};m_collisionAll={};std::vector<LightSolid>{}.swap(m_lightSolids);std::vector<LightNode>{}.swap(m_lightNodes);std::vector<MapLayer>{}.swap(m_layers);std::vector<Structure>{}.swap(m_structures);std::vector<TerrainTriangle>{}.swap(m_terrain);std::vector<std::int8_t>{}.swap(m_terrainDensity);std::vector<std::uint8_t>{}.swap(m_terrainTops);std::vector<std::uint8_t>{}.swap(m_terrainMaterial);std::vector<std::vector<uint16_t>>{}.swap(m_structureCells);std::vector<WorldProp>{}.swap(m_props);std::vector<Fixture>{}.swap(m_fixtures);std::vector<PipeRun>{}.swap(m_pipes);std::vector<WorldLight>{}.swap(m_lights);std::vector<Hazard>{}.swap(m_hazards);std::vector<Compactor>{}.swap(m_compactors);std::vector<Terminal>{}.swap(m_terminals);}
    int nearbyDoor(Vec2 position,Vec2 forward,float feet=0)const;
    const std::vector<Door>& doors()const{return m_doors;}
    const std::vector<Terminal>& terminals()const{return m_terminals;}

private:
    CargoLift m_cargoLift{};
    float m_cargoLiftHeight=0;
    friend class Game; // Save codec persists dynamic state, never geometry or pointers.
    std::vector<MapLayer> m_layers;
    std::vector<std::pair<int,int>> m_destroyedTiles;
    int m_breakableFirst=0;std::vector<std::uint16_t> m_brokenBreakables;std::vector<float> m_breakableDamage,m_breakableAge;
    std::vector<LeadRope> m_leadRopes;
    void removeBreakable(int i);
    // Never copied: a plan belongs to the World it was made on.
    struct DoorPlanSlot {const std::vector<char>* open=nullptr;DoorPlanSlot()=default;DoorPlanSlot(const DoorPlanSlot&){}DoorPlanSlot& operator=(const DoorPlanSlot&){return *this;}};
    mutable DoorPlanSlot m_doorPlan;
    float m_internalWallHeight=0;
    int m_level=0;
    WorldId m_worldId=WorldId::Campaign;
    std::shared_ptr<const AuthoredMapData> m_mapData;
    const AuthoredMapData* m_definitionSource=nullptr;
    std::vector<ActorPose> m_actorPoses;
    void loadAuthoredMap(std::shared_ptr<const AuthoredMapData> map);
    bool m_openNorthBoundary=false,m_openSouthBoundary=false,m_openWestBoundary=false,m_openEastBoundary=false;
    std::vector<WorldProp> m_props;
    std::vector<Fixture> m_fixtures;
    std::vector<Compactor> m_compactors;
    std::vector<PipeRun> m_pipes;
    std::vector<WorldLight> m_lights;
    std::vector<CreatureSpawn> m_creatureSpawns;
    std::vector<PickupSpawn> m_pickupSpawns;
    std::vector<ClutterSpawn> m_clutterSpawns;
    std::vector<WaterVolume> m_waterVolumes;
    void buildPopulation();
    std::vector<ParticleEmitter> m_particleEmitters;
    std::vector<ScriptEvent> m_scriptEvents;
    std::vector<Hazard> m_hazards;
    void buildLights(int startX=2,int startY=1);
    std::vector<Door> m_doors;
    std::vector<PushCrate> m_pushCrates;std::vector<CratePlate> m_cratePlates;
    std::vector<Terminal> m_terminals;
    std::vector<Structure> m_structures;
    std::vector<TerrainTriangle> m_terrain;
    // One ghost sample on each horizontal side lets Surface Nets build seam
    // faces from neighbouring voxel cells without storing duplicate world maps.
    static constexpr int TerrainBorder=1;
    static constexpr int TerrainSamplesX=Width+3;
    static constexpr int TerrainSamplesY=Height+3;
    static constexpr int TerrainSamplesZ=TerrainMaxZ-TerrainMinZ+3;
    std::vector<std::int8_t> m_terrainDensity;
    float supportBelowUncached(float x,float y,float feet,bool cavities=false)const;
    bool fitsUncached(float x,float y,float feet,float height,bool dynamic,bool shelfCavities)const;
    // Highest positive sample per XY column; conservative empty-air bounds.
    std::vector<std::uint8_t> m_terrainTops;
    uint64_t m_terrainRevision=0;
    std::vector<std::uint8_t> m_terrainMaterial;
    // Custom outdoor wasteland packs (Exclusion Zone) sample a local heightfield
    // instead of the stitched Ashfall world voxels.
    bool m_exclusionTerrain=false;
    void buildTerrain();
    void buildTerrainBounds();
    void settleExclusionFacilityOnTerrain();
    std::size_t terrainSampleIndex(int sx,int sy,int sz)const;
    float terrainDensity(float x,float y,float z)const;
    float terrainSurfaceBelow(float x,float y,float feet)const;
    float terrainSurfaceAbove(float x,float y,float feet)const;
    struct LightSolid {Point3 center,half;float cosine=1,sine=0;};
    struct LightNode {Point3 minimum,maximum;int begin=0,end=0,left=-1,right=-1;};
    mutable std::vector<LightSolid> m_lightSolids;
    mutable std::vector<LightNode> m_lightNodes;
    void buildLightOcclusion()const;
    mutable std::array<std::vector<std::vector<size_t>>,4> m_collisionCells;
    mutable std::array<std::vector<size_t>,4> m_collisionAll;
    bool m_collisionCandidatesEnabled=true;
    mutable std::vector<Vec2> m_fixtureRotation;
    bool insideFixtureCandidate(size_t index,float x,float y,float margin=0)const;
    std::span<const size_t> wallCandidates(Vec2 center,Vec2 along,float width,std::vector<size_t>& scratch)const;
    bool solidReference(float x, float y) const;
    bool solidIndexed(float x, float y) const;
    bool wallSpaceFreeReference(Vec2 center,Vec2 along,float width,float bottom,float top)const;
    bool wallSpaceFreeIndexed(Vec2 center,Vec2 along,float width,float bottom,float top)const;
    float clearanceAboveReference(float x,float y,float feet)const;
    float clearanceAboveIndexed(float x,float y,float feet)const;
    std::vector<Span> spansAtReference(int x,int y)const;
    void spansAtIndexed(int x,int y,std::vector<Span>& result)const;
    void buildCollisionCandidates()const;
    std::span<const size_t> collisionCandidates(int kind,float x,float y)const;
    std::vector<std::vector<uint16_t>> m_structureCells;
    const std::vector<uint16_t>& structureIndices(float x,float y)const{static const std::vector<uint16_t> empty;int ix=int(std::floor(x)),iy=int(std::floor(y));return m_structureCells.empty()||ix<0||iy<0||ix>=Width||iy>=Height?empty:m_structureCells[iy*Width+ix];}
    bool m_controlReleased=false;
    LiftPhase m_liftPhase=LiftPhase::Ready;
    float m_liftHeight=0,m_liftTimer=0,m_liftVelocity=0;
    bool m_liftCaught=false,m_reactorFault=false;
    ReactorStage m_reactorStage=ReactorStage::NoDisk;
    void refreshReactorTerminals();
    void buildLayers(std::span<const Staircase> stairs);
};

}

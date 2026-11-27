#pragma once

#include <unordered_map>
#include <deque>
#include <map>
#include <atomic>
#include <filesystem>
#include <memory>
#include <functional>
#include "../world/SkinbotWalker.h"
#include "../core/Math.h"
#include "../world/World.h"
#include <vector>
#include <algorithm>
#include "../audio/Sound.h"
#include "../audio/NpcVoiceCatalog.h"
#include "Combat.h"
#include "SaveSections.h"
#include "Liquids.h"
#include "ai/guard/Config.h"
#include "ai/guard/GuardCtx.h"
#include "ai/Scheduler.h"
#include "ai/Nodes.h"
#include "ai/guard/Schedule.h"
#include "ai/enemy/Schedule.h"
#include "ai/friendly/Schedule.h"
#include "ai/friendly/Mind.h"
#include "ai/engineer/Drones.h"
#include <string>
#include <string_view>
#include <iosfwd>
#include <utility>
#include <optional>
#include "../net/NetTypes.h"
namespace retro::net{class Match;}
#include <tuple>
#include <cstdint>
#include "Ragdoll.h"
#include "Electricity.h"
#include "SoftBodies.h"
#include "physics/Vectra.h"
#include "Cloth.h"

namespace retro {
// Bumped once per chunk built while a save loads on a worker thread, so the
// loading bar can move; null (no counting) everywhere else.
inline thread_local std::atomic<int>* loadProgressCounter=nullptr;
struct LoadJob;
constexpr int DisplayWidth=640,DisplayHeight=360;
// Zero denotes an older animation bank whose legacy simulation clocks remain valid.
float cleanupAnimationDuration(ActorVisual visual,int clip);
int guardTreatmentDuration(const ActorTrack& track);


struct InputState {
    bool console=false;
    std::string textInput;
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool sprint = false;
    bool jump = false;
    bool crouch = false;
    bool prone = false;
    bool fire = false;
    bool guard = false;
    bool rotateProp = false;
    bool physglove = false;   // G: the Physgloves pull the prop pointed at (Physgloves.cpp)
    bool reload = false;
    int weaponScroll=0;
    bool use=false;
    bool flashlight=false;
    bool subdue=false;
    bool mute = false, music = false;
    bool escape=false, menuUp=false,menuDown=false,menuLeft=false,menuRight=false,menuAccept=false;
    bool scoreboard=false;   // Tab, or the controller's back/view button, held: the match's scoreboard
    bool chat=false;         // T in a match: opens the chat line (typed text then arrives in textInput)
    bool talk=false;         // the push-to-talk key (Settings::pushToTalkKey, V by default), held
    bool inventory=false;
    // 1-4 while a number key is held: a reply in a conversation. Outside one,
    // 1/2/3 pick fists, the shotgun or the handgun.
    int choice=0;
    // A direct weapon pick from the VR shoulder slot: 1 fists, 2 shotgun, 3 handgun.
    int weaponSlot=0;
    int pointerX=-1,pointerY=-1;
    Vec2 controllerMove{};  // Right and forward, normalized together.
    Vec2 controllerLook{};  // Yaw and pitch in radians per second.
    bool padActive=false;   // the controller was the last thing touched (its button names in the prompts, aim assist)
    float mouseDx = 0.0f;
    float mouseDy = 0.0f;
    bool vrTracked=false;
    std::array<bool,2> vrGrip{};
    // Each hand's trigger (by role: 0 off hand, 1 gun hand); fire is the gun hand's.
    std::array<bool,2> vrTrigger{};
    float vrYaw=0,vrPitch=0,vrEye=1.42f,vrMoveForward=0,vrMoveRight=0;
};

struct Enemy {
    using Kind=CreatureKind;
    Vec2 pos{};
    float hp = 110.0f;
    float attackCooldown = 0.0f;
    float painFlash = 0.0f;
    bool alive = true;
    Kind kind = Kind::Huntsman;
    float maxHp=110, deathTime=0, windup=0, strike=0, heading=0, gait=0;
    bool moving=false;
    // Its calls while aware, and its idle sound while unaware (archived where
    // the Brute's old step clock was).
    float voiceTimer=0,idleTimer=0;
    float z=0,awareness=0,searchTime=0;
    // How far it has made the player out (0..1, over its traits' noticeMs):
    // below 1 it has only seen something. Archived from version 20; an older
    // save rebuilds it from the hunt (a Chase has them made out).
    float recognition=0;
    float verticalVelocity=0,repathTimer=0,lastKnownZ=0;
    Vec2 waypoint{};
    Vec2 home{},lastKnown{};
    enum class State {Idle,Investigate,Chase,Search};State state=State::Idle;
    // Grab and Recover are the Snatcher's: holding its prey, sprawled after a lunge.
    enum class StalkMode {Watch,Flank,Rush,Retreat,Grab,Recover};StalkMode stalkMode=StalkMode::Watch;
    float stalkTimer=0,stalkSide=1;
    int surfaceMode=0; // floor, wall, ceiling
    Vec2 surfaceNormal{};
    // The Snatcher's fade: the cover it chose when it was seen, and how many times
    // the player has made it give ground since its last snatch. Not archived.
    Vec2 hideAt{};bool hideSet=false;int fades=0;
    // No route to its goal: it ambles a cell at a time instead of re-planning
    // every frame. Not archived.
    bool lostPath=false;
    // Presentation transitions are rebuilt after loading; gameplay clocks stay authoritative.
    int visualClip=-1,previousVisualClip=0;float visualPhase=0,previousVisualPhase=0,visualTransition=1;
    // Ephemeral MaintainSchedule mind. Rebuilt from state after load; not archived.
    int schedule=0,scheduleStep=0,scheduleTask=0,scheduleStatus=0,scheduleMs=0,scheduleCursor=0,scheduleCursorSaved=0,scheduleTransitions=0;
    unsigned scheduleConditions=0;
    bool scriptSpawned=false; // Persistent population owned by a script, not the authored baseline.
    // The locker it knows its prey is shut in (fixture+1), and the stun after
    // being thrown off those doors (LockerCapture.cpp). Not archived.
    int lockerHunt=0;float lockerStun=0;
    StateId wakeState=0;      // Dormant, blind and deaf until this state is set or it is hurt.
    float homeZ=-999;         // The floor it walks back to; -999 (an older save) uses its last known floor.
    float scale=1;            // Authored size: its body, hull and reach grow with it.
    // An authored creature that followed the player across a seam keeps the chunk
    // and home it was placed at, so a load matches it there instead of respawning it.
    int originLevel=-1;Vec2 originHome{};
    // A recurring hunter (CreatureSpawn::hunter, prey) and, while it comes after
    // the player through a seam, the seconds it still has to cover to the
    // crossing it walks in at (waypoint). Archived from version 22.
    StateId hunter=0;int prey=-1;float arrive=0;
    // Not saved: feeding on a kill (seconds left, the body's actor, its hp when it
    // began, so a wound ends it), a hunter's flight (seconds) and the last noise
    // of the player's it went to by ear (Game elapsed).
    float feeding=0,feedHp=0,fleeTime=0,earTime=-1;int feedActor=-1;
    // Not saved: the cursed equipment's seconds in live current (Machine.cpp), the clock of its
    // sparks, and whether the plant's current is what killed it (its lamp blown, its death a short).
    float shock=0,sparkClock=0;bool shorted=false;
    // A hunter that got away: not dead, gone (alive off, its hp kept).
    bool gone()const{return !alive&&hp>0&&hunter!=0;}
    // Not saved: how it died, for the body it leaves (Carcass), and the shots that struck it
    // (each ray's origin relative to pos and z), for the wounds drawn on it.
    struct Wound {Point3 origin{},direction{};};
    int deathSerial=0,deathSurface=0;Point3 deathPush{},blastOrigin{};bool deathBlast=false;
    std::array<Wound,6> wounds{};int woundCount=0;
    void wound(Point3 origin,Point3 direction){wounds[size_t(woundCount++)%wounds.size()]={origin-Point3{pos.x,pos.y,z},direction};}
    // A body lies where it fell, then thins out to nothing over its last seconds.
    static constexpr float CorpseLifetime=15.f,CorpseFade=3.5f;
    float dissolve()const{return alive?0.f:std::clamp((deathTime-(CorpseLifetime-CorpseFade))/CorpseFade,0.f,1.f);}
    bool visible()const{return alive||deathTime<CorpseLifetime;}
    float bodyBottom()const{return z+(kind==Kind::Wasp?.55f:0.f);}
    float bodyTop()const{return z+creatureTraits(kind).top*scale;}
    float hull()const{return creatureTraits(kind).hull*scale;}
    float reach()const{return creatureTraits(kind).reach*scale;}
    float radius()const{return creatureTraits(kind).radius*scale;}
    float contactRadius()const{return creatureTraits(kind).contactRadius*scale;}
    // A kind that stands dead until woken (CreatureTraits::dormant) keeps its wake state set while it sleeps.
    bool sleeping()const{return alive&&wakeState!=0&&creatureTraits(kind).dormant;}
    // Map format 2 overrides of its CREATURE record (display name, look, side, order). Not saved:
    // a load takes them from the authored spawn it matches (adoptTraits).
    EntityTraitsRef traits;
    const char* name()const{return traits&&!traits->display.empty()?traits->display.c_str():creatureTraits(kind).name;}
    // Who it fights as (factionsHostile): every authored and scripted creature is a
    // Creature. A console summon (Summon.cpp) may be anything; its serial (0: not a
    // summon), the standing order it keeps with nothing to fight (SummonOrder),
    // where that order is anchored, and whether the console holds it still. None
    // of this is saved: a save leaves summons out (they are transient).
    AiFaction faction=AiFaction::Creature;std::uint32_t summon=0;std::uint8_t order=0;bool frozen=false;
    bool babyHuntsman=false; // Transient summon variant of the adult species.
    // An authored creature given an order (CREATURE ai=): it keeps it as a summon does.
    bool ordered=false;
    // What a fresh spawn of the same record knows that a save does not keep (traits, side, order).
    void adoptTraits(const Enemy& spawn){traits=spawn.traits;faction=spawn.faction;ordered=spawn.ordered;order=spawn.order;anchor=spawn.anchor;anchorZ=spawn.anchorZ;patrolTo=spawn.patrolTo;patrolToZ=spawn.patrolToZ;}
    Vec2 anchor{},patrolTo{};float anchorZ=0,patrolToZ=0,orderClock=0,fleeClock=0;
    // Not saved: which player it hunts (Game::players() id; Game::creatureTarget), since when, and the player
    // who last wounded it and when (aggro). Single player: always the one player, 0.
    int target=0,aggro=-1;float targetSince=-99,aggroTime=-99;
};

struct Pickup {
    using Kind=PickupKind;
    Vec2 pos{};
    Kind kind = Kind::Health;
    bool active = true;
    float z = -999;
};
enum class GuardState {Patrol,Suspicious,Investigate,Search,Alert};
// step: a footstep, heard however far its floor and gait carry it (Game::stepHeard).
struct AiStimulus {Vec2 source{};float z=0,radius=0,gain=0;bool loud=false;int npc=-1;unsigned categories=ai::SoundWorld;bool step=false;};
struct NpcWeaponPose {bool armed=false,handgun=false,maintenance=false,crouched=false,supportAnimation=false,padVisible=false,pistolVisible=false;int supportClip=6;float supportPhase=0,treatment=0,engineering=0;Point3 grip{},muzzle{},direction{};float raise=0,flash=0,recoil=0;GuardAction action=GuardAction::None;float actionProgress=0,reload=0,grenadeThrow=0,haul=0;Point3 handle{};};
enum class ShotKind {None,Solid,Enemy,Npc,Actor,Player,Ragdoll,Drone,Prop};
struct ShotPassage {int kind=0,level=0,index=-1;float distance=0;Point3 point{},normal{};};
struct ShotHit {ShotKind kind=ShotKind::None;int level=0,index=-1;float distance=0,damageDistance=0;Point3 point{},normal{};int breakable=-1;std::array<ShotPassage,2> passed{};int waterLevel=-1;Point3 waterPoint{};float waterDistance=0;};
struct AiTargetRef {ShotKind kind=ShotKind::None;int level=0,index=-1;};
struct TacticalGrenadePose {Point3 position{};float fuse=0;int level=0;};
struct AiTarget : AiTargetRef {
 Vec2 position{};float feet=0,height=0,eye=0;AiFaction faction=AiFaction::Neutral;
 bool valid()const{return kind==ShotKind::Player||kind==ShotKind::Enemy||kind==ShotKind::Npc||kind==ShotKind::Actor||kind==ShotKind::Drone;}
};
struct FriendlyActor {
 // The authored identity owns state keys; level/pose describe physical location.
 // Keep the immutable track alive when its home chunk releases geometry.
 int homeLevel=0,level=0;size_t homeActor=0;ActorPose pose{};
 std::shared_ptr<const AuthoredMapData> definition;
 // A console summon (Summon.cpp): its serial, its track the only one in its own
 // definition, its homeActor SummonActorBase+serial so no authored (level, actor)
 // identity ever matches it, and no home world pose to write back. Not saved.
 std::uint32_t summon=0;bool frozen=false;
 static constexpr size_t SummonActorBase=size_t(1)<<24;
 const ActorTrack& track()const{return definition->actorTracks.at(summon?0:homeActor);}
};
// A Cleanup man on the patched-through yard cameras: where he stands in the
// current chunk, which way he looks, how far he sees and how wide (cone is the
// half-angle in radians), and what he makes of the player.
struct RadarContact {Vec2 position{};float yaw=0,range=0,cone=0;GuardState state=GuardState::Patrol;size_t actor=0;};
// material: an ImpactMaterial, or -1 for marks made before shots knew what they struck. size: a
// blood mark's half-width (0 for the usual).
struct BulletImpact {Vec2 pos{};float z=0;Vec2 normal{};float time=0;int level=0;bool blood=false;int tint=0;float normalZ=0;int material=-1;float size=0;
 int door=-1;float doorOpen=0;};   // a mark on a door's leaf: that door, and how far open it was then (it rides the leaf: Door::carry)
// Every shot fired, for the bodies it passes through: a ray in the frame of `level` (the active
// chunk when it was fired), as far as it flew.
struct ShotRay {Point3 origin{},direction{};float range=0,time=0,damage=0,impulse=0;int level=0;};
// What a hit throws off (GoreFx): blood from flesh (species: a CreatureKind, or -1 for a person),
// dust and chips from masonry, splinters from wood, clods from earth. Level-local. floor: where
// what it throws comes to rest; reach: how far along `direction` before a wall stops it.
struct HitBurst {Point3 point{},direction{};float time=0;int level=0;ImpactMaterial material=ImpactMaterial::Concrete;int species=-1;float power=1;bool kill=false;float floor=0,reach=9;bool stump=false;};
// Something meeting standing water (WaterFx.cpp): a round, a thing or a body falling in, a step, a
// wake. Its splash (Particles) and its rings on the surface (WaterRipples) scale with power (a round
// .65-1.5; a fall by the root of its energy, .25-3). A splash due later (a body collapsing) is not
// yet sounded; Game::updateWater raises its sound when its time comes.
enum class WaterSplashKind:int {Shot,Entry,Step,Wake,Body};
struct WaterSplash {Point3 point{},direction{};float time=0;int level=0;float power=1;WaterSplashKind kind=WaterSplashKind::Shot;bool sounded=true;};
struct BarrelExplosion {Vec2 pos{};float z=0,time=0;int level=0;};
struct PropTraits {PropMaterial material;float mass,health;std::array<float,3> size;};
// model: the facility model that broke (its chips take its own sheet), or -1.
struct BreakEvent {Vec2 pos{};float z=0;Point3 direction{};float time=0;int level=0;PropMaterial material=PropMaterial::Glass;float width=.1f,height=.1f;bool pane=false;Vec2 along{};int index=-1;int model=-1;};
// What a shard (clutter kind 6) was broken from, so it is drawn as a piece of that thing: a crate's
// splinter (the old look), an eighth of an exploded barrel's hull, a chunk of a drop-ceiling tile, or
// an eighth of a broken prop or fixture part cut from its own mesh with its own sheet.
// Glass: a piece of a pane (GlassFracture.h), a thin sheet whose outline its piece number picks.
enum class ShardOf:int {Crate,Barrel,Ceiling,Model,Glass,Count};
// The drawn size of a 'B' barrel (Scene3D draws it 1.1 m tall in a .94 m footprint): its shards are eighths of it.
inline constexpr std::array<float,3> kBarrelSize{.74f,.74f,1.1f};
// Where a pane gave way and which way the blow came from, so its shards fall
// from the hole and lie where they landed. Not saved: a reloaded pane breaks
// about its centre.
struct GlassBreak {int level=0,index=0;Point3 at{},direction{};};
// What struck a pane (Game::damagePane): a round holes it where it hits; a fist punches a
// fist-sized hole; a prop (radius: its own) punches a hole its size once the blow beats the
// glass there; a body coming through, or a blast, takes the whole pane.
struct PaneBlow {enum Kind {Round,Fist,Prop,Body,Blast};Kind kind=Round;float radius=-1;};
struct Clutter {
 Vec2 pos{},velocity{};float z=0,vz=0,yaw=0,spin=0;int kind=0;bool projectile=false;float impactCooldown=0;
 float pitch=0,roll=0,pitchSpeed=0,rollSpeed=0,restTime=0;bool sleeping=false;
 int model=-1;std::array<float,3> dims{};float damage=0;bool authored=false,broken=false;Vec2 home{};float homeZ=0;StateId breakState=0;int soundOwner=-1;float blockedTime=0;
 // Seconds until a blow on a pane counts again: a prop held against glass (shoved into it, resting on
 // it) strikes it once, not every step (not saved: a loaded prop strikes afresh).
 float paneRearm=0;
 // Seconds a blow from another prop leaves it free to pivot on its base edge (not saved).
 float tipTime=0;
 // How loud its scrape and roll loops run (0..1), eased toward its contacts' slide and roll speeds each step (not saved).
 float scrapeLevel=0,rollLevel=0;
 // Separate from sound throttling: a resting body contact must not deal repeated damage.
 float bodyImpactCooldown=0;
 // Carried along in a moving container this step (Containers.h): the solver leaves it be (not saved).
 bool riding=false;
 // Vectra's orientation (physics/PropBodies.h): a unit quaternion (w, x, y, z), and the Euler
 // angles it last wrote. yaw/pitch/roll stay what everything else reads and writes; when they differ
 // from these (an outside write: seeding, a carry, a save), the body is turned to them. Angular velocity
 // lives in spin (world z), pitchSpeed and rollSpeed (about the yaw-turned y and x axes): exact both ways.
 // island: the sleep island it rests in (-1 awake). Not in the save stream (the physics save section).
 std::array<float,4> orient{1,0,0,0};std::array<float,3> orientEuler{1e9f,1e9f,1e9f};int island=-1;
 // A mattress or a pad (physics/PropBodies.h, chained): three segments along its length on springy hinges.
 // bend: each joint's bend (rad, the outer segment's turn about the width axis); bendSpeed its rate; boneRoot
 // which segment the prop's own frame is (1, the middle; the end in the hand while it is carried).
 std::array<float,2> bend{0,0},bendSpeed{0,0};int boneRoot=1;
 // A shard (kind 6): what it was cut from, which eighth of it (bit 0 +x, bit 1 +y, bit 2 up, in
 // the thing's own frame), the facility model it came from (ShardOf::Model) and that thing's size
 // (x, y, up). Its own size is `dims` when set (half the source each way), else the old splinter's.
 ShardOf shardOf=ShardOf::Crate;int piece=0,shardModel=-1;std::array<float,3> from{};
 // Map format 2 overrides of the CLUTTER record it came from (EntityTraits.h): mass, health,
 // material, friction, restitution, density, container volume, impact sound, look. Not saved:
 // a load takes them again from the authored prop it matches (Saves.cpp reconcile).
 EntityTraitsRef traits;
 // Held where it was put: anchored, or frozen (a named prop's `frozen` property, which a script
 // may clear). Game::refreshPinnedClutter sets it each step; the solver leaves it be.
 bool pinned=false;
 // Liquid in it (Liquids.h: a bottle's water, oil or chemical, its sloshing surface, a hole): saved in the "liquids" section.
 liquid::State liquid;
 // A match's name for it (docs/Systems/multiplayer.md "Props"): (map+1)<<16 | its CLUTTER record+1 for an
 // authored prop (seedClutter, the same on every machine), (map+1)<<16 | 0x8000+n for one the host spawned,
 // NetDebris for a shard or gib (each machine's own, never sent). netHolder: on a host, the slot of the guest
 // carrying it (the guest's hand poses it; the solver leaves it be), else -1. Neither is saved.
 static constexpr std::uint32_t NetDebris=0xFFFFFFFFu;
 std::uint32_t netId=0;int netHolder=-1;
 PropMaterial material()const;float mass()const;float supportHeight()const;
 float friction()const{return traits&&traits->friction>=0?traits->friction*8.f:model>=0?propMaterialTraits(material()).friction:7.f;}
 float restitution()const{return traits&&traits->restitution>=0?traits->restitution:propMaterialTraits(material()).restitution;}
 bool footprintContains(Vec2 point)const{auto d=point-pos;if(std::fabs(pitch)+std::fabs(roll)<.1f){float c=std::cos(yaw),s=std::sin(yaw);return std::fabs(d.x*c+d.y*s)<=dims[0]*.5f+.001f&&std::fabs(-d.x*s+d.y*c)<=dims[1]*.5f+.001f;}auto e=extent();return std::fabs(d.x)<=e[0]+.001f&&std::fabs(d.y)<=e[1]+.001f;}
 std::array<float,3> size()const{if(model>=0||(kind==6&&dims[0]>0))return dims;constexpr std::array<float,3> sizes[]={{.22f,.22f,.003231f},{.156383f,.25f,.054035f},{.082788f,.083595f,.28f},{.24f,.222403f,.139543f},{.263634f,.28f,.014386f},{.12f,.11894f,.006745f},{.34f,.045f,.009f}};return sizes[kind];}
 std::array<float,3> rotate(float x,float y,float localZ)const{
  float cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll),cy=std::cos(yaw),sy=std::sin(yaw);
  float a=cp*x+sp*localZ,b=-sp*x+cp*localZ,c=cr*y-sr*b,d=sr*y+cr*b;
  return {cy*a-sy*c,sy*a+cy*c,d};
 }
 std::array<float,3> extent()const{auto s=size(),a=rotate(s[0]*.5f,0,0),b=rotate(0,s[1]*.5f,0),c=rotate(0,0,s[2]*.5f);return {std::fabs(a[0])+std::fabs(b[0])+std::fabs(c[0]),std::fabs(a[1])+std::fabs(b[1])+std::fabs(c[1]),std::fabs(a[2])+std::fabs(b[2])+std::fabs(c[2])};}
 // Horizontal projection in model orientation; avoids empty world-AABB corners.
 std::array<Vec2,9> footprintOffsets()const{auto local=*this;local.yaw=0;auto e=local.extent();std::array<Vec2,9> points;int i=0;float co=std::cos(yaw),si=std::sin(yaw);for(float x:{-e[0],0.f,e[0]})for(float y:{-e[1],0.f,e[1]})points[size_t(i++)]={x*co-y*si,x*si+y*co};return points;}
 // The footprint's outline every `spacing` metres (corners and edges, in model orientation) and
 // its middle: f(offset) for each until one returns false. A wall thinner than the nine
 // footprintOffsets' gaps (half a big prop's width) still meets the outline.
 template<class F> bool everyOutlinePoint(F&& f,float spacing=.1f)const{auto local=*this;local.yaw=0;const auto e=local.extent();const float co=std::cos(yaw),si=std::sin(yaw);
  auto at=[&](float x,float y){return f(Vec2{x*co-y*si,x*si+y*co});};if(!at(0,0))return false;
  const int nx=std::max(1,int(std::ceil(2*e[0]/spacing))),ny=std::max(1,int(std::ceil(2*e[1]/spacing)));
  for(int i=0;i<=nx;++i){const float x=-e[0]+2*e[0]*float(i)/float(nx);if(!at(x,-e[1])||!at(x,e[1]))return false;}
  for(int j=1;j<ny;++j){const float y=-e[1]+2*e[1]*float(j)/float(ny);if(!at(-e[0],y)||!at(e[0],y))return false;}
  return true;}
 float height()const{return extent()[2]*2;}
 float footprint()const{auto s=size();return std::max(s[0],s[1]);}
 // The pieces a thing breaks into (kind 6 shards of `of`): `count` of its eighths, each where
 // that eighth sat in it (centre: its middle; size: x, y, up; yaw: its heading), thrown out from
 // the middle at `speed` and up at `lift`. ShardOf::Ceiling gives small slabs of a tile instead.
 static std::vector<Clutter> shards(ShardOf of,Point3 centre,std::array<float,3> size,float yaw,int count,float speed,float lift,int model=-1);
 Sound impactSound()const{if(traits&&traits->impact>=0)return Sound(traits->impact);if(model>=0||(kind==6&&shardOf!=ShardOf::Crate))return propMaterialTraits(material()).impact;return kind==2?Sound::JunkGlass:kind==0||kind==1||kind==5||kind==6?Sound::JunkSoft:Sound::JunkMetal;}
};

// One firearm's rounds: total counts the loaded ones too, so the reserve is
// total-loaded and loaded<=total always.
struct AmmoPool {int total=0,loaded=0;bool operator==(const AmmoPool&)const=default;};
// A spent case in the world (Casings.cpp): thrown from a gun's ejection port, it
// tumbles, bounces off floors, walls and props with a clink, then lies a while.
// In the active chunk's coordinates; kind 0 a shotgun shell, 1 9 mm brass, 2 .380
// brass. Runtime only: not saved, cleared with the map.
struct Casing {Point3 position{},velocity{},spin{};float yaw=0,pitch=0,roll=0,age=0,still=0;int kind=0,bounces=0;bool resting=false;};
// A downed sidearm carrier's unlooted pistol on the floor, in its level's local coordinates.
struct DroppedSidearm {Point3 position{};float yaw=0;int level=0;size_t actor=0;};
struct Player {
    static constexpr float StandingHeight=1.6f;
    // Three stances. A crouch is a kneel (her crouch clips put her eyes near
    // a metre); prone is the old .58 m crawl hull every low passage is built
    // for. Crouching where a kneel cannot fit drops her prone.
    static constexpr float CrouchedHeight=1.05f;
    static constexpr float ProneHeight=.58f;
    static constexpr float StandingEye=1.42f;
    static constexpr float CrouchedEye=.92f;
    static constexpr float ProneEye=.40f;
    Vec2 pos{2.5f, 2.5f};
    float angle = 0.0f;
    float pitch = 0.0f;
    float health = 100.0f;
    // Rounds per firearm, indexed by WeaponId (Fists holds none).
    std::array<AmmoPool,5> pools{AmmoPool{},AmmoPool{36,6},AmmoPool{},AmmoPool{},AmmoPool{}};
    AmmoPool& pool(WeaponId id){return pools[size_t(id)];}
    const AmmoPool& pool(WeaponId id)const{return pools[size_t(id)];}
    float z = 0.0f;
    float verticalVelocity = 0.0f;
    bool grounded = true;
    bool crouched = false;   // crouched or prone: low and quiet
    float eye=StandingEye;
    bool prone = false;      // appended, not saved: re-derived from the room on load
    float hullHeight()const{return prone?ProneHeight:crouched?CrouchedHeight:StandingHeight;}
    float stanceEye()const{return prone?ProneEye:crouched?CrouchedEye:StandingEye;}
};
// sprint (appended, not saved: it settles in a fraction of a second) is how
// far a firearm has dropped to low ready while running.
struct WeaponMotion {float yaw=0,pitch=0,bob=0,back=0,elbow=0,bolt=0,roll=0,sprint=0;};
// depthOfField and lensEffects scale the cinematic lens (1 = the tuned look, 0 off, up to 2).
// VR comfort (the VR COMFORT page; VrRuntime reads them): turn speed (1 = 120 degrees a second; turning is smooth
// only), the vignette's strength while moving or turning (0 off), seated play, a left-handed avatar (gun hand, hip
// holster and sticks swapped), the frame-rate readout, and the render scale (0 automatic, else 0.6-1 fixed).
// Controller options (SETTINGS > CONTROLS, docs/Systems/accessibility.md): for hands that shake, tire or cannot hold a
// trigger. Indexes into the tables in Win32Window.cpp: look speed, stick dead zone, steady aim (a low-pass on the
// sticks), and the press filter (a button must be held that long before it counts). curve: how the look stick's
// travel becomes speed (0 linear, 1 standard, 2 precise: slower near the middle); assist: aim help on the pad
// (0 off, 1 light, 2 strong: the view slows over a target and drifts with it); rumble: 0 off, 1 light, 2 standard.
// All provisional: a player with cerebral palsy is trying them.
struct PadOptions {int look=3,dead=0,steady=0,filter=0;bool toggleAim=false,toggleSprint=false,toggleCrouch=false,bumpers=false,swapSticks=false;int curve=1,assist=1,rumble=2;};
struct Settings {PadOptions pad;float master=1,music=.75f,effects=1,sensitivity=1;bool invertMouse=false;float depthOfField=1,lensEffects=1;
 float vrTurnSpeed=1,vrVignette=.5f;bool vrSeated=false,vrLeftHanded=false,vrFrameRate=false;float vrRenderScale=0;
 int vrHeightCm=0,vrArmLength=0;   // VR body fit overrides: YOUR HEIGHT (cm, 0 AUTO), ARM LENGTH (0 AUTO, 1 SHORT, 2 AVERAGE, 3 LONG)
 int pullHighlight=1;   // pullHighlight: the Physgloves target's outline, 1 subtle, 0 none
 int viewDistance=1;    // viewDistance: VIDEO > VIEW DISTANCE, 0 near (x0.7), 1 normal, 2 far (x2), 3 very far (x3.5)
 std::string playerName; // playerName: MULTIPLAYER > NAME, what others see; empty uses the Windows user name
 int character=0;       // character: the player's body, 0 Claire, 1 the Limitator, 2 Leon (SoftwareRenderer::playerBody)
 // Communication (the COMMUNICATION page; docs/Systems/multiplayer.md "Chat and voice"): text chat and its word
 // filter; voice 0 off, 1 push to talk, 2 open mic; the push-to-talk key (a Windows virtual-key code); open mic's
 // threshold on the mic meter (0-1); mic and speakers by device name ("" the system default); mic and voice
 // volumes (0-2, 1 as it comes); proximity voice in co-op; players muted, by display name.
 bool textChat=true,chatFilter=true;int voiceMode=1,pushToTalkKey=0x56;float openMicThreshold=.25f,micVolume=1,voiceVolume=1;bool proximityVoice=true;
 std::string micDevice,speakerDevice;std::vector<std::string> mutedPlayers;
 bool muted(const std::string& name)const{for(const auto& n:mutedPlayers)if(n==name)return true;return false;}
};
// The save and load pages and their confirmations: a slab in the middle (PAUSE and SETTINGS have their own layouts).
struct MenuLayout {static constexpr int X=(DisplayWidth-304)/2,Y=(DisplayHeight-330)/2,Width=304,Height=351,RowTop=Y+46,RowHeight=21,Rows=13,SliderX=X+179,SliderWidth=75;};
// PAUSE (SettingsMenu.cpp; drawn by renderer/FrontEnd.cpp) in the title's language: grouped rows from X,Y. Out of a
// match: RESUME, SETTINGS | SAVE GAME, LOAD GAME | RESTART, QUIT TO TITLE. In a match: RESUME, SETTINGS | JOIN CODE |
// LEAVE MATCH. rowY and rowAt are shared by the pointer and the renderer.
enum PauseRow:int{PauseResume=0,PauseSettings=1,PauseSave=2,PauseLoad=3,PauseRestart=4,PauseQuit=5,PauseMatchCode=2,PauseMatchLeave=3};
struct PauseMenuLayout {
 static constexpr int X=66,Y=150,Width=250,RowHeight=22,Gap=10;
 static constexpr int rows(bool match){return match?4:6;}
 static constexpr int rowY(int row,bool match){return Y+row*RowHeight+(row>=2?Gap:0)+(row>=(match?3:4)?Gap:0);}
 static constexpr int rowAt(int y,bool match){for(int r=0;r<rows(match);++r)if(y>=rowY(r,match)&&y<rowY(r,match)+RowHeight)return r;return -1;}
};
// SETTINGS (SettingsMenu.cpp; drawn by renderer/FrontEnd.cpp): the column of pages on the left (BACK under them),
// the open page's rows on the right, the chosen row's one plain line at HelpY. Sliders run from SliderX.
enum class SettingsTab:std::uint8_t{Audio,Video,Controls,Controller,Gameplay,Communication,Vr,Count};
struct SettingsLayout {
 static constexpr int TabX=40,TabY=72,TabWidth=140,TabHeight=19,BackGap=10;
 static constexpr int RowX=206,RowY=72,RowWidth=394,RowHeight=17,SliderX=RowX+214,SliderWidth=104,HelpY=306;
 static constexpr int tabY(int tab){return TabY+tab*TabHeight+(tab>=int(SettingsTab::Count)?BackGap:0);}   // tab Count is BACK
 static constexpr int rowY(int row){return RowY+row*RowHeight;}
};
// What SETTINGS shows: the page column and the open page's rows, where the pointer finds them and the renderer draws them.
struct MenuItem {
 enum Kind:std::uint8_t{Tab,Slider,Toggle,Choice,Action,Back} kind=Action;
 int x=0,y=0,w=0,h=0;
 std::string label,value,help;
 float fraction=-1;     // a slider's position, 0-1
 float meter=-1;        // a live level under the slider (the mic), 0-1; -1 none
 bool enabled=true,current=false,armed=false,warn=false;   // current: the open page's tab; armed: waiting for its confirm
 int id=0;
};
// The title's main menu (Settings.cpp, MultiplayerMenu.cpp; drawn by renderer/FrontEnd.cpp) in three groups: the
// campaign (new game, custom maps, load game), MULTIPLAYER, then settings and quit, a gap between groups. rowY and
// rowAt are shared by the pointer and the renderer. The custom map archive's rows are evenly spaced from Y.
struct TitleMenuLayout {
 static constexpr int X=66,Y=150,Width=250,RowHeight=22,Rows=6,Gap=10;
 static constexpr int rowY(int row){return Y+row*RowHeight+(row>=3?Gap:0)+(row>=4?Gap:0);}
 static constexpr int rowAt(int y){for(int r=0;r<Rows;++r)if(y>=rowY(r)&&y<rowY(r)+RowHeight)return r;return -1;}
};
enum TitleRow:int{TitleNewGame=0,TitleCustomMaps=1,TitleLoadGame=2,TitleMultiplayer=3,TitleSettings=4,TitleQuit=5};
// One row of a multiplayer page on the title (MultiplayerMenu.cpp builds them, FrontEnd.cpp draws them; the pointer
// hits the same boxes). Row: a menu row; Chip: a small toggle in a strip; Lobby: a browser row (lobby: its index in
// the session's results); Field: a text field; Back: the page's way out.
struct TitleItem {
 enum Kind:std::uint8_t{Row,Chip,Lobby,Field,Back} kind=Row;
 int x=0,y=0,w=0,h=0;
 std::string label,value;
 bool enabled=true;
 int lobby=-1,action=0;
};


class Game {
    friend struct GuardCtx;
    friend struct CivilianMindView;
    friend struct LabAccess; // the feature lab (src/lab) stages scenes and snapshots them
public:
    explicit Game(WorldId id=WorldId::Campaign);
    explicit Game(std::shared_ptr<const CustomCampaign> campaign);
    WorldId worldId()const{return m_worldId;}
    static bool testWorldIsolation();
    static bool testServiceMaps();
    static bool testCampaignExtension();
    bool hazardActive(const Hazard& hazard)const;
    float compactorHeight(const Compactor& machine)const;
    void showTitleScreen();
    bool titleScreen()const{return m_titleScreen;}
    // A camera placed for an inspection or test shot, not a player in play: no body under it.
    bool inspectionCamera()const{return m_inspectionCamera;}
    // 0..1 beacon level from any running sequence whose repeating sound is the
    // Alarm klaxon: peaks on each blast and decays until the next.
    float alarmPulse()const;
    int titleSelection()const{return m_titleSelection;}
    bool customMapsOpen()const{return m_customMapsOpen;}
    int titleRows()const;
    std::string customMenuLabel(int row)const;
    int customCampaignCount()const{return int(m_customCampaigns.size());}
    const std::vector<std::string>& customCampaignErrors()const{return m_customCampaignErrors;}
    void loadCustomCampaignDirectory(std::wstring directory);
    bool menuFromTitle()const{return m_menuFromTitle;}

    void update(const InputState& input, float dt);
    // Multiplayer (src/net, docs/Systems/multiplayer.md). main owns the Session (EOS, or none); the Match is
    // the game over it and fills remotePlayers and matchView, which the menus, HUD and renderer read.
    void setNetSession(net::Session* session){m_netSession=session;if(session&&!m_settings.playerName.empty())session->setDisplayName(m_settings.playerName);}
    net::Session* netSession()const{return m_netSession;}
    net::Match* match()const{return m_match.get();}
    const std::vector<net::RemotePlayer>& remotePlayers()const{return m_remotePlayers;}
    const net::MatchView& matchView()const{return m_matchView;}
    bool inMatch()const{return m_matchView.active;}
    // Chat in a match (SettingsMenu.cpp keeps the typing, MatchChat.cpp the lines): T opens the line, Enter sends,
    // Esc drops it; chatTyped is the line so far. sendChat hands a line to the Match (false: refused).
    bool chatOpen()const{return m_chatOpen;}
    const std::string& chatTyped()const{return m_chatTyped;}
    bool sendChat(const std::string& text);
    // The newest chat line for the VR caption strip (no typing in a headset), "" when none is fresh.
    std::string vrChatCaption()const;
    // The mic meter the COMMUNICATION page shows: TEST MY MIC's own listening, else the session's level in a voice
    // room; -1 when nothing is listening.
    float micMeter()const;
    bool micTesting()const{return m_micTest;}
    // Mutes or unmutes a player by display name (the scoreboard, the COMMUNICATION page); kept in the settings.
    void setPlayerMuted(const std::string& name,bool muted);
    int scoreboardPick()const{return m_scoreboardPick;}     // the scoreboard row the arrows picked (-1 none)
    // Spent cases (Casings.cpp): a fired or pumped-out shell, or a pistol's brass, thrown from the ejection port in
    // world space; it tumbles, bounces, clinks and lies a while. Every gun and the VR pump call this; the renderer
    // draws casings(). kind: 0 a shotgun shell, 1 9 mm brass (the handgun), 2 .380 brass (the MAC-11).
    void ejectCasing(int kind,Point3 position,Point3 velocity);
    const std::vector<Casing>& casings()const{return m_casings;}
    static constexpr int MaxCasings=48;          // the oldest goes when a new one would pass this
    static constexpr float CasingLife=14.f;      // seconds a case lies once it has come to rest
    // Where the gun in hand throws its case from, in the active chunk's world
    // space, and the way it goes (the right of the bore, up and a little back),
    // with the player's own motion added. Desktop: from the view; VR: the aim.
    Point3 ejectionPort(WeaponId gun)const;
    Point3 ejectionVelocity(WeaponId gun);
    // A VR player's avatar as VrRuntime solves it, sent to the others in a match (main sets it each frame).
    void setNetAvatar(const AvatarPose& pose){m_netAvatar=pose;}
    // Bodies of the players in a match, in the active chunk's frame: Match lends them to traceShot while it
    // resolves a shot (they come back as ShotKind::Player with index slot+1); never seen by anything else.
    struct NetBody {int slot=0;Vec2 pos{};float z=0,height=Player::StandingHeight;};
    // Every player the game's minds know (docs/Systems/multiplayer.md "One path"). Script triggers, @-conditions,
    // creature perception and targeting, guards and people read this list, never m_player alone. Single player is
    // the list with one entry: the local player (id 0, body &m_player, live). A co-op host adds each guest's body
    // as the Match last heard it (id: their slot; the host is slot 0). A body is in the active chunk's frame, as
    // m_player is; level is the chunk it stands in. The list is rebuilt by each call (identical within a frame).
    struct PlayerView {int id=0;bool local=true;int level=0;const Player* body=nullptr;
     bool alive()const{return body&&body->health>0;}};
    const std::vector<PlayerView>& players()const;
    const PlayerView* playerById(int id)const;          // nullptr: no such player now
    const PlayerView& nearestPlayer(Vec2 at,float z)const;   // the nearest living one (the local player if none lives)
    // What the minds read of a player beyond the body: its speed, how lit it is (0..1), whether it is shut in a
    // locker, whether its torch is on. The local player's are the game's own live values.
    Vec2 playerVelocity(const PlayerView& p)const;
    float playerLit(const PlayerView& p)const;
    bool playerHidden(const PlayerView& p)const;
    bool playerTorch(const PlayerView& p)const;
    // A blow on a player from a creature or a guard: the local player takes it as receiveDamage does; a remote
    // player's goes out through the Match (m_netHurts), the host takes it off that guest and sends it to them.
    bool hurtPlayer(const PlayerView& p,float amount,Vec2 source,bool blockable=true,bool shieldable=true);
    // A co-op guest's body as the host keeps it (the Match fills it each frame; empty everywhere else).
    // pushing: they lean into something (walking keys held while stopped): a prop is shoved their way.
    struct RemoteBody {int id=0,level=0;Player body;Vec2 velocity{};bool hidden=false,torch=false,pushing=false;float lit=1,litTimer=0;};
    // The light on a point (a body's chest): the lamps, the room's ambient and darkness, a torch.
    float lightAt(Vec2 at,float chest,bool torch)const;
    // Script effects a co-op host sends on (the Match drains it after the frame; cleared at each script pass):
    // target -1 everyone, else the player id an item or a shake is for.
    struct ScriptFx {int target=-1;ScriptAction action;};
    // A terminal used (a switch, a lever, a lift call): run where the minds run, for that player. On a co-op
    // guest it is queued for the host (m_netUses) instead. Returns 1 the lift set off, 2 its switch was thrown.
    int operateTerminal(int terminal,int player);
    // The multiplayer pages on the title (MultiplayerMenu.cpp, docs/Systems/multiplayer.md). They call the Session
    // directly and pump it while open; once the session is InLobby the Match takes over.
    enum class TitlePage:std::uint8_t{Main,Multiplayer,QuickMatch,Browse,Host,Lobby,JoinCode,Character,PlayerName};
    TitlePage titlePage()const{return m_titlePage;}
    void openTitlePage(TitlePage page);
    const std::string& playerNameDraft()const{return m_nameDraft;}
    std::vector<TitleItem> titleItems()const;          // the current multiplayer page's rows (Main: none)
    // The session in a word ("SIGNED IN", "OFFLINE", "SIGNING IN", ...) and why, in plain words; usable: it can
    // host, search and join now.
    std::string sessionWord()const;
    std::string sessionReason()const;
    bool sessionUsable()const;
    const std::string& joinCodeTyped()const{return m_joinTyped;}
    float joinCodeCopied()const{return m_joinCopied;}  // seconds the COPIED note still shows
    int browseMode()const{return m_browseMode;}         // 0 any, 1 deathmatch, 2 co-op
    bool browseHideFull()const{return m_browseHideFull;}
    int browseScroll()const{return m_browseScroll;}
    bool browseSearched()const{return m_browseSearched;}
    struct HostDraft{net::Mode mode=net::Mode::Deathmatch;int map=0,maxPlayers=8;bool privateGame=false;std::string name;};
    const HostDraft& hostDraft()const{return m_hostDraft;}
    // The loaded custom campaigns whose MULTIPLAYER record allows the mode, by index in customCampaigns order.
    std::vector<int> hostableMaps(net::Mode mode)const;
    std::string customCampaignName(int index)const;
    const MultiplayerSettings* campaignMultiplayer(int index)const{return index>=0&&index<int(m_customCampaigns.size())&&m_customCampaigns[size_t(index)]?&m_customCampaigns[size_t(index)]->multiplayer:nullptr;}
    std::string titleNotice()const{return m_titleNotice;}
    // Held Tab (or the controller's back/view button) in a match: the scoreboard.
    bool scoreboardHeld()const{return m_scoreboardHeld;}
    // LEAVE MATCH (the pause menu): leaves the session's lobby and returns to the title's MULTIPLAYER page.
    void leaveMatch();
    static bool testMultiplayerMenu(std::ostream& report);
    void moveVrRoom(Vec2 delta){tryMove(delta);}
    void setVrAim(Point3 position,Point3 direction,bool valid,bool support=false){m_vrAimPosition=position;m_vrAimDirection=direction;m_vrAimValid=valid;m_vrSupportGrip=support&&valid;}
    // up: the hand's own up (the controller's, mapped to game space as its pointing is); left zero, the up nearest the
    // world's across the pointing (a hand held level, no roll).
    void setVrHand(int side,Point3 position,Point3 direction,bool valid,Point3 velocity={},Point3 up={}){m_vrHandVelocity[side]=velocity;m_vrInputActive=true;m_vrHandPosition[side]=position;m_vrHandDirection[side]=direction;m_vrHandUp[side]=up;m_vrHandValid[side]=valid;}
    bool vrSupportGrip()const{return m_vrSupportGrip;}
    // --- VR guns by hand (VrGuns.cpp; docs/Systems/vr-playtest.md) ---------------------------------------------
    // Each frame VrRuntime hands the game the drawn gun's frame (game space, chunk-relative: the grip in the fist and
    // the gun's right, up and back, back pointing from the muzzle to the grip), each hand's fist, velocity and grip
    // (by role: 0 the support hand, 1 the gun hand) and whether the support hand's wrist is at the belt pouch. From
    // that the game runs the handgun's magazine and slide and the shotgun's shells and pump; the recoil the gun shows
    // and the haptic pulses VrRuntime plays come back out. Desktop play never touches any of it.
    struct VrGunInput {bool valid=false;Point3 grip{},right{},up{},back{};std::array<Point3,2> fist{},velocity{};std::array<bool,2> grips{};bool pouch=false;};
    // A gun model's working points in the gun frame (x right, y up, z back: metres from the grip in the fist), as
    // the VR hands draw it. The renderer measures them from the meshes (setVrGunGeometry); the defaults are those
    // measurements, so the game and its tests agree with the drawing without a renderer.
    //  muzzle: where the bore leaves the gun; rearSight/frontSight: the tops of the sights (the sight line);
    //  port: the ejection port (where a case is thrown from); magwell: the seated magazine's base, magAxis the way
    //  it slides up into the grip, magLength how far; slideRear: where a hand takes the slide (the MAC-11: its charging knob) to rack it;
    //  loadingPort: under the receiver, where shells go into the tube.
    struct VrGunGeometry {Point3 muzzle{},rearSight{},frontSight{},port{},magwell{},magAxis{0,1,0},slideRear{},loadingPort{};float magLength=.11f;};
    static void setVrGunGeometry(WeaponId id,const VrGunGeometry& geometry);
    static VrGunGeometry vrGunGeometry(WeaponId id);
    // The shot's line from a gun frame (game space): from the muzzle toward the point 15 m out along the sight line
    // (the gun's zero), so the bore, the sights and the trace agree.
    static constexpr float VrZero=15.f;
    static void vrShotLine(const VrGunGeometry& g,Point3 grip,Point3 right,Point3 up,Point3 back,Point3& origin,Point3& direction);
    void setVrGunInput(const VrGunInput& in){m_vrGunIn=in;}
    // Recoil on the gun in the hand (never the view): metres pushed back along the bore, radians of muzzle rise.
    struct VrRecoil {float back=0,rise=0;};
    VrRecoil vrRecoil()const{return {m_vrGun.kickBack,m_vrGun.kickRise};}
    // Haptic pulses asked of the controllers (by role); VrRuntime takes and plays them each frame. cause names the
    // event (shot, magazine out, magazine seated, slide, pump back, pump forward, shell, draw, holster, pouch).
    struct VrHaptic {int hand=1;float seconds=.02f,frequency=160,amplitude=.5f;const char* cause="";};
    void vrHaptic(int hand,float seconds,float frequency,float amplitude,const char* cause){m_vrHaptics.push_back({hand,seconds,frequency,amplitude,cause});}
    std::vector<VrHaptic> takeVrHaptics(){std::vector<VrHaptic> out;out.swap(m_vrHaptics);return out;}
    const std::vector<VrHaptic>& vrHaptics()const{return m_vrHaptics;}
    // What the VR drawing shows: held 0 nothing in the support hand, 1 a magazine, 2 a shell; seat how far it is
    // into its port (0 at the opening to 1 home; -1 not at it) and depth the magazine's base below its seated place
    // (m); the pistol's magazine out and slide travel back (m); the magazine last dropped, falling or on the floor.
    struct VrGunView {int held=0;float seat=-1,depth=0;bool magOut=false;float slide=0;bool dropped=false;Point3 dropAt{},dropRight{},dropUp{},dropBack{};WeaponId dropGun=WeaponId::Handgun;};
    VrGunView vrGunView()const;
    // The VR reload button (B on the gun hand): drops the handgun's or the MAC-11's magazine, or (the handgun only)
    // with a loaded one in on a locked-back slide, releases the slide. The shotgun loads by hand only: the button
    // does nothing to it. The MAC-11 has no bolt release: its locked-back knob is pulled back by hand.
    void vrReloadButton();
    // The trigger may fire: the handgun or MAC-11 has a round chambered, the shotgun has been pumped.
    // vrMagazineGun: a gun the VR hands reload by magazine (the handgun, the MAC-11).
    static constexpr bool vrMagazineGun(WeaponId id){return id==WeaponId::Handgun||id==WeaponId::MachinePistol;}
    bool vrGunReady()const;
    // The support hand holds a magazine or shell, or has just put one in and has not opened since: no fore-end snap,
    // no two-hand hold and no grab for it until it opens.
    bool vrSupportBusy()const{return m_vrGun.held!=0||m_vrGun.spentGrip;}
    // A support-hand fist here would take the handgun's slide or the MAC-11's charging knob (it needs racking:
    // nothing chambered).
    bool vrRackZone(Point3 fist)const;
    bool vrRacking()const{return m_vrGun.racking;}
    static bool testVrGuns(std::ostream& report);
    // A lit room with no creatures and the gun raised (VrRuntime's scripted checks, the lab's VR captures).
    static Game vrGunScene(WeaponId gun,AmmoPool rounds,bool leftHanded=false);
    // Developer mode (--dev, console DEV): no damage, no creatures, every door open.
    // Console noclip: fly through walls (Movement.cpp). Never saved.
    bool noclip()const{return m_noclip;}
    void setNoclip(bool on);
    static bool testNoclip(std::ostream& report);
    static bool testSummon(std::ostream& report);
    void setDeveloperMode(bool on){m_god=m_peaceful=m_unlockAll=on;if(on){m_player.health=std::max(m_player.health,100.f);m_enemies.clear();}}
    bool developerMode()const{return m_unlockAll;}
    bool reloading()const{return m_reloadTimer>0;}
    // Aiming down the sights (right mouse with the shotgun up): 0 at the hip, 1 aimed.
    float aimBlend()const{return m_aimBlend;}
    // VR pump: fired with the support hand on the fore-end, the shotgun waits for
    // a stroke (back 7 cm, forward again). How far back the pump is, for drawing.
    float vrPumpOffset()const{return m_vrSupportGrip&&m_equippedWeapon==WeaponId::Shotgun?std::clamp(m_vrPumpRest-m_vrPumpAlong,0.f,.09f):0.f;}
    void restart();
    int level()const{return m_level;}
    // Campaign content count; runtime custom packs use their authored map count.
    static constexpr int ChunkCount=CampaignChunkCount;
    static constexpr int MaxChunks=WorldChunkCapacity;
    int chunkCount()const{return m_worldId==WorldId::Custom&&m_customCampaign?int(m_customCampaign->maps.size()):worldChunkCount(m_worldId);}
    Vec2 chunkOffset(int level)const{return m_worldId==WorldId::Custom&&m_customCampaign?m_customCampaign->maps.at(size_t(level))->definition.origin:chunkDefinition(m_worldId,level).origin;}
    Game chunkView(int level)const;
    // The World ensureChunk would make now, as a copy: the renderer builds geometry ahead from it.
    World previewChunkWorld(int level)const;
    const World& chunkWorld(int level)const{return level==m_level?m_world:m_chunks.at(size_t(level)).world;}
    const std::vector<Enemy>& chunkEnemies(int level)const{return level==m_level?m_enemies:m_chunks.at(size_t(level)).enemies;}
    const std::vector<Pickup>& chunkPickups(int level)const{return level==m_level?m_pickups:m_chunks.at(size_t(level)).pickups;}
    const std::vector<Clutter>& chunkClutter(int level)const{return level==m_level?m_clutter:m_chunks.at(size_t(level)).clutter;}
    // Soft padding (SoftBodies.cpp): the awake pad of a fixture or a prop (one of the two, the other -1),
    // and how far an awake pad covering p with its top within 3 cm of `surface` is pressed down there.
    const SoftPad* softPad(int level,int fixture,int clutter)const;
    float softSinkAt(int level,Vec2 p,float surface)const;
    const std::vector<SoftPad>& softPads()const{return m_softPads;}
    // How far below its top a soft prop gives (0 if it is hard): a body standing that deep in it is on it.
    static float softGive(const Clutter& c);
    const World& worldAt(Vec2& local)const;
    bool hearingRayClear(Vec2 from,float fromZ,Vec2 to,float toZ)const;
    bool soundRayClear(Vec2 from,float fromZ,Vec2 to,float toZ,float nearTrim=0,float farTrim=0)const;
    bool chunkResident(int level)const{return level>=0&&level<chunkCount()&&(level==m_level||m_chunks[size_t(level)].resident);}

    const World& world() const { return m_world; }
    const Player& player() const { return m_player; }
    void restoreEditorCamera(const Player& camera);
    Vec2 playerVelocity()const{return m_velocity;}
    float bodyGait()const{return m_bodyGait;}
    bool bodySprinting()const{return m_bodySprint;}
    const Ragdoll& hazmat()const{return m_hazmat;}
    const std::vector<Ragdoll>& hungSuits(int level)const{static const std::vector<Ragdoll> none;return level>=0&&size_t(level)<m_hungSuits.size()?m_hungSuits[size_t(level)]:none;}
    static Game hazmatInspection(int view=0);
    static bool testHazmat();
    void advanceDebugNavigation(ai::NodeGraph& graph) const;
    void reportMapHealthLoad() const;
    static int runMapHealth(const wchar_t* map,const std::wstring& campaign);
    const std::vector<Enemy>& enemies() const { return m_enemies; }
    const std::vector<FriendlyActor>& friendlyActors()const{return m_friendlyActors;}
    const std::vector<FriendlyActor>& npcActors()const{return m_friendlyActors;}
    // A man a creature's bite took the head off, and seconds since (saved: a
    // reload shows him headless; the age is large after a load).
    bool npcHeadless(size_t actor)const;
    // A man reeling from a prop's shove (RigidClutter.cpp): how far he leans with it from his feet (the
    // shove's way, the slope of the lean); none when he is not.
    Vec2 npcStumbleLean(size_t actor)const;
    float npcHeadlessAge(size_t actor)const;
    // How a man died, for the body the renderer makes of him: seconds since on the death clip's own
    // clock and whether that clock has run out, the clip phase he died at (a man already down dies
    // lying), and the way the killing hit threw him.
    struct NpcDeath {bool dead=false,done=false;float age=0,from=0;Point3 push{};};
    NpcDeath npcDeath(size_t actor)const;
    static bool testChimeraHunt();
    static bool testDoorAxis();
    static bool testSniper();      // --each-test=sniper: the bolt-action rifle, a head hit, the bolt, the reload and the scope
    static bool testAiDirector();  // --each-test=ai-director: an AI over a zone, its beat, the LIGHTS, NOISE and LATER blocks
    static bool testWalkIn();      // --each-test=walk-in: into an open container, up stairs, along a balcony rail
    static bool testMapFormat();   // MapFormatTest.cpp: map format 2
    static bool testDoors();
    static bool testMachine();
    // --machine-frames: 0 the electrical death on a lit test floor, 1 the lure across Experimental Power's split feed (chunk 92).
    static Game machineScene(int which);
    static StateId machineAsleepState();
    // Chapter 2 as the player arrives in `level`, the god camera at pos/z/yaw, for staging R-117
    // (--chimera-kill-frames and the chimera-hunt content checks).
    static Game chimeraStage(int level,Vec2 pos,float z,float yaw,float pitch=0);
    void chimeraFilm(bool close);std::string chimeraFilmLine()const;
    NpcWeaponPose npcWeaponPose(size_t actor)const;
    // Set by the CCTV patch in Perimeter Ops; from then on the minimap shows
    // the hostile guards in the current chunk (empty before it).
    static constexpr StateId CctvRadar=stateId("cctv_radar");
    std::vector<RadarContact> radarContacts()const;
    static bool testRadar();
    std::vector<TacticalGrenadePose> tacticalGrenades(bool dangerOnly=false)const;
    std::vector<EngineerDronePose> engineerDrones()const;
    AiTarget engineerDroneTarget(int slot,int observerLevel)const;
    GuardState guardState(size_t actor)const{return GuardState(state(friendlyKey(actor,"guard_mode")));}
    bool autonomousActor(const ActorTrack& track)const{return track.ai.mode!=ActorAiMode::Scripted&&(!track.ai.enableState||state(track.ai.enableState)||(track.deadState&&state(track.deadState)));}
    bool scriptedHandoffActive(size_t actor)const;
    bool scriptedHandoffActive(int level,size_t homeActor)const;
    bool friendlyChunkNeeded(int level)const;
    void friendlyChunksNeeded(std::vector<char>& needed)const;
    const std::vector<Pickup>& pickups() const { return m_pickups; }
    float pickupHeight(const Pickup& pickup)const{return pickup.z>-999?pickup.z:m_world.floorHeight(pickup.pos.x,pickup.pos.y);}
    const std::vector<Clutter>& clutter()const{return m_clutter;}
    const std::vector<BulletImpact>& bulletImpacts()const{return m_bulletImpacts;}
    void pinToDoor(BulletImpact& mark)const;   // a mark landing on a door's leaf rides with it (Hitscan.cpp)
    const std::vector<ShotRay>& shotRays()const{return m_shotRays;}
    // The skinbot walking on the map being played (null: none here), and sending it to a spot (a laser pointer).
    const skinbot::Walker* skinbotWalker()const{return m_skinbotLevel==m_level?&m_skinbot:nullptr;}
    void pointSkinbot(Vec2 at){m_skinbotPointed=true;m_skinbot.pointAt(at.x,at.y);m_skinbotDetour.clear();m_skinbotRetry=0;}
    // Dead bodies' poses (SaveSections.h CorpsePose) by (creature, level, slot): the renderer keeps each body's current
    // pose here as it moves, the physics save section writes and reads them, and a body made again after a load takes
    // its pose back from here. (Kept by the game so a save holds them; written from the renderer's const view.)
    using CorpsePoses=std::map<std::tuple<bool,int,std::uint64_t>,CorpsePose>;
    CorpsePoses& corpsePoses()const{return m_corpsePoses;}
    const std::vector<HitBurst>& hitBursts()const{return m_hitBursts;}
    const std::vector<WaterSplash>& waterSplashes()const{return m_waterSplashes;}
    const std::vector<BarrelExplosion>& barrelExplosions()const{return m_barrelExplosions;}
    bool holdingClutter()const{return m_heldClutter>=0;}
    int vrCarryHand()const{return m_vrCarryHand;}
    // Physgloves (Physgloves.cpp): point at a loose prop, flick, and it pops toward the hand on an arc to
    // be caught. target: the prop highlighted (this map's list, -1 none); flying: the prop pulled and not
    // yet caught or missed; lurch: it was too heavy to fly and only lurched.
    struct PhysgloveState {int target=-1,hand=1;bool locked=false;int flying=-1,flyingHand=1;float flight=0,window=0;bool lurch=false;
     Point3 aimedAt{};std::array<bool,2> grip{};bool key=false,popping=false;float popRim=0;int pulls=0,catches=0,misses=0;float lockAge=0;};
    // VR: a hand that can lock the Physgloves on: not the hand holding a drawn gun, not the hand on its fore-end.
    bool physgloveHandFree(int hand)const{return !(hand==1&&m_weaponEquipped&&!unarmed())&&!(hand==0&&m_vrSupportGrip);}
    // Two-hand carry (TwoHand.cpp): one hand of two letting go is only acted on after TwoHandGrace, so both hands
    // opening a few frames apart throw the prop with both instead of handing it to the last hand for a frame.
    // 0 keep both, 1 the second hand lets go, 2 it passes to the second hand, 3 both let go: thrown.
    static constexpr float TwoHandGrace=.15f;
    static int twoHandRelease(bool carryGrip,bool secondGrip,float& grace,float dt);
    static bool testVrHardening(std::ostream& report);
    // --vr-test: held props turn with the hand (one and two hands), grip down takes and grip up drops, the trigger punts.
    static bool testVrHeldProps(std::ostream& report,Point3(*map)(Point3)=nullptr);
    // A VR hand's turn in game space: its pointing on +x, its up on +z (a proper rotation built in game space from the two
    // mapped vectors, so the tracking mirror cancels out of every turn taken between two of them).
    std::array<float,4> vrHandTurn(int hand)const;
    // The punt (VR trigger while holding a prop, desktop fire): thrown along dir at PuntSpeed scaled by its mass.
    static constexpr float PuntSpeed=9.f;
    static float puntSpeed(const Clutter& c);
    void puntHeld(Point3 dir,float lift);
    const PhysgloveState& physgloves()const{return m_physglove;}
    // Where a prop pulled by `hand` is caught (its middle): before the eye at carry reach, or in the VR hand.
    Point3 physgloveCatchPoint(int hand,const Clutter& c)const;
    // The best loose prop for `hand` to pull (-1 the desktop look), or -1.
    int physgloveAim(int hand)const;
    bool physglovePull(int index,int hand);void physgloveLaunch(int index,int hand);
    // VR two-hand carry (Clutter.cpp): the second hand on the held prop (-1 none), and the hand bodies
    // (VrHands.cpp): where each drawn hand is, stopped at the surface of what it pushes into.
    int vrSecondHand()const{return m_vrSecondHand;}
    Point3 vrHandProxy(int hand)const{return m_vrHandProxy[size_t(hand&1)];}
    bool vrHandBlocked(int hand)const{return m_vrHandBlocked[size_t(hand&1)];}
    // Liquids (Liquids.cpp): drops in the air and the puddles they made, this map's, in its own coordinates.
    // level: the map (chunk) it was spilt in; its places are that map's own, and it is drawn and kept only there.
    struct LiquidDrop {Point3 p{},v{};liquid::Kind kind=liquid::Kind::Water;float volume=0;int stream=0;int level=0;};   // stream: the mouth or hole it ran from (0: a spill), drops of one stream in the order they left
    struct Puddle {Point3 at{};liquid::Kind kind=liquid::Kind::Water;float volume=0,radius=0;int level=0;};
    const std::vector<LiquidDrop>& liquidDrops()const{return m_liquidDrops;}
    const std::vector<Puddle>& puddles()const{return m_puddles;}
    // Fill a prop with liquid (a vessel only); a round holing it at a world point.
    bool fillLiquid(Clutter& c,liquid::Kind kind,float level);void holeLiquid(Clutter& c,Point3 at);
    static void registerLiquidSave();   // the "liquids" save section (at start-up)
    Point3 vrHandTracked(int hand)const{return m_vrHandPosition[size_t(hand&1)];}
    Point3 vrHandOffset(int hand)const{const size_t h=size_t(hand&1);return m_vrHandProxySet[h]?m_vrHandProxy[h]-m_vrHandPosition[h]:Point3{};}   // drawn hand minus tracked hand
    static bool testClutter();
    static PropTraits propTraits(int model);
    static bool auditMapScene(Game& game,int level,std::vector<std::pair<std::string,unsigned>>* findings=nullptr);
    static bool testPhysicsStages();static bool testProps();static bool testPropSaves();static bool testGlass();static Game physicsInspection(int view,float seconds);
    // A test bay of its own (a test set's sockets, test leads, a charged cell): the panel close (0),
    // the cell's terminals close (1), the bench (2), the cell lifted, turned and tipped (3), a plug
    // pulling off (4), both off and swinging (5), the leads hanging still (6), the cell set down with
    // its posts bare (7), view 6 saved and loaded (8), the black lead loose from the start (9).
    // Seconds into the tear.
    static Game leadInspection(int view,float* tearAge=nullptr);
    // Electricity (Electricity.cpp): a shower of sparks thrown from `at` along `along` (chunk-local)
    // and an arc struck between two points for `seconds`, drawn and lit like every other source.
    void sparkBurst(int level,Point3 at,Point3 along,float heat=1,int count=24,bool smoke=false);
    void strikeArc(int level,Point3 from,Point3 to,float seconds,float strength=1,bool resonance=false);
    // Everything electric in a chunk now, chunk-local: its showers, arcs and their light.
    void electricScene(int level,electric::Scene& out)const;
    // electricScene worked out once a frame, however many chunks and eyes draw it and light by it
    // (ElectricFx, weaponFlashes); anew when the clock, the player or an electric event moves.
    const electric::Scene& electricNow(int level)const;
    // A plain room with a live lead run from the wall to a charged cell on a desk, `seconds` after
    // (before, negative) the cell is knocked and the lead tears, seen from the room (view 0) or close to
    // the desk (1); a round strikes the wall (2); a live floor along the wall is powered, pulsed (3).
    // `tearAge` gets the time since the tear (the round, the power).
    static Game electricInspection(float seconds,int view=0,float* tearAge=nullptr);
    const std::vector<BreakEvent>& breakEvents()const{return m_breakEvents;}
    const GlassBreak* glassBreak(int level,int index)const{for(const auto&g:m_glassBreaks)if(g.level==level&&g.index==index)return &g;return nullptr;}
    static Game clutterInspection(int kind,float seconds);
    static bool testStreaming();
    static bool testCampaignMaps();
    static bool testChapterRoute();
    static bool testChapterSequences();
    void updateMechanisms(float dt);
    std::string_view actorCaption()const;
    // Captioned sequence cues emitted at a terminal's position (its computer's
    // messages) that have already played, in sequence order then time.
    std::vector<const SequenceCue*> terminalMessages(int terminal)const;
    // True while a sequence that speaks through this terminal still has lines to come.
    bool terminalConversationActive(int terminal)const;
    int sequenceClock(const World& world,StateId timer)const;
    static bool testMechanisms();

    float weaponKick() const { return m_weaponKick; }
    const WeaponMotion& weaponMotion()const{return m_weaponMotion;}
    float shotAge()const{return m_shotAge;}
    float holster()const{return m_holster;}
    float punchAge()const{return m_punchAge;}
    bool punchLeft()const{return m_punchLeft;}
    // A gun at the bottom of a switch (going down for another, or about to come
    // up) is still in hand: no fists, no FISTS on the HUD, no punch.
    bool unarmed()const{return m_holster>=.98f&&m_switchTarget==WeaponId::Fists&&!m_switchRaise;}
    void stowWeapon(){m_holster=1;}
    bool handsLowered()const{return m_handsLowered;}
    bool guarding()const{return m_guarding;}
    bool takedownActive()const{return m_takedownActor>=0;}
    std::array<Point3,2> takedownGrips()const;
    static bool testUnarmed();
    static bool testVrMelee();
    static bool testVrInteraction();
    // 1 fists, 2 shotgun shot (age), 3 guard; the handgun: 4 shot (age),
    // 5 tactical and 6 empty reload (age = 0..1 through it), 7 sprinting
    // (age = how far lowered), 8 raised from a switch (age = holster), 9 torch on.
    static Game weaponInspection(int mode,float age=0);
    float damageFlash() const { return m_damageFlash; }
    float elapsed() const { return m_elapsed; }
    int kills() const { return m_kills; }
    bool won() const { return m_won; }
    bool dead() const { return m_player.health <= 0.0f; }
    int enemiesRemaining() const;
    int enemiesRemaining(int level)const{return hostilesRemaining(level);}
    float hitFlash()const{return m_hitFlash;}
    // The controller (Controller.cpp): the motors' strengths now (x the heavy low motor, y the light high one, 0-1,
    // the RUMBLE setting applied); a prompt with its keys named as the pad's buttons while the pad is in use; aim
    // assist on the look stick (yaw and pitch rates, radians and pitch units a second, in and out).
    Vec2 padRumble()const;
    std::string padPrompt(const char* text)const;
    bool padPrompts()const{return m_padPrompts;}
    Vec2 aimAssist(Vec2 stickLook,float dt)const;
    static bool testController(std::ostream& report);
    float critFlash()const{return m_critFlash;}   // a head hit with the sniper rifle: the cross reads HEAD
    // The scope: how far the rifle is at the eye (0 hip, 1 looking through it). The view narrows and a ring closes round it.
    float scopeBlend()const{return m_weaponEquipped&&m_equippedWeapon==WeaponId::Sniper&&!unarmed()?m_scopeBlend:0.f;}   // the rifle shoulders over its iron sights first (aimBlend); held there, the view closes down the scope
    const Enemy* targetEnemy()const;
    static bool testCombat();
    static bool testWeaponMotion();
    static bool testAudioEvents();
    static bool testSettings();
    static bool testSaves();
    // Saves written while playing many campaign maps (deaths, debris, bodies, seams), each loaded back.
    static bool testSavesInPlay(std::ostream& report);
    // Why the last load on this thread was refused ("" when it loaded): also written to RawMetal-load.txt.
    static const std::string& lastLoadFailure();
    // The save container (SaveSections.h, docs/Systems/save-format.md): sections, the refit of
    // older saves, and the stores for entity overrides and map variables.
    // Parts: load paths, damage, stores, hooks and the misplaced folder (Core); formats 12-27 on a room; the
    // campaign formats in two halves; the real saves found on this machine and in lab/fixtures/saves.
    enum SaveTestPart {SaveTestCore=1,SaveTestCampaignEarly=2,SaveTestCampaignLate=4,SaveTestRealSaves=8,SaveTestRoomFormats=16,SaveTestAll=31};
    static bool testSaveFormat(std::ostream& report,int parts=SaveTestAll);
    // Adds (or, under a name already taken, replaces) a save section. Call it at start-up,
    // before the first save or load; the built-in sections come first, then these in order.
    static void registerSaveSection(SaveSection section);
    // Every section this build reads and writes, in order.
    static std::vector<std::string> saveSectionNames();
    // The save format a loaded game came from (0: not loaded from a save).
    int loadedSaveVersion()const{return m_loadedSaveVersion;}
    // What loading an older save from a file did about it (refitSaveFile).
    struct SaveRefit {int from=0;bool rewritten=false;std::filesystem::path backup;std::string note;std::vector<std::string> skipped;};
    // Runtime changes to an entity's properties, kept by the save ("entities" section). `chunk` is
    // the map (-1: the campaign); `entity` its name, or entityIdKey(kind,index) for one without a
    // name. A false return: a name empty, too long (128) or with control characters, a text past
    // 4096 bytes, a real number not finite, or the store full.
    bool setEntityProperty(int chunk,std::string_view entity,std::string_view property,SaveValue value);
    const SaveValue* entityProperty(int chunk,std::string_view entity,std::string_view property)const;
    const SaveStore::Properties* entityProperties(int chunk,std::string_view entity)const;
    bool clearEntityProperty(int chunk,std::string_view entity,std::string_view property);
    static std::string entityIdKey(std::string_view kind,int index){return "#"+std::string(kind)+":"+std::to_string(index);}
    // Named values scripts set, kept by the save ("variables" section): per map (chunk), or -1
    // for the whole campaign. Same limits as entity properties.
    bool setMapVariable(int chunk,std::string_view name,SaveValue value);
    const SaveValue* mapVariable(int chunk,std::string_view name)const;
    bool clearMapVariable(int chunk,std::string_view name);
    const SaveStore& saveStore()const{return m_saveStore;}
    // The save folder an older build wrote by mistake (%LOCALAPPDATA%RawMetalsaves, a lost
    // backslash): its saves and settings are copied into the right one, never over a newer file.
    static void adoptMisplacedSaves(const std::wstring& localAppData);
    static bool testNpcVoices();
    // Pause: PAUSE (PauseRow); Options: SETTINGS (SettingsTab pages); the save and load pages and their
    // confirmations. RESTART, QUIT TO TITLE and RESET TO DEFAULTS confirm in place, on a second press.
    enum class MenuPage {Pause,Save,Load,Overwrite,ConfirmLoad,Options};
    MenuPage menuPage()const{return m_menuPage;}
    int menuRows()const{return m_menuPage==MenuPage::Pause?PauseMenuLayout::rows(m_matchView.active):m_menuPage==MenuPage::Save?4:m_menuPage==MenuPage::Load?5:m_menuPage==MenuPage::Options?int(settingsRows().size()):2;}
    bool vrMenu()const{return m_vrInputActive;}
    // SETTINGS: the open page, whether the page column has the keys (else its rows), the muted-players list open on
    // COMMUNICATION, the armed row (a confirm waiting; -1 none), and the rows and items the pointer and renderer see.
    SettingsTab settingsTab()const{return m_settingsTab;}
    bool settingsOnTabs()const{return m_settingsOnTabs;}
    bool settingsMutedList()const{return m_settingsMuted;}
    int menuArmed()const{return m_menuArmed;}
    std::vector<int> settingsRows()const;               // the open page's row ids (SettingsMenu.cpp)
    std::vector<MenuItem> settingsItems()const;         // the page column, BACK, then the rows
    static const char* settingsTabName(SettingsTab tab);
    void openSettings(SettingsTab tab);                 // from PAUSE or the title
    bool takeSettingsSave(){const bool r=m_settingsSaveDue;m_settingsSaveDue=false;return r;}   // left SETTINGS: main saves
    static bool testSettingsMenu(std::ostream& report);
    // Recalibrate height was chosen: VrRuntime takes it (once) and sets the standing eye to the head's height now.
    bool takeVrRecalibrate(){const bool r=m_vrRecalibrate;m_vrRecalibrate=false;return r;}
    static bool testComfortMenu(std::ostream& report);
    const std::string& menuMessage()const{return m_menuMessage;}
    const std::string& slotLabel(int slot)const{return m_slotLabels[slot];}
    const std::string& checkpointLabel()const{return m_checkpointLabel;}
    void setSaveDirectory(std::wstring path){m_saveDirectory=std::move(path);refreshSaveSlots();}
    bool saveSlot(int slot);
    bool loadSlot(int slot);
    bool saveCheckpoint();
    bool loadCheckpoint();
    // With asynchronous loads (the shipped game turns them on) a load builds the
    // saved game on a worker while the window stays responsive and a loading
    // bar shows; update() commits it when it is ready. Tests load in place.
    void setAsyncLoads(bool enabled){m_asyncLoads=enabled;}
    bool loading()const{return m_loadJob!=nullptr;}
    float loadProgress()const;
    unsigned sessionRevision()const{return m_sessionRevision;}
    // What the renderer keys baked geometry and lighting by. The epoch changes
    // when everything must be rebuilt (a restart, a load); a map's static
    // revision also changes when that one map's content does (the campaign
    // builder's live edits), so an edit rebuilds its own map and no other.
    unsigned renderEpoch()const{return m_sessionRevision-m_liveEdits;}
    unsigned staticRevision(int level)const{return renderEpoch()*40503u+(level>=0&&size_t(level)<m_mapEdits.size()?m_mapEdits[size_t(level)]:0u);}
    // Which game this is, for the renderer's per-game caches: unique in the process, and a copy
    // or an assignment takes a new one. Never the game's address, which a later game can take
    // when this one is gone (the builder's preview builds them one after another).
    std::uint64_t renderSerial()const{return m_renderSerial.value;}
    static bool testPickups();
    static bool testMovement();
    static bool testProgression();
    static bool testAI();
    static bool testHearing();
    static bool testGuardAI();
    static bool testGuardCombat();
    static bool testGuardTactics();
    static bool testGuardPolicy();
    static bool testGuardDefense();
    static bool testGuardPursuit();
    static bool testGuardActions();
    static bool testTacticalGrenades();
    static bool testGuardRoles();
    static bool testEngineerDrones();
    static bool testGuardDanger();
    static bool testGuardSchedule();
    static bool testGuardPatient();
    static bool testGuardCover();
    static bool testGuardTable();
    static bool testNavGraph();
    static bool testCreatureSchedule();
    static bool testCreatureBehaviour();
    static bool testEngineRegression();
    static bool testEngineRegressionSaves(std::ostream& report);
    static bool testSnatcherAmbush(std::ostream& report);
    static bool testCivilianSchedule();
    static Game squadSupportInspection(int stage=0);
    static Game guardDangerInspection(int stage=0);
    static Game engineerSupportInspection(int stage=0);
    static Game guardPolicyInspection(int stage=0);
    static Game killSquadInspection(int stage=0);
    static Game cleanupLayerInspection(int role,int sample,int view=0,float phase=-1);
    // One civilian body (civilianActorBody order) alone on a floor: clip and
    // phase on a still actor, or armed with the shotgun raised (clip<0).
    static Game civilianInspection(int body,int clip,float phase,int view);
    static Game guardTacticsInspection(int stage=0);
    static Game guardCombatInspection(int stage=0);
    static Game guardInspection(int stage=0);
    static bool testFriendlyAI();
    static bool testStance();
    static bool testDialogue();
    static bool testStorm();
    static bool testZonePeople();
    // The stance room with its corporal mid-conversation (the --dialogue-shot frame).
    static Game dialogueInspection();
    // The stance room's squad hostile and on Alert, seen from `distance` metres (--card-shot).
    static Game cardInspection(float distance);
    static bool testAiQueryBatch();
    static bool testAiDecisionClock();
    void setAiQueryCaching(bool enabled){m_aiQueryCaching=enabled;}
    void setAiDecisionScheduling(bool enabled){m_aiDecisionScheduling=enabled;}
    void setNavAuthority(bool enabled){m_navAuthority=enabled;}
    static bool testExclusion();
    static bool testExclusionAuthoring();
    static bool testPrologue();
    static bool testCrates();
    // Reachability and placement audit of a campaign map (map-audit-<level>.txt).
    static bool auditMap(int level);
    static Game prologueInspection(int stage);
    static Game uiInspection(int view);
    static Game bodyInspection();
    static Game exclusionInspection(int stage=0);
    // A standing view anywhere in the Exclusion Zone pack: map, local x/y,
    // yaw in degrees and view pitch, on the ground (for --zone-shots).
    static Game zoneInspection(int level,Vec2 position,float yawDegrees,float pitch,float lift=0);
    // --zone-pack: the custom campaign file zoneInspection renders instead of the Exclusion Zone.
    static inline std::wstring s_zoneInspectionPack;
    static bool testStealth();
    bool npcIncapacitated(size_t actor)const;
    bool npcHeld(size_t actor)const;
    int heldNpc()const;
    std::array<Point3,2> heldNpcGrips()const;
    void updateTakedown(float dt);
    static bool testGantry();
    static bool testGameplayFixes();
    static bool testLift();
    static Game liftInspection(float seconds,int view=0);
    static bool testReactor();
    const char* interactionHint()const;
    bool doorLocked(const Door& door)const;
    void followDoors();   // doors with follow set open and shut with their state, one at a time (World.h Door::follow)
    int activeLog()const{return m_activeLog;}
    float logTime()const{return m_logTime;}
    const Pickup* nearbyPickup()const;
    const std::string& pickupNotice()const{return m_pickupNotice;}
    float pickupNoticeTime()const{return m_pickupNoticeTime;}
    // The major-area title card: its text and opacity (fade in, hold, fade out).
    static constexpr float AreaTitleTime=7.f;
    const std::string& areaTitle()const{return m_areaTitle;}
    float areaTitleAlpha()const{if(m_areaTitle.empty())return 0;float t=m_areaTitleTime;return std::clamp(std::min(t/1.5f,(AreaTitleTime-t)/2.f),0.f,1.f);}
    bool paused()const{return m_paused;}
    bool consoleOpen()const{return m_consoleOpen;}
    const std::string& consoleLine()const{return m_consoleLine;}
    const std::vector<std::string>& consoleLog()const{return m_consoleLog;}
    bool showFps()const{return m_showFps;}
    float renderScale()const{return m_renderScale;}
    // How far outdoor views reach, metres (docs/Systems/render-distance.md): the map's own distance (a
    // VIEW record, else 70 m, 110 m under the open-zone sky) times the player's VIEW DISTANCE choice.
    float viewDistance()const;
    // Beyond fullDetailRange() a chunk is drawn as its far LOD (terrain, impostor trees, boxes), 48 m.
    static constexpr float fullDetailRange(){return 48.f;}
    void setViewDistance(float metres){m_viewOverride=metres;}   // a fixed distance (the lab, a capture); 0 the map's own
    float viewFadeStart()const{const float v=viewDistance();return v>70.f?v*.62f:0.f;}
    // The fade the frame uses (render-distance.md "Edges"): surfaces melt into the horizon (or the map's haze
    // underground) between start and end metres; end 0 is no fade. Where an open map edge with nothing past it
    // is nearer than the view distance in the view's direction, the fade ends before that edge, eased over a
    // second, so the void never shows. voidTone: pixels with no surface take the haze colour (no sky to show).
    struct ViewFade{float start=0,end=0;bool voidTone=false;};
    ViewFade viewFade(Vec2 eye,float yaw,float halfFov)const;
    // How far from eye (map space) real ground runs along dir before an open edge with nothing beyond it;
    // a closed edge (a wall) or the far side of a HORIZON strip counts as reaching on. Capped at limit.
    float edgeReach(Vec2 eye,Vec2 dir,float limit)const;
    static bool testConsole();
    static bool testSystems();
    int state(StateId id)const;
    int state(std::string_view name)const{return state(stateId(name));}
    void setState(StateId id,int value=1);
    ObjectiveStatus objective(StateId id)const;
    std::string_view activeObjectiveText()const;
    void setObjective(StateId id,ObjectiveStatus status);
    bool hasQuestItem(StateId id,int count=1)const;
    int questItemCount(StateId id)const;
    void giveQuestItem(StateId id,int count=1);
    bool takeQuestItem(StateId id,int count=1);
    const std::vector<QuestItemStack>& questItems()const{return m_questItems;}
    static const char* questItemName(StateId id);
    static constexpr StateId ReactorAuthDisk=stateId("reactor_auth_disk");
    static constexpr StateId Flashlight=stateId("flashlight");
    bool hasFlashlight()const{return hasQuestItem(Flashlight);}
    // The handgun is owned as a quest item: saved, carried and cleared with the others.
    static constexpr StateId HandgunItem=stateId("handgun");
    bool hasHandgun()const{return hasQuestItem(HandgunItem);}
    // The MAC-11 too (a floor pickup, a script's GIVE, `give mac11`).
    static constexpr StateId MachinePistolItem=stateId("machine_pistol");
    bool hasMachinePistol()const{return hasQuestItem(MachinePistolItem);}
    // And the sniper rifle (a floor pickup, a script's GIVE, `give sniper`).
    static constexpr StateId SniperItem=stateId("sniper_rifle");
    bool hasSniper()const{return hasQuestItem(SniperItem);}
    // The shotgun is carried from the start everywhere but a deathmatch, where nobody has a gun until one is picked up.
    bool m_shotgunOwned=true;
    bool ownsWeapon(WeaponId id)const{return id==WeaponId::Fists||(id==WeaponId::Shotgun&&m_shotgunOwned)||(id==WeaponId::Handgun&&hasHandgun())||(id==WeaponId::MachinePistol&&hasMachinePistol())||(id==WeaponId::Sniper&&hasSniper());}
    // The MAC-11 with `rounds` more .380; a first one comes with them in its magazine.
    void grantMachinePistol(int rounds);
    // The sniper rifle with `rounds` more .308; a first one comes with them in its magazine.
    void grantSniper(int rounds);
    // The hip holster's contents (AvatarPose::hip, WeaponId+1, 0 empty) with `inHand`
    // drawn: one sidearm rides there, the one not in hand. With both owned and
    // neither drawn it is the one drawn last (the MAC-11 when it was the last
    // carried there); the long gun is on the back.
    int hipHolster(WeaponId inHand)const;
    // The full-auto gun's state: the spread's bloom (radians over its base) and
    // how far the view has climbed in this burst (pitch units still to return).
    float spreadBloom()const{return m_spread;}
    float recoilDebt()const{return m_recoilDebt;}
    // The cone a round from the gun in hand leaves within now (radians, half-angle).
    float shotSpread()const;
    // The handgun with `rounds` more 9 mm; a first one comes with them in its magazine (console, map shots).
    void grantHandgun(int rounds);
    // Inspection only (map shots): every sidearm carrier on this level killed, so his pistol falls.
    void downSidearmCarriers(){for(size_t i=0;i<m_friendlyActors.size();++i)if(m_friendlyActors[i].level==m_level&&m_friendlyActors[i].track().ai.dropsSidearm)damageFriendly(i,1e4f,m_player.pos);updateSidearms();}
    // Unlooted pistols of downed carriers (dropsSidearm), where they fell.
    std::vector<DroppedSidearm> droppedSidearms()const;
    static bool testHandgun();
    static bool testHandgunSaves(std::ostream& report);
    // 20: how far each creature has made the player out (Enemy::recognition).
    // 21: each chunk's torn leads.
    // 26: no new fields. Chairs on a floor are loose props from v1.4.8, carried in each chunk's
    // clutter; an older build would draw them twice (its fixed chair and the saved loose one), so
    // it refuses a 26. Older saves load here: the props they lack come back where they were put.
    // docs/Systems/save-format.md keeps the whole history.
    // 28: the section container (SaveSections.h). Sections are added without a new format; a 1-27
    // save is refitted into it on load and rewritten, the original kept beside it as .v<N>.bak.
    static constexpr int SaveVersion=28;
    // The last positional (text) format; archiveSave writes and reads 1..this.
    static constexpr int LegacySaveVersion=27;
    // That format's save of this game (tests: an old save still loads).
    std::string legacySaveText(int version);
    // The torch lights with the item, its switch (flashlight_on) and power:
    // inside a jamming DarkZone it sputters for FlashlightJamTime, then dies.
    float playerExposure()const{return m_playerExposure;}
    // How far the last footstep carried (0 silent .. 1 at 12 m, a sprint on
    // steel): it holds through a stride, then fades out over .3 s. The HUD's noise pips.
    float playerNoise()const;
    // How long a guard who has the player must be on the radio before his squad knows.
    static constexpr int RadioCallMs=1500;
    void updateExposure(float dt);
    // E against a bare wall: two knocks a guard on the other side will come to.
    void knockOnWall();float m_lastKnock=-10;
    // A knocked-out guard the player is dragging by the collar (not saved: the
    // body's own pose is, every step). -1 when the hands are free.
    int m_draggedNpc=-1;
    int nearbyBody()const;
    void dropBody();
    bool draggingBody()const{return m_draggedNpc>=0;}
    // Hiding in a steel locker (Stealth.cpp). The fixture the player is shut
    // in (-1 when out; state "player_locker", so it saves), the one in front of
    // them they could climb into, and getting in and out. A guard who saw them
    // climb in keeps guard_locker and pulls them out at its doors. However the
    // player comes out, forgetLocker drops the record and every guard's memory
    // of it. lockerExit squeezes further afield when the plain spots are taken.
    int hiddenLocker()const;
    int lockerAhead()const;
    bool enterLocker();
    bool leaveLocker();
    void forgetLocker();
    bool lockerExit(const Fixture& locker,Vec2& at,float& feet,bool squeeze=false)const;
    bool updateLocker(const InputState& input);
    bool pullFromLocker(int level,size_t actor,ActorPose& pose);
    static bool testLockers();
    // Locker doors (LockerDoors.cpp): each locker's two leaves swing on their
    // hinges. Gameplay commits stay instant; the doors and the player's view
    // play the motion over the following fraction of a second.
    enum class LockerMotion {Enter,Exit,Caught,Stow,Knock};
    void animateLockerDoor(int level,int fixture,LockerMotion motion);
    // How far a leaf stands open: 0 shut, 1 at its full swing (leaf 0 left, 1 right, seen from outside).
    float lockerDoorOpen(int level,int fixture,int leaf)const;
    // The camera: the player's pose, or while a locker's choreography plays the
    // path from where they stood to where the instant commit put them. Shut in,
    // the eye sits behind the leaves while the record keeps the body at the doors.
    Player view()const;
    // True while a path holds the controls (enter, step out, the first half second of being pulled out).
    bool lockerViewLocked()const;
    // How much of the view the shut leaves cover from inside: 0 open, 1 shut in.
    float lockerShutView()const;
    // The locker whose leaves the camera looks out through, or -1.
    int lockerViewFixture()const;
    // LMB shut in a locker: a rattle of the leaves and a knock guards come to.
    void knockInLocker();
    // The hands while a locker path or a stow plays (world grips, right then
    // left, as takedownGrips): pulling the leaves open and shut on the way in,
    // palms shoving them on the way out, shoving a body in. False, no hands.
    bool lockerHands(std::array<Point3,2>* grips=nullptr)const;
    // Stepping out into a guard in the leaves' swing who is not on to the
    // player: the doors are flung into him and he goes down (the actor, or -1).
    int lockerBurstTarget()const;
    // --map-shots "burst": the nearest guard stands at the doors checking them, the player bursts out.
    bool stageLockerBurst();
    // The gun put away for the way in, raised again once the way out ends.
    bool m_lockerRearm=false;void lockerRearm();
    // Transient, never saved: a save mid-motion loads at the committed end.
    // lead: how long the leaves wait for the hands to reach them (Enter).
    struct LockerDoor {int level=-1,fixture=-1;LockerMotion motion=LockerMotion::Knock;float age=0,length=0,lead=0;std::array<float,2> from{},angle{},limit{};bool shut=true;};
    std::vector<LockerDoor> m_lockerDoors;
    struct LockerView {bool active=false;LockerMotion motion=LockerMotion::Enter;float age=0,length=0;Player from;int fixture=-1;float facePitch=0;bool burst=false;};
    LockerView m_lockerView;
    void startLockerView(LockerMotion motion,const Player& from,int fixture,float facePitch=0);
    void updateLockerDoors(float dt);
    void clearLockerMotion(bool doors);
    // --map-shots "caught": the nearest guard here steps to the doors and hauls the player out.
    bool stageLockerCatch();
    // Guards and lockers (ai/guard/Lockers.cpp). A guard is a witness only if
    // he watched the player go in for a quarter second of the entry's first
    // 0.65 s and already half had them; anyone else who glimpsed it checks the
    // floor in front. The watch is transient (m_lockerWatch: per guard, when the
    // current unbroken look began, and his verdict), and so is when each guard
    // last had the player confirmed (for the hot-dive rule); what they decide
    // saves: guard_locker (fixture+1), guard_check (a suspect check, not a
    // sighting), guard_check_ms/guard_check_said/guard_doors_ms (the bark lead
    // and the pause at the doors), guard_haul (the reach for the handle, ms)
    // and guard_checked (one check per search).
    static constexpr int LockerHaulMs=350,LockerCheckLeadMs=1500,LockerCheckPauseMs=1000,LockerStowMs=1400;
    struct LockerWatch {int locker=-1;float started=-10;Point3 body{};float height=0,eye=0,exposure=0;bool crouched=false;std::vector<float> since;std::vector<std::uint8_t> verdict;std::vector<float> hadPlayer;float dashAt=-10;bool dash=false;};
    LockerWatch m_lockerWatch;
    void watchLockerEntry(int locker);
    // The entry latch's hearing radius: an arm's length (1.2 m) walked in,
    // five metres when the player ran at the doors (dash: faster than a walk
    // within 0.3 s of the latch). A guard who hears a locker's doors worked
    // checks that locker (lockerSuspectNoise); so does one whose confirmed
    // search ends within 1.5 m of a locker's doors, and, on a hunch, one whose
    // unconfirmed search does while his squad is on Caution (lockerAtSearch).
    float lockerEntryHeard()const;
    int lockerDoorNoise(const AiStimulus& noise)const;
    bool lockerAtSearch(int level,size_t actor);
    static bool lockerHunch(size_t actor,int locker,int x,int y);
    void suspectLocker(size_t actor,int locker);
    void lockerWitnessSense(int level,size_t actor,const ActorPose& pose);
    bool lockerSuspectNoise(int level,size_t actor,const ActorPose& pose,const AiStimulus& noise);
    int guardAtLocker(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove,bool free);
    void clearGuardLocker(size_t actor);
    void validateGuardLocker(size_t actor)const;
    Point3 guardLockerHandle(size_t actor,float& reach)const;
    // Bodies stowed in lockers: guard_stowed (fixture+1), guard_stowed_level
    // (+1) and guard_stow_ms (the slide in; at LockerStowMs he is shut in and
    // out of sight). One body to a locker, and nobody hides in that one.
    int lockerBody(int level,int fixture)const;
    bool npcStowed(size_t actor)const;
    int stowLockerAhead()const;
    bool stowBody();
    void updateStowedBody(size_t actor,ActorPose& pose,float dt);
    void tumbleFromLocker(int level,int fixture,size_t finder);
    // Every door motion a guard or a stow asks for goes through here, counted for the tests.
    void lockerDoorsMove(int level,int fixture,LockerMotion motion);int m_lockerDoorCalls=0;LockerMotion m_lockerDoorLast=LockerMotion::Enter;
    static bool testLockerGuards();
    // Caught in a locker (LockerCapture.cpp). A humanoid creature that watched
    // the player shut themselves in, or heard a knock from inside, tears the
    // doors open and takes hold: USE, mashed, against its bites. A guard who
    // hauls the player out pins their arms, walks them to open floor, puts them
    // on their knees and calls his squad to talk it over; the same struggle
    // breaks his grip. Not saved: a save mid-hold loads with the player loose.
    struct LockerCapture {
        enum class Kind {None,Creature,Guard};Kind kind=Kind::None;
        enum class Stage {Pin,March,Kneel,Verdict};Stage stage=Stage::Pin;
        int who=-1,locker=-1,line=0,teamLine=0,foot=0;
        const char* caption=nullptr;float captionAge=10; // what was just said over the prisoner (actorCaption)
        float age=0,stageAge=0,struggle=0,bite=0,talk=0,team=0,walked=0;
        bool called=false,march=false,lastUse=true;
        Vec2 spot{},dir{};
    };
    LockerCapture m_capture;
    bool captured()const;
    // 0..1 while held (the HUD's bar), -1 when not.
    float captureStruggle()const;
    const char* captureHint()const;
    static bool lockerHunter(CreatureKind kind);
    void creaturesWatchLocker(int index,const Player& before);
    void creatureLockerSense(Enemy& e);
    bool creatureLockerHold(Enemy& e,float dt);
    void beginCreatureGrapple(int self,int index);
    void beginGuardCapture(size_t actor,int index);
    bool captorIntent(int level,size_t actor,ActorPose& pose,float dt,Vec2& goal,float& goalZ,bool& wantsMove);
    void endGuardCapture(bool broke);
    bool updateCapture(const InputState& input,float dt);
    // The leaves a creature tore open stay on their stops while it holds its prey.
    void holdLockerDoors(int fixture);
    static bool testLockerCapture();
    // Authored sound and the room's acoustics (Ambience.cpp): the map's AMBIENT
    // emitters that play now and then or on a state, and what the audio
    // engine's reverb should sound like where the player stands.
    struct Acoustics {float wet=.12f,seconds=.8f;};
    Acoustics acoustics()const;
    void updateAmbience(float dt);
    static bool testAmbience();
    struct AmbientTimer {float at=0;int count=0;};
    std::unordered_map<std::uint64_t,AmbientTimer> m_ambientNext;std::unordered_map<std::uint64_t,bool> m_ambientLatch;
    struct AcousticsCache {Acoustics value{};float at=-1;int level=-1;};
    mutable AcousticsCache m_acoustics;
    static Game lockerCaptureScene(int creature); // --locker-capture-shots
    // --map-shots "caught"/"stow": the nearest guard comes for the locker the
    // player just hid in, or is knocked out and dragged to the one they face.
    // staged: he starts a few steps off the doors ("reach"), and with nobody
    // hiding he checks the locker a body was stowed in ("tumble").
    void inspectLockerGuard(bool stow,bool staged=false);
    // --map-shots "watch": the eye moved to a fixed observer for the frame drawn
    // (feet -999 the floor there), the scene simulated no further.
    void inspectLockerWatch(Vec2 at,float feet,float yaw,float pitch);
    // A guard's read on the player for the HUD: 0 unaware, up to 1 while
    // suspicion builds (recognition over recognitionMs), 2 once he has you,
    // 3 while he hunts the player he had confirmed, .5 checking a noise.
    float guardAwareness(size_t actor)const;
    // His card over his head: which glyph, how far the '?' has filled, how
    // long the '!' has been up (it pops for its first ~100 ms), and how far
    // through a radio call he is (0 when not calling).
    enum class GuardCardKind{None,Making,Checking,Hunting,Alert};
    struct GuardCard{GuardCardKind kind=GuardCardKind::None;float fill=0;int bangMs=0;float call=0;};
    GuardCard guardCard(size_t actor)const;
    // Lockers and bodies (ai/guard/Lockers.cpp). A man bound for a locker's
    // doors (a witness, a suspect check) wears the full '?' of a hunt: the doors
    // are about to open. The burst (leaveLocker): guard_burst (fixture+1) holds
    // the man at the doors until a leaf's edge reaches him; guard_burst_ms runs
    // from the hit, the push back easing out under him, until he lands.
    GuardCard guardLockerCard(size_t actor)const;
    bool lockerBurstPending(size_t actor)const;
    void updateLockerBurst(float dt);
    // The fall clip's phase at which his shoulders stand `lift` (0 down, 1
    // standing) of their full height, so a rise or a slump moves at an even pace.
    static float fallPhaseAtLift(ActorVisual visual,float lift);
    // The collar of a body (midway between his shoulders, where a hand takes
    // him), for the hands that stow him.
    Point3 npcCollar(size_t actor)const;
    std::pair<Point3,Point3> fallenHand(size_t actor,Point3 at,float yaw)const;
    // A confirmed hunt for the player is searched at least HuntSearchMs, so
    // EVASION's clock is the time the hunters really give it.
    static constexpr int HuntSearchMs=20000;
    int guardSearchMs(size_t actor)const;
    // A radio call is a committed action: guard_call is what he is reporting
    // (1 the player seen, 2 a body), guard_radio how long he has been on it.
    // Only a choke, a knockout, death or a fresh hit cuts it short. A sighting
    // that lands raises the alarm flag; a body that lands puts his squad on Caution.
    static constexpr int CallSighting=1,CallBody=2;
    void startGuardCall(size_t actor,int kind);
    bool guardCalling(size_t actor)const;
    // EVASION: a squad still hunting the player it had confirmed. The longest
    // search left (ms), smoothed so the clock only runs down within one hunt; 0 when none.
    int guardEvasion()const;int guardEvasionPeak()const{return std::max(m_evasionPeakMs,guardEvasion());}
    // The HUD keeps ALERT up this long after the last man loses sight of the player.
    static constexpr int AlertLatchMs=2000;
    // Caution: when a guard who had the player gives up his search, his squad
    // (or he alone, with no squad) stays wary this long: quicker to make the
    // player out, a little longer sighted, shorter at their posts. An Alert ends it.
    static constexpr int CautionMs=45000;
    int guardCaution(size_t actor)const;
    // Caution's longer sight, which a man in Alert keeps: guardCanSee, body
    // spotting and the CCTV radar's wedges all read it.
    bool guardWary(size_t actor)const;
    void startGuardCaution(size_t actor);
    // The HUD's alarm line: -1 while a guard in any resident chunk is in Alert
    // with the player as his target (and for AlertLatchMs after he loses them
    // while the hunt goes on), -2 during EVASION (guardEvasion has the clock),
    // else the longest Caution (ms) of a squad hostile to the player with a
    // man in this chunk, else 0.
    static constexpr int AlarmAlert=-1,AlarmEvasion=-2;
    int guardAlarm()const;
    static bool testCaution();
    static bool testAlarm();
    bool flashlightOn()const{return hasFlashlight()&&state(stateId("flashlight_on"))!=0&&!flashlightJammed();}
    static constexpr float FlashlightJamTime=1.5f;
    bool flashlightJammed()const;
    // Seconds inside an active jamming DarkZone, capped at FlashlightJamTime; 0 outside.
    float flashlightJam()const;
    static bool testFlashlight();
    // Switchable lamps (WorldLight gating, mirroring Hazard). lampSwitchedOn is
    // the state gate alone; lampPower adds the timed window and flicker (0..1).
    bool lampSwitchedOn(const WorldLight& lamp)const;
    float lampPower(const WorldLight& lamp)const;
    float lampPowerAuthored(const WorldLight& lamp)const;   // as authored: switching, timing, flicker, hung lamps, live leads
    // A lamp riding a prop that hangs on a joint (a swinging lamp): where its light is now (map coordinates), lit
    // while its prop is whole. False for any other lamp.
    bool hungLampAt(const WorldLight& lamp,Point3& at,bool* lit=nullptr)const;
    // A joint's prop in its level's list (-1: the room or not there), and a rope's or chain's line as it is now:
    // its ends (map coordinates; a pulley's four points: end, wheel, wheel, end). False when there is nothing to draw.
    int jointProp(int level,int spawn,int hint=-1)const;
    bool jointLine(int level,int joint,std::array<Point3,4>& points,int& count)const;
    // Curtains, straps and cables (Cloth.cpp, CLOTH records) on the level played: how many, and how each hangs now (its
    // beads, `cols` along it and `rows` down it, row 0 the top; and their radius). A round along origin..end knocks them.
    int clothCount()const{return int(m_cloths.size());}
    bool clothBeads(int index,std::vector<Point3>& points,int& cols,int& rows,float& bead)const;
    const ClothRun* clothRun(int index)const;
    void shootCloth(Point3 origin,Point3 end);
    // The highest top of a jointed prop under p within a step of `feet` (Movement's ground), or -1e9.
    float jointedPropTop(Vec2 p,float feet,float radius)const;
    // Feet at p on (or over) the top face of prop i of `level` when a joint holds it: it does not block him (a tilted plank).
    bool onJointedTop(int level,int i,Vec2 p,float feet)const;
    bool darkZoneActive(const DarkZone& zone)const;
    // 0 lit .. 1 true darkness: the player's eye inside an active DarkZone of
    // the current map, eased over about .3 s (and immediate after a load).
    float darkness()const;
    // The air the player's eye is in (World::haze: zone, map, environment),
    // eased over a second from the last; immediate after a load.
    Haze haze()const;
    // The player's muzzle flash as a light (0 none): about three frames lit; in
    // darkness it holds .12 s, burns brighter and fades out over .2 s after.
    float muzzleFlash()const;
    // Transient lights besides the player's muzzle flash, world xyz: NPC gun flashes in darkness and
    // every spark shower and arc (electricScene), the four that matter most to where the player stands.
    std::array<electric::Flash,4> weaponFlashes()const;
    static bool testDarkness();
    static bool testScripting();
    static bool customMapSmoke(const std::filesystem::path& file,std::ostream& report);
    bool inventoryOpen()const{return m_inventoryOpen;}
    // A conversation with choices is open (DialogueNode): the world runs on,
    // the player stands and picks a reply with 1-4, the arrows or the mouse.
    bool talking()const{return m_talk.actor>=0;}
    // The campaign's ash storm (StormCycle): its phase (0 calm, 1 warning, 2 storm),
    // how far it has closed in (0-1), the ash falling (0-1) and whether the player is under a roof.
    const StormCycle* stormCycle()const;
    int stormPhase()const;
    float stormIntensity()const;
    float ashFall()const;
    bool sheltered()const;
    void updateStorm(float dt);
    // The conversation panel's layout in HUD pixels (DisplayWidth x DisplayHeight), shared with the mouse.
    static constexpr int DialoguePanelY=244,DialogueFirstReplyY=294,DialogueReplyRowHeight=14;
    struct DialogueView {std::string speaker,line;std::vector<std::string> replies;int selected=0;float age=0;};
    std::optional<DialogueView> dialogueView()const;
    // The faction this actor fights as now: its declared one, or what its squad's stance makes it.
    AiFaction npcFaction(const ActorTrack& track)const;
    SquadStance npcStance(const ActorTrack& track)const;
    // Reactor rendering wakes during the first lift shake, one vertical band
    // at a time. Direct reactor entry and restored underground saves bypass it.
    float dormantBelow()const{return dormantBelow(m_level);}
    float dormantBelow(int level)const{
     const auto&world=chunkWorld(level);
     if(!world.hasLift()||m_player.z<-2)return -1000.f;
     if(world.liftPhase()==World::LiftPhase::Ready)return -4.5f;
     if(world.liftPhase()==World::LiftPhase::Ascending)return -4.5f-5.5f*std::clamp((world.liftPhaseTime()-10.f)/3.f,0.f,1.f);
     return -1000.f;
    }
    bool dormantEntity(float z)const{return m_world.hasLift()&&z<-2&&m_player.z>=-2&&m_world.liftPhase()!=World::LiftPhase::Crashed;}
    bool weaponEquipped()const{return m_weaponEquipped;}
    WeaponId equippedWeapon()const{return m_weaponEquipped?m_equippedWeapon:WeaponId::Fists;}
    // The player's row (kWeapons) for the firearm in hand or last drawn.
    const WeaponDef& equippedWeaponDef()const{return playerWeaponDef(m_equippedWeapon);}
    // That firearm and its rounds, whether raised or lowered: the HUD, the VR
    // plate and the viewmodel read these, and nothing else spends ammunition.
    WeaponId activeFirearm()const{return m_equippedWeapon;}
    const AmmoPool& firearmPool()const{return m_player.pool(m_equippedWeapon);}
    // The ammo counter: only the firearm in hand (none with fists or holstered),
    // its loaded and reserve rounds, magazine size and caliber.
    struct HudAmmo {bool shown=false;WeaponId weapon=WeaponId::Fists;int loaded=0,reserve=0,capacity=0;const char* caliber="";};
    HudAmmo hudAmmo()const{if(unarmed()||!m_weaponEquipped)return {};const auto&rounds=firearmPool();const auto&cycle=equippedWeaponDef().cycle;return {true,m_equippedWeapon,rounds.loaded,std::max(0,rounds.total-rounds.loaded),cycle.tubeCapacity,cycle.caliber};}
    const MeleeDef& unarmedMelee()const{return kUnarmedMelee;}
    // Immediate: holsters (Fists) or puts an owned firearm in hand, raised when it has rounds.
    void equipWeapon(WeaponId id);
    // The player's pick (wheel, keys, VR slot): Fists holsters at once; another
    // firearm lowers the one in hand, swaps at the bottom and raises the new one.
    void selectWeapon(WeaponId id);
    // The wheel: up draws the next owned firearm (from fists the last one drawn), down holsters.
    WeaponId wheelWeapon(int delta)const;
    // The VR shoulder slot's next pick: fists, the shotgun, the handgun once owned, then fists again.
    WeaponId shoulderSlotWeapon()const{WeaponId at=m_switchTarget!=WeaponId::Fists?m_switchTarget:equippedWeapon();
     return at==WeaponId::Fists?WeaponId::Shotgun:at==WeaponId::Shotgun&&hasHandgun()?WeaponId::Handgun:WeaponId::Fists;}
    // Switch animation: 0 none, 1 lowering the old gun (holster() rising to 1),
    // 2 raising the new one (holster() falling to raiseHolster). The swap of
    // activeFirearm() happens on the frame holster() reaches 1.
    int weaponSwitchPhase()const{return m_switchTarget!=WeaponId::Fists?1:m_switchRaise?2:0;}
    WeaponId weaponSwitchTarget()const{return m_switchTarget;}
    // Magazine reload: seconds in all, 0..1 through it (0 when not reloading),
    // and whether it started from an empty magazine (the longer one, ending
    // with the slide released at cycle.rackAt).
    float reloadDuration()const{return m_reloadTimer>0?m_reloadLength:0.f;}
    // The pump gun reloads shell by shell, at the pace of its authored clips:
    // a start, one insert per two missing shells (two to a hand), then the
    // finish with the pump
    // racked. Firing with a shell in the tube cuts it short.
    static constexpr float ShellStart=11/24.f,ShellInsert=17/24.f,ShellFinish=33/24.f,ShellLands=.6f;
    bool shellReload()const{return m_reloadTimer>0&&m_equippedWeapon==WeaponId::Shotgun;}
    float reloadElapsed()const{return m_reloadLength-m_reloadTimer;}
    static constexpr int ShellsPerInsert=2;
    int reloadShells()const{return std::max(0,int(std::lround((m_reloadLength-ShellStart-ShellFinish)/ShellInsert)));}
    float reloadProgress()const{return m_reloadTimer>0&&m_reloadLength>0?std::clamp(1-m_reloadTimer/m_reloadLength,0.f,1.f):0.f;}
    bool emptyReload()const{return m_reloadTimer>0&&m_emptyReload;}
    // The handgun's slide is held back: its magazine ran dry, until an empty
    // reload passes cycle.rackAt (the rack) or rounds are loaded.
    // The MAC-11's bolt the same: its cocking knob stays back on an empty magazine.
    bool slideLocked()const{return sidearm(m_equippedWeapon)&&firearmPool().loaded<=0&&!(m_reloadTimer>0&&m_emptyReload&&reloadProgress()>=equippedWeaponDef().cycle.rackAt);}
    int medkits()const{return m_medkits;}
    int selectedItem()const{return m_selectedItem;}
    int itemCell(int item)const{return m_itemCells[item];}
    static bool testInventory();
    bool quitRequested()const{return m_quitRequested;}
    int menuSelection()const{return m_menuSelection;}
    const Settings& settings()const{return m_settings;}
    void loadSettings(const std::wstring& path);
    void saveSettings(const std::wstring& path)const;
    const std::vector<SoundEvent>& sounds()const{return m_sounds;}
    bool audioMuted()const{return m_audioMuted;}
    bool musicEnabled()const{return m_musicEnabled;}
    static Game validationScene(Enemy::Kind kind,float deathTime=-1,float windup=0);
    static Game stalkerInspection(int clip,float phase,int view=0,Enemy::Kind kind=Enemy::Kind::Warden);
    // One creature's death, looked at from in front of it, `age` seconds on. Mode 0: as it fell. 1: shot
    // through at the moment it died. 2: killed by a blast. 3: no creature, a round into each material
    // on the floor. 4: alive, then killed by the player's own weapon and the game run on. `empty` is the
    // same view and clock with nothing in it.
    static Game goreInspection(Enemy::Kind kind,int mode,float age,bool empty=false);
    static Game creaturePreview(Enemy::Kind kind,int view);
    void posePreviewCreature(int clip,float phase);
    static Game terminalInspection(int level,int terminal=0);
    static Game mapInspection(Vec2 position,float angle,float pitch=0,int level=0,bool openDoors=false,float height=-999,bool sceneryOnly=false,WorldId id=WorldId::Campaign);
    // Move the inspection camera without rebuilding the map (audio orbit, map-shot walks).
    void inspectAt(Vec2 position,float angle,float pitch=0,float height=-999){m_inspectionCamera=true;m_player.pos=position;m_player.angle=angle;m_player.pitch=pitch;m_player.z=height==-999?(m_world.hasTerrain()?groundHeight(position,float(World::TerrainMaxZ+1)):m_world.floorHeight(position.x,position.y)):height;}
    // --map-shots "shut N": the chunk's doors shut again (the inspection opens them all).
    void shutDoors(){for(int i=0;i<int(m_world.doors().size());++i)m_world.setDoor(i,0,false);}
    static Game barrelInspection(float age);
    // --perf-profile (PerfProfile.cpp): a campaign view with god mode on (a run
    // never ends on a dead player), creatures added on the floor under him, every
    // hunter woken onto the player (perfWake each frame holds them on him), and
    // a one-line census of what is resident and awake. perfCensus lists every map.
    static Game perfScene(int level,Vec2 position,float feet,float yawDegrees,float pitch,bool openDoors=false);
    void perfClearCreatures();
    int perfSpawn(CreatureKind kind,int count,float minimumDistance);
    void perfWake(bool guards);
    void perfHold();
    int perfSpawnProps(int count);void perfShoveProps();
    std::string perfPopulation()const;
    static void perfCensus(std::ostream& out);

private:
    int nearbyTakedown()const;
    bool canTakedown(size_t actor)const;
    void restrainNpc(size_t actor,bool held);
    void releaseNpc(bool unconscious=false);
    void updateStealth(const InputState& input,float dt);
    void saveRestrainedPose(size_t actor);
    std::pair<Point3,Point3> friendlyShotBounds(size_t actor)const;
    bool m_previousSubdue=false;
    int m_takedownActor=-1;
    float m_takedownAge=0;
    WorldId m_worldId=WorldId::Campaign;
    std::shared_ptr<const CustomCampaign> m_customCampaign;
    std::vector<std::shared_ptr<const CustomCampaign>> m_customCampaigns;
    std::vector<std::string> m_customCampaignErrors;
    std::wstring m_customMapDirectory;
    std::uint64_t m_customCampaignKey=0;
    bool m_titleScreen=false,m_menuFromTitle=false,m_customMapsOpen=false;
    float m_aimBlend=0,m_scopeBlend=0,m_scopeWait=0;
    int m_titleSelection=0,m_customMenuOffset=0;
    InputState m_titlePrevious;
    void updateTitle(const InputState& input);
    MenuPage m_menuPage=MenuPage::Pause;
    // PAUSE and SETTINGS (SettingsMenu.cpp).
    SettingsTab m_settingsTab=SettingsTab::Audio;bool m_settingsOnTabs=true,m_settingsMuted=false,m_settingsSaveDue=false;int m_menuArmed=-1;
    void updatePauseMenu(const InputState& input);
    void updateSettingsMenu(const InputState& input);
    void settingsEscape();
    void leaveSettings();
    void settingsChange(int id,int direction,bool activate);
    void settingsDrag(int id,float fraction);
    void settingsReset(SettingsTab tab);
    // Chat and voice (SettingsMenu.cpp): the chat line, the scoreboard's mute pick, the voice settings handed to the
    // session, TEST MY MIC's listening (shared: a copied Game listens on the same device).
    bool m_chatOpen=false,m_chatFeeding=false,m_previousChat=false,m_micTest=false;std::string m_chatTyped;
    int m_scoreboardPick=-1;InputState m_boardPrevious;
    net::VoiceSettings m_voiceApplied;bool m_voicePushed=false;
    std::shared_ptr<class MicMonitor> m_micMonitor;
    void updateChat(const InputState& input);
    void updateScoreboardPick(const InputState& input);
    void updateVoice(const InputState& input);
    std::wstring m_saveDirectory;
    std::array<std::string,3> m_slotLabels{"SLOT 1 / EMPTY","SLOT 2 / EMPTY","SLOT 3 / EMPTY"};
    std::string m_checkpointLabel="AUTOSAVE / EMPTY";
    std::string m_menuMessage;
    int m_pendingSlot=-1;
    unsigned m_sessionRevision=0;
    MapSerial m_renderSerial; // renderSerial(): a new number on construction, copy and assignment
    unsigned m_liveEdits=0;std::vector<unsigned> m_mapEdits; // live edits so far, and how many touched each map
    void refreshSaveSlots();
    std::string savedLabel(const std::filesystem::path& path,const char* invalid)const;
    std::string encodeSave()const;
    bool decodeSave(const std::string& data);
    // Builds the game a save describes into `next` without touching this one
    // (it only reads this session's settings); decodeSave commits it.
    bool prepareLoad(const std::string& data,Game& next)const;
    void adoptSession(Game& next)const;
    bool beginLoad(std::string data,int slot);
    void finishLoad();
    bool loadReady()const;
    std::shared_ptr<LoadJob> m_loadJob;bool m_asyncLoads=false;
    // Returns how many chunks the archive carried; later ones are authored fresh. Formats 1-27
    // only (the positional text archive); format 28 is sections (encodeSections/decodeSections).
    template<class Archive> int archiveSave(Archive& archive,int version=2);
    // One chunk's parts, as the positional archive (every part, in its old order) and the format
    // 28 sections ("world", "creatures", "pickups", "props") both keep them.
    enum ChunkPart:unsigned {ChunkWorld=1,ChunkCreatures=2,ChunkPickups=4,ChunkProps=8,ChunkAll=15};
    template<class Archive> void archiveChunk(Archive& archive,int version,int index,unsigned parts);
    bool adoptSavedCampaign();
    std::string encodeSections(int& repaired);
    int decodeSections(const std::string& payload,std::uint32_t hash);
    static std::vector<SaveSection> builtInSaveSections();
    static std::vector<SaveSection> currentSaveSections();
    // A 1-27 save, read into this game: the per-version upgrade steps to the current form.
    void refitLegacySave(int fromVersion);
    // After a load of an older format from `path`: the original kept beside it as
    // <file>.v<old>.bak (never over another file), the file rewritten in the current format.
    SaveRefit refitSaveFile(const std::filesystem::path& path,const std::string& original)const;
    void noteRefit(const std::filesystem::path& path,const SaveRefit& refit);
    int m_loadedSaveVersion=0;
    SaveStore m_saveStore;
    bool nearReactorDisk()const;
    void useReactorAction(int action);
    World* reactorWorld();
    void syncReactorComplex();
    void updateConsole(const InputState& input);
    void executeConsole(std::string command);
    // Console summons (Summon.cpp, docs/console.md): creatures, Cleanup men and
    // civilians placed in front of the player with an allegiance and an order.
    // Transient: encodeSave leaves them, their states and their keys out.
    enum class SummonOrder:std::uint8_t {Idle,Patrol,Wander,Guard,Hunt,Follow};
    struct SummonRecord {std::uint32_t serial=0;std::string name,kind;bool creature=true;SummonOrder order=SummonOrder::Idle;
     std::shared_ptr<AuthoredMapData> map;Vec2 anchor{},patrolTo{};float anchorZ=0,patrolToZ=0,clock=0;int level=0,leg=0;};
    std::vector<SummonRecord> m_summons;std::uint32_t m_summonSerial=0;
    // Every state a summoned man's AI has touched (friendlyKey), for encodeSave to drop.
    mutable std::unordered_map<StateId,char> m_summonKeys;
    // `ai off`: no creature or NPC thinks. Per-actor allegiance from `ally`/`enemy`
    // on an authored man (summons carry their own). Neither is saved.
    bool m_aiOff=false;std::vector<std::pair<const ActorTrack*,AiFaction>> m_factionOverrides;
    bool consoleSummonCommand(const std::string& command);
    int summon(const std::string& kind,int count,const std::string& options,std::string& error);
    bool summonSpot(Vec2& at,float& z,float hull,float radius,const std::vector<Vec2>& taken)const;
    AiTargetRef aimedEntity(float range=40.f)const;
    void updateSummons(float dt);
    bool summonFlight(Enemy& e,Vec2& goal,float& goalZ,float dt);
    void stripSummons();
    bool m_consoleOpen=false,m_previousConsole=false,m_consoleUp=false,m_consoleDown=false,m_showFps=false,m_god=false,m_peaceful=false,m_unlockAll=false,m_noclip=false;
    std::string m_consoleLine;
    std::vector<std::string> m_consoleLog{"DEPTHWORKS DEVELOPER CONSOLE", "MAPS / MAP <NAME OR ID> / RELOAD / WHERE / FPS / CLEAR"};
    std::vector<std::string> m_consoleHistory;
    int m_consoleHistoryIndex=0;
    float m_renderScale=1.f,m_viewOverride=0;
    mutable float m_edgeFadeEnd=-1,m_edgeFadeTime=0;   // the eased edge-limited fade end (viewFade), and when it was last eased
    std::vector<StateValue> m_states,m_objectives;
    std::vector<QuestItemStack> m_questItems;
    std::vector<StateId> m_firedEvents;
    std::vector<ScriptEvent> m_scriptEvents;
    // ---- The AIs over map zones (AI records, AiAgents.cpp) and the AI blocks they are wired with ----
    // Every AI keeps what it decides in states (saved with the rest); these are only the fractions a state's
    // whole number cannot hold, rebuilt from the states after a load.
    std::vector<AiAgent> m_aiAgents;
    struct AiMind {float tension=0,quiet=0;bool seeded=false;};std::vector<AiMind> m_aiMinds;
    // The walking skinbot (AiAgents.cpp updateSkinbot): a map's skinbot model walks its beat round where it was
    // placed, until it is pointed somewhere. A body to look at only: it is not yet a creature, and is not saved.
    skinbot::Walker m_skinbot;int m_skinbotLevel=-1,m_skinbotStop=0;float m_skinbotWait=0;bool m_skinbotPointed=false;Vec2 m_skinbotHome{};
    std::vector<Vec2> m_skinbotDetour;float m_skinbotRetry=0;
    void updateSkinbot(float dt);
    // The middle of the patch the AI last heard someone in (or, haunt, hears them in most), if it has heard anyone.
    bool aiRemembered(bool haunt,Vec2& at,float& z)const;
    // The Lights block: per AI, a mode (0 as authored, 1 flicker, 2 dark, 3 red) and the tenths of a second left
    // (0 until changed), in states AI_lights and AI_lights_left; the Later block: a state per Later action
    // counting tenths down (Game::laterKey).
    float m_aiTick=0;
    void seedAiAgents();
    void updateAiAgents(float dt);
    float aiLampScale(const WorldLight& lamp,int level,Point3* tint=nullptr)const;
    static StateId laterKey(StateId event,size_t action){return stateId("later:"+std::to_string(event)+":"+std::to_string(action));}
    float m_hazardSoundTimer=0;
    // Darkness ramp and torch jam; runtime only, re-settled after any load.
    float m_darkness=0,m_flashlightJam=0;bool m_darknessSettled=false;
    Haze m_hazeFrom{},m_hazeTo{};float m_hazeBlend=1;
    // How lit the player is (0 black to 1 floodlit): lamps in reach with a
    // clear line, the map's ambient, true darkness and the flashlight.
    float m_playerExposure=1,m_exposureTimer=0;
    float darknessTarget(bool* jamming)const;
    void updateDarkness(float dt);
    void seedScripts();
    // ---- Map format 2 entity properties (EntityTraits.h, docs/Systems/map-format.md) ----
    // Every read and write of a named record's property goes through these four: scripts
    // (@entity.key conditions and STATE/ADD) and a prop's `frozen`. The authored value is the
    // map's; what scripts change goes to the save's entity store (setEntityProperty, the
    // "entities" section) under the map, the entity and "p.<key>" ("frozen" for the flag).
    struct EntityPropertyRef {int level=-1;std::string entity,key;int authored=0;};
    std::unordered_map<StateId,EntityPropertyRef> m_entityProperties;
    void seedEntityProperties();
    bool isEntityProperty(StateId id)const{return m_entityProperties.count(id)!=0;}
    int entityPropertyInt(StateId id)const;
    void setEntityPropertyInt(StateId id,int value);
    int entityPropertyInt(int level,std::string_view entity,std::string_view key)const;
    // Props held by anchored= or frozen= (Clutter::pinned), refreshed each physics step.
    void refreshPinnedClutter();
    void updateScripts(float dt);
public:
    // Preview-only observer. An index of -1 marks the trigger and its gates.
    void setScriptTrace(std::function<void(StateId, int)> trace) { m_scriptTrace = std::move(trace); }
private:
    std::function<void(StateId, int)> m_scriptTrace;
    // @health and @map read the player tested (players()); the rest are the world's.
    int scriptValue(const ScriptEvent& event,const ScriptCondition& condition,const PlayerView& player)const;
    // Ready for one player (on its map, its gates, inside its box), and over the players: any of them, or every
    // one when the trigger says players=all. who: the first player it is ready for.
    bool scriptEventReadyFor(const ScriptEvent& event,const PlayerView& player)const;
    bool scriptEventReady(const ScriptEvent& event,int* who=nullptr)const;
    void updateReceivingBreach();
    void executeScriptAction(const ScriptAction& action);
    void updateNoclip(const InputState& input,float dt);
    void spawnScriptEnemy(const ScriptAction& action);
    void spawnCreature(const CreatureSpawn& spawn,float awareness=0,bool announce=false);
    void updateHazards(float dt);
    explicit Game(WorldId id,bool deferCampaignLoad);
    void loadLevel(int level,bool carry,bool reportHealth=true);
    World makeWorld(int level)const;
    void selectCustomCampaign(int index);
    // New Game: the built-in campaign from its first map. A custom prologue
    // requests this through ENTER_CAMPAIGN; it applies at the next update.
    void startMainCampaign(int level=0);
    // Title New Game: the embedded Security Office prologue, then Map 0.
    void startNewGame();
    void startCustomCampaign(std::shared_ptr<const CustomCampaign> campaign);
    int customMenuItemCount()const{return int(m_customCampaigns.size())+1;} // uploaded campaigns + Back
    int customMenuLogicalIndex(int row)const{return m_customMenuOffset+row;}
    int m_level=0;
    struct ChunkState {World world;ai::NodeGraph nav;bool navReady=false;std::vector<Enemy> enemies;std::vector<Pickup> pickups;int kills=0;bool resident=true;std::vector<Clutter> clutter;};
    // Chunk storage outlives a frame and can be large; keep it off the native
    // stack, including nested save previews and campaign validation scenes.
    std::vector<ChunkState> m_chunks=std::vector<ChunkState>(MaxChunks);
    ai::NodeGraph m_levelNav{};
    bool m_levelNavReady=false;
    void storeChunk();
    void crossChunkBoundary();
    void ensureChunk(int level);
    void updateStreaming(float dt);
    void useDoor(int index);
    // Doors in play (DoorPlay.cpp): the hand kept on a leaf (hold E and it eases
    // while held, and stays where it is let go: ajar, to look through), a shove
    // from walking into a shut leaf, each leaf's sounds, and the creatures that
    // batter a door that gives.
    void updateDoorPlay(const InputState& input,float dt);
    bool batterDoor(Enemy& e,Vec2 goal);
    static bool battersDoors(Enemy::Kind kind){return kind==Enemy::Kind::Brute||kind==Enemy::Kind::Chimera||kind==Enemy::Kind::Machine;}
    int m_doorHand=-1;float m_doorHandTime=0,m_doorRattle=0;
    // Index of the door in chunk `level` across the boundary from this chunk's door, or -1.
    int boundaryPartner(int level,const Door& door)const;
    void updateClutter(const InputState& input,float dt);
    // Physgloves.cpp
    PhysgloveState m_physglove;
    void updatePhysgloves(const InputState& input,float dt);
    // Two-hand VR carry (Clutter.cpp): the second hand's grip on the held prop, in the prop's frame at the grab.
    int m_vrSecondHand=-1;float m_vrReleaseGrace=0;std::array<Point3,2> m_vrGripLocal{};std::array<float,4> m_vrGripTurn{1,0,0,0};Point3 m_vrGripAxis{},m_vrGripUp{};
    // Each hand's turn as the second hand took hold (the roll about the line between the hands is the mean of their turns since).
    std::array<std::array<float,4>,2> m_vrGripHandTurn{};
    bool updateTwoHandCarry(const InputState& input,Clutter& c);bool grabSecondHand(int hand);
    // One-hand VR carry: the prop's turn and middle in the carry hand's frame, taken as the hand closed on it (or as it
    // came back to one hand), so it turns exactly with the hand from where it was held.
    bool m_vrHoldSet=false;std::array<float,4> m_vrHoldTurn{1,0,0,0};Point3 m_vrHoldLocal{};
    std::array<bool,2> m_previousVrTrigger{};
    Point3 carryVelocity()const;
    // VR hand bodies (VrHands.cpp): each hand a kinematic sphere in the solver, its drawn place held out of props.
    std::array<Point3,2> m_vrHandProxy{};std::array<bool,2> m_vrHandBlocked{},m_vrHandProxySet{};bool m_vrHandStepped=false;
    static constexpr int VrHandBody=-3;   // hand k is the body of user VrHandBody-k
    bool vrHandFree(int hand)const;bool vrHandsNear(const std::vector<Clutter>& list)const;bool vrHandMeets(const vectra::Body& other)const;
    void addVrHandBodies(vectra::Space& space,std::vector<Clutter>& list,float h);void settleVrHands(const vectra::Space& space,int held);void followVrHands();
    // Liquids.cpp
    std::vector<LiquidDrop> m_liquidDrops;std::vector<Puddle> m_puddles;
    void updateLiquids(float dt);void spillLiquid(int level,Clutter& c,Point3 velocity,float share);void seedLiquid(Clutter& c);void emitLiquid(Point3 at,Point3 velocity,liquid::Kind kind,float volume,int drops,int stream=0);
    // Held heavy props: sweep bodies along a hand move, or re-clear overlaps after AI.
    void pushBodiesFromCarried(Clutter& prop,const Clutter& previous);
    void resolveHeldPropBodies();
    bool interactClutter(int hand=-1);
    float vrInteractionDistance(Point3 target,float housingRadius)const;
    int nearbyInteractionDoor()const;
    int nearbyClutter(int hand=-1)const;
    float clutterPathFraction(Point3 from,Point3 to,Point3&normal)const;
    void seedClutter();
    // Electrical leads (World::leads): each chunk's cables step with the clutter
    // physics. A clip tears off its prop pulled past the cable's length, carried
    // out of the chunk or broken; a live one arcs (LeadArc, a 14 m stimulus) and
    // shocks the hand holding the prop, LeadShock and never the last point.
    void updateLeads(int level,float dt);
    // Showers thrown as each burst of a torn live lead's crackle begins; old showers and arcs dropped.
    void updateElectricity(float dt);
    // Hang a chunk's cables at rest where their clips now are (a fresh map, a load).
    void settleLeads(int level);
    void tearLead(int level,int index,bool quiet=false);
    int leadProp(int level,const Lead& lead,int hint=-1)const;
    static Point3 leadClip(const Clutter& c,Point3 offset);
    // The prop's box as its clipped cable meets it, in the same frame as leadClip.
    static LeadBody leadBody(const Clutter& c);
    // How far lead i's cable runs into the prop it is clipped to (its box brought down to the
    // clip's level, leadBodyAtClip), at worst (0 or less: clear of it).
    float leadThrough(int level,int index)const;
    static constexpr float LeadShock=6.f;
    // What a carried prop that will not break absorbs before a blow tears it out of the hands.
    static constexpr float ShieldKnockAway=60.f;
    bool damagePane(int level,int index,float amount,Point3 at,Point3 direction,PaneBlow blow={});
    // The glass (GlassPanes.cpp): what a blow does to a pane that stays up (0 a crack, 1 a hole,
    // 2 it comes down), the resistance left at a point (what a prop's blow must beat there), the
    // shards a blow or a fall lets go, a shard landing hard, and bodies and feet meeting glass.
    int glassBlow(int level,int index,float amount,Point3 at,Point3 direction,PaneBlow blow);
    float paneResistance(int level,int index,Point3 at)const;
    void dropGlassPieces(int level,int index,const std::vector<glass::Piece>& pieces,Point3 direction,float blowRadius,bool blownProps);
    void dropCollapsedPane(int level,int index,Point3 at,Point3 direction);
    void crackGlassShard(int level,size_t index,float speed);
    void updateGlass(float dt);
    float m_glassCrunchWalked=0,m_glassBodyRearm=0;Vec2 m_glassCrunchLast{};
    void updateBreakables(float dt=0);
    void blastProps(Point3 origin,bool tiles);
    void simulateClutter(std::vector<Clutter>& list,int level,float dt,int held);
    // Vectra's step for a list's model props (RigidClutter.cpp; the old one: vectra::legacyProps()).
    void simulateRigidProps(std::vector<Clutter>& list,int level,float dt,int held);
    // A man or a creature knocked back by a prop (RigidClutter.cpp): he stumbles along `velocity`, slowing,
    // through his own hull sweeps; a creature's blow is spoilt while it lasts. A shove (not a nudge) plays
    // his stumble for `stumble` seconds (`age` so far): a man's hit clip (3), reeling while he slides and
    // finding his feet after, held where the shove leaves him (`at`), his gun down till he has; a
    // creature's flinch (its clip 3, through painFlash) from `flinch` down. Not saved.
    struct BodyKnock {int level=0;bool enemy=false;size_t index=0;Vec2 velocity{};float time=0;float stumble=0,slide=0,age=0,flinch=0;Vec2 at{},push{};bool placed=false;};
    std::vector<BodyKnock> m_bodyKnocks;
    void updateBodyKnocks(float dt);
    // After everyone has moved this frame: the knocked play their stumble (RigidClutter.cpp).
    void poseBodyKnocks(float dt);
    // Where in the hit clip a stumble is `age` seconds in: reeling over the slide, recovering after.
    static float stumblePhase(float age,float slide,float stumble);
    // A list's rigid-body state between frames: the fixed step's leftover time and the contacts' warm start.
    struct RigidPropState {float accumulator=0;std::vector<std::pair<std::uint64_t,vectra::Space::Cached>> cache;
     // A region of the world found to be one flat floor with every column round it open (RigidClutter.cpp),
     // kept per prop: asked again only when the prop leaves it or a door moves.
     int heldIndex=-1;vectra::Quat heldTurn{}; // the prop in the hand and how it was turned last step (its spin)
     std::vector<std::pair<std::uint64_t,Vec2>> actorLast;   // where each man and creature near props was last frame (their bodies' speed)
     struct Flat {const void* world=nullptr;float x0=0,y0=0,x1=0,y1=0,lo=0,hi=0,floor=0,doors=0;bool flat=false,valid=false;};std::vector<Flat> flat;};
    std::map<int,RigidPropState> m_rigidProps;
    // ---- Joints between props (PropJoints.cpp; World.h PropJoint, Vectra's Joint) ----
    // Each step, the level's joints as Vectra joints between the props' bodies (or a prop and the room), their
    // frames set from where the map laid the props out; what snapped is marked, sounded and its state= set.
    std::map<int,std::vector<std::pair<std::uint64_t,vectra::Space::JointCached>>> m_jointCache;   // warm starting, per level
    mutable CorpsePoses m_corpsePoses;   // corpsePoses()
    std::vector<std::pair<int,int>> jointedProps(int level,const std::vector<Clutter>& list);           // each joint's props (list indices; -1 the room)
    void addPropJoints(int level,std::vector<Clutter>& list,const std::vector<int>& bodyOf,vectra::Space& space,std::deque<vectra::Joint>& out,std::vector<int>& which);
    void afterPropJoints(int level,std::vector<Clutter>& list,vectra::Space& space,std::deque<vectra::Joint>& joints,const std::vector<int>& which);
    void snapPropJoint(int level,int joint,Point3 at,bool quiet=false);
    // A round along origin..end cuts every rope or chain it passes within its thickness of.
    void cutJointsByShot(Point3 origin,Point3 end);
    // One frame of the curtains, straps and cables (Cloth.cpp): built afresh when the level or its map changes.
    void stepCloth(float dt);
    void stashClothPoses(int level);     // copy live beads into m_clothPoses for a map (before rebuild or on save)
    void applyClothPoses(int level);     // put saved beads back onto m_cloths after they are built
    std::vector<std::shared_ptr<ClothRun>> m_cloths;const void* m_clothMap=nullptr;int m_clothLevel=-1;Point3 m_clothPlayerAt{};bool m_clothPlayerSeen=false;
    std::vector<ClothPose> m_clothPoses;  // physics layout 5: curtains/straps/cables across maps
    // What the last rigid step did (the lab's perf and debug views).
    struct RigidStats {int bodies=0,awake=0,manifolds=0,worldManifolds=0,steps=0,ccdSweeps=0;double ms=0,worldMs=0,pairMs=0,solveMs=0,buildMs=0,preMs=0,postMs=0,ccdMs=0;std::vector<vectra::ContactEvent> events;std::vector<vectra::Manifold> manifoldsSeen;} m_rigidStats;
    void updateSoftBodies(float dt);
    // The player on a soft prop (a floor mattress, a bedroll): its pressed top under p, or `height`.
    float softGround(Vec2 p,float feet,float height,float radius)const;
    void knockClutter(int level,size_t index,Point3 at,Point3 impulse);
    bool damageClutter(int level,size_t index,float amount,Point3 at,Point3 direction);
    void breakClutter(int level,size_t index,Point3 direction);
    // Pieces of props broken this frame (level, shard), added to their lists once nothing holds a reference into them.
    std::vector<std::pair<int,Clutter>> m_pendingShards;void landShards();
    bool tryMove(Vec2 delta);
    bool hullFits(Vec2 position,float feet,float height,float radius=.20f,bool creatures=true,bool props=true)const;   // props=false: a creature's step, which shoves loose props aside (Clutter kick) instead of stopping at them
    bool unstickPlayer();
    float groundHeight(Vec2 position)const;
    float groundHeight(Vec2 position,float feet,float radius=.20f)const;
    void updateMovement(const InputState& input,float dt);
    void updateLift(float dt);
    void updateInteraction(const InputState& input,float dt);
    // First footstep of the set for a surface: painted zone ground gives dirt,
    // grass or gravel; elsewhere metal floors ring and the rest is concrete.
    static Sound stepSoundAt(const World& world,Vec2 position);
    // How far a step on that floor carries against concrete: steel rings 1.6,
    // gravel crunches 1.35, packed dirt .85 and grass .7. stepHeard is the
    // hearing radius it is written with, times that: .95 m crouched (never past
    // 1.1 m, under the 1.25 m takedown reach) and 3.12 m walking, as concrete
    // always carried them, and 7.5 m sprinting (it was 3.6).
    static float stepNoise(Sound step);
    static float stepHeard(Sound step,bool crouched,bool sprint);
    bool lineOfSight(const Vec2& a, const Vec2& b) const;
    void shoot();
    ShotHit traceShot(Point3 origin,Point3 direction,float range,int ignoredNpc=-1,bool includePlayer=false,const HitscanDef* assist=nullptr,AiTargetRef ignoredEntity={})const;
    void resolveShot(const ShotHit& hit,Point3 origin,Point3 direction,const HitscanDef& definition,bool playerFired,float damageOverride=0,AiTargetRef attacker={});
    static constexpr int ScriptedFlinchMs=450;
    void startleScriptedActors(const ShotHit& hit,Point3 origin,Point3 direction);
    void reloadWeapon();
    void punchImpact();
    void applyPunchHit(Enemy& enemy);
    void applyNpcPunchHit(size_t actor);
    bool applyEnemyDamage(Enemy& enemy,float amount,bool bloodDecal=true,bool closeHitStop=false,int level=-1);
    void updateVrMelee(const InputState& input,float dt);
    // True when the blow reached the player. A carried prop held toward the source takes it instead
    // (shieldBlow): breakable props take the damage until they burst, solid ones are knocked away.
    bool receiveDamage(float amount,Vec2 source,bool blockable=true,bool shieldable=true);
    bool shieldBlow(float amount,Vec2 source);
    void updateEnemies(float dt);
    // The player a creature hunts (EnemyAI.cpp), chosen each think from players() by threat: seen now (its own
    // sight and the stealth rules), who wounded it lately, where it last knew someone, nearness; it keeps its
    // prey unless another is clearly better and never within a second; a pack spreads; the downed are dropped.
    const PlayerView& creatureTarget(Enemy& e,bool huntsPlayer);
    const PlayerView& creaturePrey(const Enemy& e)const;   // the one it hunts now (its last choice)
    // Hunter.cpp: the bite that takes a man's head, feeding on him, the
    // recurring hunter's flight, return and ear, and its walk in after the
    // player through a seam.
    void biteKill(Enemy& e,size_t actor);void headlessPool(size_t actor);
    bool updateFeeding(Enemy& e,float dt);
    int hunterPrey(const Enemy& e)const;
    void wakeHunter(Enemy& e);
    void vanishHunter(Enemy& e);
    bool hunterFlight(Enemy& e,Vec2& goal,float& goalZ,float dt);
    void hunterArrive(Enemy& e,float dt);
    // Machine.cpp: the cursed equipment, asleep among the plant until woken; the plant's live current
    // (machineCurrent: a live Electricity hazard, a torn live lead) seizes and shorts it out. machineUpdate
    // runs first each frame for a dormant kind and returns true while it is asleep, starting up or seized.
    bool machineUpdate(Enemy& e,const std::vector<AiStimulus>& stimuli,float dt);
    void wakeMachine(Enemy& e,Vec2 toward,float towardZ,bool knows);
    void wakeMachinesNear(Vec2 at,float z,float radius);
    bool machineCurrent(const Enemy& e,Point3& contact)const;
    void machineAttraction(const Enemy& e,Vec2& goal,float& goalZ)const;
    void shortMachine(Enemy& e,Point3 contact);
    Point3 machineLamp(const Enemy& e)const;
    // Set while a blast resolves through resolveShot: what it does is not a round, and no hide turns it.
    bool m_blastShot=false;
    void hearPlayerNoise(const std::vector<AiStimulus>& stimuli);
    struct PlayerNoise {Vec2 pos{};float z=0,time=-1;} m_playerNoise;
    unsigned creatureConditions(const Enemy& enemy,bool visible,bool heard,bool canMelee,bool hurt)const;
    CreatureScheduleChoice creatureScheduleChoice(const Enemy& enemy,unsigned conditions,bool arrive)const;
    bool creatureTaskAllows(const Enemy& enemy,CreatureTask task)const;
    void clearCreatureSchedule(Enemy& enemy)const;
    void runCreatureSchedule(Enemy& enemy,int ms,bool visible,bool heard,bool canMelee,bool hurt,bool wantsMove);
    // Perception runs in the map the actor stands in; its track and restraint
    // state belong to its home map, which differs once an escort crosses a seam.
    ActorPose friendlyAwareness(const World& world,int level,const ActorTrack& track,int homeLevel,size_t homeActor,const ActorPose& old,ActorPose pose,float dt)const;
    ActorPose updateFriendlyAI(World& world,int level,size_t actor,const ActorTrack& track,const ActorPose& old,ActorPose pose,float dt,const std::vector<AiStimulus>* stimuli=nullptr);
    void guardIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<AiStimulus>& stimuli,Vec2& goal,float& goalZ,bool& wantsMove);
    void guardBaseIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    void planGuardReposition(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,Point3 target,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    void runGuardSchedule(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    GuardScheduleChoice guardScheduleChoice(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,int ms,Vec2& goal,float& goalZ,bool& wantsMove,unsigned& conditions,GuardSchedule previous,int oldSubject,int retry,bool changed,bool close,const AiTarget& target);
    bool guardTaskAllows(size_t actor,GuardTask task)const;
    void finishGuardTask(size_t actor);
    void guardTaskMovement(size_t actor,const ActorPose& old,const ActorPose& pose,Vec2 waypoint,bool wanted,int ms);
    void failGuardTask(size_t actor,GuardTaskReason reason);
    void clearGuardSchedule(size_t actor);
    void validateGuardSchedule(size_t actor)const;
    void civilianEscortIntent(World& world,int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<Enemy>& enemies,Vec2& goal,float& goalZ,bool& wantsMove,float& speedScale);
    void scriptedHandoffIntent(World& world,int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<Enemy>& enemies,Vec2& goal,float& goalZ,bool& wantsMove,float& speedScale);
    void runCivilianSchedule(size_t actor,int ms,unsigned conditions,bool wantsMove);
    void clearCivilianSchedule(size_t actor);
    GuardCtx makeGuardCtx(size_t actor,int level,const ActorTrack& track,ActorPose& pose){return {*this,actor,level,track,pose};}
    void validateGuardState(size_t actor)const;
    void updateGuardFlags();
    void updateGuardCombat(float dt);
    NpcWeaponPose npcWeaponPoseAt(size_t actor,const ActorPose& pose,Point3 target,float raise)const;
    ShotHit guardShot(size_t actor,const ActorPose& pose,const NpcWeaponPose& weapon)const;
    void guardReposition(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,Point3 target,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    const ai::NodeGraph& navGraph(int level,float height);
    bool advanceNavGraph(int level,float height,size_t pairBudget);
    void prebuildNavGraph();
    bool findGuardGraphCover(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ);
    bool findGuardLateralCover(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ,float sideSign=0.f);
    bool findGuardCoverFromEnemy(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ,float sideSign=0.f);
    // Cover/path post with a clear raised shot — used for flank and offense in
    // rooms that already expose structure Cover on the retained nav graph.
    bool findGuardCombatPost(int level,size_t actor,const ActorPose& pose,Point3 threat,Vec2& goal,float& goalZ,float sideSign=0.f);
    float guardLateralClearance(int level,size_t actor,const ActorPose& pose,Vec2 direction)const;
    Vec2 guardGraphWaypoint(int level,Vec2 start,float feet,Vec2 goal,float goalFeet,float height);
    AiTarget resolveGuardTarget(size_t actor,AiTargetRef reference)const;
    AiTarget guardThreat(size_t actor,const ActorPose& pose)const;
    AiTarget guardTarget(size_t actor)const;
    AiTarget guardAttacker(size_t actor)const;
    bool guardCanSee(size_t actor,const ActorPose& pose,const AiTarget& target)const;
    void rememberGuardTarget(size_t actor,const AiTarget& target);
    AiTarget guardSquadReport(size_t actor)const;
    bool guardShotMatches(size_t actor,const ShotHit& hit)const;
    bool guardMeleeReady(size_t actor,const ActorPose& pose,const AiTarget& target)const;
    void damageGuardTarget(size_t actor,float amount);
    bool guardCovered(size_t actor,const ActorPose& pose,const AiTarget& target)const;
    void updateGuardSquadSlots(float dt);
    StateId guardCautionKey(size_t actor)const;
    void updateGuardCaution(float dt);int m_cautionMicros=0;
    // The HUD's ALERT latch and EVASION clock (not saved: both rebuild from the guards).
    // m_alarmSting spaces the detection sting and the '?' tick when several men turn at once.
    int m_alertLatchMs=0,m_evasionMs=0,m_evasionPeakMs=0;float m_alarmSting=0,m_alarmTick=0;
    int guardHuntLeft(size_t actor)const;bool guardAlarmActor(size_t actor)const;
    bool guardFireSlot(size_t actor)const;
    int guardFlankSide(size_t actor)const;
    bool guardPositionReserved(size_t actor,Vec2 position,float feet)const;
    bool guardHostileToPlayer(size_t actor)const;
    void validateGuardPolicyState(size_t actor)const;
    void validateGuardActions(size_t actor)const;
    bool guardDangerIntent(size_t actor,ActorPose& pose,float dt,Vec2& goal,float& goalZ,bool& wantsMove);
    bool guardGrenadeExposed(size_t actor)const;
    void validateGuardDanger(size_t actor)const;
    void guardSupportIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,Vec2& goal,float& goalZ,bool& wantsMove,bool advanceWork=true);
    void guardEngineerIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove);
    void validateGuardSupport(size_t actor)const;
    void runGuardTreatment(size_t actor,int ms);
    bool guardPatientIntent(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove,int& medic);
    int guardPatientMedic(size_t actor)const;
    void releaseGuardPatient(size_t actor);
    int guardCoveringPeer(size_t actor)const;
    unsigned guardConditions(size_t actor,const ActorPose& pose)const;
    void guardAvoidGrenade(size_t actor,const ActorPose& pose,Vec2& goal,float& goalZ,bool& wantsMove);
    void updateEngineerDrones(float dt);
    void validateEngineerDrones()const;
    // AI state holds enemies as (kind, level+1, index+1). When a chunk's list
    // changes, moved[i] says where its entry i is now; references follow it.
    void remapEnemyRefs(int level,const std::vector<AiTargetRef>& moved);
    void followEnemyRef(const std::function<StateId(std::string_view)>& key,std::string_view prefix,int level,const std::vector<AiTargetRef>& moved);
    void remapDroneEnemyRefs(int level,const std::vector<AiTargetRef>& moved);
    void remapGrenadeEnemyRefs(int level,const std::vector<AiTargetRef>& moved);
    bool deployEngineerDrone(size_t owner);
    void damageEngineerDrone(int slot,float amount,AiTargetRef attacker={});
    ShotHit traceEngineerDrone(Point3 from,Point3 to,size_t owner)const;
    bool engineerDroneChunkNeeded(int level)const;
    bool guardSuppressionPoint(size_t actor,Point3& point)const;
    bool guardRoute(int level,size_t actor,Vec2 globalGoal,float feet,int ms,Vec2& localGoal,float& goalFeet);
    void updateTacticalGrenades(float dt);
    void tryGuardGrenade(size_t actor,float dt);
    void validateTacticalGrenades()const;
    bool spawnTacticalGrenade(Point3 origin,Point3 velocity,AiTargetRef attacker,StateId squad=0,AiFaction faction=AiFaction::Neutral,int fuseMs=2400);
    void explodeTacticalGrenade(int slot,Point3 globalPosition,AiTargetRef attacker);
    ShotHit tacticalGrenadeTrace(Point3 from,Point3 to)const;
    int npcMilliseconds(size_t actor,float dt,std::string_view field);
    std::vector<AiStimulus> aiStimuli()const;
    void rebuildFriendlyActors();
    void updateFriendlyActors(float dt,bool guards=false);
    void updateNpcVoices(float dt);
    bool npcVoice(size_t actor,NpcVoiceCue cue);
    bool npcHasVoice(size_t actor)const;
    // A scripted person shot: a gasp, or the death cry of his (her) voice once he is killed.
    void scriptedPersonCry(const ActorTrack& track,size_t index,Vec2 at,float feet,bool killed);
    // A cue in a person's own voice (a scream, a moan, a breath) whose body at the cue is dead or gone: it stays silent.
    bool vocalCueSilenced(int level,Vec2 at)const;
    StateId friendlyKey(size_t actor,std::string_view field)const;
    World& friendlyWorld(int level){return level==m_level?m_world:m_chunks[level].world;}
    int friendlyChunkAt(Vec2 global,int preferred)const;
    bool friendlyBoundaryOpen(int from,int to,Vec2 global)const;
    bool friendlyWalkSegment(int level,Vec2 start,float feet,Vec2 goal,float height,float* endFeet=nullptr);
    bool friendlyWalkSegmentUncached(int level,Vec2 start,float feet,Vec2 goal,float height,float* endFeet=nullptr);
    float friendlySupportAt(int level,Vec2 position,float feet);
    bool friendlyBodyFits(int level,Vec2 position,float feet,float height);
    // Cleanup's kneeling bodies stay near standing height; the player's deep
    // crouch is a separate pose and cannot describe an NPC's physical hull.
    static float npcCrouchedHeight(const ActorTrack& track){return track.scale*.85f;}
    float friendlyHullHeight(size_t actor,const ActorTrack& track)const;
    bool friendlyRayClear(Point3 from,Point3 to)const;
    bool friendlyPortal(int from,int target,Vec2 position,float feet,float height,Vec2& goal,float& goalFeet,float targetFeet=std::numeric_limits<float>::quiet_NaN());
    int nearbyFriendly()const;
    bool commandFriendly();
    // How a squad is holding up: 1 steady, 2 shaken (a man down or the speaker badly hurt), 3 broken (half the squad down).
    int squadMood(size_t actor)const;
    void stanceStruck(size_t actor,AiTargetRef attacker);
    void setSquadStance(size_t actor,SquadStance stance);
    void updateStances(float dt);
    int nearbyTalker()const;
    bool openDialogue();
    void updateDialogue(const InputState& input,float dt);
    void closeDialogue();
    std::vector<int> dialogueReplies()const;
    const DialogueNode* dialogueNode()const;
    struct TalkState {int actor=-1;StateId node=0;int selected=0;float age=0;bool primed=false;InputState previous{};} m_talk;
    void requestAiDoor(World& world,int level,Vec2 position,float feet,Vec2 goal);
    bool aiDoorLocked(const World& world,int level,const Door& door)const;
    int hostilesRemaining(int level)const;
    void damageFriendly(size_t actor,float amount,Vec2 source,AiTargetRef attacker={});
    void migrateEnemiesAcrossChunks();
    void updatePickups();
    // Records where a downed carrier's pistol fell and lets the player take it.
    void updateSidearms();
    // The round count a downed carrier's pistol gives: his magazine, 6-12.
    int sidearmRounds(size_t actor)const;

    World m_world;
    Ragdoll m_hazmat;
    float m_hazmatPushCooldown=0;
    // Empty suits on hooks (World::hungSuits), by chunk: hung when the chunk is resident, swung
    // by the player and by shots on the current chunk. Not saved: a load hangs them again.
    std::vector<std::vector<Ragdoll>> m_hungSuits;float m_suitPushCooldown=0;
    void updateHungSuits(float dt);
    // How long the player has leaned on a heavy crate (it goes at ShoveSeconds).
    float m_crateShove=0;
    void updatePushCrates(const InputState& input,float dt);
    bool crateFree(const World& w,size_t index,Vec2 at)const;
    void restoreCrates(World& w,int level)const;
    Player m_player;
    net::Session* m_netSession=nullptr;          // multiplayer: not owned, not saved
    std::shared_ptr<net::Match> m_match;          // live while in a match (Match.h)
    std::vector<net::RemotePlayer> m_remotePlayers;
    net::MatchView m_matchView;
    // The title's multiplayer pages (MultiplayerMenu.cpp).
    TitlePage m_titlePage=TitlePage::Main;
    std::string m_nameDraft;
    bool m_nameUpper=false;
    int m_browseMode=0,m_browseScroll=0;bool m_browseHideFull=true,m_browseSearched=false,m_quickSearching=false;int m_quickMode=0;
    HostDraft m_hostDraft;std::string m_joinTyped,m_titleNotice;float m_joinCopied=0;
    bool m_scoreboardHeld=false;
    void updateMultiplayerMenu(const InputState& input);
    void updatePlayerNameKeyboard(const InputState& input);
    void savePlayerName(std::string name);
    void drainSessionEvents();
    void browseRefresh();
    friend class net::Match;
    // Multiplayer hooks the Match sets (Match.cpp): a guest's scripts and creature minds stay off (the host runs
    // them and sends what happens), and the bodies a shot is traced against while the Match resolves it.
    bool m_netScriptsOff=false,m_netCreaturesRemote=false;
    const std::vector<NetBody>* m_netBodies=nullptr;
    AvatarPose m_netAvatar{};
    // The players list (players()) and what it is built from; the co-op traffic the game leaves for the Match.
    std::vector<RemoteBody> m_remoteBodies;
    mutable std::vector<PlayerView> m_playerViews;
    struct NetHurt {int id=0;float amount=0;};
    std::vector<NetHurt> m_netHurts;
    std::vector<ScriptFx> m_scriptFx;
    struct NetUse {int level=0,terminal=0;};
    std::vector<NetUse> m_netUses;
    // Match publishes the remote footfalls after the game update. The next update feeds them to hearing and audio.
    std::vector<SoundEvent> m_netSounds;
    bool m_padPrompts=false;   // the pad was the last thing touched (InputState::padActive)
    int m_scriptPlayer=0;       // the player whose step or hand fires the script running now
    int m_shooter=0;            // the player whose shot is being resolved (a co-op host resolving a guest's)
    // ----- Several maps at once (ForeignChunks.cpp; docs/Systems/multiplayer.md "Several maps") -----
    // A co-op host runs every map a player stands on. Its own map is the active chunk as always; each other one
    // is swapped in as the active chunk for a frame of its own (ChunkFrame) in which its creatures, people and
    // scripts think against the players on it (players() holds the players in the active chunk's frame: its map
    // and the maps stitched beside it on the grid). m_netForeign: the maps that get such a frame this update
    // (the Match sets it); m_netKeep: maps kept resident for a player on them or beside them. Both empty alone.
    std::vector<char> m_netForeign,m_netKeep;
    bool m_foreignFrame=false;int m_foreignHome=-1;Player m_foreignLocal{};
    struct ChunkFrame {
     Game& g;int home=-1,level=-1,held=-1,carry=-1;bool swapped=false,homeResident=false;Vec2 delta{};
     std::vector<SoundEvent> sounds;PlayerNoise noise{};
     ChunkFrame(Game& game,int level);~ChunkFrame();
     ChunkFrame(const ChunkFrame&)=delete;ChunkFrame& operator=(const ChunkFrame&)=delete;
    };
    void updateForeignChunks(float dt);
    // Whether a map's players and things are in the active chunk's frame: it, or a resident map stitched beside
    // it (touching on the grid; maps of a custom campaign may share an origin and are then not in each other's).
    bool inPlayerFrame(int level)const;
    bool stitchedTo(int a,int b)const;
    // A person thinks in this update's frame: the host's own frame takes everyone not on a map that has its own.
    bool actorInFrame(int level)const{if(m_foreignFrame)return level==m_level;return level<0||size_t(level)>=m_netForeign.size()||!m_netForeign[size_t(level)];}
    // To another map keeping every map's state (the Match's teleports): the active chunk is stored and that one
    // swapped in as crossing a seam does; the player is placed by the caller.
    void enterChunk(int level);
    // ----- Props in a match (MatchProps.cpp): the host's are the truth -----
    // m_netPropsRemote (a guest): props are posed from the host; a hand, a punch or a kick on one is sent to the
    // host (m_netPropIntents) instead of done here, and nothing breaks here but what the host breaks.
    // m_netPropsHost: breaks are queued for the guests (m_netPropEvents). m_netPropApply: the Match is applying the
    // host's word (or a guest's intent on the host). m_propSimulating: inside the solver's own step.
    bool m_netPropsRemote=false,m_netPropsHost=false,m_netPropApply=false,m_propSimulating=false;
    struct NetPropIntent {int type=0,level=0;std::uint32_t id=0;Point3 at{},v{};float amount=0;};   // type 0 knock (v impulse), 1 damage (v way)
    std::vector<NetPropIntent> m_netPropIntents;
    struct NetPropEvent {int level=0;std::uint32_t id=0;Point3 way{};};   // a prop the host broke
    std::vector<NetPropEvent> m_netPropEvents;
    void updateNet();                              // the Session's frame: makes and drops the Match
    void updateFrame(const InputState& input,float dt);
    std::vector<Enemy> m_enemies;
    std::vector<FriendlyActor> m_friendlyActors;
    std::vector<AiStimulus> m_guardShots;
    std::vector<Pickup> m_pickups;
    std::vector<Clutter> m_clutter;
    std::vector<SoftPad> m_softPads;int m_softLevel=-1;bool m_softGrounded=true;float m_softAirSpeed=0;
    std::vector<BulletImpact> m_bulletImpacts;
    std::vector<ShotRay> m_shotRays;std::vector<HitBurst> m_hitBursts;
    std::vector<WaterSplash> m_waterSplashes;
    // Water (WaterFx.cpp), runtime only: the player's feet a frame ago (a fall into water), the way
    // waded since the last wake, the sounds already looked at for steps in water, and which
    // creatures and people were already down (a body falls into the water once).
    float m_waterFeet=-1e9f,m_wakeWay=0;size_t m_waterSoundsSeen=0;std::vector<char> m_waterNpcDown;std::vector<int> m_waterDeaths;
    // A splash where something meets the water: level-local point on the surface; its sound (and
    // the AI's hearing of it) now, or when a later `time` comes.
    void raiseWaterSplash(int level,Point3 point,Point3 direction,float power,WaterSplashKind kind,float delay=0);
    // Steps, wakes and falls into water; `bodies` the pass after the creatures moved (Game::update).
    void updateWater(float dt,bool bodies);
    // point and direction level-local to `level`.
    void hitBurst(int level,Point3 point,Point3 direction,ImpactMaterial material,int species,float power,bool kill);
    std::vector<BarrelExplosion> m_barrelExplosions;
    std::vector<BreakEvent> m_breakEvents;
    std::vector<GlassBreak> m_glassBreaks;
    std::vector<electric::Burst> m_sparkBursts;std::vector<electric::Arc> m_timedArcs;
    // This frame's electricity per chunk (electricNow) and the transient lights from it (weaponFlashes).
    struct ElectricFrame {std::uint64_t key=0;std::vector<electric::Scene> scenes;std::vector<std::uint8_t> made;bool lit=false;std::array<electric::Flash,4> flashes{};};
    mutable ElectricFrame m_electricFrame;std::uint32_t m_electricEdits=0;
    // Barrels caught in a blast go up after a short fuse.
    std::vector<BarrelExplosion> m_pendingBarrels;
    void detonateBarrel(int level,int tx,int ty);
    int m_heldClutter=-1;bool m_propHalfStep=false,m_propPushAttempt=false;
    int m_carryClutter=-1;float m_carryDistance=.9f,m_carryYawOffset=0;
    // Desktop release inherits the hand path (and this frame's look flick) like VR hand velocity.
    Vec2 m_carryHandVelocity{};float m_carryHandVz=0,m_lookYawRate=0,m_lookPitchRate=0;
    int m_npcNavBudget=4;
    bool m_aiQueryCaching=true;
    bool m_aiDecisionScheduling=true;
    bool m_navAuthority=true;
    bool m_vrAimValid=false,m_vrSupportGrip=false,m_vrChamberEmpty=false,m_vrPumpBack=false;float m_vrPumpRest=0,m_vrPumpAlong=0;
    Point3 m_vrAimPosition{},m_vrAimDirection{};
    // VR guns by hand (VrGuns.cpp). gun: the weapon this state is for (a switch resets it). held: 1 a magazine, 2 a
    // shell in the support hand; spentGrip: one just went in and the hand has not opened. The handgun: magOut,
    // chambered, slideBack (locked open), racking (rackFrom the fist where it took the slide, rack its pull back).
    // The shotgun: spent (a fired case waits in the chamber for the pump), cycleAt (a one-handed shot's self-cycle).
    // kick*: the recoil springs. drop*: the magazine let fall.
    struct VrGunState {WeaponId gun=WeaponId::Fists;int held=0;bool spentGrip=false,prevGrip=false,wasSupport=false;
     bool magOut=false,chambered=false,slideBack=false,racking=false;Point3 rackFrom{};float rack=0,rackMax=0,seat=-1,depth=0;
     bool spent=false;float cycleAt=-1;
     float kickBack=0,kickBackV=0,kickRise=0,kickRiseV=0;
     bool dropped=false,dropRest=false;Point3 dropAt{},dropVelocity{},dropRight{},dropUp{},dropBack{};float dropAge=0;WeaponId dropGun=WeaponId::Handgun;} m_vrGun;
    VrGunInput m_vrGunIn;
    std::vector<VrHaptic> m_vrHaptics;
    // Every case the VR guns threw through ejectCasing (kind, where, how fast), the last 16: what the tests read.
    struct VrEject {int kind=0;Point3 at{},velocity{};};std::vector<VrEject> m_vrEjected;
    void vrEject(int kind,Point3 at,Point3 velocity);
    void updateVrGun(const InputState& input,float dt);
    void vrShotFired();
    void soundAt(Sound sound,Point3 at,float gain,float pitch=1);
    bool m_vrInputActive=false;int m_vrCarryHand=1,m_vrInteractionHand=1;
    std::array<Point3,2> m_vrHandPosition{},m_vrHandDirection{},m_vrHandVelocity{},m_vrHandUp{};
    std::array<bool,2> m_vrHandValid{},m_vrMeleeTracked{};
    std::array<Point3,2> m_vrPreviousFist{};
    std::array<float,2> m_vrFistCooldown{};
    bool m_previousFire = false;
    bool m_previousReload = false;
    // A semi-automatic trigger must come back up before the next shot.
    bool m_triggerReset=true;
    int m_previousChoice=0;
    // Full auto (the MAC-11): the spread's bloom, the view's climb still to give
    // back, and the seed of the spread's own generator. Not saved.
    float m_spread=0,m_recoilDebt=0;unsigned m_spreadSeed=0x2545f491u;float nextSpreadRandom();
public:
    // The shotgun is eight pellets (docs/Systems/weapons.md): each its own ray inside a cone, from one seed, so a
    // client can lay out the same spread the host traced. A pellet carries about a third of a shot's damage and push,
    // and does not pull toward a target the way the single aimed ray does.
    static constexpr int ShotgunPellets=8;
    static Point3 pelletDirection(Point3 aim,std::uint32_t seed,int pellet);   // deterministic: same seed and pellet, same ray
    static HitscanDef pelletDef(const HitscanDef& shot);
private:
    // The sidearm drawn last: the one the hip holster keeps while neither is out.
    WeaponId m_lastSidearm=WeaponId::Handgun;
    std::vector<Casing> m_casings;unsigned m_casingSeed=0x9e3779b9u;float m_casingClink=-1;
    static void placeMachinePistolCell(Game& g);
    float casingRandom();
    void updateCasings(float dt);
    // Pending switch: the firearm to swap to at the bottom (Fists: none), then the raise.
    WeaponId m_switchTarget=WeaponId::Fists;bool m_switchRaise=false;
    float m_reloadLength=0;bool m_emptyReload=false;
    float m_weaponKick = 0.0f,m_reloadTimer=0.0f;
    float m_shotCooldown = 0.0f;
    Vec2 m_velocity{};float m_stuckTime=0;
    float m_damageFlash = 0.0f;
    float m_stormHurtTimer=0;
    float m_hitStopRemaining=0.f;
    float m_elapsed = 0.0f;
    float m_grabbed=0; // Seconds a Snatcher still has hold of the player: slowed, no sprint.
    std::unordered_map<StateId,float> m_sequenceEnded; // Game time each one-shot reached its end; not saved.
    int m_kills = 0;
    bool m_won = false;
    bool m_pendingMainCampaign = false;
    bool m_handsLowered = false;
    float m_hitFlash=0,m_critFlash=0;
    // When a lead last shocked the hand on its prop: a pair torn together shocks once. Not saved.
    float m_leadShockAt=-10;
    WeaponMotion m_weaponMotion;
    Vec2 m_sway{},m_swayVelocity{};
    float m_elbowVelocity=0,m_shotAge=10;
    float m_holster=0,m_punchAge=10,m_verticalSpring=0,m_verticalSpringVelocity=0;
    bool m_punchLeft=false;
    bool m_guarding=false;
    std::vector<SoundEvent> m_sounds;
    float m_stepDistance=0;
    // The last footstep's hearing radius and its age (the noise meter); runtime only.
    float m_stepNoise=0,m_stepNoiseAge=99;
    // The player's body: gait phase (radians, one cycle per two footsteps) and
    // whether it is sprinting. Presentation only; not archived.
    float m_bodyGait=0;bool m_bodySprint=false;bool m_inspectionCamera=false;
    bool m_previousJump=false,m_previousUse=false,m_previousProne=false,m_proneToggle=false;
    float m_jumpBuffer=0,m_coyote=0,m_logTime=0;
    int m_activeLog=-1;
    int nearbyTerminal()const;
    std::string m_pickupNotice;
    float m_pickupNoticeTime=0;
    std::string m_areaTitle;
    float m_areaTitleTime=0;
    unsigned m_stepVariant=0;
    bool m_audioMuted=false,m_musicEnabled=true,m_previousMute=false,m_previousMusic=false,m_previousFlashlight=false;
    Settings m_settings;bool m_vrRecalibrate=false;
    bool m_paused=false,m_inventoryOpen=false,m_quitRequested=false,m_previousEscape=false,m_previousInventory=false,m_suppressFire=false;
    int m_menuSelection=0,m_pointerX=-1,m_pointerY=-1,m_dragSlider=-1;
    std::array<bool,2> m_previousVrGrip{};
    InputState m_menuPrevious;
    void updateMenu(const InputState& input);
    void updateInventory(const InputState& input);
    bool m_weaponEquipped=true,m_inventoryClick=false,m_inventoryUse=false;
    WeaponId m_equippedWeapon=WeaponId::Shotgun;
    int m_medkits=0,m_selectedItem=-1;
    // Storage cells: 0 shotgun 4x2, 1 shells 1x2, 2 first aid 2x2, 3 9 mm 1x2 (version 19), 4 .380 1x2 (inventory section 2).
    std::array<int,5> m_itemCells{12,0,2,1,4};
    void sound(Sound sound,float gain=1,float pitch=1);
    void enemySound(const Enemy& enemy,int action,float gain=1,float pitch=1);
    void updateWeaponMotion(const InputState& input,float dt);
};

}

#pragma once
#include "../core/Math.h"
#include <array>
#include <cstddef>
#include <stdexcept>

namespace retro {
// A chunk index is local to a world. Never use it as a global content identity.
enum class WorldId { Campaign, Ashfall, Custom };
enum class Environment { Interior, Outdoor };
enum class FloorMaterial { Industrial, OfficeCarpet };
// 24-38: OFFICE pack furniture and computers (assets/office, OFFICE-SOURCE.md).
// 39-46: Mega Pack industrial set pieces (assets/facility/industrial).
inline constexpr int FacilityModelCount=332;
enum class CreatureKind { Huntsman, Wasp, Brute, Warden, Mutant, Snatcher, Chimera };
// One record per creature kind: what it is called, how much it takes, the
// standing height navigation and hits use (hull) against its visible top, its
// pace, bite, wind-up before a strike and the step it can climb; its call set
// (0 huntsman, 1 wasp, 2 the big bodies) and pitch; whether it works doors;
// and whether it is a skinned body posed from baked clips. Then how its clips
// run against game time: the idle loop's length, gait radians per metre walked
// (one move-clip cycle every 2*pi of gait), how fast a flinch decays, and how
// long the death clip takes before the corpse settles. Last, how it fights:
// melee reach, the rush speed of a stalker (0: it does not stalk; a stalker
// stares, flanks to the blind side, rushes, and hides when badly hurt), how
// long a strike lands for and the cooldown after it.
struct CreatureTraits {const char* name;float health,hull,top,speed,damage,windup,step;int voice;float voicePitch;bool opensDoors,baked;
 float idleSeconds,gaitPerMetre,painDecay,deathSeconds,reach,rushSpeed,strikeSeconds,cooldown;
 // A stalker that freezes while the player watches it (the reactor's thing);
 // one that does not ends its stare-down and charges when caught flanking.
 bool freezesWatched;};
inline const CreatureTraits& creatureTraits(CreatureKind kind){
 static constexpr CreatureTraits table[]={
  {"HUNTSMAN",       110,1.05f,1.05f,1.4f, 9,.32f,.62f,0,1.f,  false,false,2.5f,7.f, 5.f,1.15f,1.05f,0.f, .25f,1.f,false},
  {"XENOWASP",        85,1.6f, 1.55f,1.85f,9,.32f,.46f,1,1.f,  false,false,2.5f,7.f, 5.f,1.15f,1.05f,0.f, .25f,.8f,false},
  {"SCISSOR FIEND",  280,1.85f,1.85f,.75f,18,.8f, .34f,2,1.f,  false,false,2.5f,7.f, 5.f,1.15f,1.25f,0.f, .25f,1.6f,false},
  {"REACTOR STALKER",320,1.85f,1.85f,1.4f,30,.55f,.38f,2,.78f, true, true, 2.5f,7.f, 5.f,1.15f,1.1f, 3.8f,.45f,1.05f,true},
  {"MUTATED HUMAN",  180,1.85f,1.85f,1.9f,16,.55f,.34f,2,1.f,  true, true, 2.5f,7.f, 5.f,1.15f,1.05f,0.f, .25f,1.6f,false},
  // A transformed worker that ambushes rather than chases: it limps at 1.1 m/s
  // (its injured walk covers 1.9 m a cycle), crouches for .65 s before a 4.6 m/s
  // lunge, and needs 2.2 s before it can snatch again.
  {"SNATCHER",       140,1.85f,1.85f,1.1f,14,.65f,.34f,2,1.32f,true, true, 3.f, 3.3f,1.8f,1.2f,1.35f,4.6f,.3f, 2.2f,false},
  // A resonance specimen on four legs: slow, heavy, stooping through doors; a
  // long reach and a slam that lands hard and takes its time to recover.
  {"CHIMERA",        420,2.f,  2.4f, 1.6f,28,.9f, .5f, 2,.62f, true, true, 2.5f,3.14f,2.2f,1.2f,1.7f, 0.f, .35f,2.f,false}};
 return table[int(kind)];
}
// The area score a map plays; stitched maps share one so the loop carries on.
enum class MusicCue { Default, Freight, Undercover, Subliminal, PitchBlack, Heat, Darkness, LastChapter, DeathClose, Count };
enum class PickupKind { Health, Ammo }; // Values retain the existing save format.
struct ChunkDefinition {
    Vec2 origin;
    Vec2 playerStart;
    float spawnHeight;
    Environment environment;
    bool lift=false;
    float ceiling=3.4f;
    float ambient=.27f;
    int residencyGroup=-1;
    FloorMaterial floorMaterial=FloorMaterial::Industrial;
    MusicCue musicCue=MusicCue::Default;
    float spawnYaw=.08f,spawnPitch=0;
    // Off-duty openings: no weapon drawn and no viewmodel until the first punch.
    bool startUnarmed=false;
};
inline constexpr int CampaignChunkCount=179;
inline constexpr int AshfallChunkCount=15;
inline constexpr int WorldChunkCapacity=179;
// Chunks 34 on are the descent chapters (power plant to the Dark Below),
// generated from tools/campaign/plan.py; their builders live in the
// CampaignChapter*.cpp files.
// Maps 10-31 are the generated freight district. Chunks from 32 on are
// stitched extensions of earlier areas: their index is an authoring ID, and
// they sit wherever their origin places them in the world.
inline constexpr int FreightDistrictEnd=32;
inline constexpr int ReactorCoreChunk=32,CoolantLoopChunk=33;
static_assert(CampaignChunkCount<=WorldChunkCapacity&&AshfallChunkCount<=WorldChunkCapacity,"World storage must cover every authored chunk");
inline constexpr std::array<ChunkDefinition,CampaignChunkCount> CampaignChunks{{
    // Maps 0-2: brighter fill for their darker brick, rust panel and green steel finishes.
    {{0,0},{3.5f,4.5f},0,Environment::Interior,false,3.4f,.38f},
    {{18,24},{3.5f,3.5f},0,Environment::Interior,false,3.4f,.36f},
    {{36,48},{3.5f,3.5f},0,Environment::Interior,false,3.4f,.36f},
    // The Reactor Complex (3, 32, 33) streams by world position: the core and
    // coolant loop are stitched east of the lift. The Service Gallery and the
    // Coolant Return share an open seam, so they stay resident together.
    {{54,72},{3.5f,3.5f},0,Environment::Interior,true,3.4f,.27f,8},
    {{69,96},{14.5f,1.5f},-9,Environment::Interior,false,3.4f,.27f,9},
    {{69,120},{12,2},-9,Environment::Interior,false,3.4f,.27f,9},
    {{74,144},{3.5f,2},-9,Environment::Interior,false,-4.65f,.68f},
    {{92,168},{3.5f,2},-9,Environment::Interior,false,-1,.74f},
    {{109,192},{3.5f,2},-4,Environment::Interior,false,-.8f,.62f},
    {{127,216},{3.5f,2},-9,Environment::Interior,false,-5.3f,.64f},
    {{145,240},{3.5f,2},-12,Environment::Interior,false,-8.5f,.34f,0},
    {{145,264},{12,2},-12,Environment::Interior,false,1,.30f,0},
    {{145,288},{12,2},-12,Environment::Interior,false,1,.28f,0},
    {{145,312},{12,2},-19,Environment::Interior,false,-1,.30f,1},
    {{169,312},{2,12},-25,Environment::Interior,false,-1,.30f,1},
    {{169,336},{12,2},-25,Environment::Interior,false,-1,.29f,1},
    {{169,360},{12,2},-25,Environment::Interior,false,2,.32f,2},
    {{193,360},{2,12},-25,Environment::Interior,false,2,.32f,2},
    {{193,384},{12,2},-19,Environment::Interior,false,2,.32f,2},
    {{169,384},{22,12},-13,Environment::Interior,false,2,.32f,2},
    {{169,408},{12,2},-7,Environment::Interior,false,-3.4f,.48f,3},
    {{169,432},{12,2},-7,Environment::Interior,false,-3.4f,.46f,3},
    {{169,456},{12,2},-7,Environment::Interior,false,-1,.31f,4},
    {{169,480},{12,2},-25,Environment::Interior,false,-1,.30f,4},
    {{169,504},{12,2},-25,Environment::Interior,false,-1,.29f,4},
    {{169,528},{12,2},-25,Environment::Interior,false,-18,.36f,5},
    {{169,552},{12,2},-25,Environment::Interior,false,-18,.28f,5},
    {{169,576},{12,2},-25,Environment::Interior,false,-5,.32f,6},
    {{169,600},{12,2},-28,Environment::Interior,false,-5,.31f,6},
    {{169,624},{12,2},-19,Environment::Interior,false,-5,.30f,6},
    {{169,648},{12,2},-25,Environment::Interior,false,-19,.25f,7},
    {{169,672},{12,2},-25,Environment::Interior,false,-17,.23f,7},
    {{78,72},{1.5f,12.f},-9,Environment::Interior,false,4.f,.3f,8},
    {{78,48},{12.f,22.5f},-9,Environment::Interior,false,-2.f,.3f,8},
    // PLAN:CHUNKS BEGIN (tools/campaign/plan.py)
    {{169,696},{19.f,2.2f},-25.f,Environment::Interior,false,-18.f,0.22f,10,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,720},{12.f,2.2f},-25.f,Environment::Interior,false,-16.f,0.26f,10,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,720},{2.2f,11.5f},-25.f,Environment::Interior,false,-17.f,0.26f,11,FloorMaterial::Industrial,MusicCue::Subliminal,0},
    {{145,720},{21.8f,11.5f},-25.f,Environment::Interior,false,-10.f,0.26f,12,FloorMaterial::Industrial,MusicCue::Subliminal,3.142f},
    {{217,720},{2.2f,17.5f},-28.f,Environment::Interior,false,-25.2f,0.18f,11,FloorMaterial::Industrial,MusicCue::Subliminal,0},
    {{217,744},{16.f,2.2f},-28.f,Environment::Interior,false,-19.f,0.26f,13,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{145,744},{12.f,2.2f},-19.f,Environment::Interior,false,-15.5f,0.26f,12,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,744},{12.f,2.2f},-25.f,Environment::Interior,false,6.f,0.26f,14,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,744},{2.2f,12.f},-28.f,Environment::Interior,false,6.f,0.26f,14,FloorMaterial::Industrial,MusicCue::Heat,0},
    {{169,768},{12.f,2.2f},-28.f,Environment::Interior,false,6.f,0.26f,14,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,768},{12.f,2.2f},-28.f,Environment::Interior,false,6.f,0.26f,14,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,792},{16.f,2.2f},-22.f,Environment::Interior,false,-12.f,0.26f,15,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,792},{21.8f,6.f},-22.f,Environment::Interior,false,-19.4f,0.16f,15,FloorMaterial::Industrial,MusicCue::Heat,3.142f},
    {{169,816},{6.f,2.2f},-22.f,Environment::Interior,false,-15.f,0.26f,16,FloorMaterial::Industrial,MusicCue::PitchBlack,1.571f},
    {{145,816},{21.8f,16.f},-25.f,Environment::Interior,false,-20.f,0.2f,17,FloorMaterial::Industrial,MusicCue::PitchBlack,3.142f},
    {{145,840},{10.f,2.2f},-25.f,Environment::Interior,false,-16.f,0.26f,17,FloorMaterial::Industrial,MusicCue::PitchBlack,1.571f},
    {{193,816},{2.2f,10.f},-25.f,Environment::Interior,false,20.f,0.26f,18,FloorMaterial::Industrial,MusicCue::PitchBlack,0},
    {{193,840},{12.f,2.2f},-28.f,Environment::Interior,false,20.f,0.26f,18,FloorMaterial::Industrial,MusicCue::PitchBlack,1.571f},
    {{217,840},{2.2f,8.f},-19.f,Environment::Interior,false,-12.f,0.26f,19,FloorMaterial::Industrial,MusicCue::PitchBlack,0},
    {{217,864},{12.f,2.2f},-19.f,Environment::Interior,false,-15.f,0.26f,20,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{193,864},{21.8f,11.5f},-19.f,Environment::Interior,false,-12.f,0.26f,21,FloorMaterial::Industrial,MusicCue::Undercover,3.142f},
    {{217,888},{12.f,2.2f},-19.f,Environment::Interior,false,-13.f,0.26f,22,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{241,864},{2.2f,11.5f},-19.f,Environment::Interior,false,-15.5f,0.26f,20,FloorMaterial::Industrial,MusicCue::Undercover,0},
    {{217,912},{12.f,2.2f},-19.f,Environment::Interior,false,-15.5f,0.26f,23,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{217,936},{12.f,2.2f},-19.f,Environment::Interior,false,-10.f,0.26f,24,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{241,936},{2.2f,12.f},-19.f,Environment::Interior,false,-10.f,0.26f,24,FloorMaterial::Industrial,MusicCue::Darkness,0},
    {{217,960},{12.f,2.2f},-19.f,Environment::Interior,false,-12.f,0.26f,25,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,960},{21.8f,11.f},-16.f,Environment::Interior,false,-10.5f,0.26f,25,FloorMaterial::Industrial,MusicCue::Darkness,3.142f},
    {{217,984},{12.f,2.2f},-19.f,Environment::Interior,false,-13.f,0.26f,26,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,984},{21.8f,16.5f},-19.f,Environment::Interior,false,-12.f,0.26f,27,FloorMaterial::Industrial,MusicCue::Darkness,3.142f},
    {{193,1008},{10.f,2.2f},-19.f,Environment::Interior,false,-11.f,0.26f,27,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,1032},{12.f,2.2f},-16.f,Environment::Interior,false,-12.5f,0.26f,28,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,1056},{12.f,2.2f},-16.f,Environment::Interior,false,8.f,0.26f,29,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,1080},{12.f,2.2f},-16.f,Environment::Interior,false,8.f,0.26f,29,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,1104},{12.f,2.2f},-12.f,Environment::Interior,false,-8.5f,0.26f,30,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1128},{12.f,2.2f},-12.f,Environment::Interior,false,-8.8f,0.26f,30,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,1128},{21.8f,11.5f},-12.f,Environment::Interior,false,-6.f,0.26f,31,FloorMaterial::Industrial,MusicCue::Subliminal,3.142f},
    {{217,1128},{2.2f,11.5f},-12.f,Environment::Interior,false,-8.8f,0.26f,32,FloorMaterial::Industrial,MusicCue::DeathClose,0},
    {{241,1128},{2.2f,11.5f},-12.f,Environment::Interior,false,-8.8f,0.26f,32,FloorMaterial::Industrial,MusicCue::DeathClose,0},
    {{217,1152},{12.f,2.2f},-12.f,Environment::Interior,false,-9.f,0.26f,33,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{241,1152},{2.2f,11.5f},-12.f,Environment::Interior,false,-9.f,0.26f,33,FloorMaterial::Industrial,MusicCue::DeathClose,0},
    {{217,1176},{12.f,2.2f},-12.f,Environment::Interior,false,-9.f,0.26f,34,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{217,1200},{12.f,2.2f},-12.f,Environment::Interior,false,-9.f,0.26f,34,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{241,1200},{2.2f,11.5f},-12.f,Environment::Interior,false,-8.5f,0.26f,35,FloorMaterial::Industrial,MusicCue::DeathClose,0},
    {{217,1224},{12.f,2.2f},-12.f,Environment::Interior,false,-8.5f,0.26f,35,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{193,1152},{12.f,2.2f},-12.f,Environment::Interior,false,-6.f,0.26f,36,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,1152},{21.8f,11.5f},-12.f,Environment::Interior,false,-8.f,0.26f,36,FloorMaterial::Industrial,MusicCue::Subliminal,3.142f},
    {{193,1176},{12.f,2.2f},-12.f,Environment::Interior,false,-8.5f,0.26f,37,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,1176},{21.8f,11.5f},-12.f,Environment::Interior,false,-8.5f,0.26f,37,FloorMaterial::Industrial,MusicCue::Subliminal,3.142f},
    {{193,1200},{12.f,2.2f},-12.f,Environment::Interior,false,-8.5f,0.26f,38,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,1200},{8.f,2.2f},-12.f,Environment::Interior,false,-7.f,0.26f,38,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1224},{12.f,2.2f},-12.f,Environment::Interior,false,-6.f,0.26f,39,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,1224},{21.8f,11.5f},-12.f,Environment::Interior,false,4.f,0.26f,39,FloorMaterial::Industrial,MusicCue::Subliminal,3.142f},
    {{169,1248},{12.f,2.2f},-12.f,Environment::Interior,false,6.f,0.26f,40,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1248},{12.f,2.2f},-12.f,Environment::Interior,false,6.f,0.26f,40,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{169,1272},{12.f,2.2f},-12.f,Environment::Interior,false,6.f,0.26f,40,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1272},{12.f,2.2f},-12.f,Environment::Interior,false,6.f,0.26f,40,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{145,1248},{21.8f,11.5f},-12.f,Environment::Interior,false,-8.f,0.26f,41,FloorMaterial::Industrial,MusicCue::Subliminal,3.142f},
    {{145,1272},{12.f,2.2f},-12.f,Environment::Interior,false,-6.f,0.26f,41,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{145,1296},{12.f,2.2f},-16.f,Environment::Interior,false,-4.f,0.26f,42,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{217,1248},{12.f,2.2f},-15.f,Environment::Interior,false,-11.5f,0.26f,43,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{217,1272},{12.f,2.2f},-15.f,Environment::Interior,false,-8.f,0.26f,44,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{241,1272},{2.2f,11.5f},-15.f,Environment::Interior,false,-9.f,0.26f,44,FloorMaterial::Industrial,MusicCue::DeathClose,0},
    {{241,1296},{12.f,2.2f},-15.f,Environment::Interior,false,-11.6f,0.26f,45,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{217,1296},{12.f,2.2f},-15.f,Environment::Interior,false,-8.f,0.26f,45,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{217,1320},{12.f,2.2f},-15.f,Environment::Interior,false,-6.f,0.26f,46,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{241,1248},{2.2f,11.5f},-15.f,Environment::Interior,false,-11.f,0.26f,43,FloorMaterial::Industrial,MusicCue::DeathClose,0},
    {{241,1320},{12.f,2.2f},-15.f,Environment::Interior,false,-8.f,0.26f,46,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{193,1296},{21.8f,11.5f},-15.f,Environment::Interior,false,-4.f,0.26f,47,FloorMaterial::Industrial,MusicCue::DeathClose,3.142f},
    {{193,1320},{12.f,2.2f},-15.f,Environment::Interior,false,-10.f,0.26f,47,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{193,1344},{12.f,2.2f},-15.f,Environment::Interior,false,-9.f,0.26f,48,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{217,1344},{2.2f,11.5f},-15.f,Environment::Interior,false,-7.f,0.26f,48,FloorMaterial::Industrial,MusicCue::Subliminal,0},
    {{193,1368},{12.f,2.2f},-15.f,Environment::Interior,false,-10.f,0.26f,49,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1392},{12.f,2.2f},-15.f,Environment::Interior,false,5.f,0.26f,50,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1416},{12.f,2.2f},-15.f,Environment::Interior,false,-9.f,0.26f,51,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1440},{12.f,2.2f},-15.f,Environment::Interior,false,-5.f,0.26f,51,FloorMaterial::Industrial,MusicCue::Subliminal,1.571f},
    {{193,1464},{12.f,2.2f},-15.f,Environment::Interior,false,2.f,0.26f,52,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1464},{21.8f,12.f},-15.f,Environment::Interior,false,-2.f,0.26f,52,FloorMaterial::Industrial,MusicCue::Heat,3.142f},
    {{193,1488},{12.f,2.2f},-4.f,Environment::Interior,false,2.f,0.26f,53,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,1512},{12.f,2.2f},-28.f,Environment::Interior,false,-20.f,0.26f,54,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1512},{21.8f,12.f},-28.f,Environment::Interior,false,-20.f,0.26f,54,FloorMaterial::Industrial,MusicCue::Heat,3.142f},
    {{145,1512},{21.8f,12.f},-24.f,Environment::Interior,false,-10.f,0.26f,55,FloorMaterial::Industrial,MusicCue::Heat,3.142f},
    {{145,1536},{11.f,2.2f},-24.f,Environment::Interior,false,-12.f,0.26f,55,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1536},{11.f,2.2f},-28.f,Environment::Interior,false,16.f,0.26f,56,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1560},{12.f,2.2f},-28.f,Environment::Interior,false,16.f,0.26f,56,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,1536},{12.f,2.2f},-28.f,Environment::Interior,false,16.f,0.26f,57,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,1560},{12.f,2.2f},-28.f,Environment::Interior,false,-24.5f,0.26f,57,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1584},{11.f,2.2f},-20.f,Environment::Interior,false,-14.f,0.26f,58,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{145,1584},{21.8f,12.f},-20.f,Environment::Interior,false,-12.f,0.26f,58,FloorMaterial::Industrial,MusicCue::Heat,3.142f},
    {{169,1608},{12.f,2.2f},-20.f,Environment::Interior,false,-10.f,0.26f,59,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,1608},{2.2f,12.f},-20.f,Environment::Interior,false,-6.f,0.26f,60,FloorMaterial::Industrial,MusicCue::Heat,0},
    {{145,1608},{21.8f,12.f},-20.f,Environment::Interior,false,8.f,0.26f,61,FloorMaterial::Industrial,MusicCue::Heat,3.142f},
    {{145,1632},{12.f,2.2f},-24.f,Environment::Interior,false,8.f,0.26f,61,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{121,1608},{21.8f,12.f},-16.f,Environment::Interior,false,-11.f,0.26f,61,FloorMaterial::Industrial,MusicCue::Heat,3.142f},
    {{169,1632},{12.f,2.2f},-20.f,Environment::Interior,false,20.f,0.26f,62,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1656},{12.f,2.2f},-28.f,Environment::Interior,false,-16.f,0.26f,63,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{193,1656},{2.2f,12.f},-28.f,Environment::Interior,false,-25.f,0.26f,64,FloorMaterial::Industrial,MusicCue::Heat,0},
    {{193,1680},{12.f,2.2f},-28.f,Environment::Interior,false,-24.f,0.26f,64,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1680},{12.f,2.2f},-28.f,Environment::Interior,false,10.f,0.26f,65,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1704},{12.f,2.2f},-10.f,Environment::Interior,false,20.f,0.26f,66,FloorMaterial::Industrial,MusicCue::Heat,1.571f},
    {{169,1728},{12.f,2.2f},-10.f,Environment::Interior,false,12.f,0.26f,67,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,1752},{12.f,2.2f},-16.f,Environment::Interior,false,12.f,0.26f,67,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,1776},{12.f,2.2f},-28.f,Environment::Interior,false,4.f,0.26f,68,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,1776},{2.2f,12.f},-28.f,Environment::Interior,false,4.f,0.26f,68,FloorMaterial::Industrial,MusicCue::Darkness,0},
    {{193,1800},{12.f,2.2f},-24.f,Environment::Interior,false,16.f,0.26f,69,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,1824},{12.f,2.2f},-28.f,Environment::Interior,false,16.f,0.26f,69,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,1824},{21.8f,12.f},-20.f,Environment::Interior,false,20.f,0.26f,70,FloorMaterial::Industrial,MusicCue::Darkness,3.142f},
    {{145,1824},{21.8f,12.f},-28.f,Environment::Interior,false,-14.f,0.26f,71,FloorMaterial::Industrial,MusicCue::Darkness,3.142f},
    {{145,1848},{12.f,2.2f},-28.f,Environment::Interior,false,-20.f,0.26f,71,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{145,1872},{12.f,2.2f},-28.f,Environment::Interior,false,16.f,0.26f,72,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,1872},{2.2f,12.f},-20.f,Environment::Interior,false,20.f,0.26f,73,FloorMaterial::Industrial,MusicCue::Darkness,0},
    {{169,1896},{12.f,2.2f},-24.f,Environment::Interior,false,8.f,0.26f,73,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,1896},{2.2f,12.f},-24.f,Environment::Interior,false,-8.f,0.26f,74,FloorMaterial::Industrial,MusicCue::Darkness,0},
    {{193,1920},{12.f,2.2f},-24.f,Environment::Interior,false,-6.f,0.26f,74,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,1920},{12.f,2.2f},-24.f,Environment::Interior,false,4.f,0.26f,75,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,1944},{12.f,2.2f},-20.f,Environment::Interior,false,6.f,0.26f,76,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{145,1944},{21.8f,12.f},-20.f,Environment::Interior,false,6.f,0.26f,76,FloorMaterial::Industrial,MusicCue::Undercover,3.142f},
    {{169,1968},{12.f,2.2f},-20.f,Environment::Interior,false,-12.f,0.26f,77,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{145,1968},{12.f,2.2f},-20.f,Environment::Interior,false,-6.f,0.26f,77,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{193,1968},{2.2f,12.f},-20.f,Environment::Interior,false,10.f,0.26f,78,FloorMaterial::Industrial,MusicCue::Undercover,0},
    {{193,1992},{12.f,2.2f},-20.f,Environment::Interior,false,-10.f,0.26f,78,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{145,1992},{12.f,2.2f},-20.f,Environment::Interior,false,20.f,0.26f,79,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{145,2016},{12.f,2.2f},-20.f,Environment::Interior,false,-4.f,0.26f,79,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{169,1992},{12.f,2.2f},-20.f,Environment::Interior,false,16.f,0.26f,80,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{169,2016},{12.f,2.2f},-20.f,Environment::Interior,false,16.f,0.26f,80,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{169,2040},{12.f,2.2f},-20.f,Environment::Interior,false,4.f,0.26f,81,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{193,2040},{2.2f,12.f},-20.f,Environment::Interior,false,12.f,0.26f,81,FloorMaterial::Industrial,MusicCue::Undercover,0},
    {{169,2064},{12.f,2.2f},-20.f,Environment::Interior,false,12.f,0.26f,82,FloorMaterial::Industrial,MusicCue::Undercover,1.571f},
    {{169,2088},{12.f,2.2f},-20.f,Environment::Interior,false,16.f,0.26f,83,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{169,2112},{12.f,2.2f},-28.f,Environment::Interior,false,16.f,0.26f,83,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{169,2136},{12.f,2.2f},-28.f,Environment::Interior,false,-8.f,0.26f,84,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,2160},{12.f,2.2f},-28.f,Environment::Interior,false,4.f,0.26f,85,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,2160},{2.2f,12.f},-28.f,Environment::Interior,false,-6.f,0.26f,86,FloorMaterial::Industrial,MusicCue::Darkness,0},
    {{193,2184},{12.f,2.2f},-28.f,Environment::Interior,false,20.f,0.26f,86,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,2184},{12.f,2.2f},-28.f,Environment::Interior,false,12.f,0.26f,87,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,2208},{12.f,2.2f},-28.f,Environment::Interior,false,-4.f,0.26f,88,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,2232},{12.f,2.2f},-10.f,Environment::Interior,false,24.f,0.26f,89,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,2256},{12.f,2.2f},-28.f,Environment::Interior,false,20.f,0.26f,90,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{169,2280},{12.f,2.2f},-28.f,Environment::Interior,false,20.f,0.26f,90,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{169,2304},{12.f,2.2f},-28.f,Environment::Interior,false,8.f,0.26f,91,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,2328},{12.f,2.2f},-24.f,Environment::Interior,false,16.f,0.26f,92,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{193,2328},{2.2f,12.f},-24.f,Environment::Interior,false,16.f,0.26f,92,FloorMaterial::Industrial,MusicCue::Darkness,0},
    {{169,2352},{12.f,2.2f},-24.f,Environment::Interior,false,24.f,0.26f,93,FloorMaterial::Industrial,MusicCue::Darkness,1.571f},
    {{169,2376},{12.f,2.2f},-24.f,Environment::Interior,false,12.f,0.26f,94,FloorMaterial::Industrial,MusicCue::DeathClose,1.571f},
    {{169,2400},{12.f,2.2f},-24.f,Environment::Interior,false,24.f,0.26f,95,FloorMaterial::Industrial,MusicCue::LastChapter,1.571f},
    // PLAN:CHUNKS END
}};
inline constexpr std::array<const char*,CampaignChunkCount> CampaignMapNames{{
 "FOUNDRY","PRESSURE WORKS","TURBINE GANTRY","REACTOR COMPLEX","SERVICE GALLERY","COOLANT RETURN","CABLE VAULTS","PUMP ANNEX","UTILITY JUNCTION","WASTE HANDLING",
 "FREIGHT ACCESS","LIFT MACHINERY","AUXILIARY FREIGHT LIFT","RECEIVING OVERLOOK","RECEIVING LANES","WAREHOUSE INTAKE",
 "WAREHOUSE / A-B","WAREHOUSE / C BLOCK","WAREHOUSE / D BLOCK","WAREHOUSE / UPPER RACKS","MANIFEST OFFICES","MANIFEST ROUTING",
 "TRANSFER / CONVEYORS","TRANSFER / SORTING","TRANSFER / RAIL YARD","EMPTY PLATFORM","PLATFORM / SERVICE END",
 "DEPOT / TRACKSIDE","DEPOT / INSPECTION PITS","DEPOT / GANTRIES","CARGO / MAINTENANCE BORE","CARGO / DEEP JUNCTION",
 "REACTOR CORE","COOLANT LOOP",
    // PLAN:NAMES BEGIN (tools/campaign/plan.py)
 "POWER DISTRIBUTION","DISTRIBUTION / HUB","SUBSTATION A","SWITCH HALL","CABLE TRENCH","EMERGENCY GENERATION",
 "LOAD CONTROL","MACHINE HALL / NORTH GALLERY","MACHINE HALL / EXCITER BAY","MACHINE HALL / TURBINE FLOOR","MACHINE HALL / CRANE END","VENTILATION WORKS",
 "AIR HANDLING","WATER TREATMENT","DRAINAGE GALLERY","SUMP WORKS","COOLING TOWERS / BASIN","COOLING TOWERS / SHELL",
 "HEAT EXCHANGE","MAINTENANCE DISTRICT","CENTRAL STORES","FABRICATION SHOP","MAINTENANCE / AID STATION","OPERATIONS CORRIDOR",
 "SECURITY PERIMETER / YARD","SECURITY PERIMETER / GATEHOUSE","CHECKPOINT DELTA","ADMINISTRATIVE LINK","PERSONNEL TRANSIT","RESTRICTED UTILITIES",
 "RESEARCH RECEIVING","DECONTAMINATION","ENERGY RESEARCH / FORECOURT","ENERGY RESEARCH / GREAT DOOR","RESEARCH INTAKE","PERSONNEL PROCESSING",
 "RESEARCH ADMINISTRATION","MEDICAL SCREENING","MEDICAL / TRIAGE","MEDICAL / IMAGING","MEDICAL / PHARMACY","MEDICAL / WARD C NORTH",
 "MEDICAL / WARD C SOUTH","MEDICAL / SURGERY","MEDICAL / MORGUE","MATERIALS INTAKE","SAMPLE STORAGE","ANALYTICAL CHEMISTRY",
 "INSTRUMENTATION","ENERGY DIAGNOSTICS / LAB 4","FIELD MEASUREMENT","RESONANCE LABORATORY","HIGH-ENERGY TEST BAY","OBSERVATION RING / NORTH-WEST",
 "OBSERVATION RING / NORTH-EAST","OBSERVATION RING / SOUTH-WEST","OBSERVATION RING / SOUTH-EAST","SHIELDING CORRIDOR","EXPERIMENTAL POWER","CONVERSION LAB",
 "QUARANTINE WARD","CONTAINMENT INTAKE","CONTAINMENT A","CONTAINMENT SERVICE","CONTAINMENT B","SPECIMEN HANDLING",
 "ISOLATION WING","OBSERVATION THEATER","BREACH SECTOR","EMERGENCY CONTAINMENT","DEEP MATERIALS LAB","BORE SAMPLE ARCHIVE",
 "GEOPHYSICS","ENERGY CORE RESEARCH","DEEP ACCESS CONTROL","EXTRACTION INTERFACE","MINE ACCESS","EQUIPMENT DEPOT",
 "PERSONNEL DESCENT","UPPER MINE","ORE HAULAGE","CRUSHER STATION","SCREENING PLANT","CONVEYOR CANYON / UPPER",
 "CONVEYOR CANYON / LOWER","VENTILATION SHAFT","MAINTENANCE DRIFT","LOWER MINE","PUMP STATION DEEP","SHAFT JUNCTION",
 "BORING WORKS","DRILL ASSEMBLY / GALLERY","DRILL ASSEMBLY / HEAD","DRILL CONTROL","DEEP BORE","EXTRACTION FACE",
 "SURVEY DRIFT","ABANDONED HEADING","BREACH SHAFT","THRESHOLD","THE BREACH","DESCENT",
 "BLACKWATER / SHORE","BLACKWATER / LAKE","BROKEN SHELF / UPPER","BROKEN SHELF / LOWER","HANGING CAVERNS","NESTING GROUNDS",
 "BONE PASSAGE","SINK","IRON FALL","SCRAP FIELDS","WRECK HOLLOW","THE TANGLE",
 "SCRAP GATE","OUTER SHANTIES / GATE ROW","OUTER SHANTIES / LAMP STREET","LOWER MARKET","FURNACE ROW","CABLE DISTRICT",
 "FLOODED QUARTER","SCRAP TOWERS","FOUNDRY BELOW","CENTRAL SPAN / NORTH","CENTRAL SPAN / SOUTH","INNER CITY",
 "MACHINE SHRINE","PIT DISTRICT","BOSS ARENA I / RIM","BOSS ARENA I / PIT","AFTERMATH","DEEP WILDS",
 "CRYSTAL GALLERIES","THE SCAR","HOLLOW FOREST","BURIED MACHINERY","THE MAW","BOSS ARENA II / FUNNEL",
 "BOSS ARENA II / FLOOR","LOWER DEPTHS","OLD CITY / CAUSEWAY","OLD CITY / TEMPLE","THE DEEP GATE","DEEP GATE / APPROACH",
 "THE DARK HEART",
    // PLAN:NAMES END
}};
// The campaign chunk whose 24 m square contains a world-space point, or -1.
inline int campaignChunkAt(Vec2 global){
 for(int i=0;i<CampaignChunkCount;++i){auto o=CampaignChunks[size_t(i)].origin;
  if(global.x>=o.x&&global.x<o.x+24&&global.y>=o.y&&global.y<o.y+24)return i;}
 return -1;
}
// Preserve the original 4 x 3 chunk IDs for old saves. Three new eastern
// chunks extend the same stitched world into a 120 x 72 m coastline.
inline constexpr std::array<ChunkDefinition,AshfallChunkCount> AshfallChunks{{
    {{0,0},{3.5f,4.5f},0,Environment::Outdoor},
    {{24,0},{12.f,12.f},0,Environment::Outdoor},
    {{48,0},{12.f,12.f},0,Environment::Outdoor},
    {{72,0},{12.f,12.f},0,Environment::Outdoor},
    {{0,24},{12.f,12.f},0,Environment::Outdoor},
    {{24,24},{12.f,12.f},0,Environment::Outdoor},
    {{48,24},{12.f,12.f},0,Environment::Outdoor},
    {{72,24},{12.f,12.f},0,Environment::Outdoor},
    {{0,48},{12.f,12.f},0,Environment::Outdoor},
    {{24,48},{12.f,12.f},0,Environment::Outdoor},
    {{48,48},{12.f,12.f},0,Environment::Outdoor},
    {{72,48},{12.f,12.f},0,Environment::Outdoor},
    {{96,0},{3.5f,12.f},0,Environment::Outdoor},
    {{96,24},{3.5f,12.f},0,Environment::Outdoor},
    {{96,48},{3.5f,12.f},0,Environment::Outdoor}
}};
inline constexpr int worldChunkCount(WorldId world){return world==WorldId::Campaign?CampaignChunkCount:world==WorldId::Ashfall?AshfallChunkCount:0;}
inline const ChunkDefinition& chunkDefinition(WorldId world,int chunk){
    if(world==WorldId::Campaign)return CampaignChunks.at(std::size_t(chunk));
    if(world==WorldId::Ashfall)return AshfallChunks.at(std::size_t(chunk));
    throw std::out_of_range("Custom worlds use runtime chunk definitions");
}
}

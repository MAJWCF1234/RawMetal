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
inline constexpr int FacilityModelCount=24;
enum class CreatureKind { Huntsman, Wasp, Brute, Warden, Mutant };
enum class MusicCue { Default, Freight };
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
};
inline constexpr int CampaignChunkCount=32;
inline constexpr int AshfallChunkCount=15;
inline constexpr int WorldChunkCapacity=32;
static_assert(CampaignChunkCount<=WorldChunkCapacity&&AshfallChunkCount<=WorldChunkCapacity,"World storage must cover every authored chunk");
inline constexpr std::array<ChunkDefinition,CampaignChunkCount> CampaignChunks{{
    {{0,0},{3.5f,4.5f},0,Environment::Interior},
    {{18,24},{3.5f,3.5f},0,Environment::Interior},
    {{36,48},{3.5f,3.5f},0,Environment::Interior},
    {{54,72},{3.5f,3.5f},0,Environment::Interior,true},
    {{69,96},{6.5f,1.5f},-9,Environment::Interior},
    {{69,120},{12,2},-9,Environment::Interior},
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
    {{169,672},{12,2},-25,Environment::Interior,false,-17,.23f,7}
}};
inline constexpr std::array<const char*,CampaignChunkCount> CampaignMapNames{{
 "FOUNDRY","PRESSURE WORKS","TURBINE GANTRY","REACTOR COMPLEX","SERVICE GALLERY","COOLANT RETURN","CABLE VAULTS","PUMP ANNEX","UTILITY JUNCTION","WASTE HANDLING",
 "FREIGHT ACCESS","LIFT MACHINERY","AUXILIARY FREIGHT LIFT","RECEIVING OVERLOOK","RECEIVING LANES","WAREHOUSE INTAKE",
 "WAREHOUSE / A-B","WAREHOUSE / C BLOCK","WAREHOUSE / D BLOCK","WAREHOUSE / UPPER RACKS","MANIFEST OFFICES","MANIFEST ROUTING",
 "TRANSFER / CONVEYORS","TRANSFER / SORTING","TRANSFER / RAIL YARD","EMPTY PLATFORM","PLATFORM / SERVICE END",
 "DEPOT / TRACKSIDE","DEPOT / INSPECTION PITS","DEPOT / GANTRIES","CARGO / MAINTENANCE BORE","CARGO / DEEP JUNCTION"
}};
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

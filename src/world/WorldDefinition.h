#pragma once
#include "../core/Math.h"
#include <array>

namespace retro {
// A chunk index is local to a world. Never use it as a global content identity.
enum class WorldId { Campaign, Ashfall };
enum class Environment { Interior, Outdoor };
enum class CreatureKind { Huntsman, Wasp, Brute, Warden };
enum class PickupKind { Health, Ammo }; // Values retain the existing save format.
struct ChunkDefinition {
    Vec2 origin;
    Vec2 playerStart;
    float spawnHeight;
    Environment environment;
    bool lift=false;
};
inline constexpr int CampaignChunkCount=6;
inline constexpr int AshfallChunkCount=12;
inline constexpr int WorldChunkCapacity=12;
inline constexpr std::array<ChunkDefinition,CampaignChunkCount> CampaignChunks{{
    {{0,0},{3.5f,4.5f},0,Environment::Interior},
    {{18,24},{3.5f,3.5f},0,Environment::Interior},
    {{36,48},{3.5f,3.5f},0,Environment::Interior},
    {{54,72},{3.5f,3.5f},0,Environment::Interior,true},
    {{69,96},{6.5f,1.5f},-9,Environment::Interior},
    {{69,120},{12,2},-9,Environment::Interior}
}};
// Ashfall is a stitched 4 x 3 wasteland. Keeping 24 m chunks preserves the
// proven service-map streaming granularity while allowing the surface world to
// grow into a sparse 96 x 72 m landscape.
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
    {{72,48},{12.f,12.f},0,Environment::Outdoor}
}};
inline constexpr int worldChunkCount(WorldId world){return world==WorldId::Campaign?CampaignChunkCount:AshfallChunkCount;}
inline const ChunkDefinition& chunkDefinition(WorldId world,int chunk){
    return world==WorldId::Campaign?CampaignChunks.at(size_t(chunk)):AshfallChunks.at(size_t(chunk));
}
}

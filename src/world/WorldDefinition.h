#pragma once
#include "../core/Math.h"
#include <array>
#include <cstddef>
#include <stdexcept>

namespace retro {
// A chunk index is local to a world. Never use it as a global content identity.
enum class WorldId { Campaign, Ashfall, Custom };
enum class Environment { Interior, Outdoor };
enum class CreatureKind { Huntsman, Wasp, Brute, Warden };
enum class PickupKind { Health, Ammo }; // Values retain the existing save format.
struct ChunkDefinition {
    Vec2 origin;
    Vec2 playerStart;
    float spawnHeight;
    Environment environment;
    bool lift=false;
    float ceiling=3.4f;
    float ambient=.27f;
};
inline constexpr int CampaignChunkCount=10;
inline constexpr int AshfallChunkCount=15;
inline constexpr int WorldChunkCapacity=32;
inline constexpr std::array<ChunkDefinition,CampaignChunkCount> CampaignChunks{{
    {{0,0},{3.5f,4.5f},0,Environment::Interior},
    {{18,24},{3.5f,3.5f},0,Environment::Interior},
    {{36,48},{3.5f,3.5f},0,Environment::Interior},
    {{54,72},{3.5f,3.5f},0,Environment::Interior,true},
    {{69,96},{6.5f,1.5f},-9,Environment::Interior},
    {{69,120},{12,2},-9,Environment::Interior},
    {{74,144},{3.5f,2},-9,Environment::Interior,false,-6.35f,.09f},
    {{92,168},{3.5f,2},-9,Environment::Interior,false,-1},
    {{109,192},{3.5f,2},-4,Environment::Interior,false,-.8f},
    {{127,216},{3.5f,2},-9,Environment::Interior,false,-5.3f}
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

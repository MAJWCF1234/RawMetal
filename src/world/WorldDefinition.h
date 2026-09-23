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
inline constexpr std::array<ChunkDefinition,6> CampaignChunks{{
    {{0,0},{3.5f,4.5f},0,Environment::Interior},
    {{18,24},{3.5f,3.5f},0,Environment::Interior},
    {{36,48},{3.5f,3.5f},0,Environment::Interior},
    {{54,72},{3.5f,3.5f},0,Environment::Interior,true},
    {{69,96},{6.5f,1.5f},-9,Environment::Interior},
    {{69,120},{12,2},-9,Environment::Interior}
}};
// Ashfall is a stitched 3 x 2 outdoor world. Chunk origins are real world-space
// coordinates, so the same streaming/renderer path can grow to larger outdoor
// grids later without treating the map index as a direction.
inline constexpr std::array<ChunkDefinition,6> AshfallChunks{{
    {{0,0},{3.5f,4.5f},0,Environment::Outdoor},
    {{24,0},{1.5f,12.f},0,Environment::Outdoor},
    {{48,0},{1.5f,12.f},0,Environment::Outdoor},
    {{0,24},{12.f,1.5f},0,Environment::Outdoor},
    {{24,24},{12.f,1.5f},0,Environment::Outdoor},
    {{48,24},{12.f,1.5f},0,Environment::Outdoor}
}};
inline const ChunkDefinition& chunkDefinition(WorldId world,int chunk){
    return (world==WorldId::Campaign?CampaignChunks:AshfallChunks).at(chunk);
}
}

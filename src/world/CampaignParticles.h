#pragma once
#include "World.h"
#include <array>
#include <memory>
namespace retro {
// Chapters 1 and 2 (chunks 34-109) get their air after the chapter files have built the maps, the way
// CampaignSoundscape gives Chapter 2 its sound: EMITTER records, the same a custom map writes
// (CampaignParticles.cpp). The Foundry (chunk 0) gets its smoke and dust too.
inline constexpr int ChapterOneAirFirst=34,ChapterTwoAirLast=109,MostDressedEmitters=12;
void dressCampaignAir(std::array<std::shared_ptr<const AuthoredMapData>,CampaignChunkCount>& maps);
// How many EMITTER records the dressing gave a chunk's map (0 for one it never dressed).
int dressedAirCount(int chunk);
}

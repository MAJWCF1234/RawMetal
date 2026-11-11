#pragma once
#include "World.h"
#include <array>
#include <memory>
namespace retro {
// Chapter 2 (the research levels, chunks 68-109) gets its sound after the
// chapter files have built the maps: AMBIENT emitters and REVERB, the same data
// a custom map writes (CampaignSoundscape.cpp).
inline constexpr int ChapterTwoSoundFirst=68,ChapterTwoSoundLast=109;
void dressChapterTwoSound(std::array<std::shared_ptr<const AuthoredMapData>,CampaignChunkCount>& maps);
}

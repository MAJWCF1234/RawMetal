#pragma once
// Chapter files for campaign chunks 34 onward (tools/campaign/plan.py).
#include "World.h"
#include <array>
#include <memory>
namespace retro {
using CampaignMapTable=std::array<std::shared_ptr<const AuthoredMapData>,CampaignChunkCount>;
void buildChapterPower(CampaignMapTable& maps);
void buildChapterMachineHall(CampaignMapTable& maps);
void buildChapterWater(CampaignMapTable& maps);
void buildChapterMaintenance(CampaignMapTable& maps);
void buildChapterPerimeterA(CampaignMapTable& maps);
void buildChapterPerimeterB(CampaignMapTable& maps);
void buildChapterResearchFront(CampaignMapTable& maps);
void buildChapterMedicalFront(CampaignMapTable& maps);
void buildChapterMedicalWard(CampaignMapTable& maps);
void buildChapterAnalysis(CampaignMapTable& maps);
void buildChapterEnergyTestLabs(CampaignMapTable& maps);
void buildChapterObservationRing(CampaignMapTable& maps);
void buildChapterContainmentA(CampaignMapTable& maps);
void buildChapterContainmentB(CampaignMapTable& maps);
void buildChapterDeepResearch(CampaignMapTable& maps);
void buildChapterMinehead(CampaignMapTable& maps);
void buildChapterUpperMineA(CampaignMapTable& maps);
void buildChapterUpperMineB(CampaignMapTable& maps);
void buildChapterLowerMine(CampaignMapTable& maps);
void buildChapterDeepBore(CampaignMapTable& maps);
void buildChapterWildsA(CampaignMapTable& maps);
void buildChapterWildsB(CampaignMapTable& maps);
void buildChapterScrap(CampaignMapTable& maps);
void buildChapterHavenA(CampaignMapTable& maps);
void buildChapterHavenB(CampaignMapTable& maps);
void buildChapterFinaleA(CampaignMapTable& maps);
void buildChapterFinaleB(CampaignMapTable& maps);
void buildChapterFinaleC(CampaignMapTable& maps);
// Every chapter, in plan order.
void buildCampaignChapters(CampaignMapTable& maps);
}

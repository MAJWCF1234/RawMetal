// THE RESTRICTED PERIMETER (chapter 1, manifest 36-44): chunks 63, 64, 65, 66, 67. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// RESTRICTED UTILITIES (manifest 41)
std::shared_ptr<const Map> makeRestrictedUtilities(){return placeholderChunk(63,{{'E',14.f,19.f,-19.f,false,0},{'S',8.f,12.f,-19.f,false,0}});}
// RESEARCH RECEIVING (manifest 42)
std::shared_ptr<const Map> makeResearchReceiving(){return placeholderChunk(64,{{'N',8.f,12.f,-19.f,false,0},{'S',10.f,14.f,-16.f,false,0}});}
// DECONTAMINATION (manifest 43)
std::shared_ptr<const Map> makeDecontamination(){return placeholderChunk(65,{{'N',10.f,14.f,-16.f,false,0},{'S',10.f,14.f,-16.f,true,stateId("decon_cycle")}});}
// ENERGY RESEARCH / FORECOURT (manifest 44)
std::shared_ptr<const Map> makeEnergyResearchForecourt(){return placeholderChunk(66,{{'N',10.f,14.f,-16.f,true,0},{'S',2.f,22.f,-16.f,false,0}});}
// ENERGY RESEARCH / GREAT DOOR (manifest 44)
std::shared_ptr<const Map> makeEnergyResearchGreatDoor(){return placeholderChunk(67,{{'N',2.f,22.f,-16.f,false,0},{'S',9.f,15.f,-12.f,true,stateId("ere_great_door")}});}
}
void buildChapterPerimeterB(CampaignMapTable& maps){
 maps[63]=makeRestrictedUtilities();
 maps[64]=makeResearchReceiving();
 maps[65]=makeDecontamination();
 maps[66]=makeEnergyResearchForecourt();
 maps[67]=makeEnergyResearchGreatDoor();
}
}

// THE DEEP (chapter 4, manifest 123-136): chunks 174, 175, 176, 177, 178. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// OLD CITY / CAUSEWAY (manifest 133)
std::shared_ptr<const Map> makeOldCityCauseway(){return placeholderChunk(174,{{'N',8.f,16.f,-24.f,false,0},{'E',8.f,16.f,-24.f,false,0},{'S',9.f,15.f,-24.f,true,stateId("oldcity_seal")}});}
// OLD CITY / TEMPLE (manifest 133)
std::shared_ptr<const Map> makeOldCityTemple(){return placeholderChunk(175,{{'W',8.f,16.f,-24.f,false,0}});}
// THE DEEP GATE (manifest 134)
std::shared_ptr<const Map> makeTheDeepGate(){return placeholderChunk(176,{{'N',9.f,15.f,-24.f,true,0},{'S',6.f,18.f,-24.f,false,0}});}
// DEEP GATE / APPROACH (manifest 135)
std::shared_ptr<const Map> makeDeepGateApproach(){return placeholderChunk(177,{{'N',6.f,18.f,-24.f,false,0},{'S',9.f,15.f,-24.f,true,stateId("boss3_dead")}});}
// THE DARK HEART (manifest 136)
std::shared_ptr<const Map> makeTheDarkHeart(){return placeholderChunk(178,{{'N',9.f,15.f,-24.f,true,0}});}
}
void buildChapterFinaleC(CampaignMapTable& maps){
 maps[174]=makeOldCityCauseway();
 maps[175]=makeOldCityTemple();
 maps[176]=makeTheDeepGate();
 maps[177]=makeDeepGateApproach();
 maps[178]=makeTheDarkHeart();
}
}

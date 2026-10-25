// THE SCRAP CITY (chapter 4, manifest 112-122): chunks 156, 157, 158, 159, 160, 161. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// FOUNDRY BELOW (manifest 118)
std::shared_ptr<const Map> makeFoundryBelow(){return placeholderChunk(156,{{'N',8.f,16.f,-20.f,false,0},{'E',8.f,16.f,-20.f,false,0}});}
// CENTRAL SPAN / NORTH (manifest 119)
std::shared_ptr<const Map> makeCentralSpanNorth(){return placeholderChunk(157,{{'N',8.f,16.f,-20.f,false,0},{'E',8.f,16.f,-20.f,false,0},{'W',8.f,16.f,-20.f,false,0},{'S',8.f,16.f,-20.f,false,0}});}
// CENTRAL SPAN / SOUTH (manifest 119)
std::shared_ptr<const Map> makeCentralSpanSouth(){return placeholderChunk(158,{{'N',8.f,16.f,-20.f,false,0},{'W',8.f,16.f,-20.f,false,0},{'S',8.f,16.f,-20.f,false,0}});}
// INNER CITY (manifest 120)
std::shared_ptr<const Map> makeInnerCity(){return placeholderChunk(159,{{'N',8.f,16.f,-20.f,false,0},{'E',8.f,16.f,-20.f,false,0},{'S',9.f,15.f,-20.f,true,stateId("haven_pit")}});}
// MACHINE SHRINE (manifest 121)
std::shared_ptr<const Map> makeMachineShrine(){return placeholderChunk(160,{{'W',8.f,16.f,-20.f,false,0}});}
// PIT DISTRICT (manifest 122)
std::shared_ptr<const Map> makePitDistrict(){return placeholderChunk(161,{{'N',9.f,15.f,-20.f,true,0},{'S',8.f,16.f,-20.f,false,0}});}
}
void buildChapterHavenB(CampaignMapTable& maps){
 maps[156]=makeFoundryBelow();
 maps[157]=makeCentralSpanNorth();
 maps[158]=makeCentralSpanSouth();
 maps[159]=makeInnerCity();
 maps[160]=makeMachineShrine();
 maps[161]=makePitDistrict();
}
}

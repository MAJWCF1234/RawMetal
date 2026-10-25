// THE SCRAP FIELDS (chapter 4, manifest 107-111): chunks 144, 145, 146, 147, 148. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// IRON FALL (manifest 107)
std::shared_ptr<const Map> makeIronFall(){return placeholderChunk(144,{{'W',8.f,16.f,-20.f,false,0},{'S',6.f,18.f,-24.f,false,0}});}
// SCRAP FIELDS (manifest 108)
std::shared_ptr<const Map> makeScrapFields(){return placeholderChunk(145,{{'N',6.f,18.f,-24.f,false,0},{'E',8.f,16.f,-24.f,false,0},{'S',6.f,18.f,-24.f,false,0}});}
// WRECK HOLLOW (manifest 109)
std::shared_ptr<const Map> makeWreckHollow(){return placeholderChunk(146,{{'W',8.f,16.f,-24.f,false,0},{'S',6.f,18.f,-24.f,false,0}});}
// THE TANGLE (manifest 110)
std::shared_ptr<const Map> makeTheTangle(){return placeholderChunk(147,{{'N',6.f,18.f,-24.f,false,0},{'W',8.f,16.f,-20.f,false,0}});}
// SCRAP GATE (manifest 111)
std::shared_ptr<const Map> makeScrapGate(){return placeholderChunk(148,{{'N',6.f,18.f,-24.f,false,0},{'E',8.f,16.f,-20.f,false,0},{'S',9.f,15.f,-20.f,true,stateId("haven_gate")}});}
}
void buildChapterScrap(CampaignMapTable& maps){
 maps[144]=makeIronFall();
 maps[145]=makeScrapFields();
 maps[146]=makeWreckHollow();
 maps[147]=makeTheTangle();
 maps[148]=makeScrapGate();
}
}

// THE SCRAP CITY (chapter 4, manifest 112-122): chunks 149, 150, 151, 152, 153, 154, 155. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// OUTER SHANTIES / GATE ROW (manifest 112)
std::shared_ptr<const Map> makeOuterShantiesGateRow(){return placeholderChunk(149,{{'N',9.f,15.f,-20.f,true,0},{'W',4.f,20.f,-20.f,false,0},{'S',6.f,18.f,-20.f,false,0}});}
// OUTER SHANTIES / LAMP STREET (manifest 112)
std::shared_ptr<const Map> makeOuterShantiesLampStreet(){return placeholderChunk(150,{{'E',4.f,20.f,-20.f,false,0},{'S',6.f,18.f,-20.f,false,0}});}
// LOWER MARKET (manifest 113)
std::shared_ptr<const Map> makeLowerMarket(){return placeholderChunk(151,{{'N',6.f,18.f,-20.f,false,0},{'W',8.f,16.f,-20.f,false,0},{'E',8.f,16.f,-20.f,false,0},{'S',8.f,16.f,-20.f,false,0}});}
// FURNACE ROW (manifest 114)
std::shared_ptr<const Map> makeFurnaceRow(){return placeholderChunk(152,{{'N',6.f,18.f,-20.f,false,0},{'E',8.f,16.f,-20.f,false,0},{'S',8.f,16.f,-20.f,false,0}});}
// CABLE DISTRICT (manifest 115)
std::shared_ptr<const Map> makeCableDistrict(){return placeholderChunk(153,{{'W',8.f,16.f,-20.f,false,0},{'S',6.f,18.f,-20.f,false,0}});}
// FLOODED QUARTER (manifest 116)
std::shared_ptr<const Map> makeFloodedQuarter(){return placeholderChunk(154,{{'N',6.f,18.f,-20.f,false,0},{'W',8.f,16.f,-20.f,false,0}});}
// SCRAP TOWERS (manifest 117)
std::shared_ptr<const Map> makeScrapTowers(){return placeholderChunk(155,{{'N',8.f,16.f,-20.f,false,0},{'E',8.f,16.f,-20.f,false,0},{'S',8.f,16.f,-20.f,false,0}});}
}
void buildChapterHavenA(CampaignMapTable& maps){
 maps[149]=makeOuterShantiesGateRow();
 maps[150]=makeOuterShantiesLampStreet();
 maps[151]=makeLowerMarket();
 maps[152]=makeFurnaceRow();
 maps[153]=makeCableDistrict();
 maps[154]=makeFloodedQuarter();
 maps[155]=makeScrapTowers();
}
}

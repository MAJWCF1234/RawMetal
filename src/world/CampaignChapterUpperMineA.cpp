// UPPER MINE (chapter 3, manifest 80-86): chunks 113, 114, 115, 116. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// UPPER MINE (manifest 80)
std::shared_ptr<const Map> makeUpperMine(){return placeholderChunk(113,{{'N',9.f,15.f,-28.f,false,0},{'W',9.f,15.f,-28.f,false,0},{'S',9.f,15.f,-28.f,false,0}});}
// ORE HAULAGE (manifest 81)
std::shared_ptr<const Map> makeOreHaulage(){return placeholderChunk(114,{{'E',9.f,15.f,-28.f,false,0},{'W',9.f,15.f,-24.f,false,0},{'S',8.f,14.f,-28.f,false,0}});}
// CRUSHER STATION (manifest 82)
std::shared_ptr<const Map> makeCrusherStation(){return placeholderChunk(115,{{'E',9.f,15.f,-24.f,false,0},{'S',8.f,14.f,-24.f,false,0}});}
// SCREENING PLANT (manifest 83)
std::shared_ptr<const Map> makeScreeningPlant(){return placeholderChunk(116,{{'N',8.f,14.f,-24.f,false,0},{'E',9.f,15.f,-20.f,false,0}});}
}
void buildChapterUpperMineA(CampaignMapTable& maps){
 maps[113]=makeUpperMine();
 maps[114]=makeOreHaulage();
 maps[115]=makeCrusherStation();
 maps[116]=makeScreeningPlant();
}
}

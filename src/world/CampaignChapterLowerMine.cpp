// LOWER MINE (chapter 3, manifest 87-92): chunks 121, 122, 123, 124, 125, 126, 127. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// LOWER MINE (manifest 87)
std::shared_ptr<const Map> makeLowerMine(){return placeholderChunk(121,{{'N',8.f,14.f,-20.f,false,0},{'W',9.f,15.f,-20.f,false,0},{'S',9.f,15.f,-20.f,false,0}});}
// PUMP STATION DEEP (manifest 88)
std::shared_ptr<const Map> makePumpStationDeep(){return placeholderChunk(122,{{'E',9.f,15.f,-20.f,false,0}});}
// SHAFT JUNCTION (manifest 89)
std::shared_ptr<const Map> makeShaftJunction(){return placeholderChunk(123,{{'N',9.f,15.f,-20.f,false,0},{'E',9.f,15.f,-20.f,false,0},{'W',9.f,15.f,-20.f,false,0},{'S',9.f,15.f,-20.f,true,stateId("mine_drill_power")}});}
// BORING WORKS (manifest 90)
std::shared_ptr<const Map> makeBoringWorks(){return placeholderChunk(124,{{'W',9.f,15.f,-20.f,false,0}});}
// DRILL ASSEMBLY / GALLERY (manifest 91)
std::shared_ptr<const Map> makeDrillAssemblyGallery(){return placeholderChunk(125,{{'E',9.f,15.f,-20.f,false,0},{'S',1.f,23.f,-24.f,false,0},{'W',9.f,15.f,-16.f,false,0}});}
// DRILL ASSEMBLY / HEAD (manifest 91)
std::shared_ptr<const Map> makeDrillAssemblyHead(){return placeholderChunk(126,{{'N',1.f,23.f,-24.f,false,0}});}
// DRILL CONTROL (manifest 92)
std::shared_ptr<const Map> makeDrillControl(){return placeholderChunk(127,{{'E',9.f,15.f,-16.f,false,0}});}
}
void buildChapterLowerMine(CampaignMapTable& maps){
 maps[121]=makeLowerMine();
 maps[122]=makePumpStationDeep();
 maps[123]=makeShaftJunction();
 maps[124]=makeBoringWorks();
 maps[125]=makeDrillAssemblyGallery();
 maps[126]=makeDrillAssemblyHead();
 maps[127]=makeDrillControl();
}
}

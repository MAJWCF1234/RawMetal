// THE WILD CAVERNS (chapter 4, manifest 99-106): chunks 140, 141, 142, 143. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// HANGING CAVERNS (manifest 103)
std::shared_ptr<const Map> makeHangingCaverns(){return placeholderChunk(140,{{'E',8.f,16.f,-20.f,false,0},{'W',8.f,16.f,-28.f,false,0}});}
// NESTING GROUNDS (manifest 104)
std::shared_ptr<const Map> makeNestingGrounds(){return placeholderChunk(141,{{'E',8.f,16.f,-28.f,false,0},{'S',8.f,16.f,-28.f,false,0}});}
// BONE PASSAGE (manifest 105)
std::shared_ptr<const Map> makeBonePassage(){return placeholderChunk(142,{{'N',8.f,16.f,-28.f,false,0},{'S',8.f,16.f,-28.f,false,0}});}
// SINK (manifest 106)
std::shared_ptr<const Map> makeSink(){return placeholderChunk(143,{{'N',8.f,16.f,-28.f,false,0},{'E',8.f,16.f,-20.f,false,0}});}
}
void buildChapterWildsB(CampaignMapTable& maps){
 maps[140]=makeHangingCaverns();
 maps[141]=makeNestingGrounds();
 maps[142]=makeBonePassage();
 maps[143]=makeSink();
}
}

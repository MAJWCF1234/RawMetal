// THE DEEP (chapter 4, manifest 123-136): chunks 162, 163, 164, 165, 166, 167. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// BOSS ARENA I / RIM (manifest 123)
std::shared_ptr<const Map> makeBossArenaIRim(){return placeholderChunk(162,{{'N',8.f,16.f,-20.f,false,0},{'S',1.f,23.f,-28.f,false,0}});}
// BOSS ARENA I / PIT (manifest 123)
std::shared_ptr<const Map> makeBossArenaIPit(){return placeholderChunk(163,{{'N',1.f,23.f,-28.f,false,0},{'S',9.f,15.f,-28.f,true,stateId("boss1_dead")}});}
// AFTERMATH (manifest 124)
std::shared_ptr<const Map> makeAftermath(){return placeholderChunk(164,{{'N',9.f,15.f,-28.f,true,0},{'S',8.f,16.f,-28.f,false,0}});}
// DEEP WILDS (manifest 125)
std::shared_ptr<const Map> makeDeepWilds(){return placeholderChunk(165,{{'N',8.f,16.f,-28.f,false,0},{'E',8.f,16.f,-28.f,false,0},{'S',8.f,16.f,-28.f,false,0}});}
// CRYSTAL GALLERIES (manifest 126)
std::shared_ptr<const Map> makeCrystalGalleries(){return placeholderChunk(166,{{'W',8.f,16.f,-28.f,false,0},{'S',8.f,16.f,-28.f,false,0}});}
// THE SCAR (manifest 127)
std::shared_ptr<const Map> makeTheScar(){return placeholderChunk(167,{{'N',8.f,16.f,-28.f,false,0},{'W',8.f,16.f,-24.f,false,0}});}
}
void buildChapterFinaleA(CampaignMapTable& maps){
 maps[162]=makeBossArenaIRim();
 maps[163]=makeBossArenaIPit();
 maps[164]=makeAftermath();
 maps[165]=makeDeepWilds();
 maps[166]=makeCrystalGalleries();
 maps[167]=makeTheScar();
}
}

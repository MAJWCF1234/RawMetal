// THE WILD CAVERNS (chapter 4, manifest 99-106): chunks 134, 135, 136, 137, 138, 139. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// THE BREACH (manifest 99)
std::shared_ptr<const Map> makeTheBreach(){return placeholderChunk(134,{{'N',8.f,16.f,-10.f,false,0},{'S',6.f,18.f,-16.f,false,0}});}
// DESCENT (manifest 100)
std::shared_ptr<const Map> makeDescent(){return placeholderChunk(135,{{'N',6.f,18.f,-16.f,false,0},{'S',6.f,18.f,-28.f,false,0}});}
// BLACKWATER / SHORE (manifest 101)
std::shared_ptr<const Map> makeBlackwaterShore(){return placeholderChunk(136,{{'N',6.f,18.f,-28.f,false,0},{'E',4.f,20.f,-28.f,false,0}});}
// BLACKWATER / LAKE (manifest 101)
std::shared_ptr<const Map> makeBlackwaterLake(){return placeholderChunk(137,{{'W',4.f,20.f,-28.f,false,0},{'S',6.f,18.f,-24.f,false,0}});}
// BROKEN SHELF / UPPER (manifest 102)
std::shared_ptr<const Map> makeBrokenShelfUpper(){return placeholderChunk(138,{{'N',6.f,18.f,-24.f,false,0},{'S',2.f,22.f,-28.f,false,0}});}
// BROKEN SHELF / LOWER (manifest 102)
std::shared_ptr<const Map> makeBrokenShelfLower(){return placeholderChunk(139,{{'N',2.f,22.f,-28.f,false,0},{'W',8.f,16.f,-20.f,false,0}});}
}
void buildChapterWildsA(CampaignMapTable& maps){
 maps[134]=makeTheBreach();
 maps[135]=makeDescent();
 maps[136]=makeBlackwaterShore();
 maps[137]=makeBlackwaterLake();
 maps[138]=makeBrokenShelfUpper();
 maps[139]=makeBrokenShelfLower();
}
}

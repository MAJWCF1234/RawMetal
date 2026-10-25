// THE DEEP (chapter 4, manifest 123-136): chunks 168, 169, 170, 171, 172, 173. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// HOLLOW FOREST (manifest 128)
std::shared_ptr<const Map> makeHollowForest(){return placeholderChunk(168,{{'N',8.f,16.f,-28.f,false,0},{'E',8.f,16.f,-24.f,false,0},{'S',8.f,16.f,-28.f,false,0}});}
// BURIED MACHINERY (manifest 129)
std::shared_ptr<const Map> makeBuriedMachinery(){return placeholderChunk(169,{{'N',8.f,16.f,-28.f,false,0},{'S',8.f,16.f,-10.f,false,0}});}
// THE MAW (manifest 130)
std::shared_ptr<const Map> makeTheMaw(){return placeholderChunk(170,{{'N',8.f,16.f,-10.f,false,0},{'S',6.f,18.f,-28.f,false,0}});}
// BOSS ARENA II / FUNNEL (manifest 131)
std::shared_ptr<const Map> makeBossArenaIiFunnel(){return placeholderChunk(171,{{'N',6.f,18.f,-28.f,false,0},{'S',1.f,23.f,-28.f,false,0}});}
// BOSS ARENA II / FLOOR (manifest 131)
std::shared_ptr<const Map> makeBossArenaIiFloor(){return placeholderChunk(172,{{'N',1.f,23.f,-28.f,false,0},{'S',9.f,15.f,-28.f,true,stateId("boss2_dead")}});}
// LOWER DEPTHS (manifest 132)
std::shared_ptr<const Map> makeLowerDepths(){return placeholderChunk(173,{{'N',9.f,15.f,-28.f,true,0},{'S',8.f,16.f,-24.f,false,0}});}
}
void buildChapterFinaleB(CampaignMapTable& maps){
 maps[168]=makeHollowForest();
 maps[169]=makeBuriedMachinery();
 maps[170]=makeTheMaw();
 maps[171]=makeBossArenaIiFunnel();
 maps[172]=makeBossArenaIiFloor();
 maps[173]=makeLowerDepths();
}
}

// MINEHEAD (chapter 3, manifest 77-79): chunks 110, 111, 112. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// MINE ACCESS (manifest 77)
std::shared_ptr<const Map> makeMineAccess(){return placeholderChunk(110,{{'N',8.f,16.f,-15.f,false,0},{'W',9.f,15.f,-15.f,false,0},{'S',9.f,15.f,-4.f,false,0}});}
// EQUIPMENT DEPOT (manifest 78)
std::shared_ptr<const Map> makeEquipmentDepot(){return placeholderChunk(111,{{'E',9.f,15.f,-15.f,false,0}});}
// PERSONNEL DESCENT (manifest 79)
std::shared_ptr<const Map> makePersonnelDescent(){return placeholderChunk(112,{{'N',9.f,15.f,-4.f,false,0},{'S',9.f,15.f,-28.f,false,0}});}
}
void buildChapterMinehead(CampaignMapTable& maps){
 maps[110]=makeMineAccess();
 maps[111]=makeEquipmentDepot();
 maps[112]=makePersonnelDescent();
}
}

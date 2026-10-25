// DEPTHWORKS MEDICAL (chapter 2, manifest 48 (subchapter)): chunks 75, 76, 77, 78. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// MEDICAL / WARD C NORTH (manifest 48)
std::shared_ptr<const Map> makeMedicalWardCNorth(){return placeholderChunk(75,{{'N',10.f,14.f,-12.f,false,0},{'S',2.f,22.f,-12.f,false,0}});}
// MEDICAL / WARD C SOUTH (manifest 48)
std::shared_ptr<const Map> makeMedicalWardCSouth(){return placeholderChunk(76,{{'N',2.f,22.f,-12.f,false,0},{'W',15.f,19.f,-12.f,false,0},{'E',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// MEDICAL / SURGERY (manifest 48)
std::shared_ptr<const Map> makeMedicalSurgery(){return placeholderChunk(77,{{'W',9.f,14.f,-12.f,false,0}});}
// MEDICAL / MORGUE (manifest 48)
std::shared_ptr<const Map> makeMedicalMorgue(){return placeholderChunk(78,{{'N',10.f,14.f,-12.f,false,0},{'S',10.f,14.f,-15.f,true,stateId("med_morgue_release")}});}
}
void buildChapterMedicalWard(CampaignMapTable& maps){
 maps[75]=makeMedicalWardCNorth();
 maps[76]=makeMedicalWardCSouth();
 maps[77]=makeMedicalSurgery();
 maps[78]=makeMedicalMorgue();
}
}

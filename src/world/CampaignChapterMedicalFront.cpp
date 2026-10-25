// DEPTHWORKS MEDICAL (chapter 2, manifest 48 (subchapter)): chunks 71, 72, 73, 74. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// MEDICAL SCREENING (manifest 48)
std::shared_ptr<const Map> makeMedicalScreening(){return placeholderChunk(71,{{'W',9.f,14.f,-12.f,false,0},{'E',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,true,stateId("med_override")}});}
// MEDICAL / TRIAGE (manifest 48)
std::shared_ptr<const Map> makeMedicalTriage(){return placeholderChunk(72,{{'W',9.f,14.f,-12.f,false,0}});}
// MEDICAL / IMAGING (manifest 48)
std::shared_ptr<const Map> makeMedicalImaging(){return placeholderChunk(73,{{'N',10.f,14.f,-12.f,true,0},{'E',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// MEDICAL / PHARMACY (manifest 48)
std::shared_ptr<const Map> makeMedicalPharmacy(){return placeholderChunk(74,{{'W',9.f,14.f,-12.f,false,0}});}
}
void buildChapterMedicalFront(CampaignMapTable& maps){
 maps[71]=makeMedicalScreening();
 maps[72]=makeMedicalTriage();
 maps[73]=makeMedicalImaging();
 maps[74]=makeMedicalPharmacy();
}
}

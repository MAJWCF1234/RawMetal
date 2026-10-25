// RESEARCH FRONT (chapter 2, manifest 45-47): chunks 68, 69, 70. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// RESEARCH INTAKE (manifest 45)
std::shared_ptr<const Map> makeResearchIntake(){return placeholderChunk(68,{{'N',9.f,15.f,-12.f,true,0},{'S',10.f,14.f,-12.f,false,0}});}
// PERSONNEL PROCESSING (manifest 46)
std::shared_ptr<const Map> makePersonnelProcessing(){return placeholderChunk(69,{{'N',10.f,14.f,-12.f,false,0},{'W',9.f,14.f,-12.f,false,0},{'E',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,true,stateId("proc_clearance")}});}
// RESEARCH ADMINISTRATION (manifest 47)
std::shared_ptr<const Map> makeResearchAdministration(){return placeholderChunk(70,{{'E',9.f,14.f,-12.f,false,0}});}
}
void buildChapterResearchFront(CampaignMapTable& maps){
 maps[68]=makeResearchIntake();
 maps[69]=makePersonnelProcessing();
 maps[70]=makeResearchAdministration();
}
}

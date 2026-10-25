// CONTAINMENT (chapter 2, manifest 61-70): chunks 99, 100, 101, 102, 103. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// SPECIMEN HANDLING (manifest 65)
std::shared_ptr<const Map> makeSpecimenHandling(){return placeholderChunk(99,{{'N',10.f,14.f,-15.f,false,0},{'E',9.f,14.f,-15.f,false,0}});}
// ISOLATION WING (manifest 67)
std::shared_ptr<const Map> makeIsolationWing(){return placeholderChunk(100,{{'W',9.f,14.f,-15.f,false,0}});}
// OBSERVATION THEATER (manifest 68)
std::shared_ptr<const Map> makeObservationTheater(){return placeholderChunk(101,{{'N',10.f,14.f,-15.f,false,0},{'W',9.f,14.f,-15.f,false,0}});}
// BREACH SECTOR (manifest 69)
std::shared_ptr<const Map> makeBreachSector(){return placeholderChunk(102,{{'E',9.f,14.f,-15.f,false,0},{'S',10.f,14.f,-15.f,false,0}});}
// EMERGENCY CONTAINMENT (manifest 70)
std::shared_ptr<const Map> makeEmergencyContainment(){return placeholderChunk(103,{{'N',10.f,14.f,-15.f,false,0},{'S',10.f,14.f,-15.f,true,stateId("cont_barricade")}});}
}
void buildChapterContainmentB(CampaignMapTable& maps){
 maps[99]=makeSpecimenHandling();
 maps[100]=makeIsolationWing();
 maps[101]=makeObservationTheater();
 maps[102]=makeBreachSector();
 maps[103]=makeEmergencyContainment();
}
}

// CONTAINMENT (chapter 2, manifest 61-70): chunks 94, 95, 96, 97, 98. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// QUARANTINE WARD (manifest 66)
std::shared_ptr<const Map> makeQuarantineWard(){return placeholderChunk(94,{{'N',10.f,14.f,-15.f,true,0},{'E',9.f,14.f,-15.f,false,0},{'S',10.f,14.f,-15.f,false,0}});}
// CONTAINMENT INTAKE (manifest 61)
std::shared_ptr<const Map> makeContainmentIntake(){return placeholderChunk(95,{{'N',10.f,14.f,-15.f,false,0},{'W',9.f,14.f,-12.f,false,0},{'E',9.f,14.f,-15.f,false,0},{'S',10.f,14.f,-15.f,true,stateId("cont_power")}});}
// CONTAINMENT A (manifest 62)
std::shared_ptr<const Map> makeContainmentA(){return placeholderChunk(96,{{'W',9.f,14.f,-15.f,false,0},{'S',10.f,14.f,-15.f,false,0}});}
// CONTAINMENT SERVICE (manifest 63)
std::shared_ptr<const Map> makeContainmentService(){return placeholderChunk(97,{{'N',10.f,14.f,-15.f,false,0},{'W',15.f,19.f,-15.f,false,0},{'S',10.f,14.f,-15.f,false,0}});}
// CONTAINMENT B (manifest 64)
std::shared_ptr<const Map> makeContainmentB(){return placeholderChunk(98,{{'N',10.f,14.f,-15.f,true,0},{'E',15.f,19.f,-15.f,false,0},{'W',9.f,14.f,-15.f,false,0},{'S',10.f,14.f,-15.f,false,0}});}
}
void buildChapterContainmentA(CampaignMapTable& maps){
 maps[94]=makeQuarantineWard();
 maps[95]=makeContainmentIntake();
 maps[96]=makeContainmentA();
 maps[97]=makeContainmentService();
 maps[98]=makeContainmentB();
}
}

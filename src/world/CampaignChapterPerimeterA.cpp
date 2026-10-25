// THE RESTRICTED PERIMETER (chapter 1, manifest 36-44): chunks 57, 58, 59, 60, 61, 62. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// OPERATIONS CORRIDOR (manifest 36)
std::shared_ptr<const Map> makeOperationsCorridor(){return placeholderChunk(57,{{'N',10.f,14.f,-19.f,true,0},{'S',10.f,14.f,-19.f,false,0}});}
// SECURITY PERIMETER / YARD (manifest 37)
std::shared_ptr<const Map> makeSecurityPerimeterYard(){return placeholderChunk(58,{{'N',10.f,14.f,-19.f,false,0},{'E',4.f,20.f,-19.f,false,0},{'S',10.f,14.f,-19.f,true,stateId("perim_gate")}});}
// SECURITY PERIMETER / GATEHOUSE (manifest 37)
std::shared_ptr<const Map> makeSecurityPerimeterGatehouse(){return placeholderChunk(59,{{'W',4.f,20.f,-19.f,false,0}});}
// CHECKPOINT DELTA (manifest 38)
std::shared_ptr<const Map> makeCheckpointDelta(){return placeholderChunk(60,{{'N',10.f,14.f,-19.f,true,0},{'W',9.f,13.f,-16.f,false,0},{'S',10.f,14.f,-19.f,true,stateId("delta_override")}});}
// ADMINISTRATIVE LINK (manifest 39)
std::shared_ptr<const Map> makeAdministrativeLink(){return placeholderChunk(61,{{'E',9.f,13.f,-16.f,false,0}});}
// PERSONNEL TRANSIT (manifest 40)
std::shared_ptr<const Map> makePersonnelTransit(){return placeholderChunk(62,{{'N',10.f,14.f,-19.f,true,0},{'W',14.f,19.f,-19.f,false,0}});}
}
void buildChapterPerimeterA(CampaignMapTable& maps){
 maps[57]=makeOperationsCorridor();
 maps[58]=makeSecurityPerimeterYard();
 maps[59]=makeSecurityPerimeterGatehouse();
 maps[60]=makeCheckpointDelta();
 maps[61]=makeAdministrativeLink();
 maps[62]=makePersonnelTransit();
}
}

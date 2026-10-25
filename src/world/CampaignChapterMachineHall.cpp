// MACHINE HALL AND AIR (chapter 1, manifest 25-27): chunks 41, 42, 43, 44, 45, 46. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// MACHINE HALL / NORTH GALLERY (manifest 25)
std::shared_ptr<const Map> makeMachineHallNorthGallery(){return placeholderChunk(41,{{'N',10.f,14.f,-25.f,true,0},{'E',1.f,23.f,-28.f,false,0},{'S',1.f,23.f,-28.f,false,0}});}
// MACHINE HALL / EXCITER BAY (manifest 25)
std::shared_ptr<const Map> makeMachineHallExciterBay(){return placeholderChunk(42,{{'W',1.f,23.f,-28.f,false,0},{'S',1.f,23.f,-28.f,false,0},{'E',8.f,12.f,-25.f,false,0}});}
// MACHINE HALL / TURBINE FLOOR (manifest 25)
std::shared_ptr<const Map> makeMachineHallTurbineFloor(){return placeholderChunk(43,{{'N',1.f,23.f,-28.f,false,0},{'E',1.f,23.f,-28.f,false,0}});}
// MACHINE HALL / CRANE END (manifest 25)
std::shared_ptr<const Map> makeMachineHallCraneEnd(){return placeholderChunk(44,{{'N',1.f,23.f,-28.f,false,0},{'W',1.f,23.f,-28.f,false,0},{'S',14.f,18.f,-22.f,false,0}});}
// VENTILATION WORKS (manifest 26)
std::shared_ptr<const Map> makeVentilationWorks(){return placeholderChunk(45,{{'N',14.f,18.f,-22.f,false,0},{'W',4.f,8.f,-22.f,false,0}});}
// AIR HANDLING (manifest 27)
std::shared_ptr<const Map> makeAirHandling(){return placeholderChunk(46,{{'E',4.f,8.f,-22.f,false,0},{'S',4.f,8.f,-22.f,false,0}});}
}
void buildChapterMachineHall(CampaignMapTable& maps){
 maps[41]=makeMachineHallNorthGallery();
 maps[42]=makeMachineHallExciterBay();
 maps[43]=makeMachineHallTurbineFloor();
 maps[44]=makeMachineHallCraneEnd();
 maps[45]=makeVentilationWorks();
 maps[46]=makeAirHandling();
}
}

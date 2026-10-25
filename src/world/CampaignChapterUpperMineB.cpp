// UPPER MINE (chapter 3, manifest 80-86): chunks 117, 118, 119, 120. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// CONVEYOR CANYON / UPPER (manifest 84)
std::shared_ptr<const Map> makeConveyorCanyonUpper(){return placeholderChunk(117,{{'N',8.f,14.f,-28.f,false,0},{'W',9.f,15.f,-20.f,false,0},{'S',1.f,23.f,-28.f,false,0}});}
// CONVEYOR CANYON / LOWER (manifest 84)
std::shared_ptr<const Map> makeConveyorCanyonLower(){return placeholderChunk(118,{{'N',1.f,23.f,-28.f,false,0},{'E',9.f,15.f,-28.f,false,0},{'S',8.f,14.f,-20.f,false,0}});}
// VENTILATION SHAFT (manifest 85)
std::shared_ptr<const Map> makeVentilationShaft(){return placeholderChunk(119,{{'N',9.f,15.f,-28.f,false,0},{'S',9.f,15.f,-28.f,false,0}});}
// MAINTENANCE DRIFT (manifest 86)
std::shared_ptr<const Map> makeMaintenanceDrift(){return placeholderChunk(120,{{'N',9.f,15.f,-28.f,false,0},{'W',9.f,15.f,-28.f,false,0}});}
}
void buildChapterUpperMineB(CampaignMapTable& maps){
 maps[117]=makeConveyorCanyonUpper();
 maps[118]=makeConveyorCanyonLower();
 maps[119]=makeVentilationShaft();
 maps[120]=makeMaintenanceDrift();
}
}

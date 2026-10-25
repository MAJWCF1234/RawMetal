// MAINTENANCE DISTRICT (chapter 1, manifest 33-35): chunks 53, 54, 55, 56. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// MAINTENANCE DISTRICT (manifest 33)
std::shared_ptr<const Map> makeMaintenanceDistrict(){return placeholderChunk(53,{{'N',10.f,14.f,-19.f,false,0},{'W',9.f,14.f,-19.f,false,0},{'E',9.f,14.f,-19.f,false,0},{'S',10.f,14.f,-19.f,false,0}});}
// CENTRAL STORES (manifest 34)
std::shared_ptr<const Map> makeCentralStores(){return placeholderChunk(54,{{'E',9.f,14.f,-19.f,false,0}});}
// FABRICATION SHOP (manifest 35)
std::shared_ptr<const Map> makeFabricationShop(){return placeholderChunk(55,{{'N',10.f,14.f,-19.f,false,0},{'S',10.f,14.f,-19.f,true,stateId("maint_shutter_pin")}});}
// MAINTENANCE / AID STATION (manifest 33)
std::shared_ptr<const Map> makeMaintenanceAidStation(){return placeholderChunk(56,{{'W',9.f,14.f,-19.f,false,0}});}
}
void buildChapterMaintenance(CampaignMapTable& maps){
 maps[53]=makeMaintenanceDistrict();
 maps[54]=makeCentralStores();
 maps[55]=makeFabricationShop();
 maps[56]=makeMaintenanceAidStation();
}
}

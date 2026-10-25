// ENERGY TEST COMPLEX (chapter 2, manifest 55-60): chunks 87, 88, 89, 90. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// OBSERVATION RING / NORTH-WEST (manifest 58)
std::shared_ptr<const Map> makeObservationRingNorthWest(){return placeholderChunk(87,{{'N',10.f,14.f,-12.f,false,0},{'E',1.f,23.f,-12.f,false,0},{'S',1.f,23.f,-12.f,false,0},{'W',9.f,14.f,-12.f,false,0}});}
// OBSERVATION RING / NORTH-EAST (manifest 58)
std::shared_ptr<const Map> makeObservationRingNorthEast(){return placeholderChunk(88,{{'N',10.f,14.f,-12.f,false,0},{'W',1.f,23.f,-12.f,false,0},{'S',1.f,23.f,-12.f,false,0}});}
// OBSERVATION RING / SOUTH-WEST (manifest 58)
std::shared_ptr<const Map> makeObservationRingSouthWest(){return placeholderChunk(89,{{'N',1.f,23.f,-12.f,false,0},{'E',1.f,23.f,-12.f,false,0}});}
// OBSERVATION RING / SOUTH-EAST (manifest 58)
std::shared_ptr<const Map> makeObservationRingSouthEast(){return placeholderChunk(90,{{'N',1.f,23.f,-12.f,false,0},{'W',1.f,23.f,-12.f,false,0},{'E',9.f,14.f,-12.f,false,0}});}
}
void buildChapterObservationRing(CampaignMapTable& maps){
 maps[87]=makeObservationRingNorthWest();
 maps[88]=makeObservationRingNorthEast();
 maps[89]=makeObservationRingSouthWest();
 maps[90]=makeObservationRingSouthEast();
}
}

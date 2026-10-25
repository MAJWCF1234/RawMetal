// ANALYSIS LABORATORIES (chapter 2, manifest 49-54): chunks 79, 80, 81, 82, 83, 84. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// MATERIALS INTAKE (manifest 49)
std::shared_ptr<const Map> makeMaterialsIntake(){return placeholderChunk(79,{{'N',10.f,14.f,-12.f,true,0},{'W',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// SAMPLE STORAGE (manifest 50)
std::shared_ptr<const Map> makeSampleStorage(){return placeholderChunk(80,{{'E',9.f,14.f,-12.f,false,0}});}
// ANALYTICAL CHEMISTRY (manifest 51)
std::shared_ptr<const Map> makeAnalyticalChemistry(){return placeholderChunk(81,{{'N',10.f,14.f,-12.f,false,0},{'W',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,true,stateId("lab_chem_seal")}});}
// INSTRUMENTATION (manifest 52)
std::shared_ptr<const Map> makeInstrumentation(){return placeholderChunk(82,{{'E',9.f,14.f,-12.f,false,0},{'S',6.f,10.f,-12.f,false,0}});}
// ENERGY DIAGNOSTICS / LAB 4 (manifest 53)
std::shared_ptr<const Map> makeEnergyDiagnosticsLab4(){return placeholderChunk(83,{{'N',10.f,14.f,-12.f,true,0},{'E',15.f,19.f,-12.f,false,0},{'W',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// FIELD MEASUREMENT (manifest 54)
std::shared_ptr<const Map> makeFieldMeasurement(){return placeholderChunk(84,{{'N',6.f,10.f,-12.f,false,0},{'E',9.f,14.f,-12.f,false,0}});}
}
void buildChapterAnalysis(CampaignMapTable& maps){
 maps[79]=makeMaterialsIntake();
 maps[80]=makeSampleStorage();
 maps[81]=makeAnalyticalChemistry();
 maps[82]=makeInstrumentation();
 maps[83]=makeEnergyDiagnosticsLab4();
 maps[84]=makeFieldMeasurement();
}
}

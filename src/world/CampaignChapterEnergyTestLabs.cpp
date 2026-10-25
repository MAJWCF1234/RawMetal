// ENERGY TEST COMPLEX (chapter 2, manifest 55-60): chunks 85, 86, 91, 92, 93. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// RESONANCE LABORATORY (manifest 55)
std::shared_ptr<const Map> makeResonanceLaboratory(){return placeholderChunk(85,{{'N',10.f,14.f,-12.f,false,0},{'W',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// HIGH-ENERGY TEST BAY (manifest 56)
std::shared_ptr<const Map> makeHighEnergyTestBay(){return placeholderChunk(86,{{'E',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// SHIELDING CORRIDOR (manifest 57)
std::shared_ptr<const Map> makeShieldingCorridor(){return placeholderChunk(91,{{'E',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// EXPERIMENTAL POWER (manifest 59)
std::shared_ptr<const Map> makeExperimentalPower(){return placeholderChunk(92,{{'N',10.f,14.f,-12.f,false,0},{'S',10.f,14.f,-16.f,false,0}});}
// CONVERSION LAB (manifest 60)
std::shared_ptr<const Map> makeConversionLab(){return placeholderChunk(93,{{'N',10.f,14.f,-16.f,false,0}});}
}
void buildChapterEnergyTestLabs(CampaignMapTable& maps){
 maps[85]=makeResonanceLaboratory();
 maps[86]=makeHighEnergyTestBay();
 maps[91]=makeShieldingCorridor();
 maps[92]=makeExperimentalPower();
 maps[93]=makeConversionLab();
}
}

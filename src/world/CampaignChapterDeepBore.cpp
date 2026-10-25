// DEEP BORE (chapter 3, manifest 93-98): chunks 128, 129, 130, 131, 132, 133. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;
// DEEP BORE (manifest 93)
std::shared_ptr<const Map> makeDeepBore(){return placeholderChunk(128,{{'N',9.f,15.f,-20.f,true,0},{'S',9.f,15.f,-28.f,false,0}});}
// EXTRACTION FACE (manifest 94)
std::shared_ptr<const Map> makeExtractionFace(){return placeholderChunk(129,{{'N',9.f,15.f,-28.f,false,0},{'E',9.f,15.f,-28.f,false,0},{'S',9.f,15.f,-28.f,false,0}});}
// SURVEY DRIFT (manifest 95)
std::shared_ptr<const Map> makeSurveyDrift(){return placeholderChunk(130,{{'W',9.f,15.f,-28.f,false,0},{'S',9.f,15.f,-28.f,false,0}});}
// ABANDONED HEADING (manifest 96)
std::shared_ptr<const Map> makeAbandonedHeading(){return placeholderChunk(131,{{'N',9.f,15.f,-28.f,false,0},{'W',9.f,15.f,-28.f,false,0}});}
// BREACH SHAFT (manifest 97)
std::shared_ptr<const Map> makeBreachShaft(){return placeholderChunk(132,{{'N',9.f,15.f,-28.f,false,0},{'E',9.f,15.f,-28.f,false,0},{'S',8.f,16.f,-10.f,false,0}});}
// THRESHOLD (manifest 98)
std::shared_ptr<const Map> makeThreshold(){return placeholderChunk(133,{{'N',8.f,16.f,-10.f,false,0},{'S',8.f,16.f,-10.f,false,0}});}
}
void buildChapterDeepBore(CampaignMapTable& maps){
 maps[128]=makeDeepBore();
 maps[129]=makeExtractionFace();
 maps[130]=makeSurveyDrift();
 maps[131]=makeAbandonedHeading();
 maps[132]=makeBreachShaft();
 maps[133]=makeThreshold();
}
}

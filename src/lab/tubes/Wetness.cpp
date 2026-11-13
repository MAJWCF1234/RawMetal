// Wetness: floors are wet only where something wets them (renderer/Wetness.h). The map's mask is
// baked here exactly as the probes bake it and checked by number; the frame and the wetness view
// (--detail 17: red wet, blue puddles and wall streaks) show where it fell.
//
// Fixture keys: wet-dry 1 (a map with no water, leak or drain: almost no floor wet), wet-at1..4 X Y
// (a point that must be wet), wet-dry-at1..4 X Y (a point that must stay dry), wet-rain R (the
// rain outdoors, 0 dry .. 1 pouring; default the map's own).
#include "lab/Lab.h"
#include "lab/LabFixture.h"
#include "renderer/Wetness.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
WetnessMask maskOf(const World& w){float low=1e9f;
 for(int y=0;y<World::Height;++y)for(int x=0;x<World::Width;++x)if(w.tile(x,y)!='#')low=std::min(low,w.floorHeight(x+.5f,y+.5f));
 return bakeWetness(w,low<1e8f?low:0.f);}
bool point(const Fixture& f,const std::string& key,float& x,float& y){const auto v=f.value(key.c_str());if(v.empty())return false;std::istringstream in(v);return bool(in>>x>>y);}
const bool wetness=add({
 .name="wetness",.summary="local wetness: dry floors stay dry, wet round water, under leaks and drains, rain only on what sees the sky",
 .fixtures={"wet-*"},
 .setup=[](Scene& s){rainOverride()=float(s.fixture->number("wet-rain",-1));},
 .capture=[](Scene& s,const Frame&){
  if(!s.last()||!s.renderer)return;auto& r=*s.renderer;const int layer=SoftwareRenderer::detailLayer();
  SoftwareRenderer::setDetailLayer(17);r.render(s.g());s.shoot("wetness (red wet, blue puddles and streaks)");
  // The share of the frame drawn wet (red well over green in the wetness view).
  const auto& shot=s.shots.back();long n=0;for(auto p:shot.pixels){const int R=int((p>>16)&255u),G=int((p>>8)&255u);if(R>G+40)++n;}
  s.board["wet.view_share"]=double(n)/double(std::max<size_t>(1,shot.pixels.size()));
  SoftwareRenderer::setDetailLayer(layer);r.render(s.g());},
 .metrics=[](Scene& s,Report& r){
  const World& w=s.g().world();const auto mask=maskOf(w);const auto& f=*s.fixture;
  // The share of open floor that is wet at all (on its own floor).
  long open=0,wet=0;for(int y=0;y<WetnessMask::Size;++y)for(int x=0;x<WetnessMask::Size;++x){const float px=(x+.5f)*WetnessMask::Texel,py=(y+.5f)*WetnessMask::Texel;
   if(w.tile(int(px),int(py))=='#')continue;++open;if(mask.floorWet(px,py,w.floorHeight(px,py))>.1f)++wet;}
  const double share=open?double(wet)/open:0;r.metric("wet_floor_share",share,"share");r.metric("map_wetness",w.wetness());
  if(f.number("wet-dry",0)>0)r.check("a map with nothing to wet it has dry floors (under 1% wet)",share<.01,fixed(share*100,2)+"%");
  else if(!w.outdoors())r.check("an indoor map is wet in patches, never wall to wall (under 60% of its floor)",share<.6,fixed(share*100,1)+"%");
  for(int i=1;i<=4;++i){float x,y;
   if(point(f,"wet-at"+std::to_string(i),x,y)){const float v=mask.floorWet(x,y,w.floorHeight(x,y));r.metric("wet_at"+std::to_string(i),v);
    r.check("wet at ("+fixed(x,1)+", "+fixed(y,1)+")",v>.5f,fixed(v));}
   if(point(f,"wet-dry-at"+std::to_string(i),x,y)){const float v=mask.floorWet(x,y,w.floorHeight(x,y));r.metric("dry_at"+std::to_string(i),v);
    r.check("dry at ("+fixed(x,1)+", "+fixed(y,1)+")",v<.05f,fixed(v));}}
  r.metric("rain",outdoorRain(w));const double drawn=s.board["wet.view_share"];r.metric("wet_in_view",drawn,"share");
  if(!f.value("wet-rain").empty()){const double rain=f.number("wet-rain",0);
   if(rain>0)r.check("in rain the open ground is drawn wet (over 15% of the frame)",drawn>.15,fixed(drawn*100,1)+"%");
   else r.check("dry weather leaves the open ground dry (under 2% of the frame)",drawn<.02,fixed(drawn*100,2)+"%");}},
 .teardown=[](Scene&){rainOverride()=-1;},
 .needsGpu=true});
}
}

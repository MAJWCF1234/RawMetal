#pragma once
// Footings: a room or a floor laid on the land, or raised off a floor (STRUCT footing=).
//
//   STRUCT|map|x1|y1|x2|y2|bottom|top|0|material|...|footing=pad|snap=mean|raise=0.6|base=31|cap=33
//
// The box is the footprint. Its level L is where the land meets it: on a terrain map the
// ground under the footprint (the lowest, mean or highest point: snap=) plus `bottom`, or
// `bottom` itself (snap=fixed, and on maps without land). The floor is walked at
// S = L + (top - bottom) + raise, a slab of `material` under it, and below the slab:
//   pad         the land under the footprint is cut and filled to L (a skirt blends it back
//               into the hills); with raise, a plinth of `base` stands on it
//   foundation  the land above L is cut away; a plinth of `base` reaches down to the land
//               on the low side
//   stilts      the land above L is cut away; posts of `base` hold the floor; walk under it
//   stepped     the footprint is cut into terraces along its long side (terrace= metres),
//               each a pad at its own ground, steps between them
// cap= rings the slab's top edge in another material (capwidth= wide); steps=0 leaves off
// the steps the engine lays from a raised door down to the ground outside.
//
// Header-only, free of the World: the engine (CustomCampaign.cpp, World::buildFootings)
// and the Campaign Builder plan the same footing from the same ground.
#include "TerrainField.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace retro {
enum FootingMode:int {FootingNone,FootingPad,FootingFoundation,FootingStilts,FootingStepped};
enum FootingSnap:int {FootingSnapLow,FootingSnapMean,FootingSnapHigh,FootingSnapFixed};
inline constexpr const char* FootingModeNames[]={"","pad","foundation","stilts","stepped"};
inline constexpr const char* FootingSnapNames[]={"low","mean","high","fixed"};
inline int footingModeOf(std::string_view w){for(int i=1;i<5;++i)if(w==FootingModeNames[i])return i;return -1;}
inline int footingSnapOf(std::string_view w){for(int i=0;i<4;++i)if(w==FootingSnapNames[i])return i;return -1;}
inline constexpr float FootingMaxRaise=6.f,FootingSlabMax=.3f,FootingSkirt=2.5f;

struct FootingSpec {
 float x1=0,y1=0,x2=0,y2=0,bottom=0,top=.1f;
 int mode=FootingPad,snap=FootingSnapMean;
 float raise=0,terrace=3;
};
// One level piece of a footing (a stepped footing has several, in order along its long side).
struct FootingPart {
 float x1=0,y1=0,x2=0,y2=0;
 float level=0;    // L: the ground it is levelled to (pads) or measured from
 float surface=0;  // S: the floor walked on
 float slab=0;     // the slab's underside: S less its thickness
};
inline float footingSlab(const FootingSpec& s){return std::clamp(s.top-s.bottom,.02f,FootingSlabMax);}
// The footing's pieces over the land: ground(x, y) is the natural walked ground at map
// metres (TerrainField.h, without any footing pad), `land` false on a map without terrain.
template<class Ground> std::vector<FootingPart> planFooting(const FootingSpec& s,bool land,Ground&& ground){
 const float x1=std::min(s.x1,s.x2),x2=std::max(s.x1,s.x2),y1=std::min(s.y1,s.y2),y2=std::max(s.y1,s.y2);
 const bool alongX=x2-x1>=y2-y1;const float length=alongX?x2-x1:y2-y1;
 const int count=s.mode==FootingStepped&&land?std::clamp(int(std::lround(length/std::max(1.f,s.terrace))),1,16):1;
 const float thick=footingSlab(s);
 std::vector<FootingPart> parts;
 for(int i=0;i<count;++i){
  FootingPart p{x1,y1,x2,y2};
  const float a=float(i)/float(count),b=float(i+1)/float(count);
  if(alongX){p.x1=x1+(x2-x1)*a;p.x2=x1+(x2-x1)*b;}else{p.y1=y1+(y2-y1)*a;p.y2=y1+(y2-y1)*b;}
  float level=s.bottom;
  if(land&&s.snap!=FootingSnapFixed){
   // The ground over the piece on a half-metre lattice, its edges included.
   float low=1e9f,high=-1e9f,sum=0;int n=0;
   const int nx=std::max(1,int(std::ceil((p.x2-p.x1)*2))),ny=std::max(1,int(std::ceil((p.y2-p.y1)*2)));
   for(int j=0;j<=ny;++j)for(int k=0;k<=nx;++k){const float z=ground(p.x1+(p.x2-p.x1)*float(k)/float(nx),p.y1+(p.y2-p.y1)*float(j)/float(ny));
    low=std::min(low,z);high=std::max(high,z);sum+=z;++n;}
   const float g=s.snap==FootingSnapLow?low:s.snap==FootingSnapHigh?high:sum/float(std::max(1,n));
   // Kept to the 32nd of a metre the ground's densities hold (TerrainField.h), so a pad meets its slab exactly.
   level=std::round((g+s.bottom)*32.f)/32.f;
  }
  p.level=level;p.surface=level+thick+std::clamp(s.raise,0.f,FootingMaxRaise);p.slab=p.surface-thick;
  parts.push_back(p);
 }
 return parts;
}
// The pads a footing puts in the land (world metres: ox, oy the map's origin). Whole
// metre columns covering the footprint; the skirt blends outside it.
inline std::vector<TerrainFlat> footingPads(const FootingSpec& s,const std::vector<FootingPart>& parts,float ox,float oy){
 std::vector<TerrainFlat> pads;
 const int kind=s.mode==FootingPad||s.mode==FootingStepped?TerrainPadLevel:TerrainPadCut;
 for(const auto& p:parts)pads.push_back({ox+std::floor(p.x1),oy+std::floor(p.y1),ox+std::ceil(p.x2),oy+std::ceil(p.y2),p.level,kind==TerrainPadLevel?FootingSkirt:1.5f,kind});
 // Terraces share their boundary column: the lower one is written last, so it wins and
 // the land never rises into the lower slab (the higher one's plinth covers its edge).
 std::stable_sort(pads.begin(),pads.end(),[](const TerrainFlat& a,const TerrainFlat& b){return a.height>b.height;});
 return pads;
}
}

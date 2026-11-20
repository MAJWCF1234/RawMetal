// A synthetic open-world campaign for the open-space tube: N x N stitched 24 m outdoor chunks of
// hilly terrain with the painterly horizon beyond, a pine/oak/leafy/aspen forest, rocks, fallen
// trunks, ruined walls and sheds, parked wrecks, barrels and a few physics props. It uses library
// models by catalogue slot but no map of the game, so it stands in for any large outdoor campaign
// an artist builds with the Terrain and Nature tools. Density is the Ashfall zone's own (about 200
// fixtures a chunk). Nothing here ships: only the lab links it.
#include "StressZone.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
namespace retro::lab {
namespace {
struct Rng {std::uint32_t s;float next(){s=s*1664525u+1013904223u;return float(s>>8)/16777216.f;}float range(float a,float b){return a+(b-a)*next();}};
struct Model {int slot;float w,d,h,core;};
// Catalogue slots (src/assets/library/catalog.json): width, depth, height in metres.
constexpr Model Pine{121,3.7f,3.4f,10.1f,.1f},PineB{122,3.7f,3.4f,10.1f,.1f},Oak{118,3.7f,3.6f,6.9f,.1f},OakB{119,3.2f,3.3f,6.1f,.1f},LeafyA{148,3.4f,3.4f,4.f,.1f},
 Aspen{150,3.9f,4.5f,10.f,.1f},RockBig{124,3.6f,3.3f,2.f,.9f},RockMid{125,2.5f,2.7f,2.1f,.9f},RockSmall{126,1.5f,1.4f,.8f,.9f},Fallen{152,.7f,5.f,1.6f,1.f},
 WallA{425,4.f,.4f,3.f,1.f},WallB{426,4.f,.4f,3.f,1.f},Hut{97,7.7f,3.6f,4.3f,1.f},Shed{104,8.7f,5.2f,5.5f,1.f},Warehouse{110,37.5f,14.7f,12.4f,1.f},
 CarA{69,2.6f,6.2f,2.2f,1.f},Barrel{87,.5f,.6f,1.f,1.f},Crate{93,.8f,.8f,.8f,1.f};
}
std::string stressCampaign(int chunks,int detail){
 chunks=std::clamp(chunks,1,13);detail=std::clamp(detail,1,4); // 13 x 13 = 169, under the engine's 180
 const Model trees[]={Pine,PineB,Pine,Oak,OakB,LeafyA,Aspen,PineB};
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nFORMAT|2\n";char b[400];
 const int mid=chunks/2;t+="CAMPAIGN|Open space stress|"+std::to_string(mid*chunks+mid)+"\n";
 t+="TERRAIN|7331|4|3.2|34|0.45\nHORIZON|150|5|14|121,122,148,149,150,151,118,119\n";
 for(int cy=0;cy<chunks;++cy)for(int cx=0;cx<chunks;++cx){
  const int i=cy*chunks+cx;
  std::snprintf(b,sizeof b,"MAP|%d|Stress %d-%d|%d|%d|12|12|0|1|2.8|0.3|zone_overcast|%d|%d|%d|%d|0|-1|0|0|0\nLAYER|%d|0|Stress ground|0|0\n",i,cx,cy,cx*24,cy*24,cy>0,cy<chunks-1,cx>0,cx<chunks-1,i);t+=b;
  for(int r=0;r<24;++r){std::snprintf(b,sizeof b,"ROW|%d|0|%d|........................\n",i,r);t+=b;}
  Rng g{std::uint32_t(i*2654435761u+97u)};
  auto put=[&](const Model& m,float x,float y,float yaw,float scale,int solid){
   std::snprintf(b,sizeof b,"FIXTURE|%d|%d|%.2f|%.2f|0|%.3f|%.3f|%.3f|%.3f|%d|%.2f\n",i,m.slot,x,y,m.w*scale,m.d*scale,m.h*scale,yaw,solid,m.core);t+=b;};
  const bool ruin=(i%3)==0,yard=(i%11)==5;
  for(int n=0;n<42*detail+8;++n)put(trees[size_t(g.next()*8)%8],g.range(1.f,23.f),g.range(1.f,23.f),g.range(0,6.28f),g.range(.8f,1.3f),1);
  for(int n=0;n<5*detail;++n){const Model rocks[]={RockBig,RockMid,RockSmall,RockSmall};put(rocks[size_t(g.next()*4)%4],g.range(1.f,23.f),g.range(1.f,23.f),g.range(0,6.28f),g.range(.8f,1.5f),1);}
  for(int n=0;n<detail;++n)put(Fallen,g.range(2.f,22.f),g.range(2.f,22.f),g.range(0,6.28f),1.f,1);
  if(ruin){const float x=g.range(5.f,15.f),y=g.range(5.f,15.f);
   put(g.next()<.5f?Hut:Shed,x,y,g.range(0,6.28f),1.f,1);
   for(int n=0;n<6*detail;++n)put(g.next()<.5f?WallA:WallB,x+g.range(-8.f,8.f),y+g.range(-8.f,8.f),g.next()<.5f?0.f:1.5708f,1.f,1);}
  if(yard)put(Warehouse,12.f,12.f,0.f,1.f,1);
  for(int n=0;n<6*detail;++n){const Model props[]={CarA,Barrel,Crate,Barrel};put(props[size_t(g.next()*4)%4],g.range(1.f,23.f),g.range(1.f,23.f),g.range(0,6.28f),1.f,1);}
  if(i%4==1)for(int n=0;n<4;++n){std::snprintf(b,sizeof b,"CLUTTER|%d|%d|%.2f|%.2f|-999|%.2f\n",i,n%4,g.range(3.f,21.f),g.range(3.f,21.f),g.range(0,6.28f));t+=b;}
 }
 t+="--- CUSTOM_CAMPAIGN_DATA_END ---\n";
 return t;
}
}

// Floors: stair and balcony finishes, buildings on terrain and laid flooring, as the
// renderer draws them (lab/fixtures/floors-*.lab over the .rmc maps beside them).
//
// Fixture keys:
//   floors-shot LABEL              the picture's name (the last frame is kept as it)
//   floors-room X1 Y1 X2 Y2        a floor footprint (map metres) to measure:
//     floor_range_m                the spread of the walked height over it (0.25 m grid)
//     terrain_through              terrain mesh corners inside it above that floor
//   floors-level 1                 check: the footprint is one level floor (spread under 1 cm)
//   floors-sealed 1                check: no terrain pokes through it
//   floors-pieces MATERIAL N       check: at least N structures of MATERIAL were built
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fx(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Rect {float x1=0,y1=0,x2=0,y2=0;bool set=false;};
Rect rectOf(const Fixture& f){Rect r;const auto t=f.value("floors-room");if(!t.empty()&&std::sscanf(t.c_str(),"%f %f %f %f",&r.x1,&r.y1,&r.x2,&r.y2)==4)r.set=true;return r;}
}
const bool floorsTube=add({
 .name="floors",.summary="stair and balcony finishes, a building on terrain (one level floor, no terrain through it) and laid flooring, drawn as the game draws them",
 .fixtures={"floors-*"},
 .capture=[](Scene& s,const Frame&){
  if(!s.last())return;const World& w=s.g().world();
  if(const Rect r=rectOf(*s.fixture);r.set){
   float low=1e9f,high=-1e9f;
   for(float y=r.y1+.125f;y<r.y2;y+=.25f)for(float x=r.x1+.125f;x<r.x2;x+=.25f){const float z=w.supportBelow(x,y,60.f,false);low=std::min(low,z);high=std::max(high,z);}
   s.board["floors.range"]=high-low;s.board["floors.top"]=high;
   int through=0;
   for(const auto& t:w.terrain())for(const auto* v:{&t.a,&t.b,&t.c})
    if(v->x>r.x1+.02f&&v->x<r.x2-.02f&&v->y>r.y1+.02f&&v->y<r.y2-.02f&&v->z>w.supportBelow(v->x,v->y,60.f,false)-.005f)++through;
   s.board["floors.through"]=through;}
  if(const auto piece=s.fixture->value("floors-pieces");!piece.empty()){int material=-1,want=0;std::sscanf(piece.c_str(),"%d %d",&material,&want);
   int n=0;for(const auto& st:w.structures())n+=st.material==material&&st.x2>st.x1;s.board["floors.pieces"]=n;s.board["floors.want"]=want;}
  s.shoot(s.fixture->value("floors-shot","floors"));},
 .metrics=[](Scene& s,Report& r){
  if(rectOf(*s.fixture).set){
   r.metric("floor_range_m",s.board["floors.range"],"m");r.metric("floor_top_m",s.board["floors.top"],"m");r.metric("terrain_through",s.board["floors.through"]);
   if(s.fixture->value("floors-level")=="1")r.check("the floor is one level",s.board["floors.range"]<.01,"spread "+fx(s.board["floors.range"]));
   if(s.fixture->value("floors-sealed")=="1")r.check("no terrain through the floor",s.board["floors.through"]==0,fx(s.board["floors.through"],0)+" corners");}
  if(!s.fixture->value("floors-pieces").empty())
   r.check("the finish is built",s.board["floors.pieces"]>=s.board["floors.want"],fx(s.board["floors.pieces"],0)+" pieces");}});
}

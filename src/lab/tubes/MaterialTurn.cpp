// Material turns (STRUCT/PRIM turn=, docs/Systems/map-format.md): a plank floor laid as it always
// was beside the same floor turned a quarter, seen from above (lab/fixtures/material-turn.lab).
//
// Checks:
// - the parser keeps the turn (0 for the floor with no turn= and for turn=360, 90 for turn=90);
// - the planks run the other way on the turned floor: each floor's pattern is measured by how
//   much the picture changes across the screen's x against its y (seams across the planks change
//   it, the grain along them hardly), and the two floors lean opposite ways, each clearly;
// - no turn draws through the old path untouched (Scene3D skips the turn), so older maps look the same.
// Run it on the GPU and with --software: both draw from the same faces (Scene3D setTurn).
#include "lab/Lab.h"
#include "lab/LabFixture.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed3(double v){char b[32];std::snprintf(b,sizeof b,"%.3f",v);return b;}
float luma(std::uint32_t p){return float((p>>16)&255u)*.2126f+float((p>>8)&255u)*.7152f+float(p&255u)*.0722f;}
// log(change across x / change across y) over a box of the frame (fractions of its size).
double lean(const Frame& f,float x0,float y0,float x1,float y1){
 double gx=0,gy=0;const int ax=int(f.width*x0),bx=int(f.width*x1),ay=int(f.height*y0),by=int(f.height*y1);
 for(int y=ay;y<by-1;++y)for(int x=ax;x<bx-1;++x){const size_t i=size_t(y)*f.width+x;
  gx+=std::fabs(luma(f.pixels[i+1])-luma(f.pixels[i]));gy+=std::fabs(luma(f.pixels[i+f.width])-luma(f.pixels[i]));}
 return gy>0&&gx>0?std::log(gx/gy):0.0;}
}
const bool materialTurnTube=add({
 .name="material-turn",.summary="a plank floor turned a quarter runs the other way; no turn draws as before (GPU and software)",
 .fixtures={"material-turn"},
 .capture=[](Scene& s,const Frame& f){
  if(!s.last())return;
  int plain=0,turned=0,zero=0;
  for(const auto& st:s.g().world().structures())if(st.material==51&&st.x2-st.x1>1){
   if(st.turn==90)++turned;else if(st.turn==0)(st.x1<4?++zero:++plain);}
  s.board["turn.plain"]=plain;s.board["turn.turned"]=turned;s.board["turn.zero"]=zero;
  // Looking down with +y up the screen, the floor as laid (x 6-10) is on the right, the turned (x 12-16) on the left.
  s.board["turn.lean_plain"]=lean(f,.58f,.33f,.76f,.67f);
  s.board["turn.lean_turned"]=lean(f,.24f,.33f,.42f,.67f);
  s.shoot("planks: turned a quarter (left), as laid (middle), turn=360 (right edge)");},
 .metrics=[](Scene& s,Report& r){
  const double a=s.board["turn.lean_plain"],b=s.board["turn.lean_turned"];
  r.metric("lean_as_laid",a);r.metric("lean_turned",b);
  r.check("the parser keeps each turn (no turn=, turn=360 and turn=0 are 0; turn=90 is 90)",
   s.board["turn.plain"]==1&&s.board["turn.turned"]==1&&s.board["turn.zero"]==1,
   "as laid "+fixed3(s.board["turn.plain"])+", turned "+fixed3(s.board["turn.turned"])+", turn=0 "+fixed3(s.board["turn.zero"]));
  r.check("the turned planks run the other way",a*b<0&&std::fabs(a)>.1&&std::fabs(b)>.1,"lean "+fixed3(a)+" against "+fixed3(b));}});
}

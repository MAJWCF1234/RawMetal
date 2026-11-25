// The Airlock kit as it is placed in the Starter Map's Kits Gallery: two lifting hatches that follow one cycle
// state (Door::follow, Game::followDoors). The outer opens by itself; the terminal inside flips the cycle; the
// outer shuts and only then does the inner open; they are never open together. Pictures: outer open, cycling,
// inner open.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <algorithm>
#include <cmath>
namespace retro::lab {
namespace {
const StateId Cycle=stateId("airlock_cycle");
int hatch(const World& w,const char* label){for(size_t i=0;i<w.doors().size();++i)if(w.doors()[i].label==label)return int(i);return -1;}
const bool airlock=add({
 .name="airlock",.summary="the Airlock kit: two hatches on one cycle state, one open at a time (Door::follow)",
 .fixtures={"airlock"},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);},
 .step=[](Scene& s,float){if(s.frame==150)s.g().setState(Cycle,1);},   // the terminal's toggle, at 2.5 s
 .capture=[](Scene& s,const Frame&){const World& w=s.g().world();const int outer=hatch(w,"OUTER HATCH"),inner=hatch(w,"INNER HATCH");
  if(outer<0||inner<0)return;const float o=w.doors()[size_t(outer)].open,i=w.doors()[size_t(inner)].open;
  s.board["both"]=std::max(s.board["both"],double(std::min(o,i)));
  if(s.frame==140){s.board["outer_open"]=o;s.board["inner_shut"]=i;s.shoot("cycle 0: the outer hatch open, the inner shut");}
  if(s.frame==175)s.shoot("cycling: the outer hatch going down, the inner still shut");
  if(s.frame==s.frames-1){s.board["outer_end"]=o;s.board["inner_end"]=i;s.shoot("cycle 1: the outer shut, the inner open");}},
 .metrics=[](Scene& s,Report& r){const World& w=s.g().world();
  r.check("the gallery's airlock has both hatches",hatch(w,"OUTER HATCH")>=0&&hatch(w,"INNER HATCH")>=0);
  r.check("at cycle 0 the outer hatch opens by itself and the inner stays shut",s.board["outer_open"]>.95&&s.board["inner_shut"]<.02);
  r.check("at cycle 1 the outer has shut and the inner opened",s.board["outer_end"]<.02&&s.board["inner_end"]>.95);
  r.check("the two are never open together",s.board["both"]<.02,"the lesser of the two reached "+std::to_string(s.board["both"]));}
});
}
}

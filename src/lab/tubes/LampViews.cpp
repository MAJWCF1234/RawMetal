// Lamps as a player sees them (the map audit's L: LAMPS, MapAudit.cpp): a caged lamp with its own bulb and no
// wall lamp drawn over it, a fitting flush under its ceiling in a custom map, one housing where two were. Each
// fixture lamps-*.lab is one view from its camera, turned to look at `aim-x aim-y aim-z` (the lamp, in the map's
// own metres); the picture is the evidence, and the check is that the view is lit (the frame is not black).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
namespace retro::lab {
namespace {
const bool lampViews=add({
 .name="lamp-views",.summary="lamps mounted at their own faces: caged bulbs, flush fittings, one housing per lamp (map audit L: LAMPS)",
 .fixtures={"lamps-*"},
 .step=[](Scene& s,float){if(!s.fixture->values.count("aim-x"))return;auto& p=LabAccess::player(s.g());
  const Point3 aim{s.fixture->number("aim-x",0),s.fixture->number("aim-y",0),s.fixture->number("aim-z",0)};const float dx=aim.x-p.pos.x,dy=aim.y-p.pos.y;
  p.angle=std::atan2(dy,dx);p.pitch=std::atan2(aim.z-(p.z+p.eye),std::sqrt(dx*dx+dy*dy))*140.f;},
 .capture=[](Scene& s,const Frame& f){
  if(s.frame!=s.frames-1)return;s.shoot(s.fixture->name);
  double sum=0;const size_t n=size_t(f.width)*size_t(f.height);for(size_t i=0;i<n;++i){const auto p=f.pixels[i];sum+=((p>>16)&255)*.2126+((p>>8)&255)*.7152+(p&255)*.0722;}
  s.board["luma"]=n?sum/double(n):0;},
 .metrics=[](Scene& s,Report& r){r.metric("mean_luma",s.board["luma"]);r.check("the view is lit",s.board["luma"]>8,"mean luma "+std::to_string(int(s.board["luma"])));}
});
}
}

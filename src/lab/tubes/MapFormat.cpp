// Map format 2 in play (docs/Systems/map-format.md): the room of MapFormatTest.cpp
// (lab/fixtures/map-format.rmc) as the renderer draws it.
//   map-format       the red crate (tint=1,0.25,0.25) beside a plain one, the frozen box
//                    hanging in the air, the prefab desks; the crate must read red.
//   map-format-open  standing in the script's box: it reads @vault.code, sets
//                    @vault.opened and clears @held_box.frozen, and the box falls.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fx(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
const Clutter* prop(Game& g,const char* name){for(const auto& c:LabAccess::clutter(g))if(c.traits&&c.traits->name==name)return &c;return nullptr;}
// Mean red over green of the pixels round a world point (a 7 x 7 patch), or -1 off screen.
double redness(const Scene& s,const Frame& f,Point3 at){
 const auto p=project(s,at);if(!p.visible)return -1;double r=0,g=0;int n=0;
 for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx){const int x=int(p.x)+dx,y=int(p.y)+dy;if(x<0||y<0||x>=f.width||y>=f.height)continue;
  const auto px=f.pixels[size_t(y)*f.width+x];r+=(px>>16)&255;g+=(px>>8)&255;++n;}
 return n?(r+1)/(g+1):-1;
}
}
const bool mapFormatTube=add({
 .name="map-format",.summary="map format 2 overrides in play: a tinted crate, a frozen prop a script lets go, a custom property read by a script (EntityTraits.h)",
 .fixtures={"map-format","map-format-open"},
 .capture=[](Scene& s,const Frame& f){
  if(!s.last())return;Game& g=s.g();
  s.board["mf.red"]=redness(s,f,{8,8,.3f});s.board["mf.plain"]=redness(s,f,{9.6f,9.4f,.3f});
  if(const auto* c=prop(g,"held_box")){s.board["mf.box_z"]=c->z;s.board["mf.box_pinned"]=c->pinned;}
  s.board["mf.opened"]=LabAccess::entityProperty(g,0,"vault","opened");s.board["mf.code"]=LabAccess::entityProperty(g,0,"vault","code");
  s.shoot(s.fixture->value("mf-scene"));},
 .metrics=[](Scene& s,Report& r){
  const auto scene=s.fixture->value("mf-scene");
  r.metric("red_crate_r_over_g",s.board["mf.red"]);r.metric("plain_crate_r_over_g",s.board["mf.plain"]);r.metric("held_box_z",s.board["mf.box_z"],"m");
  r.metric("vault_code",s.board["mf.code"]);r.metric("vault_opened",s.board["mf.opened"]);
  if(scene=="closed"){
   r.check("the tinted crate reads red beside the plain one",s.board["mf.red"]>0&&s.board["mf.plain"]>0&&s.board["mf.red"]>s.board["mf.plain"]*1.6,"r/g "+fx(s.board["mf.red"])+" vs "+fx(s.board["mf.plain"]));
   r.check("the frozen box hangs where it was put",s.board["mf.box_pinned"]==1&&std::fabs(s.board["mf.box_z"]-1.2)<.01,"z "+fx(s.board["mf.box_z"]));
   r.check("the vault holds its authored code and is shut",s.board["mf.code"]==1234&&s.board["mf.opened"]==0);}
  else{
   r.check("the script read @vault.code and set @vault.opened",s.board["mf.opened"]==1,"opened "+fx(s.board["mf.opened"],0));
   r.check("the script freed the box and it fell",s.board["mf.box_pinned"]==0&&s.board["mf.box_z"]<.6,"z "+fx(s.board["mf.box_z"]));}}});
}

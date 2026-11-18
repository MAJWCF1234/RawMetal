// Window walls (GLAZING, src/world/Openings.cpp): a storefront dragged along a wall of # tiles in the
// Campaign Builder, as the engine plays it. The tube authors one across the plain room: four bays, two
// rows, a doorway bay with a transom pane, and one pane of each kind (clear, dirty, mesh, frosted, wired,
// tinted, pre-cracked); it films the wall whole, then a round volley into one pane.
//
// storefront: the numbers, each from the authored room:
//  - the cut: the wall's tiles under it are open, the wall built back above the head (infill), the
//    frame solid, every glass pane a breakable of its own and the mesh pane a chain-link box;
//  - one pane: rounds into one pane bring it down and leave every other pane without a mark;
//  - mesh: rounds at the mesh pass through it and it stands; it stops a body;
//  - the doorway: the player walks through the door bay; the glass bays stop him;
//  - saves: a save of the broken pane and a holed one loads back exactly;
//  - the pre-cracked pane starts with its cracks.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// The storefront: x 4-12 along the wall of tiles at y 10, floor to the 8 m ceiling.
Glazing storefront(){Glazing g;g.x1=4;g.y1=10;g.x2=12;g.y2=11;g.bottom=0;g.top=8;g.columns=4;g.rows=2;g.door=2;g.look=12;g.panes="cdmfwtx";return g;}
void author(AuthoredMapData& m){
 m.layers[0].rows[10]=std::string(24,'#');m.glazings.push_back(storefront());
 // Behind it a back room: a yellow wall to see the glass against, crates and a lamp.
 kit::wall(m,3,3.4f,13,3.6f,0,3,14);kit::prop(m,model::wooden_crate,6,6,0,.3f);kit::prop(m,model::ind_barrel,9.5f,5.5f,0,0);}
std::shared_ptr<CustomCampaign> room(){auto m=regression::room();m->definition.playerStart={8,15};author(*m);return regression::campaign(m);}
// The panes in bay order: 0 clear, 1 dirty, 2 frosted (bay 1's mesh is a box), 3 wired, 4 tinted, 5 cracked.
Point3 paneCentre(const World& w,int i){const auto& b=w.breakables()[size_t(i)];return {(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,(b.bottom+b.top)*.5f};}
int meshBoxes(const World& w){int n=0;for(const auto& s:w.structures())n+=s.material==ChainLinkMaterial;return n;}
const HitscanDef& shotgun(){return playerWeaponDef(WeaponId::Shotgun).hitscan;}
const HitscanDef& handgun(){return playerWeaponDef(WeaponId::Handgun).hitscan;}
// Rounds from the shop floor (y 14) straight at pane i until it comes down (at most 40).
int bring(Game& g,int i){const auto c=paneCentre(g.world(),i);int n=0;
 for(;n<40&&g.world().breakableIntact(i);++n){const float u=float(n%5-2)*.12f,v=float(n/5%5-2)*.1f;LabAccess::fireFrom(g,{c.x+u,14,c.z+v},{c.x+u,c.y,c.z+v},shotgun());}
 return g.world().breakableIntact(i)?-1:n;}
int marked(const World& w,int except){int n=0;for(int i=0;i<int(w.breakables().size());++i)if(i!=except&&(!w.breakableIntact(i)||(w.paneGlass(i)&&w.breakables()[size_t(i)].cracks==0)))++n;return n;}
constexpr int Volley=40,Every=6;
const bool tube=add({
 .name="storefront",.summary="window walls (GLAZING): a storefront cut into a wall of tiles, its frame solid, every glass pane breaking on its own, chain-link mesh that stops a body and nothing else, a doorway bay, panes of every kind, a save keeping the broken ones",
 .friends={"glass"},.fixtures={"storefront"},
 .author=[](Scene&,AuthoredMapData& m){author(m);},
 .step=[](Scene& s,float){Game& g=s.g();
  // A volley at the clear pane (bay 0, bottom row), from the shop floor.
  if(s.frame>=Volley&&(s.frame-Volley)%Every==0&&(s.frame-Volley)/Every<10&&g.world().breakables().size()>0){const auto c=paneCentre(g.world(),0);const int k=(s.frame-Volley)/Every;
   LabAccess::fireFrom(g,{c.x-.4f,13.5f,1.5f},{c.x+.08f*float(k%3-1),c.y,c.z+.1f*float(k/3%3-1)},shotgun());}},
 .capture=[](Scene& s,const Frame&){if(s.frame==Volley-1)s.shoot("whole");if(s.last())s.shoot("one pane down");},
 .metrics=[](Scene& s,Report& r){
  {Game& g=s.g();r.metric("scene_panes",double(g.world().breakables().size()));
   r.check("scene: the volley brings down the clear pane and leaves every other pane unmarked",!g.world().breakableIntact(0)&&marked(g.world(),0)==0,std::to_string(marked(g.world(),0))+" other panes marked");}
  // ---- the cut and what stands in it ----
  {Game g(room());const World& w=g.world();int glass=0,wired=0,cracked=0;for(const auto& b:w.breakables()){glass+=b.material==17;wired+=b.holds&&b.glaze==Glaze::Wired;cracked+=b.cracks>0;}
   bool open=true;for(int x=4;x<12;++x)open&=w.tile(x,10)=='.';const bool wall=w.tile(3,10)=='#'&&w.tile(12,10)=='#';
   int frame=0,infill=0;for(const auto& s:w.structures()){frame+=s.material==12;infill+=s.material==3&&s.y1>=10-1e-3f&&s.y2<=11+1e-3f&&s.top>=8-1e-3f;}
   r.metric("glass_panes",glass);r.metric("mesh_boxes",meshBoxes(w));r.metric("frame_members",frame);r.metric("infill_boxes",infill);
   r.check("the wall's tiles under the storefront are cut, the tiles beside it stand, and the wall is built back over the head",open&&wall&&infill>=8,std::to_string(infill)+" infill boxes");
   r.check("six glass panes (one wired, one pre-cracked), one chain-link pane and a frame of members",glass==6&&wired==1&&cracked==1&&meshBoxes(w)==1&&frame>=9,std::to_string(glass)+" panes, "+std::to_string(frame)+" members");
   const auto* crack=w.paneGlass(5);r.check("the pre-cracked pane starts with its cracks, and stands",crack&&crack->hits.size()==3&&w.breakableIntact(5),crack?std::to_string(crack->hits.size())+" hits":"no hits");
   const auto& d=w.breakables()[2];r.check("the dirty, frosted and tinted panes carry their glaze",w.breakables()[1].glaze==Glaze::Dirty&&d.glaze==Glaze::Frosted&&w.breakables()[4].glaze==Glaze::Tinted);}
  // ---- one pane ----
  for(int i:{0,2,4}){Game g(room());const int rounds=bring(g,i);
   r.metric("rounds_to_bring_pane_"+std::to_string(i),rounds);
   r.check("rounds into pane "+std::to_string(i)+" bring it down and leave every other pane unmarked",rounds>0&&marked(g.world(),i)==0,std::to_string(rounds)+" rounds, "+std::to_string(marked(g.world(),i))+" others marked");}
  {Game g(room());const int rounds=bring(g,3);r.metric("rounds_at_wired",rounds);r.check("wired glass webs and holds",rounds<0&&g.world().breakableIntact(3));}
  // ---- mesh ----
  {Game g(room());const World& w=g.world();Point3 c{};for(const auto& s:w.structures())if(s.material==ChainLinkMaterial)c={(s.x1+s.x2)*.5f,(s.y1+s.y2)*.5f,(s.bottom+s.top)*.5f};
   for(int k=0;k<12;++k)LabAccess::fireFrom(g,{c.x,14,c.z},{c.x+.05f*float(k%3-1),c.y,c.z},handgun());
   const auto through=LabAccess::trace(g,{c.x,14,c.z},{0,-1,0},10);const bool blocks=w.structureBlocksHull(c.x,c.y,.3f,0,1.7f);
   r.check("chain-link mesh: rounds go through it, it never shatters, and it stops a body",meshBoxes(w)==1&&marked(w,-1)==0&&blocks&&through.kind!=ShotKind::Solid,
    "trace kind "+std::to_string(int(through.kind))+", blocks "+std::to_string(blocks));}
  // ---- the doorway ----
  {Game g(room());auto& p=LabAccess::player(g);p.pos={9,12.6f};p.angle=-kPi*.5f;for(int i=0;i<150;++i){InputState in;in.forward=true;g.update(in,1.f/60);}
   const float doorY=p.pos.y;Game h(room());auto& q=LabAccess::player(h);q.pos={5,12.6f};q.angle=-kPi*.5f;for(int i=0;i<150;++i){InputState in;in.forward=true;h.update(in,1.f/60);}
   r.metric("door_walk_y",doorY,"m");r.metric("glass_walk_y",q.pos.y,"m");
   r.check("the player walks through the doorway bay; a glass bay stops him",doorY<9.f&&q.pos.y>11.f,"through the door to y "+fixed(doorY)+", at the glass y "+fixed(q.pos.y));}
  // ---- saves ----
  {Game g(room());bring(g,1);const auto c=paneCentre(g.world(),4);LabAccess::fireFrom(g,{c.x,14,c.z},c,handgun());
   const auto bytes=LabAccess::encodeSave(g);Game back(room());const bool loaded=LabAccess::decodeSave(back,bytes);
   bool same=loaded&&back.world().breakables().size()==g.world().breakables().size();
   for(int i=0;same&&i<int(g.world().breakables().size());++i){same&=back.world().breakableIntact(i)==g.world().breakableIntact(i);const auto* a=g.world().paneGlass(i);const auto* b=back.world().paneGlass(i);same&=(!a)==(!b)&&(!a||a->gone==b->gone);}
   r.check("a save keeps the broken pane, the holed one and the pre-cracked one exactly",same&&!back.world().breakableIntact(1)&&back.world().paneGlass(4)&&LabAccess::encodeSave(back)==bytes);}
 },
 .reporter=[](Scene& s,Overlay& o){const World& w=s.g().world();for(int i=0;i<int(w.breakables().size());++i){const auto& b=w.breakables()[size_t(i)];
  o.box({b.x1,b.y1,b.bottom},{b.x2,b.y2,b.top},w.breakableIntact(i)?0x40c0ff:0xff4040);o.label({(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,b.top+.05f},std::to_string(i)+" "+glazeWord(b.glaze));}
  for(const auto& st:w.structures())if(st.material==ChainLinkMaterial)o.box({st.x1,st.y1,st.bottom},{st.x2,st.y2,st.top},0xe0e040);}});
}
}

// What breaks leaves pieces of itself, and the Machine is drawn.
//
// debris: a barrel blown up, a crate blasted, a drop-ceiling tile and boarding
// broken, a wooden prop smashed. Every piece thrown must be a shard (kind 6) cut
// from the thing that broke: drawn with the very sheet that thing is drawn with
// (the v1.4.2 bug: a barrel threw five power supply units, clutter kind 3), its
// mesh section not empty, and visible in the frame.
//
// machine: the cursed equipment (CreatureKind::Machine) drawn in a plain room, in
// the Starter Map's LIFE row (map 3) and in its showcase room (map 40): the
// pixels in its box change when it is taken out of the scene.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// Pixels inside the screen box of [lo,hi] that differ between two frames.
struct BoxDiff {int changed=0,area=0;};
BoxDiff boxDiff(const Scene& s,Point3 lo,Point3 hi,const std::vector<std::uint32_t>& a,const std::uint32_t* b){
 const int W=s.renderer->width(),H=s.renderer->height();float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;bool any=false;
 for(int k=0;k<8;++k){const Point3 p{k&1?hi.x:lo.x,k&2?hi.y:lo.y,k&4?hi.z:lo.z};const auto q=project(s,p);if(!q.visible)continue;any=true;x0=std::min(x0,q.x);y0=std::min(y0,q.y);x1=std::max(x1,q.x);y1=std::max(y1,q.y);}
 BoxDiff d;if(!any)return d;const int ix0=std::clamp(int(x0),0,W-1),ix1=std::clamp(int(x1),0,W-1),iy0=std::clamp(int(y0),0,H-1),iy1=std::clamp(int(y1),0,H-1);
 for(int y=iy0;y<=iy1;++y)for(int x=ix0;x<=ix1;++x){++d.area;const auto p=a[size_t(y*W+x)],q=b[size_t(y*W+x)];
  const int dr=int((p>>16)&255)-int((q>>16)&255),dg=int((p>>8)&255)-int((q>>8)&255),db=int(p&255)-int(q&255);if(std::abs(dr)+std::abs(dg)+std::abs(db)>24)++d.changed;}
 return d;}
std::vector<std::uint32_t> copyFrame(const Scene& s){return {s.renderer->pixels(),s.renderer->pixels()+size_t(s.renderer->width())*s.renderer->height()};}
// Faces of the shard's source mesh that reach into its eighth (what Scene3D clips and draws for it).
int sectionFaces(const Mesh& mesh,int piece){const Point3 c=(mesh.minimum+mesh.maximum)*.5f;int n=0;
 const float side[3]={piece&1?1.f:-1.f,piece&4?1.f:-1.f,piece&2?1.f:-1.f};
 auto along=[&](const Point3& p,int axis){return (axis==0?p.x-c.x:axis==1?p.y-c.y:p.z-c.z)*side[axis];};
 for(const auto& f:mesh.triangles){bool out=false;for(int axis=0;axis<3&&!out;++axis)out=along(f.v[0].p,axis)<0&&along(f.v[1].p,axis)<0&&along(f.v[2].p,axis)<0;if(!out)++n;}return n;}

// ------------------------------------------------------------------ debris
constexpr int BX=8,BY=8;                 // the barrel's / crate's tile
constexpr Vec2 Mid{BX+.5f,BY+.5f};
const bool debris=add({
 .name="debris",.summary="what breaks leaves pieces of itself: a barrel's hull, a crate's splinters, a ceiling tile's slabs, boarding, a wooden prop; each shard drawn with its source's sheet (v1.4.2: barrels threw power supply units)",
 .fixtures={"debris-barrel","debris-crate","debris-ceiling","debris-boarding","debris-prop"},
 .author=[](Scene& s,AuthoredMapData& m){
  const std::string what=s.fixture->value("debris","barrel");
  if(what=="barrel")m.layers[0].rows[BY][BX]='B';
  else if(what=="crate")m.layers[0].rows[BY][BX]='C';
  else if(what=="ceiling")kit::breakable(m,7.5f,7.5f,9.5f,9.5f,2.6f,2.63f,22);
  else if(what=="boarding")kit::breakable(m,8.45f,7.2f,8.55f,9.8f,0,2.2f,10);
  else if(what=="prop")kit::prop(m,model::wooden_table,Mid.x,Mid.y,0,.4f);},
 .step=[](Scene& s,float){
  Game& g=s.g();if(s.frame!=4)return;const std::string what=s.fixture->value("debris","barrel");
  s.board["debris.before"]=double(LabAccess::clutter(g).size());
  if(what=="barrel")LabAccess::detonateBarrel(g,BX,BY);
  else if(what=="crate")LabAccess::blastProps(g,{Mid.x-1.2f,Mid.y,.4f},true);
  else if(what=="ceiling")LabAccess::damagePane(g,0,1e6f,{Mid.x,Mid.y,2.6f},{0,0,1});
  else if(what=="boarding")LabAccess::damagePane(g,0,1e6f,{8.45f,Mid.y,1.1f},{1,0,0});
  else if(what=="prop"){auto& list=LabAccess::clutter(g);for(size_t i=0;i<list.size();++i)if(list[i].model==model::wooden_table){s.board["debris.prop"]=double(i);LabAccess::breakClutter(g,i,{1,0,0});break;}}
  s.board["debris.at"]=s.frame;},
 .capture=[](Scene& s,const Frame& f){
  if(!s.board.count("debris.at"))return;const int since=f.index-int(s.board["debris.at"]);
  for(int k:{2,8,18,40})if(since==k)s.shoot("t"+std::to_string(k));
  if(since!=18)return;
  // The shards drawn: the frame against one with them taken out.
  Game& g=s.g();auto& list=LabAccess::clutter(g);const size_t before=size_t(s.board["debris.before"]);if(list.size()<=before)return;
  const auto with=copyFrame(s);std::vector<Clutter> kept(list.begin()+long(before),list.end());list.resize(before);s.renderer->render(g);
  Point3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};for(const auto& c:kept){const auto e=c.extent();lo={std::min(lo.x,c.pos.x-e[0]),std::min(lo.y,c.pos.y-e[1]),std::min(lo.z,c.z)};hi={std::max(hi.x,c.pos.x+e[0]),std::max(hi.y,c.pos.y+e[1]),std::max(hi.z,c.z+c.height())};}
  const auto d=boxDiff(s,lo,hi,with,s.renderer->pixels());s.board["debris.drawn_px"]=d.changed;s.board["debris.box_px"]=d.area;
  list.insert(list.end(),kept.begin(),kept.end());s.renderer->render(g);},
 .metrics=[](Scene& s,Report& r){
  Game& g=s.g();const std::string what=s.fixture->value("debris","barrel");const auto& list=LabAccess::clutter(g);const size_t before=size_t(s.board["debris.before"]);
  const auto* source=what=="barrel"?LabAccess::tileSheet(*s.renderer,'B'):what=="crate"?LabAccess::tileSheet(*s.renderer,'C'):what=="ceiling"?LabAccess::breakableSheet(*s.renderer,22):what=="boarding"?LabAccess::breakableSheet(*s.renderer,10):LabAccess::modelSheet(*s.renderer,model::wooden_table);
  const ShardOf expect=what=="barrel"?ShardOf::Barrel:what=="ceiling"?ShardOf::Ceiling:what=="prop"?ShardOf::Model:ShardOf::Crate;
  int pieces=0,foreign=0,sheetMatch=0,emptySections=0,meshed=0,below=0;
  for(size_t i=before;i<list.size();++i){const auto& c=list[i];if(c.model>=0)continue;++pieces;
   if(c.kind!=6||c.shardOf!=expect||(expect==ShardOf::Model&&c.shardModel!=model::wooden_table)){++foreign;continue;}
   const auto look=LabAccess::shardLook(*s.renderer,c);if(look.sheet==source)++sheetMatch;
   if(look.mesh){++meshed;if(sectionFaces(*look.mesh,c.piece)==0)++emptySections;
    // The barrel as the tile draws it, against the size its pieces are given (kBarrelSize).
    if(look.barrel){const Point3 range=look.mesh->maximum-look.mesh->minimum;const float u=std::min(1.1f/std::max(.001f,range.y),.94f/std::max({.001f,range.x,range.z}));
     s.board["debris.barrel_w"]=range.x*u;s.board["debris.barrel_d"]=range.z*u;s.board["debris.barrel_h"]=range.y*u;}}
   if(c.z<g.world().floorHeight(c.pos.x,c.pos.y)-.05f)++below;}
  if(expect==ShardOf::Barrel){r.metric("barrel_w",s.board["debris.barrel_w"],"m");r.metric("barrel_d",s.board["debris.barrel_d"],"m");r.metric("barrel_h",s.board["debris.barrel_h"],"m");
   r.check("the pieces are eighths of the barrel as drawn (kBarrelSize within 3 cm)",std::fabs(s.board["debris.barrel_w"]-kBarrelSize[0])<.03&&std::fabs(s.board["debris.barrel_d"]-kBarrelSize[1])<.03&&std::fabs(s.board["debris.barrel_h"]-kBarrelSize[2])<.03,
    fixed(s.board["debris.barrel_w"])+" x "+fixed(s.board["debris.barrel_d"])+" x "+fixed(s.board["debris.barrel_h"])+" m");}
  r.metric("pieces",pieces);r.metric("foreign",foreign);r.metric("sheet_match",sheetMatch);r.metric("meshed",meshed);r.metric("empty_sections",emptySections);
  r.metric("drawn_px",s.board["debris.drawn_px"],"px");r.metric("box_px",s.board["debris.box_px"],"px");
  r.check("it breaks into pieces",pieces>=4,std::to_string(pieces)+" pieces");
  r.check("every piece is a shard of the "+what+" (no unrelated model: v1.4.2 threw power supply units)",foreign==0,std::to_string(foreign)+" foreign");
  r.check("every piece is drawn with the "+what+"'s own sheet (texture id matches the source)",pieces>0&&sheetMatch==pieces,std::to_string(sheetMatch)+"/"+std::to_string(pieces));
  if(expect==ShardOf::Barrel||expect==ShardOf::Model)r.check("each piece is a non-empty section of the "+what+"'s mesh",meshed==pieces&&emptySections==0,std::to_string(meshed)+" meshed, "+std::to_string(emptySections)+" empty");
  r.check("the pieces are drawn (pixels change when they are taken out)",s.board["debris.drawn_px"]>=30,fixed(s.board["debris.drawn_px"],0)+" px of "+fixed(s.board["debris.box_px"],0));
  r.check("no piece falls through the floor",below==0,std::to_string(below)+" below");
  // A save keeps what each piece was cut from (save version 25).
  {Game loaded=g;const bool ok=LabAccess::decodeSave(loaded,LabAccess::encodeSave(g));int kept=0,shards=0;const auto& back=LabAccess::clutter(loaded);
   for(size_t i=before;i<list.size()&&i<back.size();++i){const auto& a=list[i];if(a.model>=0||a.kind!=6)continue;++shards;const auto& b=back[i];
    if(b.kind==6&&b.shardOf==a.shardOf&&b.piece==a.piece&&b.shardModel==a.shardModel&&b.from==a.from&&b.dims==a.dims)++kept;}
   r.check("a save and load keeps every piece as what it was cut from",ok&&back.size()==list.size()&&kept==shards,std::to_string(kept)+"/"+std::to_string(shards)+(ok?"":" (load failed)"));}},
 .reporter=[](Scene& s,Overlay& o){
  Game& g=s.g();const auto& list=LabAccess::clutter(g);const size_t before=size_t(s.board["debris.before"]);
  for(size_t i=before;i<list.size();++i){const auto& c=list[i];if(c.model>=0)continue;const auto e=c.extent();o.box({c.pos.x-e[0],c.pos.y-e[1],c.z},{c.pos.x+e[0],c.pos.y+e[1],c.z+c.height()},c.kind==6?0x40ff80:0xff4040);
   o.label({c.pos.x,c.pos.y,c.z+c.height()+.1f},"SHARD "+std::to_string(int(c.shardOf))+"/"+std::to_string(c.piece),0xc0ffc0);}}});

// ------------------------------------------------------------------ machine
Enemy* machineIn(Game& g){for(auto& e:LabAccess::enemies(g))if(e.kind==CreatureKind::Machine)return &e;return nullptr;}
const bool machine=add({
 .name="machine",.summary="the cursed equipment (CreatureKind::Machine, kind 8) is drawn: in a plain room, in the Starter Map's LIFE row and in its showcase room (map 40)",
 .fixtures={"machine-room","machine-starter-life","machine-starter-40"},
 .capture=[](Scene& s,const Frame&){
  if(!s.last())return;Game& g=s.g();Enemy* e=machineIn(g);if(!e)return;
  const float r=1.f;const Point3 lo{e->pos.x-r,e->pos.y-r,e->bodyBottom()},hi{e->pos.x+r,e->pos.y+r,e->bodyTop()};
  const auto with=copyFrame(s);const Vec2 was=e->pos;e->pos+=Vec2{0,-200.f};s.renderer->render(g);const auto d=boxDiff(s,lo,hi,with,s.renderer->pixels());e->pos=was;s.renderer->render(g);
  s.board["machine.drawn_px"]=d.changed;s.board["machine.box_px"]=d.area;s.board["machine.found"]=1;
  const auto c=project(s,{e->pos.x,e->pos.y,(e->bodyBottom()+e->bodyTop())*.5f});s.board["machine.in_frame"]=c.visible&&c.x>=0&&c.y>=0&&c.x<s.renderer->width()&&c.y<s.renderer->height();
  s.board["machine.height"]=e->bodyTop()-e->bodyBottom();},
 .metrics=[](Scene& s,Report& r){
  r.metric("drawn_px",s.board["machine.drawn_px"],"px");r.metric("box_px",s.board["machine.box_px"],"px");r.metric("height_m",s.board["machine.height"],"m");
  r.check("the map has a Machine (CREATURE kind 8)",s.board["machine.found"]>0);
  r.check("it stands in the camera's frame",s.board["machine.in_frame"]>0);
  const double share=s.board["machine.box_px"]>0?s.board["machine.drawn_px"]/s.board["machine.box_px"]:0;r.metric("drawn_share",share);
  r.check("it is drawn (pixels in its box change when it is taken out)",s.board["machine.drawn_px"]>=200&&share>.04,fixed(s.board["machine.drawn_px"],0)+" px, "+fixed(share*100,1)+"% of its box");},
 .reporter=[](Scene& s,Overlay& o){Game& g=s.g();if(const Enemy* e=machineIn(g)){o.box({e->pos.x-1,e->pos.y-1,e->bodyBottom()},{e->pos.x+1,e->pos.y+1,e->bodyTop()},0x40c0ff);o.label({e->pos.x,e->pos.y,e->bodyTop()+.2f},"MACHINE",0x80e0ff);}}});
}
}

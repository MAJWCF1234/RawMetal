// The underground (CAVE records, src/world/CaveField.h) in lab/fixtures/caves.rmc: a mouth cut into a level
// field, a tunnel down to a cavern six metres under it, a side passage, the east wall pulled out (move),
// smoothed, the floor flattened, the passage's end filled back, a pool in a basin, and a lantern and its
// lamp standing on the cave floor (cave=1).
//
// caves:
//  - the carving renders: cave-rock triangles in the land's mesh, the mine pack's rock on them;
//  - no holes in the walls: inside the map every edge of the land's mesh is shared by two triangles;
//  - the player walks in: from the field, down the mouth and the tunnel, into the cavern;
//  - the rock holds: a roof over the cavern, a wall the player cannot walk into;
//  - the lantern and the lamp stand on the cave floor, not on the field over it;
//  - under the roof it is dark (the sky's light removed; the lamp still lights);
//  - export and load keep every CAVE record, and the same field.
// Pictures (1280 x 720): the mouth from the field, down the tunnel, the cavern by its lamp, the pool.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CampaignExport.h"
#include "world/CustomCampaign.h"
#include "world/ModelIds.h"
#include <cmath>
#include <map>
#include <tuple>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Run {float lowest=99;Vec2 end{};float endZ=0;float dark=0;std::string trace;int leg=0;};
std::map<const Scene*,Run>& runs(){static std::map<const Scene*,Run> r;return r;}
constexpr Vec2 Path[]={{4,12},{9,12},{13,12},{16,12.6f}};
void view(Scene& s,const char* label,Vec2 at,float z,float yawDeg,float pitch){Game& g=s.g();LabAccess::setCamera(g,at,z,yawDeg*kPi/180.f,pitch);s.renderer->render(g);s.shoot(label);LabAccess::playerCamera(g);}
const bool tube=add({
 .name="caves",.summary="CAVE records: a mouth, a tunnel and a cavern carved into the land render as cave rock with no holes, the player walks in, the rock holds, cave=1 things stand on the cave floor, it is dark underground, and export keeps them",
 .fixtures={"caves"},
 .setup=[](Scene& s){runs()[&s]=Run{};},
 .step=[](Scene& s,float){auto& r=runs()[&s];Game& g=s.g();auto& p=LabAccess::player(g);
  // Walk the path: turn to the next point, forward.
  if(s.frame<400){if(r.leg<3&&std::hypot(Path[r.leg].x-p.pos.x,Path[r.leg].y-p.pos.y)<.6f)++r.leg;const Vec2 to=Path[r.leg];
   p.angle=std::atan2(to.y-p.pos.y,to.x-p.pos.x);p.pitch=0;s.input.forward=std::hypot(to.x-p.pos.x,to.y-p.pos.y)>.4f;}
  if(s.frame%25==0){const Vec2 ahead=p.pos+Vec2{std::cos(p.angle),std::sin(p.angle)}*.35f;r.trace+=fixed(p.pos.x)+","+fixed(p.z)+"/"+std::to_string(LabAccess::moveBlock(g,ahead))+" ";}
  r.lowest=std::min(r.lowest,p.z);r.end=p.pos;r.endZ=p.z;r.dark=g.darkness();},
 .capture=[](Scene& s,const Frame&){if(!s.last())return;
  const World& w=s.g().world();auto floor=[&](float x,float y){return w.caveFloor(x,y);};
  view(s,"the mouth from the field",{1.2f,13.4f},floor(1.2f,13.4f),-8,-10);
  view(s,"down the tunnel",{6,12},floor(6,12),0,-16);
  view(s,"the cavern by its lamp",{12.6f,11.4f},floor(12.6f,11.4f),25,-6);
  view(s,"the pool",{16,12.6f},floor(16,12.6f),-90,-28);},
 .metrics=[](Scene& s,Report& rep){auto& r=runs()[&s];Game& g=s.g();const World& w=g.world();
  const auto* shape=w.terrainShape();
  rep.check("the campaign's land has its underground",shape&&shape->caves.size()==8&&shape->caveVolume,shape?std::to_string(shape->caves.size())+" strokes":"no land");
  // ---- renders ----
  int cave=0;for(const auto& t:w.terrain())cave+=t.material==TerrainCave;
  rep.metric("cave_triangles",cave);rep.check("the carving renders as cave rock",cave>200,std::to_string(cave)+" cave-rock triangles");
  // ---- watertight ----
  {std::map<std::tuple<long,long,long,long,long,long>,int> edges;auto key=[](const TerrainVertex& a){return std::tuple<long,long,long>{std::lround(a.x*1000),std::lround(a.y*1000),std::lround(a.z*1000)};};
   for(const auto& t:w.terrain()){const TerrainVertex* v[3]={&t.a,&t.b,&t.c};for(int i=0;i<3;++i){auto a=key(*v[i]),b=key(*v[(i+1)%3]);if(b<a)std::swap(a,b);
    edges[{std::get<0>(a),std::get<1>(a),std::get<2>(a),std::get<0>(b),std::get<1>(b),std::get<2>(b)}]++;}}
   int open=0;std::string where;for(const auto& [e,n]:edges){const float x0=std::get<0>(e)/1000.f,y0=std::get<1>(e)/1000.f,x1=std::get<3>(e)/1000.f,y1=std::get<4>(e)/1000.f;
    if(std::min(x0,x1)<1.f||std::max(x0,x1)>23.f||std::min(y0,y1)<1.f||std::max(y0,y1)>23.f)continue;if(n%2){++open;if(open<6)where+=fixed(x0)+","+fixed(y0)+","+fixed(std::get<2>(e)/1000.f)+"x"+std::to_string(n)+" ";}}
   rep.metric("open_edges",open);rep.check("no holes: every edge of the land inside the map joins two triangles",open==0,std::to_string(open)+" open edges of "+std::to_string(edges.size())+" "+where);}
  // ---- walked in ----
  rep.metric("lowest_feet",r.lowest);rep.check("trace",true,r.trace);
  {std::string prof;for(float x=2;x<=16;x+=1){const float f=w.caveFloor(x,12);prof+=fixed(x,0)+":"+fixed(f)+"/"+fixed(w.terrainCeilingAt(x,12,f+.1f))+" ";}rep.check("profile",true,prof);}
  rep.check("the player walks down the mouth and the tunnel into the cavern",std::hypot(r.end.x-Path[3].x,r.end.y-Path[3].y)<1.2f&&r.endZ<-2.6f,
   "ended at "+fixed(r.end.x)+", "+fixed(r.end.y)+" feet "+fixed(r.endZ));
  // ---- the rock holds ----
  {const float floor=w.caveFloor(16,12);const float roof=w.terrainCeilingAt(16,12,floor+.1f);const float wall=w.clearanceAbove(16,4.6f,floor+.05f);
   rep.metric("cavern_floor",floor);rep.metric("cavern_roof",roof);
   rep.check("a roof over the cavern and a wall the player cannot walk into",roof<2.4f&&roof-floor>2.2f&&wall<floor+1.f,
    "floor "+fixed(floor)+", roof "+fixed(roof)+", clearance in the wall "+fixed(wall-floor));}
  // ---- on the cave floor ----
  {const retro::Fixture* lantern=nullptr;for(const auto& f:w.fixtures())if(f.model==model::mine_lantern)lantern=&f;
   const float floor=w.caveFloor(16,14.2f),field=w.floorHeight(16,14.2f);
   const float stands=lantern?w.floorHeight(lantern->position.x,lantern->position.y)+lantern->base:99.f;
   const float lamp=w.lights().empty()?99.f:w.lights().front().z;
   rep.check("the lantern and its lamp stand on the cave floor, not on the field over it",lantern&&std::abs(stands-floor)<.05f&&std::abs(lamp-(floor+1.2f))<.05f&&field-floor>3.f,
    "floor "+fixed(floor)+", field "+fixed(field)+", lantern "+fixed(stands)+", lamp "+fixed(lamp));}
  // ---- dark underground ----
  rep.check("under the roof it is dark",r.dark>.9f&&w.underRock(16,12,w.caveFloor(16,12)+1.6f)&&!w.underRock(-1.f+1.5f,4,4),"darkness "+fixed(r.dark));
  // ---- export ----
  {const auto c=LabAccess::campaignOf(g);const auto text=exportRmc(c->name,{{c->maps[0],-1}},0).text;int lines=0;for(size_t at=text.find("\nCAVE|");at!=std::string::npos;at=text.find("\nCAVE|",at+1))++lines;
   const auto again=loadCustomCampaignText(text,"caves-again.rmc");const auto& t=again->maps[0]->terrain;
   const bool same=t&&t->caves==shape->caves&&t->caveVolume&&t->caveVolume->density==shape->caveVolume->density;
   rep.check("export and load keep every CAVE record and the same field",lines==8&&same,std::to_string(lines)+" CAVE lines");}}});
}
}

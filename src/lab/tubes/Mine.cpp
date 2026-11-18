// The Starter Map's underground (tools/campaign/make_starter_map.py underground): a hill east of the
// Parts Gallery, a cave mouth in its west face, a timbered drift with rails and an ore cart, a crystal
// cavern, a passage south and a cave pool, carved with CAVE records and dressed with the 1.5.3 parts.
//
// mine: from the shipped Starter Map:
//  - the player walks from the hill's foot in through the mouth, down the drift, into the crystal cavern;
//  - every cave=1 thing stands on the cave floor under it, well under the hill's top;
//  - the lanterns' lamps are lit down there, and it is dark under the rock;
//  - the pool's water lies in its basin, under the cavern's floor level and over its bed.
// Pictures (1280 x 720): the mouth from the hill's foot, the drift with its cart, the crystal cavern,
// the pool.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CustomCampaign.h"
#include "world/ModelIds.h"
#include <cmath>
#include <filesystem>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
std::shared_ptr<const CustomCampaign> starter(){static auto c=loadCustomCampaignFile(std::filesystem::path(labRoot())/"custom maps/Starter Map.rmc");return c;}
int levelNamed(const char* name){const auto c=starter();if(!c)return -1;for(size_t i=0;i<c->maps.size();++i)if(c->maps[i]->name==std::string("Starter Map / ")+name)return int(i);return -1;}
struct Run {int leg=0;Vec2 end{};float endZ=0,dark=0;int endLevel=-1;};
std::map<const Scene*,Run>& runs(){static std::map<const Scene*,Run> r;return r;}
// Map metres on the mouth's map; the last two run on into the works' map (24 m east).
constexpr Vec2 Path[]={{2,14},{8,14},{14,13.6f},{20,13.4f},{26,13.2f},{31,13.6f}};
void view(Scene& s,const char* label,int level,Vec2 at,float yawDeg,float pitch){
 auto g=std::make_unique<Game>(starter());LabAccess::loadLevel(*g,level);const float z=g->world().caveFloor(at.x,at.y);
 LabAccess::setCamera(*g,at,z,yawDeg*kPi/180.f,pitch);s.renderer->render(*g);s.shoot(label);}
const bool tube=add({
 .name="mine",.summary="the Starter Map's underground as shipped: walk in through the cave mouth, down the drift to the crystal cavern; everything stands on the cave floor, the lanterns light it, it is dark under the rock, and the pool lies in its basin",
 .friends={"caves"},.fixtures={"starter-mine"},
 .setup=[](Scene& s){runs()[&s]=Run{};},
 .step=[](Scene& s,float){auto& r=runs()[&s];Game& g=s.g();auto& p=LabAccess::player(g);
  const float east=float(g.level()==levelNamed("Mine Works")?24:0);   // the works' map is 24 m east of the mouth's
  if(s.frame<560){if(r.leg<5&&std::hypot(Path[r.leg].x-east-p.pos.x,Path[r.leg].y-p.pos.y)<.6f)++r.leg;const Vec2 to{Path[r.leg].x-east,Path[r.leg].y};
   p.angle=std::atan2(to.y-p.pos.y,to.x-p.pos.x);p.pitch=0;s.input.forward=std::hypot(to.x-p.pos.x,to.y-p.pos.y)>.4f;}
  r.end=p.pos;r.endZ=p.z;r.dark=g.darkness();r.endLevel=g.level();},
 .capture=[](Scene& s,const Frame&){if(!s.last())return;const int mouth=levelNamed("Mine Mouth"),works=levelNamed("Mine Works"),pool=levelNamed("Forest");(void)pool;
  view(s,"the mouth from the hill's foot",mouth,{2,13.2f},0,-4);
  view(s,"the drift and its cart",works,{.4f,13.6f},0,-8);
  view(s,"the crystal cavern",works,{7.4f,13.2f},10,-6);},
 .metrics=[](Scene& s,Report& rep){auto& r=runs()[&s];
  const int mouth=levelNamed("Mine Mouth"),works=levelNamed("Mine Works");
  rep.check("the Starter Map has its mine",mouth>=0&&works>=0,std::to_string(mouth)+", "+std::to_string(works));if(mouth<0||works<0)return;
  rep.check("the player walks in through the mouth, down the drift, into the crystal cavern",r.endLevel==works&&std::hypot(r.end.x-7,r.end.y-13.6f)<1.3f&&r.endZ<-3.f,
   "ended on map "+std::to_string(r.endLevel)+" at "+fixed(r.end.x)+", "+fixed(r.end.y)+" feet "+fixed(r.endZ));
  rep.check("under the rock it is dark",r.dark>.9f,"darkness "+fixed(r.dark));
  // Everything with cave=1 on the cave floor, on every map of the mine.
  int placed=0,off=0,lamps=0;std::string worst;
  const auto c=starter();
  for(int level=0;level<int(c->maps.size());++level){const auto& m=*c->maps[size_t(level)];bool mine=false;
   for(const auto& [key,fields]:m.recordFields)for(const auto& f:fields)mine|=f.key=="cave";if(!mine)continue;
   auto g=std::make_unique<Game>(c);LabAccess::loadLevel(*g,level);const World& w=g->world();
   for(size_t i=0;i<w.fixtures().size()&&i<m.fixtures.size();++i){const auto it=m.recordFields.find(recordFieldKey("FIXTURE","",int(i)));if(it==m.recordFields.end()||!findNamedField(it->second,"cave"))continue;
    const auto& f=w.fixtures()[i];const float floor=w.caveFloor(f.position.x,f.position.y),stands=w.floorHeight(f.position.x,f.position.y)+f.base,top=w.floorHeight(f.position.x,f.position.y);++placed;
    if(std::abs(stands-floor)>.05f||top-floor<2.f){++off;worst=std::to_string(f.model)+" at "+fixed(f.position.x)+","+fixed(f.position.y)+" stands "+fixed(stands)+" floor "+fixed(floor)+" top "+fixed(top);}}
   for(const auto& l:w.lights())lamps+=l.z<w.floorHeight(l.position.x,l.position.y)-1.5f;}
  rep.metric("cave_things",placed);rep.metric("cave_lamps",lamps);
  rep.check("every cave=1 thing stands on the cave floor, well under the hill",placed>=20&&off==0,std::to_string(placed)+" placed, "+std::to_string(off)+" off"+(worst.empty()?"":": "+worst));
  rep.check("lanterns light it",lamps>=4,std::to_string(lamps)+" lamps underground");
  // The pool: water in its basin.
  int pools=0;for(int level=0;level<int(c->maps.size());++level){if(c->maps[size_t(level)]->waterVolumes.empty()||c->maps[size_t(level)]->waterVolumes.front().surface>-2)continue;auto g=std::make_unique<Game>(c);
   LabAccess::loadLevel(*g,level);const World& w=g->world();const auto& v=c->maps[size_t(level)]->waterVolumes.front();const float cx=(v.x1+v.x2)*.5f,cy=(v.y1+v.y2)*.5f;
   const float bed=w.caveFloor(cx,cy);if(bed<v.surface-.3f&&w.floorHeight(cx,cy)-v.surface>2.f)++pools;}
  rep.check("the pool's water lies in its basin",pools==1,std::to_string(pools)+" cave pools");}});
}
}

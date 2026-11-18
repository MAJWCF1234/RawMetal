// The Parts Gallery (tools/campaign/make_starter_map.py parts_gallery): the 76 models imported for 1.5.3
// from the purchased packs (docs/Assets/library-parts-1.5.3.md, slots bld_wall_a..cave_gem_pile_red).
//
// parts: from the shipped Starter Map:
//  - both gallery yards are there, and every one of the 76 stands in them once;
//  - every one has a mesh with triangles, and no material of any is a 4x4 colour swatch (a texture the
//    import could not find), so none renders as a grey sheet;
//  - each stands on the yard (its base on the levelled ground, nothing sunk or floating).
// Pictures (1280 x 720): the north yard from its gate, walls and stairs close, the yard from above.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CustomCampaign.h"
#include "world/ModelIds.h"
#include <cmath>
#include <filesystem>
#include <set>
namespace retro::lab {
namespace {
constexpr int First=model::bld_wall_a,Last=model::cave_gem_pile_red;
std::shared_ptr<const CustomCampaign> starter(){static auto c=loadCustomCampaignFile(std::filesystem::path(labRoot())/"custom maps/Starter Map.rmc");return c;}
int levelNamed(const char* name){const auto c=starter();if(!c)return -1;for(size_t i=0;i<c->maps.size();++i)if(c->maps[i]->name=="Starter Map / "+std::string(name))return int(i);return -1;}
void view(Scene& s,const char* label,Vec2 at,float z,float yawDeg,float pitch){Game& g=s.g();LabAccess::setCamera(g,at,z,yawDeg*kPi/180.f,pitch);s.renderer->render(g);s.shoot(label);}
const bool tube=add({
 .name="parts",.summary="the Parts Gallery as shipped: all 76 models imported for 1.5.3 stand in it, each with its mesh and real textures (no 4x4 swatch), on the level yard",
 .fixtures={"parts-gallery"},
 .capture=[](Scene& s,const Frame&){if(!s.last())return;
  view(s,"the north yard from its gate",{12,1.2f},0,90,-8);
  view(s,"walls, pillars and stairs",{4,9},2.5f,60,-18);
  view(s,"the north yard from above",{12,-2},9,90,-38);},
 .metrics=[](Scene& s,Report& rep){
  const int north=levelNamed("Parts Gallery"),south=levelNamed("Parts Gallery 2");
  rep.check("the Starter Map has both gallery yards",north>=0&&south>=0,std::to_string(north)+", "+std::to_string(south));if(north<0||south<0)return;
  std::multiset<int> seen;int floating=0;std::string worst;
  for(int level:{north,south}){auto g=std::make_unique<Game>(starter());LabAccess::loadLevel(*g,level);const World& w=g->world();
   for(const auto& f:w.fixtures())if(f.model>=First&&f.model<=Last){seen.insert(f.model);
    const float ground=w.floorHeight(f.position.x,f.position.y);if(std::abs(f.base)>.02f||std::abs(ground)>.6f){++floating;worst=std::to_string(f.model)+" base "+std::to_string(f.base)+" ground "+std::to_string(ground);}}}
  int missing=0,twice=0;for(int m=First;m<=Last;++m){const auto n=seen.count(m);missing+=n==0;twice+=n>1;}
  rep.metric("parts",Last-First+1);rep.metric("placed",int(seen.size()));
  rep.check("every one of the 76 parts stands in the gallery once",missing==0&&twice==0&&Last-First+1==76,std::to_string(missing)+" missing, "+std::to_string(twice)+" twice");
  rep.check("each stands on the level yard",floating==0,floating?worst:"all at base 0 on ground within 0.6 m of level");
  int empty=0,swatches=0;std::string which;
  for(int m=First;m<=Last;++m){const auto& mesh=LabAccess::facilityMesh(*s.renderer,m);if(mesh.triangles.empty()){++empty;which+=" mesh"+std::to_string(m);}
   for(int part=0;part<int(mesh.materialNames.size());++part){const auto* t=LabAccess::modelPart(*s.renderer,m,part);if(!t||t->width<=4||t->height<=4){++swatches;which+=" "+mesh.materialNames[size_t(part)];}}}
  rep.metric("swatches",swatches);
  rep.check("every part has a mesh and real textures (no 4x4 swatch)",empty==0&&swatches==0,which.empty()?"all textured":which);}});
}
}

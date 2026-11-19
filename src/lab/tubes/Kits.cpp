// The Kits Gallery and the 1.5.6 parts (tools/campaign/make_starter_map.py kits_gallery, parts_gallery):
// the Campaign Builder's level-design kits (tools/campaign/kits.py) set down as PREFAB_PLACEs on maps 126
// and 127, and the 104 models imported for 1.5.6 (docs/Assets/library-parts-1.5.6.md, slots
// fur_desk_worn..out_sawbuck) standing on Parts Gallery 3 and 4.
//
// kits: from the shipped Starter Map:
//  - every kit has its PREFAB block, and each is placed once in the Kits Gallery's two yards;
//  - the kits' pieces stand in the game (fixtures, boxes and lamps on both yards);
//  - every 1.5.6 model stands in Parts Gallery 3 or 4 once, has a mesh with triangles, and no material of
//    any is a 4x4 colour swatch (a texture the import could not find);
//  - every 1.5.6 model renders: a close-up from its front, the eye at its height, finds its surface in the
//    depth buffer where it stands (models under 10 cm tall are checked by mesh and texture only).
// Pictures (1280 x 720): the rooms from above and from their doors, inside the office, the second yard,
// the loading dock, the campsite, and Parts Gallery 3 and 4 from above.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "renderer/Projection.h"
#include "world/CustomCampaign.h"
#include "world/ModelIds.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
namespace retro::lab {
namespace {
constexpr int First=model::fur_desk_worn,Last=model::out_sawbuck;
std::shared_ptr<const CustomCampaign> starter(){static auto c=loadCustomCampaignFile(std::filesystem::path(labRoot())/"custom maps/Starter Map.rmc");return c;}
int levelNamed(const char* name){const auto c=starter();if(!c)return -1;for(size_t i=0;i<c->maps.size();++i)if(c->maps[i]->name=="Starter Map / "+std::string(name))return int(i);return -1;}
void look(Scene& s,Game& g,const char* label,Vec2 at,float z,float yawDeg,float pitch){LabAccess::setCamera(g,at,z,yawDeg*kPi/180.f,pitch);s.renderer->render(g);s.shoot(label);}
void elsewhere(Scene& s,const char* map,const char* label,Vec2 at,float z,float yawDeg,float pitch){
 const int level=levelNamed(map);if(level<0)return;auto g=std::make_unique<Game>(starter());LabAccess::loadLevel(*g,level);look(s,*g,label,at,z,yawDeg,pitch);}
const bool tube=add({
 .name="kits",.summary="the Kits Gallery as shipped: every Builder kit has its PREFAB block and is placed once as a PREFAB_PLACE; their pieces stand in the game",
 .fixtures={"kits-gallery"},
 .capture=[](Scene& s,const Frame&){if(!s.last())return;
  // The second yard first: its campfire flickers, so the tube's own map is the last thing drawn (the
  // repeat check compares that).
  elsewhere(s,"Kits Gallery 2","corridors, stairs, dock and outdoors from above",{12,23.5f},13,-90,-95);
  elsewhere(s,"Kits Gallery 2","the loading dock",{8.6f,10.2f},-999,-118,-12);
  elsewhere(s,"Kits Gallery 2","the campsite",{14.2f,16.4f},-999,38,-16);
  elsewhere(s,"Parts Gallery 3","Parts Gallery 3 (1.5.6 parts) from above",{12,23.5f},13,-90,-95);
  elsewhere(s,"Parts Gallery 4","Parts Gallery 4 (1.5.6 parts) from above",{12,23.5f},13,-90,-95);
  look(s,s.g(),"the rooms from above",{12,23.5f},13,-90,-95);
  look(s,s.g(),"office and ward from their doors",{8.25f,9.6f},-999,-120,-6);
  look(s,s.g(),"inside the office",{4.5f,6.2f},-999,-90,-12);
  look(s,s.g(),"bathroom, storage and control room",{12,16.f},-999,-90,-10);},
 .metrics=[](Scene& s,Report& rep){
  const int kits[]={levelNamed("Kits Gallery"),levelNamed("Kits Gallery 2")},parts[]={levelNamed("Parts Gallery 3"),levelNamed("Parts Gallery 4")};
  rep.check("the Starter Map has both kit yards and both 1.5.6 part yards",kits[0]>=0&&kits[1]>=0&&parts[0]>=0&&parts[1]>=0,
   std::to_string(kits[0])+", "+std::to_string(kits[1])+", "+std::to_string(parts[0])+", "+std::to_string(parts[1]));
  if(kits[0]<0||kits[1]<0||parts[0]<0||parts[1]<0)return;
  // The file as shipped: one block per kit, each placed once on the two yards.
  std::ifstream in(std::filesystem::path(labRoot())/"custom maps/Starter Map.rmc");std::set<std::string> blocks;std::multiset<std::string> placed;
  for(std::string line;std::getline(in,line);){if(!line.empty()&&line.back()=='\r')line.pop_back();
   if(line.rfind("PREFAB|kit_",0)==0)blocks.insert(line.substr(7));
   for(int k:kits)if(line.rfind("PREFAB_PLACE|"+std::to_string(k)+"|",0)==0){auto rest=line.substr(line.find('|',13)+1);placed.insert(rest.substr(0,rest.find('|')));}}
  int once=0;for(const auto& b:blocks)once+=placed.count(b)==1;
  rep.metric("kits",int(blocks.size()));
  rep.check("every kit has its block and stands once in the Kits Gallery",blocks.size()>=21&&once==int(blocks.size())&&placed.size()==blocks.size(),
   std::to_string(blocks.size())+" blocks, "+std::to_string(placed.size())+" placed, "+std::to_string(once)+" once");
  int fixtures=0,lamps=0;for(int k:kits){auto g=std::make_unique<Game>(starter());LabAccess::loadLevel(*g,k);fixtures+=int(g->world().fixtures().size());lamps+=int(g->world().lights().size());}
  rep.metric("kit fixtures",fixtures);rep.metric("kit lamps",lamps);
  rep.check("the kits' pieces stand in the game",fixtures>=150&&lamps>=25,std::to_string(fixtures)+" fixtures, "+std::to_string(lamps)+" lamps");
}});
const bool parts156=add({
 .name="parts156",.summary="the 104 models imported for 1.5.6 as shipped in Parts Gallery 3 and 4: each stands once on the level yard, has its mesh and real textures (no 4x4 swatch), and is drawn where it stands (a software close-up's depth)",
 .fixtures={"parts-gallery-156"},
 .capture=[](Scene& s,const Frame&){if(!s.last())return;
  look(s,s.g(),"Parts Gallery 3 from its south end (software)",{12,23.f},-999,-90,-8);},
 .metrics=[](Scene& s,Report& rep){
  const int parts[]={levelNamed("Parts Gallery 3"),levelNamed("Parts Gallery 4")};
  rep.check("the Starter Map has both 1.5.6 part yards",parts[0]>=0&&parts[1]>=0,std::to_string(parts[0])+", "+std::to_string(parts[1]));
  if(parts[0]<0||parts[1]<0)return;
  // The 1.5.6 models: in the part yards once, meshed, textured, and drawn.
  std::multiset<int> seen;int floating=0,flat=0,drawn=0,tall=0;std::string worst,unseen,first;
  for(int level:parts){auto g=std::make_unique<Game>(starter());LabAccess::loadLevel(*g,level);const World& w=g->world();
   float eye=-1;
   for(const auto& f:w.fixtures()){if(f.model<First||f.model>Last)continue;seen.insert(f.model);
    const float ground=w.floorHeight(f.position.x,f.position.y);if(std::abs(f.base)>.02f||std::abs(ground)>.6f){++floating;worst=std::to_string(f.model)+" base "+std::to_string(f.base);}
    if(f.height<.1f){++flat;continue;}
    ++tall;
    // A close-up from its front, the eye at 60% of its height: rays at or above the eye cannot meet the
    // ground, so a depth inside the model's own slab is the model drawn.
    const Vec2 front{-std::sin(f.yaw),std::cos(f.yaw)};const float D=std::max(.9f,f.depth*.5f+.6f); // 0.6 m off its face: inside the 1.1 m gap before the next row
    const Vec2 at=f.position+front*D;const float angle=std::atan2(-front.y,-front.x);
    if(eye<0){LabAccess::setCamera(*g,at,ground,angle,0);s.renderer->render(*g);
     const Vec2 ahead=at-front*5.f;const auto c0=LabAccess::cameraPoint(*s.renderer,{ahead.x,ahead.y,ground}),c1=LabAccess::cameraPoint(*s.renderer,{ahead.x,ahead.y,ground+10});
     eye=std::abs(c1.y-c0.y)>1e-4f?10.f*(-c0.y)/(c1.y-c0.y):1.6f;}
    const float target=ground+f.height*.6f;LabAccess::setCamera(*g,at,target-eye,angle,0);s.renderer->render(*g);
    const int W=s.renderer->width(),H=s.renderer->height();const float focal=Projection{float(W)/H}.focal(float(W));
    float x0=1e9f,x1=-1e9f,y0=1e9f,y1=-1e9f;const float c=std::cos(f.yaw),sn=std::sin(f.yaw);
    for(int k=0;k<8;++k){const float lx=(k&1?.5f:-.5f)*f.width,ly=(k&2?.5f:-.5f)*f.depth,lz=k&4?f.height:0.f;
     const Point3 p{f.position.x+lx*c-ly*sn,f.position.y+lx*sn+ly*c,ground+lz};const auto q=LabAccess::cameraPoint(*s.renderer,p);if(q.z<.05f)continue;
     const float sx=W*.5f+q.x*focal/q.z,sy=H*.5f-q.y*focal/q.z;x0=std::min(x0,sx);x1=std::max(x1,sx);y0=std::min(y0,sy);y1=std::max(y1,sy);}
    x0=std::max(x0,0.f);x1=std::min(x1,float(W-1));y0=std::max(y0,0.f);y1=std::min(y1,H*.5f);
    int hits=0;if(x1>x0&&y1>y0)for(int i=0;i<=32;++i)for(int j=0;j<=32;++j){const int px=int(x0+(x1-x0)*i/32.f),py=int(y0+(y1-y0)*j/32.f);
     const float z=LabAccess::depthAt(*s.renderer,px,py);if(z>D-f.depth*.5f-.3f&&z<D+f.depth*.5f+.3f)++hits;}
    if(hits>=2)++drawn;else{unseen+=" "+std::to_string(f.model);if(first.empty())first=" (first: eye "+std::to_string(eye)+", D "+std::to_string(D)+", box "+std::to_string(int(x0))+"-"+std::to_string(int(x1))+" x "+std::to_string(int(y0))+"-"+std::to_string(int(y1))+", centre depth "+std::to_string(LabAccess::depthAt(*s.renderer,W/2,H/2))+")";}}}
  int missing=0,twice=0;std::string absent;for(int m=First;m<=Last;++m){const auto n=seen.count(m);missing+=n==0;twice+=n>1;if(!n)absent+=" "+std::to_string(m);}
  rep.metric("1.5.6 models",Last-First+1);rep.metric("in the yards",int(seen.size()));rep.metric("drawn in close-up",drawn);rep.metric("flat (mesh and texture only)",flat);
  rep.check("every 1.5.6 model stands in Parts Gallery 3 or 4 once",missing==0&&twice==0,std::to_string(missing)+" missing ("+absent+"), "+std::to_string(twice)+" twice");
  rep.check("each stands on the level yard",floating==0,floating?worst:"all at base 0 on ground within 0.6 m of level");
  rep.check("every 1.5.6 model 10 cm or taller is drawn where it stands (close-up depth)",drawn==tall&&tall>0,std::to_string(drawn)+" of "+std::to_string(tall)+(unseen.empty()?"":"; not seen:"+unseen+first));
  int empty=0,swatches=0;std::string which;
  for(int m=First;m<=Last;++m){const auto& mesh=LabAccess::facilityMesh(*s.renderer,m);if(mesh.triangles.empty()){++empty;which+=" mesh"+std::to_string(m);}
   for(int part=0;part<int(mesh.materialNames.size());++part){const auto* t=LabAccess::modelPart(*s.renderer,m,part);if(!t||t->width<=4||t->height<=4){++swatches;which+=" "+mesh.materialNames[size_t(part)];}}}
  rep.metric("swatches",swatches);
  rep.check("every 1.5.6 model has a mesh and real textures (no 4x4 swatch)",empty==0&&swatches==0,which.empty()?"all textured":which);}});
}
}

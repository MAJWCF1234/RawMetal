// Authored gibs (src/world/GibSets.h, made by tools/library/make_gibs.py in Blender): a prop with a set breaks into
// its own pieces, each a prop of its own on Vectra. Fixtures lab/fixtures/gibs-*.lab, `prop`:
//  wooden_chair, scrap_chair, wooden_crate, packing_crate, glass_bottle, crt_monitor: broken where it stands. The
//    pieces are the set's (count and models), assemble back into the prop at the break (the frame barely changes),
//    keep its mass, fly apart, fall and come to rest on the floor asleep, and survive a save and load.
//  crush: a 9 kg crate dropped on a glass bottle: the blow by weight breaks it into its neck and shards.
//  table: a wooden table (no set) still breaks into slices of itself (the fallback).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include "world/GibSets.h"
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string f2(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Watch {std::map<std::string,double> v;int prop=-1;size_t before=0;int model=-1;std::vector<std::uint32_t> whole;Point3 lo{},hi{};float mass=0;};
std::map<const Scene*,Watch>& watches(){static std::map<const Scene*,Watch> w;return w;}
int modelNamed(const std::string& n){if(n=="wooden_chair")return model::wooden_chair;if(n=="scrap_chair")return model::scrap_chair;if(n=="wooden_crate")return model::wooden_crate;
 if(n=="packing_crate")return model::packing_crate;if(n=="glass_bottle"||n=="crush")return model::glass_bottle;if(n=="crt_monitor")return model::crt_monitor;if(n=="table")return model::wooden_table;return -1;}
constexpr float X=8.5f,Y=8.5f;
const bool registered=add({
 .name="gibs",.summary="authored gibs: chairs, crates, a bottle and a monitor break into pieces cut from their own meshes in Blender, as Vectra props that assemble back into the prop, keep its mass, settle and save; a crate dropped on a bottle crushes it into its neck and shards; a table without a set is still sliced",
 .fixtures={"gibs-wooden_chair","gibs-scrap_chair","gibs-wooden_crate","gibs-packing_crate","gibs-glass_bottle","gibs-crt_monitor","gibs-crush","gibs-table"},
 .author=[](Scene& s,AuthoredMapData& m){const std::string what=s.fixture->value("prop");const int id=modelNamed(what);
  if(id==model::wooden_crate)kit::prop(m,id,X,Y,0,.3f,.6f);else if(id==model::packing_crate)kit::prop(m,id,X,Y,0,.3f,.5f);
  else if(id==model::crt_monitor)kit::prop(m,id,X,Y,0,.42f,.4f,.38f,.3f);else if(id==model::wooden_chair)kit::prop(m,id,X,Y,0,.417f,.525f,.95f,.5f);
  else if(id==model::wooden_table)kit::prop(m,id,X,Y,0,1.2f,.7f,.75f,.2f);else if(id>=0)kit::prop(m,id,X,Y,0,.5f);
  if(what=="crush")kit::prop(m,model::wooden_crate,X,Y,1.4f,.6f,.6f,.6f,.2f);},
 .setup=[](Scene& s){watches().erase(&s);auto& w=watches()[&s];Game& g=s.g();LabAccess::setGod(g,true);w.model=modelNamed(s.fixture->value("prop"));
  const auto& l=LabAccess::clutter(g);for(int i=0;i<int(l.size());++i)if(l[size_t(i)].model==w.model){w.prop=i;w.mass=l[size_t(i)].mass();}},
 .step=[](Scene& s,float){auto& w=watches()[&s];Game& g=s.g();const std::string what=s.fixture->value("prop");auto& list=LabAccess::clutter(g);if(w.prop<0)return;
  if(what=="crush"){if(list[size_t(w.prop)].broken&&!w.v.count("brokeAt")){w.v["brokeAt"]=s.frame;w.before=0;}return;}
  if(s.frame!=10)return;
  // The prop whole, drawn now; then broken, its pieces landed, and drawn again before anything moves.
  s.renderer->render(g);w.whole.assign(s.renderer->pixels(),s.renderer->pixels()+size_t(s.renderer->width())*s.renderer->height());
  const auto& c=list[size_t(w.prop)];const auto e=c.extent();w.lo={c.pos.x-e[0],c.pos.y-e[1],c.z};w.hi={c.pos.x+e[0],c.pos.y+e[1],c.z+2*e[2]};
  w.before=list.size();LabAccess::breakClutter(g,size_t(w.prop),{1,.3f,0});
  // Each piece put back to rest where it was cut from (the break's own throw taken off for the picture).
  std::vector<Clutter> moving(list.begin()+long(w.before),list.end());for(size_t k=w.before;k<list.size();++k){auto& p=list[k];p.velocity={};p.vz=0;}
  // (The break's own chips, splash and the broken prop's glass grit are left out of this picture: it is of the pieces.)
  auto& tomb=list[size_t(w.prop)];const Vec2 keep=tomb.pos;tomb.pos.x+=1000;const auto impacts=LabAccess::impacts(g);const auto events=LabAccess::breakEvents(g);LabAccess::impacts(g).clear();LabAccess::breakEvents(g).clear();
  s.renderer->render(g);const int W=s.renderer->width(),H=s.renderer->height();
  tomb.pos=keep;LabAccess::impacts(g)=impacts;LabAccess::breakEvents(g)=events;
  float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;for(int k=0;k<8;++k){const auto q=project(s,{k&1?w.hi.x:w.lo.x,k&2?w.hi.y:w.lo.y,k&4?w.hi.z:w.lo.z});if(!q.visible)continue;x0=std::min(x0,q.x);y0=std::min(y0,q.y);x1=std::max(x1,q.x);y1=std::max(y1,q.y);}
  int area=0,changed=0;for(int y=std::max(0,int(y0));y<=std::min(H-1,int(y1));++y)for(int x=std::max(0,int(x0));x<=std::min(W-1,int(x1));++x){++area;const auto a=w.whole[size_t(y*W+x)],b=s.renderer->pixels()[size_t(y*W+x)];
   const int d=std::abs(int((a>>16)&255)-int((b>>16)&255))+std::abs(int((a>>8)&255)-int((b>>8)&255))+std::abs(int(a&255)-int(b&255));if(d>60)++changed;}
  w.v["assembledChange"]=area?double(changed)/area:1;s.shoot("assembled");
  for(size_t k=w.before;k<list.size();++k){list[k].velocity=moving[k-w.before].velocity;list[k].vz=moving[k-w.before].vz;}
  w.v["brokeAt"]=s.frame;},
 .capture=[](Scene& s,const Frame& f){auto& w=watches()[&s];if(!w.v.count("brokeAt"))return;const int since=f.index-int(w.v["brokeAt"]);for(int k:{3,9,18,60})if(since==k)s.shoot("t"+std::to_string(k));},
 .metrics=[](Scene& s,Report& r){auto& w=watches()[&s];Game& g=s.g();const std::string what=s.fixture->value("prop");auto& list=LabAccess::clutter(g);
  if(w.prop<0){r.check("the scene has its prop",false);return;}
  r.check("it broke",list[size_t(w.prop)].broken);
  const auto* set=gibs::setOf(w.model);
  if(what=="table"){int slices=0;for(const auto& c:list)slices+=c.kind==6&&c.shardOf==ShardOf::Model&&c.shardModel==model::wooden_table;
   r.check("no authored set: sliced as before (pieces of its own mesh)",!set&&slices>=4,std::to_string(slices)+" slices");return;}
  if(!set){r.check("the prop has an authored set",false);return;}
  std::vector<const Clutter*> pieces;for(const auto& c:list)for(int k=0;k<set->count;++k)if(c.model==gibs::pieces[size_t(set->first+k)].model){pieces.push_back(&c);break;}
  r.metric("pieces",double(pieces.size()));
  r.check("it broke into its authored pieces, every one",int(pieces.size())==set->count,std::to_string(pieces.size())+" of "+std::to_string(set->count));
  float mass=0,lowest=1e9f;int awake=0;for(const auto* p:pieces){mass+=p->mass();lowest=std::min(lowest,p->z);awake+=!p->sleeping;}
  r.check("the pieces weigh what the prop did (within 10%)",std::fabs(mass-w.mass)<=.1f*w.mass,f2(mass)+" kg of "+f2(w.mass));
  r.check("they came to rest on the floor, asleep",lowest>-.02f&&awake==0,"lowest "+f2(lowest,3)+", awake "+std::to_string(awake));
  if(what!="crush"){r.metric("assembled_change",w.v["assembledChange"]);
   r.check("at the break the pieces sit where they were cut from (under 12% of the prop's pixels change)",w.v["assembledChange"]<.12,f2(w.v["assembledChange"]*100,1)+"%");}
  else r.check("the crate's weight crushed the bottle",w.v.count("brokeAt")>0);
  // Through a save: the pieces come back where they lay.
  const std::string bytes=LabAccess::encodeSave(g);Game loaded=g;const bool ok=LabAccess::decodeSave(loaded,bytes);int kept=0;
  if(ok){const auto& back=loaded.clutter();for(const auto* p:pieces)for(const auto& q:back)if(q.model==p->model&&length(q.pos-p->pos)<.001f&&std::fabs(q.z-p->z)<.001f){++kept;break;}}
  r.check("the pieces survive a save and load",ok&&kept==int(pieces.size()),std::to_string(kept)+" of "+std::to_string(pieces.size()));}});
}
}

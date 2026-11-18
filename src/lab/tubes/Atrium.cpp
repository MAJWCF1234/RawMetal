// The Starter Map's atrium (Showcase Atrium, south of Predator; tools/campaign/make_starter_map.py
// atrium()): the shipped map as the game plays it. A storefront of mixed panes (GLAZING) closes the shop,
// a ragged hole (BREACH) in the east wall lets the light court's light in, and the Growth brush's ivy,
// moss, creepers and grass have taken the place over.
//
// atrium: from the shipped Starter Map:
//  - the storefront stands: glass panes with their glazes, two chain-link panes, a solid painted-metal frame;
//  - a volley into one clear pane brings it down and leaves every other pane unmarked;
//  - the chain-link panes take rounds and never shatter, and stop a body;
//  - the player walks through the breach into the court, and the creepers over its lip swing as he does;
//  - every creeper is a hanging plant rooted at its top;
//  - a save of the broken pane loads back exactly.
// Pictures (1600 x 900): the atrium from the balcony and from the hall, the creepers over the breach
// against the court's light, the shattered pane.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CustomCampaign.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
std::shared_ptr<const CustomCampaign> starter(){static auto c=loadCustomCampaignFile(std::filesystem::path(labRoot())/"custom maps/Starter Map.rmc");return c;}
int atriumLevel(){const auto c=starter();if(!c)return -1;for(size_t i=0;i<c->maps.size();++i)if(c->maps[i]->name.find("Showcase Atrium")!=std::string::npos)return int(i);return -1;}
bool hangs(int model){return model==model::growth_hanging_vine||model==model::ivy_strand||model==model::growth_ivy_creeper||model==model::growth_ivy_curtain||model==model::growth_ivy_single;}
bool growth(int model){using namespace model;return hangs(model)||model==creeper||model==ivy_wall_a||model==ivy_wall_b||model==growth_vine_sheet||model==growth_moss_wall||model==growth_moss_floor||model==grass_l||model==grass_a||model==grass_b||model==grass_c||model==tall_grass_m;}
Point3 centre(const World& w,int i){const auto& b=w.breakables()[size_t(i)];return {(b.x1+b.x2)*.5f,(b.y1+b.y2)*.5f,(b.bottom+b.top)*.5f};}
int marked(const World& w,int except){int n=0;for(int i=0;i<int(w.breakables().size());++i)if(i!=except&&(!w.breakableIntact(i)||(w.paneGlass(i)&&w.breakables()[size_t(i)].cracks==0&&!w.paneGlass(i)->hits.empty())))++n;return n;}
// The clear pane the volley takes: the lowest clear one nearest x 12.4 (bay 3, east of the doorway).
int target(const World& w){int best=-1;float d=1e9f;for(int i=0;i<int(w.breakables().size());++i){const auto& b=w.breakables()[size_t(i)];if(b.glaze!=Glaze::Clear||b.cracks)continue;
 const float e=std::abs((b.x1+b.x2)*.5f-12.4f)+b.bottom;if(e<d){d=e;best=i;}}return best;}
const HitscanDef& shotgun(){return playerWeaponDef(WeaponId::Shotgun).hitscan;}
const HitscanDef& handgun(){return playerWeaponDef(WeaponId::Handgun).hitscan;}
struct Run {int strand=-1,slot=-1;float height=1,pushTip=0;int pane=-1;};
std::map<const Scene*,Run>& runs(){static std::map<const Scene*,Run> r;return r;}
constexpr int Wide=50,Close=62,WalkFrom=70,WalkTo=170,Volley=180,PaneShot=240;
void view(Scene& s,const char* label,Vec2 at,float z,float yawDeg,float pitch){Game& g=s.g();LabAccess::setCamera(g,at,z,yawDeg*kPi/180.f,pitch);s.renderer->render(g);s.shoot(label);}
const bool tube=add({
 .name="atrium",.summary="the Starter Map's atrium as shipped: a storefront of mixed panes (one breaks alone, mesh never shatters), a walk-through breach with the court's light coming through, creepers that swing as you pass, a save keeping the broken pane",
 .friends={"storefront","breach","growth"},.fixtures={"atrium"},
 .setup=[](Scene& s){auto& r=runs()[&s];r=Run{};const World& w=s.g().world();
  // The creeper over the breach's lip the walk goes through: the strand nearest (17.9, 6.6).
  float best=1e9f;for(size_t i=0;i<w.fixtures().size();++i){const auto& f=w.fixtures()[i];if(f.model!=model::growth_ivy_curtain)continue;const float d=std::hypot(f.position.x-17.9f,f.position.y-6.9f);
   if(d<best){best=d;r.strand=int(i);r.height=f.height;}}
  r.pane=target(w);},
 .step=[](Scene& s,float){auto& r=runs()[&s];Game& g=s.g();
  // Through the breach from the hall into the court, at a walk.
  if(s.frame>=WalkFrom&&s.frame<WalkTo){const float t=float(s.frame-WalkFrom)/float(WalkTo-WalkFrom);LabAccess::setCamera(g,{15.6f+t*5.f,6.75f},0,0,0);}
  // A volley into the clear pane from the hall.
  if(s.frame>=Volley&&s.frame<Volley+40&&(s.frame-Volley)%5==0&&r.pane>=0&&g.world().breakableIntact(r.pane)){const auto c=centre(g.world(),r.pane);const int k=(s.frame-Volley)/5;
   LabAccess::setCamera(g,{c.x-.3f,13.4f},0,kPi*.5f,0);LabAccess::fireFrom(g,{c.x-.3f,13.4f,1.5f},{c.x+.1f*float(k%3-1),c.y,c.z+.1f*float(k/3%3-1)},shotgun());}},
 .capture=[](Scene& s,const Frame&){auto& r=runs()[&s];auto& plants=LabAccess::plants(*s.renderer);Game& g=s.g();
  if(r.slot<0&&r.strand>=0)r.slot=plants.slotOf(g.level(),r.strand);
  if(r.slot>=0&&s.frame>=WalkFrom&&s.frame<WalkTo+30){const Point3 tip=plants.tip(r.slot);r.pushTip=std::max(r.pushTip,std::hypot(tip.x,tip.y));}
  if(s.frame==Wide){view(s,"atrium from the balcony",{6.4f,4.5f},3.6f,50,-24);view(s,"the hall to the breach",{3.0f,14.2f},0,-28,2);view(s,"atrium from the hall",{15.8f,2.2f},0,128,-6);view(s,"the storefront",{6.6f,10.6f},0,62,-4);}
  if(s.frame==Close)view(s,"creepers over the breach, the court's light behind",{14.2f,7.4f},0,-6,4);view(s,"creepers in the hall",{11.4f,10.2f},0,-52,16);
  if(s.frame==PaneShot&&r.pane>=0){const auto c=centre(g.world(),r.pane);view(s,"the shattered pane",{c.x-1.2f,13.1f},0,74,-10);}},
 .metrics=[](Scene& s,Report& rep){auto& r=runs()[&s];const int level=atriumLevel();
  rep.check("the Starter Map has its atrium",level>=0&&s.g().level()==level,"level "+std::to_string(level)+", playing "+std::to_string(s.g().level()));if(level<0)return;
  auto fresh=[&]{auto g=std::make_unique<Game>(starter());LabAccess::loadLevel(*g,level);return g;};
  const World& w=s.g().world();
  // ---- the storefront as played ----
  {int glass=0,dirty=0,cracked=0,frosted=0,mesh=0,frame=0;for(const auto& b:w.breakables()){glass+=b.material==17;dirty+=b.glaze==Glaze::Dirty;cracked+=b.cracks>0;frosted+=b.glaze==Glaze::Frosted;}
   for(const auto& st:w.structures()){mesh+=st.material==ChainLinkMaterial;frame+=st.material==12&&st.solid&&st.y1>=15.8f&&st.y2<=17.2f;}
   rep.metric("glass_panes",glass);rep.metric("mesh_panes",mesh);rep.metric("frame_members",frame);
   rep.check("the storefront stands: 7 glass panes (2 dirty, 1 frosted, 1 pre-cracked), 2 chain-link panes, a painted-metal frame",glass==7&&dirty==2&&frosted==1&&cracked==1&&mesh==2&&frame>=10,
    std::to_string(glass)+" glass, "+std::to_string(mesh)+" mesh, "+std::to_string(frame)+" members");}
  // ---- one pane ----
  rep.check("the volley brings down the clear pane and leaves every other pane unmarked",r.pane>=0&&!w.breakableIntact(r.pane)&&marked(w,r.pane)==0,
   r.pane<0?"no clear pane":std::to_string(marked(w,r.pane))+" others marked");
  // ---- mesh ----
  {auto g=fresh();const World& gw=g->world();int boxes=0;Point3 c{};for(const auto& st:gw.structures())if(st.material==ChainLinkMaterial){++boxes;c={(st.x1+st.x2)*.5f,(st.y1+st.y2)*.5f,(st.bottom+st.top)*.5f};}
   for(int k=0;k<12;++k)LabAccess::fireFrom(*g,{c.x,13,c.z},{c.x+.05f*float(k%3-1),c.y,c.z},handgun());
   int after=0;for(const auto& st:gw.structures())after+=st.material==ChainLinkMaterial;
   rep.check("the chain-link panes take rounds and never shatter, and stop a body",boxes==2&&after==2&&marked(gw,-1)==0&&gw.structureBlocksHull(c.x,c.y,.3f,0,1.7f));}
  // ---- the breach ----
  {auto g=fresh();auto& p=LabAccess::player(*g);p.pos={15.6f,6.75f};p.angle=0;for(int i=0;i<150;++i){InputState in;in.forward=true;g->update(in,1.f/60);}
   rep.metric("breach_walk_x",p.pos.x,"m");rep.check("the player walks through the breach into the light court",p.pos.x>19.5f,"reached x "+fixed(p.pos.x));
   Point3 n;const float lit=g->world().lightRayHit({20.2f,6.8f,2.9f},{15.5f,6.8f,.05f},n);rep.check("the court's lamp reaches the hall floor through the breach",lit>=.999f,fixed(lit,3));}
  // ---- the growth ----
  {auto& plants=LabAccess::plants(*s.renderer);int pieces=0,hung=0,rooted=0;for(size_t i=0;i<w.fixtures().size();++i){const auto& f=w.fixtures()[i];if(!growth(f.model))continue;++pieces;
    if(hangs(f.model)){const int slot=plants.slotOf(s.g().level(),int(i));hung+=plants.hanging(slot);rooted+=slot>=0&&std::abs(plants.root(slot).z-(f.base+f.height))<.02f;}}
   int creepers=0;for(const auto& f:w.fixtures())creepers+=hangs(f.model);
   rep.metric("growth_pieces",pieces);rep.metric("creepers",creepers);
   rep.check("the atrium is overgrown: ivy, moss, creepers and grass, every creeper a hanging plant rooted at its top",pieces>=180&&creepers>=20&&hung==creepers&&rooted==creepers,
    std::to_string(pieces)+" pieces, "+std::to_string(hung)+"/"+std::to_string(creepers)+" creepers hanging");
   rep.metric("strand_push_tip_m",r.pushTip,"m");
   rep.check("walking through the breach swings the creeper over its lip (10% of its length or more)",r.slot>=0&&r.pushTip>=.1f*r.height,fixed(r.pushTip)+" m of "+fixed(r.height)+" m");}
  // ---- saves ----
  {const auto bytes=LabAccess::encodeSave(s.g());auto back=fresh();const bool loaded=LabAccess::decodeSave(*back,bytes);
   bool same=loaded&&back->world().breakables().size()==w.breakables().size();
   for(int i=0;same&&i<int(w.breakables().size());++i)same&=back->world().breakableIntact(i)==w.breakableIntact(i);
   rep.check("a save of the broken pane loads back exactly",same&&r.pane>=0&&!back->world().breakableIntact(r.pane)&&LabAccess::encodeSave(*back)==bytes);}
 },
 .teardown=[](Scene& s){runs().erase(&s);},.needsGpu=true});
}
}

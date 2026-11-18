// Growth (the Campaign Builder's Growth brush): ivy and moss flat on a wall, creepers hanging from the
// ceiling and over a ledge, grass through the floor, as FIXTUREs of the brush's models. The creepers are
// hanging plants (renderer/Foliage.h): rooted at their top, they drift indoors where there is no wind,
// part round whoever walks through them and flinch when shot, then settle.
//
// growth: the numbers, from the authored room:
//  - every creeper is a hanging plant slot rooted at its top; grass is a standing one; ivy and moss are no plants;
//  - none of it is solid: the player walks through a curtain of creepers and a patch of grass;
//  - walked through, a creeper swings away (tip 10% of its length or more), springs back and sleeps;
//  - a round through one makes it flinch (4 cm or more) and it settles again;
//  - indoors, with no wind, the creepers still drift: the picture moves against the same moment held still.
// Pictures: the creepers up close (still and drifting), walked through, shot.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
constexpr float Ceiling=3.4f;
void piece(AuthoredMapData& m,int model,float k,float x,float y,float z,float yaw){const auto& s=model::size[size_t(model)];
 retro::Fixture f{model,{x,y},z,s[0]*k,std::max(.005f,s[1]*k),std::max(.005f,s[2]*k),yaw,false};m.fixtures.push_back(f);}
// On the face of the wall at y 10 (facing north, toward the player), turned out from it.
void onWall(AuthoredMapData& m,int model,float k,float x,float z){const auto& s=model::size[size_t(model)];piece(m,model,k,x,10.f-(s[1]*k*.5f+.01f),z,kPi);}
void author(AuthoredMapData& m){
 using namespace model;
 m.definition.ceiling=Ceiling;m.definition.playerStart={8,4};
 m.layers[0].rows[10]=std::string(24,'#');
 onWall(m,ivy_wall_a,1.1f,5,0);onWall(m,ivy_wall_b,1.0f,9.5f,1.2f);onWall(m,growth_vine_sheet,1.f,12.5f,.2f);onWall(m,growth_moss_wall,1.f,14,0);
 // A ledge (a low wall, 1.2 m) with strands over its north lip.
 m.structures.push_back({15,6,19,6.5f,0,1.2f,false,3});
 for(float x:{15.6f,16.6f,17.6f}){const auto& s=size[ivy_strand];const float k=.5f;piece(m,ivy_strand,k,x,6.f-(s[1]*k*.5f+.015f),1.24f-s[2]*k,kPi);}
 // A curtain of creepers across the player's way north (x 7-9, y 7), hanging from the ceiling to 1.3 m.
 for(int i=0;i<3;++i){const auto& s=size[growth_ivy_creeper];const float k=(Ceiling-1.3f)/s[2];piece(m,growth_ivy_creeper,k,7.2f+.8f*float(i),7,Ceiling-s[2]*k,.7f*float(i));}
 // Grass where the player walks.
 for(int i=0;i<6;++i)piece(m,grass_l,.8f,6.5f+.6f*float(i),8.6f,0,1.3f*float(i));
 // A lamp over the creepers.
 WorldLight lamp;lamp.position={8,6};lamp.z=Ceiling-.1f;lamp.intensity=1.2f;lamp.range=9;m.lights.push_back(lamp);}
std::shared_ptr<CustomCampaign> room(){auto m=regression::room();author(*m);return regression::campaign(m);}
bool hangs(int model){return model==model::growth_hanging_vine||model==model::ivy_strand||model==model::growth_ivy_creeper;}
struct Run {int slot=-1,fixture=-1;float height=1,pushTip=0,leftAt=-1,backAt=-1,shotAt=-1,shotTip=0,shotBack=-1;double drift=-1,push=-1;bool inside=false;};
std::map<const Scene*,Run>& runs(){static std::map<const Scene*,Run> r;return r;}
constexpr int WalkFrom=30,ShootAt=400;
// The share of pixels that move between the plants held still and as they are, from the camera.
double moving(Scene& s,const char* still,const char* live){auto& plants=LabAccess::plants(*s.renderer);plants.still=true;s.renderer->render(s.g());s.shoot(still);
 const std::vector<std::uint32_t> a(s.renderer->pixels(),s.renderer->pixels()+size_t(s.renderer->width())*s.renderer->height());
 plants.still=false;s.renderer->render(s.g());s.shoot(live);size_t changed=0;const auto* b=s.renderer->pixels();
 for(size_t i=0;i<a.size();++i){int d=0;for(int k=0;k<24;k+=8)d+=std::abs(int((a[i]>>k)&255)-int((b[i]>>k)&255));changed+=d>24;}
 return a.empty()?0:100.0*double(changed)/double(a.size());}
const bool tube=add({
 .name="growth",.summary="the Growth brush's pieces: ivy and moss flat on a wall, creepers hanging from the ceiling and a ledge (hanging plants that drift, part and flinch), grass through the floor, none of it solid",
 .friends={"plants"},.fixtures={"growth"},
 .author=[](Scene&,AuthoredMapData& m){author(m);},
 .setup=[](Scene& s){auto& r=runs()[&s];r=Run{};const World& w=s.g().world();
  for(size_t i=0;i<w.fixtures().size();++i)if(w.fixtures()[i].model==model::growth_ivy_creeper&&std::abs(w.fixtures()[i].position.x-8.f)<.1f){r.fixture=int(i);r.height=w.fixtures()[i].height;}},
 .step=[](Scene& s,float){auto& r=runs()[&s];Game& g=s.g();
  // The player walks north from y 4 through the creeper curtain and the grass, then turns and shoots a creeper.
  if(s.frame>=WalkFrom&&s.frame<ShootAt){const float t=std::min(1.f,float(s.frame-WalkFrom)/150.f);LabAccess::setCamera(g,{8.f,4.f+t*5.f},0,kPi*.5f,0);}
  if(s.frame==ShootAt){LabAccess::setCamera(g,{6.f,4.6f},0,std::atan2(7.f-4.6f,8.f-6.f),0);ShotHit hit{};LabAccess::fireAt(g,{8.f,7.f,2.2f},&hit);r.shotAt=s.time;}},
 .capture=[](Scene& s,const Frame&){auto& r=runs()[&s];if(r.fixture<0)return;auto& plants=LabAccess::plants(*s.renderer);Game& g=s.g();
  if(r.slot<0)r.slot=plants.slotOf(g.level(),r.fixture);if(r.slot<0)return;
  const Point3 tip=plants.tip(r.slot);const float bend=std::hypot(tip.x,tip.y);const Player& p=g.player();
  const bool inside=s.frame>=WalkFrom&&std::abs(p.pos.y-7.f)<.45f;if(inside)r.pushTip=std::max(r.pushTip,bend);
  if(r.inside&&!inside&&r.leftAt<0)r.leftAt=s.time;r.inside=inside;
  if(r.leftAt>=0&&r.backAt<0&&s.frame<ShootAt&&bend<.01f&&!plants.awake(r.slot))r.backAt=s.time;
  if(r.shotAt>=0){if(s.time-r.shotAt<.5f)r.shotTip=std::max(r.shotTip,bend);if(r.shotBack<0&&s.time-r.shotAt>.05f&&bend<.01f&&!plants.awake(r.slot))r.shotBack=s.time;}
  // From the start: the creepers up close, held still and drifting (no wind indoors).
  if(s.frame==WalkFrom-5)r.drift=moving(s,"creepers held still","creepers drifting");
  if(s.frame==WalkFrom+95)r.push=moving(s,"walked through (held)","walked through");
  if(s.frame==ShootAt+3)s.shoot("shot");},
 .metrics=[](Scene& s,Report& rep){auto& r=runs()[&s];
  {Game g(room());const World& w=g.world();int ivy=0,hung=0,grass=0,solid=0;
   for(const auto& f:w.fixtures()){solid+=f.solid;hung+=hangs(f.model);grass+=f.model==model::grass_l;ivy+=f.model==model::ivy_wall_a||f.model==model::ivy_wall_b||f.model==model::growth_vine_sheet||f.model==model::growth_moss_wall;}
   rep.metric("ivy_and_moss",ivy);rep.metric("creepers",hung);rep.metric("grass",grass);
   rep.check("growth is never solid",solid==0,std::to_string(solid)+" solid");
   // The player walks north through the creeper curtain and the grass to the wall.
   auto& p=LabAccess::player(g);p.pos={8,4};p.angle=kPi*.5f;for(int i=0;i<150;++i){InputState in;in.forward=true;g.update(in,1.f/60);}
   rep.metric("walk_to_y",p.pos.y,"m");rep.check("the player walks through the creepers and the grass to the wall",p.pos.y>9.f,"reached y "+fixed(p.pos.y));
   // Rounds go through a creeper (it is no wall) to the wall behind.
   const auto through=LabAccess::trace(g,{8,4,1.8f},{0,1,0},10);rep.check("a round passes through the creepers to the wall",through.kind==ShotKind::Solid&&through.distance>5.5f,"stopped at "+fixed(through.distance));}
  auto& plants=LabAccess::plants(*s.renderer);const World& w=s.g().world();int hanging=0,rooted=0,standing=0;
  for(size_t i=0;i<w.fixtures().size();++i){const auto& f=w.fixtures()[i];const int slot=plants.slotOf(s.g().level(),int(i));
   if(hangs(f.model)){hanging+=plants.hanging(slot);rooted+=slot>=0&&std::abs(plants.root(slot).z-(f.base+f.height))<.02f;}
   if(f.model==model::grass_l)standing+=slot>=0&&!plants.hanging(slot);}
  rep.check("every creeper is a hanging plant rooted at its top; the grass stands",hanging==6&&rooted==6&&standing==6,std::to_string(hanging)+" hanging, "+std::to_string(rooted)+" rooted at the top, "+std::to_string(standing)+" grass");
  rep.check("a creeper of the curtain to watch",r.slot>=0);if(r.slot<0)return;
  rep.metric("push_tip_m",r.pushTip,"m");rep.metric("spring_back_s",r.backAt>=0&&r.leftAt>=0?r.backAt-r.leftAt:-1,"s");rep.metric("flinch_tip_m",r.shotTip,"m");
  rep.metric("drift_pixels_pct",r.drift,"%");rep.metric("push_pixels_pct",r.push,"%");
  rep.check("walked through, the creeper swings away (tip 10% of its length or more)",r.pushTip>=.1f*r.height,fixed(r.pushTip)+" m of "+fixed(r.height)+" m");
  rep.check("left alone, it swings back and sleeps within 3.5 s",r.backAt>=0&&r.leftAt>=0&&r.backAt-r.leftAt<=3.5f,r.backAt<0?"still moving":fixed(r.backAt-r.leftAt)+" s");
  rep.check("a round through it makes it flinch (4 cm or more)",r.shotTip>=.04f,fixed(r.shotTip*100,1)+" cm");
  rep.check("and it settles again within 3.5 s",r.shotBack>=0&&r.shotBack-r.shotAt<=3.5f,r.shotBack<0?"still moving":fixed(r.shotBack-r.shotAt)+" s");
  rep.check("indoors, with no wind, the creepers drift on screen",r.drift>.02,fixed(r.drift,3)+"% of pixels");
  rep.check("walked through, the curtain moves more than the drift alone",r.push>r.drift,fixed(r.push,3)+"% vs "+fixed(r.drift,3)+"%");},
 .reporter=[](Scene& s,Overlay& o){auto& plants=LabAccess::plants(*s.renderer);const World& w=s.g().world();
  for(size_t i=0;i<w.fixtures().size();++i){const auto& f=w.fixtures()[i];if(!hangs(f.model))continue;const int slot=plants.slotOf(s.g().level(),int(i));if(slot<0)continue;
   const Point3 root=plants.root(slot),tip=plants.tip(slot);o.line(root,{root.x+tip.x,root.y+tip.y,root.z-plants.height(slot)},plants.awake(slot)?0xffa020:0x40ff60);}},
 .teardown=[](Scene& s){LabAccess::plants(*s.renderer).still=false;runs().erase(&s);},.needsGpu=true});
}
}

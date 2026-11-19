// Liquid in bottles (src/game/Liquids.h). Fixtures lab/fixtures/liquids-*.lab, `scene`:
//  slosh: a bottle of water carried along a table at 1.2 m/s and stopped dead (as a hand moves it): its surface tips against the stop (past
//         8 degrees off level in the bottle) and settles back level; nothing pours from an upright bottle.
//  pour:  bottles of water and oil held tipped over the floor: they pour, the level drops, drops fall and a puddle
//         grows where they land holding what left; the oil runs slower; the bottle gets lighter as it empties.
//  leak:  a bottle of water on a table shot once with the handgun: it is holed, not broken; it leaks from the hole
//         until the surface is down to the hole, and stops; the level and the hole survive a save and load.
//  break: a bottle of green chemical shot twice: the second round breaks it; it spills everything it held.
//  mass:  a full bottle weighs its glass and its contents, sinks where an empty one floats (by its density).
//  occlude: puddles, a stream and drops laid behind a crate and behind the table block, and one puddle in the open:
//         rendered with and without the hidden ones, the frames match (they are hidden where the crate and block
//         cover them), and the open puddle shows. Run on the GPU and with --software.
//  maps:  campaign maps 4 and 5 (stitched): a spill in 4 pools there; the player walks into 5, where nothing is
//         drawn at the same spot (a puddle put in 5 itself there does show: the spot is in view); back in 4 the
//         puddle is where it was; a new map loaded starts dry.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include "game/Buoyancy.h"
#include <cmath>
#include <cstdio>
#include <map>
namespace retro::lab {
namespace {
std::string f2(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Watch {std::map<std::string,double> v;std::vector<int> bottles;};
std::map<const Scene*,Watch>& watches(){static std::map<const Scene*,Watch> w;return w;}
float puddleVolume(const Game& g,liquid::Kind k){float v=0;for(const auto& p:g.puddles())if(p.kind==k)v+=p.volume;return v;}
float tiltOff(const Clutter& c){const auto& n=c.liquid.normal;return std::acos(std::clamp(n.z,-1.f,1.f))*57.2958f;}
// Pixels that differ by more than 24 in a channel.
int differing(const Scene::Shot& a,const Scene::Shot& b){if(a.pixels.size()!=b.pixels.size())return 1<<30;int n=0;
 for(size_t i=0;i<a.pixels.size();++i){const auto p=a.pixels[i],q=b.pixels[i];for(int k=0;k<24;k+=8)if(std::abs(int((p>>k)&255)-int((q>>k)&255))>24){++n;break;}}return n;}
// Two views, each rendered: with nothing, with the open puddle, with the open and the hidden liquid, and that again
// (the render's own frame-to-frame noise). Hidden: puddles on the floor behind the crate (and behind the table block),
// a stream of drops and a loose drop behind them low enough to be covered.
void occlude(Scene& s,Watch& w){Game& g=s.g();auto& puddles=LabAccess::puddles(g);auto& drops=LabAccess::liquidDrops(g);const Player keep=LabAccess::player(g);
 struct View {const char* name;Vec2 eye;Point3 hidden;float top;Point3 open;bool dust;};
 // dust: the crate view again with a burst of dust in the air (soft particle cards: the GPU draws the blended things
 // after the room's depth is copied for them, a second pass that must keep that depth).
 for(const View v:{View{"crate",{5.4f,7.f},{7.75f,7.f,0},.36f,{8.2f,6.f,0},false},View{"block",{5.4f,8.5f},{9.4f,8.5f,0},.5f,{7.2f,9.3f,0},false},View{"dust",{5.4f,7.f},{7.75f,7.f,0},.36f,{8.2f,6.f,0},true}}){
  LabAccess::setCamera(g,v.eye,-999,0,-36.f);if(v.dust)LabAccess::hitBurst(g,{9.f,7.6f,1.2f},{-1,0,0},ImpactMaterial::Concrete,0,1.f,false);
  auto render=[&](bool open,bool hidden){puddles.clear();drops.clear();
   if(open)puddles.push_back({v.open,liquid::Kind::Water,.4f,.32f});
   if(hidden){puddles.push_back({v.hidden,liquid::Kind::Oil,.4f,.3f});puddles.push_back({v.hidden+Point3{0,.12f,0},liquid::Kind::Chemical,.2f,.18f});
    for(int k=0;k<10;++k)drops.push_back({v.hidden+Point3{0,-.05f,.04f+v.top*float(k)/10.f},{},liquid::Kind::Water,1e-6f,1});
    drops.push_back({v.hidden+Point3{0,.08f,v.top*.6f},{},liquid::Kind::Oil,1e-6f,0});}
   s.renderer->render(g);s.shoot(std::string("occlude-")+v.name+(hidden?"-hidden":open?"-open":"-none"));return s.shots.back();};
  render(false,false);const auto none=render(false,false);const auto open=render(true,false);render(true,true);const auto all=render(true,true);const auto again=render(true,true);
  const std::string n=v.name;w.v[n+"Hidden"]=differing(open,all);w.v[n+"Noise"]=differing(all,again);w.v[n+"Open"]=differing(none,open);}
 puddles.clear();drops.clear();LabAccess::player(g)=keep;LabAccess::playerCamera(g);}
// Campaign maps 4 and 5 meet along y (4's y=24 is 5's y=0); the player crosses there along x=12.
void maps(Scene& s,Watch& w,int f){Game& g=s.g();auto& puddles=LabAccess::puddles(g);auto& drops=LabAccess::liquidDrops(g);auto& p=LabAccess::player(g);
 const Point3 spot{12.f,21.5f,0};
 auto look=[&](Vec2 eye,float yaw){const Player keep=p;LabAccess::setCamera(g,eye,-999,yaw,-30.f);s.renderer->render(g);s.shoot("maps-"+std::to_string(s.shots.size()));const auto shot=s.shots.back();p=keep;LabAccess::playerCamera(g);return shot;};
 if(f==0){p.pos={12.f,20.f};p.z=g.world().floorHeight(12.f,20.f);
  // A bottle's worth poured from 40 cm over the floor of map 4 at the spot.
  const float floor=g.world().floorHeight(spot.x,spot.y);for(int k=0;k<30;++k)drops.push_back({{spot.x+.01f*float(k%5),spot.y+.01f*float(k/5),floor+.4f},{},liquid::Kind::Oil,1e-5f,0,g.level()});}
 if(f==40){int n=0;for(const auto& q:puddles)if(q.level==4)++n;w.v["pooledA"]=g.level()==4&&n>0?n:0;w.v["ax"]=puddles.empty()?0:puddles[0].at.x;w.v["ay"]=puddles.empty()?0:puddles[0].at.y;
  p.pos={12.f,24.1f};LabAccess::crossChunk(g);}
 if(f==42&&g.level()==5){
  // In map 5, from 3 m back along the corridor, looking at the same local spot.
  const Vec2 eye{12.f,spot.y-3.f};const auto kept=puddles;
  look(eye,kPi*.5f);const auto with=look(eye,kPi*.5f);puddles.clear();const auto without=look(eye,kPi*.5f);const auto again=look(eye,kPi*.5f);
  const float floorB=g.world().floorHeight(spot.x,spot.y);puddles.push_back({{spot.x,spot.y,floorB},liquid::Kind::Oil,.3f,.4f,5});look(eye,kPi*.5f);const auto control=look(eye,kPi*.5f);
  puddles=kept;w.v["leakB"]=differing(with,without);w.v["noiseB"]=differing(without,again);w.v["controlB"]=differing(without,control);
  p.pos={12.f,-.1f};LabAccess::crossChunk(g);}
 if(f==44&&g.level()==4){bool same=false;for(const auto& q:puddles)if(q.level==4&&std::fabs(q.at.x-float(w.v["ax"]))<.001f&&std::fabs(q.at.y-float(w.v["ay"]))<.001f)same=true;w.v["backA"]=same;
  const Vec2 eye{12.f,spot.y-3.f};const auto kept=puddles;look(eye,kPi*.5f);const auto with=look(eye,kPi*.5f);puddles.clear();const auto without=look(eye,kPi*.5f);puddles=kept;w.v["shownA"]=differing(with,without);
  LabAccess::loadLevel(g,6);w.v["dry"]=puddles.empty()&&drops.empty();}}
const bool registered=add({
 .name="liquids",.summary="liquid in bottles: water, oil and a green chemical slosh on a damped spring, pour past their level into falling drops and growing puddles, leak from a round's hole down to it, spill when broken, weigh what they hold and save their level",
 .fixtures={"liquids-slosh","liquids-pour","liquids-leak","liquids-break","liquids-mass","liquids-occlude","liquids-maps"},
 .author=[](Scene& s,AuthoredMapData& m){const std::string scene=s.fixture->value("scene");
  kit::wall(m,8.1f,7.6f,8.9f,9.4f,0,.9f,40);   // a table-high block for bottles to stand on
  if(scene=="slosh")kit::prop(m,model::glass_bottle,8.2f,8.f,.9f,0);
  if(scene=="pour"){kit::prop(m,model::glass_bottle,8.5f,8.1f,1.25f,0);kit::prop(m,model::glass_bottle,8.5f,8.9f,1.25f,0);}
  if(scene=="leak"||scene=="break")kit::prop(m,model::glass_bottle,8.5f,8.5f,.9f,0);
  if(scene=="mass"){kit::prop(m,model::glass_bottle,6.f,6.f,0,0);kit::prop(m,model::glass_bottle,6.5f,6.f,0,0);}
  if(scene=="occlude")kit::prop(m,model::wooden_crate,7.f,7.f,0,0);},
 .setup=[](Scene& s){watches().erase(&s);auto& w=watches()[&s];Game& g=s.g();LabAccess::setGod(g,true);const std::string scene=s.fixture->value("scene");auto& list=LabAccess::clutter(g);
  for(int i=0;i<int(list.size());++i)if(list[size_t(i)].model==model::glass_bottle)w.bottles.push_back(i);
  auto at=[&](int k)->Clutter&{return list[size_t(w.bottles[size_t(k)])];};
  if(scene=="slosh"){g.fillLiquid(at(0),liquid::Kind::Water,.55f);at(0).pinned=true;}   // moved as a hand would move it (below)
  if(scene=="pour"){g.fillLiquid(at(0),liquid::Kind::Water,.7f);g.fillLiquid(at(1),liquid::Kind::Oil,.7f);
   // Held tipped over the floor, mouths out over the table's edge (pinned: the hand that holds them).
   for(int k=0;k<2;++k){auto& c=at(k);c.pinned=true;c.pitch=2.05f;c.yaw=0;c.roll=0;c.pos={8.85f,k?8.95f:8.05f};c.z=1.15f;c.sleeping=false;}}
  if(scene=="leak")g.fillLiquid(at(0),liquid::Kind::Water,.8f);
  if(scene=="break")g.fillLiquid(at(0),liquid::Kind::Chemical,.75f);
  if(scene=="mass"){g.fillLiquid(at(0),liquid::Kind::Water,1.f);g.fillLiquid(at(1),liquid::Kind::Water,0.f);}
  for(int k=0;k<int(w.bottles.size());++k){w.v["level0_"+std::to_string(k)]=at(k).liquid.level;w.v["mass0_"+std::to_string(k)]=at(k).mass();}},
 .step=[](Scene& s,float){auto& w=watches()[&s];Game& g=s.g();const std::string scene=s.fixture->value("scene");auto& list=LabAccess::clutter(g);const int f=s.frame;
  if(scene=="occlude"&&f==s.frames-1)occlude(s,w);
  if(scene=="maps")maps(s,w,f);
  if(w.bottles.empty())return;
  auto& c=list[size_t(w.bottles[0])];
  if(scene=="slosh"){if(f>=20&&f<44)c.pos.x+=1.2f/60.f;if(f>20&&f<80)w.v["maxTilt"]=std::max(w.v["maxTilt"],double(tiltOff(c)));if(f>20)w.v["pourMax"]=std::max(w.v["pourMax"],double(c.liquid.pouring));
   if(f==47)s.shoot("sloshing");}
  if(scene=="pour"){const auto& o=list[size_t(w.bottles[1])];
   if(f==60){w.v["water60"]=c.liquid.level;w.v["oil60"]=o.liquid.level;w.v["mass60"]=c.mass();s.shoot("pouring");}
   if(f==150)s.shoot("pouring-late");}
  if(scene=="leak"){if(f==20){LabAccess::fireFrom(g,{6.2f,8.5f,1.05f},{8.5f,8.5f,1.03f},weaponDef(WeaponId::Handgun).hitscan);w.v["holed"]=c.liquid.holed&&!c.broken;w.v["afterShot"]=c.liquid.level;}
   if(f==50)s.shoot("leaking");if(f==50)w.v["leaking50"]=c.liquid.pouring;}
  if(scene=="break"){const Point3 aim{8.5f,8.5f,1.03f};if(f==20||f==40)LabAccess::fireFrom(g,{6.2f,8.5f,1.05f},aim,weaponDef(WeaponId::Handgun).hitscan);
   if(f==30){w.v["holedFirst"]=c.liquid.holed&&!c.broken;w.v["held30"]=c.liquid.level;}if(f==41){w.v["broken"]=c.broken;w.v["spilt"]=c.liquid.level;}if(f==48)s.shoot("spilling");}},
 .metrics=[](Scene& s,Report& r){auto& w=watches()[&s];Game& g=s.g();const std::string scene=s.fixture->value("scene");auto& list=LabAccess::clutter(g);
  if(scene=="maps"){
   r.check("a spill in map 4 pools in map 4",w.v["pooledA"]>0,f2(w.v["pooledA"],0)+" puddles");
   r.check("the spot in map 5 is in view (a puddle of map 5's own there shows, 200 px and more)",w.v["controlB"]>=200,f2(w.v["controlB"],0)+" px");
   r.check("in map 5 nothing of map 4's spill is drawn at the same spot (frames match to the render's own noise + 20 px)",w.v["leakB"]<=w.v["noiseB"]+20,f2(w.v["leakB"],0)+" px differ, noise "+f2(w.v["noiseB"],0));
   r.check("back in map 4 the puddle is where it was (1 mm) and shows",w.v["backA"]>0&&w.v["shownA"]>=200,"shown "+f2(w.v["shownA"],0)+" px");
   r.check("a new map loaded starts dry",w.v["dry"]>0);return;}
  if(scene=="occlude"){for(const char* view:{"crate","block","dust"}){const std::string v=view;
    r.metric(v+"_hidden_px",w.v[v+"Hidden"],"px");r.metric(v+"_noise_px",w.v[v+"Noise"],"px");r.metric(v+"_open_px",w.v[v+"Open"],"px");
    r.check("puddles, a stream and drops behind the "+(v=="dust"?std::string("crate, dust in the air,"):v)+" are hidden where it covers them (frames match to the render's own noise + 20 px)",w.v[v+"Hidden"]<=w.v[v+"Noise"]+20,f2(w.v[v+"Hidden"],0)+" px differ, noise "+f2(w.v[v+"Noise"],0));
    r.check("the puddle in the open shows ("+v+" view, 200 px and more)",w.v[v+"Open"]>=200,f2(w.v[v+"Open"],0)+" px");}
   return;}
  if(w.bottles.empty()){r.check("the scene has its bottles",false);return;}auto& c=list[size_t(w.bottles[0])];
  if(scene=="slosh"){r.metric("max_tilt",w.v["maxTilt"],"deg");
   r.check("stopped dead, the surface tips against the stop (over 8 degrees)",w.v["maxTilt"]>8,f2(w.v["maxTilt"],1)+" deg");
   r.check("and settles back level (under 2 degrees)",tiltOff(c)<2,f2(tiltOff(c),2)+" deg");
   r.check("nothing pours from an upright bottle",w.v["pourMax"]==0&&c.liquid.level>.549f,"level "+f2(c.liquid.level,3));}
  if(scene=="pour"){const auto& o=list[size_t(w.bottles[1])];const float cap=liquid::capacity(model::glass_bottle,c.size());
   const float waterOut=(w.v["level0_0"]-c.liquid.level)*cap,oilOut=(w.v["level0_1"]-o.liquid.level)*liquid::capacity(model::glass_bottle,o.size());
   const float waterPool=puddleVolume(g,liquid::Kind::Water),oilPool=puddleVolume(g,liquid::Kind::Oil);float flying=0;for(const auto& d:g.liquidDrops())flying+=d.volume;
   r.metric("water_left",c.liquid.level);r.metric("oil_left",o.liquid.level);r.metric("water_puddle_l",waterPool*1000,"l");r.metric("oil_puddle_l",oilPool*1000,"l");
   r.check("the water poured (its level fell by half and more)",c.liquid.level<w.v["level0_0"]*.5,f2(w.v["level0_0"])+" -> "+f2(c.liquid.level));
   r.check("the oil ran slower than the water (by its first second)",(w.v["level0_1"]-w.v["oil60"])<(w.v["level0_0"]-w.v["water60"])*.6,"water "+f2(w.v["level0_0"]-w.v["water60"],3)+", oil "+f2(w.v["level0_1"]-w.v["oil60"],3));
   r.check("puddles grew under them holding what left (all but what is still in the air, 2%)",std::fabs(waterPool+oilPool+flying-(waterOut+oilOut))<.02f*(waterOut+oilOut)+1e-6f,f2((waterPool+oilPool)*1000,3)+" l + "+f2(flying*1000,3)+" l flying of "+f2((waterOut+oilOut)*1000,3)+" l");
   r.check("the bottle is lighter for what it lost",w.v["mass0_0"]-c.mass()>.9f*waterOut*1000,f2(w.v["mass0_0"])+" -> "+f2(c.mass())+" kg");
   r.check("the puddles lie on the floor",[&]{for(const auto& p:g.puddles())if(std::fabs(p.at.z)>.02f)return false;return !g.puddles().empty();}());}
  if(scene=="leak"){r.check("one round holes the bottle; it is not broken",w.v["holed"]>0);
   r.check("it leaks from the hole",w.v["leaking50"]>0,f2(w.v["leaking50"],3)+" l/s at 0.5 s");
   // Down to the hole, then it stops.
   const float holeZ=c.liquid.hole.z,surface=c.liquid.normal.z>0?c.liquid.plane/c.liquid.normal.z:0;
   r.check("the level falls to the hole (within 1.5 cm) and the leak dwindles (under 40 ml/s, from 100)",c.liquid.pouring<.04f&&std::fabs(surface-holeZ)<.015f,"surface "+f2(surface,3)+" hole "+f2(holeZ,3));
   const std::string bytes=LabAccess::encodeSave(g);Game loaded=g;const bool ok=LabAccess::decodeSave(loaded,bytes);const auto& b=loaded.clutter()[size_t(w.bottles[0])];
   r.check("the level and the hole survive a save and load",ok&&b.liquid.holds()&&std::fabs(b.liquid.level-c.liquid.level)<1e-5f&&b.liquid.holed&&std::fabs(b.liquid.hole.z-holeZ)<1e-5f,ok?"level "+f2(b.liquid.level,4):"load failed");}
  if(scene=="break"){r.check("the first round holes it",w.v["holedFirst"]>0);r.check("the second breaks it",w.v["broken"]>0);
   r.check("broken, it spills all it held",w.v["spilt"]==0);float pool=puddleVolume(g,liquid::Kind::Chemical);for(const auto& d:g.liquidDrops())pool+=d.volume;
   r.check("its puddle holds what it held",pool>.9f*w.v["held30"]*liquid::capacity(model::glass_bottle,c.size()),f2(pool*1000,3)+" l");}
  if(scene=="mass"){const auto& e=list[size_t(w.bottles[1])];const float cap=liquid::capacity(model::glass_bottle,c.size());
   r.metric("full_kg",c.mass(),"kg");r.metric("empty_kg",e.mass(),"kg");r.metric("full_density",buoyancy::density(c));r.metric("empty_density",buoyancy::density(e));
   r.check("a full bottle weighs its glass and its contents",std::fabs(c.mass()-e.mass()-cap*1000)<.01f,f2(c.mass())+" vs "+f2(e.mass())+" + "+f2(cap*1000));
   r.check("full, it sinks (denser than water); empty, it floats",buoyancy::density(c)>1000&&buoyancy::density(e)<1000,f2(buoyancy::density(c),0)+" / "+f2(buoyancy::density(e),0));}}});
}
}

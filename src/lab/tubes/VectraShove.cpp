// Props against people and things by their weight (RigidClutter.cpp's body contacts and breaking), on legacy
// and on Vectra side by side, and the chained mattress (PropBodies.h: three segments on springy hinges).
//
// vectra-shove: a mug, a crate and a drum thrown or rolled into a standing guard and into a creature: how far
//  each is shoved and what each blow costs it (the shove by momentum, the damage by the blow's energy past a
//  threshold); a heavy and a light box dropped on a glass floor (the heavy one breaks it); a mug under a
//  dropped crate (crushed) and a mug dropped on its own (whole); a light and a heavy carried crate as a shield
//  (the heavy one takes more blows before it is torn away).
// mattress: carried by one end it sags; dropped half over a step it drapes over the edge; struck at one end it
//  wobbles and settles; left lying it does not stir.
// vectra-feel: the pictures (fixtures feel-shove, feel-throw, feel-mattress).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropBodies.h"
#include "world/CampaignKit.h"
#include "world/ModelIds.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
namespace retro::lab {
namespace {
std::string f2(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
Clutter prop(int model,Vec2 at,float z,float scale=1){Clutter c;c.kind=7;c.model=model;c.dims=Game::propTraits(model).size;for(auto& d:c.dims)d*=scale;c.pos=at;c.z=z;c.sleeping=false;return c;}
Clutter weighing(Clutter c,float kg,float health=-1){auto t=std::make_shared<EntityTraits>();t->mass=kg;t->health=health;c.traits=t;return c;}
struct Thing {const char* name;Clutter c;Vec2 v;float vz;};
// A prop sent at a target 1.6 m off along +x (a standing guard, or a creature), 1.5 s: the target's shove and its hurt.
struct Hit {float shove=0,hurt=0;};
Hit throwAt(bool legacy,bool creature,const Thing& t){
 vectra::forceLegacyProps(legacy?1:0);auto m=regression::room();m->definition.playerStart={2,2};const Vec2 at{11,8};
 if(!creature)m->actorTracks.push_back(regression::guard(at,at));
 Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};LabAccess::setGod(g,true);
 if(creature){LabAccess::spawnCreature(g,{CreatureKind::Mutant,at});LabAccess::enemies(g)[0].pos=at;}
 auto where=[&]{return creature?LabAccess::enemies(g)[0].pos:LabAccess::friendlies(g)[0].pose.position;};
 for(int i=0;i<10;++i)g.update({},1.f/60);const Vec2 start=where();const float hp=creature?LabAccess::enemies(g)[0].hp:0;
 auto c=t.c;c.pos={at.x-(t.vz==0?1.f:1.6f),at.y};c.velocity=t.v;c.vz=t.vz;c.projectile=true;c.sleeping=false;LabAccess::clutter(g)={c};
 float shove=0;for(int i=0;i<90;++i){g.update({},1.f/60);shove=std::max(shove,where().x-start.x);if(creature){auto& e=LabAccess::enemies(g)[0];e.awareness=0;}
  if(std::getenv("RM_SHOVE_TRACE")&&!legacy){static std::ofstream tr("vectra-shove-trace.txt");for(const auto& e:LabAccess::rigidStats(g).events)if(e.b>=0)tr<<t.name<<(creature?" creature":" guard")<<" frame "<<i<<" event "<<e.a<<"-"<<e.b<<" n "<<f2(e.normal.x)<<","<<f2(e.normal.y)<<","<<f2(e.normal.z)<<" approach "<<f2(e.approach)<<" impulse "<<f2(e.impulse)<<std::endl;
   for(const auto& k:LabAccess::bodyKnocks(g))tr<<t.name<<" frame "<<i<<" knock v "<<f2(k.velocity.x)<<","<<f2(k.velocity.y)<<" t "<<f2(k.time)<<" at "<<f2(where().x,3)<<std::endl;}}
 Hit h;h.shove=shove;h.hurt=creature?hp-LabAccess::enemies(g)[0].hp:float(g.state(stateId("reg_guard_damage")));vectra::forceLegacyProps(-1);return h;}

const bool shove=add({.name="vectra-shove",.summary="props against people and things by weight: thrown and rolled into a guard and a creature, dropped on glass and on a mug, held up as a shield; legacy and Vectra",.standalone=[]{
 std::ofstream r("vectra-shove.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){r<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;r.flush();};
 r<<"Legacy against Vectra (the checks are Vectra's), Game::update at 60 Hz.\n\n";
 // A mug thrown (0.35 kg at 9 m/s), a crate thrown (9 kg at 4.7 m/s, the player's throw), a drum rolled (35 kg at 3 m/s).
 const Thing things[]={{"mug",prop(model::mug,{},.9f),{9,0},.6f},{"crate",prop(model::wooden_crate,{},.5f,.6f),{4.7f,0},.8f},{"drum",[]{auto d=prop(model::ind_barrel,{},0,1);d.yaw=kPi*.5f;d.pitch=kPi*.5f;return d;}(),{3,0},0}};
 Hit hit[2][2][3];
 for(int legacy=0;legacy<2;++legacy)for(int creature=0;creature<2;++creature)for(int k=0;k<3;++k)hit[legacy][creature][k]=throwAt(legacy==1,creature==1,things[k]);
 for(int creature=0;creature<2;++creature){r<<(creature?"A CREATURE (mutant)":"A STANDING GUARD")<<": shoved (m) and hurt\n";
  for(int k=0;k<3;++k)r<<"  "<<things[k].name<<": legacy "<<f2(hit[1][creature][k].shove,3)<<" m, "<<f2(hit[1][creature][k].hurt,1)<<"; Vectra "<<f2(hit[0][creature][k].shove,3)<<" m, "<<f2(hit[0][creature][k].hurt,1)<<"\n";}
 for(int creature=0;creature<2;++creature){const auto& v=hit[0][creature];const std::string who=creature?"a creature":"a guard";
  check(v[0].shove<.03f&&v[1].shove>.06f&&v[2].shove>v[1].shove,"a mug barely nudges "+who+"; a thrown crate shoves it back; a rolling drum shoves it harder");
  check(v[0].hurt>0&&v[1].hurt>v[0].hurt*3,"a thrown mug stings "+who+"; a thrown crate hurts it badly");}
 // Glass: a 20 kg and a 1 kg box dropped 1.2 m onto a glass floor.
 {float broke[2][2]{};
  for(int legacy=0;legacy<2;++legacy)for(int heavy=0;heavy<2;++heavy){vectra::forceLegacyProps(legacy);auto m=regression::room();m->definition.playerStart={2,2};kit::breakable(*m,7,7,10,10,1.f,1.03f,17,false);
   Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};LabAccess::clutter(g)={weighing(prop(model::packing_crate,{8.5f,8.5f},2.25f,.5f),heavy?20.f:1.f)};
   for(int i=0;i<120;++i)g.update({},1.f/60);broke[legacy][heavy]=g.clutter()[0].z<.6f?1.f:0.f;}   // through the glass (a hole its size, or the pane gone)
  vectra::forceLegacyProps(-1);
  r<<"\nGLASS FLOOR: a 20 kg box dropped 1.2 m on it  legacy "<<(broke[1][1]?"breaks it":"does not")<<", Vectra "<<(broke[0][1]?"breaks it":"does not")<<"; a 1 kg box  legacy "<<(broke[1][0]?"breaks it":"does not")<<", Vectra "<<(broke[0][0]?"breaks it":"does not")<<"\n";
  check(broke[0][1]>0&&broke[0][0]==0,"a heavy box dropped on a glass floor breaks it; a light one dropped as far does not");}
 // Crushing: a crate dropped 0.4 m onto a mug, and the mug dropped 0.4 m by itself.
 {bool crushed[2]{},alone[2]{};
  for(int legacy=0;legacy<2;++legacy){vectra::forceLegacyProps(legacy);
   {Game g(regression::campaign(regression::room()));LabAccess::player(g).pos={2,2};auto mug=prop(model::mug,{8,8},0);mug.sleeping=true;LabAccess::clutter(g)={mug,weighing(prop(model::wooden_crate,{8,8},.5f,.6f),20)};
    for(int i=0;i<90;++i){g.update({},1.f/60);if(std::getenv("RM_SHOVE_TRACE")&&!legacy)for(const auto& e:LabAccess::rigidStats(g).events)r<<"  frame "<<i<<" event "<<e.a<<"-"<<e.b<<" approach "<<f2(e.approach)<<" impulse "<<f2(e.impulse)<<" mass "<<f2(e.effectiveMass)<<" mug z "<<f2(g.clutter()[0].z,3)<<(g.clutter()[0].sleeping?" asleep":"")<<"\n";}crushed[legacy]=g.clutter()[0].broken;}
   {Game g(regression::campaign(regression::room()));LabAccess::player(g).pos={2,2};LabAccess::clutter(g)={prop(model::mug,{8,8},.4f)};for(int i=0;i<90;++i)g.update({},1.f/60);alone[legacy]=g.clutter()[0].broken;}}
  vectra::forceLegacyProps(-1);
  r<<"CRUSHING: a 20 kg crate dropped 0.4 m onto a mug  legacy "<<(crushed[1]?"crushes it":"leaves it")<<", Vectra "<<(crushed[0]?"crushes it":"leaves it")<<"; the mug dropped 0.4 m alone  legacy "<<(alone[1]?"breaks":"whole")<<", Vectra "<<(alone[0]?"breaks":"whole")<<"\n";
  check(crushed[0]&&!alone[0],"a heavy crate dropped on a mug crushes it; the mug dropped as far alone stays whole");}
 // The shield: a 4 kg and a 20 kg crate held up against 15-point blows until it is torn from the hands.
 {int blows[2][2]{};
  for(int legacy=0;legacy<2;++legacy)for(int heavy=0;heavy<2;++heavy){vectra::forceLegacyProps(legacy);Game g(regression::campaign(regression::room()));auto& p=LabAccess::player(g);p.pos={8.6f,8};p.angle=0;p.pitch=0;
   auto crate=weighing(prop(model::wooden_crate,{9.4f,8},0,.7f),heavy?20.f:4.f,0);   /* one that does not burst: torn away */LabAccess::clutter(g)={crate};LabAccess::grab(g);int n=0;
   for(;n<40&&LabAccess::held(g)==0;++n){LabAccess::shieldBlow(g,15,p.pos+Vec2{3,0});g.update({},1.f/60);}blows[legacy][heavy]=n;}
  vectra::forceLegacyProps(-1);
  r<<"SHIELD: blows a carried crate takes before it is torn away  4 kg legacy "<<blows[1][0]<<" Vectra "<<blows[0][0]<<"; 20 kg legacy "<<blows[1][1]<<" Vectra "<<blows[0][1]<<"\n";
  check(blows[0][1]>blows[0][0],"a heavier crate held up as a shield takes more blows before it is torn away");}
 return ok;}});

// ---- The mattress ----
// A segment's own middle (world).
vectra::V3 middle(const Clutter& c,int k){const auto ch=vectra::chainOf(c);const vectra::V3 ax=ch.along==0?vectra::V3{1,0,0}:vectra::V3{0,1,0};return ch.frame[size_t(k)]+ch.turn[size_t(k)].rotate(ax*(float(k-1)*ch.length/3));}
struct Pad {std::vector<Clutter> frames;Vec2 player{};};
const bool pad=add({.name="mattress",.summary="the chained mattress: sags carried by one end, drapes over a step, wobbles when struck and settles, still at rest",.standalone=[]{
 std::ofstream r("mattress.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){r<<(pass?"PASS ":"FAIL ")<<what<<'\n';ok&=pass;r.flush();};
 auto mattress=[](Vec2 at,float z,float yaw=0){auto c=prop(model::med_mattress_stained,at,z);c.yaw=yaw;return c;};
 auto bendOf=[](const Clutter& c){return std::max(std::fabs(c.bend[0]),std::fabs(c.bend[1]));};
 r<<"The mattress (med_mattress_stained, 1.9 m, 9 kg) on Vectra: three segments on springy hinges. Legacy has no bones (a rigid box).\n\n";
 // 1. Carried by one end.
 {Game g(regression::campaign(regression::room()));auto& p=LabAccess::player(g);p.pos={8,8};p.angle=0;p.pitch=0;LabAccess::clutter(g)={mattress({9.2f,8},0)};
  for(int i=0;i<10;++i)g.update({},1.f/60);const bool grabbed=LabAccess::grab(g)&&LabAccess::held(g)==0;float worst=0;for(int i=0;i<120;++i){g.update({},1.f/60);if(i>60)worst=std::max(worst,bendOf(g.clutter()[0]));}
  const auto& c=g.clutter()[0];const float drop=middle(c,c.boneRoot).z-middle(c,2-c.boneRoot).z;
  r<<"1 CARRIED BY ONE END: grabbed "<<(grabbed?"yes":"no")<<", held by segment "<<c.boneRoot<<", bends "<<f2(c.bend[0])<<", "<<f2(c.bend[1])<<" rad, the far end hangs "<<f2(drop)<<" m below the near\n";
  check(grabbed&&c.boneRoot!=1&&worst>.3f,"carried by one end, the mattress sags");}
 // 2. Dropped half over a step's edge.
 {auto step=[](AuthoredMapData& m){m.structures.push_back({10,4,16,12,0,.4f,false,40});};auto m=regression::room();step(*m);Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};
  LabAccess::clutter(g)={mattress({10.25f,8},.5f)};for(int i=0;i<240;++i)g.update({},1.f/60);const auto& c=g.clutter()[0];
  float lowest=1e9f,highest=-1e9f;for(int k=0;k<3;++k){lowest=std::min(lowest,middle(c,k).z);highest=std::max(highest,middle(c,k).z);}
  r<<"2 DRAPED OVER A STEP: bends "<<f2(c.bend[0])<<", "<<f2(c.bend[1])<<" rad, its segments' middles from z "<<f2(lowest)<<" to "<<f2(highest)<<", asleep "<<(c.sleeping?"yes":"no")<<"\n";
  check(bendOf(c)>.4f&&lowest<.3f&&c.sleeping,"dropped half over a step, it drapes over the edge (its overhang down toward the floor) and comes to rest");}
 // 3. Struck at one end (a round's blow), and 4. left lying.
 {Game g(regression::campaign(regression::room()));LabAccess::player(g).pos={2,2};LabAccess::clutter(g)={mattress({10,8},0)};for(int i=0;i<60;++i)g.update({},1.f/60);
  const auto rest=g.clutter()[0];float still=0;for(int i=0;i<120;++i){g.update({},1.f/60);still=std::max({still,bendOf(g.clutter()[0]),length(g.clutter()[0].pos-rest.pos)});}
  const bool slept=g.clutter()[0].sleeping;
  r<<"4 AT REST: over 2 s it moved or bent at most "<<f2(still*1000,2)<<" mm/mrad, asleep "<<(slept?"yes":"no")<<"\n";
  check(still<.002f&&slept,"left lying, it does not stir");
  const auto& c=g.clutter()[0];LabAccess::knock(g,0,{c.pos.x+.9f,c.pos.y,c.z+c.height()},{0,0,25});float most=0;int moving=0;
  for(int i=0;i<240;++i){g.update({},1.f/60);most=std::max(most,bendOf(g.clutter()[0]));if(!g.clutter()[0].sleeping)moving=i+1;}
  r<<"3 STRUCK AT ONE END: it bent up to "<<f2(most)<<" rad, was still after "<<f2(moving/60.f)<<" s, bends at the end "<<f2(g.clutter()[0].bend[0],3)<<", "<<f2(g.clutter()[0].bend[1],3)<<"\n";
  check(most>.1f&&moving<200&&bendOf(g.clutter()[0])<.05f,"struck at one end it wobbles, then settles flat and sleeps");}
 return ok;}});

// ---- The pictures ----
const bool feel=add({.name="vectra-feel",.summary="pictures: a crate shoving a guard, a heavy and a light throw, a mattress draped over a step",
 .fixtures={"feel-shove","feel-throw","feel-mattress"},
 .author=[](Scene& s,AuthoredMapData& m){const std::string k=s.fixture->value("feel","shove");m.definition.playerStart={2,2};
  if(k=="shove")m.actorTracks.push_back(regression::guard({11,8},{11,8},AiFaction::Player));
  if(k=="throw"){m.actorTracks.push_back(regression::guard({11,7.6f},{11,7.6f},AiFaction::Player));m.actorTracks.push_back(regression::guard({11,9.6f},{11,9.6f},AiFaction::Player));}
  if(k=="mattress")m.structures.push_back({10,4,16,12,0,.4f,false,40});},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);for(auto& npc:LabAccess::friendlies(s.g()))npc.frozen=true;},   // they stand where put; a knock still moves them
 .step=[](Scene& s,float){const std::string k=s.fixture->value("feel","shove");auto& l=LabAccess::clutter(s.g());
  if(s.frame==20){
   if(k=="shove"){auto c=prop(model::wooden_crate,{9.2f,8},.5f,.8f);c.velocity={5.5f,0};c.vz=.6f;c.projectile=true;l={c};}
   if(k=="throw"){auto a=prop(model::mug,{9.6f,9.6f},1.f);a.velocity={9,0};a.vz=.6f;a.projectile=true;auto b=prop(model::wooden_crate,{9.4f,7.6f},.5f,.8f);b.velocity={5.5f,0};b.vz=.6f;b.projectile=true;l={a,b};}
   if(k=="mattress"){auto c=prop(model::med_mattress_stained,{10.25f,8},.6f);l={c};}}},
 .capture=[](Scene& s,const Frame&){if(s.frame==19)s.shoot("a-before");if(s.frame==34)s.shoot("b-contact");if(s.frame==60)s.shoot("c-after");if(s.last())s.shoot("d-end");},
 .metrics=[](Scene& s,Report& r){r.check("ran",true);}});
}
}

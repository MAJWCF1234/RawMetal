// Creatures meeting the room: a Brute routing through a breakable door to the
// player, and the Chimera under fire. These tubes reproduce the three engine
// bugs the Starter Map's showcase found (2026-11-12), so they fail until fixed:
//   1 a Brute behind a SOLID breakable door walks to the leaf and gives up
//     without battering it (a see-through grille leaf it does batter); the
//     doors test forces awareness every frame and so misses it;
//   2 a shot Chimera hunter vanishes in plain view, short of its run;
//   3 a feeding Chimera seems to take no damage from shots.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
Vec2 pairAt(const Fixture& f,const char* key,Vec2 fallback){Vec2 v=fallback;const auto t=f.value(key);if(!t.empty())std::sscanf(t.c_str(),"%f %f",&v.x,&v.y);return v;}
Enemy* find(Game& g,CreatureKind kind){for(auto& e:LabAccess::enemies(g))if(e.kind==kind)return &e;return nullptr;}
// The player (the camera) turned and tipped to look at a point.
void lookAt(Game& g,Point3 at){Player& p=LabAccess::player(g);const float dx=at.x-p.pos.x,dy=at.y-p.pos.y,dz=at.z-(p.z+p.stanceEye());p.angle=std::atan2(dy,dx);p.pitch=std::atan2(dz,std::hypot(dx,dy))*140.f;}

// ------------------------------------------------------------------ brute
const bool brute=add({
 .name="brute",.summary="a Brute that has seen the player beyond a locked breakable door comes to it and beats it down (starter-map bug 1: a solid leaf is never battered)",
 .friends={"doors"},.fixtures={"brute-door-solid","brute-door-grille"},
 .author=[](Scene& s,AuthoredMapData& m){
  if(m.doors.empty())m.doors.push_back({7,10,8});auto& d=m.doors[0];
  d.swinging=true;d.leaf=int(s.fixture->number("door-leaf",0));d.strength=s.fixture->number("door-strength",120);d.requireState=stateId("lab_door_key");d.requireValue=1;d.lock=1;
  const Vec2 at=pairAt(*s.fixture,"brute-at",{8.5f,4.f});m.creatureSpawns.push_back({CreatureKind::Brute,at,0});},
 .setup=[](Scene& s){
  // The player in play (not an inspection camera), unhurt, seen once: the Brute is told where he is at the start and not again.
  Game& g=s.g();LabAccess::playerCamera(g);LabAccess::setGod(g,true);
  if(auto* e=find(g,CreatureKind::Brute)){e->awareness=60;e->recognition=1;e->lastKnown=g.player().pos;e->lastKnownZ=g.player().z;}},
 .capture=[](Scene& s,const Frame&){
  Game& g=s.g();const auto* e=find(g,CreatureKind::Brute);const World& w=g.world();if(!e||w.doors().empty())return;const auto& d=w.doors()[0];
  const float gap=std::hypot(e->pos.x-d.centre().x,e->pos.y-d.centre().y);s.board["brute.closest"]=s.board.count("brute.closest")?std::min(s.board["brute.closest"],double(gap)):gap;
  if(d.damage>s.board["brute.damage"]){s.board["brute.damage"]=d.damage;s.board["brute.blows"]+=1;if(!s.board.count("brute.first_blow_s"))s.board["brute.first_blow_s"]=s.time;}
  if(d.broken&&!s.board.count("brute.broke_s"))s.board["brute.broke_s"]=s.time;
  if(e->pos.y>d.y+.4f&&!s.board.count("brute.through_s"))s.board["brute.through_s"]=s.time;
  s.board["brute.awareness_end"]=e->awareness;},
 .metrics=[](Scene& s,Report& r){
  const World& w=s.g().world();if(w.doors().empty()){r.check("the tube has a door",false);return;}const auto& d=w.doors()[0];
  r.metric("leaf",d.leaf);r.metric("closest_m",s.board["brute.closest"],"m");r.metric("blows",s.board["brute.blows"]);r.metric("damage",d.damage);r.metric("broken",d.broken);r.metric("awareness_end",s.board["brute.awareness_end"]);
  const std::string leaf=d.leaf==1?"grille":"solid";
  r.check("it comes to the door",s.board["brute.closest"]<2.5,"closest "+fixed(float(s.board["brute.closest"]))+" m");
  r.check("it starts battering the "+leaf+" leaf",s.board["brute.blows"]>0,fixed(float(s.board["brute.blows"]),0)+" blows");
  r.check("[starter-map bug 1] it keeps on until the "+leaf+" leaf gives, and comes through",d.broken&&s.board.count("brute.through_s")>0,
   d.broken?"broken at "+fixed(float(s.board["brute.broke_s"]))+" s":"gave up after "+fixed(float(s.board["brute.blows"]),0)+" blows: damage "+fixed(d.damage,0)+" of "+fixed(d.strength,0)+", the door held");},
 .reporter=[](Scene& s,Overlay& o){
  Game& g=s.g();const auto* e=find(g,CreatureKind::Brute);if(!e)return;const Point3 p{e->pos.x,e->pos.y,e->z+1.2f};
  o.point(p,0xff6040,4);o.line(p,{e->lastKnown.x,e->lastKnown.y,e->lastKnownZ+1.f},0x804020);
  o.label({p.x,p.y,p.z+.6f},"BRUTE AWARE "+fixed(e->awareness,0)+" BLOWS "+fixed(float(s.board["brute.blows"]),0),0xff8060);},
 .with={{"doors",[](Scene& s,Report& r){
  // With the doors feature in the tube too, its lock and strength replace the fixture's; the handoff still has to happen.
  r.check("the door's state reaches the Brute's route (damage, then broken)",s.board["brute.blows"]>0);}}}});

// ------------------------------------------------------------------ chimera under fire
const bool shot=add({
 .name="chimera-shot",.summary="the Chimera under fire: feeding it must take the shot (bug 3); hurt past its threshold it runs and is gone only out of sight (bug 2)",
 .friends={"chimera","ragdoll"},.fixtures={"chimera-flee"},
 .author=[](Scene& s,AuthoredMapData& m){
  if(s.has("chimera"))return; // the feed scene's own Chimera is the one shot
  // Alone: a hunter with no quarry, woken at once (testChimeraHunt part 3).
  CreatureSpawn c{CreatureKind::Chimera,pairAt(*s.fixture,"chimera-at",{9.f,12.f}),0};c.hunter=stateId("lab_shot_hunter");m.creatureSpawns.push_back(c);},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Shotgun);LabAccess::playerCamera(g);},
 .step=[](Scene& s,float){
  Game& g=s.g();Enemy* e=find(g,CreatureKind::Chimera);if(!e)return;
  if(s.has("chimera")){
   // Bug 3: once it feeds, aim at its body and fire twice.
   if(e->feeding>0&&!s.board.count("shot.at")){s.board["shot.at"]=s.frame;s.board["shot.hp_before"]=e->hp;s.board["shot.feeding_before"]=e->feeding;}
   if(s.board.count("shot.at")){const int since=s.frame-int(s.board["shot.at"]);lookAt(g,{e->pos.x,e->pos.y,e->z+.55f});s.input.fire=since==1||since==40;
    if(since==70){s.board["shot.hp_after"]=e->hp;s.board["shot.feeding_after"]=e->feeding;}}
   return;}
  // Bug 2: hurt past its threshold at frame 10, then watch it run; note whether it is gone while in sight.
  if(s.frame==10){LabAccess::damageEnemy(g,*e,e->maxHp*.65f);s.board["shot.retreat"]=e->stalkMode==Enemy::StalkMode::Retreat;s.board["shot.from"]=0;}
  if(s.frame>10&&e->alive){const auto p=project(s,{e->pos.x,e->pos.y,e->z+.6f});const bool inFrame=p.visible&&p.x>=0&&p.y>=0&&p.x<s.renderer->width()&&p.y<s.renderer->height();
   const Player& pl=g.player();const bool clear=g.world().rayClear(pl.pos,pl.z+pl.stanceEye(),e->pos,e->z+.6f);
   s.board["shot.seen"]=inFrame&&clear;s.board["shot.dist"]=std::hypot(e->pos.x-pl.pos.x,e->pos.y-pl.pos.y);s.points["shot.last"]={{e->pos.x,e->pos.y,e->z}};}
  if(s.frame>10&&e->gone()&&!s.board.count("shot.gone_s")){s.board["shot.gone_s"]=s.time;s.board["shot.gone_seen"]=s.board["shot.seen"];s.board["shot.gone_dist"]=s.board["shot.dist"];}},
 .metrics=[](Scene& s,Report& r){
  Game& g=s.g();
  if(s.has("chimera")){
   if(!s.board.count("shot.at")){r.check("it feeds, to be shot at",false,"never fed");return;}
   const double before=s.board["shot.hp_before"],after=s.board.count("shot.hp_after")?s.board["shot.hp_after"]:-1;
   r.metric("hp_before",before);r.metric("hp_after",after);r.metric("hp_per_round",(before-after)/2);r.metric("feeding_after",s.board["shot.feeding_after"]);
   r.check("[starter-map bug 3] two shotgun rounds into it while it feeds hurt it",after>=0&&after<before,"hp "+fixed(float(before),0)+" -> "+fixed(float(after),0));
   r.check("hurt, it leaves the body",s.board["shot.feeding_after"]<=0,"feeding "+fixed(float(s.board["shot.feeding_after"]))+" s left");
   return;}
  r.metric("retreat",s.board["shot.retreat"]);r.check("past its threshold it runs",s.board["shot.retreat"]>0);
  const bool gone=s.board.count("shot.gone_s")>0;r.metric("gone_s",gone?s.board["shot.gone_s"]:-1,"s");r.metric("gone_dist_m",gone?s.board["shot.gone_dist"]:-1,"m");
  r.check("[starter-map bug 2] it is never gone while the player can see it",!gone||s.board["shot.gone_seen"]==0,gone?std::string(s.board["shot.gone_seen"]>0?"vanished in plain view ":"got away out of sight ")+fixed(float(s.board["shot.gone_dist"]))+" m off at "+fixed(float(s.board["shot.gone_s"]))+" s":"still about");
  (void)g;},
 .reporter=[](Scene& s,Overlay& o){
  Game& g=s.g();const Enemy* e=find(g,CreatureKind::Chimera);
  if(e&&e->alive){const Point3 p{e->pos.x,e->pos.y,e->z+.6f};o.point(p,0xff40ff,4);o.label({p.x,p.y,p.z+.5f},"HP "+fixed(e->hp,0)+(e->stalkMode==Enemy::StalkMode::Retreat?" RETREAT":"")+(e->feeding>0?" FEEDING":""),0xff80ff);}
  for(const auto& p:s.points["shot.last"]){o.cross(p,.3f,0xffff40);o.label(p,s.board.count("shot.gone_s")?"GONE HERE":"LAST SEEN",0xffff40);}}});
}
}

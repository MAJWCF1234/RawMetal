// Console summons (src/game/Summon.cpp): bodies typed into being with a side and
// an order, then left to fight it out. Each fixture is one match-up, staged by the
// console lines the testers would type:
//   allies   an allied Chimera against a hostile Huntsman and Snatcher on a hunt
//   guards   two allied soldiers against a hostile Chimera on a hunt
//   raiders  two hostile guards and two Mutants: creatures and guards fight each other
//   neutral  a neutral worker and a neutral wasp near a hunting Brute: neutrals run
// The numbers: who struck whom first and when, what is left of each side, how far
// the neutrals got, and that the player (in god mode, so the run is not cut short)
// was struck only by what is hostile to him.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fx(double v,int d=1){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// Type lines, turning the player to face `yawDeg` first (summons stand in front of him).
void typeAt(Game& g,float yawDeg,const std::string& line){Player& p=LabAccess::player(g);const float keep=p.angle;p.angle=yawDeg*kPi/180.f;LabAccess::console(g,line);p.angle=keep;}
struct Side {int alive=0,count=0;float hp=0,maxHp=0;};
Side creatures(Game& g,AiFaction f){Side s;for(const auto& e:LabAccess::enemies(g))if(e.summon&&e.faction==f){++s.count;s.alive+=e.alive;s.hp+=std::max(0.f,e.hp);s.maxHp+=e.maxHp;}return s;}
Side men(Game& g,AiFaction f){Side s;for(size_t i=0;i<LabAccess::friendlies(g).size();++i){const auto& n=LabAccess::friendlies(g)[i];if(!n.summon)continue;const auto& t=n.track();if(g.npcFaction(t)!=f)continue;
  const int damage=t.damageState?g.state(t.damageState):0;const bool down=(t.deadState&&g.state(t.deadState))||n.pose.clip==4;++s.count;s.alive+=!down;s.hp+=float(std::max(0,t.health-damage));s.maxHp+=float(t.health);}return s;}
float damageTaken(const Side& s){return s.maxHp-s.hp;}
}
const bool summonTube=add({
 .name="summon",.summary="console summons with allegiances: allies fight hostiles, creatures and guards fight each other, neutrals run (Summon.cpp)",
 .fixtures={"summon-allies","summon-guards","summon-raiders","summon-neutral"},
 .setup=[](Scene& s){
  Game& g=s.g();LabAccess::playerCamera(g);LabAccess::setGod(g,true);const std::string scene=s.fixture->value("summon-scene");
  if(scene=="allies"){typeAt(g,0,"summon chimera ally ai=guard");typeAt(g,40,"summon huntsman enemy ai=hunt");typeAt(g,-40,"summon snatcher enemy ai=hunt");}
  else if(scene=="guards"){typeAt(g,-30,"summon soldier 2 ally");typeAt(g,30,"summon chimera enemy ai=hunt");}
  else if(scene=="raiders"){typeAt(g,-35,"summon guard 2 enemy");typeAt(g,35,"summon mutant 2 enemy ai=hunt");}
  else if(scene=="neutral"){typeAt(g,0,"summon civilian neutral");typeAt(g,-45,"summon wasp neutral");typeAt(g,45,"summon brute enemy ai=hunt");}
  s.board["summon.made"]=double(LabAccess::summons(g).size());s.board["summon.player_hp0"]=g.player().health;
  for(const auto& line:LabAccess::consoleLog(g))if(line.find("SUMMONED")!=std::string::npos||line.find("NO CLEAR")!=std::string::npos)std::printf("  [summon] %s\n",line.c_str());},
 .capture=[](Scene& s,const Frame&){
  Game& g=s.g();const std::string scene=s.fixture->value("summon-scene");
  auto note=[&](const char* key,bool happened){if(happened&&!s.board.count(key))s.board[key]=s.time;};
  if(scene=="allies"){note("summon.ally_struck_s",damageTaken(creatures(g,AiFaction::Creature))>0);note("summon.ally_hurt_s",damageTaken(creatures(g,AiFaction::Facility))>0);}
  if(scene=="guards"){note("summon.chimera_shot_s",damageTaken(creatures(g,AiFaction::Creature))>0);note("summon.men_hurt_s",damageTaken(men(g,AiFaction::Facility))>0);}
  if(scene=="raiders"){note("summon.mutant_hurt_s",damageTaken(creatures(g,AiFaction::Creature))>0);note("summon.guard_hurt_s",damageTaken(men(g,AiFaction::Raider))>0);}
  if(scene=="neutral"){Vec2 brute{};for(const auto& e:LabAccess::enemies(g))if(e.faction==AiFaction::Creature)brute=e.pos;
   float worker=0,wasp=0;for(const auto& n:LabAccess::friendlies(g))if(n.summon)worker=length(n.pose.position-brute);for(const auto& e:LabAccess::enemies(g))if(e.faction==AiFaction::Neutral)wasp=length(e.pos-brute);
   if(s.frame==1){s.board["summon.worker_gap0"]=worker;s.board["summon.wasp_gap0"]=wasp;}
   s.board["summon.worker_gap"]=worker;s.board["summon.wasp_gap"]=wasp;
   s.board["summon.wasp_hurt"]=0;for(const auto& e:LabAccess::enemies(g))if(e.faction==AiFaction::Neutral)s.board["summon.wasp_hurt"]=e.maxHp-e.hp;}},
 .metrics=[](Scene& s,Report& r){
  Game& g=s.g();const std::string scene=s.fixture->value("summon-scene");
  auto t=[&](const char* key){return s.board.count(key)?s.board[key]:-1.0;};
  r.metric("summoned",s.board["summon.made"]);
  auto sideMetrics=[&](const std::string& tag,const Side& v){r.metric(tag+"_alive",v.alive);r.metric(tag+"_count",v.count);r.metric(tag+"_hp",v.hp);};
  if(scene=="allies"){const auto ally=creatures(g,AiFaction::Facility),foe=creatures(g,AiFaction::Creature);sideMetrics("ally",ally);sideMetrics("hostile",foe);
   r.metric("first_blow_on_hostiles_s",t("summon.ally_struck_s"),"s");r.metric("ally_first_hurt_s",t("summon.ally_hurt_s"),"s");
   r.check("three summoned (1 ally, 2 hostile)",s.board["summon.made"]==3&&ally.count==1&&foe.count==2,fx(s.board["summon.made"],0));
   r.check("the ally and the hostiles fight (blows both ways)",t("summon.ally_struck_s")>=0&&t("summon.ally_hurt_s")>=0,"ally hit them at "+fx(t("summon.ally_struck_s"))+" s, they hit it at "+fx(t("summon.ally_hurt_s"))+" s");
   r.check("the allied Chimera kills at least one hostile",foe.alive<2,std::to_string(foe.alive)+" of 2 hostiles alive, ally "+fx(ally.hp,0)+" hp");}
  if(scene=="guards"){const auto ally=men(g,AiFaction::Facility);const auto foe=creatures(g,AiFaction::Creature);sideMetrics("soldiers",ally);sideMetrics("chimera",foe);
   r.metric("chimera_first_hit_s",t("summon.chimera_shot_s"),"s");r.metric("soldier_first_hurt_s",t("summon.men_hurt_s"),"s");
   r.check("two allied soldiers and a hostile chimera",ally.count==2&&foe.count==1);
   r.check("the soldiers shoot the chimera",t("summon.chimera_shot_s")>=0,"chimera at "+fx(foe.hp,0)+" of "+fx(foe.maxHp,0)+" hp");
   r.check("the chimera goes for the soldiers",t("summon.men_hurt_s")>=0,"soldiers at "+fx(ally.hp,0)+" of "+fx(ally.maxHp,0)+" hp");}
  if(scene=="raiders"){const auto guards=men(g,AiFaction::Raider);const auto mutants=creatures(g,AiFaction::Creature);sideMetrics("guards",guards);sideMetrics("mutants",mutants);
   r.metric("mutant_first_hurt_s",t("summon.mutant_hurt_s"),"s");r.metric("guard_first_hurt_s",t("summon.guard_hurt_s"),"s");
   r.check("hostile guards shoot hostile creatures",t("summon.mutant_hurt_s")>=0,"mutants at "+fx(mutants.hp,0)+" of "+fx(mutants.maxHp,0)+" hp");
   r.check("hostile creatures attack hostile guards",t("summon.guard_hurt_s")>=0,"guards at "+fx(guards.hp,0)+" of "+fx(guards.maxHp,0)+" hp");}
  if(scene=="neutral"){r.metric("worker_gap_start_m",s.board["summon.worker_gap0"],"m");r.metric("worker_gap_end_m",s.board["summon.worker_gap"],"m");
   r.metric("wasp_gap_start_m",s.board["summon.wasp_gap0"],"m");r.metric("wasp_gap_end_m",s.board["summon.wasp_gap"],"m");r.metric("wasp_damage",s.board["summon.wasp_hurt"]);
   r.check("the brute leaves the neutral wasp alone",s.board["summon.wasp_hurt"]==0,fx(s.board["summon.wasp_hurt"],0)+" damage");
   r.check("the neutral wasp runs from the brute",s.board["summon.wasp_gap"]>s.board["summon.wasp_gap0"]+1,fx(s.board["summon.wasp_gap0"])+" m -> "+fx(s.board["summon.wasp_gap"])+" m");
   r.check("the neutral worker runs from the brute",s.board["summon.worker_gap"]>s.board["summon.worker_gap0"]+1,fx(s.board["summon.worker_gap0"])+" m -> "+fx(s.board["summon.worker_gap"])+" m");}},
 .reporter=[](Scene& s,Overlay& o){
  Game& g=s.g();
  for(const auto& e:LabAccess::enemies(g))if(e.summon){const Point3 p{e.pos.x,e.pos.y,e.bodyTop()+.3f};const unsigned c=e.faction==AiFaction::Facility?0x40ff60u:e.faction==AiFaction::Neutral?0xc0c0c0u:0xff5040u;
   o.point(p,c,4);o.label({p.x,p.y,p.z+.3f},std::string(e.name())+" "+fx(e.hp,0),c);}
  for(const auto& n:LabAccess::friendlies(g))if(n.summon){const Point3 p{n.pose.position.x,n.pose.position.y,n.pose.z+n.track().scale+.3f};const auto f=g.npcFaction(n.track());const unsigned c=f==AiFaction::Facility?0x40ff60u:f==AiFaction::Neutral?0xc0c0c0u:0xff5040u;
   o.point(p,c,4);}}});
}

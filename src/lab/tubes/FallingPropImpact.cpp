// A modeled prop falling straight onto a stationary body must deliver its blow before
// overlap recovery moves it sideways. A fast fall can cross the old 12 cm head band
// between substeps; a normal fall can cross it at the end of a 180/240 Hz frame.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace retro::lab {
namespace {
struct FallRun {
 float damage=0,minZ=100,bodyDrift=0,finalZ=0;
 bool finite=true,aboveFloor=true;
};

FallRun fallOnBody(int rate,bool creature,int model,float speed,float gap){
 auto map=regression::room();map->definition.playerStart={3,3};
 Game game(regression::campaign(map));auto& player=LabAccess::player(game);
 player.pos=creature?Vec2{3,3}:Vec2{8,8};player.z=0;
 float head=player.hullHeight();
 if(creature){Enemy enemy;enemy.pos=enemy.home=enemy.lastKnown={8,8};
  LabAccess::enemies(game)={enemy};head=LabAccess::enemies(game)[0].bodyTop();}
 Clutter prop;prop.kind=7;prop.model=model;prop.dims={.4f,.4f,.4f};
 prop.pos={8,8};prop.z=head+gap;prop.vz=-speed;prop.sleeping=false;
 LabAccess::clutter(game)={prop};
 FallRun result;const float health=creature?LabAccess::enemies(game)[0].hp:player.health;
 for(int frame=0;frame<rate;++frame){
  LabAccess::stepProps(game,1.f/rate);
  for(const auto& object:LabAccess::clutter(game)){
   result.finite&=std::isfinite(object.pos.x)&&std::isfinite(object.pos.y)&&std::isfinite(object.z);
   result.minZ=std::min(result.minZ,object.z);
   result.aboveFloor&=object.z>=game.world().floorHeight(object.pos.x,object.pos.y)-.01f;
  }
  const Vec2 at=creature?LabAccess::enemies(game)[0].pos:player.pos;
  result.bodyDrift=std::max(result.bodyDrift,length(at-Vec2{8,8}));
 }
 result.damage=health-(creature?LabAccess::enemies(game)[0].hp:player.health);
 result.finalZ=LabAccess::clutter(game)[0].z;
 return result;
}

bool run(){
 std::ofstream report("falling-prop-impact-test.txt");bool ok=true;
 auto check=[&](bool pass,const std::string& label){report<<(pass?"PASS ":"FAIL ")<<label<<'\n';ok&=pass;};
 // Hand-selected launch phases straddle a head crossing at each solver frequency.
 // They begin above the head, with no horizontal velocity or initial overlap.
 struct Phase {int rate;float gap;};
 for(const Phase phase:{Phase{60,.08f},Phase{120,.02f},Phase{180,.08f},Phase{240,.02f}}){
  for(bool creature:{false,true}){
   const std::string label=std::string(creature?"creature":"player")+" at "+std::to_string(phase.rate)+" Hz";
   const auto light=fallOnBody(phase.rate,creature,model::keyboard,8,phase.gap);
   const auto heavy=fallOnBody(phase.rate,creature,model::crt_monitor,8,phase.gap);
   const auto fast=fallOnBody(phase.rate,creature,model::crt_monitor,40,phase.gap);
   report<<"fall "<<label<<": keyboard8m/s damage "<<light.damage
         <<", monitor8m/s damage "<<heavy.damage<<", monitor40m/s damage "<<fast.damage
         <<", fast finalZ "<<fast.finalZ<<", minZ "<<fast.minZ<<'\n';
   check(heavy.damage>0,"ordinary vertical fall damages the stationary "+label);
   check(light.damage>0&&heavy.damage>light.damage+2,"equal-speed falling damage scales with weight against the "+label);
   check(fast.damage>0,"fast fall cannot skip the stationary "+label+" between head-band samples");
   check(light.finite&&heavy.finite&&fast.finite&&light.aboveFloor&&heavy.aboveFloor&&fast.aboveFloor,
         "falling impact keeps finite prop positions above the world floor for the "+label);
   check(light.bodyDrift<.001f&&heavy.bodyDrift<.001f&&fast.bodyDrift<.001f,
         "a purely vertical fall does not invent a horizontal shove of the "+label);
  }
 }
 // A shelf under only the prop's right edge is encountered before the player's
 // head. A trace down the prop's center misses it, so the whole outline matters.
 {
  const float head=Player{}.hullHeight(),shelfTop=head+.008f;
  auto map=regression::room();map->definition.playerStart={7.92f,8};
  map->structures.push_back({8.15f,7.5f,8.4f,8.5f,head-.10f,shelfTop});
  Game game(regression::campaign(map));auto& player=LabAccess::player(game);
  player.pos={7.92f,8};player.z=0;
  Clutter prop;prop.kind=7;prop.model=model::crt_monitor;prop.dims={.4f,.4f,.4f};
  prop.pos={8,8};prop.z=player.hullHeight()+.012f;prop.vz=-4.1f;prop.sleeping=false;
  const bool bodyClear=!game.world().structureBlocksHull(player.pos.x,player.pos.y,.2f,player.z,player.hullHeight());
  const bool launchClear=prop.everyOutlinePoint([&](Vec2 offset){auto point=prop.pos+offset;return game.world().fits(point.x,point.y,prop.z,prop.height(),true,true);});
  LabAccess::clutter(game)={prop};const float health=player.health;
  LabAccess::stepProps(game,1.f/240);
  const auto& landed=LabAccess::clutter(game)[0];
  report<<"off-center shelf: head "<<player.hullHeight()<<", shelf top "<<shelfTop
        <<", damage "<<health-player.health<<", landed z "<<landed.z<<'\n';
  check(bodyClear&&launchClear,"off-center shelf staging clears the player's hull and the launched monitor");
  check(player.health==health,"a shelf under the monitor's edge prevents premature head damage");
  check(std::fabs(landed.z-shelfTop)<.001f,"the monitor lands on the nearer off-center shelf instead of the player's head");
 }
 report<<(ok?"ALL PASSED":"FAILED")<<" falling-prop-impact\n";return ok;
}
const bool registered=add({.name="falling-prop-impact",
 .summary="modeled props falling onto stationary player and creature bodies: mass-scaled vertical blows without skipped head contacts at 60/120/180/240 Hz",
 .standalone=run});
}
}

#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "world/ModelIds.h"
#include <fstream>

namespace retro::lab {
namespace {
Clutter approachingMonitor(Vec2 body,float radius){
 Clutter monitor;
 monitor.kind=7;
 monitor.model=model::crt_monitor;
 monitor.dims=Game::propTraits(monitor.model).size;
 monitor.pos={body.x-radius-monitor.extent()[0]-.01f,body.y};
 monitor.z=.18f;
 monitor.velocity={.8f,0};
 monitor.sleeping=false;
 return monitor;
}

bool checkWalkingBody(std::ostream& report,bool guard,bool approaching){
 constexpr Vec2 start{8,8};
 auto room=regression::room();
 room->definition.playerStart={3,3};
 if(guard)room->actorTracks.push_back(regression::guard(start,{8,12}));
 Game game(regression::campaign(room));
 float radius=.2f;
 if(guard){
  auto& pose=LabAccess::friendlies(game).at(0).pose;
  pose.clip=1;
  pose.yaw=approaching?-kPi*.5f:0.f;
 }else{
  Enemy enemy;
  enemy.pos=enemy.home=enemy.lastKnown=start;
  enemy.moving=true;
  enemy.heading=approaching?kPi:kPi*.5f;
  radius=enemy.radius();
  LabAccess::enemies(game)={enemy};
 }

 const auto monitor=approachingMonitor(start,radius);
 LabAccess::clutter(game)={monitor};

 // The prop advances only 3.3 mm on this tick, short of the initial 10 mm
 // gap. Walking must not kick it backwards before the two hulls meet.
 constexpr float dt=1.f/240;
 LabAccess::stepProps(game,dt);
 const auto& after=LabAccess::clutter(game).front();
 const bool approaches=after.pos.x>monitor.pos.x&&after.velocity.x>0;
 const char* body=guard?"guard":"creature";
 const char* heading=approaching?"towards the prop":"across its path";
 report<<(approaches?"PASS ":"FAIL ")<<"monitor advances across the gap to a "<<body
       <<" walking "<<heading<<" (first-tick vx "<<after.velocity.x<<")\n";

 // Advance only the ordinary prop phase so patrol locomotion cannot itself
 // produce the measured x-displacement. The walk pose stays active each tick.
 for(int tick=1;tick<24;++tick)LabAccess::stepProps(game,dt);
 const float x=guard?LabAccess::friendlies(game).front().pose.position.x:LabAccess::enemies(game).front().pos.x;
 const bool displaced=x>start.x+.001f;
 report<<(displaced?"PASS ":"FAIL ")<<"monitor contact displaces a "<<body
       <<" walking "<<heading<<" (body dx "<<x-start.x<<")\n";
 return approaches&&displaced;
}

bool checkPatrol(std::ostream& report){
 auto room=regression::room();
 room->definition.playerStart={3,3};
 room->actorTracks.push_back(regression::guard({8,8},{8,12}));
 const auto campaign=regression::campaign(room);
 Game game(campaign),baseline(campaign);
 constexpr float dt=1.f/120;
 const auto& pose=game.friendlyActors().at(0).pose;
 for(int frame=0;frame<240&&(pose.clip!=1||pose.position.y<8.03f);++frame){
  game.update({},dt);
  baseline.update({},dt);
 }
 if(pose.clip!=1||pose.position.y<8.03f){
  report<<"FAIL guard starts walking towards its authored patrol point (clip "<<pose.clip<<", y "<<pose.position.y<<")\n";
  return false;
 }
 const Vec2 before=pose.position;
 const auto monitor=approachingMonitor(before,.2f);
 LabAccess::clutter(game)={monitor};
 game.update({},dt);
 baseline.update({},dt);
 const float firstSpeed=LabAccess::clutter(game).front().velocity.x;
 const bool advances=firstSpeed>0&&LabAccess::clutter(game).front().pos.x>monitor.pos.x;
 for(int frame=1;frame<16;++frame){
  game.update({},dt);
  baseline.update({},dt);
 }
 const float lateral=pose.position.x-baseline.friendlyActors().at(0).pose.position.x;
 const bool displaced=lateral>.001f&&pose.position.y>before.y;
 report<<(advances?"PASS ":"FAIL ")<<"incoming monitor keeps advancing before reaching a patrolling guard (vx "<<firstSpeed<<")\n";
 report<<(displaced?"PASS ":"FAIL ")<<"patrol continues after the shove and retains its sideways displacement"
       <<" (x versus patrol alone "<<lateral<<", patrol dy "<<pose.position.y-before.y<<")\n";
 return advances&&displaced;
}

const bool propBodyContacts=add({
 .name="prop-body-contacts",
 .summary="a moving heavy prop reaches and displaces walking guards and creatures",
 .standalone=+[]{
  std::ofstream report("prop-body-contacts-test.txt");
  bool passed=true;
  for(bool guard:{true,false})for(bool approaching:{false,true})
   passed&=checkWalkingBody(report,guard,approaching);
  passed&=checkPatrol(report);
  return passed;
 }
});
}
}

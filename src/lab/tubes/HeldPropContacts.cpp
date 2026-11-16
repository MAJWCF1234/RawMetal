#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "world/ModelIds.h"
#include <fstream>

namespace retro::lab {
namespace {
bool holdMonitor(Game& game,float dt){
 auto& player=LabAccess::player(game);
 player.angle=player.pitch=0;
 Clutter monitor;
 monitor.kind=7;
 monitor.model=model::crt_monitor;
 monitor.dims=Game::propTraits(monitor.model).size;
 monitor.pos=player.pos+Vec2{.9f,0};
 monitor.z=player.z+player.eye-monitor.height()*.5f;
 LabAccess::clutter(game)={monitor};
 if(monitor.mass()!=11||!LabAccess::grab(game))return false;
 LabAccess::stepProps(game,dt);
 return game.holdingClutter();
}

bool checkReach(std::ostream& out,float dt,bool guard,bool pinned){
 auto room=regression::room();
 room->definition.playerStart={pinned?21.2f:6.5f,8};
 Enemy enemy;
 // Keep a tall creature across the monitor's eye-height path.
 enemy.scale=1.5f;
 const float radius=guard?.2f:enemy.radius();
 const Vec2 start{pinned?23.f-radius-.01f:8.f,8};
 if(guard)room->actorTracks.push_back(regression::guard(start,start));
 Game game(regression::campaign(room));
 enemy.pos=enemy.home=start;
 if(!guard)LabAccess::enemies(game)={enemy};
 if(!holdMonitor(game,dt)){out<<"FAIL monitor pickup\n";return false;}
 const auto player=game.player();
 const float originalX=game.clutter().front().pos.x;
 InputState reach;
 reach.weaponScroll=8;
 LabAccess::stepProps(game,dt,reach);
 // Holding the wheel setting must continue to respect a pinned body.
 for(int i=0;i<6;++i)LabAccess::stepProps(game,dt);
 const auto& monitor=game.clutter().front();
 const auto body=guard?game.friendlyActors().front().pose.position:game.enemies().front().pos;
 const float gap=body.x-radius-monitor.pos.x-monitor.extent()[0];
 const bool unharmed=guard?game.state("reg_guard_damage")==0:game.enemies().front().hp==enemy.hp;
 const bool clear=gap>=-.002f&&std::fabs(body.y-start.y)<.01f;
 const bool distance=pinned?body.x+radius<=23.001f&&monitor.pos.x>originalX+.03f:
     body.x>start.x+.1f&&monitor.pos.x>=player.pos.x+2.49f;
 const bool ok=game.holdingClutter()&&unharmed&&clear&&distance&&length(game.player().pos-player.pos)<.001f;
 out<<(ok?"PASS ":"FAIL ")<<"extend held monitor toward "<<(pinned?"wall-pinned ":"")
    <<(guard?"guard":"creature")<<" at "<<1/dt<<" Hz (body dx "<<body.x-start.x
    <<", gap "<<gap<<", prop dx "<<monitor.pos.x-originalX<<", unharmed "<<unharmed<<")\n";
 return ok;
}

bool checkRotation(std::ostream& out,float dt){
 auto room=regression::room();
 room->definition.playerStart={6.5f,8};
 room->actorTracks.push_back(regression::guard({8,8},{8,8}));
 Game game(regression::campaign(room));
 if(!holdMonitor(game,dt)){out<<"FAIL monitor pickup for rotation\n";return false;}
 const auto before=game.clutter().front();
 auto& pose=LabAccess::friendlies(game).front().pose;
 pose.position={before.pos.x+before.extent()[0]+.21f,before.pos.y};
 const float start=pose.position.x;
 InputState turn;
 turn.rotateProp=true;
 turn.mouseDx=8;
 turn.mouseDy=2;
 for(int i=0;i<8;++i)LabAccess::stepProps(game,dt,turn);
 const auto& after=game.clutter().front();
 const bool ok=game.holdingClutter()&&std::fabs(after.yaw-before.yaw)>.1f&&
     std::fabs(after.pitch-before.pitch)>.02f&&length(after.pos-before.pos)<.005f&&
     pose.position.x>start+.005f&&game.state("reg_guard_damage")==0;
 out<<(ok?"PASS ":"FAIL ")<<"turning the held monitor in place pushes the guard at "<<1/dt
    <<" Hz (body dx "<<pose.position.x-start<<", yaw "<<after.yaw<<", pitch "<<after.pitch<<")\n";
 return ok;
}

bool checkWalk(std::ostream& out,float dt){
 auto room=regression::room();
 room->definition.playerStart={6.5f,8};
 room->actorTracks.push_back(regression::guard({8.25f,8},{8.25f,8}));
 const auto campaign=regression::campaign(room);
 Game game(campaign),baseline(campaign);
 if(!holdMonitor(game,dt)){out<<"FAIL monitor pickup for walk\n";return false;}
 InputState walk;
 walk.forward=true;
 for(int i=0;i<int(.35f/dt);++i){game.update(walk,dt);baseline.update(walk,dt);}
 const auto body=game.friendlyActors().front().pose.position;
 const auto& monitor=game.clutter().front();
 const float dx=body.x-baseline.friendlyActors().front().pose.position.x;
 const float gap=body.x-.2f-(monitor.pos.x+monitor.extent()[0]);
 const bool held=game.holdingClutter(),moved=game.player().pos.x>6.75f,pushed=dx>.03f,clear=gap>=-.003f,safe=game.state("reg_guard_damage")==0;
 const bool ok=held&&moved&&pushed&&clear&&safe;
 out<<(ok?"PASS ":"FAIL ")<<"walking forward with the monitor pushes a guard in Game::update at "
    <<1/dt<<" Hz (body dx versus walking empty-handed "<<dx<<", gap "<<gap
    <<", held "<<held<<", player x "<<game.player().pos.x<<", unharmed "<<safe<<")\n";
 return ok;
}

const bool heldPropContacts=add({
 .name="held-prop-contacts",
 .summary="carried heavy props sweep and push living bodies without impact damage",
 .standalone=+[]{
  std::ofstream out("held-prop-contacts-test.txt");
  bool ok=true;
  for(float dt:{1.f/60,1.f/120}){
   ok&=checkReach(out,dt,true,false);
   ok&=checkReach(out,dt,false,false);
   ok&=checkReach(out,dt,true,true);
   ok&=checkReach(out,dt,false,true);
   ok&=checkRotation(out,dt);
   ok&=checkWalk(out,dt);
  }
  return ok;
 }
});
}
}

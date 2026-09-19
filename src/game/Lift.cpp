#include "Game.h"
#include <fstream>
namespace retro {
bool World::startLift(){
 if(m_level!=3||m_liftPhase!=LiftPhase::Ready)return false;
 m_liftPhase=LiftPhase::Ascending;m_liftTimer=0;return true;
}
void World::updateLift(float dt){
 if(m_level!=3)return;
 // Fixed substeps keep the entire room's descent deterministic at 30/60/120 Hz.
 int steps=std::max(1,int(std::ceil(dt*240)));float step=dt/steps;
 for(int i=0;i<steps;++i){
  m_liftTimer+=step;
  switch(m_liftPhase){
   case LiftPhase::Ascending:
    m_liftHeight=std::min(9.f,m_liftHeight+1.5f*step);
    if(m_liftHeight>=9){m_liftPhase=LiftPhase::Jammed;m_liftTimer=0;}
    break;
   case LiftPhase::Jammed:
    if(m_liftTimer>=2.5f){m_liftPhase=LiftPhase::Falling;m_liftVelocity=0;m_liftTimer=0;}
    break;
   case LiftPhase::Falling:
    m_liftVelocity-=12.f*step;m_liftHeight+=m_liftVelocity*step;
    if(m_liftHeight<=-9){m_liftHeight=-9;m_liftVelocity=0;m_liftPhase=LiftPhase::Crashed;m_controlReleased=true;m_liftTimer=0;}
    break;
   default:break;
  }
 }
 if(m_terminals.size()>1)m_terminals[1].z=9+m_liftHeight;
 if(!m_lights.empty())m_lights.back().z=m_liftHeight+2.45f;
}
void World::restoreLift(const World& saved){
 m_liftPhase=saved.m_liftPhase;m_liftHeight=saved.m_liftHeight;m_liftTimer=saved.m_liftTimer;m_liftVelocity=saved.m_liftVelocity;
 if(m_level==3&&m_terminals.size()>1)m_terminals[1].z=9+m_liftHeight;
 if(m_level==3&&!m_lights.empty())m_lights.back().z=m_liftHeight+2.45f;
}
const char* World::liftStatus()const{
 switch(m_liftPhase){
  case LiftPhase::Ready:return "BOARD LIFT / SURFACE DISPATCH";
  case LiftPhase::Ascending:return "ASCENDING / SURFACE ACCESS";
  case LiftPhase::Jammed:return "GATES JAMMED / CABLE OVERLOAD";
  case LiftPhase::Falling:return "CABLE FAILURE / EMERGENCY BRAKE";
  case LiftPhase::Crashed:return "REACTOR / CAB DISABLED";
 }return "";
}
void Game::updateLift(float dt){
 if(m_level!=3)return;
 auto phase=m_world.liftPhase();float old=m_world.liftHeight();
 bool passenger=m_world.insideLift(m_player.pos.x,m_player.pos.y)&&m_player.z>=old-.1f&&m_player.z<old+2.6f;
 m_world.updateLift(dt);float delta=m_world.liftHeight()-old;
 if(passenger){m_player.z+=delta;
  // The cab is a closed moving room. Keep the passenger hull inside its walls,
  // including during jumps and the scripted six-storey fall.
  if(m_world.liftMoving()||phase==World::LiftPhase::Falling){
   m_player.pos.x=std::clamp(m_player.pos.x,10.33f,13.67f);
   m_player.pos.y=std::clamp(m_player.pos.y,10.33f,13.67f);
  }
 }
 for(int i=0;i<int(m_clutter.size());++i){auto&c=m_clutter[i];if(i!=m_heldClutter&&m_world.insideLift(c.pos.x,c.pos.y)&&std::fabs(c.z-old)<.15f)c.z+=delta;}
 if(phase!=m_world.liftPhase()){
  m_logTime=0;m_activeLog=-1;
  if(m_world.liftPhase()==World::LiftPhase::Jammed){sound(Sound::Exit,.6f,.55f);sound(Sound::LiftCreak,.9f,.8f);}
  if(m_world.liftPhase()==World::LiftPhase::Falling){sound(Sound::LiftSnap,1.f,1.1f);m_verticalSpringVelocity-=.5f;}
  if(m_world.liftPhase()==World::LiftPhase::Crashed){sound(Sound::LiftCrash,1.f,.8f);m_verticalSpringVelocity-=1.4f;m_damageFlash=.4f;}
 }
}
Game Game::liftInspection(float seconds,int view){
 auto game=mapInspection({12,11},kPi*.5f,0,3,false,0,true);game.m_world.startLift();
 for(int i=0;i<int(seconds*120);++i)game.update({},1.f/120);
 if(view==1){game.m_player.pos={12,16.5f};game.m_player.z=-9;game.m_player.pitch=30;}
 if(view==2){game.m_player.pos={16,17};game.m_player.z=-6;game.m_player.angle=2.45f;game.m_player.pitch=-25;}
 if(view==3){game.m_player.angle=.25f;game.m_player.pitch=15;}
 return game;
}
bool Game::testLift(){
 std::ofstream report("lift-test.txt");
 auto check=[&](bool ok,const char* label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();return ok;};
 World world(3);
 if(!check(world.layers().size()==7,"Seven stacked map layers"))return false;
 for(auto&layer:world.layers())for(auto row:layer.rows)if(row.size()!=24)return check(false,"Map width");
 for(auto&s:world.structures())if(s.top<s.bottom)return check(false,"Inverted structure");
 if(!check(world.supportBelow(17.5f,20.5f,-6)==-6&&world.supportBelow(17.5f,20.5f,-9)==-9,"Two reactor floors at same XY"))return false;
 if(!check(!world.rayClear({17.5f,20.5f},-8,{17.5f,20.5f},-5),"Vertical sight rays cannot pass through decks"))return false;
 auto game=mapInspection({3.5f,1.5f},kPi*.5f,0,3,false,0,true);game.m_clutter.clear();
 auto walk=[&](Vec2 target){for(int i=0;i<2200;++i){auto delta=target-game.player().pos;if(game.won()||length(delta)<.12f)return true;
   InputState input{};input.forward=true;input.mouseDx=wrapAngle(std::atan2(delta.y,delta.x)-game.player().angle)/.0022f;game.update(input,1.f/120);}
   report<<"Stopped at "<<game.player().pos.x<<','<<game.player().pos.y<<','<<game.player().z<<'\n';return false;};
 for(Vec2 p: {Vec2{3.5f,6.f},Vec2{8.f,6.f},Vec2{12,6.f},Vec2{12,11.5f}})if(!check(walk(p),"Boarding route"))return false;
 if(!check(std::fabs(game.player().z)<.01f,"Cab at collar height"))return false;
 game.m_player.angle=0;game.m_velocity={};InputState use{};use.use=true;game.update(use,.01f);
 if(!check(game.world().liftPhase()==World::LiftPhase::Ascending,"Dispatch via E inside cab"))return false;
 float top=-100;bool jammed=false,falling=false;bool impactSound=false;
 for(int i=0;i<1500;++i){game.update({},1.f/120);top=std::max(top,game.world().liftHeight());
  jammed|=game.world().liftPhase()==World::LiftPhase::Jammed;falling|=game.world().liftPhase()==World::LiftPhase::Falling;
  for(auto&e:game.sounds())impactSound|=e.sound==Sound::LiftCrash;
  if(std::fabs(game.player().z-game.world().liftHeight())>=.03f)return check(false,"Passenger rides floor");
 }
 if(!check(std::fabs(top-9)<.01f&&jammed&&falling&&game.world().liftHeight()==-9&&impactSound,"Rise three / fall six / crash sound"))return false;
 if(!check(!game.m_world.startLift()&&game.world().controlReleased(),"Crash is permanent, exit released"))return false;
 for(Vec2 p:{Vec2{11.5f,11.5f},Vec2{11.5f,15.5f},Vec2{18,15.5f},Vec2{18,11.f},Vec2{20,11.f},Vec2{20,18.5f}})if(!check(walk(p),"Reactor stairs route"))return false;
 if(!check(std::fabs(game.player().z+6)<.03f,"Reactor upper floor reached"))return false;
 game.storeChunk();game.m_chunks[3].world.unloadGeometry();game.m_chunks[3].resident=false;game.ensureChunk(3);
 if(!check(game.m_chunks[3].world.liftPhase()==World::LiftPhase::Crashed&&game.m_chunks[3].world.liftHeight()==-9,"Lift survives geometry unload"))return false;
 for(Vec2 p:{Vec2{20,11.f},Vec2{21.5f,11.f},Vec2{21.5f,20.3f}})if(!check(walk(p),"Return to lower reactor exit"))return false;
 game.m_player.angle=kPi*.5f;game.update({},.01f);game.update(use,.01f);
 for(int i=0;i<180;++i)game.update({},1.f/120);
 if(!check(game.world().doors().back().open>.95f,"Lower reactor interlock opens"))return false;
 if(!check(walk({21.5f,22.5f})||game.won(),"Lower reactor exit reached"))return false;
 if(!check(game.won(),"Authored route completes at reactor exit"))return false;
 for(int rate:{30,60,120}){auto ride=liftInspection(0);for(int i=0;i<rate*12;++i)ride.update({},1.f/rate);
  if(!check(ride.world().liftHeight()==-9&&std::fabs(ride.player().z+9)<.03f,"Frame-rate independent ride"))return false;}
 auto ride=liftInspection(0);InputState push{};push.right=true;push.jump=true;
 for(int i=0;i<600;++i){ride.update(push,1.f/120);if(!ride.world().insideLift(ride.player().pos.x,ride.player().pos.y)||ride.player().z<ride.world().liftHeight()-.03f)return check(false,"Moving room contains jumping passenger");}
 float pausedHeight=ride.world().liftHeight();InputState pause{};pause.escape=true;ride.update(pause,.02f);for(int i=0;i<60;++i)ride.update({},1.f/60);
 if(!check(ride.world().liftHeight()==pausedHeight,"Pause freezes lift"))return false;
 World doors(3);if(!check(doors.doorBlocks(21.5f,21.5f,-9,1)&&doors.nearbyDoor({21.5f,20.5f},{0,1},-6)==-1,"Door collision and interaction use floor height"))return false;
 auto bridge=mapInspection({12,9},kPi*.5f,0,3,false,0,true);InputState hop{};hop.right=true;hop.jump=true;
 for(int i=0;i<240;++i){hop.crouch=i>12;bridge.update(hop,1.f/120);if(bridge.player().z<-.05f)return check(false,"Boarding cage prevents premature shaft fall");}
 check(true,"Boarding cage prevents premature shaft fall");
 doors.openDoor(1);doors.updateDoors(2);if(doors.doorBlocks(21.5f,21.5f,-9,1))return false;
 game.restart();return check(game.world().liftPhase()==World::LiftPhase::Ready,"Restart resets lift");
}
}

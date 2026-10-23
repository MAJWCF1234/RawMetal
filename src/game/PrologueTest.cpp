#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
#include <filesystem>

namespace retro {
// Views: 0 desk at shift start, 1 chat open, 2 corridor during the alarm,
// 3 lobby facing the waiting cab, 4 inside the cab after impact.
Game Game::prologueInspection(int stage){
 Game g(loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Security-Office-Prologue.txt"));
 auto frames=[&](int count){for(int i=0;i<count;++i)g.update({},1.f/60);};
 auto place=[&](Vec2 p,float z,Vec2 target,float pitch){g.m_player.pos=p;g.m_player.z=z;g.m_player.angle=std::atan2(target.y-p.y,target.x-p.x);g.m_player.pitch=pitch;};
 frames(120);
 if(stage>=1){InputState use;use.use=true;g.update(use,1.f/60);frames(stage==1?60*8:60*25);}
 if(stage==2)place({13.f,5.5f},0,{18.f,5.8f},-4);
 if(stage==3)place({17.f,10.5f},0,{17.f,17.f},-8);
 // 5-8: wide from the south-east corner, the doorway, over the shoulder at the desk, west side.
 if(stage>=5){const Vec2 eye[]={{9.1f,9.1f},{9.6f,6.f},{4.6f,5.6f},{2.7f,5.f}},look[]={{4.2f,3.f},{3.f,4.f},{4.6f,3.f},{6.5f,3.f}};const float pitch[]={-10,-8,-24,-12};place(eye[stage-5],0,look[stage-5],pitch[stage-5]);}
 if(stage==4){place({17.f,16.8f},0,{17.f,13.f},0);g.update({},1.f/60);g.setState(stateId("office_drop"),1);frames(60*4);place({16.f,17.5f},g.player().z,{18.f,15.2f},10);}
 g.m_logTime=std::min(g.m_logTime,1.f);g.equipWeapon(WeaponId::Fists);g.update({},1.f/60);
 return g;
}
// Plays the Security Office prologue with real input: open the chat, wait out
// the alarm, run to the service lift, ride the drop and hand off to Map 0.
bool Game::testPrologue(){
 std::ofstream report("prologue-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 try{
  auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Security-Office-Prologue.txt");
  Game g(campaign);g.m_player.health=100000;
  auto frames=[&](int count,InputState input={}){for(int i=0;i<count;++i)g.update(input,1.f/60);};
  frames(2);
  check(g.activeObjectiveText()=="Answer the message on your workstation","Shift starts on the workstation objective");
  check(g.nearbyTerminal()>=0,"Spawn faces a usable workstation");
  frames(120);
  check(g.state("office_lift_arrived")!=0,"Service lift is waiting at the security level before the alarm");
  InputState use;use.use=true;g.update(use,1.f/60);frames(1);
  check(g.state("office_chat")>0,"E opens the Lab 4 chat");
  frames(60*20);
  check(g.state("office_chat")>0&&!g.state("office_run"),"Chat plays before the alarm without a premature run order");
  frames(60*7);
  check(g.state("office_alarm")>0&&g.state("office_run")!=0,"Alarm follows the chat and orders the run");
  check(g.activeObjectiveText()=="Get to the service lift","Run objective replaces the workstation objective");
  auto walk=[&](Vec2 target){for(int i=0;i<900&&length(g.player().pos-target)>.15f;++i){auto d=target-g.player().pos;g.m_player.angle=std::atan2(d.y,d.x);InputState step;step.forward=true;step.sprint=true;g.update(step,1.f/60);}return length(g.player().pos-target)<=.15f;};
  bool ran=walk({8.f,5.5f})&&walk({10.5f,5.5f})&&walk({17.f,5.8f})&&walk({17.f,13.f})&&walk({17.f,16.8f});
  report<<"player="<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z<<'\n';
  check(ran&&std::fabs(g.player().z)<.1f,"Real movement runs from the desk through the corridor onto the lift");
  frames(60*4);
  check(g.state("office_lift_bottom")!=0&&g.player().z< -8.5f,"Cable failure drops the cab and its passenger to the sub-level");
  check(g.state("office_crash")>0,"Impact starts the crash beat");
  frames(60*6);
  check(g.worldId()==WorldId::Campaign&&g.level()==0&&!g.titleScreen()&&g.player().health==100,"Prologue hands off to the main campaign's first map");
 }catch(const std::exception&e){report<<"Runtime exception: "<<e.what()<<'\n';check(false,"Prologue runs without an exception");}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

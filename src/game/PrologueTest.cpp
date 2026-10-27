#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace retro {
// Views: 0 desk at shift start, 1 chat open, 2 corridor during the alarm,
// 3 lobby facing the waiting cab, 4 inside the cab after impact. The office
// floor is z 9 (the shaft floor is level with the Foundry's).
Game Game::prologueInspection(int stage){
 Game g;g.startNewGame();
 auto frames=[&](int count){for(int i=0;i<count;++i)g.update({},1.f/60);};
 auto place=[&](Vec2 p,float z,Vec2 target,float pitch){g.m_player.pos=p;g.m_player.z=z;g.m_player.angle=std::atan2(target.y-p.y,target.x-p.x);g.m_player.pitch=pitch;};
 frames(120);
 if(stage>=1){InputState use;use.use=true;g.update(use,1.f/60);frames(stage==1?60*8:60*25);}
 if(stage==2)place({13.f,5.5f},9,{18.f,5.8f},-4);
 if(stage==3)place({17.f,10.5f},9,{17.f,17.f},-8);
 // 5-8: wide from the south-east corner, the doorway, over the shoulder at the desk, west side.
 if(stage>=5){const Vec2 eye[]={{9.1f,9.1f},{9.6f,6.f},{4.6f,5.6f},{2.7f,5.f}},look[]={{4.2f,3.f},{3.f,4.f},{4.6f,3.f},{6.5f,3.f}};const float pitch[]={-10,-8,-24,-12};place(eye[stage-5],9,look[stage-5],pitch[stage-5]);}
 // 9-10: standing against corridor walls (close-range post and lamp checks).
 if(stage==9)place({18.3f,5.6f},9,{20.f,5.6f},-4);
 if(stage==10)place({13.f,5.15f},9,{13.4f,3.f},2);
 if(stage==4){place({17.f,16.8f},9,{17.f,13.f},0);g.update({},1.f/60);g.setState(stateId("office_drop"),1);frames(60*4);place({16.f,17.5f},g.player().z,{18.f,15.2f},10);}
 g.m_logTime=std::min(g.m_logTime,1.f);g.equipWeapon(WeaponId::Fists);g.update({},1.f/60);
 return g;
}
// UI captures: 0 title, 1 HUD armed, 2 pause menu, 3 death, 4 inventory, 5 prologue objective.
Game Game::uiInspection(int view){
 if(view==5)return prologueInspection(0);
 Game g;
 if(view==0){g.showTitleScreen();g.update({},1.f/60);return g;}
 for(int i=0;i<30;++i)g.update({},1.f/60);
 if(view==2)g.m_paused=true;
 if(view==3)g.m_player.health=0;
 if(view==4)g.m_inventoryOpen=true;
 return g;
}
// Plays the Security Office prologue with real input: open the chat, wait out
// the alarm, run to the service lift, ride the drop, open the cab door and walk
// the shaft passage into Map 0 without the session restarting.
bool Game::testPrologue(){
 std::ofstream report("prologue-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 try{
  {Game menu;menu.showTitleScreen();menu.m_titleSelection=0;InputState accept{};accept.menuAccept=true;menu.update(accept,.01f);menu.update({},.01f);
   check(menu.worldId()==WorldId::Campaign&&menu.level()==PrologueChunk&&!menu.titleScreen(),"New Game from the title opens the Security Office on the campaign grid");}
  {bool listed=false;for(const auto& pack:retro::loadCustomCampaignDirectory(std::filesystem::path("custom maps")))listed|=pack->sourceFile=="RawMetal-Security-Office-Prologue.txt";
   check(!listed,"The prologue belongs to the main campaign, not the Custom Maps list");}
  auto saves=std::filesystem::temp_directory_path()/("rawmetal-prologue-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(saves);
  Game g;g.startNewGame();g.m_saveDirectory=saves.wstring();
  auto frames=[&](int count,InputState input={}){for(int i=0;i<count;++i)g.update(input,1.f/60);};
  frames(2);
  check(g.activeObjectiveText()=="Answer the message on your workstation","Shift starts on the workstation objective");
  check(g.nearbyTerminal()>=0,"Spawn faces a usable workstation");
  frames(120);
  check(g.state("office_lift_arrived")!=0,"Service lift is waiting at the security level before the alarm");
  InputState use;use.use=true;g.update(use,1.f/60);frames(1);
  check(g.state("office_chat")>0,"E opens the Lab 4 chat");
  frames(60*12);
  {int desk=g.activeLog();check(desk>=0&&g.logTime()>0&&g.terminalMessages(desk).size()>=3&&g.terminalConversationActive(desk),"Workstation stays open past the old 9 s limit while Lab 4 is still messaging");}
  frames(60*8);
  check(g.state("office_chat")>0&&!g.state("office_run"),"Chat plays before the alarm without a premature run order");
  check(g.alarmPulse()==0,"No alarm wash during the quiet chat");
  frames(60*7);
  check(g.state("office_alarm")>0&&g.state("office_run")!=0,"Alarm follows the chat and orders the run");
  check(g.activeObjectiveText()=="Get to the service lift","Run objective replaces the workstation objective");
  {Game resumed;resumed.m_saveDirectory=saves.wstring();bool loaded=resumed.loadCheckpoint();check(loaded&&resumed.worldId()==WorldId::Campaign&&resumed.level()==PrologueChunk&&resumed.state("office_run")&&resumed.activeObjectiveText()=="Get to the service lift","Run checkpoint reloads the embedded prologue in a fresh session");}
  {float peak=0;for(int i=0;i<120;++i){g.update({},1.f/60);peak=std::max(peak,g.alarmPulse());}check(peak>.8f,"Alarm klaxon drives the red beacon wash");}
  auto walk=[&](Vec2 target){for(int i=0;i<900&&length(g.player().pos-target)>.15f;++i){auto d=target-g.player().pos;g.m_player.angle=std::atan2(d.y,d.x);InputState step;step.forward=true;step.sprint=true;g.update(step,1.f/60);}return length(g.player().pos-target)<=.15f;};
  bool ran=walk({8.f,5.5f});check(g.logTime()==0,"Walking away from the desk closes the computer");ran=ran&&walk({10.5f,5.5f})&&walk({17.f,5.8f})&&walk({17.f,13.f})&&walk({17.f,16.8f});
  report<<"player="<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z<<'\n';
  check(ran&&std::fabs(g.player().z-9)<.1f,"Real movement runs from the desk through the corridor onto the lift");
  const unsigned revision=g.sessionRevision();
  frames(60*4);
  check(g.state("office_lift_bottom")!=0&&g.player().z<.5f,"Cable failure drops the cab and its passenger to the sub-level");
  check(g.state("office_crash")>0,"Impact starts the crash beat");
  {bool early=walk({17.f,18.6f});g.m_player.angle=kPi*.5f;InputState open;open.use=true;g.update(open,1.f/60);frames(60);
   InputState push;push.forward=true;for(int i=0;i<60;++i)g.update(push,1.f/60);
   report<<"settling="<<early<<' '<<g.player().pos.x<<','<<g.player().pos.y<<" done="<<g.state("office_prologue_done")<<'\n';
   check(g.state("office_prologue_done")||g.player().pos.y<19.3f,"The cab door holds while the crash is still settling");}
  frames(60*6);
  check(g.worldId()==WorldId::Campaign&&g.level()==PrologueChunk&&g.activeObjectiveText()=="Get out of the shaft","The crash leaves you in the shaft with a way out to find");
  {g.m_player.angle=kPi*.5f;InputState open;open.use=true;g.update(open,1.f/60);frames(90);}
  bool out=walk({17.f,20.6f})&&walk({3.5f,20.6f})&&walk({3.5f,23.5f});
  report<<"passage="<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z<<" level "<<g.level()<<'\n';
  check(out,"The cab door opens on the shaft passage, which runs to the Foundry seam");
  for(int i=0;i<120&&g.level()!=0;++i){g.m_player.angle=kPi*.5f;InputState step;step.forward=true;g.update(step,1.f/60);}
  check(g.worldId()==WorldId::Campaign&&g.level()==0&&g.sessionRevision()==revision&&!g.titleScreen()&&g.player().health==100,"Walking down the passage streams into the Foundry: no restart, no cut");
  check(walk({3.5f,4.f})&&g.objective(stateId("office_out"))==ObjectiveStatus::Complete,"The passage comes out in the Foundry's lift alcove and the shaft objective is done");
  std::error_code error;std::filesystem::remove_all(saves,error);
 }catch(const std::exception&e){report<<"Runtime exception: "<<e.what()<<'\n';check(false,"Prologue runs without an exception");}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

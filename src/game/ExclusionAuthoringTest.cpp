#include "Game.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace retro {
bool Game::testExclusionAuthoring(){
 std::ofstream report("exclusion-authoring-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*name){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<name<<'\n';report.flush();};
 const std::string start="--- CUSTOM_CAMPAIGN_DATA_START ---\n",end="--- CUSTOM_CAMPAIGN_DATA_END ---\n";
 std::string base="CAMPAIGN|Script parser fixture|0\nMAP|0|Room|0|0|4.5|6.5|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row)base+="ROW|0|0|"+std::to_string(row)+"|........................\n";
 const std::string label="OBJECTIVE_TEXT|0|test_objective|Use the local relay\n";
 const std::string event="SCRIPT_EVENT|0|test_event|0|0|24|24|-1|4||1|0\n";
 const std::string stateAction="SCRIPT_ACTION|0|test_event|STATE|test_flag|1\n";
 auto parse=[&](const std::string&records){return loadCustomCampaignText(start+base+records+end,"script-parser-fixture.txt");};
 auto rejects=[&](const std::string&records){try{parse(records);return false;}catch(const std::exception&){return true;}};
 try{
  auto parsed=parse(label+event+stateAction+"SCRIPT_ACTION|0|test_event|OBJECTIVE|test_objective|1\nSCRIPT_ACTION|0|test_event|ITEM|flashlight|1\nSCRIPT_ACTION|0|test_event|COMPLETE\nSCRIPT_ACTION|0|test_event|CHECKPOINT\n");
  const auto&map=*parsed->maps.front();const auto&authored=map.scriptEvents.front();
  check(map.objectiveTexts.size()==1&&map.objectiveTexts[0].text=="Use the local relay"&&authored.once&&authored.actions.size()==5,"Named objectives and the five bounded action kinds parse into existing engine data");
  check(authored.actions[3].type==ScriptAction::Type::CompleteCampaign&&authored.actions[4].type==ScriptAction::Type::Checkpoint,"Parser preserves completion-before-checkpoint action order");
 }catch(const std::exception&e){report<<e.what()<<'\n';check(false,"Valid script authoring parses");}
 check(rejects(event+"SCRIPT_ACTION|0|test_event|TELEPORT|somewhere|1\n"),"Unknown action kinds are rejected");
 check(rejects("SCRIPT_ACTION|0|missing_event|STATE|test_flag|1\n"),"Actions require an existing named event in their map");
 check(rejects(event+"SCRIPT_ACTION|0|test_event|STATE|test_flag\n"),"Named actions require the exact field count");
 check(rejects(event+"SCRIPT_ACTION|0|test_event|CHECKPOINT|extra\n"),"Checkpoint actions reject extra fields");
 check(rejects("SCRIPT_EVENT|0|bad|-1|0|24|24|-1|4||1|0\n"+stateAction),"Event regions cannot leave their chunk");
 check(rejects("SCRIPT_EVENT|0|test_event|4|0|4|24|-1|4||1|0\n"+stateAction),"Degenerate event regions are rejected");
 check(rejects("SCRIPT_EVENT|0|test_event|0|0|24|24|4|-1||1|0\n"+stateAction),"Inverted vertical event regions are rejected");
 check(rejects("SCRIPT_EVENT|0|test_event|nan|0|24|24|-1|4||1|0\n"+stateAction),"Nonfinite event coordinates are rejected");
 check(rejects("SCRIPT_EVENT|0|test_event|0|0|24|24|-1|4||1|2\n"+stateAction),"Enemy-clear gate must be a boolean");
 check(rejects(event+stateAction+event+stateAction),"Duplicate event names are rejected");
 check(rejects(event),"Events require at least one action");
 check(rejects(event+"SCRIPT_ACTION|0|test_event|ITEM|flashlight|0\n")&&rejects(event+"SCRIPT_ACTION|0|test_event|ITEM|flashlight|100\n"),"Item grants are bounded to one through ninety-nine");
 check(rejects(label+event+"SCRIPT_ACTION|0|test_event|OBJECTIVE|test_objective|4\n"),"Objective actions reject unknown statuses");
 check(rejects(event+"SCRIPT_ACTION|0|test_event|STATE|test_flag|100001\n"),"State actions reject out-of-range values");
 check(rejects(event+"SCRIPT_ACTION|0|test_event|OBJECTIVE|unnamed_objective|1\n"),"Every objective action requires a display label");
 check(rejects(label+label),"Duplicate objective labels are rejected");
 check(rejects("OBJECTIVE_TEXT|0|test_objective|"+std::string(81,'x')+"\n")&&rejects("OBJECTIVE_TEXT|0|test_objective|Bad%0Aline\n"),"Objective labels reject excessive length and decoded control characters");
 std::string tooMany=event;for(int i=0;i<17;++i)tooMany+=stateAction;
 check(rejects(tooMany),"Event action count is bounded");
 std::filesystem::path taskDirectory;
 try{
  auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt");Game game(campaign);
  taskDirectory=std::filesystem::temp_directory_path()/("rawmetal-exclusion-authoring-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(taskDirectory);game.m_saveDirectory=taskDirectory.wstring();
  // The zone is 5 x 4 maps of 24 m; quest terminals are found by the state
  // they set, then used from whichever side the player can reach them.
  auto operate=[&](StateId expected){
   for(int level=0;level<game.chunkCount();++level)for(const auto& terminal:game.chunkWorld(level).terminals()){
    if(terminal.activateState!=expected)continue;
    for(int k=0;k<8;++k){float a=k*kPi/4;Vec2 stand=terminal.position+Vec2{std::cos(a),std::sin(a)}*.85f;
     if(game.level()!=level){game.storeChunk();game.loadLevel(level,true);game.updateStreaming(0);}
     game.m_player.pos=stand;game.m_player.z=game.groundHeight(stand,float(World::TerrainMaxZ+1));
     game.m_player.angle=std::atan2(terminal.position.y-stand.y,terminal.position.x-stand.x);game.m_logTime=0;game.m_activeLog=-1;game.m_previousUse=false;
     int nearby=game.nearbyTerminal();
     report<<"  terminal at "<<terminal.position.x<<','<<terminal.position.y<<" map "<<level<<" stand "<<stand.x<<','<<stand.y<<" nearby="<<nearby<<" hull="<<game.hullFits(stand,game.player().z,Player::StandingHeight)<<'\n';
     if(nearby<0||game.world().terminals()[size_t(nearby)].activateState!=expected||!game.hullFits(stand,game.player().z,Player::StandingHeight))continue;
     InputState use;use.use=true;game.updateInteraction(use,.01f);game.updateScripts(0);return true;}
   }
   return false;
  };
  const auto elder=stateId("zone_obj_elder"),pda=stateId("zone_obj_pda"),tower=stateId("zone_obj_tower"),out=stateId("zone_obj_out");
  check(game.chunkCount()==20&&game.level()==15,"The zone starts at the rookie camp in its south-west map");
  game.updateScripts(0);
  check(game.objective(elder)==ObjectiveStatus::Active&&game.activeObjectiveText()=="Talk to the camp elder by the fire","Arrival shows the elder objective");
  check(operate(stateId("zone_pda"))&&!game.state("zone_pda"),"The courier's PDA stays locked until the elder has briefed the player");
  check(operate(stateId("zone_briefed"))&&game.state("zone_briefed"),"Real E at the elder's terminal records the briefing");
  game.updateScripts(0);
  check(game.objective(elder)==ObjectiveStatus::Complete&&game.objective(pda)==ObjectiveStatus::Active,"The briefing advances to the PDA objective");
  check(operate(stateId("zone_transmit"))&&!game.state("zone_transmit"),"The tower transmitter needs the relay code from the PDA");
  check(operate(stateId("zone_pda"))&&game.state("zone_pda"),"The PDA at the fuel station gives the relay code");
  game.updateScripts(0);
  check(game.objective(pda)==ObjectiveStatus::Complete&&game.objective(tower)==ObjectiveStatus::Active,"The PDA advances to the radio-tower objective");
  check(operate(stateId("zone_transmit"))&&game.state("zone_transmit"),"The radio tower transmits with the code");
  game.updateScripts(0);
  check(game.objective(tower)==ObjectiveStatus::Complete&&game.objective(out)==ObjectiveStatus::Active&&game.state("zone_complete"),"Transmitting completes the chain and leaves the Zone open to roam");
  for(StateId id:{elder,pda,tower,out})report<<"objective "<<int(game.objective(id))<<'\n';
 }catch(const std::exception&e){report<<"Runtime exception: "<<e.what()<<'\n';check(false,"The zone quest chain runs without an exception");}
 if(!taskDirectory.empty()){std::error_code error;std::filesystem::remove_all(taskDirectory,error);}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

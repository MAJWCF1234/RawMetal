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
  auto warp=[&](int level,Vec2 position,float angle){
   if(game.level()!=level){game.storeChunk();game.loadLevel(level,true);game.updateStreaming(0);}
   game.m_player.pos=position;game.m_player.z=game.groundHeight(position);game.m_player.angle=angle;game.m_logTime=0;game.m_activeLog=-1;game.m_previousUse=false;
  };
  auto operate=[&](int level,Vec2 position,float angle,StateId expected){
   warp(level,position,angle);int nearby=game.nearbyTerminal();bool correct=nearby>=0&&game.world().terminals()[size_t(nearby)].activateState==expected;
   InputState use;use.use=true;game.updateInteraction(use,.01f);game.updateScripts(0);return correct;
  };
  auto advance=[&](int frames){for(int i=0;i<frames;++i){game.m_sounds.clear();game.updateMechanisms(1.f/60);game.updateScripts(0);}};
  const auto cache=stateId("exclusion_cache_objective"),relay=stateId("exclusion_relay_objective"),winch=stateId("exclusion_winch_objective"),extract=stateId("exclusion_extract_objective");
  check(game.chunkCount()==3&&std::fabs(game.player().pos.x-4.1f)<.001f&&std::fabs(game.player().angle+kPi*.5f)<.001f,"Authored intro starts facing its usable arrival cache within the original three chunks");
  game.updateScripts(0);
  check(game.objective(cache)==ObjectiveStatus::Active&&!game.hasFlashlight()&&game.activeObjectiveText()=="Read the arrival cache brief and collect its flashlight","Arrival displays the named cache objective without silently granting a flashlight");
  check(operate(0,{4.3f,6.7f},kPi*.5f,stateId("exclusion_north_lure"))&&!game.state("exclusion_north_lure"),"North control respects the physical briefing/cache prerequisite");
  check(operate(0,{4.1f,6.2f},-kPi*.5f,stateId("exclusion_brief_read"))&&game.hasFlashlight()&&game.questItemCount(Flashlight)==1,"Real E interaction at the entry cache grants one owned flashlight");
  check(game.objective(cache)==ObjectiveStatus::Complete&&game.objective(relay)==ObjectiveStatus::Active&&std::filesystem::exists(taskDirectory/"checkpoint.rms"),"Cache acquisition advances the objective and writes a real checkpoint");
  operate(0,{4.1f,6.2f},-kPi*.5f,stateId("exclusion_brief_read"));game.updateScripts(0);
  check(game.questItemCount(Flashlight)==1,"Repeated briefing interaction cannot duplicate the cache item");
  Game cached(campaign);cached.m_saveDirectory=taskDirectory.wstring();
  check(cached.loadCheckpoint()&&cached.questItemCount(Flashlight)==1&&cached.objective(relay)==ObjectiveStatus::Active,"The authored cache checkpoint restores item ownership and objective progression");
  check(operate(0,{11.f,14.6f},-kPi*.5f,stateId("exclusion_relay_secured"))&&!game.state("exclusion_relay_secured"),"Facility relay stays gated until the north distraction completes");
  check(operate(2,{12.3f,3.6f},kPi*.5f,stateId("exclusion_extract_logged"))&&!game.state("exclusion_extract_logged")&&!game.m_won,"Early arrival at South Wash cannot bypass east-winch clearance or complete the map");
  check(operate(0,{4.3f,6.7f},kPi*.5f,stateId("exclusion_north_lure"))&&game.state("exclusion_north_lure")>0,"Briefed players start the authored north distraction through E");
  advance(310);
  check(game.state("exclusion_north_lure_done")!=0,"Existing mechanism timeline supplies the north completion gate");
  check(operate(0,{11.f,14.6f},-kPi*.5f,stateId("exclusion_relay_secured"))&&game.state("exclusion_relay_secured")&&game.objective(relay)==ObjectiveStatus::Complete&&game.objective(winch)==ObjectiveStatus::Active,"Securing the actual facility relay advances to the east-winch objective");
  check(operate(0,{17.3f,20.5f},-kPi*.5f,stateId("exclusion_east_lure"))&&game.state("exclusion_east_lure")>0,"Relay authorization enables the real east-winch terminal");
  advance(310);
  check(game.state("exclusion_east_lure_done")&&game.objective(winch)==ObjectiveStatus::Complete&&game.objective(extract)==ObjectiveStatus::Active&&!game.m_won&&game.activeObjectiveText()=="Reach South Wash and register departure at its relay","East-winch completion displays the cross-map departure objective without prematurely winning");
  Game staged(campaign);staged.m_saveDirectory=taskDirectory.wstring();
  check(staged.loadCheckpoint()&&staged.objective(extract)==ObjectiveStatus::Active&&staged.hasFlashlight(),"Protected south staging writes a checkpoint with the extraction objective");
  check(operate(2,{12.3f,3.6f},kPi*.5f,stateId("exclusion_extract_logged"))&&game.state("exclusion_complete")&&game.objective(extract)==ObjectiveStatus::Complete&&game.m_won,"Authorized departure terminal completes the existing ScriptEvent campaign arc");
  Game completed(campaign);completed.m_saveDirectory=taskDirectory.wstring();
  check(completed.loadCheckpoint()&&completed.m_won&&completed.state("exclusion_complete")&&completed.objective(extract)==ObjectiveStatus::Complete,"Completion precedes checkpoint so reloading the final autosave remains won");
  completed.updateScripts(0);
  check(completed.m_won&&completed.questItemCount(Flashlight)==1&&std::count(completed.m_firedEvents.begin(),completed.m_firedEvents.end(),stateId("exclusion_cache_taken"))==1,"Restored once-only events preserve completion and never replay the item award");
 }catch(const std::exception&e){report<<"Runtime exception: "<<e.what()<<'\n';check(false,"Authored objective/item/checkpoint progression runs without an exception");}
 if(!taskDirectory.empty()){std::error_code error;std::filesystem::remove_all(taskDirectory,error);}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

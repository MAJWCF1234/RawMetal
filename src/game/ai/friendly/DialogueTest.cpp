// Stance and dialogue validation: --stance-test, --dialogue-test.
#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
#include <cmath>
#include <fstream>

namespace retro {
namespace {
// Two guards of one squad on a single post each, facing the player's side of
// the room, their stance in "sq_stance"; the first opens "hello" with E.
Game stanceRoomBare(int stance,bool patrol=false){
 auto campaign=std::make_shared<CustomCampaign>();campaign->name="Stance validation";campaign->key=554411;
 auto map=std::make_shared<AuthoredMapData>();map->name="Stance room";map->definition={{0,0},{6.5f,3.f},0,Environment::Interior,false,8,.3f,42};
 AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
 TimedSequence timer;timer.timerState=stateId("sq_clock");timer.finishState=stateId("sq_done");timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
 for(int i=0;i<2;++i){Vec2 at{6.5f+i*2.f,6.5f};
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,at,0,kPi},{60000,at,0,kPi}};guard.scale=1.6f;guard.health=70;
  guard.damageState=stateId("sq_damage/"+std::to_string(i));guard.deadState=stateId("sq_dead/"+std::to_string(i));
  guard.ai.mode=ActorAiMode::Guard;guard.ai.armed=true;guard.ai.faction=AiFaction::Raider;guard.ai.squad=stateId("sq");guard.ai.stance=stateId("sq_stance");
  guard.ai.patrol={{at,0,1000}};if(patrol&&i==0)guard.ai.patrol.push_back({{at.x,at.y+6.f},0,1000});
  if(i==0)guard.talk=stateId("hello");map->actorTracks.push_back(guard);}
 DialogueNode hello{stateId("hello"),"CLEANUP CORPORAL","Easy. Nobody shoots unless you make us.","",{}};
 hello.replies.push_back({"Who are you people?",stateId("who")});
 {DialogueReply pass{"I have a pass."};pass.require=stateId("sq_pass");pass.sets={{stateId("sq_passed"),1}};hello.replies.push_back(pass);}
 {DialogueReply hand{"I could use a hand."};hand.sets={{stateId("sq_deal"),1}};hand.stance=int(SquadStance::Ally);hello.replies.push_back(hand);}
 hello.replies.push_back({"Never mind."});
 DialogueNode who{stateId("who"),"CLEANUP CORPORAL","Cleanup. What is left of it.","",{{"Back to it.",stateId("hello")}}};
 map->dialogue={hello,who};campaign->maps.push_back(map);
 Game g(campaign);g.setState(stateId("sq_stance"),stance);return g;
}
void run(Game&g,float seconds,const InputState&input={}){for(int i=0;i<int(seconds*60);++i)g.update(input,1.f/60);}
}

Game Game::dialogueInspection(){
 auto g=stanceRoomBare(int(SquadStance::Truce));g.m_player.pos={6.5f,4.9f};g.m_player.angle=kPi*.5f;g.setState(stateId("sq_pass"),1);
 InputState use{};use.use=true;g.update({},1.f/60);g.update(use,1.f/60);for(int i=0;i<90;++i)g.update({},1.f/60);g.m_talk.selected=1;return g;
}
Game Game::cardInspection(float distance){
 auto g=stanceRoomBare(int(SquadStance::Hostile));g.m_god=true;g.m_player.pos={7.5f,6.5f-distance};g.m_player.angle=kPi*.5f;
 for(int i=0;i<150&&g.guardState(0)!=GuardState::Alert;++i)g.update({},1.f/60);g.m_player.pos={7.5f,6.5f-distance};g.m_player.angle=kPi*.5f;g.m_player.pitch=8;g.update({},1.f/60);return g;
}
bool Game::testStance(){
 // The player stands 3.5 m south of the first guard, facing him.
 auto stanceRoom=[](int stance,bool patrol=false){auto g=stanceRoomBare(stance,patrol);g.m_player.pos={6.5f,3.f};g.m_player.angle=kPi*.5f;return g;};
 std::ofstream report("stance-test.txt");bool passed=true;int checks=0,good=0;
 auto check=[&](bool ok,const char* label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;good+=ok;};
 check(factionsHostile(AiFaction::Wary,AiFaction::Creature)&&factionsHostile(AiFaction::Creature,AiFaction::Wary),"A squad on a truce still fights the creatures, and they it");
 check(!factionsHostile(AiFaction::Wary,AiFaction::Player)&&!factionsHostile(AiFaction::Wary,AiFaction::Raider)&&!factionsHostile(AiFaction::Facility,AiFaction::Wary),"A truce makes it nobody else's enemy");
 {auto g=stanceRoom(0);const auto&t=g.m_friendlyActors[0].track();
  check(g.npcFaction(t)==AiFaction::Raider,"Declared stance keeps the authored faction");
  g.setState(stateId("sq_stance"),int(SquadStance::Truce));check(g.npcFaction(t)==AiFaction::Wary,"Truce fights as Wary");
  g.setState(stateId("sq_stance"),int(SquadStance::Ally));check(g.npcFaction(t)==AiFaction::Facility&&factionsHostile(g.npcFaction(t),AiFaction::Raider),"An ally fights on the player's side, against hostile squads");
  g.setState(stateId("sq_stance"),int(SquadStance::Hostile));check(g.npcFaction(t)==AiFaction::Raider,"Hostile fights the player");}
 // In play: a truce squad watches the player stand in front of it; the same squad hostile does not.
 {auto g=stanceRoom(int(SquadStance::Truce));bool alert=false;for(int i=0;i<240;++i){g.update({},1.f/60);alert|=g.guardState(0)==GuardState::Alert||g.guardState(1)==GuardState::Alert;}
  check(!alert&&g.player().health==100,"A squad on a truce never raises the alarm on the player in plain view (4 s)");}
 {auto g=stanceRoom(int(SquadStance::Hostile));bool alert=false;for(int i=0;i<240&&!alert;++i){g.update({},1.f/60);alert=g.guardState(0)==GuardState::Alert||g.player().health<100;}
  check(alert,"The same squad hostile takes the player on within 4 s");}
 // A shot ends a truce for the whole squad; an ally forgives one hit, not two.
 {auto g=stanceRoom(int(SquadStance::Truce));g.damageFriendly(0,5,g.player().pos,{ShotKind::Player,0,0});
  check(g.state(stateId("sq_stance"))==int(SquadStance::Hostile)&&g.npcFaction(g.m_friendlyActors[1].track())==AiFaction::Raider,"Shooting one man on a truce turns his whole squad hostile");
  bool alert=false;for(int i=0;i<180&&!alert;++i){g.update({},1.f/60);alert=g.guardState(1)==GuardState::Alert||g.guardState(0)==GuardState::Alert;}
  check(alert,"Betrayed, the squad goes to Alert within 3 s");}
 {auto g=stanceRoom(int(SquadStance::Ally));g.damageFriendly(1,5,g.player().pos,{ShotKind::Player,0,0});
  check(g.state(stateId("sq_stance"))==int(SquadStance::Ally)&&g.state(g.friendlyKey(0,"stance_strike_ms"))>0,"An ally forgives the first stray hit and warns the squad");
  g.damageFriendly(1,5,g.player().pos,{ShotKind::Player,0,0});check(g.state(stateId("sq_stance"))==int(SquadStance::Hostile),"The second hit within the window turns the allies");
  auto h=stanceRoom(int(SquadStance::Ally));h.damageFriendly(1,5,h.player().pos,{ShotKind::Player,0,0});h.updateStances(21.f);h.damageFriendly(1,5,h.player().pos,{ShotKind::Player,0,0});
  check(h.state(stateId("sq_stance"))==int(SquadStance::Ally),"Hits far apart are two accidents, not a betrayal");
  auto c=stanceRoom(int(SquadStance::Truce));c.damageFriendly(0,5,c.player().pos,{ShotKind::Npc,0,1});check(c.state(stateId("sq_stance"))==int(SquadStance::Truce),"Only the player's shots break a truce");}
 // A gun held on a man: the warning, then the truce is gone; fists, or looking away, are no threat.
 {auto g=stanceRoom(int(SquadStance::Truce));g.equipWeapon(WeaponId::Shotgun);g.m_player.angle=kPi*.5f;
  for(int i=0;i<13;++i)g.updateStances(.1f);
  check(g.state(g.friendlyKey(0,"stance_warned"))==1&&g.state(stateId("sq_stance"))==int(SquadStance::Truce),"A gun held on him 1.3 s draws the warning, not yet a fight");
  for(int i=0;i<33;++i)g.updateStances(.1f);check(g.state(stateId("sq_stance"))==int(SquadStance::Hostile),"Held on him 4.6 s, the truce breaks");
  auto f=stanceRoom(int(SquadStance::Truce));f.equipWeapon(WeaponId::Fists);for(int i=0;i<60;++i)f.updateStances(.1f);check(f.state(stateId("sq_stance"))==int(SquadStance::Truce),"Bare hands are no threat");
  auto a=stanceRoom(int(SquadStance::Truce));a.equipWeapon(WeaponId::Shotgun);a.m_player.angle=-kPi*.5f;for(int i=0;i<60;++i)a.updateStances(.1f);check(a.state(stateId("sq_stance"))==int(SquadStance::Truce),"A gun pointed away is no threat");
  auto b=stanceRoom(int(SquadStance::Truce));b.equipWeapon(WeaponId::Shotgun);for(int i=0;i<20;++i)b.updateStances(.1f);b.m_player.angle=-kPi*.5f;for(int i=0;i<20;++i)b.updateStances(.1f);b.m_player.angle=kPi*.5f;for(int i=0;i<20;++i)b.updateStances(.1f);
  check(b.state(stateId("sq_stance"))==int(SquadStance::Truce),"Lowering the gun lets the tension fall away");}
 // Stance is a state: it saves with the game.
 {auto g=stanceRoom(int(SquadStance::Truce));g.setSquadStance(0,SquadStance::Ally);Game loaded(g.m_customCampaign);bool decoded=loaded.decodeSave(g.encodeSave());
  check(decoded&&loaded.npcStance(loaded.m_friendlyActors[0].track())==SquadStance::Ally,"A squad's stance survives a save");}
 // Moods for the voices: steady, shaken by a man down, broken with half the squad gone.
 {auto g=stanceRoom(int(SquadStance::Hostile));check(g.squadMood(0)==1,"A whole squad is steady");
  g.damageFriendly(1,70,{6.5f,9.f},{});check(g.squadMood(0)==3,"Half the squad down: broken");
  auto h=stanceRoom(int(SquadStance::Hostile));h.damageFriendly(0,40,{6.5f,9.f},{});check(h.squadMood(0)==2,"Badly hurt himself: shaken");}
 report<<good<<"/"<<checks<<" checks passed\n";return passed;
}

bool Game::testDialogue(){
 // The player stands 3.5 m south of the first guard, facing him.
 auto stanceRoom=[](int stance,bool patrol=false){auto g=stanceRoomBare(stance,patrol);g.m_player.pos={6.5f,3.f};g.m_player.angle=kPi*.5f;return g;};
 std::ofstream report("dialogue-test.txt");bool passed=true;int checks=0,good=0;
 auto check=[&](bool ok,const char* label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;good+=ok;};
 InputState use{};use.use=true;InputState none{};
 auto press=[&](Game&g,InputState key){g.update(key,1.f/60);g.update(none,1.f/60);};
 auto choose=[&](Game&g,int choice){run(g,.5f);InputState key{};key.choice=choice;press(g,key);};
 {auto g=stanceRoom(int(SquadStance::Truce));g.m_player.pos={6.5f,5.f};run(g,.2f);
  check(g.nearbyTalker()==0,"The man with something to say is the one in front of the player");
  press(g,use);check(g.talking()&&g.dialogueView()&&g.dialogueView()->line=="Easy. Nobody shoots unless you make us.","E opens his conversation on its first line");
  check(g.dialogueView()->replies.size()==3,"A reply that needs a state the player lacks is not offered");
  InputState key{};key.choice=1;press(g,key);check(g.dialogueNode()&&g.dialogueNode()->id==stateId("hello"),"The keys that opened it cannot answer in the same breath");
  choose(g,1);check(g.dialogueNode()&&g.dialogueNode()->id==stateId("who"),"A reply leads to its next line");
  choose(g,1);check(g.dialogueNode()&&g.dialogueNode()->id==stateId("hello"),"And back again");
  g.setState(stateId("sq_pass"),1);check(g.dialogueView()->replies.size()==4&&g.dialogueView()->replies[1]=="I have a pass.","The reply appears once its state holds");
  run(g,.5f);InputState down{};down.menuDown=true;press(g,down);press(g,down);check(g.dialogueView()->selected==2,"The arrows move the selection");
  press(g,use);check(!g.talking()&&g.state(stateId("sq_deal"))==1&&g.state(stateId("sq_stance"))==int(SquadStance::Ally),"Picking a reply sets its states and the squad's stance, and ends the talk");
  run(g,.5f);press(g,use);check(g.talking(),"He talks again");run(g,.5f);InputState escape{};escape.escape=true;press(g,escape);check(!g.talking()&&!g.paused(),"Escape leaves the conversation, not into the menu");}
 {auto g=stanceRoom(int(SquadStance::Truce),true);g.m_player.pos={6.5f,5.f};run(g,.1f);press(g,use);auto at=g.m_friendlyActors[0].pose.position;
  bool held=g.talking();run(g,3.f);held&=length(g.m_friendlyActors[0].pose.position-at)<.05f;
  check(held&&g.talking(),"A guard on his route stops while he is spoken to");
  auto turn=g.m_player.pos-g.m_friendlyActors[0].pose.position;check(std::fabs(std::remainder(std::atan2(turn.x,turn.y)-g.m_friendlyActors[0].pose.yaw,2*kPi))<.3f,"And turns to face the player");
  g.closeDialogue();run(g,4.f);check(length(g.m_friendlyActors[0].pose.position-at)>.3f,"When it is over he walks on");}
 {auto g=stanceRoom(int(SquadStance::Hostile));g.m_player.pos={6.5f,5.f};check(g.nearbyTalker()<0,"A hostile man has nothing to say");}
 {auto g=stanceRoom(int(SquadStance::Truce));g.m_player.pos={6.5f,5.f};run(g,.1f);press(g,use);g.m_player.pos={6.5f,1.f};run(g,.1f);check(!g.talking(),"Walking off ends the conversation");}
 {auto g=stanceRoom(int(SquadStance::Truce));g.m_player.pos={6.5f,5.f};run(g,.1f);press(g,use);g.damageFriendly(0,5,g.player().pos,{ShotKind::Player,0,0});
  check(!g.talking()&&g.state(stateId("sq_stance"))==int(SquadStance::Hostile),"Shooting the man mid-sentence ends the talk and the truce");}
 // The campaign records: stance, talk, lines and replies, and a broken tree refused.
 {auto record=[](std::initializer_list<std::string> fields){std::string line;for(const auto&field:fields){if(!line.empty())line+='|';line+=field;}return line+"\n";};
  std::string base="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Dialogue fixture|0\nMAP|0|Room|0|0|6.5|3|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int row=0;row<24;++row)base+="ROW|0|0|"+std::to_string(row)+"|"+(row==0||row==23?std::string(24,'#'):"#"+std::string(22,'.')+"#")+"\n";
  base+=record({"TIMED_SEQUENCE","0","d_clock","d_done","0","0","24","24","-10","10","60000","60000","0","0","0","1","1","1"});
  base+=record({"ACTOR","0","d_clock","0","1.7","0","0","0"})+record({"ACTOR_KEY","0","0","0","6.5","6.5","0","3.1415927","0","0"})+record({"ACTOR_KEY","0","0","60000","6.5","6.5","0","3.1415927","0","0"});
  base+=record({"ACTOR_AI","0","0","1","","1.4","2","6"})+record({"ACTOR_FACTION","0","0","2",""})+record({"ACTOR_STANCE","0","0","d_stance"})+record({"ACTOR_TALK","0","0","d_hello"});
  base+=record({"DIALOGUE","0","d_hello","SENTRY","Halt. Who goes there?",""});
  auto parse=[&](const std::string&replies)->std::shared_ptr<const CustomCampaign>{try{return loadCustomCampaignText(base+replies+"--- CUSTOM_CAMPAIGN_DATA_END ---\n","dialogue-fixture.txt");}catch(const std::exception&e){report<<"parse: "<<e.what()<<'\n';return nullptr;}};
  auto fixture=parse(record({"DIALOGUE_REPLY","0","d_hello","A friend.","","d_badge","1","","d_friend=1;d_seen=2","3"})+record({"DIALOGUE_REPLY","0","d_hello","Nobody.","","","1","d_friend","",""}));
  bool ok=fixture&&fixture->maps.size()==1&&fixture->maps[0]->dialogue.size()==1&&fixture->maps[0]->dialogue[0].replies.size()==2;
  if(ok){const auto&reply=fixture->maps[0]->dialogue[0].replies[0];const auto&actor=fixture->maps[0]->actorTracks[0];
   ok=reply.require==stateId("d_badge")&&reply.sets.size()==2&&reply.sets[1].second==2&&reply.stance==3&&fixture->maps[0]->dialogue[0].replies[1].hide==stateId("d_friend")&&actor.ai.stance==stateId("d_stance")&&actor.talk==stateId("d_hello");}
  check(ok,"ACTOR_STANCE, ACTOR_TALK, DIALOGUE and DIALOGUE_REPLY parse into the map");
  bool loads=false;if(fixture)try{Game g(fixture);loads=true;}catch(const std::exception&e){report<<"load: "<<e.what()<<'\n';}check(loads,"A sound conversation loads");
  auto broken=parse(record({"DIALOGUE_REPLY","0","d_hello","Run.","d_nowhere","","1","","",""}));bool refused=!broken;
  if(broken)try{Game g(broken);}catch(const std::exception&){refused=true;}check(refused,"A reply leading to a line that does not exist is refused at load");
  check(!parse(record({"DIALOGUE_REPLY","0","d_missing","Hm.","","","1","","",""})),"A reply to a line that does not exist is refused when parsed");}
 report<<good<<"/"<<checks<<" checks passed\n";return passed;
}
}

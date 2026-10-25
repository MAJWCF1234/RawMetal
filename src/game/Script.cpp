#include "Game.h"
#include "ai/StateLookupHints.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <utility>
#include <chrono>

namespace retro {
namespace {
template<class T> int findValue(const std::vector<T>& values,StateId id){
 for(int i=0;i<int(values.size());++i)if(values[size_t(i)].id==id)return i;return -1;
}
bool containsEvent(const std::vector<StateId>& values,StateId id){
 return std::find(values.begin(),values.end(),id)!=values.end();
}
}

int Game::state(StateId id)const{auto index=ai::StateLookupHints::lowerBound(m_states,id,m_aiQueryCaching);return index<m_states.size()&&m_states[index].id==id?m_states[index].value:0;}
void Game::setState(StateId id,int value){
 if(!id)return;auto it=m_states.begin()+ai::StateLookupHints::lowerBound(m_states,id,m_aiQueryCaching);
 if(it==m_states.end()||it->id!=id){if(value)m_states.insert(it,{id,value});}else if(value)it->value=value;else m_states.erase(it);
}
ObjectiveStatus Game::objective(StateId id)const{int index=findValue(m_objectives,id);return index<0?ObjectiveStatus::Hidden:static_cast<ObjectiveStatus>(m_objectives[size_t(index)].value);}
std::string_view Game::activeObjectiveText()const{
 if(m_customCampaign)for(const auto&map:m_customCampaign->maps)for(const auto&label:map->objectiveTexts)if(objective(label.id)==ObjectiveStatus::Active)return label.text;
 return {};
}
void Game::setObjective(StateId id,ObjectiveStatus status){if(!id)return;int value=int(status),index=findValue(m_objectives,id);if(index<0){if(status!=ObjectiveStatus::Hidden)m_objectives.push_back({id,value});}else if(status==ObjectiveStatus::Hidden)m_objectives.erase(m_objectives.begin()+index);else m_objectives[size_t(index)].value=value;}
int Game::questItemCount(StateId id)const{int index=findValue(m_questItems,id);return index<0?0:m_questItems[size_t(index)].count;}
bool Game::hasQuestItem(StateId id,int count)const{return count>0&&questItemCount(id)>=count;}
void Game::giveQuestItem(StateId id,int count){if(!id||count<=0)return;int index=findValue(m_questItems,id);if(index<0)m_questItems.push_back({id,std::min(99,count)});else m_questItems[size_t(index)].count=std::min(99,m_questItems[size_t(index)].count+count);}
bool Game::takeQuestItem(StateId id,int count){if(!id||count<=0)return false;int index=findValue(m_questItems,id);if(index<0||m_questItems[size_t(index)].count<count)return false;auto&item=m_questItems[size_t(index)];item.count-=count;if(item.count==0){m_questItems.erase(m_questItems.begin()+index);if(id==Flashlight)setState(stateId("flashlight_on"),0);}return true;}
const char* Game::questItemName(StateId id){
 if(id==ReactorAuthDisk)return "REACTOR AUTH DISK";
 if(id==Flashlight)return "FLASHLIGHT";
 if(id==stateId("keycard"))return "KEYCARD";
 if(id==stateId("fuse"))return "FUSE";
 if(id==stateId("tool"))return "SERVICE TOOL";
 if(id==stateId("strange_sample"))return "STRANGE SAMPLE";
 if(id==stateId("valve_handle"))return "VALVE HANDLE";
 if(id==stateId("document"))return "DOCUMENT";
 return "QUEST ITEM";
}

void Game::seedScripts(){
 m_scriptEvents.clear();
 for(int chunk=0;chunk<chunkCount();++chunk)for(auto event:m_chunks[chunk].world.scriptEvents()){
  event.level=chunk;m_scriptEvents.push_back(std::move(event));
 }
}
void Game::spawnScriptEnemy(const ScriptAction& action){
 spawnCreature({action.enemyKind,action.position,action.z},action.amount,true);m_enemies.back().scriptSpawned=true;
}
void Game::spawnCreature(const CreatureSpawn& spawn,float awareness,bool announce){
 Enemy e{};e.kind=spawn.kind;e.pos=spawn.position;e.home=e.pos;e.lastKnown=e.pos;
 e.hp=e.maxHp=e.kind==Enemy::Kind::Wasp?85.f:e.kind==Enemy::Kind::Brute?280.f:e.kind==Enemy::Kind::Warden?320.f:e.kind==Enemy::Kind::Mutant?180.f:110.f;
 e.z=spawn.z==-999?m_world.floorHeight(e.pos.x,e.pos.y):spawn.z;e.lastKnownZ=e.z;e.heading=kPi;
 e.awareness=std::max(0.f,awareness);e.state=e.awareness>0?Enemy::State::Investigate:Enemy::State::Idle;
 e.voiceTimer=announce?.25f:.8f+float(m_enemies.size())*.9f;m_enemies.push_back(e);
 if(announce){if(e.kind==Enemy::Kind::Brute)sound(Sound::Land,.8f,.7f);else enemySound(m_enemies.back(),0,.55f,.9f);}
}
void Game::executeScriptAction(const ScriptAction& action){
 switch(action.type){
  case ScriptAction::Type::SetState:setState(action.id,action.value);break;
  case ScriptAction::Type::SetObjective:setObjective(action.id,static_cast<ObjectiveStatus>(std::clamp(action.value,0,3)));break;
  case ScriptAction::Type::GiveItem:giveQuestItem(action.id,std::max(1,action.value));if(action.id==Flashlight){m_pickupNotice="FLASHLIGHT ACQUIRED / F TO TOGGLE";m_pickupNoticeTime=3.f;sound(Sound::Pickup,.6f);}break;
  case ScriptAction::Type::TakeItem:takeQuestItem(action.id,std::max(1,action.value));break;
  case ScriptAction::Type::OpenDoor:if(action.index>=0&&action.index<int(m_world.doors().size())){auto d=m_world.doors()[size_t(action.index)];m_world.setDoor(action.index,d.open,true);}break;
  case ScriptAction::Type::CloseDoor:if(action.index>=0&&action.index<int(m_world.doors().size())){auto d=m_world.doors()[size_t(action.index)];m_world.setDoor(action.index,d.open,false);}break;
  case ScriptAction::Type::ReleaseControl:m_world.releaseControl();break;
  case ScriptAction::Type::PlaySound:sound(action.sound,action.amount>0?action.amount:1.f);break;
  case ScriptAction::Type::SpawnEnemy:spawnScriptEnemy(action);break;
  case ScriptAction::Type::Shake:m_verticalSpringVelocity-=action.amount;m_damageFlash=std::max(m_damageFlash,std::min(1.f,action.amount*.2f));break;
  case ScriptAction::Type::Checkpoint:saveCheckpoint();break;
  case ScriptAction::Type::CompleteCampaign:m_won=true;break;
  // Deferred: the script pass is still iterating this world's events.
  case ScriptAction::Type::EnterMainCampaign:m_pendingMainCampaign=true;break;
 }
}
void Game::updateReceivingBreach(){
 if(!m_world.campaign()||m_level!=14)return;
 const auto squad=stateId("receiving_cleanup");int picked=0;std::vector<size_t> living;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&t=npc.track();
  if(npc.homeLevel!=14||t.ai.squad!=squad)continue;
  if(state(t.deadState)){picked+=state(friendlyKey(i,"killed_by_player"))!=0;}
  else if(!npcIncapacitated(i))living.push_back(i);
 }
 if(!state("receiving_breach")&&picked>=2&&!living.empty()){
  // Prefer the technical specialist, then the living member nearest the exit.
  size_t survivor=living.front();float best=1e9f;
  for(auto i:living){const auto&npc=m_friendlyActors[i];float score=length(npc.pose.position-Vec2{12,23});if(npc.track().ai.role==GuardSquadRole::Engineer)score-=100;
   if(score<best){best=score;survivor=i;}}
  setState(stateId("receiving_breach"),1);setState(stateId("receiving_survivor"),int(m_friendlyActors[survivor].homeActor)+1);
  for(auto pair:{std::pair{"evac_level",16},std::pair{"evac_x",12000},std::pair{"evac_y",18000},std::pair{"evac_z",-25000}})setState(friendlyKey(survivor,pair.first),pair.second);
  clearGuardSchedule(survivor);setState(friendlyKey(survivor,"repath"),0);
  // These creatures use ordinary perception, navigation and melee damage.
  // No surviving guard is killed by a script action or made invulnerable.
  const Vec2 positions[]={{8.3f,14.5f},{20.5f,12.5f},{11,20.8f},{7.6f,11.5f},{15.8f,20.5f},{20.5f,10.5f}};
  for(int i=0;i<6;++i){ScriptAction spawn;spawn.type=ScriptAction::Type::SpawnEnemy;spawn.enemyKind=i<3?CreatureKind::Brute:CreatureKind::Huntsman;spawn.position=positions[i];spawn.z=-25;spawn.amount=8;executeScriptAction(spawn);}
  m_pickupNotice="CONTAINMENT BREACH / THE DETAIL IS BEING OVERRUN";m_pickupNoticeTime=4;sound(Sound::LiftCrash,.65f,.8f);
 }
 if(state("receiving_breach")&&!state("receiving_retreat_sealed")){
  int track=state("receiving_survivor")-1;
  for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];if(npc.homeLevel!=14||int(npc.homeActor)!=track||state(npc.track().deadState)||npc.level!=15||npc.pose.position.y<.8f)continue;
   for(size_t door=0;door<m_world.doors().size();++door)if(std::fabs(m_world.doors()[door].y-23.5f)<.01f)m_world.setDoor(int(door),0,false);
   setState(stateId("receiving_retreat_sealed"),1);m_pickupNotice="ONE CONTACT RETREATING / PRESSURE DOOR CLOSED";m_pickupNoticeTime=3;
  }
 }
}
void Game::updateScripts(float){
 updateReceivingBreach();
 for(const auto&event:m_scriptEvents){
  if(event.level!=m_level||!event.id||(event.once&&containsEvent(m_firedEvents,event.id)))continue;
  if(event.requireState&&state(event.requireState)!=event.requireValue)continue;
  if(event.requireEnemiesClear&&enemiesRemaining()>0)continue;
  if(m_player.pos.x<event.x1||m_player.pos.x>event.x2||m_player.pos.y<event.y1||m_player.pos.y>event.y2||m_player.z<event.bottom||m_player.z>event.top)continue;
  if(event.once)m_firedEvents.push_back(event.id);for(const auto&action:event.actions)executeScriptAction(action);
 }
}
bool Game::hazardActive(const Hazard& h)const{
 bool enabled=!h.enabledFlag||state(h.enabledFlag)==h.enabledValue;if(h.invertFlag)enabled=!enabled;
 return enabled&&(h.period<=0||std::fmod(m_elapsed+h.phase,h.period)<h.onTime);
}
float Game::compactorHeight(const Compactor& c)const{
 if(state(c.stopState))return c.raised;
 float t=std::fmod(m_elapsed,c.period)/c.period;
 float stroke=t<.35f?0:t<.5f?(t-.35f)/.15f:t<.63f?1:t<.8f?1-(t-.63f)/.17f:0;
 return c.raised+(c.bed+.12f-c.raised)*stroke;
}
void Game::updateHazards(float dt){
 m_hazardSoundTimer=std::max(0.f,m_hazardSoundTimer-dt);float damage=0;Hazard::Kind loudest=Hazard::Kind::Toxic;
 for(const auto&hazard:m_world.hazards()){
  if(!hazardActive(hazard))continue;
  if(hazard.period>0&&std::fmod(m_elapsed+hazard.phase,hazard.period)<dt)
   m_sounds.push_back({Sound::Machine,{(hazard.x1+hazard.x2)*.5f,(hazard.y1+hazard.y2)*.5f},.25f,1.3f,true,17,(hazard.bottom+hazard.top)*.5f});
  if(m_player.pos.x<hazard.x1||m_player.pos.x>hazard.x2||m_player.pos.y<hazard.y1||m_player.pos.y>hazard.y2)continue;
  if(m_player.z+m_player.hullHeight()<hazard.bottom||m_player.z>hazard.top)continue;
  damage+=std::max(0.f,hazard.damagePerSecond)*dt;loudest=hazard.kind;
 }
 for(const auto& c:m_world.compactors()){
  float height=compactorHeight(c);
  auto inside=[&](Vec2 p){return p.x>c.x1+.1f&&p.x<c.x2-.1f&&p.y>c.y1&&p.y<c.y2;};
  bool running=!state(c.stopState);
  if(running&&inside(m_player.pos)&&m_player.z<c.bed+.25f)tryMove({0,dt*.55f});
  if(inside(m_player.pos)&&m_player.z<height&&m_player.z+m_player.hullHeight()>height){damage+=100*dt;loudest=Hazard::Kind::Crusher;}
  for(auto& e:m_enemies)if(e.alive&&inside(e.pos)){
   if(running&&e.z<c.bed+.25f)e.pos.y+=dt*.55f;
   if(e.bodyTop()>height&&e.bodyBottom()<height){e.hp-=300*dt;e.painFlash=1;if(e.hp<=0){e.alive=false;e.deathTime=0;e.windup=0;++m_kills;enemySound(e,2);}}
  }
  for(int i=int(m_clutter.size())-1;i>=0;--i){auto& item=m_clutter[i];if(!inside(item.pos)||i==m_heldClutter)continue;
   if(running&&item.z<c.bed+.3f){item.sleeping=false;item.velocity.y+=dt*2.f;}
   if(height<c.bed+.3f&&item.z<c.bed+.35f){m_sounds.push_back({Sound::JunkMetal,item.pos,.4f,.7f,true,17,item.z+item.height()*.5f});m_clutter.erase(m_clutter.begin()+i);if(m_heldClutter>i)--m_heldClutter;}
  }
 }
 if(damage<=0||m_god)return;m_player.health=std::max(0.f,m_player.health-damage);m_damageFlash=std::max(m_damageFlash,.35f);
 if(m_hazardSoundTimer<=0){sound(loudest==Hazard::Kind::Crusher||loudest==Hazard::Kind::FallingDebris?Sound::JunkMetal:Sound::Hurt,.55f,loudest==Hazard::Kind::Anomaly?.72f:1.f);m_hazardSoundTimer=.35f;}
}

bool Game::testFlashlight(){
 Game game;InputState key{};key.flashlight=true;game.update(key,.01f);if(game.hasFlashlight()||game.flashlightOn())return false;
 game.update({},.01f);game.giveQuestItem(Flashlight);game.update(key,.01f);if(!game.flashlightOn())return false;
 game.update(key,.01f);if(!game.flashlightOn())return false;game.update({},.01f);game.update(key,.01f);if(game.flashlightOn())return false;
 game.setState(stateId("flashlight_on"),1);if(!game.takeQuestItem(Flashlight)||game.flashlightOn())return false;
 game.giveQuestItem(Flashlight);game.setState(stateId("flashlight_on"),1);Game restored;
 if(!restored.decodeSave(game.encodeSave())||!restored.hasFlashlight()||!restored.flashlightOn())return false;
 restored.update({},.01f);InputState menu{};menu.escape=true;restored.update(menu,.01f);restored.update(key,.01f);
 if(!restored.flashlightOn())return false;
 restored.update(menu,.01f);restored.update(key,.01f);
 if(restored.flashlightOn())return false;
 restored.update({},.01f);menu={};menu.inventory=true;restored.update(menu,.01f);restored.update(key,.01f);
 return !restored.flashlightOn();
}

// Switchable lamps, true darkness and the torch jam, end to end from the
// custom-campaign records through validation to the per-frame values the
// renderers read. The rendered evidence is --darkness-test (main.cpp).
bool Game::testDarkness(){
 std::ofstream report("darkness-test.txt");int failed=0;
 auto check=[&](bool ok,const char*name){failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<name<<'\n';report.flush();return ok;};
 auto record=[](std::initializer_list<std::string> fields){std::string line;for(const auto&field:fields){if(!line.empty())line+='|';line+=field;}return line+"\n";};
 const std::string start="--- CUSTOM_CAMPAIGN_DATA_START ---\n",end="--- CUSTOM_CAMPAIGN_DATA_END ---\n";
 std::string base="CAMPAIGN|Darkness fixture|0\nMAP|0|Room|0|0|6.5|12|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row)base+="ROW|0|0|"+std::to_string(row)+"|"+(row==0||row==23?std::string(24,'#'):"#"+std::string(22,'.')+"#")+"\n";
 // A gated lamp, a timed one, a failing tube and an inverted wall lamp; the
 // west half is dark while test_dark is set (jamming), the east half always (no jam).
 const std::string lamps=record({"LIGHT","0","6","8","3.2","0","0","1.4","10","3400","0","test_lamp","1","0","","","",""})+
  record({"LIGHT","0","18","8","3.2","0","0","1","8","5600","0.3","","","","2","0.5","0.25",""})+
  record({"LIGHT","0","18","16","3.2","0","0","1","8","5600","0.3","","","","","","","0.6"})+
  record({"LIGHT","0","1","16","2","1","0","1.2","6","","","test_lamp","1","1"});
 const std::string zones=record({"DARK","0","1","1","12","23","-10","10","test_dark","1","0","1"})+record({"DARK","0","12","1","23","23","","","","","","0"});
 auto parse=[&](const std::string&records){return loadCustomCampaignText(start+base+records+end,"darkness-fixture.txt");};
 auto rejects=[&](const std::string&records){try{parse(records);return false;}catch(const std::exception&){return true;}};
 std::shared_ptr<const CustomCampaign> campaign;
 try{campaign=parse(lamps+zones);}catch(const std::exception&e){report<<e.what()<<'\n';}
 if(!check(campaign!=nullptr,"LIGHT switching fields and DARK records parse"))return false;
 const auto&map=*campaign->maps[0];
 check(map.lights.size()==4&&map.lights[0].enabledState==stateId("test_lamp")&&map.lights[0].enabledValue==1&&map.lights[1].period==2&&map.lights[1].onTime==.5f&&map.lights[1].phase==.25f&&map.lights[2].flicker==.6f&&map.lights[3].invert&&map.lights[3].mount==WorldLightMount::Wall&&!map.lights[2].enabledState,"Parsed lamps keep state, value, invert, timed window and flicker");
 check(map.darkZones.size()==2&&map.darkZones[0].state==stateId("test_dark")&&map.darkZones[0].jamFlashlight&&map.darkZones[0].x2==12&&!map.darkZones[1].state&&!map.darkZones[1].jamFlashlight&&map.darkZones[1].bottom==-1000,"Parsed dark zones keep their box, gate and jam flag");
 check(rejects(record({"LIGHT","0","6","8","3.2","0","0","1","8","","","","","","","","","1.5"}))&&rejects(record({"LIGHT","0","6","8","3.2","0","0","1","8","","","","","","2","3"}))&&rejects(record({"LIGHT","0","6","8","3.2","0","0","1","8","","","","","","-1","0"}))&&rejects(record({"DARK","0","5","1","4","23"}))&&rejects(record({"DARK","0","1","1","4"}))&&rejects(record({"DARK","0","1","1","4","4","2","1"})),"Parser rejects flicker over 1, on-time past its period, negative periods and empty or inverted dark boxes");
 auto invalid=[&](auto edit){auto copy=std::make_shared<AuthoredMapData>(map);edit(*copy);try{World world(0,copy);return false;}catch(const std::exception&){return true;}};
 check(invalid([](AuthoredMapData&m){m.lights[0].flicker=std::numeric_limits<float>::quiet_NaN();})&&invalid([](AuthoredMapData&m){m.lights[0].period=-1;})&&invalid([](AuthoredMapData&m){m.lights[1].onTime=3;})&&invalid([](AuthoredMapData&m){m.lights[2].flicker=1.2f;})&&invalid([](AuthoredMapData&m){m.darkZones[0].x2=m.darkZones[0].x1;})&&invalid([](AuthoredMapData&m){m.darkZones[0].top=std::numeric_limits<float>::infinity();})&&invalid([](AuthoredMapData&m){m.darkZones[1].x1=30;m.darkZones[1].x2=40;}),"Map loading rejects nonfinite or out-of-range switching and degenerate or off-map dark zones");

 Game game(campaign);const auto lights=game.m_world.lights();
 check(lights.size()==4&&lights[0].switchable()&&lights[1].switchable()&&lights[2].switchable()&&lights[3].switchable(),"Every gated, timed, flickering or inverted lamp reports itself switchable");
 check(game.lampPower(lights[0])==0&&game.lampPower(lights[3])==1,"A gated lamp starts off; its inverted twin starts lit");
 game.setState(stateId("test_lamp"),1);
 check(game.lampPower(lights[0])==1&&game.lampPower(lights[3])==0&&game.lampSwitchedOn(lights[0]),"Setting the state swaps them");
 auto at=[&](float t,const WorldLight&lamp){game.m_elapsed=t;return game.lampPower(lamp);};
 check(at(0,lights[1])==1&&at(.2f,lights[1])==1&&at(.3f,lights[1])==0&&at(1.7f,lights[1])==0&&at(1.8f,lights[1])==1&&at(5.8f,lights[1])==1,"A timed lamp lights only inside its on-window (phase shifts it)");
 int off=0,dim=0,on=0,blinks=0,apart=0;float sum=0;bool repeatable=true;std::vector<float> trace;WorldLight twin=lights[2];twin.position.x+=3;
 for(int i=0;i<6000;++i){float p=at(float(i)/60.f,lights[2]);sum+=p;off+=p==0;on+=p==1;dim+=p>0&&p<1;if(!trace.empty())blinks+=(p==0)!=(trace.back()==0);trace.push_back(p);apart+=game.lampPower(twin)!=p;}
 for(int i=0;i<6000;i+=7)repeatable&=at(float(i)/60.f,lights[2])==trace[size_t(i)];
 report<<"flicker .6 over 100 s at 60 Hz: on "<<on<<" dim "<<dim<<" off "<<off<<" mean "<<sum/6000<<" off-transitions "<<blinks<<" frames apart from a moved twin "<<apart<<'\n';
 check(on>3000&&off>150&&dim>150&&blinks>60&&repeatable,"A failing tube is mostly on, with off-blinks and dim holds, identical on every replay");
 check(apart>600,"Two failing tubes never blink in step");

 game.m_elapsed=0;game.giveQuestItem(Flashlight);game.setState(stateId("flashlight_on"),1);game.m_player.pos={6.5f,12};game.update({},.01f);
 check(game.darkness()==0&&game.flashlightOn()&&!game.flashlightJammed(),"An inactive dark zone leaves the room lit and the torch on");
 game.setState(stateId("test_dark"),1);game.update({},.05f);
 check(game.flashlightJam()>0&&game.flashlightJam()<FlashlightJamTime&&game.pickupNotice()=="FLASHLIGHT / FAILING","Entering a jamming zone starts the torch sputter with a caption");
 game.update({},.05f);float early=game.darkness();for(int i=0;i<3;++i)game.update({},.05f);float later=game.darkness();
 int lit=0,out=0;for(int i=0;i<21;++i){game.update({},.05f);(game.flashlightOn()?lit:out)++;}
 report<<"darkness after .1 s "<<early<<", .25 s "<<later<<", then "<<game.darkness()<<"; torch lit "<<lit<<" / out "<<out<<" frames while failing\n";
 check(early>.2f&&early<.5f&&later>.7f&&later<1&&game.darkness()==1,"Entering an active zone ramps to true darkness over about .3 s");
 check(lit>0&&out>0,"The torch flickers while it fails");
 for(int i=0;i<6;++i)game.update({},.05f);
 check(!game.flashlightOn()&&game.flashlightJammed()&&game.pickupNotice()=="FLASHLIGHT / NO POWER","After 1.5 s inside the torch is dead");
 InputState key{};key.flashlight=true;game.update(key,.05f);game.update({},.05f);game.update(key,.05f);game.update({},.05f);
 check(game.state("flashlight_on")==1&&!game.flashlightOn()&&game.pickupNotice()=="FLASHLIGHT / NO POWER","Switching a dead torch back on reports no power");
 {Game restored(campaign);bool decoded=restored.decodeSave(game.encodeSave());
  check(decoded&&restored.darkness()==1&&restored.flashlightJammed()&&restored.lampPower(restored.m_world.lights()[0])==1,"A restored save is dark at once, its torch dead and its lamps as saved");}
 InputState fire{};fire.fire=true;game.update(fire,.01f);float flash=game.muzzleFlash();
 game.update({},.05f);game.update({},.05f);float held=game.muzzleFlash();game.update({},.05f);game.update({},.05f);float tail=game.muzzleFlash();
 for(int i=0;i<3;++i)game.update({},.05f);float gone=game.muzzleFlash();
 report<<"dark muzzle flash at .01 s "<<flash<<", .11 s "<<held<<", .21 s "<<tail<<", .36 s "<<gone<<'\n';
 check(flash>2&&held>0&&tail>0&&tail<.1f&&gone==0,"In darkness a shot flashes brighter and longer, then fades");
 game.setState(stateId("test_dark"),0);game.update({},.05f);
 check(game.flashlightOn()&&game.pickupNotice()=="FLASHLIGHT / ON","Switching the zone off restores the torch at once");
 for(int i=0;i<30;++i)game.update({},.05f);
 check(game.darkness()==0,"Darkness lifts again");
 game.update(fire,.01f);float litFlash=game.muzzleFlash();game.update({},.05f);float litAfter=game.muzzleFlash();
 game.m_shotAge=0;game.m_weaponKick=.1f;float reload=game.muzzleFlash();
 report<<"lit muzzle flash at .01 s "<<litFlash<<", .06 s "<<litAfter<<'\n';
 check(litFlash>.7f&&litFlash<=1&&litAfter==0&&reload==0,"In light the flash keeps its three frames; a finished reload never flashes");
 game.m_player.pos={18,12};game.update({},.05f);for(int i=0;i<8;++i)game.update({},.05f);
 check(game.darkness()==1&&game.flashlightOn(),"An always-dark zone without jam keeps the torch working");
 report<<(failed?"FAILED ":"ALL PASSED ")<<failed<<'\n';return failed==0;
}

bool Game::testSystems(){
 std::ofstream out("systems-test.txt");auto check=[&](bool ok,const char*name){out<<name<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 {Game dense;for(int i=0;i<4096;++i){int index=(i*1543)%4096;dense.setState(StateId(index+1),index+1);}
  dense.setState(4096,0);dense.setState(4000,-7);dense.setState(0,123);
  if(!check(dense.state(4096)==0&&dense.state(4000)==-7&&dense.state(3999)==3999&&dense.state(0)==0,"Large state store supports unordered inserts, updates and removals"))return false;
  using Clock=std::chrono::steady_clock;std::int64_t indexed=0,linear=0;std::uint32_t seed=17;auto start=Clock::now();
  for(int i=0;i<250000;++i){seed=seed*1664525u+1013904223u;indexed+=dense.state((seed>>16)%4096+1);}auto middle=Clock::now();seed=17;
  for(int i=0;i<250000;++i){seed=seed*1664525u+1013904223u;int index=findValue(dense.m_states,(seed>>16)%4096+1);if(index>=0)linear+=dense.m_states[size_t(index)].value;}auto finish=Clock::now();
  out<<"250000 lookups / "<<dense.m_states.size()<<" flags: indexed "<<std::chrono::duration<double,std::milli>(middle-start).count()<<" ms; linear reference "<<std::chrono::duration<double,std::milli>(finish-middle).count()<<" ms\n";
  if(!check(indexed==linear,"Indexed state lookups exactly match the linear reference"))return false;
  const auto expected=dense.m_states;std::reverse(dense.m_states.begin(),dense.m_states.end());Game restored;
  bool exact=restored.decodeSave(dense.encodeSave())&&restored.m_states.size()==expected.size();
  if(exact)for(size_t i=0;i<expected.size();++i)exact&=restored.m_states[i].id==expected[i].id&&restored.m_states[i].value==expected[i].value;
  if(!check(exact&&restored.state(4000)==-7&&restored.state(3999)==3999&&restored.state(4096)==0,"Large unsorted legacy state payload loads and restores indexed lookup"))return false;
 }
 Game game;auto power=stateId("power_restored"),objectiveId=stateId("reach_service_gallery"),fuse=stateId("fuse");
 game.setState(power,2);game.setObjective(objectiveId,ObjectiveStatus::Active);game.giveQuestItem(fuse,2);
 if(!check(game.state(power)==2&&game.objective(objectiveId)==ObjectiveStatus::Active&&game.questItemCount(fuse)==2,"Named state, objective and quest-item stores"))return false;
 if(!check(game.takeQuestItem(fuse)&&game.questItemCount(fuse)==1,"Quest items are counted and consumable"))return false;

 game.m_enemies.clear();game.m_scriptEvents.clear();game.m_firedEvents.clear();game.m_player.pos={3.5f,4.5f};game.m_player.z=0;
 ScriptEvent event;event.id=stateId("test_entry");event.level=0;event.x1=3;event.y1=4;event.x2=4;event.y2=5;
 ScriptAction flag;flag.type=ScriptAction::Type::SetState;flag.id=stateId("freight_gate_open");flag.value=1;event.actions.push_back(flag);
 ScriptAction spawn;spawn.type=ScriptAction::Type::SpawnEnemy;spawn.enemyKind=Enemy::Kind::Wasp;spawn.position={6.5f,4.5f};spawn.amount=4;event.actions.push_back(spawn);
 ScriptAction shake;shake.type=ScriptAction::Type::Shake;shake.amount=.8f;event.actions.push_back(shake);game.m_scriptEvents.push_back(event);
 game.updateScripts(.01f);size_t count=game.m_enemies.size();game.updateScripts(.01f);
 if(!check(game.state("freight_gate_open")==1&&count==1&&game.m_enemies.size()==1&&game.m_enemies[0].awareness>0,"One-shot volume event sets state, spawns an alerted monster and shakes view"))return false;

 Hazard hazard;hazard.kind=Hazard::Kind::Electricity;hazard.x1=3;hazard.y1=4;hazard.x2=4;hazard.y2=5;hazard.bottom=-.1f;hazard.top=1.2f;hazard.damagePerSecond=20;
 hazard.enabledFlag=stateId("hazard_live");game.m_world.m_hazards.push_back(hazard);float health=game.m_player.health;
 game.updateHazards(.5f);if(!check(game.m_player.health==health,"Flag-gated hazard stays off"))return false;
 game.setState(hazard.enabledFlag,1);game.updateHazards(.5f);
 if(!check(std::fabs(game.m_player.health-(health-10))<.01f,"Environmental hazard applies frame-rate-scaled damage"))return false;

 ScriptAction finish;finish.type=ScriptAction::Type::CompleteCampaign;game.executeScriptAction(finish);
 return check(game.won(),"Campaign completion is explicit, not tied to the highest compiled chunk");
}
}

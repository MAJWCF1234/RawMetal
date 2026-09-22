#include "Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

namespace retro {
namespace {
template<class T> int findValue(const std::vector<T>& values,StateId id){
 for(int i=0;i<int(values.size());++i)if(values[size_t(i)].id==id)return i;return -1;
}
bool containsEvent(const std::vector<StateId>& values,StateId id){
 return std::find(values.begin(),values.end(),id)!=values.end();
}
}

int Game::state(StateId id)const{int index=findValue(m_states,id);return index<0?0:m_states[size_t(index)].value;}
void Game::setState(StateId id,int value){if(!id)return;int index=findValue(m_states,id);if(index<0){if(value)m_states.push_back({id,value});}else if(value)m_states[size_t(index)].value=value;else m_states.erase(m_states.begin()+index);}
ObjectiveStatus Game::objective(StateId id)const{int index=findValue(m_objectives,id);return index<0?ObjectiveStatus::Hidden:static_cast<ObjectiveStatus>(m_objectives[size_t(index)].value);}
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
 for(int chunk=0;chunk<ChunkCount;++chunk)for(auto event:m_chunks[chunk].world.scriptEvents()){
  event.level=chunk;m_scriptEvents.push_back(std::move(event));
 }
}
void Game::spawnScriptEnemy(const ScriptAction& action){
 spawnCreature({action.enemyKind,action.position,action.z},action.amount,true);
}
void Game::spawnCreature(const CreatureSpawn& spawn,float awareness,bool announce){
 Enemy e{};e.kind=spawn.kind;e.pos=spawn.position;e.home=e.pos;e.lastKnown=e.pos;
 e.hp=e.maxHp=e.kind==Enemy::Kind::Wasp?85.f:e.kind==Enemy::Kind::Brute?280.f:e.kind==Enemy::Kind::Warden?320.f:110.f;
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
 }
}
void Game::updateScripts(float){
 for(const auto&event:m_scriptEvents){
  if(event.level!=m_level||!event.id||(event.once&&containsEvent(m_firedEvents,event.id)))continue;
  if(event.requireState&&state(event.requireState)!=event.requireValue)continue;
  if(event.requireEnemiesClear&&enemiesRemaining()>0)continue;
  if(m_player.pos.x<event.x1||m_player.pos.x>event.x2||m_player.pos.y<event.y1||m_player.pos.y>event.y2||m_player.z<event.bottom||m_player.z>event.top)continue;
  if(event.once)m_firedEvents.push_back(event.id);for(const auto&action:event.actions)executeScriptAction(action);
 }
}
void Game::updateHazards(float dt){
 m_hazardSoundTimer=std::max(0.f,m_hazardSoundTimer-dt);float damage=0;Hazard::Kind loudest=Hazard::Kind::Toxic;
 for(const auto&hazard:m_world.hazards()){
  bool enabled=!hazard.enabledFlag||state(hazard.enabledFlag)==hazard.enabledValue;if(hazard.invertFlag)enabled=!enabled;if(!enabled)continue;
  if(m_player.pos.x<hazard.x1||m_player.pos.x>hazard.x2||m_player.pos.y<hazard.y1||m_player.pos.y>hazard.y2)continue;
  if(m_player.z+m_player.hullHeight()<hazard.bottom||m_player.z>hazard.top)continue;
  damage+=std::max(0.f,hazard.damagePerSecond)*dt;loudest=hazard.kind;
 }
 if(damage<=0)return;m_player.health=std::max(0.f,m_player.health-damage);m_damageFlash=std::max(m_damageFlash,.35f);
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

bool Game::testSystems(){
 std::ofstream out("systems-test.txt");auto check=[&](bool ok,const char*name){out<<name<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
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

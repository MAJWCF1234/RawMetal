#include "../../../Game.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testGuardPolicy(){
 std::ofstream report("guard-policy-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](auto edit){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Guard policy validation";campaign->key=882743;
  auto map=std::make_shared<AuthoredMapData>();map->name="Threat and squad room";map->definition={{0,0},{10.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("policy_clock");timer.finishState=stateId("policy_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{30000,{6.5f,6.5f},0,kPi*.5f}};guard.scale=1.6f;guard.health=70;
  guard.damageState=stateId("policy_guard_damage");guard.deadState=stateId("policy_guard_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.armed=true;guard.ai.patrol={{{6.5f,6.5f},0,1000}};
  map->actorTracks.push_back(guard);edit(*map);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int frames,int rate=60){int shots=0;for(int i=0;i<frames;++i){g.update({},1.f/rate);for(const auto&sound:g.sounds())shots+=sound.sound==Sound::Shot;}return shots;};
 auto standing=[](const Game&g,size_t actor){const auto&npc=g.npcActors()[actor];return npc.pose.position+Vec2{std::sin(npc.pose.yaw),std::cos(npc.pose.yaw)}*2;};
 for(int rate:{60,120}){
  auto g=scene([](auto&map){map.structures.push_back({11.8f,0,12,24,0,4});});tick(g,rate,rate);
  check(g.guardState(0)==GuardState::Alert&&g.guardTarget(0).kind==ShotKind::Player,"Visible hostile is confirmed after recognition at 60/120 Hz");
  g.m_player.pos={18.5f,18.5f};tick(g,rate/5,rate);
  check(g.guardState(0)==GuardState::Investigate&&g.state(g.friendlyKey(0,"guard_confirmed"))==1&&g.player().health==100,"Brief occlusion preserves the confirmed entity while cancelling aim");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());
  check(decoded&&restored.guardTarget(0).kind==ShotKind::Player&&restored.state(restored.friendlyKey(0,"guard_confirmed"))==1,"Save preserves confirmed threat identity during occlusion");
  g.m_player.pos={10.5f,6.5f};tick(g,1,rate);check(g.guardState(0)==GuardState::Alert,"Known hostile is reacquired immediately after a short peek");
  check(tick(g,rate/3,rate)==0&&g.player().health==100,"Reacquisition still requires a fresh weapon raise and aim delay");
  if(decoded){restored.m_player.pos={10.5f,6.5f};tick(restored,1+rate/3,rate);check(restored.guardState(0)==g.guardState(0)&&restored.player().health==g.player().health,"Interrupted and restored peeks retain the same reaction and health");}
  g.m_player.pos={18.5f,18.5f};tick(g,rate*16,rate);
  check(g.guardState(0)==GuardState::Patrol&&!g.guardTarget(0).valid()&&!g.state(g.friendlyKey(0,"guard_confirmed")),"An expired investigation/search forgets the entity and returns to patrol");
  g.m_player.pos=standing(g,0);tick(g,1,rate);check(g.guardState(0)==GuardState::Suspicious&&g.player().health==100,"A genuinely forgotten hostile must be recognised again");
 }
 std::vector<float> skipTimes;
 for(int rate:{60,120}){
  auto g=scene([](auto&map){map.definition.playerStart={18.5f,18.5f};map.structures.push_back({8,0,8.2f,24,0,4});map.actorTracks[0].ai.armed=false;map.actorTracks[0].ai.patrol={{{10.5f,6.5f},0,0},{{6.5f,10.5f},0,500}};});
  int frame=0;while(frame<rate*10&&g.state(g.friendlyKey(0,"guard_patrol_wait"))<2500){tick(g,1,rate);++frame;}
  check(g.npcActors()[0].pose.position.x<8&&g.state(g.friendlyKey(0,"guard_point"))==0&&g.state(g.friendlyKey(0,"guard_patrol_wait"))>=2500,"Unreachable patrol fixture physically stalls at a sealed wall");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded,"A stalled patrol can be saved without losing its recovery clock");
  int continued=0;while(continued<rate*8&&g.state(g.friendlyKey(0,"guard_point"))==0){tick(g,1,rate);if(decoded)tick(restored,1,rate);++continued;}
  skipTimes.push_back(float(frame+continued)/rate);
  check(g.state(g.friendlyKey(0,"guard_point"))==1,"A stalled patrol skips the unreachable waypoint instead of retrying forever");
  if(decoded)check(restored.state(restored.friendlyKey(0,"guard_point"))==1&&length(restored.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.03f,"Saved and uninterrupted patrols abandon the same failed point together");
  tick(g,rate*2,rate);check(g.npcActors()[0].pose.position.y>7.5f&&g.npcActors()[0].pose.position.x<8,"Recovered patrol advances to the reachable point without crossing the wall");
 }
 check(std::fabs(skipTimes[0]-skipTimes[1])<.08f,"Patrol recovery is consistent across 60/120 Hz");
 {auto g=scene([](auto&map){map.actorTracks[0].ai.faction=AiFaction::Facility;});check(tick(g,240)==0&&g.player().health==100&&!g.guardTarget(0).valid(),"Facility guards preserve friendship with the player without accidental shots");}
 {auto g=scene([](auto&map){map.actorTracks[0].ai.faction=AiFaction::Facility;map.definition.playerStart={10.5f,10.5f};auto creature=map.actorTracks[0];creature.ai={};creature.visual=ActorVisual::Wasp;creature.health=200;creature.damageState=stateId("policy_creature_damage");creature.deadState=stateId("policy_creature_dead");for(auto&key:creature.keys)key.position={10.5f,6.5f};map.actorTracks.push_back(creature);});
  tick(g,180);check(g.guardTarget(0).kind==ShotKind::Actor&&g.guardTarget(0).index==1&&g.state("policy_creature_damage")>0&&g.player().health==100,"Friendly guard acquires and shoots a visible creature rather than the player");
 }
 {auto g=scene([](auto&map){map.actorTracks[0].ai.faction=AiFaction::Facility;auto worker=map.actorTracks[0];worker.ai={};worker.health=0;worker.ai.faction=AiFaction::Neutral;worker.damageState=stateId("policy_neutral_damage");worker.deadState=stateId("policy_neutral_dead");for(auto&key:worker.keys)key.position={8.5f,6.5f};map.actorTracks.push_back(worker);});
  check(tick(g,180)==0&&!g.guardTarget(0).valid()&&g.state("policy_neutral_damage")==0,"An invulnerable neutral worker never becomes an automatic enemy");
 }
 {auto g=scene([](auto&map){map.actorTracks[0].ai.faction=AiFaction::Neutral;});g.damageFriendly(0,1,g.player().pos,{ShotKind::Player,0,0});tick(g,120);check(g.guardTarget(0).kind==ShotKind::Player&&g.player().health<100,"A neutral guard can retaliate against its explicitly identified personal attacker");}
 {auto g=scene([](auto&map){map.definition.playerStart={18.5f,18.5f};map.actorTracks[0].ai.faction=AiFaction::Facility;});Enemy enemy;enemy.pos=enemy.home=enemy.lastKnown={5.7f,6.5f};g.m_enemies={enemy};int frames=0;while(g.state("policy_guard_damage")==0&&frames++<180)tick(g,1);tick(g,2);
  check(g.state("policy_guard_damage")>0&&g.guardTarget(0).kind==ShotKind::Enemy&&g.guardTarget(0).index==0&&g.player().health==100,"Guard retaliates against the actual creature that struck it from behind");
 }
 {auto g=scene([](auto&map){auto&guard=map.actorTracks[0];guard.ai.squad=stateId("policy_squad");auto other=guard;other.ai.armed=false;other.ai.patrol={{{6.5f,8.5f},0,1000}};for(auto&key:other.keys){key.position={6.5f,8.5f};key.yaw=kPi;}other.damageState=stateId("policy_teammate_damage");other.deadState=stateId("policy_teammate_dead");map.actorTracks.push_back(other);map.structures.push_back({7,7.5f,11.5f,7.7f,0,4});});tick(g,50);
  check(g.guardState(0)==GuardState::Alert&&g.guardState(1)==GuardState::Investigate&&g.guardTarget(1).kind==ShotKind::Player,"A nearby squadmate's fresh sighting supplies last-known threat knowledge");
  check(g.state(g.friendlyKey(1,"guard_seen"))==0&&g.state(g.friendlyKey(1,"weapon_rounds"))==0,"Squad reports do not fabricate personal visibility or arm an unarmed teammate");
 }
 for(bool otherFaction:{false,true}){auto g=scene([&](auto&map){auto&guard=map.actorTracks[0];guard.ai.squad=stateId("policy_squad");auto other=guard;other.ai.armed=false;other.ai.patrol={{{6.5f,8.5f},0,1000}};if(otherFaction)other.ai.faction=AiFaction::Neutral;else other.ai.squad=stateId("policy_other_squad");for(auto&key:other.keys){key.position={6.5f,8.5f};key.yaw=kPi;}other.damageState=stateId("policy_isolated_damage");other.deadState=stateId("policy_isolated_dead");map.actorTracks.push_back(other);map.structures.push_back({7,7.5f,11.5f,7.7f,0,4});});tick(g,50);check(g.guardState(0)==GuardState::Alert&&g.guardState(1)==GuardState::Patrol&&!g.guardTarget(1).valid(),"Squad knowledge does not cross faction or group boundaries");}
 {auto g=scene([](auto&map){map.definition.playerStart={12.5f,7.5f};auto prototype=map.actorTracks[0];map.actorTracks.clear();for(int i=0;i<4;++i){auto guard=prototype;Vec2 p{6.5f,2.5f+3*i};for(auto&key:guard.keys){key.position=p;key.yaw=std::atan2(12.5f-p.x,7.5f-p.y);}guard.ai.patrol={{p,0,1000}};guard.ai.squad=stateId("policy_fireteam");guard.ai.attackDamage=1;guard.ai.enableState=stateId("policy_enable/"+std::to_string(i));guard.damageState=stateId("policy_squad_damage/"+std::to_string(i));guard.deadState=stateId("policy_squad_dead/"+std::to_string(i));map.actorTracks.push_back(guard);}});
  for(size_t i=0;i<4;++i)g.setState(g.npcActors()[i].track().ai.enableState,1);unsigned fired=0;bool bounded=true;
  for(int frame=0;frame<840;++frame){tick(g,1);int slots=0;for(size_t i=0;i<4;++i){slots+=g.guardFireSlot(i);if(g.npcWeaponPose(i).flash>0)fired|=1u<<i;}bounded&=slots<=2;}
  check(bounded&&fired==15&&g.player().health<100,"Squad limits fire leases to two while rotating opportunities fairly across four guards");
  auto flank1=g.npcActors()[1].pose.position,flank2=g.npcActors()[2].pose.position;
  check(g.state(g.friendlyKey(1,"guard_flank_done"))==1&&g.state(g.friendlyKey(2,"guard_flank_done"))==1&&length(flank1-Vec2{6.5f,5.5f})>.4f&&length(flank2-Vec2{6.5f,8.5f})>.4f&&g.state("policy_squad_damage/1")==0&&g.state("policy_squad_damage/2")==0,"Squad flank roles reach distinct firing positions without damaging teammates");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded,"An active fireteam can be saved with leased shooting opportunities");if(decoded){int a=tick(g,120),b=tick(restored,120);check(a==b&&g.player().health==restored.player().health,"Restored squad retains its firing order and health outcome");check(length(g.npcActors()[1].pose.position-flank1)<.05f&&length(g.npcActors()[2].pose.position-flank2)<.05f,"Completed squad flanks hold their firing positions instead of orbiting the target");}
  g.damageFriendly(0,100,g.player().pos,{ShotKind::Player,0,0});g.setState(g.npcActors()[1].track().ai.enableState,0);float health=g.player().health;tick(g,240);
  check(!g.guardFireSlot(0)&&!g.guardFireSlot(1)&&g.player().health<health,"Dead and disabled squadmates release slots so surviving members can fire");
 }
 {auto g=scene([](auto&map){map.definition.playerStart={12.5f,7.5f};auto prototype=map.actorTracks[0];map.actorTracks.clear();for(int i=0;i<4;++i){auto guard=prototype;Vec2 p{6.5f,2.5f+3*i};for(auto&key:guard.keys){key.position=p;key.yaw=std::atan2(12.5f-p.x,7.5f-p.y);}guard.ai.patrol={{p,0,1000}};guard.ai.squad=stateId("policy_long_aim_fireteam");guard.ai.aimMs=3000;guard.ai.attackDamage=1;guard.damageState=stateId("policy_long_aim_damage/"+std::to_string(i));guard.deadState=stateId("policy_long_aim_dead/"+std::to_string(i));map.actorTracks.push_back(guard);}});unsigned fired=0;bool bounded=true;
  for(int frame=0;frame<1080;++frame){tick(g,1);int slots=0;for(size_t i=0;i<4;++i){slots+=g.guardFireSlot(i);if(g.npcWeaponPose(i).flash>0)fired|=1u<<i;}bounded&=slots<=2;}
  check(bounded&&fired==15&&g.player().health<100,"Maximum authored three-second aim completes for every squadmate while keeping at most two fire leases");
 }
 for(int rate:{60,120}){auto g=scene([](auto&map){map.definition.playerStart={7.3f,6.5f};});int frames=0,shots=0;while(g.player().health==100&&frames<rate*3){shots+=tick(g,1,rate);++frames;}
  report<<"melee first-hit rate="<<rate<<" seconds="<<float(frames)/rate<<'\n';
  check(g.player().health<100&&shots==0&&g.state(g.friendlyKey(0,"weapon_rounds"))==6&&frames>rate*.7f,"Close hostile receives a telegraphed melee strike without gunfire or ammunition use");
  float health=g.player().health;check(tick(g,rate/2,rate)==0&&g.player().health==health,"Melee recovery prevents rapid repeated contact hits");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded,"Interrupted melee recovery can be saved");if(decoded){tick(g,rate*2,rate);tick(restored,rate*2,rate);check(g.player().health==restored.player().health&&g.state(g.friendlyKey(0,"weapon_rounds"))==restored.state(restored.friendlyKey(0,"weapon_rounds")),"Saved melee resumes the same health and ammunition outcome");}
 }
 {auto g=scene([](auto&map){map.definition.playerStart={7.3f,6.5f};map.structures.push_back({7,0,7.02f,24,0,3});});check(tick(g,180)==0&&g.player().health==100&&g.state(g.friendlyKey(0,"weapon_rounds"))==6,"Close melee cannot reach through a thin full-height wall");}
 {auto g=scene([](auto&map){map.definition.playerStart={7.3f,6.5f};});g.equipWeapon(WeaponId::Fists);g.m_player.angle=kPi;InputState input{};input.guard=true;int frames=0;while(g.player().health==100&&frames++<180)g.update(input,1.f/60);check(g.player().health<100&&g.player().health>88&&g.state(g.friendlyKey(0,"weapon_rounds"))==6,"Bare-handed frontal guarding reduces a close guard's twelve-damage bash");}
 {auto g=scene([](auto&map){map.structures.push_back({7.1f,7.1f,7.8f,9,0,3});});tick(g,60);auto start=g.npcActors()[0].pose.position;g.setState(g.friendlyKey(0,"weapon_rounds"),0);g.setState(g.friendlyKey(0,"weapon_saved"),1);bool reached=false,moved=false,safe=true;int shots=0,hiddenFrames=0,longestHidden=0;std::unique_ptr<Game> restored;
  for(int i=0;i<108;++i){shots+=tick(g,1);if(restored)tick(*restored,1);if(i==30){restored=std::make_unique<Game>(g.m_customCampaign);if(!restored->decodeSave(g.encodeSave()))restored.reset();}
   const auto&npc=g.npcActors()[0];auto pose=npc.pose;moved|=length(pose.position-start)>.2f;safe&=g.friendlyWorld(npc.level).fits(pose.position.x,pose.position.y,pose.z,npc.track().scale);bool head=g.soundRayClear(pose.position,pose.z+npc.track().scale*.85f,g.player().pos,g.player().z+g.player().eye),body=g.soundRayClear(pose.position,pose.z+npc.track().scale*.6f,g.player().pos,g.player().z+g.player().hullHeight()*.6f);reached|=!head&&!body;hiddenFrames=!head&&!body&&g.state(g.friendlyKey(0,"weapon_reload"))>0?hiddenFrames+1:0;longestHidden=std::max(longestHidden,hiddenFrames);}
  check(shots==0&&moved&&reached&&safe&&g.player().health==100,"An empty-tube guard moves to reachable cover hiding both head and body during reload");
  check(longestHidden>=12,"Reloading guard remains protected instead of immediately chasing back into exposure");
  check(restored&&length(restored->npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.03f&&restored->player().health==g.player().health&&restored->state(restored->friendlyKey(0,"weapon_reload"))==g.state(g.friendlyKey(0,"weapon_reload")),"Save resumes an interrupted defensive retreat and remaining reload duration");
 }
 for(auto mode:{ActorAiMode::Civilian,ActorAiMode::Escort})for(auto faction:{AiFaction::Creature,AiFaction::Neutral,AiFaction::Facility})for(bool scripted:{false,true}){
  auto g=scene([&](auto&map){map.definition.playerStart={18.5f,18.5f};auto&worker=map.actorTracks[0];worker.ai={};worker.ai.mode=mode;worker.ai.faction=faction;
   if(scripted){auto creature=worker;creature.ai={};creature.visual=ActorVisual::Wasp;creature.damageState=stateId("fear_creature_damage");creature.deadState=stateId("fear_creature_dead");for(auto&key:creature.keys)key.position={8,6.5f};map.actorTracks.push_back(creature);}
  });
  if(!scripted){Enemy creature;creature.pos=creature.home=creature.lastKnown={8,6.5f};creature.heading=kPi;g.m_enemies={creature};}
  if(mode==ActorAiMode::Escort)g.setState(g.friendlyKey(0,"hold"),1);Vec2 start=g.npcActors()[0].pose.position;tick(g,30);
  report<<"worker-fear mode="<<int(mode)<<" faction="<<int(faction)<<" scripted="<<scripted<<'\n';
  if(faction==AiFaction::Facility)check(g.state(g.friendlyKey(0,"fear"))>0&&length(g.npcActors()[0].pose.position-start)>.1f,"Facility civilians and escorts retain fear and physically flee hostile enemy or scripted creatures");
  else{
   check(g.state(g.friendlyKey(0,"fear"))==0&&length(g.npcActors()[0].pose.position-start)<.001f,"Creature-aligned and neutral civilians/held escorts remain calm around nonhostile enemy or scripted creatures");
   g.damageFriendly(0,1,{8,6.5f},scripted?AiTargetRef{ShotKind::Actor,0,1}:AiTargetRef{ShotKind::Enemy,0,0});tick(g,30);
   check(g.state(g.friendlyKey(0,"fear"))>0&&length(g.npcActors()[0].pose.position-start)>.1f,"Actual injury still makes a normally nonhostile civilian or escort flee its captured damage source");
  }
 }
 for(const char*field:{"guard_confirmed","guard_patrol_wait","guard_patrol_progress","guard_under_fire"}){auto g=scene([](auto&){});g.setState(g.friendlyKey(0,field),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Corrupted saved policy state is rejected without overflow or invalid continuation");}
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}

#include "../../../Game.h"
#include "../../engineer/Drones.h"
#include "../../../../world/CustomCampaign.h"
#include <array>
#include <fstream>
#include <limits>
namespace retro {
namespace {
StateId dangerDroneKey(int slot,std::string_view field){
 auto value="drone/slot/"+std::to_string(slot)+"/"+std::string(field);StateId key=2166136261u;
 for(unsigned char c:value)key=(key^c)*16777619u;return key|0x80000000u;
}
StateId dangerGrenadeKey(int slot,std::string_view field){
 auto value="grenade/slot/"+std::to_string(slot)+"/"+std::string(field);StateId key=2166136261u;
 for(unsigned char c:value)key=(key^c)*16777619u;return key|0x80000000u;
}
}
bool Game::testGuardDanger(){
 std::ofstream report("guard-danger-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](auto edit){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Guard danger validation";campaign->key=882747;
  auto map=std::make_shared<AuthoredMapData>();map->name="Danger response room";
  map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("danger_clock");timer.finishState=stateId("danger_done");
  timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{60000,{6.5f,6.5f},0,kPi*.5f}};
  guard.scale=1.6f;guard.health=100;guard.damageState=stateId("danger_damage/0");guard.deadState=stateId("danger_dead/0");
  guard.ai.mode=ActorAiMode::Guard;guard.ai.faction=AiFaction::Raider;guard.ai.speed=2.f;guard.ai.patrol={{{6.5f,6.5f},0,6000}};
  map->actorTracks.push_back(guard);edit(*map);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int frames,int rate=60){for(int i=0;i<frames;++i)g.update({},1.f/rate);};
 auto known=[](Game&g,size_t actor=0){
  const auto&npc=g.m_friendlyActors[actor];auto shift=g.chunkOffset(g.level())-g.chunkOffset(npc.level);
  AiTarget target;target.kind=ShotKind::Player;target.level=g.level();target.index=0;target.position=g.player().pos+shift;
  target.feet=g.player().z;target.height=g.player().hullHeight();target.eye=g.player().eye;target.faction=AiFaction::Player;
  g.rememberGuardTarget(actor,target);g.setState(g.friendlyKey(actor,"guard_mode"),int(GuardState::Alert));
  for(const char*f:{"guard_confirmed","guard_seen","guard_memory"})g.setState(g.friendlyKey(actor,f),1);
  auto global=g.player().pos+g.chunkOffset(g.level());g.setState(g.friendlyKey(actor,"guard_x"),int(std::round(global.x*1000)));
  g.setState(g.friendlyKey(actor,"guard_y"),int(std::round(global.y*1000)));g.setState(g.friendlyKey(actor,"guard_z"),int(std::round(target.feet*1000)));
 };
 auto safePose=[](Game&g,size_t actor){const auto&npc=g.npcActors()[actor];const auto&p=npc.pose;
  return p.position.x>=.35f&&p.position.x<=23.65f&&p.position.y>=.35f&&p.position.y<=23.65f&&
   std::fabs(p.z)<.001f&&g.friendlyWorld(npc.level).fits(p.position.x,p.position.y,p.z,npc.track().scale);
 };
 std::vector<Vec2> escaped;
 for(int rate:{60,120})for(bool allied:{false,true}){
  auto g=scene([](auto&){});tick(g,rate/3,rate);auto start=g.npcActors()[0].pose.position;
  check(g.guardState(0)==GuardState::Patrol&&g.npcActors()[0].pose.clip==0&&length(start-Vec2{6.5f,6.5f})<.001f,"Danger fixture starts with a stationary guard on an authored patrol pause");
  const Vec2 hazard{7.2f,6.5f};auto owner=allied?AiTargetRef{ShotKind::Npc,0,0}:AiTargetRef{ShotKind::Player,0,0};
  check(g.spawnTacticalGrenade({hazard.x,hazard.y,.1f},{0,0,0},owner,0,allied?AiFaction::Raider:AiFaction::Player),"Stationary guard receives a real hostile or allied grenade with a finite warning fuse");
  bool observed=false,safe=true,bounded=true;float moved=0;
  for(int frame=0;frame<rate*3;++frame){auto before=g.npcActors()[0].pose.position;tick(g,1,rate);auto after=g.npcActors()[0].pose.position;
   observed|=g.state(g.friendlyKey(0,"guard_danger_ms"))>0;safe&=safePose(g,0);bounded&=length(after-before)<=2.f*1.35f/rate+.003f;
   moved=std::max(moved,length(after-start));}
  report<<"escape Hz="<<rate<<" allied="<<allied<<" move="<<moved<<" radius="<<length(g.npcActors()[0].pose.position-hazard)<<" damage="<<g.state("danger_damage/0")<<'\n';
  check(observed&&moved>3.4f,"Stationary guards interrupt their task and physically flee hostile and allied grenades at 60/120 Hz");
  check(safe&&bounded,"Grenade escape preserves floor support, authored bounds and a finite movement speed at 60/120 Hz");
  check(g.state("danger_damage/0")==0&&g.tacticalGrenades().empty()&&length(g.npcActors()[0].pose.position-hazard)>3.5f,"Physical escape survives the real detonation outside its damage radius");
  if(!allied)escaped.push_back(g.npcActors()[0].pose.position);
 }
 check(escaped.size()==2&&length(escaped[0]-escaped[1])<.12f,"Escape outcomes stay consistent between 60 and 120 Hz");
 {auto g=scene([](auto&){});bool spawned=g.spawnTacticalGrenade({7.2f,6.5f,.8f},{3,0,0},{ShotKind::Player,0,0},0,AiFaction::Player,6000);
  tick(g,1);check(spawned&&g.state(dangerGrenadeKey(0,"vx"))>2000&&!g.state(g.friendlyKey(0,"guard_danger_ms")),
   "A fast grenade still in flight with more than one second of fuse does not cause an immediate stationary blast panic");
  // A valid stored fuse isolates the final-second cue without artificially moving the guard or projectile.
  g.setState(dangerGrenadeKey(0,"fuse_us"),900000);tick(g,1);
  check(g.state(dangerGrenadeKey(0,"vx"))>2000&&g.state(g.friendlyKey(0,"guard_danger_ms"))>0,
   "The same exposed moving grenade becomes a perceived danger during its final fuse warning");
 }
 for(int rate:{60,120}){
  auto g=scene([](auto&){});const Vec2 primary{7.2f,6.5f},secondary{1.8f,6.5f},start=g.npcActors()[0].pose.position;
  check(length(start-secondary)>4.5f&&length(start-secondary)<6.f,"Second grenade fixture is audible nearby but starts outside the guard's danger radius");
  bool spawned=g.spawnTacticalGrenade({primary.x,primary.y,.1f},{0,0,0},{ShotKind::Player,0,0})&&
   g.spawnTacticalGrenade({secondary.x,secondary.y,.1f},{0,0,0},{ShotKind::Player,0,0});
  check(spawned&&g.tacticalGrenades().size()==2,"Two independent physical grenades share the room and retain their real warning fuses");
  tick(g,1,rate);Vec2 endpoint{g.state(g.friendlyKey(0,"guard_danger_goal_x"))*.001f,g.state(g.friendlyKey(0,"guard_danger_goal_y"))*.001f};
  check(g.state(g.friendlyKey(0,"guard_danger_goal"))&&length(endpoint-primary)>3.5f&&length(endpoint-secondary)>3.5f&&std::fabs(endpoint.y-start.y)>1.f,
   "Escape endpoint accounts for the second clear fuse instead of choosing the radial route into its blast");
  bool safe=true,secondClear=true;for(int frame=1;frame<rate*3;++frame){tick(g,1,rate);safe&=safePose(g,0);secondClear&=length(g.npcActors()[0].pose.position-secondary)>3.5f;}
  report<<"two-fuse Hz="<<rate<<" endpoint="<<endpoint.x<<','<<endpoint.y<<" final="<<g.npcActors()[0].pose.position.x<<','<<g.npcActors()[0].pose.position.y<<'\n';
  check(safe&&secondClear&&g.state("danger_damage/0")==0&&g.tacticalGrenades().empty(),"Physical two-grenade escape avoids entering the second damage radius and survives both detonations at 60/120 Hz");
 }
 for(int rate:{60,120}){
  auto g=scene([](auto&map){map.structures.push_back({7.2f,0,7.35f,24,0,4});});
  g.spawnTacticalGrenade({8.5f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0});bool phantom=false,crossed=false;
  for(int i=0;i<rate*3;++i){tick(g,1,rate);phantom|=g.state(g.friendlyKey(0,"guard_danger_ms"))>0;crossed|=g.npcActors()[0].pose.position.x>=7.2f;}
  check(!phantom&&!crossed&&g.state("danger_damage/0")==0,"A full-height wall occludes the fuse cue and blast without a phantom escape or wall crossing");
 }
 for(int rate:{60,120}){
  auto g=scene([](auto&map){map.structures.push_back({5.5f,5.8f,5.7f,7.2f,0,3});});const Vec2 hazard{7.5f,6.5f};
  g.spawnTacticalGrenade({hazard.x,hazard.y,.1f},{0,0,0},{ShotKind::Player,0,0});bool safe=true;float side=0;
  for(int i=0;i<rate*3;++i){tick(g,1,rate);safe&=safePose(g,0);side=std::max(side,std::fabs(g.npcActors()[0].pose.position.y-6.5f));}
  check(side>1.f&&safe&&g.state("danger_damage/0")==0&&length(g.npcActors()[0].pose.position-hazard)>3.5f,"A blocked radial exit selects an angled physical escape without entering the obstruction");
 }
 for(bool locked:{false,true}){
  auto g=scene([&](auto&map){
   // A narrow corridor makes the northern door the only useful escape. Its
   // support and walls are identical in the ordinary and permission-locked cases.
   map.structures.push_back({5.9f,0,6.1f,13,0,3});map.structures.push_back({6.9f,0,7.1f,13,0,3});
   map.structures.push_back({6.1f,5.5f,6.9f,5.7f,0,3});Door door{};door.left=6;door.right=7;door.y=7.5f;
   if(locked)door.requireState=stateId("danger_door_permission");map.doors.push_back(door);
  });
  g.spawnTacticalGrenade({6.5f,5.8f,.1f},{0,0,0},{ShotKind::Player,0,0},0,AiFaction::Player,6000);
  bool safe=true,opened=false;for(int frame=0;frame<180;++frame){tick(g,1);const auto&npc=g.npcActors()[0];
   safe&=safePose(g,0)&&!g.world().doorBlocks(npc.pose.position.x,npc.pose.position.y,npc.pose.z,npc.track().scale);opened|=g.world().doors()[0].opening;}
  const auto&door=g.world().doors()[0];float position=g.npcActors()[0].pose.position.y;
  report<<"danger door locked="<<locked<<" open="<<door.open<<" y="<<position<<'\n';
  if(locked)check(!opened&&door.open==0&&position<7.3f&&safe,"A permission-locked door stays closed and physically blocks the escape without hull penetration");
  else check(opened&&door.open>.2f&&position>8.f&&safe,"Normal escape movement requests and opens an ordinary door before physically walking through it");
 }
 auto trapped=[](auto&map){
  map.definition.playerStart={10.5f,6.5f};auto&guard=map.actorTracks[0];guard.ai.armed=true;guard.ai.attackDamage=1;guard.ai.squad=stateId("danger_team");guard.ai.grenades=2;
  // Low barriers block the standing hull while leaving the target and weapon ray exposed.
  map.structures.push_back({5.9f,5.9f,6.05f,7.1f,0,.6f});map.structures.push_back({6.95f,5.9f,7.1f,7.1f,0,.6f});
  map.structures.push_back({5.9f,5.9f,7.1f,6.05f,0,.6f});map.structures.push_back({5.9f,6.95f,7.1f,7.1f,0,.6f});
 };
 for(int rate:{60,120}){
  auto g=scene(trapped);known(g);g.setState(g.friendlyKey(0,"weapon_saved"),1);g.setState(g.friendlyKey(0,"weapon_rounds"),6);
  g.setState(g.friendlyKey(0,"weapon_raise"),1000);g.setState(g.friendlyKey(0,"weapon_aim"),g.npcActors()[0].track().ai.aimMs-1);
  g.setState(g.friendlyKey(0,"guard_slot"),1);g.setState(g.friendlyKey(0,"guard_slot_ms"),1600);g.setState(g.friendlyKey(0,"grenade_windup"),440);
  g.spawnTacticalGrenade({6.7f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0},0,AiFaction::Player,6000);
  bool held=true,noAttack=true,active=true;for(int i=0;i<rate;++i){tick(g,1,rate);
   held&=length(g.npcActors()[0].pose.position-Vec2{6.5f,6.5f})<.03f&&safePose(g,0);
   active&=g.state(g.friendlyKey(0,"guard_danger_ms"))>0;noAttack&=g.player().health==100&&g.state(g.friendlyKey(0,"weapon_rounds"))==6&&
    !g.state(g.friendlyKey(0,"weapon_aim"))&&!g.state(g.friendlyKey(0,"guard_slot"))&&!g.state(g.friendlyKey(0,"grenade_windup"))&&!g.state(g.friendlyKey(0,"grenade_used"));}
  check(held&&active&&noAttack,"A trapped guard holds a valid hull and releases aim, throw windup and its firing lease instead of attacking during danger");
 }
 {auto g=scene([](auto&map){map.definition.playerStart={7.4f,6.5f};map.actorTracks[0].ai.armed=true;});known(g);
  g.setState(g.friendlyKey(0,"weapon_saved"),1);g.setState(g.friendlyKey(0,"weapon_rounds"),6);
  g.setState(g.friendlyKey(0,"weapon_action"),int(GuardAction::Bash));g.setState(g.friendlyKey(0,"weapon_action_ms"),290);
  g.setState(g.friendlyKey(0,"weapon_action_events"),GuardActionStart);g.setState(g.friendlyKey(0,"weapon_bash"),290);g.setState(g.friendlyKey(0,"weapon_raise"),1000);
  g.spawnTacticalGrenade({6.5f,5.8f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1);
  check(g.player().health==100&&!g.state(g.friendlyKey(0,"weapon_bash"))&&!g.state(g.friendlyKey(0,"weapon_bash_pose"))&&g.state(g.friendlyKey(0,"weapon_action"))==int(GuardAction::None),"Danger cancels a pre-impact bash on the frame that would otherwise damage the player");
 }
 auto medic=[](auto&map){auto&guard=map.actorTracks[0];guard.ai.role=GuardSquadRole::Medic;guard.ai.armed=true;guard.ai.squad=stateId("danger_team");
  auto patient=guard;patient.ai.role=GuardSquadRole::Default;patient.ai.armed=false;patient.damageState=stateId("danger_damage/1");patient.deadState=stateId("danger_dead/1");
  for(auto&key:patient.keys)key.position={7.5f,6.5f};patient.ai.patrol={{{7.5f,6.5f},0,6000}};map.actorTracks.push_back(patient);
 };
 {auto g=scene(medic);g.setState(stateId("danger_damage/1"),50);g.setState(g.friendlyKey(0,"support_target"),2);g.setState(g.friendlyKey(0,"support_ms"),2390);
  g.spawnTacticalGrenade({6.5f,5.8f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1);
  check(g.state("danger_damage/1")==50&&!g.state(g.friendlyKey(0,"support_used"))&&!g.state(g.friendlyKey(0,"support_ms"))&&g.state(g.friendlyKey(0,"guard_danger_ms"))>0,"Danger interrupts a 2390ms treatment before its same-frame heal or finite charge is committed");
  int heals=0,previous=g.state("danger_damage/1");for(int frame=0;frame<60*18;++frame){tick(g,1);int damage=g.state("danger_damage/1");heals+=damage<previous;previous=damage;}
  check(heals==1&&g.state("danger_damage/1")==20&&g.state(g.friendlyKey(0,"support_used"))==1&&!g.state(g.friendlyKey(0,"support_ms")),
   "After escaping, the Medic reapproaches its living patient and spends exactly one charge on one thirty-point treatment without replaying the interrupted heal");
 }
 auto engineer=[](auto&map){auto&guard=map.actorTracks[0];guard.ai.role=GuardSquadRole::Engineer;guard.ai.armed=true;guard.ai.squad=stateId("danger_team");};
 {auto g=scene([&](auto&map){engineer(map);map.definition.playerStart={10.5f,6.5f};});known(g);
  g.setState(g.friendlyKey(0,"drone_windup"),1190);g.setState(g.friendlyKey(0,"drone_busy"),1);
  g.spawnTacticalGrenade({6.5f,5.8f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1);
  check(g.engineerDrones().empty()&&!g.state(g.friendlyKey(0,"drone_used"))&&!g.state(g.friendlyKey(0,"drone_windup"))&&!g.state(g.friendlyKey(0,"drone_busy")),"Danger cancels 1190ms assembly before the same-frame deployment spends drone stock");
 }
 {auto g=scene(engineer);bool deployed=g.deployEngineerDrone(0);check(deployed,"Repair interruption fixture deploys an actual owned drone");
  if(deployed){int slot=g.engineerDrones()[0].slot;g.damageEngineerDrone(slot,20);g.setState(g.friendlyKey(0,"drone_repair_ms"),99);
   int health=g.state(dangerDroneKey(slot,"hp")),stock=g.state(g.friendlyKey(0,"drone_repair_charge"));
   g.spawnTacticalGrenade({6.5f,6.f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1);
   check(g.state(dangerDroneKey(slot,"hp"))==health&&g.state(g.friendlyKey(0,"drone_repair_charge"))==stock&&g.state(g.friendlyKey(0,"drone_used"))==1&&
    !g.state(g.friendlyKey(0,"drone_repair_ms"))&&!g.state(g.friendlyKey(0,"drone_busy")),"Danger cancels a 99ms maintenance pulse without healing the drone or consuming repair stock");
  }
 }
 for(int rate:{60,120}){
  auto g=scene([](auto&map){map.actorTracks[0].ai.armed=true;});
  g.setState(g.friendlyKey(0,"weapon_saved"),1);g.setState(g.friendlyKey(0,"weapon_rounds"),0);g.setState(g.friendlyKey(0,"weapon_reloading"),1);
  g.setState(g.friendlyKey(0,"weapon_reload"),700);g.setState(g.friendlyKey(0,"weapon_reload_events"),int(guardActionMask(GuardAction::Reload,1100)));
  g.setState(g.friendlyKey(0,"weapon_action"),int(GuardAction::Reload));g.setState(g.friendlyKey(0,"weapon_action_ms"),1100);
  g.setState(g.friendlyKey(0,"weapon_action_events"),int(guardActionMask(GuardAction::Reload,1100)));
  g.spawnTacticalGrenade({7.2f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0});bool paused=true,seen=false;
  for(int i=0;i<rate;++i){tick(g,1,rate);if(g.state(g.friendlyKey(0,"guard_danger_ms"))>0){seen=true;paused&=g.state(g.friendlyKey(0,"weapon_reload"))==700&&
   g.state(g.friendlyKey(0,"weapon_rounds"))==0&&g.state(g.friendlyKey(0,"weapon_action_ms"))==1100&&g.state(g.friendlyKey(0,"weapon_reload_events"))==int(guardActionMask(GuardAction::Reload,1100));}}
  check(seen&&paused,"Danger pauses a canonical partial reload clock and its already-consumed animation events without refilling ammunition");
  tick(g,rate*5,rate);check(!g.state(g.friendlyKey(0,"guard_danger_ms"))&&!g.state(g.friendlyKey(0,"weapon_reload"))&&!g.state(g.friendlyKey(0,"weapon_reloading"))&&
   g.state(g.friendlyKey(0,"weapon_rounds"))==6&&g.state("danger_damage/0")==0,"After danger expires the same partial reload resumes and completes with one finite magazine fill");
 }
 for(int rate:{60,120}){
  auto g=scene([](auto&){});g.spawnTacticalGrenade({7.2f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,rate/5,rate);
  check(g.state(g.friendlyKey(0,"guard_danger_ms"))>0&&g.npcActors()[0].pose.clip==1,"Save fixture interrupts a real escape with an active grenade and physical walking pose");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());bool same=decoded;
  for(const char*f:{"guard_danger_ms","guard_danger_fraction","guard_danger_goal","guard_danger_wait","guard_danger_x","guard_danger_y","guard_danger_z","guard_danger_goal_x","guard_danger_goal_y","guard_danger_goal_z"})
   same&=restored.state(restored.friendlyKey(0,f))==g.state(g.friendlyKey(0,f));
  check(same&&restored.tacticalGrenades().size()==1&&length(restored.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.002f,"Save restores the physical grenade, danger memory, cached escape and walking position");
  if(decoded){tick(g,rate*4,rate);tick(restored,rate*4,rate);check(g.tacticalGrenades().empty()&&restored.tacticalGrenades().empty()&&g.state("danger_damage/0")==0&&
   restored.state("danger_damage/0")==0&&length(restored.npcActors()[0].pose.position-g.npcActors()[0].pose.position)<.12f,"Saved and uninterrupted escapes detonate once and survive with matching physical continuation");}
 }
 for(const char*f:{"guard_danger_ms","guard_danger_fraction","guard_danger_goal","guard_danger_wait","guard_danger_x","guard_danger_y","guard_danger_z","guard_danger_goal_x","guard_danger_goal_y","guard_danger_goal_z"}){
  auto g=scene([](auto&){});g.setState(g.friendlyKey(0,f),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);
  check(!restored.decodeSave(g.encodeSave()),"Malformed saved danger clocks, flags, hazard or escape coordinates are rejected safely");
 }
 {auto g=scene([](auto&){});g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Patrol));g.setState(g.friendlyKey(0,"guard_memory"),0);
  g.setState(g.friendlyKey(0,"guard_return"),0);check(!g.friendlyChunkNeeded(0),"An idle home Patrol without threat memory does not hold extra friendly residency");
  g.setState(g.friendlyKey(0,"guard_danger_ms"),600);check(g.friendlyChunkNeeded(0),"An active danger response holds the guard's physical chunk resident even while Patrol has no enemy memory");
  g.setState(g.friendlyKey(0,"guard_danger_ms"),0);check(!g.friendlyChunkNeeded(0),"Ending the danger response releases residency once the home Patrol has no other active task");
 }
 {auto g=scene([](auto&map){map.actorTracks[0].ai.enableState=stateId("danger_enabled");});g.setState(stateId("danger_enabled"),1);
  g.spawnTacticalGrenade({7.2f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1);
  check(g.state(g.friendlyKey(0,"guard_danger_ms"))>0&&g.state(g.friendlyKey(0,"guard_danger_goal")),"Disable fixture begins with a genuine active danger goal");
  g.setState(stateId("danger_enabled"),0);tick(g,1);
  bool clear=true;for(const char*f:{"guard_danger_ms","guard_danger_goal","guard_danger_wait","guard_danger_fraction"})clear&=!g.state(g.friendlyKey(0,f));
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());
  check(clear&&!g.friendlyChunkNeeded(0)&&decoded&&!restored.state(restored.friendlyKey(0,"guard_danger_ms"))&&!restored.state(restored.friendlyKey(0,"guard_danger_goal")),
   "Disabling an escaping guard clears danger clocks, cached goal and residency while preserving a decodable save");
  g.setState(stateId("danger_enabled"),1);tick(g,1);check(g.state(g.friendlyKey(0,"guard_danger_ms"))>0,
   "Re-enabling the guard detects the still-exposed real grenade through a fresh danger response");
 }
 {auto g=scene([](auto&map){map.definition.origin={48,24};map.actorTracks[0].ai.squad=stateId("danger_reservation_team");auto peer=map.actorTracks[0];
   for(auto&key:peer.keys)key.position={14.5f,14.5f};peer.ai.patrol={{{14.5f,14.5f},0,6000}};peer.damageState=stateId("danger_damage/1");peer.deadState=stateId("danger_dead/1");map.actorTracks.push_back(peer);
  });auto origin=g.chunkOffset(0);g.setState(g.friendlyKey(1,"guard_danger_ms"),600);g.setState(g.friendlyKey(1,"guard_danger_goal"),1);
  g.setState(g.friendlyKey(1,"guard_danger_goal_x"),int(std::round((origin.x+3.5f)*1000)));g.setState(g.friendlyKey(1,"guard_danger_goal_y"),int(std::round((origin.y+6.5f)*1000)));
  g.setState(g.friendlyKey(1,"guard_danger_goal_z"),0);
  check(g.guardPositionReserved(0,{3.8f,6.5f},0)&&!g.guardPositionReserved(0,{3.8f,6.5f},3.f),
   "A named peer's active global escape destination reserves nearby space in the correct physical chunk and floor");
  g.setState(g.friendlyKey(1,"guard_danger_ms"),0);check(!g.guardPositionReserved(0,{3.8f,6.5f},0),"Expired peer danger releases its escape destination reservation");
 }
 {auto g=scene([](auto&map){auto prototype=map.actorTracks[0];map.actorTracks.clear();for(int actor=0;actor<9;++actor){auto guard=prototype;Vec2 p{6.5f,3.5f+actor*.75f};
   for(auto&key:guard.keys)key.position=p;guard.ai.patrol={{p,0,6000}};guard.ai.squad=stateId("danger_crowd_team");guard.damageState=stateId("danger_crowd_damage/"+std::to_string(actor));guard.deadState=stateId("danger_crowd_dead/"+std::to_string(actor));map.actorTracks.push_back(guard);}});
  g.spawnTacticalGrenade({7.2f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0},0,AiFaction::Player,6000);unsigned serviced=0;bool bounded=true,safe=true,distinct=true,pairs=false;
  for(int frame=0;frame<60;++frame){tick(g,1);bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;
   for(size_t actor=0;actor<9;++actor){serviced|=g.state(g.friendlyKey(actor,"guard_danger_goal"))?1u<<actor:0;safe&=safePose(g,actor);
    if(!g.state(g.friendlyKey(actor,"guard_danger_goal")))continue;Vec2 a{g.state(g.friendlyKey(actor,"guard_danger_goal_x"))*.001f,g.state(g.friendlyKey(actor,"guard_danger_goal_y"))*.001f};
    for(size_t peer=actor+1;peer<9;++peer)if(g.state(g.friendlyKey(peer,"guard_danger_goal"))){Vec2 b{g.state(g.friendlyKey(peer,"guard_danger_goal_x"))*.001f,g.state(g.friendlyKey(peer,"guard_danger_goal_y"))*.001f};pairs=true;distinct&=length(a-b)>=.695f;}}
  }
  report<<"danger searches serviced-mask="<<serviced<<'\n';
  check(bounded&&safe,"Crowded grenade responses keep navigation within the shared four-search frame budget and preserve valid physical hulls");
  check(serviced==511,"All nine threatened guards receive a bounded escape search without starvation");
  check(pairs&&distinct,"Nine members of a named squad reserve separate active escape destinations rather than converging on the same close point");
 }
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}

#include "../../../Game.h"
#include "../Schedule.h"
#include "../../engineer/Drones.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
#include <limits>
namespace retro {
namespace {
StateId scheduleDroneKey(int slot,std::string_view field){auto text="drone/slot/"+std::to_string(slot)+"/"+std::string(field);StateId key=2166136261u;for(unsigned char c:text)key=(key^c)*16777619u;return key|0x80000000u;}
}
bool Game::testGuardSchedule(){
 std::ofstream report("guard-schedule-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](auto edit){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Shared guard scheduling validation";campaign->key=882748;
  auto map=std::make_shared<AuthoredMapData>();map->name="Scheduler encounter room";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("schedule_clock");timer.finishState=stateId("schedule_done");timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{60000,{6.5f,6.5f},0,kPi*.5f}};
  guard.scale=1.6f;guard.health=100;guard.damageState=stateId("schedule_damage/0");guard.deadState=stateId("schedule_dead/0");
  guard.ai.mode=ActorAiMode::Guard;guard.ai.faction=AiFaction::Raider;guard.ai.speed=2;guard.ai.armed=true;guard.ai.attackDamage=1;guard.ai.squad=stateId("schedule_team");guard.ai.patrol={{{6.5f,6.5f},0,6000}};
  map->actorTracks.push_back(guard);edit(*map);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int frames,int rate=60){for(int i=0;i<frames;++i)g.update({},1.f/rate);};
 auto medic=[](auto&map){auto&guard=map.actorTracks[0];guard.ai.role=GuardSquadRole::Medic;auto patient=guard;patient.ai.role=GuardSquadRole::Default;patient.ai.armed=false;
  // Named squads belong to Guard actors. A slow unarmed patient remains
  // physically stable while retaining the same valid squad/faction contract.
  patient.ai.mode=ActorAiMode::Guard;patient.ai.speed=.01f;patient.ai.sightRange=1;
  patient.damageState=stateId("schedule_damage/1");patient.deadState=stateId("schedule_dead/1");for(auto&key:patient.keys)key.position={7.5f,6.5f};patient.ai.patrol={{{7.5f,6.5f},0,60000}};map.actorTracks.push_back(patient);
 };
 auto engineer=[](auto&map){map.actorTracks[0].ai.role=GuardSquadRole::Engineer;map.actorTracks[0].ai.attackRange=12;};
 auto opponent=[](auto&map){auto target=map.actorTracks[0];target.ai={};target.ai.faction=AiFaction::Facility;target.health=5000;
  target.damageState=stateId("schedule_opponent_damage");target.deadState=stateId("schedule_opponent_dead");for(auto&key:target.keys)key.position={12.5f,6.5f};map.actorTracks.push_back(target);
 };
 auto safePose=[](Game&g,size_t actor){const auto&npc=g.npcActors()[actor];auto p=npc.pose.position;return p.x>=.35f&&p.x<=23.65f&&p.y>=.35f&&p.y<=23.65f&&std::fabs(npc.pose.z)<.001f&&g.friendlyWorld(npc.level).fits(p.x,p.y,npc.pose.z,npc.track().scale);};
 auto schedule=[](Game&g,size_t actor=0){return GuardSchedule(g.state(g.friendlyKey(actor,"schedule")));};
 auto task=[](Game&g,size_t actor=0){return GuardTask(g.state(g.friendlyKey(actor,"task")));};
 {auto g=scene(medic);g.setState(g.friendlyKey(0,"schedule"),int(GuardSchedule::Medic));g.setState(g.friendlyKey(0,"task_status"),int(GuardTaskStatus::Running));
  bool closed=true;for(int step:{0,1}){g.setState(g.friendlyKey(0,"schedule_step"),step);g.setState(g.friendlyKey(0,"task"),int(step?GuardTask::Move:GuardTask::Prepare));
   for(auto work:{GuardTask::Fire,GuardTask::Bash,GuardTask::Reload,GuardTask::Throw,GuardTask::Treat,GuardTask::Deploy,GuardTask::Repair})closed&=!g.guardTaskAllows(0,work);}
  check(closed,"Scheduler preparation and movement phases deny every work consumer before the actor owns a running work task");
  g.setState(g.friendlyKey(0,"schedule_step"),2);g.setState(g.friendlyKey(0,"task"),int(GuardTask::Treat));
  check(g.guardTaskAllows(0,GuardTask::Treat)&&!g.guardTaskAllows(0,GuardTask::Fire)&&!g.guardTaskAllows(0,GuardTask::Deploy)&&!g.guardTaskAllows(0,GuardTask::Repair),"A running treatment phase permits exactly its owned work consumer");
  g.setState(g.friendlyKey(0,"task_status"),int(GuardTaskStatus::Failed));check(!g.guardTaskAllows(0,GuardTask::Treat),"A failed work task cannot commit its pending treatment event");
 }
 for(int rate:{60,120}){
  auto g=scene([&](auto&map){medic(map);map.definition.playerStart={10.5f,6.5f};});g.setState(stateId("schedule_damage/1"),50);
  bool working=false,exclusive=true,owned=true;int frames=0;
  while(frames++<rate*8&&g.state(g.friendlyKey(0,"support_ms"))<2300){tick(g,1,rate);
   if(g.state(g.friendlyKey(0,"support_ms"))){working=true;exclusive&=!g.state(g.friendlyKey(0,"weapon_aim"))&&!g.state(g.friendlyKey(0,"weapon_flash"))&&
    !g.state(g.friendlyKey(0,"guard_slot"))&&!g.state(g.friendlyKey(0,"drone_windup"))&&!g.state(g.friendlyKey(0,"grenade_windup"))&&g.player().health==100;
    owned&=schedule(g)==GuardSchedule::Medic&&task(g)==GuardTask::Treat&&g.state(g.friendlyKey(0,"schedule_step"))==2&&g.guardTaskAllows(0,GuardTask::Treat)&&g.state(g.friendlyKey(0,"task_subject"))==2;}}
  check(working&&exclusive&&g.state("schedule_damage/1")==50&&!g.state(g.friendlyKey(0,"support_used")),"One live Medic treatment owns the actor's work before completion and excludes shooting, assembly and grenade release at 60/120 Hz");
  check(working&&owned,"Real treatment is executed by the shared scheduler's running Medic work phase with its distinct patient subject");
  int rounds=g.state(g.friendlyKey(0,"weapon_rounds"));g.spawnTacticalGrenade({6.5f,5.8f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1,rate);
  check(g.state(g.friendlyKey(0,"guard_danger_ms"))>0&&!g.state(g.friendlyKey(0,"support_ms"))&&g.state("schedule_damage/1")==50&&
   !g.state(g.friendlyKey(0,"support_used"))&&g.state(g.friendlyKey(0,"weapon_rounds"))==rounds,"Danger preempts nearly completed support work without replaying its heal or spending ammunition");
  check(schedule(g)==GuardSchedule::Danger&&task(g)==GuardTask::Escape&&!g.guardTaskAllows(0,GuardTask::Treat)&&!g.guardTaskAllows(0,GuardTask::Fire),"The escape schedule becomes the actor's sole owner and denies old work consumers immediately");
 }
 for(int rate:{60,120}){
  auto g=scene(medic);g.setState(stateId("schedule_damage/1"),50);int frame=0;while(frame++<rate*6&&g.state(g.friendlyKey(0,"support_ms"))<1000)tick(g,1,rate);
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());bool same=decoded;
  for(const char*f:{"schedule","schedule_step","task","task_status","task_ms","task_fraction","task_subject","task_work_start","task_goal_x","task_goal_y","task_goal_z","task_enemy_kind","task_enemy_level","task_enemy_index"})
   same&=g.state(g.friendlyKey(0,f))==restored.state(restored.friendlyKey(0,f));
  check(same&&schedule(g)==GuardSchedule::Medic&&task(g)==GuardTask::Treat,"Saving during actual work preserves scheduler ownership, phase, subject, clocks and resource commitment");
  tick(g,rate*4,rate);if(decoded)tick(restored,rate*4,rate);
  check(decoded&&g.state("schedule_damage/1")==20&&restored.state("schedule_damage/1")==20&&g.state(g.friendlyKey(0,"support_used"))==1&&restored.state(restored.friendlyKey(0,"support_used"))==1,
   "Saved and uninterrupted treatment schedules finish the same single heal and spend exactly one charge");
 }
 {auto g=scene(medic);g.setState(stateId("schedule_damage/1"),50);g.setState(g.friendlyKey(0,"support_target"),2);g.setState(g.friendlyKey(0,"support_ms"),1000);
  // Missing state entries model older saves: the mechanical action/resource
  // fields survive, while the new scheduler has never claimed an ownership phase.
  for(const char*f:{"schedule","schedule_step","task","task_status","task_ms","task_fraction","task_stall","task_progress","task_retry","task_failures","task_reason","task_goal_x","task_goal_y","task_goal_z","task_enemy_kind","task_enemy_level","task_enemy_index","task_subject","task_work_start","task_conditions","task_transitions"}){
   auto key=g.friendlyKey(0,f);g.m_states.erase(std::remove_if(g.m_states.begin(),g.m_states.end(),[&](const auto&entry){return entry.id==key;}),g.m_states.end());}
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());check(decoded,"A valid older save without scheduler keys can restore its existing partial mechanical treatment");
  if(decoded){tick(restored,4);check(schedule(restored)==GuardSchedule::Medic&&task(restored)==GuardTask::Treat&&restored.state(restored.friendlyKey(0,"support_ms"))>=1000&&
   restored.state("schedule_damage/1")==50&&!restored.state(restored.friendlyKey(0,"support_used")),"Legacy adoption claims the existing treatment phase without resetting its work or replaying a completion");
   tick(restored,240);check(restored.state("schedule_damage/1")==20&&restored.state(restored.friendlyKey(0,"support_used"))==1,"Adopted legacy treatment still commits one finite heal rather than duplicating the old progress");}
 }
 for(int rate:{60,120}){
  auto g=scene([&](auto&map){medic(map);for(auto&key:map.actorTracks[1].keys)key.position={10.5f,6.5f};map.structures.push_back({8,0,8.2f,24,0,1});});g.setState(stateId("schedule_damage/1"),50);
  bool failed=false,recovery=false,safe=true,released=false,bounded=true;
  for(int frame=0;frame<rate*10;++frame){tick(g,1,rate);failed|=g.state(g.friendlyKey(0,"task_failures"))>0;recovery|=schedule(g)==GuardSchedule::Recovery;
   if(schedule(g)==GuardSchedule::Recovery)released|=!g.state(g.friendlyKey(0,"support_target"))&&!g.state(g.friendlyKey(0,"guard_slot"))&&!g.guardTaskAllows(0,GuardTask::Treat);
   safe&=safePose(g,0)&&g.npcActors()[0].pose.position.x<8;bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;}
  report<<"blocked Medic Hz="<<rate<<" failures="<<g.state(g.friendlyKey(0,"task_failures"))<<" x="<<g.npcActors()[0].pose.position.x<<'\n';
  check(failed&&recovery&&released,"A physically blocked support approach fails into a bounded recovery schedule and releases its patient and firing reservations");
  check(safe&&bounded&&g.state("schedule_damage/1")==50&&!g.state(g.friendlyKey(0,"support_used")),"Failed support movement preserves collision, search budgets and finite work resources instead of healing through a wall");
 }
 {auto g=scene([&](auto&map){medic(map);map.definition.playerStart={10.5f,6.5f};});g.setState(stateId("schedule_damage/1"),50);int frame=0;while(frame++<360&&g.state(g.friendlyKey(0,"support_ms"))<500)tick(g,1);
  g.damageFriendly(0,5,g.player().pos,{ShotKind::Player,0,0});tick(g,1);
  check((schedule(g)==GuardSchedule::Cover||schedule(g)==GuardSchedule::Recovery)&&!g.guardTaskAllows(0,GuardTask::Treat)&&!g.state(g.friendlyKey(0,"support_ms"))&&
   g.state("schedule_damage/1")==50&&!g.state(g.friendlyKey(0,"support_used")),"Fresh incoming damage interrupts role work through defensive scheduling before treatment can commit");
 }
 {auto g=scene(opponent);int frame=0;while(frame++<600&&!g.state(g.friendlyKey(0,"weapon_flash")))tick(g,1);
  int rounds=g.state(g.friendlyKey(0,"weapon_rounds")),damage=g.state("schedule_opponent_damage");check(rounds==5&&damage>0&&schedule(g)==GuardSchedule::Combat,
   "An ordinary scheduler-owned attack consumes one real round and damages the first-hit target");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());tick(g,1);if(decoded)tick(restored,1);
  check(decoded&&g.state(g.friendlyKey(0,"weapon_rounds"))==rounds&&restored.state(restored.friendlyKey(0,"weapon_rounds"))==rounds&&
   g.state("schedule_opponent_damage")==damage&&restored.state("schedule_opponent_damage")==damage,"Restoring a consumed fire event never replays its damage or ammunition spend on the next schedule tick");
 }
 for(int aim:{800,1500}){
  auto g=scene([&](auto&map){map.definition.playerStart={12.5f,6.5f};map.actorTracks[0].ai.aimMs=aim;map.actorTracks[0].ai.grenades=2;
   auto protectedBody=map.actorTracks[0];protectedBody.ai={};protectedBody.ai.faction=AiFaction::Neutral;protectedBody.health=0;
   protectedBody.damageState=stateId("schedule_grenade_bystander_damage");protectedBody.deadState=stateId("schedule_grenade_bystander_dead");
   for(auto&key:protectedBody.keys)key.position={12.5f,7.5f};map.actorTracks.push_back(protectedBody);
  });bool recovered=false,fireOwned=false,finite=true;int frame=0;
  while(frame++<60*12&&g.player().health==100){tick(g,1);recovered|=schedule(g)==GuardSchedule::Recovery&&g.state(g.friendlyKey(0,"task_failures"))>0;
   fireOwned|=task(g)==GuardTask::Fire&&g.state(g.friendlyKey(0,"weapon_aim"))>0;finite&=!g.state(g.friendlyKey(0,"grenade_used"))&&g.tacticalGrenades().empty();}
  report<<"unsafe grenade landing aim-ms="<<aim<<" first-fire-frame="<<frame<<" failures="<<g.state(g.friendlyKey(0,"task_failures"))<<'\n';
  check(recovered&&fireOwned&&finite&&g.player().health<100&&g.state("schedule_grenade_bystander_damage")==0,
   "A real unsafe grenade landing fails into recovery, then preserves long firearm aim ownership until an actual safe shot instead of retry-starving combat");
 }
 {auto g=scene([](auto&map){map.definition.playerStart={12.5f,6.5f};map.actorTracks[0].ai.aimMs=1500;map.actorTracks[0].ai.grenades=2;});bool full=true;
  for(int slot=0;slot<8;++slot)full&=g.spawnTacticalGrenade({18.5f,2.5f,.1f},{0,0,0},{ShotKind::Player,0,0},0,AiFaction::Player,6000);
  check(full&&g.tacticalGrenades().size()==8,"Throw-capacity fixture fills all eight global projectile slots with far unrelated player grenades");
  bool releaseAttempt=false,recovered=false,fireOwned=false,finite=true;int frame=0;
  while(frame++<60*6&&g.player().health==100){tick(g,1);releaseAttempt|=g.state(g.friendlyKey(0,"grenade_windup"))==450;
   recovered|=schedule(g)==GuardSchedule::Recovery&&g.state(g.friendlyKey(0,"task_failures"))>0;
   fireOwned|=task(g)==GuardTask::Fire&&g.state(g.friendlyKey(0,"weapon_aim"))>0;finite&=!g.state(g.friendlyKey(0,"grenade_used"));}
  report<<"full grenade pool first-fire-frame="<<frame<<" failures="<<g.state(g.friendlyKey(0,"task_failures"))<<" active="<<g.tacticalGrenades().size()<<'\n';
  check(releaseAttempt&&recovered&&fireOwned&&finite&&g.player().health<100&&g.tacticalGrenades().size()==8,
   "A full-pool release failure times out even at 450ms windup and reaches an owned firearm shot before capacity clears without spending grenade stock");
 }
 for(int rate:{60,120}){
  auto g=scene([](auto&){});g.setState(g.friendlyKey(0,"weapon_saved"),1);g.setState(g.friendlyKey(0,"weapon_rounds"),0);g.setState(g.friendlyKey(0,"weapon_reloading"),1);g.setState(g.friendlyKey(0,"weapon_reload"),700);
  int events=int(guardActionMask(GuardAction::Reload,1100));g.setState(g.friendlyKey(0,"weapon_reload_events"),events);g.setState(g.friendlyKey(0,"weapon_action"),int(GuardAction::Reload));g.setState(g.friendlyKey(0,"weapon_action_ms"),1100);g.setState(g.friendlyKey(0,"weapon_action_events"),events);
  g.spawnTacticalGrenade({7.2f,6.5f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1,rate);
  check(schedule(g)==GuardSchedule::Danger&&!g.guardTaskAllows(0,GuardTask::Reload)&&g.state(g.friendlyKey(0,"weapon_reload"))==700&&g.state(g.friendlyKey(0,"weapon_reload_events"))==events,
   "Danger owns the actor while preserving a committed reload's consumed events and remaining mechanical time");
  tick(g,rate*8,rate);check(g.state(g.friendlyKey(0,"weapon_rounds"))==6&&!g.state(g.friendlyKey(0,"weapon_reloading"))&&!g.state(g.friendlyKey(0,"weapon_reload")),
   "After escape and bounded recovery the committed reload resumes through scheduling and refills exactly one finite tube");
 }
 {auto g=scene([](auto&map){map.definition.playerStart={7.4f,6.5f};});AiTarget target;target.kind=ShotKind::Player;target.level=0;target.index=0;target.position=g.player().pos;
  target.feet=g.player().z;target.height=g.player().hullHeight();target.eye=g.player().eye;target.faction=AiFaction::Player;g.rememberGuardTarget(0,target);
  g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Alert));for(const char*f:{"guard_confirmed","guard_seen","guard_memory"})g.setState(g.friendlyKey(0,f),1);
  g.setState(g.friendlyKey(0,"weapon_target_kind"),int(ShotKind::Player));g.setState(g.friendlyKey(0,"weapon_target_level"),1);g.setState(g.friendlyKey(0,"weapon_target_index"),1);
  g.setState(g.friendlyKey(0,"weapon_saved"),1);g.setState(g.friendlyKey(0,"weapon_rounds"),0);g.setState(g.friendlyKey(0,"weapon_reloading"),1);g.setState(g.friendlyKey(0,"weapon_reload"),1);
  g.setState(g.friendlyKey(0,"weapon_reload_events"),0);g.setState(g.friendlyKey(0,"weapon_raise"),1000);g.setState(g.friendlyKey(0,"weapon_action"),int(GuardAction::Bash));
  g.setState(g.friendlyKey(0,"weapon_action_ms"),100);g.setState(g.friendlyKey(0,"weapon_action_events"),GuardActionStart);g.setState(g.friendlyKey(0,"weapon_bash"),100);
  g.setState(g.friendlyKey(0,"schedule"),int(GuardSchedule::Combat));g.setState(g.friendlyKey(0,"schedule_step"),2);g.setState(g.friendlyKey(0,"task"),int(GuardTask::Bash));g.setState(g.friendlyKey(0,"task_status"),int(GuardTaskStatus::Running));
  g.m_sounds.clear();g.updateGuardCombat(1.f/60);
  check(g.guardTaskAllows(0,GuardTask::Bash)&&!g.guardTaskAllows(0,GuardTask::Reload)&&g.state(g.friendlyKey(0,"weapon_bash"))>100&&
   g.state(g.friendlyKey(0,"weapon_reload"))==1&&g.state(g.friendlyKey(0,"weapon_reload_events"))==0&&g.state(g.friendlyKey(0,"weapon_rounds"))==0&&g.player().health==100,
   "An owned pre-impact Bash pauses a one-millisecond legacy reload before both refill and zero-mask adoption");
  g.setState(g.friendlyKey(0,"task"),int(GuardTask::Reload));g.setState(g.friendlyKey(0,"weapon_bash"),0);g.setState(g.friendlyKey(0,"weapon_action"),int(GuardAction::Reload));
  g.setState(g.friendlyKey(0,"weapon_action_ms"),1799);g.setState(g.friendlyKey(0,"weapon_action_events"),0);g.m_sounds.clear();g.updateGuardCombat(1.f/60);
  bool replay=std::any_of(g.sounds().begin(),g.sounds().end(),[](const auto&sound){return sound.sound==Sound::Pump||sound.sound==Sound::Shot;});g.updateGuardCombat(1.f/60);
  check(!replay&&g.state(g.friendlyKey(0,"weapon_rounds"))==6&&!g.state(g.friendlyKey(0,"weapon_reload"))&&!g.state(g.friendlyKey(0,"weapon_reloading")),
   "Returning ownership to Reload adopts the consumed legacy phase and fills once without replaying chamber, pump or attack events");
 }
 for(int rate:{60,120}){
  auto g=scene([](auto&map){map.definition.playerStart={7.4f,6.5f};});int recognition=0;while(recognition++<rate*2&&!g.state(g.friendlyKey(0,"guard_confirmed")))tick(g,1,rate);
  check(g.state(g.friendlyKey(0,"guard_confirmed"))&&g.player().health==100,"Live contact-defense fixture recognizes its real close opponent before an attack event");
  g.setState(g.friendlyKey(0,"weapon_saved"),1);g.setState(g.friendlyKey(0,"weapon_rounds"),0);g.setState(g.friendlyKey(0,"weapon_reloading"),1);g.setState(g.friendlyKey(0,"weapon_reload"),1);
  g.setState(g.friendlyKey(0,"weapon_reload_events"),0);auto start=g.npcActors()[0].pose.position;bool owned=false,paused=true;int frame=0;
  while(frame++<rate*6&&g.player().health==100){tick(g,1,rate);owned|=task(g)==GuardTask::Bash&&g.state(g.friendlyKey(0,"schedule_step"))==2;
   paused&=g.state(g.friendlyKey(0,"weapon_reload"))==1&&!g.state(g.friendlyKey(0,"weapon_reload_events"))&&!g.state(g.friendlyKey(0,"weapon_rounds"))&&length(g.npcActors()[0].pose.position-start)<.03f;}
  check(owned&&paused&&g.player().health==88&&g.state(g.friendlyKey(0,"weapon_bash_pose"))>0,
   "Actual intent scheduling lets contact Bash preempt cover movement and a near-complete empty reload while consuming exactly one physical strike at 60/120 Hz");
  int health=int(g.player().health);Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());tick(g,1,rate);if(decoded)tick(restored,1,rate);
  check(decoded&&int(g.player().health)==health&&int(restored.player().health)==health,"Saving immediately after scheduler-owned Bash preserves its consumed impact and never replays damage on the next tick");
 }
 {auto g=scene([&](auto&map){engineer(map);opponent(map);});bool deployOwner=false,exclusive=true;int frame=0;
  while(frame++<1200&&g.state(g.friendlyKey(0,"drone_used"))<2){tick(g,1);if(g.state(g.friendlyKey(0,"drone_windup"))){deployOwner=true;
   exclusive&=schedule(g)==GuardSchedule::Engineer&&task(g)==GuardTask::Deploy&&g.guardTaskAllows(0,GuardTask::Deploy)&&!g.guardTaskAllows(0,GuardTask::Fire)&&!g.guardTaskAllows(0,GuardTask::Treat)&&!g.state(g.friendlyKey(0,"support_ms"));}}
  check(deployOwner&&exclusive&&g.engineerDrones().size()>=2,"Actual multi-drone terminal work is owned by Engineer deployment scheduling and excludes incompatible work consumers");
 }
 {auto g=scene([](auto&map){map.actorTracks[0].ai.enableState=stateId("schedule_enabled");});g.setState(stateId("schedule_enabled"),1);tick(g,4);g.setState(stateId("schedule_enabled"),0);tick(g,1);
  Game restored(g.m_customCampaign);check(schedule(g)==GuardSchedule::None&&task(g)==GuardTask::None&&restored.decodeSave(g.encodeSave())&&schedule(restored)==GuardSchedule::None,
   "Disabling an actor releases scheduler ownership and preserves a decodable inactive save");
 }
 {auto g=scene(medic);g.setState(stateId("schedule_damage/1"),50);tick(g,30);int used=g.state(g.friendlyKey(0,"support_used"));g.damageFriendly(0,200,g.player().pos,{ShotKind::Player,0,0});tick(g,1);
  Game restored(g.m_customCampaign);check(g.npcActors()[0].pose.clip==4&&schedule(g)==GuardSchedule::None&&task(g)==GuardTask::None&&!g.state(g.friendlyKey(0,"support_target"))&&
   g.state(g.friendlyKey(0,"support_used"))==used&&restored.decodeSave(g.encodeSave()),"A work-owning actor's death releases its subject and scheduler without committing another resource event or invalidating the corpse save");
 }
 for(const char*f:{"schedule","schedule_step","task","task_status","task_ms","task_fraction","task_stall","task_progress","task_retry","task_failures","task_reason","task_goal_x","task_goal_y","task_goal_z","task_enemy_kind","task_enemy_level","task_enemy_index","task_subject","task_work_start","task_conditions","task_transitions"}){
  auto g=scene([](auto&){});g.setState(g.friendlyKey(0,f),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),
   "Malformed scheduler identity, phase, clock, reason, goal, subject, condition or commitment state is rejected safely");
 }
 {auto g=scene([](auto&map){map.structures.push_back({8,0,8.2f,24,0,4});auto prototype=map.actorTracks[0];prototype.ai.armed=false;map.actorTracks.clear();
   for(int actor=0;actor<9;++actor){auto guard=prototype;Vec2 start{7.3f,3.5f+actor*.75f};for(auto&key:guard.keys)key.position=start;guard.ai.patrol={{{10.5f,start.y},0,0}};
    guard.damageState=stateId("schedule_crowd_damage/"+std::to_string(actor));guard.deadState=stateId("schedule_crowd_dead/"+std::to_string(actor));map.actorTracks.push_back(guard);}});
  unsigned serviced=0;bool bounded=true,safe=true;for(int frame=0;frame<600;++frame){tick(g,1);bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;
   if(frame==0){bool uncharged=true;for(size_t actor=4;actor<9;++actor)uncharged&=!g.state(g.friendlyKey(actor,"guard_patrol_wait"))&&!g.state(g.friendlyKey(actor,"task_stall"));check(uncharged,"A denied first navigation search advances neither patrol nor task failure clocks");}
   for(size_t actor=0;actor<9;++actor){serviced|=g.state(g.friendlyKey(actor,"task_failures"))?1u<<actor:0;safe&=safePose(g,actor)&&g.npcActors()[actor].pose.position.x<8;}}
  report<<"schedule blocked searches failures-mask="<<serviced<<'\n';check(bounded&&safe,"Crowded scheduled movement remains within the shared four-search budget and never crosses a sealed wall");
  check(serviced==511,"All nine budget-limited movement tasks eventually receive a real attempt and bounded failure recovery without starvation");
 }
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}

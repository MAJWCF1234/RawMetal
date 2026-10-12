#include "../../../Game.h"
#include "../ScheduleTables.h"
#include "../../../../world/CustomCampaign.h"
#include <array>
#include <fstream>
#include <limits>
namespace retro {
bool Game::testGuardTable(){
 std::ofstream report("guard-table-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](auto edit){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Declarative guard scheduling validation";campaign->key=882750;
  auto map=std::make_shared<AuthoredMapData>();map->name="Schedule table room";map->definition={{0,0},{2.5f,20.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData floor;for(auto&row:floor.rows)row=std::string(24,'.');map->layers.push_back(floor);
  TimedSequence timer;timer.timerState=stateId("table_clock");timer.finishState=stateId("table_done");timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{60000,{6.5f,6.5f},0,kPi*.5f}};guard.scale=1.6f;guard.health=100;
  guard.damageState=stateId("table_guard_damage");guard.deadState=stateId("table_guard_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.faction=AiFaction::Raider;
  guard.ai.armed=true;guard.ai.speed=.01f;guard.ai.attackRange=12;guard.ai.sightRange=18;guard.ai.attackDamage=1;guard.ai.aimMs=1500;guard.ai.recognitionMs=100;
  guard.ai.squad=stateId("table_team");guard.ai.patrol={{{6.5f,6.5f},0,60000}};map->actorTracks.push_back(guard);
  edit(*map);campaign->maps.push_back(map);return Game(campaign);
 };
 auto opponent=[](auto&map,Vec2 position=Vec2{12.5f,6.5f}){auto target=map.actorTracks[0];target.ai={};target.ai.faction=AiFaction::Facility;target.health=5000;
  target.damageState=stateId("table_target_damage");target.deadState=stateId("table_target_dead");for(auto&key:target.keys)key.position=position;map.actorTracks.push_back(target);};
 auto medic=[](auto&map){map.actorTracks[0].ai.role=GuardSquadRole::Medic;auto patient=map.actorTracks[0];patient.ai.role=GuardSquadRole::Default;patient.ai.armed=false;
  patient.ai.sightRange=1;patient.damageState=stateId("table_patient_damage");patient.deadState=stateId("table_patient_dead");for(auto&key:patient.keys)key.position={7.5f,6.5f};
  patient.ai.patrol={{{7.5f,6.5f},0,60000}};map.actorTracks.push_back(patient);};
 auto tick=[](Game&g,int frames,int rate=60){for(int frame=0;frame<frames;++frame)g.update({},1.f/rate);};
 auto schedule=[](Game&g,size_t actor=0){return GuardSchedule(g.state(g.friendlyKey(actor,"schedule")));};
 auto task=[](Game&g,size_t actor=0){return GuardTask(g.state(g.friendlyKey(actor,"task")));};
 auto conditions=[](Game&g,size_t actor=0){return unsigned(g.state(g.friendlyKey(actor,"task_conditions")));};
 // This executes the real sensory/physical/action pipeline with a captured
 // sound event. Game::update clears externally queued sounds at frame start.
 auto soundTick=[](Game&g,Sound sound,Vec2 origin,int rate=60){g.m_sounds.clear();g.m_guardShots.clear();g.m_npcNavBudget=4;
  g.m_sounds.push_back({sound,origin,1,1,true,17,1.f});g.updateFriendlyActors(1.f/rate,true);g.updateGuardCombat(1.f/rate);g.updateEngineerDrones(1.f/rate);
  for(size_t actor=0;actor<g.npcActors().size();++actor)g.finishGuardTask(actor);};
 auto awaitTreat=[&](Game&g,int rate,int minimum=600){for(int frame=0;frame<rate*8;++frame){tick(g,1,rate);if(g.state(g.friendlyKey(0,"support_ms"))>=minimum)return true;}return false;};
 struct SoundCase {Sound sound;unsigned category;};
 constexpr std::array<SoundCase,3> sounds={{{Sound::Door,ai::SoundWorld},{Sound::Shot,ai::SoundWeapon},{Sound::JunkMetal,ai::SoundImpact}}};

 {bool valid=validGuardScheduleCatalog()&&guardScheduleCatalog.size()==11,distinct=true;
  for(size_t id=0;id<guardScheduleCatalog.size();++id){const auto&row=guardScheduleDefinition(GuardSchedule(id));valid&=row.schedule.id==int(id)&&ai::validScheduleDefinition(row.schedule)&&*row.schedule.name;
   for(size_t prior=0;prior<id;++prior)distinct&=std::string_view(row.schedule.name)!=guardScheduleCatalog[prior].schedule.name;
   if(id&&id!=size_t(GuardSchedule::Danger)&&id!=size_t(GuardSchedule::Recovery))valid&=row.schedule.tasks.size()==3&&row.schedule.tasks[0].operation==ai::TaskOperation::Prepare&&row.schedule.tasks[0].phase==0&&
    row.schedule.tasks[1].operation==ai::TaskOperation::Move&&row.schedule.tasks[1].phase==1&&row.schedule.tasks[2].operation==ai::TaskOperation::ExecuteWork&&row.schedule.tasks[2].phase==2&&row.schedule.failureSchedule==int(GuardSchedule::Recovery);
  }
  check(valid&&distinct,"All eleven named schedule definitions expose valid distinct task lists, compatible phases and explicit ordinary failure destinations");
  const auto&escape=guardScheduleDefinition(GuardSchedule::Danger);const auto&recovery=guardScheduleDefinition(GuardSchedule::Recovery);
  check(escape.schedule.tasks.size()==1&&escape.schedule.tasks[0].operation==ai::TaskOperation::Escape&&escape.work==guardTaskBit(GuardTask::Escape)&&
   recovery.schedule.tasks.size()==1&&recovery.schedule.tasks[0].operation==ai::TaskOperation::Wait&&recovery.schedule.tasks[0].argument==700&&recovery.schedule.failureSchedule==int(GuardSchedule::Patrol),
   "Danger and failure recovery have genuinely different declarative execution lists and Recovery declares its seven-hundred-millisecond wait");
  bool masks=true;for(auto role:{GuardSchedule::Medic,GuardSchedule::Engineer,GuardSchedule::Patient}){const auto&row=guardScheduleDefinition(role).schedule;
   masks&=ai::acceptedScheduleSounds(row,guardAllSounds)==ai::SoundDanger&&ai::scheduleInterrupts(row,GuardInDanger|GuardHeavyDamage|GuardEnemyDead);}
  for(auto ordinary:{GuardSchedule::Patrol,GuardSchedule::Investigate,GuardSchedule::Search})masks&=ai::acceptedScheduleSounds(guardScheduleDefinition(ordinary).schedule,guardAllSounds)==guardAllSounds;
  for(auto armed:{GuardSchedule::Combat,GuardSchedule::Cover})masks&=ai::acceptedScheduleSounds(guardScheduleDefinition(armed).schedule,guardAllSounds)==(ai::SoundWeapon|ai::SoundDanger);
  check(masks,"The catalog gives ordinary investigation, armed combat and support work distinct sensory interests with danger retained for every support role");
 }
 struct Adapter {
  std::vector<ai::TaskOperation> starts,runs;bool failMove=false,holdMove=false;int charges=0;
  void start(const ai::ScheduleTask&t,ai::SchedulerState&){starts.push_back(t.operation);}
  ai::TaskStatus run(const ai::ScheduleTask&t,ai::SchedulerState&s){runs.push_back(t.operation);
   if(t.operation==ai::TaskOperation::Move)return failMove?ai::TaskStatus::Failed:holdMove?ai::TaskStatus::Running:ai::TaskStatus::Complete;
   if(t.operation==ai::TaskOperation::Prepare)return ai::TaskStatus::Complete;
   if(t.operation==ai::TaskOperation::Wait)return s.elapsedMs>=t.argument?ai::TaskStatus::Complete:ai::TaskStatus::Running;
   if(t.operation==ai::TaskOperation::Escape)return ai::TaskStatus::Running;
   if(s.elapsedMs<100)return ai::TaskStatus::Running;++charges;return ai::TaskStatus::Complete;
  }
 };
 {Adapter actor;ai::SchedulerState state;const auto&definition=guardScheduleDefinition(GuardSchedule::Medic).schedule;
  auto result=ai::MaintainSchedule(definition,state,actor);check(result.outcome==ai::ScheduleOutcome::Running&&result.transitions==3&&state.cursor==2&&state.status==ai::TaskStatus::Running&&actor.starts.size()==3,
   "The shared table executor starts Prepare, Move and actual work in catalog order within the bounded transition budget");
  state.elapsedMs=60;result=ai::MaintainSchedule(definition,state,actor);bool kept=result.outcome==ai::ScheduleOutcome::Running&&!result.transitions&&actor.starts.size()==3&&state.elapsedMs==60&&!actor.charges;
  state.elapsedMs=100;result=ai::MaintainSchedule(definition,state,actor);check(kept&&result.outcome==ai::ScheduleOutcome::ScheduleDone&&state.cursor==3&&actor.charges==1,
   "Running table work retains its original elapsed time and commits one finite event when its adapter reports completion");
  size_t runs=actor.runs.size();result=ai::MaintainSchedule(definition,state,actor);check(result.outcome==ai::ScheduleOutcome::ScheduleDone&&!result.transitions&&actor.runs.size()==runs&&actor.charges==1,
   "Maintaining a completed table cursor cannot restart its action adapter or replay the consumed work event");
 }
 {Adapter actor;ai::SchedulerState state;const auto&definition=guardScheduleDefinition(GuardSchedule::Combat).schedule;
  auto first=ai::MaintainSchedule(definition,state,actor,1);auto second=ai::MaintainSchedule(definition,state,actor,1);auto third=ai::MaintainSchedule(definition,state,actor,1);
  check(first.outcome==ai::ScheduleOutcome::TransitionLimit&&second.outcome==ai::ScheduleOutcome::TransitionLimit&&third.outcome==ai::ScheduleOutcome::Running&&
   first.transitions==1&&second.transitions==1&&third.transitions==1&&actor.starts.size()==3&&state.cursor==2,"One-transition maintenance resumes the next listed task without skipping or duplicating Prepare, Move or work");
 }
 {constexpr std::array<ai::ScheduleTask,5> longer={{{ai::TaskOperation::Prepare,0,0},{ai::TaskOperation::Move,0,1},{ai::TaskOperation::Prepare,0,0},{ai::TaskOperation::Move,0,1},{ai::TaskOperation::ExecuteWork,0,2}}};
  const ai::ScheduleDefinition definition{12,"Bounded longer actor list",longer,9,0,0};Adapter actor;ai::SchedulerState state;
  auto boundary=ai::MaintainSchedule(definition,state,actor,4);bool deferred=boundary.outcome==ai::ScheduleOutcome::TransitionLimit&&boundary.transitions==4&&state.cursor==4&&state.status==ai::TaskStatus::New&&actor.starts.size()==4;
  auto resumed=ai::MaintainSchedule(definition,state,actor,1);state.elapsedMs=100;auto done=ai::MaintainSchedule(definition,state,actor,1);
  check(deferred&&resumed.outcome==ai::ScheduleOutcome::Running&&resumed.transitions==1&&actor.starts.size()==5&&done.outcome==ai::ScheduleOutcome::ScheduleDone&&actor.charges==1,
   "A genuine longer task list defers its next New task at the transition boundary and starts that work exactly once on the next maintenance pass");
 }
 {Adapter actor;actor.failMove=true;ai::SchedulerState state;const auto&definition=guardScheduleDefinition(GuardSchedule::Investigate).schedule;
  auto result=ai::MaintainSchedule(definition,state,actor);size_t starts=actor.starts.size();auto repeated=ai::MaintainSchedule(definition,state,actor);
  check(result.outcome==ai::ScheduleOutcome::TaskFailed&&result.failureSchedule==int(GuardSchedule::Recovery)&&state.cursor==1&&actor.charges==0&&
   repeated.outcome==ai::ScheduleOutcome::TaskFailed&&actor.starts.size()==starts,"A listed movement task failure returns the catalog fallback before any work event and repeated maintenance cannot restart failed work");
 }
 {Adapter actor;ai::SchedulerState state;const auto&definition=guardScheduleDefinition(GuardSchedule::Recovery).schedule;ai::MaintainSchedule(definition,state,actor);
  state.elapsedMs=699;auto held=ai::MaintainSchedule(definition,state,actor);state.elapsedMs=700;auto released=ai::MaintainSchedule(definition,state,actor);
  check(held.outcome==ai::ScheduleOutcome::Running&&released.outcome==ai::ScheduleOutcome::ScheduleDone&&actor.starts.size()==1&&released.failureSchedule==int(GuardSchedule::Patrol),
   "The shared executor honors the actual Recovery row's wait argument before releasing its single task");
 }
 for(int invalid:{-1,9}){Adapter actor;ai::SchedulerState state;state.cursor=invalid;auto result=ai::MaintainSchedule(guardScheduleDefinition(GuardSchedule::Combat).schedule,state,actor);
  check(result.outcome==ai::ScheduleOutcome::InvalidState&&actor.starts.empty()&&actor.runs.empty(),"An invalid table cursor fails before an action adapter is called");}

 for(int rate:{60,120})for(auto noise:sounds){auto g=scene([](auto&map){map.actorTracks[0].ai.armed=false;});tick(g,2,rate);Vec2 source{9,6.5f};soundTick(g,noise.sound,source,rate);
  auto goal=Vec2{g.state(g.friendlyKey(0,"task_goal_x"))*.001f,g.state(g.friendlyKey(0,"task_goal_y"))*.001f};
  check((g.state(g.friendlyKey(0,"guard_heard"))&int(noise.category))&&(conditions(g)&GuardHeardSound)&&schedule(g)==GuardSchedule::Investigate&&
   task(g)==GuardTask::Move&&g.state(g.friendlyKey(0,"task_cursor_saved"))&&g.state(g.friendlyKey(0,"task_cursor"))==1&&length(goal-source)<.05f,
   "An actual audible world, weapon or impact event interrupts Patrol and executes the table's Move task toward the captured source at both frame rates");
 }
 {auto g=scene([](auto&map){map.actorTracks[0].ai.armed=false;map.structures.push_back({7.4f,0,7.6f,24,0,4});});tick(g,2);soundTick(g,Sound::JunkMetal,{10.5f,6.5f});
  check(!(conditions(g)&GuardHeardSound)&&!g.state(g.friendlyKey(0,"guard_heard"))&&schedule(g)==GuardSchedule::Patrol,
   "A quiet impact behind a full wall does not fabricate an audible condition or interrupt the patrol task");
 }
 for(int rate:{60,120}){auto g=scene(medic);g.setState(stateId("table_patient_damage"),50);check(awaitTreat(g,rate),"Sound fixture reaches actual scheduler-owned treatment through normal updates");
  bool ignored=true;for(auto noise:sounds){int progress=g.state(g.friendlyKey(0,"support_ms"));auto position=g.npcActors()[0].pose.position;soundTick(g,noise.sound,{9,6.5f},rate);
   ignored&=(g.state(g.friendlyKey(0,"guard_heard"))&int(noise.category))&&!(conditions(g)&GuardHeardSound)&&schedule(g)==GuardSchedule::Medic&&task(g)==GuardTask::Treat&&
    schedule(g,1)==GuardSchedule::Patient&&!(conditions(g,1)&GuardHeardSound)&&
    g.state(g.friendlyKey(0,"support_ms"))>progress&&length(g.npcActors()[0].pose.position-position)<.02f&&!g.state(g.friendlyKey(0,"support_used"));}
  check(ignored,"The working Medic and its holding Patient hear ordinary sounds but their table filters preserve the same unspent treatment and reciprocal claim");
  g.spawnTacticalGrenade({6.5f,5.8f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1,rate);
  check(schedule(g)==GuardSchedule::Danger&&task(g)==GuardTask::Escape&&g.state(g.friendlyKey(0,"task_cursor"))==0&&(conditions(g)&GuardInDanger)&&
   !g.guardTaskAllows(0,GuardTask::Treat)&&!g.state(g.friendlyKey(0,"support_ms"))&&!g.state(g.friendlyKey(0,"support_used"))&&g.state("table_patient_damage")==50,
   "A physical exposed grenade bypasses the support sound filter and replaces treatment with the one-task Escape list before spending a charge");
 }
 for(int rate:{60,120}){auto g=scene([&](auto&map){map.actorTracks[0].ai.role=GuardSquadRole::Engineer;opponent(map);});int frame=0;
  while(frame++<rate*8&&g.state(g.friendlyKey(0,"drone_windup"))<200)tick(g,1,rate);bool ready=g.state(g.friendlyKey(0,"drone_windup"))>=200,ignored=ready;
  for(auto noise:sounds){int progress=g.state(g.friendlyKey(0,"drone_windup"));soundTick(g,noise.sound,{8.5f,8.5f},rate);
   ignored&=schedule(g)==GuardSchedule::Engineer&&task(g)==GuardTask::Deploy&&g.state(g.friendlyKey(0,"drone_windup"))>progress&&!(conditions(g)&GuardHeardSound)&&!g.state(g.friendlyKey(0,"drone_used"));}
  check(ignored,"Ordinary audible events cannot reset the Engineer's live deployment adapter or spend stock before its terminal event at either rate");
  auto position=g.npcActors()[0].pose.position;g.spawnTacticalGrenade({position.x,position.y-.7f,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1,rate);
  check(ready&&schedule(g)==GuardSchedule::Danger&&task(g)==GuardTask::Escape&&!g.state(g.friendlyKey(0,"drone_windup"))&&!g.state(g.friendlyKey(0,"drone_used"))&&g.engineerDrones().empty(),
   "The same Engineer support mask always yields assembly to an exposed physical grenade before any deployment stock is committed");
 }
 for(int rate:{60,120}){auto g=scene(medic);g.setState(stateId("table_patient_damage"),50);bool ready=awaitTreat(g,rate,2360);int before=g.state(g.friendlyKey(0,"support_ms"));
  soundTick(g,Sound::LiftCrash,{8.5f,6.5f},rate);
  check(ready&&(g.state(g.friendlyKey(0,"guard_heard"))&ai::SoundDanger)&&(conditions(g)&GuardHeardSound)&&
   g.state(g.friendlyKey(0,"support_ms"))<100&&!g.state(g.friendlyKey(0,"support_used"))&&g.state("table_patient_damage")==50,
   "An actual audible danger event cancels the same near-complete treatment work before its heal boundary even if the chooser keeps that Medic job");
  report<<"audible danger treatment Hz="<<rate<<" before="<<before<<" after="<<g.state(g.friendlyKey(0,"support_ms"))<<'\n';
 }
 for(int rate:{60,120}){auto g=scene([&](auto&map){map.actorTracks[0].ai.role=GuardSquadRole::Engineer;opponent(map);});int frame=0,threshold=1160;
  while(frame++<rate*8&&g.state(g.friendlyKey(0,"drone_windup"))<threshold)tick(g,1,rate);bool ready=g.state(g.friendlyKey(0,"drone_windup"))>=threshold;int before=g.state(g.friendlyKey(0,"drone_windup"));
  soundTick(g,Sound::LiftCrash,{8.5f,6.5f},rate);
  check(ready&&(g.state(g.friendlyKey(0,"guard_heard"))&ai::SoundDanger)&&(conditions(g)&GuardHeardSound)&&g.state(g.friendlyKey(0,"drone_windup"))<100&&
   !g.state(g.friendlyKey(0,"drone_used"))&&g.engineerDrones().empty(),"An actual audible danger event cancels the same near-complete assembly before its deployment event or stock spend at both frame rates");
  report<<"audible danger assembly Hz="<<rate<<" before="<<before<<" after="<<g.state(g.friendlyKey(0,"drone_windup"))<<'\n';
 }
 for(int rate:{60,120}){auto g=scene([&](auto&map){opponent(map,{7.4f,6.5f});});int frame=0,threshold=260;
  while(frame++<rate*5&&g.state(g.friendlyKey(0,"weapon_bash"))<threshold)tick(g,1,rate);bool ready=g.state(g.friendlyKey(0,"weapon_bash"))>=threshold;
  int rounds=g.state(g.friendlyKey(0,"weapon_rounds")),before=g.state(g.friendlyKey(0,"weapon_bash"));soundTick(g,Sound::Shot,{8.5f,6.5f},rate);
  report<<"weapon interruption bash Hz="<<rate<<" ready="<<ready<<" before="<<before<<" after="<<g.state(g.friendlyKey(0,"weapon_bash"))<<" damage="<<g.state("table_target_damage")<<'\n';
  check(ready&&(conditions(g)&GuardHeardSound)&&g.state(g.friendlyKey(0,"weapon_bash"))<100&&!g.state(g.friendlyKey(0,"weapon_bash_pose"))&&
   !g.state("table_target_damage")&&g.state(g.friendlyKey(0,"weapon_rounds"))==rounds,"A real accepted weapon interruption restarts an uncommitted contact strike before its impact while preserving ammunition");
 }

 for(int rate:{60,120}){auto g=scene([&](auto&map){opponent(map);});int frame=0;
  while(frame++<rate*5&&g.state(g.friendlyKey(0,"weapon_aim"))<300)tick(g,1,rate);bool aimed=g.state(g.friendlyKey(0,"weapon_aim"))>=300&&schedule(g)==GuardSchedule::Combat,kept=aimed;
  for(Sound noise:{Sound::Door,Sound::JunkMetal}){int aim=g.state(g.friendlyKey(0,"weapon_aim")),elapsed=g.state(g.friendlyKey(0,"task_ms"));soundTick(g,noise,{9,8.5f},rate);
   kept&=schedule(g)==GuardSchedule::Combat&&task(g)==GuardTask::Fire&&!(conditions(g)&GuardHeardSound)&&g.state(g.friendlyKey(0,"weapon_aim"))>aim&&g.state(g.friendlyKey(0,"task_ms"))>elapsed;}
  check(kept,"Real owned firearm aim continues through audible world and impact events excluded by the Combat table instead of restarting its attack task");
  int aim=g.state(g.friendlyKey(0,"weapon_aim")),elapsed=g.state(g.friendlyKey(0,"task_ms"));soundTick(g,Sound::Shot,{9,8.5f},rate);
  check(aimed&&(conditions(g)&GuardHeardSound)&&(g.state(g.friendlyKey(0,"guard_heard"))&ai::SoundWeapon)&&schedule(g)==GuardSchedule::Combat&&
   g.state(g.friendlyKey(0,"weapon_aim"))<aim&&g.state(g.friendlyKey(0,"task_ms"))<elapsed&&!g.state("table_target_damage"),
   "A distinct audible weapon event actually interrupts Combat's listed running Fire task before its long aim commits a shot");
 }

 for(int rate:{60,120})for(int mode=0;mode<3;++mode){Vec2 position=mode==0?Vec2{12.5f,6.5f}:mode==1?Vec2{7.4f,6.5f}:Vec2{21.5f,6.5f};
  auto g=scene([&](auto&map){opponent(map,position);});tick(g,rate/3,rate);unsigned bits=conditions(g);
  check((bits&GuardHasThreat)&&(bits&GuardSeesThreat)&&!(bits&GuardEnemyOccluded)&&!(bits&GuardEnemyDead)&&
   (mode==0?(bits&GuardCanRangeAttack)&&!(bits&GuardCanMeleeAttack):mode==1?(bits&GuardCanMeleeAttack)&&(bits&GuardCloseThreat):!(bits&GuardCanRangeAttack)&&!(bits&GuardCanMeleeAttack)),
   "Real confirmed targets publish firearm readiness, contact readiness or neither according to physical range instead of treating all visible enemies as attackable");
 }
 {auto g=scene([&](auto&map){opponent(map);auto body=map.actorTracks.back();body.ai.faction=AiFaction::Neutral;body.health=100;body.damageState=stateId("table_neutral_damage");body.deadState=stateId("table_neutral_dead");
   for(auto&key:body.keys)key.position={9.5f,6.5f};map.actorTracks.push_back(body);});tick(g,20);
  check((conditions(g)&GuardSeesThreat)&&!(conditions(g)&GuardCanRangeAttack)&&!g.state("table_neutral_damage")&&!g.state("table_target_damage"),
   "A personally visible enemy behind a neutral body fails the exact barrel-hit readiness condition without firing through the intervening actor");
 }
 {auto g=scene([&](auto&map){opponent(map);map.structures.push_back({8,8,17,8.2f,0,4});auto&target=map.actorTracks[1];target.keys={{0,{12.5f,6.5f},0,0},{1600,{12.5f,6.5f},0,0},{1601,{12.5f,12.5f},0,0},{60000,{12.5f,12.5f},0,0}};});
  tick(g,60);Vec2 lastKnown{g.state(g.friendlyKey(0,"guard_x"))*.001f,g.state(g.friendlyKey(0,"guard_y"))*.001f};tick(g,50);unsigned bits=conditions(g);
  auto remembered=Vec2{g.state(g.friendlyKey(0,"guard_x"))*.001f,g.state(g.friendlyKey(0,"guard_y"))*.001f};
  check((bits&GuardHasThreat)&&(bits&GuardEnemyOccluded)&&!(bits&(GuardEnemyDead|GuardSeesThreat|GuardCanRangeAttack|GuardCanMeleeAttack))&&
   g.state(g.friendlyKey(0,"guard_confirmed"))&&length(remembered-lastKnown)<.1f&&length(remembered-g.m_world.actorPose(1).position)>3,
   "A known live opponent entering real occluding geometry retains only the witnessed location and loses both physical attack conditions without becoming dead");
  // Use the canonical target-damage path: a dead-state toggle alone omits
  // the captured corpse position and the mechanism loader places it at 0,0.
  g.damageGuardTarget(0,6000);tick(g,1);
  check(!(conditions(g)&GuardEnemyDead)&&!g.state(g.friendlyKey(0,"guard_enemy_dead")),"An unseen opponent's hidden death cannot publish a witnessed-death condition or reveal its hidden position");
 }
 {auto g=scene([&](auto&map){opponent(map);});tick(g,20);bool known=g.guardTarget(0).valid();g.damageGuardTarget(0,6000);tick(g,1);
  report<<"witnessed corpse known="<<known<<" bits="<<conditions(g)<<" seen="<<g.state(g.friendlyKey(0,"guard_seen"))<<" dead="<<g.state(g.friendlyKey(0,"guard_enemy_dead"))
   <<" mode="<<int(g.guardState(0))<<" confirmed="<<g.state(g.friendlyKey(0,"guard_confirmed"))<<" target="<<g.state(g.friendlyKey(0,"guard_target_kind"))
   <<" corpse="<<g.m_world.actorPose(1).position.x<<','<<g.m_world.actorPose(1).position.y<<','<<g.m_world.actorPose(1).z<<" clip="<<g.m_world.actorPose(1).clip<<'\n';
  check(known&&(conditions(g)&GuardEnemyDead)&&!(conditions(g)&(GuardHasThreat|GuardEnemyOccluded|GuardCanRangeAttack|GuardCanMeleeAttack))&&!g.guardTarget(0).valid()&&!g.state(g.friendlyKey(0,"guard_confirmed")),
   "A witnessed known opponent corpse produces a distinct dead condition and releases the stale attack reference rather than investigating it as a live occluded enemy");
 }
 for(int amount:{5,20}){auto g=scene(medic);g.setState(stateId("table_patient_damage"),50);check(awaitTreat(g,60,2300),"Fresh-damage fixture reaches the final uncommitted kit interval");
  g.damageFriendly(0,float(amount),{5.5f,6.5f});tick(g,1);unsigned bits=conditions(g);
  check((bits&GuardHurt)&&(amount==5?(bits&GuardLightDamage)&&!(bits&GuardHeavyDamage):(bits&GuardHeavyDamage)&&!(bits&GuardLightDamage))&&
   g.state(g.friendlyKey(0,"guard_hurt_amount"))==amount&&schedule(g)==GuardSchedule::Cover&&!g.state(g.friendlyKey(0,"support_ms"))&&!g.state(g.friendlyKey(0,"support_used"))&&g.state("table_patient_damage")==50,
   "Actual light or heavy fresh damage interrupts near-complete support work once without committing its heal or charge");
  tick(g,1);check(!(conditions(g)&(GuardLightDamage|GuardHeavyDamage))&&!g.state(g.friendlyKey(0,"guard_hurt_amount"))&&(conditions(g)&GuardHurt),
   "Fresh damage classifications clear on the next unchanged-health frame while the existing transient under-fire memory continues");
 }

 {auto g=scene([](auto&map){auto&guard=map.actorTracks[0];guard.ai.armed=false;guard.ai.faction=AiFaction::Neutral;guard.ai.speed=2;for(auto&key:guard.keys)key.position={7.3f,6.5f};guard.ai.patrol={{{10.5f,6.5f},0,0}};map.structures.push_back({8,0,8.2f,24,0,4});});
  bool held=false,bounded=true,safe=true;int frame=0;while(frame++<60*10&&!held){tick(g,1);bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4&&g.state(g.friendlyKey(0,"task_transitions"))<=4;
   safe&=g.npcActors()[0].pose.position.x<8&&g.m_world.fits(g.npcActors()[0].pose.position.x,g.npcActors()[0].pose.position.y,g.npcActors()[0].pose.z,1.6f);held=schedule(g)==GuardSchedule::Recovery;}
  check(held&&safe&&bounded&&g.state(g.friendlyKey(0,"task_reason"))==int(GuardTaskReason::Blocked)&&g.state(g.friendlyKey(0,"task_cursor"))==0&&g.state(g.friendlyKey(0,"schedule_step"))==2&&
   g.state(g.friendlyKey(0,"task_fail_schedule"))==guardScheduleDefinition(GuardSchedule::Recovery).schedule.failureSchedule&&task(g)==GuardTask::Hold,
   "Actual sealed-wall movement fails into the catalog's distinct Recovery cursor and physical Wait phase while remaining inside navigation and hull limits");
  report<<"blocked patrol frame="<<frame<<" held="<<held<<" safe="<<safe<<" bounded="<<bounded<<" schedule="<<int(schedule(g))<<" cursor="<<g.state(g.friendlyKey(0,"task_cursor"))
   <<" phase="<<g.state(g.friendlyKey(0,"schedule_step"))<<" reason="<<g.state(g.friendlyKey(0,"task_reason"))<<" fails="<<g.state(g.friendlyKey(0,"task_failures"))
   <<" fail-dest="<<g.state(g.friendlyKey(0,"task_fail_schedule"))<<" task="<<int(task(g))<<" wait="<<g.state(g.friendlyKey(0,"guard_patrol_wait"))<<" stall="<<g.state(g.friendlyKey(0,"task_stall"))
   <<" position="<<g.npcActors()[0].pose.position.x<<','<<g.npcActors()[0].pose.position.y<<'\n';
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());tick(g,48);if(decoded)tick(restored,48);
  check(decoded&&schedule(g)!=GuardSchedule::Recovery&&schedule(restored)!=GuardSchedule::Recovery&&g.state(g.friendlyKey(0,"task_failures"))==1&&
   restored.state(restored.friendlyKey(0,"task_failures"))==1,"A saved live Recovery wait resumes its remaining clock and releases once without replaying the failed movement event");
  report<<"recovery save decoded="<<decoded<<" final schedule="<<int(schedule(g))<<','<<int(schedule(restored))<<" fails="<<g.state(g.friendlyKey(0,"task_failures"))<<','<<restored.state(restored.friendlyKey(0,"task_failures"))<<'\n';
 }
 {auto g=scene(medic);g.setState(stateId("table_patient_damage"),50);check(awaitTreat(g,60,1200),"Cursor save fixture reaches actual treatment halfway through its finite event");
  Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());bool same=decoded;
  for(const char*f:{"task_cursor","task_cursor_saved","task_fail_schedule","task","schedule_step","task_status","task_ms","task_work_start","support_ms","support_used"})same&=g.state(g.friendlyKey(0,f))==restored.state(restored.friendlyKey(0,f));
  check(same&&g.state(g.friendlyKey(0,"task_cursor"))==2&&g.state(g.friendlyKey(0,"task_cursor_saved"))==1&&
   g.state(g.friendlyKey(0,"task_fail_schedule"))==guardScheduleDefinition(GuardSchedule::Medic).schedule.failureSchedule,"Saving real role work preserves its declarative cursor, failure destination and original resource baseline");
  for(const char*f:{"task_cursor","task_cursor_saved","task_fail_schedule"})g.setState(g.friendlyKey(0,f),0);
  Game legacy(g.m_customCampaign);bool adopted=legacy.decodeSave(g.encodeSave());if(adopted)tick(legacy,60*12);if(decoded)tick(restored,60*12);
  check(adopted&&decoded&&legacy.state("table_patient_damage")==20&&restored.state("table_patient_damage")==20&&legacy.state(legacy.friendlyKey(0,"support_used"))==1&&
   restored.state(restored.friendlyKey(0,"support_used"))==1,"An older save with absent cursor and failure metadata adopts its existing treatment phase and completes exactly one identical heal without replay");
 }
 {auto g=scene([&](auto&map){opponent(map);});int frame=0;while(frame++<60*8&&!g.state("table_target_damage"))tick(g,1);
  int rounds=g.state(g.friendlyKey(0,"weapon_rounds")),damage=g.state("table_target_damage");bool completed=damage>0&&g.state(g.friendlyKey(0,"task_status"))==int(GuardTaskStatus::Complete);
  Game atEvent(g.m_customCampaign);bool eventSave=atEvent.decodeSave(g.encodeSave());tick(g,1);if(eventSave)tick(atEvent,1);
  Game atRestart(g.m_customCampaign);bool restartSave=atRestart.decodeSave(g.encodeSave());
  check(completed&&eventSave&&restartSave&&g.state(g.friendlyKey(0,"task_cursor"))==2&&g.state(g.friendlyKey(0,"schedule_step"))==2&&g.state(g.friendlyKey(0,"task_status"))==int(GuardTaskStatus::Running)&&
   g.guardTaskAllows(0,GuardTask::Fire)&&g.state(g.friendlyKey(0,"task_transitions"))==4&&g.state(g.friendlyKey(0,"task_work_start"))==rounds&&g.state(g.friendlyKey(0,"weapon_rounds"))==rounds&&
   atEvent.state(atEvent.friendlyKey(0,"weapon_rounds"))==rounds&&g.state("table_target_damage")==damage&&atEvent.state("table_target_damage")==damage,
   "A live completed firearm event and its four-transition list restart both save with matching cursor/phase and a new baseline without replaying the spent round or damage");
 }
 for(const char*f:{"task_cursor","task_cursor_saved","task_fail_schedule","guard_heard","guard_hurt_amount","guard_enemy_dead","task_conditions"}){auto g=scene([](auto&){});tick(g,2);
  g.setState(g.friendlyKey(0,f),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Malformed table identity or expanded sensory fields are rejected before saved work can execute");}
 {auto g=scene(medic);g.setState(stateId("table_patient_damage"),50);awaitTreat(g,60);g.setState(g.friendlyKey(0,"task_cursor"),0);g.setState(g.friendlyKey(0,"task_cursor_saved"),1);
  Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"A syntactically bounded saved cursor that disagrees with its live treatment phase is rejected rather than rerunning Prepare or dropping the resource baseline");}
 {auto g=scene(medic);g.setState(stateId("table_patient_damage"),50);awaitTreat(g,60);g.setState(g.friendlyKey(0,"task_cursor"),3);g.setState(g.friendlyKey(0,"task_status"),int(GuardTaskStatus::Complete));
  g.setState(g.friendlyKey(0,"schedule_step"),0);g.setState(g.friendlyKey(0,"task"),int(GuardTask::Prepare));Game restored(g.m_customCampaign);
  check(!restored.decodeSave(g.encodeSave()),"A terminal completed cursor must agree with the final listed task instead of claiming that Prepare completed the whole treatment list");}
 for(const char*partial:{"task_cursor","task_fail_schedule"}){auto g=scene(medic);g.setState(stateId("table_patient_damage"),50);awaitTreat(g,60);
  int retained=g.state(g.friendlyKey(0,partial));for(const char*f:{"task_cursor","task_cursor_saved","task_fail_schedule"})g.setState(g.friendlyKey(0,f),0);g.setState(g.friendlyKey(0,partial),retained);
  Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Partial new table metadata cannot use a missing saved-cursor marker to bypass validation as a legacy save");}
 {auto g=scene([&](auto&map){opponent(map);});int frame=0;while(frame++<300&&g.state(g.friendlyKey(0,"weapon_aim"))<300)tick(g,1);bool owned=g.guardTaskAllows(0,GuardTask::Fire);
  g.setState(g.friendlyKey(0,"task_cursor"),0);int rounds=g.state(g.friendlyKey(0,"weapon_rounds"));bool rejected=!g.guardTaskAllows(0,GuardTask::Fire);
  g.updateGuardCombat(1.f/60);check(owned&&rejected&&!g.state(g.friendlyKey(0,"weapon_aim"))&&g.state(g.friendlyKey(0,"weapon_rounds"))==rounds&&!g.state("table_target_damage"),
   "A marked live cursor pointing at Prepare cannot acquire Fire ownership from stale compatible work-phase fields or advance an ammunition event");}
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}

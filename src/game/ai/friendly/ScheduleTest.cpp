#include "../../Game.h"
#include "Mind.h"
#include "ScheduleTables.h"
#include <fstream>
namespace retro {
bool Game::testCivilianSchedule(){
 std::ofstream report("civilian-schedule-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};

 {bool valid=validCivilianScheduleCatalog()&&civilianScheduleCatalog.size()==6,distinct=true;
  for(size_t id=0;id<civilianScheduleCatalog.size();++id){const auto&row=civilianScheduleDefinition(CivilianSchedule(id));valid&=row.schedule.id==int(id)&&ai::validScheduleDefinition(row.schedule)&&*row.schedule.name;
   for(size_t prior=0;prior<id;++prior)distinct&=std::string_view(row.schedule.name)!=civilianScheduleCatalog[prior].schedule.name;}
  check(valid&&distinct,"Civilian schedule catalog exposes six valid distinct named task lists with explicit failure destinations");
  const auto&idle=civilianScheduleDefinition(CivilianSchedule::Idle);const auto&follow=civilianScheduleDefinition(CivilianSchedule::Follow);const auto&flee=civilianScheduleDefinition(CivilianSchedule::Flee);
  check(idle.schedule.tasks.size()==1&&idle.schedule.tasks[0].operation==ai::TaskOperation::Wait&&idle.work==civilianTaskBit(CivilianTask::Hold),
   "Idle holds in place through Wait work");
  check(follow.schedule.tasks.size()==3&&follow.schedule.tasks[0].operation==ai::TaskOperation::Prepare&&follow.schedule.tasks[1].operation==ai::TaskOperation::Move&&follow.work==civilianTaskBit(CivilianTask::Hold),
   "Follow walks through Prepare/Move before Hold work");
  check(flee.schedule.tasks.size()==1&&flee.schedule.tasks[0].operation==ai::TaskOperation::Escape&&flee.work==civilianTaskBit(CivilianTask::Escape)&&flee.schedule.failureSchedule==int(CivilianSchedule::Recovery),
   "Flee is a dedicated Escape list that recovers explicitly");
 }

 {struct Adapter{std::vector<int> starts,runs;int charges=0;
  void start(const ai::ScheduleTask&task,ai::SchedulerState&){starts.push_back(int(task.operation));}
  ai::TaskStatus run(const ai::ScheduleTask&task,ai::SchedulerState&state){
   runs.push_back(int(task.operation));
   if(task.operation==ai::TaskOperation::Prepare)return ai::TaskStatus::Complete;
   if(task.operation==ai::TaskOperation::Move)return ai::TaskStatus::Complete;
   if(task.operation==ai::TaskOperation::Escape)return state.elapsedMs>=40?ai::TaskStatus::Complete:ai::TaskStatus::Running;
   if(task.operation==ai::TaskOperation::Wait)return state.elapsedMs>=task.argument?ai::TaskStatus::Complete:ai::TaskStatus::Running;
   ++charges;return state.elapsedMs>=50?ai::TaskStatus::Complete:ai::TaskStatus::Running;
  }};
  Adapter actor;ai::SchedulerState state;const auto&definition=civilianScheduleDefinition(CivilianSchedule::Follow).schedule;
  auto result=ai::MaintainSchedule(definition,state,actor);check(result.outcome==ai::ScheduleOutcome::Running&&state.cursor==2&&state.status==ai::TaskStatus::Running&&actor.starts.size()==3,
   "Civilian Follow MaintainSchedule advances Prepare and Move synchronously into Hold work");
  Adapter fleeActor;ai::SchedulerState fleeState;const auto&flee=civilianScheduleDefinition(CivilianSchedule::Flee).schedule;
  auto fleeing=ai::MaintainSchedule(flee,fleeState,fleeActor);check(fleeing.outcome==ai::ScheduleOutcome::Running&&fleeState.cursor==0&&fleeActor.starts.size()==1&&fleeActor.starts[0]==int(ai::TaskOperation::Escape),
   "Civilian Flee MaintainSchedule starts Escape work through the shared bounded executor");
  fleeState.elapsedMs=40;fleeing=ai::MaintainSchedule(flee,fleeState,fleeActor);check(fleeing.outcome==ai::ScheduleOutcome::ScheduleDone,
   "Civilian Flee Escape completes through the shared bounded executor");
 }

 auto scene=[](ActorAiMode mode){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Civilian schedule validation";campaign->key=771902;
  auto map=std::make_shared<AuthoredMapData>();map->name="Schedule room";map->definition={{0,0},{16.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("civ_sched_clock");timer.finishState=stateId("civ_sched_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  ActorTrack worker;worker.timerState=timer.timerState;worker.keys={{0,{6.5f,6.5f},0,0},{30000,{6.5f,6.5f},0,0}};worker.ai.mode=mode;worker.health=70;
  worker.damageState=stateId("civ_sched_damage");worker.deadState=stateId("civ_sched_dead");map->actorTracks.push_back(worker);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int count,float dt=1.f/60.f){for(int i=0;i<count;++i){g.updateStreaming(dt);g.updateMechanisms(dt);g.m_world.updateDoors(dt);}};

 {auto g=scene(ActorAiMode::Civilian);Enemy threat{};threat.pos={4.5f,7.5f};g.m_enemies.push_back(threat);tick(g,30);
  CivilianMindView mind{g,0};
  check(mind.schedule()==CivilianSchedule::Flee&&mind.task()==CivilianTask::Escape,
   "Visible threat adopts Flee Escape ownership through the civilian schedule executor");
  check(g.state(g.friendlyKey(0,"schedule_step"))==2&&mind.taskStatus()==CivilianTaskStatus::Running,
   "Flee Escape remains Running while the civilian is still escaping");
  mind.setFear(2500);mind.setThreat({4.5f,7.5f});
  check(mind.fear()==2500&&length(mind.threat()-Vec2{4.5f,7.5f})<.01f&&g.state(g.friendlyKey(0,"fear"))==2500,
   "Typed CivilianMindView fear accessors write the same save keys as the legacy string fields");
 }

 {auto g=scene(ActorAiMode::Escort);tick(g,90);
  check(g.state(g.friendlyKey(0,"schedule"))==int(CivilianSchedule::Follow)&&g.state(g.friendlyKey(0,"task_cursor_saved")),
   "Escort follow adopts Follow through the civilian schedule executor");
  auto position=g.world().actorPose(0).position;g.m_player.pos=position+Vec2{1,0};g.m_player.angle=kPi;check(g.commandFriendly(),"Escort accepts Hold command");
  tick(g,30);check(g.state(g.friendlyKey(0,"schedule"))==int(CivilianSchedule::Hold)&&g.state(g.friendlyKey(0,"task"))==int(CivilianTask::Hold),
   "Hold command selects the Hold schedule while preserving executor ownership");
 }

 {auto g=scene(ActorAiMode::Scripted);auto start=g.world().actorPose(0).position;tick(g,60);
  check(length(g.world().actorPose(0).position-start)<.001f&&g.state(g.friendlyKey(0,"schedule"))==0,
   "Scripted actors stay on timeline until fear hands control to the shared executor");
  Enemy threat{};threat.pos={4.5f,7.5f};g.m_enemies.push_back(threat);tick(g,45);
  check(g.state(g.friendlyKey(0,"schedule"))==int(CivilianSchedule::Flee)&&length(g.world().actorPose(0).position-start)>.2f,
   "Scripted fear handoff flees through Flee Escape without abandoning physical movement");
  g.m_enemies[0].alive=false;g.setState(g.friendlyKey(0,"fear"),0);tick(g,90);
  check(g.state(g.friendlyKey(0,"schedule"))==0||g.state(g.friendlyKey(0,"schedule"))==int(CivilianSchedule::Idle)||g.state(g.friendlyKey(0,"schedule"))==int(CivilianSchedule::Recovery),
   "Scripted handoff clears or recovers schedule ownership once fear ends");
 }

 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}

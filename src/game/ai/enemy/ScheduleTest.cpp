#include "../../Game.h"
#include "ScheduleTables.h"
#include <fstream>
namespace retro {
bool Game::testCreatureSchedule(){
 std::ofstream report("creature-schedule-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};

 {bool valid=validCreatureScheduleCatalog()&&creatureScheduleCatalog.size()==7,distinct=true;
  for(size_t id=0;id<creatureScheduleCatalog.size();++id){const auto&row=creatureScheduleDefinition(CreatureSchedule(id));valid&=row.schedule.id==int(id)&&ai::validScheduleDefinition(row.schedule)&&*row.schedule.name;
   for(size_t prior=0;prior<id;++prior)distinct&=std::string_view(row.schedule.name)!=creatureScheduleCatalog[prior].schedule.name;}
  check(valid&&distinct,"Creature schedule catalog exposes seven valid distinct named task lists with explicit failure destinations");
  const auto&idle=creatureScheduleDefinition(CreatureSchedule::Idle);const auto&chase=creatureScheduleDefinition(CreatureSchedule::Chase);const auto&attack=creatureScheduleDefinition(CreatureSchedule::Attack);const auto&search=creatureScheduleDefinition(CreatureSchedule::Search);
  check(idle.schedule.tasks.size()==3&&idle.schedule.tasks[0].operation==ai::TaskOperation::Prepare&&idle.schedule.tasks[1].operation==ai::TaskOperation::Move&&idle.schedule.tasks[2].operation==ai::TaskOperation::ExecuteWork,
   "Idle walks home through Prepare/Move before Hold work");
  check(chase.schedule.tasks.size()==3&&chase.work==(creatureTaskBit(CreatureTask::Hold)|creatureTaskBit(CreatureTask::Strike))&&attack.schedule.tasks.size()==1&&attack.schedule.tasks[0].operation==ai::TaskOperation::ExecuteWork&&attack.work==creatureTaskBit(CreatureTask::Strike),
   "Chase owns approach work while Attack is a dedicated melee ExecuteWork list");
  check(search.schedule.tasks.size()==2&&search.schedule.tasks[1].operation==ai::TaskOperation::Wait&&search.schedule.failureSchedule==int(CreatureSchedule::Idle),
   "Search holds in place with a Wait task and recovers to Idle");
 }

 {struct Adapter{std::vector<int> starts,runs;int charges=0;
  void start(const ai::ScheduleTask&task,ai::SchedulerState&){starts.push_back(int(task.operation));}
  ai::TaskStatus run(const ai::ScheduleTask&task,ai::SchedulerState&state){
   runs.push_back(int(task.operation));
   if(task.operation==ai::TaskOperation::Prepare)return ai::TaskStatus::Complete;
   if(task.operation==ai::TaskOperation::Move)return ai::TaskStatus::Complete;
   if(task.operation==ai::TaskOperation::Wait)return state.elapsedMs>=task.argument?ai::TaskStatus::Complete:ai::TaskStatus::Running;
   ++charges;return state.elapsedMs>=50?ai::TaskStatus::Complete:ai::TaskStatus::Running;
  }};
  Adapter actor;ai::SchedulerState state;const auto&definition=creatureScheduleDefinition(CreatureSchedule::Chase).schedule;
  auto result=ai::MaintainSchedule(definition,state,actor);check(result.outcome==ai::ScheduleOutcome::Running&&state.cursor==2&&state.status==ai::TaskStatus::Running&&actor.starts.size()==3,
   "Creature Chase MaintainSchedule advances Prepare and Move synchronously into Strike work");
  state.elapsedMs=50;result=ai::MaintainSchedule(definition,state,actor);check(result.outcome==ai::ScheduleOutcome::ScheduleDone&&actor.charges>=1,
   "Creature Chase Strike completes through the shared bounded executor");
 }

 {auto g=validationScene(Enemy::Kind::Huntsman);auto&e=g.m_enemies[0];
  e.pos={4.5f,9.5f};e.home=e.lastKnown=e.pos;e.heading=-kPi*.5f;g.m_player.pos={4.5f,6.5f};g.m_sounds.clear();
  g.m_sounds.push_back({Sound::Concrete1,{6.5f,9.5f},1.f,1,true});g.updateEnemies(.01f);
  check(e.state==Enemy::State::Investigate&&e.schedule==int(CreatureSchedule::Investigate)&&e.scheduleCursorSaved&&e.scheduleStep>=1,
   "Hearing adopts Investigate through the creature schedule executor");
 }

 {auto g=validationScene(Enemy::Kind::Mutant);auto&body=g.m_enemies[0];body.pos=g.m_player.pos+Vec2{.7f,0};body.heading=kPi;
  g.updateEnemies(.01f);
  check(body.windup>0&&(body.schedule==int(CreatureSchedule::Attack)||g.creatureTaskAllows(body,CreatureTask::Strike)),
   "Close mutant contact enters Attack/Strike ownership before the committed swing");
  for(int i=0;i<72;++i)g.updateEnemies(1.f/120.f);
  check(g.player().health<100&&g.player().health>=70,"Scheduled mutant melee still lands contact damage");
 }

 {auto g=validationScene(Enemy::Kind::Huntsman);auto&e=g.m_enemies[0];e.pos={4.5f,9.5f};e.home=e.pos;e.lastKnown=e.pos;e.heading=-kPi*.5f;g.m_player.pos={4.5f,6.5f};
  for(int i=0;i<30;++i)g.updateEnemies(1.f/60.f);
  check(e.state==Enemy::State::Idle&&e.schedule==int(CreatureSchedule::Idle)&&e.scheduleStep==2&&e.scheduleTask==int(CreatureTask::Hold),
   "Idle creatures settle into Hold work after the shared Prepare/Move list");
  InputState fire{};fire.fire=true;g.update(fire,.02f);
  check(e.state==Enemy::State::Investigate&&e.schedule==int(CreatureSchedule::Investigate),"Gunfire interrupts Idle Hold into Investigate ownership");
 }

 {Game rough(WorldId::Ashfall);rough.m_enemies.clear();rough.m_sounds.clear();
  rough.m_player.pos={20.5f,12.5f};rough.m_player.z=rough.world().supportBelow(20.5f,12.5f,float(World::TerrainMaxZ+1));rough.m_player.angle=0;
  Enemy climber{};climber.kind=Enemy::Kind::Brute;climber.pos={6.5f,12.5f};climber.z=rough.world().supportBelow(6.5f,12.5f,float(World::TerrainMaxZ+1));climber.home=climber.pos;climber.lastKnown=rough.m_player.pos;climber.lastKnownZ=rough.m_player.z;climber.awareness=40.f;climber.state=Enemy::State::Investigate;climber.heading=0;
  rough.m_enemies.push_back(climber);
  for(int i=0;i<3600&&length(rough.m_enemies[0].pos-rough.m_player.pos)>1.5f;++i)rough.updateEnemies(1.f/120.f);
  auto&done=rough.m_enemies[0];float support=rough.world().supportBelow(done.pos.x,done.pos.y,float(World::TerrainMaxZ+1));
  report<<"brute full dist="<<length(done.pos-rough.m_player.pos)<<" z="<<done.z<<" support="<<support<<" state="<<int(done.state)<<" schedule="<<done.schedule<<'\n';
  check(length(done.pos-rough.m_player.pos)<=1.8f&&std::fabs(done.z-support)<=.05f,"Brute completes Ashfall rough-terrain pursuit under MaintainSchedule");
 }

 report<<"checks "<<checks<<'\n';return passed;
}
}

#include "../../Game.h"
#include "ScheduleTables.h"
namespace retro {
CreatureTaskPolicy creatureTaskPolicy(CreatureSchedule schedule){const auto&row=creatureScheduleDefinition(schedule);return {row.schedule.interruptMask,row.work};}
CreatureSchedule creatureScheduleFromState(Enemy::State state){
 switch(state){
 case Enemy::State::Investigate:return CreatureSchedule::Investigate;
 case Enemy::State::Search:return CreatureSchedule::Search;
 case Enemy::State::Chase:return CreatureSchedule::Chase;
 default:return CreatureSchedule::Idle;
 }
}
unsigned Game::creatureConditions(const Enemy& enemy,bool visible,bool heard,bool canMelee,bool hurt)const{
 unsigned conditions=0;
 if(enemy.awareness>0)conditions|=CreatureHasThreat;
 if(visible)conditions|=CreatureSeesThreat;else if(enemy.awareness>0)conditions|=CreatureOccluded|CreatureLostTarget;
 if(heard)conditions|=CreatureHeardSound;
 if(canMelee)conditions|=CreatureCanMelee;
 if(hurt)conditions|=CreatureHurt;
 return conditions;
}
CreatureScheduleChoice Game::creatureScheduleChoice(const Enemy& enemy,unsigned conditions,bool arrive)const{
 CreatureScheduleChoice choice;
 choice.arrive=arrive;
 if(enemy.windup>0||enemy.strike>0){choice.schedule=CreatureSchedule::Attack;choice.work=CreatureTask::Strike;return choice;}
 choice.schedule=creatureScheduleFromState(enemy.state);
 if(choice.schedule==CreatureSchedule::Chase||choice.schedule==CreatureSchedule::Investigate)choice.work=(conditions&CreatureCanMelee)?CreatureTask::Strike:CreatureTask::Hold;
 else choice.work=CreatureTask::Hold;
 return choice;
}
bool Game::creatureTaskAllows(const Enemy& enemy,CreatureTask task)const{
 if(enemy.schedule==int(CreatureSchedule::None))return true;
 if(enemy.scheduleCursorSaved){
  const auto&definition=creatureScheduleDefinition(CreatureSchedule(enemy.schedule)).schedule;int cursor=enemy.scheduleCursor;
  if(cursor<0||size_t(cursor)>=definition.tasks.size())return false;
  auto operation=definition.tasks[size_t(cursor)].operation;
  if(operation!=ai::TaskOperation::ExecuteWork&&!(operation==ai::TaskOperation::Wait&&task==CreatureTask::Hold))return false;
 }
 return (creatureTaskPolicy(CreatureSchedule(enemy.schedule)).work&creatureTaskBit(task))&&enemy.scheduleStep==2&&enemy.scheduleStatus==int(CreatureTaskStatus::Running)&&enemy.scheduleTask==int(task);
}
void Game::clearCreatureSchedule(Enemy& enemy)const{
 enemy.schedule=0;enemy.scheduleStep=0;enemy.scheduleTask=0;enemy.scheduleStatus=0;enemy.scheduleMs=0;enemy.scheduleCursor=0;enemy.scheduleCursorSaved=0;enemy.scheduleConditions=0;enemy.scheduleTransitions=0;
}
void Game::runCreatureSchedule(Enemy& enemy,int ms,bool visible,bool heard,bool canMelee,bool hurt,bool wantsMove){
 unsigned oldConditions=enemy.scheduleConditions;
 unsigned conditions=creatureConditions(enemy,visible,heard,canMelee,hurt);
 auto previous=CreatureSchedule(enemy.schedule);
 auto choice=creatureScheduleChoice(enemy,conditions,!wantsMove);
 auto desired=choice.schedule;auto work=choice.work;
 enemy.scheduleConditions=conditions;
 // Match guard event edges: continuous sight/melee readiness are not sticky
 // interrupts. Rising edges still arrive through conditions&~oldConditions.
 unsigned events=(conditions&~oldConditions)|(conditions&(CreatureHeardSound|CreatureHurt|CreatureLostTarget));
 bool interrupted=previous!=desired||ai::scheduleInterrupts(creatureScheduleDefinition(previous).schedule,events);
 if(interrupted){
  enemy.schedule=int(desired);enemy.scheduleStep=0;enemy.scheduleTask=int(CreatureTask::Prepare);enemy.scheduleStatus=int(CreatureTaskStatus::New);enemy.scheduleMs=0;enemy.scheduleCursor=0;enemy.scheduleCursorSaved=1;
 }
 const auto&definition=creatureScheduleDefinition(desired).schedule;
 ai::SchedulerState cursor{enemy.scheduleCursor,ai::TaskStatus(enemy.scheduleStatus),enemy.scheduleMs};
 if(!enemy.scheduleCursorSaved){for(size_t i=0;i<definition.tasks.size();++i)if(definition.tasks[i].phase==enemy.scheduleStep){cursor.cursor=int(i);break;}enemy.scheduleCursorSaved=1;}
 if(enemy.scheduleStep==2&&desired!=CreatureSchedule::Attack&&desired!=CreatureSchedule::Recovery&&desired!=CreatureSchedule::Search&&desired!=CreatureSchedule::Idle&&(wantsMove||enemy.scheduleTask!=int(work))){
  int phase=wantsMove?1:2;for(size_t i=0;i<definition.tasks.size();++i)if(definition.tasks[i].phase==phase){cursor={int(i),ai::TaskStatus::New,0};break;}
 }
 auto concreteTask=[&](const ai::ScheduleTask&task){return task.operation==ai::TaskOperation::Prepare?CreatureTask::Prepare:task.operation==ai::TaskOperation::Move?CreatureTask::Move:task.operation==ai::TaskOperation::Wait?CreatureTask::Hold:work;};
 auto startTask=[&](const ai::ScheduleTask&task,ai::SchedulerState&state){
  auto concrete=concreteTask(task);enemy.scheduleStep=task.phase;enemy.scheduleTask=int(concrete);enemy.scheduleStatus=int(state.status);enemy.scheduleMs=0;
 };
 auto runTask=[&](const ai::ScheduleTask&task,ai::SchedulerState&state){switch(task.operation){
  case ai::TaskOperation::Prepare:return ai::TaskStatus::Complete;
  case ai::TaskOperation::Move:return wantsMove?ai::TaskStatus::Running:ai::TaskStatus::Complete;
  case ai::TaskOperation::Wait:return state.elapsedMs>=task.argument?ai::TaskStatus::Complete:ai::TaskStatus::Running;
  default:return ai::TaskStatus::Running;
 }};
 struct Adapter{decltype(startTask)&start;decltype(runTask)&run;};Adapter adapter{startTask,runTask};
 auto result=ai::MaintainSchedule(definition,cursor,adapter,4);unsigned transitions=result.transitions;
 if(result.outcome==ai::ScheduleOutcome::ScheduleDone&&desired!=CreatureSchedule::Recovery&&desired!=CreatureSchedule::Idle&&transitions<4){
  cursor={0,ai::TaskStatus::New,0};auto next=ai::MaintainSchedule(definition,cursor,adapter,4-transitions);transitions+=next.transitions;result=next;
 }
 if(result.outcome==ai::ScheduleOutcome::TaskFailed||result.outcome==ai::ScheduleOutcome::InvalidState){
  const auto&failure=creatureScheduleDefinition(CreatureSchedule(definition.failureSchedule)).schedule;
  enemy.schedule=failure.id;enemy.scheduleStep=failure.tasks.empty()?0:failure.tasks.front().phase;enemy.scheduleTask=int(CreatureTask::Hold);
  enemy.scheduleStatus=int(CreatureTaskStatus::New);enemy.scheduleMs=0;enemy.scheduleCursor=0;enemy.scheduleCursorSaved=1;enemy.scheduleTransitions=0;return;
 }
 if(cursor.status==ai::TaskStatus::New&&size_t(cursor.cursor)<definition.tasks.size()){const auto&pending=definition.tasks[size_t(cursor.cursor)];enemy.scheduleStep=pending.phase;enemy.scheduleTask=int(concreteTask(pending));}
 enemy.scheduleCursor=cursor.cursor;enemy.scheduleCursorSaved=1;enemy.scheduleStatus=int(cursor.status);enemy.scheduleTransitions=int(transitions);enemy.scheduleMs=std::min(60000,cursor.elapsedMs+ms);
}
}

#include "../../Game.h"
#include "ScheduleTables.h"
namespace retro {
CivilianTaskPolicy civilianTaskPolicy(CivilianSchedule schedule){const auto&row=civilianScheduleDefinition(schedule);return {row.schedule.interruptMask,row.work};}
void Game::clearCivilianSchedule(size_t actor){
 for(const char*f:{"schedule","schedule_step","task","task_status","task_ms","task_cursor","task_cursor_saved","task_conditions","task_transitions","task_fail_schedule"})setState(friendlyKey(actor,f),0);
}
void Game::runCivilianSchedule(size_t actor,int ms,unsigned conditions,bool wantsMove){
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto put=[&](const char*f,int v){setState(friendlyKey(actor,f),v);};
 auto previous=CivilianSchedule(get("schedule"));unsigned oldConditions=unsigned(get("task_conditions"));
 CivilianScheduleChoice choice;
 if(conditions&CivilianInFear){choice.schedule=CivilianSchedule::Flee;choice.work=CivilianTask::Escape;}
 else if(conditions&CivilianShouldFollow){choice.schedule=CivilianSchedule::Follow;choice.work=CivilianTask::Hold;}
 else if((get("hold")&&m_friendlyActors[actor].track().ai.mode==ActorAiMode::Escort)||get("talking")){choice.schedule=CivilianSchedule::Hold;choice.work=CivilianTask::Hold;}
 else{choice.schedule=CivilianSchedule::Idle;choice.work=CivilianTask::Hold;}
 auto desired=choice.schedule;auto work=choice.work;put("task_conditions",int(conditions));
 unsigned events=(conditions&~oldConditions)|(conditions&(CivilianInFear|CivilianHurt|CivilianShouldFollow));
 bool interrupted=previous!=desired||ai::scheduleInterrupts(civilianScheduleDefinition(previous).schedule,events);
 if(interrupted){
  put("schedule",int(desired));put("schedule_step",0);put("task",int(CivilianTask::Prepare));put("task_status",int(CivilianTaskStatus::New));put("task_ms",0);put("task_cursor",0);put("task_cursor_saved",1);
 }
 const auto&definition=civilianScheduleDefinition(desired).schedule;put("task_fail_schedule",definition.failureSchedule);
 ai::SchedulerState cursor{get("task_cursor"),ai::TaskStatus(get("task_status")),get("task_ms")};
 if(!get("task_cursor_saved")){for(size_t i=0;i<definition.tasks.size();++i)if(definition.tasks[i].phase==get("schedule_step")){cursor.cursor=int(i);break;}put("task_cursor_saved",1);}
 if(get("schedule_step")==2&&desired==CivilianSchedule::Follow&&(wantsMove||get("task")!=int(work))){
  int phase=wantsMove?1:2;for(size_t i=0;i<definition.tasks.size();++i)if(definition.tasks[i].phase==phase){cursor={int(i),ai::TaskStatus::New,0};break;}
 }
 auto concreteTask=[&](const ai::ScheduleTask&task){return task.operation==ai::TaskOperation::Prepare?CivilianTask::Prepare:task.operation==ai::TaskOperation::Move?CivilianTask::Move:task.operation==ai::TaskOperation::Escape?CivilianTask::Escape:task.operation==ai::TaskOperation::Wait?CivilianTask::Hold:work;};
 auto startTask=[&](const ai::ScheduleTask&task,ai::SchedulerState&state){put("schedule_step",task.phase);put("task",int(concreteTask(task)));put("task_status",int(state.status));put("task_ms",0);};
 auto runTask=[&](const ai::ScheduleTask&task,ai::SchedulerState&state){switch(task.operation){
  case ai::TaskOperation::Prepare:return ai::TaskStatus::Complete;
  case ai::TaskOperation::Move:return wantsMove?ai::TaskStatus::Running:ai::TaskStatus::Complete;
  case ai::TaskOperation::Escape:return wantsMove?ai::TaskStatus::Running:ai::TaskStatus::Complete;
  case ai::TaskOperation::Wait:return state.elapsedMs>=task.argument?ai::TaskStatus::Complete:ai::TaskStatus::Running;
  default:return ai::TaskStatus::Running;
 }};
 struct Adapter{decltype(startTask)&start;decltype(runTask)&run;};Adapter adapter{startTask,runTask};
 auto result=ai::MaintainSchedule(definition,cursor,adapter,4);unsigned transitions=result.transitions;
 if(result.outcome==ai::ScheduleOutcome::ScheduleDone&&desired!=CivilianSchedule::Recovery&&desired!=CivilianSchedule::Idle&&desired!=CivilianSchedule::Hold&&transitions<4){
  cursor={0,ai::TaskStatus::New,0};auto next=ai::MaintainSchedule(definition,cursor,adapter,4-transitions);transitions+=next.transitions;result=next;
 }
 if(result.outcome==ai::ScheduleOutcome::TaskFailed||result.outcome==ai::ScheduleOutcome::InvalidState){
  const auto&failure=civilianScheduleDefinition(CivilianSchedule(definition.failureSchedule)).schedule;
  put("schedule",failure.id);put("schedule_step",failure.tasks.empty()?0:failure.tasks.front().phase);put("task",int(CivilianTask::Hold));
  put("task_status",int(CivilianTaskStatus::New));put("task_ms",0);put("task_cursor",0);put("task_cursor_saved",1);put("task_transitions",0);return;
 }
 if(cursor.status==ai::TaskStatus::New&&size_t(cursor.cursor)<definition.tasks.size()){const auto&pending=definition.tasks[size_t(cursor.cursor)];put("schedule_step",pending.phase);put("task",int(concreteTask(pending)));}
 put("task_cursor",cursor.cursor);put("task_cursor_saved",1);put("task_status",int(cursor.status));put("task_transitions",int(transitions));put("task_ms",std::min(60000,cursor.elapsedMs+ms));
}
}

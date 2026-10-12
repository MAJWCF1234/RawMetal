#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace retro::ai {
// The status values match persisted actor tasks. Each actor adapter owns its
// concrete action identifiers, goals, senses, resource commitments and clock.
enum class TaskStatus : std::uint8_t {New,Running,Complete,Failed};
enum class TaskOperation : std::uint8_t {Prepare,Move,ExecuteWork,Escape,Wait};
enum ScheduleSound : std::uint32_t {SoundWorld=1,SoundWeapon=2,SoundImpact=4,SoundDanger=8};
inline constexpr int MaximumScheduleTasks=8;
struct ScheduleTask {
 TaskOperation operation=TaskOperation::Prepare;
 int argument=0;
 std::uint8_t phase=0;
};
struct ScheduleDefinition {
 int id=0;
 const char* name="";
 std::span<const ScheduleTask> tasks{};
 int failureSchedule=0;
 std::uint32_t interruptMask=0,soundMask=0;
};
struct SchedulerState {
 int cursor=0;
 TaskStatus status=TaskStatus::New;
 int elapsedMs=0;
};
enum class ScheduleOutcome : std::uint8_t {Running,ScheduleDone,TaskFailed,InvalidState,TransitionLimit};
struct MaintainResult {
 ScheduleOutcome outcome=ScheduleOutcome::Running;
 unsigned transitions=0;
 int failureSchedule=0;
};
constexpr bool scheduleInterrupts(const ScheduleDefinition& definition,std::uint32_t conditions){return (definition.interruptMask&conditions)!=0;}
constexpr std::uint32_t acceptedScheduleSounds(const ScheduleDefinition& definition,std::uint32_t sounds){return definition.soundMask&sounds;}
constexpr bool validScheduleDefinition(const ScheduleDefinition& definition){
 if(definition.id<0||definition.failureSchedule<0||!definition.name||definition.tasks.size()>MaximumScheduleTasks||definition.soundMask>15)return false;
 for(const auto&task:definition.tasks)if(unsigned(task.operation)>unsigned(TaskOperation::Wait)||task.phase>2||task.argument<0)return false;
 return true;
}
// Start publishes the concrete owned task before Run observes it. Run may
// finish a synchronous task or leave work Running for an external consumer.
// Async completion is supplied in state.status on the next maintenance pass.
// The adapter advances elapsedMs once per actor update; immediate transitions
// cannot charge the same simulation time to several tasks or replay work.
// Adapter: void start(const ScheduleTask&, SchedulerState&);
//          TaskStatus run(const ScheduleTask&, SchedulerState&);
template<class Adapter>
MaintainResult MaintainSchedule(const ScheduleDefinition& definition,SchedulerState& state,Adapter&& adapter,unsigned maximumTransitions=4){
 MaintainResult result{ScheduleOutcome::Running,0,definition.failureSchedule};
 auto finish=[&](ScheduleOutcome outcome){result.outcome=outcome;return result;};
 if(!validScheduleDefinition(definition)||state.cursor<0||std::size_t(state.cursor)>definition.tasks.size()||state.elapsedMs<0||unsigned(state.status)>unsigned(TaskStatus::Failed))return finish(ScheduleOutcome::InvalidState);
 if(std::size_t(state.cursor)==definition.tasks.size())return finish(state.status==TaskStatus::Complete||(definition.tasks.empty()&&state.status==TaskStatus::New)?ScheduleOutcome::ScheduleDone:ScheduleOutcome::InvalidState);
 if(state.status==TaskStatus::Failed)return finish(ScheduleOutcome::TaskFailed);
 // Advancing a completed task is one transition. Starting its successor and
 // running it synchronously is part of that task's single bounded iteration.
 // Prepare, Move and an asynchronous Work start therefore fit in three.
 while(result.transitions<maximumTransitions){
  int cursor=state.cursor;
  const auto&task=definition.tasks[std::size_t(state.cursor)];
  bool started=state.status==TaskStatus::New;
  if(started){state.status=TaskStatus::Running;state.elapsedMs=0;adapter.start(task,state);}
  if(state.cursor!=cursor||state.elapsedMs<0)return finish(ScheduleOutcome::InvalidState);
  if(state.status==TaskStatus::Running)state.status=adapter.run(task,state);
  if(state.cursor!=cursor||state.elapsedMs<0||unsigned(state.status)>unsigned(TaskStatus::Failed)||state.status==TaskStatus::New)return finish(ScheduleOutcome::InvalidState);
  if(state.status==TaskStatus::Failed)return finish(ScheduleOutcome::TaskFailed);
  if(state.status==TaskStatus::Running){if(started)++result.transitions;return finish(ScheduleOutcome::Running);}
  ++state.cursor;++result.transitions;state.elapsedMs=0;
  if(std::size_t(state.cursor)==definition.tasks.size())return finish(ScheduleOutcome::ScheduleDone);
  state.status=TaskStatus::New;
 }
 return finish(ScheduleOutcome::TransitionLimit);
}
}

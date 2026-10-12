#pragma once
#include "../Scheduler.h"
#include "Schedule.h"
#include <array>
namespace retro {
struct GuardScheduleDefinition {ai::ScheduleDefinition schedule;unsigned work=0;};
inline constexpr std::array<ai::ScheduleTask,3> guardOrdinaryTasks={{{ai::TaskOperation::Prepare,0,0},{ai::TaskOperation::Move,0,1},{ai::TaskOperation::ExecuteWork,0,2}}};
inline constexpr std::array<ai::ScheduleTask,1> guardEscapeTasks={{{ai::TaskOperation::Escape,0,2}}};
inline constexpr std::array<ai::ScheduleTask,1> guardRecoveryTasks={{{ai::TaskOperation::Wait,700,2}}};
inline constexpr unsigned guardArmedWork=guardTaskBit(GuardTask::Hold)|guardTaskBit(GuardTask::Fire)|guardTaskBit(GuardTask::Bash)|guardTaskBit(GuardTask::Reload)|guardTaskBit(GuardTask::Throw);
inline constexpr unsigned guardAwarenessInterrupts=GuardInDanger|GuardNewThreat|GuardEnemyDead|GuardLightDamage|GuardHeavyDamage|GuardNeedsReload|GuardHeardSound;
inline constexpr unsigned guardSupportInterrupts=GuardInDanger|GuardNewThreat|GuardEnemyDead|GuardLightDamage|GuardHeavyDamage|GuardCloseThreat|GuardNeedsReload|GuardHeardSound;
inline constexpr unsigned guardAllSounds=ai::SoundWorld|ai::SoundWeapon|ai::SoundImpact|ai::SoundDanger;
// Rows are indexed by the existing saved schedule IDs. The physical task list
// is independent of its compatible Prepare/Move/Work phase projection. Work
// binds the already selected role action; new lists may add tasks without
// changing mechanical action/resource serialization.
inline constexpr std::array<GuardScheduleDefinition,11> guardScheduleCatalog={{
 {{int(GuardSchedule::None),"None",{},int(GuardSchedule::None),0,0},0},
 {{int(GuardSchedule::Patrol),"Patrol",guardOrdinaryTasks,int(GuardSchedule::Recovery),guardAwarenessInterrupts,guardAllSounds},guardArmedWork},
 {{int(GuardSchedule::Investigate),"Investigate",guardOrdinaryTasks,int(GuardSchedule::Recovery),guardAwarenessInterrupts|GuardCanRangeAttack|GuardCanMeleeAttack,guardAllSounds},guardArmedWork},
 {{int(GuardSchedule::Search),"Search",guardOrdinaryTasks,int(GuardSchedule::Recovery),guardAwarenessInterrupts|GuardCanRangeAttack|GuardCanMeleeAttack,guardAllSounds},guardArmedWork},
 {{int(GuardSchedule::Combat),"Combat",guardOrdinaryTasks,int(GuardSchedule::Recovery),guardAwarenessInterrupts|GuardEnemyOccluded|GuardCanMeleeAttack,ai::SoundWeapon|ai::SoundDanger},guardArmedWork},
 {{int(GuardSchedule::Cover),"Cover",guardOrdinaryTasks,int(GuardSchedule::Recovery),GuardInDanger|GuardNewThreat|GuardEnemyDead|GuardHeavyDamage|GuardHeardSound,ai::SoundWeapon|ai::SoundDanger},guardArmedWork},
 {{int(GuardSchedule::Danger),"Danger",guardEscapeTasks,int(GuardSchedule::Recovery),GuardEnemyDead,ai::SoundDanger},guardTaskBit(GuardTask::Escape)},
 {{int(GuardSchedule::Medic),"Medic",guardOrdinaryTasks,int(GuardSchedule::Recovery),guardSupportInterrupts,ai::SoundDanger},guardTaskBit(GuardTask::Hold)|guardTaskBit(GuardTask::Treat)},
 {{int(GuardSchedule::Engineer),"Engineer",guardOrdinaryTasks,int(GuardSchedule::Recovery),guardSupportInterrupts,ai::SoundDanger},guardArmedWork|guardTaskBit(GuardTask::Deploy)|guardTaskBit(GuardTask::Repair)},
 {{int(GuardSchedule::Recovery),"Recovery",guardRecoveryTasks,int(GuardSchedule::Patrol),GuardInDanger|GuardNewThreat|GuardEnemyDead|GuardHeavyDamage|GuardHeardSound,ai::SoundDanger},guardTaskBit(GuardTask::Hold)},
 {{int(GuardSchedule::Patient),"Patient",guardOrdinaryTasks,int(GuardSchedule::Recovery),guardSupportInterrupts,ai::SoundDanger},guardTaskBit(GuardTask::Hold)|guardTaskBit(GuardTask::Fire)|guardTaskBit(GuardTask::Bash)|guardTaskBit(GuardTask::Reload)}
}};
constexpr const GuardScheduleDefinition& guardScheduleDefinition(GuardSchedule schedule){auto index=unsigned(schedule);return guardScheduleCatalog[index<guardScheduleCatalog.size()?index:0];}
constexpr bool validGuardScheduleCatalog(){
 for(std::size_t i=0;i<guardScheduleCatalog.size();++i){const auto&row=guardScheduleCatalog[i];if(!ai::validScheduleDefinition(row.schedule)||row.schedule.id!=int(i)||std::size_t(row.schedule.failureSchedule)>=guardScheduleCatalog.size()||(row.schedule.interruptMask&~GuardAllConditions))return false;}
 return true;
}
static_assert(validGuardScheduleCatalog());
static_assert(int(GuardTaskStatus::New)==int(ai::TaskStatus::New)&&int(GuardTaskStatus::Running)==int(ai::TaskStatus::Running)&&int(GuardTaskStatus::Complete)==int(ai::TaskStatus::Complete)&&int(GuardTaskStatus::Failed)==int(ai::TaskStatus::Failed));
}

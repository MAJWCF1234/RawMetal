#pragma once
#include "../Scheduler.h"
#include "Schedule.h"
#include <array>
namespace retro {
struct CivilianScheduleDefinition {ai::ScheduleDefinition schedule;unsigned work=0;};
inline constexpr std::array<ai::ScheduleTask,3> civilianOrdinaryTasks={{{ai::TaskOperation::Prepare,0,0},{ai::TaskOperation::Move,0,1},{ai::TaskOperation::ExecuteWork,0,2}}};
inline constexpr std::array<ai::ScheduleTask,1> civilianFleeTasks={{{ai::TaskOperation::Escape,0,2}}};
inline constexpr std::array<ai::ScheduleTask,1> civilianHoldTasks={{{ai::TaskOperation::Wait,60000,2}}};
inline constexpr std::array<ai::ScheduleTask,1> civilianRecoveryTasks={{{ai::TaskOperation::Wait,400,2}}};
inline constexpr unsigned civilianFearInterrupts=CivilianInFear|CivilianHurt;
inline constexpr unsigned civilianFollowInterrupts=CivilianInFear|CivilianHurt|CivilianShouldFollow;
inline constexpr unsigned civilianAllSounds=ai::SoundWorld|ai::SoundWeapon|ai::SoundImpact|ai::SoundDanger;
inline constexpr std::array<CivilianScheduleDefinition,6> civilianScheduleCatalog={{
 {{int(CivilianSchedule::None),"None",{},int(CivilianSchedule::None),0,0},0},
 {{int(CivilianSchedule::Idle),"Idle",civilianHoldTasks,int(CivilianSchedule::Recovery),civilianFearInterrupts|CivilianShouldFollow,civilianAllSounds},civilianTaskBit(CivilianTask::Hold)},
 {{int(CivilianSchedule::Follow),"Follow",civilianOrdinaryTasks,int(CivilianSchedule::Recovery),civilianFollowInterrupts,civilianAllSounds},civilianTaskBit(CivilianTask::Hold)},
 {{int(CivilianSchedule::Flee),"Flee",civilianFleeTasks,int(CivilianSchedule::Recovery),CivilianHurt,ai::SoundDanger|ai::SoundWeapon},civilianTaskBit(CivilianTask::Escape)},
 {{int(CivilianSchedule::Hold),"Hold",civilianHoldTasks,int(CivilianSchedule::Idle),civilianFearInterrupts|CivilianShouldFollow,civilianAllSounds},civilianTaskBit(CivilianTask::Hold)},
 {{int(CivilianSchedule::Recovery),"Recovery",civilianRecoveryTasks,int(CivilianSchedule::Idle),civilianFearInterrupts|CivilianShouldFollow,ai::SoundDanger},civilianTaskBit(CivilianTask::Hold)}
}};
constexpr const CivilianScheduleDefinition& civilianScheduleDefinition(CivilianSchedule schedule){
 auto index=unsigned(schedule);return civilianScheduleCatalog[index<civilianScheduleCatalog.size()?index:0];
}
constexpr bool validCivilianScheduleCatalog(){
 for(std::size_t i=0;i<civilianScheduleCatalog.size();++i){const auto&row=civilianScheduleCatalog[i];
  if(!ai::validScheduleDefinition(row.schedule)||row.schedule.id!=int(i)||std::size_t(row.schedule.failureSchedule)>=civilianScheduleCatalog.size()||(row.schedule.interruptMask&~CivilianAllConditions))return false;}
 return true;
}
static_assert(validCivilianScheduleCatalog());
static_assert(int(CivilianTaskStatus::New)==int(ai::TaskStatus::New)&&int(CivilianTaskStatus::Running)==int(ai::TaskStatus::Running)&&int(CivilianTaskStatus::Complete)==int(ai::TaskStatus::Complete)&&int(CivilianTaskStatus::Failed)==int(ai::TaskStatus::Failed));
}

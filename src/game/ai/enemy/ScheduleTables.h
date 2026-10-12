#pragma once
#include "../Scheduler.h"
#include "Schedule.h"
#include <array>
namespace retro {
struct CreatureScheduleDefinition {ai::ScheduleDefinition schedule;unsigned work=0;};
inline constexpr std::array<ai::ScheduleTask,3> creatureOrdinaryTasks={{{ai::TaskOperation::Prepare,0,0},{ai::TaskOperation::Move,0,1},{ai::TaskOperation::ExecuteWork,0,2}}};
inline constexpr std::array<ai::ScheduleTask,2> creatureSearchTasks={{{ai::TaskOperation::Prepare,0,0},{ai::TaskOperation::Wait,1400,2}}};
inline constexpr std::array<ai::ScheduleTask,1> creatureAttackTasks={{{ai::TaskOperation::ExecuteWork,0,2}}};
inline constexpr std::array<ai::ScheduleTask,1> creatureRecoveryTasks={{{ai::TaskOperation::Wait,400,2}}};
inline constexpr unsigned creatureSenseInterrupts=CreatureSeesThreat|CreatureHeardSound|CreatureHurt|CreatureLostTarget|CreatureCanMelee;
// Lost contact is owned by Enemy::State (Chase→Search/Idle). Chase only
// interrupts for pain, melee commit and fresh non-target sounds.
inline constexpr unsigned creatureChaseInterrupts=CreatureHurt|CreatureCanMelee|CreatureHeardSound;
inline constexpr unsigned creatureAllSounds=ai::SoundWorld|ai::SoundWeapon|ai::SoundImpact|ai::SoundDanger;
inline constexpr unsigned creatureMeleeWork=creatureTaskBit(CreatureTask::Hold)|creatureTaskBit(CreatureTask::Strike);
// Rows stay indexed by CreatureSchedule. Enemy::State maps onto Idle..Chase;
// Attack and Recovery are executor-only overlays that do not widen the saved
// state enum used by older saves. Idle still walks home through Move before Hold.
// Chase must not interrupt on SeesThreat: continuous sight is the normal chase
// condition, not a schedule change.
inline constexpr std::array<CreatureScheduleDefinition,7> creatureScheduleCatalog={{
 {{int(CreatureSchedule::None),"None",{},int(CreatureSchedule::None),0,0},0},
 {{int(CreatureSchedule::Idle),"Idle",creatureOrdinaryTasks,int(CreatureSchedule::Recovery),creatureSenseInterrupts,creatureAllSounds},creatureTaskBit(CreatureTask::Hold)},
 {{int(CreatureSchedule::Investigate),"Investigate",creatureOrdinaryTasks,int(CreatureSchedule::Recovery),creatureSenseInterrupts|CreatureCanMelee|CreatureOccluded,creatureAllSounds},creatureMeleeWork},
 {{int(CreatureSchedule::Search),"Search",creatureSearchTasks,int(CreatureSchedule::Idle),creatureSenseInterrupts,creatureAllSounds},creatureTaskBit(CreatureTask::Hold)},
 {{int(CreatureSchedule::Chase),"Chase",creatureOrdinaryTasks,int(CreatureSchedule::Recovery),creatureChaseInterrupts,ai::SoundWeapon|ai::SoundDanger},creatureMeleeWork},
 {{int(CreatureSchedule::Attack),"Attack",creatureAttackTasks,int(CreatureSchedule::Chase),CreatureHurt|CreatureLostTarget,ai::SoundDanger},creatureTaskBit(CreatureTask::Strike)},
 {{int(CreatureSchedule::Recovery),"Recovery",creatureRecoveryTasks,int(CreatureSchedule::Idle),CreatureSeesThreat|CreatureHeardSound|CreatureHurt|CreatureCanMelee,ai::SoundDanger},creatureTaskBit(CreatureTask::Hold)}
}};
constexpr const CreatureScheduleDefinition& creatureScheduleDefinition(CreatureSchedule schedule){
 auto index=unsigned(schedule);return creatureScheduleCatalog[index<creatureScheduleCatalog.size()?index:0];
}
constexpr bool validCreatureScheduleCatalog(){
 for(std::size_t i=0;i<creatureScheduleCatalog.size();++i){const auto&row=creatureScheduleCatalog[i];
  if(!ai::validScheduleDefinition(row.schedule)||row.schedule.id!=int(i)||std::size_t(row.schedule.failureSchedule)>=creatureScheduleCatalog.size()||(row.schedule.interruptMask&~CreatureAllConditions))return false;}
 return true;
}
static_assert(validCreatureScheduleCatalog());
static_assert(int(CreatureTaskStatus::New)==int(ai::TaskStatus::New)&&int(CreatureTaskStatus::Running)==int(ai::TaskStatus::Running)&&int(CreatureTaskStatus::Complete)==int(ai::TaskStatus::Complete)&&int(CreatureTaskStatus::Failed)==int(ai::TaskStatus::Failed));
}

#pragma once
#include <cstdint>
namespace retro {
// Civilian/Escort and scripted-handoff schedules share one catalog. Flee is the
// Escape list; Follow/Idle keep Prepare/Move/Hold compatible with saved phases.
enum class CivilianSchedule {None,Idle,Follow,Flee,Hold,Recovery};
enum class CivilianTask {None,Prepare,Move,Hold,Escape};
enum class CivilianTaskStatus {New,Running,Complete,Failed};
enum CivilianCondition : unsigned {
 CivilianInFear=1,CivilianShouldFollow=2,CivilianEngaged=4,CivilianHurt=8,CivilianNearPlayer=16
};
constexpr unsigned CivilianAllConditions=(1u<<5)-1;
constexpr unsigned civilianTaskBit(CivilianTask task){return 1u<<unsigned(task);}
struct CivilianScheduleChoice {
 CivilianSchedule schedule=CivilianSchedule::Idle;
 CivilianTask work=CivilianTask::Hold;
};
struct CivilianTaskPolicy {unsigned interrupts,work;};
CivilianTaskPolicy civilianTaskPolicy(CivilianSchedule schedule);
}

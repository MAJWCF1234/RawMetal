#pragma once
#include <cstdint>
namespace retro {
enum class GuardSchedule {None,Patrol,Investigate,Search,Combat,Cover,Danger,Medic,Engineer,Recovery};
enum class GuardTask {None,Prepare,Move,Hold,Fire,Bash,Reload,Throw,Treat,Deploy,Repair,Escape};
enum class GuardTaskStatus {New,Running,Complete,Failed};
enum class GuardTaskReason {None,Complete,Danger,ThreatChanged,Defense,TargetLost,Blocked,Unavailable,Disabled};
enum GuardCondition : unsigned {
 GuardHasThreat=1,GuardSeesThreat=2,GuardNewThreat=4,GuardHurt=8,GuardNeedsReload=16,
 GuardInDanger=32,GuardHasPatient=64,GuardHasFleet=128,GuardNeedsRepair=256,GuardCloseThreat=512
};
constexpr unsigned guardTaskBit(GuardTask task){return 1u<<unsigned(task);}
struct GuardTaskPolicy {unsigned interrupts,work;};
constexpr GuardTaskPolicy guardTaskPolicy(GuardSchedule schedule){
 constexpr unsigned armed=guardTaskBit(GuardTask::Hold)|guardTaskBit(GuardTask::Fire)|guardTaskBit(GuardTask::Bash)|guardTaskBit(GuardTask::Reload)|guardTaskBit(GuardTask::Throw);
 switch(schedule){
 case GuardSchedule::Danger:return {0,guardTaskBit(GuardTask::Escape)};
 case GuardSchedule::Cover:return {GuardInDanger|GuardNewThreat,armed};
 case GuardSchedule::Recovery:return {GuardInDanger|GuardNewThreat,guardTaskBit(GuardTask::Hold)};
 case GuardSchedule::Medic:return {GuardInDanger|GuardNewThreat|GuardHurt|GuardCloseThreat|GuardNeedsReload,guardTaskBit(GuardTask::Hold)|guardTaskBit(GuardTask::Treat)};
 case GuardSchedule::Engineer:return {GuardInDanger|GuardNewThreat|GuardHurt|GuardCloseThreat|GuardNeedsReload,armed|guardTaskBit(GuardTask::Deploy)|guardTaskBit(GuardTask::Repair)};
 default:return {GuardInDanger|GuardNewThreat|GuardHurt|GuardNeedsReload,armed};
 }
}
}

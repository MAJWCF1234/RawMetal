#pragma once
#include <cstdint>
namespace retro {
enum class GuardSchedule {None,Patrol,Investigate,Search,Combat,Cover,Danger,Medic,Engineer,Recovery,Patient};
enum class GuardTask {None,Prepare,Move,Hold,Fire,Bash,Reload,Throw,Treat,Deploy,Repair,Escape};
enum class GuardTaskStatus {New,Running,Complete,Failed};
enum class GuardTaskReason {None,Complete,Danger,ThreatChanged,Defense,TargetLost,Blocked,Unavailable,Disabled};
struct GuardScheduleChoice {
 GuardSchedule schedule=GuardSchedule::Patrol;
 GuardTask work=GuardTask::Hold;
 int subject=0;
 bool danger=false;
};
enum GuardCondition : unsigned {
 GuardHasThreat=1,GuardSeesThreat=2,GuardNewThreat=4,GuardHurt=8,GuardNeedsReload=16,
 GuardInDanger=32,GuardHasPatient=64,GuardHasFleet=128,GuardNeedsRepair=256,GuardCloseThreat=512,
 GuardHeardSound=1024,GuardEnemyOccluded=2048,GuardEnemyDead=4096,GuardCanRangeAttack=8192,
 GuardCanMeleeAttack=16384,GuardLightDamage=32768,GuardHeavyDamage=65536
};
constexpr unsigned GuardAllConditions=(1u<<17)-1;
constexpr unsigned guardTaskBit(GuardTask task){return 1u<<unsigned(task);}
struct GuardTaskPolicy {unsigned interrupts,work;};
GuardTaskPolicy guardTaskPolicy(GuardSchedule schedule);
}

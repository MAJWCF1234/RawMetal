#pragma once
#include <cstdint>
namespace retro {
// Creature schedules mirror Enemy::State identities for save compatibility.
// Attack is the committed melee work list while Chase owns approach.
enum class CreatureSchedule {None,Idle,Investigate,Search,Chase,Attack,Recovery};
enum class CreatureTask {None,Prepare,Move,Hold,Strike};
enum class CreatureTaskStatus {New,Running,Complete,Failed};
enum CreatureCondition : unsigned {
 CreatureHasThreat=1,CreatureSeesThreat=2,CreatureHeardSound=4,CreatureCanMelee=8,
 CreatureLostTarget=16,CreatureHurt=32,CreatureOccluded=64
};
constexpr unsigned CreatureAllConditions=(1u<<7)-1;
constexpr unsigned creatureTaskBit(CreatureTask task){return 1u<<unsigned(task);}
struct CreatureScheduleChoice {
 CreatureSchedule schedule=CreatureSchedule::Idle;
 CreatureTask work=CreatureTask::Hold;
 bool arrive=false;
};
struct CreatureTaskPolicy {unsigned interrupts,work;};
CreatureTaskPolicy creatureTaskPolicy(CreatureSchedule schedule);
}

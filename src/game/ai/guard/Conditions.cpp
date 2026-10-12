#include "../../Game.h"
#include "ScheduleTables.h"
namespace retro {
unsigned Game::guardConditions(size_t actor,const ActorPose&pose)const{
 if(actor>=m_friendlyActors.size())return 0;
 const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};
 unsigned conditions=0;auto target=guardTarget(actor);
 if(target.valid())conditions|=GuardHasThreat;
 bool seen=target.valid()&&get("guard_seen")&&guardCanSee(actor,pose,target);
 if(seen)conditions|=GuardSeesThreat;
 else if(target.valid()&&get("guard_memory"))conditions|=GuardEnemyOccluded;
 if(get("guard_enemy_dead"))conditions|=GuardEnemyDead;
 if(get("guard_under_fire"))conditions|=GuardHurt;
 int hurt=get("guard_hurt_amount");
 if(hurt>0)conditions|=hurt>=std::max(10,int(std::ceil(track.health*.20f)))?GuardHeavyDamage:GuardLightDamage;
 if(get("weapon_reload")||get("weapon_reloading")||(track.ai.armed&&get("weapon_saved")&&!get("weapon_rounds")))conditions|=GuardNeedsReload;
 auto schedule=GuardSchedule(get("schedule"));
 if(schedule==GuardSchedule::None)schedule=GuardSchedule::Patrol;
 if(ai::acceptedScheduleSounds(guardScheduleDefinition(schedule).schedule,unsigned(get("guard_heard"))))conditions|=GuardHeardSound;
 if(get("guard_danger_ms"))conditions|=GuardInDanger;
 if(get("support_target"))conditions|=GuardHasPatient;
 bool close=seen&&guardMeleeReady(actor,pose,target);
 if(close)conditions|=GuardCloseThreat;
 // Attack conditions report physically possible work. They do not depend on
 // the current task lease or already advanced aim/raise clocks, and cannot
 // reveal a remembered target through occlusion.
 bool recovered=!get("weapon_recovery")&&!get("weapon_bash_pose")&&!get("grenade_windup")&&!get("drone_busy")&&!get("support_target");
 bool actionReady=get("weapon_action")==int(GuardAction::None);
 if(track.ai.armed&&seen&&get("guard_confirmed")&&recovered&&close&&(actionReady||get("weapon_action")==int(GuardAction::Reload)))conditions|=GuardCanMeleeAttack;
 if(!track.ai.armed||!seen||!get("guard_confirmed")||!recovered||!actionReady)return conditions;
 bool loaded=!get("weapon_saved")||get("weapon_rounds")>0;
 if(!loaded||get("weapon_reload")||get("weapon_reloading"))return conditions;
 Point3 point{target.position.x,target.position.y,target.feet+target.height*.6f};
 auto delta=point-Point3{pose.position.x,pose.position.y,pose.z+track.scale*.85f};
 if(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z>=track.ai.attackRange*track.ai.attackRange)return conditions;
 if(guardShotMatches(actor,guardShot(actor,pose,npcWeaponPoseAt(actor,pose,point,1))))conditions|=GuardCanRangeAttack;
 return conditions;
}
}

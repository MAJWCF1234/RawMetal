#include "../../../core/FrameProfile.h"
#include "../../Game.h"
#include "../engineer/Drones.h"
#include "ScheduleTables.h"
#include <array>
namespace retro {
namespace {
enum class GuardRolePlan {Ordinary,Medic,Engineer};
constexpr std::array<GuardSchedule,5> guardModeSchedules={GuardSchedule::Patrol,GuardSchedule::Investigate,GuardSchedule::Investigate,GuardSchedule::Search,GuardSchedule::Combat};
constexpr std::array<GuardRolePlan,7> guardRolePlans={GuardRolePlan::Ordinary,GuardRolePlan::Ordinary,GuardRolePlan::Engineer,GuardRolePlan::Ordinary,GuardRolePlan::Ordinary,GuardRolePlan::Medic,GuardRolePlan::Ordinary};
}
GuardScheduleChoice Game::guardScheduleChoice(int level,size_t actor,const ActorTrack&track,ActorPose&pose,float dt,int ms,Vec2&goal,float&goalZ,bool&wantsMove,unsigned&conditions,GuardSchedule previous,int oldSubject,int retry,bool changed,bool close,const AiTarget&target){
 perf::Scope frameProfile(perf::Intent);
 auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*f){return ctx.get(f);};GuardScheduleChoice choice;
 // Authored evacuation uses the existing exclusive Escape task and physical
 // stitched navigator. It never changes an actor's health or teleports it.
 if(int destination=get("evac_level")-1;destination>=0&&destination<chunkCount()){
  Vec2 local{get("evac_x")*.001f,get("evac_y")*.001f};float feet=get("evac_z")*.001f;
  bool arrived=level==destination&&length(pose.position-local)<.35f&&std::fabs(pose.z-feet)<.3f;
  ctx.put("evacuated",arrived?1:0);wantsMove=!arrived;
  if(wantsMove)guardRoute(level,actor,chunkOffset(destination)+local,feet,ms,goal,goalZ);
  else{goal=pose.position;goalZ=pose.z;}
  choice.danger=true;conditions|=GuardInDanger;choice.schedule=GuardSchedule::Danger;choice.work=GuardTask::Escape;return choice;
 }
 // Perception chooses intent before any task can own movement or spend work.
 choice.danger=guardDangerIntent(actor,pose,dt,goal,goalZ,wantsMove);
 if(choice.danger){conditions|=GuardInDanger;choice.schedule=GuardSchedule::Danger;choice.work=GuardTask::Escape;return choice;}
 if(previous==GuardSchedule::Recovery&&get("task_status")==int(GuardTaskStatus::Running)&&get("task_ms")<guardScheduleDefinition(GuardSchedule::Recovery).schedule.tasks.front().argument&&!(conditions&(GuardNewThreat|GuardHeavyDamage))){
  // Awareness deadlines still run during backoff. A newly identified threat
  // or fresh heavy damage can choose ordinary defense immediately.
  guardBaseIntent(level,actor,track,pose,ms,goal,goalZ,wantsMove);
  choice.schedule=GuardSchedule::Recovery;goal=pose.position;goalZ=pose.z;wantsMove=false;return choice;
 }
 guardBaseIntent(level,actor,track,pose,ms,goal,goalZ,wantsMove);
 auto mode=unsigned(guardState(actor));choice.schedule=mode<guardModeSchedules.size()?guardModeSchedules[mode]:GuardSchedule::Investigate;
 bool defensive=track.ai.armed&&((conditions&(GuardHurt|GuardNeedsReload))||(target.valid()&&((get("guard_cover_mode")==2&&get("guard_cover"))||(track.health>0&&state(track.damageState)>=track.health*.65f))));
 if(defensive)choice.schedule=GuardSchedule::Cover;
 bool maySupport=!defensive&&!(conditions&GuardCloseThreat)&&!(changed&&previous!=GuardSchedule::None);
 int medic=-1;bool patient=maySupport&&guardPatientIntent(level,actor,track,pose,ms,goal,goalZ,wantsMove,medic);
 if(patient){choice.schedule=GuardSchedule::Patient;choice.subject=medic+1;}
 auto planMedic=[&](){
  guardSupportIntent(level,actor,track,pose,dt,goal,goalZ,wantsMove,false);
  if(get("support_target")){choice.schedule=GuardSchedule::Medic;choice.subject=get("support_target");conditions|=GuardHasPatient;
   bool ready=!get("weapon_recovery")&&!get("weapon_bash")&&!get("grenade_windup")&&get("weapon_action")!=int(GuardAction::Fire)&&get("weapon_action")!=int(GuardAction::Bash);
   choice.work=ready?GuardTask::Treat:GuardTask::Hold;
  }
 };
 auto planEngineer=[&](){
  guardEngineerIntent(level,actor,track,pose,ms,goal,goalZ,wantsMove);
  int deployed=0,repair=-1;float priority=-1;auto origin=chunkOffset(level),active=chunkOffset(m_level);
  float standOff=std::min(8.f,std::max(6.f,track.ai.attackRange*.6f));bool rear=!get("guard_engineer_active")||length(pose.position-Vec2{get("guard_x")*.001f-origin.x,get("guard_y")*.001f-origin.y})>=standOff-.5f||guardCovered(actor,pose,target);
  bool mechanical=!get("weapon_reload")&&!get("weapon_reloading")&&!get("weapon_recovery")&&!get("weapon_bash")&&!get("weapon_bash_pose")&&!get("grenade_windup")&&get("weapon_action")==int(GuardAction::None);
  for(const auto&drone:engineerDrones())if(drone.owner==int(actor)){
   ++deployed;auto p=drone.position+Point3{chunkOffset(drone.level).x-origin.x,chunkOffset(drone.level).y-origin.y,0};
   if(!rear||!mechanical||!get("drone_repair_charge")||drone.walking||drone.health>=EngineerDroneHealth||length(Vec2{p.x,p.y}-pose.position)>=2||std::fabs(p.z-pose.z)>=.3f)continue;
   Point3 hand{pose.position.x+origin.x-active.x,pose.position.y+origin.y-active.y,pose.z+track.scale*.65f},eye{p.x+origin.x-active.x,p.y+origin.y-active.y,p.z+EngineerDroneHeight*.85f};
   if(traceEngineerDrone(hand,eye,actor).kind==ShotKind::Solid)continue;
   float score=EngineerDroneHealth-drone.health+(oldSubject==drone.slot+1?1.f:0.f);if(score>priority){priority=score;repair=drone.slot;}
  }
  if(deployed)conditions|=GuardHasFleet;
  if(repair>=0){choice.schedule=GuardSchedule::Engineer;choice.work=GuardTask::Repair;choice.subject=repair+1;conditions|=GuardNeedsRepair;goal=pose.position;goalZ=pose.z;wantsMove=false;}
  else if(get("guard_engineer_active")||get("drone_windup")||(rear&&mechanical&&get("guard_confirmed")&&get("guard_memory")&&target.valid()&&get("guard_seen")&&get("drone_used")<EngineerDroneCapacity&&!get("drone_cooldown"))){
   choice.schedule=GuardSchedule::Engineer;
   bool justified=get("guard_confirmed")&&get("guard_memory")&&target.valid()&&(get("guard_seen")||deployed>0);
   if(justified&&rear&&mechanical&&deployed<EngineerDroneCapacity&&get("drone_used")<EngineerDroneCapacity&&!get("drone_cooldown"))choice.work=GuardTask::Deploy;
   if(get("drone_windup")){goal=pose.position;goalZ=pose.z;wantsMove=false;}
  }
 };
 if(maySupport&&!patient){auto role=unsigned(track.ai.role);auto planner=role<guardRolePlans.size()?guardRolePlans[role]:GuardRolePlan::Ordinary;switch(planner){
  case GuardRolePlan::Medic:planMedic();break;
  case GuardRolePlan::Engineer:planEngineer();break;
  case GuardRolePlan::Ordinary:break;
 }}
 auto planFirearm=[&](){
  if(choice.work!=GuardTask::Hold||!track.ai.armed||choice.schedule==GuardSchedule::Medic)return;
  if(close&&!get("weapon_recovery"))choice.work=GuardTask::Bash;
  else if(conditions&GuardNeedsReload)choice.work=GuardTask::Reload;
  else{Point3 suppress;bool attack=get("guard_seen")||guardSuppressionPoint(actor,suppress);if(attack)choice.work=GuardTask::Fire;}
  float distance=length(target.position-pose.position);
  // A grenade retry cannot replace an already raised/aiming firearm task.
  bool firing=get("task")==int(GuardTask::Fire)&&get("schedule_step")==2&&get("task_status")==int(GuardTaskStatus::Running)&&(get("weapon_raise")||get("weapon_aim"));
  if(choice.schedule!=GuardSchedule::Patient&&choice.work==GuardTask::Fire&&!firing&&track.ai.grenades>get("grenade_used")&&!get("grenade_cooldown")&&!retry&&get("guard_confirmed")&&get("guard_memory")&&(get("guard_seen")||(get("grenade_seen")&&get("grenade_memory_ms")<=1500))&&distance>=4.75f&&distance<=11.f&&!get("weapon_recovery")&&!get("weapon_bash_pose")&&get("weapon_action")==int(GuardAction::None)&&!wantsMove)choice.work=GuardTask::Throw;
 };
 planFirearm();
 if(choice.work==GuardTask::Bash){goal=pose.position;goalZ=pose.z;wantsMove=false;}
 guardAvoidGrenade(actor,pose,goal,goalZ,wantsMove);
 return choice;
}
}

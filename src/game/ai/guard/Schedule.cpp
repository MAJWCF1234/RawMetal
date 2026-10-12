#include "../../Game.h"
#include "../engineer/Drones.h"
namespace retro {
namespace {
StateId taskDroneKey(int slot,std::string_view field){auto text="drone/slot/"+std::to_string(slot)+"/"+std::string(field);StateId key=2166136261u;for(unsigned char c:text)key=(key^c)*16777619u;return key|0x80000000u;}
}
bool Game::guardTaskAllows(size_t actor,GuardTask task)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};
 // Direct mechanical helpers and older saves retain their existing contract
 // until the live intent pass adopts them.
 if(get("schedule")==int(GuardSchedule::None))return true;
 return (guardTaskPolicy(GuardSchedule(get("schedule"))).work&guardTaskBit(task))&&get("schedule_step")==2&&get("task_status")==int(GuardTaskStatus::Running)&&get("task")==int(task);
}
void Game::clearGuardSchedule(size_t actor){
 for(const char*f:{"schedule","schedule_step","task","task_status","task_ms","task_fraction","task_stall","task_progress","task_retry","task_failures","task_reason","task_goal_x","task_goal_y","task_goal_z","task_enemy_kind","task_enemy_level","task_enemy_index","task_subject","task_work_start","task_conditions","task_transitions","support_target","support_ms","drone_windup","drone_busy","drone_repair_target","drone_repair_ms","guard_slot","guard_slot_ms"})setState(friendlyKey(actor,f),0);
}
void Game::failGuardTask(size_t actor,GuardTaskReason reason){
 auto ctx=makeGuardCtx(actor,m_friendlyActors[actor].level,m_friendlyActors[actor].track(),m_friendlyActors[actor].pose);
 auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 put("task_failures",std::min(255,get("task_failures")+1));put("task_reason",int(reason));
 // Release reservations and uncommitted work. Reload/event masks and finite
 // spent resources survive interruption and retry.
 for(const char*f:{"support_target","support_ms","drone_windup","drone_busy","drone_repair_target","drone_repair_ms","grenade_windup","weapon_aim","weapon_bash","guard_slot","guard_slot_ms","guard_cover","guard_cover_mode","guard_cover_wait","guard_route_next","guard_route_wait","guard_engineer_goal","guard_engineer_wait","repath","stuck"})put(f,0);
 if(get("weapon_action")==int(GuardAction::Bash)&&get("weapon_action_ms")<300){put("weapon_action",0);put("weapon_action_ms",0);put("weapon_action_events",0);}
 if(reason==GuardTaskReason::Blocked&&get("schedule")==int(GuardSchedule::Patrol)){put("guard_point",(get("guard_point")+1)%int(ctx.track.ai.patrol.size()));put("guard_arrived",0);put("guard_pause",0);put("guard_patrol_wait",0);}
 put("schedule",int(GuardSchedule::Recovery));put("schedule_step",2);put("task",int(GuardTask::Hold));put("task_status",int(GuardTaskStatus::Running));put("task_ms",0);put("task_stall",0);put("task_progress",0);put("task_subject",0);put("task_work_start",0);
 put("task_retry",reason==GuardTaskReason::Unavailable?std::min(5000,ctx.track.ai.aimMs+1000):700);
}
void Game::runGuardSchedule(int level,size_t actor,const ActorTrack&track,ActorPose&pose,float dt,int ms,Vec2&goal,float&goalZ,bool&wantsMove){
 auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 auto previous=GuardSchedule(get("schedule"));int oldSubject=get("task_subject");
 auto target=guardTarget(actor);unsigned conditions=0;
 if(target.valid())conditions|=GuardHasThreat;if(get("guard_seen"))conditions|=GuardSeesThreat;
 bool changed=target.valid()&&(get("task_enemy_kind")!=int(target.kind)||get("task_enemy_level")!=target.level+1||get("task_enemy_index")!=target.index+1);
 if(changed)conditions|=GuardNewThreat;if(get("guard_under_fire"))conditions|=GuardHurt;
 if(get("weapon_reload")||get("weapon_reloading")||(track.ai.armed&&get("weapon_saved")&&!get("weapon_rounds")))conditions|=GuardNeedsReload;
 bool close=get("guard_seen")&&guardMeleeReady(actor,pose,target);if(close)conditions|=GuardCloseThreat;
 put("task_enemy_kind",target.valid()?int(target.kind):0);put("task_enemy_level",target.valid()?target.level+1:0);put("task_enemy_index",target.valid()?target.index+1:0);
 put("task_transitions",0);int retry=std::max(0,get("task_retry")-ms);put("task_retry",retry);
 GuardSchedule desired;GuardTask work=GuardTask::Hold;int subject=0;
 // One sensory danger pass owns its clock and cached escape, before ordinary
 // navigation or work can consume time or finite resources.
 bool danger=guardDangerIntent(actor,pose,dt,goal,goalZ,wantsMove);
 if(danger){conditions|=GuardInDanger;desired=GuardSchedule::Danger;work=GuardTask::Escape;}
 else if(previous==GuardSchedule::Recovery&&get("task_status")==int(GuardTaskStatus::Running)&&get("task_ms")<700&&!(conditions&GuardNewThreat)){
  // Awareness deadlines continue while the movement job backs off. Retry
  // cannot renew unseen knowledge or postpone forgetting indefinitely.
  guardBaseIntent(level,actor,track,pose,ms,goal,goalZ,wantsMove);
  desired=GuardSchedule::Recovery;goal=pose.position;goalZ=pose.z;wantsMove=false;
 }
 else{
  guardBaseIntent(level,actor,track,pose,ms,goal,goalZ,wantsMove);
  auto mode=guardState(actor);desired=mode==GuardState::Patrol?GuardSchedule::Patrol:mode==GuardState::Search?GuardSchedule::Search:mode==GuardState::Alert?GuardSchedule::Combat:GuardSchedule::Investigate;
  bool defensive=track.ai.armed&&((conditions&(GuardHurt|GuardNeedsReload))||(target.valid()&&(get("guard_cover_mode")==2||(track.health>0&&state(track.damageState)>=track.health*.65f))));
  if(defensive)desired=GuardSchedule::Cover;
  bool maySupport=!defensive&&!(conditions&GuardCloseThreat)&&!(changed&&previous!=GuardSchedule::None);
  if(maySupport&&track.ai.role==GuardSquadRole::Medic){
   guardSupportIntent(level,actor,track,pose,dt,goal,goalZ,wantsMove,false);
   if(get("support_target")){desired=GuardSchedule::Medic;subject=get("support_target");conditions|=GuardHasPatient;
    bool ready=!get("weapon_recovery")&&!get("weapon_bash")&&!get("grenade_windup")&&get("weapon_action")!=int(GuardAction::Fire)&&get("weapon_action")!=int(GuardAction::Bash);
    work=ready?GuardTask::Treat:GuardTask::Hold;
   }
  }
  if(maySupport&&track.ai.role==GuardSquadRole::Engineer){
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
   if(repair>=0){desired=GuardSchedule::Engineer;work=GuardTask::Repair;subject=repair+1;conditions|=GuardNeedsRepair;goal=pose.position;goalZ=pose.z;wantsMove=false;}
   else if(get("guard_engineer_active")||get("drone_windup")||(rear&&mechanical&&get("guard_confirmed")&&get("guard_memory")&&target.valid()&&get("guard_seen")&&get("drone_used")<EngineerDroneCapacity&&!get("drone_cooldown"))){
    desired=GuardSchedule::Engineer;
    bool justified=get("guard_confirmed")&&get("guard_memory")&&target.valid()&&(get("guard_seen")||deployed>0);
    if(justified&&rear&&mechanical&&deployed<EngineerDroneCapacity&&get("drone_used")<EngineerDroneCapacity&&!get("drone_cooldown"))work=GuardTask::Deploy;
    if(get("drone_windup")){goal=pose.position;goalZ=pose.z;wantsMove=false;}
   }
  }
  if(work==GuardTask::Hold&&track.ai.armed&&desired!=GuardSchedule::Medic){
   if(close&&!get("weapon_recovery"))work=GuardTask::Bash;
   else if(conditions&GuardNeedsReload)work=GuardTask::Reload;
   else{Point3 suppress;bool attack=get("guard_seen")||guardSuppressionPoint(actor,suppress);if(attack)work=GuardTask::Fire;}
   float distance=length(target.position-pose.position);
   // A grenade retry cannot replace an already raised/aiming owned firearm
   // task. Finish that finite shot or handle a real interruption first.
   bool firing=get("task")==int(GuardTask::Fire)&&get("schedule_step")==2&&get("task_status")==int(GuardTaskStatus::Running)&&(get("weapon_raise")||get("weapon_aim"));
   if(work==GuardTask::Fire&&!firing&&track.ai.grenades>get("grenade_used")&&!get("grenade_cooldown")&&!retry&&get("guard_confirmed")&&get("guard_memory")&&(get("guard_seen")||(get("grenade_seen")&&get("grenade_memory_ms")<=1500))&&distance>=4.75f&&distance<=11.f&&!get("weapon_recovery")&&!get("weapon_bash_pose")&&get("weapon_action")==int(GuardAction::None)&&!wantsMove)work=GuardTask::Throw;
  }
  if(work==GuardTask::Bash){goal=pose.position;goalZ=pose.z;wantsMove=false;}
  guardAvoidGrenade(actor,pose,goal,goalZ,wantsMove);
 }
 put("task_conditions",int(conditions));unsigned interrupts=conditions&guardTaskPolicy(previous).interrupts;
 bool interrupted=previous!=desired||oldSubject!=subject||(interrupts&(GuardInDanger|GuardNewThreat))||(desired==GuardSchedule::Cover&&(previous==GuardSchedule::Medic||previous==GuardSchedule::Engineer));
 if(interrupted){
  GuardTaskReason reason=danger?GuardTaskReason::Danger:changed?GuardTaskReason::ThreatChanged:desired==GuardSchedule::Cover?GuardTaskReason::Defense:previous==GuardSchedule::Medic&&desired!=GuardSchedule::Medic?GuardTaskReason::TargetLost:GuardTaskReason::None;
  put("task_reason",int(reason));put("task_stall",0);put("task_progress",0);
  if(previous!=GuardSchedule::None||danger||desired==GuardSchedule::Cover){
   put("weapon_aim",0);put("guard_slot",0);put("guard_slot_ms",0);
   if(desired!=GuardSchedule::Medic||oldSubject!=subject){put("support_ms",0);if(desired!=GuardSchedule::Medic)put("support_target",0);}
   if(work!=GuardTask::Deploy)put("drone_windup",0);
   if(work!=GuardTask::Repair||oldSubject!=subject){put("drone_repair_ms",0);put("drone_repair_target",0);put("drone_busy",0);}
   if(work!=GuardTask::Throw)put("grenade_windup",0);
   if(work!=GuardTask::Bash){put("weapon_bash",0);if(get("weapon_action")==int(GuardAction::Bash)&&get("weapon_action_ms")<300){put("weapon_action",0);put("weapon_action_ms",0);put("weapon_action_events",0);}}
  }
  put("schedule",int(desired));put("schedule_step",0);put("task",int(GuardTask::Prepare));put("task_status",int(GuardTaskStatus::New));put("task_ms",0);put("task_subject",subject);
 }
 if(desired!=GuardSchedule::Medic){put("support_target",0);put("support_ms",0);}if(work==GuardTask::Repair)put("drone_repair_target",subject);
 auto global=goal+chunkOffset(level);put("task_goal_x",int(std::round(global.x*1000)));put("task_goal_y",int(std::round(global.y*1000)));put("task_goal_z",int(std::round(goalZ*1000)));
 if(desired==GuardSchedule::Danger){put("schedule_step",2);put("task",int(GuardTask::Escape));put("task_status",int(GuardTaskStatus::Running));put("task_ms",std::min(60000,get("task_ms")+ms));return;}
 // Bound immediate transitions. Movement owns the physical approach, then
 // exactly one consumer owns work; failed/moving actors cannot spend events.
 for(int transition=0;transition<4;++transition){
  int step=get("schedule_step");auto task=GuardTask(get("task"));auto status=GuardTaskStatus(get("task_status"));
  if(step==0){put("schedule_step",1);put("task",int(GuardTask::Move));put("task_status",int(GuardTaskStatus::New));}
  else if(step==1&&!wantsMove){put("schedule_step",2);put("task",int(work));put("task_status",int(GuardTaskStatus::New));}
  else if(step==2&&(wantsMove||task!=work||status==GuardTaskStatus::Complete||status==GuardTaskStatus::Failed)){
   if(wantsMove||task!=work){
    put("weapon_aim",0);put("guard_slot",0);put("guard_slot_ms",0);
    if(task==GuardTask::Throw)put("grenade_windup",0);
    if(task==GuardTask::Deploy){put("drone_windup",0);put("drone_busy",0);}
    if(task==GuardTask::Repair){put("drone_repair_ms",0);put("drone_busy",0);}
    if(task==GuardTask::Treat)put("support_ms",0);
   }
   put("schedule_step",wantsMove?1:2);put("task",int(wantsMove?GuardTask::Move:work));put("task_status",int(GuardTaskStatus::New));
  }
  else break;
  put("task_transitions",transition+1);
 }
 if(get("task_status")==int(GuardTaskStatus::New)){
  put("task_ms",0);put("task_status",int(GuardTaskStatus::Running));
  int baseline=work==GuardTask::Treat?get("support_used"):work==GuardTask::Deploy?get("drone_used"):work==GuardTask::Throw?get("grenade_used"):work==GuardTask::Fire?(get("weapon_saved")?get("weapon_rounds"):weaponDef(guardHandgun(track.ai)?WeaponId::Handgun:WeaponId::Shotgun).cycle.tubeCapacity):work==GuardTask::Repair?get("drone_repair_charge"):0;
  put("task_work_start",baseline);
 }
 put("task_fraction",get("guard_fraction"));put("task_ms",std::min(60000,get("task_ms")+ms));
 if(get("schedule_step")==2&&work==GuardTask::Treat)runGuardTreatment(actor,ms);
}
void Game::finishGuardTask(size_t actor){
 auto ctx=makeGuardCtx(actor,m_friendlyActors[actor].level,m_friendlyActors[actor].track(),m_friendlyActors[actor].pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 if(!get("schedule")||get("schedule_step")!=2||get("task_status")!=int(GuardTaskStatus::Running))return;
 auto task=GuardTask(get("task"));bool done=false;
 switch(task){
 case GuardTask::Treat:done=get("support_used")>get("task_work_start");break;
 case GuardTask::Deploy:done=get("drone_used")>get("task_work_start");break;
 case GuardTask::Fire:done=get("weapon_rounds")<get("task_work_start");break;
 case GuardTask::Throw:done=get("grenade_used")>get("task_work_start");break;
 case GuardTask::Bash:done=get("weapon_bash_pose")>0;break;
 case GuardTask::Reload:done=get("weapon_rounds")>0&&!get("weapon_reload")&&!get("weapon_reloading");break;
 case GuardTask::Repair:{int slot=get("task_subject")-1;done=slot<0||slot>=EngineerDroneSlots||!state(taskDroneKey(slot,"active"))||state(taskDroneKey(slot,"hp"))>=EngineerDroneHealth||!get("drone_repair_charge");break;}
 case GuardTask::Hold:done=get("task_ms")>=700;break;
 default:break;
 }
 if(done){
  if(task==GuardTask::Deploy||task==GuardTask::Repair)put("drone_busy",0);
  if(task==GuardTask::Repair){put("drone_repair_target",0);put("drone_repair_ms",0);}
  put("task_status",int(GuardTaskStatus::Complete));put("task_reason",int(GuardTaskReason::Complete));return;
 }
 // Failed placement/arc work releases its job. The retry interval leaves
 // firearms enough time to finish their authored aim instead of starving.
 bool unavailable=task==GuardTask::Throw&&get("task_ms")>=750;
 unavailable|=task==GuardTask::Deploy&&!get("drone_windup")&&get("drone_cooldown")>0;
 if(unavailable)failGuardTask(actor,GuardTaskReason::Unavailable);
}
void Game::guardTaskMovement(size_t actor,const ActorPose&old,const ActorPose&pose,Vec2 waypoint,bool wanted,int ms){
 auto ctx=makeGuardCtx(actor,m_friendlyActors[actor].level,m_friendlyActors[actor].track(),m_friendlyActors[actor].pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 if(!wanted||get("schedule_step")!=1||get("task_status")!=int(GuardTaskStatus::Running)){put("task_stall",0);put("task_progress",0);return;}
 // Follow the chosen physical waypoint, so valid detours and seam migration
 // do not look like failure merely because target distance grows.
 auto direction=normalized(waypoint-old.position);float advance=std::max(0.f,dot(pose.position-old.position,direction));int progress=get("task_progress")+int(std::round(advance*1000));
 if(progress>=100){put("task_progress",progress%100);put("task_stall",0);return;}put("task_progress",progress);
 bool patrol=get("schedule")==int(GuardSchedule::Patrol);int stall=patrol?get("guard_patrol_wait"):std::min(5000,get("task_stall")+ms);put("task_stall",stall);int deadline=patrol?5000:2500;if(stall>=deadline)failGuardTask(actor,GuardTaskReason::Blocked);
}
void Game::validateGuardSchedule(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto range=[&](const char*f,int lo,int hi){if(get(f)<lo||get(f)>hi)throw std::runtime_error("Invalid saved guard schedule");};
 // Reconstruction validates guards in authoring order. Count the complete
 // authored collection so a subject appearing later can restore normally.
 size_t npcCount=0;for(int level=0;level<chunkCount();++level)for(const auto&track:(level==m_level?m_world:m_chunks[level].world).actorTracks())npcCount+=track.ai.mode!=ActorAiMode::Scripted;
 range("schedule",0,int(GuardSchedule::Recovery));range("schedule_step",0,2);range("task",0,int(GuardTask::Escape));range("task_status",0,int(GuardTaskStatus::Failed));range("task_ms",0,60000);range("task_fraction",0,999);range("task_stall",0,5000);range("task_progress",0,99);range("task_retry",0,5000);range("task_failures",0,255);range("task_reason",0,int(GuardTaskReason::Disabled));range("task_goal_x",-10000000,10000000);range("task_goal_y",-10000000,10000000);range("task_goal_z",-100000,100000);range("task_subject",0,4096);range("task_work_start",0,65535);range("task_conditions",0,1023);range("task_transitions",0,4);range("task_enemy_level",0,chunkCount());range("task_enemy_index",0,65536);
 int kind=get("task_enemy_kind");if(kind&&kind!=int(ShotKind::Player)&&kind!=int(ShotKind::Npc)&&kind!=int(ShotKind::Actor)&&kind!=int(ShotKind::Enemy)&&kind!=int(ShotKind::Drone))throw std::runtime_error("Invalid saved scheduled threat");
 if((kind&&(!get("task_enemy_level")||!get("task_enemy_index")))||(!kind&&(get("task_enemy_level")||get("task_enemy_index"))))throw std::runtime_error("Inconsistent scheduled threat reference");
 if((kind==int(ShotKind::Npc)&&size_t(get("task_enemy_index"))>npcCount)||(kind==int(ShotKind::Player)&&get("task_enemy_index")!=1)||(kind==int(ShotKind::Drone)&&get("task_enemy_index")>EngineerDroneSlots))throw std::runtime_error("Scheduled threat is outside its collection");
 auto schedule=GuardSchedule(get("schedule"));auto task=GuardTask(get("task"));int step=get("schedule_step"),subject=get("task_subject");const auto&track=m_friendlyActors[actor].track();
 if(schedule==GuardSchedule::None){if(task!=GuardTask::None||step||subject||get("task_status"))throw std::runtime_error("Inactive guard has an owned task");return;}
 if(task==GuardTask::None||(step==0&&task!=GuardTask::Prepare)||(step==1&&task!=GuardTask::Move)||(step==2&&(task==GuardTask::Prepare||task==GuardTask::Move)))throw std::runtime_error("Inconsistent saved task phase");
 if(step==2&&!(guardTaskPolicy(schedule).work&guardTaskBit(task)))throw std::runtime_error("Saved task is outside its schedule policy");
 if((schedule==GuardSchedule::Danger)!=(task==GuardTask::Escape))throw std::runtime_error("Invalid saved escape ownership");
 if(schedule==GuardSchedule::Medic&&(track.ai.role!=GuardSquadRole::Medic||subject<=0||size_t(subject)>npcCount||size_t(subject)==actor+1||(step==2&&task!=GuardTask::Treat&&task!=GuardTask::Hold)))throw std::runtime_error("Invalid saved treatment ownership");
 if(schedule==GuardSchedule::Engineer&&track.ai.role!=GuardSquadRole::Engineer)throw std::runtime_error("Invalid saved engineering ownership");
 if(task==GuardTask::Treat&&schedule!=GuardSchedule::Medic)throw std::runtime_error("Treatment has no Medic owner");
 if((task==GuardTask::Deploy||task==GuardTask::Repair)&&schedule!=GuardSchedule::Engineer)throw std::runtime_error("Engineering has no owner");
 if(task==GuardTask::Repair&&(subject<=0||subject>EngineerDroneSlots))throw std::runtime_error("Invalid saved maintenance subject");
 if(schedule!=GuardSchedule::Medic&&task!=GuardTask::Repair&&subject)throw std::runtime_error("Unexpected saved task subject");
 if(task>=GuardTask::Fire&&task<=GuardTask::Repair&&task!=GuardTask::Treat&&!track.ai.armed)throw std::runtime_error("Unarmed guard owns armed work");
}
}

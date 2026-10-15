#include "../../Game.h"
#include "../engineer/Drones.h"
#include "ScheduleTables.h"
namespace retro {
namespace {
StateId taskDroneKey(int slot,std::string_view field){auto text="drone/slot/"+std::to_string(slot)+"/"+std::string(field);StateId key=2166136261u;for(unsigned char c:text)key=(key^c)*16777619u;return key|0x80000000u;}
}
GuardTaskPolicy guardTaskPolicy(GuardSchedule schedule){const auto&row=guardScheduleDefinition(schedule);return {row.schedule.interruptMask,row.work};}
bool Game::guardTaskAllows(size_t actor,GuardTask task)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};
 // Direct mechanical helpers and older saves retain their existing contract
 // until the live intent pass adopts them.
 if(get("schedule")==int(GuardSchedule::None))return true;
 if(get("task_cursor_saved")){
  const auto&definition=guardScheduleDefinition(GuardSchedule(get("schedule"))).schedule;int cursor=get("task_cursor");
  if(cursor<0||size_t(cursor)>=definition.tasks.size())return false;
  auto operation=definition.tasks[size_t(cursor)].operation;
  if(operation!=ai::TaskOperation::ExecuteWork&&!(operation==ai::TaskOperation::Wait&&task==GuardTask::Hold)&&!(operation==ai::TaskOperation::Escape&&task==GuardTask::Escape))return false;
 }
 return (guardTaskPolicy(GuardSchedule(get("schedule"))).work&guardTaskBit(task))&&get("schedule_step")==2&&get("task_status")==int(GuardTaskStatus::Running)&&get("task")==int(task);
}
void Game::clearGuardSchedule(size_t actor){
 releaseGuardPatient(actor);
 for(const char*f:{"engineer_pad_stage","engineer_pad_ms"})setState(friendlyKey(actor,f),0);
 for(const char*f:{"task_cursor","task_cursor_saved","task_fail_schedule","guard_heard","guard_hurt_amount","guard_enemy_dead"})setState(friendlyKey(actor,f),0);
 for(const char*f:{"schedule","schedule_step","task","task_status","task_ms","task_fraction","task_stall","task_progress","task_retry","task_failures","task_reason","task_goal_x","task_goal_y","task_goal_z","task_enemy_kind","task_enemy_level","task_enemy_index","task_subject","task_work_start","task_conditions","task_transitions","support_target","support_ms","drone_windup","drone_busy","drone_repair_target","drone_repair_ms","guard_slot","guard_slot_ms"})setState(friendlyKey(actor,f),0);
}
void Game::failGuardTask(size_t actor,GuardTaskReason reason){
 releaseGuardPatient(actor);
 auto ctx=makeGuardCtx(actor,m_friendlyActors[actor].level,m_friendlyActors[actor].track(),m_friendlyActors[actor].pose);
 auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 put("task_failures",std::min(255,get("task_failures")+1));put("task_reason",int(reason));
 // Release reservations and uncommitted work. Reload/event masks and finite
 // spent resources survive interruption and retry.
 for(const char*f:{"support_target","support_ms","drone_windup","drone_busy","drone_repair_target","drone_repair_ms","grenade_windup","weapon_aim","weapon_bash","guard_slot","guard_slot_ms","guard_cover","guard_cover_mode","guard_cover_wait","guard_route_next","guard_route_wait","guard_engineer_goal","guard_engineer_wait","repath","stuck"})put(f,0);
 if(get("weapon_action")==int(GuardAction::Bash)&&get("weapon_action_ms")<300){put("weapon_action",0);put("weapon_action_ms",0);put("weapon_action_events",0);}
 if(reason==GuardTaskReason::Blocked&&get("schedule")==int(GuardSchedule::Patrol)){put("guard_point",(get("guard_point")+1)%int(ctx.track.ai.patrol.size()));put("guard_arrived",0);put("guard_pause",0);put("guard_patrol_wait",0);}
 auto failed=GuardSchedule(get("schedule"));auto failure=GuardSchedule(guardScheduleDefinition(failed).schedule.failureSchedule);if(failure==GuardSchedule::None)failure=GuardSchedule::Recovery;
 const auto&destination=guardScheduleDefinition(failure).schedule;const auto&first=destination.tasks.front();
 auto firstTask=first.operation==ai::TaskOperation::Prepare?GuardTask::Prepare:first.operation==ai::TaskOperation::Move?GuardTask::Move:first.operation==ai::TaskOperation::Escape?GuardTask::Escape:GuardTask::Hold;
 put("schedule",int(failure));put("schedule_step",first.phase);put("task",int(firstTask));put("task_status",int(first.operation==ai::TaskOperation::Wait?GuardTaskStatus::Running:GuardTaskStatus::New));put("task_ms",0);put("task_stall",0);put("task_progress",0);put("task_subject",0);put("task_work_start",0);put("task_cursor",0);put("task_cursor_saved",1);put("task_fail_schedule",destination.failureSchedule);
 put("task_retry",reason==GuardTaskReason::Unavailable?std::min(5000,ctx.track.ai.aimMs+1000):700);
}
void Game::runGuardSchedule(int level,size_t actor,const ActorTrack&track,ActorPose&pose,float dt,int ms,Vec2&goal,float&goalZ,bool&wantsMove){
 auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 auto previous=GuardSchedule(get("schedule"));int oldSubject=get("task_subject");unsigned oldConditions=unsigned(get("task_conditions"));
 auto target=guardTarget(actor);unsigned conditions=guardConditions(actor,pose);
 // Identity comes from the last observed/reference record, not a concealed
 // NPC's current streamed level. Hidden migration is not a fresh enemy cue.
 int kind=target.valid()?(get("guard_target_kind")?get("guard_target_kind"):int(target.kind)):0;
 int targetLevel=target.valid()?(get("guard_target_level")?get("guard_target_level"):target.level+1):0,index=target.valid()?(get("guard_target_index")?get("guard_target_index"):target.index+1):0;
 bool changed=target.valid()&&(get("task_enemy_kind")!=kind||get("task_enemy_level")!=targetLevel||get("task_enemy_index")!=index);
 if(changed)conditions|=GuardNewThreat;if(get("guard_under_fire"))conditions|=GuardHurt;
 if(get("weapon_reload")||get("weapon_reloading")||(track.ai.armed&&get("weapon_saved")&&!get("weapon_rounds")))conditions|=GuardNeedsReload;
 bool close=get("guard_seen")&&guardMeleeReady(actor,pose,target);if(close)conditions|=GuardCloseThreat;
 put("task_enemy_kind",kind);put("task_enemy_level",targetLevel);put("task_enemy_index",index);
 put("task_transitions",0);int retry=std::max(0,get("task_retry")-ms);put("task_retry",retry);
 auto choice=guardScheduleChoice(level,actor,track,pose,dt,ms,goal,goalZ,wantsMove,conditions,previous,oldSubject,retry,changed,close,target);
 auto desired=choice.schedule;auto work=choice.work;int subject=choice.subject;bool danger=choice.danger;
 put("task_conditions",int(conditions));unsigned events=(conditions&~oldConditions)|(conditions&(GuardNewThreat|GuardEnemyDead|GuardLightDamage|GuardHeavyDamage|GuardHeardSound));
 if(!ai::acceptedScheduleSounds(guardScheduleDefinition(previous).schedule,unsigned(get("guard_heard"))))events&=~GuardHeardSound;
 bool interrupted=previous!=desired||oldSubject!=subject||ai::scheduleInterrupts(guardScheduleDefinition(previous).schedule,events)||(desired==GuardSchedule::Cover&&(previous==GuardSchedule::Medic||previous==GuardSchedule::Engineer));
 if(interrupted){
  if(previous==GuardSchedule::Patient&&(desired!=GuardSchedule::Patient||oldSubject!=subject))releaseGuardPatient(actor);
  GuardTaskReason reason=danger?GuardTaskReason::Danger:changed?GuardTaskReason::ThreatChanged:desired==GuardSchedule::Cover?GuardTaskReason::Defense:previous==GuardSchedule::Medic&&desired!=GuardSchedule::Medic?GuardTaskReason::TargetLost:GuardTaskReason::None;
  put("task_reason",int(reason));put("task_stall",0);put("task_progress",0);
  if(previous!=GuardSchedule::None||danger||desired==GuardSchedule::Cover){
   put("weapon_aim",0);put("guard_slot",0);put("guard_slot_ms",0);
   put("support_ms",0);if(desired!=GuardSchedule::Medic)put("support_target",0);
   put("drone_windup",0);put("drone_repair_ms",0);put("drone_repair_target",0);put("drone_busy",0);put("grenade_windup",0);
   put("weapon_bash",0);if(get("weapon_action")==int(GuardAction::Bash)&&get("weapon_action_ms")<300){put("weapon_action",0);put("weapon_action_ms",0);put("weapon_action_events",0);}
  }
  put("schedule",int(desired));put("schedule_step",0);put("task",int(GuardTask::Prepare));put("task_status",int(GuardTaskStatus::New));put("task_ms",0);put("task_subject",subject);
  put("task_cursor",0);put("task_cursor_saved",1);
 }
 if(desired!=GuardSchedule::Medic){put("support_target",0);put("support_ms",0);}if(work==GuardTask::Repair)put("drone_repair_target",subject);
 if(desired!=GuardSchedule::Patient)put("guard_patient_move",0);
 auto global=goal+chunkOffset(level);put("task_goal_x",int(std::round(global.x*1000)));put("task_goal_y",int(std::round(global.y*1000)));put("task_goal_z",int(std::round(goalZ*1000)));
 const auto&definition=guardScheduleDefinition(desired).schedule;put("task_fail_schedule",definition.failureSchedule);
 ai::SchedulerState cursor{get("task_cursor"),ai::TaskStatus(get("task_status")),get("task_ms")};
 // Saves predating task lists stored the compatible phase only. Adopt that
 // exact active task, including its resource baseline and consumed events.
 if(!get("task_cursor_saved")){for(size_t i=0;i<definition.tasks.size();++i)if(definition.tasks[i].phase==get("schedule_step")){cursor.cursor=int(i);break;}put("task_cursor_saved",1);}
 if(get("schedule_step")==2&&desired!=GuardSchedule::Danger&&desired!=GuardSchedule::Recovery&&(wantsMove||get("task")!=int(work))){
  auto task=GuardTask(get("task"));put("weapon_aim",0);put("guard_slot",0);put("guard_slot_ms",0);
  if(task==GuardTask::Throw)put("grenade_windup",0);if(task==GuardTask::Deploy){put("drone_windup",0);put("drone_busy",0);}if(task==GuardTask::Repair){put("drone_repair_ms",0);put("drone_busy",0);}if(task==GuardTask::Treat)put("support_ms",0);
  int phase=wantsMove?1:2;for(size_t i=0;i<definition.tasks.size();++i)if(definition.tasks[i].phase==phase){cursor={int(i),ai::TaskStatus::New,0};break;}
 }
 auto concreteTask=[&](const ai::ScheduleTask&task){return task.operation==ai::TaskOperation::Prepare?GuardTask::Prepare:task.operation==ai::TaskOperation::Move?GuardTask::Move:task.operation==ai::TaskOperation::Escape?GuardTask::Escape:task.operation==ai::TaskOperation::Wait?GuardTask::Hold:work;};
 auto startTask=[&](const ai::ScheduleTask&task,ai::SchedulerState&state){
  auto concrete=concreteTask(task);
  put("schedule_step",task.phase);put("task",int(concrete));put("task_status",int(state.status));put("task_ms",0);
  if(task.operation==ai::TaskOperation::ExecuteWork){int baseline=work==GuardTask::Treat?get("support_used"):work==GuardTask::Deploy?get("drone_used"):work==GuardTask::Throw?get("grenade_used"):work==GuardTask::Fire?(get("weapon_saved")?get("weapon_rounds"):weaponDef(guardHandgun(track.ai)?WeaponId::Handgun:WeaponId::Shotgun).cycle.tubeCapacity):work==GuardTask::Repair?get("drone_repair_charge"):0;put("task_work_start",baseline);}
 };
 auto runTask=[&](const ai::ScheduleTask&task,ai::SchedulerState&state){switch(task.operation){
  case ai::TaskOperation::Prepare:return ai::TaskStatus::Complete;
  case ai::TaskOperation::Move:return wantsMove?ai::TaskStatus::Running:ai::TaskStatus::Complete;
  case ai::TaskOperation::Wait:return state.elapsedMs>=task.argument?ai::TaskStatus::Complete:ai::TaskStatus::Running;
  default:return ai::TaskStatus::Running;
 }};
 struct Adapter{decltype(startTask)&start;decltype(runTask)&run;};Adapter adapter{startTask,runTask};
 auto result=ai::MaintainSchedule(definition,cursor,adapter,4);unsigned transitions=result.transitions;
 if(result.outcome==ai::ScheduleOutcome::ScheduleDone&&desired!=GuardSchedule::Recovery&&transitions<4){cursor={0,ai::TaskStatus::New,0};auto next=ai::MaintainSchedule(definition,cursor,adapter,4-transitions);transitions+=next.transitions;result=next;}
 if(result.outcome==ai::ScheduleOutcome::TaskFailed||result.outcome==ai::ScheduleOutcome::InvalidState){failGuardTask(actor,GuardTaskReason::Unavailable);return;}
 if(cursor.status==ai::TaskStatus::New&&size_t(cursor.cursor)<definition.tasks.size()){const auto&pending=definition.tasks[size_t(cursor.cursor)];put("schedule_step",pending.phase);put("task",int(concreteTask(pending)));}
 put("task_cursor",cursor.cursor);put("task_cursor_saved",1);put("task_status",int(cursor.status));put("task_transitions",int(transitions));put("task_fraction",get("guard_fraction"));put("task_ms",std::min(60000,cursor.elapsedMs+ms));
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
 case GuardTask::Hold:{const auto&definition=guardScheduleDefinition(GuardSchedule(get("schedule"))).schedule;int cursor=get("task_cursor"),duration=700;if(cursor>=0&&size_t(cursor)<definition.tasks.size()&&definition.tasks[size_t(cursor)].operation==ai::TaskOperation::Wait)duration=definition.tasks[size_t(cursor)].argument;done=get("task_ms")>=duration;break;}
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
 // Deploy finishes windup then waits on the shared nav budget; without a
 // deadline that stand mutes the engineer forever. Repair likewise.
 unavailable|=task==GuardTask::Deploy&&((!get("drone_windup")&&get("drone_cooldown")>0)||(get("drone_windup")>=1200&&get("task_ms")>=2500));
 unavailable|=task==GuardTask::Repair&&get("task_ms")>=4000;
 if(unavailable)failGuardTask(actor,GuardTaskReason::Unavailable);
}
void Game::guardTaskMovement(size_t actor,const ActorPose&old,const ActorPose&pose,Vec2 waypoint,bool wanted,int ms){
 auto ctx=makeGuardCtx(actor,m_friendlyActors[actor].level,m_friendlyActors[actor].track(),m_friendlyActors[actor].pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 if(!wanted||get("schedule_step")!=1||get("task_status")!=int(GuardTaskStatus::Running)){put("task_stall",0);put("task_progress",0);return;}
 // Follow the chosen physical waypoint, so valid detours and seam migration
 // do not look like failure merely because target distance grows.
 auto direction=normalized(waypoint-old.position);float advance=std::max(0.f,dot(pose.position-old.position,direction));int progress=get("task_progress")+int(std::round(advance*1000));
 if(progress>=100){put("task_progress",progress%100);put("task_stall",0);return;}put("task_progress",progress);
 bool patrol=get("schedule")==int(GuardSchedule::Patrol);
 int stall=patrol?get("guard_patrol_wait"):std::min(8000,get("task_stall")+ms);put("task_stall",stall);
 // Every role can need a cover/portal detour under the four-search budget;
 // give the same stall window so combat Move does not wedge into Recovery early.
 int deadline=5000;if(stall>=deadline)failGuardTask(actor,GuardTaskReason::Blocked);
}
void Game::validateGuardSchedule(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto range=[&](const char*f,int lo,int hi){if(get(f)<lo||get(f)>hi)throw std::runtime_error("Invalid saved guard schedule");};
 // Reconstruction validates guards in authoring order. Count the complete
 // authored collection so a subject appearing later can restore normally.
 size_t npcCount=0;for(int level=0;level<chunkCount();++level)for(const auto&track:(level==m_level?m_world:m_chunks[level].world).actorTracks())npcCount+=track.ai.mode!=ActorAiMode::Scripted;
 range("schedule",0,int(GuardSchedule::Patient));range("schedule_step",0,2);range("task",0,int(GuardTask::Escape));range("task_status",0,int(GuardTaskStatus::Failed));range("task_ms",0,60000);range("task_fraction",0,999);range("task_stall",0,8000);range("task_progress",0,99);range("task_retry",0,5000);range("task_failures",0,255);range("task_reason",0,int(GuardTaskReason::Disabled));range("task_goal_x",-10000000,10000000);range("task_goal_y",-10000000,10000000);range("task_goal_z",-100000,100000);range("task_subject",0,4096);range("task_work_start",0,65535);range("task_conditions",0,int(GuardAllConditions));range("task_transitions",0,4);range("task_enemy_level",0,chunkCount());range("task_enemy_index",0,65536);range("guard_patient_move",0,1);range("task_cursor",0,ai::MaximumScheduleTasks);range("task_cursor_saved",0,1);range("task_fail_schedule",0,int(GuardSchedule::Patient));
 int kind=get("task_enemy_kind");if(kind&&kind!=int(ShotKind::Player)&&kind!=int(ShotKind::Npc)&&kind!=int(ShotKind::Actor)&&kind!=int(ShotKind::Enemy)&&kind!=int(ShotKind::Drone))throw std::runtime_error("Invalid saved scheduled threat");
 if((kind&&(!get("task_enemy_level")||!get("task_enemy_index")))||(!kind&&(get("task_enemy_level")||get("task_enemy_index"))))throw std::runtime_error("Inconsistent scheduled threat reference");
 if((kind==int(ShotKind::Npc)&&size_t(get("task_enemy_index"))>npcCount)||(kind==int(ShotKind::Player)&&get("task_enemy_index")!=1)||(kind==int(ShotKind::Drone)&&get("task_enemy_index")>EngineerDroneSlots))throw std::runtime_error("Scheduled threat is outside its collection");
 auto schedule=GuardSchedule(get("schedule"));auto task=GuardTask(get("task"));int step=get("schedule_step"),subject=get("task_subject");const auto&track=m_friendlyActors[actor].track();
 if(get("task_cursor_saved")){
  const auto&definition=guardScheduleDefinition(schedule).schedule;auto cursor=size_t(get("task_cursor"));bool done=cursor==definition.tasks.size()&&get("task_status")==int(GuardTaskStatus::Complete);
  if(get("task_fail_schedule")!=definition.failureSchedule||cursor>definition.tasks.size()||(!done&&cursor==definition.tasks.size()&&schedule!=GuardSchedule::None)||(cursor<definition.tasks.size()&&definition.tasks[cursor].phase!=step))throw std::runtime_error("Inconsistent saved schedule task list");
  if(!definition.tasks.empty()){
   const auto&listed=definition.tasks[done?definition.tasks.size()-1:cursor];bool matches=listed.phase==step;
   switch(listed.operation){case ai::TaskOperation::Prepare:matches&=task==GuardTask::Prepare;break;case ai::TaskOperation::Move:matches&=task==GuardTask::Move;break;case ai::TaskOperation::Escape:matches&=task==GuardTask::Escape;break;case ai::TaskOperation::Wait:matches&=task==GuardTask::Hold;break;case ai::TaskOperation::ExecuteWork:matches&=bool(guardTaskPolicy(schedule).work&guardTaskBit(task));break;}
   if(!matches)throw std::runtime_error("Saved task does not match its listed operation");
  }
 }
 else if(get("task_cursor")||get("task_fail_schedule"))throw std::runtime_error("Incomplete saved schedule cursor metadata");
 if(get("guard_patient_move")&&schedule!=GuardSchedule::Patient)throw std::runtime_error("Rendezvous has no Patient owner");
 if(schedule==GuardSchedule::None){if(task!=GuardTask::None||step||subject||get("task_status"))throw std::runtime_error("Inactive guard has an owned task");return;}
 if(task==GuardTask::None||(step==0&&task!=GuardTask::Prepare)||(step==1&&task!=GuardTask::Move)||(step==2&&(task==GuardTask::Prepare||task==GuardTask::Move)))throw std::runtime_error("Inconsistent saved task phase");
 if(step==2&&!(guardTaskPolicy(schedule).work&guardTaskBit(task)))throw std::runtime_error("Saved task is outside its schedule policy");
 if((schedule==GuardSchedule::Danger)!=(task==GuardTask::Escape))throw std::runtime_error("Invalid saved escape ownership");
 if(schedule==GuardSchedule::Medic&&(track.ai.role!=GuardSquadRole::Medic||subject<=0||size_t(subject)>npcCount||size_t(subject)==actor+1||(step==2&&task!=GuardTask::Treat&&task!=GuardTask::Hold)))throw std::runtime_error("Invalid saved treatment ownership");
 if(schedule==GuardSchedule::Patient){
  const ActorTrack*provider=nullptr;int remaining=subject;
  if(subject>0&&size_t(subject)<=npcCount&&size_t(subject)!=actor+1)for(int level=0;level<chunkCount()&&!provider;++level)for(const auto&candidate:(level==m_level?m_world:m_chunks[level].world).actorTracks())if(candidate.ai.mode!=ActorAiMode::Scripted&&--remaining==0){provider=&candidate;break;}
  if(!provider||provider->ai.role!=GuardSquadRole::Medic||provider->ai.squad!=track.ai.squad||actorFaction(*provider)!=actorFaction(track))throw std::runtime_error("Invalid saved Patient provider");
 }
 if(schedule==GuardSchedule::Engineer&&track.ai.role!=GuardSquadRole::Engineer)throw std::runtime_error("Invalid saved engineering ownership");
 if(task==GuardTask::Treat&&schedule!=GuardSchedule::Medic)throw std::runtime_error("Treatment has no Medic owner");
 if((task==GuardTask::Deploy||task==GuardTask::Repair)&&schedule!=GuardSchedule::Engineer)throw std::runtime_error("Engineering has no owner");
 if(task==GuardTask::Repair&&(subject<=0||subject>EngineerDroneSlots))throw std::runtime_error("Invalid saved maintenance subject");
 if(schedule!=GuardSchedule::Medic&&schedule!=GuardSchedule::Patient&&task!=GuardTask::Repair&&subject)throw std::runtime_error("Unexpected saved task subject");
 if(task>=GuardTask::Fire&&task<=GuardTask::Repair&&task!=GuardTask::Treat&&!track.ai.armed)throw std::runtime_error("Unarmed guard owns armed work");
}
}

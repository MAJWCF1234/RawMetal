#include "../../Game.h"
namespace retro {
int guardTreatmentDuration(const ActorTrack&track){float duration=track.visual==ActorVisual::CleanupMedic?cleanupAnimationDuration(track.visual,6):0;return duration>0?std::max(1,int(std::round(duration*1000))):2400;}
void Game::guardSupportIntent(int level,size_t actor,const ActorTrack&track,ActorPose&pose,float dt,Vec2&goal,float&goalZ,bool&wantsMove,bool advanceWork){
 if(track.ai.role==GuardSquadRole::Engineer){guardEngineerIntent(level,actor,track,pose,std::max(1,int(std::round(dt*1000))),goal,goalZ,wantsMove);return;}
 if(track.ai.role!=GuardSquadRole::Medic)return;
 auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};int ms=npcMilliseconds(actor,dt,"support_fraction");
 put("support_cooldown",std::max(0,get("support_cooldown")-ms));
 auto origin=chunkOffset(level);int target=-1;float priority=-1;
 if(get("support_used")<guardRoleProfile(track.ai.role).treatments&&!get("support_cooldown"))for(size_t i=0;i<m_friendlyActors.size();++i){
  const auto&patient=m_friendlyActors[i];const auto&t=patient.track();if(i==actor||!t.health||!chunkResident(patient.level)||!autonomousActor(t)||patient.pose.clip==4||state(t.deadState)||t.ai.squad!=track.ai.squad||actorFaction(t)!=actorFaction(track))continue;
  int damage=state(t.damageState);auto position=patient.pose.position+chunkOffset(patient.level)-origin;float distance=length(position-pose.position);if(damage<t.health*.50f||distance>12||std::fabs(patient.pose.z-pose.z)>.3f)continue;
  if(state(friendlyKey(i,"guard_danger_ms"))||state(friendlyKey(i,"schedule"))==int(GuardSchedule::Recovery))continue;
  if(t.ai.mode==ActorAiMode::Guard&&(state(friendlyKey(i,"guard_under_fire"))||state(friendlyKey(i,"hit_source"))||guardGrenadeExposed(i)||(get("schedule")!=int(GuardSchedule::None)&&damage>state(friendlyKey(i,"guard_damage")))))continue;
  bool reserved=false;for(size_t other=0;other<m_friendlyActors.size();++other)if(other!=actor&&m_friendlyActors[other].track().ai.role==GuardSquadRole::Medic&&m_friendlyActors[other].pose.clip!=4&&state(friendlyKey(other,"support_target"))==int(i)+1)reserved=true;
  if(reserved||!friendlyRayClear({pose.position.x+origin.x,pose.position.y+origin.y,pose.z+track.scale*.85f},{position.x+origin.x,position.y+origin.y,patient.pose.z+t.scale*.85f}))continue;
  float score=float(damage)/t.health-distance*.005f;if(int(i)+1==get("support_target"))score+=.2f;if(score>priority){target=int(i);priority=score;}
 }
 if(target<0){put("support_target",0);put("support_ms",0);return;}
 if(target+1!=get("support_target")){put("support_target",target+1);put("support_ms",0);}
 const auto&patient=m_friendlyActors[size_t(target)];const auto&t=patient.track();auto position=patient.pose.position+chunkOffset(patient.level)-origin;
  bool rendezvous=state(friendlyKey(size_t(target),"schedule"))==int(GuardSchedule::Patient)&&state(friendlyKey(size_t(target),"task_subject"))==int(actor)+1&&state(friendlyKey(size_t(target),"guard_patient_move"))&&state(friendlyKey(size_t(target),"schedule_step"))==1;
  if(rendezvous){goal=pose.position;goalZ=pose.z;wantsMove=false;put("support_ms",0);return;}
 auto toward=position-pose.position;if(lengthSq(toward)>.001f)pose.yaw+=std::clamp(std::remainder(std::atan2(toward.x,toward.y)-pose.yaw,2*kPi),-4.f*dt,4.f*dt);
 // Saved positions are millimetre-quantized. Keep a small work-distance
 // hysteresis so restoration at the approach threshold cannot cancel an
 // already committed treatment interval.
 float reach=get("support_ms")>0?1.3f:1.2f;
 goal=position;goalZ=patient.pose.z;wantsMove=lengthSq(toward)>reach*reach;put("guard_cover",0);put("guard_cover_wait",0);
 if(!wantsMove&&!friendlyRayClear({pose.position.x+origin.x,pose.position.y+origin.y,pose.z+track.scale*.6f},{position.x+origin.x,position.y+origin.y,patient.pose.z+t.scale*.6f}))wantsMove=true;
 if(wantsMove){put("support_ms",0);if(patient.level!=level)guardRoute(level,actor,position+origin,goalZ,ms,goal,goalZ);return;}
 if(get("weapon_reload")||get("weapon_reloading")||get("weapon_recovery")||get("weapon_bash")||get("grenade_windup")||get("weapon_action")==int(GuardAction::Fire)||get("weapon_action")==int(GuardAction::Bash)){put("support_ms",0);return;}
 if(advanceWork)runGuardTreatment(actor,ms);
}
void Game::runGuardTreatment(size_t actor,int ms){
 auto ctx=makeGuardCtx(actor,m_friendlyActors[actor].level,m_friendlyActors[actor].track(),m_friendlyActors[actor].pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 int target=get("support_target")-1;if(target<0||size_t(target)>=m_friendlyActors.size()||!guardTaskAllows(actor,GuardTask::Treat))return;
 if(get("weapon_reload")||get("weapon_reloading")||get("weapon_recovery")||get("weapon_bash")||get("grenade_windup")||get("weapon_action")==int(GuardAction::Fire)||get("weapon_action")==int(GuardAction::Bash))return;
 const auto&patient=m_friendlyActors[size_t(target)];const auto&t=patient.track();auto origin=chunkOffset(ctx.level),position=patient.pose.position+chunkOffset(patient.level)-origin;
 // Player hits precede the NPC pass. A provider authored before its patient
 // must not heal over that unsensed hit and erase the patient's damage cue.
 if(t.ai.mode==ActorAiMode::Guard&&(state(friendlyKey(size_t(target),"guard_under_fire"))||state(friendlyKey(size_t(target),"hit_source"))||guardGrenadeExposed(size_t(target))||(get("schedule")!=int(GuardSchedule::None)&&state(t.damageState)>state(friendlyKey(size_t(target),"guard_damage"))))){put("support_ms",0);return;}
 if(patient.pose.clip==4||!autonomousActor(t)||state(t.deadState)||length(position-ctx.pose.position)>1.31f)return;
 int duration=guardTreatmentDuration(ctx.track),treatment=std::min(duration,get("support_ms")+ms);put("support_ms",treatment);if(treatment<duration)return;
 setState(t.damageState,std::max(0,state(t.damageState)-30));put("support_used",get("support_used")+1);put("support_cooldown",6000);put("support_ms",0);put("support_target",0);put("weapon_recovery",std::max(get("weapon_recovery"),450));
 m_sounds.push_back({Sound::Pickup,position+origin-chunkOffset(m_level),.65f,.8f,true,10,patient.pose.z+t.scale*.6f});
}
void Game::validateGuardSupport(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};const auto&track=m_friendlyActors[actor].track();
 if(get("support_target")<0||get("support_target")>4096||get("support_used")<0||get("support_used")>guardRoleProfile(track.ai.role).treatments||get("support_ms")<0||get("support_ms")>guardTreatmentDuration(track)||get("support_fraction")<0||get("support_fraction")>=1000||get("support_cooldown")<0||get("support_cooldown")>6000||(get("support_ms")&&!get("support_target")))throw std::runtime_error("Invalid saved squad treatment");
 if(track.ai.role!=GuardSquadRole::Medic&&(get("support_target")||get("support_ms")||get("support_used")))throw std::runtime_error("Saved treatment requires Medic profile");
}
}

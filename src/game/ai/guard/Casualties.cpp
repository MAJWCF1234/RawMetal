#include "../../Game.h"
namespace retro {
int Game::guardPatientMedic(size_t actor)const{
 const auto&patient=m_friendlyActors.at(actor);const auto&track=patient.track();
 if(!track.health||!track.ai.squad||!autonomousActor(track)||patient.pose.clip==4||(track.deadState&&state(track.deadState))||state(track.damageState)<track.health*.5f)return -1;
 auto origin=chunkOffset(patient.level);int chosen=-1;float nearest=10000;
 for(size_t medic=0;medic<m_friendlyActors.size();++medic){const auto&npc=m_friendlyActors[medic];const auto&t=npc.track();auto get=[&](const char*f){return state(friendlyKey(medic,f));};
  if(medic==actor||t.ai.mode!=ActorAiMode::Guard||t.ai.role!=GuardSquadRole::Medic||t.ai.squad!=track.ai.squad||actorFaction(t)!=actorFaction(track)||!autonomousActor(t)||!chunkResident(npc.level)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||get("support_target")!=int(actor)+1||get("support_used")>=guardRoleProfile(t.ai.role).treatments||get("support_cooldown")||get("guard_danger_ms")||get("guard_under_fire"))continue;
  if(get("schedule")!=int(GuardSchedule::None)&&get("schedule")!=int(GuardSchedule::Medic))continue;
  float distance=length(npc.pose.position+chunkOffset(npc.level)-origin-patient.pose.position);
  if(distance>12||std::fabs(npc.pose.z-patient.pose.z)>.3f||distance>=nearest)continue;
  nearest=distance;chosen=int(medic);
 }return chosen;
}
void Game::releaseGuardPatient(size_t actor){
 if(state(friendlyKey(actor,"schedule"))==int(GuardSchedule::Patient)){
  int medic=state(friendlyKey(actor,"task_subject"))-1;
  if(medic>=0&&size_t(medic)<m_friendlyActors.size()&&state(friendlyKey(size_t(medic),"support_target"))==int(actor)+1){setState(friendlyKey(size_t(medic),"support_target"),0);setState(friendlyKey(size_t(medic),"support_ms"),0);}
 }
 setState(friendlyKey(actor,"guard_patient_move"),0);
}
bool Game::guardPatientIntent(int level,size_t actor,const ActorTrack&track,const ActorPose&pose,int ms,Vec2&goal,float&goalZ,bool&wantsMove,int&medic){
 (void)ms;
 medic=guardPatientMedic(actor);if(medic<0){setState(friendlyKey(actor,"guard_patient_move"),0);return false;}
 const auto&provider=m_friendlyActors[size_t(medic)];auto ctx=makeGuardCtx(actor,level,track,m_friendlyActors[actor].pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};auto origin=chunkOffset(level);
 bool paired=get("schedule")==int(GuardSchedule::Patient)&&get("task_subject")==medic+1;
 // A patient already holding cover stays there. The Medic comes to them;
 // treatment is never permission to surrender a protected firing position.
 auto threat=guardTarget(actor);bool protectedNow=threat.valid()&&guardCovered(actor,pose,threat);
 bool moving=get("walking")||((track.ai.role==GuardSquadRole::Rusher||track.ai.role==GuardSquadRole::Assault)&&get("guard_seen")&&get("guard_confirmed"));
 bool withdraw=paired&&get("guard_patient_move");Vec2 destination=pose.position;float feet=pose.z;
 if(withdraw){destination={get("task_goal_x")*.001f-origin.x,get("task_goal_y")*.001f-origin.y};feet=get("task_goal_z")*.001f;}
 else if(moving&&!protectedNow&&provider.level==level&&get("guard_memory")){
  Vec2 remembered{get("guard_x")*.001f-origin.x,get("guard_y")*.001f-origin.y};float before=length(pose.position-remembered),rear=length(provider.pose.position-remembered);
  if(rear>=before+.5f&&length(provider.pose.position-pose.position)>1.3f){
   auto candidate=provider.pose.position+normalized(pose.position-provider.pose.position)*1.05f;float supported=pose.z;
   bool safe=candidate.x>=.35f&&candidate.y>=.35f&&candidate.x<=23.65f&&candidate.y<=23.65f&&!guardPositionReserved(actor,candidate,pose.z)&&friendlyWalkSegment(level,pose.position,pose.z,candidate,track.scale,&supported)&&std::fabs(supported-provider.pose.z)<.3f;
   // Retreat uses only this patient's remembered threat. The route must not
   // move closer to it, even if the rear endpoint appears farther away.
   for(int sample=1;sample<=8&&safe;++sample)safe=length(pose.position+(candidate-pose.position)*(sample/8.f)-remembered)>=before-.05f;
   if(safe){destination=candidate;feet=supported;withdraw=true;put("repath",0);}
  }
 }
 if(protectedNow){withdraw=false;destination=pose.position;feet=pose.z;}
 // Fixed rendezvous avoids two actors chasing each other's changing position.
 // The ordinary physical navigator still owns collision, floors and bodies.
 goal=destination;goalZ=feet;wantsMove=withdraw&&(length(destination-pose.position)>.12f||std::fabs(feet-pose.z)>.3f);put("guard_patient_move",wantsMove);
 if(!wantsMove){goal=pose.position;goalZ=pose.z;}
 put("guard_cover",0);put("guard_cover_wait",0);put("guard_route_next",0);put("guard_route_wait",0);return true;
}
}

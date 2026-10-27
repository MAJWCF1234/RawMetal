#include "../../../core/FrameProfile.h"
#include "../../Game.h"
#include "../Navigation.h"
#include "../Perception.h"
#include "../StateLookupHints.h"
#include "../WalkQueryCache.h"
#include "../../../world/CollisionQueryBatch.h"
#include <optional>
#include <fstream>
#include <chrono>
namespace retro {
void Game::damageFriendly(size_t actor,float amount,Vec2 source,AiTargetRef attacker){
 const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();auto pose=npc.pose;
 if(!track.health||state(track.deadState)||!std::isfinite(amount)||amount<=0)return;
 int previous=std::clamp(state(track.damageState),0,track.health);int damage=previous+std::min(track.health-previous,int(std::ceil(std::min(amount,float(track.health)))));setState(track.damageState,damage);
 setState(friendlyKey(actor,"hit_x"),int(std::round(source.x*1000)));setState(friendlyKey(actor,"hit_y"),int(std::round(source.y*1000)));setState(friendlyKey(actor,"hit_source"),1);
 setState(friendlyKey(actor,"hit_kind"),int(attacker.kind));setState(friendlyKey(actor,"hit_level"),attacker.kind==ShotKind::None?0:attacker.level+1);setState(friendlyKey(actor,"hit_index"),attacker.kind==ShotKind::None?0:attacker.index+1);
 if(damage>=track.health){
  npcVoice(actor,NpcVoiceCue::Down);
  setState(friendlyKey(actor,"killed_by_player"),attacker.kind==ShotKind::Player?1:0);
  if(npcIncapacitated(actor)&&!npcHeld(actor)){setState(friendlyKey(actor,"death_ms"),state(friendlyKey(actor,"restraint_ms")));setState(friendlyKey(actor,"death_fraction"),state(friendlyKey(actor,"restraint_fraction")));}
  int heldWeapon=state(friendlyKey(actor,"restraint_weapon"));if(npcHeld(actor)&&heldWeapon>0&&heldWeapon<weaponCount())equipWeapon(WeaponId(heldWeapon));
  for(const char*field:{"restraint","restraint_ms","restraint_fraction","restraint_weapon"})setState(friendlyKey(actor,field),0);
  setState(track.deadState,1);setState(actorPositionState(track.deadState,0),int(std::round(pose.position.x*1000)));setState(actorPositionState(track.deadState,1),int(std::round(pose.position.y*1000)));setState(actorPositionState(track.deadState,2),int(std::round(pose.z*1000)));if(track.ai.mode==ActorAiMode::Guard)clearGuardSchedule(actor);updateMechanisms(0);
 }
 bool voiced=damage>=track.health?std::any_of(m_sounds.begin(),m_sounds.end(),[&](const auto&e){return e.speech>=0&&e.speaker==actor+1;}):npcVoice(actor,NpcVoiceCue::Hurt);
 if(!voiced&&!npcHasVoice(actor))m_sounds.push_back({damage>=track.health?Sound::WorkerDying:Sound::Hurt,pose.position+chunkOffset(npc.level)-chunkOffset(m_level),.7f,1,true,17,pose.z+track.scale*.7f});
 if(damage>=track.health&&track.ai.squad){
  // The nearest squadmate who saw him go down (a voiced one, for the spoken line).
  auto witness=[&](bool voiced){size_t reporter=SIZE_MAX;float best=15.f*15.f;
   for(size_t other=0;other<m_friendlyActors.size();++other){const auto&buddy=m_friendlyActors[other];
    if(other==actor||buddy.track().ai.squad!=track.ai.squad||(voiced&&!npcHasVoice(other))||(!voiced&&buddy.track().ai.mode!=ActorAiMode::Guard)||state(buddy.track().deadState)||npcIncapacitated(other))continue;
    auto delta=buddy.pose.position+chunkOffset(buddy.level)-pose.position-chunkOffset(npc.level);float dist=lengthSq(delta);if(dist>best)continue;
    // Only a man who saw him go down can call it.
    auto from=buddy.pose.position+chunkOffset(buddy.level)-chunkOffset(m_level),to=pose.position+chunkOffset(npc.level)-chunkOffset(m_level);
    if(!soundRayClear(from,buddy.pose.z+buddy.track().scale*.85f,to,pose.z+track.scale*.5f))continue;
    best=dist;reporter=other;
   }return reporter;};
  // He radios it in: the same committed call as a sighting, cut short if he is dropped before it lands.
  if(auto caller=witness(false);caller!=SIZE_MAX)startGuardCall(caller,CallBody);
  if(size_t reporter=state(stateId("npc_voice_cd_allydown"))<=0?witness(true):SIZE_MAX;reporter!=SIZE_MAX){
   setState(friendlyKey(reporter,"voice_pending"),int(NpcVoiceCue::AllyDown)+1);setState(friendlyKey(reporter,"voice_pending_ms"),4500);setState(friendlyKey(reporter,"voice_reply_wait"),0);
   // Reserve the channel immediately so a multi-kill frame cannot queue several reports.
   setState(stateId("npc_voice_cd_allydown"),9000);
  }
 }
}
bool Game::aiDoorLocked(const World&world,int level,const Door&door)const{
 perf::count(perf::DoorLockChecks);
 int living=hostilesRemaining(level);
 return door.transfer||door.entry||(door.requireEnemiesClear&&living)||(door.requireControl&&!world.controlReleased())||(door.requireState&&state(door.requireState)!=door.requireValue);
}
void Game::requestAiDoor(World& world,int level,Vec2 position,float feet,Vec2 goal){
 perf::Scope doorProfile(perf::DoorRequests);
 for(size_t i=0;i<world.doors().size();++i){const auto&door=world.doors()[i];
  if(aiDoorLocked(world,level,door)||door.opening||std::fabs(position.y-door.y)>1.8f||position.x<door.left+.2f||position.x>door.right-.2f||(position.y-door.y)*(goal.y-door.y)>=0)continue;
  float z=world.floorHeight((door.left+door.right)*.5f,door.y)+door.z;if(std::fabs(feet-z)>.3f)continue;
  Vec2 face{position.x,door.y+std::copysign(.2f,position.y-door.y)};
  if(world.rayClear(position,feet+.7f,face,feet+.7f))world.openDoor(int(i));
 }
}
ActorPose Game::updateFriendlyAI(World& world,int level,size_t actor,const ActorTrack& track,const ActorPose& old,ActorPose pose,float dt,const std::vector<AiStimulus>* stimuli){
 perf::count(perf::FriendlyUpdates);
 const auto&config=track.ai;
 auto key=[&](std::string_view field){return friendlyKey(actor,field);};
 auto get=[&](std::string_view field){return state(key(field));};auto put=[&](std::string_view field,int value){setState(key(field),value);};
 if(track.deadState&&state(track.deadState)){
  if(config.mode==ActorAiMode::Guard)clearGuardSchedule(actor);
  if(config.mode==ActorAiMode::Civilian||config.mode==ActorAiMode::Escort||config.mode==ActorAiMode::Scripted)clearCivilianSchedule(actor);
  pose.position={state(actorPositionState(track.deadState,0))*.001f,state(actorPositionState(track.deadState,1))*.001f};pose.z=state(actorPositionState(track.deadState,2))*.001f;pose.clip=4;
  float sourceDuration=cleanupAnimationDuration(track.visual,4);int duration=sourceDuration>0?std::max(1,int(std::round(sourceDuration*1000))):1000;
  if(sourceDuration>0&&(get("death_fraction")<0||get("death_fraction")>=1000))throw std::runtime_error("Invalid saved NPC death clock");
  int age=std::clamp(get("death_ms"),0,duration);if(dt>0)age=std::min(duration,age+(sourceDuration>0?npcMilliseconds(actor,dt,"death_fraction"):std::max(1,int(std::round(dt*1000)))));put("death_ms",age);pose.phase=float(age)/duration;return pose;
 }
 if(npcIncapacitated(actor)){
  // Restraint is a living state. Stop every pending action without spending
  // ammunition, support charges or death events, and keep the placed body.
  if(get("schedule")||get("task")||get("support_target")||get("drone_windup")||get("drone_busy")||get("guard_slot")){
   if(config.mode==ActorAiMode::Guard)clearGuardSchedule(actor);else clearCivilianSchedule(actor);
  }
  for(const char*field:{"running","walking","weapon_aim","weapon_raise","weapon_flash","weapon_recoil","weapon_reload","weapon_reloading","weapon_reload_events","weapon_action","weapon_action_ms","weapon_action_events","weapon_bash","weapon_bash_pose","grenade_windup","guard_mode","guard_seen","guard_confirmed","guard_memory","guard_suppress_ms","guard_locker","guard_check","guard_check_ms","guard_check_said","guard_doors_ms","guard_haul"})if(get(field))put(field,0);
  if(get("guard_target_kind"))rememberGuardTarget(actor,{});
  pose=old;pose.lookAtActor=-1;
  if(npcHeld(actor)){pose.clip=0;pose.phase=0;}
  else{
   float sourceDuration=cleanupAnimationDuration(track.visual,4);int duration=sourceDuration>0?std::max(1,int(std::round(sourceDuration*1000))):1000;
   int age=std::clamp(get("restraint_ms"),0,duration);if(dt>0)age=std::min(duration,age+npcMilliseconds(actor,dt,"restraint_fraction"));put("restraint_ms",age);pose.clip=4;pose.phase=float(age)/duration;
   // Stowed in a locker: the slide in, then shut out of sight.
   if(get("guard_stowed"))updateStowedBody(actor,pose,dt);
  }
  return pose;
 }
 if(pose.clip==4){if(config.mode==ActorAiMode::Guard)clearGuardSchedule(actor);if(config.mode!=ActorAiMode::Guard)clearCivilianSchedule(actor);return pose;}
 if(config.enableState&&!state(config.enableState)){
  if(config.mode==ActorAiMode::Guard&&(get("schedule")||get("guard_danger_ms")||get("guard_mode")||get("guard_target_kind")||get("support_target")))clearGuardSchedule(actor);
  if(config.mode==ActorAiMode::Civilian||config.mode==ActorAiMode::Escort)clearCivilianSchedule(actor);put("running",0);
  // Stage idle still cycles so Guard Lab / ambush squads read as alive before
  // their enable trigger, instead of freezing on UAL sample 0 (bind).
  if(dt>0&&pose.clip!=4){
   float previous=get("saved")?get("phase")*.0001f:old.phase;
   float duration=cleanupAnimationDuration(track.visual,0);pose.clip=0;pose.phase=std::fmod(previous+dt*(duration>0?1/duration:.45f),1.f);
   put("phase",int(std::round(pose.phase*10000)));put("walking",0);put("crouched",0);put("saved",1);
  }
  return pose;
 }
 const auto&enemies=level==m_level?m_enemies:m_chunks[level].enemies;
 if(get("saved")){
  if(dt==0||!get("running")){pose.position={get("x")*.001f,get("y")*.001f};pose.z=get("z")*.001f;pose.yaw=get("yaw")*.0001f;pose.phase=get("phase")*.0001f;pose.clip=get("walking")?1:get("crouched")?5:0;
   if(pose.position.x<0||pose.position.y<0||pose.position.x>=24||pose.position.y>=24||std::fabs(pose.z)>100||std::fabs(pose.yaw)>kPi+.001f||pose.phase<0||pose.phase>1)throw std::runtime_error("Invalid saved NPC pose");
   // Keep the saved signed pi endpoint inside the angle domain after
   // angle quantization, rather than wrapping it on each load.
   pose.yaw=std::clamp(pose.yaw,-kPi,kPi);
  }
  else pose=old;
 }
 pose.lookAtActor=-1;
 if(dt<=0)return pose;
 // Decision, route and local movement probes read one settled collision
 // state. The cache expires on return, before another actor or physics step.
 CollisionQueryBatch queries(&world,m_aiQueryCaching);
 ai::WalkQueryCache::Scope walkMemo(world,world.m_mapData,m_aiQueryCaching);
 put("running",1);float standing=track.scale;bool crouched=get("crouched")!=0;float height=crouched?npcCrouchedHeight(track):standing;
 bool grounded=get("grounded")!=0||!get("locomotion");float vv=get("vv")*.001f;int ms=std::max(1,int(std::round(dt*1000)));
 Vec2 goal=pose.position;float goalZ=pose.z;bool wantsMove=false;float speedScale=1;
 int guardMs=0;
 if(config.mode==ActorAiMode::Scripted)scriptedHandoffIntent(world,level,actor,track,pose,dt,enemies,goal,goalZ,wantsMove,speedScale);
 else if(config.mode==ActorAiMode::Guard){guardMs=(get("guard_fraction")+int(std::round(double(dt)*1000000)))/1000;guardIntent(level,actor,track,pose,dt,*stimuli,goal,goalZ,wantsMove);if(get("guard_danger_ms"))speedScale=1.35f;if(get("evac_level"))speedScale=1.8f;}
 else civilianEscortIntent(world,level,actor,track,pose,dt,enemies,goal,goalZ,wantsMove,speedScale);
 bool wantCrouch=false;
 if(config.mode==ActorAiMode::Guard&&!get("evac_level"))wantCrouch=get("guard_cover_mode")==2||get("guard_under_fire")>0||get("weapon_reload")>0||get("weapon_reloading")||(track.health>0&&state(track.damageState)>=int(track.health*.65f));
 if((config.mode==ActorAiMode::Escort||config.mode==ActorAiMode::Civilian)&&level==m_level&&m_player.crouched&&length(pose.position-m_player.pos)<3.5f)wantCrouch=true;
 float crouchHeight=npcCrouchedHeight(track);
 if(!wantCrouch&&grounded&&!friendlyBodyFits(level,pose.position,pose.z,standing)&&friendlyBodyFits(level,pose.position,pose.z,crouchHeight))wantCrouch=true;
 // Duck before the standing planner rejects a low passage. Both sweeps retain
 // the same floor, door, seam and footprint checks used by actual movement.
 if(!wantCrouch&&grounded&&wantsMove){auto toward=goal-pose.position;float distance=length(toward);auto probe=pose.position+toward*(std::min(1.f,distance)/std::max(.001f,distance));
  if(!friendlyWalkSegment(level,pose.position,pose.z,probe,standing)&&friendlyWalkSegment(level,pose.position,pose.z,probe,crouchHeight))wantCrouch=true;
 }
 if(wantCrouch&&!crouched){crouched=true;height=crouchHeight;}
 else if(!wantCrouch&&crouched){if(friendlyBodyFits(level,pose.position,pose.z,standing)){crouched=false;height=standing;}else height=crouchHeight;}
 if(crouched)speedScale*=.55f;
 float timer=std::max(0.f,std::clamp(get("repath"),0,4000)*.001f-dt);Vec2 waypoint=get("saved")?Vec2{get("way_x")*.001f,get("way_y")*.001f}:pose.position;
 bool navigationDeferred=config.mode==ActorAiMode::Guard&&m_npcNavBudget<=0&&get("guard_route_target")&&get("guard_route_target")!=level+1&&!get("guard_route_next")&&!get("guard_route_wait")&&lengthSq(goal-pose.position)<.00001f;
 if(lengthSq(goal-Vec2{get("goal_x")*.001f,get("goal_y")*.001f})>1.f)timer=0;
 put("goal_x",int(std::round(goal.x*1000)));put("goal_y",int(std::round(goal.y*1000)));
 auto direction=Vec2{};
 if(wantsMove){
  requestAiDoor(world,level,pose.position,pose.z,goal);
  auto toward=goal-pose.position;float distance=length(toward);auto probe=pose.position+toward*(std::min(1.f,distance)/std::max(.001f,distance));
  // A direct goal probe is expensive. Reuse the current validated waypoint
  // while its route timer is live; recheck immediately when the goal changes,
  // the route expires, or the actor reaches its waypoint.
  bool routeExpired=timer<=0||(get("walking")&&lengthSq(waypoint-pose.position)<.04f);
  if(routeExpired&&(world.hasTerrain()||std::fabs(goalZ-pose.z)<.3f)&&friendlyWalkSegment(level,pose.position,pose.z,probe,height))waypoint=goal;
  else if(routeExpired){
   if(m_npcNavBudget<=0)navigationDeferred=true;
   else{--m_npcNavBudget;
   // Plan through doors this actor may operate; actual movement still collides
   // until a nearby request has physically opened the door.
   // (An overlay on the live world: copying the whole world per search stalled
   // crowded rooms.)
   std::vector<char> operable(world.doors().size(),0);bool anyOperable=false;
   for(size_t i=0;i<world.doors().size();++i)if(!aiDoorLocked(world,level,world.doors()[i])&&world.doors()[i].open<world.doors()[i].maxOpen){operable[i]=1;anyOperable=true;}
   // Prefer the retained hint graph for guards; fall back to the span flood
   // when the graph has no useful hop from the current feet.
   // The graph answers the goal itself when it has no route (a door shut in
   // the way, say): then the span flood plans through the doors it may open.
   if(config.mode==ActorAiMode::Guard){waypoint=guardGraphWaypoint(level,pose.position,pose.z,goal,goalZ,height);
    if(lengthSq(waypoint-pose.position)<.04f||(lengthSq(waypoint-goal)<1e-6f&&!friendlyWalkSegment(level,pose.position,pose.z,goal,height))){std::optional<World::DoorPlan> plan;if(anyOperable)plan.emplace(world,operable);waypoint=aiWalkWaypoint(world,pose.position,pose.z,goal,goalZ,height);}}
   else{std::optional<World::DoorPlan> plan;if(anyOperable)plan.emplace(world,operable);waypoint=aiWalkWaypoint(world,pose.position,pose.z,goal,goalZ,height);}
   timer=.3f+float((actor*17+size_t(level)*5)%11)*.02f;}
  }
  // A saved pose can exist before its first route search. When the shared
  // budget defers that search, no old/default waypoint may move or jump it.
  if(navigationDeferred)waypoint=pose.position;
  else{
  direction=normalized(waypoint-pose.position);Vec2 separation{};
  for(size_t i=0;i<m_friendlyActors.size();++i)if(i!=actor&&m_friendlyActors[i].level==level&&autonomousActor(m_friendlyActors[i].track())){const auto&other=m_friendlyActors[i].pose;if(other.clip==4||std::fabs(other.z-pose.z)>height)continue;
   auto away=pose.position-other.position;float neighborDistance=length(away);if(neighborDistance>.001f&&neighborDistance<.7f)separation+=away*((.7f-neighborDistance)/neighborDistance);
  }
  auto drones=engineerDrones();for(const auto&drone:drones){if(pose.z>=drone.position.z+EngineerDroneHeight||pose.z+height<=drone.position.z)continue;auto shift=chunkOffset(drone.level)-chunkOffset(level);
   auto away=pose.position-Vec2{drone.position.x+shift.x,drone.position.y+shift.y};float droneDistance=length(away);if(droneDistance>.001f&&droneDistance<1.05f)separation+=away*((1.05f-droneDistance)/droneDistance);
  }
  direction=normalized(direction+separation*2.f);
  auto climbable=[&](Vec2 at){float ground=friendlySupportAt(level,at,pose.z+.265f);float rise=ground-pose.z;float stepHeight=world.hasTerrain()?.34f:.24f;
   return grounded&&rise>stepHeight+.025f&&rise<=1.05f&&friendlyBodyFits(level,at,ground,height);};
  bool wantJump=false;
  if(grounded&&lengthSq(direction)>.0001f)wantJump=climbable(pose.position+direction*.45f)||climbable(pose.position+direction*.7f);
  if(!wantJump&&grounded&&goalZ-pose.z>.26f&&goalZ-pose.z<=1.05f&&length(goal-pose.position)<3.5f)wantJump=true;
  if(!wantJump&&grounded&&(get("stuck")>120||!friendlyWalkSegment(level,pose.position,pose.z,pose.position+direction*.4f,height))&&lengthSq(goal-pose.position)>.01f)wantJump=climbable(pose.position+normalized(goal-pose.position)*.55f)||(goalZ-pose.z>.26f&&goalZ-pose.z<=1.05f);
  if(wantJump){vv=5.4f;grounded=false;if(lengthSq(goal-pose.position)>.01f){direction=normalized(goal-pose.position);waypoint=goal;}}
  float travel=std::min(std::max(length(waypoint-pose.position),grounded?0.f:.35f),config.speed*speedScale*dt);
  auto next=pose.position+direction*travel;float feet=pose.z;
  bool clear=false;
  if(grounded)clear=travel>0&&friendlyWalkSegment(level,pose.position,pose.z,next,height,&feet);
  else{if(!grounded&&goalZ>pose.z+.05f&&lengthSq(goal-pose.position)>.01f){direction=normalized(goal-pose.position);next=pose.position+direction*travel;}
   clear=travel>0&&friendlyBodyFits(level,next,pose.z,height);}
  auto player=m_player.pos+chunkOffset(m_level)-chunkOffset(level);
  if(feet<m_player.z+m_player.hullHeight()&&feet+height>m_player.z&&std::fabs(next.x-player.x)<.45f&&std::fabs(next.y-player.y)<.45f&&lengthSq(next-player)<lengthSq(pose.position-player))clear=false;
  for(const auto&enemy:enemies)if(enemy.alive&&enemy.bodyBottom()<feet+height&&enemy.bodyTop()>feet&&lengthSq(next-enemy.pos)<.45f*.45f&&lengthSq(next-enemy.pos)<lengthSq(pose.position-enemy.pos))clear=false;
  for(const auto&drone:drones){if(feet>=drone.position.z+EngineerDroneHeight||feet+height<=drone.position.z)continue;auto shift=chunkOffset(drone.level)-chunkOffset(level);Vec2 body{drone.position.x+shift.x,drone.position.y+shift.y};
   if(lengthSq(next-body)<.85f*.85f&&lengthSq(next-body)<lengthSq(pose.position-body))clear=false;
  }
  if(clear){pose.position=next;if(grounded)pose.z=feet;
   // An alerted armed guard can sidestep while keeping its eyes on the target.
   // Guard intent has already applied the bounded facing turn for this frame.
   if(config.mode!=ActorAiMode::Guard||!config.armed||guardState(actor)!=GuardState::Alert||get("guard_danger_ms")||get("evac_level"))pose.yaw=old.yaw+std::clamp(std::remainder(std::atan2(direction.x,direction.y)-old.yaw,2*kPi),-6.f*dt,6.f*dt);
  }
  }
 }
 float support=friendlySupportAt(level,pose.position,pose.z+(grounded?.265f:.01f));
 if(grounded&&pose.z>support+.03f){if(pose.z-support<=.24f&&friendlyBodyFits(level,pose.position,support,height))pose.z=support;else grounded=false;}
 if(!grounded){float previousZ=pose.z;vv-=14.f*dt;pose.z+=vv*dt;support=friendlySupportAt(level,pose.position,std::max(previousZ,pose.z)+.265f);
  if(vv<=0.f&&pose.z<=support+.005f&&previousZ>=support-.025f){pose.z=support;vv=0;grounded=true;}
  else if(!friendlyBodyFits(level,pose.position,pose.z,height)){if(vv>0){vv=0;pose.z=previousZ;}else if(friendlyBodyFits(level,pose.position,support,height)){pose.z=support;vv=0;grounded=true;}}
 }
 float moved=length(pose.position-old.position);Vec2 movementWaypoint=waypoint;pose.clip=moved>.0001f?1:crouched?5:0;
 // Idle/crouch must keep cycling phase or UAL loops stay stuck on sample 0 (bind).
 // Walk usually advances from displacement; when the clip is walk but feet have
 // not moved yet, keep a source-timed fallback so the stride does not freeze.
 if(pose.clip==1){float duration=cleanupAnimationDuration(track.visual,1);float rate=moved>.0001f?moved*1.1f:dt*(duration>0?1/duration:.9f);pose.phase=std::fmod(old.phase+rate,1.f);}
 else if(pose.clip==0||pose.clip==5){float duration=cleanupAnimationDuration(track.visual,pose.clip);pose.phase=std::fmod(old.phase+dt*(duration>0?1/duration:pose.clip==5?.55f:.45f),1.f);}
 else pose.phase=0;
 // Pausing stall clocks while nav budget is empty is fine during an active hop
 // or a single denied search frame. A deferred actor that stays physically stuck
 // must still charge stall/patrol wait or Move tasks never fail.
 bool stallPaused=navigationDeferred&&!(wantsMove&&moved<.0001f&&get("stuck")>120);
 if(config.mode==ActorAiMode::Guard&&!get("guard_danger_ms")){
  int point=get("guard_point");bool patrol=guardState(actor)==GuardState::Patrol&&(get("schedule")==int(GuardSchedule::None)||get("schedule")==int(GuardSchedule::Patrol));
  if(!patrol||!wantsMove||get("guard_patrol_point_seen")!=point+1){put("guard_patrol_wait",0);put("guard_patrol_progress",0);put("guard_patrol_fraction",0);put("guard_patrol_movement_fraction",0);put("guard_patrol_point_seen",patrol?point+1:0);}
  if(patrol&&wantsMove){
   // Progress toward a route waypoint also counts when a legitimate detour
   // temporarily moves farther from the final patrol point.
   float advance=std::max(0.f,dot(pose.position-old.position,normalized(waypoint-old.position)));int movement=get("guard_patrol_movement_fraction")+int(std::round(advance*1000000));int progress=get("guard_patrol_progress")+movement/1000;movement%=1000;
   int wait=std::min(5000,get("guard_patrol_wait")+npcMilliseconds(actor,stallPaused?0.f:dt,"guard_patrol_fraction"));
   int meaningful=std::max(1,int(std::round(std::min(.1f,config.speed*2.f)*1000)));if(progress>=meaningful){progress=0;movement=0;wait=0;}
   if(wait>=5000&&get("schedule")==int(GuardSchedule::None)){put("guard_point",(point+1)%int(config.patrol.size()));put("guard_arrived",0);put("guard_pause",0);put("guard_patrol_point_seen",0);progress=0;movement=0;wait=0;timer=0;waypoint=pose.position;}
   put("guard_patrol_progress",progress);put("guard_patrol_movement_fraction",movement);put("guard_patrol_wait",wait);
  }
 }
 if(config.mode==ActorAiMode::Guard&&moved>0){int steps=get("guard_step")+int(std::round(moved*1000));if(steps>=1000){auto material=stepSoundAt(world,pose.position);m_sounds.push_back({material,pose.position+chunkOffset(level)-chunkOffset(m_level),.3f,.96f,true,17,pose.z+.04f});steps%=1000;}put("guard_step",steps);}
 if(pose.clip==0&&!get("guard_danger_ms")&&moved<.0001f&&(config.mode==ActorAiMode::Civilian||config.mode==ActorAiMode::Escort||(config.mode==ActorAiMode::Guard&&guardState(actor)==GuardState::Patrol)))
  pose=friendlyAwareness(world,level,track,m_friendlyActors[actor].homeLevel,m_friendlyActors[actor].homeActor,old,pose,dt);
 int stuck=wantsMove&&moved<.0001f?std::clamp(get("stuck"),0,500)+ms:0;if(stuck>500){timer=0;stuck=0;}put("stuck",stuck);
 put("repath",int(std::round(timer*1000)));put("way_x",int(std::round(waypoint.x*1000)));put("way_y",int(std::round(waypoint.y*1000)));
 put("x",int(std::round(pose.position.x*1000)));put("y",int(std::round(pose.position.y*1000)));put("z",int(std::round(pose.z*1000)));put("yaw",int(std::round(std::remainder(pose.yaw,2*kPi)*10000)));put("phase",int(std::round(pose.phase*10000)));put("walking",pose.clip==1);put("crouched",crouched?1:0);put("grounded",grounded?1:0);put("vv",int(std::round(vv*1000)));put("locomotion",1);put("saved",1);
 // Watch actual local displacement before Streaming converts the actor into
 // another chunk. A deferred search charges no stall time; a searched route
 // or direct movement still reports its physical block with the real elapsed time.
 if(config.mode==ActorAiMode::Guard)guardTaskMovement(actor,old,pose,movementWaypoint,wantsMove,stallPaused?0:guardMs);
 return pose;
}
int Game::nearbyFriendly()const{
 int best=-1;float nearest=1.8f;Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();auto pose=npc.pose;pose.position+=chunkOffset(npc.level)-chunkOffset(m_level);
  if((track.ai.mode!=ActorAiMode::Civilian&&track.ai.mode!=ActorAiMode::Escort)||npcIncapacitated(i)||(track.ai.enableState&&!state(track.ai.enableState))||pose.clip==4||(track.deadState&&state(track.deadState)))continue;
  auto delta=pose.position-m_player.pos;float distance=length(delta);
  Point3 target{pose.position.x+chunkOffset(m_level).x,pose.position.y+chunkOffset(m_level).y,pose.z+track.scale*.7f};
  if(!friendlyRayClear({m_player.pos.x+chunkOffset(m_level).x,m_player.pos.y+chunkOffset(m_level).y,m_player.z+m_player.eye},target))continue;
  if(m_vrInputActive){if(distance>2.4f||!m_vrHandValid[m_vrInteractionHand])continue;auto hand=m_vrHandPosition[m_vrInteractionHand],to=Point3{pose.position.x,pose.position.y,target.z}-hand;distance=std::sqrt(to.x*to.x+to.y*to.y+to.z*to.z);auto direction=m_vrHandDirection[m_vrInteractionHand];float magnitude=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
   if(distance>.65f&&(magnitude<.001f||to.x*direction.x+to.y*direction.y+to.z*direction.z<distance*magnitude*.94f))continue;
   if(!friendlyRayClear({hand.x+chunkOffset(m_level).x,hand.y+chunkOffset(m_level).y,hand.z},target))continue;
  }else if(dot(normalized(delta),forward)<.6f||std::fabs(pose.z-m_player.z)>.6f)continue;
  if(distance<nearest){best=int(i);nearest=distance;}
 }return best;
}
__declspec(noinline) bool Game::testAiQueryBatch(){
 std::ofstream report("ai-query-test.txt");bool passed=true;
 auto check=[&](bool good,const char*name){report<<(good?"PASS ":"FAIL ")<<name<<'\n';report.flush();passed&=good;};
 {
  auto reference=[](int level,size_t actor,StateId timer,std::string_view field){auto text="npc/"+std::to_string(level)+"/"+std::to_string(actor)+"/"+std::to_string(timer)+"/"+std::string(field);StateId hash=2166136261u;for(unsigned char c:text)hash=(hash^c)*16777619u;return hash|0x80000000u;};
  bool exact=true;std::array<std::string,7> fields{"","x","guard_route_next","support_target",std::string(256,'q'),std::string("a\0b",3),std::string("\xc3\xa9\xff",3)};
  for(int level:{std::numeric_limits<int>::min(),-1,0,1,14,std::numeric_limits<int>::max()})
   for(size_t actor:{size_t(0),size_t(1),size_t(63),size_t(128),size_t(UINT32_MAX),std::numeric_limits<size_t>::max()})
    for(StateId timer:{StateId(0),StateId(1),StateId(0x7fffff),StateId(0x80000000u),StateId(UINT32_MAX)})for(const auto&field:fields)
     exact&=actorAiState(level,actor,timer,field)==reference(level,actor,timer,field);
  uint32_t random=0x529b34u;auto next=[&]{random=random*1664525u+1013904223u;return random;};
  for(int i=0;i<10000;++i){int level=int(next()%1000)-500;size_t actor=(size_t(next())<<32)|next();StateId timer=next();std::string field(next()%80,' ');for(auto&c:field)c=char(next()>>24);
   auto full=reference(level,actor,timer,field);exact&=actorAiState(level,actor,timer,field)==full&&actorAiState(level,actor,timer,field)==full;
  }
  check(exact,"NPC state keys match legacy decimal formatting and hashing, including limits, arbitrary bytes and prefix collisions");
  std::array<char,80> mutableField{};
  for(int iteration=0;iteration<1000;++iteration){for(size_t i=0;i<mutableField.size();++i)mutableField[i]=char((iteration*37+int(i)*11)&255);
   std::string_view field(mutableField.data(),size_t(iteration)%mutableField.size());auto expected=reference(14,3,12345,field);
   exact&=actorAiState(14,3,12345,field)==expected&&actorAiState(14,3,12345,field)==expected;}
  check(exact,"Full-field ID memo validates changing bytes at the same address, embedded zero bytes, lengths and uncached long fields");
 }
 {
  std::array<std::vector<StateValue>,2> banks;bool exact=true;uint32_t seed=0x1654ac78u;
  auto next=[&]{seed=seed*1664525u+1013904223u;return seed;};
  for(int i=0;i<30000;++i){auto&values=banks[(next()>>17)&1];StateId id=next()%1536u;if(i&1)id|=0x80000000u;
   auto reference=std::lower_bound(values.begin(),values.end(),id,[](const StateValue&value,StateId key){return value.id<key;});
   exact&=ai::StateLookupHints::lowerBound(values,id)==size_t(reference-values.begin());
   if(next()&1){if(reference==values.end()||reference->id!=id)values.insert(reference,{id,int(next()%3000)-1500});else values.erase(reference);}
   if(i%311==0)std::swap(banks[0],banks[1]);if(i%997==0)banks[1].clear();
  }
  check(exact&&ai::StateLookupHints::statistics.hits>100,"State index hints match lower_bound through insert/erase, missing keys, collisions, vector swaps and clears");
 }
 CollisionQueryBatch::resetStatistics();
 for(int level:{0,1,3,6,-1}){
  World world(level<0?0:level,level<0?WorldId::Ashfall:WorldId::Campaign);bool exact=true;
  CollisionQueryBatch batch(&world);
  for(int y=-1;y<26;y+=3)for(int x=-1;x<26;x+=3)for(float offset:{-.03f,.02f,.3f,1.1f}){
   float px=x+.137f,py=y+.713f,feet=world.floorHeight(px,py)+offset;
   for(bool dynamic:{false,true})for(bool cavities:{false,true})for(float height:{.015f,.8f,1.6f}){
    bool a=world.fits(px,py,feet,height,dynamic,cavities),b=world.fits(px,py,feet,height,dynamic,cavities);float support=world.supportBelow(px,py,feet),again=world.supportBelow(px,py,feet);
    CollisionQueryBatch reference(&world,false);
    exact&=a==b&&b==world.fits(px,py,feet,height,dynamic,cavities)&&std::bit_cast<uint32_t>(support)==std::bit_cast<uint32_t>(again)&&std::bit_cast<uint32_t>(support)==std::bit_cast<uint32_t>(world.supportBelow(px,py,feet));
   }
  }
  check(exact,"Exact query keys preserve height, shelf, dynamic and support results");
 }
 auto initialStats=CollisionQueryBatch::statistics();
 for(int scenario=0;scenario<3;++scenario){
  auto cached=scenario==0?exclusionInspection(12):scenario==1?mapInspection({18,17},kPi*.5f,0,3,false,-9,false):mapInspection({1.4f,20},0,0,14,false,-25,false);
  cached.m_player.health=100;auto reference=cached;cached.m_aiQueryCaching=true;reference.m_aiQueryCaching=false;
  std::array<std::vector<double>,2> timings;
  bool same=true,restored=true;
  for(int frame=0;frame<360&&same;++frame){InputState input{};input.mouseDx=11;input.mouseDy=std::sin(frame*.13f)*1.7f;
   float dt=scenario==1?1.f/120:1.f/60;cached.m_player.health=reference.m_player.health=100;
   for(int pass=0;pass<2;++pass){int mode=(pass+(frame&1))&1;auto begin=std::chrono::steady_clock::now();
    if(mode)cached.update(input,dt);else reference.update(input,dt);
    if(frame>=30)timings[mode].push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
   }
   same=cached.m_states.size()==reference.m_states.size()&&std::equal(cached.m_states.begin(),cached.m_states.end(),reference.m_states.begin(),[](const StateValue&a,const StateValue&b){return a.id==b.id&&a.value==b.value;});
   if(frame%30==0||frame==359)same&=cached.encodeSave()==reference.encodeSave();
   if(frame==180&&same){auto a=cached.encodeSave(),b=reference.encodeSave();restored=cached.decodeSave(a)&&reference.decodeSave(b);cached.m_aiQueryCaching=true;reference.m_aiQueryCaching=false;same&=restored;}
   if(!same)report<<"scenario "<<scenario<<" diverged at frame "<<frame<<'\n';
  }
  check(same&&restored,"Cached/uncached AI state and full saves match throughout replay and restoration");
  for(int mode=0;mode<2;++mode)if(!timings[mode].empty()){auto&v=timings[mode];std::sort(v.begin(),v.end());double sum=0;for(auto ms:v)sum+=ms;
   report<<"scenario "<<scenario<<(mode?" cached":" reference")<<" simulation avg/p95_ms="<<sum/v.size()<<'/'<<v[v.size()*95/100]<<'\n';
  }
 }
 auto stats=CollisionQueryBatch::statistics();report<<"replay queries="<<stats.queries-initialStats.queries<<" hits="<<stats.hits-initialStats.hits<<'\n';
 check(stats.hits-initialStats.hits>100,"Replay actually reuses exact collision queries");
 return passed;
}
bool Game::commandFriendly(){
 int actor=nearbyFriendly();if(actor<0)return false;const auto&track=m_friendlyActors[size_t(actor)].track();bool escort=track.ai.mode==ActorAiMode::Escort;
 auto key=friendlyKey(size_t(actor),escort?"hold":"follow");setState(key,!state(key));
 bool follow=escort?!state(key):state(key)!=0;m_pickupNotice=follow?"WORKER / FOLLOWING":"WORKER / HOLDING POSITION";m_pickupNoticeTime=2;return true;
}
bool Game::testFriendlyAI(){
 std::ofstream report("friendly-ai-test.txt");bool passed=true;auto check=[&](bool ok,const char*label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;};
 auto scene=[](ActorAiMode mode,bool wall=false,int door=0){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Friendly AI validation";campaign->key=192837;
  auto map=std::make_shared<AuthoredMapData>();map->name="NPC validation room";map->definition={{0,0},{16.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');
  if(wall)for(int y=0;y<11;++y)layer.rows[y][10]='#';
  if(door){layer.rows[8]=std::string(24,'#');layer.rows[8][6]='.';Door gate{6,7,8,0,false,false,false};if(door==2)gate.requireState=stateId("npc_gate_permission");map->doors.push_back(gate);}
  map->layers.push_back(layer);TimedSequence timer;timer.timerState=stateId("npc_clock");timer.finishState=stateId("npc_clock_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  ActorTrack worker;worker.timerState=timer.timerState;worker.keys={{0,{6.5f,6.5f},0,0},{30000,{6.5f,6.5f},0,0}};worker.ai.mode=mode;worker.health=70;worker.damageState=stateId("npc_damage");worker.deadState=stateId("npc_dead");map->actorTracks.push_back(worker);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int count,float dt){for(int i=0;i<count;++i){g.updateStreaming(dt);g.updateMechanisms(dt);g.m_world.updateDoors(dt);}};
 {auto g=scene(ActorAiMode::Civilian);check(!g.hullFits({6.5f,6.5f},0,Player::StandingHeight)&&g.hullFits({6.5f,6.5f},3,Player::StandingHeight),"Living NPC body blocks player/enemy hulls without blocking another floor");}
 for(int rate:{60,120}){auto g=scene(ActorAiMode::Escort);tick(g,rate*8,1.f/rate);auto pose=g.world().actorPose(0);
  check(length(pose.position-g.player().pos)<2.4f&&length(pose.position-g.player().pos)>1.2f,"Escort follows and stops outside the player hull at both frame rates");
  Game restored(g.m_customCampaigns.back());check(restored.decodeSave(g.encodeSave())&&length(restored.world().actorPose(0).position-pose.position)<.002f,"Save restores autonomous NPC position without timeline reset");
 }
 {auto g=scene(ActorAiMode::Civilian);tick(g,60,1.f/60);check(length(g.world().actorPose(0).position-Vec2{6.5f,6.5f})<.001f,"Civilian waits until asked to follow");
  g.m_player.pos={7.5f,6.5f};g.m_player.angle=kPi;InputState use{};use.use=true;g.updateInteraction(use,.01f);g.updateInteraction({},.01f);g.m_player.pos={12.5f,6.5f};tick(g,240,1.f/60);
  check(g.world().actorPose(0).position.x>8.5f,"PC use command enables civilian following");
  auto position=g.world().actorPose(0).position;g.m_player.pos=position+Vec2{1,0};g.setVrHand(0,{position.x+.4f,position.y,1.2f},{-1,0,0},true);g.m_vrInteractionHand=0;
  check(g.commandFriendly(),"VR controller reach accepts Follow/Wait command");tick(g,120,1.f/60);check(length(g.world().actorPose(0).position-position)<.001f,"Wait command stops following");
 }
 {auto g=scene(ActorAiMode::Escort,true);tick(g,1200,1.f/60);check(g.world().actorPose(0).position.x>12&&length(g.world().actorPose(0).position-g.player().pos)<2.5f,"Escort routes around a wall instead of pushing into it");}
 for(bool unreachable:{false,true}){auto original=scene(ActorAiMode::Escort,true);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto worker=map->actorTracks[0];map->actorTracks.clear();
  if(unreachable)for(int y=11;y<24;++y)map->layers[0].rows[y][10]='#';
  for(int i=0;i<9;++i){auto npc=worker;for(auto&key:npc.keys)key.position={9.3f,6.5f+i*.25f};map->actorTracks.push_back(npc);}campaign->maps[0]=map;Game crowd(campaign);crowd.updateMechanisms(1.f/60);
  int planned=0;bool stationary=true;float deferredMotion=0;for(size_t i=0;i<9;++i){planned+=crowd.state(actorAiState(0,i,worker.timerState,"repath"))>0;if(i>=4){float moved=length(crowd.world().actorPose(i).position-map->actorTracks[i].keys[0].position);deferredMotion=std::max(deferredMotion,moved);stationary&=moved<.001f;}}
  report<<"crowd unreachable="<<unreachable<<" planned="<<planned<<" budget="<<crowd.m_npcNavBudget<<" deferred_motion="<<deferredMotion<<'\n';
  check(planned==4&&crowd.m_npcNavBudget==0&&stationary,"Crowded scene limits route searches and keeps deferred actors stationary");
  crowd.updateMechanisms(1.f/60);crowd.updateMechanisms(1.f/60);planned=0;for(size_t i=0;i<9;++i)planned+=crowd.state(actorAiState(0,i,worker.timerState,"repath"))>0;
  check(planned==9,"Reachable and unreachable goals do not starve later actors");
 }
 for(int locked:{0,1}){auto g=scene(ActorAiMode::Escort,false,locked?2:1);g.m_player.pos={6.5f,11.5f};tick(g,480,1.f/60);
  check(locked?(!g.world().doors()[0].opening&&g.world().actorPose(0).position.y<8):(g.world().doors()[0].opening&&g.world().actorPose(0).position.y>8.5f),"AI opens ordinary doors and preserves locked-door requirements");
 }
 {auto g=scene(ActorAiMode::Civilian);Enemy threat{};threat.pos={4.5f,7.5f};g.m_enemies.push_back(threat);float before=length(g.world().actorPose(0).position-threat.pos);tick(g,120,1.f/60);
  auto position=g.world().actorPose(0).position;check(length(position-threat.pos)>before+1.f,"Civilian flees a visible enemy");
  g.m_enemies[0].alive=false;Game restored(g.m_customCampaigns.back());check(restored.decodeSave(g.encodeSave()),"Save restores fear memory");tick(restored,60,1.f/60);check(length(restored.world().actorPose(0).position-threat.pos)>length(position-threat.pos),"Civilian continues escape briefly after losing the threat");
 }
 {auto g=scene(ActorAiMode::Escort);tick(g,120,1.f/60);auto position=g.world().actorPose(0).position;g.m_player.pos=position+Vec2{2,0};g.m_player.angle=kPi;g.m_player.pitch=0;g.shoot();g.shoot();g.shoot();g.updateMechanisms(.4f);
  auto phase=g.world().actorPose(0).phase;Game restored(g.m_customCampaigns.back());check(phase>0&&phase<1&&restored.decodeSave(g.encodeSave())&&std::fabs(restored.world().actorPose(0).phase-phase)<.002f,"Death animation progresses and restores mid-animation");tick(g,120,1.f/60);
  check(g.world().actorPose(0).clip==4&&length(g.world().actorPose(0).position-position)<.002f,"Killed autonomous worker freezes at actual death position");
 }
 {auto g=scene(ActorAiMode::Civilian);Enemy attacker{};attacker.pos={5.8f,6.5f};attacker.heading=0;g.m_enemies.push_back(attacker);
  g.updateEnemies(.01f);check(g.m_enemies[0].windup>0,"Enemy acquires a nearby friendly NPC");
  for(int i=0;i<45;++i)g.updateEnemies(1.f/60);check(g.state("npc_damage")>0&&g.player().health==100,"Enemy contact attack damages the NPC, not a distant player");
  g.damageFriendly(0,100,g.m_enemies[0].pos);g.updateEnemies(.01f);check(g.state("npc_dead")&&g.world().actorPose(0).clip==4&&g.kills()==0,"NPC death clears targeting without awarding a player kill");
 }
 for(bool south:{false,true})for(bool blocked:{false,true}){
  auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);campaign->maps.clear();
  for(int index=0;index<3;++index){auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);int coordinate=index==0?0:index==1?2:1;map->definition.origin=south?Vec2{0,float(coordinate*24)}:Vec2{float(coordinate*24),0};map->definition.residencyGroup=100+index;
   if(index){map->actorTracks.clear();map->timedSequences.clear();}else for(auto&key:map->actorTracks[0].keys)key.position=south?Vec2{6.5f,22.3f}:Vec2{22.3f,6.5f};
   map->openEast=!south&&(index!=1)&&!(blocked&&index==0);map->openWest=!south&&index!=0;
   map->openSouth=south&&(index!=1)&&!(blocked&&index==0);map->openNorth=south&&index!=0;
   campaign->maps.push_back(map);
  }
  Game g(campaign);g.m_player.pos=south?Vec2{6.5f,52.5f}:Vec2{52.5f,6.5f};g.crossChunkBoundary();g.updateStreaming(0);tick(g,360,1.f/60);
  if(blocked){check(g.friendlyActors()[0].level==0,"Closed stitched edge stops the escort without teleporting");continue;}
  check(g.friendlyActors().size()==1&&g.friendlyActors()[0].level==2,"Escort physically enters non-sequential intermediate chunk without duplication");
  Game loaded(campaign);bool restored=loaded.decodeSave(g.encodeSave());check(restored&&loaded.friendlyActors().size()==1&&loaded.friendlyActors()[0].level==2&&length(loaded.friendlyActors()[0].pose.position-g.friendlyActors()[0].pose.position)<.002f,"Save restores escort location and identity after crossing a seam");
  if(!restored)continue;tick(loaded,1440,1.f/60);const auto&npc=loaded.friendlyActors()[0];
  check(npc.level==1&&length(npc.pose.position-loaded.player().pos)<2.4f,"Escort follows across two stitched chunks with arbitrary authoring IDs");
  loaded.m_player.pos=npc.pose.position+Vec2{1,0};loaded.m_player.angle=kPi;auto position=npc.pose.position;
  check(loaded.commandFriendly(),"Migrated escort still accepts commands in a chunk with no authored NPC slot");tick(loaded,120,1.f/60);
  check(length(loaded.friendlyActors()[0].pose.position-position)<.001f,"Wait command retains the original NPC identity after migration");
  loaded.damageFriendly(0,100,loaded.player().pos);tick(loaded,120,1.f/60);Game corpse(campaign);
  check(corpse.decodeSave(loaded.encodeSave())&&corpse.friendlyActors()[0].level==1&&corpse.friendlyActors()[0].pose.clip==4,"Migrated NPC death restores in the destination chunk");
  Game returning(campaign);bool returnSave=returning.decodeSave(g.encodeSave());if(returnSave){auto global=south?Vec2{6.5f,4.5f}:Vec2{4.5f,6.5f};returning.m_player.pos=global-returning.chunkOffset(returning.level());returning.crossChunkBoundary();tick(returning,1800,1.f/60);}
  check(returnSave&&returning.friendlyActors()[0].level==0&&length(returning.friendlyActors()[0].pose.position-returning.player().pos)<2.4f,"Escort can reverse direction and return through stitched seams");
 }
 auto seamScene=[&](bool door,bool raised=false){
  auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);campaign->maps.clear();
  for(int level=0;level<2;++level){auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);map->definition.origin={0,float(level*24)};map->definition.residencyGroup=100+level;
   if(level){map->actorTracks.clear();map->timedSequences.clear();}else for(auto&key:map->actorTracks[0].keys)key.position={6.5f,23.3f};
   if(raised&&level)map->layers[0].elevation=3;
   if(door){Door gate{};gate.left=5;gate.right=8;gate.y=level?0:24;gate.entry=level!=0;gate.transfer=level==0;map->doors.push_back(gate);}
   else{map->openNorth=level!=0;map->openSouth=level==0;}
   campaign->maps.push_back(map);
  }return Game(campaign);
 };
 {auto g=seamScene(false);g.m_player.pos={6.5f,24.1f};g.crossChunkBoundary();g.updateStreaming(0);tick(g,180,1.f/60);auto npc=g.friendlyActors()[0];auto global=npc.pose.position+g.chunkOffset(npc.level);
  check(length(global-(g.player().pos+g.chunkOffset(g.level())))>=.449f,"Escort stops outside the player hull when player occupies the seam");
  check(!g.hullFits(global-g.chunkOffset(g.level()),npc.pose.z,Player::StandingHeight),"Migrating NPC body blocks player collision across chunk ownership");
  g.m_player.angle=-kPi*.5f;check(g.commandFriendly(),"Follow/Wait works across an open seam before NPC ownership changes");
 }
 for(bool open:{false,true}){auto g=seamScene(true);g.m_player.pos={6.5f,28.5f};g.crossChunkBoundary();g.updateStreaming(0);
  if(open){g.m_world.setDoor(0,1,true);g.m_chunks[0].world.setDoor(0,1,true);}
  tick(g,480,1.f/60);
  check(open?g.friendlyActors()[0].level==1:(g.friendlyActors()[0].level==0&&!g.m_world.doors()[0].opening),"Escort uses open paired transfer doors and never forces a closed transfer");
  if(!open){g.m_player.pos={6.5f,.1f};g.m_player.angle=-kPi*.5f;check(!g.commandFriendly(),"Closed seam prevents remote NPC commands through its door");}
 }
 {auto g=seamScene(false,true);g.m_player.pos={6.5f,28.5f};g.crossChunkBoundary();g.m_player.z=3;tick(g,480,1.f/60);
  check(g.friendlyActors()[0].level==0&&std::fabs(g.friendlyActors()[0].pose.z)<.001f,"Unsafe elevation discontinuity stops NPC instead of teleporting or falling");
 }
 {auto g=seamScene(false);g.setState(g.friendlyKey(0,"chunk"),3);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Save rejects an out-of-range physical NPC chunk");}
 {auto g=seamScene(false);tick(g,1,1.f/60);g.setState(g.friendlyKey(0,"x"),24000);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Save rejects invalid NPC coordinates even for unloaded chunks");}
 {auto original=seamScene(true);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto worker=map->actorTracks[0];map->actorTracks.clear();
  for(int i=0;i<9;++i){auto npc=worker;for(auto&key:npc.keys)key.position={2.5f+float(i),22.f};map->actorTracks.push_back(npc);}campaign->maps[0]=map;Game g(campaign);
  g.m_player.pos={6.5f,42.f};g.crossChunkBoundary();tick(g,1,1.f/60);int planned=0;for(size_t i=0;i<9;++i)planned+=g.state(g.friendlyKey(i,"route_wait"))>0;
  check(!g.chunkResident(0)&&planned==4&&g.m_npcNavBudget==0,"Unloaded NPCs share the bounded portal-search budget without loading a closed route");
  tick(g,2,1.f/60);planned=0;for(size_t i=0;i<9;++i)planned+=g.state(g.friendlyKey(i,"route_wait"))>0;tick(g,1,1.f/60);
  check(planned==9&&g.m_npcNavBudget==4,"Failed unloaded portal searches back off and do not starve later actors");
 }
 {auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);campaign->maps.clear();
  for(int level=0;level<3;++level){auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);map->definition.origin=level==0?Vec2{}:level==1?Vec2{24,24}:Vec2{24,0};map->definition.residencyGroup=100+level;
   if(level){map->actorTracks.clear();map->timedSequences.clear();}else for(auto&key:map->actorTracks[0].keys)key.position={22.3f,6.5f};
   map->openEast=level==0;map->openWest=level==2;map->openSouth=level==2;map->openNorth=level==1;campaign->maps.push_back(map);
  }
  Game g(campaign);g.m_player.pos={30.5f,30.5f};g.crossChunkBoundary();tick(g,2400,1.f/60);
  check(g.friendlyActors()[0].level==1&&length(g.friendlyActors()[0].pose.position-g.player().pos)<2.4f,"Escort routes around a two-axis stitched turn with streaming enabled");
 }
 try{
  check(true,"NPC locomotion suite begins");
  {auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);
   for(auto&key:map->actorTracks[0].keys)key.position={8.5f,6.5f};map->actorTracks[0].scale=1.6f;
   map->fixtures.push_back({6,{11.5f,6.5f},0,1.8f,1.4f,.7f,0,true});campaign->maps[0]=map;Game g(campaign);
   g.m_player.pos={12.4f,6.5f};g.m_player.z=.7f;tick(g,900,1.f/60);auto pose=g.world().actorPose(0);
   check(pose.z>.55f&&pose.position.x>10.3f,"Escort jumps and climbs a .7 m solid ledge to follow the player");
   Game restored(g.m_customCampaign);check(restored.decodeSave(g.encodeSave())&&std::fabs(restored.world().actorPose(0).z-pose.z)<.002f&&restored.state(restored.friendlyKey(0,"locomotion")),"Save restores NPC climb height and locomotion");
  }
  auto beamScene=[&](float clearance,float followDistance=2.f){auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);
   auto&track=map->actorTracks[0];for(auto&key:track.keys)key.position={9.5f,6.5f};track.scale=1.6f;track.ai.followDistance=followDistance;
   // Span the room so a passing result cannot come from going around the beam.
   map->structures.push_back({10.9f,0,14.1f,24,clearance,clearance+.55f});campaign->maps[0]=map;return Game(campaign);
  };
  for(int rate:{60,120}){auto g=beamScene(1.4f);g.m_player.pos={14.5f,6.5f};tick(g,rate*8,1.f/rate);auto pose=g.friendlyActors()[0].pose;const auto&track=g.friendlyActors()[0].track();float height=g.friendlyHullHeight(0,track);
   check(g.state(g.friendlyKey(0,"crouched"))&&std::fabs(height-npcCrouchedHeight(track))<.001f&&height>Player::CrouchedHeight+.5f,"Escort anticipates 1.4 m clearance using its own kneeling hull at both frame rates");
   check(pose.position.x>11.5f&&g.friendlyBodyFits(0,pose.position,pose.z,height)&&!g.friendlyBodyFits(0,pose.position,pose.z,track.scale),"Crouched escort physically enters a room-wide beam that blocks its standing body");
   Game restored(g.m_customCampaign);bool saved=restored.decodeSave(g.encodeSave());check(saved&&restored.state(restored.friendlyKey(0,"crouched"))&&std::fabs(restored.friendlyHullHeight(0,restored.friendlyActors()[0].track())-height)<.001f,"Save retains the NPC-specific crouched hull inside the low passage");
   tick(g,rate*2,1.f/rate);check(g.state(g.friendlyKey(0,"crouched"))&&g.friendlyBodyFits(0,g.friendlyActors()[0].pose.position,g.friendlyActors()[0].pose.z,height),"Escort stays crouched beneath overhead geometry without a player crouch command");
   g.m_player.pos={8.5f,6.5f};tick(g,rate*8,1.f/rate);pose=g.friendlyActors()[0].pose;
   check(pose.position.x<10.7f&&!g.state(g.friendlyKey(0,"crouched"))&&std::fabs(g.friendlyHullHeight(0,track)-track.scale)<.001f,"Escort exits the beam and restores standing clearance at both frame rates");
  }
  for(int rate:{60,120}){auto g=beamScene(.9f,1.2f);g.m_player.pos={12.5f,6.5f};g.m_player.crouched=true;g.m_player.eye=Player::CrouchedEye;bool safe=true,kneeling=false;
   for(int i=0;i<rate*8;++i){tick(g,1,1.f/rate);const auto&npc=g.friendlyActors()[0];float height=g.friendlyHullHeight(0,npc.track());safe&=npc.pose.position.x<=10.7f&&g.friendlyBodyFits(0,npc.pose.position,npc.pose.z,height);kneeling|=g.state(g.friendlyKey(0,"crouched"))!=0;}
   check(safe&&kneeling&&g.m_player.hullHeight()<.9f&&npcCrouchedHeight(g.friendlyActors()[0].track())>.9f,"Kneeling NPC refuses a .9 m player-only passage without penetrating its beam");
   g.m_player.pos={8.5f,6.5f};g.m_player.crouched=false;g.m_player.eye=Player::StandingEye;tick(g,rate*6,1.f/rate);const auto&npc=g.friendlyActors()[0];
   check(!g.state(g.friendlyKey(0,"crouched"))&&length(npc.pose.position-g.m_player.pos)<1.3f&&g.friendlyBodyFits(0,npc.pose.position,npc.pose.z,npc.track().scale),"Blocked escort recovers, follows a reachable goal and stands after the player leaves the low passage");
  }
  for(int gun:{1,2}){auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto&track=map->actorTracks[0];
   for(auto&key:track.keys){key.position={6.5f,6.5f};key.yaw=kPi*.5f;}track.ai.mode=ActorAiMode::Guard;track.ai.armed=true;track.ai.weapon=gun;track.visual=gun==2?ActorVisual::CleanupMedic:ActorVisual::Cleanup;track.scale=1.6f;track.health=100;track.ai.patrol={{{6.5f,6.5f},0,1000}};
   map->structures.push_back({5.3f,5.3f,7.7f,7.7f,1.4f,1.95f});campaign->maps[0]=map;Game g(campaign);Point3 aim{10.5f,6.5f,1.1f};auto standing=g.npcWeaponPoseAt(0,g.friendlyActors()[0].pose,aim,1);
   auto supportHeight=[](const NpcWeaponPose&w){return w.handgun?w.grip.z:w.grip.z-w.direction.z*.249f-std::hypot(w.direction.x,w.direction.y)*.0065f;};
   auto kneeling=g.friendlyActors()[0].pose;kneeling.clip=5;auto candidate=g.npcWeaponPoseAt(0,kneeling,aim,1);float candidateDrop=supportHeight(standing)-supportHeight(candidate);check(candidate.crouched&&candidateDrop>.20f&&candidateDrop<.70f,"A kneeling pose candidate lowers its gun onto the animated shoulders before persisted locomotion changes");
   if(gun==1){auto down=g.npcWeaponPoseAt(0,g.friendlyActors()[0].pose,{10.5f,6.5f,-1.f},1),up=g.npcWeaponPoseAt(0,g.friendlyActors()[0].pose,{10.5f,6.5f,3.f},1);check(down.grip.z<standing.grip.z-.08f&&up.grip.z>standing.grip.z+.08f&&std::fabs(supportHeight(up)-supportHeight(down))<.05f,"Signed rifle aim pivots the physical grip around a seated stock rather than raising the stock above the shoulder");}
   g.setState(g.friendlyKey(0,"guard_under_fire"),1500);g.setState(g.friendlyKey(0,"weapon_reload"),500);
   for(int i=0;i<20;++i){g.updateStreaming(1.f/60);g.updateMechanisms(1.f/60);g.updateFriendlyActors(1.f/60,true);g.m_world.updateDoors(1.f/60);}
   const auto&npc=g.friendlyActors()[0];float height=g.friendlyHullHeight(0,npc.track());auto weapon=g.npcWeaponPoseAt(0,npc.pose,aim,1);
   check(g.state(g.friendlyKey(0,"crouched"))&&height>1.3f&&height<1.4f&&g.friendlyBodyFits(0,npc.pose.position,npc.pose.z,height),"Reloading or threatened guard retains a body-sized defensive crouch under valid cover");
   float supportDrop=supportHeight(standing)-supportHeight(weapon);check(weapon.armed&&weapon.crouched&&weapon.handgun==(gun==2)&&supportDrop>.20f&&supportDrop<.70f&&weapon.muzzle.z<1.4f,"Handgun and long gun supports follow the animated crouch inside the conservative collision hull");
   auto animated=npc.pose;animated.clip=1;auto moving=g.npcWeaponPoseAt(0,animated,aim,1);check(std::fabs(moving.grip.z-weapon.grip.z)<.001f,"Crouched movement retains its lowered gun grip while the walk clip is active");
   auto hit=g.traceShot({4.5f,npc.pose.position.y,1.1f},{1,0,0},4);check(hit.kind==ShotKind::Npc&&hit.index==0,"Hitscan still strikes a kneeling guard's upper body above the player's crouched hull");
   auto body=Point3{npc.pose.position.x,npc.pose.position.y,weapon.grip.z};check(g.friendlyRayClear(body,weapon.grip)&&g.friendlyRayClear(weapon.grip,weapon.muzzle),"Crouched physical gun has clear shoulder-to-grip and grip-to-muzzle segments under the beam");
   Point3 tip=weapon.muzzle+weapon.direction*4;float t=(aim.x-weapon.muzzle.x)/weapon.direction.x;auto impact=weapon.muzzle+weapon.direction*t;
   check(weapon.direction.x>.9f&&t>0&&std::fabs(impact.y-aim.y)<.001f&&std::fabs(impact.z-aim.z)<.001f&&g.friendlyRayClear(weapon.muzzle,tip),"Lowered crouch muzzle still aims physically at the intended target beneath cover");
  }
  {auto original=scene(ActorAiMode::Escort);auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);
   for(auto&key:map->actorTracks[0].keys){key.position={8.5f,8.5f};key.z=1.25f;key.clip=0;key.phase=0;}
   map->layers[0].elevation=1.25f;map->definition.spawnHeight=1.25f;
   map->actorTracks[0].ai.mode=ActorAiMode::Guard;map->actorTracks[0].ai.armed=true;map->actorTracks[0].ai.enableState=stateId("lab_squad_enabled");
   map->actorTracks[0].ai.patrol={{{8.5f,8.5f},1.25f,1000}};map->actorTracks[0].scale=1.6f;campaign->maps[0]=map;Game g(campaign);
   float phase=g.friendlyActors()[0].pose.phase;
   for(int i=0;i<45;++i){g.updateStreaming(1.f/60);g.updateMechanisms(1.f/60);g.updateFriendlyActors(1.f/60,true);}
   check(g.friendlyActors()[0].pose.clip==0&&g.friendlyActors()[0].pose.phase>phase+.1f&&!g.state("lab_squad_enabled"),"Disabled Guard Lab squad members still advance idle phase before their enable trigger");
    auto staged=g.friendlyActors()[0].pose;
    check(length(staged.position-Vec2{8.5f,8.5f})<.001f&&std::fabs(staged.z-1.25f)<.001f&&g.state(g.friendlyKey(0,"saved"))&&g.state(g.friendlyKey(0,"x"))==8500&&g.state(g.friendlyKey(0,"y"))==8500&&g.state(g.friendlyKey(0,"z"))==1250,
     "Disabled squad staging publishes complete authored nonzero XYZ before marking its animated pose saved");
    Game restored(campaign);bool decoded=restored.decodeSave(g.encodeSave());
    check(decoded&&length(restored.friendlyActors()[0].pose.position-staged.position)<.002f&&std::fabs(restored.friendlyActors()[0].pose.z-staged.z)<.002f&&!restored.state("lab_squad_enabled"),
     "A disabled elevated squad member restores at its authored physical position before activation");
   g.setState(stateId("lab_squad_enabled"),1);float armedPhase=g.friendlyActors()[0].pose.phase;
    g.updateStreaming(1.f/60);g.updateMechanisms(1.f/60);g.updateFriendlyActors(1.f/60,true);
    check(length(g.friendlyActors()[0].pose.position-staged.position)<.05f&&std::fabs(g.friendlyActors()[0].pose.z-staged.z)<.002f,
     "First activation restores the complete staged pose rather than an origin pose with an idle phase");
    if(decoded){restored.setState(stateId("lab_squad_enabled"),1);restored.updateStreaming(1.f/60);restored.updateMechanisms(1.f/60);restored.updateFriendlyActors(1.f/60,true);
     check(length(restored.friendlyActors()[0].pose.position-staged.position)<.05f&&std::fabs(restored.friendlyActors()[0].pose.z-staged.z)<.002f,
      "A restored staged guard activates at the same nonzero authored XYZ without a teleport");}
   for(int i=0;i<90;++i){g.updateStreaming(1.f/60);g.updateMechanisms(1.f/60);g.updateFriendlyActors(1.f/60,true);}
   check(g.friendlyActors()[0].pose.phase!=armedPhase||g.friendlyActors()[0].pose.clip==1,"Enabled idle/patrol guards keep cycling animation phase");
  }
 }catch(const std::exception&ex){check(false,ex.what());}
 return passed;
}
}

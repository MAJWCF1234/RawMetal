#include "Game.h"
namespace retro {
namespace {
Point3 unit(Point3 p){float length=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);return length>.0001f?p*(1/length):Point3{};}
}
NpcWeaponPose Game::npcWeaponPoseAt(size_t actor,const ActorPose& pose,Point3 target,float raise)const{
 const auto&track=m_friendlyActors.at(actor).track();NpcWeaponPose weapon;
 if(track.ai.mode!=ActorAiMode::Guard||!track.ai.armed)return weapon;
 weapon.armed=true;weapon.handgun=guardHandgun(track.ai);weapon.raise=std::clamp(raise,0.f,1.f);
 float scale=track.scale/1.6f;Point3 forward{std::sin(pose.yaw),std::cos(pose.yaw),0},right{forward.y,-forward.x,0};
 float gripForward=track.visual==ActorVisual::Worker?.40f:.31f;
 weapon.grip=Point3{pose.position.x,pose.position.y,pose.z}+right*(-.12f*scale)+forward*(gripForward*scale)+Point3{0,0,(1.024f+.216f*weapon.raise)*scale};
 weapon.direction=unit(forward+Point3{0,0,-.32f});
 if(weapon.raise>0){auto aimed=unit(target-weapon.grip);
  for(int i=0;i<3;++i){float horizontal=std::hypot(aimed.x,aimed.y);auto r=horizontal>.001f?Point3{aimed.y/horizontal,-aimed.x/horizontal,0}:right;Point3 up{r.y*aimed.z,-r.x*aimed.z,r.x*aimed.y-r.y*aimed.x};aimed=unit(target-weapon.grip-up*((weapon.handgun?.035f:.058961f)*scale)-r*((weapon.handgun?0.f:-.007496f)*scale));}
  weapon.direction=unit(weapon.direction*(1-weapon.raise)+aimed*weapon.raise);
 }
 float horizontal=std::hypot(weapon.direction.x,weapon.direction.y);auto r=horizontal>.001f?Point3{weapon.direction.y/horizontal,-weapon.direction.x/horizontal,0}:right;Point3 up{r.y*weapon.direction.z,-r.x*weapon.direction.z,r.x*weapon.direction.y-r.y*weapon.direction.x};
 weapon.muzzle=weapon.grip+weapon.direction*((weapon.handgun?.172f:.500522f)*scale)+up*((weapon.handgun?.035f:.058961f)*scale)+r*((weapon.handgun?0.f:-.007496f)*scale);return weapon;
}
NpcWeaponPose Game::npcWeaponPose(size_t actor)const{
 const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();NpcWeaponPose weapon;
 if(track.ai.mode!=ActorAiMode::Guard||!track.ai.armed||!autonomousActor(track)||npc.pose.clip==4||(track.deadState&&state(track.deadState)))return weapon;
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};float raise=get("weapon_raise")*.001f;
 if(get("weapon_target")){auto shift=chunkOffset(npc.level);Point3 target{get("weapon_x")*.001f-shift.x,get("weapon_y")*.001f-shift.y,get("weapon_z")*.001f},cover;if(guardSuppressionPoint(actor,cover))target=cover;weapon=npcWeaponPoseAt(actor,npc.pose,target,raise);}
 else{
  // A raised saved pose without an aim target retains its low-ready direction.
  weapon=npcWeaponPoseAt(actor,npc.pose,{},0);float lift=.216f*raise*track.scale/1.6f;weapon.grip.z+=lift;weapon.muzzle.z+=lift;weapon.raise=raise;
 }
 weapon.flash=get("weapon_flash")/80.f;weapon.recoil=get("weapon_recoil")/220.f;
 weapon.treatment=get("support_ms")/2400.f;
 weapon.action=GuardAction(get("weapon_action"));weapon.actionProgress=guardActionDuration(weapon.action)?float(get("weapon_action_ms"))/guardActionDuration(weapon.action):0;
 weapon.reload=get("weapon_reloading")?1.f-get("weapon_reload")/1800.f:0;
 int grenade=get("grenade_windup");weapon.grenadeThrow=grenade>0?grenade/450.f+.0001f:0;
 // Both hands follow this cosmetic assembly offset in the shared renderer.
 // Recovery prevents firing from the transient bash pose.
 float bash=get("weapon_bash_pose")/220.f,windup=get("weapon_bash")/300.f;
 auto lunge=weapon.direction*((bash>.0f?.12f*bash:.045f*windup)*track.scale/1.6f);weapon.grip=weapon.grip+lunge;weapon.muzzle=weapon.muzzle+lunge;return weapon;
}
ShotHit Game::guardShot(size_t actor,const ActorPose& pose,const NpcWeaponPose& weapon)const{
 const auto&npc=m_friendlyActors.at(actor);if(!weapon.armed)return {};
 auto shift=chunkOffset(npc.level)-chunkOffset(m_level);auto grip=weapon.grip+Point3{shift.x,shift.y,0},muzzle=weapon.muzzle+Point3{shift.x,shift.y,0};
 auto segment=[&](Point3 from,Point3 to){auto direction=to-from;float distance=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);if(distance<=.0001f){ShotHit blocked;blocked.kind=ShotKind::Solid;blocked.level=npc.level;blocked.point=from;return blocked;}return traceShot(from,direction,distance,int(actor),false);};
 // Both candidate evaluation and actual firing use the same physical barrel
 // clearance, with this NPC's own hull ignored and all points in active space.
 auto shoulder=Point3{pose.position.x+shift.x,pose.position.y+shift.y,grip.z};auto blocked=segment(shoulder,grip);if(blocked.kind!=ShotKind::None)return blocked;
 blocked=segment(grip,muzzle);if(blocked.kind!=ShotKind::None)return blocked;
 return traceShot(muzzle,weapon.direction,npc.track().ai.attackRange,int(actor),true);
}
void Game::updateGuardCombat(float dt){
 if(dt<=0)return;updateGuardSquadSlots(dt);
 for(size_t i=0;i<m_friendlyActors.size();++i){auto&npc=m_friendlyActors[i];const auto&track=npc.track();const auto&config=track.ai;if(config.mode!=ActorAiMode::Guard||!config.armed||!chunkResident(npc.level))continue;
  const auto&gun=weaponDef(guardHandgun(config)?WeaponId::Handgun:WeaponId::Shotgun);
  auto get=[&](const char*field){return state(friendlyKey(i,field));};auto put=[&](const char*field,int value){setState(friendlyKey(i,field),value);};
  auto cancelAction=[&](){put("weapon_action",0);put("weapon_action_ms",0);put("weapon_action_events",0);put("weapon_bash",0);put("weapon_bash_pose",0);};
  if(!autonomousActor(track)||npc.pose.clip==4||(track.deadState&&state(track.deadState))){put("weapon_aim",0);put("weapon_raise",0);put("weapon_flash",0);put("weapon_recoil",0);put("guard_suppress_ms",0);put("weapon_reload",0);put("weapon_reloading",0);put("weapon_reload_events",0);cancelAction();continue;}
  if(get("drone_busy")||get("support_ms")){put("weapon_aim",0);put("weapon_raise",0);continue;}
  int ms=npcMilliseconds(i,dt,"weapon_fraction");
  if(get("guard_danger_ms")){
   put("weapon_recovery",std::max(0,get("weapon_recovery")-ms));put("weapon_flash",std::max(0,get("weapon_flash")-ms));put("weapon_recoil",std::max(0,get("weapon_recoil")-ms));
   // The pending reload is deliberately paused before any refill/event work.
   // Consumed ammunition and attack recovery continue to describe the past.
   put("weapon_aim",0);put("weapon_raise",0);continue;
  }
  auto shift=chunkOffset(npc.level)-chunkOffset(m_level);
  auto pump=[&](float gain,float pitch){m_sounds.push_back({Sound::Pump,npc.pose.position+shift,gain,pitch,true,17,npc.pose.z+track.scale*.7f});};
  auto phase=[&](GuardAction action,int elapsed,unsigned consumed){put("weapon_action",int(action));put("weapon_action_ms",elapsed);put("weapon_action_events",int(consumed));};
  auto action=GuardAction(get("weapon_action"));int elapsed=get("weapon_action_ms");unsigned consumed=unsigned(get("weapon_action_events"));
  // Older saves lack the timeline fields. Adopt their already consumed phase,
  // never retrospectively replaying a strike, flash, shell or refill.
  if(action==GuardAction::None&&get("weapon_bash")){action=GuardAction::Bash;elapsed=get("weapon_bash");consumed=guardActionMask(action,elapsed);phase(action,elapsed,consumed);}
  else if(action==GuardAction::None&&get("weapon_bash_pose")){action=GuardAction::Bash;elapsed=520-get("weapon_bash_pose");consumed=guardActionMask(action,elapsed);phase(action,elapsed,consumed);}
  if(action==GuardAction::Fire||(action==GuardAction::Bash&&elapsed>=300)){
   elapsed=std::min(guardActionDuration(action),elapsed+std::min(ms,1800));auto events=consumeGuardAction(action,elapsed,consumed);
   if(action==GuardAction::Fire&&!guardHandgun(config)&&(events&GuardActionWork))pump(.3f,.9f);
   if(action==GuardAction::Bash)put("weapon_bash_pose",std::max(0,520-elapsed));
   if(events&GuardActionComplete){cancelAction();action=GuardAction::None;}else phase(action,elapsed,consumed);
  }
  if(!get("weapon_saved")){put("weapon_rounds",gun.cycle.tubeCapacity);put("weapon_saved",1);put("weapon_reload",0);put("weapon_reloading",0);put("weapon_reload_events",0);}
  // A loaded tube cannot also be in its empty-tube reload interval. Normalize
  // stale in-memory flags as well as rejecting inconsistent saves on decode.
  if(get("weapon_rounds")>0&&(get("weapon_reload")||get("weapon_reloading"))){put("weapon_reload",0);put("weapon_reloading",0);put("weapon_reload_events",0);}
  int recovery=std::max(0,get("weapon_recovery")-ms),oldReload=get("weapon_reload"),reload=std::max(0,oldReload-ms);put("weapon_recovery",recovery);put("weapon_reload",reload);put("weapon_flash",std::max(0,get("weapon_flash")-ms));put("weapon_recoil",std::max(0,get("weapon_recoil")-ms));
  if(get("weapon_rounds")==0){
   if(get("weapon_reloading")){
    unsigned reloadEvents=unsigned(get("weapon_reload_events"));if(!reloadEvents)reloadEvents=guardActionMask(GuardAction::Reload,1800-oldReload);
    auto events=consumeGuardAction(GuardAction::Reload,1800-reload,reloadEvents);put("weapon_reload_events",int(reloadEvents));
    if(events&GuardActionWork)pump(.25f,.8f);if(events&GuardActionChamber)pump(.3f,.85f);
    if(events&GuardActionComplete){put("weapon_rounds",gun.cycle.tubeCapacity);put("weapon_reloading",0);put("weapon_reload_events",0);if(action==GuardAction::Reload){cancelAction();action=GuardAction::None;}}
   }else{
    put("weapon_reloading",1);put("weapon_reload",1800);put("weapon_reload_events",GuardActionStart);reload=1800;pump(.4f,.75f);
   }
  }
  if(get("weapon_reloading")&&(action==GuardAction::None||action==GuardAction::Reload)){action=GuardAction::Reload;phase(action,1800-reload,unsigned(get("weapon_reload_events")));}
  else if(!get("weapon_reloading")&&action==GuardAction::Reload){cancelAction();action=GuardAction::None;}
  auto target=guardTarget(i);int kind=target.valid()?int(target.kind):0,targetLevel=target.valid()?target.level+1:0,targetIndex=target.valid()?target.index+1:0;
  if(kind!=get("weapon_target_kind")||targetLevel!=get("weapon_target_level")||targetIndex!=get("weapon_target_index")){put("weapon_aim",0);put("weapon_bash",0);put("guard_suppress_ms",0);if(action==GuardAction::Bash){cancelAction();action=GuardAction::None;}put("weapon_target_kind",kind);put("weapon_target_level",targetLevel);put("weapon_target_index",targetIndex);}
  Point3 aimPoint{target.position.x,target.position.y,target.feet+target.height*.6f};auto delta=aimPoint-Point3{npc.pose.position.x,npc.pose.position.y,npc.pose.z+track.scale*.85f};
  bool visible=target.valid()&&guardState(i)==GuardState::Alert&&delta.x*delta.x+delta.y*delta.y+delta.z*delta.z<config.attackRange*config.attackRange&&guardCanSee(i,npc.pose,target);
  if(visible){auto global=target.position+chunkOffset(npc.level);put("weapon_target",1);put("weapon_x",int(std::round(global.x*1000)));put("weapon_y",int(std::round(global.y*1000)));put("weapon_z",int(std::round(aimPoint.z*1000)));}
  else if(!target.valid())put("weapon_target",0);
  put("guard_suppress_ms",guardSuppressionEnabled(config)&&visible?1200:std::max(0,get("guard_suppress_ms")-ms));
  if(get("support_target")){put("weapon_aim",0);put("weapon_raise",0);put("weapon_bash",0);put("grenade_windup",0);if(action==GuardAction::Bash)cancelAction();continue;}
  Point3 coverPoint;bool suppress=!visible&&guardSuppressionPoint(i,coverPoint);
  int grenadesUsed=get("grenade_used");tryGuardGrenade(i,dt);if(get("grenade_windup")>0||get("grenade_used")!=grenadesUsed){put("weapon_aim",0);put("weapon_raise",0);if(action==GuardAction::Bash){cancelAction();action=GuardAction::None;}continue;}
  bool close=visible&&guardMeleeReady(i,npc.pose,target);
  int raised=(visible||suppress)&&(!reload||close)&&npc.pose.clip!=1?1000:0;int current=get("weapon_raise");put("weapon_raise",current+std::clamp(raised-current,-ms*3,ms*3));
  // At contact range a finite, interruptible weapon bash takes precedence.
  // It is usable with an empty tube, and never spends ammunition or shares a
  // frame with a firearm attack. The live target/cover test runs every tick.
  if(close&&!recovery&&npc.pose.clip!=1){
   put("weapon_aim",0);if(get("weapon_raise")<1000){put("weapon_bash",0);continue;}
   if(action!=GuardAction::Bash){action=GuardAction::Bash;elapsed=0;consumed=0;}
   elapsed=std::min(300,elapsed+std::min(ms,1800));auto events=consumeGuardAction(action,elapsed,consumed);phase(action,elapsed,consumed);put("weapon_bash",elapsed);
   auto weapon=npcWeaponPose(i);auto origin=weapon.grip+Point3{shift.x,shift.y,0};
   if(events&GuardActionStart)m_sounds.push_back({Sound::PunchSwing,{origin.x,origin.y},.45f,.9f,true,17,origin.z});
   if(!(events&GuardActionWork))continue;
   put("weapon_bash",0);put("weapon_bash_pose",220);put("weapon_recovery",900);put("weapon_flash",0);
   damageGuardTarget(i,12.f);m_sounds.push_back({Sound::PunchHit,{origin.x,origin.y},.7f,.85f,true,17,origin.z});auto global=chunkOffset(m_level);m_guardShots.push_back({{origin.x+global.x,origin.y+global.y},origin.z,4.f,.7f,true,int(i)});continue;
  }
  put("weapon_bash",0);if(action==GuardAction::Bash&&elapsed<300){cancelAction();action=GuardAction::None;if(get("weapon_reloading"))phase(GuardAction::Reload,1800-reload,unsigned(get("weapon_reload_events")));}
  bool pendingFlank=guardFlankSide(i)!=0&&!get("guard_flank_done");
  if((!visible&&!suppress)||reload||recovery||npc.pose.clip==1||get("weapon_raise")<1000||!guardFireSlot(i)||pendingFlank){put("weapon_aim",0);continue;}
  auto weapon=suppress?npcWeaponPoseAt(i,npc.pose,coverPoint,1):npcWeaponPose(i);auto origin=weapon.muzzle+Point3{shift.x,shift.y,0};auto hit=guardShot(i,npc.pose,weapon);
  // Seeing the head is insufficient: the barrel and its full shot line must
  // clear cover and intervening bodies. Cancel an interrupted aim completely.
  bool ready=(visible||suppress)&&!reload&&!recovery&&npc.pose.clip!=1&&weapon.raise>=.999f&&(suppress?hit.kind==ShotKind::Solid&&hit.distance>.15f:guardShotMatches(i,hit));
  int aim=ready?std::min(config.aimMs,get("weapon_aim")+ms):0;put("weapon_aim",aim);
  if(aim<config.aimMs)continue;
  unsigned fireEvents=0;auto events=consumeGuardAction(GuardAction::Fire,0,fireEvents);phase(GuardAction::Fire,0,fireEvents);
  if(!(events&GuardActionStart))continue;
  put("weapon_aim",0);put("weapon_recovery",config.recoveryMs);put("weapon_flash",80);put("weapon_recoil",220);put("weapon_rounds",get("weapon_rounds")-1);
  m_sounds.push_back({gun.cycle.fireSound,{origin.x,origin.y},.7f,1,true,17,origin.z});auto global=chunkOffset(m_level);m_guardShots.push_back({{origin.x+global.x,origin.y+global.y},origin.z,14.f*std::sqrt(.7f),.7f,true,int(i)});
  resolveShot(hit,origin,weapon.direction,gun.hitscan,false,guardHandgun(config)?std::min(config.attackDamage,10.f):config.attackDamage,{ShotKind::Npc,npc.level,int(i)});
 }
}
void Game::validateGuardActions(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto action=GuardAction(get("weapon_action"));int elapsed=get("weapon_action_ms"),events=get("weapon_action_events"),reloadEvents=get("weapon_reload_events");
 if(events<0||events>15||!validGuardAction(action,elapsed,unsigned(events))||reloadEvents<0||reloadEvents>15||get("guard_suppress_ms")<0||get("guard_suppress_ms")>1200)throw std::runtime_error("Invalid saved guard action phase");
 // Zero event masks are accepted for pre-timeline saves and adopted without
 // replay. A new timeline must agree with the existing mechanical reload.
 if(reloadEvents&&(!get("weapon_reloading")||reloadEvents!=int(guardActionMask(GuardAction::Reload,1800-get("weapon_reload")))))throw std::runtime_error("Inconsistent saved guard reload events");
 if(action==GuardAction::Reload&&(!get("weapon_reloading")||elapsed!=1800-get("weapon_reload")||events!=reloadEvents))throw std::runtime_error("Inconsistent saved guard reload animation");
}
}

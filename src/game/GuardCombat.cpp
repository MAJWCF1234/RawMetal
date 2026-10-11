#include "Game.h"
namespace retro {
namespace {
Point3 unit(Point3 p){float length=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);return length>.0001f?p*(1/length):Point3{};}
}
NpcWeaponPose Game::npcWeaponPoseAt(size_t actor,const ActorPose& pose,Point3 target,float raise)const{
 const auto&track=m_friendlyActors.at(actor).track();NpcWeaponPose weapon;
 if(track.ai.mode!=ActorAiMode::Guard||!track.ai.armed)return weapon;
 weapon.armed=true;weapon.raise=std::clamp(raise,0.f,1.f);
 float scale=track.scale/1.6f;Point3 forward{std::sin(pose.yaw),std::cos(pose.yaw),0},right{forward.y,-forward.x,0};
 weapon.grip=Point3{pose.position.x,pose.position.y,pose.z}+right*(-.12f*scale)+forward*(.40f*scale)+Point3{0,0,(1.024f+.216f*weapon.raise)*scale};
 weapon.direction=unit(forward+Point3{0,0,-.32f});
 if(weapon.raise>0){auto aimed=unit(target-weapon.grip);
  for(int i=0;i<3;++i){float horizontal=std::hypot(aimed.x,aimed.y);auto r=horizontal>.001f?Point3{aimed.y/horizontal,-aimed.x/horizontal,0}:right;Point3 up{r.y*aimed.z,-r.x*aimed.z,r.x*aimed.y-r.y*aimed.x};aimed=unit(target-weapon.grip-up*(.058961f*scale)-r*(-.007496f*scale));}
  weapon.direction=unit(weapon.direction*(1-weapon.raise)+aimed*weapon.raise);
 }
 float horizontal=std::hypot(weapon.direction.x,weapon.direction.y);auto r=horizontal>.001f?Point3{weapon.direction.y/horizontal,-weapon.direction.x/horizontal,0}:right;Point3 up{r.y*weapon.direction.z,-r.x*weapon.direction.z,r.x*weapon.direction.y-r.y*weapon.direction.x};
 weapon.muzzle=weapon.grip+weapon.direction*(.500522f*scale)+up*(.058961f*scale)+r*(-.007496f*scale);return weapon;
}
NpcWeaponPose Game::npcWeaponPose(size_t actor)const{
 const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();NpcWeaponPose weapon;
 if(track.ai.mode!=ActorAiMode::Guard||!track.ai.armed||!autonomousActor(track)||npc.pose.clip==4||(track.deadState&&state(track.deadState)))return weapon;
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};float raise=get("weapon_raise")*.001f;
 if(get("weapon_target")){auto shift=chunkOffset(npc.level);Point3 target{get("weapon_x")*.001f-shift.x,get("weapon_y")*.001f-shift.y,get("weapon_z")*.001f};weapon=npcWeaponPoseAt(actor,npc.pose,target,raise);}
 else{
  // A raised saved pose without an aim target retains its low-ready direction.
  weapon=npcWeaponPoseAt(actor,npc.pose,{},0);float lift=.216f*raise*track.scale/1.6f;weapon.grip.z+=lift;weapon.muzzle.z+=lift;weapon.raise=raise;
 }
 weapon.flash=get("weapon_flash")/80.f;weapon.recoil=get("weapon_recoil")/220.f;
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
 if(dt<=0)return;updateGuardSquadSlots(dt);const auto&gun=weaponDef(WeaponId::Shotgun);
 for(size_t i=0;i<m_friendlyActors.size();++i){auto&npc=m_friendlyActors[i];const auto&track=npc.track();const auto&config=track.ai;if(config.mode!=ActorAiMode::Guard||!config.armed||!chunkResident(npc.level))continue;
  auto get=[&](const char*field){return state(friendlyKey(i,field));};auto put=[&](const char*field,int value){setState(friendlyKey(i,field),value);};
  if(!autonomousActor(track)||npc.pose.clip==4||(track.deadState&&state(track.deadState))){put("weapon_aim",0);put("weapon_raise",0);put("weapon_flash",0);put("weapon_recoil",0);put("weapon_bash",0);put("weapon_bash_pose",0);continue;}
  int ms=npcMilliseconds(i,dt,"weapon_fraction");
  if(!get("weapon_saved")){put("weapon_rounds",gun.cycle.tubeCapacity);put("weapon_saved",1);put("weapon_reload",0);put("weapon_reloading",0);}
  // A loaded tube cannot also be in its empty-tube reload interval. Normalize
  // stale in-memory flags as well as rejecting inconsistent saves on decode.
  if(get("weapon_rounds")>0&&(get("weapon_reload")||get("weapon_reloading"))){put("weapon_reload",0);put("weapon_reloading",0);}
  int recovery=std::max(0,get("weapon_recovery")-ms),reload=std::max(0,get("weapon_reload")-ms);put("weapon_recovery",recovery);put("weapon_reload",reload);put("weapon_flash",std::max(0,get("weapon_flash")-ms));put("weapon_recoil",std::max(0,get("weapon_recoil")-ms));put("weapon_bash_pose",std::max(0,get("weapon_bash_pose")-ms));
  if(get("weapon_rounds")==0){if(get("weapon_reloading")&&reload==0){put("weapon_rounds",gun.cycle.tubeCapacity);put("weapon_reloading",0);}else if(!get("weapon_reloading")){put("weapon_reloading",1);put("weapon_reload",1800);reload=1800;auto shift=chunkOffset(npc.level)-chunkOffset(m_level);m_sounds.push_back({Sound::Pump,npc.pose.position+shift,.4f,.75f,true,17,npc.pose.z+track.scale*.7f});}}
  auto shift=chunkOffset(npc.level)-chunkOffset(m_level);auto target=guardTarget(i);int kind=target.valid()?int(target.kind):0,targetLevel=target.valid()?target.level+1:0,targetIndex=target.valid()?target.index+1:0;
  if(kind!=get("weapon_target_kind")||targetLevel!=get("weapon_target_level")||targetIndex!=get("weapon_target_index")){put("weapon_aim",0);put("weapon_bash",0);put("weapon_target_kind",kind);put("weapon_target_level",targetLevel);put("weapon_target_index",targetIndex);}
  Point3 aimPoint{target.position.x,target.position.y,target.feet+target.height*.6f};auto delta=aimPoint-Point3{npc.pose.position.x,npc.pose.position.y,npc.pose.z+track.scale*.85f};
  bool visible=target.valid()&&guardState(i)==GuardState::Alert&&delta.x*delta.x+delta.y*delta.y+delta.z*delta.z<config.attackRange*config.attackRange&&guardCanSee(i,npc.pose,target);
  if(visible){auto global=target.position+chunkOffset(npc.level);put("weapon_target",1);put("weapon_x",int(std::round(global.x*1000)));put("weapon_y",int(std::round(global.y*1000)));put("weapon_z",int(std::round(aimPoint.z*1000)));}
  else if(!target.valid())put("weapon_target",0);
  bool close=visible&&guardMeleeReady(i,npc.pose,target);
  int raised=visible&&(!reload||close)&&npc.pose.clip!=1?1000:0;int current=get("weapon_raise");put("weapon_raise",current+std::clamp(raised-current,-ms*3,ms*3));
  // At contact range a finite, interruptible weapon bash takes precedence.
  // It is usable with an empty tube, and never spends ammunition or shares a
  // frame with a firearm attack. The live target/cover test runs every tick.
  if(close&&!recovery&&npc.pose.clip!=1){
   put("weapon_aim",0);if(get("weapon_raise")<1000){put("weapon_bash",0);continue;}
   int bash=get("weapon_bash");auto weapon=npcWeaponPose(i);auto origin=weapon.grip+Point3{shift.x,shift.y,0};
   if(!bash&&ms>0)m_sounds.push_back({Sound::PunchSwing,{origin.x,origin.y},.45f,.9f,true,17,origin.z});
   bash=std::min(300,bash+ms);put("weapon_bash",bash);if(bash<300)continue;
   put("weapon_bash",0);put("weapon_bash_pose",220);put("weapon_recovery",900);put("weapon_flash",0);
   damageGuardTarget(i,12.f);m_sounds.push_back({Sound::PunchHit,{origin.x,origin.y},.7f,.85f,true,17,origin.z});auto global=chunkOffset(m_level);m_guardShots.push_back({{origin.x+global.x,origin.y+global.y},origin.z,4.f,.7f,true,int(i)});continue;
  }
  put("weapon_bash",0);
  bool pendingFlank=guardFlankSide(i)!=0&&!get("guard_flank_done");
  if(!visible||reload||recovery||npc.pose.clip==1||get("weapon_raise")<1000||!guardFireSlot(i)||pendingFlank){put("weapon_aim",0);continue;}
  auto weapon=npcWeaponPose(i);auto origin=weapon.muzzle+Point3{shift.x,shift.y,0};auto hit=guardShot(i,npc.pose,weapon);
  // Seeing the head is insufficient: the barrel and its full shot line must
  // clear cover and intervening bodies. Cancel an interrupted aim completely.
  bool ready=visible&&!reload&&!recovery&&npc.pose.clip!=1&&weapon.raise>=.999f&&guardShotMatches(i,hit);
  int aim=ready?std::min(config.aimMs,get("weapon_aim")+ms):0;put("weapon_aim",aim);
  if(aim<config.aimMs)continue;
  put("weapon_aim",0);put("weapon_recovery",config.recoveryMs);put("weapon_flash",80);put("weapon_recoil",220);put("weapon_rounds",get("weapon_rounds")-1);
  m_sounds.push_back({gun.cycle.fireSound,{origin.x,origin.y},.7f,1,true,17,origin.z});auto global=chunkOffset(m_level);m_guardShots.push_back({{origin.x+global.x,origin.y+global.y},origin.z,14.f*std::sqrt(.7f),.7f,true,int(i)});
  resolveShot(hit,origin,weapon.direction,gun.hitscan,false,config.attackDamage,{ShotKind::Npc,npc.level,int(i)});
 }
}
}

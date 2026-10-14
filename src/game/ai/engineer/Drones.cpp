#include "../../Game.h"
#include "Drones.h"
#include "../Navigation.h"
namespace retro {
namespace {
StateId droneKey(int slot,std::string_view field){auto text="drone/slot/"+std::to_string(slot)+"/"+std::string(field);StateId key=2166136261u;for(unsigned char c:text)key=(key^c)*16777619u;return key|0x80000000u;}
float norm3(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
bool sameTarget(const ShotHit&hit,const AiTarget&target){return target.valid()&&hit.kind==target.kind&&hit.index==target.index&&(hit.kind==ShotKind::Npc||hit.level==target.level);}
}
std::vector<EngineerDronePose> Game::engineerDrones()const{
 std::vector<EngineerDronePose> drones;
 for(int slot=0;slot<EngineerDroneSlots;++slot)if(state(droneKey(slot,"active"))){auto get=[&](const char*f){return state(droneKey(slot,f));};int level=get("level")-1;if(level<0||level>=chunkCount())continue;auto origin=chunkOffset(level);
  drones.push_back({slot,get("owner")-1,level,{get("x")*.001f-origin.x+get("x_fraction")*.000001f,get("y")*.001f-origin.y+get("y_fraction")*.000001f,get("z")*.001f},get("yaw")*.0001f,float(get("hp")),get("flash")/60.f,get("repair")/200.f,get("phase")*.0001f,get("walking")!=0});
 }return drones;
}
AiTarget Game::engineerDroneTarget(int slot,int observerLevel)const{
 if(slot<0||slot>=EngineerDroneSlots||observerLevel<0||observerLevel>=chunkCount()||!state(droneKey(slot,"active"))||state(droneKey(slot,"hp"))<=0)return {};
 int owner=state(droneKey(slot,"owner"))-1,level=state(droneKey(slot,"level"))-1;if(owner<0||size_t(owner)>=m_friendlyActors.size()||level<0||level>=chunkCount()||!chunkResident(level))return {};
 const auto&npc=m_friendlyActors[size_t(owner)];const auto&t=npc.track();if(t.ai.role!=GuardSquadRole::Engineer||!autonomousActor(t)||npc.pose.clip==4||(t.deadState&&state(t.deadState)))return {};
 auto origin=chunkOffset(observerLevel);AiTarget target;target.kind=ShotKind::Drone;target.level=level;target.index=slot;target.position={state(droneKey(slot,"x"))*.001f-origin.x+state(droneKey(slot,"x_fraction"))*.000001f,state(droneKey(slot,"y"))*.001f-origin.y+state(droneKey(slot,"y_fraction"))*.000001f};target.feet=state(droneKey(slot,"z"))*.001f;target.height=EngineerDroneHeight;target.eye=EngineerDroneHeight*.85f;target.faction=actorFaction(t);return target;
}
bool Game::engineerDroneChunkNeeded(int level)const{
 for(int slot=0;slot<EngineerDroneSlots;++slot)if(state(droneKey(slot,"active"))&&(state(droneKey(slot,"level"))==level+1||state(droneKey(slot,"route_next"))==level+1))return true;return false;
}
ShotHit Game::traceEngineerDrone(Point3 from,Point3 to,size_t)const{return tacticalGrenadeTrace(from,to);}
bool Game::deployEngineerDrone(size_t owner){
 if(owner>=m_friendlyActors.size())return false;const auto&npc=m_friendlyActors[owner];const auto&t=npc.track();
 if(t.ai.mode!=ActorAiMode::Guard||t.ai.role!=GuardSquadRole::Engineer||!autonomousActor(t)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||!chunkResident(npc.level)||state(friendlyKey(owner,"drone_used"))>=EngineerDroneCapacity)return false;
 int slot=-1,owned=0;for(int i=0;i<EngineerDroneSlots;++i){if(state(droneKey(i,"active"))&&state(droneKey(i,"owner"))==int(owner)+1)++owned;if(slot<0&&!state(droneKey(i,"active")))slot=i;}if(slot<0||owned>=EngineerDroneCapacity)return false;
 auto origin=chunkOffset(npc.level),active=chunkOffset(m_level),shift=origin-active;Vec2 chosen;float z=npc.pose.z;bool found=false;
 for(float radius:{1.45f,2.3f})for(float turn:{kPi*.5f,-kPi*.5f,0.f,kPi,kPi*.25f,-kPi*.25f,kPi*.75f,-kPi*.75f}){auto facing=Vec2{std::sin(npc.pose.yaw+turn),std::cos(npc.pose.yaw+turn)};auto candidate=npc.pose.position+facing*radius;
  if(found)break;
  if(candidate.x<EngineerDroneRadius||candidate.x>24-EngineerDroneRadius||candidate.y<EngineerDroneRadius||candidate.y>24-EngineerDroneRadius)continue;
  float feet=friendlyWorld(npc.level).supportBelow(candidate.x,candidate.y,npc.pose.z+.265f);if(std::fabs(feet-npc.pose.z)>.24f)continue;bool clear=true;
  for(float x:{-.4f,0.f,.4f})for(float y:{-.4f,0.f,.4f})clear&=friendlyWalkSegment(npc.level,npc.pose.position+Vec2{x,y},npc.pose.z,candidate+Vec2{x,y},EngineerDroneHeight);
  Point3 from{npc.pose.position.x+shift.x,npc.pose.position.y+shift.y,npc.pose.z+t.scale*.65f},to{candidate.x+shift.x,candidate.y+shift.y,feet+EngineerDroneHeight*.65f};auto delta=to-from;float range=norm3(delta);
  if(clear&&traceShot(from,delta,range,int(owner),true).kind!=ShotKind::None)clear=false;
  for(const auto&drone:engineerDrones()){auto p=drone.position+Point3{chunkOffset(drone.level).x-origin.x,chunkOffset(drone.level).y-origin.y,0};if(std::hypot(candidate.x-p.x,candidate.y-p.y)<EngineerDroneRadius*2&&feet<p.z+EngineerDroneHeight&&feet+EngineerDroneHeight>p.z)clear=false;}
  if(clear){chosen=candidate;z=feet;found=true;break;}
 }if(!found)return false;
 auto put=[&](const char*f,int v){setState(droneKey(slot,f),v);};put("active",1);put("owner",int(owner)+1);put("level",npc.level+1);put("hp",EngineerDroneHealth);put("ammo",24);put("x",int(std::round((chosen.x+origin.x)*1000)));put("y",int(std::round((chosen.y+origin.y)*1000)));put("z",int(std::round(z*1000)));put("yaw",int(std::round(std::remainder(npc.pose.yaw,2*kPi)*10000)));
 for(const char*f:{"aim","cooldown","flash","repair","phase","walking","fraction","x_fraction","y_fraction","route_next","route_wait","way_x","way_y","repath","target_kind","target_level","target_index"})put(f,0);
 setState(friendlyKey(owner,"drone_used"),state(friendlyKey(owner,"drone_used"))+1);setState(friendlyKey(owner,"drone_cooldown"),2200);setState(friendlyKey(owner,"drone_windup"),0);
 if(!state(friendlyKey(owner,"drone_repair_saved"))){setState(friendlyKey(owner,"drone_repair_saved"),1);setState(friendlyKey(owner,"drone_repair_charge"),80);}
 m_sounds.push_back({Sound::JunkMetal,chosen+shift,.6f,.7f,true,17,z+.5f});return true;
}
void Game::damageEngineerDrone(int slot,float amount,AiTargetRef){
 if(slot<0||slot>=EngineerDroneSlots||!state(droneKey(slot,"active"))||!std::isfinite(amount)||amount<=0)return;
 int damage=std::min(EngineerDroneHealth,int(std::ceil(std::min(amount,float(EngineerDroneHealth))))),health=std::max(0,state(droneKey(slot,"hp"))-damage);setState(droneKey(slot,"hp"),health);setState(droneKey(slot,"aim"),0);
 auto active=chunkOffset(m_level);Vec2 point{state(droneKey(slot,"x"))*.001f-active.x,state(droneKey(slot,"y"))*.001f-active.y};float z=state(droneKey(slot,"z"))*.001f;
 m_sounds.push_back({health?Sound::JunkMetal:Sound::LiftCrash,point,health?.4f:.8f,health?1.1f:1.3f,true,17,z+.5f});if(!health){setState(droneKey(slot,"active"),0);int owner=state(droneKey(slot,"owner"))-1;
  if(owner>=0&&size_t(owner)<m_friendlyActors.size()&&state(friendlyKey(size_t(owner),"drone_repair_target"))==slot+1){for(const char*f:{"drone_repair_target","drone_repair_ms","drone_busy"})setState(friendlyKey(size_t(owner),f),0);}
 }
}
void Game::updateEngineerDrones(float dt){
 if(!std::isfinite(dt)||dt<=0)return;dt=std::min(dt,.05f);
 // Deployment belongs to an explicitly authored Engineer, is finite and must
 // begin with personal threat recognition. An existing fleet may continue
 // using confirmed memory, without acquiring hidden enemy coordinates.
 for(size_t owner=0;owner<m_friendlyActors.size();++owner){const auto&npc=m_friendlyActors[owner];const auto&t=npc.track();if(t.ai.mode!=ActorAiMode::Guard||t.ai.role!=GuardSquadRole::Engineer)continue;
  auto get=[&](const char*f){return state(friendlyKey(owner,f));};auto put=[&](const char*f,int v){setState(friendlyKey(owner,f),v);};put("drone_busy",0);int ms=npcMilliseconds(owner,dt,"drone_fraction");put("drone_cooldown",std::max(0,get("drone_cooldown")-ms));
  int deployed=0;for(int slot=0;slot<EngineerDroneSlots;++slot)deployed+=state(droneKey(slot,"active"))&&state(droneKey(slot,"owner"))==int(owner)+1;
  auto target=guardTarget(owner);auto origin=chunkOffset(npc.level),active=chunkOffset(m_level);
  Vec2 remembered{get("guard_x")*.001f-origin.x,get("guard_y")*.001f-origin.y};float standOff=std::min(8.f,std::max(6.f,t.ai.attackRange*.6f));
  bool rearReady=!get("guard_engineer_active")||length(npc.pose.position-remembered)>=standOff-.5f||guardCovered(owner,npc.pose,target);
  bool workAllowed=rearReady&&!get("guard_danger_ms")&&!get("weapon_reload")&&!get("weapon_reloading")&&!get("weapon_recovery")&&!get("weapon_bash")&&!get("weapon_bash_pose")&&!get("grenade_windup")&&get("weapon_action")==int(GuardAction::None);
  // One selected physical repair task owns the owner's clock. Five nearby
  // drones cannot each advance that clock or multiply repair throughput.
  int repair=-1;float priority=-1;
  if(guardTaskAllows(owner,GuardTask::Repair)&&autonomousActor(t)&&npc.pose.clip==0&&!get("walking")&&!get("drone_windup")&&workAllowed&&get("drone_repair_charge")>0)for(const auto&drone:engineerDrones()){
   if(get("schedule")!=int(GuardSchedule::None)&&drone.slot+1!=get("task_subject"))continue;
   if(drone.owner!=int(owner)||drone.walking||drone.health>=EngineerDroneHealth)continue;auto shift=chunkOffset(drone.level)-origin;Vec2 point{drone.position.x+shift.x,drone.position.y+shift.y};
   if(length(point-npc.pose.position)>=2||std::fabs(drone.position.z-npc.pose.z)>=.3f)continue;
   Point3 hand{npc.pose.position.x+origin.x-active.x,npc.pose.position.y+origin.y-active.y,npc.pose.z+t.scale*.65f},eye{point.x+origin.x-active.x,point.y+origin.y-active.y,drone.position.z+EngineerDroneHeight*.85f};
   if(traceEngineerDrone(hand,eye,owner).kind==ShotKind::Solid)continue;float score=EngineerDroneHealth-drone.health+(get("drone_repair_target")==drone.slot+1?1.f:0.f);if(score>priority){priority=score;repair=drone.slot;}
  }
  if(get("drone_repair_target")&&get("drone_repair_target")!=repair+1)put("drone_repair_ms",0);put("drone_repair_target",repair+1);if(repair<0)put("drone_repair_ms",0);
  bool justified=get("guard_confirmed")&&get("guard_memory")&&target.valid()&&((get("guard_seen")&&guardCanSee(owner,npc.pose,target))||deployed>0);
  bool ready=guardTaskAllows(owner,GuardTask::Deploy)&&repair<0&&deployed<EngineerDroneCapacity&&get("drone_used")<EngineerDroneCapacity&&!get("drone_cooldown")&&autonomousActor(t)&&npc.pose.clip!=4&&npc.pose.clip!=1&&(!t.deadState||!state(t.deadState))&&chunkResident(npc.level)&&justified&&workAllowed;
  // The directly rigged Engineer owns a short pad equip/use/stow timeline.
  // Resource clocks begin only after equip, and normal completion stows the
  // device before firearms can resume. Physical escape always wins immediately.
  bool direct=t.visual==ActorVisual::CleanupEngineer&&cleanupAnimationDuration(t.visual,8)>0,animationReady=true;
  if(direct){int stage=get("engineer_pad_stage"),clock=get("engineer_pad_ms");bool requested=ready||repair>=0;
   bool interrupted=!autonomousActor(t)||!chunkResident(npc.level)||npc.pose.clip!=0||get("walking")||(get("locomotion")&&!get("grounded"))||get("guard_danger_ms")||!workAllowed;
   if(interrupted){stage=0;clock=0;}
   else if(stage==0){if(requested){stage=1;clock=0;}}
   else if(stage==1){if(!requested){stage=3;clock=0;}else{clock+=ms;int duration=int(std::round(cleanupAnimationDuration(t.visual,6)*1000));if(clock>=duration){stage=2;clock=0;}}}
   else if(stage==2){if(!requested){stage=3;clock=0;}else{int duration=std::max(1,int(std::round(cleanupAnimationDuration(t.visual,7)*1000)));clock=(clock+ms)%duration;}}
   else{clock+=ms;int duration=int(std::round(cleanupAnimationDuration(t.visual,8)*1000));if(clock>=duration){stage=0;clock=0;}}
   put("engineer_pad_stage",stage);put("engineer_pad_ms",clock);animationReady=stage==2;
   if(stage==1||stage==2)put("drone_busy",1);
  }else{put("engineer_pad_stage",0);put("engineer_pad_ms",0);}
  put("drone_windup",ready&&animationReady?std::min(1200,get("drone_windup")+ms):0);if(ready&&get("drone_windup")>0)put("drone_busy",1);
  if(ready&&animationReady&&get("drone_windup")>=1200&&m_npcNavBudget>0){--m_npcNavBudget;if(!deployEngineerDrone(owner)){put("drone_windup",0);put("drone_cooldown",1500);put("drone_busy",0);}}
 }
 for(int slot=0;slot<EngineerDroneSlots;++slot){auto get=[&](const char*f){return state(droneKey(slot,f));};auto put=[&](const char*f,int v){setState(droneKey(slot,f),v);};if(!get("active"))continue;int owner=get("owner")-1,level=get("level")-1;
  if(owner<0||size_t(owner)>=m_friendlyActors.size()||level<0||level>=chunkCount()){put("active",0);continue;}const auto&npc=m_friendlyActors[size_t(owner)];const auto&t=npc.track();
  if(t.ai.role!=GuardSquadRole::Engineer||!autonomousActor(t)||npc.pose.clip==4||(t.deadState&&state(t.deadState))){put("active",0);for(const char*f:{"drone_repair_target","drone_repair_ms","drone_busy","drone_windup","guard_engineer_active","guard_engineer_goal","guard_engineer_wait"})setState(friendlyKey(size_t(owner),f),0);continue;}
  int micros=get("fraction")+int(std::round(dt*1000000)),ms=micros/1000;put("fraction",micros%1000);put("cooldown",std::max(0,get("cooldown")-ms));put("flash",std::max(0,get("flash")-ms));put("repair",std::max(0,get("repair")-ms));
  if(!chunkResident(level)){if(m_npcNavBudget<=0)continue;--m_npcNavBudget;ensureChunk(level);}auto origin=chunkOffset(level),active=chunkOffset(m_level),shift=origin-active;
  Point3 p{get("x")*.001f-origin.x+get("x_fraction")*.000001f,get("y")*.001f-origin.y+get("y_fraction")*.000001f,get("z")*.001f};Vec2 here{p.x,p.y},ownerLocal=npc.pose.position+chunkOffset(npc.level)-origin;
  auto target=guardTarget(size_t(owner));bool confirmed=state(friendlyKey(size_t(owner),"guard_confirmed"))&&state(friendlyKey(size_t(owner),"guard_memory"));auto live=confirmed?resolveGuardTarget(size_t(owner),{target.kind,target.level,target.index}):AiTarget{};
  Point3 eye{p.x+shift.x,p.y+shift.y,p.z+EngineerDroneHeight*.85f},aim{};ShotHit hit;bool seen=false;
  if(live.valid()){auto ownerShift=chunkOffset(npc.level)-active;aim={live.position.x+ownerShift.x,live.position.y+ownerShift.y,live.feet+live.height*.6f};auto direction=aim-eye;float range=norm3(direction);
   if(range>.01f&&range<12){hit=traceShot(eye,direction,range+.05f,-1,true,nullptr,{ShotKind::Drone,level,slot});seen=sameTarget(hit,live);}
  }
  bool same=seen&&get("target_kind")==int(live.kind)&&get("target_level")==live.level+1&&get("target_index")==live.index+1;
  if(seen){put("target_kind",int(live.kind));put("target_level",live.level+1);put("target_index",live.index+1);auto toward=Vec2{aim.x-eye.x,aim.y-eye.y};float yaw=get("yaw")*.0001f;yaw+=std::clamp(std::remainder(std::atan2(toward.x,toward.y)-yaw,2*kPi),-4.f*dt,4.f*dt);put("yaw",int(std::round(std::remainder(yaw,2*kPi)*10000)));put("aim",same?std::min(500,get("aim")+ms):0);
   // The cannon starts at the measured body position; both body-to-barrel and
   // first-hit target checks are repeated at the attack event.
   if(get("aim")>=500&&!get("cooldown")&&get("ammo")>0){Point3 muzzle=eye+Point3{std::sin(yaw)*.48f,std::cos(yaw)*.48f,-EngineerDroneHeight*.25f};auto barrel=muzzle-eye;auto clear=traceShot(eye,barrel,norm3(barrel),-1,true,nullptr,{ShotKind::Drone,level,slot});auto ray=aim-muzzle;float range=norm3(ray);auto shot=traceShot(muzzle,ray,range+.05f,-1,true,nullptr,{ShotKind::Drone,level,slot});
    if(clear.kind==ShotKind::None&&sameTarget(shot,live)&&dot(normalized(toward),Vec2{std::sin(yaw),std::cos(yaw)})>.97f){resolveShot(shot,muzzle,ray*(1/range),weaponDef(WeaponId::Shotgun).hitscan,false,6,{ShotKind::Npc,npc.level,owner});put("ammo",get("ammo")-1);put("cooldown",700);put("flash",60);put("aim",0);m_sounds.push_back({Sound::Shot,{muzzle.x,muzzle.y},.5f,1.35f,true,17,muzzle.z});}
   }
  }else{put("aim",0);put("target_kind",0);put("target_level",0);put("target_index",0);}
  auto ownerGet=[&](const char*f){return state(friendlyKey(size_t(owner),f));};bool stationary=npc.pose.clip!=1&&!ownerGet("walking")&&!get("walking");
  bool workAllowed=!ownerGet("guard_danger_ms")&&!ownerGet("weapon_reload")&&!ownerGet("weapon_reloading")&&!ownerGet("weapon_recovery")&&!ownerGet("weapon_bash")&&!ownerGet("weapon_bash_pose")&&!ownerGet("grenade_windup")&&ownerGet("weapon_action")==int(GuardAction::None);
  bool repair=guardTaskAllows(size_t(owner),GuardTask::Repair)&&ownerGet("drone_repair_target")==slot+1&&
   (ownerGet("schedule")==int(GuardSchedule::None)||ownerGet("task_subject")==slot+1)&&stationary&&workAllowed&&!ownerGet("drone_windup")&&length(ownerLocal-here)<2&&std::fabs(npc.pose.z-p.z)<.3f&&get("hp")<EngineerDroneHealth&&ownerGet("drone_repair_charge")>0;
  if(t.visual==ActorVisual::CleanupEngineer&&cleanupAnimationDuration(t.visual,8)>0)repair&=ownerGet("engineer_pad_stage")==2;
  Point3 hand{ownerLocal.x+shift.x,ownerLocal.y+shift.y,npc.pose.z+t.scale*.65f};if(repair&&traceEngineerDrone(hand,eye,size_t(owner)).kind==ShotKind::None){int clock=state(friendlyKey(size_t(owner),"drone_repair_ms"))+ms;
   setState(friendlyKey(size_t(owner),"drone_busy"),1);
   while(clock>=100&&get("hp")<EngineerDroneHealth&&state(friendlyKey(size_t(owner),"drone_repair_charge"))>0){clock-=100;put("hp",get("hp")+1);put("repair",200);setState(friendlyKey(size_t(owner),"drone_repair_charge"),state(friendlyKey(size_t(owner),"drone_repair_charge"))-1);}setState(friendlyKey(size_t(owner),"drone_repair_ms"),clock%100);
  }else if(ownerGet("drone_repair_target")==slot+1){setState(friendlyKey(size_t(owner),"drone_repair_ms"),0);setState(friendlyKey(size_t(owner),"drone_repair_target"),0);}
  // Supervision follows the living Engineer, never the concealed enemy. A
  // drone with a safe firing solution holds its position; elsewhere it uses
  // the existing bounded portal/local planner and the actual support hull.
  int ordinal=0;for(int prior=0;prior<slot;++prior)ordinal+=state(droneKey(prior,"active"))&&state(droneKey(prior,"owner"))==owner+1;
  float angle=npc.pose.yaw+kPi*.5f+ordinal*(2*kPi/EngineerDroneCapacity);Vec2 formation{std::sin(angle)*1.6f,std::cos(angle)*1.6f};
  Vec2 goal=ownerLocal+(npc.level==level?formation:Vec2{});if(npc.level==level){goal.x=std::clamp(goal.x,EngineerDroneRadius,24-EngineerDroneRadius);goal.y=std::clamp(goal.y,EngineerDroneRadius,24-EngineerDroneRadius);}
  float goalFeet=npc.pose.z;bool moving=!seen&&(length(goal-here)>.25f||npc.level!=level);if(ownerGet("drone_repair_target")==slot+1)moving=false;int routeWait=std::max(0,get("route_wait")-ms),next=get("route_next")-1;
  if(moving&&npc.level!=level){Vec2 portal{get("portal_x")*.001f-origin.x,get("portal_y")*.001f-origin.y};bool cached=routeWait>0&&next>=0&&next<chunkCount()&&next!=level&&friendlyBoundaryOpen(level,next,portal+origin);
   if(!cached&&m_npcNavBudget>0&&(routeWait==0||next>=0)){--m_npcNavBudget;cached=friendlyPortal(level,npc.level,here,p.z,EngineerDroneHeight,portal,goalFeet,npc.pose.z);next=cached?friendlyChunkAt(portal+origin,level):-1;put("route_next",next+1);routeWait=600+(slot%7)*30;if(cached){put("portal_x",int(std::round((portal.x+origin.x)*1000)));put("portal_y",int(std::round((portal.y+origin.y)*1000)));put("portal_z",int(std::round(goalFeet*1000)));}}
   moving=cached;if(cached){goal=portal;goalFeet=get("portal_z")*.001f;}
  }else put("route_next",0);put("route_wait",routeWait);
  Vec2 before=here,waypoint{get("way_x")*.001f-origin.x,get("way_y")*.001f-origin.y};int repath=std::max(0,get("repath")-ms);
  if(moving){auto delta=goal-here;auto probe=here+normalized(delta)*std::min(1.f,length(delta));if(friendlyWalkSegment(level,here,p.z,probe,EngineerDroneHeight))waypoint=goal;
   else if(!repath&&m_npcNavBudget>0){--m_npcNavBudget;waypoint=aiWalkWaypoint(friendlyWorld(level),here,p.z,goal,goalFeet,EngineerDroneHeight);repath=350+slot*30;}
   auto deltaWay=waypoint-here;float distance=length(deltaWay);auto direction=normalized(deltaWay);auto end=here+direction*std::min(distance,1.6f*dt);float feet=-10000;bool clear=true;
   for(float x:{-.4f,0.f,.4f})for(float y:{-.4f,0.f,.4f}){float supported=p.z;clear&=friendlyWalkSegment(level,here+Vec2{x,y},p.z,end+Vec2{x,y},EngineerDroneHeight,&supported);feet=std::max(feet,supported);}
   auto activeEnd=end+origin-active;auto bodyOverlap=[&](Vec2 point,float bottom,float top,float radius){float combined=EngineerDroneRadius+radius;if(feet<top&&feet+EngineerDroneHeight>bottom&&lengthSq(activeEnd-point)<combined*combined)clear=false;};
   bodyOverlap(m_player.pos,m_player.z,m_player.z+m_player.hullHeight(),.2f);
   for(const auto&other:m_friendlyActors)if(autonomousActor(other.track())&&other.pose.clip!=4&&(!other.track().deadState||!state(other.track().deadState)))bodyOverlap(other.pose.position+chunkOffset(other.level)-active,other.pose.z,other.pose.z+other.track().scale,.25f);
   for(int chunk=0;chunk<chunkCount();++chunk)if(chunkResident(chunk)){auto off=chunkOffset(chunk)-active;const auto&enemies=chunk==m_level?m_enemies:m_chunks[chunk].enemies;for(const auto&enemy:enemies)if(enemy.alive)bodyOverlap(enemy.pos+off,enemy.bodyBottom(),enemy.bodyTop(),enemy.kind==Enemy::Kind::Wasp?.28f:.4f);
    const auto&world=friendlyWorld(chunk);for(size_t index=0;index<world.actorTracks().size();++index){const auto&other=world.actorTracks()[index];const auto&pose=world.actorPose(index);if(other.ai.mode==ActorAiMode::Scripted&&!other.platform&&pose.clip!=4&&(!other.deadState||!state(other.deadState)))bodyOverlap(pose.position+off,pose.z,pose.z+other.scale,other.visual==ActorVisual::Cargo?other.scale*.5f:.2f);}}
   for(const auto&other:engineerDrones())if(other.slot!=slot){auto off=chunkOffset(other.level)-active;bodyOverlap({other.position.x+off.x,other.position.y+off.y},other.position.z,other.position.z+EngineerDroneHeight,EngineerDroneRadius);}
   if(clear&&distance>.001f){here=end;p.z=feet;float yaw=get("yaw")*.0001f;yaw+=std::clamp(std::remainder(std::atan2(direction.x,direction.y)-yaw,2*kPi),-5.f*dt,5.f*dt);put("yaw",int(std::round(std::remainder(yaw,2*kPi)*10000)));}
  }
  float travelled=length(here-before);int destination=friendlyChunkAt(here+origin,level);if(destination<0){put("active",0);continue;}if(destination!=level){put("level",destination+1);put("route_next",0);put("route_wait",0);put("repath",0);}
  int x=int(std::round((here.x+origin.x)*1000)),y=int(std::round((here.y+origin.y)*1000));put("x",x);put("y",y);put("x_fraction",int(std::round((here.x+origin.x-x*.001f)*1000000)));put("y_fraction",int(std::round((here.y+origin.y-y*.001f)*1000000)));put("z",int(std::round(p.z*1000)));put("walking",travelled>.0001f);put("phase",int(std::round(std::fmod(get("phase")*.0001f+(travelled>.0001f?travelled*1.5f:dt*.3f),1.f)*10000)));put("way_x",int(std::round((waypoint.x+origin.x)*1000)));put("way_y",int(std::round((waypoint.y+origin.y)*1000)));if(destination==level)put("repath",repath);
 }
}
void Game::validateEngineerDrones()const{
 std::vector<int> owners(m_friendlyActors.size());
 for(int slot=0;slot<EngineerDroneSlots;++slot){auto get=[&](const char*f){return state(droneKey(slot,f));};if(get("active")<0||get("active")>1)throw std::runtime_error("Invalid saved drone activation");if(!get("active"))continue;int owner=get("owner")-1,level=get("level")-1;
  if(owner<0||size_t(owner)>=m_friendlyActors.size()||++owners[size_t(owner)]>EngineerDroneCapacity||level<0||level>=chunkCount()||m_friendlyActors[size_t(owner)].track().ai.role!=GuardSquadRole::Engineer)throw std::runtime_error("Invalid saved drone owner or physical chunk");
  if(get("x_fraction")< -999||get("x_fraction")>999||get("y_fraction")< -999||get("y_fraction")>999)throw std::runtime_error("Invalid saved drone movement fraction");
  for(const char*f:{"x","y","z","portal_x","portal_y","portal_z","way_x","way_y"})if(get(f)< -10000000||get(f)>10000000)throw std::runtime_error("Invalid saved drone position");
  if(friendlyChunkAt({get("x")*.001f+get("x_fraction")*.000001f,get("y")*.001f+get("y_fraction")*.000001f},level)!=level||get("z")< -100000||get("z")>100000||get("hp")<=0||get("hp")>EngineerDroneHealth||get("ammo")<0||get("ammo")>24||get("cooldown")<0||get("cooldown")>700||get("aim")<0||get("aim")>500||get("flash")<0||get("flash")>60||get("repair")<0||get("repair")>200||get("fraction")<0||get("fraction")>=1000||get("phase")<0||get("phase")>10000||get("yaw")< -31416||get("yaw")>31416||get("walking")<0||get("walking")>1||get("route_wait")<0||get("route_wait")>1000||get("route_next")<0||get("route_next")>chunkCount()||get("repath")<0||get("repath")>1000)throw std::runtime_error("Invalid saved drone hull, resource or clock state");
  int kind=get("target_kind");if(kind&&kind!=int(ShotKind::Player)&&kind!=int(ShotKind::Npc)&&kind!=int(ShotKind::Actor)&&kind!=int(ShotKind::Enemy)&&kind!=int(ShotKind::Drone))throw std::runtime_error("Invalid saved drone target kind");if(get("target_level")<0||get("target_level")>chunkCount()||get("target_index")<0||get("target_index")>65536||(kind&&(!get("target_level")||!get("target_index")))||(kind==int(ShotKind::Drone)&&get("target_index")>EngineerDroneSlots))throw std::runtime_error("Invalid saved drone target reference");
 }
 for(size_t owner=0;owner<m_friendlyActors.size();++owner){auto get=[&](const char*f){return state(friendlyKey(owner,f));};if(get("drone_used")<owners[owner]||get("drone_used")>EngineerDroneCapacity||get("drone_cooldown")<0||get("drone_cooldown")>8000||get("drone_windup")<0||get("drone_windup")>1200||get("drone_fraction")<0||get("drone_fraction")>=1000||get("drone_repair_saved")<0||get("drone_repair_saved")>1||get("drone_busy")<0||get("drone_busy")>1||get("drone_repair_charge")<0||get("drone_repair_charge")>80||get("drone_repair_ms")<0||get("drone_repair_ms")>=100||get("drone_repair_target")<0||get("drone_repair_target")>EngineerDroneSlots)throw std::runtime_error("Invalid saved Engineer deployment/repair resources");
  int stage=get("engineer_pad_stage"),clock=get("engineer_pad_ms");const auto&track=m_friendlyActors[owner].track();int clip=stage==1?6:stage==2?7:8;float duration=cleanupAnimationDuration(track.visual,clip);
  if(stage<0||stage>3||clock<0||(!stage&&clock)||(stage&&(track.ai.role!=GuardSquadRole::Engineer||track.visual!=ActorVisual::CleanupEngineer||duration<=0||clock>=int(std::round(duration*1000)))))throw std::runtime_error("Invalid saved Engineer pad animation");
  if(get("drone_repair_target")&&(!state(droneKey(get("drone_repair_target")-1,"active"))||state(droneKey(get("drone_repair_target")-1,"owner"))!=int(owner)+1))throw std::runtime_error("Saved Engineer repair target is not owned and active");
  if(get("guard_engineer_active")<0||get("guard_engineer_active")>1||get("guard_engineer_goal")<0||get("guard_engineer_goal")>1||get("guard_engineer_wait")<0||get("guard_engineer_wait")>700||get("guard_engineer_chunk")<0||get("guard_engineer_chunk")>chunkCount())throw std::runtime_error("Invalid saved Engineer support task");
  for(const char*f:{"guard_engineer_x","guard_engineer_y","guard_engineer_z"})if(get(f)<-100000||get(f)>100000)throw std::runtime_error("Invalid saved Engineer support destination");
  if(get("guard_engineer_goal")&&(get("guard_engineer_x")<350||get("guard_engineer_x")>23650||get("guard_engineer_y")<350||get("guard_engineer_y")>23650||!get("guard_engineer_chunk")))throw std::runtime_error("Saved Engineer support destination is outside its chunk");
 }
}
}

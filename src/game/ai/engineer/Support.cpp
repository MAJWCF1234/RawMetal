#include "../../Game.h"
#include "Drones.h"
namespace retro {
void Game::guardEngineerIntent(int level,size_t actor,const ActorTrack&track,ActorPose&pose,int ms,Vec2&goal,float&goalZ,bool&wantsMove){
 auto ctx=makeGuardCtx(actor,level,track,pose);auto get=[&](const char*f){return ctx.get(f);};auto put=[&](const char*f,int v){ctx.put(f,v);};
 auto target=guardTarget(actor);Vec2 remembered{get("guard_x")*.001f-chunkOffset(level).x,get("guard_y")*.001f-chunkOffset(level).y};float rememberedFeet=get("guard_z")*.001f;
 const float distance=length(pose.position-remembered),standOff=std::min(8.f,std::max(6.f,track.ai.attackRange*.6f));
 bool support=(get("drone_used")>0||distance<3.f)&&get("guard_confirmed")&&get("guard_memory")&&target.valid();put("guard_engineer_active",support);
 if(!support){put("guard_engineer_goal",0);put("guard_engineer_wait",0);if(get("drone_windup")){goal=pose.position;goalZ=pose.z;wantsMove=false;}return;}
 // Shared defensive intent owns an in-progress retreat and protected reload.
 // A role hook must not erase that task to start ordinary supervision.
 if(get("guard_cover_mode")==2&&(get("guard_cover")||guardCovered(actor,pose,target))){put("guard_engineer_goal",0);put("guard_engineer_wait",0);return;}
 // Supervision uses last-observed threat coordinates. Drones own the forward
 // firing positions; the Engineer never chases an unseen enemy to support them.
 put("guard_route_next",0);put("guard_route_wait",0);put("guard_cover",0);put("guard_cover_wait",0);
 goal=pose.position;goalZ=pose.z;wantsMove=false;
 if(get("drone_windup")){put("guard_engineer_goal",0);if(distance>=standOff-.5f)return;put("drone_windup",0);put("drone_busy",0);}
 int repair=-1;float priority=-1;auto origin=chunkOffset(level);auto drones=engineerDrones();
 auto walkClear=[&](Vec2 end,float*feet){if(!friendlyWalkSegment(level,pose.position,pose.z,end,track.scale,feet))return false;auto delta=end-pose.position;float span=lengthSq(delta);if(span<.00001f)return true;
  for(const auto&drone:drones){auto shift=chunkOffset(drone.level)-origin;Vec2 body{drone.position.x+shift.x,drone.position.y+shift.y};if(pose.z>=drone.position.z+EngineerDroneHeight||pose.z+track.scale<=drone.position.z)continue;
   float t=std::clamp(dot(body-pose.position,delta)/span,0.f,1.f),before=lengthSq(body-pose.position),after=lengthSq(body-end);
   if(lengthSq(pose.position+delta*t-body)<.85f*.85f&&(before>=.85f*.85f||after<=before))return false;
  }return true;
 };
 if(get("drone_repair_charge")>0&&distance>=standOff-.35f)for(const auto&drone:drones){
  if(drone.owner!=int(actor)||drone.health>=EngineerDroneHealth||drone.level!=level||std::fabs(drone.position.z-pose.z)>.3f)continue;
  Vec2 point{drone.position.x,drone.position.y};float reach=length(point-pose.position);
  if(reach>6.f||length(point-remembered)<standOff-.35f)continue;
  if(!friendlyRayClear({pose.position.x+origin.x,pose.position.y+origin.y,pose.z+track.scale*.65f},{point.x+origin.x,point.y+origin.y,drone.position.z+EngineerDroneHeight*.65f}))continue;
  float score=(EngineerDroneHealth-drone.health)-reach;if(score>priority){priority=score;repair=drone.slot;goal=point;goalZ=drone.position.z;}
 }
 if(repair>=0){float reach=length(goal-pose.position);put("guard_engineer_goal",0);
  if(reach<1.85f){goal=pose.position;goalZ=pose.z;wantsMove=false;return;}
  // Approach a maintenance distance, not the drone's occupied center.
  goal=goal+normalized(pose.position-goal)*1.65f;
  float supported=pose.z;if(walkClear(goal,&supported)){goalZ=supported;wantsMove=true;return;}
  goal=pose.position;goalZ=pose.z;
 }
 if(get("guard_engineer_chunk")!=level+1){put("guard_engineer_goal",0);put("guard_engineer_wait",0);}
 int wait=std::max(0,get("guard_engineer_wait")-ms);bool cached=get("guard_engineer_goal")!=0;
 Vec2 saved{get("guard_engineer_x")*.001f,get("guard_engineer_y")*.001f};float savedFeet=get("guard_engineer_z")*.001f;
 if(cached&&(length(saved-remembered)<standOff-.2f||get("stuck")>=250||guardPositionReserved(actor,saved,savedFeet)||!walkClear(saved,&savedFeet))){cached=false;wait=0;}
 auto protectedAt=[&](Vec2 point,float feet){Point3 enemy{remembered.x+origin.x,remembered.y+origin.y,rememberedFeet+target.height*.6f};
  return !friendlyRayClear(enemy,{point.x+origin.x,point.y+origin.y,feet+track.scale*.5f})&&!friendlyRayClear(enemy,{point.x+origin.x,point.y+origin.y,feet+track.scale*.85f});};
 bool exposed=get("guard_seen")&&!protectedAt(pose.position,pose.z);
 bool needsRear=distance<standOff-.15f||(exposed&&get("guard_under_fire"));
 if(cached){if(length(saved-pose.position)>.2f){goal=saved;goalZ=savedFeet;wantsMove=true;}else cached=false;}
 if(!cached&&needsRear&&wait==0&&m_npcNavBudget>0){--m_npcNavBudget;wait=700;
  auto away=normalized(pose.position-remembered);if(lengthSq(away)<.001f)away={-std::sin(pose.yaw),-std::cos(pose.yaw)};
  float best=-10000;bool found=false;
  for(float step:{2.5f,4.5f})for(float turn:{0.f,.6f,-.6f,1.2f,-1.2f,1.8f,-1.8f,kPi}){
   float c=std::cos(turn),s=std::sin(turn);Vec2 end=pose.position+Vec2{away.x*c-away.y*s,away.x*s+away.y*c}*step;
   end.x=std::clamp(end.x,.35f,23.65f);end.y=std::clamp(end.y,.35f,23.65f);float gap=length(end-remembered),feet=pose.z;
   if(gap<standOff-.2f||guardPositionReserved(actor,end,pose.z)||!walkClear(end,&feet))continue;
   bool safe=true;for(int sample=1;sample<=4;++sample)safe&=length(pose.position+(end-pose.position)*(sample/4.f)-remembered)>=distance-.05f;
   if(!safe)continue;float score=(protectedAt(end,feet)?12.f:0.f)-std::fabs(gap-standOff)-step*.05f;
   if(score>best){best=score;saved=end;savedFeet=feet;found=true;}
  }
  cached=found;if(found){put("guard_engineer_x",int(std::round(saved.x*1000)));put("guard_engineer_y",int(std::round(saved.y*1000)));put("guard_engineer_z",int(std::round(savedFeet*1000)));put("guard_engineer_chunk",level+1);put("repath",0);goal=saved;goalZ=savedFeet;wantsMove=true;}
 }
 put("guard_engineer_wait",wait);put("guard_engineer_goal",cached);
}
}

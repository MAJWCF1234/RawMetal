#include "../../Game.h"
#include "../Navigation.h"
#include <array>
#include <optional>
namespace retro {
namespace {
constexpr float DangerRadius=4.5f;
float norm3(Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);}
}
bool Game::guardGrenadeExposed(size_t actor)const{
 const auto&npc=m_friendlyActors.at(actor);auto shift=chunkOffset(npc.level)-chunkOffset(m_level);Point3 receiver{npc.pose.position.x+shift.x,npc.pose.position.y+shift.y,npc.pose.z+npc.track().scale*.5f};
 for(const auto&grenade:tacticalGrenades(true)){auto offset=chunkOffset(grenade.level)-chunkOffset(m_level);auto hazard=grenade.position+Point3{offset.x,offset.y,0};if(norm3(receiver-hazard)<DangerRadius&&tacticalGrenadeTrace(hazard,receiver).kind!=ShotKind::Solid)return true;}return false;
}
bool Game::guardDangerIntent(size_t actor,ActorPose&pose,float dt,Vec2&goal,float&goalZ,bool&wantsMove){
 const auto&npc=m_friendlyActors.at(actor);const auto&track=npc.track();
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto put=[&](const char*f,int v){setState(friendlyKey(actor,f),v);};
 int ms=npcMilliseconds(actor,dt,"guard_danger_fraction"),remaining=std::max(0,get("guard_danger_ms")-ms);
 auto origin=chunkOffset(npc.level),active=chunkOffset(m_level),shift=origin-active;
 struct Cue {Point3 p;float urgency;};std::array<Cue,8> cues{};size_t count=0;
 // Fuse danger is a local sensory cue, not enemy-location knowledge. Use the
 // exposed physical projectile from any faction. An actor protected by solid
 // blast cover stays protected rather than abandoning it to scatter.
 auto exposure=[&](Point3 hazard,Vec2 position,float feet){Point3 receiver{position.x+shift.x,position.y+shift.y,feet+track.scale*.5f};float distance=norm3(receiver-hazard);
  if(distance>=DangerRadius||tacticalGrenadeTrace(hazard,receiver).kind==ShotKind::Solid)return 0.f;
  float risk=1-distance/DangerRadius;return risk*risk;
 };
 auto grenades=tacticalGrenades(true);float worst=0;Point3 chosen{};
 for(const auto&grenade:grenades){auto off=chunkOffset(grenade.level)-active;Point3 p=grenade.position+Point3{off.x,off.y,0};
  Point3 receiver{pose.position.x+shift.x,pose.position.y+shift.y,pose.z+track.scale*.5f};
  if(norm3(receiver-p)>6.f||tacticalGrenadeTrace(p,receiver).kind==ShotKind::Solid)continue;
  float risk=exposure(p,pose.position,pose.z);
  float urgency=1.f+std::max(0.f,1.f-grenade.fuse);cues[count++]={p,urgency};
  if(risk*urgency>worst){worst=risk*urgency;chosen=p;}
 }
 if(worst>0){auto global=chosen+Point3{active.x,active.y,0};bool changed=get("guard_danger_ms")==0||norm3(global-Point3{get("guard_danger_x")*.001f,get("guard_danger_y")*.001f,get("guard_danger_z")*.001f})>.35f;
  put("guard_danger_x",int(std::round(global.x*1000)));put("guard_danger_y",int(std::round(global.y*1000)));put("guard_danger_z",int(std::round(global.z*1000)));remaining=600;
  if(changed){put("guard_danger_goal",0);put("guard_danger_wait",0);put("repath",0);}
 }
 else if(get("guard_danger_ms")&&count)remaining=600;
 put("guard_danger_ms",remaining);
 if(!remaining){if(get("guard_danger_goal"))put("repath",0);put("guard_danger_goal",0);put("guard_danger_wait",0);return false;}
 // Keep only the last perceived position through brief occlusion; never read
 // an unseen projectile's live position or its thrower's private landing goal.
 if(!count)cues[count++]={{get("guard_danger_x")*.001f-active.x,get("guard_danger_y")*.001f-active.y,get("guard_danger_z")*.001f},1.f};
 auto riskAt=[&](Vec2 position,float feet){float score=0;for(size_t i=0;i<count;++i)score+=exposure(cues[i].p,position,feet)*cues[i].urgency;return score;};
 auto safeRiskPath=[&](Vec2 end,float feet,float initialRisk){for(int sample=1;sample<=8;++sample){float t=sample/8.f;if(riskAt(pose.position+(end-pose.position)*t,pose.z+(feet-pose.z)*t)>initialRisk+.002f)return false;}return true;};
 float currentRisk=riskAt(pose.position,pose.z);int wait=std::max(0,get("guard_danger_wait")-ms);
 Vec2 cached{get("guard_danger_goal_x")*.001f-origin.x,get("guard_danger_goal_y")*.001f-origin.y};float cachedFeet=get("guard_danger_goal_z")*.001f;
 bool hasGoal=get("guard_danger_goal")!=0;
 if(hasGoal&&(riskAt(cached,cachedFeet)>currentRisk+.001f||!safeRiskPath(cached,cachedFeet,currentRisk)||get("stuck")>=250)){hasGoal=false;wait=0;}
 if(currentRisk<=.00001f){hasGoal=false;wantsMove=false;}
 else if(wait==0&&m_npcNavBudget>0){
  --m_npcNavBudget;wait=350;float best=currentRisk;Vec2 selected=pose.position;float selectedFeet=pose.z;bool found=false;
  std::optional<World> planning;const auto&world=friendlyWorld(npc.level);
  for(size_t door=0;door<world.doors().size();++door)if(!aiDoorLocked(world,npc.level,world.doors()[door])&&world.doors()[door].open<world.doors()[door].maxOpen){if(!planning)planning=world;planning->setDoor(int(door),world.doors()[door].maxOpen,true);}
  Vec2 hazard{get("guard_danger_x")*.001f-origin.x,get("guard_danger_y")*.001f-origin.y};auto away=normalized(pose.position-hazard);if(lengthSq(away)<.001f)away={std::sin(pose.yaw),std::cos(pose.yaw)};
  // One bounded local search, with angular alternatives and two distances.
  // Ordinary movement still owns doors, floor support and collision. A trapped
  // actor holds instead of firing or forcing a route through solid geometry.
  for(float distance:{4.9f,2.5f})for(float turn:{0.f,.6f,-.6f,1.2f,-1.2f,1.8f,-1.8f,kPi}){
   float c=std::cos(turn),s=std::sin(turn);Vec2 direction{away.x*c-away.y*s,away.x*s+away.y*c};auto candidate=pose.position+direction*distance;
   candidate.x=std::clamp(candidate.x,.35f,23.65f);candidate.y=std::clamp(candidate.y,.35f,23.65f);
   if(lengthSq(candidate-pose.position)<.15f*.15f||guardPositionReserved(actor,candidate,pose.z))continue;
   float feet=pose.z;bool clear=friendlyWalkSegment(npc.level,pose.position,pose.z,candidate,track.scale,&feet);
   if(!clear&&planning)clear=aiWalkSegment(*planning,pose.position,pose.z,candidate,track.scale,&feet);
   if(!clear||!safeRiskPath(candidate,feet,currentRisk))continue;
   float risk=riskAt(candidate,feet);if(risk+.00001f<best){best=risk;selected=candidate;selectedFeet=feet;found=true;}
  }
  hasGoal=found;if(found){cached=selected;cachedFeet=selectedFeet;put("guard_danger_goal_x",int(std::round((cached.x+origin.x)*1000)));put("guard_danger_goal_y",int(std::round((cached.y+origin.y)*1000)));put("guard_danger_goal_z",int(std::round(cachedFeet*1000)));put("repath",0);}
 }else if(wait==0&&hasGoal){
  // Keep a chosen escape while the shared search budget is empty; actors still
  // without a goal keep wait at 0 so they can claim the next free search slot.
  wait=350;
 }
 put("guard_danger_wait",wait);put("guard_danger_goal",hasGoal);
 goal=hasGoal?cached:pose.position;goalZ=hasGoal?cachedFeet:pose.z;wantsMove=hasGoal&&lengthSq(cached-pose.position)>.12f*.12f&&currentRisk>.00001f;
 for(const char*f:{"weapon_aim","weapon_raise","weapon_bash","weapon_bash_pose","grenade_windup","guard_suppress_ms","guard_slot","guard_slot_ms","support_target","support_ms","drone_windup","drone_busy","drone_repair_ms","drone_repair_target","guard_engineer_active","guard_engineer_goal","guard_engineer_wait","guard_cover","guard_cover_wait","guard_cover_mode","guard_route_next","guard_route_wait"})put(f,0);
 // Damage and ammunition already consumed remain consumed. Pending reload
 // clocks/events pause intact; uncommitted attacks cannot fire during escape.
 auto action=GuardAction(get("weapon_action"));if(action==GuardAction::Fire||action==GuardAction::Bash){put("weapon_action",0);put("weapon_action_ms",0);put("weapon_action_events",0);}
 return true;
}
void Game::validateGuardDanger(size_t actor)const{
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};
 if(get("guard_danger_ms")<0||get("guard_danger_ms")>600||get("guard_danger_fraction")<0||get("guard_danger_fraction")>=1000||get("guard_danger_goal")<0||get("guard_danger_goal")>1||get("guard_danger_wait")<0||get("guard_danger_wait")>350||(get("guard_danger_goal")&&!get("guard_danger_ms")))throw std::runtime_error("Invalid saved guard danger clock or route");
 for(const char*f:{"guard_danger_x","guard_danger_y","guard_danger_z","guard_danger_goal_x","guard_danger_goal_y","guard_danger_goal_z"})if(get(f)<-10000000||get(f)>10000000)throw std::runtime_error("Invalid saved guard danger position");
 if(get("guard_danger_goal")){int savedChunk=get("chunk");int level=savedChunk?savedChunk-1:m_friendlyActors[actor].level;if(level<0||level>=chunkCount())throw std::runtime_error("Invalid saved danger chunk");auto origin=chunkOffset(level);float x=get("guard_danger_goal_x")*.001f-origin.x,y=get("guard_danger_goal_y")*.001f-origin.y,z=get("guard_danger_goal_z")*.001f;
  if(x<.349f||x>23.651f||y<.349f||y>23.651f||std::fabs(z)>100)throw std::runtime_error("Saved guard danger goal is outside its physical chunk");
 }
}
}

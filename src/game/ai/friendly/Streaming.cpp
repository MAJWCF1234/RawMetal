#include "../../../core/FrameProfile.h"
#include "../../Game.h"
#include "../Navigation.h"
#include "../WalkQueryCache.h"
#include "Schedule.h"
#include <cmath>
#include <queue>
namespace retro {
StateId Game::friendlyKey(size_t actor,std::string_view field)const{
 const auto&npc=m_friendlyActors.at(actor);return actorAiState(npc.homeLevel,npc.homeActor,npc.track().timerState,field);
}
bool Game::scriptedHandoffActive(size_t actor)const{
 if(actor>=m_friendlyActors.size()||npcIncapacitated(actor))return false;const auto&track=m_friendlyActors[actor].track();
 return track.ai.mode==ActorAiMode::Scripted&&(state(friendlyKey(actor,"fear"))>0||state(friendlyKey(actor,"schedule"))==int(CivilianSchedule::Flee));
}
bool Game::scriptedHandoffActive(int level,size_t homeActor)const{
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];if(npc.homeLevel==level&&npc.homeActor==homeActor)return scriptedHandoffActive(i);}
 return false;
}
void Game::rebuildFriendlyActors(){
 m_friendlyActors.clear();
 for(int level=0;level<chunkCount();++level){const auto&world=level==m_level?m_world:m_chunks[level].world;
  for(size_t i=0;i<world.actorTracks().size();++i){const auto&track=world.actorTracks()[i];
   FriendlyActor npc{level,level,i,world.actorPose(i),world.m_mapData};size_t index=m_friendlyActors.size();m_friendlyActors.push_back(npc);
   int saved=state(friendlyKey(index,"chunk"));if(saved<0||saved>chunkCount())throw std::runtime_error("Invalid saved NPC chunk");
   int restraint=state(friendlyKey(index,"restraint")),restraintMs=state(friendlyKey(index,"restraint_ms"));
   int restraintFraction=state(friendlyKey(index,"restraint_fraction"));
   if(restraint<0||restraint>2||restraintMs<0||restraintMs>120000||restraintFraction<0||restraintFraction>=1000||(restraint&&!state(friendlyKey(index,"saved"))))throw std::runtime_error("Invalid saved NPC restraint");
   if(track.ai.mode==ActorAiMode::Guard)validateGuardState(index);
   if(saved)m_friendlyActors[index].level=saved-1;
   if(m_friendlyActors[index].level==m_level)setState(friendlyKey(index,"engaged"),1);
   if(state(friendlyKey(index,"saved"))){auto&pose=m_friendlyActors[index].pose;pose.position={state(friendlyKey(index,"x"))*.001f,state(friendlyKey(index,"y"))*.001f};pose.z=state(friendlyKey(index,"z"))*.001f;pose.yaw=state(friendlyKey(index,"yaw"))*.0001f;pose.phase=state(friendlyKey(index,"phase"))*.0001f;
    int crouched=state(friendlyKey(index,"crouched")),grounded=state(friendlyKey(index,"grounded")),vv=state(friendlyKey(index,"vv"));
    if(state(friendlyKey(index,"locomotion"))&&(crouched<0||crouched>1||grounded<0||grounded>1||vv<-20000||vv>10000))throw std::runtime_error("Invalid saved NPC locomotion");
    pose.clip=state(friendlyKey(index,"walking"))?1:crouched?5:0;
    if(pose.position.x<0||pose.position.y<0||pose.position.x>=24||pose.position.y>=24||std::fabs(pose.z)>100||std::fabs(pose.yaw)>kPi+.001f||pose.phase<0||pose.phase>1)throw std::runtime_error("Invalid saved NPC pose");
    // Quantized +/-31416 is slightly outside +/-pi. Restore the endpoint
    // before re-encoding so remainder() cannot flip its serialized sign.
    pose.yaw=std::clamp(pose.yaw,-kPi,kPi);
   }
   if(track.deadState&&state(track.deadState)){auto&pose=m_friendlyActors[index].pose;pose.position={state(actorPositionState(track.deadState,0))*.001f,state(actorPositionState(track.deadState,1))*.001f};pose.z=state(actorPositionState(track.deadState,2))*.001f;pose.clip=4;}
   else if(restraint){auto&pose=m_friendlyActors[index].pose;pose.lookAtActor=-1;pose.clip=restraint==2?0:4;float duration=cleanupAnimationDuration(track.visual,4);pose.phase=restraint==2?0:std::clamp(restraintMs/(duration>0?duration*1000:1000.f),0.f,1.f);}
   if(track.ai.mode==ActorAiMode::Guard&&(npcIncapacitated(index)||m_friendlyActors[index].pose.clip==4||!autonomousActor(track)))clearGuardSchedule(index);
  }
 }
 // A legacy partial kit is evidence that its wound was already admitted.
 // Adopt that baseline after the complete NPC collection is reconstructed;
 // canonical pending hits and active under-fire cues still require defense.
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&provider=m_friendlyActors[actor];
  if(npcIncapacitated(actor)||provider.track().ai.role!=GuardSquadRole::Medic||state(friendlyKey(actor,"schedule"))||!state(friendlyKey(actor,"support_ms")))continue;
  int patient=state(friendlyKey(actor,"support_target"))-1;if(patient<0||size_t(patient)>=m_friendlyActors.size()||npcIncapacitated(size_t(patient)))continue;const auto&recipient=m_friendlyActors[size_t(patient)];
  if(recipient.track().ai.mode==ActorAiMode::Guard&&!state(friendlyKey(size_t(patient),"hit_source"))&&!state(friendlyKey(size_t(patient),"guard_under_fire")))setState(friendlyKey(size_t(patient),"guard_damage"),state(recipient.track().damageState));
 }
}
bool Game::friendlyChunkNeeded(int level)const{
 if(engineerDroneChunkNeeded(level))return true;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();
  if(npcIncapacitated(i)||!autonomousActor(track)||npc.pose.clip==4)continue;
  if(track.ai.mode==ActorAiMode::Guard){
   auto schedule=GuardSchedule(state(friendlyKey(i,"schedule")));bool scheduled=schedule!=GuardSchedule::None&&schedule!=GuardSchedule::Patrol;
   bool active=scheduled||state(friendlyKey(i,"guard_danger_ms"))>0||(guardState(i)!=GuardState::Patrol&&state(friendlyKey(i,"guard_memory")))||npc.level!=npc.homeLevel||state(friendlyKey(i,"guard_return"));
   int next=state(friendlyKey(i,"guard_route_next"))-1;if(active&&(npc.level==level||next==level))return true;continue;
  }
  if(!state(friendlyKey(i,"engaged")))continue;
  bool follow=track.ai.mode==ActorAiMode::Escort?!state(friendlyKey(i,"hold")):state(friendlyKey(i,"follow"))!=0;
  int next=state(friendlyKey(i,"route_next"))-1;
  if(follow&&next>=0&&(npc.level==level||next==level))return true;
 }return false;
}
int Game::friendlyChunkAt(Vec2 global,int preferred)const{
 auto inside=[&](int level){auto origin=chunkOffset(level);return global.x>=origin.x&&global.x<origin.x+24&&global.y>=origin.y&&global.y<origin.y+24;};
 if(inside(preferred))return preferred;if(inside(m_level))return m_level;
 for(int level=0;level<chunkCount();++level)if(inside(level))return level;return -1;
}
bool Game::friendlyBoundaryOpen(int from,int to,Vec2 global)const{
 if(from==to)return true;const auto&a=from==m_level?m_world:m_chunks[from].world;const auto&b=to==m_level?m_world:m_chunks[to].world;
 auto origin=chunkOffset(from),other=chunkOffset(to),delta=other-origin;
 if(std::fabs(delta.x-24)<.01f&&std::fabs(delta.y)<.01f)return a.openEastBoundary()&&b.openWestBoundary();
 if(std::fabs(delta.x+24)<.01f&&std::fabs(delta.y)<.01f)return a.openWestBoundary()&&b.openEastBoundary();
 if(std::fabs(delta.x)>.01f||std::fabs(std::fabs(delta.y)-24)>.01f)return false;
 bool south=delta.y>0;if(south?(a.openSouthBoundary()&&b.openNorthBoundary()):(a.openNorthBoundary()&&b.openSouthBoundary()))return true;
 auto permitted=[&](const World&w,int level,const Door&door){return !(door.requireEnemiesClear&&hostilesRemaining(level))&&!(door.requireControl&&!w.controlReleased())&&(!door.requireState||state(door.requireState)==door.requireValue);};
 for(const auto&door:a.doors())if((south?door.transfer:door.entry)&&door.open>.05f&&permitted(a,from,door)&&global.x>origin.x+door.left+.2f&&global.x<origin.x+door.right-.2f)
  for(const auto&match:b.doors())if((south?match.entry:match.transfer)&&match.open>.05f&&permitted(b,to,match)&&global.x>other.x+match.left+.2f&&global.x<other.x+match.right-.2f)return true;
 return false;
}
bool Game::friendlyWalkSegment(int level,Vec2 start,float feet,Vec2 goal,float height,float*endFeet){
 // Exact memo for in-chunk sweeps while a WalkQueryCache::Scope is live.
 // Same answers as the uncached body; seam-crossing probes stay live.
 if(auto*cache=ai::WalkQueryCache::active(friendlyWorld(level))){
  auto live=[&](Vec2 from,float fromFeet,Vec2 to,float hull,float*out){return friendlyWalkSegmentUncached(level,from,fromFeet,to,hull,out);};
  return cache->walk(start,feet,goal,height,endFeet,live);
 }
 return friendlyWalkSegmentUncached(level,start,feet,goal,height,endFeet);
}
bool Game::friendlyWalkSegmentUncached(int level,Vec2 start,float feet,Vec2 goal,float height,float*endFeet){
 perf::Scope walkProfile(perf::Walk);
 // TraceHull-style: the actor already occupies `start`. Re-proving that
 // footprint every probe doubled short locomotion cost for free. Same support,
 // structure, door and destination fits as before — only the path ahead.
 // Matches enemy `roughSegment`, which already samples from i=1.
 auto delta=goal-start;float distance=length(delta);
 if(distance<.001f){if(endFeet)*endFeet=feet;return true;}
 auto doorLine=[&](const World&w,Vec2 from,Vec2 to,float z,float lattice){
  if(!m_navAuthority||lattice<.001f)return true;auto span=to-from;float spanLength=length(span);
  if(spanLength<.001f)return true;int doors=std::max(1,int(std::ceil(spanLength/lattice)));
  for(int i=1;i<doors;++i){auto at=from+span*(float(i)/doors);if(w.doorBlocks(at.x,at.y,z,height))return false;}
  return true;
 };
 // A convex segment whose complete footprint stays inside this chunk cannot
 // cross a seam. Nav authority uses TraceHull-scale footprint sites; the dense
 // lattice remains available via --lattice-walk-probes.
 if(std::min(start.x,goal.x)>=.2f&&std::min(start.y,goal.y)>=.2f&&std::max(start.x,goal.x)<23.8f&&std::max(start.y,goal.y)<23.8f){
  ensureChunk(level);const auto&w=friendlyWorld(level);float lattice=aiWalkSamplePitch(w.hasTerrain());
  float pitch=m_navAuthority?aiWalkAuthorityPitch(w.hasTerrain()):lattice;
  int samples=std::max(1,int(std::ceil(distance/pitch)));float z=feet;
  for(int step=1;step<=samples;++step){auto point=start+delta*(float(step)/samples);auto previous=start+delta*(float(step-1)/samples);
   if(!aiStructureSegment(w,previous,z,point,height,m_aiQueryCaching))return false;
   if(!doorLine(w,previous,point,z,lattice))return false;
   float ground=-10000;for(float x:{-.2f,.2f})for(float y:{-.2f,.2f})ground=std::max(ground,w.supportBelow(point.x+x,point.y+y,z+.265f));
   float stepHeight=w.hasTerrain()?.34f:.24f;if(ground-z>stepHeight+.025f||z-ground>.48f||w.railBlocksHull(point.x,point.y,.2f,ground,height))return false;
   for(float x:{-.2f,0.f,.2f})for(float y:{-.2f,0.f,.2f})if(!w.fits(point.x+x,point.y+y,ground,height)||w.doorBlocks(point.x+x,point.y+y,ground,height))return false;
   z=ground;
  }
  if(endFeet)*endFeet=z;return true;
 }
 auto origin=chunkOffset(level);ensureChunk(level);bool terrain=friendlyWorld(level).hasTerrain();float lattice=aiWalkSamplePitch(terrain);
 float pitch=m_navAuthority?aiWalkAuthorityPitch(terrain):lattice;
 int samples=std::max(1,int(std::ceil(distance/pitch))),previous=level;float z=feet;
 for(int step=1;step<=samples;++step){Vec2 global=origin+start+delta*(float(step)/samples);Vec2 before=origin+start+delta*(float(step-1)/samples);
  int center=friendlyChunkAt(global,previous);if(center<0||!friendlyBoundaryOpen(previous,center,global))return false;ensureChunk(center);float ground=-10000;
  auto off=chunkOffset(center);const auto&centerWorld=friendlyWorld(center);
  if(!aiStructureSegment(centerWorld,before-off,z,global-off,height,m_aiQueryCaching))return false;
  if(!doorLine(centerWorld,before-off,global-off,z,lattice))return false;
  for(float x:{-.2f,.2f})for(float y:{-.2f,.2f}){Vec2 point=global+Vec2{x,y};int chunk=friendlyChunkAt(point,center);if(chunk<0||!friendlyBoundaryOpen(center,chunk,point))return false;ensureChunk(chunk);auto local=point-chunkOffset(chunk);ground=std::max(ground,friendlyWorld(chunk).supportBelow(local.x,local.y,z+.265f));}
  float stepHeight=centerWorld.hasTerrain()?.34f:.24f;if(ground-z>stepHeight+.025f||z-ground>.48f)return false;
  for(float x:{-.2f,0.f,.2f})for(float y:{-.2f,0.f,.2f}){Vec2 point=global+Vec2{x,y};int chunk=friendlyChunkAt(point,center);if(chunk<0||!friendlyBoundaryOpen(center,chunk,point))return false;ensureChunk(chunk);auto local=point-chunkOffset(chunk);const auto&w=friendlyWorld(chunk);
   auto localCenter=global-chunkOffset(chunk);if(!w.fits(local.x,local.y,ground,height)||w.doorBlocks(local.x,local.y,ground,height)||w.railBlocksHull(localCenter.x,localCenter.y,.2f,ground,height))return false;}
  z=ground;previous=center;
 }
 if(endFeet)*endFeet=z;return true;
}
float Game::friendlyHullHeight(size_t actor,const ActorTrack&track)const{
 if(actor<m_friendlyActors.size()&&npcIncapacitated(actor))return npcHeld(actor)?track.scale:.35f;
 float standing=track.scale;if(actor>=m_friendlyActors.size()||!state(friendlyKey(actor,"crouched")))return standing;
 return npcCrouchedHeight(track);
}
float Game::friendlySupportAt(int level,Vec2 position,float feet){
 auto origin=chunkOffset(level);Vec2 global=origin+position;int center=friendlyChunkAt(global,level);if(center<0)return feet;ensureChunk(center);float ground=-10000;
 auto offset=chunkOffset(center);
 // Use the actual rounded global corners for this test. Keep the original
 // global+corner-origin arithmetic, including on chunks far from the origin.
 if(m_aiQueryCaching&&global.x-.2f>=offset.x&&global.y-.2f>=offset.y&&global.x+.2f<offset.x+24&&global.y+.2f<offset.y+24){
  const auto&w=friendlyWorld(center);
  for(float x:{-.2f,.2f})for(float y:{-.2f,.2f}){auto local=global+Vec2{x,y}-offset;ground=std::max(ground,w.supportBelow(local.x,local.y,feet));}
  return ground>-9999?ground:feet;
 }
 for(float x:{-.2f,.2f})for(float y:{-.2f,.2f}){Vec2 point=global+Vec2{x,y};int chunk=friendlyChunkAt(point,center);if(chunk<0||!friendlyBoundaryOpen(center,chunk,point))continue;ensureChunk(chunk);auto local=point-chunkOffset(chunk);ground=std::max(ground,friendlyWorld(chunk).supportBelow(local.x,local.y,feet));}
 return ground>-9999?ground:feet;
}
bool Game::friendlyBodyFits(int level,Vec2 position,float feet,float height){
 auto origin=chunkOffset(level);Vec2 global=origin+position;int center=friendlyChunkAt(global,level);if(center<0||!friendlyBoundaryOpen(level,center,global))return false;ensureChunk(center);
 auto offset=chunkOffset(center);
 if(m_aiQueryCaching&&global.x-.2f>=offset.x&&global.y-.2f>=offset.y&&global.x+.2f<offset.x+24&&global.y+.2f<offset.y+24){
  const auto&w=friendlyWorld(center);auto localCenter=global-offset;
  if(w.railBlocksHull(localCenter.x,localCenter.y,.2f,feet,height))return false;
  for(float x:{-.2f,0.f,.2f})for(float y:{-.2f,0.f,.2f}){auto local=global+Vec2{x,y}-offset;if(!w.fits(local.x,local.y,feet,height)||w.doorBlocks(local.x,local.y,feet,height))return false;}
  return true;
 }
 for(float x:{-.2f,0.f,.2f})for(float y:{-.2f,0.f,.2f}){Vec2 point=global+Vec2{x,y};int chunk=friendlyChunkAt(point,center);if(chunk<0||!friendlyBoundaryOpen(center,chunk,point))return false;ensureChunk(chunk);auto local=point-chunkOffset(chunk);const auto&w=friendlyWorld(chunk);
  auto localCenter=global-chunkOffset(chunk);if(!w.fits(local.x,local.y,feet,height)||w.doorBlocks(local.x,local.y,feet,height)||w.railBlocksHull(localCenter.x,localCenter.y,.2f,feet,height))return false;}
 return true;
}
bool Game::friendlyRayClear(Point3 from,Point3 to)const{
 auto delta=to-from;int samples=std::max(1,int(std::ceil(std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z)/.04f)));int previous=friendlyChunkAt({from.x,from.y},m_level);if(previous<0)return false;
 for(int i=0;i<=samples;++i){auto point=from+delta*(float(i)/samples);int level=friendlyChunkAt({point.x,point.y},previous);if(level<0||!chunkResident(level)||!friendlyBoundaryOpen(previous,level,{point.x,point.y}))return false;
  auto local=Vec2{point.x,point.y}-chunkOffset(level);const auto&world=level==m_level?m_world:m_chunks[level].world;
  if(!world.fits(local.x,local.y,point.z,.015f)||world.doorBlocks(local.x,local.y,point.z,.015f))return false;previous=level;
 }return true;
}
bool Game::friendlyPortal(int from,int target,Vec2 position,float feet,float height,Vec2&goal,float&goalFeet,float targetFeet){
 if(from==target)return false;std::vector<int> parent(chunkCount(),-1);std::queue<int> queue;queue.push(from);parent[from]=from;
 while(!queue.empty()&&parent[target]<0){int current=queue.front();queue.pop();auto origin=chunkOffset(current);
  for(int next=0;next<chunkCount();++next)if(parent[next]<0){auto other=chunkOffset(next),delta=other-origin;if(!((std::fabs(std::fabs(delta.x)-24)<.01f&&std::fabs(delta.y)<.01f)||(std::fabs(std::fabs(delta.y)-24)<.01f&&std::fabs(delta.x)<.01f)))continue;
   bool connected=false;for(int cell=0;cell<24&&!connected;++cell){Vec2 point=origin+(std::fabs(delta.x)>1?Vec2{delta.x>0?24.f:0.f,cell+.5f}:Vec2{cell+.5f,delta.y>0?24.f:0.f});connected=friendlyBoundaryOpen(current,next,point);}
   if(connected){parent[next]=current;queue.push(next);}
  }
 }
 if(parent[target]<0)return false;int next=target;while(parent[next]!=from)next=parent[next];ensureChunk(from);ensureChunk(next);
 auto origin=chunkOffset(from),delta=chunkOffset(next)-origin;float best=1e9f;bool found=false;
 for(int cell=-1;cell<24;++cell){float along=cell<0?(std::fabs(delta.x)>1?position.y:position.x):cell+.5f;along=std::clamp(along,.35f,23.65f);
  Vec2 inside=std::fabs(delta.x)>1?Vec2{delta.x>0?23.65f:.35f,along}:Vec2{along,delta.y>0?23.65f:.35f};auto outside=inside+normalized(delta)*.7f;
  if(!friendlyBoundaryOpen(from,next,origin+outside))continue;
  for(float trial:{feet,std::isfinite(targetFeet)?targetFeet:m_player.z}){float z=friendlyWorld(from).supportBelow(inside.x,inside.y,trial+.265f),end=z;
   // This is a hypothetical planner start, not an already occupied actor
   // footprint. A seam sweep alone can start behind a wall and look clear.
   if(!friendlyBodyFits(from,inside,z,height))continue;
   if(!friendlyWalkSegment(from,inside,z,outside,height,&end))continue;float score=lengthSq(inside-position)+std::fabs(z-feet)*8;
   if(score<best){best=score;goal=outside;goalFeet=z;found=true;}
  }
 }
 return found;
}
bool Game::guardRoute(int level,size_t actor,Vec2 globalGoal,float feet,int ms,Vec2&goal,float&goalFeet){
 const auto&npc=m_friendlyActors.at(actor);auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto put=[&](const char*f,int v){setState(friendlyKey(actor,f),v);};
 auto origin=chunkOffset(level);int target=friendlyChunkAt(globalGoal,level),next=get("guard_route_next")-1;goal=npc.pose.position;goalFeet=npc.pose.z;
 if(target<0){put("guard_route_target",0);put("guard_route_next",0);put("guard_route_wait",0);return false;}
 if(target==level){goal=globalGoal-origin;goalFeet=feet;put("guard_route_target",target+1);put("guard_route_next",0);put("guard_route_wait",0);return true;}
 int wait=get("guard_route_target")==target+1?std::max(0,get("guard_route_wait")-ms):0;
 Vec2 portalGlobal{get("guard_portal_x")*.001f,get("guard_portal_y")*.001f};
 bool cached=get("guard_route_target")==target+1&&next>=0&&next<chunkCount()&&next!=level&&wait>0&&friendlyBoundaryOpen(level,next,portalGlobal);
 if(!cached){
  if(m_npcNavBudget>0&&(wait==0||next>=0||get("guard_route_target")!=target+1)){
   --m_npcNavBudget;Vec2 localPortal;float z=feet;cached=friendlyPortal(level,target,npc.pose.position,npc.pose.z,friendlyHullHeight(actor,npc.track()),localPortal,z,feet);
   next=cached?friendlyChunkAt(localPortal+origin,level):-1;put("guard_route_next",next+1);
   if(cached){portalGlobal=localPortal+origin;put("guard_portal_x",int(std::round(portalGlobal.x*1000)));put("guard_portal_y",int(std::round(portalGlobal.y*1000)));put("guard_portal_z",int(std::round(z*1000)));}
   wait=600+int(actor%7)*30;
  }else if(wait==0){
   // Same backoff as flank/cover: empty budget must not pin wait at 0 forever.
   wait=600+int(actor%7)*30;
  }
 }
 put("guard_route_target",target+1);put("guard_route_wait",wait);
 if(cached){goal=portalGlobal-origin;goalFeet=get("guard_portal_z")*.001f;}return cached;
}
void Game::updateFriendlyActors(float dt,bool guards){
 perf::Scope frameProfile(perf::Friendly);
 auto stimuli=guards?aiStimuli():std::vector<AiStimulus>{};
 if(guards){for(auto shot:m_guardShots){shot.source=shot.source-chunkOffset(m_level);shot.categories=ai::SoundWeapon;stimuli.push_back(shot);}m_guardShots.clear();}
 for(size_t i=0;i<m_friendlyActors.size();++i){auto&npc=m_friendlyActors[i];const auto&track=npc.track();auto&home=friendlyWorld(npc.homeLevel);
  if(dt>0&&(track.ai.mode==ActorAiMode::Guard)!=guards)continue;
  if(npcIncapacitated(i)){
   // Root-controlled held placement and a dropped unconscious body bypass
   // patrol, scripted stage resets and autonomous chunk migration.
   auto old=npc.pose;npc.pose=updateFriendlyAI(friendlyWorld(npc.level),npc.level,i,track,old,old,dt,&stimuli);
   setState(friendlyKey(i,"phase"),int(std::round(npc.pose.phase*10000)));setState(friendlyKey(i,"saved"),1);
   auto projected=npc.pose;projected.position+=chunkOffset(npc.level)-chunkOffset(npc.homeLevel);home.setActorPose(npc.homeActor,projected);continue;
  }
  if(track.ai.mode==ActorAiMode::Guard&&(npc.pose.clip==4||(track.deadState&&state(track.deadState))))clearGuardSchedule(i);
  bool scripted=track.ai.mode==ActorAiMode::Scripted;
  if(scripted&&guards)continue;
  if(!autonomousActor(track)&&!scripted){
   npc.level=npc.homeLevel;auto staged=home.actorPose(npc.homeActor);
   // Enable-gated squads are not AI-autonomous yet, but they stay on stage.
   // Keep UAL idle cycling instead of freezing on authored key phase 0 (bind).
   if(dt>0&&humanActorVisual(track.visual)&&staged.clip!=4){
    auto previousPose=staged;
    float previous=state(friendlyKey(i,"saved"))?state(friendlyKey(i,"phase"))*.0001f:staged.phase;
    float duration=cleanupAnimationDuration(track.visual,0);
    staged.clip=0;staged.phase=std::fmod(previous+dt*(duration>0?1/duration:.45f),1.f);
    staged=friendlyAwareness(home,npc.homeLevel,npc.homeActor,previousPose,staged,dt);
    setState(friendlyKey(i,"x"),int(std::round(staged.position.x*1000)));setState(friendlyKey(i,"y"),int(std::round(staged.position.y*1000)));setState(friendlyKey(i,"z"),int(std::round(staged.z*1000)));
    setState(friendlyKey(i,"phase"),int(std::round(staged.phase*10000)));setState(friendlyKey(i,"yaw"),int(std::round(std::remainder(staged.yaw,2*kPi)*10000)));setState(friendlyKey(i,"walking"),0);setState(friendlyKey(i,"saved"),1);
   }else setState(friendlyKey(i,"saved"),0);
   npc.pose=staged;setState(friendlyKey(i,"running"),0);setState(friendlyKey(i,"chunk"),npc.level+1);
   if(track.ai.mode==ActorAiMode::Guard&&(state(friendlyKey(i,"schedule"))||state(friendlyKey(i,"guard_danger_ms"))||state(friendlyKey(i,"guard_mode"))||state(friendlyKey(i,"guard_target_kind"))||state(friendlyKey(i,"support_target")))){
    clearGuardSchedule(i);for(const char*field:{"guard_danger_ms","guard_danger_goal","guard_danger_wait","guard_danger_fraction","guard_engineer_active","guard_engineer_goal","guard_engineer_wait","drone_repair_target","drone_repair_ms","drone_busy","drone_windup"})setState(friendlyKey(i,field),0);setState(friendlyKey(i,"guard_mode"),0);setState(friendlyKey(i,"guard_recognition"),0);setState(friendlyKey(i,"guard_seen"),0);setState(friendlyKey(i,"guard_confirmed"),0);rememberGuardTarget(i,{});
   }
   continue;
  }
  if(scripted&&!scriptedHandoffActive(i)){npc.level=npc.homeLevel;npc.pose=home.actorPose(npc.homeActor);setState(friendlyKey(i,"chunk"),npc.level+1);}
  if(npc.level==m_level)setState(friendlyKey(i,"engaged"),1);
  if(!chunkResident(npc.level)){
   if(scripted)continue;
   bool follow=track.ai.mode==ActorAiMode::Escort?!state(friendlyKey(i,"hold")):state(friendlyKey(i,"follow"))!=0;Vec2 goal;float z;
   if(track.ai.mode==ActorAiMode::Guard){
    auto schedule=GuardSchedule(state(friendlyKey(i,"schedule")));bool scheduled=schedule!=GuardSchedule::None&&schedule!=GuardSchedule::Patrol;
    bool active=scheduled||state(friendlyKey(i,"guard_danger_ms"))>0||(guardState(i)!=GuardState::Patrol&&state(friendlyKey(i,"guard_memory")))||npc.level!=npc.homeLevel||state(friendlyKey(i,"guard_return"));
    if(dt<=0||!active||m_npcNavBudget<=0)continue;--m_npcNavBudget;ensureChunk(npc.level);
   }else{
   if(dt<=0||!follow||!state(friendlyKey(i,"engaged")))continue;
   int wait=std::max(0,state(friendlyKey(i,"route_wait"))-std::max(1,int(std::round(dt*1000))));setState(friendlyKey(i,"route_wait"),wait);
   // During portal backoff keep ticking AI so escorts do not freeze mid-idle.
   // Only skip the actor when a fresh search is needed and the budget is empty.
   // Failed-search backoff: keep AI ticking so idle does not freeze, but do not
   // spend another portal search until wait elapses.
   bool waiting=wait>0&&state(friendlyKey(i,"route_player"))==m_level+1&&!state(friendlyKey(i,"route_next"));
   if(!waiting){
    if(m_npcNavBudget<=0)continue;--m_npcNavBudget;
    bool found=friendlyPortal(npc.level,m_level,npc.pose.position,npc.pose.z,friendlyHullHeight(i,track),goal,z);
    setState(friendlyKey(i,"route_player"),m_level+1);setState(friendlyKey(i,"route_wait"),600+int(i%7)*30);setState(friendlyKey(i,"route_next"),found?friendlyChunkAt(goal+chunkOffset(npc.level),npc.level)+1:0);
    if(!found)continue;
    setState(friendlyKey(i,"portal_x"),int(std::round(goal.x*1000)));setState(friendlyKey(i,"portal_y"),int(std::round(goal.y*1000)));setState(friendlyKey(i,"portal_z"),int(std::round(z*1000)));
   }
   }
  }
  // Every actor moves and executes tasks every simulation tick. Only expensive
  // senses/planning have a separate cadence inside guardIntent.
  float thinkDt=dt;int debt=state(friendlyKey(i,"think_debt_us"));
  if(debt>0){thinkDt=std::min(thinkDt+debt*1e-6f,3.f/60.f);setState(friendlyKey(i,"think_debt_us"),0);}
  auto old=npc.pose;npc.pose=updateFriendlyAI(friendlyWorld(npc.level),npc.level,i,track,old,old,thinkDt,&stimuli);
  if(scripted&&!scriptedHandoffActive(i)){setState(friendlyKey(i,"saved"),0);setState(friendlyKey(i,"running"),0);setState(friendlyKey(i,"chunk"),npc.level+1);continue;}
  auto global=npc.pose.position+chunkOffset(npc.level);int destination=friendlyChunkAt(global,npc.level);
  if(destination<0)throw std::runtime_error("NPC left the stitched world");
  if(destination!=npc.level){auto shift=chunkOffset(npc.level)-chunkOffset(destination);npc.pose.position+=shift;
   for(auto field:std::array<std::array<const char*,2>,4>{{{"way_x","way_y"},{"goal_x","goal_y"},{"threat_x","threat_y"},{"hit_x","hit_y"}}}){setState(friendlyKey(i,field[0]),state(friendlyKey(i,field[0]))+int(std::round(shift.x*1000)));setState(friendlyKey(i,field[1]),state(friendlyKey(i,field[1]))+int(std::round(shift.y*1000)));}
   npc.level=destination;setState(friendlyKey(i,"repath"),0);
   if(track.ai.mode==ActorAiMode::Guard){
    for(const char*field:{"sense_ready","plan_ready","tactic_ready","room_ready","sense_age","plan_age","tactic_age"})setState(friendlyKey(i,field),0);
    setState(friendlyKey(i,"guard_route_next"),0);setState(friendlyKey(i,"guard_route_wait"),0);
    Vec2 cover{state(friendlyKey(i,"guard_cover_x"))*.001f+shift.x,state(friendlyKey(i,"guard_cover_y"))*.001f+shift.y};
    bool inside=cover.x>=.35f&&cover.x<=23.65f&&cover.y>=.35f&&cover.y<=23.65f;
    if(!inside){setState(friendlyKey(i,"guard_cover"),0);setState(friendlyKey(i,"guard_cover_mode"),0);setState(friendlyKey(i,"guard_cover_wait"),0);}
    setState(friendlyKey(i,"guard_cover_x"),int(std::round(std::clamp(cover.x,0.f,24.f)*1000)));setState(friendlyKey(i,"guard_cover_y"),int(std::round(std::clamp(cover.y,0.f,24.f)*1000)));
   }
   if(npc.pose.clip==4&&track.deadState&&state(track.deadState)){setState(actorPositionState(track.deadState,0),int(std::round(npc.pose.position.x*1000)));setState(actorPositionState(track.deadState,1),int(std::round(npc.pose.position.y*1000)));}
  }
  setState(friendlyKey(i,"chunk"),npc.level+1);setState(friendlyKey(i,"x"),int(std::round(npc.pose.position.x*1000)));setState(friendlyKey(i,"y"),int(std::round(npc.pose.position.y*1000)));setState(friendlyKey(i,"z"),int(std::round(npc.pose.z*1000)));setState(friendlyKey(i,"yaw"),int(std::round(std::remainder(npc.pose.yaw,2*kPi)*10000)));setState(friendlyKey(i,"phase"),int(std::round(npc.pose.phase*10000)));setState(friendlyKey(i,"walking"),npc.pose.clip==1);setState(friendlyKey(i,"saved"),1);
  auto projected=npc.pose;projected.position+=chunkOffset(npc.level)-chunkOffset(npc.homeLevel);home.setActorPose(npc.homeActor,projected);
 }
 if(guards||dt==0)updateGuardFlags();
}
}

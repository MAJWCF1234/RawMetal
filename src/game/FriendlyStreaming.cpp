#include "Game.h"
#include "AINavigation.h"
#include <queue>
namespace retro {
StateId Game::friendlyKey(size_t actor,std::string_view field)const{
 const auto&npc=m_friendlyActors.at(actor);return actorAiState(npc.homeLevel,npc.homeActor,npc.track().timerState,field);
}
void Game::rebuildFriendlyActors(){
 m_friendlyActors.clear();
 for(int level=0;level<chunkCount();++level){const auto&world=level==m_level?m_world:m_chunks[level].world;
  for(size_t i=0;i<world.actorTracks().size();++i){const auto&track=world.actorTracks()[i];if(track.ai.mode==ActorAiMode::Scripted)continue;
   FriendlyActor npc{level,level,i,world.actorPose(i),world.m_mapData};size_t index=m_friendlyActors.size();m_friendlyActors.push_back(npc);
   int saved=state(friendlyKey(index,"chunk"));if(saved<0||saved>chunkCount())throw std::runtime_error("Invalid saved NPC chunk");
   if(track.ai.mode==ActorAiMode::Guard)validateGuardState(index);
   if(saved)m_friendlyActors[index].level=saved-1;
   if(m_friendlyActors[index].level==m_level)setState(friendlyKey(index,"engaged"),1);
   if(state(friendlyKey(index,"saved"))){auto&pose=m_friendlyActors[index].pose;pose.position={state(friendlyKey(index,"x"))*.001f,state(friendlyKey(index,"y"))*.001f};pose.z=state(friendlyKey(index,"z"))*.001f;pose.yaw=state(friendlyKey(index,"yaw"))*.0001f;pose.phase=state(friendlyKey(index,"phase"))*.0001f;pose.clip=state(friendlyKey(index,"walking"))?1:0;
    if(pose.position.x<0||pose.position.y<0||pose.position.x>=24||pose.position.y>=24||std::fabs(pose.z)>100||std::fabs(pose.yaw)>kPi+.001f||pose.phase<0||pose.phase>1)throw std::runtime_error("Invalid saved NPC pose");
   }
   if(track.deadState&&state(track.deadState)){auto&pose=m_friendlyActors[index].pose;pose.position={state(actorPositionState(track.deadState,0))*.001f,state(actorPositionState(track.deadState,1))*.001f};pose.z=state(actorPositionState(track.deadState,2))*.001f;pose.clip=4;}
  }
 }
}
bool Game::friendlyChunkNeeded(int level)const{
 if(engineerDroneChunkNeeded(level))return true;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();
  if(!autonomousActor(track)||npc.pose.clip==4)continue;
  if(track.ai.mode==ActorAiMode::Guard){
   bool active=state(friendlyKey(i,"guard_danger_ms"))>0||(guardState(i)!=GuardState::Patrol&&state(friendlyKey(i,"guard_memory")))||npc.level!=npc.homeLevel||state(friendlyKey(i,"guard_return"));
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
 auto origin=chunkOffset(level),delta=goal-start;int samples=std::max(1,int(std::ceil(length(delta)/.08f))),previous=level;float z=feet;
 for(int step=0;step<=samples;++step){Vec2 global=origin+start+delta*(float(step)/samples);int center=friendlyChunkAt(global,previous);
  if(center<0||!friendlyBoundaryOpen(previous,center,global))return false;ensureChunk(center);float ground=-10000;
  if(step>0){auto before=origin+start+delta*(float(step-1)/samples),off=chunkOffset(center);if(!aiStructureSegment(friendlyWorld(center),before-off,z,global-off,height))return false;}
  for(float x:{-.2f,.2f})for(float y:{-.2f,.2f}){Vec2 point=global+Vec2{x,y};int chunk=friendlyChunkAt(point,center);if(chunk<0||!friendlyBoundaryOpen(center,chunk,point))return false;ensureChunk(chunk);auto local=point-chunkOffset(chunk);ground=std::max(ground,friendlyWorld(chunk).supportBelow(local.x,local.y,z+.265f));}
  float stepHeight=friendlyWorld(center).hasTerrain()?.34f:.24f;if(ground-z>stepHeight+.025f||z-ground>.48f)return false;
  for(float x:{-.2f,0.f,.2f})for(float y:{-.2f,0.f,.2f}){Vec2 point=global+Vec2{x,y};int chunk=friendlyChunkAt(point,center);if(chunk<0||!friendlyBoundaryOpen(center,chunk,point))return false;ensureChunk(chunk);auto local=point-chunkOffset(chunk);const auto&w=friendlyWorld(chunk);
   auto localCenter=global-chunkOffset(chunk);if(!w.fits(local.x,local.y,ground,height)||w.doorBlocks(local.x,local.y,ground,height)||w.railBlocksHull(localCenter.x,localCenter.y,.2f,ground,height))return false;}
  z=ground;previous=center;
 }
 if(endFeet)*endFeet=z;return true;
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
 if(!cached&&m_npcNavBudget>0&&(wait==0||next>=0||get("guard_route_target")!=target+1)){
  --m_npcNavBudget;Vec2 localPortal;float z=feet;cached=friendlyPortal(level,target,npc.pose.position,npc.pose.z,npc.track().scale,localPortal,z,feet);
  next=cached?friendlyChunkAt(localPortal+origin,level):-1;put("guard_route_next",next+1);
  if(cached){portalGlobal=localPortal+origin;put("guard_portal_x",int(std::round(portalGlobal.x*1000)));put("guard_portal_y",int(std::round(portalGlobal.y*1000)));put("guard_portal_z",int(std::round(z*1000)));}
  wait=600+int(actor%7)*30;
 }
 put("guard_route_target",target+1);put("guard_route_wait",wait);
 if(cached){goal=portalGlobal-origin;goalFeet=get("guard_portal_z")*.001f;}return cached;
}
void Game::updateFriendlyActors(float dt,bool guards){
 auto stimuli=guards?aiStimuli():std::vector<AiStimulus>{};
 if(guards){for(auto shot:m_guardShots){shot.source=shot.source-chunkOffset(m_level);stimuli.push_back(shot);}m_guardShots.clear();}
 for(size_t i=0;i<m_friendlyActors.size();++i){auto&npc=m_friendlyActors[i];const auto&track=npc.track();auto&home=friendlyWorld(npc.homeLevel);
  if(dt>0&&(track.ai.mode==ActorAiMode::Guard)!=guards)continue;
  if(!autonomousActor(track)){npc.level=npc.homeLevel;npc.pose=home.actorPose(npc.homeActor);setState(friendlyKey(i,"saved"),0);setState(friendlyKey(i,"running"),0);setState(friendlyKey(i,"chunk"),npc.level+1);if(track.ai.mode==ActorAiMode::Guard){for(const char*field:{"guard_danger_ms","guard_danger_goal","guard_danger_wait","guard_danger_fraction"})setState(friendlyKey(i,field),0);setState(friendlyKey(i,"guard_mode"),0);setState(friendlyKey(i,"guard_recognition"),0);setState(friendlyKey(i,"guard_seen"),0);setState(friendlyKey(i,"guard_confirmed"),0);rememberGuardTarget(i,{});}continue;}
  if(npc.level==m_level)setState(friendlyKey(i,"engaged"),1);
  if(!chunkResident(npc.level)){bool follow=track.ai.mode==ActorAiMode::Escort?!state(friendlyKey(i,"hold")):state(friendlyKey(i,"follow"))!=0;Vec2 goal;float z;
   if(track.ai.mode==ActorAiMode::Guard){
    bool active=state(friendlyKey(i,"guard_danger_ms"))>0||(guardState(i)!=GuardState::Patrol&&state(friendlyKey(i,"guard_memory")))||npc.level!=npc.homeLevel||state(friendlyKey(i,"guard_return"));
    if(dt<=0||!active||m_npcNavBudget<=0)continue;--m_npcNavBudget;ensureChunk(npc.level);
   }else{
   if(dt<=0||!follow||!state(friendlyKey(i,"engaged")))continue;
   int wait=std::max(0,state(friendlyKey(i,"route_wait"))-std::max(1,int(std::round(dt*1000))));setState(friendlyKey(i,"route_wait"),wait);
   if(wait>0&&state(friendlyKey(i,"route_player"))==m_level+1&&!state(friendlyKey(i,"route_next")))continue;
   if(m_npcNavBudget<=0)continue;--m_npcNavBudget;
   bool found=friendlyPortal(npc.level,m_level,npc.pose.position,npc.pose.z,track.scale,goal,z);
   setState(friendlyKey(i,"route_player"),m_level+1);setState(friendlyKey(i,"route_wait"),600+int(i%7)*30);setState(friendlyKey(i,"route_next"),found?friendlyChunkAt(goal+chunkOffset(npc.level),npc.level)+1:0);
   if(!found)continue;
   setState(friendlyKey(i,"portal_x"),int(std::round(goal.x*1000)));setState(friendlyKey(i,"portal_y"),int(std::round(goal.y*1000)));setState(friendlyKey(i,"portal_z"),int(std::round(z*1000)));
   }
  }
  auto old=npc.pose;npc.pose=updateFriendlyAI(friendlyWorld(npc.level),npc.level,i,track,old,old,dt,&stimuli);
  auto global=npc.pose.position+chunkOffset(npc.level);int destination=friendlyChunkAt(global,npc.level);
  if(destination<0)throw std::runtime_error("NPC left the stitched world");
  if(destination!=npc.level){auto shift=chunkOffset(npc.level)-chunkOffset(destination);npc.pose.position+=shift;
   for(auto field:std::array<std::array<const char*,2>,4>{{{"way_x","way_y"},{"goal_x","goal_y"},{"threat_x","threat_y"},{"hit_x","hit_y"}}}){setState(friendlyKey(i,field[0]),state(friendlyKey(i,field[0]))+int(std::round(shift.x*1000)));setState(friendlyKey(i,field[1]),state(friendlyKey(i,field[1]))+int(std::round(shift.y*1000)));}
   npc.level=destination;setState(friendlyKey(i,"repath"),0);
   if(track.ai.mode==ActorAiMode::Guard){
    setState(friendlyKey(i,"guard_route_next"),0);setState(friendlyKey(i,"guard_route_wait"),0);
    Vec2 cover{state(friendlyKey(i,"guard_cover_x"))*.001f+shift.x,state(friendlyKey(i,"guard_cover_y"))*.001f+shift.y};
    bool inside=cover.x>=.35f&&cover.x<=23.65f&&cover.y>=.35f&&cover.y<=23.65f;
    if(!inside){setState(friendlyKey(i,"guard_cover"),0);setState(friendlyKey(i,"guard_cover_mode"),0);setState(friendlyKey(i,"guard_cover_wait"),0);}
    setState(friendlyKey(i,"guard_cover_x"),int(std::round(std::clamp(cover.x,0.f,24.f)*1000)));setState(friendlyKey(i,"guard_cover_y"),int(std::round(std::clamp(cover.y,0.f,24.f)*1000)));
   }
   if(npc.pose.clip==4&&track.deadState){setState(actorPositionState(track.deadState,0),int(std::round(npc.pose.position.x*1000)));setState(actorPositionState(track.deadState,1),int(std::round(npc.pose.position.y*1000)));}
  }
  setState(friendlyKey(i,"chunk"),npc.level+1);setState(friendlyKey(i,"x"),int(std::round(npc.pose.position.x*1000)));setState(friendlyKey(i,"y"),int(std::round(npc.pose.position.y*1000)));setState(friendlyKey(i,"z"),int(std::round(npc.pose.z*1000)));setState(friendlyKey(i,"yaw"),int(std::round(std::remainder(npc.pose.yaw,2*kPi)*10000)));setState(friendlyKey(i,"phase"),int(std::round(npc.pose.phase*10000)));setState(friendlyKey(i,"walking"),npc.pose.clip==1);setState(friendlyKey(i,"saved"),1);
  auto projected=npc.pose;projected.position+=chunkOffset(npc.level)-chunkOffset(npc.homeLevel);home.setActorPose(npc.homeActor,projected);
 }
 if(guards||dt==0)updateGuardFlags();
}
}

#include "../../core/FrameProfile.h"
#include "../Game.h"
#include "Navigation.h"
#include "WalkQueryCache.h"
#include <algorithm>
namespace retro {
const ai::NodeGraph& Game::navGraph(int level,float height){
 ensureChunk(level);
 auto&ready=level==m_level?m_levelNavReady:m_chunks[size_t(level)].navReady;
 auto&graph=level==m_level?m_levelNav:m_chunks[size_t(level)].nav;
 if(!ready){
  const auto&world=friendlyWorld(level);
  std::vector<ai::AuthoredHint> authored;
  if(m_worldId==WorldId::Custom&&m_customCampaign&&level>=0&&size_t(level)<m_customCampaign->maps.size()&&m_customCampaign->maps[size_t(level)]){
   for(const auto&hint:m_customCampaign->maps[size_t(level)]->navHints){
    if(hint.hint<0||hint.hint>1)continue;
    authored.push_back({hint.position,hint.z,hint.hint?ai::NodeHint::Cover:ai::NodeHint::Path});
   }
  }
  // Link through every door as if open: a door shut when the graph is built
  // may open later, and each query certifies its route against live doors.
  std::vector<char> everyDoor(world.doors().size(),1);World::DoorPlan open(world,everyDoor);
  graph=ai::buildNodeGraph(world,height,[&](Vec2 from,float fromFeet,Vec2 to,float hull,float* endFeet){
   return aiWalkSegment(world,from,fromFeet,to,hull,endFeet);
  },authored);
  ready=true;
 }
 return graph;
}
bool Game::findGuardGraphCover(int level,size_t actor,const ActorPose&pose,Point3 threat,Vec2&goal,float&goalZ){
 if(actor>=m_friendlyActors.size())return false;
 const auto&track=m_friendlyActors[actor].track();
 const auto&graph=navGraph(level,track.scale);
 auto walk=[&](Vec2 from,float fromFeet,Vec2 to,float hull,float* endFeet){return friendlyWalkSegment(level,from,fromFeet,to,hull,endFeet);};
 auto covered=[&](Vec2 position,float feet){ActorPose trial=pose;trial.position=position;trial.z=feet;auto facing=Vec2{threat.x,threat.y}-position;if(lengthSq(facing)>.001f)trial.yaw=std::atan2(facing.x,facing.y);return guardCovered(actor,trial,guardTarget(actor));};
 auto reserved=[&](Vec2 position,float feet){return guardPositionReserved(actor,position,feet);};
 return ai::findCoverNode(graph,pose.position,pose.z,{threat.x,threat.y},track.scale,walk,covered,reserved,goal,goalZ);
}
bool Game::findGuardLateralCover(int level,size_t actor,const ActorPose&pose,Point3 threat,Vec2&goal,float&goalZ,float sideSign){
 if(actor>=m_friendlyActors.size())return false;
 const auto&track=m_friendlyActors[actor].track();
 auto walk=[&](Vec2 from,float fromFeet,Vec2 to,float hull,float* endFeet){return friendlyWalkSegment(level,from,fromFeet,to,hull,endFeet);};
 auto covered=[&](Vec2 position,float feet){ActorPose trial=pose;trial.position=position;trial.z=feet;auto facing=Vec2{threat.x,threat.y}-position;if(lengthSq(facing)>.001f)trial.yaw=std::atan2(facing.x,facing.y);return guardCovered(actor,trial,guardTarget(actor));};
 auto reserved=[&](Vec2 position,float feet){return guardPositionReserved(actor,position,feet);};
 return ai::findLateralCover(pose.position,pose.z,{threat.x,threat.y},track.scale,walk,covered,reserved,goal,goalZ,sideSign);
}
bool Game::findGuardCoverFromEnemy(int level,size_t actor,const ActorPose&pose,Point3 threat,Vec2&goal,float&goalZ,float sideSign){
 if(actor>=m_friendlyActors.size())return false;
 const auto&track=m_friendlyActors[actor].track();
 const auto&graph=navGraph(level,track.scale);
 auto walk=[&](Vec2 from,float fromFeet,Vec2 to,float hull,float* endFeet){return friendlyWalkSegment(level,from,fromFeet,to,hull,endFeet);};
 auto covered=[&](Vec2 position,float feet){ActorPose trial=pose;trial.position=position;trial.z=feet;auto facing=Vec2{threat.x,threat.y}-position;if(lengthSq(facing)>.001f)trial.yaw=std::atan2(facing.x,facing.y);return guardCovered(actor,trial,guardTarget(actor));};
 auto reserved=[&](Vec2 position,float feet){return guardPositionReserved(actor,position,feet);};
 // Prefer retained Cover hints so authored/structure nodes win, then lateral.
 if(ai::findNodeCoverFromEnemy(graph,pose.position,pose.z,{threat.x,threat.y},track.scale,walk,covered,reserved,goal,goalZ))return true;
 return ai::findLateralCover(pose.position,pose.z,{threat.x,threat.y},track.scale,walk,covered,reserved,goal,goalZ,sideSign);
}
float Game::guardLateralClearance(int level,size_t actor,const ActorPose&pose,Vec2 direction)const{
 perf::Scope queryProfile(perf::Room);
 if(actor>=m_friendlyActors.size()||lengthSq(direction)<.0001f)return 0;
 const auto&track=m_friendlyActors[actor].track();Vec2 dir=normalized(direction);float clear=0,feet=pose.z;Vec2 previous=pose.position;
 for(float step=.4f;step<=2.4f;step+=.4f){
  auto at=pose.position+dir*step;if(at.x<.35f||at.y<.35f||at.x>23.65f||at.y>23.65f)break;
  float end=feet;if(!const_cast<Game*>(this)->friendlyWalkSegment(level,previous,feet,at,track.scale,&end)||std::fabs(end-pose.z)>.3f)break;
  clear=step;feet=end;previous=at;
 }return clear;
}
bool Game::findGuardCombatPost(int level,size_t actor,const ActorPose&pose,Point3 threat,Vec2&goal,float&goalZ,float sideSign){
 perf::Scope queryProfile(perf::Post);
 if(actor>=m_friendlyActors.size())return false;
 const auto&track=m_friendlyActors[actor].track();const auto&graph=navGraph(level,track.scale);
 Vec2 threatXy{threat.x,threat.y};Vec2 toward=normalized(threatXy-pose.position);if(lengthSq(toward)<.001f)toward={0,1};
 Vec2 preferred{-toward.y,toward.x};if(sideSign<0)preferred=preferred*(-1.f);else if(sideSign==0)preferred={};
 // Preserve the authored score and tie order, but reject worse posts before
 // expensive hull sweeps and raised-weapon shot traces.
 std::vector<std::pair<float,size_t>> ranked;ranked.reserve(graph.nodes.size());
 for(size_t index=0;index<graph.nodes.size();++index){const auto&node=graph.nodes[index];
  if(std::fabs(node.feet-pose.z)>.5f)continue;
  float reach=length(node.position-pose.position);if(reach<.35f||reach>6.5f)continue;
  auto at=node.position;float sideDot=lengthSq(preferred)>.001f?dot(normalized(at-pose.position),preferred):0.f;
  float score=reach+(node.hint==ai::NodeHint::Cover?0.f:1.1f)+std::max(0.f,length(pose.position-threatXy)-length(at-threatXy))*.4f-sideDot*1.25f;
  ranked.push_back({score,index});
 }
 std::sort(ranked.begin(),ranked.end());
 for(auto [score,index]:ranked){const auto&node=graph.nodes[index];auto at=node.position;if(guardPositionReserved(actor,at,node.feet))continue;
  float end=pose.z;if(!friendlyWalkSegment(level,pose.position,pose.z,at,track.scale,&end)||std::fabs(end-node.feet)>.35f)continue;
  auto trial=pose;trial.position=at;trial.z=end;auto facing=threatXy-at;if(lengthSq(facing)>.001f)trial.yaw=std::atan2(facing.x,facing.y);
  if(!guardShotMatches(actor,guardShot(actor,trial,npcWeaponPoseAt(actor,trial,threat,1))))continue;
  goal=at;goalZ=end;return true;
 }
 return false;
}
Vec2 Game::guardGraphWaypoint(int level,Vec2 start,float feet,Vec2 goal,float goalFeet,float height){
 const auto&graph=navGraph(level,height);
 const auto&world=friendlyWorld(level);ai::WalkQueryCache::Scope walkMemo(world,world.m_mapData,m_aiQueryCaching);
 auto walk=[&](Vec2 from,float fromFeet,Vec2 to,float hull,float*endFeet){return friendlyWalkSegment(level,from,fromFeet,to,hull,endFeet);};
 return ai::graphWaypoint(graph,start,feet,goal,goalFeet,height,walk);
}
}

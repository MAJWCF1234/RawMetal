#include "../Game.h"
#include "Navigation.h"
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
Vec2 Game::guardGraphWaypoint(int level,Vec2 start,float feet,Vec2 goal,float goalFeet,float height){
 const auto&graph=navGraph(level,height);
 auto walk=[&](Vec2 from,float fromFeet,Vec2 to,float hull,float* endFeet){return friendlyWalkSegment(level,from,fromFeet,to,hull,endFeet);};
 return ai::graphWaypoint(graph,start,feet,goal,goalFeet,height,walk);
}
}

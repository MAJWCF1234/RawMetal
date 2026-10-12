#include "../../Game.h"
#include "../Perception.h"
namespace retro {
void Game::civilianEscortIntent(World& world,int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<Enemy>& enemies,Vec2& goal,float& goalZ,bool& wantsMove,float& speedScale){
 const auto&config=track.ai;auto get=[&](std::string_view field){return state(friendlyKey(actor,field));};auto put=[&](std::string_view field,int value){setState(friendlyKey(actor,field),value);};
 float height=track.scale;int ms=std::max(1,int(std::round(dt*1000)));
 int fear=std::max(0,std::clamp(get("fear"),0,4000)-ms);Vec2 threat{get("threat_x")*.001f,get("threat_y")*.001f};float nearest=config.dangerRange*config.dangerRange;bool danger=false;
 auto consider=[&](Vec2 position,float z){float distance=lengthSq(position-pose.position);
  if(distance<nearest&&aiCanSee(world,pose.position,pose.z+height*.85f,{std::sin(pose.yaw),std::cos(pose.yaw)},position,z,config.dangerRange,-.25f,2.5f)){nearest=distance;threat=position;danger=true;}};
 if(factionsHostile(actorFaction(track),AiFaction::Creature))for(const auto&enemy:enemies)if(enemy.alive)consider(enemy.pos,(enemy.bodyBottom()+enemy.bodyTop())*.5f);
 for(size_t i=0;i<world.actorTracks().size();++i){const auto&other=world.actorTracks()[i];if(other.visual!=ActorVisual::Wasp&&other.visual!=ActorVisual::Huntsman)continue;
  const auto&target=world.actorPose(i);if(factionsHostile(actorFaction(track),actorFaction(other))&&target.clip!=4&&(!other.deadState||!state(other.deadState)))consider(target.position,target.z+other.scale*.5f);}
 int damage=track.damageState?state(track.damageState):0;if(damage>get("damage")&&level==m_level){danger=true;threat=get("hit_source")?Vec2{get("hit_x")*.001f,get("hit_y")*.001f}:m_player.pos;}put("damage",damage);put("hit_source",0);
 if(danger){fear=4000;put("threat_x",int(std::round(threat.x*1000)));put("threat_y",int(std::round(threat.y*1000)));}put("fear",fear);
 bool follow=config.mode==ActorAiMode::Escort?!get("hold"):get("follow")!=0;
 speedScale=fear>0?1.5f:1.f;
 if(fear>0&&lengthSq(pose.position-threat)<25.f){
  Vec2 away=normalized(pose.position-threat);if(lengthSq(away)<.001f)away={std::sin(pose.yaw),std::cos(pose.yaw)};
  float best=-1e9f;
  for(float angle:{0.f,.8f,-.8f,1.6f,-1.6f}){float c=std::cos(angle),s=std::sin(angle);auto direction=Vec2{away.x*c-away.y*s,away.x*s+away.y*c};auto candidate=pose.position+direction*3.f;
   int destination=friendlyChunkAt(candidate+chunkOffset(level),level);if(destination<0||!friendlyBoundaryOpen(level,destination,candidate+chunkOffset(level)))continue;
   ensureChunk(destination);auto local=candidate+chunkOffset(level)-chunkOffset(destination);const auto&targetWorld=friendlyWorld(destination);
   float z=targetWorld.supportBelow(local.x,local.y,pose.z+.25f);if(!targetWorld.fits(local.x,local.y,z,height))continue;
   float score=lengthSq(candidate-threat);if(friendlyWalkSegment(level,pose.position,pose.z,pose.position+direction*.7f,height))score+=10.f;
   if(score>best){best=score;goal=candidate;goalZ=z;wantsMove=true;}}
 }else if(fear==0&&follow&&get("engaged")){
  if(level==m_level){put("route_next",0);float distance=length(pose.position-m_player.pos);wantsMove=distance>config.followDistance+(get("walking")?-.3f:.3f);if(wantsMove){goal=m_player.pos;goalZ=m_player.z;}}
  else{
   int next=get("route_next")-1,wait=std::max(0,get("route_wait")-ms);Vec2 portal{get("portal_x")*.001f,get("portal_y")*.001f};float z=get("portal_z")*.001f;
   bool cached=next>=0&&next<chunkCount()&&next!=level&&get("route_player")==m_level+1&&wait>0&&friendlyBoundaryOpen(level,next,portal+chunkOffset(level));
   if(!cached&&m_npcNavBudget>0&&(wait<=0||get("route_player")!=m_level+1||next>=0)){--m_npcNavBudget;cached=friendlyPortal(level,m_level,pose.position,pose.z,height,portal,z);next=cached?friendlyChunkAt(portal+chunkOffset(level),level):-1;wait=600+int(actor%7)*30;put("portal_x",int(std::round(portal.x*1000)));put("portal_y",int(std::round(portal.y*1000)));put("portal_z",int(std::round(z*1000)));put("route_player",m_level+1);put("route_next",next+1);}
   put("route_wait",wait);if(cached){goal=portal;goalZ=z;wantsMove=true;}
  }
 }
}
}

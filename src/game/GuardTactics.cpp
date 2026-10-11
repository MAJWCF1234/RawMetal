#include "Game.h"
namespace retro {
void Game::guardReposition(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,Point3 target,int ms,Vec2& goal,float& goalZ,bool& wantsMove){
 auto get=[&](const char* field){return state(friendlyKey(actor,field));};
 auto put=[&](const char* field,int value){setState(friendlyKey(actor,field),value);};
 // Evaluate the fully raised weapon. Raising/aiming still happens separately,
 // so a low-ready barrel does not make the guard shuffle before every shot.
 if(guardShot(actor,pose,npcWeaponPoseAt(actor,pose,target,1)).kind==ShotKind::Player){put("guard_cover",0);put("guard_cover_wait",0);wantsMove=false;return;}
 int wait=std::max(0,get("guard_cover_wait")-ms);
 Vec2 cached{get("guard_cover_x")*.001f,get("guard_cover_y")*.001f};
 if(get("guard_cover")&&lengthSq(cached-pose.position)<.04f)wait=0;
 if(wait==0&&m_npcNavBudget>0){
  --m_npcNavBudget;put("guard_cover",0);
  Vec2 toward=normalized(Vec2{target.x,target.y}-pose.position),side{-toward.y,toward.x};
  if(actor%2)side=side*(-1);
  const std::array<Vec2,8> offsets={side*.75f,side*(-.75f),side*1.5f,side*(-1.5f),side*2.f,side*(-2.f),toward*.65f+side*1.2f,toward*.65f-side*1.2f};
  float best=10000;Vec2 destination{};float destinationZ=pose.z;
  for(auto offset:offsets){auto candidate=pose.position+offset;
   if(candidate.x<.35f||candidate.y<.35f||candidate.x>23.65f||candidate.y>23.65f)continue;
   float feet=pose.z;if(!friendlyWalkSegment(level,pose.position,pose.z,candidate,track.scale,&feet))continue;
   auto trial=pose;trial.position=candidate;trial.z=feet;auto facing=Vec2{target.x,target.y}-candidate;trial.yaw=std::atan2(facing.x,facing.y);
   if(guardShot(actor,trial,npcWeaponPoseAt(actor,trial,target,1)).kind!=ShotKind::Player)continue;
   float score=lengthSq(offset);if(score>=best)continue;best=score;destination=candidate;destinationZ=feet;
  }
  if(best<10000){put("guard_cover",1);put("guard_cover_x",int(std::round(destination.x*1000)));put("guard_cover_y",int(std::round(destination.y*1000)));put("guard_cover_z",int(std::round(destinationZ*1000)));cached=destination;}
  wait=600+int(actor%7)*30;
 }
 put("guard_cover_wait",wait);
 if(get("guard_cover")){goal=cached;goalZ=get("guard_cover_z")*.001f;}
 // With no direct sidestep, the normal collision-aware navigator can approach
 // the remembered target around larger obstacles. It cannot force a locked door.
 wantsMove=true;
}
}

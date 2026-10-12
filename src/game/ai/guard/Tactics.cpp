#include "../../Game.h"
namespace retro {
void Game::guardReposition(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,Point3 target,int ms,Vec2& goal,float& goalZ,bool& wantsMove){
 auto get=[&](const char* field){return state(friendlyKey(actor,field));};
 auto put=[&](const char* field,int value){setState(friendlyKey(actor,field),value);};
 auto selected=guardTarget(actor);if(selected.valid())target={selected.position.x,selected.position.y,selected.feet+selected.height*.6f};
 int role=guardFlankSide(actor);
 auto preferredSide=[&](Vec2 toward){Vec2 side{-toward.y,toward.x};if(role)side=side*float(-role);else if(actor%2)side=side*(-1);return side;};
 int damage=track.damageState?state(track.damageState):0;bool lowHealth=track.health>0&&track.health-damage<=track.health*.35f;
 bool defensive=get("weapon_reload")>0||get("weapon_reloading")||(get("weapon_saved")&&get("weapon_rounds")==0)||get("guard_under_fire")>0||lowHealth;
 int mode=get("guard_cover_mode");
 bool protectedNow=selected.valid()&&guardCovered(actor,pose,selected);
 // Complete a chosen retreat even if a short damage/reload timer expires
 // during the walk. Return to offense once the protected position is reached.
 defensive|=mode==2&&get("guard_cover")&&!protectedNow;
 if(defensive){
  if(mode!=2){put("guard_cover",0);put("guard_cover_wait",0);}put("guard_cover_mode",2);
  if(protectedNow){put("guard_cover",0);put("guard_cover_wait",0);wantsMove=false;return;}
  int wait=std::max(0,get("guard_cover_wait")-ms);Vec2 cached{get("guard_cover_x")*.001f,get("guard_cover_y")*.001f};
  if(get("guard_cover")){auto trial=pose;trial.position=cached;trial.z=get("guard_cover_z")*.001f;if(lengthSq(cached-pose.position)<.04f||!selected.valid()||!guardCovered(actor,trial,selected)||guardPositionReserved(actor,cached,trial.z)){put("guard_cover",0);wait=0;}}
  if(wait==0&&m_npcNavBudget>0){
   --m_npcNavBudget;put("guard_cover",0);Vec2 toward=normalized(Vec2{target.x,target.y}-pose.position),side=preferredSide(toward);
   const std::array<Vec2,8> offsets={toward*(-.75f),toward*(-1.5f),side*.75f,side*(-.75f),toward*(-.65f)+side*1.2f,toward*(-.65f)-side*1.2f,side*2.f,side*(-2.f)};
   float best=10000;Vec2 destination{};float destinationZ=pose.z;
   for(auto offset:offsets){auto candidate=pose.position+offset;if(candidate.x<.35f||candidate.y<.35f||candidate.x>23.65f||candidate.y>23.65f)continue;
    float feet=pose.z;if(!friendlyWalkSegment(level,pose.position,pose.z,candidate,track.scale,&feet)||guardPositionReserved(actor,candidate,feet))continue;
    auto trial=pose;trial.position=candidate;trial.z=feet;auto facing=Vec2{target.x,target.y}-candidate;trial.yaw=std::atan2(facing.x,facing.y);
    if(!selected.valid()||!guardCovered(actor,trial,selected))continue;float score=lengthSq(offset);if(score>=best)continue;best=score;destination=candidate;destinationZ=feet;
   }
   if(best<10000){put("guard_cover",1);put("guard_cover_x",int(std::round(destination.x*1000)));put("guard_cover_y",int(std::round(destination.y*1000)));put("guard_cover_z",int(std::round(destinationZ*1000)));cached=destination;}
   wait=600+int(actor%7)*30;
  }
  put("guard_cover_wait",wait);wantsMove=get("guard_cover")!=0;if(wantsMove){goal=cached;goalZ=get("guard_cover_z")*.001f;}return;
 }
 if(mode==2){put("guard_cover",0);put("guard_cover_wait",0);}put("guard_cover_mode",0);
 if(guardMeleeReady(actor,pose,selected)){put("guard_cover",0);put("guard_cover_wait",0);wantsMove=false;return;}
 bool flank=role!=0&&!get("guard_flank_done");
 if(flank){
  // A selected target change invalidates the previous encounter's destination
  // before combat updates its stored weapon target identity later this frame.
  bool changed=selected.valid()&&(get("weapon_target_kind")!=int(selected.kind)||get("weapon_target_level")!=selected.level+1||get("weapon_target_index")!=selected.index+1);
  if(changed){put("guard_cover",0);put("guard_cover_wait",0);put("guard_flank_wait",0);}
  if(get("guard_cover")){
   Vec2 cached{get("guard_cover_x")*.001f,get("guard_cover_y")*.001f};float feet=get("guard_cover_z")*.001f;
   int remaining=std::max(0,get("guard_flank_wait")-ms);put("guard_flank_wait",remaining);
   if(lengthSq(cached-pose.position)<.04f&&std::fabs(feet-pose.z)<.3f){put("guard_flank_done",1);put("guard_flank_wait",0);put("guard_cover",0);put("guard_cover_wait",0);flank=false;}
   else{
    float endFeet=pose.z;
    bool reachable=remaining>0&&!guardPositionReserved(actor,cached,feet)&&friendlyWalkSegment(level,pose.position,pose.z,cached,track.scale,&endFeet)&&std::fabs(endFeet-feet)<.3f;
    if(reachable){put("guard_cover_mode",1);goal=cached;goalZ=feet;wantsMove=true;return;}
    // A closed doorway, newly occupied endpoint or stalled movement must not
    // permanently suppress the guard's firing opportunity after this flank.
    put("guard_flank_done",1);put("guard_flank_wait",0);put("guard_cover",0);put("guard_cover_mode",0);put("guard_cover_wait",600+int(actor%7)*30);flank=false;
   }
  }
  if(flank){
   int wait=std::max(0,get("guard_cover_wait")-ms);
   if(wait==0&&m_npcNavBudget>0){
    --m_npcNavBudget;Vec2 toward=normalized(Vec2{target.x,target.y}-pose.position),side=preferredSide(toward);
    const std::array<Vec2,8> offsets={side*1.5f,side*.75f,side*2.f,toward*.65f+side*1.2f,side*1.5f-toward*.5f,side*.75f-toward*.75f,side*(-.75f),side*(-1.5f)};
    bool found=false;
    for(auto offset:offsets){auto candidate=pose.position+offset;if(candidate.x<.35f||candidate.y<.35f||candidate.x>23.65f||candidate.y>23.65f)continue;
     float feet=pose.z;if(!friendlyWalkSegment(level,pose.position,pose.z,candidate,track.scale,&feet)||guardPositionReserved(actor,candidate,feet))continue;
     auto trial=pose;trial.position=candidate;trial.z=feet;auto facing=Vec2{target.x,target.y}-candidate;trial.yaw=std::atan2(facing.x,facing.y);
     if(!guardShotMatches(actor,guardShot(actor,trial,npcWeaponPoseAt(actor,trial,target,1))))continue;
     put("guard_cover",1);put("guard_cover_mode",1);put("guard_cover_x",int(std::round(candidate.x*1000)));put("guard_cover_y",int(std::round(candidate.y*1000)));put("guard_cover_z",int(std::round(feet*1000)));goal=candidate;goalZ=feet;found=true;break;
    }
    if(found){put("guard_flank_wait",5000);put("guard_cover_wait",600+int(actor%7)*30);wantsMove=true;return;}
    // A blocked room should not leave a flanker repeatedly orbiting or
    // monopolizing the four-search budget. It can use normal firing tactics.
    put("guard_flank_done",1);put("guard_flank_wait",0);flank=false;wait=600+int(actor%7)*30;
   }
   put("guard_cover_wait",wait);if(flank){wantsMove=false;return;}
  }
 }
 // Evaluate the fully raised weapon. Raising/aiming still happens separately,
 // so a low-ready barrel does not make the guard shuffle before every shot.
 if(guardShotMatches(actor,guardShot(actor,pose,npcWeaponPoseAt(actor,pose,target,1)))){put("guard_cover",0);put("guard_cover_wait",0);wantsMove=false;return;}
 int wait=std::max(0,get("guard_cover_wait")-ms);
 Vec2 cached{get("guard_cover_x")*.001f,get("guard_cover_y")*.001f};
 if(get("guard_cover")&&guardPositionReserved(actor,cached,get("guard_cover_z")*.001f)){put("guard_cover",0);wait=0;}
 if(get("guard_cover")&&lengthSq(cached-pose.position)<.04f)wait=0;
 if(wait==0&&m_npcNavBudget>0){
  --m_npcNavBudget;put("guard_cover",0);
  Vec2 toward=normalized(Vec2{target.x,target.y}-pose.position),side=preferredSide(toward);
  const std::array<Vec2,8> offsets={side*.75f,side*(-.75f),side*1.5f,side*(-1.5f),side*2.f,side*(-2.f),toward*.65f+side*1.2f,toward*.65f-side*1.2f};
  float best=10000;Vec2 destination{};float destinationZ=pose.z;
  for(auto offset:offsets){auto candidate=pose.position+offset;
   if(candidate.x<.35f||candidate.y<.35f||candidate.x>23.65f||candidate.y>23.65f)continue;
   float feet=pose.z;if(!friendlyWalkSegment(level,pose.position,pose.z,candidate,track.scale,&feet)||guardPositionReserved(actor,candidate,feet))continue;
   auto trial=pose;trial.position=candidate;trial.z=feet;auto facing=Vec2{target.x,target.y}-candidate;trial.yaw=std::atan2(facing.x,facing.y);
   if(!guardShotMatches(actor,guardShot(actor,trial,npcWeaponPoseAt(actor,trial,target,1))))continue;
   float score=lengthSq(offset);if(score>=best)continue;best=score;destination=candidate;destinationZ=feet;
  }
  if(best<10000){put("guard_cover",1);put("guard_cover_mode",1);put("guard_cover_x",int(std::round(destination.x*1000)));put("guard_cover_y",int(std::round(destination.y*1000)));put("guard_cover_z",int(std::round(destinationZ*1000)));cached=destination;}
  wait=600+int(actor%7)*30;
 }
 put("guard_cover_wait",wait);
 if(get("guard_cover")){put("guard_cover_mode",1);goal=cached;goalZ=get("guard_cover_z")*.001f;}
 // With no direct sidestep, the normal collision-aware navigator can approach
 // the remembered target around larger obstacles. It cannot force a locked door.
 wantsMove=true;
}
}

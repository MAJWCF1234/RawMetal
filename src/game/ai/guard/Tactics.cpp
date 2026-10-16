#include "../../Game.h"
namespace retro {
void Game::guardReposition(int level,size_t actor,const ActorTrack& track,const ActorPose& pose,Point3 target,int ms,Vec2& goal,float& goalZ,bool& wantsMove){
 auto ctx=makeGuardCtx(actor,level,track,const_cast<ActorPose&>(pose));
 auto get=[&](const char* field){return ctx.get(field);};
 auto put=[&](const char* field,int value){ctx.put(field,value);};
 auto selected=guardTarget(actor);if(selected.valid())target={selected.position.x,selected.position.y,selected.feet+selected.height*.6f};
 int role=guardFlankSide(actor);
 Vec2 towardThreat=normalized(Vec2{target.x,target.y}-pose.position);if(lengthSq(towardThreat)<.001f)towardThreat={0,1};
 Vec2 left{-towardThreat.y,towardThreat.x},right=left*(-1.f);
 float leftClear=guardLateralClearance(level,actor,pose,left),rightClear=guardLateralClearance(level,actor,pose,right);
 // Role/rank picks a side; corridors flip to whichever side still has room.
 float sideSign=role?float(-role):(actor%2?-1.f:1.f);
 if(sideSign>0&&rightClear<.55f&&leftClear>rightClear+.15f)sideSign=-1.f;
 if(sideSign<0&&leftClear<.55f&&rightClear>leftClear+.15f)sideSign=1.f;
 float room=std::max(leftClear,rightClear);
 float stepScale=std::clamp(room>0.f?room/1.5f:.55f,.45f,1.35f);
 auto preferredSide=[&](){Vec2 side=left;if(sideSign)side=side*sideSign;else if(actor%2)side=side*(-1);return side;};
 int damage=track.damageState?state(track.damageState):0;bool lowHealth=track.health>0&&track.health-damage<=track.health*.35f;
 bool defensive=get("weapon_reload")>0||get("weapon_reloading")||(get("weapon_saved")&&get("weapon_rounds")==0)||get("guard_under_fire")>0||lowHealth;
 int mode=ctx.coverMode();
 bool protectedNow=selected.valid()&&guardCovered(actor,pose,selected);
 // Complete a chosen retreat even if a short damage/reload timer expires
 // during the walk. Return to offense once the protected position is reached.
 defensive|=mode==2&&ctx.hasCoverGoal()&&!protectedNow;
 if(defensive){
  if(mode!=2)ctx.clearCoverGoal();put("guard_cover_mode",2);
  if(protectedNow){ctx.clearCoverGoal();wantsMove=false;return;}
  int wait=std::max(0,get("guard_cover_wait")-ms);Vec2 cached=ctx.coverGoal();
  if(ctx.hasCoverGoal()){auto trial=pose;trial.position=cached;trial.z=ctx.coverFeet();if(lengthSq(cached-pose.position)<.04f||!selected.valid()||!guardCovered(actor,trial,selected)||guardPositionReserved(actor,cached,trial.z)){ctx.clearCoverGoal();wait=0;}}
  if(wait==0){
   if(m_npcNavBudget>0){
    --m_npcNavBudget;ctx.clearCoverGoal();Vec2 destination{};float destinationZ=pose.z;
    bool found=findGuardCoverFromEnemy(level,actor,pose,target,destination,destinationZ,sideSign);
    if(found){ctx.setCoverGoal(destination,destinationZ,2);cached=destination;}
   }
   // Budget starvation must still back off; otherwise wait stays 0 and a
   // defensive actor with no cover goal stands still forever.
   wait=600+int(actor%7)*30;
  }
  put("guard_cover_wait",wait);wantsMove=ctx.hasCoverGoal();if(wantsMove){goal=cached;goalZ=ctx.coverFeet();}return;
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
    bool reserved=guardPositionReserved(actor,cached,feet);
    bool direct=remaining>0&&!reserved&&friendlyWalkSegment(level,pose.position,pose.z,cached,track.scale,&endFeet)&&std::fabs(endFeet-feet)<.3f;
    if(direct){put("guard_cover_mode",1);goal=cached;goalZ=feet;wantsMove=true;return;}
    // Door/body can block the straight span — hop the retained graph instead
    // of abandoning the flank until the wait actually expires.
    if(remaining>0&&!reserved){
     auto hop=guardGraphWaypoint(level,pose.position,pose.z,cached,feet,track.scale);
     if(lengthSq(hop-pose.position)>.04f){put("guard_cover_mode",1);goal=hop;goalZ=pose.z;wantsMove=true;return;}
     wantsMove=false;return;
    }
    // Wait elapsed with no path: release the flank so fire leases can resume.
    put("guard_flank_done",1);put("guard_flank_wait",0);put("guard_cover",0);put("guard_cover_mode",0);put("guard_cover_wait",600+int(actor%7)*30);flank=false;
   }
  }
  if(flank){
   int wait=std::max(0,get("guard_cover_wait")-ms);
   if(wait==0){
    if(m_npcNavBudget>0){
     --m_npcNavBudget;Vec2 side=preferredSide();
     Vec2 destination{};float destinationZ=pose.z;bool found=findGuardCombatPost(level,actor,pose,target,destination,destinationZ,sideSign);
     if(!found){
      const std::array<Vec2,8> offsets={side*1.5f*stepScale,side*.75f*stepScale,side*2.f*stepScale,towardThreat*.65f+side*1.2f*stepScale,side*1.5f*stepScale-towardThreat*.5f,side*.75f*stepScale-towardThreat*.75f,side*(-.75f*stepScale),side*(-1.5f*stepScale)};
      for(auto offset:offsets){auto candidate=pose.position+offset;if(candidate.x<.35f||candidate.y<.35f||candidate.x>23.65f||candidate.y>23.65f)continue;
       float feet=pose.z;if(!friendlyWalkSegment(level,pose.position,pose.z,candidate,track.scale,&feet)||guardPositionReserved(actor,candidate,feet))continue;
       auto trial=pose;trial.position=candidate;trial.z=feet;auto facing=Vec2{target.x,target.y}-candidate;trial.yaw=std::atan2(facing.x,facing.y);
       if(!guardShotMatches(actor,guardShot(actor,trial,npcWeaponPoseAt(actor,trial,target,1))))continue;
       destination=candidate;destinationZ=feet;found=true;break;
      }
     }
     if(found){put("guard_cover",1);put("guard_cover_mode",1);put("guard_cover_x",int(std::round(destination.x*1000)));put("guard_cover_y",int(std::round(destination.y*1000)));put("guard_cover_z",int(std::round(destinationZ*1000)));put("guard_flank_wait",5000);put("guard_cover_wait",600+int(actor%7)*30);goal=destination;goalZ=destinationZ;wantsMove=true;return;}
    }
    // Blocked room or a starved search frame must not mute fire forever while
    // pendingFlank keeps wantsMove false. Same backoff either way.
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
 if(wait==0){
  if(m_npcNavBudget>0){
   --m_npcNavBudget;put("guard_cover",0);
   Vec2 side=preferredSide();Vec2 destination{};float destinationZ=pose.z;bool found=findGuardCombatPost(level,actor,pose,target,destination,destinationZ,sideSign);
   float best=found?lengthSq(destination-pose.position):10000;
   if(found)cached=destination;
   const std::array<Vec2,8> offsets={side*.75f*stepScale,side*(-.75f*stepScale),side*1.5f*stepScale,side*(-1.5f*stepScale),side*2.f*stepScale,side*(-2.f*stepScale),towardThreat*.65f+side*1.2f*stepScale,towardThreat*.65f-side*1.2f*stepScale};
   for(auto offset:offsets){auto candidate=pose.position+offset;
    if(candidate.x<.35f||candidate.y<.35f||candidate.x>23.65f||candidate.y>23.65f)continue;
    float feet=pose.z;if(!friendlyWalkSegment(level,pose.position,pose.z,candidate,track.scale,&feet)||guardPositionReserved(actor,candidate,feet))continue;
    auto trial=pose;trial.position=candidate;trial.z=feet;auto facing=Vec2{target.x,target.y}-candidate;trial.yaw=std::atan2(facing.x,facing.y);
    if(!guardShotMatches(actor,guardShot(actor,trial,npcWeaponPoseAt(actor,trial,target,1))))continue;
    float score=lengthSq(offset);if(score>=best)continue;best=score;destination=candidate;destinationZ=feet;found=true;cached=destination;
   }
   if(found){put("guard_cover",1);put("guard_cover_mode",1);put("guard_cover_x",int(std::round(cached.x*1000)));put("guard_cover_y",int(std::round(cached.y*1000)));put("guard_cover_z",int(std::round(destinationZ*1000)));}
  }
  // Starved frames still back off so every role keeps trying locomotion toward
  // the remembered target instead of spinning on wait==0 with no search.
  wait=600+int(actor%7)*30;
 }
 put("guard_cover_wait",wait);
 if(get("guard_cover")){put("guard_cover_mode",1);goal=cached;goalZ=get("guard_cover_z")*.001f;}
 // With no direct sidestep, the normal collision-aware navigator can approach
 // the remembered target around larger obstacles. It cannot force a locked door.
 wantsMove=true;
}
}

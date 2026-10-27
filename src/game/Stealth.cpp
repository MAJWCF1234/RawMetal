#include "Game.h"
#include "../world/CustomCampaign.h"
#include "../world/ModelIds.h"
#include <fstream>
#include <filesystem>
#include <tuple>

namespace retro {
void Game::updateExposure(float dt){
 if(hiddenLocker()>=0){m_playerExposure=0;return;} // no lamp reaches inside a shut locker
 if((m_exposureTimer-=dt)>0)return;m_exposureTimer=.1f;
 const Vec2 at=m_player.pos;const float chest=m_player.z+m_player.hullHeight()*.6f;
 float lit=m_world.definition().ambient*.8f;
 for(const auto& lamp:m_world.lights()){float power=lampPower(lamp);if(power<=0)continue;
  float dz=lamp.z-chest,distance=std::sqrt(lengthSq(lamp.position-at)+dz*dz),range=std::max(1.f,lamp.range);if(distance>=range)continue;
  float falloff=1-distance/range,share=power*lamp.intensity*falloff*falloff;if(share<.02f)continue;
  if(m_world.lightRayClear(lamp.position,lamp.z-.05f,at,chest))lit+=share;
 }
 lit*=1-m_darkness;if(flashlightOn())lit=std::max(lit,1.f);
 m_playerExposure=std::clamp(lit,0.f,1.f);
}
void Game::knockOnWall(){
 if(m_elapsed-m_lastKnock<.6f||!m_player.grounded)return;
 Vec2 ahead{std::cos(m_player.angle),std::sin(m_player.angle)};const float hand=m_player.z+m_player.hullHeight()*.7f;
 for(float reach=.35f;reach<=.85f;reach+=.1f){Vec2 p=m_player.pos+ahead*reach;
  if(m_world.fits(p.x,p.y,hand-.05f,.1f,false))continue;
  m_lastKnock=m_elapsed;m_sounds.push_back({Sound::Land,p,.7f,1.35f,true,12,hand});
  return;
 }
}
// An unconscious guard lying within reach, to drag out of sight.
int Game::nearbyBody()const{
 int chosen=-1;float nearest=1.4f;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto& npc=m_friendlyActors[actor];
  if(npc.level!=m_level||state(friendlyKey(actor,"restraint"))!=1||npc.pose.clip!=4||(npc.track().deadState&&state(npc.track().deadState))||!humanActorVisual(npc.track().visual))continue;
  float distance=length(npc.pose.position-m_player.pos);if(distance<nearest&&std::fabs(npc.pose.z-m_player.z)<.5f){nearest=distance;chosen=int(actor);}
 }return chosen;
}
void Game::dropBody(){if(m_draggedNpc<0)return;if(size_t(m_draggedNpc)<m_friendlyActors.size())saveRestrainedPose(size_t(m_draggedNpc));m_draggedNpc=-1;m_pickupNotice="BODY LEFT";m_pickupNoticeTime=1.2f;}
float Game::guardAwareness(size_t actor)const{
 if(actor>=m_friendlyActors.size()||npcIncapacitated(actor))return 0;const auto& npc=m_friendlyActors[actor];
 if(npc.track().ai.mode!=ActorAiMode::Guard||npc.pose.clip==4||(npc.track().deadState&&state(npc.track().deadState)))return 0;
 auto mode=guardState(actor);if(mode==GuardState::Alert)return 2;
 if(mode==GuardState::Suspicious)return std::clamp(state(friendlyKey(actor,"guard_recognition"))/float(std::max(1,npc.track().ai.recognitionMs)),.05f,1.f);
 if(mode==GuardState::Investigate||mode==GuardState::Search)return .5f;
 return 0;
}
bool Game::npcIncapacitated(size_t actor)const{return actor<m_friendlyActors.size()&&state(friendlyKey(actor,"restraint"))!=0;}
bool Game::npcHeld(size_t actor)const{return actor<m_friendlyActors.size()&&state(friendlyKey(actor,"restraint"))==2;}
int Game::heldNpc()const{for(size_t i=0;i<m_friendlyActors.size();++i)if(npcHeld(i))return int(i);return -1;}
bool Game::canTakedown(size_t actor)const{
 if(actor>=m_friendlyActors.size()||dead()||holdingClutter()||hiddenLocker()>=0)return false;
 const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();
 if(npc.level!=m_level||t.ai.mode!=ActorAiMode::Guard||!humanActorVisual(t.visual)||!t.health||npc.pose.clip==4||npcIncapacitated(actor)||state(t.deadState)||(t.ai.enableState&&!state(t.ai.enableState))||!guardHostileToPlayer(actor))return false;
 if(guardState(actor)==GuardState::Alert||state(friendlyKey(actor,"guard_confirmed"))||state(friendlyKey(actor,"guard_under_fire")))return false;
 auto delta=npc.pose.position-m_player.pos;float distance=length(delta);
 Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)},facing{std::sin(npc.pose.yaw),std::cos(npc.pose.yaw)};
 if(distance<.25f||distance>1.25f||std::fabs(npc.pose.z-m_player.z)>.5f||dot(normalized(delta),forward)<.72f||dot(normalized(delta)*-1,facing)>-.35f)return false;
 Point3 from{m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye},to{npc.pose.position.x,npc.pose.position.y,npc.pose.z+t.scale*.55f},ray=to-from;
 float range=std::sqrt(ray.x*ray.x+ray.y*ray.y+ray.z*ray.z);auto hit=traceShot(from,ray,range+.05f);
 return hit.kind==ShotKind::Npc&&hit.index==int(actor);
}
int Game::nearbyTakedown()const{
 int chosen=-1;float nearest=1.26f;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(canTakedown(actor)){float distance=length(m_friendlyActors[actor].pose.position-m_player.pos);if(distance<nearest){chosen=int(actor);nearest=distance;}}
 return chosen;
}
void Game::saveRestrainedPose(size_t actor){
 auto&npc=m_friendlyActors[actor];auto put=[&](const char*name,int value){setState(friendlyKey(actor,name),value);};
 put("saved",1);put("running",0);put("walking",0);put("crouched",0);put("chunk",npc.level+1);
 put("x",int(std::round(npc.pose.position.x*1000)));put("y",int(std::round(npc.pose.position.y*1000)));put("z",int(std::round(npc.pose.z*1000)));
 put("yaw",int(std::round(std::remainder(npc.pose.yaw,2*kPi)*10000)));put("phase",int(std::round(npc.pose.phase*10000)));
 auto projected=npc.pose;projected.position+=chunkOffset(npc.level)-chunkOffset(npc.homeLevel);friendlyWorld(npc.homeLevel).setActorPose(npc.homeActor,projected);
}
void Game::restrainNpc(size_t actor,bool held){
 if(!canTakedown(actor)||heldNpc()>=0||m_takedownActor>=0)return;
 auto&npc=m_friendlyActors[actor];clearGuardSchedule(actor);
 setState(friendlyKey(actor,"restraint"),held?2:1);setState(friendlyKey(actor,"restraint_ms"),0);setState(friendlyKey(actor,"restraint_fraction"),0);
 for(const char*field:{"weapon_aim","weapon_recovery","weapon_bash","weapon_bash_pose","grenade_windup","support_ms","drone_windup","drone_busy","engineer_pad_stage","engineer_pad_ms","guard_seen","guard_slot"})setState(friendlyKey(actor,field),0);
 if(held)setState(friendlyKey(actor,"restraint_weapon"),m_weaponEquipped?int(m_equippedWeapon):0);
 equipWeapon(WeaponId::Fists);npc.pose.lookAtActor=-1;
 if(held){
  npc.pose.clip=0;npc.pose.phase=0;saveRestrainedPose(actor);
  m_punchAge=10;m_shotCooldown=.5f;m_suppressFire=true;sound(Sound::PunchHit,.30f,.8f);
  m_pickupNotice="GUARD HELD / KEEP THEM BETWEEN YOU AND FIRE";m_pickupNoticeTime=2;
 }else{
  // Standing rear choke: wrap the neck, then drop them. Do not fire the jab
  // animation — crouched punches read as ankle hits. The struggle is a real
  // spatial impact so nearby detail can investigate before the body is found.
  if(m_player.crouched){float feet=groundHeight(m_player.pos);if(hullFits(m_player.pos,feet,Player::StandingHeight)){m_player.crouched=false;m_player.z=feet;}}
  m_player.pitch*=.2f;m_takedownActor=int(actor);m_takedownAge=0;
  npc.pose.clip=0;npc.pose.phase=0;saveRestrainedPose(actor);
  m_punchAge=10;m_shotCooldown=.55f;m_suppressFire=true;
  // A choke is a scuffle, not a punch: heard by a man a room's width away
  // (about 6.7 m in the open, 2.3 m through a wall), not across the yard.
  float neck=npc.pose.z+npc.track().scale*.85f;m_sounds.push_back({Sound::PunchHit,npc.pose.position,.45f,.72f,true,14,neck});
  m_pickupNotice="GUARD UNCONSCIOUS";m_pickupNoticeTime=2;
 }
 updateGuardFlags();
}
void Game::updateTakedown(float dt){
 if(m_takedownActor<0)return;
 if(size_t(m_takedownActor)>=m_friendlyActors.size()||dead()){m_takedownActor=-1;m_takedownAge=0;return;}
 auto&npc=m_friendlyActors[size_t(m_takedownActor)];
 if(npc.level!=m_level||state(npc.track().deadState)||!npcIncapacitated(size_t(m_takedownActor))||npcHeld(size_t(m_takedownActor))){m_takedownActor=-1;m_takedownAge=0;return;}
 m_takedownAge+=std::max(0.f,dt);
 if(m_player.crouched){float feet=groundHeight(m_player.pos);if(hullFits(m_player.pos,feet,Player::StandingHeight)){m_player.crouched=false;m_player.z=feet;}}
 m_player.pitch+=(0.f-m_player.pitch)*std::min(1.f,dt*10.f);
 // Keep them upright and facing away while the arms lock around the neck.
 setState(friendlyKey(size_t(m_takedownActor),"restraint_ms"),0);setState(friendlyKey(size_t(m_takedownActor),"restraint_fraction"),0);
 npc.pose.yaw=wrapAngle(kPi*.5f-m_player.angle);npc.pose.clip=0;npc.pose.phase=0;npc.pose.lookAtActor=-1;
 if(m_takedownAge>=.42f){
  npc.pose.clip=4;npc.pose.phase=0;saveRestrainedPose(size_t(m_takedownActor));
  m_sounds.push_back({Sound::Land,npc.pose.position,.55f,.88f,true,10,npc.pose.z+.04f});
  m_takedownActor=-1;m_takedownAge=0;
 }else saveRestrainedPose(size_t(m_takedownActor));
}
void Game::releaseNpc(bool unconscious){
 int index=heldNpc();if(index<0)return;size_t actor=size_t(index);auto&npc=m_friendlyActors[actor];
 int weapon=state(friendlyKey(actor,"restraint_weapon"));setState(friendlyKey(actor,"restraint_weapon"),0);
 setState(friendlyKey(actor,"restraint"),unconscious?1:0);setState(friendlyKey(actor,"restraint_ms"),0);setState(friendlyKey(actor,"restraint_fraction"),0);
 npc.pose.clip=unconscious?4:0;npc.pose.phase=0;saveRestrainedPose(actor);
 if(!unconscious&&!state(npc.track().deadState)){
  rememberGuardTarget(actor,{ShotKind::Player,m_level,0});auto global=m_player.pos+chunkOffset(m_level);
  setState(friendlyKey(actor,"guard_x"),int(std::round(global.x*1000)));setState(friendlyKey(actor,"guard_y"),int(std::round(global.y*1000)));setState(friendlyKey(actor,"guard_z"),int(std::round(m_player.z*1000)));
  setState(friendlyKey(actor,"guard_memory"),npc.track().ai.searchMs);setState(friendlyKey(actor,"guard_confirmed"),1);setState(friendlyKey(actor,"guard_recognition"),npc.track().ai.recognitionMs);setState(friendlyKey(actor,"guard_mode"),int(GuardState::Investigate));
 }
 if(weapon>0&&weapon<weaponCount())equipWeapon(WeaponId(weapon));
 m_pickupNotice=unconscious?"GUARD UNCONSCIOUS":"GUARD RELEASED / THEY KNOW YOU ARE HERE";m_pickupNoticeTime=2;m_suppressFire=true;
}
void Game::updateStealth(const InputState&input,float){
 bool subdue=input.subdue&&!m_previousSubdue;m_previousSubdue=input.subdue;
 if(updateLocker(input))return;
 if(input.vrTracked){if(heldNpc()>=0)releaseNpc(true);return;}
 if(m_draggedNpc>=0){
  if(size_t(m_draggedNpc)>=m_friendlyActors.size()){m_draggedNpc=-1;}
  else{auto& body=m_friendlyActors[size_t(m_draggedNpc)];
   if(dead()||body.level!=m_level||state(friendlyKey(size_t(m_draggedNpc),"restraint"))!=1||(body.track().deadState&&state(body.track().deadState))||input.jump||!m_player.grounded||(input.fire&&!m_previousFire)||subdue){dropBody();}
   else{Vec2 back{-std::cos(m_player.angle),-std::sin(m_player.angle)},target=m_player.pos+back*.85f;float feet=groundHeight(target);auto old=body.pose.position;
    bool clear=target.x>=.25f&&target.y>=.25f&&target.x<23.75f&&target.y<23.75f&&std::fabs(feet-m_player.z)<.28f&&friendlyWalkSegment(m_level,old,body.pose.z,target,.45f);
    if(clear){body.pose.position=target;body.pose.z=feet;body.pose.yaw=wrapAngle(kPi*.5f-m_player.angle);}
    body.pose.clip=4;body.pose.phase=1;saveRestrainedPose(size_t(m_draggedNpc));
    if(length(body.pose.position-m_player.pos)>1.6f)dropBody();
   }}
 }
 int index=heldNpc();
 if(index>=0){
  auto&npc=m_friendlyActors[size_t(index)];
  if(dead()||npc.level!=m_level||state(npc.track().deadState)||input.jump||!m_player.grounded){releaseNpc(true);return;}
  if(subdue){releaseNpc(true);m_punchAge=10;sound(Sound::PunchHit,.3f,.8f);return;}
  if(input.fire&&!m_previousFire&&!m_suppressFire){size_t actor=size_t(index);releaseNpc(true);damageFriendly(actor,float(npc.track().health),m_player.pos,{ShotKind::Player,m_level,0});m_punchAge=10;return;}
  Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)},target=m_player.pos+forward*.62f;
  float feet=groundHeight(target);auto old=npc.pose.position;
  bool clear=target.x>=.25f&&target.y>=.25f&&target.x<23.75f&&target.y<23.75f&&std::fabs(feet-m_player.z)<.28f&&friendlyWalkSegment(m_level,old,npc.pose.z,target,npc.track().scale)&&
   m_world.rayClear(old,npc.pose.z+npc.track().scale*.55f,target,feet+npc.track().scale*.55f,true,true,true)&&
   m_world.rayClear(m_player.pos,m_player.z+m_player.eye,target,feet+npc.track().scale*.55f,true,true,true);
  int samples=std::max(1,int(std::ceil(length(target-old)/.1f)));
  for(int i=1;clear&&i<=samples;++i){auto at=old+(target-old)*(float(i)/samples);clear=hullFits(at,groundHeight(at),npc.track().scale);}
  if(!clear){releaseNpc(true);return;}
  npc.pose.position=target;npc.pose.z=feet;npc.pose.yaw=wrapAngle(kPi*.5f-m_player.angle);npc.pose.clip=0;npc.pose.phase=0;saveRestrainedPose(size_t(index));
  equipWeapon(WeaponId::Fists);m_punchAge=10;m_shotCooldown=.25f;
 }else if(subdue){int target=nearbyTakedown();if(target>=0)restrainNpc(size_t(target),false);else{m_pickupNotice="GET CLOSE BEHIND AN UNAWARE GUARD";m_pickupNoticeTime=1.5f;}}
}
// --- Lockers -----------------------------------------------------------------
// Any solid steel locker bank whose doors (the model's local +x) open onto
// floor a body can stand on is somewhere to hide. Shut in, the player looks
// out through the vent slats: no steps, no light on them, and nobody outside
// sees in. A guard who watched them climb in knows which locker and comes to
// open it; nobody else ever looks inside one.
namespace {
constexpr StateId PlayerLocker=stateId("player_locker"); // the fixture shut in, +1
// Metres out from the doors: where the eye sits, where the player steps out
// to, and where a guard who saw them go in stands to open it.
constexpr float LockerEye=.06f,LockerStep=.32f,LockerStand=1.1f;
// How far the head turns behind the slats: about thirty degrees either way.
constexpr float LockerYaw=.55f,LockerPitch=45.f;
Vec2 lockerOut(const Fixture& f){return {std::cos(f.yaw),-std::sin(f.yaw)};}
Vec2 lockerFront(const Fixture& f,float ahead){return f.position+lockerOut(f)*(f.width*.5f+ahead);}
bool lockerModel(const Fixture& f){return f.model==model::pwr_locker_bank&&f.solid&&f.visible;}
}
int Game::hiddenLocker()const{
 const int index=state(PlayerLocker)-1;if(index<0||dead()||size_t(index)>=m_world.fixtures().size())return -1;const auto&f=m_world.fixtures()[size_t(index)];
 return lockerModel(f)&&lengthSq(m_player.pos-lockerFront(f,LockerEye))<.15f*.15f?index:-1;
}
// A standing spot in front of the doors, reached from them without passing
// through anything: straight out first, then a little to either side.
// Squeezing, when someone stands in all of those, it takes the nearest clear
// spot out to a metre and a half in front, so nobody is ever boxed in.
bool Game::lockerExit(const Fixture& f,Vec2& at,float& feet,bool squeeze)const{
 const Vec2 out=lockerOut(f),side{-out.y,out.x},mouth=lockerFront(f,0);const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 // Hull samples 20 cm apart can straddle a thin partition, and walking never
 // gets there but a step out could: so the ray from the doors runs to the far
 // side of the body, and the body is felt out every few centimetres from its
 // middle to each sample.
 auto whole=[&](Vec2 p,float z){for(float x:{-1.f,0.f,1.f})for(float y:{-1.f,0.f,1.f})for(float t:{.2f,.4f,.6f,.8f}){if(x==0&&y==0)continue;Vec2 q=p+Vec2{x,y}*(.2f*t);const auto&w=worldAt(q);
  if(!w.fits(q.x,q.y,z,Player::StandingHeight)||w.doorBlocks(q.x,q.y,z,Player::StandingHeight))return false;}return true;};
 auto clear=[&](Vec2 p){const float z=groundHeight(p,floor+.1f);
  if(std::fabs(z-floor)<.25f&&hullFits(p,z,Player::StandingHeight)&&m_world.rayClear(lockerFront(f,.03f),floor+1.f,p+out*.21f,z+1.f,true)&&whole(p,z)){at=p;feet=z;return true;}return false;};
 for(float ahead:{LockerStep,.45f,.6f})for(float across:{0.f,.42f,-.42f})if(clear(lockerFront(f,ahead)+side*across))return true;
 if(squeeze)for(int ring=3;ring<=15;++ring)for(int k=0;k<13;++k){const float turn=float((k+1)/2)*(k%2?1.f:-1.f)*kPi/12;const Vec2 p=mouth+(out*std::cos(turn)+side*std::sin(turn))*(ring*.1f);
  if(dot(p-mouth,out)>=.21f&&clear(p))return true;}
 return false;
}
int Game::lockerAhead()const{
 // A Snatcher's grip is struggled out of, not slipped into a locker.
 if(m_vrInputActive||dead()||draggingBody()||heldNpc()>=0||holdingClutter()||takedownActive()||m_grabbed>0||!m_player.grounded||hiddenLocker()>=0||nearbyBody()>=0)return -1;
 const Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};int best=-1;float nearest=1.05f;const auto&fixtures=m_world.fixtures();
 for(size_t i=0;i<fixtures.size();++i){const auto&f=fixtures[i];if(!lockerModel(f))continue;
  const Vec2 out=lockerOut(f),side{-out.y,out.x},delta=m_player.pos-f.position;const float ahead=dot(delta,out)-f.width*.5f;
  if(ahead<.1f||ahead>=nearest||std::fabs(dot(delta,side))>f.depth*.5f+.1f||dot(forward,out)>-.55f)continue;
  const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;Vec2 exit;float feet;
  if(std::fabs(m_player.z-floor)>.3f||!m_world.rayClear(m_player.pos,m_player.z+1.f,lockerFront(f,.03f),floor+1.f,true)||!lockerExit(f,exit,feet))continue;
  best=int(i);nearest=ahead;
 }return best;
}
bool Game::enterLocker(){
 const int index=lockerAhead();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];
 const Vec2 out=lockerOut(f),stand=lockerFront(f,LockerStand)+chunkOffset(m_level);const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 // Whoever has eyes on the player right now sees which locker.
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();
  if(t.ai.mode!=ActorAiMode::Guard||!autonomousActor(t)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||!guardHostileToPlayer(actor))continue;
  const auto me=resolveGuardTarget(actor,{ShotKind::Player,m_level,0});if(!guardCanSee(actor,npc.pose,me))continue;
  auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};rememberGuardTarget(actor,me);
  put("guard_x",int(std::round(stand.x*1000)));put("guard_y",int(std::round(stand.y*1000)));put("guard_z",int(std::round(floor*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);put("guard_locker",index+1);
  if(guardState(actor)!=GuardState::Alert)put("guard_mode",int(GuardState::Investigate));
 }
 setState(PlayerLocker,index+1);
 m_player.pos=lockerFront(f,LockerEye);m_player.z=floor;m_player.crouched=false;m_player.eye=Player::StandingEye;m_player.grounded=true;m_player.verticalVelocity=0;
 m_player.angle=std::atan2(out.y,out.x);m_player.pitch=0;m_velocity={};m_stepDistance=0;m_jumpBuffer=0;m_suppressFire=true;m_playerExposure=0;
 // The doors pull to with a soft clank only someone at arm's length would notice.
 SoundEvent clank{Sound::Door,lockerFront(f,0),.3f,1.35f,true,6,floor+1.2f};clank.aiRadius=1.2f;m_sounds.push_back(clank);
 m_pickupNotice="HIDING";m_pickupNoticeTime=1.2f;return true;
}
bool Game::leaveLocker(){
 const int index=hiddenLocker();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];Vec2 exit;float feet;
 if(!lockerExit(f,exit,feet,true)){m_pickupNotice="NO ROOM TO STEP OUT";m_pickupNoticeTime=1.2f;return false;}
 forgetLocker();m_player.pos=exit;m_player.z=feet;m_velocity={};m_stepDistance=0;
 SoundEvent clank{Sound::Door,lockerFront(f,0),.3f,1.2f,true,6,feet+1.2f};clank.aiRadius=1.2f;m_sounds.push_back(clank);return true;
}
// Out, however it happened: the next time in is a new secret, so a guard who
// saw this one cannot open the locker on the strength of it.
void Game::forgetLocker(){setState(PlayerLocker,0);for(size_t actor=0;actor<m_friendlyActors.size();++actor)setState(friendlyKey(actor,"guard_locker"),0);}
// Shut in, the body stays put and the hands stay still; the head turns only
// as far as the slats let it see out. True while the player is inside.
bool Game::updateLocker(const InputState& input){
 if(!state(PlayerLocker))return false;const int index=hiddenLocker();
 // A record that matches no locker the player is in (a jump to another map,
 // an older build's save) just lets them go where they stand.
 if(index<0){forgetLocker();return false;}
 if(input.vrTracked){if(!leaveLocker())forgetLocker();return false;}
 const Vec2 out=lockerOut(m_world.fixtures()[size_t(index)]);const float facing=std::atan2(out.y,out.x);
 m_player.angle=wrapAngle(facing+std::clamp(wrapAngle(m_player.angle-facing),-LockerYaw,LockerYaw));m_player.pitch=std::clamp(m_player.pitch,-LockerPitch,LockerPitch);
 m_velocity={};m_suppressFire=true;m_playerExposure=0;return true;
}
// The guard who watched the player climb in, at its doors: he opens them and
// hauls the player out in front of him. False, and he forgets the locker,
// when the player is no longer in it or he never got to its doors.
bool Game::pullFromLocker(int level,size_t actor,ActorPose& pose){
 const int saw=state(friendlyKey(actor,"guard_locker"))-1;setState(friendlyKey(actor,"guard_locker"),0);
 const int index=hiddenLocker();if(index<0||index!=saw||level<0||level>=chunkCount())return false;const auto&f=m_world.fixtures()[size_t(index)];
 const Vec2 shift=chunkOffset(m_level)-chunkOffset(level);Vec2 exit;float feet;
 if(lengthSq(pose.position-lockerFront(f,LockerEye)-shift)>1.9f*1.9f||std::fabs(pose.z-m_player.z)>.6f||!lockerExit(f,exit,feet,true))return false;
 forgetLocker();m_player.pos=exit;m_player.z=feet;m_player.pitch=0;m_velocity={};
 const Vec2 toward=pose.position-exit-shift;m_player.angle=std::atan2(toward.y,toward.x);pose.yaw=std::atan2(-toward.x,-toward.y);
 rememberGuardTarget(actor,resolveGuardTarget(actor,{ShotKind::Player,m_level,0}));const Vec2 global=exit+chunkOffset(m_level);auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
 put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(feet*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_confirmed",1);put("guard_recognition",m_friendlyActors[actor].track().ai.recognitionMs);
 // The doors bang back: anyone near hears it.
 m_sounds.push_back({Sound::Door,lockerFront(f,0),.8f,1.1f,true,12,feet+1.2f});
 m_pickupNotice="FOUND";m_pickupNoticeTime=1.5f;return true;
}
bool Game::testStealth(){
 std::ofstream report("stealth-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 auto scene=[](){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Stealth physical validation";campaign->key=883015;
  auto map=std::make_shared<AuthoredMapData>();map->name="Existing mechanics test room";map->definition={{0,0},{4.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("stealth_clock");timer.finishState=stateId("stealth_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack track;track.timerState=timer.timerState;track.visual=ActorVisual::Cleanup;track.keys={{0,{5.5f,6.5f},0,kPi*.5f},{30000,{5.5f,6.5f},0,kPi*.5f}};track.scale=1.6f;track.health=90;track.damageState=stateId("stealth_damage");track.deadState=stateId("stealth_dead");track.ai.mode=ActorAiMode::Guard;track.ai.armed=true;track.ai.patrol={{{5.5f,6.5f},0,5000}};map->actorTracks.push_back(track);campaign->maps.push_back(map);return Game(campaign);
 };
 for(int rate:{60,120}){
  auto g=scene();InputState q;q.subdue=true;g.update(q,1.f/rate);
  check(g.npcIncapacitated(0)&&!g.npcHeld(0)&&!g.state("stealth_dead")&&!g.state("stealth_damage")&&g.kills()==0,"Rear Q incapacitates without a lethal flag, health loss or kill credit at 60/120 Hz");
  for(int i=0;i<rate*3;++i)g.update({},1.f/rate);
  check(g.npcActors()[0].pose.clip==4&&!g.npcWeaponPose(0).armed&&g.player().health==100&&g.enemiesRemaining()==0,"Unconscious guards stay down, cannot fire and satisfy hostile-clear gates");
  Game loaded(g.m_customCampaign);check(loaded.decodeSave(g.encodeSave())&&loaded.npcIncapacitated(0)&&!loaded.state("stealth_dead"),"Unconscious identity and living health survive save/load");
  loaded.update({},1.f/rate);check(loaded.npcActors()[0].pose.clip==4&&loaded.player().health==100,"A loaded unconscious guard does not resume its schedule");
  float fallenPhase=g.npcActors()[0].pose.phase;g.damageFriendly(0,90,g.player().pos,{ShotKind::Player,0,0});check(g.state("stealth_dead")&&!g.npcIncapacitated(0)&&g.npcActors()[0].pose.phase>=fallenPhase-.01f,"Lethal damage to an unconscious body preserves its fallen animation instead of standing it up again");
  auto held=scene();InputState use;use.use=true;held.update(use,1.f/rate);held.update({},1.f/rate);
  check(held.npcHeld(0)&&!held.npcWeaponPose(0).armed,"Rear E captures and disarms a living guard");
  auto front=held.traceShot({9.f,6.5f,1.f},{-1,0,0},6,-1,true);
  auto side=held.traceShot({4.5f,10.5f,1.f},{0,-1,0},6,-1,true);
  check(front.kind==ShotKind::Npc&&front.index==0&&side.kind==ShotKind::Player,"Shield physically intercepts frontal fire while side fire still reaches the player");
  held.resolveShot(front,{9,6.5f,1},{-1,0,0},weaponDef(WeaponId::Shotgun).hitscan,false,18);
  check(held.state("stealth_damage")==18&&held.player().health==100,"Intercepted fire damages the hostage instead of applying a player damage discount");
  Game heldLoad(held.m_customCampaign);check(heldLoad.decodeSave(held.encodeSave())&&heldLoad.npcHeld(0),"A living held identity survives save/load");
  InputState walk;walk.back=true;auto at=heldLoad.player().pos;for(int i=0;i<rate;++i)heldLoad.update(walk,1.f/rate);
  check(heldLoad.npcHeld(0)&&length(heldLoad.player().pos-at)>.5f&&length(heldLoad.npcActors()[0].pose.position-heldLoad.player().pos)<.8f,"Held body follows real slow movement after load");
  heldLoad.update(q,1.f/rate);check(heldLoad.npcIncapacitated(0)&&!heldLoad.npcHeld(0)&&!heldLoad.state("stealth_dead"),"Q ends a grab with a nonlethal knockout");
  auto lethal=scene();lethal.equipWeapon(WeaponId::Fists);lethal.punchImpact();check(lethal.state("stealth_dead")==1&&!lethal.npcIncapacitated(0),"Existing rear lethal melee remains available");
 }
 {auto g=scene();g.m_player.pos={6.5f,6.5f};g.m_player.angle=kPi;check(g.nearbyTakedown()<0,"Frontal approaches cannot capture or silently knock out");}
 {
  auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto&medic=map->actorTracks[0];medic.visual=ActorVisual::CleanupMedic;medic.ai.role=GuardSquadRole::Medic;medic.ai.squad=stateId("stealth_support_squad");
  auto patient=medic;patient.visual=ActorVisual::CleanupHeavy;patient.ai.role=GuardSquadRole::Heavy;patient.damageState=stateId("stealth_patient_damage");patient.deadState=stateId("stealth_patient_dead");for(auto&key:patient.keys)key.position={5.5f,8.5f};patient.ai.patrol={{{5.5f,8.5f},0,5000}};map->actorTracks.push_back(patient);campaign->maps[0]=map;
  Game g(campaign);g.setState(patient.damageState,50);for(int i=0;i<900&&g.state(g.friendlyKey(0,"support_ms"))<300;++i)g.update({},1.f/60);
  bool opened=g.state(g.friendlyKey(0,"support_ms"))>=300;auto pose=g.npcActors()[0].pose;Vec2 forward{std::sin(pose.yaw),std::cos(pose.yaw)};g.m_player.pos=pose.position-forward*.95f;g.m_player.angle=std::atan2(forward.y,forward.x);InputState q;q.subdue=true;g.update(q,1.f/60);
  Game loaded(campaign);bool restored=loaded.decodeSave(g.encodeSave());for(int i=0;i<660&&restored;++i)loaded.update({},1.f/60);
  check(opened&&g.npcIncapacitated(0)&&restored&&loaded.npcIncapacitated(0)&&loaded.state(patient.damageState)==50&&loaded.state(loaded.friendlyKey(0,"support_used"))==0,"Rear knockout during a real Medic kit cancels treatment across save/load without healing or spending stock");
 }
 {auto g=scene();g.setState(g.friendlyKey(0,"guard_confirmed"),1);check(g.nearbyTakedown()<0,"A guard with a confirmed hostile memory cannot be silently captured");}
 {auto g=scene();g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Investigate));check(g.nearbyTakedown()==0,"A noise investigator is vulnerable from behind before identifying the player");}
 {
  auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);
  auto&victim=map->actorTracks[0];for(auto&key:victim.keys){key.position={8.5f,6.5f};key.yaw=-kPi*.5f;}victim.ai.patrol={{{8.5f,6.5f},0,5000}};
  auto witness=victim;witness.damageState=stateId("stealth_witness_damage");witness.deadState=stateId("stealth_witness_dead");
  for(auto&key:witness.keys){key.position={5.5f,6.5f};key.yaw=kPi*.5f;}witness.ai.patrol={{{5.5f,6.5f},0,5000}};map->actorTracks.push_back(witness);campaign->maps[0]=map;
  Game g(campaign);g.m_player.pos={9.4f,6.5f};g.m_player.angle=kPi;InputState q;q.subdue=true;g.update(q,1.f/60);
  for(int i=0;i<90;++i)g.update({},1.f/60);
  g.m_player.pos={4.55f,6.5f};g.m_player.angle=0;
  check(g.npcIncapacitated(0)&&g.state(g.friendlyKey(1,"guard_confirmed"))&&g.nearbyTakedown()<0,"Seeing a fallen colleague confirms a hostile and bars the next silent rear takedown");
 }
 {
  auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);
  auto&victim=map->actorTracks[0];for(auto&key:victim.keys){key.position={8.5f,6.5f};key.yaw=-kPi*.5f;}victim.ai.patrol={{{8.5f,6.5f},0,5000}};
  auto listener=victim;listener.damageState=stateId("stealth_listen_damage");listener.deadState=stateId("stealth_listen_dead");
  // Face away so sight cannot steal the frame — the choke must carry on sound alone.
  for(auto&key:listener.keys){key.position={14.5f,6.5f};key.yaw=kPi*.5f;}listener.ai.patrol={{{14.5f,6.5f},0,5000}};map->actorTracks.push_back(listener);campaign->maps[0]=map;
  Game g(campaign);g.m_player.pos={9.4f,6.5f};g.m_player.angle=kPi;InputState q;q.subdue=true;g.update(q,1.f/60);
  auto choke=std::find_if(g.m_sounds.begin(),g.m_sounds.end(),[](const SoundEvent&e){return e.sound==Sound::PunchHit&&e.spatial;});
  check(choke!=g.m_sounds.end()&&choke->gain>=.4f&&choke->gain<=.5f,"A rear knockout emits a spatial struggle at the victim, heard across a room but not a yard");
  check(g.guardState(1)==GuardState::Investigate,"A nearby guard investigates the choke instead of remaining on patrol");
 }
 {auto g=scene();g.m_player.pos={3.5f,6.5f};check(g.nearbyTakedown()<0,"Rear actions respect physical reach");}
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);map->structures.push_back({4.95f,6,5.05f,7,0,2});campaign->maps[0]=map;Game g(campaign);check(g.nearbyTakedown()<0,"A narrow wall blocks a rear action even within arm's reach");}
 {auto g=scene();g.restrainNpc(0,true);g.update({},.01f);InputState fire;fire.fire=true;g.update(fire,.01f);Game loaded(g.m_customCampaign);check(g.state("stealth_dead")==1&&!g.npcIncapacitated(0)&&loaded.decodeSave(g.encodeSave()),"Executing a held guard clears the living restraint and remains saveable");}
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);map->structures.push_back({4,5.7f,6,5.9f,0,2});campaign->maps[0]=map;Game g(campaign);g.restrainNpc(0,true);g.update({},.01f);auto at=g.npcActors()[0].pose.position;g.m_player.angle=-kPi*.5f;g.update({},.01f);check(!g.npcHeld(0)&&g.npcIncapacitated(0)&&length(g.npcActors()[0].pose.position-at)<.01f,"Turning a captive into a wall releases them at the last collision-safe position");}
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);map->structures.push_back({4.75f,6.02f,4.95f,6.2f,0,.6f});campaign->maps[0]=map;Game g(campaign);g.restrainNpc(0,true);g.update({},.01f);auto at=g.npcActors()[0].pose.position;g.m_player.angle=-kPi*.5f;g.update({},.01f);check(!g.npcHeld(0)&&g.npcIncapacitated(0)&&length(g.npcActors()[0].pose.position-at)<.01f,"A captive's full swept legs cannot pass through a low obstacle below the chest ray");}
 {auto g=scene();g.restrainNpc(0,true);g.update({},.01f);g.m_player.angle=kPi;g.update({},.01f);check(g.npcHeld(0),"A clear turn repositions the held body without leaving its shield behind");g.update({},.01f);InputState use;use.use=true;g.update(use,.01f);check(!g.npcIncapacitated(0)&&g.state(g.friendlyKey(0,"guard_confirmed")),"E release returns a living, alerted opponent to ordinary AI");}
 {
  auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto first=std::make_shared<AuthoredMapData>(*campaign->maps[0]);first->openEast=true;first->definition.playerStart={23.9f,6.5f};first->definition.spawnYaw=kPi;
  auto&track=first->actorTracks[0];for(auto&key:track.keys){key.position={23.2f,6.5f};key.yaw=-kPi*.5f;}track.ai.patrol={{{23.2f,6.5f},0,5000}};
  auto second=std::make_shared<AuthoredMapData>(*first);second->definition.origin={24,0};second->definition.playerStart={1,6.5f};second->openEast=false;second->openWest=true;second->actorTracks.clear();campaign->maps={first,second};
  Game g(campaign);auto directory=std::filesystem::current_path()/".build"/"stealth-seam-test-saves";std::filesystem::create_directories(directory);g.setSaveDirectory(directory.wstring());g.restrainNpc(0,true);g.update({},1.f/60);
  InputState back;back.back=true;for(int i=0;i<120&&g.level()==0;++i)g.update(back,1.f/60);
  Game loaded(campaign);loaded.setSaveDirectory(directory.wstring());check(g.level()==1&&g.heldNpc()<0&&loaded.loadCheckpoint()&&loaded.level()==1&&loaded.npcIncapacitated(0),"Crossing a stitched seam releases the captive before its automatic checkpoint, which loads successfully");
  std::error_code error;std::filesystem::remove(directory/"checkpoint.rms",error);std::filesystem::remove(directory,error);
 }
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
bool Game::testLockers(){
 std::ofstream report("locker-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 // A bare dim room: one locker at the west end with its doors facing east,
 // the player a step in front of them, and one guard on a beat down the room.
 auto scene=[](std::vector<ActorPatrolPoint> beat,float yaw,std::vector<Structure> walls={},bool engineer=false){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Locker validation";campaign->key=883016;
  auto map=std::make_shared<AuthoredMapData>();map->name="Locker test room";map->definition={{0,0},{3.2f,6.5f},0,Environment::Interior,false,8,.3f,43};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  map->fixtures.push_back({model::pwr_locker_bank,{2.f,6.5f},0,.87f,.85f,2.42f,0,true});
  for(const auto&wall:walls)map->structures.push_back(wall);
  TimedSequence timer;timer.timerState=stateId("locker_clock");timer.finishState=stateId("locker_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack track;track.timerState=timer.timerState;track.visual=ActorVisual::Cleanup;track.keys={{0,beat[0].position,0,yaw},{30000,beat[0].position,0,yaw}};track.scale=1.6f;track.health=90;track.damageState=stateId("locker_damage");track.deadState=stateId("locker_dead");track.ai.mode=ActorAiMode::Guard;track.ai.armed=true;track.ai.patrol=beat;if(engineer){track.ai.role=GuardSquadRole::Engineer;track.ai.squad=stateId("locker_team");}
  map->actorTracks.push_back(track);campaign->maps.push_back(map);Game g(campaign);g.m_player.pos={3.2f,6.5f};g.m_player.z=0;g.m_player.angle=kPi;g.m_player.pitch=0;return g;
 };
 InputState use;use.use=true;const float out=2.435f;
 auto hide=[&](Game& g){g.update(use,1.f/60);g.update({},1.f/60);return g.hiddenLocker()>=0;};
 // Unseen: the guard has his back turned, then walks his beat up to the doors.
 {auto g=scene({{{10.5f,6.5f},0,600},{{4.2f,6.5f},0,4000}},kPi*.5f);
  check(g.lockerAhead()==0&&hide(g)&&g.hiddenLocker()==0&&g.player().pos.x>out&&g.player().pos.x<out+.15f&&std::fabs(g.player().pos.y-6.5f)<.01f,"E at a locker's doors shuts the player in it, eye at the doors");
  check(!g.state(g.friendlyKey(0,"guard_locker"))&&g.guardState(0)==GuardState::Patrol,"A guard with his back turned does not see which locker");
  ActorPose facing=g.npcActors()[0].pose;facing.position={3.6f,6.5f};facing.yaw=-kPi*.5f;
  check(!g.guardCanSee(0,facing,g.resolveGuardTarget(0,{ShotKind::Player,0,0})),"A guard square in front of a shut locker cannot see the player in it");
  float closest=99;bool calm=true;auto at=g.player().pos;int steps=0;
  InputState push;push.forward=true;push.fire=true;push.crouch=true;push.jump=true;int ammo=g.player().ammo,chambered=g.player().loaded;
  for(int i=0;i<60*12;++i){g.update(i<120?push:InputState{},1.f/60);closest=std::min(closest,length(g.npcActors()[0].pose.position-at));calm&=g.guardState(0)==GuardState::Patrol&&!g.state(g.friendlyKey(0,"guard_seen"));
   for(const auto&s:g.sounds())steps+=!s.spatial&&(footstepSound(s.sound)||s.sound==Sound::Jump||s.sound==Sound::Shot||s.sound==Sound::PunchSwing);} // the player's own sounds are the unplaced ones
  check(closest<2.f&&calm&&g.hiddenLocker()==0,"A guard who walks his beat right up to the doors stays on patrol and never opens them");
  check(length(g.player().pos-at)<.001f&&!g.player().crouched&&g.player().ammo==ammo&&g.player().loaded==chambered&&steps==0,"Inside, walking, crouching, jumping and firing do nothing and make no steps");
  check(g.playerExposure()==0,"The light meter reads nothing inside a shut locker");
  // Under a radian a frame, too little to wrap: each way ends hard on its own
  // edge of the cone (the doors face +x, so the facing is angle 0).
  auto turn=[&](float dx,float dy){InputState look;look.mouseDx=dx;look.mouseDy=dy;for(int i=0;i<4;++i)g.update(look,1.f/60);return std::pair{wrapAngle(g.player().angle),g.player().pitch};};
  auto [right,down]=turn(400,400);auto [left,up]=turn(-400,-400);
  check(std::fabs(right-LockerYaw)<.001f&&std::fabs(left+LockerYaw)<.001f&&std::fabs(down+LockerPitch)<.001f&&std::fabs(up-LockerPitch)<.001f,"The view stays in a cone looking out of the doors, to its edge either way");
  Game loaded(g.m_customCampaign);check(loaded.decodeSave(g.encodeSave())&&loaded.hiddenLocker()==0&&length(loaded.player().pos-at)<.001f,"A save made in a locker loads with the player still shut in it");
  loaded.update({},1.f/60);loaded.update(use,1.f/60);
  check(loaded.hiddenLocker()<0&&loaded.player().pos.x>out+.2f&&std::fabs(loaded.player().pos.y-6.5f)<.5f&&loaded.hullFits(loaded.player().pos,loaded.player().z,Player::StandingHeight),"E again steps out in front of the doors where a standing body fits");
 }
 // Seen: the guard is looking down the room as the player climbs in.
 {auto g=scene({{{8.5f,6.5f},0,5000}},-kPi*.5f);bool hidden=hide(g);
  check(hidden&&g.state(g.friendlyKey(0,"guard_locker"))==1&&g.guardState(0)==GuardState::Investigate,"A guard watching the player climb in remembers the locker and goes to it");
  int frames=0;for(;frames<60*15&&g.hiddenLocker()>=0;++frames)g.update({},1.f/60);
  const auto&guard=g.npcActors()[0].pose;
  check(g.hiddenLocker()<0&&g.guardState(0)==GuardState::Alert&&g.state(g.friendlyKey(0,"guard_confirmed"))&&!g.state(g.friendlyKey(0,"guard_locker")),"At the doors he opens them and pulls the player out, alert and sure of them");
  const Vec2 facingGuard{std::cos(g.player().angle),std::sin(g.player().angle)};
  check(g.player().pos.x>out+.2f&&g.player().pos.x<out+.7f&&length(guard.position-g.player().pos)<1.9f&&dot(facingGuard,normalized(guard.position-g.player().pos))>.95f&&g.hullFits(g.player().pos,g.player().z,Player::StandingHeight),"The player lands in front of the locker, facing the guard, with room to stand");
  report<<"  pulled out after "<<frames<<" frames\n";
 }
 // Heard, not seen: a guard sent to the doors by a noise searches and leaves them shut.
 {auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f);hide(g);auto put=[&](const char*f,int v){g.setState(g.friendlyKey(0,f),v);};
  put("guard_x",3535);put("guard_y",6500);put("guard_z",0);put("guard_memory",1);put("guard_travel",0);put("guard_mode",int(GuardState::Investigate));
  float closest=99;bool searched=false,never=true;for(int i=0;i<60*15;++i){g.update({},1.f/60);closest=std::min(closest,length(g.npcActors()[0].pose.position-Vec2{3.535f,6.5f}));searched|=g.guardState(0)==GuardState::Search;never&=g.guardState(0)!=GuardState::Alert;}
  check(closest<.6f&&searched&&never&&g.hiddenLocker()==0,"A guard who only heard something searches in front of the locker and never opens it");
 }
 // Hands full, or the doors against a wall: no way in.
 {auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f);g.setState(g.friendlyKey(0,"restraint"),1);g.m_friendlyActors[0].pose.clip=4;g.m_friendlyActors[0].pose.position={4.05f,6.5f};g.m_draggedNpc=0;
  bool refused=g.lockerAhead()<0;g.update(use,1.f/60);g.update({},1.f/60);
  check(refused&&g.hiddenLocker()<0&&!g.draggingBody(),"E while dragging a body puts the body down instead of climbing in");
 }
 {auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f);g.m_grabbed=.4f;bool held=g.lockerAhead()<0&&!hide(g);g.m_grabbed=0;
  check(held&&g.lockerAhead()==0,"In a Snatcher's grip there is no slipping into a locker; free of it there is");
 }
 // The wall stands a hand's breadth off the doors. The player is right at
 // them with a clear look in, so only the missing way out turns them away.
 const Structure flush{2.75f,5.f,2.85f,8.f,0,2.6f};
 {auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f,{flush});g.m_player.pos={out+.11f,6.5f};const auto&locker=g.m_world.fixtures()[0];Vec2 spot;float feet;
  bool refused=g.m_world.rayClear(g.player().pos,1.f,{out+.03f,6.5f},1.f,true)&&!g.lockerExit(locker,spot,feet)&&!g.lockerExit(locker,spot,feet,true)&&g.lockerAhead()<0;
  g.m_player.pos={out+LockerEye,6.5f};g.setState(PlayerLocker,1);bool shut=g.hiddenLocker()==0;g.update(use,1.f/60);
  check(refused&&shut&&g.hiddenLocker()==0&&g.pickupNotice()=="NO ROOM TO STEP OUT","A locker whose doors open against a wall is no hiding place, and nobody steps out into the wall");
 }
 // A guard planted at the doors of a narrow nook takes every plain way out;
 // stepping out squeezes past him rather than leaving the player boxed in.
 {auto g=scene({{{out+.45f,6.5f},0,60000}},-kPi*.5f,{{out+.005f,5.95f,4.5f,6.05f,0,2.6f},{out+.005f,6.95f,4.5f,7.05f,0,2.6f}});
  g.m_player.pos={out+LockerEye,6.5f};g.setState(PlayerLocker,1);Vec2 spot;float feet;bool boxed=g.hiddenLocker()==0&&!g.lockerExit(g.m_world.fixtures()[0],spot,feet);g.update(use,1.f/60);
  check(boxed&&g.hiddenLocker()<0&&g.player().pos.x>out+.2f&&std::fabs(g.player().pos.y-6.5f)<.3f&&g.hullFits(g.player().pos,g.player().z,Player::StandingHeight),"With a guard planted at the doors of a narrow nook the player still squeezes out");
  report<<"  boxed "<<boxed<<", out at "<<g.player().pos.x<<','<<g.player().pos.y<<", guard at "<<g.npcActors()[0].pose.position.x<<','<<g.npcActors()[0].pose.position.y<<'\n';
 }
 // A save made in a locker whose doors an update has since walled off loads
 // with the player let go, and they work free of the steel.
 {auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f);hide(g);auto blocked=scene({{{10.5f,6.5f},0,5000}},kPi*.5f,{flush});
  bool freed=blocked.decodeSave(g.encodeSave())&&blocked.hiddenLocker()<0&&!blocked.state(PlayerLocker);InputState walk;walk.left=true;for(int i=0;i<90;++i)blocked.update(walk,1.f/60);
  check(freed&&blocked.hullFits(blocked.player().pos,blocked.player().z,Player::StandingHeight),"A save in a locker an update has walled off loads with the player out of it and free to move");
 }
 // A witness sent searching some other way gives the locker up, and every
 // witness forgets it once the player is out.
 {auto g=scene({{{8.5f,6.5f},0,5000}},-kPi*.5f);bool seen=hide(g)&&g.state(g.friendlyKey(0,"guard_locker"))==1;g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Search));g.update({},1.f/60);
  bool dropped=!g.state(g.friendlyKey(0,"guard_locker"))&&g.hiddenLocker()==0;
  auto again=scene({{{8.5f,6.5f},0,5000}},-kPi*.5f);seen&=hide(again)&&again.state(again.friendlyKey(0,"guard_locker"))==1;again.update(use,1.f/60);
  check(seen&&dropped&&again.hiddenLocker()<0&&!again.state(again.friendlyKey(0,"guard_locker")),"A witness who goes to searching, or whose player steps out, forgets the locker");
 }
 // An Engineer's drone with the player in its sights fires on them in the
 // open and loses them at the doors: no aim, no rounds into the steel.
 for(bool inside:{false,true}){auto g=scene({{{6.5f,6.5f},0,60000}},kPi*.5f,{},true);if(inside)hide(g);
  bool deployed=g.deployEngineerDrone(0);g.rememberGuardTarget(0,g.resolveGuardTarget(0,{ShotKind::Player,0,0}));for(const char*f:{"guard_confirmed","guard_seen","guard_memory"})g.setState(g.friendlyKey(0,f),1);g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Alert));
  const float health=g.player().health;for(int i=0;i<240;++i){g.m_npcNavBudget=4;g.updateEngineerDrones(1.f/60);}
  if(inside)check(deployed&&g.hiddenLocker()==0&&g.player().health==health,"An Engineer's drone cannot see or shoot the player shut in a locker");
  else check(deployed&&g.player().health<health,"The same drone fires on the player standing in the open");
 }
 // Nothing that hunts by sight finds the player, but a blast still does.
 {auto g=scene({{{20.5f,20.5f},0,5000}},0);Enemy e;e.pos={6.5f,6.5f};e.home=e.pos;e.homeZ=0;e.heading=kPi;g.m_enemies={e};auto watched=g;
  for(int i=0;i<20;++i)watched.update({},1.f/60);hide(g);for(int i=0;i<20;++i)g.update({},1.f/60);
  check(watched.enemies()[0].state==Enemy::State::Chase&&g.enemies()[0].state!=Enemy::State::Chase&&g.enemies()[0].awareness==0,"A creature that sees the player in the open sees nothing of them in a locker");
  g.m_enemies.clear();float health=g.player().health;g.spawnTacticalGrenade({3.2f,6.5f,.3f},{0,0,0},{ShotKind::Npc,0,0},0,AiFaction::Neutral,1800);for(int i=0;i<150;++i)g.update({},1.f/60);
  check(g.player().health<health&&g.hiddenLocker()==0,"A grenade at the doors still hurts the player inside");
 }
 // The campaign's own lockers: the Operations Corridor's west wall, and the
 // Gatehouse bunk room's pair, which now open into the room.
 for(auto [level,at,angle]:{std::tuple{57,Vec2{2.55f,2.87f},kPi},std::tuple{59,Vec2{17.4f,15.05f},kPi*.5f}}){
  auto g=mapInspection(at,angle,0,level,false,-999,true);bool ahead=g.lockerAhead()>=0;bool hidden=hide(g);Game loaded=g;bool restored=loaded.decodeSave(g.encodeSave())&&loaded.hiddenLocker()==g.hiddenLocker()&&length(loaded.player().pos-g.player().pos)<.001f;
  loaded.update({},1.f/60);loaded.update(use,1.f/60);
  check(ahead&&hidden&&restored&&loaded.hiddenLocker()<0&&loaded.hullFits(loaded.player().pos,loaded.player().z,Player::StandingHeight),level==57?"Map 57's west-wall lockers hide the player through a save and let them out again":"Map 59's bunk-room lockers hide the player through a save and let them out again");
 }
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

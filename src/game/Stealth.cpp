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
  if(npc.level!=m_level||state(friendlyKey(actor,"restraint"))!=1||npc.pose.clip!=4||state(friendlyKey(actor,"guard_stowed"))||(npc.track().deadState&&state(npc.track().deadState))||!humanActorVisual(npc.track().visual))continue;
  float distance=length(npc.pose.position-m_player.pos);if(distance<nearest&&std::fabs(npc.pose.z-m_player.z)<.5f){nearest=distance;chosen=int(actor);}
 }return chosen;
}
void Game::dropBody(){if(m_draggedNpc<0)return;if(size_t(m_draggedNpc)<m_friendlyActors.size())saveRestrainedPose(size_t(m_draggedNpc));m_draggedNpc=-1;m_pickupNotice="BODY LEFT";m_pickupNoticeTime=1.2f;}
Game::GuardCard Game::guardCard(size_t actor)const{
 GuardCard card;if(actor>=m_friendlyActors.size()||npcIncapacitated(actor))return card;const auto& npc=m_friendlyActors[actor];
 if(npc.track().ai.mode!=ActorAiMode::Guard||npc.pose.clip==4||(npc.track().deadState&&state(npc.track().deadState)))return card;
 auto get=[&](const char*f){return state(friendlyKey(actor,f));};auto mode=guardState(actor);
 if(guardCalling(actor))card.call=std::clamp(get("guard_radio")/float(RadioCallMs),0.f,1.f);
 // The '?' fills from the foot as he makes the player out.
 card.fill=std::clamp(get("guard_recognition")/float(std::max(1,npc.track().ai.recognitionMs)),0.f,1.f);
 if(mode==GuardState::Alert){card.kind=GuardCardKind::Alert;card.bangMs=get("guard_bang_ms");}
 else if(mode==GuardState::Suspicious){card.kind=GuardCardKind::Making;card.fill=std::max(card.fill,.05f);}
 // Searching for the player he had: a full red '?'. A noise, or a glimpse he never made out: a dim one.
 else if(mode==GuardState::Investigate||mode==GuardState::Search)card.kind=get("guard_confirmed")&&get("guard_target_kind")==int(ShotKind::Player)?GuardCardKind::Hunting:GuardCardKind::Checking;
 else if(card.call>0)card.kind=GuardCardKind::Checking;
 return card;
}
float Game::guardAwareness(size_t actor)const{
 auto card=guardCard(actor);
 switch(card.kind){case GuardCardKind::Alert:return 2;case GuardCardKind::Hunting:return 3;case GuardCardKind::Making:return card.fill;case GuardCardKind::Checking:return .5f;default:return 0;}
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
// Any solid double-door steel locker whose leaves (the model's local +x) open
// onto floor a body can stand on is somewhere to hide. Shut in, the player
// looks out through the perforations in its leaves: no steps, no light on them,
// and nobody outside sees in. A guard who watched them climb in knows which locker and comes to
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
bool lockerModel(const Fixture& f){return f.model==model::psx_locker&&f.solid&&f.visible;}
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
 if(m_vrInputActive||dead()||draggingBody()||heldNpc()>=0||holdingClutter()||takedownActive()||m_grabbed>0||!m_player.grounded||hiddenLocker()>=0)return -1;
 const Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};int best=-1;float nearest=1.05f;const auto&fixtures=m_world.fixtures();
 for(size_t i=0;i<fixtures.size();++i){const auto&f=fixtures[i];if(!lockerModel(f)||lockerBody(m_level,int(i)))continue;
  const Vec2 out=lockerOut(f),side{-out.y,out.x},delta=m_player.pos-f.position;const float ahead=dot(delta,out)-f.width*.5f;
  if(ahead<.1f||ahead>=nearest||std::fabs(dot(delta,side))>f.depth*.5f+.1f||dot(forward,out)>-.55f)continue;
  const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;Vec2 exit;float feet;
  if(std::fabs(m_player.z-floor)>.3f||!m_world.rayClear(m_player.pos,m_player.z+1.f,lockerFront(f,.03f),floor+1.f,true)||!lockerExit(f,exit,feet))continue;
  best=int(i);nearest=ahead;
 }return best;
}
bool Game::enterLocker(){
 const int index=lockerAhead();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];const Player before=m_player;
 const Vec2 out=lockerOut(f),stand=lockerFront(f,LockerStand)+chunkOffset(m_level);const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 // Who watches the body go in decides who comes for the doors (ai/guard/Lockers.cpp).
 watchLockerEntry(index);
 setState(PlayerLocker,index+1);
 m_player.pos=lockerFront(f,LockerEye);m_player.z=floor;m_player.crouched=false;m_player.eye=Player::StandingEye;m_player.grounded=true;m_player.verticalVelocity=0;
 m_player.angle=std::atan2(out.y,out.x);m_player.pitch=0;m_velocity={};m_stepDistance=0;m_jumpBuffer=0;m_suppressFire=true;m_playerExposure=0;
 // The latch lifts with a click only someone at arm's length would notice;
 // the leaves swing wide, the player turns in, and they are pulled to.
 SoundEvent latch{Sound::LockerLatch,lockerFront(f,0),.45f,1.f,true,6,floor+1.2f};latch.aiRadius=lockerEntryHeard();m_sounds.push_back(latch);
 // Both hands go to the leaves, so the gun goes away (as for a takedown) and
 // comes back up once the way out ends. The leaves wait for the hands: from a
 // long step back the body has to close in before they can take the edges.
 // (Put away mid-cycle, the last shot is over: nothing raises it again before the way out.)
 m_lockerRearm|=m_weaponEquipped;equipWeapon(WeaponId::Fists);m_handsLowered=false;m_shotAge=std::max(m_shotAge,equippedWeaponDef().cycle.autoHolsterAfter+.01f);
 const float back=dot(before.pos-lockerFront(f,0),out);float lead=.1f;
 auto closing=[&](float t){const float k=std::clamp(t/.4f,0.f,1.f);return back+(.26f-back)*k*k*(3-2*k);};
 while(lead<.18f&&closing(lead)>.6f)lead+=.01f;
 animateLockerDoor(m_level,index,LockerMotion::Enter);for(auto&d:m_lockerDoors)if(d.level==m_level&&d.fixture==index)d.lead=lead;
 startLockerView(LockerMotion::Enter,before,index);return true;
}
bool Game::leaveLocker(){
 const int index=hiddenLocker();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];Vec2 exit;float feet;
 if(!lockerExit(f,exit,feet,true)){m_pickupNotice="NO ROOM TO STEP OUT";m_pickupNoticeTime=1.2f;return false;}
 // A man at the doors who is not on to the player takes both leaves full in
 // the face and goes down the way a takedown leaves him; anyone near hears it.
 const int burst=lockerBurstTarget();
 const Player before=view();forgetLocker();m_player.pos=exit;m_player.z=feet;m_velocity={};m_stepDistance=0;m_lockerRearm|=m_weaponEquipped;equipWeapon(WeaponId::Fists);
 SoundEvent latch{Sound::LockerLatch,lockerFront(f,0),.45f,.92f,true,6,feet+1.2f};latch.aiRadius=1.2f;m_sounds.push_back(latch);
 if(burst>=0){const size_t actor=size_t(burst);auto&npc=m_friendlyActors[actor];const Vec2 away=lockerOut(f);clearGuardSchedule(actor);
  for(const char*field:{"weapon_aim","weapon_recovery","weapon_bash","weapon_bash_pose","grenade_windup","support_ms","drone_windup","drone_busy","engineer_pad_stage","engineer_pad_ms","guard_seen","guard_slot","guard_locker"})setState(friendlyKey(actor,field),0);
  // The blow lands at once: the fall picks up past the clip's opening stagger.
  const float fall=cleanupAnimationDuration(npc.track().visual,4),into=fall>0?.3f:0.f;
  setState(friendlyKey(actor,"restraint"),1);setState(friendlyKey(actor,"restraint_ms"),int(std::round(fall*1000*into)));setState(friendlyKey(actor,"restraint_fraction"),0);
  // Thrown back off the doors: shoved a step back where the floor takes him,
  // facing them, he goes over backwards (the fall clip drops a man on his back).
  const Vec2 hit=npc.pose.position;{const Vec2 shoved=hit+away*.3f;float landed=npc.pose.z;if(friendlyWalkSegment(m_level,hit,npc.pose.z,shoved,.45f,&landed)){npc.pose.position=shoved;npc.pose.z=landed;}}
  npc.pose.yaw=std::atan2(-away.x,-away.y);npc.pose.clip=4;npc.pose.phase=into;npc.pose.lookAtActor=-1;saveRestrainedPose(actor);updateGuardFlags();
  SoundEvent bang{Sound::LockerBang,hit,.95f,1.04f,true,14,feet+1.2f};bang.aiRadius=8;m_sounds.push_back(bang);
  m_sounds.push_back({Sound::Land,npc.pose.position+away*.4f,.6f,.85f,true,10,feet+.05f});
  m_pickupNotice="GUARD UNCONSCIOUS";m_pickupNoticeTime=1.6f;
 }else m_sounds.push_back({Sound::LockerCreak,lockerFront(f,.1f),.28f,1.1f,true,6,feet+1.6f});
 animateLockerDoor(m_level,index,burst>=0?LockerMotion::Caught:LockerMotion::Exit);startLockerView(LockerMotion::Exit,before,index);m_lockerView.burst=burst>=0;return true;
}
// Out, however it happened: the next time in is a new secret, so a guard who
// saw this one cannot open the locker on the strength of it.
void Game::forgetLocker(){setState(PlayerLocker,0);for(size_t actor=0;actor<m_friendlyActors.size();++actor)setState(friendlyKey(actor,"guard_locker"),0);}
// The way out over: the gun put away for the way in comes up with its own
// raise from below the view; empty hands stay down until they are wanted.
void Game::lockerRearm(){
 if(hiddenLocker()>=0||dead())return;
 if(m_lockerRearm&&!m_weaponEquipped){m_weaponEquipped=true;m_handsLowered=false;m_holster=.97f;}
 else if(!m_weaponEquipped&&!draggingBody()&&heldNpc()<0&&!takedownActive())m_handsLowered=true;
 m_lockerRearm=false;
}
// Shut in, the body stays put and the hands stay still; the head turns only
// as far as the vent holes let it see out; LMB knocks on the leaves. True
// while the player is inside.
bool Game::updateLocker(const InputState& input){
 if(!state(PlayerLocker))return false;const int index=hiddenLocker();
 // A record that matches no locker the player is in (a jump to another map,
 // an older build's save) just lets them go where they stand.
 if(index<0){forgetLocker();return false;}
 if(input.vrTracked){if(!leaveLocker())forgetLocker();return false;}
 if(input.fire&&!m_previousFire)knockInLocker();
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
 const Player before=view();forgetLocker();m_player.pos=exit;m_player.z=feet;m_velocity={};
 const Vec2 toward=pose.position-exit-shift;m_player.angle=std::atan2(toward.y,toward.x);pose.yaw=std::atan2(-toward.x,-toward.y);
 // Eye to eye with him: the view lands looking at his face, not his chest.
 const float face=pose.z+m_friendlyActors[actor].track().scale*.92f-(feet+Player::StandingEye);m_player.pitch=std::clamp(std::atan2(face,std::max(.4f,length(toward)))*140.f,-50.f,50.f);
 rememberGuardTarget(actor,resolveGuardTarget(actor,{ShotKind::Player,m_level,0}));const Vec2 global=exit+chunkOffset(m_level);auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
 put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(feet*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_confirmed",1);put("guard_recognition",m_friendlyActors[actor].track().ai.recognitionMs);
 // The doors bang back on their stops: anyone near hears it. The sting is the player's alone.
 m_sounds.push_back({Sound::LockerBang,lockerFront(f,0),.8f,1.f,true,12,feet+1.2f});m_sounds.push_back({Sound::LockerSting,{},.75f,1.f,false});m_alarmSting=std::max(m_alarmSting,.6f); // the locker's own sting is this detection's; no second one on his '!'
 m_lockerRearm|=m_weaponEquipped;equipWeapon(WeaponId::Fists);animateLockerDoor(m_level,index,LockerMotion::Caught);startLockerView(LockerMotion::Caught,before,index);
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
  map->fixtures.push_back({model::psx_locker,{2.115f,6.5f},0,.64f,.85f,2.42f,0,true});
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
  for(int i=0;i<60*12;++i){g.update(i<120?push:InputState{},1.f/60);closest=std::min(closest,length(g.npcActors()[0].pose.position-at));calm&=g.guardState(0)!=GuardState::Alert&&!g.state(g.friendlyKey(0,"guard_seen"))&&!g.state(g.friendlyKey(0,"guard_locker"));
   for(const auto&s:g.sounds())steps+=!s.spatial&&(footstepSound(s.sound)||s.sound==Sound::Jump||s.sound==Sound::Shot||s.sound==Sound::PunchSwing);} // the player's own sounds are the unplaced ones
  // He turns on his beat as the doors shut: that glimpse sends him to look
  // over the floor in front (Lockers.cpp), never to open them.
  check(closest<2.f&&calm&&g.hiddenLocker()==0,"A guard who walks his beat right up to the doors never sees the player, never opens them and never knows which one");
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
 // Seen: the guard is looking down the room as the player climbs in, and
 // has half made them out already (a witness needs both: Lockers.cpp).
 auto halfSeen=[&](Game& g){const int need=g.npcActors()[0].track().ai.recognitionMs;for(int i=0;i<600&&g.state(g.friendlyKey(0,"guard_recognition"))*2<need;++i)g.update({},1.f/60);};
 {auto g=scene({{{8.5f,6.5f},0,5000}},-kPi*.5f);halfSeen(g);bool hidden=hide(g);for(int i=0;i<20;++i)g.update({},1.f/60);
  check(hidden&&g.state(g.friendlyKey(0,"guard_locker"))==1&&g.guardState(0)==GuardState::Investigate,"A guard who half had the player and watched them climb in remembers the locker and goes to it");
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
 // Hands full, or the doors against a wall: no way in. (With a body by the
 // collar at a free locker E now stows him: locker-guards tests the rest.)
 {auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f);g.setState(g.friendlyKey(0,"restraint"),1);g.m_friendlyActors[0].pose.clip=4;g.m_friendlyActors[0].pose.position={4.05f,6.5f};g.m_draggedNpc=0;
  bool refused=g.lockerAhead()<0;g.update(use,1.f/60);g.update({},1.f/60);
  check(refused&&g.hiddenLocker()<0&&!g.draggingBody()&&g.state(g.friendlyKey(0,"guard_stowed"))==1,"E while dragging a body at a free locker stows the body instead of climbing in");
 }
 // A man dropped by the lockers does not stand between the player and hiding:
 // facing the doors E hides, facing him it takes him by the collar.
 for(bool doors:{true,false}){auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f);g.setState(g.friendlyKey(0,"restraint"),1);g.m_friendlyActors[0].pose.clip=4;g.m_friendlyActors[0].pose.position={4.05f,6.5f};
  if(!doors)g.m_player.angle=0;const std::string hint=g.interactionHint()?g.interactionHint():"";const bool hid=hide(g);
  if(doors)check(hint=="E / HIDE"&&hid&&!g.draggingBody(),"A body lying behind the player does not stop them hiding in the locker they face");
  else check(hint=="E / DRAG BODY"&&!hid&&g.draggingBody(),"Facing the body instead, E takes him by the collar");}
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
 {auto g=scene({{{8.5f,6.5f},0,5000}},-kPi*.5f);halfSeen(g);bool seen=hide(g);for(int i=0;i<20;++i)g.update({},1.f/60);seen&=g.state(g.friendlyKey(0,"guard_locker"))==1;g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Search));g.update({},1.f/60);
  bool dropped=!g.state(g.friendlyKey(0,"guard_locker"))&&g.hiddenLocker()==0;
  auto again=scene({{{8.5f,6.5f},0,5000}},-kPi*.5f);halfSeen(again);seen&=hide(again);for(int i=0;i<60&&again.lockerViewLocked();++i)again.update({},1.f/60);for(int i=0;i<20;++i)again.update({},1.f/60);seen&=again.state(again.friendlyKey(0,"guard_locker"))==1;again.update(use,1.f/60);
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
 for(auto [level,at,angle]:{std::tuple{57,Vec2{2.55f,2.87f},kPi},std::tuple{59,Vec2{17.4f,15.25f},kPi*.5f}}){
  auto g=mapInspection(at,angle,0,level,false,-999,true);bool ahead=g.lockerAhead()>=0;bool hidden=hide(g);Game loaded=g;bool restored=loaded.decodeSave(g.encodeSave())&&loaded.hiddenLocker()==g.hiddenLocker()&&length(loaded.player().pos-g.player().pos)<.001f;
  loaded.update({},1.f/60);loaded.update(use,1.f/60);
  check(ahead&&hidden&&restored&&loaded.hiddenLocker()<0&&loaded.hullFits(loaded.player().pos,loaded.player().z,Player::StandingHeight),level==57?"Map 57's west-wall lockers hide the player through a save and let them out again":"Map 59's bunk-room lockers hide the player through a save and let them out again");
 }
 // The leaves: every motion swings them out and lets them settle shut, each
 // within its time, and none passes the stops.
 {auto g=scene({{{20.5f,20.5f},0,5000}},0);bool settled=true;std::string detail;
  for(auto [motion,peak,within]:{std::tuple{LockerMotion::Enter,.864f,.95f},std::tuple{LockerMotion::Exit,.864f,1.05f},std::tuple{LockerMotion::Caught,1.f,2.45f},std::tuple{LockerMotion::Stow,.864f,1.45f},std::tuple{LockerMotion::Knock,.027f,.3f}}){
   g.animateLockerDoor(0,0,motion);float most=0;int frames=0;for(;frames<240&&!g.m_lockerDoors.empty();++frames){g.updateLockerDoors(1.f/60);most=std::max({most,g.lockerDoorOpen(0,0,0),g.lockerDoorOpen(0,0,1)});}
   const bool ok=std::fabs(most-peak)<.03f&&g.m_lockerDoors.empty()&&g.lockerDoorOpen(0,0,0)==0&&frames<=int(within*60)+2;settled&=ok;
   detail+=" "+std::to_string(int(motion))+":"+std::to_string(most).substr(0,5)+"/"+std::to_string(frames);}
  check(settled,"Each locker motion swings both leaves to its stop and settles them shut in its time");report<<"  peaks/frames"<<detail<<'\n';
 }
 // In: the camera walks the path from where the player stood to the eye behind
 // the shut leaves, with no pop, while the body is already committed inside.
 {auto g=scene({{{10.5f,6.5f},0,600},{{4.2f,6.5f},0,4000}},kPi*.5f);const Player before=g.view();bool started=hide(g)&&g.lockerViewLocked();const Player at=g.player();
  float jump=0,turnStep=0;Player last=g.view();bool still=true;InputState push;push.forward=true;push.mouseDx=300;push.mouseDy=200;push.jump=true;push.crouch=true;push.use=true;push.fire=true;int frames=0;
  // Everything held down until the last frames: the release frame itself takes input as usual.
  auto held=[&]{return g.m_lockerView.age<g.m_lockerView.length-.04f?push:InputState{};};
  for(;frames<120&&g.lockerViewLocked();++frames){g.update(held(),1.f/60);const Player v=g.view();jump=std::max(jump,length(v.pos-last.pos));turnStep=std::max(turnStep,std::fabs(wrapAngle(v.angle-last.angle)));last=v;
   still&=length(g.player().pos-at.pos)<1e-5f&&g.player().angle==at.angle&&g.player().pitch==at.pitch&&g.hiddenLocker()==0;}
  const Player end=g.view();const Vec2 inside=lockerFront(g.m_world.fixtures()[0],-.14f);
  check(started&&length(before.pos-Vec2{3.2f,6.5f})<1e-4f&&frames>=50&&frames<=56&&length(end.pos-inside)<1e-4f&&std::fabs(wrapAngle(end.angle-at.angle))<1e-5f&&jump<.08f&&turnStep<.2f,"Hiding plays a 0.9 s way in that ends on the eye behind the shut leaves, without a pop");
  report<<"  way in "<<frames<<" frames, largest step "<<jump<<" m, largest turn "<<turnStep<<" rad\n";
  check(still,"Nothing the player presses moves, turns or lets them out while the way in plays");
  check(g.lockerShutView()==1&&g.lockerDoorOpen(0,0,0)==0&&g.lockerDoorOpen(0,0,1)==0,"Once in, the leaves are shut and the view is their perforated inner face");
  // A save mid-way loads at the committed end: shut in, no path, eye behind the leaves.
  auto mid=scene({{{10.5f,6.5f},0,600},{{4.2f,6.5f},0,4000}},kPi*.5f);hide(mid);for(int i=0;i<20;++i)mid.update({},1.f/60);Game loaded(mid.m_customCampaign);
  check(mid.lockerViewLocked()&&loaded.decodeSave(mid.encodeSave())&&loaded.hiddenLocker()==0&&!loaded.lockerViewLocked()&&length(loaded.view().pos-inside)<1e-4f&&loaded.lockerShutView()==1,"A save made halfway in loads with the player shut in, the motion over");
  // Knocking from inside: the leaves rattle and a guard hears it at the doors.
  InputState knock;knock.fire=true;g.update({},1.f/60);g.update(knock,1.f/60);bool rattled=!g.m_lockerDoors.empty()&&g.m_lockerDoors[0].motion==LockerMotion::Knock;
  bool heard=false;for(const auto&s:g.aiStimuli())heard|=s.radius>4.5f&&length(s.source-lockerFront(g.m_world.fixtures()[0],.15f))<.01f;
  g.update({},1.f/60);g.update(knock,1.f/60);int knocks=0;for(const auto&s:g.sounds())knocks+=s.sound==Sound::LockerKnock;
  const std::string hint=g.interactionHint()?g.interactionHint():"";
  check(rattled&&heard&&knocks==0&&g.hiddenLocker()==0&&hint=="E / STEP OUT   LMB / KNOCK","LMB shut in knocks on the leaves: they rattle, a guard can hear it at the doors, and it keeps knockOnWall's cooldown");
  // Out: the leaves fly open and the camera steps out onto the committed spot.
  for(int i=0;i<60;++i)g.update({},1.f/60);const Player inView=g.view();g.update(use,1.f/60);const Player out=g.player();bool exitStarted=g.lockerViewLocked()&&g.hiddenLocker()<0&&length(g.view().pos-inView.pos)<.02f;
  jump=0;last=g.view();frames=0;bool stayed=true;for(;frames<60&&g.lockerViewLocked();++frames){g.update(held(),1.f/60);jump=std::max(jump,length(g.view().pos-last.pos));last=g.view();stayed&=length(g.player().pos-out.pos)<1e-5f;}
  check(exitStarted&&frames>=28&&frames<=32&&length(g.view().pos-g.player().pos)<1e-5f&&jump<.08f&&stayed&&std::max(g.lockerDoorOpen(0,0,0),g.lockerDoorOpen(0,0,1))>.5f,"Stepping out flings the leaves open and walks the camera out to the player in half a second, input held");
  report<<"  way out "<<frames<<" frames, largest step "<<jump<<" m\n";
  for(int i=0;i<60;++i)g.update({},1.f/60);check(g.m_lockerDoors.empty(),"Behind the player the leaves swing shut on their own");
 }
 // The hands: every frame a leaf moves while the player pulls or shoves it,
 // each hand is on a leaf (within 6 cm of its face, at a height on it); the
 // gun is away for the whole way in and out and comes back up from below the
 // view only after the way out ends.
 {auto g=scene({{{10.5f,6.5f},0,600},{{4.2f,6.5f},0,4000}},kPi*.5f);g.m_player.ammo=std::max(g.m_player.ammo,8);g.m_weaponEquipped=true;g.m_holster=0;
  const auto&f=g.m_world.fixtures()[0];const float sz=f.depth/.9f;const Vec2 o=lockerOut(f),side{-o.y,o.x};
  auto onLeaf=[&](Point3 p){float best=9;for(int leaf=0;leaf<2;++leaf){const float hand=leaf?-1.f:1.f,a=g.lockerDoorOpen(0,0,leaf)*kPi*(110.f/180.f);
    const Vec2 hinge=f.position+o*(f.width*.5f)+side*((leaf?.4412f:.432f)*sz*hand),toward=side*(-hand*std::cos(a))+o*std::sin(a),q{p.x,p.y};
    const float along=std::clamp(dot(q-hinge,toward),0.f,(leaf?.443f:.43f)*sz);best=std::min(best,length(q-(hinge+toward*along)));}return p.z>.3f&&p.z<2.2f?best:9.f;};
  int moving=0,gripped=0;bool holstered=true,always=true;float worst=0;std::array<float,2> last{0,0};
  auto watch=[&]{std::array<Point3,2> grips;const bool hands=g.lockerHands(&grips);const std::array<float,2> now{g.lockerDoorOpen(0,0,0),g.lockerDoorOpen(0,0,1)};
   if(g.lockerViewLocked()||g.hiddenLocker()>=0)holstered&=g.unarmed()&&!g.m_weaponEquipped;if(std::fabs(now[0]-last[0])+std::fabs(now[1]-last[1])>1e-4f&&g.lockerViewLocked()){++moving;always&=hands;
    if(hands){const float d=std::max(onLeaf(grips[0]),onLeaf(grips[1]));worst=std::max(worst,d);gripped+=d<.06f;}}last=now;};
  bool in=hide(g);watch();for(int i=0;i<70&&g.lockerViewLocked();++i){g.update({},1.f/60);watch();}
  const int inMoving=moving,inGripped=gripped;for(int i=0;i<30;++i)g.update({},1.f/60);bool shutAway=g.unarmed()&&!g.lockerHands();
  g.update(use,1.f/60);last={0,0};watch();for(int i=0;i<40&&g.lockerViewLocked();++i){g.update({},1.f/60);watch();}
  const float atEnd=g.holster();for(int i=0;i<40;++i)g.update({},1.f/60);
  check(in&&inMoving>20&&inGripped==inMoving&&moving>inMoving+5&&gripped==moving&&always,"Every frame the leaves swing on the way in and out, both hands are on them (pulling open, pulling shut, shoving open)");
  report<<"  hands: "<<gripped<<"/"<<moving<<" moving frames gripped, worst "<<worst<<" m off a leaf\n";
  report<<"  gun: away "<<holstered<<shutAway<<", back "<<g.m_weaponEquipped<<", holster at the end of the way out "<<atEnd<<", then "<<g.holster()<<'\n';
  check(holstered&&shutAway&&g.m_weaponEquipped&&atEnd>.9f&&g.holster()<.02f,"The gun is away for the way in and out and comes back up from below the view once the way out ends");
 }
 // The locker's own play: a man checking the doors at arm's length takes both
 // leaves as the player bursts out, and goes down as a takedown leaves him,
 // thrown back off the doors; the bang carries to anyone near. A man alert on
 // the player braces: the doors only open.
 for(bool alert:{false,true}){auto g=scene({{{4.3f,6.5f},0,5000}},kPi*.5f);hide(g);for(int i=0;i<70;++i)g.update({},1.f/60);
  auto&npc=g.m_friendlyActors[0];const Vec2 o=lockerOut(g.m_world.fixtures()[0]),standing=lockerFront(g.m_world.fixtures()[0],.8f);
  npc.pose.position=standing;npc.pose.z=0;npc.pose.yaw=std::atan2(-o.x,-o.y);g.setState(g.friendlyKey(0,"guard_mode"),int(alert?GuardState::Alert:GuardState::Investigate));
  const bool aimed=g.lockerBurstTarget()==(alert?-1:0);const std::string hint=g.interactionHint()?g.interactionHint():"";
  g.update(use,1.f/60);bool bang=false,heard=false;for(const auto&s:g.sounds())bang|=s.sound==Sound::LockerBang&&length(s.position-standing)<.3f&&s.aiRadius>=8;
  for(const auto&s:g.aiStimuli())heard|=s.radius>=7.9f&&length(s.source-standing)<.3f;
  const bool steppedOut=g.hiddenLocker()<0&&g.lockerViewLocked();const float startAhead=dot(standing-lockerFront(g.m_world.fixtures()[0],0),o);
  for(int i=0;i<180;++i)g.update({},1.f/60);const auto box=g.friendlyShotBounds(0);const Vec2 middle{(box.first.x+box.second.x)*.5f,(box.first.y+box.second.y)*.5f};
  const bool fell=dot(middle-lockerFront(g.m_world.fixtures()[0],0),o)>startAhead+.2f;
  if(!alert)check(aimed&&hint=="E / BURST OUT   LMB / KNOCK"&&steppedOut&&bang&&heard&&g.npcIncapacitated(0)&&g.state(g.friendlyKey(0,"restraint"))==1&&g.npcActors()[0].pose.clip==4&&fell&&!g.state("locker_dead")&&g.hullFits(g.player().pos,g.player().z,Player::StandingHeight),
   "Bursting out into a man checking the doors 0.8 m off knocks him out, thrown back off them, and the bang reaches anyone within 8 m");
  else check(aimed&&hint=="E / STEP OUT   LMB / KNOCK"&&steppedOut&&!bang&&!g.npcIncapacitated(0),"A man alert on the player is not knocked down: the doors just open");
  report<<"  burst"<<(alert?" (alert)":"")<<": aimed "<<aimed<<" '"<<hint<<"' out "<<steppedOut<<" bang "<<bang<<" heard "<<heard<<" down "<<g.npcIncapacitated(0)<<" clip "<<g.npcActors()[0].pose.clip<<" phase "<<g.npcActors()[0].pose.phase<<", body "<<dot(middle-lockerFront(g.m_world.fixtures()[0],0),o)<<" m out from "<<startAhead<<'\n';
 }
 // Shut in, an area title card waits for the player to step out.
 {auto g=scene({{{10.5f,6.5f},0,5000}},kPi*.5f);hide(g);for(int i=0;i<70;++i)g.update({},1.f/60);g.m_areaTitle="TEST AREA";g.m_areaTitleTime=.4f;
  for(int i=0;i<120;++i)g.update({},1.f/60);const bool held=g.m_areaTitleTime==.4f&&g.areaTitle()=="TEST AREA";g.update(use,1.f/60);for(int i=0;i<40;++i)g.update({},1.f/60);
  check(held&&g.m_areaTitleTime>.9f,"An area title card waits while the player is shut in a locker and plays once they step out");
 }
 // Caught: the yank lands on the committed pose, facing the guard, inside half a second.
 {auto g=scene({{{8.5f,6.5f},0,5000}},-kPi*.5f);halfSeen(g);hide(g);int frames=0;for(;frames<60*15&&g.hiddenLocker()>=0;++frames)g.update({},1.f/60);
  const bool started=g.hiddenLocker()<0&&g.lockerViewLocked()&&g.m_lockerView.motion==LockerMotion::Caught;int held=0;for(;held<60&&g.lockerViewLocked();++held)g.update({},1.f/60);
  const bool open=g.lockerDoorOpen(0,0,0)>.8f&&g.lockerDoorOpen(0,0,1)>.8f;
  check(started&&held<=30&&length(g.view().pos-g.player().pos)<1e-5f&&std::fabs(wrapAngle(g.view().angle-g.player().angle))<1e-5f&&open,"Pulled out, the leaves are ripped back and the camera lands facing the guard, the controls back inside half a second");
 }
 // Every locker the campaign places is the double-door locker, and from in
 // front of its doors the player can get into each one.
 {std::string missing,levels;int lockers=0,banks=0;Game probe;
  for(int level=0;level<probe.chunkCount();++level){const World w=probe.makeWorld(level);int here=0;for(const auto&f:w.fixtures()){banks+=f.model==model::pwr_locker_bank;here+=f.model==model::psx_locker;}if(!here)continue;
   levels+=" "+std::to_string(level);auto g=mapInspection({12,12},0,0,level,false,-999,true);const auto&fixtures=g.m_world.fixtures();bool hid=false;
   for(size_t i=0;i<fixtures.size();++i){const auto&f=fixtures[i];if(f.model!=model::psx_locker)continue;++lockers;
    const Vec2 o=lockerOut(f),spot=lockerFront(f,.45f);g.m_player.pos=spot;g.m_player.z=g.m_world.floorHeight(f.position.x,f.position.y)+f.base;g.m_player.angle=std::atan2(-o.y,-o.x);g.m_player.grounded=true;
    if(g.lockerAhead()!=int(i)){missing+=" "+std::to_string(level)+"#"+std::to_string(i)+"@"+std::to_string(f.position.x).substr(0,5)+","+std::to_string(f.position.y).substr(0,5);continue;}
    if(!hid){Game h=g;hid=h.enterLocker()&&h.hiddenLocker()==int(i);if(!hid)missing+=" "+std::to_string(level)+"#"+std::to_string(i)+"(enter)";}}
  }
  check(lockers>0&&banks==0&&missing.empty(),"Every campaign locker is the double-door locker and each one can be got into from in front of its doors");
  report<<"  maps"<<levels<<": lockers "<<lockers<<", old banks "<<banks<<", unreachable"<<(missing.empty()?" none":missing)<<'\n';
 }
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

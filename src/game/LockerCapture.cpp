#include "Game.h"
#include "ai/Navigation.h"
#include "ai/Perception.h"
#include "../world/ModelIds.h"
#include "../world/CustomCampaign.h"
#include <cmath>
#include <fstream>
// Caught in a locker. A locker hides the player from eyes, not from whoever
// knows they are in it.
//
// A humanoid creature (the reactor's thing, the mutated human, the Snatcher,
// the transformed worker) that watched its prey shut itself in, or hears a
// knock from inside, walks to the doors, tears them back to their stops and
// takes hold: its face in the vents' place, a sting, and a struggle. USE,
// mashed, fills the struggle against its bites; full, it is thrown back off
// the doors, stunned, and the player is out. Nothing else ends it.
//
// A guard who hauls the player out (Stealth.cpp: pullFromLocker) no longer
// shoots: after the muzzle in the face he pins their arms, walks them along
// the bank of lockers to open floor, puts them on their knees and calls his
// squad to it. Nobody fires on a held prisoner; they talk it over. When they
// are done talking it is over for the player too, unless the same struggle
// breaks his grip first: he reels, the gun comes back up, and the fight is on
// the player's terms for a moment.
//
// None of it is saved: a save taken mid-hold loads with the player loose.
namespace retro {
namespace {
constexpr float GrappleReach=1.05f;                       // how near the doors it must stand to tear them open
constexpr float StruggleGain=.072f,StruggleDrain=.16f;    // a press, and what a second of not pressing loses
constexpr float FirstBite=.6f,BiteEvery=1.15f,BiteCost=.06f,ThrownOff=2.2f;
constexpr float GripGain=.058f,GripDrain=.13f;             // a guard's hold is a little harder to break
constexpr float PinSeconds=1.2f,MarchSpeed=1.25f,Behind=.62f,MarchLimit=9.f;
constexpr float KneelSeconds=30.f,VerdictSeconds=1.8f,TalkEvery=3.2f;
// What is said over the prisoner. The men's recorded lines are short barks, so
// each is captioned with what it means here: the captor alone until someone
// answers his call, then the two of them.
struct Said {int who;NpcVoiceCue cue;const char* caption;}; // who: 0 the captor, 1 whoever came
constexpr Said Alone[]={
 {0,NpcVoiceCue::Found,"CONTROL, I HAVE THE GUARD. LOCKER ROW. SEND WHO YOU HAVE."},
 {0,NpcVoiceCue::Warn,"STAY DOWN. THEY ARE COMING."},
 {0,NpcVoiceCue::Witness,"EVERYBODY PICKS A LOCKER. I DO NOT KNOW WHY."}};
constexpr Said Together[]={
 {1,NpcVoiceCue::Witness,"THIS IS THE ONE OFF THE LIST?"},
 {0,NpcVoiceCue::Found,"BADGE SAYS SECURITY. THE LIST SAYS NOBODY."},
 {1,NpcVoiceCue::Alert,"THEN WHY IS SHE STILL BREATHING?"},
 {0,NpcVoiceCue::Warn,"I WANTED SOMEBODY TO SEE IT DONE RIGHT."}};
constexpr const char* VerdictSaid="ALL RIGHT. LOOK AT THE FLOOR.";
Vec2 outOf(const Fixture& f){return {std::cos(f.yaw),-std::sin(f.yaw)};}
Vec2 mouthOf(const Fixture& f,float ahead){return f.position+outOf(f)*(f.width*.5f+ahead);}
bool hidingLocker(const Fixture& f){return f.model==model::psx_locker&&f.solid&&f.visible;}
bool stalkerHold(Enemy::Kind kind){return kind==Enemy::Kind::Snatcher||kind==Enemy::Kind::Transformed;}
// Where its face is while it stands at the doors, as a share of its height: the
// Snatcher stoops, the rest stand over their prey (the transformed worker with
// both fists up, the raise of its overhead blow held: at rest it is doubled over).
float faceShare(Enemy::Kind kind){return kind==Enemy::Kind::Snatcher?.7f:kind==Enemy::Kind::Transformed?.84f:.88f;}
}
// The ones with hands: they work a latch, and they know what a locker is for.
bool Game::lockerHunter(CreatureKind kind){return kind==CreatureKind::Warden||kind==CreatureKind::Mutant||kind==CreatureKind::Snatcher||kind==CreatureKind::Transformed;}
bool Game::captured()const{return m_capture.kind!=LockerCapture::Kind::None&&!dead();}
float Game::captureStruggle()const{return captured()?m_capture.struggle:-1.f;}
const char* Game::captureHint()const{
 if(!captured()||lockerViewLocked())return nullptr;
 return m_capture.kind==LockerCapture::Kind::Creature?"E / FIGHT IT OFF":"E / STRUGGLE";
}
// On the way in (enterLocker): every hunter that has the player made out and
// can see where they stood knows which doors closed on them.
void Game::creaturesWatchLocker(int index,const Player& before){
 for(auto&e:m_enemies){if(!e.alive||e.wakeState||!lockerHunter(e.kind)||e.awareness<=0||e.recognition<1)continue;const auto&traits=creatureTraits(e.kind);
  const Vec2 facing{std::cos(e.heading),std::sin(e.heading)};
  if(aiCanSee(m_world,e.pos,e.z+.7f,facing,before.pos,before.z+before.eye,traits.sightRange,traits.sightDot,2.5f))e.lockerHunt=index+1;}
}
// Its senses, each frame (updateEnemies): a knock from inside is a sound with
// a box around it, and a hunter that knows the box keeps its mind on the doors.
void Game::creatureLockerSense(Enemy& e){
 if(!lockerHunter(e.kind))return;const int index=hiddenLocker();
 if(index<0||(captured()&&m_capture.kind==LockerCapture::Kind::Guard)){e.lockerHunt=0;return;}
 const auto&f=m_world.fixtures()[size_t(index)];const auto&traits=creatureTraits(e.kind);
 if(e.lockerHunt!=index+1){e.lockerHunt=0;
  for(const auto&s:m_sounds){if(s.sound!=Sound::LockerKnock)continue;const float reach=s.radius*traits.hearingScale,dz=(std::isnan(s.z)?e.z:s.z)-e.z-.7f;
   if(lengthSq(s.position-e.pos)+dz*dz<reach*reach){e.lockerHunt=index+1;enemySound(e,0,.9f);break;}}}
 if(e.lockerHunt!=index+1)return;
 e.lastKnown=mouthOf(f,.7f);e.lastKnownZ=m_world.floorHeight(f.position.x,f.position.y)+f.base;e.awareness=std::max(e.awareness,6.f);
 if(e.state==Enemy::State::Idle||e.state==Enemy::State::Search)e.state=Enemy::State::Investigate;
}
// True while this frame is the hold's (or the stun after it): the creature
// does nothing else. At the doors of the locker it hunts, it takes hold.
bool Game::creatureLockerHold(Enemy& e,float dt){
 const int self=int(&e-m_enemies.data());
 if(e.lockerStun>0){e.lockerStun=std::max(0.f,e.lockerStun-dt);e.moving=false;if(e.lockerStun<=0&&stalkerHold(e.kind))e.stalkMode=Enemy::StalkMode::Watch;return true;}
 if(m_capture.kind==LockerCapture::Kind::Creature&&m_capture.who==self&&!dead()){
  const Vec2 to=m_player.pos-e.pos;if(lengthSq(to)>.01f)e.heading=std::atan2(to.y,to.x);e.moving=false;
  e.windup=e.kind==Enemy::Kind::Transformed?creatureTraits(e.kind).windup*.3f:0.f;return true;}
 if(!e.lockerHunt||captured()||dead()||m_vrInputActive)return false;
 const int index=hiddenLocker();if(index!=e.lockerHunt-1||lockerViewLocked())return false;
 const auto&f=m_world.fixtures()[size_t(index)];const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 if(length(e.pos-mouthOf(f,0))>GrappleReach*std::max(1.f,e.scale)||std::fabs(e.z-floor)>.5f)return false;
 beginCreatureGrapple(self,index);return true;
}
void Game::beginCreatureGrapple(int self,int index){
 auto&e=m_enemies[size_t(self)];const auto&f=m_world.fixtures()[size_t(index)];const auto&traits=creatureTraits(e.kind);
 const Vec2 out=outOf(f),mouth=mouthOf(f,0);const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 m_capture={};m_capture.kind=LockerCapture::Kind::Creature;m_capture.who=self;m_capture.locker=index;m_capture.bite=FirstBite;m_capture.lastUse=true;
 e.heading=std::atan2(-out.y,-out.x);e.windup=0;e.strike=1;e.attackCooldown=traits.cooldown;e.moving=false;if(stalkerHold(e.kind))e.stalkMode=Enemy::StalkMode::Watch; // on its feet at the doors, not down on a pin
 // The doors go back on their stops with a bang anyone near hears; the sting and the lurch are the player's.
 animateLockerDoor(m_level,index,LockerMotion::Caught);
 SoundEvent bang{Sound::LockerBang,mouth,.95f,.96f,true,14,floor+1.2f};bang.aiRadius=8;m_sounds.push_back(bang);
 m_sounds.push_back({Sound::LockerSting,{},.85f,.94f,false});m_alarmSting=std::max(m_alarmSting,.6f);enemySound(e,1,1.f,1.08f);
 m_verticalSpringVelocity-=1.1f;m_damageFlash=std::max(m_damageFlash,.6f);
 // The eyes go to its face: all there is to see through the open doors.
 m_player.angle=std::atan2(out.y,out.x);const float face=e.z+creatureTraits(e.kind).top*e.scale*faceShare(e.kind)-(m_player.z+m_player.eye);
 m_player.pitch=std::clamp(std::atan2(face,std::max(.35f,length(e.pos-m_player.pos)))*140.f,-80.f,50.f);
 m_lockerRearm|=m_weaponEquipped;m_pickupNotice="IT HAS YOU";m_pickupNoticeTime=1.6f;
}
// A guard has the player out of the locker (pullFromLocker's last line): where
// he will walk them. Along the bank of lockers, whichever way is open for
// longer, with room behind the player for him; nowhere to go, he holds them
// where they stand.
void Game::beginGuardCapture(size_t actor,int index){
 if(m_vrInputActive||actor>=m_friendlyActors.size()||size_t(index)>=m_world.fixtures().size())return;
 const auto&f=m_world.fixtures()[size_t(index)];const Vec2 out=outOf(f),side{-out.y,out.x},from=m_player.pos;const float floor=m_player.z;
 m_capture={};m_capture.kind=LockerCapture::Kind::Guard;m_capture.who=int(actor);m_capture.locker=index;m_capture.lastUse=true;m_capture.spot=from;m_capture.dir=out;
 auto open=[&](Vec2 d,float reach){
  if(!m_world.fits(from.x-d.x*Behind,from.y-d.y*Behind,floor,1.6f))return false;
  // (Clear a pace past the spot too: nobody is walked nose-first into a wall.)
  for(float t=.2f;t<=reach+.9f;t+=.2f){const Vec2 p=from+d*t;if(p.x<.6f||p.y<.6f||p.x>World::Width-.6f||p.y>World::Height-.6f)return false;
   const float z=groundHeight(p,floor+.1f);if(std::fabs(z-floor)>.2f||!hullFits(p,z,Player::StandingHeight))return false;}
  return m_world.rayClear(from,floor+1.f,from+d*reach,floor+1.f,true);};
 for(float reach:{3.4f,2.8f,2.2f,1.6f}){for(float sign:{1.f,-1.f})if(open(side*sign,reach)){m_capture.march=true;m_capture.dir=side*sign;m_capture.spot=from+side*(sign*reach);break;}if(m_capture.march)break;}
 m_handsLowered=true;m_pickupNotice="ARMS PINNED";m_pickupNoticeTime=1.6f;
}
// The captor's own intent while he has the player (guardIntent): behind them
// on the march, a step and a half off with the gun on them once they kneel.
// His squad, once his call has reached them, comes to stand round the prisoner.
bool Game::captorIntent(int level,size_t actor,ActorPose& pose,float dt,Vec2& goal,float& goalZ,bool& wantsMove){
 const auto&c=m_capture;if(c.kind!=LockerCapture::Kind::Guard||dead())return false;
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
 const Vec2 player=m_player.pos+chunkOffset(m_level)-chunkOffset(level);
 if(c.who!=int(actor)){
  // A squadmate who has the report: to his place in the ring, a little over two metres off, eyes on them.
  const bool kneeling=c.stage==LockerCapture::Stage::Kneel||c.stage==LockerCapture::Stage::Verdict;
  if(!kneeling||!guardHostileToPlayer(actor)||!get("guard_confirmed")||get("guard_target_kind")!=int(ShotKind::Player)||std::fabs(pose.z-m_player.z)>1.2f||length(pose.position-player)>20.f)return false;
  const Vec2 captor=size_t(c.who)<m_friendlyActors.size()?m_friendlyActors[size_t(c.who)].pose.position+chunkOffset(m_friendlyActors[size_t(c.who)].level)-chunkOffset(level)-player:Vec2{1,0};
  const float base=std::atan2(captor.y,captor.x),turn=(float(actor%5)+1.f)*(kPi*2/6.f);Vec2 want=player+Vec2{std::cos(base+turn),std::sin(base+turn)}*2.3f;float feet=pose.z;
  if(!friendlyWalkSegment(level,player,m_player.z,want,1.6f,&feet)){const Vec2 off=pose.position-player;const float d=length(off);want=d>2.3f?player+off*(2.3f/d):pose.position;}
  goal=pose.position;goalZ=pose.z;wantsMove=false;
  if(length(want-pose.position)>.25f){goal=want;goalZ=m_player.z;wantsMove=true;}
  else{const Vec2 toward=player-pose.position;if(lengthSq(toward)>.001f)pose.yaw+=std::clamp(std::remainder(std::atan2(toward.x,toward.y)-pose.yaw,2*kPi),-6*dt,6*dt);}
  return true;
 }
 goal=pose.position;goalZ=pose.z;wantsMove=false;Vec2 want=pose.position;
 // He has them, and knows it: his memory stays on them, and the call he keys
 // (the kneel) runs its second and a half and lands like any other.
 {const Vec2 global=m_player.pos+chunkOffset(m_level);put("guard_mode",int(GuardState::Alert));put("guard_seen",1);put("guard_confirmed",1);put("guard_memory",1);
  put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(m_player.z*1000)));
  if(guardCalling(actor)){const int radio=std::min(RadioCallMs,get("guard_radio")+std::max(1,int(std::round(dt*1000))));put("guard_radio",radio);
   if(radio>=RadioCallMs)m_sounds.push_back({Sound::RadioSquelch,pose.position+chunkOffset(level)-chunkOffset(m_level),.6f,1.15f,true,14,pose.z+1.4f});}}
 if(c.stage==LockerCapture::Stage::March)want=player-c.dir*Behind;
 else if(c.stage!=LockerCapture::Stage::Pin){const Vec2 off=pose.position-player;const float d=length(off);if(d<1.2f||d>1.9f)want=player+(d>.05f?off*(1.f/d):c.dir*-1.f)*1.5f;}
 if(c.stage!=LockerCapture::Stage::Pin&&length(want-pose.position)>.12f){goal=want;goalZ=m_player.z;wantsMove=true;}
 else{const Vec2 toward=player-pose.position;if(lengthSq(toward)>.001f)pose.yaw+=std::clamp(std::remainder(std::atan2(toward.x,toward.y)-pose.yaw,2*kPi),-6*dt,6*dt);}
 return true;
}
void Game::endGuardCapture(bool broke){
 auto&c=m_capture;const int who=c.who;c={};m_previousUse=true;
 for(size_t a=0;a<m_friendlyActors.size();++a){const auto&npc=m_friendlyActors[a];if(npc.track().ai.mode!=ActorAiMode::Guard||!chunkResident(npc.level))continue;
  // Broken free, the squad takes a beat to believe it; let go for the verdict, no beat at all.
  setState(friendlyKey(a,"guard_hold_ms"),broke?(int(a)==who?1150:650):0);}
 if(broke&&who>=0&&size_t(who)<m_friendlyActors.size()){
  // An elbow back into him: he takes it, reels a step and has no gun on anyone for a second.
  auto&npc=m_friendlyActors[size_t(who)];const Vec2 his=npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level);
  damageFriendly(size_t(who),18,m_player.pos+chunkOffset(m_level)-chunkOffset(npc.level),{ShotKind::Player,m_level,0});
  const Vec2 away=normalized(his-m_player.pos);float feet=npc.pose.z;const Vec2 to=npc.pose.position+away*.45f;
  if(lengthSq(away)>.5f&&friendlyWalkSegment(npc.level,npc.pose.position,npc.pose.z,to,npc.track().scale,&feet)&&std::fabs(feet-npc.pose.z)<.3f){npc.pose.position=to;npc.pose.z=feet;
   setState(friendlyKey(size_t(who),"x"),int(std::round(to.x*1000)));setState(friendlyKey(size_t(who),"y"),int(std::round(to.y*1000)));}
  setState(friendlyKey(size_t(who),"weapon_raise"),0);setState(friendlyKey(size_t(who),"weapon_aim"),0);
  m_sounds.push_back({Sound::PunchHit,his,.8f,.9f,true,12,m_player.z+1.2f});m_verticalSpringVelocity-=.6f;
  // The view comes round on him.
  if(lengthSq(his-m_player.pos)>.01f)m_player.angle=std::atan2(his.y-m_player.pos.y,his.x-m_player.pos.x);m_player.pitch=0;
  m_pickupNotice="BROKE FREE";m_pickupNoticeTime=1.4f;
 }
 if(broke&&m_player.crouched&&hullFits(m_player.pos,m_player.z,Player::StandingHeight)){m_player.crouched=false;m_player.eye=Player::StandingEye;}
 lockerRearm();
}
// Each frame while held (updateStealth). True: the frame was the hold's.
bool Game::updateCapture(const InputState& input,float dt){
 auto&c=m_capture;if(c.kind==LockerCapture::Kind::None)return false;
 if(dead()){c={};return false;}
 const bool press=input.use&&!c.lastUse&&!lockerViewLocked();c.lastUse=input.use;c.age+=dt;c.stageAge+=dt;c.captionAge+=dt;
 m_suppressFire=true;m_velocity={};m_stepDistance=0;
 auto look=[&](float facing,float yaw,float pitch){m_player.angle=wrapAngle(facing+std::clamp(wrapAngle(m_player.angle-facing),-yaw,yaw));m_player.pitch=std::clamp(m_player.pitch,-pitch,pitch);};
 if(c.kind==LockerCapture::Kind::Creature){
  const int index=hiddenLocker();
  if(c.who<0||size_t(c.who)>=m_enemies.size()||!m_enemies[size_t(c.who)].alive||index!=c.locker||index<0){c={};return index>=0;}
  auto&e=m_enemies[size_t(c.who)];const auto&traits=creatureTraits(e.kind);const auto&f=m_world.fixtures()[size_t(index)];const Vec2 out=outOf(f);
  // The eyes stay on its face: a little to either side, no looking away.
  {const float face=e.z+traits.top*e.scale*faceShare(e.kind)-(m_player.z+m_player.eye),want=std::clamp(std::atan2(face,std::max(.35f,length(e.pos-m_player.pos)))*140.f,-80.f,50.f);
   look(std::atan2(out.y,out.x),.4f,90.f);m_player.pitch=want+std::clamp(m_player.pitch-want,-14.f,14.f);}
  m_playerExposure=0;holdLockerDoors(index);
  if(press){c.struggle=std::min(1.f,c.struggle+StruggleGain);m_verticalSpringVelocity-=.14f;
   m_sounds.push_back({Sound::LockerKnock,mouthOf(f,.15f),.4f,.86f+.04f*float(int(c.age*9)%4),true,12,m_player.z+1.1f});}
  else c.struggle=std::max(0.f,c.struggle-StruggleDrain*dt);
  c.bite-=dt;
  if(c.bite<=0){c.bite=BiteEvery;receiveDamage(traits.damage*.5f,e.pos,false);e.strike=1;enemySound(e,1,.8f,.97f+.05f*float(int(c.age*3)%3));c.struggle=std::max(0.f,c.struggle-BiteCost);m_verticalSpringVelocity-=.55f;
   if(dead()){c={};return true;}}
  if(c.struggle>=1){
   // Thrown back off the doors: a step, a stumble, and time to get out and away.
   e.lockerStun=ThrownOff;e.lockerHunt=0;e.painFlash=1;e.windup=0;e.strike=0;e.attackCooldown=std::max(e.attackCooldown,ThrownOff);if(stalkerHold(e.kind)){e.stalkMode=Enemy::StalkMode::Recover;e.stalkTimer=ThrownOff;}
   float feet=e.z;const Vec2 to=e.pos+out*.9f;if(aiWalkSegment(m_world,e.pos,e.z,to,e.hull(),&feet)&&std::fabs(feet-e.z)<=traits.step+.025f){e.pos=to;e.z=feet;}
   enemySound(e,2,.9f);m_sounds.push_back({Sound::PunchHit,e.pos,.8f,.85f,true,12,e.z+1.2f});
   c={};m_previousUse=true;leaveLocker();m_pickupNotice="BROKE FREE";m_pickupNoticeTime=1.4f;
  }
  return true;
 }
 // --- A guard's hold ---
 const bool gone=c.who<0||size_t(c.who)>=m_friendlyActors.size();
 if(gone||npcIncapacitated(size_t(c.who))||!chunkResident(m_friendlyActors[size_t(c.who)].level)||m_friendlyActors[size_t(c.who)].pose.clip==4||
    (m_friendlyActors[size_t(c.who)].track().deadState&&state(m_friendlyActors[size_t(c.who)].track().deadState))){endGuardCapture(false);return false;}
 auto&npc=m_friendlyActors[size_t(c.who)];const Vec2 his=npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level);
 // Nobody fires on a held prisoner: every gun that is on the player waits.
 for(size_t a=0;a<m_friendlyActors.size();++a){const auto&other=m_friendlyActors[a];if(other.track().ai.mode!=ActorAiMode::Guard||!chunkResident(other.level)||npcIncapacitated(a))continue;
  // (A man with something else in his sights, a creature, keeps his own counsel.)
  const int aim=state(friendlyKey(a,"guard_target_kind"));if(aim!=int(ShotKind::None)&&aim!=int(ShotKind::Player))continue;
  const auto key=friendlyKey(a,"guard_hold_ms");if(state(key)<400)setState(key,400);}
 m_handsLowered=true;
 if(press){c.struggle=std::min(1.f,c.struggle+GripGain);m_verticalSpringVelocity-=.1f;}else c.struggle=std::max(0.f,c.struggle-GripDrain*dt);
 if(c.struggle>=1){endGuardCapture(true);return true;}
 const Vec2 toHim=his-m_player.pos;const float himYaw=std::atan2(toHim.y,toHim.x);
 auto turn=[&](float want,float rate){m_player.angle=wrapAngle(m_player.angle+std::remainder(want-m_player.angle,2*kPi)*std::min(1.f,dt*rate));};
 switch(c.stage){
 case LockerCapture::Stage::Pin:
  look(himYaw,.5f,50.f);
  if(c.stageAge>=PinSeconds&&!lockerViewLocked()){c.stage=c.march?LockerCapture::Stage::March:LockerCapture::Stage::Kneel;c.stageAge=0;
   if(c.march){m_verticalSpringVelocity-=.5f;sound(Sound::PunchHit,.3f,.7f);m_pickupNotice="MOVE";m_pickupNoticeTime=1.2f;}}
  break;
 case LockerCapture::Stage::March:{
  // Turned to the way he wants them to go, and walked there while he is at their back.
  const float way=std::atan2(c.dir.y,c.dir.x);if(c.stageAge<.6f)turn(way,7.f);look(way,1.1f,55.f);
  const float left=dot(c.spot-m_player.pos,c.dir);const bool pushed=length(toHim)<1.2f&&dot(toHim,c.dir)<.1f;
  bool stopped=left<.05f||c.stageAge>MarchLimit;
  if(!stopped&&pushed){const float step=std::min(MarchSpeed*dt,left);const Vec2 p=m_player.pos+c.dir*step;const float z=groundHeight(p,m_player.z+.1f);
   if(std::fabs(z-m_player.z)<.3f&&hullFits(p,z,Player::StandingHeight)){m_player.pos=p;m_player.z=z;c.walked+=step;
    if(c.walked>=.62f){c.walked=0;SoundEvent foot{Sound(int(stepSoundAt(m_world,p))+(c.foot++&3)),p,.4f,1.f,false};m_sounds.push_back(foot);m_verticalSpringVelocity-=.12f;}}
   else stopped=true;}
  if(stopped){c.stage=LockerCapture::Stage::Kneel;c.stageAge=0;}
  break;}
 case LockerCapture::Stage::Kneel:
  // Shoved down where he wants them, and the call goes out.
  if(!c.called){c.called=true;m_verticalSpringVelocity-=1.f;sound(Sound::PunchHit,.35f,.62f);m_player.crouched=true;startGuardCall(size_t(c.who),CallSighting);c.talk=1.6f;m_pickupNotice="ON YOUR KNEES";m_pickupNoticeTime=1.6f;}
  m_player.eye+=(Player::CrouchedEye-m_player.eye)*std::min(1.f,dt*7.f);
  // On their knees they look up at him.
  if(c.stageAge<1.2f&&length(toHim)<3.f){turn(himYaw,4.f);const float face=npc.pose.z+npc.track().scale*.82f-(m_player.z+m_player.eye);
   m_player.pitch+=(std::clamp(std::atan2(face,std::max(.4f,length(toHim)))*140.f,-50.f,60.f)-m_player.pitch)*std::min(1.f,dt*4.f);}
  {// Whoever has come stands over them and they talk: the captor, then each arrival in turn.
   std::vector<size_t> circle{size_t(c.who)};
   for(size_t a=0;a<m_friendlyActors.size();++a){const auto&other=m_friendlyActors[a];if(int(a)==c.who||other.track().ai.mode!=ActorAiMode::Guard||!chunkResident(other.level)||npcIncapacitated(a)||other.pose.clip==4||!guardHostileToPlayer(a))continue;
    const Vec2 at=other.pose.position+chunkOffset(other.level)-chunkOffset(m_level);if(length(at-m_player.pos)<5.f&&std::fabs(other.pose.z-m_player.z)<1.2f)circle.push_back(a);}
   auto say=[&](size_t speaker,const Said& said){npcVoice(speaker,said.cue);c.caption=said.caption;c.captionAge=0;c.talk=TalkEvery;};
   c.talk-=dt;
   if(c.talk<=0){
    if(circle.size()>1&&size_t(c.teamLine)<std::size(Together)){const auto&said=Together[size_t(c.teamLine)];say(said.who?circle[1+size_t(c.teamLine/2)%(circle.size()-1)]:circle[0],said);++c.teamLine;++c.line;}
    else if(circle.size()==1&&size_t(c.line)<std::size(Alone)){say(circle[0],Alone[size_t(c.line)]);++c.line;}
    else if(size_t(c.teamLine)>=std::size(Together))c.team=1; // said what they came to say
   }
  }
  if(c.team>0||c.stageAge>=KneelSeconds){c.stage=LockerCapture::Stage::Verdict;c.stageAge=0;npcVoice(size_t(c.who),NpcVoiceCue::Alert);c.caption=VerdictSaid;c.captionAge=0;m_sounds.push_back({Sound::Pump,his,.8f,1.f,true,12,npc.pose.z+1.2f});}
  break;
 case LockerCapture::Stage::Verdict:
  m_player.eye+=(Player::CrouchedEye-m_player.eye)*std::min(1.f,dt*7.f);
  if(c.stageAge>=VerdictSeconds){endGuardCapture(false);return false;}
  break;
 }
 return true;
}
// --locker-capture-shots: a lit room, the player a step in front of a locker
// (doors east) and one creature down the room.
Game Game::lockerCaptureScene(int creature){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Locker capture inspection|0\nMAP|0|Lockers|0|0|3.2|6.5|0|0|4.5|0.7|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+(y<3||y>10?std::string(24,'#'):"#"+std::string(12,'.')+std::string(11,'#'))+'\n';
 t+="LIGHT|0|4|6.5|3.2|0|0|2.2|11|4000\nLIGHT|0|9|6.5|3.2|0|0|2.2|11|4000\n";
 for(float y:{5.63f,6.5f,7.37f})t+="FIXTURE|0|"+std::to_string(model::psx_locker)+"|1.44|"+std::to_string(y)+"|0|0.87|0.85|2.42|0|1\n";
 if(creature>=0)t+="CREATURE|0|"+std::to_string(creature)+"|10|6.5|0\n";
 Game g(loadCustomCampaignText(t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n","locker-capture.rmc"));
 g.m_player.pos={2.6f,6.5f};g.m_player.z=0;g.m_player.angle=kPi;g.m_player.pitch=0;return g;
}
// --- Tests ---------------------------------------------------------------------------
bool Game::testLockerCapture(){
 std::ofstream report("locker-capture-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const std::string& text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 // testLockerGuards' room: the locker at the west end, doors east, the player a step in front.
 struct Post {Vec2 at;float yaw;};
 auto scene=[](std::vector<CreatureSpawn> creatures,std::vector<Post> guards={}){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Locker capture validation";campaign->key=883019;
  auto map=std::make_shared<AuthoredMapData>();map->name="Locker capture room";map->definition={{0,0},{3.2f,6.5f},0,Environment::Interior,false,8,.3f,44};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  map->fixtures.push_back({model::psx_locker,{2.f,6.5f},0,.87f,.85f,2.42f,0,true});
  for(const auto&c:creatures)map->creatureSpawns.push_back(c);
  TimedSequence timer;timer.timerState=stateId("lc_clock");timer.finishState=stateId("lc_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  int n=0;for(const auto&post:guards){ActorTrack track;track.timerState=timer.timerState;track.visual=ActorVisual::Cleanup;track.keys={{0,post.at,0,post.yaw},{30000,post.at,0,post.yaw}};track.scale=1.6f;track.health=90;
   track.damageState=stateId(("lc_damage"+std::to_string(n)).c_str());track.deadState=stateId(("lc_dead"+std::to_string(n)).c_str());track.ai.mode=ActorAiMode::Guard;track.ai.armed=true;track.ai.patrol={{post.at,0,60000}};track.ai.squad=stateId("lc_squad");map->actorTracks.push_back(track);++n;}
  campaign->maps.push_back(map);Game g(campaign);g.m_player.pos={3.2f,6.5f};g.m_player.z=0;g.m_player.angle=kPi;g.m_player.pitch=0;return g;
 };
 InputState use;use.use=true;InputState knock;knock.fire=true;const float frame=1.f/60;
 auto hide=[&](Game& g){g.update(use,frame);g.update({},frame);for(int i=0;i<70;++i)g.update({},frame);return g.hiddenLocker()>=0&&!g.lockerViewLocked();};
 // About seven presses a second: what a hand does.
 auto mash=[&](Game& g,int frames){for(int i=0;i<frames;++i)g.update(i%8<4?use:InputState{},frame);};
 auto heard=[](Game& g,Sound s){for(const auto&e:g.sounds())if(e.sound==s)return true;return false;};
 // 1. A knock brings a mutated human to the doors, and it takes hold.
 {auto g=scene({{CreatureKind::Mutant,{9.f,6.5f}}});const bool hidden=hide(g);const bool quiet=g.m_enemies.size()==1&&!g.m_enemies[0].lockerHunt;
  g.update(knock,frame);g.update({},frame);const bool hunting=g.m_enemies[0].lockerHunt==1;
  bool grabbed=false,sting=false,bang=false;int at=0;for(;at<60*15&&!grabbed;++at){g.update({},frame);grabbed=g.m_capture.kind==LockerCapture::Kind::Creature;sting|=heard(g,Sound::LockerSting);bang|=heard(g,Sound::LockerBang);}
  const float open=std::max(g.lockerDoorOpen(g.m_level,0,0),g.lockerDoorOpen(g.m_level,0,1));for(int i=0;i<12;++i)g.update({},frame);const float wide=std::max(g.lockerDoorOpen(g.m_level,0,0),g.lockerDoorOpen(g.m_level,0,1));
  report<<"  hidden "<<hidden<<" quiet "<<quiet<<" hunting "<<hunting<<" grabbed after "<<at*frame<<" s, doors "<<open<<" -> "<<wide<<", creature "<<length(g.m_enemies[0].pos-g.m_player.pos)<<" m off\n";
  check(hidden&&quiet&&hunting&&grabbed&&sting&&bang&&wide>.8f&&g.hiddenLocker()==0&&std::string(g.interactionHint()?g.interactionHint():"")=="E / FIGHT IT OFF","A knock from inside brings a mutated human to the doors: it tears them open, the sting plays and it has the player");
  // Held, it bites, and the doors stay on their stops.
  const float before=g.m_player.health;for(int i=0;i<60*4;++i)g.update({},frame);const float bitten=before-g.m_player.health;
  const float held=std::max(g.lockerDoorOpen(g.m_level,0,0),g.lockerDoorOpen(g.m_level,0,1));
  check(bitten>=16&&bitten<=40&&g.m_capture.kind==LockerCapture::Kind::Creature&&held>.8f&&g.hiddenLocker()==0,"Left alone it bites every second or so ("+std::to_string(int(bitten))+" health in 4 s) and the doors stay open");
  // Mashing USE throws it off; the player is out and it is stunned.
  int frames=0;for(;frames<60*8&&g.m_capture.kind!=LockerCapture::Kind::None;frames+=8)mash(g,8);
  const bool out=g.hiddenLocker()<0&&g.m_capture.kind==LockerCapture::Kind::None,stunned=g.m_enemies[0].lockerStun>1.5f&&!g.m_enemies[0].lockerHunt;const float health=g.m_player.health;
  for(int i=0;i<90;++i)g.update({},frame);
  report<<"  thrown off after "<<frames*frame<<" s of mashing; health "<<health<<" -> "<<g.m_player.health<<"\n";
  check(out&&stunned&&frames>60&&frames<60*5&&g.m_player.health==health&&g.captureStruggle()<0,"Mashing USE throws it back off the doors in "+std::to_string(frames*frame).substr(0,3)+" s: the player is out and it does nothing for a second and a half");
  for(int i=0;i<60*4;++i)g.update({},frame);
  check(g.m_enemies[0].lockerStun<=0&&g.m_enemies[0].awareness>0,"The stun over, it hunts again");
 }
 // 2. One that watched the player climb in needs no knock.
 {auto g=scene({{CreatureKind::Transformed,{8.f,6.5f}}});auto&e=g.m_enemies[0];e.heading=kPi;e.recognition=1;e.awareness=6;e.state=Enemy::State::Chase;e.lastKnown=g.m_player.pos;e.attackCooldown=30;
  g.update(use,frame);const bool saw=g.hiddenLocker()==0&&g.m_enemies[0].lockerHunt==1;
  bool grabbed=false;for(int i=0;i<60*15&&!grabbed;++i){g.update({},frame);grabbed=g.m_capture.kind==LockerCapture::Kind::Creature;}
  check(saw&&grabbed,"A transformed worker that watched the player shut themselves in comes straight for the doors");}
 // 3. Seen by nothing, heard by nothing: the locker still hides.
 {auto g=scene({{CreatureKind::Mutant,{9.f,6.5f}}});g.m_enemies[0].heading=0;hide(g);bool left=true;for(int i=0;i<60*8;++i){g.update({},frame);left&=!g.m_enemies[0].lockerHunt&&g.m_capture.kind==LockerCapture::Kind::None;}
  check(left&&g.hiddenLocker()==0,"Unseen and silent, the player is left alone in the locker");}
 // 4. The ones without hands do not work doors.
 {auto g=scene({{CreatureKind::Huntsman,{7.f,6.5f}}});hide(g);g.update(knock,frame);bool left=true;for(int i=0;i<60*8;++i){g.update({},frame);left&=!g.m_enemies[0].lockerHunt&&g.m_capture.kind==LockerCapture::Kind::None;}
  check(left&&g.hiddenLocker()==0&&!lockerHunter(CreatureKind::Chimera)&&!lockerHunter(CreatureKind::Wasp)&&!lockerHunter(CreatureKind::Brute),"A huntsman hears the knock and cannot open the doors (nor can the wasp, the fiend or the chimera)");}
 // 5. A guard hauls the player out, pins them, walks them along the lockers and calls it in; nobody fires.
 {auto g=scene({},{{{6.f,6.5f},-kPi*.5f},{{14.f,10.f},-kPi*.5f}});hide(g);const bool caught=g.stageLockerCatch()&&g.m_capture.kind==LockerCapture::Kind::Guard&&g.m_capture.who==0;
  const Vec2 from=g.m_player.pos;const bool planned=g.m_capture.march&&std::fabs(g.m_capture.dir.x)<.01f;
  for(int i=0;i<60*2;++i)g.update({},frame);const bool marching=g.m_capture.stage==LockerCapture::Stage::March;const bool lowered=g.handsLowered()&&!g.weaponEquipped();
  bool behind=false;int i=0;for(;i<60*12&&g.m_capture.stage==LockerCapture::Stage::March;++i){g.update({},frame);const Vec2 his=g.npcActors()[0].pose.position-g.m_player.pos;behind|=length(his)<1.2f&&dot(his,g.m_capture.dir)<0;}
  const float walked=length(g.m_player.pos-from);const bool knelt=g.m_capture.stage==LockerCapture::Stage::Kneel;
  for(int k=0;k<30;++k)g.update({},frame);const bool called=g.state(g.friendlyKey(0,"guard_call"))==CallSighting,down=g.m_player.crouched&&g.m_player.eye<1.2f;
  report<<"  caught "<<caught<<" planned "<<planned<<" marching "<<marching<<" behind "<<behind<<" walked "<<walked<<" m in "<<i*frame<<" s, knelt "<<knelt<<" called "<<called<<"\n";
  check(caught&&planned&&marching&&lowered&&behind&&walked>1.4f&&knelt&&called&&down&&std::string(g.interactionHint()?g.interactionHint():"")=="E / STRUGGLE",
   "Hauled out by a guard: arms pinned, walked "+std::to_string(walked).substr(0,3)+" m along the lockers with him at their back, put on their knees, and he radios it in");
  // Through the whole hold nobody fires, a squadmate comes, and they talk.
  const float health=g.m_player.health;bool came=false,spoke=false,captioned=false;int held=0;
  for(;held<60*40&&g.m_capture.kind==LockerCapture::Kind::Guard;++held){g.update({},frame);came|=length(g.npcActors()[1].pose.position-g.m_player.pos)<5.f;spoke|=g.m_capture.teamLine>=2;captioned|=g.actorCaption()=="THEN WHY IS SHE STILL BREATHING?";}
  const bool unhurt=g.m_player.health==health;const float heldFor=held*frame;
  // The verdict: let go, and shot.
  bool shot=false;for(int k=0;k<60*8&&!shot;++k){g.update({},frame);shot=g.m_player.health<health;}
  report<<"  held "<<heldFor<<" s, squadmate came "<<came<<", lines "<<spoke<<", unhurt "<<unhurt<<", shot after "<<shot<<"\n";
  check(unhurt&&spoke&&captioned&&heldFor>6&&heldFor<36&&shot,"Held for "+std::to_string(heldFor).substr(0,4)+" s nobody fires and they talk it over; then the verdict, and they shoot");
  check(came,"His squadmate answers the call and comes to stand over the prisoner");}
 // 6. Breaking a guard's grip: he reels, the gun comes back, and he can be killed.
 {auto g=scene({},{{{6.f,6.5f},-kPi*.5f}});const bool armed=g.weaponEquipped();hide(g);g.stageLockerCatch();for(int i=0;i<60*2;++i)g.update({},frame);
  int frames=0;for(;frames<60*8&&g.m_capture.kind!=LockerCapture::Kind::None;frames+=8)mash(g,8);
  const bool free=g.m_capture.kind==LockerCapture::Kind::None,hurt=g.state(g.npcActors()[0].track().damageState)>0,reeling=g.state(g.friendlyKey(0,"guard_hold_ms"))>600;
  const float health=g.m_player.health;for(int i=0;i<40;++i)g.update({},frame);
  report<<"  free after "<<frames*frame<<" s, his damage "<<g.state(g.npcActors()[0].track().damageState)<<", hold "<<g.state(g.friendlyKey(0,"guard_hold_ms"))<<" ms, armed "<<armed<<" -> "<<g.weaponEquipped()<<"\n";
  check(free&&hurt&&reeling&&frames>90&&frames<60*6&&g.m_player.health==health&&g.weaponEquipped()==armed&&!g.handsLowered()==armed,"Mashing USE breaks his grip in "+std::to_string(frames*frame).substr(0,3)+" s: he takes an elbow and reels, the gun comes back up, and for a moment nobody shoots");}
 // 7. With no room along the lockers he holds the player where they stand.
 {std::vector<Post> guards{{{6.f,6.5f},-kPi*.5f}};auto g=scene({},guards);hide(g);
  g.m_capture={};const bool caught=g.stageLockerCatch();
  // (The open room always has a way; the plan itself is what is checked here.)
  Game boxed=g;boxed.m_capture.march=false;boxed.m_capture.stage=LockerCapture::Stage::Pin;const Vec2 at=boxed.m_player.pos;for(int i=0;i<60*3;++i)boxed.update({},frame);
  check(caught&&boxed.m_capture.stage==LockerCapture::Stage::Kneel&&length(boxed.m_player.pos-at)<.01f&&boxed.m_player.crouched,"With nowhere to walk them he puts the player on their knees where they stand");}
 report<<(failed?"FAILED ":"PASSED ")<<checks-failed<<'/'<<checks<<'\n';return failed==0;
}
}

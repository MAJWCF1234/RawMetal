#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
#include "../../../world/ModelIds.h"
#include <fstream>
#include <tuple>
// Guards and lockers: who saw the player go in, the walk to the doors and the
// haul, a suspect locker checked out loud, and bodies stowed out of sight.
namespace retro {
namespace {
// The same reading of a locker as Stealth.cpp's: the doors are the model's local +x.
Vec2 doorsOut(const Fixture& f){return {std::cos(f.yaw),-std::sin(f.yaw)};}
Vec2 doorsFront(const Fixture& f,float ahead){return f.position+doorsOut(f)*(f.width*.5f+ahead);}
bool hidingLocker(const Fixture& f){return f.model==model::psx_locker&&f.solid&&f.visible;}
// Where a guard stands to open the doors: square on, an arm's length out, so
// the plain way out beside him stays clear for the player he hauls.
constexpr float HaulStand=.6f;
// The entry is watched this long; an unbroken look of WitnessLook inside it,
// by a man who already half had the player, makes a witness.
constexpr float WatchWindow=.65f,WitnessLook=.25f,GlimpseLook=.1f;
enum : std::uint8_t {Qualified=1,Glimpsed=2,Witness=4};
// The doors heard: walked in, the latch carries an arm's length; run at (any
// faster than the 3.6 m/s walk within 0.3 s of the latch) it carries 5 m.
constexpr float DashSpeed=4.2f,DashWindow=.3f,DoorsHeard=1.2f,DashHeard=5.f;
// A search that ends this close to a locker's doors may take in the locker:
// always on a confirmed hunt for the player, on a hunch (one in three, the
// same for the same man, locker and search point) while his squad is on Caution.
constexpr float SearchReach=1.5f;constexpr int HunchPerMille=350;
// A suspect check starts with a stop: he turns square to the doors (up to
// FaceMs) before he walks to them, so the intent is read before he moves.
constexpr int FaceMs=700;
std::uint32_t scramble(std::uint32_t h){h^=h>>16;h*=0x7feb352dU;h^=h>>15;h*=0x846ca68bU;h^=h>>16;return h;}
// A stowed body: dragged on his back to LockerStowMouth in front of the doors,
// then lifted into the locker facing its back wall over LockerStowRise of the
// stow, his feet LockerStowSeat behind the doors (the middle, so no boot shows
// under the shut leaves), his shoulders brought up at an even pace to
// LockerStowLift of their height: slumped, knees gone, head down, propped on
// the doors. Opened, the rest of his fall carries him out backwards over their
// foot, his boots sliding out to LockerTumbleFeet, clear of the leaves' arc.
constexpr float LockerStowMouth=.25f,LockerStowSeat=.32f,LockerStowRise=.4f,LockerStowLift=.62f,LockerTumbleFeet=.4f;
// The burst: the man at the doors is caught by a leaf's edge coming within
// BurstBody of his middle (arms and chest) or BurstGun of his gun; the blow
// starts his fall where it already moves (BurstLift) and throws him BurstPush
// back off the doors, easing out over BurstPushMs; he lands at LandLift.
constexpr float BurstBody=.36f,BurstGun=.07f,BurstLift=.93f,BurstPush=.3f,LandLift=.1f;constexpr int BurstPushMs=180,BurstFastMs=360;
// Hauled out, the player is held at gunpoint at arm's length: as the hold
// begins the hauler takes HoldStep straight back off the doors, along their
// normal, or as near it as the room lets him (holdStep).
constexpr float HoldStep=.5f;
// Opened on a body, the leaves stay back on their stops (inside the Caught
// swing's hold, LockerDoors.cpp: out by 0.15 s, held to 1.15 s) for as long as
// a man lies across their arc; dragged clear, they drift to.
constexpr float LeavesHeld=1.f,LeafClear=.22f; // his boots and a hand's breadth past the free edge's sweep
// A leaf at a swing (the geometry LockerDoors.cpp hangs them by): hinged on
// an outer edge of the open front, its free edge meeting the other's in the middle.
std::pair<Vec2,Vec2> leafAt(const Fixture& f,int leaf,float angle){const float sx=f.width/.64f,sz=f.depth/.9f,hand=leaf?-1.f:1.f;const Vec2 o=doorsOut(f),side{-o.y,o.x};
 const Vec2 hinge=f.position+o*(.32f*sx)+side*((leaf?.4412f:.432f)*sz*hand),toward=side*(-hand*std::cos(angle))+o*std::sin(angle);return {hinge,hinge+toward*((leaf?.443f:.43f)*sz)};}
float segmentDistance(Vec2 p,Vec2 a,Vec2 b){const Vec2 ab=b-a;const float k=std::clamp(dot(p-a,ab)/std::max(1e-6f,dot(ab,ab)),0.f,1.f);return length(a+ab*k-p);}
// The handle, where the leaves meet, and how square he stands to the doors before reaching.
constexpr float LockerHandleZ=1.05f,LockerHandleOut=.03f,LockerHaulSquare=.12f;
}
void Game::validateGuardLocker(size_t actor)const{
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};
 if(get("guard_check")<0||get("guard_check")>2||get("guard_checked")<0||get("guard_checked")>1||get("guard_check_ms")<0||get("guard_check_ms")>60000||get("guard_check_said")<0||get("guard_check_said")>60001||get("guard_doors_ms")<0||get("guard_doors_ms")>60000||get("guard_haul")<0||get("guard_haul")>LockerHaulMs)throw std::runtime_error("Invalid saved guard locker check");
 if(get("guard_stowed")<0||get("guard_stowed")>65536||get("guard_stowed_level")<0||get("guard_stowed_level")>chunkCount()||(get("guard_stowed")&&!get("guard_stowed_level"))||get("guard_stow_ms")<0||get("guard_stow_ms")>LockerStowMs||get("guard_stow_x")<0||get("guard_stow_x")>24000||get("guard_stow_y")<0||get("guard_stow_y")>24000||std::abs(get("guard_stow_z"))>100000)throw std::runtime_error("Invalid saved stowed body");
 if(get("guard_burst")<0||get("guard_burst")>65536||get("guard_burst_ms")<0||get("guard_burst_ms")>60001||get("guard_tumble")<0||get("guard_tumble")>65536||get("guard_recoil")<0||get("guard_recoil")>451||get("guard_recoil_x")<0||get("guard_recoil_x")>24000||get("guard_recoil_y")<0||get("guard_recoil_y")>24000)throw std::runtime_error("Invalid saved locker burst");
}
void Game::lockerDoorsMove(int level,int fixture,LockerMotion motion){++m_lockerDoorCalls;m_lockerDoorLast=motion;animateLockerDoor(level,fixture,motion);}
void Game::clearGuardLocker(size_t actor){for(const char*field:{"guard_locker","guard_check","guard_check_ms","guard_check_said","guard_doors_ms","guard_haul"})setState(friendlyKey(actor,field),0);}
// --- Who saw it ----------------------------------------------------------------
// Called by enterLocker before the player is moved in: the body going in is
// what a guard sees, where it stood and as lit as it was.
void Game::watchLockerEntry(int locker){
 auto&w=m_lockerWatch;const auto&f=m_world.fixtures()[size_t(locker)];const size_t count=m_friendlyActors.size();
 w.dash=length(m_velocity)>DashSpeed||m_elapsed-w.dashAt<=DashWindow+1e-4f;
 w.locker=locker;w.started=m_elapsed;const Vec2 at=doorsFront(f,.2f);w.body={at.x,at.y,m_world.floorHeight(f.position.x,f.position.y)+f.base};
 w.height=Player::StandingHeight;w.eye=Player::StandingEye;w.exposure=m_playerExposure;w.crouched=m_player.crouched;
 w.since.assign(count,-1);w.verdict.assign(count,0);if(w.hadPlayer.size()<count)w.hadPlayer.resize(count,-100);
 for(size_t actor=0;actor<count;++actor){const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();auto get=[&](const char*field){return state(friendlyKey(actor,field));};
  if(t.ai.mode!=ActorAiMode::Guard||!autonomousActor(t)||npc.pose.clip==4||npcIncapacitated(actor)||(t.deadState&&state(t.deadState))||!guardHostileToPlayer(actor))continue;
  const auto mode=guardState(actor);const bool onPlayer=get("guard_target_kind")==int(ShotKind::Player)&&(mode==GuardState::Investigate||mode==GuardState::Search||mode==GuardState::Alert);
  if(onPlayer||get("guard_recognition")*2>=std::max(1,t.ai.recognitionMs))w.verdict[actor]=Qualified;
  lockerWitnessSense(npc.level,actor,npc.pose);
 }
}
// Each guard's own look at the entry, every frame of the window; at its end
// the half-witnesses and any hot dive are settled.
void Game::lockerWitnessSense(int level,size_t actor,const ActorPose& pose){
 auto&w=m_lockerWatch;auto get=[&](const char*field){return state(friendlyKey(actor,field));};
 if(w.hadPlayer.size()<m_friendlyActors.size())w.hadPlayer.resize(m_friendlyActors.size(),-100);
 // The pace the player last ran at, for the latch's reach (lockerEntryHeard).
 if(length(m_velocity)>DashSpeed&&hiddenLocker()<0)w.dashAt=m_elapsed;
 if(get("guard_seen")&&get("guard_confirmed")&&get("guard_target_kind")==int(ShotKind::Player))w.hadPlayer[actor]=m_elapsed;
 if(w.locker<0)return;
 // Out again (or pulled) and the record is moot; on the entry frame itself he is not in yet.
 if((m_elapsed>w.started&&hiddenLocker()!=w.locker)||size_t(w.locker)>=m_world.fixtures().size()){w.locker=-1;return;}
 const auto&f=m_world.fixtures()[size_t(w.locker)];
 if(m_elapsed-w.started>WatchWindow+1e-4f){
  // Settled once, by whichever guard thinks first after the window.
  const int locker=w.locker;w.locker=-1;bool witnessed=false;
  for(auto v:w.verdict)witnessed|=(v&Witness)!=0;
  for(size_t other=0;other<w.verdict.size()&&other<m_friendlyActors.size();++other){if((w.verdict[other]&(Glimpsed|Witness))!=Glimpsed||npcIncapacitated(other))continue;
   // Half saw it: he looks over the floor in front of the bank, never inside.
   auto put=[&](const char*field,int value){setState(friendlyKey(other,field),value);};const Vec2 front=doorsFront(f,1.1f)+chunkOffset(m_level);
   put("guard_x",int(std::round(front.x*1000)));put("guard_y",int(std::round(front.y*1000)));put("guard_z",int(std::round(w.body.z*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);
   if(guardState(other)!=GuardState::Alert)put("guard_mode",int(GuardState::Investigate));
  }
  if(witnessed)return;
  // Hot dive: shut in within 4 s of a man having the player, close to where
  // he last had them. The nearest such searcher checks it, out loud, once.
  size_t chosen=SIZE_MAX;float best=8.f;const Vec2 mouth=doorsFront(f,0)+chunkOffset(m_level);
  for(size_t other=0;other<m_friendlyActors.size()&&other<w.hadPlayer.size();++other){const auto&npc=m_friendlyActors[other];const auto&t=npc.track();auto value=[&](const char*field){return state(friendlyKey(other,field));};
   const auto mode=guardState(other);
   if(t.ai.mode!=ActorAiMode::Guard||!autonomousActor(t)||npc.pose.clip==4||npcIncapacitated(other)||(t.deadState&&state(t.deadState))||!guardHostileToPlayer(other))continue;
   if(w.started-w.hadPlayer[other]>4.f||!value("guard_confirmed")||value("guard_target_kind")!=int(ShotKind::Player)||value("guard_checked")||value("guard_locker"))continue;
   if(mode!=GuardState::Alert&&mode!=GuardState::Investigate&&mode!=GuardState::Search)continue;
   const float distance=length(Vec2{value("guard_x")*.001f,value("guard_y")*.001f}-mouth);if(distance<best){best=distance;chosen=other;}
  }
  if(chosen==SIZE_MAX)return;
  auto put=[&](const char*field,int value){setState(friendlyKey(chosen,field),value);};const Vec2 stand=doorsFront(f,HaulStand)+chunkOffset(m_level);
  clearGuardLocker(chosen);put("guard_locker",locker+1);put("guard_check",1);put("guard_checked",1);
  put("guard_x",int(std::round(stand.x*1000)));put("guard_y",int(std::round(stand.y*1000)));put("guard_z",int(std::round(w.body.z*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);put("guard_mode",int(GuardState::Investigate));
  return;
 }
 if(actor>=w.verdict.size()||(w.verdict[actor]&Witness))return;
 // The same eyes as guardCanSee, on the body going in rather than the shut
 // doors: crouching and shadow shorten them, and the edge of his vision halves them.
 const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();const auto&c=t.ai;
 bool sees=t.ai.mode==ActorAiMode::Guard&&autonomousActor(t)&&npc.pose.clip!=4&&!npcIncapacitated(actor)&&!(t.deadState&&state(t.deadState))&&guardHostileToPlayer(actor);
 if(sees){
  const Vec2 shift=chunkOffset(level)-chunkOffset(m_level),body=Vec2{w.body.x,w.body.y}-shift;const float eye=friendlyHullHeight(actor,t)*.85f;
  const Vec2 delta=body-pose.position;const float z=w.body.z+w.eye-pose.z-eye;
  float range=c.sightRange;if(w.crouched)range*=.55f;if(!get("guard_confirmed"))range*=.55f+.45f*std::min(1.f,w.exposure*2.f);if(guardWary(actor))range*=1.15f;
  const float facing=dot(normalized(delta),Vec2{std::sin(pose.yaw),std::cos(pose.yaw)});if(facing<(c.sightDot+1.f)*.5f)range*=.5f;
  sees=lengthSq(delta)+z*z<range*range&&facing>c.sightDot&&soundRayClear(pose.position+shift,pose.z+eye,Vec2{w.body.x,w.body.y},w.body.z+w.eye);
 }
 if(!sees){w.since[actor]=-1;return;}
 // A frame or two as he turns is nothing; a tenth of a second is a glimpse.
 if(w.since[actor]<0)w.since[actor]=m_elapsed;if(m_elapsed-w.since[actor]>=GlimpseLook-1e-4f)w.verdict[actor]|=Glimpsed;
 if(m_elapsed-w.since[actor]<WitnessLook-1e-4f||!(w.verdict[actor]&Qualified))return;
 // A witness: he knows which locker and goes straight to its doors.
 w.verdict[actor]|=Witness;auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};const Vec2 stand=doorsFront(f,HaulStand)+chunkOffset(m_level);
 rememberGuardTarget(actor,resolveGuardTarget(actor,{ShotKind::Player,m_level,0}));clearGuardLocker(actor);
 put("guard_x",int(std::round(stand.x*1000)));put("guard_y",int(std::round(stand.y*1000)));put("guard_z",int(std::round(w.body.z*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);put("guard_locker",w.locker+1);
 if(guardState(actor)!=GuardState::Alert)put("guard_mode",int(GuardState::Investigate));
 // "Saw that." stands in for the mode change's own bark.
 if(npcVoice(actor,NpcVoiceCue::Witness))put("voice_mode",int(GuardState::Investigate));
}
// A noise from the locker the player is shut in, heard by a man who came to
// look at it and stands within 2.5 m: now he suspects it. True when the noise
// is his locker's business (his memory stays on its doors).
bool Game::lockerSuspectNoise(int level,size_t actor,const ActorPose& pose,const AiStimulus& noise){
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};
 // The doors themselves, worked by the player (in, out, a body stowed): he
 // heard which locker, and checks it, once a search, whatever he was doing.
 if(const int doors=lockerDoorNoise(noise);doors>=0){
  if(get("guard_locker"))return get("guard_locker")==doors+1;
  if(get("guard_checked")||!guardHostileToPlayer(actor))return false;
  suspectLocker(actor,doors);return true;}
 const int index=hiddenLocker();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];
 const Vec2 mouth=doorsFront(f,0);if(length(noise.source-mouth)>1.f)return false;
 if(get("guard_locker"))return get("guard_locker")==index+1;
 const auto mode=guardState(actor);if(mode!=GuardState::Investigate&&mode!=GuardState::Search)return false;
 const Vec2 global=mouth+chunkOffset(m_level),remembered{get("guard_x")*.001f,get("guard_y")*.001f};
 if(!get("guard_memory")&&mode!=GuardState::Search)return false;
 if(length(remembered-global)>1.5f||length(pose.position+chunkOffset(level)-global)>2.5f)return false;
 suspectLocker(actor,index);return true;
}
// A latch or a shut this frame from a locker's mouth: the fixture, or -1. Doors
// a guard ripped open are his own noise, not the player's.
int Game::lockerDoorNoise(const AiStimulus& noise)const{
 bool doors=false;for(const auto&e:m_sounds)doors|=(e.sound==Sound::LockerLatch||e.sound==Sound::LockerShut)&&lengthSq(e.position-noise.source)<1e-4f&&std::fabs(e.z-noise.z)<.05f;
 if(!doors)return -1;const auto&fixtures=m_world.fixtures();
 for(size_t i=0;i<fixtures.size();++i){const auto&f=fixtures[i];if(!hidingLocker(f)||lengthSq(doorsFront(f,0)-noise.source)>.1f*.1f)continue;
  for(const auto&d:m_lockerDoors)if(d.level==m_level&&d.fixture==int(i)&&d.motion==LockerMotion::Caught)return -1;
  return int(i);}
 return -1;
}
float Game::lockerEntryHeard()const{return m_lockerWatch.dash?DashHeard:DoorsHeard;}
// He has a locker to look into, out loud: square on to its doors, an arm's
// length out, with this search's one check spent on it.
void Game::suspectLocker(size_t actor,int index){
 const auto&f=m_world.fixtures()[size_t(index)];auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};const Vec2 stand=doorsFront(f,HaulStand)+chunkOffset(m_level);
 clearGuardLocker(actor);put("guard_locker",index+1);put("guard_check",1);put("guard_checked",1);
 put("guard_x",int(std::round(stand.x*1000)));put("guard_y",int(std::round(stand.y*1000)));put("guard_z",int(std::round((m_world.floorHeight(f.position.x,f.position.y)+f.base)*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);
}
// Investigate's end, where he means to search (guardBaseIntent): a locker whose
// doors open within SearchReach of that point is looked into first - always on
// a confirmed hunt for the player, on a hunch while his squad is on Caution;
// never from his beat, never twice a search. True when he takes one.
bool Game::lockerAtSearch(int level,size_t actor){
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};
 if(get("guard_checked")||get("guard_locker")||!get("guard_memory")||!guardHostileToPlayer(actor)||guardState(actor)==GuardState::Patrol)return false;
 const bool hunt=get("guard_confirmed")&&get("guard_target_kind")==int(ShotKind::Player);if(!hunt&&!guardCaution(actor))return false;
 const Vec2 point{get("guard_x")*.001f,get("guard_y")*.001f};const float feet=get("guard_z")*.001f;int chosen=-1;float best=SearchReach;const auto&fixtures=m_world.fixtures();
 for(size_t i=0;i<fixtures.size();++i){const auto&f=fixtures[i];if(!hidingLocker(f)||std::fabs(m_world.floorHeight(f.position.x,f.position.y)+f.base-feet)>1.f)continue;
  const float d=length(doorsFront(f,0)+chunkOffset(m_level)-point);if(d<best){best=d;chosen=int(i);}}
 if(chosen<0||(!hunt&&!lockerHunch(actor,chosen,get("guard_x"),get("guard_y"))))return false;
 // One of a bank he means to go through, not a door he heard (the take he says it in).
 suspectLocker(actor,chosen);setState(friendlyKey(actor,"guard_check"),2);(void)level;return true;
}
bool Game::lockerHunch(size_t actor,int locker,int x,int y){return scramble(scramble(std::uint32_t(actor)*0x9e3779b9u+std::uint32_t(locker))^std::uint32_t(x)*0x85ebca6bu^std::uint32_t(y)*0xc2b2ae35u)%1000<HunchPerMille;}
// --- At the doors ----------------------------------------------------------------
// guardBaseIntent's Investigate step for a man with a locker to open: 0 when
// he has none (or gives it up), 1 while he walks to it, waits or reaches,
// 2 once he has hauled the player out.
int Game::guardAtLocker(int level,size_t actor,const ActorTrack& track,ActorPose& pose,int ms,Vec2& goal,float& goalZ,bool& wantsMove,bool free){
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
 const int index=get("guard_locker")-1;if(index<0)return 0;
 if(size_t(index)>=m_world.fixtures().size()||!hidingLocker(m_world.fixtures()[size_t(index)])){clearGuardLocker(actor);return 0;}
 const auto&f=m_world.fixtures()[size_t(index)];const Vec2 shift=chunkOffset(m_level)-chunkOffset(level),out=doorsOut(f);
 const Vec2 stand=doorsFront(f,HaulStand)+shift,mouth=doorsFront(f,0)+shift;const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 // Sent somewhere else (a noise away from these doors, a squadmate's report):
 // he has something else to see to.
 const Vec2 remembered{get("guard_x")*.001f-chunkOffset(level).x,get("guard_y")*.001f-chunkOffset(level).y};
 if(length(remembered-mouth)>1.5f){clearGuardLocker(actor);return 0;}
 if(!free){put("guard_haul",0);return 0;}
 const Vec2 global=stand+chunkOffset(level);put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(floor*1000)));
 const bool suspect=get("guard_check")!=0;int check=get("guard_check_ms");
 if(suspect){check=std::min(60000,check+ms);put("guard_check_ms",check);
  // Out loud, before he acts: a suspect check is never a surprise.
  if(!get("guard_check_said")&&(npcVoice(actor,NpcVoiceCue::CheckLockers)||(check>=700&&npcVoice(actor,NpcVoiceCue::Investigate))||!npcHasVoice(actor)||check>=2500)){put("guard_check_said",check+1);put("voice_mode",int(GuardState::Investigate));}
  // The stop and the look: he turns square to the doors before his first step.
  const Vec2 toward=mouth-pose.position;const float turn=std::remainder(std::atan2(toward.x,toward.y)-pose.yaw,2*kPi);
  if(check<=FaceMs&&!get("guard_doors_ms")&&std::fabs(turn)>.2f&&lengthSq(toward)>.04f){pose.yaw+=std::clamp(turn,-4.f*ms*.001f,4.f*ms*.001f);goal=pose.position;goalZ=pose.z;wantsMove=false;return 1;}}
 const float distance=length(pose.position-stand);
 if(distance>.03f||std::fabs(pose.z-floor)>.3f){
  int travel=std::min(8000,get("guard_travel")+ms);put("guard_travel",travel);put("guard_doors_ms",0);put("guard_haul",0);
  if(travel>=8000){clearGuardLocker(actor);return 0;}
  goal=stand;goalZ=floor;wantsMove=true;return 1;
 }
 // At the doors: square up to them, wait out a suspect check, reach, open.
 wantsMove=false;goal=pose.position;goalZ=pose.z;const float want=std::atan2(-out.x,-out.y),turn=std::remainder(want-pose.yaw,2*kPi);
 pose.yaw+=std::clamp(turn,-4.f*ms*.001f,4.f*ms*.001f);
 int doors=std::min(60000,get("guard_doors_ms")+ms);put("guard_doors_ms",doors);
 // Square on to the doors before the hand goes out.
 if(std::fabs(turn)>LockerHaulSquare)return 1;
 // The lead (said LockerCheckLeadMs before the doors move) is for whoever hides
 // in there to hear; with nobody in it the pause at the doors is all he takes.
 const bool hider=hiddenLocker()==index;
 if(suspect&&(doors<LockerCheckPauseMs||(hider&&(!get("guard_check_said")||check<get("guard_check_said")-1+LockerCheckLeadMs))))return 1;
 int haul=std::min(LockerHaulMs,get("guard_haul")+ms);put("guard_haul",haul);if(haul<LockerHaulMs)return 1;
 lockerDoorsMove(m_level,index,LockerMotion::Caught);
 if(hiddenLocker()==index){
  if(npcVoice(actor,NpcVoiceCue::Found))put("voice_mode",int(GuardState::Alert));
  // (The step back for the hold is updateLockerBurst's, from the hold's first frame.)
  put("guard_haul",0);const bool pulled=pullFromLocker(level,actor,pose);clearGuardLocker(actor);
  return pulled?2:0;
 }
 clearGuardLocker(actor);
 if(lockerBody(m_level,index))tumbleFromLocker(m_level,index,actor);
 else m_sounds.push_back({Sound::Door,doorsFront(f,0),.5f,1.15f,true,10,floor+1.2f});
 (void)track;return 0;
}
// His support hand on the door handle for the reach (the haul's ms), for the renderer.
Point3 Game::guardLockerHandle(size_t actor,float& reach)const{
 reach=0;const int index=state(friendlyKey(actor,"guard_locker"))-1;const int haul=state(friendlyKey(actor,"guard_haul"));
 if(index<0||size_t(index)>=m_world.fixtures().size()||actor>=m_friendlyActors.size())return {};
 const auto&npc=m_friendlyActors[actor];const auto&f=m_world.fixtures()[size_t(index)];const Vec2 shift=chunkOffset(m_level)-chunkOffset(npc.level);
 const Vec2 at=doorsFront(f,LockerHandleOut)+shift;const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 // Up over the first 60%, then held on the handle as the doors give.
 float t=std::clamp(haul/(LockerHaulMs*.6f),0.f,1.f);reach=haul>0?t*t*(3-2*t):0;
 return {at.x,at.y,floor+LockerHandleZ};
}
// --- Bodies in lockers ---------------------------------------------------------------
int Game::lockerBody(int level,int fixture)const{
 for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(state(friendlyKey(actor,"guard_stowed"))==fixture+1&&state(friendlyKey(actor,"guard_stowed_level"))==level+1)return int(actor)+1;
 return 0;
}
bool Game::npcStowed(size_t actor)const{return actor<m_friendlyActors.size()&&state(friendlyKey(actor,"guard_stowed"))&&state(friendlyKey(actor,"guard_stow_ms"))>=LockerStowMs;}
// The free locker the player faces with a body by the collar: the same reach
// as climbing in, with no room needed to step back out.
int Game::stowLockerAhead()const{
 if(m_vrInputActive||dead()||!draggingBody()||!m_player.grounded||hiddenLocker()>=0)return -1;
 const Vec2 forward{std::cos(m_player.angle),std::sin(m_player.angle)};int best=-1;float nearest=1.05f;const auto&fixtures=m_world.fixtures();
 for(size_t i=0;i<fixtures.size();++i){const auto&f=fixtures[i];if(!hidingLocker(f)||lockerBody(m_level,int(i)))continue;
  const Vec2 out=doorsOut(f),side{-out.y,out.x},delta=m_player.pos-f.position;const float ahead=dot(delta,out)-f.width*.5f;
  if(ahead<.1f||ahead>=nearest||std::fabs(dot(delta,side))>f.depth*.5f+.1f||dot(forward,out)>-.55f)continue;
  const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
  if(std::fabs(m_player.z-floor)>.3f||!m_world.rayClear(m_player.pos,m_player.z+1.f,doorsFront(f,.03f),floor+1.f,true))continue;
  best=int(i);nearest=ahead;
 }return best;
}
bool Game::stowBody(){
 const int index=stowLockerAhead();if(index<0)return false;const size_t actor=size_t(m_draggedNpc);const auto&f=m_world.fixtures()[size_t(index)];
 auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};const auto&body=m_friendlyActors[actor].pose;
 put("guard_stowed",index+1);put("guard_stowed_level",m_level+1);put("guard_stow_ms",0);put("guard_stow_fraction",0);
 put("guard_stow_x",int(std::round(std::clamp(body.position.x,0.f,24.f)*1000)));put("guard_stow_y",int(std::round(std::clamp(body.position.y,0.f,24.f)*1000)));put("guard_stow_z",int(std::round(body.z*1000)));
 dropBody();m_pickupNotice.clear();m_pickupNoticeTime=0;lockerDoorsMove(m_level,index,LockerMotion::Stow);
 const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 SoundEvent clank{Sound::Door,doorsFront(f,0),.3f,1.3f,true,6,floor+1.2f};clank.aiRadius=1.2f;m_sounds.push_back(clank);
 return true;
}
// The stow, over the Stow swing of the leaves (open by 0.25 s, held to 1.05 s,
// shut at 1.4 s): dragged round whichever side of the player he lay on to the
// open doors, stood up into the locker between the leaves, his length along
// its depth so nothing passes through its sides, and held there as they shut.
// He is drawn until they have shut, so he never vanishes in an open locker.
void Game::updateStowedBody(size_t actor,ActorPose& pose,float dt){
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
 const int index=get("guard_stowed")-1,level=get("guard_stowed_level")-1;if(index<0)return;
 if(level<0||level>=chunkCount()||!chunkResident(level)||size_t(index)>=friendlyWorld(level).fixtures().size()){for(const char*f:{"guard_stowed","guard_stowed_level","guard_stow_ms"})put(f,0);return;}
 const auto&f=friendlyWorld(level).fixtures()[size_t(index)];const auto&npc=m_friendlyActors[actor];const Vec2 shift=chunkOffset(level)-chunkOffset(npc.level);
 int ms=get("guard_stow_ms");const int was=ms;if(dt>0&&ms<LockerStowMs)ms=std::min(LockerStowMs,ms+npcMilliseconds(actor,dt,"guard_stow_fraction"));put("guard_stow_ms",ms);
 if(was<LockerStowMs&&ms>=LockerStowMs){m_pickupNotice="BODY STOWED";m_pickupNoticeTime=1.2f;} // said once the leaves are shut on him
 const Vec2 out=doorsOut(f),side{-out.y,out.x},start{get("guard_stow_x")*.001f,get("guard_stow_y")*.001f},mouth=doorsFront(f,LockerStowMouth)+shift,inside=doorsFront(f,-LockerStowSeat)+shift;
 const float floor=friendlyWorld(level).floorHeight(f.position.x,f.position.y)+f.base,startZ=get("guard_stow_z")*.001f;
 const float t=float(ms)/LockerStowMs;Vec2 at;float feet,phase=1;
 if(t<.42f){const float u=t/.42f,s=u*u*(3-2*u);const Vec2 bend=(start+mouth)*.5f+side*(dot(start-mouth,side)>=0?.45f:-.45f);
  at=start*((1-s)*(1-s))+bend*(2*s*(1-s))+mouth*(s*s);feet=startZ+(floor-startZ)*s;const Vec2 heading=normalized(mouth-at+out*-.01f);if(lengthSq(heading)>.0001f)pose.yaw=std::atan2(heading.x,heading.y);}
 else{const float u=std::min(1.f,(t-.42f)/LockerStowRise),s=u*u*(3-2*u);at=mouth+(inside-mouth)*s;feet=floor;phase=fallPhaseAtLift(npc.track().visual,LockerStowLift*s);pose.yaw=std::atan2(-out.x,-out.y);}
 pose.position=at;pose.z=feet;pose.clip=4;pose.phase=phase;pose.lookAtActor=-1;
 m_friendlyActors[actor].pose=pose;saveRestrainedPose(actor);
}
// The doors open on a stowed body: he tumbles out across their foot, and the
// man who opened them has found a fallen colleague (Mind.cpp's casualty rules).
void Game::tumbleFromLocker(int level,int fixture,size_t finder){
 const int body=lockerBody(level,fixture)-1;if(body<0)return;const size_t actor=size_t(body);const auto&f=friendlyWorld(level).fixtures()[size_t(fixture)];
 for(const char*field:{"guard_stowed","guard_stowed_level","guard_stow_ms","guard_stow_fraction","guard_stow_x","guard_stow_y","guard_stow_z"})setState(friendlyKey(actor,field),0);
 // Slumped against the doors, his back to them: as they fly open the rest of
 // his fall carries him out backwards, skewed off to the open side, his boots
 // sliding out over their foot (updateLockerBurst) clear of the leaves' arc.
 auto&npc=m_friendlyActors[actor];const Vec2 out=doorsOut(f),side{-out.y,out.x};const Vec2 shift=chunkOffset(level)-chunkOffset(npc.level);
 const auto&world=friendlyWorld(level);const float floor=world.floorHeight(f.position.x,f.position.y)+f.base;
 float lean=1;{float room[2]{};for(int k=0;k<2;++k)for(float reach=.3f;reach<=1.5f;reach+=.3f){const Vec2 p=doorsFront(f,1.1f)+side*(reach*(k?-1.f:1.f));if(!world.fits(p.x,p.y,floor+.1f,.3f,false))break;room[k]=reach;}if(room[1]>room[0])lean=-1;}
 const Vec2 fallTo=normalized(out*std::cos(.6f)+side*(lean*std::sin(.6f)));
 const float fall=cleanupAnimationDuration(npc.track().visual,4);const int duration=fall>0?std::max(1,int(std::round(fall*1000))):1000;const float slump=fallPhaseAtLift(npc.track().visual,LockerStowLift);
 npc.pose.position=doorsFront(f,-LockerStowSeat)+shift;npc.pose.z=floor;npc.pose.yaw=std::atan2(-fallTo.x,-fallTo.y);npc.pose.clip=4;npc.pose.phase=slump;npc.pose.lookAtActor=-1;
 setState(friendlyKey(actor,"restraint_ms"),int(std::round(slump*duration)));setState(friendlyKey(actor,"restraint_fraction"),0);setState(friendlyKey(actor,"guard_tumble"),fixture+1);saveRestrainedPose(actor);
 if(finder>=m_friendlyActors.size()||npcIncapacitated(finder))return;
 auto put=[&](const char*field,int value){setState(friendlyKey(finder,field),value);};const Vec2 global=doorsFront(f,LockerTumbleFeet)+chunkOffset(level);
 // The man who opened them starts back from the body falling at him, to the
 // side it does not fall to, his eyes on it (updateLockerBurst).
 {const Vec2 away=doorsFront(f,.95f)-side*(lean*.75f)+chunkOffset(level)-chunkOffset(m_friendlyActors[finder].level);put("guard_recoil",1);put("guard_recoil_x",int(std::round(away.x*1000)));put("guard_recoil_y",int(std::round(away.y*1000)));}
 AiTarget blame{ShotKind::Player,m_level,0};blame.position=m_player.pos+chunkOffset(m_level)-chunkOffset(m_friendlyActors[finder].level);blame.feet=m_player.z;blame.height=m_player.hullHeight();blame.eye=m_player.eye;blame.faction=AiFaction::Player;
 rememberGuardTarget(finder,blame);put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(npc.pose.z*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);
 // He blames the player, as for any found body (Mind.cpp's casualty rules):
 // sure someone did this, he wears the hunt's red '?', searches from the body
 // and radios it in, so the squad goes to Caution when it lands (a body's call
 // is no hunt EVASION counts). This search has had its check: he does not go
 // back to the doors over him.
 put("guard_confirmed",1);put("guard_recognition",m_friendlyActors[finder].track().ai.recognitionMs);put("guard_checked",1);put("guard_mode",int(GuardState::Investigate));if(!state(friendlyKey(finder,"guard_call")))startGuardCall(finder,CallBody);
 if(npcVoice(finder,NpcVoiceCue::AllyDown))put("voice_mode",int(GuardState::Investigate));
}
// --- The burst, the tumble and the start back ----------------------------------------
bool Game::lockerBurstPending(size_t actor)const{
 if(actor>=m_friendlyActors.size()||npcIncapacitated(actor))return false;return state(friendlyKey(actor,"guard_burst"))||state(friendlyKey(actor,"guard_recoil"));
}
// Each frame, after the guards have moved: a man the doors were burst into is
// hit on the frame a leaf's edge (anywhere it has swept so far) comes within
// reach of his chest or his gun; a body tumbling from a locker slides out with
// his fall; the man who opened it backs off from it. A save loaded mid-burst
// has no leaves in motion: the blow lands at once.
void Game::updateLockerBurst(float dt){
 const int ms=std::max(1,int(std::lround(dt*1000)));
 // The hold's step back: HoldStep straight off the doors along their normal;
 // where a wall or cabinet stops that, the step turns along it toward
 // whichever side is clear, shorter only when nothing else is; always farther
 // from the player, never past them. Where he can go farthest from them wins,
 // straight back by a hand's breadth.
 auto lockerHoldStep=[&](size_t actor,const Fixture& f,Vec2& to){const auto&npc=m_friendlyActors[actor];const auto&pose=npc.pose;
  const Vec2 o=doorsOut(f),side{-o.y,o.x},player=m_player.pos+chunkOffset(m_level)-chunkOffset(npc.level);float best=length(pose.position-player)+.1f;bool found=false;
  for(float step:{HoldStep,.35f,.2f})for(float turn:{0.f,.5f,-.5f,1.f,-1.f,1.4f,-1.4f}){const Vec2 at=pose.position+(o*std::cos(turn)+side*std::sin(turn))*step;float feet=pose.z;
   if(!friendlyWalkSegment(npc.level,pose.position,pose.z,at,npc.track().scale,&feet)||std::fabs(feet-pose.z)>.3f)continue;
   const float score=length(at-player)-.05f*std::fabs(turn);if(score>best){best=score;to=at;found=true;}}
  return found;};
 // Leaves opened on a body stay back on their stops while he lies across their arc.
 for(auto&d:m_lockerDoors){if(d.motion!=LockerMotion::Caught||d.age<=LeavesHeld||d.age>=d.length||!chunkResident(d.level)||size_t(d.fixture)>=friendlyWorld(d.level).fixtures().size())continue;
  const auto&f=friendlyWorld(d.level).fixtures()[size_t(d.fixture)];bool across=false;
  for(size_t actor=0;actor<m_friendlyActors.size()&&!across;++actor){const auto&n=m_friendlyActors[actor];if(!chunkResident(n.level)||n.pose.clip!=4||npcStowed(actor)||npcHeld(actor))continue;
   const Vec2 shift=chunkOffset(n.level)-chunkOffset(d.level);const Point3 c=npcCollar(actor);const Vec2 feet=n.pose.position+shift,collar=Vec2{c.x,c.y}+shift;
   for(int leaf=0;leaf<2;++leaf){const auto [hinge,edge]=leafAt(f,leaf,0);across|=segmentDistance(hinge,feet,collar)<length(edge-hinge)+LeafClear;}}
  if(across)d.age=LeavesHeld;}
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
  auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
  // The hold's first frame (pullFromLocker, from a witness's haul or any
  // other): he has the gun up from the haul, and takes his step back.
  if(get("guard_hold_ms")>=1200&&!get("guard_recoil")&&get("guard_target_kind")==int(ShotKind::Player)&&!npcIncapacitated(actor)){put("weapon_raise",1000);
   if(const int index=lockerViewFixture();index>=0&&size_t(index)<m_world.fixtures().size()){Vec2 to;if(lockerHoldStep(actor,m_world.fixtures()[size_t(index)],to)){put("guard_recoil",1);put("guard_recoil_x",int(std::round(to.x*1000)));put("guard_recoil_y",int(std::round(to.y*1000)));}}}
  if(!get("guard_burst")&&!get("guard_tumble")&&!get("guard_recoil"))continue;
  const bool dead=track.deadState&&state(track.deadState);
  if(dead||npcHeld(actor)){for(const char*field:{"guard_burst","guard_burst_ms","guard_tumble","guard_recoil","guard_recoil_x","guard_recoil_y"})put(field,0);continue;}
  const float fall=cleanupAnimationDuration(track.visual,4);
  // He starts back: eased over 450 ms to the spot beside the doors, facing the
  // body (or, holding the player, turning with them), the walk played backwards under him.
  if(int recoil=get("guard_recoil")){
   const Vec2 to{get("guard_recoil_x")*.001f,get("guard_recoil_y")*.001f};const int was=recoil-1,now=std::min(450,was+ms);
   auto ease=[](float u){u=std::clamp(u,0.f,1.f);return 1-(1-u)*(1-u)*(1-u);};const float a=ease(was/450.f),b=ease(now/450.f),k=a<1?(b-a)/(1-a):1;
   auto&pose=npc.pose;const Vec2 step=(to-pose.position)*k;float feet=pose.z;
   if(npcIncapacitated(actor)||npc.pose.clip==4){put("guard_recoil",0);continue;}
   if(get("guard_hold_ms")>0){const Vec2 toward=m_player.pos+chunkOffset(m_level)-chunkOffset(npc.level)-pose.position;if(lengthSq(toward)>.001f)pose.yaw+=std::clamp(std::remainder(std::atan2(toward.x,toward.y)-pose.yaw,2*kPi),-6*dt,6*dt);}
   if(friendlyWalkSegment(npc.level,pose.position,pose.z,pose.position+step,track.scale,&feet)){pose.position+=step;pose.z=feet;}
   pose.clip=1;pose.phase=std::fmod(pose.phase+1.f-std::min(.9f,length(step)*1.1f),1.f);
   put("x",int(std::round(pose.position.x*1000)));put("y",int(std::round(pose.position.y*1000)));put("z",int(std::round(pose.z*1000)));put("yaw",int(std::round(std::remainder(pose.yaw,2*kPi)*10000)));put("phase",int(std::round(pose.phase*10000)));put("walking",1);
   put("guard_recoil",now>=450?0:now+1);if(now>=450){put("guard_recoil_x",0);put("guard_recoil_y",0);}
  }
  // Out of the locker with his fall: the boots from the seat to clear of the leaves.
  if(const int index=get("guard_tumble")-1;index>=0){
   const int level=npc.level;if(size_t(index)>=friendlyWorld(level).fixtures().size()||!npcIncapacitated(actor)){put("guard_tumble",0);continue;}
   const auto&f=friendlyWorld(level).fixtures()[size_t(index)];const float slump=fallPhaseAtLift(track.visual,LockerStowLift),land=fallPhaseAtLift(track.visual,LandLift);
   const float u=std::clamp((npc.pose.phase-slump)/std::max(.01f,land-slump),0.f,1.f),s=u*u*(3-2*u);
   npc.pose.position=doorsFront(f,-LockerStowSeat+(LockerStowSeat+LockerTumbleFeet)*s);saveRestrainedPose(actor);
   if(u>=1){put("guard_tumble",0);m_sounds.push_back({Sound::Land,npc.pose.position+doorsOut(f)*.6f+chunkOffset(level)-chunkOffset(m_level),.6f,.85f,true,12,npc.pose.z+.1f});}
   continue;
  }
  const int index=get("guard_burst")-1;if(index<0)continue;
  auto stop=[&]{put("guard_burst",0);put("guard_burst_ms",0);};
  if(npc.level!=m_level||size_t(index)>=m_world.fixtures().size()){stop();continue;}
  const auto&f=m_world.fixtures()[size_t(index)];const Vec2 away=doorsOut(f);
  if(!npcIncapacitated(actor)){
   // Caught at the doors: has a leaf reached him yet?
   const LockerDoor*door=nullptr;for(const auto&d:m_lockerDoors)if(d.level==m_level&&d.fixture==index)door=&d;
   bool reached=!door||door->motion!=LockerMotion::Caught||door->age>=.15f;
   if(!reached&&door->age>0){const auto weapon=npcWeaponPose(actor);const Vec2 grip{weapon.grip.x,weapon.grip.y},muzzle{weapon.muzzle.x,weapon.muzzle.y};
    for(int leaf=0;leaf<2&&!reached;++leaf)for(float a=door->from[size_t(leaf)];!reached;a=std::min(a+.035f,door->angle[size_t(leaf)])){const auto [hinge,edge]=leafAt(f,leaf,a);
     reached=segmentDistance(npc.pose.position,hinge,edge)<BurstBody||(weapon.armed&&(segmentDistance(grip,hinge,edge)<BurstGun||segmentDistance(muzzle,hinge,edge)<BurstGun));if(a>=door->angle[size_t(leaf)])break;}}
   if(!reached)continue;
   // The blow: steel into a man, and the man (said before he is down: a man out cold says nothing).
   npcVoice(actor,NpcVoiceCue::Hurt);m_sounds.push_back({Sound::PunchHit,npc.pose.position,.85f,.9f,true,14,npc.pose.z+1.1f});
   clearGuardSchedule(actor);for(const char*field:{"weapon_aim","weapon_recovery","weapon_bash","weapon_bash_pose","grenade_windup","support_ms","drone_windup","drone_busy","engineer_pad_stage","engineer_pad_ms","guard_seen","guard_slot","guard_locker"})put(field,0);
   const float into=fall>0?fallPhaseAtLift(track.visual,BurstLift):0.f;
   put("restraint",1);put("restraint_ms",int(std::round(fall*1000*into)));put("restraint_fraction",0);
   // Thrown back off the doors, facing them: he goes over backwards (the fall clip drops a man on his back).
   npc.pose.yaw=std::atan2(-away.x,-away.y);npc.pose.clip=4;npc.pose.phase=into;npc.pose.lookAtActor=-1;saveRestrainedPose(actor);updateGuardFlags();
   put("guard_burst_ms",1);continue;
  }
  // Down: shoved back off the doors, easing out over BurstPushMs, and noticed
  // when he lands. The blow takes his legs at once: the first BurstFastMs of
  // the fall run at half as fast again, so it reads as knocked down, not sat down.
  const int was=get("guard_burst_ms")-1,now=std::min(60000,was+ms);put("guard_burst_ms",now+1);
  if(was<BurstFastMs&&fall>0){const int duration=std::max(1,int(std::round(fall*1000))),age=std::min(duration,get("restraint_ms")+(std::min(now,BurstFastMs)-was)/2);put("restraint_ms",age);npc.pose.phase=float(age)/duration;}
  if(was<BurstPushMs){auto ease=[](float u){u=std::clamp(u,0.f,1.f);return 1-(1-u)*(1-u);};
   const Vec2 from=npc.pose.position,to=from+away*(BurstPush*(ease(now/float(BurstPushMs))-ease(was/float(BurstPushMs))));float landed=npc.pose.z;
   if(friendlyWalkSegment(m_level,from,npc.pose.z,to,.45f,&landed)){npc.pose.position=to;npc.pose.z=landed;saveRestrainedPose(actor);}}
  if(npc.pose.phase>=fallPhaseAtLift(track.visual,LandLift)||fall<=0){
   const Point3 collar=npcCollar(actor);m_sounds.push_back({Sound::Land,{collar.x,collar.y},.6f,.85f,true,10,npc.pose.z+.05f});
   m_pickupNotice="GUARD UNCONSCIOUS";m_pickupNoticeTime=1.6f;stop();}
 }
}
// The card over a man bound for a locker's doors: whatever brought him (he
// saw the climb, heard the doors, his search ends at them), the doors are
// coming open, so he wears the hunt's full '?' all the way there.
Game::GuardCard Game::guardLockerCard(size_t actor)const{
 auto card=guardCard(actor);if(card.kind==GuardCardKind::None||card.kind==GuardCardKind::Alert)return card;
 if(state(friendlyKey(actor,"guard_locker"))&&guardState(actor)==GuardState::Investigate){card.kind=GuardCardKind::Hunting;card.fill=1;}
 return card;
}
void Game::inspectLockerGuard(bool stow,bool staged){
 size_t chosen=SIZE_MAX;float best=1e9f;
 // A man whose squad is still waiting on its trigger (an ambush not yet sprung)
 // is taken only when no one else is about, and then his squad is switched on.
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();
  if(npc.level!=m_level||t.ai.mode!=ActorAiMode::Guard||!autonomousActor(t)||npcIncapacitated(actor)||npc.pose.clip==4||(t.deadState&&state(t.deadState)))continue;
  const float d=length(npc.pose.position-m_player.pos)+(t.ai.enableState&&!state(t.ai.enableState)?100.f:0.f);if(d<best){best=d;chosen=actor;}}
 {std::ofstream log("locker-inspect.txt",std::ios::app);log<<(stow?"stow":staged?"staged":"caught")<<" level "<<m_level<<" chooses guard "<<(chosen==SIZE_MAX?-1:int(chosen))<<" at "<<best<<" m\n";}
 if(chosen==SIZE_MAX)return;auto put=[&](const char*field,int value){setState(friendlyKey(chosen,field),value);};
 if(const StateId enable=m_friendlyActors[chosen].track().ai.enableState;enable&&!state(enable))setState(enable,1);
 if(stow){auto&npc=m_friendlyActors[chosen];clearGuardSchedule(chosen);put("restraint",1);npc.pose.clip=4;npc.pose.phase=1;
  npc.pose.position=m_player.pos-Vec2{std::cos(m_player.angle),std::sin(m_player.angle)}*.85f;npc.pose.z=m_player.z;npc.pose.yaw=wrapAngle(kPi*.5f-m_player.angle);saveRestrainedPose(chosen);m_draggedNpc=int(chosen);equipWeapon(WeaponId::Fists);return;}
 int index=hiddenLocker();
 // "tumble": no one hiding, the locker a body was stowed in is checked instead.
 if(index<0&&staged)for(size_t i=0;i<m_world.fixtures().size()&&index<0;++i)if(lockerBody(m_level,int(i)))index=int(i);
 if(index<0)return;const auto&f=m_world.fixtures()[size_t(index)];const Vec2 stand=doorsFront(f,HaulStand)+chunkOffset(m_level);
 // "reach"/"tumble": he starts a few steps off the doors, so the walk up, the
 // stop, the reach and the haul all fall inside a short sequence.
 if(staged){auto&npc=m_friendlyActors[chosen];const Vec2 out=doorsOut(f),side{-out.y,out.x},at=doorsFront(f,HaulStand+1.1f)+side*.7f-chunkOffset(npc.level)+chunkOffset(m_level),to=doorsFront(f,0)-at;
  npc.pose.position=at;npc.pose.z=m_world.floorHeight(f.position.x,f.position.y)+f.base;npc.pose.yaw=std::atan2(to.x,to.y);
  put("x",int(std::round(at.x*1000)));put("y",int(std::round(at.y*1000)));put("z",int(std::round(npc.pose.z*1000)));put("yaw",int(std::round(std::remainder(npc.pose.yaw,2*kPi)*10000)));
  // Nobody hiding: the player stands off in the corner, out of his sight.
  if(hiddenLocker()<0){suspectLocker(chosen,index);put("guard_mode",int(GuardState::Investigate));m_player.pos={.3f,.3f};return;}}
 clearGuardLocker(chosen);rememberGuardTarget(chosen,resolveGuardTarget(chosen,{ShotKind::Player,m_level,0}));
 put("guard_x",int(std::round(stand.x*1000)));put("guard_y",int(std::round(stand.y*1000)));put("guard_z",int(std::round((m_world.floorHeight(f.position.x,f.position.y)+f.base)*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);put("guard_locker",index+1);put("guard_mode",int(GuardState::Investigate));
}
void Game::inspectLockerWatch(Vec2 at,float feet,float yaw,float pitch){
 m_lockerView.active=false;m_player.pos=at;m_player.z=feet==-999?m_world.floorHeight(at.x,at.y):feet;m_player.angle=yaw;m_player.pitch=pitch;m_player.crouched=false;m_player.eye=Player::StandingEye;
}
// --- Tests ---------------------------------------------------------------------------
bool Game::testLockerGuards(){
 std::ofstream report("locker-guards-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const std::string& text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 // testLockers' dim room: the locker at the west end, doors east (x 2.435),
 // the player a step in front; each guard {x,y,yaw,beat}.
 struct Post {Vec2 at;float yaw;};
 auto scene=[](std::vector<Post> guards,std::vector<Structure> walls={}){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Locker guard validation";campaign->key=883017;
  auto map=std::make_shared<AuthoredMapData>();map->name="Locker guard room";map->definition={{0,0},{3.2f,6.5f},0,Environment::Interior,false,8,.3f,44};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  map->fixtures.push_back({model::psx_locker,{2.f,6.5f},0,.87f,.85f,2.42f,0,true});
  for(const auto&wall:walls)map->structures.push_back(wall);
  TimedSequence timer;timer.timerState=stateId("lg_clock");timer.finishState=stateId("lg_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  int n=0;for(const auto&post:guards){ActorTrack track;track.timerState=timer.timerState;track.visual=ActorVisual::Cleanup;track.keys={{0,post.at,0,post.yaw},{30000,post.at,0,post.yaw}};track.scale=1.6f;track.health=90;
   track.damageState=stateId(("lg_damage"+std::to_string(n)).c_str());track.deadState=stateId(("lg_dead"+std::to_string(n)).c_str());track.ai.mode=ActorAiMode::Guard;track.ai.armed=true;track.ai.patrol={{post.at,0,60000}};track.ai.squad=stateId("lg_squad");map->actorTracks.push_back(track);++n;}
  campaign->maps.push_back(map);Game g(campaign);g.m_player.pos={3.2f,6.5f};g.m_player.z=0;g.m_player.angle=kPi;g.m_player.pitch=0;return g;
 };
 InputState use;use.use=true;const float out=2.435f;const float frame=1.f/60;
 auto get=[](Game&g,size_t a,const char*f){return g.state(g.friendlyKey(a,f));};
 auto hide=[&](Game& g){g.update(use,frame);g.update({},frame);return g.hiddenLocker()>=0;};
 auto halfSeen=[&](Game& g,size_t a){const int need=g.npcActors()[a].track().ai.recognitionMs;for(int i=0;i<600&&get(g,a,"guard_recognition")*2<need;++i)g.update({},frame);return get(g,a,"guard_recognition")*2>=need;};
 auto dump=[&](Game&g,size_t a,const char*tag){const auto&p=g.npcActors()[a].pose;report<<"  "<<tag<<" guard "<<a<<" at "<<p.position.x<<","<<p.position.y<<" mode "<<int(g.guardState(a))<<" locker "<<get(g,a,"guard_locker")<<" check "<<get(g,a,"guard_check")<<" haul "<<get(g,a,"guard_haul")<<" doors "<<get(g,a,"guard_doors_ms")<<" said "<<get(g,a,"guard_check_said")<<" ms "<<get(g,a,"guard_check_ms")<<" mem "<<get(g,a,"guard_x")<<","<<get(g,a,"guard_y")<<" heard "<<get(g,a,"guard_heard")<<" hidden "<<g.hiddenLocker()<<" stowed "<<get(g,0,"guard_stowed")<<"\n";};
 // A frame starts by clearing its sounds, so a test noise is heard the way an
 // interaction's is: made, then the guards' update in the same frame.
 auto noise=[&](Game& g,SoundEvent e){g.m_sounds.clear();g.m_sounds.push_back(e);g.updateFriendlyActors(frame,true);g.m_sounds.clear();};
 auto knock=[&](Game& g){noise(g,{Sound::Land,{out+.02f,6.5f},.7f,1.35f,true,12,1.2f});};
 // 1. The witness window: a man who half had the player and watches the body
 // go in for a quarter second is a witness; not on the first frame, and not
 // from the edge of his eye in shadow.
 {auto g=scene({{{8.5f,6.5f},-kPi*.5f}});bool half=halfSeen(g,0);bool hidden=hide(g);bool early=get(g,0,"guard_locker")==0;
  int frames=0;while(frames<40&&!get(g,0,"guard_locker")){g.update({},frame);++frames;}
  check(half&&hidden&&early&&get(g,0,"guard_locker")==1&&frames>=12&&frames<=20&&g.guardState(0)==GuardState::Investigate,"A guard who half had the player becomes a witness after a quarter second's unbroken look, not on the entry frame ("+std::to_string(frames+2)+" frames)");
  // 2. The haul: straight to 0.6 m off the doors, a 350 ms reach for the handle, then the doors and the pull.
  int reachStart=-1,pulledAt=-1,calls=g.m_lockerDoorCalls,walked=0;float standOut=0,handleReach=0,handleGap=99;bool exitClear=false,carded=true,racked=false;
  for(int i=0;i<60*15&&pulledAt<0;++i){g.update({},frame);
   if(reachStart<0&&get(g,0,"guard_haul")>0){reachStart=i;const auto&p=g.npcActors()[0].pose;standOut=p.position.x-out;Vec2 spot;float feet;exitClear=g.lockerExit(g.m_world.fixtures()[0],spot,feet);}
   if(get(g,0,"guard_haul")>0){auto w=g.npcWeaponPose(0);handleReach=std::max(handleReach,w.haul);handleGap=std::min(handleGap,std::fabs(w.handle.x-out));}
   // On his way to the doors he wears the hunt's full '?', whatever his plain card says.
   if(reachStart<0&&get(g,0,"guard_locker")){++walked;carded&=g.guardLockerCard(0).kind==GuardCardKind::Hunting;}
   if(g.hiddenLocker()<0){pulledAt=i;for(const auto&e:g.sounds())racked|=e.sound==Sound::Pump;}}
  const float reachMs=(pulledAt-reachStart)*1000.f/60;
  check(reachStart>=0&&standOut>.5f&&standOut<.7f,"The witness stops about 0.6 m in front of the doors ("+std::to_string(standOut)+" m)");
  check(pulledAt>reachStart&&reachMs>=300&&reachMs<=400&&handleReach>.99f&&handleGap<.1f,"He reaches for the handle for about 350 ms before the doors open ("+std::to_string(int(reachMs))+" ms)");
  check(exitClear&&g.m_lockerDoorCalls>calls&&g.m_lockerDoorLast==LockerMotion::Caught&&g.guardState(0)==GuardState::Alert,"Where he stands leaves a plain way out, and the doors are flung open (Caught) as he hauls");
  check(g.hullFits(g.player().pos,g.player().z,Player::StandingHeight)&&g.player().pos.x>out+.2f&&length(g.npcActors()[0].pose.position-g.player().pos)<1.2f,"The player lands standing just in front of the locker, beside the guard");
  check(walked>20&&carded,"Walking to the doors the witness wears the full red '?' of a hunt ("+std::to_string(walked)+" frames), so the player knows they are coming open");
  // The hold: racked as he hauls, the muzzle up at the player's face within a third of a second, held 1.2 s.
  const int hold=get(g,0,"guard_hold_ms");for(int i=0;i<20;++i)g.update({},frame);
  {const auto w=g.npcWeaponPose(0);const Point3 eye{g.player().pos.x,g.player().pos.y,g.player().z+g.player().eye},to=eye-w.muzzle;const float d=std::sqrt(to.x*to.x+to.y*to.y+to.z*to.z);const float aim=d>0?(to.x*w.direction.x+to.y*w.direction.y+to.z*w.direction.z)/d:0;
   report<<"  hold "<<hold<<" ms, after 20 frames "<<get(g,0,"guard_hold_ms")<<" ms: muzzle "<<w.muzzle.z<<" m up and "<<d<<" m off the eye ("<<eye.z<<"), raise "<<w.raise<<", aim "<<aim<<", guard "<<length(g.npcActors()[0].pose.position-g.player().pos)<<" m off\n";
   check(racked&&hold>=1150&&get(g,0,"guard_hold_ms")>0&&w.raise>.99f&&std::fabs(w.muzzle.z-eye.z)<.3f&&d>.3f&&aim>.95f,"Hauled out, he racks it, steps back to arm's length and holds the muzzle up at the player's face ("+std::to_string(d).substr(0,4)+" m off it) through the 1.2 s hold");}
 }
 {auto g=scene({{{8.5f,6.5f},-kPi*.5f+.73f}});auto&p=g.m_friendlyActors[0].pose;(void)p;const int need=g.npcActors()[0].track().ai.recognitionMs;
  g.rememberGuardTarget(0,g.resolveGuardTarget(0,{ShotKind::Player,0,0}));for(auto [f,v]:{std::pair{"guard_recognition",need*6/10},std::pair{"guard_mode",int(GuardState::Suspicious)}})g.setState(g.friendlyKey(0,f),v);
  hide(g);for(int i=0;i<60;++i)g.update({},frame);bool none=!get(g,0,"guard_locker");bool opened=false;for(int i=0;i<60*15;++i){g.update({},frame);opened|=g.hiddenLocker()<0;}
  check(none&&!opened,"A man who half had the player but only glimpsed the climb from the edge of his eye, in shadow, is no witness and never opens the doors");
 }
 {auto g=scene({{{8.5f,6.5f},-kPi*.5f}});hide(g);for(int i=0;i<60;++i)g.update({},frame);
  const Vec2 memory{get(g,0,"guard_x")*.001f,get(g,0,"guard_y")*.001f};bool none=!get(g,0,"guard_locker")&&g.guardState(0)==GuardState::Investigate&&length(memory-Vec2{out+1.1f,6.5f})<.2f;
  bool opened=false;for(int i=0;i<60*15;++i){g.update({},frame);opened|=g.hiddenLocker()<0;}
  check(none&&!opened,"A man who never had the player but saw the climb checks the floor in front of the bank and never opens it");
 }
 // Aborts: knocked out, or called away, mid-reach.
 for(int way=0;way<2;++way){auto g=scene({{{8.5f,6.5f},-kPi*.5f}});halfSeen(g,0);hide(g);int i=0;for(;i<60*15&&!get(g,0,"guard_haul");++i)g.update({},frame);
  if(way==0){g.setState(g.friendlyKey(0,"restraint"),1);g.m_friendlyActors[0].pose.clip=4;g.saveRestrainedPose(0);}else noise(g,{Sound::JunkMetal,{7.5f,6.5f},1,1,true,17,.5f});
  bool kept=true;for(int k=0;k<60*3;++k){g.update({},frame);kept&=g.hiddenLocker()==0;if(k==1||k==30)dump(g,0,"abort");}
  check(i<60*15&&kept&&!get(g,0,"guard_haul")&&!get(g,0,"guard_locker"),way==0?"A witness knocked out mid-reach lets go and the doors stay shut":"A witness called away by a noise mid-reach gives the locker up");
 }
 // 3. Stowing a body: E at a free locker with a man by the collar.
 {auto g=scene({{{10.5f,6.5f},kPi*.5f},{{9.5f,9.5f},kPi*.5f}});g.setState(g.friendlyKey(0,"restraint"),1);g.m_friendlyActors[0].pose.clip=4;g.m_friendlyActors[0].pose.position={4.05f,6.5f};g.saveRestrainedPose(0);g.m_draggedNpc=0;
  const std::string hint=g.interactionHint()?g.interactionHint():"";const int calls=g.m_lockerDoorCalls;g.update(use,frame);
  check(hint=="E / STOW THE BODY"&&!g.draggingBody()&&get(g,0,"guard_stowed")==1&&g.m_lockerDoorCalls==calls+1&&g.m_lockerDoorLast==LockerMotion::Stow,"Facing a free locker with a body, E / STOW THE BODY puts him in and opens the doors for it");
  float jump=0;auto at=g.npcActors()[0].pose.position;bool shown=true;std::vector<float> collar;for(int i=0;i<100;++i){g.update({},frame);jump=std::max(jump,length(g.npcActors()[0].pose.position-at));at=g.npcActors()[0].pose.position;if(i<80)shown&=!g.npcStowed(0);collar.push_back(g.npcCollar(0).z);}
  check(jump<.12f&&shown&&g.npcStowed(0)&&length(at-Vec2{out-LockerStowSeat,6.5f})<.05f,"He is dragged and heaved in over 1.4 s, drawn until the leaves shut, without a jump (largest step "+std::to_string(jump)+" m), then is shut in out of sight");
  // Lifted, not stood up: his shoulders come up at an even pace over about half a second from the mouth, to a slump.
  {const float low=*std::min_element(collar.begin(),collar.end()),high=collar.back(),standing=g.npcCollar(1).z;int from=-1,to=-1;float steepest=0;
   for(size_t i=0;i<collar.size();++i){if(from<0&&collar[i]>low+(high-low)*.05f)from=int(i);if(to<0&&collar[i]>=low+(high-low)*.95f)to=int(i);if(i)steepest=std::max(steepest,collar[i]-collar[i-1]);}
   report<<"  stow rise: collar "<<low<<" -> "<<high<<" m (standing "<<standing<<") from frame "<<from<<" to "<<to<<", steepest "<<steepest<<" m a frame\n";
   check(from>=30&&to-from>=22&&to-from<=36&&high<standing*.85f&&high>standing*.4f&&steepest<.07f,"A stowed body is lifted in from the mouth over about half a second ("+std::to_string((to-from)*1000/60)+" ms) to a slump, not stood up to attention ("+std::to_string(high)+" of "+std::to_string(standing)+" m)");}
  g.m_player.pos={3.2f,6.5f};g.m_player.angle=kPi;const std::string after=g.interactionHint()?g.interactionHint():"";
  check(g.nearbyBody()<0&&g.lockerAhead()<0&&after!="E / HIDE"&&!hide(g),"A stowed body is no body to drag, and his locker hides nobody else");
  // The second man turns to look straight at the locker, the player well behind him.
  {auto&look=g.m_friendlyActors[1].pose;look.yaw=std::atan2(-7.5f,-3.f);g.setState(g.friendlyKey(1,"yaw"),int(std::round(look.yaw*10000)));g.m_player.pos={12.5f,9.5f};}
  bool calm=true;for(int i=0;i<120;++i){g.update({},frame);calm&=!get(g,1,"guard_confirmed");}
  check(calm,"A guard looking at the locker sees no fallen colleague in it");
  Game loaded(g.m_customCampaign);bool restored=loaded.decodeSave(g.encodeSave());loaded.update({},frame);
  check(restored&&loaded.npcStowed(0)&&loaded.lockerBody(0,0)==1&&loaded.lockerAhead()<0,"A stowed body stays stowed through a save");
  // Opened: he tumbles out, and the man who opened it has found a body.
  auto put=[&](const char*f,int v){loaded.setState(loaded.friendlyKey(1,f),v);};put("guard_locker",1);put("guard_check",1);put("guard_checked",1);put("guard_check_said",0);put("guard_check_ms",0);put("guard_x",3035);put("guard_y",6500);put("guard_z",0);put("guard_memory",1);put("guard_mode",int(GuardState::Investigate));
  // Nobody hides in it, so nobody needs to hear him first: a second at the doors and he opens them.
  bool out2=false;int waited=-1;for(int i=0;i<60*15&&!out2;++i){loaded.update({},frame);if(waited<0&&loaded.state(loaded.friendlyKey(1,"guard_haul"))>0)waited=loaded.state(loaded.friendlyKey(1,"guard_doors_ms"));out2=!loaded.state(loaded.friendlyKey(0,"guard_stowed"));}
  // As for any found body he is sure someone did this: the hunt's red '?', not the dim one.
  const bool blamed=loaded.state(loaded.friendlyKey(1,"guard_confirmed"))&&loaded.state(loaded.friendlyKey(1,"guard_target_kind"))==int(ShotKind::Player)&&loaded.guardCard(1).kind==GuardCardKind::Hunting;
  // He falls out backwards as the leaves fly open, and they stay back on their stops while he lies across their arc;
  // and the man who opened them starts back out of his way.
  float peak=0,feetAtClose=-1,nearest=9,step=0;Vec2 last=loaded.npcActors()[0].pose.position;
  for(int i=0;i<150;++i){loaded.update({},frame);const float leaf=loaded.lockerDoorOpen(0,0,0);if(leaf<peak-.01f&&feetAtClose<0)feetAtClose=loaded.npcActors()[0].pose.position.x-out;peak=std::max(peak,leaf);
   const auto&body=loaded.npcActors()[0].pose;const Point3 c=loaded.npcCollar(0);step=std::max(step,length(body.position-last));last=body.position;
   nearest=std::min(nearest,segmentDistance(loaded.npcActors()[1].pose.position,body.position,{c.x,c.y}));}
  dump(loaded,1,"tumble");const auto&body=loaded.npcActors()[0].pose;const Point3 c=loaded.npcCollar(0);
  report<<"  tumble: boots "<<body.position.x-out<<" m out ("<<feetAtClose<<" as the leaves turned back), collar "<<c.x-out<<" m out, "<<c.z<<" m up, opener at least "<<nearest<<" m off him, largest step "<<step<<"; leaves at "<<loaded.lockerDoorOpen(0,0,0)<<" of a peak "<<peak<<", he waited "<<waited<<" ms at the doors, blamed "<<blamed<<" (card "<<int(loaded.guardCard(1).kind)<<")\n";
  check(out2&&!loaded.npcStowed(0)&&body.clip==4&&body.position.x-out>LockerTumbleFeet-.05f&&feetAtClose<0&&peak>.5f&&c.x-out>.8f&&c.z<.5f&&step<.06f&&nearest>.3f&&blamed&&waited>=LockerCheckPauseMs&&waited<LockerCheckPauseMs+100,"Opening it after a second at the doors ("+std::to_string(waited)+" ms), a guard has the body tumble out onto the floor in front of them, the leaves held open over him, and treats it as a found colleague (the red '?' of a hunt)");
  // Dragged clear of their arc, the leaves drift to.
  {auto&b=loaded.m_friendlyActors[0].pose;b.position={out+2.2f,6.5f};loaded.saveRestrainedPose(0);for(int i=0;i<200;++i)loaded.update({},frame);check(loaded.lockerDoorOpen(0,0,0)<.01f,"Once the body is out of their arc, the held leaves swing to");}
  bool corrupt=false;{Game bad(g.m_customCampaign);g.setState(g.friendlyKey(0,"guard_stow_ms"),LockerStowMs+1);corrupt=!bad.decodeSave(g.encodeSave());}
  check(corrupt,"A corrupt stow clock is refused on load");
 }
 // 4. Knocks from inside: one brings a look round in front; a second, close, a check.
 for(int knocks:{1,2}){auto g=scene({{{7.f,6.5f},kPi*.5f}});hide(g);knock(g);
  bool came=false,opened=false,suspect=false;int doors=0,lead=0;for(int i=0;i<60*20;++i){g.update({},frame);
   const float near=length(g.npcActors()[0].pose.position-Vec2{out,6.5f});came|=near<2.5f;
   if(knocks==2&&came&&!suspect&&(g.guardState(0)==GuardState::Investigate||g.guardState(0)==GuardState::Search)&&near<2.4f){knock(g);g.update({},frame);suspect=get(g,0,"guard_check")==1&&get(g,0,"guard_locker")==1;}
   if(get(g,0,"guard_haul")>0&&!doors){doors=get(g,0,"guard_doors_ms");lead=get(g,0,"guard_check_ms")-(get(g,0,"guard_check_said")-1);}
   if(g.hiddenLocker()<0){opened=true;break;}}
  dump(g,0,knocks==1?"knock1":"knock2");report<<"  came "<<came<<" suspect "<<suspect<<"\n";
  if(knocks==1)check(came&&!opened&&!get(g,0,"guard_locker"),"A single knock brings a guard to look round in front of the locker, and he leaves it shut");
  else check(suspect&&opened&&doors>=LockerCheckPauseMs&&lead>=LockerCheckLeadMs,"A second knock heard within 2.5 m makes it suspect: he says so, waits a second at the doors and hauls (paused "+std::to_string(doors)+" ms, spoke "+std::to_string(lead)+" ms before)");
 }
 // Stepping out during his pause is the player's ambush, not a catch.
 {auto g=scene({{{8.5f,6.5f},kPi*.5f}});hide(g);auto put=[&](const char*f,int v){g.setState(g.friendlyKey(0,f),v);};
  put("guard_locker",1);put("guard_check",1);put("guard_checked",1);put("guard_x",3035);put("guard_y",6500);put("guard_z",0);put("guard_memory",1);put("guard_mode",int(GuardState::Investigate));
  int i=0;for(;i<60*15&&get(g,0,"guard_doors_ms")<400;++i)g.update({},frame);g.update(use,frame);g.update({},frame);
  check(i<60*15&&g.hiddenLocker()<0&&!get(g,0,"guard_locker")&&g.pickupNotice()!="FOUND","Stepping out while he waits at the doors gets the player out first");
 }
 // 5. Hot dive: shut in seconds after a man had them, near where he had them.
 const Structure screen{5.f,4.5f,5.1f,8.5f,0,2.6f};
 auto spotted=[&](Game& g){g.m_player.pos={6.2f,8.8f};g.m_player.angle=0;for(int i=0;i<240&&!(get(g,0,"guard_confirmed")&&get(g,0,"guard_seen"));++i)g.update({},frame);return get(g,0,"guard_confirmed")!=0;};
 {auto g=scene({{{8.5f,6.5f},-kPi*.5f}},{screen});bool had=spotted(g);g.m_player.pos={3.2f,6.5f};g.m_player.angle=kPi;g.m_player.health=100;g.update({},frame);bool hidden=hide(g);
  for(int i=0;i<50;++i)g.update({},frame);const bool assigned=get(g,0,"guard_locker")==1&&get(g,0,"guard_check")==1&&get(g,0,"guard_checked")==1;
  int doors=0,lead=0;bool opened=false;for(int i=0;i<60*20&&!opened;++i){g.update({},frame);if(get(g,0,"guard_haul")>0&&!doors){doors=get(g,0,"guard_doors_ms");lead=get(g,0,"guard_check_ms")-(get(g,0,"guard_check_said")-1);}opened=g.hiddenLocker()<0;}
  dump(g,0,"hotdive");report<<"  had "<<had<<" hidden "<<hidden<<" assigned "<<assigned<<" opened "<<opened<<"\n";
  check(had&&hidden&&assigned&&opened&&doors>=LockerCheckPauseMs&&lead>=LockerCheckLeadMs,"Diving into a locker right after being seen, close by, has the searcher call it, wait at the doors and check it");
  check(get(g,0,"guard_checked")==1,"That search has had its one check");
 }
 {auto g=scene({{{8.5f,6.5f},-kPi*.5f}},{screen});spotted(g);g.m_player.pos={3.2f,6.5f};g.m_player.angle=kPi;g.setState(g.friendlyKey(0,"guard_checked"),1);g.update({},frame);hide(g);
  bool opened=false;for(int i=0;i<60*15;++i){g.update({},frame);opened|=g.hiddenLocker()<0;}
  check(!opened,"One check per search: a searcher who has already checked a locker does not check another");
 }
 {auto g=scene({{{8.5f,6.5f},-kPi*.5f}},{screen});spotted(g);g.m_player.pos={3.2f,6.5f};g.m_player.angle=kPi;for(int i=0;i<60*5;++i)g.update({},frame);hide(g);
  bool opened=false;for(int i=0;i<60*15;++i){g.update({},frame);opened|=g.hiddenLocker()<0;}
  check(!opened&&!get(g,0,"guard_check"),"Five seconds after he lost them the same dive is safe");
 }
 {auto g=scene({{{8.5f,6.5f},kPi*.5f}});for(size_t i=0;i<g.npcActors().size();++i)g.startGuardCaution(i);hide(g);
  bool opened=false;for(int i=0;i<60*15;++i){g.update({},frame);opened|=g.hiddenLocker()<0||get(g,0,"guard_check");}
  check(!opened,"A squad on Caution never checks a locker it has no reason to");
 }
 // 6. Fair search rules. A check, however it starts, is read before it lands:
 // he stops and turns square to the doors, says so, walks up, waits a second, hauls.
 struct Watched {bool assigned=false,opened=false,squared=true;int doors=0,lead=0;};
 auto follow=[&](Game& g,int seconds){Watched w;Vec2 from{};float moved=0;
  for(int i=0;i<60*seconds&&!w.opened;++i){g.update({},frame);const auto&p=g.npcActors()[0].pose;
   if(!w.assigned&&get(g,0,"guard_check")&&get(g,0,"guard_locker")==1){w.assigned=true;from=p.position;}
   if(w.assigned&&moved<.05f){moved=length(p.position-from);const Vec2 to=Vec2{out,6.5f}-p.position;if(moved>=.05f)w.squared=std::fabs(std::remainder(std::atan2(to.x,to.y)-p.yaw,2*kPi))<.35f;}
   if(get(g,0,"guard_haul")>0&&!w.doors){w.doors=get(g,0,"guard_doors_ms");w.lead=get(g,0,"guard_check_ms")-(get(g,0,"guard_check_said")-1);}
   w.opened=g.hiddenLocker()<0;}
  return w;};
 auto fair=[&](const Watched& w){return w.assigned&&w.opened&&w.squared&&w.doors>=LockerCheckPauseMs&&w.lead>=LockerCheckLeadMs;};
 auto describe=[&](const Watched& w){return " (assigned "+std::to_string(w.assigned)+", squared "+std::to_string(w.squared)+", paused "+std::to_string(w.doors)+" ms, spoke "+std::to_string(w.lead)+" ms before)";};
 // a. The doors heard: an arm's length walked in, five metres run in.
 {auto g=scene({{{3.f,7.4f},0}});const bool hidden=hide(g);const Watched w=follow(g,15);dump(g,0,"heard-near");
  check(hidden&&fair(w),"A guard at arm's length (back turned) who hears the latch turns to the locker, says it, waits at the doors and checks it"+describe(w));}
 for(int way=0;way<3;++way){auto g=scene({{{5.6f,7.6f},0}});
  if(way==2){g.m_player.pos={3.9f,6.5f};g.m_velocity={-5.4f,0};for(int i=0;i<10;++i)g.update({},frame);}else g.m_velocity={way?-5.4f:-3.6f,0};
  const bool hidden=hide(g);const Watched w=follow(g,20);dump(g,0,way==0?"walk-3m":way==1?"run-3m":"ran-3m");
  if(way==0)check(hidden&&!w.assigned&&!w.opened&&!get(g,0,"guard_check"),"A walking entry 3.3 m behind a guard is not heard");
  else check(hidden&&fair(w),std::string(way==1?"Running":"Having run, a sixth of a second before,")+" into a locker 3.3 m behind a guard is heard: he comes to check it"+describe(w));}
 {auto g=scene({{{8.5f,6.5f},kPi*.5f}});hide(g);for(int i=0;i<70;++i)g.update({},frame);auto&p=g.m_friendlyActors[0].pose;p.position={3.f,7.4f};p.yaw=0;
  for(auto [f,v]:{std::pair{"x",3000},std::pair{"y",7400},std::pair{"yaw",0}})g.setState(g.friendlyKey(0,f),v);
  g.update(use,frame);const bool assigned=g.hiddenLocker()<0&&get(g,0,"guard_locker")==1&&get(g,0,"guard_check")==1;dump(g,0,"exit");
  check(assigned,"Stepping out at a guard's back, he hears the doors and turns to that locker");}
 // b. A confirmed search that ends within 1.5 m of the doors takes in the locker; 2.5 m off it does not.
 // (Set a second after the climb in, so the entry's own watch has closed.)
 auto hunt=[&](Game& g,Vec2 point,bool confirmed){g.rememberGuardTarget(0,g.resolveGuardTarget(0,{ShotKind::Player,0,0}));if(!confirmed)g.rememberGuardTarget(0,{});const int need=g.npcActors()[0].track().ai.recognitionMs;
  for(auto [f,v]:{std::pair{"guard_confirmed",int(confirmed)},std::pair{"guard_recognition",confirmed?need:0},std::pair{"guard_mode",int(GuardState::Investigate)},std::pair{"guard_x",int(std::round(point.x*1000))},std::pair{"guard_y",int(std::round(point.y*1000))},std::pair{"guard_z",0},std::pair{"guard_memory",1},std::pair{"guard_travel",0}})g.setState(g.friendlyKey(0,f),v);};
 for(float off:{1.44f,2.5f}){auto g=scene({{{8.5f,6.5f},kPi*.5f}});hide(g);for(int i=0;i<60;++i)g.update({},frame);hunt(g,{out+off*.83f,6.5f+off*.55f},true);const Watched w=follow(g,20);dump(g,0,"search");
  if(off<2)check(fair(w),"A confirmed search for the player that ends 1.4 m from the doors checks the locker first"+describe(w));
  else check(!w.assigned&&!w.opened,"A confirmed search that ends 2.5 m off the doors leaves the locker alone");}
 // c. On Caution an unconfirmed searcher near the doors checks on a hunch: the
 // same man, locker and search point always decide alike, about one in three.
 {Vec2 hit{},miss{};bool foundHit=false,foundMiss=false;int hits=0,tries=0;
  for(int x=0;x<12;++x)for(int y=0;y<12;++y){const int gx=int(std::round((out+.6f+x*.06f)*1000)),gy=int(std::round((6.f+y*.09f)*1000));const bool h=lockerHunch(0,0,gx,gy);hits+=h;++tries;
   if(h&&!foundHit){hit={gx*.001f,gy*.001f};foundHit=true;}if(!h&&!foundMiss){miss={gx*.001f,gy*.001f};foundMiss=true;}}
  report<<"  hunch "<<hits<<'/'<<tries<<" hit "<<hit.x<<','<<hit.y<<" miss "<<miss.x<<','<<miss.y<<'\n';
  check(foundHit&&foundMiss&&hits*100>=tries*25&&hits*100<=tries*45,"A hunch comes up about one time in three ("+std::to_string(hits)+"/"+std::to_string(tries)+")");
  for(int way=0;way<3;++way){auto g=scene({{{8.5f,6.5f},kPi*.5f}});hide(g);for(int i=0;i<60;++i)g.update({},frame);if(way<2)g.startGuardCaution(0);hunt(g,way==1?miss:hit,false);const Watched w=follow(g,20);dump(g,0,"hunch");
   if(way==0)check(fair(w),"On Caution, an unconfirmed search that ends by the doors checks the locker when the hunch says so"+describe(w));
   else check(!w.assigned&&!w.opened,way==1?"On Caution, the same search with no hunch leaves it shut":"Off Caution an unconfirmed search never checks a locker");}}
 // d. A man on his beat never checks one, Caution or not, even posted 1.3 m from its doors.
 {auto g=scene({{{out+1.3f,6.5f},kPi*.5f}});g.startGuardCaution(0);hide(g);Watched w=follow(g,15);
  check(!w.assigned&&!w.opened&&g.guardState(0)==GuardState::Patrol,"A guard on his beat by the doors, on Caution, never checks the locker");}
 // 7. The burst is a blow, not a switch: he stands until a leaf's edge reaches
 // him, is falling within three frames of it, is shoved back over ~0.15 s,
 // and is counted out only when he lands.
 report<<"  fall clip "<<cleanupAnimationDuration(ActorVisual::Cleanup,4)<<" s; phase at shoulder lift";for(float lift:{1.f,.96f,.9f,.8f,.62f,.4f,.2f,.1f,0.f})report<<' '<<lift<<':'<<fallPhaseAtLift(ActorVisual::Cleanup,lift);report<<'\n';
 for(float off:{.6f,.45f}){auto g=scene({{{8.5f,6.5f},kPi*.5f}});hide(g);for(int i=0;i<70;++i)g.update({},frame);auto&p=g.m_friendlyActors[0].pose;p.position={out+off,6.5f};p.yaw=-kPi*.5f;
  for(auto [f,v]:{std::pair{"x",int(std::round((out+off)*1000))},std::pair{"y",6500},std::pair{"yaw",int(std::round(-kPi*.5f*10000))},std::pair{"guard_mode",int(GuardState::Investigate)}})g.setState(g.friendlyKey(0,f),v);
  const bool aimed=g.lockerBurstTarget()==0;g.update(use,frame);const bool standing=!g.npcIncapacitated(0)&&g.npcActors()[0].pose.clip!=4;bool bang=false;for(const auto&e:g.sounds())bang|=e.sound==Sound::LockerBang;
  int hit=-1,landed=-1,noticed=-1,framed=0,falling=0;float leafAtHit=0,collarAtHit=0,collar3=0,biggest=0;Vec2 before=g.npcActors()[0].pose.position,start=before;bool punched=false,landSeen=false;
  // Where his collar falls in the player's view (the renderer's projection at 960 x 540).
  auto inView=[&](Point3 p,float* across=nullptr){const Player v=g.view();const float cy=std::cos(v.angle),sy=std::sin(v.angle),cp=std::cos(v.pitch/140.f),sp=std::sin(v.pitch/140.f),focal=960*.65f;const Point3 r{p.x-v.pos.x,p.y-v.pos.y,p.z-(v.z+v.eye)};
   const float depth=r.x*cy*cp+r.y*sy*cp+r.z*sp,x=(-r.x*sy+r.y*cy)/std::max(.01f,depth)*focal,y=(-r.x*cy*sp-r.y*sy*sp+r.z*cp)/std::max(.01f,depth)*focal;if(across)*across=x;return depth>.06f&&std::fabs(x)<480&&std::fabs(y)<270;};
  float drift=0;
  for(int i=1;i<150;++i){g.update({},frame);const auto&n=g.npcActors()[0].pose;const Point3 c=g.npcCollar(0);
   if(hit<0&&g.npcIncapacitated(0)){hit=i;leafAtHit=std::max(g.lockerDoorOpen(0,0,0),g.lockerDoorOpen(0,0,1))*110;collarAtHit=c.z;for(const auto&e:g.sounds())punched|=e.sound==Sound::PunchHit;}
   if(hit>0&&i==hit+3)collar3=c.z;if(hit>0)biggest=std::max(biggest,length(n.position-before));before=n.position;
   if(hit>0&&noticed<0){++falling;float x=0;framed+=inView(c,&x);drift=std::max(drift,std::fabs(x));}
   if(noticed<0&&g.pickupNotice()=="GUARD UNCONSCIOUS"){noticed=i;landed=c.z<.5f?i:-1;landSeen=inView(c);}}
  const float pushed=g.npcActors()[0].pose.position.x-start.x;const Point3 rest=g.npcCollar(0);
  report<<"  burst at "<<off<<" m: hit on frame "<<hit<<" with the leaves "<<leafAtHit<<" deg out, collar "<<collarAtHit<<" -> "<<collar3<<" three frames on, shoved "<<pushed<<" m (largest step "<<biggest<<"), counted out on frame "<<noticed<<"; collar at rest "<<rest.x-start.x<<" m out, "<<rest.y-start.y<<" m across, "<<rest.z<<" m up\n";
  report<<"  his collar in the player's view on "<<framed<<" of "<<falling<<" frames of the fall (at most "<<drift<<" px off the middle across), landing seen "<<landSeen<<"\n";
  check(landSeen&&framed*10>=falling*7,"Bursting out at "+std::to_string(off).substr(0,4)+" m, the view goes down with him: his collar is in frame on "+std::to_string(framed)+" of "+std::to_string(falling)+" frames of the fall, and when he lands");
  check(aimed&&standing&&bang&&hit>=(off>.5f?2:1)&&hit<=6&&leafAtHit>25&&punched,"Bursting out at a man "+std::to_string(off).substr(0,4)+" m off the doors: he stands until a leaf reaches him (frame "+std::to_string(hit)+", the leaves "+std::to_string(int(leafAtHit))+" degrees out), and the blow lands with it");
  check(hit>0&&collar3<collarAtHit-.015f&&pushed>.2f&&pushed<.36f&&biggest<.07f,"He is falling within three frames of the hit and is shoved back off the doors over a few frames, not in one ("+std::to_string(biggest)+" m largest step)");
  check(noticed>hit+10&&landed==noticed,"GUARD UNCONSCIOUS comes up when he lands (frame "+std::to_string(noticed)+"), not under a standing man");
 }
 // 8. EVASION's floor: a confirmed hunt for the player is searched for at least 20 s.
 {auto g=scene({{{8.5f,6.5f},kPi*.5f}});hide(g);for(int i=0;i<60;++i)g.update({},frame);hunt(g,{6.5f,6.5f},true);g.update({},frame);const int clock=g.guardHuntLeft(0);int searched=0;
  for(int i=0;i<60*10&&!searched;++i){g.update({},frame);if(g.guardState(0)==GuardState::Search)searched=get(g,0,"guard_search");}
  check(clock>=HuntSearchMs&&searched>=HuntSearchMs-50,"A confirmed hunt gives the search at least 20 s: EVASION reads "+std::to_string(clock)+" ms and the search runs "+std::to_string(searched)+" ms");}
 // 9. The hold in the campaign's own lockers (map 57's west wall, map 59's bunk
 // room), hauled out as the review saw it: for every frame of guard_hold_ms the
 // muzzle points within 10 degrees of the player's eye, and he has stepped back
 // off them (along the doors' normal, or along the wall where that is blocked).
 for(auto [level,at,angle]:{std::tuple{57,Vec2{2.55f,2.87f},kPi},std::tuple{59,Vec2{17.4f,15.25f},kPi*.5f}}){
  auto g=mapInspection(at,angle,0,level,false,-999,true);g.stowWeapon();g.update(use,frame);for(int i=0;i<70;++i)g.update({},frame);
  const int index=g.hiddenLocker();const bool staged=index>=0&&g.stageLockerCatch();int guard=-1;
  for(size_t a=0;a<g.npcActors().size();++a)if(get(g,a,"guard_hold_ms")>0)guard=int(a);
  const Vec2 start=guard>=0?g.npcActors()[size_t(guard)].pose.position:Vec2{};const float apart=guard>=0?length(start-g.player().pos):0;
  int frames=0,offAim=0;float worst=0,gasX=0,gasY=0,steepest=-90;
  for(int i=0;i<90&&guard>=0;++i){g.update({},frame);if(get(g,size_t(guard),"guard_hold_ms")<=0)break;++frames;
   const auto&npc=g.npcActors()[size_t(guard)];const auto w=g.npcWeaponPose(size_t(guard));const Point3 eye{g.player().pos.x,g.player().pos.y,g.player().z+g.player().eye},to=eye-w.muzzle;
   const float d=std::sqrt(to.x*to.x+to.y*to.y+to.z*to.z),aim=std::acos(std::clamp(d>0?(to.x*w.direction.x+to.y*w.direction.y+to.z*w.direction.z)/d:-1.f,-1.f,1.f))*180/kPi;
   worst=std::max(worst,aim);offAim+=aim>10;steepest=std::max(steepest,std::asin(std::clamp(w.direction.z,-1.f,1.f))*180/kPi);
   // His gasmask in the player's view (the view package frames it; reported here).
   const Player v=g.view();const float cy=std::cos(v.angle),sy=std::sin(v.angle),cp=std::cos(v.pitch/140.f),sp=std::sin(v.pitch/140.f),focal=960*.65f;
   const Point3 r{npc.pose.position.x-v.pos.x,npc.pose.position.y-v.pos.y,npc.pose.z+npc.track().scale*.93f-(v.z+v.eye)};const float depth=r.x*cy*cp+r.y*sy*cp+r.z*sp;
   if(depth>.05f){gasX=std::max(gasX,std::fabs((-r.x*sy+r.y*cy)/depth*focal));gasY=std::max(gasY,std::fabs((-r.x*cy*sp-r.y*sy*sp+r.z*cp)/depth*focal));}}
  const float held=guard>=0?length(g.npcActors()[size_t(guard)].pose.position-g.player().pos):0;
  report<<"  hold in map "<<level<<": guard "<<guard<<", "<<frames<<" frames, muzzle off the eye by up to "<<worst<<" deg ("<<offAim<<" frames over 10), the barrel at most "<<steepest<<" deg up; he stood "<<apart<<" m off the player and holds from "<<held<<" m; gasmask up to "<<gasX<<","<<gasY<<" px off the middle of the view\n";
  check(staged&&guard>=0&&frames>=60&&offAim==0&&held>apart+.15f,"Map "+std::to_string(level)+": hauled out, the muzzle is on the player's eye for the whole hold (at worst "+std::to_string(worst).substr(0,4)+" degrees off) and he steps back to "+std::to_string(held).substr(0,4)+" m");
 }
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

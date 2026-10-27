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
// then stood up into the locker facing its back wall, his feet LockerStowSeat
// behind the doors (the middle, so no boot shows under the shut leaves), the
// knockout's fall run back to LockerStowSlump. Opened, he is turned to the
// doors and the rest of that fall carries him out over their foot.
constexpr float LockerStowMouth=.25f,LockerStowSeat=.32f,LockerStowSlump=.12f;
// The handle, where the leaves meet, and how square he stands to the doors before reaching.
constexpr float LockerHandleZ=1.05f,LockerHandleOut=.03f,LockerHaulSquare=.12f;
}
void Game::validateGuardLocker(size_t actor)const{
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};
 if(get("guard_check")<0||get("guard_check")>1||get("guard_checked")<0||get("guard_checked")>1||get("guard_check_ms")<0||get("guard_check_ms")>60000||get("guard_check_said")<0||get("guard_check_said")>60001||get("guard_doors_ms")<0||get("guard_doors_ms")>60000||get("guard_haul")<0||get("guard_haul")>LockerHaulMs)throw std::runtime_error("Invalid saved guard locker check");
 if(get("guard_stowed")<0||get("guard_stowed")>65536||get("guard_stowed_level")<0||get("guard_stowed_level")>chunkCount()||(get("guard_stowed")&&!get("guard_stowed_level"))||get("guard_stow_ms")<0||get("guard_stow_ms")>LockerStowMs||get("guard_stow_x")<0||get("guard_stow_x")>24000||get("guard_stow_y")<0||get("guard_stow_y")>24000||std::abs(get("guard_stow_z"))>100000)throw std::runtime_error("Invalid saved stowed body");
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
 suspectLocker(actor,chosen);(void)level;return true;
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
 if(suspect&&(doors<LockerCheckPauseMs||!get("guard_check_said")||check<get("guard_check_said")-1+LockerCheckLeadMs))return 1;
 int haul=std::min(LockerHaulMs,get("guard_haul")+ms);put("guard_haul",haul);if(haul<LockerHaulMs)return 1;
 lockerDoorsMove(m_level,index,LockerMotion::Caught);
 if(hiddenLocker()==index){
  if(npcVoice(actor,NpcVoiceCue::Found))put("voice_mode",int(GuardState::Alert));
  put("guard_haul",0);const bool pulled=pullFromLocker(level,actor,pose);clearGuardLocker(actor);return pulled?2:0;
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
 else{const float u=std::min(1.f,(t-.42f)/.33f),s=u*u*(3-2*u);at=mouth+(inside-mouth)*s;feet=floor;phase=1-(1-LockerStowSlump)*s;pose.yaw=std::atan2(-out.x,-out.y);}
 pose.position=at;pose.z=feet;pose.clip=4;pose.phase=phase;pose.lookAtActor=-1;
 m_friendlyActors[actor].pose=pose;saveRestrainedPose(actor);
}
// The doors open on a stowed body: he tumbles out across their foot, and the
// man who opened them has found a fallen colleague (Mind.cpp's casualty rules).
void Game::tumbleFromLocker(int level,int fixture,size_t finder){
 const int body=lockerBody(level,fixture)-1;if(body<0)return;const size_t actor=size_t(body);const auto&f=friendlyWorld(level).fixtures()[size_t(fixture)];
 for(const char*field:{"guard_stowed","guard_stowed_level","guard_stow_ms","guard_stow_fraction","guard_stow_x","guard_stow_y","guard_stow_z"})setState(friendlyKey(actor,field),0);
 // Where he slumped, turned out to the doors while they are still shut: the
 // rest of his fall carries him out over their foot as they fly open.
 auto&npc=m_friendlyActors[actor];const Vec2 out=doorsOut(f);const Vec2 shift=chunkOffset(level)-chunkOffset(npc.level);
 const float fall=cleanupAnimationDuration(npc.track().visual,4);const int duration=fall>0?std::max(1,int(std::round(fall*1000))):1000;
 npc.pose.position=doorsFront(f,-LockerStowSeat)+shift;npc.pose.z=friendlyWorld(level).floorHeight(f.position.x,f.position.y)+f.base;npc.pose.yaw=std::atan2(out.x,out.y);npc.pose.clip=4;npc.pose.phase=LockerStowSlump;
 setState(friendlyKey(actor,"restraint_ms"),int(std::round(LockerStowSlump*duration)));setState(friendlyKey(actor,"restraint_fraction"),0);saveRestrainedPose(actor);
 m_sounds.push_back({Sound::Land,doorsFront(f,.3f)+chunkOffset(level)-chunkOffset(m_level),.6f,.85f,true,12,npc.pose.z+.1f});
 if(finder>=m_friendlyActors.size()||npcIncapacitated(finder))return;
 auto put=[&](const char*field,int value){setState(friendlyKey(finder,field),value);};const Vec2 global=npc.pose.position+chunkOffset(npc.level);
 AiTarget blame{ShotKind::Player,m_level,0};blame.position=m_player.pos+chunkOffset(m_level)-chunkOffset(m_friendlyActors[finder].level);blame.feet=m_player.z;blame.height=m_player.hullHeight();blame.eye=m_player.eye;blame.faction=AiFaction::Player;
 rememberGuardTarget(finder,blame);put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(npc.pose.z*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);
 // He blames the player but has not seen them: he searches from the body and
 // radios it in, so the squad goes to Caution as for any found body.
 put("guard_confirmed",0);put("guard_mode",int(GuardState::Investigate));if(!state(friendlyKey(finder,"guard_call")))startGuardCall(finder,CallBody);
 if(npcVoice(finder,NpcVoiceCue::AllyDown))put("voice_mode",int(GuardState::Investigate));
}
void Game::inspectLockerGuard(bool stow,bool staged){
 size_t chosen=SIZE_MAX;float best=1e9f;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();
  if(npc.level!=m_level||t.ai.mode!=ActorAiMode::Guard||!autonomousActor(t)||npcIncapacitated(actor)||npc.pose.clip==4||(t.deadState&&state(t.deadState)))continue;
  const float d=length(npc.pose.position-m_player.pos);if(d<best){best=d;chosen=actor;}}
 if(chosen==SIZE_MAX)return;auto put=[&](const char*field,int value){setState(friendlyKey(chosen,field),value);};
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
  int reachStart=-1,pulledAt=-1,calls=g.m_lockerDoorCalls;float standOut=0,handleReach=0,handleGap=99;bool exitClear=false;
  for(int i=0;i<60*15&&pulledAt<0;++i){g.update({},frame);
   if(reachStart<0&&get(g,0,"guard_haul")>0){reachStart=i;const auto&p=g.npcActors()[0].pose;standOut=p.position.x-out;Vec2 spot;float feet;exitClear=g.lockerExit(g.m_world.fixtures()[0],spot,feet);}
   if(get(g,0,"guard_haul")>0){auto w=g.npcWeaponPose(0);handleReach=std::max(handleReach,w.haul);handleGap=std::min(handleGap,std::fabs(w.handle.x-out));}
   if(g.hiddenLocker()<0)pulledAt=i;}
  const float reachMs=(pulledAt-reachStart)*1000.f/60;
  check(reachStart>=0&&standOut>.5f&&standOut<.7f,"The witness stops about 0.6 m in front of the doors ("+std::to_string(standOut)+" m)");
  check(pulledAt>reachStart&&reachMs>=300&&reachMs<=400&&handleReach>.99f&&handleGap<.1f,"He reaches for the handle for about 350 ms before the doors open ("+std::to_string(int(reachMs))+" ms)");
  check(exitClear&&g.m_lockerDoorCalls>calls&&g.m_lockerDoorLast==LockerMotion::Caught&&g.guardState(0)==GuardState::Alert,"Where he stands leaves a plain way out, and the doors are flung open (Caught) as he hauls");
  check(g.hullFits(g.player().pos,g.player().z,Player::StandingHeight)&&g.player().pos.x>out+.2f&&length(g.npcActors()[0].pose.position-g.player().pos)<1.2f,"The player lands standing just in front of the locker, beside the guard");
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
  float jump=0;auto at=g.npcActors()[0].pose.position;bool shown=true;for(int i=0;i<100;++i){g.update({},frame);jump=std::max(jump,length(g.npcActors()[0].pose.position-at));at=g.npcActors()[0].pose.position;if(i<80)shown&=!g.npcStowed(0);}
  check(jump<.12f&&shown&&g.npcStowed(0)&&length(at-Vec2{out-LockerStowSeat,6.5f})<.05f,"He is dragged and heaved in over 1.4 s, drawn until the leaves shut, without a jump (largest step "+std::to_string(jump)+" m), then is shut in out of sight");
  g.m_player.pos={3.2f,6.5f};g.m_player.angle=kPi;const std::string after=g.interactionHint()?g.interactionHint():"";
  check(g.nearbyBody()<0&&g.lockerAhead()<0&&after!="E / HIDE"&&!hide(g),"A stowed body is no body to drag, and his locker hides nobody else");
  // The second man turns to look straight at the locker, the player well behind him.
  {auto&look=g.m_friendlyActors[1].pose;look.yaw=std::atan2(-7.5f,-3.f);g.setState(g.friendlyKey(1,"yaw"),int(std::round(look.yaw*10000)));g.m_player.pos={12.5f,9.5f};}
  bool calm=true;for(int i=0;i<120;++i){g.update({},frame);calm&=!get(g,1,"guard_confirmed");}
  check(calm,"A guard looking at the locker sees no fallen colleague in it");
  Game loaded(g.m_customCampaign);bool restored=loaded.decodeSave(g.encodeSave());loaded.update({},frame);
  check(restored&&loaded.npcStowed(0)&&loaded.lockerBody(0,0)==1&&loaded.lockerAhead()<0,"A stowed body stays stowed through a save");
  // Opened: he tumbles out, and the man who opened it has found a body.
  auto put=[&](const char*f,int v){loaded.setState(loaded.friendlyKey(1,f),v);};put("guard_locker",1);put("guard_check",1);put("guard_check_said",1);put("guard_check_ms",3000);put("guard_x",3035);put("guard_y",6500);put("guard_z",0);put("guard_memory",1);put("guard_mode",int(GuardState::Investigate));
  bool out2=false;for(int i=0;i<60*15&&!out2;++i){loaded.update({},frame);out2=!loaded.state(loaded.friendlyKey(0,"guard_stowed"));}
  for(int i=0;i<30;++i)loaded.update({},frame);dump(loaded,1,"tumble");
  check(out2&&!loaded.npcStowed(0)&&loaded.npcActors()[0].pose.clip==4&&std::fabs(loaded.npcActors()[0].pose.position.x-(out-LockerStowSeat))<.05f&&std::cos(loaded.npcActors()[0].pose.yaw-kPi*.5f)>.99f&&!loaded.state(loaded.friendlyKey(1,"guard_confirmed"))&&loaded.state(loaded.friendlyKey(1,"guard_target_kind"))==int(ShotKind::Player),"Opening it, a guard has the body tumble out at his feet and treats it as a found colleague");
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
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

#include "Game.h"
#include "../world/ModelIds.h"
#include <cmath>
// Locker doors and the camera's way in and out of a locker. Every hiding
// locker is the retro pack's double-door locker: two leaves hung on the outer
// edges of its open front (local +x), meeting in the middle. The game commits
// hiding, stepping out and being pulled out instantly (saves, guards and tests
// see only that); this file plays the leaves and the camera over the next
// fraction of a second, and none of it is saved: a save mid-motion loads at
// the committed end.
namespace retro {
namespace {
constexpr float Degree=kPi/180.f;
constexpr float FullSwing=110*Degree,WideOpen=95*Degree; // lockerDoorOpen()==1, and the ordinary swing
constexpr float LockerInset=.14f; // the eye behind the leaves' plane, shut in
Vec2 out(const Fixture& f){return {std::cos(f.yaw),-std::sin(f.yaw)};}
Vec2 front(const Fixture& f,float ahead){return f.position+out(f)*(f.width*.5f+ahead);}
float ease(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
float easeOut(float t){t=std::clamp(t,0.f,1.f);return 1-(1-t)*(1-t)*(1-t);}
// Each motion's leaf angle over time, from the angle the leaf stood at.
float lengthOf(Game::LockerMotion m){using M=Game::LockerMotion;return m==M::Enter?.9f:m==M::Exit?1.f:m==M::Caught?2.4f:m==M::Stow?1.4f:.25f;}
float angleAt(Game::LockerMotion m,float t,float from,float lead=0){using M=Game::LockerMotion;
 auto swing=[&](float open,float peak,float hold,float shut){if(t<lead+open)return from+(peak-from)*easeOut((t-lead)/open);if(t<hold)return peak;return peak*(1-ease((t-hold)/shut));};
 switch(m){
  case M::Enter:return swing(.25f,WideOpen,.65f,.25f);   // the hands take the edges (lead), open, the player turns in, pulled shut over them
  case M::Exit:return swing(.15f,WideOpen,.5f,.5f);      // flung open, then they swing to on their own
  case M::Caught:return swing(.15f,FullSwing,1.15f,1.25f);// ripped back to the stops, drifting back after
  case M::Stow:return swing(.25f,WideOpen,1.05f,.35f);
  case M::Knock:return from+3*Degree*std::fabs(std::sin(t*kPi*24))*(1-t/.25f); // a rattle on the latch
 }return 0;
}
}
// Each leaf's swing stops short of anything it would pass through: a wall or
// cabinet beside the locker, probed along the arc its free edge sweeps.
void Game::animateLockerDoor(int level,int fixture,LockerMotion motion){
 if(level<0||level>=chunkCount()||fixture<0)return;const auto&world=level==m_level?m_world:chunkWorld(level);
 if(size_t(fixture)>=world.fixtures().size()||world.fixtures()[size_t(fixture)].model!=model::psx_locker)return;
 auto found=std::find_if(m_lockerDoors.begin(),m_lockerDoors.end(),[&](const LockerDoor&d){return d.level==level&&d.fixture==fixture;});
 if(found==m_lockerDoors.end()){LockerDoor d;d.level=level;d.fixture=fixture;found=m_lockerDoors.insert(m_lockerDoors.end(),d);}
 auto&d=*found;const auto&f=world.fixtures()[size_t(fixture)];const Vec2 o=out(f),side{-o.y,o.x};
 const float floor=world.floorHeight(f.position.x,f.position.y)+f.base,sx=f.width/.64f,sz=f.depth/.9f;
 for(int leaf=0;leaf<2;++leaf){const float hand=leaf?-1.f:1.f,hinge=(leaf?.4412f:.432f)*sz,length=(leaf?.443f:.43f)*sz;
  const Vec2 pivot=f.position+o*(.32f*sx)+side*(hinge*hand);float limit=FullSwing;
  for(float a=40*Degree;a<=FullSwing+.01f;a+=5*Degree){const Vec2 edge=side*(-hand*std::cos(a))+o*std::sin(a);bool clear=true;
   for(float along:{.5f,.97f}){const Vec2 p=pivot+edge*(length*along);for(float z:{.4f,1.2f,2.f})clear&=world.fits(p.x,p.y,floor+z,.05f,false);}
   if(!clear){limit=a-6*Degree;break;}}
  d.limit[size_t(leaf)]=std::max(limit,30*Degree);d.from[size_t(leaf)]=d.angle[size_t(leaf)];
 }
 d.motion=motion;d.age=0;d.length=lengthOf(motion);d.lead=0;d.shut=false;
}
float Game::lockerDoorOpen(int level,int fixture,int leaf)const{
 for(const auto&d:m_lockerDoors)if(d.level==level&&d.fixture==fixture)return std::clamp(d.angle[size_t(leaf&1)]/FullSwing,0.f,1.f);
 return 0;
}
// The doors' own sounds after the first: hinges taking the weight as they
// swing back, and the shut, quiet enough that only someone at arm's length
// would notice (the latch, bang and knock go out with the commit itself).
void Game::updateLockerDoors(float dt){
 const bool handsOn=lockerHands();
 if(dead())m_lockerView.active=false;
 // The way out over, the gun comes back up with its ordinary raise.
 if(m_lockerView.active&&(m_lockerView.age+=dt)>=m_lockerView.length){m_lockerView.active=false;if(m_lockerView.motion!=LockerMotion::Enter)lockerRearm();}
 for(auto&d:m_lockerDoors){const float before=d.age;d.age+=dt;const float t=std::min(d.age,d.length);
  for(int leaf=0;leaf<2;++leaf)d.angle[size_t(leaf)]=std::clamp(angleAt(d.motion,t,d.from[size_t(leaf)],d.lead),0.f,d.limit[size_t(leaf)]);
  // A stow's hands let go as the leaves meet: nothing pops back into them.
  if(d.motion==LockerMotion::Stow&&before<d.length&&d.age>=d.length&&d.level==m_level&&handsOn&&!m_weaponEquipped&&!draggingBody()&&!holdingClutter())m_handsLowered=true;
  if(d.level!=m_level||size_t(d.fixture)>=m_world.fixtures().size())continue;
  const auto&f=m_world.fixtures()[size_t(d.fixture)];const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
  auto at=[&](float moment){return before<moment&&d.age>=moment;};
  auto emit=[&](Sound s,float gain,float pitch){SoundEvent e{s,front(f,.02f),gain,pitch,true,8,floor+1.2f};if(s==Sound::LockerShut)e.aiRadius=1.2f;m_sounds.push_back(e);};
  const float creak=d.motion==LockerMotion::Enter?.6f:d.motion==LockerMotion::Exit?.5f:d.motion==LockerMotion::Caught?1.15f:d.motion==LockerMotion::Stow?1.05f:-1;
  if(creak>0&&at(creak))emit(Sound::LockerCreak,.3f,d.motion==LockerMotion::Caught?.9f:1.f);
  if(d.motion!=LockerMotion::Knock&&at(d.length))emit(Sound::LockerShut,d.motion==LockerMotion::Enter?.42f:.5f,d.motion==LockerMotion::Caught?.95f:1.05f);
 }
 std::erase_if(m_lockerDoors,[](const LockerDoor&d){return d.age>=d.length&&d.angle[0]<=0&&d.angle[1]<=0;});
}
void Game::startLockerView(LockerMotion motion,const Player& from,int fixture,float facePitch){
 if(m_vrInputActive)return; // the headset is the camera: no path ever moves it
 m_lockerView={true,motion,0,motion==LockerMotion::Enter?.9f:motion==LockerMotion::Exit?.5f:.45f,from,fixture,facePitch};
}
void Game::clearLockerMotion(bool doors){const bool out=m_lockerView.active&&m_lockerView.motion!=LockerMotion::Enter;m_lockerView.active=false;if(doors)m_lockerDoors.clear();if(out)lockerRearm();}
bool Game::lockerViewLocked()const{return m_lockerView.active&&!dead();}
int Game::lockerViewFixture()const{return m_lockerView.active?m_lockerView.fixture:hiddenLocker();}
float Game::lockerShutView()const{
 if(m_vrInputActive)return 0;
 if(!m_lockerView.active)return hiddenLocker()>=0?1.f:0.f;
 const float t=m_lockerView.age;
 // The perforated inner face takes over the view as the leaves meet in front of the eye.
 if(m_lockerView.motion==LockerMotion::Enter)return ease((t-.7f)/.2f);
 if(m_lockerView.motion==LockerMotion::Caught)return 0; // the real leaves are what rips away
 return 1-ease(t/.07f);
}
Player Game::view()const{
 Player v=m_player;if(dead()||m_vrInputActive)return v;
 const auto&fixtures=m_world.fixtures();const int hidden=hiddenLocker();
 auto inside=[&](int index,Player p){p.pos=front(fixtures[size_t(index)],-LockerInset);return p;};
 const Player to=hidden>=0?inside(hidden,v):v; // the committed end of any path
 if(!m_lockerView.active||m_lockerView.fixture<0||size_t(m_lockerView.fixture)>=fixtures.size())return to;
 const auto&f=fixtures[size_t(m_lockerView.fixture)];const auto&a=m_lockerView.from;const float t=m_lockerView.age;v=to;
 auto mix=[](float x,float y,float k){return x+(y-x)*k;};
 auto turn=[](float from,float target,float k){return wrapAngle(from+wrapAngle(target-from)*k);};
 switch(m_lockerView.motion){
  case LockerMotion::Enter:{
   // A step to the open doors, the about-turn on the spot, then backing in as
   // the leaves come to; the two moves overlap so the body never stops dead.
   const Vec2 mouth=front(f,.26f),toMouth=a.pos+(mouth-a.pos)*ease(t/.4f);
   v.pos=toMouth+(to.pos-toMouth)*ease((t-.35f)/.45f);
   v.angle=turn(a.angle,to.angle,ease((t-.2f)/.45f));v.pitch=mix(a.pitch,to.pitch,ease(t/.5f));
   v.z=mix(a.z,to.z,ease(t/.3f));v.eye=mix(a.eye,to.eye,ease(t/.3f));break;}
  case LockerMotion::Exit:{
   // The leaves fly open first; the body follows them out. Burst into a man
   // at the doors, the blow comes back up the arms: a short hard jolt.
   const float k=ease((t-.06f)/.44f);v.pos=a.pos+(to.pos-a.pos)*k;v.z=mix(a.z,to.z,k);
   v.angle=turn(a.angle,to.angle,k);v.pitch=mix(a.pitch,to.pitch,k);v.eye=mix(a.eye,to.eye,k);
   if(m_lockerView.burst&&t>.07f&&t<.32f){const float u=(t-.07f)/.25f,shake=(1-u)*(1-u);v.angle=wrapAngle(v.angle+.035f*std::sin(u*31)*shake);v.pitch+=5.f*std::sin(u*23+1.3f)*shake;v.eye-=.025f*std::sin(u*kPi)*shake;}
   break;}
  default:{
   // Hauled out: a hard yank by the collar, wrenched round onto the guard's
   // face, the grip shaking the view; it settles exactly on the commit.
   // The eye stays put for the first frames while the leaves swing clear of it.
   const float k=easeOut(std::max(0.f,t-.08f)/.22f),r=easeOut(std::max(0.f,t-.08f)/.3f),shake=(1-t/m_lockerView.length)*(1-t/m_lockerView.length);
   const Vec2 o=out(f),side{-o.y,o.x};
   v.pos=a.pos+(to.pos-a.pos)*k+side*(.03f*std::sin(t*57)*shake)+o*(.02f*std::sin(t*43+1)*shake);
   v.angle=wrapAngle(turn(a.angle,to.angle,r)+.04f*std::sin(t*49+2)*shake);v.pitch=mix(a.pitch,to.pitch,r);
   v.z=mix(a.z,to.z,k);v.eye=mix(a.eye,to.eye,k);break;}
 }
 return v;
}
// LMB shut in: the leaves rattle on their latch, and the knock knockOnWall
// makes goes out from just in front of the doors, so a guard comes to stand
// there. Same 0.6 s cooldown, the same stimulus.
void Game::knockInLocker(){
 const int index=hiddenLocker();if(index<0||lockerViewLocked()||m_elapsed-m_lastKnock<.6f)return;
 const auto&f=m_world.fixtures()[size_t(index)];const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base;
 m_lastKnock=m_elapsed;animateLockerDoor(m_level,index,LockerMotion::Knock);
 m_sounds.push_back({Sound::LockerKnock,front(f,.15f),.7f,1.f,true,12,floor+Player::StandingHeight*.7f});
}
// Where a hand goes on a leaf at its present swing: a fraction of the way
// from the hinge to the free edge, at a height off the floor, a little off its
// inner face (negative: the outer face). The geometry Scene3D hangs the leaves by.
namespace {
Point3 leafGrip(const Fixture& f,float floor,int leaf,float angle,float along,float height,float inward){
 const float sx=f.width/.64f,sz=f.depth/.9f,hand=leaf?-1.f:1.f;const Vec2 o=out(f),side{-o.y,o.x};
 const Vec2 hinge=f.position+o*(.32f*sx)+side*((leaf?.4412f:.432f)*sz*hand),toward=side*(-hand*std::cos(angle))+o*std::sin(angle),inner=o*-std::cos(angle)-side*(hand*std::sin(angle));
 const Vec2 p=hinge+toward*((leaf?.443f:.43f)*sz*along)+inner*inward;return {p.x,p.y,floor+height};
}
Point3 blend(Point3 a,Point3 b,float k){return a+(b-a)*std::clamp(k,0.f,1.f);}
}
// The hands rest below the view between holds, so they never pop in or out:
// in, they come up and take the leaves' edges as the leaves open, drop for the
// about-turn, then take the inner faces and pull them shut; out, both palms
// shove the leaves open and drop as the body follows; stowing, they shove the
// body in by the chest and push the leaves to on him.
bool Game::lockerHands(std::array<Point3,2>* grips)const{
 if(m_vrInputActive||dead())return false;const auto&fixtures=m_world.fixtures();
 const Player v=view();const Vec2 forward{std::cos(v.angle),std::sin(v.angle)},right{-forward.y,forward.x};const float eye=v.z+v.eye;
 auto rest=[&](int hand){const Vec2 p=v.pos+forward*.3f+right*(hand?-.2f:.2f);return Point3{p.x,p.y,eye-.62f};};
 auto door=[&](int fixture)->const LockerDoor*{for(const auto&d:m_lockerDoors)if(d.level==m_level&&d.fixture==fixture)return &d;return nullptr;};
 std::array<Point3,2> at{rest(0),rest(1)};
 if(m_lockerView.active&&(m_lockerView.motion==LockerMotion::Enter||m_lockerView.motion==LockerMotion::Exit)&&m_lockerView.fixture>=0&&size_t(m_lockerView.fixture)<fixtures.size()){
  const auto&f=fixtures[size_t(m_lockerView.fixture)];const LockerDoor*d=door(m_lockerView.fixture);if(!d)return false;
  const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base,t=m_lockerView.age;
  for(int hand=0;hand<2;++hand){
   // Facing in, the right hand is on the leaf hung on the right (leaf 1); facing out, on leaf 0.
   if(m_lockerView.motion==LockerMotion::Enter){const float lead=std::max(d->lead,.05f);
    const float take=ease(t/lead)*(1-ease((t-lead-.25f)/.12f)),pull=ease((t-.55f)/.1f);
    const int outside=hand?0:1,inside=hand?1:0;
    at[size_t(hand)]=blend(blend(at[size_t(hand)],leafGrip(f,floor,outside,d->angle[size_t(outside)],.9f,1.12f,-.03f),take),leafGrip(f,floor,inside,d->angle[size_t(inside)],.85f,1.2f,.04f),pull);
   }else{const int leaf=hand?1:0;at[size_t(hand)]=blend(leafGrip(f,floor,leaf,d->angle[size_t(leaf)],.75f,1.3f,.05f),at[size_t(hand)],ease((t-.16f)/.14f));}
  }
  if(grips)*grips=at;return true;
 }
 // A stow the player stands over: the hands go to the body, then the leaves.
 for(const auto&d:m_lockerDoors){if(d.level!=m_level||d.motion!=LockerMotion::Stow||d.age>=d.length||size_t(d.fixture)>=fixtures.size())continue;
  const auto&f=fixtures[size_t(d.fixture)];const Vec2 o=out(f),mouth=front(f,0);const float ahead=dot(m_player.pos-mouth,o);
  if(ahead<0||ahead>1.4f||std::fabs(dot(m_player.pos-mouth,Vec2{-o.y,o.x}))>.8f||dot(forward,o)>-.3f)continue;
  int body=-1;for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(state(friendlyKey(actor,"guard_stowed"))==d.fixture+1&&state(friendlyKey(actor,"guard_stowed_level"))==m_level+1)body=int(actor);
  const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base,t=d.age;
  const float shove=ease((t-.4f)/.15f)*(1-ease((t-.95f)/.1f)),shut=ease((t-.98f)/.08f)*(1-ease((t-d.length+.04f)/.04f));
  for(int hand=0;hand<2;++hand){Point3 chest=at[size_t(hand)];
   if(body>=0){const auto&pose=m_friendlyActors[size_t(body)].pose;const Vec2 p=pose.position+right*(hand?-.16f:.16f);chest={p.x,p.y,pose.z+.42f};}
   const int leaf=hand?0:1;at[size_t(hand)]=blend(blend(at[size_t(hand)],chest,shove),leafGrip(f,floor,leaf,d.angle[size_t(leaf)],.8f,1.15f,-.04f),shut);}
  if(grips)*grips=at;return true;
 }
 return false;
}
// Who the doors are flung into: a hostile guard on his feet within 1.3 m of
// them and inside the width the leaves sweep, who is not alert on the player
// (a man who has you braces for them, and the doors only open).
int Game::lockerBurstTarget()const{
 const int index=hiddenLocker();if(index<0||lockerViewLocked())return -1;const auto&f=m_world.fixtures()[size_t(index)];
 const Vec2 o=out(f),side{-o.y,o.x},mouth=front(f,0);const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base,reach=f.depth*.5f+.2f;
 int best=-1;float nearest=1.3f;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
  if(npc.level!=m_level||track.ai.mode!=ActorAiMode::Guard||npcIncapacitated(actor)||npcHeld(actor)||npcStowed(actor)||(track.deadState&&state(track.deadState))||guardState(actor)==GuardState::Alert)continue;
  const float ahead=dot(npc.pose.position-mouth,o),across=dot(npc.pose.position-mouth,side);
  if(ahead<.05f||ahead>=nearest||std::fabs(across)>reach||std::fabs(npc.pose.z-floor)>.4f)continue;
  best=int(actor);nearest=ahead;
 }return best;
}
bool Game::stageLockerBurst(){
 const int index=hiddenLocker();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];int chosen=-1;float best=1e9f;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];if(npc.level!=m_level||npc.track().ai.mode!=ActorAiMode::Guard)continue;
  const float d=lengthSq(npc.pose.position-f.position);if(d<best){best=d;chosen=int(actor);}}
 if(chosen<0)return false;auto&npc=m_friendlyActors[size_t(chosen)];const Vec2 toward=out(f)*-1.f;
 npc.pose.position=front(f,.8f);npc.pose.z=m_player.z;npc.pose.yaw=std::atan2(toward.x,toward.y);
 setState(friendlyKey(size_t(chosen),"guard_mode"),int(GuardState::Investigate));return lockerBurstTarget()==chosen;
}
bool Game::stageLockerCatch(){
 const int index=hiddenLocker();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];int chosen=-1;float best=1e9f;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];if(npc.level!=m_level||npc.track().ai.mode!=ActorAiMode::Guard)continue;
  const float d=lengthSq(npc.pose.position-f.position);if(d<best){best=d;chosen=int(actor);}}
 if(chosen<0)return false;auto&npc=m_friendlyActors[size_t(chosen)];npc.pose.position=front(f,1.f);npc.pose.z=m_player.z;
 setState(friendlyKey(size_t(chosen),"guard_locker"),index+1);return pullFromLocker(m_level,size_t(chosen),npc.pose);
}
}

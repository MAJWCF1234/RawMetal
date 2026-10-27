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
float angleAt(Game::LockerMotion m,float t,float from){using M=Game::LockerMotion;
 auto swing=[&](float open,float peak,float hold,float shut){if(t<open)return from+(peak-from)*easeOut(t/open);if(t<hold)return peak;return peak*(1-ease((t-hold)/shut));};
 switch(m){
  case M::Enter:return swing(.25f,WideOpen,.65f,.25f);   // open, the player turns in, pulled shut over them
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
 d.motion=motion;d.age=0;d.length=lengthOf(motion);d.shut=false;
}
float Game::lockerDoorOpen(int level,int fixture,int leaf)const{
 for(const auto&d:m_lockerDoors)if(d.level==level&&d.fixture==fixture)return std::clamp(d.angle[size_t(leaf&1)]/FullSwing,0.f,1.f);
 return 0;
}
// The doors' own sounds after the first: hinges taking the weight as they
// swing back, and the shut, quiet enough that only someone at arm's length
// would notice (the latch, bang and knock go out with the commit itself).
void Game::updateLockerDoors(float dt){
 if(dead())m_lockerView.active=false;
 if(m_lockerView.active&&(m_lockerView.age+=dt)>=m_lockerView.length)m_lockerView.active=false;
 for(auto&d:m_lockerDoors){const float before=d.age;d.age+=dt;const float t=std::min(d.age,d.length);
  for(int leaf=0;leaf<2;++leaf)d.angle[size_t(leaf)]=std::clamp(angleAt(d.motion,t,d.from[size_t(leaf)]),0.f,d.limit[size_t(leaf)]);
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
void Game::clearLockerMotion(bool doors){m_lockerView.active=false;if(doors)m_lockerDoors.clear();}
bool Game::lockerViewLocked()const{return m_lockerView.active&&!dead();}
int Game::lockerViewFixture()const{return m_lockerView.active?m_lockerView.fixture:hiddenLocker();}
float Game::lockerShutView()const{
 if(m_vrInputActive)return 0;
 if(!m_lockerView.active)return hiddenLocker()>=0?1.f:0.f;
 const float t=m_lockerView.age;
 // The perforated inner face takes over the view as the leaves meet in front of the eye.
 if(m_lockerView.motion==LockerMotion::Enter)return ease((t-.7f)/.2f);
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
   // The leaves fly open first; the body follows them out.
   const float k=ease((t-.06f)/.44f);v.pos=a.pos+(to.pos-a.pos)*k;v.z=mix(a.z,to.z,k);
   v.angle=turn(a.angle,to.angle,k);v.pitch=mix(a.pitch,to.pitch,k);v.eye=mix(a.eye,to.eye,k);break;}
  default:{
   // Hauled out: a hard yank by the collar, wrenched round onto the guard's
   // face, the grip shaking the view; it settles exactly on the commit.
   const float k=easeOut(t/.22f),r=easeOut(t/.3f),shake=(1-t/m_lockerView.length)*(1-t/m_lockerView.length);
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
bool Game::stageLockerCatch(){
 const int index=hiddenLocker();if(index<0)return false;const auto&f=m_world.fixtures()[size_t(index)];int chosen=-1;float best=1e9f;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];if(npc.level!=m_level||npc.track().ai.mode!=ActorAiMode::Guard)continue;
  const float d=lengthSq(npc.pose.position-f.position);if(d<best){best=d;chosen=int(actor);}}
 if(chosen<0)return false;auto&npc=m_friendlyActors[size_t(chosen)];npc.pose.position=front(f,1.f);npc.pose.z=m_player.z;
 setState(friendlyKey(size_t(chosen),"guard_locker"),index+1);return pullFromLocker(m_level,size_t(chosen),npc.pose);
}
}

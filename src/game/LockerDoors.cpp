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
// Stepping out, the palms come up from below the view and reach the leaves in
// PalmsReach (six frames, in sight for the last four; four bursting out, a
// shove); the leaves start their swing on that frame.
constexpr float PalmsReach=6/60.f,BurstReach=4/60.f;
constexpr float LockerInset=.2f; // the eye behind the leaves' plane, shut in: far enough back that the vent band, not one hole, fills the view
// The vent band punched through both leaves (the door textures' alpha, seen
// from inside as from out): three staggered rows across each leaf, their middle
// row 57.8% of the way up it. Shut in, the eye sits level with it, so the
// Gatehouse's short pair (1.94 m) has the player stooped to theirs.
constexpr float VentRise=.578237f;
float ventEye(const Fixture& f,float floor){const float sy=f.height/2.2969f;return floor+(.075f+VentRise*2.2f)*sy;}
// Where the body turns about on the way in: where it stood, if that is close,
// otherwise 0.45 m out, so the leaves and the room frame the turn rather than
// the locker's back wall.
float turnAt(float ahead){return std::clamp(ahead,.26f,.45f);}
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
 // The way out over, the gun comes back up with its ordinary raise. The age
 // runs on past the end: a held area title waits on it (Game.cpp).
 const bool viewing=m_lockerView.active;m_lockerView.age=std::min(m_lockerView.age+dt,60.f);
 if(viewing&&m_lockerView.age>=m_lockerView.length){m_lockerView.active=false;if(m_lockerView.motion!=LockerMotion::Enter)lockerRearm();}
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
 // Out (stepped or burst), the leaves wait for the palms to come up onto them.
 if(motion==LockerMotion::Exit)for(auto&d:m_lockerDoors)if(d.level==m_level&&d.fixture==fixture&&d.age<=0)d.lead=d.motion==LockerMotion::Caught?BurstReach:PalmsReach;
}
void Game::clearLockerMotion(bool doors){const bool out=m_lockerView.active&&m_lockerView.motion!=LockerMotion::Enter;m_lockerView.active=false;if(doors)m_lockerDoors.clear();if(out)lockerRearm();}
bool Game::lockerViewLocked()const{return m_lockerView.active&&!dead();}
int Game::lockerViewFixture()const{return m_lockerView.active?m_lockerView.fixture:hiddenLocker();}
// 1: shut in, the leaves met (a knock's rattle is the most they move): the HUD
// is down to what hiding needs and the guards' cards read through the vents
// (drawHud); the way there and back it is how far the HUD has drained, over
// the last fifth of a second in and the first fifth out. The leaves themselves
// are drawn the same either way (Scene3D), so there is nothing to pop.
float Game::lockerShutView()const{
 if(m_vrInputActive)return 0;const float t=m_lockerView.age;
 // (While a way in or out plays, the hands may be on the real leaves: they are what is drawn.)
 float shut=!m_lockerView.active?(hiddenLocker()>=0?1.f:0.f):m_lockerView.motion==LockerMotion::Caught?0.f:std::min(.999f,m_lockerView.motion==LockerMotion::Enter?ease((t-.7f)/.2f):1-ease(t/.2f));
 const int index=lockerViewFixture();
 if(shut>=1&&index>=0&&std::max(lockerDoorOpen(m_level,index,0),lockerDoorOpen(m_level,index,1))>4*Degree/FullSwing)shut=.999f;
 return shut;
}
Player Game::view()const{
 Player v=m_player;if(dead()||m_vrInputActive)return v;
 const auto&fixtures=m_world.fixtures();const int hidden=hiddenLocker();
 auto inside=[&](int index,Player p){const auto&f=fixtures[size_t(index)];p.pos=front(f,-LockerInset);p.eye=ventEye(f,m_world.floorHeight(f.position.x,f.position.y)+f.base)-p.z;return p;};
 Player to=hidden>=0?inside(hidden,v):v; // the committed end of any path
 // A knock from inside: the head gives a little with the rattle of the leaves.
 if(!m_lockerView.active&&hidden>=0)for(const auto&d:m_lockerDoors)if(d.level==m_level&&d.fixture==hidden&&d.motion==LockerMotion::Knock&&d.age<d.length){
  const float u=d.age/d.length,shake=(1-u)*(1-u);to.pitch+=.3f*std::sin(u*kPi*5)*shake;to.angle=wrapAngle(to.angle+.0016f*std::sin(u*kPi*7+1)*shake);}
 // A stow the player stands over (the stance lockerHands takes for it): the
 // eyes drop 30 degrees over the first 150 ms onto the body coming round to
 // the mouth, then ride his collar as the hands lift him in (the view's middle
 // a little over it, so the hands on it and the slump above them are in
 // frame), and come level again as the leaves swing to on him.
 if(!m_lockerView.active&&hidden<0)for(const auto&d:m_lockerDoors){if(d.level!=m_level||d.motion!=LockerMotion::Stow||d.age>=d.length||size_t(d.fixture)>=fixtures.size())continue;
  const auto&f=fixtures[size_t(d.fixture)];const Vec2 o=out(f),mouth=front(f,0),forward{std::cos(m_player.angle),std::sin(m_player.angle)};const float ahead=dot(m_player.pos-mouth,o);
  if(ahead<0||ahead>1.4f||std::fabs(dot(m_player.pos-mouth,Vec2{-o.y,o.x}))>.8f||dot(forward,o)>-.3f)continue;
  constexpr float Unit=140*Degree;float target=-30*Unit;
  for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(state(friendlyKey(actor,"guard_stowed"))==d.fixture+1&&state(friendlyKey(actor,"guard_stowed_level"))==m_level+1){
   const Point3 c=npcCollar(actor);const float run=std::max(.15f,dot(Vec2{c.x,c.y}-m_player.pos,forward)),ride=std::clamp(std::atan2(c.z-(m_player.z+m_player.eye),run)*140.f+8*Unit,-62*Unit,-8*Unit);
   target+=(ride-target)*ease((d.age-.45f)/.15f);}
  const float w=ease(d.age/.15f)*(1-ease((d.age-1.05f)/.3f));to.pitch+=(target-to.pitch)*w;break;}
 if(!m_lockerView.active||m_lockerView.fixture<0||size_t(m_lockerView.fixture)>=fixtures.size())return to;
 const auto&f=fixtures[size_t(m_lockerView.fixture)];const auto&a=m_lockerView.from;const float t=m_lockerView.age;v=to;
 auto mix=[](float x,float y,float k){return x+(y-x)*k;};
 auto turn=[](float from,float target,float k){return wrapAngle(from+wrapAngle(target-from)*k);};
 switch(m_lockerView.motion){
  case LockerMotion::Enter:{
   // A step to the open doors, the about-turn short of their mouth, then
   // backing in as the leaves come to; the moves overlap so the body never
   // stops dead, and the turn is half done before the locker's inside could
   // fill the view.
   const Vec2 mouth=front(f,turnAt(dot(a.pos-front(f,0),out(f)))),toMouth=a.pos+(mouth-a.pos)*ease(t/.4f);
   v.pos=toMouth+(to.pos-toMouth)*ease((t-.35f)/.45f);
   v.angle=turn(a.angle,to.angle,ease((t-.14f)/.45f));v.pitch=mix(a.pitch,to.pitch,ease(t/.5f));
   v.z=mix(a.z,to.z,ease(t/.3f));v.eye=mix(a.eye,to.eye,ease(t/.3f));break;}
  case LockerMotion::Exit:{
   // The palms come up onto the leaves, the leaves fly open, the body follows
   // them out. Burst into a man at the doors, the blow comes back up the
   // arms: a short hard jolt.
   const float reach=m_lockerView.burst?BurstReach:PalmsReach,k=ease((t-reach-.06f)/(.44f-reach));v.pos=a.pos+(to.pos-a.pos)*k;v.z=mix(a.z,to.z,k);
   v.angle=turn(a.angle,to.angle,k);v.pitch=mix(a.pitch,to.pitch,k);v.eye=mix(a.eye,to.eye,k);
   if(m_lockerView.burst&&t>.07f+reach&&t<.32f+reach){const float u=(t-.07f-reach)/.25f,shake=(1-u)*(1-u);v.angle=wrapAngle(v.angle+.035f*std::sin(u*31)*shake);v.pitch+=5.f*std::sin(u*23+1.3f)*shake;v.eye-=.025f*std::sin(u*kPi)*shake;}
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
// come up onto the leaves (the leaves wait for them), shove them open and drop
// as the body follows; stowing, they shove the body in by the chest and push
// the leaves to on him.
bool Game::lockerHands(std::array<Point3,2>* grips)const{
 if(m_vrInputActive||dead())return false;const auto&fixtures=m_world.fixtures();
 const Player v=view();const Vec2 forward{std::cos(v.angle),std::sin(v.angle)},right{-forward.y,forward.x};const float eye=v.z+v.eye;
 auto rest=[&](int hand){const Vec2 p=v.pos+forward*.3f+right*(hand?-.2f:.2f);return Point3{p.x,p.y,eye-.62f};};
 auto door=[&](int fixture)->const LockerDoor*{for(const auto&d:m_lockerDoors)if(d.level==m_level&&d.fixture==fixture)return &d;return nullptr;};
 std::array<Point3,2> at{rest(0),rest(1)};
 if(m_lockerView.active&&(m_lockerView.motion==LockerMotion::Enter||m_lockerView.motion==LockerMotion::Exit)&&m_lockerView.fixture>=0&&size_t(m_lockerView.fixture)<fixtures.size()){
  const auto&f=fixtures[size_t(m_lockerView.fixture)];const LockerDoor*d=door(m_lockerView.fixture);if(!d)return false;
  const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base,t=m_lockerView.age,tall=f.height/2.42f; // grips scale with the locker, as the eye in it does
  for(int hand=0;hand<2;++hand){
   // Facing in, the right hand is on the leaf hung on the right (leaf 1); facing out, on leaf 0.
   if(m_lockerView.motion==LockerMotion::Enter){const float lead=std::max(d->lead,.05f);
    // They let go for the last twenty degrees and drop away: the leaves meet
    // on their own swing, with no hand left in front of the perforated face.
    const float take=ease(t/lead)*(1-ease((t-lead-.25f)/.12f)),pull=ease((t-.55f)/.1f)*(1-ease((t-.82f)/.07f));
    const int outside=hand?0:1,inside=hand?1:0;
    at[size_t(hand)]=blend(blend(at[size_t(hand)],leafGrip(f,floor,outside,d->angle[size_t(outside)],.9f,1.12f*tall,-.03f),take),leafGrip(f,floor,inside,d->angle[size_t(inside)],.85f,1.2f*tall,.04f),pull);
   }else{ // out: straight up onto the leaves over the lead from a hand's length under the frame (fast, settling on them), the shove, then down to rest
    const int leaf=hand?1:0;const float lead=d->lead;const Point3 grip=leafGrip(f,floor,leaf,d->angle[size_t(leaf)],.75f,1.3f*tall,.05f);
    at[size_t(hand)]=blend(blend(grip-Point3{0,0,.22f},grip,lead>0?easeOut(t/lead):1.f),at[size_t(hand)],ease((t-lead-.16f)/.14f));}
  }
  if(grips)*grips=at;return true;
 }
 // A stow the player stands over: the hands go to the body, then the leaves.
 for(const auto&d:m_lockerDoors){if(d.level!=m_level||d.motion!=LockerMotion::Stow||d.age>=d.length||size_t(d.fixture)>=fixtures.size())continue;
  const auto&f=fixtures[size_t(d.fixture)];const Vec2 o=out(f),mouth=front(f,0);const float ahead=dot(m_player.pos-mouth,o);
  if(ahead<0||ahead>1.4f||std::fabs(dot(m_player.pos-mouth,Vec2{-o.y,o.x}))>.8f||dot(forward,o)>-.3f)continue;
  int body=-1;for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(state(friendlyKey(actor,"guard_stowed"))==d.fixture+1&&state(friendlyKey(actor,"guard_stowed_level"))==m_level+1)body=int(actor);
  const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base,t=d.age;
  // By the collar from the moment he reaches the mouth: the grips ride his
  // collar as it comes up off the floor (his fall clip run back: phase 1
  // lying, 0 standing), so the hands lift him in rather than meet him there.
  // The push to starts low on the leaves, the forearms coming up from below.
  const float shove=ease((t-.5f)/.1f)*(1-ease((t-1.f)/.08f)),shut=ease((t-.98f)/.08f)*(1-ease((t-d.length+.04f)/.04f));
  for(int hand=0;hand<2;++hand){Point3 chest=at[size_t(hand)];
   if(body>=0){const auto&npc=m_friendlyActors[size_t(body)];const auto&pose=npc.pose;const float rise=std::clamp(pose.phase,0.f,1.f);
    const Vec2 facing{std::sin(pose.yaw),std::cos(pose.yaw)},p=pose.position+facing*(.1f-.25f*rise)+right*(hand?-.13f:.13f);chest={p.x,p.y,pose.z+npc.track().scale*(.84f-.6f*rise)};}
   const int leaf=hand?0:1;const float up=.9f+.22f*ease((t-1.06f)/.26f); // low on the leaves: the forearms come up into the foot of the view, not in from its sides
   at[size_t(hand)]=blend(blend(at[size_t(hand)],chest,shove),leafGrip(f,floor,leaf,d.angle[size_t(leaf)],.88f,up,-.04f),shut);}
  if(grips)*grips=at;return true;
 }
 return false;
}
// Who the doors are flung into: a hostile guard on his feet within 1.3 m of
// them and inside the width the leaves sweep, who is not alert on the player
// (a man who has you braces for them, and the doors only open).
int Game::lockerBurstTarget()const{
 const int index=hiddenLocker();if(index<0||lockerViewLocked())return -1;const auto&f=m_world.fixtures()[size_t(index)];
 const Vec2 o=out(f),side{-o.y,o.x},mouth=front(f,0);const float floor=m_world.floorHeight(f.position.x,f.position.y)+f.base,reach=std::min(f.depth*.5f+.2f,.45f);
 int best=-1;float nearest=.72f; // within the leaves' sweep, not a step beyond it
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
 npc.pose.position=front(f,.6f);npc.pose.z=m_player.z;npc.pose.yaw=std::atan2(toward.x,toward.y);
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

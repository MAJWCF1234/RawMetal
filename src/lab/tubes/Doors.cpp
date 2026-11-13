// A swinging door (World doors, DoorPlay.cpp): it eases off the latch, knocks
// its stop and settles, shuts onto the latch; locked, it holds a hand; given a
// strength, a creature that wants through beats it down. Friend: the Chimera,
// set on a man behind the locked door: door breaks -> it routes through -> kill.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
const StateId DoorKey=stateId("lab_door_key");
// A creature in the tube has to come through: the door stays locked and no hand moves it.
bool creature(const Scene& s){return s.has("chimera")||s.has("brute");}
const bool doors=add({
 .name="doors",.summary="a swinging door: weight off the latch, its stop, the latch, its lock and a creature beating it down (World doors, DoorPlay)",
 .friends={"chimera"},.fixtures={"door-hunt"},
 .author=[](Scene& s,AuthoredMapData& m){
  if(m.doors.empty())m.doors.push_back({7,10,8});
  auto& d=m.doors[0];d.swinging=true;d.strength=tuned("doors.strength",s.fixture->number("door-strength",120));d.maxOpen=1;
  // Locked unless the tube opens it itself: the key state stays 0 when a creature has to come through.
  d.requireState=DoorKey;d.requireValue=1;d.lock=1;},
 .setup=[](Scene& s){if(!creature(s))s.g().setState(DoorKey,1);
  // Is there a man on one side of the door and a creature on the other? Only then must it be beaten down.
  const World& w=s.g().world();if(w.doors().empty()||s.g().npcActors().empty())return;const float y=w.doors()[0].y;const float man=s.g().npcActors()[0].pose.position.y-y;
  for(const auto& e:s.g().enemies())if(man*(e.pos.y-y)<0)s.board["doors.split"]=1;},
 .step=[](Scene& s,float){
  if(creature(s)||s.map==nullptr)return;World& w=LabAccess::world(s.g());if(w.doors().empty())return;
  // Alone: the hand swings it open, then shuts it, as the doors test does.
  const int open=int(s.fixture->number("open-frame",5)),shut=s.frames/2;
  if(s.frame==open)w.setDoor(0,0,true);
  if(s.frame==shut)w.toggleDoor(0);},
 .capture=[](Scene& s,const Frame&){
  const World& w=s.g().world();if(w.doors().empty())return;const auto& d=w.doors()[0];
  const int open=int(s.fixture->number("open-frame",5));
  if(s.frame==open+std::max(1,int(std::lround(.1f/s.dt))))s.board["doors.at01"]=d.open;   // 0.1 s after the hand
  s.board["doors.peak"]=std::max(s.board["doors.peak"],double(d.open));
  if(s.frame==s.frames/2-1)s.board["doors.settled"]=d.open;
  if(d.broken&&!s.board.count("doors.broke_s"))s.board["doors.broke_s"]=s.time;
  // Who has crossed the door line (the wall at y = d.y along X).
  for(const auto& e:s.g().enemies())if(e.alive&&e.pos.y>d.y+.4f&&!s.board.count("doors.crossed_s"))s.board["doors.crossed_s"]=s.time;},
 .metrics=[](Scene& s,Report& r){
  const World& w=s.g().world();r.check("the tube has a door",!w.doors().empty());if(w.doors().empty())return;const auto& d=w.doors()[0];
  r.metric("open_end",d.open);r.metric("damage",d.damage);r.metric("broken",d.broken);
  if(creature(s)){
   r.metric("broke_s",s.board.count("doors.broke_s")?s.board["doors.broke_s"]:-1,"s");
   if(s.board.count("doors.split"))r.check("locked, it holds until it is beaten down",d.broken||d.damage>0,"damage "+fixed(d.damage,0)+" of "+fixed(d.strength,0));
   return;}
  const float at01=float(s.board["doors.at01"]),peak=float(s.board["doors.peak"]),settled=float(s.board["doors.settled"]);
  r.metric("open_at_0.1s",at01);r.metric("peak",peak);r.metric("settled",settled);
  r.check("it eases off the latch",at01>0&&at01<.06f,fixed(at01)+" open 0.1 s after the hand");
  r.check("it knocks its stop and settles on it",std::fabs(settled-d.maxOpen)<.01f,"settled at "+fixed(settled));
  r.check("shut by the hand it latches",d.open<.001f,"open "+fixed(d.open)+" at the end");},
 .reporter=[](Scene& s,Overlay& o){
  const World& w=s.g().world();for(const auto& d:w.doors()){if(!d.swinging)continue;const Vec2 h=d.hinge();const float wid=d.width(),z=d.z+.05f;
   const std::uint32_t col=d.broken?0xff3030:d.requireState&&!s.g().state(d.requireState)?0xffb020:0x40e0ff;
   o.point({h.x,h.y,z},col,4);Vec2 last=h+d.leafAxis(0)*wid;
   for(int i=1;i<=12;++i){const Vec2 p=h+d.leafAxis(d.maxOpen*float(i)/12)*wid;o.line({last.x,last.y,z},{p.x,p.y,z},0x606060);last=p;}
   const Vec2 leaf=h+d.leafAxis(d.open)*wid;o.line({h.x,h.y,z},{leaf.x,leaf.y,z},col);o.line({h.x,h.y,z+2},{leaf.x,leaf.y,z+2},col);o.line({leaf.x,leaf.y,z},{leaf.x,leaf.y,z+2},col);
   const Vec2 n=d.centre()+d.normal()*.6f;o.line({d.centre().x,d.centre().y,z},{n.x,n.y,z},0x8080ff);
   o.label({leaf.x,leaf.y,z+2.1f},std::string(d.broken?"BROKEN":d.requireState&&!s.g().state(d.requireState)?"LOCKED":"FREE")+" OPEN "+fixed(d.open,2)+" DMG "+fixed(d.damage,0)+"/"+fixed(d.strength,0),col);}},
 .with={{"chimera",[](Scene& s,Report& r){
  // The handoff: the door gives -> the Chimera routes through it -> the man behind it dies.
  if(!s.board.count("doors.split")){r.metric("split",0);return;} // the man and the beast share a side here: nothing to hand off
  const bool broke=s.board.count("doors.broke_s")>0,crossed=s.board.count("doors.crossed_s")>0;
  const bool dead=!s.g().npcActors().empty()&&s.g().npcDeath(0).dead;
  r.metric("crossed_s",crossed?s.board["doors.crossed_s"]:-1,"s");
  r.check("the door gives way to the Chimera",broke,broke?"broken at "+fixed(float(s.board["doors.broke_s"]),2)+" s":"held");
  r.check("it comes through the broken door",crossed&&(!broke||s.board["doors.crossed_s"]>=s.board["doors.broke_s"]),crossed?"crossed at "+fixed(float(s.board["doors.crossed_s"]),2)+" s":"never crossed");
  r.check("and reaches the man behind it",dead);}}}});
}
}

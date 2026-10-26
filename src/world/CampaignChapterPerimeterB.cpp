// THE RESTRICTED PERIMETER, inner half (chapter 1, manifest 41-44): chunks 63-67.
// Restricted Utilities, Research Receiving, Decontamination and the Energy
// Research Entrance (forecourt and great door), the chapter 1 finale. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- PerimeterB kit (pb) ---------------------------------------------------------------
// A glazed screen: painted dado, glass between mullions, painted header.
void pbGlazed(Map& m,float x1,float y1,float x2,float y2,float f,float top=2.6f){
 wall(m,x1,y1,x2,y2,f,f+.95f,3);wall(m,x1,y1,x2,y2,f+.95f,f+2.3f,17);wall(m,x1,y1,x2,y2,f+2.3f,f+top,3);
 const bool alongX=x2-x1>y2-y1;const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.5f+.5f));
 for(int i=1;i<bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,t-.03f,y1-.01f,t+.03f,y2+.01f,f+.95f,f+2.3f,20);else wall(m,x1-.01f,t-.03f,x2+.01f,t+.03f,f+.95f,f+2.3f,20);}
}
// A body left where it fell: a scripted track held in the dead pose.
void pbBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw,float scale=1.7f){
 actor(m,clock,visual,scale,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});
}
// Blood on a floor: thin strips that cast no shadow.
void pbBlood(Map& m,float x1,float y1,float x2,float y2,float f){wall(m,x1,y1,x2,y2,f+.004f,f+.010f,16);}
ScriptEvent pbEvent(const char* id,float x1,float y1,float x2,float y2,float bottom,float top,StateId require=0){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=bottom;e.top=top;e.requireState=require;return e;
}
ScriptAction pbSet(StateId id,int value=1){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=id;a.value=value;return a;}
ScriptAction pbSound(Sound s,float gain){ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=s;a.amount=gain;return a;}
ScriptAction pbShake(float amount){ScriptAction a;a.type=ScriptAction::Type::Shake;a.amount=amount;return a;}
ScriptAction pbSave(){ScriptAction a;a.type=ScriptAction::Type::Checkpoint;return a;}
ScriptAction pbSpawn(CreatureKind kind,Vec2 p,float z,float awareness){ScriptAction a;a.type=ScriptAction::Type::SpawnEnemy;a.enemyKind=kind;a.position=p;a.z=z;a.amount=awareness;return a;}
// A sequence started only by setting its timer (its trigger box is off the map).
void pbSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=-2;s.y1=-2;s.x2=-1;s.y2=-1;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
// A lamp switched by a state: lit while the state is set (or, inverted, until it is).
WorldLight pbSwitched(WorldLight l,StateId state,bool invert=false,float flicker=0){l.enabledState=state;l.invert=invert;l.flicker=flicker;return l;}
// A sign plate hung from the roof on two rods, lettered on both faces. alongY:
// the plate runs along y and faces east (first text) and west (second).
void pbHangingSign(Map& m,float x,float y,float z,bool alongY,float width,const char* title,const char* sub,const char* backTitle,const char* backSub){
 const float roof=m.definition.ceiling,h=.65f;
 if(alongY){wall(m,x-.15f,y-width*.5f,x+.15f,y+width*.5f,z-.02f,z+h+.02f,2);
  for(float s:{-1.f,1.f})m.pipes.push_back({{x,y+s*width*.4f},{x,y+s*width*.4f},z+h,.022f,roof,6});
  m.signs.push_back({{x+.162f,y},z,width,h,kPi*.5f,title,sub});m.signs.push_back({{x-.162f,y},z,width,h,-kPi*.5f,backTitle,backSub});}
 else{wall(m,x-width*.5f,y-.15f,x+width*.5f,y+.15f,z-.02f,z+h+.02f,2);
  for(float s:{-1.f,1.f})m.pipes.push_back({{x+s*width*.4f,y},{x+s*width*.4f,y},z+h,.022f,roof,6});
  m.signs.push_back({{x,y+.162f},z,width,h,0,title,sub});m.signs.push_back({{x,y-.162f},z,width,h,kPi,backTitle,backSub});}
}
// A small steel stand: four legs and a plate, for instruments off the floor.
void pbStand(Map& m,float x1,float y1,float x2,float y2,float f,float h){
 for(float x:{x1,x2-.06f})for(float y:{y1,y2-.06f})wall(m,x,y,x+.06f,y+.06f,f,f+h-.04f,12);
 wall(m,x1,y1,x2,y2,f+h-.04f,f+h,2);
 for(float y:{y1,y2-.04f})wall(m,x1,y,x2,y+.04f,f+.25f,f+.3f,12);
}
// Sign accents: research blue for wayfinding, lockdown red, plant amber, clean green.
constexpr std::uint32_t pbBlue=0xff3f86c8u,pbRed=0xffc8392du,pbAmber=0xffca994du,pbGreen=0xff3fb36au;
void pbSign(Map& m,float x,float y,float z,const char* title,const char* sub,float width,float yaw,std::uint32_t accent){
 wallSign(m,x,y,z,title,sub,width,yaw);m.signs.back().accent=accent;
}
// A fixture at an absolute base height. Facing: models whose front is local +y
// (office, medical, power) face (sin yaw, cos yaw); lab and mine models whose
// front is local -y face the opposite way, so add kPi for them.
void pbFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
constexpr float pbS=0,pbN=kPi,pbE=kPi*.5f,pbW=-kPi*.5f;
// A steel locker bank (doors on the model's +x) turned to face `front`.
void pbLockers(Map& m,float x,float y,float z,float front,float scale=1){
 pbFix(m,model::pwr_locker_bank,x,y,z,.87f*scale,.85f*scale,2.42f*scale,front-kPi*.5f,true);
}
// The thin-walled freight layer for a deck at z (deck() may reallocate layers).
AuthoredLayerData& pbDeckLayer(Map& m,float z){for(auto& l:m.layers)if(l.thickness>0&&std::fabs(l.elevation-z)<.01f)return l;return m.layers.back();}
// A 20 ft freight container lying along x (6 m by 3.02 m) with its doors at
// the east or west end: corrugated skins, corner posts, end frames, roof and
// floor plate. Closed doors carry their locking rods; an open mouth is left
// clear for the caller to dress.
void pbContainerX(Map& m,float x,float y,float f,bool mouthEast,bool open,int mat=11){
 const float x2=x+6.f,y2=y+3.02f,h=2.8f;
 wall(m,x,y,x2,y+.12f,f,f+h,mat);wall(m,x,y2-.12f,x2,y2,f,f+h,mat);
 const float back=mouthEast?x:x2-.15f;wall(m,back,y,back+.15f,y2,f,f+h,mat);
 wall(m,x,y,x2,y2,f+h-.1f,f+h,mat);
 wall(m,x+.12f,y+.12f,x2-.12f,y2-.12f,f,f+.08f,8);
 for(float xx=x+.3f;xx<x2-.2f;xx+=.65f)for(float yy:{y-.03f,y2-.03f})wall(m,xx,yy,xx+.08f,yy+.06f,f+.14f,f+h-.14f,2);
 for(float xx:{x,x2-.14f}){wall(m,xx,y-.05f,xx+.14f,y2+.05f,f+h-.16f,f+h+.02f,12);wall(m,xx,y-.05f,xx+.14f,y2+.05f,f,f+.14f,12);
  for(float yy:{y-.05f,y2-.09f})wall(m,xx,yy,xx+.14f,yy+.14f,f,f+h,12);}
 if(!open){const float face=mouthEast?x2-.1f:x,out=mouthEast?x2+.035f:x-.035f;
  wall(m,face,y+.12f,face+.1f,y2-.12f,f+.08f,f+h-.1f,mat);
  wall(m,mouthEast?x2:x-.012f,(y+y2)*.5f-.012f,mouthEast?x2+.012f:x,(y+y2)*.5f+.012f,f+.14f,f+h-.16f,8);
  for(float yy:{y+.45f,y+1.15f,y2-1.15f,y2-.45f}){m.pipes.push_back({{out,yy},{out,yy},f+.12f,.022f,f+h-.18f,6});
   for(float z:{f+.9f,f+1.9f})wall(m,mouthEast?x2:out-.03f,yy-.05f,mouthEast?out+.03f:x,yy+.05f,z,z+.06f,8);}}
}
// A lamp head on a short bracket: the housing a Bare light needs.
void pbLampHead(Map& m,float x,float y,float z,int mat=20){wall(m,x-.09f,y-.09f,x+.09f,y+.09f,z+.03f,z+.13f,mat);}
// An open packing crate: plank sides, no lid, packing straw inside.
void pbOpenCrate(Map& m,float x,float y,float f,float s=1.1f){
 const float h=s*.8f,t=.05f,a=s*.5f;
 wall(m,x-a,y-a,x+a,y-a+t,f,f+h,10);wall(m,x-a,y+a-t,x+a,y+a,f,f+h,10);
 wall(m,x-a,y-a,x-a+t,y+a,f,f+h,10);wall(m,x+a-t,y-a,x+a,y+a,f,f+h,10);
 wall(m,x-a+t,y-a+t,x+a-t,y+a-t,f,f+h*.55f,18);
 for(float z:{f+.12f,f+h-.14f})for(float yy:{y-a-.02f,y+a})wall(m,x-a,yy,x+a,yy+.02f,z,z+.08f,10);
}
// Clean-side glazing: panel dado, pale steel sill, glass with mullions every
// 1.2 m, transom, panel bulkhead up to the absolute height `top`.
void pbLabGlass(Map& m,float x1,float y1,float x2,float y2,float f,float top,int mat=28,float sill=.95f,float head=2.4f){
 wall(m,x1,y1,x2,y2,f,f+sill,mat);
 wall(m,x1-.015f,y1-.015f,x2+.015f,y2+.015f,f+sill-.03f,f+sill+.03f,20);
 const bool alongX=x2-x1>y2-y1;const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)wall(m,x1,cy-.012f,x2,cy+.012f,f+sill+.03f,f+head,17);else wall(m,cx-.012f,y1,cx+.012f,y2,f+sill+.03f,f+head,17);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,std::min(t,x2-.05f),y1,std::min(t+.05f,x2),y2,f+sill+.03f,f+head,20);
  else wall(m,x1,std::min(t,y2-.05f),x2,std::min(t+.05f,y2),f+sill+.03f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.07f,20);
 if(top>f+head+.07f)wall(m,x1,y1,x2,y2,f+head+.07f,top,mat);
}
// A seated workstation whose worker looks along `face` (pbS/pbN/pbE/pbW).
void pbDesk(Map& m,float x,float y,float f,float face,int variant){
 Vec2 p{x,y},d{std::sin(face),std::cos(face)},s{std::cos(face),-std::sin(face)};
 auto at=[&](float u,float v){return p+s*u+d*v;};
 const float b=f-m.layers.front().elevation;
 m.fixtures.push_back({model::desk,p,b,1.4f,.7f,.75f,face,true});
 m.fixtures.push_back({model::office_chair,at(variant%2?.15f:-.1f,-.78f),b,.55f,.55f,.95f,face+kPi+(variant%3-1)*.25f,true});
 m.fixtures.push_back({model::crt_monitor,at(-.1f,.12f),b+.75f,.42f,.4f,.38f,face+kPi,false});
 m.fixtures.push_back({model::keyboard,at(-.1f,-.2f),b+.75f,.45f,.16f,.03f,face+kPi,false});
 m.fixtures.push_back({model::desk_phone,at(.48f,.05f),b+.75f,.2f,.2f,.1f,face+kPi,false});
 if(variant%2)m.fixtures.push_back({model::mug,at(.42f,-.2f),b+.75f,.09f,.09f,.1f,0,false});
 if(variant%3==0)m.fixtures.push_back({model::binder,at(-.52f,.18f),b+.75f,.3f,.08f,.32f,face+kPi,false});
}
// An armed Cleanup man on an authored beat. A role other than Default needs a
// squad; enable keeps him on his mark (idle) until that state is set.
void pbGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,
             float sight,float dot,int weapon,float speed,StateId enable=0,StateId alert=0,bool suppress=false,int grenades=0,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=dot;ai.searchMs=7000;ai.speed=speed;ai.enableState=enable;ai.alertState=alert;
 ai.suppress=suppress;ai.grenades=grenades;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
ScriptAction pbDoor(ScriptAction::Type type,int index){ScriptAction a;a.type=type;a.index=index;return a;}
// A seam header: closes the step between this chunk's roof and a passage of
// `height` across an edge, whatever the neighbour's roof.
void pbSeamHeader(Map& m,char side,float a,float b,float f,float height,int mat=3){
 const float roof=m.definition.ceiling;if(roof<=f+height+.01f)return;
 if(side=='N')wall(m,a,0,b,.2f,f+height,roof,mat);
 if(side=='S')wall(m,a,23.8f,b,24,f+height,roof,mat);
}

// RESTRICTED UTILITIES (manifest 41). The labs breathe through here: the
// laboratory supply handler AHU-1 under its fan deck, the containment exhaust
// scrubbers and their trap cartridges, the chilled water plant and, up on the
// deck, the cascade control room that holds every lab below atmosphere.
// Security Order 7 stopped the supply at 02:20 with 41 badges still inside.
// The exhaust runs on, the labs sit at -310 Pa and pressure lock PL-3 to
// Research Receiving cannot open against the load. The player cranks the
// locked-out outside-air damper open by hand at the west end of the fan deck
// and restarts supply from the control room: the cascade equalizes, the lock
// releases, and whatever was nesting in the riser comes out with the air.
std::shared_ptr<const Map> makeRestrictedUtilities(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[63];m.definition=CampaignChunks[63];m.definition.ambient=.3f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float base=-19,fd=-16.2f,under=fd-.22f,roof=m.definition.ceiling;
 const StateId damper=stateId("ere_util_damper"),supply=stateId("ere_util_supply");
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 openEdge(m,{'E',14,19,base,false,0});openEdge(m,{'S',8,12,base,false,0});
 machineryClock(m,"ere_util_clock",60000);
 // --- Arrival from Personnel Transit: a portal frame and the plant's name ----------------
 wall(m,22.6f,14.f,23.f,19.f,base+3.4f,roof,15);
 for(float y:{13.75f,19.f})wall(m,22.5f,y,23.f,y+.25f,base,roof,15);
 wall(m,22.45f,13.7f,22.6f,19.3f,base+3.25f,base+3.4f,12);
 floorLine(m,13.f,16.42f,23.f,16.58f,base);floorLine(m,9.92f,16.42f,13.f,16.58f,base);floorLine(m,9.92f,16.42f,10.08f,18.9f,base);
 // --- Fan deck over the supply handler, the walkway and the control room -----------------
 deck(m,fd,1,1,15,7);deck(m,fd,15,5,17,7);deck(m,fd,17,1,23,9);
 m.stairs.push_back({1.f,7.f,2.3f,11.2f,base,fd,15,true,false});
 m.stairs.push_back({18.2f,9.f,19.6f,13.2f,base,fd,15,true,false});
 for(float x:{4.5f,9.5f,14.4f,16.f})steelColumn(m,x,6.86f,base,under);
 // --- AHU-1, laboratory supply: mixing box, filters, coil, fans, discharge plenum -------
 wall(m,1.f,1.2f,14.7f,5.35f,base,base+.15f,15);
 wall(m,1.f,1.3f,14.6f,5.2f,base+.15f,under-.02f,20);
 for(float x:{3.f,4.6f,6.6f,9.2f,10.8f,13.4f})wall(m,x-.05f,5.2f,x+.05f,5.27f,base+.15f,under-.02f,12);
 wall(m,1.f,5.2f,14.6f,5.26f,under-.22f,under-.02f,12);
 for(auto [x1,x2]:{std::pair{1.3f,2.7f},{4.75f,6.45f},{6.8f,7.85f},{7.95f,9.f},{11.f,13.2f}}){
  wall(m,x1,5.2f,x2,5.235f,base+.3f,base+1.75f,2);
  wall(m,x2-.3f,5.235f,x2-.18f,5.28f,base+.95f,base+1.15f,8);
  for(float z:{base+.45f,base+1.5f})wall(m,x1,5.2f,x1+.06f,5.25f,z,z+.12f,8);
 }
 wall(m,9.45f,5.2f,10.55f,5.215f,base+1.2f,base+1.65f,9);
 wall(m,9.3f,5.2f,10.7f,5.25f,base+.22f,base+.34f,8);
 m.pipes.push_back({{10.f,5.32f},{10.f,5.32f},base+.02f,.04f,base+.28f,0});
 wall(m,9.75f,5.3f,10.25f,5.8f,base,base+.012f,8);
 for(float x:{4.6f,6.6f,9.2f})m.fixtures.push_back({model::lab_pressure_gauge,{x,5.33f},1.95f,.29f,.11f,.41f,kPi,false});
 // Discharge plenum into the riser, and the supply main to the labs along the roof.
 wall(m,14.6f,1.6f,15.f,3.8f,base+.3f,base+2.4f,20);
 wall(m,15.f,1.3f,16.6f,4.f,base,roof,20);
 for(float z=base+.8f;z<roof-.3f;z+=1.6f)wall(m,14.94f,1.24f,16.66f,4.06f,z,z+.08f,12);
 wall(m,15.f,4.f,16.6f,23.f,roof-1.1f,roof-.35f,20);
 for(float y=5.f;y<23.f;y+=2.f)wall(m,14.97f,y,16.63f,y+.06f,roof-1.13f,roof-.32f,12);
 for(float y=6.f;y<22.5f;y+=4.f){wall(m,14.85f,y-.04f,16.75f,y+.04f,roof-1.18f,roof-1.1f,12);
  for(float x:{14.9f,16.7f})m.pipes.push_back({{x,y},{x,y},roof-1.15f,.022f,roof,6});}
 // --- On the fan deck: the outside-air duct and its damper, motors, the coil header -----
 wall(m,1.2f,1.2f,3.f,3.2f,fd,roof,20);
 for(float z=fd+.4f;z<roof-.3f;z+=1.4f)wall(m,1.14f,1.14f,3.06f,3.26f,z,z+.07f,12);
 wall(m,1.1f,1.1f,3.1f,3.3f,fd+.9f,fd+1.6f,8);
 m.fixtures.push_back({model::valve_wheel,{2.1f,3.52f},fd-base+.85f,.7f,.12f,.7f,0,false});
 m.pipes.push_back({{2.1f,3.3f},{2.1f,3.47f},fd+1.2f,.04f,fd+1.2f,6});
 terminal(m,{3.6f,3.7f},"DAMPER OA-2 / HAND CRANK","ACTUATOR POWER REMOVED BY SECURITY. WHEEL ENGAGED.","E / CRANK OA-2 OPEN. THEN RESTART SF-2, EAST.",fd-base,damper);
 m.terminals.back().control=true;m.terminals.back().yaw=0;
 for(float x:{11.6f,12.9f}){m.fixtures.push_back({model::generator,{x,2.4f},fd-base,1.1f,.7f,.9f,kPi*.5f,true});
  wall(m,x-.22f,2.95f,x+.22f,4.4f,fd,fd+.55f,18);wall(m,x-.35f,4.4f,x+.35f,4.9f,fd,fd+.12f,15);}
 wall(m,9.6f,2.2f,10.8f,3.8f,fd,fd+1.f,20);wall(m,9.55f,2.15f,10.85f,3.85f,fd+.94f,fd+1.f,12);
 // Chilled water flow and return from the pump set below, up and over the deck to the coil.
 for(float o:{0.f,.45f}){const float zz=-13.6f+o*.6f,run=14.f+o*.8f;
  pipeRoute(m,{{10.f+o,3.f,fd+1.f},{10.f+o,3.f,zz},{run,3.f,zz},{run,22.45f,zz},{14.85f+o,22.45f,zz},{14.85f+o,22.45f,base+1.49f}},.1f,o<.2f?5:3,roof);}
 for(float x=5.f;x<9.f;x+=1.4f){wall(m,x,4.6f,x+1.f,5.6f,fd,fd+.03f,8);wall(m,x+.4f,4.62f,x+.6f,4.7f,fd+.03f,fd+.06f,12);}
 // --- Cascade control, on the deck east ---------------------------------------------------
 pbGlazed(m,17.f,1.f,17.12f,5.f,fd);pbGlazed(m,17.f,7.f,17.12f,8.88f,fd);wall(m,17.f,5.f,17.12f,7.f,fd+2.2f,fd+2.6f,3);
 pbGlazed(m,17.f,8.88f,18.2f,9.f,fd);pbGlazed(m,19.6f,8.88f,23.f,9.f,fd);wall(m,18.2f,8.88f,19.6f,9.f,fd+2.2f,fd+2.6f,3);
 wall(m,17.f,1.f,23.f,9.f,fd+2.6f,fd+2.75f,2);
 wall(m,17.f,8.95f,23.f,9.05f,fd+2.75f,fd+3.5f,3);
 // The mimic board of the pressure cascade fills the north wall.
 wall(m,17.4f,1.f,22.7f,1.14f,fd+.8f,fd+2.45f,2);
 for(auto [z,mat]:{std::pair{.95f,19},{1.12f,18},{1.29f,20},{1.46f,14}})wall(m,17.7f,1.14f,22.4f,1.152f,fd+z,fd+z+.05f,mat);
 for(float x=18.2f;x<22.4f;x+=1.05f)wall(m,x,1.14f,x+.04f,1.152f,fd+.9f,fd+1.55f,20);
 for(float x:{18.4f,21.7f})m.fixtures.push_back({model::lab_control_board,{x,1.24f},fd-base+.05f,1.32f,.19f,.6f,kPi,false});
 terminal(m,{18.8f,3.3f},"CASCADE CONTROL / STATUS","SUPPLY SF-1, SF-2 STOPPED: SECURITY ORDER 7, 02:20.","EXHAUST RUNNING. LABS -310 PA. LOCKS INTERLOCKED.",fd-base);
 terminal(m,{20.7f,3.3f},"SUPPLY FAN SF-2 / RESTART","INTERLOCK: OUTSIDE AIR DAMPER OA-2 MUST BE OPEN.","OA-2 LOCKED OUT: OPEN IT BY HAND, FAN DECK WEST END.",fd-base,supply,damper);
 terminal(m,{22.3f,6.1f},"CASCADE / DUTY LOG","02:20 ORDER 7: STOP LAB SUPPLY. 41 BADGES STILL INSIDE.","02:23 ASKED WHO SIGNED IT. THEY SAID OPEN THE DOOR.",fd-base);m.terminals.back().yaw=-kPi*.5f;
 for(float x:{18.8f,20.9f})m.fixtures.push_back({model::office_chair,{x,4.15f},fd-base,.55f,.55f,.95f,x<19?.2f:-.5f,true});
 m.fixtures.push_back({model::file_cabinet_wide,{22.7f,8.f},fd-base,.9f,.5f,.75f,-kPi*.5f,true});
 m.fixtures.push_back({model::cctv_tv,{22.7f,8.f},fd-base+.75f,.5f,.4f,.42f,-kPi*.5f,false});
 m.fixtures.push_back({model::binder,{22.6f,7.55f},fd-base+.75f,.3f,.08f,.32f,-kPi*.5f,false});
 // The duty engineer: shot at his console when he would not open the lock.
 pbBody(m,"ere_util_clock",ActorVisual::Worker,{21.2f,4.9f},fd,2.2f);
 pbBlood(m,20.5f,4.5f,21.9f,5.4f,fd);pbBlood(m,21.f,5.4f,21.4f,6.1f,fd);
 wall(m,21.3f,1.152f,21.9f,1.158f,fd+1.05f,fd+1.65f,16);
 // --- UPS room under the control room -------------------------------------------------------
 wall(m,17.f,1.f,17.2f,8.8f,base,under,15);
 wall(m,17.f,8.8f,20.6f,9.f,base,under,15);wall(m,22.2f,8.8f,23.f,9.f,base,under,15);wall(m,20.6f,8.8f,22.2f,9.f,base+2.15f,under,15);
 for(float y:{1.9f,3.2f,4.5f,5.8f})m.fixtures.push_back({model::pwr_tall_switch_cabinet,{22.6f,y},0,.76f,1.23f,2.42f,kPi,true});
 for(float x:{18.f,19.3f})m.fixtures.push_back({model::electrical_cabinet,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 cableTray(m,{17.6f,1.6f},{22.f,1.6f},base+2.35f);
 // --- Containment exhaust: two scrubbers with trap cartridges and the HEPA bank ----------
 for(Vec2 c:{Vec2{3.4f,15.4f},Vec2{3.4f,19.8f}}){
  wall(m,c.x-1.45f,c.y-1.45f,c.x+1.45f,c.y+1.45f,base,base+.2f,15);
  vessel(m,c,1.15f,base+.2f,-14.6f,5);
  m.pipes.push_back({c,c,-14.6f,.95f,-14.3f,5});m.pipes.push_back({c,c,-14.3f,.55f,-14.1f,6});
  pipeRoute(m,{{c.x,c.y,-14.1f},{c.x,c.y,roof}},.32f,5,NoRoof);
  // The trap cartridge housing on its stand, piped off the scrubber's east side.
  pbStand(m,c.x+1.5f,c.y-.45f,c.x+2.3f,c.y+.45f,base,.95f);
  m.fixtures.push_back({model::lab_test_chamber,{c.x+1.9f,c.y},.95f,1.f,.6f,.56f,kPi*.5f,false});
  m.pipes.push_back({{c.x+1.15f,c.y},{c.x+1.55f,c.y},base+1.22f,.09f,base+1.22f,6});
 }
 pipeRoute(m,{{3.4f,22.95f,-16.f},{3.4f,20.95f,-16.f}},.3f,0,roof);
 pipeRoute(m,{{3.4f,16.55f,-16.f},{3.4f,18.65f,-16.f}},.3f,0,roof);
 for(float y:{13.9f,15.3f}){
  wall(m,5.7f,y,7.1f,y+1.2f,base+.4f,base+1.95f,20);
  for(float x:{5.72f,7.02f})for(float yy:{y+.05f,y+1.09f})wall(m,x,yy,x+.06f,yy+.06f,base,base+.4f,12);
  wall(m,7.1f,y+.15f,7.13f,y+1.05f,base+.6f,base+1.7f,2);
  wall(m,7.13f,y+.5f,7.17f,y+.7f,base+1.05f,base+1.25f,8);
  pipeRoute(m,{{6.4f,y+.6f,base+1.95f},{6.4f,y+.6f,-15.6f},{4.45f,y+.6f,-15.6f}},.14f,0,roof);
 }
 terminal(m,{6.4f,12.2f},"CONTAINMENT EXHAUST / EX-1","TRAP CARTRIDGES CHANGED 3 TIMES THIS WEEK. SPEC: QUARTERLY.","SPENT UNITS GO BACK TO ENERGY RESEARCH. DO NOT OPEN.");
 m.fixtures.push_back({model::pwr_duct_fan_unit,{1.42f,12.9f},0,.79f,1.49f,1.95f,0,true});
 for(float x:{5.4f,6.7f}){m.fixtures.push_back({model::pallet,{x,22.3f},0,1.2f,1.15f,.16f,0,true});
  m.fixtures.push_back({model::lab_specimen_crate,{x,22.3f},.16f,1.f,1.f,1.f,x>6?.25f:0,true});}
 // The lab gas manifold, chained upright behind its mesh.
 for(float x=1.45f;x<4.4f;x+=.42f)m.fixtures.push_back({model::gas_cylinder,{x,22.6f},0,.33f,.33f,1.58f,0,true});
 wall(m,1.f,22.38f,4.5f,22.42f,base+1.1f,base+1.15f,12);
 m.pipes.push_back({{1.2f,22.88f},{4.4f,22.88f},base+1.8f,.04f,base+1.8f,3});
 m.fixtures.push_back({model::mine_mesh_fence,{2.7f,21.5f},0,3.4f,.06f,2.3f,0,true});
 // --- Pressure lock PL-3 to Research Receiving ---------------------------------------------
 wall(m,7.6f,19.4f,7.8f,23.f,base,base+3.4f,3);wall(m,12.2f,19.4f,12.4f,23.f,base,base+3.4f,3);
 wall(m,7.6f,19.4f,8.f,19.6f,base,base+3.4f,3);wall(m,12.f,19.4f,12.4f,19.6f,base,base+3.4f,3);
 wall(m,8.f,19.4f,12.f,19.6f,base+2.65f,base+3.4f,3);
 wall(m,7.6f,19.4f,12.4f,24.f,base+3.4f,base+3.6f,2);
 wall(m,8.f,22.85f,12.f,23.f,base+3.6f,roof,3);
 for(float x:{7.8f,12.05f})wall(m,x,21.9f,x+.15f,22.1f,base,base+2.7f,12);
 wall(m,7.8f,21.9f,12.2f,22.1f,base+2.65f,base+2.75f,12);
 {Door lock{8.f,12.f,19.5f};lock.requireState=supply;m.doors.push_back(lock);}
 for(float x=8.1f;x<12.f;x+=.5f)wall(m,x,19.1f,x+.25f,19.35f,base,base+.008f,13);
 wallSign(m,10.f,19.345f,base+2.72f,"PRESSURE LOCK PL-3","RESEARCH RECEIVING / CLEAN SIDE",3.6f,kPi);
 terminal(m,{13.5f,18.7f},"PRESSURE LOCK PL-3 / RECEIVING","DIFFERENTIAL 310 PA. THE DOOR WILL NOT OPEN AGAINST IT.","RESTORE SUPPLY AT CASCADE CONTROL: STAIR UP, EAST.");
 m.lights.push_back(pbSwitched(lamp({7.8f,19.38f},base+2.3f,1.2f,5.f,1200,-.3f,WorldLightMount::Wall,-kPi*.5f),supply,true));
 m.lights.push_back(pbSwitched(lamp({12.2f,19.38f},base+2.3f,1.1f,5.f,5600,.4f,WorldLightMount::Wall,-kPi*.5f),supply));
 m.lights.push_back(lamp({10.f,21.f},base+3.25f,.9f,4.5f,6400,.15f));
 // --- Chilled water: CH-2 and its pump set, the overflowing condensate tray -------------
 for(float y:{20.5f,21.9f}){
  m.pipes.push_back({{19.4f,y},{22.6f,y},base+.85f,.5f,base+.85f,5});
  for(float x:{19.4f,22.6f})m.pipes.push_back({{x-.05f,y},{x+.05f,y},base+.85f,.56f,base+.85f,6});
  for(float x:{19.9f,22.1f})wall(m,x-.15f,y-.45f,x+.15f,y+.45f,base,base+.45f,15);
 }
 m.structures.push_back({19.3f,19.95f,22.7f,22.45f,base,base+1.35f,false,6});
 m.fixtures.push_back({model::compressor,{21.f,21.2f},1.35f,1.8f,1.05f,.9f,0,true});
 m.fixtures.push_back({model::mine_dewatering_pump,{15.9f,22.45f},0,2.6f,.8f,1.49f,0,true});
 pipeRoute(m,{{17.1f,22.45f,base+1.49f},{17.1f,22.45f,-14.8f},{20.5f,22.45f,-14.8f},{20.5f,21.2f,-14.8f},{20.5f,21.2f,base+2.25f}},.12f,5,roof);
 m.waterVolumes.push_back({14.4f,19.8f,18.8f,21.7f,base-.16f,base-.04f});
 hazard(m,Hazard::Kind::Anomaly,15.1f,20.3f,18.1f,21.2f,base-.5f,base+2.f,7);
 for(float x:{14.25f,18.85f})floorLine(m,x,19.6f,x+.15f,21.9f,base,13);floorLine(m,14.25f,19.6f,19.f,19.75f,base,13);
 board(m,16.6f,19.05f,base+1.15f,"CONDENSATE","STANDING WATER HUMS / KEEP OUT",2.4f);
 m.lights.push_back(lamp({16.6f,20.75f},base+.35f,.7f,4.f,11000,-.05f,WorldLightMount::Bare));
 m.lights.back().flicker=.35f;
 cableTray(m,{12.6f,22.6f},{22.6f,22.6f},base+3.8f);
 cableTray(m,{1.2f,22.6f},{7.4f,22.6f},base+3.8f);
 // --- Supply restart: the fans spool up, the lock releases, the riser empties ----------
 pbSequence(m,"ere_util_fan_ms","ere_util_fan_burst",9000,2900);
 cue(m,"ere_util_fan_ms",200,Sound::LiftMotor,{12.2f,2.4f},"SF-2 RUNNING. CASCADE EQUALIZING.",.8f,fd+1.f);
 cue(m,"ere_util_fan_ms",2500,Sound::JunkMetal,{15.8f,4.3f},"",1.f,fd+1.2f);
 cue(m,"ere_util_fan_ms",3100,Sound::WaspCall,{15.8f,5.5f},"",1.f,fd+.8f);
 cue(m,"ere_util_fan_ms",7000,Sound::Door,{10.f,20.5f},"PL-3: DIFFERENTIAL 12 PA. LOCK RELEASED.",.9f,base+1.5f);
 {auto e=pbEvent("ere_util_supply_start",0,0,24,24,-100,100,supply);e.actions={pbSet(stateId("ere_util_fan_ms")),pbShake(.16f),pbSave()};m.scriptEvents.push_back(e);}
 {auto e=pbEvent("ere_util_riser_burst",0,0,24,24,-100,100,stateId("ere_util_fan_burst"));e.actions={pbSpawn(CreatureKind::Wasp,{15.9f,5.8f},fd,8.f)};m.scriptEvents.push_back(e);}
 // Fan-running lamps on the deck come on with the supply.
 for(float x:{11.6f,12.9f})m.lights.push_back(pbSwitched(lamp({x,1.02f},fd+1.8f,.6f,3.f,2200,-.1f,WorldLightMount::Wall,kPi*.5f),supply));
 // --- Light -------------------------------------------------------------------------------
 for(Vec2 l:{Vec2{6.f,10.f},Vec2{12.f,10.f},Vec2{12.5f,15.5f},Vec2{19.5f,16.5f}})m.lights.push_back(lamp(l,roof-.3f,1.7f,10.f,4300,.15f));
 m.lights.push_back(pbSwitched(lamp({5.f,17.5f},roof-.3f,1.4f,9.f,4300,.15f),0,false,.3f));
 for(Vec2 l:{Vec2{4.5f,3.5f},Vec2{10.5f,4.f}})m.lights.push_back(lamp(l,roof-.3f,1.1f,7.f,4300,.15f));
 for(float x:{3.8f,8.5f,13.f})m.lights.push_back(lamp({x,5.22f},base+2.3f,.9f,4.5f,4000,.1f,WorldLightMount::Wall,kPi*.5f));
 m.lights.push_back(lamp({19.3f,4.f},fd+2.45f,1.1f,6.f,3800));m.lights.push_back(lamp({21.5f,7.f},fd+2.45f,.8f,4.f,3600));
 m.lights.push_back(pbSwitched(lamp({17.7f,1.16f},fd+2.05f,.8f,3.f,1200,-.3f,WorldLightMount::Wall,kPi*.5f),supply,true));
 m.lights.push_back(lamp({20.f,5.f},under-.15f,.8f,5.f,2400,-.1f));
 m.lights.push_back(lamp({19.f,21.f},roof-.3f,1.2f,8.f,1900,-.2f));
 m.lights.push_back(lamp({19.f,11.f},roof-.3f,1.f,7.f,4300,.15f));
 m.lights.push_back(lamp({1.02f,9.2f},base+3.f,.9f,5.f,4000,.1f,WorldLightMount::Wall,0));
 // --- Encounters and rewards ----------------------------------------------------------
 m.creatureSpawns={{CreatureKind::Huntsman,{6.2f,18.4f},base}};
 m.pickupSpawns={{{1.9f,12.2f},PickupKind::Health,base},{{19.6f,3.f},PickupKind::Ammo,base},{{4.4f,6.f},PickupKind::Ammo,fd}};
 m.clutterSpawns={{3,{18.6f,2.2f},base},{4,{20.4f,6.4f},base},{2,{9.f,12.f},base},{0,{14.f,13.f},base},{1,{19.7f,7.4f},fd}};
 zone(m,"RESTRICTED UTILITIES / CASCADE CONTROL",17,1,23,9,fd-.5f,1000);
 zone(m,"RESTRICTED UTILITIES / FAN DECK",1,1,17,7,fd-.5f,1000);
 zone(m,"RESTRICTED UTILITIES / UPS ROOM",17.2f,1,23,8.8f,-100,fd-.5f);
 zone(m,"RESTRICTED UTILITIES / PRESSURE LOCK PL-3",7.6f,19.4f,12.4f,24);
 zone(m,"RESTRICTED UTILITIES / CONTAINMENT EXHAUST",1,13,7.6f,23);
 zone(m,"RESTRICTED UTILITIES / CHILLED WATER",13,19.5f,23,23);
 zone(m,"RESTRICTED UTILITIES",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// RESEARCH RECEIVING (manifest 42). Where the freight manifests pointed:
// "RESEARCH CONTAINERS HELD AT THE NEXT TRANSFER". Train 27 was unloaded here
// off the books at 01:40 and its containers held under Order 7 in the
// chain-link hold. R-114 started knocking at 01:50 and was logged as load
// settling; at 02:31 it opened from the inside. R-119 is still sealed: LIVE,
// 74 KG. Staff came through the personnel sort to the staff lane and up to
// the clean transfer gallery and Decon 1; the lane has been locked since the
// lockdown at 02:09, so the only way up is the freight stair at the back of
// the hold, past the containers. The clean-side release opens the lane again
// as a shortcut. The thing from R-114 waits in the dark of the cold hold.
std::shared_ptr<const Map> makeResearchReceiving(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[64];m.definition=CampaignChunks[64];m.definition.ambient=.28f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float base=-19,gz=-16,roof=m.definition.ceiling,under=gz-.22f;
 const StateId wake=stateId("ere_recv_wake"),staff=stateId("ere_recv_staff");
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 openEdge(m,{'N',8,12,base,false,0});openEdge(m,{'S',10,14,gz,false,0});
 machineryClock(m,"ere_recv_clock",60000);
 // --- Arrival from PL-3: a low vestibule, then the hall opens ------------------------------
 for(float x:{7.6f,12.f})wall(m,x,1.f,x+.4f,3.2f,base,base+3.4f,3);
 wall(m,7.6f,0,12.4f,3.2f,base+3.4f,base+3.6f,2);
 wall(m,8.f,0,12.f,.2f,base+3.6f,roof,3);
 for(float x:{7.4f,12.f})wall(m,x,3.2f,x+.6f,3.5f,base,base+4.f,15);
 wall(m,7.4f,3.2f,12.6f,3.5f,base+3.4f,base+4.f,15);
 floorLine(m,8.f,3.2f,12.f,3.35f,base,13);
 // --- Hold 2, west: Train 27's research containers behind the chain-link ----------------
 // The rest of the load stands stacked against the north wall. Leave the
 // corner pallet clear of both rows and the vestibule wall.
 for(auto [x,y,two]:{std::tuple{1.8f,1.75f,true},{3.1f,1.75f,false},{4.4f,1.75f,true},{5.7f,1.75f,false},{1.8f,2.95f,false},{3.1f,2.95f,true},{4.4f,2.95f,false},{5.7f,2.95f,true},{6.95f,2.3f,false}}){
  pbFix(m,model::pallet,x,y,base,1.2f,1.15f,.16f,0,true);
  pbFix(m,model::lab_specimen_crate,x,y,base+.16f,1.08f,1.08f,1.08f,0,true);
  if(two)pbFix(m,model::lab_specimen_crate,x,y,base+1.24f,1.f,1.f,1.f,.15f,true);}
 pbContainerX(m,1.25f,3.75f,base,true,true);    // R-114, opened from the inside
 pbContainerX(m,1.25f,7.2f,base,true,false);    // R-119, sealed
 // R-114: one door torn off and thrown, the other folded back, the inner
 // grille bent outward, a latch bar twisted down to the floor.
 wall(m,7.35f,3.7f,8.3f,5.15f,base,base+.06f,11);
 for(float x:{7.5f,8.1f})wall(m,x,3.75f,x+.06f,5.1f,base+.06f,base+.1f,8);
 wall(m,5.8f,6.77f,7.25f,6.83f,base+.06f,base+2.66f,11);
 pbFix(m,model::mine_bent_grate,7.33f,5.26f,base+.08f,2.45f,.086f,2.64f,pbE,false);
 wall(m,7.28f,3.87f,7.38f,4.55f,base,base+2.7f,CollisionProxyMaterial);wall(m,7.28f,5.97f,7.38f,6.65f,base,base+2.7f,CollisionProxyMaterial);
 m.pipes.push_back({{7.32f,4.4f},{7.85f,4.2f},base+1.2f,.025f,base+.09f,6});
 pbFix(m,model::med_mattress_stained,3.1f,4.6f,base+.08f,1.9f,.9f,.12f,0,false);
 for(float y:{4.35f,5.f})m.pipes.push_back({{1.42f,y},{2.25f,y+.05f},base+.45f,.016f,base+.22f,6});
 for(float x=5.3f;x<7.f;x+=.38f)wall(m,x,6.62f,x+.03f,6.65f,base+.9f+std::fmod(x*7.f,.5f),base+1.6f+std::fmod(x*5.f,.4f),8);
 pbBlood(m,5.6f,4.9f,7.2f,5.6f,base+.08f);pbBlood(m,7.3f,5.05f,9.4f,5.4f,base);
 wall(m,1.4f,3.9f,1.43f,6.62f,base+.3f,base+2.5f,20);
 // A red hold beacon on R-114's end frame.
 wall(m,7.f,5.16f,7.2f,5.36f,base+2.8f,base+2.86f,19);
 // R-119: placard, seal, status readout; something knocks inside.
 pbSign(m,7.305f,8.71f,base+1.25f,"R-119 / LIVE / 74 KG","SEDATE ON ARRIVAL / DO NOT OPEN BELOW DECON",1.9f,pbE,pbRed);
 pbFix(m,model::lab_status_display,7.28f,8.71f,base+2.15f,.69f,.06f,.2f,pbE+kPi,false);
 wall(m,7.25f,7.85f,7.32f,8.15f,base+1.05f,base+1.2f,19);
 // The fence: chain-link panels, one standing open as the gate, and the
 // cold hold fenced in under the gallery.
 for(float y:{5.195f,8.385f,14.765f})pbFix(m,model::mine_mesh_fence,8.45f,y,base,3.19f,.06f,2.78f,pbE,true);
 pbFix(m,model::mine_mesh_doorway,8.45f,11.575f,base,3.19f,.06f,2.78f,pbE,false);
 wall(m,8.42f,9.98f,8.48f,11.03f,base,base+2.78f,CollisionProxyMaterial);wall(m,8.42f,12.12f,8.48f,13.17f,base,base+2.78f,CollisionProxyMaterial);
 for(float y:{18.45f,21.35f})pbFix(m,model::mine_mesh_fence,8.45f,y,base,2.9f,.06f,2.78f,pbE,true);
 wall(m,8.f,3.2f,8.5f,3.5f,base,base+2.9f,15);wall(m,8.42f,3.5f,8.48f,3.6f,base,base+2.78f,12);
 wall(m,8.4f,16.36f,8.5f,17.f,base,base+2.78f,12);
 for(float y:{3.6f,6.79f,9.98f,13.17f,16.36f,19.9f})wall(m,8.41f,y-.04f,8.49f,y+.04f,base,base+2.78f,12);
 // The freight stair to the gallery, against the west wall.
 m.stairs.push_back({1.3f,12.5f,3.3f,17.f,base,gz,16,true,true});
 // R-117: a transfer cage, its door pulled off and bent.
 pbFix(m,model::med_holding_cage,6.25f,14.f,base,3.82f,1.73f,1.71f,kPi,true);
 pbFix(m,model::mine_bent_grate,3.95f,14.f,base,1.6f,.06f,1.72f,pbE,false);
 wall(m,4.25f,15.f,5.3f,15.2f,base+.5f,base+.62f,19);
 for(float x:{4.6f,5.9f,7.2f}){pbFix(m,model::pallet,x,16.f,base,1.2f,1.15f,.16f,0,true);pbFix(m,model::lab_specimen_crate,x,16.f,base+.16f,1.08f,1.08f,1.08f,x>5?.2f:0,true);}
 // --- Receiving office and personnel sort, north-east -----------------------------------
 const float oc=base+2.9f;
 pbGlazed(m,12.4f,4.2f,15.2f,4.4f,base,2.9f);pbGlazed(m,16.2f,4.2f,17.8f,4.4f,base,2.9f);
 wall(m,15.2f,4.2f,16.2f,4.4f,base+2.2f,oc,21);
 for(float x:{15.12f,16.2f})wall(m,x,4.17f,x+.08f,4.43f,base,base+2.2f,20);
 {Door d{15.2f,16.2f,4.3f,0,false,false,false,0};d.swinging=true;d.open=.75f;d.opening=true;m.doors.push_back(d);}
 wall(m,17.6f,1.f,17.8f,4.2f,base,oc,21);wall(m,12.2f,3.5f,12.4f,4.4f,base,oc,21);
 wall(m,12.4f,1.f,17.8f,4.4f,oc,oc+.15f,2);
 ceilingGrid(m,12.4f,1.f,17.6f,4.2f,oc);
 workstation(m,14.5f,3.25f,base,1,2);
 pbFix(m,model::lab_desk_lamp,15.05f,3.4f,base+.75f,.18f,.4f,.52f,kPi,false);
 terminal(m,{13.3f,1.6f},"RESEARCH RECEIVING / HOLD LOG","01:50 R-114 KNOCKING. LOGGED AS LOAD SETTLING.","02:31 R-114 OPEN. LATCHES BENT OUTWARD. NOT LOGGED.");m.terminals.back().yaw=pbS;
 terminal(m,{17.05f,2.5f},"TRAIN 27 / RECEIVED HERE","01:40 OFF-BOOK UNLOAD: 6 RESEARCH CONTAINERS.","FREIGHT TO RECORD CARGO: NONE. HOLD PER ORDER 7.");m.terminals.back().yaw=pbW;
 pbFix(m,model::file_cabinet_tall,12.75f,3.75f,base,.5f,.6f,1.3f,pbE,true);
 pbFix(m,model::file_cabinet_wide,15.6f,1.3f,base,.9f,.5f,.75f,pbS,true);
 pbFix(m,model::cctv_tv,15.6f,1.3f,base+.75f,.5f,.4f,.42f,pbS,false);
 terminal(m,{13.7f,5.f},"PERSONNEL SORT / SHIFT 02","ARRIVALS 31. CLEARED TO DECON 27. HELD FOR SCREENING 4.","HELD STAFF: MEDICAL ESCORT ONLY. DO NOT INFORM THEM.");m.terminals.back().yaw=pbS;
 for(float x:{16.6f,19.6f})for(float y:{4.9f,5.5f}){m.pipes.push_back({{x,y},{x,y},base,.035f,base+.95f,5});m.pipes.push_back({{x,y},{x,y},base,.17f,base+.03f,6});wall(m,x-.06f,y-.06f,x+.06f,y+.06f,base,base+.95f,CollisionProxyMaterial);}
 for(float y:{4.9f,5.5f})wall(m,16.6f,y-.015f,19.6f,y+.015f,base+.83f,base+.89f,19);
 // The staff lane: glazed on the hall side, its door locked since the lockdown.
 pbGlazed(m,17.9f,6.1f,18.1f,17.f,base,3.f);
 wall(m,17.9f,5.9f,20.f,6.1f,base,base+3.6f,3);wall(m,22.6f,5.9f,23.f,6.1f,base,base+3.6f,3);wall(m,20.f,5.9f,22.6f,6.1f,base+2.65f,base+3.6f,3);
 for(float x:{19.92f,22.6f})wall(m,x,5.86f,x+.08f,6.14f,base,base+2.7f,20);
 {Door d{20.f,22.6f,6.f,0};d.requireState=staff;m.doors.push_back(d);}
 pbFix(m,model::lab_door_reader,19.45f,5.64f,base,.85f,.52f,2.39f,0,false);
 pbSign(m,18.5f,5.845f,base+1.45f,"LOCKDOWN 02:09","RELEASE FROM CLEAN SIDE",.95f,kPi,pbRed);
 m.stairs.push_back({20.8f,12.5f,22.8f,17.f,base,gz,16,true,true});
 for(float y:{7.6f,8.5f,9.4f})pbLockers(m,22.5f,y,base,pbW);
 bench(m,19.8f,10.8f,base);
 // --- The sorting floor and the gantry crane ------------------------------------------
 for(float x:{1.f,22.6f}){wall(m,x,3.6f,x+.4f,22.6f,-13.6f,-13.2f,12);
  for(float y=5.f;y<22;y+=5.f){wall(m,x,y-.22f,x+.4f,y+.22f,-14.6f,-13.6f,15);wall(m,x,y-.18f,x+.24f,y+.18f,base+3.f,-14.6f,15);}}
 for(float y:{10.2f,11.1f})wall(m,1.4f,y,22.6f,y+.3f,-13.2f,-12.6f,18);
 for(float x:{1.f,22.4f})wall(m,x,10.f,x+.6f,11.6f,-13.25f,-12.5f,12);
 wall(m,11.2f,10.1f,12.8f,11.5f,-12.6f,-12.2f,12);
 m.pipes.push_back({{11.3f,10.85f},{12.7f,10.85f},-12.75f,.18f,-12.75f,6});
 for(float y:{10.65f,11.05f})m.pipes.push_back({{12.f,y},{12.f,y},-12.9f,.02f,-14.1f,6});
 wall(m,11.8f,10.6f,12.2f,11.1f,-14.45f,-14.1f,18);
 for(float dx:{-.45f,.45f})for(float dy:{-.45f,.45f})m.pipes.push_back({{12.f,10.85f},{12.f+dx,10.85f+dy},-14.45f,.012f,-14.9f,6});
 pbFix(m,model::lab_specimen_crate,12.f,10.85f,-16.02f,1.12f,1.12f,1.12f,.2f,false);
 for(float y:{9.8f,11.9f})floorLine(m,10.9f,y,13.1f,y+.12f,base,13);
 for(float x:{10.9f,12.98f})floorLine(m,x,9.8f,x+.12f,12.02f,base,13);
 // Sorting lanes painted from the vestibule to the staging bays.
 for(float x:{12.9f,15.1f,17.3f})floorLine(m,x,6.3f,x+.1f,9.4f,base);
 floorLine(m,12.9f,9.3f,17.4f,9.4f,base);
 cargo(m,14.f,7.8f,base);cargo(m,16.2f,7.6f,base,false);
 pbFix(m,model::lab_specimen_crate,16.2f,7.6f,base+.16f,1.08f,1.08f,1.08f,.1f,true);
 // Inspection bench: specimen 4471 unpacked, its seal broken.
 pbFix(m,model::lab_clean_bench,14.7f,14.75f,base,1.18f,.56f,.81f,0,true);
 pbFix(m,model::lab_scope_monitor,15.f,14.8f,base+.81f,.47f,.34f,.45f,0,false);
 pbFix(m,model::lab_field_meter,14.35f,14.7f,base+.81f,.25f,.33f,.15f,0,false);
 pbFix(m,model::pallet,13.f,14.6f,base,1.2f,1.15f,.16f,0,true);
 pbOpenCrate(m,13.f,14.6f,base+.16f);
 pbFix(m,model::lab_anomaly_relic,13.f,14.6f,base+.64f,.5f,.4f,.4f,.6f,false);
 wall(m,11.6f,15.4f,12.7f,16.5f,base,base+.05f,10);
 pbFix(m,model::mine_depot_forklift,15.9f,12.6f,base,1.2f,3.73f,2.27f,pbE,true);
 // The receiving clerk, dragged from the hold gate.
 pbBody(m,"ere_recv_clock",ActorVisual::Worker,{9.8f,12.9f},base,1.1f);
 pbBlood(m,8.6f,11.3f,9.4f,11.8f,base);pbBlood(m,9.f,11.7f,9.5f,12.5f,base);pbBlood(m,9.3f,12.4f,10.3f,13.3f,base);
 // --- Under the gallery: the cold hold and the container lift ----------------------------
 wall(m,10.f,22.8f,14.f,24.f,base,under,15);
 for(float x:{3.4f,4.2f,5.f})pbFix(m,model::lab_sample_fridge,x,22.36f,base,.7f,.85f,1.f,0,true);
 pbFix(m,model::lab_specimen_cabinet,7.6f,22.42f,base,1.34f,.74f,2.19f,0,true);
 pbFix(m,model::med_wheelchair,2.6f,20.2f,base,.55f,1.04f,.91f,.7f,true);
 pbBlood(m,3.6f,19.4f,6.1f,20.f,base);
 // The container lift CL-2 rises through the gallery to the clean side.
 wall(m,14.15f,18.15f,17.85f,21.85f,base,base+.22f,18);
 for(float y:{18.15f,21.73f})floorLine(m,14.15f,y,17.85f,y+.12f,base+.22f,13);
 for(float x:{14.15f,17.85f})for(float y:{18.15f,21.85f})steelColumn(m,x,y,base,roof-.6f,false);
 for(float y:{18.15f,21.85f})wall(m,14.f,y-.12f,18.f,y+.12f,roof-.9f,roof-.6f,12);
 m.pipes.push_back({{16.f,18.f},{16.f,22.f},roof-.75f,.22f,roof-.75f,6});
 // Suspend the CL-2 overhead run from the roof at both ends.
 for(float y:{18.f,22.f})m.pipes.push_back({{16.f,y},{16.f,y},roof-.75f,.022f,roof,6});
 pbFix(m,model::pallet,15.2f,20.f,base+.22f,1.2f,1.15f,.16f,0,true);
 pbFix(m,model::lab_specimen_crate,15.2f,20.f,base+.38f,1.08f,1.08f,1.08f,0,true);
 // Staff lockers in the lane's end, under the gallery.
 wall(m,17.9f,17.f,18.1f,22.8f,base,under,3);
 for(float y:{18.3f,19.2f,20.1f,21.f})pbLockers(m,22.5f,y,base,pbW);
 // --- The clean transfer gallery ------------------------------------------------------
 deck(m,gz,1,17,23,23);deck(m,gz,10,23,14,24);
 rectangle(pbDeckLayer(m,gz),14,18,18,22,'.');
 for(float x:{4.8f,12.8f,16.6f,20.f})steelColumn(m,x,17.3f,base,under);
 wall(m,1.f,17.f,23.f,17.3f,under-.32f,under,12);
 for(float y:{17.7f,22.f})wall(m,14.f,y,18.f,y+.3f,under-.32f,under,12);
 wall(m,8.6f,16.98f,13.4f,17.18f,gz,gz+1.1f,15);wall(m,8.56f,16.94f,13.44f,17.22f,gz+1.1f,gz+1.16f,20);
 guardRail(m,3.3f,17.f,8.6f,17.06f,gz);guardRail(m,13.4f,17.f,20.8f,17.06f,gz);
 guardRail(m,14.f,17.94f,18.f,18.f,gz);guardRail(m,14.f,22.f,18.f,22.06f,gz);guardRail(m,13.94f,18.f,14.f,22.f,gz);guardRail(m,18.f,18.f,18.06f,22.f,gz);
 pbFix(m,model::mine_cage_gate,13.9f,20.f,gz,2.1f,.07f,2.55f,pbE,false);
 // The clean face: lab panel from the gallery up, the portal to Decon 1.
 wall(m,1.f,22.88f,9.55f,23.f,gz,roof,28);wall(m,14.45f,22.88f,23.f,23.f,gz,roof,28);
 for(float x:{9.55f,14.f})wall(m,x,22.55f,x+.45f,23.f,gz,gz+3.3f,20);
 wall(m,9.55f,22.55f,14.45f,23.f,gz+2.8f,gz+3.3f,20);
 wall(m,10.f,23.f,14.f,24.f,gz+2.8f,roof,28);
 wall(m,9.f,20.5f,14.f,23.f,gz,gz+.015f,27);floorLine(m,9.f,20.4f,14.f,20.52f,gz,20);
 bench(m,5.5f,21.9f,gz);bench(m,20.4f,21.9f,gz);
 terminal(m,{8.f,21.95f},"DECON 1 / QUEUE","CYCLE ABORTED 02:20: SUPPLY AIR LOST. CHAMBER HELD SEALED.","SUPPLY BACK. ONE OCCUPANT STILL LOGGED INSIDE.",gz-base);
 terminal(m,{22.3f,19.8f},"STAFF LANE / CLEAN-SIDE RELEASE","E / RELEASE THE STAFF LANE DOOR BELOW.","LOCKDOWN 02:09: RELEASE FROM THIS SIDE ONLY.",gz-base,staff);m.terminals.back().yaw=pbW;
 // --- Script: the entry creak, the knocking in R-119, the waking -----------------------
 {TimedSequence s;s.timerState=stateId("ere_recv_creak_ms");s.finishState=stateId("ere_recv_creak_done");s.x1=8;s.y1=2.5f;s.x2=12;s.y2=5.5f;s.bottom=base-1;s.top=base+2;
  s.durationMs=4000;s.finishAtMs=3000;s.soundIntervalMs=0;m.timedSequences.push_back(s);}
 cue(m,"ere_recv_creak_ms",250,Sound::LiftCreak,{12.f,10.85f},"",.9f,-14.4f);
 {TimedSequence s;s.timerState=stateId("ere_recv_knock_ms");s.finishState=stateId("ere_recv_knock_cycled");s.x1=1;s.y1=3.5f;s.x2=8.45f;s.y2=23;s.bottom=base-1;s.top=base+2;
  s.durationMs=9500;s.finishAtMs=9500;s.soundIntervalMs=0;s.loop=true;m.timedSequences.push_back(s);}
 for(int t:{350,800,1150})cue(m,"ere_recv_knock_ms",t,Sound::JunkMetal,{5.5f,8.71f},"",.55f,base+1.2f);
 cue(m,"ere_recv_knock_ms",5200,Sound::JunkMetal,{5.5f,8.71f},"",.4f,base+.6f);
 pbSequence(m,"ere_recv_wake_ms","ere_recv_wake_done",6000,5000);
 cue(m,"ere_recv_wake_ms",80,Sound::JunkGlass,{4.6f,21.6f},"",1.f,base+.8f);
 cue(m,"ere_recv_wake_ms",700,Sound::WorkerDying,{5.6f,19.8f},"",1.1f,base+1.5f);
 {auto e=pbEvent("ere_recv_wake_trip",1.2f,10.25f,8.45f,13.f,base-.5f,base+1.5f);e.actions={pbSet(wake),pbSet(stateId("ere_recv_wake_ms"))};m.scriptEvents.push_back(e);}
 // --- Light ---------------------------------------------------------------------
 for(Vec2 l:{Vec2{10.5f,6.5f},Vec2{15.f,7.f},Vec2{10.f,14.f}})m.lights.push_back(lamp(l,roof-.5f,2.3f,14.f,4300,.22f));
 m.lights.push_back(pbSwitched(lamp({15.f,14.5f},roof-.5f,2.f,13.f,4300,.22f),0,false,.3f));
 m.lights.push_back(lamp({4.6f,11.6f},roof-.5f,2.f,11.f,1900,-.25f));
 m.lights.push_back(lamp({6.f,15.6f},roof-.5f,1.1f,7.f,1800,-.25f));
 pbLampHead(m,7.1f,5.26f,base+2.86f,19);
 {auto l=lamp({7.1f,5.26f},base+3.05f,.9f,5.f,1000,-.5f,WorldLightMount::Bare);l.flicker=.45f;m.lights.push_back(l);}
 m.lights.push_back(lamp({7.25f,9.75f},base+2.45f,.6f,3.5f,2700,0,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({15.f,2.6f},oc-.1f,.8f,5.f,3200));
 m.lights.push_back(lamp({15.05f,3.25f},base+1.2f,.35f,2.5f,2600,0,WorldLightMount::Bare));
 m.lights.push_back(lamp({10.f,1.8f},base+3.3f,.9f,5.f,4000,.1f));
 m.lights.push_back(lamp({20.5f,3.6f},roof-.5f,1.4f,9.f,4300,.2f));
 for(float y:{9.f,14.5f})m.lights.push_back(lamp({22.98f,y},base+2.6f,1.f,6.f,6000,.08f,WorldLightMount::Wall,kPi));
 m.lights.push_back(lamp({11.5f,20.f},under-.2f,.7f,6.f,8000,.05f));
 {auto l=lamp({2.2f,22.98f},base+2.3f,.5f,5.f,1300,-.3f,WorldLightMount::Wall,-kPi*.5f);l.flicker=.25f;m.lights.push_back(l);}
 m.lights.push_back(lamp({20.5f,20.f},under-.2f,.6f,5.f,4000));
 m.lights.push_back(lamp({12.f,10.85f},-12.95f,1.2f,9.f,4800));
 for(float x:{3.5f,7.5f,16.5f,20.5f})m.lights.push_back(lamp({x,22.88f},gz+2.5f,1.f,7.f,6500,.05f,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({12.f,23.4f},gz+2.7f,.9f,5.f,7000,.03f));
 // --- Encounters and rewards ----------------------------------------------------------
 // R-114 has an 8 cm steel floor; occupants and supplies stand on it.
 m.creatureSpawns={{CreatureKind::Huntsman,{3.6f,5.3f},base+.08f},{CreatureKind::Mutant,{5.6f,19.8f},base,1,0,wake},{CreatureKind::Huntsman,{12.f,20.4f},base}};
 m.pickupSpawns={{{2.2f,6.1f},PickupKind::Ammo,base+.08f},{{16.9f,1.6f},PickupKind::Health,base},{{9.2f,21.f},PickupKind::Ammo,gz},{{19.6f,18.4f},PickupKind::Health,base}};
 m.clutterSpawns={{3,{13.2f,3.7f},base},{4,{16.7f,3.8f},base},{0,{9.4f,15.2f},base},{2,{16.6f,16.2f},base},{1,{20.9f,20.5f},gz}};
 zone(m,"RESEARCH RECEIVING / CLEAN TRANSFER GALLERY",1,17,23,24,gz-.5f,1000);
 zone(m,"RESEARCH RECEIVING / RECEIVING OFFICE",12.4f,1,17.6f,4.2f);
 zone(m,"RESEARCH RECEIVING / STAFF LANE",18.1f,6.1f,23,23,-100,gz-.5f);
 zone(m,"RESEARCH RECEIVING / COLD HOLD",1,17,8.45f,23,-100,gz-.5f);
 zone(m,"RESEARCH RECEIVING / HOLD 2",1,1,8.45f,17);
 zone(m,"RESEARCH RECEIVING / PERSONNEL SORT",12.4f,4.4f,23,5.9f);
 zone(m,"RESEARCH RECEIVING",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// DECONTAMINATION (manifest 43). Decon 1, where the plant ends and the labs
// begin: the factory palette gives way to white panel, tile and cold tubes.
// Stage 1 disrobe (lockers, sinks, garment bins, the operator's window), the
// stage 2 shower, then the sealed chamber: air shower and exposure monitor,
// run from a console inside. Cycle 0412 aborted at 02:20 when Order 7 stopped
// the supply air, sealing a technician in; at 03:12 the Cleanup opened the
// clean door with an override and closed it again a minute later. With supply
// restored the player runs cycle 0413: the chamber seals, the fans run, and
// on the clean side the man who signed in at 03:05 radios that someone is
// coming up. Cycle complete sets decon_cycle: the clean door opens on him, and
// past stage 5 gowning the south door gives onto the Energy Research court.
// States: ere_decon_start (console), ere_decon_sealed (chamber lock),
// ere_decon_ms (the cycle), decon_cycle (cycle complete).
std::shared_ptr<const Map> makeDecontamination(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[65];m.definition=CampaignChunks[65];m.definition.ambient=.42f;
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceLabPanel;
 const float f=-16,ceil=m.definition.ceiling;
 const StateId cycle=stateId("decon_cycle"),start=stateId("ere_decon_start"),sealed=stateId("ere_decon_sealed");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'S',10,14,f,true,cycle});      // door 0: the clean side's way out
 openEdge(m,{'N',10,14,f,false,0});
 machineryClock(m,"ere_decon_clock",60000);
 auto panel=[&](float x1,float y1,float x2,float y2,int mat){wall(m,x1,y1,x2,y2,f,ceil,mat);wall(m,x1-.012f,y1-.012f,x2+.012f,y2+.012f,f,f+.12f,20);};
 auto frame=[&](float a,float b,float y){for(float x:{a-.08f,b})wall(m,x,y-.14f,x+.08f,y+.14f,f,f+2.7f,20);wall(m,a-.08f,y-.14f,b+.08f,y+.14f,f+2.62f,f+2.7f,20);};
 // --- The way in from the clean transfer gallery ---------------------------------------
 wall(m,10.f,.8f,14.f,1.f,f+2.8f,ceil,28);
 for(float x:{9.8f,14.f})wall(m,x,.8f,x+.2f,1.12f,f,f+2.8f,20);
 // --- Stage 1: disrobe ----------------------------------------------------------------
 panel(1.f,7.4f,3.f,7.6f,28);panel(5.f,7.4f,10.5f,7.6f,28);panel(13.5f,7.4f,18.5f,7.6f,28);panel(20.f,7.4f,23.f,7.6f,28);
 for(auto [a,b]:{std::pair{3.f,5.f},{10.5f,13.5f},{18.5f,20.f}}){wall(m,a,7.4f,b,7.6f,f+2.65f,ceil,28);frame(a,b,7.5f);}
 m.doors.push_back({3.f,5.f,7.5f,0});            // 1: decon plant
 m.doors.push_back({10.5f,13.5f,7.5f,0});        // 2: stage 2 shower
 for(int i=0;i<8;++i)pbLockers(m,1.5f+i*.9f,1.45f,f,pbS);
 bench(m,4.6f,3.25f,f);bench(m,4.6f,5.35f,f);
 for(float y:{4.4f,5.1f,5.8f})pbFix(m,model::drum,1.45f,y,f,.6f,.6f,.86f,y*2.f,true);
 pbFix(m,model::stock_cage,2.05f,6.85f,f,1.6f,.9f,1.1f,0,true);
 for(float x:{6.2f,7.2f,8.2f,9.2f})pbFix(m,model::med_scrub_sink,x,7.13f,f,.49f,.54f,1.13f,pbN,true);
 for(float x:{6.7f,8.7f})pbFix(m,model::med_washroom_mirror,x,7.38f,f+1.15f,1.5f,.04f,1.f,pbN,false);
 floorLine(m,9.6f,6.85f,14.4f,6.97f,f,19);
 pbSign(m,5.f,1.055f,f+2.62f,"DECONTAMINATION / 5 STAGES","DISROBE - SHOWER - AIR - MONITOR - GOWN",4.f,0,pbBlue);
 ceilingGrid(m,1.f,1.f,15.f,7.4f,ceil);
 for(Vec2 l:{Vec2{4.f,3.f},Vec2{8.f,5.4f},Vec2{12.f,2.8f},Vec2{12.f,5.8f}})m.lights.push_back(lamp(l,ceil-.16f,.95f,6.5f,6200,.04f));
 // The operator's booth: glazed onto stage 1, entered from the clean side.
 pbLabGlass(m,15.f,1.f,15.2f,7.4f,f,ceil);
 m.doors.push_back({18.5f,20.f,7.5f,0});         // 3: booth to the suit store
 for(float x:{17.5f,20.5f})pbFix(m,model::lab_control_board,x,1.12f,f+1.f,1.32f,.19f,.6f,kPi,false);
 pbDesk(m,16.1f,4.f,f,pbW,1);
 for(float y:{2.4f,3.5f}){pbFix(m,model::file_cabinet_wide,22.5f,y,f,.9f,.5f,.75f,pbW,true);pbFix(m,model::cctv_tv,22.5f,y,f+.75f,.5f,.4f,.42f,pbW,false);}
 pbFix(m,model::file_cabinet_tall,22.6f,6.6f,f,.5f,.6f,1.3f,pbW,true);
 pbFix(m,model::water_cooler,19.f,1.35f,f,.35f,.35f,1.1f,pbS,true);
 pbFix(m,model::lab_wall_clock,18.f,1.03f,f+2.3f,.4f,.03f,.4f,kPi,false);
 terminal(m,{14.45f,4.f},"DECON 1 / OPERATOR LOG","02:20 SUPPLY LOST. CHAMBER SEALED. RELEASE ASKED 02:21, 02:24, 02:31.","03:12 CLEAN DOOR OPENED BY CLEANUP OVERRIDE. 03:13 CLOSED.");m.terminals.back().yaw=pbW;
 m.lights.push_back(lamp({19.f,4.2f},ceil-.16f,.9f,6.f,5200,.03f));
 // --- Stage 2: the shower ------------------------------------------------------------
 for(float x:{9.2f,14.6f})wall(m,x,7.6f,x+.2f,11.4f,f,ceil,27);
 wall(m,9.2f,11.4f,10.5f,11.6f,f,ceil,27);wall(m,13.5f,11.4f,14.8f,11.6f,f,ceil,27);wall(m,10.5f,11.4f,13.5f,11.6f,f+2.65f,ceil,27);frame(10.5f,13.5f,11.5f);
 {Door d{10.5f,13.5f,11.5f,0};d.requireState=sealed;d.requireValue=0;m.doors.push_back(d);}   // 4: chamber, locked while sealed
 for(float x:{9.47f,14.53f}){m.pipes.push_back({{x,7.6f},{x,11.4f},f+2.15f,.035f,f+2.15f,5});
  for(float y=8.f;y<11.2f;y+=.55f)m.pipes.push_back({{x,y},{x+(x<12?.1f:-.1f),y},f+2.15f,.022f,f+2.07f,5});}
 for(float y:{8.4f,9.5f,10.6f}){m.pipes.push_back({{12.f,y},{12.f,y},ceil-.3f,.02f,ceil,5});m.pipes.push_back({{12.f,y},{12.f,y},ceil-.34f,.12f,ceil-.3f,5});}
 for(float y:{8.6f,10.4f})pbFix(m,model::med_floor_drain,12.f,y,f,.5f,.5f,.01f,0,false);
 for(float y:{8.4f,10.6f})m.lights.push_back(lamp({11.2f,y},ceil-.18f,.9f,5.f,6500,.04f));
 // --- The chamber: air shower and exposure monitor -------------------------------------
 wall(m,9.2f,11.6f,9.4f,12.6f,f,ceil,27);wall(m,9.2f,16.2f,9.4f,17.f,f,ceil,27);
 pbLabGlass(m,9.2f,12.6f,9.4f,16.2f,f,ceil,27);
 wall(m,14.6f,11.6f,14.8f,17.f,f,ceil,27);
 pbLabGlass(m,9.2f,17.f,10.5f,17.2f,f,ceil,27);pbLabGlass(m,13.5f,17.f,14.8f,17.2f,f,ceil,27);
 wall(m,10.5f,17.f,13.5f,17.2f,f+2.65f,ceil,27);frame(10.5f,13.5f,17.1f);
 {Door d{10.5f,13.5f,17.1f,0};d.requireState=cycle;m.doors.push_back(d);}       // 5: clean door, opens on decon_cycle
 // Air-shower nozzle columns.
 for(float y:{12.2f,13.5f,14.8f,16.1f}){wall(m,14.45f,y-.15f,14.6f,y+.15f,f+.25f,f+2.5f,20);
  for(float z=f+.5f;z<f+2.4f;z+=.35f)m.pipes.push_back({{14.45f,y},{14.37f,y},z,.04f,z,6});}
 for(float y:{12.1f,16.6f}){wall(m,9.4f,y-.15f,9.55f,y+.15f,f+.25f,f+2.5f,20);
  for(float z=f+.5f;z<f+2.4f;z+=.35f)m.pipes.push_back({{9.55f,y},{9.63f,y},z,.04f,z,6});}
 for(float y=12.f;y<16.8f;y+=.3f)wall(m,10.6f,y,13.4f,y+.06f,ceil-.06f,ceil,8);
 // The exposure monitor: a portal frame with its readouts.
 for(float x:{10.4f,13.25f})wall(m,x,14.25f,x+.35f,14.55f,f,f+2.45f,20);
 wall(m,10.4f,14.25f,13.6f,14.55f,f+2.3f,f+2.6f,20);
 for(float x:{10.4f,13.25f})wall(m,x+.06f,14.24f,x+.29f,14.25f,f+1.f,f+1.9f,14);
 pbFix(m,model::lab_status_display,12.f,14.22f,f+2.34f,.69f,.06f,.2f,0,false);
 pbFix(m,model::lab_status_display,12.f,14.58f,f+2.34f,.69f,.06f,.2f,kPi,false);
 for(float x:{11.6f,12.15f})floorLine(m,x,14.3f,x+.25f,14.5f,f,20);
 terminal(m,{9.95f,13.4f},"DECON 1 / CYCLE CONTROL","LAST CYCLE 0412 ABORTED 02:20. SUPPLY RESTORED.","E / START CYCLE 0413. STAND IN THE MONITOR FRAME.",0,start);m.terminals.back().yaw=pbE;
 // The technician sealed in at 02:20: he got as far as the clean door.
 pbBody(m,"ere_decon_clock",ActorVisual::Worker,{11.3f,16.3f},f,.4f);
 pbBlood(m,10.6f,15.7f,12.4f,16.85f,f);pbBlood(m,12.3f,16.2f,13.f,16.6f,f);
 for(auto [x,z]:{std::pair{9.75f,1.25f},{9.9f,1.55f},{10.15f,1.3f},{9.65f,.95f}})wall(m,x,16.97f,x+.08f,16.99f,f+z,f+z+.09f,16);
 wall(m,13.7f,16.97f,14.f,16.99f,f+1.35f,f+1.6f,16);
 // Light: steady white until the cycle seals the chamber; then the cycle tubes pulse.
 for(float y:{12.6f,15.9f})m.lights.push_back(pbSwitched(lamp({12.f,y},ceil-.18f,1.f,6.f,6800,.03f),sealed,true));
 for(float y:{13.5f,16.1f}){auto l=pbSwitched(lamp({14.58f,y},f+2.75f,1.3f,6.f,12000,-.4f,WorldLightMount::Wall,kPi),sealed);l.period=.9f;l.onTime=.55f;l.phase=y*.1f;m.lights.push_back(l);}
 m.lights.push_back(pbSwitched(lamp({12.f,11.6f},f+2.8f,.8f,4.f,1000,-.5f,WorldLightMount::Wall,kPi*.5f),sealed));
 m.lights.push_back(pbSwitched(lamp({12.f,17.2f},f+2.8f,.8f,4.f,5000,.5f,WorldLightMount::Wall,kPi*.5f),cycle));
 // --- The decon plant, west -----------------------------------------------------------
 panel(1.f,17.f,9.2f,17.2f,28);
 vessel(m,{2.4f,9.4f},.75f,f,f+2.7f,5);
 m.pipes.push_back({{2.4f,9.4f},{2.4f,9.4f},f+2.7f,.5f,f+2.9f,5});
 pipeRoute(m,{{2.4f,9.4f,f+2.9f},{2.4f,9.4f,f+3.1f},{9.2f,9.4f,f+3.1f}},.07f,4,ceil);
 pipeRoute(m,{{3.15f,10.f,f+2.2f},{9.2f,10.f,f+2.2f}},.06f,0,ceil);
 for(float y:{8.8f,10.75f}){pbFix(m,model::pwr_air_handler,8.f,y,f,.81f,1.64f,2.62f,0,true);
  wall(m,8.3f,y-.3f,9.2f,y+.3f,f+2.62f,f+3.1f,20);wall(m,7.7f,y-.3f,8.3f,y+.3f,f+2.62f,f+3.1f,20);}
 pbFix(m,model::pwr_control_cabinet,1.25f,12.5f,f,.8f,.4f,1.6f,pbE,true);
 wall(m,4.2f,15.2f,6.2f,16.8f,f,f+2.2f,20);wall(m,4.15f,15.15f,6.25f,16.85f,f+2.2f,f+2.3f,12);
 wall(m,4.9f,15.6f,5.5f,16.4f,f+2.3f,ceil,20);
 for(auto [x,y]:{std::pair{1.6f,15.7f},{2.3f,15.95f},{1.6f,16.5f}})pbFix(m,model::ind_barrel,x,y,f,.53f,.56f,.95f,x+y,true);
 pbStand(m,3.f,14.6f,3.9f,15.3f,f,.6f);
 pbFix(m,model::pwr_circulation_pump,3.45f,14.95f,f+.6f,.84f,.5f,.39f,0,false);
 m.lights.push_back(lamp({5.f,10.f},ceil-.2f,1.f,7.f,4000,.1f));
 m.lights.push_back(lamp({5.6f,13.6f},ceil-.2f,.9f,6.f,4000,.1f));
 {auto l=lamp({1.02f,14.f},f+2.4f,.5f,4.f,1300,-.3f,WorldLightMount::Wall,0);l.flicker=.2f;m.lights.push_back(l);}
 // --- Clean suits, east (clean side) ---------------------------------------------------
 panel(14.8f,17.f,18.f,17.2f,28);panel(20.f,17.f,23.f,17.2f,28);wall(m,18.f,17.f,20.f,17.2f,f+2.65f,ceil,28);frame(18.f,20.f,17.1f);
 for(int i=0;i<8;++i)pbLockers(m,22.5f,8.5f+i*.9f,f,pbW);
 for(float y:{9.4f,11.6f})pbFix(m,model::metal_shelf,15.1f,y,f,1.8f,.5f,1.8f,pbE,true);
 m.pipes.push_back({{17.6f,8.f},{17.6f,14.6f},f+2.05f,.03f,f+2.05f,5});
 for(float y:{8.f,14.6f}){m.pipes.push_back({{17.6f,y},{17.6f,y},f,.03f,f+2.05f,5});wall(m,17.54f,y-.06f,17.66f,y+.06f,f,f+2.05f,CollisionProxyMaterial);}
 for(float y=8.6f;y<14.2f;y+=.6f){wall(m,17.38f,y-.1f,17.82f,y+.1f,f+.75f,f+1.85f,18);wall(m,17.48f,y-.06f,17.72f,y+.06f,f+1.85f,f+2.02f,18);}
 pbFix(m,model::med_supply_cabinet,16.4f,16.55f,f,1.1f,.44f,1.7f,pbN,true);
 bench(m,19.f,15.6f,f);
 for(float y:{10.f,14.5f})m.lights.push_back(lamp({19.f,y},ceil-.16f,.9f,6.f,6000,.04f));
 // --- Stage 5: gowning, and the way out to the court -----------------------------------
 wall(m,10.f,23.f,14.f,23.3f,f+2.65f,ceil,28);
 wall(m,10.f,21.6f,14.f,22.95f,f,f+.012f,23);
 floorLine(m,1.f,17.6f,23.f,17.72f,f,20);
 wall(m,2.f,18.8f,8.f,19.2f,f+.4f,f+.5f,20);for(float x:{2.2f,5.f,7.7f})wall(m,x,18.85f,x+.1f,19.15f,f,f+.4f,8);
 for(float x:{3.f,6.f})floorLine(m,x,19.4f,x+1.2f,19.52f,f,13);
 for(int i=0;i<8;++i)pbLockers(m,1.5f+i*.9f,22.55f,f,pbN);
 pbFix(m,model::med_washroom_mirror,1.03f,20.4f,f+1.1f,1.5f,.04f,1.f,pbE,false);
 terminal(m,{16.5f,18.2f},"DECON 1 / CLEAN-SIDE LOG","OUT 01:58: 6 STAFF TO THE COURT. IN SINCE 02:09: NONE.","03:05 CLEANUP 2 SIGNED IN: HOLD DECON 1. NOBODY GOES UP.");m.terminals.back().yaw=pbS;
 pbFix(m,model::water_cooler,22.5f,18.1f,f,.35f,.35f,1.1f,pbW,true);
 pbFix(m,model::lab_potted_plant,22.4f,22.4f,f,.5f,.58f,.93f,0,false);
 bench(m,20.5f,22.3f,f);
 for(Vec2 l:{Vec2{5.f,20.2f},Vec2{12.f,19.6f},Vec2{12.f,22.2f},Vec2{18.6f,20.4f}})m.lights.push_back(lamp(l,ceil-.16f,.95f,6.5f,6200,.04f));
 // The door watcher: idle at his mark until the cycle releases the clean door.
 pbGuard(m,"ere_decon_clock","ere_decon_guard",ActorVisual::Cleanup,GuardSquadRole::Default,nullptr,
  {{{12.4f,20.9f},f,4000,kPi},{{16.6f,20.4f},f,3500,kPi*.75f},{{12.f,22.f},f,5000,kPi}},13,.6f,1,1.3f,cycle);
 // --- The cycle -------------------------------------------------------------------------
 pbSequence(m,"ere_decon_ms","decon_cycle",26000,22000);
 const Vec2 console{9.95f,13.4f},guard{12.4f,20.9f};
 cue(m,"ere_decon_ms",0,Sound::Door,{12.f,11.5f},"DECON 1: CHAMBER SEALED. CYCLE 0413.",.9f,f+1.5f);
 cue(m,"ere_decon_ms",400,Sound::LiftMotor,{8.f,9.8f},"",.9f,f+2.2f);
 cue(m,"ere_decon_ms",1500,Sound::Machine,console,"STAGE 3: AIR SHOWER. ARMS UP. TURN SLOWLY.",.8f,f+2.f);
 cue(m,"ere_decon_ms",4500,Sound::Empty,guard,"RADIO: DECON ONE IS CYCLING. SOMEBODY'S INSIDE.",.5f,f+1.6f);
 cue(m,"ere_decon_ms",7200,Sound::Empty,guard,"RADIO: COPY. LET HIM FINISH. HE COMES OUT TO YOU.",.5f,f+1.6f);
 cue(m,"ere_decon_ms",9500,Sound::Machine,console,"STAGE 4: MONITOR. HOLD STILL IN THE FRAME.",.8f,f+2.f);
 cue(m,"ere_decon_ms",12500,Sound::Pickup,console,"MONITOR: EXPOSURE 0.4. WITHIN SHIFT LIMIT.",.6f,f+2.3f);
 cue(m,"ere_decon_ms",15200,Sound::Empty,guard,"RADIO: HE'S NOT ON THE ROSTER. TAKE HIM AT THE DOOR.",.5f,f+1.6f);
 cue(m,"ere_decon_ms",18500,Sound::LiftMotor,{8.f,9.8f},"",.6f,f+2.2f);
 cue(m,"ere_decon_ms",22000,Sound::Door,{12.f,17.1f},"CYCLE 0413 COMPLETE. CLEAN SIDE RELEASED.",.9f,f+1.5f);
 {auto e=pbEvent("ere_decon_seal_go",0,0,24,24,-100,100,start);e.actions={pbSet(sealed),pbDoor(ScriptAction::Type::CloseDoor,4),pbSet(stateId("ere_decon_ms")),pbShake(.08f)};m.scriptEvents.push_back(e);}
 {auto e=pbEvent("ere_decon_release",0,0,24,24,-100,100,cycle);e.actions={pbSet(sealed,0),pbDoor(ScriptAction::Type::OpenDoor,5),pbSave()};m.scriptEvents.push_back(e);}
 // --- Rewards ----------------------------------------------------------------------------
 m.pickupSpawns={{{21.2f,5.8f},PickupKind::Ammo,f},{{2.f,13.8f},PickupKind::Health,f},{{19.4f,9.f},PickupKind::Ammo,f}};
 m.clutterSpawns={{0,{7.f,4.4f},f},{2,{3.f,16.4f},f},{1,{15.9f,5.6f},f}};
 zone(m,"DECONTAMINATION / CHAMBER",9.4f,11.6f,14.6f,17.f);
 zone(m,"DECONTAMINATION / STAGE 2 SHOWER",9.4f,7.6f,14.6f,11.4f);
 zone(m,"DECONTAMINATION / OPERATOR BOOTH",15.2f,1.f,23.f,7.4f);
 zone(m,"DECONTAMINATION / DECON PLANT",1.f,7.6f,9.2f,17.f);
 zone(m,"DECONTAMINATION / CLEAN SUITS",14.8f,7.6f,23.f,17.f);
 zone(m,"DECONTAMINATION / STAGE 5 GOWNING",1.f,17.2f,23.f,24.f);
 zone(m,"DECONTAMINATION / STAGE 1 DISROBE",0,0,24,7.4f);
 zone(m,"DECONTAMINATION",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// ENERGY RESEARCH / FORECOURT (manifest 44). The first time the plant shows
// what it was for. Out of the cleanroom into a hall twenty-four metres tall:
// four concrete piers, the company's name in steel letters, and on a plinth
// at the centre THE FIRST SAMPLE, the thing the whole company grew around,
// lit like a relic. Visitors' benches, the reception desk, the film. The
// Cleanup Force has turned it into a firing lane toward the great door.
std::shared_ptr<const Map> makeEnergyResearchForecourt(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[66];m.definition=CampaignChunks[66];m.definition.ambient=.3f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceStone;m.ceilingSurface=SurfaceConcrete;
 const float f=-16,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,-16.f,true,0});openEdge(m,{'S',2.f,22.f,-16.f,false,0});
 pbSeamHeader(m,'N',10.f,14.f,f,2.65f,15);
 // --- The entrance from Decontamination: a low portal into the tall hall -------------------------
 for(float x:{8.6f,14.f})wall(m,x,1.f,x+1.4f,2.2f,f,f+3.6f,15);
 wall(m,8.6f,1.f,15.4f,2.2f,f+2.65f,f+3.6f,15);
 // --- Four piers and the hall's ribs ----------------------------------------------------------
 for(Vec2 c:{Vec2{5.f,7.f},Vec2{19.f,7.f},Vec2{5.f,17.f},Vec2{19.f,17.f}}){
  wall(m,c.x-.8f,c.y-.8f,c.x+.8f,c.y+.8f,f,roof,15);
  wall(m,c.x-.95f,c.y-.95f,c.x+.95f,c.y+.95f,f,f+.6f,33);
  wall(m,c.x-.86f,c.y-.86f,c.x+.86f,c.y+.86f,f+6.f,f+6.25f,20);
  m.lights.push_back(lamp({c.x,c.y-1.05f},f+.7f,1.6f,8.f,3000,0,WorldLightMount::Bare));pbLampHead(m,c.x,c.y-1.05f,f+.55f);}
 for(float y:{7.f,17.f})wall(m,1.f,y-.3f,23.f,y+.3f,roof-1.2f,roof,15);
 for(float x:{5.f,19.f})wall(m,x-.3f,1.f,x+.3f,24.f,roof-1.2f,roof,15);
 // --- The company's name, high on the north wall ---------------------------------------------
 wall(m,3.f,1.f,21.f,1.3f,f+9.f,f+11.6f,20);
 pbSign(m,12.f,1.355f,f+9.4f,"DEPTHWORKS ENERGY RESEARCH","POWER FROM BELOW",16.f,pbS,pbBlue);
 m.signs.back().height=2.f;
 // --- THE FIRST SAMPLE on its plinth ---------------------------------------------------------
 wall(m,10.4f,10.4f,13.6f,13.6f,f,f+.9f,33);wall(m,10.7f,10.7f,13.3f,13.3f,f+.9f,f+1.05f,20);
 pbFix(m,model::lab_anomaly_relic,12.f,12.f,f+1.05f,1.5f,1.2f,1.2f,kPi*.25f,false);
 wall(m,10.4f,13.6f,13.6f,13.66f,f+.25f,f+.75f,20);
 pbSign(m,12.f,13.715f,f+.2f,"THE FIRST SAMPLE","RECOVERED 1979 / LEVEL 11 / THE REASON WE ARE HERE",3.f,pbS,pbAmber);
 m.signs.back().height=.5f;
 for(Vec2 l:{Vec2{12.f,10.f},Vec2{12.f,14.f}})m.lights.push_back(lamp(l,f+.35f,1.4f,5.f,6500,.1f,WorldLightMount::Bare));
 m.lights.push_back(lamp({12.f,12.f},roof-1.5f,6.f,24.f,5600,0));
 // --- Visitors: benches, the reception desk and the welcome film ------------------------------
 for(float y:{5.f,9.f})for(float x:{1.6f,22.4f})pbFix(m,model::bench,x,y,f,1.86f,.44f,.63f,x<12?pbE:pbW,true);
 pbFix(m,model::lab_exec_desk,7.8f,3.6f,f,2.2f,1.1f,.9f,pbS,true);
 pbFix(m,model::lab_potted_plant,9.f,3.2f,f,.37f,.43f,.69f,0,false);pbFix(m,model::lab_potted_plant,3.f,3.f,f,.37f,.43f,.69f,0,false);
 terminal(m,{16.2f,3.6f},"VISITOR CENTRE / WELCOME FILM","\"DEEP BENEATH YOUR FEET, A NEW KIND OF POWER WAITS.\" (03:12 RUNTIME)","\"DEPTHWORKS: WE GO DOWN SO THE WORLD CAN GO FORWARD.\"");
 m.terminals.back().yaw=pbS;
 terminal(m,{7.8f,4.6f},"RECEPTION / VISITOR LOG","LAST VISITOR: BOARD TOUR, 01:30. 6 GUESTS. SIGNED OUT: 0.","NOTE: TOUR HELD IN RESEARCH ADMINISTRATION BY ORDER 7.");
 m.terminals.back().yaw=pbS;
 // --- The firing lane: barriers the Cleanup Force dragged across the hall -------------------------
 for(float x:{3.f,9.f,15.f,21.f})pbFix(m,model::ind_barrier_wall,x,20.6f,f,2.96f,.4f,2.05f,0,true);
 for(Vec2 r:{Vec2{8.f,14.6f},Vec2{16.f,14.6f}})pbFix(m,model::road_block,r.x,r.y,f,1.6f,.5f,.7f,.3f,true);
 pbBody(m,"ere_fore_clock",ActorVisual::Worker,{14.4f,16.8f},f,2.2f);pbBlood(m,13.8f,16.2f,15.2f,17.4f,f);
 machineryClock(m,"ere_fore_clock",60000);
 // A forward pair holding the hall; the squad proper waits at the door.
 pbGuard(m,"ere_fore_clock","ere_fore_0",ActorVisual::Cleanup,GuardSquadRole::Assault,"ere_finale",
  {{{6.5f,20.f},f,3500,pbN},{{6.5f,14.f},f,3000,pbN}},14,.5f,1,1.5f);
 pbGuard(m,"ere_fore_clock","ere_fore_1",ActorVisual::CleanupRusher,GuardSquadRole::Rusher,"ere_finale",
  {{{17.5f,20.f},f,3500,pbN},{{17.5f,13.f},f,3000,pbN}},12,.55f,1,1.7f);
 m.pickupSpawns.push_back({{2.4f,12.f},PickupKind::Ammo,f});
 m.pickupSpawns.push_back({{21.6f,12.f},PickupKind::Health,f});
 // --- Light: high cold lamps, warm uplights on the piers -------------------------------------------
 for(Vec2 l:{Vec2{12.f,4.f},Vec2{3.f,12.f},Vec2{21.f,12.f},Vec2{12.f,20.f}})m.lights.push_back(lamp(l,roof-1.5f,4.f,20.f,5200,.05f));
 zone(m,"ENERGY RESEARCH / THE FIRST SAMPLE",9.f,9.f,15.f,15.f);
 zone(m,"ENERGY RESEARCH / FORECOURT",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// ENERGY RESEARCH / GREAT DOOR (manifest 44). The chapter's end: a stair as
// wide as the hall climbs four metres to a podium and the great blast door
// of the research levels. The Cleanup Force's last squad on this side holds
// the podium behind sandbags. When they are down, the door console answers.
std::shared_ptr<const Map> makeEnergyResearchGreatDoor(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[67];m.definition=CampaignChunks[67];m.definition.ambient=.3f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceStone;m.ceilingSurface=SurfaceConcrete;
 const float f=-16,top=-12,roof=m.definition.ceiling;
 const StateId door=stateId("ere_great_door"),clear=stateId("ere_finale_clear");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',2.f,22.f,-16.f,false,0});openEdge(m,{'S',9.f,15.f,-12.f,true,door});
 // --- The great stair and the podium ----------------------------------------------------------
 m.stairs.push_back({4.f,10.4f,20.f,16.f,f,top,20,true,true});
 wall(m,1.f,16.f,23.f,23.f,f,top,15);
 floorLine(m,4.f,15.85f,20.f,16.f,top,13);
 for(float x:{3.6f,20.f})wall(m,x,10.4f,x+.4f,16.f,f,top+.9f,33);
 // Lamps in the stair's cheek walls and along the podium edge: the climb is lit.
 for(float y:{11.2f,13.2f,15.2f})for(float x:{4.05f,19.95f}){m.lights.push_back(lamp({x,y},f+(y-10.4f)*.71f+.9f,1.4f,6.f,2700,-.05f,WorldLightMount::Wall,x<12?0.f:kPi));}
 for(float x:{6.f,10.f,14.f,18.f}){m.lights.push_back(lamp({x,16.25f},top+.15f,1.2f,5.f,3000,0,WorldLightMount::Bare));pbLampHead(m,x,16.3f,top);}
 // --- Piers carried on from the forecourt ---------------------------------------------------
 for(Vec2 c:{Vec2{5.f,6.f},Vec2{19.f,6.f}}){wall(m,c.x-.8f,c.y-.8f,c.x+.8f,c.y+.8f,f,roof,15);wall(m,c.x-.95f,c.y-.95f,c.x+.95f,c.y+.95f,f,f+.6f,33);}
 for(float x:{5.f,19.f})wall(m,x-.3f,0.f,x+.3f,23.f,roof-1.2f,roof,15);
 // --- The great door's frame and the lettering over it --------------------------------------------
 for(float x:{7.f,15.f})wall(m,x,21.6f,x+2.f,23.f,top,top+9.f,7);
 wall(m,7.f,21.6f,17.f,23.f,top+2.65f,top+9.f,7);
 for(float z=top+3.f;z<top+8.6f;z+=.9f)wall(m,9.f,21.55f,15.f,21.6f,z,z+.1f,13);
 pbSign(m,12.f,21.545f,top+3.4f,"GREAT DOOR","OPENS ON LOCAL CONTROL ONLY",3.2f,pbN,pbRed);
 // --- The squad's position on the podium --------------------------------------------------------
 for(float x:{3.f,15.f}){wall(m,x,17.2f,x+6.f,17.8f,top,top+1.1f,23);wall(m,x+.02f,17.22f,x+5.98f,17.78f,top,top+1.08f,CollisionProxyMaterial);}
 for(float x:{2.f,21.f}){wall(m,x,18.f,x+1.f,21.f,top,top+1.1f,23);}
 for(Vec2 c:{Vec2{4.f,20.f},Vec2{20.f,20.f}})pbFix(m,model::ind_crate,c.x,c.y,top,.9f,.9f,.9f,.2f,true);
 machineryClock(m,"ere_door_clock",60000);
 pbGuard(m,"ere_door_clock","ere_door_0",ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,"ere_finale",
  {{{12.f,19.f},top,6000,pbN},{{9.f,19.f},top,4000,pbN}},16,.45f,1,1.2f,0,0,true,0,160);
 pbGuard(m,"ere_door_clock","ere_door_1",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"ere_finale",
  {{{4.5f,18.6f},top,5000,pbN},{{6.5f,18.6f},top,4000,pbN}},18,.45f,2,1.2f);
 pbGuard(m,"ere_door_clock","ere_door_2",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"ere_finale",
  {{{19.5f,18.6f},top,5000,pbN},{{17.5f,18.6f},top,4000,pbN}},18,.45f,2,1.2f);
 pbGuard(m,"ere_door_clock","ere_door_3",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"ere_finale",
  {{{12.f,21.f},top,5000,pbN},{{14.f,20.6f},top,4000,pbN}},14,.5f,2,1.3f);
 pbGuard(m,"ere_door_clock","ere_door_4",ActorVisual::Cleanup,GuardSquadRole::Assault,"ere_finale",
  {{{8.f,8.f},f,4000,pbS},{{16.f,8.f},f,4000,pbS}},14,.5f,1,1.5f,0,0,false,2);
 m.pickupSpawns.push_back({{2.f,4.f},PickupKind::Ammo,f});
 m.pickupSpawns.push_back({{22.f,4.f},PickupKind::Health,f});
 m.pickupSpawns.push_back({{6.4f,19.4f},PickupKind::Health,top});
 // --- The door console: it answers once the squad is down --------------------------------------
 {auto e=pbEvent("ere_finale_cleared",0,0,24,24,-100,100);e.requireEnemiesClear=true;e.actions={pbSet(clear),pbSave()};m.scriptEvents.push_back(e);}
 terminal(m,{18.f,20.6f},"GREAT DOOR / LOCAL CONTROL","E / OPEN THE GREAT DOOR TO ENERGY RESEARCH.","LOCKED OUT WHILE THE CLEANUP DETAIL HOLDS THE PODIUM.",top-f,door,clear);
 m.terminals.back().yaw=pbN;
 {auto e=pbEvent("ere_great_door_opens",0,0,24,24,-100,100,door);e.actions={pbShake(.3f),pbSound(Sound::LiftCreak,.8f),pbSave()};m.scriptEvents.push_back(e);}
 // --- Light: the door lit from below, the hall from above ----------------------------------------
 for(float x:{9.5f,14.5f}){m.lights.push_back(lamp({x,20.6f},top+.4f,2.2f,9.f,2400,-.1f,WorldLightMount::Bare));pbLampHead(m,x,20.6f,top+.25f);}
 for(Vec2 l:{Vec2{12.f,4.f},Vec2{4.f,12.f},Vec2{20.f,12.f},Vec2{12.f,18.f}})m.lights.push_back(lamp(l,roof-1.5f,4.f,20.f,5200,.05f));
 m.lights.push_back(pbSwitched(lamp({12.f,21.f},top+2.4f,3.f,10.f,1200,-.4f,WorldLightMount::Bare),door,true));
 m.lights.push_back(pbSwitched(lamp({12.f,21.f},top+2.4f,3.f,10.f,6500,.3f,WorldLightMount::Bare),door));
 zone(m,"ENERGY RESEARCH / GREAT DOOR",0,16,24,24,top-.5f,1000);
 zone(m,"ENERGY RESEARCH / FORECOURT",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
}
void buildChapterPerimeterB(CampaignMapTable& maps){
 maps[63]=makeRestrictedUtilities();
 maps[64]=makeResearchReceiving();
 maps[65]=makeDecontamination();
 maps[66]=makeEnergyResearchForecourt();
 maps[67]=makeEnergyResearchGreatDoor();
}
}

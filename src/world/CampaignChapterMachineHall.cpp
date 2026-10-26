// MACHINE HALL AND AIR (chapter 1, manifest 25-27): chunks 41-46.
//
// Feeder 9 runs from Load Control into the Machine Hall: the plant's turbine
// house, 48 x 48 m and 34 m to the roof, four chunks that stay resident
// together (41 north-west, 42 north-east, 43 south-west, 44 south-east).
// Three turbine-generator sets stand on their concrete tables: Unit 1 and
// Unit 2 tripped at 23:58, Unit 3 opened for overhaul with its rotor out.
// Two overhead cranes ride runways on the side walls; Crane 1 dropped the
// Unit 2 end shield through the north gallery when its hoist brake failed.
// The way on is up: the south catwalk at -22 leads into the Ventilation Works,
// but its fire damper closed when the vent supply died. Switchboard 9, in the
// exciter bay's unit control room across the broken gallery, restores it.
// Then the Ventilation Works (fans, scrubbers, a duct you walk through) and
// Air Handling: crawlspaces, the quietest map so far.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <vector>
namespace retro {
namespace {
using namespace kit;

// --- Shared heights ------------------------------------------------------------------------
constexpr float mhFloor=-28.f,mhTable=-25.f,mhCatwalk=-22.f,mhRoof=6.f;
constexpr float mhRail=-19.6f;        // crane rail top
constexpr float mhGirderTop=-18.2f;   // crane bridge girder top
constexpr float mhLampRig=-15.4f;     // lighting girder underside

// Absolute-base fixture: the kit stores fixture bases relative to layer 0.
void mhFix(Map& m,int model,float x,float y,float base,float w,float d,float h,float yaw=0,bool solid=true){
 m.fixtures.push_back({model,{x,y},base-m.layers.front().elevation,w,d,h,yaw,solid});
}
void mhLamp(Map& m,Vec2 p,float z,float intensity,float range,float kelvin,float tint=0,WorldLightMount mount=WorldLightMount::Ceiling,float yaw=0,StateId state=0,int value=1,bool invert=false,float flicker=0){
 WorldLight l=lamp(p,z,intensity,range,kelvin,tint,mount,yaw);l.enabledState=state;l.enabledValue=value;l.invert=invert;l.flicker=flicker;m.lights.push_back(l);
}
// A horizontal run near a floor gets its own collision so nothing walks through it.
void mhLowPipe(Map& m,Vec2 a,Vec2 b,float z,float r,int paint,float floor){
 m.pipes.push_back({a,b,z,r,z,paint});
 m.structures.push_back({std::min(a.x,b.x)-r,std::min(a.y,b.y)-r,std::max(a.x,b.x)+r,std::max(a.y,b.y)+r,floor,z+r,false,6});
}
// A vertical riser standing on a floor, with its collision core.
void mhRiser(Map& m,Vec2 c,float bottom,float top,float r,int paint){
 m.pipes.push_back({c,c,bottom,r,top,paint});
 m.structures.push_back({c.x-r,c.y-r,c.x+r,c.y+r,bottom,top,false,6});
}
// A glazed run: sill, glass and head between z and z+2.7.
void mhGlazed(Map& m,float x1,float y1,float x2,float y2,float z){
 wall(m,x1,y1,x2,y2,z,z+.95f,3);wall(m,x1,y1,x2,y2,z+.95f,z+2.35f,17);wall(m,x1,y1,x2,y2,z+2.35f,z+2.7f,3);
 bool alongX=x2-x1>y2-y1;float len=alongX?x2-x1:y2-y1;int bays=std::max(1,int(len/1.3f+.5f));
 for(int i=1;i<bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,t-.04f,y1-.01f,t+.04f,y2+.01f,z+.95f,z+2.35f,12);else wall(m,x1-.01f,t-.04f,x2+.01f,t+.04f,z+.95f,z+2.35f,12);}
}
// Hard hats and boots of a crew that never signed out.
void mhHelmet(Map& m,float x,float y,float floor,float yaw){mhFix(m,model::mine_hard_hat,x,y,floor,.28f,.35f,.15f,yaw,false);}

// ===========================================================================================
// THE MACHINE HALL, in hall coordinates (x, y 0..48). Chunk 41 holds x<24,
// y<24; 42 x>=24, y<24; 43 x<24, y>=24; 44 x>=24, y>=24.
// ===========================================================================================
struct MhDeck {float z;int x1,y1,x2,y2;};
struct MhHall {Map h;std::vector<MhDeck> decks;};
void mhDeck(MhHall& hall,float z,int x1,int y1,int x2,int y2){hall.decks.push_back({z,x1,y1,x2,y2});}
// Steel plate on a walkway deck, so galleries read as steel over the concrete floor.
void mhPlate(Map& m,float x1,float y1,float x2,float y2,float z){wall(m,x1,y1,x2,y2,z,z+.012f,2);}

// A built-up crane column against a side wall (west when dir>0, east when dir<0),
// the runway bracket at its head and the lighter roof column above.
void mhCraneColumn(Map& m,float wallX,float y,float dir){
 auto xs=[&](float a,float b){return std::pair{std::min(wallX+dir*a,wallX+dir*b),std::max(wallX+dir*a,wallX+dir*b)};};
 auto [c1,c2]=xs(0,.7f);
 // Flanges and web of the lower column, with a solid core for collision.
 wall(m,c1,y-.36f,c2,y-.3f,mhFloor,mhRail-.8f,12);wall(m,c1,y+.3f,c2,y+.36f,mhFloor,mhRail-.8f,12);
 {auto [w1,w2]=xs(.28f,.42f);wall(m,w1,y-.3f,w2,y+.3f,mhFloor,mhRail-.8f,12);}
 {auto [f1,f2]=xs(.62f,.7f);wall(m,f1,y-.36f,f2,y+.36f,mhFloor,mhRail-.8f,12);}
 m.structures.push_back({c1,y-.36f,c2,y+.36f,mhFloor,mhRail-.8f,false,CollisionProxyMaterial});
 {auto [b1,b2]=xs(-.0f,.9f);wall(m,b1,y-.5f,b2,y+.5f,mhFloor,mhFloor+.06f,8);}
 // Stiffener bands every 3 m and the bracket (corbel) that carries the runway.
 for(float z=mhFloor+3.f;z<mhRail-1.f;z+=3.f){auto [s1,s2]=xs(0,.74f);wall(m,s1,y-.4f,s2,y+.4f,z,z+.08f,12);}
 {auto [k1,k2]=xs(0,1.25f);wall(m,k1,y-.32f,k2,y+.32f,mhRail-1.3f,mhRail-.8f,12);}
 {auto [k1,k2]=xs(.7f,1.25f);wall(m,k1,y-.28f,k2,y+.28f,mhRail-1.7f,mhRail-1.3f,12);}
 // Upper column to the roof truss bearing.
 {auto [u1,u2]=xs(0,.42f);wall(m,u1,y-.22f,u2,y+.22f,mhRail-.8f,2.f,12);}
}
// One crane: two box girders across the hall at y..y+2, end trucks on the
// rails, a trolley at trolleyX. The cab, cables and hook are added by the caller.
void mhCrane(Map& m,float y,float trolleyX){
 const float lo=mhGirderTop-1.2f;
 for(float g:{y+.1f,y+1.3f}){
  wall(m,2.5f,g,45.5f,g+.6f,lo,mhGirderTop,18);
  // Diaphragm stiffeners on the girder faces.
  for(float x=4.f;x<45;x+=3.f)for(float face:{g-.03f,g+.6f})wall(m,x,face,x+.08f,face+.03f,lo+.05f,mhGirderTop-.05f,12);
 }
 // Footwalk and handrail along the north girder.
 wall(m,2.5f,y-.6f,45.5f,y+.1f,mhGirderTop-.08f,mhGirderTop,2);
 for(int i=0;i<=16;++i){float x=3.f+i*2.625f;m.pipes.push_back({{x,y-.55f},{x,y-.55f},mhGirderTop,.02f,mhGirderTop+1.05f,3});}
 m.pipes.push_back({{3.f,y-.55f},{45.f,y-.55f},mhGirderTop+1.05f,.025f,mhGirderTop+1.05f,3});
 // End trucks riding the two rails, with their wheels.
 for(float x:{1.55f,45.15f}){
  wall(m,x,y-.6f,x+1.3f,y+2.6f,mhRail+.5f,mhRail+1.f,18);
  wall(m,x-.02f,y-.62f,x+1.32f,y+2.62f,mhRail+.92f,mhRail+1.02f,12);
  for(float yy:{y-.2f,y+2.2f}){m.pipes.push_back({{x+.35f,yy},{x+.95f,yy},mhRail+.3f,.3f,mhRail+.3f,6});wall(m,x+.2f,yy-.35f,x+1.1f,yy+.35f,mhRail+.45f,mhRail+.5f,12);}
 }
 // Trolley: frame, hoist drum and motor.
 wall(m,trolleyX-1.5f,y-.1f,trolleyX+1.5f,y+2.1f,mhGirderTop,mhGirderTop+.35f,18);
 m.pipes.push_back({{trolleyX-1.1f,y+1.f},{trolleyX+.7f,y+1.f},mhGirderTop+.85f,.48f,mhGirderTop+.85f,6});
 for(float x:{trolleyX-1.2f,trolleyX+.8f})wall(m,x,y+.2f,x+.12f,y+1.8f,mhGirderTop+.35f,mhGirderTop+1.45f,12);
 wall(m,trolleyX+.95f,y+.4f,trolleyX+1.45f,y+1.6f,mhGirderTop+.35f,mhGirderTop+1.1f,8);
}
// The lighting rig: an E-W girder on wall brackets carrying sodium high bays.
// Each row strikes on its own contactor (state), A in the north to D in the south.
void mhLightingGirder(Map& m,float y,StateId row,float failing=-1){
 wall(m,1.f,y-.18f,47.f,y+.18f,mhLampRig,mhLampRig+.55f,12);
 for(float x=3.f;x<46;x+=4.f)wall(m,x,y-.2f,x+.06f,y+.2f,mhLampRig+.05f,mhLampRig+.5f,8);
 for(float x:{1.f,46.4f})wall(m,x,y-.3f,x+.6f,y+.3f,mhLampRig-.6f,mhLampRig,12);
 for(float x:{4.5f,12.f,19.5f,28.5f,36.f,43.5f}){
  // Each high bay hangs in a ballast box clamped under the girder.
  wall(m,x-.3f,y-.22f,x+.3f,y+.22f,mhLampRig-.42f,mhLampRig,8);
  mhLamp(m,{x,y},mhLampRig-.55f,5.2f,21.f,std::fabs(x-failing)<.1f?2300:1900,-.15f,WorldLightMount::Ceiling,0,row,1,false,std::fabs(x-failing)<.1f?.35f:0.f);
 }
}
// A red emergency bulkhead on a wall: lit until the high bays are all back.
void mhRed(Map& m,Vec2 p,float z,float yaw,StateId off,float intensity=.9f,float range=6.5f){
 mhLamp(m,p,z,intensity,range,1150,-.4f,WorldLightMount::Wall,yaw,off,1,true);
}
// A Worker lying where he fell (death clip held at its end), on a clock that
// exists in this map.
void mhBody(Map& m,const char* clock,Vec2 p,float z,float yaw){
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,p,z,yaw,4,1},{60000,p,z,yaw,4,1}});
}
// A blood pool: overlapping thin decals, darkest at the centre.
void mhBlood(Map& m,Vec2 c,float floor,float r){
 wall(m,c.x-r,c.y-r*.6f,c.x+r,c.y+r*.6f,floor+.003f,floor+.006f,16);
 wall(m,c.x-r*.6f,c.y-r,c.x+r*.7f,c.y+r*.8f,floor+.004f,floor+.007f,16);
}
// One-shot sequence whose timer only starts when a state sets it (off-map box).
void mhOneShot(Map& m,const char* timer,const char* finish,int duration,int finishAt){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=-3;s.y1=-3;s.x2=-2;s.y2=-2;
 s.durationMs=duration;s.finishAtMs=finishAt;s.soundIntervalMs=0;m.timedSequences.push_back(s);
}
// A sequence cue with its own pitch (the kit's cue() is fixed at 1).
void mhCue(Map& m,const char* timer,int time,Sound sound,Vec2 p,float z,float gain,float pitch,const char* caption=""){
 SequenceCue c;c.timerState=stateId(timer);c.timeMs=time;c.sound=sound;c.position=p;c.gain=gain;c.pitch=pitch;c.caption=caption;c.z=z;m.sequenceCues.push_back(c);
}
// A script event on the player's box with a list of actions.
ScriptEvent& mhEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,float bottom,float top,StateId require=0){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=bottom;e.top=top;e.requireState=require;m.scriptEvents.push_back(e);return m.scriptEvents.back();
}
ScriptAction mhSet(StateId id,int value=1){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=id;a.value=value;return a;}
ScriptAction mhSound(Sound s,float gain){ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=s;a.amount=gain;return a;}
ScriptAction mhShake(float amount){ScriptAction a;a.type=ScriptAction::Type::Shake;a.amount=amount;return a;}
ScriptAction mhSave(){ScriptAction a;a.type=ScriptAction::Type::Checkpoint;return a;}
ScriptAction mhOpen(int door){ScriptAction a;a.type=ScriptAction::Type::OpenDoor;a.index=door;return a;}
// A terminal turned to face `yaw` (0 = +y).
void mhTerminal(Map& m,Vec2 p,float z,float yaw,const char* title,const char* a,const char* b,StateId state=0,StateId prerequisite=0){
 terminal(m,p,title,a,b,z-m.layers.front().elevation,state,prerequisite);m.terminals.back().yaw=yaw;
}

// --- District states -------------------------------------------------------------------------
const StateId MhBayA=stateId("mh_bay_a"),MhBayB=stateId("mh_bay_b"),MhBayC=stateId("mh_bay_c"),MhBayD=stateId("mh_bay_d");
const StateId MhVentSupply=stateId("mh_vent_supply");   // Switchboard 9, board 9-14 closed (chunk 42)
const StateId MhFanIsolated=stateId("mh_sf2_isolated"); // supply fan SF-2 isolated (chunk 45)
const StateId MhDropped=stateId("mh_ah_dropped");      // the player is down in riser room R-2 (chunk 46)

// One turbine-generator set on its table. x0 is the west end of the 19 m
// pedestal, yc the shaft line. unit 3 stands opened for overhaul.
void mhUnit(MhHall& hall,float x0,float yc,int unit,float northGap,float southGap){
 Map& m=hall.h;const float G=mhTable,F=mhFloor,under=G-.22f;
 const bool opened=unit==3;
 // --- The concrete table: four bents, edge beams, the deck over them -------------------
 mhDeck(hall,G,int(x0-.5f),int(yc-4),int(x0+19.5f),int(yc+4));
 for(auto [a,b]:{std::pair{0.f,1.2f},{5.6f,6.6f},{11.f,12.f},{17.8f,19.f}}){
  wall(m,x0+a,yc-3.6f,x0+b,yc+3.6f,F,under,15);
  // Haunches under the table edge.
  for(float s:{-1.f,1.f})wall(m,x0+a-.25f,yc+s*3.6f-(s>0?.4f:0.f),x0+b+.25f,yc+s*3.6f+(s<0?.4f:0.f),under-.5f,under,15);
 }
 for(float s:{-1.f,1.f}){float y=yc+s*3.4f;wall(m,x0,y-.2f,x0+19.f,y+.2f,G-1.3f,under,15);}
 for(float s:{-1.f,1.f})floorLine(m,x0,yc+s*3.75f-.06f,x0+19.f,yc+s*3.75f+.06f,F,13);
 // Kerb paint and the table's steel edge angle.
 for(float s:{-1.f,1.f})wall(m,x0-.5f,yc+s*4.f-(s>0?.06f:0.f),x0+19.5f,yc+s*4.f+(s<0?.06f:0.f),under,G+.02f,12);
 // --- Under the table: condenser in the middle bay, oil plant west, bus ducts east ----
 wall(m,x0+7.f,yc-2.6f,x0+10.6f,yc+2.6f,F,F+2.f,11);
 for(float x:{x0+7.f,x0+10.45f})wall(m,x,yc-2.7f,x+.15f,yc+2.7f,F,F+2.1f,8);
 for(float x=x0+7.6f;x<x0+10.4f;x+=.7f)wall(m,x,yc-2.66f,x+.06f,yc+2.66f,F+.2f,F+1.9f,12);
 for(float s:{-1.f,1.f}){float y=yc+s*2.6f;
  mhLowPipe(m,{x0+8.8f,y},{x0+8.8f,y+s*.9f},F+1.f,.42f,5,F);
  mhRiser(m,{x0+8.8f,y+s*.9f},F,F+1.f,.42f,5);
  flange(m,{x0+8.8f,y+s*.9f,F+.05f},{},.42f);
 }
 // Lube oil: the tank and pumps west, lines up into the bearings.
 wall(m,x0+1.8f,yc-1.8f,x0+4.8f,yc+.2f,F,F+1.5f,18);
 wall(m,x0+1.7f,yc-1.9f,x0+4.9f,yc+.3f,F+1.5f,F+1.6f,12);
 for(float x:{x0+2.4f,x0+4.1f})mhFix(m,model::pwr_circulation_pump,x,yc+1.3f,F+.12f,.84f,.5f,.39f,0);
 for(float x:{x0+2.4f,x0+4.1f})wall(m,x-.5f,yc+.95f,x+.5f,yc+1.65f,F,F+.12f,15);
 for(float x:{x0+2.6f,x0+3.6f})m.pipes.push_back({{x,yc-.8f},{x,yc-.8f},F+1.6f,.05f,under,3});
 // Isolated-phase bus from the generator terminals down through the east bay.
 for(float s:{-.8f,0.f,.8f}){wall(m,x0+14.6f,yc+s-.28f,x0+15.2f,yc+s+.28f,F,under,8);}
 wall(m,x0+13.f,yc+2.f,x0+16.6f,yc+3.f,F,F+2.f,12);
 // --- On the table -------------------------------------------------------------------
 const float shaft=G+1.6f;
 auto bearing=[&](float a,float b){wall(m,x0+a,yc-.9f,x0+b,yc+.9f,G,shaft+.2f,12);wall(m,x0+a-.05f,yc-.6f,x0+b+.05f,yc+.6f,shaft+.2f,shaft+.55f,8);
  m.pipes.push_back({{x0+a-.05f,yc},{x0+b+.05f,yc},shaft,.22f,shaft,6});};
 bearing(3.8f,4.4f);bearing(8.93f,9.4f);bearing(13.8f,14.3f);bearing(18.1f,18.5f);
 if(!opened){
  // HP casing, the IP casing, the LP exhaust hood stepping up to its crown.
  mhFix(m,model::pwr_turbine_housing,x0+2.1f,yc,G,3.4f,3.4f,2.48f,0);
  mhFix(m,model::turbine_casing,x0+6.67f,yc,G,4.53f,2.68f,3.22f,0);
  const float hood[][3]={{2.4f,G,G+1.6f},{2.1f,G+1.6f,G+2.5f},{1.65f,G+2.5f,G+3.1f},{.95f,G+3.1f,G+3.45f}};
  for(auto& h:hood)wall(m,x0+9.4f,yc-h[0],x0+13.8f,yc+h[0],h[1],h[2],20);
  for(float x=x0+9.9f;x<x0+13.6f;x+=.9f)for(float s:{-1.f,1.f})wall(m,x,yc+s*2.4f-(s>0?0.f:.06f),x+.1f,yc+s*2.4f+(s>0?.06f:0.f),G,G+1.6f,12);
  wall(m,x0+9.35f,yc-2.45f,x0+13.85f,yc+2.45f,G+1.55f,G+1.65f,12);
  // Crossover from the IP casing over into the LP crown.
  pipeRoute(m,{{x0+6.67f,yc,G+3.22f},{x0+6.67f,yc,G+4.4f},{x0+11.6f,yc,G+4.4f},{x0+11.6f,yc,G+3.45f}},.42f,5,NoRoof);
 }else{
  // Opened for overhaul: the lower halves stand open, their tops on the lay-down floor.
  wall(m,x0+.6f,yc-1.6f,x0+3.6f,yc+1.6f,G,G+1.15f,20);
  for(float x=x0+1.f;x<x0+3.4f;x+=.55f)wall(m,x,yc-1.4f,x+.08f,yc+1.4f,G+1.15f,G+1.75f,8);
  wall(m,x0+4.6f,yc-1.3f,x0+8.7f,yc+1.3f,G,G+1.3f,20);
  for(float x=x0+5.f;x<x0+8.5f;x+=.6f)wall(m,x,yc-1.15f,x+.08f,yc+1.15f,G+1.3f,G+2.f,8);
  for(float s:{-1.f,1.f})wall(m,x0+9.4f,yc+s*2.4f-(s>0?.15f:0.f),x0+13.8f,yc+s*2.4f+(s<0?.15f:0.f),G,G+1.6f,20);
  for(float x:{x0+9.4f,x0+13.65f})wall(m,x,yc-2.4f,x+.15f,yc+2.4f,G,G+1.6f,20);
  wall(m,x0+9.55f,yc-2.25f,x0+13.65f,yc+2.25f,G,G+.4f,8);
  for(float x=x0+10.2f;x<x0+13.4f;x+=.8f)wall(m,x,yc-2.25f,x+.1f,yc+2.25f,G+.4f,G+1.4f,8);
  // Scaffold tower over the open HP end: standards, ledgers and a board lift.
  for(float x:{x0+.2f,x0+4.2f})for(float s:{-1.f,1.f})mhRiser(m,{x,yc+s*2.f},G,G+4.2f,.03f,2);
  for(float z:{G+2.1f,G+4.1f})for(float s:{-1.f,1.f})m.pipes.push_back({{x0+.2f,yc+s*2.f},{x0+4.2f,yc+s*2.f},z,.03f,z,2});
  for(float z:{G+2.1f,G+4.1f})for(float x:{x0+.2f,x0+4.2f})m.pipes.push_back({{x,yc-2.f},{x,yc+2.f},z,.03f,z,2});
  wall(m,x0+.2f,yc-2.05f,x0+4.2f,yc+2.05f,G+4.1f,G+4.16f,10);
 }
 // Generator: feet, stator, end bells, hydrogen coolers; then the exciter.
 wall(m,x0+14.4f,yc-1.4f,x0+18.f,yc+1.4f,G,G+.6f,8);
 m.pipes.push_back({{x0+14.5f,yc},{x0+17.9f,yc},G+2.f,1.35f,G+2.f,unit==2?4:5});
 for(float x=x0+15.f;x<x0+17.6f;x+=.8f)m.pipes.push_back({{x,yc},{x+.12f,yc},G+2.f,1.42f,G+2.f,6});
 for(float x:{x0+14.3f,x0+17.9f})m.pipes.push_back({{x,yc},{x+.2f,yc},G+2.f,1.18f,G+2.f,6});
 wall(m,x0+15.2f,yc-.6f,x0+17.2f,yc+.6f,G+3.3f,G+3.65f,12);
 m.structures.push_back({x0+14.3f,yc-1.45f,x0+18.1f,yc+1.45f,G,G+3.4f,false,6});
 m.pipes.push_back({{x0+18.5f,yc},{x0+19.3f,yc},G+.95f,.55f,G+.95f,6});
 wall(m,x0+18.5f,yc-.6f,x0+19.3f,yc+.6f,G,G+.4f,8);
 m.structures.push_back({x0+18.5f,yc-.6f,x0+19.3f,yc+.6f,G,G+1.5f,false,6});
 // Guard rails round the table and lagging covers over the valve gear.
 // Gaps (2 m) where the floor stair arrives on the south side and a bridge on the north.
 auto edgeRail=[&](float y1,float y2,float gap){
  if(gap<0){guardRail(m,x0-.5f,y1,x0+19.5f,y2,G);return;}
  // Unit 2's north-east table meets the shutter gallery; keep that lane open.
  const float eastEnd=unit==2&&y2<yc?44.f:x0+19.5f;
  guardRail(m,x0-.5f,y1,gap,y2,G);guardRail(m,gap+2.f,y1,eastEnd,y2,G);};
 edgeRail(yc-4.f,yc-3.95f,northGap);edgeRail(yc+3.95f,yc+4.f,southGap);
 guardRail(m,x0-.5f,yc-4.f,x0-.45f,yc+4.f,G);guardRail(m,x0+19.45f,yc-4.f,x0+19.5f,yc+4.f,G);
 // Painted walkway lines on the table.
 for(float s:{-1.f,1.f})floorLine(m,x0,yc+s*3.f-.05f,x0+19.f,yc+s*3.f+.05f,G,14);
 // Local panel, east end.
 mhFix(m,model::pwr_control_cabinet,x0+16.5f,yc-3.55f,G,.8f,.4f,1.6f,0);
}

// A tripod floodlight standing on a floor: three legs, the head box and its lamp.
void mhFlood(Map& m,Vec2 p,float floor,float yaw,float kelvin=5600){
 const float top=floor+2.f;
 for(int i=0;i<3;++i){float a=i*kPi*2/3+yaw;Vec2 foot=p+Vec2{std::cos(a),std::sin(a)}*.55f;m.pipes.push_back({foot,p,floor,.025f,top,12});}
 m.pipes.push_back({p,p,top,.03f,top+.25f,12});
 Vec2 d{std::cos(yaw),std::sin(yaw)};
 wall(m,p.x+d.x*.1f-.2f,p.y+d.y*.1f-.2f,p.x+d.x*.1f+.2f,p.y+d.y*.1f+.2f,top+.25f,top+.55f,8);
 m.lights.push_back(lamp(p+d*.32f,top+.4f,2.2f,10.f,kelvin,.05f,WorldLightMount::Bare,yaw));
}
// Timber cribbing: crossed layers of sleepers under a load.
void mhCrib(Map& m,float x,float y,float floor,float top,float w=1.2f){
 int layers=std::max(1,int((top-floor)/.15f+.5f));float h=(top-floor)/layers;
 for(int i=0;i<layers;++i){float z=floor+i*h;
  if(i%2==0)for(float dy:{-w*.35f,w*.35f})wall(m,x-w*.5f,y+dy-.1f,x+w*.5f,y+dy+.1f,z,z+h,10);
  else for(float dx:{-w*.35f,w*.35f})wall(m,x+dx-.1f,y-w*.5f,x+dx+.1f,y+w*.5f,z,z+h,10);}
 m.structures.push_back({x-w*.5f,y-w*.5f,x+w*.5f,y+w*.5f,floor,top,false,6});
}

// The hall floor between and around the units: feedwater heaters and boiler
// feed pumps on the centre floor, the laydown bay where Unit 3's rotor rests
// on its stands, the outage office, the blade bench, aisles and floor paint.
void mhHallFloor(MhHall& hall){
 Map& m=hall.h;const float F=mhFloor,G=mhTable,C=mhCatwalk;
 // --- Aisles: green walkway paint from the gallery stair to the crane stair ----------------
 for(float y:{21.7f,23.5f})floorLine(m,4.6f,y-.05f,43.f,y+.05f,F,14);
 for(float x:{22.65f,25.35f})floorLine(m,x-.05f,9.f,x+.05f,21.65f,F,14);
 for(float y:{4.6f,9.f})floorLine(m,4.6f,y-.05f,22.6f,y+.05f,F,14);
 floorLine(m,4.55f,4.6f,4.65f,21.65f,F,14);
 for(float x=6.f;x<43;x+=4.f)floorLine(m,x,22.55f,x+1.2f,22.65f,F,13);
 // --- Feedwater heaters on their pads, west of the centre floor --------------------------
 for(float x:{8.f,16.4f}){
  equipmentPad(m,x-3.5f,25.4f,x+3.5f,29.2f,F);
  mhFix(m,model::storage_tank,x,27.3f,F+.1f,6.34f,2.86f,3.84f,0);
  // Shell outlet up to the pipe bridge, a drain to the floor gully.
  pipeRoute(m,{{x+1.6f,27.3f,F+3.94f},{x+1.6f,27.3f,F+5.1f},{x+1.6f,25.05f,F+5.1f}},.2f,5,NoRoof);
  pipeRoute(m,{{x-2.4f,28.4f,F+.75f},{x-2.4f,29.6f,F+.75f},{x-2.4f,29.6f,F}},.06f,3,NoRoof,F);
 }
 // The pipe bridge they feed: columns, a beam and the header west through the wall.
 for(float x:{4.f,10.f,16.f,21.f})steelColumn(m,x,24.9f,F,F+4.86f);
 wall(m,3.6f,24.7f,21.4f,25.1f,F+4.86f,F+4.98f,12);
 for(float y:{24.82f,25.f})m.pipes.push_back({{1.f,y},{21.4f,y},F+5.1f,.16f,F+5.1f,y<24.9f?5:3});
 for(float x:{1.3f,21.25f})for(float y:{24.82f,25.f})flange(m,{x,y,F+5.1f},{1,0},.16f);
 // --- Boiler feed pumps on the centre floor ------------------------------------------------
 for(float y:{26.4f,29.6f}){
  P3 out=pumpSkid(m,{23.6f,y},F,kPi,F+1.35f);
  pipeRoute(m,{{out.x,out.y,F+1.35f},{out.x+1.6f,out.y,F+1.35f},{out.x+1.6f,out.y,F}},.11f,3,NoRoof,F);
 }
 // --- Laydown bay: Unit 3's rotor on its stands -------------------------------------------
 {const float y=30.f,z=F+1.75f;
  for(float x:{27.7f,38.7f}){mhCrib(m,x,y,F,F+.6f);wall(m,x-.35f,y-.45f,x+.35f,y+.45f,F+.6f,z-.62f,12);
   wall(m,x-.35f,y-.55f,x+.35f,y-.25f,z-.62f,z-.15f,12);wall(m,x-.35f,y+.25f,x+.35f,y+.55f,z-.62f,z-.15f,12);}
  m.pipes.push_back({{26.9f,y},{40.3f,y},z,.42f,z,6});
  // HP and IP wheels, then the LP rows growing toward L-0.
  const float wheels[][2]={{28.6f,.8f},{29.1f,.82f},{29.6f,.86f},{30.1f,.9f},{30.6f,.95f},{31.1f,1.f},{32.f,1.2f},{32.7f,1.3f},{33.5f,1.4f},{34.4f,1.5f},{35.4f,1.62f}};
  for(auto& w:wheels){m.pipes.push_back({{w[0],y},{w[0]+.24f,y},z,w[1],z,w[1]>1.3f?5:6});m.pipes.push_back({{w[0]-.02f,y},{w[0]+.26f,y},z,w[1]*.55f,z,6});}
  // The L-0 row: the pitted blades, marked off in chalk paint.
  m.pipes.push_back({{36.4f,y},{36.72f,y},z,1.66f,z,5});
  for(int i=0;i<16;++i){float a=i*kPi/8;m.pipes.push_back({{36.36f,y+std::cos(a)*1.5f},{36.76f,y+std::cos(a)*1.5f},z+std::sin(a)*1.5f,.05f,z+std::sin(a)*1.5f,4});}
  m.pipes.push_back({{39.6f,y},{39.9f,y},z,.72f,z,6});
  m.structures.push_back({28.3f,y-1.7f,37.f,y+1.7f,F+.1f,z+1.7f,false,6});
  m.structures.push_back({26.9f,y-.45f,40.3f,y+.45f,z-.45f,z+.45f,false,6});
 }
 // The casing top halves set down on sleepers: LP hood and the HP shell.
 for(auto [x1,x2,r]:{std::tuple{26.4f,32.8f,2.1f},{35.4f,40.f,1.25f}}){
  const float yc=x2-x1>5?36.6f:36.2f;
  for(float x=x1+.4f;x<x2;x+=2.f)wall(m,x,yc-r-.3f,x+.3f,yc+r+.3f,F,F+.24f,10);
  m.pipes.push_back({{x1,yc},{x2,yc},F+.24f,r,F+.24f,5});
  // The horizontal joint flange along both sides, bolted every 0.6 m.
  wall(m,x1,yc-r-.2f,x2,yc-r+.02f,F+.24f,F+.42f,12);wall(m,x1,yc+r-.02f,x2,yc+r+.2f,F+.24f,F+.42f,12);
  for(float x=x1+.3f;x<x2;x+=.6f)for(float s:{-1.f,1.f})m.pipes.push_back({{x,yc+s*(r+.1f)},{x,yc+s*(r+.1f)},F+.42f,.035f,F+.5f,6});
  for(float x:{x1+.8f,x2-.8f})wall(m,x-.15f,yc-.12f,x+.15f,yc+.12f,F+.2f+r,F+.5f+r,18);
  m.structures.push_back({x1,yc-r-.3f,x2,yc+r+.3f,F,F+.24f+r*.7f,false,6});
 }
 // Crates of spare blades, the spares cage, a tool chest, gas, plate stock.
 mhFix(m,model::packing_crate,27.8f,26.f,F,1.681f,1.68f,1.2f,0);mhFix(m,model::packing_crate,29.7f,26.1f,F,1.681f,1.68f,1.2f,.06f);
 mhFix(m,model::packing_crate,27.85f,26.f,F+1.2f,1.681f,1.68f,1.2f,-.05f);
 cargo(m,33.f,25.9f,F);cargo(m,34.4f,25.9f,F,false);
 mhFix(m,model::stock_cage,39.6f,25.7f,F,4.49f,2.01f,2.03f,0);
 mhFix(m,model::mine_tool_chest,36.5f,26.f,F,.75f,.45f,.97f,0);
 mhFix(m,model::pwr_sheet_steel_stack,41.f,31.6f,F,1.79f,3.4f,.13f,0);
 for(float x:{41.4f,41.8f,42.2f})mhFix(m,model::gas_cylinder,x,40.5f,F,.33f,.33f,1.58f,0);
 wall(m,41.1f,40.15f,42.5f,40.2f,F+.9f,F+1.f,12);for(float x:{41.1f,42.45f})wall(m,x,40.15f,x+.05f,40.85f,F,F+1.f,12);
 forklift(m,38.4f,41.8f,F);
 // Laydown paint: the bay outline and its load limit.
 for(auto [x1,y1,x2,y2]:{std::tuple{25.8f,24.8f,42.6f,24.92f},{25.8f,42.9f,42.6f,43.02f},{25.8f,24.8f,25.92f,43.02f},{42.48f,24.8f,42.6f,32.f}})floorLine(m,x1,y1,x2,y2,F,13);
 // --- Outage office: a container against the laydown bay, permit desk inside ---------------
 container(m,24.4f,37.2f,F,true,12);
 mhTerminal(m,{25.9f,42.3f},F,kPi,"UNIT 3 / OUTAGE PERMIT 3-118","ROTOR OUT 10/19. ROW L-0 BLADES PITTED.","NOT EROSION. BLADE SAMPLES SENT TO LAB 4.");
 mhFix(m,model::file_cabinet_tall,27.f,40.1f,F,.5f,.6f,1.3f,-kPi*.5f);
 mhFix(m,model::wooden_chair,25.9f,41.4f,F,.417f,.525f,.95f,.2f);
 m.lights.push_back(lamp({25.9f,40.2f},F+2.6f,.9f,4.5f,3600,0));
 // --- South of Unit 3: the blade inspection bench --------------------------------------------
 for(float x:{5.2f,6.7f})mhFix(m,model::wooden_table,x,44.f,F,1.402f,.742f,.8f,0);
 for(int i=0;i<6;++i){float x=4.7f+i*.48f;wall(m,x,43.75f,x+.09f,44.3f,F+.8f,F+.83f,20);wall(m,x-.04f,43.7f,x+.13f,43.82f,F+.8f,F+.86f,20);}
 mhFix(m,model::wooden_chair,6.2f,44.9f,F,.417f,.525f,.95f,kPi+.4f);
 m.pipes.push_back({{7.6f,44.3f},{7.6f,44.3f},F,.025f,F+1.9f,12});m.pipes.push_back({{7.6f,44.3f},{7.f,44.f},F+1.9f,.02f,F+1.8f,12});
 mhFix(m,model::pendant_lamp,7.f,44.f,F+1.45f,.3f,.3f,.35f,0,false);
 m.lights.push_back(lamp({7.f,44.f},F+1.5f,.8f,4.f,3000,0,WorldLightMount::Bare));
 // --- Hard hats and litter of a shift that left in a hurry -----------------------------------
 mhHelmet(m,5.f,2.8f,G,.4f);mhHelmet(m,33.2f,27.6f,F,2.1f);mhHelmet(m,13.6f,22.8f,F,-.6f);mhHelmet(m,42.8f,45.6f,C,1.f);
 m.clutterSpawns.push_back({0,{9.f,2.6f},G,.3f});m.clutterSpawns.push_back({2,{37.4f,5.f},G,1.2f});m.clutterSpawns.push_back({4,{42.4f,3.2f},G,0});
 m.clutterSpawns.push_back({6,{31.f,27.6f},F,.8f});m.clutterSpawns.push_back({6,{35.6f,33.4f},F,2.f});m.clutterSpawns.push_back({1,{8.1f,45.3f},F,.4f});
 m.clutterSpawns.push_back({0,{20.4f,22.2f},F,0});m.clutterSpawns.push_back({2,{24.f,31.8f},F,1.f});
}

// The hall's light: emergency red until the high bays are back, the lamps
// people worked under (control room, panels, under the tables, the outage
// floods, the crane stair and catwalk), and the damper's status lamps.
void mhHallLight(MhHall& hall){
 Map& m=hall.h;const float F=mhFloor,G=mhTable,C=mhCatwalk;
 // Red emergency bulkheads: gallery, the stair foot, side walls, catwalk.
 for(float x:{6.f,16.5f,33.f})mhRed(m,{x,x==6.f?1.42f:1.02f},G+2.4f,kPi*.5f,MhBayD);
 mhRed(m,{1.02f,9.6f},F+2.6f,0,MhBayD);
 for(float y:{22.f,36.f}){mhRed(m,{1.02f,y},F+3.f,0,MhBayD,1.f,8.f);mhRed(m,{46.98f,y+1.f},F+3.f,kPi,MhBayD,1.f,8.f);}
 mhRed(m,{46.98f,28.6f},G+2.3f,kPi,MhBayD,.8f,5.f);
 // The hub door light and the gallery work lights by the lighting panel.
 mhLamp(m,{12.f,1.02f},G+2.95f,1.1f,6.f,2800,0,WorldLightMount::Wall,kPi*.5f);
 // Unit control room: fluorescent battens, one tube failing.
 for(Vec2 p:{Vec2{38.f,3.f},Vec2{41.5f,4.6f}})mhLamp(m,p,G+2.6f,1.1f,6.f,4300,.25f);
 mhLamp(m,{42.6f,2.f},G+2.6f,.9f,5.f,4300,.25f,WorldLightMount::Ceiling,0,0,1,false,.3f);
 // Under each table: a caged bulkhead over the condenser bay.
 for(Vec2 p:{Vec2{10.5f,13.f},Vec2{34.5f,13.f},Vec2{10.5f,37.f}})mhLamp(m,p,G-.4f,1.f,6.5f,3000,0);
 // Local panel lights on each table, on a post by the generator end.
 for(Vec2 p:{Vec2{17.4f,9.75f},Vec2{41.4f,9.75f},Vec2{17.4f,33.75f}}){
  wall(m,p.x-.05f,p.y-.05f,p.x+.05f,p.y+.05f,G,G+2.45f,12);
  mhLamp(m,{p.x,p.y+.06f},G+2.3f,1.f,6.f,2700,0,WorldLightMount::Wall,kPi*.5f);
 }
 // Unit 3 outage floods on the table, one in the laydown bay.
 mhFlood(m,{4.6f,34.4f},G,.6f);mhFlood(m,{20.f,39.8f},G,-2.4f);mhFlood(m,{34.f,33.2f},F,-1.3f,5200);
 // Crane access: the landing bulkhead; the catwalk's sodium wall packs.
 mhLamp(m,{46.98f,33.f},G+2.3f,1.f,6.f,3000,0,WorldLightMount::Wall,kPi);
 for(float x:{28.f,34.5f})mhLamp(m,{x,46.98f},C+2.4f,1.2f,7.f,2000,-.1f,WorldLightMount::Wall,-kPi*.5f);
 mhLamp(m,{43.7f,41.8f},C+2.15f,.7f,4.f,3000,0);
 // FD-9 status: red while the damper is held shut, then white once reset.
 mhLamp(m,{37.4f,46.98f},C+2.7f,1.f,5.f,1150,-.4f,WorldLightMount::Wall,-kPi*.5f,MhVentSupply,1,true);
 mhLamp(m,{42.6f,46.98f},C+2.7f,1.f,5.f,4600,.2f,WorldLightMount::Wall,-kPi*.5f,MhVentSupply);
 // Load Control's window glows warm high in the west wall.
 mhLamp(m,{.55f,15.f},-16.35f,1.2f,4.f,3400,0);
}

// Who and what uses the hall: terminals, doors, zones, creatures, pickups.
void mhHallScript(MhHall& hall){
 Map& m=hall.h;const float F=mhFloor,G=mhTable,C=mhCatwalk;
 using CK=CreatureKind;using P=PickupKind;
 // --- Terminals -------------------------------------------------------------------------------
 mhTerminal(m,{7.8f,1.55f},G,0,"MACHINE HALL / LIGHTING L-9","FEEDER 9 LIVE. HIGH BAYS STRIKE ROW A TO D.","GALLERY SPAN DOWN 02:06. CROSS ON THE FLOOR.");
 mhTerminal(m,{14.8f,9.6f},G,0,"UNIT 1 / TURNING GEAR","TRIPPED 23:58 ON GRID LOSS. RUNDOWN NORMAL.","GEAR ENGAGED 00:31. SHAFT STOPPED. SAFE.");
 mhTerminal(m,{38.8f,9.6f},G,0,"UNIT 2 / TURNING GEAR","GEAR DISENGAGED. NO STEAM ON UNIT. SHAFT 3 RPM.","MANUAL ANNOTATION: WHAT IS TURNING IT.");
 mhTerminal(m,{39.4f,2.35f},G,0,"SWITCHBOARD 9 / BOARD 9-14","VENT WORKS CONTROL SUPPLY: OPEN SINCE 23:58.","E / CLOSE 9-14. FIRE DAMPER FD-9 RESETS.",MhVentSupply);
 mhTerminal(m,{36.4f,46.15f},C,kPi,"FIRE DAMPER FD-9","CLOSED: CONTROL SUPPLY LOST AT 23:58.","RESET AT SWITCHBOARD 9 / UNIT CONTROL ROOM.");
 mhTerminal(m,{43.7f,41.25f},C,0,"CRANE 2 / CAB","LAST LIFT 10/19: UNIT 3 ROTOR TO LAYDOWN.","CRANE 1 HOIST BRAKE FAULT. DO NOT TRAVEL NORTH.");
 // --- Doors -------------------------------------------------------------------------------------
 {Door d{40.25f,41.75f,6.93f,0,false,false,false,G-F};d.swinging=true;d.swing=1;m.doors.push_back(d);}
 {Door d{38.1f,41.9f,46.85f,0,false,false,false,C-F};d.requireState=MhVentSupply;m.doors.push_back(d);}
 // The damper's frame: hazard banding on the jambs, the actuator box over it.
 for(float x:{37.8f,42.f})wall(m,x,46.64f,x+.2f,46.7f,C+.2f,C+2.5f,13);
 wall(m,38.6f,46.5f,40.2f,46.7f,C+2.95f,C+3.5f,8);m.pipes.push_back({{39.4f,46.6f},{39.4f,46.6f},C+2.9f,.05f,C+2.95f,6});
 // --- Zones (first match wins) ----------------------------------------------------------------
 zone(m,"UNIT CONTROL ROOM 9",36.f,1.f,44.f,7.f,G-.5f,G+3.f);
 zone(m,"MACHINE HALL / OUTAGE OFFICE",24.4f,37.2f,27.5f,43.2f,F-.5f,F+3.f);
 zone(m,"MACHINE HALL / NORTH GALLERY",0.f,0.f,48.f,4.2f,G-.5f,G+3.f);
 zone(m,"MACHINE HALL / SOUTH CATWALK",24.f,38.f,48.f,48.f,C-.5f,C+3.f);
 zone(m,"MACHINE HALL / CRANE STAIR",42.5f,27.5f,47.f,38.f,F-.5f,C+1.f);
 zone(m,"UNIT 1 / TURBINE-GENERATOR",1.5f,8.5f,22.5f,17.5f);
 zone(m,"UNIT 2 / EXCITER BAY",25.5f,7.f,48.f,17.5f);
 zone(m,"UNIT 3 / OUTAGE",1.5f,32.5f,22.5f,46.f);
 zone(m,"MACHINE HALL / LAYDOWN BAY",24.f,24.f,43.f,44.f,F-.5f,F+6.f);
 zone(m,"MACHINE HALL / FEED PUMPS",1.f,23.6f,26.f,32.f,F-.5f,F+6.f);
 // --- Creatures ---------------------------------------------------------------------------------
 // A huntsman lives in the dark under Unit 1's table; wasps nest in the laydown
 // bay and stir when the vent supply comes back (the louvres start to hum).
 m.creatureSpawns.push_back({CK::Huntsman,{19.f,11.6f},F});
 {CreatureSpawn w{CK::Wasp,{31.f,33.2f},F};w.wakeState=MhVentSupply;m.creatureSpawns.push_back(w);}
 {CreatureSpawn w{CK::Wasp,{37.f,27.6f},F};w.wakeState=MhVentSupply;m.creatureSpawns.push_back(w);}
 {CreatureSpawn w{CK::Wasp,{11.f,44.f},F};w.wakeState=MhVentSupply;m.creatureSpawns.push_back(w);}
 // --- Pickups -----------------------------------------------------------------------------------
 m.pickupSpawns.push_back({{1.9f,2.5f},P::Health,G});
 m.pickupSpawns.push_back({{7.4f,15.9f},P::Ammo,F});
 m.pickupSpawns.push_back({{42.7f,3.4f},P::Health,G});
 m.pickupSpawns.push_back({{45.6f,2.4f},P::Ammo,G});
 m.pickupSpawns.push_back({{20.4f,44.6f},P::Ammo,F});
 m.pickupSpawns.push_back({{44.4f,42.3f},P::Ammo,C});
 m.pickupSpawns.push_back({{26.4f,45.6f},P::Health,C});
 // --- Signs that guide -------------------------------------------------------------------------
 board(m,32.6f,21.f,F+1.4f,"UNIT 2 PLATFORM","CONTROL ROOM 9 / NORTH OVER THE TABLE",2.6f);
}

const MhHall& mhHall(){
 static const MhHall built=[]{
  MhHall hall;Map& m=hall.h;
  m.definition=CampaignChunks[41];m.definition.ceiling=mhRoof;
  m.layers.push_back(floorLayer("Machine Hall",mhFloor));
  const float F=mhFloor,G=mhTable,C=mhCatwalk,R=mhRoof;
  // --- Walls: plinth band, pilasters, girts, high louvres ------------------------------
  wall(m,1.f,1.f,1.3f,47.f,F,F+1.2f,15);wall(m,46.7f,1.f,47.f,47.f,F,F+1.2f,15);
  wall(m,1.f,1.f,47.f,1.3f,F,F+1.2f,15);wall(m,1.f,46.7f,47.f,47.f,F,F+1.2f,15);
  for(float x:{6.f,18.f,24.f,30.f,36.f,42.f})wall(m,x-.3f,1.f,x+.3f,1.4f,F,2.f,15);
  for(float x:{6.f,12.f,18.f,24.f,30.f,34.f,45.f})wall(m,x-.3f,46.6f,x+.3f,47.f,F,2.f,15);
  for(float z:{-11.f,-3.f}){wall(m,1.f,1.f,47.f,1.15f,z,z+.3f,12);wall(m,1.f,46.85f,47.f,47.f,z,z+.3f,12);
   wall(m,1.f,1.f,1.15f,47.f,z,z+.3f,12);wall(m,46.85f,1.f,47.f,47.f,z,z+.3f,12);}
  for(float x:{9.f,15.f,21.f,27.f,33.f,39.f}){mhFix(m,model::pwr_vent_grille,x,1.12f,-9.f,2.05f,.1f,2.f,0,false);mhFix(m,model::pwr_vent_grille,x,46.88f,-9.f,2.05f,.1f,2.f,kPi,false);}
  // --- Crane runways on the side walls -----------------------------------------------
  for(float y:{6.f,12.f,18.f,24.f,30.f,36.f,42.f})mhCraneColumn(m,1.f,y,1.f);
  for(float y:{7.f,13.f,19.f,25.f,31.f,37.f,43.f})mhCraneColumn(m,47.f,y,-1.f);
  for(auto [x1,x2,r1,r2]:{std::tuple{1.6f,2.25f,1.88f,1.98f},{45.75f,46.4f,46.02f,46.12f}}){
   wall(m,x1,1.f,x2,47.f,mhRail-.8f,mhRail-.12f,12);
   wall(m,r1,1.f,r2,47.f,mhRail-.12f,mhRail,8);
   for(float y:{1.4f,46.3f})wall(m,x1,y,x2,y+.3f,mhRail-.12f,mhRail+.45f,13);
  }
  // --- Roof: trusses every 6 m, the lighting rig under the crane clearance ----------------
  for(float y=4.f;y<47;y+=6.f){
   wall(m,1.f,y-.15f,47.f,y+.15f,R-.5f,R,12);wall(m,1.f,y-.12f,47.f,y+.12f,2.f,2.35f,12);
   for(float x=2.f;x<47;x+=3.f)wall(m,x-.06f,y-.08f,x+.06f,y+.08f,2.35f,R-.5f,12);
  }
  mhLightingGirder(m,8.f,MhBayA);mhLightingGirder(m,20.f,MhBayB,36.f);mhLightingGirder(m,32.f,MhBayC);mhLightingGirder(m,44.f,MhBayD,12.f);
  // Dim roof lamps hang under the trusses so the height reads in the dark.
  for(Vec2 l:{Vec2{12.f,10.f},Vec2{36.f,16.f},Vec2{12.f,34.f},Vec2{36.f,40.f}})mhLamp(m,l,1.6f,2.2f,14.f,2100,-.1f);
  // --- Cranes --------------------------------------------------------------------------
  mhCrane(m,5.5f,28.f);mhCrane(m,40.5f,30.5f);
  // Crane 1: the hoist ropes parted; frayed ends hang under the trolley.
  for(float dx:{-.3f,.3f})for(float dy:{-.25f,.25f})m.pipes.push_back({{28.f+dx,6.5f+dy},{28.f+dx,6.5f+dy},mhGirderTop-1.f,.03f,mhGirderTop,6});
  // Crane 1 cab under the west end: dark, door open.
  wall(m,3.f,5.7f,5.2f,7.7f,mhGirderTop-3.3f,mhGirderTop-3.15f,12);
  for(float x:{3.f,5.05f})wall(m,x,5.7f,x+.15f,7.7f,mhGirderTop-3.15f,mhGirderTop-1.2f,12);
  wall(m,3.15f,7.55f,5.05f,7.7f,mhGirderTop-3.15f,mhGirderTop-2.3f,12);wall(m,3.15f,7.56f,5.05f,7.64f,mhGirderTop-2.3f,mhGirderTop-1.35f,9);
  wall(m,3.15f,5.7f,3.6f,5.85f,mhGirderTop-3.15f,mhGirderTop-1.2f,12);
  wall(m,3.f,5.7f,5.2f,7.7f,mhGirderTop-1.35f,mhGirderTop-1.2f,12);
  // Crane 2: its hook block still hangs over the lay-down bay.
  for(float dy:{-.15f,.15f})m.pipes.push_back({{30.5f,41.5f+dy},{30.5f,41.5f+dy},-24.5f,.025f,mhGirderTop,6});
  wall(m,30.05f,41.15f,30.95f,41.85f,-25.6f,-24.5f,18);
  for(float x:{30.2f,30.8f})m.pipes.push_back({{x,41.15f},{x,41.85f},-24.85f,.32f,-24.85f,6});
  m.pipes.push_back({{30.5f,41.5f},{30.5f,41.5f},-26.f,.05f,-25.6f,6});
  m.pipes.push_back({{30.5f,41.5f},{30.85f,41.5f},-26.1f,.05f,-26.f,6});
  m.pipes.push_back({{30.85f,41.5f},{30.9f,41.5f},-26.1f,.05f,-25.75f,6});
  // Crane 2 cab, docked at the catwalk landing: floor flush with the -22 deck.
  wall(m,42.4f,40.6f,45.f,43.05f,C-.2f,C,8);
  wall(m,42.4f,40.6f,42.55f,43.05f,C,C+.9f,12);wall(m,42.4f,40.6f,42.55f,43.05f,C+.9f,C+2.f,9);wall(m,42.4f,40.6f,42.55f,43.05f,C+2.f,C+2.3f,12);
  wall(m,42.55f,40.6f,45.f,40.75f,C,C+.9f,12);wall(m,42.55f,40.6f,45.f,40.68f,C+.9f,C+2.f,9);wall(m,42.55f,40.6f,45.f,40.75f,C+2.f,C+2.3f,12);
  wall(m,44.85f,40.75f,45.f,43.05f,C,C+2.3f,12);
  wall(m,42.55f,42.9f,43.4f,43.05f,C,C+2.3f,12);wall(m,44.5f,42.9f,44.85f,43.05f,C,C+2.3f,12);wall(m,43.4f,42.9f,44.5f,43.05f,C+2.1f,C+2.3f,12);
  wall(m,42.4f,40.6f,45.f,43.05f,C+2.3f,C+2.45f,12);
  for(float x:{42.8f,44.6f})m.pipes.push_back({{x,41.8f},{x,41.8f},C+2.45f,.06f,mhGirderTop-1.2f,6});
  // --- North gallery at -25: from the hub door west and east to the broken span -------------
  mhDeck(hall,G,1,1,25,4);mhDeck(hall,G,10,0,14,1);
  mhDeck(hall,G,31,1,47,4);
  mhPlate(m,1.f,1.f,25.f,4.f,G);mhPlate(m,31.f,1.f,36.f,4.f,G);mhPlate(m,10.f,0.f,14.f,1.f,G);
  // The door's sill: concrete below the -25 threshold, the hub beyond.
  wall(m,10.f,0.f,14.f,1.f,F,G-.22f,15);
  guardRail(m,4.2f,3.95f,17.f,4.f,G);guardRail(m,19.f,3.95f,25.f,4.f,G);guardRail(m,1.f,3.95f,2.2f,4.f,G);
  guardRail(m,31.f,3.95f,36.f,4.f,G);
  for(float x:{3.f,9.f,15.f,21.f,33.f})steelColumn(m,x,3.75f,F,G-.22f);
  for(float x:{6.f,12.f,24.f,33.f,39.f,45.f})m.pipes.push_back({{x,1.05f},{x,3.7f},G-.7f,.05f,G-.25f,6});
  // Gallery stair down the west end, and the bridge out to Unit 1's table.
  m.stairs.push_back({2.2f,4.f,4.2f,8.2f,F,G,15,true,false});
  mhDeck(hall,G,17,4,19,9);mhPlate(m,17.f,4.f,19.f,9.f,G);
  guardRail(m,16.95f,4.f,17.f,9.f,G);guardRail(m,19.f,4.f,19.05f,9.f,G);
  steelColumn(m,18.f,6.5f,F,G-.22f);
  // --- The broken span: Crane 1 dropped the Unit 2 end shield through it ------------------
  // Torn plate and bent rail at both ends of the gap.
  for(auto [x,dir]:{std::pair{25.f,1.f},std::pair{31.f,-1.f}}){
   for(float y:{1.3f,2.2f,3.1f})wall(m,std::min(x,x+dir*.45f),y,std::max(x,x+dir*.45f),y+.5f,G-.24f,G-.1f,2);
   m.pipes.push_back({{x-dir*.05f,3.9f},{x+dir*.7f,3.9f},G+1.05f,.03f,G-.6f,3});
   m.pipes.push_back({{x-dir*.05f,3.9f},{x+dir*.5f,3.6f},G+.5f,.03f,G-.9f,3});
   mhFix(m,model::barrier,x-dir*2.f,2.5f,G,1.6f,.8f,.88f,kPi*.5f);
  }
  wallSign(m,21.f,1.055f,G+1.3f,"GALLERY CLOSED","SPAN DOWN / USE THE HALL FLOOR",2.2f,0);
  // The fallen span lies on the floor, its west end propped on the rubble.
  wall(m,25.4f,1.4f,30.4f,3.7f,F,F+.2f,2);
  wall(m,25.2f,1.5f,26.5f,3.6f,F+.2f,F+.45f,15);wall(m,25.3f,1.6f,26.3f,3.5f,F+.45f,F+.62f,2);
  for(float x=25.6f;x<30.4f;x+=.9f)m.pipes.push_back({{x,3.6f},{x+.3f,3.6f},F+.25f,.03f,F+.85f,3});
  m.pipes.push_back({{25.6f,3.6f},{30.2f,3.6f},F+.95f,.03f,F+.4f,3});
  m.pipes.push_back({{26.4f,2.2f},{29.8f,4.6f},F+.15f,.14f,F+.25f,6});
  // The end shield stands where it hit, sunk into the cracked slab.
  m.pipes.push_back({{26.95f,6.6f},{27.55f,6.6f},F+2.f,2.05f,F+2.f,5});
  m.pipes.push_back({{26.75f,6.6f},{27.75f,6.6f},F+2.f,.62f,F+2.f,6});
  for(int i=0;i<12;++i){float a=i*kPi/6;Vec2 c{27.25f,6.6f+std::cos(a)*1.75f};m.pipes.push_back({{26.9f,c.y},{27.6f,c.y},F+2.f+std::sin(a)*1.75f,.06f,F+2.f+std::sin(a)*1.75f,6});}
  m.structures.push_back({26.9f,4.7f,27.6f,8.5f,F,F+3.6f,false,6});
  m.structures.push_back({26.9f,5.2f,27.6f,8.f,F+3.6f,F+4.f,false,6});
  for(auto b:{std::tuple{25.6f,5.4f,.5f},{28.3f,5.f,.4f},{28.6f,7.9f,.55f},{25.9f,8.2f,.35f},{24.9f,6.7f,.3f}}){auto [x,y,s]=b;wall(m,x-s,y-s*.8f,x+s,y+s*.8f,F,F+s*.7f,15);}
  for(auto c:{std::tuple{24.8f,5.9f,27.f,6.f},{27.6f,6.4f,29.9f,6.5f},{26.2f,8.4f,26.3f,9.8f},{27.9f,4.f,28.f,5.6f}}){auto [x1,y1,x2,y2]=c;wall(m,x1,y1,x2,y2,F,F+.006f,8);}
  // The hook block lies on its side, rope coiled round it.
  wall(m,29.f,7.4f,30.4f,8.5f,F,F+.95f,18);
  for(float x:{29.3f,29.75f,30.15f})m.pipes.push_back({{x,7.35f},{x,8.55f},F+.5f,.38f,F+.5f,6});
  m.pipes.push_back({{30.4f,7.95f},{31.1f,7.7f},F+.35f,.1f,F+.12f,6});
  m.pipes.push_back({{31.1f,7.7f},{31.55f,7.15f},F+.12f,.1f,F+.1f,6});
  m.structures.push_back({29.f,7.35f,31.6f,9.f,F,F+.95f,false,6});
  for(int ring=0;ring<3;++ring){float r=1.f+ring*.18f;Vec2 c{32.8f,6.3f};
   for(int i=0;i<12;++i){float a=i*kPi/6,b=(i+1)*kPi/6;m.pipes.push_back({c+Vec2{std::cos(a),std::sin(a)}*r,c+Vec2{std::cos(b),std::sin(b)}*r,F+.04f+ring*.06f,.035f,F+.04f+ring*.06f,6});}}
  // --- Exciter bay unit control room at -25, over the north-east gallery ----------------
  mhDeck(hall,G,36,1,44,7);mhDeck(hall,G,44,1,47,12);mhDeck(hall,G,47,8,48,12);mhDeck(hall,G,40,7,42,9);
  mhPlate(m,44.f,4.f,47.f,12.f,G);mhPlate(m,47.f,8.f,48.f,12.f,G);mhPlate(m,40.f,7.f,42.f,9.f,G);
  wall(m,36.f,1.f,44.f,7.f,G,G+.012f,24);
  mhGlazed(m,36.15f,6.85f,40.2f,7.f,G);mhGlazed(m,41.8f,6.85f,44.f,7.f,G);wall(m,40.2f,6.85f,41.8f,7.f,G+2.25f,G+2.7f,3);
  mhGlazed(m,36.f,3.4f,36.15f,7.f,G);wall(m,36.f,1.f,36.15f,1.6f,G,G+2.7f,3);wall(m,36.f,1.6f,36.15f,3.4f,G+2.25f,G+2.7f,3);
  wall(m,43.85f,1.f,44.f,4.6f,G,G+2.7f,3);wall(m,43.85f,6.4f,44.f,6.85f,G,G+2.7f,3);wall(m,43.85f,4.6f,44.f,6.4f,G+2.25f,G+2.7f,3);
  wall(m,36.f,1.f,44.f,7.f,G+2.7f,G+2.86f,2);
  ceilingGrid(m,36.15f,1.f,43.85f,6.85f,G+2.7f);
  // Switchboard 9: the station service lineup along the north wall.
  for(float x:{37.15f,38.65f,40.15f})mhFix(m,model::pwr_switchgear_cabinet,x+.75f,1.41f,G,1.5f,.82f,2.7f,0);
  mhFix(m,model::pwr_tall_switch_cabinet,43.3f,2.2f,G,.95f,1.54f,2.3f,kPi);
  // The operator's desk under the windows faces the hall; the shift log is on it.
  mhTerminal(m,{38.4f,6.15f},G,kPi,"UNIT CONTROL / SHIFT LOG","02:40 CLEANUP FORCE ORDERS THE HALL CLEARED.","OPERATORS TO MUSTER C. K. VOSS STAYS AT DESK.");
  mhFix(m,model::office_chair,38.6f,5.25f,G,.55f,.55f,.95f,.3f);
  mhFix(m,model::desk_phone,39.25f,6.2f,G+.75f,.2f,.2f,.1f,kPi,false);
  mhFix(m,model::mug,37.7f,6.05f,G+.75f,.09f,.09f,.1f,0,false);
  mhFix(m,model::binder,37.4f,6.35f,G+.75f,.3f,.08f,.32f,kPi,false);
  // Bridge from Unit 2's table to the control room door.
  guardRail(m,39.95f,7.f,40.f,9.f,G);guardRail(m,42.f,7.f,42.05f,9.f,G);
  // East strip to Emergency Generation's shutter.
  guardRail(m,44.f,7.f,44.05f,12.f,G);guardRail(m,44.f,11.95f,47.f,12.f,G);
  steelColumn(m,44.3f,11.7f,F,G-.22f);steelColumn(m,44.3f,7.4f,F,G-.22f);
  wall(m,47.f,8.f,48.f,12.f,F,G-.22f,15);wall(m,47.f,8.f,48.f,12.f,G+3.2f,R,15);
  for(float y:{7.9f,11.95f})wall(m,46.9f,y,47.1f,y+.15f,G,G+3.2f,12);
  wall(m,46.9f,7.9f,47.1f,12.1f,G+3.05f,G+3.2f,12);
  // --- The south catwalk at -22 and the crane access stair --------------------------------
  mhDeck(hall,C,25,44,47,47);mhDeck(hall,C,38,47,42,48);mhDeck(hall,C,42,38,47,44);mhDeck(hall,G,43,32,47,34);
  mhPlate(m,25.f,44.f,47.f,47.f,C);mhPlate(m,38.f,47.f,42.f,48.f,C);mhPlate(m,42.f,38.f,47.f,44.f,C);mhPlate(m,43.f,32.f,47.f,34.f,G);
  m.stairs.push_back({43.6f,27.8f,45.6f,32.f,F,G,15,true,true});
  m.stairs.push_back({43.6f,34.f,45.6f,38.2f,G,C,15,true,true});
  guardRail(m,25.f,43.95f,42.95f,44.f,C);guardRail(m,25.f,44.f,25.05f,47.f,C);
  guardRail(m,41.95f,38.f,42.f,44.f,C);
  guardRail(m,42.95f,32.f,43.f,34.f,G);guardRail(m,45.6f,32.f,47.f,32.05f,G);
  for(float x:{26.f,32.f,38.f})steelColumn(m,x,44.25f,F,C-.22f);
  for(float y:{32.25f,33.75f})steelColumn(m,43.25f,y,F,G-.22f);
  for(float y:{38.3f,41.f})steelColumn(m,43.25f,y,F,C-.22f);
  for(float x=27.5f;x<46.5f;x+=3.f)m.pipes.push_back({{x,46.85f},{x,44.4f},C-1.6f,.05f,C-.25f,6});
  // The vestibule through the south wall: concrete below, lintel over the damper.
  wall(m,38.f,47.f,42.f,48.f,F,C-.22f,15);wall(m,38.f,47.f,42.f,48.f,C+2.65f,R,15);
  for(float x:{37.8f,42.f})wall(m,x,46.7f,x+.2f,47.f,C,C+2.9f,12);
  wall(m,37.8f,46.7f,42.2f,47.f,C+2.65f,C+2.9f,13);
  // --- Units -----------------------------------------------------------------------------
  mhUnit(hall,2.5f,13.f,1,17.f,7.f);mhUnit(hall,26.5f,13.f,2,40.f,29.f);mhUnit(hall,2.5f,37.f,3,-1.f,16.f);
  // Main steam to Units 1 and 3 from the west wall, to Unit 2 overhead from the north.
  for(float dy:{-.1f,1.1f}){
   pipeRoute(m,{{1.f,13.f+dy,-21.f},{4.f,13.f+dy,-21.f},{4.f,13.f+dy,G+2.48f}},.28f,5,NoRoof);
   gateValve(m,{2.6f,13.f+dy,-21.f},{1,0},.28f,F,dy>0?1:-1);
   // Unit 3's steam is cut and blanked at a stand while its casings are open.
   pipeRoute(m,{{1.f,37.f+dy,-21.f},{2.4f,37.f+dy,-21.f}},.28f,5,NoRoof);
   wall(m,2.15f,37.f+dy-.25f,2.55f,37.f+dy+.25f,G,-21.32f,12);
   gateValve(m,{1.7f,37.f+dy,-21.f},{1,0},.28f,F,dy>0?1:-1);
  }
  // Unit 2's steam comes over the north gallery, hung from the lighting rig.
  for(float dx:{-.6f,.6f}){float x=28.6f+dx;
   pipeRoute(m,{{x,1.f,-20.5f},{x,12.5f,-20.5f},{x,12.5f,G+2.48f}},.28f,5,NoRoof);
   m.pipes.push_back({{x,8.f},{x,8.f},-20.2f,.03f,mhLampRig,6});
   wall(m,x-.3f,1.f,x+.3f,1.5f,-21.f,-20.78f,12);
  }
  // Platform stairs: each table is reached from the floor on its south side.
  m.stairs.push_back({7.f,17.f,9.f,21.2f,F,G,15,true,false});
  m.stairs.push_back({29.f,17.f,31.f,21.2f,F,G,15,true,false});
  m.stairs.push_back({16.f,41.f,18.f,45.2f,F,G,15,true,false});
  // Unit numbers on the walls.
  mhHallFloor(hall);mhHallLight(hall);mhHallScript(hall);
  return hall;}();
 return built;
}

// Copies the hall's quarter (qx, qy) into a chunk: boxes and pipe runs are
// cut at the seams, everything else goes where its centre lies.
void mhTakeQuarter(Map& m,int qx,int qy){
 const MhHall& hall=mhHall();const Map& h=hall.h;
 const float ox=qx*24.f,oy=qy*24.f;
 auto inside=[&](Vec2 p){return p.x>=ox&&p.x<ox+24&&p.y>=oy&&p.y<oy+24;};
 auto local=[&](Vec2 p){return Vec2{p.x-ox,p.y-oy};};
 for(auto s:h.structures){float x1=std::max(s.x1,ox),x2=std::min(s.x2,ox+24),y1=std::max(s.y1,oy),y2=std::min(s.y2,oy+24);
  if(x2-x1<.002f||y2-y1<.002f)continue;s.x1=x1-ox;s.x2=x2-ox;s.y1=y1-oy;s.y2=y2-oy;m.structures.push_back(s);}
 for(auto f:h.fixtures){
  if(inside(f.position)){f.position=local(f.position);m.fixtures.push_back(f);continue;}
  // A solid fixture reaching over the border from a neighbouring quarter
  // leaves an invisible collision copy here.
  float r=std::max(f.width,f.depth)*.5f;
  if(f.solid&&f.position.x+r>ox&&f.position.x-r<ox+24&&f.position.y+r>oy&&f.position.y-r<oy+24){f.position=local(f.position);f.visible=false;m.fixtures.push_back(f);}
 }
 auto clipBox=[&](auto& b)->bool{float x1=std::max(b.x1,ox),x2=std::min(b.x2,ox+24),y1=std::max(b.y1,oy),y2=std::min(b.y2,oy+24);
  if(x2-x1<.01f||y2-y1<.01f)return false;b.x1=x1-ox;b.x2=x2-ox;b.y1=y1-oy;b.y2=y2-oy;return true;};
 for(auto z:h.zones)if(clipBox(z))m.zones.push_back(z);
 for(auto z:h.hazards)if(clipBox(z))m.hazards.push_back(z);
 for(auto z:h.darkZones)if(clipBox(z))m.darkZones.push_back(z);
 for(auto d:h.doors)if(inside({(d.left+d.right)*.5f,d.y})){d.left-=ox;d.right-=ox;d.y-=oy;m.doors.push_back(d);}
 for(auto p:h.pipes){
  Vec2 a=p.start,b=p.end,d=b-a;
  if(lengthSq(d)<1e-8f){if(inside(a)){p.start=p.end=local(a);m.pipes.push_back(p);}continue;}
  float t0=0,t1=1;bool keep=true;
  auto clip=[&](float q,float r){if(std::fabs(q)<1e-9f){if(r<0)keep=false;return;}float t=r/q;if(q<0){if(t>t1)keep=false;else if(t>t0)t0=t;}else{if(t<t0)keep=false;else if(t<t1)t1=t;}};
  clip(-d.x,a.x-ox);clip(d.x,ox+24-a.x);clip(-d.y,a.y-oy);clip(d.y,oy+24-a.y);
  if(!keep||t1-t0<1e-4f)continue;
  PipeRun q=p;q.start=local(a+d*t0);q.end=local(a+d*t1);
  if(p.endZ>-999.f){q.z=p.z+(p.endZ-p.z)*t0;q.endZ=p.z+(p.endZ-p.z)*t1;}
  m.pipes.push_back(q);
 }
 for(auto l:h.lights)if(inside(l.position)){l.position=local(l.position);m.lights.push_back(l);}
 for(auto s:h.signs)if(inside(s.position)){s.position=local(s.position);m.signs.push_back(s);}
 for(auto t:h.terminals)if(inside(t.position)){t.position=local(t.position);m.terminals.push_back(t);}
 for(auto p:h.props)if(inside(p.position)){p.position=local(p.position);m.props.push_back(p);}
 for(auto s:h.stairs)if(inside({(s.x1+s.x2)*.5f,(s.y1+s.y2)*.5f})){s.x1-=ox;s.x2-=ox;s.y1-=oy;s.y2-=oy;m.stairs.push_back(s);}
 for(auto c:h.clutterSpawns)if(inside(c.position)){c.position=local(c.position);m.clutterSpawns.push_back(c);}
 for(auto c:h.pickupSpawns)if(inside(c.position)){c.position=local(c.position);m.pickupSpawns.push_back(c);}
 for(auto c:h.creatureSpawns)if(inside(c.position)){c.position=local(c.position);m.creatureSpawns.push_back(c);}
 for(const auto& d:hall.decks){int x1=std::max(d.x1,qx*24),x2=std::min(d.x2,qx*24+24),y1=std::max(d.y1,qy*24),y2=std::min(d.y2,qy*24+24);
  if(x2>x1&&y2>y1)deck(m,d.z,x1-qx*24,y1-qy*24,x2-qx*24,y2-qy*24);}
}
// Every hall chunk starts the same way: concrete floor, open seams to its three
// neighbours, the hall's quarter, and Load Control kept in view.
void mhHallChunk(Map& m,int index,int qx,int qy){
 m.name=CampaignMapNames[size_t(index)];m.definition=CampaignChunks[size_t(index)];
 m.definition.ambient=.24f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceCorrugated;
 m.visibleResidencyGroups={12};
 m.layers.push_back(floorLayer(m.name.c_str(),mhFloor));
 // Seams into the rest of the hall, exactly as planned (1-23 on each inner side).
 openEdge(m,{qx==0?'E':'W',1.f,23.f,mhFloor,false,0});
 openEdge(m,{qy==0?'S':'N',1.f,23.f,mhFloor,false,0});
 // The hall's centre stands open: no stub of wall where the four quarters meet.
 rectangle(m.layers[0],qx==0?23:0,qy==0?23:0,qx==0?24:1,qy==0?24:1,'.');
 mhTakeQuarter(m,qx,qy);
}

// MACHINE HALL / NORTH GALLERY (manifest 25)
std::shared_ptr<const Map> makeMachineHallNorthGallery(){
 auto p=std::make_shared<Map>();auto& m=*p;
 mhHallChunk(m,41,0,0);
 openEdge(m,{'N',10.f,14.f,mhTable,true,0});
 // Load Control's observation window, high in the west wall: the room the
 // player energized Feeder 9 from looks down into the hall. Sealed glazing,
 // the wall made up below and above the opening.
 rectangle(m.layers[0],0,13,1,17,'.');
 wall(m,0.f,13.f,1.f,17.f,mhFloor,-18.1f,15);wall(m,0.f,13.f,1.f,17.f,-16.1f,mhRoof,15);
 wall(m,.02f,13.f,.06f,17.f,-18.1f,-16.1f,17);
 for(float y:{13.f,14.3f,15.6f,16.88f})wall(m,0.f,y,1.f,y+.12f,-18.1f,-16.1f,12);
 wall(m,.9f,12.85f,1.12f,17.15f,-18.35f,-18.1f,12);wall(m,.9f,12.85f,1.12f,17.15f,-16.1f,-15.85f,12);
 // The high-bay restrike: when the player first reaches the gallery rail the
 // four lighting contactors close one after another, north to south, and the
 // hall comes up out of the dark row by row (chunk 41 holds the hall's
 // origin, so its local coordinates are hall coordinates).
 const char* timers[4]={"mh_bay_a_ms","mh_bay_b_ms","mh_bay_c_ms","mh_bay_d_ms"};
 const StateId rows[4]={MhBayA,MhBayB,MhBayC,MhBayD};
 for(int i=0;i<4;++i){
  TimedSequence s;s.timerState=stateId(timers[i]);s.finishState=rows[i];
  s.x1=5.f;s.y1=2.6f;s.x2=20.f;s.y2=4.f;s.bottom=-25.5f;s.top=-23.f;
  s.durationMs=8000;s.finishAtMs=900+i*1800;s.soundIntervalMs=0;m.timedSequences.push_back(s);
  mhCue(m,timers[i],s.finishAtMs-1,Sound::Machine,{24.f,8.f+i*12.f},mhLampRig,1.f,.55f);
  mhCue(m,timers[i],s.finishAtMs+250,Sound::Metal3,{8.f+i*9.f,8.f+i*12.f},mhLampRig,.5f,.7f);
 }
 mhCue(m,"mh_bay_a_ms",120,Sound::Metal1,{7.8f,1.6f},mhTable+1.f,.8f,.6f,"LIGHTING L-9: CONTACTOR ROW A");
 freightFinish(m,-1);
 titleCard(m,"MACHINE HALL");
 return p;
}
// MACHINE HALL / EXCITER BAY (manifest 25)
std::shared_ptr<const Map> makeMachineHallExciterBay(){
 auto p=std::make_shared<Map>();auto& m=*p;
 mhHallChunk(m,42,1,0);
 openEdge(m,{'E',8.f,12.f,mhTable,false,0});
 // K. Voss, the operator who stayed at his desk when the Cleanup Force
 // cleared the hall.
 machineryClock(m,"mh_cr_clock",60000);
 mhBody(m,"mh_cr_clock",{16.1f,5.f},mhTable,2.2f);
 mhBlood(m,{15.9f,5.15f},mhTable+.012f,.5f);
 // Unit 2's shaft turns with no steam on it: a slow groan from the generator.
 machineryClock(m,"mh_u2_turn_ms",2900);
 mhCue(m,"mh_u2_turn_ms",200,Sound::LiftCreak,{18.5f,13.f},mhTable+2.f,.3f,.35f);
 // Closing board 9-14 brings the vent works' control supply back.
 {auto& e=mhEvent(m,"mh_ev_vent_supply",12.f,1.f,20.f,7.f,-25.5f,-22.f,MhVentSupply);e.actions={mhSound(Sound::Machine,.8f),mhShake(.06f),mhSave()};}
 freightFinish(m,-1);
 return p;
}
// MACHINE HALL / TURBINE FLOOR (manifest 25)
std::shared_ptr<const Map> makeMachineHallTurbineFloor(){
 auto p=std::make_shared<Map>();auto& m=*p;
 mhHallChunk(m,43,0,1);
 // The blade inspector, shot at his bench.
 machineryClock(m,"mh_tf_clock",60000);
 mhBody(m,"mh_tf_clock",{7.9f,21.7f},mhFloor,1.3f);
 mhBlood(m,{7.7f,21.8f},mhFloor,.6f);
 freightFinish(m,-1);
 return p;
}
// MACHINE HALL / CRANE END (manifest 25)
std::shared_ptr<const Map> makeMachineHallCraneEnd(){
 auto p=std::make_shared<Map>();auto& m=*p;
 mhHallChunk(m,44,1,1);
 openEdge(m,{'S',14.f,18.f,mhCatwalk,false,0});
 // FD-9 resets as the player comes along the catwalk once its supply is back.
 {auto& e=mhEvent(m,"mh_ev_damper",11.f,19.6f,21.f,22.7f,-22.5f,-20.f,MhVentSupply);e.actions={mhOpen(0),mhSound(Sound::Door,.9f),mhSave()};}
 freightFinish(m,-1);
 return p;
}
// VENTILATION WORKS (manifest 26). Where the plant breathes. Two supply fans
// in casings the size of a van stand on their plinths; each blows up a round
// riser into the header that runs under the roof and drops into the supply
// main, a duct big enough to walk through, which is the way on to Air
// Handling. The scrubber plant lines the south wall. The player only gets
// here once board 9-14 is closed in the Machine Hall, so the fans are running.
std::shared_ptr<const Map> makeVentilationWorks(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[45];m.definition=CampaignChunks[45];m.definition.ambient=.28f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-22,roof=m.definition.ceiling,dz=f+2.6f,hd=f+7.2f,ht=f+8.6f;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',14.f,18.f,-22.f,false,0});openEdge(m,{'W',4.f,8.f,-22.f,false,0});
 wall(m,14.f,0.f,18.f,1.f,f+3.f,roof,15);
 wallBays(m,f,roof,4.f,true,false,false,true,16.f);
 // --- The supply main: a walk-through duct from the hall to the west wall ----------------------
 // Panel-steel shell 4.4 m wide and 2.6 m high with stiffener ribs; the
 // access hatch is in its south face at the east end.
 wall(m,0.f,3.6f,11.f,3.8f,f,dz,2);wall(m,0.f,8.2f,9.f,8.4f,f,dz,2);
 wall(m,0.f,3.6f,11.f,8.4f,dz,dz+.2f,2);
 wall(m,10.8f,3.8f,11.f,8.4f,f,dz,2);
 for(float x=1.5f;x<10.5f;x+=1.5f){wall(m,x,3.5f,x+.08f,8.5f,dz+.2f,dz+.3f,8);
  for(float y:{3.5f,8.4f})wall(m,x,y,x+.08f,y+.1f,f,dz+.2f,8);}
 for(float x=2.f;x<10.f;x+=1.f)wall(m,x,5.6f,x+.5f,6.4f,f,f+.02f,8);
 wall(m,9.f,8.2f,10.8f,8.4f,f+2.f,dz,2);
 mhFix(m,model::pwr_vent_grille,9.9f,8.45f,f+2.05f,.82f,.04f,.5f,0,false);
 wallSign(m,7.6f,8.455f,f+1.5f,"DANGER / SUPPLY MAIN","LOCK OUT FAN 2 BEFORE ENTRY",1.6f,0);
 mhLamp(m,{5.f,6.f},dz-.1f,.8f,5.f,3400,0);
 // --- Fan 1 and Fan 2 -----------------------------------------------------------------------
 // Casing on a plinth, a transition piece, and a round riser to the header.
 for(Vec2 c:{Vec2{16.5f,11.f},Vec2{16.5f,18.5f}}){
  wall(m,c.x-2.6f,c.y-2.3f,c.x+3.8f,c.y+2.3f,f,f+.4f,15);
  for(float y:{c.y-2.7f,c.y+2.62f})floorLine(m,c.x-3.f,y,c.x+4.2f,y+.08f,f,13);
  mhFix(m,model::pwr_turbine_housing,c.x,c.y,f+.4f,4.f,4.f,2.91f,0,true);
  wall(m,c.x+2.f,c.y-.55f,c.x+2.6f,c.y+.55f,f+.9f,f+2.5f,2);
  vessel(m,{c.x+2.9f,c.y},.75f,f+.4f,hd,5);
  mhFix(m,model::pwr_circulation_pump,c.x-1.4f,c.y+2.75f,f,.84f,.5f,.39f,0,true);
 }
 // The header under the roof: a branch over the risers, the main west to the
 // supply main, and its drop onto the main's roof. Rods carry it.
 wall(m,18.7f,5.2f,20.1f,19.25f,hd,ht,2);
 wall(m,9.4f,5.2f,20.1f,6.8f,hd,ht,2);
 wall(m,9.4f,5.2f,10.8f,6.8f,dz+.2f,hd,2);
 for(float x=12.f;x<19.f;x+=3.f)for(float y:{5.3f,6.7f})m.pipes.push_back({{x,y},{x,y},ht,.02f,roof,6});
 for(float y=9.f;y<19.f;y+=3.f)for(float x:{18.8f,20.f})m.pipes.push_back({{x,y},{x,y},ht,.02f,roof,6});
 // --- The scrubber plant along the south wall ----------------------------------------------------
 // Five air-handling units on a pad, their collector over them and its riser.
 equipmentPad(m,1.2f,21.7f,11.8f,23.f,f);
 for(int i=0;i<5;++i)mhFix(m,model::pwr_air_handler,2.f+i*2.2f,22.4f,f+.1f,.93f,1.89f,3.01f,kPi*.5f,true);
 wall(m,1.f,21.6f,11.8f,23.f,f+3.2f,f+4.3f,2);
 wall(m,1.f,21.6f,2.4f,23.f,f+4.3f,roof,2);
 // --- The fan control desk ------------------------------------------------------------------------
 mhFix(m,model::pwr_control_cabinet,21.6f,3.f,f,.8f,.4f,1.6f,-kPi*.5f,true);
 mhTerminal(m,{20.f,4.2f},f,kPi*.5f,"VENT WORKS / FAN CONTROL","FAN 1 RUNNING ON BOARD 9-14 SUPPLY. FAN 2 STANDBY.","EXHAUST ON MANUAL SINCE 23:58. AIR HANDLING: WEST, THROUGH THE MAIN.");
 mhTerminal(m,{2.f,12.f},f,kPi*.5f,"VENT WORKS / DEFECT LOG","DAMPER 6, AIR HANDLING: SOMETHING NESTING. FITTER REFUSED THE JOB.","SUPERVISOR: CLEAR IT. FITTER: NOT FOR THIS MONEY. 23:40.");
 mhFix(m,model::pwr_fire_cabinet,12.f,1.33f,f+1.1f,.25f,.2f,.56f,0,false);
 for(Vec2 b:{Vec2{2.2f,9.6f},Vec2{2.95f,9.35f}})mhFix(m,model::ind_barrel,b.x,b.y,f,.53f,.56f,.95f,b.x,true);
 handCart(m,13.4f,21.6f,f);
 // --- Wasps in the plant; one woken by the fans ---------------------------------------------------
 m.creatureSpawns.push_back({CreatureKind::Wasp,{7.f,15.f},f});
 {CreatureSpawn w{CreatureKind::Wasp,{21.f,21.8f},f};w.wakeState=MhVentSupply;m.creatureSpawns.push_back(w);}
 m.pickupSpawns.push_back({{2.f,18.f},PickupKind::Ammo,f});
 // --- Machinery and light ---------------------------------------------------------------------
 {TimedSequence s;s.timerState=stateId("mh_fans_ms");s.finishState=stateId("mh_fans_cycled");s.x1=13;s.y1=9;s.x2=20;s.y2=20.5f;
  s.durationMs=3000;s.finishAtMs=3000;s.soundIntervalMs=750;s.soundUntilMs=3000;s.tickSound=Sound::Machine;s.gain=.6f;s.pitch=.45f;s.loop=true;m.timedSequences.push_back(s);
  auto& e=mhEvent(m,"mh_ev_fans",0,0,24,24,-100,100,MhVentSupply);e.actions={mhSet(stateId("mh_fans_ms"))};}
 // Work lights hung on rods; floods on the east pilasters; one failing over the scrubbers.
 for(Vec2 l:{Vec2{14.f,3.f},Vec2{5.f,11.5f},Vec2{5.f,17.f},Vec2{11.5f,14.5f},Vec2{21.5f,10.f},Vec2{21.5f,17.f}})mhLamp(m,l,f+5.5f,2.2f,12.f,4600,.05f);
 for(float y:{8.f,20.f})mhLamp(m,{22.76f,y},f+4.f,1.6f,10.f,3400,-.1f,WorldLightMount::Wall,kPi);
 mhLamp(m,{8.f,20.f},f+5.5f,1.4f,8.f,4300,.1f,WorldLightMount::Ceiling,0,0,1,false,.45f);
 zone(m,"VENTILATION WORKS / SUPPLY MAIN",0,3.6f,11,8.4f);
 zone(m,"VENTILATION WORKS",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// AIR HANDLING (manifest 27). The quietest map so far. The supply main
// breaks up into plenums and low ducts under the slab: two plenum chambers,
// a filter room, low walkways you stoop through, and true crawlspaces off to
// the side. Something lives in Damper 6. It is seen once, through a grille,
// and heard breathing. It comes for the player in the last plenum.
std::shared_ptr<const Map> makeAirHandling(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[46];m.definition=CampaignChunks[46];m.definition.ambient=.2f;
 m.wallSurface=SurfacePressure;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfacePressure;
 const float f=-22,roof=m.definition.ceiling,low=f+1.85f,crawl=f+1.f;
 const StateId wake=stateId("mh_air_wake");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 auto& L=m.layers[0];
 rectangle(L,1,1,23,23,'#');
 rectangle(L,16,2,23,10,'.');   // plenum 1, east
 rectangle(L,10,4,16,8,'.');    // low duct west
 rectangle(L,4,3,10,12,'.');    // filter room
 rectangle(L,6,12,9,16,'.');    // low duct south
 rectangle(L,2,16,13,23,'.');   // plenum 2, the last
 rectangle(L,18,10,21,18,'.');  // crawl south from plenum 1
 rectangle(L,13,17,21,21,'.');  // the nest chamber
 rectangle(L,6,1,9,3,'.');      // crawl north from the filter room
 openEdge(m,{'E',4.f,8.f,-22.f,false,0});openEdge(m,{'S',4.f,8.f,-22.f,false,0});
 // --- Low ducts on the route: stoop height, panel steel ------------------------------------------
 wall(m,10.f,4.f,16.f,8.f,low,roof,2);
 wall(m,6.f,12.f,9.f,16.f,low,roof,2);
 for(float x=10.6f;x<16.f;x+=1.2f)wall(m,x,4.f,x+.06f,8.f,low-.08f,low,8);
 for(float y=12.6f;y<16.f;y+=1.2f)wall(m,6.f,y,9.f,y+.06f,low-.08f,low,8);
 // --- Crawlspaces off the route ------------------------------------------------------------------
 wall(m,18.f,10.f,21.f,17.f,crawl,roof,2);
 wall(m,6.f,1.f,9.f,3.f,crawl,roof,2);
 wall(m,13.f,17.f,14.f,21.f,crawl,roof,2);
 // --- Plenum 1: the turning vanes, the damper banks ------------------------------------------------
 for(float y=2.6f;y<9.6f;y+=.9f)wall(m,22.4f,y,22.9f,y+.06f,f+.2f,roof-.2f,8);
 // Fire dampers in the north wall: framed louvre banks, floor to roof.
 for(float x:{17.f,19.f}){wall(m,x,2.f,x+.08f,2.3f,f,roof,2);wall(m,x+1.12f,2.f,x+1.2f,2.3f,f,roof,2);
  for(float z=f+.3f;z<roof-.3f;z+=.18f)wall(m,x+.08f,2.1f,x+1.12f,2.2f,z,z+.04f,8);}
 mhFix(m,model::pwr_duct_fan_unit,21.6f,9.f,f,.79f,1.49f,1.95f,-kPi*.5f,true);
 // --- The filter room: filter racks, the grille, the puppet ---------------------------------------
 // Filter banks: two rows of air-handling units and a one-metre aisle.
 for(float x:{5.2f,7.f})for(float y=4.6f;y<10.f;y+=1.6f)mhFix(m,model::pwr_air_handler,x,y,f,.75f,1.53f,2.44f,0,true);
 mhFix(m,model::metal_shelf,9.4f,10.6f,f,1.8f,.5f,1.8f,-kPi*.5f,true);
 mhTerminal(m,{8.f,4.6f},f,0,"AIR HANDLING / FILTER ROOM","PRE-FILTERS DUE 22:00. NOT DONE.","SCRATCHING IN DAMPER 6 SINCE 23:00. NOBODY GOES IN ALONE.");
 // The thing in the duct. Standing at the mouth of the low duct the player
 // looks straight down it into the filter room, and something crosses the
 // far end, out of the north crawl and down towards plenum 2. Then it is gone.
 {TimedSequence s;s.timerState=stateId("mh_air_seen");s.finishState=stateId("mh_air_seen_done");s.x1=16.f;s.y1=3.f;s.x2=19.f;s.y2=9.f;s.bottom=f-1;s.top=f+3;
  s.durationMs=5000;s.finishAtMs=4500;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);}
 actor(m,"mh_air_seen",ActorVisual::Huntsman,1.f,{{0,{7.6f,1.8f},f,kPi*.5f},{900,{8.9f,4.4f},f,kPi*.5f},{2000,{8.9f,8.6f},f,kPi*.5f},{2900,{7.6f,11.8f},f,kPi*.6f},{3500,{7.6f,14.6f},f,kPi*.5f},{3600,{7.6f,14.6f},f-3.f,kPi*.5f},{5000,{7.6f,14.6f},f-3.f,kPi*.5f}});
 cue(m,"mh_air_seen",600,Sound::SpiderCall,{8.9f,6.f},"",.45f,f+.5f);
 // --- Breathing in the walls ----------------------------------------------------------------
 {TimedSequence s;s.timerState=stateId("mh_air_breath");s.finishState=stateId("mh_air_breath_cycled");s.x1=2.f;s.y1=12.f;s.x2=13.f;s.y2=23.f;s.bottom=f-1;s.top=f+3;
  s.durationMs=6000;s.finishAtMs=6000;s.soundIntervalMs=3000;s.soundUntilMs=6000;s.tickSound=Sound::Wings;s.gain=.25f;s.pitch=.35f;s.loop=true;m.timedSequences.push_back(s);}
 // --- The nest chamber (crawl in from plenum 1) ---------------------------------------------------
 mhFix(m,model::cave_web_corner,20.4f,17.4f,f+1.f,1.36f,.02f,.8f,kPi*.25f,false);
 mhBody(m,"mh_air_body_clock",{16.f,19.f},f,1.2f);machineryClock(m,"mh_air_body_clock",60000);
 mhBlood(m,{16.2f,19.2f},f,.5f);
 mhFix(m,model::mine_hard_hat,17.2f,18.2f,f,.28f,.35f,.15f,.6f,false);
 m.pickupSpawns.push_back({{19.6f,19.6f},PickupKind::Health,f});
 m.pickupSpawns.push_back({{7.4f,2.f},PickupKind::Ammo,f});
 // --- Plenum 2: it comes when the player is halfway across -------------------------------------
 for(float x:{4.5f,10.5f})steelColumn(m,x,19.5f,f,roof,true);
 mhFix(m,model::pwr_duct_fan_unit,2.42f,18.5f,f,.79f,1.49f,1.95f,0,true);
 mhFix(m,model::pwr_air_handler,11.5f,21.9f,f,.75f,1.53f,2.44f,kPi*.5f,true);
 {CreatureSpawn h{CreatureKind::Huntsman,{12.2f,18.2f},f};h.wakeState=wake;m.creatureSpawns.push_back(h);}
 {auto& e=mhEvent(m,"mh_air_ambush",2.f,17.f,9.f,23.f,f-1,f+3);e.actions={mhSet(wake),mhSound(Sound::SpiderCall,.7f)};}
 // --- Light: almost none ------------------------------------------------------------------------
 mhLamp(m,{19.5f,6.f},roof-.15f,.8f,5.f,2600,-.1f,WorldLightMount::Ceiling,0,0,0,false);
 m.lights.back().flicker=.5f;
 mhLamp(m,{8.6f,7.f},roof-.15f,1.1f,6.f,4300,.3f);m.lights.back().flicker=.2f;
 mhLamp(m,{13.6f,6.f},low-.08f,.5f,3.5f,5000,0,WorldLightMount::Ceiling,0,0,1,false,.35f);
 mhLamp(m,{7.5f,14.4f},low-.08f,.45f,3.5f,5000,0,WorldLightMount::Ceiling,0,0,1,false,.3f);
 mhLamp(m,{4.f,17.2f},roof-.15f,.5f,4.f,4300,.1f);
 mhLamp(m,{7.f,19.5f},roof-.15f,.6f,5.f,1150,-.4f);
 mhLamp(m,{17.f,19.f},roof-.15f,.4f,3.f,1150,-.4f);
 zone(m,"AIR HANDLING / FILTER ROOM",4,3,10,12);
 zone(m,"AIR HANDLING / DAMPER 6",13,17,21,21);
 zone(m,"AIR HANDLING",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
}
void buildChapterMachineHall(CampaignMapTable& maps){
 maps[41]=makeMachineHallNorthGallery();
 maps[42]=makeMachineHallExciterBay();
 maps[43]=makeMachineHallTurbineFloor();
 maps[44]=makeMachineHallCraneEnd();
 maps[45]=makeVentilationWorks();
 maps[46]=makeAirHandling();
}
}

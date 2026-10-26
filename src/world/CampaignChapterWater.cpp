// WATER AND COOLING (chapter 1, manifest 28-32): chunks 47-52.
// The plant's water: treatment, drainage, the sump pumps and Tower 2. The rule
// taught here is resonance's first: it pools where metal is wet and still, and
// live water hums before it bites. Tower 2's basin is live (pale motes over
// arcing water) until MCC-2 in the Sump Works is isolated (water_isolated):
// the hub shows the problem, the spoke west and south fixes it, and the way on
// is back through the tower and up its shell stair to Heat Exchange.
// See docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <string>
#include <vector>
#include <array>
namespace retro {
namespace {
using namespace kit;
using C=CreatureKind;using P=PickupKind;

// --- Water chapter kit ---------------------------------------------------------
// MCC-2 in the Sump Works feeds everything live in this chapter: Tower 2's
// recirculation pumps, drain pump P-4, Reservoir 2's P-7 and the sump lights.
const StateId watIsolated=stateId("water_isolated");

void watSurfaces(Map& m,int walls,int floor,int ceiling){m.wallSurface=walls;m.floorSurface=floor;m.ceilingSurface=ceiling;}
// A lamp that follows a state: lit while state==1 (or, inverted, until it is).
WorldLight watSwitched(WorldLight l,StateId state,bool invert,float flicker=0){l.enabledState=state;l.enabledValue=1;l.invert=invert;l.flicker=flicker;return l;}
WorldLight watFlicker(WorldLight l,float flicker){l.flicker=flicker;return l;}
WorldLight watWall(Vec2 p,float z,float yaw,float intensity,float range,float kelvin,float tint=0){return lamp(p,z,intensity,range,kelvin,tint,WorldLightMount::Wall,yaw);}

ScriptEvent watEvent(const char* id,float x1,float y1,float x2,float y2,float bottom,float top,StateId require=0,int value=1){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=bottom;e.top=top;e.requireState=require;e.requireValue=value;return e;}
ScriptAction watSet(StateId id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=id;a.value=value;return a;}
ScriptAction watSpawn(CreatureKind kind,Vec2 p,float z,float awareness=6){ScriptAction a;a.type=ScriptAction::Type::SpawnEnemy;a.enemyKind=kind;a.position=p;a.z=z;a.amount=awareness;return a;}
ScriptAction watSound(Sound s,float gain){ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=s;a.amount=gain;return a;}
ScriptAction watShake(float amount){ScriptAction a;a.type=ScriptAction::Type::Shake;a.amount=amount;return a;}
ScriptAction watCheckpoint(){ScriptAction a;a.type=ScriptAction::Type::Checkpoint;return a;}

// The hum of a live pool: AnomalyHum every 10 s (the loop's length) from the
// centre of the box, for an hour once the player first stands in the box.
// Isolation pushes the clock past soundUntil, which silences it for good.
constexpr int WatHumUntil=3000000,WatHumQuiet=3100000;
void watHum(Map& m,const char* timer,float x1,float y1,float x2,float y2,float bottom,float top,float gain=.6f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_done");
 s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;s.bottom=bottom;s.top=top;s.durationMs=3600000;s.finishAtMs=3600000;
 s.soundIntervalMs=10000;s.soundUntilMs=WatHumUntil;s.tickSound=Sound::AnomalyHum;s.gain=gain;s.pitch=1;m.timedSequences.push_back(s);
}
// When MCC-2 is isolated, each listed hum falls silent the next time the
// player stands anywhere in this chunk.
void watQuiet(Map& m,const char* id,std::initializer_list<const char*> timers){
 auto e=watEvent(id,0,0,24,24,-100,100,watIsolated);for(auto t:timers)e.actions.push_back(watSet(stateId(t),WatHumQuiet));m.scriptEvents.push_back(e);
}
// Live water. Pale motes stand over the pool and a little beyond its edge
// (Anomaly: the tingle that warns), and arcs skim its surface (Electricity:
// the bite). Both are gone once MCC-2 is isolated.
void watLiveWater(Map& m,float x1,float y1,float x2,float y2,float surface,float bite,float margin=.5f){
 for(float y=y1;y<y2-.05f;y+=1.f){
  int cells=std::max(1,int(std::round((x2-x1)/4.f)));
  for(int i=0;i<cells;++i){Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=x1+(x2-x1)*i/cells;h.x2=x1+(x2-x1)*(i+1)/cells;h.y1=y;h.y2=std::min(y2,y+1.f);
   h.bottom=surface-.43f;h.top=surface+2.f;h.damagePerSecond=bite;h.enabledFlag=watIsolated;h.invertFlag=true;m.hazards.push_back(h);}
 }
 const float mx1=x1-margin,my1=y1-margin,mx2=x2+margin,my2=y2+margin;
 // Sparse cells: each draws 18 motes, so a dense grid over a basin floods the view.
 int nx=std::max(1,int(std::round((mx2-mx1)/7.f))),ny=std::max(1,int(std::round((my2-my1)/7.f)));
 for(int j=0;j<ny;++j)for(int i=0;i<nx;++i){Hazard h;h.kind=Hazard::Kind::Anomaly;
  h.x1=mx1+(mx2-mx1)*i/nx;h.x2=mx1+(mx2-mx1)*(i+1)/nx;h.y1=my1+(my2-my1)*j/ny;h.y2=my1+(my2-my1)*(j+1)/ny;
  h.bottom=surface;h.top=surface+2.2f;h.damagePerSecond=2.5f;h.enabledFlag=watIsolated;h.invertFlag=true;m.hazards.push_back(h);}
}
// A glazed partition from z: dado, glass with mullions, header.
void watGlazed(Map& m,float x1,float y1,float x2,float y2,float z,float h=2.7f){
 wall(m,x1,y1,x2,y2,z,z+.95f,3);wall(m,x1,y1,x2,y2,z+.95f,z+h-.35f,17);wall(m,x1,y1,x2,y2,z+h-.35f,z+h,3);
 bool alongX=x2-x1>y2-y1;float len=alongX?x2-x1:y2-y1;int n=std::max(1,int(len/1.5f+.5f));
 for(int i=1;i<n;++i){float t=(alongX?x1:y1)+len*i/n;
  if(alongX)wall(m,t-.03f,y1-.01f,t+.03f,y2+.01f,z+.95f,z+h-.35f,12);else wall(m,x1-.01f,t-.03f,x2+.01f,t+.03f,z+.95f,z+h-.35f,12);}
}
// One straight run of tubular guard rail: posts, two rails and a solid core.
void watRailRun(Map& m,Vec2 a,Vec2 b,float floor,float top=1.1f,int paint=3){
 Vec2 d=b-a;float len=std::sqrt(d.x*d.x+d.y*d.y);int n=std::max(1,int(std::ceil(len/1.6f)));
 for(int i=0;i<=n;++i){Vec2 q=a+d*(float(i)/n);m.pipes.push_back({q,q,floor,.03f,floor+top,paint});}
 for(float z:{floor+top,floor+top*.55f})m.pipes.push_back({a,b,z,.025f,z,paint});
 m.structures.push_back({std::min(a.x,b.x)-.05f,std::min(a.y,b.y)-.05f,std::max(a.x,b.x)+.05f,std::max(a.y,b.y)+.05f,floor,floor+top,false,6});
 // Nobody climbs a pump riser onto the rail and over: the core stands higher.
 m.structures.push_back({std::min(a.x,b.x)-.05f,std::min(a.y,b.y)-.05f,std::max(a.x,b.x)+.05f,std::max(a.y,b.y)+.05f,floor+top,floor+2.6f,false,CollisionProxyMaterial});
}
// A sunk basin with a coping kerb, hazard paint at its lip and a rail round it.
void watBasin(Map& m,float x1,float y1,float x2,float y2,float floor,float bed,float surface,bool fenced){
 m.waterVolumes.push_back({x1,y1,x2,y2,bed,surface});
 for(float x:{x1-.25f,x2})wall(m,x,y1-.25f,x+.25f,y2+.25f,bed-.1f,floor+.16f,15);
 for(float y:{y1-.25f,y2})wall(m,x1,y,x2,y+.25f,bed-.1f,floor+.16f,15);
 floorLine(m,x1-.25f,y1-.4f,x2+.25f,y1-.28f,floor,13);floorLine(m,x1-.25f,y2+.28f,x2+.25f,y2+.4f,floor,13);
 if(fenced){const float rx1=x1-.12f,rx2=x2+.12f,ry1=y1-.12f,ry2=y2+.12f,f=floor+.16f;
  watRailRun(m,{rx1,ry1},{rx2,ry1},f,1.4f);watRailRun(m,{rx1,ry2},{rx2,ry2},f,1.4f);
  watRailRun(m,{rx1,ry1},{rx1,ry2},f,1.4f);watRailRun(m,{rx2,ry1},{rx2,ry2},f,1.4f);}
}
// A vertical riser through walkway height: its run, flanges and a solid core.
void watRiser(Map& m,Vec2 c,float z1,float z2,float r,int paint){
 m.pipes.push_back({c,c,z1,r,z2,paint});
 for(float z:{z1+.02f,z2-.06f})m.pipes.push_back({c,c,z,r*1.3f,z+.05f,6});
 m.structures.push_back({c.x-r-.04f,c.y-r-.04f,c.x+r+.04f,c.y+r+.04f,z1,z2,false,6});
}
// A dead worker, the timer his only clock.
void watBody(Map& m,const char* clock,Vec2 p,float z,float yaw,ActorVisual visual=ActorVisual::Worker,float scale=1.7f){
 machineryClock(m,clock,60000);
 actor(m,clock,visual,scale,{{0,p,z,yaw,4,1},{60000,p,z,yaw,4,1}},true);
}
// Every live pool's hum timer in the chapter: isolating MCC-2 silences them all.
const char* const WatHumTimers[]={"wat_reservoir_hum_ms","wat_culvert_hum_ms","wat_sump_hum_ms","wat_basin_hum_ms","wat_shell_hum_ms"};
// The hum's first loop plays a second after the player reaches the box,
// not after the loop's full ten seconds.
void watHumPrime(Map& m,const char* id,const char* timer,float x1,float y1,float x2,float y2,float bottom,float top){
 auto e=watEvent(id,x1,y1,x2,y2,bottom,top);e.requireState=watIsolated;e.requireValue=0;
 e.actions.push_back(watSet(stateId(timer),9000));m.scriptEvents.push_back(e);
}
// A 1.1 m guard rail along every open edge of the deck tiles at z: the side a
// body could walk off, not the shell wall behind it or a flight's head.
void watDeckRails(Map& m,float z,float top=1.1f){
 const AuthoredLayerData* d=nullptr;for(const auto& l:m.layers)if(l.thickness>0&&std::fabs(l.elevation-z)<.01f)d=&l;
 if(!d)return;
 auto on=[&](int x,int y){return x>=0&&y>=0&&x<24&&y<24&&d->rows[size_t(y)][size_t(x)]=='=';};
 auto blocked=[&](float x,float y){
  for(const auto& s:m.stairs)if(x>s.x1-.05f&&x<s.x2+.05f&&y>s.y1-.05f&&y<s.y2+.05f&&z>std::min(s.top,s.bottom)-.3f&&z<std::max(s.top,s.bottom)+.3f)return true;
  for(const auto& s:m.structures)if(!s.rail&&s.material!=6&&s.material!=CollisionProxyMaterial&&s.bottom<z+.2f&&s.top>z+1.f&&x>s.x1-.02f&&x<s.x2+.02f&&y>s.y1-.02f&&y<s.y2+.02f)return true;
  int tx=int(std::floor(x)),ty=int(std::floor(y));if(tx<0||ty<0||tx>23||ty>23)return true;
  return m.layers.front().rows[size_t(ty)][size_t(tx)]=='#';};
 std::vector<Structure> rails;
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(on(x,y)){
  if(!on(x,y-1)&&!blocked(x+.5f,y-.15f))rails.push_back({float(x),y-.03f,x+1.f,y+.03f,z,z+top,true,2});
  if(!on(x,y+1)&&!blocked(x+.5f,y+1.15f))rails.push_back({float(x),y+.97f,x+1.f,y+1.03f,z,z+top,true,2});
  if(!on(x-1,y)&&!blocked(x-.15f,y+.5f))rails.push_back({x-.03f,float(y),x+.03f,y+1.f,z,z+top,true,2});
  if(!on(x+1,y)&&!blocked(x+1.15f,y+.5f))rails.push_back({x+.97f,float(y),x+1.03f,y+1.f,z,z+top,true,2});
 }
 for(auto& r:rails)m.structures.push_back(r);
}
// --- Tower 2's shell ------------------------------------------------------------
// In plan a stadium 20 m wide across both chunks: straight walls at x 2 and 22,
// ends rounded with radius 7 about (9,cy) and (15,cy). Vertical to the fill
// gallery at -16, then a hyperboloid narrowing to a throat 2.4 m in at +9 and
// flaring a little to the roof.
float watThroatInset(float z){if(z<=-16.f)return 0;float t=(z-9.f)/29.2f;return std::max(0.f,10.f-7.6f*std::sqrt(1.f+t*t));}
// The rounded end of one band of shell from z1 to z2, inset d: half-metre
// strips round both corners and the flat end between. end -1 is the north
// end (face toward +y), +1 the south end.
void watShellEnd(Map& m,float z1,float z2,float d,int end,float cy){
 const float r=7.f-d;
 for(float x=1.f;x<22.99f;x+=.5f){
  const float xc=x+.25f,dx=xc<9.f?9.f-xc:xc>15.f?xc-15.f:0.f;
  float reach=dx>=r?0.f:std::sqrt(r*r-dx*dx);
  if(end<0){if(cy-reach>1.02f)wall(m,x,1.f,x+.5f,cy-reach,z1,z2,15);}
  else if(cy+reach<22.98f)wall(m,x,cy+reach,x+.5f,23.f,z1,z2,15);
 }
}
// The shell above the fill gallery: narrowing bands two metres tall, the side
// walls between y1 and y2 and the chunk's rounded end; a ring beam marks every
// third band.
void watShellUpper(Map& m,int end,float cy,float y1,float y2){
 int band=0;for(float z=-16.f;z<m.definition.ceiling-.01f;z+=2.f,++band){
  const float z2=std::min(z+2.f,m.definition.ceiling),d=watThroatInset(z+1.f);
  wall(m,1.f,y1,2.f+d,y2,z,z2,15);wall(m,22.f-d,y1,23.f,y2,z,z2,15);
  watShellEnd(m,z,z2,d,end,cy);
  if(band%3==1){wall(m,2.f+d,y1,2.12f+d,y2,z,z+.22f,12);wall(m,21.88f-d,y1,22.f-d,y2,z,z+.22f,12);}
 }
}
// Louvred air inlets along a straight base wall (x is the inner face, dir +1
// for the west wall, -1 for the east): slats, frame posts and a sill.
void watLouvres(Map& m,float x,int dir,float y1,float y2){
 const float a=dir>0?x:x-.1f,b=dir>0?x+.1f:x;
 for(float z=-27.5f;z<-25.f;z+=.32f)wall(m,a,y1,b,y2,z,z+.07f,8);
 for(float y=y1;y<=y2+.01f;y+=2.5f)wall(m,dir>0?x:x-.14f,y-.07f,dir>0?x+.14f:x,y+.07f,-27.7f,-24.7f,12);
 wall(m,dir>0?x:x-.16f,y1,dir>0?x+.16f:x,y2,-24.85f,-24.65f,12);
}
// Concrete ribs across a gallery running along x (tunnelRibs runs along y).
void watRibsX(Map& m,float y1,float y2,float floor,float roof,float x1,float x2,float step){
 for(float x=x1;x<x2;x+=step){
  for(float y:{y1,y2-.24f})wall(m,x-.12f,y,x+.12f,y+.24f,floor,roof-.5f,15);
  wall(m,x-.12f,y1,x+.12f,y2,roof-.4f,roof,15);
  wall(m,x-.12f,y1,x+.12f,y1+.7f,roof-.75f,roof-.4f,15);wall(m,x-.12f,y2-.7f,x+.12f,y2,roof-.75f,roof-.4f,15);
 }
}

// ===============================================================================
// WATER TREATMENT (manifest 28). The plant's water hall. Air Handling's vent
// drops onto the north gallery, high over the hall: two sand filters, Reservoir
// 2 behind its rail (its stopped pump still energized, so the water round it
// shimmers and hums) and the transfer pumps sending make-up water east to Tower
// 2. The control room at the gallery's east end holds the mimic: the tower
// basin is live, and the feeder isolator is MCC-2, west and down through the
// Drainage Gallery to the Sump Works. Its lamps change when that is done.
std::shared_ptr<const Map> makeWaterTreatment(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[47];m.definition=CampaignChunks[47];m.definition.ambient=.24f;
 const float F=-25,G=-22,R=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),F));
 watSurfaces(m,SurfaceConcrete,SurfaceConcrete,SurfaceConcrete);
 openEdge(m,{'N',4,8,G,false,0});openEdge(m,{'W',14,18,F,false,0});openEdge(m,{'E',8,12,F,false,0});
 m.definition.playerStart={6.f,2.6f};m.definition.spawnHeight=G;m.definition.spawnYaw=kPi*.5f;m.definition.spawnPitch=-14.f;
 // --- North gallery (-22) and its stair ---------------------------------------------
 deck(m,G,4,0,8,1);deck(m,G,1,1,14,4);deck(m,G,14,1,23,6);
 m.stairs.push_back({1.3f,4,3.3f,8.2f,F,G,15,true,false});
 // The vent mouth from Air Handling: concrete under the deck at the seam, a
 // steel frame on the wall face and a lintel down to Air Handling's roof.
 wall(m,4,0,8,1,F,G-.22f,15);wall(m,4,0,8,.3f,-19.4f,R,15);
 for(float x:{3.8f,8.f})wall(m,x,1.f,x+.2f,1.2f,G,-19.4f,12);
 wall(m,3.8f,1.f,8.2f,1.2f,-19.65f,-19.4f,12);
 guardRail(m,3.3f,3.95f,14.f,4.f,G);
 // --- Control room (east end of the gallery) -------------------------------------
 watGlazed(m,14.f,5.88f,23.f,6.f,G);
 wall(m,14.f,1.f,14.12f,1.4f,G,G+2.7f,3);wall(m,14.f,1.4f,14.12f,3.f,G+2.2f,G+2.7f,3);
 watGlazed(m,14.f,3.f,14.12f,5.88f,G);
 wall(m,14.f,1.f,23.f,6.f,G+2.7f,G+2.85f,2);
 // The mimic: a pale steel board along the north wall, its circuit painted
 // on, the basin lamp red until MCC-2 is isolated and the isolated lamp green.
 wall(m,15.2f,1.f,21.8f,1.12f,G+.85f,G+2.45f,20);
 for(float y:{G+1.25f,G+1.9f})wall(m,15.5f,1.12f,21.5f,1.135f,y,y+.04f,14);
 for(float x:{16.f,18.5f,21.f})wall(m,x,1.12f,x+.04f,1.135f,G+1.25f,G+1.94f,14);
 wallSign(m,18.5f,1.175f,G+1.95f,"COOLING WATER","TOWER 2 BASIN: LIVE / RECIRC P-11 TO P-14: TRIPPED",3.6f,0);
 wall(m,16.05f,1.12f,16.35f,1.16f,G+1.55f,G+1.75f,25);wall(m,20.65f,1.12f,20.95f,1.16f,G+1.55f,G+1.75f,20);
 m.lights.push_back(watSwitched(watWall({16.2f,1.12f},G+1.68f,kPi*.5f,1.1f,3.f,1200,-.4f),watIsolated,true,.15f));
 m.lights.push_back(watSwitched(watWall({20.8f,1.12f},G+1.68f,kPi*.5f,1.1f,3.f,5200,.7f),watIsolated,false));
 terminal(m,{18.5f,2.55f},"TREATMENT / BASIN STATUS","TOWER 2 BASIN LIVE. RECIRC P-11 TO P-14 TRIPPED 01:52.","DO NOT CROSS. ISOLATE MCC-2 AT SUMP WORKS, WEST AND DOWN.",G-F);
 m.terminals.back().yaw=.001f;
 workstation(m,16.f,2.7f,G,-1,1);
 terminal(m,{22.3f,4.4f},"TREATMENT / SHIFT LOG","02:51 PRYOR TO TOWER 2 TO RESET THE RECIRC PUMPS.","03:20 NO RADIO FROM PRYOR. NOT SENDING ANYONE ELSE.",G-F);
 m.terminals.back().yaw=-kPi*.5f;
 for(float y:{1.8f,2.75f})m.fixtures.push_back({model::electrical_cabinet,{22.75f,y},G-F,.9066f,.4956f,2.2f,kPi*.5f,true});
 m.fixtures.push_back({model::lab_pa_horn,{14.6f,1.36f},G-F+2.05f,.49f,.56f,.43f,kPi,false});
 shelf(m,{15.2f,5.5f},0,G-F);
 for(Vec2 b:{Vec2{14.8f,5.45f},Vec2{15.5f,5.45f}})m.fixtures.push_back({model::binder,b,G-F+1.05f,.3f,.08f,.32f,0,false});
 // Under the control room: the treatment MCC, still on the plant supply.
 for(float x:{15.2f,16.2f,17.2f,18.2f})m.fixtures.push_back({model::electrical_cabinet,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 cableTray(m,{14.6f,1.4f},{19.f,1.4f},F+2.45f);
 // Dosing under the gallery: hypochlorite tanks, drums, the cylinder rack.
 for(float x:{5.4f,8.f}){m.fixtures.push_back({model::tank_system,{x,2.1f},0,1.58f*.95f,1.62f*.95f,2.3f*.95f,0,true});}
 for(Vec2 d:{Vec2{10.f,1.6f},Vec2{10.8f,1.6f},Vec2{10.4f,2.35f}})m.fixtures.push_back({model::drum,d,0,.75f,.75f,1.08f,0,true});
 for(float x:{12.f,12.45f,12.9f})m.fixtures.push_back({model::gas_cylinder,{x,1.3f},0,.33f,.33f,1.58f,0,true});
 wall(m,11.7f,1.f,13.2f,1.12f,F+.9f,F+1.f,12);wall(m,11.7f,1.f,13.2f,1.12f,F+1.3f,F+1.4f,12);
 deckSupports(m,m.layers[1],F,6.f);
 // --- Sand filters --------------------------------------------------------------
 const float header=-20.4f;
 pipeRoute(m,{{1.f,7.4f,header},{15.6f,7.4f,header}},.18f,0,R);
 for(int i=0;i<2;++i){Vec2 c{9.5f+i*6.f,10.6f};
  vessel(m,c,2.f,F,-19.8f,5);
  m.pipes.push_back({c,c,-19.8f,1.55f,-19.45f,5});m.pipes.push_back({c,c,-19.45f,.55f,-19.2f,6});
  pipeRoute(m,{{c.x,7.4f,header},{c.x,8.62f,header}},.14f,0,R);
  // The outlet leaves the shell low and drops into the floor collector.
  pipeRoute(m,{{c.x+1.3f,c.y+1.55f,F+.6f},{c.x+1.9f,c.y+2.15f,F+.6f}},.12f,0,NoRoof);
  watRiser(m,{c.x+1.9f,c.y+2.15f},F,F+.6f,.12f,0);
  m.structures.push_back({c.x+1.1f,c.y+1.35f,c.x+2.05f,c.y+2.3f,F,F+.75f,false,6});
  m.fixtures.push_back({model::pwr_wall_ladder,{c.x-2.06f,c.y},0,.08f,.84f,5.f,0,false});
 }
 m.pipes.push_back({{9.5f,10.6f},{15.5f,10.6f},-19.3f,.09f,-19.3f,6});
 // --- Reservoir 2 -----------------------------------------------------------------
 const float rbed=-26.6f,rsurf=-25.35f;
 watBasin(m,8.5f,15.5f,15.5f,20.5f,F,rbed,rsurf,true);
 // P-7 hangs from a beam across the corner: column, discharge head, motor.
 const Vec2 p7{14.3f,16.7f};
 wall(m,14.15f,15.25f,14.45f,20.75f,F+1.6f,F+1.85f,18);
 for(float y:{15.25f,20.45f})wall(m,14.1f,y,14.5f,y+.3f,F+.16f,F+1.6f,12);
 m.pipes.push_back({p7,p7,rbed+.2f,.2f,F+.9f,5});m.pipes.push_back({p7,p7,F+.9f,.34f,F+1.25f,6});m.pipes.push_back({p7,p7,F+1.25f,.28f,F+1.6f,0});
 pipeRoute(m,{{14.3f,17.04f,F+1.05f},{14.3f,17.6f,F+1.05f},{16.4f,17.6f,F+1.05f}},.1f,5,NoRoof);
 watRiser(m,{16.4f,17.6f},F,F+1.05f,.1f,5);
 m.structures.push_back({15.6f,17.45f,16.5f,17.75f,F,F+1.16f,false,6});
 watLiveWater(m,12.6f,15.5f,15.5f,18.4f,rsurf,30.f,.3f);
 board(m,12.f,14.6f,F+1.3f,"RESERVOIR 2","P-7 STOPPED / STILL ENERGIZED / KEEP OFF THE RAIL",3.4f);
 terminal(m,{17.4f,16.4f},"RESERVOIR 2 / P-7 LOCAL","P-7 STOPPED. MOTOR STILL ENERGIZED FROM MCC-2.","WATER ROUND IT SHIMMERS. HAIR STANDS UP. DO NOT LEAN ON THE RAIL.");
 m.terminals.back().yaw=kPi*.5f;
 watHum(m,"wat_reservoir_hum_ms",11.f,14.f,17.f,20.f,F-1.f,F+3.f,.45f);
 // --- Transfer pumps and the make-up main to Tower 2 ------------------------------
 const float main=-22.6f;
 P3 t1=pumpSkid(m,{20.6f,16.f},F,kPi,main),t2=pumpSkid(m,{20.6f,19.8f},F,kPi,main);
 pipeRoute(m,{t2,{21.9f,t2.y,main},{21.9f,9.2f,main},{24.f,9.2f,main}},.2f,5,R);
 pipeRoute(m,{t1,{21.75f,t1.y,main}},.15f,5,R);
 gateValve(m,{21.9f,13.f,main},{0,1},.2f,F,1);
 // --- The east passage to Tower 2 ------------------------------------------------
 wall(m,23.f,8.f,24.f,12.f,-22.22f,R,15);
 for(float y:{7.6f,12.f})wall(m,22.6f,y,23.f,y+.4f,F,-21.8f,12);
 wall(m,22.6f,7.6f,23.f,12.4f,-22.22f,-21.8f,12);
 floorLine(m,22.6f,8.1f,23.f,11.9f,F,13);
 for(float x:{18.f,20.5f})floorLine(m,x,9.9f,x+1.8f,10.1f,F,14);
 // --- The west passage to the Drainage Gallery ------------------------------------
 wall(m,0.f,14.f,1.f,18.f,-21.4f,R,15);
 for(float y:{13.6f,18.f})wall(m,1.f,y,1.4f,y+.4f,F,-21.f,12);
 wall(m,1.f,13.6f,1.4f,18.4f,-21.4f,-21.f,12);
 for(float x:{2.f,4.5f})floorLine(m,x,15.9f,x+1.8f,16.1f,F,14);
 // --- Water lab, south-west -----------------------------------------------------------
 const float labTop=F+2.8f;
 wall(m,1.f,19.f,3.f,19.12f,F,labTop,3);wall(m,4.3f,19.f,7.f,19.12f,F,labTop,3);wall(m,3.f,19.f,4.3f,19.12f,F+2.2f,labTop,3);
 watGlazed(m,6.88f,19.12f,7.f,23.f,F,2.8f);
 wall(m,1.f,19.f,7.f,23.f,labTop,labTop+.15f,2);
 m.fixtures.push_back({model::lab_clean_bench,{2.45f,22.42f},0,1.18f,.56f,.81f,0,true});
 m.fixtures.push_back({model::lab_clean_bench,{3.65f,22.42f},0,1.18f,.56f,.81f,0,true});
 m.fixtures.push_back({model::lab_microscope,{2.25f,22.45f},.81f,.18f,.22f,.29f,0,false});
 m.fixtures.push_back({model::lab_field_meter,{3.45f,22.45f},.81f,.25f,.33f,.15f,0,false});
 m.fixtures.push_back({model::lab_sample_fridge,{4.9f,22.55f},0,.7f,.85f,1.f,0,true});
 m.fixtures.push_back({model::med_scrub_sink,{6.3f,22.7f},0,.49f,.54f,1.13f,0,true});
 shelf(m,{1.27f,21.6f},-kPi*.5f,0);
 for(float y:{21.1f,21.6f,22.1f})m.fixtures.push_back({model::med_specimen_jar,{1.3f,y},1.05f,.15f,.15f,.24f,0,false});
 terminal(m,{5.5f,20.2f},"WATER LAB / CONDUCTIVITY","TOWER 2 BASIN 03:02: 9999 US/CM. METER LIMIT.","NO DOSING SINCE 01:40. MANUAL NOTE: IT IS NOT THE SALT.");
 m.terminals.back().yaw=.001f;
 // --- Structure and services -----------------------------------------------------------
 for(float x:{6.f,12.f,18.f})wall(m,x-.15f,1.f,x+.15f,23.f,R-.55f,R,12);
 wallBays(m,F,R,4.f,false,true,true,true,16.f,10.f);
 cableTray(m,{7.6f,22.8f},{22.6f,22.8f},F+3.2f);
 for(float z:{F+3.6f,F+3.85f})servicePipe(m,{1.2f,22.75f},{22.8f,22.75f},z,.06f,z<F+3.7f?3:4);
 // --- Light -------------------------------------------------------------------
 for(Vec2 b:{Vec2{12.5f,13.4f},Vec2{5.5f,15.5f},Vec2{18.5f,13.f},Vec2{13.f,21.8f}})m.lights.push_back(lamp(b,R-.3f,2.4f,13.f,1900,-.25f));
 m.lights.push_back(watFlicker(lamp({19.f,21.f},R-.3f,2.f,11.f,1900,-.25f),.35f));
 for(float x:{4.f,8.5f,12.5f})m.lights.push_back(lamp({x,2.6f},G-.45f,.9f,5.5f,4300,.25f));
 for(float x:{16.5f,20.5f})m.lights.push_back(lamp({x,3.6f},G-.45f,.8f,5.f,4300,.25f));
 for(float x:{16.f,20.5f})m.lights.push_back(lamp({x,3.8f},G+2.5f,1.f,6.f,5600,.1f));
 m.lights.push_back(lamp({4.f,21.f},labTop-.12f,1.f,5.f,6200,.35f));
 m.lights.push_back(watSwitched(lamp({13.5f,16.3f},F+.1f,1.4f,5.f,12000,-.05f,WorldLightMount::Bare),watIsolated,true,.35f));
 m.lights.push_back(watWall({22.98f,7.f},F+2.6f,kPi,1.1f,4.5f,1300,-.3f));
 m.lights.push_back(watWall({1.02f,13.f},F+2.6f,0,1.1f,4.5f,1300,-.3f));
 for(float x:{3.f,12.6f})m.lights.push_back(watWall({x,1.02f},G+2.4f,kPi*.5f,.9f,6.f,4000,.1f));
 for(float x:{10.f,17.f})m.lights.push_back(watWall({x,22.98f},F+3.f,-kPi*.5f,1.f,6.f,4000,.1f));
 m.lights.push_back(lamp({2.f,9.6f},R-.3f,1.f,7.f,4300,.15f));
 // --- Encounters and script ------------------------------------------------------
 m.pickupSpawns={{{21.6f,5.2f},P::Ammo,G},{{2.f,20.4f},P::Health,F}};
 m.clutterSpawns={{4,{19.6f,4.6f},G},{2,{3.f,22.4f},F+.81f},{0,{18.3f,13.7f},F},{1,{6.8f,3.6f},F},{2,{17.2f,21.6f},F},{3,{21.2f,14.2f},F}};
 {auto e=watEvent("wat_hub_arrival",3,1,9,4,G-.5f,G+2,0);e.actions.push_back(watCheckpoint());m.scriptEvents.push_back(e);}
 // Back from the Sump Works with the plant dark behind you: two huntsmen have
 // come up out of the reservoir drains into the hall.
 {auto e=watEvent("wat_hub_return",0,13,4,19,F-.5f,F+2,watIsolated);
  e.actions.push_back(watSpawn(C::Huntsman,{18.6f,13.2f},F));e.actions.push_back(watSpawn(C::Huntsman,{12.f,13.6f},F,4));
  e.actions.push_back(watSound(Sound::SpiderCall,.8f));m.scriptEvents.push_back(e);}
 watQuiet(m,"wat_hub_quiet",{"wat_reservoir_hum_ms","wat_basin_hum_ms"});
 zone(m,"WATER TREATMENT / CONTROL ROOM",14,1,23,6,G-.5f,G+3);
 zone(m,"WATER TREATMENT / WATER LAB",1,19,7,23);
 zone(m,"WATER TREATMENT",0,0,24,24);
 freightFinish(m,-1);
 titleCard(m,"WATER AND COOLING");
 return p;
}
// ===============================================================================
// DRAINAGE GALLERY (manifest 29). The culverts under the treatment hall. The
// short way west, Culvert 2, hums: drain pump P-4 tripped standing in it with
// its motor still on MCC-2, and the fitter sent to clear its screen lies at
// the mouth. The overflow gallery north is flooded too, but no pump stands in
// that water and it is silent: wade it and learn to listen. Both ways meet at
// the sluice junction, south to the Sump Works. Once MCC-2 is isolated,
// Culvert 2 is the quick way back to the hub.
std::shared_ptr<const Map> makeDrainageGallery(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[48];m.definition=CampaignChunks[48];m.definition.ambient=.22f;
 const float F=-25,R=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),F));
 watSurfaces(m,SurfaceConcrete,SurfaceConcrete,SurfaceConcrete);
 openEdge(m,{'E',14,18,F,false,0});openEdge(m,{'S',8,12,F,false,0});
 // Rock and concrete between the culverts: the landing east, Culvert 2 through
 // the middle, the overflow gallery north, the west passage and the junction.
 {auto& g=m.layers[0];
  rectangle(g,5,9,19,12,'#');rectangle(g,22,9,23,12,'#');rectangle(g,5,12,18,14,'#');
  rectangle(g,5,18,18,19,'#');rectangle(g,13,19,18,23,'#');rectangle(g,18,20,23,23,'#');}
 m.definition.playerStart={21.6f,16.f};m.definition.spawnHeight=F;m.definition.spawnYaw=kPi;m.definition.spawnPitch=-4.f;
 // --- Culvert 2: the short way, live ------------------------------------------------
 const float abed=-25.9f,asurf=-25.35f,aroof=-22.f;
 m.waterVolumes.push_back({5.f,14.f,18.f,18.f,abed,asurf});
 wall(m,5.f,14.f,18.f,18.f,aroof,R,15);
 for(float x=6.f;x<17.9f;x+=2.4f)wall(m,x-.14f,14.f,x+.14f,18.f,aroof-.3f,aroof,15);
 for(float y:{14.f,17.78f})wall(m,5.f,y,18.f,y+.22f,aroof-.22f,aroof,15);
 // Tide marks and a scum line on the culvert walls.
 for(float y:{14.f,17.97f})wall(m,5.f,y,18.f,y+.03f,F-.02f,F+.1f,8);
 // P-4: the drain pump stands in the water where it tripped. Its riser runs
 // under the culvert roof and up through the landing ceiling to the treatment
 // drain; its supply cable comes down the wall from the isolator box.
 const Vec2 p4{11.5f,16.6f};
 m.pipes.push_back({p4,p4,abed,.46f,abed+.95f,5});
 for(float z:{abed+.12f,abed+.82f})m.pipes.push_back({p4,p4,z,.52f,z+.08f,6});
 m.pipes.push_back({p4,p4,abed+.95f,.3f,abed+1.15f,6});
 m.structures.push_back({p4.x-.5f,p4.y-.5f,p4.x+.5f,p4.y+.5f,abed,abed+1.15f,false,6});
 pipeRoute(m,{{p4.x,p4.y,abed+1.15f},{p4.x,p4.y,-22.55f},{p4.x,17.4f,-22.55f},{18.4f,17.4f,-22.55f},{18.4f,17.4f,-21.2f},{21.2f,17.4f,-21.2f},{21.2f,17.4f,R}},.14f,5,aroof);
 m.fixtures.push_back({model::pwr_disconnect_switch,{10.4f,14.18f},1.25f,.43f,.35f,.85f,kPi,false});
 pipeRoute(m,{{10.4f,14.12f,F+1.25f},{10.4f,14.12f,-22.25f},{11.5f,14.12f,-22.25f},{11.5f,16.44f,-22.25f}},.03f,6,NoRoof);
 m.fixtures.push_back({model::pwr_breaker_panel,{12.6f,14.06f},1.3f,.18f*1.6f,.1f*1.6f,.46f*1.6f,kPi,false});
 watLiveWater(m,5.f,14.f,18.f,18.f,asurf,70.f,.6f);
 watHum(m,"wat_culvert_hum_ms",4.f,12.f,23.f,20.f,F-1.5f,F+3.f,.6f);
 watHumPrime(m,"wat_culvert_hum_prime","wat_culvert_hum_ms",17.f,12.f,24.f,20.f,F-1.f,F+3.f);
 // A blue-white glow where the motor sits: the water lit from inside.
 m.lights.push_back(watSwitched(lamp({p4.x+.9f,p4.y-.7f},asurf+.08f,1.3f,5.f,12000,-.05f,WorldLightMount::Bare),watIsolated,true,.4f));
 // The fitter who went in to clear the screen: lockout tag in the water.
 watBody(m,"wat_culvert_fitter",{17.5f,15.3f},-25.45f,-kPi*.5f);
 wallSign(m,18.055f,15.6f,aroof+.06f,"CULVERT 2","LIVE / P-4 ON MCC-2 / DO NOT WADE",2.6f,kPi*.5f);
 // --- The landing (east, from Water Treatment) ------------------------------------
 terminal(m,{22.2f,19.3f},"DRAINAGE / P-4 LOCAL","P-4 TRIPPED 01:52. MOTOR STILL FED FROM MCC-2.","CULVERT 2 IS LIVE. WALK THE OVERFLOW. MCC-2 IS AT THE SUMP WORKS.");
 m.fixtures.push_back({model::pwr_control_cabinet,{18.45f,12.25f},0,.8f,.4f,1.6f,kPi,true});
 m.fixtures.push_back({model::pwr_ammeter_box,{18.06f,13.3f},1.3f,.23f,.1f,.37f,-kPi*.5f,false});
 cableTray(m,{18.2f,19.7f},{22.8f,19.7f},F+2.7f);
 floorLine(m,18.2f,15.9f,23.f,16.1f,F,14);
 // Drain grating and a hose reel at the culvert mouth.
 wall(m,18.3f,13.f,18.9f,13.6f,F,F+.012f,8);
 m.fixtures.push_back({model::cable_reel,{22.5f,18.9f},0,.5f,1.f,1.f,kPi*.5f,true});
 // --- The overflow passage north ----------------------------------------------------
 wall(m,19.f,9.f,22.f,12.f,-22.2f,R,15);
 for(float y:{9.2f,10.6f})floorLine(m,20.4f,y,20.6f,y+.9f,F,14);
 // --- Overflow gallery: flooded, and silent ---------------------------------------
 const float bbed=-25.75f,bsurf=-25.3f;
 m.waterVolumes.push_back({1.f,1.f,23.f,9.f,bbed,bsurf});
 // The south ledge carries you most of the way. It failed under the middle
 // outfall, so for six metres you wade.
 for(auto [x1,x2]:{std::pair{12.f,23.f},std::pair{1.f,6.f}}){
  wall(m,x1,7.6f,x2,9.f,bbed-.05f,F,15);
  floorLine(m,x1,7.6f,x2,7.72f,F,13);
 }
 for(float x:{6.f,12.f})wall(m,x-.25f,7.6f,x+.25f,9.f,bbed-.05f,F-.25f,15);
 wallSign(m,9.f,8.945f,F+1.4f,"OVERFLOW / LEDGE FAILED","WADE THE CHANNEL. NO PUMP ON THIS LINE.",3.f,kPi);
 // The gallery's ribs stand in the water; three overflow outfalls come down
 // the north wall from the treatment hall.
 for(float x=2.5f;x<22.6f;x+=4.f){
  for(float y:{1.f,8.76f})wall(m,x-.14f,y,x+.14f,y+.24f,bbed-.05f,R-.5f,15);
  wall(m,x-.14f,1.f,x+.14f,9.f,R-.4f,R,15);
  wall(m,x-.14f,1.f,x+.14f,1.7f,R-.75f,R-.4f,15);wall(m,x-.14f,8.3f,x+.14f,9.f,R-.75f,R-.4f,15);
 }
 for(float x:{4.5f,8.5f,16.5f}){
  m.pipes.push_back({{x,1.45f},{x,1.45f},F+.5f,.34f,R,1});
  m.pipes.push_back({{x,1.45f},{x,1.45f},F+.5f,.42f,F+.62f,6});
  m.pipes.push_back({{x,1.45f},{x,1.45f},R-.6f,.42f,R-.5f,6});
  m.structures.push_back({x-.4f,1.f,x+.4f,1.85f,bbed,R,false,6});
 }
 terminal(m,{22.45f,8.1f},"OVERFLOW / LEVEL","OVERFLOW GALLERY 0.45 M AND STEADY. NO PUMP ON THIS LINE.","IT IS JUST WATER. LISTEN FOR THE HUM. CULVERT 2 HUMS.",0);
 m.terminals.back().yaw=-kPi*.5f;
 cableTray(m,{2.f,1.3f},{22.6f,1.3f},R-1.f);
 // --- West passage ------------------------------------------------------------------
 wall(m,1.f,9.f,5.f,19.f,-21.6f,R,15);
 for(float y:{11.f,13.5f,18.5f})wall(m,1.f,y-.12f,5.f,y+.12f,-21.9f,-21.6f,15);
 for(float y:{10.5f,12.5f,19.5f})floorLine(m,2.9f,y,3.1f,y+.9f,F,14);
 // --- Sluice junction ----------------------------------------------------------------
 // Two penstock headstocks on their plinths: the gallery sluices to the sump.
 for(float x:{3.f,6.f}){
  wall(m,x-.45f,20.6f,x+.45f,21.5f,F,F+.25f,15);
  m.pipes.push_back({{x,21.05f},{x,21.05f},F+.25f,.12f,F+1.1f,6});
  m.fixtures.push_back({model::valve_wheel,{x,21.05f},1.12f,.55f,.12f,.55f,0,false});
  m.structures.push_back({x-.2f,20.85f,x+.2f,21.25f,F,F+1.15f,false,6});
 }
 terminal(m,{12.35f,19.9f},"SLUICE JUNCTION / SUMP ACCESS","MCC-2 FEEDER ISOLATOR: SUMP WORKS PIT FLOOR, SOUTH-WEST ROOM.","OPEN IT AND EVERY LOAD DROPS: P-4, P-7, TOWER 2 RECIRC, THE SUMP.");
 m.terminals.back().yaw=-kPi*.5f;
 servicePipe(m,{1.2f,19.6f},{12.8f,19.6f},F+3.6f,.16f,0);
 cableTray(m,{12.6f,19.2f},{12.6f,22.8f},F+3.f);
 m.fixtures.push_back({model::electrical_cabinet,{12.55f,21.6f},0,.9066f,.4956f,2.2f,-kPi*.5f,true});
 m.fixtures.push_back({model::pwr_fire_cabinet,{1.12f,21.f},1.1f,.25f,.2f,.56f,-kPi*.5f,false});
 for(float x:{9.f,11.f})floorLine(m,x,20.6f,x+.2f,23.f,F,14);
 // --- Light ---------------------------------------------------------------------
 m.lights.push_back(lamp({20.5f,16.f},R-.25f,1.4f,8.f,6200,.4f));
 m.lights.push_back(watWall({22.98f,13.5f},F+2.4f,kPi,1.f,5.f,3000));
 m.lights.push_back(watWall({20.5f,19.98f},F+3.3f,-kPi*.5f,.8f,4.f,3000));
 m.lights.push_back(watFlicker(lamp({14.f,16.f},aroof-.32f,.7f,5.f,4300,.3f),.45f));
 m.lights.push_back(lamp({7.5f,16.f},aroof-.32f,.6f,4.5f,4300,.3f));
 m.lights.push_back(lamp({20.5f,10.5f},-22.35f,1.f,5.f,3600));
 m.lights.push_back(watFlicker(lamp({17.f,5.f},R-.3f,1.1f,8.f,1900,-.25f),.3f));
 m.lights.push_back(lamp({8.5f,4.5f},R-.3f,.9f,7.f,1900,-.25f));
 m.lights.push_back(watFlicker(lamp({13.f,2.5f},R-.3f,.6f,6.f,6200,.4f),.6f));
 m.lights.push_back(watWall({1.02f,7.f},F+2.f,0,1.f,4.5f,1100,-.45f));
 m.lights.push_back(lamp({3.f,15.f},-21.75f,1.f,5.f,3400));
 m.lights.push_back(lamp({3.f,10.5f},-21.75f,.7f,4.f,3400));
 for(Vec2 b:{Vec2{4.5f,21.f},Vec2{10.f,21.5f}})m.lights.push_back(lamp(b,R-.25f,1.2f,6.f,4300,.2f));
 m.lights.push_back(watWall({7.f,22.98f},F+2.4f,-kPi*.5f,1.f,4.f,1300,-.3f));
 // --- Encounters and script ----------------------------------------------------------
 m.creatureSpawns={{C::Huntsman,{3.6f,4.2f}},{C::Huntsman,{15.f,3.f}}};
 m.pickupSpawns={{{2.4f,8.3f},P::Ammo,F},{{7.4f,21.6f},P::Health,F}};
 m.clutterSpawns={{0,{9.f,6.f},-999,.4f},{2,{7.5f,4.f},-999,1.1f},{0,{15.5f,6.6f},-999,2.f},{2,{17.3f,16.6f},-999,.3f},{3,{20.f,8.3f},F,0},{1,{12.f,21.f},F,.8f}};
 {auto e=watEvent("wat_drainage_arrival",18,12,24,20,F-.5f,F+2);e.actions.push_back(watCheckpoint());m.scriptEvents.push_back(e);}
 zone(m,"DRAINAGE GALLERY / CULVERT 2",5,14,18,18);
 zone(m,"DRAINAGE GALLERY / OVERFLOW GALLERY",1,1,23,9);
 zone(m,"DRAINAGE GALLERY / SLUICE JUNCTION",1,19,13,23);
 zone(m,"DRAINAGE GALLERY",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// ===============================================================================
// SUMP WORKS (manifest 30). The bottom of the plant's water: the wet well and
// its three duty pumps, two great screw lifts raising the drains to the
// outfall, and the screenings press. You arrive on the motor terrace and look
// down into the pit. MCC-2, the board that feeds everything live in this
// chapter, stands in its room on the pit floor south-west, and the only way
// to it is under the press while it cycles. Isolate it and the plant's water
// goes dead and dark: the sump lights die for red emergency lamps, the press
// stops raised, and what sheltered in the drains comes out.
std::shared_ptr<const Map> makeSumpWorks(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[49];m.definition=CampaignChunks[49];m.definition.ambient=.24f;
 const float F=-28,G=-25,R=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),F));
 watSurfaces(m,SurfaceConcrete,SurfaceConcrete,SurfaceConcrete);
 openEdge(m,{'N',8,12,G,false,0});
 m.definition.playerStart={10.f,2.2f};m.definition.spawnHeight=G;m.definition.spawnYaw=kPi*.5f;m.definition.spawnPitch=-18.f;
 // --- Motor terrace (-25) and the stair down -------------------------------------
 deck(m,G,1,1,23,5);deck(m,G,8,0,12,1);
 deck(m,G,2,5,4,7);deck(m,G,2,7,13,9);       // the pump bridge over the wet well
 wall(m,1,1,23,5,F,G-.22f,15);wall(m,8,0,12,1,F,G-.22f,15);
 // Over the way in from the Drainage Gallery: its roof is lower than this one.
 wall(m,8,0,12,1,-20.f,R,15);
 floorLine(m,1.f,4.82f,23.f,4.97f,G,13);
 m.stairs.push_back({21.f,5.f,22.8f,9.2f,F,G,15,true,false});
 board(m,18.2f,4.55f,G+1.f,"MCC-2 / FEEDER ISOLATOR","PIT FLOOR SOUTH-WEST / STAIR EAST / UNDER THE PRESS",2.8f);
 // The terrace desk: the duty pump log faces the pit.
 terminal(m,{6.f,3.2f},"SUMP WORKS / DUTY PUMP LOG","P-2 STARTED 02:40. NO START COMMAND. P-2 STOPPED 02:41. NO STOP COMMAND.","P-2 STARTED 02:52. NOBODY IS LOGGED IN DOWN HERE.",G-F);
 m.terminals.back().yaw=.001f;
 for(float x:{2.f,2.95f})m.fixtures.push_back({model::electrical_cabinet,{x,1.27f},G-F,.9066f,.4956f,2.2f,kPi,true});
 m.fixtures.push_back({model::pwr_tall_switch_cabinet,{13.5f,1.6f},G-F,.95f*.85f,1.54f*.85f,3.02f*.85f,kPi*.5f,true});
 cableTray(m,{1.6f,1.3f},{12.6f,1.3f},G+2.75f);
 for(Vec2 d:{Vec2{20.6f,2.f},Vec2{21.4f,2.f},Vec2{21.f,2.75f}})m.fixtures.push_back({model::drum,d,G-F,.6f,.6f,.86f,0,true});
 // Overhead crane runway along both side walls, the bridge parked north.
 for(float x:{1.f,22.6f}){wall(m,x,1.f,x+.4f,23.f,R-2.f,R-1.75f,12);
  for(float y=3.f;y<23;y+=5.f)wall(m,x,y-.2f,x+.4f,y+.2f,R-2.6f,R-2.f,15);}
 for(float y:{2.6f,3.6f})wall(m,1.4f,y,22.6f,y+.3f,R-1.75f,R-1.35f,18);
 wall(m,10.6f,2.5f,11.6f,4.f,R-2.15f,R-1.75f,12);
 for(float y:{3.2f,3.4f})m.pipes.push_back({{11.1f,y},{11.1f,y},R-2.15f,.018f,G+3.4f,6});
 wall(m,10.85f,3.05f,11.35f,3.55f,G+3.1f,G+3.4f,18);
 // --- The wet well and its duty pumps --------------------------------------------
 const float wbed=-29.6f,wsurf=-28.4f;
 m.waterVolumes.push_back({2.f,6.f,14.f,12.5f,wbed,wsurf});
 for(float x:{1.75f,14.f})wall(m,x,5.75f,x+.25f,12.75f,wbed-.1f,F+.16f,15);
 wall(m,2.f,12.5f,14.f,12.75f,wbed-.1f,F+.16f,15);wall(m,2.f,5.75f,14.f,6.f,wbed-.1f,F+.16f,15);
 floorLine(m,1.75f,12.8f,14.25f,12.92f,F,13);
 watRailRun(m,{1.88f,12.62f},{14.12f,12.62f},F+.16f,1.3f);watRailRun(m,{14.12f,9.2f},{14.12f,12.62f},F+.16f,1.3f);
 watRailRun(m,{1.88f,6.f},{1.88f,12.62f},F+.16f,1.3f);
 // The pump bridge hangs from the roof on rods outboard of its edges.
 for(Vec2 h:{Vec2{5.5f,6.94f},Vec2{8.5f,6.94f},Vec2{11.5f,6.94f},Vec2{3.f,9.06f},Vec2{6.f,9.06f},Vec2{9.f,9.06f},Vec2{12.f,9.06f},Vec2{13.06f,8.f},Vec2{1.94f,6.5f}})
  m.pipes.push_back({h,h,G-.22f,.03f,R-.05f,6});
 for(int i=0;i<3;++i){const float x=4.5f+i*3.f;const Vec2 c{x,10.2f};
  // Bowl on the well floor, column up out of the water, motor on a bracket
  // platform off the bridge, discharge head under it.
  m.pipes.push_back({c,c,wbed,.5f,wbed+.9f,5});
  m.pipes.push_back({c,c,wbed+.9f,.24f,G-.6f,6});
  for(float z:{wbed+.2f,wsurf+.4f,G-1.6f})m.pipes.push_back({c,c,z,.32f,z+.1f,6});
  m.pipes.push_back({c,c,G-.6f,.36f,G-.25f,5});
  wall(m,x-.55f,9.f,x+.55f,10.9f,G-.25f,G,12);
  m.pipes.push_back({c,c,G,.42f,G+1.25f,i==1?4:0});
  for(float z=G+.2f;z<G+1.2f;z+=.25f)m.pipes.push_back({c,c,z,.46f,z+.05f,6});
  m.pipes.push_back({c,c,G+1.25f,.26f,G+1.45f,6});
  m.structures.push_back({x-.46f,9.74f,x+.46f,10.66f,G,G+1.45f,false,6});
  pipeRoute(m,{{x,10.55f,G-.42f},{x,11.6f,G-.42f},{x,11.6f,-25.6f}},.16f,5,NoRoof);
  wall(m,x-.4f,9.25f,x+.4f,9.4f,G,G+.95f,2);
 }
 pipeRoute(m,{{4.5f,11.6f,-25.6f},{19.9f,11.6f,-25.6f},{19.9f,9.f,-25.6f}},.22f,5,R);
 watHum(m,"wat_sump_hum_ms",2.f,5.f,14.f,13.f,F-2.f,G+3.f,.45f);
 watLiveWater(m,2.f,6.f,14.f,12.5f,wsurf,40.f,.3f);
 // --- The screw lifts and the outfall ------------------------------------------------
 const float outfall=-24.5f;
 wall(m,19.2f,5.f,20.6f,9.f,F,outfall,15);
 wall(m,19.2f,5.f,20.6f,9.f,outfall,outfall+.03f,8);
 for(float y:{6.7f,8.25f}){
  const float x1=13.5f,z1=-29.1f,x2=19.2f,z2=-24.85f,slope=(z2-z1)/(x2-x1);
  // The screw in short lengths, so each sits under its own trough step.
  for(float x=x1;x<x2-.01f;x+=.4f){const float xe=std::min(x2,x+.4f);m.pipes.push_back({{x,y},{xe,y},z1+(x-x1)*slope,.6f,z1+(xe-x1)*slope,1});}
  for(float x=x1+.3f;x<x2-.1f;x+=.45f){float z=z1+(x-x1)*slope;m.pipes.push_back({{x-.03f,y},{x+.03f,y},z,.68f,z+.06f*slope,6});}
  // The trough the screw turns in, and its collision.
  for(float x=13.5f;x<19.2f;x+=.4f){float zc=z1+(x+.2f-x1)*slope;
   const float bottom=x<14.2f?wbed-.1f:F;
   if(zc+.2f*slope+.72f>bottom)m.structures.push_back({x,y-.66f,x+.4f,y+.66f,bottom,zc+.2f*slope+.72f,false,6});
   if(zc-.62f>F+.05f)wall(m,x,y-.72f,x+.4f,y+.72f,F,zc-.6f,15);}
  wall(m,19.f,y-.55f,20.4f,y+.55f,outfall,outfall+.9f,12);
 }
 // --- Dividing wall and the screenings press ------------------------------------
 wall(m,1.f,13.f,15.f,13.4f,F,-24.f,15);wall(m,19.f,13.f,23.f,13.4f,F,-24.f,15);
 for(float x:{14.6f,19.f})wall(m,x,13.f,x+.4f,18.4f,F,-24.f,15);
 m.compactors={{15,13,19,18,F,-25.6f,9,watIsolated}};
 for(float x:{15.16f,18.84f})for(float y:{13.12f,17.88f})m.structures.push_back({x-.105f,y-.105f,x+.105f,y+.105f,F,-24.98f,false,6});
 wall(m,15.f,13.f,19.f,18.4f,F,F+.03f,8);
 for(float y:{12.75f,18.45f})floorLine(m,15.f,y,19.f,y+.12f,F,13);
 m.pipes.push_back({{17.f,15.5f},{17.f,15.5f},-24.9f,.45f,R,1});
 m.pipes.push_back({{17.f,15.5f},{17.f,15.5f},-24.95f,.55f,-24.75f,6});
 wall(m,14.6f,13.f,19.4f,13.4f,F+3.3f,-24.f,15);
 wallSign(m,21.1f,12.945f,F+1.9f,"SCREENINGS PRESS","CYCLES ON MCC-2 / PASS ON THE UPSTROKE",2.8f,kPi);
 // The press hydraulics in the east pocket.
 m.fixtures.push_back({model::tank_system,{21.6f,14.7f},0,1.58f*.8f,1.62f*.8f,2.3f*.8f,0,true});
 P3 hp=pumpSkid(m,{21.4f,17.f},F,kPi*.5f,-24.6f);
 pipeRoute(m,{hp,{19.6f,hp.y,-24.6f},{19.6f,16.f,-24.6f},{19.4f,16.f,-24.6f}},.08f,4,R);
 // --- MCC-2 room (south-west) ---------------------------------------------------------
 const float mtop=F+3.4f;
 watGlazed(m,1.f,15.f,8.85f,15.15f,F,3.4f);
 watGlazed(m,8.85f,15.f,9.f,18.8f,F,3.4f);watGlazed(m,8.85f,20.3f,9.f,23.f,F,3.4f);
 wall(m,8.85f,18.8f,9.f,20.3f,F+2.3f,mtop,3);
 wall(m,1.f,15.f,9.f,23.f,mtop,mtop+.15f,2);
 for(float y:{18.7f,20.3f})wall(m,8.75f,y,9.1f,y+.1f,F,F+2.3f,12);
 // The lineup along the south wall, one section per load.
 const char* loads[]={"P-4 DRAIN","P-7 RES 2","TOWER 2 RECIRC","SUMP P-1 TO P-3","SUMP LIGHTS"};
 for(int i=0;i<5;++i){float x=1.85f+i*1.4f;
  m.fixtures.push_back({model::pwr_switchgear_cabinet,{x,22.63f},0,1.35f,.74f,2.43f,0,true});
  }
 // The feeder isolator: the big switch on the west wall, its control desk.
 m.fixtures.push_back({model::pwr_disconnect_switch,{1.3f,18.2f},.75f,.43f*1.7f,.35f*1.7f,.85f*1.7f,-kPi*.5f,false});
 terminal(m,{2.75f,18.2f},"MCC-2 / FEEDER ISOLATOR","FEEDER CLOSED. P-4, P-7, TOWER 2 RECIRC AND THE SUMP ENERGIZED.","E / OPEN THE FEEDER. EVERYTHING ON MCC-2 GOES DEAD.",0,watIsolated);
 m.terminals.back().yaw=kPi*.5f;
 terminal(m,{6.2f,16.f},"MCC-2 / FAULT LOG","01:52 EARTH FAULT ON P-4, P-7 AND TOWER 2 RECIRC. FEEDER DID NOT OPEN.","NOBODY CAME DOWN TO OPEN IT. THE WATER HAS BEEN LIVE SINCE.");
 m.terminals.back().yaw=.001f;
 m.fixtures.push_back({model::office_chair,{7.6f,16.6f},0,.55f,.55f,.95f,.6f,true});
 for(float y:{20.2f,21.f})m.fixtures.push_back({model::pwr_control_cabinet,{1.25f,y},0,.8f,.4f,1.6f,-kPi*.5f,true});
 m.fixtures.push_back({model::pwr_fire_cabinet,{8.72f,22.f},1.1f,.25f,.2f,.56f,kPi*.5f,false});
 cableTray(m,{1.5f,15.6f},{8.4f,15.6f},F+3.f);
 // --- The pockets and the south bay ---------------------------------------------------
 m.fixtures.push_back({model::dumpster,{11.2f,16.6f},0,1.96f,1.27f,1.17f,kPi*.5f,true});
 for(Vec2 b:{Vec2{13.4f,14.f},Vec2{13.4f,14.8f}})m.fixtures.push_back({model::drum,b,0,.6f,.6f,.86f,0,true});
 // The grit culvert: a dark mouth in the south bay's back wall.
 wall(m,13.f,21.2f,15.5f,21.5f,F,F+3.4f,15);wall(m,18.f,21.2f,23.f,21.5f,F,F+3.4f,15);
 wall(m,15.5f,21.2f,18.f,23.f,F+2.2f,F+3.4f,15);wall(m,13.f,21.2f,23.f,23.f,F+3.4f,F+3.6f,15);
 for(float x:{15.5f,17.9f})wall(m,x-.1f,21.2f,x+.1f,21.6f,F,F+2.2f,12);
 m.fixtures.push_back({model::mine_bent_grate,{16.75f,22.85f},0,3.14f*.68f,.11f*.68f,3.38f*.68f,0,true});
 servicePipe(m,{1.2f,22.75f},{22.8f,22.75f},F+3.9f,.12f,0);
 // --- Light ---------------------------------------------------------------------------
 // The sump lights run off MCC-2; red emergency fittings take over when it opens.
 for(Vec2 b:{Vec2{7.f,10.f},Vec2{17.f,11.f},Vec2{12.f,20.f},Vec2{21.f,15.5f},Vec2{11.5f,15.f}})
  m.lights.push_back(watSwitched(lamp(b,-21.8f,1.8f,10.f,2000,-.2f),watIsolated,true));
 m.lights.push_back(watSwitched(lamp({17.f,20.5f},-22.4f,1.2f,8.f,2000,-.2f),watIsolated,true,.25f));
 for(float x:{15.02f,18.98f})m.lights.push_back(watSwitched(watWall({x,15.5f},F+3.6f,x<17?0:kPi,1.f,4.f,4200,.1f),watIsolated,true));
 for(Vec2 b:{Vec2{14.8f,12.98f},Vec2{19.2f,12.98f},Vec2{19.2f,18.42f}}){WorldLight l=watWall(b,F+3.4f,b.y<15?-kPi*.5f:kPi*.5f,1.2f,4.f,1500,-.25f);l.period=.9f;l.onTime=.45f;m.lights.push_back(watSwitched(l,watIsolated,true));}
 for(Vec2 b:{Vec2{4.f,3.f},Vec2{14.f,3.f}})m.lights.push_back(lamp(b,G+3.f,1.2f,7.f,3400));
 m.lights.push_back(lamp({8.f,8.f},G-.3f,.8f,5.f,4300,.2f));
 for(float x:{3.f,7.f})m.lights.push_back(lamp({x,19.f},mtop-.12f,1.f,5.f,4300,.25f));
 m.lights.push_back(watSwitched(watWall({22.98f,10.5f},F+2.4f,kPi,1.f,6.f,1050,-.5f),watIsolated,false));
 m.lights.push_back(watSwitched(watWall({12.f,22.98f},F+2.4f,-kPi*.5f,1.f,6.f,1050,-.5f),watIsolated,false));
 m.lights.push_back(watSwitched(watWall({12.f,13.42f},F+2.4f,kPi*.5f,.9f,5.f,1050,-.5f),watIsolated,false));
 m.lights.push_back(watSwitched(watWall({19.5f,1.02f},G+2.3f,kPi*.5f,.9f,6.f,1050,-.5f),watIsolated,false));
 m.lights.push_back(watSwitched(watWall({1.02f,10.f},F+2.6f,0,.8f,6.f,1050,-.5f),watIsolated,false));
 // --- Encounters and script ---------------------------------------------------------
 m.creatureSpawns={{C::Huntsman,{16.75f,22.2f},F,1,0,watIsolated},{C::Huntsman,{21.5f,11.2f},F,1,0,watIsolated},{C::Huntsman,{10.2f,14.2f},F,1,0,watIsolated}};
 m.pickupSpawns={{{4.f,19.8f},P::Health,F},{{12.f,8.f},P::Ammo,G},{{21.5f,19.5f},P::Ammo,F}};
 m.clutterSpawns={{0,{12.8f,16.f},F},{3,{7.5f,21.4f},F},{1,{3.6f,21.6f},F},{1,{18.5f,3.6f},G},{2,{22.f,13.9f},F}};
 {auto e=watEvent("wat_sump_arrival",7,0,13,5,G-.5f,G+2);e.actions.push_back(watCheckpoint());m.scriptEvents.push_back(e);}
 watHumPrime(m,"wat_sump_hum_prime","wat_sump_hum_ms",1,0,23,5,G-.5f,G+2.5f);
 // MCC-2 opens: the plant's water goes dead. Every live pool's hum stops, the
 // pumps run down, and the drains answer.
 {auto e=watEvent("wat_mcc2_open",0,0,24,24,-100,100,watIsolated);
  e.actions.push_back(watShake(.3f));e.actions.push_back(watSound(Sound::LiftMotor,.8f));e.actions.push_back(watCheckpoint());
  for(auto t:WatHumTimers)e.actions.push_back(watSet(stateId(t),WatHumQuiet));
  e.actions.push_back(watSet(stateId("wat_sump_rundown_ms"),1));m.scriptEvents.push_back(e);}
 {TimedSequence s;s.timerState=stateId("wat_sump_rundown_ms");s.finishState=stateId("wat_sump_rundown_done");s.x1=-2;s.y1=-2;s.x2=-1;s.y2=-1;s.durationMs=12000;s.finishAtMs=11000;s.soundIntervalMs=0;m.timedSequences.push_back(s);}
 cue(m,"wat_sump_rundown_ms",400,Sound::WaterReturn,{8.f,9.f},"MCC-2 OPEN. THE PUMPS RUN DOWN.",.7f,F);
 cue(m,"wat_sump_rundown_ms",3800,Sound::SpiderCall,{16.75f,22.f},"",.9f,F+.5f);
 cue(m,"wat_sump_rundown_ms",6200,Sound::SpiderCall,{21.f,6.f},"",.8f,F+.5f);
 zone(m,"SUMP WORKS / MCC-2 ROOM",1,15,9,23);
 zone(m,"SUMP WORKS / SCREENINGS PRESS",14.6f,13,19.4f,18.4f);
 zone(m,"SUMP WORKS / MOTOR TERRACE",0,0,24,9,G-.5f,100);
 zone(m,"SUMP WORKS",0,0,24,24);
 freightFinish(m,-1);
 watDeckRails(m,G);
 return p;
}
// ===============================================================================
// COOLING TOWERS / BASIN (manifest 31). Tower 2 is one hollow concrete shell,
// 48 m from basin to throat, across this chunk and the next. You come in high
// on the make-up water platform and see all of it at once: the basin lit from
// inside and arcing, the fill packs over it on their concrete forest, the
// recirculation pumps that tripped on the north apron, Pryor face down on the
// crossing, and far south the shell stair climbing to a lit door. The basin
// is live until MCC-2 is isolated; the crossing walkway is wet steel on still
// water, which is exactly what resonance likes.
std::shared_ptr<const Map> makeCoolingTowersBasin(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[50];m.definition=CampaignChunks[50];m.definition.ambient=.2f;
 const float F=-28,E=-25,R=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),F));
 watSurfaces(m,SurfaceConcrete,SurfaceConcrete,SurfaceConcrete);
 openEdge(m,{'W',8,12,E,false,0});openEdge(m,{'S',2,22,F,false,0});
 m.definition.playerStart={1.4f,10.f};m.definition.spawnHeight=E;m.definition.spawnYaw=.35f;m.definition.spawnPitch=-12.f;
 // --- The shell -----------------------------------------------------------------------
 const float base=-29.3f;
 watShellEnd(m,base,-16.f,0,-1,8.f);
 wall(m,1.f,12.f,2.f,24.f,base,-16.f,15);wall(m,1.f,8.f,2.f,12.f,base,E-.22f,15);wall(m,1.f,8.f,2.f,12.f,-21.9f,-16.f,15);
 wall(m,0.f,8.f,1.f,12.f,-21.9f,R,15);wall(m,0.f,8.f,1.f,12.f,F,E-.22f,15);
 wall(m,22.f,8.f,23.f,24.f,base,-16.f,15);
 watShellUpper(m,-1,8.f,8.f,24.f);
 watLouvres(m,2.f,1,13.f,23.f);watLouvres(m,22.f,-1,9.5f,23.f);
 // --- The make-up water platform (-25) and its stair down -----------------------
 deck(m,E,0,8,2,12);deck(m,E,2,7,5,12);
 wall(m,2.f,7.f,5.f,12.f,base,E-.22f,15);
 m.stairs.push_back({5.f,7.f,9.2f,8.6f,F,E,15,false,false});
 floorLine(m,9.2f,7.f,9.4f,8.6f,F,13);
 // The make-up main from the transfer pumps comes through the shell with you
 // and drops into the basin.
 pipeRoute(m,{{0.f,9.2f,-22.6f},{5.6f,9.2f,-22.6f},{5.6f,9.2f,-27.9f}},.2f,5,NoRoof);
 m.structures.push_back({5.33f,8.93f,5.87f,9.47f,-29.2f,-22.4f,false,6});
 gateValve(m,{2.8f,9.2f,-22.6f},{1,0},.2f,F,-1);
 // --- The basin -----------------------------------------------------------------------
 const float bed=-29.2f,surf=-28.35f;
 m.waterVolumes.push_back({2.f,9.f,22.f,24.f,bed,surf});
 for(auto [x1,x2]:{std::pair{2.f,11.3f},std::pair{12.7f,22.f}})wall(m,x1,8.75f,x2,9.f,bed-.1f,F+.15f,15);
 floorLine(m,2.f,8.5f,22.f,8.66f,F,13);
 watLiveWater(m,2.f,9.f,22.f,24.f,surf,80.f,.6f);
 watHum(m,"wat_basin_hum_ms",2.f,9.f,22.f,24.f,F-2.f,E+3.f,.7f);
 watHumPrime(m,"wat_basin_hum_prime","wat_basin_hum_ms",0.f,7.f,6.f,13.f,E-.5f,E+2.5f);
 // The crossing: steel grating on concrete stubs, wet, a hand's breadth over
 // the water. South under the fill, east, and over the seam to the shell stair.
 auto walkway=[&](float x1,float y1,float x2,float y2){
  wall(m,x1,y1,x2,y2,-28.1f,-27.95f,8);
  for(float x=x1+.7f;x<x2-.3f;x+=2.f)for(float y=y1+.7f;y<y2-.3f;y+=2.f)wall(m,x-.15f,y-.15f,x+.15f,y+.15f,bed,-28.1f,15);
 };
 walkway(11.3f,8.6f,12.7f,22.f);walkway(12.7f,20.6f,19.8f,22.f);walkway(18.4f,22.f,19.8f,24.f);
 for(auto r:{std::array<float,4>{11.27f,8.6f,11.33f,22.f},{12.67f,8.6f,12.73f,20.6f},{12.7f,20.57f,19.8f,20.63f},{11.3f,21.97f,18.4f,22.03f},{18.37f,22.f,18.43f,24.f},{19.77f,20.6f,19.83f,24.f}})
  m.structures.push_back({r[0],r[1],r[2],r[3],-27.95f,-26.95f,true,2});
 // Pryor, face down on the crossing, one hand in the water.
 watBody(m,"wat_basin_pryor",{12.f,11.7f},-27.95f,kPi*.5f);
 m.clutterSpawns.push_back({0,{13.1f,12.3f},-999,.7f});
 // --- The fill: packs on a concrete forest, spray laterals and eliminators over ----
 const float beamB=-21.6f,beamT=-21.2f,fillT=-19.8f;
 for(float x:{5.f,9.f,15.f,19.f})for(float y:{11.5f,15.5f,19.5f})wall(m,x-.25f,y-.25f,x+.25f,y+.25f,bed,beamB,15);
 for(float y:{9.5f,11.5f,15.5f,19.5f,23.6f})wall(m,2.f,y-.2f,22.f,y+.2f,beamB,beamT,15);
 for(float x:{5.f,9.f,15.f,19.f})wall(m,x-.2f,9.5f,x+.2f,24.f,beamB,beamT,15);
 for(float x=2.1f;x<21.9f;x+=2.f)for(float y=9.6f;y<19.4f;y+=2.f)wall(m,x,y,x+1.9f,y+1.9f,beamT,fillT,29);
 wall(m,2.f,9.5f,22.f,19.5f,-18.2f,-18.f,29);
 for(float y=10.6f;y<19.f;y+=2.f){
  m.pipes.push_back({{2.f,y},{22.f,y},-19.4f,.06f,-19.4f,0});
  for(float x=3.2f;x<21.f;x+=1.5f)m.pipes.push_back({{x,y},{x,y},-19.58f,.035f,-19.4f,5});
  m.pipes.push_back({{14.f,y},{14.f,y},-19.4f,.06f,-18.6f,0});
 }
 // --- Recirculation pumps P-11 to P-14 on the north apron --------------------------
 for(int i=0;i<4;++i){const float x=10.6f+i*2.3f;
  pumpSkid(m,{x,4.3f},F,-kPi*.5f,-26.2f);
  pipeRoute(m,{{x,4.05f,-26.2f},{x,4.05f,-18.6f}},.2f,5,NoRoof);
 }
 pipeRoute(m,{{10.6f,4.05f,-18.6f},{17.5f,4.05f,-18.6f}},.24f,5,NoRoof);
 pipeRoute(m,{{14.f,4.05f,-18.6f},{14.f,18.6f,-18.6f}},.22f,5,NoRoof);
 for(float x:{11.f,16.5f})m.pipes.push_back({{x,1.f},{x,3.85f},-18.4f,.05f,-18.4f,6});
 for(float y=10.f;y<19.4f;y+=4.f)m.pipes.push_back({{14.f,y},{14.f,y},fillT,.05f,-18.8f,6});
 terminal(m,{14.2f,7.3f},"TOWER 2 / RECIRC P-11 TO P-14","TRIPPED 01:52. RESET 02:58. RESET 02:59. RESET 03:00.","RESET REFUSED: FEEDER FAULT ON MCC-2. NO FURTHER INPUT.");
 m.terminals.back().yaw=.001f;
 board(m,17.6f,8.2f,F+1.f,"BASIN CROSSING","LIVE / RECIRC STILL ENERGIZED / ISOLATE MCC-2, SUMP WORKS",2.8f);
 // Biocide dosing in the north-east of the apron.
 {float s=.7f;m.fixtures.push_back({model::tank_system,{19.7f,6.f},0,1.58f*s,1.62f*s,2.3f*s,0,true});}
 for(Vec2 d:{Vec2{20.4f,7.6f},Vec2{19.6f,7.7f}})m.fixtures.push_back({model::drum,d,0,.6f,.6f,.86f,0,true});
 m.fixtures.push_back({model::pwr_fire_cabinet,{9.4f,1.12f},1.1f,.25f,.2f,.56f,kPi,false});
 m.fixtures.push_back({model::cable_reel,{7.4f,3.6f},0,.5f,1.f,1.f,0,true});
 // --- Light ------------------------------------------------------------------------
 m.lights.push_back(watWall({2.02f,12.5f},-22.6f,0,1.1f,6.f,3000));
 m.lights.push_back(watWall({12.f,1.02f},-23.4f,kPi*.5f,1.4f,9.f,1900,-.25f));
 m.lights.push_back(lamp({14.f,5.2f},-19.f,1.4f,9.f,4300,.2f));
 for(Vec2 b:{Vec2{12.f,11.5f},Vec2{12.f,15.5f},Vec2{12.f,19.5f},Vec2{6.5f,15.5f},Vec2{17.f,11.5f},Vec2{17.f,19.5f}})
  m.lights.push_back(watFlicker(lamp(b,beamB-.2f,1.2f,8.f,6500,.3f),b.x<10||b.y>19?.3f:0.f));
 for(Vec2 b:{Vec2{8.f,14.f},Vec2{16.5f,18.5f},Vec2{6.f,18.4f}})
  m.lights.push_back(watSwitched(lamp(b,surf+.1f,1.5f,6.f,12000,-.05f,WorldLightMount::Bare),watIsolated,true,.3f));
 m.lights.push_back(watWall({2.02f,15.f},-17.f,0,2.2f,16.f,1900,-.25f));
 m.lights.push_back(watWall({21.98f,15.f},-17.f,kPi,2.2f,16.f,1900,-.25f));
 m.lights.push_back(watWall({12.f,1.02f},-17.f,kPi*.5f,1.6f,14.f,1900,-.25f));
 for(float z:{-5.f,7.f,17.f}){const float d=watThroatInset(z);
  m.lights.push_back(watWall({2.02f+d,16.f},z,0,.8f,5.f,1000,-.5f));
  m.lights.push_back(watWall({21.98f-d,16.f},z,kPi,.8f,5.f,1000,-.5f));}
 m.lights.push_back(lamp({12.f,14.f},R-.4f,2.f,14.f,1900,-.25f));
 // --- Encounters and script ---------------------------------------------------------
 m.pickupSpawns={{{3.6f,8.4f},P::Ammo,E},{{7.f,5.f},P::Health,F}};
 m.clutterSpawns.push_back({3,{8.6f,2.6f},F});m.clutterSpawns.push_back({1,{18.6f,7.f},F});m.clutterSpawns.push_back({2,{4.4f,10.6f},E});
 {auto e=watEvent("wat_tower_arrival",0,7,5,12,E-.5f,E+2);e.actions.push_back(watCheckpoint());m.scriptEvents.push_back(e);}
 // Halfway along the crossing once the water is dead: they come out of the
 // fill and off the columns.
 {auto e=watEvent("wat_basin_ambush",10.8f,14.f,13.2f,17.5f,F-1.f,F+1.5f,watIsolated);
  e.actions.push_back(watSpawn(C::Huntsman,{5.6f,20.6f},-999));e.actions.push_back(watSpawn(C::Huntsman,{19.f,13.4f},-999,4));
  e.actions.push_back(watSound(Sound::SpiderCall,.9f));m.scriptEvents.push_back(e);}
 zone(m,"COOLING TOWER 2 / RECIRC PUMPS",1,1,23,8.75f,F-1,F+2.5f);
 zone(m,"COOLING TOWER 2 / BASIN CROSSING",2,8.75f,22,24,F-2,F+2.5f);
 zone(m,"COOLING TOWER 2",0,0,24,24);
 freightFinish(m,-1);
 watDeckRails(m,E);
 return p;
}
// ===============================================================================
// COOLING TOWERS / SHELL (manifest 31). The south half of Tower 2. Its fill
// cells were stripped for new packs a fortnight ago, so here the shell is
// open from the water to the throat 48 m up, ringed with red lamps. The
// crossing lands on the stair apron, and the shell stair spirals once round
// the inside wall: east, round the south end, back up the west wall, and
// across the void on the gallery bridge to the fill gallery door and Heat
// Exchange.
std::shared_ptr<const Map> makeCoolingTowersShell(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[51];m.definition=CampaignChunks[51];m.definition.ambient=.2f;
 const float F=-28,R=m.definition.ceiling,L1=-24.4f,L2=-20.8f,L3=-19.f;
 m.layers.push_back(floorLayer(m.name.c_str(),F));
 watSurfaces(m,SurfaceConcrete,SurfaceConcrete,SurfaceConcrete);
 openEdge(m,{'N',2,22,F,false,0});openEdge(m,{'E',6,10,L3,false,0});
 m.definition.playerStart={19.6f,1.6f};m.definition.spawnHeight=F;m.definition.spawnYaw=kPi*.5f;m.definition.spawnPitch=-4.f;
 // --- The shell -----------------------------------------------------------------------
 const float base=-29.3f;
 watShellEnd(m,base,-16.f,0,1,16.f);
 wall(m,1.f,0.f,2.f,16.f,base,-16.f,15);
 wall(m,22.f,0.f,23.f,6.f,base,-16.f,15);wall(m,22.f,12.f,23.f,16.f,base,-16.f,15);wall(m,22.f,10.f,23.f,12.f,base,L3-.22f,15);wall(m,22.f,10.f,23.f,12.f,L3+3.f,-16.f,15);wall(m,22.f,6.f,23.f,10.f,base,L3-.22f,15);
 wall(m,23.f,6.f,24.f,10.f,F,L3-.22f,15);wall(m,23.f,6.f,24.f,10.f,-16.f,R,15);
 watShellUpper(m,1,16.f,0.f,16.f);
 watLouvres(m,2.f,1,1.f,14.f);watLouvres(m,22.f,-1,.5f,3.2f);
 // --- The basin and the stair apron ------------------------------------------------
 const float bed=-29.2f,surf=-28.35f;
 m.waterVolumes.push_back({2.f,0.f,22.f,23.f,bed,surf});
 wall(m,18.f,0.f,22.f,3.5f,base,F,15);
 wall(m,18.f,3.5f,20.2f,3.68f,bed,F+.15f,15);wall(m,17.82f,0.f,18.f,3.68f,bed,F+.15f,15);
 floorLine(m,18.f,3.2f,20.2f,3.32f,F,13);
 watLiveWater(m,2.f,0.f,18.f,23.f,surf,80.f,.6f);watLiveWater(m,18.f,3.5f,22.f,23.f,surf,80.f,.6f);
 watHum(m,"wat_shell_hum_ms",2.f,0.f,22.f,23.f,F-2.f,F+3.f,.6f);
 // Stripped cells: bare beams on their columns, three packs left at the west.
 const float beamB=-21.6f,beamT=-21.2f;
 for(float x:{5.f,9.f,15.f})for(float y:{1.5f,5.5f})wall(m,x-.25f,y-.25f,x+.25f,y+.25f,bed,beamB,15);
 for(float y:{1.5f,5.5f})wall(m,2.f,y-.2f,18.f,y+.2f,beamB,beamT,15);
 for(float x:{5.f,9.f,15.f})wall(m,x-.2f,0.f,x+.2f,5.5f,beamB,beamT,15);
 for(Vec2 c:{Vec2{2.1f,1.6f},Vec2{4.1f,1.6f},Vec2{2.1f,3.6f}})wall(m,c.x,c.y,c.x+1.9f,c.y+1.9f,beamT,-19.8f,29);
 // New packs staged on the apron, and the replacement log.
 for(Vec2 c:{Vec2{18.95f,.95f}}){for(float d:{-.45f,.3f})wall(m,c.x-.55f,c.y+d,c.x+.55f,c.y+d+.15f,F,F+.14f,10);for(float z:{0.f,1.f})wall(m,c.x-.5f,c.y-.45f,c.x+.5f,c.y+.45f,F+.14f+z*.62f,F+.74f+z*.62f,29);}
 terminal(m,{21.25f,1.2f},"TOWER 2 / FILL REPLACEMENT","CELLS 5 TO 8 STRIPPED 10/02. NEW PACKS STAGED ON THE APRON.","SHELL CREW: THE SHELL HUMS WHEN THE PUMPS ARE OFF. LOGGED. NOT EXPLAINED.");
 m.terminals.back().yaw=-kPi*.5f;
 // --- The shell stair --------------------------------------------------------------------
 // East wall up from the apron, round the south end, back up the west wall
 // and across the gallery bridge.
 m.stairs.push_back({20.2f,3.5f,21.85f,9.1f,F,L1,20,true,true});
 deck(m,L1,20,9,22,17);deck(m,L1,19,17,21,18);deck(m,L1,18,18,21,19);deck(m,L1,17,19,20,20);deck(m,L1,14,20,19,21);deck(m,L1,14,21,18,22);
 m.stairs.push_back({9.f,20.f,14.6f,21.8f,L1,L2,20,false,false});
 deck(m,L2,6,21,9,22);deck(m,L2,5,20,9,21);deck(m,L2,4,19,7,20);deck(m,L2,3,18,6,19);deck(m,L2,3,17,5,18);deck(m,L2,2,15,4,17);
 m.stairs.push_back({2.1f,13.f,3.85f,15.8f,L2,L3,10,true,false});
 deck(m,L3,2,7,4,13);deck(m,L3,4,7,20,9);deck(m,L3,20,6,24,10);deck(m,L3,20,10,23,12);
 // Knee braces carry the wall runs; the bridge is a truss girder each side.
 for(float y:{10.f,12.f,14.f,16.f})m.pipes.push_back({{22.f,y},{20.15f,y},L1-1.3f,.05f,L1-.24f,6});
 for(float y:{7.5f,9.5f,11.5f})m.pipes.push_back({{2.f,y},{3.85f,y},L3-1.3f,.05f,L3-.24f,6});
 for(float y:{6.5f,9.5f})m.pipes.push_back({{22.f,y},{20.15f,y},L3-1.3f,.05f,L3-.24f,6});
 for(float y:{15.5f,16.5f})m.pipes.push_back({{2.f,y},{3.85f,y},L2-1.3f,.05f,L2-.24f,6});
 for(float y:{6.92f,9.f}){wall(m,4.f,y,20.f,y+.08f,L3-.62f,L3-.22f,12);
  m.pipes.push_back({{4.f,y+.04f},{20.f,y+.04f},L3+1.1f,.035f,L3+1.1f,6});
  for(float x=4.f;x<20.01f;x+=2.f)m.pipes.push_back({{x,y+.04f},{x,y+.04f},L3-.62f,.03f,L3+1.1f,6});
  for(float x=4.f;x<19.9f;x+=2.f)m.pipes.push_back({{x,y+.04f},{x+2.f,y+.04f},L3-.6f,.025f,L3+1.08f,6});}
 // --- The fill gallery door --------------------------------------------------------------
 for(float y:{5.8f,10.f})wall(m,21.8f,y,22.f,y+.2f,L3,L3+3.f,12);
 wall(m,21.8f,5.8f,22.f,10.2f,L3+2.8f,L3+3.f,12);
 terminal(m,{22.5f,11.45f},"TOWER 2 / FILL GALLERY BOOK","HEAT EXCHANGE THROUGH THE EAST DOOR. SIGN THE BOOK.","03:10 TWO FROM SECURITY CAME UP THE STAIR. DID NOT SIGN. ASKED WHO WAS ON SHIFT.",L3-F);
 // --- Light ------------------------------------------------------------------------------
 m.lights.push_back(watWall({21.98f,1.8f},F+2.4f,kPi,1.2f,6.f,4300,.2f));
 for(float y:{12.f,16.f})m.lights.push_back(watWall({21.98f,y},L1+2.2f,kPi,1.f,6.f,3000));
 m.lights.push_back(watWall({12.f,22.98f},L1+1.6f,-kPi*.5f,1.f,7.f,3000));
 m.lights.push_back(watWall({2.02f,15.5f},L2+2.2f,0,1.f,6.f,3000));
 m.lights.push_back(watWall({2.02f,11.f},L3+2.1f,0,1.f,6.f,3000));
 m.lights.push_back(watWall({21.98f,5.4f},L3+2.4f,kPi,1.6f,8.f,2400));m.lights.push_back(watWall({22.98f,11.f},L3+2.3f,kPi,1.1f,5.f,2600));
 m.lights.push_back(lamp({22.5f,8.f},-16.25f,1.f,4.f,3400));
 for(Vec2 b:{Vec2{8.f,8.f},Vec2{15.5f,20.8f}})m.lights.push_back(watSwitched(lamp(b,surf+.1f,1.5f,6.f,12000,-.05f,WorldLightMount::Bare),watIsolated,true,.3f));
 m.lights.push_back(watWall({2.02f,4.f},-17.f,0,2.f,16.f,1900,-.25f));
 m.lights.push_back(watWall({21.98f,13.f},-17.f,kPi,2.f,16.f,1900,-.25f));
 for(float z:{-5.f,7.f,17.f}){const float d=watThroatInset(z);
  m.lights.push_back(watWall({2.02f+d,8.f},z,0,.8f,5.f,1000,-.5f));
  m.lights.push_back(watWall({21.98f-d,8.f},z,kPi,.8f,5.f,1000,-.5f));}
 m.lights.push_back(lamp({12.f,10.f},R-.4f,2.f,14.f,1900,-.25f));
 // --- Encounters and script ---------------------------------------------------------
 m.creatureSpawns={{C::Wasp,{19.6f,18.5f},L1}};
 m.pickupSpawns={{{12.f,8.f},P::Ammo,L3},{{18.6f,2.4f},P::Health,F}};
 m.clutterSpawns={{0,{20.8f,15.f},L1},{2,{6.f,20.5f},L2},{1,{21.f,7.f},L3}};
 {auto e=watEvent("wat_shell_apron",18,0,22,3.5f,F-.5f,F+2,watIsolated);e.actions.push_back(watCheckpoint());m.scriptEvents.push_back(e);}
 {auto e=watEvent("wat_fill_gallery",20,6,24,10,L3-.5f,L3+2);e.actions.push_back(watCheckpoint());m.scriptEvents.push_back(e);}
 zone(m,"COOLING TOWER 2 / FILL GALLERY EAST",20,6,24,10,L3-.5f,L3+3);
 zone(m,"COOLING TOWER 2 / SHELL STAIR",0,0,24,24,-25.f,100);
 zone(m,"COOLING TOWER 2",0,0,24,24);
 freightFinish(m,-1);
 for(float z:{L1,L2,L3})watDeckRails(m,z);
 return p;
}
// ===============================================================================
// HEAT EXCHANGE (manifest 32). Tower 2's fill gallery door opens onto the
// secondary loop: six shell-and-tube exchangers in three rows, the hot headers
// over them, and walkways a body wide between. The reactor scrammed at 01:47;
// nothing should be heating this loop, and it is still climbing. Three tired
// gaskets lift every six seconds and blow steam across the walkways: the hiss
// comes first, then the scald. The way through snakes east, west and east
// again to the Maintenance District opening south. The operator who logged
// the climb lies by the plate exchangers. Security came up the stair first.
//
// A horizontal shell-and-tube exchanger along x at y cy, axis height axis,
// radius r: channel head and bonnet west, dished cover east, girth bands, two
// concrete saddles with steel cradles, shell nozzles up to the hot header and
// a channel nozzle up to the cold one, a drain line on shoes along the aisle
// side (side +1 south, -1 north), and a nameplate on the west saddle.
void watExchanger(Map& m,float x1,float x2,float cy,float r,float axis,const char* tag,const char* sub,int side,float hot,float cold){
 const float F=m.layers.front().elevation;
 m.pipes.push_back({{x1+.55f,cy},{x2-.3f,cy},axis,r,axis,5});
 m.pipes.push_back({{x1+.16f,cy},{x1+.55f,cy},axis,r*1.03f,axis,1});
 for(float x:{x1+.2f,x1+.52f})m.pipes.push_back({{x-.05f,cy},{x+.05f,cy},axis,r*1.13f,axis,6});
 m.pipes.push_back({{x1,cy},{x1+.16f,cy},axis,r*.86f,axis,6});
 m.pipes.push_back({{x2-.3f,cy},{x2-.12f,cy},axis,r*.94f,axis,5});m.pipes.push_back({{x2-.12f,cy},{x2,cy},axis,r*.66f,axis,5});
 m.pipes.push_back({{x2-.36f,cy},{x2-.26f,cy},axis,r*1.1f,axis,6});
 for(float x=x1+2.1f;x<x2-1.1f;x+=1.9f)m.pipes.push_back({{x-.04f,cy},{x+.04f,cy},axis,r+.045f,axis,6});
 for(float x:{x1+1.35f,x2-1.35f}){
  wall(m,x-.22f,cy-r*.82f,x+.22f,cy+r*.82f,F,axis-r*.6f,15);
  wall(m,x-.26f,cy-r*.9f,x+.26f,cy+r*.9f,axis-r*.6f,axis-r*.5f,12);
 }
 m.structures.push_back({x1-.02f,cy-r-.05f,x2,cy+r+.05f,F,axis+r,false,6});
 for(float x:{x1+1.f,x2-1.7f}){const Vec2 n{x,cy-.42f};
  m.pipes.push_back({n,n,axis+r*.8f,.15f,hot,4});for(float z:{axis+r+.08f,hot-.34f})m.pipes.push_back({n,n,z,.2f,z+.07f,6});}
 {const Vec2 n{x1+.36f,cy+.42f};
  m.pipes.push_back({n,n,axis+r*.8f,.13f,cold,3});for(float z:{axis+r+.1f,cold-.3f})m.pipes.push_back({n,n,z,.18f,z+.07f,6});}
 const float dy=cy+side*(r+.2f);
 m.pipes.push_back({{x1+.3f,dy},{x2-.3f,dy},F+.32f,.07f,F+.32f,0});
 for(float x=x1+.8f;x<x2-.3f;x+=2.2f){m.pipes.push_back({{x,dy},{x,dy},F,.03f,F+.25f,6});wall(m,x-.1f,dy-.1f,x+.1f,dy+.1f,F,F+.02f,8);}
 m.structures.push_back({x1+.3f,std::min(dy,cy+side*(r+.05f))-.06f,x2-.3f,std::max(dy,cy+side*(r+.05f))+.06f,F,F+.5f,false,6});
}
// A gasket that lifts: a wisp at the flange all the time, and every six
// seconds (the engine's hiss at each period start) scalding steam across the
// walkway from the shell face to the far side, between hazard-striped lines.
void watGasket(Map& m,float x,float face,int side,float far,float axis,float phase){
 const float F=m.layers.front().elevation;
 m.emitters.push_back({{x,face+side*.08f},axis-.15f,{0,side*1.1f},.55f,.85f,.06f,.45f,9});
 m.emitters.push_back({{x,face+side*.08f},axis+.35f,{0,side*.7f},.9f,.6f,.05f,.35f,6});
 Hazard h;h.kind=Hazard::Kind::Steam;h.x1=x-.7f;h.x2=x+.7f;h.y1=std::min(face,far);h.y2=std::max(face,far);h.bottom=F-.5f;h.top=F+2.6f;
 h.damagePerSecond=34;h.period=6;h.onTime=2.3f;h.phase=phase;m.hazards.push_back(h);
 for(float d:{-.7f,.6f})floorLine(m,x+d,std::min(face,far),x+d+.1f,std::max(face,far),F,13);
}
std::shared_ptr<const Map> makeHeatExchange(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[52];m.definition=CampaignChunks[52];m.definition.ambient=.2f;
 const float F=-19,R=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),F));
 watSurfaces(m,SurfaceConcrete,SurfaceConcrete,SurfaceConcrete);
 openEdge(m,{'W',6,10,F,false,0});openEdge(m,{'S',10,14,F,false,0});
 rectangle(m.layers[0],1,1,5,5,'#');
 m.definition.playerStart={1.8f,8.f};m.definition.spawnHeight=F;m.definition.spawnYaw=0;m.definition.spawnPitch=-4.f;
 // --- The fill gallery door and the vestibule -----------------------------------------
 wall(m,0,6,1,10,F+3.f,R,15);
 for(float y:{5.8f,10.f})wall(m,.8f,y,1.f,y+.2f,F,F+3.f,12);
 wall(m,.8f,5.8f,1.f,10.2f,F+2.8f,F+3.f,12);
 wall(m,5.f,6.95f,5.2f,10.2f,F,R,15);wall(m,1.f,10.f,5.2f,10.2f,F,R,15);
 floorLine(m,1.f,7.9f,5.f,8.1f,F,14);
 wallSign(m,5.f-.055f,8.6f,F+1.35f,"GASKETS LIFTING","E-301 / E-304 / E-305 / STAND CLEAR WHEN IT HISSES",2.4f,-kPi*.5f);
 terminal(m,{2.4f,5.75f},"HEAT EXCHANGE / ROUNDS","LOOP 02:10 88 C. 02:40 91 C. 03:05 96 C. REACTOR SCRAMMED 01:47.","NOTHING IS HEATING THIS LOOP. IT IS STILL CLIMBING.");
 m.terminals.back().yaw=.001f;
 m.fixtures.push_back({model::pwr_locker_bank,{1.45f,9.45f},0,.87f,.85f,2.42f,-kPi*.5f,true});
 m.fixtures.push_back({model::pwr_fire_cabinet,{4.88f,6.5f},1.1f,.25f,.2f,.56f,kPi*.5f,false});
 m.fixtures.push_back({model::lab_wall_clock,{3.9f,5.05f},2.05f,.4f,.03f,.4f,kPi,false});
 // --- Exchanger rows ----------------------------------------------------------------------
 const float axis=F+1.75f,r=1.2f,hotA=-14.f,coldA=-13.3f;
 struct WatRow{float cy,x1a,x2a,x1b,x2b;const char* a;const char* b;int side;float west;};
 const WatRow rows[]={{3.6f,5.2f,12.4f,13.6f,20.6f,"E-301","E-302",1,5.f},{8.2f,5.2f,12.4f,13.6f,20.8f,"E-303","E-304",-1,5.2f},{12.8f,6.6f,13.4f,14.6f,21.2f,"E-305","E-306",-1,1.f}};
 for(const auto& w:rows){
  watExchanger(m,w.x1a,w.x2a,w.cy,r,axis,w.a,"SECONDARY / 6 BAR",w.side,hotA,coldA);
  watExchanger(m,w.x1b,w.x2b,w.cy,r,axis,w.b,"SECONDARY / 6 BAR",w.side,hotA,coldA);
  pipeRoute(m,{{w.west,w.cy-.42f,hotA},{23.f,w.cy-.42f,hotA}},.24f,4,R);
  pipeRoute(m,{{w.west,w.cy+.42f,coldA},{23.f,w.cy+.42f,coldA}},.2f,3,R);
 }
 // E-303/E-304 face the second walkway on their south side too.
 {const float dy=8.2f+r+.2f;m.pipes.push_back({{5.5f,dy},{20.5f,dy},F+.32f,.07f,F+.32f,0});
  for(float x=6.f;x<20.4f;x+=2.2f)m.pipes.push_back({{x,dy},{x,dy},F,.03f,F+.25f,6});
  m.structures.push_back({5.5f,8.2f+r,20.5f,dy+.06f,F,F+.5f,false,6});}
 // The crossover stations between the units of rows B and C: no way through.
 for(auto [cx,cy]:{std::pair{13.f,8.2f},std::pair{14.f,12.8f}}){
  wall(m,cx-.6f,cy-1.15f,cx+.6f,cy+1.15f,F,F+.2f,15);
  for(float d:{-.5f,.5f}){const Vec2 c{cx,cy+d};m.pipes.push_back({c,c,F+.2f,.16f,d<0?hotA:coldA,d<0?4:3});
   for(float z:{F+.25f,F+1.6f})m.pipes.push_back({c,c,z,.21f,z+.07f,6});}
  for(float s:{-1.f,1.f})m.fixtures.push_back({model::valve_wheel,{cx,cy+s*1.2f},1.1f,.5f,.12f,.5f,s>0?0.f:kPi,false});
  m.structures.push_back({cx-.6f,cy-1.15f,cx+.6f,cy+1.15f,F,F+3.f,false,6});
  wall(m,cx-.6f,cy-1.15f,cx+.6f,cy-1.1f,F+.2f,F+2.6f,8);wall(m,cx-.6f,cy+1.1f,cx+.6f,cy+1.15f,F+.2f,F+2.6f,8);
 }
 // Row C's east end: the loop's relief valves on a stand against the wall.
 wall(m,21.2f,11.5f,23.f,14.1f,F,F+.2f,15);
 for(float y:{12.2f,13.4f}){m.pipes.push_back({{22.2f,y},{22.2f,y},F+.2f,.18f,F+2.4f,4});m.pipes.push_back({{22.2f,y},{22.2f,y},F+2.4f,.26f,F+2.9f,6});
  m.pipes.push_back({{22.2f,y},{22.2f,y},F+2.9f,.1f,R,1});}
 m.structures.push_back({21.2f,11.5f,23.f,14.1f,F,F+3.f,false,6});
 // --- The gaskets -------------------------------------------------------------------------
 watGasket(m,9.85f,3.6f+r,1,6.95f,axis,0.f);
 watGasket(m,16.35f,8.2f+r,1,11.55f,axis,0.f);
 watGasket(m,8.45f,12.8f-r,-1,9.45f,axis,3.f);
 // --- The north service strip and the pump bay -----------------------------------------
 for(float x:{8.f,16.f})m.fixtures.push_back({model::lab_pressure_gauge,{x,1.06f},1.4f,.29f,.11f,.41f,kPi,false});
 cableTray(m,{5.2f,1.25f},{20.4f,1.25f},F+3.4f);
 for(float y:{1.95f,3.75f}){P3 t=pumpSkid(m,{21.65f,y},F,0,-14.6f);(void)t;}
 pipeRoute(m,{{21.4f,1.95f,-14.6f},{21.4f,3.75f,-14.6f},{23.f,3.75f,-14.6f}},.11f,3,R);
 // --- South: plate exchangers, loop pumps, the control room, the way on ------------------
 for(float x:{8.7f,16.1f}){
  wall(m,x-.5f,18.4f,x+.5f,18.6f,F,F+2.1f,19);wall(m,x-.5f,21.4f,x+.5f,21.6f,F,F+2.1f,19);
  for(float y=18.66f;y<21.36f;y+=.09f)wall(m,x-.42f,y,x+.42f,y+.06f,F+.12f,F+1.95f,20);
  for(float dx:{-.3f,.3f})wall(m,x+dx-.04f,18.4f,x+dx+.04f,21.6f,F+2.1f,F+2.2f,12);
  for(float dx:{-.25f,.25f}){m.pipes.push_back({{x+dx,18.4f},{x+dx,17.95f},F+1.5f,.1f,F+1.5f,dx<0?4:3});
   m.pipes.push_back({{x+dx,17.95f},{x+dx,17.95f},F+1.4f,.1f,F+4.2f,dx<0?4:3});
   pipeRoute(m,{{x+dx,17.95f,F+4.2f},{x+dx,23.f,F+4.2f}},.1f,dx<0?4:3,R);}
  m.structures.push_back({x-.5f,17.8f,x+.5f,21.6f,F,F+2.2f,false,6});
 }
 for(float y:{18.2f,21.f}){P3 t=pumpSkid(m,{20.2f,y},F,kPi*.5f,-14.6f);(void)t;}
 pipeRoute(m,{{20.2f,18.45f,-14.6f},{20.2f,21.25f,-14.6f},{23.f,21.25f,-14.6f}},.11f,3,R);
 vessel(m,{21.4f,16.1f},.75f,F,F+3.2f,5);
 // The control room, south-west: glazed onto the walkway, its door east.
 const float ct=F+2.8f;
 watGlazed(m,1.f,17.9f,6.5f,18.05f,F,2.8f);
 watGlazed(m,6.38f,18.05f,6.5f,19.2f,F,2.8f);watGlazed(m,6.38f,20.5f,6.5f,23.f,F,2.8f);
 wall(m,6.38f,19.2f,6.5f,20.5f,F+2.2f,ct,3);for(float y:{19.12f,20.5f})wall(m,6.33f,y,6.55f,y+.08f,F,F+2.2f,12);
 wall(m,1.f,17.9f,6.5f,23.f,ct,ct+.15f,2);
 workstation(m,3.4f,21.9f,F,-1,2);
 terminal(m,{2.f,18.6f},"HX CONTROL / E-301 TO E-306","LOOP 101 C 03:30. RELIEF LIFTING. NO HEAT SOURCE ON ANY RECORD.","MANUAL NOTE: PUT YOUR HAND ON E-304. IT HUMS. LIKE THE BASIN.");
 m.terminals.back().yaw=.001f;
 for(float x:{1.6f,2.55f})m.fixtures.push_back({model::electrical_cabinet,{x,22.73f},0,.9066f,.4956f,2.2f,0,true});
 m.fixtures.push_back({model::lab_control_board,{5.2f,22.9f},1.f,1.32f,.19f,.6f,kPi,false});
 m.fixtures.push_back({model::pwr_fire_cabinet,{1.12f,20.5f},1.1f,.25f,.2f,.56f,-kPi*.5f,false});
 // The operator: told to wait, and he waited.
 watBody(m,"wat_hx_operator",{12.9f,19.6f},F,kPi*.8f);
 floorLine(m,12.5f,19.1f,13.6f,19.9f,F,16);
 terminal(m,{8.f,16.9f},"HX / SHIFT HANDOVER","03:14 TWO FROM SECURITY TOOK THE ROUNDS SHEET. ASKED WHO ELSE READ THE LOOP.","03:16 TOLD TO WAIT HERE FOR THE NEXT SHIFT.");
 // The way on: a framed opening south to the Maintenance District.
 wall(m,10.f,23.f,14.f,24.f,F+3.2f,R,15);
 for(float x:{9.6f,14.f})wall(m,x,22.6f,x+.4f,23.f,F,F+3.2f,12);
 wall(m,9.6f,22.6f,14.4f,23.f,F+3.f,F+3.2f,12);
 for(float y:{16.f,18.5f,21.f})floorLine(m,11.9f,y,12.1f,y+1.6f,F,14);
 // --- Structure and services ------------------------------------------------------------
 wallBays(m,F,R,4.f,false,true,false,false,8.f,12.f);
 cableTray(m,{7.f,22.8f},{22.6f,22.8f},F+3.6f);
 servicePipe(m,{1.2f,22.75f},{22.8f,22.75f},F+4.6f,.08f,0);
 for(float x:{6.f,12.f,18.f})wall(m,x-.15f,1.f,x+.15f,23.f,R-.5f,R,12);
 // --- Light -------------------------------------------------------------------------------
 for(float x:{8.f,15.5f,20.f})m.lights.push_back(lamp({x,5.9f},-15.2f,1.3f,7.f,1800,-.25f));
 m.lights.push_back(watFlicker(lamp({18.f,10.5f},-15.2f,1.2f,7.f,1800,-.25f),.35f));
 for(float x:{7.f,12.5f})m.lights.push_back(lamp({x,10.5f},-15.2f,1.2f,7.f,1800,-.25f));
 m.lights.push_back(lamp({21.9f,6.2f},-15.2f,1.f,6.f,1800,-.25f));
 m.lights.push_back(lamp({3.5f,13.f},-15.2f,1.1f,7.f,1800,-.25f));
 for(Vec2 b:{Vec2{10.f,15.6f},Vec2{17.f,15.8f},Vec2{18.f,20.f}})m.lights.push_back(lamp(b,-15.2f,1.2f,8.f,1900,-.2f));
 m.lights.push_back(lamp({12.f,21.8f},-15.6f,1.3f,7.f,6200,.35f));
 m.lights.push_back(lamp({3.7f,20.5f},ct-.12f,1.4f,6.f,5600,.2f));
 m.lights.push_back(watWall({3.f,5.02f},F+2.6f,kPi*.5f,1.f,5.f,3000));
 m.lights.push_back(watWall({1.02f,13.5f},F+2.4f,0,1.f,5.f,1100,-.45f));
 m.lights.push_back(watWall({22.98f,9.5f},F+2.4f,kPi,1.f,5.f,1100,-.45f));
 m.lights.push_back(watFlicker(watWall({22.98f,2.8f},F+2.6f,kPi,.9f,5.f,1100,-.45f),.25f));
 for(float x:{7.f,13.f,18.5f})m.lights.push_back(watWall({x,1.02f},F+2.2f,kPi*.5f,.7f,4.f,2400,-.1f));
 // --- Encounters and script ----------------------------------------------------------------
 m.creatureSpawns={{C::Wasp,{17.5f,17.2f}},{C::Wasp,{4.f,15.6f}},{C::Huntsman,{20.5f,6.f}}};
 m.pickupSpawns={{{13.f,1.7f},P::Ammo,F},{{2.2f,20.2f},P::Health,F},{{19.4f,5.9f},P::Ammo,F}};
 m.clutterSpawns={{0,{4.2f,7.f},F},{2,{10.6f,19.9f},F},{1,{5.6f,21.2f},F},{3,{14.4f,6.4f},F},{0,{3.f,14.6f},F}};
 {auto e=watEvent("wat_hx_arrival",0,5,5,10,F-.5f,F+2);e.actions.push_back(watCheckpoint());m.scriptEvents.push_back(e);}
 zone(m,"HEAT EXCHANGE / HX CONTROL",1,17.9f,6.5f,23);
 zone(m,"HEAT EXCHANGE / EXCHANGER ROWS",5,1,23,14.1f);
 zone(m,"HEAT EXCHANGE",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
}
void buildChapterWater(CampaignMapTable& maps){
 maps[47]=makeWaterTreatment();
 maps[48]=makeDrainageGallery();
 maps[49]=makeSumpWorks();
 maps[50]=makeCoolingTowersBasin();
 maps[51]=makeCoolingTowersShell();
 maps[52]=makeHeatExchange();
}
}

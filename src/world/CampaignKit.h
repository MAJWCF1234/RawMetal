#pragma once
// The campaign map kit: the shared helpers every authored chunk is built from.
// CampaignMaps.cpp and the chapter files (CampaignChapter*.cpp) include it.
#include "World.h"
#include "ModelIds.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <tuple>
namespace retro {
namespace kit {
using Map=AuthoredMapData;
inline void rectangle(AuthoredLayerData& layer,int x1,int y1,int x2,int y2,char tile){
 for(int y=y1;y<y2;++y)for(int x=x1;x<x2;++x)layer.rows[y][x]=tile;
}
inline AuthoredLayerData floorLayer(const char* name,float z,bool openNorth=false,bool openSouth=false){
 AuthoredLayerData layer;layer.name=name;layer.elevation=z;
 for(int y=0;y<24;++y)layer.rows[y]=std::string(24,'.');
 rectangle(layer,0,0,1,24,'#');rectangle(layer,23,0,24,24,'#');
 if(!openNorth)rectangle(layer,0,0,24,1,'#');
 if(!openSouth)rectangle(layer,0,23,24,24,'#');
 return layer;
}
inline void wall(Map& m,float x1,float y1,float x2,float y2,float lo,float hi,int material=2){m.structures.push_back({x1,y1,x2,y2,lo,hi,false,material});}
inline void cargo(Map& m,float x,float y,float floor,bool loaded=true,float yaw=0){
 m.fixtures.push_back({15,{x,y},floor-m.layers.front().elevation,1.2f,1.15f,.16f,yaw,true});
 if(loaded)m.fixtures.push_back({17,{x,y},floor+.16f-m.layers.front().elevation,1.1f,1.f,.99f,yaw,true});
}
inline void board(Map& m,float x,float y,float z,const char* title,const char* subtitle,float width=3.f){
 // Every sign has a steel backing and two supports rooted in the floor.
 wall(m,x-width*.5f,y,x+width*.5f,y+.16f,z,z+.9f,2);
 for(float dx:{-width*.44f,width*.44f})wall(m,x+dx-.04f,y+.04f,x+dx+.04f,y+.12f,m.layers.front().elevation,z,2);
 m.signs.push_back({{x,y-.012f},z,width,.9f,kPi,title,subtitle});
}
inline void terminal(Map& m,Vec2 p,const char* title,const char* a,const char* b,float z=0,StateId state=0,StateId prerequisite=0){
 AuthoredTerminalData t;t.position=p;t.title=title;t.line1=a;t.line2=b;t.z=z;t.activateState=state;t.requireState=prerequisite;m.terminals.push_back(std::move(t));
}
inline void deck(Map& m,float z,int x1,int y1,int x2,int y2){
 for(auto& l:m.layers)if(l.thickness>0&&std::fabs(l.elevation-z)<.01f){rectangle(l,x1,y1,x2,y2,'=');return;}
 AuthoredLayerData l;l.name="Freight steel walkway";l.elevation=z;l.thickness=.22f;for(auto& row:l.rows)row=std::string(24,'.');rectangle(l,x1,y1,x2,y2,'=');m.layers.push_back(std::move(l));
}
inline void rack(Map& m,float x,float y,float floor,int tiers=4,bool braceNorth=false){
 for(float xx:{x,x+2.6f})for(float yy:{y,y+3.8f})wall(m,xx,yy,xx+.16f,yy+.16f,floor,floor+tiers*3.f,12);
 for(int tier=0;tier<tiers;++tier){float z=floor+tier*3.f;
  wall(m,x,y,x+2.72f,y+4,z,z+.12f,2);
  wall(m,x,y-.07f,x+2.76f,y+.07f,z+.12f,z+.30f,18);
  wall(m,x,y+3.86f,x+2.76f,y+4.02f,z+.12f,z+.30f,18);
  for(int pallet=0;pallet<2;++pallet){float yy=y+.3f+pallet*1.8f;
   if(tier==0)cargo(m,x+1.36f,yy+.7f,z+.14f);
   else {
    // Open stock slots and varied loads reveal the shelf depth; every load
    // rests on a pallet rather than becoming an identical black wall.
    int slot=tier*2+pallet;
    if(slot%5==0){cargo(m,x+1.36f,yy+.7f,z+.14f,false);continue;}
    for(int column=0;column<2;++column){float xx=x+.8f+column*1.15f;
     m.fixtures.push_back({15,{xx,yy+.7f},z+.14f-m.layers.front().elevation,1.08f,1.38f,.16f,0,true});
     float height=slot%3==0?.8f:slot%3==1?1.25f:1.65f;
     m.fixtures.push_back({17,{xx,yy+.7f},z+.30f-m.layers.front().elevation,1.02f,1.28f,height,(slot+column)%2?kPi:0,true});
    }
   }
  }
  // Rear diagonal bracing remains inside the rack footprint and clear of
  // the personnel aisle and shelf lamps.
  // braceNorth puts it on the north face, for a rack whose back is an aisle.
  const float brace=braceNorth?y+.1f:y+3.9f;
  m.pipes.push_back({{x+.12f,brace},{x+2.6f,brace},z+.35f,.035f,z+2.65f,1});
  m.pipes.push_back({{x+.12f,brace},{x+2.6f,brace},z+2.65f,.035f,z+.35f,1});
 }
}
inline void container(Map& m,float x,float y,float floor,bool open=false,int material=11){
 // A container is a room with a real walk-in mouth, floor, sides and roof.
 wall(m,x,y,x+.12f,y+6,floor,floor+2.8f,material);wall(m,x+2.9f,y,x+3.02f,y+6,floor,floor+2.8f,material);
 wall(m,x,y+5.85f,x+3.02f,y+6,floor,floor+2.8f,material);
 wall(m,x,y,x+3.02f,y+6,floor+2.7f,floor+2.8f,material);
 if(!open)wall(m,x,y,x+3.02f,y+.12f,floor,floor+2.7f,material);
 else m.clutterSpawns.push_back({3,{x+1.5f,y+3.f},floor});
 // Corrugated side skins and end frames turn broad rust into assembled steel.
 for(float yy=y;yy<y+6;yy+=.65f)for(float xx:{x-.025f,x+2.985f})wall(m,xx,yy,xx+.06f,yy+.08f,floor+.12f,floor+2.7f,2);
 for(float yy:{y,y+5.86f})wall(m,x-.04f,yy,x+3.06f,yy+.12f,floor+2.7f,floor+2.86f,12);
}
inline void floorLine(Map& m,float x1,float y1,float x2,float y2,float floor,int material=14){wall(m,x1,y1,x2,y2,floor+.007f,floor+.015f,material);}
inline void wallLamp(Map& m,float x,float y,float z,float yaw,float intensity=1.f,float range=7.f){
 m.lights.push_back({{x,y},z,WorldLightMount::Wall,yaw,intensity,range});
}
inline void servicePipe(Map& m,Vec2 a,Vec2 b,float z,float radius,int paint){
 // An end within 0.7 m of a perimeter wall runs on into the wall face.
 // Across an open seam the run continues to the chunk edge, into the next hall.
 auto snap=[&](float& v,bool openLow,bool openHigh){if(v<1.7f&&v>.9f)v=openLow?0.f:1.f;else if(v>22.3f&&v<23.1f)v=openHigh?24.f:23.f;};
 if(std::fabs(a.x-b.x)<.01f){snap(a.y,m.openNorth,m.openSouth);snap(b.y,m.openNorth,m.openSouth);}
 else if(std::fabs(a.y-b.y)<.01f){snap(a.x,m.openWest,m.openEast);snap(b.x,m.openWest,m.openEast);}
 m.pipes.push_back({a,b,z,radius,z,paint});
 Vec2 d=b-a;float length=std::sqrt(d.x*d.x+d.y*d.y);if(length<.01f)return;
 Vec2 unit=d*(1.f/length);
 for(float t=.4f;t<length;t+=2.5f){Vec2 c=a+unit*t;
  m.pipes.push_back({c-unit*.045f,c+unit*.045f,z,radius*1.32f,z,2});
  // The clamp hangs from the nearest surface overhead: a deck or the roof.
  float top=m.definition.ceiling-.05f;int tx=int(std::floor(c.x)),ty=int(std::floor(c.y));
  if(tx>=0&&ty>=0&&tx<24&&ty<24)for(const auto& l:m.layers)if(l.thickness>0&&l.rows[size_t(ty)][size_t(tx)]=='='&&l.elevation-l.thickness>z)top=std::min(top,l.elevation-l.thickness-.04f);
  if(top>z+radius+.05f)m.pipes.push_back({c,c,z+radius*1.32f,.022f,top,6});
 }
}
inline void wallSign(Map& m,float x,float y,float z,const char* title,const char* subtitle,float width=2.f,float yaw=kPi){
 // Signs fasten to their architectural face, not free-standing floor poles.
 float dx=std::cos(yaw),dy=std::sin(yaw);
 float hx=std::abs(dx)*width*.5f+std::abs(dy)*.055f;
 float hy=std::abs(dy)*width*.5f+std::abs(dx)*.055f;
 wall(m,x-hx,y-hy,x+hx,y+hy,z-.02f,z+.67f,2);
 m.signs.push_back({{x+dy*.06f,y+dx*.06f},z,width,.65f,yaw,title,subtitle});
}
inline void equipmentPad(Map& m,float x1,float y1,float x2,float y2,float floor){
 wall(m,x1,y1,x2,y2,floor,floor+.10f,15);
 floorLine(m,x1,y1,x2,y1+.12f,floor+.10f,13);
 floorLine(m,x1,y2-.12f,x2,y2,floor+.10f,13);
}
inline void serviceCeiling(Map& m,float x1,float y1,float x2,float y2,float roof){
 // A layered service soffit gives human spaces a ceiling with actual depth.
 wall(m,x1,y1,x2,y2,roof,roof+.16f,2);
 for(float y=y1+.35f;y<y2;y+=3.f){
  wall(m,x1,y,x2,y+.16f,roof-.28f,roof,18);
  for(float x:{x1,x2-.18f})wall(m,x,y-.035f,x+.18f,y+.20f,roof-.46f,roof,13);
 }
 for(float x:{x1+.35f,x2-.45f}){
  servicePipe(m,{x,y1},{x,y2},roof-.37f,.085f,x<x1+1?3:4);
  for(float y=y1+.5f;y<y2;y+=3.f)wall(m,x-.16f,y,x+.16f,y+.065f,roof-.50f,roof,2);
  // End straps at both ends of the soffit run.
  for(float y:{y1-.02f,y2-.16f})wall(m,x-.16f,y,x+.16f,y+.18f,roof-.50f,roof,2);
 }
}
inline void actor(Map& m,const char* timer,ActorVisual visual,float scale,std::initializer_list<ActorKey> keys,bool loop=false,bool tool=false){m.actorTracks.push_back({stateId(timer),visual,scale,loop,tool,keys});}
inline void cue(Map& m,const char* timer,int time,Sound sound,Vec2 position,const char* caption="",float gain=1,float z=std::numeric_limits<float>::quiet_NaN()){m.sequenceCues.push_back({stateId(timer),time,sound,position,gain,1,caption,z});}
inline void machineryClock(Map& m,const char* timer,int period){TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_cycled");s.durationMs=period;s.finishAtMs=period;s.soundIntervalMs=0;s.loop=true;m.timedSequences.push_back(s);}
inline void handCart(Map& m,float x,float y,float floor){
 wall(m,x-.42f,y-.6f,x+.42f,y+.6f,floor+.2f,floor+.29f,12);
 for(float xx:{x-.47f,x+.37f})for(float yy:{y-.4f,y+.4f})m.pipes.push_back({{xx,yy},{xx+.1f,yy},floor+.14f,.14f,floor+.14f,1});
 for(float xx:{x-.38f,x+.34f})wall(m,xx,y+.56f,xx+.04f,y+.6f,floor+.27f,floor+1.02f,12);
 wall(m,x-.38f,y+.56f,x+.38f,y+.6f,floor+.97f,floor+1.02f,12);
}
inline void forklift(Map& m,float x,float y,float floor){
 // The supplied complete forklift retains its source proportions and atlases.
 // Forward is -Y at yaw 0; the parked vehicle leaves the centre aisle clear.
 m.fixtures.push_back({23,{x,y},floor-m.layers.front().elevation,1.1641f,3.6204f,2.2f,0,true});
}
inline void rail(Map& m,float x,float y1,float y2,float z){
 for(float dx:{-.7f,.7f})wall(m,x+dx-.045f,y1,x+dx+.045f,y2,z,z+.07f,2);
 for(float y=y1;y<y2;y+=.8f)wall(m,x-1.f,y,x+1.f,y+.12f,z-.02f,z+.015f,2);
}
// A running line along y at x: it continues through an open seam and ends
// at a buffer stop (steel frame, timber beam, hazard face) against a wall.
inline void trackRun(Map& m,float x,float floor){
 float y1=m.openNorth?0.f:1.9f,y2=m.openSouth?24.f:22.1f;
 rail(m,x,y1,y2,floor);
 auto stop=[&](float y,float dir){
  for(float dx:{-.7f,.7f}){wall(m,x+dx-.05f,y,x+dx+.05f,y+dir*.9f,floor,floor+.07f,2);wall(m,x+dx-.06f,y+dir*.82f,x+dx+.06f,y+dir*.9f,floor,floor+1.15f,12);}
  wall(m,x-1.f,std::min(y+dir*.9f,y+dir*.9f-dir*.25f),x+1.f,std::max(y+dir*.9f,y+dir*.9f-dir*.25f),floor+.7f,floor+1.f,10);
  wall(m,x-1.f,y+dir*.9f-(dir>0?.26f:-.01f),x+1.f,y+dir*.9f-(dir>0?.25f:-.02f),floor+.72f,floor+.98f,13);
  m.structures.push_back({x-1.f,std::min(y,y+dir*.9f),x+1.f,std::max(y,y+dir*.9f),floor,floor+1.15f,false,6});
 };
 if(!m.openNorth)stop(1.f,1.f);if(!m.openSouth)stop(23.f,-1.f);
}
inline void locomotive(Map& m,float x,float y,float floor,bool stripped=false){
 // Rail gauge, underframe and bogies share one centreline. The suspended
 // inspection chassis retains the same footprint as the complete engine.
 for(float yy:{y+1.5f,y+5.5f}){
  wall(m,x-.65f,yy-.76f,x+.65f,yy+.76f,floor+.52f,floor+.78f,8);
  for(float axle:{yy-.52f,yy+.52f}){
   m.pipes.push_back({{x-.85f,axle},{x+.85f,axle},floor+.48f,.085f,floor+.48f,2});
   for(float dx:{-.82f,.82f}){
    m.pipes.push_back({{x+dx-.10f,axle},{x+dx+.10f,axle},floor+.48f,.44f,floor+.48f,2});
    m.pipes.push_back({{x+dx-.12f,axle},{x+dx+.12f,axle},floor+.48f,.17f,floor+.48f,2});
    wall(m,x+dx-.10f,axle-.11f,x+dx+.10f,axle+.11f,floor+.78f,floor+.99f,8);
   }
  }
 }
 m.structures.push_back({x-1.15f,y-.4f,x+1.15f,y+7.4f,floor,floor+(stripped?1.3f:1.22f),false,6});
 if(!stripped)wall(m,x-1.25f,y,x+1.25f,y+7.f,floor+.96f,floor+1.22f,2);
 for(float yy:{y-.4f,y+7.f})wall(m,x-.28f,yy,x+.28f,yy+.4f,floor+.7f,floor+1.f,8);
 if(stripped){
  for(float dx:{-1.25f,1.03f})wall(m,x+dx,y,x+dx+.22f,y+7.f,floor+.96f,floor+1.3f,12);
  for(float yy:{y+.15f,y+1.5f,y+3.5f,y+5.5f,y+6.65f})wall(m,x-1.25f,yy,x+1.25f,yy+.20f,floor+1.03f,floor+1.27f,12);
  m.pipes.push_back({{x,y+.25f},{x,y+6.75f},floor+1.13f,.13f,floor+1.13f,1});
  return;
 }
 wall(m,x-1.08f,y+.3f,x+1.08f,y+3.7f,floor+1.22f,floor+2.65f,2);
 for(float dx:{-1.10f,1.08f}){
  for(int vent=0;vent<5;++vent)wall(m,x+dx,y+.7f,x+dx+.02f,y+3.25f,floor+1.75f+vent*.14f,floor+1.79f+vent*.14f,8);
  wall(m,x+dx,y+.45f,x+dx+.02f,y+3.55f,floor+1.48f,floor+1.60f,14);
 }
 wall(m,x-.65f,y+1.f,x+.65f,y+2.9f,floor+2.65f,floor+2.88f,8);
 wall(m,x-.18f,y+2.5f,x+.18f,y+2.9f,floor+2.88f,floor+3.45f,8);
 wall(m,x-1.2f,y+4.f,x+1.2f,y+6.8f,floor+1.22f,floor+3.5f,2);
 wall(m,x-1.3f,y+3.9f,x+1.3f,y+6.9f,floor+3.5f,floor+3.66f,2);
 wall(m,x-.96f,y+3.97f,x-.12f,y+4.01f,floor+2.48f,floor+3.25f,9);
 wall(m,x+.12f,y+3.97f,x+.96f,y+4.01f,floor+2.48f,floor+3.25f,9);
 for(float dx:{-1.21f,1.19f})wall(m,x+dx,y+4.35f,x+dx+.02f,y+5.75f,floor+2.48f,floor+3.25f,9);
 for(float dx:{-1.5f,1.25f})wall(m,x+dx,y+4.2f,x+dx+.25f,y+6.5f,floor+.55f,floor+.72f,2);
}
inline void office(Map& m,float x,float y,float floor,const char* title,const char* line1,const char* line2,StateId state=0,bool observation=false){
 if(observation){
  for(auto span:{std::pair{floor,floor+.85f},std::pair{floor+2.45f,floor+2.7f}}){
   wall(m,x-1.4f,y-1.5f,x-1.25f,y+2.f,span.first,span.second,3);
   wall(m,x-1.4f,y+1.85f,x+2.3f,y+2.f,span.first,span.second,3);
  }
  for(float yy:{y-1.5f,y+.2f,y+1.85f})wall(m,x-1.4f,yy,x-1.25f,yy+.08f,floor+.85f,floor+2.45f,2);
  for(float xx:{x-1.4f,x+.4f,x+2.2f})wall(m,xx,y+1.85f,xx+.08f,y+2.f,floor+.85f,floor+2.45f,2);
 }else{
  wall(m,x-1.4f,y-1.5f,x-1.25f,y+2.f,floor,floor+2.7f,3);
  wall(m,x-1.4f,y+1.85f,x+2.3f,y+2.f,floor,floor+2.7f,3);
 }
 terminal(m,{x,y},title,line1,line2,floor-m.layers.front().elevation,state);
 m.fixtures.push_back({7,{x+1.8f,y+1.35f},floor-m.layers.front().elevation,1.4f,.45f,1.6f,kPi,true});
 m.lights.push_back({{x,y},floor+2.55f});
}
inline void bench(Map& m,float x,float y,float floor){
 // Open space below a seated-height bench, rather than a solid cargo block.
 wall(m,x-1.35f,y-.25f,x+1.35f,y+.25f,floor+.43f,floor+.51f,10);
 wall(m,x-1.35f,y+.20f,x+1.35f,y+.28f,floor+.55f,floor+1.05f,10);
 for(float dx:{-1.f,1.f})wall(m,x+dx-.06f,y-.20f,x+dx+.06f,y+.20f,floor,floor+.43f,2);
}
inline void recordsDesk(Map& m,float x,float y,float floor){
 m.fixtures.push_back({19,{x,y},floor-m.layers.front().elevation,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({18,{x,y-.73f},floor-m.layers.front().elevation,.417f,.525f,.95f,kPi,true});
 // Manifest sheets sit in steel in/out trays, with leg space under the desk.
 for(float dx:{-.36f,.36f}){
  wall(m,x+dx-.25f,y-.22f,x+dx+.25f,y+.22f,floor+.78f,floor+.81f,2);
  wall(m,x+dx-.21f,y-.18f,x+dx+.21f,y+.18f,floor+.81f,floor+.825f,15);
  for(float line:{-.10f,0.f,.10f})wall(m,x+dx-.15f,y+line,x+dx+.13f,y+line+.008f,floor+.825f,floor+.826f,8);
 }
}
// ---------------------------------------------------------------------------
// Maps 0-3: the original facility, rebuilt as authored maps.
// ---------------------------------------------------------------------------
inline WorldLight lamp(Vec2 p,float z,float intensity,float range,float kelvin,float tint=0,WorldLightMount mount=WorldLightMount::Ceiling,float yaw=0){
 WorldLight l{p,z,mount,yaw,intensity,range};l.kelvin=kelvin;l.tint=tint;return l;
}
inline void zone(Map& m,const char* name,float x1,float y1,float x2,float y2,float bottom=-1000,float top=1000){m.zones.push_back({name,x1,y1,x2,y2,bottom,top});}
// Opens a major area: its title card shows the first time the player walks in.
inline void titleCard(Map& m,const char* title,float x1=0,float y1=0,float x2=24,float y2=24){AuthoredZone z;z.x1=x1;z.y1=y1;z.x2=x2;z.y2=y2;z.title=title;m.zones.push_back(std::move(z));}
inline void hazard(Map& m,Hazard::Kind kind,float x1,float y1,float x2,float y2,float bottom,float top,float damage){Hazard h;h.kind=kind;h.x1=x1;h.y1=y1;h.x2=x2;h.y2=y2;h.bottom=bottom;h.top=top;h.damagePerSecond=damage;m.hazards.push_back(h);}
inline void shelf(Map& m,Vec2 p,float yaw,float floor=0){m.fixtures.push_back({7,p,floor,1.8f,.5f,1.8f,yaw,true});}
// Exit to the next map: the stitched gap in the south wall at columns 20-22
// and the transfer door that opens once the map's hostiles are cleared.
inline void transferExit(Map& m,float floor=0){
 rectangle(m.layers[0],20,23,23,24,'.');
 rectangle(m.layers[0],19,21,20,23,'#');
 Door exit{20,23,21.5f,0,false,true};exit.requireEnemiesClear=true;m.doors.push_back(exit);
 // The boundary opening is a 2.5 m portal, walled above to the roof.
 wall(m,20,23,23,24,floor+2.5f,m.definition.ceiling,3);
}

// --- Plant kit ----------------------------------------------------------------
// Reactor plant is built from anchored parts: every pipe run ends in a flange
// on equipment, a wall sleeve or a floor collar, and every horizontal span is
// clamped to a hanger rod or a floor stand. Nothing hangs in mid-air.
struct P3 {float x,y,z;};
constexpr float NoRoof=-1000.f;
inline void flange(Map& m,P3 at,Vec2 axis,float radius){
 // Axis {0,0} is a vertical pipe; otherwise a horizontal run's direction.
 if(axis.x==0&&axis.y==0)m.pipes.push_back({{at.x,at.y},{at.x,at.y},at.z-.045f,radius*1.3f,at.z+.045f,6});
 else m.pipes.push_back({Vec2{at.x,at.y}-axis*.045f,Vec2{at.x,at.y}+axis*.045f,at.z,radius*1.3f,at.z,6});
}
inline void pipeRoute(Map& m,std::initializer_list<P3> points,float radius,int paint,float roof,float floor=NoRoof){
 const P3* previous=nullptr;Vec2 lastAxis{};
 for(const auto& p:points){
  if(previous){
   const P3& a=*previous;Vec2 d{p.x-a.x,p.y-a.y};float run=std::sqrt(d.x*d.x+d.y*d.y);
   m.pipes.push_back({{a.x,a.y},{p.x,p.y},a.z,radius,p.z,paint});
   Vec2 axis=run>.01f?d*(1.f/run):Vec2{};
   if(previous==points.begin())flange(m,a,axis,radius);
   else {
    // A bend: a forged elbow body where the two runs meet.
    m.pipes.push_back({{a.x,a.y},{a.x,a.y},a.z-radius*1.1f,radius*1.12f,a.z+radius*1.1f,paint});
   }
   if(run>1.f&&std::fabs(p.z-a.z)<.01f)for(float t=1.1f;t<run-.5f;t+=2.4f){
    Vec2 c=Vec2{a.x,a.y}+axis*t;
    m.pipes.push_back({c-axis*.05f,c+axis*.05f,a.z,radius*1.2f,a.z,6});
    if(floor>NoRoof&&a.z-radius-floor<1.6f){
     // Low runs stand on a pipe shoe and post bolted to the floor.
     m.pipes.push_back({c,c,floor,.045f,a.z-radius,6});
     wall(m,c.x-.16f,c.y-.16f,c.x+.16f,c.y+.16f,floor,floor+.025f,8);
    }else if(roof>NoRoof){
     float top=roof-.05f;int tx=int(std::floor(c.x)),ty=int(std::floor(c.y));
     if(tx>=0&&ty>=0&&tx<24&&ty<24)for(const auto& l:m.layers)if(l.thickness>0&&l.rows[size_t(ty)][size_t(tx)]=='='&&l.elevation-l.thickness>a.z)top=std::min(top,l.elevation-l.thickness-.04f);
     m.pipes.push_back({c,c,a.z+radius*1.2f,.022f,top,6});
    }
   }
   lastAxis=axis;
  }
  previous=&p;
 }
 if(previous)flange(m,*previous,lastAxis,radius);
}
// A handwheel gate valve on a horizontal run along `axis`: the body, the
// bonnet and stem out to one side, and the purchased wheel facing the stem.
inline void gateValve(Map& m,P3 at,Vec2 axis,float radius,float floor,int side=1){
 m.pipes.push_back({Vec2{at.x,at.y}-axis*.22f,Vec2{at.x,at.y}+axis*.22f,at.z,radius*1.45f,at.z,6});
 Vec2 out{-axis.y*side,axis.x*side};
 m.pipes.push_back({Vec2{at.x,at.y}+out*(radius*1.2f),Vec2{at.x,at.y}+out*(radius+.34f),at.z,.035f,at.z,6});
 Vec2 w=Vec2{at.x,at.y}+out*(radius+.38f);
 m.fixtures.push_back({45,w,at.z-.2f-floor,.4f,.12f,.4f,std::atan2(out.x,out.y),false});
}
// A rolled-section steel column: flanges, web, base plate and cap.
inline void steelColumn(Map& m,float x,float y,float bottom,float top,bool alongX=true){
 float a=alongX?.14f:.11f,b=alongX?.11f:.14f;
 wall(m,x-a,y-b,x+a,y-b+.025f,bottom,top,12);wall(m,x-a,y+b-.025f,x+a,y+b,bottom,top,12);
 wall(m,x-.012f,y-b,x+.012f,y+b,bottom,top,12);
 if(!alongX){wall(m,x-a,y-b,x-a+.025f,y+b,bottom,top,12);wall(m,x+a-.025f,y-b,x+a,y+b,bottom,top,12);}
 wall(m,x-.22f,y-.22f,x+.22f,y+.22f,bottom,bottom+.03f,8);
 wall(m,x-.2f,y-.2f,x+.2f,y+.2f,top-.03f,top,8);
 // The thin plates leave channels a body could slip into between hull
 // samples and be boxed in; an invisible solid core fills the profile.
 wall(m,x-a,y-b,x+a,y+b,bottom,top,CollisionProxyMaterial);
}
// Floor stands under a deck's open edge, every few metres: decks are carried.
inline void deckSupports(Map& m,const AuthoredLayerData& deck,float floor,float spacing=6.f){
 const float under=deck.elevation-deck.thickness;
 auto on=[&](int x,int y){return x>=0&&y>=0&&x<24&&y<24&&deck.rows[y][x]=='=';};
 // No column where equipment or a plinth already stands.
 auto occupied=[&](float x,float y){for(const auto& s:m.structures)if((s.material==6||s.material==15)&&s.bottom<=floor+.1f&&x>s.x1-.3f&&x<s.x2+.3f&&y>s.y1-.3f&&y<s.y2+.3f)return true;return false;};
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(on(x,y)){
  bool west=!on(x-1,y)&&x>1,east=!on(x+1,y)&&x<22,north=!on(x,y-1)&&y>1,south=!on(x,y+1)&&y<22;
  // One column per bay on the open edge, outboard of the walkway.
  if((west||east)&&int(std::round(y+.5f))%int(spacing)==int(spacing)/2){float cx=west?x+.25f:x+.75f;if(!occupied(cx,y+.5f))steelColumn(m,cx,y+.5f,floor,under,false);}
  else if((north||south)&&int(std::round(x+.5f))%int(spacing)==int(spacing)/2){float cy=north?y+.25f:y+.75f;if(!occupied(x+.5f,cy))steelColumn(m,x+.5f,cy,floor,under);}
 }
}
// A deck hung from the roof on rods rather than carried on columns: for a
// gallery above another walkway, which columns would block.
inline void deckHangers(Map& m,const AuthoredLayerData& deck,float roof,float spacing=3.f){
 auto on=[&](int x,int y){return x>=0&&y>=0&&x<24&&y<24&&deck.rows[y][x]=='=';};
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(on(x,y)){
  bool west=!on(x-1,y),east=!on(x+1,y),north=!on(x,y-1),south=!on(x,y+1);
  if((west||east)&&int(std::round(y+.5f))%int(spacing)==1){float cx=west?x+.12f:x+.88f;m.pipes.push_back({{cx,y+.5f},{cx,y+.5f},deck.elevation,.03f,roof-.05f,6});}
  else if((north||south)&&int(std::round(x+.5f))%int(spacing)==1){float cy=north?y+.12f:y+.88f;m.pipes.push_back({{x+.5f,cy},{x+.5f,cy},deck.elevation,.03f,roof-.05f,6});}
 }
}
// A pump on a concrete plinth: the purchased pump body, suction from a floor
// collar and a discharge riser out of the casing, both with flanges. Returns
// the top of the discharge riser.
inline P3 pumpSkid(Map& m,Vec2 c,float floor,float yaw,float riser){
 float s=std::cos(yaw),n=std::sin(yaw);
 const bool alongX=std::fabs(s)>.5f;
 Vec2 half=alongX?Vec2{1.1f,.66f}:Vec2{.66f,1.1f},pad=alongX?Vec2{1.25f,.75f}:Vec2{.75f,1.25f};
 wall(m,c.x-pad.x,c.y-pad.y,c.x+pad.x,c.y+pad.y,floor,floor+.15f,15);
 if(alongX)floorLine(m,c.x-pad.x,c.y-pad.y,c.x+pad.x,c.y-pad.y+.12f,floor+.15f,13);
 else floorLine(m,c.x-pad.x,c.y-pad.y,c.x-pad.x+.12f,c.y+pad.y,floor+.15f,13);
 m.props.push_back({0,c,1.25f,2.2f,yaw,half,.15f});
 Vec2 forward{s,-n};
 Vec2 suction=c+forward*1.45f;
 m.pipes.push_back({c+forward*.95f,suction,floor+.62f,.13f,floor+.62f,3});
 m.pipes.push_back({suction,suction,floor,.13f,floor+.62f,3});
 flange(m,{suction.x,suction.y,floor+.02f},{},.13f);
 flange(m,{c.x+forward.x*.95f,c.y+forward.y*.95f,floor+.62f},forward,.13f);
 Vec2 out=c-forward*.25f;
 m.pipes.push_back({out,out,floor+1.15f,.11f,riser,3});
 flange(m,{out.x,out.y,floor+1.2f},{},.11f);
 m.structures.push_back({std::min(c.x-half.x,suction.x-.2f),std::min(c.y-half.y,suction.y-.2f),std::max(c.x+half.x,suction.x+.2f),std::max(c.y+half.y,suction.y+.2f),floor,floor+1.4f,false,6});
 return {out.x,out.y,riser};
}
// A cable tray along a wall at height z: rungs, side rails and wall brackets.
inline void cableTray(Map& m,Vec2 a,Vec2 b,float z){
 bool alongX=std::fabs(b.x-a.x)>std::fabs(b.y-a.y);
 float x1=std::min(a.x,b.x),x2=std::max(a.x,b.x),y1=std::min(a.y,b.y),y2=std::max(a.y,b.y);
 if(alongX){wall(m,x1,y1-.2f,x2,y1-.18f,z,z+.1f,2);wall(m,x1,y1+.18f,x2,y1+.2f,z,z+.1f,2);
  for(float x=x1+.15f;x<x2;x+=.3f)wall(m,x,y1-.18f,x+.03f,y1+.18f,z,z+.02f,2);
  for(float x=x1+.6f;x<x2;x+=1.8f)wall(m,x,y1-.2f,x+.04f,y1+.2f,z-.08f,z,8);
 }else{wall(m,x1-.2f,y1,x1-.18f,y2,z,z+.1f,2);wall(m,x1+.18f,y1,x1+.2f,y2,z,z+.1f,2);
  for(float y=y1+.15f;y<y2;y+=.3f)wall(m,x1-.18f,y,x1+.18f,y+.03f,z,z+.02f,2);
  for(float y=y1+.6f;y<y2;y+=1.8f)wall(m,x1-.2f,y,x1+.2f,y+.04f,z-.08f,z,8);
 }
}
// Wall pilasters and a concrete plinth band give tall plain walls structure.
inline void wallBays(Map& m,float floor,float top,float spacing,bool north,bool south,bool west,bool east,float skip1=-1,float skip2=-1){
 auto skipped=[&](float t){return (skip1>=0&&std::fabs(t-skip1)<2.1f)||(skip2>=0&&std::fabs(t-skip2)<2.1f);};
 for(float t=spacing;t<24-spacing*.5f;t+=spacing){
  if(north&&!skipped(t))wall(m,t-.25f,1,t+.25f,1.22f,floor,top,15);
  if(south&&!skipped(t))wall(m,t-.25f,22.78f,t+.25f,23,floor,top,15);
  if(west&&!skipped(t))wall(m,1,t-.25f,1.22f,t+.25f,floor,top,15);
  if(east&&!skipped(t))wall(m,22.78f,t-.25f,23,t+.25f,floor,top,15);
 }
}

// A vertical pressure vessel built from coaxial cylinders, with collision from
// boxes that cover its round footprint.
inline void vessel(Map& m,Vec2 c,float radius,float bottom,float top,int paint=1){
 m.pipes.push_back({c,c,bottom,radius,top,paint});
 m.pipes.push_back({c,c,bottom,radius+.1f,bottom+.35f,6});
 // Forged courses: a heavy band every 2.2 m up the shell.
 for(float z=bottom+1.4f;z<top-.4f;z+=2.2f)m.pipes.push_back({c,c,z,radius+.07f,z+.2f,6});
 // Four boxes whose union encloses the whole circle: no gap at any angle.
 for(auto [hx,hy]:{std::pair{1.02f,.44f},{.94f,.74f},{.74f,.94f},{.44f,1.02f}})
  m.structures.push_back({c.x-radius*hx,c.y-radius*hy,c.x+radius*hx,c.y+radius*hy,bottom,top,false,6});
}

// Every freight chunk ends the same way: steel flights get open treads and
// stringers, decks are cut back over each flight, and boundary doors name
// the chunk on their far side.
inline void freightFinish(Map& m,int level){
 for(auto& stair:m.stairs)if(stair.steps>1){
 stair.openUnderside=true;stair.treadThickness=.12f;stair.sideRails=true;
 float z1=stair.ascending?stair.bottom:stair.top,z2=stair.ascending?stair.top:stair.bottom;
 for(bool right:{false,true}){
 Vec2 a=stair.alongY?Vec2{right?stair.x2-.12f:stair.x1+.12f,stair.y1}:Vec2{stair.x1,right?stair.y2-.12f:stair.y1+.12f};
 Vec2 b=stair.alongY?Vec2{a.x,stair.y2}:Vec2{stair.x2,a.y};
 m.pipes.push_back({a,b,z1-.08f,.085f,z2-.08f,2});
 }
 }
 for(const auto& stair:m.stairs)for(auto& layer:m.layers)if(layer.thickness>0&&layer.elevation>=stair.bottom-.02f&&layer.elevation<=stair.top+.02f)
 rectangle(layer,int(stair.x1),int(stair.y1),int(stair.x2),int(stair.y2),'.');
 // A transfer or entry door hangs in a whole-metre wall opening; jambs fill
 // the opening either side of the leaf so the next map never shows through.
 for(const auto& d:m.doors){if(!d.transfer&&!d.entry)continue;
  float y1=std::floor(d.y),base=m.layers.front().elevation+d.z,top=m.definition.ceiling;
  float l=std::floor(d.left),r=std::ceil(d.right);
  if(d.left-l>.01f){wall(m,l,y1,d.left,y1+1.f,base,top,15);wall(m,d.left-.12f,y1-.02f,d.left,y1+1.02f,base,base+3.f,12);}
  if(r-d.right>.01f){wall(m,d.right,y1,r,y1+1.f,base,top,15);wall(m,d.right,y1-.02f,d.right+.12f,y1+1.02f,base,base+3.f,12);}
 }
}
// A 1.1 m guard rail along an open edge: the walk-off side of a deck.
inline void guardRail(Map& m,float x1,float y1,float x2,float y2,float z){m.structures.push_back({x1,y1,x2,y2,z,z+1.1f,true,2});}

// Aisle-end label on a rack, on its own backing plate.
inline void rackSign(Map& m,float x,float y,float z,const char* title,const char* subtitle){
 wall(m,x-.75f,y-.12f,x+.75f,y-.02f,z-.05f,z+.7f,2);
 wallSign(m,x,y-.175f,z,title,subtitle,1.4f,kPi);
}



// --- Office construction kit -------------------------------------------------------------
// A full-height office wall run (plaster, skirting) from (x1,y1) to (x2,y2).
inline void officeWall(Map& m,float x1,float y1,float x2,float y2,float f,float ceil){
 wall(m,x1,y1,x2,y2,f,ceil,21);
 wall(m,x1-.01f,y1-.01f,x2+.01f,y2+.01f,f,f+.1f,20);
}
// A glazed partition: plaster dado, glass with aluminium mullions every
// 1.2 m and transom, plaster bulkhead to the ceiling.
inline void officeGlass(Map& m,float x1,float y1,float x2,float y2,float f,float ceil){
 wall(m,x1,y1,x2,y2,f,f+.9f,21);wall(m,x1-.01f,y1-.01f,x2+.01f,y2+.01f,f+.88f,f+.94f,20);
 bool alongX=x2-x1>y2-y1;float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)wall(m,x1,cy-.01f,x2,cy+.01f,f+.94f,f+2.3f,17);else wall(m,cx-.01f,y1,cx+.01f,y2,f+.94f,f+2.3f,17);
 float len=alongX?x2-x1:y2-y1;int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,std::min(t,x2-.06f),y1,std::min(t+.06f,x2),y2,f+.94f,f+2.3f,20);else wall(m,x1,std::min(t,y2-.06f),x2,std::min(t+.06f,y2),f+.94f,f+2.3f,20);}
 wall(m,x1,y1,x2,y2,f+2.3f,f+2.38f,20);wall(m,x1,y1,x2,y2,f+2.38f,ceil,21);
}
// A doorway frame in a wall run along x at y: architrave posts and head.
inline void officeDoorway(Map& m,float x1,float x2,float y1,float y2,float f,float ceil){
 for(float x:{x1-.08f,x2})wall(m,x,y1-.02f,x+.08f,y2+.02f,f,f+2.2f,20);
 wall(m,x1-.08f,y1-.02f,x2+.08f,y2+.02f,f+2.12f,f+2.2f,20);wall(m,x1,y1,x2,y2,f+2.2f,ceil,21);
}
// A workstation facing `facing` (+1 south / -1 north) at desk centre (x,y):
// desk, chair pulled in, monitor and keyboard turned to the seat, phone,
// and on some desks a mug and binders.
inline void workstation(Map& m,float x,float y,float f,int facing,int variant){
 float base=f-m.layers.front().elevation,seat=facing>0?0.f:kPi,screen=facing>0?kPi:0.f,s=float(facing);
 m.fixtures.push_back({24,{x,y},base,1.4f,.7f,.75f,seat,true});
 // The office chair model faces -y at yaw 0, the opposite of the desk set.
 m.fixtures.push_back({26,{x+(variant%2?.15f:-.1f),y-s*.78f},base,.55f,.55f,.95f,seat+kPi+(variant%3-1)*.25f,true});
 m.fixtures.push_back({27,{x-.1f,y+s*.12f},base+.75f,.42f,.4f,.38f,screen,false});
 m.fixtures.push_back({28,{x-.1f,y-s*.2f},base+.75f,.45f,.16f,.03f,screen,false});
 m.fixtures.push_back({33,{x+.48f,y+s*.05f},base+.75f,.2f,.2f,.1f,screen,false});
 if(variant%2)m.fixtures.push_back({34,{x+.42f,y-s*.2f},base+.75f,.09f,.09f,.1f,0,false});
 if(variant%3==0)m.fixtures.push_back({38,{x-.52f,y+s*.18f},base+.75f,.3f,.08f,.32f,screen,false});
 if(variant%4==1)m.fixtures.push_back({29,{x+.52f,y+s*.1f},base,.2f,.45f,.42f,screen,false});
}
// A row of cubicles along x either side of a fabric spine at y: 1.25 m panels
// with pale steel caps, side panels between seats.
inline void cubicleBank(Map& m,float x1,int seats,float y,float f,int seed){
 const float w=1.6f,x2=x1+seats*w;
 wall(m,x1,y-.03f,x2,y+.03f,f,f+1.25f,23);wall(m,x1,y-.04f,x2,y+.04f,f+1.25f,f+1.29f,20);
 for(int i=0;i<=seats;++i){float x=x1+i*w;
  wall(m,x-.03f,y-1.5f,x+.03f,y+1.5f,f,f+1.1f,23);wall(m,x-.04f,y-1.5f,x+.04f,y+1.5f,f+1.1f,f+1.14f,20);}
 for(int i=0;i<seats;++i){float x=x1+(i+.5f)*w;workstation(m,x,y-.42f,f,1,seed+i);workstation(m,x,y+.42f,f,-1,seed+i+3);}
}
// A suspended ceiling: grid tees every 1.2 m and recessed troffers.
inline void ceilingGrid(Map& m,float x1,float y1,float x2,float y2,float ceil){
 for(float x=x1+1.2f;x<x2;x+=1.2f)wall(m,x-.02f,y1,x+.02f,y2,ceil-.03f,ceil,20);
 for(float y=y1+1.2f;y<y2;y+=1.2f)wall(m,x1,y-.02f,x2,y+.02f,ceil-.03f,ceil,20);
}

// A suspended belt conveyor along x at y, belt top z: side frames, the belt,
// return rollers, hanger rods to the roof and feed hoods at both ends.
inline void beltConveyor(Map& m,float x1,float x2,float y,float z,float roof){
 for(float s:{-.4f,.32f})wall(m,x1,y+s,x2,y+s+.08f,z-.3f,z+.06f,12);
 wall(m,x1,y-.32f,x2,y+.32f,z-.1f,z,8);
 for(float x=x1+.4f;x<x2;x+=1.2f)wall(m,x,y-.32f,x+.06f,y+.32f,z-.26f,z-.2f,2);
 for(float x=x1+1.f;x<x2-.5f;x+=3.f)for(float s:{-.36f,.36f})m.pipes.push_back({{x,y+s},{x,y+s},z+.06f,.025f,roof,6});
 for(float x:{x1,x2-1.2f}){wall(m,x,y-.5f,x+1.2f,y+.5f,z+.05f,z+.7f,12);wall(m,x-.02f,y-.52f,x+1.22f,y+.52f,z+.7f,z+.78f,8);}
}
// A ground belt conveyor along y at x on legs, belt top z.
inline void floorConveyor(Map& m,float x,float y1,float y2,float floor,float z){
 for(float s:{-.4f,.32f})wall(m,x+s,y1,x+s+.08f,y2,z-.3f,z+.06f,12);
 wall(m,x-.32f,y1,x+.32f,y2,z-.1f,z,8);
 for(float y=y1+.6f;y<y2-.3f;y+=1.8f)for(float s:{-.4f,.32f})wall(m,x+s,y,x+s+.08f,y+.08f,floor,z-.3f,12);
 m.structures.push_back({x-.4f,y1,x+.4f,y2,floor,z,false,8});
}
// A flat wagon on a running line at x from y, its bogies on the rails, with a
// shipping container chained to the deck.
inline void flatWagon(Map& m,float x,float y,float floor,bool loaded=true){
 wall(m,x-1.25f,y,x+1.25f,y+12.4f,floor+.98f,floor+1.12f,8);
 for(float s:{-1.3f,1.2f})wall(m,x+s,y,x+s+.1f,y+12.4f,floor+.82f,floor+1.12f,12);
 for(float yy:{y+2.2f,y+10.2f}){
  wall(m,x-.65f,yy-.9f,x+.65f,yy+.9f,floor+.55f,floor+.85f,8);
  for(float axle:{yy-.55f,yy+.55f}){
   m.pipes.push_back({{x-.85f,axle},{x+.85f,axle},floor+.44f,.075f,floor+.44f,2});
   for(float dx:{-.78f,.78f})m.pipes.push_back({{x+dx-.07f,axle},{x+dx+.07f,axle},floor+.44f,.42f,floor+.44f,6});
  }
 }
 for(float yy:{y-.35f,y+12.4f})wall(m,x-.15f,yy,x+.15f,yy+.35f,floor+.75f,floor+.95f,8);
 m.structures.push_back({x-1.3f,y,x+1.3f,y+12.4f,floor,floor+.98f,false,6});
 if(loaded)m.fixtures.push_back({48,{x,y+6.2f},floor+1.12f-m.layers.front().elevation,2.44f,10.8f,2.5f,0,true});
}

// A closed roller shutter over a running line where it passes through the
// south (or north) wall: steel frame, slats and a hazard-striped sill.
inline void trackPortal(Map& m,float x,float y,float floor,float face){
 wall(m,x-2.f,y,x+2.f,y+.2f,floor,floor+4.4f,7);
 for(float z=floor+.3f;z<floor+4.3f;z+=.3f)wall(m,x-1.9f,y+face*.06f,x+1.9f,y+.2f-(1-face)*.06f,z,z+.05f,2);
 for(float px:{x-2.3f,x+2.f})wall(m,px,y-.15f,px+.3f,y+.35f,floor,floor+4.8f,12);
 wall(m,x-2.3f,y-.15f,x+2.3f,y+.35f,floor+4.4f,floor+4.8f,12);
}

// Tunnel lining between x1 and x2: steel ribs every `step` metres along y,
// each a pair of posts, a crown beam and stepped haunches into the corners.
inline void tunnelRibs(Map& m,float x1,float x2,float floor,float roof,float y1,float y2,float step){
 for(float y=y1;y<y2;y+=step){
  for(float x:{x1,x2-.22f})wall(m,x,y-.1f,x+.22f,y+.1f,floor,roof-.6f,12);
  wall(m,x1,y-.1f,x2,y+.1f,roof-.45f,roof,12);
  for(int i=0;i<3;++i){float d=.22f+i*.3f,z=roof-.6f+i*.05f-i*.0f;
   wall(m,x1,y-.1f,x1+d+.3f,y+.1f,roof-.6f+i*.05f,roof-.45f,12);wall(m,x2-d-.3f,y-.1f,x2,y+.1f,roof-.6f+i*.05f,roof-.45f,12);(void)z;}
 }
}

// --- Descent chapters (chunks 34 on) -------------------------------------------
// One passage across a chunk side, as planned in tools/campaign/plan.py: side
// N/S/E/W, the local range a..b along that side, the floor height z where it
// is crossed, and whether it is a door (N entry / S transfer, optionally
// locked by a state) or an open seam.
struct EdgeSpec {char side;float a,b,z;bool door;StateId state;};
// Opens a planned edge in layer 0: carves the border tiles, sets the seam flag
// or adds the boundary door. The passage floor is the caller's business.
inline void openEdge(Map& m,const EdgeSpec& e){
 auto& layer=m.layers.front();const float base=layer.elevation;
 int a=int(std::floor(e.a)),b=int(std::ceil(e.b));
 if(e.side=='N'||e.side=='S'){int row=e.side=='N'?0:23;rectangle(layer,a,row,b,row+1,'.');
  if(e.door){Door d{e.a,e.b,e.side=='N'?.5f:23.5f,0,false,e.side=='S',e.side=='N',e.z-base};d.requireState=e.state;m.doors.push_back(d);}
  else if(e.side=='N')m.openNorth=true;else m.openSouth=true;}
 else{int col=e.side=='W'?0:23;rectangle(layer,col,a,col+1,b,'.');if(e.side=='W')m.openWest=true;else m.openEast=true;}
}
// A connected placeholder for a chunk not authored yet: a room at the lowest
// edge height with every planned edge open, a stair up to any raised edge and
// plain light, so the whole route can be walked before its maps exist.
inline std::shared_ptr<const Map> placeholderChunk(int index,std::initializer_list<EdgeSpec> edges){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[size_t(index)];m.definition=CampaignChunks[size_t(index)];
 float base=1000;for(const auto& e:edges)base=std::min(base,e.z);
 const float roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 for(const auto& e:edges){
  openEdge(m,e);const float rise=e.z-base;if(rise<.01f)continue;
  const int steps=int(std::ceil(rise/.19f));const float run=steps*.28f;
  if(e.side=='N'){wall(m,e.a,0,e.b,3,base,e.z,15);m.stairs.push_back({e.a,3,e.b,3+run,base,e.z,steps,true,false});}
  if(e.side=='S'){wall(m,e.a,21,e.b,24,base,e.z,15);m.stairs.push_back({e.a,21-run,e.b,21,base,e.z,steps,true,true});}
  if(e.side=='W'){wall(m,0,e.a,3,e.b,base,e.z,15);m.stairs.push_back({3,e.a,3+run,e.b,base,e.z,steps,false,false});}
  if(e.side=='E'){wall(m,21,e.a,24,e.b,base,e.z,15);m.stairs.push_back({21-run,e.a,21,e.b,base,e.z,steps,false,true});}
 }
 for(float x:{6.f,18.f})for(float y:{6.f,18.f})m.lights.push_back(lamp({x,y},roof-.4f,1.2f,10.f,4200));
 zone(m,(m.name+" / UNDER CONSTRUCTION").c_str(),0,0,24,24);
 freightFinish(m,-1);
 return p;
}
}
}

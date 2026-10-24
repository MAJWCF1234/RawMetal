#include "CampaignMaps.h"
#include <algorithm>
#include <cmath>
namespace retro {
namespace {
using Map=AuthoredMapData;
void rectangle(AuthoredLayerData& layer,int x1,int y1,int x2,int y2,char tile){
 for(int y=y1;y<y2;++y)for(int x=x1;x<x2;++x)layer.rows[y][x]=tile;
}
AuthoredLayerData floorLayer(const char* name,float z,bool openNorth=false,bool openSouth=false){
 AuthoredLayerData layer;layer.name=name;layer.elevation=z;
 for(int y=0;y<24;++y)layer.rows[y]=std::string(24,'.');
 rectangle(layer,0,0,1,24,'#');rectangle(layer,23,0,24,24,'#');
 if(!openNorth)rectangle(layer,0,0,24,1,'#');
 if(!openSouth)rectangle(layer,0,23,24,24,'#');
 return layer;
}
void wall(Map& m,float x1,float y1,float x2,float y2,float lo,float hi,int material=2){m.structures.push_back({x1,y1,x2,y2,lo,hi,false,material});}
void cargo(Map& m,float x,float y,float floor,bool loaded=true,float yaw=0){
 m.fixtures.push_back({15,{x,y},floor-m.layers.front().elevation,1.2f,1.15f,.16f,yaw,true});
 if(loaded)m.fixtures.push_back({17,{x,y},floor+.16f-m.layers.front().elevation,1.1f,1.f,.99f,yaw,true});
}
void board(Map& m,float x,float y,float z,const char* title,const char* subtitle,float width=3.f){
 // Every sign has a steel backing and two supports rooted in the floor.
 wall(m,x-width*.5f,y,x+width*.5f,y+.12f,z,z+.9f,2);
 for(float dx:{-width*.44f,width*.44f})wall(m,x+dx-.04f,y+.03f,x+dx+.04f,y+.11f,m.layers.front().elevation,z,2);
 m.signs.push_back({{x,y-.012f},z,width,.9f,kPi,title,subtitle});
}
void terminal(Map& m,Vec2 p,const char* title,const char* a,const char* b,float z=0,StateId state=0,StateId prerequisite=0){
 AuthoredTerminalData t;t.position=p;t.title=title;t.line1=a;t.line2=b;t.z=z;t.activateState=state;t.requireState=prerequisite;m.terminals.push_back(std::move(t));
}
void deck(Map& m,float z,int x1,int y1,int x2,int y2){
 for(auto& l:m.layers)if(l.thickness>0&&std::fabs(l.elevation-z)<.01f){rectangle(l,x1,y1,x2,y2,'=');return;}
 AuthoredLayerData l;l.name="Freight steel walkway";l.elevation=z;l.thickness=.22f;for(auto& row:l.rows)row=std::string(24,'.');rectangle(l,x1,y1,x2,y2,'=');m.layers.push_back(std::move(l));
}
void rack(Map& m,float x,float y,float floor,int tiers=4){
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
  m.pipes.push_back({{x+.12f,y+3.9f},{x+2.6f,y+3.9f},z+.35f,.035f,z+2.8f,1});
  m.pipes.push_back({{x+.12f,y+3.9f},{x+2.6f,y+3.9f},z+2.8f,.035f,z+.35f,1});
 }
}
void container(Map& m,float x,float y,float floor,bool open=false,int material=11){
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
void floorLine(Map& m,float x1,float y1,float x2,float y2,float floor,int material=14){wall(m,x1,y1,x2,y2,floor+.007f,floor+.015f,material);}
void wallLamp(Map& m,float x,float y,float z,float yaw,float intensity=1.f,float range=7.f){
 m.lights.push_back({{x,y},z,WorldLightMount::Wall,yaw,intensity,range});
}
void servicePipe(Map& m,Vec2 a,Vec2 b,float z,float radius,int paint){
 m.pipes.push_back({a,b,z,radius,z,paint});
 Vec2 d=b-a;float length=std::sqrt(d.x*d.x+d.y*d.y);if(length<.01f)return;
 Vec2 unit=d*(1.f/length);
 for(float t=.4f;t<length;t+=2.5f){Vec2 c=a+unit*t;
  m.pipes.push_back({c-unit*.045f,c+unit*.045f,z,radius*1.32f,z,2});
 }
}
void wallSign(Map& m,float x,float y,float z,const char* title,const char* subtitle,float width=2.f,float yaw=kPi){
 // Signs fasten to their architectural face, not free-standing floor poles.
 float dx=std::cos(yaw),dy=std::sin(yaw);
 float hx=std::abs(dx)*width*.5f+std::abs(dy)*.055f;
 float hy=std::abs(dy)*width*.5f+std::abs(dx)*.055f;
 wall(m,x-hx,y-hy,x+hx,y+hy,z-.02f,z+.67f,2);
 m.signs.push_back({{x+dy*.06f,y+dx*.06f},z,width,.65f,yaw,title,subtitle});
}
void equipmentPad(Map& m,float x1,float y1,float x2,float y2,float floor){
 wall(m,x1,y1,x2,y2,floor,floor+.10f,15);
 floorLine(m,x1,y1,x2,y1+.12f,floor+.10f,13);
 floorLine(m,x1,y2-.12f,x2,y2,floor+.10f,13);
}
void serviceCeiling(Map& m,float x1,float y1,float x2,float y2,float roof){
 // A layered service soffit gives human spaces a ceiling with actual depth.
 wall(m,x1,y1,x2,y2,roof,roof+.16f,2);
 for(float y=y1+.35f;y<y2;y+=3.f){
  wall(m,x1,y,x2,y+.16f,roof-.28f,roof,18);
  for(float x:{x1,x2-.18f})wall(m,x,y-.035f,x+.18f,y+.20f,roof-.46f,roof,13);
 }
 for(float x:{x1+.35f,x2-.45f}){
  servicePipe(m,{x,y1},{x,y2},roof-.37f,.085f,x<x1+1?3:4);
  for(float y=y1+.5f;y<y2;y+=3.f)wall(m,x-.16f,y,x+.16f,y+.065f,roof-.50f,roof,2);
 }
}
void warehouseServices(Map& m,float floor,float roof){
 for(float y:{3.f,11.f,19.f})for(bool east:{false,true}){
  if(east?m.openEast:m.openWest)continue;
  float x=east?22.05f:1.f;
  wall(m,x,y,x+.95f,y+.75f,floor,roof,15);
  wall(m,x-.04f,y-.04f,x+.99f,y+.79f,floor,floor+.28f,2);
  for(float z:{floor+2.3f,floor+8.3f,floor+14.3f}){
   float face=east?x-.01f:x+.96f;
   wallLamp(m,face,y+.375f,z,east?kPi:0,1.1f,7.f);
  }
 }
 for(bool east:{false,true}){
  if(east?m.openEast:m.openWest)continue;
  float x=east?22.35f:1.65f;
  for(int i=0;i<2;++i)servicePipe(m,{x,1.2f},{x,22.7f},floor+4.3f+i*.34f,.10f,i?4:3);
  for(float z:{floor+6.f,floor+12.f,floor+18.f})wall(m,east?22.65f:1.f,1,east?23.f:1.35f,23,z,z+.25f,20);
 }
}
void actor(Map& m,const char* timer,ActorVisual visual,float scale,std::initializer_list<ActorKey> keys,bool loop=false,bool tool=false){m.actorTracks.push_back({stateId(timer),visual,scale,loop,tool,keys});}
void cue(Map& m,const char* timer,int time,Sound sound,Vec2 position,const char* caption="",float gain=1,float z=std::numeric_limits<float>::quiet_NaN()){m.sequenceCues.push_back({stateId(timer),time,sound,position,gain,1,caption,z});}
void machineryClock(Map& m,const char* timer,int period){TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_cycled");s.durationMs=period;s.finishAtMs=period;s.soundIntervalMs=0;s.loop=true;m.timedSequences.push_back(s);}
// Early contact is a doomed containment detail, not the main Cleanup battle.
// Entities take the remaining squad while one living member evacuates.
void receivingCleanup(Map& m){
 machineryClock(m,"receiving_cleanup_clock",60000);
 const Vec2 starts[]={{7.8f,16},{14.6f,18.2f},{21,10.5f},{12,20},{15,20.5f},{7.5f,12}};
 const Vec2 ends[]={{7.8f,14.5f},{15.5f,18.2f},{21,12},{12,21.5f},{15.8f,21.5f},{7.5f,13.5f}};
 for(int i=0;i<6;++i){
  ActorTrack t;t.timerState=stateId("receiving_cleanup_clock");t.visual=ActorVisual(4+i);t.scale=1.6f;t.health=90;
  t.damageState=stateId("receiving_cleanup_damage_"+std::to_string(i));t.deadState=stateId("receiving_cleanup_dead_"+std::to_string(i));
  t.keys={{0,starts[i],-25,kPi},{60000,starts[i],-25,kPi,0,1}};
  auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=GuardSquadRole(i+1);ai.faction=AiFaction::Raider;ai.squad=stateId("receiving_cleanup");ai.armed=true;
  ai.weapon=(i==1||i==4)?2:1;ai.sightRange=14;ai.sightDot=.45f;ai.searchMs=6000;
  ai.speed=i==3?1.25f:i==5?2.1f:1.55f;ai.suppress=i==3;
  ai.patrol={{starts[i],-25,1500},{ends[i],-25,1200}};m.actorTracks.push_back(std::move(t));
 }
 // No extra progression gate: the personnel aisle remains an escape route.
 terminal(m,{7,21.2f},"INSPECTION / FIELD CACHE","E: TAKE FLASHLIGHT. F: TOGGLE OWNED LIGHT.","CONTAINMENT DETAIL AHEAD. WATCH THE SEALED LOADS.",0,stateId("receiving_cache_read"));
 terminal(m,{7,19.5f},"INSPECTION / CHAIN TEST","E: RATTLE THE CHAIN NORTH OF THE AISLE.","WAIT FOR HIS BACK. CROUCH CLOSE. Q KO / E GRAB.",0,stateId("receiving_inspection_lure"));
 TimedSequence lure;lure.timerState=stateId("receiving_inspection_lure");lure.finishState=stateId("receiving_inspection_lure_done");lure.x1=-2;lure.y1=-2;lure.x2=-1;lure.y2=-1;lure.durationMs=10000;lure.finishAtMs=5000;lure.soundIntervalMs=0;m.timedSequences.push_back(lure);
 cue(m,"receiving_inspection_lure",1600,Sound::JunkMetal,{8.5f,14.5f},"CHAIN TEST / INSPECTION AISLE. WATCH THE PATROL TURN.",.6f,-24.3f);
 cue(m,"receiving_inspection_lure",3200,Sound::JunkMetal,{8.5f,14.5f},"CROUCH / WAIT FOR HIS BACK / Q WHEN CLOSE.",.5f,-24.3f);
 ScriptEvent cache;cache.id=stateId("receiving_cache_award");cache.level=14;cache.x1=0;cache.y1=0;cache.x2=24;cache.y2=24;cache.requireState=stateId("receiving_cache_read");
 ScriptAction item;item.type=ScriptAction::Type::GiveItem;item.id=stateId("flashlight");item.value=1;cache.actions.push_back(item);m.scriptEvents.push_back(cache);
 wall(m,6.1f,17.3f,9,17.5f,-25,-23.15f,15);wall(m,6.1f,17.3f,9,17.5f,-23.25f,-23.15f,13);
 m.signs.push_back({{7.5f,17.52f},-23.8f,2.7f,.5f,0,"CLEANUP / HOLD","WEST / Q KO / E GRAB"});
 wallLamp(m,7.5f,17.56f,-23.4f,kPi*.5f,1.3f,5);
 // A real, player-operable pressure door gives the fleeing member a head start.
 wall(m,1,23.4f,10.5f,23.6f,-25,-22,15);wall(m,13.5f,23.4f,23,23.6f,-25,-22,15);
 Door retreat{10.5f,13.5f,23.5f,0};retreat.open=1;retreat.opening=true;m.doors.push_back(retreat);

}
void handCart(Map& m,float x,float y,float floor){
 wall(m,x-.42f,y-.6f,x+.42f,y+.6f,floor+.2f,floor+.29f,12);
 for(float xx:{x-.47f,x+.37f})for(float yy:{y-.4f,y+.4f})m.pipes.push_back({{xx,yy},{xx+.1f,yy},floor+.14f,.14f,floor+.14f,1});
 for(float xx:{x-.38f,x+.34f})wall(m,xx,y+.56f,xx+.04f,y+.6f,floor+.27f,floor+1.02f,12);
 wall(m,x-.38f,y+.56f,x+.38f,y+.6f,floor+.97f,floor+1.02f,12);
}
void forklift(Map& m,float x,float y,float floor){
 // The supplied complete forklift retains its source proportions and atlases.
 // Forward is -Y at yaw 0; the parked vehicle leaves the centre aisle clear.
 m.fixtures.push_back({23,{x,y},floor-m.layers.front().elevation,1.1641f,3.6204f,2.2f,0,true});
}
void intakeBay(Map& m){
 for(float x:{6.f,17.6f})wall(m,x,22.8f,x+.4f,23.2f,-25,-18.5f,12);
 wall(m,6,22.8f,18,23.2f,-19,-18.5f,12);
 // Panelled freight barrier flanks the damaged, functioning central shutter.
 for(float x:{6.4f,13.55f}){float right=x<10?10.45f:17.6f;
  wall(m,x,22.86f,right,22.96f,-25,-19,11);
  for(float z=-24.8f;z<-19;z+=.85f)wall(m,x,22.79f,right,22.86f,z,z+.07f,2);
  for(float xx=x+.2f;xx<right;xx+=1.f)for(float z:{-24.3f,-20.f})wall(m,xx,22.765f,xx+.06f,22.79f,z,z+.06f,8);
 }
 for(float x:{9.1f,14.8f})floorLine(m,x,8,x+.10f,22.6f,-25);
 floorLine(m,9.1f,21.8f,14.9f,22.05f,-25,13);
 for(float x:{8.5f,15.5f})wall(m,x,21.3f,x+.18f,21.5f,-25,-24.05f,12);
 for(float x:{7.f,16.8f})wall(m,x,16.8f,x+.22f,17.2f,-25,-20.6f,12);
 wall(m,7,16.8f,17.02f,17.2f,-20.85f,-20.6f,12);
 // A hoist has a drum on the gantry, paired cheeks, chain, lower block and
 // curved hook. The suspended load point is not a floating black slab.
 for(float x:{11.52f,12.36f})wall(m,x,16.77f,x+.12f,17.23f,-21.28f,-20.82f,12);
 m.pipes.push_back({{11.64f,17},{12.36f,17},-21.1f,.19f,-21.1f,2});
 for(float y:{16.93f,17.07f})m.pipes.push_back({{12,y},{12,y},-21.27f,.018f,-21.98f,2});
 wall(m,11.86f,16.86f,12.14f,17.14f,-22.20f,-21.98f,18);
 m.pipes.push_back({{12,17},{12,17},-22.2f,.035f,-22.46f,2});
 m.pipes.push_back({{12,17},{12.14f,17},-22.46f,.035f,-22.55f,2});
 m.pipes.push_back({{12.14f,17},{12.25f,17},-22.55f,.035f,-22.45f,2});
 m.fixtures.push_back({8,{18.3f,22.7f},1.f,.75f,.15f,1.f,kPi,false});
 m.lights.clear();m.lights.push_back({{12,20},-20.8f});
 wallLamp(m,6.4f,22.74f,-22.7f,-kPi*.5f,1.6f,8.f);
 wallLamp(m,17.6f,22.74f,-22.7f,-kPi*.5f,1.6f,8.f);
 m.definition.ambient=.38f;
}
void rail(Map& m,float x,float y1,float y2,float z){
 for(float dx:{-.7f,.7f})wall(m,x+dx-.045f,y1,x+dx+.045f,y2,z,z+.07f,2);
 for(float y=y1;y<y2;y+=.8f)wall(m,x-1.f,y,x+1.f,y+.12f,z-.02f,z+.015f,2);
}
void locomotive(Map& m,float x,float y,float floor,bool stripped=false){
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
 if(!stripped)wall(m,x-1.25f,y,x+1.25f,y+7.f,floor+.96f,floor+1.22f,2);
 for(float yy:{y-.4f,y+7.f})wall(m,x-.28f,yy,x+.28f,yy+.4f,floor+.7f,floor+1.f,8);
 if(stripped){
  for(float dx:{-1.25f,1.03f})wall(m,x+dx,y,x+dx+.22f,y+7.f,floor+.96f,floor+1.3f,12);
  for(float yy:{y+.15f,y+1.5f,y+3.5f,y+5.5f,y+6.65f})wall(m,x-1.25f,yy,x+1.25f,yy+.20f,floor+1.03f,floor+1.27f,12);
  m.pipes.push_back({{x,y+.7f},{x,y+6.3f},floor+1.13f,.13f,floor+1.13f,1});
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
void office(Map& m,float x,float y,float floor,const char* title,const char* line1,const char* line2,StateId state=0,bool observation=false){
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
void bench(Map& m,float x,float y,float floor){
 // Open space below a seated-height bench, rather than a solid cargo block.
 wall(m,x-1.35f,y-.25f,x+1.35f,y+.25f,floor+.43f,floor+.51f,10);
 wall(m,x-1.35f,y+.20f,x+1.35f,y+.28f,floor+.55f,floor+1.05f,10);
 for(float dx:{-1.f,1.f})wall(m,x+dx-.06f,y-.20f,x+dx+.06f,y+.20f,floor,floor+.43f,2);
}
void recordsDesk(Map& m,float x,float y,float floor){
 m.fixtures.push_back({19,{x,y},floor-m.layers.front().elevation,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({18,{x,y-.73f},floor-m.layers.front().elevation,.417f,.525f,.95f,kPi,true});
 // Manifest sheets sit in steel in/out trays, with leg space under the desk.
 for(float dx:{-.36f,.36f}){
  wall(m,x+dx-.25f,y-.22f,x+dx+.25f,y+.22f,floor+.78f,floor+.81f,2);
  wall(m,x+dx-.21f,y-.18f,x+dx+.21f,y+.18f,floor+.81f,floor+.825f,15);
  for(float line:{-.10f,0.f,.10f})wall(m,x+dx-.15f,y+line,x+dx+.13f,y+line+.008f,floor+.825f,floor+.826f,8);
 }
}
void observationWindow(Map& m,float x,float y1,float y2,float floor){
 // Framed, open observation apertures keep the warehouse visible; a solid
 // opaque panel would hide the route the player has just climbed.
 wall(m,x,y1,x+.12f,y2,floor,floor+.85f,3);
 wall(m,x,y1,x+.12f,y2,floor+2.65f,floor+3.3f,3);
 for(float y=y1;y<y2;y+=2.f)wall(m,x,y,x+.12f,y+.08f,floor+.85f,floor+2.65f,2);
 wall(m,x+.045f,y1+.08f,x+.065f,y2,floor+.85f,floor+2.65f,17);
}
void climb(Map& m,float bottom,float top){
 float middle=(bottom+top)*.5f;
 if(bottom>m.layers.front().elevation+.1f)deck(m,bottom,8,0,23,4);
 m.stairs.push_back({8,4,11,13,bottom,middle,24,true,true});
 deck(m,middle,8,13,16,16);
 deck(m,middle,11,10,16,13);
 m.stairs.push_back({13,13,16,22,middle,top,24,true,true});
 deck(m,top,8,22,16,24);
}
// ---------------------------------------------------------------------------
// Maps 0-3: the original facility, rebuilt as authored maps.
// ---------------------------------------------------------------------------
WorldLight lamp(Vec2 p,float z,float intensity,float range,float kelvin,float tint=0,WorldLightMount mount=WorldLightMount::Ceiling,float yaw=0){
 WorldLight l{p,z,mount,yaw,intensity,range};l.kelvin=kelvin;l.tint=tint;return l;
}
void zone(Map& m,const char* name,float x1,float y1,float x2,float y2,float bottom=-1000,float top=1000){m.zones.push_back({name,x1,y1,x2,y2,bottom,top});}
void hazard(Map& m,Hazard::Kind kind,float x1,float y1,float x2,float y2,float bottom,float top,float damage){Hazard h;h.kind=kind;h.x1=x1;h.y1=y1;h.x2=x2;h.y2=y2;h.bottom=bottom;h.top=top;h.damagePerSecond=damage;m.hazards.push_back(h);}
void shelf(Map& m,Vec2 p,float yaw,float floor=0){m.fixtures.push_back({7,p,floor,1.8f,.5f,1.8f,yaw,true});}
// Exit to the next map: the stitched gap in the south wall at columns 20-22
// and the transfer door that opens once the map's hostiles are cleared.
void transferExit(Map& m,float floor=0){
 rectangle(m.layers[0],20,23,23,24,'.');
 rectangle(m.layers[0],19,21,20,23,'#');
 Door exit{20,23,21.5f,0,false,true};exit.requireEnemiesClear=true;m.doors.push_back(exit);
 // The boundary opening is a 2.5 m portal, walled above to the roof.
 wall(m,20,23,23,24,floor+2.5f,m.definition.ceiling,3);
}

// FOUNDRY. The service lift that fell from the Security Office lies wrecked in
// the Intake receiving hall. South through the gate is the Foundry: a tall
// brick hall around two converters whose pour runs in an open molten channel,
// crossed on steel bridges or flanked by the west service passage; a sodium-lit
// mezzanine holds the containment console. Containment beyond is a cold
// corridor of breached holding cells ending in decontamination and the
// transfer gate to Pressure Works.
std::shared_ptr<const Map> makeFoundry(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[0];m.definition=CampaignChunks[0];m.definition.ceiling=6.4f;m.definition.ambient=.34f;
 m.layers.push_back(floorLayer("Foundry / ground",0));
 // Always index the ground layer: deck() appends layers and would invalidate a reference.
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 const float hall=6.4f,low=3.4f;

 // --- 01 INTAKE ----------------------------------------------------------
 // Lift alcove (the wrecked cab from the Security Office drop).
 rectangle(g(),5,1,6,3,'#');rectangle(g(),5,6,6,7,'#');
 // Gate wall to the Foundry: a wide freight gate and a narrow service door.
 rectangle(g(),1,7,9,8,'#');rectangle(g(),13,7,19,8,'#');rectangle(g(),21,7,23,8,'#');
 m.doors.push_back({9,13,7.5f});m.doors.push_back({19,21,7.5f});
 // Receiving floor: crate stacks and drums give cover across the approach.
 for(auto c:{std::pair{8,2},{8,3},{9,2},{13,4},{14,4},{14,5}})g().rows[c.second][c.first]='C';
 // Drums stand clear of every doorway and the lift mouth.
 for(auto b:{std::pair{12,2},{22,5},{7,1}})g().rows[b.second][b.first]='B';
 forklift(m,17.2f,4.2f,0);
 // A 1.1 m concrete dock bumper: low enough to vault with a crouch-jump.
 wall(m,10.5f,3.f,12.f,3.35f,0,1.06f,15);wall(m,10.5f,3.f,12.f,3.35f,1.06f,1.1f,13);
 // Supervisor's booth: framed walls with a real doorway, desk, chair, shelf.
 wall(m,18,1,18.15f,3.85f,0,low,3);
 wall(m,18,3.85f,19.2f,4,0,low,3);wall(m,20.4f,3.85f,23,4,0,low,3);wall(m,19.2f,3.85f,20.4f,4,2.2f,low,3);
 for(float y:{1.8f,2.9f})wall(m,18,y,18.15f,y+.6f,.95f,2.1f,17); // view glass toward the floor
 recordsDesk(m,21.3f,2.f,0);
 terminal(m,{21.3f,2.05f},"SHIFT LOG / 03:17","COOLANT FAILED. THE NIGHT CREW","SEALED FOUNDRY WITH US INSIDE.");
 shelf(m,{19.4f,1.3f},0);
 m.lights.push_back(lamp({20.6f,2.4f},low-.15f,.9f,4.f,3200));
 serviceCeiling(m,1,1,23,7,low);
 // The cab: buckled car walls and the snapped cable drum overhead.
 wall(m,1.1f,1.1f,4.9f,1.25f,0,2.6f,11);wall(m,1.1f,1.1f,1.25f,6.9f,0,2.6f,11);wall(m,1.1f,6.75f,4.9f,6.9f,0,2.6f,11);
 wall(m,1.1f,1.1f,4.9f,6.9f,2.6f,2.75f,11);
 for(float y:{2.2f,4.1f,5.8f})wall(m,1.25f,y,1.35f,y+.08f,.3f,2.5f,2);
 m.pipes.push_back({{3,1.4f},{3,1.4f},3.2f,.025f,2.75f,2});
 floorLine(m,5.2f,3.f,9,3.12f,0,13);floorLine(m,5.2f,5.88f,9,6.f,0,13);
 wallSign(m,16,6.93f,2.35f,"02 / FOUNDRY","HOT METAL / USE THE BRIDGES",2.6f,kPi);
 wallSign(m,22,6.93f,2.35f,"SERVICE","AUTHORISED STAFF",1.4f,kPi);

 // --- 02 FOUNDRY ---------------------------------------------------------
 // Brick piers carry the roof; the west wall screens the service passage.
 for(auto pier:{std::pair{6,10},{6,13},{18,14}})g().rows[pier.second][pier.first]='#';
 rectangle(g(),3,10,4,16,'#');
 // A ventilation trunk has dropped across the west passage on its hangers:
 // the way through is a crawl under it.
 wall(m,1,12.f,3,13.6f,1.f,1.62f,11);
 for(float y:{12.f,13.52f})wall(m,1,y,3,y+.08f,.92f,1.7f,12);
 // Hazard banding on both faces marks the duct as something to duck under.
 for(float y:{11.97f,13.6f})wall(m,1.f,y,3.f,y+.03f,1.1f,1.3f,13);
 for(float x:{1.25f,2.75f})m.pipes.push_back({{x,12.8f},{x,12.8f},1.62f,.02f,6.2f,2});
 // Converter plinth: two vessels, each venting up a flue into the roof.
 rectangle(g(),10,10,14,13,'T');
 for(float x:{11.f,13.f}){m.fixtures.push_back({39,{x,11.5f},0,1.9f,1.4f,2.7f,0,false});m.fixtures.push_back({40,{x,11.5f},2.7f,.4f,.4f,.7f,0,false});
  m.pipes.push_back({{x,11.5f},{x,11.5f},3.4f,.2f,hall,2});}
 m.structures.push_back({10.04f,10.04f,13.96f,12.96f,.14f,.165f,false,25});
 // The pour: an open molten channel wall to wall, crossed on two steel bridges.
 const float c1=13.2f,c2=13.85f;
 wall(m,4,c1,23,c2,0,.012f,25);
 for(float x:{7.4f,15.4f}){wall(m,x,c1-.25f,x+1.3f,c2+.25f,0,.09f,12);
  // Handrails along both sides of the walkway, open at each end.
  for(float side:{x,x+1.22f})wall(m,side,c1-.25f,side+.08f,c2+.25f,.09f,1.0f,12);}
 hazard(m,Hazard::Kind::Fire,4,c1,7.4f,c2,-1,.7f,32);
 hazard(m,Hazard::Kind::Fire,8.7f,c1,15.4f,c2,-1,.7f,32);
 hazard(m,Hazard::Kind::Fire,16.7f,c1,23,c2,-1,.7f,32);
 // Mezzanine: a steel deck along the north-east wall with the console.
 rectangle(g(),15,8,23,10,'.');deck(m,2.6f,15,8,23,10);
 m.stairs.push_back({20,10,22,14.5f,0,2.6f,13,true,false});
 m.fixtures.push_back({41,{8,15.3f},0,2.4f,1.f,1.1f,0,true});
 for(Vec2 b:{Vec2{4.3f,8.6f},Vec2{4.3f,15.4f},Vec2{22.7f,15.4f}})m.fixtures.push_back({43,b,0,.6f,.5f,hall-.2f,kPi*.5f,true});
 terminal(m,{21.f,8.6f},"CONTAINMENT / 04:06","THE VESSELS WERE NOT EMPTY.","CLEAR THE HOSTILES. GET OUT.",2.6f);
 // Roof trusses across the hall.
 for(float y:{8.5f,12.f,15.5f})wall(m,1,y,23,y+.18f,hall-.5f,hall-.3f,2);
 wallSign(m,6.f,8.06f,2.4f,"HOT METAL","CROSS ON THE BRIDGES ONLY",2.4f,0);

 // --- 03 CONTAINMENT -----------------------------------------------------
 // Openings from the Foundry: the west service passage and the east hall.
 rectangle(g(),1,16,2,17,'#');rectangle(g(),4,16,15,17,'#');rectangle(g(),19,16,23,17,'#');
 // Holding cells off a two-metre corridor, each breached, a burst vessel inside.
 rectangle(g(),1,19,2,20,'#');rectangle(g(),3,19,7,20,'#');rectangle(g(),8,19,12,20,'#');rectangle(g(),13,19,16,20,'#');
 rectangle(g(),5,20,6,23,'#');rectangle(g(),10,20,11,23,'#');rectangle(g(),15,20,16,23,'#');
 for(float x:{3.4f,8.2f,13.2f}){float h=2.2f,scale=h/2.390135f;m.fixtures.push_back({14,{x,21.6f},0,1.58f*scale,1.62f*scale,h,.3f,true});}
 for(float x:{2.f,7.f,12.f})floorLine(m,x+.05f,19.9f,x+.95f,20.05f,0,13);
 // Decontamination: steel-lined room with the exit gate.
 transferExit(m);
 equipmentPad(m,16.3f,17.3f,18.9f,19.6f,0);
 terminal(m,{17.6f,20.6f},"DISPATCH / LAST SIGNAL","PRESSURE WORKS STILL HAS POWER.","THE TRANSFER INTERLOCK IS LIVE.");
 serviceCeiling(m,1,17,23,23,3.f);
 wallSign(m,20.055f,21.9f,2.2f,"04 / PRESSURE WORKS","TRANSFER INTERLOCK",1.6f,kPi*.5f);

 // --- Light ---------------------------------------------------------------
 // Intake: cold fluorescent tubes; red emergency lamp in the wrecked cab.
 m.lights.push_back(lamp({8.5f,3.5f},low-.12f,1.1f,6.5f,4300,.3f));
 m.lights.push_back(lamp({14.5f,2.5f},low-.12f,1.f,6.f,4300,.3f));
 m.lights.push_back(lamp({1.25f,4.f},2.f,1.3f,4.f,1300,-.3f,WorldLightMount::Wall,0));
 // Foundry: sodium high-bays and the molten pour's own orange glow.
 m.lights.push_back(lamp({12.f,11.5f},3.6f,2.6f,9.f,1700));
 m.lights.push_back(lamp({7.f,9.f},hall-.4f,1.6f,9.f,2100));
 m.lights.push_back(lamp({16.f,14.5f},hall-.4f,1.5f,8.f,2100));
 m.lights.push_back(lamp({8.f,14.8f},hall-.4f,1.3f,7.5f,2100));
 m.lights.push_back(lamp({19.f,8.6f},5.2f,1.1f,5.5f,4300,.25f));
 m.lights.push_back(lamp({1.f,12.5f},2.4f,.9f,4.5f,4300,.35f,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({1.f,11.f},1.6f,.7f,3.f,1400,-.3f,WorldLightMount::Wall,0));
 // Containment: sick green-white tubes, cells left dark, red at the gate.
 m.lights.push_back(lamp({5.5f,18.f},2.85f,1.f,5.5f,6200,.45f));
 m.lights.push_back(lamp({12.5f,18.f},2.85f,.9f,5.5f,6200,.45f));
 // Each cell keeps one weak warm caged lamp: dark, but a shape is readable.
 for(float x:{3.f,8.f,13.f})m.lights.push_back(lamp({x,23.f},2.2f,.55f,3.8f,1900,0,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({18.5f,18.5f},2.85f,1.2f,6.f,5200,.15f));
 m.lights.push_back(lamp({23.f,21.f},2.1f,1.4f,4.5f,1250,-.35f,WorldLightMount::Wall,kPi));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{15.5f,2.5f}},{C::Wasp,{21.f,5.5f}},
                   {C::Brute,{8.f,11.5f}},{C::Huntsman,{16.5f,15.f}},{C::Wasp,{18.5f,9.f},2.6f},
                   {C::Huntsman,{6.6f,22.3f}},{C::Wasp,{12.5f,18.f}},{C::Brute,{18.f,20.f}}};
 m.pickupSpawns={{{7.f,5.8f},P::Ammo},{{22.f,8.8f},P::Health,2.6f},{{1.6f,14.6f},P::Ammo},{{14.3f,22.3f},P::Health}};
 const Vec2 junk[]={{6.5f,1.8f},{6.6f,4.2f},{2.f,9.2f},{16.5f,11.5f},{3.8f,20.2f},{17.f,22.f}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,junk[i],-999,i*.7f});

 zone(m,"01 INTAKE",0,0,24,7.5f);
 zone(m,"02 FOUNDRY",0,7.5f,24,16.5f);
 zone(m,"03 CONTAINMENT",0,16.5f,24,24);
 return p;
}
// PRESSURE WORKS. The transfer from Containment drops into a rail receiving
// dock of stacked freight; through the pressure wall is the Pump Hall, four
// high-pressure vessels on a plinth fed by riser bundles, lit by mercury lamps;
// south lies the Control floor with the night shift's glazed booth and the
// transfer gate to the Turbine Gantry.
std::shared_ptr<const Map> makePressureWorks(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[1];m.definition=CampaignChunks[1];m.definition.ceiling=5.5f;m.definition.ambient=.32f;
 m.layers.push_back(floorLayer("Pressure Works / ground",0));
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 const float hall=5.5f;

 // --- 04 RAIL DOCK ---------------------------------------------------------
 rectangle(g(),2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true});
 rectangle(g(),1,7,3,8,'#');rectangle(g(),6,7,18,8,'#');rectangle(g(),21,7,23,8,'#');
 m.doors.push_back({3,6,7.5f});m.doors.push_back({18,21,7.5f});
 // Rail spur along the dock with freight left where the last train stopped.
 for(float y:{3.15f,3.85f})floorLine(m,6,y,23,y+.1f,0,2);
 for(float x=6.3f;x<23;x+=.8f)floorLine(m,x,3.f,x+.18f,4.1f,0,11);
 cargo(m,9.f,2.f,0);cargo(m,10.4f,2.f,0,false);cargo(m,13.5f,5.4f,0);cargo(m,12.2f,5.4f,0);
 for(auto c:{std::pair{8,5},{8,6},{17,1},{18,1}})g().rows[c.second][c.first]='C';
 g().rows[5][21]='B';
 forklift(m,15.6f,4.9f,0);
 for(float y:{1.6f,3.6f})shelf(m,{22.65f,y+.6f},-kPi*.5f);
 terminal(m,{6.5f,1.4f},"RAIL DOCK / MANIFEST","THE LAST TRAIN ARRIVED EMPTY.","THE PRESSURE GAUGES KEPT RISING.");
 serviceCeiling(m,1,1,23,7,3.6f);
 wallSign(m,9.5f,6.93f,2.4f,"05 / PUMP HALL","HIGH PRESSURE / EAR DEFENDERS",2.2f,kPi);

 // --- 05 PUMP HALL -------------------------------------------------------
 for(auto pier:{std::pair{6,10},{6,14},{18,10},{18,14}})g().rows[pier.second][pier.first]='#';
 // The vessel plinth: four pressure tanks fed from an overhead header.
 equipmentPad(m,8.6f,9.6f,15.4f,15.4f,0);
 for(float x:{10.4f,13.6f})for(float y:{11.f,14.f}){float h=3.f,scale=h/2.390135f;
  m.fixtures.push_back({14,{x,y},.1f,1.58f*scale,1.62f*scale,h,0,true});
  m.pipes.push_back({{x,y},{x,y},3.1f,.14f,hall,3});}
 servicePipe(m,{8.6f,12.5f},{15.4f,12.5f},4.6f,.22f,3);
 servicePipe(m,{1,9.f},{23,9.f},4.8f,.16f,4);servicePipe(m,{1,16.f},{23,16.f},4.8f,.16f,4);
 // Riser bundles with valve wheels along both walls.
 for(float y:{9.2f,12.6f,15.4f}){m.fixtures.push_back({44,{1.06f,y},0,.4f,.1f,4.7f,kPi*.5f,false});m.fixtures.push_back({45,{1.13f,y},1.25f,.32f,.12f,.32f,kPi*.5f,false});}
 for(float y:{9.6f,13.6f}){m.fixtures.push_back({44,{22.94f,y},0,.4f,.1f,4.7f,-kPi*.5f,false});m.fixtures.push_back({45,{22.87f,y},1.25f,.32f,.12f,.32f,-kPi*.5f,false});}
 // Cover: drums and a crate line on the west flank, a breaker cage east.
 for(auto c:{std::pair{3,12},{4,12}})g().rows[c.second][c.first]='C';
 for(auto b:{std::pair{8,9},{20,12},{16,16}})g().rows[b.second][b.first]='B';
 m.fixtures.push_back({41,{21.f,15.6f},0,2.4f,1.f,1.1f,0,true});
 // A blown gland on the north-west vessel vents across the plinth.
 hazard(m,Hazard::Kind::Steam,9.f,10.2f,9.5f,11.8f,-1,2.f,14);
 for(float y:{8.5f,12.5f,16.2f})wall(m,1,y,23,y+.18f,hall-.4f,hall-.22f,2);
 wallSign(m,8.5f,16.93f,2.4f,"06 / CONTROL","NIGHT SHIFT ONLY",2.f,kPi);

 // --- 06 CONTROL ---------------------------------------------------------
 rectangle(g(),1,17,2,18,'#');rectangle(g(),5,17,12,18,'#');rectangle(g(),15,17,23,18,'#');
 m.doors.push_back({2,5,17.5f});m.doors.push_back({12,15,17.5f});
 // Operator floor: a bank of desks facing the glazed supervisor's booth.
 for(float x:{4.f,7.5f,11.f})recordsDesk(m,x,20.6f,0);
 for(float x:{2.f,3.f,13.f})m.fixtures.push_back({13,{x,22.74f},0,.9f,.5f,1.9f,0,true});
 // Booth: steel sill and glass along x 16, open to the south.
 wall(m,16.f,18.f,16.15f,20.4f,0,1.f,3);wall(m,16.f,18.f,16.15f,20.4f,1.f,2.6f,17);wall(m,16.f,18.f,16.15f,20.4f,2.6f,3.f,3);
 recordsDesk(m,18.f,19.f,0);
 terminal(m,{18.f,19.05f},"CONTROL / NIGHT SHIFT","SURFACE LIFT IS ON EMERGENCY POWER.","THE PUMPS MUST STAY RUNNING.");
 transferExit(m);
 serviceCeiling(m,1,18,23,23,3.f);
 wallSign(m,20.055f,21.9f,2.2f,"07 / TURBINE HALL","TRANSFER INTERLOCK",1.6f,kPi*.5f);

 // --- Light ---------------------------------------------------------------
 m.lights.push_back(lamp({9.f,3.5f},3.45f,1.f,6.5f,4300,.3f));
 m.lights.push_back(lamp({16.f,3.5f},3.45f,1.f,6.5f,4300,.3f));
 m.lights.push_back(lamp({3.5f,3.f},3.45f,.8f,5.f,3400));
 m.lights.push_back(lamp({12.f,13.4f},hall-.6f,2.f,9.f,5600,.35f));
 m.lights.push_back(lamp({4.f,10.5f},hall-.3f,1.2f,7.f,2000));
 m.lights.push_back(lamp({20.f,14.f},hall-.3f,1.2f,7.f,2000));
 m.lights.push_back(lamp({10.5f,17.f},2.4f,1.3f,3.5f,1300,-.3f,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({7.5f,19.5f},2.85f,1.f,6.f,3200));
 m.lights.push_back(lamp({19.5f,19.5f},2.85f,1.1f,5.f,3800,.1f));
 m.lights.push_back(lamp({23.f,21.f},2.1f,1.4f,4.5f,1250,-.35f,WorldLightMount::Wall,kPi));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{17.5f,4.5f}},{C::Wasp,{11.f,4.6f}},
                   {C::Brute,{4.5f,14.5f}},{C::Huntsman,{8.f,12.5f}},{C::Wasp,{19.5f,12.5f}},{C::Brute,{21.f,10.5f}},
                   {C::Huntsman,{9.5f,19.2f}},{C::Wasp,{13.5f,20.5f}},{C::Brute,{21.5f,19.5f}}};
 m.pickupSpawns={{{4.5f,4.5f},P::Ammo},{{16.5f,2.5f},P::Health},{{2.5f,15.5f},P::Ammo},{{22.f,11.5f},P::Ammo},{{7.5f,22.f},P::Health},{{17.2f,21.5f},P::Ammo}};
 const Vec2 junk[]={{6.8f,3.2f},{7.1f,3.4f},{9.2f,12.2f},{9.5f,8.6f},{18.7f,19.8f},{19.1f,20.f}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,junk[i],-999,i*.7f});

 zone(m,"04 RAIL DOCK",0,0,24,7.5f);
 zone(m,"05 PUMP HALL",0,7.5f,24,17.5f);
 zone(m,"06 CONTROL",0,17.5f,24,24);
 return p;
}

// TURBINE GANTRY. A seven-metre turbine hall: the main set and its generator
// on the floor with two auxiliary units, ringed at three metres by a steel
// gantry. The gantry loops from the west stair along the north wall to the
// glazed control room, and back across the hall on a bridge. The control room
// releases the turbine brakes, which unseals the transfer interlock in the
// switchgear bay to the south; the east stair drops back down to it.
std::shared_ptr<const Map> makeTurbineGantry(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[2];m.definition=CampaignChunks[2];m.definition.ceiling=7.f;m.definition.ambient=.36f;
 m.layers.push_back(floorLayer("Turbine Gantry / ground",0));
 {AuthoredLayerData upper;upper.name="Turbine Gantry / upper catwalk";upper.elevation=3;upper.thickness=.3f;
  for(auto& row:upper.rows)row=std::string(24,'.');m.layers.push_back(std::move(upper));}
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 const float hall=7.f,deckZ=3.f;

 // --- 07 TURBINE HALL ----------------------------------------------------
 rectangle(g(),2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true});
 // Main set: turbine casing on its plinth, coupled to the generator, with
 // exhaust trunks rising into the roof.
 equipmentPad(m,8.7f,9.2f,13.3f,11.8f,0);
 m.fixtures.push_back({46,{11.f,10.5f},.1f,3.76f,1.9f,2.66f,0,true});
 const float generator=.75f;
 m.fixtures.push_back({6,{15.5f,10.5f},0,3.4f*generator,generator,1.8f*generator,0,true});
 m.pipes.push_back({{12.9f,10.5f},{14.2f,10.5f},.62f,.13f,-999,2});
 for(float x:{10.2f,11.8f})m.pipes.push_back({{x,10.5f},{x,10.5f},2.76f,.24f,hall,3});
 // Auxiliary units on the floor south of the main set.
 for(Vec2 a:{Vec2{6.f,14.f},Vec2{16.5f,14.f}}){
  equipmentPad(m,a.x-1.3f,a.y-1.3f,a.x+1.3f,a.y+1.3f,0);
  m.fixtures.push_back({46,a,.1f,1.9f,1.9f,1.6f,0,true});
  m.pipes.push_back({a,a,1.7f,.16f,hall,3});
 }
 // A cracked gland on the west auxiliary unit vents steam across its plinth.
 hazard(m,Hazard::Kind::Steam,7.3f,13.2f,7.9f,14.8f,-1,2.2f,12);
 // Condensate mains along the west wall and the turbine feed overhead.
 servicePipe(m,{1.2f,3.f},{1.2f,16.5f},1.4f,.14f,3);
 servicePipe(m,{1.f,12.f},{23.f,12.f},6.2f,.2f,4);
 // Cover on the hall floor: drums by the entry lane and a crate stack.
 for(auto b:{std::pair{6,4},{14,4},{15,4}})g().rows[b.second][b.first]='B';
 for(auto c:{std::pair{3,13},{3,14},{12,15}})g().rows[c.second][c.first]='C';
 // Roof trusses.
 for(float y:{4.5f,9.f,13.5f})wall(m,1,y,23,y+.2f,hall-.55f,hall-.35f,2);

 // --- 08 UPPER GANTRY ----------------------------------------------------
 deck(m,deckZ,1,1,23,3);    // north run, over the entry lane
 deck(m,deckZ,1,3,3,11);    // west run, down to the west stair
 deck(m,deckZ,3,6,17,8);    // bridge across the hall
 deck(m,deckZ,17,3,23,9);   // control room floor
 m.stairs.push_back({1,11,3,16,0,deckZ,15,true,false});
 m.stairs.push_back({20,9,22,14,0,deckZ,15,true,false});
 // Control room: steel sill, glazing and frame, roofed; doorways from the
 // north run (x 20-21.5) and the bridge (y 6-8).
 auto glazed=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,deckZ,deckZ+.9f,3);wall(m,x1,y1,x2,y2,deckZ+.9f,deckZ+2.4f,17);wall(m,x1,y1,x2,y2,deckZ+2.4f,deckZ+2.7f,3);};
 glazed(17,3,17.12f,6);glazed(17,8,17.12f,9);
 glazed(17.12f,3,20,3.12f);glazed(21.5f,3,23,3.12f);
 wall(m,20,3,21.5f,3.12f,deckZ+2.2f,deckZ+2.7f,3);
 wall(m,17,6,17.12f,8,deckZ+2.2f,deckZ+2.7f,3);
 wall(m,17,3,23,9,deckZ+2.7f,deckZ+2.85f,2);
 {AuthoredTerminalData t;t.position={19.5f,4.f};t.title="GANTRY CONTROL / 05:42";t.line1="TURBINE BRAKES RELEASED.";t.line2="LOWER TRANSFER INTERLOCK UNSEALED.";
  t.z=deckZ;t.control=true;t.yaw=kPi;m.terminals.push_back(std::move(t));}
 recordsDesk(m,21.6f,5.2f,deckZ);
 for(float x:{22.65f})shelf(m,{x,7.6f},-kPi*.5f,deckZ);
 wallSign(m,8.f,1.06f,deckZ+1.6f,"08 / UPPER GANTRY","KEEP TO THE WALKWAYS",2.4f,0);

 // --- 09 SWITCHGEAR ------------------------------------------------------
 rectangle(g(),1,17,8,18,'#');rectangle(g(),11,17,20,18,'#');
 m.doors.push_back({8,11,17.5f});m.doors.push_back({20,23,17.5f});
 for(float x:{2.f,3.f,4.f,5.f,6.f})m.fixtures.push_back({13,{x,18.27f},0,.9f,.5f,1.9f,kPi,true});
 m.fixtures.push_back({41,{14.f,20.6f},0,2.4f,1.f,1.1f,0,true});
 for(float x:{12.f,16.f})m.fixtures.push_back({8,{x,22.9f},.9f,.67f,.2f,.91f,0,false});
 // A burst breaker arcs across the floor at the west end of the bank.
 hazard(m,Hazard::Kind::Electricity,1.f,20.6f,2.6f,22.f,-1,2.f,18);
 transferExit(m);m.doors.back().requireControl=true;
 serviceCeiling(m,1,18,23,23,3.2f);
 wallSign(m,20.055f,21.9f,2.2f,"10 / SURFACE LIFT","TRANSFER INTERLOCK",1.6f,kPi*.5f);
 wallSign(m,14.5f,18.06f,2.2f,"DANGER","LIVE SWITCHGEAR",2.f,0);

 // --- Light ---------------------------------------------------------------
 // Hall: metal-halide high bays; amber lamps along the gantry walls.
 for(Vec2 b:{Vec2{5.f,10.f},Vec2{12.5f,5.f},Vec2{17.f,12.5f},Vec2{9.f,15.f},Vec2{12.5f,12.8f},Vec2{20.f,15.5f}})m.lights.push_back(lamp(b,hall-.4f,2.5f,12.f,4200,.15f));
 for(float x:{4.f,10.f,16.f})m.lights.push_back(lamp({x,1.f},deckZ+1.9f,1.1f,6.f,2600,.1f,WorldLightMount::Wall,kPi*.5f));
 for(float y:{5.5f,9.5f})m.lights.push_back(lamp({1.f,y},deckZ+1.9f,1.1f,6.f,2600,.1f,WorldLightMount::Wall,0));
 // An inspection lamp slung under the bridge picks out the main set.
 m.lights.push_back(lamp({11.f,7.6f},deckZ-.42f,1.4f,6.f,3300));
 // Entry lane under the gantry: low tubes on the deck underside.
 for(float x:{6.f,12.f,18.f})m.lights.push_back(lamp({x,2.f},deckZ-.4f,.8f,4.5f,3600,.2f));
 // Control room: warm desk light and a cold console glow.
 m.lights.push_back(lamp({19.5f,6.f},deckZ+2.55f,1.f,5.f,2900));
 // Switchgear: cold tubes, a blue-white arc light and red at the gate.
 m.lights.push_back(lamp({6.f,20.5f},3.05f,1.f,6.f,5200,.25f));
 m.lights.push_back(lamp({14.f,20.5f},3.05f,.9f,6.f,5200,.25f));
 m.lights.push_back(lamp({1.f,21.3f},1.5f,1.2f,3.5f,9000,-.1f,WorldLightMount::Wall,0));
 m.lights.push_back(lamp({23.f,21.f},2.1f,1.4f,4.5f,1250,-.35f,WorldLightMount::Wall,kPi));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{8.f,4.5f}},{C::Wasp,{14.f,13.f}},{C::Brute,{4.5f,10.5f}},
                   {C::Huntsman,{10.f,7.f},deckZ},{C::Wasp,{16.f,15.5f}},{C::Brute,{10.5f,21.f}}};
 m.pickupSpawns={{{3.5f,3.5f},P::Ammo},{{2.f,8.5f},P::Health,deckZ},{{15.5f,7.f},P::Ammo,deckZ},{{21.5f,19.f},P::Ammo}};
 const Vec2 junk[]={{7.4f,3.6f},{7.6f,4.6f},{12.5f,19.2f},{13.f,19.6f},{18.4f,5.f},{18.9f,5.4f}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,junk[i],i>=4?deckZ:-999.f,i*.7f});

 zone(m,"08 UPPER GANTRY",0,0,24,24,2.5f,1000);
 zone(m,"07 TURBINE HALL",0,0,24,17.5f);
 zone(m,"09 SWITCHGEAR",0,17.5f,24,24);
 return p;
}

// REACTOR COMPLEX. The surface lift's collar: an entry corridor into the lift
// lobby, with the hoist plant behind it and a casualty by the landing. The cab
// climbs three storeys of shaft, jams and falls into the containment pit.
// Below, a two-storey atrium stands round the reactor vessel, with the P-01
// feed pumps west and the R-03 control room east (the bulkhead out, and the
// Warden). A west stair climbs to the upper manifold: the maintenance bench
// with the authorization disk overlooks the vessel, and the P-02 return station
// stands on the east balcony.
std::shared_ptr<const Map> makeReactorComplex(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[3];m.definition=CampaignChunks[3];m.definition.ceiling=16.f;m.definition.ambient=.28f;
 const float lower=-9,upper=-6;
 m.layers.push_back(floorLayer("Reactor / lower containment",lower));
 auto layer=[&](const char* name,float z,float thickness){AuthoredLayerData l;l.name=name;l.elevation=z;l.thickness=thickness;
  for(auto& row:l.rows)row=std::string(24,'.');m.layers.push_back(std::move(l));};
 layer("Reactor / upper manifold",upper,.3f);layer("Scenery / coolant risers",-3,.25f);layer("Surface lift / collar",0,.3f);
 layer("Scenery / ventilation plant",3,.25f);layer("Scenery / electrical service",6,.25f);layer("Surface / sealed gates",9,.35f);
 auto L=[&](size_t i)->AuthoredLayerData&{return m.layers[i];};
 // The shaft (x 9-15, y 9-15) passes every storey; at each passing storey a
 // ring of floor encloses it as a bay seen from the moving cab.
 auto ring=[](AuthoredLayerData& l){for(int x=8;x<16;++x){l.rows[8][x]='=';l.rows[15][x]='=';}for(int y=9;y<15;++y){l.rows[y][8]='=';l.rows[y][15]='=';}};
 for(size_t i:{2u,4u,5u,6u})ring(L(i));
 for(float z:{-3.f,3.f,6.f,9.f}){
  wall(m,7.8f,7.8f,8,16.2f,z,z+2.75f,3);wall(m,16,7.8f,16.2f,16.2f,z,z+2.75f,3);
  wall(m,8,7.8f,16,8,z,z+2.75f,3);wall(m,8,16,16,16.2f,z,z+2.75f,3);
  wall(m,7.8f,7.8f,9,16.2f,z+2.65f,z+2.7f,2);wall(m,15,7.8f,16.2f,16.2f,z+2.65f,z+2.7f,2);
  wall(m,9,7.8f,15,9,z+2.65f,z+2.7f,2);wall(m,9,15,15,16.2f,z+2.65f,z+2.7f,2);
 }
 // Roof slabs of the manifold storey (-3.3) and of the collar (2.7).
 for(float z:{-3.3f,2.7f}){wall(m,1,1,23,9,z,z+.12f,2);wall(m,1,15,23,23,z,z+.12f,2);wall(m,1,9,9,15,z,z+.12f,2);wall(m,15,9,23,15,z,z+.12f,2);}

 // --- 10 SURFACE LIFT (collar, 0 m) ----------------------------------------
 {auto& c=L(3);
  rectangle(c,2,0,5,4,'=');rectangle(c,1,4,23,9,'=');rectangle(c,14,1,23,4,'=');ring(c);
  c.rows[8][11]=c.rows[8][12]='S';}
 rectangle(L(0),2,0,5,1,'.');m.doors.push_back({2,5,.5f,0,false,false,true,9});
 wall(m,2,0,5,1,lower,-.3f,3);
 // The collar's ring round the shaft is the shaft wall, not a corridor.
 for(float x:{8.f,15.f})wall(m,x,9,x+1,9.12f,0,2.7f,3);
 // Landing plate into the cab, caged on both sides.
 wall(m,11,8,13,10,-.3f,0,7);
 m.structures.push_back({10.95f,8,11,10,0,2.1f,true});m.structures.push_back({13,8,13.05f,10,0,2.1f,true});
 terminal(m,{13.9f,8.55f},"LIFT SYSTEM / OVERRIDE","SURFACE GATES CLAMPED SHUT.","LAST CAB TO THE TOP. BOARD AND DISPATCH.",9);
 {AuthoredTerminalData t;t.position={13.35f,11.5f};t.title="CAB CONTROL / DISPATCH";t.line1="ASCENDING TO SURFACE.";t.line2="WARNING: SAME BRAKE MODEL AS SERVICE LIFT.";
  t.z=9;t.control=true;m.terminals.push_back(std::move(t));}
 // Lobby: lockers on the north wall, freight waiting for a cab that never came.
 for(float x:{6.f,7.f,8.f})m.fixtures.push_back({13,{x,4.3f},9,.9066f,.4956f,2.2f,kPi,true});
 cargo(m,3.f,7.6f,0);cargo(m,19.2f,7.6f,0);cargo(m,20.6f,7.6f,0,false);
 for(auto b:{std::pair{22,5},{1,5}})m.fixtures.push_back({15,{b.first+.5f,b.second+.5f},9,1.2f,1.15f,.16f,0,true});
 // Hoist plant: the traction motor and its cable drum, with the drive panels.
 m.fixtures.push_back({6,{18.f,2.2f},9,2.38f,.7f,1.26f,0,true});
 m.pipes.push_back({{19.6f,2.2f},{21.6f,2.2f},1.f,.45f,-999,2});
 for(float x:{19.6f,21.5f})wall(m,x,1.9f,x+.12f,2.5f,0,1.f,12);
 for(float x:{15.f,16.f})m.fixtures.push_back({13,{x,1.3f},9,.9066f,.4956f,2.2f,kPi,true});
 wallSign(m,6.5f,8.89f,1.9f,"10 / SURFACE LIFT","LAST CAB / BOARD AND DISPATCH",2.4f,kPi);
 wallSign(m,22.945f,2.5f,1.9f,"HOIST PLANT","NO ENTRY WHILE CAB MOVES",1.6f,-kPi*.5f);

 // --- 11 REACTOR CONTAINMENT (-9 m) ----------------------------------------
 // Upper manifold floor over everything but the shaft, the west stair well
 // and the atrium round the vessel.
 {auto& u=L(1);rectangle(u,1,1,23,23,'=');rectangle(u,9,9,15,15,'.');rectangle(u,1,9,3,15,'.');rectangle(u,9,16,15,22,'.');}
 m.stairs.push_back({1,9,3,15,lower,upper,15,true,false});
 const float roomTop=upper-.3f;
 // P-01 feed pump room (west) and R-03 control room (east) open on the atrium.
 wall(m,1,16.85f,9,17,lower,roomTop,3);
 wall(m,8.85f,17,9,18.6f,lower,roomTop,3);wall(m,8.85f,20.2f,9,23,lower,roomTop,3);wall(m,8.85f,18.6f,9,20.2f,roomTop-.6f,roomTop,3);
 wall(m,15,16.85f,23,17,lower,roomTop,3);
 wall(m,15,17,15.15f,18.6f,lower,roomTop,3);wall(m,15,20.2f,15.15f,23,lower,roomTop,3);wall(m,15,18.6f,15.15f,20.2f,roomTop-.6f,roomTop,3);
 // The vessel rises through the atrium to the manifold roof.
 m.structures.push_back({10.5f,17.5f,13.5f,20.5f,lower,-3.4f,false,1});
 // Terminal order is the puzzle's: log, R-03 computer, P-01 feed, P-02 return.
 terminal(m,{7.8f,21.9f},"MAINTENANCE / SHIFT LOG","RETURN TRIPPED AGAIN. NO FEED PRESSURE.","LEFT THE SERVICE DISK WITH THE SPARES.",3);
 {AuthoredTerminalData t;t.position={17.4f,21.9f};t.title="REACTOR ACCESS / DRIVE A:";t.reactorAction=1;m.terminals.push_back(std::move(t));}
 {AuthoredTerminalData t;t.position={3.2f,21.9f};t.title="01 / COOLANT FEED";t.reactorAction=2;m.terminals.push_back(std::move(t));}
 for(float x:{16.3f,18.45f})m.fixtures.push_back({13,{x,22.6f},0,.9066f,.4956f,2.2f,0,true});
 m.fixtures.push_back({19,{20.8f,18.3f},0,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({18,{20.8f,19.03f},0,.417f,.525f,.95f,kPi,true});
 // Feed pumps and the coolant main into the vessel jacket.
 m.props.push_back({0,{4.2f,18.4f},1.25f,2.2f,0,{1.1f,.66f},.15f});
 m.props.push_back({1,{7.2f,21.6f},1.12f,2.f,0,{1.f,.31f},.12f});
 servicePipe(m,{5.3f,18.4f},{10.4f,18.4f},-7.7f,.16f,3);
 servicePipe(m,{13.6f,19.f},{15.f,19.f},-7.2f,.12f,4);
 m.fixtures.push_back({44,{22.94f,12.f},0,.4f,.1f,2.6f,-kPi*.5f,false});m.fixtures.push_back({45,{22.87f,12.f},1.25f,.32f,.12f,.32f,-kPi*.5f,false});
 // North store: drums and crates, and a coolant spill along the east wall.
 for(auto c:{std::pair{4,2},{5,2},{4,3},{18,6},{19,6}})L(0).rows[c.second][c.first]='C';
 for(auto b:{std::pair{8,2},{14,3},{21,7}})L(0).rows[b.second][b.first]='B';
 // Leaked coolant pools in the corner, taped off.
 hazard(m,Hazard::Kind::Toxic,20.2f,1.2f,22.8f,4.f,lower-1,lower+1.2f,6);
 floorLine(m,20.f,4.f,23.f,4.15f,lower,13);floorLine(m,20.f,1.f,20.15f,4.15f,lower,13);
 wallSign(m,22.945f,5.2f,lower+1.8f,"DANGER","COOLANT LEAK / DO NOT ENTER",1.6f,-kPi*.5f);
 transferExit(m,lower);
 m.doors.back().requireControl=true;
 wallSign(m,14.945f,21.5f,lower+2.f,"R-03 CONTROL","BULKHEAD / LOCAL CONTROL",1.6f,-kPi*.5f);
 wallSign(m,9.055f,21.5f,lower+2.f,"P-01 FEED","COOLANT PUMPS",1.6f,kPi*.5f);

 // --- 12 UPPER MANIFOLD (-6 m) ----------------------------------------------
 wall(m,1,15.85f,5,16,upper,-3.3f,3);wall(m,6.5f,15.85f,9,16,upper,-3.3f,3);wall(m,5,15.85f,6.5f,16,-3.9f,-3.3f,3);
 // Maintenance bench (the authorization disk lies on it) overlooking the vessel.
 m.fixtures.push_back({19,{6.f,21.1f},3,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({18,{6.f,21.84f},3,.417f,.525f,.95f,0,true});
 m.fixtures.push_back({7,{3.f,22.5f},3,1.8f,.5f,1.8f,0,true});
 {AuthoredTerminalData t;t.position={17.5f,21.9f};t.title="02 / COOLANT RETURN";t.z=3;t.reactorAction=3;m.terminals.push_back(std::move(t));}
 for(float x:{15.6f,16.6f}){m.fixtures.push_back({44,{x,22.94f},3,.4f,.1f,2.6f,0,false});m.fixtures.push_back({45,{x,22.87f},4.2f,.32f,.12f,.32f,0,false});}
 // North pipe gallery: coolant headers over the shaft head.
 for(float y:{2.f,3.2f})servicePipe(m,{3.5f,y},{22.5f,y},-4.2f,.2f,y<3?3:4);
 wallSign(m,3.f,15.795f,-4.6f,"MAINTENANCE","SPARES / SERVICE BENCH",1.8f,kPi);
 wallSign(m,22.945f,19.f,-4.6f,"P-02 RETURN","COOLANT CIRCULATION",1.6f,-kPi*.5f);

 // --- Light ---------------------------------------------------------------
 // Collar: lobby tubes, the plant, an amber beacon over the landing.
 for(float x:{4.f,9.f,17.f})m.lights.push_back(lamp({x,6.5f},2.55f,1.f,6.f,4300,.25f));
 m.lights.push_back(lamp({3.5f,2.f},2.55f,.8f,4.f,4300,.25f));
 m.lights.push_back(lamp({18.5f,2.5f},2.55f,.9f,5.f,3000));
 m.lights.push_back(lamp({12.f,7.f},2.55f,1.1f,4.f,1800,-.1f));
 // Containment: cold control light, sodium in the pump room, red emergency
 // lamps and the vessel's blue glow from the atrium roof.
 m.lights.push_back(lamp({19.f,20.f},roomTop-.15f,1.1f,6.f,6500,.1f));
 m.lights.push_back(lamp({5.f,20.f},roomTop-.15f,1.f,6.f,2000));
 m.lights.push_back(lamp({5.f,5.f},roomTop-.15f,.7f,6.f,2800));
 m.lights.push_back(lamp({18.f,5.f},roomTop-.15f,.6f,6.f,1300,-.3f));
 m.lights.push_back(lamp({6.f,12.f},roomTop-.15f,.6f,5.f,1300,-.3f));
 m.lights.push_back(lamp({18.f,12.f},roomTop-.15f,.6f,5.f,1300,-.3f));
 for(Vec2 a:{Vec2{9.6f,17.f},Vec2{14.4f,21.f}})m.lights.push_back(lamp(a,-3.45f,1.6f,10.f,12000,-.05f));
 // Manifold: work lamps over the bench, the valve station and the gallery.
 m.lights.push_back(lamp({5.f,19.f},-3.45f,1.1f,6.f,3500));
 m.lights.push_back(lamp({19.f,19.5f},-3.45f,1.f,6.f,4500,.1f));
 for(float x:{6.f,18.f})m.lights.push_back(lamp({x,4.5f},-3.45f,.9f,7.f,2600,.1f));
 // Each storey the cab passes is lit in its own colour, so the ride reads as
 // movement: blue coolant risers, the collar, amber plant, red electrical,
 // white at the sealed surface gates.
 const std::pair<float,float> storeys[]={{-3.f,9000},{0.f,4300},{3.f,2200},{6.f,1300},{9.f,5200}};
 for(auto [z,kelvin]:storeys)for(Vec2 bay:{Vec2{8.5f,9.5f},Vec2{15.5f,14.5f}})m.lights.push_back(lamp(bay,z+(z==0?2.55f:2.5f),.9f,5.f,kelvin));
 // The cab lamp rides with the cab: it must be the map's last light.
 m.lights.push_back(lamp({12.f,11.8f},2.45f,1.4f,12.f,3600));

 // --- Encounters ------------------------------------------------------------
 using C=CreatureKind;using P=PickupKind;
 m.creatureSpawns={{C::Huntsman,{5.5f,5.f}},{C::Wasp,{12.f,16.3f}},{C::Warden,{21.2f,20.5f}},{C::Huntsman,{18.5f,5.5f},upper}};
 m.pickupSpawns={{{13.f,16.f},P::Health},{{2.5f,20.f},P::Ammo,upper},{{20.5f,11.5f},P::Ammo},{{16.5f,6.5f},P::Health,upper}};
 for(int i=0;i<6;++i)m.clutterSpawns.push_back({i,{6.6f+i*.45f,4.4f},lower,i*.7f});
 {ScriptEvent checkpoint;checkpoint.id=stateId("event_reactor_exit_checkpoint");checkpoint.x1=20;checkpoint.y1=21;checkpoint.x2=23;checkpoint.y2=24;
  checkpoint.bottom=-9.5f;checkpoint.top=-7.5f;checkpoint.requireState=stateId("reactor_bulkhead_released");checkpoint.requireEnemiesClear=true;
  ScriptAction save;save.type=ScriptAction::Type::Checkpoint;checkpoint.actions.push_back(save);m.scriptEvents.push_back(checkpoint);}

 zone(m,"10 SURFACE LIFT",0,0,24,24,-1.f,1000);
 zone(m,"12 UPPER MANIFOLD",0,0,24,24,-7.f,-1.f);
 zone(m,"11 REACTOR CONTAINMENT",0,0,24,24);
 return p;
}

// SERVICE GALLERY. The reactor bulkhead opens on a north landing; a four-metre
// service spine runs south under the headers and cable tray, straight on into
// the Coolant Return. Off the spine, each behind an open archway and a
// swinging door from its cross corridor: the pump workshop where the assembly
// was moved, the electrical room that must stay dry, the stores and the plant
// bay. Nothing hunts here: it is the breath between the reactor and the water.
std::shared_ptr<const Map> makeServiceGallery(){
 auto p=std::make_shared<Map>();auto& m=*p;
 const float floor=-9,roof=-5.8f,top=roof;
 m.name=CampaignMapNames[4];m.definition=CampaignChunks[4];m.definition.ceiling=roof;m.definition.ambient=.32f;
 m.layers.push_back(floorLayer("Reactor Service Gallery",floor,false,true));m.openSouth=true;
 auto g=[&]()->AuthoredLayerData&{return m.layers[0];};
 rectangle(g(),5,0,8,1,'.');m.doors.push_back({5,8,.5f,0,false,false,true});
 auto partition=[&](float x1,float y1,float x2,float y2){wall(m,x1,y1,x2,y2,floor,top,3);};
 auto swing=[&](float left,float right,float y){
  // A swinging leaf in a 25 cm frame, with the wall closed above the frame.
  Door d{left,right,y};d.swinging=true;m.doors.push_back(d);wall(m,left,y-.125f,right,y+.125f,floor+2.55f,top,3);};
 // Rooms: workshop (NW), electrical (NE), stores (SW), plant (SE). Each opens
 // on the spine through an archway and on its cross corridor through a door.
 partition(1,4.75f,9.85f,5);partition(14.15f,4.75f,23,5);
 for(float x:{9.85f,14.f}){partition(x,5,x+.15f,6.5f);partition(x,8.f,x+.15f,11.25f);wall(m,x,6.5f,x+.15f,8.f,floor+2.4f,top,3);
  partition(x,13,x+.15f,17.f);partition(x,18.5f,x+.15f,23);wall(m,x,17.f,x+.15f,18.5f,floor+2.4f,top,3);}
 partition(1,11,4.6f,11.25f);partition(6.f,11,10,11.25f);swing(4.6f,6.f,11.125f);
 partition(14,11,18.f,11.25f);partition(19.4f,11,23,11.25f);swing(18.f,19.4f,11.125f);
 partition(1,13,3.f,13.25f);partition(4.4f,13,10,13.25f);swing(3.f,4.4f,13.125f);
 partition(14,13,20.f,13.25f);partition(21.4f,13,23,13.25f);swing(20.f,21.4f,13.125f);
 partition(1,22.75f,10,23);partition(14,22.75f,23,23);

 // --- Landing --------------------------------------------------------------
 {AuthoredTerminalData t;t.position={14.5f,1.4f};t.title="SERVICE GALLERY / WORK ORDER";t.line1="PUMP ASSEMBLY MOVED TO WEST WORKSHOP.";t.line2="ELECTRICAL ROOM MUST REMAIN DRY.";t.yaw=.001f;m.terminals.push_back(std::move(t));}
 for(float x:{17.f,18.f,19.f})m.fixtures.push_back({13,{x,1.27f},0,.9066f,.4956f,2.2f,kPi,true});
 wallSign(m,10.5f,1.055f,floor+1.9f,"13 / SERVICE GALLERY","REACTOR SERVICE / COOLANT RETURN SOUTH",2.6f,0);
 // Spine services: headers along the west side, a cable tray along the east.
 for(float x:{10.4f,10.85f})servicePipe(m,{x,1.2f},{x,24.f},roof-.45f,.11f,x<10.6f?3:4);
 wall(m,13.55f,1,13.95f,24,roof-.42f,roof-.34f,2);
 for(float y=2.f;y<24;y+=3.f)wall(m,13.55f,y,13.95f,y+.06f,roof-.34f,roof,2);

 // --- 13A PUMP WORKSHOP -----------------------------------------------------
 m.fixtures.push_back({12,{4.f,6.f},0,3.06f,.94f,1.751f,0,true});
 m.props.push_back({0,{3.6f,9.1f},1.25f,2.2f,0,{1.1f,.66f},.15f});
 m.fixtures.push_back({19,{7.6f,9.3f},0,1.367f,.724f,.78f,0,true});
 m.fixtures.push_back({7,{1.27f,7.6f},0,1.8f,.5f,1.8f,kPi*.5f,true});
 wallSign(m,10.055f,9.5f,floor+1.8f,"PUMP WORKSHOP","P-01 ASSEMBLY",1.8f,kPi*.5f);

 // --- 13B ELECTRICAL ----------------------------------------------------------
 for(float y:{6.f,7.f,8.f,9.f})m.fixtures.push_back({13,{22.75f,y},0,.9066f,.4956f,2.2f,kPi*.5f,true});
 m.fixtures.push_back({41,{17.5f,6.f},0,2.4f,1.f,1.1f,0,true});
 // A cabinet with a cracked bus bar arcs across the floor in front of it.
 hazard(m,Hazard::Kind::Electricity,20.6f,8.6f,21.8f,9.4f,floor-1,floor+2,10);
 wallSign(m,13.945f,9.5f,floor+1.8f,"ELECTRICAL","KEEP DRY / LIVE GEAR",1.8f,-kPi*.5f);

 // --- 13C STORES --------------------------------------------------------------
 for(float y:{15.f,17.5f,20.f})m.fixtures.push_back({7,{1.27f,y},0,1.8f,.5f,1.8f,kPi*.5f,true});
 for(float y:{16.f,19.5f})m.fixtures.push_back({7,{5.5f,y},0,1.8f,.5f,1.8f,0,true});
 cargo(m,8.4f,21.5f,floor);cargo(m,8.4f,14.6f,floor,false);
 wallSign(m,10.055f,20.5f,floor+1.8f,"STORES","SPARES / CONSUMABLES",1.6f,kPi*.5f);

 // --- 13D PLANT BAY -----------------------------------------------------------
 m.fixtures.push_back({14,{17.6f,16.f},0,2.404f,2.645f,2.2f,0,true});
 for(float y:{16.5f,21.3f})m.fixtures.push_back({6,{21.3f,y},0,1.87f,.55f,.99f,0,true});
 m.props.push_back({1,{17.6f,20.6f},1.12f,2.f,0,{1.f,.31f},.12f});
 servicePipe(m,{17.6f,16.f},{17.6f,23.f},roof-.5f,.12f,3);
 wallSign(m,13.945f,20.5f,floor+1.8f,"PLANT BAY","COOLANT RETURN PUMPS",1.6f,-kPi*.5f);

 // --- Light ---------------------------------------------------------------
 const float lampZ=roof-.15f;
 for(float x:{6.f,18.f})m.lights.push_back(lamp({x,2.6f},lampZ,.9f,6.f,4300,.2f));
 m.lights.push_back(lamp({9.f,1.f},floor+2.f,1.1f,4.f,1300,-.3f,WorldLightMount::Wall,kPi*.5f));
 for(float y:{8.f,15.f,21.f})m.lights.push_back(lamp({12.f,y},lampZ,1.f,7.f,4300,.2f));
 for(float x:{5.f,19.f})m.lights.push_back(lamp({x,12.1f},lampZ,.6f,4.f,4300,.2f));
 m.lights.push_back(lamp({5.5f,8.f},lampZ,1.1f,6.f,3200));
 m.lights.push_back(lamp({18.5f,8.f},lampZ,1.f,6.f,6500,.1f));
 m.lights.push_back(lamp({5.5f,18.f},lampZ,.8f,6.f,4000));
 m.lights.push_back(lamp({18.5f,18.f},lampZ,1.f,6.f,2200));

 // --- Supplies ----------------------------------------------------------------
 using P=PickupKind;
 m.pickupSpawns={{{8.6f,6.6f},P::Health},{{3.f,21.6f},P::Ammo}};
 const Vec2 tools[]={{7.2f,10.3f},{7.9f,10.4f},{5.6f,11.8f},{12.6f,3.f}};
 for(int i=0;i<4;++i)m.clutterSpawns.push_back({i+2,tools[i],floor,i*.9f});

 zone(m,"13A PUMP WORKSHOP",1,5,10,11);
 zone(m,"13B ELECTRICAL",14,5,23,11);
 zone(m,"13C STORES",1,13,10,23);
 zone(m,"13D PLANT BAY",14,13,23,23);
 zone(m,"13 SERVICE GALLERY",0,0,24,24);
 return p;
}

std::shared_ptr<const Map> make(int level){
 auto p=std::make_shared<Map>();auto& m=*p;m.name=CampaignMapNames[level];m.definition=CampaignChunks[level];m.definition.musicCue=MusicCue::Freight;
 float base=level==11||level==12?-25.f:level>=13&&level<=19?-25.f:level==22?-25.f:level==27||level==28?-28.f:level==29?-25.f:m.definition.spawnHeight;
 for(int other=10;other<CampaignChunkCount;++other)if(other!=level&&CampaignChunks[other].residencyGroup==m.definition.residencyGroup){
  auto d=CampaignChunks[other].origin-m.definition.origin;
  m.openNorth|=d.x==0&&d.y==-24;m.openSouth|=d.x==0&&d.y==24;
  m.openWest|=d.x==-24&&d.y==0;m.openEast|=d.x==24&&d.y==0;
 }
 m.layers.push_back(floorLayer(m.name.c_str(),base,m.openNorth,m.openSouth));
 if(m.openWest)rectangle(m.layers[0],0,1,1,23,'.');
 if(m.openEast)rectangle(m.layers[0],23,1,24,23,'.');
 float entry=m.definition.spawnHeight;
 float exit=level==12?-19.f:level==13?-25.f:level==17?-19.f:level==18?-13.f:level==19?-7.f:level==22?-25.f:level==27?-28.f:level==28?-19.f:level==29?-25.f:entry;
 if(CampaignChunks[level].residencyGroup!=CampaignChunks[level-1].residencyGroup){m.doors.push_back({level==10?2.f:10.5f,level==10?5.f:13.5f,.5f,0,false,false,true,entry-base});rectangle(m.layers[0],level==10?2:10,0,level==10?5:14,1,'.');}
 if(level+1<CampaignChunkCount&&CampaignChunks[level].residencyGroup!=CampaignChunks[level+1].residencyGroup){m.doors.push_back({10.5f,13.5f,23.5f,0,false,true,false,exit-base});rectangle(m.layers[0],10,23,14,24,'.');}
 if(entry>base+.1f){if(level==19)deck(m,entry,20,9,24,16);else deck(m,entry,1,0,23,4);}
 if(exit>base+.1f)deck(m,exit,8,21,16,24);
 // Complete roof structure: columns land on the floor, crossbeams meet them,
 // and lights hang from crossbeams instead of arbitrary floating coordinates.
 float roof=m.definition.ceiling;
 for(float y:{4.f,12.f,20.f}){
  for(float x:{1.25f,22.6f})wall(m,x,y,x+.15f,y+.15f,base,roof,2);
  wall(m,1.25f,y,22.75f,y+.15f,roof-.25f,roof,2);
  if(roof-base<8.f)m.lights.push_back({{12,y+.07f},roof-.48f});
  else for(float x:{4.5f,12.f,19.5f})m.lights.push_back({{x,y+.07f},std::min(roof-.36f,base+5.55f)});
 }
 switch(level){
 case 10:
  // Mundane freight approach: offset doors and personnel/tool branches.
  wall(m,7,1,7.2f,9,-12,-8.5f,3);wall(m,7,11,7.2f,17,-12,-8.5f,3);
  wall(m,7,9,7.2f,11,-9.5f,-8.5f,3);
  wall(m,7,17,17,17.2f,-12,-8.5f,3);wall(m,19,17,23,17.2f,-12,-8.5f,3);
  wall(m,17,17,19,17.2f,-9.5f,-8.5f,3);
  for(float y:{4.f,7.f,13.f})m.fixtures.push_back({13,{1.6f,y},0,.9066f,.4956f,2.2f,-kPi*.5f,true});
  terminal(m,{4.5f,5},"FREIGHT / CLOCK-IN","EMPLOYEE TRANSIT / USE PERSONNEL ROUTE.","CARGO LIFTS: FOLLOW CLEARANCE MARKINGS.");
  terminal(m,{20.8f,13},"LOADING SCHEDULE","SHIFT 02 / RECEIVING 01-03 ACTIVE.","RESTRICTED CARGO: MANIFEST AUTHORIZATION.");
  wallSign(m,6.96f,12.7f,-10.25f,"AUX / PERSONNEL","FOLLOW WEST WALKWAY",1.8f,-kPi*.5f);
  cargo(m,20,6,-12);cargo(m,20,8,-12,false);
  handCart(m,17.5f,11,-12);
  board(m,19,3,-9.8f,"FREIGHT SAFETY","KEEP CLEAR / NO PASSENGERS ON LOADS",3.f);
  for(float yy:{18.f,21.f})m.fixtures.push_back({7,{2,yy},0,1.5f,.65f,2.f,-kPi*.5f,true});
  // Cross feeds leave both extraction housings and their grilles clear.
  for(float y:{3.f,7.f,14.5f})m.pipes.push_back({{1.1f,y},{6.8f,y},-8.85f,.08f});
  m.fixtures.push_back({19,{10.5f,5.f},0,1.8f,.65f,.9f,0,true});
  m.pickupSpawns.push_back({{11,5.7f},PickupKind::Ammo,-12});
  m.lights.clear();serviceCeiling(m,1,1,7,17,-8.7f);
  m.terminals[0].position={2.4f,5.5f};m.terminals[0].yaw=kPi*.5f;
  // Concrete door piers contain service risers; the headers physically join
  // the soffit, with clearance for the standing player at both openings.
  for(float y:{3.f,8.6f,12.f,16.f}){
   wall(m,1,y,1.65f,y+.45f,-12,-8.7f,15);
   wallLamp(m,1.66f,y+.225f,-10.1f,0,1.15f,6.f);
  }
  for(float y:{8.7f,16.7f})wall(m,1.65f,y,7.25f,y+.20f,-9.3f,-8.7f,18);
  for(float y:{4.5f,11.f})m.fixtures.push_back({20,{3.8f,y},3.08f,1.10f,1.10f,.184f,0,false});
  // The freight side holds a real service island and guarded spare stores.
  equipmentPad(m,19,11,22.5f,14.4f,-12);
  m.terminals[1].yaw=-kPi*.5f;m.terminals[1].z=.1f;
  m.fixtures.push_back({22,{19.5f,11.7f},.1f,.70f,.44f,1.16f,-kPi*.5f,true});
  m.fixtures.push_back({21,{21.4f,12.1f},.1f,1.68f,1.68f,1.2f,0,true});
  servicePipe(m,{22.55f,10.7f},{22.95f,10.7f},-9.42f,.085f,4);
  servicePipe(m,{22.55f,15.3f},{22.95f,15.3f},-9.42f,.085f,4);
  servicePipe(m,{22.55f,10.7f},{22.55f,15.3f},-9.42f,.085f,4);
  for(float y:{11.3f,13.f,14.7f})wall(m,22.39f,y,22.71f,y+.065f,-9.56f,-8.9f,2);
  wall(m,18.2f,10.7f,22.5f,15.3f,-9.05f,-8.9f,2);
  for(float x:{18.2f,22.3f})wall(m,x,15.1f,x+.18f,15.3f,-12,-8.9f,18);
  wallLamp(m,22.95f,13.3f,-10.f,kPi,1.4f,8.f);
  for(float y:{10.7f,15.3f})wall(m,22.8f,y-.15f,23,y+.15f,-9.65f,-9.2f,2);
  // A wall-fed cable run stays behind the operators and outside the packing
  // case, visibly connecting the control stand without passing through it.
  servicePipe(m,{19.87f,11.7f},{19.97f,11.7f},-11.25f,.035f,4);
  servicePipe(m,{19.97f,11.7f},{19.97f,14.1f},-11.25f,.035f,4);
  servicePipe(m,{19.97f,14.1f},{22.80f,14.1f},-11.25f,.035f,4);
  m.pipes.push_back({{22.80f,14.1f},{22.80f,14.1f},-11.25f,.035f,-9.42f,4});
  for(float z:{-11.f,-10.f})wall(m,22.77f,14.06f,23,14.14f,z,z+.065f,2);
  wallLamp(m,7.21f,12.5f,-10.f,0,1.1f,7.f);
  m.structures.push_back({1.5f,17.8f,6.5f,17.86f,-12,-9.3f,true,2});
  wallSign(m,4,17.76f,-10.f,"CONTROLLED STORES","PERSONNEL ONLY / RETURN TOOLS",2.6f);
  floorLine(m,8.1f,2,8.2f,16,-12);floorLine(m,17,2,17.1f,16,-12);
  m.definition.ambient=.33f;
  break;
 case 11:
  // Two dead main shafts remain visible around a fenced machinery route.
  deck(m,-12,1,4,6,24);deck(m,-12,18,4,23,24);deck(m,-12,6,20,18,24);
  m.structures.push_back({6,3.9f,18,4,-12,-10.9f,true,2});
  m.structures.push_back({5.9f,4,6,20,-12,-10.9f,true,2});
  m.structures.push_back({18,4,18.1f,20,-12,-10.9f,true,2});
  m.structures.push_back({6,19.9f,18,20,-12,-10.9f,true,2});
  board(m,3.5f,15,-9.5f,"01 / AUX BRAKE","WEST PERSONNEL WALKWAY",2.f);
  board(m,12,21,-9.5f,"AUXILIARY LIFT","LOCAL CONTROL / NEXT BAY",4.f);
  wall(m,6.5f,5,6.7f,17,-25,-9,2);wall(m,17.3f,5,17.5f,17,-25,-9,2);
  wall(m,7,5,11,11,-18.5f,-18.25f,2); // stalled freight car
  wall(m,13,6,17,6.2f,-25,-12,2);
  m.props.push_back({1,{3.5f,12},1.4f,1.f,0,{.45f,.65f},13});
  terminal(m,{3.5f,17.5f},"AUXILIARY / BRAKE RELEASE","MAIN SHAFTS OUT OF SERVICE.","01: RELEASE AUXILIARY BRAKE.",13,stateId("freight_brake"));
  m.pipes.push_back({{2,2},{2,22},-8.7f,.13f});
  wallLamp(m,1.01f,7,-10.1f,0,1.35f,8.f);wallLamp(m,22.99f,17,-10.1f,kPi,1.35f,8.f);
  servicePipe(m,{2.4f,2},{2.4f,22},-9.5f,.13f,3);
  servicePipe(m,{21.6f,2},{21.6f,22},-9.8f,.09f,4);
  break;
 case 12:
  deck(m,-12,1,4,23,9);m.stairs.push_back({10,8,14,10,-12.12f,-12,1,true,true});
  terminal(m,{6.5f,6},"AUXILIARY / LOCAL CONTROL","02: SELECT LOCAL AFTER BRAKE RELEASE.","INTERLOCK: BRAKE RELEASE REQUIRED.",13,stateId("freight_local"),stateId("freight_brake"));
  terminal(m,{12,6},"AUXILIARY / CALL PLATFORM","03: CALL PLATFORM TO BOARDING LEVEL.","INTERLOCK: LOCAL CONTROL REQUIRED.",13,stateId("freight_call"),stateId("freight_local"));
  terminal(m,{18,6},"AUXILIARY / MANUAL DOOR RELEASE","04: RELEASE THE CAGE DOOR AFTER ARRIVAL.","BOARD THE PLATFORM TO DESCEND.",13,stateId("freight_release"),stateId("freight_platform_arrived"));
  deck(m,-19,8,15,16,24);
  // A flush lower threshold leaves a real walk-off opening in the landing's
  // automatic guardrail. Previously the descent ended behind a sealed rail.
  m.stairs.push_back({10,14,14,16,-19,-19,1,true,true});
  board(m,12,6.8f,-9.6f,"02 LOCAL > 03 CALL > 04 RELEASE","WAIT FOR PLATFORM / THEN BOARD",6.f);
  m.cargoLift={9.5f,10,14.5f,15,-19,-12,1.f,stateId("freight_call"),stateId("freight_release"),stateId("freight_lift_mm"),stateId("freight_lift_down"),stateId("freight_platform_arrived"),stateId("freight_descended")};
  m.lights.clear();m.lights.push_back({{12,6},-9.1f});m.lights.push_back({{19,12},-9.1f});
  for(float x:{9.25f,14.7f})wall(m,x,9,x+.12f,16,-25,0,2);
 m.doors.back().requireState=stateId("freight_descended");
 machineryClock(m,"lift_distant_motion",12000);
 actor(m,"lift_distant_motion",ActorVisual::Huntsman,1.5f,{{0,{6,18},-16,0},{2000,{6,18},-16,0},{3600,{17,18},-16,0},{12000,{17,18},-16,0}});
 deck(m,-16,6,17,18,19);cargo(m,17,18,-16);
  {Door gate{9.5f,14.5f,10.f,0,false,false,false,13.f};gate.requireState=stateId("freight_release");m.doors.insert(m.doors.begin(),gate);}
  wallLamp(m,1.01f,6,-10.1f,0,1.35f,8.f);wallLamp(m,22.99f,6,-10.1f,kPi,1.35f,8.f);
  wallLamp(m,1.01f,19,-17.1f,0,1.3f,8.f);wallLamp(m,22.99f,19,-17.1f,kPi,1.3f,8.f);
  break;
 case 13:{
  m.definition.playerStart={7.6f,9.5f};m.definition.spawnYaw=.69f;m.definition.spawnPitch=-62.f;
  board(m,12,6.5f,-17.9f,"RECEIVING 01","UNLOADING / KEEP CLEAR",6.4f);
  m.timedSequences.push_back({stateId("receiving_worker_encounter_ms"),stateId("receiving_worker_encounter_complete"),6.4f,8,8.5f,10,-19.5f,-18.6f,18000,12500,0,0});
  m.timedSequences.back().sightActor=0;
  actor(m,"receiving_worker_encounter_ms",ActorVisual::Worker,1.7f,{
   {0,{16.2f,16},-25,0,1,0},{5200,{16.2f,16},-25,0,0,0},
   {6000,{16.2f,16},-25,0,0,0,1},
   {6200,{16.2f,16},-25,0,5,0,1},{6500,{16.2f,16},-25,0,5,1,1},
   {6501,{16.2f,16},-25,0,2,0,1},{7040,{16.2f,16},-25,0,2,1,1},
   {7100,{16.2f,16},-25,0,1,0,1},{8200,{16.2f,17.2f},-25,0,1,2,1},
   {8201,{16.2f,17.2f},-25,0,0,0,1},{9000,{16.2f,17.2f},-25,0,3,0,2},
   {9450,{16.4f,17.5f},-25,0,3,1,2},{9451,{16.4f,17.5f},-25,0,2,0,2},
   {9900,{16.4f,17.5f},-25,0,2,1,2},{9901,{16.4f,17.5f},-25,0,3,0,2},
   {10400,{17,18.1f},-25,0,3,1,2},{10401,{17,18.1f},-25,0,2,0,1},
   {10850,{17,18.1f},-25,0,2,1,1},{10851,{17,18.1f},-25,0,3,0,2},
   {11300,{17.3f,18.3f},-25,0,3,1,2},{11301,{17.3f,18.3f},-25,0,4,0},
   {12500,{17.8f,19},-25,0,4,1},{18000,{17.8f,19},-25,0,4,1}},false,true);
  auto& worker=m.actorTracks.back();worker.idleUntilMs=3000;worker.approachUntilMs=5200;
  worker.idleKeys={
   {0,{14.6f,15},-25,.785f,1,0},{2600,{15.6f,16},-25,.785f,1,4},
   {2601,{15.6f,16},-25,.785f,0,0},{3300,{15.6f,16},-25,-1.418f,1,0},
   {5900,{14.3f,16.2f},-25,-1.418f,1,4},{5901,{14.3f,16.2f},-25,-1.418f,0,0},
   {7000,{14.3f,16.2f},-25,2.897f,1,0},{9600,{14.6f,15},-25,2.897f,1,4},
   {9601,{14.6f,15},-25,2.897f,0,0},{10000,{14.6f,15},-25,.785f,0,0}};
  actor(m,"receiving_worker_encounter_ms",ActorVisual::Wasp,1.15f,{
   {0,{23,13},-23.4f,0},{5600,{23,13},-23.4f,0},{6200,{16,15},-23.7f,0},
   {6900,{16.2f,15.4f},-23.8f,0},{7040,{14.6f,16.4f},-24.9f,0,4},
   {9300,{14.6f,16.4f},-24.9f,0,4},{10100,{16.8f,17.5f},-23.7f,0},
   {10700,{16.8f,17.6f},-23.8f,0},{10900,{15.6f,19},-24.96f,0,4},{18000,{15.6f,19},-24.96f,0,4}});
  actor(m,"receiving_worker_encounter_ms",ActorVisual::Wasp,1.15f,{
   {0,{23,23},-23,0},{8800,{23,23},-23,0},{9300,{17.2f,17.5f},-23.7f,0},
   {9800,{17.3f,17.7f},-23.8f,0},{10500,{17.8f,18.3f},-23.9f,0},
   {11300,{17.7f,18.6f},-24.2f,0},{13000,{17.8f,19},-24.5f,0},
   {15500,{22,22},-22.7f,0},{18000,{25,23},-22.7f,0}});
  cue(m,"receiving_worker_encounter_ms",6100,Sound::WaspAttack,{16,15},"WORKER: GET BACK!",1.2f);
  cue(m,"receiving_worker_encounter_ms",6200,Sound::WorkerDying,{16.2f,16},"",1.5f,-23.81f);
  cue(m,"receiving_worker_encounter_ms",6900,Sound::PunchHit,{16.2f,16});
  cue(m,"receiving_worker_encounter_ms",7040,Sound::Metal1,{14.6f,16.4f},"",1,-24.96f);
  cue(m,"receiving_worker_encounter_ms",9150,Sound::Hurt,{16.2f,17.5f},"WORKER: NO! GET OFF ME!",0);
  cue(m,"receiving_worker_encounter_ms",9750,Sound::PunchHit,{17.2f,17.5f});
  cue(m,"receiving_worker_encounter_ms",10100,Sound::WaspAttack,{16.8f,17.5f});
  cue(m,"receiving_worker_encounter_ms",10700,Sound::PunchHit,{16.8f,17.6f});
  cue(m,"receiving_worker_encounter_ms",10900,Sound::WaspDeath,{15.6f,19});
  cue(m,"receiving_worker_encounter_ms",11400,Sound::Hurt,{17.8f,19},"WORKER: SOMEBODY...",0);
  // First receiving balcony, above unloading lanes. The descent folds back
  // along the west side instead of allowing a straight drop into the scene.
  deck(m,-19,1,4,8,10);m.stairs.push_back({3,9,6,18,-25,-19,32,true,false});
  for(float x:{8.f,18.f})container(m,x,7,-25,x<10);
  office(m,20.4f,20,-25,"RECEIVING / WORKER NOTE","THEY ARE STILL DOWN HERE. KEEP THE RADIO ON.","WAREHOUSE INTAKE: FOLLOW THE WEST SERVICE AISLE.");
  m.clutterSpawns.push_back({1,{17.5f,17.5f},-25});m.clutterSpawns.push_back({3,{19.7f,18.2f},-25});
  cargo(m,18.7f,17.5f,-25);cargo(m,17.4f,21.2f,-25,false);
  wall(m,18.9f,20,21.7f,20.12f,-25,-24.1f,10);
  m.clutterSpawns.push_back({1,{18.8f,19.4f},-25});m.clutterSpawns.push_back({2,{20.8f,19.5f},-25});
  m.pipes.push_back({{17.4f,19.2f},{18.1f,19.5f},-24.97f,.023f,-24.97f,2});
  terminal(m,{20.8f,21},"WORKER / HANDWRITTEN ROUTE","I HAVE BEEN HERE SINCE SHIFT CHANGE. TWO DAYS.","WEST SERVICE STAIRS / RADIO CALLS: STILL NO ANSWER.");
  m.lights.push_back({{16.5f,16.5f},-21.8f});
  m.pickupSpawns.push_back({{18.5f,20.5f},PickupKind::Health,-25});
  break;
 }
 case 14:
  for(float x:{2.f,9.f})container(m,x,5,-25,x!=9);
  m.fixtures.push_back({16,{19.1f,8},0,2.05f,4.9f,2.15f,0,true});
  cargo(m,16.5f,5.5f,-25);cargo(m,16.5f,9.5f,-25,false);
  board(m,19,12.5f,-22.3f,"RECEIVING 02","VEHICLE UNLOADING",4.f);
  container(m,17,15,-25,true);container(m,2,15,-25,false);
  for(float y:{4.f,13.f})wall(m,2,y,6.5f,y+1.2f,-25,-24.1f,3);
  terminal(m,{10.5f,18},"RESTRICTED MATERIAL INTAKE","DESTINATION: ENERGY RESEARCH.","TRANSFER AUTHORIZATION REQUIRED.");
  // Cleanup now owns this existing hold; avoid an offscreen creature fight.
  m.creatureSpawns.clear();
  receivingCleanup(m);
  m.pickupSpawns.push_back({{16,7},PickupKind::Ammo,-25});
  // Inspection islands divide unloading from the through personnel aisle.
  board(m,5,21,-22.5f,"INSPECTION HOLD","RESEARCH RECEIVING / SEALED LOAD",3.f);
  // Guarded inspection controls and sealed loads form a work island beside
  // the unloading lane; every console and load sits on its actual pad.
  equipmentPad(m,13,11,16.2f,14.8f,-25);
  m.fixtures.push_back({22,{15.4f,11.8f},.10f,.78f,.49f,1.3f,kPi,true});
  m.fixtures.push_back({21,{14.6f,13.6f},.10f,1.54f,1.54f,1.1f,0,true});
  for(float x:{13.f,16.f})for(float y:{11.f,14.6f})wall(m,x,y,x+.20f,y+.20f,-25,-20.85f,18);
  // A low guard contains the load, with an open operator frontage.
  wall(m,13,11.2f,13.10f,14.6f,-24.8f,-24.1f,12);
  wall(m,16.10f,11.2f,16.2f,14.6f,-24.8f,-24.1f,12);
  wall(m,13,11,16.2f,14.8f,-21,-20.85f,2);
  servicePipe(m,{13.7f,11},{13.7f,14.8f},-21.3f,.10f,3);
  wallLamp(m,13.21f,14.7f,-22.6f,0,1.5f,6.f);
  wallLamp(m,22.99f,8,-22.5f,kPi,1.5f,8.f);
  for(float yy:{4.f,15.f,21.f}){wall(m,22.1f,yy,23,yy+.7f,-25,-1,15);wallLamp(m,22.09f,yy+.35f,-22.6f,kPi,1.3f,7.f);}
  servicePipe(m,{22,1.5f},{22,22.5f},-20.6f,.16f,3);
  floorLine(m,11.8f,3,11.9f,21,-25);floorLine(m,20.8f,3,20.9f,21,-25);
  break;
 case 15:
  board(m,12,5.5f,-22.3f,"RECEIVING 03","WAREHOUSE INTAKE",5.f);
  cargo(m,6,10,-25);cargo(m,6,12,-25);cargo(m,20,17,-25,false);
  container(m,2,7,-25,true);container(m,18,8,-25,false);
  wall(m,6,6,18,6.25f,-4,-3.6f,2);wall(m,11.8f,2,12.2f,20,-3.5f,-3.1f,2);
  terminal(m,{5,18},"WAREHOUSE / TRACK DAMAGE","INTAKE SHUTTER CANNOT FULLY RETRACT.","CROUCH THROUGH THE CLEARANCE OPENING.");
  m.doors.back().maxOpen=.4f;
  m.pickupSpawns.push_back({{4,15},PickupKind::Ammo,-25});
  intakeBay(m);
  equipmentPad(m,18.4f,17.8f,22.5f,21,-25);
  m.fixtures.push_back({21,{21.3f,19.5f},.1f,1.68f,1.68f,1.2f,0,true});
  m.fixtures.push_back({22,{19.2f,19.2f},.1f,.75f,.47f,1.25f,kPi,true});
  serviceCeiling(m,18.4f,17.8f,23,21,-21.45f);
  wall(m,22.75f,17.8f,23,21,-25,-21.3f,15);
  wallLamp(m,22.74f,19,-22.7f,kPi,1.4f,7.f);
  wallSign(m,20.4f,22.74f,-22.8f,"INTAKE / LOCAL CONTROL","CHECK SEALS BEFORE TRANSFER",2.3f);
  break;
 case 16:case 17:case 18:case 19:
  board(m,19.35f,4.8f,-22.8f,level==16?"B / MAINTENANCE SPARES":level==17?"C / CHEMICALS":level==18?"D / RESTRICTED":"E / LONG TERM","STORAGE / KEEP AISLES CLEAR",2.6f);
  for(float x:{2.f,18.f})for(float y:{5.f,13.f})rack(m,x,y,-25,6);
  for(int tier=1;tier<=5;++tier){float z=-25+tier*3.f;
   deck(m,z,5,4,6,21);deck(m,z,17,4,18,21);
   // Two staggered crossovers, not a five-storey solid ceiling at the entry.
   if(tier==2||tier==4)deck(m,z,6,17,17,19);
  }
  deck(m,-19,1,18,6,21);if(level==19)deck(m,-13,17,9,24,16);
  if(level==17)climb(m,-25,-19);
  if(level==18)climb(m,-19,-13);
  if(level==18)deck(m,-13,0,9,11,24);
  if(level==19)climb(m,-13,-7);
  if(level==16){office(m,7.5f,14,-25,"WAREHOUSE / OPERATIONS","DO NOT SEND ANYTHING TO RECEIVING.","THEY ARE NOT ANSWERING.");m.fixtures.push_back({19,{7.5f,9.5f},0,2.4f,.9f,1.1f,kPi*.5f,true});
   // A staffed dispatch alcove gives the tall freight hall a human scale.
   // Its open frontage and sheltered screen face the actual personnel aisle.
   wall(m,6.3f,12,6.5f,16,-25,-21.8f,15);wall(m,6.3f,15.8f,9.5f,16,-25,-21.8f,15);
   serviceCeiling(m,6.3f,12,9.5f,16,-21.8f);
   m.terminals.back().position={7.5f,14.5f};m.terminals.back().yaw=kPi;
   m.fixtures.push_back({18,{7.5f,13.4f},0,.417f,.525f,.95f,kPi,true});
   wallSign(m,8.1f,15.75f,-22.7f,"WAREHOUSE / DISPATCH","LOCAL OPERATIONS",1.65f);
   m.definition.spawnYaw=.85f;
  }
  if(level==16){forklift(m,14.5f,6,-25);terminal(m,{21,20},"FORKLIFT / SHIFT REPORT","DRIVE MOTOR LEFT RUNNING. BATTERY EMPTY.","OPERATOR DID NOT CLOCK OUT.");
   m.fixtures.push_back({21,{14.7f,9.7f},0,1.96f,1.96f,1.4f,0,true});floorLine(m,13.2f,3.7f,16,3.82f,-25,13);
  }
  if(level==17){machineryClock(m,"warehouse_car",26000);actor(m,"warehouse_car",ActorVisual::Cargo,1.f,{{0,{12,11},-25,0},{4000,{12,11},-25,0},{12000,{12,11},-7,0},{16000,{12,11},-7,0},{26000,{12,11},-25,0}},true);m.actorTracks.back().platform=true;m.actorTracks.back().footprint={3,3};m.actorTracks.back().suspensionTop=1;
   for(float xx:{10.4f,13.4f})wall(m,xx,9.4f,xx+.2f,9.6f,-25,1,12);wall(m,10.4f,9.4f,13.6f,11.2f,.8f,1.1f,12);
   board(m,12,16,-21.8f,"WAREHOUSE CARGO LIFT","AUTOMATIC INVENTORY TRANSFER",3.f);}
  // Tall aisle-end labels remain readable from the opposite rack balcony.
  board(m,3.35f,4.8f,-17.7f,level==16?"A / INDUSTRIAL PARTS":level==17?"C / CHEMICAL INVENTORY":level==18?"D / RESTRICTED FREIGHT":"E / LONG TERM STORAGE","BLOCK DIRECTORY / UPPER WALK",2.6f);
  if(level==16){bench(m,7,19,-25);terminal(m,{7,21},"WAREHOUSE / BREAK CORNER","RADIO CHECK 02:14 / NO REPLY FROM RECEIVING.","DO NOT USE THE FREIGHT AISLES FOR EVACUATION.");}
  if(level==17){m.fixtures.push_back({14,{16.1f,18.2f},0,1.3f,1.35f,2.f,0,true});board(m,20,20,-22.7f,"CHEMICAL HOLD","SEALED STOCK / NO TRANSFER",2.5f);}
  if(level==18){container(m,2,17,-25,true,7);terminal(m,{3.5f,19},"RESTRICTED / SEALED MANIFEST","CONTAINMENT HARDWARE / SAMPLE HANDLING.","CONTENTS NOT FOR LOCAL INSPECTION.",6);}
  if(level==19){terminal(m,{12,22},"UPPER STORAGE / BREACH","RESTRICTED FREIGHT: MANIFEST CONTROL.","OFFICES ABOVE THE INTAKE END.",18);m.clutterSpawns.push_back({3,{13,22},-7});board(m,19,6,-4.8f,"E BLOCK","LONG TERM STORAGE",3.f);}
  if(level==19){
   // The upper office overlooks the actual warehouse across this seam.
   rectangle(m.layers.front(),2,23,10,24,'.');
   wall(m,2,23.85f,10,24,-25,-6.15f,3);wall(m,2,23.85f,10,24,-4.35f,2,3);
   for(float x:{2.f,5.9f,9.85f})wall(m,x,23.85f,x+.15f,24,-6.15f,-4.35f,2);
  }
  if(level==18){machineryClock(m,"upper_rack_noise",23000);cue(m,"upper_rack_noise",7000,Sound::JunkMetal,{19,14},"",.9f,-6.7f);cue(m,"upper_rack_noise",16000,Sound::SpiderCall,{19,14},"",.5f);for(float yy:{12.f,13.f,14.f})wall(m,18,yy,21,yy+.15f,-7,-6.85f,10);board(m,19.5f,16,-4.7f,"RACK ACCESS CLOSED","OVERHEAD STOCK DISPLACED",2.7f);}
  if(level==17||level==18)m.creatureSpawns={{CreatureKind::Huntsman,{20,11},entry},{CreatureKind::Wasp,{12,18},entry+2}};
  m.pickupSpawns.push_back({level==19?Vec2{21,12}:Vec2{12,3},PickupKind::Ammo,entry});
  m.lights.clear();for(float z:{-22.5f,-16.5f,-10.5f})for(float y:{6.f,14.f})for(float x:{6.6f,16.4f}){
   m.lights.push_back({{x,y},z});float anchor=x<10?5.3f:17.5f;
   wall(m,std::min(x-.5f,anchor),y-.1f,std::max(x+.5f,anchor+.1f),y+.1f,z+.17f,z+.27f,12);
   wall(m,anchor,y-.1f,anchor+.1f,y+.1f,z+.17f,z+.65f,12);
  }
  m.definition.ambient=.40f;
  for(float x:{7.f,16.f})floorLine(m,x,4,x+.10f,22,-25);
  warehouseServices(m,-25,roof);
  if(level==16)wallLamp(m,6.8f,15.79f,-22.9f,-kPi*.5f,1.2f,5.f);
  for(auto& light:m.lights)if(light.mount==WorldLightMount::Ceiling){light.intensity=1.25f;light.range=8.f;}
  // Fewer opaque wooden blocks and some pale freight cases separate loads
  // from the rack silhouette without adding a new storage aisle.
  for(auto& fixture:m.fixtures)if(fixture.model==17&&fixture.base>3.f&&int(fixture.base/3.f)%3==1){fixture.model=21;fixture.height=fixture.width/1.401f;fixture.depth=fixture.width;}
  m.definition.ambient=.34f;
  break;
 case 20:case 21:
  m.definition.floorMaterial=FloorMaterial::OfficeCarpet;
  m.visibleResidencyGroups={2};
  if(level==20){
   rectangle(m.layers.front(),2,0,10,1,'.');
   wall(m,2,0,10,.16f,-7,-6.15f,3);wall(m,2,0,10,.16f,-4.35f,-3.4f,3);
   for(float x:{2.f,5.9f,9.85f})wall(m,x,0,x+.15f,.16f,-6.15f,-4.35f,2);
   wall(m,2.15f,.045f,9.85f,.065f,-6.15f,-4.35f,17);
  }
  board(m,12,19.5f,-4.9f,"MANIFEST CONTROL","FREIGHT RECORDS / ROUTING",3.5f);
  for(float y:{6.f,12.f,17.f})m.fixtures.push_back({7,{1.5f,y},0,1.5f,.45f,1.8f,-kPi*.5f,true});
  office(m,5.5f,7,-7,"MANIFEST / ORDINARY FREIGHT","BEARINGS / FILTERS / FOOD / MEDICAL SUPPLIES.","SPECIALIZED CONTAINMENT HARDWARE: RESEARCH RECEIVING.",0,true);
  office(m,18,8,-7,"TRAIN 27 / ARRIVAL RECORD","PASSENGERS: 0 / CARGO: NONE / PLATFORM 3.","MANUAL ANNOTATION: THIS IS WRONG.",0,true);
  observationWindow(m,10,5,18,-7);
  // Records bays have an open frontage and carpeted circulation, leaving the
  // central personnel route and the routing control's approach unobstructed.
  for(float y:{6.f,12.f}){
   wall(m,14.5f,y,22.8f,y+.12f,-7,-6.15f,3);
   wall(m,14.5f,y,22.8f,y+.12f,-4.4f,-3.4f,3);
   for(float x:{14.5f,18.5f,22.7f})wall(m,x,y,x+.08f,y+.12f,-6.15f,-4.4f,2);
   wall(m,14.6f,y+.045f,22.7f,y+.065f,-6.15f,-4.4f,17);
  }
  bench(m,6,19,-7);
  m.fixtures.push_back({19,{4.5f,12},0,1.6f,.84f,.9f,kPi*.5f,true});
  wall(m,4.25f,11.9f,4.75f,12.4f,-6.1f,-5.86f,2);wall(m,4.32f,12,4.68f,12.25f,-5.86f,-5.82f,15);
  m.clutterSpawns.push_back({0,{4.6f,12.4f},-6.1f});
  recordsDesk(m,6,11,-7);recordsDesk(m,19,15,-7);
  // Low filing bays stay out of the central circulation and control approach.
  for(float y:{10.f,15.5f})m.fixtures.push_back({7,{2,y},0,1.35f,.5f,1.15f,-kPi*.5f,true});
  terminal(m,{6,16},"MANIFEST / MISSED CALLS","RECEIVING: 02:14 / 02:17 / 02:21 / NO ANSWER.","EMPLOYEE TRANSIT LOG: PLATFORM 3.");
  if(level==21){office(m,18,18,-7,"MANIFEST / ROUTING SHUTTERS","TRANSFER YARD: ROUTING AUTHORIZATION.","E / RELEASE THE DOWNSTREAM SHUTTERS.",stateId("freight_routing"),true);m.doors.back().requireState=stateId("freight_routing");}
  m.pickupSpawns.push_back({{6,18},PickupKind::Health,-7});
  // Human administration has occupied desks, legroom, and a window-facing
  // counter. Incoming players see the operator face of the near terminals.
  for(Vec2 p:{Vec2{5.5f,7},Vec2{18,8},Vec2{6,16}})m.fixtures.push_back({18,{p.x,p.y-.85f},0,.417f,.525f,.95f,kPi,true});
  recordsDesk(m,19,18.5f,-7);recordsDesk(m,6,14,-7);
  wall(m,2.25f,1.15f,9.7f,1.65f,-6.2f,-6.12f,20);
  for(float x:{2.35f,9.45f})wall(m,x,1.2f,x+.12f,1.6f,-7,-6.2f,2);
  if(level==20){m.fixtures.push_back({11,{4.5f,1.4f},.88f,.39f,.38f,.45f,0,false});m.fixtures.push_back({18,{4.5f,2.35f},0,.417f,.525f,.95f,0,true});}
  m.lights.clear();
  for(float y:{5.f,11.f,17.f}){
   wallLamp(m,1.01f,y==11.f?12.f:y,-5.25f,0,.85f,7.f);wallLamp(m,22.99f,y,-5.25f,kPi,.85f,7.f);
   m.lights.push_back({{12,y},-3.88f,WorldLightMount::Ceiling,0,.8f,7.f});
  }
  m.lights.push_back({{6,11},-4.7f,WorldLightMount::Ceiling,0,.55f,3.f});
  // Wall-fastened record shelves carry distinct folders above the low files.
  for(float z:{-5.4f,-4.95f,-4.5f}){
   wall(m,1.02f,9.7f,1.47f,11.3f,z,z+.045f,2);
   for(int i=0;i<7;++i){float y=9.8f+i*.20f;wall(m,1.14f,y,1.38f,y+.15f,z+.045f,z+.37f,i%3==0?19:i%3==1?20:18);}
  }
  for(float y:{9.73f,11.2f})wall(m,1.02f,y,1.10f,y+.07f,-5.6f,-4.4f,2);
  m.definition.ambient=.42f;
  break;
 case 22:
  machineryClock(m,"transfer_belts",14000);
  for(float y:{7.f,15.f})actor(m,"transfer_belts",ActorVisual::Cargo,.7f,{{0,{2,y+.35f},-4.7f,0},{4000,{8,y+.35f},-4.7f,0},{6500,{8,y+.35f},-4.7f,0},{12000,{21,y+.35f},-4.7f,0},{14000,{2,y+.35f},-4.7f,0}},true);
  cue(m,"transfer_belts",100,Sound::Machine,{8,7},"",.35f);
  // Three stair flights carry the player down alongside live conveyor spines.
  m.stairs={{3,4,6,13,-13,-7,32,true,false},{8,13,17,16,-19,-13,32,false,false},{17,18,20,23,-25,-19,32,true,false}};
  deck(m,-13,3,13,11,16);deck(m,-19,17,13,20,20);
  for(float y:{7.f,15.f})wall(m,1,y,23,y+.8f,-5,-4.75f,2);
  // Suspended transfer spines bear on actual columns and cross-members.
  // Service lamps follow the three landings rather than the distant roof.
  for(float y:{7.f,15.f}){
   for(float x:{1.3f,21.8f})wall(m,x,y-.14f,x+.22f,y+.96f,-25,-4.65f,12);
   for(float x=1.5f;x<22;x+=3.f){wall(m,x,y-.05f,x+.18f,y+.85f,-5.45f,-5.f,18);
    m.pipes.push_back({{x,y+.40f},{x+2.6f,y+.40f},-5.42f,.045f,-5.02f,2});
   }
  }
  wallLamp(m,1.01f,5,-4.8f,0,1.1f,7.f);wallLamp(m,1.01f,14,-10.8f,0,1.15f,7.f);
  wallLamp(m,22.99f,16,-16.8f,kPi,1.15f,7.f);wallLamp(m,22.99f,21,-22.8f,kPi,1.1f,7.f);
  servicePipe(m,{1.65f,1.5f},{1.65f,22.5f},-3.1f,.10f,3);
  terminal(m,{22,21},"TRANSFER / CONVEYOR ROUTE","SORTING FLOOR BELOW / FOLLOW AMBER CLEARANCE.","DEPOT AND PLATFORMS: SOUTH RAIL SPUR.");
  break;
 case 23:
  machineryClock(m,"sorting_crane",18000);
  actor(m,"sorting_crane",ActorVisual::Cargo,1.2f,{{0,{13,8},-19,0},{4000,{13,8},-19,0},{10000,{13,17},-19,.35f},{14000,{13,17},-19,.35f},{18000,{13,8},-19,0}},true);
  m.actorTracks.back().suspensionTop=-17.5f;
  wall(m,12.8f,5,13.2f,21,-17.5f,-17.2f,12);
  for(float y:{5.f,21.f})wall(m,12.8f,y,13.2f,y+.3f,-25,-17.2f,12);
  cue(m,"sorting_crane",5000,Sound::LiftCreak,{13,12},"",.5f);
  board(m,13,20,-22.3f,"SORTING","DEPOT / PLATFORM 3",3.f);
  cargo(m,3,12,-25);cargo(m,5,12,-25,false);
  m.compactors.push_back({7,7,11,15,-25,-22.4f,8,stateId("sorting_gate_stop")});
  terminal(m,{5,6},"SORTING / CARGO GATE","HYDRAULIC PUSHER: ACTIVE.","E / TOGGLE GATE ISOLATION.",0,stateId("sorting_gate_stop"));m.terminals.back().toggleState=true;
  container(m,17,7,-25,true);m.creatureSpawns={{CreatureKind::Huntsman,{9,17},-25},{CreatureKind::Wasp,{16,13},-23}};
  for(float x:{6.75f,11.05f}){
   wall(m,x,6.75f,x+.20f,15.3f,-25,-24.75f,18);
   for(float y:{6.75f,15.1f})wall(m,x,y,x+.20f,y+.20f,-25,-21.7f,12);
  }
  wall(m,6.75f,15.1f,11.25f,15.3f,-21.9f,-21.7f,18);
  wallLamp(m,6.74f,6.85f,-22.8f,kPi,1.1f,6.f);wallLamp(m,11.26f,6.85f,-22.8f,0,1.1f,6.f);
  wallLamp(m,22.99f,13,-22.8f,kPi,1.1f,7.f);wallLamp(m,1.01f,12,-22.8f,0,1.1f,7.f);
  servicePipe(m,{1.6f,1.5f},{1.6f,22.5f},-20.8f,.09f,4);
  m.pickupSpawns.push_back({{19,10},PickupKind::Ammo,-25});
  break;
 case 24:
  machineryClock(m,"yard_cradle",20000);
  actor(m,"yard_cradle",ActorVisual::Cargo,1.f,{{0,{4,16},-24.7f,0},{3000,{4,16},-24.7f,0},{10000,{11,16},-24.7f,0},{13000,{11,16},-24.7f,0},{20000,{4,16},-24.7f,0}},true);
  m.actorTracks.back().platform=true;m.actorTracks.back().footprint={2.2f,2.2f};
  m.actorTracks.back().suspensionTop=-17.7f;
  wall(m,2.8f,15.9f,12.2f,16.1f,-18,-17.7f,12);
  for(float x:{2.8f,12.f})wall(m,x,14.8f,x+.2f,15,-25,-17.7f,12);
  for(float x:{2.8f,12.f})wall(m,x,14.8f,x+.2f,16.1f,-18,-17.7f,12);
  board(m,5,18.5f,-22.5f,"MAINTENANCE CRADLE","AUTOMATIC TRANSFER / STAND CLEAR",2.4f);
  for(float x:{7.f,17.f})rail(m,x,1,23,-25);
  container(m,2,6,-25,false);office(m,20,18,-25,"TRANSFER / SIGNAL ROUTING","DEPOT / PLATFORMS / DEEP FREIGHT.","PLATFORM 3: SERVICE ACCESS ONLY.");
  wall(m,8,6,17,6.4f,-9,-8.7f,2);
  // A scale and sheltered signal desk establish the rail transfer function.
  wall(m,14.5f,7,19.5f,12,-25,-24.88f,2);
  cargo(m,17,9.5f,-24.88f);
  board(m,6,18,-22.5f,"DEEP FREIGHT","WEIGHING / DEPOT / PLATFORM 3",3.f);
  wallLamp(m,1.01f,14,-22.8f,0,1.1f,7.f);wallLamp(m,22.99f,18,-22.8f,kPi,1.2f,7.f);
  servicePipe(m,{1.65f,1.5f},{1.65f,22.5f},-20.8f,.1f,3);
  break;
 case 25:case 26:
  board(m,12,8,-22.2f,"PLATFORM 3","TRAIN 27 / ARRIVED 02:13",4.2f);
  if(level==26)board(m,12,22,-22.3f,"DEPOT ACCESS","EMPLOYEE TRANSIT",3.f);
  rail(m,7,1,23,-25);rail(m,18,1,23,-25);
  for(float y:{10.f,18.f})bench(m,14.5f,y,-25);
  // Trackside fences separate transit from freight, with gaps at entry/exit.
  for(float x:{9.f,16.f})m.structures.push_back({x,4,x+.07f,21,-25,-23.95f,true,2});
  m.fixtures.push_back({8,{22.7f,12},1.f,.7f,.15f,1.f,-kPi*.5f,false});
  if(level==25)terminal(m,{12,3},"PLATFORM 3 / DEPARTURES","TRAIN 27 / ARRIVED 02:13.","NO DEPARTURE RECORDED / TRACK CLEAR.");
  if(level==25){m.fixtures.push_back({8,{1.25f,14},1.1f,.55f,.12f,.8f,-kPi*.5f,false});terminal(m,{2,14},"PLATFORM / EMERGENCY PHONE","DISPATCH LINE OPEN / NO OPERATOR RESPONSE.","HOLD THE PLATFORM. DO NOT ENTER THE TRACK.");board(m,20,13,-22.5f,"DEPTHWORKS EMPLOYEE TRANSIT","YOUR SHIFT / YOUR CONNECTION",3.f);}
  else {terminal(m,{12,18},"PLATFORM / SECURITY RECORD","02:12:51 ... 02:19:04 / BLOCK MISSING.","EMPLOYEE TRANSIT: ROLLING STOCK DEPOT.");for(int i=0;i<6;++i)m.clutterSpawns.push_back({i%4,{11.f+float(i%3),19.f+float(i/3)},-25});}
  m.lights.clear();for(float y:{3.f,12.f,21.f})m.lights.push_back({{12,y},-18.25f});
  for(float x:{9.15f,15.75f})floorLine(m,x,2,x+.10f,22,-25);
  for(float y:{5.f,15.f}){
   for(float x:{1.f,22.4f})wall(m,x,y,x+.6f,y+.8f,-25,-18,15);
   wallLamp(m,1.61f,y+.4f,-22.8f,0,.85f,7.f);wallLamp(m,22.39f,y+.4f,-22.8f,kPi,.85f,7.f);
  }
  break;
 case 27:
  board(m,12,6,-22.2f,"ROLLING STOCK","ENGINE 04 / MAINTENANCE",3.f);
  for(float x:{7.f,17.f})rail(m,x,1,23,-25);
  deck(m,-25,1,4,11,10);deck(m,-25,13,4,23,24);
  m.stairs.push_back({3,9,6,18,-28,-25,16,true,false});
  locomotive(m,17,8,-25);
  m.lights.push_back({{14.5f,8},-22.f});
  m.lights.push_back({{17,11},-20.5f});
  for(float x:{15.2f,18.3f})wall(m,x,15.3f,x+.5f,16.3f,-25,-23.5f,3);
  terminal(m,{18,18},"DEPOT / MAINTENANCE LOG","ENGINE 04: DRIVER ATTEMPTED EVACUATION.","VEHICLE STOPPED AT THE BARRICADE.",3);
  break;
 case 28:
  rail(m,7,1,23,-25);rail(m,17,1,23,-25);
  // A suspended chassis straddles the reachable inspection pit.
  locomotive(m,17,5,-25,true);
  m.lights.push_back({{14.5f,7},-23.f});
  m.lights.push_back({{17,8},-20.5f});
  for(float x:{15.65f,18.13f})for(float y:{6.f,10.5f}){
   wall(m,x-.22f,y-.22f,x+.22f,y+.22f,-28,-27.6f,8);
   wall(m,x-.07f,y-.07f,x+.07f,y+.07f,-27.6f,-24.04f,2);
  }
  climb(m,-28,-19);deck(m,-19,8,21,16,24);
  m.pickupSpawns.push_back({{12,3},PickupKind::Ammo,-28});
  m.fixtures.push_back({12,{3,20},0,1.8f,1.4f,1.6f,0,true});
  board(m,20,18,-25.4f,"INSPECTION PIT","JACKS ENGAGED / DO NOT LOWER",2.6f);
  // Workshop wings sit at track height; the entry and chassis pit remain
  // open below them so the train, trench and floor levels share one view.
  deck(m,-25,1,4,11,22);deck(m,-25,19,4,23,22);deck(m,-25,11,14,19,22);
  for(float x:{14.8f,18.9f}){
   wall(m,x,x<18?10.f:4.f,x+.16f,14,-28,-25,15);
   floorLine(m,x,4,x+.16f,14,-25,13);
  }
  for(float x:{16.23f,17.65f})wall(m,x,4,x+.12f,14,-25.45f,-24.97f,12);
  for(float y:{4.f,13.7f})wall(m,15,y,19,y+.18f,-25.5f,-25.05f,12);
  for(float y:{6.f,10.f,13.f}){
   wall(m,14.8f,y-.12f,14.96f,y+.12f,-28,-26.03f,12);
   wall(m,14.8f,y-.12f,15.8f,y+.12f,-26.13f,-26.03f,12);
   wallLamp(m,14.97f,y,-26.3f,0,1.25f,5.f);
  }
  for(float y:{6.f,10.f})wallLamp(m,18.89f,y,-26.3f,kPi,1.25f,5.f);
  deck(m,-25,11,4,15,14);
  // Continuous masonry cheeks define a service trench, rather than leaving
  // the maintenance vehicle over an entire open lower storey.
  wall(m,14.8f,4,14.96f,14,-28,-25,15);
  servicePipe(m,{15.08f,4},{15.08f,13.5f},-27.2f,.055f,4);
  servicePipe(m,{18.77f,4},{18.77f,13.5f},-27.2f,.055f,3);
  m.definition.ambient=.40f;
  // Track-height workshop equipment gives the pit a visible maintenance
  // purpose, while its entry, jacks and stairs retain their clearance.
  for(float y:{5.5f,9.5f}){
   m.fixtures.push_back({19,{21,y},3.f,2.4f,.8f,.9f,0,true});
   wall(m,20.1f,y-.22f,21.9f,y+.22f,-24.1f,-23.98f,12);
   for(float x:{20.35f,21.5f})m.pipes.push_back({{x,y-.16f},{x,y+.16f},-23.82f,.16f,-23.82f,1});
  }
  for(float y:{5.5f,9.f,12.5f}){
   m.fixtures.push_back({7,{2,y},3.f,1.4f,.5f,1.8f,-kPi*.5f,true});
   floorLine(m,1.3f,y-1,3.f,y-.9f,-25,13);
  }
  board(m,21,13,-22.8f,"ENGINE 04 / TOOL BAY","BOGIE SERVICE / RETURN TOOLS",2.8f);
  m.fixtures.push_back({22,{21,16},3.f,.718f,.453f,1.2f,-kPi*.5f,true});
  m.fixtures.push_back({21,{21,19},3.f,1.68f,1.68f,1.2f,0,true});
  wallLamp(m,22.99f,6,-22.8f,kPi,1.3f,7.f);wallLamp(m,22.99f,16,-22.8f,kPi,1.3f,7.f);
  break;
 case 29:
  machineryClock(m,"depot_roof_movement",24000);actor(m,"depot_roof_movement",ActorVisual::Huntsman,1.5f,{{0,{19,6},-16,0},{6000,{19,6},-16,0},{8500,{19,14},-16,kPi*.5f},{11000,{19,17},-19,kPi*.5f},{24000,{19,17},-19,kPi*.5f}});
  wall(m,18.6f,5.7f,19.4f,14.5f,-16.2f,-16,12);for(float yy:{6.f,14.f})wall(m,19.3f,yy,19.5f,yy+.15f,-25,-16,12);
  cue(m,"depot_roof_movement",7000,Sound::JunkMetal,{19,12},"",.8f);
  deck(m,-19,1,4,7,22);deck(m,-19,17,4,23,22);
  m.stairs.push_back({3,13,6,22,-25,-19,32,true,false});
  for(float x:{7.f,17.f})rail(m,x,1,23,-25);
  locomotive(m,17,4,-25);
  m.lights.push_back({{17,7},-20.5f});
  m.creatureSpawns={{CreatureKind::Huntsman,{20,15},-19}};
  terminal(m,{12,22},"DEEP CARGO ROUTE","POWER DISTRIBUTION / RESTRICTED INDUSTRIAL ACCESS.","BEYOND THIS POINT: NO EMPLOYEE TRANSIT.");
  break;
 case 30:case 31:
  board(m,5,11.5f,-22.7f,level==30?"TUNNEL 4A":"DEEP JUNCTION","POWER DISTRIBUTION",2.8f);
  for(float x:{7.f,17.f})rail(m,x,1,23,-25);
  wall(m,11,1,11.2f,10,-25,-19,3);wall(m,11,14,11.2f,23,-25,-19,3);
  for(float y:{3.f,7.f,11.f,15.f,19.f})m.pipes.push_back({{1.2f,y},{22.8f,y},-19.4f,.06f});
  if(level==30){m.waterVolumes.push_back({14,5,20,12,-25.35f,-25.1f});office(m,4,17,-25,"SIGNAL ROOM / TUNNEL 4A","MAIN LINE CLOSED / PARALLEL BORE AVAILABLE.","POWER DISTRIBUTION: DEEP JUNCTION.");
   // Occlusion and a west-side bypass break the bore's repeated straight run.
   // Roof damage stays over the freight lane, with a real drain/service edge.
   wall(m,18,13,22.8f,14.5f,-25,-23.3f,15);
   board(m,5,12,-22.6f,"SERVICE BYPASS","MAIN BORE / COLLAPSE AHEAD",2.8f);
   m.fixtures.push_back({13,{2,6},0,.9f,.5f,2.2f,-kPi*.5f,true});
  }
  else {wall(m,3,7,9,9,-25,-23.3f,3);wall(m,15,13,21,15,-25,-23.2f,3);
   // A breached signal alcove anchors the forced service door to actual
   // architecture, leaving the centre partition crossing at y10..14 clear.
   wall(m,2.1f,9.3f,2.25f,12.1f,-25,-22.2f,3);
   wall(m,6.1f,9.3f,6.25f,12.1f,-25,-22.2f,3);
   wall(m,2.1f,9.3f,6.25f,9.45f,-25,-22.2f,3);
   wall(m,2.1f,11.9f,4,12.1f,-25,-22.2f,3);wall(m,6,11.9f,6.25f,12.1f,-25,-22.2f,3);
   wall(m,4,11.9f,6,12.1f,-22.6f,-22.2f,2);
   wall(m,2.1f,9.3f,6.25f,12.1f,-22.2f,-22.05f,2);
   terminal(m,{3.25f,10.55f},"SIGNAL / BLOCK 4A","BOTH TRACK CIRCUITS OCCUPIED / NO VEHICLE LOGGED.","SERVICE OVERRIDE REMAINS ISOLATED.");
   m.terminals.back().yaw=0;
   terminal(m,{12,21},"DEEP JUNCTION / SUBSTATION","FREIGHT MAIN LINE CONTINUES EAST.","POWER DISTRIBUTION / AUTHORIZED TECHNICIANS ONLY.");
   for(float xx:{14.5f,15.7f,17.f})wall(m,xx,16.5f,xx+.75f,18.8f,-24.997f,-24.992f,16);
   for(float xx:{11.3f,12.3f,13.3f})wall(m,xx,17.5f,xx+.22f,20,-24.99f,-24.98f,12);
   Door forced{4,6,12,0,true,false,false};forced.swinging=true;forced.open=.8f;forced.opening=true;m.doors.push_back(forced);
   m.creatureSpawns={{CreatureKind::Huntsman,{18,19},-25}};
  }
  m.lights.clear();for(float y:{2.f,10.f,18.f})m.lights.push_back({{3,y},-19.45f});
  if(level==31)m.lights.push_back({{3.4f,10.55f},-22.65f});
  for(float y:{5.f,11.f,17.f,22.f}){
   for(float x:{1.2f,22.5f})wall(m,x,y,x+.25f,y+.2f,-25,-19.3f,12);
   wall(m,1.2f,y,22.75f,y+.2f,-19.5f,-19.2f,12);
   m.fixtures.push_back({8,{22.7f,y+.6f},1.1f,.6f,.12f,.8f,-kPi*.5f,false});
  }
  for(float y:{7.f,15.f})m.pipes.push_back({{1.5f,y},{10.7f,y},-22.4f,.07f});
  // Continuous wall-mounted cable bank, with collars fastened to the wall.
  for(float z:{-23.0f,-22.75f,-22.5f})m.pipes.push_back({{22.15f,2},{22.15f,22},z,.045f,z,2});
  for(float y:{3.f,7.f,11.f,15.f,19.f,22.f}){
   wall(m,22.10f,y,23,y+.07f,-23.18f,-23.10f,2);
   wall(m,22.10f,y,22.20f,y+.07f,-23.18f,-22.35f,2);
  }
  // Drain grilles distinguish the rail bed from the service walkway.
  floorLine(m,20.85f,2,21.4f,22,-25,8);
  for(float y=2;y<22;y+=.5f)wall(m,20.85f,y,21.4f,y+.045f,-24.982f,-24.972f,2);
  m.lights.push_back({{17,14},-21.8f});floorLine(m,10.2f,1,10.3f,22,-25);
  for(auto it=m.lights.begin();it!=m.lights.end();)if(it->position.x==3&&it->mount==WorldLightMount::Ceiling)it=m.lights.erase(it);else ++it;
  for(float y:{3.f,11.f,19.f})wallLamp(m,1.01f,y,-22.85f,0,1.25f,6.f);
  servicePipe(m,{1.65f,1.5f},{1.65f,22.5f},-20.3f,.10f,3);
  servicePipe(m,{2.f,1.5f},{2.f,22.5f},-20.55f,.075f,4);
  // The flood has a concrete edge and a dark, recessed drainage bed.
  if(level==30){
   wall(m,13.8f,5,14,12,-25.35f,-24.92f,15);wall(m,20,5,20.2f,12,-25.35f,-24.92f,15);
   m.waterVolumes.front().surface=-25.18f;
   // Split the failed support into a recognisable collapsed roof assembly,
   // leaving the west service bypass free of an arbitrary full-width block.
   for(auto& s:m.structures)if(s.x1==18&&s.y1==13&&s.x2==22.8f){s.x2=20.2f;s.top=-24.1f;}
   wall(m,20.3f,13.5f,22.8f,14.8f,-25,-24.3f,15);
   m.pipes.push_back({{19,13.2f},{22.4f,14.3f},-24.25f,.14f,-21.3f,3});
   m.pipes.push_back({{20.2f,13.8f},{21.1f,14.3f},-24.5f,.11f,-22.5f,4});
   wall(m,22.35f,13,22.8f,14,-23,-19.1f,2);
  }
  break;
 }
 // Steel flights have open space beneath their individual treads. Stringers
 // and side rails connect each landing instead of hiding a room-sized wedge.
 for(auto& stair:m.stairs)if(stair.steps>1){
  stair.openUnderside=true;stair.treadThickness=.12f;stair.sideRails=true;
  float z1=stair.ascending?stair.bottom:stair.top,z2=stair.ascending?stair.top:stair.bottom;
  for(bool right:{false,true}){
   Vec2 a=stair.alongY?Vec2{right?stair.x2-.12f:stair.x1+.12f,stair.y1}:Vec2{stair.x1,right?stair.y2-.12f:stair.y1+.12f};
   Vec2 b=stair.alongY?Vec2{a.x,stair.y2}:Vec2{stair.x2,a.y};
   m.pipes.push_back({a,b,z1-.08f,.085f,z2-.08f,2});
  }
 }
 // Floors at each stair's destination must leave the actual stairwell open.
 // A slab drawn across the flight can otherwise hide and block the descent.
 for(const auto& stair:m.stairs)for(auto& layer:m.layers)if(layer.thickness>0&&layer.elevation>=stair.bottom-.02f&&layer.elevation<=stair.top+.02f)
  rectangle(layer,int(stair.x1),int(stair.y1),int(stair.x2),int(stair.y2),'.');
 for(auto& d:m.doors)d.sign=d.entry?level-1:level+1;
 if(level==17)m.creatureSpawns.push_back({CreatureKind::Mutant,{12,10},-25});
 if(level==27)m.creatureSpawns.push_back({CreatureKind::Mutant,{20,19},-25});
 return p;
}
}
std::shared_ptr<const AuthoredMapData> campaignMap(int level){
 static const auto maps=[](){std::array<std::shared_ptr<const Map>,CampaignChunkCount> result;
  result[0]=makeFoundry();result[1]=makePressureWorks();result[2]=makeTurbineGantry();result[3]=makeReactorComplex();result[4]=makeServiceGallery();
  for(int i=10;i<CampaignChunkCount;++i)result[i]=make(i);return result;}();
 return maps.at(size_t(level));
}
}

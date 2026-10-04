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
  wall(m,x,y-.07f,x+2.76f,y+.07f,z+.12f,z+.30f,12);
  wall(m,x,y+3.86f,x+2.76f,y+4.02f,z+.12f,z+.30f,12);
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
void actor(Map& m,const char* timer,ActorVisual visual,float scale,std::initializer_list<ActorKey> keys,bool loop=false,bool tool=false){m.actorTracks.push_back({stateId(timer),visual,scale,loop,tool,keys});}
void cue(Map& m,const char* timer,int time,Sound sound,Vec2 position,const char* caption="",float gain=1){m.sequenceCues.push_back({stateId(timer),time,sound,position,gain,1,caption});}
void machineryClock(Map& m,const char* timer,int period){TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_cycled");s.durationMs=period;s.finishAtMs=period;s.soundIntervalMs=0;s.loop=true;m.timedSequences.push_back(s);}
void handCart(Map& m,float x,float y,float floor){
 wall(m,x-.42f,y-.6f,x+.42f,y+.6f,floor+.2f,floor+.29f,12);
 for(float xx:{x-.47f,x+.37f})for(float yy:{y-.4f,y+.4f})m.pipes.push_back({{xx,yy},{xx+.1f,yy},floor+.14f,.14f,floor+.14f,1});
 for(float xx:{x-.38f,x+.34f})wall(m,xx,y+.56f,xx+.04f,y+.6f,floor+.27f,floor+1.02f,12);
 wall(m,x-.38f,y+.56f,x+.38f,y+.6f,floor+.97f,floor+1.02f,12);
}
void forklift(Map& m,float x,float y,float floor){
 wall(m,x-.8f,y-.8f,x+.8f,y+1.f,floor+.32f,floor+.8f,12);
 wall(m,x-.65f,y+.4f,x+.65f,y+1.f,floor+.8f,floor+1.1f,11);
 for(float xx:{x-.9f,x+.7f})for(float yy:{y-.45f,y+.65f})m.pipes.push_back({{xx,yy},{xx+.2f,yy},floor+.32f,.32f,floor+.32f,1});
 wall(m,x-.32f,y-.15f,x+.32f,y+.45f,floor+.8f,floor+.9f,8);wall(m,x-.32f,y+.35f,x+.32f,y+.45f,floor+.9f,floor+1.45f,8);
 for(float xx:{x-.65f,x+.57f})for(float yy:{y-.65f,y+.8f})wall(m,xx,yy,xx+.08f,yy+.08f,floor+.78f,floor+2.1f,12);
 wall(m,x-.75f,y-.7f,x+.75f,y+.95f,floor+2.1f,floor+2.2f,12);
 for(float xx:{x-.6f,x+.45f})wall(m,xx,y-.95f,xx+.15f,y-.8f,floor+.05f,floor+2.65f,12);
 for(float xx:{x-.6f,x+.45f})wall(m,xx,y-2.15f,xx+.15f,y-.8f,floor+.09f,floor+.16f,12);
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
 wall(m,11.5f,16.7f,12.5f,17.3f,-21.4f,-20.85f,2);
 m.pipes.push_back({{12,17},{12,17},-21.4f,.035f,-22.15f});
 wall(m,11.7f,16.85f,12.3f,17.15f,-22.3f,-22.15f,8);
 m.fixtures.push_back({8,{18.3f,22.7f},1.f,.75f,.15f,1.f,kPi,false});
 m.lights.clear();for(auto p:{Vec2{8,15},Vec2{16,15},Vec2{12,21}})m.lights.push_back({p,-21.7f});
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
  board(m,4,8,-9.7f,"PERSONNEL ROUTE","AUX BRAKE / WEST MACHINERY WALKWAY",3.8f);
  cargo(m,20,6,-12);cargo(m,20,8,-12,false);
  handCart(m,19,11,-12);
  board(m,19,3,-9.8f,"FREIGHT SAFETY","KEEP CLEAR / NO PASSENGERS ON LOADS",3.f);
  for(float yy:{18.f,21.f})m.fixtures.push_back({7,{2,yy},0,1.5f,.65f,2.f,-kPi*.5f,true});
  for(float y:{3.f,7.f,11.f})m.pipes.push_back({{1.1f,y},{6.8f,y},-8.85f,.08f});
  m.fixtures.push_back({19,{10.5f,5.f},0,1.8f,.65f,.9f,0,true});
  m.pickupSpawns.push_back({{11,5.7f},PickupKind::Ammo,-12});
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
  cue(m,"receiving_worker_encounter_ms",6200,Sound::WorkerDying,{16.2f,16},"",1.5f);
  cue(m,"receiving_worker_encounter_ms",6900,Sound::PunchHit,{16.2f,16});
  cue(m,"receiving_worker_encounter_ms",7040,Sound::Metal1,{14.6f,16.4f});
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
  m.creatureSpawns={{CreatureKind::Huntsman,{14,15},-25}};
  m.pickupSpawns.push_back({{16,7},PickupKind::Ammo,-25});
  // Inspection islands divide unloading from the through personnel aisle.
  wall(m,13,12,15,14,-25,-24.35f,2);
  cargo(m,13.9f,13,-24.35f);
  board(m,5,21,-22.5f,"INSPECTION HOLD","RESEARCH RECEIVING / SEALED LOAD",3.f);
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
  if(level==16){office(m,7.5f,14,-25,"WAREHOUSE / OPERATIONS","DO NOT SEND ANYTHING TO RECEIVING.","THEY ARE NOT ANSWERING.");m.fixtures.push_back({19,{7.5f,9.5f},0,2.4f,.9f,1.1f,kPi*.5f,true});}
  if(level==16){forklift(m,20,17,-25);terminal(m,{21,20},"FORKLIFT / SHIFT REPORT","DRIVE MOTOR LEFT RUNNING. BATTERY EMPTY.","OPERATOR DID NOT CLOCK OUT.");}
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
  if(level==18){machineryClock(m,"upper_rack_noise",23000);cue(m,"upper_rack_noise",7000,Sound::JunkMetal,{19,14},"",.9f);cue(m,"upper_rack_noise",16000,Sound::SpiderCall,{19,14},"",.5f);for(float yy:{12.f,13.f,14.f})wall(m,18,yy,21,yy+.15f,-7,-6.85f,10);board(m,19.5f,16,-4.7f,"RACK ACCESS CLOSED","OVERHEAD STOCK DISPLACED",2.7f);}
  if(level==17||level==18)m.creatureSpawns={{CreatureKind::Huntsman,{20,11},entry},{CreatureKind::Wasp,{12,18},entry+2}};
  m.pickupSpawns.push_back({level==19?Vec2{21,12}:Vec2{12,3},PickupKind::Ammo,entry});
  m.lights.clear();for(float z:{-22.5f,-16.5f,-10.5f})for(float y:{6.f,14.f})for(float x:{6.6f,16.4f}){
   m.lights.push_back({{x,y},z});float anchor=x<10?5.3f:17.5f;
   wall(m,std::min(x-.5f,anchor),y-.1f,std::max(x+.5f,anchor+.1f),y+.1f,z+.17f,z+.27f,12);
   wall(m,anchor,y-.1f,anchor+.1f,y+.1f,z+.17f,z+.65f,12);
  }
  m.definition.ambient=.40f;
  for(float x:{7.f,16.f})floorLine(m,x,4,x+.10f,22,-25);
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
  break;
 case 22:
  machineryClock(m,"transfer_belts",14000);
  for(float y:{7.f,15.f})actor(m,"transfer_belts",ActorVisual::Cargo,.7f,{{0,{2,y+.35f},-4.7f,0},{4000,{8,y+.35f},-4.7f,0},{6500,{8,y+.35f},-4.7f,0},{12000,{21,y+.35f},-4.7f,0},{14000,{2,y+.35f},-4.7f,0}},true);
  cue(m,"transfer_belts",100,Sound::Machine,{8,7},"",.35f);
  // Three stair flights carry the player down alongside live conveyor spines.
  m.stairs={{3,4,6,13,-13,-7,32,true,false},{8,13,17,16,-19,-13,32,false,false},{17,18,20,23,-25,-19,32,true,false}};
  deck(m,-13,3,13,11,16);deck(m,-19,17,13,20,20);
  for(float y:{7.f,15.f})wall(m,1,y,23,y+.8f,-5,-4.75f,2);
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
   m.lights.push_back({{15.35f,y},-26.3f});
  }
  for(float y:{6.f,10.f}){wall(m,18.05f,y-.12f,19.15f,y+.12f,-25.53f,-25.43f,12);m.lights.push_back({{18.45f,y},-25.7f});}
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
  break;
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
 static const auto maps=[](){std::array<std::shared_ptr<const Map>,CampaignChunkCount> result;for(int i=10;i<CampaignChunkCount;++i)result[i]=make(i);return result;}();
 return maps.at(size_t(level));
}
}

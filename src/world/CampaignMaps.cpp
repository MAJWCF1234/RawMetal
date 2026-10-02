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
 for(float xx:{x,x+2.6f})for(float yy:{y,y+3.8f})wall(m,xx,yy,xx+.12f,yy+.12f,floor,floor+tiers*3.f);
 for(int tier=0;tier<tiers;++tier){float z=floor+tier*3.f;
  wall(m,x,y,x+2.72f,y+4,z,z+.12f);
  for(int pallet=0;pallet<2;++pallet){float yy=y+.3f+pallet*1.8f;
   if(tier==0)cargo(m,x+1.36f,yy+.7f,z+.14f);
   else for(float xx:{x+.8f,x+1.95f})m.fixtures.push_back({17,{xx,yy+.7f},z+.14f-m.layers.front().elevation,1.05f,1.3f,1.5f,0,true});
  }
 }
}
void container(Map& m,float x,float y,float floor,bool open=false,int material=11){
 // A container is a room with a real walk-in mouth, floor, sides and roof.
 wall(m,x,y,x+.12f,y+6,floor,floor+2.8f,material);wall(m,x+2.9f,y,x+3.02f,y+6,floor,floor+2.8f,material);
 wall(m,x,y+5.85f,x+3.02f,y+6,floor,floor+2.8f,material);
 wall(m,x,y,x+3.02f,y+6,floor+2.7f,floor+2.8f,material);
 if(!open)wall(m,x,y,x+3.02f,y+.12f,floor,floor+2.7f,material);
 else m.clutterSpawns.push_back({3,{x+1.5f,y+3.f},floor});
}
void rail(Map& m,float x,float y1,float y2,float z){
 for(float dx:{-.7f,.7f})wall(m,x+dx-.045f,y1,x+dx+.045f,y2,z,z+.07f,2);
 for(float y=y1;y<y2;y+=.8f)wall(m,x-1.f,y,x+1.f,y+.12f,z-.02f,z+.015f,2);
}
void locomotive(Map& m,float x,float y,float floor,bool stripped=false){
 // Rail gauge, underframe and bogies share one centreline. The suspended
 // inspection chassis retains the same footprint as the complete engine.
 for(float yy:{y+1.5f,y+5.5f}){
  wall(m,x-.9f,yy-.65f,x+.9f,yy+.65f,floor+.32f,floor+.78f,8);
  for(float dx:{-.82f,.82f}){
   wall(m,x+dx-.13f,yy-.52f,x+dx+.13f,yy+.52f,floor+.18f,floor+.82f,8);
   wall(m,x+dx-.15f,yy-.30f,x+dx+.15f,yy+.30f,floor+.04f,floor+.96f,8);
  }
 }
 wall(m,x-1.25f,y,x+1.25f,y+7.f,floor+.96f,floor+1.22f,2);
 for(float yy:{y-.4f,y+7.f})wall(m,x-.28f,yy,x+.28f,yy+.4f,floor+.7f,floor+1.f,8);
 if(stripped){
  for(float yy:{y+1.f,y+3.f,y+5.f})wall(m,x-.85f,yy,x+.85f,yy+.5f,floor+1.22f,floor+1.65f,2);
  return;
 }
 wall(m,x-1.08f,y+.3f,x+1.08f,y+3.7f,floor+1.22f,floor+2.65f,11);
 wall(m,x-.65f,y+1.f,x+.65f,y+2.9f,floor+2.65f,floor+2.88f,8);
 wall(m,x-.18f,y+2.5f,x+.18f,y+2.9f,floor+2.88f,floor+3.45f,8);
 wall(m,x-1.2f,y+4.f,x+1.2f,y+6.8f,floor+1.22f,floor+3.5f,11);
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
void observationWindow(Map& m,float x,float y1,float y2,float floor){
 // Framed, open observation apertures keep the warehouse visible; a solid
 // opaque panel would hide the route the player has just climbed.
 wall(m,x,y1,x+.12f,y2,floor,floor+.85f,3);
 wall(m,x,y1,x+.12f,y2,floor+2.65f,floor+3.3f,3);
 for(float y=y1;y<y2;y+=2.f)wall(m,x,y,x+.12f,y+.08f,floor+.85f,floor+2.65f,2);
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
  for(float y:{3.f,7.f,11.f})m.pipes.push_back({{1.1f,y},{6.8f,y},-8.85f,.08f});
  m.fixtures.push_back({6,{10.5f,5.f},0,1.8f,.65f,.9f,0,true});
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
  {Door gate{9.5f,14.5f,10.f,0,false,false,false,13.f};gate.requireState=stateId("freight_release");m.doors.insert(m.doors.begin(),gate);}
  break;
 case 13:
  board(m,12,6.5f,-17.9f,"RECEIVING 01","UNLOADING / KEEP CLEAR",6.4f);
  m.timedSequences.push_back({stateId("freight_worker_ms"),stateId("freight_worker_dead"),0,0,24,9,-22,100});
  // First receiving balcony, above unloading lanes. The descent folds back
  // along the west side instead of allowing a straight drop into the scene.
  deck(m,-19,1,4,8,10);m.stairs.push_back({3,9,6,18,-25,-19,32,true,false});
  for(float x:{8.f,18.f})container(m,x,7,-25,x<10);
  office(m,18.5f,18.5f,-25,"RECEIVING / WORKER NOTE","THEY ARE STILL DOWN HERE. KEEP THE RADIO ON.","WAREHOUSE INTAKE: FOLLOW THE WEST SERVICE AISLE.");
  m.clutterSpawns.push_back({1,{17.5f,17.5f},-25});m.clutterSpawns.push_back({3,{19.7f,18.2f},-25});
  m.creatureSpawns={{CreatureKind::Wasp,{18,17},-23.5f},{CreatureKind::Wasp,{20,18},-23.5f}};
  m.pickupSpawns.push_back({{18.5f,20.5f},PickupKind::Health,-25});
  break;
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
  break;
 case 16:case 17:case 18:case 19:
  board(m,12,10,-20.6f,level==16?"A / PARTS + B / SPARES":level==17?"C / CHEMICALS":level==18?"D / RESTRICTED":"E / LONG TERM","STORAGE / KEEP AISLES CLEAR",5.f);
  for(float x:{2.f,18.f})for(float y:{5.f,13.f})rack(m,x,y,-25,6);
  for(int tier=1;tier<=5;++tier){float z=-25+tier*3.f;
   deck(m,z,1,4,6,21);deck(m,z,18,4,23,21);
   deck(m,z,6,5,18,8);
  }
  if(level==17)climb(m,-25,-19);
  if(level==18)climb(m,-19,-13);
  if(level==18)deck(m,-13,0,9,11,24);
  if(level==19)climb(m,-13,-7);
  if(level==16){office(m,12,15,-25,"WAREHOUSE / OPERATIONS","DO NOT SEND ANYTHING TO RECEIVING.","THEY ARE NOT ANSWERING.");m.fixtures.push_back({6,{12,8},0,2.4f,.9f,1.1f,.15f,true});}
  // Tall aisle-end labels remain readable from the opposite rack balcony.
  board(m,3.35f,4.8f,-17.7f,level==16?"A / INDUSTRIAL PARTS":level==17?"C / CHEMICAL INVENTORY":level==18?"D / RESTRICTED FREIGHT":"E / LONG TERM STORAGE","BLOCK DIRECTORY / UPPER WALK",2.6f);
  if(level==16){bench(m,7,19,-25);terminal(m,{7,21},"WAREHOUSE / BREAK CORNER","RADIO CHECK 02:14 / NO REPLY FROM RECEIVING.","DO NOT USE THE FREIGHT AISLES FOR EVACUATION.");}
  if(level==17){m.fixtures.push_back({14,{16.1f,18.2f},0,1.3f,1.35f,2.f,0,true});board(m,20,20,-22.7f,"CHEMICAL HOLD","SEALED STOCK / NO TRANSFER",2.5f);}
  if(level==18){container(m,2,17,-25,true,7);terminal(m,{3.5f,19},"RESTRICTED / SEALED MANIFEST","CONTAINMENT HARDWARE / SAMPLE HANDLING.","CONTENTS NOT FOR LOCAL INSPECTION.",6);}
  if(level==19){terminal(m,{12,22},"UPPER STORAGE / BREACH","RESTRICTED FREIGHT: MANIFEST CONTROL.","OFFICES ABOVE THE INTAKE END.",18);m.clutterSpawns.push_back({3,{13,22},-7});}
  if(level==17||level==18)m.creatureSpawns={{CreatureKind::Huntsman,{20,11},entry},{CreatureKind::Wasp,{12,18},entry+2}};
  m.pickupSpawns.push_back({level==19?Vec2{21,12}:Vec2{12,3},PickupKind::Ammo,entry});
  break;
 case 20:case 21:
  m.definition.floorMaterial=FloorMaterial::OfficeCarpet;
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
  }
  bench(m,6,19,-7);
  terminal(m,{6,16},"MANIFEST / MISSED CALLS","RECEIVING: 02:14 / 02:17 / 02:21 / NO ANSWER.","EMPLOYEE TRANSIT LOG: PLATFORM 3.");
  if(level==21){office(m,18,18,-7,"MANIFEST / ROUTING SHUTTERS","TRANSFER YARD: ROUTING AUTHORIZATION.","E / RELEASE THE DOWNSTREAM SHUTTERS.",stateId("freight_routing"),true);m.doors.back().requireState=stateId("freight_routing");}
  m.pickupSpawns.push_back({{6,18},PickupKind::Health,-7});
  break;
 case 22:
  // Three stair flights carry the player down alongside live conveyor spines.
  m.stairs={{3,4,6,13,-13,-7,32,true,false},{8,13,17,16,-19,-13,32,false,false},{17,18,20,23,-25,-19,32,true,false}};
  deck(m,-13,3,13,11,16);deck(m,-19,17,13,20,20);
  for(float y:{7.f,15.f})wall(m,1,y,23,y+.8f,-5,-4.75f,2);
  terminal(m,{22,21},"TRANSFER / CONVEYOR ROUTE","SORTING FLOOR BELOW / FOLLOW AMBER CLEARANCE.","DEPOT AND PLATFORMS: SOUTH RAIL SPUR.");
  break;
 case 23:
  board(m,13,20,-22.3f,"SORTING","DEPOT / PLATFORM 3",3.f);
  cargo(m,3,12,-25);cargo(m,5,12,-25,false);
  m.compactors.push_back({7,7,11,15,-25,-22.4f,8,stateId("sorting_gate_stop")});
  terminal(m,{5,6},"SORTING / CARGO GATE","HYDRAULIC PUSHER: ACTIVE.","E / TOGGLE GATE ISOLATION.",0,stateId("sorting_gate_stop"));m.terminals.back().toggleState=true;
  container(m,17,7,-25,true);m.creatureSpawns={{CreatureKind::Huntsman,{9,17},-25},{CreatureKind::Wasp,{16,13},-23}};
  m.pickupSpawns.push_back({{19,10},PickupKind::Ammo,-25});
  break;
 case 24:
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
  for(float y:{5.f,11.f,17.f})bench(m,13,y,-25);
  // Trackside fences separate transit from freight, with gaps at entry/exit.
  for(float x:{9.f,16.f})m.structures.push_back({x,4,x+.07f,21,-25,-23.95f,true,2});
  m.fixtures.push_back({8,{22.7f,12},1.f,.7f,.15f,1.f,-kPi*.5f,false});
  if(level==25)terminal(m,{12,3},"PLATFORM 3 / DEPARTURES","TRAIN 27 / ARRIVED 02:13.","NO DEPARTURE RECORDED / TRACK CLEAR.");
  else {terminal(m,{12,18},"PLATFORM / SECURITY RECORD","02:12:51 ... 02:19:04 / BLOCK MISSING.","EMPLOYEE TRANSIT: ROLLING STOCK DEPOT.");for(int i=0;i<6;++i)m.clutterSpawns.push_back({i%4,{11.f+float(i%3),19.f+float(i/3)},-25});}
  m.lights.clear();for(float y:{3.f,12.f,21.f})m.lights.push_back({{12,y},-18.25f});
  break;
 case 27:
  board(m,12,6,-22.2f,"ROLLING STOCK","ENGINE 04 / MAINTENANCE",3.f);
  for(float x:{7.f,17.f})rail(m,x,1,23,-25);
  deck(m,-25,1,4,11,10);deck(m,-25,13,4,23,24);
  m.stairs.push_back({3,9,6,18,-28,-25,16,true,false});
  locomotive(m,17,8,-25);
  m.lights.push_back({{17,11},-20.5f});
  for(float x:{15.2f,18.3f})wall(m,x,15.3f,x+.5f,16.3f,-25,-23.5f,3);
  terminal(m,{18,18},"DEPOT / MAINTENANCE LOG","ENGINE 04: DRIVER ATTEMPTED EVACUATION.","VEHICLE STOPPED AT THE BARRICADE.",3);
  break;
 case 28:
  rail(m,7,1,23,-25);rail(m,17,1,23,-25);
  // A suspended chassis straddles the reachable inspection pit.
  locomotive(m,17,5,-25,true);
  m.lights.push_back({{17,8},-20.5f});
  for(float x:{15.65f,18.13f})for(float y:{6.f,10.5f}){
   wall(m,x-.22f,y-.22f,x+.22f,y+.22f,-28,-27.6f,8);
   wall(m,x-.07f,y-.07f,x+.07f,y+.07f,-27.6f,-24.04f,2);
  }
  climb(m,-28,-19);deck(m,-19,8,21,16,24);
  m.pickupSpawns.push_back({{12,3},PickupKind::Ammo,-28});
  m.fixtures.push_back({12,{3,20},0,1.8f,1.4f,1.6f,0,true});
  board(m,20,18,-25.4f,"INSPECTION PIT","JACKS ENGAGED / DO NOT LOWER",2.6f);
  break;
 case 29:
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
   wall(m,12.5f,13,22.8f,14.5f,-25,-22.9f,3);
   board(m,5,12,-22.6f,"SERVICE BYPASS","MAIN BORE / COLLAPSE AHEAD",2.8f);
   m.fixtures.push_back({13,{2,6},0,.9f,.5f,2.2f,-kPi*.5f,true});
  }
  else {wall(m,3,7,9,9,-25,-23.3f,3);wall(m,15,13,21,15,-25,-23.2f,3);
   terminal(m,{12,21},"DEEP JUNCTION / SUBSTATION","FREIGHT MAIN LINE CONTINUES EAST.","POWER DISTRIBUTION: NEXT DISTRICT / ROUTE UNDER CONSTRUCTION.");
   m.creatureSpawns={{CreatureKind::Huntsman,{18,19},-25}};
  }
  m.lights.clear();for(float y:{2.f,10.f,18.f})m.lights.push_back({{3,y},-19.45f});
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

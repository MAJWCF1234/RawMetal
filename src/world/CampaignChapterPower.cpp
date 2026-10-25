// POWER DISTRIBUTION (chapter 1, manifest 19-24): chunks 34-40. See
// docs/Design/descent-campaign-20261025.md for the brief.
//
// The deep plant's electrical heart, dead since the grid tripped at 02:13.
// The player black-starts it: Substation A is dead and points through the
// Cable Trench to Emergency Generation; the generators (pwr_gen_online) bring
// the district's work lights back and make Substation A and the Switch Hall
// live; Substation A's breakers (pwr_sub_a) and the three-step load transfer
// at Load Control (pwr_feeder9) energise Feeder 9, which opens the hub's gate
// to the Machine Hall and wakes the mail relay.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;

// --- District states ------------------------------------------------------------------------
const StateId PwrGen=stateId("pwr_gen_online"),PwrSubA=stateId("pwr_sub_a"),PwrFeeder=stateId("pwr_feeder9");

// --- Light ----------------------------------------------------------------------------------
// The district runs on dim red emergency lamps until the generators start;
// then the red goes out and the work lights come on.
void pwrRed(Map& m,Vec2 p,float z,float intensity=.9f,float range=6.f,WorldLightMount mount=WorldLightMount::Ceiling,float yaw=0){
 WorldLight l=lamp(p,z,intensity,range,1150,-.4f,mount,yaw);l.enabledState=PwrGen;l.invert=true;m.lights.push_back(l);
}
void pwrWork(Map& m,Vec2 p,float z,float intensity,float range,float kelvin=5200,float tint=.1f,WorldLightMount mount=WorldLightMount::Ceiling,float yaw=0,StateId state=PwrGen,float flicker=0){
 WorldLight l=lamp(p,z,intensity,range,kelvin,tint,mount,yaw);l.enabledState=state;l.flicker=flicker;m.lights.push_back(l);
}

// --- Dressing -------------------------------------------------------------------------------
// A buffer stop on a running line along y at x, its beam face at y toward
// -y: rail ends, a steel frame, a timber beam with a hazard face, and the
// concrete block it is bolted to.
void pwrBuffer(Map& m,float x,float y,float floor){
 for(float dx:{-.7f,.7f}){wall(m,x+dx-.05f,y,x+dx+.05f,y+.9f,floor,floor+.07f,2);wall(m,x+dx-.08f,y+.12f,x+dx+.08f,y+.9f,floor,floor+1.12f,12);}
 wall(m,x-1.f,y,x+1.f,y+.3f,floor+.7f,floor+1.05f,10);
 wall(m,x-1.f,y-.012f,x+1.f,y,floor+.72f,floor+1.03f,13);
 wall(m,x-1.2f,y+.9f,x+1.2f,y+1.8f,floor,floor+1.25f,15);
 floorLine(m,x-1.2f,y+.9f,x+1.2f,y+1.02f,floor+1.25f,13);
 m.structures.push_back({x-1.f,y,x+1.f,y+.9f,floor,floor+1.12f,false,6});
}
// A worker's body, dead where he fell (Worker clip 4 held at its end).
void pwrBody(Map& m,const char* clock,Vec2 p,float z,float yaw){
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,p,z,yaw,4,1},{60000,p,z,yaw,4,1}});
}
// A blood pool on the floor: overlapping thin decals, darkest at the centre.
void pwrBlood(Map& m,Vec2 c,float floor,float r){
 wall(m,c.x-r,c.y-r*.6f,c.x+r,c.y+r*.6f,floor+.003f,floor+.006f,16);
 wall(m,c.x-r*.6f,c.y-r,c.x+r*.7f,c.y+r*.8f,floor+.004f,floor+.007f,16);
 wall(m,c.x+r*.5f,c.y-r*.2f,c.x+r*1.6f,c.y+r*.1f,floor+.003f,floor+.006f,16);
}
// A rectangular bus duct along x or y, its casing joined by collars every
// 1.5 m and hung on trapezes (a bearer under it, two rods to the roof).
void pwrBusDuct(Map& m,Vec2 a,Vec2 b,float z,float roof,float w=.36f,float h=.24f){
 const bool alongX=std::fabs(b.x-a.x)>std::fabs(b.y-a.y);
 const float x1=std::min(a.x,b.x),x2=std::max(a.x,b.x),y1=std::min(a.y,b.y),y2=std::max(a.y,b.y);
 if(alongX){
  wall(m,x1,y1-w*.5f,x2,y1+w*.5f,z,z+h,2);
  for(float x=x1+.75f;x<x2-.2f;x+=1.5f)wall(m,x,y1-w*.5f-.025f,x+.07f,y1+w*.5f+.025f,z-.025f,z+h+.025f,8);
  for(float x=x1+1.1f;x<x2-.3f;x+=3.f){wall(m,x-.03f,y1-w*.5f-.12f,x+.03f,y1+w*.5f+.12f,z-.07f,z,8);
   for(float s:{-1.f,1.f})m.pipes.push_back({{x,y1+s*(w*.5f+.08f)},{x,y1+s*(w*.5f+.08f)},z-.05f,.014f,roof-.02f,6});}
 }else{
  wall(m,x1-w*.5f,y1,x1+w*.5f,y2,z,z+h,2);
  for(float y=y1+.75f;y<y2-.2f;y+=1.5f)wall(m,x1-w*.5f-.025f,y,x1+w*.5f+.025f,y+.07f,z-.025f,z+h+.025f,8);
  for(float y=y1+1.1f;y<y2-.3f;y+=3.f){wall(m,x1-w*.5f-.12f,y-.03f,x1+w*.5f+.12f,y+.03f,z-.07f,z,8);
   for(float s:{-1.f,1.f})m.pipes.push_back({{x1+s*(w*.5f+.08f),y},{x1+s*(w*.5f+.08f),y},z-.05f,.014f,roof-.02f,6});}
 }
}
// Rubber insulating matting laid over the floor: the safe lane.
void pwrMat(Map& m,float x1,float y1,float x2,float y2,float floor){
 wall(m,x1,y1,x2,y2,floor,floor+.012f,8);
 floorLine(m,x1,y1,x2,y1+.06f,floor+.012f,13);floorLine(m,x1,y2-.06f,x2,y2,floor+.012f,13);
}
// A portal through a wall along x (face toward -y when face<0): concrete
// jambs with hazard-striped returns, a lintel and the wall over it to `top`.
void pwrPortalX(Map& m,float x1,float x2,float y1,float y2,float floor,float height,float top){
 for(float x:{x1-.6f,x2}){wall(m,x,y1,x+.6f,y2,floor,floor+height,15);}
 wall(m,x1-.05f,y1,x1,y2,floor,floor+height,13);wall(m,x2,y1,x2+.05f,y2,floor,floor+height,13);
 wall(m,x1-.6f,y1,x2+.6f,y2,floor+height,top,15);
 wall(m,x1-.6f,y1-.04f,x2+.6f,y1,floor+height,floor+height+.2f,13);
}

// POWER DISTRIBUTION. The main line's last stop inside the plant: the HV
// railhead, where transformers and switchgear came in by rail. The blast door
// from Deep Junction opens on the siding: a spare transformer still chained
// to its flat wagon under the unloading crane, its hook still in the slings.
// West of the track, the personnel side: the PPE and permit room where every
// permit was voided at 02:31, the cable store, and the checkpoint the Cleanup
// Force left at the high-voltage portal that leads south to the hub.
std::shared_ptr<const Map> makePowerDistribution(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[34];m.definition=CampaignChunks[34];m.definition.ambient=.24f;
 m.definition.playerStart={19.f,2.3f};m.definition.spawnYaw=kPi*.5f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'N',16.5f,21.5f,-25.f,true,0});
 openEdge(m,{'S',9.f,15.f,-25.f,false,0});

 // --- The siding: the main line runs on from Deep Junction to a buffer ---------------------------
 rail(m,19.f,0.f,19.5f,base);
 pwrBuffer(m,19.f,19.5f,base);
 wallSign(m,19.f,22.945f,base+2.1f,"SIDING 1 / HV RAILHEAD","END OF MAIN LINE / MAX 40 T",2.6f,kPi);
 flatWagon(m,19.f,6.4f,base,false);
 const float wagonDeck=base+1.12f;
 // Spare T3 for Substation A, chained down, and two crates of spares.
 m.fixtures.push_back({model::transformer,{19.f,12.6f},wagonDeck-base,4.4f,2.4f,2.6f,kPi*.5f,true});
 m.fixtures.push_back({model::packing_crate,{19.f,8.1f},wagonDeck-base,1.5f,1.2f,1.f,0,true});
 m.fixtures.push_back({model::packing_crate,{18.9f,17.3f},wagonDeck-base,1.3f,1.1f,1.15f,.05f,true});
 for(float y:{10.7f,14.5f})for(float s:{-1.f,1.f})
  m.pipes.push_back({{19.f+s*1.24f,y+(y<12?-.5f:.5f)},{19.f+s*1.05f,y},wagonDeck+.02f,.022f,wagonDeck+1.6f,6});
 // --- The unloading crane: a semi-gantry over the siding -------------------------------------------
 const float runway=roof-1.3f;
 for(float y:{3.5f,9.5f,15.5f,21.5f}){steelColumn(m,16.f,y,base,runway,false);
  wall(m,22.6f,y-.25f,23.f,y+.25f,runway-.8f,runway,15);}
 wall(m,15.82f,2.6f,16.18f,22.6f,runway,runway+.35f,12);wall(m,22.55f,2.6f,23.f,22.6f,runway,runway+.35f,12);
 for(float x:{15.95f,22.72f})wall(m,x,2.6f,x+.1f,22.6f,runway+.35f,runway+.42f,8);
 const float girder=runway+.42f;
 for(float y:{12.f,12.95f})wall(m,16.1f,y,22.75f,y+.3f,girder,girder+.5f,18);
 for(float x:{15.7f,22.5f})wall(m,x,11.75f,x+.55f,13.5f,girder-.1f,girder+.3f,12);
 wall(m,16.1f,12.3f,22.75f,12.95f,girder+.46f,girder+.5f,12);
 // The trolley over the transformer, its rope drum, the hook block and the
 // four slings still made off to the lifting lugs.
 wall(m,18.3f,11.9f,19.7f,13.35f,girder+.5f,girder+.78f,18);
 m.pipes.push_back({{18.45f,12.62f},{19.55f,12.62f},girder+.2f,.18f,girder+.2f,8});
 for(float y:{12.5f,12.75f})m.pipes.push_back({{19.f,y},{19.f,y},-20.25f,.018f,girder+.05f,6});
 wall(m,18.78f,12.42f,19.22f,12.83f,-20.6f,-20.25f,18);
 m.pipes.push_back({{19.f,12.62f},{19.f,12.62f},-20.95f,.045f,-20.6f,6});
 for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f})
  m.pipes.push_back({{19.f,12.62f},{19.f+sx*.95f,12.62f+sy*1.7f},-20.92f,.02f,-21.27f,6});
 wallSign(m,22.945f,6.5f,base+2.f,"CRANE 2 / 40 T","PENDANT ISOLATED / NO LIFT ON EMERGENCY POWER",2.2f,-kPi*.5f);
 // The siding terminal by the door: what was on the wagon, and why.
 terminal(m,{14.9f,2.25f},"HV RAILHEAD / SIDING 1","T3 SPARE / 11 KV 40 MVA / ARRIVED 01:12. CONSIGNEE: SUBSTATION A.","T3 TRIPPED 00:58 ON EARTH FAULT. UNLOADING STOPPED 02:13.");
 m.terminals.back().yaw=-kPi*.5f;

 // --- PPE and permit room, north-west ------------------------------------------------------------
 const float room=base+2.8f;
 wall(m,1.f,6.85f,5.6f,7.f,base,room,3);wall(m,6.8f,6.85f,8.2f,7.f,base,room,3);wall(m,5.6f,6.85f,6.8f,7.f,base+2.2f,room,3);
 for(float x:{5.52f,6.8f})wall(m,x,6.82f,x+.08f,7.03f,base,base+2.2f,20);
 // East wall: sill, head, two piers and a glazed screen with one mullion.
 wall(m,8.05f,1.f,8.2f,6.85f,base,base+1.f,3);wall(m,8.05f,1.f,8.2f,6.85f,base+2.3f,room,3);
 wall(m,8.05f,1.f,8.2f,1.6f,base+1.f,base+2.3f,3);wall(m,8.05f,6.3f,8.2f,6.85f,base+1.f,base+2.3f,3);
 wall(m,8.11f,1.6f,8.14f,6.3f,base+1.f,base+2.3f,17);
 wall(m,8.06f,3.9f,8.19f,4.f,base+1.f,base+2.3f,20);
 for(float z:{base+1.f,base+2.26f})wall(m,8.03f,1.6f,8.22f,6.3f,z,z+.04f,20);
 wall(m,1.f,1.f,8.2f,7.f,room,room+.15f,2);
 for(float y:{1.5f,2.37f,3.24f,4.11f})m.fixtures.push_back({model::pwr_locker_bank,{1.46f,y},0,.87f,.85f,2.42f,0,true});
 m.fixtures.push_back({model::bench,{2.95f,2.8f},0,1.86f,.44f,.63f,kPi*.5f,true});
 m.fixtures.push_back({model::metal_shelf,{4.7f,1.27f},0,1.8f,.5f,1.8f,0,true});
 for(float x:{4.2f,4.65f,5.1f})m.fixtures.push_back({model::mine_hard_hat,{x,1.27f},1.8f,.28f,.35f,.15f,0,false});
 m.fixtures.push_back({model::pwr_fire_cabinet,{3.f,6.75f},1.1f,.25f,.2f,.56f,kPi,false});
 m.fixtures.push_back({model::med_first_aid_box,{4.1f,6.78f},1.35f,.37f,.12f,.35f,kPi,false});
 terminal(m,{6.6f,1.75f},"PERMIT TO WORK / POWER DISTRIBUTION","OPEN 01:40: SUBSTATION A (OYELARAN, VASK). LOAD CONTROL (HALE).","SIGNED OUT: NONE. ALL PERMITS VOIDED 02:31 BY CLEANUP COMMAND.");
 m.terminals.back().yaw=.001f;
 wallSign(m,6.6f,1.055f,base+1.55f,"PERMIT TO WORK","SIGN IN / SIGN OUT / NO EXCEPTIONS",1.6f,0);
 wallSign(m,3.f,1.055f,base+1.95f,"ARC FLASH PPE","CAT 4 SUIT / VISOR / GLOVES / MAT",1.4f,0);
 wallSign(m,4.2f,6.995f,base+2.15f,"PPE / PERMITS","SIGN IN BEFORE THE PORTAL",1.6f,0);
 m.pickupSpawns.push_back({{3.6f,5.6f},PickupKind::Health,base});

 // --- Cable store, west ----------------------------------------------------------------------------
 for(Vec2 d:{Vec2{2.4f,9.6f},Vec2{2.4f,12.2f},Vec2{5.2f,9.6f}}){
  m.fixtures.push_back({model::cable_reel,d,0,.85f,1.7f,1.7f,0,true});
  for(float s:{-1.f,1.f})wall(m,d.x-.5f,d.y+s*.42f-.08f,d.x+.5f,d.y+s*.42f+.08f,base,base+.11f,10);
 }
 cargo(m,5.4f,12.6f,base);cargo(m,5.3f,14.2f,base,false);
 for(float y:{8.4f,15.4f})floorLine(m,1.2f,y,7.f,y+.1f,base,13);
 floorLine(m,6.9f,8.4f,7.f,15.5f,base,13);
 wallSign(m,1.055f,11.8f,base+2.4f,"CABLE STORE","11 KV XLPE / 415 V SWA / RETURN OFFCUTS",2.4f,kPi*.5f);
 m.fixtures.push_back({model::metal_shelf,{1.27f,16.8f},0,1.8f,.5f,1.8f,kPi*.5f,true});
 cableTray(m,{1.2f,7.4f},{1.2f,22.6f},base+3.4f);

 // --- The Cleanup checkpoint at the portal ------------------------------------------------------
 // Road blocks across the approach leave one lane; a field desk, crates and
 // a battery floodlight on a ballasted field case aimed at the portal.
 for(Vec2 r:{Vec2{9.2f,17.4f},Vec2{14.9f,17.4f}})m.fixtures.push_back({model::road_block,r,0,1.6f,.5f,.7f,0,true});
 m.fixtures.push_back({model::road_block,{7.6f,18.2f},0,1.6f,.5f,.7f,.35f,true});
 m.fixtures.push_back({model::wooden_chair,{4.7f,19.7f},0,.417f,.525f,.95f,kPi*.5f+.3f,true});
 for(Vec2 c:{Vec2{2.f,21.7f},Vec2{2.f,20.8f},Vec2{2.9f,21.7f}})m.fixtures.push_back({model::ind_crate,c,0,.8f,.8f,.8f,0,true});
 m.fixtures.push_back({model::ind_crate,{2.f,21.7f},.8f,.8f,.8f,.8f,.2f,true});
 terminal(m,{5.5f,19.6f},"CLEANUP / SWEEP 3 / FIELD LOG","02:40 POWER DISTRIBUTION SWEPT. 3 STAFF PROCESSED. 0 OUTSTANDING.","LOAD CONTROL HELD BY TEAM 3. FEEDER 9 STAYS OPEN. NO RESTORATION.");
 m.terminals.back().yaw=kPi*.5f;
 {const Vec2 t{7.6f,16.6f};const float head=base+2.3f;
  m.fixtures.push_back({model::ind_crate,t,0,1.3f,1.3f,.6f,0,true});
  for(float a:{.4f,2.5f,4.6f})m.pipes.push_back({{t.x+std::cos(a)*.55f,t.y+std::sin(a)*.55f},t,base+.6f,.022f,head-.2f,6});
  m.pipes.push_back({t,t,head-.2f,.03f,head-.05f,6});
  wall(m,t.x-.2f,t.y-.13f,t.x+.2f,t.y+.13f,head-.05f,head+.22f,8);
  WorldLight flood=lamp({t.x+.15f,t.y+.32f},head+.08f,2.4f,11.f,6400,0,WorldLightMount::Bare);m.lights.push_back(flood);}
 m.pickupSpawns.push_back({{2.9f,20.6f},PickupKind::Ammo,base});
 m.clutterSpawns.push_back({1,{4.1f,18.6f},base,.4f});m.clutterSpawns.push_back({2,{3.3f,19.9f},base});
 m.clutterSpawns.push_back({0,{6.f,21.f},base,1.f});m.clutterSpawns.push_back({1,{3.5f,18.4f},base,2.f});

 // --- Walkway and the HV portal south to the hub -----------------------------------------------------
 for(float y:{2.6f,4.3f})floorLine(m,8.3f,y,15.6f,y+.1f,base,14);
 for(float x:{10.6f,13.3f})floorLine(m,x,4.4f,x+.1f,20.4f,base,14);
 for(float y=4.6f;y<20.f;y+=1.f)wall(m,11.55f,y,12.35f,y+.95f,base,base+.012f,8);
 pwrPortalX(m,9.f,15.f,22.2f,23.f,base,3.4f,base+4.9f);
 wall(m,9.f,23.f,15.f,24.f,base+3.4f,roof,15);
 wallSign(m,12.f,22.145f,base+3.95f,"DANGER / HIGH VOLTAGE","AUTHORISED PERSONS ONLY / ARC FLASH PPE BEYOND",4.f,kPi);
 pwrMat(m,9.2f,20.6f,14.8f,23.f,base);
 wallSign(m,6.f,22.945f,base+1.4f,"POWER DISTRIBUTION","HUB SOUTH / SUBSTATION A EAST / SWITCH HALL WEST",3.2f,kPi);

 // --- Services ------------------------------------------------------------------------------------
 cableTray(m,{8.6f,1.2f},{15.4f,1.2f},base+3.4f);
 servicePipe(m,{1.2f,22.55f},{8.4f,22.55f},base+3.9f,.09f,4);
 pwrBusDuct(m,{9.f,21.4f},{9.f,7.6f},base+4.6f,roof);
 wall(m,8.75f,7.2f,9.25f,7.6f,base,base+4.84f,15);
 wall(m,8.6f,21.4f,9.4f,22.2f,base+4.4f,base+4.9f,15);
 wallBays(m,base,roof,4.f,false,true,false,false,12.f);
 for(float y:{6.f,17.f})wall(m,1.f,y-.15f,15.8f,y+.15f,roof-.5f,roof,12);

 // --- Light ---------------------------------------------------------------------------------------
 pwrRed(m,{15.f,1.02f},base+3.2f,1.f,7.f,WorldLightMount::Wall,kPi*.5f);
 pwrRed(m,{8.22f,6.58f},base+2.f,.8f,5.f,WorldLightMount::Wall,0);
 pwrRed(m,{2.8f,4.6f},room-.15f,.7f,4.5f);
 for(float x:{8.7f,15.3f})pwrRed(m,{x,22.18f},base+2.6f,1.1f,6.f,WorldLightMount::Wall,-kPi*.5f);
 pwrRed(m,{19.f,4.4f},roof-.5f,.9f,7.f);
 pwrRed(m,{4.5f,12.f},roof-.5f,.7f,6.f);
 pwrRed(m,{12.f,11.f},roof-.5f,.7f,7.f);
 for(Vec2 l:{Vec2{4.5f,10.5f},Vec2{12.f,8.f},Vec2{12.f,16.5f},Vec2{4.8f,18.5f},Vec2{19.5f,4.f},Vec2{19.5f,19.8f}})pwrWork(m,l,roof-.45f,2.2f,12.f,5000,.1f);
 pwrWork(m,{6.f,4.f},room-.15f,1.f,6.f,4300,.25f);
 pwrWork(m,{12.f,21.2f},base+3.25f,1.2f,6.f,4600,.1f);

 zone(m,"POWER DISTRIBUTION / PERMITS",1,1,8.2f,7);
 zone(m,"POWER DISTRIBUTION / HV RAILHEAD",15.6f,0,24,24);
 zone(m,"POWER DISTRIBUTION",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// An operational pedestal in an otherwise unfinished district room. The
// terminal renderer supplies the desk and CRT; its labelled steel backboard
// is rooted in the room floor, leaving the north approach clear for use.
void pwrStation(Map& m,Vec2 pos,const char* title,const char* operation,const char* requirement,StateId state,StateId prerequisite=0){
 const float f=m.layers.front().elevation;
 wall(m,pos.x-.8f,pos.y+.65f,pos.x+.8f,pos.y+.82f,f,f+2.1f,2);
 wallSign(m,pos.x,pos.y+.638f,f+1.45f,title,operation,1.45f,kPi);
 terminal(m,pos,title,operation,requirement,0,state,prerequisite);
 m.terminals.back().yaw=kPi;
}
// DISTRIBUTION / HUB (manifest 19)
std::shared_ptr<const Map> makeDistributionHub(){return placeholderChunk(35,{{'N',9.f,15.f,-25.f,false,0},{'E',9.f,14.f,-25.f,false,0},{'W',9.f,14.f,-25.f,false,0},{'S',10.f,14.f,-25.f,true,stateId("pwr_feeder9")}});}
// SUBSTATION A (manifest 20)
std::shared_ptr<const Map> makeSubstationA(){
 auto p=std::make_shared<Map>(*placeholderChunk(36,{{'W',9.f,14.f,-25.f,false,0},{'E',16.f,19.f,-28.f,false,0}}));
 pwrStation(*p,{12.f,4.f},"SUBSTATION A / BREAKERS","E / CLOSE THE SUBSTATION BREAKERS.","BLACK START REQUIRED: SYNCHRONIZE EMERGENCY GENERATION VIA THE CABLE TRENCH.",PwrSubA,PwrGen);
 return p;
}
// SWITCH HALL (manifest 21)
std::shared_ptr<const Map> makeSwitchHall(){return placeholderChunk(37,{{'E',9.f,14.f,-25.f,false,0},{'S',10.f,14.f,-19.f,false,0}});}
// CABLE TRENCH (manifest 22)
std::shared_ptr<const Map> makeCableTrench(){return placeholderChunk(38,{{'W',16.f,19.f,-28.f,false,0},{'S',14.f,18.f,-28.f,false,0}});}
// EMERGENCY GENERATION (manifest 23)
std::shared_ptr<const Map> makeEmergencyGeneration(){
 auto p=std::make_shared<Map>(*placeholderChunk(39,{{'N',14.f,18.f,-28.f,false,0},{'W',8.f,12.f,-25.f,false,0}}));
 pwrStation(*p,{13.f,7.f},"EMERGENCY GENERATION / SYNC","E / CONNECT THE DIESEL SET TO THE EMERGENCY BUS.","THEN RETURN THROUGH THE CABLE TRENCH AND CLOSE SUBSTATION A'S BREAKERS.",PwrGen);
 return p;
}
// LOAD CONTROL (manifest 24): a temporary model showroom while the district is authored.
std::shared_ptr<const Map> makeLoadControl(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[40];m.definition=CampaignChunks[40];m.definition.ambient=.6f;
 const float f=-19;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,-19.f,false,0});
 struct S{int model;float w,d,h,base;};
 const std::initializer_list<S> rows[]={
  {{model::pwr_switchgear_cabinet,1.5f,.82f,2.7f,0},{model::pwr_tall_switch_cabinet,.95f,1.54f,3.02f,0},{model::pwr_step_transformer,.64f,.89f,1.67f,0},{model::pwr_control_cabinet,.8f,.4f,1.6f,0},{model::pwr_locker_bank,.87f,.85f,2.42f,0},{model::transformer,4.4f,2.4f,2.6f,0}},
  {{model::pwr_diesel_genset,2.29f,1.07f,1.22f,0},{model::pwr_generator_unit,1.13f,2.61f,1.67f,0},{model::pwr_catwalk_section,4.f,2.f,1.31f,0},{model::pwr_lift_station,2.04f,2.f,2.14f,0},{model::pwr_duct_fan_unit,.79f,1.49f,1.95f,0},{model::pwr_air_handler,.81f,1.64f,2.62f,0}},
  {{model::pwr_breaker_panel,.36f,.2f,.92f,1},{model::pwr_breaker_panel_open,.64f,.46f,.92f,1},{model::pwr_breaker_lever,.62f,.4f,.64f,1},{model::pwr_ammeter_box,.46f,.2f,.74f,1},{model::pwr_disconnect_switch,.43f,.35f,.85f,1},{model::pwr_fire_cabinet,.5f,.4f,1.12f,1},{model::pwr_gate_valve,.98f,.56f,.66f,1},{model::pwr_vent_grille,.82f,.04f,.8f,1},{model::lab_control_board,1.32f,.19f,.6f,1},{model::lab_status_display,.69f,.06f,.2f,1.6f},{model::lab_knife_switch,.62f,.4f,.64f,1}}};
 float y=4.f;
 for(const auto& row:rows){float x=1.6f;
  for(const auto& s:row){m.fixtures.push_back({s.model,{x+s.w*.5f,y},s.base,s.w,s.d,s.h,0,false});
   wall(m,x+s.w*.5f-.05f,y+s.d*.5f+.15f,x+s.w*.5f+.05f,y+s.d*.5f+.45f,f,f+.02f,19);
   x+=std::max(s.w,.6f)+.8f;}
  y+=6.5f;}
 for(float x:{4.f,10.f,16.f,21.f})for(float yy:{2.f,7.f,13.f,19.f})m.lights.push_back(lamp({x,yy},-15.9f,1.6f,10.f,4200));
 // Complete the reachable black-start chain without bypassing the hub gate.
 // The showroom remains unfinished; these three controls are operational.
 const StateId ready=stateId("pwr_transfer_ready"),bus=stateId("pwr_transfer_bus");
 pwrStation(m,{5.f,21.f},"LOAD CONTROL / 1 SHED","E / SHED NON-ESSENTIAL LOADS.","REQUIRES SUBSTATION A BREAKERS CLOSED. THEN USE TRANSFER CONTROL 2.",ready,PwrSubA);
 pwrStation(m,{12.f,21.f},"LOAD CONTROL / 2 TRANSFER","E / TRANSFER TO EMERGENCY GENERATION.","REQUIRES LOAD CONTROL 1. THEN USE FEEDER CONTROL 3.",bus,ready);
 pwrStation(m,{19.f,21.f},"LOAD CONTROL / 3 FEEDER 9","E / ENERGIZE FEEDER 9 AND RELEASE THE HUB GATE.","REQUIRES LOAD CONTROL 2. RETURN TO DISTRIBUTION HUB AND CONTINUE SOUTH.",PwrFeeder,bus);
 freightFinish(m,-1);
 return p;
}
}
void buildChapterPower(CampaignMapTable& maps){
 maps[34]=makePowerDistribution();
 maps[35]=makeDistributionHub();
 maps[36]=makeSubstationA();
 maps[37]=makeSwitchHall();
 maps[38]=makeCableTrench();
 maps[39]=makeEmergencyGeneration();
 maps[40]=makeLoadControl();
}
}

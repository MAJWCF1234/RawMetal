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
// Emergency Generation's work lights strike a second after the bus closes;
// Load Control's room lamps follow its own supply (0 dead, 1 on generation,
// 2 the dip while the transfer breaker swings the plant across).
const StateId PwrStrike=stateId("pwr_strike_ms_done"),PwrLcMains=stateId("pwr_lc_mains");

// --- Light ----------------------------------------------------------------------------------
// The district runs on dim red emergency lamps until the generators start;
// then the red goes out and the work lights come on.
void pwrRed(Map& m,Vec2 p,float z,float intensity=.9f,float range=6.f,WorldLightMount mount=WorldLightMount::Ceiling,float yaw=0){
 // Strong enough to read the room by: the dark is red, not black.
 WorldLight l=lamp(p,z,std::min(8.f,intensity*1.7f),std::min(24.f,range+3.f),1250,-.35f,mount,yaw);l.enabledState=PwrGen;l.invert=true;m.lights.push_back(l);
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
 // The siding terminal by the door: what was on the wagon, and why.
 terminal(m,{14.9f,2.25f},"HV RAILHEAD / SIDING 1","T3 SPARE / 11 KV 40 MVA / ARRIVED 01:12. CONSIGNEE: SUBSTATION A.","T3 TRIPPED 00:58 ON EARTH FAULT. UNLOADING STOPPED 02:13.");m.terminals.back().readable=Readable::Letter;
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
 for(float y:{1.5f,2.37f,3.24f,4.11f})m.fixtures.push_back({model::psx_locker,{1.345f,y},0,.64f,.85f,2.42f,0,true}); // backs on the wall, doors east
 m.fixtures.push_back({model::bench,{2.95f,2.8f},0,1.86f,.44f,.63f,kPi*.5f,true});
 m.fixtures.push_back({model::metal_shelf,{4.7f,1.27f},0,1.8f,.5f,1.8f,0,true});
 for(float x:{4.2f,4.65f,5.1f})m.fixtures.push_back({model::mine_hard_hat,{x,1.27f},1.8f,.28f,.35f,.15f,0,false});
 m.fixtures.push_back({model::pwr_fire_cabinet,{3.f,6.75f},1.1f,.25f,.2f,.56f,kPi,false});
 m.fixtures.push_back({model::med_first_aid_box,{4.1f,6.78f},1.35f,.37f,.12f,.35f,kPi,false});
 terminal(m,{6.6f,1.75f},"PERMIT TO WORK / POWER DISTRIBUTION","OPEN 01:40: SUBSTATION A (OYELARAN, VASK). LOAD CONTROL (HALE).","SIGNED OUT: NONE. ALL PERMITS VOIDED 02:31 BY CLEANUP COMMAND.");m.terminals.back().readable=Readable::Letter;
 m.terminals.back().yaw=.001f;
 m.pickupSpawns.push_back({{3.6f,5.6f},PickupKind::Health,base});

 // --- Cable store, west ----------------------------------------------------------------------------
 for(Vec2 d:{Vec2{2.4f,9.6f},Vec2{2.4f,12.2f},Vec2{5.2f,9.6f}}){
  m.fixtures.push_back({model::cable_reel,d,0,.85f,1.7f,1.7f,0,true});
  for(float s:{-1.f,1.f})wall(m,d.x-.5f,d.y+s*.42f-.08f,d.x+.5f,d.y+s*.42f+.08f,base,base+.11f,10);
 }
 cargo(m,5.4f,12.6f,base);cargo(m,5.3f,14.2f,base,false);
 for(float y:{8.4f,15.4f})floorLine(m,1.2f,y,7.f,y+.1f,base,13);
 floorLine(m,6.9f,8.4f,7.f,15.5f,base,13);
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
 terminal(m,{5.5f,19.6f},"CLEANUP / SWEEP 3 / FIELD LOG","02:40 POWER DISTRIBUTION SWEPT. 3 STAFF PROCESSED. 0 OUTSTANDING.","LOAD CONTROL HELD BY TEAM 3. FEEDER 9 STAYS OPEN. NO RESTORATION.");m.terminals.back().readable=Readable::Note;
 m.terminals.back().yaw=kPi*.5f;
 {const Vec2 t{7.6f,16.6f};const float head=base+2.3f;
  m.fixtures.push_back({model::ind_crate,t,0,.95f,.95f,.95f,0,true});
  for(float a:{.4f,2.5f,4.6f})m.pipes.push_back({{t.x+std::cos(a)*.38f,t.y+std::sin(a)*.38f},t,base+.95f,.022f,head-.2f,6});
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
 titleCard(m,"POWER DISTRIBUTION");
 return p;
}
// A portal through an east or west wall: jambs either side of y1..y2 across
// the wall's thickness x1..x2, and a lintel up to `top`.
void pwrPortalY(Map& m,float x1,float x2,float y1,float y2,float floor,float height,float top){
 for(float y:{y1-.6f,y2})wall(m,x1,y,x2,y+.6f,floor,floor+height,15);
 wall(m,x1,y1-.6f,x2,y2+.6f,floor+height,top,15);
 wall(m,x1-.04f,y1-.6f,x1,y2+.6f,floor+height,floor+height+.2f,13);
}
// A switchgear lineup along x, fronts toward +y (front>0) or -y: cabinets on a
// plinth, the cable trench cover in front and the rubber mat for the operator.
void pwrLineup(Map& m,float x1,int count,float y,float floor,float front){
 const float w=1.5f,d=.82f;
 wall(m,x1-.05f,y-d*.5f-.05f,x1+count*w+.05f,y+d*.5f+.05f,floor,floor+.12f,15);
 for(int i=0;i<count;++i)m.fixtures.push_back({model::pwr_switchgear_cabinet,{x1+w*(i+.5f),y},.12f,w,d,2.7f,front>0?0.f:kPi,true});
 const float mat=y+front*(d*.5f+.1f);
 pwrMat(m,x1,std::min(mat,mat+front*1.2f),x1+count*w,std::max(mat,mat+front*1.2f),floor);
}
// A red emergency lamp that goes dark when `state` is set, or a work lamp that
// comes on with it.
void pwrLamp(Map& m,Vec2 p,float z,float intensity,float range,float kelvin,float tint,StateId state,bool onWithState,WorldLightMount mount=WorldLightMount::Ceiling,float yaw=0,float flicker=0){
 WorldLight l=lamp(p,z,intensity,range,kelvin,tint,mount,yaw);l.enabledState=state;l.invert=!onWithState;l.flicker=flicker;m.lights.push_back(l);
}
// A control answered where the machinery is: once `state` is set, a timer runs
// for lengthMs (its finish state is "<timer>_done") and the listed sounds play
// at their places. With tickMs the machine at `at` also runs, its sound every
// tickMs until untilMs. The sequence's box is the machine's footprint lifted
// out of reach, so walking past it never starts it.
struct PwrSound {int ms;Sound sound;Vec2 at;float gain=.6f,pitch=1;};
void pwrAnswer(Map& m,const char* timer,StateId state,int level,int lengthMs,Vec2 at,std::initializer_list<PwrSound> sounds,
               int tickMs=0,int untilMs=0,Sound tick=Sound::Machine,float gain=.5f,float pitch=.55f,bool loop=false){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_done");
 s.x1=at.x-.5f;s.y1=at.y-.5f;s.x2=at.x+.5f;s.y2=at.y+.5f;s.bottom=1000;s.top=1001;
 s.durationMs=lengthMs;s.finishAtMs=lengthMs;s.soundIntervalMs=tickMs;s.soundUntilMs=untilMs;s.tickSound=tick;s.gain=gain;s.pitch=pitch;s.loop=loop;
 m.timedSequences.push_back(s);
 for(const auto& c:sounds)m.sequenceCues.push_back({stateId(timer),c.ms,c.sound,c.at,c.gain,c.pitch,""});
 ScriptEvent e;e.id=stateId(std::string(timer)+"_go");e.level=level;e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=state;
 ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(timer);a.value=1;e.actions.push_back(a);m.scriptEvents.push_back(e);
}
// A machine-room hum where the machine stands, from when `state` is set.
void pwrRunning(Map& m,const char* timer,StateId state,int level,float gain,Vec2 at){pwrAnswer(m,timer,state,level,4000,at,{},1300,4000,Sound::Machine,gain,.55f,true);}
// Once `state` is set and the player is in the chunk, sets `target` to value
// (and shakes the view a little if shake>0).
void pwrOnState(Map& m,const char* id,int level,StateId state,StateId target,int value,float shake=0){
 ScriptEvent e;e.id=stateId(id);e.level=level;e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=state;
 ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=target;a.value=value;e.actions.push_back(a);
 if(shake>0){ScriptAction k;k.type=ScriptAction::Type::Shake;k.amount=shake;e.actions.push_back(k);}
 m.scriptEvents.push_back(e);
}
ScriptAction pwrSpawn(CreatureKind kind,Vec2 p,float z,float awareness=4){ScriptAction a;a.type=ScriptAction::Type::SpawnEnemy;a.enemyKind=kind;a.position=p;a.z=z;a.amount=awareness;return a;}

// DISTRIBUTION / HUB. The district's switching concourse: the 11 kV bus comes
// in overhead from the railhead and splits three ways. East to Substation A,
// west to the Switch Hall and Load Control above it, south through the
// Feeder 9 gate to the Machine Hall. The mimic board says what is dead. The
// mail relay in the corner holds one message it cannot send.
std::shared_ptr<const Map> makeDistributionHub(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[35];m.definition=CampaignChunks[35];m.definition.ambient=.3f;
 const float base=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'N',9.f,15.f,-25.f,false,0});openEdge(m,{'E',9.f,14.f,-25.f,false,0});
 openEdge(m,{'W',9.f,14.f,-25.f,false,0});openEdge(m,{'S',10.f,14.f,-25.f,true,PwrFeeder});
 // --- Portals: north from the railhead, east, west, and the Feeder 9 gate ----------------------
 wall(m,9.f,0.f,15.f,1.f,base+3.4f,roof,15);
 pwrPortalY(m,22.4f,23.f,9.f,14.f,base,3.4f,roof);
 pwrPortalY(m,1.f,1.6f,9.f,14.f,base,3.4f,roof);
 wall(m,23.f,9.f,24.f,14.f,base+3.4f,roof,15);wall(m,0.f,9.f,1.f,14.f,base+3.4f,roof,15);
 pwrPortalX(m,10.f,14.f,22.2f,23.f,base,2.65f,roof);
 pwrLamp(m,{9.7f,22.2f},base+2.25f,1.3f,6.f,1150,-.4f,PwrFeeder,false,WorldLightMount::Wall,-kPi*.5f);
 pwrLamp(m,{14.3f,22.2f},base+2.25f,1.3f,6.f,4300,.4f,PwrFeeder,true,WorldLightMount::Wall,-kPi*.5f);
 // --- Route paint: the walkway lanes to each portal -------------------------------------------
 for(float x:{9.6f,14.3f})floorLine(m,x,1.2f,x+.1f,22.f,base,14);
 for(float y:{8.6f,14.3f}){floorLine(m,1.7f,y,9.6f,y+.1f,base,14);floorLine(m,14.4f,y,22.3f,y+.1f,base,14);}
 // --- The mimic board and its desk, west of the entry -------------------------------------------
 wall(m,1.f,1.f,8.6f,1.4f,base,base+3.f,2);
 wallSign(m,4.8f,1.455f,base+1.25f,"11 KV DISTRIBUTION","SUB A: DEAD    SWITCH HALL: DEAD    FEEDER 9: NO SUPPLY",7.f,0);
 for(float x:{2.6f,4.8f,7.f})m.fixtures.push_back({model::control_console,{x,2.4f},0,1.6f,.8f,1.1f,kPi,true});
 m.fixtures.push_back({model::office_chair,{4.8f,3.5f},0,.55f,.55f,.95f,kPi+.4f,true});
 terminal(m,{6.9f,3.6f},"DISTRIBUTION / SHIFT HANDOVER","00:58 T3 EARTH FAULT. SUB A ON T1/T2. 02:13 GRID LOST. ALL FEEDERS TRIPPED.","BLACK START: GENERATION FIRST, THEN SUB A BREAKERS, THEN LOAD CONTROL.");m.terminals.back().readable=Readable::Note;
 m.terminals.back().yaw=.001f;
 // --- Switchgear lineup, east of the entry; tall relay panels on the side walls ------------------
 pwrLineup(m,15.6f,4,1.47f,base,1.f);
 for(float y:{3.5f,5.f,6.5f})m.fixtures.push_back({model::pwr_tall_switch_cabinet,{22.3f,y},0,.69f,1.12f,2.2f,-kPi*.5f,true});
 for(float y:{16.f,17.5f})m.fixtures.push_back({model::pwr_control_cabinet,{1.42f,y+3.f},0,.8f,.4f,1.6f,kPi*.5f,true});
 // --- Columns, the overhead bus and its junction ----------------------------------------------------
 for(Vec2 c:{Vec2{6.f,6.f},Vec2{18.f,6.f},Vec2{6.f,18.f},Vec2{18.f,18.f}}){wall(m,c.x-.35f,c.y-.35f,c.x+.35f,c.y+.35f,base,roof,15);
  wall(m,c.x-.38f,c.y-.38f,c.x+.38f,c.y+.38f,base,base+1.1f,13);}
 pwrBusDuct(m,{12.f,1.2f},{12.f,21.8f},roof-1.7f,roof);
 pwrBusDuct(m,{1.8f,11.5f},{22.2f,11.5f},roof-2.4f,roof);
 wall(m,11.3f,10.8f,12.7f,12.2f,roof-2.6f,roof-1.4f,2);
 cableTray(m,{1.4f,15.2f},{1.4f,22.6f},base+3.4f);cableTray(m,{22.6f,15.2f},{22.6f,22.6f},base+3.4f);
 // --- The mail relay, south-west -----------------------------------------------------------------
 m.fixtures.push_back({model::pwr_tall_switch_cabinet,{1.55f,21.6f},0,.69f,1.12f,2.2f,kPi*.5f,true});
 m.fixtures.push_back({model::lab_status_display,{1.08f,19.6f},1.9f,.69f,.06f,.2f,kPi*.5f,false});
 terminal(m,{4.4f,21.f},"MAIL RELAY / NODE 9","CARRIER LOST ON FEEDER 9. 1 MESSAGE QUEUED.","DELIVERY RESUMES WHEN THE FEEDER IS LIVE.");
 {const StateId relay=stateId("pwr_relay_ms");
  TimedSequence s;s.timerState=relay;s.finishState=stateId("pwr_relay_done");s.x1=-4;s.y1=-4;s.x2=-3;s.y2=-3;
  s.durationMs=15000;s.finishAtMs=14000;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
  cue(m,"pwr_relay_ms",600,Sound::Pickup,{1.6f,17.4f},"MAIL RELAY 9: CARRIER RESTORED. DELIVERING 1 QUEUED MESSAGE.",.5f);
  cue(m,"pwr_relay_ms",3600,Sound::Metal1,{1.6f,17.4f},"FROM: LAB 4 / ENERGY DIAGNOSTICS / SENT 02:19",.2f);
  cue(m,"pwr_relay_ms",6600,Sound::Metal1,{1.6f,17.4f},"if you're alive don't come down to the labs.",.15f);
  cue(m,"pwr_relay_ms",9600,Sound::Metal1,{1.6f,17.4f},"go up.",.15f);
  ScriptEvent e;e.id=stateId("pwr_evt_relay");e.level=35;e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=PwrFeeder;
  ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=relay;a.value=1;e.actions.push_back(a);
  // The gate's interlock clunk wakes what was sleeping in the cable runs.
  e.actions.push_back(pwrSpawn(CreatureKind::Huntsman,{3.f,12.f},base));
  e.actions.push_back(pwrSpawn(CreatureKind::Huntsman,{21.f,16.f},base));
  m.scriptEvents.push_back(e);}
 // --- The dead technician by the eastern lane -----------------------------------------------
 machineryClock(m,"pwr_hub_body",60000);
 pwrBody(m,"pwr_hub_body",{17.4f,19.6f},base,2.2f);pwrBlood(m,{17.f,19.9f},base,.6f);
 m.fixtures.push_back({model::mine_hard_hat,{16.4f,20.6f},0,.28f,.35f,.15f,.6f,false});
 m.clutterSpawns.push_back({2,{19.2f,20.4f},base});m.clutterSpawns.push_back({0,{8.f,19.f},base,.5f});
 m.pickupSpawns.push_back({{20.4f,3.4f},PickupKind::Ammo,base});
 m.pickupSpawns.push_back({{2.6f,14.6f},PickupKind::Health,base});
 // --- Light: red emergency until the generators run, then the work lights -------------------------
 for(Vec2 l:{Vec2{12.f,5.f},Vec2{4.f,19.f},Vec2{20.f,19.f},Vec2{12.f,17.f}})pwrRed(m,l,roof-.5f,.9f,7.f);
 for(float y:{9.f,14.f})pwrRed(m,{22.38f,y-.2f+(y>10?.2f:0.f)},base+2.6f,.7f,4.f,WorldLightMount::Wall,kPi);
 for(Vec2 l:{Vec2{6.f,3.5f},Vec2{18.f,3.5f},Vec2{4.f,11.5f},Vec2{20.f,11.5f},Vec2{6.f,20.f},Vec2{18.f,20.f},Vec2{12.f,8.f},Vec2{12.f,15.f}})pwrWork(m,l,roof-.45f,2.f,11.f,5000,.08f);
 zone(m,"DISTRIBUTION HUB / MIMIC",1,1,9,5);
 zone(m,"DISTRIBUTION HUB",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// SUBSTATION A. Two transformers in fire-walled bays and the empty pad where
// T3 stood before it failed at 00:58. The switchgear lineup and its breaker
// desk stand on the basement floor; the entry gallery from the hub runs along
// the west wall at the hub's level. The two technicians on the permit were
// shot at the top of the gallery stair.
std::shared_ptr<const Map> makeSubstationA(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[36];m.definition=CampaignChunks[36];m.definition.ambient=.3f;
 m.definition.playerStart={2.2f,11.5f};m.definition.spawnHeight=-25;m.definition.spawnYaw=0;
 const float base=-28,deckZ=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'W',9.f,14.f,-25.f,false,0});openEdge(m,{'E',16.f,19.f,-28.f,false,0});
 // --- The west gallery at the hub's level, its stair and supports ------------------------------
 wall(m,0.f,9.f,1.f,14.f,base,deckZ-.22f,15);
 deck(m,deckZ,0,9,1,14);deck(m,deckZ,1,1,6,23);
 deckSupports(m,m.layers.back(),base,4.f);
 m.stairs.push_back({6.f,19.f,10.2f,21.f,base,deckZ,15,false,false});
 guardRail(m,5.92f,1.2f,6.f,19.f,deckZ);guardRail(m,5.92f,21.f,6.f,22.8f,deckZ);
 terminal(m,{3.4f,4.2f},"SUBSTATION A / PERMIT LOG","OYELARAN, VASK: ISOLATING T3 AFTER EARTH FAULT 00:58. SAFE TO WORK.","02:36. TWO IN CLEANUP GREY. THEY HAD OUR NAMES. THEY ASKED FOR BOTH OF US.",deckZ-base);m.terminals.back().readable=Readable::Note;
 m.terminals.back().yaw=.001f;
 // The technicians, where they were stopped at the top of the stair.
 machineryClock(m,"pwr_suba_bodies",60000);
 pwrBody(m,"pwr_suba_bodies",{3.2f,18.3f},deckZ,1.4f);pwrBody(m,"pwr_suba_bodies",{4.6f,20.4f},deckZ,3.9f);
 pwrBlood(m,{3.4f,18.6f},deckZ,.55f);pwrBlood(m,{4.4f,20.1f},deckZ,.45f);
 m.fixtures.push_back({model::pwr_tool_chest,{2.f,21.9f},deckZ-base,.75f,.45f,.97f,kPi*.5f,true});
 // --- The switchgear lineup and the breaker desk ---------------------------------------------
 pwrLineup(m,7.5f,6,5.3f,base,-1.f);
 wallSign(m,12.f,1.055f,base+2.6f,"SUBSTATION A / 11 KV BREAKERS","CLOSE ONLY WITH EMERGENCY GENERATION SYNCHRONISED",4.2f,0);
 terminal(m,{12.f,4.f},"SUBSTATION A / BREAKER DESK","E / CLOSE INCOMERS AND BUS SECTION. BUS IS DEAD WITHOUT GENERATION.","BLACK START: SYNCHRONISE EMERGENCY GENERATION, VIA THE CABLE TRENCH EAST.",0,PwrSubA,PwrGen);
 for(float x:{8.f,16.f})pwrBusDuct(m,{x,5.7f},{x,7.8f},roof-3.f,roof);
 // --- The transformer bays: fire walls, T1, T2 and the empty T3 pad -----------------------------
 for(float x:{7.f,13.f,19.f})wall(m,x-.2f,8.5f,x+.2f,15.4f,base,base+4.2f,15);
 wall(m,6.8f,8.3f,19.2f,8.5f,base,base+4.2f,15);
 m.fixtures.push_back({model::transformer,{10.f,12.f},0,4.4f,2.4f,2.6f,0,true});
 m.fixtures.push_back({model::transformer,{16.f,12.f},0,4.4f,2.4f,2.6f,0,true});
 wall(m,19.8f,9.4f,22.6f,14.6f,base,base+.15f,15);
 wall(m,20.2f,10.6f,22.2f,13.4f,base+.15f,base+.16f,8);
 for(float x:{8.4f,11.6f,14.4f,17.6f})m.fixtures.push_back({model::med_cell_bars,{x,15.55f},0,2.5f,.04f,2.4f,0,true});
 for(float x:{7.4f,13.4f}){Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=x;h.x2=x+5.2f;h.y1=8.7f;h.y2=15.3f;h.bottom=base;h.top=base+2.6f;h.damagePerSecond=40;h.enabledFlag=PwrGen;h.period=3.2f;h.onTime=1.1f;h.phase=x*.37f;m.hazards.push_back(h);}
 // --- The basement walk to the Cable Trench ----------------------------------------------------
 pwrMat(m,10.4f,19.f,23.f,21.f,base);
 pwrPortalY(m,22.4f,23.f,16.f,19.f,base,2.6f,roof);
 wall(m,23.f,16.f,24.f,19.f,base+2.6f,roof,15);
 cableTray(m,{11.f,22.6f},{22.6f,22.6f},base+2.8f);cableTray(m,{22.6f,1.4f},{22.6f,15.4f},base+3.2f);
 m.pickupSpawns.push_back({{3.f,16.f},PickupKind::Ammo,base});
 m.pickupSpawns.push_back({{21.f,3.f},PickupKind::Health,base});
 // The bus comes alive: something in the trench heard it.
 {ScriptEvent e;e.id=stateId("pwr_evt_suba_live");e.level=36;e.x1=10;e.y1=16;e.x2=24;e.y2=24;e.requireState=PwrGen;
  e.actions.push_back(pwrSpawn(CreatureKind::Huntsman,{20.5f,21.f},base));m.scriptEvents.push_back(e);}
 pwrRunning(m,"pwr_suba_hum",PwrGen,36,.35f,{13.f,12.f});
 // --- Light ----------------------------------------------------------------------------------
 // Dead, the substation has only its emergency bulkheads: on the gallery wall,
 // over the breaker desk, by the empty T3 pad and along the basement walk.
 for(float y:{5.f,16.5f})pwrRed(m,{1.02f,y},deckZ+2.5f,.9f,7.f,WorldLightMount::Wall,0);
 pwrRed(m,{17.5f,1.02f},base+2.5f,.9f,7.f,WorldLightMount::Wall,kPi*.5f);
 pwrRed(m,{22.98f,12.f},base+2.4f,.9f,7.f,WorldLightMount::Wall,kPi);
 pwrRed(m,{15.f,22.98f},base+2.3f,.8f,6.f,WorldLightMount::Wall,-kPi*.5f);
 for(Vec2 l:{Vec2{3.5f,4.f},Vec2{3.5f,12.f},Vec2{3.5f,20.f},Vec2{10.f,3.f},Vec2{16.f,3.f},Vec2{10.f,12.f},Vec2{16.f,12.f},Vec2{21.f,12.f},Vec2{16.f,20.f}})pwrWork(m,l,roof-.45f,2.f,11.f,5200,.05f);
 zone(m,"SUBSTATION A / GALLERY",0,0,6,24,deckZ-.5f,1000);
 zone(m,"SUBSTATION A / TRANSFORMER BAYS",6.8f,8.3f,23,15.6f);
 zone(m,"SUBSTATION A",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// SWITCH HALL. An 18 m shaft of switchgear: the 415 V boards stand on the pit
// floor and three catwalk levels climb the walls over them. Live after the
// black start: the boards arc on a cycle and the catwalks are the only way.
std::shared_ptr<const Map> makeSwitchHall(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[37];m.definition=CampaignChunks[37];m.definition.ambient=.3f;
 m.definition.playerStart={21.8f,11.f};m.definition.spawnHeight=-25;m.definition.spawnYaw=kPi;
 const float base=-28,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'E',9.f,14.f,-25.f,false,0});openEdge(m,{'S',10.f,14.f,-19.f,false,0});
 wall(m,23.f,9.f,24.f,14.f,base,-25.22f,15);
 // --- Catwalk A (-25): the entry platform and the west run ---------------------------------------
 deck(m,-25,16,9,24,14);deck(m,-25,2,9,16,12);
 m.stairs.push_back({2.f,12.f,4.f,16.2f,-25,-22,15,true,true});
 // --- Catwalk B (-22): the west landing and the run east ----------------------------------------
 deck(m,-22,1,16,22,19);deck(m,-22,18,14,22,16);
 m.stairs.push_back({19.f,10.f,21.f,14.2f,-22,-19,15,true,false});
 // --- Catwalk C (-19): the top landing and the bridge south to Load Control -----------------------
 deck(m,-19,17,6,23,10);deck(m,-19,10,6,17,8);deck(m,-19,10,8,14,24);
 for(Vec2 c:{Vec2{6.f,9.2f},Vec2{12.f,9.2f},Vec2{15.8f,11.8f},Vec2{6.f,18.8f},Vec2{16.f,18.8f},Vec2{22.8f,15.f},Vec2{17.2f,6.2f}})steelColumn(m,c.x,c.y,base,-18.5f,true);
 for(float x:{3.f,9.f,15.f,21.f})steelColumn(m,x,23.5f,base,-18.5f,true);
 for(float y:{2.f,8.f})steelColumn(m,1.5f,y,base,-18.5f,false);
 // --- The pit: two 415 V boards facing each other, live on a cycle after the black start ------------
 pwrLineup(m,4.f,5,4.4f,base,1.f);pwrLineup(m,6.5f,5,21.2f,base,-1.f);
 for(float y:{6.f,19.f}){Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=4.f;h.x2=13.f+(y>10?2.5f:0.f);h.y1=y-1.2f;h.y2=y+1.2f;h.bottom=base;h.top=base+2.4f;h.damagePerSecond=45;h.enabledFlag=PwrGen;h.period=2.6f;h.onTime=.8f;h.phase=y*.2f;m.hazards.push_back(h);}
 // --- Signage, risers and the cable shaft -----------------------------------------------------
 for(float x:{6.f,8.f})m.pipes.push_back({{x,1.3f},{x,1.3f},base,.11f,roof,6});
 cableTray(m,{22.6f,1.4f},{22.6f,8.6f},-21.f);cableTray(m,{1.4f,1.4f},{1.4f,8.f},-20.f);
 for(float z:{-24.f,-21.f})servicePipe(m,{22.75f,15.f},{22.75f,22.8f},z,.07f,3);
 // --- Creatures: a huntsman on the middle landing, a wasp in the shaft --------------------------------
 m.creatureSpawns.push_back({CreatureKind::Huntsman,{5.f,17.6f},-22});
 m.creatureSpawns.push_back({CreatureKind::Wasp,{20.f,8.f},-19});
 m.pickupSpawns.push_back({{2.5f,10.5f},PickupKind::Ammo,-25});
 m.pickupSpawns.push_back({{21.5f,7.f},PickupKind::Health,-19});
 // --- Light: each catwalk level has its own fittings -----------------------------------------
 // Emergency bulkheads on the walls at every level, so the dark climb reads in red.
 pwrRed(m,{22.98f,17.f},-23.f,.8f,5.f,WorldLightMount::Wall,kPi);
 pwrRed(m,{1.02f,12.5f},-22.6f,.9f,6.f,WorldLightMount::Wall,0);
 pwrRed(m,{1.02f,19.5f},-19.5f,.9f,6.f,WorldLightMount::Wall,0);
 pwrRed(m,{22.98f,6.5f},-16.6f,.9f,6.f,WorldLightMount::Wall,kPi);
 // With generation: bulkheads beside each run, and fittings hung under the
 // Load Control bridge over the two lower catwalks.
 for(auto [p,z,yaw]:{std::tuple{Vec2{22.98f,8.2f},-22.6f,kPi},{Vec2{1.02f,10.5f},-22.6f,0.f},{Vec2{1.02f,17.5f},-19.5f,0.f},
                     {Vec2{22.98f,16.3f},-19.5f,kPi},{Vec2{22.98f,8.f},-16.6f,kPi},{Vec2{9.4f,22.98f},-16.6f,-kPi*.5f},{Vec2{14.6f,22.98f},-16.6f,-kPi*.5f}})
  pwrWork(m,p,z,1.5f,8.f,4300,.25f,WorldLightMount::Wall,yaw);
 for(Vec2 l:{Vec2{12.f,10.5f},Vec2{12.f,17.5f}})pwrWork(m,l,-19.6f,1.6f,8.f,4300,.25f);
 for(Vec2 l:{Vec2{8.f,6.f},Vec2{8.f,19.f},Vec2{16.f,12.f}})pwrWork(m,l,-25.6f,1.6f,9.f,4300,.3f,WorldLightMount::Ceiling,0,PwrGen,.15f);
 for(Vec2 l:{Vec2{12.f,10.5f},Vec2{6.f,17.5f},Vec2{20.f,17.f},Vec2{12.f,20.f},Vec2{20.f,8.f}})pwrWork(m,l,roof-.45f,2.2f,12.f,5200,.05f);
 zone(m,"SWITCH HALL / PIT",0,0,24,24,base,base+2.6f);
 zone(m,"SWITCH HALL",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// CABLE TRENCH. The service tunnels under the substation: cable racks
// either side of a duckboard walk, a pull pit with its winch, a loop past a
// drain where water stands on live cable, and one stretch where the duct
// bank comes down low enough to crawl under. Dark: lamps only at junctions.
std::shared_ptr<const Map> makeCableTrench(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[38];m.definition=CampaignChunks[38];m.definition.ambient=.3f;m.definition.spawnHeight=-27.95f;
 const float base=-28,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 auto& L=m.layers[0];
 rectangle(L,1,1,23,23,'#');
 rectangle(L,1,16,18,19,'.');   // main walk from Substation A
 rectangle(L,14,16,18,23,'.');  // south leg to Emergency Generation
 rectangle(L,4,5,6,16,'.');     // north branch
 rectangle(L,2,2,10,6,'.');     // pull pit
 rectangle(L,6,8,20,10,'.');    // east loop
 rectangle(L,18,8,20,19,'.');
 openEdge(m,{'W',16.f,19.f,-28.f,false,0});openEdge(m,{'S',14.f,18.f,-28.f,false,0});
 // --- Racks along the walks ----------------------------------------------------------------------
 for(float z:{-27.f,-26.4f}){cableTray(m,{1.2f,16.12f},{3.8f,16.12f},z);cableTray(m,{6.2f,16.12f},{13.8f,16.12f},z);cableTray(m,{1.2f,18.88f},{13.8f,18.88f},z);
  cableTray(m,{14.12f,19.2f},{14.12f,22.8f},z);cableTray(m,{17.88f,19.2f},{17.88f,22.8f},z);
  cableTray(m,{6.2f,8.12f},{17.8f,8.12f},z);}
 for(float x=2.f;x<13.5f;x+=2.5f)m.fixtures.push_back({model::pwr_cable_bundle,{x,18.82f},.8f,2.4f,.14f,.22f,0,false});
 // Duckboards on the walk.
 for(float x=1.2f;x<13.8f;x+=.9f)wall(m,x,16.9f,x+.8f,18.1f,base,base+.05f,10);
 for(float y=19.2f;y+.8f<=24.f;y+=.9f)wall(m,15.f,y,17.f,y+.8f,base,base+.05f,10); // the last cover stops at the seam
 // --- The crawl: the duct bank drops across the walk ------------------------------------------------
 wall(m,8.5f,16.f,11.f,19.f,base+1.05f,roof,8);
 wallSign(m,8.445f,17.5f,base+1.12f,"DUCT BANK 4","LOW HEADROOM",1.2f,-kPi*.5f);
 // --- The pull pit: winch, drum, the jointer who never came back up -----------------------------
 m.fixtures.push_back({model::cable_reel,{3.6f,3.6f},0,.85f,1.7f,1.7f,kPi*.5f,true});
 m.fixtures.push_back({model::mine_portable_generator,{7.6f,3.f},0,.87f,1.04f,.82f,.3f,true});
 terminal(m,{8.6f,5.f},"CABLE TRENCH / PULL PIT 4","FEEDER 9 CABLES RE-ROUTED 01:30. MEGGER BEFORE ENERGISING.","EMERGENCY GENERATION: SOUTH LEG. KEEP OFF THE WET DRAIN, IT HUMS.");m.terminals.back().readable=Readable::Letter;
 m.terminals.back().yaw=-kPi*.5f;
 machineryClock(m,"pwr_trench_body",60000);
 pwrBody(m,"pwr_trench_body",{5.f,3.3f},base,.4f);
 m.pickupSpawns.push_back({{2.6f,4.8f},PickupKind::Health,base});
 // --- The wet drain on the loop: standing water on live cable ---------------------------------------
 m.waterVolumes.push_back({10.f,8.f,14.f,10.f,-28.45f,-28.1f});
 {Hazard h;h.kind=Hazard::Kind::Anomaly;h.x1=10.2f;h.x2=13.8f;h.y1=8.1f;h.y2=9.9f;h.bottom=-28.1f;h.top=-26.f;h.damagePerSecond=3;m.hazards.push_back(h);}
 {Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=10.4f;h.x2=13.6f;h.y1=8.2f;h.y2=9.8f;h.bottom=-28.6f;h.top=-26.8f;h.damagePerSecond=30;h.period=4.f;h.onTime=1.f;m.hazards.push_back(h);}
 m.pickupSpawns.push_back({{19.f,9.f},PickupKind::Ammo,base});
 // --- Creatures ------------------------------------------------------------------------------
 m.creatureSpawns.push_back({CreatureKind::Huntsman,{8.f,9.f},base});
 m.creatureSpawns.push_back({CreatureKind::Huntsman,{16.f,21.5f},base});
 // --- Light: junction lamps only ------------------------------------------------------------------
 for(Vec2 l:{Vec2{5.f,17.5f},Vec2{16.f,17.5f},Vec2{5.f,4.f},Vec2{19.f,12.f}})pwrRed(m,l,roof-.15f,.8f,5.f);
 for(Vec2 l:{Vec2{2.5f,17.5f},Vec2{12.f,17.5f},Vec2{16.f,21.f},Vec2{5.f,11.f},Vec2{8.f,4.f},Vec2{15.f,9.f}})pwrWork(m,l,roof-.12f,1.4f,6.f,4300,.3f,WorldLightMount::Ceiling,0,PwrGen,.35f);
 zone(m,"CABLE TRENCH / PULL PIT 4",2,2,10,6);
 zone(m,"CABLE TRENCH",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// EMERGENCY GENERATION. Three diesel sets in a hall that has not run since
// the monthly test. Black start in three steps, each answered by the plant:
// the day tanks' valves clank and the transfer pump runs until Tank 2 is
// full; Set 2's starter turns it over until it coughs and catches; closing it
// onto the dead emergency bus drops the red lamps and, a second later, the
// work lights strike bank by bank. The battery room keeps the controls
// alive. A gallery shutter on the west wall opens onto the Machine Hall once
// Feeder 9 is live: the way back is short.
std::shared_ptr<const Map> makeEmergencyGeneration(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[39];m.definition=CampaignChunks[39];m.definition.ambient=.3f;
 const float base=-28,deckZ=-25,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),base));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'N',14.f,18.f,-28.f,false,0});openEdge(m,{'W',8.f,12.f,-25.f,false,0});
 const StateId fuel=stateId("pwr_fuel_transfer"),crank=stateId("pwr_set2_cranked");
 // --- The west gallery and its shortcut door to the Machine Hall ------------------------------------
 wall(m,0.f,8.f,1.f,12.f,base,deckZ-.22f,15);
 deck(m,deckZ,0,8,1,12);deck(m,deckZ,1,4,5,15);
 deckSupports(m,m.layers.back(),base,4.f);
 m.stairs.push_back({1.f,15.f,3.f,19.2f,base,deckZ,15,true,false});
 {Door d{1.f,3.f,14.75f,0,false,false,false,deckZ-base};d.requireState=PwrFeeder;m.doors.push_back(d);}
 wall(m,3.f,14.6f,5.f,14.9f,deckZ,deckZ+2.65f,3);
 wall(m,1.f,14.6f,5.f,14.9f,deckZ+2.65f,deckZ+3.3f,3);
 // --- The day tanks and the fuel transfer panel, north-east --------------------------------------
 m.fixtures.push_back({model::storage_tank,{21.f,3.6f},.2f,3.f,1.4f,1.8f,kPi*.5f,true});
 m.fixtures.push_back({model::storage_tank,{21.f,8.1f},.2f,3.f,1.4f,1.8f,kPi*.5f,true});
 wall(m,19.4f,1.f,23.f,10.4f,base,base+.2f,15);
 floorLine(m,19.3f,1.2f,19.4f,10.4f,base,13);
 terminal(m,{19.f,6.f},"DAY TANKS / FUEL TRANSFER","DAY TANK 2 AT 3%. BULK FEED VALVES SHUT SINCE THE MONTHLY TEST.","E / OPEN THE BULK VALVES AND RUN THE TRANSFER PUMP.",0,fuel);
 m.terminals.back().yaw=-kPi*.5f;
 pwrAnswer(m,"pwr_fuel_ms",fuel,39,20000,{20.4f,6.f},
  {{10,Sound::Metal2,{19.6f,3.6f},.6f,.8f},{380,Sound::Metal2,{19.6f,8.1f},.6f,.75f},{900,Sound::LiftMotor,{20.4f,6.f},.35f,1.3f},
   {1300,Sound::WaterReturn,{21.f,5.8f},.3f,1.f},{19900,Sound::Metal1,{20.4f,6.f},.4f,.7f}},
  700,19000,Sound::Machine,.28f,1.2f);
 // --- The three sets -------------------------------------------------------------------------
 for(float x:{7.f,13.f,19.f}){wall(m,x-2.2f,15.2f,x+2.2f,18.6f,base,base+.25f,15);
  m.fixtures.push_back({model::pwr_diesel_genset,{x,16.9f},.25f,3.7f,1.7f,2.f,0,true});
  m.fixtures.push_back({model::flue,{x+1.4f,17.9f},2.25f,.5f,.5f,roof-base-2.6f,0,false});}
 terminal(m,{13.f,13.8f},"SET 2 / LOCAL PANEL","FUEL RAIL DRY. BATTERY 24.6 V. NOTHING TO PRIME FROM UNTIL DAY TANK 2 FILLS.","E / PRIME THE RAIL AND CRANK ON THE BATTERY.",0,crank,fuel);
 // The starter turns it over twice; it coughs, catches and settles to a run.
 const StateId caught=stateId("pwr_set2_ms_done");
 pwrAnswer(m,"pwr_set2_ms",crank,39,1600,{13.f,16.9f},
  {{10,Sound::LiftMotor,{13.f,16.f},.7f,.7f},{620,Sound::Metal3,{13.f,16.6f},.5f,.8f},{860,Sound::LiftMotor,{13.f,16.f},.7f,.78f},{1520,Sound::LiftCrash,{13.f,16.9f},.45f,.6f}});
 pwrOnState(m,"pwr_evt_set2_caught",39,caught,stateId("pwr_set2_shook"),1,.12f);
 pwrRunning(m,"pwr_set2_running",caught,39,.7f,{13.f,16.9f});
 // --- The control desk: synchronise to the emergency bus ---------------------------------------------
 for(float x:{10.4f,11.6f,14.4f,15.6f})m.fixtures.push_back({model::pwr_control_cabinet,{x,7.85f},0,.8f,.4f,1.6f,0,true});
 terminal(m,{13.f,7.f},"EMERGENCY GENERATION / CONTROL","EMERGENCY BUS DEAD. IT CLOSES ONLY ONTO A SET RUNNING ON SPEED.","E / DEAD-BUS CLOSE SET 2 ONTO THE EMERGENCY BUS.",0,PwrGen,caught);
 m.terminals.back().yaw=kPi;
 pwrAnswer(m,"pwr_strike_ms",PwrGen,39,950,{13.f,8.f},
  {{10,Sound::LiftCrash,{13.f,7.6f},.8f,.7f},{760,Sound::Metal3,{7.f,5.6f},.5f,.9f},{880,Sound::Metal3,{16.f,5.6f},.5f,.85f},{940,Sound::Machine,{13.f,12.f},.5f,.45f}});
 pwrOnState(m,"pwr_evt_lc_mains_39",39,PwrGen,PwrLcMains,1);
 wallSign(m,8.5f,1.055f,base+2.4f,"BLACK START PROCEDURE","1 FUEL / 2 CRANK SET 2 / 3 SYNCHRONISE / 4 SUB A",4.f,0);
 {ScriptEvent e;e.id=stateId("pwr_evt_gen_online");e.level=39;e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=PwrGen;
  ScriptAction s;s.type=ScriptAction::Type::Shake;s.amount=.25f;e.actions.push_back(s);
  ScriptAction c;c.type=ScriptAction::Type::Checkpoint;e.actions.push_back(c);m.scriptEvents.push_back(e);}
 // --- The battery room, south-east -------------------------------------------------------------------
 wall(m,16.f,9.8f,23.f,10.f,base,base+2.8f,3);
 for(float y:{11.6f,13.f})m.fixtures.push_back({model::pwr_tall_switch_cabinet,{21.9f,y},0,.69f,1.12f,2.2f,-kPi*.5f,true});
 // --- The stores corner, south-west, and what waits there --------------------------------------
 for(Vec2 d:{Vec2{2.2f,21.4f},Vec2{3.f,22.2f},Vec2{4.1f,21.6f}})m.fixtures.push_back({model::drum,d,0,.6f,.6f,.9f,0,true});
 m.fixtures.push_back({model::metal_shelf,{6.3f,22.6f},0,1.8f,.5f,1.8f,kPi,true});
 m.pickupSpawns.push_back({{4.8f,20.f},PickupKind::Ammo,base});
 m.pickupSpawns.push_back({{9.6f,3.f},PickupKind::Health,base});
 m.creatureSpawns.push_back({CreatureKind::Brute,{6.f,10.f},base});
 m.creatureSpawns.push_back({CreatureKind::Huntsman,{20.f,20.5f},base});
 // --- Services and light -------------------------------------------------------------------------
 for(float y:{4.f,12.f})wall(m,1.f,y-.15f,23.f,y+.15f,roof-.5f,roof,12);
 cableTray(m,{6.f,1.4f},{18.f,1.4f},base+3.f);
 for(Vec2 l:{Vec2{13.f,6.f},Vec2{5.f,10.f},Vec2{13.f,20.f},Vec2{20.f,15.f}})pwrRed(m,l,roof-.5f,1.f,8.f);
 // The gallery deck shades the ceiling reds off the west walls: two low reds
 // under it, so the walls read before the sets are running.
 for(Vec2 l:{Vec2{1.6f,6.f},Vec2{3.2f,12.6f}})pwrRed(m,l,deckZ-.4f,.7f,5.f);
 for(Vec2 l:{Vec2{7.f,5.6f},Vec2{16.f,5.6f},Vec2{7.f,10.4f},Vec2{13.f,11.f},Vec2{7.f,20.5f},Vec2{13.f,20.5f},Vec2{19.f,20.5f},Vec2{20.f,13.f}})pwrWork(m,l,roof-.45f,2.f,11.f,5000,.05f,WorldLightMount::Ceiling,0,PwrStrike);
 zone(m,"EMERGENCY GENERATION / GALLERY",0,4,6,15,deckZ-.5f,1000);
 zone(m,"EMERGENCY GENERATION / BATTERY ROOM",16,10,23,15);
 zone(m,"EMERGENCY GENERATION",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// LOAD CONTROL. The district control room: operators' desks facing the
// mimic wall, the supervisor's office, and the three transfer stations that
// shed load, move the plant onto generation and energise Feeder 9. Cleanup
// Team 3 was told to hold it and keep the feeder open.
std::shared_ptr<const Map> makeLoadControl(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[40];m.definition=CampaignChunks[40];m.definition.ambient=.24f;
 const float f=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 m.wallSurface=SurfaceOfficePlaster;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceOfficePlaster;
 openEdge(m,{'N',10.f,14.f,-19.f,false,0});
 // --- Vestibule: the operators' door frame and the clock-in board --------------------------------
 for(float x:{8.6f,14.f})wall(m,x,3.8f,x+1.4f,4.1f,f,roof,21);
 wall(m,1.f,3.8f,8.6f,4.1f,f,roof,21);wall(m,15.4f,3.8f,23.f,4.1f,f,roof,21);
 wall(m,10.f,3.8f,14.f,4.1f,f+2.6f,roof,21);
 m.fixtures.push_back({model::water_cooler,{2.f,1.7f},0,.4f,.4f,1.2f,0,true});
 m.fixtures.push_back({model::file_cabinet_tall,{21.8f,1.6f},0,.5f,.6f,1.4f,kPi,true});
 wallSign(m,6.f,1.055f,f+1.3f,"CLOCK IN","SHIFT 2: HALE, OKAFOR, BRANDT / OUT: ---",2.f,0);
 // --- The mimic wall and the three transfer stations ---------------------------------------------
 wall(m,1.f,22.3f,23.f,23.f,f,f+2.9f,2);
 // The single-line diagram the desks face: the 11 kV bus across the top fed
 // by Substation A's two transformers and the emergency set, seven feeders
 // dropping to the 415 V boards, each through its breaker; Feeder 9's breaker
 // striped where Cleanup locked it open. Washed by two lamps on the room's supply.
 {const float face=22.28f,t=.03f;auto strip=[&](float x1,float z1,float x2,float z2,int mat){wall(m,x1,face-t,x2,face,f+z1,f+z2,mat);};
  wall(m,1.6f,face-.012f,22.4f,face,f+1.1f,f+2.82f,20);
  strip(2.5f,2.42f,21.5f,2.5f,18);strip(3.f,1.5f,21.f,1.56f,19);
  for(float x:{6.f,12.f,18.f})strip(x-.04f,2.5f,x+.04f,2.78f,19);
  for(float x:{3.5f,6.5f,9.5f,12.5f,15.5f,18.5f,20.8f}){strip(x-.03f,1.56f,x+.03f,2.42f,18);
   wall(m,x-.08f,face-.045f,x+.08f,face,f+2.02f,f+2.18f,x>18.f&&x<19.f?13:8);}
  for(Vec2 l:{Vec2{6.5f,face},Vec2{17.5f,face}})pwrWork(m,l,f+2.62f,1.f,5.f,5600,.1f,WorldLightMount::Wall,-kPi*.5f,PwrLcMains);}
 const StateId ready=stateId("pwr_transfer_ready"),bus=stateId("pwr_transfer_bus");
 terminal(m,{5.f,21.f},"LOAD CONTROL / 1 SHED","PLANT DEMAND 36 MW. SET 2 CARRIES 2. NO SUPPLY HERE UNTIL SUBSTATION A CLOSES.","E / SHED ALL BUT LIFE SAFETY AND THE FEEDERS.",0,ready,PwrSubA);
 terminal(m,{12.f,21.f},"LOAD CONTROL / 2 TRANSFER","PLANT STILL ON THE DEAD GRID. SET 2 TRIPS IF IT TAKES UNSHED LOAD.","E / TRANSFER THE PLANT TO EMERGENCY GENERATION.",0,bus,ready);
 terminal(m,{19.f,21.f},"LOAD CONTROL / 3 FEEDER 9","FEEDER 9 OPEN BY CLEANUP ORDER. IT FEEDS THE MACHINE HALL AND THE HUB GATE.","E / CLOSE FEEDER 9 ONTO GENERATION.",0,PwrFeeder,bus);
 // The room answers: breakers trip down the mimic wall as load sheds; the
 // transfer breaker drops the room dark for a breath and brings it back on
 // generation; Feeder 9 closes with a slam and the hall's load comes on.
 pwrAnswer(m,"pwr_lc_shed_ms",ready,40,1400,{12.f,22.6f},
  {{10,Sound::Metal1,{3.f,22.6f},.5f,1.f},{180,Sound::Metal2,{7.f,22.6f},.5f,.9f},{330,Sound::Metal1,{11.f,22.6f},.5f,1.1f},
   {520,Sound::Metal2,{15.f,22.6f},.5f,.95f},{700,Sound::Metal1,{19.f,22.6f},.5f,1.05f},{900,Sound::Metal4,{22.f,22.6f},.45f,.9f},{1300,Sound::Machine,{12.f,22.f},.4f,.4f}});
 pwrOnState(m,"pwr_evt_lc_mains",40,PwrGen,PwrLcMains,1);
 pwrOnState(m,"pwr_evt_lc_dip",40,bus,PwrLcMains,2);
 pwrAnswer(m,"pwr_lc_dip_ms",bus,40,750,{12.f,22.6f},{{10,Sound::LiftCrash,{12.f,22.4f},.7f,.7f},{700,Sound::Metal3,{12.f,13.f},.5f,.9f},{740,Sound::Machine,{12.f,22.f},.45f,.5f}});
 pwrOnState(m,"pwr_evt_lc_dip_back",40,stateId("pwr_lc_dip_ms_done"),PwrLcMains,1,.1f);
 pwrAnswer(m,"pwr_lc_feeder_ms",PwrFeeder,40,1500,{19.f,22.6f},{{10,Sound::LiftCrash,{19.f,22.4f},.8f,.6f},{450,Sound::Machine,{12.f,22.f},.5f,.5f}});
 // --- Operators' desks, facing the mimic wall -----------------------------------------------------
 for(int i=0;i<4;++i){workstation(m,4.f+i*5.f,11.f,f,1,i);workstation(m,4.f+i*5.f,15.5f,f,1,i+2);}
 // --- Supervisor's office, north-east ----------------------------------------------------------------
 office(m,19.f,6.2f,f,"LOAD CONTROL / SUPERVISOR","CLEANUP ORDER 02:31: FEEDER 9 STAYS OPEN. NO RESTORATION.","HALE: SIGNED UNDER PROTEST. OKAFOR WENT DOWN TO THE MACHINE HALL.");
 // --- Cleanup Team 3: three of them, holding the room -----------------------------------------
 machineryClock(m,"pwr_team3_clock",60000);
 const std::vector<ActorPatrolPoint> beats[3]={
  {{{6.f,19.f},f,3500,kPi*.5f},{{3.f,13.f},f,3000,kPi}},
  {{{12.f,8.5f},f,4000,-kPi*.5f},{{17.f,13.f},f,2500,0}},
  {{{20.5f,18.f},f,3000,kPi*.5f},{{21.f,9.f},f,3200,-kPi*.5f}}};
 for(int i=0;i<3;++i){const Vec2 start=beats[i].front().position;
  ActorTrack t;t.timerState=stateId("pwr_team3_clock");t.visual=i==1?ActorVisual::CleanupHeavy:ActorVisual::Cleanup;t.scale=1.6f;t.health=90;
  t.damageState=stateId("pwr_team3_damage_"+std::to_string(i));t.deadState=stateId("pwr_team3_dead_"+std::to_string(i));
  t.keys={{0,start,f,beats[i].front().look},{60000,start,f,beats[i].front().look,0,1}};
  auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=i==1?GuardSquadRole::Heavy:GuardSquadRole::Assault;ai.faction=AiFaction::Raider;ai.squad=stateId("pwr_team3");ai.armed=true;
  ai.weapon=i==2?2:1;ai.sightRange=12;ai.sightDot=.5f;ai.searchMs=6000;ai.speed=1.5f;ai.suppress=i==1;
  ai.patrol=beats[i];m.actorTracks.push_back(std::move(t));}
 m.pickupSpawns.push_back({{2.f,20.5f},PickupKind::Health,f});
 m.pickupSpawns.push_back({{22.f,20.5f},PickupKind::Ammo,f});
 // --- Ceiling and light -----------------------------------------------------------------------------
 ceilingGrid(m,1.f,4.1f,23.f,22.3f,roof);
 for(Vec2 l:{Vec2{6.f,2.f},Vec2{18.f,2.f}})m.lights.push_back(lamp(l,roof-.1f,1.f,6.f,3800));
 for(Vec2 l:{Vec2{4.f,7.f},Vec2{12.f,7.f},Vec2{4.f,13.f},Vec2{12.f,13.f},Vec2{20.f,13.f},Vec2{4.f,19.f},Vec2{12.f,19.f},Vec2{20.f,19.f}}){
  pwrRed(m,l,roof-.1f,.6f,5.f);pwrWork(m,l,roof-.1f,1.5f,8.f,4300,.3f,WorldLightMount::Ceiling,0,PwrLcMains,.05f);}
 zone(m,"LOAD CONTROL / SUPERVISOR",15,4,23,9);
 zone(m,"LOAD CONTROL",0,0,24,24);
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

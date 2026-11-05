// ANALYSIS LABORATORIES (chapter 2): Analytical Chemistry (81) and Instrumentation (82).
// Split from CampaignChapterAnalysis.cpp; buildChapterAnalysis there assigns them.
#include "CampaignChapterAnalysisKit.h"
namespace retro {
using namespace kit;
using namespace an;
namespace {
// --- The labs' own fittings (lb): built the way the Medical chapter builds its rooms -------------
// A ceiling tube that works: the emitter clear below its recessed fitting.
WorldLight lbTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;}
// A fitting with its tubes dead: the housing alone, flush to the ceiling.
void lbDeadTube(Map& m,float x,float y,float roof,float yaw=0){anFix(m,model::clinic_ceiling_lamp_2,x,y,roof-.17f,.44f,1.04f,.17f,yaw,false);}
// A lockdown beacon: a red wall lamp that blinks while `state` is unset and goes out once it is set;
// amber for a cycle that never finished.
WorldLight lbBeacon(Vec2 p,float z,float yaw,float phase,const char* state,bool amber=false){
 auto l=lamp(p,z,amber?.36f:.32f,amber?3.6f:4.f,3000,0,WorldLightMount::Wall,yaw,true,amber?Point3{1.f,.5f,.06f}:Point3{1.f,.12f,.06f});
 l.period=amber?1.f:1.4f;l.onTime=.5f*l.period;l.phase=phase;l.enabledState=stateId(state);l.invert=true;return l;}
// Somebody who did not get out: a posed body on the map's clock.
void lbBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
// An industrial pendant: a drop rod from the roof to its reflector head at z, lit unless intensity is 0.
void lbPendant(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float flicker=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});anFix(m,model::pendant_lamp,p.x,p.y,z,.5f,.5f,.34f,0,false);
 if(intensity>0){auto l=lamp(p,z-.05f,intensity,range,kelvin,0,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);}}
// A fitted bench run along its longer side: kick plinth, steel carcass, drawer lines, worktop at f+h.
void lbCounter(Map& m,float x1,float y1,float x2,float y2,float f,float h=.9f,int worktop=28){
 const float top=f+h;
 wall(m,x1+.05f,y1+.05f,x2-.05f,y2-.05f,f,f+.08f,8);wall(m,x1,y1,x2,y2,f+.08f,top-.04f,20);wall(m,x1,y1,x2,y2,top-.04f,top,worktop);
 if(x2-x1>=y2-y1)for(float y:{y1-.006f,y2-.004f}){wall(m,x1+.02f,y,x2-.02f,y+.01f,top-.3f,top-.29f,8);for(float x=x1+.6f;x<x2-.3f;x+=.6f)wall(m,x,y,x+.01f,y+.01f,f+.1f,top-.06f,8);}
 else for(float x:{x1-.006f,x2-.004f}){wall(m,x,y1+.02f,x+.01f,y2-.02f,top-.3f,top-.29f,8);for(float y=y1+.6f;y<y2-.3f;y+=.6f)wall(m,x,y,x+.01f,y+.01f,f+.1f,top-.06f,8);}
}
// A lab island along x: two benches back to back (north edge y), the reagent spine between them with
// its two shelves, gas taps and the service risers to the ceiling, a sink set in one end. Nobody
// climbs onto the spine's top shelf.
void lbIsland(Map& m,float x1,float x2,float y,float f,float roof,bool sinkEast){
 lbCounter(m,x1,y,x2,y+.65f,f);lbCounter(m,x1,y+.75f,x2,y+1.4f,f);
 wall(m,x1+.1f,y+.65f,x2-.1f,y+.75f,f+.9f,f+1.85f,28);
 for(float z:{1.32f,1.8f})wall(m,x1+.15f,y+.5f,x2-.15f,y+.9f,f+z,f+z+.03f,20);
 wall(m,x1+.1f,y+.5f,x2-.1f,y+.9f,f+2.1f,f+2.95f,30);
 for(float x=x1+.6f;x<x2-.3f;x+=1.2f)for(float s:{-.09f,.09f})m.pipes.push_back({{x,y+.7f+s},{x,y+.7f+s},f+.9f,.012f,f+1.03f,5});
 for(float x:{x1+.3f,x2-.3f})m.pipes.push_back({{x,y+.7f},{x,y+.7f},f+1.85f,.035f,roof,6});
 const float s=sinkEast?x2-.95f:x1+.45f;wall(m,s,y+.1f,s+.5f,y+.52f,f+.86f,f+.902f,8);
 m.pipes.push_back({{s+.25f,y+.6f},{s+.25f,y+.6f},f+.9f,.014f,f+1.03f,5});m.pipes.push_back({{s+.25f,y+.6f},{s+.25f,y+.5f},f+1.03f,.012f,f+1.03f,5});
}
// Reagents on a spine's two shelves from x1 to x2 (spine centre y): jars and bottles in runs with gaps.
void lbReagents(Map& m,float x1,float x2,float y,float f,int seed){
 int k=seed;for(float x=x1;x<x2;x+=.4f,++k)for(int t=0;t<2;++t){const float z=f+(t?1.83f:1.35f);
  switch((k+t*3)%5){case 0:case 3:kit::prop(m,model::med_specimen_jar,x,y+(k%2?.125f:-.125f),z,0);break;
   case 1:for(int b=0;b<2;++b)kit::prop(m,model::med_pill_bottle,x-.05f+b*.1f,y+(b?.12f:-.12f),z,.07f,.03f,.13f,b*.2f);break;default:break;}}
}
// Bulk stock on a metal shelf at (x,y), along y or along x: boxes, bottles and jars on its three boards.
void lbStock(Map& m,float x,float y,bool alongY,int seed){
 shelf(m,{x,y},alongY?kPi*.5f:0.f,0);const float f=m.layers.front().elevation;
 wall(m,x-(alongY?.25f:.9f),y-(alongY?.9f:.25f),x+(alongY?.25f:.9f),y+(alongY?.9f:.25f),f+1.95f,m.definition.ceiling,30);   // nobody sits on its top board
 for(int t=0;t<3;++t){const float z=f+(t==0?.306f:t==1?.972f:1.656f);
  for(int i=0;i<3;++i){if((i+t+seed)%4==3)continue;const float a=-.58f+i*.58f,px=alongY?x:x+a,py=alongY?y+a:y;
   switch((i*3+t+seed)%5){
    case 1:for(int b=0;b<3;++b)kit::prop(m,model::med_pill_bottle,px+(alongY?0.f:(b-1)*.13f),py+(alongY?(b-1)*.13f:0.f),z,.07f,.03f,.13f,b*.7f+seed);break;
    case 3:for(int b=0;b<2;++b)kit::prop(m,model::med_specimen_jar,px+(alongY?0.f:(b?.11f:-.11f)),py+(alongY?(b?.11f:-.11f):0.f),z,b*.9f+seed);break;
    default:kit::prop(m,model::prop_cardboard_box,px,py,z,.436f,.432f,.22f,(i+t)%2?0.f:.06f);}}}
}
// Full cells on a metal shelf at (x,y), along y or along x: two or three to a board, gaps where some went out.
void lbCellShelf(Map& m,float x,float y,bool alongY,int seed){
 shelf(m,{x,y},alongY?kPi*.5f:0.f,0);const float f=m.layers.front().elevation;
 wall(m,x-(alongY?.25f:.9f),y-(alongY?.9f:.25f),x+(alongY?.25f:.9f),y+(alongY?.9f:.25f),f+2.1f,m.definition.ceiling,30);
 for(int t=0;t<3;++t){const float z=f+(t==0?.306f:t==1?.972f:1.656f);
  for(int i=0;i<4;++i){if((i*2+t+seed)%3==0)continue;const float a=-.54f+i*.36f;
   kit::prop(m,model::anl_cell_charged,alongY?x:x+a,alongY?y+a:y,z,(i+t+seed)*.9f);}}
}
// Glass on the floor where something was dropped: slivers scattered round (x,y).
void lbShards(Map& m,float x,float y,float f,unsigned seed){
 unsigned h=seed*2654435761u+7;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return (h&0xffffu)/65535.f;};
 for(int i=0;i<9;++i){const float r=.05f+next()*.38f,a=next()*6.283f,px=x+std::cos(a)*r,py=y+std::sin(a)*r,w=.02f+next()*.04f;
  wall(m,px-w,py-.018f,px+w,py+.018f,f+.002f,f+.006f,17);}
}
}

// ANALYTICAL CHEMISTRY (manifest 51). Where the intake's material was put into
// solution to find out what it was. It would not stay dissolved: whatever they
// did to it, it came back, the same mass and the same shape. Hood 3 held the
// last assay. At 02:22 it vented, the lab sealed itself and the extract fans
// stalled; the south door waits on a clean-air reading that only the purge in
// Instrumentation (west) can give it. Four suits went out, three came back.
// Route: the N door -> the suit room or decon -> the lab -> W to Instrumentation
// (the purge) -> back -> the S door, unsealed.
std::shared_ptr<const Map> makeAnalyticalChemistry(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[81];m.definition=CampaignChunks[81];m.definition.ambient=.20f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});                                                      // door 0: in from intake
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,true,stateId("lab_chem_seal")});                               // door 1: the lockdown
 machineryClock(m,"anl_chem_clock",60000);
 auto crash=[&](float x1,float x2,float y1,float y2){wall(m,x1,y1,x2,y2,f+.86f,f+.96f,20);};
 // the room's own walls where things hang on them
 wall(m,1.f,.9f,10.f,1.f,f,roof,3);wall(m,14.f,.9f,23.f,1.f,f,roof,3);
 wall(m,.9f,1.f,1.f,9.f,f,roof,3);wall(m,.9f,14.f,1.f,23.f,f,roof,3);wall(m,22.9f,1.f,23.f,23.f,f,roof,3);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,1.f,9.f,4.8f},{9.2f,1.f,14.8f,4.8f},{15.f,1.f,22.9f,4.8f},{1.f,5.f,22.9f,16.f},{1.f,16.2f,9.f,23.f},{9.2f,16.2f,14.8f,23.f},{15.f,16.2f,22.9f,23.f}})
  ceilingGrid(m,x1,y1,x2,y2,roof);
 // --- The vestibule: in from intake, a door either side, the line into the lab ---------------------
 // The suit room and decon are the airlocks either side of the line: open from the vestibule, each
 // shut from the lab by its own door in the lab's wall (a leaf hangs only in a wall along x).
 wall(m,9.f,1.f,9.2f,2.4f,f,roof,3);doorwayY(m,2.4f,3.6f,9.f,9.2f,f,roof);wall(m,9.f,3.6f,9.2f,5.f,f,roof,3);
 wall(m,14.8f,1.f,15.f,2.4f,f,roof,3);doorwayY(m,2.4f,3.6f,14.8f,15.f,f,roof);wall(m,14.8f,3.6f,15.f,5.f,f,roof,3);
 wallDoor(m,1.f,10.8f,4.8f,5.f,4.15f,5.35f,f,roof,-1);wallDoor(m,13.2f,23.f,4.8f,5.f,18.6f,19.8f,f,roof,-1);wall(m,10.8f,4.8f,13.2f,5.f,f+2.4f,roof,3);
 for(float x:{10.72f,13.2f})wall(m,x,4.76f,x+.08f,5.04f,f,f+2.4f,20);
 wall(m,10.72f,4.76f,13.28f,5.04f,f+2.36f,f+2.4f,20);
 for(float x:{11.1f,11.7f,12.3f,12.9f})wall(m,x,4.86f,x+.06f,4.94f,f+2.3f,f+2.36f,8);      // the air curtain's heads, dry since the fans stalled
 m.lights.push_back(lbTube(12.f,2.8f,roof,.5f,5.f,5600,.03f));
 m.lights.push_back(lbBeacon({10.2f,4.78f},f+2.45f,-kPi*.5f,0,"lab_chem_seal"));
 wallSign(m,12.f,4.79f,f+2.75f,"SUITS PAST THIS LINE","RESPIRATOR AND OVERSUIT BEYOND THE YELLOW LINE",2.2f,anN);
 floorLine(m,9.2f,4.5f,14.8f,4.62f,f,13);
 anFix(m,model::lab_id_scanner,13.75f,4.77f,f+1.1f,.3f,.06f,.41f,anN,false);
 anFix(m,model::med_first_aid_box,9.26f,1.75f,f+1.3f,.367f,.12f,.349f,anE,false);
 extinguisher(m,14.8f,1.7f,f,anW);
 kit::prop(m,model::item_dosimeter,11.4f,3.9f,f,.31f,.146f,.066f,2.3f);                    // dropped on the way out
 // --- The suit room (west): lockers, the bench, the rail with one hook bare ------------------------
 for(int i=0;i<5;++i)anFix(m,model::psx_locker,1.5f+i*.86f,1.33f,f,.64f,.85f,2.42f,-kPi*.5f,true);
 wall(m,1.05f,1.f,5.4f,1.7f,f+2.42f,roof,30);
 bench(m,3.6f,3.2f,f);
 anFix(m,model::med_scrub_sink,1.3f,4.1f,f,.49f,.54f,1.13f,anE,true);
 wall(m,1.f,3.8f,1.16f,4.4f,f+1.42f,f+1.45f,20);for(float y:{3.85f,4.37f})wall(m,1.f,y,1.16f,y+.02f,f+1.32f,f+1.42f,8);   // the shelf over the sink
 for(auto [y,yaw]:{std::pair{3.95f,.3f},{4.05f,1.1f},{4.25f,2.f}})kit::prop(m,model::med_pill_bottle,1.08f,y,f+1.45f,.07f,.03f,.13f,yaw);
 anFix(m,model::med_floor_drain,3.4f,4.15f,f,.5f,.5f,.01f,0,false);
 anFix(m,model::med_notice_board,1.035f,2.75f,f+1.25f,1.2f,.07f,.9f,anE,false);
 anFix(m,model::med_wall_clock,8.99f,4.25f,f+2.05f,.24f,.02f,.23f,anW,false);
 wall(m,5.6f,4.74f,8.4f,4.8f,f+1.9f,f+1.94f,20);                                            // the suit rail
 for(float x:{5.95f,6.65f,7.35f,8.05f})wall(m,x-.015f,4.66f,x+.015f,4.8f,f+1.82f,f+1.94f,8);  // its four hooks
 for(auto [x,turn]:{std::pair{5.95f,-.16f},{6.65f,.1f},{7.35f,-.06f}}){hungSuit(m,{x,4.58f,f+1.48f},-kPi*.5f+turn);wall(m,x-.012f,4.6f,x+.012f,4.64f,f+1.66f,f+1.83f,8);m.structures.back().solid=false;}   // three back, hung by their hood loops; the fourth hook bare
 kit::prop(m,model::item_dosimeter,3.1f,3.15f,f+.51f,.31f,.146f,.066f,.4f);
 kit::prop(m,model::item_walkie,5.9f,3.4f,f,.114f,.376f,.046f,2.1f);
 kit::prop(m,model::item_id_card,2.5f,3.75f,f,.132f,.085f,.002f,.9f);
 m.lights.push_back(lbTube(5.4f,2.9f,roof,.55f,5.5f,3800,0));
 m.lights.push_back(lbTube(2.3f,2.6f,roof,.4f,4.5f,3800,0));
 taped(m,{8.99f,1.9f},f+1.5f,anW,Readable::Note,"SUIT COUNT / 02:30","FOUR SUITS OUT AFTER THE VENT. THREE BACK. MORAN WENT BACK IN FOR HER NOTEBOOK.",
  "SHE TOOK HER HOOD OFF IN THERE. SHE SAID THE AIR WAS FINE. SHE SAID IT TWICE, THE SAME WAY.");
 m.pickupSpawns.push_back({{7.8f,4.2f},PickupKind::Health,f});
 // --- Decon (east): three tiled stalls, roses on their risers, drains, the hose, the bench ---------
 wall(m,15.f,1.f,22.9f,1.06f,f,f+2.1f,27);wall(m,22.84f,1.06f,22.9f,4.8f,f,f+2.1f,27);
 for(float x:{16.6f,18.8f,21.f}){wall(m,x-.04f,1.06f,x+.04f,3.2f,f,f+2.1f,27);wall(m,x-.05f,1.06f,x+.05f,3.2f,f+2.1f,f+2.13f,20);
  wall(m,x+.06f,3.08f,x+.2f,3.2f,f+.24f,f+2.3f,39);wall(m,x+.12f,3.12f,x+.24f,3.22f,f+.28f,f+2.26f,39);}   // curtains drawn back
 for(float c:{17.7f,19.9f,21.95f}){m.pipes.push_back({{c,1.13f},{c,1.13f},f+2.17f,.02f,roof,5});
  wall(m,c-.09f,1.06f,c+.09f,1.26f,f+2.14f,f+2.2f,20);wall(m,c-.05f,1.06f,c+.05f,1.12f,f+1.15f,f+1.27f,20);   // rose, valve
  anFix(m,model::med_floor_drain,c,2.1f,f,.5f,.5f,.01f,0,false);}
 m.pipes.push_back({{16.6f,3.15f},{22.84f,3.15f},f+2.35f,.012f,-999,5});for(float x:{16.6f,22.8f})m.pipes.push_back({{x,3.15f},{x,3.15f},f+2.35f,.007f,roof,5});   // the curtain track on its rods
 wall(m,16.56f,3.2f,22.84f,3.26f,f,f+.06f,20);                                               // the stalls' kerb
 bench(m,21.3f,4.35f,f);
 anFix(m,model::med_scrub_sink,15.3f,4.35f,f,.49f,.54f,1.13f,anE,true);
 kit::prop(m,model::trash_can,16.f,4.45f,f,.35f,.35f,.6f,.3f);                             // the suit bin
 anModel(m,model::lab_status_display,19.2f,4.771f,f+2.6f,anN,false);                       // the cycle read-out over the lab door
 wall(m,15.f,1.5f,15.07f,1.62f,f+.86f,f+.96f,4);                                             // the hose tap, its hose run out to the drain
 m.pipes.push_back({{15.04f,1.56f},{15.04f,1.56f},f+.88f,.02f,f+.03f,3});
 m.pipes.push_back({{15.04f,1.56f},{15.7f,2.2f},f+.03f,.02f,f+.03f,3});m.pipes.push_back({{15.7f,2.2f},{16.3f,1.9f},f+.03f,.02f,f+.03f,3});
 // what the three who came back shed: a suit peeled down, a badge, a dosimeter
 wall(m,19.7f,3.55f,20.4f,4.05f,f,f+.05f,18);wall(m,19.85f,3.65f,20.25f,3.95f,f+.05f,f+.11f,18);
 kit::prop(m,model::item_id_card,20.65f,3.75f,f,.132f,.085f,.002f,1.2f);
 kit::prop(m,model::item_dosimeter,21.f,4.33f,f+.51f,.31f,.146f,.066f,.3f);
 for(float c:{17.7f,19.9f,21.95f})wall(m,c-.2f,1.06f,c+.2f,1.18f,f+1.4f,f+1.43f,20);                // soap shelves
 for(float x:{16.6f,18.8f,21.f})for(float s:{-.06f,.04f})wall(m,x+s,1.5f,x+s+.02f,2.6f,f+1.f,f+1.03f,20);   // grab rails
 floorLine(m,15.f,2.4f,15.12f,3.6f,f,13);
 floorLine(m,16.56f,3.3f,22.84f,3.42f,f,13);                                                  // stand in the stall, not past it
 // the supply over the stalls: the manifold along the tiles, its gauge, the valve nobody closed
 m.pipes.push_back({{15.2f,1.15f},{22.8f,1.15f},f+2.5f,.035f,-999,5});
 for(float x:{15.2f,22.8f})m.pipes.push_back({{x,1.15f},{x,1.15f},f+2.5f,.035f,roof,5});
 anModel(m,model::lab_pressure_gauge,15.75f,1.115f,f+1.75f,anS,false);
 anFix(m,model::pwr_gate_valve,16.15f,1.2f,f+2.38f,.49f,.28f,.33f,anS,false);
 // the cycle lamp over the lab door: amber, and it never cleared
 m.lights.push_back(lbBeacon({18.1f,4.78f},f+2.45f,-kPi*.5f,.2f,"lab_chem_seal",true));
 wallSign(m,16.4f,4.79f,f+2.2f,"HOOD ON UNTIL THE LAMP CLEARS","RINSE UNDER THE ROSE / SUIT IN THE BIN / THEN UNHOOD",1.6f,anN);
 m.lights.push_back(lbTube(17.7f,2.4f,roof,.7f,6.f,6000,.05f));m.lights.push_back(lbTube(21.f,2.4f,roof,.6f,5.5f,6000,.05f,.3f));
 // --- The lab: the instrument wall, two island pairs, the hoods ---------------------------------
 // The instrument wall (north): bench runs either side of both doors and the R-41 terminal, lab-panel
 // splashbacks, an instrument shelf or wall cupboards over them, a cable tray over all of it.
 for(auto [a,b,h]:{std::tuple{1.2f,3.8f,1.55f},{5.7f,10.6f,1.75f},{13.4f,16.3f,1.55f}}){lbCounter(m,a,5.f,b,5.66f,f);wall(m,a,5.f,b,5.03f,f+.9f,f+h,28);}
 for(auto [a,b]:{std::pair{1.2f,3.8f},{13.4f,16.3f}}){wall(m,a,5.f,b,5.32f,f+1.62f,f+1.65f,20);
  for(float x:{a+.05f,b-.07f})wall(m,x,5.f,x+.02f,5.28f,f+1.5f,f+1.62f,8);wall(m,a,5.f,b,5.32f,f+2.1f,roof,30);}
 wall(m,5.7f,5.f,10.6f,5.36f,f+1.75f,f+2.45f,20);wall(m,5.7f,5.f,10.6f,5.36f,f+2.45f,roof,30);   // wall cupboards
 for(float x=6.3f;x<10.5f;x+=.6f)wall(m,x,5.36f,x+.01f,5.37f,f+1.78f,f+2.42f,8);
 for(float x=6.f;x<10.5f;x+=.6f)wall(m,x+.2f,5.36f,x+.26f,5.39f,f+1.8f,f+1.84f,8);
 cableTray(m,{1.2f,5.2f},{20.6f,5.2f},f+2.95f);
 kit::prop(m,model::crt_monitor,2.1f,5.24f,f+.9f,.42f,.4f,.38f,0);kit::prop(m,model::keyboard,2.1f,5.56f,f+.9f,.45f,.16f,.03f,.05f);
 kit::prop(m,model::lab_field_meter,3.35f,5.3f,f+.9f,.25f,.328f,.148f,-.3f);kit::prop(m,model::office_paper_stack,2.8f,5.33f,f+.9f,.33f,.44f,.03f,1.5f);
 for(int i=0;i<6;++i)kit::prop(m,model::office_file_binder,1.4f+i*.1f,5.16f,f+1.65f,.093f,.301f,.424f,0);
 for(float x:{2.45f,2.75f,3.05f})kit::prop(m,model::med_specimen_jar,x,5.16f,f+1.65f,x);
 anFix(m,model::lab_test_chamber,6.55f,5.34f,f+.9f,1.14f,.68f,.64f,anS,false);              // the furnace R-41 came back out of
 m.lights.push_back(lamp({6.55f,5.6f},f+1.7f,.25f,1.8f,3200,0,WorldLightMount::Bare,0,false));   // its warm task light
 anModel(m,model::lab_scope_monitor,8.1f,5.24f,f+.9f,anS,false);
 kit::prop(m,model::lab_microscope,9.4f,5.35f,f+.9f,.4f);kit::prop(m,model::office_paper_stack,8.85f,5.33f,f+.9f,.33f,.44f,.03f,1.6f);
 for(auto [x,y]:{std::pair{9.85f,5.2f},{10.15f,5.36f}})kit::prop(m,model::med_specimen_jar,x,y,f+.9f,x);
 anModel(m,model::lab_instrument_terminal,14.1f,5.24f,f+.9f,anS,false,.8f);
 kit::prop(m,model::crt_monitor,15.15f,5.24f,f+.9f,.42f,.4f,.38f,0);kit::prop(m,model::keyboard,15.15f,5.56f,f+.9f,.45f,.16f,.03f,0);
 const int deskLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,15.95f,5.3f,f+.9f,.18f,.4f,.52f,-.5f);
 for(int i=0;i<5;++i)kit::prop(m,model::office_file_binder,13.6f+i*.1f,5.16f,f+1.65f,.093f,.301f,.424f,0);
 for(float x:{14.6f,15.f})kit::prop(m,model::med_specimen_jar,x,5.16f,f+1.65f,x);
 for(float x:{2.1f,14.65f})kit::prop(m,model::office_chair,x,6.35f,f,.55f,.55f,.95f,x*.1f-.6f);
 terminal(m,{16.8f,5.5f},"ASSAY / R-41 IN SOLUTION","DISSOLVED IN 6M HCL 01:40. RECOVERED BY EVAPORATION 02:00: 100.0% OF THE MASS. SAME CRYSTAL HABIT AS THE INTAKE PIECE.",
  "REPEATED IN NITRIC, IN AQUA REGIA, IN THE FURNACE. IT COMES BACK. IT COMES BACK THE SAME SHAPE. MOVED TO HOOD 3 FOR THE VAPOUR.");
 m.terminals.back().yaw=0;                                                                   // its screen to the lab, not the wall
 anModel(m,model::lab_sample_fridge,17.75f,5.43f,f,anS,true);
 anModel(m,model::lab_status_display,17.75f,5.03f,f+1.55f,anS,false);
 anFix(m,model::med_scrub_sink,20.35f,5.3f,f,.49f,.54f,1.13f,anS,true);                     // the eyewash by the hoods
 m.pipes.push_back({{20.35f,5.08f},{20.35f,5.08f},f+1.13f,.03f,roof,3});
 anModel(m,model::lab_wall_clock,12.f,5.016f,f+2.45f,anS,false);
 // The west wall: the glass-fronted reagent cabinet, the stalled extract grille, the PA, the specimen cabinet.
 anModel(m,model::office_display_cabinet,1.27f,7.4f,f,anE,true);wall(m,1.f,6.92f,1.54f,7.88f,f+2.1f,roof,30);
 m.lights.push_back(lamp({1.85f,7.4f},f+1.95f,.18f,1.4f,4000,0,WorldLightMount::Bare,0,false));   // a light on the cabinet's glass, so the jars read
 anFix(m,model::pwr_vent_grille,1.03f,6.f,f+2.3f,.819f,.04f,.8f,anE,false);
 anFix(m,model::lab_pa_horn,1.29f,8.6f,f+2.5f,.495f,.564f,.433f,anE,false);
 anModel(m,model::lab_specimen_cabinet,1.37f,15.2f,f,anE,true);wall(m,1.f,14.53f,1.74f,15.87f,f+2.19f,roof,30);
 // The south wall: crash rails, the boards, the safety shower and eyewash, the extinguishers.
 crash(1.8f,5.5f,15.96f,16.f);crash(6.9f,7.2f,15.96f,16.f);crash(8.f,10.f,15.96f,16.f);crash(14.f,16.5f,15.96f,16.f);crash(17.9f,20.7f,15.96f,16.f);
 anFix(m,model::med_notice_board,3.4f,15.965f,f+1.25f,1.2f,.07f,.9f,anN,false);
 anFix(m,model::med_scrub_sink,7.6f,15.73f,f,.49f,.54f,1.13f,anN,true);
 m.pipes.push_back({{7.6f,15.94f},{7.6f,15.94f},f+1.13f,.03f,f+2.25f,3});
 wall(m,7.35f,15.5f,7.85f,16.f,f+2.25f,f+2.3f,20);wall(m,7.35f,15.5f,7.85f,16.f,f+2.3f,roof,30);   // the shower's head; nobody stands on it
 m.pipes.push_back({{7.75f,15.62f},{7.75f,15.62f},f+1.75f,.006f,f+2.25f,5});wall(m,7.7f,15.6f,7.8f,15.64f,f+1.7f,f+1.75f,19);   // its pull
 floorLine(m,6.95f,14.95f,8.25f,15.96f,f,13);
 extinguisher(m,9.6f,16.f,f,anN);
 m.lights.push_back(lbBeacon({9.6f,15.98f},f+2.3f,-kPi*.5f,.7f,"lab_chem_seal"));
 anFix(m,model::med_first_aid_box,14.75f,15.94f,f+1.3f,.367f,.12f,.349f,anN,false);
 anFix(m,model::lab_notice_board,19.25f,15.965f,f+1.25f,1.2f,.07f,.9f,anN,false);
 extinguisher(m,20.4f,16.f,f,anN);
 // The islands: two pairs either side of the aisle from the line to the lockdown.
 lbIsland(m,3.4f,10.2f,8.1f,f,roof,true);lbIsland(m,3.4f,10.2f,12.1f,f,roof,false);
 lbIsland(m,14.6f,18.8f,8.1f,f,roof,false);lbIsland(m,14.6f,18.8f,12.1f,f,roof,true);
 lbReagents(m,3.8f,9.6f,8.8f,f,1);lbReagents(m,3.8f,9.6f,12.8f,f,4);lbReagents(m,15.f,18.2f,8.8f,f,2);lbReagents(m,15.f,18.2f,12.8f,f,7);
 for(auto [x,y,mdl,yaw]:{std::tuple{5.8f,8.4f,model::lab_microscope,anN},{4.6f,9.2f,model::lab_field_meter,.4f},{7.9f,9.25f,model::lab_microscope,.2f},
   {6.6f,12.35f,model::lab_microscope,anN},{8.4f,13.2f,model::lab_field_meter,2.6f},{15.6f,8.4f,model::lab_field_meter,-.3f},{17.5f,9.15f,model::lab_microscope,.1f},
   {16.2f,12.4f,model::lab_microscope,anN+.3f}})kit::prop(m,mdl,x,y,f+.9f,yaw);
 for(auto [x,y,yaw]:{std::tuple{5.7f,9.2f,.3f},{9.f,12.4f,1.2f},{15.2f,13.2f,.8f},{18.f,8.45f,2.1f}})kit::prop(m,model::office_paper_stack,x,y,f+.9f,.33f,.44f,.03f,yaw);
 for(auto [x,y,yaw]:{std::tuple{7.f,8.35f,.2f},{4.3f,12.4f,2.4f},{17.2f,12.35f,.6f}})kit::prop(m,model::office_file_binder,x,y,f+.9f,.093f,.301f,.424f,yaw);
 kit::prop(m,model::prop_cardboard_box_open,9.6f,13.15f,f+.9f,.36f,.36f,.36f,.3f);
 for(auto [x,y,yaw]:{std::tuple{5.f,10.12f,.25f},{8.3f,10.15f,-.4f},{7.4f,7.48f,kPi+.2f},{6.f,11.48f,kPi-.3f},{16.f,10.12f,.2f},{17.4f,11.48f,kPi+.4f},{4.1f,14.12f,-.2f}})
  kit::prop(m,model::office_chair,x,y,f,.55f,.55f,.95f,yaw);
 // the hoods: three bays on the east wall, sashes down, the stainless chases and extract stacks between
 // them; hood 3 is the one
 for(auto [a,b]:{std::pair{5.f,5.6f},{8.f,9.2f},{11.6f,12.8f},{15.2f,16.f}})wall(m,20.7f,a,22.9f,b,f,roof,28);
 for(int i=0;i<3;++i){const float y1=5.6f+i*3.6f,y2=y1+2.4f,yc=(y1+y2)*.5f;
  for(float y:{y1,y2-.1f})wall(m,20.7f,y,22.9f,y+.1f,f,f+2.5f,28);                           // cheeks
  wall(m,20.8f,y1+.1f,22.9f,y2-.1f,f,f+.9f,28);wall(m,20.8f,y1+.1f,22.9f,y2-.1f,f+.9f,f+.94f,20); // base and worktop
  wall(m,22.84f,y1+.1f,22.9f,y2-.1f,f+.94f,f+2.1f,20);                                         // the steel back
  for(float z:{1.3f,1.7f})wall(m,22.8f,y1+.3f,22.84f,y2-.3f,f+z,f+z+.03f,8);                   // its baffle slots
  wall(m,20.7f,y1+.1f,22.9f,y2-.1f,f+2.1f,f+2.5f,28);                                         // canopy
  glazing(m,20.7f,y1+.1f,20.78f,y2-.1f,f+.94f,f+2.1f,28,0.f,1.16f);                            // the sash
  wall(m,20.64f,y1+.4f,20.7f,y2-.4f,f+1.02f,f+1.05f,20);                                       // its pull
  anModel(m,model::lab_pressure_gauge,20.76f,y1+.3f,f+2.15f,anW,false);
  m.structures.push_back({20.8f,y1+.1f,22.9f,y2-.1f,f+1.45f,f+1.85f,false,CollisionProxyMaterial});   // nobody climbs in a hood
  m.pipes.push_back({{21.8f,yc},{21.8f,yc},f+2.5f,.17f,f+2.95f,2});m.pipes.push_back({{21.8f,yc},{21.8f,yc},f+2.6f,.2f,f+2.68f,6});   // extract stack, damper
  auto l=lamp({21.85f,yc},f+2.f,i==2?.45f:.38f,2.8f,i==2?6500.f:5200.f,0,WorldLightMount::Ceiling,0,true,i==2?Point3{.78f,.5f,1.f}:Point3{1,1,1});   // the hood's own tube
  l.fitting=model::med_fluoro_light;l.fittingWidth=.11f;l.fittingDepth=1.2f;l.fittingHeight=.13f;if(i==2)l.flicker=.25f;m.lights.push_back(l);}
 wall(m,21.4f,5.6f,22.2f,15.2f,f+2.95f,f+3.3f,20);                                            // the duct the stalled fans pull on
 for(float y:{6.6f,10.2f,13.8f})m.pipes.push_back({{21.45f,y},{21.45f,y},f+3.3f,.012f,roof,6});
 // on the chases: the airflow alarm and the fans' isolator; an eyewash on the next
 anFix(m,model::lab_status_display,20.67f,8.6f,f+1.75f,.693f,.058f,.2f,anW,false);
 anFix(m,model::pwr_disconnect_switch,20.52f,8.6f,f+.95f,.43f,.35f,.85f,anW,false);
 anFix(m,model::med_scrub_sink,20.43f,12.2f,f,.49f,.54f,1.13f,anW,true);
 m.pipes.push_back({{20.65f,12.2f},{20.65f,12.2f},f+1.13f,.03f,roof,3});
 // hood 3: lines condensed on the inside of the glass, the sample behind it, the shimmer
 for(float z:{1.4f,1.62f})wall(m,20.79f,13.2f+z*.3f,20.8f,14.6f+z*.2f,f+z,f+z+.006f,17);
 anFix(m,model::lab_anomaly_relic,22.1f,14.2f,f+.94f,.3f,.24f,.24f,.7f,false);                // R-41, what came back
 m.lights.push_back(lamp({21.9f,14.1f},f+1.15f,.3f,2.2f,6500,0,WorldLightMount::Bare,0,false,{.75f,.5f,1.f}));m.lights.back().flicker=.35f;
 {Hazard h;h.kind=Hazard::Kind::Anomaly;h.x1=20.8f;h.y1=12.9f;h.x2=22.9f;h.y2=15.1f;h.bottom=f+.94f;h.top=f+2.1f;h.damagePerSecond=0;m.hazards.push_back(h);}
 anHum(m,"anl_hood_hum",18.5f,12.f,22.9f,16.f,f,f+2.5f,.45f);
 taped(m,{20.79f,14.f},f+.55f,anW,Readable::Note,"HOOD 3 / SASH DOWN","THE VAPOUR CONDENSES ON THE INSIDE OF THE GLASS IN LINES. THE LINES MOVE WHEN YOU WALK PAST.",
  "THEY FOLLOW THE TORCH. KEEP IT OFF IN HERE. DO NOT RAISE THE SASH. EXTRACT FANS STALLED 02:22.");
 // hood 1: a spent cell opened for its residue, the top collar off, the cage beside it;
 // hood 2: a full one, still sealed, faintly lit from inside. Nobody here would open a full one.
 kit::prop(m,model::anl_cell_core,21.9f,6.5f,f+.94f,.4f);kit::prop(m,model::anl_cell_cap,21.3f,7.3f,f+.94f,2.2f);
 {const int cell=int(m.clutterSpawns.size());kit::prop(m,model::anl_cell_charged,22.2f,9.85f,f+.94f,.9f);
  m.lights.push_back(lamp({22.2f,9.85f},f+1.25f,.16f,1.4f,6500,0,WorldLightMount::Bare,0,false,{.75f,.5f,1.f}));m.lights.back().prop=cell;}
 taped(m,{20.79f,7.4f},f+.55f,anW,Readable::Note,"HOOD 1 / CELL 0447 RESIDUE","SPENT CELL OPENED FOR THE DEPOSIT ON THE WINDOW FLOOR. SAME CRYSTAL HABIT AS R-41.",
  "SO WHAT IS IN THE FULL ONES IS WHAT CAME UP FROM THE FACE. WE HAVE BEEN SELLING IT AS POWER.");
 // Moran's notebook, where she went back for it
 kit::prop(m,model::office_file_binder,21.6f,10.4f,f+.94f,.6f);
 // The way they left it: paper off the benches, a chair over in the aisle, a flask dropped and stepped in.
 paperDrift(m,2.f,5.9f,20.4f,15.7f,f,14,8101);
 paperDrift(m,3.6f,8.15f,10.f,8.6f,f+.9f,3,8102,false);paperDrift(m,14.8f,13.f,18.6f,13.45f,f+.9f,2,8103,false);
 propPose(m,model::office_chair,12.4f,10.6f,f,.55f,.55f,.95f,2.2f,kPi*.5f,0);
 lbShards(m,11.3f,11.6f,f,81);propPose(m,model::med_specimen_jar,11.75f,11.35f,f,.5f,kPi*.5f,0);
 propPose(m,model::office_file_binder,13.2f,7.9f,f,1.2f,kPi*.5f,0);
 kit::prop(m,model::item_id_card,11.4f,6.4f,f,.132f,.085f,.002f,.4f);kit::prop(m,model::item_dosimeter,19.6f,6.9f,f,.31f,.146f,.066f,1.4f);
 // Light: cold over the islands, the one by hood 3 failing, warmer down the aisle, one fitting dead;
 // the beacons and hood 3's violet are the only colour in it.
 for(auto [x,y]:{std::pair{5.f,8.8f},{8.6f,8.8f},{5.f,12.8f},{8.6f,12.8f},{16.7f,8.8f}})m.lights.push_back(lbTube(x,y,roof,.6f,6.f,6000,.04f));
 m.lights.push_back(lbTube(16.7f,12.8f,roof,.5f,5.5f,6000,.04f,.35f));
 for(float y:{7.1f,13.6f})m.lights.push_back(lbTube(12.4f,y,roof,.52f,5.5f,4600,.02f));
 m.lights.push_back(lbTube(19.7f,7.2f,roof,.48f,5.f,5200,.03f));
 for(float y:{7.2f,11.5f,14.7f})m.lights.push_back(lbTube(2.2f,y,roof,.45f,5.f,5200,.03f));   // the west aisle and its cabinets
 lbDeadTube(m,19.7f,11.f,roof);
 m.lights.push_back(lamp({15.95f,5.45f},f+1.35f,.3f,2.2f,2800,0,WorldLightMount::Bare));m.lights.back().prop=deskLamp;
 m.lights.push_back(lbBeacon({11.f,5.02f},f+2.6f,kPi*.5f,.35f,"lab_chem_seal"));
 // --- The south wall: the lobby to the lockdown door, the stores either side --------------------
 // the stores open off the lab, each by its door in the lab's south wall, opening in
 wallDoor(m,1.f,10.f,16.f,16.2f,5.6f,6.8f,f,roof,1);wallDoor(m,14.f,23.f,16.f,16.2f,16.6f,17.8f,f,roof,1);wall(m,10.f,16.f,14.f,16.2f,f+2.4f,roof,3);
 wall(m,9.f,16.2f,9.2f,23.f,f,roof,3);wall(m,14.8f,16.2f,15.f,23.f,f,roof,3);
 wall(m,9.2f,22.9f,10.f,23.f,f,roof,3);wall(m,14.f,22.9f,14.8f,23.f,f,roof,3);
 // The lockdown lobby: the held door, its beacons, the dead purge inlets, the line nobody crossed.
 m.lights.push_back(lbTube(12.f,19.f,roof,.45f,5.f,4400,0));
 m.lights.push_back(lbTube(12.f,21.9f,roof,.3f,4.f,4400,0,.55f));                              // going, over the door
 // the one who ran south: he got as far as the held door, and the extinguisher he beat it with
 lbBody(m,"anl_chem_clock",ActorVisual::LabTech,{11.3f,21.9f},f,kPi*.5f+.35f);
 propPose(m,model::med_extinguisher,12.75f,22.3f,f,.24f,.16f,.66f,1.3f,kPi*.5f,0);
 kit::prop(m,model::item_id_card,10.6f,21.2f,f,.132f,.085f,.002f,2.2f);
 // the walls: a rail each side, the respirator cabinet emptied, the gas head that called the lockdown
 crash(9.2f,9.24f,16.4f,17.5f);crash(9.2f,9.24f,18.8f,21.1f);crash(14.76f,14.8f,16.4f,22.f);
 anFix(m,model::med_notice_board,14.765f,19.9f,f+1.25f,1.2f,.07f,.9f,anW,false);              // the evacuation plan, south door ringed
 anModel(m,model::med_supply_cabinet,9.42f,18.15f,f,anE,true);wall(m,9.2f,17.6f,9.64f,18.7f,f+1.7f,roof,30);
 anFix(m,model::lab_test_chamber,13.9f,18.4f,f+2.1f,.684f,.408f,.384f,kPi*.5f,false);
 m.pipes.push_back({{13.9f,18.4f},{13.9f,18.4f},f+2.48f,.04f,roof,6});for(float z:{2.48f,2.9f})wall(m,13.83f,18.33f,13.97f,18.47f,f+z,f+z+.03f,20);
 for(auto [x,yaw,phase]:{std::tuple{9.22f,0.f,.2f},{14.78f,kPi,.9f}})m.lights.push_back(lbBeacon({x,22.2f},f+2.3f,yaw,phase,"lab_chem_seal"));
 terminal(m,{9.45f,21.6f},"CHEMICAL LOCKDOWN / SOUTH DOOR","FUME RELEASE HOOD 3 02:22. LAB SEALED. EXTRACT FANS STALLED. DOOR HELD UNTIL CLEAN AIR.",
  "PURGE TO BE RUN FROM INSTRUMENTATION, WEST. DOOR RELEASES ON THE CLEAN READING.");
 anModel(m,model::lab_status_display,9.23f,20.3f,f+2.1f,anE,false);
 floorLine(m,10.f,22.45f,14.f,22.7f,f,13);
 anFix(m,model::lab_id_scanner,14.77f,22.3f,f+1.1f,.3f,.06f,.41f,anW,false);
 for(auto [x,yaw]:{std::pair{9.22f,anE},{14.78f,anW}})anFix(m,model::pwr_vent_grille,x,18.6f,f+2.4f,.819f,.04f,.8f,yaw,false);
 anFix(m,model::lab_pa_horn,9.48f,16.6f,f+2.5f,.495f,.564f,.433f,anE,false);
 servicePipe(m,{14.45f,16.25f},{14.45f,22.8f},roof-.3f,.05f,3);
 paperDrift(m,9.6f,16.5f,14.4f,22.2f,f,5,8104);
 kit::prop(m,model::item_dosimeter,13.2f,21.1f,f,.31f,.146f,.066f,.8f);
 // the solvent store (west): block-built, the drums on their bund, the stock, the yellow cabinet, the fourth suit
 wall(m,1.f,16.2f,5.5f,16.26f,f,roof,31);wall(m,6.9f,16.2f,9.f,16.26f,f,roof,31);wall(m,5.5f,16.2f,6.9f,16.26f,f+2.55f,roof,31);
 wall(m,1.f,16.26f,1.06f,23.f,f,roof,31);wall(m,1.06f,22.94f,8.94f,23.f,f,roof,31);wall(m,8.94f,16.26f,9.f,23.f,f,roof,31);
 equipmentPad(m,1.15f,16.4f,3.9f,18.1f,f);
 for(auto [x,y]:{std::pair{1.75f,16.95f},{2.35f,16.95f},{1.75f,17.55f},{3.25f,17.3f}})anModel(m,model::ind_barrel,x,y,f+.1f,x,true);
 propPose(m,model::ind_barrel,4.5f,18.7f,f,.4f,kPi*.5f,0);                                       // knocked over on the way out
 lbStock(m,1.36f,19.2f,true,3);lbStock(m,1.36f,21.4f,true,8);
 lbStock(m,4.5f,22.6f,false,5);lbStock(m,6.5f,22.6f,false,11);
 wall(m,8.36f,17.9f,8.94f,19.1f,f,f+1.75f,18);wall(m,8.35f,18.49f,8.36f,18.51f,f+.1f,f+1.65f,8);   // the flammables cabinet
 wall(m,8.345f,17.95f,8.36f,19.05f,f+1.2f,f+1.3f,13);for(float y:{18.35f,18.6f})wall(m,8.32f,y,8.35f,y+.05f,f+.9f,f+1.1f,8);
 wall(m,8.36f,17.9f,8.94f,19.1f,f+1.75f,roof,30);
 anFix(m,model::med_first_aid_box,4.5f,16.32f,f+1.3f,.367f,.12f,.349f,anS,false);
 m.pipes.push_back({{1.06f,22.86f},{8.94f,22.86f},f+2.6f,.035f,-999,3});                         // the solvent main along the back wall
 for(float x:{3.1f,5.5f,7.5f}){m.pipes.push_back({{x,22.86f},{x,22.86f},f+2.1f,.02f,f+2.6f,3});anFix(m,model::pwr_gate_valve,x,22.8f,f+2.05f,.245f,.14f,.165f,anN,false);}
 anFix(m,model::pwr_fire_cabinet,8.84f,20.6f,f+1.1f,.252f,.203f,.559f,anW,false);
 for(auto [x,fl]:{std::pair{3.f,0.f},{7.7f,.5f}}){m.lights.push_back(lamp({x,16.28f},f+2.3f,.65f,6.5f,3600,0,WorldLightMount::Wall,kPi*.5f));m.lights.back().flicker=fl;}
 m.lights.push_back(lbTube(3.f,21.f,roof,.52f,5.5f,4000,0));m.lights.push_back(lbTube(7.3f,21.6f,roof,.42f,4.5f,4000,0));
 m.lights.push_back(lbTube(5.f,20.4f,roof,.3f,4.f,4000,0,.6f));                                 // stuttering over her
 m.pickupSpawns.push_back({{7.6f,20.6f},PickupKind::Ammo,f});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{5.f,20.f},-999,1,0});                // Moran, still in her suit
 // the spill store (east): block-built, absorbent, the spill cart, the gas cylinders chained up
 wall(m,15.f,16.2f,16.5f,16.26f,f,roof,31);wall(m,17.9f,16.2f,22.9f,16.26f,f,roof,31);wall(m,16.5f,16.2f,17.9f,16.26f,f+2.55f,roof,31);
 wall(m,15.f,16.26f,15.06f,23.f,f,roof,31);wall(m,15.06f,22.94f,22.84f,23.f,f,roof,31);wall(m,22.84f,16.26f,22.9f,22.94f,f,roof,31);
 equipmentPad(m,21.15f,16.4f,22.8f,18.1f,f);
 for(auto [x,y]:{std::pair{21.75f,16.95f},{22.3f,17.55f},{21.75f,17.55f}})anModel(m,model::ind_barrel,x,y,f+.1f,y,true);
 for(float y:{19.6f,20.f,20.4f})anFix(m,model::gas_cylinder,22.58f,y,f,.33f,.33f,1.58f,0,true);
 wall(m,22.78f,19.35f,22.84f,20.65f,f+1.08f,f+1.13f,2);                                       // their chain bar
 lbStock(m,17.4f,22.6f,false,4);lbStock(m,19.4f,22.6f,false,9);
 anFix(m,model::pallet,21.7f,21.6f,f,1.2f,1.15f,.16f,0,true);
 for(int i=0;i<3;++i)kit::prop(m,model::prop_cardboard_box,21.44f+(i%2)*.52f,21.34f+(i/2)*.52f,f+.16f,.5f,.495f,.252f,0);   // absorbent, by the sack
 handCart(m,16.1f,19.6f,f);kit::prop(m,model::prop_cardboard_box_open,16.1f,19.55f,f+.29f,.4f,.4f,.4f,.2f);
 anFix(m,model::med_first_aid_box,15.12f,21.f,f+1.3f,.367f,.12f,.349f,anE,false);
 anFix(m,model::pwr_fire_cabinet,20.6f,16.36f,f+1.1f,.252f,.203f,.559f,anS,false);
 m.lights.push_back(lbTube(18.8f,19.8f,roof,.6f,6.f,4200,0));m.lights.push_back(lbTube(21.4f,21.f,roof,.45f,5.f,4200,0,.2f));
 m.lights.push_back(lbTube(16.9f,21.9f,roof,.4f,4.5f,4200,0));
 m.lights.push_back(lamp({21.f,16.28f},f+2.3f,.55f,6.f,3600,0,WorldLightMount::Wall,kPi*.5f));
 m.pickupSpawns.push_back({{16.f,21.8f},PickupKind::HandgunAmmo,f});
 extinguisher(m,15.06f,17.f,f,anE);
 recessedFittings(m);
 // Names, and the air: foul over the lab, thicker toward hood 3, violet at it.
 zone(m,"",18.4f,11.8f,22.9f,16.f,-1000,1000,{.22f,.17f,.28f,.024f});
 zone(m,"",14.6f,9.6f,18.4f,16.f,-1000,1000,{.18f,.17f,.19f,.016f});
 zone(m,"ANALYTICAL CHEMISTRY / SUIT ROOM",1.f,1.f,9.f,4.8f);
 zone(m,"ANALYTICAL CHEMISTRY / DECON",15.f,1.f,23.f,4.8f,-1000,1000,{.14f,.17f,.19f,.007f});
 zone(m,"ANALYTICAL CHEMISTRY / LAB",0,5.f,23.f,16.f,-1000,1000,{.16f,.18f,.13f,.011f});
 zone(m,"ANALYTICAL CHEMISTRY / SOLVENT STORE",1.f,16.2f,9.f,23.f,-1000,1000,{.15f,.14f,.11f,.012f});
 zone(m,"ANALYTICAL CHEMISTRY / LOCKDOWN",9.2f,16.2f,14.8f,24.f,-1000,1000,{.14f,.14f,.13f,.012f});
 zone(m,"ANALYTICAL CHEMISTRY / SPILL STORE",15.f,16.2f,23.f,23.f);
 zone(m,"ANALYTICAL CHEMISTRY",0,0,24,24);
 return p;
}
// INSTRUMENTATION (manifest 52). Where the cells were proved. Four stands along the
// north wall put each cell on a load and wrote down what it gave; the discharge rig
// in the middle ran them flat, or tried to. No cell ever read lower after a test. The
// spent ones lie where the techs dropped them, and the reject cage holds the ones that
// filled up again overnight. The control room's glass looks onto the floor; its
// console runs the fume purge that lets Analytical Chemistry's south door open.
// Route: the E seam -> the control room (the purge) -> the back corridor -> the test
// floor -> S to Field Measurement; the cell store off the control room, the locker
// room off the corridor.
std::shared_ptr<const Map> makeInstrumentation(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[82];m.definition=CampaignChunks[82];m.definition.ambient=.22f;
 m.definition.playerStart={22.2f,11.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi;
 m.wallSurface=SurfacePaintedGreen;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',6.f,10.f,f,false,0});
 machineryClock(m,"anl_inst_clock",60000);
 auto crash=[&](float x1,float x2,float y1,float y2){wall(m,x1,y1,x2,y2,f+.86f,f+.96f,20);};
 // the room's own walls where things stand against them
 wall(m,1.f,.9f,23.f,1.f,f,roof,3);wall(m,.9f,1.f,1.f,23.f,f,roof,3);
 wall(m,22.9f,1.f,23.f,9.f,f,roof,3);wall(m,22.9f,14.f,23.f,23.f,f,roof,3);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,1.f,17.f,17.f},{17.2f,6.2f,22.9f,14.6f},{17.2f,14.8f,22.9f,17.f},{17.2f,17.2f,22.9f,23.f},{17.2f,1.1f,22.9f,6.f},{6.f,17.2f,10.f,23.f}})
  ceilingGrid(m,x1,y1,x2,y2,roof);
 // --- The control room (east): the purge, the window onto the floor ------------------------------
 // Its doors hang in its walls along x (a leaf hangs in no other): north to the cell store, south
 // to the back corridor, whose framed mouth in the west wall opens onto the floor.
 wall(m,17.f,1.f,17.2f,8.f,f,roof,3);glazing(m,17.f,8.f,17.2f,12.f,f,roof,3,.95f,2.4f);wall(m,17.f,12.f,17.2f,15.2f,f,roof,3);
 doorwayY(m,15.2f,16.6f,17.f,17.2f,f,roof);wall(m,17.f,16.6f,17.2f,23.f,f,roof,3);
 wallDoor(m,17.2f,22.9f,6.f,6.2f,21.5f,22.7f,f,roof,-1);                                      // control room / cell store
 wallDoor(m,17.2f,22.9f,14.6f,14.8f,17.7f,18.9f,f,roof,-1);                                   // control room / back corridor
 terminal(m,{18.f,7.f},"FUME PURGE / ANALYTICAL CHEMISTRY","HOOD 3 RELEASE 02:22. CHEMISTRY SEALED ON THE FOUL-AIR READING. EXTRACT FANS 1-3 STALLED.",
  "RUN THE PURGE: FANS TO BOOST, MAKE-UP AIR OPEN. THE SOUTH DOOR RELEASES ON THE CLEAN READING.",0,stateId("lab_chem_seal"));
 // the purge board on its steel plate, the knife switches, the clock over them
 wall(m,19.7f,6.2f,21.3f,6.215f,f+.92f,f+1.68f,20);
 anModel(m,model::lab_control_board,20.5f,6.31f,f+1.f,anS,false);
 for(float x:{19.05f,19.45f})anModel(m,model::lab_knife_switch,x,6.3f,f+1.75f,anS,false);   // under the clock, clear of the store door
 anModel(m,model::lab_wall_clock,19.f,6.22f,f+2.3f,anS,false);
 // the console under the glass: its screens to the operators, its back to the floor
 wall(m,17.3f,8.05f,17.8f,11.95f,f,f+.08f,8);wall(m,17.25f,8.f,17.85f,12.f,f+.08f,f+.78f,20);wall(m,17.23f,7.98f,17.87f,12.02f,f+.78f,f+.82f,8);
 for(float y=8.6f;y<11.8f;y+=.6f)wall(m,17.85f,y,17.86f,y+.01f,f+.12f,f+.74f,8);
 for(int i=0;i<10;++i){const float y=8.15f+i*.37f;if(i==2||i==5||i==8)continue;wall(m,17.3f,y,17.4f,y+.22f,f+.82f,f+.84f,i%3?18:19);}   // button rows
 for(auto [y,mdl]:{std::pair{8.75f,model::crt_monitor},{10.05f,model::crt_monitor}})kit::prop(m,mdl,17.52f,y,f+.82f,.42f,.4f,.38f,anE);
 anModel(m,model::lab_instrument_terminal,17.6f,11.3f,f+.82f,anE,false,.65f);
 for(float y:{8.75f,10.05f})kit::prop(m,model::keyboard,17.78f,y,f+.82f,.45f,.16f,.03f,anE);
 for(auto [y,yaw]:{std::pair{8.9f,kPi*.5f+.25f},{10.7f,kPi*.5f-.3f}})kit::prop(m,model::office_chair,18.4f,y,f,.55f,.55f,.95f,yaw);
 m.lights.push_back(lamp({17.7f,10.f},f+1.2f,.28f,2.6f,9000,-.2f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.08f;   // the screens' glow on the glass
 // the duty desk, its lamp, the records, the tea
 anModel(m,model::lab_exec_desk,20.6f,10.2f,f,anS,true,.8f);
 deskSet(m,{20.6f,10.2f},f+.713f,kPi,1);
 const int dutyLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,21.25f,10.f,f+.713f,.18f,.4f,.52f,.3f);
 kit::prop(m,model::office_chair,20.55f,11.1f,f,.55f,.55f,.95f,-.15f);
 kit::prop(m,model::office_paper_stack,21.2f,10.43f,f+.713f,.33f,.44f,.03f,1.5f);
 for(float x:{19.9f,20.85f})anModel(m,model::med_records_cabinet,x,14.35f,f,anN,true);
 for(float x:{19.7f,19.8f,19.9f})kit::prop(m,model::office_file_binder,x,14.35f,f+1.f,.093f,.301f,.424f,0);
 kit::prop(m,model::prop_cardboard_box,20.85f,14.33f,f+1.f,.4f,.396f,.2f,0);
 kit::prop(m,model::mug,21.15f,14.3f,f+1.f,.09f,.09f,.1f,.7f);
 // the cooler under the camera shelf and the bin in the corner, clear of the way through to chemistry
 anFix(m,model::water_cooler,22.65f,8.68f,f,.35f,.35f,1.1f,anW,true);
 kit::prop(m,model::trash_can,22.55f,14.3f,f,.35f,.35f,.6f,.4f);
 // the camera bank on its shelf, clear of the way to the store door
 wall(m,22.5f,6.7f,22.9f,8.3f,f+1.7f,f+1.74f,20);for(float y:{6.75f,8.21f})wall(m,22.82f,y,22.9f,y+.04f,f+1.5f,f+1.7f,8);
 for(float y:{7.f,7.5f,8.f})anFix(m,model::cctv_tv,22.7f,y,f+1.74f,.5f,.4f,.42f,anW,false);
 m.lights.push_back(lamp({22.3f,7.5f},f+1.95f,.22f,2.2f,8000,-.15f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.12f;
 paperDrift(m,18.2f,7.6f,22.f,14.f,f,8,8202);
 m.lights.push_back(lbTube(20.f,7.4f,roof,.5f,5.f,5200,.05f));                                // on the purge board
 m.lights.push_back(lbTube(20.2f,12.2f,roof,.45f,5.5f,4600,.03f));
 m.lights.push_back(lamp({21.25f,10.15f},f+1.15f,.3f,2.2f,2800,0,WorldLightMount::Bare));m.lights.back().prop=dutyLamp;
 // the back corridor: the log, the board, the way round to the floor
 m.lights.push_back(lbTube(20.f,15.9f,roof,.42f,4.5f,4200,.02f));
 taped(m,{22.89f,15.6f},f+1.5f,anW,Readable::Note,"STAND LOG / NIGHT","STAND 3 TRIPPED 01:52 ON CELL 0451. RATED 40. GAVE 212 BEFORE THE BREAKER WENT. READS FULL.",
  "NO CELL HAS EVER READ LOWER AFTER A TEST. WE HAVE STOPPED WRITING DOWN WHERE THE EXTRA COMES FROM.");
 anFix(m,model::med_notice_board,21.f,14.835f,f+1.25f,1.2f,.07f,.9f,anS,false);
 anFix(m,model::pwr_fire_cabinet,22.79f,16.5f,f+1.1f,.252f,.203f,.559f,anW,false);
 crash(19.f,20.3f,14.8f,14.84f);crash(21.7f,22.9f,16.96f,17.f);
 // the locker room (southeast, off the back corridor): somewhere to stand still
 wallDoor(m,17.2f,22.9f,17.f,17.2f,19.f,20.2f,f,roof,1);
 for(int i=0;i<4;++i)anFix(m,model::psx_locker,18.f+i*.86f,22.67f,f,.64f,.85f,2.42f,kPi*.5f,true);
 wall(m,17.6f,22.2f,21.f,22.9f,f+2.42f,roof,30);
 bench(m,19.3f,21.55f,f);
 wall(m,22.35f,18.7f,22.9f,21.f,f,f+.08f,8);wall(m,22.3f,18.65f,22.9f,21.05f,f+.08f,f+.86f,21);wall(m,22.25f,18.6f,22.9f,21.1f,f+.86f,f+.9f,20);   // the tea counter
 kit::prop(m,model::coffee_machine,22.6f,19.05f,f+.9f,.35f,.35f,.45f,-kPi*.5f);
 for(auto [x,y]:{std::pair{22.45f,19.6f},{22.7f,19.75f},{22.5f,20.3f}})kit::prop(m,model::mug,x,y,f+.9f,.09f,.09f,.1f,y);
 anFix(m,model::wooden_table,20.4f,19.6f,f,1.367f,.724f,.78f,0,true);
 kit::prop(m,model::wooden_chair,20.f,18.9f,f,.417f,.525f,.95f,kPi+.2f);kit::prop(m,model::wooden_chair,21.f,20.35f,f,.417f,.525f,.95f,-.25f);
 kit::prop(m,model::mug,20.2f,19.5f,f+.78f,.09f,.09f,.1f,.4f);kit::prop(m,model::office_paper_stack,20.7f,19.65f,f+.78f,.33f,.44f,.03f,1.1f);
 {int n=0;for(auto [x,h]:{std::pair{17.45f,.95f},{18.f,1.f},{20.55f,.9f},{21.1f,1.05f},{21.65f,.95f}}){wall(m,x,17.2f,x+.4f,17.24f,f+h,f+1.62f,n++%2?23:39);wall(m,x+.04f,17.2f,x+.36f,17.25f,f+1.62f,f+1.7f,8);wall(m,x+.18f,17.2f,x+.22f,17.27f,f+1.7f,f+1.75f,20);}}   // coats on their pegs, collars up
 kit::prop(m,model::mine_hard_hat,17.55f,20.95f,f,.279f,.351f,.153f,.6f);kit::prop(m,model::item_dosimeter,19.9f,21.5f,f+.51f,.31f,.146f,.066f,2.f);
 anFix(m,model::med_notice_board,17.235f,20.1f,f+1.25f,1.2f,.07f,.9f,anE,false);
 anFix(m,model::med_wall_clock,17.21f,21.9f,f+2.05f,.24f,.02f,.23f,anE,false);
 m.lights.push_back(lbTube(20.f,20.f,roof,.55f,5.5f,3100,0));
 m.pickupSpawns.push_back({{22.3f,18.f},PickupKind::Health,f});
 // --- The test floor: four stands, the discharge rig ---------------------------------------------
 wall(m,1.f,1.f,17.f,1.04f,f,roof,28);                                                       // clean-room facing behind the stands
 wallSign(m,8.65f,1.1f,f+2.4f,"HIGH CURRENT","NO LOOSE METAL ON THE STAND BENCHES",2.f,anS);
 cableTray(m,{1.4f,2.45f},{16.6f,2.45f},roof-.45f);
 for(float x=2.2f;x<16.5f;x+=1.8f)for(float s:{-.17f,.17f})m.pipes.push_back({{x,2.45f+s},{x,2.45f+s},roof-.35f,.01f,roof,6});
 // A lead hangs a quarter longer than the span from its post to the cell's top.
 auto slack=[](Point3 a,Point3 b){auto d=b-a;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z)*1.25f;};
 for(int i=0;i<4;++i){const float x=2.8f+i*3.9f;
  // The load: a switchgear cabinet, or on stands 2 and 4 the beige rack, at equipment height, its
  // terminals on the side toward the bench.
  float side,top;
  if(i%2){const float s=.55f;const auto& v=model::size[model::lab_mainframe];anModel(m,model::lab_mainframe,x-1.15f,1.04f+v[0]*s*.5f,f,anE,true,s);side=x-1.15f+v[1]*s*.5f+.012f;top=f+v[2]*s;}
  else{const float s=.65f;const auto& v=model::size[model::anl_switch_cabinet_1];anModel(m,model::anl_switch_cabinet_1,x-1.15f,1.04f+v[1]*s*.5f,f,anS,true,s);side=x-1.15f+v[0]*s*.5f+.012f;top=f+v[2]*s;}
  m.pipes.push_back({{x-1.15f,1.2f},{x-1.15f,1.2f},top,.035f,roof,6});                         // its feed down from the ceiling
  wall(m,x-1.85f,1.04f,side-.012f,2.3f,top,roof,30);                                          // nobody climbs onto it
  lbCounter(m,x,1.04f,x+1.25f,1.66f,f,.85f,20);                                              // the stand's bench
  anModel(m,model::lab_scope_monitor,x+.95f,1.25f,f+.85f,anS,false);
  // The test set at the bench's end, its sockets toward the cell; its red and black leads plugged
  // onto the cell's posts, live, so a cell pulled off its stand tears them off arcing.
  anFix(m,model::lab_field_meter,x+.17f,i==2?1.45f:1.35f,f+.85f,.25f,.328f,.148f,i==2?kPi:-kPi*.5f,false);const int set=int(m.fixtures.size())-1;   // stand 3's turned to the aisle, where its lead was pulled
  if(i!=2){kit::prop(m,model::anl_cell_charged,x+.55f,1.35f,f+.85f,.2f*i);const int cell=int(m.clutterSpawns.size())-1;   // on the load
   const float span=slack({x+.32f,1.35f,f+.905f},{x+.55f,1.35f,f+1.255f});
   testLead(m,set,"+",cell,"+",span);testLead(m,set,"-",cell,"-",span+.04f);}
  else{kit::prop(m,model::anl_cell_spent,x+.55f,1.35f,f+.85f,1.1f);                             // stand 3, tripped
   looseTestLead(m,set,"+",{x+.3f,2.2f,f},1.1f);}                                               // its lead, hanging where it was pulled
  // the bay: its divider, the keep-out line, the meters on the facing, the read-out over the bench
  wall(m,x+1.55f,1.04f,x+1.6f,2.45f,f,f+2.1f,20);wall(m,x+1.5f,1.04f,x+1.65f,2.45f,f+2.1f,roof,30);
  floorLine(m,std::max(1.04f,x-1.95f),2.62f,x+1.55f,2.72f,f,13);
  if(i==2)anFix(m,model::pwr_breaker_panel_open,x+.7f,1.152f,f+1.25f,.318f,.225f,.457f,anS,false);
  else anFix(m,model::pwr_breaker_panel,x+.7f,1.091f,f+1.3f,.183f,.102f,.457f,anS,false);
  anFix(m,model::pwr_ammeter_box,x+1.1f,1.091f,f+1.35f,.229f,.102f,.366f,anS,false);
  anFix(m,model::lab_status_display,x+.6f,1.084f,f+2.f,1.04f,.087f,.3f,anS,false);
  if(i!=2){m.lights.push_back(lamp({x+.6f,1.25f},f+2.12f,.2f,2.f,9000,-.1f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.1f;
   lbPendant(m,{x+.6f,2.05f},roof,f+2.45f,.55f,3.6f,3200);                                      // the stand's work light
   kit::prop(m,model::office_chair,x+.55f,2.3f,f,.55f,.55f,.95f,.2f-i*.15f);}
  else{lbPendant(m,{x+.6f,2.05f},roof,f+2.45f,0,0,0);                                            // dead with the breaker
   m.lights.push_back(lamp({x-.25f,1.06f},f+2.35f,.22f,2.6f,3400,0,WorldLightMount::Wall,kPi*.5f,true,{1.f,.1f,.06f}));   // the emergency lamp on it
   for(auto [a,b,lo,hi]:{std::tuple{-.72f,-.42f,1.2f,2.05f},{-.66f,-.5f,2.05f,2.4f},{-.8f,-.58f,1.05f,1.3f}})wall(m,x+a,1.04f,x+b,1.046f,f+lo,f+hi,8);   // soot over its terminals
   propPose(m,model::office_chair,x+.35f,2.85f,f,.55f,.55f,.95f,1.2f,kPi*.5f,0);                // the chair he went over
   paperDrift(m,x-.9f,2.5f,x+1.f,3.6f,f,5,8203);}
  if(i==3){kit::prop(m,model::mug,x+1.1f,1.5f,f+.85f,.09f,.09f,.1f,.3f);kit::prop(m,model::office_file_binder,x+.75f,1.5f,f+.85f,.093f,.301f,.424f,kPi*.5f);
   kit::prop(m,model::item_dosimeter,x+.2f,3.1f,f,.31f,.146f,.066f,2.4f);}                      // the night tech's
  kit::prop(m,model::prop_cardboard_box_open,x+1.25f,2.1f,f,.36f,.36f,.36f,.12f*i);}              // each bench's reject tote
 // the discharge rig: the analyser on its plinth, the transformer beside it, the strobe on top; the
 // cell in the cradle in front, its leads run into the analyser's face
 equipmentPad(m,6.9f,7.2f,10.3f,11.7f,f);
 anModel(m,model::lab_analyzer_cabinet,8.5f,8.3f,f+.1f,anS,true,.8f);
 anFix(m,model::pwr_step_transformer,9.75f,8.3f,f+.1f,.64f,.886f,1.673f,anS,true);
 wall(m,9.43f,7.86f,10.07f,8.74f,f+2.1f,roof,30);wall(m,9.68f,8.23f,9.82f,8.37f,f+1.773f,f+1.84f,8);wall(m,9.7f,8.25f,9.8f,8.35f,f+1.84f,f+1.92f,20);
 {auto l=lamp({9.75f,8.3f},f+1.98f,.7f,4.5f,9000,0,WorldLightMount::Bare,0,false);l.period=1.7f;l.onTime=.07f;m.lights.push_back(l);}   // the strobe
 m.pipes.push_back({{9.6f,7.81f},{9.6f,7.81f},f+1.773f,.03f,roof-.35f,6});                         // its feed up to the tray
 cableTray(m,{9.4f,8.f},{16.9f,8.f},roof-.4f);
 for(float x=10.4f;x<16.8f;x+=1.8f)for(float s:{-.17f,.17f})m.pipes.push_back({{x,8.f+s},{x,8.f+s},roof-.3f,.01f,roof,6});
 wall(m,8.05f,9.45f,8.95f,10.65f,f+.1f,f+.55f,8);wall(m,8.f,9.45f,9.f,10.7f,f+.55f,f+.57f,20);   // the cradle
 kit::prop(m,model::anl_cell_charged,8.5f,10.05f,f+.57f,.6f);
 {const int cell=int(m.clutterSpawns.size())-1;anFix(m,model::lab_field_meter,8.5f,9.64f,f+.57f,.25f,.328f,.148f,kPi,false);const int set=int(m.fixtures.size())-1;
  const float span=slack({8.5f,9.79f,f+.625f},{8.5f,10.05f,f+.975f});testLead(m,set,"+",cell,"+",span,true,0,.009f);testLead(m,set,"-",cell,"-",span+.04f,true,0,.009f);
  m.pipes.push_back({{8.5f,9.46f},{8.5f,9.22f},f+.6f,.014f,f+.6f,7});}                             // the set's feed into the analyser's face
 lbPendant(m,{8.5f,10.45f},roof,f+2.35f,.6f,3.6f,3400);
 for(auto [x1,y1,x2,y2]:{std::tuple{6.4f,6.75f,10.8f,6.87f},{6.4f,12.03f,10.8f,12.15f},{6.4f,6.75f,6.52f,12.15f},{10.68f,6.75f,10.8f,12.15f}})floorLine(m,x1,y1,x2,y2,f,13);
 taped(m,{8.5f,7.405f},f+1.4f,anN,Readable::Note,"DISCHARGE RIG / STROBE","THE MASS LEANS AWAY FROM THE STROBE. IT LEANS TOWARD WHOEVER IS HOLDING THE TORCH.",
  "TORCHES OFF ON THE FLOOR WHILE A CELL IS ON THE RIG. NOT A REQUEST.");
 // spent cells, collars and cores where the techs dropped them: by the totes, and a trail from stand
 // 3 toward the cage
 propPose(m,model::anl_cell_spent,4.5f,2.6f,f,.3f,kPi*.5f,0);
 kit::prop(m,model::anl_cell_spent,15.8f,2.6f,f,.7f);
 kit::prop(m,model::anl_cell_cap,8.6f,4.3f,f,1.1f);
 kit::prop(m,model::anl_cell_core,6.3f,6.1f,f,2.f);
 kit::prop(m,model::anl_cell_cap,4.5f,8.2f,f,2.6f);
 // the reject cage (west): the spent ones that filled up again, faintly green
 anModel(m,model::med_holding_cage,2.2f,12.f,f,anE,false);                                    // its mesh; collision on its open sides
 wall(m,3.17f,9.75f,3.21f,14.25f,f,f+2.03f,30);for(float y:{9.75f,14.21f})wall(m,1.f,y,3.21f,y+.04f,f,f+2.03f,30);
 wall(m,1.f,9.75f,3.21f,14.25f,f+2.03f,roof,30);
 floorLine(m,3.32f,9.6f,3.44f,14.4f,f,13);
 wall(m,2.48f,11.18f,2.92f,11.62f,f,f+.34f,8);wall(m,2.5f,11.2f,2.9f,11.6f,f+.34f,f+.35f,20);   // a steel tote by the bars
 {const int glow=int(m.clutterSpawns.size());kit::prop(m,model::anl_cell_charged,2.7f,11.4f,f+.35f,.3f);
  m.lights.push_back(lamp({2.7f,11.4f},f+.8f,.17f,1.9f,6500,0,WorldLightMount::Bare,0,false,{.15f,1.f,.2f}));m.lights.back().prop=glow;}
 propPose(m,model::anl_cell_spent,2.4f,12.3f,f,1.2f,kPi*.5f,0);
 kit::prop(m,model::anl_cell_spent,1.8f,13.4f,f,2.2f);
 taped(m,{1.01f,9.1f},f+1.4f,anE,Readable::Note,"REJECTS / DO NOT STACK","SPENT MEANS THE WINDOW IS CLEAR. IT DOES NOT MEAN EMPTY.",
  "TWO OF THESE WERE FULL AGAIN IN THE MORNING. NOBODY CHARGED THEM. KEEP THE CAGE SHUT.");
 m.creatureSpawns.push_back({CreatureKind::Transformed,{13.f,9.f},-999,1,0});                 // the night tech, at his stand
 m.pickupSpawns.push_back({{15.6f,6.4f},PickupKind::Ammo,f});
 // the south of the floor: the extract duct to its fan, the prep bench, the crates in, the chart desk
 wall(m,1.9f,15.7f,16.6f,16.4f,roof-.66f,roof-.16f,20);
 for(float x=3.f;x<16.f;x+=2.4f)for(float y:{15.75f,16.35f})m.pipes.push_back({{x,y},{x,y},roof-.16f,.012f,roof,6});
 for(float x:{4.5f,9.f,13.5f})anFix(m,model::pwr_vent_grille,x,15.69f,roof-.62f,.41f,.02f,.4f,anN,false);
 anFix(m,model::pwr_duct_fan_unit,1.45f,16.2f,f,.794f,1.494f,1.95f,0,true);                    // its back to the south wall
 m.pipes.push_back({{1.6f,16.05f},{1.6f,16.05f},f+1.95f,.15f,roof-.41f,2});m.pipes.push_back({{1.6f,16.05f},{2.f,16.05f},roof-.41f,.15f,roof-.41f,2});
 anFix(m,model::pallet,1.6f,14.85f,f,1.2f,1.15f,.16f,kPi*.5f,true);anModel(m,model::lab_specimen_crate,1.6f,14.85f,f+.16f,.3f,true,.75f);   // crates in, by the cage
 lbCounter(m,10.4f,16.34f,15.6f,17.f,f,.9f,28);
 wall(m,10.4f,16.68f,15.6f,17.f,f+1.55f,f+1.58f,20);for(float x:{10.45f,15.53f})wall(m,x,16.7f,x+.02f,17.f,f+1.42f,f+1.55f,8);
 wall(m,10.4f,16.68f,15.6f,17.f,f+2.f,roof,30);
 kit::prop(m,model::anl_cell_spent,11.f,16.62f,f+.9f,.4f);kit::prop(m,model::anl_cell_cap,11.45f,16.7f,f+.9f,1.9f);kit::prop(m,model::anl_cell_core,13.9f,16.6f,f+.9f,.4f);
 kit::prop(m,model::lab_field_meter,12.3f,16.6f,f+.9f,.25f,.328f,.148f,.2f);kit::prop(m,model::lab_microscope,14.7f,16.62f,f+.9f,.1f);
 kit::prop(m,model::office_paper_stack,12.95f,16.65f,f+.9f,.33f,.44f,.03f,-.3f);kit::prop(m,model::item_dosimeter,15.2f,16.6f,f+.9f,.31f,.146f,.066f,.9f);
 for(int i=0;i<6;++i)kit::prop(m,model::office_file_binder,10.6f+i*.1f,16.84f,f+1.58f,.093f,.301f,.424f,0);
 for(float x:{12.4f,12.8f,14.2f})kit::prop(m,model::anl_cell_charged,x,16.84f,f+1.58f,x);
 anFix(m,model::pallet,3.4f,16.2f,f,1.2f,1.15f,.16f,0,true);anModel(m,model::lab_specimen_crate,3.4f,16.2f,f+.16f,.2f,true,.85f);
 anFix(m,model::pallet,4.95f,16.2f,f,1.2f,1.15f,.16f,kPi*.5f,true);anModel(m,model::lab_specimen_crate,4.95f,16.25f,f+.16f,-.15f,true,.75f);
 kit::prop(m,model::prop_cardboard_box_open,5.2f,15.2f,f,.5f,.5f,.5f,.4f);
 lbCounter(m,12.2f,12.3f,15.2f,12.96f,f,.85f,20);                                           // the chart desk
 anModel(m,model::lab_instrument_terminal,13.f,12.62f,f+.85f,anS,false,.75f);
 kit::prop(m,model::office_paper_stack,13.9f,12.6f,f+.85f,.33f,.44f,.03f,.15f);
 for(float x:{14.5f,14.6f,14.7f})kit::prop(m,model::office_file_binder,x,12.55f,f+.85f,.093f,.301f,.424f,0);
 kit::prop(m,model::office_chair,13.f,13.6f,f,.55f,.55f,.95f,-.2f);
 kit::prop(m,model::med_crash_cart,5.6f,13.9f,f,.736f,.483f,.999f,.35f);                      // the instrument trolley
 kit::prop(m,model::lab_field_meter,5.55f,13.9f,f+.8f,.25f,.328f,.148f,.6f);
 handCart(m,15.8f,13.8f,f);kit::prop(m,model::anl_cell_charged,15.7f,13.6f,f+.29f,.8f);
 floorLine(m,7.94f,15.2f,8.06f,17.2f,f,14);floorLine(m,8.06f,15.2f,17.f,15.32f,f,14);           // the route to the control room
 paperDrift(m,3.5f,3.2f,16.5f,15.f,f,10,8201);
 // the east wall: the calibration bench where the read-outs were checked against a meter, the logger
 // cabinet the stands wrote to
 lbCounter(m,16.3f,3.2f,17.f,5.8f,f,.9f,28);wall(m,16.97f,3.2f,17.f,5.8f,f+.9f,f+1.55f,28);
 wall(m,16.68f,3.2f,17.f,5.8f,f+1.6f,f+1.63f,20);wall(m,16.68f,3.2f,17.f,5.8f,f+2.1f,roof,30);
 for(float y:{3.25f,5.72f})wall(m,16.7f,y,17.f,y+.02f,f+1.48f,f+1.6f,8);
 kit::prop(m,model::crt_monitor,16.72f,4.1f,f+.9f,.42f,.4f,.38f,anW);kit::prop(m,model::keyboard,16.44f,4.1f,f+.9f,.45f,.16f,.03f,anW);
 kit::prop(m,model::lab_field_meter,16.62f,3.5f,f+.9f,.25f,.328f,.148f,anW+.2f);kit::prop(m,model::lab_field_meter,16.6f,5.05f,f+.9f,.25f,.328f,.148f,anW-.15f);
 kit::prop(m,model::anl_cell_spent,16.62f,5.5f,f+.9f,.8f);kit::prop(m,model::office_paper_stack,16.55f,4.65f,f+.9f,.33f,.44f,.03f,.2f);
 for(int i=0;i<7;++i)kit::prop(m,model::office_file_binder,16.84f,3.4f+i*.1f,f+1.63f,.093f,.301f,.424f,kPi*.5f);
 for(float y:{4.6f,4.95f})kit::prop(m,model::med_specimen_jar,16.84f,y,f+1.63f,y);
 kit::prop(m,model::office_chair,15.75f,4.3f,f,.55f,.55f,.95f,anW+.4f);
 anModel(m,model::pwr_control_cabinet,16.8f,6.6f,f,anW,true);wall(m,16.6f,6.2f,17.f,7.f,f+1.6f,roof,30);
 m.pipes.push_back({{16.88f,6.6f},{16.88f,6.6f},f+1.6f,.03f,roof,8});
 lbPendant(m,{16.4f,4.5f},roof,f+2.45f,.42f,3.f,3200);
 // the south wall over the crates: the floor's own distribution boards, the test schedule
 for(float x:{2.35f,2.75f}){anFix(m,model::pwr_breaker_panel,x,16.949f,f+1.4f,.183f,.102f,.457f,anN,false);m.pipes.push_back({{x,16.95f},{x,16.95f},f+1.86f,.02f,roof,8});}
 anFix(m,model::pwr_ammeter_box,3.25f,16.949f,f+1.45f,.229f,.102f,.366f,anN,false);
 anFix(m,model::lab_notice_board,4.9f,16.965f,f+1.25f,1.2f,.07f,.9f,anN,false);
 // the walls: rails, boards, the clock, the PA, the reader at the control room's door
 crash(1.f,1.04f,2.8f,9.7f);crash(1.9f,6.f,16.96f,17.f);crash(15.7f,17.f,16.96f,17.f);crash(16.96f,17.f,1.2f,3.15f);crash(16.96f,17.f,7.05f,7.8f);
 anModel(m,model::pwr_disconnect_switch,16.93f,2.2f,f+1.2f,anW,false);m.pipes.push_back({{16.9f,2.2f},{16.9f,2.2f},f+1.75f,.025f,roof,8});   // the floor's isolator
 anFix(m,model::med_notice_board,1.035f,6.2f,f+1.25f,1.2f,.07f,.9f,anE,false);
 anFix(m,model::lab_pa_horn,1.29f,4.4f,f+2.6f,.495f,.564f,.433f,anE,false);
 anFix(m,model::med_wall_clock,12.f,16.99f,f+2.05f,.24f,.02f,.23f,anN,false);
 anFix(m,model::med_first_aid_box,16.1f,16.94f,f+1.3f,.367f,.12f,.349f,anN,false);
 anFix(m,model::lab_id_scanner,16.97f,14.8f,f+1.1f,.3f,.06f,.41f,anW,false);
 extinguisher(m,17.f,12.7f,f,anW);
 // Light: tubes over the floor (one going), a work light over each stand and the rig, the read-outs'
 // blue, stand 3's red, the strobe, the cage's green.
 for(auto [x,y,fl]:{std::tuple{3.6f,6.8f,0.f},{8.5f,4.8f,0.f},{14.f,6.6f,.3f},{4.2f,14.4f,0.f},{9.f,14.f,0.f},{14.2f,14.6f,0.f}})m.lights.push_back(lbTube(x,y,roof,.6f,6.f,5200,.05f,fl));
 m.lights.push_back(lamp({1.02f,14.6f},f+2.4f,.42f,5.f,3400,0,WorldLightMount::Wall,0));       // the bulkhead over the fan unit
 // --- The cell store (northeast, off the control room): full cells waiting their turn ----------
 wall(m,17.2f,1.f,22.9f,1.1f,f,roof,3);
 lbCellShelf(m,18.3f,1.36f,false,1);lbCellShelf(m,20.2f,1.36f,false,2);lbCellShelf(m,22.62f,2.05f,true,3);lbCellShelf(m,22.62f,3.9f,true,4);
 int storeGlow=-1;
 for(auto [x,y]:{std::pair{18.5f,3.5f},{19.4f,3.5f},{18.5f,4.4f},{19.4f,4.4f}}){wall(m,x-.35f,y-.35f,x+.35f,y+.35f,f,f+.05f,8);
  if(storeGlow<0)storeGlow=int(m.clutterSpawns.size());kit::prop(m,model::anl_cell_charged,x,y,f+.05f,x*1.3f+y);}   // the day's issue
 for(auto [x1,y1,x2,y2]:{std::tuple{18.f,3.f,19.9f,3.08f},{18.f,4.82f,19.9f,4.9f},{18.f,3.f,18.08f,4.9f},{19.82f,3.f,19.9f,4.9f}})floorLine(m,x1,y1,x2,y2,f,13);
 anFix(m,model::med_notice_board,17.235f,3.4f,f+1.25f,1.2f,.07f,.9f,anE,false);                 // the sign-out sheets
 anFix(m,model::lab_id_scanner,21.2f,5.97f,f+1.1f,.3f,.06f,.41f,anN,false);
 anModel(m,model::lab_specimen_cabinet,18.5f,5.6f,f,anN,true);wall(m,17.83f,5.23f,19.17f,5.97f,f+2.19f,roof,30);
 handCart(m,20.7f,4.3f,f);for(auto [x,y]:{std::pair{20.6f,4.05f},{20.85f,4.4f}})kit::prop(m,model::anl_cell_charged,x,y,f+.29f,x+y);
 m.lights.push_back(lbTube(20.f,3.2f,roof,.62f,5.5f,4000,0));m.lights.push_back(lbTube(21.4f,4.7f,roof,.4f,4.5f,4000,0));
 m.lights.push_back(lamp({18.95f,3.95f},f+.5f,.2f,2.2f,6500,0,WorldLightMount::Bare,0,false,{.15f,1.f,.2f}));m.lights.back().prop=storeGlow;
 // --- The south: the corridor on to Field Measurement --------------------------------------------
 wall(m,1.f,17.f,6.f,17.2f,f,roof,3);wall(m,10.f,17.f,17.f,17.2f,f,roof,3);
 wall(m,5.8f,17.2f,6.f,23.f,f,roof,3);wall(m,10.f,17.2f,10.2f,23.f,f,roof,3);
 wall(m,1.f,17.2f,5.8f,23.f,f,roof,3);wall(m,10.2f,17.2f,17.f,23.f,f,roof,3);
 crash(6.f,6.04f,17.2f,23.f);crash(9.96f,10.f,17.2f,23.f);
 floorLine(m,7.94f,17.2f,8.06f,23.f,f,14);
 anFix(m,model::med_notice_board,6.035f,20.6f,f+1.25f,1.2f,.07f,.9f,anE,false);
 anFix(m,model::med_wall_clock,9.99f,21.6f,f+2.05f,.24f,.02f,.23f,anW,false);
 shelf(m,{9.72f,18.9f},kPi*.5f,0);wall(m,9.47f,18.f,9.97f,19.8f,f+1.95f,roof,30);             // the PPE rack
 m.lights.push_back(lamp({9.45f,18.9f},f+2.2f,.25f,2.f,4000,0,WorldLightMount::Bare,0,false));
 for(auto [y,z]:{std::pair{18.3f,.306f},{18.9f,.306f},{19.5f,.972f},{18.4f,1.656f}})kit::prop(m,model::mine_hard_hat,9.72f,y,f+z,.279f,.351f,.153f,y);
 for(auto [y,z]:{std::pair{18.5f,.972f},{18.85f,.972f},{19.3f,1.656f}})kit::prop(m,model::item_dosimeter,9.72f,y,f+z,.31f,.146f,.066f,kPi*.5f);
 anFix(m,model::lab_id_scanner,6.04f,22.4f,f+1.1f,.3f,.06f,.41f,anE,false);
 extinguisher(m,6.f,19.f,f,anE);
 m.lights.push_back(lbTube(8.f,19.2f,roof,.62f,6.f,4400,.02f,.4f));
 cableTray(m,{6.9f,17.3f},{6.9f,22.9f},roof-.45f);for(float y=18.f;y<22.9f;y+=1.8f)for(float s:{-.17f,.17f})m.pipes.push_back({{6.9f+s,y},{6.9f+s,y},roof-.35f,.01f,roof,6});
 servicePipe(m,{9.9f,17.3f},{9.9f,22.9f},roof-.22f,.05f,3);
 handCart(m,6.75f,21.75f,f);anModel(m,model::lab_specimen_crate,6.75f,21.7f,f+.29f,.15f,true,.6f);
 lbDeadTube(m,8.f,21.7f,roof);
 m.pickupSpawns.push_back({{9.4f,21.8f},PickupKind::HandgunAmmo,f});
 recessedFittings(m);
 zone(m,"INSTRUMENTATION / CONTROL ROOM",17.2f,6.2f,24.f,14.6f);
 zone(m,"INSTRUMENTATION / CELL STORE",17.2f,1.1f,22.9f,6.f,-1000,1000,{.10f,.20f,.13f,.014f});
 zone(m,"INSTRUMENTATION / BACK CORRIDOR",17.2f,14.8f,22.9f,17.f);
 zone(m,"INSTRUMENTATION / LOCKERS",17.2f,17.2f,22.9f,23.f);
 zone(m,"INSTRUMENTATION / SOUTH CORRIDOR",6.f,17.2f,10.f,24.f);
 zone(m,"INSTRUMENTATION / TEST FLOOR",1.f,1.f,17.f,17.f,-1000,1000,{.13f,.16f,.19f,.011f});
 zone(m,"INSTRUMENTATION",0,0,24,24);
 return p;
}
}

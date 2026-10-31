// RESEARCH FRONT (chapter 2, manifest 45-47): chunks 68, 69, 70. Research
// Intake, Personnel Processing and Research Administration: the clean,
// corporate front of Energy Research behind the great door, where the player
// first hears the Investigator who runs the cleanup, then sees him. See
// docs/Design/descent-campaign-20261025.md (The Investigator) for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- ResearchFront kit (rf) --------------------------------------------------------------
constexpr float rfS=0,rfN=kPi,rfE=kPi*.5f,rfW=-kPi*.5f;
constexpr std::uint32_t rfBlue=0xff3f86c8u,rfRed=0xffc8392du;
// A fixture at an absolute base height (see PerimeterB: lab and mine models face -y, add kPi).
void rfFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
void rfSign(Map& m,float x,float y,float z,const char* title,const char* sub,float width,float yaw,std::uint32_t accent){
 wallSign(m,x,y,z,title,sub,width,yaw);m.signs.back().accent=accent;
}
// Clean-side glazing: panel dado, sill, glass with mullions every 1.2 m, transom, bulkhead to `top`.
void rfGlass(Map& m,float x1,float y1,float x2,float y2,float f,float top,int mat=3,float sill=.95f,float head=2.4f){
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
// A door frame in a wall line at `at` (along x when alongX) from a to b, with a lintel at 2.65.
void rfFrame(Map& m,float a,float b,float at,bool alongX,float f,float roof,int mat){
 if(alongX){wall(m,a,at-.1f,b,at+.1f,f+2.65f,roof,mat);for(float x:{a-.08f,b})wall(m,x,at-.14f,x+.08f,at+.14f,f,f+2.7f,20);}
 else{wall(m,at-.1f,a,at+.1f,b,f+2.65f,roof,mat);for(float y:{a-.08f,b})wall(m,at-.14f,y,at+.14f,y+.08f,f,f+2.7f,20);}
}
// A double-door steel locker turned to face `front`.
void rfLockers(Map& m,float x,float y,float z,float front,float scale=1){
 const float yaw=front-kPi*.5f,back=.115f*scale;
 rfFix(m,model::psx_locker,x-std::cos(yaw)*back,y+std::sin(yaw)*back,z,.64f*scale,.85f*scale,2.42f*scale,yaw,true);
}
// A lamp switched by a state: lit while the state is set (or, inverted, until it is).
WorldLight rfSwitched(WorldLight l,StateId state,bool invert=false){l.enabledState=state;l.invert=invert;return l;}
// A seated workstation whose worker looks along `face`.
void rfDesk(Map& m,float x,float y,float f,float face,int variant){
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
// An armed Cleanup man on an authored beat; enable holds him on his mark until that state is set.
void rfGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,
             float sight,float dot,int weapon,float speed,StateId enable=0,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=dot;ai.searchMs=7000;ai.speed=speed;ai.enableState=enable;
 ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
// A sequence started only by entering its box (or by setting its timer).
void rfSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1=-2,float y1=-2,float x2=-1,float y2=-1){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
// A seam header: closes the step between this chunk's roof and a passage of `height` across an edge.
void rfSeamHeader(Map& m,char side,float a,float b,float f,float height,int mat=3){
 const float roof=m.definition.ceiling;if(roof<=f+height+.01f)return;
 if(side=='N')wall(m,a,0,b,.2f,f+height,roof,mat);
 if(side=='S')wall(m,a,23.8f,b,24,f+height,roof,mat);
}
// An air-shower booth along y from y1 to y2 against the west wall (x 1 to x2):
// nozzle columns on its side walls, a drain, the cycle display over the open doorway.
void rfBooth(Map& m,float x2,float y1,float y2,float f,float roof,float doorA,float doorB){
 wall(m,1.f,y1,x2,y1+.2f,f,roof,27);
 rfGlass(m,x2,y1,x2+.2f,doorA,f,roof,27);rfGlass(m,x2,doorB,x2+.2f,y2,f,roof,27);rfFrame(m,doorA,doorB,x2+.1f,false,f,roof,27);
 for(float y:{y1+.9f,y2-.9f}){wall(m,1.2f,y-.15f,1.35f,y+.15f,f+.25f,f+2.5f,20);
  for(float z=f+.5f;z<f+2.4f;z+=.35f)m.pipes.push_back({{1.35f,y},{1.43f,y},z,.04f,z,6});}
 for(float x=1.6f;x<x2-.3f;x+=.3f)wall(m,x,y1+.6f,x+.06f,y2-.6f,roof-.06f,roof,8);
 rfFix(m,model::med_floor_drain,(1.f+x2)*.5f,(y1+y2)*.5f,f,.5f,.5f,.01f,0,false);
 rfFix(m,model::lab_status_display,x2+.32f,(doorA+doorB)*.5f,f+2.72f,.69f,.06f,.2f,rfE,false);
 m.lights.push_back(lamp({(1.f+x2)*.5f,(y1+y2)*.5f},roof-.18f,.9f,5.f,6800,.04f));
}

// --- Research Intake kit (rfi): chunk 68's own helpers ----------------------------------------------
// Clean-side glazing in anodised steel: panel dado, a steel sill, glass between
// steel mullions every 1.2 m, a steel head and the panel bulkhead up to `top`.
void rfiGlass(Map& m,float x1,float y1,float x2,float y2,float f,float top,int mat=28,float sill=.95f,float head=2.4f){
 const int steel=2;
 wall(m,x1,y1,x2,y2,f,f+sill,mat);
 wall(m,x1-.015f,y1-.015f,x2+.015f,y2+.015f,f+sill,f+sill+.04f,steel);
 const bool alongX=x2-x1>y2-y1;const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)wall(m,x1,cy-.012f,x2,cy+.012f,f+sill+.04f,f+head,17);else wall(m,cx-.012f,y1,cx+.012f,y2,f+sill+.04f,f+head,17);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,std::min(t,x2-.05f),y1,std::min(t+.05f,x2),y2,f+sill+.04f,f+head,steel);
  else wall(m,x1,std::min(t,y2-.05f),x2,std::min(t+.05f,y2),f+sill+.04f,f+head,steel);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.07f,steel);
 if(top>f+head+.07f)wall(m,x1,y1,x2,y2,f+head+.07f,top,mat);
}
// A swinging service door in a wall along x: the renderer draws the leaf, its
// posts and head to 2.55 m; the wall closes above it.
void rfiSwing(Map& m,float x1,float x2,float y,float f,float top,float swing,int mat){
 wall(m,x1,y-.1f,x2,y+.1f,f+2.55f,top,mat);
 Door d{x1,x2,y,0};d.swinging=true;d.swing=swing;m.doors.push_back(d);
}
// An open doorway from y1 to y2 through a wall along y (x1..x2): steel posts and head, the wall above.
void rfiDoorY(Map& m,float y1,float y2,float x1,float x2,float f,float top,int mat){
 for(float y:{y1-.08f,y2})wall(m,x1-.03f,y,x2+.03f,y+.08f,f,f+2.3f,2);
 wall(m,x1-.03f,y1-.08f,x2+.03f,y2+.08f,f+2.22f,f+2.3f,2);
 wall(m,x1,y1,x2,y2,f+2.3f,top,mat);
}
// A full-height wall along y with open doorways (y pairs) cut through it.
void rfiWallY(Map& m,float x1,float x2,float y1,float y2,std::initializer_list<std::pair<float,float>> gaps,float f,float top,int mat){
 float y=y1;for(auto [a,b]:gaps){if(a>y)wall(m,x1,y,x2,a,f,top,mat);rfiDoorY(m,a,b,x1,x2,f,top,mat);y=b;}
 if(y2>y)wall(m,x1,y,x2,y2,f,top,mat);
}
// A ceiling lamp in the clinic two-tube fitting: back of house and the booth canopy.
WorldLight rfiTube(Vec2 p,float z,float intensity,float range,float kelvin,float tint,float yaw=0){
 WorldLight l=lamp(p,z,intensity,range,kelvin,tint);l.yaw=yaw;
 l.fitting=model::clinic_ceiling_lamp_2;l.fittingWidth=.44f;l.fittingDepth=1.04f;l.fittingHeight=.17f;return l;
}

// RESEARCH INTAKE (manifest 45). The checkpoint the player was never cleared
// for. The great door opens into an interlock; past its collar a screening
// line of three glass booths splits the floor into the unsecured approach and
// the secured concourse. A security post watches the line from the west, the
// badge office from the east. The Cleanup detail holds the reception island,
// and over its radio the Investigator is heard for the first time: at his
// order two of the men walk back through the booths toward the door. Three
// ways through: the lit centre booth; the staff side (post, kit room, the
// corridor behind the holding pen); the visitor side (badge office, the dark
// welcome theatre, the lounge). Both sides come out behind the island.
std::shared_ptr<const Map> makeResearchIntake(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[68];m.definition=CampaignChunks[68];m.definition.ambient=.30f;
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceOfficePlaster;
 const float f=-12,roof=m.definition.ceiling;
 const int panel=28,plaster=21;
 const StateId sweep=stateId("rf_intake_sweep"),alert=stateId("rf_intake_alert");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 // Door 0, the great door's inner leaf, stays sealed until Chapter 2 ships: no
 // ordinary playthrough sets chapter2_open. Playtesters open it with the
 // console's UNLOCK while looking at it, MAP 68, or developer mode.
 openEdge(m,{'N',9.f,15.f,f,true,stateId("chapter2_open")});m.doors.back().sign=CampaignChunkCount+2;
 openEdge(m,{'S',10.f,14.f,f,false,0});
 rfSeamHeader(m,'S',10.f,14.f,f,3.2f);
 // Solid either side of the way south: nothing is built behind the lobby.
 rectangle(m.layers[0],1,20,10,23,'#');rectangle(m.layers[0],14,20,23,23,'#');
 machineryClock(m,"rf_intake_clock",60000);

 // --- R1 The interlock vestibule (x 8.8-15.2, y 1-3.15) -------------------------------------------
 rfiGlass(m,8.6f,1.f,8.8f,3.15f,f,roof);                                   // the post looks in
 // The badge hatch to the clerk: glass over a pass slot and a steel ledge.
 wall(m,15.2f,1.f,15.4f,1.6f,f,roof,panel);wall(m,15.2f,2.6f,15.4f,3.15f,f,roof,panel);
 wall(m,15.2f,1.6f,15.4f,2.6f,f,f+.95f,panel);wall(m,15.2f,1.6f,15.4f,2.6f,f+2.f,roof,panel);
 wall(m,15.29f,1.64f,15.31f,2.56f,f+1.08f,f+1.96f,17);
 for(float y:{1.6f,2.56f})wall(m,15.2f,y,15.4f,y+.04f,f+.99f,f+2.f,2);
 wall(m,15.2f,1.6f,15.4f,2.6f,f+1.96f,f+2.f,2);
 wall(m,14.92f,1.56f,15.4f,2.64f,f+.95f,f+.99f,2);
 // The collar: 1.3 m of cased wall that holds the inner leaf, a vision slot each side.
 for(auto [x1,x2,s1,s2]:{std::tuple{8.8f,10.5f,9.2f,10.f},{13.5f,15.2f,14.f,14.8f}}){
  wall(m,x1,3.15f,x2,4.45f,f,f+1.3f,panel);wall(m,x1,3.15f,x2,4.45f,f+1.8f,roof,panel);
  wall(m,x1,3.15f,s1,4.45f,f+1.3f,f+1.8f,panel);wall(m,s2,3.15f,x2,4.45f,f+1.3f,f+1.8f,panel);
  wall(m,s1,3.79f,s2,3.81f,f+1.3f,f+1.8f,17);
 }
 wall(m,10.5f,3.15f,13.5f,4.45f,f+2.65f,roof,panel);
 m.doors.push_back({10.5f,13.5f,3.8f,0});                                 // 1: the inner interlock leaf
 floorLine(m,9.f,1.2f,15.f,1.32f,f,13);floorLine(m,9.f,2.95f,15.f,3.07f,f,13);
 rfFix(m,model::lab_id_scanner,10.22f,3.12f,f+1.f,.3f,.06f,.41f,rfS,false);   // lab models face -y: yaw = facing + kPi
 rfFix(m,model::lab_status_display,12.f,3.121f,f+2.72f,.693f,.058f,.2f,rfS,false);
 rfFix(m,model::lab_pa_horn,14.918f,2.1f,f+2.45f,.495f,.564f,.433f,rfE,false);
 extinguisher(m,15.2f,2.88f,f,rfW);
 m.lights.push_back(lamp({12.f,2.1f},roof-.16f,.8f,5.f,6400,.03f));
 {auto l=lamp({12.f,3.03f},f+2.82f,.18f,1.5f,1100,-.45f,WorldLightMount::Bare);l.period=2;l.onTime=1;m.lights.push_back(l);}   // the cycle pilot

 // --- R4/R5 The approach and the screening line -------------------------------------------------
 // Three booths between panel piers under one steel canopy: an air wash and a band scan in one pass.
 for(auto [a,b]:{std::pair{8.8f,9.f},{10.6f,11.2f},{12.8f,13.4f},{15.f,15.2f}})wall(m,a,5.8f,b,7.4f,f,f+2.4f,panel);
 wall(m,8.8f,5.8f,15.2f,7.4f,f+2.4f,f+2.55f,12);wall(m,8.8f,5.8f,15.2f,7.4f,f+2.55f,roof,panel);
 for(auto [a,b]:{std::pair{9.f,10.6f},{11.2f,12.8f},{13.4f,15.f}}){
  wall(m,a,5.8f,b,7.4f,f+.003f,f+.012f,12);
  rfFix(m,model::med_floor_drain,(a+b)*.5f,6.6f,f+.012f,.5f,.5f,.01f,0,false);
  rfFix(m,model::pwr_vent_grille,a+.02f,6.8f,f+.6f,.819f,.04f,.8f,rfE,false);
  rfFix(m,model::pwr_vent_grille,b-.02f,6.8f,f+.6f,.819f,.04f,.8f,rfW,false);
 }
 rfFix(m,model::lab_id_scanner,10.57f,6.05f,f+1.f,.3f,.06f,.41f,rfE,false);
 rfFix(m,model::lab_id_scanner,11.23f,6.05f,f+1.f,.3f,.06f,.41f,rfW,false);
 rfFix(m,model::lab_id_scanner,13.43f,6.05f,f+1.f,.3f,.06f,.41f,rfW,false);
 for(float x:{9.8f,14.2f})rfFix(m,model::lab_status_display,x,5.771f,f+2.62f,.693f,.058f,.2f,rfS,false);
 rfSign(m,12.f,5.8f,f+2.6f,"SCREENING SUSPENDED","BY ORDER OF INTERNAL INVESTIGATIONS",3.2f,kPi,rfRed);
 rfFix(m,model::lab_wall_clock,9.9f,7.416f,f+2.75f,.402f,.032f,.4f,rfN,false);
 // Lane 1 keeps the staff gate; lane 3 the Investigator closed; lane 2 is the way, painted and lit.
 rfFix(m,model::ticket_gate,9.3f,6.6f,f,.4f,1.42f,1.09f,0,true);
 crewBarrier(m,14.2f,5.72f,f,1.4f);
 taped(m,{14.2f,5.72f},f+.75f,kPi,Readable::Note,"LANE CLOSED","BY ORDER OF INTERNAL INVESTIGATIONS 03:45.","USE LANE 2 ONLY.");
 floorLine(m,11.88f,4.5f,12.12f,5.8f,f,14);wall(m,11.88f,5.8f,12.12f,7.4f,f+.012f,f+.016f,14);floorLine(m,11.88f,7.4f,12.12f,12.6f,f,14);
 m.lights.push_back(rfiTube({12.f,6.6f},f+2.24f,1.3f,4.5f,6800,.05f));
 m.lights.push_back(rfiTube({14.2f,6.6f},f+2.24f,.35f,2.5f,1100,-.45f));
 m.lights.push_back(lamp({9.8f,6.6f},f+2.2f,.35f,2.5f,5600,.05f,WorldLightMount::Bare));   // lane 1, the staff gate's own light

 // --- R2 The security post (x 3.6-8.6, y 1-6.4) and R3 the kit room (x 1-3.4) -------------------
 rfiWallY(m,3.4f,3.6f,1.f,6.4f,{{2.2f,3.2f}},f,roof,panel);
 rfiWallY(m,8.6f,8.8f,3.15f,6.6f,{{4.6f,5.6f}},f,roof,panel);
 wall(m,1.f,6.4f,1.4f,6.6f,f,roof,panel);rfiSwing(m,1.4f,2.4f,6.5f,f,roof,1,panel);   // 2: D3, kit room to the corridor
 wall(m,2.4f,6.4f,4.f,6.6f,f,roof,panel);
 rfiGlass(m,4.f,6.4f,8.6f,6.6f,f,roof);                                    // over the holding pen
 // The CCTV wall: two monitors on cabinets, three on a steel shelf over them.
 for(float x:{5.f,5.92f}){rfFix(m,model::file_cabinet_wide,x,1.25f,f,.9f,.5f,.75f,rfS,true);rfFix(m,model::cctv_tv,x,1.22f,f+.75f,.5f,.4f,.42f,rfS,false);}
 wall(m,4.6f,1.f,6.4f,1.42f,f+1.5f,f+1.54f,12);
 for(float x:{4.72f,6.28f})wall(m,x-.02f,1.f,x+.02f,1.38f,f+1.3f,f+1.5f,12);
 for(float x:{4.95f,5.5f,6.05f})rfFix(m,model::cctv_tv,x,1.22f,f+1.54f,.5f,.4f,.42f,rfS,false);
 rfFix(m,model::lab_control_board,7.6f,1.094f,f+1.f,1.32f,.187f,.6f,rfN,false);
 rfFix(m,model::lab_wall_clock,7.6f,1.016f,f+2.1f,.402f,.032f,.4f,rfN,false);
 taped(m,{6.67f,1.f},f+1.25f,rfS,Readable::Note,"II / CCTV","ALL TAPES 01:50-03:40 TO THE INVESTIGATOR.","TAKEN 03:52. NO COPIES.");
 // The PA desk: one call to the holding room speaker.
 terminal(m,{4.1f,1.4f},"INTAKE PA / PAGE HOLDING","ONE CALL TO THE HOLDING ROOM SPEAKER.","LAST PAGE 02:30: TIER 2 TO SCREENING.",0,stateId("rf_intake_page"));m.terminals.back().yaw=rfS;
 // The operator counter under the vestibule glass.
 wall(m,7.9f,1.3f,8.6f,4.4f,f,f+.74f,plaster);wall(m,7.85f,1.25f,8.6f,4.45f,f+.74f,f+.78f,12);
 const float ctr=f+.78f;
 for(float y:{2.f,3.5f}){rfFix(m,model::crt_monitor,8.3f,y,ctr,.42f,.4f,.38f,rfW,false);rfFix(m,model::keyboard,7.98f,y,ctr,.45f,.16f,.03f,rfW,false);}
 rfFix(m,model::desk_phone,8.35f,2.75f,ctr,.2f,.2f,.1f,rfW,false);
 rfFix(m,model::office_paper_stack,8.04f,2.72f,ctr,.329f,.443f,.03f,rfW+.2f,false);
 rfFix(m,model::item_walkie,8.22f,1.5f,ctr,.114f,.376f,.046f,.1f,false);
 rfFix(m,model::item_walkie,8.42f,1.48f,ctr,.114f,.376f,.046f,-.12f,false);
 terminal(m,{8.15f,4.08f},"INTAKE SECURITY / SHIFT LOG","01:58 SURGE ALARM. INTAKE LOCKED BY ORDER 7. 41 BANDS INSIDE.",
  "03:40 INTERNAL INVESTIGATIONS IN BY SERVICE LIFT 2. ALL DETAILS NOW REPORT TO THE INVESTIGATOR.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfW;m.terminals.back().surface=.78f;
 rfFix(m,model::office_chair,7.3f,2.f,f,.55f,.55f,.95f,rfW,true);
 rfFix(m,model::office_chair,7.35f,3.55f,f,.55f,.55f,.95f,rfW+.35f,true);
 // The officer who wrote it: down by his chair, his band cut off.
 rfFix(m,model::office_chair,6.5f,4.8f,f,.55f,.55f,.95f,1.9f,true);
 floorLine(m,5.f,5.f,6.3f,5.85f,f,16);
 rfFix(m,model::item_id_card,6.25f,4.45f,f,.132f,.085f,.002f,.7f,false);
 paperDrift(m,4.2f,2.4f,7.f,4.2f,f,4,683);
 // The kit room: the detail took the shotguns and left the lockers.
 for(float y:{3.7f,4.6f,5.5f})rfLockers(m,1.45f,y,f,rfE);
 wall(m,1.2f,1.f,3.2f,1.08f,f+.95f,f+1.f,8);wall(m,1.2f,1.f,3.2f,1.08f,f+1.75f,f+1.8f,8);
 for(float x=1.3f;x<3.15f;x+=.36f)wall(m,x,1.f,x+.03f,1.14f,f+.9f,f+1.9f,8);
 rfFix(m,model::med_first_aid_box,3.34f,4.6f,f+1.3f,.367f,.12f,.349f,rfW,false);
 rfFix(m,model::prop_cardboard_box_open,2.7f,5.7f,f,.73f,.725f,.726f,.25f,false);
 m.lights.push_back([&]{auto l=lamp({6.1f,3.7f},roof-.16f,.7f,5.f,4300,.25f);l.flicker=.35f;return l;}());
 m.lights.push_back(lamp({5.45f,1.55f},f+1.75f,.45f,2.5f,7000,.1f,WorldLightMount::Bare));   // the monitors' glow
 m.lights.push_back(lamp({7.6f,1.6f},f+1.9f,.75f,2.2f,6500,.1f,WorldLightMount::Bare));     // the control board's bays
 m.lights.push_back(rfiTube({2.2f,3.7f},roof-.16f,.5f,4.f,4300,.3f));

 // --- R6 Holding, R7 the effects store, R8 the staff corridor and back passage ----------------
 rfiWallY(m,2.8f,3.f,6.6f,17.8f,{{8.4f,9.4f},{14.4f,15.4f}},f,roof,panel);
 wall(m,3.f,11.8f,6.8f,12.f,f,roof,panel);
 wall(m,6.6f,12.f,6.8f,17.6f,f,roof,panel);
 wall(m,2.8f,17.6f,6.8f,17.8f,f,roof,panel);
 wall(m,6.6f,6.6f,6.8f,6.8f,f,f+2.4f,panel);wall(m,6.6f,6.6f,6.8f,11.8f,f+2.4f,roof,panel);
 rfFix(m,model::med_cell_bars,6.7f,8.05f,f,2.5f,.04f,2.4f,rfE,false);
 rfFix(m,model::med_cell_bars_doorway,6.7f,10.55f,f,2.5f,.04f,2.4f,rfE,false);
 wall(m,6.66f,6.8f,6.74f,9.97f,f,f+2.4f,CollisionProxyMaterial);wall(m,6.66f,11.13f,6.74f,11.8f,f,f+2.4f,CollisionProxyMaterial);
 bench(m,4.8f,7.5f,f);bench(m,4.4f,11.3f,f);
 // 41 bands taken at the door, tipped out of the box they were counted into.
 rfFix(m,model::prop_cardboard_box_open,3.6f,10.15f,f,.73f,.725f,.726f,-.3f,false);
 for(auto [x,y,yaw]:{std::tuple{4.2f,9.7f,.4f},{4.5f,10.1f,2.f},{4.05f,10.55f,1.1f},{4.8f,9.85f,2.7f},{4.35f,10.75f,.2f},{5.f,10.4f,1.6f}})
  rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{4.6f,9.55f,.9f},{4.15f,10.3f,2.4f},{5.2f,10.f,.3f},{4.7f,10.65f,1.9f},{3.75f,9.6f,1.2f},{5.35f,9.65f,2.8f}})
  rfFix(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw,false);
 taped(m,{3.5f,6.6f},f+1.5f,rfS,Readable::Note,"HOLDING / 01:58","HELD: 41. RELEASED: 0. BANDS TAKEN AT THE DOOR.","02:30 TIER 2 WALKED TO SCREENING. REST TO FOLLOW.");
 rfFix(m,model::lab_pa_horn,3.282f,7.3f,f+2.5f,.495f,.564f,.433f,rfW,false);
 rfFix(m,model::med_first_aid_box,3.06f,11.f,f+1.3f,.367f,.12f,.349f,rfE,false);
 m.lights.push_back(lamp({3.f,9.9f},f+2.3f,.5f,4.f,2700,0,WorldLightMount::Wall,0));
 // The store: what was taken from the 41, held for the Investigator.
 rfFix(m,model::stock_cage,4.8f,12.6f,f,2.4f,1.f,1.1f,0,true);
 for(auto [x,yaw]:{std::pair{4.f,.1f},{4.8f,-.15f},{5.6f,.3f}})rfFix(m,model::prop_cardboard_box,x,12.6f,f+1.1f,.727f,.72f,.367f,yaw,false);
 taped(m,{4.8f,13.1f},f+.75f,rfS,Readable::Note,"EFFECTS / HELD FOR II","41 BANDS. 41 BAGS. NOT TO BE RETURNED TO STAFF.","SIGNED FOR 03:47.");
 for(float y:{14.f,16.3f}){shelf(m,{6.35f,y},rfW);rfFix(m,model::prop_cardboard_box,6.35f,y+.2f,f+1.8f,.727f,.72f,.367f,rfW+.1f,false);
  rfFix(m,model::prop_cardboard_box,6.35f,y-.45f,f+.972f,.45f,.45f,.23f,rfW,false);rfFix(m,model::prop_cardboard_box_open,6.35f,y+.4f,f+.972f,.45f,.45f,.45f,rfW+.2f,false);
  for(int i=0;i<5;++i)rfFix(m,model::office_file_binder,6.42f,y-.6f+i*.12f,f+.306f,.093f,.301f,.424f,rfW+(i==3?.25f:0),false);}
 rfFix(m,model::office_file_cabinet_open,4.2f,17.2f,f,1.451f,.699f,1.054f,rfN,true);
 rfFix(m,model::wooden_table,4.6f,15.6f,f,1.367f,.724f,.78f,0,true);
 const float table=f+.78f;
 rfFix(m,model::item_walkie,4.2f,15.4f,table,.114f,.376f,.046f,.6f,false);
 for(auto [x,y,yaw]:{std::tuple{4.65f,15.45f,.2f},{4.7f,15.72f,-.3f},{5.02f,15.5f,1.2f}})rfFix(m,model::item_dosimeter,x,y,table,.31f,.146f,.066f,yaw,false);
 rfFix(m,model::office_paper_stack,4.25f,15.8f,table,.329f,.443f,.03f,.1f,false);
 m.lights.push_back(lamp({4.8f,14.8f},roof-.16f,.5f,4.f,5000,.05f));
 // The corridor behind it all.
 cableTray(m,{1.2f,6.8f},{1.2f,17.6f},f+3.f);
 for(float y:{12.f,12.4f})rfFix(m,model::pwr_breaker_panel,1.051f,y,f+1.3f,.183f,.102f,.457f,rfE,false);
 extinguisher(m,1.f,16.5f,f,rfE);
 handCart(m,5.5f,18.9f,f);
 // Where the 41 were walked through: a drain, a box set down, bands that came off on the way.
 floorLine(m,2.62f,6.8f,2.7f,17.6f,f,13);floorLine(m,1.f,17.92f,6.6f,18.f,f,13);
 rfFix(m,model::med_floor_drain,1.9f,11.2f,f,.5f,.5f,.01f,0,false);
 rfFix(m,model::prop_cardboard_box_open,1.42f,13.9f,f,.45f,.45f,.45f,.3f,false);
 rfFix(m,model::prop_cardboard_box,1.4f,14.45f,f,.45f,.45f,.23f,-.2f,false);
 for(auto [x,y,yaw]:{std::tuple{2.f,10.4f,1.3f},{1.7f,13.1f,2.6f},{3.2f,18.6f,.5f}})rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{2.2f,12.2f,.8f},{4.2f,19.35f,2.1f}})rfFix(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw,false);
 paperDrift(m,1.3f,7.f,2.5f,17.f,f,4,684);
 m.lights.push_back(rfiTube({1.9f,9.5f},roof-.16f,.6f,5.f,4300,.3f));
 m.lights.push_back([&]{auto l=rfiTube({1.9f,15.5f},roof-.16f,.6f,5.f,4300,.3f);l.flicker=.5f;return l;}());
 m.lights.push_back(rfiTube({3.9f,18.9f},roof-.16f,.5f,5.f,4300,.3f,kPi*.5f));

 // --- R9 The intake concourse (x 6.8-17.2, y 7.4-17.6) and R10 the exit lobby ------------------
 for(float x:{8.6f,15.4f}){wall(m,x-.3f,11.7f,x+.3f,12.3f,f,roof,panel);wall(m,x-.36f,11.64f,x+.36f,12.36f,f,f+.12f,12);}
 for(float x:{9.25f,14.75f})rfFix(m,model::trash_can,x,12.f,f,.5f,.5f,.62f,x,false);
 // The reception island the detail holds: a front and returns, the transaction
 // ledge, two clerks' desks behind it.
 wall(m,10.f,13.f,14.f,13.5f,f,f+1.05f,plaster);
 wall(m,10.f,13.5f,10.3f,14.6f,f,f+1.05f,plaster);wall(m,13.7f,13.5f,14.f,14.6f,f,f+1.05f,plaster);
 wall(m,9.95f,12.9f,14.05f,13.55f,f+1.05f,f+1.1f,12);
 wall(m,10.f,12.98f,14.f,13.f,f,f+.1f,8);for(float x:{11.f,12.f,13.f})wall(m,x-.015f,12.98f,x+.015f,13.f,f+.1f,f+1.05f,12);   // kick plate, panel joints
 rfDesk(m,11.f,14.15f,f,rfN,1);rfDesk(m,13.f,14.15f,f,rfN,2);
 for(float x:{11.55f,13.45f})rfFix(m,model::office_pc_tower,x,14.25f,f,.262f,.609f,.609f,rfS,false);
 const float ledge=f+1.1f;
 rfFix(m,model::item_walkie,12.4f,13.2f,ledge,.114f,.376f,.046f,.4f,false);   // the detail's radio
 for(auto [x,y,yaw]:{std::tuple{10.6f,13.2f,.3f},{10.95f,13.28f,-.4f},{13.3f,13.2f,1.3f},{13.62f,13.27f,.2f}})rfFix(m,model::item_dosimeter,x,y,ledge,.31f,.146f,.066f,yaw,false);
 for(auto [x,yaw]:{std::pair{11.62f,.1f},{12.9f,-.2f}})rfFix(m,model::office_paper_stack,x,13.22f,ledge,.329f,.443f,.03f,yaw,false);
 // The queue rails that funnel the centre booth to the ledge.
 for(float x:{10.4f,13.6f}){
  for(float y:{8.8f,10.2f,11.6f}){wall(m,x-.03f,y-.03f,x+.03f,y+.03f,f,f+.95f,12);wall(m,x-.12f,y-.12f,x+.12f,y+.12f,f,f+.03f,12);}
  wall(m,x-.012f,8.83f,x+.012f,11.57f,f+.86f,f+.92f,19);
 }
 bench(m,10.5f,15.9f,f);bench(m,13.5f,15.9f,f);
 // The exhibit: the product, the first cell, lit in its case. No plaque.
 rfFix(m,model::office_display_cabinet,16.93f,8.6f,f,.968f,.54f,2.1f,rfW,true);
 rfFix(m,model::lab_test_chamber,16.93f,8.6f,f+1.05f,.627f,.374f,.352f,rfW,false);
 // A soft edge to the exit lobby: planters, and a few things on the walls.
 for(auto [x1,x2]:{std::pair{7.2f,9.6f},{14.4f,16.8f}}){
  wall(m,x1,17.3f,x2,17.8f,f,f+.5f,plaster);wall(m,x1-.02f,17.28f,x2+.02f,17.82f,f+.5f,f+.54f,12);
  for(float x:{x1+.45f,(x1+x2)*.5f,x2-.45f})rfFix(m,model::lab_potted_plant,x,17.55f,f+.54f,.44f,.51f,.82f,x,false);
 }
 rfFix(m,model::lab_potted_plant,7.2f,7.f,f,.44f,.51f,.82f,.6f,false);
 rfFix(m,model::lab_potted_plant,7.15f,16.75f,f,.44f,.51f,.82f,2.f,false);
 rfFix(m,model::water_cooler,17.025f,15.f,f,.35f,.35f,1.1f,rfW,true);
 rfFix(m,model::art_painting_5,17.16f,16.4f,f+1.1f,1.156f,.08f,.983f,rfW,false);
 rfFix(m,model::art_painting_6,6.815f,14.6f,f+1.25f,.689f,.03f,.8f,rfE,false);
 for(auto [p,z,i]:{std::tuple{Vec2{15.95f,16.4f},f+2.f,1.5f},{Vec2{8.f,14.6f},f+2.f,1.5f},{Vec2{8.4f,19.2f},f+2.1f,1.1f},{Vec2{19.6f,1.9f},f+2.2f,1.1f}})
  m.lights.push_back(lamp(p,z,i,2.8f,5200,0.f,WorldLightMount::Bare));   // picture lights
 // Bands and sheets dropped where the line broke at 01:58.
 paperDrift(m,9.2f,7.5f,14.8f,8.6f,f,7,681);
 paperDrift(m,9.4f,12.1f,14.6f,12.8f,f,4,682);
 paperDrift(m,7.2f,13.f,9.6f,16.4f,f,3,685);paperDrift(m,14.8f,9.6f,16.8f,14.f,f,3,686);
 // What the line dropped when it broke: a radio, a carton, bands by the queue posts.
 rfFix(m,model::prop_cardboard_box_open,7.3f,13.7f,f,.45f,.45f,.45f,.7f,false);
 rfFix(m,model::office_file_binder,7.7f,14.05f,f,.093f,.301f,.424f,1.4f,false);
 rfFix(m,model::item_walkie,15.6f,11.3f,f,.114f,.376f,.046f,2.2f,false);
 for(auto [x,y,yaw]:{std::tuple{10.2f,9.4f,.9f},{13.85f,10.9f,2.2f},{7.8f,12.9f,1.6f},{16.1f,13.4f,.4f}})rfFix(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{10.1f,7.9f,.6f},{11.7f,8.25f,2.4f},{12.65f,7.7f,1.2f},{14.1f,8.05f,.3f},{9.8f,12.5f,2.6f},{14.3f,12.6f,1.f}})
  rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 // The exit lobby: the detail's kit crates, and the warm light south.
 rfFix(m,model::lab_specimen_crate,15.6f,19.35f,f,1.12f,1.12f,1.12f,.12f,true);
 rfFix(m,model::lab_specimen_crate,15.55f,19.3f,f+1.12f,1.12f,1.12f,1.12f,-.18f,true);
 rfFix(m,model::item_walkie,15.15f,17.55f,f+.54f,.114f,.376f,.046f,1.1f,false);
 rfFix(m,model::art_painting_5,8.4f,19.96f,f+1.15f,1.156f,.08f,.983f,rfN,false);
 floorLine(m,10.2f,21.f,13.8f,21.12f,f,13);
 extinguisher(m,10.03f,22.4f,f,rfE);
 // The way south in plaster, as Personnel Processing carries it on.
 wall(m,10.f,20.f,10.03f,24.f,f,roof,plaster);wall(m,13.97f,20.f,14.f,24.f,f,roof,plaster);
 for(Vec2 l:{Vec2{8.6f,9.2f},Vec2{15.8f,9.2f}})m.lights.push_back(lamp(l,roof-.16f,.9f,6.5f,6200,.04f));
 m.lights.push_back(lamp({12.2f,10.4f},roof-.16f,1.f,6.f,6200,.04f));
 m.lights.push_back(lamp({12.2f,14.f},roof-.16f,1.1f,5.f,5600,.03f));          // the squad, lit
 m.lights.push_back(lamp({16.58f,8.6f},f+1.45f,1.1f,3.f,5200,0,WorldLightMount::Bare));   // the case light, at its glass
 m.lights.push_back(lamp({16.62f,8.6f},f+1.8f,1.5f,2.4f,5600,0,WorldLightMount::Bare));   // over the cell, through its glass
 m.lights.push_back(lamp({12.2f,18.8f},roof-.16f,.9f,6.f,3400,0));
 m.lights.push_back(lamp({12.f,22.f},roof-.16f,.8f,5.f,3400,0));

 // --- R11 The badge office and band cage (x 15.4-23, y 1-6.4) -------------------------------
 rfiWallY(m,15.2f,15.4f,3.15f,6.6f,{{4.6f,5.6f}},f,roof,panel);
 wall(m,15.2f,6.4f,17.8f,6.6f,f,roof,panel);rfiSwing(m,17.8f,18.8f,6.5f,f,roof,1,panel);   // 3: D4, to the theatre
 wall(m,18.8f,6.4f,23.f,6.6f,f,roof,panel);
 rfDesk(m,15.75f,2.1f,f,rfW,0);                                           // the clerk at the hatch
 terminal(m,{16.f,1.6f},"INTAKE / VISITOR LOG","03:40 VISITOR: INTERNAL INVESTIGATIONS (1). IN BY SERVICE LIFT 2.","BAND NOT ISSUED. NOT REQUIRED, PER VISITOR.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfE;m.terminals.back().surface=.75f;
 for(float x:{17.4f,17.95f})rfFix(m,model::file_cabinet_tall,x,1.3f,f,.5f,.6f,1.3f,rfS,true);
 rfFix(m,model::lab_photocopier,18.8f,1.4f,f,.826f,.752f,.967f,rfN,true);
 rfFix(m,model::lab_notice_board,19.6f,1.035f,f+.95f,1.2f,.07f,.9f,rfS,false);   // the shift rota, pinned up
 // The band printer, and the cage of bands nobody was issued.
 rfFix(m,model::desk,21.1f,1.35f,f,1.4f,.7f,.75f,rfN,true);
 rfFix(m,model::lab_instrument_terminal,21.1f,1.3f,f+.75f,.918f,.468f,.684f,rfN,false);
 rfFix(m,model::stock_cage,22.5f,3.f,f,2.4f,1.f,1.1f,rfW,true);
 rfFix(m,model::prop_cardboard_box,22.5f,2.4f,f+1.1f,.727f,.72f,.367f,.2f,false);
 for(auto [y,yaw]:{std::pair{3.25f,.1f},{3.45f,-.2f},{3.7f,.15f}})rfFix(m,model::item_dosimeter,22.45f,y,f+1.1f,.31f,.146f,.066f,yaw,false);
 taped(m,{22.f,3.f},f+.8f,rfW,Readable::Note,"BAND STOCK","BANDS: 120 IN STOCK.","ISSUED SINCE 01:58: 0.");
 shelf(m,{22.75f,5.3f},rfW);
 bench(m,20.5f,6.f,f);                                                    // where applicants waited for a band
 m.lights.push_back(lamp({17.5f,3.5f},roof-.16f,.8f,5.f,5600,.03f));
 m.lights.push_back(lamp({21.f,3.5f},roof-.16f,.6f,4.f,5600,.03f));

 // --- R12 The welcome theatre (x 17.4-23, y 6.6-13) -------------------------------------------
 rfiWallY(m,17.2f,17.4f,6.6f,17.8f,{{11.4f,12.6f}},f,roof,panel);
 wall(m,18.4f,10.8f,18.6f,13.f,f,roof,panel);                            // the light lock
 wall(m,17.4f,13.f,21.f,13.2f,f,roof,panel);rfiSwing(m,21.f,22.f,13.1f,f,roof,1,panel);   // 4: D5, to the lounge
 wall(m,22.f,13.f,23.f,13.2f,f,roof,panel);
 wall(m,17.4f,6.6f,23.f,13.f,f+.002f,f+.012f,23);
 wall(m,22.85f,7.6f,23.f,12.f,f+.55f,f+2.95f,8);
 // The film stopped on its title slide: the screen itself is the theatre's one sign.
 {AuthoredSign s{{22.84f,9.8f},f+.75f,4.1f,2.f,rfW,"YOUR WORK POWERS THE WORLD","DEPTHWORKS ENERGY RESEARCH / WELCOME, NEW STAFF"};s.accent=rfBlue;m.signs.push_back(s);}
 for(float x:{20.2f,21.4f})for(float y:{8.25f,10.55f})rfFix(m,model::platform_chair,x,y,f,2.27f,.54f,.95f,rfE+(x>21&&y>9?.12f:0),true);
 // The film, running for nobody since 01:58: its console at the back.
 terminal(m,{19.3f,7.4f},"WELCOME TO ENERGY RESEARCH","YOUR WORK POWERS THE WORLD. EVERY SHIFT BELOW BRINGS LIGHT ABOVE.",
  "BANDS STAY ON. SCREENING IS FOR YOUR SAFETY. ASK YOUR SUPERVISOR ABOUT EXPOSURE TIERS.",0,stateId("rf_intake_film_loud"));m.terminals.back().yaw=rfW;
 rfFix(m,model::med_crash_cart,19.2f,9.6f,f,.736f,.483f,.999f,rfE,true);
 rfFix(m,model::pc_tower,19.2f,9.6f,f+1.f,.2f,.45f,.42f,rfE,false);
 {auto l=lamp({22.5f,9.8f},f+1.8f,1.f,5.f,6500,.1f,WorldLightMount::Bare);l.period=3.1f;l.onTime=2.6f;m.lights.push_back(l);}
 {auto l=lamp({22.5f,8.5f},f+1.2f,.6f,4.f,5800,.05f,WorldLightMount::Bare);l.period=1.7f;l.onTime=1.2f;l.phase=.4f;m.lights.push_back(l);}
 m.lights.push_back(lamp({19.45f,9.6f},f+1.22f,.5f,3.f,6500,0,WorldLightMount::Bare));   // the projector
 for(Vec2 l:{Vec2{19.6f,8.4f},Vec2{21.4f,11.6f}})m.lights.push_back(rfSwitched(lamp(l,roof-.16f,.8f,5.f,5600,.03f),stateId("rf_intake_theatre_lights")));   // house lights, off

 // --- R13 The waiting lounge (x 17.4-23, y 13.2-20) -------------------------------------------
 rfFix(m,model::office_couch_green,22.318f,16.3f,f,2.242f,1.364f,1.192f,rfW,true);
 for(float y:{15.f,17.6f})rfFix(m,model::office_armchair_green,19.6f,y,f,1.399f,1.364f,1.192f,rfE,true);
 rfFix(m,model::wooden_table,20.95f,16.3f,f,.82f,.43f,.47f,rfE,true);
 for(auto [x,y]:{std::pair{20.85f,16.05f},{21.05f,16.5f}})rfFix(m,model::mug,x,y,f+.47f,.09f,.09f,.1f,x,false);
 rfFix(m,model::metro_vending,19.4f,13.54f,f,1.5f,.68f,2.38f,rfN,true);   // front -(sin,cos) yaw: faces +y
 m.lights.push_back(lamp({19.4f,14.35f},f+1.7f,.5f,2.2f,4300,.15f,WorldLightMount::Bare));   // its glow
 rfFix(m,model::file_cabinet_wide,22.75f,13.8f,f,.9f,.5f,.75f,rfW,true);
 rfFix(m,model::coffee_machine,22.75f,13.8f,f+.75f,.4f,.45f,.5f,rfW,false);
 rfFix(m,model::art_painting_6,22.985f,16.3f,f+1.45f,.689f,.03f,.8f,rfW,false);
 rfFix(m,model::file_cabinet_wide,22.75f,18.9f,f,.9f,.5f,.75f,rfW,true);
 rfFix(m,model::lab_desk_lamp,22.75f,18.9f,f+.75f,.18f,.398f,.521f,rfE,false);
 rfFix(m,model::lab_potted_plant,22.6f,19.7f,f,.44f,.51f,.82f,1.f,false);
 for(float y:{14.6f,15.5f})rfLockers(m,17.85f,y,f,rfE);
 taped(m,{17.4f,16.6f},f+1.5f,rfE,Readable::Note,"INTAKE / VISITOR NOTICES","SCREENING CALLS BY TIER. TIER 2 AND ABOVE REPORT TO MEDICAL SCREENING, EAST OF PROCESSING.",
  "LOST BANDS ARE A SAFETY BREACH. A BREACH IS REPORTED, NOT FORGIVEN.");
 rfFix(m,model::med_first_aid_box,17.46f,17.35f,f+1.3f,.367f,.12f,.349f,rfE,false);
 m.lights.push_back(lamp({22.75f,18.9f},f+1.15f,.7f,3.5f,2700,0,WorldLightMount::Bare));   // under the desk lamp's shade
 m.lights.push_back(lamp({20.f,16.4f},roof-.16f,.6f,5.f,3400,0));

 // --- The intake detail ---------------------------------------------------------------------------
 // The lead keeps the island and the radio; the medic walks the concourse. Two
 // more wait either side of the island for orders: the sweep sends one west
 // through the staff side and one east through the visitor side.
 const char* clock="rf_intake_clock";
 rfGuard(m,clock,"rf_intake_lead",ActorVisual::Cleanup,GuardSquadRole::Assault,"rf_intake",{{{12.f,15.f},f,6000,rfN}},14,.6f,1,1.3f,0,110);
 rfGuard(m,clock,"rf_intake_hall",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"rf_intake",
  {{{8.f,8.2f},f,4000,rfE},{{15.8f,8.2f},f,4000,rfW},{{16.2f,16.6f},f,3000,rfN},{{8.2f,16.6f},f,3000,rfN}},13,.6f,2,1.2f);
 rfGuard(m,clock,"rf_intake_sweep_a",ActorVisual::CleanupRusher,GuardSquadRole::Rusher,"rf_intake",
  {{{9.4f,14.9f},f,3000,rfN},{{9.6f,8.2f},f,2500,rfN},{{12.f,8.2f},f,2500,rfN},{{12.f,5.f},f,3000,rfN},{{8.2f,5.f},f,3000,rfW},
   {{6.f,3.6f},f,4000,rfW},{{2.4f,4.4f},f,3000,rfW},{{6.f,3.6f},f,1000,rfE},{{12.f,5.f},f,1000,rfS}},13,.6f,1,1.4f,sweep);
 rfGuard(m,clock,"rf_intake_sweep_b",ActorVisual::CleanupEngineer,GuardSquadRole::Engineer,"rf_intake",
  {{{14.6f,14.9f},f,3000,rfN},{{14.6f,8.2f},f,2500,rfN},{{12.4f,5.2f},f,2000,rfE},{{17.2f,4.f},f,3000,rfE},{{18.3f,5.9f},f,1500,rfS},
   {{18.9f,9.f},f,4000,rfE},{{19.4f,12.4f},f,1500,rfS},{{20.f,16.3f},f,3000,rfW},{{16.4f,18.3f},f,2000,rfW}},12,.6f,2,1.2f,sweep);
 for(auto& t:m.actorTracks)if(t.ai.mode==ActorAiMode::Guard)t.ai.alertState=alert;
 // The intake officer: no corpse clip sits, so he lies where he fell.
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{5.7f,5.45f},f,1.2f,4,1.f},{60000,{5.7f,5.45f},f,1.2f,4,1.f}});

 // --- The Investigator on the radio ---------------------------------------------------------------
 // Stepping into the vestibule starts it; the order at 13 s sets the sweep going.
 // The voice comes from the detail's radio on the island ledge.
 rfSequence(m,"rf_intake_radio","rf_intake_sweep",20000,13000,8.8f,1.f,15.2f,3.15f);
 const Vec2 radio{12.4f,13.2f};const float rz=f+1.2f;
 cue(m,"rf_intake_radio",800,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. Report.",.6f,rz,"inv_intake_1");
 cue(m,"rf_intake_radio",3800,Sound::Empty,radio,"RADIO: Intake holding, sir. We heard the great door go.",.6f,rz,"rf_intake_lead_1");
 cue(m,"rf_intake_radio",7600,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then someone opened it. Sweep the booths. Anyone without a band is exposed.",.6f,rz,"inv_intake_2");
 cue(m,"rf_intake_radio",12200,Sound::Empty,radio,"RADIO: ...Copy. Booths. Move.",.6f,rz,"rf_intake_lead_2");
 cue(m,"rf_intake_radio",15600,Sound::Empty,radio,"INVESTIGATOR (RADIO): And Intake. If it is one of ours, it is still exposed.",.6f,rz,"inv_intake_3");

 // --- The alarm, the PA page and the film: three things that pull a man --------------------------
 {ScriptEvent e;e.id=stateId("rf_intake_alarm_on");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;e.requireState=alert;
  ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId("rf_intake_alarm");a.value=1;e.actions={a};m.scriptEvents.push_back(e);}
 {TimedSequence s;s.timerState=stateId("rf_intake_alarm");s.finishState=stateId("rf_intake_alarm_done");s.x1=-2;s.y1=-2;s.x2=-1;s.y2=-1;
  s.durationMs=20000;s.finishAtMs=20000;s.tickSound=Sound::Alarm;s.soundIntervalMs=900;s.soundUntilMs=20000;s.gain=.55f;s.pitch=1;
  s.soundAt={12.f,10.f};s.soundZ=f+3.f;m.timedSequences.push_back(s);}
 for(auto [pos,z,phase]:{std::tuple{Vec2{12.f,7.4f},f+2.95f,0.f},{Vec2{12.f,4.45f},f+3.15f,.5f}}){
  auto l=lamp(pos,z,1.6f,7.f,1000,-.5f,WorldLightMount::Wall,kPi*.5f);l.enabledState=alert;l.period=1;l.onTime=.5f;l.phase=phase;m.lights.push_back(l);}
 rfSequence(m,"rf_intake_page","rf_intake_page_done",6000,6000);
 cue(m,"rf_intake_page",200,Sound::AlarmDistant,{4.8f,9.2f},"PA: TIER TWO STAFF TO THE INTAKE DESK.",.7f,f+2.5f);m.sequenceCues.back().aiRadius=12;
 rfSequence(m,"rf_intake_film_loud","rf_intake_film_loud_done",8000,8000);
 cue(m,"rf_intake_film_loud",300,Sound::Machine,{22.5f,9.8f},"FILM: WELCOME TO ENERGY RESEARCH. YOUR WORK POWERS THE WORLD.",1.f,f+1.6f);m.sequenceCues.back().aiRadius=12;
 cue(m,"rf_intake_film_loud",4200,Sound::Machine,{22.5f,9.8f},"FILM: EVERY SHIFT BELOW BRINGS LIGHT ABOVE. WEAR YOUR BAND.",1.f,f+1.6f);m.sequenceCues.back().aiRadius=12;
 machineryClock(m,"rf_intake_film",24000);
 cue(m,"rf_intake_film",500,Sound::Machine,{22.5f,9.8f},"",.25f,f+1.6f);

 // --- Rewards, throwables and names -------------------------------------------------------------
 m.pickupSpawns={{{2.6f,2.8f},PickupKind::Ammo,f},{{21.4f,3.3f},PickupKind::Ammo,f},{{5.6f,9.2f},PickupKind::Health,f},{{18.3f,19.2f},PickupKind::Health,f}};
 m.clutterSpawns={{5,{8.1f,3.f},ctr},{5,{8.25f,3.15f},ctr},{1,{4.6f,5.6f},f},{2,{11.2f,13.25f},ledge},{3,{5.05f,15.8f},table},
  {4,{20.6f,1.5f},f+.75f},{4,{21.6f,1.55f},f+.75f},{2,{20.9f,16.6f},f+.47f},{2,{21.05f,15.95f},f+.47f},{1,{20.4f,14.1f},f}};
 zone(m,"RESEARCH INTAKE / INTERLOCK",8.8f,1.f,15.2f,4.45f);
 zone(m,"RESEARCH INTAKE / SECURITY POST",1.f,1.f,8.6f,6.4f);
 zone(m,"RESEARCH INTAKE / HOLDING",3.f,6.6f,6.8f,11.8f);
 zone(m,"RESEARCH INTAKE / STAFF CORRIDOR",1.f,6.6f,2.8f,20.f);
 zone(m,"RESEARCH INTAKE / EFFECTS",3.f,12.f,6.8f,17.6f);
 zone(m,"RESEARCH INTAKE / STAFF CORRIDOR",1.f,17.6f,6.8f,20.f);
 zone(m,"RESEARCH INTAKE / SCREENING",8.8f,4.45f,15.2f,7.4f);
 zone(m,"RESEARCH INTAKE / BADGE OFFICE",15.4f,1.f,23.f,6.4f);
 zone(m,"RESEARCH INTAKE / WELCOME THEATRE",17.4f,6.6f,23.f,13.f);
 zone(m,"RESEARCH INTAKE / LOUNGE",17.4f,13.2f,23.f,20.f);
 zone(m,"RESEARCH INTAKE / CONCOURSE",6.8f,7.4f,17.4f,24.f);
 zone(m,"RESEARCH INTAKE",0,0,24,24);
 titleCard(m,"RESEARCH FRONT",6.8f,7.4f,17.2f,17.6f);
 recessedFittings(m,true);freightFinish(m,-1);
 return p;
}
// A reply in a conversation (DialogueReply).
DialogueReply rfReply(const char* text,const char* next="",std::initializer_list<std::pair<const char*,int>> sets={},int stance=-1,const char* require="",const char* hide=""){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;r.require=*require?stateId(require):0;r.hide=*hide?stateId(hide):0;r.stance=stance;
 for(auto [id,value]:sets)r.sets.push_back({stateId(id),value});return r;
}
void rfLine(Map& m,const char* node,const char* speaker,const char* line,std::vector<DialogueReply> replies){
 m.dialogue.push_back({stateId(node),speaker,line,node,std::move(replies)});
}

// PERSONNEL PROCESSING (manifest 46). The hub every shift passed through: the
// locker bays, the suit stations, the biometric gates and the clearance door
// down to the labs. West to Administration, east to Medical Screening. A
// Cleanup private sits wounded against the lockers; he will not lift his gun,
// and he is the first to say out loud who is giving the orders.
std::shared_ptr<const Map> makePersonnelProcessing(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[69];m.definition=CampaignChunks[69];m.definition.ambient=.36f;
 m.wallSurface=SurfacePaintedGreen;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 const StateId clearance=stateId("proc_clearance"),stance=stateId("rf_proc_stance");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,true,clearance});   // door 0: down to the labs, on clearance
 machineryClock(m,"rf_proc_clock",60000);
 // --- The north passage from Intake -----------------------------------------------------------
 wall(m,9.8f,1.f,10.f,3.6f,f,roof,3);wall(m,14.f,1.f,14.2f,3.6f,f,roof,3);
 wall(m,1.f,3.6f,10.f,3.8f,f,roof,3);wall(m,14.f,3.6f,23.f,3.8f,f,roof,3);
 m.lights.push_back(lamp({12.f,2.2f},roof-.16f,.8f,5.f,6000,.04f));
 // --- The locker bays, west (north and south of the way to Administration) ----------------------
 for(int i=0;i<6;++i)rfLockers(m,1.5f+i*.9f,4.5f,f,rfS);
 for(int i=0;i<6;++i)rfLockers(m,1.5f+i*.9f,19.5f,f,rfN);
 wall(m,1.f,8.6f,6.6f,8.8f,f,f+1.1f,3);wall(m,1.f,14.2f,6.6f,14.4f,f,f+1.1f,3);
 for(float y:{6.6f,17.2f})bench(m,3.8f,y,f);
 rfFix(m,model::med_washroom_mirror,6.95f,6.2f,f+1.15f,1.5f,.04f,1.f,rfW,false);
 for(Vec2 l:{Vec2{3.8f,6.2f},Vec2{3.8f,17.6f}})m.lights.push_back(lamp(l,roof-.16f,.85f,5.5f,5600,.05f));
 rfSign(m,4.f,3.82f,f+2.55f,"PERSONNEL PROCESSING","LOCKERS / SUITS / SCREENING / CLEARANCE",4.f,0,rfBlue);
 // --- The hub hall ------------------------------------------------------------------------------
 wall(m,6.8f,3.8f,7.f,8.6f,f,roof,3);wall(m,6.8f,14.4f,7.f,22.f,f,roof,3);
 wall(m,17.f,3.8f,17.2f,8.6f,f,roof,3);wall(m,17.f,14.4f,17.2f,22.f,f,roof,3);
 for(Vec2 c:{Vec2{9.5f,7.f},Vec2{14.5f,7.f},Vec2{9.5f,16.f},Vec2{14.5f,16.f}})wall(m,c.x-.25f,c.y-.25f,c.x+.25f,c.y+.25f,f,roof,20);
 ceilingGrid(m,7.f,3.8f,17.f,22.f,roof);
 for(Vec2 l:{Vec2{12.f,6.f},Vec2{12.f,11.5f},Vec2{12.f,17.f}})m.lights.push_back(lamp(l,roof-.16f,.95f,6.5f,6200,.04f));
 floorLine(m,7.2f,11.4f,16.8f,11.52f,f,20);
 terminal(m,{16.75f,10.f},"PROCESSING / DIRECTIONS","WEST: RESEARCH ADMINISTRATION. EAST: MEDICAL SCREENING. SOUTH: LABORATORIES, CLEARANCE ONLY.",
  "CLEARANCE IS ISSUED AT THE ADMINISTRATION DESK. TIER 2 AND ABOVE: SCREENING FIRST.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfW;
 // --- The biometric gates and the clearance door, south -----------------------------------------
 wall(m,7.f,21.8f,10.f,22.f,f,roof,3);wall(m,14.f,21.8f,17.f,22.f,f,roof,3);
 for(float x:{8.f,9.4f,14.6f,16.f}){wall(m,x-.12f,19.2f,x+.12f,20.2f,f,f+1.f,20);wall(m,x-.14f,19.18f,x+.14f,20.22f,f+1.f,f+1.04f,12);
  rfFix(m,model::lab_id_scanner,x,19.4f,f+1.04f,.21f,.042f,.29f,rfN,false);}
 wall(m,10.f,22.f,10.2f,23.8f,f,roof,3);wall(m,13.8f,22.f,14.f,23.8f,f,roof,3);
 rfFix(m,model::lab_id_scanner,9.8f,21.78f,f+1.2f,.15f,.03f,.205f,rfN,false);
 rfFix(m,model::lab_status_display,12.f,21.76f,f+2.72f,.69f,.06f,.2f,rfN,false);
 m.lights.push_back(rfSwitched(lamp({12.f,22.6f},roof-.18f,.8f,4.f,1000,-.5f),clearance,true));
 m.lights.push_back(rfSwitched(lamp({12.f,22.6f},roof-.18f,.8f,4.f,5200,.3f),clearance));
 // --- The suit stations, north-east ---------------------------------------------------------------
 for(float y:{5.0f,7.0f})rfFix(m,model::metal_shelf,22.5f,y,f,1.8f,.5f,1.8f,rfW,true);
 rfFix(m,model::med_supply_cabinet,19.f,4.4f,f,1.1f,.44f,1.7f,rfS,true);
 wall(m,17.2f,8.6f,23.f,8.8f,f,f+1.1f,3);
 bench(m,19.8f,7.2f,f);
 m.lights.push_back(lamp({20.f,6.2f},roof-.16f,.85f,5.5f,5600,.05f));
 // --- Screening intake, south-east: where a tier was read -----------------------------------------
 wall(m,17.2f,14.2f,23.f,14.4f,f,f+1.1f,3);
 rfFix(m,model::med_nurse_desk,20.2f,16.6f,f,1.48f,.7f,1.01f,rfN,true);
 rfFix(m,model::med_vitals_monitor,21.f,16.5f,f+1.01f,.42f,.2f,.31f,rfN,false);
 rfFix(m,model::file_cabinet_tall,22.5f,19.8f,f,.5f,.6f,1.3f,rfW,true);
 rfFix(m,model::med_wall_clock,20.f,21.8f,f+2.3f,.41f,.034f,.39f,rfN,false);
 terminal(m,{18.f,17.f},"SCREENING INTAKE / TIER BOARD","BANDS READ AT THE GATES. TIER 1: CLEAR. TIER 2: SCREENING DUE. TIER 3: OBSERVATION.",
  "02:30 ALL TIER 2 STAFF HELD FOR SCREENING. 03:52 TIER LIST COPIED TO INTERNAL INVESTIGATIONS.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfE;
 m.lights.push_back(lamp({20.f,17.5f},roof-.16f,.85f,5.5f,5000,.05f));
 // --- The wounded private ------------------------------------------------------------------------
 // Against the south lockers, gun across his knees. A truce, his by default:
 // he only turns if the player turns on him.
 rfGuard(m,"rf_proc_clock","rf_proc_private",ActorVisual::CleanupEngineer,GuardSquadRole::Engineer,"rf_proc",{{{5.6f,16.3f},f,60000,rfN}},10,.6f,2,.6f,0,60);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("proc_hello");t.voice=2;}
 rfLine(m,"proc_hello","WOUNDED CLEANUP PRIVATE","Don't. I'm not going to shoot you. I can't lift it.",{
  rfReply("Who's giving your orders down here?","proc_who"),
  rfReply("What happened to your squad?","proc_squad"),
  rfReply("Where's Administration?","proc_admin"),
  rfReply("I'll leave you be.")});
 rfLine(m,"proc_who","WOUNDED CLEANUP PRIVATE","The Investigator. Company man. Came down the service lift at three forty, and every radio's been his since. He doesn't count us. He counts what we've seen.",{
  rfReply("Back.","proc_hello",{{"rf_proc_heard",1}})});
 rfLine(m,"proc_squad","WOUNDED CLEANUP PRIVATE","Booth sweep at Medical. He said treat them as exposed. They were nurses. I stopped following orders about then. So did my leg.",{
  rfReply("Back.","proc_hello")});
 rfLine(m,"proc_admin","WOUNDED CLEANUP PRIVATE","West. The clearance desk's in there. He was in Records an hour ago, reading cards. Looking for somebody.",{
  rfReply("Thanks.","",{{"rf_proc_hint",1}})});
 // Walking in from Intake sets the truce.
 {ScriptEvent e;e.id=stateId("rf_proc_truce");e.x1=9.8f;e.y1=0;e.x2=14.2f;e.y2=4.f;e.bottom=-100;e.top=100;
  ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stance;a.value=int(SquadStance::Truce);e.actions={a};m.scriptEvents.push_back(e);}
 // --- What the night left -------------------------------------------------------------------------
 // The lockers were being emptied into boxes when the sweep went to Medical.
 rfFix(m,model::prop_cardboard_box_open,1.65f,17.95f,f,.73f,.725f,.726f,.15f,true);
 rfFix(m,model::prop_cardboard_box,6.1f,7.6f,f,.727f,.72f,.367f,-.25f,true);
 paperDrift(m,1.4f,5.2f,6.4f,8.2f,f,5,69);paperDrift(m,1.4f,14.8f,6.4f,18.8f,f,5,691);
 // The private: what he wrapped his leg with, where he bled, the radio he stopped answering.
 wall(m,5.f,15.75f,6.35f,16.85f,f+.004f,f+.010f,16);
 for(auto [x,y,yaw]:{std::tuple{4.85f,15.9f,.7f},{6.4f,16.95f,2.1f},{4.6f,16.4f,-.4f}})rfFix(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw,false);
 rfFix(m,model::item_walkie,4.75f,16.85f,f,.114f,.376f,.046f,1.2f,false);
 // Suit stations: stock boxes by the cabinet, dosimeters laid out on the bench for a shift that never came.
 rfFix(m,model::prop_cardboard_box,17.75f,4.45f,f,.727f,.72f,.367f,0,true);
 rfFix(m,model::prop_cardboard_box,17.77f,4.47f,f+.367f,.727f,.72f,.367f,.15f,true);
 for(float x:{18.9f,19.35f,19.8f,20.25f,20.7f})rfFix(m,model::item_dosimeter,x,7.12f,f+.51f,.31f,.146f,.066f,rfE+(x>20.f?.3f:0),false);
 // Screening intake: bands read and left on the desk, the readings under them.
 rfFix(m,model::office_paper_stack,19.65f,16.6f,f+1.01f,.33f,.44f,.03f,rfN+.2f,false);
 for(auto [x,y,yaw]:{std::tuple{20.15f,16.45f,.2f},{20.4f,16.75f,1.7f},{20.05f,16.8f,2.9f}})rfFix(m,model::item_id_card,x,y,f+1.01f,.132f,.085f,.002f,yaw,false);
 rfFix(m,model::office_pc_tower,19.15f,17.3f,f,.262f,.609f,.609f,rfS,true);
 paperDrift(m,17.6f,17.6f,22.4f,21.4f,f,6,692);
 // The hall and the gates.
 paperDrift(m,8.f,12.2f,16.f,18.6f,f,9,693);
 for(auto [x,y,yaw]:{std::tuple{11.8f,18.7f,.5f},{8.7f,18.85f,2.2f},{15.1f,13.f,1.f}})rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 extinguisher(m,17.f,6.f,f,rfW);extinguisher(m,7.f,18.f,f,rfE);
 // The hub: the PA that called the tiers, the notices nobody reads, the bins by the columns.
 rfFix(m,model::lab_pa_horn,7.28f,7.6f,f+2.6f,.495f,.564f,.433f,rfE,false);
 rfFix(m,model::lab_notice_board,7.04f,16.2f,f+1.3f,1.2f,.07f,.9f,rfE,false);
 for(Vec2 b:{Vec2{10.05f,7.f},Vec2{13.95f,16.f}})rfFix(m,model::trash_can,b.x,b.y,f,.5f,.5f,.62f,0,true);
 rfLockers(m,20.3f,4.5f,f,rfS);
 // The queue for the gates: belt rails funnelling each pair of turnstiles, the lanes taped on the floor.
 for(float x:{8.f,9.4f,14.6f,16.f}){
  for(float y:{16.6f,17.8f,19.f}){wall(m,x-.03f,y-.03f,x+.03f,y+.03f,f,f+.95f,20);wall(m,x-.05f,y-.05f,x+.05f,y+.05f,f+.95f,f+.99f,12);}
  wall(m,x-.012f,16.6f,x+.012f,19.f,f+.86f,f+.92f,12);}
 for(float x:{8.7f,15.3f})floorLine(m,x-.06f,14.6f,x+.06f,19.1f,f,13);
 // The way west, past the lockers: the laundry nobody collected.
 rfFix(m,model::prop_cardboard_box_open,5.8f,9.45f,f,.73f,.725f,.726f,-.3f,true);
 rfFix(m,model::prop_cardboard_box,5.75f,13.5f,f,.727f,.72f,.367f,.1f,true);
 // --- Rewards and names ------------------------------------------------------------------------
 m.pickupSpawns={{{2.f,15.6f},PickupKind::Health,f},{{22.2f,10.f},PickupKind::Ammo,f},{{18.6f,6.4f},PickupKind::Health,f}};
 m.clutterSpawns={{1,{20.6f,16.4f},f+1.f},{0,{3.f,7.2f},f}};
 zone(m,"PERSONNEL PROCESSING / LOCKER BAYS",1.f,3.8f,6.8f,22.f);
 zone(m,"PERSONNEL PROCESSING / SUIT STATIONS",17.2f,3.8f,23.f,8.6f);
 zone(m,"PERSONNEL PROCESSING / SCREENING INTAKE",17.2f,14.4f,23.f,22.f);
 zone(m,"PERSONNEL PROCESSING",0,0,24,24);
 recessedFittings(m,true);freightFinish(m,-1);
 return p;
}
// RESEARCH ADMINISTRATION (manifest 47). Where the company kept score: a lobby
// two floors tall under an observation mezzanine, the boardroom with the last
// quarter's minutes, the clearance desk, and the records office behind glass.
// The player first sees the Investigator here, through that glass: reading
// personnel cards among the cabinets, directing his detail by hand, then
// leaving by the far door with one card in his coat. Records opens after.
std::shared_ptr<const Map> makeResearchAdministration(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[70];m.definition=CampaignChunks[70];m.definition.ambient=.34f;
 m.wallSurface=SurfacePaintedMaroon;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling,dz=-9;
 const StateId gone=stateId("rf_admin_gone"),clearance=stateId("proc_clearance");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'E',9.f,14.f,f,false,0});
 machineryClock(m,"rf_admin_clock",60000);
 // --- The observation mezzanine, north-east, and its stair ---------------------------------------
 deck(m,dz,12,1,23,5);
 m.stairs.push_back({20.f,5.f,22.f,9.5f,f,dz,16,true,false});
 deckHangers(m,m.layers.back(),roof);
 // No rod where the stair arrives on the deck.
 std::erase_if(m.pipes,[](const PipeRun& r){return r.start.x>19.6f&&r.start.x<22.4f&&r.start.y>4.f&&r.start.y<5.6f;});
 for(float x:{14.5f,17.5f})bench(m,x,2.f,dz);
 rfFix(m,model::lab_scope_monitor,22.f,1.4f,dz+.75f,.47f,.34f,.45f,rfN,false);
 rfFix(m,model::lab_potted_plant,12.6f,1.6f,dz,.5f,.58f,.93f,0,false);
 for(Vec2 l:{Vec2{15.f,3.f},Vec2{20.f,3.f}})m.lights.push_back(lamp(l,roof-.2f,.85f,6.f,5200,.05f));
 // --- The lobby ------------------------------------------------------------------------------------
 for(Vec2 c:{Vec2{15.f,8.f},Vec2{15.f,16.f}})wall(m,c.x-.3f,c.y-.3f,c.x+.3f,c.y+.3f,f,roof,20);
 for(Vec2 l:{Vec2{16.5f,11.5f},Vec2{19.f,16.5f},Vec2{14.f,20.f}})m.lights.push_back(lamp(l,roof-.2f,1.1f,8.f,5800,.04f));
 m.lights.push_back(lamp({19.f,7.f},dz-.25f,.9f,6.f,5600,.04f));
 rfSign(m,23.f,18.f,f+3.2f,"DEPTHWORKS ENERGY RESEARCH","YOUR WORK POWERS THE WORLD",4.4f,rfW,rfBlue);
 for(float y:{19.6f,21.2f})bench(m,20.5f,y,f);
 rfFix(m,model::lab_potted_plant,22.4f,6.f,f,.5f,.58f,.93f,0,false);rfFix(m,model::lab_potted_plant,13.f,22.2f,f,.5f,.58f,.93f,0,false);
 rfFix(m,model::water_cooler,22.5f,15.f,f,.35f,.35f,1.1f,rfW,true);
 // The clearance desk: it issues proc_clearance once the floor is quiet again.
 rfFix(m,model::desk_corner,18.4f,13.6f,f,2.205f,2.211f,.891f,rfS,true);
 rfFix(m,model::crt_monitor,18.4f,13.9f,f+.891f,.42f,.4f,.38f,rfN,false);
 rfFix(m,model::office_chair,18.4f,12.1f,f,.55f,.55f,.95f,rfS,true);
 terminal(m,{18.4f,14.75f},"CLEARANCE DESK / LABORATORY ACCESS","E / ISSUE LABORATORY CLEARANCE TO THIS BAND.",
  "CLEARANCE ISSUED. THE PROCESSING DOOR SOUTH WILL READ IT.",0,clearance,gone);m.terminals.back().yaw=rfS;
 // --- The boardroom, north-west ----------------------------------------------------------------
 wall(m,12.f,1.f,12.2f,10.f,f,roof,3);
 wall(m,1.f,10.f,8.f,10.2f,f,roof,3);wall(m,9.5f,10.f,12.2f,10.2f,f,roof,3);rfFrame(m,8.f,9.5f,10.1f,true,f,roof,3);
 m.doors.push_back({8.f,9.5f,10.1f,0});              // 0: boardroom
 rfFix(m,model::lab_exec_desk,6.2f,5.2f,f,2.21f,1.1f,.89f,0,true);
 rfFix(m,model::lab_exec_desk,6.2f,3.f,f,2.21f,1.1f,.89f,0,true);
 for(float x:{4.9f,6.2f,7.5f})for(float y:{1.9f,6.3f})rfFix(m,model::lab_boardroom_chair,x,y,f,.79f,.74f,.93f,y<4?rfS:rfN,true);
 rfFix(m,model::lab_boardroom_chair,3.6f,4.1f,f,.79f,.74f,.93f,rfE,true);
 rfFix(m,model::cctv_tv,11.73f,4.1f,f+1.4f,1.f,.5f,.8f,rfW,false);
 rfFix(m,model::lab_wall_clock,6.2f,1.04f,f+2.6f,.4f,.03f,.4f,rfS,false);
 rfFix(m,model::lab_potted_plant,1.6f,9.f,f,.5f,.58f,.93f,0,false);
 terminal(m,{2.f,7.4f},"BOARD MINUTES / Q3","QUOTA UP 18 PERCENT. SCREENING INTERVAL EXTENDED FROM 30 TO 90 DAYS. MOTION CARRIED.",
  "EXPOSURE TIER 2 RECLASSIFIED: FIT FOR SHIFT. TIER 3 TO OBSERVATION AT MEDICAL'S DISCRETION.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfE;
 for(Vec2 l:{Vec2{6.2f,4.1f},Vec2{3.f,8.4f}})m.lights.push_back(lamp(l,f+3.4f,.95f,6.f,4600,.08f));
 // --- The corridor and the records office, south-west, behind glass ------------------------------
 wall(m,1.f,12.f,5.f,12.2f,f,roof,3);wall(m,6.5f,12.f,12.f,12.2f,f,roof,3);rfFrame(m,5.f,6.5f,12.1f,true,f,roof,3);
 {Door d{5.f,6.5f,12.1f,0};d.requireState=gone;m.doors.push_back(d);}   // 1: records, open once he has gone
 rfGlass(m,12.f,12.2f,12.2f,21.f,f,roof);
 wall(m,1.f,21.f,3.f,21.2f,f,roof,3);wall(m,4.5f,21.f,12.2f,21.2f,f,roof,3);rfFrame(m,3.f,4.5f,21.1f,true,f,roof,3);
 {Door d{3.f,4.5f,21.1f,0};d.requireState=stateId("rf_admin_far_door");m.doors.push_back(d);}   // 2: the far door: his, never the player's
 // The service closet he leaves into, and the solid block east of it.
 wall(m,6.f,21.2f,12.2f,23.f,f,roof,3);
 for(float x:{1.6f,2.6f,3.6f})for(float y:{14.f,16.f,18.f})rfFix(m,model::file_cabinet_tall,x,y,f,.5f,.6f,1.3f,rfE,true);
 for(float y:{13.2f,15.2f,17.2f,19.2f})rfFix(m,model::file_cabinet_tall,11.4f,y,f,.5f,.6f,1.3f,rfW,true);
 rfFix(m,model::desk,7.f,17.f,f,2.205f,1.092f,.891f,0,true);
 for(float x:{6.4f,7.6f})rfFix(m,model::binder,x,16.9f,f+.891f,.3f,.08f,.32f,x,false);
 rfFix(m,model::lab_desk_lamp,7.9f,17.3f,f+.891f,.18f,.4f,.52f,rfS,false);
 terminal(m,{8.4f,20.5f},"PERSONNEL CARD INDEX","ENERGY DIAGNOSTICS: 14 CARDS ON FILE. 13 IN THE DRAWER.",
  "LAB 4 ANALYST: CARD SIGNED OUT 04:02 BY INTERNAL INVESTIGATIONS. REASON: CONTAINMENT.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfN;
 // Lit for the glass: the scene in here must read from the lobby.
 for(Vec2 l:{Vec2{3.f,14.f},Vec2{3.f,18.6f},Vec2{7.f,15.f},Vec2{7.f,19.f},Vec2{10.f,17.f}})m.lights.push_back(lamp(l,f+3.f,1.5f,7.f,4800,.06f));
 m.lights.push_back(lamp({7.9f,17.3f},f+1.3f,.9f,3.5f,2800,.2f,WorldLightMount::Bare));
 // --- The detail ----------------------------------------------------------------------------------
 // One man holds the records door; one walks the lobby. They take the
 // Investigator's orders by hand through the glass.
 const char* clock="rf_admin_clock";
 rfGuard(m,clock,"rf_admin_door",ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,"rf_admin",{{{12.9f,11.1f},f,60000,rfW}},13,.6f,1,1.f,0,130);
 rfGuard(m,clock,"rf_admin_walk",ActorVisual::Cleanup,GuardSquadRole::Assault,"rf_admin",{{{16.5f,19.f},f,5000,rfN},{{20.5f,10.f},f,4000,rfW},{{13.5f,13.5f},f,4000,rfS}},14,.6f,1,1.3f);
 // --- The scene through the glass -------------------------------------------------------------------
 rfSequence(m,"rf_admin_scene","rf_admin_leaving",30000,21000,17.f,9.f,24.f,14.f);
 // The Investigator himself (ActorVisual::Investigator, clips: 0 idle, 1 walk,
 // 7 reading a card, 8 pointing). On the scene's clock: he reads, points his
 // door guard to the drawers, takes one card, walks to the far door and is gone.
 {const Vec2 drawers{5.f,17.f},table{6.3f,16.3f},door{3.75f,20.5f},closet{3.75f,22.25f};
  const float toDoor=std::atan2(door.x-table.x,door.y-table.y);
  ActorTrack inv;inv.timerState=stateId("rf_admin_scene");inv.visual=ActorVisual::Investigator;inv.scale=1.8f;
  inv.keys={{0,drawers,f,rfE,7,0},{8800,drawers,f,rfE,7,4.9f},
   {9000,drawers,f,rfE,8,0},{10500,drawers,f,rfE,8,1},{12200,drawers,f,rfE,8,1},
   {12400,drawers,f,rfE,1,0},{13600,table,f,rfE,1,1.1f},
   {13800,table,f,rfE,7,0},{17600,table,f,rfE,7,2.9f},
   {17800,table,f,toDoor,1,0},{21800,door,f,toDoor,1,3.6f},{23600,closet,f,rfS,1,5.2f},{24000,closet,f,rfS,0,0},{30000,closet,f,rfS,0,1}};
  m.actorTracks.push_back(std::move(inv));}
 // At 21 s the far door opens for him; four seconds later it shuts and the floor is quiet.
 rfSequence(m,"rf_admin_exit","rf_admin_gone",5000,4000);
 {ScriptEvent e;e.id=stateId("rf_admin_door_open");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;e.requireState=stateId("rf_admin_leaving");
  ScriptAction open;open.type=ScriptAction::Type::OpenDoor;open.index=2;ScriptAction start;start.type=ScriptAction::Type::SetState;start.id=stateId("rf_admin_exit");start.value=1;
  e.actions={open,start};m.scriptEvents.push_back(e);}
 {ScriptEvent e;e.id=stateId("rf_admin_door_shut");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.bottom=-100;e.top=100;e.requireState=gone;
  ScriptAction shut;shut.type=ScriptAction::Type::CloseDoor;shut.index=2;ScriptAction save;save.type=ScriptAction::Type::Checkpoint;
  e.actions={shut,save};m.scriptEvents.push_back(e);}
 const Vec2 records{6.6f,16.f};
 cue(m,"rf_admin_scene",2500,Sound::Empty,records,"INVESTIGATOR: Every card from Energy Diagnostics. Bag them.",.7f,f+1.6f,"inv_admin_1");
 cue(m,"rf_admin_scene",6000,Sound::Empty,{12.9f,11.1f},"CLEANUP: Sir.",.6f,f+1.6f,"rf_admin_door_1");
 cue(m,"rf_admin_scene",9500,Sound::Empty,records,"INVESTIGATOR: The rest of this floor can burn with the building.",.7f,f+1.6f,"inv_admin_2");
 cue(m,"rf_admin_scene",15400,Sound::Empty,Vec2{6.3f,16.3f},"INVESTIGATOR: Not this one. This one comes with me.",.7f,f+1.6f,"inv_admin_3");
 cue(m,"rf_admin_scene",21100,Sound::Door,{3.75f,21.1f},"",.8f,f+1.2f);
 cue(m,"rf_admin_exit",4100,Sound::Door,{3.75f,21.1f},"",.7f,f+1.2f);
 // --- What the night left -------------------------------------------------------------------------
 // The boardroom: the binders of quarters past, the minutes still on the table.
 rfFix(m,model::office_bookcase,1.27f,4.1f,f,2.025f,.5f,2.5f,rfE,true);
 rfFix(m,model::office_bookcase,9.8f,1.27f,f,2.025f,.5f,2.5f,rfS,true);
 for(int i=0;i<7;++i)rfFix(m,model::office_file_binder,1.2f,3.35f+i*.12f,f+1.27f,.093f,.301f,.424f,rfE,false);
 for(int i=0;i<5;++i)rfFix(m,model::office_file_binder,9.2f+i*.12f,1.2f,f+.64f,.093f,.301f,.424f,rfS,false);
 for(auto [x,y,yaw]:{std::tuple{5.4f,3.25f,.1f},{7.f,3.f,-.2f},{6.9f,5.05f,3.f},{5.3f,5.3f,3.3f}})rfFix(m,model::office_paper_stack,x,y,f+.89f,.33f,.44f,.03f,yaw,false);
 // Records: the cabinets he had pulled, the cards he did not want, the boxes for the rest.
 rfFix(m,model::office_file_cabinet_open,7.6f,12.56f,f,1.451f,.699f,1.054f,rfS,true);
 rfFix(m,model::prop_cardboard_box_open,9.5f,14.1f,f,.73f,.725f,.726f,.2f,true);
 rfFix(m,model::prop_cardboard_box,9.5f,19.6f,f,.727f,.72f,.367f,0,true);
 rfFix(m,model::prop_cardboard_box,9.52f,19.62f,f+.367f,.727f,.72f,.367f,-.2f,true);
 paperDrift(m,4.4f,12.8f,10.6f,20.4f,f,14,70);
 for(auto [x,y,yaw]:{std::tuple{4.6f,14.9f,.3f},{5.4f,16.1f,1.9f},{4.2f,18.6f,.8f},{6.f,19.5f,2.6f},{8.3f,15.2f,1.1f},{9.2f,17.f,.2f},{6.8f,13.4f,2.f},{8.9f,14.6f,.6f}})
  rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{6.45f,17.25f,.4f},{7.35f,16.75f,2.2f},{6.75f,16.7f,1.f}})rfFix(m,model::item_id_card,x,y,f+.891f,.132f,.085f,.002f,yaw,false);
 // The lobby: the waiting group nobody waited in, and the first boxes carried out.
 rfFix(m,model::office_couch_green,16.6f,22.3f,f,2.242f,1.364f,1.192f,rfN,true);
 rfFix(m,model::office_armchair_green,14.4f,21.2f,f,1.399f,1.364f,1.192f,rfE,true);
 rfFix(m,model::prop_cardboard_box,13.f,17.6f,f,.727f,.72f,.367f,.1f,true);
 rfFix(m,model::prop_cardboard_box_open,13.f,18.45f,f,.73f,.725f,.726f,-.1f,true);
 paperDrift(m,13.f,12.6f,22.f,20.4f,f,6,701);
 extinguisher(m,23.f,7.8f,f,rfW);
 rfFix(m,model::trash_can,22.5f,20.4f,f,.5f,.5f,.62f,0,true);
 rfFix(m,model::lab_pa_horn,22.72f,6.f,f+3.5f,.495f,.564f,.433f,rfW,false);
 rfFix(m,model::lab_photocopier,10.6f,20.6f,f,.826f,.752f,.967f,rfN,true);
 // The lobby's walls: what the company hung where visitors waited.
 rfFix(m,model::art_painting_5,12.24f,7.4f,f+1.45f,1.156f,.08f,.983f,rfE,false);
 rfFix(m,model::art_painting_6,12.24f,3.f,f+1.3f,.689f,.03f,.8f,rfE,false);
 rfFix(m,model::art_painting_4,20.f,22.97f,f+1.55f,1.089f,.038f,.9f,rfN,false);
 // And the case it was proud of: the first core out of the deep seam, under glass.
 rfFix(m,model::office_display_cabinet,22.72f,17.2f,f,.968f,.54f,2.1f,rfW,true);
 rfFix(m,model::lab_anomaly_relic,22.75f,17.05f,f+1.02f,.25f,.2f,.2f,.6f,false);
 m.lights.push_back(lamp({21.9f,17.2f},f+2.3f,.7f,2.6f,6800,-.15f,WorldLightMount::Bare));   // the case lit for visitors
 // The boardroom: flowers nobody chose, over the minutes.
 rfFix(m,model::art_painting_6,4.6f,9.97f,f+1.3f,.689f,.03f,.8f,rfN,false);
 // Picture lights: a warm wash on each canvas.
 for(auto [x,y]:{std::pair{12.6f,7.4f},{12.6f,3.f},{20.f,22.5f},{4.6f,9.5f}})m.lights.push_back(lamp({x,y},f+2.5f,.55f,2.2f,3000,.12f,WorldLightMount::Bare));
 // --- Rewards and names ---------------------------------------------------------------------------
 m.pickupSpawns={{{22.f,22.f},PickupKind::Ammo,f},{{2.f,2.f},PickupKind::Health,f},{{13.f,2.6f},PickupKind::Ammo,dz}};
 m.clutterSpawns={{0,{7.f,17.4f},f+.891f},{2,{21.f,22.f},f}};
 zone(m,"RESEARCH ADMINISTRATION / BOARDROOM",1.f,1.f,12.f,10.f);
 zone(m,"RESEARCH ADMINISTRATION / RECORDS",1.f,12.2f,12.f,21.f);
 zone(m,"RESEARCH ADMINISTRATION / OBSERVATION GALLERY",12.f,1.f,23.f,5.f,dz-.5f,1000);
 zone(m,"RESEARCH ADMINISTRATION",0,0,24,24);
 recessedFittings(m,true);freightFinish(m,-1);
 return p;
}
}
void buildChapterResearchFront(CampaignMapTable& maps){
 maps[68]=makeResearchIntake();
 maps[69]=makePersonnelProcessing();
 maps[70]=makeResearchAdministration();
}
}

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

// RESEARCH INTAKE (manifest 45). The great door opens onto the clean world the
// plant was paying for: an interlock vestibule, a biometric turnstile line, the
// intake security booth, a reception desk with its welcome film, and two air-
// shower booths nobody walked through tonight. A Cleanup detail holds the
// reception. As the player steps through, the detail's radio comes alive: the
// Investigator, heard for the first time, orders the booths swept, and two
// more men who were standing at the desk go and do it.
std::shared_ptr<const Map> makeResearchIntake(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[68];m.definition=CampaignChunks[68];m.definition.ambient=.40f;
 m.wallSurface=SurfacePaintedGreen;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 const StateId sweep=stateId("rf_intake_sweep");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 // Door 0, the great door's inner leaf, stays sealed until Chapter 2 ships: no
 // ordinary playthrough sets chapter2_open. Playtesters open it with the
 // console's UNLOCK while looking at it, MAP 68, or developer mode.
 openEdge(m,{'N',9.f,15.f,f,true,stateId("chapter2_open")});m.doors.back().sign=CampaignChunkCount+2;
 openEdge(m,{'S',10.f,14.f,f,false,0});
 rfSeamHeader(m,'S',10.f,14.f,f,3.2f);
 machineryClock(m,"rf_intake_clock",60000);
 // --- The interlock vestibule ---------------------------------------------------------------
 wall(m,7.8f,1.f,8.f,3.4f,f,roof,3);wall(m,16.f,1.f,16.2f,3.4f,f,roof,3);
 rfGlass(m,8.f,3.4f,10.5f,3.6f,f,roof);rfGlass(m,13.5f,3.4f,16.2f,3.6f,f,roof);rfFrame(m,10.5f,13.5f,3.5f,true,f,roof,3);
 m.doors.push_back({10.5f,13.5f,3.5f,0});          // 1: the inner interlock leaf
 floorLine(m,8.2f,1.2f,15.8f,1.32f,f,13);
 rfFix(m,model::lab_id_scanner,10.2f,3.38f,f+1.2f,.15f,.03f,.205f,rfN,false);
 m.lights.push_back(lamp({12.f,2.2f},roof-.16f,.9f,5.5f,6400,.03f));
 rfSign(m,12.f,3.62f,f+2.75f,"DEPTHWORKS ENERGY RESEARCH","INTAKE / ALL STAFF PRESENT BADGE AND BAND",4.4f,0,rfBlue);
 // --- The turnstile line ---------------------------------------------------------------------
 for(float x:{8.6f,10.f,11.4f,12.8f,14.2f,15.6f}){
  wall(m,x-.12f,5.7f,x+.12f,6.7f,f,f+1.f,20);wall(m,x-.14f,5.68f,x+.14f,6.72f,f+1.f,f+1.04f,12);
  rfFix(m,model::lab_id_scanner,x,5.9f,f+1.04f,.21f,.042f,.29f,rfN,false);}
 for(float x:{9.3f,10.7f,12.1f,13.5f,14.9f})floorLine(m,x-.45f,7.f,x+.45f,7.12f,f,13);
 wall(m,7.f,5.7f,8.48f,6.7f,f,f+1.f,20);wall(m,15.72f,5.7f,16.4f,6.7f,f,f+1.f,20);
 // --- Intake security booth, west --------------------------------------------------------------
 rfGlass(m,7.f,3.6f,7.2f,8.f,f,roof);
 wall(m,1.f,8.f,3.f,8.2f,f,roof,3);wall(m,4.4f,8.f,7.2f,8.2f,f,roof,3);rfFrame(m,3.f,4.4f,8.1f,true,f,roof,3);
 m.doors.push_back({3.f,4.4f,8.1f,0});              // 2: booth door
 rfFix(m,model::lab_control_board,4.2f,3.72f,f+1.f,1.32f,.19f,.6f,rfS,false);
 rfDesk(m,5.6f,5.4f,f,rfE,1);
 for(float y:{4.4f,5.5f}){rfFix(m,model::file_cabinet_wide,1.5f,y,f,.9f,.5f,.75f,rfE,true);rfFix(m,model::cctv_tv,1.5f,y,f+.75f,.5f,.4f,.42f,rfE,false);}
 rfFix(m,model::file_cabinet_tall,1.4f,7.3f,f,.5f,.6f,1.3f,rfE,true);
 rfFix(m,model::lab_wall_clock,4.f,3.63f,f+2.35f,.4f,.03f,.4f,rfS,false);
 terminal(m,{6.75f,4.6f},"INTAKE SECURITY / SHIFT LOG","01:58 SURGE ALARM. INTAKE LOCKED BY ORDER 7. 41 BANDS INSIDE.",
  "03:40 INTERNAL INVESTIGATIONS IN BY SERVICE LIFT 2. ALL DETAILS NOW REPORT TO THE INVESTIGATOR.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfE;
 m.lights.push_back(lamp({4.f,5.8f},roof-.16f,.85f,5.f,5200,.03f));
 // --- Reception, east -----------------------------------------------------------------------------
 wall(m,16.4f,3.6f,16.6f,4.6f,f,roof,3);
 rfFix(m,model::desk_corner,18.2f,6.3f,f,2.205f,2.211f,.891f,rfW,true);
 rfFix(m,model::desk,18.3f,8.f,f,2.205f,1.092f,.891f,0,true);
 rfFix(m,model::crt_monitor,18.7f,6.6f,f+.891f,.42f,.4f,.38f,rfE,false);
 rfFix(m,model::desk_phone,18.9f,8.1f,f+.891f,.2f,.2f,.1f,rfE,false);
 rfFix(m,model::office_chair,19.95f,6.4f,f,.55f,.55f,.95f,rfW,true);
 rfFix(m,model::lab_potted_plant,22.4f,4.2f,f,.5f,.58f,.93f,0,false);
 rfFix(m,model::water_cooler,16.95f,11.3f,f,.35f,.35f,1.1f,rfE,true);
 for(float y:{9.6f,11.f})bench(m,21.3f,y,f);
 // The welcome film, running for nobody since 01:58.
 terminal(m,{22.7f,6.4f},"WELCOME TO ENERGY RESEARCH","YOUR WORK POWERS THE WORLD. EVERY SHIFT BELOW BRINGS LIGHT ABOVE.",
  "BANDS STAY ON. SCREENING IS FOR YOUR SAFETY. ASK YOUR SUPERVISOR ABOUT EXPOSURE TIERS.");m.terminals.back().yaw=rfW;
 rfFix(m,model::cctv_tv,22.73f,7.7f,f+1.6f,.6f,.5f,.5f,rfW,false);
 for(Vec2 l:{Vec2{19.5f,5.8f},Vec2{19.5f,9.8f}})m.lights.push_back(lamp(l,roof-.16f,.95f,6.f,6000,.04f));
 // --- The air-shower booths, west ----------------------------------------------------------------
 rfBooth(m,6.8f,8.2f,12.f,f,roof,9.4f,10.8f);
 rfBooth(m,6.8f,12.f,15.8f,f,roof,13.2f,14.6f);
 wall(m,1.f,15.8f,7.f,16.f,f,roof,27);
 // --- The hall and the lounge ---------------------------------------------------------------------
 ceilingGrid(m,7.4f,3.8f,16.4f,20.f,roof);
 for(Vec2 l:{Vec2{9.5f,9.f},Vec2{14.5f,9.f},Vec2{9.5f,14.f},Vec2{14.5f,14.f},Vec2{12.f,18.5f}})m.lights.push_back(lamp(l,roof-.16f,.95f,6.5f,6200,.04f));
 wall(m,16.6f,12.f,19.f,12.2f,f,roof,3);wall(m,21.f,12.f,23.f,12.2f,f,roof,3);rfFrame(m,19.f,21.f,12.1f,true,f,roof,3);
 for(float x:{17.8f,20.2f})bench(m,x,15.f,f);
 rfFix(m,model::coffee_machine,22.6f,13.f,f+.9f,.4f,.45f,.5f,rfW,false);
 rfFix(m,model::file_cabinet_wide,22.55f,13.f,f,.9f,.5f,.9f,rfW,true);
 rfFix(m,model::lab_notice_board,22.92f,16.6f,f+1.3f,1.2f,.07f,.9f,rfW,false);
 rfFix(m,model::lab_potted_plant,17.f,19.4f,f,.5f,.58f,.93f,0,false);rfFix(m,model::lab_potted_plant,22.4f,19.4f,f,.5f,.58f,.93f,0,false);
 terminal(m,{22.7f,18.f},"INTAKE / VISITOR NOTICES","SCREENING CALLS BY TIER. TIER 2 AND ABOVE REPORT TO MEDICAL SCREENING, EAST OF PROCESSING.",
  "LOST BANDS ARE A SAFETY BREACH. A BREACH IS REPORTED, NOT FORGIVEN.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=rfW;
 for(Vec2 l:{Vec2{18.f,16.f},Vec2{21.5f,16.f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,5600,.05f));
 // --- The way south ---------------------------------------------------------------------------------
 wall(m,1.f,20.f,10.f,20.2f,f,roof,3);wall(m,14.f,20.f,23.f,20.2f,f,roof,3);
 wall(m,9.8f,20.2f,10.f,23.8f,f,roof,3);wall(m,14.f,20.2f,14.2f,23.8f,f,roof,3);
 floorLine(m,10.2f,21.f,13.8f,21.12f,f,13);
 m.lights.push_back(lamp({12.f,22.f},roof-.16f,.8f,5.f,6000,.04f));
 // --- The intake detail --------------------------------------------------------------------------
 // The leader keeps the desk and the radio; a second man walks the hall. Two
 // more stand at the desk waiting for orders: the sweep sends them to the booths.
 const char* clock="rf_intake_clock";
 rfGuard(m,clock,"rf_intake_lead",ActorVisual::Cleanup,GuardSquadRole::Assault,"rf_intake",{{{17.4f,9.6f},f,6000,rfW},{{12.f,9.2f},f,4000,rfN}},14,.6f,1,1.3f,0,110);
 rfGuard(m,clock,"rf_intake_hall",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"rf_intake",{{{12.f,13.5f},f,5000,rfN},{{12.f,18.6f},f,5000,rfS}},13,.6f,2,1.2f);
 rfGuard(m,clock,"rf_intake_sweep_a",ActorVisual::CleanupRusher,GuardSquadRole::Rusher,"rf_intake",{{{20.2f,9.f},f,3000,rfW},{{8.f,10.1f},f,3500,rfW},{{8.f,13.9f},f,3500,rfW},{{10.f,16.5f},f,2500,rfS}},13,.6f,1,1.4f,sweep);
 rfGuard(m,clock,"rf_intake_sweep_b",ActorVisual::CleanupEngineer,GuardSquadRole::Engineer,"rf_intake",{{{20.8f,10.4f},f,3000,rfW},{{9.6f,12.f},f,4000,rfW},{{12.f,6.6f},f,3000,rfN}},12,.6f,2,1.2f,sweep);
 // --- The Investigator on the radio ---------------------------------------------------------------
 // Stepping into the vestibule starts it; the order at 13 s sets the sweep going.
 rfSequence(m,"rf_intake_radio","rf_intake_sweep",20000,13000,8.f,1.f,16.f,3.4f);
 const Vec2 radio{17.4f,9.6f};
 cue(m,"rf_intake_radio",800,Sound::Empty,radio,"INVESTIGATOR (RADIO): Intake detail. Report.",.6f,f+1.5f,"inv_intake_1");
 cue(m,"rf_intake_radio",3800,Sound::Empty,radio,"RADIO: Intake holding, sir. We heard the great door go.",.6f,f+1.5f,"rf_intake_lead_1");
 cue(m,"rf_intake_radio",7600,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then someone opened it. Sweep the booths. Anyone without a band is exposed.",.6f,f+1.5f,"inv_intake_2");
 cue(m,"rf_intake_radio",12200,Sound::Empty,radio,"RADIO: ...Copy. Booths. Move.",.6f,f+1.5f,"rf_intake_lead_2");
 cue(m,"rf_intake_radio",15600,Sound::Empty,radio,"INVESTIGATOR (RADIO): And Intake. If it is one of ours, it is still exposed.",.6f,f+1.5f,"inv_intake_3");
 // --- What the night left -------------------------------------------------------------------------
 // The counter: dosimeters handed in at the surge and never read, the detail's
 // spare radio, the intake sheets. Under it, the clerk's machine still on.
 const float top=f+.891f;
 for(auto [x,y,yaw]:{std::tuple{18.f,7.75f,.3f},{18.3f,8.05f,-.2f},{18.62f,7.8f,1.4f},{19.1f,7.7f,.1f}})rfFix(m,model::item_dosimeter,x,y,top,.31f,.146f,.066f,yaw,false);
 rfFix(m,model::item_walkie,17.55f,7.95f,top,.114f,.376f,.046f,.5f,false);
 rfFix(m,model::office_paper_stack,18.95f,8.25f,top,.33f,.44f,.03f,rfE+.1f,false);
 rfFix(m,model::office_pc_tower,19.85f,7.35f,f,.262f,.609f,.609f,rfW,true);
 // The booth: two boxes of confiscated badges, one taped, one still open.
 rfFix(m,model::prop_cardboard_box,2.55f,7.45f,f,.727f,.72f,.367f,.2f,true);
 rfFix(m,model::prop_cardboard_box,2.57f,7.4f,f+.367f,.727f,.72f,.367f,-.1f,true);
 rfFix(m,model::prop_cardboard_box_open,3.55f,4.45f,f,.73f,.725f,.726f,0,true);
 for(auto [x,y,yaw]:{std::tuple{3.1f,4.95f,.4f},{4.2f,5.05f,2.f},{5.45f,4.95f,1.f}})rfFix(m,model::item_id_card,x,y,x>5.f?f+.75f:f,.132f,.085f,.002f,yaw,false);
 paperDrift(m,1.6f,4.f,6.4f,7.6f,f,6,68);
 // The lounge: the couch under the plants, an armchair by the coffee.
 rfFix(m,model::office_couch_green,19.7f,19.28f,f,2.242f,1.364f,1.192f,rfN,true);
 rfFix(m,model::office_armchair_green,22.25f,14.45f,f,1.399f,1.364f,1.192f,rfW,true);
 rfFix(m,model::mug,22.5f,13.42f,f+.9f,.09f,.09f,.1f,0,false);
 // The hall: bands dropped on the way to the booths, the sheets that went with them.
 paperDrift(m,8.6f,7.4f,15.6f,12.6f,f,11,681);
 paperDrift(m,17.2f,4.4f,22.6f,11.4f,f,5,682);
 for(auto [x,y,yaw]:{std::tuple{9.7f,8.1f,.6f},{13.4f,7.6f,2.4f},{8.4f,11.2f,1.2f},{5.2f,11.f,.3f},{4.1f,13.8f,2.6f}})rfFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 extinguisher(m,15.98f,2.6f,f,rfW);extinguisher(m,10.f,22.4f,f,rfE);
 // The hall's south wall: the notices, the plants, the copier reception shared.
 rfFix(m,model::lab_notice_board,8.6f,19.96f,f+1.3f,1.2f,.07f,.9f,rfN,false);
 rfFix(m,model::lab_potted_plant,15.6f,19.5f,f,.5f,.58f,.93f,0,false);
 rfFix(m,model::lab_photocopier,20.8f,4.05f,f,.826f,.752f,.967f,rfS,true);
 rfFix(m,model::trash_can,22.6f,10.3f,f,.5f,.5f,.62f,0,true);
 // --- Rewards and names ---------------------------------------------------------------------------
 m.pickupSpawns={{{2.2f,6.6f},PickupKind::Ammo,f},{{3.9f,10.f},PickupKind::Health,f},{{21.6f,16.2f},PickupKind::Ammo,f}};
 m.clutterSpawns={{0,{19.3f,8.9f},f+.891f},{1,{17.9f,15.8f},f},{2,{9.f,19.2f},f}};
 zone(m,"RESEARCH INTAKE / INTERLOCK",8.f,1.f,16.f,3.4f);
 zone(m,"RESEARCH INTAKE / SECURITY BOOTH",1.f,3.6f,7.f,8.f);
 zone(m,"RESEARCH INTAKE / AIR SHOWERS",1.f,8.2f,6.8f,15.8f);
 zone(m,"RESEARCH INTAKE / RECEPTION",16.6f,3.6f,23.f,12.f);
 zone(m,"RESEARCH INTAKE / LOUNGE",16.6f,12.2f,23.f,20.f);
 zone(m,"RESEARCH INTAKE",0,0,24,24);
 titleCard(m,"RESEARCH FRONT",7.4f,3.8f,16.4f,20.f);
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

// DEPTHWORKS MEDICAL (chapter 2, manifest 48 (subchapter)): chunks 71, 72, 73, 74.
// Screening, Triage and Imaging are authored; Pharmacy is still the plan's
// placeholder. See docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- MedicalFront kit (md) ---------------------------------------------------------------
constexpr float mdS=0,mdN=kPi,mdE=kPi*.5f,mdW=-kPi*.5f;
void mdFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
// Clinic glazing: dado, sill, glass with mullions every 1.2 m, transom, bulkhead to `top`.
void mdGlass(Map& m,float x1,float y1,float x2,float y2,float f,float top,int mat=3){
 const float sill=.95f,head=2.4f;
 wall(m,x1,y1,x2,y2,f,f+sill,mat);wall(m,x1-.015f,y1-.015f,x2+.015f,y2+.015f,f+sill-.03f,f+sill+.03f,20);
 const bool alongX=x2-x1>y2-y1;const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)wall(m,x1,cy-.012f,x2,cy+.012f,f+sill+.03f,f+head,17);else wall(m,cx-.012f,y1,cx+.012f,y2,f+sill+.03f,f+head,17);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;
  if(alongX)wall(m,std::min(t,x2-.05f),y1,std::min(t+.05f,x2),y2,f+sill+.03f,f+head,20);
  else wall(m,x1,std::min(t,y2-.05f),x2,std::min(t+.05f,y2),f+sill+.03f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.07f,20);if(top>f+head+.07f)wall(m,x1,y1,x2,y2,f+head+.07f,top,mat);
}
// A doorway frame in a wall along x at y, from a to b, with its lintel to the roof.
void mdFrame(Map& m,float a,float b,float y,float f,float roof,int mat=3){
 wall(m,a,y-.1f,b,y+.1f,f+2.65f,roof,mat);for(float x:{a-.08f,b})wall(m,x,y-.14f,x+.08f,y+.14f,f,f+2.7f,20);
}
// Somebody left where they fell: a scripted track held in the dead pose.
void mdBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
void mdBlood(Map& m,float x1,float y1,float x2,float y2,float f){wall(m,x1,y1,x2,y2,f+.004f,f+.010f,16);}
void mdGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,float sight,int weapon,float speed,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
// A reading desk facing north: desk, chair, monitor.
void mdDeskLocal(Map& m,float x,float y,float f){
 mdFix(m,model::desk,x,y,f,1.4f,.7f,.75f,mdN,true);
 mdFix(m,model::office_chair,x,y+.8f,f,.55f,.55f,.95f,mdN,true);
 mdFix(m,model::crt_monitor,x,y-.12f,f+.75f,.42f,.4f,.38f,mdS,false);
}
void mdSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1,float y1,float x2,float y2){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}

// MEDICAL SCREENING (manifest 48). The clinic front every shift was sent to
// "for routine monitoring": a waiting room, three glass screening booths, the
// nurses' desk, and the triage board where LAB 4's name is still lit. The
// Investigator's sweep came through here at three o'clock; what it left is on
// the floor of the booths. The way south into the deep wing is sealed.
std::shared_ptr<const Map> makeMedicalScreening(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[71];m.definition=CampaignChunks[71];m.definition.ambient=.30f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,true,stateId("med_override")});   // door 0: Ward C and the deep wing
 machineryClock(m,"md_screen_clock",60000);
 // --- The corridor through ------------------------------------------------------------------
 wall(m,1.f,8.6f,4.f,8.8f,f,roof,3);wall(m,6.f,8.6f,11.f,8.8f,f,roof,3);mdFrame(m,4.f,6.f,8.7f,f,roof);
 m.doors.push_back({4.f,6.f,8.7f,0});                                     // 1: waiting room
 mdGlass(m,11.f,8.6f,23.f,8.8f,f,roof);                                   // the booths' glass onto the corridor
 // The south rooms open off the corridor: the triage side west, the nurses' desk east.
 wall(m,1.f,14.4f,6.f,14.6f,f,roof,3);wall(m,7.5f,14.4f,10.f,14.6f,f,roof,3);mdFrame(m,6.f,7.5f,14.5f,f,roof);
 wall(m,14.f,14.4f,17.f,14.6f,f,roof,3);wall(m,18.5f,14.4f,23.f,14.6f,f,roof,3);mdFrame(m,17.f,18.5f,14.5f,f,roof);
 mdFrame(m,10.f,14.f,14.5f,f,roof);
 ceilingGrid(m,1.f,8.8f,23.f,14.4f,roof);
 for(Vec2 l:{Vec2{4.f,11.6f},Vec2{12.f,11.6f},Vec2{20.f,11.6f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,6000,.04f));
 {auto l=lamp({16.f,11.6f},roof-.16f,.8f,5.f,5600,.05f);l.flicker=.35f;m.lights.push_back(l);}
 // --- The waiting room, north-west ----------------------------------------------------------
 for(float y:{3.f,4.6f,6.2f})for(float x:{2.6f,5.6f})bench(m,x,y,f);
 mdFix(m,model::water_cooler,9.6f,1.5f,f,.35f,.35f,1.1f,mdS,true);
 mdFix(m,model::med_notice_board,1.04f,4.6f,f+1.3f,1.2f,.07f,.9f,mdE,false);
 mdFix(m,model::med_wall_clock,6.f,1.03f,f+2.3f,.41f,.034f,.39f,mdS,false);
 mdFix(m,model::lab_potted_plant,9.5f,7.8f,f,.5f,.58f,.93f,0,false);
 wall(m,10.6f,1.f,10.8f,8.6f,f,roof,3);
 terminal(m,{1.4f,7.2f},"ROUTINE MONITORING / A NOTE FOR STAFF","YOUR BAND KEEPS YOU SAFE. A SCREENING CALL IS ROUTINE, NOT A CONCERN.",
  "IF YOU ARE ASKED TO STAY FOR OBSERVATION, YOUR SHIFT PAY CONTINUES. YOUR FAMILY WILL BE CALLED.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=mdE;
 for(Vec2 l:{Vec2{4.f,3.6f},Vec2{8.f,5.6f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,5200,.05f));
 // --- The screening booths, north-east ------------------------------------------------------
 for(float x:{14.6f,18.6f})wall(m,x,1.f,x+.2f,8.6f,f,roof,27);
 for(float x0:{11.f,14.8f,18.8f}){const float cx=x0+1.9f;
  mdFix(m,model::med_vitals_monitor,cx+.8f,1.4f,f+.9f,.42f,.2f,.31f,mdS,false);
  mdFix(m,model::lab_field_meter,cx-.8f,1.38f,f+.9f,.25f,.33f,.15f,mdS+kPi*.5f,false);
  wall(m,cx-1.3f,1.1f,cx+1.3f,1.7f,f,f+.9f,20);
  mdFix(m,model::office_chair,cx,4.2f,f,.55f,.55f,.95f,mdN,true);
  m.lights.push_back(lamp({cx,4.f},roof-.16f,.85f,5.f,6400,.03f));}
 // What the sweep left: a doctor in the first booth, a technician in the third.
 mdBody(m,"md_screen_clock",ActorVisual::FemaleDoctor,{13.f,6.4f},f,2.4f);mdBlood(m,12.2f,5.8f,13.9f,7.1f,f);
 mdBody(m,"md_screen_clock",ActorVisual::FemaleScientist,{20.4f,6.6f},f,-.8f);mdBlood(m,19.5f,6.1f,21.2f,7.2f,f);
 for(auto [x,z]:{std::pair{13.1f,1.1f},{13.5f,1.4f},{20.4f,.9f}})wall(m,x,8.58f,x+.1f,8.6f,f+z,f+z+.1f,16);
 // --- The triage board and the nurses' desk, south ---------------------------------------
 terminal(m,{4.6f,14.25f},"TRIAGE BOARD / TONIGHT","TIER 2 HELD FOR SCREENING: 31. TIER 3 MOVED TO OBSERVATION: 9.",
  "LAB 4 / ENERGY DIAGNOSTICS / HELD FOR OBSERVATION / WARD C.");m.terminals.back().yaw=mdN;
 mdFix(m,model::med_nurse_desk,18.4f,17.4f,f,1.48f,.7f,1.01f,mdN,true);
 mdFix(m,model::file_cabinet_tall,22.5f,16.f,f,.5f,.6f,1.3f,mdW,true);
 mdFix(m,model::med_supply_cabinet,22.5f,19.f,f,1.1f,.44f,1.7f,mdW,true);
 mdFix(m,model::med_crash_cart,16.f,20.6f,f,.74f,.48f,1.f,.4f,true);
 mdFix(m,model::med_wheelchair,3.f,17.6f,f,.53f,.99f,.87f,1.2f,true);
 mdFix(m,model::med_ward_bed_tossed,5.f,20.8f,f,2.1f,1.52f,.84f,mdE+.3f+kPi*.5f,true);
 wall(m,10.f,14.6f,10.2f,23.8f,f,roof,3);wall(m,13.8f,14.6f,14.f,23.8f,f,roof,3);
 for(Vec2 l:{Vec2{5.f,18.6f},Vec2{12.f,18.f},Vec2{18.6f,19.6f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,4800,.06f));
 m.lights.push_back(lamp({12.f,22.4f},roof-.18f,.8f,4.f,1000,-.5f));
 // --- The sweep detail, still here ------------------------------------------------------------
 const char* clock="md_screen_clock";
 mdGuard(m,clock,"md_screen_a",ActorVisual::Cleanup,GuardSquadRole::Assault,"md_screen",{{{8.f,11.6f},f,4000,mdE},{{20.f,11.6f},f,4000,mdW}},14,1,1.3f,110);
 mdGuard(m,clock,"md_screen_b",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"md_screen",{{{18.4f,16.f},f,7000,mdN},{{16.f,18.f},f,4000,mdW}},16,2,1.1f);
 // --- The Investigator, over their radio ----------------------------------------------------------
 mdSequence(m,"md_screen_radio","md_screen_radio_done",16000,14000,1.f,8.8f,3.f,14.4f);
 const Vec2 radio{8.f,11.6f};
 cue(m,"md_screen_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Medical detail. The screening floor is closed. Anyone walking it is exposed.",.6f,f+1.5f,"inv_screen_1");
 cue(m,"md_screen_radio",6200,Sound::Empty,radio,"RADIO: Sir, we did the booths. There's nobody left to walk it.",.6f,f+1.5f,"md_screen_a_1");
 cue(m,"md_screen_radio",11000,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then you will not be busy.",.6f,f+1.5f,"inv_screen_2");
 // --- What the night left -------------------------------------------------------------------------
 // The waiting room: the slips people were holding when they were called, the bands they dropped.
 paperDrift(m,1.6f,1.6f,10.2f,8.2f,f,7,71);
 for(auto [x,y,yaw]:{std::tuple{4.1f,5.4f,.4f},{7.6f,2.2f,2.f},{8.8f,6.9f,1.1f}})mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 extinguisher(m,10.6f,4.f,f,mdW);
 // The booths: each counter's dosimeter reader and pills, a band near each of the dead.
 for(float x0:{11.f,14.8f,18.8f}){const float cx=x0+1.9f;
  mdFix(m,model::item_dosimeter,cx-.1f,1.4f,f+.9f,.31f,.146f,.066f,mdS+.2f,false);
  mdFix(m,model::med_pill_bottle,cx+.35f,1.35f,f+.9f,.07f,.03f,.13f,0,false);}
 for(auto [x,y,yaw]:{std::tuple{13.7f,7.4f,.6f},{19.8f,7.5f,2.3f},{16.9f,6.8f,1.4f}})mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 mdFix(m,model::item_bandage,16.5f,5.4f,f,.075f,.148f,.061f,.8f,false);
 paperDrift(m,11.4f,2.f,22.6f,8.2f,f,6,711);
 // The corridor, walked over in boots.
 paperDrift(m,3.f,9.2f,22.6f,14.f,f,9,712);
 extinguisher(m,15.5f,14.4f,f,mdN);
 // The triage side: a bed made up for observation, the kit on the wall.
 mdFix(m,model::med_ward_bed,1.5f,19.9f,f,1.98f,.94f,.84f,kPi*.5f,true);
 mdFix(m,model::med_first_aid_box,1.06f,16.8f,f+1.3f,.367f,.12f,.349f,mdE,false);
 paperDrift(m,1.6f,15.f,9.6f,22.6f,f,6,713);
 // The nurses' side: the list they were reading from, the cabinet they emptied.
 mdFix(m,model::office_paper_stack,18.f,17.4f,f+1.01f,.33f,.44f,.03f,mdN+.15f,false);
 for(auto [x,y,yaw]:{std::tuple{18.6f,17.3f,.3f},{18.85f,17.55f,1.9f}})mdFix(m,model::item_id_card,x,y,f+1.01f,.132f,.085f,.002f,yaw,false);
 mdFix(m,model::office_file_cabinet_open,21.5f,22.65f,f,1.451f,.699f,1.054f,mdN,true);
 mdFix(m,model::prop_cardboard_box_open,14.8f,22.4f,f,.73f,.725f,.726f,.3f,true);
 paperDrift(m,14.6f,15.f,22.6f,22.2f,f,7,714);
 // Second row of observation beds on the south wall, a curtain between them, the sink at the door.
 for(float x:{7.f,8.95f})mdFix(m,model::med_ward_bed,x,22.f,f,1.98f,.94f,.84f,kPi*.5f,true);
 mdFix(m,model::med_vitals_monitor,7.f,22.9f,f+1.2f,.38f,.181f,.28f,mdN,false);
 wall(m,7.95f,20.9f,8.01f,23.f,f+.2f,f+2.1f,39);m.pipes.push_back({{7.98f,20.9f},{7.98f,23.f},f+2.15f,.02f,f+2.15f,6});
 mdFix(m,model::med_scrub_sink,8.6f,14.87f,f,.488f,.536f,1.127f,mdS,true);
 mdFix(m,model::med_notice_board,1.04f,22.f,f+1.3f,1.2f,.07f,.9f,mdE,false);
 // The nurses' room: the drug fridge, the scrub sink, stock on the shelf, the chair pushed back.
 mdFix(m,model::med_scrub_sink,14.27f,16.4f,f,.488f,.536f,1.127f,mdE,true);
 mdFix(m,model::lab_sample_fridge,14.45f,17.6f,f,.7f,.85f,1.f,mdE,true);
 for(auto [x,y,yaw]:{std::tuple{14.3f,17.45f,.4f},{14.55f,17.75f,1.6f}})mdFix(m,model::med_pill_bottle,x,y,f+1.f,.07f,.03f,.13f,yaw,false);
 shelf(m,{20.6f,14.88f},mdS,0);
 mdFix(m,model::prop_cardboard_box,20.2f,14.9f,f+1.8f,.727f,.72f,.367f,0,false);
 mdFix(m,model::office_chair,18.1f,18.3f,f,.55f,.55f,.95f,mdN+.5f,true);
 mdFix(m,model::med_wheelchair,22.5f,20.9f,f,.482f,.9f,.787f,.15f,true);
 // The corridor: the cart they were pushing, the horn his voice came out of.
 mdFix(m,model::med_crash_cart,20.5f,14.1f,f,.74f,.48f,1.f,0,true);
 mdFix(m,model::lab_pa_horn,9.f,9.1f,f+2.6f,.495f,.564f,.433f,mdS,false);
 // Where the overflow sat: a bench against the corridor wall, the notices over the cart.
 bench(m,3.f,13.95f,f);
 mdFix(m,model::med_notice_board,21.5f,14.36f,f+1.3f,1.2f,.07f,.9f,mdN,false);
 // --- Rewards and names -------------------------------------------------------------------------
 m.pickupSpawns={{{9.f,3.f},PickupKind::Health,f},{{22.f,13.6f},PickupKind::Ammo,f},{{3.f,15.6f},PickupKind::Health,f}};
 m.clutterSpawns={{2,{5.4f,6.6f},f},{1,{18.2f,17.4f},f+1.f}};
 zone(m,"MEDICAL SCREENING / WAITING ROOM",1.f,1.f,10.6f,8.6f);
 zone(m,"MEDICAL SCREENING / BOOTHS",11.f,1.f,23.f,8.6f);
 zone(m,"MEDICAL SCREENING / TRIAGE DESK",1.f,14.6f,23.f,23.f);
 zone(m,"MEDICAL SCREENING",0,0,24,24);
 titleCard(m,"DEPTHWORKS MEDICAL",1.f,8.8f,23.f,14.4f);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// MEDICAL / TRIAGE (manifest 48). Five curtained bays, the band desk where
// every shift was given a colour, and the mess of a ward cleared in a hurry:
// beds tossed, a crash cart abandoned, bags of blood on the floor. A nurse
// sits slumped at the band desk. In the third bay a patient lies where he fell
// off his gurney. Turn your back on him to look at her, and he sits up.
std::shared_ptr<const Map> makeMedicalTriage(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[72];m.definition=CampaignChunks[72];m.definition.ambient=.24f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 machineryClock(m,"md_triage_clock",60000);
 // --- The way in and the band desk ------------------------------------------------------------
 wall(m,1.f,8.6f,5.f,8.8f,f,roof,3);wall(m,1.f,14.4f,5.f,14.6f,f,roof,3);
 wall(m,5.f,1.f,5.2f,8.8f,f,roof,3);wall(m,5.f,14.4f,5.2f,23.f,f,roof,3);
 ceilingGrid(m,5.2f,1.f,23.f,23.f,roof);
 mdFix(m,model::med_nurse_desk,8.2f,6.f,f,1.48f,.7f,1.01f,mdE,true);
 mdFix(m,model::med_vitals_monitor,8.1f,6.3f,f+1.01f,.42f,.2f,.31f,mdE,false);
 mdFix(m,model::office_chair,7.2f,6.f,f,.55f,.55f,.95f,mdE,true);
 for(float y:{2.f,3.2f})mdFix(m,model::file_cabinet_tall,5.6f,y,f,.5f,.6f,1.3f,mdE,true);
 mdFix(m,model::med_supply_cabinet,10.f,1.45f,f,1.1f,.44f,1.7f,mdS,true);
 mdFix(m,model::med_wall_clock,8.f,1.03f,f+2.3f,.41f,.034f,.39f,mdS,false);
 terminal(m,{9.2f,5.2f},"BAND DESK / ISSUE LOG","GREEN: TIER 1, BACK TO SHIFT. AMBER: TIER 2, SCREENING DUE. RED: TIER 3, WARD C.",
  "02:12 NO MORE AMBER BANDS. ISSUE RED.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=mdE;
 // The nurse at the desk (actor 0): looking at her is turning your back on the bays.
 actor(m,"md_triage_clock",ActorVisual::FemaleDoctor,1.7f,{{0,{7.3f,6.f},f,mdE,4,1},{1000,{7.3f,6.f},f,mdE,4,1}});
 mdBlood(m,6.7f,5.4f,8.f,6.8f,f);
 // --- The curtained bays, east ------------------------------------------------------------------
 for(float y:{4.6f,9.1f,13.6f,18.1f})wall(m,15.6f,y-.03f,23.f,y+.03f,f+.2f,f+2.1f,39);
 for(float y:{4.6f,9.1f,13.6f,18.1f})m.pipes.push_back({{15.6f,y},{23.f,y},f+2.15f,.02f,f+2.15f,6});
 const float bays[5]={2.6f,6.85f,11.35f,15.85f,20.4f};
 for(int i=0;i<5;++i){const float y=bays[i];
  if(i==1||i==3)mdFix(m,model::med_ward_bed_tossed,20.4f,y+.3f,f,2.1f,1.52f,.84f,mdE+.4f*(i-2)+kPi*.5f,true);
  else if(i!=2)mdFix(m,model::med_ward_bed,20.4f,y,f,1.98f,.94f,.84f,mdE+kPi*.5f,true);
  mdFix(m,model::med_vitals_monitor,22.88f,y-.7f,f+1.2f,.42f,.2f,.31f,mdW,false);
  m.lights.push_back(lamp({19.f,y},roof-.16f,.7f,4.5f,5000,.05f));}
 // Bay 3: the gurney he fell from, its frame twisted.
 mdFix(m,model::med_bed_frame_wrecked,20.6f,11.f,f,1.98f,.94f,.84f,mdE+.3f+kPi*.5f,true);
 for(auto [x,y]:{std::pair{18.4f,12.2f},{17.6f,7.8f},{13.f,16.6f}})mdBlood(m,x-.3f,y-.25f,x+.3f,y+.25f,f);
 mdFix(m,model::med_crash_cart,13.6f,10.f,f,.74f,.48f,1.f,.6f,true);
 mdFix(m,model::med_wheelchair,11.f,18.6f,f,.53f,.99f,.87f,2.4f,true);
 mdFix(m,model::med_ward_bed_tossed,9.6f,21.4f,f,2.1f,1.52f,.84f,.5f+kPi*.5f,true);
 mdFix(m,model::med_crutches,6.f,20.f,f,.17f,.06f,1.2f,.3f,false);
 // The patient (actor 1): on the floor beside the gurney, until his timer runs.
 // Turning to the nurse at the desk starts it; he comes up off the floor (the
 // fall played backwards, stopping on his knees) and stays there, facing you.
 const Vec2 patient{18.6f,11.3f};
 actor(m,"md_triage_rise",ActorVisual::Worker,1.7f,{{0,patient,f,mdW,4,1},{500,patient,f,mdW,4,1},{2300,patient,f,mdW,4,.5f},{2500,patient,f,mdW,5,0},{60000,patient,f,mdW,5,40.f}});
 // Sight is checked on top of the box: anywhere in the room, looking at her.
 {TimedSequence s;s.timerState=stateId("md_triage_rise");s.finishState=stateId("md_triage_risen");s.x1=0;s.y1=0;s.x2=24;s.y2=24;
  s.durationMs=60000;s.finishAtMs=3000;s.soundIntervalMs=0;s.soundUntilMs=0;s.sightActor=0;s.sightDistance=7.f;m.timedSequences.push_back(s);}
 cue(m,"md_triage_rise",600,Sound::WorkerDying,patient,"",.45f,f+.6f);
 cue(m,"md_triage_rise",2600,Sound::Metal1,{20.6f,11.f},"",.35f,f+.5f);
 // --- What the night left -------------------------------------------------------------------------
 // The band desk: the box the bands came from, the bands that spilled, the readers on the desk.
 mdFix(m,model::prop_cardboard_box_open,11.5f,1.75f,f,.73f,.725f,.726f,-.15f,true);
 for(auto [x,y,yaw]:{std::tuple{11.f,2.5f,.4f},{12.1f,2.7f,1.6f},{11.7f,3.1f,2.8f},{10.6f,3.4f,.9f},{12.6f,2.f,2.2f},{9.9f,5.3f,1.3f}})
  mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{8.3f,5.55f,.2f},{8.15f,5.8f,1.5f}})mdFix(m,model::item_dosimeter,x,y,f+1.01f,.31f,.146f,.066f,yaw,false);
 mdFix(m,model::office_paper_stack,8.35f,6.65f,f+1.01f,.33f,.44f,.03f,mdE,false);
 paperDrift(m,6.f,2.f,15.f,8.2f,f,8,72);paperDrift(m,1.4f,9.f,5.f,14.f,f,3,721);
 extinguisher(m,5.2f,7.6f,f,mdE);
 // A bed parked against the wall for the next one, the kit above it.
 mdFix(m,model::med_ward_bed,5.75f,16.6f,f,1.98f,.94f,.84f,kPi*.5f,true);
 mdFix(m,model::med_first_aid_box,5.26f,18.4f,f+1.3f,.367f,.12f,.349f,mdE,false);
 // The bays: dressings torn open where the bands were cut off.
 for(auto [x,y,yaw]:{std::tuple{17.f,3.5f,.4f},{16.8f,16.4f,1.9f},{17.2f,20.9f,2.7f},{16.4f,8.3f,1.2f}})mdFix(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw,false);
 paperDrift(m,6.f,9.f,15.f,22.4f,f,8,722);
 for(auto [x,y,yaw]:{std::tuple{8.4f,13.2f,.5f},{12.6f,17.f,2.f},{14.4f,21.2f,1.f}})mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 // The ward floor: beds shoved out of the bays in a hurry, a mattress dragged off one.
 mdFix(m,model::med_ward_bed,9.f,14.4f,f,1.98f,.94f,.84f,kPi*.5f+.25f,true);
 mdFix(m,model::med_ward_bed_tossed,12.5f,15.6f,f,2.1f,1.52f,.84f,kPi*.5f-.35f,true);
 mdFix(m,model::med_mattress_stained,11.2f,11.6f,f,1.9f,.9f,.118f,.7f,false);
 mdFix(m,model::med_supply_cabinet,8.f,22.75f,f,1.1f,.444f,1.7f,mdN,true);
 mdFix(m,model::med_scrub_sink,5.47f,21.9f,f,.488f,.536f,1.127f,mdE,true);
 mdFix(m,model::med_notice_board,5.24f,20.4f,f+1.3f,1.2f,.07f,.9f,mdE,false);
 // The band desk: more bands boxed on the shelf; the PA that read the colours out.
 shelf(m,{13.6f,1.27f},mdS,0);
 for(float x:{13.1f,14.f})mdFix(m,model::prop_cardboard_box,x,1.3f,f+1.8f,.727f,.72f,.367f,x*.3f,false);
 mdFix(m,model::lab_pa_horn,5.5f,4.f,f+2.6f,.495f,.564f,.433f,mdE,false);
 // Where they waited to be given a colour.
 for(float y:{3.2f,4.6f})bench(m,12.6f,y,f);
 // --- Light: the desk lit, the bays dimming, one tube going out over bay 3 ----------------------
 for(Vec2 l:{Vec2{8.f,4.f},Vec2{3.f,11.5f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,5200,.05f));
 {auto l=lamp({18.6f,11.3f},roof-.16f,.8f,4.f,5600,.04f);l.flicker=.6f;m.lights.push_back(l);}
 m.lights.push_back(lamp({10.f,15.f},roof-.16f,.7f,6.f,4200,.08f));
 m.lights.push_back(lamp({10.f,20.5f},roof-.16f,.6f,6.f,4400,.07f));
 // --- Rewards and names -------------------------------------------------------------------------
 m.pickupSpawns={{{9.8f,3.f},PickupKind::Health,f},{{22.2f,21.6f},PickupKind::Health,f},{{12.f,22.f},PickupKind::Ammo,f}};
 m.clutterSpawns={{2,{14.4f,12.6f},f},{1,{9.f,6.2f},f+1.f},{0,{12.f,6.f},f}};
 zone(m,"MEDICAL / TRIAGE / BAND DESK",5.2f,1.f,15.6f,8.6f);
 zone(m,"MEDICAL / TRIAGE / BAYS",15.6f,1.f,23.f,23.f);
 zone(m,"MEDICAL / TRIAGE",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// MEDICAL / IMAGING (manifest 48). The first room past the seal: two X-ray
// suites behind lead glass, the scanning bay, the reading room and its film
// archive. The films on the lightboxes are wrong: skulls grown through with
// something crystalline, and the readings say it is the same on everyone in
// tier 3. One of the scanned is still in the scanning bay, and awake.
std::shared_ptr<const Map> makeMedicalImaging(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[73];m.definition=CampaignChunks[73];m.definition.ambient=.20f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});   // door 0: Screening's sealed door, this side
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"md_imaging_clock",60000);
 // --- The spine corridor ------------------------------------------------------------------------
 // West wall: open doorways into the two X-ray suites and the scanning bay, a window onto suite 2.
 wall(m,8.8f,1.f,9.f,4.6f,f,roof,3);wall(m,8.8f,5.8f,9.f,7.2f,f,roof,3);wall(m,8.8f,8.4f,9.f,9.f,f,roof,3);mdGlass(m,8.8f,9.f,9.f,10.6f,f,roof);
 wall(m,8.8f,10.6f,9.f,13.6f,f,roof,3);wall(m,8.8f,15.f,9.f,23.8f,f,roof,3);
 for(float y:{4.6f,7.2f,13.6f})wall(m,8.75f,y,9.05f,y+1.2f,f+2.65f,roof,3);
 wall(m,15.f,1.f,15.2f,9.f,f,roof,3);wall(m,15.f,14.f,15.2f,23.8f,f,roof,3);
 ceilingGrid(m,9.f,1.f,15.f,23.f,roof);
 for(float y:{4.f,11.5f,19.f})m.lights.push_back(lamp({12.f,y},roof-.16f,.7f,5.5f,5400,.05f));
 {auto l=lamp({12.f,15.f},roof-.16f,.7f,5.f,5400,.05f);l.flicker=.5f;m.lights.push_back(l);}
 // --- X-ray suites, north-west: the tables behind a lead-glass control booth -----------------------
 wall(m,1.f,6.f,8.8f,6.2f,f,roof,27);
 for(float y0:{1.f,6.2f}){const float cy=y0+2.4f;
  mdFix(m,model::med_autopsy_table,5.2f,cy,f,1.98f,.81f,.95f,mdE+kPi*.5f,true);
  wall(m,4.6f,cy-.3f,5.8f,cy+.3f,f+2.3f,f+2.5f,20);m.pipes.push_back({{5.2f,cy},{5.2f,cy},f+2.5f,.05f,roof,6});
  mdFix(m,model::med_xray_viewer_skull,1.06f,cy-.6f,f+1.4f,.41f,.1f,.5f,mdE,false);
  mdFix(m,model::med_xray_viewer_chest,1.06f,cy+.6f,f+1.4f,.41f,.1f,.5f,mdE,false);
  m.lights.push_back(lamp({1.3f,cy},f+1.6f,.6f,3.f,7500,-.3f,WorldLightMount::Bare));
  m.lights.push_back(lamp({3.4f,cy},roof-.16f,.6f,4.5f,5600,.04f));}
 mdGlass(m,7.f,1.f,7.2f,4.2f,f,roof,27);   // the booth's lead glass, open at its south end
 mdFix(m,model::lab_control_board,8.7f,3.4f,f+.9f,1.32f,.19f,.6f,mdW,false);   // on the booth wall
 terminal(m,{8.3f,2.2f},"RADIOLOGY / READINGS","03:00 FRONTAL BONE: CRYSTALLINE GROWTH THROUGH THE SUTURES. SAME AS LAST WEEK'S. SAME AS EVERY TIER 3.",
  "DO NOT REPORT AS ABNORMAL. REPORT AS EXPOSURE, TIER 3. THE BOARD HAS ASKED FOR THE FILMS.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=mdW;
 // --- The scanning bay, south-west ----------------------------------------------------------------
 wall(m,1.f,11.8f,8.8f,12.f,f,roof,27);
 mdFix(m,model::lab_test_chamber,4.4f,16.6f,f,2.5f,1.5f,1.4f,mdE,true);
 mdFix(m,model::lab_control_board,8.7f,13.f,f+.9f,1.32f,.19f,.6f,mdW,false);   // on the bay wall
 mdFix(m,model::desk,8.3f,17.4f,f,1.4f,.7f,.75f,mdW,true);
 mdFix(m,model::lab_scope_monitor,8.35f,17.4f,f+.75f,.47f,.34f,.45f,mdW,false);
 mdFix(m,model::med_wheelchair,7.2f,20.6f,f,.53f,.99f,.87f,2.8f,true);
 mdBlood(m,5.8f,18.6f,7.4f,20.f,f);
 {auto l=lamp({4.4f,16.6f},roof-.2f,.9f,5.f,9000,-.4f);l.flicker=.4f;m.lights.push_back(l);}
 m.lights.push_back(lamp({4.4f,20.8f},roof-.16f,.6f,5.f,5200,.05f));
 m.lights.push_back(lamp({7.6f,17.4f},f+1.3f,.5f,3.f,7200,-.2f,WorldLightMount::Bare));   // the scope's glow
 m.creatureSpawns.push_back({CreatureKind::Transformed,{3.f,20.6f},f});
 // --- The reading room, north-east: lightboxes along the wall --------------------------------------
 wall(m,15.2f,8.8f,17.f,9.f,f,roof,3);wall(m,21.f,8.8f,23.f,9.f,f,roof,3);
 for(float x:{16.4f,17.6f,18.8f,20.f,21.2f})mdFix(m,model::med_xray_viewer_skull,x,1.06f,f+1.4f,.41f,.1f,.5f,mdS,false);
 for(float x:{17.f,20.6f})mdDeskLocal(m,x,3.2f,f);
 mdBody(m,"md_imaging_clock",ActorVisual::SeniorScientist,{18.8f,5.6f},f,1.9f);
 m.lights.push_back(lamp({18.8f,1.4f},f+1.5f,.8f,4.5f,7500,-.3f,WorldLightMount::Bare));
 m.lights.push_back(lamp({19.f,5.6f},roof-.16f,.6f,5.f,4800,.06f));
 // --- The film archive, south-east ----------------------------------------------------------------
 wall(m,15.2f,14.f,21.f,14.2f,f,roof,3);wall(m,22.6f,14.f,23.f,14.2f,f,roof,3);mdFrame(m,21.f,22.6f,14.1f,f,roof);
 for(float y:{15.4f,17.f,18.6f,20.2f})for(float x:{17.f,19.4f})mdFix(m,model::file_cabinet_tall,x,y,f,.5f,.6f,1.3f,mdS,true);
 mdFix(m,model::file_cabinet_tall,22.5f,21.6f,f,.5f,.6f,1.3f,mdW,true);
 m.lights.push_back(lamp({20.f,18.f},roof-.16f,.55f,5.f,4600,.08f));
 // --- What the night left -------------------------------------------------------------------------
 // The reading room: films on the floor, the boxes the board sent for them.
 paperDrift(m,15.6f,1.8f,22.6f,8.4f,f,10,73);
 for(auto [x,y,yaw]:{std::tuple{16.6f,3.1f,.1f},{20.25f,3.25f,-.15f}})mdFix(m,model::office_paper_stack,x,y,f+.75f,.33f,.44f,.03f,yaw,false);
 mdFix(m,model::prop_cardboard_box,16.f,7.9f,f,.727f,.72f,.367f,0,true);
 mdFix(m,model::prop_cardboard_box,16.02f,7.88f,f+.367f,.727f,.72f,.367f,.2f,true);
 mdFix(m,model::prop_cardboard_box_open,22.2f,7.9f,f,.73f,.725f,.726f,-.2f,true);
 // The film archive: the drawer cabinet pulled open, the envelopes thrown down.
 mdFix(m,model::office_file_cabinet_open,21.75f,22.65f,f,1.451f,.699f,1.054f,mdN,true);
 mdFix(m,model::prop_cardboard_box_open,15.9f,22.3f,f,.73f,.725f,.726f,.4f,true);
 paperDrift(m,15.6f,14.6f,22.6f,22.6f,f,10,731);
 // The scanning bay and the spine.
 for(auto [x,y,yaw]:{std::tuple{6.1f,18.9f,.5f},{7.7f,19.6f,2.f}})mdFix(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw,false);
 mdFix(m,model::item_id_card,5.5f,18.3f,f,.132f,.085f,.002f,1.2f,false);
 paperDrift(m,9.4f,2.f,14.6f,22.6f,f,6,732);
 extinguisher(m,15.f,20.f,f,mdW);extinguisher(m,2.f,6.f,f,mdN);
 // The archive's walls: shelving of boxed films, the boxes the board has not had yet.
 for(float y:{16.5f,18.6f}){shelf(m,{15.47f,y},mdE,0);mdFix(m,model::prop_cardboard_box,15.5f,y,f+1.8f,.727f,.72f,.367f,y,false);}
 for(float y:{15.6f,17.6f}){shelf(m,{22.73f,y},mdW,0);mdFix(m,model::prop_cardboard_box,22.7f,y+.3f,f+1.8f,.727f,.72f,.367f,-y,false);}
 for(auto [x,y]:{std::pair{18.2f,21.8f},{18.25f,21.82f}})mdFix(m,model::prop_cardboard_box,x,y,x>18.22f?f+.367f:f,.727f,.72f,.367f,x,true);
 // The scanning bay: the cabinet for the contrast, the fridge for the samples.
 mdFix(m,model::med_supply_cabinet,1.23f,14.f,f,1.1f,.444f,1.7f,mdE,true);
 mdFix(m,model::lab_sample_fridge,1.43f,12.55f,f,.7f,.85f,1.f,mdE,true);
 // The spine: gurneys parked against the wall for the morning.
 for(float y:{5.f,18.4f})mdFix(m,model::med_ward_bed,14.5f,y,f,1.98f,.94f,.84f,kPi*.5f,true);
 mdFix(m,model::med_wheelchair,9.5f,21.4f,f,.482f,.9f,.787f,.3f,true);
 // --- Rewards and names -------------------------------------------------------------------------
 m.pickupSpawns={{{2.f,3.f},PickupKind::Health,f},{{22.f,4.f},PickupKind::Ammo,f},{{22.f,16.f},PickupKind::Ammo,f}};
 m.clutterSpawns={{0,{18.f,3.4f},f+.75f},{2,{12.6f,20.f},f}};
 zone(m,"MEDICAL / IMAGING / X-RAY",1.f,1.f,8.8f,11.8f);
 zone(m,"MEDICAL / IMAGING / SCANNING BAY",1.f,12.f,8.8f,23.f);
 zone(m,"MEDICAL / IMAGING / READING ROOM",15.2f,1.f,23.f,8.8f);
 zone(m,"MEDICAL / IMAGING / FILM ARCHIVE",15.2f,14.2f,23.f,23.f);
 zone(m,"MEDICAL / IMAGING",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// MEDICAL / PHARMACY (manifest 48)
std::shared_ptr<const Map> makeMedicalPharmacy(){return placeholderChunk(74,{{'W',9.f,14.f,-12.f,false,0}});}
}
void buildChapterMedicalFront(CampaignMapTable& maps){
 maps[71]=makeMedicalScreening();
 maps[72]=makeMedicalTriage();
 maps[73]=makeMedicalImaging();
 maps[74]=makeMedicalPharmacy();
}
}

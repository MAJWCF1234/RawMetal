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
void mdGlass(Map& m,float x1,float y1,float x2,float y2,float f,float top,int mat=28){
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
void mdFrame(Map& m,float a,float b,float y,float f,float roof,int mat=28){
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
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,true,stateId("med_override")});   // door 0: Ward C and the deep wing
 machineryClock(m,"md_screen_clock",60000);
 // --- The corridor through ------------------------------------------------------------------
 wall(m,1.f,8.6f,4.f,8.8f,f,roof,28);wall(m,6.f,8.6f,11.f,8.8f,f,roof,28);mdFrame(m,4.f,6.f,8.7f,f,roof);
 m.doors.push_back({4.f,6.f,8.7f,0});                                     // 1: waiting room
 mdGlass(m,11.f,8.6f,23.f,8.8f,f,roof);                                   // the booths' glass onto the corridor
 // The south rooms open off the corridor: the triage side west, the nurses' desk east.
 wall(m,1.f,14.4f,6.f,14.6f,f,roof,28);wall(m,7.5f,14.4f,10.f,14.6f,f,roof,28);mdFrame(m,6.f,7.5f,14.5f,f,roof);
 wall(m,14.f,14.4f,17.f,14.6f,f,roof,28);wall(m,18.5f,14.4f,23.f,14.6f,f,roof,28);mdFrame(m,17.f,18.5f,14.5f,f,roof);
 mdFrame(m,10.f,14.f,14.5f,f,roof);
 ceilingGrid(m,1.f,8.8f,23.f,14.4f,roof);
 for(Vec2 l:{Vec2{4.f,11.6f},Vec2{12.f,11.6f},Vec2{20.f,11.6f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,6000,.04f));
 {auto l=lamp({16.f,11.6f},roof-.16f,.8f,5.f,5600,.05f);l.flicker=.35f;m.lights.push_back(l);}
 // --- The waiting room, north-west ----------------------------------------------------------
 for(float y:{3.f,4.6f,6.2f})for(float x:{2.6f,5.6f})bench(m,x,y,f);
 mdFix(m,model::water_cooler,9.6f,1.5f,f,.35f,.35f,1.1f,mdS,true);
 mdFix(m,model::med_notice_board,1.04f,4.6f,f+1.3f,1.2f,.06f,.9f,mdE,false);
 mdFix(m,model::med_wall_clock,6.f,1.04f,f+2.3f,.4f,.03f,.4f,mdS,false);
 mdFix(m,model::lab_potted_plant,9.5f,7.8f,f,.5f,.58f,.93f,0,false);
 wall(m,10.6f,1.f,10.8f,8.6f,f,roof,28);
 terminal(m,{1.4f,7.2f},"ROUTINE MONITORING / A NOTE FOR STAFF","YOUR BAND KEEPS YOU SAFE. A SCREENING CALL IS ROUTINE, NOT A CONCERN.",
  "IF YOU ARE ASKED TO STAY FOR OBSERVATION, YOUR SHIFT PAY CONTINUES. YOUR FAMILY WILL BE CALLED.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=mdE;
 for(Vec2 l:{Vec2{4.f,3.6f},Vec2{8.f,5.6f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,5200,.05f));
 // --- The screening booths, north-east ------------------------------------------------------
 for(float x:{14.6f,18.6f})wall(m,x,1.f,x+.2f,8.6f,f,roof,27);
 for(float x0:{11.f,14.8f,18.8f}){const float cx=x0+1.9f;
  mdFix(m,model::med_vitals_monitor,cx+.8f,1.4f,f+.9f,.4f,.3f,.35f,mdS,false);
  mdFix(m,model::lab_field_meter,cx-.8f,1.38f,f+.9f,.3f,.2f,.25f,mdS,false);
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
 mdFix(m,model::med_nurse_desk,18.4f,17.4f,f,1.7f,.8f,1.f,mdN,true);
 mdFix(m,model::med_records_cabinet,22.5f,16.f,f,.9f,.5f,1.3f,mdW,true);
 mdFix(m,model::med_supply_cabinet,22.5f,19.f,f,1.1f,.44f,1.7f,mdW,true);
 mdFix(m,model::med_crash_cart,16.f,20.6f,f,.6f,.5f,1.f,.4f,true);
 mdFix(m,model::med_wheelchair,3.f,17.6f,f,.65f,1.f,.95f,1.2f,true);
 mdFix(m,model::med_ward_bed_tossed,5.f,20.8f,f,1.f,2.1f,.9f,mdE+.3f,true);
 wall(m,10.f,14.6f,10.2f,23.8f,f,roof,28);wall(m,13.8f,14.6f,14.f,23.8f,f,roof,28);
 for(Vec2 l:{Vec2{5.f,18.6f},Vec2{18.6f,19.6f}})m.lights.push_back(lamp(l,roof-.16f,.8f,5.5f,4800,.06f));
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
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 machineryClock(m,"md_triage_clock",60000);
 // --- The way in and the band desk ------------------------------------------------------------
 wall(m,1.f,8.6f,5.f,8.8f,f,roof,28);wall(m,1.f,14.4f,5.f,14.6f,f,roof,28);
 wall(m,5.f,1.f,5.2f,8.8f,f,roof,28);wall(m,5.f,14.4f,5.2f,23.f,f,roof,28);
 ceilingGrid(m,5.2f,1.f,23.f,23.f,roof);
 mdFix(m,model::med_nurse_desk,8.2f,6.f,f,1.7f,.8f,1.f,mdE,true);
 mdFix(m,model::med_vitals_monitor,8.1f,6.3f,f+1.f,.4f,.3f,.35f,mdE,false);
 mdFix(m,model::office_chair,7.2f,6.f,f,.55f,.55f,.95f,mdE,true);
 for(float y:{2.f,3.2f})mdFix(m,model::med_records_cabinet,5.6f,y,f,.9f,.5f,1.3f,mdE,true);
 mdFix(m,model::med_supply_cabinet,10.f,1.45f,f,1.1f,.44f,1.7f,mdS,true);
 mdFix(m,model::med_wall_clock,8.f,1.04f,f+2.3f,.4f,.03f,.4f,mdS,false);
 terminal(m,{9.2f,5.2f},"BAND DESK / ISSUE LOG","GREEN: TIER 1, BACK TO SHIFT. AMBER: TIER 2, SCREENING DUE. RED: TIER 3, WARD C.",
  "02:12 NO MORE AMBER BANDS. ISSUE RED.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=mdE;
 // The nurse at the desk (actor 0): looking at her is turning your back on the bays.
 actor(m,"md_triage_clock",ActorVisual::FemaleDoctor,1.7f,{{0,{7.3f,6.f},f,mdE,4,1},{1000,{7.3f,6.f},f,mdE,4,1}});
 mdBlood(m,6.7f,5.4f,8.f,6.8f,f);
 // --- The curtained bays, east ------------------------------------------------------------------
 for(float y:{4.6f,9.1f,13.6f,18.1f})wall(m,15.6f,y-.03f,23.f,y+.03f,f+.2f,f+2.1f,27);
 for(float y:{4.6f,9.1f,13.6f,18.1f})m.pipes.push_back({{15.6f,y},{23.f,y},f+2.15f,.02f,f+2.15f,6});
 const float bays[5]={2.6f,6.85f,11.35f,15.85f,20.4f};
 for(int i=0;i<5;++i){const float y=bays[i];
  if(i==1||i==3)mdFix(m,model::med_ward_bed_tossed,20.4f,y+.3f,f,1.f,2.1f,.9f,mdE+.4f*(i-2),true);
  else if(i!=2)mdFix(m,model::med_ward_bed,20.4f,y,f,1.f,2.1f,.9f,mdE,true);
  mdFix(m,model::med_vitals_monitor,22.6f,y-.7f,f+1.2f,.4f,.3f,.35f,mdW,false);
  m.lights.push_back(lamp({19.f,y},roof-.16f,.7f,4.5f,5000,.05f));}
 // Bay 3: the gurney he fell from, its frame twisted.
 mdFix(m,model::med_bed_frame_wrecked,20.6f,11.f,f,1.f,2.f,.8f,mdE+.3f,true);
 for(auto [x,y]:{std::pair{18.4f,12.2f},{17.6f,7.8f},{13.f,16.6f}})mdFix(m,model::med_blood_bag,x,y,f,.2f,.3f,.05f,x,false);
 mdFix(m,model::med_crash_cart,13.6f,10.f,f,.6f,.5f,1.f,.6f,true);
 mdFix(m,model::med_wheelchair,11.f,18.6f,f,.65f,1.f,.95f,2.4f,true);
 mdFix(m,model::med_ward_bed_tossed,9.6f,21.4f,f,1.f,2.1f,.9f,.5f,true);
 mdFix(m,model::med_crutches,6.f,20.f,f,.4f,.2f,1.3f,.3f,false);
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
 // --- Light: the desk lit, the bays dimming, one tube going out over bay 3 ----------------------
 for(Vec2 l:{Vec2{8.f,4.f},Vec2{3.f,11.5f}})m.lights.push_back(lamp(l,roof-.16f,.85f,6.f,5200,.05f));
 {auto l=lamp({18.6f,11.3f},roof-.16f,.8f,4.f,5600,.04f);l.flicker=.6f;m.lights.push_back(l);}
 m.lights.push_back(lamp({10.f,15.f},roof-.16f,.6f,6.f,4200,.08f));
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
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});   // door 0: Screening's sealed door, this side
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"md_imaging_clock",60000);
 // --- The spine corridor ------------------------------------------------------------------------
 // West wall: open doorways into the two X-ray suites and the scanning bay, a window onto suite 2.
 wall(m,8.8f,1.f,9.f,4.6f,f,roof,28);wall(m,8.8f,5.8f,9.f,7.2f,f,roof,28);wall(m,8.8f,8.4f,9.f,9.f,f,roof,28);mdGlass(m,8.8f,9.f,9.f,10.6f,f,roof);
 wall(m,8.8f,10.6f,9.f,13.6f,f,roof,28);wall(m,8.8f,15.f,9.f,23.8f,f,roof,28);
 for(float y:{4.6f,7.2f,13.6f})wall(m,8.75f,y,9.05f,y+1.2f,f+2.65f,roof,28);
 wall(m,15.f,1.f,15.2f,9.f,f,roof,28);wall(m,15.f,14.f,15.2f,23.8f,f,roof,28);
 ceilingGrid(m,9.f,1.f,15.f,23.f,roof);
 for(float y:{4.f,11.5f,19.f})m.lights.push_back(lamp({12.f,y},roof-.16f,.7f,5.5f,5400,.05f));
 {auto l=lamp({12.f,15.f},roof-.16f,.7f,5.f,5400,.05f);l.flicker=.5f;m.lights.push_back(l);}
 // --- X-ray suites, north-west: the tables behind a lead-glass control booth -----------------------
 wall(m,1.f,6.f,8.8f,6.2f,f,roof,27);
 for(float y0:{1.f,6.2f}){const float cy=y0+2.4f;
  mdFix(m,model::med_autopsy_table,5.2f,cy,f,.9f,2.f,.9f,mdE,true);
  wall(m,4.6f,cy-.3f,5.8f,cy+.3f,f+2.3f,f+2.5f,20);m.pipes.push_back({{5.2f,cy},{5.2f,cy},f+2.5f,.05f,roof,6});
  mdFix(m,model::med_xray_viewer_skull,1.04f,cy-.6f,f+1.4f,.6f,.05f,.5f,mdE,false);
  mdFix(m,model::med_xray_viewer_chest,1.04f,cy+.6f,f+1.4f,.6f,.05f,.5f,mdE,false);
  m.lights.push_back(lamp({1.3f,cy},f+1.6f,.6f,3.f,7500,-.3f,WorldLightMount::Bare));}
 mdGlass(m,7.f,1.f,7.2f,4.2f,f,roof,27);   // the booth's lead glass, open at its south end
 mdFix(m,model::lab_control_board,8.4f,3.4f,f+.9f,1.32f,.19f,.6f,mdW,false);
 terminal(m,{8.3f,2.2f},"RADIOLOGY / READINGS","03:00 FRONTAL BONE: CRYSTALLINE GROWTH THROUGH THE SUTURES. SAME AS LAST WEEK'S. SAME AS EVERY TIER 3.",
  "DO NOT REPORT AS ABNORMAL. REPORT AS EXPOSURE, TIER 3. THE BOARD HAS ASKED FOR THE FILMS.");m.terminals.back().readable=Readable::Note;m.terminals.back().yaw=mdW;
 // --- The scanning bay, south-west ----------------------------------------------------------------
 wall(m,1.f,11.8f,8.8f,12.f,f,roof,27);
 mdFix(m,model::lab_test_chamber,4.4f,16.6f,f,2.6f,2.6f,2.5f,mdE,true);
 mdFix(m,model::lab_control_board,7.9f,13.f,f+.9f,1.32f,.19f,.6f,mdW,false);
 mdFix(m,model::lab_scope_monitor,8.2f,14.4f,f+.9f,.5f,.4f,.45f,mdW,false);
 mdFix(m,model::med_wheelchair,7.2f,20.6f,f,.65f,1.f,.95f,2.8f,true);
 mdBlood(m,5.8f,18.6f,7.4f,20.f,f);
 {auto l=lamp({4.4f,16.6f},roof-.2f,.8f,5.f,9000,-.4f);l.flicker=.4f;m.lights.push_back(l);}
 m.creatureSpawns.push_back({CreatureKind::Transformed,{3.f,20.6f},f});
 // --- The reading room, north-east: lightboxes along the wall --------------------------------------
 wall(m,15.2f,8.8f,17.f,9.f,f,roof,28);wall(m,21.f,8.8f,23.f,9.f,f,roof,28);
 for(float x:{16.4f,17.6f,18.8f,20.f,21.2f})mdFix(m,model::med_xray_viewer_skull,x,1.04f,f+1.4f,.6f,.05f,.5f,mdS,false);
 for(float x:{17.f,20.6f})mdDeskLocal(m,x,3.2f,f);
 mdBody(m,"md_imaging_clock",ActorVisual::SeniorScientist,{18.8f,5.6f},f,1.9f);
 m.lights.push_back(lamp({18.8f,1.4f},f+1.5f,.8f,4.5f,7500,-.3f,WorldLightMount::Bare));
 m.lights.push_back(lamp({19.f,5.6f},roof-.16f,.6f,5.f,4800,.06f));
 // --- The film archive, south-east ----------------------------------------------------------------
 wall(m,15.2f,14.f,21.f,14.2f,f,roof,28);wall(m,22.6f,14.f,23.f,14.2f,f,roof,28);mdFrame(m,21.f,22.6f,14.1f,f,roof);
 for(float y:{15.4f,17.f,18.6f,20.2f})for(float x:{17.f,19.4f})mdFix(m,model::med_records_cabinet,x,y,f,.9f,.5f,1.3f,mdS,true);
 mdFix(m,model::file_cabinet_tall,22.5f,21.6f,f,.5f,.6f,1.3f,mdW,true);
 m.lights.push_back(lamp({20.f,18.f},roof-.16f,.55f,5.f,4600,.08f));
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

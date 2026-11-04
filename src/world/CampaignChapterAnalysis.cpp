// ANALYSIS LABORATORIES (chapter 2, manifest 49-54): chunks 79, 80, 81, 82, 83, 84. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include "ModelIds.h"
namespace retro {
namespace {
using namespace kit;
// --- Analysis kit (an) -----------------------------------------------------------------------------
constexpr float anS=0,anN=kPi,anE=kPi*.5f,anW=-kPi*.5f;
void anFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
// A pack model at its own proportions (times s).
void anModel(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];anFix(m,model,x,y,z,v[0]*s,v[1]*s,v[2]*s,yaw,solid);
}
// The hum of a live pool: AnomalyHum every 10 s from the box, for an hour once the player
// first stands in it (the water chapter's live pools, the same way).
void anHum(Map& m,const char* timer,float x1,float y1,float x2,float y2,float bottom,float top,float gain=.6f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_done");
 s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;s.bottom=bottom;s.top=top;s.durationMs=3600000;s.finishAtMs=3600000;
 s.soundIntervalMs=10000;s.soundUntilMs=3000000;s.tickSound=Sound::AnomalyHum;s.gain=gain;s.pitch=1;m.timedSequences.push_back(s);
}
// MATERIALS INTAKE (manifest 49). Where what came up from the deep extraction was
// signed for. The dock hall under a gantry hoist; crates on pallets in the order
// they were booked in; the manifest booth; the quarantine cage where the crates
// that would not settle were put. One of those was opened from the inside.
// Route: the N door -> the dock hall -> (W to Sample Storage) -> the S seam on.
std::shared_ptr<const Map> makeMaterialsIntake(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[79];m.definition=CampaignChunks[79];m.definition.ambient=.24f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});                                                      // door 0: in from the north
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"anl_intake_clock",60000);
 // --- The receiving lobby: the decon arch, the booking hatch --------------------------------------
 wall(m,1.f,4.8f,9.4f,5.f,f,f+3.2f,3);wall(m,14.6f,4.8f,23.f,5.f,f,f+3.2f,3);            // lobby front wall (to the hall)
 wall(m,9.4f,4.8f,14.6f,5.f,f+2.9f,f+3.2f,3);                                              // header over the hall mouth
 for(float x:{9.4f,14.4f})wall(m,x,4.6f,x+.2f,5.2f,f,f+2.9f,20);                          // the decon arch
 wall(m,9.4f,4.6f,14.6f,5.2f,f+2.75f,f+2.9f,20);
 for(float x:{9.6f,10.6f,11.6f,12.6f,13.6f})wall(m,x,4.66f,x+.06f,4.72f,f+2.6f,f+2.75f,8); // spray heads
 floorLine(m,9.6f,4.7f,14.4f,5.1f,f,13);                                                   // the line you wait behind
 wall(m,1.f,1.f,9.f,4.8f,f+3.2f,roof,3);wall(m,15.f,1.f,23.f,4.8f,f+3.2f,roof,3);
 wall(m,9.f,1.f,9.2f,4.8f,f,f+3.2f,3);wall(m,14.8f,1.f,15.f,4.8f,f,f+3.2f,3);             // lobby side walls
 ceilingGrid(m,9.2f,1.f,14.8f,4.8f,f+3.2f);
 m.lights.push_back(lamp({12.f,2.6f},f+3.1f,.9f,7.f,4600));
 wallSign(m,5.f,5.03f,f+2.55f,"MATERIALS INTAKE","DEEP EXTRACTION RECEIVING",2.6f,anS);
 taped(m,{14.79f,3.2f},f+1.45f,anW,Readable::Note,"RECEIVING ORDER","EVERY CONSIGNMENT FROM DEEP EXTRACTION IS SIGNED FOR IN THE BOOTH BEFORE IT IS LIFTED.",
  "IF A CRATE IS WARM, IT GOES IN THE CAGE. IF IT KNOCKS, IT GOES IN THE CAGE AND YOU CALL ANALYSIS.");
 // --- The dock hall: high bays, the gantry hoist over it -----------------------------------------
 for(float x:{4.f,12.f,20.f})for(float y:{8.f,15.f})m.lights.push_back(lamp({x,y},roof-.05f,1.1f,12.f,4000));
 for(float x:{1.f,22.6f})for(float y:{6.f,11.f,16.f})steelColumn(m,x+.2f,y,f,roof,false);
 // the hoist: twin runway girders along the hall, a bridge across them, the trolley and its hook
 for(float y:{9.9f,12.1f})wall(m,1.4f,y,22.6f,y+.22f,roof-1.1f,roof-.8f,12);
 wall(m,7.8f,9.9f,8.3f,12.32f,roof-1.35f,roof-1.1f,12);                                    // the bridge
 wall(m,7.85f,10.8f,8.25f,11.4f,roof-1.6f,roof-1.35f,18);                                  // the trolley
 wall(m,8.03f,11.07f,8.07f,11.13f,f+1.8f,roof-1.6f,8);                                     // the chain
 wall(m,7.9f,10.95f,8.2f,11.25f,f+1.6f,f+1.8f,18);                                         // the hook block
 anModel(m,model::lab_specimen_crate,8.05f,11.1f,f,.18f,true,1.1f);                        // crate 4, set down and left on the hook
 for(float x:{7.62f,8.48f})wall(m,x,11.08f,x+.03f,11.12f,f+1.24f,f+1.6f,8);               // the sling legs
 floorLine(m,7.2f,10.3f,8.9f,10.38f,f,13);floorLine(m,7.2f,11.8f,8.9f,11.88f,f,13);         // the lift zone, painted
 // the dock: a roller shutter in the east wall, down, the leveller in front of it
 wall(m,22.75f,6.f,23.f,10.f,f,f+4.2f,7);for(float z=f+.3f;z<f+4.1f;z+=.3f)wall(m,22.7f,6.1f,22.75f,9.9f,z,z+.05f,2);
 wall(m,21.1f,6.f,22.75f,10.f,f,f+.04f,8);floorLine(m,21.f,5.9f,22.7f,6.f,f,13);floorLine(m,21.f,10.f,22.7f,10.1f,f,13);
 // consignments as booked in: specimen crates on pallets, the cargo boxes from the bore
 for(auto [x,y,yaw]:{std::tuple{14.2f,7.2f,0.f},{16.4f,7.25f,.04f},{18.6f,7.2f,0.f},{14.25f,9.25f,-.03f},{16.4f,9.2f,0.f}}){
  anFix(m,model::pallet,x,y,f,1.08f,1.2f,.16f,yaw,true);anModel(m,model::lab_specimen_crate,x,y,f+.16f,yaw+.02f,true);}
 anModel(m,model::ind_cargo_full,17.9f,14.3f,f,anS,true);                                 // the bore's cargo box, not yet lifted
 forklift(m,12.2f,13.4f,f);
 kit::prop(m,model::prop_cardboard_box,17.9f,10.4f,f,.3f);
 // --- The manifest booth (northwest): glazed onto the hall, the signing desk ---------------------
 wall(m,1.f,9.8f,6.f,10.f,f,f+.95f,3);glazing(m,1.f,9.8f,6.f,10.f,f,f+2.6f,3,.95f,2.4f);
 wall(m,.9f,5.f,1.f,10.f,f,f+2.6f,3);wall(m,6.f,5.f,6.2f,8.2f,f,f+2.6f,3);officeDoorway(m,6.f,6.2f,8.2f,9.4f,f,f+2.6f);wall(m,6.f,9.4f,6.2f,10.f,f,f+2.6f,3);
 wall(m,1.f,5.f,6.2f,10.f,f+2.6f,f+2.75f,3);ceilingGrid(m,1.f,5.f,6.f,9.8f,f+2.6f);
 m.lights.push_back(lamp({3.5f,7.4f},f+2.55f,.7f,5.f,3800));
 workstation(m,3.2f,8.6f,f,1,1);
 terminal(m,{1.25f,6.4f},"INTAKE / MANIFEST","LAST CONSIGNMENT 02:10: SIX CRATES FROM DEEP EXTRACTION, LEVEL 9 FACE. ROUTED: SAMPLE STORAGE.",
  "CRATE 4 REFUSED AT THE HOIST: WARM TO THE TOUCH. CRATE 6 BOOKED TO THE CAGE. CAGE DOOR ALARM 02:31, ACKNOWLEDGED.");
 taped(m,{1.01f,8.3f},f+1.5f,anE,Readable::Note,"MANIFEST 0211-49","DEEP EXTRACTION / HANDLE AS LIVE. DO NOT OPEN IN RECEIVING. DO NOT STACK.",
  "SIGNED FOR BY THE NIGHT PORTER. HE SAID IT WAS KNOCKING. I SAID SIGN IT ANYWAY.");
 m.pickupSpawns.push_back({{3.75f,8.55f},PickupKind::HandgunAmmo,f+.76f});                  // 9 mm in the porter's drawer
 // --- The quarantine cage (southeast): crate 6, open from the inside -----------------------------
 for(float x:{17.f,22.8f})wall(m,x,17.f,x+.1f,22.8f,f,f+2.6f,30);
 wall(m,17.f,17.f,19.f,17.1f,f,f+2.6f,30);wall(m,21.f,17.f,22.9f,17.1f,f,f+2.6f,30);  // the gate bay
 wall(m,21.f,17.1f,21.08f,18.9f,f+.05f,f+2.5f,30);                                         // the gate, swung in and leftwall(m,17.f,17.f,22.9f,22.8f,f+2.55f,f+2.6f,30);
  anFix(m,model::pallet,20.f,20.f,f,1.08f,1.2f,.16f,.1f,true);
 anModel(m,model::lab_specimen_crate,19.8f,20.1f,f+.16f,.5f,true);                         // crate 6
 anModel(m,model::prop_cardboard_box_open,21.f,19.2f,f,1.9f,false);                       // its packing, torn out
 anModel(m,model::lab_anomaly_relic,21.4f,21.3f,f,1.2f,false,.35f);
 for(float y:{19.f,19.6f,20.2f})wall(m,17.1f,y,17.14f,y+.1f,f+.9f,f+1.4f,8);              // the mesh bulged out
 wall(m,17.15f,18.6f,18.6f,18.9f,f,f+.01f,16);                                              // what it left on the floor
 m.creatureSpawns.push_back({CreatureKind::Mutant,{21.5f,15.8f},-999,1,0});                  // out, in the hall's shadow
 // --- The west wall: racking for lifted consignments, and the seam to Sample Storage ------------
 for(float y:{17.1f,18.7f,20.3f})anModel(m,model::lab_specimen_cabinet,1.6f,y,f,anE,true);
 anModel(m,model::lab_sample_fridge,1.65f,21.8f,f,anE,true);
 wall(m,1.f,14.f,1.2f,22.8f,f,f+.05f,8);
 m.pickupSpawns.push_back({{3.4f,19.2f},PickupKind::Ammo,f});
 // --- The south passage on -----------------------------------------------------------------------
 wall(m,9.8f,17.f,10.f,23.f,f,f+3.f,3);wall(m,14.f,17.f,14.2f,23.f,f,f+3.f,3);
 wall(m,10.f,17.f,14.f,23.f,f+3.f,f+3.2f,3);ceilingGrid(m,10.f,17.f,14.f,23.f,f+3.f);
 m.lights.push_back(lamp({12.f,20.f},f+2.95f,.8f,7.f,4300));
 extinguisher(m,10.f,18.4f,f,anE);
 m.pickupSpawns.push_back({{13.6f,21.6f},PickupKind::Health,f});
 recessedFittings(m);
 zone(m,"MATERIALS INTAKE",0,0,24,24);
 return p;
}
// SAMPLE STORAGE (manifest 50)
// SAMPLE STORAGE (manifest 50). The cold store a floor below intake: everything
// Deep Extraction sent up, and everything Medical drew out of its patients, kept
// at -80 in numbered freezers. The cold plant failed at 02:10. The meltwater lies
// on the steel floor, still and wet, which is what resonance likes: it hums, and
// standing in it stings. Freezer 6 is still running and still warming. The
// Investigator's people came at 03:05 for Procedure 14's material and LAB 4's
// draws; their cabinet stands open and empty. Something that was kept here
// watches from the far aisle.
// Route: the E seam (z -12) -> the control gallery -> the stair -> the store -> back.
std::shared_ptr<const Map> makeSampleStorage(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[80];m.definition=CampaignChunks[80];m.definition.ambient=.2f;
 m.definition.playerStart={22.f,11.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,up=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'E',9.f,14.f,up,false,0});
 machineryClock(m,"anl_cold_clock",60000);
 // --- The control gallery at intake level: the cold store's desk, the stair down ---------------------
 deck(m,up,17,7,24,16);wall(m,17.f,7.f,24.f,16.f,f,up-.22f,15);                              // the gallery on its plinth
 guardRail(m,17.f,7.f,17.08f,9.f,up);guardRail(m,17.f,11.5f,17.08f,16.f,up);
 guardRail(m,17.f,7.f,23.f,7.08f,up);guardRail(m,17.f,15.92f,23.f,16.f,up);
 m.stairs.push_back({12.5f,9.f,17.f,11.5f,f,up,16,false,true});                            // down, westward
 m.lights.push_back(lamp({20.f,11.5f},roof-.05f,.75f,8.f,5200)); m.lights.push_back(lamp({11.2f,10.2f},roof-.05f,.8f,8.f,5200));                         // over the stair foot
 terminal(m,{22.75f,8.f},"COLD STORE / CONTROL","PLANT FAULT 02:10. LN2 FEED LOST. FREEZERS 1-12 ON BATTERY. BATTERY 0% 02:58.",
  "FREEZER 6 STILL RUNNING. FREEZER 6 INTERNAL +4 C AND RISING. COMPRESSOR DRAW NORMAL. NO FAULT FOUND.",up-f);
 wall(m,22.9f,14.2f,23.f,16.f,up,up+3.f,3);
 taped(m,{22.89f,15.1f},up+1.5f,anW,Readable::Note,"FREEZER 6 / ALARM LOG","WARM ALARM 02:12. RESET. WARM ALARM 02:14. RESET. WARM ALARM 02:15. UNPLUGGED IT.",
  "IT IS STILL RUNNING. THE LIGHT IS ON. THE CORD IS IN MY HAND. THE SAMPLES ARE WARMING THE FREEZER, NOT THE OTHER WAY ROUND.");
 m.pickupSpawns.push_back({{21.6f,15.f},PickupKind::Health,up});
 // --- The store: the specimen wall, the freezer aisles, cold light ----------------------------------
 wallSign(m,8.f,1.07f,f+3.4f,"SAMPLE STORAGE","COLD STORE / -80",2.6f,anS);
 for(float x:{2.2f,3.6f,5.f,6.4f,7.8f,9.2f,10.6f})anModel(m,model::lab_specimen_cabinet,x,1.42f,f,anS,true);
 for(float y:{4.f,5.4f})anModel(m,model::lab_specimen_cabinet,1.42f,y,f,anE,true);
 for(float x:{4.f,4.75f,5.5f,6.25f,7.f,7.75f,8.5f,9.25f}){                                    // freezer row A, back to back
  anModel(m,model::lab_sample_fridge,x,7.6f,f,anN,true);anModel(m,model::lab_sample_fridge,x,8.45f,f,anS,true);}
 for(float x:{4.f,4.75f,5.5f,6.25f,7.f,7.75f,8.5f,9.25f}){                                    // freezer row B
  anModel(m,model::lab_sample_fridge,x,13.1f,f,anN,true);anModel(m,model::lab_sample_fridge,x,13.95f,f,anS,true);}
 for(float x:{5.f,9.f,13.f})for(float y:{4.5f,11.f,17.5f})m.lights.push_back(lamp({x,y},roof-.05f,.95f,10.f,6500));
 m.lights.push_back(lamp({20.f,19.5f},roof-.05f,.5f,8.f,6500));
 // the Investigator's cabinet: doors open, shelves bare, the cold box's footprint in the frost
 wall(m,1.f,.9f,17.f,1.f,f,roof,3);wall(m,.9f,1.f,1.f,23.f,f,roof,3);
 anModel(m,model::lab_specimen_cabinet,13.8f,1.42f,f,anS,true);
 wall(m,13.15f,1.84f,14.45f,2.9f,f,f+.006f,20);wall(m,13.3f,2.f,14.1f,2.6f,f+.006f,f+.008f,3);
 taped(m,{12.7f,1.01f},f+1.4f,anS,Readable::Note,"RELEASED TO I.I. / 03:05","ALL PROCEDURE 14 MATERIAL. ALL OBS 6 DRAWS. ALL PAPERWORK FOR BOTH.",
  "THEY BROUGHT THEIR OWN COLD BOX AND THEIR OWN SEALS. THEY DID NOT SIGN. SHE DID NOT LOOK AT THE SHELVES, ONLY AT ME.");
 // LAB 4's draw: one tube missed, its rack card still on freezer 9
 taped(m,{9.25f,8.885f},f+.75f,anS,Readable::Note,"FREEZER 9 / RACK CARD","OBS 6 (LAB 4). 2 X 10 ML, EVERY SIX HOURS. READS HIGHER EVERY DRAW.",
  "DO NOT LABEL WITH NAMES. ONE TUBE LEFT IN THE DOOR RACK AFTER I.I. CLEARED IT. IT IS WARM.");
 m.pickupSpawns.push_back({{2.5f,10.4f},PickupKind::Ammo,f});
 // --- The cold plant (southwest): the LN2 manifold, failed, and its meltwater -----------------------
 anModel(m,model::lab_analyzer_cabinet,2.25f,19.5f,f,anE,true);                                // the cryo plant
 for(float y:{17.6f,21.4f})anModel(m,model::lab_pressure_gauge,1.06f,y,f+1.5f,anE,false);
 for(float x:{1.2f,1.35f})m.pipes.push_back({{x,16.4f},{x,22.8f},f+2.6f,.05f,f+2.6f,1});
 for(float z=f+1.f;z<f+2.5f;z+=.14f)wall(m,3.36f,18.7f,3.39f,20.3f,z,z+.05f,20);               // frost on its face
 for(auto [x1,y1,x2,y2]:{std::tuple{3.6f,15.6f,10.4f,22.2f},{3.4f,17.f,3.6f,21.f},{10.4f,16.4f,10.9f,20.6f}})floorLine(m,x1,y1,x2,y2,f,22);   // the meltwater, still
 anHum(m,"anl_cold_hum",3.2f,15.4f,10.6f,22.4f,f,f+2.f);
 for(auto [x1,y1,x2,y2]:{std::tuple{3.2f,15.4f,7.f,19.f},{7.f,15.4f,10.6f,19.f},{3.2f,19.f,7.f,22.4f},{7.f,19.f,10.6f,22.4f}}){
  Hazard h;h.kind=Hazard::Kind::Anomaly;h.x1=x1;h.y1=y1;h.x2=x2;h.y2=y2;h.bottom=f;h.top=f+2.2f;h.damagePerSecond=3.f;m.hazards.push_back(h);}
 taped(m,{1.01f,16.6f},f+1.45f,anE,Readable::Note,"COLD PLANT / DO NOT MOP","THE WATER HUMS. IT HUMMED BEFORE THE LIGHTS WENT. IT STINGS THROUGH BOOTS.",
  "WALK THE AISLES ROUND IT. IF YOU HAVE TO CROSS, DON'T STOP IN IT. NOTHING STILL GOES IN THE WATER.");
 // round it: the dry aisle along the east, past the warm freezer
 anModel(m,model::lab_sample_fridge,13.4f,18.f,f,anW,true);                                 // freezer 6, still running
 m.lights.push_back(lamp({13.4f,18.f},f+1.05f,.25f,2.f,2600,0,WorldLightMount::Bare));
 m.pickupSpawns.push_back({{14.6f,21.8f},PickupKind::Ammo,f});
 // what was kept here: it waits at the end of the far aisle, and it watches
 m.creatureSpawns.push_back({CreatureKind::Warden,{15.5f,20.5f},-999,1,0});
 recessedFittings(m);
 zone(m,"SAMPLE STORAGE",0,0,24,24);
 return p;
}
// ANALYTICAL CHEMISTRY (manifest 51)
std::shared_ptr<const Map> makeAnalyticalChemistry(){return placeholderChunk(81,{{'N',10.f,14.f,-12.f,false,0},{'W',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,true,stateId("lab_chem_seal")}});}
// INSTRUMENTATION (manifest 52)
std::shared_ptr<const Map> makeInstrumentation(){return placeholderChunk(82,{{'E',9.f,14.f,-12.f,false,0},{'S',6.f,10.f,-12.f,false,0}});}
// ENERGY DIAGNOSTICS / LAB 4 (manifest 53)
std::shared_ptr<const Map> makeEnergyDiagnosticsLab4(){return placeholderChunk(83,{{'N',10.f,14.f,-12.f,true,0},{'E',15.f,19.f,-12.f,false,0},{'W',9.f,14.f,-12.f,false,0},{'S',10.f,14.f,-12.f,false,0}});}
// FIELD MEASUREMENT (manifest 54)
std::shared_ptr<const Map> makeFieldMeasurement(){return placeholderChunk(84,{{'N',6.f,10.f,-12.f,false,0},{'E',9.f,14.f,-12.f,false,0}});}
}
void buildChapterAnalysis(CampaignMapTable& maps){
 maps[79]=makeMaterialsIntake();
 maps[80]=makeSampleStorage();
 maps[81]=makeAnalyticalChemistry();
 maps[82]=makeInstrumentation();
 maps[83]=makeEnergyDiagnosticsLab4();
 maps[84]=makeFieldMeasurement();
}
}

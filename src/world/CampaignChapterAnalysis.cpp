// ANALYSIS LABORATORIES (chapter 2, manifest 49-54): chunks 79, 80, 81, 82, 83, 84. Placeholder builders generated
// by tools/campaign/plan.py: replace each with the authored chunk. See
// docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapterAnalysisKit.h"
namespace retro {
std::shared_ptr<const kit::Map> makeAnalyticalChemistry(); // CampaignChapterAnalysisLabs.cpp
std::shared_ptr<const kit::Map> makeInstrumentation();
namespace {
using namespace kit;
using namespace an;
// --- Intake and the stores (79, 80): their own kit, in this file -------------------------------------
// A high-bay pendant: a drop rod from the roof to a reflector head, lit just under it; 0 is a dead one.
void inPendant(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float tint,float flicker=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});
 anFix(m,model::pendant_lamp,p.x,p.y,z,.5f,.5f,.34f,0,false);
 if(intensity<=0)return;
 auto l=lamp(p,z-.05f,intensity,range,kelvin,tint,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);
}
// A tube under the ceiling at `ceil` that works: the emitter clear below its recessed fitting.
WorldLight inTube(float x,float y,float ceil,float intensity,float range,float kelvin,float tint,float flicker=0,float yaw=0){
 auto l=lamp({x,y},ceil-.30f,intensity,range,kelvin,tint);l.flicker=flicker;l.yaw=yaw;return l;
}
// A fitting whose tubes are dead: the housing alone, flush under the ceiling.
void inDeadTube(Map& m,float x,float y,float ceil,float yaw=0){anFix(m,model::clinic_ceiling_lamp_2,x,y,ceil-.17f,.44f,1.04f,.17f,yaw,false);}
// Glazing in a straight wall: dado, steel sill and mullions, wired glass to `head`, wall over; an
// invisible proxy from sill to head across the wall so nobody perches on the sill.
void inGlaze(Map& m,float x1,float y1,float x2,float y2,float f,float top,float sill,float head,int mat=3){
 const bool alongX=x2-x1>y2-y1;const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 wall(m,x1,y1,x2,y2,f,f+sill,mat);
 if(alongX)wall(m,x1,y1-.03f,x2,y2+.03f,f+sill-.03f,f+sill+.01f,20);else wall(m,x1-.03f,y1,x2+.03f,y2,f+sill-.03f,f+sill+.01f,20);
 if(alongX)glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill+.01f,f+head,17);else glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill+.01f,f+head,17);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.025f,alongX?x1:y1,(alongX?x2:y2)-.05f);
  if(alongX)wall(m,t,y1+.02f,t+.05f,y2-.02f,f+sill+.01f,f+head,20);else wall(m,x1+.02f,t,x2-.02f,t+.05f,f+sill+.01f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.06f,20);if(top>f+head+.06f)wall(m,x1,y1,x2,y2,f+head+.06f,top,mat);
 if(alongX)wall(m,x1,y1-.04f,x2,y2+.04f,f+sill+.01f,f+head,30);else wall(m,x1-.04f,y1,x2+.04f,y2,f+sill+.01f,f+head,30);
}
// An open doorway x1..x2 through a wall along x (y1..y2): pale-steel jambs and head, the wall over it.
void inDoorwayX(Map& m,float x1,float x2,float y1,float y2,float f,float top,float head=2.2f,int mat=3){
 wall(m,x1,y1,x2,y2,f+head,top,mat);
 for(float x:{x1-.06f,x2})wall(m,x,y1-.03f,x+.06f,y2+.03f,f,f+head,20);
 wall(m,x1-.06f,y1-.03f,x2+.06f,y2+.03f,f+head-.04f,f+head,20);
}
// A fitted counter along x (fronts at y1 and y2): kick plinth, steel carcass, drawer lines, worktop.
void inCounter(Map& m,float x1,float y1,float x2,float y2,float f,float h,int worktop=28){
 const float top=f+h;
 wall(m,x1+.05f,y1+.05f,x2-.05f,y2-.05f,f,f+.08f,8);wall(m,x1,y1,x2,y2,f+.08f,top-.04f,20);wall(m,x1,y1,x2,y2,top-.04f,top,worktop);
 for(float y:{y1-.006f,y2-.004f})wall(m,x1+.02f,y,x2-.02f,y+.01f,top-.3f,top-.29f,8);
 for(float x=x1+.6f;x<x2-.3f;x+=.6f)for(float y:{y1-.006f,y2-.004f})wall(m,x,y,x+.01f,y+.01f,f+.1f,top-.06f,8);
}
// An upright freezer: two of the pack's sample fridges stacked in one column, facing `yaw`.
void ssFreezer(Map& m,float x,float y,float f,float yaw){
 const auto& s=model::size[model::lab_sample_fridge];
 for(float z:{0.f,s[2]})anFix(m,model::lab_sample_fridge,x,y,f+z,s[0],s[1],s[2],yaw,true);
}
// A metal shelf of sample jars along x (yaw 0) or y (yaw kPi/2), a slot in four emptied.
void ssJars(Map& m,float x,float y,float yaw,float f,int seed){
 shelf(m,{x,y},yaw,0);const float c=std::cos(yaw),s=std::sin(yaw);
 for(int t=0;t<3;++t){const float z=f+(t==0?.306f:t==1?.972f:1.656f);
  for(int i=0;i<5;++i){if((i*7+t*3+seed)%4==0)continue;const float u=-.7f+i*.35f,v=i%2?.08f:-.06f;
   kit::prop(m,model::med_specimen_jar,x+u*c+v*s,y-u*s+v*c,z,.146f,.146f,.245f,i*.9f+float(seed));}}
}
// An LN2 dewar: a stainless drum with its shoulder, neck and dark foot ring, standing on the floor;
// the old gas-cylinder model reads as a black silhouette under the store's cold tubes.
void ssDewar(Map& m,float x,float y,float f,float r=.22f,float h=1.12f){
 m.pipes.push_back({{x,y},{x,y},f+.06f,r,f+h,5});
 m.pipes.push_back({{x,y},{x,y},f,r+.01f,f+.06f,6});
 m.pipes.push_back({{x,y},{x,y},f+h,r*.55f,f+h+.08f,5});m.pipes.push_back({{x,y},{x,y},f+h+.08f,.045f,f+h+.24f,6});
 const float k=r*.7f;wall(m,x-k,y-k,x+k,y+k,f,f+h+.24f,30);                                    // inside the skin, so nobody walks through it
}
// A stainless kick plate at the foot of an insulated-panel wall face (x1..x2 by y1..y2, a thin strip):
// the store's walls meet its floor on a steel line, the way the Medical rooms meet theirs on a dado.
void ssKick(Map& m,float x1,float y1,float x2,float y2,float f){wall(m,x1,y1,x2,y2,f,f+.26f,20);}
// MATERIALS INTAKE (manifest 49). Where what came up from the deep extraction was
// signed for. The dock hall under its mercury and sodium high-bays and the gantry
// hoist; the booking bay with tonight's crates as they were booked in; the manifest
// booth; the porters' office; the racking; the quarantine cage where the crates that
// would not settle were put. One of those was opened from the inside, and the porter
// who went to look at 02:31 left his hat at the gate.
// Route: the N door -> the lobby -> the dock hall -> (W to Sample Storage) -> the S passage on.
std::shared_ptr<const Map> makeMaterialsIntake(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[79];m.definition=CampaignChunks[79];m.definition.ambient=.2f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi*.5f;
 // Block painted to a green dado, a concrete floor, the roof in sheet; dock dust thick enough for shafts.
 m.wallSurface=SurfacePaintedGreen;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceCorrugated;
 m.definition.haze={.22f,.2f,.15f,.011f};
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 rectangle(m.layers[0],1,1,9,5,'#');                                                         // behind the booth: the riser, no way in
 openEdge(m,{'N',10.f,14.f,f,true,0});                                                      // door 0: in from the north
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"anl_intake_clock",60000);
 auto crash=[&](float x1,float x2,float y1,float y2){wall(m,x1,y1,x2,y2,f+.86f,f+.96f,20);};
 // --- The receiving lobby: Research's lab panel, the decon arch, a ceiling at 3.2 --------------------
 wall(m,9.f,1.f,9.2f,4.8f,f,f+3.2f,28);wall(m,14.8f,1.f,15.f,4.8f,f,f+3.2f,28);
 wall(m,9.f,4.8f,9.4f,5.f,f,f+3.2f,3);wall(m,14.6f,4.8f,15.f,5.f,f,f+3.2f,3);
 wall(m,9.4f,4.8f,14.6f,5.f,f+2.9f,f+3.2f,3);                                              // header over the hall mouth
 for(float x:{9.4f,14.4f})wall(m,x,4.6f,x+.2f,5.2f,f,f+2.9f,20);                          // the decon arch
 wall(m,9.4f,4.6f,14.6f,5.2f,f+2.75f,f+2.9f,20);
 for(float x:{9.6f,10.6f,11.6f,12.6f,13.6f})wall(m,x,4.66f,x+.06f,4.72f,f+2.6f,f+2.75f,8); // spray heads
 floorLine(m,9.6f,4.7f,14.4f,5.1f,f,13);                                                   // the line you wait behind
 wall(m,9.2f,1.f,14.8f,4.8f,f,f+.005f,27);
 wall(m,9.2f,1.f,14.8f,4.8f,f+3.2f,f+3.35f,22);ceilingGrid(m,9.2f,1.f,14.8f,4.8f,f+3.2f);
 wall(m,9.f,4.8f,23.f,5.f,f+3.2f,roof,3);                                                 // one face over the lobby and the office, to the roof
 crash(9.2f,9.24f,1.f,4.8f);crash(14.76f,14.8f,2.f,4.8f);
 anFix(m,model::med_notice_board,9.235f,2.6f,f+1.25f,1.2f,.07f,.9f,anE,false);
 anModel(m,model::pwr_fire_cabinet,9.3f,1.45f,f+1.f,anE,false);
 anModel(m,model::lab_id_scanner,9.2f,4.77f,f+1.1f,anN,false);                              // the arch's badge reader
 anFix(m,model::water_cooler,14.55f,1.45f,f,.35f,.35f,1.1f,anW,true);
 kit::prop(m,model::trash_can,14.5f,2.15f,f,.27f,.27f,.34f,.3f);
 wall(m,14.6f,3.75f,14.8f,4.55f,f+1.1f,f+1.13f,20);                                        // the dosimeter shelf, two left
 for(float y:{3.95f,4.33f})kit::prop(m,model::item_dosimeter,14.7f,y,f+1.13f,.31f,.146f,.066f,kPi*.5f+(y-4.f)*.2f);
 taped(m,{14.79f,3.f},f+1.5f,anW,Readable::Note,"RECEIVING ORDER","EVERY CONSIGNMENT FROM DEEP EXTRACTION IS SIGNED FOR IN THE BOOTH BEFORE IT IS LIFTED.",
  "IF A CRATE IS WARM, IT GOES IN THE CAGE. IF IT KNOCKS, IT GOES IN THE CAGE AND YOU CALL ANALYSIS.");
 wallSign(m,12.f,5.03f,f+3.62f,"HANDLE AS LIVE","DEEP EXTRACTION / NOTHING IS LIFTED UNSIGNED",2.6f,anS);
 m.lights.push_back(inTube(10.7f,2.9f,f+3.2f,.45f,5.f,5600,.05f));m.lights.push_back(inTube(13.3f,2.9f,f+3.2f,.45f,5.f,5600,.05f));
 // --- The porters' office (northeast): onto the hall by its doorway and its window -------------------
 wall(m,15.f,4.8f,15.6f,5.f,f,f+3.2f,3);inDoorwayX(m,15.6f,16.6f,4.8f,5.f,f,f+3.2f);wall(m,16.6f,4.8f,17.f,5.f,f,f+3.2f,3);   // propped open: the booth's is the hall's one door
 inGlaze(m,17.f,4.8f,22.6f,5.f,f,f+3.2f,.95f,2.1f);wall(m,22.6f,4.8f,23.f,5.f,f,f+3.2f,3);
 wall(m,15.f,1.f,23.f,4.8f,f,f+.005f,27);wall(m,15.f,1.f,15.02f,4.8f,f,f+3.2f,3);                // the office's side of the lobby wall in its own paint
 wall(m,15.f,1.f,23.f,4.8f,f+3.2f,f+3.35f,22);ceilingGrid(m,15.f,1.f,23.f,4.8f,f+3.2f);
 for(int i=0;i<3;++i)anFix(m,model::psx_locker,15.6f+i*.86f,1.33f,f,.64f,.85f,2.42f,-kPi*.5f,true);
 wall(m,15.15f,1.f,17.8f,1.7f,f+2.42f,f+3.2f,30);                                          // nobody on the lockers
 workstation(m,19.4f,2.9f,f,1,2);                                                           // his desk, facing the window onto the hall
 terminal(m,{18.9f,2.75f},"NIGHT PORTER / RECEIVING","02:14 CRATE 6 KNOCKING ON THE PALLET. TOLD THE BOOTH. THE BOOTH SAID SIGN IT AND CAGE IT. SIGNED, CAGED.",
  "02:31 CAGE ALARM. IT IS PROBABLY THE LATCH. GOING DOWN TO LOOK. KETTLE IS ON.");
 {auto& t=m.terminals.back();t.readable=Readable::Note;t.surface=.75f;t.yaw=kPi;}
 anModel(m,model::lab_notice_board,15.055f,3.3f,f+1.25f,anE,false);
 anFix(m,model::med_wall_clock,19.4f,1.01f,f+2.1f,.24f,.02f,.23f,anS,false);
 anFix(m,model::wooden_table,21.8f,1.5f,f,1.367f,.724f,.78f,0,true);
 kit::prop(m,model::coffee_machine,21.5f,1.45f,f+.78f,anS);
 kit::prop(m,model::mug,22.05f,1.6f,f+.78f,0);
 kit::prop(m,model::wooden_chair,21.6f,2.25f,f,.417f,.525f,.95f,.3f);
 kit::prop(m,model::trash_can,22.6f,3.9f,f,.27f,.27f,.34f,0);
 paperDrift(m,15.4f,2.5f,22.6f,4.5f,f,5,7903);
 anFix(m,model::med_notice_board,22.965f,3.1f,f+1.25f,1.2f,.07f,.9f,anW,false);              // the porters' rota and the lift tickets
 wall(m,22.9f,1.6f,23.f,2.2f,f+1.7f,f+1.74f,20);for(float y:{1.75f,2.05f})wall(m,22.84f,y,22.9f,y+.03f,f+1.6f,f+1.74f,8);   // the coat hooks, one jacket gone
 kit::prop(m,model::mine_hard_hat,22.6f,4.25f,f,.3f);                                         // the spare hat; his is at the cage
 m.lights.push_back(inTube(19.6f,2.9f,f+3.2f,.45f,5.f,3400,0));inDeadTube(m,16.6f,2.9f,f+3.2f);
 // --- The manifest booth (northwest): glazed onto the hall, the signing desk ---------------------------
 // its door hangs in the glazed front (a leaf hangs only in a wall along x) and opens in
 inGlaze(m,1.f,9.8f,4.6f,10.f,f,f+2.6f,.95f,2.1f);wallDoor(m,4.6f,6.2f,9.8f,10.f,4.6f,5.8f,f,f+2.6f,-1);
 wall(m,.9f,5.f,1.f,10.f,f,f+2.6f,3);wall(m,6.f,5.f,6.2f,9.8f,f,f+2.6f,3);
 wall(m,1.f,5.f,6.2f,10.f,f+2.6f,f+2.64f,22);wall(m,.9f,5.f,6.24f,10.04f,f+2.64f,f+2.8f,20);ceilingGrid(m,1.f,5.f,6.f,9.8f,f+2.6f);
 wall(m,1.f,5.f,6.f,9.8f,f,f+.005f,27);
 workstation(m,3.2f,8.6f,f,1,1);wall(m,2.45f,7.5f,3.95f,9.f,f+1.35f,f+2.6f,30);            // nobody on the desk or its chair into the roof
 const int boothLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,2.65f,8.68f,f+.75f,.18f,.4f,.52f,kPi);
 paperDrift(m,2.55f,8.3f,3.9f,8.9f,f+.75f,3,7902,false);
 terminal(m,{1.25f,6.4f},"INTAKE / MANIFEST","LAST CONSIGNMENT 02:10: SIX CRATES FROM DEEP EXTRACTION, LEVEL 9 FACE. ROUTED: SAMPLE STORAGE.",
  "CRATE 4 REFUSED AT THE HOIST: WARM TO THE TOUCH. CRATE 6 BOOKED TO THE CAGE. CAGE DOOR ALARM 02:31, ACKNOWLEDGED.");
 wall(m,1.f,5.9f,1.8f,6.9f,f+1.45f,f+2.6f,30);                                                // nobody climbs the terminal into the roof
 taped(m,{1.01f,8.3f},f+1.5f,anE,Readable::Note,"MANIFEST 0211-49","DEEP EXTRACTION / HANDLE AS LIVE. DO NOT OPEN IN RECEIVING. DO NOT STACK.",
  "SIGNED FOR BY THE NIGHT PORTER. HE SAID IT WAS KNOCKING. I SAID SIGN IT ANYWAY.");
 m.pickupSpawns.push_back({{3.75f,8.55f},PickupKind::HandgunAmmo,f+.76f});                  // 9 mm in the porter's drawer
 anModel(m,model::med_records_cabinet,2.6f,5.245f,f,anS,true);
 anModel(m,model::office_file_cabinet_open,4.25f,5.35f,f,anS,true);
 anModel(m,model::lab_notice_board,4.25f,5.035f,f+1.3f,anS,false);
 wall(m,2.1f,5.f,5.05f,5.75f,f+1.05f,f+2.6f,30);                                           // nobody on the cabinets into the roof
 anModel(m,model::lab_wall_clock,1.6f,5.015f,f+1.95f,anS,false);
 anModel(m,model::lab_sample_fridge,5.55f,6.f,f,anW,true);wall(m,5.1f,5.6f,6.f,6.4f,f+1.15f,f+2.6f,30);   // the sample fridge, brought in off the hall
 for(auto [y,yaw]:{std::pair{5.85f,.05f},{6.2f,-.1f}})kit::prop(m,model::prop_cardboard_box,5.55f,y,f+1.f,.24f,.24f,.12f,yaw);
 kit::prop(m,model::trash_can,1.4f,9.35f,f,.27f,.27f,.34f,.2f);
 wall(m,5.94f,5.3f,6.f,5.34f,f+1.62f,f+1.66f,20);wall(m,5.9f,5.14f,5.98f,5.52f,f+.95f,f+1.62f,14);wall(m,5.89f,5.14f,5.9f,5.52f,f+1.2f,f+1.25f,20);   // the porter's hi-vis on its peg, its reflective band
 anModel(m,model::pwr_ammeter_box,5.6f,5.06f,f+1.45f,anS,false);m.pipes.push_back({{5.6f,5.04f},{5.6f,5.04f},f+1.82f,.015f,f+2.6f,6});
 wall(m,5.64f,7.f,6.f,8.9f,f+1.52f,f+1.55f,20);                                               // the manifest shelf: tonight's binders, one gone
 for(int i=0;i<7;++i){if(i==4)continue;kit::prop(m,model::office_file_binder,5.82f,7.15f+i*.105f,f+1.55f,.093f,.301f,.424f,kPi*.5f+(i%3-1)*.04f);}
 // its outside: the booking board and the first-aid box on the hall face, the overflow archive on its roof
 anModel(m,model::lab_notice_board,6.235f,6.6f,f+1.25f,anE,false);
 anModel(m,model::med_first_aid_box,6.26f,7.8f,f+1.35f,anE,false);
 anFix(m,model::med_wall_clock,6.21f,8.9f,f+1.95f,.24f,.02f,.23f,anE,false);
 for(auto [x,y,z,yaw]:{std::tuple{1.9f,5.9f,0.f,0.f},{2.45f,5.95f,0.f,.06f},{1.95f,6.45f,0.f,-.05f},{1.9f,5.9f,.252f,0.f},{5.1f,9.f,0.f,.3f}})
  kit::prop(m,model::prop_cardboard_box,x,y,f+2.8f+z,.5f,.495f,.252f,yaw);
 kit::prop(m,model::prop_cardboard_box_open,5.4f,6.3f,f+2.8f,.5f,.5f,.5f,.4f);
 kit::prop(m,model::item_dosimeter,6.75f,10.55f,f,.31f,.146f,.066f,2.1f);
 kit::prop(m,model::item_id_card,6.f,10.65f,f,.132f,.085f,.002f,.7f);
 m.lights.push_back(inTube(3.4f,7.2f,f+2.6f,.38f,4.5f,3800,0));
 m.lights.push_back(lamp({2.65f,8.6f},f+1.2f,.3f,2.2f,2800,0,WorldLightMount::Bare));m.lights.back().prop=boothLamp;
 // --- The dock hall: a portal frame every six metres, sheet over the blockwork, the hoist -------------
 for(float x:{3.f,9.f,15.f,21.f}){                                                        // roof beams from the north wall, on columns at the south
  wall(m,x-.13f,5.f,x+.13f,23.f,roof-.6f,roof-.54f,12);wall(m,x-.13f,5.f,x+.13f,23.f,roof-.06f,roof,12);wall(m,x-.02f,5.f,x+.02f,23.f,roof-.54f,roof-.06f,12);
  steelColumn(m,x,22.75f,f,roof-.6f,true);floorLine(m,x-.32f,22.4f,x+.32f,22.97f,f,13);}
 for(float y:{11.f,16.f}){steelColumn(m,22.8f,y,f,roof,false);floorLine(m,22.45f,y-.32f,22.97f,y+.32f,f,13);}
 steelColumn(m,1.2f,14.35f,f,roof,false);floorLine(m,1.03f,14.03f,1.55f,14.67f,f,13);
 // the blockwork to 4.4 m, corrugated sheet above it, a steel angle at the joint
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,5.f,23.f,5.03f},{1.f,22.97f,23.f,23.f},{1.f,5.f,1.03f,23.f},{22.97f,5.f,23.f,23.f}})wall(m,x1,y1,x2,y2,f+4.4f,roof,29);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,5.f,23.f,5.05f},{1.f,22.95f,23.f,23.f},{1.f,5.f,1.05f,23.f},{22.95f,5.f,23.f,23.f}})wall(m,x1,y1,x2,y2,f+4.36f,f+4.44f,12);
 wall(m,0.f,9.f,1.f,14.f,f+2.9f,roof,3);wall(m,0.f,9.f,.9f,10.f,f,f+2.9f,3);                // the W seam: Sample Storage's ceiling height, the booth's corner
 // the hoist: twin runway girders along the hall hung from the beams, a bridge, the trolley and its hook
 for(float y:{9.9f,12.1f}){wall(m,1.4f,y,22.6f,y+.22f,roof-1.1f,roof-.8f,12);
  for(float x:{3.f,9.f,15.f,21.f})m.pipes.push_back({{x,y+.11f},{x,y+.11f},roof-.8f,.025f,roof-.6f,6});}
 for(float x:{1.f,22.6f})wall(m,x,9.8f,x+.4f,12.42f,roof-1.2f,roof-1.1f,12);                 // end corbels
 wall(m,7.8f,9.9f,8.3f,12.32f,roof-1.35f,roof-1.1f,12);                                    // the bridge
 wall(m,7.85f,10.8f,8.25f,11.4f,roof-1.6f,roof-1.35f,18);                                  // the trolley
 wall(m,8.03f,11.07f,8.07f,11.13f,f+1.8f,roof-1.6f,8);                                     // the chain
 wall(m,7.9f,10.95f,8.2f,11.25f,f+1.6f,f+1.8f,18);                                         // the hook block
 // crate 4, refused at the hoist: set down and left on the hook, warm, the floor scorched round it
 anModel(m,model::lab_specimen_crate,8.05f,11.1f,f,.18f,true,1.1f);
 for(float x:{7.62f,8.48f})wall(m,x,11.08f,x+.03f,11.12f,f+1.24f,f+1.6f,8);               // the sling legs
 floorLine(m,7.2f,10.3f,8.9f,10.38f,f,13);floorLine(m,7.2f,11.8f,8.9f,11.88f,f,13);         // the lift zone, painted
 wall(m,7.25f,10.42f,8.85f,11.76f,f+.002f,f+.005f,8);
 wall(m,7.3f,10.4f,8.8f,11.8f,f+1.24f,f+3.f,30);                                           // nobody on the crate or the hook
 anHum(m,"anl_crate4_hum",7.f,10.f,9.1f,12.2f,f,f+2.f,.3f);
 // the walkway from the arch to the passage, and the drains
 floorLine(m,11.2f,5.2f,11.3f,17.f,f,14);floorLine(m,12.7f,5.2f,12.8f,17.f,f,14);
 anFix(m,model::med_floor_drain,12.f,11.f,f,.5f,.5f,.01f,0,false);anFix(m,model::med_floor_drain,20.5f,8.f,f,.5f,.5f,.01f,0,false);
 // the hall's walls at eye height: breaker panels piped to the tray, the board, the horn, the grilles
 cableTray(m,{6.4f,5.25f},{22.6f,5.25f},f+3.3f);
 for(float x:{7.2f,7.5f}){anFix(m,model::pwr_breaker_panel,x,5.051f,f+1.3f,.183f,.102f,.457f,anS,false);m.pipes.push_back({{x,5.06f},{x,5.06f},f+1.76f,.02f,f+3.3f,6});}
 anModel(m,model::lab_wall_clock,8.3f,5.015f,f+2.2f,anS,false);
 anFix(m,model::med_notice_board,22.965f,14.f,f+1.25f,1.2f,.07f,.9f,anW,false);
 extinguisher(m,23.f,15.4f,f,anW);extinguisher(m,8.6f,23.f,f,anN);
 anModel(m,model::lab_pa_horn,22.69f,13.5f,f+3.95f,anW,false);
 for(float x:{7.f,12.f,17.f})anModel(m,model::pwr_vent_grille,x,22.95f,f+3.4f,anN,false);
 servicePipe(m,{22.35f,5.15f},{22.35f,22.6f},roof-1.1f,.15f,3);
 // --- The dock: the roller shutter in the east wall, down; leveller, bollards, controls, beacon --------
 wall(m,22.75f,6.f,23.f,10.f,f,f+4.2f,18);for(float z=f+.3f;z<f+4.1f;z+=.3f)wall(m,22.7f,6.1f,22.75f,9.9f,z,z+.04f,12);wall(m,22.68f,6.1f,22.75f,9.9f,f,f+.12f,8);
 wall(m,22.55f,5.85f,22.75f,10.15f,f+4.2f,f+4.45f,8);                                       // the coil box
 for(float y:{5.85f,10.f})wall(m,22.6f,y,22.75f,y+.15f,f,f+4.2f,13);                         // the guides, striped
 wall(m,21.1f,6.f,22.6f,10.f,f,f+.04f,8);floorLine(m,21.f,5.9f,22.6f,6.f,f,13);floorLine(m,21.f,10.f,22.6f,10.1f,f,13);
 for(float y:{5.4f,10.6f})anModel(m,model::pwr_safety_bollard,22.25f,y,f,0,true);
 anModel(m,model::pwr_control_cabinet,22.75f,12.5f,f,anW,true);m.pipes.push_back({{22.8f,12.5f},{22.8f,12.5f},f+1.6f,.025f,roof,6});
 wall(m,22.85f,10.36f,22.97f,10.64f,f+4.36f,f+4.52f,18);                                    // the beacon, at the shutter's south end: clear of the office
 // --- The booking bay: tonight's consignments as they were booked in ---------------------------------
 floorLine(m,13.4f,6.4f,19.4f,6.5f,f,13);floorLine(m,13.4f,10.f,19.4f,10.1f,f,13);floorLine(m,13.4f,6.5f,13.5f,10.f,f,13);floorLine(m,19.3f,6.5f,19.4f,10.f,f,13);
 // crates 2 and 5, strapped and tagged, waiting on the hoist; the bore's timber cargo; a pallet of packing
 for(auto [x,y,n]:{std::tuple{16.4f,7.25f,2},{14.25f,9.2f,5}}){
  anFix(m,model::pallet,x,y,f,1.08f,1.2f,.16f,0,true);anModel(m,model::lab_specimen_crate,x,y,f+.16f,0,true);
  for(float d:{-.3f,.3f}){wall(m,x+d-.03f,y-.57f,x+d+.03f,y+.57f,f+1.28f,f+1.29f,18);for(float s:{-.58f,.57f})wall(m,x+d-.03f,y+s,x+d+.03f,y+s+.01f,f+.2f,f+1.28f,18);}
  if(n==2)taped(m,{x,y-.58f},f+.8f,anN,Readable::Note,"CRATE 2 / 0211-49","DEEP EXTRACTION, LEVEL 9 FACE. ORE, RAW, AS CUT. 310 KG. HANDLE AS LIVE.",
   "BOOKED IN 02:11. COLD. QUIET. HOLD FOR ANALYSIS.");
  else taped(m,{x,y-.58f},f+.8f,anN,Readable::Note,"CRATE 5 / 0211-49","DEEP EXTRACTION, LEVEL 9 FACE. ORE, RAW, AS CUT. 296 KG. HANDLE AS LIVE.",
   "BOOKED IN 02:16. COLD. IT TICKS WHEN THE HOIST RUNS, LIKE IT IS COUNTING.");}
 cargo(m,14.25f,7.2f,f,true,0);cargo(m,18.55f,7.2f,f,true,.04f);
 anFix(m,model::pallet,18.5f,9.25f,f,1.2f,1.15f,.16f,kPi*.5f,true);
 for(int k=0;k<2;++k)for(int i=0;i<4-k*2;++i)kit::prop(m,model::prop_cardboard_box,18.24f+(i%2)*.52f,8.99f+(i/2)*.52f,f+.16f+k*.252f,.5f,.495f,.252f,0);
 anModel(m,model::ind_cargo_full,17.9f,14.3f,f,anS,true);                                 // the bore's cargo box, not yet lifted
 kit::prop(m,model::prop_cardboard_box,17.9f,10.6f,f,.3f);
 paperDrift(m,13.6f,10.3f,19.f,12.6f,f,6,7901);
 handCart(m,20.4f,12.f,f);
 // --- The racking (southwest): two bays against the wall, one across the aisle; the forklift ----------
 rack(m,1.4f,14.6f,f,2);rack(m,1.4f,18.5f,f,2);rack(m,5.5f,16.4f,f,2,true);
 for(auto [x,y]:{std::pair{1.4f,14.6f},{1.4f,18.5f},{5.5f,16.4f}})wall(m,x,y-.07f,x+2.76f,y+4.02f,f,roof,30);   // nobody climbs or crawls into the racking
 // caged lamps clamped to the aisle uprights under the top deck: the high-bays cannot reach the lower bays
 for(auto [x,y,yaw]:{std::tuple{4.27f,14.68f,anE},{4.27f,22.38f,anE},{5.39f,16.48f,anW},{5.39f,20.28f,anW}}){
  anModel(m,model::mine_cage_lamp,x,y,f+2.5f,yaw,false);m.lights.push_back(lamp({x+(yaw>0?.12f:-.12f),y},f+2.62f,.42f,4.5f,3200,0,WorldLightMount::Bare));}
 anModel(m,model::ind_pallets,6.4f,21.85f,f,anS,true);wall(m,5.29f,20.96f,7.51f,22.74f,f+1.1f,f+3.f,30);   // the empties, stacked in the corner
 m.lights.push_back(lamp({8.2f,22.97f},f+2.9f,.5f,6.f,3800,0,WorldLightMount::Wall,-kPi*.5f));
 m.lights.push_back(lamp({9.78f,19.f},f+2.7f,.45f,5.f,3200,0,WorldLightMount::Wall,kPi));     // the alley behind the racking
 anModel(m,model::pwr_fire_cabinet,9.77f,20.6f,f+1.f,anW,false);anModel(m,model::ind_barrel,9.35f,22.4f,f,1.1f,true);kit::prop(m,model::mine_shovel,9.6f,21.75f,f,.114f,.313f,1.514f,anW);
 forklift(m,6.6f,14.2f,f);wall(m,6.02f,12.4f,7.18f,16.f,f+2.2f,f+3.4f,30);
 wall(m,6.1f,12.9f,7.1f,15.4f,f+.002f,f+.006f,8);                                          // its oil
 m.pickupSpawns.push_back({{4.8f,18.2f},PickupKind::Ammo,f});
 // between the passage and the cage: the empty pallets, the drums, the packing for the next lift
 anModel(m,model::ind_pallets,15.85f,21.75f,f,anS,true);wall(m,14.74f,20.86f,16.96f,22.64f,f+1.1f,f+3.f,30);
 for(auto [x,y]:{std::pair{16.35f,19.4f},{16.9f,19.55f}})anModel(m,model::ind_barrel,x,y,f,x*2.f,true);
 anModel(m,model::pwr_duct_fan_unit,14.95f,19.9f,f,anE,true);wall(m,14.2f,19.5f,15.7f,20.3f,f+1.95f,roof,30);for(float y:{19.7f,20.1f})m.pipes.push_back({{14.75f,y},{14.75f,y},f+1.95f,.05f,roof,8});   // the passage's air, up through the sheet
 anFix(m,model::pallet,16.f,17.9f,f,1.2f,1.15f,.16f,0,true);
 for(int i=0;i<3;++i)kit::prop(m,model::prop_cardboard_box_open,15.74f+(i%2)*.52f,17.64f+(i/2)*.52f,f+.16f,.5f,.5f,.5f,0);
 // --- The quarantine cage (southeast): crate 6, opened from the inside ------------------------------
 // Steel grille in whole 2.5 m bays (x 17.5-22.5, y 17-22), each with its collision behind it;
 // the second front bay is the gate, swung in and left.
 for(float y:{18.25f,20.75f})for(float x:{17.5f,22.5f}){anFix(m,model::med_cell_bars,x,y,f,2.5f,.04f,2.4f,anE,false);wall(m,x-.03f,y-1.25f,x+.03f,y+1.25f,f,f+2.4f,30);}
 for(float x:{18.75f,21.25f}){anFix(m,model::med_cell_bars,x,22.f,f,2.5f,.04f,2.4f,0,false);wall(m,x-1.25f,21.97f,x+1.25f,22.03f,f,f+2.4f,30);}
 anFix(m,model::med_cell_bars,18.75f,17.f,f,2.5f,.04f,2.4f,0,false);wall(m,17.5f,16.97f,20.f,17.03f,f,f+2.4f,30);
 anFix(m,model::med_cell_bars,20.05f,18.3f,f+.03f,2.5f,.04f,2.4f,anE+.12f,false);wall(m,20.f,17.1f,20.1f,19.5f,f,f+2.4f,30);   // the gate
 for(float x=17.5f;x<22.51f;x+=.5f)wall(m,x-.02f,17.f,x+.02f,22.f,f+2.4f,f+2.44f,8);          // its roof, flat bar both ways
 for(float y=17.f;y<22.01f;y+=.5f)wall(m,17.5f,y-.02f,22.5f,y+.02f,f+2.44f,f+2.48f,8);
 wall(m,17.45f,16.95f,22.55f,22.05f,f+2.48f,roof,30);                                       // and nobody on it
 anModel(m,model::mine_cage_lamp,20.f,19.5f,f+2.04f,0,false);                              // its caged bulkhead, under the bars
 wall(m,21.2f,16.98f,21.4f,17.02f,f+2.28f,f+2.4f,19);                                      // the gate alarm
 // the empty pallet, its straps cut; crate 6 knocked off it, open; its lid, and what it was packed in
 anFix(m,model::pallet,19.4f,20.4f,f,1.08f,1.2f,.16f,.1f,true);
 for(float x:{19.15f,19.7f})wall(m,x,19.9f,x+.04f,20.95f,f+.16f,f+.17f,18);
 anModel(m,model::lab_specimen_crate,21.3f,20.6f,f,0,true);
 wall(m,20.86f,20.16f,21.74f,21.04f,f+1.12f,f+1.124f,8);                                    // its top, gone
 wall(m,20.7f,20.f,21.9f,21.2f,f+1.13f,f+2.4f,30);
 wall(m,18.3f,18.1f,19.4f,19.2f,f,f+.05f,8);wall(m,18.32f,18.12f,19.38f,18.16f,f+.05f,f+.07f,18);   // the lid, thrown off
 anModel(m,model::prop_cardboard_box_open,21.4f,18.6f,f,1.9f,false);
 anModel(m,model::lab_anomaly_relic,21.95f,21.6f,f,1.2f,true);                            // what was left in it
 anHum(m,"anl_cage_hum",17.5f,17.f,22.5f,22.f,f,f+2.4f,.4f);
 // behind it, against the wall, the drums nobody has moved since the cage went up; they shut the gap off
 for(auto [x,y]:{std::pair{17.24f,22.45f},{18.05f,22.5f},{21.3f,22.55f},{22.68f,16.72f}}){anModel(m,model::ind_barrel,x,y,f,x*1.7f,true);wall(m,x-.28f,y-.28f,x+.28f,y+.28f,f+.97f,f+2.6f,30);}
 // out of the gate: the porter's things where he dropped them, and where he was dragged
 wall(m,20.3f,17.4f,21.8f,17.7f,f,f+.01f,16);
 for(auto [x1,y1,x2,y2]:{std::tuple{21.05f,16.3f,21.45f,17.4f},{21.3f,15.2f,21.75f,16.3f},{21.6f,14.3f,22.1f,15.2f}})wall(m,x1,y1,x2,y2,f+.004f,f+.01f,16);
 kit::prop(m,model::mine_hard_hat,19.85f,16.3f,f,.4f);
 kit::prop(m,model::item_walkie,20.55f,15.85f,f,1.1f);
 kit::prop(m,model::office_paper_stack,19.3f,16.55f,f,.33f,.44f,.03f,.5f);
 kit::prop(m,model::item_dosimeter,18.8f,16.15f,f,.31f,.146f,.066f,2.6f);
 m.creatureSpawns.push_back({CreatureKind::Mutant,{21.5f,15.8f},-999,1,0});                  // out, in the hall's shadow
 // --- The south passage on to Analysis: the wing's teal starts here ----------------------------------
 wall(m,9.8f,17.f,10.f,23.f,f,f+3.f,36);wall(m,14.f,17.f,14.2f,23.f,f,f+3.f,36);
 wall(m,9.8f,17.f,14.2f,23.f,f+3.f,f+3.2f,22);ceilingGrid(m,10.f,17.f,14.f,23.f,f+3.f);
 wall(m,10.f,17.f,14.f,23.f,f,f+.005f,27);
 wall(m,10.f,22.97f,14.f,23.f,f+3.2f,f+4.4f,3);wall(m,10.f,23.f,14.f,24.f,f+3.f,roof,3);     // over it at the seam, to the sheet
 crash(10.f,10.04f,17.f,23.f);crash(13.96f,14.f,17.f,23.f);
 wall(m,9.8f,17.f,14.2f,23.f,f+3.2f,roof,30);                                              // nobody on its roof
 extinguisher(m,10.f,18.4f,f,anE);
 anFix(m,model::med_notice_board,13.965f,20.4f,f+1.25f,1.2f,.07f,.9f,anW,false);
 m.pickupSpawns.push_back({{13.6f,21.6f},PickupKind::Health,f});
 m.lights.push_back(inTube(12.f,19.6f,f+3.f,.5f,5.5f,5600,.05f,0,kPi*.5f));
 // --- Light: mercury and sodium high-bays under the sheet, one dead; the dock's flood and beacon; the
 //     cage's bulkhead, its alarm, and the green of what it held; crate 4's heat --------------------------
 inPendant(m,{6.f,8.2f},roof,f+4.4f,1.5f,9.f,4300,.08f);
 inPendant(m,{12.f,8.2f},roof,f+4.4f,1.5f,9.f,2100,-.05f);                                    // sodium, the one they replaced last
 inPendant(m,{18.f,8.2f},roof,f+4.4f,1.5f,9.f,4300,.08f);
 inPendant(m,{6.f,15.5f},roof,f+4.4f,1.3f,9.f,4300,.08f,.3f);
 inPendant(m,{12.f,15.5f},roof,f+4.4f,0,0,0,0);                                                // dead
 inPendant(m,{18.f,15.5f},roof,f+4.4f,1.4f,9.f,4300,.08f);
 inPendant(m,{4.8f,20.f},roof,f+4.6f,1.15f,8.f,4300,.08f);                                     // the rack aisle
 inPendant(m,{16.2f,20.7f},roof,f+4.4f,1.1f,8.f,2100,-.05f);                                    // the cage approach
 m.lights.push_back(lamp({22.96f,8.f},f+4.65f,.9f,9.f,4300,.08f,WorldLightMount::Wall,kPi));    // the dock flood
 {auto l=lamp({22.82f,10.5f},f+4.2f,.45f,5.f,2000,0,WorldLightMount::Bare,0,true,{1.f,.45f,.08f});l.flicker=.7f;m.lights.push_back(l);}
 {auto l=lamp({20.f,19.5f},f+1.95f,.35f,4.f,5200,.06f,WorldLightMount::Bare);l.flicker=.4f;m.lights.push_back(l);}
 {auto l=lamp({21.3f,16.85f},f+2.3f,.2f,2.6f,1700,-.25f,WorldLightMount::Bare,0,true,{1.f,.12f,.06f});l.flicker=.8f;m.lights.push_back(l);}
 {auto l=lamp({21.95f,21.6f},f+.55f,.5f,3.f,3000,0,WorldLightMount::Bare,0,false,{.35f,1.f,.5f});l.flicker=.25f;m.lights.push_back(l);}
 {auto l=lamp({8.05f,12.f},f+.25f,.35f,2.f,1800,0,WorldLightMount::Bare,0,false,{1.f,.35f,.12f});l.flicker=.2f;m.lights.push_back(l);}
 // --- Names: the cage keeps its own green air -----------------------------------------------------------
 zone(m,"",17.5f,17.f,22.5f,22.f,-1000,1000,{.1f,.28f,.16f,.03f});
 zone(m,"MATERIALS INTAKE / RECEIVING",9.f,0.f,15.f,5.f);
 zone(m,"MATERIALS INTAKE / MANIFEST BOOTH",1.f,5.f,6.2f,10.f);
 zone(m,"MATERIALS INTAKE / PORTERS",15.f,1.f,23.f,4.8f);
 zone(m,"MATERIALS INTAKE / QUARANTINE CAGE",17.5f,17.f,22.5f,22.f);
 zone(m,"MATERIALS INTAKE / LABS PASSAGE",9.8f,17.f,14.2f,24.f);
 zone(m,"MATERIALS INTAKE / DOCK HALL",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// SAMPLE STORAGE (manifest 50). The cold store a floor below intake: everything
// Deep Extraction sent up, and everything Medical drew out of its patients, kept
// at -80 in numbered freezers. The cold plant failed at 02:10, the freezers ran on
// battery till 02:58 and the tubes are going with it. The meltwater lies on the
// plant room floor, still and wet, which is what resonance likes: it hums, and
// standing in it stings. Freezer 6 is still running and still warming. The
// Investigator's people came at 03:05 for Procedure 14's material and LAB 4's
// draws; their cabinet stands open and empty. Something that was kept here
// watches from the -80 room at the end of the dry aisle.
// Route: the E seam (z -12) -> the control room -> the stair -> the store: the specimen
// room, the freezer hall, round the plant room's water, the dry aisle -> back.
std::shared_ptr<const Map> makeSampleStorage(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[80];m.definition=CampaignChunks[80];m.definition.ambient=.22f;
 m.definition.playerStart={22.f,11.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi;
 // Insulated white panel and lab tile under a slab at -12.2; the air is cold.
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceLabPanel;
 m.definition.haze={.16f,.18f,.21f,.006f};
 const float f=-15,up=-12,roof=m.definition.ceiling,ceil=-12.2f;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 rectangle(m.layers[0],17,1,23,7,'#');                                                       // the plant riser behind the control room
 openEdge(m,{'E',9.f,14.f,up,false,0});
 machineryClock(m,"anl_cold_clock",60000);
 auto crash=[&](float x1,float x2,float y1,float y2,float z){if(z>up-.5f)wall(m,x1,y1,x2,y2,z+.86f,z+.96f,20);else wall(m,x1,y1,x2,y2,z+.82f,z+.97f,18);};   // the store's bump rails are safety yellow
 // --- The control room at intake level, on its plinth over the store; the stair down ------------------
 deck(m,up,17,7,24,16);wall(m,17.f,7.f,24.f,16.f,f,up-.22f,3);
 m.stairs.push_back({12.5f,9.f,17.f,11.5f,f,up,16,false,true});                            // down, westward
 for(auto [x1,y1,x2,y2]:{std::tuple{16.8f,7.f,17.f,9.f},{16.8f,11.5f,17.f,16.2f},{17.f,16.f,23.f,16.2f}})wall(m,x1,y1,x2,y2,up,roof,37);   // intake's green dado: this is intake's floor
 for(auto [x1,y1,x2,y2]:{std::tuple{17.f,7.f,23.f,7.05f},{22.95f,7.f,23.f,9.f},{22.95f,14.f,23.f,16.f}})wall(m,x1,y1,x2,y2,up,up+2.9f,37);
 for(auto [x1,y1,x2,y2]:{std::tuple{17.f,7.f,23.f,7.07f},{22.93f,7.f,23.f,9.f},{22.93f,14.f,23.f,16.f},{17.f,15.93f,23.f,16.f},{17.f,7.f,17.07f,9.f},{17.f,11.5f,17.07f,16.f}})
  wall(m,x1,y1,x2,y2,up,up+.1f,20);                                                         // skirting
 crash(17.f,17.04f,11.5f,16.f,up);crash(17.f,23.f,15.96f,16.f,up);
 wall(m,17.f,7.f,23.f,16.f,up,up+.008f,23);
 wall(m,12.3f,7.f,24.f,16.2f,up+2.9f,up+3.05f,22);ceilingGrid(m,17.f,7.f,23.f,16.f,up+2.9f);ceilingGrid(m,12.5f,9.f,17.f,11.5f,up+2.9f);
 // the desk, facing the control board on the north wall; the freezer mimic on the west wall
 workstation(m,20.4f,8.4f,up,-1,0);
 const int deskLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,19.85f,8.52f,up+.75f,.18f,.4f,.52f,kPi);
 anModel(m,model::lab_control_board,20.4f,7.15f,up+1.25f,anS,false);
 terminal(m,{22.75f,8.f},"COLD STORE / CONTROL","PLANT FAULT 02:10. LN2 FEED LOST. FREEZERS 1-12 ON BATTERY. BATTERY 0% 02:58.",
  "FREEZER 6 STILL RUNNING. FREEZER 6 INTERNAL +4 C AND RISING. COMPRESSOR DRAW NORMAL. NO FAULT FOUND.",up-f);
 anFix(m,model::med_wall_clock,18.2f,7.06f,up+2.1f,.24f,.02f,.23f,anS,false);
 anFix(m,model::med_notice_board,22.f,15.965f,up+1.25f,1.2f,.07f,.9f,anN,false);
 anFix(m,model::water_cooler,17.5f,15.7f,up,.35f,.35f,1.1f,anN,true);
 kit::prop(m,model::office_chair,18.7f,11.9f,up,.55f,.55f,.95f,2.3f);                       // shoved back from the mimic
 kit::prop(m,model::trash_can,21.8f,7.4f,up,.27f,.27f,.34f,.4f);
 paperDrift(m,17.4f,9.3f,19.6f,11.3f,up,4,8003);
 wall(m,17.f,11.95f,17.03f,15.05f,up+1.1f,up+2.15f,2);                                       // the mimic board: freezers 1-12
 for(int i=0;i<12;++i)anModel(m,model::lab_status_display,17.06f,12.35f+(i%4)*.76f,up+1.2f+(i/4)*.3f,anE,false);
 taped(m,{22.95f,15.1f},up+1.5f,anW,Readable::Note,"FREEZER 6 / ALARM LOG","WARM ALARM 02:12. RESET. WARM ALARM 02:14. RESET. WARM ALARM 02:15. UNPLUGGED IT.",
  "IT IS STILL RUNNING. THE LIGHT IS ON. THE CORD IS IN MY HAND. THE SAMPLES ARE WARMING THE FREEZER, NOT THE OTHER WAY ROUND.");
 m.pickupSpawns.push_back({{21.6f,15.f},PickupKind::Health,up});
 // --- The stair well: walled each side from the store floor to the ceiling, the bulkhead over its foot --
 wall(m,12.3f,8.8f,17.f,9.f,f,up+2.9f,3);wall(m,12.3f,11.5f,17.f,11.7f,f,up+2.9f,3);wall(m,12.3f,9.f,12.5f,11.5f,ceil,up+2.9f,3);
 for(auto [x1,x2,top]:{std::tuple{13.6f,14.7f,-14.55f},{14.7f,15.8f,-13.8f},{15.8f,17.f,-13.05f}})wall(m,x1,9.f,x2,11.5f,f,top,15);   // the flight's stepped base
 wall(m,12.5f,9.f,12.56f,11.5f,ceil,ceil+.35f,20);for(float y:{9.f,11.42f})wall(m,12.44f,y,12.52f,y+.08f,f,ceil,8);   // the bulkhead's band and posts
 floorLine(m,12.3f,9.f,12.42f,11.5f,f,13);
 // --- The store: a slab at -12.2 over all of it but the well; rooms in insulated panel ------------------
 wall(m,1.f,1.f,17.f,8.8f,ceil,up,22);wall(m,1.f,8.8f,12.3f,11.7f,ceil,up,22);wall(m,1.f,11.7f,17.f,23.f,ceil,up,22);wall(m,17.f,16.f,23.f,23.f,ceil,up,22);
 wall(m,1.f,.9f,17.f,1.f,f,roof,3);wall(m,.9f,1.f,1.f,23.f,f,roof,3);
 // freezer hall | specimen room, glazed; the open way from the stair foot
 wall(m,10.9f,1.f,11.1f,2.f,f,ceil,3);inGlaze(m,10.9f,2.f,11.1f,7.f,f,ceil,.95f,2.1f);wall(m,10.9f,7.f,11.1f,9.3f,f,ceil,3);
 doorwayY(m,9.3f,11.3f,10.9f,11.1f,f,ceil,2.3f);wall(m,10.9f,11.3f,11.1f,15.f,f,ceil,3);
 // freezer hall | cold plant, a doorway; cold plant | dry aisle, another: the loop round the water
 wall(m,1.f,14.8f,5.6f,15.f,f,ceil,3);inDoorwayX(m,5.6f,6.8f,14.8f,15.f,f,ceil,2.2f);wall(m,6.8f,14.8f,10.9f,15.f,f,ceil,3);
 wall(m,10.9f,15.f,11.1f,19.6f,f,ceil,3);doorwayY(m,19.6f,20.9f,10.9f,11.1f,f,ceil,2.2f);wall(m,10.9f,20.9f,11.1f,23.f,f,ceil,3);
 // dry aisle | the -80 room: insulated panel, its door wedged open
 wall(m,17.f,16.f,23.f,16.2f,f,ceil,28);
 wall(m,16.8f,16.f,17.f,18.6f,f,ceil,28);doorwayY(m,18.6f,19.8f,16.8f,17.f,f,ceil,2.15f);wall(m,16.8f,19.8f,17.f,23.f,f,ceil,28);
 for(auto [x1,y1,x2,y2]:{std::tuple{16.76f,16.2f,16.8f,18.6f},{16.76f,19.8f,16.8f,23.f},{17.f,16.2f,17.04f,18.6f},{17.f,19.8f,17.04f,23.f}})wall(m,x1,y1,x2,y2,f,f+.12f,20);   // steel kerb
 wall(m,15.66f,19.8f,16.76f,19.88f,f+.02f,f+2.1f,28);                                        // the door, swung right back
 wall(m,15.7f,19.88f,15.76f,19.95f,f+.95f,f+1.25f,8);wall(m,15.62f,19.8f,15.66f,19.88f,f+.02f,f+2.1f,8);   // its lever and gasket edge
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,1.f,10.9f,14.8f},{11.1f,1.f,17.f,8.8f},{1.f,15.f,10.9f,23.f},{11.1f,11.7f,16.8f,23.f},{17.f,16.2f,23.f,23.f}})ceilingGrid(m,x1,y1,x2,y2,ceil);
 for(auto [x1,y1,x2,y2]:{std::tuple{5.75f,1.f,10.9f,1.012f},{1.f,6.65f,1.012f,14.8f},{1.f,14.788f,5.54f,14.8f},{6.86f,14.788f,10.9f,14.8f},{10.888f,1.f,10.9f,9.24f},{10.888f,11.36f,10.9f,14.8f},
  {11.1f,1.45f,11.112f,8.8f},{16.988f,1.45f,17.f,8.8f},{12.3f,8.788f,17.f,8.8f},{11.1f,11.7f,11.112f,19.54f},{11.1f,20.96f,11.112f,23.f},{16.988f,11.7f,17.f,16.f},{12.3f,11.7f,17.f,11.712f},{11.1f,22.988f,16.8f,23.f}})ssKick(m,x1,y1,x2,y2,f);
 cableTray(m,{10.55f,1.4f},{10.55f,14.4f},ceil-.28f);                                        // the freezers' feeds, along the hall
 for(float y=2.f;y<14.4f;y+=1.8f)for(float dx:{-.19f,.19f})m.pipes.push_back({{10.55f+dx,y},{10.55f+dx,y},ceil-.18f,.01f,ceil,6});
 // --- The freezer hall: 1-5 and 7-12 along the walls, 6's bay empty; the sample bench ------------------
 for(int i=0;i<6;++i)if(i!=5)ssFreezer(m,1.65f+i*.75f,1.425f,f,anS);
 for(int i=0;i<6;++i)ssFreezer(m,1.425f,2.4f+i*.75f,f,anE);
 wall(m,1.25f,1.f,5.05f,1.87f,f+2.f,f+2.12f,8);wall(m,1.f,2.f,1.87f,6.6f,f+2.f,f+2.12f,8);       // compressor grilles over the runs
 wall(m,1.f,1.f,6.4f,1.9f,f+2.12f,ceil,30);wall(m,1.f,1.9f,1.9f,6.6f,f+2.12f,ceil,30);
 floorLine(m,1.25f,1.95f,5.05f,2.f,f,13);floorLine(m,1.95f,2.f,2.f,6.6f,f,13);
 // each freezer's controller on the grille, dark since 02:58; three still blink their alarm on the last of the cell
 for(int i=0;i<5;++i)anModel(m,model::lab_status_display,1.65f+i*.75f,1.885f,f+2.01f,anS,false,.5f);
 for(int i=0;i<6;++i)anModel(m,model::lab_status_display,1.885f,2.4f+i*.75f,f+2.01f,anE,false,.5f);
 for(auto [x,y,phase]:{std::tuple{2.4f,2.f,0.f},{2.f,3.15f,.3f},{2.f,5.4f,.55f}}){auto l=lamp({x,y},f+2.05f,.15f,.9f,2400,0,WorldLightMount::Bare,0,false,{1.f,.1f,.06f});l.period=.8f;l.onTime=.4f;l.phase=phase;m.lights.push_back(l);}
 wall(m,5.05f,1.05f,5.75f,1.88f,f+.002f,f+.004f,15);                                          // where 6 stood, before they moved it out
 looseLead(m,{5.66f,1.04f,f+.42f},{5.9f,2.3f,f},1.1f,7,false);
 taped(m,{1.86f,3.9f},f+1.45f,anE,Readable::Note,"FREEZER 9 / RACK CARD","OBS 6 (LAB 4). 2 X 10 ML, EVERY SIX HOURS. READS HIGHER EVERY DRAW.",
  "DO NOT LABEL WITH NAMES. ONE TUBE LEFT IN THE DOOR RACK AFTER I.I. CLEARED IT. IT IS WARM.");
 {auto l=lamp({2.02f,3.9f},f+1.25f,.06f,.6f,5000,0,WorldLightMount::Bare,0,false,{.4f,1.f,.5f});l.flicker=.3f;m.lights.push_back(l);}   // the tube in 9's door rack
 // the sample bench: two worktops back to back, the rack shelf over them, stools
 inCounter(m,3.2f,6.6f,8.8f,7.3f,f,.9f);inCounter(m,3.2f,7.3f,8.8f,8.f,f,.9f);
 for(float x:{3.3f,6.f,8.7f})wall(m,x,7.28f,x+.04f,7.32f,f+.9f,f+1.75f,20);
 for(float z:{1.3f,1.72f})wall(m,3.25f,7.12f,8.75f,7.48f,f+z,f+z+.03f,20);
 wall(m,3.2f,7.08f,8.8f,7.52f,f+1.72f,ceil,30);
 for(int i=0;i<7;++i){if(i==2||i==5)continue;kit::prop(m,model::med_specimen_jar,3.5f+i*.75f,7.25f+(i%2)*.1f,f+1.33f,.146f,.146f,.245f,i*.7f);}
 kit::prop(m,model::lab_microscope,4.1f,6.85f,f+.9f,kPi);kit::prop(m,model::lab_microscope,7.4f,7.75f,f+.9f,0);
 kit::prop(m,model::desk_phone,8.4f,6.9f,f+.9f,.2f,.2f,.1f,kPi);
 kit::prop(m,model::office_paper_stack,5.3f,6.95f,f+.9f,.33f,.44f,.03f,.3f);
 for(auto [x,y,yaw]:{std::tuple{6.3f,7.7f,.2f},{6.7f,6.85f,1.4f}})kit::prop(m,model::med_specimen_jar,x,y,f+.9f,.146f,.146f,.245f,yaw);
 kit::prop(m,model::prop_cardboard_box_open,4.9f,7.65f,f+.9f,.3f,.3f,.3f,.25f);
 const int benchLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,3.55f,6.9f,f+.9f,.18f,.4f,.52f,kPi+.3f);
 for(auto [x,y,yaw]:{std::tuple{4.4f,5.92f,kPi+.3f},{7.1f,6.15f,2.4f},{5.6f,8.55f,1.f}})kit::prop(m,model::wooden_chair,x,y,f,.417f,.525f,.95f,yaw);
 // the hall's walls: the sink, the board, first aid, the LN2 dewars on their chain
 anFix(m,model::med_scrub_sink,1.268f,8.2f,f,.488f,.536f,1.127f,anE,true);wall(m,1.f,7.9f,1.6f,8.5f,f+1.13f,ceil,30);   // nobody on the sink under the slab
 anFix(m,model::med_notice_board,1.035f,10.6f,f+1.25f,1.2f,.07f,.9f,anE,false);
 anModel(m,model::med_first_aid_box,1.07f,12.2f,f+1.35f,anE,false);
 extinguisher(m,1.f,13.4f,f,anE);
 for(float x:{2.25f,2.8f,3.35f})ssDewar(m,x,14.5f,f);
 wall(m,2.f,14.76f,3.6f,14.8f,f+.82f,f+.86f,8);
 crash(1.9f,10.9f,14.76f,14.8f,f);crash(6.4f,10.9f,1.f,1.04f,f);crash(1.f,1.04f,8.6f,13.f,f);
 m.pickupSpawns.push_back({{2.5f,10.4f},PickupKind::Ammo,f});
 // the refrigerant pair from the plant, under the slab to the freezer runs, and its drops
 for(float x:{2.3f,2.45f}){m.pipes.push_back({{x,14.8f},{x,1.6f},ceil-.1f,.035f,ceil-.1f,5});m.pipes.push_back({{x,1.6f},{x,1.6f},f+2.12f,.035f,ceil-.1f,5});}
 for(float y:{4.f,7.f,10.f,13.f})wall(m,2.2f,y,2.55f,y+.05f,ceil-.16f,ceil,8);
 // --- The specimen room: the cabinet run, I.I.'s cabinet stripped, crates 1 and 3 from intake -----------
 for(float x:{11.75f,12.9f,16.4f})anModel(m,model::med_supply_cabinet,x,1.222f,f,anS,true);
 anModel(m,model::lab_specimen_cabinet,14.4f,1.37f,f,anS,true);                            // I.I.'s: open, every shelf bare
 wall(m,11.2f,1.f,13.45f,1.45f,f+1.7f,ceil,30);wall(m,15.85f,1.f,16.95f,1.45f,f+1.7f,ceil,30);
 wall(m,13.55f,1.8f,15.25f,3.1f,f,f+.004f,20);wall(m,13.85f,2.05f,14.95f,2.8f,f+.004f,f+.006f,15);   // frost, and the cold box's clean footprint in it
 taped(m,{15.46f,1.01f},f+1.4f,anS,Readable::Note,"RELEASED TO I.I. / 03:05","ALL PROCEDURE 14 MATERIAL. ALL OBS 6 DRAWS. ALL PAPERWORK FOR BOTH.",
  "THEY BROUGHT THEIR OWN COLD BOX AND THEIR OWN SEALS. THEY DID NOT SIGN. HE DID NOT LOOK AT THE SHELVES, ONLY AT ME.");
 for(auto [x,y,yaw]:{std::tuple{13.1f,3.7f,.3f},{12.3f,5.3f,1.9f},{14.5f,4.6f,2.6f}})kit::prop(m,model::prop_cardboard_box_open,x,y,f,.36f,.36f,.36f,yaw);
 for(auto [x,y,yaw]:{std::tuple{12.6f,6.4f,.4f},{11.9f,7.6f,2.2f},{13.4f,5.9f,1.1f},{11.7f,8.4f,.9f}})kit::prop(m,model::office_paper_sheet,x,y,f,.4f,.293f,.004f,yaw);
 kit::prop(m,model::item_id_card,12.2f,7.1f,f,.132f,.085f,.002f,.7f);
 for(auto [y,n]:{std::pair{3.5f,1},{5.5f,3}}){anFix(m,model::pallet,16.2f,y,f,1.08f,1.2f,.16f,kPi*.5f,true);anModel(m,model::lab_specimen_crate,16.2f,y,f+.16f,kPi*.5f,true);
  if(n==1)taped(m,{15.64f,y},f+.8f,anW,Readable::Note,"CRATE 1 / 0211-49","DEEP EXTRACTION, LEVEL 9 FACE. ORE, RAW, AS CUT. 288 KG. HANDLE AS LIVE.",
   "ROUTED HERE 02:20 FROM INTAKE. COLD ON ARRIVAL. -80 WHEN A FREEZER IS FREE. NONE IS.");}
 wall(m,15.6f,2.9f,16.8f,6.1f,f+1.28f,ceil,30);                                            // nobody on the crates into the slab
 anModel(m,model::med_crash_cart,13.f,7.6f,f,.4f,true);wall(m,12.5f,7.15f,13.5f,8.05f,f+1.35f,ceil,30);   // their trolley, left at the foot
 kit::prop(m,model::prop_cardboard_box_open,13.f,7.6f,f+1.f,.3f,.3f,.3f,.2f);
 crash(11.1f,11.14f,7.f,8.8f,f);crash(16.96f,17.f,6.2f,8.8f,f);
 anFix(m,model::med_notice_board,14.7f,8.765f,f+1.25f,1.2f,.07f,.9f,anN,false);extinguisher(m,16.4f,8.8f,f,anN);
 anFix(m,model::med_wall_clock,13.2f,8.78f,f+2.f,.24f,.02f,.23f,anN,false);
 // --- The stair foot and the dry aisle: the route line to freezer 6, still running at the far end -----
 floorLine(m,12.2f,11.8f,12.28f,22.6f,f,14);floorLine(m,12.2f,22.52f,13.3f,22.6f,f,14);
 ssFreezer(m,14.f,22.55f,f,anN);                                                            // freezer 6, moved out of the hall, alone
 wall(m,13.65f,22.12f,14.35f,22.98f,f+2.f,f+2.12f,8);
 anModel(m,model::lab_status_display,14.f,22.11f,f+2.12f,anN,false,.5f);
 wall(m,15.36f,22.97f,15.46f,23.f,f+.3f,f+.42f,20);                                          // the socket it was pulled from
 looseLead(m,{14.37f,22.8f,f+.3f},{15.3f,22.3f,f},1.6f,7,false);
 m.waterVolumes.push_back({13.3f,21.3f,14.7f,22.1f,f-.08f,f-.03f});                          // what it sweats
 anHum(m,"anl_freezer6_hum",12.9f,20.8f,15.1f,23.f,f,f+2.4f,.35f);
 for(float y:{16.4f,17.15f,17.9f})anModel(m,model::lab_sample_fridge,11.525f,y,f,anE,true);     // the bench fridges, boxes on them
 wall(m,11.1f,16.f,11.98f,18.3f,f+1.16f,ceil,30);
 for(auto [y,yaw]:{std::pair{16.3f,.05f},{17.1f,-.08f},{18.05f,.1f}})kit::prop(m,model::prop_cardboard_box,11.5f,y,f+1.f,.24f,.24f,.12f,yaw);
 anModel(m,model::lab_status_display,11.13f,17.15f,f+1.9f,anE,false);
 anFix(m,model::med_notice_board,11.135f,13.3f,f+1.25f,1.2f,.07f,.9f,anE,false);anFix(m,model::med_wall_clock,16.99f,15.2f,f+2.05f,.24f,.02f,.23f,anW,false);
 for(float y:{12.55f,13.1f,13.65f})ssDewar(m,16.7f,y,f);
 wall(m,16.96f,12.2f,17.f,14.1f,f+.82f,f+.86f,8);
 anModel(m,model::med_crash_cart,14.6f,15.2f,f,.5f,true);wall(m,14.1f,14.75f,15.1f,15.65f,f+1.2f,ceil,30);   // a sample trolley, abandoned mid-aisle
 kit::prop(m,model::prop_cardboard_box,14.6f,15.2f,f+1.f,.24f,.24f,.12f,.5f);
 kit::prop(m,model::prop_cardboard_box_open,12.6f,17.4f,f,.36f,.36f,.36f,.9f);
 crash(11.1f,11.14f,14.3f,19.6f,f);crash(11.1f,11.14f,20.9f,23.f,f);crash(16.76f,16.8f,16.2f,18.4f,f);crash(16.96f,17.f,14.2f,16.f,f);
 anModel(m,model::lab_id_scanner,16.77f,18.3f,f+1.1f,anW,false);
 taped(m,{16.76f,20.6f},f+1.5f,anW,Readable::Note,"-80 ROOM / DOOR","WEDGED OPEN 03:05 FOR THE I.I. TROLLEY. THEY TOOK THE TOP SHELF AND LEFT THE DOOR.",
  "03:40 THE BACK SHELF HAS MOVED. NOBODY HAS BEEN IN. I AM NOT GOING IN TO SHUT IT.");
 m.pickupSpawns.push_back({{15.6f,21.4f},PickupKind::Ammo,f});
 // --- The cold plant (southwest): the cryo unit, its LN2 line, the pumps, and its meltwater ------------
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,15.f,1.05f,23.f},{1.f,22.95f,10.9f,23.f},{1.f,15.f,5.6f,15.03f},{6.8f,15.f,10.9f,15.03f},{10.87f,15.f,10.9f,19.6f},{10.87f,20.9f,10.9f,23.f}})wall(m,x1,y1,x2,y2,f,ceil,37);
 equipmentPad(m,1.05f,18.3f,3.25f,20.7f,f);
 anModel(m,model::lab_analyzer_cabinet,2.f,19.5f,f+.1f,anE,true,.85f);                       // the cryo plant
 wall(m,2.95f,19.2f,2.96f,19.8f,f+.7f,f+2.3f,28);                                            // frost on its face
 for(float y:{18.92f,20.08f})anModel(m,model::lab_pressure_gauge,3.f,y,f+1.45f,anE,false);
 pipeRoute(m,{{2.95f,19.f,f+2.35f},{4.6f,19.f,f+2.35f},{4.6f,15.05f,f+2.35f}},.05f,5,ceil);  // the LN2 feed to the freezer hall
 gateValve(m,{4.6f,17.4f,f+2.35f},{0,1},.05f,f);
 equipmentPad(m,1.05f,21.3f,3.5f,22.95f,f);
 for(float x:{1.8f,2.9f}){anModel(m,model::pwr_circulation_pump,x,22.1f,f+.1f,0,true);m.pipes.push_back({{x,22.1f},{x,22.1f},f+.49f,.05f,f+2.45f,1});}
 m.pipes.push_back({{2.9f,22.1f},{1.05f,22.1f},f+2.45f,.05f,f+2.45f,1});
 anModel(m,model::pwr_control_cabinet,1.25f,17.4f,f,anE,true);
 for(float x:{2.3f,2.6f})anFix(m,model::pwr_breaker_panel,x,15.081f,f+1.3f,.183f,.102f,.457f,anS,false);
 anModel(m,model::pwr_duct_fan_unit,10.25f,22.2f,f,0,true);for(float x:{10.f,10.4f})m.pipes.push_back({{x,22.5f},{x,22.5f},f+1.95f,.04f,ceil,8});
 for(float x:{5.3f,5.85f,6.4f})ssDewar(m,x,22.68f,f);
 wall(m,5.f,22.91f,6.6f,22.95f,f+.82f,f+.86f,8);
 // the meltwater: a skin on the floor below the pad, humming; it stings to stand in
 m.waterVolumes.push_back({3.6f,16.2f,9.6f,21.8f,f-.14f,f-.03f});
 for(auto [x1,y1,x2,y2]:{std::tuple{3.35f,15.95f,9.85f,16.2f},{3.35f,21.8f,9.85f,22.05f},{3.35f,16.2f,3.6f,21.8f},{9.6f,16.2f,9.85f,21.8f}})wall(m,x1,y1,x2,y2,f+.001f,f+.003f,8);   // the wet edge
 hazard(m,Hazard::Kind::Anomaly,3.8f,16.4f,9.4f,21.6f,f-.2f,f+2.2f,3.f);
 anHum(m,"anl_cold_hum",3.4f,16.f,9.8f,22.f,f,f+2.f);
 anFix(m,model::med_floor_drain,10.2f,16.6f,f,.5f,.5f,.01f,0,false);
 floorLine(m,5.6f,15.08f,6.8f,15.2f,f,13);floorLine(m,10.7f,19.6f,10.82f,20.9f,f,13);
 taped(m,{1.05f,16.4f},f+1.45f,anE,Readable::Note,"COLD PLANT / DO NOT MOP","THE WATER HUMS. IT HUMMED BEFORE THE LIGHTS WENT. IT STINGS THROUGH BOOTS.",
  "WALK THE AISLES ROUND IT. IF YOU HAVE TO CROSS, DON'T STOP IN IT. NOTHING STILL GOES IN THE WATER.");
 // --- The -80 room: jars on the shelves, the back shelf moved, frost on the floor --------------------------
 wall(m,17.f,16.2f,23.f,23.f,f,f+.004f,20);
 ssJars(m,18.7f,22.68f,0,f,3);ssJars(m,22.68f,17.4f,kPi*.5f,f,5);ssJars(m,22.68f,21.f,kPi*.5f,f,7);
 ssJars(m,19.9f,18.4f,.45f,f,9);                                                          // the back shelf, pushed out of its line
 for(auto [x,y,yaw]:{std::tuple{19.2f,19.3f,.4f},{20.4f,19.5f,1.9f},{18.6f,18.9f,2.8f}})kit::prop(m,model::med_specimen_jar,x,y,f,.146f,.146f,.245f,yaw);
 for(auto [x1,y1,x2,y2]:{std::tuple{19.12f,17.45f,19.16f,18.1f},{20.62f,17.45f,20.66f,18.75f},{19.9f,17.45f,19.93f,18.3f}})wall(m,x1,y1,x2,y2,f+.004f,f+.006f,8);   // dragged, from inside
 kit::propPose(m,model::med_specimen_jar,20.15f,20.35f,f,.146f,.146f,.245f,.7f,kPi*.5f,0);wall(m,19.75f,20.05f,20.45f,20.6f,f+.004f,f+.006f,16);
 for(auto [x,y,z,yaw]:{std::tuple{20.35f,22.6f,0.f,.1f},{20.88f,22.62f,0.f,-.05f},{20.35f,22.6f,.252f,.1f}})kit::prop(m,model::prop_cardboard_box,x,y,f+z,.5f,.495f,.252f,yaw);
 inDeadTube(m,20.f,22.f,ceil);
 // a tile out of the ceiling over the back shelf, and on the frost below it
 wall(m,21.f,17.2f,21.6f,17.8f,ceil-.006f,ceil,8);wall(m,21.35f,18.2f,21.95f,18.8f,f+.004f,f+.022f,22);
 wall(m,22.4f,19.1f,23.f,20.3f,f+2.f,f+2.6f,20);for(float z=f+2.08f;z<f+2.55f;z+=.08f)wall(m,22.38f,19.15f,22.4f,20.25f,z,z+.03f,8);   // the evaporator
 anModel(m,model::pwr_vent_grille,20.6f,16.22f,f+1.7f,anS,false);
 m.lights.push_back(lamp({22.22f,19.7f},f+2.25f,.22f,3.f,9000,0,WorldLightMount::Bare,0,false,{.55f,.8f,1.f}));   // the evaporator's pilot, still on
 m.creatureSpawns.push_back({CreatureKind::Warden,{21.3f,20.4f},-999,1,0});                   // what was kept here: it waits, and it watches
 // --- Light: the tubes are going with the battery; cold blue-white where they still burn, the plant's
 //     reds, freezer 6's warm glow, the amber over the -80 door, the warm desk upstairs -----------------
 m.lights.push_back(inTube(19.f,9.6f,up+2.9f,.55f,5.5f,4600,.03f));m.lights.push_back(inTube(19.f,13.8f,up+2.9f,.52f,5.5f,4600,.03f));m.lights.push_back(inTube(21.6f,11.5f,up+2.9f,.5f,5.5f,4600,.03f,0,kPi*.5f));
 m.lights.push_back(lamp({19.85f,8.45f},up+1.22f,.3f,2.4f,3000,0,WorldLightMount::Bare));m.lights.back().prop=deskLamp;
 m.lights.push_back(lamp({17.45f,14.2f},up+1.65f,.22f,1.8f,1200,0,WorldLightMount::Bare,0,false,{1.f,.1f,.06f}));
 m.lights.push_back(lamp({17.2f,13.11f},up+1.6f,.1f,.9f,5000,0,WorldLightMount::Bare,0,false,{.3f,1.f,.4f}));      // 6, green on the mimic
 m.lights.push_back(lamp({14.8f,9.f},-11.3f,.5f,6.f,5000,.04f,WorldLightMount::Wall,kPi*.5f));                       // over the flight
 m.lights.push_back(inTube(11.75f,10.2f,ceil,.45f,5.f,5600,.04f,0,kPi*.5f));
 m.lights.push_back(inTube(3.4f,4.2f,ceil,.72f,5.5f,6000,.03f));m.lights.push_back(inTube(7.8f,4.2f,ceil,.4f,5.f,6200,.03f,.6f));
 m.lights.push_back(inTube(6.2f,9.7f,ceil,.62f,5.5f,6000,.03f,.35f));m.lights.push_back(inTube(8.6f,6.3f,ceil,.6f,5.5f,5800,.03f));
 m.lights.push_back(inTube(3.4f,11.4f,ceil,.68f,5.5f,6200,.03f));inDeadTube(m,7.8f,11.4f,ceil);
 m.lights.push_back(lamp({3.55f,6.85f},f+1.36f,.28f,2.2f,3000,0,WorldLightMount::Bare));m.lights.back().prop=benchLamp;
 m.lights.push_back(inTube(13.6f,4.6f,ceil,.62f,5.5f,5000,.03f));m.lights.push_back(inTube(15.4f,7.3f,ceil,.5f,5.f,5000,.03f));
 m.lights.push_back(lamp({14.4f,1.f},f+2.45f,.38f,4.f,4200,0,WorldLightMount::Wall,kPi*.5f));                       // on I.I.'s cabinet
 m.lights.push_back(inTube(14.f,13.6f,ceil,.65f,5.5f,6500,.04f,.4f,kPi*.5f));m.lights.push_back(inTube(14.f,20.6f,ceil,.3f,4.5f,7200,.05f,.5f,kPi*.5f));inDeadTube(m,14.f,17.6f,ceil,kPi*.5f);
 m.lights.push_back(lamp({14.f,21.35f},f+1.1f,.62f,4.5f,2400,0,WorldLightMount::Bare));m.lights.back().flicker=.1f;   // freezer 6, warm
 m.lights.push_back(lamp({14.f,22.05f},f+1.f,.22f,1.2f,2000,0,WorldLightMount::Bare,0,false,{1.f,.55f,.25f}));        // and at its door seal
 m.lights.push_back(lamp({14.f,22.f},f+1.9f,.12f,1.f,5000,0,WorldLightMount::Bare,0,false,{.3f,1.f,.4f}));
 m.lights.push_back(lamp({16.8f,19.2f},f+2.45f,.25f,3.f,1700,-.25f,WorldLightMount::Wall,kPi));                       // the -80 door's alarm
 for(auto [x,y,yaw]:{std::tuple{8.5f,15.03f,kPi*.5f},{8.f,22.95f,-kPi*.5f}}){auto l=lamp({x,y},f+2.35f,.25f,4.f,2400,0,WorldLightMount::Wall,yaw,true,{1.f,.1f,.06f});l.period=.9f;l.onTime=.55f;m.lights.push_back(l);}
 {auto l=lamp({6.6f,19.f},f+.35f,.55f,4.f,11000,-.05f,WorldLightMount::Bare,0,false,{.6f,1.f,.75f});l.flicker=.35f;m.lights.push_back(l);}   // the water's glint
 m.lights.push_back(inTube(10.2f,18.4f,ceil,.45f,5.f,6500,.05f,.2f,kPi*.5f));inDeadTube(m,6.6f,19.f,ceil);
 {auto l=inTube(20.f,19.6f,ceil,.6f,5.f,7500,.06f,.25f);l.gel={.72f,.86f,1.f};m.lights.push_back(l);}
 // --- Names and air: the plant and the -80 room colder and thicker than the rest -----------------------
 zone(m,"SAMPLE STORAGE / CONTROL",16.8f,7.f,24.f,16.2f,up-.5f,1000,{.16f,.15f,.13f,.008f});
 zone(m,"SAMPLE STORAGE / STAIR",12.3f,8.8f,17.f,11.7f);
 zone(m,"SAMPLE STORAGE / COLD PLANT",1.f,15.f,10.9f,23.f,-1000,1000,{.2f,.25f,.3f,.03f});
 zone(m,"SAMPLE STORAGE / -80 ROOM",17.f,16.2f,23.f,23.f,-1000,1000,{.22f,.27f,.32f,.035f});
 zone(m,"SAMPLE STORAGE / FREEZERS",1.f,1.f,10.9f,14.8f,-1000,1000,{.14f,.17f,.21f,.008f});
 zone(m,"SAMPLE STORAGE / SPECIMENS",11.1f,1.f,17.f,8.8f);
 zone(m,"SAMPLE STORAGE / DRY AISLE",11.1f,11.7f,16.8f,23.f);
 zone(m,"SAMPLE STORAGE",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
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

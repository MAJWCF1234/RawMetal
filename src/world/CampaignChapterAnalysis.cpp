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
 // White wall tile and lab tile under a slab at -12.2; the air is cold. The insulated panel is kept for
 // the -80 room; the specimen room is teal-banded paint, the control room and the plant green-banded.
 m.wallSurface=SurfaceClinicTile;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceLabPanel;
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
 for(auto [x1,y1,x2,y2]:{std::tuple{11.1f,1.f,17.f,1.005f},{16.995f,1.005f,17.f,8.8f},{12.3f,8.795f,16.995f,8.8f}})wall(m,x1,y1,x2,y2,f,ceil,36);   // the specimen room's paint
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
 m.waterVolumes.push_back({13.3f,21.3f,14.7f,22.1f,f-.08f,f-.03f,.7f});                          // what it sweats
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
 m.waterVolumes.push_back({3.6f,16.2f,9.6f,21.8f,f-.14f,f-.03f,.75f});
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
 ssJars(m,19.9f,18.4f,.45f,f,9);
 ssJars(m,18.25f,16.47f,0,f,11);                                                            // the north wall's rack, untouched                                                          // the back shelf, pushed out of its line
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
 {auto l=lamp({6.6f,19.f},f+.3f,.2f,2.2f,11000,-.05f,WorldLightMount::Bare,0,false,{.6f,1.f,.75f});l.flicker=.35f;m.lights.push_back(l);}   // the water's glint: a glint, not a lit pool
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
 // The flight runs in a walled well: no stepped side guards (they showed as black teeth against the lit well),
 // a handrail on each wall instead, on three brackets.
 m.stairs.front().sideRails=false;
 for(float y:{9.07f,11.43f}){m.pipes.push_back({{12.56f,y},{16.94f,y},f+.96f,.024f,up+.92f,6});
  for(float x:{12.9f,14.75f,16.6f})m.pipes.push_back({{x,y<10.f?9.f:11.5f},{x,y},f+(x-12.5f)/1.5f+.94f,.014f,f+(x-12.5f)/1.5f+.94f,6});}
 return p;
}
// --- Energy Diagnostics and Field Measurement (83, 84): their own kit (dg), in this file ---------------
// Somebody left where they fell: a scripted track held in the dead pose on the map's clock.
void dgBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
// A timeline started by a terminal setting `timer` (no trigger box), or by walking into x1..y2.
void dgSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1=0,float y1=0,float x2=0,float y2=0){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
// A captioned cue on a timeline, at its own pitch (LAB 4's chat line plays high).
void dgSay(Map& m,const char* timer,int t,Sound s,Vec2 p,float z,const char* caption,float gain,float pitch=1){cue(m,timer,t,s,p,caption,gain,z);m.sequenceCues.back().pitch=pitch;}
// A guard on a beat (the Medical chapter's mdGuard; to be promoted to the kit with it).
void dgGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,float sight,int weapon,float speed,int health){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
// A walking-lamp stake: a steel pole on a foot plate, a caged lamp on top, lit in its turn of the
// walk while `state` is set (period/on/phase in seconds: the light steps from stake to stake).
void dgStake(Map& m,Vec2 p,float f,float head,const char* state,float period,float on,float phase){
 wall(m,p.x-.025f,p.y-.025f,p.x+.025f,p.y+.025f,f,head,20);wall(m,p.x-.16f,p.y-.16f,p.x+.16f,p.y+.16f,f,f+.025f,8);
 anFix(m,model::mine_cage_lamp,p.x,p.y,head,.21f,.2f,.36f,0,false);
 auto l=lamp(p,head+.2f,.6f,4.5f,5200,0,WorldLightMount::Bare);l.enabledState=stateId(state);l.period=period;l.onTime=on;l.phase=phase;m.lights.push_back(l);
}
// A workstation whose screen is a terminal: the desk set at its east half, the terminal's
// console volume at the west end, clear of the props.
void dgDeskTerminal(Map& m,float x,float y,float f,int facing,int variant,const char* title,const char* a,const char* b,StateId state=0,StateId prerequisite=0){
 workstation(m,x,y,f,facing,variant,false);const float s=float(facing),screen=facing>0?kPi:0.f;
 kit::prop(m,model::crt_monitor,x+.15f,y+s*.12f,f+.75f,.42f,.4f,.38f,screen);kit::prop(m,model::keyboard,x+.15f,y-s*.2f,f+.75f,.45f,.16f,.03f,screen);
 terminal(m,{x-.52f,y},title,a,b,0,state,prerequisite);
}
// A paper readable lying on furniture `surface` above the floor (0: on the floor itself).
void dgPaper(Map& m,Vec2 p,float surface,float yaw,Readable kind,const char* title,const char* a,const char* b,StateId state=0){
 terminal(m,p,title,a,b,0,state);auto& t=m.terminals.back();t.readable=kind;t.surface=surface;t.yaw=yaw;
}
// A sound of the building's own (World.h AmbientEmitter, from the bank): a bed that loops, a sound
// now and then, or one that plays once each time `state` takes `value`. A state on a bed or a
// now-and-then sound gates it: heard only while the state has that value.
void dgAmbient(Map& m,Sound sound,Vec2 p,float z,float gain,float radius,int mode,float every=8,float spread=8,const char* state=nullptr,int value=1){
 AmbientEmitter a;a.sound=sound;a.position=p;a.z=z;a.gain=gain;a.radius=radius;a.mode=mode;a.every=every;a.spread=spread;a.state=state?stateId(state):0;a.value=value;m.ambients.push_back(a);
}
// A crash rail on a wall face (a thin strip x1..y2 against it), the way the labs north of here carry theirs.
void dgCrash(Map& m,float x1,float y1,float x2,float y2,float f){wall(m,x1,y1,x2,y2,f+.86f,f+.96f,20);}
// A slatted bench along x, its back to the north (toward -y) or the south.
void dgBench(Map& m,float x,float y,float f,bool backNorth,float half=1.35f){
 wall(m,x-half,y-.25f,x+half,y+.25f,f+.43f,f+.51f,10);
 if(backNorth)wall(m,x-half,y-.28f,x+half,y-.20f,f+.55f,f+1.05f,10);else wall(m,x-half,y+.20f,x+half,y+.28f,f+.55f,f+1.05f,10);
 for(float dx:{-half+.35f,half-.35f})wall(m,x+dx-.06f,y-.20f,x+dx+.06f,y+.20f,f,f+.43f,2);
}
// ENERGY DIAGNOSTICS / LAB 4 (manifest 53). Where the cells were read against the
// deep face. At 01:58:04 every cell on her bench, the cell store and the Level 9
// face sensor moved on one trace. The I.I. detail came through Chemistry at 02:20
// to box her desk and was sealed in by the vent at 02:22; the arrays have logged on
// no feed since the circuits were isolated. Lab 4 itself is lit by what was left on:
// her desk lamp, the cut cell's violet, the hand lamp of R. Hale, the orderly who
// came back for her box at 03:24 and lies by the Ward C service door with the
// medical override disk on him.
// Route: the N door (or 84's airlock into the hall's west aisle) -> the vestibule ->
// the array hall, past the detail -> Lab 4 (doorway, hiding slot, or the NE office's
// cable pass) -> the disk -> the service door -> the corridor E to Ward C. S: the
// passage to the Test Complex, sealed from the Resonance Laboratory's side.
std::shared_ptr<const Map> makeEnergyDiagnosticsLab4(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[83];m.definition=CampaignChunks[83];m.definition.ambient=.23f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi*.5f;
 // Chemistry's teal-banded paint carried through; Lab 4 itself in white tile, Array Power plant-finished
 // (cast concrete over the painted band), her office in green-banded paint.
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 m.definition.haze={.11f,.11f,.12f,.005f};
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});                                                      // door 0: in from Chemistry
 openEdge(m,{'E',15.f,19.f,f,false,0});                                                     // the service corridor to Ward C
 openEdge(m,{'W',9.f,14.f,f,false,0});                                                      // Field Measurement's airlock
 openEdge(m,{'S',10.f,14.f,f,false,0});                                                     // the Test Complex passage
 machineryClock(m,"anl_diag_clock",60000);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,.9f,10.f,1.f},{14.f,.9f,23.f,1.f},{.9f,1.f,1.f,9.f},{.9f,14.f,1.f,23.f},{22.9f,1.f,23.f,15.f},{22.9f,19.f,23.f,23.f},{1.f,22.9f,10.f,23.f},{14.f,22.9f,23.f,23.f}})
  wall(m,x1,y1,x2,y2,f,roof,3);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,1.f,9.f,4.8f},{9.2f,1.f,14.8f,4.8f},{15.f,1.f,22.9f,4.8f},{1.f,5.f,15.f,14.8f},{15.2f,5.f,22.9f,14.8f},{1.f,15.f,9.8f,22.9f},{10.f,15.f,14.f,23.f},{14.2f,15.f,23.f,19.f}})
  ceilingGrid(m,x1,y1,x2,y2,roof);
 // --- The vestibule: in from Chemistry, a doorway either side, the glass onto the hall ----------------
 wall(m,9.f,1.f,9.2f,3.2f,f,roof,3);doorwayY(m,3.2f,4.4f,9.f,9.2f,f,roof);wall(m,9.f,4.4f,9.2f,5.f,f,roof,3);
 wall(m,14.8f,1.f,15.f,2.6f,f,roof,3);doorwayY(m,2.6f,3.8f,14.8f,15.f,f,roof);wall(m,14.8f,3.8f,15.f,5.f,f,roof,3);
 glazing(m,9.2f,4.8f,11.4f,5.f,f,roof,3,.95f,2.4f);wall(m,11.4f,4.8f,12.6f,5.f,f+2.4f,roof,3);glazing(m,12.6f,4.8f,14.8f,5.f,f,roof,3,.95f,2.4f);
 for(float x:{11.34f,12.6f})wall(m,x,4.76f,x+.06f,5.04f,f,f+2.4f,20);
 for(int i=0;i<2;++i)anFix(m,model::psx_locker,9.525f,1.5f+i*.86f,f,.64f,.85f,2.42f,0,true);   // clean painted lockers: rust stays below
 wall(m,9.2f,1.05f,10.08f,2.8f,f+2.42f,roof,30);
 m.lights.push_back(inTube(12.f,2.8f,roof,.4f,6.f,4600,0,.15f));
 taped(m,{14.79f,1.8f},f+1.45f,anW,Readable::Note,"DIAGNOSTICS / NIGHT ORDER","LAB 4 CIRCUITS ISOLATED AT THE PANEL 02:27 BY ORDER OF I.I. THE ARRAYS RESTARTED 02:28 ON NO FEED. LEAVE THEM.",
  "NOBODY INTO LAB 4 WITH A TORCH ON. NOBODY INTO LAB 4.");
 m.pickupSpawns.push_back({{13.9f,1.6f},PickupKind::HandgunAmmo,f});
 // the detail's radio, the moment the player is in: the Investigator wants the disk left where it is
 dgSequence(m,"anl_diag_radio","anl_diag_radio_done",22000,21000,9.2f,1.f,14.8f,4.8f);
 // A carrier's static under the whole exchange, the handset keyed at its ends; the lines are captions on it.
 {const Vec2 w{6.9f,1.45f};const float z=f+.8f;
  dgSay(m,"anl_diag_radio",400,Sound::RadioSquelch,w,z,"",.5f);
  dgSay(m,"anl_diag_radio",600,Sound::AmbRadioStatic,w,z,"",.3f);dgSay(m,"anl_diag_radio",11000,Sound::AmbRadioStatic,w,z,"",.3f);
  dgSay(m,"anl_diag_radio",800,Sound::AmbRadioStatic,w,z,"INVESTIGATOR (RADIO): Diagnostics detail. Report.",0);
  dgSay(m,"anl_diag_radio",3800,Sound::AmbRadioStatic,w,z,"RADIO: Her drives are boxed. Paper's bagged. Nobody's been in the hall.",0);
  dgSay(m,"anl_diag_radio",8000,Sound::AmbRadioStatic,w,z,"INVESTIGATOR (RADIO): And the orderly's disk.",0);
  dgSay(m,"anl_diag_radio",11200,Sound::AmbRadioStatic,w,z,"RADIO: Hale's on the floor in Lab 4 by the service door. Nobody's going in after it.",0);
  dgSay(m,"anl_diag_radio",17000,Sound::AmbRadioStatic,w,z,"INVESTIGATOR (RADIO): Then the disk stays with him. Hold the hall.",0);
  dgSay(m,"anl_diag_radio",20200,Sound::RadioSquelch,w,z,"RADIO: ...Copy.",.35f);}
 // --- The NW office: the detail's packing room, her drives and papers boxed for I.I. ----------------
 wallDoor(m,1.f,9.f,4.8f,5.f,6.6f,7.8f,f,roof,1);
 workstation(m,2.4f,2.35f,f,1,0);
 for(auto [x,y,yaw]:{std::tuple{3.6f,3.7f,.1f},{4.5f,3.85f,-.2f}})kit::prop(m,model::prop_cardboard_box_open,x,y,f,.6f,.6f,.6f,yaw);   // boxed for I.I.
 for(int i=0;i<4;++i)propPose(m,model::office_pc_tower,5.6f+(i%2)*.75f,3.2f+(i/2)*.75f,f,.26f,.61f,.61f,i*.4f,0,kPi*.5f);   // her drives, pulled and laid down
 for(int i=0;i<5;++i)kit::prop(m,model::office_file_binder,7.9f+i*.13f,4.5f,f,.093f,.301f,.424f,(i%2)*.04f);
 // the packing bench under the one lamp the detail left on: still light, safe light
 anModel(m,model::lab_clean_bench,6.6f,1.4f,f,anS,true);wall(m,6.f,1.1f,7.2f,1.7f,f+1.45f,roof,30);
 const int packLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,6.2f,1.35f,f+.81f,.18f,.4f,.52f,kPi);
 m.lights.push_back(lamp({6.2f,1.5f},f+1.26f,.45f,4.f,3000,0,WorldLightMount::Bare));m.lights.back().prop=packLamp;
 paperDrift(m,1.2f,1.2f,8.8f,4.6f,f,12,8301);
 kit::prop(m,model::item_walkie,6.9f,1.45f,f+.81f,.11f,.38f,.05f,.4f);
 dgPaper(m,{2.65f,2.2f},.75f,kPi,Readable::Letter,"I.I. / RECOVERY ORDER / 02:20","DIAGNOSTICS DETAIL: HOLD ENERGY DIAGNOSTICS. LAB 4 STATION DRIVES, HER PAPERS, ALL PRINTED READINGS, BOXED FOR I.I.",
  "THE ANALYST GOES TO MEDICAL WITH THE ESCORT. YOU DO NOT SPEAK TO HER.");
 m.pickupSpawns.push_back({{1.6f,3.6f},PickupKind::Ammo,f});
 // --- The NE office: the analysts' copier and lockers; its south wall is Lab 4's, with the cable pass --
 anModel(m,model::lab_photocopier,16.2f,1.5f,f,anS,true);wall(m,15.75f,1.1f,16.65f,1.9f,f+1.45f,roof,30);
 dgPaper(m,{16.2f,1.5f},.97f,kPi,Readable::Letter,"ARRAY LOG / CONTINUOUS","01:58:04 ARRAYS 1-6 OFF SCALE. 02:20 FEEDS PULLED. 02:21 STILL OFF SCALE.",
  "02:27 MAINS ISOLATED. 02:28 STILL LOGGING. THE PAPER COMES OUT WARM.");
 for(float x:{21.3f,22.16f})anFix(m,model::psx_locker,x,1.325f,f,.64f,.85f,2.42f,-kPi*.5f,true);
 wall(m,20.85f,1.f,22.6f,1.88f,f+2.42f,roof,30);
 workstation(m,18.6f,2.35f,f,1,1);
 m.lights.push_back(inTube(19.f,2.8f,roof,.4f,5.f,4200,0));
 m.pickupSpawns.push_back({{18.4f,3.8f},PickupKind::Health,f});
 wall(m,15.f,4.8f,21.4f,5.f,f,roof,3);wall(m,21.4f,4.8f,22.4f,5.f,f+.7f,roof,3);wall(m,22.4f,4.8f,22.9f,5.f,f,roof,3);   // the cable pass: prone only
 anFix(m,model::pwr_vent_grille,21.75f,4.45f,f,.82f,.04f,.8f,anS+.35f,false);                                           // its grille, off and leant aside
 // --- The array hall: circuits isolated, no ceiling light; the arrays log on nothing -----------------
 for(float x:{6.2f,7.45f,8.7f,9.95f}){anModel(m,model::anl_switch_cabinet_1,x,8.27f,f,anN,true);anModel(m,model::anl_switch_cabinet_1,x,10.53f,f,anS,true);}
 wall(m,5.6f,7.94f,10.55f,8.6f,f+2.91f,roof,30);wall(m,5.6f,10.2f,10.55f,10.86f,f+2.91f,roof,30);
 wall(m,10.55f,8.6f,11.f,10.2f,f,f+2.6f,2);wall(m,10.55f,8.6f,11.f,10.2f,f+2.6f,roof,30);   // the end cap: the slot between the rows opens west
 floorLine(m,5.6f,9.3f,10.55f,9.5f,f,8);
 cableTray(m,{1.2f,7.f},{14.8f,7.f},f+3.1f);
 for(float x:{7.45f,9.95f})anModel(m,model::lab_status_display,x,7.92f,f+1.6f,anN,false);
 // 02:20, feeds pulled: three of the arrays' feeders hang out of the tray where the detail dropped them, and
 // what the recorders have written since lies along the cabinets' feet
 for(float x:{6.55f,8.4f,10.35f})looseLead(m,{x,7.88f,f+2.88f},{x+.3f,7.3f,f},3.3f,7,false,.016f);
 for(auto [x,y,yaw]:{std::tuple{5.9f,7.5f,.3f},{6.9f,7.25f,1.9f},{7.7f,7.6f,2.6f},{8.9f,7.2f,.9f},{9.8f,7.55f,4.1f},{10.6f,7.3f,5.2f},{6.4f,11.2f,.7f},{8.1f,11.35f,2.2f},{9.7f,11.15f,3.9f}})
  kit::prop(m,model::office_paper_sheet,x,y,f,.4f,.293f,.004f,yaw);
 // --- The hall's west third: the array watch. Somebody sat here and wrote down what no feed was sending;
 // the screens are still drawing it, and the chart has run off the desk and across the floor.
 {const float b=f-m.layers.front().elevation;
  m.fixtures.push_back({24,{1.42f,7.3f},b,1.4f,.7f,.75f,anE,true});m.fixtures.push_back({26,{2.22f,7.2f},b,.55f,.55f,.95f,anE+.35f,true});}
 kit::prop(m,model::crt_monitor,1.3f,7.05f,f+.75f,.42f,.4f,.38f,anE);kit::prop(m,model::keyboard,1.62f,7.05f,f+.75f,.45f,.16f,.03f,anE);
 kit::prop(m,model::lab_scope_monitor,1.3f,7.72f,f+.75f,.468f,.345f,.446f,anE);
 kit::prop(m,model::mug,1.62f,6.72f,f+.75f,.09f,.09f,.1f,.8f);
 wall(m,1.76f,7.62f,1.775f,7.84f,f+.02f,f+.75f,21);m.structures.back().solid=false;wall(m,1.76f,7.62f,2.55f,7.84f,f+.004f,f+.008f,21);   // the chart, down the desk's front and out over the floor
 {auto l=lamp({1.7f,7.3f},f+1.1f,.42f,3.4f,7500,-.1f,WorldLightMount::Bare,0,false);l.flicker=.08f;m.lights.push_back(l);}                // the watch desk's screens
 for(float y:{6.95f,7.65f})anModel(m,model::lab_status_display,1.04f,y,f+1.75f,anE,false);
 anModel(m,model::lab_wall_clock,1.016f,8.5f,f+2.2f,anE,false);
 anModel(m,model::pwr_control_cabinet,1.2f,5.66f,f,anE,true);wall(m,1.f,5.24f,1.42f,6.06f,f+1.6f,roof,30);
 for(auto [x,y,yaw]:{std::tuple{2.2f,6.3f,.5f},{2.35f,8.2f,2.4f},{1.95f,8.75f,4.f}})kit::prop(m,model::office_paper_sheet,x,y,f,.4f,.293f,.004f,yaw);
 extinguisher(m,1.f,14.4f,f,anE);
 // --- The hall's east third: where the detail has stood since 02:22. A table pulled under the vestibule glass,
 // one lantern set down and left alone (still light, the kind they have learned is safe), their hand lamps
 // switched off beside it; the burst main's water roped off behind them.
 anFix(m,model::wooden_table,14.05f,5.5f,f,1.367f,.724f,.78f,0,true);
 {const int lantern=int(m.clutterSpawns.size());kit::prop(m,model::mine_cage_lamp,13.6f,5.42f,f+.78f,.21f,.2f,.36f,.3f);
  auto l=lamp({13.6f,5.5f},f+1.05f,.5f,5.f,3200,0,WorldLightMount::Bare);l.prop=lantern;m.lights.push_back(l);}
 propPose(m,model::mine_cage_lamp,14.15f,5.4f,f+.78f,.21f,.2f,.36f,1.2f,kPi*.5f,0);propPose(m,model::mine_cage_lamp,14.45f,5.62f,f+.78f,.21f,.2f,.36f,-.4f,kPi*.5f,0);   // theirs, off
 kit::prop(m,model::item_walkie,13.95f,5.7f,f+.78f,.114f,.376f,.046f,kPi*.5f);
 for(auto [x,y]:{std::pair{13.5f,5.7f},{14.5f,5.36f}})kit::prop(m,model::mug,x,y,f+.78f,.09f,.09f,.1f,x);
 kit::prop(m,model::wooden_chair,14.3f,6.25f,f,.417f,.525f,.95f,.2f);kit::prop(m,model::wooden_chair,10.7f,5.6f,f,.417f,.525f,.95f,.3f);
 kit::prop(m,model::prop_cardboard_box,12.95f,5.45f,f,.5f,.495f,.252f,.2f);
 m.clutterSpawns.push_back({1,{13.f,5.45f},f+.252f,.4f});m.clutterSpawns.push_back({2,{14.6f,5.9f},f,1.1f});
 crewBarrier(m,12.9f,11.2f,f,2.2f);
 taped(m,{12.9f,11.2f},f+.76f,anN,Readable::Note,"MAIN BURST / 01:58","THE SPRINKLER MAIN LET GO THE SECOND THE ARRAYS WENT OFF SCALE. THE WATER HAS NOT DRAINED AND HAS NOT GONE STILL.",
  "IT HUMS. TWO OF US HAVE BURNS FROM STANDING IN IT. GO ROUND.");
 floorLine(m,11.2f,11.2f,14.8f,11.32f,f,13);floorLine(m,11.2f,11.32f,11.32f,14.8f,f,13);
 m.pipes.push_back({{13.2f,14.55f},{13.2f,14.55f},roof-.95f,.06f,roof-.35f,4});flange(m,{13.2f,14.55f,roof-.93f},{},.06f);                 // the stub the main let go at
 dgAmbient(m,Sound::AmbWaterDrip,{13.2f,13.6f},-999,.5f,9,AmbientEmitter::Loop);
 // the arrays' faces in the slot between the rows: their standby lamps, cold, the only light that moves in here
 for(float x:{6.8f,9.3f}){auto l=lamp({x,9.4f},f+2.3f,.42f,4.f,7200,0,WorldLightMount::Bare,0,false);l.flicker=.06f;m.lights.push_back(l);}
 // the hall's frame: two steel portal beams across it on wall piers, a spine beam down the array rows,
 // so the low concrete box reads as a structure with bays
 for(float x:{3.f,7.6f}){wall(m,x-.14f,5.f,x+.14f,14.8f,roof-.26f,roof,12);wall(m,x-.2f,5.f,x+.2f,5.32f,f,roof,15);wall(m,x-.2f,14.48f,x+.2f,14.8f,f,roof,15);
  floorLine(m,x-.22f,5.32f,x+.22f,5.42f,f,13);}
 wall(m,1.f,9.28f,15.f,9.52f,roof-.28f,roof,12);
 // the burst main and what it left: still, wet metal, humming
 servicePipe(m,{1.f,14.55f},{15.f,14.55f},roof-.35f,.06f,4);
 m.waterVolumes.push_back({11.6f,11.6f,14.8f,14.8f,f-.14f,f-.03f,.8f});
 for(auto [x1,y1,x2,y2]:{std::tuple{11.35f,11.35f,14.8f,11.6f},{11.35f,11.6f,11.6f,14.8f}})wall(m,x1,y1,x2,y2,f+.001f,f+.003f,8);
 hazard(m,Hazard::Kind::Anomaly,11.8f,11.8f,14.8f,14.8f,f-.2f,f+2.2f,3.f);
 anHum(m,"anl_diag_hum",11.2f,11.2f,15.f,14.8f,f,f+2.5f,.5f);
 m.lights.push_back(lamp({1.02f,6.2f},f+2.4f,.18f,5.f,1000,-.5f,WorldLightMount::Wall,0,true,{1.f,.12f,.06f}));
 {auto l=lamp({13.2f,13.2f},f+.3f,.3f,3.f,11000,-.05f,WorldLightMount::Bare,0,false,{.6f,.75f,1.f});l.flicker=.3f;m.lights.push_back(l);}   // the water's glint
 // the battery packs on the hall's north wall, the only light the isolation left it: cold, low, along the aisle to Lab 4
 for(float x:{3.6f,8.2f})m.lights.push_back(lamp({x,5.02f},f+2.55f,.48f,7.f,6800,0,WorldLightMount::Wall,kPi*.5f,true));
 for(float x:{3.6f,8.2f})wall(m,x-.22f,5.f,x+.22f,5.09f,f+2.5f,f+2.62f,20);
 for(float x:{3.6f,8.4f}){m.lights.push_back(lamp({x,14.78f},f+2.55f,.42f,7.f,6800,0,WorldLightMount::Wall,-kPi*.5f,true));wall(m,x-.22f,14.71f,x+.22f,14.8f,f+2.5f,f+2.62f,20);}
 // its south wall: Array Power's door, the passage mouth
 wall(m,1.f,14.8f,5.4f,15.f,f,roof,3);swingDoor(m,5.4f,6.6f,14.9f,f,roof,1);wall(m,6.6f,14.8f,10.f,15.f,f,roof,3);
 wall(m,10.f,14.8f,14.f,15.f,f+2.6f,roof,3);wall(m,14.f,14.8f,15.f,15.f,f,roof,3);
 // the Diagnostics detail: two men sealed in since 02:22, no torches, standing in still light
 dgGuard(m,"anl_diag_clock","anl_diag_a",ActorVisual::Cleanup,GuardSquadRole::Assault,"anl_diag_detail",{{{4.f,3.4f},f,16000,-kPi*.5f},{{7.2f,5.8f},f,5000,0}},9,1,1.f,100);
 dgGuard(m,"anl_diag_clock","anl_diag_b",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"anl_diag_detail",
  {{{3.2f,6.4f},f,3000,0},{{13.4f,6.4f},f,4000,kPi*.5f},{{13.2f,10.6f},f,2500,kPi},{{3.2f,12.2f},f,3500,kPi}},11,2,1.f,90);
 m.actorTracks.back().ai.dropsSidearm=true;
 // --- Lab 4: her room -----------------------------------------------------------------------------
 wall(m,15.f,5.f,15.2f,6.f,f,roof,3);doorwayY(m,6.f,7.2f,15.f,15.2f,f,roof);wall(m,15.f,7.2f,15.2f,7.6f,f,roof,3);
 glazing(m,15.f,7.6f,15.2f,14.4f,f,roof,3,.95f,2.4f,true);wall(m,15.f,14.4f,15.2f,14.8f,f,roof,3);
 anModel(m,model::lab_id_scanner,14.97f,7.4f,f+1.2f,anW,false);
 taped(m,{14.99f,8.2f},f+.6f,anW,Readable::Note,"DOOR READER / LAB 4","02:24 OUT: LAB 4, ESCORTED. 02:52 IN: LAB 4, R. HALE. 02:55 OUT: LAB 4, R. HALE.","03:24 IN: R. HALE. NO OUT.");
 wall(m,15.f,14.8f,19.4f,15.f,f,roof,3);
 {const int d=swingDoor(m,19.4f,20.6f,14.9f,f,roof,1);m.doors[size_t(d)].requireState=stateId("med_override");}
 wall(m,20.6f,14.8f,22.9f,15.f,f,roof,3);
 titleCard(m,"LAB 4",15.2f,5.f,22.9f,14.8f);
 // Lab 4 in white tile: her room is the lab, not the corridor.
 wall(m,15.2f,5.f,20.6f,5.005f,f,roof,27);wall(m,22.895f,9.8f,22.9f,14.8f,f,roof,27);
 // her office: a glazed box in Lab 4's north-east corner, square on the line from the array hall's north
 // aisle through Lab 4's doorway, so the hall sees it lit from the moment it is entered. Its tubes are on
 // the security circuit and were never isolated: the brightest room on the floor, and the most crowded.
 glazing(m,20.6f,5.f,20.8f,7.3f,f,roof,3,.9f,2.3f,true);doorwayY(m,7.3f,8.3f,20.6f,20.8f,f,roof);glazing(m,20.6f,8.3f,20.8f,9.6f,f,roof,3,.9f,2.3f,true);
 wall(m,20.6f,9.6f,22.9f,9.8f,f,roof,3);
 wall(m,22.84f,5.f,22.9f,9.6f,f,f+2.5f,37);wall(m,20.8f,5.f,22.84f,9.6f,f,f+.006f,23);            // its painted back, her carpet
 m.lights.push_back(inTube(21.75f,6.4f,roof,.95f,6.5f,3700,0));m.lights.push_back(inTube(21.75f,8.6f,roof,.8f,6.f,3700,0,.04f));
 // the second desk: the array screens she watched, still drawing the 01:58 trace
 {const float b=f-m.layers.front().elevation;m.fixtures.push_back({24,{22.4f,8.85f},b,1.4f,.7f,.75f,anW,true});m.fixtures.push_back({26,{21.6f,8.95f},b,.55f,.55f,.95f,anW-.4f,true});}
 for(float y:{8.5f,9.2f})kit::prop(m,model::crt_monitor,22.55f,y,f+.75f,.42f,.4f,.38f,anW);
 kit::prop(m,model::keyboard,22.2f,8.85f,f+.75f,.45f,.16f,.03f,anW);
 for(float y:{8.55f,9.15f})anModel(m,model::lab_status_display,22.86f,y,f+1.75f,anW,false);
 m.lights.push_back(lamp({22.2f,8.85f},f+1.05f,.45f,2.8f,7500,-.1f,WorldLightMount::Bare,0,false));m.lights.back().flicker=.08f;   // the screens
 // the clutter: printouts in drifts, binders she never filed, her drives' empty sleds, coffee gone cold
 kit::prop(m,model::mug,22.16f,9.43f,f+.75f,.12f,.12f,.1f,1.f);
 wall(m,21.f,9.26f,22.8f,9.6f,f+1.48f,f+1.51f,20);
 for(int i=0;i<12;++i){if(i==3||i==7)continue;kit::prop(m,model::office_file_binder,21.1f+i*.14f,9.43f,f+1.51f,.093f,.301f,.424f,(i%3-1)*.05f);}
 {const float b=f-m.layers.front().elevation;
  m.fixtures.push_back({24,{22.4f,6.55f},b,1.4f,.7f,.75f,anW,true});
  m.fixtures.push_back({26,{21.62f,6.7f},b,.55f,.55f,.95f,anW+.2f,true});}
 kit::prop(m,model::crt_monitor,22.55f,6.45f,f+.75f,.42f,.4f,.38f,anW);kit::prop(m,model::keyboard,22.22f,6.45f,f+.75f,.45f,.16f,.03f,anW);
 terminal(m,{22.4f,7.12f},"LAB 4 / DIRECT LINE: SECURITY DESK 2","7 DRAFTS NOT SENT. 1 QUEUED 02:19: CARRIER LOST.","E: OPEN THE DRAFTS.",0,stateId("anl_l4_drafts"));
 kit::prop(m,model::mug,22.2f,6.1f,f+.75f,.12f,.12f,.1f,0);
 const int deskLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,22.55f,5.98f,f+.75f,.18f,.4f,.52f,anW);
 m.lights.push_back(lamp({22.45f,6.f},f+1.2f,.6f,3.6f,3000,0,WorldLightMount::Bare));m.lights.back().prop=deskLamp;
 {auto l=lamp({22.45f,6.f},f+1.22f,.7f,4.5f,2900,0,WorldLightMount::Bare);l.prop=deskLamp;l.enabledState=stateId("anl_l4_gutter");l.period=.16f;l.onTime=.09f;m.lights.push_back(l);}   // the gutter, after her last draft
 anFix(m,model::pendant_lamp,21.95f,6.5f,f+2.1f,.5f,.5f,.34f,0,false);m.pipes.push_back({{21.95f,6.5f},{21.95f,6.5f},f+2.44f,.016f,f+2.5f,6});
 m.lights.push_back(lamp({21.95f,6.5f},f+2.f,.42f,4.5f,3100,0,WorldLightMount::Bare));            // the lamp over her desk, left on
 anModel(m,model::lab_notice_board,22.86f,6.5f,f+1.25f,anW,false);
 anModel(m,model::art_painting_5,22.87f,5.55f,f+1.45f,anW,false,.5f);
 kit::prop(m,model::prop_cardboard_box_open,21.45f,5.5f,f,.6f,.6f,.6f,.25f);                    // her box: she asked to fetch it
 kit::prop(m,model::item_dosimeter,21.6f,7.6f,f,.31f,.146f,.066f,.6f);
 // her drafts: the prologue's chat from this side, then the session ends
 dgSequence(m,"anl_l4_drafts","anl_l4_gutter",48000,39500);
 // The tube's own static under them while the drafts scroll; her lamp starts to buzz before the last, and the
 // last has the one note the room allows itself.
 {const Vec2 d{19.2f,5.6f};const float z=f+1.f;const char* t="anl_l4_drafts";
  for(int at:{0,10000,20000,30000})dgSay(m,t,at,Sound::AmbStatic,{22.5f,6.45f},f+1.f,"",.14f);
  dgSay(m,t,100,Sound::AmbStatic,d,z,"SECURITY DESK 2 / 7 DRAFTS NOT SENT / 1 QUEUED",0);
  dgSay(m,t,2500,Sound::AmbStatic,d,z,"LAB 4 (02:03, NOT SENT): the lift. tell me you made the lift.",0);
  dgSay(m,t,7000,Sound::AmbStatic,d,z,"LAB 4 (02:06, NOT SENT): every cell on my bench moved at 01:58. the face sensor on 9 moved with them. same instant.",0);
  dgSay(m,t,12000,Sound::AmbStatic,d,z,"LAB 4 (02:08, NOT SENT): nothing travels that fast. it isn't a signal. it's one thing down there, and we cut pieces off it.",0);
  dgSay(m,t,17500,Sound::AmbStatic,d,z,"LAB 4 (02:11, NOT SENT): i put my hand on the array to steady it. it read me like a cell.",0);
  dgSay(m,t,23000,Sound::AmbStatic,d,z,"LAB 4 (02:16, NOT SENT): they reprinted my band. red. nobody will say what red means. i feel fine. that's the part i don't like.",0);
  dgSay(m,t,28500,Sound::AmbStatic,d,z,"LAB 4 (02:19, QUEUED): if you're alive don't come down to the labs. go up.",0);
  dgSay(m,t,33500,Sound::AmbStatic,d,z,"LAB 4 (02:22, NOT SENT): field measurement's dog is still in my pen. it has never looked at me once. it watches my lamps.",0);
  dgSay(m,t,37500,Sound::AmbLightBuzz,{22.45f,6.f},f+1.2f,"",.4f);
  dgSay(m,t,39000,Sound::StingPiano,d,z,"LAB 4 (02:55, NOT SENT): hale's giving me two minutes for my things. if you come down anyway, keep your light still. it goes to the light, not to you. i'm sorry about the lift",.3f);
  dgSay(m,t,45500,Sound::AmbStatic,d,z,"SESSION ENDED 02:56 / LOGGED OUT BY I.I.",0);}
 // the drive rack, its drives pulled onto the floor; her printout on the board
 anModel(m,model::anl_switch_cabinet_2,19.3f,5.43f,f,anS,true);wall(m,18.5f,5.f,20.1f,5.86f,f+2.7f,roof,30);
 for(auto [x,y,yaw]:{std::tuple{18.7f,6.35f,.3f},{19.55f,6.5f,-.2f},{19.9f,7.f,1.2f}})propPose(m,model::office_pc_tower,x,y,f,.26f,.61f,.61f,yaw,0,kPi*.5f);
 anModel(m,model::lab_notice_board,22.86f,10.55f,f+1.3f,anW,false);
 taped(m,{22.89f,10.4f},f+1.45f,anW,Readable::Note,"ARRAY / 01:58:04 / PRINTOUT","DEEP FACE SENSOR (LEVEL 9). CELL BENCH, 14 CELLS. CELL STORE, INSTRUMENTATION. ONE TRACE.",
  "zero delay over 900 m. nothing travelled. it was already here.");
 // the cut cell: refined ore, opened, and what it was cut from looking back
 anModel(m,model::lab_clean_bench,22.3f,11.6f,f,anW,true);
 kit::prop(m,model::lab_microscope,22.3f,11.2f,f+.81f,.18f,.22f,.29f,anW);
 kit::prop(m,model::anl_cell_core,22.32f,11.62f,f+.81f,.2f,.2f,.3f,0);
 kit::prop(m,model::lab_anomaly_relic,22.3f,11.98f,f+.81f,.125f,.1f,.1f,.6f);
 kit::prop(m,model::med_specimen_jar,22.42f,11.4f,f+.81f,.146f,.146f,.245f,0);
 m.lights.push_back(lamp({22.2f,11.6f},f+1.15f,.5f,4.5f,9000,-.4f,WorldLightMount::Bare,0,false,{.62f,.35f,1.f}));   // the cut cell: the violet the hall sees through the glass
 dgPaper(m,{22.18f,11.85f},.81f,anW,Readable::Letter,"ARRAY / CONTACT READ / 02:10","PALM CONTACT, ANALYST'S OWN HAND: 0.11 CELL EQ. TIER 2 STOPS AT 0.05.",
  "MIRRORED TO I.I. AS TAKEN. 02:15 REPLY: SCREENING. BAND RED. NO NAME GIVEN.");
 taped(m,{22.89f,11.6f},f+1.4f,anW,Readable::Sticky,"ON THE CUT CELL","same.","");
 // The pen in the south-west corner. R-47 was lent up from Field Measurement at 01:50, and it is not in it now:
 // the gate thrown open off its hinge, three roof bars sprung upward at the back, the extract duct over them
 // torn open and its grille on the pen floor, the tether parted and hanging out of the duct's mouth.
 // CREATURE, NOT BUILT IN THIS PASS: the Pointer R-47 (docs/Design/lab4-field-measurement-plan-20261104.md,
 // sections 3 and 8) belongs here, spawned on the floor at (18.4,12.4) between the pen and the disk. When the
 // engine has it, add its CreatureSpawn at this line; the duct mouth, the tether and the sounds keyed on
 // anl_l4_rig further down (the thing in the ceiling) are what stands in for it until then.
 anFix(m,model::med_cell_bars,16.65f,12.1f,f,2.5f,.04f,2.4f,0,false);wall(m,15.4f,12.07f,17.9f,12.13f,f,f+2.4f,30);
 anFix(m,model::med_cell_bars,18.91f,11.38f,f+.02f,2.5f,.04f,2.4f,.62f,false);                                           // the gate, thrown open off its hinge
 // its roof: bars, not a lid. Rods front to back on three cross members; the three over the back corner end
 // short, bent up toward the duct.
 for(int i=0;i<11;++i){const float x=15.3f+i*.24f;const bool sprung=i>=4&&i<=6;
  wall(m,x,12.1f,x+.03f,sprung?13.f:14.8f,f+2.4f,f+2.43f,8);
  if(sprung)m.pipes.push_back({{x+.015f,12.92f},{i==6?17.15f:16.16f,i==5?14.5f:14.32f},f+2.415f,.015f,roof-(i==5?.45f:.62f),6});}
 wall(m,15.2f,12.1f,17.9f,12.14f,f+2.43f,f+2.47f,8);wall(m,15.2f,14.76f,17.9f,14.8f,f+2.43f,f+2.47f,8);
 wall(m,15.2f,13.4f,16.2f,13.44f,f+2.43f,f+2.47f,8);wall(m,17.1f,13.4f,17.9f,13.44f,f+2.43f,f+2.47f,8);
 // nobody on its roof; the shaft under the duct mouth left clear for the tether
 wall(m,15.2f,12.05f,17.95f,12.95f,f+2.47f,roof,30);wall(m,15.2f,12.95f,16.2f,14.8f,f+2.47f,roof,30);wall(m,17.1f,12.95f,17.95f,14.8f,f+2.47f,roof,30);
 // the extract duct along the south wall, open where it comes over the pen
 wall(m,15.2f,14.25f,16.2f,14.8f,roof-.6f,roof-.12f,20);wall(m,17.1f,14.25f,18.9f,14.8f,roof-.6f,roof-.12f,20);
 wall(m,16.2f,14.25f,17.1f,14.8f,roof-.16f,roof-.12f,20);wall(m,16.2f,14.74f,17.1f,14.8f,roof-.6f,roof-.16f,8);
 for(float x:{16.2f,17.07f})wall(m,x,14.25f,x+.03f,14.8f,roof-.6f,roof-.16f,8);
 for(float x:{15.6f,18.3f})for(float y:{14.3f,14.75f})m.pipes.push_back({{x,y},{x,y},roof-.12f,.012f,roof,6});
 propPose(m,model::pwr_vent_grille,16.9f,14.f,f,.819f,.04f,.8f,.5f,kPi*.5f,0);                                           // its grille, where it landed
 chain(m,{16.27f,14.5f,roof-.4f},2.3f);                                                                                   // the tether, parted at the collar
 wall(m,16.5f,13.3f,16.8f,13.6f,f+.004f,f+.03f,8);m.pipes.push_back({{16.65f,13.45f},{16.65f,13.45f},f+.03f,.05f,f+.1f,6});   // the floor ring it was run to
 wall(m,15.3f,12.2f,17.8f,14.7f,f,f+.004f,8);kit::prop(m,model::ind_barrel,15.7f,14.3f,f,.4f);
 taped(m,{16.f,12.07f},f+1.4f,anN,Readable::Note,"PEN / R-47","ON LOAN FROM FIELD MEASUREMENT 01:50. TETHER TO THE FLOOR RING. GATE PINNED. IT DOES NOT EAT.",
  "IT HAS NEVER PULLED ON THE TETHER. NOT ONCE IN ELEVEN SURVEYS. NO LAMP IN A HAND NEAR IT. -DUNNE");
 // what it dragged: the tether's gouges out of the pen's open side and up to the rig, and from there what
 // was left of Hale's walk to the service door
 for(float o:{0.f,.09f,.2f}){wall(m,16.8f,13.38f+o,18.46f+o,13.4f+o,f+.003f,f+.006f,8);wall(m,18.44f+o,11.3f,18.46f+o,13.4f+o,f+.003f,f+.006f,8);}
 for(int i=0;i<5;++i)wall(m,18.5f+i*.22f,11.25f+i*.42f,18.5f+i*.22f+(i%2?.34f:.26f),11.25f+i*.42f+.5f,f+.004f,f+.008f,16);
 // her bench rig: four stakes on the field lamp bus, dead until Field Measurement closes it
 for(int i=0;i<4;++i)dgStake(m,{21.6f-i*1.8f,10.f},f,f+1.5f,"anl_l4_rig",4.8f,1.2f,4.8f-1.2f*i);
 terminal(m,{17.f,5.55f},"BENCH RIG / WALKING LAMPS","FOUR STAKES ON THE FIELD LAMP BUS. BUS OPENED AT FIELD MEASUREMENT 02:24.","E / WALK THE LAMPS. E AGAIN / STOP THEM.",0,stateId("anl_l4_rig"),stateId("anl_fm_bus"));
 m.terminals.back().toggleState=true;
 wall(m,16.5f,5.17f,17.45f,5.93f,f+1.5f,roof,30);
 // The rig's own result, with nothing on the floor to follow it. The bus closed at Field Measurement puts a
 // pilot on the rig's console. Walked, the lamps are answered: the cut cell sheds its motes and beats with each
 // step, the arrays in the hall strobe on the same count, and over the ceiling something heavy turns and goes
 // along the duct with the light; stopped, it sets down once, where the light stopped.
 {auto l=lamp({17.f,5.3f},f+1.42f,.22f,1.8f,5200,0,WorldLightMount::Bare,0,false,{.2f,1.f,.35f});l.enabledState=stateId("anl_fm_bus");m.lights.push_back(l);}
 wall(m,16.97f,5.f,17.03f,5.02f,f+1.36f,f+1.42f,20);
 {auto l=lamp({22.2f,11.6f},f+1.3f,.75f,5.f,9000,-.4f,WorldLightMount::Bare,0,false,{.62f,.35f,1.f});l.enabledState=stateId("anl_l4_rig");l.period=1.2f;l.onTime=.4f;m.lights.push_back(l);}
 {auto l=lamp({5.9f,9.4f},f+1.7f,.6f,5.f,7200,0,WorldLightMount::Bare,0,false);l.enabledState=stateId("anl_l4_rig");l.period=1.2f;l.onTime=.22f;m.lights.push_back(l);}
 {Hazard h;h.kind=Hazard::Kind::Anomaly;h.x1=21.9f;h.y1=11.1f;h.x2=22.75f;h.y2=12.1f;h.bottom=f+.81f;h.top=f+1.9f;h.damagePerSecond=0;h.enabledFlag=stateId("anl_l4_rig");m.hazards.push_back(h);}
 dgAmbient(m,Sound::StingScratch,{18.9f,10.f},roof-.3f,.45f,14,AmbientEmitter::OnState,0,0,"anl_l4_rig");
 dgAmbient(m,Sound::OneMetalDrag,{18.f,10.6f},roof-.2f,.5f,14,AmbientEmitter::Random,3.2f,1.6f,"anl_l4_rig");
 dgAmbient(m,Sound::OneMetalCreak,{20.4f,10.3f},roof-.2f,.42f,14,AmbientEmitter::Random,5.f,3.f,"anl_l4_rig");
 dgAmbient(m,Sound::OneMetalBang,{16.5f,10.4f},roof-.2f,.55f,16,AmbientEmitter::OnState,0,0,"anl_l4_rig",0);
 dgAmbient(m,Sound::AmbStatic,{8.f,9.4f},f+1.6f,.4f,9,AmbientEmitter::Loop,0,0,"anl_l4_rig");
 taped(m,{15.21f,7.4f},f+1.5f,anE,Readable::Sticky,"ON THE RIG","it follows light that walks and stops where the light stops.","it has never once looked at me.");
 // Hale, by the service door, his hand lamp still burning beside him; the disk on him
 dgBody(m,"anl_diag_clock",ActorVisual::CleanupMedic,{20.1f,14.1f},f,kPi*.5f);
 wall(m,19.3f,13.3f,21.f,14.7f,f+.004f,f+.01f,16);wall(m,20.7f,14.78f,21.3f,14.8f,f+.9f,f+1.5f,16);
 kit::prop(m,model::item_walkie,19.6f,13.6f,f,.11f,.38f,.05f,1.9f);
 const int haleLamp=int(m.clutterSpawns.size());propPose(m,model::mine_cage_lamp,19.3f,13.4f,f,.21f,.2f,.36f,.5f,kPi*.5f,0);
 m.lights.push_back(lamp({19.3f,13.4f},f+.25f,1.1f,5.5f,5600,0,WorldLightMount::Bare));m.lights.back().prop=haleLamp;
 dgPaper(m,{20.9f,13.8f},0,.4f,Readable::Letter,"DETAIL 4 / ORDER 03:20","R. HALE: RETURN TO LAB 4 BY THE SERVICE DOOR. CLEAR THE ANALYST'S DESK. ALL MEDIA TO I.I. BY HAND. DO NOT READ IT.",
  "THE DIAGNOSTICS DETAIL WILL NOT GO IN. YOU HAVE THE DISK.");
 dgPaper(m,{20.5f,14.4f},0,-.3f,Readable::Letter,"WARD C OVERRIDE / R. HALE","DEPTHWORKS MEDICAL. EVERY MEDICAL LOCK ANSWERS IT: THE SERVICE DOOR, THE WARD, SCREENING SOUTH.","E / TAKE THE DISK.",stateId("med_override"));
 taped(m,{19.25f,14.79f},f+1.45f,anN,Readable::Note,"ESCORT / LAB 4 / 02:55","COLLECTED FROM SCREENING 02:50. VIA LAB 4 FOR HER THINGS, AS SHE ASKED. THROUGH TO WARD C.",
  "SHE LEFT HER BOX ON THE DESK. I SAID I WOULD BRING IT DOWN. -R. HALE");
 anModel(m,model::lab_id_scanner,21.f,14.78f,f+1.2f,anN,false);
 taped(m,{21.5f,14.79f},f+1.2f,anN,Readable::Note,"WARD C SERVICE DOOR / READER","MEDICAL OVERRIDE ONLY. EVERY OPENING LOGGED TO I.I.","LAST: 03:24 IN, R. HALE.");
 wallSign(m,20.f,14.75f,f+2.62f,"WARD C / SERVICE DOOR","MEDICAL OVERRIDE ONLY",1.8f,anN);
 m.pickupSpawns.push_back({{18.9f,14.4f},PickupKind::HandgunAmmo,f});
 // --- The service corridor to Ward C: the way they brought her --------------------------------------
 wall(m,14.2f,19.f,22.9f,22.9f,f,roof,3);
 for(float y:{16.6f,17.4f})floorLine(m,14.2f,y,24.f,y+.08f,f,8);
 {auto l=inTube(19.f,17.f,roof,.4f,5.f,5000,0,0,kPi*.5f);l.enabledState=stateId("med_blackout");l.invert=true;l.shadows=false;m.lights.push_back(l);}   // level with the services: no shadow streaks off them
 m.lights.push_back(lamp({14.22f,17.f},f+2.4f,.12f,4.f,1000,-.5f,WorldLightMount::Wall,0,true,{1.f,.12f,.06f}));
 extinguisher(m,14.2f,16.2f,f,anE);
 // Analysis ends at the service door and Medical begins: crash rails, the services overhead, the trolley
 // Ward C sent up for her and nobody wheeled back, Medical's overflow parked at the blind end, and a green
 // lamp over the way through.
 dgCrash(m,15.f,15.f,19.3f,15.04f,f);dgCrash(m,20.7f,15.f,22.9f,15.04f,f);dgCrash(m,14.2f,18.96f,23.f,19.f,f);
 servicePipe(m,{14.2f,15.3f},{24.f,15.3f},roof-.3f,.06f,4);servicePipe(m,{14.2f,15.55f},{24.f,15.55f},roof-.26f,.04f,3);
 cableTray(m,{14.3f,18.6f},{23.9f,18.6f},roof-.42f);for(float x=15.2f;x<23.5f;x+=1.8f)for(float o:{-.17f,.17f})m.pipes.push_back({{x,18.6f+o},{x,18.6f+o},roof-.32f,.01f,roof,6});
 anFix(m,model::med_ward_bed,16.f,18.35f,f,1.984f,.944f,.842f,0,true);                              // the trolley, hard against the wall
 anModel(m,model::med_supply_cabinet,16.6f,15.24f,f,anS,true);wall(m,16.05f,15.f,17.15f,15.48f,f+1.7f,roof,30);
 anFix(m,model::med_wheelchair,14.75f,17.6f,f,.482f,.9f,.787f,anE+.25f,true);
 for(auto [x,y,z]:{std::tuple{14.75f,15.55f,0.f},{15.4f,15.5f,0.f},{14.75f,15.55f,.252f}})kit::prop(m,model::prop_cardboard_box,x,y,f+z,.5f,.495f,.252f,z>0?0.f:x-14.75f);
 for(float x:{18.2f,21.6f})anFix(m,model::radiator_panel,x,18.84f,f,1.23f,.24f,.91f,anN,true);
 anFix(m,model::med_notice_board,19.9f,18.965f,f+1.25f,1.2f,.07f,.9f,anN,false);
 anFix(m,model::med_wall_clock,17.6f,15.01f,f+2.05f,.24f,.02f,.23f,anS,false);
 anFix(m,model::med_floor_drain,17.6f,17.f,f,.5f,.5f,.01f,0,false);
 anFix(m,model::med_first_aid_box,22.3f,15.06f,f+1.3f,.367f,.12f,.349f,anS,false);
 kit::prop(m,model::item_id_card,21.3f,16.9f,f,.132f,.085f,.002f,.7f);                             // a band, red, where they walked her
 wall(m,22.3f,15.f,22.9f,15.08f,f+2.42f,f+2.6f,20);
 m.lights.push_back(lamp({22.6f,15.09f},f+2.5f,.34f,5.f,5200,0,WorldLightMount::Wall,kPi*.5f,true,{.2f,1.f,.35f}));   // the way through to the ward
 dgSequence(m,"anl_l4_door_radio","anl_l4_door_radio_done",15000,14000,18.5f,15.1f,21.5f,16.8f);
 {const Vec2 w{19.6f,13.6f};const float z=f+.2f;
  dgSay(m,"anl_l4_door_radio",900,Sound::RadioSquelch,w,z,"",.5f);
  dgSay(m,"anl_l4_door_radio",1100,Sound::AmbRadioStatic,w,z,"",.3f);
  dgSay(m,"anl_l4_door_radio",1300,Sound::AmbRadioStatic,w,z,"INVESTIGATOR (RADIO): Lab 4 service door. Opened on the Ward C disk.",0);
  dgSay(m,"anl_l4_door_radio",5600,Sound::AmbRadioStatic,w,z,"INVESTIGATOR (RADIO): Detail four is lost. So it is not Detail four.",0);
  dgSay(m,"anl_l4_door_radio",10200,Sound::AmbRadioStatic,w,z,"INVESTIGATOR (RADIO): Diagnostics detail. Whoever carries that disk is not one of ours. Find them.",0);
  dgSay(m,"anl_l4_door_radio",13600,Sound::RadioSquelch,w,z,"",.35f);}
 // --- The south passage to the Test Complex: the back door, sealed from the far side ------------------
 wall(m,9.8f,15.f,10.f,22.9f,f,roof,3);wall(m,14.f,15.f,14.2f,22.9f,f,roof,3);
 {Door d{10.f,14.f,19.1f,0};d.requireState=stateId("res_lab_unsealed");m.doors.push_back(d);}
 wall(m,10.f,19.f,14.f,19.2f,f+2.6f,roof,3);
 m.lights.push_back(lamp({12.f,18.9f},f+2.95f,.3f,4.f,1000,-.5f,WorldLightMount::Wall,-kPi*.5f,true,{1.f,.12f,.06f}));
 taped(m,{10.01f,18.2f},f+1.45f,anE,Readable::Note,"BACK DOOR / RESONANCE LAB","SEALED FROM THEIR SIDE AT 01:58. IT OPENS WHEN THEY OPEN THEIRS.","IF YOU ARE READING THIS FROM IN HERE, THEY HAVEN'T.");
 wallSign(m,12.f,18.94f,f+2.65f,"TEST COMPLEX PASSAGE","SEALED FROM THE RESONANCE LABORATORY SIDE",2.4f,anN);
 m.pickupSpawns.push_back({{13.4f,17.6f},PickupKind::Ammo,f});
 m.lights.push_back(inTube(12.f,21.f,roof,.3f,4.5f,5000,0,.2f,kPi*.5f));
 // --- Array Power: the breakers left open ------------------------------------------------------------
 for(float x:{2.6f,4.6f,6.6f}){anModel(m,model::anl_switch_cabinet_2,x,22.5f,f,anN,true);anModel(m,model::lab_knife_switch,x+.4f,22.06f,f+1.3f,anN,false);}
 wall(m,1.8f,22.05f,7.4f,22.9f,f+2.7f,roof,30);
 taped(m,{1.01f,18.6f},f+1.45f,anE,Readable::Note,"ARRAY POWER","MAINS ISOLATED 02:27. ARRAYS 1-6 LOGGING ON NOTHING.","BREAKERS LEFT OPEN. I AM NOT CLOSING THEM TO SEE WHAT HAPPENS.");
 {auto l=inTube(6.9f,19.2f,roof,.42f,5.5f,4000,0,.25f);m.lights.push_back(l);}   // clear of the tray, which threw its length down the wall
 // the switchroom's plant finish: cast concrete over the painted band
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,15.f,1.005f,22.9f},{1.f,22.895f,9.8f,22.9f},{9.795f,15.f,9.8f,22.9f},{1.f,15.f,5.4f,15.005f},{6.6f,15.f,9.8f,15.005f}})wall(m,x1,y1,x2,y2,f+1.1f,roof,42);
 // the switchroom round them: the rubber mat at the board, the board's feeders up into a tray and away to the
 // hall, the two transformers on their pad with the isolator between, the distribution on the west wall with
 // its lock-out board (five tags, every one of them hung tonight), the electrician's bench by the door
 wall(m,1.9f,21.1f,7.35f,22.05f,f,f+.012f,8);floorLine(m,1.9f,20.98f,7.35f,21.1f,f,13);
 for(float x:{2.6f,4.6f,6.6f})m.pipes.push_back({{x,22.4f},{x,22.4f},f+2.7f,.045f,roof-.38f,8});
 cableTray(m,{4.6f,15.1f},{4.6f,22.6f},roof-.5f);for(float y=16.f;y<22.5f;y+=1.8f)for(float o:{-.17f,.17f})m.pipes.push_back({{4.6f+o,y},{4.6f+o,y},roof-.4f,.01f,roof,6});
 for(float x:{2.6f,6.6f})m.pipes.push_back({{x,22.4f},{4.6f,22.4f},roof-.38f,.045f,roof-.38f,8});
 m.lights.push_back(lamp({4.6f,21.5f},f+1.9f,.3f,3.4f,1000,-.5f,WorldLightMount::Bare,0,false,{1.f,.12f,.06f}));   // OPEN, three times, in red
 equipmentPad(m,8.7f,16.5f,9.78f,19.7f,f);
 for(float y:{17.2f,19.f}){anFix(m,model::pwr_step_transformer,9.25f,y,f+.1f,.64f,.886f,1.673f,anW,true);wall(m,8.8f,y-.33f,9.7f,y+.33f,f+2.1f,roof,30);
  m.pipes.push_back({{9.45f,y},{9.45f,y},f+1.773f,.035f,roof,8});}
 anModel(m,model::pwr_disconnect_switch,9.73f,18.1f,f+1.2f,anW,false);m.pipes.push_back({{9.76f,18.1f},{9.76f,18.1f},f+2.05f,.025f,roof,8});
 for(float y:{15.9f,16.3f}){anFix(m,model::pwr_breaker_panel,1.051f,y,f+1.4f,.183f,.102f,.457f,anE,false);m.pipes.push_back({{1.04f,y},{1.04f,y},f+1.86f,.02f,roof,8});}
 anFix(m,model::pwr_breaker_panel_open,1.113f,16.85f,f+1.35f,.318f,.225f,.457f,anE,false);
 anFix(m,model::pwr_ammeter_box,1.051f,17.4f,f+1.45f,.229f,.102f,.366f,anE,false);
 anFix(m,model::pwr_cable_bundle,1.08f,18.f,f+2.7f,4.f,.14f,.222f,anE,false);
 wall(m,1.f,19.5f,1.03f,20.7f,f+1.15f,f+1.9f,19);
 for(int i=0;i<5;++i){const float y=19.62f+i*.22f;wall(m,1.03f,y,1.045f,y+.02f,f+1.72f,f+1.8f,8);wall(m,1.03f,y-.03f,1.04f,y+.05f,f+1.5f,f+1.7f,i==3?13:21);}
 anModel(m,model::lab_clean_bench,8.2f,15.33f,f,anS,true);wall(m,7.6f,15.f,8.8f,15.62f,f+1.45f,roof,30);
 kit::prop(m,model::lab_field_meter,7.8f,15.3f,f+.81f,.25f,.328f,.148f,.2f);kit::prop(m,model::office_paper_stack,8.16f,15.3f,f+.81f,.33f,.44f,.03f,.15f);
 kit::prop(m,model::mine_hard_hat,8.55f,15.33f,f+.81f,.279f,.351f,.153f,0);
 kit::prop(m,model::office_chair,8.f,16.2f,f,.55f,.55f,.95f,.3f);kit::prop(m,model::trash_can,9.5f,15.4f,f,.3f,.3f,.45f,.2f);
 anFix(m,model::pwr_fire_cabinet,6.9f,15.1f,f+1.1f,.252f,.203f,.559f,anS,false);
 extinguisher(m,9.8f,20.6f,f,anW);
 paperDrift(m,2.f,16.f,7.5f,20.5f,f,5,8305);
 m.pickupSpawns.push_back({{8.8f,16.f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{1.8f,21.6f},PickupKind::Health,f});
 // --- Names and air: Lab 4 a shade violet and thick; the hall dark -----------------------------------
 zone(m,"ENERGY DIAGNOSTICS / LAB 4",15.2f,5.f,22.9f,14.8f,-1000,1000,{.12f,.11f,.13f,.006f});
 zone(m,"ENERGY DIAGNOSTICS / SERVICE CORRIDOR",14.2f,15.f,24.f,19.f,-1000,1000,{.12f,.12f,.13f,.006f});
 zone(m,"ENERGY DIAGNOSTICS / TEST COMPLEX PASSAGE",10.f,15.f,14.f,24.f);
 zone(m,"ENERGY DIAGNOSTICS / ARRAY POWER",1.f,15.f,9.8f,23.f);
 zone(m,"ENERGY DIAGNOSTICS / PACKING ROOM",1.f,1.f,9.f,4.8f);
 zone(m,"ENERGY DIAGNOSTICS / ANALYSTS",15.f,1.f,22.9f,4.8f);
 zone(m,"ENERGY DIAGNOSTICS / VESTIBULE",9.2f,0.f,14.8f,4.8f);
 zone(m,"ENERGY DIAGNOSTICS / ARRAY HALL",0.f,5.f,15.f,14.8f,-1000,1000,{.1f,.11f,.12f,.005f});
 zone(m,"ENERGY DIAGNOSTICS",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// FIELD MEASUREMENT (manifest 54). Where the plant's meters were calibrated, until the
// department found its dogs read the ore better than the meters did. The calibration pan
// is a flooded basin in five metres of dark; R-46 was kept in it, R-47 lent to Lab 4 at
// 01:50. Dunne went in for his second meter at 02:10 with the torch in his teeth. The
// gallery glass held. Across the pan, Chamber 3 holds the field lamp bus that feeds Lab 4's
// bench rig. The airlock to Energy Diagnostics waits on Chemistry's purge.
// Route: N from Instrumentation -> the corridor (kit room W, techs' office E) -> the
// gallery -> (the pan door: round the dry edge to Chamber 3 and the bus) -> the airlock,
// once purged -> E into Lab 4's array hall, behind the detail.
std::shared_ptr<const Map> makeFieldMeasurement(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[84];m.definition=CampaignChunks[84];m.definition.ambient=.22f;
 m.definition.playerStart={8.f,1.6f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 m.definition.haze={.11f,.12f,.13f,.005f};
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',6.f,10.f,f,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 machineryClock(m,"anl_fm_clock",60000);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,.9f,6.f,1.f},{10.f,.9f,23.f,1.f},{.9f,1.f,1.f,23.f},{22.9f,1.f,23.f,9.f},{22.9f,14.f,23.f,23.f},{1.f,22.9f,23.f,23.f}})wall(m,x1,y1,x2,y2,f,roof,3);
 // --- The north band: the corridor in from Instrumentation, the kit room west, the techs' office east ---
 wall(m,5.8f,1.f,6.f,2.2f,f,roof,3);doorwayY(m,2.2f,3.4f,5.8f,6.f,f,roof);wall(m,5.8f,3.4f,6.f,5.f,f,roof,3);
 wall(m,10.f,1.f,10.2f,2.2f,f,roof,3);doorwayY(m,2.2f,3.4f,10.f,10.2f,f,roof);wall(m,10.f,3.4f,10.2f,5.f,f,roof,3);
 wall(m,1.f,4.8f,5.8f,5.f,f,roof,3);wall(m,10.2f,4.8f,22.9f,5.f,f,roof,3);
 wall(m,6.f,4.8f,10.f,5.f,f+2.6f,roof,3);                                                   // the corridor's mouth onto the gallery
 wall(m,6.f,0.f,10.f,4.8f,f+3.5f,roof,3);ceilingGrid(m,6.f,0.f,10.f,4.8f,f+3.5f);
 wall(m,1.f,1.f,5.8f,4.8f,f+3.f,roof,3);ceilingGrid(m,1.f,1.f,5.8f,4.8f,f+3.f);
 wall(m,10.2f,1.f,22.9f,4.8f,f+3.f,roof,3);ceilingGrid(m,10.2f,1.f,22.9f,4.8f,f+3.f);
 m.lights.push_back(inTube(8.f,2.4f,f+3.5f,.55f,6.f,4400,.02f,0,kPi*.5f));
 // the kit room: the lockers, the sign-out counter, a gap in the dosimeters that is hers
 for(int i=0;i<3;++i)anFix(m,model::psx_locker,1.6f+i*.86f,1.325f,f,.64f,.85f,2.42f,-kPi*.5f,true);
 wall(m,1.05f,1.f,3.8f,1.88f,f+2.42f,f+3.f,30);
 inCounter(m,1.4f,4.2f,5.6f,4.8f,f,.9f);
 for(float x:{1.75f,2.15f,4.7f})kit::prop(m,model::lab_field_meter,x,4.5f,f+.9f,.25f,.33f,.15f,x);
 for(float x:{5.05f,5.3f})kit::prop(m,model::item_walkie,x,4.5f,f+.9f,.11f,.38f,.05f,.1f);
 for(float x:{2.6f,2.95f,3.65f,4.f})kit::prop(m,model::item_dosimeter,x,4.62f,f+.9f,.31f,.146f,.066f,kPi*.5f);
 dgPaper(m,{3.3f,4.4f},.9f,kPi,Readable::Note,"FIELD KIT / SIGN-OUT","01:50 LAB 4: R-47 ON LOAN FOR THE BENCH TEST. 1 HAND LAMP. IN HER HAND: BACK BY THREE.",
  "02:05 DUNNE: 2 METERS, 1 RECORDER, CHASE RIG KEY. NOTHING SIGNED BACK IN.");
 anModel(m,model::lab_notice_board,1.04f,2.85f,f+1.25f,anE,false);
 taped(m,{1.01f,3.75f},f+1.45f,anE,Readable::Note,"BODY SURVEY / THIS WEEK","LAB 4 (ENERGY DIAG.): MON 0.03, WED 0.04, FRI 0.06 CELL EQ. TIER 2 STOPS AT 0.05. SCREENING DUE.",
  "PER I.I. FORMAT: ALL STAFF FIELDS IN CELL EQUIVALENTS. NAMES ON THE COPY THAT GOES UP ONLY.");
 m.lights.push_back(inTube(3.4f,2.9f,f+3.f,.5f,5.f,4200,0));
 m.pickupSpawns.push_back({{2.f,3.6f},PickupKind::Health,f});
 // the hand lamps' shelf: one slot empty (hers, out on loan with R-47), one lamp left burning on its charge;
 // the meters and hats under them, the spare stakes she asked Dunne for laid along the wall
 shelf(m,{4.75f,1.3f},0,0);wall(m,3.85f,1.05f,5.65f,1.55f,f+1.95f,f+3.f,30);
 for(float x:{4.1f,5.3f})kit::prop(m,model::mine_cage_lamp,x,1.3f,f+.972f,.21f,.2f,.36f,x);
 {const int charged=int(m.clutterSpawns.size());kit::prop(m,model::mine_cage_lamp,4.5f,1.3f,f+.972f,.21f,.2f,.36f,.4f);
  auto l=lamp({4.5f,1.72f},f+1.25f,.4f,3.6f,3000,0,WorldLightMount::Bare);l.prop=charged;m.lights.push_back(l);}
 for(float x:{4.1f,4.5f})kit::prop(m,model::lab_field_meter,x,1.3f,f+1.656f,.25f,.328f,.148f,0);
 for(float x:{4.95f,5.42f})kit::prop(m,model::mine_hard_hat,x,1.3f,f+1.656f,.279f,.351f,.153f,0);
 for(float x:{4.2f,5.2f})kit::prop(m,model::prop_cardboard_box,x,1.3f,f+.306f,.436f,.432f,.22f,x>5?.05f:0.f);
 for(auto [x,z]:{std::pair{5.6f,.03f},{5.7f,.03f},{5.65f,.078f}}){m.pipes.push_back({{x,3.5f},{x,4.15f},f+z,.025f,f+z,5});m.pipes.push_back({{x,3.5f},{x,3.56f},f+z,.04f,f+z,6});}
 kit::prop(m,model::trash_can,1.3f,3.3f,f,.3f,.3f,.45f,.3f);
 anFix(m,model::med_first_aid_box,5.74f,1.85f,f+1.3f,.367f,.12f,.349f,anW,false);
 // the techs' office: the survey mirrored up to I.I. as it was taken
 workstation(m,13.f,2.35f,f,1,3);workstation(m,16.f,2.35f,f,1,1);
 dgDeskTerminal(m,20.6f,2.35f,f,1,0,"SURVEY / MIRROR TO I.I.","EVERY READING COPIED TO INTERNAL INVESTIGATIONS AS TAKEN. 214 STAFF ON FILE.","03:02 FILE CALLED UP IN FULL BY I.I. 03:02 LOCAL COPY DELETED.");
 anModel(m,model::lab_notice_board,18.2f,1.04f,f+1.3f,anS,false);
 taped(m,{19.3f,1.01f},f+1.45f,anS,Readable::Note,"PAN MAP / 01:50","R-46 TURNS TO A LIGHT BEFORE THE METERS DO. ONLY A LIGHT THAT MOVES. THE PLANT'S OWN LAMPS IT IGNORES.",
  "WE STOPPED CALIBRATING THE METERS. WE CALIBRATE THE DOG.");
 paperDrift(m,10.6f,3.f,21.2f,4.2f,f,6,8401);
 m.lights.push_back(inTube(16.5f,2.9f,f+3.f,.5f,5.f,4200,0));
 m.pickupSpawns.push_back({{19.7f,4.3f},PickupKind::Ammo,f});
 // the survey's years on a shelf over the desks; the files, the books and the cooler along the glass side; and at
 // the dark end, under the tube that went, the chair somebody slept in between pans, a lamp left on beside it
 wall(m,11.8f,1.f,17.2f,1.38f,f+1.5f,f+1.53f,20);for(float x:{11.85f,14.5f,17.13f})wall(m,x,1.f,x+.02f,1.36f,f+1.38f,f+1.5f,8);
 wall(m,11.8f,1.f,17.2f,1.38f,f+2.f,f+3.f,30);
 for(int i=0;i<8;++i)kit::prop(m,model::office_file_binder,12.f+i*.105f,1.18f,f+1.53f,.093f,.301f,.424f,0);
 for(int i=0;i<6;++i)kit::prop(m,model::office_file_binder,15.3f+i*.105f,1.18f,f+1.53f,.093f,.301f,.424f,0);
 kit::prop(m,model::prop_cardboard_box,13.6f,1.18f,f+1.53f,0,.4f);
 anModel(m,model::lab_wall_clock,11.1f,1.016f,f+2.2f,anS,false);
 for(float x:{11.4f,12.35f})anModel(m,model::med_records_cabinet,x,4.55f,f,anN,true);
 for(float x:{11.15f,11.25f,11.35f})kit::prop(m,model::office_file_binder,x,4.55f,f+1.f,.093f,.301f,.424f,0);
 kit::prop(m,model::coffee_machine,12.4f,4.55f,f+1.f,.35f,.35f,.45f,kPi);kit::prop(m,model::mug,12.05f,4.45f,f+1.f,.09f,.09f,.1f,.5f);
 anFix(m,model::office_bookcase,14.6f,4.55f,f,2.025f,.5f,2.5f,anN,true);wall(m,13.58f,4.3f,15.62f,4.8f,f+2.5f,f+3.f,30);
 anFix(m,model::water_cooler,16.3f,4.5f,f,.35f,.35f,1.1f,anN,true);
 for(auto [x,y,z]:{std::tuple{17.4f,4.42f,0.f},{18.05f,4.45f,0.f},{17.4f,4.42f,.252f}})kit::prop(m,model::prop_cardboard_box,x,y,f+z,.5f,.495f,.252f,z>0?0.f:x-17.4f);
 anFix(m,model::office_armchair_green,22.2f,3.85f,f,1.399f,1.364f,1.192f,anW,true);
 anModel(m,model::med_records_cabinet,20.95f,4.55f,f,anN,true);
 {const int lit=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,21.2f,4.55f,f+1.f,.18f,.4f,.52f,kPi+.4f);
  auto l=lamp({21.2f,4.4f},f+1.45f,.42f,4.2f,2800,0,WorldLightMount::Bare);l.prop=lit;m.lights.push_back(l);}
 kit::prop(m,model::mug,20.7f,4.5f,f+1.f,.09f,.09f,.1f,1.3f);
 anModel(m,model::lab_notice_board,22.86f,3.85f,f+1.3f,anW,false);                                  // the pan rota over his chair
 anModel(m,model::lab_potted_plant,22.5f,1.45f,f,0,true);
 inDeadTube(m,21.f,2.9f,f+3.f);
 kit::prop(m,model::trash_can,10.6f,4.4f,f,.3f,.3f,.45f,.1f);
 // --- The gallery: the glass onto the pan, Dunne's desk facing it, the lamp test console ------------
 wall(m,1.f,5.f,22.9f,8.8f,f+3.4f,roof,3);ceilingGrid(m,1.f,5.f,22.9f,8.8f,f+3.4f);
 wall(m,1.f,8.8f,2.2f,9.f,f,roof,3);
 glazing(m,2.2f,8.8f,15.f,9.f,f,roof,3,.95f,2.4f,true);                                     // protective: it holds
 wall(m,15.f,8.8f,15.4f,9.f,f,roof,3);
 swingDoor(m,15.4f,16.6f,8.9f,f,roof,1);                                                     // the pan door: never locked
 wall(m,16.6f,8.8f,19.4f,9.f,f,roof,3);
 {const int d=swingDoor(m,19.4f,20.6f,8.9f,f,roof,1);m.doors[size_t(d)].requireState=stateId("lab_chem_seal");}   // the airlock, held on Chemistry's reading
 wall(m,20.6f,8.8f,22.9f,9.f,f,roof,3);
 wallSign(m,20.f,8.74f,f+2.62f,"ZONE 3 AIRLOCK","HELD ON THE CHEMISTRY READING. PURGE AT INSTRUMENTATION",2.4f,anN);
 wallSign(m,16.f,8.74f,f+2.62f,"CALIBRATION PAN","LIVE WATER. NO LIGHTS IN THE PAN",1.9f,anN);
 taped(m,{15.2f,8.79f},f+1.45f,anN,Readable::Note,"PAN ENTRY / 02:10","DUNNE WENT IN FOR HIS SECOND METER WITH THE TORCH IN HIS TEETH. R-46 IS OUT OF ITS PEN.",
  "NOBODY GOES IN. IF YOU HAVE TO, GO DARK AND GO SLOW. IT HEARS BOOTS.");
 // Dunne's desk: he faced the glass. His hand lamp, the recorder, the last tape in the machine
 dgDeskTerminal(m,9.5f,7.6f,f,1,1,"FIELD RECORDER / TAPE 46-11","02:02 / DUNNE. LAST TAPE IN THE MACHINE.","E / PLAY.",stateId("anl_fm_tape"));
 wall(m,8.75f,6.6f,10.25f,8.f,f+1.55f,f+3.4f,30);
 const int handLamp=int(m.clutterSpawns.size());kit::prop(m,model::mine_cage_lamp,9.98f,7.55f,f+.75f,.21f,.2f,.36f,.4f);
 {auto l=lamp({9.98f,7.55f},f+1.f,.7f,5.f,5600,0,WorldLightMount::Bare);l.prop=handLamp;l.enabledState=stateId("anl_fm_desk");l.period=.16f;l.onTime=.09f;m.lights.push_back(l);}
 dgSequence(m,"anl_fm_tape","anl_fm_desk",32000,17000);                                    // the hand lamp stutters where the tape says it did
 // The deck's key going down and coming up, tape hiss under Dunne, and the one sound the tape kept: the glass.
 {const Vec2 d{9.6f,7.8f};const float z=f+1.f;const char* t="anl_fm_tape";
  dgSay(m,t,100,Sound::LockerLatch,d,z,"",.5f);
  for(int at:{300,10300,20300})dgSay(m,t,at,Sound::AmbStatic,d,z,"",.16f);
  dgSay(m,t,400,Sound::AmbStatic,d,z,"TAPE 46-11. 02:02. DUNNE, FIELD MEASUREMENT. R-46 IN THE PAN. R-47 IS UP IN LAB 4 FOR HER TEST.",0);
  dgSay(m,t,5800,Sound::AmbStatic,d,z,"DUNNE (TAPE): West lamps walking.",0);
  dgSay(m,t,8600,Sound::AmbStatic,d,z,"DUNNE (TAPE): It's up. It's going with them. Like a dog after a ball.",0);
  dgSay(m,t,14000,Sound::AmbStatic,d,z,"DUNNE (TAPE): Lamps off. It stops where the light stopped. It doesn't look for me.",0);
  dgSay(m,t,16400,Sound::AmbStatic,d,z,"DUNNE (TAPE): Hand lamp on. Two seconds, for the log.",0);
  dgSay(m,t,19300,Sound::AmbStatic,d,z,"DUNNE (TAPE): Off. Off.",0);
  dgSay(m,t,21700,Sound::OneGlassBang,d,z,"",.3f);
  dgSay(m,t,22500,Sound::AmbStatic,d,z,"DUNNE (TAPE): It came to the glass. It's sitting where my hand was.",0);
  dgSay(m,t,27500,Sound::AmbStatic,d,z,"DUNNE (TAPE): Note for Lab 4. Her rule holds. It doesn't see us. It sees what we carry.",0);
  dgSay(m,t,31000,Sound::LockerLatch,d,z,"TAPE 46-11: END.",.5f);}
 // the lamp test console: runs the west walk by hand
 wall(m,12.6f,5.f,14.2f,5.03f,f+.9f,f+1.7f,20);anModel(m,model::lab_control_board,13.4f,5.06f,f+1.f,anS,false);
 terminal(m,{13.4f,5.55f},"FIELD LAMPS / WEST WALK","CHASE RIG, STAKES 1-4. THE LIGHT WALKS THE WEST SIDE OF THE PAN AND STARTS AGAIN.","E / RUN. E AGAIN / STOP.",0,stateId("anl_fm_west"));
 m.terminals.back().toggleState=true;
 wall(m,12.9f,5.17f,13.85f,5.93f,f+1.5f,f+3.4f,30);
 m.lights.push_back(inTube(5.f,7.f,f+3.4f,.55f,5.5f,4000,0,.1f));m.lights.push_back(inTube(19.f,7.f,f+3.4f,.55f,5.5f,4000,0));
 // The gallery's west end: the pan recorders, the oilskins on their pegs, the eyewash, the bench the survey sat
 // on to watch the water.
 for(float y:{5.75f,6.65f}){anModel(m,model::pwr_control_cabinet,1.2f,y,f,anE,true);m.pipes.push_back({{1.12f,y},{1.12f,y},f+1.6f,.03f,f+3.4f,8});}
 wall(m,1.f,5.33f,1.42f,7.07f,f+1.6f,f+3.4f,30);
 anModel(m,model::lab_status_display,1.04f,7.6f,f+1.75f,anE,false);
 {int n=0;for(auto [x,h]:{std::pair{2.1f,.95f},{2.65f,1.f},{3.75f,.9f},{4.3f,1.05f}}){wall(m,x,5.f,x+.4f,5.04f,f+h,f+1.62f,n++%2?18:39);wall(m,x+.04f,5.f,x+.36f,5.05f,f+1.62f,f+1.7f,8);wall(m,x+.18f,5.f,x+.22f,5.07f,f+1.7f,f+1.75f,20);}}
 anFix(m,model::med_scrub_sink,5.3f,5.3f,f,.49f,.54f,1.13f,anS,true);m.pipes.push_back({{5.3f,5.08f},{5.3f,5.08f},f+1.13f,.03f,f+3.4f,3});
 dgBench(m,4.f,7.9f,f,true);
 kit::prop(m,model::mine_hard_hat,3.2f,7.9f,f+.51f,.279f,.351f,.153f,0);kit::prop(m,model::mug,4.9f,7.95f,f+.51f,.09f,.09f,.1f,.2f);
 for(auto [x,y,yaw]:{std::tuple{8.4f,6.9f,.4f},{8.5f,8.3f,2.1f},{10.8f,8.2f,3.7f},{11.1f,6.9f,5.f}})kit::prop(m,model::office_paper_sheet,x,y,f,.4f,.293f,.004f,yaw);
 // East of the lamp console: the survey bench, its scopes still tracing, the way to the airlock painted on the floor.
 inCounter(m,15.f,5.f,18.6f,5.62f,f,.9f);wall(m,15.f,5.f,18.6f,5.03f,f+.9f,f+1.5f,28);
 for(float x:{15.85f,16.6f})kit::prop(m,model::lab_scope_monitor,x,5.26f,f+.9f,.468f,.345f,.446f,0);
 anModel(m,model::lab_instrument_terminal,17.5f,5.3f,f+.9f,anS,false,.75f);
 kit::prop(m,model::lab_field_meter,18.25f,5.3f,f+.9f,.25f,.328f,.148f,.4f);
 for(int i=0;i<4;++i)kit::prop(m,model::office_file_binder,15.08f+i*.1f,5.2f,f+.9f,.093f,.301f,.424f,0);
 for(auto [x,yaw]:{std::pair{15.9f,.2f},{17.5f,-.3f}})kit::prop(m,model::office_chair,x,6.3f,f,.55f,.55f,.95f,yaw);
 {auto l=lamp({16.f,5.5f},f+1.3f,.3f,2.8f,8000,-.15f,WorldLightMount::Bare,0,false);l.flicker=.1f;m.lights.push_back(l);}
 for(float x:{15.9f,16.9f})anModel(m,model::lab_status_display,x,5.03f,f+1.9f,anS,false);
 anModel(m,model::lab_wall_clock,19.2f,5.016f,f+2.3f,anS,false);
 floorLine(m,7.95f,5.f,8.05f,6.5f,f,14);floorLine(m,8.05f,6.4f,20.05f,6.5f,f,14);floorLine(m,19.95f,6.5f,20.05f,8.8f,f,14);
 // The east end, at the airlock's door: the cabinet, the bench, the reader, and the door's own lamps: red and
 // blinking while Chemistry reads foul, green once the purge has run.
 anModel(m,model::med_supply_cabinet,22.65f,7.2f,f,anW,true);wall(m,22.4f,6.6f,22.9f,7.8f,f+1.7f,f+3.4f,30);
 dgBench(m,21.5f,5.35f,f,true,1.f);
 anFix(m,model::med_first_aid_box,20.2f,5.06f,f+1.3f,.367f,.12f,.349f,anS,false);
 extinguisher(m,22.9f,8.3f,f,anW);
 anFix(m,model::lab_id_scanner,20.85f,8.77f,f+1.1f,.3f,.06f,.41f,anN,false);anFix(m,model::lab_id_scanner,16.85f,8.77f,f+1.1f,.3f,.06f,.41f,anN,false);
 {auto l=lamp({19.15f,8.78f},f+2.3f,.32f,4.f,3000,0,WorldLightMount::Wall,-kPi*.5f,true,{1.f,.12f,.06f});l.period=1.4f;l.onTime=.7f;l.enabledState=stateId("lab_chem_seal");l.invert=true;m.lights.push_back(l);}
 {auto l=lamp({20.85f,8.78f},f+2.3f,.3f,4.f,5200,0,WorldLightMount::Wall,-kPi*.5f,true,{.2f,1.f,.35f});l.enabledState=stateId("lab_chem_seal");m.lights.push_back(l);}
 // --- The calibration pan: five metres of dark over live water; no working light of its own ----------
 m.waterVolumes.push_back({5.6f,11.6f,14.4f,20.6f,f-.2f,f-.04f,.85f});
 // THE CATWALK: a steel walk raised round the whole basin on posts, three steps up, railed both sides; the
 // basin's coping runs in under its inner edge. Steps down into the pan only opposite the reference stand
 // and at the dry edge, where the old rail was left open.
 const float cw=f+.62f;
 for(auto [x1,y1,x2,y2]:{std::tuple{4.f,10.f,16.f,11.f},{4.f,21.f,16.f,22.f},{4.f,11.f,5.f,21.f},{15.f,11.f,16.f,21.f}}){
  wall(m,x1,y1,x2,y2,cw-.05f,cw,8);wall(m,x1,y1,x2,y2,f,cw-.05f,30);                             // the grating, and the solid under it
  const bool alongX=x2-x1>y2-y1;
  for(float s0:{0.f,1.f}){if(alongX)wall(m,x1,s0?y2-.06f:y1,x2,s0?y2:y1+.06f,cw-.2f,cw-.05f,12);else wall(m,s0?x2-.06f:x1,y1,s0?x2:x1+.06f,y2,cw-.2f,cw-.05f,12);}   // stringers
  for(float t=(alongX?x1:y1)+.4f;t<(alongX?x2:y2);t+=1.6f)for(float s0:{0.f,1.f}){
   if(alongX)wall(m,t-.04f,s0?y2-.1f:y1+.02f,t+.04f,s0?y2-.02f:y1+.1f,f,cw-.2f,12);else wall(m,s0?x2-.1f:x1+.02f,t-.04f,s0?x2-.02f:x1+.1f,t+.04f,f,cw-.2f,12);}}
 m.stairs.push_back({16.f,10.f,16.84f,11.f,f,cw,3,false,false});                                  // up from the pan door
 m.stairs.push_back({3.16f,15.5f,4.f,16.5f,f,cw,3,false,true});                                   // up from the west walk
 m.stairs.push_back({9.1f,11.f,10.9f,11.84f,f,cw,3,true,false});                                  // down into the pan, opposite the stand
 m.stairs.push_back({14.16f,15.2f,15.f,16.8f,f,cw,3,false,true});                                 // down into the pan at the dry edge
 // the coping, from the catwalk's inner edge to the water: concrete with a hazard nosing, cut at the two steps
 for(auto [x1,y1,x2,y2]:{std::tuple{5.f,11.f,9.1f,11.6f},{10.9f,11.f,15.f,11.6f},{5.f,20.6f,15.f,21.f},{5.f,11.6f,5.6f,20.6f},{14.4f,11.6f,15.f,15.2f},{14.4f,16.8f,15.f,20.6f}}){
  wall(m,x1,y1,x2,y2,f,f+.22f,15);wall(m,x1,y1,x2,y2,f+.22f,f+.24f,13);}
 // rails: the outer side broken only at the two stairs up, the inner only at the two steps down
 for(auto [x1,y1,x2,y2]:{std::tuple{4.f,10.f,16.f,10.08f},{4.f,21.92f,16.f,22.f},{4.f,10.f,4.08f,15.5f},{4.f,16.5f,4.08f,22.f},{15.92f,11.f,16.f,19.3f},{15.92f,20.7f,16.f,22.f},
   {5.f,10.92f,9.1f,11.f},{10.9f,10.92f,15.f,11.f},{5.f,21.f,15.f,21.08f},{4.92f,11.f,5.f,21.f},{15.f,11.f,15.08f,15.2f},{15.f,16.8f,15.08f,19.3f},{15.f,20.7f,15.08f,21.f}})
  guardRail(m,x1,y1,x2,y2,cw);
 // caged lamps on the rail posts at hand height, plant lamps that do not move: the walk is lit, the water
 // takes their light and gives none of it back
 for(auto [x,y]:{std::pair{4.1f,10.1f},{9.f,10.1f},{15.9f,10.1f},{4.1f,15.f},{15.9f,13.f},{4.1f,21.9f},{11.f,21.9f}}){
  anFix(m,model::mine_cage_lamp,x,y,cw+1.1f,.21f,.2f,.36f,0,false);
  m.lights.push_back(lamp({x,y},cw+1.3f,.58f,5.2f,4300,0,WorldLightMount::Bare,0,false));}
 // the water's skin: rings standing on it round the stand that never spread and never fade, and the
 // pan's black taking the rail lamps' light a hand's width from its edge
 for(auto [rx,ry]:{std::pair{1.6f,1.45f},{2.5f,2.05f},{3.35f,2.4f},{4.1f,2.48f}}){const int n=40;
  for(int k=0;k<n;++k){const float a0=k*2*kPi/n,a1=(k+1)*2*kPi/n;
   m.pipes.push_back({{10.f+std::cos(a0)*rx,17.9f+std::sin(a0)*ry},{10.f+std::cos(a1)*rx,17.9f+std::sin(a1)*ry},f-.036f,.011f,f-.036f,5});}}
 hazard(m,Hazard::Kind::Anomaly,5.6f,11.6f,14.4f,20.6f,f-.3f,f+2.2f,3.f);
 anHum(m,"anl_fm_hum",5.4f,11.4f,14.6f,20.8f,f,f+2.5f,.5f);
 for(float x:{3.f,9.f,15.f})anModel(m,model::pwr_vent_grille,x,22.95f,f+3.6f,anN,false);
 servicePipe(m,{1.f,9.4f},{17.8f,9.4f},roof-.4f,.08f,3);
 // the reference stand, across the water from the gallery glass: a steel platform, the bench, four
 // charged cells and the mast that holds the reference lamp still over the pan. Her field book on it.
 wall(m,8.9f,17.1f,11.1f,18.7f,f,f+.42f,15);for(auto [y1,y2]:{std::pair{17.1f,17.2f},{18.6f,18.7f}})wall(m,8.9f,y1,11.1f,y2,f+.42f,f+.44f,13);
 anModel(m,model::lab_clean_bench,10.f,17.6f,f+.62f,anN,true);                                // fixtures in the pan stand on its bottom, .2 under the floor
 for(int i=0;i<4;++i)kit::prop(m,model::anl_cell_charged,9.6f+i*.27f,17.6f,f+1.23f,.22f,.2f,.41f,i*.7f);
 steelColumn(m,10.f,18.42f,f+.42f,f+3.9f);
 wall(m,8.2f,18.3f,11.8f,18.54f,f+3.62f,f+3.84f,18);                                           // the crossarm
 for(float x:{8.4f,11.6f}){anFix(m,model::mine_cage_lamp,x,18.42f,f+3.24f,.21f,.2f,.36f,0,false);}
 m.lights.push_back(lamp({10.f,17.9f},f+3.f,1.15f,6.5f,7200,0,WorldLightMount::Bare,0,true,{.82f,.86f,1.f}));   // the reference lamp: still light, the safe kind, cold white; the violet is the cells' and the bus's
 // the mast's two outer lamps are on the field lamp bus: dark until Chamber 3's switch is closed, then seen from its window
 for(float x:{8.4f,11.6f}){auto l=lamp({x,18.3f},f+3.1f,.5f,6.f,9500,-.4f,WorldLightMount::Bare,0,false,{.62f,.35f,1.f});l.enabledState=stateId("anl_fm_bus");m.lights.push_back(l);}
 // THE WEST WALK'S RESULT, with R-46 out of its pen and in the pan: the water answers the lamps. While the walk
 // runs, a cold gleam on the water's west edge goes north with the light, half a second behind each stake, the
 // water there starts to run off something, and something under it breathes out. Stopped, it is the pan again.
 for(int i=0;i<4;++i){auto l=lamp({6.5f,std::clamp(21.8f-3.6f*float(i),12.2f,20.f)},f+.14f,.55f,2.8f,9500,-.2f,WorldLightMount::Bare,0,false,{.45f,1.f,.7f});
  l.enabledState=stateId("anl_fm_west");l.period=4.f;l.onTime=1.f;l.phase=3.5f-float(i);m.lights.push_back(l);}
 dgAmbient(m,Sound::Sting,{6.5f,16.f},f+.5f,.4f,16,AmbientEmitter::OnState,0,0,"anl_fm_west");
 dgAmbient(m,Sound::AmbWaterDrip,{6.3f,16.f},f+.2f,.55f,10,AmbientEmitter::Loop,0,0,"anl_fm_west");
 dgAmbient(m,Sound::OneMonsterBreath,{6.8f,15.f},f+.2f,.4f,12,AmbientEmitter::Random,5.f,3.f,"anl_fm_west");
 // over the pan, the gallery's survey flood: it hangs from the roof beams and looks straight down, a still
 // light, the safe kind; the water shows it back as one hard white sheet
 for(float y:{12.4f,16.1f,19.8f})wall(m,1.f,y-.12f,17.8f,y+.12f,roof-.36f,roof,12);
 anFix(m,model::pendant_lamp,10.f,16.1f,roof-1.1f,.5f,.5f,.34f,0,false);m.pipes.push_back({{10.f,16.1f},{10.f,16.1f},roof-.78f,.016f,roof-.36f,6});   // dead since Dunne went in
 dgPaper(m,{10.35f,17.45f},1.43f,kPi,Readable::Letter,"FIELD BOOK / LAB 4","light that moves draws the big ones. still light does nothing. dark does nothing. they don't see us, only what we carry.",
  "so the plant's lamps are safe and our torches aren't. lamps that walk would move them without a hand on them. ask dunne for stakes.");
 // the dry edge east of the pan, from the pan door to the bus walk: two red battery lamps mark it
 for(float y:{12.2f,16.2f})m.lights.push_back(lamp({17.76f,y},f+2.4f,.28f,5.5f,1000,-.5f,WorldLightMount::Wall,kPi,true,{1.f,.12f,.06f}));
 // the west walk: four stakes up the west edge, the light stepping north about a second a stake
 for(int i=0;i<4;++i)dgStake(m,{3.2f,21.8f-i*3.6f},f,f+1.66f,"anl_fm_west",4.f,1.f,4.f-float(i));
 // R-46's PEN: the dry strip between the catwalk and Chamber 3's window. The gate stands open against the wall
 // and was never the way out: one panel of the bars on the catwalk side is gone, the catwalk's rails are bent
 // through toward the water on both sides of the walk, and the scrapes go over the coping and in. Its tether
 // hangs from the wall ring, parted at the collar.
 // CREATURE, NOT BUILT IN THIS PASS: the Pointer R-46 (docs/Design/lab4-field-measurement-plan-20261104.md,
 // sections 3 and 10) belongs in this map: dormant on Dunne at (4.9,20.3) on the catwalk, waking on anl_fm_wake.
 // When the engine has it, add its CreatureSpawn (wakeState anl_fm_wake) by Dunne below and drop the three
 // sounds and four gleams keyed on anl_fm_west above, which stand in for it until then.
 anFix(m,model::med_cell_bars,16.04f,21.75f,f,2.3f,.04f,2.4f,kPi*.5f,false);wall(m,16.02f,20.6f,16.06f,22.9f,f,f+2.4f,30);
 for(float x:{16.02f,17.72f})wall(m,x,18.36f,x+.08f,18.44f,f,f+2.4f,12);wall(m,16.02f,18.36f,17.8f,18.44f,f+2.3f,f+2.4f,12);
 anFix(m,model::mine_cage_gate,17.7f,19.38f,f,1.79f,.056f,2.17f,kPi*.5f,false);
 for(auto [x1,y1,x2,y2,r,mat]:{std::tuple{15.9f,19.45f,15.2f,19.62f,.02f,6},{15.85f,20.5f,15.12f,20.25f,.02f,6},{15.8f,19.95f,15.3f,20.05f,.02f,6},{15.75f,19.4f,15.15f,20.6f,.025f,2},{15.5f,20.62f,15.14f,19.5f,.025f,2}})
  m.pipes.push_back({{x1,y1},{x2,y2},cw+r+.012f,r,cw+r+.012f,mat});
 wall(m,16.1f,19.65f,17.1f,20.25f,f+.003f,f+.007f,8);
 for(float o:{0.f,.16f,.3f}){wall(m,15.1f,19.72f+o,15.9f,19.745f+o,cw+.002f,cw+.006f,20);wall(m,14.42f,19.74f+o,15.f,19.765f+o,f+.241f,f+.245f,8);}
 wall(m,17.72f,19.95f,17.8f,20.05f,f+.7f,f+.8f,8);chain(m,{17.7f,20.f,f+.72f},1.5f);
 anFix(m,model::med_floor_drain,16.9f,21.6f,f,.5f,.5f,.01f,0,false);
 m.lights.push_back(lamp({17.76f,20.9f},f+2.35f,.4f,4.6f,2600,0,WorldLightMount::Wall,kPi,true));          // the pen's own lamp, a plant lamp: it never minded this one
 wall(m,16.3f,22.3f,17.3f,22.7f,f,f+.3f,8);wall(m,16.34f,22.34f,17.26f,22.66f,f+.3f,f+.31f,20);          // its trough: steel, dry, never used
 taped(m,{17.79f,21.6f},f+1.4f,anW,Readable::Note,"PEN / R-46","PAN DOG. TETHER ON THE WALL RING UNLESS A SURVEY IS RUNNING. NO HAND LAMPS PAST THE GATE.",
  "IT HAS NEVER TRIED THE BARS. IT SITS WHERE THE LAST LIGHT WAS AND WAITS FOR THE NEXT. -DUNNE");
 // Dunne, where he went down in the south-west corner; his torch rolled off toward the water
 dgBody(m,"anl_fm_clock",ActorVisual::LabTech,{4.5f,20.9f},cw,.3f);
 wall(m,4.1f,20.5f,4.92f,21.9f,cw+.001f,cw+.006f,16);
 propPose(m,model::mine_cage_lamp,5.3f,21.55f,cw,.21f,.2f,.36f,1.1f,kPi*.5f,0);
 kit::prop(m,model::lab_field_meter,3.7f,21.9f,f,.25f,.33f,.15f,2.2f);
 m.pickupSpawns.push_back({{3.4f,22.2f},PickupKind::HandgunAmmo,f});m.pickupSpawns.push_back({{5.6f,22.3f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{17.3f,10.5f},PickupKind::Health,f});
 // its east wall: the airlock's window, the bus walk, Chamber 3's window
 wall(m,17.8f,9.f,18.f,9.6f,f,roof,3);glazing(m,17.8f,9.6f,18.f,13.4f,f,roof,3,.95f,2.4f,true);wall(m,17.8f,13.4f,18.f,14.2f,f,roof,3);
 wall(m,17.8f,15.4f,18.f,17.f,f,roof,42);glazing(m,17.8f,17.f,18.f,20.6f,f,roof,42,1.f,2.f,true);wall(m,17.8f,20.6f,18.f,22.9f,f,roof,42);   // Chamber 3: cast shielding concrete
 // --- The airlock: open east to Energy Diagnostics' hall, held from the gallery -------------------------
 wall(m,18.f,14.f,22.9f,14.2f,f,roof,3);
 wall(m,18.f,9.f,24.f,14.f,f+3.f,roof,3);ceilingGrid(m,18.f,9.f,24.f,14.f,f+3.f);
 for(float y:{9.5f,13.5f})floorLine(m,18.2f,y,22.8f,y+.1f,f,13);
 wall(m,18.2f,9.6f,22.8f,13.4f,f,f+.004f,8);
 {auto l=inTube(20.5f,11.5f,f+3.f,.45f,5.f,3200,0);l.gel={1.f,.6f,.2f};m.lights.push_back(l);}
 // what makes it a lock: the air handler that holds it over pressure, the shower bar across the ceiling, the
 // suits on their rail (one hook bare), the lockers, the heat under the pan window
 anFix(m,model::pwr_air_handler,19.05f,13.55f,f,.815f,1.642f,2.618f,kPi*.5f,true);wall(m,18.2f,13.12f,19.9f,14.f,f+2.62f,f+3.f,30);
 wall(m,18.6f,13.3f,19.5f,13.9f,f+2.62f,f+3.f,20);
 for(int i=0;i<2;++i)anFix(m,model::psx_locker,20.75f+i*.86f,13.675f,f,.64f,.85f,2.42f,kPi*.5f,true);
 wall(m,20.38f,13.1f,22.f,14.f,f+2.42f,f+3.f,30);
 m.pipes.push_back({{18.f,11.5f},{22.75f,11.5f},f+2.8f,.035f,f+2.8f,5});m.pipes.push_back({{22.75f,11.5f},{22.75f,11.5f},f+2.8f,.03f,f+3.f,6});
 for(float x:{18.8f,20.f,21.2f,22.4f}){m.pipes.push_back({{x,11.5f},{x,11.5f},f+2.6f,.02f,f+2.8f,5});m.pipes.push_back({{x,11.5f},{x,11.5f},f+2.56f,.05f,f+2.6f,6});}
 // the suit pegs: high on the north wall, each suit on a steel hanger off its peg, boots clear of the floor
 wall(m,21.f,9.f,22.7f,9.06f,f+2.38f,f+2.42f,20);
 for(float x:{21.3f,21.9f,22.5f}){wall(m,x-.012f,9.f,x+.012f,9.3f,f+2.36f,f+2.39f,8);m.structures.back().solid=false;}
 for(auto [x,turn]:{std::pair{21.3f,.12f},{22.5f,-.1f}}){hungSuit(m,{x,9.3f,f+2.2f},turn);
  wall(m,x-.006f,9.294f,x+.006f,9.306f,f+2.f,f+2.36f,2);m.structures.back().solid=false;                       // the hanger's hook
  wall(m,x-.012f,9.09f,x+.012f,9.51f,f+1.985f,f+2.f,2);m.structures.back().solid=false;}                       // its shoulder bar
 anFix(m,model::radiator_panel,18.14f,11.5f,f,1.23f,.24f,.91f,anE,true);
 anFix(m,model::med_first_aid_box,18.7f,9.06f,f+1.3f,.367f,.12f,.349f,anS,false);
 extinguisher(m,22.5f,14.f,f,anN);
 anFix(m,model::med_floor_drain,20.5f,11.5f,f+.004f,.5f,.5f,.01f,0,false);
 // --- The bus walk and Chamber 3: the readings never stopped ----------------------------------------
 wallDoor(m,18.f,22.9f,15.4f,15.6f,19.6f,20.8f,f,roof,1);
 wall(m,18.f,15.6f,22.9f,22.9f,f+3.f,roof,15);inDeadTube(m,20.4f,19.f,f+3.f);
 for(float y:{18.6f,19.f})anModel(m,model::lab_knife_switch,22.85f,y,f+1.5f,anW,false);
 anModel(m,model::pwr_ammeter_box,22.85f,19.6f,f+1.5f,anW,false);
 terminal(m,{22.3f,21.f},"FIELD LAMP BUS / CHAMBER 3 PANEL","BUS OPENED BY HAND 02:24 WHEN MEDICAL TOOK LAB 4. HER BENCH RIG DEAD SINCE.","CLOSE TO RESTORE: ENERGY DIAGNOSTICS, LAB 4 BENCH RIG.",0,stateId("anl_fm_bus"));
 wall(m,21.8f,20.6f,22.8f,21.4f,f+1.5f,f+3.f,30);
 anModel(m,model::med_crash_cart,21.f,16.4f,f,anS,true);kit::prop(m,model::lab_scope_monitor,21.f,16.4f,f+1.f,.47f,.34f,.45f,anS);
 wall(m,20.6f,16.1f,21.4f,16.7f,f+1.45f,f+3.f,30);
 dgPaper(m,{19.6f,16.2f},0,.3f,Readable::Letter,"CHAMBER 3 / LIVE READING","READING RISING SINCE 01:58. MAINS ISOLATED 02:40. READING STILL RISING.","THE NEEDLE LEANS TOWARD WHOEVER WALKS PAST WITH A TORCH.");
 anModel(m,model::lab_clean_bench,20.2f,22.4f,f,anN,true);
 taped(m,{18.01f,21.8f},f+1.4f,anE,Readable::Sticky,"CHAMBER 3 / SIT STILL","AN HOUR AND A HALF IN HERE WITH THE LIGHTS OFF. IT CAME TO THE WINDOW ONCE, WHEN I CHECKED MY WATCH.","THE WATCH HAS A LIGHT. I HAVE TAKEN IT OFF.");
 m.lights.push_back(lamp({21.6f,15.62f},f+2.4f,.12f,3.f,1000,-.5f,WorldLightMount::Wall,kPi*.5f,true,{1.f,.12f,.06f}));
 // The chamber itself: cast shielding on every wall, steel battens over the joints; the sample on its stand in the middle of
 // the floor under four sensor posts, still giving a reading, still giving light; the lamp bus on the east wall,
 // three bars from its cabinet to the switches; and the corner where somebody sat out an hour and a half.
 wall(m,22.84f,15.6f,22.9f,22.9f,f,f+3.f,42);wall(m,18.f,22.84f,22.84f,22.9f,f,f+3.f,42);wall(m,18.f,15.6f,19.6f,15.66f,f,f+3.f,42);wall(m,20.8f,15.6f,22.84f,15.66f,f,f+3.f,42);
 for(float y=17.9f;y<22.5f;y+=1.2f)wall(m,22.82f,y,22.84f,y+.05f,f,f+3.f,12);
 for(float x=18.9f;x<22.5f;x+=1.2f)wall(m,x,22.82f,x+.05f,22.84f,f,f+3.f,12);
 // a plant lamp in its cage over the door, the kind the dog never minded: the plate reads, the corner stays dim
 m.lights.push_back(lamp({18.95f,15.66f},f+2.4f,.75f,6.5f,4300,.05f,WorldLightMount::Wall,kPi*.5f));
 m.lights.push_back(lamp({21.5f,22.84f},f+2.4f,.6f,6.f,4300,.05f,WorldLightMount::Wall,-kPi*.5f));   // and its pair on the south plate
 anFix(m,model::pwr_switchgear_cabinet,22.42f,16.62f,f,1.5f,.82f,2.7f,anW,true);wall(m,22.f,15.86f,22.84f,17.38f,f+2.7f,f+3.f,30);
 for(float z:{2.3f,2.42f,2.54f})m.pipes.push_back({{22.78f,17.38f},{22.78f,21.6f},f+z,.018f,f+z,3});
 for(float y:{18.6f,19.f,19.6f})m.pipes.push_back({{22.78f,y},{22.78f,y},f+1.84f,.014f,f+2.3f,3});
 for(float y:{17.6f,20.4f})wall(m,22.74f,y,22.84f,y+.05f,f+2.26f,f+2.58f,8);
 wall(m,19.85f,18.85f,20.75f,19.55f,f,f+.72f,20);wall(m,19.8f,18.8f,20.8f,19.6f,f+.72f,f+.75f,12);
 anFix(m,model::lab_test_chamber,20.3f,19.2f,f+.75f,.912f,.544f,.512f,anE,false);wall(m,19.85f,18.85f,20.75f,19.55f,f+1.3f,f+3.f,30);
 {auto l=lamp({20.3f,18.62f},f+1.1f,.42f,4.5f,9000,-.4f,WorldLightMount::Bare,0,false,{.62f,.35f,1.f});l.flicker=.2f;m.lights.push_back(l);}
 for(auto [x,y,yaw]:{std::tuple{19.4f,18.3f,anS},{21.2f,18.3f,anS},{19.4f,20.1f,anN},{21.2f,20.1f,anN}}){
  wall(m,x-.14f,y-.14f,x+.14f,y+.14f,f,f+.025f,8);wall(m,x-.025f,y-.025f,x+.025f,y+.025f,f,f+1.32f,20);
  anFix(m,model::lab_pressure_gauge,x,y,f+1.3f,.288f,.112f,.41f,yaw,false);
  m.pipes.push_back({{x,y},{x<20.f?19.85f:20.75f,y<19.f?18.95f:19.45f},f+.02f,.012f,f+.02f,6});}
 anFix(m,model::scrap_bedroll,18.55f,21.2f,f,.92f,1.957f,.181f,0,false);
 m.clutterSpawns.push_back({1,{19.25f,21.9f},f,.6f});m.clutterSpawns.push_back({1,{19.3f,21.5f},f,2.1f});m.clutterSpawns.push_back({2,{19.2f,20.9f},f,.3f});
 kit::prop(m,model::mug,19.9f,22.4f,f+.81f,.09f,.09f,.1f,.9f);kit::prop(m,model::lab_field_meter,20.5f,22.4f,f+.81f,.25f,.328f,.148f,2.9f);
 // THE BUS'S RESULT: closing it throws the switch with a bang, puts a green pilot over the blades and a hum on the
 // bars, lights the mast's two outer lamps across the water (seen from this window), and from then on the blades
 // spit now and then; in Lab 4 it is the pilot on the rig's console.
 {auto l=lamp({22.6f,18.8f},f+2.05f,.3f,3.2f,5200,0,WorldLightMount::Bare,0,false,{.2f,1.f,.35f});l.enabledState=stateId("anl_fm_bus");m.lights.push_back(l);}
 {Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=22.6f;h.y1=18.45f;h.x2=22.86f;h.y2=19.15f;h.bottom=f+1.45f;h.top=f+1.9f;h.damagePerSecond=0;h.enabledFlag=stateId("anl_fm_bus");h.period=11.f;h.onTime=.35f;m.hazards.push_back(h);}
 dgAmbient(m,Sound::OneMetalBang,{22.5f,18.8f},f+1.6f,.55f,14,AmbientEmitter::OnState,0,0,"anl_fm_bus");
 dgAmbient(m,Sound::AmbStatic,{22.5f,19.4f},f+2.3f,.38f,7,AmbientEmitter::Loop,0,0,"anl_fm_bus");
 m.pickupSpawns.push_back({{22.2f,22.2f},PickupKind::HandgunAmmo,f});m.pickupSpawns.push_back({{18.8f,22.4f},PickupKind::Health,f});
 // --- Names and air: the pan's air thick and cold, the airlock's amber ---------------------------------
 zone(m,"FIELD MEASUREMENT / CALIBRATION PAN",1.f,9.f,17.8f,23.f,-1000,1000,{.07f,.07f,.09f,.005f});
 zone(m,"FIELD MEASUREMENT / AIRLOCK",18.f,9.f,24.f,14.f,-1000,1000,{.2f,.15f,.09f,.007f});
 zone(m,"FIELD MEASUREMENT / CHAMBER 3",18.f,14.2f,22.9f,23.f,-1000,1000,{.12f,.09f,.09f,.007f});
 zone(m,"FIELD MEASUREMENT / KIT ROOM",1.f,1.f,5.8f,4.8f);
 zone(m,"FIELD MEASUREMENT / TECHS' OFFICE",10.2f,1.f,22.9f,4.8f);
 zone(m,"FIELD MEASUREMENT / GALLERY",1.f,4.8f,22.9f,8.8f);
 zone(m,"FIELD MEASUREMENT",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
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

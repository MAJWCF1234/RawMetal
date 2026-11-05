// CONTAINMENT (chapter 2, manifest 65-70): chunks 99, 100, 101, 102, 103. BLOCK-OUT: the real rooms,
// route, heights, doors and light at final scale, the story staged coarsely with library models.
// Polish (dressing density, pipework, writing passes, voice) comes later.
// See docs/Design/descent-campaign-20261025.md (Containment) for the brief.
// Route: from Containment B (98) S into Specimen Handling (99), E across its gallery into the
// Observation Theater (101), N from the theater back into the service corridors (97). From B W into
// the Breach Sector (102), ground zero, down through the crater and S to Emergency Containment (103),
// where the barricade (cont_barricade) is raised from the control room and the S door opens on Deep
// Research (104). The Isolation Wing (100) is the dead end east of the Quarantine Ward: LAB 4's
// bunker, her lamps and her handwriting.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- Containment kit (cb): the same fittings as CampaignChapterContainmentA.cpp ----------------
// Fixture and sign facing: 0 faces +y (south), kPi north, cbE east, cbW west.
constexpr float cbS=0,cbN=kPi,cbE=kPi*.5f,cbW=-kPi*.5f;
// Light yaw (a wall lamp's way into the room): 0 east, kPi*.5 south, kPi west, -kPi*.5 north.
constexpr float blE=0,blS=kPi*.5f,blW=kPi,blN=-kPi*.5f;
void cbFix(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,v[0]*s,v[1]*s,v[2]*s,yaw,solid});
}
WorldLight cbTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint=0,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;}
void cbDeadTube(Map& m,float x,float y,float roof){m.fixtures.push_back({model::clinic_ceiling_lamp_2,{x,y},roof-.17f-m.layers.front().elevation,.44f,1.04f,.17f,0,false});}
WorldLight cbBeacon(Vec2 p,float z,float yaw,float phase,const char* state){
 auto l=lamp(p,z,.36f,4.6f,3000,0,WorldLightMount::Wall,yaw,true,{1.f,.12f,.06f});
 l.period=1.4f;l.onTime=.7f;l.phase=phase;l.enabledState=stateId(state);l.invert=true;return l;}
WorldLight cbAlarm(Vec2 p,float z,float yaw,float phase){
 auto l=lamp(p,z,.42f,6.f,3000,0,WorldLightMount::Wall,yaw,true,{1.f,.1f,.05f});l.period=2.2f;l.onTime=1.3f;l.phase=phase;return l;}
WorldLight cbGlow(Vec2 p,float z,float intensity,float range,float kelvin,Point3 gel={1,1,1},float flicker=0){
 auto l=lamp(p,z,intensity,range,kelvin,0,WorldLightMount::Bare,0,false,gel);l.flicker=flicker;return l;}
void cbPendant(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float flicker=0,float tint=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});m.fixtures.push_back({model::pendant_lamp,p,z-m.layers.front().elevation,.5f,.5f,.34f,0,false});
 if(intensity>0){auto l=lamp(p,z-.05f,intensity,range,kelvin,tint,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);}}
void cbBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
void cbGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,float sight,int weapon,float speed,int health=90){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
void cbSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1,float y1,float x2,float y2){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
void cbHum(Map& m,const char* timer,float x1,float y1,float x2,float y2,float bottom,float top,float gain=.55f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(std::string(timer)+"_done");
 s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;s.bottom=bottom;s.top=top;s.durationMs=3600000;s.finishAtMs=3600000;
 s.soundIntervalMs=10000;s.soundUntilMs=3000000;s.tickSound=Sound::AnomalyHum;s.gain=gain;s.pitch=1;m.timedSequences.push_back(s);
}
ScriptAction cbSet(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
void cbEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,std::vector<ScriptAction> actions,bool once=true){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=once;
 if(*require)e.requireState=stateId(require);e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
DialogueReply cbReply(const char* text,const char* next="",const char* sets="",int stance=-1){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;if(*sets)r.sets.push_back({stateId(sets),1});r.stance=stance;return r;
}
void cbGlaze(Map& m,float x1,float y1,float x2,float y2,float f,float roof,float sill=.95f,float head=2.4f,bool holds=true,int mat=3){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,mat);
 const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)kit::glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill,f+head,17,holds);else kit::glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill,f+head,17,holds);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.03f,alongX?x1:y1,(alongX?x2:y2)-.06f);
  if(alongX)wall(m,t,y1,t+.06f,y2,f+sill,f+head,20);else wall(m,x1,t,x2,t+.06f,f+sill,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.08f,20);if(roof>f+head+.08f)wall(m,x1,y1,x2,y2,f+head+.08f,roof,mat);
 if(alongX)wall(m,x1,y1-.03f,x2,y2+.03f,f+sill,f+head,CollisionProxyMaterial);else wall(m,x1-.03f,y1,x2+.03f,y2,f+sill,f+head,CollisionProxyMaterial);
}
void cbCrust(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){wall(m,x1,y1,x2,y2,lo,hi,34);}
void cbMound(Map& m,float x,float y,float f,float r,float h){wall(m,x-r,y-r*.7f,x+r,y+r*.7f,f,f+h*.5f,34);wall(m,x-r*.6f,y-r*.45f,x+r*.6f,y+r*.45f,f+h*.5f,f+h,34);}
void cbKnot(Map& m,float x,float y,float f,float h){wall(m,x-.12f,y-.1f,x+.12f,y+.1f,f,f+h,35);wall(m,x+.06f,y-.18f,x+.2f,y-.04f,f,f+h*.6f,35);wall(m,x-.22f,y+.02f,x-.08f,y+.16f,f,f+h*.45f,35);}
void cbPortalY(Map& m,float x1,float x2,float y1,float y2,float f,float roof,float head){
 wall(m,x1,y1-.6f,x2,y1,f,roof,15);wall(m,x1,y2,x2,y2+.6f,f,roof,15);wall(m,x1,y1,x2,y2,f+head,roof,15);
 for(float y:{y1-.08f,y2})wall(m,x1-.04f,y,x2+.04f,y+.08f,f,f+head,20);
}
// Stencilled figures (0-9, A) painted on a wall face along x at y=face, facing +y (dir 1) or -y (dir -1):
// seven-stroke characters h high, the string centred on x, its foot at f+z.
void cbStencil(Map& m,const char* text,float x,float face,int dir,float z,float h,int mat=14){
 const float f=m.layers.front().elevation,w=h*.55f,t=h*.14f,gap=h*.24f,y1=dir>0?face:face-.014f,y2=dir>0?face+.014f:face;z+=f;
 const size_t n=std::char_traits<char>::length(text);float cx=x-(n*w+(n-1)*gap)*.5f;
 for(size_t i=0;i<n;++i,cx+=w+gap){const char c=text[i];
  const int k=c=='A'?0x77:c=='0'?0x3f:c=='1'?0x06:c=='2'?0x5b:c=='3'?0x4f:c=='4'?0x66:c=='5'?0x6d:c=='6'?0x7d:c=='7'?0x07:c=='8'?0x7f:c=='9'?0x6f:0;
  if(k&1)wall(m,cx,y1,cx+w,y2,z+h-t,z+h,mat);if(k&8)wall(m,cx,y1,cx+w,y2,z,z+t,mat);if(k&64)wall(m,cx,y1,cx+w,y2,z+(h-t)*.5f,z+(h+t)*.5f,mat);
  if(k&2)wall(m,cx+w-t,y1,cx+w,y2,z+h*.5f,z+h,mat);if(k&4)wall(m,cx+w-t,y1,cx+w,y2,z,z+h*.5f,mat);
  if(k&32)wall(m,cx,y1,cx+t,y2,z+h*.5f,z+h,mat);if(k&16)wall(m,cx,y1,cx+t,y2,z,z+h*.5f,mat);}
}
// A blown glazed front: dado, mullions and head with the glass gone, the shards on the floor (side +1/-1:
// the side the glass fell to), the sill and head at their own heights.
void cbBlownFront(Map& m,float x1,float y1,float x2,float y2,float f,float top,int side,unsigned seed,float sill=.9f,float head=2.6f){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,15);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.03f,alongX?x1:y1,(alongX?x2:y2)-.06f);
  if(alongX)wall(m,t,y1,t+.06f,y2,f+sill,f+head,20);else wall(m,x1,t,x2,t+.06f,f+sill,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.08f,20);if(top>f+head+.08f)wall(m,x1,y1,x2,y2,f+head+.08f,top,15);
 unsigned h=seed*2654435761u+3;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return (h&0xffffu)/65535.f;};
 for(int i=0;i<26;++i){float a=next()*len,o=.15f+next()*1.3f,w=.02f+next()*.05f;
  float px=alongX?x1+a:(side>0?x2+o:x1-o),py=alongX?(side>0?y2+o:y1-o):y1+a;wall(m,px-w,py-.02f,px+w,py+.02f,f+.002f,f+.006f,17);}
}
// A theater bench facing south (+y): seat, backrest on its north side, legs.
void cbBench(Map& m,float x,float y,float z){
 wall(m,x-1.3f,y-.22f,x+1.3f,y+.22f,z+.43f,z+.5f,10);wall(m,x-1.3f,y-.3f,x+1.3f,y-.22f,z+.5f,z+1.f,10);
 for(float dx:{-1.f,1.f})wall(m,x+dx-.06f,y-.2f,x+dx+.06f,y+.2f,z,z+.43f,2);
}

// SPECIMEN HANDLING (manifest 65). Where specimens were lifted, caged and moved: a handling floor
// three metres below the doors, holding cages along the west wall, isolation bays behind gates on
// the south, and an overhead crane across the middle with cage 7 still on its hook, hanging open.
// The crane log says it was lifted for the theater at 03:02 and opened at height, not by the console.
// Route: N seam from B onto the landing (-15) -> E along the gallery deck to the theater (101); the
// stair down to the floor (-18) for the cages, the bays and what came out of cage 7.
std::shared_ptr<const Map> makeSpecimenHandling(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[99];m.definition=CampaignChunks[99];m.definition.ambient=.2f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float fl=-18,up=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),fl));
 openEdge(m,{'N',10.f,14.f,up,false,0});
 openEdge(m,{'E',9.f,14.f,up,false,0});
 machineryClock(m,"conb_sh_clock",60000);const char* clock="conb_sh_clock";
 // --- The landing and the gallery (-15) -----------------------------------------------------------
 wall(m,8.f,0,17.f,6.f,fl,up,15);
 deck(m,up,17,2,23,15);deck(m,up,23,9,24,14);
 deckSupports(m,m.layers.back(),fl,6.f);
 wall(m,23.f,8.9f,24.f,14.1f,fl,up-.22f,15);                                                  // under the seam: no pit
 m.stairs.push_back({10.f,6.f,11.6f,10.48f,fl,up,16,true,false});
 guardRail(m,8.f,5.9f,10.f,6.f,up);guardRail(m,11.6f,5.9f,17.f,6.f,up);guardRail(m,7.9f,1.f,8.f,6.f,up);
 guardRail(m,17.f,6.f,17.1f,15.f,up);guardRail(m,17.f,14.9f,23.f,15.f,up);guardRail(m,17.f,2.f,23.f,2.1f,up);
 wallSign(m,15.4f,1.06f,up+2.2f,"NO ONE UNDER A SUSPENDED LOAD","CRANE 2 WORKS THE WHOLE FLOOR",2.6f,cbS);
 for(float x:{9.f,17.5f})m.lights.push_back(lamp({x,1.f},up+2.4f,.55f,6.f,3400,0,WorldLightMount::Wall,blS));
 terminal(m,{22.2f,4.2f},"CRANE 2 / LAST LIFTS","03:02 CAGE 7 LIFTED FOR TRANSFER TO THE THEATER. LOAD SWINGING. HELD AT 3.8 M.",
  "03:04 CAGE 7 DOOR OPEN AT HEIGHT. NOT FROM THIS CONSOLE. THE PADLOCK IS STILL ON IT.",up-fl);
 m.terminals.back().yaw=cbW;
 cbFix(m,model::lab_control_board,22.9f,6.2f,up+.95f,cbW,false);
 m.lights.push_back(lamp({23.f,8.f},up+2.3f,.5f,6.f,3400,0,WorldLightMount::Wall,blW));
 m.pickupSpawns.push_back({{20.6f,13.6f},PickupKind::Ammo,up});
 // --- The crane, and cage 7 on its hook --------------------------------------------------------
 wall(m,1.f,11.8f,23.f,12.2f,-9.6f,-9.f,12);
 for(float x:{1.f,22.7f})wall(m,x,1.f,x+.3f,23.f,-9.6f,-9.3f,12);
 wall(m,11.4f,11.55f,12.6f,12.45f,-10.f,-9.6f,8);
 m.fixtures.push_back({model::med_holding_cage,{12.f,12.f},3.8f,4.485f,2.026f,2.013f,0,false});
 for(float x:{11.6f,12.4f}){wall(m,x-.02f,11.98f,x+.02f,12.02f,fl+3.8f+2.01f,-10.f,8);m.structures.back().solid=false;}
 wall(m,14.24f,12.4f,14.28f,13.5f,fl+3.9f,fl+5.7f,2);m.structures.back().solid=false;           // its door, open
 m.lights.push_back(cbGlow({12.f,12.f},fl+5.4f,.4f,5.f,1000,{1.f,.12f,.06f},.2f));               // the hook's warning lamp
 // --- The floor (-18): cages west, bays south, the pool where it is wet and still --------------
 for(float y:{9.5f,12.6f,15.7f})cbFix(m,model::med_holding_cage,3.6f,y,fl,0,true);
 for(float x:{7.f,13.f,19.f})wall(m,x,19.f,x+.2f,23.f,fl,roof,15);
 // bay 1 and 3 gated, bay 2's bars hanging open, bay 4 the loading bay
 for(float cx:{4.1f,16.1f}){const float a=cx==4.1f?1.f:13.2f,b=cx==4.1f?7.f:19.f;
  wall(m,a,18.9f,cx-1.75f,19.1f,fl,roof,15);wall(m,cx+1.75f,18.9f,b,19.1f,fl,roof,15);wall(m,cx-1.75f,18.9f,cx+1.75f,19.1f,fl+2.72f,roof,15);
  cbFix(m,model::lab_cell_gate,cx,19.f,fl,0,false);wall(m,cx-1.75f,18.93f,cx+1.75f,19.07f,fl,fl+2.72f,CollisionProxyMaterial);}
 wall(m,7.2f,18.9f,8.85f,19.1f,fl,roof,15);wall(m,11.35f,18.9f,13.f,19.1f,fl,roof,15);wall(m,8.85f,18.9f,11.35f,19.1f,fl+2.4f,roof,15);
 cbFix(m,model::med_cell_bars_doorway,10.1f,19.f,fl,0,false);
 wall(m,8.85f,18.97f,9.52f,19.03f,fl,fl+2.4f,CollisionProxyMaterial);wall(m,10.68f,18.97f,11.35f,19.03f,fl,fl+2.4f,CollisionProxyMaterial);
 for(float cx:{4.1f,10.1f,16.1f}){m.lights.push_back(lamp({cx,23.f},fl+2.6f,.45f,5.f,4300,.25f,WorldLightMount::Wall,blN));cbFix(m,model::med_floor_drain,cx,21.f,fl,0,false);}
 forklift(m,21.1f,20.8f,fl);
 for(float x:{19.6f,20.8f})cbFix(m,model::lab_specimen_crate,x,17.2f,fl,x*.1f,true);
 handCart(m,8.2f,15.8f,fl);
 // The transfer line, the floor's reason: a floor track from under the gallery to bay 3's gate, the
 // hoist beam over it, a containment cart on the track with its load still strapped, and the
 // inspection table beside it where every specimen was looked at, under the one hard sodium lamp.
 const float lx=15.4f;
 rail(m,lx,7.2f,18.8f,fl);
 floorLine(m,lx-1.25f,7.2f,lx-1.13f,18.8f,fl,13);floorLine(m,lx+1.13f,7.2f,lx+1.25f,18.8f,fl,13);
 wall(m,lx-.09f,7.2f,lx+.09f,18.9f,fl+5.1f,fl+5.5f,18);                                           // the hoist beam
 for(float y:{7.4f,11.f,14.6f,18.6f})m.pipes.push_back({{lx,y},{lx,y},fl+5.5f,.035f,roof,6});
 wall(m,lx-.22f,10.9f,lx+.22f,11.5f,fl+4.7f,fl+5.1f,19);                                             // the hoist block, over the cart
 m.pipes.push_back({{lx,11.2f},{lx,11.2f},fl+1.2f,.02f,fl+4.7f,8});
 wall(m,lx-.75f,10.3f,lx+.75f,12.1f,fl+.3f,fl+.42f,11);                                              // the cart, its deck
 for(float dx:{-.6f,.6f})for(float y:{10.5f,11.9f})m.pipes.push_back({{lx+dx-.08f,y},{lx+dx+.08f,y},fl+.15f,.14f,fl+.15f,2});
 for(float dx:{-.72f,.69f})wall(m,lx+dx,10.3f,lx+dx+.03f,12.1f,fl+.15f,fl+.3f,13);
 wall(m,lx-.75f,10.3f,lx+.75f,12.1f,fl,fl+.3f,CollisionProxyMaterial);
 cbFix(m,model::lab_test_chamber,lx,11.2f,fl+.42f,cbE,true,1.2f);
 for(float y:{10.75f,11.65f})wall(m,lx-.72f,y,lx+.72f,y+.05f,fl+.42f,fl+1.22f,13);                    // its straps
 cbFix(m,model::med_autopsy_table,12.9f,14.4f,fl,cbE,true);
 m.fixtures.push_back({model::med_corpse_supine,{12.9f,14.4f},fl+.95f-m.layers.front().elevation,.744f,1.753f,.411f,0,false});
 cbFix(m,model::med_crash_cart,11.7f,15.9f,fl,cbE,true);
 kit::prop(m,model::med_microscope,11.6f,15.75f,fl+1.f,.4f);kit::prop(m,model::med_specimen_jar,11.75f,16.1f,fl+1.f,.2f);
 for(float y:{13.7f,15.1f})wall(m,12.42f,y,13.38f,y+.04f,fl+.012f,fl+.016f,16);
 cbPendant(m,{13.6f,14.4f},roof,fl+3.6f,1.9f,10.f,1900,0,-.25f);                                     // the sodium lamp over the line
 // the pool: wet steel, still, humming
 for(float x:{6.6f,8.2f})cbFix(m,model::med_floor_drain,x,13.6f,fl,0,false);
 wall(m,6.f,12.8f,9.4f,14.6f,fl+.002f,fl+.008f,17);
 cbCrust(m,6.4f,13.f,7.2f,13.6f,fl,fl+.04f);cbKnot(m,8.8f,14.f,fl,.5f);
 hazard(m,Hazard::Kind::Anomaly,6.f,12.8f,9.4f,14.6f,fl,fl+1.5f,4);
 cbHum(m,"conb_sh_hum",5.f,11.8f,10.4f,15.6f,fl,fl+3.f,.55f);
 m.lights.push_back(cbGlow({7.6f,13.7f},fl+.4f,.35f,3.f,6500,{.72f,.5f,1.f},.3f));
 // what came out of cage 7, and a handler who stayed at his post
 m.creatureSpawns.push_back({CreatureKind::Transformed,{13.9f,16.8f},fl,1,0});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{10.1f,21.4f},fl,1,0});
 cbBody(m,clock,ActorVisual::Worker,{13.3f,9.3f},fl,1.9f);
 m.pickupSpawns.push_back({{2.2f,18.2f},PickupKind::Health,fl});
 // --- Light: sodium only down the transfer line; the cages under cold service light, one out, the bays green
 cbPendant(m,{5.f,7.4f},roof,fl+5.4f,.7f,8.f,5200);cbPendant(m,{lx+.6f,8.4f},roof,fl+4.6f,1.f,8.f,2000,0,-.2f);
 cbPendant(m,{5.f,16.6f},roof,fl+5.4f,0,0,0);cbPendant(m,{lx+.6f,17.4f},roof,fl+4.6f,.9f,8.f,2000,.25f,-.2f);
 m.lights.push_back(lamp({1.f,12.6f},fl+2.6f,.5f,6.f,5000,0,WorldLightMount::Wall,blE));
 zone(m,"SPECIMEN HANDLING / GALLERY",8.f,0,24.f,15.f,up-.4f,1000,{.13f,.12f,.1f,.012f});
 zone(m,"SPECIMEN HANDLING / BAYS",1.f,19.f,23.f,23.f,-1000,1000,{.12f,.14f,.11f,.014f});
 zone(m,"SPECIMEN HANDLING",0,0,24,24,-1000,1000,{.15f,.13f,.10f,.016f});
 freightFinish(m,-1);
 return p;
}

// ISOLATION WING (manifest 67). Concrete bunkers off one corridor, steel doors with no handle on
// the inside. LAB 4 was put in Isolation 3 at 02:55 on her tier 3 reading and asked for a pen. At
// 03:52 her door opened without a card. Her lamps run down the corridor to it; inside, on the walls,
// her hand: she knows what is happening to her, and she tells the player to follow the lamps.
// In bunker 2 a man who was moved here stands facing the corner and does nothing. No fight.
// Route: W seam from the Quarantine Ward's east passage -> the corridor -> her bunker; back out W.
std::shared_ptr<const Map> makeIsolationWing(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[100];m.definition=CampaignChunks[100];m.definition.ambient=.26f;
 m.definition.playerStart={2.2f,11.5f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=0;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 machineryClock(m,"conb_iso_clock",60000);const char* clock="conb_iso_clock";
 // --- The corridor and the eight bunkers: doors in its two long walls -----------------------------
 const float doors[4][2]={{3.4f,4.6f},{9.4f,10.6f},{15.4f,16.6f},{20.4f,21.6f}};
 // Every bunker door is the same heavy issue and none is in the same state: a steel surround, its
 // number stencilled over it, a wired viewport beside the leaf, drop-bar brackets, a caged lamp. The
 // numbers zig-zag down the corridor: 1 north, 2 south, 3 north (hers), 4 south; 5-8 the spares.
 const char* numbers[2][4]={{"1","6","3","8"},{"5","2","7","4"}};
 for(int row=0;row<2;++row){const float y1=row?13.5f:9.3f,y2=y1+.2f,swing=row?1.f:-1.f,face=row?y1:y2,dir=row?-1.f:1.f;float a=0;
  auto onFace=[&](float x1,float x2,float lo,float hi,float out,int mat,bool solid=true){
   wall(m,x1,std::min(face,face+dir*out),x2,std::max(face,face+dir*out),f+lo,f+hi,mat);m.structures.back().solid=solid;};
  for(int k=0;k<4;++k){const float d0=doors[k][0],d1=doors[k][1],cx=(d0+d1)*.5f;const int n=numbers[row][k][0]-'0';
   wall(m,a,y1,d0-.9f,y2,f,roof,15);cbGlaze(m,d0-.9f,y1,d0-.4f,y2,f,roof,1.3f,1.72f,true,15);wall(m,d0-.4f,y1,d0,y2,f,roof,15);
   swingDoor(m,d0,d1,(y1+y2)*.5f,f,roof,swing);a=d1;
   onFace(d0-.18f,d0,0,2.86f,.1f,11);onFace(d1,d1+.18f,0,2.86f,.1f,11);onFace(d0-.18f,d1+.18f,2.55f,2.86f,.1f,11);
   onFace(d0-.9f,d0-.4f,1.24f,1.3f,.08f,20);
   cbStencil(m,numbers[row][k],cx,face,int(dir),2.98f,.62f);
   for(float x:{d0-.16f,d1+.04f})onFace(x,x+.12f,1.02f,1.26f,.14f,8);                                 // drop-bar brackets
   if(n==2||n>=5)onFace(d0-.24f,d1+.24f,1.08f,1.2f,.12f,8,false);                                     // barred
   if(n==1)onFace(d1+.3f,d1+.38f,0,1.7f,.08f,8);                                                      // its bar lifted off and stood by
   if(n==4){for(float z:{.85f,1.55f})onFace(d0-.1f,d1+.1f,z,z+.08f,.13f,13,false);onFace(d1-.1f,d1+.02f,1.9f,2.5f,.13f,13,false);}   // the seal tape, cut
   if(n==3)for(int i=0;i<5;++i)onFace(d1+.4f,d1+.9f+i*.35f,1.15f+i*.13f,1.17f+i*.13f,.01f,8);        // her pen, on the corridor wall
   m.fixtures.push_back({model::mine_cage_lamp,{d1+.42f,face+dir*.12f},f+2.6f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
   const float amber=n==3?2600.f:n==4?1000.f:4200.f;
   auto l=lamp({d1+.42f,face+dir*.02f},f+2.75f,n==3?.6f:n==4?.45f:.38f,5.5f,amber,0,WorldLightMount::Wall,row?blN:blS,true,n==4?Point3{1.f,.15f,.08f}:Point3{1,1,1});
   if(n==2||n==7)l.flicker=.45f;if(n!=6&&n!=8)m.lights.push_back(l);}                       // 6 and 8: lamps dead
  wall(m,a,y1,23.f,y2,f,roof,15);}
 for(float x:{6.9f,12.9f,18.9f}){wall(m,x,1.f,x+.2f,9.3f,f,roof,15);wall(m,x,13.7f,x+.2f,23.f,f,roof,15);}
 const float bays[4][2]={{1.f,6.9f},{7.1f,12.9f},{13.1f,18.9f},{19.1f,23.f}};
 for(int i=0;i<4;++i){const float a=bays[i][0],b=bays[i][1];
  wall(m,a,1.f,a+2.1f,2.f,f,f+.45f,15);wall(m,a,22.f,a+2.1f,23.f,f,f+.45f,15);                     // the bed slabs
  cbFix(m,model::med_ward_toilet,b-.5f,1.37f,f,cbS,true);cbFix(m,model::med_ward_toilet,b-.5f,22.63f,f,cbN,true);}
 // the corridor: dark but for the red at each end, and her lamps
 m.lights.push_back(lamp({6.f,9.5f},f+2.3f,.5f,7.f,1000,-.45f,WorldLightMount::Wall,blS));
 m.lights.push_back(lamp({18.f,13.5f},f+2.3f,.5f,7.f,1000,-.45f,WorldLightMount::Wall,blN));
 m.lights.push_back(cbTube(4.f,5.f,roof,.3f,4.5f,4400,.1f,.4f));m.lights.push_back(cbTube(21.f,4.6f,roof,.25f,4.f,4400,.1f));
 cbDeadTube(m,6.f,11.5f,roof);cbDeadTube(m,18.f,11.5f,roof);m.lights.push_back(cbTube(12.f,11.5f,roof,.4f,6.f,4600,.06f,.3f));
 for(auto [x,y,yaw]:{std::tuple{3.2f,12.9f,.3f},{8.2f,10.1f,2.4f},{12.6f,12.9f,-.5f},{16.f,10.1f,1.8f},{16.f,7.6f,.9f},{14.4f,3.2f,2.8f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f,.18f,.4f,.52f,yaw);
  auto l=cbGlow({x,y},f+.45f,.4f,3.8f,2700);l.prop=lampProp;m.lights.push_back(l);}
 terminal(m,{22.4f,11.5f},"ISOLATION WING / OCCUPANCY","1 EMPTY. 2 OCCUPANT DOES NOT ANSWER. 3 LAB 4 (ENERGY DIAGNOSTICS), TIER 3, IN 02:55. 4 SEE CLEANUP.",
  "03:52 DOOR 3 OPENED. NO CARD, NO OPERATOR. OCCUPANT LEFT UNASSISTED. MANUAL NOTE: THESE DOORS HAVE NO HANDLE INSIDE.",0);
 m.terminals.back().yaw=cbW;
 taped(m,{2.f,13.49f},f+1.5f,cbN,Readable::Note,"ISOLATION / BY ORDER 04:10","OCCUPANTS RELOCATED TO THE HUB FOR TRANSFER. EXCEPT 2. 2 DOES NOT ANSWER.",
  "DO NOT OPEN 2 TO ASK.");
 // --- Isolation 3: hers ------------------------------------------------------------------------
 m.fixtures.push_back({model::med_mattress_stained,{14.15f,1.5f},.45f,1.9f,.9f,.118f,0,false});
 for(auto [x1,z1,x2,z2]:{std::tuple{13.6f,1.2f,15.8f,1.24f},{13.8f,1.35f,17.6f,1.39f},{14.1f,1.5f,16.9f,1.54f},{13.5f,1.65f,18.2f,1.69f},{15.2f,1.8f,17.4f,1.84f}})
  wall(m,x1,1.f,x2,1.025f,f+z1+.5f,f+z2+.5f,20);                                                 // lines of her hand, all over the back wall
 for(float y:{3.f,4.2f,5.4f,6.6f})wall(m,18.875f,y,18.9f,y+.9f,f+1.2f,f+1.24f,20);
 taped(m,{16.f,1.03f},f+1.4f,cbS,Readable::Note,"WRITTEN ON THE WALL, IN PEN","THEY TOOK MY BADGE AT THE DOOR AND LEFT ME THE PEN. THE LAMP IN HERE HUMS WHEN I'M NOT LOOKING AT IT.",
  "I THINK THE HUM IS ME.");
 taped(m,{18.88f,5.f},f+1.5f,cbW,Readable::Note,"WRITTEN ON THE WALL, IN PEN","IF YOU'RE READING THIS YOU CAME DOWN. I SAID GO UP. I KNOW THERE IS NO UP.",
  "FOLLOW THE LAMPS. I'LL KEEP LIGHTING THEM AS LONG AS I CAN. - L4");
 taped(m,{13.12f,6.f},f+1.5f,cbE,Readable::Note,"WRITTEN ON THE WALL, IN PEN","03:50. MY HANDS ARE WARM ALL THE TIME. I CAN HEAR THE LOCK THINKING.",
  "IT OPENS IF I ASK IT. I'M GOING TO ASK IT.");
 terminal(m,{17.6f,8.6f},"ISOLATION 3 / INTERCOM","02:58 OCCUPANT: CAN SOMEONE TELL MY FRIEND IN SECURITY I'M OK. 03:20 OCCUPANT: I'M NOT OK.",
  "03:51 OCCUPANT: I'M SORRY ABOUT THE LIGHTS. 03:52 DOOR OPEN. CHANNEL CLOSED.",0);
 m.terminals.back().yaw=cbN;
 kit::prop(m,model::item_id_card,16.6f,4.f,f,.132f,.085f,.002f,1.2f);                            // somebody's; not hers
 // --- Isolation 2: he stands in the corner. He was a fitter. He keeps his habits. ----------------
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{10.8f,22.3f},f,cbS,0,0},{9000,{10.8f,22.3f},f,cbS,0,4}},true);
 m.lights.push_back(cbTube(10.f,18.f,roof,.22f,4.f,4000,.05f,.5f));
 // --- Isolation 4: the Cleanup man who shut himself in --------------------------------------------
 cbBody(m,clock,ActorVisual::CleanupSharpshooter,{21.f,18.f},f,2.2f);
 m.pickupSpawns.push_back({{20.2f,16.f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{2.6f,4.8f},PickupKind::Health,f});
 recessedFittings(m);
 zone(m,"ISOLATION WING / ISOLATION 3",13.1f,1.f,18.9f,9.3f,-1000,1000,{.16f,.12f,.08f,.012f});
 zone(m,"ISOLATION WING / CORRIDOR",0,9.5f,23.f,13.5f,-1000,1000,{.10f,.10f,.11f,.012f});
 zone(m,"ISOLATION WING",0,0,24,24,-1000,1000,{.10f,.10f,.11f,.012f});
 freightFinish(m,-1);
 return p;
}

// OBSERVATION THEATER (manifest 68). Where the board and Internal Investigations came to watch:
// raked benches from the doors down to a stage, and on the stage the containment cylinder. The
// subject stood up at 02:40 in the middle of demonstration 7 and has not sat down. The audience left
// in a hurry; two did not.
// Route: N seam from the service corridors onto the top landing (-15) -> the west gallery -> W seam
// to Specimen Handling; the aisle stair down the tiers to the stage (-18).
std::shared_ptr<const Map> makeObservationTheater(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[101];m.definition=CampaignChunks[101];m.definition.ambient=.21f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfacePaintedMaroon;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float fl=-18,up=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),fl));
 openEdge(m,{'N',10.f,14.f,up,false,0});
 openEdge(m,{'W',9.f,14.f,up,false,0});
 machineryClock(m,"conb_th_clock",60000);const char* clock="conb_th_clock";
 // --- The landing, the west gallery, the tiers ---------------------------------------------------
 wall(m,1.f,0,23.f,4.f,fl,up,15);
 wall(m,0,4.f,4.f,14.4f,fl,up,15);
 guardRail(m,4.f,4.f,4.1f,14.4f,up);guardRail(m,1.f,14.3f,4.f,14.4f,up);
 cbPortalY(m,1.f,1.4f,9.f,14.f,up,roof,2.8f);
 for(int i=0;i<5;++i){const float y0=4.f+.8f*i,top=up-.5f*(i+1);
  wall(m,4.1f,y0,11.2f,y0+.8f,fl,top,15);wall(m,12.8f,y0,23.f,y0+.8f,fl,top,15);
  for(float x:{5.6f,8.6f,14.4f,17.4f,20.4f})cbBench(m,x,y0+.45f,top);}
 m.stairs.push_back({11.2f,4.f,12.8f,8.f,fl,up,16,true,false});
 // the two who did not leave
 cbBody(m,clock,ActorVisual::SeniorScientist,{7.f,6.05f},up-1.5f,.3f);
 cbBody(m,clock,ActorVisual::Scientist,{18.6f,4.45f},up-.5f,2.8f);
 for(float x:{5.f,19.f})m.lights.push_back(lamp({x,1.f},up+2.4f,.35f,5.f,3000,0,WorldLightMount::Wall,blS));
 for(int i=0;i<4;++i){m.lights.push_back(cbGlow({11.05f,4.4f+i*.9f},up-.5f*(i+1)+.15f,.12f,1.8f,2800));}   // the aisle's step lights
 // --- The viewing glass: the audience sat behind it, the stage below. It went at 02:40 in three
 // places, outward, all over the stage floor. The aisle stair comes down through a framed gap.
 const float vg=8.f,gh=fl+4.f;
 cbGlaze(m,4.1f,vg,8.9f,vg+.12f,fl,gh+.35f,1.f,4.f);cbBlownFront(m,8.9f,vg,11.2f,vg+.12f,fl,gh+.35f,1,101,1.f,4.f);
 cbBlownFront(m,12.8f,vg,15.2f,vg+.12f,fl,gh+.35f,1,102,1.f,4.f);cbGlaze(m,15.2f,vg,20.f,vg+.12f,fl,gh+.35f,1.f,4.f);
 cbBlownFront(m,20.f,vg,23.f,vg+.12f,fl,gh+.35f,1,103,1.f,4.f);
 for(float x:{11.1f,12.8f})wall(m,x,vg-.04f,x+.1f,vg+.16f,fl,gh+.35f,20);wall(m,11.2f,vg,12.8f,vg+.12f,gh,gh+.35f,20);
 for(int i=0;i<3;++i){const float y0=4.f+.8f*(i+1),top=up-.5f*(i+2);paperDrift(m,4.4f,y0+.05f,10.9f,y0+.25f,top,3,1010+i);paperDrift(m,13.1f,y0+.05f,22.6f,y0+.25f,top,3,1020+i);}
 // --- The stage: raised the width of the room's middle, the cylinder at its centre, lit from the
 // rig: a truss across the volume with its cans on the subject, warm against the cylinder's violet.
 wall(m,7.5f,13.2f,16.5f,19.f,fl,fl+.3f,15);floorLine(m,7.5f,13.2f,16.5f,13.32f,fl+.3f,13);
 for(float x:{8.6f,10.4f,13.6f,15.4f})m.lights.push_back(cbGlow({x,13.05f},fl+.38f,.3f,2.6f,2600));     // the footlights
 wall(m,7.f,11.9f,17.f,12.1f,fl+6.4f,fl+6.7f,12);wall(m,7.f,12.5f,17.f,12.7f,fl+6.4f,fl+6.7f,12);
 for(float x:{7.2f,16.8f})for(float y:{12.f,12.6f})m.pipes.push_back({{x,y},{x,y},fl+6.7f,.03f,roof,6});
 for(float x:{9.2f,11.f,13.f,14.8f}){wall(m,x-.16f,12.14f,x+.16f,12.46f,fl+5.85f,fl+6.4f,8);
  m.lights.push_back(cbGlow({x,12.6f},fl+5.8f,1.25f,11.f,3300));}
 paperDrift(m,6.f,9.f,10.f,12.8f,fl,6,1031);
 wall(m,10.3f,14.3f,13.7f,17.7f,fl,fl+.3f,15);
 for(auto [x1,y1,x2,y2]:{std::tuple{10.6f,14.55f,13.4f,14.61f},{10.6f,17.39f,13.4f,17.45f},{10.55f,14.61f,10.61f,17.39f},{13.39f,14.61f,13.45f,17.39f}})
  breakable(m,x1,y1,x2,y2,fl+.3f,fl+3.8f,17,true);
 for(auto [x,y]:{std::pair{10.45f,14.45f},{13.35f,14.45f},{10.45f,17.35f},{13.35f,17.35f}})wall(m,x,y,x+.2f,y+.2f,fl+.3f,fl+3.8f,12);
 wall(m,10.3f,14.3f,13.7f,17.7f,fl+3.8f,fl+4.2f,12);
 for(auto [x,y]:{std::pair{10.9f,15.f},{13.1f,17.f}})m.pipes.push_back({{x,y},{x,y},fl+4.2f,.08f,roof,2});
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{12.f,16.f},fl+.3f,cbN,0,0},{9000,{12.f,16.f},fl+.3f,cbN,0,4}},true);   // the subject: he stood up
 m.lights.push_back(cbGlow({12.f,16.f},fl+2.6f,.55f,4.5f,6500,{.72f,.5f,1.f},.25f));
 hazard(m,Hazard::Kind::Anomaly,10.62f,14.62f,13.38f,17.38f,fl+.3f,fl+3.8f,0);
 cbHum(m,"conb_th_hum",7.f,11.f,17.f,21.f,fl,fl+3.f,.6f);
 for(auto [x,y]:{std::pair{9.2f,18.4f},{14.8f,18.4f}})cbPendant(m,{x,y},roof,fl+5.f,1.1f,10.f,6500);
 wall(m,6.3f,10.4f,7.3f,10.9f,fl,fl+1.1f,10);wall(m,6.25f,10.35f,7.35f,10.95f,fl+1.1f,fl+1.16f,10);   // the lectern
 kit::prop(m,model::office_paper_stack,6.8f,10.65f,fl+1.16f,.33f,.44f,.03f,.2f);
 wall(m,16.f,10.f,19.f,10.7f,fl,fl+.88f,20);wall(m,15.95f,9.95f,19.05f,10.75f,fl+.88f,fl+.92f,28);
 terminal(m,{17.5f,10.35f},"THEATER / DEMONSTRATION 7","AUDIENCE: THE BOARD, AND INTERNAL INVESTIGATIONS. SUBJECT R-31, FORMERLY A FITTER, BAY 2. FIELD STABLE.",
  "02:40 SUBJECT STOOD UP. DEMONSTRATION HALTED. AUDIENCE LEFT BY THE WEST GALLERY. INVESTIGATIONS STAYED TO WATCH.",.92f);
 m.terminals.back().yaw=0;
 kit::prop(m,model::office_chair,17.5f,11.4f,fl,.55f,.55f,.95f,0);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{5.f,20.f},fl,1,0});
 m.pickupSpawns.push_back({{21.6f,21.8f},PickupKind::Health,fl});m.pickupSpawns.push_back({{2.4f,12.6f},PickupKind::HandgunAmmo,up});
 m.lights.push_back(lamp({1.f,20.f},fl+2.6f,.3f,5.f,1000,-.45f,WorldLightMount::Wall,blE));
 for(float x:{6.f,18.f})m.lights.push_back(lamp({x,23.f},fl+2.8f,.55f,7.f,3000,0,WorldLightMount::Wall,blN));
 zone(m,"OBSERVATION THEATER / GALLERY",0,0,24.f,14.4f,up-.4f,1000,{.12f,.11f,.12f,.01f});
 zone(m,"OBSERVATION THEATER / STAGE",4.f,8.f,23.f,23.f,-1000,fl+3.f,{.14f,.13f,.17f,.018f});
 zone(m,"OBSERVATION THEATER",0,0,24,24,-1000,1000,{.12f,.11f,.13f,.014f});
 freightFinish(m,-1);
 return p;
}

// BREACH SECTOR (manifest 69). Ground zero. At 02:00 the floor of the containment sector went down
// three metres into the rock and came back up crusted: the growth on every face, knots of it lit
// from inside, fields over the crater that shimmer before they burn. Something tall walks the floor
// and stops when it is looked at. The Cleanup Force lost the sector at 03:05 and left a note on how.
// Route: E seam from B onto the east ledge (-15) -> down the slab into the crater (-18) -> across ->
// up the debris to the south ledge -> S seam to Emergency Containment.
std::shared_ptr<const Map> makeBreachSector(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[102];m.definition=CampaignChunks[102];m.definition.ambient=.14f;
 m.definition.playerStart={22.f,11.5f};m.definition.spawnHeight=-14.97f;m.definition.spawnYaw=kPi;   // on the ledge crust
 m.wallSurface=SurfaceRock;m.floorSurface=SurfaceRock;m.ceilingSurface=SurfaceConcrete;
 const float fl=-18,up=-15;
 m.layers.push_back(floorLayer(m.name.c_str(),fl));
 openEdge(m,{'E',9.f,14.f,up,false,0});
 openEdge(m,{'S',10.f,14.f,up,false,0});
 machineryClock(m,"conb_br_clock",60000);const char* clock="conb_br_clock";
 // --- What is left of the sector floor: the east ledge, the south ledge -------------------------
 wall(m,18.f,4.f,24.f,17.f,fl,up,15);wall(m,5.f,19.f,19.f,24.f,fl,up,15);
 for(auto [x1,y1,x2,y2,t]:{std::tuple{17.2f,5.2f,18.f,7.f,-15.6f},{17.f,14.f,18.f,16.6f,-16.2f},{8.8f,18.f,11.f,19.f,-15.7f},{15.f,18.2f,17.6f,19.f,-16.4f}})wall(m,x1,y1,x2,y2,fl,t,15);   // broken lips
 m.stairs.push_back({13.52f,10.f,18.f,11.6f,fl,up,16,false,true});                              // the slab that slid down
 m.stairs.push_back({7.f,14.52f,8.6f,19.f,fl,up,16,true,true});                                 // the debris up to the south
 cbCrust(m,18.f,4.f,23.f,17.f,up,up+.03f);cbCrust(m,5.f,19.f,19.f,23.f,up,up+.03f);
 // --- The crater (-18): crust, mounds, knots, the fallen catwalk, rock come up through it -------
 cbCrust(m,1.f,1.f,18.f,19.f,fl,fl+.03f);
 // The crater's silhouette: the west rim stands eleven metres, broken in steps; the north rim half
 // that; the ledges east and south are the low side you come in and leave by.
 wall(m,1.f,1.f,2.2f,19.f,fl,fl+11.f,26);wall(m,1.f,19.f,5.f,23.f,fl,up+2.f,26);
 for(auto [y1,y2,h]:{std::tuple{1.f,5.f,7.5f},{5.f,9.f,5.f},{9.f,12.5f,8.5f},{12.5f,19.f,4.f}})wall(m,2.2f,y1,y1<4.f?2.6f:3.f,y2,fl,fl+h,26);
 wall(m,2.2f,1.f,18.f,2.f,fl,fl+4.5f,26);wall(m,3.f,1.f,9.5f,2.f,fl+4.5f,fl+6.5f,26);wall(m,18.f,1.f,23.f,4.f,fl,up+1.2f,26);
 for(auto [x,y,h]:{std::tuple{3.3f,8.6f,1.2f},{3.4f,11.f,.9f},{3.3f,17.2f,.7f}})wall(m,x-.3f,y-.4f,x+.3f,y+.4f,fl+h-.6f,fl+h,26);
 m.lights.push_back(cbGlow({5.f,10.f},fl+9.5f,.8f,13.f,7200));
 for(auto [x,y,mdl,z,yaw]:{std::tuple{1.7f,4.f,model::cave_stalagmite,fl+11.f,.3f},{1.6f,10.6f,model::cave_stalagmite,fl+11.f,2.1f},{1.7f,16.2f,model::cave_rock_column,fl+11.f,1.f},
  {6.2f,1.5f,model::cave_rock_column,fl+6.5f,1.57f},{13.f,1.5f,model::cave_stalagmite,fl+4.5f,1.57f}})
  m.fixtures.push_back({mdl,{x,y},z-m.layers.front().elevation,model::size[mdl][0],model::size[mdl][1],model::size[mdl][2],yaw,false});   // the rim's broken crest                                         // the shaft light on the rim
 // At the bottom, the source: a spire of the crystal out of a heave of growth, lit from inside, the
 // thing every ledge looks down at. The field round it bites before you reach it.
 cbMound(m,8.6f,7.2f,fl+.03f,1.5f,1.1f);
 for(auto [x,y,w,h]:{std::tuple{8.6f,7.2f,.42f,5.6f},{7.9f,6.7f,.26f,3.6f},{9.3f,7.8f,.3f,4.2f},{9.4f,6.6f,.2f,2.8f},{7.8f,7.8f,.22f,2.5f},{8.9f,8.3f,.16f,1.9f}})
  wall(m,x-w,y-w*.8f,x+w,y+w*.8f,fl+1.f,fl+h,35);
 m.lights.push_back(cbGlow({8.6f,8.8f},fl+2.4f,1.7f,15.f,6500,{.72f,.5f,1.f},.15f));
 m.lights.push_back(cbGlow({8.6f,7.2f},fl+6.4f,.9f,10.f,6500,{.6f,.45f,1.f},.1f));
 hazard(m,Hazard::Kind::Anomaly,6.4f,5.2f,10.8f,9.2f,fl,fl+3.f,0);hazard(m,Hazard::Kind::Anomaly,7.f,5.6f,10.2f,8.8f,fl,fl+3.f,14);
 // The way down and the way out, marked by the Force with its work lamps.
 for(auto [x,y,z]:{std::tuple{18.4f,12.f,up},{13.1f,12.f,fl+.03f},{8.9f,14.2f,fl+.03f},{8.9f,19.3f,up}}){
  m.fixtures.push_back({model::mine_cage_lamp,{x,y},z-m.layers.front().elevation,.21f,.2f,.36f,0,false});
  m.lights.push_back(cbGlow({x,y},z+.3f,.65f,6.f,3000));}
 for(auto [x,y,r,h]:{std::tuple{5.f,6.f,1.4f,1.4f},{14.f,5.f,1.1f,1.f},{3.6f,14.f,1.2f,1.6f},{15.f,15.6f,.9f,.8f},{10.f,16.6f,.8f,.6f}})cbMound(m,x,y,fl+.03f,r,h);
 cbFix(m,model::cave_rock_slab,12.9f,17.5f,fl,0,true);
 cbFix(m,model::cave_boulder_a,4.3f,3.6f,fl,1.2f,true,.6f);
 cbFix(m,model::pwr_catwalk_section,14.6f,8.f,fl,.5f,true);
 for(auto [x,y]:{std::pair{11.f,8.f},{4.6f,10.6f},{12.6f,13.f}})cbFix(m,model::cave_crystal_bed_blue,x,y,fl+.03f,x,false,1.4f);
 const Vec2 knots[4]={{6.6f,9.f},{12.f,4.f},{3.f,17.f},{16.f,13.4f}};
 for(int i=0;i<4;++i){const Vec2 k=knots[i];cbKnot(m,k.x,k.y,fl+.03f,1.1f+.3f*(i%2));
  m.lights.push_back(cbGlow(k,fl+1.75f,.6f,5.5f,6500,i%2?Point3{.3f,.6f,1.f}:Point3{.72f,.5f,1.f},.25f));
  hazard(m,Hazard::Kind::Anomaly,k.x-1.4f,k.y-1.4f,k.x+1.4f,k.y+1.4f,fl,fl+2.4f,0);                // the shimmer first
  hazard(m,Hazard::Kind::Anomaly,k.x-.7f,k.y-.7f,k.x+.7f,k.y+.7f,fl,fl+2.4f,10);}                 // then it bites
 cbHum(m,"conb_br_hum",1.f,1.f,18.f,19.f,fl,fl+4.f,.7f);
 // the cable tray that came down burning along the north wall
 wall(m,6.6f,2.f,11.4f,2.4f,fl+.5f,fl+.62f,2);
 hazard(m,Hazard::Kind::Fire,7.f,2.f,11.f,2.5f,fl,fl+1.4f,10);
 m.lights.push_back(cbGlow({9.f,2.6f},fl+.9f,.8f,7.f,1800,{1,1,1},.3f));
 // the hole in the roof the light comes down
 m.lights.push_back(cbGlow({10.f,10.f},-7.f,1.4f,14.f,7500));
 // the tall one
 m.creatureSpawns.push_back({CreatureKind::Warden,{8.f,12.4f},fl,1,0});
 cbBody(m,clock,ActorVisual::Cleanup,{12.4f,9.f},fl,1.2f);cbBody(m,clock,ActorVisual::CleanupHeavy,{5.6f,16.8f},fl,2.9f);
 cbBody(m,clock,ActorVisual::Scientist,{14.2f,17.4f},fl,.2f);
 // --- The ledges: the Force's note, the alarms still turning --------------------------------------
 taped(m,{22.98f,6.2f},up+1.5f,cbW,Readable::Note,"SECTOR LOST / 03:05","THE TALL ONE DOES NOT MOVE WHILE YOU LOOK AT IT. SO KEEP LOOKING AT IT.",
  "IT COMES WHEN YOU RELOAD. IT COMES AT THE LIGHT. DON'T DO EITHER. - REYES");
 for(auto [x,y,yaw,ph]:{std::tuple{23.f,5.f,blW,0.f},{23.f,16.f,blW,.9f},{6.f,23.f,blN,.4f},{17.f,23.f,blN,1.3f}})m.lights.push_back(cbAlarm({x,y},up+2.4f,yaw,ph));
 m.pickupSpawns.push_back({{21.6f,15.6f},PickupKind::Ammo,up});m.pickupSpawns.push_back({{6.2f,21.4f},PickupKind::Health,up});
 zone(m,"BREACH SECTOR / CRATER",1.f,1.f,18.f,19.f,-1000,up-.5f,{.15f,.12f,.18f,.017f});
 zone(m,"BREACH SECTOR",0,0,24,24,-1000,1000,{.13f,.11f,.15f,.014f});
 freightFinish(m,-1);
 return p;
}

// EMERGENCY CONTAINMENT (manifest 70). The last line between the sector and Deep Research: welded
// shutters, two lines of barricade, a room welded shut with something still in it, and the
// barricade itself lowered over the south door at 03:20. The control room raises it. The Emergency
// detail has been on that door since three; the Investigator tells them nothing goes down. They
// will go with the player, if asked.
// Route: N seam from the Breach Sector -> through the shutters' gap -> round the second line (west)
// -> the control room (east): raise the barricade [cont_barricade] -> the S door to 104.
std::shared_ptr<const Map> makeEmergencyContainment(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[103];m.definition=CampaignChunks[103];m.definition.ambient=.23f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'S',10.f,14.f,f,true,stateId("cont_barricade")});                                  // door 0: the barricade
 openEdge(m,{'N',10.f,14.f,f,false,0});
 machineryClock(m,"conb_ec_clock",60000);const char* clock="conb_ec_clock";
 const StateId stance=stateId("conb_ec_stance");
 // --- Line A: welded shutters across the hall, a gap cut at the middle --------------------------
 // Checkpoint A, failed: the lockdown door at the middle came down and jammed at head height, its
 // lamp still turning, the only red north of the second line. The console behind it still runs.
 // East, what did not wait for the door tore its own way through the shutters.
 wall(m,1.f,6.2f,10.5f,6.5f,f,f+2.6f,2);wall(m,13.5f,6.2f,19.6f,6.5f,f,f+2.6f,2);wall(m,21.1f,6.2f,23.f,6.5f,f,f+2.6f,2);
 for(float x:{10.5f,13.f})wall(m,x,6.f,x+.5f,6.7f,f,f+3.4f,7);wall(m,10.5f,6.f,13.5f,6.7f,f+2.9f,f+3.4f,7);
 for(float x:{10.5f,13.f})wall(m,x,5.98f,x+.5f,6.f,f+.2f,f+2.6f,13);
 for(float z=f+2.18f;z<f+2.86f;z+=.17f)wall(m,11.f,6.3f,13.f,6.4f,z,z+.13f,2);                           // the shutter, jammed
 wall(m,11.6f,6.28f,11.75f,6.42f,f+1.9f,f+2.18f,2);                                                       // one slat sprung
 cbFix(m,model::lab_status_display,13.25f,6.72f,f+2.1f,0,false);
 terminal(m,{14.3f,7.3f},"CHECKPOINT A / LOCKDOWN","03:18 SECTOR DOOR LOWERED ON LOSS. 03:19 DOOR JAMMED AT 2.1 M. RETRY. RETRY. RETRY.",
  "03:22 SHUTTER BREACH EAST, PANEL 6. 03:24 POST ABANDONED. LOCKDOWN STILL IN FORCE.",0);
 m.terminals.back().yaw=0;
 m.lights.push_back(cbGlow({14.3f,7.6f},f+1.3f,.35f,3.f,9000,{.5f,.9f,1.f},.12f));
 {auto l=cbAlarm({12.f,5.97f},f+3.15f,blN,0);l.period=1.1f;l.onTime=.5f;l.intensity=.6f;l.range=8.f;m.lights.push_back(l);}
 {auto l=cbAlarm({12.f,6.73f},f+3.15f,blS,.55f);l.period=1.1f;l.onTime=.5f;l.intensity=.45f;m.lights.push_back(l);}
 // the tear through panel 6: slats bent up out of the way, scorched, the drag of something through
 for(float z:{f+1.95f,f+2.25f})wall(m,19.6f,6.25f,21.1f,6.45f,z,z+.12f,2);
 for(float x:{19.6f,21.f})wall(m,x,6.18f,x+.1f,6.52f,f,f+2.6f,8);
 for(auto [x1,y1,x2,y2]:{std::tuple{19.9f,5.6f,20.4f,5.75f},{20.6f,6.9f,21.2f,7.05f},{19.2f,7.3f,19.5f,7.6f}})wall(m,x1,y1,x2,y2,f+.002f,f+.04f,2);
 for(auto [x1,y1,x2,y2]:{std::tuple{19.7f,4.6f,20.5f,5.4f},{20.1f,6.6f,20.9f,7.6f},{19.6f,8.f,20.4f,9.f}})wall(m,x1,y1,x2,y2,f+.004f,f+.008f,16);
 m.lights.push_back(cbGlow({20.3f,7.4f},f+1.7f,.8f,6.f,4600));                                           // the work light left by the tear
 for(float x:{5.f,16.4f})cbFix(m,model::mine_board_barricade,x,6.6f,f,0,true);
 cbFix(m,model::barrier,9.f,7.8f,f,.5f,true);
 wall(m,6.2f,8.5f,8.2f,9.05f,f,f+.5f,15);floorLine(m,6.2f,8.5f,8.2f,9.05f,f+.5f,13);                       // a jersey barrier, on its side
 for(auto [x,y,v,yaw]:{std::tuple{8.f,7.6f,ActorVisual::Cleanup,.2f},{11.6f,8.4f,ActorVisual::CleanupRusher,2.7f},{15.6f,7.6f,ActorVisual::Cleanup,1.4f}})cbBody(m,clock,v,{x,y},f,yaw);
 m.fixtures.push_back({model::med_corpse_supine,{20.f,9.6f},0,.744f,1.753f,.411f,cbE,false});
 // --- The welded room (west): it knocks -----------------------------------------------------------
 wall(m,1.f,7.8f,6.2f,8.f,f,roof,15);wall(m,6.f,8.f,6.2f,13.f,f,roof,15);
 {const int d=wallDoor(m,1.f,6.2f,13.f,13.2f,3.f,4.2f,f,roof,1);m.doors[size_t(d)].requireState=stateId("conb_ec_welded");}   // never: welded
 for(float x:{3.f,4.15f})wall(m,x,13.2f,x+.05f,13.26f,f,f+2.4f,13);
 taped(m,{2.f,13.2f},f+1.5f,cbS,Readable::Note,"WELDED 03:30","DO NOT OPEN. WHATEVER IT SAYS. IT KNOWS OUR NAMES NOW.","");
 cbBody(m,clock,ActorVisual::CleanupMedic,{3.6f,10.4f},f,.8f);
 // --- Line B: crates and barriers across the hall, open at the west wall --------------------------
 for(float x:{7.f,8.7f,15.5f,17.2f})cbFix(m,model::barrier,x,15.5f,f,0,true);
 for(float x:{10.2f,11.4f,12.6f,13.8f,18.7f,19.9f,21.1f,22.3f})cbFix(m,model::lab_specimen_crate,x,15.5f,f,(x-16.f)*.006f,true);
 // --- The control room (east): the release -------------------------------------------------------
 wall(m,17.f,7.8f,23.f,8.f,f,roof,15);
 wall(m,16.8f,8.f,17.f,8.6f,f,roof,15);cbGlaze(m,16.8f,8.6f,17.f,12.4f,f,roof);wall(m,16.8f,12.4f,17.f,13.2f,f,roof,15);
 wallDoor(m,17.f,23.f,13.f,13.2f,19.6f,20.8f,f,roof,-1);
 cbFix(m,model::lab_control_board,22.9f,10.6f,f+.95f,cbW,false);
 terminal(m,{22.2f,10.6f},"EMERGENCY BARRICADE / SOUTH","LOWERED 03:20 ON SECTOR LOSS. HYDRAULICS: PRESSURE HELD. RAISING OPENS THE SECTOR TO DEEP RESEARCH.",
  "RAISE ON ORDER OF THE INVESTIGATOR ONLY. HE HAS NOT ORDERED IT.",0,stateId("cont_barricade"));
 m.terminals.back().yaw=cbW;
 cbFix(m,model::lab_status_display,22.95f,12.f,f+1.8f,cbW,false);
 for(float x:{18.2f,19.2f})cbFix(m,model::med_records_cabinet,x,8.27f,f,cbS,true);
 m.lights.push_back(cbTube(20.f,10.6f,roof,.5f,5.f,4200,.03f));
 m.lights.push_back(cbGlow({22.4f,10.6f},f+1.4f,.25f,2.4f,9000,{.6f,.8f,1.f},.08f));
 // raising it brings the sector's things to the noise
 {ScriptAction a;a.type=ScriptAction::Type::SpawnEnemy;a.enemyKind=CreatureKind::Transformed;a.position={11.f,3.f};a.z=f;a.amount=8;
  ScriptAction b=a;b.position={14.f,4.2f};ScriptAction s;s.type=ScriptAction::Type::Shake;s.amount=.5f;
  cbEvent(m,"conb_ec_raised",0,0,24,24,"cont_barricade",{a,b,s});}
 // --- The south door and the Emergency detail -----------------------------------------------------
 wall(m,8.6f,22.4f,10.f,23.f,f,roof,15);wall(m,14.f,22.4f,15.4f,23.f,f,roof,15);wall(m,10.f,22.4f,14.f,23.f,f+3.2f,roof,15);
 floorLine(m,9.f,21.5f,15.f,21.7f,f,13);
 wallSign(m,12.f,22.38f,f+3.35f,"BARRICADE DOWN","RAISED FROM THE CONTROL ROOM / EAST",3.f,cbN);
 for(auto [x,ph]:{std::pair{9.3f,0.f},{14.7f,.7f}})m.lights.push_back(cbBeacon({x,22.4f},f+2.9f,blN,ph,"cont_barricade"));
 {auto l=cbGlow({12.f,22.1f},f+3.f,.6f,5.f,5000,{.2f,1.f,.3f});l.enabledState=stateId("cont_barricade");m.lights.push_back(l);}
 cbFix(m,model::lab_specimen_crate,20.6f,20.4f,f,.3f,true);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,20.6f,20.4f,f+1.12f,.18f,.4f,.52f,-2.f);
  auto l=cbGlow({20.6f,20.2f},f+1.5f,.85f,7.f,3200);l.prop=lampProp;m.lights.push_back(l);}
 cbGuard(m,clock,"conb_ec_heavy",ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,"conb_ec",{{{12.f,19.6f},f,60000,cbN}},13,1,.9f,130);
 m.actorTracks.back().ai.stance=stance;
 cbGuard(m,clock,"conb_ec_lead",ActorVisual::Cleanup,GuardSquadRole::Assault,"conb_ec",{{{16.4f,19.2f},f,60000,cbN}},12,1,1.f);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("conb_ec_hello");}
 m.dialogue.push_back({stateId("conb_ec_hello"),"EMERGENCY DETAIL","We've been on this door since three. Raise it and we go down with you, or ahead of you. Your choice.","",{
  cbReply("Cover the north. I'll raise it.","","conb_ec_allied",int(SquadStance::Ally)),cbReply("What's down there?","conb_ec_down"),cbReply("Stay here.")}});
 m.dialogue.push_back({stateId("conb_ec_down"),"EMERGENCY DETAIL","Deep research. Bore samples. Whatever they were digging toward. He says nothing goes down. He didn't say why.","",{cbReply("Back.","conb_ec_hello")}});
 cbEvent(m,"conb_ec_truce",8.f,0,16.f,5.f,"",{cbSet("conb_ec_stance",int(SquadStance::Truce))});
 cbSequence(m,"conb_ec_radio","conb_ec_radio_done",22000,20000,8.f,16.f,17.f,23.f);
 const Vec2 radio{12.f,19.6f};
 cue(m,"conb_ec_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Emergency detail. Hold the barricade. Nothing goes down to Deep Research.",.9f,f+1.5f);
 cue(m,"conb_ec_radio",7000,Sound::Empty,radio,"RADIO: Sir, we've held it four hours. There's nobody left out there to keep out.",.9f,f+1.5f);
 cue(m,"conb_ec_radio",13000,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then you are keeping something in.",.9f,f+1.5f);
 cue(m,"conb_ec_radio",18000,Sound::Empty,radio,"EMERGENCY DETAIL: He means us.",.9f,f+1.5f);
 m.pickupSpawns.push_back({{21.6f,22.2f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{2.2f,21.6f},PickupKind::Health,f});
 // --- Light: tubes mostly out, the flood by the detail, red on the lines and the door -------------
 m.lights.push_back(cbTube(6.f,3.f,roof,.6f,6.5f,5200,.03f,.4f));m.lights.push_back(cbTube(12.f,11.f,roof,.65f,7.f,5200,.03f));m.lights.push_back(cbTube(12.f,3.f,roof,.6f,6.5f,5200,.03f));m.lights.push_back(cbTube(4.f,11.f,roof,.4f,5.f,5200,.03f,.3f));
 m.lights.push_back(cbTube(4.f,18.f,roof,.4f,5.f,5200,.03f,.3f));
 cbDeadTube(m,18.f,3.f,roof);cbDeadTube(m,12.f,18.f,roof);cbDeadTube(m,7.f,11.f,roof);
 recessedFittings(m);
 zone(m,"EMERGENCY CONTAINMENT / CONTROL ROOM",17.f,8.f,23.f,13.f);
 zone(m,"",8.f,20.f,16.f,24.f,-1000,1000,{.16f,.1f,.09f,.012f});
 zone(m,"EMERGENCY CONTAINMENT",0,0,24,24,-1000,1000,{.13f,.12f,.12f,.012f});
 freightFinish(m,-1);
 return p;
}
}
void buildChapterContainmentB(CampaignMapTable& maps){
 maps[99]=makeSpecimenHandling();
 maps[100]=makeIsolationWing();
 maps[101]=makeObservationTheater();
 maps[102]=makeBreachSector();
 maps[103]=makeEmergencyContainment();
}
}

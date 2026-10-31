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

// --- Chunk 71 helpers (m71) -----------------------------------------------------------------
// A swinging clinic door: the renderer draws leaf, posts and head to 2.55 m; the wall closes above.
void m71Swing(Map& m,float x1,float x2,float y,float f,float roof,float swing,float open=0){
 wall(m,x1,y-.1f,x2,y+.1f,f+2.55f,roof,3);
 Door d{x1,x2,y,open};d.swinging=true;d.swing=swing;d.opening=open>0;d.maxOpen=.9f;m.doors.push_back(d);
}
// An open doorway through a wall that runs along y: pale-steel jambs and head, wall above.
void m71DoorwayY(Map& m,float y1,float y2,float x1,float x2,float f,float roof){
 wall(m,x1,y1,x2,y2,f+2.3f,roof,3);
 for(float y:{y1-.06f,y2})wall(m,x1-.03f,y,x2+.03f,y+.06f,f,f+2.3f,20);
 wall(m,x1-.03f,y1,x2+.03f,y2,f+2.26f,f+2.3f,20);
}
// Clinic glazing in a straight wall: painted dado, steel sill and mullions, wired glass to
// `head`, wall above. An invisible proxy fills sill to head across the wall (and any
// counter shelf) so nobody perches on the sill or steps through the pane.
void m71Glaze(Map& m,float x1,float y1,float x2,float y2,float f,float roof,float sill,float head,float counter=0){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,3);
 if(alongX)wall(m,x1,y1-.03f,x2,y2+.03f,f+sill-.03f,f+sill+.01f,20);else wall(m,x1-.03f,y1,x2+.03f,y2,f+sill-.03f,f+sill+.01f,20);
 const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)wall(m,x1,cy-.012f,x2,cy+.012f,f+sill+.01f,f+head,17);else wall(m,cx-.012f,y1,cx+.012f,y2,f+sill+.01f,f+head,17);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.025f,alongX?x1:y1,(alongX?x2:y2)-.05f);
  if(alongX)wall(m,t,y1+.02f,t+.05f,y2-.02f,f+sill+.01f,f+head,20);else wall(m,x1+.02f,t,x2-.02f,t+.05f,f+sill+.01f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.06f,20);if(roof>f+head+.06f)wall(m,x1,y1,x2,y2,f+head+.06f,roof,3);
 // counter: a shelf this deep on the -y (alongX) or -x side, under the proxy.
 if(alongX)wall(m,x1,y1-.04f-counter,x2,y2+.04f,f+sill+.01f,f+head,30);else wall(m,x1-.04f-counter,y1,x2+.04f,y2,f+sill+.01f,f+head,30);
}
// A ceiling tube that works: the emitter sits clear below its recessed fitting.
WorldLight m71Tube(float x,float y,float roof,float intensity,float range,float kelvin,float tint,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;
}
// A fitting with its tubes dead: the housing alone, flush to the ceiling.
void m71DeadTube(Map& m,float x,float y,float f,float roof,float yaw=0){
 mdFix(m,model::clinic_ceiling_lamp_2,x,y,roof-.17f,.44f,1.04f,.17f,yaw,false);
}
WorldLight m71Switched(WorldLight l,StateId state,bool invert=false){l.enabledState=state;l.invert=invert;return l;}
ScriptAction m71Set(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
void m71Event(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,std::vector<ScriptAction> actions,bool once=true){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=once;
 if(*require)e.requireState=stateId(require);e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
DialogueReply m71Reply(const char* text,const char* next="",const char* sets=""){
 DialogueReply r;r.text=text;r.next=*next?stateId(next):0;if(*sets)r.sets.push_back({stateId(sets),1});return r;
}

// MEDICAL SCREENING (manifest 48). The clinic front where Depthworks sorted
// its staff by dose: a reception window, a waiting room told it was routine,
// four screening cubicles, the counting room, and the triage board that put
// LAB 4 in Ward C. Tonight the Medical detail that "did the booths" just after
// four is burning the counts on the Investigator's orders, and the tier-3s
// they wheeled south at ten to three are behind a door that stays shut.
// Route: the W seam from Personnel opens into the lobby; the spine runs east
// to the vestibule and the E seam into Triage. Staff-side loops: waiting room
// -> technician corridor -> records; transfer passage -> linen -> staff room
// -> nurses' station. The S door to Ward C opens on med_override (from 73).
std::shared_ptr<const Map> makeMedicalScreening(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[71];m.definition=CampaignChunks[71];m.definition.ambient=.24f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'S',10.f,14.f,f,true,stateId("med_override")});   // door 0: Ward C and the deep wing
 openEdge(m,{'W',9.f,14.f,f,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 machineryClock(m,"md_screen_clock",60000);
 const char* clock="md_screen_clock";
 const StateId override_=stateId("med_override"),stance=stateId("medf_screen_stance");
 // --- Plan: walls (faces line up with the 9-14 seams) --------------------------------------------
 // Seam portals: pale-steel frames where 69's paint meets this clinic's, and where 72's does.
 for(float x:{1.f,22.88f}){wall(m,x,9.f,x+.12f,9.14f,f,f+2.7f,20);wall(m,x,13.86f,x+.12f,14.f,f,f+2.7f,20);wall(m,x,9.f,x+.12f,14.f,f+2.7f,roof,20);}
 // Lobby / waiting room: a 3 m mouth under a bulkhead.
 wall(m,1.f,8.8f,1.8f,9.f,f,roof,3);wall(m,4.8f,8.8f,7.6f,9.f,f,roof,3);wall(m,1.8f,8.8f,4.8f,9.f,f+2.4f,roof,3);
 for(float x:{1.76f,4.76f})wall(m,x,8.76f,x+.08f,9.04f,f,f+2.4f,20);
 wall(m,5.6f,9.f,7.6f,10.f,f,roof,3);wall(m,17.2f,9.f,18.4f,10.f,f,roof,3);          // service risers
 for(Vec2 c:{Vec2{5.6f,10.f},Vec2{18.4f,10.f}})wall(m,c.x-.04f,c.y-.04f,c.x+.04f,c.y+.04f,f,f+2.f,20);
 // Waiting room east wall, its staff doorway into the technician corridor.
 wall(m,7.4f,1.f,7.6f,5.2f,f,roof,3);wall(m,7.4f,6.4f,7.6f,8.8f,f,roof,3);m71DoorwayY(m,5.2f,6.4f,7.4f,7.6f,f,roof);
 // Cubicle fronts on the spine: four doors, four wired vision panels (c4's shot out).
 for(auto [a,b]:{std::pair{7.6f,7.9f},{8.8f,9.1f},{9.7f,10.4f},{11.3f,11.6f},{12.2f,12.8f},{13.7f,14.f},{14.6f,15.2f},{16.1f,16.4f},{17.f,17.2f}})wall(m,a,9.8f,b,10.f,f,roof,3);
 const float cubL[4]={7.6f,10.1f,12.5f,14.9f},cubR[4]={9.9f,12.3f,14.7f,17.2f};
 for(int i=0;i<4;++i){const float L=cubL[i],pa=L+1.5f,pb=L+2.1f;
  wall(m,pa,9.8f,pb,10.f,f,f+1.25f,3);wall(m,pa,9.8f,pb,10.f,f+2.f,roof,3);
  for(float z:{1.22f,1.99f})wall(m,pa-.03f,9.77f,pb+.03f,10.03f,f+z,f+z+.04f,20);
  if(i!=3){wall(m,pa,9.888f,pb,9.912f,f+1.26f,f+2.f,17);wall(m,pa,9.85f,pb,9.95f,f+1.26f,f+2.f,30);}
  m71Swing(m,L+.3f,L+1.2f,9.9f,f,roof,-1,i==1?.9f:0);}                        // doors 1-4
 for(float x:{9.9f,12.3f,14.7f})wall(m,x,6.8f,x+.2f,9.8f,f,roof,27);
 wall(m,17.2f,6.4f,17.4f,9.f,f,roof,3);
 // Cubicle backs: a curtained staff opening each, onto the technician corridor.
 for(auto [a,b]:{std::pair{7.6f,7.75f},{8.65f,10.25f},{11.15f,12.65f},{13.55f,15.05f},{15.95f,17.2f}})wall(m,a,6.6f,b,6.8f,f,roof,3);
 for(float L:cubL){wall(m,L+.15f,6.6f,L+1.05f,6.8f,f+2.3f,roof,3);
  wall(m,L+.02f,6.82f,L+.14f,6.94f,f+.06f,f+2.2f,39);                               // the curtain, drawn back
  m.pipes.push_back({{L+.04f,6.88f},{L+1.1f,6.88f},f+2.24f,.014f,f+2.24f,5});
  for(float x:{L+.03f,L+1.07f})wall(m,x,6.8f,x+.03f,6.9f,f+2.21f,f+2.27f,20);}
 // Technician corridor north wall: counting room and dosimeter store.
 wall(m,7.6f,5.f,9.f,5.2f,f,roof,3);wall(m,9.9f,5.f,14.f,5.2f,f,roof,3);wall(m,14.9f,5.f,17.2f,5.2f,f,roof,3);
 m71Swing(m,9.f,9.9f,5.1f,f,roof,-1);                                              // 5: counting room
 m71Swing(m,14.f,14.9f,5.1f,f,roof,-1);                                            // 6: dosimeter store
 wall(m,12.4f,1.f,12.6f,5.f,f,roof,3);
 wall(m,17.2f,1.f,17.4f,5.2f,f,roof,3);m71DoorwayY(m,5.2f,6.4f,17.2f,17.4f,f,roof);
 // Vestibule: records north (left open: the fire shows), nurses' station south.
 wall(m,17.4f,8.8f,19.4f,9.f,f,roof,3);wall(m,20.3f,8.8f,23.f,9.f,f,roof,3);
 m71Swing(m,19.4f,20.3f,8.9f,f,roof,-1,.9f);                                      // 7: records
 wall(m,18.4f,13.f,18.6f,14.2f,f,roof,3);
 wall(m,18.6f,14.f,21.4f,14.2f,f,roof,3);wall(m,22.3f,14.f,23.f,14.2f,f,roof,3);
 m71Swing(m,21.4f,22.3f,14.1f,f,roof,1);                                           // 8: nurses' station
 // Spine south wall: hold door, the board, the transfer mouth, the nurses' screen.
 wall(m,5.6f,13.f,6.2f,13.2f,f,roof,3);m71Swing(m,6.2f,7.1f,13.1f,f,roof,1);    // 9: tier-3 hold
 wall(m,7.1f,13.f,10.f,13.2f,f,roof,3);
 wall(m,10.f,13.f,14.f,13.2f,f+2.4f,roof,3);for(float x:{9.96f,13.96f})wall(m,x,12.96f,x+.08f,13.24f,f,f+2.4f,20);
 wall(m,9.96f,12.96f,14.04f,13.24f,f+2.36f,f+2.42f,20);
 wall(m,14.f,13.f,14.4f,13.2f,f,roof,3);wall(m,18.2f,13.f,18.6f,13.2f,f,roof,3);
 m71Glaze(m,14.4f,13.f,18.2f,13.2f,f,roof,.9f,2.4f);
 // Lobby south: the reception screen with its counter, the office door.
 m71Glaze(m,1.f,14.f,4.3f,14.2f,f,roof,1.05f,2.4f,.3f);
 wall(m,1.f,13.7f,4.3f,14.f,f+1.f,f+1.05f,20);for(float x:{1.4f,2.65f,3.9f})wall(m,x,13.8f,x+.04f,14.f,f+.82f,f+1.f,20);
 wall(m,4.3f,14.f,4.5f,14.2f,f,roof,3);wall(m,5.4f,14.f,5.6f,14.2f,f,roof,3);
 m71Swing(m,4.5f,5.4f,14.1f,f,roof,1);                                             // 10: reception office
 // South band partitions.
 wall(m,5.6f,13.f,5.8f,23.f,f,roof,3);
 wall(m,1.f,18.6f,1.6f,18.8f,f,roof,3);wall(m,2.5f,18.6f,5.6f,18.8f,f,roof,3);
 m71Swing(m,1.6f,2.5f,18.7f,f,roof,1);                                             // 11: staff WC
 wall(m,9.8f,13.2f,10.f,15.f,f,roof,3);m71Glaze(m,9.8f,15.f,10.f,16.6f,f,roof,1.f,2.1f);
 wall(m,9.8f,16.6f,10.f,21.6f,f,roof,3);wall(m,9.8f,22.6f,10.f,23.f,f,roof,3);m71DoorwayY(m,21.6f,22.6f,9.8f,10.f,f,roof);
 wall(m,14.f,13.2f,14.2f,19.6f,f,roof,3);wall(m,14.f,20.6f,14.2f,23.f,f,roof,3);m71DoorwayY(m,19.6f,20.6f,14.f,14.2f,f,roof);
 m71Glaze(m,10.f,16.8f,11.4f,17.f,f,roof,1.f,2.55f);m71Glaze(m,12.6f,16.8f,14.f,17.f,f,roof,1.f,2.55f);
 m71Swing(m,11.4f,12.6f,16.9f,f,roof,1);                                           // 12: the transfer screen leaf
 wall(m,14.2f,18.4f,19.4f,18.6f,f,roof,3);wall(m,20.3f,18.4f,23.f,18.6f,f,roof,3);
 m71Swing(m,19.4f,20.3f,18.5f,f,roof,1);                                           // 13: staff room
 wall(m,16.8f,18.6f,17.f,19.6f,f,roof,3);wall(m,16.8f,20.6f,17.f,23.f,f,roof,3);m71DoorwayY(m,19.6f,20.6f,16.8f,17.f,f,roof);
 // Over the Ward C door: a painted header on this side of the bulkhead's own.
 wall(m,10.f,22.7f,14.f,22.88f,f+2.5f,roof,3);
 // --- Light (28 records): half the spine dark on purpose -----------------------------------------
 m.lights.push_back(m71Tube(3.2f,11.5f,roof,.8f,6.f,5000,.1f));                   // lobby
 m.lights.push_back(m71Tube(8.8f,11.5f,roof,.75f,5.5f,5600,.2f));                 // spine west
 m.lights.push_back(m71Tube(12.f,11.5f,roof,.7f,5.f,5600,.2f,.55f));              // over the transfer mouth, failing
 m71DeadTube(m,15.6f,11.5f,f,roof);                                                // the dark stretch
 m.lights.push_back(m71Tube(20.7f,11.5f,roof,.75f,5.5f,5200,.15f));               // vestibule
 m.lights.push_back(m71Tube(2.6f,4.4f,roof,.7f,6.f,4600,.15f));                   // waiting room
 m71DeadTube(m,5.9f,5.6f,f,roof);
 m.lights.push_back(m71Tube(11.2f+.15f,8.3f,roof,.75f,4.5f,6400,.05f));           // c2, lit cold
 {auto l=m71Tube(16.35f,8.3f,roof,.8f,4.5f,6000,.1f,.45f);m.lights.push_back(l);}  // c4, failing
 m71DeadTube(m,9.05f,8.3f,f,roof);m71DeadTube(m,13.95f,8.3f,f,roof);
 m.lights.push_back(m71Tube(12.4f,5.9f,roof,.45f,4.f,4200,.1f));                  // technician corridor
 m71DeadTube(m,10.f,3.f,f,roof);m71DeadTube(m,19.2f,5.2f,f,roof);m71DeadTube(m,21.6f,5.2f,f,roof);
 m.lights.push_back(m71Tube(14.9f,3.f,roof,.65f,5.f,5000,.1f));                   // dosimeter store
 m.lights.push_back(m71Tube(3.3f,16.6f,roof,.6f,5.f,4800,.1f));                   // reception office
 m.lights.push_back(m71Tube(3.3f,20.4f,roof,.55f,4.5f,4300,.3f,.2f));             // staff WC
 m.lights.push_back(m71Tube(7.8f,16.4f,roof,.4f,4.5f,3400,0));                    // hold night light
 m.lights.push_back(m71Tube(12.f,15.f,roof,.6f,5.f,5600,.2f));                    // gurney park
 m.lights.push_back(m71Tube(19.f,16.2f,roof,.65f,5.5f,5000,.1f));                 // nurses' station
 m.lights.push_back(m71Tube(20.f,20.8f,roof,.6f,5.f,4000,.05f));                  // staff room
 m.lights.push_back(lamp({9.8f,14.2f},f+2.4f,.3f,3.f,1200,-.3f,WorldLightMount::Wall,kPi));    // ward call, red
 m.lights.push_back(lamp({9.8f,19.4f},f+2.2f,.45f,4.f,3400,0,WorldLightMount::Wall,kPi));       // hold night lamp
 m.lights.push_back(m71Switched(lamp({11.f,22.7f},f+2.8f,.8f,5.f,1000,-.5f,WorldLightMount::Wall,-kPi*.5f),override_,true));
 m.lights.push_back(m71Switched(lamp({13.f,22.7f},f+2.8f,.8f,5.f,5600,.2f,WorldLightMount::Wall,-kPi*.5f),override_));
 // --- The triage board: the board that sorted people ----------------------------------------------
 // 24 name strips, nine wiped; LAB 4's (column 4, row 3 from the top) still lit red.
 wall(m,7.2f,12.94f,9.95f,13.f,f+1.f,f+2.35f,20);
 for(float x:{7.17f,9.92f})wall(m,x,12.91f,x+.03f,13.f,f+.97f,f+2.38f,20);
 for(float z:{.97f,2.35f})wall(m,7.17f,12.91f,9.95f,13.f,f+z,f+z+.03f,20);
 {const bool wiped[6][4]={{0,0,1,0},{0,1,0,0},{1,0,0,0},{0,0,1,1},{1,0,0,1},{0,1,0,1}};
  const bool tier3[6][4]={{0,1,0,0},{0,0,0,1},{0,0,1,1},{0,1,0,0},{0,1,0,0},{1,0,0,0}};
  for(int j=0;j<6;++j)for(int i=0;i<4;++i)if(!wiped[j][i]){const float x=7.85f+.59f*i,z=f+2.03f-.19f*j;
   mdFix(m,model::lab_status_display,x,12.917f,z,.55f,.046f,.16f,0,false);
   wall(m,x-.27f,12.884f,x-.2f,12.893f,z+.03f,z+.13f,tier3[j][i]?19:18);}}   // tier tags: yellow held, red moved
 wall(m,7.6f,12.885f,9.3f,12.89f,f+1.5f,f+1.525f,8);wall(m,8.2f,12.885f,9.9f,12.89f,f+1.31f,f+1.33f,8);  // marker struck through two rows
 m.lights.push_back(lamp({9.62f,12.78f},f+1.73f,.5f,1.6f,1200,-.4f,WorldLightMount::Bare));
 mdFix(m,model::med_fluoro_light,8.6f,12.945f,f+2.39f,.11f,1.2f,.127f,kPi*.5f,false);
 m.lights.push_back(lamp({8.6f,12.86f},f+2.36f,.95f,3.8f,7500,-.1f,WorldLightMount::Bare));
 taped(m,{7.42f,12.94f},f+1.55f,kPi,Readable::Note,"TRIAGE BOARD / TONIGHT","TIER 2 HELD FOR SCREENING: 31. TIER 3 MOVED TO OBSERVATION: 9.",
  "LAB 4 / ENERGY DIAGNOSTICS / HELD FOR OBSERVATION / WARD C.");m.terminals.back().activateState=stateId("medf_board_read");
 // Reading it: two knocks from behind the Ward C door, once.
 {TimedSequence s;s.timerState=stateId("medf_board_read");s.finishState=stateId("medf_board_knocked");s.x1=-9;s.y1=-9;s.x2=-8;s.y2=-8;
  s.durationMs=2000;s.finishAtMs=1900;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);}
 cue(m,"medf_board_read",600,Sound::Metal1,{12.f,23.4f},"",.5f,f+1.2f);
 cue(m,"medf_board_read",1300,Sound::Metal1,{12.f,23.4f},"",.5f,f+1.2f);
 // --- L, the lobby: you are processed here ------------------------------------------------------
 // Queue belts toward the window, one post knocked flat.
 for(float x:{2.6f,3.8f})for(float y:{12.f,13.4f}){if(x>3.f&&y<12.5f)continue;
  wall(m,x-.13f,y-.13f,x+.13f,y+.13f,f,f+.03f,8);wall(m,x-.03f,y-.03f,x+.03f,y+.03f,f+.03f,f+.98f,20);}
 wall(m,2.585f,12.f,2.615f,13.4f,f+.84f,f+.9f,19);
 wall(m,3.6f,11.6f,3.66f,12.5f,f,f+.06f,20);wall(m,3.47f,12.37f,3.73f,12.63f,f,f+.03f,8);wall(m,3.64f,12.5f,3.8f,12.53f,f+.01f,f+.04f,19);
 mdFix(m,model::lab_id_scanner,1.4f,9.03f,f+1.1f,.3f,.06f,.41f,kPi,false);
 mdFix(m,model::trash_can,5.3f,9.32f,f,.4f,.4f,.5f,.3f,true);
 // --- N1, the waiting room: told it was routine, and held -----------------------------------------
 for(float y:{3.2f,4.4f,5.6f,6.8f})mdFix(m,model::platform_chair,2.4f+(y==5.6f?.12f:0),y,f,1.8f,.5f,.8f,(y==5.6f?.12f:0),true);
 for(float y:{3.2f,4.4f})mdFix(m,model::platform_chair,6.f,y,f,1.8f,.5f,.8f,(y==4.4f?-.2f:0),true);
 mdFix(m,model::platform_chair,5.9f,7.5f,f,1.8f,.5f,.8f,-.35f,true);              // shoved out of line in the rush
 mdFix(m,model::lab_status_display,2.6f,1.058f,f+2.15f,1.386f,.116f,.4f,kPi,false); // the call display
 m.lights.push_back(lamp({2.6f,1.15f},f+2.35f,.35f,2.5f,9000,0,WorldLightMount::Bare));m.lights.back().flicker=.1f;
 wall(m,5.3f,1.f,5.9f,1.45f,f+1.96f,f+2.f,20);wall(m,5.58f,1.f,5.62f,1.4f,f+1.7f,f+1.96f,20);
 mdFix(m,model::cctv_tv,5.6f,1.23f,f+2.f,.5f,.4f,.42f,0,false);                    // the safety film
 m.lights.push_back(lamp({5.6f,2.3f},f+2.15f,.35f,3.f,7000,.1f,WorldLightMount::Bare));m.lights.back().flicker=.15f;
 mdFix(m,model::med_wall_clock,4.1f,1.016f,f+2.3f,.378f,.032f,.373f,0,false);
 mdFix(m,model::lab_pa_horn,6.85f,1.29f,f+2.55f,.495f,.564f,.433f,kPi,false);
 cue(m,clock,30000,Sound::Empty,{6.85f,1.4f},"PA: TIER TWO, REMAIN SEATED. YOUR SHIFT PAY CONTINUES.",.4f,f+2.6f);
 mdFix(m,model::med_notice_board,1.04f,4.4f,f+1.3f,1.2f,.07f,.9f,mdE,false);
 mdFix(m,model::med_notice_board,1.04f,6.9f,f+1.3f,1.2f,.07f,.9f,mdE,false);
 taped(m,{7.4f,4.6f},f+1.45f,-kPi*.5f,Readable::Note,"ROUTINE MONITORING / A NOTE FOR STAFF","YOUR BAND KEEPS YOU SAFE. A SCREENING CALL IS ROUTINE, NOT A CONCERN.",
  "IF YOU ARE ASKED TO STAY FOR OBSERVATION, YOUR SHIFT PAY CONTINUES. YOUR FAMILY WILL BE CALLED.");
 mdFix(m,model::water_cooler,7.2f,1.6f,f,.35f,.35f,1.1f,mdW,true);
 mdFix(m,model::trash_can,7.1f,2.45f,f,.45f,.45f,.56f,.2f,true);
 mdFix(m,model::lab_potted_plant,6.97f,8.4f,f,.498f,.578f,.927f,.4f,true);
 mdFix(m,model::wooden_table,4.2f,2.f,f,1.094f,.579f,.624f,.06f,true);
 mdFix(m,model::office_paper_stack,3.85f,1.95f,f+.624f,.33f,.44f,.03f,.3f,false);
 // What people were holding when they were called: bands, badges, a mug, a coat of paper.
 for(auto [x,y,yaw]:{std::tuple{4.05f,4.9f,.4f},{4.2f,5.05f,1.9f},{4.35f,4.85f,2.8f},{3.95f,5.15f,.9f},{4.5f,5.2f,2.2f},{4.25f,5.3f,.1f}})mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{3.9f,3.8f,.7f},{4.6f,6.3f,2.4f},{2.2f,7.5f,1.3f}})mdFix(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{3.6f,6.1f,.3f},{4.8f,3.6f,2.1f},{4.4f,7.6f,1.f},{3.7f,8.2f,2.7f},{4.9f,5.8f,.6f}})mdFix(m,model::office_paper_sheet,x,y,f+.001f,.4f,.293f,.004f,yaw,false);
 mdFix(m,model::mug,1.75f,3.05f,f+.42f,.09f,.09f,.1f,.0f,false);mdFix(m,model::mug,4.75f,6.9f,f,.09f,.09f,.1f,1.f,false);
 mdFix(m,model::office_file_binder,6.6f,3.35f,f+.42f,.093f,.301f,.424f,.2f,false);
 // --- N2, the cubicles: what the sweep did -----------------------------------------------------
 for(int i=0;i<4;++i){const float L=cubL[i],R=cubR[i];
  mdFix(m,model::lab_control_board,L+1.65f,6.875f,f+1.f,1.056f,.15f,.48f,kPi,false);
  mdFix(m,model::med_nurse_desk,R-.281f,8.75f,f,1.179f,.562f,.808f,mdW,true);
  mdFix(m,model::lab_field_meter,R-.3f,8.45f,f+.808f,.25f,.328f,.148f,mdW+.3f,false);
  mdFix(m,model::item_dosimeter,R-.28f,8.95f,f+.808f,.31f,.146f,.066f,mdW-.2f,false);
  mdFix(m,model::med_pill_bottle,R-.2f,9.2f,f+.808f,.07f,.03f,.13f,0,false);
  mdFix(m,model::trash_can,R-.25f,9.58f,f,.35f,.35f,.43f,.2f,true);
  mdFix(m,model::lab_test_chamber,L+1.45f,7.5f,f+1.78f,.684f,.408f,.384f,0,false);   // the detector head
  m.pipes.push_back({{L+1.45f,7.5f},{L+1.45f,7.5f},f+2.16f,.04f,roof,6});
  for(float z:{2.16f,3.17f})wall(m,L+1.38f,7.43f,L+1.52f,7.57f,f+z,f+z+.03f,20);}
 mdFix(m,model::lab_boardroom_chair,9.05f,7.75f,f,.741f,.7f,.867f,0,true);            // c1: shut, tidy, next
 mdFix(m,model::office_paper_stack,9.65f,8.3f,f+.808f,.33f,.44f,.03f,mdW,false);
 // c2: the door left open, the light on; the doctor across the threshold.
 mdFix(m,model::lab_boardroom_chair,11.5f,7.7f,f,.741f,.7f,.867f,.5f,true);
 mdBody(m,clock,ActorVisual::FemaleDoctor,{10.75f,9.4f},f,kPi);                 // head in the booth, feet on the sill
 mdBlood(m,10.4f,8.35f,11.1f,9.25f,f);mdBlood(m,10.55f,9.25f,10.95f,10.55f,f);
 mdFix(m,model::med_vitals_monitor,11.45f,8.55f,f,.38f,.181f,.28f,1.1f,false);
 // c3: the chair kicked into the corner; the meter on the floor.
 mdFix(m,model::lab_boardroom_chair,14.15f,7.4f,f,.741f,.7f,.867f,2.6f,true);
 mdFix(m,model::lab_field_meter,13.9f,8.9f,f,.25f,.328f,.148f,1.2f,false);
 // c4: the panel shot out from the spine, the technician inside.
 mdFix(m,model::lab_boardroom_chair,16.55f,7.35f,f,.741f,.7f,.867f,.9f,true);
 mdBody(m,clock,ActorVisual::FemaleScientist,{15.75f,8.2f},f,-.8f);mdBlood(m,15.1f,7.7f,16.1f,8.9f,f);
 wall(m,14.9f,7.55f,14.91f,8.55f,f+.75f,f+1.55f,16);                                // on the partition beside her
 for(auto [x,y,yaw]:{std::tuple{16.4f,10.2f,0.f},{16.75f,10.45f,0.f},{16.55f,10.75f,0.f},{16.95f,10.25f,0.f},{17.05f,10.6f,0.f},{16.3f,10.55f,0.f}})
  wall(m,x-.04f,y-.025f,x+.04f,y+.025f,f+.002f,f+.006f,17);
 mdFix(m,model::med_first_aid_box,12.5f,10.06f,f+1.4f,.367f,.12f,.349f,0,false);
 extinguisher(m,14.9f,10.f,f,0);
 mdFix(m,model::pwr_breaker_panel,6.6f,10.08f,f+1.2f,.288f,.16f,.736f,0,false);
 // --- N3, the technician corridor: the staff side nobody watches ---------------------------------
 mdFix(m,model::med_crash_cart,12.9f,5.46f,f,.736f,.483f,.999f,0,true);
 cableTray(m,{7.6f,5.42f},{17.2f,5.42f},f+2.75f);
 for(float x=8.4f;x<17.f;x+=1.8f)for(float y:{5.24f,5.6f})m.pipes.push_back({{x,y},{x,y},f+2.85f,.01f,roof,6});
 // --- N4, the counting room: the numbers --------------------------------------------------------
 mdFix(m,model::lab_mainframe,8.54f,1.97f,f,1.867f,1.94f,2.706f,kPi,true);
 terminal(m,{10.9f,1.4f},"WHOLE-BODY COUNT / SHIFT 3","02:44 LAB 4 (ENERGY DIAGNOSTICS): RECOUNT 4.1. TIER 3. ESCORT REQUESTED.",
  "03:58 ALL COUNTS FORWARDED TO INTERNAL INVESTIGATIONS. LOCAL COPIES TO BE DESTROYED.");m.terminals.back().yaw=0;
 for(float y:{2.35f,3.7f}){mdFix(m,model::lab_clean_bench,12.119f,y,f,1.179f,.562f,.808f,kPi*.5f,true);
  mdFix(m,model::lab_instrument_terminal,12.16f,y,f+.808f,.826f,.421f,.616f,kPi*.5f,false);}
 m.lights.push_back(lamp({11.7f,2.35f},f+1.3f,.5f,3.f,9000,-.2f,WorldLightMount::Bare));
 mdFix(m,model::office_chair,11.35f,2.4f,f,.55f,.55f,.95f,-kPi*.5f,true);
 mdFix(m,model::office_chair,11.1f,3.95f,f,.55f,.55f,.95f,-kPi*.5f+.6f,true);
 mdFix(m,model::lab_photocopier,7.98f,4.f,f,.826f,.752f,.967f,-kPi*.5f,true);
 for(float dz:{0.f,.03f})mdFix(m,model::office_paper_stack,7.95f,3.95f+dz*3,f+.967f+dz,.33f,.44f,.03f,.2f+dz*10,false);
 for(auto [x,y,yaw]:{std::tuple{8.7f,4.3f,.4f},{9.1f,3.6f,1.7f},{8.9f,4.7f,2.6f}})mdFix(m,model::office_paper_sheet,x,y,f+.001f,.4f,.293f,.004f,yaw,false);
 mdFix(m,model::prop_cardboard_box_open,10.5f,4.4f,f,.73f,.725f,.726f,.25f,true);
 // --- N5, the dosimeter store --------------------------------------------------------------------
 // Shelf boards (metal_shelf, 1.8 m): .225, .975 and 1.65. Band boxes below, loose bands on top,
 // the gaps where the detail took what it needed.
 for(float x:{13.7f,15.65f}){shelf(m,{x,1.27f},0);
  for(int k=0;k<4;++k)if(!(x>15.f&&k==2))mdFix(m,model::item_dosimeter,x-.6f+.4f*k,1.27f,f+1.65f,.31f,.146f,.066f,.05f*k,false);
  for(int k=0;k<3;++k)if(!(x<15.f&&k==1))mdFix(m,model::prop_cardboard_box,x-.5f+.5f*k,1.27f,f+.975f,.436f,.432f,.22f,.06f*k-.05f,false);
  for(int k=0;k<3;++k)if(!(x>15.f&&k==0))mdFix(m,model::prop_cardboard_box,x-.5f+.5f*k,1.27f,f+.225f,.436f,.432f,.22f,.04f-.05f*k,false);
  for(int k=0;k<2;++k)mdFix(m,model::item_dosimeter,x-.45f+.95f*k,1.25f,f+1.195f,.31f,.146f,.066f,.3f*k,false);}
 shelf(m,{16.95f,3.4f},-kPi*.5f);
 mdFix(m,model::prop_cardboard_box,16.95f,3.f,f+1.65f,.436f,.432f,.22f,.1f,false);
 mdFix(m,model::prop_cardboard_box,16.95f,3.75f,f+1.65f,.436f,.432f,.22f,-.15f,false);
 for(float y:{2.9f,3.4f,3.9f})mdFix(m,model::prop_cardboard_box,16.95f,y,f+.225f,.436f,.432f,.22f,.1f*y,false);
 mdFix(m,model::prop_cardboard_box,16.95f,3.15f,f+.975f,.436f,.432f,.22f,.05f,false);
 for(float y:{3.6f,3.95f})mdFix(m,model::item_dosimeter,16.95f,y,f+.975f,.31f,.146f,.066f,kPi*.5f+.2f*y,false);
 for(auto [x,y,yaw]:{std::tuple{14.1f,1.85f,.6f},{14.35f,2.05f,2.1f},{15.9f,1.9f,1.3f},{16.4f,2.6f,2.7f}})mdFix(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw,false);
 mdFix(m,model::med_records_cabinet,12.85f,3.f,f,.9f,.493f,1.f,mdE,true);
 for(auto [x,y,yaw]:{std::tuple{13.9f,3.5f,.2f},{14.85f,2.75f,-.3f},{15.7f,3.6f,.5f}})mdFix(m,model::prop_cardboard_box,x,y,f,.727f,.72f,.367f,yaw,true);
 mdFix(m,model::prop_cardboard_box_open,16.f,4.55f,f,.73f,.725f,.726f,1.1f,true);
 for(auto [x,y,yaw]:{std::tuple{15.3f,4.55f,.4f},{15.15f,4.3f,1.4f},{15.45f,4.75f,2.5f},{15.2f,4.85f,.9f}})mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 // --- N6, records and the burn: burying the records ----------------------------------------------
 mdFix(m,model::office_bookcase,18.42f,1.25f,f,2.025f,.5f,2.5f,0,true);
 mdFix(m,model::office_file_cabinet_open,20.25f,1.35f,f,1.451f,.699f,1.054f,0,true);
 mdFix(m,model::office_bookcase,21.99f,1.25f,f,2.025f,.5f,2.5f,0,true);
 mdFix(m,model::med_records_cabinet,22.75f,2.6f,f,.9f,.493f,1.f,mdW,true);
 for(float y:{3.35f,3.85f,4.35f})mdFix(m,model::file_cabinet_tall,22.7f,y,f,.5f,.6f,1.3f,mdW,true);
 mdFix(m,model::med_records_cabinet,22.75f,5.1f,f,.9f,.493f,1.f,mdW,true);
 mdFix(m,model::scrap_burn_barrel,20.f,3.4f,f,.781f,.842f,.887f,0,true);
 hazard(m,Hazard::Kind::Fire,19.75f,3.15f,20.25f,3.65f,f+.85f,f+1.3f,8);
 {auto l=lamp({20.f,3.4f},f+1.35f,1.1f,6.f,1800,-.3f,WorldLightMount::Bare);l.flicker=.35f;m.lights.push_back(l);}
 mdFix(m,model::prop_cardboard_box_open,21.05f,3.95f,f,.73f,.725f,.726f,.35f,true);
 mdFix(m,model::prop_cardboard_box_open,18.95f,2.75f,f,.73f,.725f,.726f,-.4f,true);
 for(auto [x,y,yaw]:{std::tuple{19.3f,3.9f,.3f},{19.0f,4.4f,1.2f},{20.5f,4.65f,2.f},{21.2f,4.9f,.7f},{19.6f,4.9f,2.6f},{18.6f,3.8f,1.f},
  {20.3f,5.4f,.2f},{21.6f,5.6f,1.5f},{19.1f,5.7f,2.3f},{18.4f,5.f,.5f},{20.9f,6.4f,1.9f},{19.8f,6.6f,.9f},{22.f,6.6f,.4f},{18.9f,7.3f,2.9f}})
  mdFix(m,model::office_paper_sheet,x,y,f+.001f,.4f,.293f,.004f,yaw,false);
 terminal(m,{20.25f,1.45f},"COUNT SHEET (CHARRED)","...TIER 3: 9 / ESCORT TO WARD C 02:50 / DETAIL 4...","...LAB 4 / BED...");
 {auto& t=m.terminals.back();t.readable=Readable::Note;t.surface=1.054f;t.yaw=0;}
 // Records, the south strip: what is still waiting for the barrel.
 for(float y:{6.25f,7.05f})mdFix(m,model::prop_cardboard_box,22.6f,y,f,.727f,.72f,.367f,.08f*y-.5f,true);
 mdFix(m,model::prop_cardboard_box,22.58f,6.3f,f+.367f,.727f,.72f,.367f,.08f,false);
 mdFix(m,model::prop_cardboard_box_open,21.55f,7.5f,f,.73f,.725f,.726f,-.3f,true);
 for(auto [x,y,yaw]:{std::tuple{18.1f,7.2f,.1f},{18.12f,8.f,-.08f}})mdFix(m,model::prop_cardboard_box,x,y,f,.727f,.72f,.367f,yaw,true);
 mdFix(m,model::office_file_binder,18.2f,7.25f,f+.367f,.093f,.301f,.424f,1.2f,false);
 // --- V, the vestibule: the post the detail holds -----------------------------------------------
 for(float x:{20.95f,21.87f})mdFix(m,model::med_records_cabinet,x,9.25f,f,.9f,.493f,1.f,mdS,true);
 mdFix(m,model::item_walkie,20.8f,9.28f,f+1.f,.114f,.376f,.046f,.4f,false);
 mdFix(m,model::mug,21.25f,9.2f,f+1.f,.09f,.09f,.1f,0,false);
 mdFix(m,model::office_file_binder,21.7f,9.15f,f+1.f,.093f,.301f,.424f,kPi*.5f,false);
 mdFix(m,model::med_pill_bottle,22.05f,9.3f,f+1.f,.07f,.03f,.13f,0,false);
 mdFix(m,model::med_first_aid_box,20.95f,9.06f,f+1.4f,.367f,.12f,.349f,mdS,false);
 extinguisher(m,22.62f,9.f,f,mdS);
 for(float x:{19.05f,19.97f})mdFix(m,model::med_records_cabinet,x,13.75f,f,.9f,.493f,1.f,mdN,true);
 mdFix(m,model::office_paper_stack,19.1f,13.75f,f+1.f,.33f,.44f,.03f,.2f,false);
 mdFix(m,model::ind_crate,20.35f,12.45f,f,.81f,.81f,.812f,.12f,true);                     // the barricade
 mdFix(m,model::prop_cardboard_box,20.3f,12.5f,f+.812f,.727f,.72f,.367f,-.25f,false);
 mdFix(m,model::prop_cardboard_box_open,20.85f,13.4f,f,.73f,.725f,.726f,.1f,true);          // the ammunition, gone
 mdFix(m,model::wooden_chair,21.1f,12.35f,f,.417f,.525f,.95f,mdW,true);
 for(auto [x,y,yaw]:{std::tuple{19.6f,10.4f,.5f},{21.4f,10.9f,2.2f},{22.2f,12.6f,1.1f},{19.2f,12.3f,2.8f}})mdFix(m,model::office_paper_sheet,x,y,f+.001f,.4f,.293f,.004f,yaw,false);
 // --- S1, the reception office: the call list -------------------------------------------------
 mdFix(m,model::med_nurse_desk,1.75f,14.5f,f,1.179f,.562f,.808f,0,true);
 terminal(m,{3.2f,14.62f},"RECEPTION / CALL LIST","02:41 CUBICLE 2: LAB 4. 02:44 CUBICLE 2: RECOUNT.","03:10 CALLS SUSPENDED. HOLD THE ROOM. DO NOT ANSWER QUESTIONS.");m.terminals.back().yaw=0;
 mdFix(m,model::crt_monitor,1.55f,14.48f,f+.808f,.42f,.4f,.38f,0,false);
 mdFix(m,model::keyboard,1.55f,14.66f,f+.808f,.45f,.16f,.03f,0,false);
 mdFix(m,model::desk_phone,2.15f,14.5f,f+.808f,.2f,.2f,.1f,0,false);
 mdFix(m,model::lab_desk_lamp,1.2f,14.55f,f+.808f,.18f,.398f,.521f,.3f,false);
 m.lights.push_back(lamp({1.25f,14.7f},f+1.25f,.4f,2.5f,3000,0,WorldLightMount::Bare));
 mdFix(m,model::mug,2.25f,14.75f,f+.808f,.09f,.09f,.1f,0,false);
 mdFix(m,model::office_chair,1.8f,15.3f,f,.55f,.55f,.95f,.3f,true);
 mdFix(m,model::office_chair,4.2f,16.6f,f,.55f,.55f,.95f,2.2f,true);
 mdFix(m,model::file_cabinet_wide,1.25f,16.6f,f,.9f,.5f,.75f,kPi*.5f,true);
 mdFix(m,model::office_paper_stack,1.25f,16.6f,f+.75f,.33f,.44f,.03f,.2f,false);
 mdFix(m,model::med_wall_clock,5.585f,16.2f,f+2.2f,.236f,.02f,.233f,mdW,false);
 mdFix(m,model::med_notice_board,3.4f,18.565f,f+1.3f,1.2f,.07f,.9f,mdN,false);
 // --- S1b, the staff WC and sluice -------------------------------------------------------------
 for(float x:{2.07f,4.2f})mdFix(m,model::med_washroom_stall,x,22.23f,f,2.13f,1.542f,2.4f,mdN,true);
 for(float y:{19.75f,20.55f})mdFix(m,model::med_scrub_sink,5.33f,y,f,.488f,.536f,1.127f,mdW,true);
 mdFix(m,model::med_washroom_mirror,5.585f,20.15f,f+1.3f,1.2f,.032f,.8f,mdW,false);
 mdFix(m,model::med_floor_drain,3.3f,20.4f,f,.5f,.5f,.01f,0,false);
 mdFix(m,model::med_first_aid_box,1.06f,20.3f,f+1.3f,.367f,.12f,.349f,mdE,false);
 mdFix(m,model::trash_can,4.95f,19.15f,f,.4f,.4f,.5f,0,true);
 // --- S2, the tier-3 hold: strapped down to wait for the transfer ---------------------------------
 for(auto [y,tossed]:{std::pair{15.f,false},{17.8f,true},{20.6f,false}}){
  if(tossed)mdFix(m,model::med_ward_bed_tossed,6.85f,y,f,2.097f,1.525f,.842f,kPi,true);
  else mdFix(m,model::med_ward_bed,6.79f,y,f,1.984f,.944f,.842f,kPi,true);
  if(!tossed)for(float x:{6.6f,7.4f})wall(m,x,y-.48f,x+.06f,y+.48f,f+.842f,f+.852f,12);
  else{wall(m,7.f,y+.6f,7.06f,y+.64f,f+.25f,f+.84f,12);wall(m,7.6f,y-.62f,7.66f,y-.58f,f+.35f,f+.84f,12);}
  wall(m,5.8f,y-.25f,6.05f,y+.25f,f+1.27f,f+1.3f,20);
  mdFix(m,model::med_vitals_monitor,5.92f,y,f+1.3f,.38f,.181f,.28f,mdE,false);
  const float ix=6.15f,iy=y-(tossed?1.f:.72f);                                                     // the IV stand
  for(float d:{0.f,1.f})wall(m,ix-.2f+d*.17f,iy-.025f-d*.17f,ix+.2f-d*.17f,iy+.025f+d*.17f,f,f+.03f,20);
  wall(m,ix-.012f,iy-.012f,ix+.012f,iy+.012f,f+.03f,f+1.9f,20);
  wall(m,ix-.01f,iy-.08f,ix+.01f,iy+.08f,f+1.88f,f+1.9f,20);
  if(!tossed)mdFix(m,model::med_blood_bag,ix,iy+.05f,f+1.72f,.11f,.021f,.143f,mdE,false);}
 mdFix(m,model::med_blood_bag,8.3f,18.9f,f,.11f,.021f,.143f,1.f,false);
 mdBlood(m,7.75f,17.5f,8.35f,18.5f,f);mdBlood(m,8.1f,18.5f,8.6f,20.f,f);mdBlood(m,8.45f,20.f,9.f,21.4f,f);mdBlood(m,8.8f,21.4f,9.8f,22.3f,f);
 mdFix(m,model::med_wheelchair,9.3f,14.f,f,.554f,1.035f,.905f,.25f,true);
 mdFix(m,model::med_specimen_jar,9.5f,20.f,f,.102f,.102f,.172f,0,false);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{8.4f,17.8f},-999,1,0,stateId("medf_hold_wake")});
 // --- S3, the transfer passage: the way the tier-3s went ----------------------------------------
 for(float y:{14.5f,15.9f})mdFix(m,model::med_ward_bed,12.95f,y,f,1.984f,.944f,.842f,0,true);   // the gurney park
 mdFix(m,model::med_wheelchair,10.6f,16.25f,f,.554f,1.035f,.905f,-.35f,true);
 mdFix(m,model::med_scrub_sink,10.27f,13.9f,f,.488f,.536f,1.127f,mdE,true);
 taped(m,{10.f,14.55f},f+1.45f,kPi*.5f,Readable::Note,"TIER 3 TO WARD C / 02:50 / 9 TRANSFERRED","ESCORT: DETAIL 4. SEDATION PER ORDER. DO NOT REOPEN FROM THIS SIDE.","");
 mdFix(m,model::lab_id_scanner,13.97f,22.4f,f+1.1f,.3f,.06f,.41f,kPi*.5f,false);
 wallSign(m,10.055f,20.6f,f+1.55f,"WARD C / OBSERVATION","MEDICAL OVERRIDE ONLY",1.6f,kPi*.5f);
 mdFix(m,model::bench,10.25f,18.35f,f,1.86f,.44f,.63f,kPi*.5f,true);
 pipeRoute(m,{P3{13.3f,21.8f,roof},P3{13.3f,21.8f,f+2.25f}},.025f,5,roof);
 wall(m,13.18f,21.68f,13.42f,21.92f,f+2.2f,f+2.24f,20);
 mdFix(m,model::med_floor_drain,13.3f,21.8f,f,.5f,.5f,.01f,0,false);
 for(auto [x,y,yaw]:{std::tuple{11.2f,20.6f,.6f},{12.4f,19.1f,2.f}})mdFix(m,model::office_paper_sheet,x,y,f+.001f,.4f,.293f,.004f,yaw,false);
 // --- S4, the nurses' station: the orders ------------------------------------------------------
 for(float x:{15.2f,16.6f})mdFix(m,model::med_nurse_desk,x,13.48f,f,1.179f,.562f,.808f,0,true);
 mdFix(m,model::crt_monitor,15.55f,13.5f,f+.808f,.42f,.4f,.38f,0,false);
 mdFix(m,model::lab_desk_lamp,14.78f,13.42f,f+.808f,.18f,.398f,.521f,-.3f,false);
 m.lights.push_back(lamp({14.9f,13.65f},f+1.25f,.4f,2.5f,3000,0,WorldLightMount::Bare));
 terminal(m,{15.08f,13.58f},"STANDING ORDER / 03:20","SEDATE ALL TIER 3 FOR TRANSFER. WHOLE SHIFTS IF NECESSARY.","AUTH: INTERNAL INVESTIGATIONS. PHARMACY TO ISSUE ON SIGHT.");
 {auto& t=m.terminals.back();t.readable=Readable::Letter;t.surface=.808f;t.yaw=0;}
 for(int k=0;k<8;++k)mdFix(m,model::med_pill_bottle,16.2f+.09f*k,13.4f+.12f*(k%2),f+.808f,.07f,.03f,.13f,.4f*k,false);
 for(float x:{16.95f,17.08f})mdFix(m,model::med_specimen_jar,x,13.55f,f+.808f,.102f,.102f,.172f,0,false);
 mdFix(m,model::office_chair,14.95f,14.4f,f,.55f,.55f,.95f,.2f,true);
 mdFix(m,model::office_chair,17.6f,14.7f,f,.55f,.55f,.95f,-.7f,true);
 mdBody(m,clock,ActorVisual::FemaleDoctor,{16.35f,14.25f},f,kPi*.5f+.1f);mdBlood(m,15.6f,13.9f,17.1f,14.75f,f);
 mdFix(m,model::lab_control_board,18.33f,13.7f,f+1.1f,.924f,.131f,.42f,kPi*.5f,false);   // ward call
 m.lights.push_back(lamp({18.2f,13.6f},f+1.4f,.15f,.8f,1200,-.3f,WorldLightMount::Bare));
 mdFix(m,model::lab_sample_fridge,22.575f,15.6f,f,.7f,.85f,1.f,kPi*.5f,true);
 mdFix(m,model::med_supply_cabinet,22.778f,16.95f,f,1.1f,.444f,1.7f,mdW,true);
 for(float x:{15.5f,16.45f})mdFix(m,model::med_records_cabinet,x,18.15f,f,.9f,.493f,1.f,mdN,true);
 mdFix(m,model::med_notice_board,18.f,18.365f,f+1.3f,1.2f,.07f,.9f,mdN,false);
 mdFix(m,model::wooden_table,20.f,16.4f,f,1.367f,.724f,.78f,.05f,true);
 mdFix(m,model::wooden_chair,19.6f,15.7f,f,.417f,.525f,.95f,.15f,true);mdFix(m,model::wooden_chair,20.5f,17.15f,f,.417f,.525f,.95f,kPi-.4f,true);
 mdFix(m,model::office_file_binder,19.7f,16.35f,f+.78f,.093f,.301f,.424f,kPi*.5f,false);
 mdFix(m,model::med_specimen_jar,20.4f,16.5f,f+.78f,.102f,.102f,.172f,0,false);
 mdFix(m,model::office_paper_stack,20.2f,16.25f,f+.78f,.33f,.44f,.03f,.4f,false);
 mdFix(m,model::med_crash_cart,21.4f,17.6f,f,.736f,.483f,.999f,.25f,true);
 // --- S5, linen and O2 ----------------------------------------------------------------------
 shelf(m,{15.6f,22.73f},0);shelf(m,{14.45f,21.6f},kPi*.5f);
 // Shelf boards (metal_shelf, 1.8 m): .225, .975 and 1.65.
 for(float x:{15.f,15.6f,16.2f})mdFix(m,model::prop_cardboard_box,x,22.73f,f+1.65f,.436f,.432f,.22f,.1f*x,false);
 for(float x:{15.05f,15.55f})mdFix(m,model::prop_cardboard_box,x,22.73f,f+.975f,.436f,.432f,.22f,-.1f*x,false);
 for(float x:{15.1f,16.1f})mdFix(m,model::prop_cardboard_box,x,22.73f,f+.225f,.436f,.432f,.22f,.4f-.2f*x,false);
 for(float y:{21.f,22.f})mdFix(m,model::prop_cardboard_box,14.45f,y,f+1.65f,.436f,.432f,.22f,.3f,false);
 for(float y:{20.95f,21.45f,22.2f})mdFix(m,model::prop_cardboard_box,14.45f,y,f+(y>22.f?.225f:.975f),.436f,.432f,.22f,.2f*y,false);
 m.lights.push_back(m71Tube(15.5f,20.6f,roof,.3f,4.f,4000,.05f));                    // linen, dim
 for(int k=0;k<3;++k)mdFix(m,model::med_mattress_stained,15.5f+.04f*k,19.05f-.02f*k,f+.106f*k,1.71f,.81f,.106f,.03f*k,true);
 for(float y:{20.95f,21.35f,21.75f,22.15f})mdFix(m,model::gas_cylinder,16.6f,y,f,.33f,.33f,1.58f,0,true);
 wall(m,16.4f,20.75f,16.43f,22.35f,f+1.15f,f+1.2f,8);for(float y:{20.75f,22.32f})wall(m,16.4f,y,16.8f,y+.03f,f+1.15f,f+1.2f,8);
 // --- S6, the staff room: the squad's base ----------------------------------------------------
 for(int i=0;i<4;++i)mdFix(m,model::psx_locker,18.f+.86f*i,22.68f,f,.64f,.85f,2.42f,kPi*.5f,true);
 mdFix(m,model::wooden_table,19.8f,20.9f,f,1.367f,.724f,.78f,-.05f,true);
 mdFix(m,model::wooden_chair,19.4f,20.15f,f,.417f,.525f,.95f,.1f,true);
 mdFix(m,model::wooden_chair,20.25f,20.1f,f,.417f,.525f,.95f,-.25f,true);
 mdFix(m,model::wooden_chair,19.75f,21.7f,f,.417f,.525f,.95f,kPi+.3f,true);
 mdFix(m,model::item_walkie,20.05f,20.8f,f+.78f,.114f,.376f,.046f,.6f,false);
 mdFix(m,model::mug,19.5f,21.05f,f+.78f,.09f,.09f,.1f,0,false);
 mdFix(m,model::file_cabinet_wide,22.75f,21.2f,f,.9f,.5f,.75f,-kPi*.5f,true);
 mdFix(m,model::coffee_machine,22.75f,21.1f,f+.75f,.35f,.35f,.45f,-kPi*.5f,false);
 mdFix(m,model::mug,22.7f,21.5f,f+.75f,.09f,.09f,.1f,0,false);
 mdFix(m,model::office_couch_green,21.85f,19.29f,f,2.242f,1.364f,1.192f,0,true);
 mdFix(m,model::trash_can,17.35f,21.3f,f,.45f,.45f,.56f,0,true);
 // --- The Medical detail: two frightened men short of ammunition ----------------------------
 // A, "the burner", faces into the barrel with his back to both doors.
 mdGuard(m,clock,"md_screen_a",ActorVisual::Cleanup,GuardSquadRole::Assault,"md_screen",{{{20.f,4.35f},f,12000,kPi},{{21.85f,5.4f},f,4000,kPi*.5f}},9,1,1.1f,110);
 m.actorTracks.back().ai.stance=stance;
 // B, "the watcher", watches the spine and stares down at the door he is afraid of.
 mdGuard(m,clock,"md_screen_b",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"md_screen",
  {{{20.6f,11.5f},f,6000,-kPi*.5f},{{12.f,12.4f},f,7000,0},{{8.6f,11.2f},f,3000,kPi}},14,2,1.2f);
 {auto& t=m.actorTracks.back();t.ai.stance=stance;t.talk=stateId("medf_b_hello");}
 m.dialogue.push_back({stateId("medf_b_hello"),"MEDICAL DETAIL","Stop. Don't. I've got two rounds and a radio that won't stop talking.","",{
  m71Reply("Who did the booths?","medf_b_booths"),m71Reply("What's behind the Ward C door?","medf_b_wardc"),
  m71Reply("Go. Back the way I came.","","medf_screen_b_spared")}});
 m.dialogue.push_back({stateId("medf_b_booths"),"MEDICAL DETAIL","Us. He said they were exposed. They were reading the counts out loud, so somebody stopped them reading.","",{m71Reply("Back.","medf_b_hello")}});
 m.dialogue.push_back({stateId("medf_b_wardc"),"MEDICAL DETAIL","Nine tier threes. Ten to three. Something's been knocking since four.","",{m71Reply("Back.","medf_b_hello")}});
 // --- The Investigator, over their radio: his order turns them ----------------------------------
 mdSequence(m,"md_screen_radio","md_screen_radio_done",16000,14000,1.f,9.f,3.f,14.f);
 const Vec2 radio{20.6f,11.5f};
 cue(m,"md_screen_radio",900,Sound::Empty,radio,"INVESTIGATOR (RADIO): Medical detail. The screening floor is closed. Anyone walking it is exposed.",.9f,f+1.5f,"inv_screen_1");
 cue(m,"md_screen_radio",6200,Sound::Empty,radio,"RADIO: Sir, we did the booths. There's nobody left to walk it.",.9f,f+1.5f,"md_screen_a_1");
 cue(m,"md_screen_radio",11000,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then you will not be busy.",.9f,f+1.5f,"inv_screen_2");
 // Walking in: frightened men without orders about you. The order: shoot on sight. A falls: B breaks.
 m71Event(m,"medf_screen_truce",0,9.f,3.f,14.f,"",{m71Set("medf_screen_stance",int(SquadStance::Truce))});
 m71Event(m,"medf_screen_moot",0,0,24,24,"md_screen_a_dead",{m71Set("md_screen_radio_done",2)},false);   // an order to a dead man is moot
 m71Event(m,"medf_screen_order",0,0,24,24,"md_screen_radio_done",{m71Set("medf_screen_stance",int(SquadStance::Hostile))});
 m71Event(m,"medf_screen_break",0,0,24,24,"md_screen_a_dead",{m71Set("medf_screen_stance",int(SquadStance::Truce))});
 // Back from Ward C with the override: the screen leaf stands open and the hold wakes.
 {ScriptAction open;open.type=ScriptAction::Type::OpenDoor;open.index=12;
  m71Event(m,"medf_hold_wake_go",10.f,17.f,14.f,23.f,"med_override",{m71Set("medf_hold_wake",1),open});}
 // --- Rewards, names ----------------------------------------------------------------------------
 m.pickupSpawns={{{1.6f,17.6f},PickupKind::Health,f},{{1.8f,20.8f},PickupKind::Health,f},{{21.9f,22.3f},PickupKind::Ammo,f}};
 m.clutterSpawns={{2,{5.f,13.4f},f},{2,{4.6f,1.95f},f+.624f},{1,{4.3f,2.05f},f+.624f},{0,{9.6f,6.1f},f},{2,{20.2f,21.05f},f+.78f}};
 zone(m,"MEDICAL SCREENING / RECEPTION",1.f,9.f,5.6f,18.6f);
 zone(m,"MEDICAL SCREENING / WAITING ROOM",1.f,1.f,7.4f,8.8f);
 zone(m,"MEDICAL SCREENING / CUBICLES",7.6f,6.8f,17.2f,9.8f);
 zone(m,"MEDICAL SCREENING / COUNTING ROOM",7.6f,1.f,12.4f,5.f);
 zone(m,"MEDICAL SCREENING / DOSIMETERS",12.6f,1.f,17.2f,5.f);
 zone(m,"MEDICAL SCREENING / RECORDS",17.4f,1.f,23.f,8.8f);
 zone(m,"MEDICAL SCREENING / TIER 3 HOLD",5.8f,13.2f,9.8f,23.f);
 zone(m,"MEDICAL SCREENING / TRANSFER",10.f,13.2f,14.f,23.f);
 zone(m,"MEDICAL SCREENING / NURSES' STATION",14.2f,13.2f,23.f,18.4f);
 zone(m,"MEDICAL SCREENING / STAFF ROOM",14.2f,18.6f,23.f,23.f);
 zone(m,"MEDICAL SCREENING",0,0,24,24);
 titleCard(m,"DEPTHWORKS MEDICAL",1.f,9.f,5.6f,14.f);
 ceilingGrid(m,1.2f,9.f,5.6f,14.f,roof);ceilingGrid(m,5.6f,10.f,18.4f,13.f,roof);ceilingGrid(m,18.6f,9.f,22.8f,14.f,roof);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
// --- Chunk 72 helpers (m72) -----------------------------------------------------------------
// A swinging clinic door: the renderer draws leaf, posts and head to 2.55 m; the wall closes above.
void m72Swing(Map& m,float x1,float x2,float y,float f,float roof,float swing,float open=0){
 wall(m,x1,y-.1f,x2,y+.1f,f+2.55f,roof,3);
 Door d{x1,x2,y,open};d.swinging=true;d.swing=swing;d.opening=open>0;d.maxOpen=.9f;m.doors.push_back(d);
}
// An open doorway through a wall that runs along y: pale-steel jambs and head, wall above.
void m72DoorwayY(Map& m,float y1,float y2,float x1,float x2,float f,float roof){
 wall(m,x1,y1,x2,y2,f+2.3f,roof,3);
 for(float y:{y1-.06f,y2})wall(m,x1-.03f,y,x2+.03f,y+.06f,f,f+2.3f,20);
 wall(m,x1-.03f,y1,x2+.03f,y2,f+2.26f,f+2.3f,20);
}
// A bulkhead over a wide opening in a wall along x (y1..y2 thick): painted, steel-edged.
void m72Header(Map& m,float x1,float x2,float y1,float y2,float f,float roof,float h){
 wall(m,x1,y1,x2,y2,f+h,roof,3);wall(m,x1,y1-.02f,x2,y2+.02f,f+h-.04f,f+h,20);
}
// Cubicle curtain hung under its track: fabric along x at y (or along y at x).
void m72Curtain(Map& m,float a,float b,float at,bool alongX,float f){
 if(alongX)wall(m,a,at-.02f,b,at+.02f,f+.30f,f+2.35f,39);else wall(m,at-.02f,a,at+.02f,b,f+.30f,f+2.35f,39);
}
// A curtain drawn back: the fabric bunched at the end of its track.
void m72Bunch(Map& m,float x,float y,float f,bool alongX){
 if(alongX){wall(m,x-.08f,y-.06f,x+.08f,y+.06f,f+.24f,f+2.35f,39);wall(m,x-.06f,y+.04f,x+.1f,y+.12f,f+.27f,f+2.31f,39);}
 else{wall(m,x-.06f,y-.08f,x+.06f,y+.08f,f+.24f,f+2.35f,39);wall(m,x+.04f,y-.06f,x+.12f,y+.1f,f+.27f,f+2.31f,39);}
}
// Ceiling track: the renderer hangs the run from the roof; a drop rod holds each end.
void m72Track(Map& m,Vec2 a,Vec2 b,float f,float roof){
 m.pipes.push_back({a,b,f+2.40f,.015f,-999,5});
 for(Vec2 q:{a,b})m.pipes.push_back({q,q,f+2.40f,.007f,roof,5});
}
// A bed-head shelf on the head wall (face at wallY, the bay toward dir) with its monitor.
void m72Shelf(Map& m,float x,float wallY,float dir,float f,bool monitor=true){
 const float y1=wallY,y2=wallY+dir*.25f;
 wall(m,x-.25f,std::min(y1,y2),x+.25f,std::max(y1,y2),f+1.38f,f+1.42f,20);
 wall(m,x-.02f,std::min(y1,y1+dir*.1f),x+.02f,std::max(y1,y1+dir*.1f),f+1.28f,f+1.38f,20);
 if(monitor)mdFix(m,model::med_vitals_monitor,x,wallY+dir*.12f,f+1.42f,.38f,.18f,.28f,dir>0?mdS:mdN,false);
}
// An IV stand built from parts: weighted base, pole, hook, and the bag hanging from it.
void m72Drip(Map& m,float x,float y,float f,bool bag=true){
 wall(m,x-.18f,y-.18f,x+.18f,y+.18f,f,f+.03f,20);
 m.pipes.push_back({{x,y},{x,y},f+.03f,.012f,f+1.9f,5});wall(m,x-.18f,y-.18f,x+.18f,y+.18f,f+.03f,f+1.9f,30);
 if(bag){m.pipes.push_back({{x,y},{x+.15f,y},f+1.88f,.008f,f+1.88f,5});mdFix(m,model::med_blood_bag,x+.15f,y,f+1.72f,.11f,.02f,.14f,mdS,false);}
}
// A fitting with its tube dead: the single-tube housing alone, flush to the ceiling.
void m72DeadBedLight(Map& m,float x,float y,float roof){mdFix(m,model::med_fluoro_light,x,y,roof-.13f,.11f,1.2f,.13f,0,false);}
void m72DeadTube(Map& m,float x,float y,float roof){mdFix(m,model::clinic_ceiling_lamp_2,x,y,roof-.17f,.44f,1.04f,.17f,0,false);}
// The bed-head tube: a single fluorescent fitting over the head of the bed.
WorldLight m72BedLight(float x,float y,float roof,float intensity,float kelvin,float tint){
 auto l=lamp({x,y},roof-.30f,intensity,3.6f,kelvin,tint);l.fitting=model::med_fluoro_light;l.fittingWidth=.11f;l.fittingDepth=1.2f;l.fittingHeight=.13f;return l;
}
// An observation panel along y: painted dado to a low sill (to see a patient lying down), steel
// sill and mullions, wired glass to the head, wall above.
void m72GlazeY(Map& m,float x1,float y1,float x2,float y2,float f,float roof,float sill,float head){
 const float cx=(x1+x2)*.5f;
 wall(m,x1,y1,x2,y2,f,f+sill,3);wall(m,x1-.03f,y1,x2+.03f,y2,f+sill-.03f,f+sill+.01f,20);
 wall(m,cx-.012f,y1,cx+.012f,y2,f+sill+.01f,f+head,17);
 for(float y:{y1,(y1+y2)*.5f-.025f,y2-.05f})wall(m,x1+.02f,y,x2-.02f,y+.05f,f+sill+.01f,f+head,20);
 wall(m,x1,y1,x2,y2,f+head,f+head+.06f,20);wall(m,x1,y1,x2,y2,f+head+.06f,roof,3);
}
WorldLight m72Sw(WorldLight l,const char* state,bool invert){l.enabledState=stateId(state);l.invert=invert;return l;}
ScriptAction m72Set(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
void m72Event(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,std::vector<ScriptAction> actions,bool clear=false){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=true;e.requireEnemiesClear=clear;
 if(*require)e.requireState=stateId(require);e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}

// MEDICAL / TRIAGE (manifest 48). Where the plant's workers queued to be given
// a colour: the queue, the band office behind its hatch, eight curtained bays
// down one aisle, the resus bay, the hold where the reds waited, and the rooms
// that kept it running. The nurse was shot through the hatch from the queue
// side. The man in bay 1 was being watched from her office; read her issue log
// with your back to the window and he gets up. He says what Ward C will teach
// again: they do not get up while the lights are on. Then the bay lights die,
// far to near, and the patient in bay 7 does. The breaker is at the head of the
// service corridor; the walkie in the hold says the Cleanup detail will not come.
// Route: W seam -> queue -> band office (log) -> aisle (the whisper, the
// blackout, the fight) -> service corridor breaker -> the red-lit hold; side
// rooms: stock, sluice, staff room (lockers), decon, overflow, records, plant.
std::shared_ptr<const Map> makeMedicalTriage(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[72];m.definition=CampaignChunks[72];m.definition.ambient=.23f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 machineryClock(m,"md_triage_clock",60000);
 // --- Plan (walls .2 m, painted; faces at y 9.0 and 14.0 carry 71's corridor through the seam) --
 // West column: staff WC, band office, the queue, records, linen.
 wall(m,1.f,4.2f,4.f,4.4f,f,roof,3);m72Swing(m,4.f,4.9f,4.3f,f,roof,-1);wall(m,4.9f,4.2f,5.f,4.4f,f,roof,3);   // door 0: WC
 wall(m,1.f,8.8f,1.8f,9.f,f,roof,3);mdGlass(m,1.8f,8.8f,3.2f,9.f,f,roof);wall(m,3.2f,8.8f,3.8f,9.f,f,roof,3);
 m72Swing(m,3.8f,4.7f,8.9f,f,roof,-1);wall(m,4.7f,8.8f,5.2f,9.f,f,roof,3);                                 // door 1: band office
 wall(m,5.f,1.f,5.2f,7.2f,f,roof,3);m72GlazeY(m,5.f,7.2f,5.2f,8.6f,f,roof,.55f,2.1f);wall(m,5.f,8.6f,5.2f,10.2f,f,roof,3);
 wall(m,5.f,12.8f,5.2f,14.2f,f,roof,3);m72Header(m,5.f,5.2f,10.2f,12.8f,f,roof,2.6f);                     // the threshold
 for(float y:{10.14f,12.8f})wall(m,4.97f,y,5.23f,y+.06f,f,f+2.6f,20);
 wall(m,1.f,14.f,3.8f,14.2f,f,roof,3);m72Swing(m,3.8f,4.7f,14.1f,f,roof,1);wall(m,4.7f,14.f,5.2f,14.2f,f,roof,3); // door 2: records
 wall(m,1.f,18.6f,5.2f,18.8f,f,roof,3);
 wall(m,5.f,14.2f,5.2f,20.f,f,roof,3);m72DoorwayY(m,20.f,21.2f,5.f,5.2f,f,roof);wall(m,5.f,21.2f,5.2f,23.f,f,roof,3);
 // Bay head walls, the service corridor north and the passage south.
 wall(m,5.2f,6.8f,9.9f,7.f,f,roof,3);wall(m,11.9f,6.8f,16.6f,7.f,f,roof,3);
 for(float x:{9.9f,11.7f}){wall(m,x,1.f,x+.2f,5.2f,f,roof,3);m72DoorwayY(m,5.2f,6.4f,x,x+.2f,f,roof);wall(m,x,6.4f,x+.2f,10.2f,f,roof,3);
  wall(m,x,12.8f,x+.2f,16.f,f,roof,3);}
 m72Header(m,10.1f,11.7f,10.05f,10.2f,f,roof,2.5f);m72Header(m,10.1f,11.7f,12.8f,12.95f,f,roof,2.5f);
 wall(m,5.2f,16.f,10.4f,16.2f,f,roof,3);m72Swing(m,10.4f,11.4f,16.1f,f,roof,1);wall(m,11.4f,16.f,23.f,16.2f,f,roof,3); // door 3: overflow
 // East block: staff room, resus, the hold office, the hold, decon, plant.
 wall(m,16.6f,1.f,16.8f,10.2f,f,roof,3);wall(m,16.6f,12.8f,16.8f,16.f,f,roof,3);
 wall(m,16.6f,16.2f,16.8f,19.f,f,roof,3);m72DoorwayY(m,19.f,20.2f,16.6f,16.8f,f,roof);wall(m,16.6f,20.2f,16.8f,23.f,f,roof,3);
 wall(m,16.8f,5.4f,18.6f,5.6f,f,roof,3);m72Swing(m,18.6f,19.5f,5.5f,f,roof,-1);wall(m,19.5f,5.4f,23.f,5.6f,f,roof,3); // door 4: staff room
 m72Header(m,16.8f,19.8f,10.1f,10.2f,f,roof,2.6f);
 wall(m,19.8f,5.6f,20.f,7.75f,f,roof,3);
 wall(m,20.f,7.4f,21.f,7.6f,f,roof,3);m72Swing(m,21.f,21.9f,7.5f,f,roof,-1);mdGlass(m,21.9f,7.4f,22.9f,7.6f,f,roof);
 wall(m,22.9f,7.4f,23.f,7.6f,f,roof,3);                                                                        // door 5: hold office
 wall(m,16.8f,12.8f,17.6f,13.f,f,roof,3);m72Swing(m,17.6f,18.5f,12.9f,f,roof,1);wall(m,18.5f,12.8f,19.8f,13.f,f,roof,3); // door 6: decon
 wall(m,19.8f,12.75f,20.f,16.f,f,roof,3);
 // The hold's bars on the aisle axis: a barred front, the barred doorway open.
 mdFix(m,model::med_cell_bars,19.9f,9.f,f,2.5f,.04f,2.4f,kPi*.5f,false);
 mdFix(m,model::med_cell_bars_doorway,19.9f,11.5f,f,2.5f,.04f,2.4f,kPi*.5f,false);
 wall(m,19.88f,7.75f,19.92f,10.25f,f,f+2.4f,30);
 for(auto [a,b]:{std::pair{10.25f,10.92f},{12.08f,12.75f}})wall(m,19.88f,a,19.92f,b,f,f+2.4f,30);
 wall(m,19.8f,7.6f,20.f,12.75f,f+2.4f,roof,3);wall(m,19.78f,7.6f,20.02f,12.75f,f+2.4f,f+2.44f,20);
 // --- Bays: solid partitions at the head, curtains to the aisle, tracks hung from the roof --------
 for(float x:{7.55f,14.15f})for(auto [y1,y2]:{std::pair{7.f,8.6f},{14.4f,16.f}}){
  wall(m,x,y1,x+.1f,y2,f,f+2.1f,3);wall(m,x-.01f,y1,x+.11f,y2,f+2.1f,f+2.13f,20);}
 for(float x:{7.6f,14.2f}){m72Curtain(m,8.6f,10.13f,x,false,f);m72Curtain(m,12.87f,14.4f,x,false,f);
  m72Track(m,{x,8.6f},{x,10.15f},f,roof);m72Track(m,{x,12.85f},{x,14.4f},f,roof);}
 for(float y:{10.15f,12.85f}){m72Track(m,{5.2f,y},{9.9f,y},f,roof);m72Track(m,{11.9f,y},{16.6f,y},f,roof);}
 // Fronts: 1 drawn back, 2 half, 3 open, 4 mostly shut; 5 open, 6 shut but its east end, 7 shut but its west end, 8 open.
 m72Bunch(m,7.42f,10.15f,f,true);
 m72Curtain(m,8.9f,9.9f,10.15f,true,f);m72Bunch(m,8.82f,10.15f,f,true);
 m72Bunch(m,12.f,10.15f,f,true);
 m72Curtain(m,15.25f,16.6f,10.15f,true,f);m72Bunch(m,15.17f,10.15f,f,true);
 m72Bunch(m,5.3f,12.85f,f,true);
 m72Curtain(m,7.65f,8.7f,12.85f,true,f);m72Bunch(m,8.78f,12.85f,f,true);
 m72Curtain(m,12.95f,14.15f,12.85f,true,f);m72Bunch(m,12.87f,12.85f,f,true);
 m72Bunch(m,16.5f,12.85f,f,true);
 // Head-wall shelves and their monitors.
 m72Shelf(m,5.85f,7.f,1,f);m72Shelf(m,8.15f,7.f,1,f);m72Shelf(m,12.4f,7.f,1,f);m72Shelf(m,14.8f,7.f,1,f,false);
 m72Shelf(m,5.85f,16.f,-1,f);m72Shelf(m,9.4f,16.f,-1,f);m72Shelf(m,12.6f,16.f,-1,f);m72Shelf(m,14.75f,16.f,-1,f);
 m72DeadBedLight(m,8.95f,7.6f,roof);m72DeadBedLight(m,15.45f,7.6f,roof);m72DeadBedLight(m,6.4f,15.4f,roof);m72DeadBedLight(m,13.55f,15.4f,roof);
 // Bay 1, observation: the bed he fell from, the window to the office, the drip still up.
 mdFix(m,model::med_ward_bed,6.98f,8.05f,f,1.98f,.94f,.84f,mdW,true);
 m72Drip(m,6.3f,7.4f,f,true);
 for(auto [x,y,yaw]:{std::tuple{6.3f,9.75f,.4f},{5.45f,9.8f,2.2f}})mdFix(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw,false);
 mdBlood(m,6.1f,8.6f,6.5f,9.3f,f);mdBlood(m,5.5f,9.35f,6.2f,9.7f,f);
 // Bay 2, stripped: the frame bent, the mattress dragged off it, the monitor dark.
 mdFix(m,model::med_bed_frame_wrecked,9.33f,8.05f,f,1.98f,.94f,.84f,mdW+.06f,true);
 mdFix(m,model::med_mattress_stained,8.2f,8.95f,f,1.9f,.9f,.118f,kPi*.5f+.1f,false);
 // Bay 3, left mid-treatment: the drip, a cart of pills, a bag on the floor.
 mdFix(m,model::med_ward_bed,13.45f,8.05f,f,1.98f,.94f,.84f,mdW,true);
 m72Drip(m,12.35f,8.4f,f,false);
 mdFix(m,model::med_crash_cart,12.45f,9.45f,f,.74f,.48f,1.f,.2f,true);
 for(auto [x,y,yaw]:{std::tuple{12.3f,9.4f,.3f},{12.5f,9.5f,1.4f},{12.65f,9.35f,2.6f}})mdFix(m,model::med_pill_bottle,x,y,f+1.f,.07f,.03f,.13f,yaw,false);
 mdFix(m,model::med_blood_bag,12.6f,8.7f,f,.11f,.02f,.14f,.9f,false);
 // Bay 4, a struggle: mattress wrenched across, the monitor pulled down off its shelf.
 mdFix(m,model::med_ward_bed_tossed,15.45f,8.2f,f,2.1f,1.52f,.84f,mdW+.12f,true);
 mdFix(m,model::med_vitals_monitor,14.75f,9.6f,f,.38f,.18f,.28f,2.5f,false);
 for(auto [x,y,yaw]:{std::tuple{14.55f,8.9f,1.1f},{14.9f,9.85f,2.9f}})mdFix(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw,false);
 // Bay 5, wheeled out: the bed is in the aisle; its wheel scuffs, the chair left behind.
 for(float x:{5.95f,6.85f})floorLine(m,x,13.3f,x+.05f,15.6f,f,8);
 mdFix(m,model::med_wheelchair,6.45f,14.5f,f,.55f,1.04f,.91f,mdN+.15f,true);
 mdFix(m,model::item_dosimeter,5.7f,13.4f,f,.31f,.146f,.066f,1.2f,false);
 // Bay 6, curtained: a man on a stained mattress who was never moved, a sharps bin.
 mdFix(m,model::med_ward_bed,8.5f,14.95f,f,1.98f,.94f,.84f,mdE,true);
 mdFix(m,model::trash_can,9.62f,15.3f,f,.27f,.27f,.34f,.4f,true);
 // Bay 7, the dormant one: closed but for its west end, the monitor still green in the dark.
 mdFix(m,model::med_ward_bed,13.55f,14.95f,f,1.98f,.94f,.84f,mdE,true);
 m72Drip(m,13.85f,13.55f,f,true);
 // Bay 8, made up for the next one.
 mdFix(m,model::med_ward_bed,15.6f,14.95f,f,1.98f,.94f,.84f,mdE,true);
 mdFix(m,model::wooden_chair,14.8f,13.75f,f,.417f,.525f,.95f,mdE+.3f,true);
 taped(m,{16.1f,16.f},f+1.55f,kPi,Readable::Note,"BAY 8","KEEP CLEAR FOR AMBER.","02:12: NO AMBER LEFT. BAY 8 STAYS MADE.");
 // --- The queue: rail, hatch, bench, what the called-up workers dropped ------------------------------
 for(float x:{1.5f,2.55f,3.6f})wall(m,x-.03f,10.47f,x+.03f,10.53f,f,f+1.f,20);
 for(float z:{.93f,.48f})wall(m,1.5f,10.48f,3.6f,10.52f,f+z,f+z+.04f,20);
 floorLine(m,3.38f,9.03f,3.46f,10.44f,f,13);
 wall(m,1.7f,9.f,3.3f,9.3f,f+.98f,f+1.02f,20);for(float x:{1.85f,3.15f})wall(m,x-.02f,9.f,x+.02f,9.25f,f+.8f,f+.98f,20);
 mdFix(m,model::lab_id_scanner,1.4f,9.03f,f+1.1f,.3f,.06f,.41f,kPi,false);
 wallSign(m,2.5f,9.055f,f+2.5f,"WAIT TO BE BANDED","GREEN: BACK TO SHIFT / AMBER: SCREENING / RED: WARD C",1.9f,0);
 bench(m,2.4f,13.72f,f);
 mdFix(m,model::med_notice_board,2.4f,13.965f,f+1.2f,1.2f,.07f,.9f,mdN,false);
 for(auto [x,y,yaw]:{std::tuple{1.8f,13.6f,.4f},{3.f,13.75f,2.1f}})mdFix(m,model::mine_hard_hat,x,y,f,.28f,.35f,.15f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{2.f,11.8f,.7f},{3.7f,12.6f,2.4f}})mdFix(m,model::item_dosimeter,x,y,f,.31f,.146f,.066f,yaw,false);
 paperDrift(m,1.3f,10.8f,4.8f,13.2f,f,5,7202);
 extinguisher(m,5.f,9.6f,f,-kPi*.5f);
 mdFix(m,model::lab_status_display,4.97f,11.5f,f+2.66f,.69f,.06f,.2f,kPi*.5f,false);
 // --- Band office: the hatch counter, her, the log desk, the window onto bay 1 ----------------------
 mdFix(m,model::med_nurse_desk,2.5f,8.52f,f,1.18f,.56f,.81f,0,true);
 mdFix(m,model::med_vitals_monitor,2.05f,8.6f,f+.81f,.38f,.18f,.28f,mdN,false);
 mdFix(m,model::office_paper_stack,2.55f,8.5f,f+.81f,.33f,.44f,.03f,.15f,false);
 for(auto [x,y,yaw]:{std::tuple{2.88f,8.42f,.3f},{2.95f,8.66f,1.7f}})mdFix(m,model::item_dosimeter,x,y,f+.81f,.31f,.146f,.066f,yaw,false);
 mdFix(m,model::office_chair,1.65f,7.45f,f,.55f,.55f,.95f,1.1f,true);
 actor(m,"md_triage_clock",ActorVisual::FemaleDoctor,1.7f,{{0,{2.75f,7.85f},f,0,4,1},{1000,{2.75f,7.85f},f,0,4,1}});   // actor 0
 mdBlood(m,2.3f,6.45f,3.25f,7.6f,f);
 // Shot through the glass from the queue: the spray on both faces of the pane.
 for(float y:{8.77f,9.015f})wall(m,2.05f,y,2.95f,y+.015f,f+1.02f,f+1.92f,16);
 mdFix(m,model::desk,1.37f,5.7f,f,1.4f,.7f,.75f,mdW,true);
 terminal(m,{1.45f,5.75f},"BAND DESK / ISSUE LOG","GREEN: TIER 1, BACK TO SHIFT. AMBER: TIER 2, SCREENING DUE. RED: TIER 3, WARD C.",
  "02:12 NO MORE AMBER BANDS. ISSUE RED.",0,stateId("md_triage_turn"));
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.75f;m.terminals.back().yaw=mdE;
 mdFix(m,model::lab_desk_lamp,1.3f,6.22f,f+.75f,.18f,.4f,.52f,0,false);
 mdFix(m,model::office_paper_stack,1.38f,5.2f,f+.75f,.33f,.44f,.03f,.25f,false);
 mdFix(m,model::mug,1.6f,5.1f,f+.75f,.09f,.09f,.1f,0,false);
 mdFix(m,model::med_wall_clock,1.03f,5.7f,f+2.05f,.24f,.02f,.23f,mdE,false);
 shelf(m,{2.1f,4.67f},0,0);
 for(float x:{1.6f,2.45f})mdFix(m,model::prop_cardboard_box,x,4.67f,f+1.656f,.58f,.58f,.3f,x*.4f,false);
 mdFix(m,model::prop_cardboard_box_open,3.45f,5.f,f,.73f,.72f,.73f,0,true);
 mdFix(m,model::med_records_cabinet,4.755f,5.85f,f,.9f,.49f,1.f,mdW,true);
 mdFix(m,model::desk_phone,4.72f,5.55f,f+1.f,.2f,.2f,.1f,-kPi*.5f,false);
 mdFix(m,model::med_notice_board,4.965f,6.5f,f+1.25f,1.2f,.07f,.9f,mdW,false);
 paperDrift(m,1.9f,5.2f,4.4f,7.9f,f,6,7201);
 // Staff WC and lockers.
 mdFix(m,model::med_ward_toilet,1.55f,1.36f,f,.4f,.7f,.82f,0,true);
 mdFix(m,model::med_scrub_sink,2.65f,1.28f,f,.49f,.54f,1.13f,0,true);
 mdFix(m,model::med_washroom_mirror,2.65f,1.02f,f+1.2f,.75f,.02f,.5f,0,false);
 for(float y:{2.7f,3.56f})mdFix(m,model::psx_locker,1.33f,y,f,.64f,.85f,2.42f,0,true);
 mdFix(m,model::med_floor_drain,3.3f,2.3f,f,.5f,.5f,.01f,0,false);
 {auto l=lamp({3.2f,2.6f},roof-.3f,.16f,3.5f,6200,.08f);l.flicker=.7f;m.lights.push_back(l);}   // the WC tube, failing
 // --- The aisle: the gurney wheeled out of bay 5, the drag to the hold, a chair parked facing it ------
 mdFix(m,model::med_ward_bed,6.55f,12.25f,f,1.98f,.94f,.84f,.06f,true);
 for(auto [x,y]:{std::pair{8.2f,11.25f},{9.6f,11.55f},{11.2f,11.35f},{12.7f,11.6f},{14.3f,11.4f},{15.9f,11.6f},{17.5f,11.45f},{19.f,11.55f}})
  mdBlood(m,x-.32f,y-.07f,x+.32f,y+.07f,f);
 mdFix(m,model::med_wheelchair,17.4f,10.9f,f,.55f,1.04f,.91f,mdE,true);
 // --- Resus: the bed under its panel, the cart, oxygen, what they used up ---------------------------
 mdFix(m,model::med_ward_bed,18.3f,7.9f,f,1.98f,.94f,.84f,mdW,true);
 mdFix(m,model::med_crash_cart,17.3f,7.4f,f,.74f,.48f,1.f,.25f,true);
 for(float x:{17.15f,17.5f})mdFix(m,model::gas_cylinder,x,5.8f,f,.33f,.33f,1.58f,0,true);
 wall(m,16.95f,5.97f,17.7f,6.f,f+1.08f,f+1.13f,2);
 mdFix(m,model::med_supply_cabinet,17.02f,9.3f,f,1.1f,.44f,1.7f,mdE,true);
 m72Drip(m,19.3f,7.1f,f,true);
 for(auto [x,y,yaw]:{std::tuple{18.95f,9.45f,.5f},{17.75f,9.7f,2.2f}})mdFix(m,model::med_blood_bag,x,y,f,.11f,.02f,.14f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{17.6f,8.4f,.3f},{19.1f,8.9f,1.9f},{18.9f,6.5f,2.8f},{17.9f,9.95f,1.f}})mdFix(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw,false);
 for(auto [x,y,yaw]:{std::tuple{17.6f,7.95f,.2f},{17.75f,8.15f,1.5f},{17.5f,8.2f,2.7f}})mdFix(m,model::med_pill_bottle,x,y,f,.07f,.03f,.13f,yaw,false);
 mdBlood(m,18.85f,7.3f,19.4f,8.6f,f);
 // --- The hold and its office ---------------------------------------------------------------------
 for(float y:{9.6f,13.6f})mdFix(m,model::bench,22.72f,y,f,1.86f,.44f,.63f,kPi*.5f,true);
 mdFix(m,model::item_walkie,22.2f,13.9f,f,.11f,.38f,.05f,.3f,false);
 mdFix(m,model::med_floor_drain,21.4f,11.5f,f,.5f,.5f,.01f,0,false);
 mdFix(m,model::med_mattress_stained,21.05f,8.85f,f,1.9f,.9f,.118f,kPi*.5f+.05f,false);
 taped(m,{20.f,14.3f},f+1.5f,mdE,Readable::Note,"HOLD / TRANSFER","02:40 NINE RED TO WARD C UNDER ESCORT. HOLD EMPTY.","HOSE IT DOWN BEFORE THE NEXT ONES.");
 mdFix(m,model::prop_cardboard_box_open,21.f,15.3f,f,.73f,.72f,.73f,.2f,true);
 mdFix(m,model::med_notice_board,21.75f,15.965f,f+1.25f,1.2f,.07f,.9f,mdN,false);           // the transfer roster
 for(auto [x,y,yaw]:{std::tuple{21.6f,15.1f,.6f},{20.5f,14.7f,2.f},{21.9f,14.5f,1.1f},{20.4f,15.6f,2.8f}})mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 wall(m,22.93f,15.25f,23.f,15.35f,f+.86f,f+.94f,4);
 m.pipes.push_back({{22.96f,15.3f},{22.96f,15.3f},f+.88f,.02f,f+.03f,3});
 m.pipes.push_back({{22.96f,15.3f},{22.2f,15.05f},f+.03f,.02f,f+.03f,3});m.pipes.push_back({{22.2f,15.05f},{21.6f,12.f},f+.03f,.02f,f+.03f,3});
 mdFix(m,model::desk,22.25f,5.97f,f,1.4f,.7f,.75f,kPi,true);
 mdFix(m,model::crt_monitor,22.25f,5.85f,f+.75f,.42f,.4f,.38f,0,false);
 mdFix(m,model::keyboard,22.25f,6.15f,f+.75f,.45f,.16f,.03f,0,false);
 mdFix(m,model::office_chair,22.3f,6.75f,f,.55f,.55f,.95f,kPi-.3f,true);
 mdFix(m,model::office_paper_stack,21.8f,6.f,f+.75f,.33f,.44f,.03f,-.2f,false);
 mdFix(m,model::lab_wall_clock,20.02f,6.5f,f+2.f,.4f,.03f,.4f,-kPi*.5f,false);
 // --- Decon: two roses, the drain, the hose, the bench they undressed on ----------------------------
 m.pipes.push_back({{16.8f,15.9f},{19.8f,15.9f},f+2.5f,.025f,-999,0});
 for(float x:{17.6f,19.1f}){m.pipes.push_back({{x,15.9f},{x,15.9f},f+2.5f,.02f,f+2.2f,0});
  wall(m,x-.08f,15.74f,x+.08f,15.9f,f+2.16f,f+2.22f,20);}
 mdFix(m,model::med_floor_drain,18.3f,14.8f,f,.5f,.5f,.01f,0,false);
 wall(m,16.8f,13.f,16.82f,16.f,f,f+2.1f,27);wall(m,16.8f,15.98f,19.8f,16.f,f,f+2.1f,27);wall(m,19.78f,13.f,19.8f,16.f,f,f+2.1f,27);
 wall(m,16.8f,13.f,17.6f,13.02f,f,f+2.1f,27);wall(m,18.5f,13.f,19.8f,13.02f,f,f+2.1f,27);
 mdFix(m,model::bench,17.05f,14.4f,f,1.86f,.44f,.63f,kPi*.5f,true);
 wall(m,19.73f,13.55f,19.8f,13.65f,f+.86f,f+.94f,4);
 m.pipes.push_back({{19.76f,13.6f},{19.76f,13.6f},f+.88f,.02f,f+.03f,3});
 m.pipes.push_back({{19.76f,13.6f},{19.f,14.1f},f+.03f,.02f,f+.03f,3});m.pipes.push_back({{19.f,14.1f},{18.5f,14.75f},f+.03f,.02f,f+.03f,3});
 mdFix(m,model::prop_cardboard_box_open,19.35f,15.45f,f,.73f,.72f,.73f,-.2f,true);
 // --- Stock (clean supply) ---------------------------------------------------------------------------
 shelf(m,{6.3f,1.27f},0,0);shelf(m,{8.4f,1.27f},0,0);
 for(float x:{5.85f,6.7f,8.f,8.85f})mdFix(m,model::prop_cardboard_box,x,1.27f,f+1.656f,.58f,.58f,.3f,x*.7f,false);
 mdFix(m,model::med_supply_cabinet,5.42f,3.2f,f,1.1f,.44f,1.7f,mdE,true);
 for(float x:{5.85f,6.75f,8.0f,8.85f})mdFix(m,model::prop_cardboard_box,x,1.27f,f+.306f,.58f,.58f,.3f,x*.3f,false);
 for(float x:{6.f,8.6f})mdFix(m,model::prop_cardboard_box,x,1.27f,f+.972f,.58f,.58f,.3f,x,false);
 for(float x:{6.6f,6.72f,6.84f,6.96f})mdFix(m,model::office_file_binder,x,1.27f,f+.972f,.09f,.3f,.42f,0,false);
 for(float x:{7.9f,8.f,8.1f})mdFix(m,model::med_pill_bottle,x,1.2f,f+.972f,.07f,.03f,.13f,x,false);
 mdFix(m,model::lab_specimen_crate,8.6f,3.6f,f,.9f,.9f,.9f,.2f,true);
 rectangle(m.layers[0],5,5,6,6,'C');rectangle(m.layers[0],6,6,7,7,'C');
 for(auto [x,y,yaw]:{std::tuple{6.2f,5.85f,.4f},{6.5f,5.55f,1.9f},{6.85f,5.3f,.9f},{7.25f,5.15f,2.6f}})mdFix(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw,false); // spilled from behind the crates
 mdFix(m,model::lab_notice_board,8.95f,6.765f,f+1.25f,1.2f,.07f,.9f,mdN,false);
 mdFix(m,model::prop_cardboard_box,7.7f,6.35f,f,.73f,.72f,.37f,.1f,true);mdFix(m,model::prop_cardboard_box,7.7f,6.35f,f+.37f,.73f,.72f,.37f,.45f,true);
 // --- Service corridor: the ward lighting board at its head --------------------------------------
 mdFix(m,model::pwr_breaker_panel_open,10.55f,1.115f,f+1.25f,.32f,.23f,.46f,0,false);
 mdFix(m,model::pwr_disconnect_switch,11.3f,1.175f,f+.95f,.43f,.35f,.85f,0,false);
 m.pipes.push_back({{10.55f,1.06f},{10.55f,1.06f},f+1.71f,.02f,roof,6});m.pipes.push_back({{11.3f,1.08f},{11.3f,1.08f},f+1.8f,.025f,roof,6});
 taped(m,{10.9f,1.f},f+1.45f,0,Readable::Note,"WARD LIGHTING / BAYS 1-8 / AISLE","NIGHT CIRCUIT. IF IT DROPS: LIFT ALL BREAKERS, THEN THE MAIN.",
  "DO NOT LEAVE THE WARD DARK. NOT EVEN FOR A MINUTE.");
 m.terminals.back().activateState=stateId("md_triage_restore");m.terminals.back().requireState=stateId("md_triage_dark");
 mdFix(m,model::prop_cardboard_box,11.32f,3.4f,f,.58f,.58f,.3f,.2f,true);mdFix(m,model::prop_cardboard_box,11.32f,3.4f,f+.3f,.58f,.58f,.3f,.55f,true);
 extinguisher(m,11.7f,8.4f,f,-kPi*.5f);
 // --- Sluice (dirty utility) -------------------------------------------------------------------------
 for(float x:{12.7f,13.3f})mdFix(m,model::med_scrub_sink,x,1.28f,f,.49f,.54f,1.13f,0,true);
 mdFix(m,model::med_ward_toilet,14.3f,1.36f,f,.4f,.7f,.82f,0,true);
 for(float y:{1.45f,2.05f})mdFix(m,model::trash_can,16.27f,y,f,.45f,.45f,.56f,y,true);
 mdFix(m,model::med_floor_drain,14.f,3.5f,f,.5f,.5f,.01f,0,false);
 wall(m,12.6f,6.25f,15.4f,6.8f,f,f+.1f,8);wall(m,12.6f,6.2f,15.4f,6.8f,f+.1f,f+.86f,20);wall(m,12.6f,6.14f,15.4f,6.8f,f+.86f,f+.9f,20);
 for(auto [x,yaw]:{std::pair{12.9f,0.f},{13.15f,.7f},{14.6f,1.5f},{14.85f,.2f}})mdFix(m,model::med_specimen_jar,x,6.5f,f+.9f,.15f,.15f,.24f,yaw,false);
 for(auto [x,yaw]:{std::pair{13.6f,.3f},{13.72f,1.9f},{13.85f,.9f},{14.f,2.6f}})mdFix(m,model::med_pill_bottle,x,6.45f,f+.9f,.07f,.03f,.13f,yaw,false);
 shelf(m,{16.35f,4.6f},kPi*.5f,0);
 for(float y:{3.95f,4.25f,4.55f,4.85f,5.2f})mdFix(m,model::med_specimen_jar,16.35f,y,f+.972f,.15f,.15f,.24f,y*2,false);
 for(float y:{4.1f,4.9f})mdFix(m,model::prop_cardboard_box,16.35f,y,f+.306f,.58f,.58f,.3f,y,false);
 mdFix(m,model::med_crash_cart,14.9f,3.3f,f,.74f,.48f,1.f,.4f,true);
 for(auto [x,y,yaw]:{std::tuple{14.75f,3.25f,.3f},{14.95f,3.4f,1.6f},{15.05f,3.2f,2.4f}})mdFix(m,model::med_pill_bottle,x,y,f+1.f,.07f,.03f,.13f,yaw,false);
 mdFix(m,model::prop_cardboard_box_open,12.5f,3.6f,f,.73f,.72f,.73f,-.2f,true);
 mdBlood(m,13.75f,3.3f,14.35f,3.75f,f);
 mdFix(m,model::med_first_aid_box,14.2f,6.74f,f+1.45f,.37f,.12f,.35f,mdN,false);
 for(float y:{4.2f,4.95f})mdFix(m,model::med_specimen_jar,16.35f,y,f+1.656f,.15f,.15f,.24f,y,false);
 mdFix(m,model::lab_specimen_crate,16.05f,6.25f,f,.9f,.9f,.9f,0,true);
 m.pipes.push_back({{11.9f,1.12f},{16.6f,1.12f},f+2.6f,.03f,-999,0});
 for(float x:{12.7f,13.3f})m.pipes.push_back({{x,1.12f},{x,1.12f},f+2.6f,.015f,f+1.13f,0});
 // --- Staff room: the night shift's break, cut short --------------------------------------------------
 mdFix(m,model::wooden_table,20.3f,3.f,f,1.367f,.724f,.78f,0,true);
 mdFix(m,model::wooden_chair,19.95f,2.25f,f,.417f,.525f,.95f,0,true);
 mdFix(m,model::wooden_chair,20.75f,3.9f,f,.417f,.525f,.95f,kPi+.3f,true);
 mdFix(m,model::wooden_chair,21.7f,3.35f,f,.417f,.525f,.95f,-kPi*.5f+.5f,true);
 mdFix(m,model::office_armchair_green,17.85f,2.1f,f,1.4f,1.36f,1.19f,1.2f,true);
 wall(m,19.4f,1.f,22.f,1.58f,f,f+.1f,8);wall(m,19.4f,1.f,22.f,1.56f,f+.1f,f+.86f,21);wall(m,19.4f,1.f,22.f,1.62f,f+.86f,f+.9f,20);
 mdFix(m,model::coffee_machine,19.9f,1.3f,f+.9f,.4f,.45f,.5f,0,false);
 for(auto [x,y]:{std::pair{20.45f,1.35f},{20.65f,1.22f},{21.25f,1.42f}})mdFix(m,model::mug,x,y,f+.9f,.09f,.09f,.1f,x,false);
 mdFix(m,model::water_cooler,22.6f,1.4f,f,.35f,.35f,1.1f,0,true);
 for(float y:{2.6f,3.46f})mdFix(m,model::psx_locker,22.66f,y,f,.64f,.85f,2.42f,kPi,true);
 mdFix(m,model::med_notice_board,21.4f,5.365f,f+1.2f,1.2f,.07f,.9f,mdN,false);
 taped(m,{20.3f,5.4f},f+1.5f,kPi,Readable::Note,"NIGHT ROTA / 02:00-06:00","ALL LEAVE CANCELLED. ALL BAYS STAFFED.","BY ORDER. DO NOT DISCUSS THE WARD C TRANSFERS.");
 mdFix(m,model::med_first_aid_box,21.4f,1.06f,f+1.45f,.37f,.12f,.35f,0,false);
 mdFix(m,model::med_wall_clock,16.82f,3.4f,f+2.05f,.24f,.02f,.23f,mdE,false);
 // --- Overflow day-room: when the bays ran out ---------------------------------------------------------
 for(Vec2 c:{Vec2{9.f,19.6f},Vec2{13.f,19.6f}}){wall(m,c.x-.2f,c.y-.2f,c.x+.2f,c.y+.2f,f,roof,3);wall(m,c.x-.22f,c.y-.22f,c.x+.22f,c.y+.22f,f,f+.12f,20);}
 {int i=0;for(auto [x,y]:{std::pair{5.75f,17.2f},{6.85f,17.2f},{12.45f,17.2f},{13.55f,17.2f},{6.95f,22.f},{8.05f,22.f},{14.5f,22.f},{15.6f,22.f}})
  mdFix(m,model::med_mattress_stained,x,y,f,1.9f,.9f,.118f,kPi*.5f+.05f*float((i++%3)-1),false);}
 mdFix(m,model::med_ward_bed_tossed,7.3f,19.7f,f,2.1f,1.52f,.84f,.25f,true);
 mdFix(m,model::med_ward_bed_tossed,14.5f,19.3f,f,2.1f,1.52f,.84f,-.2f,true);
 mdFix(m,model::med_wheelchair,10.65f,20.3f,f,.55f,1.04f,.91f,2.6f,true);
 mdFix(m,model::scrap_bedroll,9.3f,22.f,f,.92f,1.96f,.18f,0,false);
 mdFix(m,model::med_wheelchair,9.9f,18.5f,f,.55f,1.04f,.91f,-.6f,true);
 m72Drip(m,5.8f,18.75f,f,true);m72Drip(m,12.3f,18.5f,f,false);
 wall(m,7.75f,16.2f,8.25f,16.62f,f+1.98f,f+2.02f,20);wall(m,7.98f,16.2f,8.02f,16.3f,f+1.6f,f+1.98f,20);
 mdFix(m,model::cctv_tv,8.f,16.42f,f+2.02f,.5f,.4f,.42f,0,false);
 for(float y:{17.2f,17.8f})mdFix(m,model::wooden_chair,16.3f,y,f,.417f,.525f,.95f,-kPi*.5f+(y-17.5f),true);
 paperDrift(m,5.6f,16.5f,16.2f,22.8f,f,8,7203);
 for(auto [x,y,yaw]:{std::tuple{9.4f,17.6f,.5f},{11.8f,20.8f,2.f},{6.4f,20.9f,1.f},{15.2f,18.f,2.7f}})mdFix(m,model::item_id_card,x,y,f,.132f,.085f,.002f,yaw,false);
 // The pharmacy pass-through, shuttered from the other side: frame, slats, dispensing tray.
 for(float x:{11.6f,13.36f})wall(m,x,22.9f,x+.04f,23.f,f+.9f,f+2.f,20);
 wall(m,11.6f,22.9f,13.4f,23.f,f+1.96f,f+2.f,20);wall(m,11.64f,22.99f,13.36f,23.f,f+.95f,f+1.96f,8);
 for(int i=0;i<10;++i){float z=f+.97f+i*.1f;wall(m,11.64f,22.94f,13.36f,22.99f,z,z+.075f,2);}
 wall(m,11.6f,22.62f,13.4f,23.f,f+.9f,f+.95f,20);for(float x:{11.8f,13.2f})wall(m,x-.02f,22.75f,x+.02f,23.f,f+.7f,f+.9f,20);
 // --- Records / band print -----------------------------------------------------------------------------
 for(float y:{14.75f,15.7f,16.65f,17.6f})mdFix(m,model::med_records_cabinet,1.25f,y,f,.9f,.49f,1.f,mdE,true);
 mdFix(m,model::lab_photocopier,4.62f,16.6f,f,.83f,.75f,.97f,kPi*.5f,true);
 mdFix(m,model::office_file_cabinet_open,2.8f,18.25f,f,1.45f,.7f,1.05f,kPi,true);
 for(float y:{14.75f,16.65f})mdFix(m,model::office_paper_stack,1.25f,y,f+1.f,.33f,.44f,.03f,y,false);
 for(float y:{15.55f,15.65f,15.75f})mdFix(m,model::office_file_binder,1.25f,y,f+1.f,.09f,.3f,.42f,mdE,false);
 terminal(m,{4.62f,16.6f},"BAND PRINT / REISSUE SLIP","02:14 LAB 4 / ENERGY DIAGNOSTICS. ISSUED AMBER.","02:15 RE-ISSUED RED. BY ORDER. NO NAME GIVEN.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.97f;m.terminals.back().yaw=mdW;
 // --- Linen and janitor ------------------------------------------------------------------------------
 shelf(m,{1.27f,21.4f},kPi*.5f,0);
 for(float y:{20.85f,21.85f})mdFix(m,model::prop_cardboard_box,1.27f,y,f+1.656f,.58f,.58f,.3f,y,false);
 mdFix(m,model::med_scrub_sink,3.f,22.73f,f,.49f,.54f,1.13f,kPi,true);
 mdFix(m,model::trash_can,4.45f,22.6f,f,.45f,.45f,.56f,.3f,true);
 mdFix(m,model::prop_cardboard_box_open,2.2f,19.5f,f,.73f,.72f,.73f,.25f,true);
 mdFix(m,model::med_crutches,1.06f,19.4f,f,.17f,.06f,1.2f,mdE+.12f,false);
 // --- Plant room: the ward's air and oxygen -------------------------------------------------------
 equipmentPad(m,19.6f,16.6f,22.8f,19.f,f);
 mdFix(m,model::pwr_air_handler,21.9f,17.75f,f+.1f,.81f,1.64f,2.62f,0,true);
 mdFix(m,model::pwr_duct_fan_unit,20.65f,17.75f,f+.1f,.79f,1.49f,1.95f,0,true);
 wall(m,21.6f,17.45f,22.2f,18.05f,f+2.82f,roof,2);wall(m,20.35f,17.45f,20.95f,18.05f,f+2.15f,roof,2);
 wall(m,20.35f,17.5f,22.2f,18.f,roof-.5f,roof,2);
 for(float y:{20.3f,20.7f,21.1f,21.5f})mdFix(m,model::gas_cylinder,22.75f,y,f,.33f,.33f,1.58f,0,true);
 for(float z:{.4f,1.1f})wall(m,22.5f,20.05f,22.55f,21.75f,f+z,f+z+.05f,2);
 for(float y:{20.f,21.75f})wall(m,22.5f,y,22.98f,y+.05f,f,f+1.3f,2);
 pipeRoute(m,{{22.4f,23.f,f+2.35f},{22.4f,16.2f,f+2.35f}},.03f,5,roof,f);
 gateValve(m,{22.4f,19.4f,f+2.35f},{0,1},.03f,f);
 for(float y:{20.3f,20.7f,21.1f,21.5f}){m.pipes.push_back({{22.75f,y},{22.75f,y},f+1.58f,.008f,f+2.35f,5});m.pipes.push_back({{22.75f,y},{22.43f,y},f+2.35f,.008f,f+2.35f,5});}
 mdFix(m,model::pwr_control_cabinet,18.6f,22.78f,f,.8f,.4f,1.6f,kPi,true);
 mdFix(m,model::prop_cardboard_box_open,21.18f,22.4f,f,.73f,.72f,.73f,0,true);mdFix(m,model::prop_cardboard_box_open,21.885f,22.4f,f,.7f,.69f,.7f,0,true); // spare filter media
 // --- The two of them: the nurse (above) and the man in bay 1 (actor 1) -------------------------------
 // He lies along the window side of bay 1. When his timer runs he comes up off
 // the floor (the fall played backwards, stopping on his knees), turned to the
 // glass and whoever is in her office.
 const Vec2 patient{5.75f,8.05f};const float kneel=-2.5f;
 actor(m,"md_triage_rise",ActorVisual::Worker,1.7f,{{0,patient,f,kPi,4,1},{500,patient,f,kPi,4,1},{2300,patient,f,kPi,4,.5f},{2500,patient,f,kneel,5,0},{60000,patient,f,kneel,5,40.f}});
 mdSequence(m,"md_triage_rise","md_triage_risen",60000,3000,-9,-9,-8,-8);
 cue(m,"md_triage_rise",600,Sound::WorkerDying,patient,"",.8f,f+.6f);
 cue(m,"md_triage_rise",2400,Sound::JunkGlass,{5.85f,7.2f},"",.75f,f+1.45f);
 cue(m,"md_triage_rise",2600,Sound::Metal1,{6.55f,8.4f},"",.6f,f+.5f);
 // Turning your back on him starts him, once: reading her log, looking down at her from the
 // office door, or walking on past his bay into the far end or the back rooms without either.
 {TimedSequence s;s.timerState=stateId("md_triage_seen");s.finishState=stateId("md_triage_turn");s.x1=1;s.y1=4.4f;s.x2=5;s.y2=8.8f;
  s.durationMs=1000;s.finishAtMs=1;s.soundIntervalMs=0;s.soundUntilMs=0;s.sightActor=0;s.sightDistance=7.f;m.timedSequences.push_back(s);}
 m72Event(m,"md_triage_late",15.f,7.6f,23.f,16.f,"",{m72Set("md_triage_turn",1)});
 m72Event(m,"md_triage_late_n",5.2f,1.f,16.6f,7.f,"",{m72Set("md_triage_turn",1)});
 m72Event(m,"md_triage_turned",0,0,24,24,"md_triage_turn",{m72Set("md_triage_rise",1)});
 // The whisper at his bay, then the bay lights die toward you and bay 7 gets up.
 // Until then, bay 7 breathes behind its curtain.
 {TimedSequence s;s.timerState=stateId("md_triage_breath");s.finishState=stateId("md_triage_breath_cycled");s.x1=-9;s.y1=-9;s.x2=-8;s.y2=-8;
  s.durationMs=23000;s.finishAtMs=23000;s.soundIntervalMs=0;s.loop=true;m.timedSequences.push_back(s);}
 m72Event(m,"md_triage_breath_go",0,0,24,24,"",{m72Set("md_triage_breath",1)});
 for(int t:{3000,14500})cue(m,"md_triage_breath",t,Sound::WorkerDying,{13.6f,14.95f},"",.12f,f+.7f);
 m72Event(m,"md_triage_hush_go",5.2f,10.2f,7.6f,11.8f,"md_triage_risen",{m72Set("md_triage_hush",1),m72Set("md_triage_breath",0),m72Set("md_triage_woke",1),
  m72Set("md_triage_dark_t1",1),m72Set("md_triage_dark_t2",1),m72Set("md_triage_dark_t3",1),m72Set("md_triage_dark_t4",1)});
 mdSequence(m,"md_triage_hush","md_triage_hush_done",6000,5000,-9,-9,-8,-8);
 cue(m,"md_triage_hush",0,Sound::WorkerDying,patient,"",.3f,f+.9f);
 cue(m,"md_triage_hush",400,Sound::Empty,patient,"PATIENT: ...keep the lights on. They don't get up while the lights are on.",.9f,f+.9f);
 const char* darkFlag[4]={"md_triage_dark_1","md_triage_dark_2","md_triage_dark_3","md_triage_dark"};
 const char* darkTimer[4]={"md_triage_dark_t1","md_triage_dark_t2","md_triage_dark_t3","md_triage_dark_t4"};
 const float darkX[4]={18.2f,14.6f,10.9f,7.f};
 for(int i=0;i<4;++i){mdSequence(m,darkTimer[i],darkFlag[i],6000,3500+350*i,-9,-9,-8,-8);
  cue(m,darkTimer[i],3500+350*i,Sound::Machine,{darkX[i],11.5f},"",.7f,roof-.3f);}
 cue(m,"md_triage_dark_t4",4700,Sound::JunkMetal,{13.55f,14.6f},"",.85f,f+.6f);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{12.55f,15.f},-999,1,0,stateId("md_triage_dark")});
 m.darkZones.push_back({5.2f,7.f,19.8f,16.f,-1000,1000,stateId("md_triage_dark"),1,false,false});
 // The breaker brings them back; what woke stays woken.
 {ScriptAction clunk;clunk.type=ScriptAction::Type::PlaySound;clunk.sound=Sound::Machine;clunk.amount=.8f;
  m72Event(m,"md_triage_restored",10.1f,1.f,11.7f,3.4f,"md_triage_restore",{m72Set("md_triage_dark_1",0),m72Set("md_triage_dark_2",0),
   m72Set("md_triage_dark_3",0),m72Set("md_triage_dark",0),clunk});}
 // After the fight, the walkie in the hold: the detail asks, the Investigator says leave it.
 m72Event(m,"md_triage_radio_go",15.f,7.6f,23.f,16.f,"md_triage_woke",{m72Set("md_triage_radio",1)},true);
 mdSequence(m,"md_triage_radio","md_triage_radio_done",14000,12000,-9,-9,-8,-8);
 {const Vec2 radio{22.2f,13.9f};
  cue(m,"md_triage_radio",0,Sound::RadioSquelch,radio,"",.8f,f+.7f);
  cue(m,"md_triage_radio",500,Sound::Empty,radio,"RADIO: Shots in triage. Somebody's moving in triage.",.9f,f+.7f);
  cue(m,"md_triage_radio",4300,Sound::RadioSquelch,radio,"",.8f,f+.7f);
  cue(m,"md_triage_radio",4700,Sound::Empty,radio,"INVESTIGATOR (RADIO): Triage is closed. Leave it.",.9f,f+.7f);
  cue(m,"md_triage_radio",8600,Sound::Empty,radio,"RADIO: ...Copy. Leaving it.",.9f,f+.7f);}
 // --- Light (29): cold clinic tubes, one warm desk, the reds that only burn in the dark -----------------
 m.lights.push_back(lamp({3.f,11.6f},roof-.3f,.55f,5.f,5000,.08f));                                         // queue
 {auto l=lamp({3.f,6.6f},roof-.3f,.6f,5.f,5200,.06f);l.flicker=.08f;m.lights.push_back(l);}                  // band office
 m.lights.push_back(lamp({1.3f,5.95f},f+1.2f,.45f,2.6f,3000,0,WorldLightMount::Bare));                      // her desk lamp
 m.lights.push_back(lamp({20.f,3.2f},roof-.3f,.6f,5.5f,3000,0));                                            // staff room, warm
 m.lights.push_back(lamp({21.2f,6.5f},roof-.3f,.35f,3.5f,4000,.05f));                                        // hold office
 m.lights.push_back(lamp({22.98f,11.5f},f+2.45f,.55f,6.f,1400,-.25f,WorldLightMount::Wall,kPi));            // the hold: red, on the aisle axis
 m.lights.push_back(m72Sw(lamp({5.21f,11.5f},f+2.8f,.6f,6.f,1300,-.35f,WorldLightMount::Wall,0),"md_triage_dark",false));
 m.lights.push_back(m72Sw(lamp({19.79f,11.5f},f+2.75f,.6f,6.f,1300,-.35f,WorldLightMount::Wall,kPi),"md_triage_dark",false));
 for(int i=0;i<4;++i)m.lights.push_back(m72Sw(lamp({darkX[i],11.5f},roof-.3f,.55f,5.5f,5400,.05f),darkFlag[i],true)); // aisle, far to near
 {auto l=m72BedLight(6.98f,7.6f,roof,.45f,6500,.15f);l.flicker=.45f;m.lights.push_back(m72Sw(l,"md_triage_risen",true));}
 m.lights.push_back(lamp({5.85f,7.45f},f+1.56f,.18f,1.6f,7000,.4f,WorldLightMount::Bare));                   // his monitor
 m.lights.push_back(m72Sw(m72BedLight(13.45f,7.6f,roof,.35f,6500,.15f),"md_triage_dark_2",true));
 m.lights.push_back(m72Sw(m72BedLight(8.5f,15.4f,roof,.35f,6500,.15f),"md_triage_dark_3",true));
 m.lights.push_back(lamp({12.6f,15.55f},f+1.56f,.22f,2.f,7000,.5f,WorldLightMount::Bare));                   // bay 7's monitor, green
 m.lights.push_back(m72Sw(m72BedLight(15.6f,15.4f,roof,.5f,6500,.12f),"md_triage_dark_1",true));
 {auto l=m72Sw(lamp({18.3f,7.9f},roof-.3f,.75f,5.5f,6000,.05f),"md_triage_dark_1",true);
  l.fitting=model::clinic_ceiling_lamp_1;l.fittingWidth=1.05f;l.fittingDepth=1.04f;l.fittingHeight=.16f;m.lights.push_back(l);}  // resus panel
 m.lights.push_back(m72Sw(lamp({18.3f,14.4f},roof-.3f,.4f,4.f,6500,.1f),"md_triage_dark_1",true));         // decon
 m.lights.push_back(lamp({14.2f,3.9f},roof-.3f,.5f,5.f,5600,.3f));                                          // sluice, sick green
 {auto l=lamp({7.6f,3.9f},roof-.3f,.45f,5.f,6000,.05f);l.flicker=.2f;m.lights.push_back(l);}                 // stock
 m.lights.push_back(lamp({10.9f,4.6f},roof-.3f,.4f,5.f,5000,.08f));                                         // service corridor
 m.lights.push_back(lamp({10.9f,1.03f},f+2.15f,.25f,2.4f,1800,0,WorldLightMount::Wall,kPi*.5f));            // the board's caged lamp
 m.lights.push_back(lamp({3.f,16.4f},roof-.3f,.45f,4.5f,4600,.05f));                                        // records
 {auto l=lamp({3.f,20.9f},roof-.3f,.35f,4.f,5000,.1f);l.flicker=.25f;m.lights.push_back(l);}                // linen
 m.lights.push_back(lamp({8.f,19.6f},roof-.3f,.5f,6.5f,4400,.07f));                                         // overflow
 m.lights.push_back(lamp({14.5f,21.5f},roof-.3f,.45f,6.f,4600,.06f));
 m72DeadTube(m,11.f,19.6f,roof);
 m.lights.push_back(lamp({19.8f,19.8f},roof-.3f,.55f,6.5f,4200,.1f));                                       // plant
 // --- Rewards and names -------------------------------------------------------------------------------
 m.pickupSpawns={{{20.3f,3.f},PickupKind::Health,f+.78f},{{5.55f,6.45f},PickupKind::Health,f},{{21.8f,15.45f},PickupKind::Ammo,f},{{20.5f,5.95f},PickupKind::Ammo,f}};
 m.clutterSpawns={{2,{1.62f,6.2f},f+.75f},{2,{6.1f,7.12f},f+1.42f},{2,{17.2f,7.35f},f+1.f,.4f},{2,{17.42f,7.5f},f+1.f,1.2f},
  {2,{12.9f,6.42f},f+.9f},{2,{14.3f,6.5f},f+.9f,.7f},{2,{21.6f,1.3f},f+.9f},{1,{20.6f,2.8f},f+.78f,.5f},{5,{2.3f,8.36f},f+.81f,.3f},
  {0,{1.9f,12.4f},f},{0,{4.2f,13.3f},f,1.f},{0,{6.6f,18.6f},f},{0,{11.5f,22.2f},f,2.f},{0,{15.4f,20.6f},f,.8f}};
 zone(m,"MEDICAL / TRIAGE / BAND OFFICE",1.f,4.4f,5.f,8.8f);
 zone(m,"MEDICAL / TRIAGE / STAFF WC",1.f,1.f,5.f,4.4f);
 zone(m,"MEDICAL / TRIAGE / QUEUE",0,9.f,5.f,14.f);
 zone(m,"MEDICAL / TRIAGE / RECORDS",1.f,14.2f,5.f,18.6f);
 zone(m,"MEDICAL / TRIAGE / LINEN",1.f,18.8f,5.f,23.f);
 zone(m,"MEDICAL / TRIAGE / CLEAN SUPPLY",5.2f,1.f,9.9f,6.8f);
 zone(m,"MEDICAL / TRIAGE / SERVICE CORRIDOR",10.1f,1.f,11.7f,10.2f);
 zone(m,"MEDICAL / TRIAGE / SLUICE",11.9f,1.f,16.6f,6.8f);
 zone(m,"MEDICAL / TRIAGE / STAFF ROOM",16.8f,1.f,23.f,5.4f);
 zone(m,"MEDICAL / TRIAGE / HOLD OFFICE",20.f,5.6f,23.f,7.4f);
 zone(m,"MEDICAL / TRIAGE / OBSERVATION HOLD",20.f,7.6f,23.f,16.f);
 zone(m,"MEDICAL / TRIAGE / RESUS",16.8f,5.6f,19.8f,10.2f);
 zone(m,"MEDICAL / TRIAGE / DECON",16.8f,13.f,19.8f,16.f);
 zone(m,"MEDICAL / TRIAGE / OVERFLOW",5.2f,16.2f,16.6f,23.f);
 zone(m,"MEDICAL / TRIAGE / PLANT",16.8f,16.2f,23.f,23.f);
 zone(m,"MEDICAL / TRIAGE / BAYS",5.2f,7.f,19.8f,16.f);
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

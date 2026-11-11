// DEEP RESEARCH (chapter 2, manifest 71-76): chunks 104-109, the end of Chapter 2. Blocked out:
// real rooms, route, floors and heights, lighting mood, the story beats staged in place; polish later.
// Research turns down. The Deep Materials Lab holds the first raw ore under glass; the Bore Sample
// Archive keeps cores from extreme depth, more violet the deeper they were cut; Geophysics plots the
// tremors back to one deep epicentre; Energy Core Research is the primary extraction and analysis
// machine in a 30 m pit, and shutting its feed (core_shutdown) releases Deep Access Control, where the
// Investigator is seen going down ahead of the player and leaving his men to hold the readers. At the
// Extraction Interface the clean lab is swallowed by rock, and LAB 4's lamps lead on to the mine.
// See docs/Design/descent-campaign-20261025.md (Deep Research, The Investigator).
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include "CampaignExport.h"
#include "Primitives.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- Deep Research kit (dr) --------------------------------------------------------------------
// Facing (fixtures, props, actors, tape, signs): 0 faces +y (south).
constexpr float drS=0,drN=kPi,drE=kPi*.5f,drW=-kPi*.5f;
// Light yaw: the direction a wall lamp throws (0 = +x).
constexpr float lS=kPi*.5f,lN=-kPi*.5f,lE=0,lW=kPi;
// The resonance's own light: a cold violet.
constexpr Point3 Violet{.62f,.22f,1.f};
// The violet climbs map by map: a held breath in the hot cell (104), a single core on the table (105),
// a node under the scanner (106), the machine's lenses in the pit (107), rising from the shaft (108),
// and the seam itself (109). It is a light that comes off things, never the air: the haze stays thin
// and near neutral everywhere, so the source, the route and the landmark read through it.
constexpr Haze LabAir{.10f,.11f,.13f,.006f},DustAir{.10f,.09f,.08f,.007f},PitAir{.06f,.06f,.07f,.004f},
 CellAir{.12f,.12f,.14f,.005f},TableAir{.12f,.09f,.14f,.006f},ShaftAir{.14f,.07f,.20f,.008f},SeamAir{.07f,.06f,.09f,.004f};
void drFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
// A pack model at its own proportions (times s).
void drModel(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];drFix(m,model,x,y,z,v[0]*s,v[1]*s,v[2]*s,yaw,solid);
}
// Nobody perches on tall furniture: an invisible proxy from its top to the ceiling.
void drProxy(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){wall(m,x1,y1,x2,y2,lo,hi,30);}
WorldLight drTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint=0,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;}
void drDeadTube(Map& m,float x,float y,float roof){drFix(m,model::clinic_ceiling_lamp_2,x,y,roof-.17f,.44f,1.04f,.17f,0,false);}
// A bare glow (ore, crystal, a lantern): no housing, no shadows.
WorldLight drGlow(Vec2 p,float z,float intensity,float range,Point3 gel=Violet,float kelvin=6500){
 return lamp(p,z,intensity,range,kelvin,0,WorldLightMount::Bare,0,false,gel);}
WorldLight drWall(Vec2 p,float z,float yaw,float intensity,float range,float kelvin,Point3 gel={1,1,1}){
 return lamp(p,z,intensity,range,kelvin,0,WorldLightMount::Wall,yaw,true,gel);}
// A red lockdown beacon on a wall: blinks while `state` is unset, out once it is set (always, for 0).
WorldLight drBeacon(Vec2 p,float z,float yaw,float phase,StateId state=0){
 auto l=lamp(p,z,.34f,4.2f,3000,0,WorldLightMount::Wall,yaw,true,{1.f,.12f,.06f});
 l.period=1.4f;l.onTime=.7f;l.phase=phase;if(state){l.enabledState=state;l.invert=true;}return l;}
WorldLight drSwitched(WorldLight l,StateId state,bool invert){l.enabledState=state;l.invert=invert;return l;}
// A fitted bench run along its longer side: plinth, carcass, worktop at f+h.
void drCounter(Map& m,float x1,float y1,float x2,float y2,float f,float h=.9f,int top=28){
 wall(m,x1+.05f,y1+.05f,x2-.05f,y2-.05f,f,f+.08f,8);wall(m,x1,y1,x2,y2,f+.08f,f+h-.04f,20);wall(m,x1,y1,x2,y2,f+h-.04f,f+h,top);}
// Lab glazing: dado, sill, panes with mullions every 1.2 m, transom, bulkhead to `roof`, and a
// proxy over the sill so nobody perches on it or steps through.
void drGlaze(Map& m,float x1,float y1,float x2,float y2,float f,float roof,bool holds=true,float sill=.95f,float head=2.4f){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,3);
 if(alongX)wall(m,x1,y1-.03f,x2,y2+.03f,f+sill-.03f,f+sill+.01f,20);else wall(m,x1-.03f,y1,x2+.03f,y2,f+sill-.03f,f+sill+.01f,20);
 const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)kit::glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill+.01f,f+head,17,holds);else kit::glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill+.01f,f+head,17,holds);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.025f,alongX?x1:y1,(alongX?x2:y2)-.05f);
  if(alongX)wall(m,t,y1+.02f,t+.05f,y2-.02f,f+sill+.01f,f+head,20);else wall(m,x1+.02f,t,x2-.02f,t+.05f,f+sill+.01f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.06f,20);if(roof>f+head+.06f)wall(m,x1,y1,x2,y2,f+head+.06f,roof,3);
 wall(m,x1,y1,x2,y2,f+sill+.01f,f+head,30);
}
// A lump of raw ore: grey rock, violet growth in its seams, crystal at its tips (scale s).
void drOre(Map& m,float x,float y,float z,float s){
 wall(m,x-.45f*s,y-.35f*s,x+.45f*s,y+.35f*s,z,z+.5f*s,26);
 wall(m,x-.3f*s,y-.25f*s,x+.25f*s,y+.3f*s,z+.5f*s,z+.78f*s,26);
 wall(m,x-.47f*s,y-.12f*s,x+.47f*s,y+.04f*s,z+.14f*s,z+.34f*s,34);
 wall(m,x-.12f*s,y-.37f*s,x+.05f*s,y+.37f*s,z+.3f*s,z+.62f*s,34);
 wall(m,x+.06f*s,y-.06f*s,x+.18f*s,y+.06f*s,z+.78f*s,z+1.05f*s,35);
 wall(m,x-.22f*s,y+.08f*s,x-.12f*s,y+.18f*s,z+.78f*s,z+.95f*s,35);
}
// A core tray along x centred at (x,y) on a surface at z: a plywood tray and two runs of core;
// `glow` 0..1 is how much of the core came up as ore.
void drTray(Map& m,float x,float y,float z,float glow){
 wall(m,x-.48f,y-.2f,x+.48f,y+.2f,z,z+.05f,10);
 for(float o:{-.09f,.09f}){
  const float ore=glow<=0?x+.46f:x+.46f-.9f*std::min(1.f,glow);
  wall(m,x-.45f,y+o-.06f,ore,y+o+.06f,z+.05f,z+.15f,26);
  if(glow>0)wall(m,ore,y+o-.06f,x+.45f,y+o+.06f,z+.05f,z+.15f,glow>.6f?35:34);}
}
// A tall archive rack along x from x1 to x2, .8 m deep from y: four boards of core trays,
// glowing more the further east (glow0 at x1 to glow1 at x2). A proxy over its top board.
void drRack(Map& m,float x1,float x2,float y,float f,float roof,float glow0,float glow1,int seed){
 for(float x=x1;x<x2-.05f;x+=2.2f)for(float yy:{y,y+.74f})wall(m,x,yy,x+.06f,yy+.06f,f,f+3.3f,12);
 for(float yy:{y,y+.74f})wall(m,x2-.06f,yy,x2,yy+.06f,f,f+3.3f,12);
 int k=seed;
 const float label=y<11.5f?y+.8f:y-.012f;                        // the depth tags face the main aisle
 for(float z:{.12f,1.f,1.9f,2.8f}){wall(m,x1,y,x2,y+.8f,f+z,f+z+.04f,2);
  for(float x=x1+.56f;x<x2-.5f;x+=1.05f,++k){if((k*7+int(z*10))%5==0)continue;
   const float t=(x-x1)/(x2-x1),g=glow0+(glow1-glow0)*t;drTray(m,x,y+.4f,f+z+.04f,((k*3)%4==0)?g*1.2f:g*.6f);
   wall(m,x-.1f,label,x+.1f,label+.012f,f+z-.07f,f+z-.01f,27);}}
 drProxy(m,x1,y,x2,y+.8f,f+3.f,roof);
}
// Somebody who did not get out: a posed body on the map's clock.
void drBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
void drGuard(Map& m,const char* clock,const std::string& key,ActorVisual visual,GuardSquadRole role,const char* squad,std::vector<ActorPatrolPoint> beat,
             float sight,int weapon,float speed,StateId stance=0,int health=80){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=health;
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=squad?stateId(squad):0;ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=.6f;ai.searchMs=7000;ai.speed=speed;ai.patrol=std::move(beat);ai.stance=stance;
 m.actorTracks.push_back(std::move(t));
}
// A sequence started by entering its box (or, with the default box, only by script).
void drSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,float x1=-2,float y1=-2,float x2=-1,float y2=-1){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=x1;s.y1=y1;s.x2=x2;s.y2=y2;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);
}
ScriptAction drSet(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
ScriptAction drDoor(int index,bool open){ScriptAction a;a.type=open?ScriptAction::Type::OpenDoor:ScriptAction::Type::CloseDoor;a.index=index;return a;}
void drEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,std::vector<ScriptAction> actions){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;
 if(*require)e.requireState=stateId(require);e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
// A vertical drop rod from the roof to a pendant reflector at z, lit unless intensity is 0.
void drPendant(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float flicker=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});drFix(m,model::pendant_lamp,p.x,p.y,z,.5f,.5f,.34f,0,false);
 if(intensity>0){auto l=lamp(p,z-.05f,intensity,range,kelvin,0,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);}}

// A pack rock at s times its own size, and a core to walk into.
void drRock(Map& m,int model,float x,float y,float z,float s,float yaw){
 const auto& v=model::size[model];drFix(m,model,x,y,z,v[0]*s,v[1]*s,v[2]*s,yaw,false);
 const float r=std::min(v[0],v[1])*s*.3f;wall(m,x-r,y-r,x+r,y+r,z,z+v[2]*s*.7f,30);
}
// A pack rock as a mass (a buttress to the roof, a course along a face) at s times its size, up to a
// fifth taller or flatter: seen, never walked into, so it stands in front of a core built by the caller.
void drRockFit(Map& m,int model,float x,float y,float z,float s,float tall,float yaw){
 const auto& v=model::size[model];drFix(m,model,x,y,z,v[0]*s,v[1]*s,v[2]*s*tall,yaw,false);}
void drPrim(Map& m,const char* kind,float x,float y,float z,float w,float d,float h,int facing,int mat,bool solid=false){expandPrimitive({kind,x,y,z,w,d,h,facing,mat,solid},m.structures);}
// A crystal shard from its foot at (x,y,z): a shaft w across and h high under a chisel tip, or
// (lean 0 +y, 1 +x, 2 -y, 3 -x) a prism leaning out `reach` metres that way as it rises h.
void drShard(Map& m,float x,float y,float z,float w,float h,int lean=-1,float reach=0){
 if(lean<0){const float tip=std::min(h*.3f,w*1.5f);drPrim(m,"box",x,y,z,w,w*.85f,h-tip,0,35);drPrim(m,"ramp",x,y,z+h-tip,w,w*.85f,tip,int(x*7+y*3)&3,35);return;}
 constexpr Vec2 fwd[4]={{0,1},{1,0},{0,-1},{-1,0}};drPrim(m,"strut",x+fwd[lean].x*reach*.5f,y+fwd[lean].y*reach*.5f,z,w,reach,h,lean,35);
}
// A knot of crystal h high: the shaft, three shards leaning out of its foot, the pack's bed of spikes round it.
void drKnot(Map& m,float x,float y,float z,float h,int seed){
 const float w=std::clamp(h*.15f,.1f,.55f);
 drShard(m,x,y,z,w,h);drShard(m,x,y,z,w*.8f,h*.7f,seed&3,h*.45f);drShard(m,x,y,z,w*.7f,h*.55f,(seed+1)&3,h*.4f);drShard(m,x,y,z,w*.6f,h*.4f,(seed+2)&3,h*.38f);
 drModel(m,model::cave_crystal_bed_blue,x,y,z,seed*.7f,false,std::clamp(h*.7f,.8f,2.6f));
}
// DEEP MATERIALS LAB (manifest 71). Through Emergency Containment's welded door the corridors turn
// clean again: lab panel, cold tubes, the hum of a lab still running. The Force's last post on this
// side is a barrier and a dead man in the vestibule. In the middle of the lab, behind glass that
// holds, sample D-7: raw ore cut from bore 7 at 3,100 m, violet in its seams, the first of it the
// player sees whole. Two men of the detail still walk the floor. East to the core archive, south on.
std::shared_ptr<const Map> makeDeepMaterialsLab(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[104];m.definition=CampaignChunks[104];m.definition.ambient=.16f;
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-15,R=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});                                   // door 0: in from Emergency Containment
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 const char* clock="dr_lab_clock";machineryClock(m,clock,60000);
 // --- Plan ------------------------------------------------------------------------------------
 wall(m,6.f,1.f,6.2f,2.4f,f,R,3);doorwayY(m,2.4f,3.6f,6.f,6.2f,f,R);wall(m,6.f,3.6f,6.2f,5.f,f,R,3);
 wall(m,17.8f,1.f,18.f,2.4f,f,R,3);doorwayY(m,2.4f,3.6f,17.8f,18.f,f,R);wall(m,17.8f,3.6f,18.f,5.f,f,R,3);
 wallDoor(m,1.f,23.f,5.f,5.2f,11.3f,12.7f,f,R,1);                         // door 1: vestibule to the lab
 wallDoor(m,1.f,8.8f,16.8f,17.f,4.f,5.4f,f,R,1);                          // door 2: the prep room
 wallDoor(m,15.2f,23.f,16.8f,17.f,18.6f,20.f,f,R,1);                      // door 3: the night office
 wall(m,8.8f,17.f,9.f,23.f,f,R,3);wall(m,15.f,17.f,15.2f,23.f,f,R,3);
 ceilingGrid(m,1.f,5.2f,23.f,16.8f,R);
 // --- The vestibule: the Force's last post on this side --------------------------------------
 crewBarrier(m,8.4f,4.f,f);
 drBody(m,clock,ActorVisual::Cleanup,{15.f,3.4f},f,1.9f);
 kit::prop(m,model::item_walkie,14.2f,2.6f,f,.114f,.376f,.046f,2.1f);
 kit::prop(m,model::prop_cardboard_box_open,17.2f,4.3f,f,.3f);
 // their post: two road barriers turned to the welded door, what they carried in stacked behind
 // them, bollards at the lab door, the notices nobody took down
 drModel(m,model::urb_jersey_barrier,8.6f,1.95f,f,.15f,true);drModel(m,model::urb_jersey_barrier,15.3f,1.95f,f,-.15f,true);
 for(auto [x,y,z,yaw]:{std::tuple{7.3f,3.5f,0.f,0.f},{8.2f,3.6f,0.f,.1f},{7.3f,3.5f,.367f,0.f}})kit::prop(m,model::prop_cardboard_box,x,y,f+z,yaw);
 kit::prop(m,model::urb_cone,9.6f,3.3f,f,.4f);kit::prop(m,model::mine_hard_hat,14.6f,4.2f,f,.279f,.351f,.153f,1.1f);
 for(float x:{10.8f,13.2f})drModel(m,model::pwr_safety_bollard,x,4.55f,f,0,true);
 drFix(m,model::lab_notice_board,7.7f,1.035f,f+1.3f,1.2f,.07f,.9f,drS,false);drFix(m,model::lab_notice_board,16.2f,1.035f,f+1.3f,1.2f,.07f,.9f,drS,false);
 drFix(m,model::lab_wall_clock,9.4f,1.02f,f+2.2f,.402f,.032f,.4f,drS,false);
 extinguisher(m,6.2f,1.6f,f+.6f,drE);
 m.lights.push_back(drTube(12.f,3.f,R,.45f,5.f,4000,0,.4f));
 m.lights.push_back(drBeacon({10.4f,4.99f},f+2.5f,lN,0));
 // the suit room (west): lockers, three suits back on the rail
 for(int i=0;i<4;++i)drFix(m,model::psx_locker,1.6f+i*.86f,1.33f,f,.64f,.85f,2.42f,-kPi*.5f,true);
 drProxy(m,1.05f,1.f,4.7f,1.7f,f+2.42f,R);
 wall(m,2.f,4.94f,5.6f,5.f,f+1.9f,f+1.94f,20);
 for(float x:{2.6f,3.6f,4.6f}){hungSuit(m,{x,4.75f,f+1.48f},-kPi*.5f+.1f*(x-3.6f));wall(m,x-.012f,4.77f,x+.012f,4.94f,f+1.66f,f+1.9f,8);m.structures.back().solid=false;}
 m.lights.push_back(drTube(3.5f,3.f,R,.4f,4.5f,4200));
 // the post room (east): what the detail boxed for the lift and never sent
 drFix(m,model::pallet,20.f,2.f,f,1.2f,1.15f,.16f,0,true);drModel(m,model::lab_specimen_crate,20.f,2.f,f+.16f,.1f,true,.8f);
 drFix(m,model::pallet,21.8f,3.7f,f,1.2f,1.15f,.16f,kPi*.5f,true);drModel(m,model::lab_specimen_crate,21.8f,3.7f,f+.16f,-.2f,true,.7f);
 m.lights.push_back(drTube(20.5f,3.f,R,.35f,4.5f,3600,0,.2f));
 m.pickupSpawns.push_back({{19.2f,4.2f},PickupKind::Ammo,f});
 // --- The lab: benches round the walls, the hot cell in the middle ---------------------------
 drCounter(m,1.2f,5.2f,9.5f,5.86f,f);drCounter(m,14.5f,5.2f,22.8f,5.86f,f);
 drFix(m,model::lab_test_chamber,3.2f,5.53f,f+.9f,1.14f,.68f,.64f,drS,false);
 kit::prop(m,model::lab_microscope,5.2f,5.5f,f+.9f,.4f);
 kit::prop(m,model::lab_field_meter,6.1f,5.45f,f+.9f,.25f,.328f,.148f,.3f);
 kit::prop(m,model::crt_monitor,7.6f,5.5f,f+.9f,.42f,.4f,.38f,0);
 drTray(m,8.9f,5.53f,f+.9f,.25f);
 drFix(m,model::lab_scope_monitor,15.4f,5.45f,f+.9f,.47f,.34f,.45f,drS,false);
 drTray(m,17.2f,5.53f,f+.9f,.4f);drTray(m,18.4f,5.53f,f+.9f,0);
 kit::prop(m,model::anl_cell_spent,19.6f,5.5f,f+.9f,.8f);
 drFix(m,model::lab_instrument_terminal,21.4f,5.45f,f+.9f,.92f,.47f,.68f,drS,false);
 // the analysers (west wall) and the logger (east wall), proxies over both
 drModel(m,model::lab_analyzer_cabinet,2.17f,7.65f,f,drE,true);drModel(m,model::lab_analyzer_cabinet,2.17f,9.95f,f,drE,true);
 drProxy(m,1.f,6.5f,3.35f,11.1f,f+3.11f,R);
 drModel(m,model::lab_mainframe,21.8f,7.2f,f,drW,true);drProxy(m,20.6f,6.05f,23.f,8.35f,f+3.18f,R);
 // the cutting bench (southwest): slices off D-7, the saw's motor, trays
 drCounter(m,3.f,13.f,7.4f,13.8f,f);
 drOre(m,3.7f,13.4f,f+.9f,.32f);drTray(m,5.f,13.4f,f+.9f,.7f);
 drFix(m,model::pwr_circulation_pump,6.6f,13.4f,f+.9f,.84f,.5f,.39f,drN,false);
 m.lights.push_back(drGlow({3.7f,13.4f},f+1.35f,.18f,2.2f));
 // the compression rig (southeast)
 drCounter(m,20.4f,16.1f,22.8f,16.8f,f);
 drFix(m,model::lab_test_chamber,21.4f,16.45f,f+.9f,1.14f,.68f,.64f,drN,false);
 kit::prop(m,model::anl_cell_core,22.4f,16.45f,f+.9f,.6f);
 // the hot cell: a glass screen that holds on all four sides, stopped at the transom so the whole
 // lab sees over it. D-7 hangs above the screen, high in the room, slung in a cradle under a crown
 // on the hoist of the crane gantry that spans the lab; under it a dense ring of instruments on
 // stands, every one aimed up at it, and the probe leads climbing to the cradle.
 drGlaze(m,8.5f,8.6f,15.5f,8.8f,f,f+2.46f);drGlaze(m,8.5f,13.2f,15.5f,13.4f,f,f+2.46f);
 drGlaze(m,8.5f,8.8f,8.7f,13.2f,f,f+2.46f);drGlaze(m,15.3f,8.8f,15.5f,13.2f,f,f+2.46f);
 for(auto [x,y]:{std::pair{8.5f,8.6f},{15.3f,8.6f},{8.5f,13.2f},{15.3f,13.2f}})wall(m,x,y,x+.2f,y+.2f,f,f+2.9f,12);   // corner posts
 {const Vec2 s{12.f,11.f};const float crown=f+3.95f,cradle=f+2.15f,beam=f+5.f;
  // the crane gantry: two runway girders wall to wall, a portal column under them each side of the
  // cell, hanger rods to the slab, the trolley over the cell and the hoist rope down to the crown
  for(float y:{10.2f,11.5f}){wall(m,1.f,y,23.f,y+.3f,beam,beam+.42f,18);wall(m,1.f,y-.04f,23.f,y+.34f,beam+.42f,beam+.46f,12);}
  for(float x:{4.2f,19.8f}){steelColumn(m,x,11.f,f,beam,false);wall(m,x-.3f,10.1f,x+.3f,11.9f,beam-.12f,beam,12);}
  for(float x:{1.f,22.6f})wall(m,x,10.f,x+.4f,12.f,beam-.3f,beam,15);                              // corbels in the walls
  for(float x:{2.6f,7.8f,16.2f,21.4f})for(float y:{10.35f,11.65f})m.pipes.push_back({{x,y},{x,y},beam+.46f,.025f,R,6});
  wall(m,s.x-.75f,10.05f,s.x+.75f,11.95f,beam-.3f,beam,19);wall(m,s.x-.45f,10.6f,s.x+.45f,11.4f,beam-.62f,beam-.3f,12);   // trolley, hoist drum
  m.pipes.push_back({s,s,crown+.3f,.025f,beam-.62f,6});                                             // the rope
  wall(m,s.x-.7f,s.y-.7f,s.x+.7f,s.y+.7f,crown,crown+.14f,12);                                       // the crown
  wall(m,s.x-.3f,s.y-.3f,s.x+.3f,s.y+.3f,crown+.14f,crown+.3f,13);
  for(auto [dx,dy]:{std::pair{-1.f,-1.f},{1.f,-1.f},{-1.f,1.f},{1.f,1.f}})
   m.pipes.push_back({{s.x+dx*.62f,s.y+dy*.62f},{s.x+dx*.58f,s.y+dy*.5f},crown+.02f,.014f,cradle-.03f,7});   // slings
  wall(m,s.x-.62f,s.y-.55f,s.x+.62f,s.y+.55f,cradle-.07f,cradle,20);                                 // the cradle
  for(auto [dx,dy]:{std::pair{-.6f,-.5f},{.56f,-.5f},{-.6f,.46f},{.56f,.46f}})wall(m,s.x+dx,s.y+dy,s.x+dx+.04f,s.y+dy+.04f,cradle,cradle+.3f,20);
  drOre(m,s.x,s.y,cradle,1.3f);
  // D-7 is more ore than rock: crystal standing out of every face of it, lit from inside
  for(auto [x1,y1,x2,y2,lo,hi]:{std::tuple{-.66f,-.1f,-.5f,.12f,.3f,.9f},{.5f,-.2f,.66f,.02f,.2f,.75f},{-.2f,-.52f,.06f,-.4f,.35f,1.f},
                                {-.04f,.4f,.24f,.52f,.15f,.7f},{-.3f,-.1f,-.1f,.12f,1.f,1.5f},{.14f,-.3f,.3f,-.12f,.95f,1.32f},{-.18f,.1f,-.02f,.3f,1.f,1.62f}})
   wall(m,s.x+x1,s.y+y1,s.x+x2,s.y+y2,cradle+lo,cradle+hi,35);
  // the catch tray under it and the line nobody crosses without tongs
  wall(m,s.x-1.f,s.y-.9f,s.x+1.f,s.y+.9f,f,f+.08f,20);
  for(auto [x1,y1,x2,y2]:{std::tuple{s.x-1.6f,s.y-1.5f,s.x+1.6f,s.y-1.38f},{s.x-1.6f,s.y+1.38f,s.x+1.6f,s.y+1.5f},{s.x-1.6f,s.y-1.38f,s.x-1.48f,s.y+1.38f},{s.x+1.48f,s.y-1.38f,s.x+1.6f,s.y+1.38f}})
   floorLine(m,x1,y1,x2,y2,f,13);
  // the instruments: eight stands round the tray, the corner four at bench height, the side four on
  // tall masts, each instrument aimed at the sample and a probe lead from each up to the cradle
  for(auto [x,y,h]:{std::tuple{9.5f,9.5f,1.12f},{14.5f,9.5f,1.12f},{14.5f,12.5f,1.12f},{9.5f,12.5f,1.12f},
                    {12.f,9.3f,1.7f},{12.f,12.7f,1.7f},{9.3f,11.f,1.7f},{14.7f,11.f,1.7f}}){
   const float aim=std::atan2(s.x-x,s.y-y);const bool meter=(x<12)==(y<11)||(h>1.5f&&(x==12.f));
   m.pipes.push_back({{x,y},{x,y},f,.035f,f+h,6});wall(m,x-.22f,y-.22f,x+.22f,y+.22f,f,f+.04f,20);
   wall(m,x-.2f,y-.2f,x+.2f,y+.2f,f+h,f+h+.04f,20);
   drFix(m,meter?model::lab_field_meter:model::lab_scope_monitor,x,y,f+h+.04f,meter?.25f:.47f,meter?.328f:.34f,meter?.148f:.45f,aim,false);
   m.pipes.push_back({{x,y},{s.x+(x-s.x)*.2f,s.y+(y-s.y)*.2f},f+h+.02f,.01f,cradle-.04f,8});     // probe leads to the cradle
   m.pipes.push_back({{x,y},{s.x+(x-s.x)*.55f,s.y+(y-s.y)*.55f},f+.05f,.03f,f+.05f,7});}          // floor cables to the tray
  drFix(m,model::lab_instrument_terminal,12.f,12.2f,f,.92f,.47f,.68f,drN,false);
  m.lights.push_back(lamp({s.x,s.y},beam-.7f,1.1f,7.f,5600,0,WorldLightMount::Bare,0,false));   // the key light, straight down on it
  m.lights.push_back(drGlow({s.x,s.y-.75f},cradle+.4f,.7f,2.2f));
  m.lights.push_back(drGlow({s.x,s.y},f+.3f,.3f,2.f));
  for(float y:{9.2f,12.8f})m.lights.push_back(lamp({12.f,y},cradle+.5f,.85f,3.6f,5200,0,WorldLightMount::Bare,0,false));
  hazard(m,Hazard::Kind::Anomaly,s.x-1.5f,s.y-1.4f,s.x+1.5f,s.y+1.4f,f,f+3.f,4.f);}
 // the observation lines: a desk along the west glass and one along the east, screens on both and
 // every chair turned to D-7
 drCounter(m,6.7f,9.3f,7.5f,12.7f,f,.76f,20);
 for(float y:{9.9f,11.f,12.1f})drFix(m,model::lab_scope_monitor,7.15f,y,f+.76f,.47f,.34f,.45f,drW,false);
 for(float y:{10.4f,11.6f})kit::prop(m,model::office_chair,6.1f,y,f,.55f,.55f,.95f,drW);
 drCounter(m,15.6f,9.6f,16.4f,12.4f,f,.76f,20);
 for(float y:{10.2f,11.8f})drFix(m,model::lab_scope_monitor,15.95f,y,f+.76f,.47f,.34f,.45f,drE,false);
 for(float y:{10.4f,11.6f})kit::prop(m,model::office_chair,17.f,y,f,.55f,.55f,.95f,drE);
 m.lights.push_back(lamp({7.1f,11.f},f+1.6f,.22f,2.2f,4200,0,WorldLightMount::Bare,0,false));
 m.lights.push_back(lamp({16.f,11.f},f+1.6f,.22f,2.2f,4200,0,WorldLightMount::Bare,0,false));
 wallSign(m,12.f,8.59f,f+.2f,"NO BARE HANDS ON THE SAMPLE","TONGS AND GLOVES ONLY / LOG EVERY OPENING",2.4f,drN);
 taped(m,{14.2f,5.2f},f+1.5f,drS,Readable::Note,"SAMPLE D-7 / 3,100 M","CUT FROM BORE 7 AT 01:10. THE KERF CLOSED BEHIND THE BLADE.",
  "IT IS ORE. IT WAS MINED, NOT GROWN, AND IT CAME UP FROM BELOW ANYTHING WE SURVEYED. GLOVES.");
 // --- The south: prep room, night office, the corridor on ------------------------------------
 drCounter(m,1.2f,22.3f,8.6f,23.f,f);drTray(m,2.f,22.65f,f+.9f,.3f);drTray(m,4.6f,22.65f,f+.9f,.8f);drOre(m,6.9f,22.65f,f+.9f,.3f);
 drModel(m,model::lab_specimen_cabinet,1.43f,18.8f,f,drE,true);drProxy(m,1.f,18.1f,1.85f,19.5f,f+2.19f,R);
 m.lights.push_back(drTube(5.f,20.f,R,.45f,5.f,5600,.04f,.6f));drDeadTube(m,3.f,18.4f,R);
 m.lights.push_back(drGlow({4.6f,22.65f},f+1.3f,.2f,2.4f));
 m.creatureSpawns.push_back({CreatureKind::Transformed,{5.5f,19.6f},-999,1,0});           // the prep tech, at the slices
 // where D-7's slices were washed, mounted and logged: sinks, the mounting bench under its lamp, the
 // cold box, the drain the saw water ran to
 for(float y:{18.5f,19.45f})drModel(m,model::med_scrub_sink,8.5f,y,f,drW,true);
 drModel(m,model::lab_clean_bench,2.6f,17.5f,f,drS,true);
 kit::prop(m,model::lab_microscope,2.3f,17.5f,f+.81f,.3f);for(float x:{2.75f,2.95f})kit::prop(m,model::med_specimen_jar,x,17.45f,f+.81f,x);
 kit::prop(m,model::lab_desk_lamp,3.05f,17.62f,f+.81f,2.2f,.6f);
 m.lights.push_back(lamp({2.8f,17.65f},f+1.2f,.5f,3.2f,3000,0,WorldLightMount::Bare,0,false));
 drModel(m,model::lab_sample_fridge,1.45f,20.5f,f,drE,true);
 drFix(m,model::med_floor_drain,5.f,21.2f,f,.5f,.5f,.01f,0,false);
 drFix(m,model::lab_notice_board,6.9f,17.035f,f+1.3f,1.2f,.07f,.9f,drS,false);drFix(m,model::lab_wall_clock,1.02f,21.6f,f+2.f,.402f,.032f,.4f,drE,false);
 drFix(m,model::pallet,7.5f,21.3f,f,1.2f,1.15f,.16f,.2f,true);drModel(m,model::lab_specimen_crate,7.5f,21.3f,f+.16f,.2f,true,.7f);
 paperDrift(m,2.f,18.4f,8.f,21.8f,f,5,1041);
 workstation(m,19.f,21.4f,f,-1,1,false);
 drModel(m,model::med_records_cabinet,22.5f,19.f,f,drW,true);
 terminal(m,{21.4f,18.2f},"MATERIALS / NIGHT LOG","02:40 THE DETAIL WELDED THE CONTAINMENT DOOR BEHIND US. WE ARE ON THE WRONG SIDE OF IT.",
  "03:15 THE INVESTIGATOR WANTS THE D-SERIES BOXED FOR THE LIFT. HE DID NOT SAY WHERE THE LIFT GOES.");
 m.lights.push_back(lamp({18.6f,21.2f},f+1.2f,.3f,3.f,3000,0,WorldLightMount::Bare,0,false));
 m.pickupSpawns.push_back({{17.4f,21.2f},PickupKind::Health,f});
 // somebody has been sleeping here since the door was welded: a ward bed dragged in, the shelf of
 // binders, a coffee machine on the records cabinet, the lamp left on
 drFix(m,model::med_ward_bed,15.95f,19.7f,f,1.984f,.944f,.842f,kPi*.5f,true);
 drModel(m,model::office_bookcase,17.05f,22.72f,f,kPi,true);
 drFix(m,model::coffee_machine,22.5f,19.f,f+1.f,.3f,.35f,.42f,drW,false);
 kit::prop(m,model::lab_desk_lamp,18.45f,21.3f,f+.75f,-.8f,.6f);kit::prop(m,model::item_walkie,19.5f,21.25f,f+.75f,.114f,.376f,.046f,.6f);
 kit::prop(m,model::prop_cardboard_box,16.f,21.6f,f,.4f);kit::prop(m,model::lab_potted_plant,22.5f,22.4f,f,.3f);
 drFix(m,model::lab_wall_clock,22.98f,20.6f,f+2.f,.402f,.032f,.4f,drW,false);drFix(m,model::art_painting_5,20.9f,22.95f,f+1.4f,1.156f,.08f,.983f,drN,false);
 paperDrift(m,17.f,18.5f,20.4f,20.5f,f,4,1042);
 handCart(m,10.3f,19.4f,f);
 for(auto [x,y]:{std::pair{10.15f,19.2f},{10.45f,19.6f}})kit::prop(m,model::anl_cell_charged,x,y,f+.29f,x+y);
 m.lights.push_back(drGlow({10.3f,19.4f},f+.9f,.12f,1.8f));
 drFix(m,model::pallet,13.8f,21.6f,f,1.2f,1.15f,.16f,0,true);drModel(m,model::lab_specimen_crate,13.8f,21.6f,f+.16f,.2f,true,.75f);
 m.lights.push_back(drTube(12.f,19.8f,R,.5f,6.f,4400,.02f));
 floorLine(m,11.94f,17.f,12.06f,23.f,f,14);
 // --- The detail: two men, frightened, short of rounds -----------------------------------------
 drGuard(m,clock,"dr_lab_a",ActorVisual::Cleanup,GuardSquadRole::Assault,"dr_lab",
  {{{5.f,11.2f},f,5000,drE},{{7.2f,15.4f},f,3000,drS},{{18.4f,15.3f},f,4000,drN},{{18.4f,8.f},f,4000,drW}},13,1,1.2f);
 drGuard(m,clock,"dr_lab_b",ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,"dr_lab",{{{21.f,11.5f},f,60000,drW}},15,2,1.1f);
 // --- Light: cold tubes over the benches, one dead, the cell's violet through the glass ---------
 for(auto [x,y,fl]:{std::tuple{3.5f,7.6f,0.f},{3.5f,12.2f,.25f},{6.5f,15.2f,0.f},{17.5f,7.f,0.f},{19.5f,11.5f,0.f},{17.5f,15.2f,.35f},{12.f,6.8f,0.f},{12.f,15.2f,0.f}})
  m.lights.push_back(drTube(x,y,R,.62f,6.5f,6200,.04f,fl));
 drDeadTube(m,6.6f,9.6f,R);
 // bench strips low on the long walls: light from the side, so the gantry's columns, the cell's posts,
 // the instrument stands and the two men are modelled and not cut out
 for(auto [x,y,face]:{std::tuple{1.06f,12.6f,drE},{1.06f,15.4f,drE},{22.94f,9.6f,drW},{22.94f,14.8f,drW}}){
  drFix(m,model::med_fluoro_light,x,y,f+1.55f,.11f,1.2f,.127f,0,false);
  m.lights.push_back(lamp({x+(x<12?.3f:-.3f),y},f+1.5f,.55f,8.f,5600,.03f,WorldLightMount::Bare,0,false));}
 recessedFittings(m);
 m.pickupSpawns.push_back({{22.f,13.6f},PickupKind::HandgunAmmo,f});
 zone(m,"DEEP MATERIALS LAB / HOT CELL",8.5f,8.6f,15.5f,13.4f,-1000,1000,CellAir);
 zone(m,"DEEP MATERIALS LAB / VESTIBULE",6.f,1.f,18.f,5.f);
 zone(m,"DEEP MATERIALS LAB / SUIT ROOM",1.f,1.f,6.f,5.f);
 zone(m,"DEEP MATERIALS LAB / PREP ROOM",1.f,17.f,8.8f,23.f);
 zone(m,"DEEP MATERIALS LAB / NIGHT OFFICE",15.2f,17.f,23.f,23.f);
 zone(m,"DEEP MATERIALS LAB",1.f,5.f,23.f,17.f,-1000,1000,LabAir);
 titleCard(m,"DEEP RESEARCH",6.f,1.f,18.f,5.f);
 zone(m,"DEEP MATERIALS LAB",0,0,24,24);
 return p;
}

// BORE SAMPLE ARCHIVE (manifest 72). A tall dark store of core racks, the deep research's memory of
// every bore they sank. The trays run in depth order west to east and the violet comes up in them
// the deeper they go, until the logging table at the east end, where bore 7's last nine metres lie
// out in a row and came up as ore. A miner's lamp is still lit on the table. LAB 4 was here.
std::shared_ptr<const Map> makeBoreSampleArchive(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[105];m.definition=CampaignChunks[105];m.definition.ambient=.14f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,R=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 m.definition.playerStart={1.6f,11.5f};m.definition.spawnHeight=f;m.definition.spawnYaw=0;
 const char* clock="dr_arc_clock";machineryClock(m,clock,60000);
 wallBays(m,f,R,4.f,true,true,false,true);
 // --- The racks: three rows each side of the central aisle, a cross aisle at x 9.2-10.8 --------
 // Every tray on them is ordinary rock, tagged and boxed: the archive's grey, so the one core
 // that is not grey owns the far end of the aisle.
 // The two racks nearest the door either side of the aisle are gone (their trays wait on pallets
 // for the lift), so the aisle opens wide at the door and narrows into the far racks: a funnel
 // that frames the logging table and the one core standing up behind it.
 int seed=1;
 for(float y:{1.3f,3.9f,6.4f,15.8f,18.4f,21.9f}){
  if(y!=6.4f&&y!=15.8f)drRack(m,2.6f,9.2f,y,f,R,0,0,seed);++seed;drRack(m,10.8f,16.4f,y,f,R,0,0,seed++);}
 for(auto [x,y]:{std::pair{3.6f,7.4f},{6.6f,7.4f},{3.6f,15.6f},{6.6f,15.6f}}){
  drFix(m,model::pallet,x,y,f,1.2f,1.15f,.16f,0,true);
  for(float o:{-.25f,.25f}){drTray(m,x,y+o,f+.16f,0);wall(m,x-.48f,y+o-.2f,x+.48f,y+o+.2f,f+.31f,f+.36f,10);drTray(m,x,y+o,f+.36f,0);}
  wall(m,x-.12f,y+(y<11?.48f:-.49f),x+.12f,y+(y<11?.49f:-.48f),f+.2f,f+.3f,27);}             // the box tags, turned to the aisle
 // the roof: steel trusses across the store every four metres, over the racks and the aisle
 for(float x:{4.f,8.f,12.f,16.f,20.f}){wall(m,x-.1f,1.f,x+.1f,23.f,R-1.5f,R-1.32f,12);wall(m,x-.1f,1.f,x+.1f,23.f,R-.5f,R-.32f,12);
  for(float y=2.f;y<23.f;y+=2.f)wall(m,x-.05f,y-.05f,x+.05f,y+.05f,R-1.32f,R-.5f,12);}
 // the aisle line, painted from the door to the table
 floorLine(m,1.4f,11.44f,17.6f,11.56f,f,14);
 // --- The logging table (east): bore 7's last nine metres laid out -------------------------------
 // Grey core either side, logged and boxed; in the middle, in a steel cradle under the one good lamp,
 // the 3,091 m run: one metre of solid ore that should not exist.
 drCounter(m,17.8f,10.7f,21.8f,12.3f,f,.9f,20);
 for(int i=0;i<3;++i)if(i!=1){drTray(m,18.4f+i*1.f,11.f,f+.9f,0);drTray(m,18.4f+i*1.f,12.f,f+.9f,i<1?0:.08f);}
 {const float t=f+.9f;
  wall(m,18.95f,11.3f,20.65f,11.7f,t,t+.04f,20);
  for(float x:{19.05f,20.5f})wall(m,x,11.26f,x+.06f,11.74f,t+.04f,t+.2f,20);
  wall(m,19.f,11.38f,20.6f,11.62f,t+.12f,t+.36f,34);                                   // the core
  wall(m,19.2f,11.42f,20.4f,11.58f,t+.36f,t+.44f,35);
  for(float x:{19.3f,19.7f,20.1f,20.5f})wall(m,x,11.3f,x+.02f,11.36f,t+.04f,t+.1f,27);    // tape marks along it
  m.lights.push_back(drGlow({19.8f,11.5f},t+.55f,.5f,2.6f));
  drPendant(m,{19.8f,11.5f},R,f+2.6f,1.05f,6.5f,4600);}                                 // the one good lamp
 kit::prop(m,model::lab_field_meter,18.4f,11.5f,f+.9f,.25f,.328f,.148f,drE);
 kit::prop(m,model::office_paper_stack,21.1f,11.f,f+.9f,.33f,.44f,.03f,.2f);
 const int lantern=int(m.clutterSpawns.size());kit::prop(m,model::scrap_lantern,21.4f,11.95f,f+.9f,.4f);
 m.lights.push_back(drGlow({21.4f,11.95f},f+1.45f,.35f,3.5f,{1,1,1},2600));m.lights.back().prop=lantern;
 kit::prop(m,model::office_chair,19.2f,13.f,f,.55f,.55f,.95f,-.34f);
 terminal(m,{17.2f,9.6f},"BORE 7 / CORE LOG","2,600 M GRANITE. 2,950 M A VOID THE BIT DROPPED THROUGH. 3,091 M ORE, SOLID TO THE END OF THE STRING.",
  "THE BIT CAME BACK WARM. THE CREW SAID THEY HEARD THE HOLE HUMMING. BORE 7 CAPPED BY ORDER AT 3,100 M.");
 taped(m,{22.92f,12.9f},f+1.45f,drW,Readable::Sticky,"LAB 4","FOLLOW THE LAMPS.","");
 drFix(m,model::lab_notice_board,22.89f,10.f,f+1.25f,1.2f,.07f,.9f,drW,false);                   // the depth chart
 // the 3,091 m run's last metre, stood up in a clamp at the end of the aisle against a pale wall
 // washed in white: the one thing in the store that is not grey, and the first thing seen from the door
 wall(m,22.93f,9.2f,23.f,13.8f,f+.3f,f+3.6f,20);
 {const float b=f+.32f;wall(m,21.88f,11.05f,22.76f,11.95f,f,b,15);wall(m,21.84f,11.f,22.8f,12.f,b-.04f,b,20);
  for(float y:{11.12f,11.82f})wall(m,22.29f,y,22.35f,y+.06f,b,b+2.5f,20);
  for(float z:{b+.6f,b+1.5f,b+2.4f})wall(m,22.29f,11.12f,22.35f,11.88f,z,z+.06f,20);
  wall(m,22.2f,11.3f,22.44f,11.7f,b+.05f,b+1.2f,34);wall(m,22.2f,11.3f,22.44f,11.7f,b+1.2f,b+2.35f,35);   // grey-violet low, crystal high
  m.lights.push_back(drGlow({21.8f,11.5f},b+1.5f,.6f,2.4f));
  m.lights.push_back(lamp({21.6f,11.5f},R-1.6f,.75f,6.f,5000,0,WorldLightMount::Bare,0,false));}
 drModel(m,model::lab_specimen_cabinet,22.5f,6.f,f,drW,true);drProxy(m,22.1f,5.3f,23.f,6.7f,f+2.19f,R);
 drModel(m,model::lab_specimen_cabinet,22.5f,18.f,f,drW,true);drProxy(m,22.1f,17.3f,23.f,18.7f,f+2.19f,R);
 drModel(m,model::lab_sample_fridge,22.5f,14.f,f,drW,true);
 m.pickupSpawns.push_back({{20.4f,13.2f},PickupKind::Ammo,f});
 m.pickupSpawns.push_back({{18.f,9.6f},PickupKind::Health,f});
 // what the archivists left: a trolley in the aisle, trays on the floor
 handCart(m,6.f,11.f,f);drTray(m,6.f,11.f,f+.29f,0);
 drTray(m,12.6f,8.2f,f,0);drTray(m,14.3f,14.6f,f,0);
 kit::prop(m,model::office_paper_stack,7.2f,12.4f,f,.33f,.44f,.03f,.7f);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{8.f,5.5f},-999,1,0});              // the night archivist, in the racks
 m.creatureSpawns.push_back({CreatureKind::Transformed,{13.f,17.3f},-999,1,0});
 // --- Light: a line of warm pendants down the aisle to the table; the racks either side fall dark ---
 for(auto [x,i,fl]:{std::tuple{3.5f,.55f,0.f},{7.f,.55f,.12f},{10.5f,.6f,0.f},{14.f,.62f,.3f}})drPendant(m,{x,11.5f},R,f+3.6f,i,6.f,2900,fl);
 drPendant(m,{10.f,4.f},R,f+3.6f,.28f,5.f,3000);
 drPendant(m,{10.f,19.5f},R,f+3.6f,0,0,0);
 zone(m,"BORE SAMPLE ARCHIVE / LOGGING TABLE",17.f,8.f,23.f,15.f,-1000,1000,TableAir);
 zone(m,"BORE SAMPLE ARCHIVE",0,0,24,24,-1000,1000,DustAir);
 return p;
}

// GEOPHYSICS (manifest 73). Where the tremors were plotted. Every trace on the wall and every chart in
// the store points down to one place, and the section shows it: a cut through the ground with the
// bore line running down to a violet node far below the deepest level, drawn across the whole south wall over the way on; the scanner on the floor reads the cores.
// A cut-off detail holds the south corridor under a truce, and the Investigator's voice on
// their radio orders them down to the drill. They do not go.
std::shared_ptr<const Map> makeGeophysics(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[106];m.definition=CampaignChunks[106];m.definition.ambient=.17f;
 // The plotting hall is two storeys: a 7 m volume so the section can be wall-scale, with the
 // side rooms and both corridors under a 3.4 m soffit round it.
 m.definition.ceiling=-8.f;
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceOfficePlaster;
 const float f=-15,R=m.definition.ceiling,low=f+3.4f;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 const char* clock="dr_geo_clock";machineryClock(m,clock,60000);
 // --- Plan ---------------------------------------------------------------------------------------
 wall(m,8.8f,1.f,9.f,5.f,f,R,3);wall(m,15.f,1.f,15.2f,5.f,f,R,3);
 wallDoor(m,1.f,8.8f,5.f,5.2f,3.f,4.2f,f,R,-1);                            // door 0: the chart store
 wallDoor(m,15.2f,23.f,5.f,5.2f,19.4f,20.6f,f,R,-1);                        // door 1: the server room
 wallDoor(m,1.f,8.8f,16.8f,17.f,4.f,5.2f,f,R,1);                            // door 2: the squad's room
 wallDoor(m,15.2f,23.f,16.8f,17.f,18.6f,19.8f,f,R,1);                        // door 3: the plotter room
 wall(m,8.8f,17.f,9.f,23.f,f,R,3);wall(m,15.f,17.f,15.2f,23.f,f,R,3);
 // the soffits: a dropped ceiling over every side room and both corridors, the hall open to the slab
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,1.f,23.f,5.f},{1.f,17.f,23.f,23.f},{9.f,23.f,15.f,24.f},{10.f,0.f,14.f,1.f}}){
  wall(m,x1,y1,x2,y2,low,low+.12f,22);ceilingGrid(m,x1,y1,x2,y2,low);}
 // the hall's roof: two deep girders across it and a hanger grid
 for(float y:{8.3f,13.1f}){wall(m,1.f,y,23.f,y+.3f,R-.75f,R,12);wall(m,1.f,y-.05f,23.f,y+.35f,R-.79f,R-.75f,8);}
 for(float x:{3.f,7.f,17.f,21.f})wall(m,x-.08f,5.2f,x+.08f,16.6f,R-.42f,R,12);
 // --- The wall of traces (north) ------------------------------------------------------------------
 // above the boards, the fourteen stations' drum traces copied out full width, the spikes all
 // landing on the same second at the east end
 wall(m,1.4f,5.2f,22.6f,5.24f,f+3.55f,f+5.65f,20);
 for(int i=0;i<7;++i){const float z=f+3.75f+i*.27f;wall(m,1.6f,5.24f,22.4f,5.255f,z,z+.012f,7);
  for(int k=0;k<44;++k){const float x=1.8f+k*.46f,a=.02f+.11f*float((k*7+i*5)%9)/8.f*(k>30?2.2f:k>20?1.f:.35f);
   wall(m,x,5.24f,x+.03f,5.262f,z-a,z+a,7);}}
 m.lights.push_back(lamp({12.f,6.4f},f+5.9f,.6f,9.f,5200,0,WorldLightMount::Bare,0,false));
 for(float x:{2.2f,5.6f,7.2f})drFix(m,model::lab_notice_board,x,5.235f,f+1.3f,1.2f,.07f,.9f,drS,false);
 for(float x:{16.6f,18.2f,21.8f})drFix(m,model::lab_notice_board,x,5.235f,f+1.3f,1.2f,.07f,.9f,drS,false);
 for(float x:{4.8f,17.4f})drFix(m,model::lab_status_display,x,5.23f,f+2.5f,.69f,.06f,.2f,drS,false);
 taped(m,{8.4f,5.2f},f+1.5f,drS,Readable::Letter,"EVENT 0240 / SOLUTION","FOURTEEN STATIONS. ONE SOURCE. DEPTH 3,900 M PLUS OR MINUS 60, DIRECTLY UNDER THE MINE.",
  "IT IS NOT A FAULT. FAULTS DO NOT REPEAT ON THE SECOND. IT IS STILL GOING, AND IT IS COMING UP.");
 // --- The section: the whole south wall above the doors is the ground cut through, 4 m tall and ---
 // 21 m wide, strata in bands, fourteen station traces converging on one violet node, and the bore
 // line running down to it straight over the way on. It is what the player faces walking in, and
 // the scanner's cables climb straight to its node.
 {const float y0=16.62f,y1=16.8f,lo=f+2.8f,hi=f+6.6f;
  wall(m,1.3f,y0-.04f,22.7f,y1,lo-.12f,lo,20);wall(m,1.3f,y0-.04f,22.7f,y1,hi,hi+.12f,20);   // sill and head
  for(float x:{1.3f,22.58f})wall(m,x,y0-.04f,x+.12f,y1,lo,hi,20);
  float z=hi;int k=0;
  for(auto [h,mat]:{std::pair{.34f,15},{.42f,33},{.3f,31},{.56f,26},{.38f,33},{.5f,26},{.46f,15},{.34f,26},{.4f,34}}){
   // each band steps where the fault cuts it, west of the bore line
   const float step=(k%2?.08f:-.06f);
   wall(m,1.42f,y0,11.2f,y1,std::max(lo,z-h+step),z+step,mat);wall(m,11.2f,y0,22.58f,y1,std::max(lo,z-h),z,mat);
   z-=h;++k;}
  wall(m,11.17f,y0-.02f,11.23f,y1,lo,hi,7);                                                // the fault
  for(int i=0;i<14;++i){const float x=1.9f+i*1.55f;                                          // the stations and their rays
   wall(m,x-.08f,y0-.06f,x+.08f,y0,hi-.16f,hi-.02f,13);
   m.pipes.push_back({{x,y0-.03f},{12.f,y0-.03f},hi-.16f,.014f,lo+.42f,8});}
  m.pipes.push_back({{12.f,y0-.05f},{12.f,y0-.05f},lo+.42f,.035f,hi,6});                   // the bore line
  wall(m,11.72f,y0-.14f,12.28f,y1,lo+.06f,lo+.5f,35);                                       // the node
  for(float x:{3.f,9.f,15.f,21.f})drFix(m,model::lab_status_display,x,y0-.03f,lo-.34f,.69f,.06f,.2f,drN,false);
  for(float x:{4.f,12.f,20.f})m.lights.push_back(lamp({x,15.4f},R-.3f,1.f,8.5f,5200,0,WorldLightMount::Ceiling,0,false));
  // the scanner's feed: three heavy cables slung from its gantry to the node, and two floor runs
  // under hazard covers from the platform's corners to the wall and up it into the section
  for(float o:{-.18f,0.f,.18f})m.pipes.push_back({{12.3f+o,12.05f},{12.f+o*.6f,y0-.08f},f+2.66f,.055f,lo+.22f,7});
  for(float x:{8.55f,15.45f}){wall(m,x-.28f,12.4f,x+.28f,y0-.04f,f,f+.09f,13);
   for(float o:{-.1f,.1f}){m.pipes.push_back({{x+o,12.4f},{x+o,y0-.1f},f+.05f,.045f,f+.05f,7});
    m.pipes.push_back({{x+o,y0-.1f},{x+o,y0-.1f},f+.05f,.045f,lo-.12f,7});}}
  m.lights.push_back(drGlow({12.f,16.2f},lo+.3f,.55f,4.5f));
  // the sensor feed: a tray along the west wall from the drum recorders, round the corner and up
  cableTray(m,{1.25f,6.8f},{1.25f,16.5f},f+2.7f);cableTray(m,{1.25f,16.4f},{8.6f,16.4f},f+2.7f);
  for(float y:{7.8f,9.6f,11.4f,13.2f,15.f})m.pipes.push_back({{1.06f,y},{1.06f,y},f+.88f,.012f,f+2.75f,7});
  m.pipes.push_back({{8.6f,16.59f},{8.6f,16.59f},f+2.75f,.01f,lo-.06f,7});}
 // --- The scanner: a core bed on rollers through a gantry, the detector head over the sample -------
 {const float bed=f+.82f;
  // the platform it stands on: a steel deck a step up, edged in hazard paint
  wall(m,8.3f,9.f,15.7f,12.4f,f,f+.16f,11);
  for(auto [x1,y1,x2,y2]:{std::tuple{8.3f,9.f,15.7f,9.1f},{8.3f,12.3f,15.7f,12.4f},{8.3f,9.1f,8.4f,12.3f},{15.6f,9.1f,15.7f,12.3f}})wall(m,x1,y1,x2,y2,f+.16f,f+.17f,13);
  wall(m,9.2f,10.4f,14.8f,11.2f,f,bed-.06f,15);wall(m,9.15f,10.35f,14.85f,11.25f,bed-.06f,bed,20);
  for(float x=9.5f;x<14.6f;x+=.42f)m.pipes.push_back({{x,10.45f},{x,11.15f},bed+.03f,.03f,bed+.03f,6});   // rollers
  drTray(m,10.f,10.8f,bed+.06f,0);drTray(m,11.05f,10.8f,bed+.06f,0);drTray(m,12.3f,10.8f,bed+.06f,.55f);drTray(m,13.5f,10.8f,bed+.06f,0);
  for(float y:{9.55f,11.85f}){wall(m,12.18f,y,12.42f,y+.2f,f,f+2.6f,20);wall(m,12.08f,y-.05f,12.52f,y+.25f,f,f+.12f,13);}
  wall(m,12.1f,9.55f,12.5f,12.05f,f+2.4f,f+2.75f,20);                                      // the gantry beam
  wall(m,12.f,10.3f,12.6f,11.3f,f+1.5f,f+2.4f,20);wall(m,12.06f,10.4f,12.54f,11.2f,f+1.36f,f+1.5f,35);   // the detector head
  drFix(m,model::lab_scope_monitor,12.3f,12.2f,f+2.75f,.47f,.34f,.45f,drS,false);
  drFix(m,model::lab_field_meter,12.3f,9.55f,f+2.75f,.25f,.328f,.148f,drN,false);
  for(float x:{12.1f,12.5f})m.pipes.push_back({{x,10.8f},{x,10.8f},f+2.75f,.025f,R,7});      // its feed up into the grid
  drProxy(m,12.1f,9.55f,12.5f,12.05f,f+2.75f,R);
  m.lights.push_back(drGlow({12.3f,10.8f},f+1.25f,.4f,2.4f));
  for(float x:{9.6f,15.f})drPendant(m,{x,10.8f},R,f+3.6f,.7f,5.5f,5200);
  m.lights.push_back(lamp({12.3f,10.8f},f+2.3f,.4f,3.5f,5600,0,WorldLightMount::Bare,0,false));}
 // --- The floor: two banks of desks facing the traces, the drum recorders along the west wall ------
 for(float y:{8.6f,12.6f})for(auto [x,v]:{std::pair{3.f,0},{5.7f,1},{18.3f,2},{21.f,3}})workstation(m,x,y,f,-1,v+int(y),false);
 drCounter(m,1.f,7.f,1.7f,15.6f,f);
 for(float y:{7.8f,9.6f,11.4f,13.2f,15.f})drFix(m,model::lab_scope_monitor,1.35f,y,f+.9f,.47f,.34f,.45f,drE,false);
 paperDrift(m,2.f,7.f,8.f,16.f,f,9,1061);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{7.6f,14.8f},-999,1,0});          // the duty seismologist
 // --- The chart store (northwest) and the server room (northeast) ---------------------------------
 for(int i=0;i<4;++i)drModel(m,model::med_records_cabinet,1.6f+i*1.2f,1.33f,f,drS,true);
 drModel(m,model::office_file_cabinet_open,7.4f,1.6f,f,drS,true);
 paperDrift(m,1.5f,2.5f,8.f,4.6f,f,8,1062);
 m.lights.push_back(drTube(4.5f,3.f,low,.45f,4.5f,3800,0,.3f));
 drModel(m,model::lab_mainframe,17.f,2.2f,f,drS,true);drModel(m,model::lab_mainframe,21.f,2.2f,f,drS,true);
 drProxy(m,15.8f,1.f,22.2f,3.4f,f+3.f,low);
 m.lights.push_back(drGlow({19.f,3.8f},f+1.2f,.25f,3.f,{.3f,.55f,1.f}));
 // --- South: the squad's room, the plotter room, the corridor they hold ----------------------------
 bench(m,4.5f,21.8f,f);drFix(m,model::pallet,2.2f,19.f,f,1.2f,1.15f,.16f,0,true);drModel(m,model::lab_specimen_crate,2.2f,19.f,f+.16f,0,true,.7f);
 kit::prop(m,model::mine_hard_hat,6.6f,21.f,f,.279f,.351f,.153f,.5f);
 // four days of two men living in one room: bedrolls against the walls, the table they eat and
 // clean rifles at, ration boxes, the water, their rota on the board, one lantern
 drFix(m,model::wooden_table,6.2f,18.5f,f,1.367f,.724f,.78f,0,true);
 for(auto [x,y,yaw]:{std::tuple{6.f,17.75f,0.f},{6.5f,19.3f,kPi+.3f}})drFix(m,model::wooden_chair,x,y,f,.417f,.525f,.95f,yaw,true);
 {const int lantern=int(m.clutterSpawns.size());kit::prop(m,model::scrap_lantern,6.6f,18.5f,f+.78f,.5f,.6f);
  m.lights.push_back(lamp({6.6f,18.5f},f+1.25f,.55f,4.5f,2600,0,WorldLightMount::Bare,0,false));m.lights.back().prop=lantern;}
 kit::prop(m,model::mug,5.9f,18.4f,f+.78f,.09f,.09f,.1f,0);kit::prop(m,model::mug,6.2f,18.7f,f+.78f,.09f,.09f,.1f,0);kit::prop(m,model::item_walkie,5.8f,18.65f,f+.78f,.114f,.376f,.046f,.9f);
 kit::prop(m,model::scrap_bedroll,1.7f,21.2f,f,0);kit::prop(m,model::scrap_bedroll,7.95f,20.5f,f,.04f);
 for(auto [x,y,z,yaw]:{std::tuple{3.5f,18.3f,0.f,0.f},{3.6f,19.5f,0.f,.1f},{3.5f,18.3f,.367f,0.f}})kit::prop(m,model::prop_cardboard_box,x,y,f+z,yaw);
 kit::prop(m,model::prop_cardboard_box_open,2.6f,20.2f,f,.6f,.6f);
 drFix(m,model::water_cooler,8.45f,17.5f,f,.35f,.35f,1.1f,drW,true);
 drModel(m,model::pwr_tool_chest,1.5f,17.5f,f,drS,true,.2f);
 drFix(m,model::lab_notice_board,2.8f,17.035f,f+1.3f,1.2f,.07f,.9f,drS,false);
 m.pickupSpawns.push_back({{7.6f,22.2f},PickupKind::Ammo,f});
 drModel(m,model::lab_photocopier,21.9f,19.f,f,drW,true);drModel(m,model::lab_photocopier,21.9f,20.3f,f,drW,true);
 // the plotters' room: the light table still on, the chart rack, the sections pinned over the machines
 for(float y:{19.5f,20.7f})drFix(m,model::lab_clean_bench,15.75f,y,f,1.179f,.562f,.808f,kPi*.5f,true);
 wall(m,15.5f,19.f,16.f,21.2f,f+.808f,f+.83f,27);
 m.lights.push_back(lamp({15.8f,20.1f},f+1.05f,.5f,3.f,7000,0,WorldLightMount::Bare,0,false));
 kit::prop(m,model::office_paper_sheet,15.75f,19.6f,f+.83f,.4f,.293f,.004f,.3f);kit::prop(m,model::office_paper_sheet,15.8f,20.6f,f+.83f,.4f,.293f,.004f,1.2f);
 for(float z:{.1f,.62f,1.14f,1.66f})wall(m,16.9f,22.5f,19.5f,23.f,f+z-.04f,f+z,10);
 for(float x:{16.9f,17.75f,18.6f,19.46f})wall(m,x,22.5f,x+.04f,23.f,f,f+1.66f,10);
 for(int row=0;row<3;++row)for(int k=0;k<9;++k)if((k*5+row*3)%7){const float x=17.05f+k*.27f+(k/3)*.06f,z=f+.1f+row*.52f;wall(m,x-.045f,22.54f,x+.045f,22.98f,z,z+.09f,k%3?22:20);}
 drModel(m,model::office_bookcase,21.5f,22.72f,f,kPi,true);
 drModel(m,model::med_records_cabinet,21.6f,17.3f,f,drS,true);drFix(m,model::lab_scope_monitor,21.6f,17.3f,f+1.f,.468f,.345f,.446f,drS,false);
 for(float y:{19.f,20.5f})drFix(m,model::lab_notice_board,22.965f,y,f+1.5f,1.2f,.07f,.9f,drW,false);
 drBody(m,clock,ActorVisual::LabTech,{19.5f,21.f},f,.8f);
 paperDrift(m,16.6f,18.f,21.2f,21.8f,f,8,1063);
 m.lights.push_back(drTube(19.f,20.f,low,.45f,4.5f,4500,.02f,.5f));
 crewBarrier(m,11.f,18.6f,f,1.4f);
 drFix(m,model::pallet,14.f,19.4f,f,1.2f,1.15f,.16f,kPi*.5f,true);
 kit::prop(m,model::item_walkie,13.95f,19.4f,f+.16f,.114f,.376f,.046f,1.2f);
 m.lights.push_back(drTube(12.f,20.5f,low,.5f,5.f,4200,0,.2f));
 // the detail: two men holding their corridor, a truce while the player keeps the weapon down
 const StateId stance=stateId("dr_geo_stance");
 drGuard(m,clock,"dr_geo_a",ActorVisual::Cleanup,GuardSquadRole::Assault,"dr_geo",{{{10.6f,20.6f},f,60000,drN}},12,1,1.1f,stance);
 drGuard(m,clock,"dr_geo_b",ActorVisual::CleanupMedic,GuardSquadRole::Medic,"dr_geo",{{{13.6f,21.6f},f,8000,drN},{{13.f,17.8f},f,4000,drN}},12,1,1.f,stance);
 drEvent(m,"dr_geo_truce",9.f,1.f,15.f,5.f,"",{drSet("dr_geo_stance",int(SquadStance::Truce))});
 drSequence(m,"dr_geo_radio","dr_geo_radio_done",16000,15000,8.f,14.f,16.f,17.f);
 const Vec2 radio{13.95f,19.4f};
 cue(m,"dr_geo_radio",800,Sound::Empty,radio,"INVESTIGATOR (RADIO): Geophysics detail. Burn the charts and fall back to the drill.",.9f,f+.4f);
 cue(m,"dr_geo_radio",5600,Sound::Empty,radio,"RADIO: Sir, nobody has come back up from the interface since three.",.9f,f+.4f);
 cue(m,"dr_geo_radio",9800,Sound::Empty,radio,"INVESTIGATOR (RADIO): Then you will be the first.",.9f,f+.4f);
 cue(m,"dr_geo_radio",13200,Sound::Empty,{10.6f,20.6f},"CLEANUP: You heard him. We're not going.",.8f,f+1.6f);
 // --- Light ----------------------------------------------------------------------------------------
 for(auto [x,y,fl]:{std::tuple{4.4f,7.f,0.f},{4.4f,14.f,0.f},{19.6f,7.f,.3f},{19.6f,14.f,0.f}}){
  drPendant(m,{x,y},R,f+3.9f,.62f,6.5f,4600,fl);}
 recessedFittings(m);
 zone(m,"GEOPHYSICS / CHART STORE",1.f,1.f,8.8f,5.f);
 zone(m,"GEOPHYSICS / SERVER ROOM",15.2f,1.f,23.f,5.f);
 zone(m,"GEOPHYSICS / DETAIL ROOM",1.f,17.f,8.8f,23.f);
 zone(m,"GEOPHYSICS / PLOTTER ROOM",15.2f,17.f,23.f,23.f);
 zone(m,"GEOPHYSICS / SOUTH CORRIDOR",9.f,17.f,15.f,24.f);
 zone(m,"GEOPHYSICS / SCANNER",8.6f,8.6f,15.4f,12.6f,-1000,1000,CellAir);
 zone(m,"GEOPHYSICS",0,0,24,24,-1000,1000,LabAir);
 return p;
}

// ENERGY CORE RESEARCH (manifest 74). The primary extraction and analysis machine: a core standing in
// a 30 m hall, fed ore from below, its feed lines coming in at the bottom of a 10 m pit. The player
// comes out on the north gallery over it in its violet. The south door is held while the core runs;
// the feed is isolated at the pit floor. Two flights down the west wall, the station at the bottom by
// the east wall, the arcs round the core's base. Shut the feed and the violet falls to an idle glow,
// the arcs stop, red work light comes on and the south door releases.
std::shared_ptr<const Map> makeEnergyCoreResearch(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[107];m.definition=CampaignChunks[107];m.definition.ambient=.13f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float pit=-25,g=-15,R=m.definition.ceiling;
 const StateId shut=stateId("core_shutdown"),shed=stateId("dr_core_shed");
 m.layers.push_back(floorLayer(m.name.c_str(),pit));
 openEdge(m,{'N',10.f,14.f,g,false,0});
 openEdge(m,{'S',10.f,14.f,g,true,shut});                                     // door 0: held while the core runs
 m.definition.playerStart={12.f,2.2f};m.definition.spawnHeight=g;
 // --- The galleries: north and south at -15, the east ledge between, the pit at -25 ---------------
 wall(m,1.f,0.f,23.f,6.f,pit,g,15);
 wall(m,1.f,18.f,23.f,24.f,pit,g,15);
 wall(m,19.f,6.f,23.f,18.f,pit,g,15);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,5.86f,23.f,6.f},{1.f,18.f,23.f,18.14f},{19.f,6.f,19.14f,18.f}})wall(m,x1,y1,x2,y2,g-.01f,g,13);   // painted edges
 guardRail(m,3.05f,5.9f,19.f,6.f,g);guardRail(m,1.f,18.f,19.f,18.1f,g);guardRail(m,19.f,6.f,19.1f,18.f,g);
 // --- The way down: two flights on the west wall and the landing between ---------------------------
 m.stairs.push_back({1.1f,6.f,3.05f,13.56f,g-5.f,g,27,true,false});
 wall(m,1.f,13.56f,5.f,16.f,pit,g-5.f,15);
 m.stairs.push_back({3.05f,6.f,5.f,13.56f,pit,g-5.f,27,true,true});
 wall(m,1.f,15.9f,5.f,16.f,g-5.f,g-3.9f,2);m.structures.back().rail=true;
 // --- The overlook: a steel stair up the east wall from the ledge to a platform hung in the hall's ---
 // northeast corner 5 m over the north gallery, level with the upper ring gantry. From its rail the
 // whole machine reads at once: the ring at eye level, the column falling away to its lit base.
 const float ov=g+5.f;
 m.stairs.push_back(Staircase{21.4f,4.4f,22.75f,12.f,g,ov,27,true,false,false,.12f,true});
 wall(m,19.4f,1.f,23.f,4.4f,ov-.25f,ov,20);
 for(float y:{1.6f,3.f})wall(m,19.4f,y,23.f,y+.25f,ov-.6f,ov-.25f,12);                        // its beams
 for(float y:{1.4f,4.15f})steelColumn(m,19.6f,y,g,ov-.25f,false);
 guardRail(m,19.4f,1.f,19.48f,4.4f,ov);guardRail(m,19.4f,4.32f,21.4f,4.4f,ov);
 for(float x:{2.6f,7.f,21.5f})wall(m,x-.25f,1.f,x+.25f,1.22f,g,R,15);                         // pilasters over the galleries
 for(float x:{2.6f,7.f,18.f,21.5f})wall(m,x-.25f,22.78f,x+.25f,23.f,g,R,15);
 for(float y:{3.f,17.f,21.f})wall(m,1.f,y-.25f,1.22f,y+.25f,g,R,15);
 for(float y:{16.5f,21.f})wall(m,22.78f,y-.25f,23.f,y+.25f,g,R,15);
 for(float x:{7.f,10.f,14.f,17.f})wall(m,x-.3f,6.f,x+.3f,6.3f,pit,g-.02f,15);                   // ribs down the pit faces
 for(float x:{7.f,15.6f})wall(m,x-.3f,17.7f,x+.3f,18.f,pit,g-.02f,15);
 for(float x:{4.f,8.f,16.f,20.f})wall(m,x-.15f,1.f,x+.15f,23.f,R-.9f,R-.6f,12);                 // roof trusses
 for(float y:{4.f,20.f})wall(m,1.f,y-.15f,23.f,y+.15f,R-.6f,R-.3f,12);
 // --- The core: the machine in the middle of the pit, its lenses, its feeds, its risers -----------
 // It stands the full 20 m from the pit floor to the crown under the roof: a lens slot burning up
 // each face the whole height, forged bands every 2.2 m, two ring gantries round it (one level with
 // the pit stair's landing, one above the galleries) and the manipulator arms reaching in.
 const Vec2 c{12.f,12.f};const float crown=R-1.4f;
 vessel(m,c,2.2f,pit,crown,6);
 for(auto [x1,y1,x2,y2]:{std::tuple{c.x-.18f,c.y-2.34f,c.x+.18f,c.y-2.16f},{c.x-.18f,c.y+2.16f,c.x+.18f,c.y+2.34f},
                         {c.x-2.34f,c.y-.18f,c.x-2.16f,c.y+.18f},{c.x+2.16f,c.y-.18f,c.x+2.34f,c.y+.18f}})wall(m,x1,y1,x2,y2,pit+1.2f,crown-.6f,35);
 for(float z=pit+3.5f;z<crown-2.f;z+=4.f){
  wall(m,c.x-.6f,c.y-2.38f,c.x+.6f,c.y-2.16f,z,z+1.2f,35);wall(m,c.x-.6f,c.y+2.16f,c.x+.6f,c.y+2.38f,z,z+1.2f,35);
  wall(m,c.x-2.38f,c.y-.6f,c.x-2.16f,c.y+.6f,z,z+1.2f,35);wall(m,c.x+2.16f,c.y-.6f,c.x+2.38f,c.y+.6f,z,z+1.2f,35);}
 for(float x:{10.5f,13.5f})pipeRoute(m,{{x,17.98f,pit+5.5f},{x,14.1f,pit+5.5f}},.35f,3,NoRoof);
 for(auto [dx,dy]:{std::pair{-1.f,-1.f},{1.f,-1.f},{0.f,1.2f}})m.pipes.push_back({{c.x+dx,c.y+dy},{c.x+dx,c.y+dy},crown,.22f,R,2});
 wall(m,c.x-2.5f,c.y-2.5f,c.x+2.5f,c.y+2.5f,crown,crown+.35f,8);wall(m,c.x-2.6f,c.y-2.6f,c.x+2.6f,c.y+2.6f,crown-.12f,crown,13);
 // the ring gantries: catwalk decks hung on rods from the roof, struts in to the shell
 for(float z:{pit+5.f,g+4.5f}){
  for(auto [x,y,yaw]:{std::tuple{c.x,c.y-3.6f,0.f},{c.x,c.y+3.6f,0.f},{c.x-3.6f,c.y,kPi*.5f},{c.x+3.6f,c.y,kPi*.5f}})
   drFix(m,model::pwr_catwalk_section,x,y,z,4.f,2.f,1.311f,yaw,false);
  for(auto [dx,dy]:{std::pair{-1.f,-1.f},{1.f,-1.f},{-1.f,1.f},{1.f,1.f}}){
   wall(m,c.x+dx*3.6f-1.f,c.y+dy*3.6f-1.f,c.x+dx*3.6f+1.f,c.y+dy*3.6f+1.f,z-.08f,z,20);
   m.pipes.push_back({{c.x+dx*4.4f,c.y+dy*4.4f},{c.x+dx*4.4f,c.y+dy*4.4f},z-.04f,.03f,R,6});}
  for(auto [dx,dy]:{std::pair{0.f,-1.f},{0.f,1.f},{-1.f,0.f},{1.f,0.f}}){
   wall(m,c.x+dx*2.75f-.18f-std::fabs(dy)*.1f,c.y+dy*2.75f-.18f-std::fabs(dx)*.1f,c.x+dx*2.75f+.18f+std::fabs(dy)*.1f,c.y+dy*2.75f+.18f+std::fabs(dx)*.1f,z-.3f,z,20);
   m.pipes.push_back({{c.x+dx*2.1f,c.y+dy*2.1f},{c.x+dx*2.75f,c.y+dy*2.75f},z-.15f,.09f,z-.15f,6});}}
 // the manipulator arms: from the crown, elbowed out over the rings, wrists in at the lenses
 for(auto [dx,dy]:{std::pair{.7f,-.7f},{-.7f,.7f},{.7f,.7f}}){
  const Vec2 root{c.x+dx*3.1f,c.y+dy*3.1f},elbow{c.x+dx*5.2f,c.y+dy*5.2f},wrist{c.x+dx*2.9f,c.y+dy*2.9f};
  m.pipes.push_back({root,root,crown-.4f,.16f,crown,6});
  m.pipes.push_back({root,elbow,crown-.4f,.14f,g+8.f,1});m.pipes.push_back({elbow,elbow,g+8.f-.22f,.2f,g+8.f+.22f,6});
  m.pipes.push_back({elbow,wrist,g+8.f,.11f,g+6.2f,1});m.pipes.push_back({wrist,wrist,g+6.f,.16f,g+6.4f,8});}
 // the shimmer round the core all the way up, and its arcs at the base while it runs
 hazard(m,Hazard::Kind::Anomaly,c.x-3.2f,c.y-3.2f,c.x+3.2f,c.y+3.2f,pit,g+4.f,3.f);
 for(int i=0;i<2;++i){Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=c.x-3.4f+i*3.4f;h.x2=h.x1+3.4f;h.y1=c.y-3.4f;h.y2=c.y+3.4f;h.bottom=pit;h.top=pit+2.4f;
  h.damagePerSecond=35;h.enabledFlag=shut;h.invertFlag=true;h.period=2.6f;h.onTime=.9f;h.phase=i*1.3f;m.hazards.push_back(h);}
 // --- The feed isolation at the pit floor (east wall): the station, the switch -------------------
 drModel(m,model::anl_switch_cabinet_1,18.62f,15.6f,pit,drW,true);drProxy(m,18.25f,14.95f,19.f,16.25f,pit+2.91f,g);
 drFix(m,model::lab_knife_switch,18.97f,13.8f,pit+1.3f,.31f,.2f,.32f,drW,false);
 drFix(m,model::lab_pressure_gauge,18.97f,13.2f,pit+1.5f,.29f,.11f,.41f,drW,false);
 terminal(m,{17.6f,16.9f},"CORE FEED / ISOLATION","FEED FROM THE 900 LEVEL: ORE IN SLURRY, FORTY TONNES AN HOUR. ISOLATE HERE. THE ACCESS DOOR RELEASES WITH IT.",
  "INTERLOCK: WILL NOT ISOLATE UNDER LOAD. SHED LOAD AT THE NORTH GALLERY FIRST. THE DUMP BANK HOLDS FIFTY SECONDS.",0,shut,shed);
 // its interlock lamp: amber while the load is on the bank
 drFix(m,model::pwr_ammeter_box,18.97f,14.4f,pit+1.5f,.229f,.102f,.366f,drW,false);
 m.lights.push_back(drSwitched(lamp({18.6f,14.4f},pit+1.7f,.9f,4.f,2200,0,WorldLightMount::Bare,0,false,{1.f,.6f,.15f}),shed,false));
 // --- The north gallery: the observation desk, the status console ---------------------------------
 drCounter(m,14.f,1.f,18.f,1.7f,g);
 drFix(m,model::lab_control_board,16.f,1.05f,g+1.1f,1.32f,.19f,.6f,drS,false);
 kit::prop(m,model::crt_monitor,14.8f,1.35f,g+.9f,.42f,.4f,.38f,0);
 kit::prop(m,model::office_chair,15.6f,2.6f,g,.55f,.55f,.95f,.14f);
 // --- The shutdown: two controls, 54 m and two flights of stairs apart ---------------------------
 // The feed will not isolate under load. The load console on the north gallery sheds it to the dump
 // bank beside it, and the bank holds for fifty seconds before it trips and the load comes back.
 // So: clear the pit, come back up, shed, and run the stairs to the isolation station at the east
 // wall, round the arcs or through them.
 terminal(m,{19.f,2.4f},"CORE / LOAD CONTROL","LOAD 104%. FEED OPEN. E / SHED LOAD TO THE DUMP BANK. THE BANK HOLDS FIFTY SECONDS, THEN TRIPS.",
  "ISOLATE THE FEED AT THE PIT FLOOR WHILE IT HOLDS. NOBODY HAS TIMED THE STAIRS.",g-pit,shed);
 taped(m,{17.4f,1.f},g+1.5f,drS,Readable::Note,"CORE / STATUS PRINT","LOAD 104%. FEED OPEN. ACCESS CONTROL HELD UNTIL THE FEED IS ISOLATED AT THE PIT FLOOR.",
  "THE CORE DOES NOT ANALYSE THE ORE. THE ORE ANALYSES THE CORE. DO NOT WRITE THAT IN THE LOG. (PEN, UNDER THE PRINT)");
 for(float x:{20.75f,21.6f,22.45f})drModel(m,model::pwr_step_transformer,x,1.62f,g,0,true);
 drProxy(m,20.4f,1.15f,22.8f,2.1f,g+1.68f,ov-.6f);
 m.pipes.push_back({{19.5f,1.3f},{20.5f,1.3f},g+.12f,.05f,g+.12f,7});
 m.lights.push_back(drSwitched(lamp({21.6f,2.4f},g+1.f,1.3f,5.f,1900,0,WorldLightMount::Bare,0,false,{1.f,.35f,.1f}),shed,false));
 {TimedSequence t;t.timerState=stateId("dr_core_dump");t.finishState=stateId("dr_core_dump_trip");t.x1=-2;t.y1=-2;t.x2=-1;t.y2=-1;
  t.durationMs=50200;t.finishAtMs=50000;t.soundIntervalMs=1000;t.soundUntilMs=50000;t.tickSound=Sound::Metal2;t.gain=.35f;t.pitch=1.3f;t.soundAt={21.6f,1.62f};t.soundZ=g+1.f;
  m.timedSequences.push_back(t);
  ScriptAction held;held.type=ScriptAction::Type::ShowMessage;held.text="LOAD ON THE DUMP BANK / 50 SECONDS";held.amount=4.f;
  ScriptAction tripped;tripped.type=ScriptAction::Type::ShowMessage;tripped.text="DUMP BANK TRIPPED / CORE BACK UNDER LOAD";tripped.amount=4.f;
  ScriptAction bang;bang.type=ScriptAction::Type::PlaySound;bang.sound=Sound::OneMetalBang;bang.amount=.7f;
  ScriptCondition running;running.id=shut;running.op=ScriptCondition::Op::Equal;running.value=0;
  ScriptEvent e;e.id=stateId("dr_core_shed_start");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=shed;e.once=false;e.rearm=true;e.conditions={running};
  e.actions={drSet("dr_core_dump",1),held};m.scriptEvents.push_back(e);
  e.id=stateId("dr_core_shed_trip");e.requireState=stateId("dr_core_dump_trip");e.actions={drSet("dr_core_shed",0),drSet("dr_core_dump_trip",0),drSet("dr_core_dump",0),tripped,bang};m.scriptEvents.push_back(e);}
 taped(m,{3.4f,1.f},g+1.5f,drS,Readable::Note,"PIT / TWO AT A TIME","THE STAIRS ARE THE ONLY WAY DOWN. NOBODY ON THE PIT FLOOR WITHOUT A SECOND ON THE GALLERY.",
  "IF THE BASE STARTS SINGING, STAND STILL. IT ARCS AT WHAT MOVES.");
 drBody(m,"dr_core_clock",ActorVisual::Scientist,{16.f,4.f},g,1.2f);machineryClock(m,"dr_core_clock",60000);
 m.pickupSpawns.push_back({{21.6f,3.f},PickupKind::Ammo,g});
 m.pickupSpawns.push_back({{17.2f,10.f},PickupKind::Health,pit});
 m.pickupSpawns.push_back({{21.4f,20.5f},PickupKind::HandgunAmmo,g});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{6.4f,16.4f},pit,1,0});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{16.8f,7.6f},pit,1,0});
 m.creatureSpawns.push_back({CreatureKind::Mutant,{6.6f,9.6f},pit,1,0});
 // --- The machine's body: what the concrete shell wears ------------------------------------------
 // a yellow base course, steel collars under both ring gantries, a frame round every lens slot,
 // conduit up the shell between the faces, and two drive housings bolted to its foot
 m.pipes.push_back({c,c,pit+.35f,2.27f,pit+1.1f,3});
 for(float z:{pit+5.f,g+4.5f}){m.pipes.push_back({c,c,z-.42f,2.55f,z-.1f,2});m.pipes.push_back({c,c,z-.5f,2.4f,z-.42f,6});}
 m.pipes.push_back({c,c,crown-1.2f,2.45f,crown-.12f,2});
 for(float sgn:{-1.f,1.f})for(float o:{-.3f,.3f}){
  wall(m,c.x+o-.05f,c.y+sgn*2.3f-.06f,c.x+o+.05f,c.y+sgn*2.3f+.06f,pit+1.1f,crown-1.2f,20);
  wall(m,c.x+sgn*2.3f-.06f,c.y+o-.05f,c.x+sgn*2.3f+.06f,c.y+o+.05f,pit+1.1f,crown-1.2f,20);}
 {int k=0;for(auto [dx,dy]:{std::pair{.707f,.707f},{-.707f,.707f},{.707f,-.707f},{-.707f,-.707f}}){
   for(float o:{-.16f,0.f,.16f})m.pipes.push_back({{c.x+dx*2.3f-dy*o,c.y+dy*2.3f+dx*o},{c.x+dx*2.3f-dy*o,c.y+dy*2.3f+dx*o},pit+1.1f,.05f,crown-1.2f,k%2?3:4});
   for(float z=pit+3.f;z<crown-2.f;z+=4.f)m.pipes.push_back({{c.x+dx*2.32f,c.y+dy*2.32f},{c.x+dx*2.32f,c.y+dy*2.32f},z,.3f,z+.5f,6});++k;}}
 for(auto [x,y]:{std::pair{14.9f,9.1f},{9.1f,14.9f}}){drFix(m,model::pwr_turbine_housing,x,y,pit,2.1f,2.1f,1.53f,0,true);drProxy(m,x-1.05f,y-1.05f,x+1.05f,y+1.05f,pit+1.53f,pit+4.f);
  m.pipes.push_back({{x,y},{c.x+(x-c.x)*.6f,c.y+(y-c.y)*.6f},pit+1.f,.28f,pit+1.9f,6});}
 // --- The pit floor: the slurry pumps under the feed mains, switchgear on the north face, the ----
 // conduit risers and the tray on the east wall
 for(float x:{9.2f,13.6f}){drModel(m,model::mine_dewatering_pump,x,17.45f,pit,0,true);drProxy(m,x-1.53f,16.98f,x+1.53f,17.92f,pit+1.75f,pit+4.f);}
 for(float x:{10.5f,13.5f}){m.pipes.push_back({{x,17.6f},{x,17.6f},pit+1.6f,.2f,pit+5.5f,3});flange(m,{x,17.6f,pit+1.8f},{},.2f);}
 gateValve(m,{10.5f,16.2f,pit+5.5f},{0,1},.35f,pit,-1);gateValve(m,{13.5f,16.2f,pit+5.5f},{0,1},.35f,pit,1);
 drModel(m,model::pwr_control_cabinet,11.4f,17.75f,pit,drN,true);
 for(float x:{8.5f,12.f})drModel(m,model::pwr_switchgear_cabinet,x,6.72f,pit,0,true);
 drModel(m,model::pwr_tall_switch_cabinet,18.5f,10.25f,pit,0,true);
 for(float y:{11.3f,11.6f,11.9f,12.2f,12.5f})m.pipes.push_back({{18.93f,y},{18.93f,y},pit,.045f,g-.05f,y<11.8f?4:6});
 cableTray(m,{18.78f,6.5f},{18.78f,12.6f},pit+3.4f);cableTray(m,{7.4f,6.22f},{16.6f,6.22f},pit+3.4f);
 drFix(m,model::drum,5.45f,17.45f,pit,.6f,.6f,.9f,0,true);drFix(m,model::drum,6.2f,17.55f,pit,.6f,.6f,.9f,0,true);
 drFix(m,model::pallet,16.2f,13.f,pit,1.2f,1.15f,.16f,.3f,true);drModel(m,model::lab_specimen_crate,16.2f,13.f,pit+.16f,.3f,true,.8f);
 kit::prop(m,model::mine_hard_hat,15.4f,16.6f,pit,.279f,.351f,.153f,.8f);
 // --- The galleries: lockers and stores on the north, the staging for the feed work on the south --
 for(int i=0;i<3;++i)drFix(m,model::psx_locker,4.55f+i*.86f,1.33f,g,.64f,.85f,2.42f,-kPi*.5f,true);
 drProxy(m,4.1f,1.f,6.72f,1.7f,g+2.42f,R);
 drFix(m,model::pallet,8.5f,2.f,g,1.2f,1.15f,.16f,.1f,true);drModel(m,model::lab_specimen_crate,8.5f,2.f,g+.16f,.1f,true,.8f);
 drFix(m,model::lab_instrument_terminal,17.2f,1.35f,g+.9f,.92f,.47f,.68f,drS,false);
 drFix(m,model::lab_status_display,15.f,1.03f,g+2.f,.693f,.058f,.2f,drS,false);
 drFix(m,model::lab_notice_board,8.6f,1.035f,g+1.3f,1.2f,.07f,.9f,drS,false);
 extinguisher(m,9.5f,1.f,g+.6f,drS);
 for(float x:{5.6f,6.5f})drModel(m,model::pwr_safety_bollard,x,5.5f,g,0,true);
 for(auto [x,y,yaw,sc]:{std::tuple{3.4f,20.2f,.1f,.9f},{5.3f,21.7f,-.2f,.8f}}){drFix(m,model::pallet,x,y,g,1.2f,1.15f,.16f,yaw,true);drModel(m,model::lab_specimen_crate,x,y,g+.16f,yaw,true,sc);}
 drModel(m,model::pwr_sheet_steel_stack,7.6f,20.6f,g,.2f,false);
 drModel(m,model::mine_portable_generator,8.4f,22.2f,g,.4f,true);
 for(float x:{15.9f,19.3f})drModel(m,model::pwr_switchgear_cabinet,x,22.56f,g,0,true);
 drFix(m,model::cable_reel,17.6f,20.3f,g,1.f,2.f,2.f,drE,true);drProxy(m,16.6f,19.8f,18.6f,20.8f,g+2.f,R);
 bench(m,4.6f,19.f,g);
 kit::prop(m,model::mine_hard_hat,4.f,19.f,g+.51f,.279f,.351f,.153f,0);kit::prop(m,model::prop_cardboard_box,6.4f,22.3f,g,.6f);
 cableTray(m,{22.78f,14.4f},{22.78f,17.6f},g+2.9f);
 // --- Light: the core's violet (out at isolation), sodium on the galleries, red after ------------
 // The violet stays on the machine: short glows at the lenses, strongest at the base. The hall
 // round it is lit white and sodium so the concrete reads grey, and the column stands out against it.
 // Six lamps cast shadows, the ones that model the machine: two floods from the roof down the shell,
 // two uplights in opposite corners of the pit that throw it up the walls, two floods over the galleries.
 for(auto [dx,dy]:{std::pair{0.f,-2.7f},{0.f,2.7f},{-2.7f,0.f},{2.7f,0.f}})m.lights.push_back(drSwitched(drGlow({c.x+dx,c.y+dy},pit+2.5f,1.f,3.6f),shut,true));
 for(float dy:{-2.7f,2.7f})m.lights.push_back(drSwitched(drGlow({c.x,c.y+dy},g+2.6f,.8f,4.6f),shut,true));
 for(float dy:{-4.6f,4.6f})m.lights.push_back(lamp({c.x,c.y+dy},R-.3f,1.3f,22.f,5600,0,WorldLightMount::Ceiling,0,true));
 for(auto [x,y]:{std::pair{6.f,7.2f},{16.6f,16.9f}})m.lights.push_back(lamp({x,y},pit+.6f,.95f,12.f,5000,0,WorldLightMount::Bare,0,true));
 for(auto [x,y,yaw]:{std::tuple{16.f,1.f,lS},{8.f,23.f,lN}})m.lights.push_back(lamp({x,y},g+6.f,1.3f,16.f,5400,0,WorldLightMount::Wall,yaw,true));
 // floods on the pit faces at mid height, thrown at the shell
 for(auto [x,y,yaw]:{std::tuple{12.f,6.f,lS},{12.f,18.f,lN},{19.f,12.f,lW}})
  m.lights.push_back(lamp({x,y},g-3.f,1.1f,12.f,5200,0,WorldLightMount::Wall,yaw,false));
 for(float x:{4.f,20.f})m.lights.push_back(lamp({x,1.f},g+2.6f,.85f,10.f,2100,0,WorldLightMount::Wall,lS,false));
 for(float x:{4.f,17.4f})m.lights.push_back(lamp({x,23.f},g+2.6f,.85f,10.f,2100,0,WorldLightMount::Wall,lN,false));
 m.lights.push_back(lamp({1.f,17.2f},pit+2.6f,.5f,7.f,2300,0,WorldLightMount::Wall,lE,false));
 m.lights.push_back(lamp({19.f,9.f},pit+2.6f,.5f,7.f,2300,0,WorldLightMount::Wall,lW,false));
 // the west stairwell and the ledge under the overlook stair, lit so the way down reads
 for(auto [y,z]:{std::pair{9.5f,g-2.f},{7.5f,pit+2.2f}})m.lights.push_back(lamp({1.f,y},z,.8f,9.f,5000,0,WorldLightMount::Wall,lE,false));
 m.lights.push_back(lamp({23.f,13.6f},g+2.4f,.6f,8.f,2400,0,WorldLightMount::Wall,lW,false));
 m.lights.push_back(lamp({20.4f,1.f},ov+2.2f,.6f,7.f,2100,0,WorldLightMount::Wall,lS,false));       // the overlook
 for(auto [x,y,yaw]:{std::tuple{1.f,8.f,lE},{19.f,16.8f,lW}})m.lights.push_back(drSwitched(lamp({x,y},pit+3.2f,.7f,10.f,3000,0,WorldLightMount::Wall,yaw,false,{1.f,.12f,.06f}),shut,false));
 m.lights.push_back(drBeacon({9.4f,23.f},g+2.4f,lN,0,shut));m.lights.push_back(drBeacon({14.6f,23.f},g+2.4f,lN,.7f,shut));
 zone(m,"ENERGY CORE RESEARCH / NORTH GALLERY",0,0,24,6.f,g-.5f,1000);
 zone(m,"ENERGY CORE RESEARCH / SOUTH GALLERY",0,18.f,24,24,g-.5f,1000);
 zone(m,"ENERGY CORE RESEARCH / CORE PIT",0,0,24,24,-1000,g-.5f,PitAir);
 zone(m,"ENERGY CORE RESEARCH",0,0,24,24,-1000,1000,PitAir);
 return p;
}

// DEEP ACCESS CONTROL (manifest 75). The checkpoint to the extraction level: a queue hall, the badge
// readers, a blast door standing open. Through the control booth's glass the Investigator gives his
// last orders on this side: hold the readers, nobody follows. Then he walks into the personnel lift
// behind the booth, the door shuts on him and the cage goes down, toward the drill. His two men are
// left holding a line nobody is coming back to relieve.
std::shared_ptr<const Map> makeDeepAccessControl(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[108];m.definition=CampaignChunks[108];m.definition.ambient=.15f;
 m.wallSurface=SurfaceLabPanel;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 // The floor is a slab over the shaft's depth: the lift hangs in an open shaft, and the hall's east
 // side stops at a rail over the open drop, so the shaft, its headgear and the violet coming up it
 // are in view from the moment the player comes in.
 const float f=-15,R=m.definition.ceiling,sb=f-9.f;
 m.layers.push_back(floorLayer(m.name.c_str(),sb));
 const float x1s=16.3f,x2s=21.8f,y1s=1.3f,y2s=4.5f;                            // the shaft mouth
 for(auto [x1,y1,x2,y2]:{std::tuple{0.f,0.f,24.f,y1s},{0.f,y2s,24.f,24.f},{0.f,y1s,x1s,y2s},{x2s,y1s,24.f,y2s}})wall(m,x1,y1,x2,y2,sb,f,15);
 openEdge(m,{'N',10.f,14.f,f,true,0});                                        // door 0: in from the core hall
 openEdge(m,{'S',9.f,15.f,f,false,0});
 const char* clock="dr_acc_clock";machineryClock(m,clock,60000);
 // --- Plan -------------------------------------------------------------------------------------
 // the shaft rail: a steel parapet at the lip, the hall open over it, closed to the body above
 guardRail(m,16.f,1.f,16.2f,5.9f,f);
 wall(m,15.96f,1.f,16.24f,5.9f,f,f+.12f,13);
 wall(m,16.f,1.f,16.2f,5.9f,f,R,30);
 const int liftDoor=wallDoor(m,16.f,23.f,5.9f,6.1f,19.6f,20.8f,f,R,-1);         // door 1: the lift, his alone
 m.doors[size_t(liftDoor)].requireState=stateId("dr_lift_called");
 drGlaze(m,16.f,6.1f,16.2f,12.9f,f,R);
 // door 2: into the booth. It stays locked until the hoist is down and the lift door has long shut
 // behind him: while the lift door stands open nobody else can be in the booth to follow him through it.
 const int boothDoor=wallDoor(m,15.f,23.f,12.9f,13.1f,17.6f,18.8f,f,R,-1);
 m.doors[size_t(boothDoor)].requireState=stateId("dr_inv_gone");
 wall(m,15.f,13.1f,15.2f,19.f,f,R,3);doorwayY(m,19.f,20.2f,15.f,15.2f,f,R);wall(m,15.f,20.2f,15.2f,23.f,f,R,3);
 wallDoor(m,1.f,9.f,15.f,15.2f,3.f,4.2f,f,R,1);                                // door 3: the armoury
 wall(m,8.8f,15.2f,9.f,23.f,f,R,3);
 // the reader line: three pylons, two lanes, the wall above
 for(float x:{9.43f,12.f,14.57f}){drModel(m,model::lab_door_reader,x,15.1f,f,drN,true);}
 wall(m,9.f,14.95f,15.f,15.25f,f+2.45f,R,3);drProxy(m,9.f,14.84f,15.f,15.36f,f+2.39f,f+2.45f);
 floorLine(m,9.85f,14.4f,14.15f,14.52f,f,13);
 // the blast door, open: jambs, the lintel, the leaf drawn up into it with its foot showing, the
 // screw jacks either side, the sill
 for(float x:{9.f,14.6f})wall(m,x,17.6f,x+.4f,18.4f,f,f+3.3f,8);
 wall(m,9.f,17.6f,15.f,18.4f,f+3.3f,R,8);
 wall(m,9.4f,17.86f,14.6f,18.14f,f+2.55f,f+3.3f,7);wall(m,9.4f,17.84f,14.6f,18.16f,f+2.55f,f+2.75f,13);
 for(float x:{9.52f,14.48f})m.pipes.push_back({{x,18.f},{x,18.f},f,.05f,f+2.55f,6});
 floorLine(m,9.4f,17.9f,14.6f,18.1f,f,13);
 m.lights.push_back(drBeacon({9.f,17.f},f+2.8f,lE,0));
 // --- The queue hall: the lamp and dosimeter lockers in an island facing the door, the suits for ---
 // below, the queue rails to the readers, and by the shaft rail what was boxed for the hoist
 for(int i=0;i<4;++i){drFix(m,model::psx_locker,10.3f+i*.86f,5.6f,f,.64f,.85f,2.42f,kPi*.5f,true);drFix(m,model::psx_locker,10.3f+i*.86f,6.24f,f,.64f,.85f,2.42f,-kPi*.5f,true);}
 drProxy(m,9.87f,5.28f,13.31f,6.56f,f+2.42f,R);
 for(int i=0;i<6;++i)drFix(m,model::psx_locker,1.33f,7.f+i*.86f,f,.64f,.85f,2.42f,0,true);
 drProxy(m,1.f,6.5f,1.7f,11.8f,f+2.42f,R);
 for(float y:{2.2f,3.2f,4.2f}){hungSuit(m,{1.32f,y,f+1.48f},0);}
 wall(m,1.f,1.9f,1.08f,4.5f,f+1.9f,f+1.94f,20);
 for(float x:{7.5f,8.7f})for(float y:{4.f,7.f,10.f})drModel(m,model::pwr_safety_bollard,x,y,f,0,true);
 bench(m,4.5f,13.8f,f);bench(m,3.6f,5.6f,f);
 for(float x:{10.72f,13.28f}){drFix(m,model::pwr_handrail,x,12.6f,f,.08f,3.6f,1.1f,0,true);floorLine(m,x-.5f,10.8f,x-.38f,14.4f,f,14);}
 for(auto [x,y,yaw,sc]:{std::tuple{14.3f,2.5f,.1f,.85f},{14.4f,7.4f,-.15f,.8f}}){drFix(m,model::pallet,x,y,f,1.2f,1.15f,.16f,yaw,true);drModel(m,model::lab_specimen_crate,x,y,f+.16f,yaw,true,sc);}
 kit::prop(m,model::prop_cardboard_box,13.2f,1.7f,f,.4f);
 drFix(m,model::lab_notice_board,5.f,1.035f,f+1.3f,1.2f,.07f,.9f,drS,false);drFix(m,model::lab_wall_clock,7.2f,1.02f,f+2.2f,.402f,.032f,.4f,drS,false);
 extinguisher(m,1.f,5.4f,f+.6f,drE);
 kit::prop(m,model::mine_hard_hat,3.9f,6.4f,f,.279f,.351f,.153f,.4f);kit::prop(m,model::item_dosimeter,5.1f,13.8f,f+.51f,1.1f);
 wallSign(m,12.f,14.89f,f+2.6f,"BADGE AT THE READER","ONE AT A TIME / DOSIMETER ON THE OUTSIDE OF THE SUIT",2.6f,drN);
 // --- The booth: his window on the hall --------------------------------------------------------
 drCounter(m,21.8f,7.f,22.9f,12.2f,f);
 for(float y:{7.8f,9.5f,11.2f})kit::prop(m,model::cctv_tv,22.4f,y,f+.9f,drW);
 drFix(m,model::lab_control_board,22.96f,9.5f,f+1.7f,1.32f,.19f,.6f,drW,false);
 kit::prop(m,model::office_chair,20.8f,9.4f,f,.55f,.55f,.95f,drE);
 terminal(m,{18.4f,7.f},"ACCESS / ORDERS","03:52 EXTRACTION LEVEL CLOSED TO ALL STAFF. DRILL TEAM HOLDS. NO RELIEF.",
  "04:10 THE PLANT GUARD IS BELOW CONTAINMENT. SHE HAS SEEN THE CORE. SHE DOES NOT COME UP.",f-sb);
 m.lights.push_back(drGlow({22.2f,9.5f},f+1.3f,.3f,3.5f,{.3f,.55f,1.f}));
 m.lights.push_back(drTube(19.f,9.5f,R,.35f,5.f,4000,0));
 // the shaft: two compartments. East, the old cage, dark and chained at the landing. West, by the
 // hall's rail, the man hoist: an open deck on a rope, the one thing in the shaft that still runs.
 // The shaft falls away round them nine metres to where the violet comes up the walls.
 const Vec2 hoist{17.6f,3.05f};
 wall(m,19.47f,2.05f,21.53f,4.35f,f-.3f,f,20);                                      // the cage floor
 drModel(m,model::mine_lift,20.5f,3.2f,f,drS,true);
 drFix(m,model::mine_cage_gate,20.5f,4.56f,f,2.1f,.066f,2.547f,0,true);
 wall(m,x1s-.12f,y2s-.04f,x2s+.12f,y2s+.12f,f-.01f,f+.01f,13);                      // the landing edge
 for(float x:{x1s+.1f,19.f,x2s-.1f})m.pipes.push_back({{x,1.2f},{x,1.2f},sb,.09f,R,6});  // guides down the back wall
 wall(m,x1s-.2f,1.f,x2s+.2f,1.3f,R-.6f,R,20);
 for(float x:{hoist.x,20.5f})m.pipes.push_back({{x,1.6f},{x,3.2f},R-.35f,.28f,R-.35f,6});   // the sheaves
 m.pipes.push_back({{20.5f,3.2f},{20.5f,3.2f},f+3.9f,.02f,R-.35f,7});
 for(auto [x1,y1,x2,y2,lo,hi]:{std::tuple{x1s,y1s,x1s+.05f,y2s,sb,f-1.f},{x2s-.05f,y1s,x2s,y2s,sb,f-2.2f},{x1s,y1s,x2s,y1s+.05f,sb,f-1.6f},
                               {x1s,y2s-.05f,x2s,y2s,sb,f-3.f},{22.9f,1.f,23.f,3.2f,f,f+1.4f}})wall(m,x1,y1,x2,y2,lo,hi,34);
 // the headgear: four yellow legs from the shaft floor and the landing up to a head frame under
 // the roof that carries the sheaves, braced across the shaft at the hall's eye level
 for(auto [x,y]:{std::pair{x1s+.18f,1.45f},{x2s-.18f,1.45f}})steelColumn(m,x,y,sb,R-.62f,true);
 for(auto [x,y]:{std::pair{x1s+.18f,4.7f},{x2s-.18f,4.7f}})steelColumn(m,x,y,f,R-.62f,true);
 for(float y:{1.3f,4.6f})wall(m,x1s,y,x2s,y+.24f,R-.62f,R-.3f,18);
 for(float x:{x1s+.06f,x2s-.3f})wall(m,x,1.3f,x+.24f,4.84f,R-.62f,R-.3f,18);
 for(float z:{f+2.6f,f-3.f})wall(m,x1s,1.3f,x2s,1.42f,z,z+.18f,18);
 // the hoist deck rides the scene's clock: at the landing until he is on it, then down the shaft
 // to the bottom, where it stays
 {ActorTrack deck;deck.timerState=stateId("dr_inv_scene");deck.visual=ActorVisual::Cargo;deck.platform=true;deck.footprint={2.f,2.3f};deck.thickness=.22f;
  deck.suspensionTop=R-.4f;deck.keys={{0,hoist,f,0,0,0},{14500,hoist,f,0,0,0},{21500,hoist,sb+.75f,0,0,0},{30000,hoist,sb+.75f,0,0,0}};
  m.actorTracks.push_back(std::move(deck));}
 // the shaft floor: the seam has come up through it, crystal and growth round the buffers
 wall(m,x1s,y1s,x2s,y2s,sb,sb+.4f,34);
 drShard(m,19.2f,1.8f,sb+.4f,.5f,2.4f);drShard(m,21.3f,1.9f,sb+.4f,.4f,1.8f,3,.7f);drShard(m,21.2f,4.f,sb+.4f,.5f,1.5f);drShard(m,19.1f,4.1f,sb+.4f,.3f,1.1f,3,.5f);drShard(m,20.2f,2.9f,sb+.4f,.3f,.9f,0,.4f);
 for(auto [x,z,i,r]:{std::tuple{17.4f,sb+1.2f,1.4f,8.f},{20.6f,sb+1.2f,1.4f,8.f},{18.6f,f-6.f,1.1f,6.f},{18.6f,f-3.5f,1.f,5.f},{18.6f,f-1.2f,.8f,4.f},{17.3f,f-.5f,.9f,3.5f}})m.lights.push_back(drGlow({x,2.9f},z,i,r));
 m.lights.push_back(lamp({19.f,2.9f},R-.8f,.8f,8.f,4600,0,WorldLightMount::Bare,0,false));      // the work light on the headgear
 m.lights.push_back(lamp({18.2f,5.2f},f+2.7f,1.2f,5.f,3400,0,WorldLightMount::Bare,0,false));   // the landing
 for(float x:{x1s+.1f,x2s-.1f}){drFix(m,model::mine_cage_lamp,x,1.36f,f+2.5f,.206f,.201f,.362f,drS,false);
  m.lights.push_back(drGlow({x,1.6f},f+2.65f,.45f,5.5f,{1.f,.6f,.2f},2400));}
 // the control pedestal in the hall, turned to the cage
 drCounter(m,14.6f,4.4f,15.4f,5.8f,f,1.05f,20);
 drFix(m,model::lab_control_board,15.f,5.1f,f+1.05f,1.32f,.19f,.6f,drE,false);
 drFix(m,model::lab_pressure_gauge,15.35f,4.5f,f+1.3f,.29f,.11f,.41f,drE,false);
 m.lights.push_back(lamp({15.f,5.1f},f+1.9f,.3f,2.6f,3400,0,WorldLightMount::Bare,0,false));
 drFix(m,model::pwr_control_cabinet,15.4f,1.22f,f,.8f,.4f,1.6f,drS,true);
 // the hall's roof: deep beams across it, a hanger rod at every crossing of the lamp grid
 for(float y:{7.f,12.f}){wall(m,1.f,y-.18f,16.f,y+.18f,R-.7f,R,12);wall(m,1.f,y-.22f,16.f,y+.22f,R-.74f,R-.7f,8);}
 for(float x:{8.f,14.5f})wall(m,x-.12f,1.f,x+.12f,14.9f,R-.45f,R,12);
 // --- The armoury (southwest): lockers, the cleaning bench, and behind a mesh cage what is left of ---
 // the detail's issue. Most of the shelves are empty; they have been here four days.
 for(int i=0;i<4;++i)drFix(m,model::psx_locker,1.33f,16.2f+i*.86f,f,.64f,.85f,2.42f,0,true);
 drProxy(m,1.f,15.7f,1.7f,19.3f,f+2.42f,R);
 drFix(m,model::mine_mesh_fence,2.6f,20.f,f,3.194f,.062f,2.779f,0,true);
 drFix(m,model::mine_mesh_doorway,5.79f,20.f,f,3.194f,.062f,2.779f,0,false);
 for(auto [x1,x2]:{std::pair{4.19f,5.25f},{6.33f,7.39f}})wall(m,x1,19.97f,x2,20.03f,f,f+2.78f,30);
 wall(m,5.25f,19.97f,6.33f,20.03f,f+2.3f,f+2.78f,30);
 drFix(m,model::psx_locker,8.f,20.36f,f,.64f,.85f,2.42f,kPi*.5f,true);drProxy(m,7.39f,19.9f,8.8f,20.9f,f+2.42f,R);
 for(float x:{2.2f,4.4f,6.6f})shelf(m,{x,22.7f},kPi,f-sb);
 drProxy(m,1.2f,22.4f,7.6f,23.f,f+1.8f,R);
 drModel(m,model::lab_clean_bench,5.f,16.f,f,drS,true);
 kit::prop(m,model::item_walkie,4.7f,16.f,f+.81f,.114f,.376f,.046f,.5f);kit::prop(m,model::mug,5.4f,16.05f,f+.81f,.09f,.09f,.1f,0);
 kit::prop(m,model::office_paper_stack,5.05f,16.f,f+.81f,.33f,.44f,.03f,0);
 drFix(m,model::wooden_chair,5.f,16.9f,f,.417f,.525f,.95f,.3f,true);
 drModel(m,model::lab_specimen_crate,7.4f,17.6f,f,.3f,true,.7f);
 drFix(m,model::lab_notice_board,3.6f,15.235f,f+1.3f,1.2f,.07f,.9f,drS,false);
 extinguisher(m,8.8f,16.4f,f+.6f,drW);
 m.pickupSpawns.push_back({{3.2f,21.4f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{6.4f,21.4f},PickupKind::HandgunAmmo,f});
 drPendant(m,{5.f,17.6f},R,f+2.9f,.9f,6.f,4200,.2f);drPendant(m,{4.4f,21.4f},R,f+2.9f,.55f,4.5f,3600);
 // --- The ready room (southeast): where two men have lived since the relief stopped coming --------
 bench(m,20.3f,22.4f,f);
 drFix(m,model::water_cooler,22.6f,14.2f,f,.35f,.35f,1.1f,drW,true);
 drFix(m,model::wooden_table,19.2f,17.4f,f,1.367f,.724f,.78f,0,true);
 for(auto [x,y,yaw]:{std::tuple{19.2f,16.6f,0.f},{18.6f,18.2f,kPi-.3f},{20.f,18.25f,kPi+.4f}})drFix(m,model::wooden_chair,x,y,f,.417f,.525f,.95f,yaw,true);
 kit::prop(m,model::mug,18.9f,17.3f,f+.78f,.09f,.09f,.1f,0);kit::prop(m,model::mug,19.6f,17.55f,f+.78f,.09f,.09f,.1f,0);
 kit::prop(m,model::item_walkie,19.4f,17.3f,f+.78f,.114f,.376f,.046f,1.9f);
 drCounter(m,21.9f,15.2f,22.9f,18.2f,f);
 drFix(m,model::coffee_machine,22.45f,15.8f,f+.9f,.3f,.35f,.42f,drW,false);kit::prop(m,model::mug,22.3f,16.6f,f+.9f,.09f,.09f,.1f,0);
 for(int i=0;i<3;++i)drFix(m,model::psx_locker,22.665f,19.2f+i*.86f,f,.64f,.85f,2.42f,kPi,true);
 drProxy(m,22.34f,18.75f,23.f,21.4f,f+2.42f,R);
 for(auto [x,y,yaw]:{std::tuple{16.f,20.6f,.05f},{17.3f,20.9f,.12f}})kit::prop(m,model::scrap_bedroll,x,y,f,yaw);
 kit::prop(m,model::prop_cardboard_box,15.9f,22.4f,f,.3f);kit::prop(m,model::mine_hard_hat,16.5f,19.4f,f,.279f,.351f,.153f,2.f);
 drFix(m,model::lab_notice_board,19.f,13.135f,f+1.3f,1.2f,.07f,.9f,drS,false);drFix(m,model::med_first_aid_box,22.94f,13.6f,f+1.2f,.367f,.12f,.349f,drW,false);
 m.pickupSpawns.push_back({{17.f,15.f},PickupKind::Health,f});
 drPendant(m,{19.2f,17.4f},R,f+2.7f,.9f,6.f,2900);
 // --- The detail he leaves behind: both turned to the glass while he speaks, their backs and the ---
 // lockers between them and the door. They go to their posts when he has gone.
 drGuard(m,clock,"dr_acc_heavy",ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,"dr_acc",{{{13.4f,9.4f},f,26000,drE},{{12.f,13.8f},f,60000,drN}},14,1,1.f,0,130);
 drGuard(m,clock,"dr_acc_walk",ActorVisual::Cleanup,GuardSquadRole::Assault,"dr_acc",
  {{{12.6f,11.6f},f,24000,drE},{{5.f,8.f},f,4000,drE},{{6.f,12.f},f,4000,drS},{{12.f,9.f},f,3000,drN},{{14.f,7.6f},f,3000,drW}},13,1,1.2f);
 // --- The Investigator: at the glass, his orders, the hoist --------------------------------------
 // The scene runs from the door: he is in view over the lockers' end and across the shaft rail the
 // whole way, from the glass to the landing to the deck, and then the deck takes him down the shaft
 // and out of sight under the landing at the bottom. The lift door opens only for him and shuts 2.8 s
 // later; the booth's own door (door 2) stays locked until dr_inv_gone, eleven seconds after that.
 drSequence(m,"dr_inv_scene","dr_inv_leaving",30000,7800,9.f,1.f,15.f,4.f);
 {const Vec2 glass{16.8f,9.5f},front{20.2f,7.f},inside{20.2f,5.2f},west{hoist.x,5.2f},under{hoist.x,6.2f};
  const float toFront=std::atan2(front.x-glass.x,front.y-glass.y),bottom=sb+.75f;
  ActorTrack inv;inv.timerState=stateId("dr_inv_scene");inv.visual=ActorVisual::Investigator;inv.scale=1.8f;
  inv.keys={{0,glass,f,drW,0,0},{2000,glass,f,drW,0,2},{2200,glass,f,drW,8,0},{4800,glass,f,drW,8,1},
   {5000,glass,f,toFront,1,0},{7800,front,f,drN,1,2.8f},{8400,front,f,drN,0,0},{9800,inside,f,drN,1,1.4f},{9900,inside,f,drW,1,0},{11700,west,f,drW,1,1.8f},
   {11800,west,f,drN,1,0},{13000,hoist,f,drN,1,1.2f},{13300,hoist,f,drW,0,0},{14500,hoist,f,drW,0,.5f},{21500,hoist,bottom,drW,0,3.f},
   {22000,hoist,bottom,drS,1,0},{24400,under,bottom,drS,1,2.4f},{30000,under,bottom,drS,0,1}};
  m.actorTracks.push_back(std::move(inv));}
 drSequence(m,"dr_inv_exit","dr_inv_shut",4000,2800);
 drSequence(m,"dr_inv_down","dr_inv_gone",12500,11200);
 drEvent(m,"dr_inv_door_open",0,0,24,24,"dr_inv_leaving",{drDoor(liftDoor,true),drSet("dr_inv_exit",1)});
 drEvent(m,"dr_inv_door_shut",0,0,24,24,"dr_inv_shut",{drDoor(liftDoor,false),drSet("dr_inv_down",1)});
 {ScriptAction save;save.type=ScriptAction::Type::Checkpoint;drEvent(m,"dr_inv_gone_save",0,0,24,24,"dr_inv_gone",{save});}
 cue(m,"dr_inv_scene",600,Sound::Empty,{16.8f,9.5f},"INVESTIGATOR: Hold the readers. Nobody follows me down.",.7f,f+1.6f);
 cue(m,"dr_inv_scene",3200,Sound::Empty,{13.4f,9.4f},"CLEANUP: Sir. The drill team has asked for relief twice.",.6f,f+1.6f);
 cue(m,"dr_inv_scene",5600,Sound::Empty,{17.6f,8.6f},"INVESTIGATOR: They will be relieved when the seam is quiet.",.7f,f+1.6f);
 cue(m,"dr_inv_scene",8000,Sound::Door,{20.2f,6.f},"",.8f,f+1.2f);
 cue(m,"dr_inv_exit",2850,Sound::Door,{20.2f,6.f},"",.7f,f+1.2f);
 cue(m,"dr_inv_scene",12600,Sound::Empty,hoist,"INVESTIGATOR: The hoist stays at the bottom.",.7f,f+1.6f);
 cue(m,"dr_inv_scene",14500,Sound::LiftMotor,hoist,"",.9f,f+1.5f);
 cue(m,"dr_inv_scene",18000,Sound::LiftCreak,hoist,"",.6f,f-4.f);
 cue(m,"dr_inv_scene",21500,Sound::OneMetalBang,hoist,"",.6f,sb+1.f);
 // --- Light: pendants hung low over the hall so the floor and the men are lit, the reader lanes, ---
 // red over the blast door
 for(auto [x,y,fl]:{std::tuple{4.5f,3.8f,0.f},{11.6f,3.3f,0.f},{4.5f,9.8f,0.f},{11.4f,9.6f,.2f},{12.f,13.f,0.f},{12.f,20.6f,0.f}})
  drPendant(m,{x,y},R,f+3.6f,1.1f,8.f,4800,fl);
 recessedFittings(m);
 zone(m,"DEEP ACCESS CONTROL / BOOTH",16.2f,6.1f,23.f,12.9f);
 zone(m,"DEEP ACCESS CONTROL / LIFT",16.2f,1.f,23.f,5.9f,-1000,1000,ShaftAir);
 zone(m,"DEEP ACCESS CONTROL / ARMOURY",1.f,15.2f,8.8f,23.f);
 zone(m,"DEEP ACCESS CONTROL / READY ROOM",15.2f,13.1f,23.f,23.f);
 zone(m,"DEEP ACCESS CONTROL / BLAST DOOR",9.f,15.2f,15.f,24.f);
 zone(m,"DEEP ACCESS CONTROL",0,0,24,24,-1000,1000,LabAir);
 return p;
}

// EXTRACTION INTERFACE (manifest 76). The end of the labs, and of Chapter 2. The head that research
// drove into the seam sits in its ring at the south of a clean lab, and the seam's own black rock has
// come up round it: through the south wall, across the tiles, crystal and violet growth climbing the
// panels. Through the ring the floor ends at a painted rim over the seam: a void 8 m deep, the drill
// rig standing in it, crystal climbing out of it, one bridge across. On the far bank, dead ahead of
// the ring, the mine head's gate stands open on a lit timber adit, and the Investigator is standing
// in it. He looks across, says three words into his handset, and the gate comes down between them.
// What is left is LAB 4's two lamps, a gate pinned from the mine side, and the way the story goes: down.
// The gate (door 1) and the leaf behind it on the chunk edge (door 0, paired with Mine Access) both
// wait on chapter3_open, which nothing sets until Chapter 3 is released (CampaignExport.h).
std::shared_ptr<const Map> makeExtractionInterface(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[109];m.definition=CampaignChunks[109];m.definition.ambient=.10f;
 m.definition.ceiling=-8.f;                                                   // the cavern's roof, 7 m over the banks
 m.wallSurface=SurfaceRock;m.floorSurface=SurfaceRock;m.ceilingSurface=SurfaceRock;
 // The lab stands on a slab over the seam: past the ring the floor ends at a steel threshold and a
 // rim, and the cave opens as a void 8 m deep. The ring, the void and the mine gate are on one line.
 const float f=-15,R=m.definition.ceiling,slab=f+3.4f,sb=f-8.f;
 const StateId ch3=stateId("chapter3_open"),sealed=stateId("dr_fin_sealed");
 m.layers.push_back(floorLayer(m.name.c_str(),sb));
 const float rimN=12.4f,rimS=17.f,bx1=7.f,bx2=10.6f,gateY=21.5f;
 wall(m,0,0,24,rimN,sb,f,26);wall(m,0,rimS,24,24,sb,f,26);
 openEdge(m,{'N',9.f,15.f,f,false,0});
 openEdge(m,{'S',8.f,16.f,f,true,ch3});                                         // door 0: the edge leaf, sealed until Chapter 3
 // door 1: the mine head gate, standing open until the Investigator has it dropped
 {Door g{10.f,14.f,gateY,1.f,true,false,false,f-sb};g.requireState=ch3;m.doors.push_back(g);}
 const int gate=int(m.doors.size())-1;
 // --- The lab: panel walls, tiled floor, the suspended ceiling breaking up to the south -------------
 wall(m,3.8f,1.f,4.f,9.2f,f,slab+.2f,28);wall(m,20.f,1.f,20.2f,9.2f,f,slab+.2f,28);
 wall(m,1.f,1.f,3.8f,9.2f,f,R,26);wall(m,20.2f,1.f,23.f,9.2f,f,R,26);
 wall(m,4.f,1.f,9.f,1.06f,f,slab,28);wall(m,15.f,1.f,20.f,1.06f,f,slab,28);
 wall(m,4.f,1.06f,20.f,8.8f,f,f+.012f,27);wall(m,9.33f,8.8f,14.67f,11.17f,f,f+.012f,11);    // clean tile, and the steel plate out through the ring
 wall(m,4.f,1.f,20.f,6.6f,slab,slab+.2f,22);ceilingGrid(m,4.f,1.f,20.f,6.6f,slab);
 wall(m,4.f,6.6f,9.f,7.8f,slab,slab+.2f,22);wall(m,13.5f,6.6f,16.f,7.4f,slab,slab+.2f,22);    // ragged where the rock took it
 // the south wall either side of the ring: panels standing, panels pushed in off their studs, panels down
 wall(m,4.f,9.f,5.2f,9.2f,f,slab+.2f,28);wall(m,8.5f,9.f,9.33f,9.2f,f,slab+.2f,28);wall(m,14.67f,9.f,15.4f,9.2f,f,slab+.2f,28);
 wall(m,5.2f,9.f,8.5f,9.2f,f,slab+.2f,30);                                         // where the rock stands in the gap
 wall(m,5.15f,8.62f,6.3f,8.7f,f,f+2.5f,28);wall(m,7.5f,8.5f,8.55f,8.58f,f,f+1.7f,28);          // two panels shoved a foot into the room
 wall(m,7.55f,8.5f,8.5f,8.58f,f+1.7f,f+1.76f,20);
 wall(m,6.1f,6.3f,7.3f,8.1f,f+.012f,f+.07f,28);wall(m,16.4f,4.6f,17.3f,6.2f,f+.012f,f+.07f,28);   // panels face down on the tiles
 wall(m,5.2f,9.f,8.5f,9.2f,f+2.9f,slab+.2f,28);drProxy(m,5.1f,8.4f,8.6f,9.f,f+2.55f,R);                                       // the head of the wall still spans it
 // the seam's rock through the west panels, and the south-east corner of the room gone under it
 drRockFit(m,model::cave_boulder_round,6.9f,9.5f,f-.5f,1.15f,1.f,.6f);wall(m,6.2f,8.8f,7.6f,10.2f,f,R,30);
 drRockFit(m,model::cave_rock_slab,6.6f,8.2f,f-.3f,.62f,1.f,2.8f);
 wall(m,15.4f,7.3f,20.f,9.2f,f,R,30);wall(m,17.f,6.2f,20.f,7.3f,f,R,30);
 drRockFit(m,model::cave_rock_column,18.6f,8.f,f-.3f,1.4f,1.05f,.4f);
 drRock(m,model::cave_boulder_a,16.3f,8.3f,f-.4f,1.05f,2.3f);
 drRockFit(m,model::cave_rock_slab,15.7f,6.5f,f-.3f,.6f,1.f,.9f);wall(m,15.f,6.f,16.5f,7.f,f,f+.7f,30);
 drRockFit(m,model::cave_stalactite,11.9f,8.3f,R-3.1f,.85f,1.05f,.6f);
 drRockFit(m,model::cave_ceiling_rock,17.5f,8.f,R-2.7f,1.3f,.85f,1.2f);
 // growth climbing the panels, crystal standing out of the rock's edge
 for(auto [x1,y1,x2,y2,lo,hi]:{std::tuple{4.f,7.f,4.06f,9.f,f,f+2.2f},{19.94f,4.5f,20.f,6.4f,f,f+2.8f},{10.f,8.6f,14.f,8.8f,f,f+.02f},{5.2f,8.56f,6.2f,8.62f,f,f+1.4f}})
  wall(m,x1,y1,x2,y2,lo,hi,34);
 drPrim(m,"dome",5.f,8.f,f,1.6f,1.3f,.14f,0,34);drPrim(m,"dome",16.6f,5.6f,f,2.2f,1.6f,.16f,0,34);
 drShard(m,15.6f,6.3f,f+.5f,.16f,1.3f);drShard(m,16.3f,6.5f,f+.6f,.12f,1.f,2,.5f);drShard(m,17.2f,6.1f,f+1.f,.2f,2.1f,2,.9f);
 drShard(m,5.3f,8.1f,f+.3f,.14f,1.1f);drShard(m,6.f,7.9f,f+.3f,.1f,.9f,2,.4f);
 drKnot(m,7.3f,10.3f,f,1.3f,1);drKnot(m,16.f,10.2f,f,1.f,2);m.lights.push_back(drGlow({6.7f,10.8f},f+1.f,.5f,3.5f,{.5f,.6f,1.f}));
 m.lights.push_back(drGlow({16.2f,5.7f},f+1.9f,1.7f,3.6f,{.5f,.75f,1.f}));m.lights.push_back(drGlow({5.6f,7.5f},f+1.3f,1.4f,3.2f,{.5f,.75f,1.f}));
 // the interface: the control benches either side of the way in, the analysers, the logger
 drCounter(m,4.f,1.06f,9.f,1.72f,f);drCounter(m,15.f,1.06f,20.f,1.72f,f);
 for(float x:{5.5f,17.5f})drFix(m,model::lab_control_board,x,1.08f,f+1.1f,1.32f,.19f,.6f,drS,false);
 kit::prop(m,model::crt_monitor,7.6f,1.4f,f+.9f,.42f,.4f,.38f,0);kit::prop(m,model::lab_field_meter,16.2f,1.4f,f+.9f,.25f,.328f,.148f,.4f);
 drModel(m,model::lab_analyzer_cabinet,5.17f,4.2f,f,drE,true);drProxy(m,4.f,3.25f,6.3f,5.15f,f+3.11f,slab);
 drModel(m,model::lab_mainframe,18.8f,3.4f,f,drW,true);drProxy(m,17.65f,2.25f,20.f,4.55f,f+3.18f,slab);
 terminal(m,{8.6f,2.6f},"EXTRACTION INTERFACE / HEAD","THE HEAD WAS SET INTO THE SEAM AT 900 M AND THE LAB WAS BUILT ROUND IT. FEED TO THE CORE, SAMPLES TO THE ARCHIVE.",
  "02:40 THE SEAM MOVED. NOT THE HEAD: THE SEAM. IT CAME UP TO MEET US.",f-sb);
 for(float x:{6.f,18.f})servicePipe(m,{x,1.2f},{x,x<12?9.f:7.3f},slab-.4f,.08f,3);
 kit::prop(m,model::office_chair,7.f,3.f,f+.012f,.55f,.55f,.95f,2.6f);
 propPose(m,model::office_chair,13.6f,5.4f,f+.012f,1.2f,kPi*.5f,0);
 drBody(m,"dr_ext_clock",ActorVisual::FemaleScientist,{11.f,6.2f},f+.012f,2.4f);machineryClock(m,"dr_ext_clock",60000);
 paperDrift(m,8.f,3.f,15.f,7.6f,f+.012f,7,1091);
 m.lights.push_back(drTube(8.f,3.4f,slab,.6f,6.f,5600,.03f,.15f));
 m.lights.push_back(drTube(16.f,3.4f,slab,.45f,5.f,5600,.03f,.7f));
 m.lights.push_back(drTube(12.f,5.2f,slab,.6f,6.f,5600,.03f));
 drDeadTube(m,12.f,3.4f,slab);drDeadTube(m,8.f,6.f,slab);
 // the ring the head was driven through: the way on, two cage lamps clamped to it by whoever came
 // through last, the cable torn out of it still live
 drModel(m,model::mine_shield_ring,12.f,9.6f,f,drS,false);
 for(float x:{9.33f,14.f})wall(m,x,8.03f,x+.67f,11.17f,f,f+3.79f,30);
 wall(m,10.f,8.03f,14.f,11.17f,f+3.f,f+3.79f,30);
 for(float x:{10.2f,13.8f}){drFix(m,model::mine_cage_lamp,x,7.86f,f+2.3f,.206f,.201f,.362f,0,false);m.pipes.push_back({{x,7.86f},{x,8.05f},f+2.62f,.012f,f+2.9f,6});
  m.lights.push_back(lamp({x,7.6f},f+2.25f,3.f,4.2f,3400,0,WorldLightMount::Bare,0,false));}
 m.lights.push_back(lamp({12.f,9.6f},f+2.8f,1.2f,4.2f,4200,0,WorldLightMount::Bare,0,false));           // inside the bore
 looseLead(m,{11.f,8.1f,f+2.99f},{11.6f,7.6f,f},2.6f,7,true,.018f);
 {Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=10.6f;h.x2=12.2f;h.y1=7.f;h.y2=8.2f;h.bottom=f;h.top=f+2.2f;h.damagePerSecond=25;h.period=2.4f;h.onTime=.6f;m.hazards.push_back(h);}
 // --- The threshold: a steel deck out of the ring to a painted rim, rails, two floods on stands ----
 // aimed across the void at the far bank. The last clean thing in the chapter.
 wall(m,6.f,11.17f,17.4f,rimN,f,f+.02f,20);floorLine(m,5.6f,rimN-.16f,18.4f,rimN,f+.02f,13);
 for(float x=6.6f;x<17.f;x+=1.2f)wall(m,x,11.17f,x+.03f,rimN-.16f,f+.02f,f+.025f,8);
 guardRail(m,5.6f,rimN-.08f,bx1,rimN,f);guardRail(m,bx2,rimN-.08f,18.4f,rimN,f);
 for(float x:{6.5f,16.6f}){m.pipes.push_back({{x,rimN-.3f},{x,rimN-.3f},f,.06f,f+3.2f,1});wall(m,x-.16f,rimN-.46f,x+.16f,rimN-.14f,f+3.15f,f+3.4f,20);
  for(float o:{-.5f,.5f})m.pipes.push_back({{x+o,rimN-.3f-.35f},{x,rimN-.3f},f,.025f,f+1.2f,6});
  m.lights.push_back(lamp({x,rimN+.25f},f+3.05f,3.4f,15.f,4300,0,WorldLightMount::Bare,0,true));}
 // --- The cavern: the masses either side are the seam's black rock, not walls. Their cores only ----
 // collide; what is seen is the pack's columns, blocks and boulders standing in front of the dark.
 for(auto [x1,y1,x2,y2,lo]:{std::tuple{1.f,9.2f,5.6f,rimN,f},{1.f,rimN,5.f,rimS,sb},{1.f,rimS,6.f,21.3f,f},
                            {18.4f,9.2f,23.f,rimN,f},{19.f,rimN,23.f,rimS,sb},{18.f,rimS,23.f,21.3f,f}})wall(m,x1,y1,x2,y2,lo,R,30);
 // west: the ledge beside the ring, the void's wall, the far bank
 drRockFit(m,model::cave_rock_column,4.2f,10.7f,f-.2f,1.75f,1.17f,.3f);
 drRock(m,model::cave_boulder_a,5.5f,9.8f,f-.4f,.85f,1.2f);
 drRockFit(m,model::cave_rock_column,3.4f,14.6f,sb-.3f,2.2f,1.19f,.1f);
 drRockFit(m,model::cave_rock_column,3.2f,15.f,f-1.8f,2.1f,1.18f,3.3f);
 drRock(m,model::cave_boulder_round,5.3f,16.1f,sb-.4f,1.4f,1.1f);
 drRockFit(m,model::cave_rock_block,4.6f,13.1f,f-3.4f,1.2f,1.f,.2f);
 drRockFit(m,model::cave_rock_column,4.5f,19.4f,f-.2f,1.75f,1.16f,2.9f);
 drRock(m,model::cave_boulder_a,5.9f,20.9f,f-.3f,.8f,.7f);
 // east
 drRockFit(m,model::cave_rock_column,19.8f,10.9f,f-.2f,1.75f,1.17f,3.4f);
 drRock(m,model::cave_boulder_round,18.2f,9.9f,f-.4f,.8f,2.2f);
 drRockFit(m,model::cave_rock_column,20.6f,14.4f,sb-.3f,2.2f,1.19f,3.3f);
 drRockFit(m,model::cave_rock_column,20.8f,15.f,f-1.8f,2.1f,1.18f,.2f);
 drRock(m,model::cave_boulder_a,18.8f,16.f,sb-.5f,1.5f,2.2f);
 drRockFit(m,model::cave_rock_column,19.5f,19.3f,f-.2f,1.75f,1.16f,.3f);
 drRock(m,model::cave_boulder_round,18.f,20.8f,f-.4f,.85f,2.f);
 // the void's own faces under both banks: the slab's flat cut is behind boulders below and a
 // course of bedrock blocks above, set back so the chasm keeps its width
 drRockFit(m,model::cave_boulder_a,8.6f,10.9f,sb-.4f,2.1f,1.f,.1f);drRockFit(m,model::cave_boulder_a,15.4f,10.8f,sb-.5f,2.f,1.1f,3.3f);
 for(auto [x,yaw,sc]:{std::tuple{6.6f,.1f,1.45f},{10.6f,3.2f,1.5f},{14.6f,.05f,1.4f},{17.8f,3.1f,1.35f}})drRockFit(m,model::cave_rock_block,x,11.6f,sb+4.f,sc,1.f,yaw);
 drRockFit(m,model::cave_boulder_a,9.4f,18.6f,sb-.4f,2.1f,1.05f,3.2f);drRockFit(m,model::cave_boulder_a,15.8f,18.5f,sb-.5f,2.f,1.f,.2f);
 for(auto [x,yaw,sc]:{std::tuple{6.2f,3.1f,1.4f},{9.8f,.1f,1.5f},{13.6f,3.2f,1.5f},{17.4f,.1f,1.4f}})drRockFit(m,model::cave_rock_block,x,17.8f,sb+3.8f,sc,1.f,yaw);
 // the roof: bosses and stalactites where the floods reach
 drRockFit(m,model::cave_ceiling_rock,8.f,18.8f,R-2.5f,1.15f,.9f,.4f);
 drRockFit(m,model::cave_ceiling_rock,16.4f,18.4f,R-2.7f,1.3f,.9f,2.1f);
 drRockFit(m,model::cave_stalactite,6.4f,14.9f,R-3.4f,.85f,1.1f,1.f);
 drRockFit(m,model::cave_stalactite,17.9f,13.7f,R-2.9f,.75f,1.1f,2.4f);
 drRockFit(m,model::cave_stalactite_cluster,11.f,14.6f,R-1.7f,.9f,1.f,.5f);
 drRockFit(m,model::cave_stalactite,13.6f,10.6f,R-2.7f,.7f,1.1f,4.f);
 // --- The bridge ---------------------------------------------------------------------------------
 wall(m,bx1,rimN,bx2,rimS,f-.32f,f,20);                                                       // the deck
 for(float x:{bx1+.25f,bx2-.25f})m.pipes.push_back({{x,rimN},{x,rimS},f-.55f,.12f,f-.55f,6});   // its girders
 for(float y:{13.6f,15.8f})m.pipes.push_back({{bx1+.25f,y},{bx2-.25f,y},f-.55f,.06f,f-.55f,6});
 for(float x:{bx1+.25f,bx2-.25f}){m.pipes.push_back({{x,rimN+.05f},{x,rimN+1.5f},f-2.f,.06f,f-.6f,6});m.pipes.push_back({{x,rimS-.05f},{x,rimS-1.5f},f-2.f,.06f,f-.6f,6});}   // knee braces off both faces
 guardRail(m,bx1,rimN,bx1+.08f,rimS,f);guardRail(m,bx2-.08f,rimN,bx2,rimS,f);
 // nobody steps off: proxies along both rims and both sides of the deck; a rock lip on the far rim
 for(auto [x1,y1,x2,y2]:{std::tuple{5.6f,rimN,bx1,rimN+.06f},{bx2,rimN,18.4f,rimN+.06f},{5.f,rimS-.06f,bx1,rimS},{bx2,rimS-.06f,19.f,rimS},
                         {bx1,rimN,bx1+.06f,rimS},{bx2-.06f,rimN,bx2,rimS}})wall(m,x1,y1,x2,y2,f,R,30);
 drRockFit(m,model::cave_rock_slab,14.7f,17.5f,f-.55f,.62f,1.f,.05f);drRockFit(m,model::cave_rock_slab,16.9f,17.6f,f-.5f,.6f,1.f,3.f);wall(m,13.7f,rimS,18.f,17.5f,f,f+.5f,30);
 // --- The seam floor: growth in blisters, crystal in knots, the tallest climbing past the rim -----
 for(auto [x,y,w,d,h]:{std::tuple{12.f,14.8f,7.f,3.6f,.5f},{17.f,14.9f,4.f,3.4f,.7f},{7.f,14.8f,3.6f,3.4f,.4f},{14.2f,13.6f,2.6f,1.6f,.9f},{9.6f,16.f,2.4f,1.6f,.7f}})drPrim(m,"dome",x,y,sb,w,d,h,0,34);
 drKnot(m,18.3f,15.5f,sb+.3f,9.2f,1);drKnot(m,17.2f,16.3f,sb+.3f,6.f,2);drKnot(m,18.5f,13.6f,sb+.2f,4.4f,3);
 drKnot(m,6.1f,14.7f,sb+.2f,5.2f,0);drKnot(m,11.4f,14.3f,sb+.3f,2.8f,2);drKnot(m,13.f,16.f,sb+.3f,3.6f,1);drKnot(m,9.2f,13.6f,sb+.2f,2.2f,3);
 for(auto [x,y,z,i,r,gel]:{std::tuple{12.f,14.9f,sb+1.6f,1.6f,9.f,Violet},{17.3f,14.6f,sb+3.f,1.3f,8.f,Point3{.55f,.5f,1.f}},{7.6f,14.9f,sb+1.4f,1.1f,6.5f,Violet},{17.6f,15.f,f+.4f,.9f,5.f,Point3{.5f,.6f,1.f}}})
  m.lights.push_back(lamp({x,y},z,i,r,6500,0,WorldLightMount::Bare,0,true,gel));
 // --- The rig, east of the line: the pack's headframe over the hole, the string, the head in ore ---
 {const Vec2 d{15.2f,14.9f};
  drFix(m,model::mine_headframe,d.x,d.y,sb,3.5f,3.46f,12.5f,0,false);
  m.pipes.push_back({d,d,sb+2.4f,.22f,f+2.f,6});                                               // the string
  m.pipes.push_back({d,d,sb+.6f,.9f,sb+2.4f,8});m.pipes.push_back({d,d,sb+.2f,.7f,sb+.6f,6});    // the head
  m.pipes.push_back({{16.9f,11.75f},{d.x+.6f,d.y-1.f},f+.15f,.07f,f+1.6f,7});m.pipes.push_back({{17.2f,11.75f},{d.x+.8f,d.y-.8f},f+.15f,.07f,f+1.7f,7});
  kit::prop(m,model::pwr_cable_bundle,16.9f,11.55f,f+.02f,.2f,.55f);
  drFix(m,model::pwr_control_cabinet,17.6f,11.f,f,.8f,.4f,1.6f,drN,true);drProxy(m,17.2f,10.8f,18.f,11.2f,f+1.6f,R);
  m.lights.push_back(lamp({d.x-.6f,d.y-.9f},sb+3.2f,1.8f,7.f,4600,0,WorldLightMount::Bare,0,false));
  m.lights.push_back(lamp({d.x,d.y-1.9f},f+.6f,1.6f,6.f,3000,0,WorldLightMount::Bare,0,false));
  hazard(m,Hazard::Kind::Anomaly,d.x-1.6f,13.3f,d.x+1.6f,16.6f,sb,sb+2.f,4.f);}                 // the live pool round its head
 // --- The far bank and the mine head ------------------------------------------------------------
 // the gate wall: rock either side and over a concrete portal; behind the gate a timber adit, two
 // cage lamps and the edge leaf
 wall(m,0.f,21.3f,10.f,24.f,f,R,26);wall(m,14.f,21.3f,24.f,24.f,f,R,26);wall(m,10.f,21.3f,14.f,24.f,f+3.3f,R,26);
 // the mine head is a cast bulkhead across the cavern's end: concrete to the roof between the rock, ribbed
 wall(m,5.f,21.2f,9.f,21.3f,f,R,15);wall(m,15.f,21.2f,19.f,21.3f,f,R,15);wall(m,9.f,21.2f,15.f,21.3f,f+4.02f,R,15);
 for(float x:{6.4f,8.f,16.f,17.6f})wall(m,x-.22f,21.02f,x+.22f,21.2f,f,R,15);
 wall(m,5.f,21.1f,19.f,21.2f,f+4.02f,f+4.3f,7);
 for(float x:{9.f,14.f})wall(m,x,20.8f,x+1.f,21.3f,f,f+3.9f,15);                                  // piers
 wall(m,9.f,20.8f,15.f,21.3f,f+2.65f,f+3.9f,15);                                                // the header
 wall(m,9.f,20.78f,15.f,20.8f,f+2.65f,f+2.8f,13);wall(m,8.94f,20.74f,15.06f,21.3f,f+3.9f,f+4.02f,7);
 for(float x:{9.f,14.92f})wall(m,x,20.78f,x+.08f,20.8f,f,f+2.65f,13);
 wallSign(m,12.f,20.74f,f+3.05f,"NO LAMP, NO ENTRY","TALLY IN / TALLY OUT / BY ORDER OF THE MINE MANAGER",3.6f,drN);
 drFix(m,model::lab_pa_horn,13.9f,20.6f,f+3.3f,.495f,.564f,.433f,drN,false);
 for(float x:{10.03f,13.91f})wall(m,x,21.7f,x+.06f,23.4f,f,f+3.3f,32);                             // lagging
 wall(m,10.f,21.7f,14.f,23.4f,f+3.24f,f+3.3f,32);
 for(float y:{22.05f,23.05f})drFix(m,model::mine_timber_set,12.f,y,f,3.7f,.3f,3.2f,0,false);
 drFix(m,model::mine_rail_straight,12.f,20.3f,f,1.5f,6.2f,.12f,0,false);
 wall(m,11.1f,17.15f,12.9f,17.4f,f,f+.45f,32);for(float x:{11.2f,12.6f})wall(m,x,17.4f,x+.2f,17.7f,f,f+.6f,12);   // the buffer at the rim
 for(float x:{11.f,13.f}){drFix(m,model::mine_cage_lamp,x,22.55f,f+2.85f,.206f,.201f,.362f,0,false);
  m.lights.push_back(drSwitched(lamp({x,22.5f},f+2.7f,2.2f,7.5f,2500,0,WorldLightMount::Bare,0,false),sealed,true));}
 m.lights.push_back(lamp({12.f,20.5f},f+2.45f,2.6f,8.f,3600,0,WorldLightMount::Bare,0,true));           // the lamp under the header
 drFix(m,model::mine_cage_lamp,12.f,20.62f,f+2.29f,.206f,.201f,.362f,0,false);
 for(auto [x,ph]:{std::pair{9.5f,0.f},{14.5f,.7f}}){auto l=lamp({x,20.8f},f+2.2f,1.8f,7.f,3000,0,WorldLightMount::Wall,lN,true,{1.f,.12f,.06f});
  l.period=1.4f;l.onTime=.7f;l.phase=ph;l.enabledState=sealed;m.lights.push_back(l);}
 // the rock round the portal and the seam showing through it: crystal out of the wall over the gate
 drRockFit(m,model::cave_rock_column,7.f,22.4f,f-.2f,1.75f,1.16f,1.57f);
 drRockFit(m,model::cave_rock_column,17.f,22.4f,f-.2f,1.75f,1.16f,4.71f);
 drRockFit(m,model::cave_ceiling_rock,12.f,21.6f,R-3.1f,1.25f,.88f,0);drRockFit(m,model::cave_ceiling_rock,9.2f,21.9f,R-3.f,1.1f,.9f,2.f);drRockFit(m,model::cave_ceiling_rock,14.9f,22.f,R-3.f,1.15f,.9f,4.f);
 for(auto [x,z,w,h,reach]:{std::tuple{9.4f,f+4.f,.3f,2.2f,1.3f},{10.6f,f+4.3f,.22f,1.6f,.8f},{12.4f,f+4.6f,.34f,2.f,1.5f},{13.8f,f+4.1f,.24f,1.8f,1.f},{15.f,f+3.6f,.3f,2.6f,1.2f},
                           {8.3f,f+1.2f,.26f,2.4f,.9f},{15.9f,f+1.6f,.22f,2.f,.8f},{7.6f,f+3.f,.2f,1.6f,.7f},{16.8f,f+3.8f,.26f,1.9f,.9f}})drShard(m,x,21.12f,z,w,h,2,reach);
 drShard(m,8.5f,20.5f,f,.2f,1.5f);drShard(m,15.6f,20.6f,f,.16f,1.1f);
 drModel(m,model::cave_crystal_bed_blue,8.4f,20.5f,f,.4f,false,1.2f);drModel(m,model::cave_crystal_bed_blue,15.7f,20.5f,f,1.4f,false,1.f);
 for(auto [x,z,i]:{std::tuple{8.4f,f+2.4f,.9f},{15.8f,f+2.8f,.9f},{12.f,f+5.4f,1.1f}})m.lights.push_back(lamp({x,20.35f},z,i,5.5f,6500,0,WorldLightMount::Bare,0,true,{.5f,.6f,1.f}));
 // the bank: the tub left off the rails, spoil, two posts with the detail's lamps on them
 drModel(m,model::mine_ore_cart,15.5f,18.6f,f,kPi*.5f+.3f,true);
 drModel(m,model::mine_muck_pile,16.5f,19.6f,f,.3f,false);drModel(m,model::mine_muck_pile,7.6f,19.8f,f,2.f,false,1.3f);
 kit::prop(m,model::mine_shovel,16.9f,19.f,f,.114f,.313f,1.514f,.6f);kit::prop(m,model::mine_hard_hat,14.3f,18.2f,f,.279f,.351f,.153f,1.9f);
 for(auto [x,y]:{std::pair{6.6f,19.f},{17.3f,18.2f}}){drFix(m,model::mine_prop_post,x,y,f,.336f,.262f,2.889f,0,true);
  drFix(m,model::mine_cage_lamp,x+(x<12?.24f:-.24f),y,f+2.3f,.206f,.201f,.362f,0,false);
  m.lights.push_back(lamp({x+(x<12?.4f:-.4f),y},f+2.2f,2.f,6.5f,2300,0,WorldLightMount::Bare,0,true));}
 // the detail's handset, on a crate by the east pier
 drFix(m,model::wooden_crate,15.5f,20.3f,f,.8f,.8f,.8f,.2f,true);
 const Vec2 radio{15.45f,20.25f};kit::prop(m,model::item_walkie,radio.x,radio.y,f+.8f,.114f,.376f,.046f,.9f);
 // LAB 4's lamps: one on a block at the bridge's far end, one at the gate under her note
 wall(m,6.8f,17.75f,7.8f,18.45f,f,f+.9f,30);drRockFit(m,model::cave_rock_block,7.3f,18.1f,f-.14f,.36f,1.f,.2f);
 {const int lamp1=int(m.clutterSpawns.size());kit::prop(m,model::scrap_lantern,7.3f,18.1f,f+.9f,.8f);
  m.lights.push_back(drGlow({7.3f,18.1f},f+1.45f,.9f,5.5f,{1,1,1},2600));m.lights.back().prop=lamp1;}
 {const int lamp2=int(m.clutterSpawns.size());kit::prop(m,model::scrap_lantern,9.5f,20.4f,f,2.2f);
  m.lights.push_back(drGlow({9.5f,20.4f},f+.55f,.8f,4.5f,{1,1,1},2600));m.lights.back().prop=lamp2;}
 taped(m,{9.5f,20.8f},f+1.45f,drN,Readable::Sticky,"LAB 4","I WENT DOWN BEFORE HE PINNED IT.","THE LAMPS ARE LIT ALL THE WAY TO THE BOTTOM. FOLLOW THEM WHEN IT OPENS.");
 // the gate's own control, west of the portal
 if(Chapter3Released){
  terminal(m,{8.1f,19.6f},"MINE HEAD / GATE CONTROL","04:31 GATE DOWN AND PINNED FROM THE MINE SIDE. AUTHORITY: INTERNAL INVESTIGATIONS.",
   "PIN WITHDRAWN AT THE 900 LEVEL. E / LOCAL RELEASE.",f-sb,stateId("dr_gate_release"),stateId("dr_fin_done"));
  ScriptAction voice;voice.type=ScriptAction::Type::PlaySound;voice.sound=Sound::PaDoorsUnlocked;voice.amount=.8f;
  drEvent(m,"dr_gate_released",0,0,24,24,"dr_gate_release",{drSet("chapter3_open",1),voice});
 }else terminal(m,{8.1f,19.6f},"MINE HEAD / GATE CONTROL","04:31 GATE DOWN AND PINNED FROM THE MINE SIDE. AUTHORITY: INTERNAL INVESTIGATIONS.",
  "LOCAL RELEASE REFUSED. THE PIN IS HELD AT THE DRILL TEAM'S KEY SWITCH, 900 LEVEL.",f-sb);
 // --- The last beat ----------------------------------------------------------------------------
 // He is standing in the gate from the moment the map loads, a pace behind the leaf's line. The scene
 // starts as the player comes out of the ring onto the threshold, facing him: the order into his
 // handset, the gate down, the adit's light pinched out, red on the piers, and what the noise wakes.
 // Stepping onto the bridge before he has spoken drops the gate at once: nobody reaches it open.
 const Vec2 inGate{12.f,22.35f},inAdit{12.f,23.f};
 {ActorTrack inv;inv.timerState=stateId("dr_fin");inv.visual=ActorVisual::Investigator;inv.scale=1.8f;
  inv.keys={{0,inGate,f,drN,0,0},{400,inGate,f,drN,6,0},{2500,inGate,f,drN,6,1},{2700,inGate,f,drN,0,0},{4300,inGate,f,drN,0,.6f},
   {5000,inGate,f,drS,0,0},{5200,inGate,f,drS,1,0},{6100,inAdit,f,drS,1,1},{6300,inAdit,f-4.f,drS,0,0},{9000,inAdit,f-4.f,drS,0,1}};
  m.actorTracks.push_back(std::move(inv));}
 {TimedSequence s;s.timerState=stateId("dr_fin");s.finishState=stateId("dr_fin_order");s.x1=9.4f;s.y1=10.2f;s.x2=14.6f;s.y2=rimN;
  s.durationMs=9000;s.finishAtMs=2500;s.soundIntervalMs=0;s.soundUntilMs=0;m.timedSequences.push_back(s);}
 drSequence(m,"dr_fin_drop","dr_fin_sealed",1600,1150);
 cue(m,"dr_fin",500,Sound::RadioSquelch,inGate,"INVESTIGATOR: Mine head. She is at the interface.",.9f,f+1.6f);
 cue(m,"dr_fin",2300,Sound::Empty,inGate,"INVESTIGATOR: Close it.",.9f,f+1.6f);
 cue(m,"dr_fin_drop",1,Sound::OneMetalDrag,{12.f,gateY},"",1.f,f+1.6f);
 cue(m,"dr_fin_drop",1150,Sound::OneMetalBang,{12.f,gateY},"",1.f,f+1.f);
 {ScriptCondition notYet;notYet.id=stateId("dr_fin_drop");notYet.op=ScriptCondition::Op::Equal;notYet.value=0;
  ScriptEvent e;e.id=stateId("dr_fin_close");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=stateId("dr_fin_order");e.conditions={notYet};
  e.actions={drDoor(gate,false),drSet("dr_fin_drop",1)};m.scriptEvents.push_back(e);
  e.id=stateId("dr_fin_rush");e.x1=5.f;e.y1=rimN;e.x2=19.f;e.y2=21.3f;e.requireState=0;m.scriptEvents.push_back(e);
  ScriptCondition unseen;unseen.id=stateId("dr_fin");unseen.op=ScriptCondition::Op::Equal;unseen.value=0;
  e.id=stateId("dr_fin_unseen");e.conditions={unseen};e.actions={drSet("dr_fin",2600)};m.scriptEvents.push_back(e);}
 {ScriptAction shake;shake.type=ScriptAction::Type::Shake;shake.amount=.45f;
  drEvent(m,"dr_fin_slam",0,0,24,24,"dr_fin_sealed",{shake});}
 m.ambients.push_back({Sound::Sting,{12.f,18.f},f+2.f,.6f,30,AmbientEmitter::OnState,0,0,sealed,1});
 m.ambients.push_back({Sound::PaDoorsLocked,{13.9f,20.6f},f+3.3f,.6f,26,AmbientEmitter::OnState,0,0,sealed,1});
 // what the slam wakes: the mine head's own shift, still at the gate they were told to hold
 m.creatureSpawns.push_back({CreatureKind::Transformed,{8.8f,16.2f},f,1,0,sealed});
 m.creatureSpawns.push_back({CreatureKind::Transformed,{13.6f,18.9f},f,1,0,sealed});
 m.creatureSpawns.push_back({CreatureKind::Mutant,{10.6f,19.6f},f,1,0,sealed});
 // when the bank is quiet and the player is at the gate, the handset: where everyone is going
 drSequence(m,"dr_fin_radio","dr_fin_done",15000,13600);
 {ScriptEvent e;e.id=stateId("dr_fin_arrive");e.x1=8.6f;e.y1=18.2f;e.x2=16.4f;e.y2=21.3f;e.requireState=sealed;e.requireEnemiesClear=true;
  e.actions={drSet("dr_fin_radio",1)};m.scriptEvents.push_back(e);}
 cue(m,"dr_fin_radio",700,Sound::RadioSquelch,radio,"RADIO: Mine head to control. Gate is down and pinned.",.9f,f+.9f);
 cue(m,"dr_fin_radio",3800,Sound::Empty,radio,"RADIO: Sir, there are lamps lit on the survey drift.",.9f,f+.9f);
 cue(m,"dr_fin_radio",6500,Sound::Empty,radio,"RADIO: They are not ours.",.9f,f+.9f);
 cue(m,"dr_fin_radio",9300,Sound::RadioSquelch,radio,"INVESTIGATOR (RADIO): They are hers. Leave them lit.",.9f,f+.9f);
 cue(m,"dr_fin_radio",12000,Sound::Empty,radio,"INVESTIGATOR (RADIO): She is going where we are going.",.9f,f+.9f);
 {ScriptAction save;save.type=ScriptAction::Type::Checkpoint;
  ScriptAction sting;sting.type=ScriptAction::Type::PlaySound;sting.sound=Sound::StingPiano;sting.amount=.7f;
  ScriptAction end;end.type=ScriptAction::Type::ShowMessage;end.text="END OF CHAPTER 2 / THE MINE HEAD IS SEALED";end.amount=9.f;
  drEvent(m,"dr_fin_end",0,0,24,24,"dr_fin_done",{sting,end,save});
  drEvent(m,"dr_ext_arrive",9.f,1.f,15.f,3.2f,"",{save});}
 m.pickupSpawns.push_back({{5.f,2.6f},PickupKind::Health,f+.012f});
 m.pickupSpawns.push_back({{9.f,10.4f},PickupKind::Ammo,f});
 m.pickupSpawns.push_back({{16.8f,20.2f},PickupKind::HandgunAmmo,f});
 zone(m,"EXTRACTION INTERFACE / THE HEAD",4.f,1.f,20.f,9.2f,-1000,1000,LabAir);
 zone(m,"EXTRACTION INTERFACE / MINE HEAD",1.f,rimS,23.f,24.f,-1000,1000,SeamAir);
 zone(m,"EXTRACTION INTERFACE / THE SEAM",1.f,9.2f,23.f,rimS,-1000,1000,SeamAir);
 zone(m,"EXTRACTION INTERFACE",0,0,24,24,-1000,1000,DustAir);
 return p;
}
}
void buildChapterDeepResearch(CampaignMapTable& maps){
 maps[104]=makeDeepMaterialsLab();
 maps[105]=makeBoreSampleArchive();
 maps[106]=makeGeophysics();
 maps[107]=makeEnergyCoreResearch();
 maps[108]=makeDeepAccessControl();
 maps[109]=makeExtractionInterface();
}
}

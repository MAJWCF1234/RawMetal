// CONTAINMENT (chapter 2, manifest 65-70): chunks 99, 100, 101, 102, 103.
// See docs/Design/descent-campaign-20261025.md (Containment) for the brief.
// Route: from Containment B (98) S into Specimen Handling (99), E across its gallery into the
// Observation Theater (101), N from the theater back into the service corridors (97). From B W into
// the Breach Sector (102), ground zero, down through the crater and S to Emergency Containment (103),
// where the barricade (cont_barricade) is raised from the control room and the S door opens on Deep
// Research (104). The Isolation Wing (100) is the dead end east of the Quarantine Ward: LAB 4's
// bunker, her lamps and her handwriting.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include "Primitives.h"
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
void cbPrim(Map& m,const char* kind,float x,float y,float z,float w,float d,float h,int facing,int mat,bool solid=true){expandPrimitive({kind,x,y,z,w,d,h,facing,mat,solid},m.structures);}
void cbModel(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid=false){m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});}
unsigned cbSeed(float x,float y){unsigned h=unsigned(x*97.f)*2654435761u^unsigned(y*131.f)*40503u;h^=h>>13;h*=1274126177u;return h^(h>>16);}
// Resonance growth and crystal, the same as CampaignChapterContainmentA.cpp builds them: the
// growth is true rounded geometry in its own finish with the cave pack's pods budding on it, the
// crystal chisel-tipped prisms in the self-lit finish over the pack's bed of spikes.
// A crust on a surface: a flat sheet.
void cbCrust(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){wall(m,x1,y1,x2,y2,lo,hi,34);}
// A blister of crust on a floor: round, a hand high, walked over.
void cbLens(Map& m,float x,float y,float f,float w,float d,float h=.09f){cbPrim(m,"dome",x,y,f,w,d,h,0,34,false);}
// A crystal shard from its foot at (x,y,z): a shaft w across and h high under a chisel tip, or
// (lean 0 +y, 1 +x, 2 -y, 3 -x) a prism leaning out `reach` metres that way as it rises h.
void cbShard(Map& m,float x,float y,float z,float w,float h,int lean=-1,float reach=0){
 if(lean<0){const float tip=std::min(h*.3f,w*1.5f);cbPrim(m,"box",x,y,z,w,w*.85f,h-tip,0,35,false);cbPrim(m,"ramp",x,y,z+h-tip,w,w*.85f,tip,int(cbSeed(x,y)&3u),35,false);return;}
 constexpr Vec2 fwd[4]={{0,1},{1,0},{0,-1},{-1,0}};cbPrim(m,"strut",x+fwd[lean].x*reach*.5f,y+fwd[lean].y*reach*.5f,z,w,reach,h,lean,35,h>=1.6f);
}
// The cave pack's bed of crystal spikes at scale s, and its black rock fang at scale s (1 is 4 m tall).
void cbBed(Map& m,float x,float y,float z,float s,float yaw){cbModel(m,model::cave_crystal_bed_blue,x,y,z,1.22f*s,1.22f*s,.245f*s,yaw);}
void cbFang(Map& m,float x,float y,float z,float s,float yaw){cbModel(m,model::cave_stalagmite,x,y,z,1.83f*s,2.55f*s,4.05f*s,yaw);}
// A mound of growth: a blister risen off the floor with a second on its shoulder, pods budding on it.
void cbMound(Map& m,float x,float y,float f,float r,float h){
 const unsigned s=cbSeed(x,y);
 cbPrim(m,"dome",x,y,f,r*2.f,r*1.5f,h,0,34,true);
 cbPrim(m,"dome",x+r*.5f*((s&1u)?1.f:-1.f),y+r*.32f*((s&2u)?1.f:-1.f),f,r*1.15f,r*.95f,h*.62f,0,34,false);
 for(int i=0;i<2+int(h>.9f);++i){const float a=float((s>>(4+i*5))%628u)*.01f,k=.38f+.14f*i,sc=.5f+.22f*float((s>>(i*3))&3u)/3.f;
  cbModel(m,model::cave_nest_pod,x+std::cos(a)*r*k,y+std::sin(a)*r*.75f*k,f+h*std::sqrt(std::max(0.f,1-k*k))*.82f-.05f,.7f*sc,.7f*sc,.56f*sc,a);}
}
// A knot of crystal: a chisel-tipped shaft, shards leaning out of its foot three ways, the bed of spikes round it.
void cbKnot(Map& m,float x,float y,float f,float h){
 const unsigned s=cbSeed(x,y);const int d=int(s&3u);const float w=std::clamp(h*.16f,.09f,.3f);
 cbShard(m,x,y,f,w,h);
 cbShard(m,x,y,f,w*.8f,h*.72f,d,h*.5f);cbShard(m,x,y,f,w*.7f,h*.55f,(d+1)&3,h*.45f);
 if(h>.75f)cbShard(m,x,y,f,w*.6f,h*.42f,(d+2)&3,h*.4f);
 cbBed(m,x,y,f,std::max(.5f,h*.9f),float(s%628u)*.01f);
 wall(m,x-w,y-w,x+w,y+w,f,f+h*.8f,CollisionProxyMaterial);
}
void cbPortalY(Map& m,float x1,float x2,float y1,float y2,float f,float roof,float head){
 wall(m,x1,y1-.6f,x2,y1,f,roof,15);wall(m,x1,y2,x2,y2+.6f,f,roof,15);wall(m,x1,y1,x2,y2,f+head,roof,15);
 for(float y:{y1-.08f,y2})wall(m,x1-.04f,y,x2+.04f,y+.08f,f,f+head,20);
}
// Stencilled figures (0-9, A) painted on a wall face along x at y=face, facing +y (dir 1) or -y (dir -1):
// seven-stroke characters h high, the string centred on x, its foot at f+z.
void cbStencil(Map& m,const char* text,float x,float face,int dir,float z,float h,int mat=14){
 const float f=m.layers.front().elevation,w=h*.55f,t=h*.14f,gap=h*.24f,y1=dir>0?face:face-.014f,y2=dir>0?face+.014f:face;z+=f;
 const size_t n=std::char_traits<char>::length(text);const float half=(n*w+(n-1)*gap)*.5f;float cx=0;
 // Strokes are laid out in reading order; read from the -y side the text runs toward -x.
 auto stroke=[&](float a,float b,float lo,float hi){const float xa=x+dir*(a-half),xb=x+dir*(b-half);wall(m,std::min(xa,xb),y1,std::max(xa,xb),y2,lo,hi,mat);};
 for(size_t i=0;i<n;++i,cx+=w+gap){const char c=text[i];
  const int k=c=='A'?0x77:c=='0'?0x3f:c=='1'?0x06:c=='2'?0x5b:c=='3'?0x4f:c=='4'?0x66:c=='5'?0x6d:c=='6'?0x7d:c=='7'?0x07:c=='8'?0x7f:c=='9'?0x6f:0;
  if(k&1)stroke(cx,cx+w,z+h-t,z+h);if(k&8)stroke(cx,cx+w,z,z+t);if(k&64)stroke(cx,cx+w,z+(h-t)*.5f,z+(h+t)*.5f);
  if(k&2)stroke(cx+w-t,cx+w,z+h*.5f,z+h);if(k&4)stroke(cx+w-t,cx+w,z,z+h*.5f);
  if(k&32)stroke(cx,cx+t,z+h*.5f,z+h);if(k&16)stroke(cx,cx+t,z,z+h*.5f);}
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
 m.name=CampaignMapNames[99];m.definition=CampaignChunks[99];m.definition.ambient=.28f;
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
 m.lights.push_back(lamp({21.1f,23.f},fl+2.6f,.6f,6.f,3400,0,WorldLightMount::Wall,blN));            // bay 4, the loading bay: warm, the forklift in it
 for(float cx:{4.1f,10.1f,16.1f}){m.lights.push_back(lamp({cx,23.f},fl+2.6f,.65f,6.f,4300,.25f,WorldLightMount::Wall,blN));cbFix(m,model::med_floor_drain,cx,21.f,fl,0,false);}
 forklift(m,21.1f,20.8f,fl);
 for(float x:{19.6f,20.8f})cbFix(m,model::lab_specimen_crate,x,17.2f,fl,x*.1f,true);
 handCart(m,8.2f,15.8f,fl);
 // The transfer line, the floor's reason, one operation end to end under its own sodium: the
 // holding cage the specimens were loaded from at the north end, a floor track and the hoist beam
 // over it, one sealed crate hanging off the hoist above the cart that was waiting for it, the
 // inspection table beside the line, and at the south end, behind bay 3's gate, the freight lift
 // that took them down. Everything else on the floor is under cold service light.
 const float lx=15.4f,cy=12.8f;
 cbFix(m,model::med_holding_cage,lx,8.45f,fl,kPi*.5f,true);
 wall(m,lx-1.03f,6.2f,lx+1.03f,10.7f,fl+2.01f,fl+2.6f,CollisionProxyMaterial);
 rail(m,lx,10.7f,18.8f,fl);
 floorLine(m,lx-1.25f,10.7f,lx-1.13f,18.8f,fl,13);floorLine(m,lx+1.13f,10.7f,lx+1.25f,18.8f,fl,13);
 wall(m,lx-.09f,6.4f,lx+.09f,18.9f,fl+5.1f,fl+5.5f,18);                                           // the hoist beam
 for(float y:{6.6f,10.4f,14.6f,18.6f})m.pipes.push_back({{lx,y},{lx,y},fl+5.5f,.035f,roof,6});
 wall(m,lx-.22f,cy-.3f,lx+.22f,cy+.3f,fl+4.7f,fl+5.1f,19);                                           // the hoist block
 for(float dy:{-.25f,.25f})m.pipes.push_back({{lx,cy+dy},{lx,cy+dy},fl+2.47f,.02f,fl+4.7f,8});      // its chains to the crate
 wall(m,lx-.56f,cy-.56f,lx+.56f,cy+.56f,fl+1.35f,fl+2.47f,10);                                      // the crate, sealed, in the air
 for(float dx:{-.58f,.5f})for(float dy:{-.58f,.5f})wall(m,lx+dx,cy+dy,lx+dx+.08f,cy+dy+.08f,fl+1.33f,fl+2.49f,12);
 for(float d:{-.3f,.3f})wall(m,lx-.58f,cy+d-.03f,lx+.58f,cy+d+.03f,fl+1.33f,fl+2.49f,13);              // its straps
 wall(m,lx-.6f,cy-.6f,lx+.6f,cy+.6f,fl+2.47f,fl+2.52f,13);
 wall(m,lx-.75f,cy-.9f,lx+.75f,cy+.9f,fl+.3f,fl+.42f,11);                                             // the cart, its deck
 for(float dx:{-.6f,.6f})for(float y:{cy-.7f,cy+.7f})m.pipes.push_back({{lx+dx-.08f,y},{lx+dx+.08f,y},fl+.15f,.14f,fl+.15f,2});
 for(float dx:{-.72f,.69f})wall(m,lx+dx,cy-.9f,lx+dx+.03f,cy+.9f,fl+.15f,fl+.3f,13);
 wall(m,lx-.75f,cy-.9f,lx+.75f,cy+.9f,fl,fl+.3f,CollisionProxyMaterial);
 cbFix(m,model::mine_lift,lx+.2f,21.2f,fl,kPi,true);                                                 // the lift, behind bay 3's gate
 m.lights.push_back(cbGlow({lx+.2f,19.5f},fl+3.4f,.8f,5.f,4800));
 cbPendant(m,{lx-.9f,cy},roof,fl+4.3f,2.3f,10.f,1900,0,-.25f);
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
 cbPendant(m,{5.f,7.4f},roof,fl+5.4f,1.f,9.f,5600);cbPendant(m,{lx-1.4f,8.4f},roof,fl+4.6f,1.2f,8.f,2000,0,-.2f);
 cbPendant(m,{5.f,16.6f},roof,fl+5.4f,0,0,0);cbPendant(m,{lx+.6f,17.4f},roof,fl+4.6f,.9f,8.f,2000,.25f,-.2f);
 m.lights.push_back(lamp({1.f,12.6f},fl+2.6f,.5f,6.f,5000,0,WorldLightMount::Wall,blE));
 // cold service fill: the cage wall, and the floor under the gallery deck, so nothing off the line goes black
 for(float y:{6.6f,18.f})m.lights.push_back(lamp({1.f,y},fl+2.6f,.6f,7.f,5200,0,WorldLightMount::Wall,blE));
 for(float y:{4.5f,11.f})m.lights.push_back(lamp({23.f,y},fl+2.3f,.5f,7.f,5200,0,WorldLightMount::Wall,blW));
 zone(m,"SPECIMEN HANDLING / GALLERY",8.f,0,24.f,15.f,up-.4f,1000,{.13f,.12f,.11f,.008f});
 zone(m,"SPECIMEN HANDLING / BAYS",1.f,19.f,23.f,23.f,-1000,1000,{.12f,.13f,.12f,.008f});
 zone(m,"SPECIMEN HANDLING",0,0,24,24,-1000,1000,{.13f,.12f,.11f,.008f});
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
 // Bunker 2 is the one the corridor is about: its door under one hard red lamp, the viewport
 // widened so you see him standing at it, three restraint bars and a chained hasp across the leaf,
 // and the concrete beside the frame scored by hands, under the detail's warning.
 for(int row=0;row<2;++row){const float y1=row?13.5f:9.3f,y2=y1+.2f,swing=row?1.f:-1.f,face=row?y1:y2,dir=row?-1.f:1.f;float a=0;
  auto onFace=[&](float x1,float x2,float lo,float hi,float out,int mat,bool solid=true){
   wall(m,x1,std::min(face,face+dir*out),x2,std::max(face,face+dir*out),f+lo,f+hi,mat);m.structures.back().solid=solid;};
  for(int k=0;k<4;++k){const float d0=doors[k][0],d1=doors[k][1],cx=(d0+d1)*.5f;const int n=numbers[row][k][0]-'0';
   const float v0=n==2?d0-1.35f:d0-.9f,v1=n==2?d0-.3f:d0-.4f,sill=n==2?1.f:1.3f,head=n==2?1.9f:1.72f;
   wall(m,a,y1,v0,y2,f,roof,15);cbGlaze(m,v0,y1,v1,y2,f,roof,sill,head,true,15);wall(m,v1,y1,d0,y2,f,roof,15);
   const int door=swingDoor(m,d0,d1,(y1+y2)*.5f,f,roof,swing);a=d1;
   if(n==3){m.doors[size_t(door)].open=.9f;m.doors[size_t(door)].opening=true;}                        // 03:52: hers stands open
   if(n==2||n>=5)m.doors[size_t(door)].requireState=stateId("conb_iso_barred");                        // never: the bar is down and nobody is lifting it
   onFace(d0-.18f,d0,0,2.86f,.1f,11);onFace(d1,d1+.18f,0,2.86f,.1f,11);onFace(d0-.18f,d1+.18f,2.55f,2.86f,.1f,11);
   onFace(v0,v1,sill-.06f,sill,.08f,20);
   if(n==2){
    for(float z:{.62f,1.32f,2.02f}){onFace(d0-.3f,d1+.3f,z,z+.11f,.17f,8,false);for(float x:{d0-.42f,d1+.24f})onFace(x,x+.18f,z-.05f,z+.16f,.2f,12);}
    onFace(d1-.16f,d1+.04f,1.05f,1.3f,.24f,19,false);                                                   // the hasp and its padlock
    for(int s2=0;s2<4;++s2)for(int i=0;i<7;++i){const float x=d1+.45f+s2*.13f+i*.07f,z=1.75f-i*.16f-s2*.05f;onFace(x,x+.05f,z,z+.13f,.012f,20,false);}   // scored by hands
    for(int i=0;i<5;++i){const float x=v0+.1f+i*.18f;onFace(x,x+.04f,.45f,.95f-i*.07f,.012f,16,false);}                               // and under the glass
    m.lights.push_back(lamp({cx,face+dir*.03f},f+3.25f,1.7f,5.5f,2000,0,WorldLightMount::Wall,blN,true,{1.f,.07f,.04f}));
    m.fixtures.push_back({model::mine_cage_lamp,{cx,face+dir*.12f},f+3.05f-m.layers.front().elevation,.21f,.2f,.36f,0,false});}
   cbStencil(m,numbers[row][k],cx,face,int(dir),2.98f,.62f);
   for(float x:{d0-.16f,d1+.04f})onFace(x,x+.12f,1.02f,1.26f,.14f,8);                                 // drop-bar brackets
   if(n==2||n>=5)onFace(d0-.24f,d1+.24f,1.08f,1.2f,.12f,8,false);                                     // barred
   if(n==1)onFace(d1+.3f,d1+.38f,0,1.7f,.08f,8);                                                      // its bar lifted off and stood by
   if(n==4){for(float z:{.85f,1.55f})onFace(d0-.1f,d1+.1f,z,z+.08f,.13f,13,false);onFace(d1-.1f,d1+.02f,1.9f,2.5f,.13f,13,false);}   // the seal tape, cut
   if(n==3)for(int i=0;i<5;++i)onFace(d1+.4f,d1+.9f+i*.35f,1.15f+i*.13f,1.17f+i*.13f,.01f,8);        // her pen, on the corridor wall
   m.fixtures.push_back({model::mine_cage_lamp,{d1+.42f,face+dir*.12f},f+2.6f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
   const float amber=n==3?2600.f:n==4?1900.f:4200.f;
   auto l=lamp({d1+.42f,face+dir*.02f},f+2.75f,n==3?.75f:n==4?.45f:.38f,5.5f,amber,0,WorldLightMount::Wall,row?blN:blS,true);
   if(n==7)l.flicker=.45f;if(n!=6&&n!=8&&n!=2)m.lights.push_back(l);}                       // 6 and 8: lamps dead
  wall(m,a,y1,23.f,y2,f,roof,15);}
 for(float x:{6.9f,12.9f,18.9f}){wall(m,x,1.f,x+.2f,9.3f,f,roof,15);wall(m,x,13.7f,x+.2f,23.f,f,roof,15);}
 // The corridor's own structure: steel ribs across the soffit every three metres, a cable tray and
 // two service runs along it, so the long run reads as built, bay by bay, not one concrete tube.
 for(float x=2.5f;x<23.f;x+=3.f){wall(m,x,9.5f,x+.22f,13.5f,roof-.32f,roof,12);for(float y:{9.5f,13.36f})wall(m,x,y,x+.22f,y+.14f,roof-.75f,roof-.32f,12);}
 cableTray(m,{1.f,9.75f},{23.f,9.75f},roof-.24f);
 for(auto [y,paint]:{std::pair{13.15f,3},{12.85f,4}})servicePipe(m,{0.f,y},{23.f,y},roof-.2f,.075f,paint);
 const float bays[4][2]={{1.f,6.9f},{7.1f,12.9f},{13.1f,18.9f},{19.1f,23.f}};
 for(int i=0;i<4;++i){const float a=bays[i][0],b=bays[i][1];
  wall(m,a,1.f,a+2.1f,2.f,f,f+.45f,15);wall(m,a,22.f,a+2.1f,23.f,f,f+.45f,15);                     // the bed slabs
  cbFix(m,model::med_ward_toilet,b-.5f,1.37f,f,cbS,true);cbFix(m,model::med_ward_toilet,b-.5f,22.63f,f,cbN,true);
  // What an isolation cell is issued: a screen wall by the pan, a basin, a drain in the middle of
  // the floor, the extract grille over the bed, the speaker they talk to you through by the door.
  const float mid=(a+b)*.5f;
  wall(m,b-1.15f,1.f,b-1.05f,2.2f,f,f+1.25f,15);wall(m,b-1.15f,21.8f,b-1.05f,23.f,f,f+1.25f,15);
  if(i!=2)cbFix(m,model::med_scrub_sink,b-.3f,2.75f,f,cbW,true);
  cbFix(m,model::med_scrub_sink,b-.3f,21.25f,f,cbW,true);
  cbFix(m,model::med_floor_drain,mid,5.4f,f+.004f,0,false);cbFix(m,model::med_floor_drain,mid,18.f,f+.004f,0,false);
  cbFix(m,model::pwr_vent_grille,a+1.05f,1.03f,f+2.7f,cbS,false);cbFix(m,model::pwr_vent_grille,a+1.05f,22.97f,f+2.7f,cbN,false);
  cbFix(m,model::lab_pa_horn,doors[i][1]+1.1f,9.02f,f+2.5f,cbN,false);cbFix(m,model::lab_pa_horn,doors[i][1]+1.1f,13.98f,f+2.5f,cbS,false);
  if(i!=0&&i!=2)m.fixtures.push_back({model::med_mattress_stained,{a+1.05f,1.5f},.45f,1.9f,.9f,.118f,0,false});
  if(i!=3)m.fixtures.push_back({model::med_mattress_stained,{a+1.05f,22.5f},.45f,1.9f,.9f,.118f,0,false});}
 // 1 was emptied at 04:10: the mattress pulled off the slab, the chair they took him out in left where it stopped.
 cbFix(m,model::med_wheelchair,4.3f,7.2f,f,.4f,true);cbFix(m,model::med_crutches,1.12f,6.f,f,cbE,false);
 m.fixtures.push_back({model::med_mattress_stained,{2.5f,3.4f},0,1.9f,.9f,.118f,.5f,false});
 kit::prop(m,model::office_paper_sheet,3.6f,6.6f,f,1.1f);
 // 4: he dragged the mattress to the door behind him, and it did not help.
 m.fixtures.push_back({model::med_mattress_stained,{21.9f,15.7f},0,1.9f,.9f,.118f,1.2f,false});cbFix(m,model::scrap_bedroll,22.2f,19.6f,f,.2f,false);
 kit::prop(m,model::item_walkie,21.6f,18.6f,f,.114f,.376f,.046f,2.1f);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,22.3f,17.2f,f,.18f,.4f,.52f,2.6f);
  auto l=cbGlow({22.3f,17.2f},f+.45f,.4f,3.8f,2700);l.prop=lampProp;m.lights.push_back(l);}
 // the corridor: cold tubes, two gone, her lamps on the floor, and red only on door 2
 // every bunker keeps a cage tube of its own, so a look through any open door finds a room, not black
 for(auto [x,y,i,fl]:{std::tuple{4.f,5.f,.6f,.4f},{21.f,4.6f,.45f,0.f},{10.f,5.f,.4f,.2f},{4.f,18.3f,.55f,0.f},{16.f,18.3f,.5f,.3f},{21.f,18.3f,.45f,.15f}})
  m.lights.push_back(cbTube(x,y,roof,i,6.f,4600,.08f,fl));
 for(auto [x,i,fl]:{std::tuple{1.9f,.55f,0.f},{13.f,.5f,.3f},{18.8f,.55f,0.f},{22.f,.5f,.1f}})m.lights.push_back(cbTube(x,11.5f,roof,i,6.5f,5600,.05f,fl));
 cbDeadTube(m,7.f,11.5f,roof);m.lights.push_back(cbGlow({22.6f,11.5f},f+1.3f,.35f,3.f,8000,{.6f,.8f,1.f},.08f));
 for(auto [x,y,yaw]:{std::tuple{3.2f,12.9f,.3f},{8.2f,10.1f,2.4f},{12.6f,12.9f,-.5f},{16.f,10.1f,1.8f},{16.f,7.6f,.9f},{14.4f,3.2f,2.8f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f,.18f,.4f,.52f,yaw);
  auto l=cbGlow({x,y},f+.45f,.4f,3.8f,2700);l.prop=lampProp;m.lights.push_back(l);}
 terminal(m,{22.4f,11.5f},"ISOLATION WING / OCCUPANCY","1 EMPTY. 2 OCCUPANT DOES NOT ANSWER. 3 LAB 4 (ENERGY DIAGNOSTICS), TIER 3, IN 02:55. 4 SEE CLEANUP.",
  "03:52 DOOR 3 OPENED. NO CARD, NO OPERATOR. OCCUPANT LEFT UNASSISTED. MANUAL NOTE: THESE DOORS HAVE NO HANDLE INSIDE.",0);
 m.terminals.back().yaw=cbW;
 taped(m,{11.75f,13.49f},f+1.95f,cbN,Readable::Note,"ISOLATION / BY ORDER 04:10","OCCUPANTS RELOCATED TO THE HUB FOR TRANSFER. EXCEPT 2. 2 DOES NOT ANSWER.",
  "DO NOT OPEN 2 TO ASK.");
 // --- Isolation 3: hers ------------------------------------------------------------------------
 m.fixtures.push_back({model::med_mattress_stained,{14.15f,1.5f},.45f,1.9f,.9f,.118f,0,false});
 // Her hand, on the concrete, in the pen they left her: big where she wanted it found, on three
 // walls. What she wrote before the pad ran out is the pad's pages, taped up beside it.
 for(auto [x,y,yaw,w,title,sub]:{std::tuple{16.2f,1.012f,cbS,3.f,"I THINK THE HUM IS ME","THE LAMP HUMS WHEN I AM NOT LOOKING AT IT"},
   {18.888f,4.6f,cbW,3.4f,"FOLLOW THE LAMPS","I WILL KEEP LIGHTING THEM AS LONG AS I CAN - L4"},{13.112f,5.2f,cbE,3.4f,"IT OPENS IF I ASK IT","I CAN HEAR THE LOCK THINKING"}}){
  AuthoredSign s;s.position={x,y};s.z=f+1.2f;s.width=w;s.height=1.05f;s.yaw=yaw;s.title=title;s.subtitle=sub;s.style=3;s.accent=0xff1c2440u;s.ink=0xff1c2440u;m.signs.push_back(std::move(s));}
 taped(m,{14.2f,1.03f},f+1.1f,cbS,Readable::Note,"TORN FROM THE INTAKE PAD, IN PEN","THEY TOOK MY BADGE AT THE DOOR AND LEFT ME THE PEN. THE LAMP IN HERE HUMS WHEN I'M NOT LOOKING AT IT.",
  "I THINK THE HUM IS ME.");
 taped(m,{18.88f,7.4f},f+1.4f,cbW,Readable::Note,"TORN FROM THE INTAKE PAD, IN PEN","IF YOU'RE READING THIS YOU CAME DOWN. I SAID GO UP. I KNOW THERE IS NO UP.",
  "FOLLOW THE LAMPS. I'LL KEEP LIGHTING THEM AS LONG AS I CAN. - L4");
 taped(m,{13.12f,7.9f},f+1.4f,cbE,Readable::Note,"TORN FROM THE INTAKE PAD, IN PEN","03:50. MY HANDS ARE WARM ALL THE TIME. I CAN HEAR THE LOCK THINKING.",
  "IT OPENS IF I ASK IT. I'M GOING TO ASK IT.");
 terminal(m,{17.6f,8.6f},"ISOLATION 3 / INTERCOM","02:58 OCCUPANT: CAN SOMEONE TELL MY FRIEND IN SECURITY I'M OK. 03:20 OCCUPANT: I'M NOT OK.",
  "03:51 OCCUPANT: I'M SORRY ABOUT THE LIGHTS. 03:52 DOOR OPEN. CHANNEL CLOSED.",0);
 m.terminals.back().yaw=cbN;
 kit::prop(m,model::item_id_card,16.6f,4.f,f,.132f,.085f,.002f,1.2f);                            // somebody's; not hers
 // She kept it lit: a tube that still works and the lamps she had left, round the walls she wrote on.
 m.lights.push_back(cbTube(16.f,5.f,roof,.55f,6.f,3200,.02f,.12f));
 for(auto [x,y,yaw]:{std::tuple{15.8f,1.5f,.6f},{18.3f,2.4f,2.6f},{18.3f,6.f,-2.2f},{13.6f,7.6f,-.4f}}){
  const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,x,y,f,.18f,.4f,.52f,yaw);
  auto l=cbGlow({x,y},f+.45f,.45f,4.f,2700);l.prop=lampProp;m.lights.push_back(l);}
 // --- Isolation 2: he stands in the corner. He was a fitter. He keeps his habits. ----------------
 // He stands at the viewport now, against the glass, rocking; the one tube behind him makes him a shape.
 actor(m,clock,ActorVisual::Worker,1.7f,{{0,{8.65f,14.15f},f,cbN,0,0},{9000,{8.65f,14.15f},f,cbN,0,4}},true);
 m.lights.push_back(cbTube(9.6f,17.6f,roof,.5f,5.f,6500,.02f,.4f));
 // --- Isolation 4: the Cleanup man who shut himself in --------------------------------------------
 cbBody(m,clock,ActorVisual::CleanupSharpshooter,{21.f,18.f},f,2.2f);
 m.pickupSpawns.push_back({{20.2f,16.f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{2.6f,4.8f},PickupKind::Health,f});
 recessedFittings(m);
 zone(m,"ISOLATION WING / ISOLATION 3",13.1f,1.f,18.9f,9.3f,-1000,1000,{.16f,.13f,.10f,.008f});
 zone(m,"ISOLATION WING / CORRIDOR",0,9.5f,23.f,13.5f,-1000,1000,{.12f,.12f,.12f,.007f});
 zone(m,"ISOLATION WING",0,0,24,24,-1000,1000,{.12f,.12f,.12f,.008f});
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
 // The house reads as seating: a lit nosing on every tier, end standards at the aisle, a handrail
 // down it, and the house lamps on the side walls still on, so the rake is a shape from the stage.
 for(int i=0;i<5;++i){const float y0=4.f+.8f*i,top=up-.5f*(i+1);
  for(auto [x1,x2]:{std::pair{4.1f,11.2f},{12.8f,23.f}})floorLine(m,x1,y0+.72f,x2,y0+.8f,top,14);
  wall(m,12.8f,y0+.1f,12.95f,y0+.7f,top,top+.85f,10);}
 for(float x:{11.3f,12.7f})m.pipes.push_back({{x,4.1f},{x,7.9f},up+.9f,.025f,fl+.9f+.5f,2});
 for(auto [y,z]:{std::pair{4.6f,up+.2f},{6.6f,up-.8f}}){m.lights.push_back(lamp({23.f,y},z+1.6f,.6f,7.f,2800,0,WorldLightMount::Wall,blW));
  m.fixtures.push_back({model::mine_cage_lamp,{22.9f,y},z+1.45f-m.layers.front().elevation,.21f,.2f,.36f,0,false});}
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
 // What the glass left in the frames: teeth hanging from the head and standing from the sill.
 {unsigned h=1011u;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return (h&0xffffu)/65535.f;};
  for(auto [x1,x2]:{std::pair{8.95f,11.15f},{12.85f,15.15f},{20.05f,22.95f}})for(float x=x1;x<x2-.08f;x+=.12f+next()*.14f){
   const float w=.05f+next()*.09f,top=.15f+next()*(next()<.3f?1.4f:.55f),low=next()<.6f?.05f+next()*.45f:0.f;
   wall(m,x,vg+.05f,x+w,vg+.07f,gh-top,gh,17);m.structures.back().solid=false;
   if(low>0){wall(m,x,vg+.05f,x+w*.8f,vg+.07f,fl+1.f,fl+1.f+low,17);m.structures.back().solid=false;}}}
 for(int i=0;i<3;++i){const float y0=4.f+.8f*(i+1),top=up-.5f*(i+2);paperDrift(m,4.4f,y0+.05f,10.9f,y0+.25f,top,3,1010+i);paperDrift(m,13.1f,y0+.05f,22.6f,y0+.25f,top,3,1020+i);}
 // --- The stage: raised the width of the room's middle, the cylinder at its centre, lit from the
 // rig: a truss across the volume with its cans on the subject, warm against the cylinder's violet.
 wall(m,7.5f,13.2f,16.5f,19.f,fl,fl+.3f,15);floorLine(m,7.5f,13.2f,16.5f,13.32f,fl+.3f,13);
 for(float x:{8.6f,10.4f,13.6f,15.4f})m.lights.push_back(cbGlow({x,13.05f},fl+.38f,.3f,2.6f,2600));     // the footlights
 wall(m,7.f,11.9f,17.f,12.1f,fl+6.4f,fl+6.7f,12);wall(m,7.f,12.5f,17.f,12.7f,fl+6.4f,fl+6.7f,12);
 for(float x:{7.2f,16.8f})for(float y:{12.f,12.6f})m.pipes.push_back({{x,y},{x,y},fl+6.7f,.03f,roof,6});
 for(float x:{11.f,13.f,14.8f}){wall(m,x-.16f,12.14f,x+.16f,12.46f,fl+5.85f,fl+6.4f,8);
  m.lights.push_back(cbGlow({x,12.6f},fl+5.8f,1.45f,11.f,3300));}
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
 wall(m,6.f,10.45f,7.1f,10.95f,fl,fl+.5f,10);wall(m,7.1f,10.38f,7.18f,11.02f,fl,fl+.62f,10);          // the lectern, knocked flat
 kit::prop(m,model::office_paper_stack,7.5f,10.4f,fl,.33f,.44f,.03f,.7f);
 kit::propPose(m,model::office_chair,8.7f,12.3f,fl,.4f,0.f,1.45f);                                    // a chair from the front row, thrown
 // One of the rig's cans came down at 02:40 and still burns, on its side on the stage, raking the boards.
 wall(m,8.4f,15.4f,8.9f,15.75f,fl+.3f,fl+.62f,8);wall(m,8.9f,15.45f,8.93f,15.7f,fl+.34f,fl+.58f,20);
 m.lights.push_back(lamp({8.9f,15.57f},fl+.48f,.9f,6.f,3000,0,WorldLightMount::Wall,0,true));
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
 zone(m,"OBSERVATION THEATER / STAGE",4.f,8.f,23.f,23.f,-1000,fl+3.f,{.13f,.12f,.13f,.008f});
 zone(m,"OBSERVATION THEATER",0,0,24,24,-1000,1000,{.12f,.11f,.12f,.008f});
 freightFinish(m,-1);
 return p;
}

// BREACH SECTOR (manifest 69). Ground zero: the primary containment hall. At 02:00 the middle of its
// floor went down three metres into the rock under it and the rock came back up through the hole:
// bedrock heaved against the walls, black stone from further down standing in it like teeth, the
// growth over all of it, and at the centre the spire, lit from inside. What is left of the floor is
// two ledges, east and south, with the slab's broken edge and its steel hanging off them; the plant
// that stood on the rest is in the hole. The Cleanup Force held the east ledge until 03:05 and left
// a note on how they lost it. The tall one stands under that ledge with its back to the door.
// The beat: it does not move while the Force's floodlight is on it. Step off the slab onto the
// crater floor and the flood blows [conb_br_dark]; it wakes, and the crossing is made in the spire's
// pulse, between the knots, looking at it.
// Route: E seam from B onto the east ledge (-15) -> down the slid slab into the crater (-18) ->
// across between the knots -> up the scree to the south ledge -> S seam to Emergency Containment.
std::shared_ptr<const Map> makeBreachSector(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[102];m.definition=CampaignChunks[102];m.definition.ambient=.15f;
 m.definition.playerStart={22.f,11.5f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceRock;m.ceilingSurface=SurfaceConcrete;
 const float fl=-18,up=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),fl));
 openEdge(m,{'E',9.f,14.f,up,false,0});
 openEdge(m,{'S',10.f,14.f,up,false,0});
 machineryClock(m,"conb_br_clock",60000);const char* clock="conb_br_clock";
 const StateId dark=stateId("conb_br_dark");
 auto rock=[&](const char* kind,float x,float y,float z,float w,float d,float h,int facing,bool solid=true){cbPrim(m,kind,x,y,z,w,d,h,facing,26,solid);};
 // A stone of the black rock that came up: the pack model, and a core to walk into.
 auto stone=[&](int mdl,float x,float y,float z,float s,float yaw,bool block=true){const auto& v=model::size[mdl];cbModel(m,mdl,x,y,z,v[0]*s,v[1]*s,v[2]*s,yaw);
  if(block){const float r=std::min(v[0],v[1])*s*.34f;wall(m,x-r,y-r,x+r,y+r,z,z+v[2]*s*.8f,CollisionProxyMaterial);}};
 // Plant that went down with the floor: the model sunk into the crater, a core to walk into.
 auto sunk=[&](int mdl,float x,float y,float sink,float yaw,float hx,float hy){const auto& v=model::size[mdl];cbModel(m,mdl,x,y,fl-sink,v[0],v[1],v[2],yaw);
  wall(m,x-hx,y-hy,x+hx,y+hy,fl,fl-sink+v[2],CollisionProxyMaterial);};
 // Crust on a floor: a patch a finger thick with a stepped edge, walked over.
 auto crust=[&](float x,float y,float w,float d,float z){const unsigned s=cbSeed(x,y);const float ox=(float(s&15u)/15.f-.5f)*w*.14f,oy=(float((s>>4)&15u)/15.f-.5f)*d*.14f;
  wall(m,x-w*.5f,y-d*.3f+oy,x+w*.5f,y+d*.3f+oy,z,z+.02f,34);wall(m,x-w*.3f+ox,y-d*.5f,x+w*.3f+ox,y+d*.5f,z,z+.026f,34);wall(m,x-w*.41f-ox,y-d*.41f-oy,x+w*.41f-ox,y+d*.41f-oy,z,z+.032f,34);};
 // The growth up a wall face ('N': the face at y 1, along x; 'W': the face at x 1, along y): a sheet
 // of crust from `lo` over the floor, risen in round-tipped fingers to the heights given along it
 // ({where, how high}), some of them running on up the concrete in a tendril.
 auto creep=[&](char side,std::initializer_list<Vec2> env,float lo,unsigned seed){
  unsigned h=seed*2654435761u+7;auto next=[&h]{h^=h<<13;h^=h>>17;h^=h<<5;return float(h&0xffffu)/65535.f;};
  const Vec2* e=env.begin();const size_t n=env.size();
  auto top=[&](float t){for(size_t i=1;i<n;++i)if(t<=e[i].x){const float k=(t-e[i-1].x)/(e[i].x-e[i-1].x);return e[i-1].y+(e[i].y-e[i-1].y)*k;}return e[n-1].y;};
  for(float a=e[0].x;a<e[n-1].x-.25f;){
   const float w=std::min(.4f+next()*1.f,e[n-1].x-a),c=a+w*.5f,hi=fl+top(c)*(.66f+next()*.5f),cap=w*(.45f+next()*.5f),t=.05f+next()*.07f;
   const bool tendril=next()<.34f;const float r=.07f+next()*.1f,run=.8f+next()*3.6f;
   if(hi-cap>fl+lo+.1f){
    if(side=='N'){wall(m,a,1.f,a+w,1.f+t,fl+lo,hi-cap,34);cbPrim(m,"dome",c,1.f,hi-cap,w,t*2.f+.32f,cap,0,34,false);}
    else{wall(m,1.f,a,1.f+t,a+w,fl+lo,hi-cap,34);cbPrim(m,"dome",1.f,c,hi-cap,t*2.f+.32f,w,cap,0,34,false);}
    m.structures[m.structures.size()-2].solid=false;
    if(tendril&&hi+.4f<roof){const float z2=std::min(hi+run,roof);if(side=='N')wall(m,c-r,1.f,c+r,1.035f,hi-.12f,z2,34);else wall(m,1.f,c-r,1.035f,c+r,hi-.12f,z2,34);m.structures.back().solid=false;}}
   a+=w;}
 };
 // --- What is left of the sector floor: the east ledge, the south ledge, the lift core between ---
 wall(m,18.f,1.f,24.f,17.f,fl,up,15);wall(m,1.f,19.f,18.f,24.f,fl,up,15);
 wall(m,18.f,17.f,23.f,23.f,fl,roof,15);
 // --- The crater's sides: bedrock heaved back against the walls and under the ledges, never one
 // straight face; the slab's broken edge shows over it, east and south. (a..b along the side, how
 // far it stands out, how high.)
 for(auto [a,b,d,h]:{std::tuple{1.f,4.2f,2.2f,3.6f},{4.2f,7.f,1.5f,3.f},{7.f,10.5f,2.f,3.3f},{10.5f,13.2f,1.3f,2.9f},{13.2f,16.f,1.9f,3.4f},{16.f,18.f,1.4f,3.1f}})rock("batter",(a+b)*.5f,1.f+d*.5f,fl,b-a,d,h,0);
 for(auto [a,b,d,h]:{std::tuple{3.f,5.5f,1.8f,3.4f},{5.5f,8.4f,1.3f,3.f},{8.4f,11.6f,2.1f,3.7f},{11.6f,14.6f,1.4f,3.1f},{14.6f,19.f,1.9f,3.3f}})rock("batter",1.f+d*.5f,(a+b)*.5f,fl,b-a,d,h,1);
 for(auto [a,b,d,h]:{std::tuple{1.f,4.f,1.6f,2.6f},{4.f,7.f,1.1f,2.4f},{7.f,9.7f,1.5f,2.7f},{11.9f,14.4f,1.2f,2.5f},{14.4f,17.f,1.7f,2.7f},{17.f,19.f,1.3f,3.f}})rock("batter",18.f-d*.5f,(a+b)*.5f,fl,b-a,d,h,3);
 for(auto [a,b,d,h]:{std::tuple{1.f,4.9f,1.5f,2.7f},{6.7f,9.5f,1.3f,2.5f},{9.5f,12.6f,1.8f,2.7f},{12.6f,15.4f,1.2f,2.4f},{15.4f,18.f,1.6f,2.7f}})rock("batter",(a+b)*.5f,19.f-d*.5f,fl,b-a,d,h,2);
 for(auto [x,y,w,d,h]:{std::tuple{3.4f,3.4f,3.4f,2.8f,2.2f},{16.4f,3.f,3.f,2.4f,1.7f},{3.f,17.f,2.6f,2.4f,1.6f},{16.6f,16.4f,2.2f,2.f,1.5f},{12.2f,2.9f,2.2f,1.8f,1.1f},
  {2.9f,9.9f,2.4f,3.f,1.f},{8.4f,17.9f,2.6f,1.9f,.9f},{3.9f,15.3f,2.2f,1.9f,.8f}})rock("dome",x,y,fl,w,d,h,0);
 // What the floor became: slabs of it, tipped in against the sides where they slid.
 for(auto [x,y,w,d,h,fc]:{std::tuple{16.3f,7.9f,2.6f,2.9f,2.2f,1},{16.2f,13.4f,2.2f,2.5f,1.7f,1},{14.9f,17.5f,2.2f,2.5f,1.9f,0},{10.f,17.8f,1.8f,2.1f,1.4f,0},{3.1f,7.3f,2.4f,2.6f,1.6f,3}})
  cbPrim(m,"ramp",x,y,fl-.12f,w,d,h,fc,42);
 // the slab edge: lips hanging off the ledges, the bar out of the concrete
 for(auto [x,y,w,d,h,fc]:{std::tuple{17.3f,5.7f,1.7f,1.4f,1.1f,1},{17.4f,15.4f,2.f,1.2f,.9f,1},{8.6f,18.4f,1.8f,1.2f,1.f,0},{16.2f,18.5f,1.9f,1.f,.8f,0}})cbPrim(m,"ramp",x,y,up-h,w,d,h,fc,42,false);
 for(auto [y,l,drop]:{std::tuple{2.6f,.7f,.3f},{3.1f,.5f,.15f},{8.3f,.5f,.1f},{8.7f,.8f,.4f},{12.6f,.9f,.5f},{13.f,.6f,.2f},{16.1f,.6f,.2f}})cbPrim(m,"strut",18.f-l*.5f,y,up-.14f-drop,.03f,l,drop+.03f,1,8,false);
 for(auto [x,l,drop]:{std::tuple{7.6f,.6f,.3f},{8.f,.8f,.45f},{11.3f,.8f,.5f},{13.8f,.5f,.1f},{14.2f,.7f,.3f},{17.1f,.7f,.4f}})cbPrim(m,"strut",x,19.f-l*.5f,up-.14f-drop,.03f,l,drop+.03f,0,8,false);
 // --- The way down: the slab that slid, one end still on the ledge; its rail on one side only -----
 cbPrim(m,"ramp",15.2f,10.8f,fl,1.6f,5.6f,3.f,1,42);
 cbPrim(m,"strut",15.2f,10.03f,fl+.92f,.05f,5.6f,3.05f,1,12,false);
 for(float x:{13.f,14.8f,16.6f}){const float z=fl+(x-12.4f)/5.6f*3.f;wall(m,x-.02f,10.01f,x+.02f,10.05f,z-.1f,z+.96f,12);m.structures.back().solid=false;}
 floorLine(m,18.05f,10.f,18.2f,11.6f,up,13);
 // --- The way out: the scree up to the south ledge, the Force's hand line along it ----------------
 rock("ramp",5.8f,16.6f,fl,1.6f,4.8f,3.f,0);
 rock("ramp",4.3f,17.3f,fl,1.4f,3.4f,1.9f,0);rock("ramp",7.3f,17.6f,fl,1.4f,2.8f,1.5f,0);rock("dome",5.9f,14.3f,fl,3.6f,1.6f,.45f,0,false);
 m.pipes.push_back({{6.64f,14.3f},{6.64f,19.1f},fl+1.f,.018f,up+1.f,3});
 for(float y:{14.3f,16.7f,19.1f}){const float z=fl+std::clamp((y-14.2f)/4.8f,0.f,1.f)*3.f;m.pipes.push_back({{6.64f,y},{6.64f,y},z,.022f,z+1.f,2});}
 // --- At the bottom, the source: the heave, the black rock standing out of it, the spire ----------
 const float sx=8.6f,sy=7.2f;
 rock("dome",sx,sy,fl,5.4f,4.6f,1.5f,0);
 for(auto [dx,dy,w,d,h]:{std::tuple{-.9f,.5f,2.4f,2.2f,1.9f},{1.f,.2f,2.6f,2.f,2.3f},{.1f,-1.f,2.2f,1.8f,1.7f},{.2f,1.3f,1.8f,1.4f,1.5f}})cbPrim(m,"dome",sx+dx,sy+dy,fl,w,d,h,0,34,false);
 for(auto [dx,dy,sc,yaw]:{std::tuple{1.9f,-1.3f,.8f,1.9f},{-.6f,-2.f,.62f,5.1f}})
  cbFang(m,sx+dx,sy+dy,fl+.25f,sc,yaw);
 for(auto [dx,dy,w,h]:{std::tuple{0.f,0.f,.86f,8.6f},{-.7f,-.5f,.5f,5.f},{.7f,.6f,.58f,6.f},{.8f,-.6f,.4f,3.9f},{-.8f,.6f,.44f,3.5f},{.3f,1.1f,.36f,2.9f},{-.35f,.15f,.34f,6.9f},{.4f,-.3f,.3f,7.5f}})
  cbShard(m,sx+dx,sy+dy,fl+1.f,w,h-1.f);
 for(auto [dx,dy,w,h,lean,reach]:{std::tuple{.5f,.1f,.42f,4.4f,1,2.4f},{-.5f,-.1f,.36f,3.6f,3,2.f},{0.f,.5f,.4f,3.9f,0,2.2f},{.1f,-.5f,.32f,3.f,2,1.7f},{.9f,.9f,.26f,2.4f,1,1.5f},{-.9f,.4f,.28f,2.6f,0,1.4f},{-.3f,-.9f,.24f,2.f,3,1.3f}})
  cbShard(m,sx+dx,sy+dy,fl+1.1f,w,h,lean,reach);
 for(auto [dx,dy,sc,z,yaw]:{std::tuple{-1.5f,.9f,1.6f,1.f,.4f},{1.6f,.2f,1.4f,1.1f,1.3f},{.9f,-1.5f,1.3f,1.f,2.2f},{-1.2f,-1.3f,1.5f,.9f,2.9f},{.1f,1.8f,1.2f,.9f,3.6f},{2.3f,1.3f,1.1f,.2f,.9f},{-2.4f,-.2f,1.2f,.25f,2.f}})
  cbBed(m,sx+dx,sy+dy,fl+z,sc,yaw);
 wall(m,sx-1.f,sy-1.f,sx+1.f,sy+1.f,fl,fl+8.f,CollisionProxyMaterial);wall(m,9.6f,7.05f,11.5f,7.55f,fl,fl+3.f,CollisionProxyMaterial);
 stone(model::cave_stalagmite_cluster,11.7f,5.6f,fl,.85f,.6f,false);stone(model::cave_stalagmite_cluster,5.9f,9.f,fl,.75f,2.3f,false);
 m.lights.push_back(cbGlow({sx,sy+1.7f},fl+2.4f,2.5f,18.f,6500,{.72f,.5f,1.f},.12f));
 m.lights.push_back(cbGlow({sx,sy-.8f},fl+9.f,1.3f,12.f,6500,{.6f,.45f,1.f},.08f));
 m.lights.push_back(cbGlow({sx+2.7f,sy-.9f},fl+1.f,.9f,7.f,7000,{.45f,.6f,1.f},.1f));                // its base, cold
 {auto l=cbGlow({sx-.4f,sy+1.9f},fl+3.4f,3.4f,17.f,6500,{.8f,.6f,1.f});l.enabledState=dark;l.period=3.2f;l.onTime=.45f;m.lights.push_back(l);}   // its pulse, once the flood is gone
 hazard(m,Hazard::Kind::Anomaly,6.2f,4.8f,11.f,9.6f,fl,fl+3.f,0);hazard(m,Hazard::Kind::Anomaly,7.f,5.6f,10.2f,8.8f,fl,fl+3.f,14);
 // --- The crater floor: rock, and the crust over it in blisters; mounds, knots, the black stones ---
 for(auto [x,y,w,d]:{std::tuple{8.6f,7.4f,9.5f,8.f},{5.f,11.6f,5.4f,4.4f},{10.4f,13.4f,5.6f,3.8f},{13.2f,4.8f,5.f,3.4f},{4.4f,15.8f,3.6f,3.6f},{13.8f,15.8f,3.6f,3.2f},{12.9f,9.4f,2.8f,2.4f},{2.6f,4.4f,2.4f,3.f},{15.6f,10.6f,1.8f,2.6f}})crust(x,y,w,d,fl);
 for(auto [x,y,r,h]:{std::tuple{5.6f,7.7f,1.1f,1.f},{10.9f,3.9f,.9f,.7f},{3.7f,13.5f,1.f,1.2f},{12.9f,17.2f,.9f,.7f},{10.f,16.4f,.7f,.5f}})cbMound(m,x,y,fl,r,h);
 stone(model::cave_boulder_a,13.f,7.9f,fl-.15f,.55f,1.1f);stone(model::cave_boulder_round,14.4f,3.6f,fl-.2f,.6f,.4f);
 stone(model::cave_rock_block,8.2f,15.3f,fl-.2f,.5f,.8f);stone(model::cave_boulder_round,2.3f,12.6f,fl-.3f,.7f,1.4f);
 stone(model::cave_rock_block,5.4f,2.3f,fl+.3f,1.1f,.2f,false);
 // the growth going up the walls it reached: blisters on the north and west faces
 creep('N',{{1.f,7.6f},{3.f,6.f},{5.f,5.2f},{7.5f,7.6f},{9.6f,10.8f},{11.5f,7.6f},{13.f,5.f},{14.6f,3.6f}},2.2f,11);
 creep('W',{{1.f,7.6f},{4.f,5.6f},{7.f,5.4f},{9.f,8.4f},{10.6f,11.2f},{12.5f,7.2f},{14.6f,5.f},{16.4f,3.6f}},2.2f,23);
 // where it is thickest it stands off the wall in swellings a metre deep, seated on the heaved rock
 for(auto [x,z,w,d,h]:{std::tuple{9.4f,2.7f,3.2f,2.2f,2.6f},{7.3f,2.6f,2.2f,1.6f,1.7f},{11.5f,2.5f,2.f,1.5f,1.5f},{2.2f,2.9f,2.4f,2.f,2.2f}})cbPrim(m,"dome",x,std::max(1.f,d*.5f),fl+z,w,d,h,0,34,false);
 for(auto [y,z,w,d,h]:{std::tuple{10.4f,3.1f,3.4f,2.4f,2.8f},{8.3f,2.8f,2.f,1.6f,1.6f},{12.6f,2.7f,2.2f,1.6f,1.8f}})cbPrim(m,"dome",std::max(1.f,d*.5f),y,fl+z,d,w,h,0,34,false);
 // and over the rock under it: the heave behind the spire is grown over, top to foot
 for(auto [a,b,d,h]:{std::tuple{7.f,10.5f,2.f,3.3f},{10.5f,13.2f,1.3f,2.9f}})cbPrim(m,"batter",(a+b)*.5f,1.f+d*.5f+.04f,fl,b-a-.16f,d+.08f,h+.05f,0,34,false);
 cbPrim(m,"batter",1.f+2.1f*.5f+.04f,10.f,fl,3.2f-.16f,2.1f+.08f,3.75f,1,34,false);
 for(auto [x,lo,w]:{std::tuple{9.f,8.2f,.34f},{10.3f,8.6f,.5f},{8.4f,6.f,.26f}}){wall(m,x-w*.5f,1.f,x+w*.5f,1.05f,fl+lo,roof,34);m.structures.back().solid=false;}
 for(auto [y,lo,w]:{std::tuple{10.f,9.2f,.5f},{11.2f,9.f,.3f},{9.2f,7.f,.28f}}){wall(m,1.f,y-w*.5f,1.05f,y+w*.5f,fl+lo,roof,34);m.structures.back().solid=false;}
 const Vec2 knots[4]={{10.6f,13.9f},{8.2f,11.4f},{11.5f,8.f},{3.3f,15.9f}};
 for(int i=0;i<4;++i){const Vec2 k=knots[i];cbKnot(m,k.x,k.y,fl,1.2f+.3f*(i%2));
  m.lights.push_back(cbGlow({k.x+.3f,k.y+.3f},fl+1.5f,.6f,5.5f,6500,i%2?Point3{.3f,.6f,1.f}:Point3{.72f,.5f,1.f},.25f));
  hazard(m,Hazard::Kind::Anomaly,k.x-1.4f,k.y-1.4f,k.x+1.4f,k.y+1.4f,fl,fl+2.4f,0);                // the shimmer first
  hazard(m,Hazard::Kind::Anomaly,k.x-.7f,k.y-.7f,k.x+.7f,k.y+.7f,fl,fl+2.4f,10);}                 // then it bites
 cbHum(m,"conb_br_hum",1.f,1.f,18.f,19.f,fl,fl+4.f,.7f);
 // --- The plant that stood on the floor: in the hole, sunk to its waist ---------------------------
 sunk(model::lab_mainframe,4.5f,5.1f,1.2f,.45f,1.f,1.f);
 sunk(model::pwr_switchgear_cabinet,6.3f,3.7f,.9f,-.3f,.7f,.45f);
 sunk(model::pwr_turbine_housing,15.3f,5.4f,.7f,.6f,1.1f,1.1f);
 sunk(model::med_holding_cage,4.8f,11.f,.5f,.3f,1.9f,.95f);                                           // a holding cage, its bars sprung
 sunk(model::lab_cell_gate,12.1f,16.9f,.9f,.5f,1.4f,.25f);
 sunk(model::pwr_catwalk_section,12.6f,4.5f,.3f,.25f,1.6f,.8f);sunk(model::pwr_catwalk_section,14.7f,7.7f,.45f,1.9f,.8f,1.6f);   // the north gallery, where it landed
 cbModel(m,model::pwr_coolant_main,14.f,3.5f,fl+.05f,.5f,7.26f,.5f,kPi*.5f);
 // The north gallery ran along the wall four metres over the floor. Its brackets are still in the
 // concrete; the corner bay still hangs, the rest is in the hole under the cable tray that came down burning.
 for(float x:{3.f,6.f,9.f,12.f})cbPrim(m,"girder",x,1.6f,-11.95f,.3f,1.2f,.35f,0,12,false);
 wall(m,14.2f,1.f,23.f,2.3f,-11.6f,-11.5f,46);wall(m,21.7f,2.3f,23.f,8.4f,-11.6f,-11.5f,46);
 guardRail(m,14.2f,2.22f,21.7f,2.3f,-11.5f);guardRail(m,21.7f,2.3f,21.78f,8.4f,-11.5f);
 for(float x:{15.f,18.f,21.f})cbPrim(m,"girder",x,1.6f,-11.95f,.3f,1.2f,.35f,0,12,false);
 for(float y:{4.f,7.f})cbPrim(m,"girder",22.4f,y,-11.95f,.3f,1.2f,.35f,3,12,false);
 cbPrim(m,"strut",13.4f,1.65f,-13.7f,.14f,1.6f,2.2f,1,12,false);cbPrim(m,"strut",22.3f,9.1f,-12.7f,.14f,1.4f,1.2f,2,12,false);   // its torn ends hanging
 cbModel(m,model::pwr_cable_bundle,8.9f,3.3f,fl+.03f,4.f,.14f,.22f,.05f);cbModel(m,model::pwr_cable_bundle,9.5f,2.7f,fl+.55f,4.f,.14f,.22f,-.08f);
 hazard(m,Hazard::Kind::Fire,7.2f,3.f,10.6f,3.6f,fl,fl+1.4f,10);
 m.lights.push_back(cbGlow({8.9f,3.9f},fl+.9f,.8f,7.f,1800,{1,1,1},.3f));
 // one high-bay still burns on its drop over the plant; two hang dead
 cbPendant(m,{5.6f,6.4f},roof,fl+3.f,2.4f,9.f,1900,.12f,-.2f);cbPendant(m,{14.f,6.2f},roof,fl+3.6f,0,0,0);cbPendant(m,{10.6f,15.6f},roof,fl+4.2f,0,0,0);
 m.lights.push_back(cbGlow({10.f,10.5f},-9.5f,4.6f,19.f,7500));                                       // the hole in the roof the light comes down
 // round it the roof slab is down and the rock over it has sagged through
 for(auto [x,y,sc,yaw]:{std::tuple{8.2f,9.3f,1.2f,.4f},{11.9f,10.f,1.f,2.1f},{9.6f,12.6f,1.1f,3.6f}}){const auto& v=model::size[model::cave_ceiling_rock];cbModel(m,model::cave_ceiling_rock,x,y,roof-v[2]*sc*.8f,v[0]*sc,v[1]*sc,v[2]*sc,yaw);}
 for(auto [x,y,sc,yaw]:{std::tuple{10.6f,8.4f,.7f,1.f},{7.6f,11.6f,.55f,2.6f}}){const auto& v=model::size[model::cave_stalactite];cbModel(m,model::cave_stalactite,x,y,roof-v[2]*sc*.88f,v[0]*sc,v[1]*sc,v[2]*sc,yaw);}
 m.lights.push_back(cbGlow({4.4f,10.6f},fl+1.75f,.45f,4.f,6500,{.72f,.5f,1.f},.3f));                  // in the cage
 // --- The tall one. It stands under the east ledge, its back to you, where the flood throws its
 // shadow across the floor; it does not move until the flood is gone.
 m.creatureSpawns.push_back({CreatureKind::Warden,{14.2f,15.f},fl,1,0,dark});
 {ScriptAction shake;shake.type=ScriptAction::Type::Shake;shake.amount=.35f;
  ScriptAction sting;sting.type=ScriptAction::Type::PlaySound;sting.sound=Sound::Sting;sting.amount=.7f;
  ScriptEvent e;e.id=stateId("conb_br_descent");e.x1=1.f;e.y1=1.f;e.x2=18.f;e.y2=19.f;e.bottom=-100;e.top=fl+1.2f;       // however she comes down
  e.actions={cbSet("conb_br_dark",1),shake,sting};m.scriptEvents.push_back(std::move(e));}
 m.ambients.push_back({Sound::OneMetalBang,{18.7f,13.2f},up+2.4f,.6f,22,AmbientEmitter::OnState,8,8,dark,1});
 m.ambients.push_back({Sound::OneMonsterGroan,{14.2f,15.f},fl+1.6f,.6f,22,AmbientEmitter::OnState,8,8,dark,1});
 cbBody(m,clock,ActorVisual::Cleanup,{13.3f,12.5f},fl,1.2f);cbBody(m,clock,ActorVisual::CleanupHeavy,{6.4f,12.5f},fl,2.9f);
 cbBody(m,clock,ActorVisual::Scientist,{12.9f,15.3f},fl,.2f);
 m.pickupSpawns.push_back({{7.f,9.9f},PickupKind::Health,fl});
 // --- The east ledge: where the Force held, and what it left ---------------------------------------
 cbPortalY(m,22.6f,23.f,9.f,14.f,up,roof,3.2f);
 for(float y:{3.f,6.f,16.f})wall(m,22.78f,y-.25f,23.f,y+.25f,up,roof,15);
 guardRail(m,17.92f,1.2f,18.f,4.9f,up);guardRail(m,17.92f,6.6f,18.f,10.f,up);guardRail(m,17.92f,11.6f,18.f,12.8f,up);
 // the floodlight on its stand at the edge, thrown down across the crater
 for(float y:{13.f,13.3f})wall(m,19.f,y,19.08f,y+.08f,up,up+2.3f,12);wall(m,18.72f,12.9f,19.04f,13.5f,up,up+.06f,8);
 wall(m,18.7f,12.95f,19.05f,13.45f,up+2.3f,up+2.7f,8);wall(m,18.66f,13.f,18.7f,13.4f,up+2.34f,up+2.66f,20);
 {auto l=lamp({18.75f,13.2f},up+2.5f,1.7f,20.f,4600,0,WorldLightMount::Wall,blW,true);l.enabledState=dark;l.invert=true;m.lights.push_back(l);}
 looseLead(m,{19.12f,13.2f,up+2.4f},{20.6f,14.6f,up},3.8f,7,false,.012f);
 cbFix(m,model::mine_portable_generator,20.9f,14.9f,up,.4f,true);
 crewBarrier(m,20.3f,12.7f,up,1.4f);
 taped(m,{20.3f,12.7f},up+.75f,cbN,Readable::Note,"SECTOR LOST / 03:05","THE TALL ONE DOES NOT MOVE WHILE YOU LOOK AT IT. SO KEEP LOOKING AT IT.",
  "IT COMES WHEN YOU RELOAD. IT COMES AT THE LIGHT. DON'T DO EITHER. - REYES");
 // the north end: four hours of holding a rail
 for(auto [x,y,yaw]:{std::tuple{19.1f,2.f,.1f},{20.3f,1.9f,-.15f},{18.9f,5.6f,1.5f}})cbFix(m,model::lab_specimen_crate,x,y,up,yaw,true);
 cbFix(m,model::scrap_bedroll,22.1f,3.6f,up,.1f,false);cbFix(m,model::scrap_bedroll,22.f,6.3f,up,-.2f,false);
 kit::prop(m,model::item_walkie,20.3f,1.9f,up+1.12f,.114f,.376f,.046f,.6f);kit::prop(m,model::mine_hard_hat,21.2f,5.f,up,1.1f);
 {const int lampProp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,19.1f,2.f,up+1.12f,.18f,.4f,.52f,2.4f);
  auto l=cbGlow({19.1f,2.2f},up+1.5f,.8f,7.f,3000);l.prop=lampProp;m.lights.push_back(l);}
 cbBody(m,clock,ActorVisual::CleanupSharpshooter,{21.2f,2.9f},up,2.2f);
 cbFix(m,model::med_first_aid_box,22.94f,7.6f,up+1.2f,cbW,false);
 m.pickupSpawns.push_back({{21.6f,15.6f},PickupKind::Ammo,up});m.pickupSpawns.push_back({{19.6f,3.4f},PickupKind::HandgunAmmo,up});
 crust(19.3f,7.6f,2.2f,3.f,up);
 // the lift core: its shutter came down at 02:00 and has not moved
 trackPortal(m,20.5f,16.8f,up,1.f);
 cbFix(m,model::lab_status_display,20.5f,16.96f,up+5.05f,cbN,false);
 // --- The south ledge: the top of the scree, the way on --------------------------------------------
 wall(m,9.4f,22.4f,10.f,23.f,up,roof,15);wall(m,14.f,22.4f,14.6f,23.f,up,roof,15);wall(m,10.f,22.4f,14.f,23.f,up+3.2f,roof,15);
 floorLine(m,10.f,22.1f,14.f,22.3f,up,13);
 cbFix(m,model::urb_jersey_barrier,3.76f,19.7f,up,0,true);cbFix(m,model::urb_jersey_broken,7.84f,19.7f,up,0,true);
 guardRail(m,9.4f,19.f,12.2f,19.08f,up);guardRail(m,14.6f,19.f,18.f,19.08f,up);
 cbMound(m,2.3f,21.6f,up,1.f,.9f);crust(3.2f,21.2f,3.8f,3.f,up);
 cbFix(m,model::pwr_generator_unit,16.2f,21.6f,up,.2f,true);cbFix(m,model::pwr_step_transformer,15.f,22.3f,up,0,true);
 cbFix(m,model::lab_status_display,18.f-.04f,20.1f,up+2.f,cbW,false);
 m.pickupSpawns.push_back({{2.8f,20.2f},PickupKind::Health,up});
 {ScriptAction save;save.type=ScriptAction::Type::Checkpoint;cbEvent(m,"conb_br_out",9.5f,20.6f,14.5f,23.6f,"conb_br_dark",{save});}
 // --- Light: the Force's work lamps mark the line; red on the walls; the way on is the one warm door
 for(auto [x,y,z]:{std::tuple{18.5f,12.2f,up},{12.f,12.f,fl},{9.3f,12.9f,fl},{6.95f,13.9f,fl},{7.1f,20.6f,up}}){
  m.fixtures.push_back({model::mine_cage_lamp,{x,y},z-m.layers.front().elevation,.21f,.2f,.36f,0,false});
  m.lights.push_back(cbGlow({x,y},z+.3f,.65f,6.f,3000));}
 for(auto [x,y,yaw,ph]:{std::tuple{23.f,4.6f,blW,0.f},{23.f,15.6f,blW,.9f},{16.6f,23.f,blN,.4f},{18.f,21.5f,blW,1.3f}})m.lights.push_back(cbAlarm({x,y},up+2.4f,yaw,ph));
 m.lights.push_back(lamp({12.f,22.4f},up+3.5f,.8f,7.f,3200,0,WorldLightMount::Wall,blN));
 zone(m,"BREACH SECTOR / CRATER",1.f,1.f,18.f,19.f,-1000,up-.5f,{.11f,.10f,.12f,.006f});
 zone(m,"BREACH SECTOR",0,0,24,24,-1000,1000,{.11f,.10f,.11f,.006f});
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
 m.name=CampaignMapNames[103];m.definition=CampaignChunks[103];m.definition.ambient=.27f;
 m.definition.playerStart={12.f,1.6f};m.definition.spawnHeight=-15.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'S',10.f,14.f,f,true,stateId("cont_barricade")});                                  // door 0: the barricade
 openEdge(m,{'N',10.f,14.f,f,false,0});
 machineryClock(m,"conb_ec_clock",60000);const char* clock="conb_ec_clock";
 const StateId stance=stateId("conb_ec_stance");
 // --- Line A: welded shutters across the hall, a gap cut at the middle --------------------------
 // Checkpoint A, failed: a wall across the hall, shutters to head height and block above, and the
 // lockdown door at its middle sealed to the floor, its beacon the only red you see from the north.
 // Its console still runs and still refuses. East, what did not wait for the door tore its own way
 // through panel 6 and the block over it: that hole is the way on.
 wall(m,1.f,6.2f,10.5f,6.5f,f,f+2.6f,2);wall(m,13.5f,6.2f,19.6f,6.5f,f,f+2.6f,2);wall(m,21.1f,6.2f,23.f,6.5f,f,f+2.6f,2);
 for(auto [x1,x2,lo]:{std::tuple{1.f,10.5f,2.6f},{10.5f,13.5f,3.4f},{13.5f,19.6f,2.6f},{21.1f,23.f,2.6f},{19.6f,21.1f,2.55f}})wall(m,x1,6.2f,x2,6.5f,f+lo,roof,15);
 wall(m,19.6f,6.2f,20.05f,6.5f,f+2.2f,f+2.55f,15);wall(m,20.7f,6.2f,21.1f,6.5f,f+2.35f,f+2.55f,15);wall(m,20.05f,6.22f,20.35f,6.48f,f+2.42f,f+2.55f,15);   // its torn lip
 for(float x:{4.f,8.f,16.f})wall(m,x-.25f,5.95f,x+.25f,6.2f,f,roof,15);                                  // the wall's piers
 for(float x:{10.5f,13.f})wall(m,x,6.f,x+.5f,6.7f,f,f+3.4f,7);wall(m,10.5f,6.f,13.5f,6.7f,f+2.9f,f+3.4f,7);
 for(float x:{10.5f,13.f})wall(m,x,5.98f,x+.5f,6.f,f+.2f,f+2.6f,13);
 for(float z=f+.03f;z<f+2.86f;z+=.17f)wall(m,11.f,6.3f,13.f,6.4f,z,z+.13f,2);                            // the shutter, sealed
 wall(m,11.f,6.29f,13.f,6.41f,f,f+2.9f,CollisionProxyMaterial);
 wall(m,11.f,5.96f,13.f,6.f,f+.02f,f+.12f,13);                                                            // its striped foot
 for(auto [x1,x2,z1,z2]:{std::tuple{11.4f,11.9f,1.1f,1.7f},{12.3f,12.65f,.4f,.9f},{11.9f,12.2f,1.9f,2.3f}})wall(m,x1,6.26f,x2,6.3f,f+z1,f+z2,16);   // hands on it, from this side
 // its console, on the north side where the Force stood to work it
 wall(m,13.85f,5.25f,15.35f,5.9f,f,f+.92f,2);wall(m,13.8f,5.2f,15.4f,5.95f,f+.92f,f+.96f,8);
 cbFix(m,model::lab_control_board,14.6f,5.88f,f+.96f,cbN,false);
 cbFix(m,model::lab_status_display,13.25f,5.97f,f+2.25f,cbN,false);
 terminal(m,{14.6f,5.5f},"CHECKPOINT A / LOCKDOWN","03:18 SECTOR DOOR LOWERED ON LOSS. 03:19 DOOR SEALED. OPEN REQUESTED. REFUSED. REFUSED. REFUSED.",
  "03:22 SHUTTER BREACH EAST, PANEL 6. 03:24 POST ABANDONED. LOCKDOWN STILL IN FORCE.",.96f);
 m.terminals.back().yaw=kPi;
 m.lights.push_back(cbGlow({14.6f,4.85f},f+1.4f,.5f,3.5f,9000,{.5f,.9f,1.f},.12f));
 m.fixtures.push_back({model::mine_cage_lamp,{12.f,5.9f},f+3.f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 {auto l=cbAlarm({12.f,5.97f},f+3.1f,blN,0);l.period=1.f;l.onTime=.45f;l.intensity=1.5f;l.range=6.f;m.lights.push_back(l);}
 {auto l=cbAlarm({12.f,6.73f},f+3.15f,blS,.55f);l.period=1.f;l.onTime=.45f;l.intensity=.3f;l.range=4.f;m.lights.push_back(l);}
 // the tear through panel 6: slats bent up out of the way, scorched, the drag of something through
 for(float z:{f+2.1f,f+2.36f})wall(m,19.6f,6.25f,21.1f,6.45f,z,z+.1f,2);
 for(float x:{19.6f,21.f})wall(m,x,6.18f,x+.1f,6.52f,f,f+2.2f,8);
 for(float x:{19.5f,21.05f})wall(m,x,6.15f,x+.15f,6.55f,f+.6f,f+2.1f,16);                               // scorched lips
 m.lights.push_back(cbGlow({20.35f,5.4f},f+.6f,.6f,3.5f,2400,{1.f,.5f,.2f},.4f));                         // something still smouldering on the floor
 for(auto [x1,y1,x2,y2]:{std::tuple{19.9f,5.6f,20.4f,5.75f},{20.6f,6.9f,21.2f,7.05f},{19.2f,7.3f,19.5f,7.6f}})wall(m,x1,y1,x2,y2,f+.002f,f+.04f,2);
 for(auto [x1,y1,x2,y2]:{std::tuple{19.7f,4.6f,20.5f,5.4f},{20.1f,6.6f,20.9f,7.6f},{19.6f,8.f,20.4f,9.f}})wall(m,x1,y1,x2,y2,f+.004f,f+.008f,16);
 m.lights.push_back(cbGlow({20.3f,7.4f},f+1.7f,1.3f,7.f,4600));                                           // the work light left by the tear
 cbFix(m,model::mine_board_barricade,5.f,6.6f,f,0,true);
 m.lights.push_back(lamp({2.4f,6.5f},f+2.35f,.45f,5.f,3400,0,WorldLightMount::Wall,blS));m.lights.back().flicker=.25f;
 // their board barricade, knocked flat off its feet by what came through, the barriers thrown
 for(float y:{6.85f,7.95f,9.05f})wall(m,13.55f,y,16.65f,y+.85f,f,f+.07f,10);
 for(float y:{7.75f,8.85f})wall(m,13.55f,y,16.65f,y+.1f,f+.07f,f+.1f,13);
 for(float x:{13.7f,16.4f})wall(m,x,9.95f,x+.12f,10.05f,f,f+.75f,2);
 cbFix(m,model::barrier,9.f,7.8f,f,.5f,true);
 wall(m,6.2f,8.5f,8.2f,9.05f,f,f+.5f,15);floorLine(m,6.2f,8.5f,8.2f,9.05f,f+.5f,13);                       // a jersey barrier, on its side
 for(auto [x,y,v,yaw]:{std::tuple{8.f,7.6f,ActorVisual::Cleanup,.2f},{11.6f,8.4f,ActorVisual::CleanupRusher,2.7f},{15.6f,7.6f,ActorVisual::Cleanup,1.4f}})cbBody(m,clock,v,{x,y},f,yaw);
 m.fixtures.push_back({model::med_corpse_supine,{20.f,9.6f},0,.744f,1.753f,.411f,cbE,false});
 // --- The welded room (west): it knocks -----------------------------------------------------------
 wall(m,1.f,7.8f,6.2f,8.f,f,roof,15);wall(m,6.f,8.f,6.2f,13.f,f,roof,15);
 {const int d=wallDoor(m,1.f,6.2f,13.f,13.2f,3.f,4.2f,f,roof,1);m.doors[size_t(d)].requireState=stateId("conb_ec_welded");}   // never: welded
 for(float x:{3.f,4.15f})wall(m,x,13.2f,x+.05f,13.26f,f,f+2.4f,13);
 taped(m,{2.f,13.2f},f+1.5f,cbS,Readable::Note,"WELDED 03:30","DO NOT OPEN. WHATEVER IT SAYS. IT KNOWS OUR NAMES NOW.","");
 // It knocks: on the door, now and then, never the same gap twice; and something taps along the wall.
 m.ambients.push_back({Sound::OneMetalBang,{3.6f,12.7f},f+1.3f,.55f,13,AmbientEmitter::Random,16,12});
 m.ambients.push_back({Sound::AmbMetalTapping,{2.f,10.4f},f+1.f,.3f,7,AmbientEmitter::Loop});
 cbBody(m,clock,ActorVisual::CleanupMedic,{3.6f,10.4f},f,.8f);
 // --- Line B: crates and barriers across the hall, open at the west wall --------------------------
 for(float x:{7.f,8.7f,15.5f,17.2f})cbFix(m,model::barrier,x,15.5f,f,0,true);
 for(float x:{10.2f,11.4f,12.6f,13.8f,18.6f,19.8f,21.f,22.2f})cbFix(m,model::lab_specimen_crate,x,15.5f,f,(x-16.f)*.006f,true);
 // --- The control room (east): the release -------------------------------------------------------
 wall(m,17.f,7.8f,23.f,8.f,f,roof,15);
 wall(m,16.8f,8.f,17.f,8.6f,f,roof,15);cbGlaze(m,16.8f,8.6f,17.f,12.4f,f,roof);wall(m,16.8f,12.4f,17.f,13.2f,f,roof,15);
 wallDoor(m,17.f,23.f,13.f,13.2f,19.6f,20.8f,f,roof,-1);
 cbFix(m,model::lab_control_board,22.9f,10.6f,f+.95f,cbW,false);
 terminal(m,{22.2f,10.6f},"EMERGENCY BARRICADE / SOUTH","LOWERED 03:20 ON SECTOR LOSS. ACCUMULATOR BLED DOWN 03:41 BY ORDER. RAISE NEEDS 180 BAR.",
  "PUMP SET: SOUTH WALL, WEST OF LINE B. RAISE ON ORDER OF THE INVESTIGATOR ONLY. HE HAS NOT ORDERED IT.",0,stateId("cont_barricade"),stateId("conb_ec_pressure"));
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
 // Written off ("He means us"), they are hers, if she has kept her side of the truce.
 cbEvent(m,"conb_ec_written_off",0,0,24,24,"conb_ec_radio_done",{cbSet("conb_ec_stance",int(SquadStance::Ally))});
 m.scriptEvents.back().conditions.push_back({ScriptCondition::Source::State,stance,ScriptCondition::Op::Equal,int(SquadStance::Truce)});
 // The pump set that charges the barricade's accumulator: bled down by order at 03:41 so nobody
 // could raise it from the console. Its line runs high along the south wall to the rams at the door.
 cbFix(m,model::mine_dewatering_pump,1.52f,20.9f,f,cbE,true);
 vessel(m,{1.62f,17.2f},.42f,f,f+2.3f,4);
 // the accumulator's line: up off the vessel's head, along the wall, into the high run to the rams
 m.pipes.push_back({{1.62f,17.2f},{1.62f,17.2f},f+2.3f,.05f,f+3.05f,4});m.pipes.push_back({{1.62f,17.2f},{1.16f,17.2f},f+3.05f,.05f,f+3.05f,4});
 m.pipes.push_back({{1.16f,17.2f},{1.16f,22.6f},f+3.05f,.05f,f+3.05f,4});m.pipes.push_back({{1.16f,22.6f},{1.2f,22.6f},f+3.05f,.05f,f+3.05f,4});
 terminal(m,{1.75f,18.55f},"BARRICADE HYDRAULICS / PUMP SET","ACCUMULATOR 0 BAR. BLED 03:41, ORDER OF THE INVESTIGATOR. PUMP SET ON LOCAL. CHARGE TO 180 BAR?",
  "03:41 MANUAL ANNOTATION: IF YOU ARE READING THIS AND YOU ARE NOT HIM, CHARGE IT.",0,stateId("conb_ec_pressure"));
 m.terminals.back().yaw=cbE;
 m.lights.push_back(lamp({1.f,19.15f},f+2.45f,.6f,6.f,3400,0,WorldLightMount::Wall,blE));
 m.fixtures.push_back({model::mine_cage_lamp,{1.1f,19.15f},f+2.25f-m.layers.front().elevation,.21f,.2f,.36f,0,false});
 {auto l=cbGlow({1.9f,18.55f},f+1.3f,.3f,2.6f,9000,{.5f,1.f,.6f});l.enabledState=stateId("conb_ec_pressure");m.lights.push_back(l);}   // CHARGED
 {ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=Sound::OneMetalCreak;a.amount=.6f;cbEvent(m,"conb_ec_pumped",0,0,24,24,"conb_ec_pressure",{a});}
 servicePipe(m,{1.2f,22.6f},{9.6f,22.6f},f+3.05f,.06f,4);
 // the rams either side of the door, the barricade's crash beam riding on them
 for(float x:{9.72f,14.28f}){m.pipes.push_back({{x,22.22f},{x,22.22f},f+.15f,.11f,f+2.9f,4});
  flange(m,{x,22.22f,f+.2f},{},.11f);flange(m,{x,22.22f,f+2.85f},{},.11f);
  for(float z:{.1f,2.86f})wall(m,x-.16f,22.06f,x+.16f,22.4f,f+z,f+z+.12f,8);                          // its strapped feet and head
  wall(m,x-.14f,22.08f,x+.14f,22.4f,f,f+3.f,CollisionProxyMaterial);}
 m.pickupSpawns.push_back({{21.6f,22.2f},PickupKind::Ammo,f});m.pickupSpawns.push_back({{3.4f,21.8f},PickupKind::Health,f});
 // --- Light: tubes mostly out, the flood by the detail, red on the lines and the door -------------
 m.lights.push_back(cbTube(6.f,3.f,roof,.6f,6.5f,5200,.03f,.4f));m.lights.push_back(cbTube(12.f,11.f,roof,.65f,7.f,5200,.03f));m.lights.push_back(cbTube(12.f,3.f,roof,.6f,6.5f,5200,.03f));m.lights.push_back(cbTube(4.f,11.f,roof,.4f,5.f,5200,.03f,.3f));
 m.lights.push_back(cbTube(4.f,18.f,roof,.4f,5.f,5200,.03f,.3f));
 // the detail's half kept its own tubes on, so the second line and the way to the door read
 for(auto [x,y,i]:{std::tuple{8.6f,17.4f,.55f},{16.f,17.4f,.55f},{8.5f,11.6f,.4f},{19.f,22.f,.4f}})m.lights.push_back(cbTube(x,y,roof,i,6.5f,5200,.03f,0));
 m.lights.push_back(cbTube(18.f,3.f,roof,.45f,6.f,5200,.03f,.35f));cbDeadTube(m,12.f,18.f,roof);cbDeadTube(m,7.f,11.f,roof);
 recessedFittings(m);
 zone(m,"EMERGENCY CONTAINMENT / CONTROL ROOM",17.f,8.f,23.f,13.f);
 // The hall's frame: downstand beams across the roof, two columns in the detail's half, pilasters.
 for(float y:{4.3f,9.3f,14.3f,20.6f})wall(m,1.f,y,23.f,y+.3f,roof-.45f,roof,15);
 for(float x:{6.5f,17.8f}){wall(m,x-.3f,18.3f,x+.3f,18.9f,f,roof,15);wall(m,x-.38f,18.22f,x+.38f,18.98f,f,f+.3f,15);floorLine(m,x-.38f,18.22f,x+.38f,18.34f,f+.3f,13);}
 wallBays(m,f,roof,4,false,true,true,true,12);
 zone(m,"",8.f,20.f,16.f,24.f,-1000,1000,{.13f,.12f,.12f,.006f});
 zone(m,"EMERGENCY CONTAINMENT",0,0,24,24,-1000,1000,{.12f,.12f,.12f,.008f});
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

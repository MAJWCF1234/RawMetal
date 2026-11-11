// DEPTHWORKS MEDICAL (chapter 2, manifest 48 (subchapter)): chunks 75, 76, Ward C.
// One long observation ward stitched across two chunks: 75 the north half off the
// Imaging corridor, 76 the south half with Lab 4's service door, the theatre transfer
// bay east to Surgery (77) and the mortuary corridor south to the Morgue stair (78).
// See docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- MedicalWard kit (mw) -----------------------------------------------------------------
// Facing for +z-fronted models (screens, cabinets, boards): the way the front looks.
constexpr float mwS=0,mwN=kPi,mwE=kPi*.5f,mwW=-kPi*.5f;
// The mattress top of med_ward_bed (its side rails are at .30 of .84; the mattress crowns at .41).
constexpr float mwDeck=.41f;
void mwFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
// A swinging clinic door in a wall along x: the renderer draws leaf, posts and head to
// 2.55 m; the wall closes above. lock: a state the door waits for (0: free).
void mwSwing(Map& m,float x1,float x2,float y,float f,float roof,float swing,float open=0,StateId lock=0){
 wall(m,x1,y-.1f,x2,y+.1f,f+2.55f,roof,3);
 Door d{x1,x2,y,open};d.swinging=true;d.swing=swing;d.opening=open>0;d.maxOpen=.9f;d.requireState=lock;m.doors.push_back(d);
}
// An open doorway through a wall along y: pale-steel jambs and head, wall above.
void mwDoorwayY(Map& m,float y1,float y2,float x1,float x2,float f,float roof,float h=2.3f){
 wall(m,x1,y1,x2,y2,f+h,roof,3);
 for(float y:{y1-.06f,y2})wall(m,x1-.03f,y,x2+.03f,y+.06f,f,f+h,20);
 wall(m,x1-.03f,y1,x2+.03f,y2,f+h-.04f,f+h,20);
}
// The same through a wall along x.
void mwDoorwayX(Map& m,float x1,float x2,float y1,float y2,float f,float roof,float h=2.3f){
 wall(m,x1,y1,x2,y2,f+h,roof,3);
 for(float x:{x1-.06f,x2})wall(m,x,y1-.03f,x+.06f,y2+.03f,f,f+h,20);
 wall(m,x1,y1-.03f,x2,y2+.03f,f+h-.04f,f+h,20);
}
// Wired glazing in a straight wall: painted dado, steel sill and mullions, glass to `head`,
// wall above; an invisible proxy fills sill to head so nobody perches or steps through.
// counter: a shelf that deep on side `side` (-1: -x/-y, +1: +x/+y).
void mwGlaze(Map& m,float x1,float y1,float x2,float y2,float f,float roof,float sill,float head,bool holds,float counter=0,float side=1){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,3);
 if(alongX)wall(m,x1,y1-.03f,x2,y2+.03f,f+sill-.03f,f+sill+.01f,20);else wall(m,x1-.03f,y1,x2+.03f,y2,f+sill-.03f,f+sill+.01f,20);
 const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)kit::glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill+.01f,f+head,17,holds);else kit::glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill+.01f,f+head,17,holds);
 const float len=alongX?x2-x1:y2-y1;const int bays=std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.025f,alongX?x1:y1,(alongX?x2:y2)-.05f);
  if(alongX)wall(m,t,y1+.02f,t+.05f,y2-.02f,f+sill+.01f,f+head,20);else wall(m,x1+.02f,t,x2-.02f,t+.05f,f+sill+.01f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.06f,20);if(roof>f+head+.06f)wall(m,x1,y1,x2,y2,f+head+.06f,roof,3);
 // counter: a kneehole worktop on side `side`, on end panels, a modesty board at the wall.
 if(counter>0){
  if(alongX){const float a=side>0?y2:y1-counter,b=side>0?y2+counter:y1,lo=std::min(a,b),hi=std::max(a,b),w=side>0?lo:hi;
   wall(m,x1,lo,x2,hi,f+sill-.05f,f+sill,20);for(float x:{x1,x2-.04f})wall(m,x,lo,x+.04f,hi,f,f+sill-.05f,21);wall(m,x1,std::min(w,w+side*.03f),x2,std::max(w,w+side*.03f),f+.3f,f+sill-.05f,21);}
  else{const float a=side>0?x2:x1-counter,b=side>0?x2+counter:x1,lo=std::min(a,b),hi=std::max(a,b),w=side>0?lo:hi;
   wall(m,lo,y1,hi,y2,f+sill-.05f,f+sill,20);for(float y:{y1,y2-.04f})wall(m,lo,y,hi,y+.04f,f,f+sill-.05f,21);wall(m,std::min(w,w+side*.03f),y1,std::max(w,w+side*.03f),y2,f+.3f,f+sill-.05f,21);}
 }
 const float proxyTop=std::max(f+head,roof);
 if(alongX)wall(m,x1,y1-.04f,x2,y2+.04f,f+sill+.01f,proxyTop,30);else wall(m,x1-.04f,y1,x2+.04f,y2,f+sill+.01f,proxyTop,30);
}
// Cubicle curtain under its track: fabric along x at y (or along y at x), feet showing below.
// A proxy closes the gap above it to the ceiling: nobody crawls along a curtain top.
void mwCurtain(Map& m,float a,float b,float at,bool alongX,float f){
 const float roof=m.definition.ceiling;
 if(alongX){wall(m,a,at-.02f,b,at+.02f,f+.30f,f+2.35f,39);wall(m,a,at-.02f,b,at+.02f,f+2.35f,roof,30);}
 else{wall(m,at-.02f,a,at+.02f,b,f+.30f,f+2.35f,39);wall(m,at-.02f,a,at+.02f,b,f+2.35f,roof,30);}
}
// A curtain drawn back: the fabric bunched at the end of its track.
void mwBunch(Map& m,float x,float y,float f,bool alongX){
 const float roof=m.definition.ceiling;
 if(alongX){wall(m,x-.08f,y-.06f,x+.08f,y+.06f,f+.24f,f+2.35f,39);wall(m,x-.06f,y+.04f,x+.1f,y+.12f,f+.27f,f+2.31f,39);wall(m,x-.08f,y-.06f,x+.1f,y+.12f,f+2.31f,roof,30);}
 else{wall(m,x-.06f,y-.08f,x+.06f,y+.08f,f+.24f,f+2.35f,39);wall(m,x+.04f,y-.06f,x+.12f,y+.1f,f+.27f,f+2.31f,39);wall(m,x-.06f,y-.08f,x+.12f,y+.1f,f+2.31f,roof,30);}
}
// Ceiling track: hung from the roof by a drop rod at each end.
void mwTrack(Map& m,Vec2 a,Vec2 b,float f,float roof){
 m.pipes.push_back({a,b,f+2.40f,.015f,-999,5});
 for(Vec2 q:{a,b})m.pipes.push_back({q,q,f+2.40f,.007f,roof,5});
}
// A collision-only cap that casts no shadow: the renderer never draws material 30, the light
// pass takes only box shapes, and movement and the audit go by a structure's bounds.
void mwCap(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){wall(m,x1,y1,x2,y2,lo,hi,30);m.structures.back().shape=ShapeColumn;}
// The shut door of a pack washroom stall, picked out on its flat front (the face at px,py,
// looking along x when faceX, toward dir): head rail, door edges, hinges, the gap under the
// leaf, pull and bolt plate with its indicator. Trim only, nothing collides.
void mwStallFront(Map& m,float px,float py,bool faceX,float dir,float f,bool engaged){
 auto box=[&](float a1,float a2,float depth,float lo,float hi,int mat){
  if(faceX)wall(m,px,py+a1,px+dir*depth,py+a2,f+lo,f+hi,mat);else wall(m,px+a1,py,px+a2,py+dir*depth,f+lo,f+hi,mat);
  m.structures.back().solid=false;};
 box(-1.05f,1.05f,.03f,2.f,2.07f,20);
 for(float a:{-.4f,.375f})box(a,a+.025f,.015f,.14f,2.f,20);
 box(-.375f,.375f,.012f,0.f,.14f,8);
 for(float z:{.4f,1.1f,1.7f})box(-.375f,-.335f,.025f,z,z+.09f,20);
 box(.25f,.34f,.02f,1.f,1.11f,20);box(.28f,.31f,.026f,1.04f,1.07f,engaged?19:21);
 box(.19f,.215f,.04f,.93f,1.13f,20);
}
// A bed-head shelf on a wall along y (face at wallX, the bay toward dir) with its monitor;
// dead: the bed-head circuit is out (or the patient is gone), the screen dark and grimed.
void mwShelf(Map& m,float wallX,float y,float dir,float f,bool monitor,bool dead=false){
 const float a=wallX,b=wallX+dir*.25f;
 wall(m,std::min(a,b),y-.25f,std::max(a,b),y+.25f,f+1.38f,f+1.42f,20);
 mwCap(m,std::min(a,b),y-.25f,std::max(a,b),y+.25f,f+1.72f,m.definition.ceiling);   // clear of the monitor, to the ceiling: nobody crouches on a bed-head shelf
 wall(m,std::min(a,a+dir*.1f),y-.02f,std::max(a,a+dir*.1f),y+.02f,f+1.28f,f+1.38f,20);
 if(monitor)kit::prop(m,dead?model::med_vitals_monitor_dead:model::med_vitals_monitor,wallX+dir*.12f,y,f+1.42f,.38f,.181f,.28f,dir>0?mwE:mwW);
}
// The same on a wall along x (face at wallY).
void mwShelfX(Map& m,float x,float wallY,float dir,float f,bool monitor,bool dead=false){
 const float a=wallY,b=wallY+dir*.25f;
 wall(m,x-.25f,std::min(a,b),x+.25f,std::max(a,b),f+1.38f,f+1.42f,20);
 mwCap(m,x-.25f,std::min(a,b),x+.25f,std::max(a,b),f+1.72f,m.definition.ceiling);
 wall(m,x-.02f,std::min(a,a+dir*.1f),x+.02f,std::max(a,a+dir*.1f),f+1.28f,f+1.38f,20);
 if(monitor)kit::prop(m,dead?model::med_vitals_monitor_dead:model::med_vitals_monitor,x,wallY+dir*.12f,f+1.42f,.38f,.181f,.28f,dir>0?mwS:mwN);
}
// An IV stand: weighted base, pole, hook, and the bag hanging from it.
void mwDrip(Map& m,float x,float y,float f,bool bag=true){
 wall(m,x-.18f,y-.18f,x+.18f,y+.18f,f,f+.03f,20);
 m.pipes.push_back({{x,y},{x,y},f+.03f,.012f,f+1.9f,5});wall(m,x-.18f,y-.18f,x+.18f,y+.18f,f+.03f,f+3.f,30);   // to the ceiling: nobody stands on a drip
 if(bag){m.pipes.push_back({{x,y},{x+.15f,y},f+1.88f,.008f,f+1.88f,5});mwFix(m,model::med_blood_bag,x+.15f,y,f+1.72f,.11f,.02f,.14f,mwS,false);}
}
void mwBlood(Map& m,float x1,float y1,float x2,float y2,float f){wall(m,x1,y1,x2,y2,f+.004f,f+.010f,16);}
// Ward lighting: every ordinary lamp in Ward C dies with the ward's power (med_blackout).
WorldLight mwOut(WorldLight l){l.enabledState=stateId("med_blackout");l.invert=true;return l;}
WorldLight mwTube(float x,float y,float roof,float intensity,float range,float kelvin,float tint,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return mwOut(l);
}
// The bed-head tube: a single fluorescent fitting along the bed (beds run along x).
WorldLight mwBedLight(float x,float y,float roof,float intensity,float flicker=0){
 auto l=lamp({x,y},roof-.30f,intensity,3.4f,6400,.12f);l.fitting=model::med_fluoro_light;l.fittingWidth=.11f;l.fittingDepth=1.2f;l.fittingHeight=.13f;l.yaw=kPi*.5f;l.flicker=flicker;return mwOut(l);
}
// A dead fitting: its own lamp record on a circuit that never comes on, so the housing
// reads unlit in the light and stays black in the blackout (a bare fixture would glow).
WorldLight mwDead(WorldLight l){l.enabledState=stateId("medw_dead_circuit");l.invert=false;l.flicker=0;return l;}
// The emergency circuit: a dim red caged wall lamp that only burns while the ward is dark.
WorldLight mwRed(float x,float y,float z,float yaw){
 auto l=lamp({x,y},z,.15f,3.5f,1000,-.5f,WorldLightMount::Wall,yaw);l.enabledState=stateId("med_blackout");return l;
}
ScriptAction mwSet(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
ScriptAction mwDo(ScriptAction::Type type,Sound sound=Sound::Exit,float amount=0,int index=0){ScriptAction a;a.type=type;a.sound=sound;a.amount=amount;a.index=index;return a;}
void mwEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,int value,std::vector<ScriptAction> actions){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=true;
 if(*require){e.requireState=stateId(require);e.requireValue=value;}e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
void mwSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,bool loop=false){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=-9;s.y1=-9;s.x2=-8;s.y2=-8;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;s.loop=loop;m.timedSequences.push_back(s);
}
// A padded seclusion lining: quilted fabric over the walls from the floor to 2.3 m.
// Above its cap a proxy to the ceiling: nobody perches on the lining.
void mwPad(Map& m,float x1,float y1,float x2,float y2,float f){wall(m,x1,y1,x2,y2,f,f+2.3f,39);wall(m,std::min(x1,x2),std::min(y1,y2),std::max(x1,x2),std::max(y1,y2),f+2.3f,f+2.34f,20);
 wall(m,std::min(x1,x2),std::min(y1,y2),std::max(x1,x2),std::max(y1,y2),f+2.34f,f+3.f,30);}
// Restraints on a bed running along x from its head at hx toward dir, centred on y.
// 0: unbuckled, the straps hanging off both sides; 1: buckled across, nobody in them;
// 2: cut through, the ends hanging.
// alongY: the bed runs along y instead (head at `head` on the y axis, `across` its x).
// The straps keep clear of the bed's middle and ends, where the audit samples it.
void mwStraps(Map& m,float head,float across,float dir,float f,int how,bool alongY=false){
 const float top=f+mwDeck+.02f;
 auto box=[&](float a1,float c1,float a2,float c2,float lo,float hi,int mat){if(alongY)wall(m,c1,a1,c2,a2,lo,hi,mat);else wall(m,a1,c1,a2,c2,lo,hi,mat);};
 for(float d:{.66f,1.22f,1.72f}){const float a=head+dir*d,c=across;
  if(how==1){box(a-.03f,c-.48f,a+.03f,c+.48f,top,top+.012f,8);box(a-.045f,c+.22f,a+.045f,c+.3f,top+.012f,top+.02f,20);}
  else if(how==2){   // cut: one half still across the mattress, a hand's width gone, the other half hanging
   box(a-.03f,c-.485f,a+.03f,c-.06f,top,top+.012f,8);box(a-.03f,c+.07f,a+.03f,c+.2f,top,top+.012f,8);
   box(a-.03f,c+.48f,a+.03f,c+.49f,top-.18f,top,8);box(a-.03f,c-.495f,a+.03f,c-.485f,top-.22f,top+.012f,8);}
  else for(float s:{-1.f,1.f})box(a-.03f,c+s*.485f-.005f,a+.03f,c+s*.485f+.005f,top-.22f,top,8);
 }
}
// One curtained bay along a wall: the bed (head to the wall at wallX, foot toward dir),
// bed-head trunking, shelf and monitor, its drip, chair, curtains and tracks.
// bed: 0 made, straps open; 1 straps buckled, empty; 2 tossed; 3 wrecked frame, mattress
// dragged off; 4 cut loose; 5 no bed (wheeled out). front: 0 open, 1 half, 2 shut, 3 shut but
// for a gap at the far end. light: 0 dead, 1 lit, 2 failing.
struct MwBay{float ya,yb;int bed,front,light;bool monitor,drip,chair;};
void mwBay(Map& m,float wallX,float dir,const MwBay& b,float f,float roof){
 const float y=(b.ya+b.yb)*.5f,head=wallX+dir*.12f,cx=wallX+dir*1.11f,fx=wallX+dir*2.75f;
 const float yawFoot=dir>0?kPi:0.f;
 // Bed-head services: trunking, two outlets, the oxygen flowmeter.
 wall(m,std::min(wallX,wallX+dir*.07f),y-.8f,std::max(wallX,wallX+dir*.07f),y+.8f,f+1.2f,f+1.36f,20);
 for(float s:{-.45f,.45f})wall(m,std::min(wallX,wallX+dir*.1f),y+s-.06f,std::max(wallX,wallX+dir*.1f),y+s+.06f,f+1.24f,f+1.32f,s<0?19:2);
 m.pipes.push_back({{wallX+dir*.1f,y+.62f},{wallX+dir*.1f,y+.62f},f+1.0f,.03f,f+1.2f,5});
 if(b.bed!=5){mwShelf(m,wallX,y-.05f,dir,f,b.monitor,b.light==0);}   // a dead bed-head: its monitor dark too
 switch(b.bed){
  case 0:mwFix(m,model::med_ward_bed,cx,y,f,1.984f,.944f,.842f,yawFoot,true);mwStraps(m,head,y,dir,f,0);
   wall(m,std::min(wallX+dir*1.38f,wallX+dir*1.92f),y-.4f,std::max(wallX+dir*1.38f,wallX+dir*1.92f),y+.4f,f+mwDeck,f+mwDeck+.07f,23);break;   // folded blanket
  case 1:mwFix(m,model::med_ward_bed,cx,y,f,1.984f,.944f,.842f,yawFoot,true);mwStraps(m,head,y,dir,f,1);break;
  case 2:mwFix(m,model::med_ward_bed_tossed,cx+dir*.06f,y+.12f,f,2.097f,1.525f,.842f,yawFoot+dir*.1f,true);break;
  case 3:mwFix(m,model::med_bed_frame_wrecked,cx,y,f,1.984f,.944f,.842f,yawFoot+.05f,true);
   kit::prop(m,model::med_mattress_stained,wallX+dir*3.15f,y+.25f,f,1.9f,.9f,.118f,.3f);break;   // dragged out under the curtain
  case 4:mwFix(m,model::med_ward_bed,cx,y,f,1.984f,.944f,.842f,yawFoot,true);mwStraps(m,head,y,dir,f,2);break;
  default:for(float s:{-.3f,.3f})floorLine(m,std::min(wallX+dir*.4f,fx+dir*.6f),y+s-.02f,std::max(wallX+dir*.4f,fx+dir*.6f),y+s+.02f,f,8);break;
 }
 if(b.drip)mwDrip(m,wallX+dir*.32f,b.ya+.3f,f,b.bed!=5);
 if(b.chair&&b.bed!=2&&b.bed!=3)kit::prop(m,model::wooden_chair,wallX+dir*1.35f,b.yb-.38f,f,.417f,.525f,.95f,dir*.12f);   // visitor's chair, turned to the bed
 auto bedLight=mwBedLight(wallX+dir*.85f,y,roof,.38f,b.light==2?.55f:0.f);m.lights.push_back(b.light==0?mwDead(bedLight):bedLight);
 // Curtains: the front along y at fx, the partitions along x at ya and yb.
 mwTrack(m,{fx,b.ya},{fx,b.yb},f,roof);
 const float lo=std::min(wallX,fx),hi=std::max(wallX,fx);
 const bool shut=b.front>=2;
 if(shut){mwCurtain(m,lo+.05f,hi,b.ya+.04f,true,f);mwCurtain(m,lo+.05f,hi,b.yb-.04f,true,f);}
 else{mwBunch(m,wallX+dir*.15f,b.ya+.08f,f,true);mwBunch(m,wallX+dir*.15f,b.yb-.08f,f,true);}
 switch(b.front){
  case 0:mwBunch(m,fx,b.ya+.1f,f,false);break;
  case 1:mwCurtain(m,b.ya+.04f,y,fx,false,f);mwBunch(m,fx,y+.08f,f,false);break;
  case 2:mwCurtain(m,b.ya+.04f,b.yb-.04f,fx,false,f);break;
  default:mwCurtain(m,b.ya+.04f,b.yb-.5f,fx,false,f);mwBunch(m,fx,b.yb-.42f,f,false);break;
 }
}

// MEDICAL / WARD C NORTH (manifest 48). Tier-3 observation: where Depthworks put the workers
// who read too high to send back up. The Imaging corridor ends at the ward's held-open doors:
// admissions (property bagged, bands checked) to the west, the clerk's door-control booth to the
// east, then the long ward itself under cold tubes, its beds strapped, its curtains half drawn.
// Off it: the seclusion pair (one patient at his panel, one facing the wall), the patients'
// bathroom, the nurses' station with its window on the ward and the fifteen-minute log that
// teaches the rule (they stop when the lights are up), the drug room, the charge nurse's office.
// The ward runs on through the open seam into 76. Route: N seam -> doors -> the aisle south.
std::shared_ptr<const Map> makeMedicalWardCNorth(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[75];m.definition=CampaignChunks[75];m.definition.ambient=.22f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 m.definition.playerStart={12.f,1.7f};m.definition.spawnYaw=kPi*.5f;m.definition.spawnHeight=-12;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});
 openEdge(m,{'S',2.f,22.f,f,false,0});
 machineryClock(m,"medw_n_clock",60000);
 const StateId dark=stateId("med_blackout");
 // --- Plan (walls .2 m, painted). The corridor keeps 73's faces at x 10 and 14 ---------------------
 wall(m,9.8f,1.f,10.f,1.8f,f,roof,3);mwDoorwayY(m,1.8f,3.f,9.8f,10.f,f,roof);wall(m,9.8f,3.f,10.f,5.8f,f,roof,3);   // admissions
 wall(m,14.f,1.f,14.2f,1.6f,f,roof,3);mwGlaze(m,14.f,1.6f,14.2f,4.f,f,roof,.95f,2.1f,true);wall(m,14.f,4.f,14.2f,5.8f,f,roof,3); // the clerk's hatch
 wall(m,1.f,5.8f,10.8f,6.f,f,roof,3);wall(m,13.2f,5.8f,19.f,6.f,f,roof,3);
 mwSwing(m,19.f,19.9f,5.9f,f,roof,-1);wall(m,19.9f,5.8f,23.f,6.f,f,roof,3);                                         // door 0: booth
 // The ward doors, hooked back flat against the ward side for the 02:40 transfer.
 wall(m,10.8f,5.8f,13.2f,6.f,f+2.4f,roof,3);wall(m,10.8f,5.77f,13.2f,6.03f,f+2.36f,f+2.4f,20);
 for(float x:{10.74f,13.2f})wall(m,x,5.77f,x+.06f,6.03f,f,f+2.4f,20);
 for(auto [a,b]:{std::pair{9.6f,10.74f},{13.26f,14.4f}}){
  wall(m,a,6.f,b,6.05f,f+.02f,f+2.3f,20);wall(m,a+.02f,6.05f,b-.02f,6.058f,f+.02f,f+.32f,8);
  wall(m,a+.3f,6.05f,b-.3f,6.06f,f+1.25f,f+1.75f,8);wall(m,a+.34f,6.06f,b-.34f,6.064f,f+1.29f,f+1.71f,17);   // wired vision panel
  const float hook=a<12?a+.1f:b-.14f;wall(m,hook,6.05f,hook+.04f,6.12f,f+.1f,f+.16f,20);}
 // West band: seclusion 1, the observation ante, seclusion 2, the bathroom.
 wall(m,6.8f,6.f,7.f,11.1f,f,roof,3);mwDoorwayY(m,11.1f,12.3f,6.8f,7.f,f,roof);wall(m,6.8f,12.3f,7.f,20.3f,f,roof,3);
 mwDoorwayY(m,20.3f,21.5f,6.8f,7.f,f,roof);wall(m,6.8f,21.5f,7.f,24.f,f,roof,3);
 wall(m,1.f,10.4f,2.8f,10.6f,f,roof,3);mwGlaze(m,2.8f,10.4f,4.1f,10.6f,f,roof,1.1f,1.75f,true);wall(m,4.1f,10.4f,4.4f,10.6f,f,roof,3);
 const int seclusionDoor=int(m.doors.size());
 mwSwing(m,4.4f,5.3f,10.5f,f,roof,-1,0,dark);wall(m,5.3f,10.4f,6.8f,10.6f,f,roof,3);                               // door 1: seclusion 1
 wall(m,1.f,12.4f,2.8f,12.6f,f,roof,3);mwGlaze(m,2.8f,12.4f,4.1f,12.6f,f,roof,1.1f,1.75f,true);wall(m,4.1f,12.4f,4.4f,12.6f,f,roof,3);
 mwSwing(m,4.4f,5.3f,12.5f,f,roof,1,0,dark);wall(m,5.3f,12.4f,6.8f,12.6f,f,roof,3);                              // door 2: seclusion 2
 wall(m,1.f,17.4f,6.8f,17.6f,f,roof,3);
 wall(m,1.f,23.9f,6.8f,24.f,f,roof,3);
 // East band: the nurses' station on the ward window, the drug room, the charge nurse's office.
 wall(m,17.f,6.f,17.2f,6.4f,f,roof,3);mwGlaze(m,17.f,6.4f,17.2f,10.6f,f,roof,.95f,2.2f,false,.55f,1);
 wall(m,17.f,10.6f,17.2f,10.9f,f,roof,3);mwDoorwayY(m,10.9f,12.1f,17.f,17.2f,f,roof);wall(m,17.f,12.1f,17.2f,20.3f,f,roof,3);
 mwDoorwayY(m,20.3f,21.5f,17.f,17.2f,f,roof);wall(m,17.f,21.5f,17.2f,24.f,f,roof,3);
 wall(m,17.2f,12.4f,21.f,12.6f,f,roof,3);mwSwing(m,21.f,21.9f,12.5f,f,roof,1);wall(m,21.9f,12.4f,23.f,12.6f,f,roof,3); // door 3: drug room
 wall(m,17.2f,17.6f,23.f,17.8f,f,roof,3);
 wall(m,17.2f,23.9f,23.f,24.f,f,roof,3);
 for(auto [x1,y1,x2,y2]:{std::tuple{10.f,0.f,14.f,6.f},{1.f,1.f,9.8f,5.8f},{14.2f,1.f,20.6f,5.8f},{2.2f,7.4f,6.8f,10.4f},{1.f,10.6f,6.8f,12.4f},
  {2.2f,12.6f,6.8f,15.8f},{1.f,17.6f,6.8f,23.9f},{7.f,6.f,17.f,24.f},{17.2f,6.f,23.f,12.4f},{17.2f,12.6f,23.f,17.6f},{17.2f,17.8f,23.f,23.9f}})ceilingGrid(m,x1,y1,x2,y2,roof);
 // --- The corridor in: the last of Imaging's scuffs, the ward's one sign ---------------------------
 wallSign(m,12.f,5.745f,f+2.42f,"WARD C","TIER 3 OBSERVATION / LIGHTS ON AT ALL TIMES",2.2f,kPi);
 extinguisher(m,10.f,4.6f,f,mwE);
 mwFix(m,model::med_notice_board,13.965f,5.f,f+1.25f,1.2f,.07f,.9f,mwW,false);
 for(float x:{11.55f,12.45f})floorLine(m,x,0.f,x+.04f,24.f,f,8);                                          // wheel scuffs, all the way down
 paperDrift(m,10.3f,.2f,13.7f,5.6f,f,4,7500);
 kit::prop(m,model::item_id_card,11.3f,3.6f,f,.132f,.085f,.002f,.7f);
 // --- Admissions: where they were stripped of the plant -------------------------------------------
 for(float y:{1.48f,2.36f,3.24f})mwFix(m,model::psx_locker,1.33f,y,f,.64f,.88f,2.25f,0,true);              // property lockers
 wall(m,3.f,1.f,8.6f,1.62f,f,f+.86f,21);wall(m,2.97f,1.f,8.63f,1.66f,f+.86f,f+.9f,20);                       // the property counter
 for(auto [x,yaw]:{std::pair{3.5f,.1f},{4.25f,-.2f},{8.2f,0.f}})kit::prop(m,model::prop_cardboard_box,x,1.3f,f+.9f,.436f,.432f,.22f,yaw);
 for(auto [x,yaw]:{std::pair{5.85f,.6f},{6.55f,2.1f}})kit::prop(m,model::mine_hard_hat,x,1.32f,f+.9f,.28f,.35f,.15f,yaw);
 kit::prop(m,model::item_dosimeter,7.4f,1.35f,f+.9f,.31f,.146f,.066f,.3f);
 kit::prop(m,model::item_id_card,5.4f,1.45f,f+.9f,.132f,.085f,.002f,2.f);
 bench(m,4.2f,5.25f,f);
 for(int i=0;i<6;++i){const float x=2.2f+i*.7f;wall(m,x+.17f,5.76f,x+.21f,5.8f,f+1.72f,f+1.78f,20);
  if(i!=2&&i!=4)wall(m,x,5.74f,x+.38f,5.77f,f+.95f,f+1.72f,23);}                                           // gowns on their hooks
 for(auto [x,y,w,d]:{std::tuple{2.6f,4.3f,.55f,.4f},{3.4f,4.55f,.4f,.5f},{6.3f,4.2f,.6f,.35f}})wall(m,x,y,x+w,y+d,f,f+.05f,23);   // clothes, left where they fell
 mwFix(m,model::desk,8.4f,4.3f,f,1.4f,.7f,.75f,mwW,true);
 kit::prop(m,model::office_chair,7.6f,4.35f,f,.55f,.55f,.95f,mwW+.2f);
 kit::prop(m,model::crt_monitor,8.55f,4.f,f+.75f,.42f,.4f,.38f,mwW);
 kit::prop(m,model::keyboard,8.2f,4.1f,f+.75f,.45f,.16f,.03f,mwW);
 kit::prop(m,model::desk_phone,8.5f,4.75f,f+.75f,.2f,.2f,.1f,mwW);
 terminal(m,{8.25f,4.6f},"WARD C / ADMISSIONS","02:40 NINE TIER 3 FROM TRIAGE UNDER ESCORT. BEDS 3 TO 11. RESTRAINED ON ARRIVAL. PROPERTY BAGGED.",
  "02:58 ONE TIER 2 FROM ENERGY DIAGNOSTICS, BY ORDER OF I.I. TO OBS 6. HER BAND SAYS AMBER. TREAT AS RED.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.75f;m.terminals.back().yaw=mwW;
 // The scale they were weighed on: platform, column, beam.
 wall(m,8.95f,5.f,9.55f,5.6f,f,f+.07f,20);m.pipes.push_back({{9.45f,5.3f},{9.45f,5.3f},f+.07f,.025f,f+1.35f,5});
 wall(m,9.22f,5.02f,9.62f,5.58f,f+.07f,roof,30);wall(m,9.3f,5.2f,9.6f,5.4f,f+1.35f,f+1.52f,20);
 mwFix(m,model::med_wall_clock,1.02f,4.6f,f+2.05f,.24f,.02f,.23f,mwE,false);
 kit::prop(m,model::med_wheelchair,2.9f,3.4f,f,.55f,1.04f,.91f,2.3f);
 kit::propPose(m,model::med_crutches,1.06f,4.25f,f,.17f,.06f,1.2f,mwE+.1f,0,.025f);                         // taken off a worker at the counter
 paperDrift(m,2.f,2.f,7.6f,4.7f,f,5,7501);
 // --- The booth: the clerk's hatch, the cameras, door control ---------------------------------------
 mwFix(m,model::med_nurse_desk,14.49f,2.8f,f,1.18f,.56f,.81f,mwW,true);
 kit::prop(m,model::office_chair,15.25f,2.85f,f,.55f,.55f,.95f,mwE);
 kit::prop(m,model::crt_monitor,14.45f,3.1f,f+.81f,.42f,.4f,.38f,mwE);
 kit::prop(m,model::office_paper_stack,14.5f,2.62f,f+.81f,.33f,.44f,.03f,.2f);
 // The camera shelf: a shallow board, nothing above it to climb to.
 wall(m,16.6f,1.f,20.4f,1.38f,f+1.78f,f+1.82f,20);for(float x:{16.8f,20.2f})wall(m,x-.02f,1.f,x+.02f,1.2f,f+1.6f,f+1.78f,20);
 wall(m,16.6f,1.f,20.4f,1.38f,f+1.82f,roof,30);
 for(float x:{17.3f,18.5f,19.7f})mwFix(m,model::cctv_tv,x,1.2f,f+1.82f,.5f,.4f,.42f,mwS,false);              // the ward on three screens
 kit::prop(m,model::office_chair,18.6f,2.3f,f,.55f,.55f,.95f,.2f);
 terminal(m,{18.6f,1.45f},"WARD C / DOOR CONTROL","OBS AND SECLUSION LOCKS ARE MAGNETIC. THEY FAIL OPEN ON POWER LOSS. FIRE CODE.",
  "IF THE WARD LOSES POWER, EVERY DOOR ON IT OPENS. DO NOT LET THE WARD LOSE POWER.");
 m.terminals.back().yaw=mwS;
 // The booth stops at x 20.6 (risers and plant behind): the door-release desk on that wall.
 wall(m,20.6f,1.f,23.f,5.8f,f,roof,3);
 for(float y:{1.86f,2.76f})mwFix(m,model::psx_locker,20.28f,y,f,.64f,.88f,2.25f,kPi,true);
 mwFix(m,model::desk,20.25f,4.25f,f,1.4f,.7f,.75f,mwW,true);
 kit::prop(m,model::office_chair,18.9f,3.6f,f,.55f,.55f,.95f,mwE-.3f);                                     // rolled back off the door line
 kit::prop(m,model::crt_monitor,20.38f,4.55f,f+.75f,.42f,.4f,.38f,mwW);
 kit::prop(m,model::keyboard,20.05f,4.45f,f+.75f,.45f,.16f,.03f,mwW);
 mwFix(m,model::pwr_breaker_panel,20.549f,3.95f,f+1.15f,.183f,.102f,.457f,mwW,false);                        // door release
 mwFix(m,model::pwr_breaker_panel,20.549f,4.3f,f+1.15f,.183f,.102f,.457f,mwW,false);
 m.pipes.push_back({{20.55f,4.12f},{20.55f,4.12f},f+1.6f,.02f,roof,6});
 mwFix(m,model::med_records_cabinet,15.35f,5.555f,f,.9f,.49f,1.f,mwN,true);
 for(float x:{15.2f,15.34f})kit::prop(m,model::office_file_binder,x,5.56f,f+1.f,.09f,.3f,.42f,mwS);
 mwFix(m,model::med_notice_board,17.6f,5.765f,f+1.25f,1.2f,.07f,.9f,mwN,false);
 mwFix(m,model::med_wall_clock,16.4f,5.78f,f+2.05f,.24f,.02f,.23f,mwN,false);
 // --- Seclusion 1 and 2: padded cells 4.5 by 3 m, a mattress each, a patient each ----------------
 // Behind them the walls are solid: the risers and the ward's plant.
 wall(m,1.f,6.f,6.8f,7.4f,f,roof,3);wall(m,1.f,7.4f,2.2f,10.4f,f,roof,3);
 wall(m,1.f,15.8f,6.8f,17.4f,f,roof,3);wall(m,1.f,12.6f,2.2f,15.8f,f,roof,3);
 for(auto [y1,y2]:{std::pair{7.4f,10.4f},{12.6f,15.8f}}){
  mwPad(m,2.2f,y1,2.3f,y2,f);mwPad(m,6.7f,y1,6.8f,y2,f);
  if(y1<8)mwPad(m,2.3f,y1,6.7f,y1+.1f,f);else mwPad(m,2.3f,y2-.1f,6.7f,y2,f);
  // and the door wall, round its door and under its panel
  const float a=y1<8?y2-.1f:y1,b=a+.1f;
  for(auto [x1,x2]:{std::pair{2.3f,2.8f},{4.1f,4.28f},{5.42f,6.7f}})mwPad(m,x1,a,x2,b,f);
  wall(m,2.8f,a,4.1f,b,f,f+1.07f,39);}
 kit::prop(m,model::med_mattress_stained,3.75f,8.25f,f,1.9f,.9f,.118f,.05f);
 kit::prop(m,model::med_mattress_stained,4.3f,14.5f,f,1.9f,.9f,.118f,-.12f);
 mwFix(m,model::med_floor_drain,5.75f,8.3f,f,.5f,.5f,.01f,0,false);mwFix(m,model::med_floor_drain,6.f,15.2f,f,.5f,.5f,.01f,0,false);
 for(auto [y1,y2]:{std::pair{14.45f,15.15f},{13.15f,13.6f}})wall(m,2.3f,y1,2.32f,y2,f+.6f,f+1.9f,16);             // smeared on the padding
 m.creatureSpawns.push_back({CreatureKind::Transformed,{3.45f,9.65f},-999,1,0,stateId("medw_wake_n")});        // at his panel
 m.creatureSpawns.push_back({CreatureKind::Transformed,{2.7f,14.85f},-999,1,0,dark});                          // facing the wall
 // The observation ante: the chair, the sheets on the panels.
 kit::prop(m,model::wooden_chair,1.55f,11.5f,f,.417f,.525f,.95f,mwW+.15f);
 taped(m,{2.6f,10.6f},f+1.45f,0,Readable::Note,"SECLUSION 1 / 10 MIN","03:40 STANDING. 03:50 STANDING. 04:00 STANDING AT THE PANEL.","04:13 HAS NOT MOVED SINCE THE LIGHTS CAME BACK UP. NEITHER WILL I.");
 kit::prop(m,model::trash_can,6.4f,10.95f,f,.27f,.27f,.34f,.3f);
 // --- The bathroom: two stalls, the basins, the shower ---------------------------------------------
 wall(m,1.f,17.6f,6.8f,17.7f,f,f+2.1f,27);wall(m,1.f,17.6f,1.1f,23.9f,f,f+2.1f,27);wall(m,1.f,23.8f,6.8f,23.9f,f,f+2.1f,27);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,17.6f,6.8f,17.7f},{1.f,17.6f,1.1f,23.9f},{1.f,23.8f,6.8f,23.9f}})mwCap(m,x1,y1,x2,y2,f+2.1f,roof);   // no ledge over the tiling
 wall(m,6.7f,17.7f,6.8f,20.24f,f,f+2.1f,27);wall(m,6.7f,21.56f,6.8f,23.8f,f,f+2.1f,27);
 for(float y:{18.8f,21.f}){mwFix(m,model::med_washroom_stall,1.87f,y,f,2.13f,1.54f,2.4f,mwE,true);mwStallFront(m,2.642f,y,true,1,f,y>20);}   // the far one bolted from inside, nobody in it
 for(float x:{3.7f,4.6f,5.5f}){mwFix(m,model::med_scrub_sink,x,17.97f,f,.49f,.54f,1.13f,mwS,true);}   // no mirrors on this ward: nothing to break, nothing to look into
 mwTrack(m,{4.5f,22.f},{6.7f,22.f},f,roof);mwCurtain(m,4.55f,5.5f,22.f,true,f);mwBunch(m,5.58f,22.f,f,true);
 m.pipes.push_back({{6.62f,22.9f},{6.62f,22.9f},f+1.f,.015f,f+2.1f,5});m.pipes.push_back({{6.62f,22.9f},{6.4f,22.9f},f+2.1f,.012f,f+2.1f,5});
 wall(m,6.3f,22.84f,6.42f,22.96f,f+2.f,f+2.1f,20);
 for(auto [x,y]:{std::pair{5.6f,23.f},{4.2f,20.f}})mwFix(m,model::med_floor_drain,x,y,f,.5f,.5f,.01f,0,false);
 kit::prop(m,model::wooden_chair,5.9f,22.7f,f,.417f,.525f,.95f,2.4f);
 for(auto [x,y,w,d]:{std::tuple{4.1f,19.1f,.6f,.45f},{3.3f,21.4f,.5f,.3f}})wall(m,x,y,x+w,y+d,f,f+.03f,23);    // towels on the floor
 mwBlood(m,5.1f,22.5f,5.7f,23.3f,f);
 // The stalls' plumbing boxed in behind the far one, tiled with the room, its access panel screwed
 // shut; by it the towel skip, full, and the bin; the bathroom's second lamp over them.
 mwCap(m,1.1f,17.7f,2.66f,22.08f,f+2.42f,roof);                                                        // nobody perches on the stall tops
 wall(m,1.1f,22.08f,2.66f,23.8f,f,f+2.1f,27);wall(m,1.1f,22.08f,2.66f,23.8f,f+2.1f,roof,3);
 wall(m,2.66f,22.65f,2.68f,23.35f,f+.3f,f+1.f,20);for(float y:{22.7f,23.27f})for(float z:{.36f,.92f})wall(m,2.68f,y,2.685f,y+.03f,f+z,f+z+.03f,8);
 {const float x=3.3f,y=23.2f;wall(m,x-.3f,y-.3f,x+.3f,y+.3f,f+.12f,f+.16f,20);
  for(float dx:{-.28f,.25f})for(float dy:{-.28f,.25f})wall(m,x+dx,y+dy,x+dx+.03f,y+dy+.03f,f+.06f,f+.9f,20);
  wall(m,x-.27f,y-.27f,x+.27f,y+.27f,f+.16f,f+.84f,39);}
 kit::prop(m,model::trash_can,4.1f,23.45f,f,.45f,.45f,.56f,.2f);
 m.lights.push_back(mwOut(lamp({3.7f,23.8f},f+2.05f,.42f,4.5f,5800,.08f,WorldLightMount::Wall,-kPi*.5f)));
 // --- The ward: eleven bays here, the aisle the gurneys came down --------------------------------
 const MwBay west[]={{6.2f,8.65f,0,0,1,true,true,true},{8.65f,11.f,1,1,0,true,false,false},{12.4f,15.05f,2,0,1,false,false,true},
  {15.05f,17.55f,1,2,1,true,true,false},{17.55f,20.1f,3,1,0,false,false,false},{21.7f,24.f,0,0,1,true,true,true}};
 for(const auto& b:west)mwBay(m,7.f,1,b,f,roof);
 const MwBay east[]={{12.4f,15.05f,1,1,0,true,true,false},{15.05f,17.55f,1,1,2,true,true,false},{17.55f,20.1f,5,0,0,false,false,false},{21.7f,24.f,4,3,1,true,false,true}};
 for(const auto& b:east)mwBay(m,17.f,-1,b,f,roof);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{15.6f,15.47f},-999,1,0,dark});                        // behind bay E2's drawn half
 // In front of the station window: the round trolley, the chart rack, a wheelchair.
 kit::prop(m,model::med_crash_cart,16.35f,7.1f,f,.74f,.48f,1.f,mwE);
 for(auto [x,y,yaw]:{std::tuple{16.25f,7.f,.3f},{16.45f,7.15f,1.5f}})kit::prop(m,model::med_pill_bottle,x,y,f+.809f,.07f,.03f,.13f,yaw);
 mwFix(m,model::med_records_cabinet,16.72f,9.4f,f,.9f,.49f,1.f,mwW,true);
 for(float y:{9.2f,9.32f,9.44f,9.56f})kit::prop(m,model::office_file_binder,16.72f,y,f+1.f,.09f,.3f,.42f,mwE);
 kit::prop(m,model::med_wheelchair,14.7f,8.4f,f,.55f,1.04f,.91f,2.7f);
 // The aisle: the ward table, the drug round abandoned, a gurney left on its way to theatre.
 mwFix(m,model::wooden_table,12.f,16.f,f,1.367f,.724f,.78f,kPi*.5f,true);
 mwFix(m,model::lab_potted_plant,12.05f,15.6f,f+.78f,.3f,.35f,.56f,.4f,false);
 kit::prop(m,model::mug,11.9f,16.4f,f+.78f,.09f,.09f,.1f,0);
 for(auto [x,y,yaw]:{std::tuple{11.f,15.4f,-kPi*.5f+.35f},{13.f,16.8f,kPi*.5f+.2f}})kit::prop(m,model::wooden_chair,x,y,f,.417f,.525f,.95f,yaw);
 kit::prop(m,model::med_crash_cart,11.2f,17.9f,f,.74f,.48f,1.f,.15f);
 for(auto [x,y,yaw]:{std::tuple{11.1f,17.85f,.3f},{11.3f,17.95f,1.6f}})kit::prop(m,model::med_pill_bottle,x,y,f+.809f,.07f,.03f,.13f,yaw);
 mwFix(m,model::med_ward_bed,13.f,22.4f,f,1.984f,.944f,.842f,kPi*.5f,true);                                  // the gurney, strapped
 mwStraps(m,22.4f-.87f,13.f,1.f,f,1,true);
 for(auto [x,y,w,d]:{std::tuple{9.9f,9.8f,.8f,.5f},{13.6f,12.9f,.6f,.7f},{14.f,19.7f,.9f,.4f}})wall(m,x,y,x+w,y+d,f,f+.03f,23);   // sheets
 kit::prop(m,model::item_bandage,10.2f,13.2f,f,.075f,.148f,.061f,.4f);
 paperDrift(m,9.9f,6.4f,14.1f,23.6f,f,8,7502);
 mwBlood(m,14.45f,17.f,14.9f,17.6f,f);
 {const float x=10.4f,y=11.7f;wall(m,x-.3f,y-.3f,x+.3f,y+.3f,f+.12f,f+.16f,20);
  for(float dx:{-.28f,.25f})for(float dy:{-.28f,.25f})wall(m,x+dx,y+dy,x+dx+.03f,y+dy+.03f,f+.06f,f+.9f,20);
  wall(m,x-.27f,y-.27f,x+.27f,y+.27f,f+.16f,f+.82f,23);}                                                    // linen skip, full
 kit::propPose(m,model::wooden_chair,13.6f,9.4f,f,2.2f,kPi*.5f,0);                                           // knocked over
 mwFix(m,model::med_wall_clock,8.8f,6.02f,f+2.1f,.24f,.02f,.23f,mwS,false);
 extinguisher(m,17.f,20.f,f,mwW);
 // --- The nurses' station: the window, the log, the light switches ---------------------------------
 for(float y:{7.6f,9.4f})kit::prop(m,model::office_chair,18.25f,y,f,.55f,.55f,.95f,mwE+(y-8.5f)*.15f);
 kit::prop(m,model::crt_monitor,17.5f,7.6f,f+.95f,.42f,.4f,.38f,mwE);
 kit::prop(m,model::keyboard,17.62f,8.06f,f+.95f,.45f,.16f,.03f,mwE);
 kit::prop(m,model::desk_phone,17.5f,10.15f,f+.95f,.2f,.2f,.1f,mwE);
 kit::prop(m,model::office_paper_stack,17.5f,8.7f,f+.95f,.33f,.44f,.03f,.1f);
 terminal(m,{17.5f,9.35f},"WARD C / OBS LOG / 15 MIN","03:15 ALL SETTLED. 03:30 ALL SETTLED. 03:45 BEDS 4 AND 7 STANDING. 04:00 ALL STANDING. ALL FACING THE TUBES.",
  "04:12 LIGHTS DIPPED NINE SECONDS. BED 12 GOT TO THE GUARD. LIGHTS UP: IT STOPPED WHERE IT WAS. IT IS STILL STANDING THERE.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.95f;m.terminals.back().yaw=mwE;
 for(float y:{6.75f,7.65f})mwFix(m,model::med_records_cabinet,22.755f,y,f,.9f,.49f,1.f,mwW,true);
 for(float y:{6.6f,6.72f,6.84f})kit::prop(m,model::office_file_binder,22.75f,y,f+1.f,.09f,.3f,.42f,mwE);
 mwFix(m,model::med_notice_board,21.f,6.035f,f+1.25f,1.2f,.07f,.9f,mwS,false);
 mwFix(m,model::pwr_breaker_panel_open,22.88f,10.2f,f+1.2f,.318f,.225f,.457f,mwW,false);                         // ward lighting
 m.pipes.push_back({{22.94f,10.2f},{22.94f,10.2f},f+1.66f,.02f,roof,6});
 taped(m,{23.f,10.85f},f+1.45f,mwW,Readable::Sticky,"WARD LIGHTS","NIGHT CIRCUIT FROM THE MAIN BOARD. NOT FROM HERE.","IF IT GOES, SURGERY HAS THE ONLY RESET.");
 kit::prop(m,model::med_crash_cart,20.6f,11.6f,f,.74f,.48f,1.f,-.2f);
 mwFix(m,model::wooden_table,20.4f,8.6f,f,1.367f,.724f,.78f,0,true);
 for(auto [x,y,yaw]:{std::tuple{20.f,7.85f,kPi},{20.9f,9.35f,.2f}})kit::prop(m,model::wooden_chair,x,y,f,.417f,.525f,.95f,yaw);
 for(auto [x,yaw]:{std::pair{19.95f,.1f},{20.85f,-.3f}})kit::prop(m,model::office_paper_stack,x,8.6f,f+.78f,.33f,.44f,.03f,yaw);
 for(float x:{20.4f,20.52f})kit::prop(m,model::office_file_binder,x,8.5f,f+.78f,.09f,.3f,.42f,mwS);
 kit::prop(m,model::mug,20.45f,8.85f,f+.78f,.09f,.09f,.1f,0);
 mwFix(m,model::med_notice_board,19.2f,12.365f,f+1.25f,1.2f,.07f,.9f,mwN,false);                            // the bed-state board
 mwFix(m,model::med_wall_clock,22.98f,8.6f,f+2.05f,.24f,.02f,.23f,mwW,false);
 // --- The drug room: the sedation that did nothing -------------------------------------------------
 shelf(m,{22.73f,14.2f},kPi*.5f,0);shelf(m,{22.73f,16.3f},kPi*.5f,0);
 for(float y:{13.6f,14.1f,14.6f,15.8f,16.3f,16.8f})kit::prop(m,model::prop_cardboard_box,22.73f,y,f+.306f,.436f,.432f,.22f,mwE);
 for(float y:{13.5f,13.8f,15.9f})kit::prop(m,model::med_pill_bottle,22.7f,y,f+.972f,.07f,.03f,.13f,y);
 for(float y:{14.5f,14.8f,16.6f})kit::prop(m,model::med_specimen_jar,22.73f,y,f+.972f,.15f,.15f,.24f,y);
 for(float y:{13.8f,16.f})kit::prop(m,model::prop_cardboard_box,22.73f,y,f+1.656f,.436f,.432f,.22f,mwE);
 mwFix(m,model::med_supply_cabinet,19.2f,17.38f,f,1.1f,.444f,1.7f,mwN,true);
 mwFix(m,model::lab_sample_fridge,18.f,13.03f,f,.7f,.85f,1.f,mwS,true);
 wall(m,17.2f,14.f,17.78f,17.f,f,f+.86f,21);wall(m,17.2f,13.97f,17.82f,17.03f,f+.86f,f+.9f,20);              // dispensing counter
 for(auto [y,yaw]:{std::pair{14.3f,.2f},{14.45f,1.1f},{14.6f,2.3f},{15.9f,.7f},{16.1f,1.9f}})kit::prop(m,model::med_pill_bottle,17.5f,y,f+.9f,.07f,.03f,.13f,yaw);
 wall(m,17.3f,15.1f,17.7f,15.6f,f+.9f,f+.93f,20);                                                           // the round tray, emptied
 taped(m,{17.2f,15.4f},f+1.5f,mwE,Readable::Note,"SEDATION / WHOLE WARD","02:50 BEDS 3-11, 10 MG. 03:40 REPEAT. 04:05 REPEAT. NO EFFECT ON ANY OF THEM.",
  "PHARMACY WILL NOT SEND MORE WITHOUT I.I.'S NAME. I.I. SAYS THEY DO NOT NEED IT.");
 kit::prop(m,model::trash_can,20.6f,13.f,f,.27f,.27f,.34f,.2f);
 // --- The charge nurse's office: I.I.'s orders ----------------------------------------------------
 mwFix(m,model::desk,21.4f,20.9f,f,1.4f,.7f,.75f,mwE,true);
 kit::prop(m,model::office_chair,22.2f,20.95f,f,.55f,.55f,.95f,mwE+.2f);
 kit::prop(m,model::crt_monitor,21.4f,21.25f,f+.75f,.42f,.4f,.38f,mwE);
 kit::prop(m,model::desk_phone,21.3f,20.35f,f+.75f,.2f,.2f,.1f,mwE);
 const int officeLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,21.62f,20.55f,f+.75f,.18f,.4f,.52f,0);
 terminal(m,{21.35f,20.8f},"FROM I.I. / WARD C","NO TRANSFERS OUT EXCEPT TO THEATRE. NO VISITORS. NO RECORDS LEAVE THE WARD.",
  "OBS 6 IS MINE. NOBODY SPEAKS TO HER. SHE GOES LAST, AND I WILL BE THERE.");
 m.terminals.back().readable=Readable::Letter;m.terminals.back().surface=.75f;m.terminals.back().yaw=mwE;
 kit::prop(m,model::wooden_chair,20.15f,20.9f,f,.417f,.525f,.95f,-kPi*.5f+.2f);
 for(float x:{19.25f,20.16f})mwFix(m,model::med_records_cabinet,x,18.045f,f,.9f,.49f,1.f,mwS,true);   // the charge nurse's own files
 mwFix(m,model::office_bookcase,19.9f,23.64f,f,2.025f,.5f,2.5f,mwN,true);
 for(float y:{18.7f,19.62f})mwFix(m,model::med_records_cabinet,17.445f,y,f,.9f,.49f,1.f,mwE,true);   // the ward's files, under I.I.'s rule
 mwFix(m,model::med_wall_clock,22.98f,19.f,f+2.05f,.24f,.02f,.23f,mwW,false);
 mwFix(m,model::med_notice_board,17.235f,22.7f,f+1.25f,1.2f,.07f,.9f,mwE,false);
 kit::prop(m,model::office_file_binder,19.05f,18.045f,f+1.f,.09f,.3f,.42f,mwS);
 paperDrift(m,18.f,18.8f,22.5f,23.f,f,5,7503);
 // --- Light (27): cold ward tubes, the bed-heads, the station, and the reds that wait ------------------
 m.lights.push_back(mwTube(12.f,3.f,roof,.55f,6.f,5200,.05f));                                               // corridor
 m.lights.push_back(mwTube(5.4f,3.4f,roof,.5f,6.f,5000,.06f));                                               // admissions
 m.lights.push_back(mwTube(17.4f,3.4f,roof,.45f,5.5f,4800,.05f));                                            // booth
 for(auto [y,i,fl]:{std::tuple{7.5f,.6f,0.f},{10.5f,.6f,0.f},{13.5f,.55f,0.f},{16.5f,.55f,.3f},{22.5f,.55f,0.f}})m.lights.push_back(mwTube(12.f,y,roof,i,6.5f,5600,.06f,fl));
 m.lights.push_back(mwTube(12.f,19.5f,roof,.4f,6.f,5600,.06f,.75f));                                       // the failing one
 m.lights.push_back(mwTube(4.5f,8.9f,roof,.45f,4.5f,5200,.05f));                                               // seclusion 1
 m.lights.push_back(mwTube(3.9f,11.5f,roof,.35f,4.f,5000,.05f,.25f));                                        // the ante
 m.lights.push_back(mwTube(4.5f,14.2f,roof,.45f,4.5f,5200,.05f));                                               // seclusion 2
 m.lights.push_back(mwTube(3.9f,20.8f,roof,.45f,5.f,5800,.1f,.2f));                                        // bathroom
 m.lights.push_back(mwTube(20.1f,9.2f,roof,.6f,6.f,5000,.05f));                                              // station
 m.lights.push_back(mwTube(20.1f,15.1f,roof,.45f,5.f,5400,.04f));                                            // drug room
 m.lights.push_back(mwTube(19.6f,20.9f,roof,.4f,5.f,4200,.03f));                                             // office
 {auto l=lamp({21.62f,20.55f},f+1.2f,.3f,2.2f,3000,0,WorldLightMount::Bare);l.prop=officeLamp;m.lights.push_back(mwOut(l));}
 m.lights.push_back(mwRed(15.6f,6.02f,f+2.3f,kPi*.5f));                                                     // emergency: the doors
 m.lights.push_back(mwRed(16.98f,11.5f,f+2.62f,kPi));                                                       // over the station doorway
 m.lights.push_back(mwRed(7.02f,19.7f,f+2.2f,0));                                                           // the bathroom doorway
 // --- The dark: the whole half, flashlight and all ------------------------------------------------
 m.darkZones.push_back({0,0,24,24,-1000,1000,dark,1,false,true});
 // Whoever crosses the ward in the dark brings seclusion 1 off his panel.
 mwEvent(m,"medw_n_dark_walk",7.f,6.f,17.f,24.f,"med_blackout",1,{mwSet("medw_wake_n",1)});
 // Door control's promise kept: the seclusion maglocks dropped with the ward's power (76 sets
 // medw_maglocks_out as its tubes die), so both cells stand open to whoever comes back up
 // here, in the dark or after it. The patients need no help with a door that is not locked.
 {ScriptEvent e;e.id=stateId("medw_n_locks_drop");e.scope=ScriptEvent::Scope::Map;e.x2=24;e.y2=24;e.once=true;
  e.requireState=stateId("medw_maglocks_out");e.requireValue=1;
  for(int d:{seclusionDoor,seclusionDoor+1})e.actions.push_back(mwDo(ScriptAction::Type::OpenDoor,Sound::Exit,0,d));
  m.scriptEvents.push_back(std::move(e));}
 // Breathing behind the curtains, the drip in the bathroom.
 for(int t:{4000,21000,38000})cue(m,"medw_n_clock",t,Sound::WorkerDying,{15.6f,15.4f},"",.1f,f+1.5f);
 for(int t:{12000,47000})cue(m,"medw_n_clock",t,Sound::WorkerDying,{3.45f,9.6f},"",.08f,f+1.5f);
 for(int t=2500;t<60000;t+=7500)cue(m,"medw_n_clock",t,Sound::Metal1,{5.6f,23.f},"",.06f,f+.1f);
 // --- Rewards, names --------------------------------------------------------------------------------
 m.pickupSpawns={{{17.5f,16.5f},PickupKind::Health,f+.9f},{{17.5f,13.9f+1.f},PickupKind::Health,f+.9f},{{20.1f,10.9f},PickupKind::Ammo,f},{{3.8f,1.3f},PickupKind::Ammo,f+.9f}};
 m.pickupSpawns.push_back({{20.15f,3.85f},PickupKind::HandgunAmmo,f+.75f});   // 9 mm on the door-release desk: the escort's spare, left with the clerk
 m.clutterSpawns.insert(m.clutterSpawns.end(),{{2,{17.55f,13.4f},f+.9f},{2,{9.6f,13.2f},f},{0,{11.2f,8.f},f},{0,{13.3f,18.6f},f,1.f},{0,{5.5f,3.2f},f,2.f}});
 titleCard(m,"WARD C",10.f,0.f,14.f,5.6f);
 zone(m,"MEDICAL / WARD C / ADMISSIONS",1.f,1.f,9.8f,5.8f);
 zone(m,"MEDICAL / WARD C / DOOR CONTROL",14.2f,1.f,20.6f,5.8f);
 zone(m,"MEDICAL / WARD C / SECLUSION",1.f,6.f,6.8f,17.4f);
 zone(m,"MEDICAL / WARD C / BATHROOM",1.f,17.6f,6.8f,24.f);
 zone(m,"MEDICAL / WARD C / NURSES' STATION",17.2f,6.f,23.f,12.4f);
 zone(m,"MEDICAL / WARD C / DRUG ROOM",17.2f,12.6f,23.f,17.6f);
 zone(m,"MEDICAL / WARD C / CHARGE NURSE",17.2f,17.8f,23.f,24.f);
 zone(m,"MEDICAL / WARD C",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}

// MEDICAL / WARD C SOUTH (manifest 48). The ward's far half and its back doors: the treatment
// room and sluice, the service corridor from Lab 4, the night desk with the guard bed 12 got to
// (bed 12 still standing over him), OBS 5 locked on its patient, and OBS 6, LAB 4's: the door
// open, her straps and band cut, her chart. Step to her bed and the ward's power is taken: the
// tubes die, the maglocks drop, and the patients come. The only light is the red emergency
// circuit, the gun, and the cold surgical light in the theatre transfer bay east (77's emergency
// circuit, the way to the reset). South, the mortuary corridor runs down to the Morgue stair.
// Route: W seam (Lab 4) or N seam (75) -> the ward end -> OBS 6 (blackout) -> up the aisle ->
// THEATRES -> E seam; later, lit again, the ward end -> MORTUARY -> S seam.
std::shared_ptr<const Map> makeMedicalWardCSouth(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[76];m.definition=CampaignChunks[76];m.definition.ambient=.22f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 m.definition.playerStart={1.7f,17.f};m.definition.spawnYaw=0;m.definition.spawnHeight=-12;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',2.f,22.f,f,false,0});
 openEdge(m,{'W',15.f,19.f,f,false,0});
 openEdge(m,{'E',9.f,14.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 machineryClock(m,"medw_s_clock",60000);
 const StateId dark=stateId("med_blackout");
 // --- Plan (walls .2 m, painted). Faces at y 15/19 (W), 9/14 (E) and x 10/14 (S) meet the seams --------
 wall(m,1.f,0.f,6.8f,.1f,f,roof,3);wall(m,17.2f,0.f,23.f,.1f,f,roof,3);                                       // under 75's side rooms
 wall(m,6.8f,0.f,7.f,5.4f,f,roof,3);mwDoorwayY(m,5.4f,6.6f,6.8f,7.f,f,roof);wall(m,6.8f,6.6f,7.f,14.8f,f,roof,3);   // treatment
 wall(m,1.f,9.f,6.8f,9.2f,f,roof,3);
 wall(m,0.f,14.8f,3.f,15.f,f,roof,3);mwSwing(m,3.f,3.9f,14.9f,f,roof,-1);wall(m,3.9f,14.8f,7.f,15.f,f,roof,3);    // door 0: sluice
 wall(m,0.f,19.f,2.f,19.2f,f,roof,3);mwSwing(m,2.f,2.9f,19.1f,f,roof,1);wall(m,2.9f,19.f,6.1f,19.2f,f,roof,3);    // door 1: linen
 mwSwing(m,6.1f,7.3f,19.1f,f,roof,-1,1.f);                                                                     // door 2: OBS 6, open (a bed's width)
 wall(m,7.3f,19.f,7.6f,19.2f,f,roof,3);mwGlaze(m,7.6f,19.f,9.4f,19.2f,f,roof,1.f,2.f,true);wall(m,9.4f,19.f,10.f,19.2f,f,roof,3);
 wall(m,10.f,19.f,11.4f,19.2f,f,roof,3);wall(m,12.6f,19.f,14.f,19.2f,f,roof,3);                                 // the mortuary corridor's fire wall
 wall(m,9.8f,19.2f,10.f,23.f,f,roof,3);wall(m,14.f,19.2f,14.2f,23.f,f,roof,3);
 wall(m,14.f,19.f,14.5f,19.2f,f,roof,3);const int obs5Door=int(m.doors.size());mwSwing(m,14.5f,15.4f,19.1f,f,roof,-1,0,dark); // door 3: OBS 5, maglocked
 wall(m,15.4f,19.f,15.7f,19.2f,f,roof,3);mwGlaze(m,15.7f,19.f,16.8f,19.2f,f,roof,1.f,2.f,true);wall(m,16.8f,19.f,17.f,19.2f,f,roof,3);
 wall(m,5.8f,19.2f,6.f,23.f,f,roof,3);
 // Theatres sit behind wired glass; the way through is recovery, and recovery's theatre door is
 // a maglock (door control: they fail open on power loss). The ward has to go dark first.
 // Once dropped they never hold shut again: see medw_maglocks_hold below.
 wall(m,17.f,0.f,17.2f,7.75f,f,roof,3);mwDoorwayY(m,7.75f,8.75f,17.f,17.2f,f,roof);wall(m,17.f,8.75f,17.2f,10.4f,f,roof,3);
 wall(m,17.f,12.6f,17.2f,23.f,f,roof,3);
 mwGlaze(m,17.f,10.4f,17.2f,12.6f,f,roof,.95f,2.1f,true);                                                       // the theatre screen
 wall(m,17.2f,8.8f,20.f,9.f,f,roof,3);const int recoveryDoor=int(m.doors.size());mwSwing(m,20.f,21.2f,8.9f,f,roof,-1,0,dark);wall(m,21.2f,8.8f,23.f,9.f,f,roof,3); // door 4: recovery, maglocked
 wall(m,17.2f,14.f,19.6f,14.2f,f,roof,3);mwSwing(m,19.6f,20.5f,14.1f,f,roof,1);wall(m,20.5f,14.f,23.f,14.2f,f,roof,3); // door 5: staff room
 const int mortuaryDoor=int(m.doors.size());mwSwing(m,11.4f,12.6f,19.1f,f,roof,1,0,dark);                       // door 6: mortuary, maglocked
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,.1f,6.8f,9.f},{1.f,9.2f,6.8f,14.8f},{0.f,15.f,7.f,19.f},{1.f,19.2f,5.8f,23.f},{6.f,19.2f,9.8f,23.f},
  {7.f,0.f,17.f,19.f},{10.f,19.2f,14.f,24.f},{14.2f,19.2f,17.f,23.f},{17.2f,.1f,23.f,8.8f},{17.2f,9.f,24.f,14.f},{17.2f,14.2f,23.f,23.f}})ceilingGrid(m,x1,y1,x2,y2,roof);
 // --- The ward, its last seven bays --------------------------------------------------------------
 const MwBay west[]={{0.f,2.7f,1,0,1,true,true,false},{2.7f,5.2f,4,1,0,true,false,true},{6.8f,9.4f,0,0,1,true,true,false},{9.4f,12.f,1,2,1,true,true,false}};
 for(const auto& b:west)mwBay(m,7.f,1,b,f,roof);
 const MwBay east[]={{0.f,2.7f,2,0,1,true,false,false},{2.7f,5.2f,1,3,0,true,true,true},{5.2f,7.5f,0,1,1,true,true,false}};   // ends short of recovery's door
 for(const auto& b:east)mwBay(m,17.f,-1,b,f,roof);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{8.4f,9.84f},-999,1,0,stateId("medw_wake_c")});         // bed 4, behind its curtain
 mwFix(m,model::med_wall_clock,16.98f,9.7f,f+2.05f,.24f,.02f,.23f,mwW,false);
 extinguisher(m,7.f,13.6f,f,mwE);
 kit::prop(m,model::med_wheelchair,10.2f,3.4f,f,.55f,1.04f,.91f,.6f);
 for(auto [x,y,w,d]:{std::tuple{10.6f,7.4f,.9f,.5f},{13.3f,1.2f,.5f,.6f}})wall(m,x,y,x+w,y+d,f,f+.03f,23);
 paperDrift(m,9.9f,.3f,14.1f,13.8f,f,6,7600);
 for(float x:{11.55f,12.45f})floorLine(m,x,0.f,x+.04f,19.f,f,8);
 for(float y:{7.95f,8.5f})floorLine(m,12.5f,y,17.f,y+.04f,f,8);                                               // turned for recovery and theatre
 kit::prop(m,model::item_bandage,13.9f,9.6f,f,.075f,.148f,.061f,1.2f);
 // --- The ward end: the night desk, the guard, bed 12 -----------------------------------------------
 mwFix(m,model::med_nurse_desk,9.2f,15.05f,f,1.18f,.56f,.81f,kPi,true);
 kit::prop(m,model::office_chair,9.25f,15.85f,f,.55f,.55f,.95f,.25f);
 kit::prop(m,model::med_vitals_monitor,9.f,14.95f,f+.81f,.38f,.18f,.28f,mwS);
 kit::prop(m,model::desk_phone,8.8f,15.15f,f+.81f,.2f,.2f,.1f,mwS);
 const int deskLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,9.55f,15.f,f+.81f,.18f,.4f,.52f,kPi*.5f);
 terminal(m,{9.15f,15.15f},"NIGHT DESK / HANDOVER","OBS 6 KEEPS ASKING WHAT TIME IT IS UPSTAIRS. SHE SAYS THEY CAN HEAR US. SHE SAYS THEY ARE STILL IN THERE.",
  "SHE ASKED FOR HER LIGHTS OFF. SAID THEY WOULD BE KINDER IN THE DARK. TOLD HER TO SLEEP.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.81f;m.terminals.back().yaw=mwS;
 const char* clock="medw_s_clock";
 actor(m,clock,ActorVisual::Cleanup,1.7f,{{0,{12.5f,17.1f},f,2.2f,4,1},{1000,{12.5f,17.1f},f,2.2f,4,1}});
 mwBlood(m,11.7f,16.5f,13.1f,17.7f,f);mwBlood(m,13.2f,17.6f,13.7f,18.1f,f);
 mwFix(m,model::item_walkie,13.25f,16.4f,f,.11f,.38f,.05f,.9f,false);
 m.pickupSpawns.push_back({{12.95f,15.85f},PickupKind::HandgunAmmo,f});        // 9 mm off the guard's belt, by his radio
 m.creatureSpawns.push_back({CreatureKind::Transformed,{13.7f,16.95f},-999,1,0,stateId("medw_wake_b")});        // bed 12, where it stopped
 kit::prop(m,model::med_crash_cart,16.1f,14.9f,f,.74f,.48f,1.f,mwW);
 for(auto [x,y,yaw]:{std::tuple{16.f,14.8f,.2f},{16.2f,15.f,1.3f}})kit::prop(m,model::med_pill_bottle,x,y,f+.809f,.07f,.03f,.13f,yaw);
 mwFix(m,model::med_notice_board,16.965f,16.6f,f+1.25f,1.2f,.07f,.9f,mwW,false);
 // Her way out: bare feet, from her door to the mortuary corridor and down it.
 for(int i=0;i<12;++i){const float t=float(i)/11.f;const float x=6.9f+t*(12.f-6.9f),y=18.3f+t*.55f+((i%2)?.1f:-.1f);
  wall(m,x-.1f,y-.05f,x+.1f,y+.05f,f+.004f,f+.007f,16);}
 for(float y=19.6f;y<23.9f;y+=.62f){wall(m,11.82f,y,11.92f,y+.2f,f+.004f,f+.007f,16);wall(m,12.1f,y+.31f,12.2f,y+.51f,f+.004f,f+.007f,16);}
 paperDrift(m,10.2f,14.4f,16.f,18.6f,f,5,7601);
 // --- OBS 6: LAB 4's room ---------------------------------------------------------------------------
 mwFix(m,model::med_ward_bed,7.11f,21.5f,f,1.984f,.944f,.842f,kPi,true);
 mwStraps(m,6.12f,21.5f,1,f,2);
 mwShelf(m,6.f,21.45f,1,f,true);
 wall(m,6.f,20.7f,6.07f,22.3f,f+1.2f,f+1.36f,20);
 for(auto [x1,x2]:{std::pair{6.9f,7.02f},{7.07f,7.2f}})wall(m,x1,21.24f,x2,21.28f,f+mwDeck+.02f,f+mwDeck+.03f,18);   // her amber band, cut through
 wall(m,6.93f,21.25f,6.99f,21.27f,f+mwDeck+.03f,f+mwDeck+.034f,21);                                         // its name strip
 kit::prop(m,model::item_dosimeter,7.95f,20.05f,f,.31f,.146f,.066f,.4f);
 kit::prop(m,model::item_id_card,8.9f,20.6f,f,.132f,.085f,.002f,1.1f);
 kit::prop(m,model::med_blood_bag,9.45f,21.75f,f,.11f,.02f,.14f,.6f);
 for(auto [x,y]:{std::pair{8.25f,20.9f},{8.4f,20.4f},{8.1f,19.9f}})wall(m,x-.03f,y-.03f,x+.03f,y+.03f,f+.004f,f+.007f,16);
 terminal(m,{7.08f,21.66f},"PATIENT: LAB 4 / OBS 6","SUBJECT LEFT WARD UNASSISTED.","DOORS OPENED FROM INSIDE.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=mwDeck+.02f;m.terminals.back().yaw=mwW;
 // The drip, pulled over on her way out; the lock box by the door, its cover off.
 wall(m,8.6f,22.35f,8.96f,22.71f,f,f+.03f,20);m.pipes.push_back({{8.78f,22.53f},{9.6f,21.9f},f+.04f,.012f,f+.04f,5});
 wall(m,7.32f,19.2f,7.5f,19.26f,f+1.f,f+1.22f,20);wall(m,7.36f,19.26f,7.46f,19.27f,f+1.04f,f+1.18f,8);
 m.pipes.push_back({{7.41f,19.27f},{7.41f,19.27f},f+.9f,.004f,f+1.04f,4});
 kit::prop(m,model::office_paper_sheet,8.6f,19.75f,f,.4f,1.f);
 kit::prop(m,model::wooden_chair,9.35f,22.4f,f,.417f,.525f,.95f,kPi*.5f-.3f);
 m.lights.push_back(mwBedLight(6.85f,21.5f,roof,.3f,.4f));
 // Step up to her bed (to read her chart) and the checkpoint is taken; the tubes buzz, and
 // while she is still being read about the ward's power goes. Every way on (recovery to
 // theatre, the mortuary) is a maglock that only the dark lets go, so this is the one trigger.
 const auto openDoor=[](int door){return mwDo(ScriptAction::Type::OpenDoor,Sound::Exit,0,door);};
 const std::vector<ScriptAction> dying={mwDo(ScriptAction::Type::PlaySound,Sound::Machine,.95f),mwDo(ScriptAction::Type::Shake),
  mwSet("med_blackout",1),mwSet("medw_maglocks_out",1),mwSet("medw_dark_clock",1),openDoor(obs5Door),openDoor(recoveryDoor),openDoor(mortuaryDoor)};
 // The trigger is gated by its state alone, never by the fired-event latch. A once-event is
 // latched before its actions run and the checkpoint is its first action, so a latched trigger
 // would be saved as spent with medw_dark_done and medw_fail still 0: loading that save gave a
 // lit ward whose blackout could never start. Unlatched, the save holds a player standing in
 // the box with medw_dark_done 0, and the trigger simply fires again on load.
 mwEvent(m,"medw_blackout",6.1f,20.5f,9.7f,22.9f,"medw_dark_done",0,{mwDo(ScriptAction::Type::Checkpoint),mwSet("medw_dark_done",1),mwSet("medw_fail",1)});
 m.scriptEvents.back().once=false;
 mwSequence(m,"medw_fail","medw_fail_done",4000,2600);
 cue(m,"medw_fail",900,Sound::Machine,{12.f,16.6f},"",.35f,roof-.3f);
 cue(m,"medw_fail",1900,Sound::Machine,{12.f,10.5f},"",.5f,roof-.3f);
 mwEvent(m,"medw_blackout_fall",0.f,0.f,24.f,24.f,"medw_fail_done",1,dying);
 // The maglocks wait on med_blackout, which Surgery's transfer puts back to 0: a door somebody
 // shut in the dark would lock again for good, with Surgery or the Morgue behind it. So a lock
 // that has dropped once (medw_maglocks_out, never cleared) is held open whenever the ward has
 // power: its leaf is driven back and, being locked, cannot be shut again. In the dark the
 // doors work by hand, for the player and for the patients.
 {ScriptEvent e;e.id=stateId("medw_maglocks_hold");e.scope=ScriptEvent::Scope::Map;e.x2=24;e.y2=24;e.once=false;
  e.requireState=stateId("medw_maglocks_out");e.requireValue=1;e.conditions.push_back({ScriptCondition::Source::State,dark,ScriptCondition::Op::Equal,0});
  e.actions={openDoor(obs5Door),openDoor(recoveryDoor),openDoor(mortuaryDoor)};m.scriptEvents.push_back(std::move(e));}
 // --- OBS 5: maglocked on its patient -----------------------------------------------------------
 mwPad(m,14.2f,19.2f,14.3f,23.f,f);mwPad(m,16.9f,19.2f,17.f,23.f,f);mwPad(m,14.3f,22.9f,16.9f,23.f,f);
 kit::prop(m,model::med_mattress_stained,15.6f,22.2f,f,1.9f,.9f,.118f,.04f);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{16.1f,20.f},-999,1,0,dark});
 taped(m,{14.25f,19.f},f+1.5f,kPi,Readable::Note,"OBS 5","04:00 STANDING AT THE GLASS. 04:15 STANDING AT THE GLASS.","WATCHING OBS 6'S DOOR. DO NOT OPEN.");
 // --- The mortuary corridor: the trolley that came back, the drain ---------------------------------
 wallSign(m,12.f,18.945f,f+2.32f,"MORTUARY","STAIR DOWN / COLD ROOMS",2.f,kPi);
 mwFix(m,model::med_ward_bed,13.32f,21.6f,f,1.984f,.944f,.842f,mwW,true);
 mwStraps(m,21.6f-.87f,13.32f,1.f,f,1,true);
 mwFix(m,model::med_floor_drain,11.4f,21.f,f,.5f,.5f,.01f,0,false);
 m.pipes.push_back({{10.05f,19.4f},{10.05f,23.f},f+2.6f,.03f,-999,0});
 // --- The theatre transfer bay: a patient waiting in the only light left ----------------------------
 wallSign(m,16.945f,11.5f,f+2.32f,"THEATRES","TRANSFER / SCRUBS ONLY PAST THIS LINE",1.9f,-kPi*.5f);
 floorLine(m,17.3f,9.05f,17.42f,13.95f,f,13);
 mwFix(m,model::med_ward_bed,21.85f,12.95f,f,1.984f,.944f,.842f,0,true);
 mwStraps(m,22.84f,12.95f,-1.f,f,1);
 mwDrip(m,23.f-.3f,12.15f,f,true);
 m.creatureSpawns.push_back({CreatureKind::Transformed,{21.4f,11.6f},-999,1,0,stateId("medw_wake_d")});
 for(float x:{18.f,18.4f})mwFix(m,model::gas_cylinder,x,9.2f,f,.33f,.33f,1.58f,0,true);
 wall(m,17.8f,9.f,18.6f,9.04f,f+1.08f,f+1.13f,2);wall(m,17.8f,9.f,18.6f,9.4f,f+1.58f,roof,30);   // nobody stands on the bottles
 mwFix(m,model::med_notice_board,18.6f,13.965f,f+1.25f,1.2f,.07f,.9f,mwN,false);
 taped(m,{19.35f,14.f},f+1.45f,kPi,Readable::Note,"THEATRE LIST / PROCEDURE 14","04:00 BED 4. 04:30 BED 9. 05:00 BED 12.",
  "05:30 OBS 6, LAB 4. I.I. TO ATTEND IN PERSON.");
 kit::prop(m,model::desk_phone,22.68f,9.25f,f+1.f,.2f,.2f,.1f,mwS);
 mwFix(m,model::med_scrub_sink,19.1f,9.27f,f,.49f,.54f,1.13f,mwS,true);
 kit::prop(m,model::med_wheelchair,18.2f,13.3f,f,.55f,1.04f,.91f,kPi*.5f+.3f);
 {const float x=20.f,y=12.95f;wall(m,x-.3f,y-.3f,x+.3f,y+.3f,f+.12f,f+.16f,20);
  for(float dx:{-.28f,.25f})for(float dy:{-.28f,.25f})wall(m,x+dx,y+dy,x+dx+.03f,y+dy+.03f,f+.06f,f+.9f,20);
  wall(m,x-.27f,y-.27f,x+.27f,y+.27f,f+.16f,f+.7f,23);}                                                      // theatre linen skip
 mwFix(m,model::med_wall_clock,22.f,9.02f,f+2.05f,.24f,.02f,.23f,mwS,false);wall(m,22.4f,9.f,22.98f,9.5f,f+.96f,f+1.f,20);
 // --- Recovery: two beds back from theatre --------------------------------------------------------
 for(float y:{2.4f,6.f}){mwFix(m,model::med_ward_bed,21.9f,y,f,1.984f,.944f,.842f,0,true);mwShelf(m,23.f,y-.05f,-1,f,y<3,y<3);   // R1's monitor dark: no recovery
  wall(m,22.93f,y-.8f,23.f,y+.8f,f+1.2f,f+1.36f,20);mwDrip(m,22.7f,y-.75f,f,y>3);}
 mwStraps(m,22.88f,2.4f,-1.f,f,1);mwStraps(m,22.88f,6.f,-1.f,f,0);
 mwBlood(m,21.25f,5.6f,21.75f,5.95f,f+mwDeck);mwBlood(m,20.4f,6.6f,21.f,7.3f,f);
 mwFix(m,model::med_nurse_desk,18.7f,4.2f,f,1.18f,.56f,.81f,kPi*.5f,true);
 kit::prop(m,model::office_chair,18.f,4.25f,f,.55f,.55f,.95f,mwW+.15f);
 kit::prop(m,model::crt_monitor,18.75f,4.5f,f+.81f,.42f,.4f,.38f,mwW);
 taped(m,{17.2f,3.f},f+1.5f,mwE,Readable::Note,"RECOVERY","R1: PROCEDURE 14, 03:10. NO RECOVERY. TO MORTUARY 03:55.",
  "R2: PROCEDURE 14, 03:40. AWAKE THROUGHOUT. BACK TO THE WARD, BED 4.");
 kit::prop(m,model::med_crash_cart,19.1f,6.6f,f,.74f,.48f,1.f,.3f);
 {const float x=20.5f,y=4.2f;wall(m,x-.25f,y-.25f,x+.25f,y+.25f,f,f+.04f,20);m.pipes.push_back({{x,y},{x,y},f+.04f,.02f,f+1.1f,5});
  wall(m,x-.22f,y-.14f,x+.22f,y+.14f,f+1.1f,f+1.14f,20);wall(m,x-.25f,y-.25f,x+.25f,y+.25f,f+.04f,f+1.1f,30);
  kit::prop(m,model::med_vitals_monitor,x,y,f+1.14f,.38f,.18f,.28f,mwE);}                                        // the recovery monitor on its stand
 kit::prop(m,model::med_wheelchair,18.f,1.6f,f,.55f,1.04f,.91f,.4f);
 mwFix(m,model::med_supply_cabinet,19.6f,.322f,f,1.1f,.444f,1.7f,mwS,true);
 for(float x:{17.6f,18.f})mwFix(m,model::gas_cylinder,x,.4f,f,.33f,.33f,1.58f,0,true);
 wall(m,17.4f,.1f,18.2f,.14f,f+1.08f,f+1.13f,2);wall(m,17.4f,.1f,18.2f,.6f,f+1.58f,roof,30);
 for(auto [x,y,yaw]:{std::tuple{20.6f,3.3f,.4f},{20.2f,5.1f,1.9f}})kit::prop(m,model::med_blood_bag,x,y,f,.11f,.02f,.14f,yaw);
 for(auto [x,y,yaw]:{std::tuple{19.9f,2.f,.7f},{21.9f,7.45f,2.f},{19.6f,6.3f,1.1f}})kit::prop(m,model::item_bandage,x,y,f,.075f,.148f,.061f,yaw);
 mwFix(m,model::med_wall_clock,17.22f,6.5f,f+2.05f,.24f,.02f,.23f,mwE,false);
 // --- The staff room: the night shift's break ------------------------------------------------------
 wall(m,22.38f,15.f,23.f,18.4f,f,f+.86f,21);wall(m,22.34f,15.f,23.f,18.4f,f+.86f,f+.9f,20);
 kit::prop(m,model::coffee_machine,22.72f,15.5f,f+.9f,.4f,.45f,.5f,mwW);
 for(auto [y,yaw]:{std::pair{16.1f,.3f},{16.3f,1.1f},{16.55f,2.f}})kit::prop(m,model::mug,22.65f,y,f+.9f,.09f,.09f,.1f,yaw);
 wall(m,22.45f,17.f,22.95f,17.8f,f+.82f,f+.9f,8);
 mwFix(m,model::wooden_table,19.8f,17.9f,f,1.367f,.724f,.78f,0,true);
 for(auto [x,y,yaw]:{std::tuple{19.45f,17.15f,kPi},{20.3f,18.7f,.25f},{18.8f,18.1f,-kPi*.5f+.3f}})kit::prop(m,model::wooden_chair,x,y,f,.417f,.525f,.95f,yaw);
 for(auto [x,y]:{std::pair{19.5f,17.8f},{20.2f,18.05f}})kit::prop(m,model::mug,x,y,f+.78f,.09f,.09f,.1f,x);
 kit::prop(m,model::office_paper_stack,19.9f,17.85f,f+.78f,.33f,.44f,.03f,.5f);
 mwFix(m,model::office_couch_green,17.9f,21.2f,f,2.242f,1.364f,1.192f,mwE,true);
 for(float y:{20.2f,21.1f,22.f})mwFix(m,model::psx_locker,22.68f,y,f,.64f,.88f,2.25f,kPi,true);
 mwFix(m,model::lab_sample_fridge,20.f,22.57f,f,.7f,.85f,1.f,mwN,true);
 mwFix(m,model::med_notice_board,17.235f,16.3f,f+1.25f,1.2f,.07f,.9f,mwE,false);
 mwFix(m,model::med_wall_clock,21.2f,14.22f,f+2.05f,.24f,.02f,.23f,mwS,false);
 wall(m,21.1f,22.6f,21.5f,23.f,f+1.98f,f+2.02f,20);wall(m,21.28f,22.8f,21.32f,23.f,f+2.02f,f+2.3f,20);
 mwFix(m,model::cctv_tv,21.3f,22.75f,f+1.56f,.5f,.4f,.42f,mwN,false);
 // --- Treatment: where the sedation was drawn up --------------------------------------------------
 mwFix(m,model::med_ward_bed,3.2f,1.22f,f,1.984f,.944f,.842f,kPi*.5f,true);
 mwShelfX(m,3.2f,.1f,1,f,true);
 for(float y:{2.3f,3.5f})mwFix(m,model::med_supply_cabinet,1.222f,y,f,1.1f,.444f,1.7f,mwE,true);
 mwFix(m,model::med_scrub_sink,5.6f,.37f,f,.49f,.54f,1.13f,mwS,true);
 kit::prop(m,model::med_crash_cart,5.4f,2.4f,f,.74f,.48f,1.f,-.3f);
 for(auto [x,y,yaw]:{std::tuple{5.3f,2.35f,.3f},{5.5f,2.5f,1.7f}})kit::prop(m,model::med_pill_bottle,x,y,f+.809f,.07f,.03f,.13f,yaw);
 kit::prop(m,model::item_bandage,5.62f,2.3f,f+.809f,.075f,.148f,.061f,.5f);
 wall(m,1.f,8.4f,4.6f,9.f,f,f+.86f,21);wall(m,1.f,8.36f,4.64f,9.f,f+.86f,f+.9f,20);
 for(auto [x,yaw]:{std::pair{1.8f,.4f},{2.f,1.6f},{2.15f,2.7f},{3.3f,.9f}})kit::prop(m,model::med_pill_bottle,x,8.7f,f+.9f,.07f,.03f,.13f,yaw);
 kit::prop(m,model::trash_can,5.1f,8.7f,f,.35f,.35f,.6f,.2f);
 for(float x:{1.4f,1.8f,2.2f}){wall(m,x,8.96f,x+.05f,9.f,f+1.6f,f+1.65f,20);wall(m,x+.005f,8.97f,x+.045f,8.99f,f+1.05f,f+1.6f,39);}  // canvas restraints on their hooks
 mwFix(m,model::lab_sample_fridge,1.45f,6.4f,f,.7f,.85f,1.f,mwE,true);                                    // the drug fridge, emptied
 kit::prop(m,model::prop_cardboard_box_open,1.45f,6.4f,f+1.f,.36f,.36f,.36f,.3f);
 mwCap(m,1.02f,6.05f,1.88f,6.75f,f+1.4f,roof);                                                           // nobody sits up on the fridge
 m.lights.push_back(mwOut(lamp({3.4f,9.f},f+1.95f,.38f,4.f,4600,.04f,WorldLightMount::Wall,-kPi*.5f)));   // the drawing-up light over the counter
 for(float x:{5.9f,6.3f})mwFix(m,model::gas_cylinder,x,8.75f,f,.33f,.33f,1.58f,0,true);
 mwFix(m,model::med_wall_clock,6.78f,3.2f,f+2.05f,.24f,.02f,.23f,mwW,false);
 kit::prop(m,model::wooden_chair,2.4f,4.6f,f,.417f,.525f,.95f,kPi*.8f);
 kit::prop(m,model::med_crash_cart,4.9f,6.9f,f,.74f,.48f,1.f,mwE+.2f);
 for(auto [x,y,yaw]:{std::tuple{4.85f,6.8f,.6f},{4.95f,7.f,2.f}})kit::prop(m,model::med_specimen_jar,x,y,f+.809f,.15f,.15f,.24f,yaw);
 mwFix(m,model::med_notice_board,1.035f,5.4f,f+1.25f,1.2f,.07f,.9f,mwE,false);
 mwFix(m,model::med_first_aid_box,4.f,.16f,f+1.45f,.37f,.12f,.35f,mwS,false);
 // --- The sluice ----------------------------------------------------------------------------------
 for(float y:{10.3f,10.9f})mwFix(m,model::med_scrub_sink,1.27f,y,f,.49f,.54f,1.13f,mwE,true);
 mwFix(m,model::med_ward_toilet,2.8f,9.56f,f,.4f,.7f,.82f,mwS,true);
 wall(m,4.f,9.2f,4.8f,9.8f,f,f+1.5f,20);wall(m,4.08f,9.8f,4.72f,9.81f,f+.3f,f+1.3f,8);wall(m,4.3f,9.81f,4.5f,9.84f,f+1.f,f+1.06f,20); // bedpan washer
 for(auto [x,y]:{std::pair{5.6f,12.9f},{4.5f,13.5f}}){wall(m,x-.3f,y-.3f,x+.3f,y+.3f,f+.12f,f+.16f,20);
  for(float dx:{-.28f,.25f})for(float dy:{-.28f,.25f})wall(m,x+dx,y+dy,x+dx+.03f,y+dy+.03f,f+.06f,f+.9f,20);
  wall(m,x-.27f,y-.27f,x+.27f,y+.27f,f+.16f,f+.82f,23);m.pipes.push_back({{x-.2f,y},{x+.2f,y},f+.04f,.04f,f+.04f,8});}  // laundry skips
 mwFix(m,model::med_floor_drain,3.4f,11.8f,f,.5f,.5f,.01f,0,false);
 shelf(m,{6.53f,11.4f},kPi*.5f,0);
 for(float y:{10.9f,11.9f})kit::prop(m,model::prop_cardboard_box,6.53f,y,f+.306f,.436f,.432f,.22f,mwE);
 for(float y:{10.7f,11.05f,11.7f})kit::prop(m,model::med_specimen_jar,6.53f,y,f+.972f,.15f,.15f,.24f,y);
 for(float y:{10.95f,11.85f})kit::prop(m,model::prop_cardboard_box,6.53f,y,f+1.656f,.436f,.432f,.22f,mwE);
 kit::prop(m,model::med_crash_cart,3.9f,11.f,f,.74f,.48f,1.f,.5f);
 kit::prop(m,model::trash_can,5.9f,9.7f,f,.27f,.27f,.34f,0);
 for(float y:{12.f,12.6f})kit::prop(m,model::trash_can,1.33f,y,f,.45f,.45f,.56f,y-12.f);
 mwBlood(m,2.6f,12.6f,3.4f,13.2f,f);
 for(auto [x,y,w,d]:{std::tuple{3.f,13.8f,.6f,.4f},{5.f,11.f,.5f,.6f}})wall(m,x,y,x+w,y+d,f,f+.03f,23);
 // --- The service corridor from Lab 4: the way they brought her ------------------------------------
 kit::prop(m,model::med_wheelchair,5.5f,15.55f,f,.55f,1.04f,.91f,kPi*.5f+.2f);
 extinguisher(m,4.6f,19.f,f,mwN);
 for(float y:{16.6f,17.4f})floorLine(m,0.f,y,7.f,y+.04f,f,8);
 mwFix(m,model::med_notice_board,1.6f,15.035f,f+1.25f,1.2f,.07f,.9f,mwS,false);
 titleCard(m,"WARD C",0.f,15.f,5.f,19.f);
 // --- Linen ---------------------------------------------------------------------------------------
 for(float y:{20.15f,22.f}){shelf(m,{1.27f,y},kPi*.5f,0);
  for(float z:{.306f,.972f,1.656f})for(float dy:{-.45f,.45f})if(int(z*10+dy*10+y)%3)kit::prop(m,model::prop_cardboard_box,1.27f,y+dy,f+z,.436f,.432f,.22f,mwE);}
 for(auto [x,y]:{std::pair{4.6f,21.9f}}){wall(m,x-.3f,y-.3f,x+.3f,y+.3f,f+.12f,f+.16f,20);
  for(float dx:{-.28f,.25f})for(float dy:{-.28f,.25f})wall(m,x+dx,y+dy,x+dx+.03f,y+dy+.03f,f+.06f,f+.9f,20);
  wall(m,x-.27f,y-.27f,x+.27f,y+.27f,f+.16f,f+.82f,23);}
 // The night's bags, tied off and left for a porter who did not come: slumped sacks of ward
 // fabric, each with its neck knotted. A rounded shape collides by its bounds and casts no shadow.
 for(auto [x,y,r,h]:{std::tuple{4.95f,20.f,.36f,.5f},{5.2f,20.72f,.34f,.44f},{4.3f,20.25f,.3f,.38f},{4.55f,20.85f,.27f,.32f}}){
  wall(m,x-r,y-r*.92f,x+r,y+r*.92f,f,f+h,39);m.structures.back().shape=ShapeDome;
  wall(m,x-.035f,y-.03f,x+.035f,y+.03f,f+h-.02f,f+h+.07f,39);m.structures.back().solid=false;                 // the knot
  wall(m,x+.03f,y-.02f,x+.13f,y+.02f,f+h-.03f,f+h+.01f,39);m.structures.back().solid=false;}                  // and its tail
 kit::prop(m,model::med_crash_cart,3.25f,22.35f,f,.74f,.48f,1.f,.1f);                                          // the linen round's cart
 // The chute down to the laundry in the east wall, its hatch shut, and the round's sheet on the
 // board by the door.
 wall(m,5.76f,21.15f,5.8f,21.95f,f+.75f,f+1.55f,20);wall(m,5.74f,21.22f,5.76f,21.88f,f+.82f,f+1.48f,28);
 wall(m,5.7f,21.45f,5.74f,21.65f,f+1.36f,f+1.42f,8);
 mwFix(m,model::med_notice_board,4.3f,19.235f,f+1.25f,1.2f,.07f,.9f,mwS,false);
 kit::prop(m,model::prop_cardboard_box,3.25f,22.35f,f+.809f,.436f,.432f,.22f,.12f);
 // --- Light (28): the bays, the end, the rooms; the two reds; theatre's own cold light --------------
 for(auto [y,i,fl]:{std::tuple{1.5f,.55f,0.f},{4.5f,.55f,0.f},{7.5f,.5f,.25f},{10.5f,.55f,0.f}})m.lights.push_back(mwTube(12.f,y,roof,i,6.5f,5600,.06f,fl));
 m.lights.push_back(mwTube(12.f,16.6f,roof,.55f,6.5f,5400,.06f));                                            // the ward end
 {auto l=lamp({9.55f,15.f},f+1.25f,.3f,2.2f,3000,0,WorldLightMount::Bare);l.prop=deskLamp;m.lights.push_back(mwOut(l));}
 m.lights.push_back(mwTube(3.9f,4.5f,roof,.55f,5.5f,5600,.04f));                                             // treatment
 m.lights.push_back(mwTube(3.9f,12.f,roof,.4f,5.f,5400,.28f,.2f));                                           // sluice
 m.lights.push_back(mwTube(3.4f,17.f,roof,.45f,5.5f,5000,.05f));                                             // service corridor
 m.lights.push_back(mwTube(3.4f,21.1f,roof,.5f,5.f,4800,.05f));                                              // linen
 m.lights.push_back(mwTube(8.f,20.4f,roof,.35f,4.f,5200,.05f));                                              // OBS 6
 m.lights.push_back(mwTube(15.6f,21.1f,roof,.3f,4.f,5200,.05f));                                             // OBS 5
 m.lights.push_back(mwTube(12.f,21.5f,roof,.4f,5.f,6000,.08f,.3f));                                          // mortuary corridor
 m.lights.push_back(mwTube(20.1f,16.6f,roof,.5f,5.5f,3300,0));m.lights.push_back(mwTube(20.1f,20.8f,roof,.45f,5.f,3300,0)); // staff room
 m.lights.push_back(mwTube(20.1f,2.6f,roof,.5f,5.5f,5400,.05f));m.lights.push_back(mwTube(20.1f,6.4f,roof,.45f,5.f,5400,.05f));  // recovery
 {auto l=lamp({20.6f,11.5f},roof-.3f,.6f,8.f,6800,.04f);l.flicker=.12f;l.fitting=model::clinic_ceiling_lamp_1;l.fittingWidth=1.05f;l.fittingDepth=1.04f;l.fittingHeight=.16f;
  m.lights.push_back(l);}                                                                                    // theatre circuit: never dies
 m.lights.push_back(mwRed(9.6f,18.98f,f+2.2f,-kPi*.5f));                                                    // emergency: her door
 m.lights.push_back(mwRed(16.98f,8.25f,f+2.62f,kPi));                                                       // over recovery's door: the way to theatres
 // --- The dark ------------------------------------------------------------------------------------
 m.darkZones.push_back({0,0,24,24,-1000,1000,dark,1,false,true});
 // After the power goes: a tube pops, OBS 5's lock lets go, the walkie, then bed 12 turns.
 mwSequence(m,"medw_dark_clock","medw_wake_b",60000,7000);
 cue(m,"medw_dark_clock",350,Sound::JunkGlass,{12.f,13.5f},"",.7f,roof-.3f);
 cue(m,"medw_dark_clock",1300,Sound::Door,{14.95f,19.1f},"",.9f,f+1.2f);
 cue(m,"medw_dark_clock",2600,Sound::RadioSquelch,{13.25f,16.4f},"",.8f,f+.1f);
 cue(m,"medw_dark_clock",3000,Sound::Empty,{13.25f,16.4f},"INVESTIGATOR (RADIO): Ward C is dark.",.9f,f+.1f,"medw_dark_1");
 cue(m,"medw_dark_clock",5300,Sound::RadioSquelch,{13.25f,16.4f},"",.8f,f+.1f);
 cue(m,"medw_dark_clock",5700,Sound::Empty,{13.25f,16.4f},"INVESTIGATOR (RADIO): Leave it dark. They know what to do.",.9f,f+.1f,"medw_dark_2");
 cue(m,"medw_dark_clock",6900,Sound::WorkerDying,{13.7f,16.95f},"",.8f,f+1.4f);
 // Into the ward's south bays in the dark: bed 4 comes through its curtain. Into the transfer bay:
 // the one waiting for theatre turns round.
 mwEvent(m,"medw_s_dark_bays",7.f,0.f,17.f,13.6f,"med_blackout",1,{mwSet("medw_wake_c",1)});
 mwEvent(m,"medw_s_dark_bay",17.3f,9.f,24.f,14.f,"med_blackout",1,{mwSet("medw_wake_d",1)});
 for(int t:{6000,26000,44000})cue(m,clock,t,Sound::WorkerDying,{16.1f,20.f},"",.09f,f+1.5f);
 for(int t:{15000,52000})cue(m,clock,t,Sound::WorkerDying,{21.4f,11.6f},"",.08f,f+1.5f);
 for(int t=1500;t<60000;t+=6000)cue(m,clock,t,Sound::Metal1,{3.4f,11.8f},"",.05f,f+.1f);
 // --- Rewards, names --------------------------------------------------------------------------------
 m.pickupSpawns.insert(m.pickupSpawns.end(),{{{12.1f,17.9f},PickupKind::Ammo,f},{{19.6f,17.7f},PickupKind::Ammo,f+.78f},{{3.6f,8.7f},PickupKind::Health,f+.9f},
  {{22.6f,17.3f},PickupKind::Health,f+.9f},{{18.2f,3.8f},PickupKind::Ammo,f},
  {{22.1f,21.55f},PickupKind::HandgunAmmo,f}});                                   // 9 mm: the night guard's locker, its box dropped at the door
 m.clutterSpawns.insert(m.clutterSpawns.end(),{{2,{22.6f,15.9f},f+.9f},{2,{1.7f,8.65f},f+.9f,.5f},{0,{11.4f,5.2f},f},{0,{15.2f,16.8f},f,1.f},{0,{4.8f,17.6f},f,2.f}});
 zone(m,"MEDICAL / WARD C / TREATMENT",1.f,.1f,6.8f,9.f);
 zone(m,"MEDICAL / WARD C / SLUICE",1.f,9.2f,6.8f,14.8f);
 zone(m,"MEDICAL / WARD C / SERVICE CORRIDOR",0.f,15.f,6.4f,19.f);
 zone(m,"MEDICAL / WARD C / LINEN",1.f,19.2f,5.8f,23.f);
 zone(m,"MEDICAL / WARD C / OBS 6",6.f,19.2f,9.8f,23.f);
 zone(m,"MEDICAL / WARD C / OBS 5",14.2f,19.2f,17.f,23.f);
 zone(m,"MEDICAL / WARD C / MORTUARY CORRIDOR",10.f,19.2f,14.f,24.f);
 zone(m,"MEDICAL / WARD C / RECOVERY",17.2f,.1f,23.f,8.8f);
 zone(m,"MEDICAL / WARD C / THEATRE TRANSFER",17.2f,9.f,24.f,14.f);
 zone(m,"MEDICAL / WARD C / STAFF ROOM",17.2f,14.2f,23.f,23.f);
 zone(m,"MEDICAL / WARD C",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
}
void buildChapterMedicalWard(CampaignMapTable& maps){
 maps[75]=makeMedicalWardCNorth();
 maps[76]=makeMedicalWardCSouth();
}
}

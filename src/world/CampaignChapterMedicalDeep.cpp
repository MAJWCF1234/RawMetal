// DEPTHWORKS MEDICAL (chapter 2, manifest 48 (subchapter)): chunks 77, 78, the end of the deep
// wing. Surgery is the lit beacon of the Ward C blackout; the Morgue is the way on to
// Quarantine. See docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- MedicalDeep kit (dd) ----------------------------------------------------------------
constexpr float ddS=0,ddN=kPi,ddE=kPi*.5f,ddW=-kPi*.5f;
void ddFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
// A pack model at its own proportions (times s).
void ddModel(Map& m,int model,float x,float y,float z,float yaw,bool solid,float s=1){
 const auto& v=model::size[model];ddFix(m,model,x,y,z,v[0]*s,v[1]*s,v[2]*s,yaw,solid);
}
// A body from the pack, on his back: model +z is the feet, so at yaw 0 they point +y. The air
// hose loops under the back, so on a flat surface the base sits a little into it.
// He lies as the pack made him, boots and all: no sheet is built over him (boxes read as
// bricks, rounded shapes as helmets; the body reads best bare).
void ddCorpse(Map& m,float x,float y,float z,float yaw,float s=1){ddModel(m,model::med_corpse_supine,x,y,z,yaw,false,s);}
// A collision-only cap that casts no shadow: the renderer never draws material 30, the light
// pass takes only box shapes, and movement and the audit go by a structure's bounds.
void ddCap(Map& m,float x1,float y1,float x2,float y2,float lo,float hi){wall(m,x1,y1,x2,y2,lo,hi,30);m.structures.back().shape=ShapeColumn;}
// A tied-off sack slumped on the floor: half an ellipsoid with its knotted neck. A rounded
// shape collides by its bounds and casts no shadow.
void ddSack(Map& m,float x,float y,float f,float r,float h,int mat){
 wall(m,x-r,y-r*.92f,x+r,y+r*.92f,f,f+h,mat);m.structures.back().shape=ShapeDome;
 wall(m,x-.035f,y-.03f,x+.035f,y+.03f,f+h-.02f,f+h+.07f,mat);m.structures.back().solid=false;
 wall(m,x+.03f,y-.02f,x+.13f,y+.02f,f+h-.03f,f+h+.01f,mat);m.structures.back().solid=false;
}
// The shut door of a pack washroom stall, picked out on its flat front (the face at px,py,
// looking along x when faceX, toward dir): head rail, door edges, hinges, the gap under the
// leaf, pull and bolt plate with its indicator. Trim only, nothing collides.
void ddStallFront(Map& m,float px,float py,bool faceX,float dir,float f,bool engaged){
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
// Swinging doors and open doorways: kit::wallDoor and kit::doorwayY.
// Clinic glazing in a straight wall: painted dado, steel sill and mullions, wired glass to
// `head`, wall above, an invisible proxy over sill and counter so nobody perches on it.
void ddGlaze(Map& m,float x1,float y1,float x2,float y2,float f,float top,float sill,float head,bool holds,float counter=0,int forceBays=0){
 const bool alongX=x2-x1>y2-y1;
 wall(m,x1,y1,x2,y2,f,f+sill,3);
 if(alongX)wall(m,x1,y1-.03f,x2,y2+.03f,f+sill-.03f,f+sill+.01f,20);else wall(m,x1-.03f,y1,x2+.03f,y2,f+sill-.03f,f+sill+.01f,20);
 const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f;
 if(alongX)glazingPanes(m,x1,cy-.012f,x2,cy+.012f,f+sill+.01f,f+head,17,holds);else glazingPanes(m,cx-.012f,y1,cx+.012f,y2,f+sill+.01f,f+head,17,holds);
 const float len=alongX?x2-x1:y2-y1;const int bays=forceBays>0?forceBays:std::max(1,int(len/1.2f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::clamp(t-.025f,alongX?x1:y1,(alongX?x2:y2)-.05f);
  if(alongX)wall(m,t,y1+.02f,t+.05f,y2-.02f,f+sill+.01f,f+head,20);else wall(m,x1+.02f,t,x2-.02f,t+.05f,f+sill+.01f,f+head,20);}
 wall(m,x1,y1,x2,y2,f+head,f+head+.06f,20);if(top>f+head+.06f)wall(m,x1,y1,x2,y2,f+head+.06f,top,3);
 const float proxyTop=std::max(f+head,top);
 if(alongX)wall(m,x1,y1-.04f,x2,y2+.04f,f+sill+.01f,proxyTop,30);else wall(m,x1-.04f-counter,y1,x2+.04f,y2,f+sill+.01f,proxyTop,30);
 if(counter>0&&!alongX)wall(m,x1-counter,y1+.05f,x1,y2-.05f,f+sill-.04f,f+sill,20);
}
WorldLight ddTube(float x,float y,float mount,float intensity,float range,float kelvin,float tint,float flicker=0){
 auto l=lamp({x,y},mount-.30f,intensity,range,kelvin,tint);l.flicker=flicker;return l;
}
WorldLight ddSw(WorldLight l,const char* state,bool invert){l.enabledState=stateId(state);l.invert=invert;return l;}
WorldLight ddPanel(WorldLight l){l.fitting=model::clinic_ceiling_lamp_1;l.fittingWidth=1.05f;l.fittingDepth=1.04f;l.fittingHeight=.16f;return l;}
ScriptAction ddSet(const char* id,int value){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=stateId(id);a.value=value;return a;}
ScriptAction ddShut(int door){ScriptAction a;a.type=ScriptAction::Type::CloseDoor;a.index=door;return a;}
ScriptAction ddPlay(Sound s,float gain){ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=s;a.amount=gain;return a;}
void ddEvent(Map& m,const char* id,float x1,float y1,float x2,float y2,const char* require,int value,std::vector<ScriptAction> actions){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=-100;e.top=100;e.once=true;
 if(*require){e.requireState=stateId(require);e.requireValue=value;}e.actions=std::move(actions);m.scriptEvents.push_back(std::move(e));
}
void ddSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,bool loop=false){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=-9;s.y1=-9;s.x2=-8;s.y2=-8;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=0;s.soundUntilMs=0;s.loop=loop;m.timedSequences.push_back(s);
}
void ddBlood(Map& m,float x1,float y1,float x2,float y2,float z){wall(m,x1,y1,x2,y2,z+.004f,z+.010f,16);}
// Somebody left where they fell: a scripted track held in the dead pose.
void ddBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){actor(m,clock,visual,1.7f,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}});}
// Bumper rail on a corridor wall face (a thin strip at trolley height).
void ddCrash(Map& m,float x1,float y1,float x2,float y2,float f){wall(m,x1,y1,x2,y2,f+.86f,f+.96f,20);}
// An IV stand built from parts: weighted base, pole, hook and bag; a proxy keeps it solid.
void ddDrip(Map& m,float x,float y,float f,bool bag=true){
 wall(m,x-.18f,y-.18f,x+.18f,y+.18f,f,f+.03f,20);
 m.pipes.push_back({{x,y},{x,y},f+.03f,.012f,f+1.9f,5});wall(m,x-.18f,y-.18f,x+.18f,y+.18f,f+.03f,m.definition.ceiling,30);   // to the ceiling: nobody stands on a drip
 if(bag){m.pipes.push_back({{x,y},{x+.15f,y},f+1.88f,.008f,f+1.88f,5});ddFix(m,model::med_blood_bag,x+.15f,y,f+1.72f,.11f,.021f,.143f,ddS,false);}
}
// Cubicle curtain under its track, and one drawn back to the end of it. A proxy closes the gap
// above each to the ceiling: nobody crawls along a curtain top.
void ddCurtain(Map& m,float a,float b,float at,bool alongX,float f){
 const float roof=m.definition.ceiling;
 if(alongX){wall(m,a,at-.02f,b,at+.02f,f+.30f,f+2.35f,39);wall(m,a,at-.02f,b,at+.02f,f+2.35f,roof,30);}
 else{wall(m,at-.02f,a,at+.02f,b,f+.30f,f+2.35f,39);wall(m,at-.02f,a,at+.02f,b,f+2.35f,roof,30);}
}
void ddBunch(Map& m,float x,float y,float f,bool alongX){
 const float roof=m.definition.ceiling;
 if(alongX){wall(m,x-.08f,y-.06f,x+.08f,y+.06f,f+.24f,f+2.35f,39);wall(m,x-.06f,y+.04f,x+.1f,y+.12f,f+.27f,f+2.31f,39);wall(m,x-.08f,y-.06f,x+.1f,y+.12f,f+2.31f,roof,30);}
 else{wall(m,x-.06f,y-.08f,x+.06f,y+.08f,f+.24f,f+2.35f,39);wall(m,x+.04f,y-.06f,x+.12f,y+.1f,f+.27f,f+2.31f,39);wall(m,x-.06f,y-.08f,x+.12f,y+.1f,f+2.31f,roof,30);}
}
void ddTrack(Map& m,Vec2 a,Vec2 b,float f,float top){
 m.pipes.push_back({a,b,f+2.40f,.015f,f+2.40f,5});
 for(Vec2 q:{a,b})m.pipes.push_back({q,q,f+2.40f,.007f,top,5});
}
// A wheeled steel trolley 2 m long (along x or y), its tray at 0.82 m, optionally a body on it,
// feet toward +x or +y
// (and then an invisible cap to the ceiling, `cap` above the floor, so nobody climbs on to him).
void ddTrolley(Map& m,float x,float y,float f,bool alongX,bool body=false,float cap=2.8f){
 auto box=[&](float u1,float u2,float v1,float v2,float lo,float hi,int mat){
  if(alongX)wall(m,x+u1,y+v1,x+u2,y+v2,lo,hi,mat);else wall(m,x+v1,y+u1,x+v2,y+u2,lo,hi,mat);};
 for(float u:{-.88f,.88f})for(float v:{-.26f,.26f})box(u-.05f,u+.05f,v-.04f,v+.04f,f,f+.1f,8);
 for(float u:{-.86f,.86f})box(u-.03f,u+.03f,-.3f,.3f,f+.1f,f+.15f,20);
 box(-.86f,.86f,-.03f,.03f,f+.1f,f+.15f,20);
 for(float u:{-.86f,.86f})for(float v:{-.27f,.27f})box(u-.02f,u+.02f,v-.02f,v+.02f,f+.15f,f+.78f,20);
 box(-1.f,1.f,-.33f,.33f,f+.78f,f+.82f,20);
 for(float v:{-.33f,.31f})box(-1.f,1.f,v,v+.02f,f+.82f,f+.86f,20);
 for(float u:{-1.04f,1.01f})box(u,u+.03f,-.22f,.22f,f+.72f,f+.98f,20);
 if(body){ddCorpse(m,x,y,f+.79f,alongX?ddE:0.f,.9f);box(-1.f,1.f,-.33f,.33f,f+.86f,f+cap,30);}
}
// A run of bloody footprints, bare, left and right of the line through `steps`.
void ddPrints(Map& m,std::initializer_list<Vec2> steps,float f){
 int i=0;for(Vec2 p:steps){const float s=(i++%2)?.07f:-.07f;wall(m,p.x-.045f+s,p.y-.1f,p.x+.045f+s,p.y+.1f,f+.003f,f+.007f,16);}
}
// A bank of body fridges, three drawers high, along a wall: the cabinet body (dark iron, seen
// in the door gaps) from the door plane `face` back `depth` against the way `dir` the doors
// face; n columns `col` wide from `a`, stainless doors with bar handles, a steel fascia and
// wall above to `top`. Columns in `skip` are a bay `bay` deep; column `open` has its middle
// door standing open on its hinge and the tray run out.
void ddDrawers(Map& m,bool alongX,float face,float dir,float a,int n,float depth,float f,float top,std::initializer_list<int> skip={},float bay=0,int open=-1,float col=.8f){
 auto box=[&](float u1,float u2,float w1,float w2,float lo,float hi,int mat){
  if(alongX)wall(m,u1,std::min(w1,w2),u2,std::max(w1,w2),lo,hi,mat);else wall(m,std::min(w1,w2),u1,std::max(w1,w2),u2,lo,hi,mat);};
 auto skipped=[&](int c){for(int s:skip)if(s==c)return true;return false;};
 const float back=face-dir*depth;
 for(int c=0;c<n;){int e=c+1;const bool gap=skipped(c);if(c!=open)while(e<n&&e!=open&&skipped(e)==gap)++e;
  const float c1=a+c*col,c2=a+e*col;
  if(c==open){const float lo=f+.8f,hi=f+1.44f;                                            // the empty slot: floor, roof, back, liners
   box(c1,c2,back,face,f,lo,8);box(c1,c2,back,face,hi,f+2.3f,8);box(c1+.025f,c2-.025f,back,back+dir*.05f,lo,hi,8);
   box(c1,c1+.025f,back+dir*.05f,face,lo,hi,28);box(c2-.025f,c2,back+dir*.05f,face,lo,hi,28);}
  else if(!gap||depth-bay>.01f)box(c1,c2,back,gap?face-dir*bay:face,f,f+2.3f,8);
  c=e;}
 box(a,a+n*col,back,face,f+2.3f,top,3);
 box(a,a+n*col,face,face+dir*.012f,f+2.2f,f+2.3f,28);
 for(int c=0;c<n;++c){if(skipped(c))continue;const float u1=a+c*col+.025f,u2=a+(c+1)*col-.025f,um=(u1+u2)*.5f;
  for(int t=0;t<3;++t){const float lo=f+.1f+t*.7f,hi=lo+.64f;
   if(c==open&&t==1){
    box(u1,u1+.03f,face,face+dir*.72f,lo,hi,28);                                  // the door, swung wide on its hinge
    box(u1-.02f,u1,face+dir*.5f,face+dir*.68f,hi-.14f,hi-.1f,8);                  // its handle, on the outside
    for(float u:{u1+.07f,u2-.11f})box(u,u+.04f,back+dir*.05f,face+dir*.62f,lo,lo+.04f,8);   // the runners
    box(u1+.05f,u2-.05f,face-dir*1.25f,face+dir*.62f,lo+.04f,lo+.07f,28);          // the tray, run out on them
    // His hands, in dried blood (red paint finish: blood decals only take on the floor): a palm
    // on the liner just inside the mouth, the fingers round the frame edge; a palm and fingers
    // on the inside of the door where he pushed it open.
    box(u2-.008f,u2,face-dir*.13f,face-dir*.04f,lo+.2f,lo+.3f,19);
    for(int k=0;k<4;++k){const float z=lo+.205f+k*.024f;box(u2-.008f,u2,face-dir*.04f,face,z,z+.014f,19);box(u2,u2+.045f,face,face+dir*.008f,z,z+.014f,19);}
    box(u1+.03f,u1+.038f,face+dir*.5f,face+dir*.59f,lo+.16f,lo+.26f,19);
    for(int k=0;k<4;++k){const float w=face+dir*(.502f+k*.023f);box(u1+.03f,u1+.038f,w,w+dir*.014f,lo+.26f,lo+.32f+(k==1||k==2?.015f:0),19);}
    box(u1+.03f,u1+.038f,face+dir*.36f,face+dir*.5f,lo+.18f,lo+.2f,19);           // the drag where it slipped
    continue;}
   box(u1,u2,face,face+dir*.025f,lo,hi,28);
   box(um-.14f,um+.14f,face+dir*.025f,face+dir*.05f,hi-.13f,hi-.09f,8);}}
}
// A surgical light: an arm from the ceiling hub out to the head, the drop, the head and its
// handle; the lamp sits just under the lens.
void ddTheatreLamp(Map& m,Vec2 hub,Vec2 head,float z,float roof,float intensity){
 m.pipes.push_back({hub,head,roof-.4f,.035f,roof-.4f,5});
 m.pipes.push_back({head,head,z+.12f,.025f,roof-.4f,5});
 m.pipes.push_back({head,head,z,.34f,z+.1f,5});
 m.pipes.push_back({head,head,z+.1f,.2f,z+.15f,20});
 m.pipes.push_back({head,head,z-.1f,.03f,z,8});
 m.lights.push_back(lamp(head,z-.06f,intensity,7.f,5800,.06f,WorldLightMount::Bare));
}

// MEDICAL / SURGERY (manifest 48). Theatre 1 and the rooms that ran it: the transfer bay at
// the red line where the ward handed its patients over, the theatre corridor, scrub and
// lay-up, the theatre itself under its lamps, the medical director's office behind the
// observation glass and his records strongroom, recovery, and the disposal corridor out to
// the sluice, the specimen room and the porters' hold. Tonight Theatre 1 ran Procedure 14
// twice: resonance tissue excision on two tier-3 patients who were awake for it. LAB 4 was
// third on the list. Surgery has its own generator: when Ward C goes dark the theatre, its
// corridor and the director's rooms stay lit, and the light out of the transfer bay is the
// way to run. The director's transfer switch puts the ward back on the theatre generator
// (med_blackout off); the strongroom's lock is on the ward board, so it opens only once the
// ward is transferred, and inside it the mortuary release (med_morgue_release) works only
// then too.
// Route: the W seam from Ward C -> transfer bay -> theatre corridor -> theatre (log) ->
// disposal corridor -> director's office (switch) -> strongroom (release) -> back out west.
std::shared_ptr<const Map> makeMedicalSurgery(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[77];m.definition=CampaignChunks[77];m.definition.ambient=.22f;
 m.definition.playerStart={2.2f,11.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=0;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceClinicTile;
 const float f=-12,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'W',9.f,14.f,f,false,0});
 // --- Plan (walls .2 m, painted) -----------------------------------------------------------------
 // The strongroom door first (door 0): its lock is on the ward board.
 wall(m,16.6f,7.2f,21.4f,7.4f,f,roof,3);
 {const int d=wallDoor(m,21.4f,23.f,7.2f,7.4f,21.4f,22.4f,f,roof,-1);m.doors[size_t(d)].requireState=stateId("med_blackout");m.doors[size_t(d)].requireValue=0;}
 wallDoor(m,16.6f,23.f,15.6f,15.8f,17.4f,18.4f,f,roof,-1);                              // door 1: the office
 wallDoor(m,8.6f,16.6f,15.6f,15.8f,14.f,15.2f,f,roof,1);                                // door 2: the theatre's way out
 // The theatre's north wall: scrub panel, scrub door (3), lay-up door (4).
 wall(m,8.6f,7.2f,9.f,7.4f,f,roof,3);ddGlaze(m,9.f,7.2f,10.8f,7.4f,f,roof,1.2f,2.2f,false);
 wallDoor(m,10.8f,14.6f,7.2f,7.4f,11.f,11.9f,f,roof,1);
 wallDoor(m,11.9f,16.6f,7.2f,7.4f,14.6f,15.5f,f,roof,1);
 wallDoor(m,6.f,14.2f,18.f,18.2f,9.6f,10.5f,f,roof,1);                                  // door 5: sluice
 wallDoor(m,10.5f,19.6f,18.f,18.2f,14.2f,15.1f,f,roof,1);                               // door 6: specimens
 wallDoor(m,15.1f,23.f,18.f,18.2f,19.6f,20.8f,f,roof,1);                                // door 7: porters
 // West column: changing, staff room, the transfer bay, recovery.
 wall(m,5.8f,1.f,6.f,2.4f,f,roof,3);doorwayY(m,2.4f,3.4f,5.8f,6.f,f,roof);wall(m,5.8f,3.4f,6.f,6.8f,f,roof,3);
 doorwayY(m,6.8f,7.8f,5.8f,6.f,f,roof);wall(m,5.8f,7.8f,6.f,8.8f,f,roof,3);
 wall(m,1.f,4.4f,5.8f,4.6f,f,roof,3);wall(m,1.f,8.6f,6.f,8.8f,f,roof,3);wall(m,1.f,14.2f,6.f,14.4f,f,roof,3);
 wall(m,5.8f,14.4f,6.f,15.f,f,roof,3);doorwayY(m,15.f,17.2f,5.8f,6.f,f,roof);wall(m,5.8f,17.2f,6.f,23.f,f,roof,3);
 // The theatre corridor's east wall: scrub entry, the theatre's double doors on the seam axis.
 wall(m,8.6f,1.f,8.8f,4.4f,f,roof,3);doorwayY(m,4.4f,5.6f,8.6f,8.8f,f,roof);wall(m,8.6f,5.6f,8.8f,7.2f,f,roof,3);
 wall(m,8.6f,7.2f,8.8f,10.4f,f,roof,3);wall(m,8.6f,12.6f,8.8f,15.8f,f,roof,3);
 wall(m,8.6f,10.4f,8.8f,12.6f,f+2.4f,roof,3);
 for(float y:{10.34f,12.6f})wall(m,8.56f,y,8.84f,y+.06f,f,f+2.4f,20);
 wall(m,8.56f,10.34f,8.84f,12.66f,f+2.36f,f+2.4f,20);
 for(auto [y1,y2]:{std::pair{10.4f,10.45f},{12.55f,12.6f}}){                          // the leaves, pinned back
  wall(m,8.84f,y1,9.92f,y2,f+.02f,f+2.15f,20);wall(m,8.88f,y1-.005f,9.88f,y2+.005f,f+.02f,f+.32f,8);
  wall(m,9.2f,y1-.006f,9.6f,y2+.006f,f+1.25f,f+1.75f,9);}
 // North band and the theatre's east wall with the observation glass.
 wall(m,12.4f,1.f,12.6f,7.2f,f,roof,3);wall(m,16.4f,1.f,16.6f,7.2f,f,roof,3);
 wall(m,16.4f,7.2f,16.6f,9.4f,f,roof,3);ddGlaze(m,16.4f,9.4f,16.6f,13.6f,f,roof,1.f,2.3f,true,0,5);wall(m,16.4f,13.6f,16.6f,15.8f,f,roof,3);
 wall(m,11.8f,18.2f,12.f,23.f,f,roof,3);wall(m,17.4f,18.2f,17.6f,23.f,f,roof,3);
 // Theatre 1 is tiled wall to ceiling.
 for(auto [y1,y2]:{std::pair{7.4f,10.4f},{12.6f,15.6f}})wall(m,8.8f,y1,8.82f,y2,f,roof,27);
 wall(m,8.8f,10.4f,8.82f,12.6f,f+2.4f,roof,27);
 for(auto [x1,x2]:{std::pair{8.8f,9.f},{10.8f,11.f},{11.9f,14.6f},{15.5f,16.4f}})wall(m,x1,7.4f,x2,7.42f,f,roof,27);
 wall(m,9.f,7.4f,10.8f,7.42f,f,f+1.17f,27);wall(m,9.f,7.4f,10.8f,7.42f,f+2.26f,roof,27);
 for(auto [x1,x2]:{std::pair{11.f,11.9f},{14.6f,15.5f}})wall(m,x1,7.4f,x2,7.42f,f+2.65f,roof,27);
 for(auto [y1,y2]:{std::pair{7.42f,9.4f},{13.6f,15.58f}})wall(m,16.38f,y1,16.4f,y2,f,roof,27);
 wall(m,16.38f,9.4f,16.4f,13.6f,f,f+.97f,27);wall(m,16.38f,9.4f,16.4f,13.6f,f+2.36f,roof,27);
 for(auto [x1,x2]:{std::pair{8.82f,14.f},{15.2f,16.38f}})wall(m,x1,15.58f,x2,15.6f,f,roof,27);
 wall(m,14.f,15.58f,15.2f,15.6f,f+2.65f,roof,27);
 for(auto [x1,y1,x2,y2]:{std::tuple{1.f,1.f,5.8f,4.4f},{1.f,4.6f,5.8f,8.6f},{1.f,8.8f,6.f,14.2f},{1.f,14.4f,5.8f,23.f},{6.f,1.f,8.6f,18.f},
  {8.8f,1.f,12.4f,7.2f},{12.6f,1.f,16.4f,7.2f},{16.6f,1.f,23.f,7.2f},{16.6f,7.4f,23.f,15.6f},{8.8f,15.8f,23.f,18.f},
  {6.f,18.2f,11.8f,23.f},{12.f,18.2f,17.4f,23.f},{17.6f,18.2f,23.f,23.f}})ceilingGrid(m,x1,y1,x2,y2,roof);
 // --- The transfer bay: the red line where the ward handed them over -------------------------------
 floorLine(m,3.3f,8.8f,3.42f,14.2f,f,19);
 ddFix(m,model::med_ward_bed,2.3f,9.32f,f,1.984f,.944f,.842f,0,true);                     // the bed 40 came in on
 for(float x:{1.9f,2.75f}){wall(m,x,8.87f,x+.06f,9.77f,f+.6f,f+.608f,8);wall(m,x,9.77f,x+.06f,9.79f,f+.36f,f+.608f,8);}  // straps, cut
 wall(m,2.55f,8.95f,2.95f,9.27f,f+.6f,f+.7f,39);
 ddTrolley(m,4.75f,13.45f,f,true);                                                  // the theatre trolley, back from the table
 wall(m,3.9f,13.2f,4.4f,13.7f,f+.82f,f+.824f,16);
 ddFix(m,model::med_notice_board,1.9f,14.165f,f+1.15f,1.2f,.07f,.9f,ddN,false);
 taped(m,{2.95f,14.2f},f+1.55f,ddN,Readable::Note,"THEATRE 1 / NIGHT LIST","03:40 SUBJECT 31 / PROCEDURE 14 / DONE. 04:45 SUBJECT 40 / PROCEDURE 14 / DONE 05:19.",
  "05:40 LAB 4 / PROCEDURE 14. STRUCK OUT IN PEN: NOT IN HER BED.");
 ddFix(m,model::med_wall_clock,4.6f,8.81f,f+2.15f,.236f,.02f,.233f,ddS,false);
 wall(m,3.62f,8.8f,4.02f,8.95f,f+1.f,f+1.36f,20);wall(m,3.66f,8.95f,3.98f,8.96f,f+1.04f,f+1.3f,9);   // shoe covers
 kit::prop(m,model::trash_can,3.85f,9.12f,f,.3f);
 kit::prop(m,model::med_wheelchair,1.55f,13.4f,f,ddE+.2f,1.1f);
 for(float y:{11.1f,11.9f})floorLine(m,1.f,y,8.8f,y+.04f,f,8);                            // wheel scuffs into theatre
 paperDrift(m,1.2f,10.2f,5.6f,12.8f,f,3,7701);
 // --- The theatre corridor ------------------------------------------------------------------------
 for(auto [a,b]:{std::pair{1.1f,4.35f},{5.65f,10.3f},{12.7f,15.75f}})ddCrash(m,8.56f,a,8.6f,b,f);
 for(auto [a,b]:{std::pair{1.1f,2.35f},{3.45f,6.75f},{7.85f,8.55f},{14.45f,14.95f},{17.25f,17.95f}})ddCrash(m,6.f,a,6.04f,b,f);
 for(float x:{6.45f,6.85f,7.25f})ddFix(m,model::gas_cylinder,x,1.27f,f,.3f,.3f,1.45f,0,true);
 wall(m,6.2f,1.f,7.5f,1.08f,f+1.05f,f+1.11f,8);wall(m,6.2f,1.f,7.5f,1.08f,f+.45f,f+.51f,8);
 // a linen trolley, the last of the gowns in it
 for(float x:{7.75f,8.35f})for(float y:{1.75f,2.55f})wall(m,x-.02f,y-.02f,x+.02f,y+.02f,f,f+.9f,20);
 wall(m,7.73f,1.73f,8.37f,2.57f,f+.88f,f+.92f,20);wall(m,7.78f,1.78f,8.32f,2.52f,f+.35f,f+.88f,39);
 extinguisher(m,8.6f,6.4f,f,ddW);
 ddFix(m,model::med_notice_board,8.565f,3.3f,f+1.2f,1.2f,.07f,.9f,ddW,false);                             // the theatre list board, by the scrub door
 wall(m,6.f,5.f,6.12f,5.35f,f+1.3f,f+1.6f,8);wall(m,6.12f,5.08f,6.16f,5.27f,f+1.42f,f+1.56f,20);          // wall phone
 kit::prop(m,model::med_crash_cart,8.3f,14.35f,f,ddE);
 ddFix(m,model::med_first_aid_box,6.06f,13.2f,f+1.3f,.367f,.12f,.349f,ddE,false);
 // --- Staff changing --------------------------------------------------------------------------------
 for(int i=0;i<4;++i)ddFix(m,model::psx_locker,1.5f+i*.86f,1.33f,f,.64f,.85f,2.42f,-kPi*.5f,true);
 bench(m,3.3f,3.3f,f);
 shelf(m,{1.27f,3.25f},kPi*.5f,0);
 for(auto [y,t]:{std::pair{2.65f,0},{3.3f,0},{3.95f,0},{2.75f,1},{3.6f,1},{3.1f,2}})kit::prop(m,model::prop_cardboard_box,1.27f,y,f+(t==0?.306f:t==1?.972f:1.656f),ddE,.5f);
 wall(m,4.85f,1.55f,5.65f,2.25f,f,f+.04f,8);for(float x:{4.88f,5.58f})for(float y:{1.58f,2.18f})wall(m,x,y,x+.04f,y+.04f,f+.04f,f+.95f,20);
 wall(m,4.87f,1.57f,5.63f,2.23f,f+.3f,f+.92f,39);                                       // laundry skip
 wall(m,2.3f,3.05f,2.95f,3.5f,f+.51f,f+.53f,23);                                         // somebody's uniform, left on the bench
 // --- The staff room ---------------------------------------------------------------------------------
 ddFix(m,model::office_couch_green,1.64f,6.6f,f,2.242f*.92f,1.364f*.92f,1.192f*.92f,ddE,true);
 ddFix(m,model::wooden_table,3.75f,6.95f,f,1.367f,.724f,.78f,0,true);
 kit::prop(m,model::wooden_chair,3.45f,6.25f,f,ddN);
 kit::prop(m,model::wooden_chair,4.15f,7.7f,f,.35f);
 wall(m,2.8f,4.6f,5.8f,5.2f,f,f+.08f,8);wall(m,2.8f,4.6f,5.8f,5.18f,f+.08f,f+.86f,21);wall(m,2.78f,4.6f,5.8f,5.22f,f+.86f,f+.9f,20);
 kit::prop(m,model::coffee_machine,3.2f,4.9f,f+.9f,ddS);
 for(auto [x,y]:{std::pair{3.75f,4.95f},{4.f,4.85f}})kit::prop(m,model::mug,x,y,f+.9f,x);
 for(auto [x,y]:{std::pair{3.4f,6.9f},{4.3f,7.1f}})kit::prop(m,model::mug,x,y,f+.78f,y);
 kit::prop(m,model::office_paper_sheet,3.85f,6.85f,f+.78f,.4f);
 taped(m,{2.9f,8.6f},f+1.5f,ddN,Readable::Note,"THEATRE 1 / NIGHT ROTA","SURGEON: THE DIRECTOR. SCRUB: K. ADEYEMI. PORTER: ON CALL.",
  "ANAESTHETIST: NOT REQUIRED. (DIRECTOR'S HAND.)");
 // --- Recovery: three bays, one still occupied --------------------------------------------------------
 for(float y:{15.6f,18.4f,21.4f}){
  wall(m,1.f,y-.25f,1.25f,y+.25f,f+1.38f,f+1.42f,20);wall(m,1.f,y-.02f,1.1f,y+.02f,f+1.28f,f+1.38f,20);
  kit::prop(m,model::med_vitals_monitor_dead,1.12f,y,f+1.42f,ddE);}   // every recovery monitor is dark, 31's included
 ddFix(m,model::med_ward_bed,2.04f,15.6f,f,1.984f,.944f,.842f,0,true);ddCorpse(m,2.f,15.6f,f+.57f,ddE,.95f);   // subject 31
 ddFix(m,model::med_ward_bed_tossed,2.1f,18.4f,f,2.097f,1.525f,.842f,0,true);
 ddFix(m,model::med_ward_bed,2.04f,21.4f,f,1.984f,.944f,.842f,0,true);
 for(float x:{1.6f,2.5f})wall(m,x,20.93f,x+.06f,21.87f,f+.6f,f+.608f,8);
 ddDrip(m,3.45f,16.25f,f);ddDrip(m,3.5f,19.25f,f,false);
 ddTrack(m,{1.f,17.f},{4.2f,17.f},f,roof);ddCurtain(m,1.f,3.3f,17.f,true,f);ddBunch(m,3.4f,17.f,f,true);
 ddTrack(m,{1.f,19.9f},{4.2f,19.9f},f,roof);ddBunch(m,4.1f,19.9f,f,true);
 ddFix(m,model::med_nurse_desk,4.65f,22.7f,f,1.179f,.562f,.808f,0,true);
 kit::prop(m,model::office_chair,4.6f,21.9f,f,ddN+.3f);
 terminal(m,{4.45f,22.65f},"RECOVERY / THEATRE 1","31 NEVER CAME ROUND. 40 CAME ROUND ON THE TABLE AND STAYED ROUND.",
  "NOTHING TO RECOVER. PORTERS TO TAKE BOTH DOWN BEFORE SHIFT CHANGE.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.808f;m.terminals.back().yaw=ddN;
 kit::prop(m,model::desk_phone,5.05f,22.7f,f+.808f,ddN);
 for(auto [x,y,yaw]:{std::tuple{3.3f,17.6f,.4f},{4.6f,18.9f,2.f},{3.9f,15.3f,1.1f}})kit::prop(m,model::item_bandage,x,y,f,yaw);
 // --- Scrub: three sinks under the panel, the gowns ---------------------------------------------------
 for(float x:{9.35f,9.95f,10.55f})ddFix(m,model::med_scrub_sink,x,6.932f,f,.488f,.536f,1.127f,ddN,true);
 shelf(m,{10.f,1.25f},0,0);
 for(auto [x,t]:{std::pair{9.4f,0},{10.f,0},{10.55f,1},{9.5f,1},{10.2f,2}})kit::prop(m,model::prop_cardboard_box,x,1.25f,f+(t==0?.306f:t==1?.972f:1.656f),ddS,.6f);
 ddFix(m,model::med_supply_cabinet,12.178f,2.6f,f,1.1f,.444f,1.7f,ddW,true);
 for(float y:{4.4f,4.95f,5.5f}){wall(m,12.32f,y,12.4f,y+.04f,f+1.78f,f+1.84f,20);wall(m,12.16f,y-.18f,12.32f,y+.22f,f+.6f,f+1.78f,39);}   // gowns on their pegs
 wall(m,9.4f,4.3f,10.3f,4.95f,f,f+.012f,39);ddBlood(m,9.6f,4.45f,10.f,4.8f,f+.008f);       // one on the floor
 wall(m,8.8f,6.2f,8.92f,6.7f,f+1.3f,f+1.62f,20);wall(m,8.8f,5.8f,8.92f,6.1f,f+1.35f,f+1.55f,8);   // soap, brushes
 kit::prop(m,model::trash_can,11.95f,6.55f,f,0);
 for(float x:{11.05f,11.65f})for(float y:{3.f,3.8f})wall(m,x-.02f,y-.02f,x+.02f,y+.02f,f,f+.9f,20);
 wall(m,11.f,2.95f,11.7f,3.85f,f+.88f,f+.92f,20);wall(m,11.05f,3.f,11.65f,3.8f,f+.3f,f+.32f,20);
 for(float y:{3.05f,3.4f})wall(m,11.1f,y,11.6f,y+.3f,f+.92f,f+1.02f,39);
 for(float y:{1.1f,1.55f})wall(m,12.3f,y,12.4f,y+.35f,f+1.25f,f+1.55f,22);
 ddFix(m,model::med_wall_clock,10.f,1.01f,f+2.2f,.236f,.02f,.233f,ddS,false);
 taped(m,{12.4f,3.9f},f+1.5f,ddW,Readable::Note,"SCRUB / THEATRE 1","04:40 SCRUBBED FOR 40. THIRD TIME TONIGHT. THE DIRECTOR WATCHES FROM HIS OFFICE.",
  "LAB 4 IS NEXT ON THE LIST. I KNOW HER. I AM NOT SCRUBBING FOR HER. -K.A.");
 // --- Lay-up: the sets, the steriliser, the bench -------------------------------------------------------
 shelf(m,{13.5f,1.25f},0,0);shelf(m,{15.3f,1.25f},0,0);
 for(auto [x,y,t]:{std::tuple{13.f,1.25f,0},{13.75f,1.25f,0},{14.95f,1.25f,0},{15.7f,1.25f,1},{13.3f,1.25f,1},{14.f,1.25f,2},{15.f,1.25f,2}})
  kit::prop(m,model::prop_cardboard_box,x,y,f+(t==0?.306f:t==1?.972f:1.656f),ddS,.55f);
 for(int i=0;i<6;++i)kit::prop(m,model::office_file_binder,13.62f+i*.11f,1.25f,f+.972f,ddS);
 wall(m,15.75f,1.9f,16.4f,5.9f,f,f+.08f,8);wall(m,15.78f,1.9f,16.4f,5.9f,f+.08f,f+.86f,20);wall(m,15.72f,1.88f,16.4f,5.92f,f+.86f,f+.9f,20);
 for(auto [y1,y2]:{std::pair{2.1f,2.9f},{3.1f,3.75f}}){wall(m,15.82f,y1,16.32f,y2,f+.9f,f+.93f,20);
  for(float y=y1+.08f;y<y2-.05f;y+=.13f)wall(m,15.9f,y,16.24f,y+.02f,f+.93f,f+.945f,8);}       // trays laid up, never used
 ddFix(m,model::lab_test_chamber,16.06f,4.85f,f+.9f,1.14f,.68f,.64f,ddE,true);           // the steriliser
 ddFix(m,model::lab_sample_fridge,13.03f,6.1f,f,.7f,.85f,1.f,ddE,true);
 kit::prop(m,model::med_crash_cart,14.f,5.3f,f,.2f);
 // --- The strongroom: the director's records and the mortuary release ----------------------------------
 terminal(m,{19.8f,1.75f},"MORTUARY 2 / QUARANTINE DOOR","HELD SHUT 05:40 BY THE DIRECTOR. RELEASE NEEDS WARD C ON THE THEATRE GENERATOR: THE DOOR MOTOR IS ON THAT BOARD.",
  "ON RELEASE THE WAY THROUGH TO QUARANTINE OPENS. 06:02 PORTER ON THE PHONE: KNOCKING IN DRAWER 9. TOLD HIM LEAVE IT SHUT.",0,stateId("med_morgue_release"),stateId("medd_ward_transfer"));
 m.terminals.back().yaw=ddS;
 ddModel(m,model::pwr_control_cabinet,21.f,1.21f,f,ddS,true);m.pipes.push_back({{21.f,1.12f},{21.f,1.12f},f+1.6f,.03f,roof,8});
 ddCap(m,20.6f,1.f,21.4f,1.42f,f+1.62f,roof);                                                           // nobody crouches up on the cabinet
 for(float x:{18.85f,19.15f})ddFix(m,model::pwr_breaker_panel,x,1.051f,f+1.4f,.183f,.102f,.457f,ddS,false);
 wall(m,18.75f,1.f,21.f,1.06f,f+2.05f,f+2.11f,8);
 for(float y:{2.05f,3.f,3.95f})ddFix(m,model::med_records_cabinet,16.847f,y,f,.9f,.493f,1.f,ddE,true);
 for(auto [y,i]:{std::pair{2.f,0},{2.95f,1},{3.9f,2}})for(int k=0;k<3;++k)kit::prop(m,model::office_file_binder,16.84f,y-.25f+k*.11f+i*.05f,f+1.f,ddE);
 for(float y:{2.25f,4.15f}){shelf(m,{22.75f,y},kPi*.5f,0);const int seed=int(y*10);
  if(y>3.f){for(auto [dy,t]:{std::pair{-.45f,0},{.35f,0},{-.3f,1},{.4f,2}})kit::prop(m,model::prop_cardboard_box,22.75f,y+dy,f+(t==0?.306f:t==1?.972f:1.656f),ddE,.62f);continue;}
  for(int t=1;t<3;++t){const float z=f+(t==1?.972f:1.656f);
   for(int i=0;i<12;++i){if((i*5+t*3+seed)%7<3)continue;kit::prop(m,model::office_file_binder,22.75f,y-.75f+i*.13f,z,ddE);}}
  kit::prop(m,model::prop_cardboard_box,22.75f,y,f+.306f,ddE,.62f);}
 ddFix(m,model::wooden_table,19.5f,4.7f,f,1.367f,.724f,.78f,0,true);kit::prop(m,model::wooden_chair,19.2f,5.45f,f,.2f);
 kit::prop(m,model::prop_cardboard_box_open,19.15f,4.72f,f+.78f,.15f,.55f);kit::prop(m,model::office_paper_stack,19.53f,4.72f,f+.78f,.05f);
 const int strongLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,19.9f,4.84f,f+.78f,ddW);   // the lamp he went through the files by
 for(auto [x,y,yaw]:{std::tuple{20.f,5.4f,.3f},{18.7f,3.9f,1.6f}})kit::prop(m,model::office_paper_sheet,x,y,f,yaw);
 wall(m,16.7f,5.75f,17.55f,6.95f,f,f+1.1f,8);wall(m,17.55f,6.05f,17.6f,6.65f,f+.45f,f+.65f,20);   // the safe
 wall(m,16.6f,4.8f,16.72f,5.2f,f+1.35f,f+1.7f,20);wall(m,16.72f,4.88f,16.73f,5.12f,f+1.45f,f+1.6f,8);   // intercom to the mortuary
 // --- The director's office: his desk faces the glass --------------------------------------------------
 wall(m,17.4f,9.6f,21.8f,13.4f,f+.001f,f+.005f,23);                                       // the rug
 ddFix(m,model::lab_exec_desk,19.6f,11.5f,f,2.205f,1.097f,.891f,ddE,true);
 kit::prop(m,model::lab_boardroom_chair,20.75f,11.35f,f,ddW-.25f);
 for(float y:{10.85f,12.15f})kit::prop(m,model::wooden_chair,18.35f,y,f,ddW+(y-11.5f)*.3f);
 const int directorLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,19.45f,12.35f,f+.891f,ddW);
 kit::prop(m,model::desk_phone,19.55f,10.7f,f+.891f,ddE);
 kit::prop(m,model::office_paper_stack,19.75f,11.05f,f+.891f,.3f);
 kit::prop(m,model::binder,19.4f,11.25f,f+.891f,ddE);
 kit::prop(m,model::mug,19.95f,12.05f,f+.891f,0);
 terminal(m,{19.75f,11.75f},"DIRECTOR / TO INTERNAL INVESTIGATIONS","PROCEDURE 14 WORKS. THE GROWTH COMES OUT WHOLE. THE PATIENT DOES NOT COME BACK FROM IT. TWO OF TWO.",
  "LAB 4 IS THE CLEAN ONE YOU ASKED FOR. SEND HER DOWN AND YOU WILL HAVE YOUR CONTROL BY SIX.");
 m.terminals.back().readable=Readable::Letter;m.terminals.back().surface=.891f;m.terminals.back().yaw=ddW;
 ddFix(m,model::office_bookcase,22.75f,9.25f,f,2.025f,.5f,2.5f,ddW,true);
 ddFix(m,model::office_display_cabinet,22.73f,12.7f,f,.968f,.54f,2.1f,ddW,true);
 ddFix(m,model::office_couch_green,21.4f,14.97f,f,2.242f*.92f,1.364f*.92f,1.192f*.92f,ddN,true);
 ddFix(m,model::med_wall_clock,19.6f,15.59f,f+2.2f,.236f,.02f,.233f,ddN,false);
 // his coat, on its stand by the door
 {const float x=17.15f,y=14.9f;m.pipes.push_back({{x,y},{x,y},f,.16f,f+.03f,8});m.pipes.push_back({{x,y},{x,y},f+.03f,.02f,f+1.78f,8});
  wall(m,x-.2f,y-.2f,x+.2f,y+.2f,f+.03f,f+1.8f,30);wall(m,x-.17f,y-.03f,x+.17f,y+.03f,f+1.7f,f+1.74f,8);
  wall(m,x-.2f,y+.03f,x+.2f,y+.16f,f+.75f,f+1.7f,39);}
 // the ward supply panel: the transfer switch and the breakers either side of it
 ddFix(m,model::pwr_disconnect_switch,17.3f,7.577f,f+.95f,.429f,.353f,.853f,ddS,false);
 for(float x:{18.95f,19.25f})ddFix(m,model::pwr_breaker_panel,x,7.451f,f+1.3f,.183f,.102f,.457f,ddS,false);
 for(float x:{17.3f,18.95f,19.25f})m.pipes.push_back({{x,7.44f},{x,7.44f},f+1.8f,.02f,roof,8});
 // The control desk under the panel: the transfer itself. It works only with the ward mains
 // dead (requireState med_blackout), so it can end a blackout and never start one.
 terminal(m,{18.25f,7.78f},"WARD C / SUPPLY TRANSFER","WARD C MAINS TO THE THEATRE GENERATOR. THROW ONLY WITH THE WARD MAINS DEAD.",
  "THE STRONGROOM LOCK AND THE MORTUARY DOOR MOTOR ARE ON THE WARD C BOARD.",0,stateId("medd_transfer_thrown"),stateId("med_blackout"));
 m.terminals.back().yaw=ddS;
 taped(m,{16.85f,7.4f},f+1.5f,ddS,Readable::Note,"WARD C / IF THE MAINS DROPS","THROW THE TRANSFER AT THE DESK: WARD C RUNS ON THE THEATRE GENERATOR. LIGHTS IN SECONDS.",
  "KEEP THE WARD LIT. THEY DO NOT GET UP IN THE LIGHT. -DIRECTOR");
 {ScriptAction k;k.type=ScriptAction::Type::Shake;k.amount=.4f;
  ddEvent(m,"medd_transfer_do",16.6f,7.4f,23.f,15.6f,"medd_transfer_thrown",1,{ddPlay(Sound::Machine,.9f),k,ddSet("med_blackout",0),ddSet("medd_power_back",1)});}
 taped(m,{20.75f,7.4f},f+1.5f,ddS,Readable::Note,"STRONGROOM","LOCK IS ON THE WARD C BOARD. IF THE WARD GOES DARK THIS DOOR STAYS SHUT TILL THE WARD IS TRANSFERRED.",
  "MORTUARY RELEASE INSIDE. DIRECTOR'S HANDS ONLY.");
 // In the dark the transfer is armed: the strongroom shuts on its dead lock until the ward is lit.
 ddEvent(m,"medd_transfer_arm",16.6f,7.4f,23.f,15.6f,"med_blackout",1,{ddSet("medd_ward_transfer",1),ddShut(0)});
 // The release, heard over the strongroom's intercom from the mortuary below.
 ddEvent(m,"medd_release_go",16.6f,1.f,23.f,7.2f,"med_morgue_release",1,{ddSet("medd_release_t",1)});
 ddSequence(m,"medd_release_t","medd_release_heard",9000,8000);
 cue(m,"medd_release_t",400,Sound::Machine,{16.75f,5.f},"",.7f,f+1.5f);
 cue(m,"medd_release_t",900,Sound::Empty,{16.75f,5.f},"MORTUARY 2: QUARANTINE DOOR RELEASED.",.8f,f+1.5f);
 for(int t:{4200,4600,5100})cue(m,"medd_release_t",t,Sound::Metal2,{16.75f,5.f},"",.35f,f+1.5f);
 // --- Theatre 1 -------------------------------------------------------------------------------------
 // The table, its head to the machine, arm boards out, straps cut through.
 {const float cx=12.7f,cy=11.5f;
  wall(m,cx-.4f,cy-.32f,cx+.4f,cy+.32f,f,f+.05f,8);wall(m,cx-.18f,cy-.18f,cx+.18f,cy+.18f,f+.05f,f+.72f,20);
  wall(m,cx-1.1f,cy-.3f,cx+1.1f,cy+.3f,f+.72f,f+.8f,20);wall(m,cx-1.08f,cy-.28f,cx+1.08f,cy+.28f,f+.8f,f+.86f,23);
  wall(m,cx-1.45f,cy-.14f,cx-1.1f,cy+.14f,f+.76f,f+.82f,20);wall(m,cx-1.42f,cy-.12f,cx-1.12f,cy+.12f,f+.82f,f+.85f,23);
  for(float s:{-1.f,1.f}){wall(m,cx-.62f,cy+s*.3f,cx-.48f,cy+s*.95f,f+.76f,f+.8f,20);wall(m,cx-.6f,cy+s*.3f,cx-.5f,cy+s*.93f,f+.8f,f+.83f,23);}
  for(float x:{cx-.1f,cx+.6f}){wall(m,x,cy-.29f,x+.07f,cy+.29f,f+.86f,f+.868f,8);for(float s:{-1.f,1.f})wall(m,x,cy+s*.3f-.005f,x+.07f,cy+s*.3f+.005f,f+.55f,f+.86f,8);}
  for(float s:{-1.f,1.f})wall(m,cx-.58f,cy+s*.7f,cx-.52f,cy+s*.7f+.06f,f+.83f,f+.84f,8);
  wall(m,cx-1.f,cy-.2f,cx-.2f,cy+.15f,f+.866f,f+.87f,16);                                    // what ran off him
  ddBlood(m,cx-1.6f,cy-.15f,cx-.7f,cy+.75f,f);ddBlood(m,cx-.4f,cy+.3f,cx+.2f,cy+.62f,f);ddBlood(m,cx+.9f,cy-.5f,cx+1.25f,cy-.32f,f);
  ddFix(m,model::med_floor_drain,cx-1.05f,cy+.4f,f,.5f,.5f,.01f,0,false);
  // the lamps on their arms from one hub, and the laminar canopy round them
  m.pipes.push_back({{cx,cy},{cx,cy},roof-.45f,.08f,roof,5});
  ddTheatreLamp(m,{cx,cy},{cx-.5f,cy},f+2.28f,roof,1.9f);
  ddTheatreLamp(m,{cx,cy},{cx+.6f,cy+.02f},f+2.28f,roof,1.7f);
  for(auto [x1,y1,x2,y2]:{std::tuple{cx-1.7f,cy-1.7f,cx+1.7f,cy-1.62f},{cx-1.7f,cy+1.62f,cx+1.7f,cy+1.7f},{cx-1.7f,cy-1.62f,cx-1.62f,cy+1.62f},{cx+1.62f,cy-1.62f,cx+1.7f,cy+1.62f}})
   wall(m,x1,y1,x2,y2,roof-.26f,roof,20);
  wall(m,cx-1.62f,cy-1.62f,cx+1.62f,cy+1.62f,roof-.03f,roof,22);
  // The Mayo stand over the foot, and what is on it
  wall(m,cx+1.55f,cy-.95f,cx+1.95f,cy-.55f,f,f+.04f,8);wall(m,cx+1.73f,cy-.77f,cx+1.77f,cy-.73f,f+.04f,f+1.f,20);
  wall(m,cx+.9f,cy-.85f,cx+1.85f,cy-.4f,f+1.f,f+1.02f,20);
  for(float x=cx+.98f;x<cx+1.5f;x+=.11f)wall(m,x,cy-.78f,x+.015f,cy-.5f,f+1.02f,f+1.03f,8);
  ddBlood(m,cx+.95f,cy-.75f,cx+1.3f,cy-.55f,f+1.02f);
  kit::prop(m,model::med_specimen_jar,cx+1.7f,cy-.6f,f+1.02f,0);}
 // The anaesthetic machine nobody was asked to run, and the gas pendant over it.
 wall(m,10.f,9.55f,10.75f,10.25f,f,f+.12f,8);wall(m,10.03f,9.58f,10.72f,10.22f,f+.12f,f+.9f,28);
 wall(m,9.98f,9.53f,10.77f,10.27f,f+.9f,f+.94f,8);wall(m,10.03f,9.58f,10.72f,9.8f,f+.94f,f+1.5f,28);
 for(auto [x,mat]:{std::pair{10.1f,19},{10.32f,18},{10.54f,13}})wall(m,x,9.8f,x+.14f,9.88f,f+1.05f,f+1.28f,mat);
 wall(m,10.48f,9.98f,10.62f,10.12f,f+.94f,f+1.28f,9);wall(m,10.46f,9.96f,10.64f,10.14f,f+1.28f,f+1.31f,8);   // bellows
 wall(m,10.f,9.47f,10.76f,9.97f,f+1.5f,f+1.53f,8);
 const int anaesMonitor=int(m.clutterSpawns.size());kit::prop(m,model::med_vitals_monitor,10.38f,9.72f,f+1.53f,.3f);
 wall(m,10.3f,9.05f,10.6f,9.3f,f+2.f,roof,28);for(float z:{2.08f,2.24f,2.4f})wall(m,10.32f,9.3f,10.58f,9.32f,f+z,f+z+.07f,8);
 ddDrip(m,11.25f,12.35f,f);
 kit::prop(m,model::med_crash_cart,12.95f,10.35f,f,.05f);
 for(auto [x,yaw]:{std::pair{10.15f,.4f},{10.3f,1.7f}})kit::prop(m,model::med_pill_bottle,x,10.12f,f+.94f,yaw);
 kit::prop(m,model::trash_can,11.f,10.6f,f,1.f);
 // the kick bucket by the table, swabs in it
 {const float x=13.6f,y=12.7f;wall(m,x-.2f,y-.2f,x+.2f,y+.2f,f+.05f,f+.07f,20);for(float s:{-1.f,1.f})for(float t:{-1.f,1.f})wall(m,x+s*.17f-.025f,y+t*.17f-.025f,x+s*.17f+.025f,y+t*.17f+.025f,f,f+.05f,8);
  for(float s:{-1.f,1.f})wall(m,x+s*.19f-.012f,y-.012f,x+s*.19f+.012f,y+.012f,f+.07f,f+.4f,20);
  wall(m,x-.19f,y-.012f,x+.19f,y+.012f,f+.19f,f+.22f,20);m.pipes.push_back({{x,y},{x,y},f+.22f,.17f,f+.42f,20});wall(m,x-.1f,y-.08f,x+.12f,y+.1f,f+.42f,f+.425f,16);wall(m,x-.13f,y-.13f,x+.13f,y+.13f,f+.07f,f+.42f,30);}
 // the diathermy and suction trolleys
 wall(m,15.35f,8.5f,16.3f,9.05f,f,f+.08f,8);for(float x:{15.4f,16.2f})wall(m,x,8.55f,x+.05f,9.f,f+.08f,f+.72f,20);
 wall(m,15.35f,8.5f,16.3f,9.05f,f+.72f,f+.75f,20);
 ddFix(m,model::lab_instrument_terminal,15.82f,8.79f,f+.75f,.918f,.468f,.684f,ddS,true);
 wall(m,15.55f,14.65f,16.25f,15.25f,f,f+.1f,8);wall(m,15.6f,14.7f,16.2f,15.2f,f+.1f,f+.85f,20);
 for(float x:{15.75f,16.05f}){wall(m,x-.09f,14.86f,x+.09f,15.04f,f+.85f,f+1.12f,9);wall(m,x-.1f,14.85f,x+.1f,15.05f,f+1.12f,f+1.15f,8);}
 // the writing shelf, the log, the swab board, the clock, the films
 wall(m,9.3f,15.28f,10.7f,15.58f,f+.97f,f+1.f,20);for(float x:{9.35f,10.6f})wall(m,x,15.5f,x+.05f,15.58f,f+.7f,f+.97f,20);
 terminal(m,{10.f,15.43f},"PROCEDURE 14: RESONANCE TISSUE EXCISION","SUBJECT AWARE. 04:52 INCISION, FRONTAL, SUBJECT 40, TIER 3. AWAKE UNDER FULL DOSE. ASKED WHAT WE WERE TAKING OUT.",
  "05:07 41 G CRYSTALLINE TISSUE OUT. STILL TALKING. 05:19 CERTIFIED DEAD BY THE DIRECTOR. TO THE MORTUARY, DRAWER 9.");
 m.terminals.back().readable=Readable::Letter;m.terminals.back().surface=1.f;m.terminals.back().yaw=ddN;
 wall(m,8.82f,13.1f,8.85f,14.9f,f+1.15f,f+2.05f,20);wall(m,8.85f,13.15f,8.86f,14.85f,f+1.2f,f+2.f,22);
 for(float z:{1.85f,1.7f,1.55f,1.4f})wall(m,8.86f,13.3f,8.865f,13.3f+(z>1.6f?1.1f:.7f),f+z,f+z+.025f,8);
 ddFix(m,model::med_wall_clock,12.7f,15.57f,f+2.3f,.236f,.02f,.233f,ddN,false);
 for(float x:{11.55f,12.7f})ddFix(m,model::med_supply_cabinet,x,15.358f,f,1.1f,.444f,1.7f,ddN,true);
 for(float x:{12.75f,13.25f,13.75f})ddFix(m,x==13.25f?model::med_xray_viewer_chest:model::med_xray_viewer_skull,x,7.47f,f+1.3f,.408f,.1f,.5f,ddS,false);
 // the trolley that took 40 down: its wheels in what he left, out of the door and east
 for(float x:{14.32f,14.88f})wall(m,x,11.3f,x+.03f,15.6f,f+.003f,f+.007f,16);
 for(float x:{14.32f,14.88f})wall(m,x,15.6f,x+.03f,15.8f,f+.003f,f+.007f,16);
 for(float y:{16.55f,17.12f})wall(m,14.3f,y,19.9f,y+.03f,f+.003f,f+.007f,16);
 for(float x:{19.9f,20.45f}){wall(m,x,16.55f,x+.03f,18.f,f+.003f,f+.007f,16);wall(m,x,18.2f,x+.03f,20.4f,f+.003f,f+.007f,16);}
 // --- The disposal corridor -------------------------------------------------------------------------
 for(auto [a,b]:{std::pair{8.9f,13.9f},{15.3f,17.3f},{18.5f,22.9f}})ddCrash(m,a,15.8f,b,15.84f,f);
 for(auto [a,b]:{std::pair{8.9f,9.5f},{10.6f,14.1f},{15.2f,19.5f},{20.9f,21.4f}})ddCrash(m,a,17.96f,b,18.f,f);
 for(float x:{9.25f,9.75f})for(float y:{16.f,16.6f})wall(m,x-.02f,y-.02f,x+.02f,y+.02f,f,f+.95f,20);
 wall(m,9.2f,15.95f,9.8f,16.65f,f+.92f,f+.95f,20);wall(m,9.25f,16.f,9.75f,16.6f,f+.4f,f+.92f,39);ddBlood(m,9.4f,16.1f,9.7f,16.4f,f+.92f);
 kit::prop(m,model::trash_can,11.2f,17.65f,f,.3f);kit::prop(m,model::trash_can,11.7f,17.62f,f,1.3f);
 for(float y:{16.15f,16.55f,16.95f,17.35f})ddFix(m,model::gas_cylinder,22.8f,y,f,.3f,.3f,1.45f,0,true);
 wall(m,22.92f,15.9f,23.f,17.6f,f+1.05f,f+1.11f,8);wall(m,22.92f,15.9f,23.f,17.6f,f+.45f,f+.51f,8);
 extinguisher(m,16.6f,18.f,f,ddN);
 // --- The sluice ----------------------------------------------------------------------------------------
 ddFix(m,model::med_ward_toilet,6.9f,22.648f,f,.403f,.703f,.823f,ddN,true);
 for(float x:{8.f,8.6f})ddFix(m,model::med_scrub_sink,x,22.732f,f,.488f,.536f,1.127f,ddN,true);
 wall(m,9.4f,22.25f,10.4f,23.f,f,f+1.15f,20);wall(m,9.5f,22.24f,10.3f,22.25f,f+.25f,f+1.f,8);
 m.pipes.push_back({{9.9f,22.8f},{9.9f,22.8f},f+1.15f,.05f,roof,0});
 wall(m,6.f,19.f,6.32f,21.4f,f+1.3f,f+1.33f,20);wall(m,6.f,19.f,6.32f,21.4f,f+1.7f,f+1.73f,20);
 for(float y:{19.25f,19.75f,20.25f,20.75f,21.15f})wall(m,6.05f,y,6.28f,y+.2f,f+1.33f,f+1.5f,20);   // bowls drying
 for(auto [x,y,r,h]:{std::tuple{11.35f,18.7f,.33f,.46f},{10.85f,18.85f,.3f,.4f},{11.3f,19.4f,.34f,.44f},{10.8f,19.5f,.26f,.32f}})ddSack(m,x,y,f,r,h,18);   // the night's clinical waste, yellow-bagged and tied, waiting on the porters
 ddFix(m,model::med_floor_drain,8.3f,21.f,f,.5f,.5f,.01f,0,false);
 kit::prop(m,model::trash_can,7.7f,20.3f,f,.6f);
 wall(m,7.55f,20.15f,7.85f,20.45f,f+.6f,f+.61f,16);
 // The porters' side of it: gloves and aprons in the cabinet by the door, the theatre's linen skip
 // waiting to go down with the bags, a lamp over the hopper end.
 ddFix(m,model::med_supply_cabinet,7.6f,18.422f,f,1.1f,.444f,1.7f,ddS,true);
 {const float x=11.25f,y=21.65f;wall(m,x-.3f,y-.3f,x+.3f,y+.3f,f+.12f,f+.16f,20);
  for(float dx:{-.28f,.25f})for(float dy:{-.28f,.25f})wall(m,x+dx,y+dy,x+dx+.03f,y+dy+.03f,f+.06f,f+.9f,20);
  wall(m,x-.27f,y-.27f,x+.27f,y+.27f,f+.16f,f+.8f,39);ddBlood(m,x-.15f,y-.1f,x+.1f,y+.12f,f+.8f);}
 for(auto [x,y,yaw]:{std::tuple{10.6f,21.9f,.5f},{8.9f,19.4f,2.2f}})kit::prop(m,model::item_bandage,x,y,f,yaw);
 // --- Specimens: the two lumps that came out of them ---------------------------------------------------
 for(float x:{13.3f,14.55f})ddFix(m,model::lab_clean_bench,x,22.69f,f,1.179f,.562f,.808f,0,true);
 kit::prop(m,model::office_chair,13.4f,21.95f,f,ddN-.2f);
 kit::propPose(m,model::office_chair,15.6f,21.1f,f,1.9f,0,kPi*.5f);
 kit::prop(m,model::lab_microscope,13.05f,22.62f,f+.808f,0);
 for(auto [x,yaw]:{std::pair{14.2f,.5f},{14.85f,2.3f}}){wall(m,x-.2f,22.5f,x+.2f,22.85f,f+.808f,f+.82f,20);
  ddModel(m,model::lab_anomaly_relic,x,22.67f,f+.82f,yaw,false,.3f);}                  // 38 g and 41 g, on their trays
 ddFix(m,model::lab_specimen_cabinet,17.03f,20.f,f,1.34f,.74f,2.19f,ddW,true);
 shelf(m,{16.25f,18.47f},0,0);
 for(auto [x,t]:{std::pair{15.6f,0},{16.f,0},{16.5f,1},{15.75f,1},{16.85f,1},{16.2f,2},{16.6f,2}})kit::prop(m,model::med_specimen_jar,x,18.47f,f+(t==0?.306f:t==1?.972f:1.656f),x);
 ddFix(m,model::lab_sample_fridge,12.43f,20.9f,f,.7f,.85f,1.f,ddE,true);
 wall(m,12.6f,22.68f,15.4f,23.f,f+1.45f,f+2.15f,20);wall(m,12.62f,22.66f,15.38f,22.68f,f+1.47f,f+2.13f,8);for(float x:{13.3f,14.f,14.7f})wall(m,x,22.62f,x+.02f,22.66f,f+1.47f,f+2.13f,20);
 for(float x:{13.6f,14.3f})wall(m,x,22.6f,x+.1f,22.62f,f+1.7f,f+1.74f,8);
 for(float x:{13.f,13.5f})ddFix(m,x>13.2f?model::med_xray_viewer_chest:model::med_xray_viewer_skull,x,18.25f,f+1.3f,.408f,.1f,.5f,ddS,false);
 taped(m,{16.2f,23.f},f+1.5f,ddN,Readable::Note,"SPECIMENS / PROCEDURE 14","31: 38 G. 40: 41 G. STILL WARM AT FOUR DEGREES. KEEP THEM OFF THE STEEL.",
  "THEY RING WHEN YOU WALK PAST. THEY RANG WHEN SHE WAS WHEELED BY.");
 // --- The porters' hold ---------------------------------------------------------------------------------
 ddTrolley(m,20.19f,21.4f,f,false);                                                // the one that took him down, back empty
 wall(m,19.95f,20.55f,20.45f,21.1f,f+.82f,f+.824f,16);
 wall(m,20.75f,21.3f,21.95f,22.1f,f,f+.012f,23);                                           // its sheet, thrown off
 ddFix(m,model::wooden_table,18.6f,19.f,f,1.367f,.724f,.78f,ddE,true);
 kit::prop(m,model::wooden_chair,19.3f,19.f,f,ddE+.4f);
 terminal(m,{18.6f,18.75f},"PORTERS / TO THE MORTUARY","05:25 SUBJECT 40 DOWN TO MORTUARY 2, DRAWER 9. WARM. STILL WARM.",
  "31 STAYS IN RECOVERY TILL THE NIGHT PORTER ANSWERS HIS PHONE. HE DOESN'T.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.78f;m.terminals.back().yaw=ddE;
 kit::prop(m,model::desk_phone,18.55f,19.3f,f+.78f,ddE);
 const int porterLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,18.6f,19.55f,f+.78f,ddE);
 for(float x:{22.35f})for(float y:{19.3f,20.1f})kit::prop(m,model::med_wheelchair,x,y,f,ddW);
 for(float y:{18.6f})wall(m,17.62f,y,17.7f,y+.04f,f+1.75f,f+1.8f,20);
 shelf(m,{21.9f,22.75f},0,0);for(auto [x,t]:{std::pair{21.4f,0},{22.25f,0},{21.6f,1},{22.3f,2}})kit::prop(m,model::prop_cardboard_box,x,22.75f,f+(t==0?.306f:t==1?.972f:1.656f),ddS,.6f);
 // Nobody lies along a shelf top: a cap over every shelf in the map, clear of its top board's stock.
 for(auto [x,y,alongX]:{std::tuple{1.27f,3.25f,false},{10.f,1.25f,true},{13.5f,1.25f,true},{15.3f,1.25f,true},{22.75f,2.25f,false},{22.75f,4.15f,false},
  {16.25f,18.47f,true},{21.9f,22.75f,true}}){const float a=alongX?.9f:.25f,b=alongX?.25f:.9f;ddCap(m,x-a,y-b,x+a,y+b,f+2.1f,roof);}
 wall(m,17.6f,20.6f,17.72f,20.95f,f+1.3f,f+1.6f,8);wall(m,17.72f,20.68f,17.76f,20.87f,f+1.42f,f+1.56f,20);
 wall(m,17.65f,18.4f,17.8f,18.85f,f+.85f,f+1.75f,23);                                     // a porter's jacket
 // --- Light (29): the theatre generator keeps the route lit; the ward board feeds the side rooms ----
 // Essential circuit: lit through the blackout.
 m.lights.push_back(ddTube(3.f,11.5f,roof,.65f,7.5f,5600,.06f));                           // the transfer bay: light out to the ward
 m.lights.push_back(ddTube(7.3f,4.4f,roof,.45f,5.5f,5200,.05f));
 m.lights.push_back(ddTube(7.3f,11.5f,roof,.5f,6.f,5200,.05f));
 m.lights.push_back(ddTube(7.3f,16.6f,roof,.45f,5.5f,5200,.05f));
 for(auto [x,y]:{std::pair{10.f,9.f},{15.3f,9.f},{10.f,14.f},{15.3f,14.f}})m.lights.push_back(ddPanel(ddTube(x,y,roof,.55f,6.f,6200,.04f)));
 m.lights.push_back(ddTube(10.6f,4.2f,roof,.5f,5.f,5600,.05f));                           // scrub
 m.lights.push_back(ddTube(14.5f,4.2f,roof,.45f,5.f,5400,.05f,.12f));                     // lay-up
 m.lights.push_back(ddTube(19.8f,4.4f,roof,.45f,5.5f,4600,.05f));                         // strongroom
 m.lights.push_back(lamp({19.9f,4.84f},f+1.24f,.5f,4.2f,3000,0,WorldLightMount::Bare));m.lights.back().prop=strongLamp;   // and the desk lamp, on
 m.lights.push_back(ddPanel(ddTube(20.6f,9.2f,roof,.4f,6.f,3600,0)));                     // office
 m.lights.push_back(lamp({19.45f,12.2f},f+1.28f,.45f,3.2f,3000,0,WorldLightMount::Bare));m.lights.back().prop=directorLamp;   // his desk lamp, on
 m.lights.push_back(ddTube(12.3f,16.9f,roof,.45f,5.5f,5200,.06f));
 m.lights.push_back(ddTube(21.4f,16.9f,roof,.45f,5.5f,5200,.06f,.12f));                    // the east end and its cylinders
 m.lights.push_back(lamp({10.4f,9.86f},f+1.68f,.16f,1.6f,7000,.4f,WorldLightMount::Bare));m.lights.back().prop=anaesMonitor;   // the machine's monitor
 m.lights.push_back(lamp({14.5f,22.5f},f+1.1f,.45f,2.2f,9000,-.25f,WorldLightMount::Bare));                      // off the two lumps
 // On the generator in the dark, the route tubes run cold and unsteady.
 for(auto [x,y]:{std::pair{3.f,11.5f},{7.3f,11.5f},{7.3f,16.6f}}){auto l=lamp({x,y},roof-.3f,.4f,6.5f,6800,.1f,WorldLightMount::Bare);l.flicker=.3f;m.lights.push_back(ddSw(l,"med_blackout",false));}
 for(auto& l:m.lights)if(l.mount!=WorldLightMount::Bare&&(std::fabs(l.position.x-3.f)<.01f||std::fabs(l.position.x-7.3f)<.01f)&&std::fabs(l.position.y-4.4f)>.01f)l=ddSw(l,"med_blackout",true);
 // Ward board: dark while the ward is.
 m.lights.push_back(ddSw(ddTube(3.4f,2.7f,roof,.45f,5.f,5000,.06f),"med_blackout",true));               // changing
 m.lights.push_back(ddSw(ddTube(3.4f,6.6f,roof,.45f,5.f,3400,0),"med_blackout",true));                  // staff room
 m.lights.push_back(ddSw(ddTube(3.4f,16.8f,roof,.4f,5.5f,5400,.06f),"med_blackout",true));              // recovery
 m.lights.push_back(ddSw(ddTube(3.4f,21.f,roof,.35f,5.f,5400,.06f,.25f),"med_blackout",true));
 m.lights.push_back(ddSw(ddTube(8.9f,20.6f,roof,.55f,5.5f,5000,.25f),"med_blackout",true));             // sluice
 m.lights.push_back(ddSw(lamp({11.8f,20.4f},f+2.2f,.4f,5.f,4200,.1f,WorldLightMount::Wall,kPi),"med_blackout",true));   // its bulkhead over the bags
 m.lights.push_back(ddSw(ddTube(14.7f,20.4f,roof,.4f,5.f,5200,.05f),"med_blackout",true));              // specimens
 m.lights.push_back(ddSw(ddTube(20.3f,20.6f,roof,.35f,5.f,4800,.06f),"med_blackout",true));             // porters
 {auto l=ddSw(lamp({18.6f,19.55f},f+1.24f,.4f,3.6f,3000,0,WorldLightMount::Bare),"med_blackout",true);l.prop=porterLamp;m.lights.push_back(l);}   // and their desk lamp
 // --- Rewards and names -----------------------------------------------------------------------------
 m.pickupSpawns={{{16.06f,2.6f},PickupKind::Health,f+.9f},{{4.f,7.f},PickupKind::Ammo,f+.78f},{{21.3f,13.95f},PickupKind::Health,f},{{18.9f,19.25f},PickupKind::Ammo,f+.78f}};
 m.pickupSpawns.push_back({{19.95f,4.55f},PickupKind::HandgunAmmo,f+.78f});   // 9 mm by the open carton: the strongroom kept the director's sidearm
 m.pickupSpawns.push_back({{2.55f,2.05f},PickupKind::HandgunAmmo,f});         // 9 mm dropped in front of the lockers when they were cleared out
 m.clutterSpawns.insert(m.clutterSpawns.end(),{{2,{3.3f,4.85f},f+.9f},{1,{3.7f,7.2f},f+.78f,.5f},{0,{7.4f,9.6f},f},{0,{2.6f,12.6f},f,1.f},
  {2,{16.f,3.f},f+.9f,.3f},{0,{12.4f,17.f},f,2.f},{5,{19.9f,10.9f},f+.891f,.2f}});
 zone(m,"MEDICAL / SURGERY / TRANSFER BAY",0,8.8f,6.f,14.2f);
 zone(m,"MEDICAL / SURGERY / THEATRE 1",8.8f,7.4f,16.4f,15.6f);
 zone(m,"MEDICAL / SURGERY / SCRUB",8.8f,1.f,12.4f,7.2f);
 zone(m,"MEDICAL / SURGERY / LAY-UP",12.6f,1.f,16.4f,7.2f);
 zone(m,"MEDICAL / SURGERY / DIRECTOR'S RECORDS",16.6f,1.f,23.f,7.2f);
 zone(m,"MEDICAL / SURGERY / MEDICAL DIRECTOR",16.6f,7.4f,23.f,15.6f);
 zone(m,"MEDICAL / SURGERY / STAFF CHANGING",1.f,1.f,5.8f,4.4f);
 zone(m,"MEDICAL / SURGERY / STAFF ROOM",1.f,4.6f,5.8f,8.6f);
 zone(m,"MEDICAL / SURGERY / RECOVERY",1.f,14.4f,5.8f,23.f);
 zone(m,"MEDICAL / SURGERY / SLUICE",6.f,18.2f,11.8f,23.f);
 zone(m,"MEDICAL / SURGERY / SPECIMENS",12.f,18.2f,17.4f,23.f);
 zone(m,"MEDICAL / SURGERY / PORTERS",17.6f,18.2f,23.f,23.f);
 zone(m,"MEDICAL / SURGERY / DISPOSAL CORRIDOR",8.8f,15.8f,23.f,18.f);
 zone(m,"MEDICAL / SURGERY / THEATRE CORRIDOR",6.f,1.f,8.8f,18.f);
 zone(m,"MEDICAL / SURGERY",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}

// MEDICAL / MORGUE (manifest 48). Mortuary 2, a floor below the ward: the stair down from
// Ward C beside the dead body lift, receiving at its foot, the body store (two walls of
// drawers and an island between them), the post-mortem room, and the quarantine lock with
// the door to the Quarantine Ward behind protective glass, the goal shown before the path.
// Subject 40 came down at 05:25 as dead and went into drawer 9. Drawer 9 is open from the
// inside; his prints cross the aisle to a bay in the island where a fridge unit was pulled,
// and he stands in it, still, while the lights are on. The store's tubes blow as you pass
// (or are already dead if the ward went dark), and he gets up.
// Route: N seam (z -12) -> landing -> stair -> receiving -> body store (round the island,
// down the drawer aisle past the bay) -> post-mortem -> quarantine lock -> S door on
// med_morgue_release. Side rooms off receiving: the office, its plant room and the night
// porter's room; off the lock, the suit-up room.
std::shared_ptr<const Map> makeMedicalMorgue(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[78];m.definition=CampaignChunks[78];m.definition.ambient=.22f;
 m.definition.playerStart={12.f,1.5f};m.definition.spawnHeight=-12.f;m.definition.spawnYaw=kPi*.5f;
 m.wallSurface=SurfacePaintedTeal;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceConcrete;
 const float f=-15,up=-12,roof=m.definition.ceiling,ceil=-12.2f;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,up,false,0});
 openEdge(m,{'S',10.f,14.f,f,true,stateId("med_morgue_release")});                          // door 0: the quarantine door
 machineryClock(m,"medd_morgue_clock",60000);
 // --- The stair hall: concrete shaft, landing at ward level, the flight down ---------------------------
 rectangle(m.layers[0],9,0,10,7,'#');rectangle(m.layers[0],12,3,16,7,'#');rectangle(m.layers[0],15,0,16,3,'#');
 deck(m,up,10,0,14,1);deck(m,up,10,1,15,3);
 wall(m,10.f,0.f,15.f,3.f,f,up-.22f,15);                                                  // the landing's concrete
 m.stairs.push_back({10.f,3.f,12.f,7.5f,f,up,16,true,false});
 for(auto [y1,top]:{std::pair{3.f,-12.9f},{4.f,-13.65f},{5.f,-14.2f}})wall(m,10.f,y1,12.f,y1+1.f,f,top,15);   // the flight's stepped base
 wall(m,9.2f,6.95f,12.f,7.15f,ceil,roof,3);
 wall(m,10.f,6.78f,12.f,6.95f,-10.55f,-10.15f,20);for(float x:{10.4f,11.6f})wall(m,x,6.9f,x+.08f,6.95f,-10.15f,roof,8);                                               // the bulkhead where the shaft meets the floor
 // the slab over the mortuary floor
 wall(m,1.f,1.f,9.2f,23.f,ceil,up,27);wall(m,9.2f,7.1f,12.f,23.f,ceil,up,27);wall(m,12.f,7.f,16.f,23.f,ceil,up,27);wall(m,16.f,1.f,23.f,23.f,ceil,up,27);
 // the body lift, dead, at both levels
 for(auto [y,z,dir]:{std::tuple{2.945f,up,-1.f},{7.f,f,1.f}}){
  const float a=std::min(y,y+dir*.04f),b=std::max(y,y+dir*.04f);
  wall(m,12.5f,a,14.5f,b,z,z+2.2f,8);for(float x:{12.6f,13.52f})wall(m,x,std::min(y,y+dir*.05f),x+.88f,std::max(y,y+dir*.05f),z,z+2.12f,28);
  wall(m,14.65f,std::min(y,y+dir*.06f),14.85f,std::max(y,y+dir*.06f),z+1.1f,z+1.4f,20);}
 taped(m,{13.52f,2.895f},up+1.5f,ddN,Readable::Note,"BODY LIFT","OUT SINCE 02:50. BRING THEM DOWN BY THE STAIR, TWO PORTERS TO A TROLLEY.",
  "NOBODY COMES DOWN ALONE. NOBODY GOES IN THE STORE ALONE.");
 ddTrolley(m,13.9f,1.95f,up,true,true,roof-up);
 // --- Lower plan (walls .2 m to the slab) ---------------------------------------------------------------
 wall(m,9.f,7.f,9.2f,9.4f,f,up,3);doorwayY(m,9.4f,11.f,9.f,9.2f,f,up);wall(m,9.f,11.f,9.2f,15.2f,f,up,3);  // receiving / body store
 wallDoor(m,1.f,9.2f,14.8f,15.f,3.4f,4.6f,f,up,1);                                      // door 1: store to post-mortem
 wall(m,9.2f,14.8f,9.8f,15.4f,f,up,3);
 wall(m,9.2f,15.2f,10.6f,15.4f,f,up,3);ddGlaze(m,10.6f,15.2f,14.6f,15.4f,f,up,.95f,2.2f,true);wall(m,14.6f,15.2f,16.f,15.4f,f,up,3);
 wall(m,9.6f,15.4f,9.8f,19.4f,f,up,3);doorwayY(m,19.4f,20.8f,9.6f,9.8f,f,up);wall(m,9.6f,20.8f,9.8f,23.f,f,up,3);
 wall(m,15.8f,7.f,16.f,8.f,f,up,3);doorwayY(m,8.f,9.1f,15.8f,16.f,f,up);wall(m,15.8f,9.1f,16.f,9.6f,f,up,3);
 ddGlaze(m,15.8f,9.6f,16.f,11.6f,f,up,.95f,2.1f,false,.3f);
 wall(m,15.8f,11.6f,16.f,19.6f,f,up,3);doorwayY(m,19.6f,20.8f,15.8f,16.f,f,up);wall(m,15.8f,20.8f,16.f,23.f,f,up,3);
 wallDoor(m,16.f,23.f,6.6f,6.8f,21.6f,22.5f,f,up,-1);                                  // door 2: plant
 wallDoor(m,16.f,23.f,12.4f,12.6f,18.f,18.9f,f,up,1);                                  // door 3: night porter
 wall(m,16.f,17.4f,23.f,17.6f,f,up,3);
 for(auto [x1,y1,x2,y2]:{std::tuple{9.2f,7.15f,15.8f,15.2f},{1.f,15.f,9.6f,23.f},{9.8f,15.4f,15.8f,23.f},{16.f,6.8f,23.f,12.4f},{16.f,1.f,23.f,6.6f},
  {16.f,12.6f,23.f,17.4f},{16.f,17.6f,23.f,23.f}})ceilingGrid(m,x1,y1,x2,y2,ceil);
 // --- The body store: two walls of drawers and the island between ---------------------------------------
 ddDrawers(m,true,3.f,1.f,3.2f,7,2.f,f,up);                                               // north wall, 3-25
 wall(m,1.f,1.f,3.f,3.f,f,up,8);wall(m,3.f,1.f,3.2f,3.f,f,up,28);wall(m,8.8f,1.f,9.f,3.f,f,up,28);
 ddDrawers(m,false,3.f,1.f,3.2f,14,2.f,f,up,{},0,4);                                       // west wall; drawer 9 stands open
 wall(m,1.f,3.f,3.f,3.2f,f,up,28);wall(m,1.f,14.4f,3.f,14.8f,f,up,28);
 ddDrawers(m,false,5.f,-1.f,5.2f,12,2.2f,f,up,{4,5},1.1f);                                 // the island: drawers on its west face, the bay pulled
 // its east face is the store's plant side: painted panel, kick plate, louvred grilles, the
 // refrigerant pair along it into the slab
 wall(m,7.2f,5.2f,7.212f,14.8f,f,f+.12f,20);wall(m,7.2f,5.2f,7.212f,14.8f,f+.12f,f+2.3f,3);
 for(float y:{6.6f,9.8f,13.f}){wall(m,7.212f,y-.45f,7.232f,y+.45f,f+.3f,f+.9f,8);for(float z=.36f;z<.86f;z+=.08f)wall(m,7.232f,y-.42f,7.25f,y+.42f,f+z,f+z+.025f,20);}
 for(float x:{7.3f,7.42f}){m.pipes.push_back({{x,5.3f},{x,14.8f},f+2.1f,.035f,f+2.1f,1});m.pipes.push_back({{x,5.3f},{x,5.3f},f+2.1f,.035f,ceil,1});}
 for(float y:{6.f,9.f,12.f,14.4f})wall(m,7.212f,y-.02f,7.47f,y+.02f,f+2.03f,f+2.065f,8);
 wall(m,7.2f,5.2f,7.48f,14.8f,f+.12f,f+2.3f,30);                                           // nobody climbs the plant side
 wall(m,5.f,5.f,7.2f,5.2f,f,up,28);wall(m,5.f,4.99f,7.2f,5.f,f+.08f,f+2.2f,3);
 // the bay: the pulled unit's coil on the back, frost, the drip tray, the hole above
 wall(m,6.05f,8.55f,6.1f,9.85f,f+.9f,f+2.1f,8);for(float z=1.f;z<2.05f;z+=.12f)wall(m,6.02f,8.6f,6.05f,9.8f,f+z,f+z+.04f,20);
 wall(m,5.75f,8.5f,6.1f,9.9f,f,f+.05f,20);wall(m,6.f,8.45f,6.1f,9.95f,f+2.18f,f+2.3f,22);
 wall(m,5.f,8.4f,6.1f,10.f,f+2.29f,f+2.3f,8);
 for(float y:{8.75f,9.6f})wall(m,6.04f,y,6.1f,y+.05f,f+2.1f,f+2.29f,8);
 m.creatureSpawns.push_back({CreatureKind::Mutant,{5.55f,9.2f},-999,1,0,stateId("medd_morgue_dark")});   // subject 40
 // drawer 9's sheet, and his prints across the aisle to the bay
 wall(m,3.f,6.45f,3.95f,7.3f,f,f+.012f,39);wall(m,3.6f,6.5f,3.62f,7.1f,f+.012f,f+.86f,39);
 ddPrints(m,{{3.75f,6.95f},{4.05f,7.25f},{4.3f,7.6f},{4.15f,7.95f},{4.4f,8.3f},{4.7f,8.6f},{4.95f,8.9f},{5.25f,9.1f}},f);
 ddBlood(m,3.05f,6.6f,3.4f,6.9f,f);
 // the hoist over the drawer aisle, its sling hooked up
 wall(m,3.43f,3.4f,3.57f,14.4f,f+2.6f,f+2.72f,8);for(float y:{3.6f,7.f,10.4f,14.2f})wall(m,3.47f,y,3.53f,y+.06f,f+2.72f,f+2.8f,8);
 wall(m,3.38f,11.9f,3.62f,12.3f,f+2.48f,f+2.6f,18);wall(m,3.48f,12.07f,3.52f,12.13f,f+2.06f,f+2.48f,8);wall(m,3.45f,11.7f,3.55f,12.5f,f+2.f,f+2.06f,18);
 // the east aisle: one that would not fit in a drawer
 ddTrolley(m,8.1f,13.4f,f,false,true);
 kit::prop(m,model::trash_can,8.55f,5.6f,f,.4f);
 extinguisher(m,9.f,6.2f,f,ddW);
 // the knocking, from a closed drawer at the head of the east aisle, while you walk up to it
 ddSequence(m,"medd_knock","medd_knock_cycled",11000,11000,true);
 for(int t:{0,380,700,1100})cue(m,"medd_knock",t,t%2?Sound::Metal3:Sound::Metal2,{8.4f,3.1f},"",.5f,f+.9f);
 ddEvent(m,"medd_knock_go",7.2f,5.f,9.f,14.8f,"",1,{ddSet("medd_knock",1)});
 // --- The store goes dark: the tubes blow in turn and he gets up ----------------------------------------
 ddSequence(m,"medd_store_pop","medd_morgue_dark",4000,1300);
 cue(m,"medd_store_pop",0,Sound::Machine,{4.f,6.f},"",.6f,ceil-.2f);
 cue(m,"medd_store_pop",550,Sound::GlassShatter,{6.f,4.2f},"",.7f,ceil-.3f);
 cue(m,"medd_store_pop",900,Sound::GlassShatter,{4.f,8.5f},"",.75f,ceil-.3f);
 cue(m,"medd_store_pop",1250,Sound::GlassShatter,{4.f,12.6f},"",.85f,ceil-.3f);
 cue(m,"medd_store_pop",1500,Sound::JunkMetal,{5.4f,9.2f},"",.9f,f+1.f);
 ddEvent(m,"medd_store_pop_go",3.f,11.4f,5.f,13.6f,"medd_morgue_dark",0,{ddSet("medd_knock",0),ddSet("medd_store_pop",1)});
 ddEvent(m,"medd_morgue_blackout",0,0,24,24,"med_blackout",1,{ddSet("medd_morgue_dark",1)});
 // Back after the transfer in Surgery: the store's tubes come back with the ward (they never
 // blew), so the pop can still take them as you pass. Before any blackout it is a no-op.
 ddEvent(m,"medd_morgue_relight",0,0,24,24,"medd_power_back",1,{ddSet("medd_morgue_dark",0)});
 m.darkZones.push_back({1.f,1.f,9.f,14.8f,-1000,1000,stateId("medd_morgue_dark"),1,false,true});
 m.darkZones.push_back({0,0,24,24,-1000,1000,stateId("med_blackout"),1,false,true});
 // --- Receiving: the stair foot, the lift, book-in, the scale ------------------------------------------
 wall(m,9.2f,7.8f,9.33f,9.45f,f,f+2.22f,28);wall(m,9.33f,8.6f,9.37f,8.9f,f+.95f,f+1.25f,8);   // the cold-room door, slid back
 wall(m,9.2f,7.7f,9.32f,11.1f,f+2.24f,f+2.3f,8);
 wall(m,13.55f,8.5f,14.85f,9.5f,f,f+.07f,8);wall(m,13.6f,8.55f,14.8f,9.45f,f+.07f,f+.08f,20);   // the body scale
 wall(m,14.85f,8.85f,14.95f,9.15f,f,f+1.f,20);wall(m,14.8f,8.8f,15.f,9.2f,f+1.f,f+1.3f,20);wall(m,14.79f,8.88f,14.8f,9.12f,f+1.05f,f+1.25f,9);
 ddTrolley(m,13.4f,13.85f,f,true,true);
 ddTrolley(m,10.f,12.9f,f,false);
 wall(m,9.75f,12.3f,10.25f,13.5f,f+.82f,f+.9f,8);                                         // an empty bag on it
 ddTrolley(m,15.35f,13.3f,f,false);                                                // parked for the next one: the bags on it
 for(int i=0;i<4;++i){const float z=f+.82f+i*.035f;wall(m,15.12f-i*.01f,12.55f+i*.04f,15.58f-i*.005f,13.75f-i*.03f,z,z+.035f,8);}
 kit::prop(m,model::office_paper_sheet,15.3f,14.f,f+.82f,.2f);
 // the bag skip beside it
 {const float y0=14.52f;wall(m,15.f,y0,15.62f,y0+.58f,f,f+.04f,8);for(float x:{15.03f,15.55f})for(float y:{y0+.03f,y0+.51f})wall(m,x,y,x+.04f,y+.04f,f+.04f,f+.95f,20);
  wall(m,15.03f,y0+.03f,15.59f,y0+.55f,f+.3f,f+.84f,8);for(float y:{y0,y0+.54f})wall(m,15.f,y,15.62f,y+.04f,f+.92f,f+.95f,20);for(float x:{15.f,15.58f})wall(m,x,y0+.04f,x+.04f,y0+.54f,f+.92f,f+.95f,20);}
 ddFix(m,model::med_wall_clock,15.2f,15.19f,f+2.15f,.236f,.02f,.233f,ddN,false);
 for(float y:{10.f,10.9f})floorLine(m,9.2f,y,12.f,y+.04f,f,8);
 floorLine(m,10.6f,14.9f,14.6f,15.f,f,13);
 ddFix(m,model::med_floor_drain,12.2f,11.4f,f,.5f,.5f,.01f,0,false);
 // --- The post-mortem room --------------------------------------------------------------------------
 for(float y:{16.9f,19.3f,21.7f}){ddModel(m,model::med_autopsy_table,5.f,y,f,0,false);wall(m,3.95f,y-.42f,6.05f,y+.42f,f,f+.9f,30);ddFix(m,model::med_floor_drain,6.55f,y,f,.5f,.5f,.01f,0,false);}
 for(auto [y,dy,s]:{std::tuple{16.9f,.06f,.93f},{19.3f,-.05f,.9f},{21.7f,.03f,.95f}})ddCorpse(m,5.05f,y,f+.88f,ddW+dy,s);   // heads to the door
 ddBlood(m,6.1f,18.9f,6.9f,19.6f,f);
 for(float y:{18.1f,20.5f}){ddFix(m,model::med_scrub_sink,1.268f,y,f,.488f,.536f,1.127f,ddE,true);ddCap(m,1.f,y-.26f,1.55f,y+.26f,f+1.2f,ceil);}   // nobody stands in a sink
 ddFix(m,model::med_supply_cabinet,1.222f,22.3f,f,1.1f,.444f,1.7f,ddE,true);
 for(float x:{6.6f,7.1f})ddFix(m,x>7.f?model::med_xray_viewer_chest:model::med_xray_viewer_skull,x,15.05f,f+1.3f,.408f,.1f,.5f,ddS,false);
 wall(m,8.9f,15.6f,9.6f,18.8f,f,f+.08f,8);wall(m,8.92f,15.6f,9.6f,18.8f,f+.08f,f+.86f,28);wall(m,8.86f,15.58f,9.6f,18.82f,f+.86f,f+.9f,20);
 ddCap(m,8.86f,15.58f,9.6f,18.82f,f+1.46f,ceil);   // head-high over the counter and its scale: nobody climbs up beside the specimens
 for(float y:{16.f,16.25f,17.6f})kit::prop(m,model::med_specimen_jar,9.3f,y,f+.9f,y);
 wall(m,9.f,16.7f,9.45f,17.25f,f+.9f,f+.92f,20);for(float y=16.78f;y<17.2f;y+=.1f)wall(m,9.08f,y,9.38f,y+.02f,f+.92f,f+.935f,8);   // the tray, laid out
 wall(m,9.5f,18.f,9.6f,18.1f,f+1.5f,f+1.9f,20);wall(m,9.38f,18.02f,9.6f,18.08f,f+1.5f,f+1.53f,20);   // the hanging scale: bracket, arm,
 m.pipes.push_back({{9.4f,18.05f},{9.4f,18.05f},f+1.33f,.006f,f+1.5f,8});wall(m,9.3f,17.9f,9.5f,18.2f,f+1.3f,f+1.33f,20);   // chain and pan
 terminal(m,{9.25f,18.45f},"POST-MORTEM ROOM","FROM INTERNAL INVESTIGATIONS: NO POST-MORTEMS ON TIER 3. CERTIFY, BAG, DRAWER. NO FILMS.",
  "31 AND 40 ARE NOT TO BE OPENED. THE THEATRE HAS ALREADY TAKEN WHAT IT WANTED.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.9f;m.terminals.back().yaw=ddW;
 ddFix(m,model::lab_specimen_cabinet,9.23f,22.1f,f,1.34f,.74f,2.19f,ddW,true);
 kit::prop(m,model::med_crash_cart,6.95f,20.35f,f,.15f);
 kit::prop(m,model::med_vitals_monitor_dead,6.95f,20.3f,f+.809f,.6f);                    // brought down with them, off
 // --- The quarantine lock ---------------------------------------------------------------------------
 wall(m,9.8f,15.4f,11.8f,15.42f,f,f+2.1f,27);wall(m,9.8f,15.4f,9.82f,17.2f,f,f+2.1f,27);
 wall(m,10.f,15.6f,11.8f,17.2f,f,f+.03f,27);ddFix(m,model::med_floor_drain,10.9f,16.4f,f+.03f,.5f,.5f,.01f,0,false);
 m.pipes.push_back({{10.9f,16.3f},{10.9f,16.3f},f+2.25f,.015f,ceil,5});m.pipes.push_back({{10.9f,16.3f},{10.9f,16.3f},f+2.17f,.08f,f+2.25f,5});
 ddTrack(m,{11.8f,15.4f},{11.8f,17.2f},f,ceil);ddCurtain(m,15.4f,16.4f,11.8f,false,f);ddBunch(m,11.8f,17.05f,f,false);
 for(float y:{21.25f,22.1f})ddFix(m,model::psx_locker,10.12f,y,f,.64f,.85f,2.42f,0,true);
 bench(m,13.6f,16.3f,f);
 for(float x:{9.75f,14.f})wall(m,x,22.8f,x+.25f,23.f,f,f+2.8f,18);
 wall(m,9.75f,22.8f,14.25f,23.f,f+2.6f,f+2.8f,18);
 for(float y:{22.2f,22.5f})floorLine(m,10.f,y,14.f,y+.12f,f,13);
 taped(m,{15.f,23.f},f+1.5f,ddN,Readable::Note,"QUARANTINE DOOR / MORTUARY 2","HELD SHUT 05:40 BY THE MEDICAL DIRECTOR. RELEASE: HIS OFFICE, IN SURGERY, PAST THE WARD.",
  "NOBODY OPENS THIS FROM HERE. NOT EVEN US. -NIGHT PORTER");
 ddBody(m,"medd_morgue_clock",ActorVisual::Worker,{12.6f,21.5f},f,1.6f);                    // the night porter, reaching for the door
 ddBlood(m,12.f,21.f,13.3f,22.1f,f);ddBlood(m,13.3f,20.4f,13.8f,20.8f,f);
 // his hands on the frame posts either side of the door, and where they slid down
 for(auto [x,z]:{std::pair{9.84f,1.32f},{9.86f,1.f},{14.07f,1.2f}}){wall(m,x,22.792f,x+.09f,22.8f,f+z,f+z+.1f,19);
  for(int k=0;k<4;++k){const float u=x+.004f+k*.022f;wall(m,u,22.792f,u+.014f,22.8f,f+z+.1f,f+z+.15f+(k==1||k==2?.015f:0),19);}}
 wall(m,14.1f,22.792f,14.15f,22.8f,f+.62f,f+1.2f,19);wall(m,9.88f,22.792f,9.92f,22.8f,f+.75f,f+1.f,19);
 kit::prop(m,model::item_walkie,14.7f,21.9f,f,1.2f);
 // --- The office: the register, the book-in window ------------------------------------------------------
 ddFix(m,model::desk,17.2f,10.6f,f,1.4f,.7f,.75f,ddE,true);
 kit::prop(m,model::office_chair,17.95f,10.55f,f,ddE+.15f);
 kit::prop(m,model::crt_monitor,17.05f,10.25f,f+.75f,ddE);
 kit::prop(m,model::keyboard,17.35f,10.3f,f+.75f,ddE);
 const int officeLamp=int(m.clutterSpawns.size());kit::prop(m,model::lab_desk_lamp,17.18f,11.02f,f+.75f,ddE);
 kit::prop(m,model::mug,17.4f,11.15f,f+.75f,0);
 terminal(m,{17.3f,10.8f},"MORTUARY 2 / REGISTER","05:31 SUBJECT 40 IN FROM THEATRE 1. CERTIFIED 05:19, DIRECTOR. DRAWER 9. NO P.M. TONIGHT.",
  "05:40 THE DIRECTOR HAS SHUT THE QUARANTINE DOOR. RELEASE FROM HIS OFFICE ONLY.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.75f;m.terminals.back().yaw=ddE;
 for(float x:{18.4f,19.4f})ddFix(m,model::med_records_cabinet,x,7.047f,f,.9f,.493f,1.f,ddS,true);
 for(int i=0;i<4;++i)kit::prop(m,model::office_file_binder,18.1f+i*.12f,7.05f,f+1.f,0);
 shelf(m,{22.75f,9.6f},kPi*.5f,0);
 for(auto [y,t]:{std::pair{9.15f,0},{10.f,0},{9.3f,1},{9.9f,2}})kit::prop(m,model::prop_cardboard_box,22.75f,y,f+(t==0?.306f:t==1?.972f:1.656f),ddE,.62f);
 workstation(m,20.4f,10.9f,f,1,2);
 ddFix(m,model::water_cooler,22.6f,7.3f,f,.35f,.35f,1.1f,ddW,true);
 ddFix(m,model::med_wall_clock,20.6f,6.81f,f+2.1f,.236f,.02f,.233f,ddS,false);
 // --- The plant room: the store's refrigeration --------------------------------------------------------
 equipmentPad(m,17.f,1.4f,22.6f,4.1f,f);
 for(float x:{18.2f,20.4f}){ddModel(m,model::pwr_circulation_pump,x,2.4f,f+.1f,0,true);
  m.pipes.push_back({{x,2.4f},{x,2.4f},f+.49f,.05f,f+2.55f,1});}
 for(float x:{18.2f,20.4f})m.pipes.push_back({{x,2.4f},{16.f,2.4f},f+2.55f,.05f,f+2.55f,1});
 ddModel(m,model::pwr_control_cabinet,22.78f,5.3f,f,ddW,true);
 ddModel(m,model::pwr_duct_fan_unit,17.f,5.5f,f,ddE,true);
 for(float x:{19.f,19.4f})ddFix(m,model::pwr_breaker_panel,x,1.051f,f+1.3f,.183f,.102f,.457f,ddS,false);
 wall(m,18.f,4.4f,19.2f,5.f,f,f+.004f,8);
 // --- The night porter's room ----------------------------------------------------------------------------
 ddFix(m,model::med_ward_bed,22.5f,13.64f,f,1.984f,.944f,.842f,-kPi*.5f,true);
 ddFix(m,model::wooden_table,19.8f,16.7f,f,1.367f,.724f,.78f,0,true);
 ddFix(m,model::office_armchair_green,20.f,14.9f,f,1.399f,1.364f,1.192f,0,true);
 const int porterTv=int(m.clutterSpawns.size());kit::prop(m,model::cctv_tv,20.05f,16.75f,f+.78f,ddN);
 kit::prop(m,model::coffee_machine,19.3f,16.85f,f+.78f,ddN);
 kit::prop(m,model::mug,19.6f,16.5f,f+.78f,0);
 terminal(m,{19.75f,16.5f},"NIGHT PORTER / LOG","06:02 KNOCKING IN 9. NOT OPENING IT. RANG THE DIRECTOR. HE SAYS LEAVE IT SHUT.",
  "06:15 IT STOPPED. 06:40 9 IS OPEN AND THE STORE LIGHTS ARE GOING. SOMETHING ON THE STAIR. TRYING THE QUARANTINE DOOR.");
 m.terminals.back().readable=Readable::Note;m.terminals.back().surface=.78f;m.terminals.back().yaw=ddN;
 for(float y:{14.f,14.86f})ddFix(m,model::psx_locker,16.32f,y,f,.64f,.85f,2.42f,0,true);
 // --- Suit-up room -------------------------------------------------------------------------------------
 for(float x:{18.4f,20.6f}){ddFix(m,model::med_washroom_stall,x,22.229f,f,2.13f,1.542f,2.4f,ddN,true);ddStallFront(m,x,21.456f,false,-1,f,false);}
 ddFix(m,model::med_scrub_sink,22.732f,19.2f,f,.488f,.536f,1.127f,ddW,true);
 ddFix(m,model::med_first_aid_box,22.94f,20.2f,f+1.3f,.367f,.12f,.349f,ddW,false);
 bench(m,19.4f,19.f,f);
 // the suits are gone: three lockers, the peg rail empty, the carton they came in
 for(int i=0;i<3;++i)ddFix(m,model::psx_locker,16.51f+i*.86f,17.92f,f,.64f,.85f,2.42f,-kPi*.5f,true);
 wall(m,19.f,17.6f,21.8f,17.65f,f+1.88f,f+1.92f,20);
 for(float x:{19.3f,19.9f,20.5f,21.1f}){wall(m,x-.015f,17.65f,x+.015f,17.77f,f+1.885f,f+1.915f,8);wall(m,x-.022f,17.77f,x+.022f,17.8f,f+1.88f,f+1.95f,8);}
 taped(m,{21.5f,17.6f},f+1.5f,ddS,Readable::Note,"SUIT-UP / MORTUARY 2","NO SUIT, NO QUARANTINE DOOR. HOOD, GLOVES, TAPE AT THE WRISTS. FOUR ON THE PEGS AT ALL TIMES.",
  "04:10 ALL FOUR SIGNED OUT TO THE QUARANTINE WARD. NONE BACK.");
 kit::prop(m,model::prop_cardboard_box_open,20.2f,18.05f,f,.2f,.7f);
 // The stalls' plumbing boxed in at both ends of the row, painted with the room, a panel on each.
 ddCap(m,17.32f,21.46f,21.68f,23.f,f+2.42f,up);                                                         // nobody perches on the stall tops
 for(auto [x1,x2]:{std::pair{16.f,17.32f},{21.68f,23.f}}){wall(m,x1,21.46f,x2,23.f,f,up,3);
  const float c=(x1+x2)*.5f;wall(m,c-.35f,21.44f,c+.35f,21.46f,f+.3f,f+1.f,20);}
 // By them: the bin the used suits went in, empty but for one glove, its lid back.
 {const float x=22.4f,y=20.75f;wall(m,x-.3f,y-.3f,x+.3f,y+.3f,f+.12f,f+.16f,20);
  for(float dx:{-.28f,.25f})for(float dy:{-.28f,.25f})wall(m,x+dx,y+dy,x+dx+.03f,y+dy+.03f,f+.06f,f+.9f,20);
  wall(m,x-.27f,y-.27f,x+.27f,y+.27f,f+.16f,f+.2f,39);wall(m,x-.12f,y-.05f,x+.04f,y+.08f,f+.2f,f+.23f,18);}
 kit::prop(m,model::prop_cardboard_box,22.55f,19.82f,f,ddW+.1f,.6f);
 for(auto [x,y,yaw]:{std::tuple{19.6f,18.3f,.4f},{20.9f,18.6f,2.1f}})kit::prop(m,model::office_paper_sheet,x,y,f,yaw);
 // --- Light (24): cold tubes under the slab; three reds and the door's own lamps in the dark --------------
 m.lights.push_back(lamp({10.f,1.6f},up+2.35f,.6f,6.5f,4800,.05f,WorldLightMount::Wall,0));      // landing
 m.lights.push_back(lamp({11.f,6.95f},-11.35f,.5f,6.f,4800,.05f,WorldLightMount::Wall,-kPi*.5f));  // on the bulkhead, over the flight
 for(auto [x,y]:{std::pair{12.5f,9.2f},{12.5f,13.2f}})m.lights.push_back(ddTube(x,y,ceil,.5f,5.5f,5400,.05f));
 for(auto [x,y,fl]:{std::tuple{6.f,4.f,0.f},{4.f,8.4f,.3f},{4.f,12.6f,0.f}})m.lights.push_back(ddSw(ddTube(x,y,ceil,.62f,6.f,6500,.06f,fl),"medd_morgue_dark",true));
 for(float y:{16.9f,19.3f,21.7f}){auto l=ddTube(5.f,y,ceil,.65f,6.f,6200,.05f,y<17.f?.12f:0.f);l.yaw=kPi*.5f;m.lights.push_back(l);}
 for(float y:{17.6f,21.f})m.lights.push_back(ddTube(12.8f,y,ceil,.45f,5.5f,5600,.05f));
 m.lights.push_back(ddTube(19.6f,9.4f,ceil,.4f,5.f,4600,.05f));
 m.lights.push_back(lamp({17.18f,10.98f},f+1.12f,.35f,2.5f,3000,0,WorldLightMount::Bare));m.lights.back().prop=officeLamp;
 m.lights.push_back(ddTube(19.6f,3.8f,ceil,.4f,5.f,5000,.15f,.2f));
 m.lights.push_back(ddTube(19.6f,15.f,ceil,.35f,4.5f,3600,0));
 m.lights.push_back(lamp({20.05f,16.5f},f+1.f,.22f,2.5f,7500,.2f,WorldLightMount::Bare));m.lights.back().prop=porterTv;
 m.lights.push_back(ddTube(19.6f,20.3f,ceil,.45f,5.f,5400,.06f));
 m.lights.push_back(lamp({23.f,20.9f},f+2.15f,.42f,4.5f,5000,.05f,WorldLightMount::Wall,kPi));                 // the suit-up's wall lamp, past the stalls
 for(auto& l:m.lights)if(!l.enabledState)l=ddSw(l,"med_blackout",true);
 // The reds: over the store doorway at the stair foot, over drawer 9 (the store's own emergency lamp), the door's interlock.
 m.lights.push_back(ddSw(lamp({9.22f,10.2f},f+2.55f,.22f,4.5f,1100,-.45f,WorldLightMount::Wall,0),"med_blackout",false));
 m.lights.push_back(ddSw(lamp({3.f,5.8f},f+2.45f,.13f,3.5f,1100,-.45f,WorldLightMount::Wall,0),"medd_morgue_dark",false));
 for(float x:{14.55f,15.1f}){wall(m,x-.09f,22.9f,x+.09f,23.f,f+2.14f,f+2.36f,8);wall(m,x-.06f,22.86f,x+.06f,22.9f,f+2.19f,f+2.31f,x<15.f?19:23);}   // the interlock lamps
 m.lights.push_back(ddSw(lamp({14.55f,22.8f},f+2.25f,.12f,2.2f,1000,-.45f,WorldLightMount::Bare),"med_morgue_release",true));
 m.lights.push_back(ddSw(lamp({15.1f,22.8f},f+2.25f,.14f,2.2f,4300,.5f,WorldLightMount::Bare),"med_morgue_release",false));
 // --- Rewards and names -----------------------------------------------------------------------------
 m.pickupSpawns={{{19.4f,19.f},PickupKind::Health,f+.51f},{{22.5f,14.2f},PickupKind::Ammo,f+.842f},{{17.6f,11.f},PickupKind::Ammo,f+.75f},{{9.3f,16.6f},PickupKind::Health,f+.9f}};
 m.pickupSpawns.push_back({{17.f,14.45f},PickupKind::HandgunAmmo,f});        // 9 mm out of the night porter's locker
 m.pickupSpawns.push_back({{14.35f,21.55f},PickupKind::HandgunAmmo,f});      // 9 mm beside his radio, where he went down at the door
 m.clutterSpawns.insert(m.clutterSpawns.end(),{{0,{11.2f,9.f},f},{0,{14.f,11.8f},f,1.3f},{2,{19.7f,16.95f},f+.78f,.4f},{0,{7.6f,10.2f},f,2.f},{0,{13.f,18.6f},f,.6f}});
 zone(m,"MEDICAL / MORTUARY / STAIR",9.f,0,16.f,7.f);
 zone(m,"MEDICAL / MORTUARY / RECEIVING",9.2f,7.f,15.8f,15.2f);
 zone(m,"MEDICAL / MORTUARY / BODY STORE",1.f,1.f,9.f,14.8f);
 zone(m,"MEDICAL / MORTUARY / POST-MORTEM",1.f,15.f,9.6f,23.f);
 zone(m,"MEDICAL / MORTUARY / QUARANTINE LOCK",9.8f,15.4f,15.8f,24.f);
 zone(m,"MEDICAL / MORTUARY / OFFICE",16.f,6.8f,23.f,12.4f);
 zone(m,"MEDICAL / MORTUARY / PLANT",16.f,1.f,23.f,6.6f);
 zone(m,"MEDICAL / MORTUARY / NIGHT PORTER",16.f,12.6f,23.f,17.4f);
 zone(m,"MEDICAL / MORTUARY / SUITS",16.f,17.6f,23.f,23.f);
 zone(m,"MEDICAL / MORTUARY",0,0,24,24);
 recessedFittings(m);freightFinish(m,-1);
 return p;
}
}
void buildChapterMedicalDeep(CampaignMapTable& maps){
 maps[77]=makeMedicalSurgery();
 maps[78]=makeMedicalMorgue();
}
}

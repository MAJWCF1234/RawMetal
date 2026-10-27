// MAINTENANCE DISTRICT (chapter 1, manifest 33-35): chunks 53, 54, 55, 56.
// A breathing valley between the plant's water works and the restricted
// perimeter. B shift lived and worked here: the district hall (53), Central
// Stores west of it (54), the Fabrication Shop south (55) and the first-aid
// outpost east (56). The Cleanup Force called a headcount in Central Stores;
// the workers who went never came back. One who did not go is still hiding in
// the aid station. The way on is the Operations shutter in the Fabrication
// Shop's south wall, its drive pin sheared: bar stock from Stores, the lathe,
// then the coupling (maint_shutter_pin).
// See docs/Design/descent-campaign-20261025.md for the brief.
#include "CampaignChapters.h"
#include "CampaignKit.h"
namespace retro {
namespace {
using namespace kit;

// --- District kit -------------------------------------------------------------------------
// Fixture base relative to the layer-0 floor.
float mntRel(const Map& m,float z){return z-m.layers.front().elevation;}
void mntFix(Map& m,int model,Vec2 p,float z,float w,float d,float h,float yaw,bool solid){m.fixtures.push_back({model,p,mntRel(m,z),w,d,h,yaw,solid});}
// A painted block partition on a concrete kerb.
void mntWall(Map& m,float x1,float y1,float x2,float y2,float f,float top,int material=3){
 wall(m,x1,y1,x2,y2,f,top,material);
 wall(m,x1-.015f,y1-.015f,x2+.015f,y2+.015f,f,f+.12f,15);
}
// The wall over a swinging door: the renderer draws the leaf, posts and head
// up to 2.55 m; the partition closes above it.
void mntSwingDoor(Map& m,float x1,float x2,float y,float f,float top,float open,float swing,int material=3){
 wall(m,x1,y-.1f,x2,y+.1f,f+2.55f,top,material);
 Door d{x1,x2,y,open};d.swinging=true;d.swing=swing;d.opening=open>0;m.doors.push_back(d);
}
// An open doorway through a wall that runs along y: frame posts and head.
void mntDoorwayY(Map& m,float y1,float y2,float x1,float x2,float f,float top,int material=3){
 for(float y:{y1-.1f,y2})wall(m,x1-.03f,y,x2+.03f,y+.1f,f,f+2.3f,12);
 wall(m,x1-.03f,y1-.1f,x2+.03f,y2+.1f,f+2.2f,f+2.3f,12);
 wall(m,x1,y1,x2,y2,f+2.3f,top,material);
}
// A glazed screen: dado, glass between steel mullions, sill and head wall.
void mntGlazed(Map& m,float x1,float y1,float x2,float y2,float f,float top,int material=3){
 wall(m,x1,y1,x2,y2,f,f+.95f,material);
 const bool alongX=x2-x1>y2-y1;const float cx=(x1+x2)*.5f,cy=(y1+y2)*.5f,len=alongX?x2-x1:y2-y1;
 if(alongX)wall(m,x1,cy-.01f,x2,cy+.01f,f+.95f,f+2.25f,17);else wall(m,cx-.01f,y1,cx+.01f,y2,f+.95f,f+2.25f,17);
 const int bays=std::max(1,int(len/1.3f+.5f));
 for(int i=0;i<=bays;++i){float t=(alongX?x1:y1)+len*i/bays;t=std::min(t,(alongX?x2:y2)-.06f);
  if(alongX)wall(m,t,y1,t+.06f,y2,f+.95f,f+2.25f,12);else wall(m,x1,t,x2,t+.06f,f+.95f,f+2.25f,12);}
 wall(m,x1-.02f,y1-.02f,x2+.02f,y2+.02f,f+.93f,f+.97f,12);
 wall(m,x1,y1,x2,y2,f+2.25f,top,material);
}
// A single-tube fluorescent fitting under a slab or roof, lit by a bare lamp.
void mntTube(Map& m,Vec2 p,float under,bool alongY,float intensity,float range,float kelvin,float tint,float flicker=0){
 m.fixtures.push_back({model::med_fluoro_light,p,mntRel(m,under-.127f),.11f,1.2f,.13f,alongY?0.f:kPi*.5f,false});
 // Short chains from the slab to the fitting's ends.
 for(float s:{-.45f,.45f}){Vec2 c=alongY?Vec2{p.x,p.y+s}:Vec2{p.x+s,p.y};m.pipes.push_back({c,c,under-.13f,.008f,under,6});}
 WorldLight l=lamp(p,under-.2f,intensity,range,kelvin,tint,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);
}
// A canteen table with chairs pulled up on both long sides.
void mntTable(Map& m,float x,float y,float f,int seed){
 mntFix(m,model::wooden_table,{x,y},f,1.367f,.724f,.78f,0,true);
 const float dx[]={-.36f,.36f};
 for(int i=0;i<2;++i){float jitter=((seed+i)%3-1)*.12f;
  mntFix(m,model::wooden_chair,{x+dx[i]+jitter*.5f,y-.74f},f,.417f,.525f,.95f,jitter,true);
  if((seed+i)%4!=3)mntFix(m,model::wooden_chair,{x+dx[i]-jitter*.4f,y+.74f},f,.417f,.525f,.95f,kPi-jitter,true);}
}
// A run of steel lockers against a wall; `face` is the direction the doors open to.
void mntLockers(Map& m,float x,float y,int count,Vec2 step,float f,float face){
 // The locker model's doors are on its local +x side.
 for(int i=0;i<count;++i)mntFix(m,model::pwr_locker_bank,{x+step.x*i,y+step.y*i},f,.87f,.85f,2.42f,face,true);
}
// A direction board: a steel plate hung on two drop rods from the roof, or,
// with roof below the board, standing on two legs from the floor.
void mntHangingBoard(Map& m,float x,float y,float bottom,float roof,const char* title,const char* subtitle,float width,float yaw){
 const bool facesY=std::fabs(std::cos(yaw))>.5f;const float n=std::cos(yaw)>0||std::sin(yaw)>.5f?1.f:-1.f;
 // Backing plate behind the face (the sign hangs on its front).
 if(facesY)wall(m,x-width*.5f-.05f,y-n*.06f-.1f,x+width*.5f+.05f,y-n*.06f+.1f,bottom-.05f,bottom+.74f,2);
 else wall(m,x-n*.06f-.1f,y-width*.5f-.05f,x-n*.06f+.1f,y+width*.5f+.05f,bottom-.05f,bottom+.74f,2);
 for(float s:{-width*.4f,width*.4f}){Vec2 c=facesY?Vec2{x+s,y-n*.06f}:Vec2{x-n*.06f,y+s};
  if(roof>bottom)m.pipes.push_back({c,c,bottom+.74f,.02f,roof,6});
  else wall(m,c.x-.04f,c.y-.04f,c.x+.04f,c.y+.04f,m.layers.front().elevation,bottom-.05f,12);}
 m.signs.push_back({{x+(facesY?0.f:n*.05f),y+(facesY?n*.05f:0.f)},bottom,width,.65f,yaw,title,subtitle});
}
// Plant signs are amber; Cleanup stencils red; fire and first aid green;
// what the survivors chalked on plywood is off-white.
constexpr std::uint32_t mntAmber=0xffca994du,mntRed=0xffc23a2eu,mntGreen=0xff3f9a5au,mntChalkInk=0xffdcd8c6u,mntBlue=0xff2f6db8u;
// A sign on its own steel plate, (x,y) on the face it hangs on; it faces (sin yaw, cos yaw).
void mntSign(Map& m,float x,float y,float z,const char* title,const char* subtitle,float width,float yaw,std::uint32_t accent=mntAmber){
 wallSign(m,x,y,z,title,subtitle,width,yaw);m.signs.back().accent=accent;
}
// A plywood sheet nailed to a face with a message chalked on it. (x,y) is the
// face; the sheet stands 4 cm proud of it.
void mntChalk(Map& m,float x,float y,float z,const char* title,const char* subtitle,float width,float yaw){
 const float fx=std::sin(yaw),fy=std::cos(yaw),ax=std::fabs(std::cos(yaw)),ay=std::fabs(std::sin(yaw));
 const float cx=x+fx*.02f,cy=y+fy*.02f,hx=ax*(width*.5f+.07f)+ay*.02f,hy=ay*(width*.5f+.07f)+ax*.02f;
 wall(m,cx-hx,cy-hy,cx+hx,cy+hy,z-.1f,z+.78f,10);
 m.signs.push_back({{x+fx*.046f,y+fy*.046f},z,width,.65f,yaw,title,subtitle,mntChalkInk});
}
// A worker or a Cleanup man where he fell: a scripted track holding the down pose.
void mntBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){
 m.actorTracks.push_back({stateId(clock),visual,visual==ActorVisual::Worker?1.7f:1.6f,false,false,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}}});
}
// An armed Cleanup man walking an authored beat: he pauses at each post facing
// its bearing and scans either side of it.
void mntGuard(Map& m,const char* clock,const char* squad,int index,ActorVisual visual,GuardSquadRole role,std::vector<ActorPatrolPoint> beat,
              float sight,float dot,int weapon,float speed,StateId alert){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=90;
 const std::string key=std::string(squad)+"_"+std::to_string(index);
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=stateId(squad);ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=dot;ai.recognitionMs=900;ai.searchMs=6000;ai.speed=speed;ai.alertState=alert;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
// A machine control read with E: a terminal inside the footprint of a solid
// fixture (a console or cabinet) draws no computer of its own.
void mntControl(Map& m,Vec2 p,const char* title,const char* a,const char* b,StateId state,StateId prerequisite,float yaw){
 terminal(m,p,title,a,b,0,state,prerequisite);m.terminals.back().control=true;m.terminals.back().yaw=yaw;
}
// A row of pallet racking along x: `bays` bays of 2.6 m sharing their frames,
// 4 m deep from y, `tiers` beam levels 3 m apart. The bay numbered `open` has
// no bottom level: a walk-through under the first beams to the next aisle.
void mntRackRow(Map& m,float x0,float y,float floor,int bays,int tiers,int open,int seed){
 const float base=m.layers.front().elevation,top=floor+tiers*3.f;
 for(int i=0;i<=bays;++i){const float xx=x0+i*2.6f-.08f;
  for(float yy:{y,y+3.84f})wall(m,xx,yy,xx+.16f,yy+.16f,floor,top,12);
  // The end frame: horizontals between the front and back posts, a foot plate.
  for(float z=floor+.35f;z<top-.4f;z+=1.45f)wall(m,xx+.05f,y+.16f,xx+.11f,y+3.84f,z,z+.05f,12);
  for(float yy:{y,y+3.84f})wall(m,xx-.04f,yy-.04f,xx+.2f,yy+.2f,floor,floor+.012f,8);
 }
 for(int i=0;i<bays;++i){const float x=x0+i*2.6f+.08f,w=2.44f;
  for(int tier=0;tier<tiers;++tier){const float z=floor+tier*3.f;
   if(tier==0&&i==open)continue;
   if(tier>0)wall(m,x,y+.1f,x+w,y+3.9f,z+.12f,z+.15f,8);
   // Step beams front and back, orange-red like every racking supplier's.
   wall(m,x,y-.02f,x+w,y+.1f,z+.12f,z+.28f,19);wall(m,x,y+3.9f,x+w,y+4.02f,z+.12f,z+.28f,19);
   const float deck=z+(tier>0?.15f:0.f);
   for(int pallet=0;pallet<2;++pallet){const float yy=y+1.f+pallet*1.95f;const int slot=seed+i*3+tier*2+pallet;
    if(tier>0&&slot%5==0)continue;
    if(tier==0||slot%3==0){cargo(m,x+w*.5f,yy,deck,slot%4!=1,slot%2?kPi:0.f);continue;}
    for(int column=0;column<2;++column){const float xx=x+.62f+column*1.2f,h=slot%3==1?.8f:1.25f;
     m.fixtures.push_back({model::pallet,{xx,yy},deck-base,1.08f,1.2f,.16f,0,true});
     m.fixtures.push_back({column%2?model::packing_crate:model::wooden_crate,{xx,yy},deck+.16f-base,1.f,1.1f,h,(slot+column)%2?kPi:0.f,true});}
   }
  }
  // A top tie at the head of the frames.
  wall(m,x,y-.02f,x+w,y+.1f,top-.12f,top,12);wall(m,x,y+3.9f,x+w,y+4.02f,top-.12f,top,12);
 }
}
// Cantilever bar racking against the west wall: columns every 1.5 m with four
// arms each, the stock lying across the arms from the first column to the last.
void mntBarRack(Map& m,float y1,float y2,float floor,int seed){
 int columns=0;
 for(float y=y1;y<=y2+.01f;y+=1.5f,++columns){
  wall(m,1.f,y-.09f,1.32f,y+.09f,floor,floor+2.95f,18);
  wall(m,1.f,y-.26f,1.7f,y+.26f,floor,floor+.07f,18);
  for(int k=0;k<4;++k){const float z=floor+.42f+k*.66f;
   wall(m,1.32f,y-.05f,2.38f,y+.05f,z,z+.09f,18);wall(m,2.31f,y-.05f,2.38f,y+.05f,z+.09f,z+.17f,18);}
 }
 const float last=y1+(columns-1)*1.5f;
 // Nobody walks in among loaded arms: the rack is solid to the aisle edge.
 wall(m,1.32f,y1-.1f,2.38f,last+.1f,floor,floor+2.95f,CollisionProxyMaterial);
 for(int k=0;k<4;++k){const float z=floor+.51f+k*.66f;float x=1.42f;
  for(int b=0;b<5;++b){const int v=(seed+k*7+b*3)%9;if(v==4&&k>0)continue;
   const float r=.012f+.008f*(v%5);x+=r;if(x+r>2.28f)break;
   m.pipes.push_back({{x,y1},{x,last},z+r,r,z+r,v%3==0?6:v%3==1?1:5});x+=r+.035f;}
 }
}
// A chain-link panel `len` long centred on c along a line: along x at y=`at`,
// or along y at x=`at`. A doorway panel collides only either side of its gap.
void mntMesh(Map& m,bool alongX,float at,float c,float len,float floor,bool doorway){
 const Vec2 p=alongX?Vec2{c,at}:Vec2{at,c};
 mntFix(m,doorway?model::mine_mesh_doorway:model::mine_mesh_fence,p,floor,len,.06f,2.78f,alongX?0.f:kPi*.5f,!doorway);
 if(!doorway)return;
 const float gap=.56f*len/3.19f;
 for(float s:{-1.f,1.f}){const float a=c+s*gap,b=c+s*len*.5f,lo=std::min(a,b),hi=std::max(a,b);
  if(alongX)wall(m,lo,at-.03f,hi,at+.03f,floor,floor+2.78f,CollisionProxyMaterial);
  else wall(m,at-.03f,lo,at+.03f,hi,floor,floor+2.78f,CollisionProxyMaterial);}
}
// A desk whose worker faces `face` (yaw: 0 south, kPi north, kPi/2 east,
// -kPi/2 west): desk, chair, screen, keyboard, phone, sometimes a mug.
void mntDesk(Map& m,float x,float y,float f,float face,int variant){
 const Vec2 p{x,y},d{std::sin(face),std::cos(face)},s{std::cos(face),-std::sin(face)};
 auto at=[&](float u,float v){return p+s*u+d*v;};
 const float b=f-m.layers.front().elevation;
 m.fixtures.push_back({model::desk,p,b,1.4f,.7f,.75f,face,true});
 m.fixtures.push_back({model::office_chair,at(variant%2?.15f:-.1f,-.78f),b,.55f,.55f,.95f,face+kPi+(variant%3-1)*.25f,true});
 m.fixtures.push_back({model::crt_monitor,at(-.1f,.12f),b+.75f,.42f,.4f,.38f,face+kPi,false});
 m.fixtures.push_back({model::keyboard,at(-.1f,-.2f),b+.75f,.45f,.16f,.03f,face+kPi,false});
 m.fixtures.push_back({model::desk_phone,at(.48f,.05f),b+.75f,.2f,.2f,.1f,face+kPi,false});
 if(variant%2)m.fixtures.push_back({model::mug,at(.42f,-.2f),b+.75f,.09f,.09f,.1f,0,false});
 if(variant%3==0)m.fixtures.push_back({model::binder,at(-.52f,.18f),b+.75f,.3f,.08f,.32f,face+kPi,false});
}
// An industrial pendant: a drop rod from the roof to a reflector head, lit.
void mntPendant(Map& m,Vec2 p,float roof,float z,float intensity,float range,float kelvin,float tint,float flicker=0){
 m.pipes.push_back({p,p,z+.32f,.016f,roof,6});
 m.fixtures.push_back({model::pendant_lamp,p,mntRel(m,z),.5f,.5f,.34f,0,false});
 WorldLight l=lamp(p,z-.05f,intensity,range,kelvin,tint,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);
}
// A fluorescent fitting hung on chains from a high roof down to `z`.
void mntHungTube(Map& m,Vec2 p,float roof,float z,bool alongY,float intensity,float range,float kelvin,float tint,float flicker=0){
 m.fixtures.push_back({model::med_fluoro_light,p,mntRel(m,z),.11f,1.2f,.13f,alongY?0.f:kPi*.5f,false});
 for(float s:{-.45f,.45f}){Vec2 c=alongY?Vec2{p.x,p.y+s}:Vec2{p.x+s,p.y};m.pipes.push_back({c,c,z+.12f,.008f,roof,6});}
 WorldLight l=lamp(p,z-.08f,intensity,range,kelvin,tint,WorldLightMount::Bare);l.flicker=flicker;m.lights.push_back(l);
}
// A script action, built in one line.
ScriptAction mntAct(ScriptAction::Type type,StateId id=0,int value=0){ScriptAction a;a.type=type;a.id=id;a.value=value;return a;}
// A sequence started only by setting its timer: its box is off the map.
TimedSequence& mntTimer(Map& m,const char* timer,const char* finish,int duration,int finishAt){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=-2;s.y1=-2;s.x2=-1;s.y2=-1;
 s.durationMs=duration;s.finishAtMs=finishAt;s.soundIntervalMs=0;m.timedSequences.push_back(s);return m.timedSequences.back();
}
// A once-only event over the whole chunk, gated on a state.
ScriptEvent& mntOn(Map& m,const char* id,StateId state,int value=1){
 ScriptEvent e;e.id=stateId(id);e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=state;e.requireValue=value;m.scriptEvents.push_back(e);return m.scriptEvents.back();
}

// MAINTENANCE DISTRICT (manifest 33). The district hall where B shift worked
// out of: a covered street under the plant roof with four blocks on its
// corners. North-west the locker room, north-east the break room where the
// survivors camped, south-west the time office with the shift board nobody
// clocked off, south-east the work-order counter. Heat Exchange is north;
// Central Stores west behind the Cleanup's tape; the aid station east, its
// door barricaded from inside; the Fabrication Shop south, and past it the
// jammed Operations shutter.
std::shared_ptr<const Map> makeMaintenanceDistrict(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[53];m.definition=CampaignChunks[53];
 const float f=-19,roof=-12.f,rt=f+3.2f;
 m.definition.ceiling=roof;m.definition.ambient=.34f;
 m.wallSurface=SurfaceBrick;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceCorrugated;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10,14,f,false,0});openEdge(m,{'W',9,14,f,false,0});openEdge(m,{'E',9,14,f,false,0});openEdge(m,{'S',10,14,f,false,0});
 // --- Portals: concrete headers over the four openings, each named ----------------------------
 wall(m,10,0,14,1,-15.f,roof,15);
 wall(m,0,9,1,14,-15.f,roof,15);
 // The aid station's roof is lower than the hall's: the header closes the step.
 wall(m,23,9,24,14,-15.5f,roof,15);
 wall(m,10,23,14,24,-15.f,roof,15);
 // Steel portal posts stand proud of the wall either side of each opening.
 for(float x:{9.72f,14.f}){wall(m,x,22.72f,x+.28f,23.f,f,-15.f,12);wall(m,x,1.f,x+.28f,1.28f,f,-15.f,12);}
 for(float y:{8.72f,14.f}){wall(m,1.f,y,1.28f,y+.28f,f,-15.f,12);wall(m,22.72f,y,23.f,y+.28f,f,-15.5f,12);}

 // --- NW: locker room and washroom --------------------------------------------------------------
 mntWall(m,8.6f,1,8.8f,8.2f,f,rt);
 mntWall(m,1,8.f,5.6f,8.2f,f,rt);mntWall(m,6.8f,8.f,8.6f,8.2f,f,rt);
 mntSwingDoor(m,5.6f,6.8f,8.1f,f,rt,.85f,-1);
 wall(m,1,1,8.8f,8.2f,rt,rt+.2f,15);
 // Washroom: tiled partition, a stall, two basins under the mirror.
 wall(m,3.5f,1,3.65f,5.2f,f,f+2.5f,27);
 mntFix(m,model::med_washroom_stall,{2.3f,1.85f},f,2.13f,1.54f,2.4f,0,true);
 for(float y:{3.45f,4.45f})mntFix(m,model::med_scrub_sink,{1.3f,y},f,.49f,.54f,1.13f,kPi*.5f,true);
 mntFix(m,model::med_washroom_mirror,{1.03f,3.95f},f+1.15f,1.5f,.04f,1.f,kPi*.5f,false);
 for(float y:{3.45f,4.45f})m.pipes.push_back({{1.02f,y},{1.02f,y},f,.03f,f+.55f,5});
 mntFix(m,model::med_floor_drain,{2.4f,4.f},f,.5f,.5f,.01f,0,false);
 mntFix(m,model::med_first_aid_box,{3.46f,4.6f},f+1.4f,.37f,.12f,.35f,-kPi*.5f,false);
 // Lockers along the north wall and a back-to-back island, benches between.
 mntLockers(m,4.25f,1.46f,5,{.85f,0},f,-kPi*.5f);
 mntLockers(m,4.65f,4.46f,4,{.85f,0},f,kPi*.5f);
 mntLockers(m,4.65f,5.32f,4,{.85f,0},f,-kPi*.5f);
 bench(m,6.f,2.95f,f);bench(m,6.f,6.3f,f);
 // Hard hats left on the lockers, hi-vis on the hooks, one hat dropped.
 for(float x:{4.3f,5.95f,7.6f})mntFix(m,model::mine_hard_hat,{x,1.5f},f+2.42f,.28f,.35f,.15f,x*1.3f,false);
 mntFix(m,model::mine_hard_hat,{7.4f,3.4f},f,.28f,.35f,.15f,2.1f,false);
 for(float y:{2.3f,2.9f,3.5f}){wall(m,8.45f,y-.02f,8.6f,y+.02f,f+1.7f,f+1.74f,2);wall(m,8.5f,y-.22f,8.56f,y+.22f,f+1.08f,f+1.68f,18);}
 mntTube(m,{6.f,3.f},rt,false,.9f,6.f,6200,.45f);
 mntTube(m,{6.f,6.9f},rt,false,.8f,6.f,6200,.45f,.45f);
 mntTube(m,{2.3f,3.9f},rt,true,.6f,4.f,6200,.45f);

 // --- NE: break room, where the survivors camped -----------------------------------------------------
 mntGlazed(m,15.2f,1,15.4f,8.f,f,rt);
 mntWall(m,15.2f,8.f,17.4f,8.2f,f,rt);mntWall(m,18.6f,8.f,23,8.2f,f,rt);
 mntSwingDoor(m,17.4f,18.6f,8.1f,f,rt,1.f,-1);
 wall(m,15.2f,1,23,8.2f,rt,rt+.2f,15);
 // Kitchenette along the north wall: counter, sink, cupboards, coffee, a fridge.
 wall(m,15.4f,1,19.8f,1.62f,f,f+.88f,21);wall(m,15.4f,1,19.85f,1.66f,f+.88f,f+.92f,20);
 wall(m,16.3f,1.12f,16.95f,1.5f,f+.905f,f+.925f,8);
 m.pipes.push_back({{16.62f,1.06f},{16.62f,1.06f},f+.92f,.018f,f+1.18f,5});m.pipes.push_back({{16.62f,1.06f},{16.62f,1.19f},f+1.18f,.018f,f+1.18f,5});
 wall(m,15.4f,1,19.8f,1.38f,f+1.55f,f+2.25f,21);wall(m,15.4f,1.36f,19.8f,1.4f,f+1.55f,f+1.6f,20);
 mntFix(m,model::coffee_machine,{18.3f,1.3f},f+.92f,.35f,.35f,.45f,0,false);
 mntFix(m,model::mug,{18.75f,1.42f},f+.92f,.09f,.09f,.1f,0,false);
 wall(m,18.85f,1.06f,19.45f,1.48f,f+.92f,f+1.24f,20);wall(m,18.9f,1.47f,19.3f,1.49f,f+.98f,f+1.2f,8);
 mntFix(m,model::lab_sample_fridge,{20.3f,1.47f},f,.7f,.85f,1.f,0,true);
 mntFix(m,model::metro_vending,{22.64f,3.f},f,1.5f,.68f,2.38f,kPi*.5f,true);
 mntTable(m,17.9f,4.4f,f,0);mntTable(m,20.4f,4.4f,f,2);
 mntFix(m,model::cctv_tv,{15.75f,7.6f},f+1.98f,.5f,.4f,.42f,kPi*.75f,false);
 wall(m,15.4f,7.3f,16.1f,8.f,f+1.9f,f+1.98f,2);
 mntFix(m,model::med_wall_clock,{21.f,1.03f},f+2.2f,.24f,.02f,.23f,0,false);
 mntFix(m,model::med_notice_board,{21.3f,7.97f},f+1.05f,1.2f,.07f,.9f,kPi,false);
 // The camp: mattresses, a bedroll, the lantern they read by, rations, water.
 mntFix(m,model::med_mattress_stained,{20.5f,7.35f},f,1.9f,.9f,.12f,0,false);
 wall(m,19.9f,6.95f,21.2f,7.75f,f+.12f,f+.16f,23);
 mntFix(m,model::scrap_bedroll,{22.4f,5.9f},f,.92f,1.96f,.18f,0,false);
 mntFix(m,model::scrap_lantern,{19.3f,7.2f},f,.26f,.31f,.77f,0,false);
 for(Vec2 j:{Vec2{22.65f,7.55f},Vec2{22.3f,7.62f},Vec2{22.62f,7.12f}}){wall(m,j.x-.13f,j.y-.13f,j.x+.13f,j.y+.13f,f,f+.42f,20);wall(m,j.x-.04f,j.y-.04f,j.x+.04f,j.y+.04f,f+.42f,f+.5f,19);}
 m.lights.push_back(lamp({19.3f,7.2f},f+.62f,.5f,3.f,2000,-.1f,WorldLightMount::Bare));
 terminal(m,{16.6f,6.6f},"B SHIFT / MESSAGE BOARD","DAY 2. SIX OF US LEFT. WATER FROM THE COOLER. NOBODY GOES TO STORES.","MOVED TO THE AID STATION. KNOCK THREE TIMES. - R. MARSH");m.terminals.back().readable=Readable::Note;
 mntTube(m,{17.9f,4.4f},rt,false,1.f,6.f,3000,0);
 mntTube(m,{20.6f,4.4f},rt,false,.9f,6.f,3000,0);

 // --- SW: time office, the shift board and the PA -------------------------------------------------
 mntWall(m,1,15.8f,2.4f,16.f,f,rt);mntWall(m,3.6f,15.8f,8.8f,16.f,f,rt);
 mntSwingDoor(m,2.4f,3.6f,15.9f,f,rt,.6f,1);
 mntWall(m,8.6f,16.f,8.8f,16.8f,f,rt);mntGlazed(m,8.6f,16.8f,8.8f,22.2f,f,rt);mntWall(m,8.6f,22.2f,8.8f,23,f,rt);
 wall(m,1,15.8f,8.8f,23,rt,rt+.2f,15);
 // The T-card board on the hall face: forty-one cards in IN, none in OUT.
 wall(m,3.6f,15.7f,7.f,15.8f,f+.95f,f+2.45f,20);
 for(float x:{3.6f,6.35f,6.94f})wall(m,x,15.66f,x+.06f,15.8f,f+.95f,f+2.45f,12);
 for(float z:{f+.95f,f+2.39f})wall(m,3.6f,15.66f,7.f,15.7f,z,z+.06f,12);
 for(int row=0;row<6;++row)for(int col=0;col<7;++col){if(row*7+col>=41)break;
  float x=3.76f+col*.36f,z=f+2.2f-row*.2f;wall(m,x,15.67f,x+.22f,15.7f,z,z+.14f,(row*7+col)%5==0?19:(row*7+col)%3==0?18:20);}
 mntFix(m,model::wall_box,{7.45f,15.72f},f+1.4f,.55f,.15f,.6f,kPi,false);
 terminal(m,{7.5f,15.2f},"TIME CLOCK / B SHIFT","CLOCKED IN 22:00: 41. CLOCKED OUT: 0.","LAST PUNCH 03:52, CARD 2207 MARSH R. / NO EXIT.");
 // Inside: the foreman's desk at the window, files, the PA console.
 workstation(m,5.2f,21.6f,f,-1,1);
 for(float y:{17.2f,17.8f,18.4f})mntFix(m,model::file_cabinet_tall,{1.35f,y},f,.5f,.6f,1.3f,kPi*.5f,true);
 mntFix(m,model::bookcase,{1.2f,20.4f},f,.9f,.3f,1.8f,kPi*.5f,true);
 for(float z:{f+.75f,f+1.3f})for(int i=0;i<4;++i)mntFix(m,model::binder,{1.22f,20.1f+i*.18f},z,.3f,.08f,.32f,kPi*.5f,false);
 mntFix(m,model::control_console,{7.9f,18.7f},f,.78f,.49f,1.3f,-kPi*.5f,true);
 terminal(m,{7.7f,20.3f},"DISTRICT PA / LOG","03:05 ALL MAINTENANCE STAFF TO CENTRAL STORES FOR HEADCOUNT.","03:40 REPEAT. BRING YOUR BADGE. AUTH: CLEANUP DETAIL 4.");
 m.terminals.back().yaw=-kPi*.5f;
 m.pipes.push_back({{7.75f,18.6f},{7.75f,18.6f},f+1.3f,.012f,f+1.56f,6});wall(m,7.7f,18.55f,7.8f,18.65f,f+1.62f,f+1.7f,8);
 mntFix(m,model::wall_box,{1.03f,22.2f},f+1.3f,.55f,.12f,.75f,kPi*.5f,false);
 mntTube(m,{4.8f,18.f},rt,false,.9f,6.f,4300,.25f);
 mntTube(m,{4.8f,21.f},rt,false,.9f,6.f,4300,.25f);

 // --- SE: work orders and the district tool crib -------------------------------------------------
 mntWall(m,15.2f,15.8f,20.6f,16.f,f,rt);mntWall(m,21.8f,15.8f,23,16.f,f,rt);
 mntSwingDoor(m,20.6f,21.8f,15.9f,f,rt,0,1);
 mntWall(m,15.2f,16.f,15.4f,18.4f,f,rt);mntWall(m,15.2f,20.f,15.4f,23,f,rt);
 // The issue hatch: counter, open window, steel shutter box overhead.
 wall(m,15.2f,18.4f,15.4f,20.f,f,f+1.f,3);wall(m,14.9f,18.3f,15.6f,20.1f,f+1.f,f+1.06f,20);
 wall(m,15.2f,18.4f,15.4f,20.f,f+2.15f,rt,3);wall(m,15.0f,18.35f,15.2f,20.05f,f+2.15f,f+2.45f,12);
 wall(m,15.2f,15.8f,23,23,rt,rt+.2f,15);
 terminal(m,{14.5f,17.6f},"WORK ORDERS / OPEN","WO 7714: OPERATIONS SHUTTER 3. DRIVE SHEAR PIN SHEARED 02:16.","TURN NEW PIN FROM 40 MM BAR (CENTRAL STORES, RACK B). LATHE 2, FAB SHOP.");m.terminals.back().readable=Readable::Letter;
 m.terminals.back().yaw=-kPi*.5f;
 // Inside: steel shelving, the tool chests, cylinders chained to the wall, a bench and vise.
 for(float y:{17.2f,19.4f})mntFix(m,model::metal_shelf,{22.7f,y},f,1.8f,.5f,1.8f,-kPi*.5f,true);
 for(float x:{17.f,18.f})mntFix(m,model::pwr_tool_chest,{x,22.6f},f,.75f,.45f,.97f,kPi,true);
 for(float x:{19.6f,20.f,20.4f}){mntFix(m,model::gas_cylinder,{x,22.75f},f,.33f,.33f,1.58f,0,true);}
 wall(m,19.4f,22.88f,20.6f,22.94f,f+1.1f,f+1.16f,8);
 wall(m,16.f,16.2f,18.8f,16.9f,f+.86f,f+.92f,10);
 for(float x:{16.1f,18.6f})wall(m,x,16.25f,x+.1f,16.85f,f,f+.86f,12);
 wall(m,16.8f,16.4f,17.1f,16.7f,f+.92f,f+1.06f,8);wall(m,16.62f,16.5f,17.28f,16.6f,f+1.06f,f+1.1f,8);
 mntFix(m,model::stock_cage,{21.5f,20.8f},f,2.4f,1.f,1.1f,kPi*.5f,true);
 mntTube(m,{18.f,18.4f},rt,true,.9f,6.f,4000,.1f);
 mntTube(m,{21.f,21.2f},rt,false,.7f,5.f,4000,.1f);

 // --- The hall: trusses, services, signs, and what was left in the aisles ------------------------
 for(float y:{3.f,7.f,11.f,15.f,19.f,22.f}){wall(m,1,y-.12f,23,y+.12f,roof-.5f,roof,12);wall(m,1,y-.18f,23,y+.18f,roof-.56f,roof-.5f,12);}
 servicePipe(m,{1.2f,1.7f},{22.8f,1.7f},-13.3f,.32f,5);
 servicePipe(m,{1.2f,22.3f},{22.8f,22.3f},-13.3f,.32f,5);
 servicePipe(m,{1.2f,2.4f},{22.8f,2.4f},-13.4f,.09f,4);
 servicePipe(m,{1.2f,21.6f},{22.8f,21.6f},-13.4f,.09f,3);
 // Drops from the supply duct into each block's roof.
 for(float x:{5.f,19.f}){pipeRoute(m,{{x,1.7f,-13.62f},{x,1.7f,rt+.1f}},.16f,5,NoRoof);pipeRoute(m,{{x,22.3f,-13.62f},{x,22.3f,rt+.1f}},.16f,5,NoRoof);}
 // The direction gantry over the crossing.
 // Floor: walkway lines, the crossing box, drain.
 for(float x:{9.3f,14.6f})floorLine(m,x,1.f,x+.1f,23.f,f,14);
 for(float y:{8.7f,15.2f})floorLine(m,1.f,y,23.f,y+.1f,f,14);
 floorLine(m,11.2f,11.2f,12.8f,12.8f,f,8);
 // West: Cleanup tape torn at the stores portal, a barrier dragged aside, drag marks.
 mntFix(m,model::road_block,{2.4f,9.2f},f,1.6f,.5f,.7f,.4f,true);
 for(float y:{8.6f,15.3f})wall(m,1.f,y-.05f,1.1f,y+.05f,f,f+1.05f,12);
 wall(m,1.1f,8.58f,1.9f,8.62f,f+.95f,f+1.03f,13);wall(m,1.1f,15.28f,1.6f,15.32f,f+.95f,f+1.03f,13);
 wall(m,1.86f,8.6f,1.9f,8.64f,f+.4f,f+.95f,13);
 for(int i=0;i<6;++i){float x=7.6f-i*1.1f,y=12.2f+std::sin(i*1.7f)*.3f;floorLine(m,x-.5f,y,x+.4f,y+.12f,f,16);floorLine(m,x-.3f,y+.5f,x+.5f,y+.58f,f,16);}
 mntHangingBoard(m,3.f,14.6f,f+1.f,f,"HEADCOUNT","ALL B SHIFT TO CENTRAL STORES / BRING YOUR BADGE",2.4f,kPi*.5f);
 // East: the survivors' signs, a trolley and a chair left outside the aid station.
 wallSign(m,21.3f,8.255f,f+1.2f,"DON'T SHOOT","PEOPLE IN THE AID STATION / KNOCK",2.f,0);
 mntFix(m,model::med_crash_cart,{21.6f,14.9f},f,.74f,.48f,1.f,.2f,true);
 mntFix(m,model::med_wheelchair,{19.6f,15.f},f,.55f,1.03f,.9f,2.6f,true);
 for(int i=0;i<3;++i)floorLine(m,17.f+i*1.7f,11.45f,17.9f+i*1.7f,11.6f,f,19);
 // South: a parts trolley and plate stock waiting for the fab shop.
 handCart(m,10.2f,19.6f,f);
 mntFix(m,model::pallet,{13.6f,20.6f},f,1.2f,1.15f,.16f,0,true);
 mntFix(m,model::pwr_sheet_steel_stack,{13.6f,20.6f},f+.16f,1.05f,2.f,.08f,kPi*.5f,false);
 // Wall lamps on the block faces light the aisles; high bays light the crossing.
 wallLamp(m,8.82f,4.6f,f+2.6f,0,1.f,7.f);wallLamp(m,15.18f,4.6f,f+2.6f,kPi,.9f,7.f);
 wallLamp(m,8.82f,19.5f,f+2.6f,0,1.f,7.f);
 wallLamp(m,2.2f,8.22f,f+2.7f,kPi*.5f,.9f,7.f);wallLamp(m,21.8f,8.22f,f+2.7f,kPi*.5f,.9f,7.f);
 wallLamp(m,1.5f,15.78f,f+2.4f,-kPi*.5f,.9f,6.f);
 for(Vec2 l:{Vec2{12.f,5.f},Vec2{12.f,13.f},Vec2{12.f,20.5f},Vec2{4.5f,12.f}})m.lights.push_back(lamp(l,roof-.75f,2.f,12.f,3300,0));
 m.lights.push_back(lamp({19.5f,12.f},roof-.75f,1.4f,10.f,4600,.2f));
 // The stores side is darker, lit red where the Cleanup rigged its lamp.
 m.lights.push_back(lamp({1.3f,8.86f},-16.6f,1.1f,6.f,1300,-.3f,WorldLightMount::Wall,0));
 // Health in the washroom.
 m.pickupSpawns.push_back({{2.6f,4.6f},PickupKind::Health,f});
 m.pickupSpawns.push_back({{19.2f,19.5f},PickupKind::Ammo,f});
 m.clutterSpawns.push_back({1,{21.3f,6.6f},f,.3f});m.clutterSpawns.push_back({1,{20.4f,6.4f},f,1.4f});m.clutterSpawns.push_back({2,{18.2f,4.4f},f+.80f});
 m.clutterSpawns.push_back({0,{11.5f,15.9f},f});m.clutterSpawns.push_back({2,{3.2f,12.8f},f});
 zone(m,"MAINTENANCE / LOCKER ROOM",1,1,8.8f,8.2f);
 zone(m,"MAINTENANCE / BREAK ROOM",15.2f,1,23,8.2f);
 zone(m,"MAINTENANCE / TIME OFFICE",1,15.8f,8.8f,23);
 zone(m,"MAINTENANCE / TOOL CRIB",15.2f,15.8f,23,23);
 zone(m,"MAINTENANCE DISTRICT",0,0,24,24);
 freightFinish(m,-1);
 titleCard(m,"MAINTENANCE DISTRICT");
 return p;
}
// CENTRAL STORES (manifest 34). The district's bulk store, west of the hall.
// The storeman's office and issue hatch face the passage in; past the goods
// line two rows of pallet racking flank the main aisle, a small-parts
// mezzanine runs along the north wall, and Rack B, the bar store, stands
// against the far west wall under its own lamp: the 40 mm bar for the
// shutter pin, visible down the aisle from the door. The Cleanup called B
// shift here for a headcount at 03:05. The assembly-point cage by the door
// still holds their lunch tins; drag marks run down the south aisle to the
// scrap returns chute. Detail 4 is still working the floor: one man walks the
// main aisle, one stands over the chute, one walks the south aisle.
// The stealth lesson: the survivors' boards teach crouch, throw, Q and E; the
// counter pendant runs the parts hoist over the north aisle as a lure; the
// mezzanine is a quiet way round; the chute man keeps his back to the bar store.
// States: mnt_hoist_lure (lure timer, re-armed by mnt_hoist_lure_done),
// mnt_bar_stock (bar signed out at Rack B: the lathe's prerequisite in 55),
// mnt_stores_alert (the detail's shared alert flag), mnt_stores_* guard states.
std::shared_ptr<const Map> makeCentralStores(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[54];m.definition=CampaignChunks[54];
 const float f=-19,roof=m.definition.ceiling,mz=-16.f,ot=f+3.f;
 m.definition.ambient=.27f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceCorrugated;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'E',9,14,f,false,0});
 m.definition.playerStart={21.8f,11.5f};m.definition.spawnYaw=kPi;m.definition.spawnHeight=f;
 machineryClock(m,"mnt_stores_clock",60000);
 // --- The portal from the district hall ------------------------------------------------------
 wall(m,23,9,24,14,-15.f,roof,15);
 for(float y:{8.72f,14.f})wall(m,22.72f,y,23.f,y+.28f,f,-15.f,12);
 // --- Storeman's office, north-east: glazed onto the floor, the hatch onto the passage ----------
 mntGlazed(m,18.6f,1.f,18.8f,8.2f,f,ot);
 mntSwingDoor(m,19.f,20.2f,8.3f,f,ot,.45f,-1);
 mntWall(m,18.6f,8.2f,19.f,8.4f,f,ot);mntWall(m,20.2f,8.2f,20.6f,8.4f,f,ot);mntWall(m,22.4f,8.2f,23.f,8.4f,f,ot);
 wall(m,20.6f,8.2f,22.4f,8.4f,f,f+1.f,3);wall(m,20.5f,8.08f,22.5f,8.62f,f+1.f,f+1.05f,20);
 wall(m,20.6f,8.2f,22.4f,8.4f,f+2.2f,ot,3);wall(m,20.55f,8.4f,22.45f,8.6f,f+2.2f,f+2.42f,12);
 for(float x:{20.6f,22.34f})wall(m,x,8.2f,x+.06f,8.4f,f+1.05f,f+2.2f,12);
 wall(m,18.6f,1.f,23.f,8.4f,ot,ot+.2f,15);
 // Inside: the storeman's desk at the glass, the ledger, cabinets, keys, the kettle.
 mntDesk(m,19.55f,4.4f,f,-kPi*.5f,1);
 terminal(m,{21.7f,1.75f},"CENTRAL STORES / ISSUE LEDGER","LAST ISSUE 02:09: FILTERS X4 TO WO 7702. NOTHING SINCE.","40 MM EN24: RACK B, BAY 3. ONE LENGTH CUT FOR WO 7714. NOT COLLECTED.");m.terminals.back().readable=Readable::Note;
 m.terminals.back().yaw=0;
 for(float y:{3.3f,3.9f})mntFix(m,model::file_cabinet_tall,{22.65f,y},f,.5f,.6f,1.3f,-kPi*.5f,true);
 mntFix(m,model::file_cabinet_wide,{22.5f,6.9f},f,.9f,.5f,.75f,-kPi*.5f,true);
 mntFix(m,model::coffee_machine,{22.55f,6.65f},f+.75f,.35f,.35f,.45f,-kPi*.5f,false);
 mntFix(m,model::mug,{22.5f,7.2f},f+.75f,.09f,.09f,.1f,0,false);
 mntFix(m,model::wall_box,{22.92f,5.3f},f+1.25f,.55f,.15f,.6f,-kPi*.5f,false);
 mntFix(m,model::med_notice_board,{20.f,1.04f},f+1.05f,1.2f,.07f,.9f,0,false);
 mntFix(m,model::med_first_aid_box,{22.95f,2.4f},f+1.35f,.37f,.12f,.35f,-kPi*.5f,false);
 m.pickupSpawns.push_back({{20.6f,7.1f},PickupKind::Health,f});
 mntTube(m,{20.4f,3.f},ot,true,.85f,5.5f,4300,.25f);
 mntTube(m,{21.4f,6.3f},ot,false,.7f,5.f,4300,.25f,.25f);
 zone(m,"CENTRAL STORES / STOREMAN",18.6f,1,23,8.4f);
 // --- The passage in and the goods line ------------------------------------------------------
 floorLine(m,18.66f,8.4f,18.8f,14.6f,f,13);
 for(float y:{8.8f,14.2f})mntFix(m,model::pwr_safety_bollard,{18.73f,y},f,.2f,.2f,1.1f,0,true);
 // Goods-in staging stacked two high: it hides the doorway from the main aisle.
 cargo(m,19.7f,11.7f,f,true,.1f);
 mntFix(m,model::wooden_crate,{19.72f,11.66f},f+1.15f,.95f,.9f,.85f,.35f,true);
 // The Cleanup's barrier, dragged aside when they marched B shift in.
 mntFix(m,model::road_block,{20.6f,13.75f},f,1.6f,.5f,.7f,.2f,true);
 for(float x:{21.4f,21.9f})wall(m,x,13.98f,x+.36f,14.02f,f+.02f,f+.03f,13);
 // The counter pendant: the parts hoist's remote.
 terminal(m,{21.5f,9.f},"PARTS HOIST / COUNTER PENDANT","RUNS THE TOTE HOIST OVER THE NORTH AISLE. THE MOTOR IS LOUD.","E / RUN IT. WHOEVER IS ON THE FLOOR WILL GO AND LOOK.",0,stateId("mnt_hoist_lure"));
 m.terminals.back().yaw=0;
 // Junk to throw, where the survivors left it.
 m.clutterSpawns.push_back({3,{21.1f,12.9f},f,.4f});m.clutterSpawns.push_back({2,{22.3f,13.3f},f});
 m.clutterSpawns.push_back({3,{20.1f,10.2f},f,1.2f});m.clutterSpawns.push_back({2,{22.4f,10.1f},f});
 // What B shift's last six chalked for each other before they gave up on the stores.
 for(float x:{21.5f,22.f})wall(m,x-.03f,13.3f,x+.03f,13.36f,f,f+1.2f,10);
 mntChalk(m,22.94f,14.6f,f+1.22f,"THEY'RE STILL IN THERE","STAY LOW. THEY HEAR BOOTS, NOT KNEES.",1.25f,-kPi*.5f);
 // --- Assembly point B: the cage where B shift waited to be counted ------------------------------
 mntMesh(m,true,14.6f,21.4f,3.18f,f,true);
 mntMesh(m,false,19.8f,16.1f,2.74f,f,false);mntMesh(m,false,19.8f,18.85f,2.74f,f,true);mntMesh(m,false,19.8f,21.6f,2.74f,f,false);
 for(Vec2 c:{Vec2{19.8f,14.6f},Vec2{19.8f,17.47f},Vec2{19.8f,20.23f},Vec2{19.8f,22.97f},Vec2{23.f,14.6f}})wall(m,c.x-.05f,c.y-.05f,c.x+.05f,c.y+.05f,f,f+2.8f,12);
 for(float y:{17.f,20.6f})bench(m,21.4f,y,f);
 // Lunch tins, flasks, hats and jackets: they expected to go back to work.
 for(int i=0;i<7;++i){const float x=20.3f+i*.32f,y=(i%2?17.f:20.6f)-.05f;wall(m,x-.11f,y-.07f,x+.11f,y+.07f,f+.51f,f+.62f,i%3==0?19:i%3==1?18:20);}
 for(float x:{20.6f,22.3f})m.pipes.push_back({{x,17.22f},{x,17.22f},f+.51f,.045f,f+.82f,5});
 for(Vec2 h:{Vec2{20.9f,17.05f},Vec2{22.6f,20.7f},Vec2{21.2f,19.3f},Vec2{22.4f,15.6f}})mntFix(m,model::mine_hard_hat,h,h.y>19.f&&h.y<20.f?f:(h.y<16.f?f:f+.51f),.28f,.35f,.15f,h.x*2.1f,false);
 for(float x:{20.4f,21.9f})wall(m,x-.25f,20.78f,x+.25f,20.86f,f+.55f,f+1.f,23);
 floorLine(m,20.1f,15.f,22.8f,15.12f,f,13);floorLine(m,20.1f,22.5f,22.8f,22.62f,f,13);
 mntSign(m,22.945f,18.8f,f+1.85f,"ASSEMBLY POINT B","FIRE / ROLL CALL / WAIT HERE TO BE COUNTED",2.4f,-kPi*.5f,mntGreen);
 m.clutterSpawns.push_back({1,{21.8f,18.6f},f,.6f});m.clutterSpawns.push_back({1,{20.6f,21.9f},f,2.f});
 m.pickupSpawns.push_back({{22.3f,22.1f},PickupKind::Ammo,f});
 m.lights.push_back(lamp({21.4f,18.8f},f+3.1f,.75f,6.f,2700,0));
 zone(m,"CENTRAL STORES / ASSEMBLY POINT",19.8f,14.6f,23,23);
 // --- The racking: Rack A north of the main aisle, Rack C south ---------------------------------
 mntRackRow(m,5.6f,6.6f,f,4,2,2,1);
 mntRackRow(m,5.6f,13.4f,f,4,2,1,6);
 for(float y:{10.75f,13.13f})floorLine(m,4.6f,y,18.6f,y+.12f,f,13);
 // Cover in the aisles: a pallet truck, staged loads.
 handCart(m,15.6f,12.9f,f);
 cargo(m,7.4f,12.95f,f,false,.15f);cargo(m,14.4f,11.25f,f,true);
 cargo(m,12.9f,18.15f,f,true,.4f);
 // --- The small-parts mezzanine along the north wall --------------------------------------------
 deck(m,mz,4,1,18,5);
 m.stairs.push_back({16.85f,5.f,17.95f,9.48f,f,mz,16,true,false});
 m.stairs.push_back({4.05f,5.f,5.15f,9.48f,f,mz,16,true,false});
 deckSupports(m,m.layers[1],f,3.f);
 for(float x:{5.2f,7.1f,9.f,10.9f,12.8f,14.7f})mntFix(m,model::metal_shelf,{x,1.36f},mz,1.8f,.5f,1.8f,0,true);
 for(int i=0;i<12;++i){const float x=4.55f+i*.95f+(i%2)*.1f,z=mz+(i%3==0?.31f:i%3==1?.97f:1.66f);
  mntFix(m,model::packing_crate,{x,1.36f},z,.42f,.34f,.26f,(i%4)*.1f,false);}
 for(auto [x,y,yaw]:{std::tuple{9.6f,3.4f,9.6f},{10.15f,3.5f,10.f},{15.6f,3.2f,15.6f}})mntFix(m,model::packing_crate,{x,y},mz,.55f,.4f,.32f,yaw,true);
 terminal(m,{12.f,3.6f},"SMALL PARTS / INDEX","BIN 4411 SEALS. BIN 4412 SEALS. BIN 4413 EMPTY SINCE MAY.","STOCK CHECK OVERDUE / SIGNED OFF ANYWAY / J. OKAFOR",mz-f);m.terminals.back().readable=Readable::Letter;
 m.terminals.back().yaw=0;
 m.pickupSpawns.push_back({{4.7f,2.3f},PickupKind::Ammo,mz});
 for(float x:{8.f,14.5f})m.lights.push_back(lamp({x,1.02f},mz+2.35f,.75f,6.f,6200,.4f,WorldLightMount::Wall,kPi*.5f));
 // The parts hoist over the north aisle: lifts totes from the floor to the deck.
 wall(m,11.2f,5.62f,13.f,5.88f,-14.92f,-14.7f,12);
 for(float x:{11.4f,12.8f})m.pipes.push_back({{x,5.75f},{x,5.75f},-14.7f,.02f,roof,6});
 wall(m,11.95f,5.6f,12.25f,5.9f,-15.25f,-14.92f,18);
 for(float y:{5.7f,5.8f})m.pipes.push_back({{12.1f,y},{12.1f,y},-16.2f,.012f,-15.25f,6});
 wall(m,11.98f,5.63f,12.22f,5.87f,-16.42f,-16.2f,18);
 mntFix(m,model::packing_crate,{12.1f,5.75f},-16.95f,.6f,.42f,.4f,.2f,false);
 for(float x:{11.6f,12.6f})wall(m,x,4.88f,x+.06f,5.f,mz,mz+1.1f,18);
 wall(m,11.6f,4.88f,12.66f,5.f,mz+1.04f,mz+1.1f,18);
 // Under the deck: small-parts shelving in bays; a tube that will not stay lit.
 for(float x:{5.3f,7.9f,10.5f,13.1f,15.7f})for(float y:{1.95f,3.8f})mntFix(m,model::metal_shelf,{x,y},f,1.8f,.5f,1.8f,kPi*.5f,true);
 mntTube(m,{9.2f,4.6f},mz-.22f,false,.7f,5.5f,6200,.45f,.55f);
 mntTube(m,{14.4f,2.9f},mz-.22f,true,.5f,4.5f,6200,.45f);
 zone(m,"CENTRAL STORES / MEZZANINE",4,1,18,5,mz-.5f,1000);
 // --- Rack B, the bar store, against the west wall ---------------------------------------------
 mntBarRack(m,9.6f,17.1f,f,3);
 mntSign(m,1.055f,13.35f,f+3.55f,"RACK B","BAR STOCK / 20 - 80 MM / SIGN FOR EVERY CUT",3.2f,kPi*.5f);
 terminal(m,{3.7f,12.6f},"RACK B / BAR ISSUE","40 MM EN24, BAY 3: ONE LENGTH CUT TO 160 MM, TAGGED WO 7714.","E / SIGN IT OUT AND TAKE IT. LATHE 2 IS IN THE FABRICATION SHOP.",0,stateId("mnt_bar_stock"));
 m.terminals.back().yaw=kPi*.5f;
 mntTimer(m,"mnt_bar_stock","mnt_bar_stock_done",3000,3000);
 cue(m,"mnt_bar_stock",250,Sound::JunkMetal,{2.1f,12.6f},"",.55f,f+1.2f);
 cue(m,"mnt_bar_stock",700,Sound::Exit,{3.7f,12.6f},"SIGNED OUT: 40 MM X 160, WO 7714. TAKE IT TO LATHE 2.",.3f,f+1.f);
 m.lights.push_back(lamp({1.02f,14.2f},f+4.1f,1.4f,9.f,3000,0,WorldLightMount::Wall,0));
 // The bar saw: a horizontal bandsaw, its roller infeed, the offcut bin.
 wall(m,1.6f,2.25f,3.f,3.f,f,f+.72f,12);wall(m,1.5f,2.15f,3.1f,3.1f,f+.72f,f+.8f,8);
 wall(m,1.75f,2.4f,2.85f,2.72f,f+.8f,f+1.28f,18);
 for(float x:{1.92f,2.68f})m.pipes.push_back({{x,2.36f},{x,2.76f},f+1.04f,.19f,f+1.04f,3});
 wall(m,2.15f,2.2f,2.45f,2.36f,f+.8f,f+.98f,8);wall(m,2.15f,2.76f,2.45f,2.92f,f+.8f,f+.98f,8);
 m.pipes.push_back({{2.3f,2.56f},{3.05f,2.56f},f+.84f,.02f,f+.84f,5});
 for(float x:{2.35f,2.65f,2.95f})m.pipes.push_back({{x,2.3f},{x,2.82f},f+.835f,.035f,f+.835f,6});
 for(float x:{3.2f,3.9f})wall(m,x-.03f,2.3f,x+.03f,2.82f,f,f+.76f,12);
 wall(m,1.2f,3.6f,1.9f,4.4f,f,f+.55f,11);
 for(int i=0;i<4;++i)m.pipes.push_back({{1.35f+i*.13f,3.75f},{1.4f+i*.13f,4.25f},f+.5f,.02f,f+.5f,i%2?5:6});
 // Re-anchor the roof rod to the existing saw housing below the lamp.
 m.pipes.push_back({{2.3f,2.6f},{2.3f,2.6f},f+1.28f,.012f,roof,6});
 m.lights.push_back(lamp({2.3f,2.6f},f+2.85f,.8f,5.f,3400,0));
 zone(m,"CENTRAL STORES / RACK B",1,5,5.5f,19.5f);
 // --- The scrap returns chute, south-west: where the headcount ended ----------------------------
 wall(m,1.f,19.6f,2.9f,22.2f,f,f+1.05f,11);
 wall(m,1.2f,19.8f,2.7f,22.f,f+1.05f,f+1.065f,8);
 wall(m,1.f,19.6f,2.9f,19.76f,f+1.05f,f+1.16f,12);wall(m,1.f,22.04f,2.9f,22.2f,f+1.05f,f+1.16f,12);wall(m,2.74f,19.6f,2.9f,22.2f,f+1.05f,f+1.16f,13);
 wall(m,1.f,19.66f,1.09f,22.14f,f+1.16f,f+2.55f,11);
 for(float z:{f+1.5f,f+2.1f})wall(m,1.09f,19.7f,1.13f,22.1f,z,z+.06f,12);
 wall(m,2.9f,20.3f,2.915f,21.35f,f+.35f,f+1.15f,16);wall(m,2.5f,20.2f,2.9f,21.5f,f+1.16f,f+1.166f,16);
 floorLine(m,2.9f,20.25f,3.9f,21.4f,f,16);
 mntFix(m,model::pwr_breaker_panel,{1.05f,18.9f},f+1.3f,.18f,.1f,.46f,kPi*.5f,false);
 // Drag marks from the cage's side gate down the south aisle to the chute.
 for(float x=18.9f;x>3.4f;x-=1.3f){const float y=20.15f+std::sin(x*.9f)*.14f;floorLine(m,x-.6f,y,x+.2f,y+.07f,f,16);floorLine(m,x-.4f,y+.36f,x+.25f,y+.42f,f,16);}
 // The detail's station: badge trays, a records box, the tally, a floodlight.
 mntFix(m,model::wooden_table,{5.3f,21.95f},f,1.367f,.724f,.78f,0,true);
 for(int i=0;i<3;++i){const float x=4.85f+i*.45f;wall(m,x-.19f,21.72f,x+.19f,22.14f,f+.78f,f+.82f,20);
  for(int k=0;k<6;++k){const float bx=x-.15f+(k%3)*.105f,by=21.8f+(k/3)*.17f;wall(m,bx,by,bx+.07f,by+.11f,f+.82f,f+.828f,(i+k)%3==0?19:(i+k)%3==1?18:20);}}
 mntFix(m,model::packing_crate,{6.55f,22.45f},f,.6f,.45f,.42f,.25f,true);
 for(Vec2 h:{Vec2{4.7f,21.8f},Vec2{6.0f,22.f}})mntFix(m,model::mine_hard_hat,h,f+.78f,.28f,.35f,.15f,h.x,false);
 terminal(m,{3.85f,22.35f},"CLEANUP DETAIL 4 / HEADCOUNT","B SHIFT PRESENT 03:10 - 03:58: 35 OF 41. PROCESSED: 35.","OUTSTANDING: 6. SWEEP THE DISTRICT. ORDER 7 STANDS.");m.terminals.back().readable=Readable::Note;
 m.terminals.back().yaw=kPi;
 wall(m,7.17f,22.27f,7.23f,22.33f,f,f+2.f,8);
 for(float a:{0.f,2.1f,4.2f})wall(m,7.2f+std::sin(a)*.34f-.02f,22.3f+std::cos(a)*.34f-.02f,7.2f+std::sin(a)*.34f+.02f,22.3f+std::cos(a)*.34f+.02f,f,f+.04f,6);
 wall(m,7.02f,22.12f,7.38f,22.48f,f+2.f,f+2.26f,8);
 m.lights.push_back(lamp({6.9f,22.2f},f+2.05f,1.8f,10.f,5600,0,WorldLightMount::Bare));
 zone(m,"CENTRAL STORES / RETURNS CHUTE",1,19.5f,8,23);
 // --- Floor stock along the south wall ----------------------------------------------------------
 mntFix(m,model::forklift,{11.5f,21.95f},f,1.1641f,3.6204f,2.2f,kPi*.5f,true);
 for(float y:{21.6f,22.5f})mntFix(m,model::cable_reel,{8.4f,y},f,.45f,.9f,.9f,kPi*.5f,true);
 mntFix(m,model::pwr_sheet_steel_stack,{14.6f,21.75f},f,1.2f,2.28f,.09f,0,true);
 mntFix(m,model::pwr_sheet_steel_stack,{14.62f,21.72f},f+.09f,1.2f,2.28f,.09f,.04f,true);
 for(Vec2 d:{Vec2{16.25f,22.4f},Vec2{17.f,22.4f},Vec2{16.6f,21.7f}})mntFix(m,model::drum,d,f,.6f,.6f,.88f,d.x,true);
 for(float x:{17.9f,18.3f,18.7f})mntFix(m,model::gas_cylinder,{x,22.72f},f,.3f,.3f,1.45f,0,true);
 wall(m,17.7f,22.86f,18.9f,22.92f,f+1.05f,f+1.11f,8);
 // --- The sling test bay: a proof-load cage that releases at rated load ------------------------
 // The test load is a 1 t timber crate that slides on its skids until it hits
 // something. Shoved south it stops against the bay's first bollard; shoved
 // west from there it runs down the aisle against the second, onto the floor
 // scale, which unlocks the cage (lifting gear and B shift's spare kit). Shoved
 // west first it wedges on the staged load, and the winch drags it home.
 m.pushCrates.push_back({{16.5f,18.5f},0});
 m.cratePlates.push_back({{9.5f,19.5f},stateId("mnt_test_load")});
 m.crateReset=stateId("mnt_test_reset");
 for(Vec2 stop:{Vec2{8.5f,19.5f},Vec2{16.5f,20.5f}})mntFix(m,model::pwr_safety_bollard,stop,f,.2f,.2f,1.1f,0,true);
 wall(m,9.04f,19.04f,9.96f,19.96f,f+.002f,f+.006f,8);
 for(auto [x1,y1,x2,y2]:{std::tuple{9.f,19.f,10.f,19.06f},{9.f,19.94f,10.f,20.f},{9.f,19.f,9.06f,20.f},{9.94f,19.f,10.f,20.f}})floorLine(m,x1,y1,x2,y2,f,13);
 // The cage: mesh on a kerb, its gate on the aisle.
 mntMesh(m,true,17.f,4.8f,3.2f,f,false);
 mntMesh(m,false,6.4f,18.2f,2.4f,f,false);mntMesh(m,false,3.2f,18.2f,2.4f,f,false);
 mntMesh(m,true,19.4f,4.8f,3.2f,f,true);
 {Door gate{4.24f,5.36f,19.4f,0};gate.swinging=true;gate.swing=-1;gate.requireState=stateId("mnt_test_load");m.doors.push_back(gate);}
 taped(m,{6.43f,19.f},f+1.45f,kPi*.5f,Readable::Note,"SLING TEST BAY","CAGE RELEASES AT RATED LOAD. TEST LOAD ON THE SCALE, NOT YOUR FOOT.","WINCH IT BACK TO THE BAY WHEN YOU'RE DONE. - J.O.");
 terminal(m,{17.6f,17.35f},"TEST BAY / WINCH PENDANT","DRAGS THE TEST LOAD BACK TO ITS BAY.","E / WINCH IT HOME.",0,stateId("mnt_test_reset"));
 m.terminals.back().yaw=kPi;
 mntFix(m,model::packing_crate,{3.8f,17.5f},f,.6f,.45f,.42f,.1f,true);
 mntFix(m,model::cable_reel,{5.7f,17.6f},f,.45f,.9f,.9f,kPi*.5f,true);
 m.pickupSpawns.push_back({{4.4f,18.4f},PickupKind::Health,f});
 m.pickupSpawns.push_back({{5.4f,18.6f},PickupKind::Ammo,f});
 m.lights.push_back(lamp({4.8f,18.2f},f+2.9f,.8f,5.f,4200,0));
 // --- The store's shell: girders, ducts, the high bays ----------------------------------------
 for(float x:{3.f,7.f,11.f,15.f,19.f}){wall(m,x-.13f,1.f,x+.13f,23.f,roof-.6f,roof-.54f,12);wall(m,x-.13f,1.f,x+.13f,23.f,roof-.06f,roof,12);wall(m,x-.02f,1.f,x+.02f,23.f,roof-.54f,roof-.06f,12);}
 wallBays(m,f,roof,4.f,false,true,true,false,12.f,20.f);
 servicePipe(m,{1.2f,6.2f},{22.8f,6.2f},roof-1.f,.3f,5);
 servicePipe(m,{1.2f,18.f},{22.8f,18.f},roof-1.1f,.12f,3);
 cableTray(m,{1.4f,5.6f},{1.4f,22.6f},f+4.6f);
 // The middle high bay over the main aisle is dead: a dark pool between the lit ends.
 for(Vec2 l:{Vec2{15.f,12.f},Vec2{6.f,12.f},Vec2{15.f,19.f},Vec2{21.f,11.5f}})mntPendant(m,l,l.x==6.f||l.x>20.f?roof:roof-.6f,roof-1.4f,l.x>20?1.2f:1.5f,10.f,2050,-.1f);
 m.pipes.push_back({{11.f,12.f},{11.f,12.f},roof-1.08f,.016f,roof-.6f,6});
 mntFix(m,model::pendant_lamp,{11.f,12.f},mntRel(m,roof-1.4f)+f-m.layers.front().elevation,.5f,.5f,.34f,0,false);
 // --- Cleanup Detail 4 ---------------------------------------------------------------------------
 const StateId alert=stateId("mnt_stores_alert");
 mntGuard(m,"mnt_stores_clock","mnt_stores",0,ActorVisual::Cleanup,GuardSquadRole::Assault,
  {{{15.2f,12.f},f,3500,0.f},{{12.1f,12.f},f,3000,kPi},{{6.6f,12.f},f,3500,-kPi*.5f},{{9.5f,12.f},f,2500,0.f}},12,.45f,1,1.25f,alert);
 mntGuard(m,"mnt_stores_clock","mnt_stores",1,ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,
  {{{4.4f,20.2f},f,8000,-1.2f},{{5.4f,20.9f},f,6000,0.f}},11,.45f,2,1.1f,alert);
 mntGuard(m,"mnt_stores_clock","mnt_stores",2,ActorVisual::CleanupHeavy,GuardSquadRole::Assault,
  {{{16.2f,19.f},f,5000,kPi*.5f},{{9.5f,19.f},f,3000,kPi},{{7.2f,19.2f},f,2500,-kPi*.25f}},12,.45f,1,1.2f,alert);
 // --- The lure: the hoist's motor and chain draw the floor into the north aisle -------------------
 mntTimer(m,"mnt_hoist_lure","mnt_hoist_lure_done",9000,9000);
 const Vec2 hoist{12.1f,5.75f};
 cue(m,"mnt_hoist_lure",200,Sound::LiftMotor,hoist,"PARTS HOIST RUNNING / NORTH AISLE.",1.f,-16.6f);
 cue(m,"mnt_hoist_lure",1500,Sound::LiftCreak,hoist,"",1.f,-16.6f);
 cue(m,"mnt_hoist_lure",2800,Sound::JunkMetal,hoist,"THEY HEARD IT. STAY LOW AND MOVE.",1.f,-16.8f);
 cue(m,"mnt_hoist_lure",4600,Sound::LiftCreak,hoist,"",1.f,-16.6f);
 cue(m,"mnt_hoist_lure",6200,Sound::LiftMotor,hoist,"",.9f,-16.6f);
 {auto& rearm=mntOn(m,"mnt_hoist_rearm",stateId("mnt_hoist_lure_done"));rearm.once=false;
  rearm.actions={mntAct(ScriptAction::Type::SetState,stateId("mnt_hoist_lure"),0),mntAct(ScriptAction::Type::SetState,stateId("mnt_hoist_lure_done"),0)};}
 zone(m,"CENTRAL STORES / GOODS LINE",18.8f,8.4f,23,14.6f);
 zone(m,"CENTRAL STORES",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// FABRICATION SHOP (manifest 35). Where the district made its own parts: the
// machine line under the west wall, a welding bay behind mesh to the east,
// plate stock and tool chests, and at the south end the roller shutter to
// Operations, down since its drive pin sheared at 02:16. R. Marsh got as far
// as Lathe 2 without the bar; the player brings it from Central Stores.
std::shared_ptr<const Map> makeFabricationShop(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[55];m.definition=CampaignChunks[55];m.definition.ambient=.26f;
 m.wallSurface=SurfaceBrick;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceCorrugated;
 const float f=-19,roof=m.definition.ceiling;
 const StateId pin=stateId("mnt_pin_machined"),shutter=stateId("maint_shutter_pin");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10,14,f,false,0});openEdge(m,{'S',10,14,f,true,shutter});
 wallBays(m,f,roof,4.f,false,false,true,true);
 // Roof steel: three trusses on the bay columns either side of the aisle.
 for(float y:{6.f,12.f,18.f}){wall(m,1,y-.12f,23,y+.12f,roof-.55f,roof,12);for(float x:{9.3f,14.7f})steelColumn(m,x,y,f,roof-.55f,false);}
 for(float x:{9.85f,14.03f})floorLine(m,x,1.f,x+.12f,22.6f,f,13);
 // --- The machine line, west ----------------------------------------------------------------
 // CNC cell: a steel enclosure with a glazed front, its machine inside.
 wall(m,1.22f,1.f,1.37f,5.2f,f,f+2.5f,2);wall(m,6.65f,1.f,6.8f,5.2f,f,f+2.5f,2);
 mntGlazed(m,1.37f,5.05f,6.65f,5.2f,f,f+2.5f,2);
 wall(m,1.22f,1.f,6.8f,5.2f,f+2.5f,f+2.6f,2);
 mntFix(m,model::test_machine,{4.f,2.9f},f,2.2f,2.8f,2.9f*.85f,kPi*.5f,true);
 // The mill: base, column, head and table, and its lamp.
 wall(m,2.6f,8.f,4.2f,9.2f,f,f+.9f,8);wall(m,2.9f,8.7f,3.9f,9.2f,f+.9f,f+2.3f,8);wall(m,2.8f,8.1f,4.f,9.2f,f+2.3f,f+2.75f,8);
 wall(m,2.5f,8.1f,4.3f,8.6f,f+1.05f,f+1.15f,12);
 m.pipes.push_back({{3.4f,8.35f},{3.4f,8.35f},f+1.6f,.05f,f+2.3f,6});
 // Lathes: a cabinet stand, the bed ways, headstock and chuck, carriage and
 // tailstock, along x with the operator on the north side.
 auto lathe=[&](float y,float part){
  wall(m,2.f,y-.4f,8.f,y+.4f,f,f+.78f,8);
  wall(m,2.1f,y-.3f,7.9f,y+.3f,f+.78f,f+1.f,12);
  wall(m,2.f,y-.45f,3.2f,y+.45f,f+1.f,f+1.6f,8);
  m.pipes.push_back({{3.2f,y},{3.42f,y},f+1.28f,.17f,f+1.28f,6});
  if(part>0)m.pipes.push_back({{3.42f,y},{3.42f+part,y},f+1.28f,.05f,f+1.28f,6});
  wall(m,4.8f,y-.42f,5.6f,y+.42f,f+1.f,f+1.22f,12);wall(m,5.05f,y-.12f,5.35f,y+.12f,f+1.22f,f+1.38f,8);
  wall(m,6.8f,y-.25f,7.4f,y+.25f,f+1.f,f+1.42f,8);wall(m,6.5f,y-.04f,6.8f,y+.04f,f+1.24f,f+1.32f,12);
  wall(m,2.f,y-.45f,8.f,y-.42f,f+1.f,f+1.08f,12);
 };
 lathe(13.f,3.08f);lathe(18.8f,0);
 // Lathe 2's pendant console in front of its bed, and its work lamp, lit
 // while the pin is being turned.
 mntFix(m,model::pwr_control_cabinet,{6.f,18.05f},f,.6f,.35f,1.25f,kPi,true);
 mntControl(m,{6.f,18.f},"LATHE 2 / SHEAR PIN","E / MACHINE THE 40 MM EN24 SHEAR PIN.","REQUIRES BAR STOCK: SIGN IT OUT AT RACK B IN CENTRAL STORES.",pin,stateId("mnt_bar_stock"),kPi);
 m.lights.push_back(lamp({4.4f,18.8f},f+2.1f,.9f,4.f,3600,.1f,WorldLightMount::Bare));
 {auto& t=mntTimer(m,"mnt_lathe_ms","mnt_lathe_done",4200,4200);t.soundIntervalMs=350;t.soundUntilMs=4000;t.tickSound=Sound::Machine;t.gain=.8f;t.pitch=1.4f;t.soundAt={3.f,18.8f};t.soundZ=f+1.3f; // lathe 2's headstock
  ScriptEvent e;e.id=stateId("mnt_ev_lathe");e.x1=0;e.y1=0;e.x2=24;e.y2=24;e.requireState=pin;e.actions={mntAct(ScriptAction::Type::SetState,stateId("mnt_lathe_ms"),1)};m.scriptEvents.push_back(e);}
 // Marsh, at the lathe he could not use; the empty bar chuck, a flask.
 mntBody(m,"mnt_marsh_clock",ActorVisual::Worker,{3.9f,17.2f},f,2.6f);machineryClock(m,"mnt_marsh_clock",60000);
 for(auto [x,y,r]:{std::tuple{3.7f,17.f,.55f},{4.4f,17.5f,.3f}})wall(m,x-r,y-r*.6f,x+r,y+r*.6f,f,f+.004f,16);
 mntFix(m,model::mine_hard_hat,{6.9f,16.6f},f,.28f,.35f,.15f,.9f,false);
 // Workbenches between the machines, vices and a grinder on them.
 auto bench=[&](float y1,float y2){wall(m,1.25f,y1,2.1f,y2,f+.86f,f+.92f,12);
  for(float y:{y1+.05f,y2-.1f})wall(m,1.3f,y,2.05f,y+.05f,f,f+.86f,12);
  wall(m,1.3f,y1,2.05f,y2,f+.18f,f+.2f,12);
  wall(m,1.6f,(y1+y2)*.5f-.12f,1.9f,(y1+y2)*.5f+.12f,f+.92f,f+1.08f,8);};
 bench(10.f,11.8f);bench(15.f,16.6f);bench(20.6f,22.8f);
 // --- The welding bay, east, behind mesh -------------------------------------------------------
 mntMesh(m,false,15.6f,4.3f,3.19f*1.f,f,true);mntMesh(m,false,15.6f,7.45f,3.1f,f,false);
 mntMesh(m,true,9.05f,17.45f,3.7f,f,false);mntMesh(m,true,9.05f,21.15f,3.7f,f,false);
 wall(m,18.f,4.f,21.f,5.4f,f+.88f,f+.94f,12);for(Vec2 l:{Vec2{18.1f,4.1f},Vec2{20.85f,4.1f},Vec2{18.1f,5.25f},Vec2{20.85f,5.25f}})wall(m,l.x,l.y,l.x+.06f,l.y+.06f,f,f+.88f,12);
 for(float x:{22.3f,22.65f})mntFix(m,model::gas_cylinder,{x,2.f},f,.33f,.33f,1.58f,0,true);
 mntFix(m,model::gas_cylinder,{22.45f,2.5f},f,.33f,.33f,1.58f,0,true);
 mntFix(m,model::drum,{17.f,8.2f},f,.6f,.6f,.88f,.4f,true);
 // --- Plate stock and the tool chests --------------------------------------------------------------
 mntFix(m,model::pwr_sheet_steel_stack,{19.6f,12.4f},f,1.79f,3.4f,.13f,0,true);
 mntFix(m,model::pwr_sheet_steel_stack,{19.6f,12.4f},f+.13f,1.79f,3.4f,.13f,.04f,true);
 for(float y:{15.6f,16.5f})mntFix(m,model::mine_tool_chest,{22.45f,y},f,.75f,.45f,.97f,-kPi*.5f,true);
 mntFix(m,model::stock_cage,{19.6f,21.7f},f,4.49f,2.01f,2.03f,0,true);
 // --- The shutter to Operations ---------------------------------------------------------------
 // Its head box and drive motor over the opening; the local control pedestal
 // stands clear of the door at the east jamb.
 wall(m,9.4f,22.6f,14.6f,23.f,f+3.f,f+3.9f,2);
 wall(m,14.6f,22.4f,15.6f,23.f,f+2.9f,f+3.7f,8);
 mntFix(m,model::pwr_control_cabinet,{15.6f,22.05f},f,.6f,.35f,1.25f,kPi,true);
 mntControl(m,{15.6f,22.f},"OPERATIONS SHUTTER / DRIVE","E / FIT THE MACHINED PIN AND RELEASE THE SHUTTER.","REQUIRES THE FINISHED PIN FROM LATHE 2. OPERATIONS IS THROUGH THE SOUTH DOOR.",shutter,pin,kPi);
 mntFix(m,model::pwr_breaker_panel_open,{16.4f,22.92f},f+1.3f,.32f,.23f,.46f,kPi,false);
 // --- A transformed worker in the welding bay --------------------------------------------------------
 // The welders' lockers stand across the bay with their backs to the south;
 // it crouches behind them, out of sight from the aisle and the entrance,
 // until the lathe's noise carries to it or the player walks into the bay.
 for(int i=0;i<4;++i)mntFix(m,model::pwr_locker_bank,{18.2f+.9f*i,6.7f},f,.87f,.85f,2.42f,kPi*.5f,true);
 {const StateId wake=stateId("mnt_weld_wake");
  m.creatureSpawns.push_back({CreatureKind::Snatcher,{19.5f,7.9f},f,1,0,wake});
  auto& noise=mntOn(m,"mnt_ev_weld_noise",stateId("mnt_lathe_done"));noise.actions={mntAct(ScriptAction::Type::SetState,wake,1)};
  ScriptEvent e;e.id=stateId("mnt_ev_weld_close");e.x1=15.7f;e.y1=1.f;e.x2=23.f;e.y2=9.5f;e.bottom=f-1;e.top=f+2;
  e.actions={mntAct(ScriptAction::Type::SetState,wake,1)};m.scriptEvents.push_back(e);}
 m.pickupSpawns.push_back({{22.f,19.f},PickupKind::Ammo,f});
 mntFix(m,model::med_first_aid_box,{14.7f,11.75f},f+1.4f,.37f,.12f,.35f,kPi,false);
 // --- Light: tube rows over the aisle and the machine line, one dying ----------------------------
 for(float y:{3.f,9.f,15.f,21.f})mntHungTube(m,{12.f,y},roof,f+3.6f,true,1.1f,8.f,4800,.05f,y==15.f?.35f:0.f);
 for(float y:{3.f,9.f,15.f,21.f})mntHungTube(m,{5.f,y},roof,f+3.6f,true,.9f,7.f,4600,.05f);
 for(float y:{4.5f,13.5f,20.f})mntHungTube(m,{19.5f,y},roof,f+3.6f,true,.8f,7.f,4400,.05f,y==4.5f?.5f:0.f);
 zone(m,"FABRICATION SHOP / LATHES",1,9.5f,9.3f,23);
 zone(m,"FABRICATION SHOP / WELDING BAY",15.6f,1,23,9.05f);
 zone(m,"FABRICATION SHOP",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// MAINTENANCE / AID STATION (manifest 33). The B shift's first-aid post,
// where the six who were left after the headcount held out: a barricade of
// lockers at the door, cots and bedrolls in the triage room, the medicine
// store behind it. Cleanup found them. One is still breathing in treatment.
std::shared_ptr<const Map> makeMaintenanceAidStation(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[56];m.definition=CampaignChunks[56];m.definition.ambient=.22f;
 m.wallSurface=SurfaceClinicTile;m.floorSurface=SurfaceClinicTile;m.ceilingSurface=SurfaceOfficePlaster;
 const float f=-19,roof=m.definition.ceiling;
 const StateId rise=stateId("mnt_aid_rise");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 auto& L=m.layers[0];
 rectangle(L,1,1,23,23,'#');
 rectangle(L,1,9,6,14,'.');      // the passage from the district
 rectangle(L,6,5,15,19,'.');     // triage
 rectangle(L,15,5,21,11,'.');    // treatment
 rectangle(L,15,12,21,19,'.');   // medicine store
 openEdge(m,{'W',9,14,f,false,0});
 // --- The barricade: lockers dragged across the doorway, one pulled aside -------------------
 mntDoorwayY(m,10.2f,12.8f,5.85f,6.15f,f,roof);
 wall(m,5.85f,9.f,6.15f,10.2f,f,roof,3);wall(m,5.85f,12.8f,6.15f,14.f,f,roof,3);
 mntFix(m,model::pwr_locker_bank,{6.7f,10.6f},f,.87f,.85f,2.42f,kPi*.5f,true);
 mntFix(m,model::pwr_locker_bank,{7.4f,12.5f},f,.87f,.85f,2.42f,kPi*.85f,true);
 mntFix(m,model::wooden_table,{8.2f,9.8f},f,1.367f,.724f,.78f,.5f,true);
 // --- Triage: cots, bedrolls, the nurse's desk, the forms on the board ------------------------
 for(float y:{6.2f,8.6f})mntFix(m,model::med_ward_bed,{13.6f,y},f,1.98f,.94f,.84f,kPi,true);
 mntFix(m,model::med_ward_bed_tossed,{13.5f,16.9f},f,2.1f,1.52f,.84f,kPi,true);
 for(auto [x,y,yaw]:{std::tuple{7.6f,15.5f,0.f},{8.8f,15.8f,.15f},{10.f,15.4f,-.1f}})mntFix(m,model::scrap_bedroll,{x,y},f,.92f,1.96f,.18f,yaw,false);
 mntFix(m,model::med_nurse_desk,{8.f,5.6f},f,1.18f,.56f,.81f,kPi,true);
 mntFix(m,model::med_notice_board,{10.5f,5.07f},f+1.f,1.2f,.07f,.9f,0,false);
 mntFix(m,model::med_scrub_sink,{6.33f,7.6f},f,.49f,.54f,1.13f,kPi*.5f,true);
 mntFix(m,model::med_crash_cart,{11.6f,12.f},f,.74f,.48f,1.f,.3f,true);
 mntFix(m,model::med_wheelchair,{7.f,17.9f},f,.55f,1.03f,.9f,2.f,true);
 mntFix(m,model::med_wall_clock,{12.f,5.03f},f+2.f,.24f,.02f,.23f,0,false);
 terminal(m,{9.3f,5.7f},"AID STATION / EXPOSURE REGISTER","B SHIFT: 41 SCREENED THIS MONTH. TIER 2: 6. TIER 3: 1 (TRANSFERRED).","TIER 3 GOES TO MEDICAL SCREENING ON LEVEL 9. NOBODY HAS COME BACK TO WORK FROM THERE.");m.terminals.back().readable=Readable::Note;
 // The ones Cleanup reached: four where they lay, and the blood.
 for(auto [x,y,yaw]:{std::tuple{9.f,11.2f,1.f},{10.6f,9.4f,2.4f},{12.4f,14.f,-.6f},{8.4f,13.6f,3.6f}}){
  std::string clock="mnt_aid_body_"+std::to_string(int(x*10));mntBody(m,clock.c_str(),ActorVisual::Worker,{x,y},f,yaw);machineryClock(m,clock.c_str(),60000);
  wall(m,x-.45f,y-.3f,x+.45f,y+.3f,f,f+.004f,16);}
 // --- Treatment: the last of them --------------------------------------------------------------
 mntDoorwayY(m,7.f,8.6f,14.85f,15.15f,f,roof);
 wall(m,14.85f,5.f,15.15f,7.f,f,roof,3);wall(m,14.85f,8.6f,15.15f,11.f,f,roof,3);
 mntFix(m,model::med_ward_bed,{18.6f,6.3f},f,1.98f,.94f,.84f,kPi,true);
 mntFix(m,model::med_crash_cart,{20.3f,6.6f},f,.74f,.48f,1.f,-kPi*.5f,true);
 mntFix(m,model::med_vitals_monitor,{20.3f,6.6f},f+1.f,.38f,.18f,.28f,-kPi*.5f,false);
 mntFix(m,model::med_supply_cabinet,{20.75f,9.4f},f,1.1f,.44f,1.7f,-kPi*.5f,true);
 // He lies on the mattress the length of the bed, head at the headboard.
 m.actorTracks.push_back({stateId("mnt_aid_last"),ActorVisual::Worker,1.7f,false,false,{{0,{17.85f,6.3f},f+.58f,-kPi*.5f,4,1},{30000,{17.85f,6.3f},f+.58f,-kPi*.5f,4,1}}});
 {auto& t=mntTimer(m,"mnt_aid_last","mnt_aid_last_done",30000,26000);t.x1=15.2f;t.y1=5.f;t.x2=21.f;t.y2=11.f;t.bottom=f-1;t.top=f+3;}
 cue(m,"mnt_aid_last",400,Sound::WorkerDying,{18.4f,6.3f},"YOU'RE NOT CLEANUP.",.6f,f+.8f);
 cue(m,"mnt_aid_last",5200,Sound::Empty,{18.4f,6.3f},"THEY CAME AT FOUR. KNOCKED THREE TIMES. WE OPENED THE DOOR.",0.f,f+.8f);
 cue(m,"mnt_aid_last",11000,Sound::Empty,{18.4f,6.3f},"MARSH WENT FOR THE SHUTTER. THE PIN. HE NEVER GOT THE BAR.",0.f,f+.8f);
 cue(m,"mnt_aid_last",17500,Sound::Empty,{18.4f,6.3f},"OPERATIONS IS THE WAY OUT. DON'T LET THEM COUNT YOU.",0.f,f+.8f);
 // --- The medicine store, and the one that was not dead ----------------------------------------
 mntDoorwayY(m,14.6f,16.2f,14.85f,15.15f,f,roof);
 wall(m,14.85f,12.f,15.15f,14.6f,f,roof,3);wall(m,14.85f,16.2f,15.15f,19.f,f,roof,3);
 for(float x:{16.4f,17.6f,18.8f})mntFix(m,model::med_supply_cabinet,{x,18.75f},f,1.1f,.44f,1.7f,kPi,true);
 mntFix(m,model::metal_shelf,{20.7f,15.f},f,1.8f,.5f,1.8f,-kPi*.5f,true);
 m.pickupSpawns.push_back({{17.f,14.5f},PickupKind::Health,f});
 m.pickupSpawns.push_back({{19.f,16.8f},PickupKind::Health,f});
 m.pickupSpawns.push_back({{16.f,16.6f},PickupKind::Ammo,f});
 {CreatureSpawn c{CreatureKind::Mutant,{13.4f,18.35f},f};c.wakeState=rise;m.creatureSpawns.push_back(c);}
 {ScriptEvent e;e.id=stateId("mnt_ev_aid_rise");e.x1=15.4f;e.y1=12.2f;e.x2=21.f;e.y2=19.f;e.bottom=f-1;e.top=f+3;
  e.actions={mntAct(ScriptAction::Type::SetState,rise,1)};m.scriptEvents.push_back(e);}
 // --- Light: what the generator still feeds --------------------------------------------------------
 mntTube(m,{10.5f,8.f},roof,false,.9f,7.f,5200,.1f);
 mntTube(m,{10.5f,14.5f},roof,false,.7f,6.f,5200,.1f,.4f);
 mntTube(m,{18.f,8.f},roof,false,.8f,5.f,4800,.05f);
 mntTube(m,{18.f,15.5f},roof,false,.5f,5.f,4800,.05f,.25f);
 mntTube(m,{3.f,11.5f},roof,true,.5f,4.f,4600,0);
 zone(m,"AID STATION / TREATMENT",15,5,21,11);
 zone(m,"AID STATION / MEDICINE STORE",15,12,21,19);
 zone(m,"AID STATION",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
}
void buildChapterMaintenance(CampaignMapTable& maps){
 maps[53]=makeMaintenanceDistrict();
 maps[54]=makeCentralStores();
 maps[55]=makeFabricationShop();
 maps[56]=makeMaintenanceAidStation();
}
}

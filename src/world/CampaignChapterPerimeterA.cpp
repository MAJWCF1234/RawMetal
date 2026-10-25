// THE RESTRICTED PERIMETER (chapter 1, manifest 36-40): chunks 57-62.
// See docs/Design/descent-campaign-20261025.md for the brief.
//
// The architecture of secrecy begins here. The maintenance district's last
// shutter opens onto the clean line (57), where boots, tools and badges were
// checked; past it the fortified Security Perimeter yard (58) and its
// gatehouse (59), held by a Cleanup detail; then Checkpoint Delta (60), the
// hard access point where the detail processed the people it caught; the
// Administrative Link (61), glass offices over the old ore pass where the
// security chief refused Order 7; and Personnel Transit (62), where the staff
// tram died at its platform.
//
// States set here: perim_gate (gatehouse console, 59), delta_override (chief's
// console, 61), and per_* scene states (see the report in each builder).
#include "CampaignChapters.h"
#include "CampaignKit.h"
#include <cmath>
#include <string>
#include <vector>
namespace retro {
namespace {
using namespace kit;
// --- PerimeterA helpers --------------------------------------------------------------------
// Corporate signage is blue; warnings red; plant and route signs keep amber.
constexpr std::uint32_t perBlue=0xff2f6db8u,perRed=0xffc23a2eu,perAmber=0xffca994du,perGreen=0xff3f9a5au;
void perSign(Map& m,float x,float y,float z,const char* title,const char* subtitle,float width,float yaw,std::uint32_t accent){
 wallSign(m,x,y,z,title,subtitle,width,yaw);m.signs.back().accent=accent;
}
// A fixture at an absolute base height.
void perFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw,bool solid){
 m.fixtures.push_back({model,{x,y},z-m.layers.front().elevation,w,d,h,yaw,solid});
}
// A thin finish on a floor (tile, carpet, paint): flush, 2 cm.
void perFinish(Map& m,float x1,float y1,float x2,float y2,float z,int material){wall(m,x1,y1,x2,y2,z,z+.02f,material);}
// A blood pool or smear on a floor; thin, so it casts no shadow.
void perBlood(Map& m,float x1,float y1,float x2,float y2,float z){wall(m,x1,y1,x2,y2,z+.021f,z+.026f,16);}
// A smear on a wall face: n is the outward normal axis (0 x, 1 y), at the face coordinate.
void perWallBlood(Map& m,bool alongY,float face,float a1,float a2,float z1,float z2,float out){
 if(alongY)wall(m,face,a1,face+out,a2,z1,z2,16);else wall(m,a1,face,a2,face+out,z1,z2,16);
}
// A workstation whose worker faces `face` (yaw: 0 south, kPi north, kPi/2 east, -kPi/2 west).
void perDesk(Map& m,float x,float y,float f,float face,int variant){
 Vec2 p{x,y},d{std::sin(face),std::cos(face)},s{std::cos(face),-std::sin(face)};
 auto at=[&](float u,float v){return p+s*u+d*v;};
 const float b=f-m.layers.front().elevation;
 m.fixtures.push_back({model::desk,p,b,1.4f,.7f,.75f,face,true});
 m.fixtures.push_back({model::office_chair,at(variant%2?.15f:-.1f,-.78f),b,.55f,.55f,.95f,face+kPi+(variant%3-1)*.25f,true});
 m.fixtures.push_back({model::crt_monitor,at(-.1f,.12f),b+.75f,.42f,.4f,.38f,face+kPi,false});
 m.fixtures.push_back({model::keyboard,at(-.1f,-.2f),b+.75f,.45f,.16f,.03f,face+kPi,false});
 m.fixtures.push_back({model::desk_phone,at(.48f,.05f),b+.75f,.2f,.2f,.1f,face+kPi,false});
 if(variant%2)m.fixtures.push_back({model::mug,at(.42f,-.2f),b+.75f,.09f,.09f,.1f,0,false});
 if(variant%3==0)m.fixtures.push_back({model::binder,at(-.52f,.18f),b+.75f,.3f,.08f,.32f,face+kPi,false});
 if(variant%4==1)m.fixtures.push_back({model::pc_tower,at(.52f,.1f),b,.2f,.45f,.42f,face+kPi,false});
}
// A CCTV camera on a wall bracket. (x,y) is on the wall face; (nx,ny) points
// into the room along one axis; the camera looks along it, slightly down.
void perCamera(Map& m,float x,float y,float z,int nx,int ny){
 auto box=[&](float u1,float u2,float v1,float v2,float z1,float z2,int material){
  // u runs along the normal (out of the wall), v along the wall.
  float ax1=nx?x+nx*u1:x+v1,ax2=nx?x+nx*u2:x+v2,ay1=ny?y+ny*u1:y+v1,ay2=ny?y+ny*u2:y+v2;
  wall(m,std::min(ax1,ax2),std::min(ay1,ay2),std::max(ax1,ax2),std::max(ay1,ay2),z1,z2,material);};
 box(0,.03f,-.09f,.09f,z-.1f,z+.1f,2);
 box(.03f,.24f,-.025f,.025f,z-.02f,z+.02f,12);
 box(.16f,.5f,-.07f,.07f,z-.16f,z-.04f,20);
 box(.14f,.52f,-.09f,.09f,z-.04f,z-.02f,2);
 box(.5f,.53f,-.045f,.045f,z-.14f,z-.06f,8);
 box(.2f,.24f,-.02f,.02f,z-.04f,z-.02f,8);
}
// A decorative body: a scripted track that holds the fallen pose.
void perBody(Map& m,const char* clock,ActorVisual visual,Vec2 p,float z,float yaw){
 m.actorTracks.push_back({stateId(clock),visual,visual==ActorVisual::Worker?1.7f:1.6f,false,false,{{0,p,z,yaw,4,1},{1000,p,z,yaw,4,1}}});
}
// An armed Cleanup guard walking an authored beat.
void perGuard(Map& m,const char* clock,const char* squad,int index,ActorVisual visual,GuardSquadRole role,std::vector<ActorPatrolPoint> beat,
              float sight,float dot,int weapon,float speed,StateId alert=0,StateId enable=0){
 ActorTrack t;t.timerState=stateId(clock);t.visual=visual;t.scale=1.6f;t.health=90;t.ai.enableState=enable;
 const std::string key=std::string(squad)+"_"+std::to_string(index);
 t.damageState=stateId(key+"_damage");t.deadState=stateId(key+"_dead");
 const Vec2 start=beat.front().position;const float z=beat.front().z,look=std::isnan(beat.front().look)?0.f:beat.front().look;
 t.keys={{0,start,z,look},{60000,start,z,look,0,1}};
 auto& ai=t.ai;ai.mode=ActorAiMode::Guard;ai.role=role;ai.faction=AiFaction::Raider;ai.squad=stateId(squad);ai.armed=true;
 ai.weapon=weapon;ai.sightRange=sight;ai.sightDot=dot;ai.searchMs=7000;ai.speed=speed;ai.alertState=alert;ai.patrol=std::move(beat);
 m.actorTracks.push_back(std::move(t));
}
// A troffer (recessed fluorescent) in a suspended ceiling.
WorldLight perTroffer(Vec2 p,float ceil,float intensity=.95f,float range=6.5f,float kelvin=4300,float tint=.12f){return lamp(p,ceil-.16f,intensity,range,kelvin,tint);}
// A doorway through a wall that runs along y (an east or west wall): jamb
// posts, a header to the ceiling. No leaf: leaves only span x.
void perDoorwayY(Map& m,float x1,float x2,float y1,float y2,float f,float ceil){
 for(float y:{y1-.08f,y2})wall(m,x1-.02f,y,x2+.02f,y+.08f,f,f+2.2f,20);
 wall(m,x1-.02f,y1-.08f,x2+.02f,y2+.08f,f+2.12f,f+2.2f,20);wall(m,x1,y1,x2,y2,f+2.2f,ceil,21);
}
// A glazed partition along y (officeGlass handles both axes; this keeps a plaster dado).
void perGlass(Map& m,float x1,float y1,float x2,float y2,float f,float ceil){officeGlass(m,x1,y1,x2,y2,f,ceil);}
// A wall-hung item: a fixture mounted with its back on a wall face.
void perWallFix(Map& m,int model,float x,float y,float z,float w,float d,float h,float yaw){perFix(m,model,x,y,z,w,d,h,yaw,false);}
// A lamp switched by a state: lit while it is set, or (invert) until it is; flicker 0..1.
WorldLight perSwitch(WorldLight l,StateId state,bool invert=false,float flicker=0){l.enabledState=state;l.invert=invert;l.flicker=flicker;return l;}
ScriptEvent perEvent(const char* id,float x1,float y1,float x2,float y2,float bottom,float top,StateId require=0){
 ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=bottom;e.top=top;e.requireState=require;return e;
}
ScriptAction perSet(StateId id,int value=1){ScriptAction a;a.type=ScriptAction::Type::SetState;a.id=id;a.value=value;return a;}
ScriptAction perSound(Sound s,float gain){ScriptAction a;a.type=ScriptAction::Type::PlaySound;a.sound=s;a.amount=gain;return a;}
ScriptAction perSave(){ScriptAction a;a.type=ScriptAction::Type::Checkpoint;return a;}
// A sequence started only by setting its timer (its trigger box is off the map),
// optionally ticking a sound; an Alarm tick pulses the red screen wash with it.
void perSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,Sound tick=Sound::Metal1,int intervalMs=0,int untilMs=0,float gain=.5f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=-2;s.y1=-2;s.x2=-1;s.y2=-1;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=intervalMs;s.soundUntilMs=untilMs;s.tickSound=tick;s.gain=gain;s.pitch=1;
 m.timedSequences.push_back(s);
}
// The deck layer at height z (after every deck() call: deck() may reallocate the layers).
const AuthoredLayerData* perDeck(const Map& m,float z){for(const auto& l:m.layers)if(l.thickness>0&&std::fabs(l.elevation-z)<.01f)return &l;return nullptr;}
// A glazed band from z1 to z2 in a wall run: glass between steel mullions, sill and head.
void perWindow(Map& m,float x1,float y1,float x2,float y2,float z1,float z2,float bay=1.2f){
 const bool ax=x2-x1>y2-y1;const float c=ax?(y1+y2)*.5f:(x1+x2)*.5f,len=ax?x2-x1:y2-y1,a=ax?x1:y1,b=ax?x2:y2;
 if(ax)wall(m,x1,c-.01f,x2,c+.01f,z1,z2,17);else wall(m,c-.01f,y1,c+.01f,y2,z1,z2,17);
 const int n=std::max(1,int(len/bay+.5f));
 for(int i=0;i<=n;++i){float s=std::clamp(a+len*i/n-.03f,a,b-.06f);
  if(ax)wall(m,s,y1,s+.06f,y2,z1,z2,12);else wall(m,x1,s,x2,s+.06f,z1,z2,12);}
 wall(m,x1,y1,x2,y2,z1-.05f,z1,12);wall(m,x1,y1,x2,y2,z2,z2+.05f,12);
}
// A full-height glazed screen on a steel sill: glass between mullions every 1.5 m,
// a transom at door height and a head rail at `top`.
void perCurtain(Map& m,float x1,float y1,float x2,float y2,float f,float top){
 const bool ax=x2-x1>y2-y1;const float c=ax?(y1+y2)*.5f:(x1+x2)*.5f,len=ax?x2-x1:y2-y1,a=ax?x1:y1,b=ax?x2:y2;
 wall(m,x1,y1,x2,y2,f,f+.3f,12);
 if(ax)wall(m,x1,c-.012f,x2,c+.012f,f+.3f,top,17);else wall(m,c-.012f,y1,c+.012f,y2,f+.3f,top,17);
 const int n=std::max(1,int(len/1.5f+.5f));
 for(int i=0;i<=n;++i){float s=std::clamp(a+len*i/n-.04f,a,b-.08f);
  if(ax)wall(m,s,y1,s+.08f,y2,f+.3f,top,12);else wall(m,x1,s,x2,s+.08f,f+.3f,top,12);}
 wall(m,x1,y1,x2,y2,f+2.15f,f+2.21f,12);wall(m,x1,y1,x2,y2,top-.08f,top,12);
}
// A chain-link run along x or y: purchased mesh panels between steel posts,
// a top rail and a strand of barbed wire.
void perFence(Map& m,float x1,float y1,float x2,float y2,float f,float h=2.4f){
 const bool ax=x2-x1>y2-y1;const float len=ax?x2-x1:y2-y1;const int n=std::max(1,int(std::ceil(len/3.f-.05f)));const float seg=len/n;
 for(int i=0;i<n;++i){float t=(ax?x1:y1)+seg*(i+.5f);
  if(ax)perFix(m,model::mine_mesh_fence,t,y1,f,seg-.08f,.06f,h,0,true);else perFix(m,model::mine_mesh_fence,x1,t,f,seg-.08f,.06f,h,kPi*.5f,true);}
 for(int i=0;i<=n;++i){float t=(ax?x1:y1)+seg*i,px=ax?t:x1,py=ax?y1:t;wall(m,px-.04f,py-.04f,px+.04f,py+.04f,f,f+h+.3f,12);}
 if(ax){wall(m,x1,y1-.02f,x2,y1+.02f,f+h-.04f,f+h,12);m.pipes.push_back({{x1,y1},{x2,y1},f+h+.22f,.008f,f+h+.22f,6});}
 else{wall(m,x1-.02f,y1,x1+.02f,y2,f+h-.04f,f+h,12);m.pipes.push_back({{x1,y1},{x1,y2},f+h+.22f,.008f,f+h+.22f,6});}
}
// A sandbag wall: staggered courses of bags up to height h.
void perSandbags(Map& m,float x1,float y1,float x2,float y2,float f,float h){
 const bool ax=x2-x1>=y2-y1;const float a=ax?x1:y1,b=ax?x2:y2;int course=0;
 for(float z=f;z<f+h-.02f;z+=.15f,++course)for(float t=a-(course%2)*.28f;t<b;t+=.56f){
  float s=std::max(t,a)+.012f,e=std::min(t+.56f,b)-.012f;if(e-s<.12f)continue;
  float in=course%2?.03f:.0f;
  if(ax)wall(m,s,y1+in,e,y2-in,z,z+.14f,23);else wall(m,x1+in,s,x2-in,e,z,z+.14f,23);}
 wall(m,x1+.02f,y1+.02f,x2-.02f,y2-.02f,f,f+h-.03f,CollisionProxyMaterial);
}
// A floodlight mast: footing, rolled column, a crossarm with two heads, the
// conduit and fused box at the base, ringed by bollards.
void perMast(Map& m,float x,float y,float f,float top,float kelvin=2000,float tint=-.15f,float intensity=2.3f,float range=13.f){
 wall(m,x-.45f,y-.45f,x+.45f,y+.45f,f,f+.3f,15);
 steelColumn(m,x,y,f+.3f,top);
 wall(m,x-1.15f,y-.06f,x+1.15f,y+.06f,top-.18f,top,12);
 for(float dx:{-.9f,.9f}){
  wall(m,x+dx-.04f,y-.04f,x+dx+.04f,y+.04f,top-.42f,top-.18f,12);
  wall(m,x+dx-.24f,y-.2f,x+dx+.24f,y+.2f,top-.72f,top-.42f,8);
  wall(m,x+dx-.21f,y-.17f,x+dx+.21f,y+.17f,top-.75f,top-.72f,20);
  m.lights.push_back(lamp({x+dx,y},top-.82f,intensity,range,kelvin,tint,WorldLightMount::Bare));
 }
 m.pipes.push_back({{x+.21f,y},{x+.21f,y},f+1.3f,.025f,top-.18f,6});
 wall(m,x+.15f,y-.14f,x+.33f,y+.14f,f+.9f,f+1.35f,8);
 for(float dx:{-.95f,.95f})for(float dy:{-.95f,.95f})perFix(m,model::pwr_safety_bollard,x+dx,y+dy,f,.15f,.15f,1.1f,0,true);
}
// A body bag on the floor: the bag, the body's bulk in it, the zip and a toe tag.
void perBodyBag(Map& m,float x,float y,float f,bool alongY){
 const float hx=alongY?.3f:.95f,hy=alongY?.95f:.3f;
 wall(m,x-hx,y-hy,x+hx,y+hy,f,f+.1f,8);
 wall(m,x-hx*.8f,y-hy*.88f,x+hx*.8f,y+hy*.88f,f+.1f,f+.2f,8);
 if(alongY){wall(m,x-.15f,y-hy*.86f,x+.15f,y-hy*.56f,f+.2f,f+.25f,8);wall(m,x-.012f,y-hy*.8f,x+.012f,y+hy*.8f,f+.2f,f+.206f,20);wall(m,x-.04f,y+hy,x+.04f,y+hy+.07f,f+.04f,f+.05f,20);}
 else{wall(m,x-hx*.86f,y-.15f,x-hx*.56f,y+.15f,f+.2f,f+.25f,8);wall(m,x-hx*.8f,y-.012f,x+hx*.8f,y+.012f,f+.2f,f+.206f,20);wall(m,x+hx,y-.04f,x+hx+.07f,y+.04f,f+.04f,f+.05f,20);}
}
// A sign plate hung from the roof on two rods, lettered on both faces. alongY: the
// plate runs along y and faces east (first text) and west (second); otherwise it
// faces south (first) and north (second).
void perHangingSign(Map& m,float x,float y,float z,bool alongY,float width,const char* title,const char* sub,const char* backTitle,const char* backSub,std::uint32_t accent,float roof){
 const float h=.65f;
 if(alongY){wall(m,x-.15f,y-width*.5f,x+.15f,y+width*.5f,z-.02f,z+h+.02f,2);
  for(float s:{-1.f,1.f})m.pipes.push_back({{x,y+s*width*.4f},{x,y+s*width*.4f},z+h+.02f,.022f,roof,6});
  m.signs.push_back({{x+.162f,y},z,width,h,kPi*.5f,title,sub,accent});m.signs.push_back({{x-.162f,y},z,width,h,-kPi*.5f,backTitle,backSub,accent});}
 else{wall(m,x-width*.5f,y-.15f,x+width*.5f,y+.15f,z-.02f,z+h+.02f,2);
  for(float s:{-1.f,1.f})m.pipes.push_back({{x+s*width*.4f,y},{x+s*width*.4f,y},z+h+.02f,.022f,roof,6});
  m.signs.push_back({{x,y+.162f},z,width,h,0,title,sub,accent});m.signs.push_back({{x,y-.162f},z,width,h,kPi,backTitle,backSub,accent});}
}
// A steel table: top, four legs and a low shelf.
void perTable(Map& m,float x1,float y1,float x2,float y2,float f,float h=.86f){
 wall(m,x1,y1,x2,y2,f+h-.05f,f+h,20);
 for(float x:{x1+.04f,x2-.09f})for(float y:{y1+.04f,y2-.09f})wall(m,x,y,x+.05f,y+.05f,f,f+h-.05f,12);
 wall(m,x1+.06f,y1+.06f,x2-.06f,y2-.06f,f+.22f,f+.25f,12);
}
// A running line along x at y: two rails on sleepers, gauge 1.4 m.
void perRailX(Map& m,float y,float x1,float x2,float f){
 for(float dy:{-.7f,.7f})wall(m,x1,y+dy-.045f,x2,y+dy+.045f,f,f+.07f,2);
 for(float x=x1+.2f;x<x2-.1f;x+=.8f)wall(m,x,y-1.f,x+.12f,y+1.f,f,f+.02f,8);
}
// A tripod work light: three legs to a lamp head and a bare lamp in front of it.
void perWorkLight(Map& m,float x,float y,float f,float h,float faceX,float faceY,float intensity=1.5f,float range=8.f){
 for(Vec2 leg:{Vec2{-.3f,.2f},Vec2{.3f,.2f},Vec2{0,-.34f}})m.pipes.push_back({{x+leg.x,y+leg.y},{x,y},f,.016f,f+h,6});
 wall(m,x-.17f,y-.13f,x+.17f,y+.13f,f+h,f+h+.3f,8);
 wall(m,x+faceX*.14f-.12f,y+faceY*.14f-.1f,x+faceX*.14f+.12f,y+faceY*.14f+.1f,f+h+.04f,f+h+.26f,20);
 m.lights.push_back(lamp({x+faceX*.3f,y+faceY*.3f},f+h+.15f,intensity,range,5600,.04f,WorldLightMount::Bare));
}

// =============================================================================================
// OPERATIONS CORRIDOR (manifest 36). The clean line. The Fabrication Shop's
// shutter opens onto the dirty side: lockers, the boot wash and the tool drop,
// where workers left the plant behind. A partition with turnstiles and a
// guard booth marks the line; past it the facility changes face: painted
// block, drop ceilings, blue corporate signage, cameras. Perimeter Ops (west)
// watches the yard on CCTV; one Cleanup man holds it with his back to the
// glass. Badging (east) revoked every sub-level badge at 02:40, which is why
// nothing below opens for a badge. The corridor ends at the yard shutter,
// jammed half open: crouch under it into the Security Perimeter.
// States: none gating. Guard squad per_ops (1 man).
std::shared_ptr<const Map> makeOperationsCorridor(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[57];m.definition=CampaignChunks[57];m.definition.ambient=.30f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;
 const float f=-19,ceil=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,true,0});
 openEdge(m,{'S',10.f,14.f,f,false,0});
 m.definition.playerStart={12.f,2.4f};m.definition.spawnYaw=kPi*.5f;
 machineryClock(m,"per_ops_clock",60000);

 // --- Dirty side: the shutter from the Fabrication Shop ---------------------------------------
 for(float x:{9.6f,14.f}){wall(m,x,1.f,x+.4f,1.45f,f,f+3.f,12);
  for(float z=f+.15f;z<f+1.4f;z+=.3f)wall(m,x-.01f,1.45f,x+.41f,1.47f,z,z+.15f,13);}
 wall(m,9.6f,1.f,14.4f,1.45f,f+2.65f,f+3.f,12);
 // Lockers on the west and north walls, benches between them.
 for(int i=0;i<4;++i)perFix(m,model::pwr_locker_bank,1.45f,2.0f+i*.87f,f,.87f,.85f,2.42f,0,true);
 for(int i=0;i<5;++i)perFix(m,model::pwr_locker_bank,3.1f+i*.88f,1.45f,f,.87f,.85f,2.42f,-kPi*.5f,true);
 bench(m,4.9f,4.3f,f);
 m.fixtures.push_back({model::trash_can,{7.6f,2.6f},0,.6f,.6f,.76f,0,true});
 perSign(m,1.055f,6.1f,f+1.6f,"CHANGE HERE","COVERALLS IN THE BIN / BOOTS ON THE RACK",1.4f,kPi*.5f,perAmber);
 // A boot rack with the boots still on it, and coveralls on hooks.
 wall(m,1.05f,5.9f,1.5f,6.75f,f,f+.08f,12);wall(m,1.05f,5.9f,1.5f,6.75f,f+.42f,f+.47f,12);
 for(float y=5.98f;y<6.7f;y+=.24f){wall(m,1.12f,y,1.42f,y+.11f,f+.08f,f+.2f,8);wall(m,1.12f,y,1.42f,y+.11f,f+.47f,f+.59f,8);}
 for(float x:{8.2f,8.7f,9.2f}){m.pipes.push_back({{x,1.f},{x,1.13f},f+1.9f,.015f,f+1.9f,6});wall(m,x-.2f,1.06f,x+.2f,1.18f,f+1.05f,f+1.86f,23);}
 // The boot wash: a steel trough along the east wall, brushes and taps.
 wall(m,21.9f,1.6f,22.95f,5.8f,f,f+.62f,20);wall(m,22.f,1.7f,22.85f,5.7f,f+.6f,f+.63f,8);
 m.pipes.push_back({{22.92f,1.6f},{22.92f,5.8f},f+.95f,.03f,f+.95f,5});
 for(float y:{2.2f,3.4f,4.6f}){m.pipes.push_back({{22.92f,y},{22.79f,y},f+.95f,.022f,f+.95f,5});m.pipes.push_back({{22.79f,y},{22.79f,y},f+.84f,.018f,f+.95f,5});
  wall(m,22.15f,y-.22f,22.55f,y+.22f,f+.63f,f+.7f,10);wall(m,22.2f,y-.18f,22.5f,y+.18f,f+.7f,f+.78f,23);}
 perSign(m,22.945f,3.7f,f+1.35f,"BOOT WASH","SCRUB SOLES / RINSE / THEN THE LINE",1.8f,-kPi*.5f,perAmber);
 // The tool drop: shelving on the north wall east of the shutter.
 for(float x:{16.f,18.f})m.fixtures.push_back({model::metal_shelf,{x,1.35f},0,1.8f,.5f,1.8f,0,true});
 for(float x:{15.6f,16.4f,17.7f,18.5f})m.fixtures.push_back({model::packing_crate,{x,1.35f},.92f,.5f,.36f,.3f,0,false});
 perSign(m,17.f,1.055f,f+2.05f,"TOOL DROP","NO TOOLS PAST THE CLEAN LINE",2.f,0,perAmber);
 // Clean line paint across the whole floor.
 floorLine(m,1.f,6.25f,23.f,6.45f,f,13);
 floorLine(m,10.3f,1.5f,10.42f,6.25f,f,14);floorLine(m,13.58f,1.5f,13.7f,6.25f,f,14);

 // --- The line: partition, turnstiles and the guard booth -------------------------------------
 wall(m,1.f,7.f,9.6f,7.3f,f,ceil,15);wall(m,14.4f,7.f,23.f,7.3f,f,ceil,15);
 wall(m,9.6f,7.f,14.4f,7.3f,f+2.6f,ceil,15);
 wall(m,9.6f,6.96f,14.4f,7.34f,f+2.52f,f+2.6f,20);
 perSign(m,12.f,6.945f,f+2.66f,"RESTRICTED PERIMETER","BADGE / BAG / BOOT CHECK / CAMERAS IN USE",3.6f,kPi,perBlue);
 for(float x:{9.8f,11.f,12.2f,13.4f})m.fixtures.push_back({model::ticket_gate,{x,7.15f},0,.4f,1.42f,1.09f,0,true});
 // Turnstile 2 forced: its flap torn off and thrown down, blood where he fell.
 wall(m,11.35f,8.25f,11.95f,8.33f,f,f+.04f,20);
 perBlood(m,11.0f,8.4f,12.3f,9.5f,f);perBlood(m,11.4f,9.5f,11.9f,10.4f,f);
 perBody(m,"per_ops_clock",ActorVisual::Worker,{11.6f,9.2f},f,.2f);
 // The guard booth faces the shutter; arrivals were checked at its window.
 perGlass(m,15.f,4.2f,15.15f,7.f,f,ceil);perGlass(m,15.f,4.2f,18.5f,4.35f,f,ceil);
 officeWall(m,18.35f,4.35f,18.5f,5.1f,f,ceil);perDoorwayY(m,18.35f,18.5f,5.1f,6.2f,f,ceil);officeWall(m,18.35f,6.2f,18.5f,7.f,f,ceil);
 wall(m,15.15f,4.35f,18.35f,4.95f,f,f+.95f,21);wall(m,15.1f,4.3f,18.4f,5.f,f+.95f,f+.99f,20);
 terminal(m,{15.75f,6.1f},"CLEAN LINE / TURNSTILE LOG","T2 FORCED 03:12 / BADGE: NONE / ALARM ACK.","RESPONSE: CLEANUP DELTA-2. INCIDENT CLOSED.");
 m.terminals.back().yaw=kPi*.5f;
 perFix(m,model::crt_monitor,16.3f,4.65f,f+.99f,.42f,.4f,.38f,0,false);
 perFix(m,model::cctv_tv,17.5f,4.65f,f+.99f,.5f,.4f,.42f,0,false);
 perFix(m,model::desk_phone,16.9f,4.7f,f+.99f,.2f,.2f,.1f,0,false);
 m.lights.push_back(lamp({16.9f,5.6f},ceil-.16f,.8f,4.5f,4000,.1f));
 // An emergency lamp on the booth: lockdown red over the line.
 m.lights.push_back(lamp({14.99f,6.5f},f+2.75f,1.1f,5.f,1300,-.4f,WorldLightMount::Wall,kPi));
 m.lights.back().flicker=.15f;

 // --- Dirty-side services and light ---------------------------------------------------------
 servicePipe(m,{1.2f,2.4f},{22.8f,2.4f},ceil-.35f,.09f,1);
 servicePipe(m,{1.2f,3.0f},{22.8f,3.0f},ceil-.3f,.05f,0);
 cableTray(m,{1.4f,5.6f},{22.6f,5.6f},ceil-.45f);
 for(Vec2 l:{Vec2{5.5f,4.f},Vec2{20.f,4.f}})m.lights.push_back(lamp(l,ceil-.5f,1.15f,7.5f,2300,-.15f));
 m.lights.push_back(lamp({8.f,1.02f},f+2.7f,1.1f,6.f,4200,.05f,WorldLightMount::Wall,kPi*.5f));

 // --- Clean side: the corridor walls -------------------------------------------------------
 // West: Perimeter Ops behind glass, then the washroom.
 perGlass(m,9.4f,7.3f,9.6f,13.4f,f,ceil);perDoorwayY(m,9.4f,9.6f,13.4f,14.6f,f,ceil);officeWall(m,9.4f,14.6f,9.6f,15.2f,f,ceil);
 officeWall(m,1.f,15.f,9.4f,15.2f,f,ceil);
 officeWall(m,9.4f,15.2f,9.6f,16.4f,f,ceil);perDoorwayY(m,9.4f,9.6f,16.4f,17.6f,f,ceil);officeWall(m,9.4f,17.6f,9.6f,23.f,f,ceil);
 // East: Badging behind its counter glass, then the ready room.
 perGlass(m,14.4f,7.3f,14.6f,12.4f,f,ceil);perDoorwayY(m,14.4f,14.6f,12.4f,13.6f,f,ceil);officeWall(m,14.4f,13.6f,14.6f,14.2f,f,ceil);
 officeWall(m,14.6f,14.f,23.f,14.2f,f,ceil);
 officeWall(m,14.4f,14.2f,14.6f,15.4f,f,ceil);perDoorwayY(m,14.4f,14.6f,15.4f,16.6f,f,ceil);officeWall(m,14.4f,16.6f,14.6f,23.f,f,ceil);
 // The clean side's south face of the partition is plaster.
 officeWall(m,1.f,7.3f,9.4f,7.36f,f,ceil);officeWall(m,14.6f,7.3f,23.f,7.36f,f,ceil);
 // Floors: office tile in the corridor and offices, clinic tile in the washroom.
 perFinish(m,9.6f,7.3f,14.4f,22.5f,f,24);perFinish(m,1.f,7.36f,9.4f,15.f,f,24);perFinish(m,14.6f,7.36f,23.f,14.f,f,24);
 perFinish(m,1.f,15.2f,9.4f,23.f,f,27);perFinish(m,14.6f,14.2f,23.f,23.f,f,23);
 ceilingGrid(m,1.f,7.36f,23.f,23.f,ceil);

 // --- Perimeter Ops: the yard on CCTV, the mimic board, the man watching it -----------------
 for(int i=0;i<4;++i){float y=8.6f+i*.92f;
  perFix(m,model::file_cabinet_wide,1.32f,y,f,.9f,.5f,.75f,kPi*.5f,true);
  perFix(m,model::cctv_tv,1.32f,y,f+.75f,.5f,.4f,.42f,kPi*.5f,false);
  perFix(m,model::cctv_tv,1.32f,y,f+1.17f,.5f,.4f,.42f,kPi*.5f,false);}
 perDesk(m,2.75f,10.f,f,-kPi*.5f,1);
 m.lights.push_back(lamp({1.75f,10.f},f+1.45f,.55f,3.2f,7500,.25f,WorldLightMount::Bare));
 // The mimic board: the yard, the gatehouse and Delta in route paint.
 wall(m,2.6f,7.36f,7.4f,7.46f,f+1.62f,f+2.5f,8);
 for(auto seg:{std::tuple{3.f,1.85f,7.f,1.9f},std::tuple{3.f,2.22f,6.2f,2.27f},std::tuple{4.9f,1.72f,4.95f,2.4f},std::tuple{6.2f,1.85f,6.25f,2.27f}})
  {auto [x1,z1,x2,z2]=seg;wall(m,x1,7.46f,x2,7.47f,f+z1,f+z2,14);}
 for(float x:{3.4f,4.4f,5.6f,6.6f})wall(m,x,7.46f,x+.08f,7.48f,f+2.02f,f+2.1f,x<5?19:18);
 perSign(m,5.f,7.415f,f+2.6f,"PERIMETER MIMIC","YARD / GATEHOUSE / DELTA",2.4f,0,perBlue);
 for(float x:{3.6f,5.f,6.4f})perWallFix(m,model::lab_control_board,x,7.455f,f+.9f,1.32f,.19f,.6f,kPi);
 perDesk(m,5.f,9.4f,f,kPi,2);
 terminal(m,{6.6f,13.9f},"PERIMETER OPS / SHIFT LOG","02:51 DELTA-2 HAS THE YARD. OPS STAFF STOOD DOWN.","03:05 GATE RELEASE LOCKED TO THE GATEHOUSE / EAST.");
 m.terminals.back().yaw=kPi;
 perFix(m,model::file_cabinet_tall,1.3f,14.4f,f,.5f,.6f,1.3f,kPi*.5f,true);
 perFix(m,model::water_cooler,8.9f,8.f,f,.35f,.35f,1.1f,0,true);
 perSign(m,1.055f,12.7f,f+1.7f,"YARD FEEDS","CAM 1-8 / RECORDING OFF SINCE 02:51",1.6f,kPi*.5f,perBlue);
 m.lights.push_back(perTroffer({4.f,9.2f},ceil));m.lights.push_back(perTroffer({6.f,12.6f},ceil,.8f));
 perGuard(m,"per_ops_clock","per_ops",0,ActorVisual::Cleanup,GuardSquadRole::Assault,
  {{{3.9f,10.7f},f,9000,-kPi*.5f},{{5.6f,8.4f},f,5000,kPi},{{3.6f,12.3f},f,4000,-kPi*.5f}},11.f,.6f,2,1.3f);

 // --- Washroom ----------------------------------------------------------------------------------
 for(float x:{2.2f,4.2f,6.2f})perFix(m,model::med_washroom_stall,x,22.3f,f,1.92f,1.39f,2.16f,kPi,true);
 for(float y:{16.4f,17.4f,18.4f})perFix(m,model::med_scrub_sink,1.32f,y,f,.49f,.54f,1.13f,kPi*.5f,true);
 for(float y:{16.9f,18.f})perWallFix(m,model::med_washroom_mirror,1.04f,y,f+1.2f,1.f,.04f,.67f,kPi*.5f);
 perWallFix(m,model::med_first_aid_box,7.6f,15.27f,f+1.35f,.37f,.12f,.35f,0);
 m.pickupSpawns.push_back({{7.6f,16.f},PickupKind::Health,f});
 perSign(m,9.655f,19.8f,f+1.65f,"STAFF WASHROOM","WASH HANDS / REPORT RASHES TO MEDICAL",1.5f,kPi*.5f,perBlue);
 m.lights.push_back(perTroffer({5.f,18.6f},ceil,.75f,6.f,4300,.32f));m.lights.back().flicker=.45f;

 // --- Badging: the corporate face --------------------------------------------------------------
 wall(m,14.6f,7.9f,15.5f,12.2f,f,f+1.02f,21);wall(m,14.55f,7.85f,15.55f,12.25f,f+1.02f,f+1.06f,20);
 perDesk(m,16.4f,10.f,f,-kPi*.5f,0);
 perFix(m,model::lab_photocopier,16.3f,8.f,f,.83f,.75f,.97f,kPi,true);
 perSign(m,22.945f,10.4f,f+1.2f,"DEPTHWORKS","SMILE. YOUR BADGE IS YOUR KEY.",2.6f,-kPi*.5f,perBlue);
 for(float y:{9.9f,10.9f})floorLine(m,21.4f,y,21.75f,y+.2f,f+.02f,14);
 perFix(m,model::wooden_chair,22.1f,10.4f,f,.42f,.52f,.95f,-kPi*.5f,true);
 // The badge camera on its tripod.
 wall(m,18.57f,10.47f,18.63f,10.53f,f,f+1.35f,8);
 for(Vec2 leg:{Vec2{18.6f,10.2f},Vec2{18.86f,10.65f},Vec2{18.34f,10.65f}})wall(m,std::min(leg.x,18.6f)-.02f,std::min(leg.y,10.5f)-.02f,std::max(leg.x,18.6f)+.02f,std::max(leg.y,10.5f)+.02f,f,f+.03f,8);
 wall(m,18.48f,10.38f,18.8f,10.62f,f+1.35f,f+1.55f,8);wall(m,18.4f,10.45f,18.48f,10.55f,f+1.4f,f+1.5f,2);
 terminal(m,{19.6f,13.f},"BADGE SYSTEM / REVOCATION RUN","02:40 ALL SUB-LEVEL BADGES VOID: 212 HOLDERS.","READERS LOCKED. MANUAL OVERRIDES ONLY.");
 m.terminals.back().yaw=kPi;
 for(float x:{21.3f,22.f})perFix(m,model::file_cabinet_tall,x,7.7f,f,.5f,.6f,1.3f,0,true);
 perWallFix(m,model::lab_notice_board,17.6f,13.93f,f+.95f,1.2f,.07f,.9f,0);
 perWallFix(m,model::lab_wall_clock,20.f,7.4f,f+2.25f,.4f,.03f,.4f,kPi);
 perSign(m,14.345f,9.6f,f+2.3f,"BADGING","PHOTO / ISSUE / REVOKE",1.6f,-kPi*.5f,perBlue);
 m.lights.push_back(perTroffer({17.5f,9.5f},ceil,.9f));m.lights.push_back(perTroffer({20.5f,11.8f},ceil,.85f));

 // --- Perimeter security ready room: the rifles are gone -------------------------------------------
 for(int i=0;i<6;++i)perFix(m,model::pwr_locker_bank,22.56f,15.3f+i*.88f,f,.87f,.85f,2.42f,kPi,true);
 perWallBlood(m,true,22.1f,19.1f,19.6f,f+.6f,f+1.6f,.02f);
 perBody(m,"per_ops_clock",ActorVisual::Cleanup,{21.3f,19.4f},f,-kPi*.4f);
 perBlood(m,20.4f,18.9f,21.9f,20.1f,f);
 m.pickupSpawns.push_back({{20.6f,16.f},PickupKind::Ammo,f});
 // The rifle rack, empty.
 wall(m,15.f,22.6f,18.6f,22.95f,f,f+.1f,12);wall(m,15.f,22.85f,18.6f,22.95f,f+.1f,f+1.7f,12);wall(m,15.f,22.6f,18.6f,22.95f,f+1.1f,f+1.16f,12);
 for(float x=15.3f;x<18.5f;x+=.42f)wall(m,x,22.62f,x+.06f,22.85f,f+.1f,f+.22f,8);
 perSign(m,16.8f,22.945f,f+1.85f,"RIFLE RACK","SIGN OUT WITH THE DUTY SERGEANT",1.8f,kPi,perAmber);
 m.fixtures.push_back({model::wooden_table,{17.6f,18.4f},0,1.367f,.724f,.78f,0,true});
 for(float x:{17.1f,18.1f}){m.fixtures.push_back({model::wooden_chair,{x,17.6f},0,.417f,.525f,.95f,0,true});m.fixtures.push_back({model::wooden_chair,{x,19.2f},0,.417f,.525f,.95f,kPi,true});}
 perFix(m,model::mug,17.3f,18.3f,f+.78f,.09f,.09f,.1f,0,false);
 perFix(m,model::binder,17.9f,18.5f,f+.78f,.3f,.08f,.32f,.4f,false);
 terminal(m,{16.2f,15.f},"PERIMETER / DUTY BOARD","ORDER 7 IN EFFECT: NO STAFF PAST DELTA. NO EXCEPTIONS.","WRITTEN UNDER IT IN PEN: WHO SIGNED THIS?");
 m.terminals.back().yaw=0;
 perWallFix(m,model::lab_notice_board,18.4f,14.27f,f+.95f,1.2f,.07f,.9f,kPi);
 perFix(m,model::coffee_machine,15.f,21.4f,f+.8f,.35f,.35f,.45f,kPi*.5f,false);
 wall(m,14.6f,20.8f,15.4f,22.1f,f,f+.8f,21);wall(m,14.6f,20.75f,15.45f,22.15f,f+.8f,f+.84f,20);
 perFix(m,model::water_cooler,15.f,19.9f,f,.35f,.35f,1.1f,0,true);
 m.lights.push_back(perTroffer({18.f,17.f},ceil,.9f,6.5f,4100,.08f));m.lights.push_back(perTroffer({20.5f,21.f},ceil,.75f,6.f,4100,.08f));m.lights.back().flicker=.3f;

 // --- The corridor ------------------------------------------------------------------------------
 perCamera(m,9.6f,8.2f,f+2.85f,1,0);perCamera(m,14.4f,21.6f,f+2.85f,-1,0);
 perSign(m,9.655f,11.f,f+2.42f,"CAMERAS IN USE","YOUR SAFETY IS MONITORED",1.6f,kPi*.5f,perBlue);
 perSign(m,14.345f,18.6f,f+1.55f,"DEPTHWORKS","SAFETY IS A SHARED RESPONSIBILITY",1.8f,-kPi*.5f,perBlue);
 m.fixtures.push_back({model::bench,{9.85f,20.3f},0,1.86f,.44f,.63f,kPi*.5f,true});
 for(Vec2 pt:{Vec2{10.f,7.75f},Vec2{14.f,15.f}})m.fixtures.push_back({model::lab_potted_plant,pt,0,.37f,.43f,.69f,0,false});
 perWallFix(m,model::pwr_fire_cabinet,14.3f,20.6f,f+1.1f,.25f,.2f,.56f,-kPi*.5f);
 perWallFix(m,model::lab_wall_clock,9.62f,15.8f,f+2.3f,.4f,.03f,.4f,-kPi*.5f);
 perWallFix(m,model::lab_pa_horn,14.12f,15.f,f+2.7f,.49f,.56f,.43f,kPi*.5f);
 perWallFix(m,model::lab_id_scanner,9.63f,14.95f,f+1.1f,.3f,.06f,.41f,-kPi*.5f);
 perWallFix(m,model::lab_id_scanner,14.37f,14.9f,f+1.1f,.3f,.06f,.41f,kPi*.5f);
 // The yard shutter, jammed at knee height: crouch under it.
 wall(m,9.6f,22.45f,10.f,23.f,f,ceil,12);wall(m,14.f,22.45f,14.4f,23.f,f,ceil,12);
 wall(m,9.95f,22.55f,14.05f,22.68f,f+.92f,f+2.75f,7);
 for(float z=f+1.1f;z<f+2.7f;z+=.3f)wall(m,10.f,22.5f,14.f,22.55f,z,z+.05f,2);
 wall(m,9.95f,22.5f,14.05f,22.7f,f+.86f,f+.94f,13);
 wall(m,9.6f,22.3f,14.4f,23.f,f+2.75f,ceil,12);
 perSign(m,12.f,22.295f,f+2.8f,"SECURITY PERIMETER","YARD / ESCORTED STAFF ONLY",2.6f,kPi,perRed);
 perWallFix(m,model::lab_status_display,12.f,22.27f,f+2.52f,.69f,.06f,.2f,0);
 floorLine(m,9.6f,21.6f,14.4f,21.8f,f+.02f,13);
 for(Vec2 l:{Vec2{12.f,9.6f},Vec2{12.f,13.6f},Vec2{12.f,17.6f}})m.lights.push_back(perTroffer(l,ceil,1.f,7.f));
 m.lights[m.lights.size()-1].flicker=.35f;
 m.lights.push_back(lamp({9.62f,21.2f},f+2.4f,1.f,4.5f,1300,-.4f,WorldLightMount::Wall,0));
 m.lights.push_back(perTroffer({12.f,20.6f},ceil,.7f,5.f));

 zone(m,"OPERATIONS / CLEAN LINE",1,1,23,7.3f);
 zone(m,"PERIMETER OPS",1,7.3f,9.4f,15.f);
 zone(m,"BADGING",14.6f,7.3f,23,14.f);
 zone(m,"PERIMETER SECURITY / READY ROOM",14.6f,14.2f,23,23);
 zone(m,"STAFF WASHROOM",1,15.2f,9.4f,23);
 zone(m,"OPERATIONS CORRIDOR",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// =============================================================================================
// SECURITY PERIMETER / YARD (manifest 37). Nine metres of concrete under sodium
// floods between the plant and the restricted zone. Staff came through Check 1
// at the north portal and walked the painted lane to the Delta gate in the
// south wall; vehicles used Lock 2 in the west wall until Delta-2 sealed it at
// 02:51. The Cleanup detail holds the yard: one man walks the lane, a
// sharpshooter watches from Sentry 1 over the south-west corner, a heavy sits in
// the sandbags beside the gate. The gate release is in the gatehouse, east (59).
// Stealth or fight: the booth, the van, the barriers and the platform's shadow
// are cover; the booth intercom rings Lock 2's bell and pulls the lane man west.
// Any alert throws the alarm floods on and sounds the yard klaxon.
// States: perim_gate (set in 59) opens the south door. per_yard_alert (squad alert
// flag, shared with 59) drives the alarm floods and the klaxon; per_yard_bell is
// the intercom lure.
std::shared_ptr<const Map> makeSecurityPerimeterYard(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[58];m.definition=CampaignChunks[58];m.definition.ambient=.24f;
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 const float f=-19,ceil=m.definition.ceiling,zd=-16.2f;
 const StateId gate=stateId("perim_gate"),alert=stateId("per_yard_alert");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 openEdge(m,{'N',10.f,14.f,f,false,0});
 openEdge(m,{'E',4.f,20.f,f,false,0});
 openEdge(m,{'S',10.f,14.f,f,true,gate});
 m.definition.playerStart={12.f,1.9f};m.definition.spawnYaw=kPi*.5f;m.definition.spawnHeight=f;
 machineryClock(m,"per_yard_clock",60000);

 // --- The hall: pilasters, roof trusses and the opening east into the gatehouse half -------
 for(float x:{4.f,7.f,17.f,21.f})wall(m,x-.3f,1,x+.3f,1.3f,f,ceil,15);
 for(float x:{4.f,20.f})wall(m,x-.3f,22.7f,x+.3f,23,f,ceil,15);
 for(float y:{3.5f,14.5f})wall(m,1,y-.3f,1.3f,y+.3f,f,ceil,15);
 for(float y:{3.f,7.f,11.f,15.f,19.f}){wall(m,1,y-.1f,24,y+.1f,ceil-.75f,ceil,12);wall(m,1,y-.2f,24,y+.2f,ceil-.8f,ceil-.75f,12);}
 wall(m,23,4,24,20,ceil-1.6f,ceil,15);
 servicePipe(m,{1.2f,2.f},{24.f,2.f},ceil-1.25f,.12f,1);
 servicePipe(m,{1.2f,21.9f},{24.f,21.9f},ceil-1.1f,.08f,5);
 cableTray(m,{1.4f,1.55f},{9.2f,1.55f},f+4.f);cableTray(m,{14.8f,1.55f},{24.f,1.55f},f+4.f);

 // --- The north portal from the Operations Corridor ---------------------------------------
 wall(m,10,0,14,1,f+2.75f,ceil,15);
 for(float x:{9.3f,14.f})wall(m,x,1,x+.7f,1.45f,f,f+3.6f,15);
 wall(m,10.f,1,14.f,1.45f,f+2.75f,f+3.6f,15);
 wall(m,9.95f,.98f,14.05f,1.5f,f+2.7f,f+2.78f,12);
 for(float x:{9.3f,14.62f})for(float z=f+.15f;z<f+1.4f;z+=.4f)wall(m,x,1.45f,x+.08f,1.47f,z,z+.2f,13);
 perSign(m,12.f,1.505f,f+2.85f,"OPERATIONS CORRIDOR","CLEAN LINE / BADGING / PERIMETER OPS",3.2f,0,perBlue);
 m.lights.push_back(lamp({12.f,1.02f},f+4.2f,.9f,6.f,4300,.15f,WorldLightMount::Wall,kPi*.5f));
 perCamera(m,6.f,1.f,f+4.5f,0,1);
 perSign(m,19.f,1.055f,f+3.2f,"SECURITY PERIMETER","CAMERAS IN USE / ESCORTED STAFF ONLY BEYOND THIS POINT",3.2f,0,perBlue);

 // --- The staff lane: barriers, paint and the arm ---------------------------------------------
 for(float y:{3.f,4.7f})perFix(m,model::barrier,9.55f,y,f,1.6f,.8f,.88f,kPi*.5f,true);
 for(float y:{3.f,4.7f,6.4f})perFix(m,model::barrier,14.45f,y,f,1.6f,.8f,.88f,kPi*.5f,true);
 for(float x:{10.3f,13.6f})floorLine(m,x,1.5f,x+.12f,7.3f,f,14);
 floorLine(m,10.3f,7.f,13.7f,7.15f,f,14);
 // The arm is down; the pedestrian side west of it is open.
 wall(m,13.75f,7.35f,14.25f,7.85f,f,f+1.05f,18);wall(m,13.7f,7.3f,14.3f,7.9f,f+1.05f,f+1.1f,12);
 for(int i=0;i<7;++i){float x=13.75f-(i+1)*.53f;wall(m,x,7.55f,x+.53f,7.65f,f+.94f,f+1.02f,i%2?20:19);}
 wall(m,14.25f,7.52f,14.7f,7.68f,f+.92f,f+1.04f,8);
 perSign(m,14.f,7.295f,f+1.12f,"STOP","BADGE CHECK",.5f,kPi,perRed);

 // --- Check 1: the arrivals booth; its intercom is wired to the Lock 2 bell ---------------------
 {const float x1=6.2f,y1=2.4f,x2=9.f,y2=5.4f,top=f+2.55f;
  wall(m,x1,y1,x2,y2,f,f+.12f,15);
  wall(m,x1,y1,x2,y1+.12f,f+.12f,top,3);
  wall(m,x1,y1+.12f,x1+.12f,3.3f,f+.12f,top,3);wall(m,x1,4.3f,x1+.12f,y2,f+.12f,top,3);
  perDoorwayY(m,x1,x1+.12f,3.3f,4.3f,f,top);
  wall(m,x2-.12f,y1+.12f,x2,y2,f+.12f,f+1.05f,3);perWindow(m,x2-.12f,y1+.12f,x2,y2,f+1.05f,f+2.2f,1.f);wall(m,x2-.12f,y1+.12f,x2,y2,f+2.25f,top,3);
  wall(m,x1+.12f,y2-.12f,x2-.12f,y2,f+.12f,f+1.05f,3);perWindow(m,x1+.12f,y2-.12f,x2-.12f,y2,f+1.05f,f+2.2f,1.f);wall(m,x1+.12f,y2-.12f,x2-.12f,y2,f+2.25f,top,3);
  wall(m,x1-.12f,y1-.12f,x2+.12f,y2+.12f,top,top+.16f,2);
  terminal(m,{8.35f,3.9f},"CHECK 1 / INTERCOM","LINE 2: VEHICLE LOCK 2 BELL. TEST ONLY.","E / RING IT. LAST ENTRY 03:10: 14 IN TIES. ARM UP.",.12f,stateId("per_yard_bell"));
  m.terminals.back().yaw=-kPi*.5f;
  perFix(m,model::office_chair,7.45f,3.95f,f+.12f,.55f,.55f,.95f,-kPi*.5f+.3f,true);
  wall(m,6.4f,2.52f,7.5f,2.9f,f+1.3f,f+1.34f,12);
  perFix(m,model::cctv_tv,6.8f,2.72f,f+1.34f,.5f,.4f,.42f,0,false);
  perFix(m,model::binder,7.3f,2.68f,f+1.34f,.3f,.08f,.32f,0,false);
  perWallFix(m,model::med_first_aid_box,6.38f,4.8f,f+1.35f,.37f,.12f,.35f,kPi*.5f);
  m.lights.push_back(lamp({7.6f,3.9f},top-.1f,.7f,3.5f,6200,.4f));
  perSign(m,7.6f,2.345f,f+1.6f,"CHECK 1","ALL STAFF STOP / BADGE AND BAG",2.2f,kPi,perBlue);}

 // --- Vehicle Lock 2 in the west wall: sealed by Delta-2, its bell still wired ---------------
 for(float y:{5.6f,11.6f})wall(m,1,y,1.55f,y+.5f,f,f+5.2f,18);
 wall(m,1,5.6f,1.55f,12.1f,f+4.6f,f+5.2f,18);
 wall(m,1,6.1f,1.18f,11.6f,f,f+4.6f,7);
 for(float z=f+.25f;z<f+4.5f;z+=.3f)wall(m,1.18f,6.1f,1.22f,11.6f,z,z+.05f,2);
 for(float y:{6.1f,11.52f})wall(m,1.18f,y,1.26f,y+.08f,f,f+4.6f,8);
 floorLine(m,1.3f,6.1f,2.4f,11.6f,f,13);
 perSign(m,1.275f,8.85f,f+2.4f,"VEHICLE LOCK 2","SEALED 02:51 BY DELTA-2 / NO VEHICLE MOVEMENTS",3.f,kPi*.5f,perRed);
 wall(m,1.f,8.8f,1.62f,8.9f,f+5.38f,f+5.48f,8);
 m.pipes.push_back({{1.6f,8.85f},{1.8f,8.85f},f+5.43f,.16f,f+5.43f,4});
 perWallFix(m,model::pwr_disconnect_switch,1.18f,12.75f,f+1.f,.43f,.35f,.85f,kPi*.5f);
 m.lights.push_back(perSwitch(lamp({1.57f,8.85f},f+4.9f,1.4f,9.f,1800,-.2f,WorldLightMount::Wall,0),0,false,.15f));
 perSign(m,1.055f,4.6f,f+2.4f,"VEHICLES 5 KM/H","STAFF ON FOOT: LANE ONLY",1.4f,kPi*.5f,perAmber);
 // The security van Delta-2 parked across the lock, and the detail's stores.
 perFix(m,model::service_van,4.3f,8.9f,f,2.05f,4.9f,2.15f,0,true);
 cargo(m,2.4f,2.3f,f);cargo(m,3.75f,2.3f,f,false);
 perFix(m,model::packing_crate,3.75f,2.3f,f+.16f,.9f,.6f,.5f,.2f,true);

 // --- Floodlight masts ------------------------------------------------------------------------
 perMast(m,7.5f,14.6f,f,f+6.6f);perMast(m,17.6f,6.8f,f,f+6.6f);

 // --- The vehicle lane to Delta: serpentine, spike strip, stop line ---------------------------
 for(float y=9.f;y<21.f;y+=2.f)floorLine(m,11.94f,y,12.06f,y+1.f,f,14);
 perFix(m,model::barrier,10.8f,11.6f,f,1.6f,.8f,.88f,0,true);
 perFix(m,model::barrier,13.2f,14.2f,f,1.6f,.8f,.88f,0,true);
 perFix(m,model::barrier,10.8f,16.8f,f,1.6f,.8f,.88f,0,true);
 wall(m,10.2f,19.45f,13.8f,19.7f,f,f+.035f,8);
 for(float x=10.3f;x<13.75f;x+=.24f)wall(m,x,19.5f,x+.05f,19.65f,f+.035f,f+.09f,20);
 floorLine(m,9.2f,21.55f,14.8f,21.85f,f,13);

 // --- The Delta gate: blast pylons, the frame, status lamps --------------------------------------
 wall(m,7.6f,22.2f,10.f,23,f,ceil,15);wall(m,14.f,22.2f,16.4f,23,f,ceil,15);
 wall(m,10.f,22.2f,14.f,23,f+3.1f,ceil,15);
 wall(m,10.f,22.2f,10.15f,23,f,f+3.1f,18);wall(m,13.85f,22.2f,14.f,23,f,f+3.1f,18);
 wall(m,10.f,22.2f,14.f,22.4f,f+2.8f,f+3.1f,18);
 for(float x=10.2f;x<13.8f;x+=.5f)wall(m,x,22.16f,x+.25f,22.2f,f+2.83f,f+3.07f,13);
 for(float x:{7.6f,16.25f})for(float z=f+.2f;z<f+2.2f;z+=.5f)wall(m,x,22.16f,x+.15f,22.2f,z,z+.25f,13);
 perSign(m,12.f,22.145f,f+3.3f,"CHECKPOINT DELTA","ESCORTED STAFF ONLY / RELEASE FROM THE GATEHOUSE",3.8f,kPi,perRed);
 perSign(m,15.2f,22.145f,f+1.45f,"GATE RELEASE","GATEHOUSE / EAST",1.5f,kPi,perAmber);
 perSign(m,8.8f,22.145f,f+1.45f,"WAIT HERE","FOR THE GATE / CAMERAS ACTIVE",1.5f,kPi,perBlue);
 perWallFix(m,model::lab_status_display,12.f,22.17f,f+4.05f,.69f,.06f,.2f,kPi);
 perCamera(m,15.8f,22.2f,f+3.9f,0,-1);
 m.lights.push_back(perSwitch(lamp({9.2f,22.18f},f+2.75f,1.3f,6.f,1100,-.45f,WorldLightMount::Wall,-kPi*.5f),gate,true));
 m.lights.push_back(perSwitch(lamp({14.8f,22.18f},f+2.75f,1.2f,6.f,5200,.5f,WorldLightMount::Wall,-kPi*.5f),gate));

 // --- Delta-2's gun position east of the gate ---------------------------------------------------
 perSandbags(m,15.9f,18.6f,19.3f,19.1f,f,1.f);
 perSandbags(m,15.9f,19.1f,16.4f,21.5f,f,1.f);
 perSandbags(m,18.8f,19.1f,19.3f,21.5f,f,1.f);
 perFix(m,model::packing_crate,17.15f,21.75f,f,.6f,.45f,.36f,0,true);
 perFix(m,model::packing_crate,18.f,21.8f,f,.6f,.45f,.36f,.3f,true);
 perFix(m,model::wall_box,17.15f,21.75f,f+.36f,.36f,.12f,.28f,0,false);
 perWorkLight(m,17.6f,19.45f,f,1.65f,0,-1,1.5f,8.f);

 // --- Sentry 1: a platform over the south-west corner, covering the gate approach ------------
 deck(m,zd,1,17,6,23);
 m.stairs.push_back({1.3f,12.8f,2.7f,17.f,f,zd,15,true,true});
 guardRail(m,2.7f,16.97f,6.f,17.03f,zd);guardRail(m,5.97f,17.f,6.03f,22.95f,zd);
 if(auto* l=perDeck(m,zd))deckSupports(m,*l,f,3);
 perSandbags(m,2.9f,17.1f,5.9f,17.45f,zd,.7f);perSandbags(m,5.5f,17.45f,5.9f,22.9f,zd,.7f);
 wall(m,5.05f,17.65f,5.15f,17.75f,zd,zd+1.5f,8);
 wall(m,4.92f,17.5f,5.28f,17.86f,zd+1.5f,zd+1.82f,8);wall(m,4.95f,17.47f,5.25f,17.5f,zd+1.53f,zd+1.79f,20);
 m.lights.push_back(lamp({5.1f,17.35f},zd+1.66f,2.f,12.f,6000,.05f,WorldLightMount::Bare));
 perSign(m,1.055f,12.4f,f+2.2f,"SENTRY 1","AUTHORIZED ONLY / KEEP THE STAIR CLEAR",1.6f,kPi*.5f,perAmber);
 for(float x:{1.9f,2.68f})perFix(m,model::packing_crate,x,21.95f,f,.7f,.5f,.42f,0,true);
 perFix(m,model::packing_crate,2.3f,21.95f,f+.42f,.7f,.5f,.42f,.15f,false);
 m.lights.push_back(lamp({3.6f,19.8f},zd-.3f,.6f,4.f,2400,-.1f));

 // --- Stock for the gun position, and the forklift that brought it ------------------------------
 forklift(m,21.5f,3.2f,f);
 perFix(m,model::pallet,18.8f,2.6f,f,1.2f,1.15f,.16f,0,true);
 perSandbags(m,18.3f,2.1f,19.3f,3.1f,f+.16f,.6f);

 // --- Light: high bays, and the alarm floods that come on when the detail is alerted ---------
 for(Vec2 l:{Vec2{5.f,7.f},Vec2{18.f,17.f}})m.lights.push_back(lamp(l,ceil-.95f,.8f,10.f,4000,.1f));
 for(float x:{8.5f,20.f})m.lights.push_back(perSwitch(lamp({x,1.02f},f+6.2f,2.6f,16.f,6500,.04f,WorldLightMount::Wall,kPi*.5f),alert));
 m.lights.push_back(perSwitch(lamp({12.f,22.18f},f+5.6f,2.6f,16.f,6500,.04f,WorldLightMount::Wall,-kPi*.5f),alert));
 perSequence(m,"per_yard_klaxon","per_yard_klaxon_done",14000,13000,Sound::Alarm,1500,9500,.7f);
 {auto e=perEvent("per_yard_alarm",0,0,24,24,-100,100,alert);e.actions={perSet(stateId("per_yard_klaxon"))};m.scriptEvents.push_back(e);}

 // --- The intercom lure: Lock 2's bell rings in the west -----------------------------------------
 perSequence(m,"per_yard_bell","per_yard_bell_done",9000,8000);
 for(int i=0;i<4;++i)cue(m,"per_yard_bell",300+i*900,i%2?Sound::JunkMetal:Sound::Door,{2.f,8.85f},i?"":"LOCK 2 BELL: RINGING, WEST WALL.",1.f,f+4.8f);

 // --- Delta-2 in the yard --------------------------------------------------------------------
 perGuard(m,"per_yard_clock","per_yard",0,ActorVisual::Cleanup,GuardSquadRole::Assault,
  {{{12.6f,10.2f},f,4500,0},{{18.6f,12.6f},f,3500,kPi*.5f},{{8.2f,10.4f},f,4000,-kPi*.5f},{{15.f,8.4f},f,3000,kPi}},12.f,.6f,1,1.2f,alert);
 perGuard(m,"per_yard_clock","per_yard",1,ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,
  {{{3.4f,19.2f},zd,7000,kPi*.75f},{{4.6f,21.f},zd,5000,kPi*.55f}},16.f,.72f,1,1.f,alert);
 perGuard(m,"per_yard_clock","per_yard",2,ActorVisual::CleanupHeavy,GuardSquadRole::Heavy,
  {{{17.6f,20.6f},f,9000,kPi},{{17.2f,21.f},f,6000,kPi*.8f}},11.f,.55f,1,1.f,alert);

 m.pickupSpawns={{{7.f,4.85f},PickupKind::Health,f},{{2.4f,4.f},PickupKind::Ammo,f},{{2.1f,21.f},PickupKind::Ammo,zd}};
 m.clutterSpawns={{2,{5.6f,6.f},f},{0,{9.f,8.4f},f},{2,{6.4f,11.8f},f},{0,{15.4f,17.6f},f},{2,{19.8f,12.f},f}};
 zone(m,"SECURITY PERIMETER / CHECK 1",6.2f,2.4f,9.f,5.4f);
 zone(m,"SECURITY PERIMETER / SENTRY 1",1,17,6,23,zd-.5f,1000);
 zone(m,"SECURITY PERIMETER / DELTA GATE",7.f,18.f,17.f,23.f);
 zone(m,"SECURITY PERIMETER / VEHICLE LOCK 2",1,5,6.f,12.5f);
 zone(m,"SECURITY PERIMETER / YARD",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// SECURITY PERIMETER / GATEHOUSE (manifest 37)
std::shared_ptr<const Map> makeSecurityPerimeterGatehouse(){return placeholderChunk(59,{{'W',4.f,20.f,-19.f,false,0}});}
// CHECKPOINT DELTA (manifest 38)
std::shared_ptr<const Map> makeCheckpointDelta(){return placeholderChunk(60,{{'N',10.f,14.f,-19.f,true,0},{'W',9.f,13.f,-16.f,false,0},{'S',10.f,14.f,-19.f,true,stateId("delta_override")}});}
// ADMINISTRATIVE LINK (manifest 39)
std::shared_ptr<const Map> makeAdministrativeLink(){return placeholderChunk(61,{{'E',9.f,13.f,-16.f,false,0}});}
// PERSONNEL TRANSIT (manifest 40)
std::shared_ptr<const Map> makePersonnelTransit(){return placeholderChunk(62,{{'N',10.f,14.f,-19.f,true,0},{'W',14.f,19.f,-19.f,false,0}});}
}
void buildChapterPerimeterA(CampaignMapTable& maps){
 maps[57]=makeOperationsCorridor();
 maps[58]=makeSecurityPerimeterYard();
 maps[59]=makeSecurityPerimeterGatehouse();
 maps[60]=makeCheckpointDelta();
 maps[61]=makeAdministrativeLink();
 maps[62]=makePersonnelTransit();
}
}

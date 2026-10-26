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
TimedSequence& perSequence(Map& m,const char* timer,const char* finish,int durationMs,int finishAtMs,Sound tick=Sound::Metal1,int intervalMs=0,int untilMs=0,float gain=.5f){
 TimedSequence s;s.timerState=stateId(timer);s.finishState=stateId(finish);s.x1=-2;s.y1=-2;s.x2=-1;s.y2=-1;
 s.durationMs=durationMs;s.finishAtMs=finishAtMs;s.soundIntervalMs=intervalMs;s.soundUntilMs=untilMs;s.tickSound=tick;s.gain=gain;s.pitch=1;
 m.timedSequences.push_back(s);return m.timedSequences.back();
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
 // A boot rack with the boots still on it, and coveralls on hooks.
 wall(m,1.05f,5.9f,1.5f,6.75f,f,f+.08f,12);wall(m,1.05f,5.9f,1.5f,6.75f,f+.42f,f+.47f,12);
 for(float y=5.98f;y<6.7f;y+=.24f){wall(m,1.12f,y,1.42f,y+.11f,f+.08f,f+.2f,8);wall(m,1.12f,y,1.42f,y+.11f,f+.47f,f+.59f,8);}
 for(float x:{8.2f,8.7f,9.2f}){m.pipes.push_back({{x,1.f},{x,1.13f},f+1.9f,.015f,f+1.9f,6});wall(m,x-.2f,1.06f,x+.2f,1.18f,f+1.05f,f+1.86f,23);}
 // The boot wash: a steel trough along the east wall, brushes and taps.
 wall(m,21.9f,1.6f,22.95f,5.8f,f,f+.62f,20);wall(m,22.f,1.7f,22.85f,5.7f,f+.6f,f+.63f,8);
 m.pipes.push_back({{22.92f,1.6f},{22.92f,5.8f},f+.95f,.03f,f+.95f,5});
 for(float y:{2.2f,3.4f,4.6f}){m.pipes.push_back({{22.92f,y},{22.79f,y},f+.95f,.022f,f+.95f,5});m.pipes.push_back({{22.79f,y},{22.79f,y},f+.84f,.018f,f+.95f,5});
  wall(m,22.15f,y-.22f,22.55f,y+.22f,f+.63f,f+.7f,10);wall(m,22.2f,y-.18f,22.5f,y+.18f,f+.7f,f+.78f,23);}
 // The tool drop: shelving on the north wall east of the shutter.
 for(float x:{16.f,18.f})m.fixtures.push_back({model::metal_shelf,{x,1.35f},0,1.8f,.5f,1.8f,0,true});
 for(float x:{15.6f,16.4f,17.7f,18.5f})m.fixtures.push_back({model::packing_crate,{x,1.35f},.92f,.5f,.36f,.3f,0,false});
 // Clean line paint across the whole floor.
 floorLine(m,1.f,6.25f,23.f,6.45f,f,13);
 floorLine(m,10.3f,1.5f,10.42f,6.25f,f,14);floorLine(m,13.58f,1.5f,13.7f,6.25f,f,14);

 // --- The line: partition, turnstiles and the guard booth -------------------------------------
 wall(m,1.f,7.f,9.6f,7.3f,f,ceil,15);wall(m,14.4f,7.f,23.f,7.3f,f,ceil,15);
 wall(m,9.6f,7.f,14.4f,7.3f,f+2.6f,ceil,15);
 wall(m,9.6f,6.96f,14.4f,7.34f,f+2.52f,f+2.6f,20);
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
 for(float x:{3.6f,5.f,6.4f})perWallFix(m,model::lab_control_board,x,7.455f,f+.9f,1.32f,.19f,.6f,kPi);
 perDesk(m,5.f,9.4f,f,kPi,2);
 terminal(m,{6.6f,13.9f},"PERIMETER OPS / SHIFT LOG","02:51 DELTA-2 HAS THE YARD. OPS STAFF STOOD DOWN.","03:05 GATE RELEASE LOCKED TO THE GATEHOUSE / EAST.");
 m.terminals.back().yaw=kPi;
 perFix(m,model::file_cabinet_tall,1.3f,14.4f,f,.5f,.6f,1.3f,kPi*.5f,true);
 perFix(m,model::water_cooler,8.9f,8.f,f,.35f,.35f,1.1f,0,true);
 m.lights.push_back(perTroffer({4.f,9.2f},ceil));m.lights.push_back(perTroffer({6.f,12.6f},ceil,.8f));
 perGuard(m,"per_ops_clock","per_ops",0,ActorVisual::Cleanup,GuardSquadRole::Assault,
  {{{3.9f,10.7f},f,9000,-kPi*.5f},{{5.6f,8.4f},f,5000,kPi},{{3.6f,12.3f},f,4000,-kPi*.5f}},11.f,.6f,2,1.3f);

 // --- Washroom ----------------------------------------------------------------------------------
 for(float x:{2.2f,4.2f,6.2f})perFix(m,model::med_washroom_stall,x,22.3f,f,1.92f,1.39f,2.16f,kPi,true);
 for(float y:{16.4f,17.4f,18.4f})perFix(m,model::med_scrub_sink,1.32f,y,f,.49f,.54f,1.13f,kPi*.5f,true);
 for(float y:{16.9f,18.f})perWallFix(m,model::med_washroom_mirror,1.04f,y,f+1.2f,1.f,.04f,.67f,kPi*.5f);
 perWallFix(m,model::med_first_aid_box,7.6f,15.27f,f+1.35f,.37f,.12f,.35f,0);
 m.pickupSpawns.push_back({{7.6f,16.f},PickupKind::Health,f});
 m.lights.push_back(perTroffer({5.f,18.6f},ceil,.75f,6.f,4300,.32f));m.lights.back().flicker=.45f;

 // --- Badging: the corporate face --------------------------------------------------------------
 wall(m,14.6f,7.9f,15.5f,12.2f,f,f+1.02f,21);wall(m,14.55f,7.85f,15.55f,12.25f,f+1.02f,f+1.06f,20);
 perDesk(m,16.4f,10.f,f,-kPi*.5f,0);
 perFix(m,model::lab_photocopier,16.3f,8.f,f,.83f,.75f,.97f,kPi,true);
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
 titleCard(m,"RESTRICTED PERIMETER");
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
 m.lights.push_back(lamp({12.f,1.02f},f+4.2f,.9f,6.f,4300,.15f,WorldLightMount::Wall,kPi*.5f));
 perCamera(m,6.f,1.f,f+4.5f,0,1);

 // --- The staff lane: barriers, paint and the arm ---------------------------------------------
 for(float y:{3.f,4.7f})perFix(m,model::barrier,9.55f,y,f,1.6f,.8f,.88f,kPi*.5f,true);
 for(float y:{3.f,4.7f,6.4f})perFix(m,model::barrier,14.45f,y,f,1.6f,.8f,.88f,kPi*.5f,true);
 for(float x:{10.3f,13.6f})floorLine(m,x,1.5f,x+.12f,7.3f,f,14);
 floorLine(m,10.3f,7.f,13.7f,7.15f,f,14);
 // The arm is down; the pedestrian side west of it is open.
 wall(m,13.75f,7.35f,14.25f,7.85f,f,f+1.05f,18);wall(m,13.7f,7.3f,14.3f,7.9f,f+1.05f,f+1.1f,12);
 for(int i=0;i<7;++i){float x=13.75f-(i+1)*.53f;wall(m,x,7.55f,x+.53f,7.65f,f+.94f,f+1.02f,i%2?20:19);}
 wall(m,14.25f,7.52f,14.7f,7.68f,f+.92f,f+1.04f,8);

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
  }

 // --- Vehicle Lock 2 in the west wall: sealed by Delta-2, its bell still wired ---------------
 for(float y:{5.6f,11.6f})wall(m,1,y,1.55f,y+.5f,f,f+5.2f,18);
 wall(m,1,5.6f,1.55f,12.1f,f+4.6f,f+5.2f,18);
 wall(m,1,6.1f,1.18f,11.6f,f,f+4.6f,7);
 for(float z=f+.25f;z<f+4.5f;z+=.3f)wall(m,1.18f,6.1f,1.22f,11.6f,z,z+.05f,2);
 for(float y:{6.1f,11.52f})wall(m,1.18f,y,1.26f,y+.08f,f,f+4.6f,8);
 floorLine(m,1.3f,6.1f,2.4f,11.6f,f,13);
 perSign(m,1.245f,8.85f,f+2.4f,"VEHICLE LOCK 2","SEALED 02:51 BY DELTA-2 / NO VEHICLE MOVEMENTS",3.f,kPi*.5f,perRed);
 wall(m,1.f,8.8f,1.62f,8.9f,f+5.38f,f+5.48f,8);
 m.pipes.push_back({{1.6f,8.85f},{1.8f,8.85f},f+5.43f,.16f,f+5.43f,4});
 perWallFix(m,model::pwr_disconnect_switch,1.18f,12.75f,f+1.f,.43f,.35f,.85f,kPi*.5f);
 m.lights.push_back(perSwitch(lamp({1.57f,8.85f},f+4.9f,1.4f,9.f,1800,-.2f,WorldLightMount::Wall,0),0,false,.15f));
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
 // The portable light sits on a stores crate, out of the lane and physically supported.
 perFix(m,model::packing_crate,17.6f,19.6f,f,.8f,.8f,.65f,0,true);
 perWorkLight(m,17.6f,19.6f,f+.65f,1.f,0,-1,1.5f,8.f);

 // --- Sentry 1: a platform over the south-west corner, covering the gate approach ------------
 deck(m,zd,1,17,6,23);
 m.stairs.push_back({1.3f,12.8f,2.7f,17.f,f,zd,15,true,true});
 guardRail(m,2.7f,16.97f,6.f,17.03f,zd);guardRail(m,5.97f,17.f,6.03f,22.95f,zd);
 if(auto* l=perDeck(m,zd))deckSupports(m,*l,f,3);
 perSandbags(m,2.9f,17.1f,5.9f,17.45f,zd,.7f);perSandbags(m,5.5f,17.45f,5.9f,22.9f,zd,.7f);
 wall(m,5.05f,17.65f,5.15f,17.75f,zd,zd+1.5f,8);
 wall(m,4.92f,17.5f,5.28f,17.86f,zd+1.5f,zd+1.82f,8);wall(m,4.95f,17.47f,5.25f,17.5f,zd+1.53f,zd+1.79f,20);
 m.lights.push_back(lamp({5.1f,17.35f},zd+1.66f,2.f,12.f,6000,.05f,WorldLightMount::Bare));
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
 {auto& k=perSequence(m,"per_yard_klaxon","per_yard_klaxon_done",14000,13000,Sound::Alarm,1500,9500,.7f);k.soundAt={14.f,1.4f};k.soundZ=f+6.f;} // the horn on the north wall, between the floods
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

 m.pickupSpawns={{{7.f,4.85f},PickupKind::Health,f+.12f},{{2.4f,4.f},PickupKind::Ammo,f},{{2.1f,21.f},PickupKind::Ammo,zd}};
 m.clutterSpawns={{2,{5.6f,6.f},f},{0,{9.f,8.4f},f},{2,{6.4f,11.8f},f},{0,{15.4f,17.6f},f},{2,{19.8f,12.f},f}};
 zone(m,"SECURITY PERIMETER / CHECK 1",6.2f,2.4f,9.f,5.4f);
 zone(m,"SECURITY PERIMETER / SENTRY 1",1,17,6,23,zd-.5f,1000);
 zone(m,"SECURITY PERIMETER / DELTA GATE",7.f,18.f,17.f,23.f);
 zone(m,"SECURITY PERIMETER / VEHICLE LOCK 2",1,5,6.f,12.5f);
 zone(m,"SECURITY PERIMETER / YARD",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
// SECURITY PERIMETER / GATEHOUSE (manifest 37). The yard's east half and the
// gatehouse that runs it: a block building with a window band on the yard,
// the gate desk and its CCTV wall, and the detail's bunk room behind it.
// The Cleanup van is parked where it unloaded. States: perim_gate (gate desk)
// opens the yard's south door; the guards share the yard's alert.
std::shared_ptr<const Map> makeSecurityPerimeterGatehouse(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[59];m.definition=CampaignChunks[59];m.definition.ambient=.26f;
 const float f=-19,roof=m.definition.ceiling,top=f+3.3f;
 const StateId alert=stateId("per_yard_alert");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'W',4.f,20.f,-19.f,false,0});
 // --- The yard's east end: fences, the van, the barrier line and a mast -----------------------
 perFence(m,1.f,3.6f,10.f,3.6f,f);perFence(m,1.f,20.4f,10.f,20.4f,f);
 perFix(m,model::service_van,4.6f,15.4f,f,2.3f,5.1f,2.2f,.08f,true);
 m.clutterSpawns.push_back({1,{6.4f,13.2f},f,.6f});m.clutterSpawns.push_back({2,{3.f,12.6f},f});
 for(float y:{6.f,9.f})perFix(m,model::road_block,7.2f,y,f,1.6f,.5f,.7f,kPi*.5f,true);
 perSandbags(m,8.6f,10.6f,9.4f,13.4f,f,1.1f);
 perMast(m,3.f,9.f,f,f+7.f);
 for(float x=1.5f;x<9.6f;x+=1.f)perFinish(m,x,11.95f,x+.55f,12.05f,f,13);
 // --- The gatehouse: walls, window band, door and roof ------------------------------------------
 wall(m,10.f,6.f,10.2f,8.f,f,top,3);wall(m,10.f,9.6f,10.2f,17.f,f,f+1.f,3);
 perWindow(m,10.f,9.6f,10.2f,17.f,f+1.f,f+2.4f);wall(m,10.f,9.6f,10.2f,17.f,f+2.45f,top,3);
 perDoorwayY(m,10.f,10.2f,8.f,9.6f,f,top);
 wall(m,10.f,6.f,23.f,6.2f,f,top,3);wall(m,10.f,16.8f,23.f,17.f,f,top,3);
 wall(m,10.f,6.f,23.f,17.f,top,top+.25f,15);
 perFinish(m,10.2f,6.2f,23.f,16.8f,f,24);
 perCamera(m,10.f,16.6f,f+3.f,-1,0);
 // --- Behind the gatehouse: its services on the north face, the detail's smoking spot on the south ----
 // North: the supply cabinet, conduit along the wall to two condensers on a
 // pad, a bulkhead over them and a generator parked against the yard wall.
 perFix(m,model::electrical_cabinet,11.6f,5.73f,f,.9066f,.4956f,2.2f,0,true);
 m.pipes.push_back({{11.6f,5.96f},{11.6f,5.96f},f+2.2f,.035f,f+2.9f,6});
 m.pipes.push_back({{11.6f,5.96f},{21.4f,5.96f},f+2.9f,.035f,f+2.9f,6});
 for(float x:{15.f,19.4f}){
  m.pipes.push_back({{x,5.96f},{x,5.96f},f+1.1f,.03f,f+2.9f,6});
  wall(m,x-1.1f,4.55f,x+1.1f,5.95f,f,f+.12f,15);
  perFix(m,model::compressor,x,5.2f,f+.12f,1.8f,1.05f,.9f,0,true);
 }
 m.lights.push_back(lamp({17.2f,5.98f},f+2.6f,.9f,6.f,2400,-.1f,WorldLightMount::Wall,-kPi*.5f));
 perFix(m,model::mine_portable_generator,20.2f,1.9f,f,.87f,1.04f,.82f,.25f,true);
 for(float x:{22.2f,22.6f})perFix(m,model::gas_cylinder,x,1.45f,f,.33f,.33f,1.58f,0,true);
 // South: a bench against the wall, a sand bucket of butts, the bins and a
 // stack of empties waiting for a run that never came.
 perFix(m,model::bench,13.6f,17.32f,f,1.86f,.44f,.63f,kPi,true);
 perFix(m,model::trash_can,15.1f,17.4f,f,.5f,.5f,.62f,0,true);
 m.lights.push_back(lamp({14.4f,17.02f},f+2.6f,.8f,5.f,2400,-.1f,WorldLightMount::Wall,kPi*.5f));
 perFix(m,model::dumpster,20.4f,22.2f,f,1.96f,1.27f,1.17f,0,true);
 for(float z:{0.f,.16f})perFix(m,model::pallet,11.6f,21.9f,f+z,1.2f,1.15f,.16f,z*2.f,true);
 perFix(m,model::ind_crate,11.6f,21.9f,f+.32f,.8f,.8f,.8f,.3f,true);
 // --- Gate desk and the CCTV wall --------------------------------------------------------------
 wall(m,10.2f,12.65f,15.6f,12.82f,f,f+2.1f,2);
 for(float x:{11.f,12.6f,14.2f}){perFix(m,model::file_cabinet_wide,x,13.2f,f,.9f,.6f,.75f,0,true);perFix(m,model::cctv_tv,x,13.2f,f+.75f,.6f,.45f,.5f,kPi,false);}
 perSign(m,12.f,12.638f,f+1.45f,"PERIMETER GATE / RELEASE","SOUTH GATE / CHECKPOINT DELTA",1.45f,kPi,perAmber);
 terminal(m,{12.f,12.f},"PERIMETER GATE / RELEASE","E / RELEASE THE YARD'S SOUTH GATE TO CHECKPOINT DELTA.","DELTA IS THROUGH THE SOUTH GATE, BACK ACROSS THE YARD.",0,stateId("perim_gate"));
 m.terminals.back().yaw=kPi;
 perDesk(m,14.6f,9.f,f,kPi*.5f,1);
 terminal(m,{11.6f,7.f},"GATEHOUSE / DETAIL LOG","03:10 YARD SECURE. 14 PROCESSED THROUGH DELTA. 0 ESCAPED.","ORDER 7: NO ONE LEAVES THE SUB-LEVELS. ANYONE IN A SHIFT BADGE IS STAFF.");
 m.terminals.back().yaw=.001f;
 // --- The bunk room -----------------------------------------------------------------------------
 wall(m,16.6f,6.2f,16.8f,12.f,f,top,21);wall(m,16.6f,13.6f,16.8f,16.8f,f,top,21);wall(m,16.6f,12.f,16.8f,13.6f,f+2.2f,top,21);
 for(float y:{7.4f,10.4f})perFix(m,model::med_ward_bed,21.8f,y,f,1.98f,.94f,.84f,0,true);
 perFix(m,model::med_ward_bed_tossed,21.8f,14.f,f,2.1f,1.5f,.84f,0,true);
 for(float x:{17.4f,18.3f})perFix(m,model::pwr_locker_bank,x,16.3f,f,.7f,.68f,1.94f,kPi,true);
 perTable(m,18.6f,9.f,19.8f,11.f,f,.76f);
 perFix(m,model::ind_crate,19.9f,7.f,f,.8f,.8f,.8f,.2f,true);
 m.pickupSpawns.push_back({{19.2f,13.2f},PickupKind::Ammo,f});
 m.pickupSpawns.push_back({{20.f,15.6f},PickupKind::Health,f});
 // --- Guards: one on the desk, one in the bunk room, one walking the fence --------------------------
 machineryClock(m,"per_gatehouse_clock",60000);
 perGuard(m,"per_gatehouse_clock","per_gatehouse",0,ActorVisual::Cleanup,GuardSquadRole::Assault,
  {{{13.f,10.6f},f,4000,kPi},{{11.f,10.6f},f,3000,-kPi*.5f}},12,.55f,1,1.4f,alert);
 perGuard(m,"per_gatehouse_clock","per_gatehouse",1,ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,
  {{{19.f,12.8f},f,5000,-kPi*.5f},{{18.f,8.f},f,3500,kPi}},13,.5f,2,1.3f,alert);
 perGuard(m,"per_gatehouse_clock","per_gatehouse",2,ActorVisual::CleanupRusher,GuardSquadRole::Rusher,
  {{{6.f,5.f},f,3000,kPi*.5f},{{6.f,19.f},f,3000,-kPi*.5f},{{2.f,12.f},f,2500,0}},11,.6f,1,1.6f,alert);
 // --- Light -------------------------------------------------------------------------------------
 for(Vec2 l:{Vec2{12.5f,9.f},Vec2{14.f,15.f},Vec2{20.f,9.f},Vec2{20.f,14.f}})m.lights.push_back(perTroffer(l,top));
 m.lights.push_back(lamp({12.4f,13.3f},f+1.4f,.6f,3.f,7000,.2f,WorldLightMount::Bare));
 m.lights.push_back(lamp({9.98f,7.f},f+2.6f,1.f,5.f,4300,.1f,WorldLightMount::Wall,kPi));
 for(Vec2 l:{Vec2{16.f,3.f},Vec2{16.f,20.f}})m.lights.push_back(lamp(l,roof-.4f,1.8f,11.f,2000,-.15f));
 zone(m,"SECURITY PERIMETER / GATEHOUSE",10,6,16.6f,17);
 zone(m,"SECURITY PERIMETER / BUNK ROOM",16.6f,6,23,17);
 zone(m,"SECURITY PERIMETER / YARD",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// CHECKPOINT DELTA (manifest 38). The hard access point between the plant and
// the research levels. The inspection lane runs north to south between the
// scanner portal and the sentry desk; the holding cells along the east wall
// are where the detail kept the people it caught. A stair climbs to the
// security office overlooking the lane, and on west into the Administrative
// Link. The south door to Personnel Transit opens only on the chief's override.
std::shared_ptr<const Map> makeCheckpointDelta(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[60];m.definition=CampaignChunks[60];m.definition.ambient=.24f;
 const float f=-19,mz=-16,roof=m.definition.ceiling;
 const StateId over=stateId("delta_override");
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'N',10.f,14.f,-19.f,true,0});openEdge(m,{'W',9.f,13.f,-16.f,false,0});openEdge(m,{'S',10.f,14.f,-19.f,true,over});
 // --- The security office mezzanine (west) and its stair -----------------------------------------
 wall(m,0.f,9.f,1.f,13.f,f,mz-.22f,15);
 deck(m,mz,0,9,1,13);deck(m,mz,1,5,6,16);
 if(auto* d=perDeck(m,mz))deckSupports(m,*d,f,4.f);
 m.stairs.push_back({2.f,16.f,4.f,20.2f,f,mz,15,true,false});
 wall(m,6.f,5.f,6.12f,16.f,mz,mz+1.f,21);perWindow(m,6.f,5.f,6.12f,16.f,mz+1.f,mz+2.5f);
 wall(m,6.f,5.f,6.12f,16.f,mz+2.55f,roof,21);
 for(float y:{7.f,10.f,13.f})perDesk(m,4.4f,y,mz,kPi*.5f,int(y));
 terminal(m,{2.f,6.f},"DELTA / PROCESSING TALLY","IN: 14 STAFF. OUT: 0. TRANSIT SEALED 02:40 BY ORDER 7.","SOUTH DOOR ON THE CHIEF'S OVERRIDE ONLY. ADEYEMI NEVER CAME DOWN FROM ADMIN.",mz-f);
 m.terminals.back().yaw=.001f;
 // --- The inspection lane: scanner portal, turnstiles and the sentry desk --------------------------
 for(float x:{9.f,15.f}){wall(m,x-.15f,4.5f,x+.15f,6.f,f,f+2.6f,20);}
 wall(m,8.85f,4.5f,15.15f,6.f,f+2.6f,f+3.6f,20);
 perWallFix(m,model::lab_id_scanner,9.2f,5.2f,f+1.1f,.3f,.06f,.41f,kPi*.5f);
 for(float x:{10.4f,12.f,13.6f})perFix(m,model::ticket_gate,x,8.f,f,1.f,.3f,1.f,0,true);
 perTable(m,9.5f,10.f,11.f,11.2f,f);
 perFix(m,model::crt_monitor,10.2f,10.6f,f+.86f,.42f,.4f,.38f,kPi,false);
 perSandbags(m,13.2f,10.f,15.f,10.6f,f,1.f);
 perFinish(m,9.4f,1.f,14.6f,22.f,f,14);
 // --- The holding cells: east wall, bars, the sealed door --------------------------------------
 for(float y:{3.f,9.f,15.f})wall(m,16.f,y-.1f,23.f,y+.1f,f,f+3.f,15);
 wall(m,16.f,20.9f,23.f,21.1f,f,f+3.f,15);wall(m,16.f,2.9f,23.f,21.1f,f+3.f,f+3.25f,15);
 // The doorway panels collide only either side of their opening.
 for(float y:{6.f,12.f,18.f}){perFix(m,model::med_cell_bars,16.05f,y-1.45f,f,2.5f,.04f,2.4f,kPi*.5f,true);perFix(m,model::med_cell_bars_doorway,16.05f,y+1.25f,f,2.5f,.04f,2.4f,kPi*.5f,false);
  for(float s:{-1.f,1.f})wall(m,16.03f,std::min(y+1.25f+s*.585f,y+1.25f+s*1.25f),16.07f,std::max(y+1.25f+s*.585f,y+1.25f+s*1.25f),f,f+2.4f,CollisionProxyMaterial);}
 wall(m,15.9f,2.9f,16.f,21.1f,f+2.4f,f+3.35f,15);
 perBodyBag(m,19.6f,5.f,f,false);perBodyBag(m,20.f,7.2f,f,false);perBodyBag(m,19.4f,11.f,f,false);
 perBlood(m,18.f,13.f,21.6f,16.4f,f);perWallBlood(m,true,22.98f,13.f,17.f,f+.3f,f+1.6f,-.01f);
 perFix(m,model::med_ward_bed_tossed,21.2f,18.f,f,2.1f,1.5f,.84f,kPi*.5f,true);
 // The door in the north-east corner, welded shut with plate.
 wall(m,22.6f,.95f,23.f,2.95f,f,f+2.4f,7);wall(m,22.55f,1.f,22.6f,2.9f,f+.1f,f+2.3f,11);
 perSign(m,22.495f,1.95f,f+1.2f,"DO NOT OPEN","WELDED 02:52 / ORDER 7",1.2f,-kPi*.5f,perRed);
 // --- The south door ---------------------------------------------------------------------------
 perSign(m,18.f,22.945f,f+1.6f,"SOUTH DOOR","CHIEF'S OVERRIDE REQUIRED",2.2f,kPi,perRed);
 // --- Who is still here -----------------------------------------------------------------------
 machineryClock(m,"per_delta_clock",60000);
 perGuard(m,"per_delta_clock","per_delta",0,ActorVisual::CleanupSharpshooter,GuardSquadRole::Sharpshooter,
  {{{4.f,15.f},mz,5000,kPi*.5f},{{3.f,8.5f},mz,4000,kPi*.5f}},14,.5f,2,1.2f);
 // One of the fourteen the detail processed, still in the holding cells: changed.
 // It crouches among the body bags until the player walks the lane past the bars.
 {const StateId wake=stateId("per_delta_cells_wake");
  m.creatureSpawns.push_back({CreatureKind::Snatcher,{20.f,12.f},f,1,0,wake});
  auto e=perEvent("per_delta_cells_trip",11.5f,3.f,16.f,21.f,f-.5f,f+1.5f);e.actions={perSet(wake),perSound(Sound::JunkMetal,.7f)};m.scriptEvents.push_back(e);}
 m.pickupSpawns.push_back({{13.6f,2.4f},PickupKind::Ammo,f});
 m.pickupSpawns.push_back({{2.4f,14.6f},PickupKind::Health,mz});
 // --- Light ------------------------------------------------------------------------------------
 for(Vec2 l:{Vec2{12.f,3.f},Vec2{12.f,11.f},Vec2{12.f,18.f}})m.lights.push_back(lamp(l,roof-.4f,1.8f,10.f,5600,.05f));
 for(float y:{6.f,12.f,18.f})m.lights.push_back(perSwitch(lamp({20.f,y},f+2.9f,.7f,4.f,4300,.3f),0,false,.4f));
 for(Vec2 l:{Vec2{3.5f,8.f},Vec2{3.5f,14.f}})m.lights.push_back(perTroffer(l,roof));
 zone(m,"CHECKPOINT DELTA / SECURITY OFFICE",0,5,6.2f,16,mz-.5f,1000);
 zone(m,"CHECKPOINT DELTA / HOLDING",16,2.9f,23,21.1f);
 zone(m,"CHECKPOINT DELTA",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// ADMINISTRATIVE LINK (manifest 39). The perimeter's offices, hung over the
// old ore pass: open-plan desks, records, and the security chief's office,
// all behind a glass gallery that looks down twelve metres into the shaft the
// plant was built around. The chief refused Order 7 and stayed at his desk.
std::shared_ptr<const Map> makeAdministrativeLink(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[61];m.definition=CampaignChunks[61];m.definition.ambient=.24f;
 m.definition.playerStart={22.f,11.f};m.definition.spawnHeight=-16;m.definition.spawnYaw=kPi;
 const float f=-28,o=-16,roof=m.definition.ceiling,ceil=o+3.f;
 m.layers.push_back(floorLayer(m.name.c_str(),f));
 m.wallSurface=SurfaceOfficePlaster;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'E',9.f,13.f,-16.f,false,0});
 // --- The office floor, hung over the shaft --------------------------------------------------
 wall(m,23.f,9.f,24.f,13.f,f,o-.22f,15);
 deck(m,o,1,1,24,16);
 if(auto* d=perDeck(m,o))deckSupports(m,*d,f,4.f);
 perFinish(m,1.f,1.f,23.f,15.8f,o,23);
 wall(m,1.f,1.f,23.f,15.8f,ceil,ceil+.2f,22);
 ceilingGrid(m,1.f,1.f,23.f,15.8f,ceil);
 // The glass gallery over the shaft.
 perCurtain(m,1.f,15.8f,23.f,16.f,o,ceil);
 // --- The shaft below: the old chute, a dead conveyor, sodium lamps far down ------------------------
 wall(m,4.f,19.f,9.f,23.f,f,f+5.f,11);wall(m,4.5f,19.5f,8.5f,22.5f,f+5.f,f+8.f,11);
 beltConveyor(m,9.f,22.f,20.f,f+1.2f,roof);
 for(float x:{11.f,17.f})steelColumn(m,x,21.5f,f,f+1.2f);
 for(Vec2 l:{Vec2{6.f,17.5f},Vec2{18.f,18.f}})m.lights.push_back(lamp(l,f+3.f,2.f,10.f,1900,-.2f,WorldLightMount::Bare));
 // --- Open plan -------------------------------------------------------------------------------
 for(int i=0;i<3;++i){perDesk(m,13.f+i*3.2f,4.f,o,0,i);perDesk(m,13.f+i*3.2f,7.4f,o,kPi,i+1);}
 for(float x:{14.f,17.f})perFix(m,model::file_cabinet_tall,x,1.45f,o,.5f,.6f,1.4f,0,true);
 perFix(m,model::water_cooler,22.5f,2.f,o,.4f,.4f,1.2f,-kPi*.5f,true);
 perFix(m,model::lab_photocopier,22.4f,5.f,o,.83f,.75f,.97f,-kPi*.5f,true);
 terminal(m,{19.f,13.f},"ADMIN / PERSONNEL NOTICE","02:40 ALL SUB-LEVEL BADGES REVOKED. REPORT TO DELTA FOR PROCESSING.","THIS IS A ROUTINE SECURITY MEASURE. YOUR COOPERATION IS VALUED.",o-f);
 m.terminals.back().yaw=.001f;
 // --- Records (centre) --------------------------------------------------------------------------
 officeWall(m,10.f,1.f,10.15f,9.f,o,ceil);officeWall(m,10.f,10.6f,10.15f,13.f,o,ceil);
 for(float y:{2.f,3.6f,5.2f})perFix(m,model::bookcase,10.6f,y,o,1.f,.4f,1.9f,kPi*.5f,true);
 // --- The security chief's office (west) -------------------------------------------------------
 officeWall(m,1.f,9.2f,6.5f,9.35f,o,ceil);officeGlass(m,6.5f,1.f,6.65f,7.4f,o,ceil);officeWall(m,6.5f,9.f,6.65f,9.35f,o,ceil);
 perFix(m,model::lab_exec_desk,3.8f,4.f,o,1.85f,.92f,.75f,kPi,true);
 perFix(m,model::lab_boardroom_chair,3.8f,3.f,o,.84f,.79f,.98f,0,true);
 perFix(m,model::bookcase,1.5f,6.f,o,1.f,.4f,1.9f,kPi*.5f,true);
 perFix(m,model::lab_potted_plant,5.8f,1.6f,o,.37f,.43f,.69f,0,false);
 perBody(m,"per_admin_clock",ActorVisual::Worker,{2.6f,7.4f},o,.4f);
 perBlood(m,1.9f,6.8f,3.5f,8.2f,o);
 machineryClock(m,"per_admin_clock",60000);
 wall(m,7.2f,8.65f,8.8f,8.82f,o,o+2.1f,2);
 perSign(m,8.f,8.638f,o+1.45f,"SECURITY CHIEF / OVERRIDE","CHECKPOINT DELTA / SOUTH DOOR",1.45f,kPi,perBlue);
 terminal(m,{8.f,8.f},"SECURITY CHIEF / OVERRIDE","E / AUTHORISE CHECKPOINT DELTA'S SOUTH DOOR.","CHIEF ADEYEMI, 02:33: I WILL NOT SIGN ORDER 7. LET THEM OUT.",o-f,stateId("delta_override"));
 m.terminals.back().yaw=kPi;
 terminal(m,{4.6f,4.9f},"SECURITY CHIEF / ORDER 7","ORDER 7, 02:19: SEAL SUB-LEVELS. PROCESS ALL STAFF. NO WITNESSES.","REPLY 02:34: REFUSED. 02:51: CLEANUP COMMAND ACKNOWLEDGES.",o-f);
 m.terminals.back().yaw=.001f;
 // --- A guard sent back for the override -----------------------------------------------------
 perGuard(m,"per_admin_clock","per_admin",0,ActorVisual::Cleanup,GuardSquadRole::Assault,
  {{{16.f,11.f},o,4000,kPi},{{12.f,11.f},o,3000,kPi*.5f},{{20.f,13.5f},o,3000,0}},12,.55f,1,1.4f);
 m.pickupSpawns.push_back({{21.f,14.6f},PickupKind::Health,o});
 // --- Light ----------------------------------------------------------------------------------
 for(Vec2 l:{Vec2{4.f,4.f},Vec2{14.f,3.f},Vec2{19.f,3.f},Vec2{14.f,9.f},Vec2{19.f,9.f},Vec2{8.f,12.f},Vec2{15.f,14.f}})m.lights.push_back(perSwitch(perTroffer(l,ceil),0,false,l.x<6?0.f:.1f));
 zone(m,"ADMINISTRATIVE LINK / SECURITY CHIEF",1,1,6.6f,9.3f,o-.5f,1000);
 zone(m,"ADMINISTRATIVE LINK / ORE PASS 2",0,16,24,24);
 zone(m,"ADMINISTRATIVE LINK",0,0,24,24);
 freightFinish(m,-1);
 return p;
}

// PERSONNEL TRANSIT (manifest 40). Platform B of the staff tram. Tram 4 was
// held here at 02:40 by Order 7 with its doors locked; they were opened at
// 03:05. Both tunnel mouths are shuttered. The way on is the platform
// walkway west, past the dead lifts, into Restricted Utilities.
std::shared_ptr<const Map> makePersonnelTransit(){
 auto p=std::make_shared<Map>();auto& m=*p;
 m.name=CampaignMapNames[62];m.definition=CampaignChunks[62];m.definition.ambient=.24f;
 const float bed=-20.f,f=-19,roof=m.definition.ceiling;
 m.layers.push_back(floorLayer(m.name.c_str(),bed));
 m.wallSurface=SurfaceConcrete;m.floorSurface=SurfaceConcrete;m.ceilingSurface=SurfaceConcrete;
 openEdge(m,{'N',10.f,14.f,-19.f,true,0});openEdge(m,{'W',14.f,19.f,-19.f,false,0});
 m.definition.playerStart={12.f,2.2f};m.definition.spawnHeight=f;
 // --- Platforms either side of the track bed ------------------------------------------------------
 deck(m,f,1,1,23,8);deck(m,f,1,13,23,23);deck(m,f,0,14,1,19);
 // The north door opens onto the platform: the deck runs under it. The door's
 // z counts from the track bed's tile floor, so it stays a metre up, on the deck.
 deck(m,f,10,0,14,1);
 wall(m,0.f,14.f,1.f,19.f,bed,f-.22f,15);
 for(float y:{7.85f,13.f})wall(m,1.f,y,23.f,y+.15f,f-.02f,f+.01f,18);
 for(float y:{7.4f,13.15f})perFinish(m,1.f,y,23.f,y+.45f,f,13);
 // The crossing at the east end: steps down to the bed and up again.
 m.stairs.push_back({19.f,8.f,21.f,9.4f,bed,f,5,true,false});
 m.stairs.push_back({19.f,11.6f,21.f,13.f,bed,f,5,true,true});
 // --- The track, the tram and the shuttered tunnel mouths ------------------------------------------
 perRailX(m,10.5f,1.f,23.f,bed);
 perFix(m,model::metro_car,10.f,10.5f,bed,2.4f,14.f,3.3f,kPi*.5f,true);
 for(float x:{1.f,22.6f}){wall(m,x,8.f,x+.4f,13.f,bed,bed+.3f,15);wall(m,x,8.2f,x+.4f,12.8f,bed+.3f,bed+3.6f,7);}
 // --- North platform: the dead lifts and the departure board --------------------------------------
 for(float x:{4.f,19.f}){wall(m,x-1.f,1.f,x+1.f,1.3f,f,f+2.4f,7);wall(m,x-1.f,1.f,x+1.f,1.3f,f+2.4f,f+3.5f,15);}
 perHangingSign(m,12.f,5.f,f+2.6f,false,3.2f,"PLATFORM B / TRAM 4","HELD 02:40 / ORDER 7","PLATFORM B","WAY OUT: WEST WALKWAY",perBlue,roof);
 for(float x:{7.f,16.f})perFix(m,model::platform_chair,x,6.4f,f,1.8f,.5f,.8f,kPi,true);
 perFix(m,model::metro_vending,21.6f,3.f,f,1.5f,.68f,2.38f,-kPi*.5f,true);
 // --- South platform: what happened when the doors opened ----------------------------------------
 for(float x:{5.f,8.f,13.5f})perBodyBag(m,x,15.4f,f,false);
 perBody(m,"per_transit_clock",ActorVisual::Worker,{16.6f,14.6f},f,1.6f);
 perBlood(m,15.4f,13.8f,18.2f,15.6f,f);
 for(Vec2 b:{Vec2{10.2f,17.f},Vec2{11.f,16.4f},Vec2{20.2f,18.f}})m.clutterSpawns.push_back({0,b,f,b.x});
 perFix(m,model::ind_crate,21.f,21.4f,f,.8f,.8f,.8f,.3f,true);
 terminal(m,{8.f,21.f},"TRANSIT / PLATFORM B LOG","TRAM 4 HELD 02:40. 31 ABOARD. DOORS LOCKED BY ORDER 7.","03:05 DOORS OPENED BY CLEANUP. 03:20 PLATFORM CLEAR.",f-bed);
 machineryClock(m,"per_transit_clock",60000);
 // --- Something is still on the tram: one of Tram 4's thirty-one -----------------------------------
 // It waits on the track bed behind the car's east end; when the player heads
 // for the crossing it bangs against the car and climbs out after them.
 {const StateId wake=stateId("per_transit_wake");
  m.creatureSpawns.push_back({CreatureKind::Snatcher,{18.6f,10.5f},bed,1,0,wake});
  auto e=perEvent("per_transit_trip",15.f,1.f,23.f,23.f,f-.5f,f+1.5f);e.actions={perSet(wake),perSound(Sound::JunkMetal,.85f)};m.scriptEvents.push_back(e);}
 m.creatureSpawns.push_back({CreatureKind::Huntsman,{6.f,4.f},f});
 m.pickupSpawns.push_back({{2.f,4.f},PickupKind::Ammo,f});
 m.pickupSpawns.push_back({{22.f,16.f},PickupKind::Health,f});
 // --- Light -----------------------------------------------------------------------------------------
 for(float x:{4.f,10.f,16.f,21.f}){m.lights.push_back(perSwitch(lamp({x,4.f},roof-.3f,1.4f,9.f,4300,.3f),0,false,x==10.f?.5f:.05f));
  m.lights.push_back(lamp({x,17.f},roof-.3f,1.2f,9.f,4300,.3f));}
 m.lights.push_back(lamp({12.f,10.5f},roof-.5f,1.f,7.f,3200,0));
 zone(m,"PERSONNEL TRANSIT / TRAM 4",1,8,23,13);
 zone(m,"PERSONNEL TRANSIT / PLATFORM B",0,0,24,24);
 freightFinish(m,-1);
 return p;
}
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

// Walk shapes (World.cpp layShape, WalkShapes.h): a culvert turned 30 degrees, an old-city arch and a concrete
// staircase turned 45, each a model that used to be one solid box. The player crouch-walks the culvert end to end,
// crouches under the arch and climbs the turned stairs, filmed through the player's eyes, the debug layer's collider
// wireframes on at the stairs' top, so the collision is seen to follow the mesh. Pictures: inside the culvert, under the arch, half
// way up the stairs, its collision from the top; checks: each is passed, as the walk-in engine test measures it.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "renderer/DebugLayer.h"
#include "world/ModelIds.h"
#include <cmath>
namespace retro::lab {
namespace {
constexpr float A30=.5235988f,A45=.7853982f;
struct Leg {Vec2 from;float yaw,seconds;bool crouch;Vec2 orbitAt;};
// Along a fixture's depth axis (world (sin yaw, cos yaw)) from its + side toward -.
Leg along(Vec2 c,float yaw,float ahead,float seconds,bool crouch){const Vec2 axis{std::sin(yaw),std::cos(yaw)};return {c+axis*ahead,std::atan2(-axis.y,-axis.x),seconds,crouch,c};}
const Leg legs[]={along({7,8},A30,4.f,7.f,true),along({16,7},0,3.f,4.f,true),along({16,16},A45,3.6f,4.f,false)};
const char* names[]={"crouched through the culvert, turned 30 degrees","crouched under the low arch","climbing a staircase turned 45 degrees"};
int legAt(float t,float& into){for(int i=0;i<3;++i){if(t<legs[i].seconds){into=t;return i;}t-=legs[i].seconds;}into=0;return -1;}
const bool walkShapes=add({
 .name="walk-shapes",.summary="arches, culverts and stairs by their mesh's shape at any turn: crouched through, under and up",
 .fixtures={"walk-shapes"},
 .author=[](Scene&,AuthoredMapData& m){
  m.fixtures.push_back({model::bld_culvert,{7,8},0,2,4.1f,2,A30,true});
  m.fixtures.push_back({model::cave_oldcity_arch,{16,7},0,3,.6f,1.8f,0,true});
  m.fixtures.push_back({model::bld_stairs_concrete,{16,16},0,2,4.08f,3.2f,A45,true});},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);SoftwareRenderer::setThirdPerson(false);debuglayer::visible=0;},
 .step=[](Scene& s,float dt){
  float into=0;const int leg=legAt(float(s.frame)*dt,into);if(leg<0)return;const Leg& L=legs[leg];
  auto& p=LabAccess::player(s.g());
  if(into<dt*.5f){p.pos=L.from;p.z=0;p.angle=L.yaw;p.crouched=false;p.prone=false;}
  s.input.crouch=L.crouch;s.input.forward=into>.6f;
  // Behind them into the culvert and under the arch; beside them up the stairs. Colliders drawn on the stairs only.
  // Through the player's own eyes: only a body inside the culvert sees its walls all round. Colliders drawn at the stairs' top.
  debuglayer::visible=leg==2&&into>L.seconds*.7f?debuglayer::Colliders:0u;
  p.pitch=leg==2?(into>L.seconds*.7f?-35.f:-28.f):leg==1&&into>1.2f?24.f:0.f;   // the stairs underfoot; the arch overhead
  double& peak=s.board["walk.peak."+std::to_string(leg)];peak=std::max(peak,double(p.z));},
 .capture=[](Scene& s,const Frame&){
  float into=0;const float dt=1.f/60;const int leg=legAt(float(s.frame)*dt,into);if(leg<0)return;const Leg& L=legs[leg];
  if(std::fabs(into-L.seconds*(leg==0?.42f:leg==1?.42f:.5f))<dt*.5f)s.shoot(names[leg]);
  if(leg==2&&std::fabs(into-L.seconds*.8f)<dt*.5f)s.shoot("the turned staircase's collision, from its mesh (debuglayer colliders)");
  // How far past the model's middle it got (along its own axis, toward -): out the far end when over half its depth.
  if(std::fabs(into-(L.seconds-dt))<dt*.5f){const auto& p=s.g().player();const Vec2 d=p.pos-L.orbitAt;const Vec2 axis{std::sin(leg==2?A45:leg==0?A30:0.f),std::cos(leg==2?A45:leg==0?A30:0.f)};
   s.board["walk.end."+std::to_string(leg)]=-(d.x*axis.x+d.y*axis.y);}},
 .metrics=[](Scene& s,Report& r){
  r.check("crouched, the player came out of the turned culvert's far end",s.board["walk.end.0"]>2.3);
  r.check("crouched, the player passed under the low arch",s.board["walk.end.1"]>.8);
  r.check("the player climbed the turned staircase without a jump",s.board["walk.peak.2"]>2.6,"to "+std::to_string(s.board["walk.peak.2"])+" m");},
 .teardown=[](Scene& s){SoftwareRenderer::setThirdPerson(false);s.renderer->setBodyOrbit(0,0);debuglayer::visible=0;}
});
}
}

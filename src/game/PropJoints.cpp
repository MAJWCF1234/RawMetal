// Joints between props in play (World.h PropJoint: the JOINT record; Vectra.h Joint). Each step of a list's props
// (RigidClutter.cpp) the level's joints become Vectra joints between the props' bodies, or a prop and the room:
//  - a joint's frames are set from where the map laid its props out (their home pose), so a lamp hangs as placed
//    and a hatch shuts where it was drawn shut, whatever has happened to them since;
//  - both props of a joint are built whenever either is (a sleeping lamp is not left behind by its chain);
//  - what a joint carried is kept for warm starting by its key, per level (m_jointCache);
//  - a joint that snaps (past its breaks=, pulled half a metre apart, its prop broken, or its rope shot
//    through) is marked in the world's PropJointState, sounded, heard, and sets its state=;
//  - the player's weight is on the jointed prop he stands on (a seesaw tips, a lift goes down, a bridge sags);
//  - a lamp riding a hung prop (LIGHT's prop) moves with it (hungLampAt).
// Ropes and chains are drawn from jointLine (Scene3D).
#include "Game.h"
#include "physics/PropBodies.h"
#include "../core/Tune.h"
#include <cmath>
namespace retro {
using namespace vectra;
namespace {
JointKind coreKind(PropJointKind k){switch(k){case PropJointKind::Hinge:return JointKind::Hinge;case PropJointKind::Ball:return JointKind::Ball;case PropJointKind::Slider:return JointKind::Slider;
 case PropJointKind::Weld:return JointKind::Weld;case PropJointKind::Spring:return JointKind::Spring;case PropJointKind::Distance:return JointKind::Distance;case PropJointKind::Pulley:return JointKind::Pulley;default:return JointKind::Rope;}}
constexpr float PlayerWeight=75*14.f;   // what the player weighs on a jointed prop he stands on (N)
// The prop's body as the map laid it out: the joint's rest.
Body homeBody(const Clutter& c,const ClutterSpawn& spawn,Vec2 home){Clutter h=c;h.pos=home;h.z=c.homeZ;h.yaw=spawn.yaw;h.pitch=spawn.pitch;h.roll=spawn.roll;h.orientEuler={1e9f,1e9f,1e9f};h.bend={0,0};h.boneRoot=1;
 Body b;toBody(h,b);return b;}
Body nowBody(const Clutter& c){Body b;toBody(c,b);return b;}
// A point fixed to a prop: where it was on the prop laid out (`at`), where it is now.
V3 carried(const Body& home,const Body& now,V3 at){return now.x+now.q.rotate(home.q.unrotate(at-home.x));}
// The height of a prop's top face over p (a tilted plank's slope), when p is over it (within `radius` of its
// edge) and the face looks up; false otherwise.
bool topAt(const Clutter& c,Vec2 p,float radius,float& top){const Body b=nowBody(c);const auto s=c.size();const V3 n=b.q.rotate({0,0,1});if(n.z<.5f)return false;
 const V3 middle=b.toWorld({0,0,0});const V3 face=middle+n*(s[2]*.5f);const V3 l=b.q.unrotate(V3{p.x,p.y,face.z}-middle);
 if(std::fabs(l.x)>s[0]*.5f+radius||std::fabs(l.y)>s[1]*.5f+radius)return false;
 top=face.z-(n.x*(p.x-face.x)+n.y*(p.y-face.y))/n.z;return true;}
std::uint64_t keyOf(int level,int joint){return (std::uint64_t(std::uint32_t(level+1))<<32)|std::uint32_t(joint+1);}
}
int Game::jointProp(int level,int spawn,int hint)const{
 if(level<0||level>=chunkCount()||spawn<0)return -1;const auto&w=chunkWorld(level);if(spawn>=int(w.clutterSpawns().size()))return -1;
 const auto&s=w.clutterSpawns()[size_t(spawn)];const auto home=s.position+chunkOffset(level);const auto&list=chunkClutter(level);
 // An authored prop keeps its spawn as home wherever it goes (the lamp-on-a-prop rule, as leadProp).
 auto same=[&](const Clutter&c){return c.authored&&c.model==s.model&&lengthSq(c.home-home)<.0004f&&std::fabs(c.homeZ-(s.z==-999?c.homeZ:s.z))<.05f;};
 if(hint>=0&&hint<int(list.size())&&same(list[size_t(hint)]))return hint;
 for(int i=0;i<int(list.size());++i)if(same(list[size_t(i)]))return i;
 return -1;}
std::vector<std::pair<int,int>> Game::jointedProps(int level,const std::vector<Clutter>& list){
 std::vector<std::pair<int,int>> out;auto&w=friendlyWorld(level);const auto&specs=w.joints();auto&states=w.m_jointStates;if(states.size()!=specs.size())states.assign(specs.size(),{});
 for(size_t i=0;i<specs.size();++i){auto&s=states[i];if(s.broken)continue;const auto&j=specs[i];
  const int a=jointProp(level,j.propA,s.hintA),b=j.propB>=0?jointProp(level,j.propB,s.hintB):-1;s.hintA=a;s.hintB=b;
  if(a<0||a>=int(list.size())||(j.propB>=0&&(b<0||b>=int(list.size()))))continue;out.push_back({a,b});}
 return out;}
void Game::addPropJoints(int level,std::vector<Clutter>& list,const std::vector<int>& bodyOf,Space& space,std::deque<Joint>& out,std::vector<int>& which){
 auto&w=friendlyWorld(level);const auto&specs=w.joints();auto&states=w.m_jointStates;if(specs.empty()||states.size()!=specs.size())return;
 const auto&spawns=w.clutterSpawns();const Vec2 origin=chunkOffset(level);
 auto&cache=m_jointCache[level];space.jointCache.swap(cache);
 std::vector<char> jointed(space.bodies.size(),0);
 for(size_t i=0;i<specs.size();++i){auto&s=states[i];if(s.broken)continue;const auto&spec=specs[i];
  const int a=s.hintA,b=spec.propB>=0?s.hintB:-1;
  if(a<0||a>=int(list.size())||spec.propA>=int(spawns.size())||(spec.propB>=0&&(b<0||b>=int(list.size())||spec.propB>=int(spawns.size()))))continue;
  if(list[size_t(a)].broken||(b>=0&&list[size_t(b)].broken)){snapPropJoint(level,int(i),{list[size_t(a)].pos.x,list[size_t(a)].pos.y,list[size_t(a)].z},true);continue;}
  const int ba=a<int(bodyOf.size())?bodyOf[size_t(a)]:-1,bb=b>=0&&b<int(bodyOf.size())?bodyOf[size_t(b)]:-1;if(ba<0||(b>=0&&bb<0))continue;
  const Body ha=homeBody(list[size_t(a)],spawns[size_t(spec.propA)],list[size_t(a)].home-origin);Body hb;if(b>=0)hb=homeBody(list[size_t(b)],spawns[size_t(spec.propB)],list[size_t(b)].home-origin);
  const Body* B=b>=0?&hb:nullptr;
  Joint j;j.kind=coreKind(spec.kind);j.a=ba;j.b=bb;j.key=keyOf(level,int(i));j.breakForce=spec.breakForce;
  const V3 at{spec.at.x,spec.at.y,spec.at.z},to{spec.to.x,spec.to.y,spec.to.z},axis{spec.axis.x,spec.axis.y,spec.axis.z};
  switch(spec.kind){
   case PropJointKind::Hinge:case PropJointKind::Slider:case PropJointKind::Ball:case PropJointKind::Weld:j.attach(ha,B,at,axis);break;
   case PropJointKind::Pulley:{j.attachEnds(ha,B,at,to,true);j.groundA={spec.over.x,spec.over.y,spec.over.z};j.groundB={spec.over2.x,spec.over2.y,spec.over2.z};j.ratio=spec.ratio;
    j.length=spec.length>0?spec.length:len(at-j.groundA)+spec.ratio*len(to-j.groundB);break;}
   default:j.attachEnds(ha,B,at,to,spec.length>0);if(spec.length>0)j.length=spec.length;break;}
  if(spec.kind==PropJointKind::Winch&&s.length>0)j.length=s.length;
  // What a hard blow leaves open (a door's pin, a weld, a slider's rail, a chain's length) is put back by position after
  // the step (Joint::positionCorrection), so a crate thrown into a hung door does not pull it visibly off its pin.
  j.positionCorrection=spec.kind!=PropJointKind::Spring&&spec.kind!=PropJointKind::Pulley;
  j.limited=spec.limited;j.lower=spec.lower;j.upper=spec.upper;
  if(spec.kind==PropJointKind::Winch&&!spec.limited){j.limited=true;j.lower=.2f;j.upper=std::max(j.length,spec.length);}
  if(spec.kind==PropJointKind::Ball){const float d=kPi/180;if(spec.cone>=0){j.coneX=spec.cone*d;j.coneY=(spec.cone2>=0?spec.cone2:spec.cone)*d;}if(spec.twist>=0){j.twistLimited=true;j.twistLower=-spec.twist*d;j.twistUpper=spec.twist*d;}}
  if(spec.kind==PropJointKind::Spring){j.springHertz=spec.hertz>0?spec.hertz:3;j.springDamping=spec.damping;}
  else if(spec.hertz>0&&(spec.kind==PropJointKind::Hinge||spec.kind==PropJointKind::Slider)){j.springHertz=spec.hertz;j.springDamping=spec.damping;j.springTarget=0;}
  // A door's pin, a drawer's runners: friction on the axis, and where it catches (PropJoint friction, detent, catch, latch).
  if(spec.friction>0&&(spec.kind==PropJointKind::Hinge||spec.kind==PropJointKind::Slider||spec.kind==PropJointKind::Ball))j.frictionTorque=spec.friction;
  if(spec.detentCount>0&&(spec.kind==PropJointKind::Hinge||spec.kind==PropJointKind::Slider)){j.detents=spec.detents;j.detentCount=spec.detentCount;
   j.detentRange=spec.catchRange>0?spec.catchRange:spec.kind==PropJointKind::Hinge?8*kPi/180:.025f;j.detentForce=spec.latch;j.detentHertz=tune("joints.detent_hertz",6.f);}
  if(spec.motor&&s.motorOn&&(!spec.motorState||state(spec.motorState))){j.motor=spec.motor==1?JointMotor::Speed:JointMotor::Servo;j.motorSpeed=s.speed;j.motorTarget=s.target;j.motorMaxForce=spec.force>0?spec.force:1000;}
  out.push_back(j);which.push_back(int(i));space.constraints.push_back(&out.back());jointed[size_t(ba)]=1;if(bb>=0)jointed[size_t(bb)]=1;
  // A kinematic body (the prop in the hand) holds no joint: what hangs from it follows, and it is never pulled.
 }
 // The player's weight on the jointed prop he stands on.
 if(level==m_level&&!m_noclip&&!m_inspectionCamera)for(size_t bi=0;bi<space.bodies.size()&&bi<jointed.size();++bi){if(!jointed[bi])continue;auto&body=space.bodies[bi];if(body.user<0||body.kinematic||body.invMass<=0)continue;
  const auto&c=list[size_t(body.user)];float top=0;if(!topAt(c,m_player.pos,.1f,top)||std::fabs(m_player.z-top)>.08f)continue;
  // His weight where he stands, and his mass in the body's inertia (its own weight kept at its middle).
  const float own=1/body.invMass,with=own+PlayerWeight/14.f;body.sleeping=false;body.sleepTime=0;body.invMass=1/with;body.invInertiaLocal=body.invInertiaLocal*(own/with);body.inertiaLocal=body.inertiaLocal*(with/own);body.gravity=body.gravity*(own/with);
  body.applyForce({0,0,-PlayerWeight},{m_player.pos.x,m_player.pos.y,top});}
}
void Game::afterPropJoints(int level,std::vector<Clutter>& list,Space& space,std::deque<Joint>& joints,const std::vector<int>& which){
 auto&w=friendlyWorld(level);auto&states=w.m_jointStates;const auto&specs=w.joints();
 for(size_t k=0;k<joints.size()&&k<which.size();++k){const int i=which[k];if(i<0||size_t(i)>=states.size())continue;auto&s=states[size_t(i)];const auto&j=joints[k];
  s.force=j.force;if(specs[size_t(i)].kind==PropJointKind::Winch)s.length=j.length;
  // Pulled apart (a prop in the hand dragged off its chain, a crate wrenched off its hinge): it gives.
  const float gap=j.kind==JointKind::Rope||j.kind==JointKind::Pulley?j.angle(space)-j.length:j.kind==JointKind::Spring||j.kind==JointKind::Slider?0.f:j.kind==JointKind::Distance?std::fabs(j.angle(space)-j.length):len(j.worldA(space)-j.worldB(space));
  if(j.broken||gap>tune("joints.tear",.5f)){const V3 at=j.worldA(space);snapPropJoint(level,i,{at.x,at.y,at.z});}}
 m_jointCache[level].swap(space.jointCache);(void)list;
}
void Game::snapPropJoint(int level,int joint,Point3 at,bool quiet){
 auto&w=friendlyWorld(level);auto&states=w.m_jointStates;if(joint<0||size_t(joint)>=states.size()||states[size_t(joint)].broken)return;auto&s=states[size_t(joint)];const auto&spec=w.joints()[size_t(joint)];
 s.broken=true;s.brokenAt=m_elapsed;s.force=0;if(spec.snapState)setState(spec.snapState,1);
 // Its props wake to fall.
 auto&list=level==m_level?m_clutter:m_chunks[size_t(level)].clutter;for(int p:{s.hintA,s.hintB})if(p>=0&&p<int(list.size())){list[size_t(p)].sleeping=false;list[size_t(p)].restTime=0;}
 if(quiet)return;
 const auto shift=chunkOffset(level)-chunkOffset(m_level);const bool metal=spec.kind==PropJointKind::Chain||spec.kind==PropJointKind::Hinge||spec.kind==PropJointKind::Slider||spec.kind==PropJointKind::Weld||spec.kind==PropJointKind::Pulley;
 SoundEvent snap{Sound::LiftSnap,{at.x+shift.x,at.y+shift.y},metal?.55f:.4f,metal?1.3f:1.8f,true,17,at.z};snap.aiRadius=6;m_sounds.push_back(snap);}
void Game::cutJointsByShot(Point3 origin,Point3 end){
 const auto active=chunkOffset(m_level);
 for(int level=0;level<chunkCount();++level){if(!chunkResident(level))continue;const auto&w=chunkWorld(level);if(w.joints().empty())continue;const auto shift=chunkOffset(level)-active;
  const Point3 o{origin.x-shift.x,origin.y-shift.y,origin.z},e{end.x-shift.x,end.y-shift.y,end.z};
  for(int i=0;i<int(w.joints().size());++i){const auto&spec=w.joints()[size_t(i)];if(!spec.rope()||spec.kind==PropJointKind::Spring||spec.kind==PropJointKind::Distance)continue;
   std::array<Point3,4> p{};int n=0;if(!jointLine(level,i,p,n))continue;
   const float thick=(spec.radius>0?spec.radius:spec.kind==PropJointKind::Chain?.012f:.008f)+.02f;   // a round is a few centimetres of luck
   for(int k=0;k+1<n;++k){
    // The closest approach of the round's path and this run of rope.
    const Point3 d1=e-o,d2=p[size_t(k+1)]-p[size_t(k)],r=o-p[size_t(k)];const float a=d1.x*d1.x+d1.y*d1.y+d1.z*d1.z,ee=d2.x*d2.x+d2.y*d2.y+d2.z*d2.z,f=d2.x*r.x+d2.y*r.y+d2.z*r.z;
    const float c=d1.x*r.x+d1.y*r.y+d1.z*r.z,b=d1.x*d2.x+d1.y*d2.y+d1.z*d2.z,den=a*ee-b*b;float s=den>1e-9f?std::clamp((b*f-c*ee)/den,0.f,1.f):0.f;float t=ee>1e-9f?(b*s+f)/ee:0.f;
    if(t<0){t=0;s=a>1e-9f?std::clamp(-c/a,0.f,1.f):0.f;}else if(t>1){t=1;s=a>1e-9f?std::clamp((b-c)/a,0.f,1.f):0.f;}
    const Point3 q1=o+d1*s,q2=p[size_t(k)]+d2*t,gap=q1-q2;if(gap.x*gap.x+gap.y*gap.y+gap.z*gap.z<thick*thick){snapPropJoint(level,i,q2);break;}}}}}
float Game::jointedPropTop(Vec2 p,float feet,float radius)const{
 // The player stands on the props joints hold (a lift's platform, a seesaw, a bridge's planks): their top faces
 // under his feet (and within a step of them) are ground.
 float best=-1e9f;const int level=m_level;const auto&w=chunkWorld(level);if(w.joints().empty())return best;const auto&states=w.jointStates();const auto&list=chunkClutter(level);
 for(size_t k=0;k<w.joints().size();++k){if(k<states.size()&&states[k].broken)continue;const auto&j=w.joints()[k];
  for(int pass=0;pass<2;++pass){const int spawn=pass?j.propB:j.propA;if(spawn<0)continue;const int hint=k<states.size()?(pass?states[k].hintB:states[k].hintA):-1;const int i=jointProp(level,spawn,hint);if(i<0)continue;
   const auto&c=list[size_t(i)];if(c.broken||c.mass()<8||(i==m_heldClutter))continue;float top=0;if(topAt(c,p,radius*.5f,top)&&top<=feet+.02f)best=std::max(best,top);}}
 return best;}
bool Game::onJointedTop(int level,int i,Vec2 p,float feet)const{
 const auto&w=chunkWorld(level);if(w.joints().empty())return false;const auto&states=w.jointStates();bool jointed=false;
 for(size_t k=0;k<states.size()&&!jointed;++k)jointed=!states[k].broken&&(states[k].hintA==i||states[k].hintB==i);if(!jointed)return false;
 const auto&c=chunkClutter(level)[size_t(i)];float top=0;const Vec2 local=p-(chunkOffset(level)-chunkOffset(m_level));return topAt(c,local,.25f,top)&&feet>=top-.2f;}   // within a step of its top: he steps onto it
bool Game::jointLine(int level,int joint,std::array<Point3,4>& points,int& count)const{
 count=0;if(level<0||level>=chunkCount()||!chunkResident(level))return false;const auto&w=chunkWorld(level);if(joint<0||size_t(joint)>=w.joints().size())return false;
 const auto&spec=w.joints()[size_t(joint)];if(!spec.rope())return false;const auto&states=w.jointStates();const PropJointState none;const auto&s=size_t(joint)<states.size()?states[size_t(joint)]:none;
 const auto&list=chunkClutter(level);const auto&spawns=w.clutterSpawns();const Vec2 origin=chunkOffset(level);
 const int a=jointProp(level,spec.propA,s.hintA),b=spec.propB>=0?jointProp(level,spec.propB,s.hintB):-1;
 auto end=[&](int prop,int spawn,Point3 at,Point3& out){if(prop<0){out=at;return spawn<0;}const auto&c=list[size_t(prop)];if(c.broken||spawn>=int(spawns.size()))return false;
  const Body home=homeBody(c,spawns[size_t(spawn)],c.home-origin),now=nowBody(c);const V3 p=carried(home,now,{at.x,at.y,at.z});out={p.x,p.y,p.z};return true;};
 if(s.broken){
  // Snapped: what is left hangs from the room's end (a stub of a fifth of it, straight down).
  if(spec.propB>=0||spec.kind==PropJointKind::Pulley)return false;const float l=std::max(.1f,(spec.length>0?spec.length:len(V3{spec.to.x-spec.at.x,spec.to.y-spec.at.y,spec.to.z-spec.at.z}))*.2f);
  points[0]=spec.to;points[1]={spec.to.x,spec.to.y,spec.to.z-l};count=2;return true;}
 Point3 pa,pb;if(!end(a,spec.propA,spec.at,pa)||!end(b,spec.propB,spec.to,pb))return false;
 if(spec.kind==PropJointKind::Pulley){points={pa,spec.over,spec.over2,pb};count=4;return true;}
 points[0]=pb;points[1]=pa;count=2;return true;}   // from the room's end (or B's) down to A's
bool Game::hungLampAt(const WorldLight& lamp,Point3& at,bool* lit)const{
 if(lamp.prop<0||lamp.propLevel<0||lamp.propLevel>=chunkCount()||!chunkResident(lamp.propLevel))return false;const int level=lamp.propLevel;const auto&w=chunkWorld(level);
 bool hung=false;for(const auto&j:w.joints())hung|=j.propA==lamp.prop||j.propB==lamp.prop;if(!hung)return false;
 const int i=jointProp(level,lamp.prop,-1);if(i<0)return false;const auto&c=chunkClutter(level)[size_t(i)];if(lit)*lit=!c.broken;
 const auto&spawns=w.clutterSpawns();if(lamp.prop>=int(spawns.size()))return false;
 const Body home=homeBody(c,spawns[size_t(lamp.prop)],c.home-chunkOffset(level)),now=nowBody(c);const auto e=lamp.emitter();const V3 p=carried(home,now,{e.x,e.y,e.z});at={p.x,p.y,p.z};return true;}
}

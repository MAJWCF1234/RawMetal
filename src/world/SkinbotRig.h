#pragma once
// The skinbot's rig. It is a machine under skin: each leg is three rigid sections joined at bolts, so nothing is
// skinned; a section is moved whole. Given where a blade tip should be, solveSkinbotLeg bends the leg to put it there:
// the leg's plane swings about the vertical through its root, the blade keeps its angle while it can, and the two
// upper sections reach for the blade's top joint with the elbow always bent to the side it is bent at rest.
// Positions are in the model's own space (x right, y up, z forward, metres at scale 1). The idea of stepping legs
// solved from foot targets follows TheCymaera's minecraft-spider (docs/Assets/CREDITS.txt); the code is our own.
#include <algorithm>
#include <array>
#include <cmath>
#include <ostream>
#include <string>

namespace retro::skinbot {
inline constexpr int Model=622;          // the library model (tools/library catalog "skinbot")
#include "SkinbotRigData.inc"
enum Leg {ArmL,ArmR,RearL,RearR,LegCount};
struct V {float x=0,y=0,z=0;};
inline V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V operator*(V a,float k){return {a.x*k,a.y*k,a.z*k};}
inline float length(V a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
inline V joint(int leg,int k){return {SkinbotJoints[leg][k][0],SkinbotJoints[leg][k][1],SkinbotJoints[leg][k][2]};}

// A leg's own plane: forward is the flat direction from its root, along which it is bent.
inline V planeForward(V flat,V fallback){const float l=std::hypot(flat.x,flat.z);return l>1e-4f?V{flat.x/l,0,flat.z/l}:fallback;}
inline V restForward(int leg){return planeForward(joint(leg,3)-joint(leg,0),{0,0,1});}
struct P {float f=0,u=0;};     // in the leg's plane: out along it, and up
inline P inPlane(V point,V root,V forward){const V d=point-root;return {d.x*forward.x+d.z*forward.z,d.y};}
inline float angleOf(P from,P to){return std::atan2(to.u-from.u,to.f-from.f);}
inline float span(P a,P b){return std::hypot(b.f-a.f,b.u-a.u);}

struct Pose {
 std::array<V,4> at{};      // the joints now, root to tip
 V forward{0,0,1};          // the leg's plane now
 std::array<float,3> turn{};// each section's turn in that plane from rest (radians)
 bool reached=true;         // false: the target was past the leg's reach, and the tip is as near as it gets
};
inline Pose restPose(int leg){Pose p;for(int k=0;k<4;++k)p.at[size_t(k)]=joint(leg,k);p.forward=restForward(leg);return p;}

inline Pose solveSkinbotLeg(int leg,V target){
 const V root=joint(leg,0),rest=restForward(leg);
 std::array<P,4> r{};for(int k=0;k<4;++k)r[size_t(k)]=inPlane(joint(leg,k),root,rest);
 const float upper=span(r[0],r[1]),fore=span(r[1],r[2]),blade=span(r[2],r[3]),restBlade=angleOf(r[2],r[3]);
 // Which side of the line from root to wrist the elbow is bent to.
 const float bend=(r[2].f*r[1].u-r[2].u*r[1].f)>0?1.f:-1.f;
 Pose out;out.forward=planeForward(target-root,rest);
 const P goal=inPlane(target,root,out.forward);
 const float farthest=upper+fore-.001f,closest=std::fabs(upper-fore)+.02f;
 auto wristAt=[&](float a){return P{goal.f-blade*std::cos(a),goal.u-blade*std::sin(a)};};
 // The blade keeps its resting angle unless the wrist would then be out of reach; then it turns toward the line
 // that points from the root at the target (stretching out) or away from it (tucking in), only as far as needed.
 float angle=restBlade;P wrist=wristAt(angle);float d=std::hypot(wrist.f,wrist.u);
 if(d>farthest||d<closest){
  const float want=std::atan2(goal.u,goal.f)+(d<closest?3.14159265f:0.f);
  const float swing=std::remainder(want-restBlade,6.2831853f);
  float lo=0,hi=1;
  for(int i=0;i<24;++i){const float mid=(lo+hi)*.5f;const P w=wristAt(restBlade+swing*mid);const float e=std::hypot(w.f,w.u);if(e>farthest||e<closest)lo=mid;else hi=mid;}
  angle=restBlade+swing*hi;wrist=wristAt(angle);d=std::hypot(wrist.f,wrist.u);
 }
 const float reach=std::clamp(d,closest,farthest);out.reached=std::fabs(reach-d)<.002f;
 if(d>1e-5f)wrist={wrist.f*reach/d,wrist.u*reach/d};else wrist={0,-reach};
 const float lean=std::acos(std::clamp((upper*upper+reach*reach-fore*fore)/(2*upper*reach),-1.f,1.f));
 const float a0=std::atan2(wrist.u,wrist.f)+bend*lean;
 const P elbow{upper*std::cos(a0),upper*std::sin(a0)};
 const P tip{wrist.f+blade*std::cos(angle),wrist.u+blade*std::sin(angle)};
 const std::array<P,4> now{P{0,0},elbow,wrist,tip};
 for(int k=0;k<4;++k)out.at[size_t(k)]=root+out.forward*now[size_t(k)].f+V{0,now[size_t(k)].u,0};
 for(int k=0;k<3;++k)out.turn[size_t(k)]=std::remainder(angleOf(now[size_t(k)],now[size_t(k+1)])-angleOf(r[size_t(k)],r[size_t(k+1)]),6.2831853f);
 return out;
}

// Where a point of leg section k, given as modelled, is in a pose.
inline V moveVertex(int leg,int k,const Pose& pose,V point){
 const V rest=restForward(leg),a=joint(leg,k);
 const V d=point-a,side{-rest.z,0,rest.x},nowSide{-pose.forward.z,0,pose.forward.x};
 const float f=d.x*rest.x+d.z*rest.z,u=d.y,n=d.x*side.x+d.z*side.z;
 const float c=std::cos(pose.turn[size_t(k)]),s=std::sin(pose.turn[size_t(k)]);
 return pose.at[size_t(k)]+pose.forward*(f*c-u*s)+V{0,f*s+u*c,0}+nowSide*n;
}

// The material a section is exported with ("skinbot_arm_r_1"): which leg and section, or the body (false).
inline bool sectionOf(const std::string& material,int& leg,int& k){
 static const char* const names[LegCount]={"skinbot_arm_l_","skinbot_arm_r_","skinbot_rear_l_","skinbot_rear_r_"};
 for(int i=0;i<LegCount;++i){const std::string head=names[i];
  if(material.size()==head.size()+1&&material.compare(0,head.size(),head)==0&&material.back()>='0'&&material.back()<='2'){leg=i;k=material.back()-'0';return true;}}
 return false;
}

// The first rig test, shown on the statue: the right front leg lifts, reaches ahead, plants its blade, holds,
// then steps back and plants again. Every other leg stays as modelled.
inline constexpr float ProbeSeconds=4.f;
inline V probeTarget(float seconds){
 const V rest=joint(ArmR,3),ahead=rest+V{.12f,0,.34f};
 const float t=std::fmod(std::max(0.f,seconds),ProbeSeconds);
 auto step=[](V from,V to,float s){const float e=s*s*(3-2*s);return from+(to-from)*e+V{0,.38f*4*s*(1-s),0};};
 if(t<1.f)return rest;
 if(t<1.9f)return step(rest,ahead,(t-1.f)/.9f);
 if(t<2.9f)return ahead;
 if(t<3.7f)return step(ahead,rest,(t-2.9f)/.8f);
 return rest;
}

inline bool testSkinbotRig(std::ostream& report){
 bool all=true;auto check=[&](bool ok,const std::string& what){report<<(ok?"PASS ":"FAIL ")<<what<<'\n';all&=ok;};
 float worstRest=0;
 for(int leg=0;leg<LegCount;++leg){const Pose p=solveSkinbotLeg(leg,joint(leg,3));for(int k=0;k<4;++k)worstRest=std::max(worstRest,length(p.at[size_t(k)]-joint(leg,k)));}
 check(worstRest<.003f,"asked for its resting foot, every leg takes its modelled pose ("+std::to_string(worstRest*1000)+" mm)");
 for(int leg=0;leg<LegCount;++leg){
  float stretch=0,miss=0;int missed=0,flips=0,tried=0;
  const V foot=joint(leg,3),root=joint(leg,0),rest=restForward(leg);
  std::array<P,4> r{};for(int k=0;k<4;++k)r[size_t(k)]=inPlane(joint(leg,k),root,rest);
  const bool restSide=(r[2].f*r[1].u-r[2].u*r[1].f)>0;
  for(int x=-2;x<=2;++x)for(int y=0;y<=3;++y)for(int z=-2;z<=3;++z){
   const V goal=foot+V{x*.1f,y*.12f,z*.11f};const Pose p=solveSkinbotLeg(leg,goal);++tried;
   for(int k=0;k<3;++k)stretch=std::max(stretch,std::fabs(length(p.at[size_t(k+1)]-p.at[size_t(k)])-length(joint(leg,k+1)-joint(leg,k))));
   if(p.reached)miss=std::max(miss,length(p.at[3]-goal));else ++missed;
   const P e=inPlane(p.at[1],root,p.forward),w=inPlane(p.at[2],root,p.forward);
   if(((w.f*e.u-w.u*e.f)>0)!=restSide)++flips;
   // A vertex at a joint stays on it.
   for(int k=0;k<3;++k)stretch=std::max(stretch,length(moveVertex(leg,k,p,joint(leg,k))-p.at[size_t(k)]));
  }
  const std::string name="leg "+std::to_string(leg);
  check(stretch<.004f,name+": no section stretches or comes off its bolt over "+std::to_string(tried)+" targets");
  check(miss<.004f&&missed<tried/4,name+": the blade tip meets each target it can reach (worst "+std::to_string(miss*1000)+" mm; "+std::to_string(missed)+" beyond reach)");
  check(flips==0,name+": the elbow never flips to the other side");
 }
 Pose last=solveSkinbotLeg(ArmR,probeTarget(0));float jump=0,lowest=1e9f,highest=-1e9f;
 for(int i=1;i<=int(ProbeSeconds*60);++i){const Pose p=solveSkinbotLeg(ArmR,probeTarget(i/60.f));for(int k=0;k<4;++k)jump=std::max(jump,length(p.at[size_t(k)]-last.at[size_t(k)]));lowest=std::min(lowest,p.at[3].y);highest=std::max(highest,p.at[3].y);last=p;}
 check(jump<.06f,"the reach-and-plant cycle moves smoothly at 60 frames a second (largest joint step "+std::to_string(jump*1000)+" mm)");
 check(lowest>joint(ArmR,3).y-.01f&&highest>joint(ArmR,3).y+.25f,"the blade lifts clear and never sinks under the floor");
 report<<(all?"PASS":"FAIL")<<'\n';return all;
}
}

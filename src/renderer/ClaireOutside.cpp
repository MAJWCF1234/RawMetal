#include "ClaireOutside.h"
#include <algorithm>
#include <cmath>
#include <string>
namespace retro {
namespace {
float dot(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 unit(Point3 v,Point3 fallback={0,0,1}){const float l=std::sqrt(dot(v,v));return l>1e-6f?v*(1.f/l):fallback;}
}
bool ClaireOutside::prepare(const Mesh& mesh){
 if(m_ready)return true;if(m_failed)return false;m_failed=true;   // one try: a bank without her bones keeps the plain clips
 const size_t count=mesh.creatureVertexCount();if(!count)return false;
 m_clips=int(mesh.creatureClipCount());m_perMetre=std::max(.01f,mesh.maximum.y-mesh.minimum.y)/1.7f;
 // The torso and arms: everything from the chest up, and half the waist, so
 // the hold blends into the legs over the belly instead of tearing at a seam.
 m_upper=mesh.creatureBoneWeights({"Spine3","Neck","Head","Head_end","Shoulder_L","Shoulder_R","Arm_L","Arm_R","ForeArm_L","ForeArm_R","Hand_L","Hand_R",
  "Thumb1_L","Thumb2_L","Thumb3_L","Index1_L","Index2_L","Index3_L","Middle1_L","Middle2_L","Middle3_L","Ring1_L","Ring2_L","Ring3_L","Pinky1_L","Pinky2_L","Pinky3_L",
  "Thumb1_R","Thumb2_R","Thumb3_R","Index1_R","Index2_R","Index3_R","Middle1_R","Middle2_R","Middle3_R","Ring1_R","Ring2_R","Ring3_R","Pinky1_R","Pinky2_R","Pinky3_R"});
 {const auto spine2=mesh.creatureBoneWeights({"Spine2"}),spine1=mesh.creatureBoneWeights({"Spine1"});
  for(size_t i=0;i<count;++i)m_upper[i]=std::clamp(m_upper[i]+.75f*spine2[i]+.3f*spine1[i],0.f,1.f);}
 auto group=[&](std::initializer_list<const char*> bones,float least){Weighted set;const auto w=mesh.creatureBoneWeights(bones);
  for(size_t i=0;i<count;++i)if(w[i]>=least)set.members.push_back({uint32_t(i),w[i]});return set;};
 m_groups[Pelvis]=group({"Hips"},.6f);m_groups[Chest]=group({"Spine2","Spine3"},.6f);m_groups[Head]=group({"Head","Head_end"},.6f);
 m_groups[FootLeft]=group({"Foot_L","Toe_L","Toe_L_end"},.6f);m_groups[FootRight]=group({"Foot_R","Toe_R","Toe_R_end"},.6f);
 for(int side=0;side<2;++side){const char* s=side?"_L":"_R";auto n=[&](const char* b){return std::string(b)+s;};
  const std::string hand=n("Hand"),index=n("Index1"),middle=n("Middle1"),pinky=n("Pinky1"),i3=n("Index3"),m3=n("Middle3"),r3=n("Ring3"),fore=n("ForeArm");
  m_hand[size_t(side)]={group({hand.c_str()},.6f),group({index.c_str()},.45f),group({middle.c_str()},.45f),group({pinky.c_str()},.45f),group({i3.c_str(),m3.c_str(),r3.c_str()},.5f),group({fore.c_str()},.7f)};}
 m_leftUpper=mesh.creatureBoneWeights({"Arm_L"});m_leftFore=mesh.creatureBoneWeights({"ForeArm_L"});
 m_leftHand=mesh.creatureBoneWeights({"Hand_L","Thumb1_L","Thumb2_L","Thumb3_L","Index1_L","Index2_L","Index3_L","Middle1_L","Middle2_L","Middle3_L","Ring1_L","Ring2_L","Ring3_L","Pinky1_L","Pinky2_L","Pinky3_L"});
 {const auto shoulder=mesh.creatureBoneWeights({"Shoulder_L"}),hand=mesh.creatureBoneWeights({"Hand_L"});
  auto ring=[&](const std::vector<float>&a,const std::vector<float>&b){Weighted set;for(float least:{.2f,.1f,.04f}){set.members.clear();
   for(size_t i=0;i<count;++i){const float w=std::min(a[i],b[i]);if(w>=least)set.members.push_back({uint32_t(i),w});}if(set.members.size()>=3)break;}return set;};
  m_leftJoints={ring(shoulder,m_leftUpper),ring(m_leftUpper,m_leftFore),ring(m_leftFore,hand)};}
 for(const auto&g:m_leftJoints)if(g.members.empty())return false;
 for(const auto&g:m_groups)if(g.members.empty())return false;
 for(const auto&h:m_hand)for(const auto&g:h)if(g.members.empty())return false;
 m_ready=true;m_failed=false;return true;
}
Point3 ClaireOutside::mean(const Weighted& set,const std::vector<Point3>& from)const{
 Point3 sum{};float total=0;for(auto [index,weight]:set.members)if(index<from.size()){sum=sum+from[index]*weight;total+=weight;}
 return total>0?sum*(1.f/total):Point3{};
}
void ClaireOutside::compose(const Mesh& mesh,int lowerClip,float lowerPhase,const Layer& layer,float pitch,float yaw){
 mesh.sampleCreatureVertices(lowerClip,lowerPhase,m_lower);
 m_pose=m_lower;const size_t count=std::min(m_pose.size(),m_upper.size());
 if(layer.clip>=0&&layer.weight>0){
  mesh.sampleCreatureVertices(layer.clip,layer.phase,m_layer);
  if(layer.mixClip>=0&&layer.mix>0){mesh.sampleCreatureVertices(layer.mixClip,layer.mixPhase,m_mix);for(size_t i=0;i<count;++i)m_layer[i]=m_layer[i]+(m_mix[i]-m_layer[i])*layer.mix;}
  // The hold's torso sits on the legs' pelvis.
  const Point3 shift=mean(m_groups[Pelvis],m_lower)-mean(m_groups[Pelvis],m_layer);
  for(size_t i=0;i<count;++i){const float u=m_upper[i]*layer.weight;if(u>0)m_pose[i]=m_pose[i]+(m_layer[i]+shift-m_pose[i])*u;}
 }
 if(pitch!=0||yaw!=0){
  // Each vertex turns as far as it belongs to the torso, so the belly bends
  // and the hips and legs stay where the clip puts them.
  const Point3 pivot=mean(m_groups[Chest],m_pose);
  for(size_t i=0;i<count;++i){const float u=m_upper[i];if(u<=0)continue;Point3 d=m_pose[i]-pivot;
   const float p=pitch*u,cp=std::cos(p),sp=std::sin(p);d={d.x,d.y*cp+d.z*sp,d.z*cp-d.y*sp};
   const float t=yaw*u,cy=std::cos(t),sy=std::sin(t);d={d.x*cy+d.z*sy,d.y,d.z*cy-d.x*sy};
   m_pose[i]=pivot+d;}
 }
}
void ClaireOutside::turnBlendStart(float yaw,Point3 centre){
 if(!m_hasFrom||yaw==0)return;const float c=std::cos(yaw),s=std::sin(yaw);
 for(auto&p:m_from){const float x=p.x-centre.x,z=p.z-centre.z;p.x=centre.x+z*s+x*c;p.z=centre.z+z*c-x*s;}
}
namespace {
// The rotation taking unit a onto unit b by the shortest arc, as a function.
struct Arc{Point3 axis{};float c=1,s=0;
 Arc(Point3 a,Point3 b){a=unit(a);b=unit(b);const Point3 n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};s=std::sqrt(dot(n,n));c=dot(a,b);axis=s>1e-6f?n*(1.f/s):Point3{1,0,0};}
 Point3 operator()(Point3 v)const{const Point3 k=axis;const Point3 kv{k.y*v.z-k.z*v.y,k.z*v.x-k.x*v.z,k.x*v.y-k.y*v.x};return v*c+kv*s+k*(dot(k,v)*(1-c));}
};
}
float ClaireOutside::reachLeft(Mesh& mesh,Point3 target){
 if(m_out.empty())return 0;
 for(int pass=0;pass<3;++pass){
  const Point3 S=mean(m_leftJoints[0],m_out),E=mean(m_leftJoints[1],m_out),W=mean(m_leftJoints[2],m_out),grip=hand(1).grip;
  const Point3 miss=target-grip;if(dot(miss,miss)<1e-6f)break;
  // The wrist goes where the grip must, and the elbow bends in the plane it
  // already bends in, the upper arm and forearm keeping their lengths.
  const float upper=std::sqrt(dot(E-S,E-S)),fore=std::sqrt(dot(W-E,W-E));
  Point3 wrist=W+miss,reach=wrist-S;float d=std::sqrt(dot(reach,reach));const float most=(upper+fore)*.995f;
  if(d>most){reach=reach*(most/d);wrist=S+reach;d=most;}
  const Point3 along=unit(reach);Point3 pole=(E-S)-along*dot(E-S,along);pole=unit(pole,unit(Point3{0,-1,0}-along*dot(Point3{0,-1,0},along)));
  const float x=(upper*upper-fore*fore+d*d)/(2*d),h=std::sqrt(std::max(0.f,upper*upper-x*x));
  const Point3 elbow=S+along*x+pole*h;
  const Arc turnUpper(E-S,elbow-S),turnFore(W-E,wrist-elbow);
  for(size_t i=0;i<m_out.size()&&i<m_leftUpper.size();++i){const float a=m_leftUpper[i],b=m_leftFore[i],c=m_leftHand[i];if(a+b+c<=0)continue;
   const Point3 p=m_out[i];const Point3 up=S+turnUpper(p-S),lo=elbow+turnFore(p-E);
   m_out[i]=p*(1-std::min(1.f,a+b+c))+up*a+lo*(b+c);}
 }
 mesh.publishCreatureVertices(m_out);
 const Point3 left=target-hand(1).grip;return std::sqrt(dot(left,left));
}
void ClaireOutside::publish(Mesh& mesh,float amount){
 m_out=m_pose;
 if(m_hasFrom&&amount<1&&m_from.size()==m_out.size())for(size_t i=0;i<m_out.size();++i)m_out[i]=m_from[i]+(m_out[i]-m_from[i])*std::clamp(amount,0.f,1.f);
 else if(amount>=1)m_hasFrom=false;
 mesh.publishCreatureVertices(m_out);
}
ClaireOutside::Hand ClaireOutside::hand(int side,bool published)const{
 const auto&from=published?m_out:m_pose;const auto&g=m_hand[size_t(side&1)];
 Hand h;h.palm=mean(g[0],from);const Point3 index=mean(g[1],from),middle=mean(g[2],from),pinky=mean(g[3],from),tips=mean(g[4],from);h.forearm=mean(g[5],from);
 h.forward=unit(middle-h.palm);h.across=unit(index-pinky);
 // The fingers close toward the palm side: the fingertips lie off the palm
 // that way, whatever the hand's roll.
 Point3 n=tips-h.palm;n=n-h.forward*dot(n,h.forward);h.normal=unit(n,unit(Point3{h.across.y*h.forward.z-h.across.z*h.forward.y,h.across.z*h.forward.x-h.across.x*h.forward.z,h.across.x*h.forward.y-h.across.y*h.forward.x}));
 // A grip runs through the closed fist: a little past the palm toward the
 // knuckles, and out from it by the grip's half thickness.
 h.grip=h.palm+(middle-h.palm)*.55f+h.normal*(.018f*m_perMetre);
 return h;
}
namespace {
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
// The rotation taking the model frame (m1, m2 made square to it) onto the
// world frame (w1, w2 likewise), as the images of the model's axes.
void turn(Point3 m1,Point3 m2,Point3 w1,Point3 w2,ClaireOutside::Gun& gun){
 m1=unit(m1);m2=unit(m2-m1*dot(m2,m1));const Point3 m3=cross(m1,m2);
 w1=unit(w1);w2=unit(w2-w1*dot(w2,w1),unit(cross(w1,Point3{1,0,0})));const Point3 w3=cross(w1,w2);
 gun.x=w1*m1.x+w2*m2.x+w3*m3.x;gun.y=w1*m1.y+w2*m2.y+w3*m3.y;gun.z=w1*m1.z+w2*m2.z+w3*m3.z;}
}
ClaireOutside::Gun ClaireOutside::fitLongGun(const Hand& right,const Hand& left,Point3 trigger,Point3 support,Point3 forward,float scale){
 Gun gun;gun.scale=scale;gun.origin=right.grip;
 turn(support-trigger,Point3{0,1,0},left.grip-right.grip,right.across+Point3{0,1,0},gun);
 gun.bore=unit(gun.x*forward.x+gun.y*forward.y+gun.z*forward.z);return gun;
}
ClaireOutside::Gun ClaireOutside::fitPistol(const Hand& right,Point3 grip,float scale){
 Gun gun;gun.scale=scale;gun.origin=right.grip;(void)grip;
 turn(Point3{0,0,1},Point3{0,1,0},right.palm-right.forearm,right.across,gun);
 gun.bore=unit(gun.z);return gun;
}
namespace {
// A hand's own frame: the fingers' way, the palm's normal square to it, and
// the third axis across the knuckles.
std::array<Point3,3> handFrame(const ClaireOutside::Hand& h){const Point3 f=unit(h.forward),n=unit(h.normal-f*dot(h.normal,f));return {f,n,cross(f,n)};}
}
void ClaireOutside::rememberInRightHand(const Hand& right,const Gun& gun){
 const auto b=handFrame(right);auto local=[&](Point3 v){return Point3{dot(v,b[0]),dot(v,b[1]),dot(v,b[2])};};
 m_gunInHand={local(gun.origin-right.grip),local(gun.x),local(gun.y),local(gun.z),local(gun.bore)};m_gunScale=gun.scale;m_gunRemembered=true;
}
bool ClaireOutside::gunFromRightHand(const Hand& right,Gun& gun)const{
 if(!m_gunRemembered)return false;const auto b=handFrame(right);auto world=[&](Point3 v){return b[0]*v.x+b[1]*v.y+b[2]*v.z;};
 gun.origin=right.grip+world(m_gunInHand[0]);gun.x=world(m_gunInHand[1]);gun.y=world(m_gunInHand[2]);gun.z=world(m_gunInHand[3]);gun.bore=world(m_gunInHand[4]);gun.scale=m_gunScale;return true;
}
Point3 ClaireOutside::centroid(Group group,bool published)const{return mean(m_groups[size_t(group)],published?m_out:m_pose);}
float ClaireOutside::lowestPublished()const{float low=1e9f;for(const auto&p:m_out)low=std::min(low,p.y);return low;}
}

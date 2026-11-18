#include "Containers.h"
#include "../world/ModelIds.h"
#include "physics/PropBodies.h"
#include <cmath>
namespace retro::containers {
namespace {
std::array<Point3,3> basis(const Clutter& c){auto a=c.rotate(1,0,0),b=c.rotate(0,1,0),z=c.rotate(0,0,1);return {Point3{a[0],a[1],a[2]},Point3{b[0],b[1],b[2]},Point3{z[0],z[1],z[2]}};}
Point3 centre(const Clutter& c){return {c.pos.x,c.pos.y,c.z+c.height()*.5f};}
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
// The item's horizontal half-widths along the box's own x and y.
Vec2 halfIn(const Clutter& box,const Clutter& item){const auto s=item.size();const float d=item.yaw-box.yaw,co=std::fabs(std::cos(d)),si=std::fabs(std::sin(d));
 return {co*s[0]*.5f+si*s[1]*.5f,si*s[0]*.5f+co*s[1]*.5f};}
Vec2 toLocal(const Clutter& box,Vec2 p){const Vec2 d=p-box.pos;const float co=std::cos(box.yaw),si=std::sin(box.yaw);return {d.x*co+d.y*si,-d.x*si+d.y*co};}
Vec2 toWorld(const Clutter& box,Vec2 l){const float co=std::cos(box.yaw),si=std::sin(box.yaw);return box.pos+Vec2{l.x*co-l.y*si,l.x*si+l.y*co};}
}
// How high an item's underside is: its box's lowest corner, or on Vectra its shape's own lowest
// point (a pumpkin or a bottle turned on its side has its box's corner well under its round).
float underside(const Clutter& c){if(c.model<0||vectra::legacyProps())return c.z;vectra::Body b;vectra::toBody(c,b);float z=1e9f;for(int p=0;p<b.shape->count;++p)z=std::min(z,vectra::support(b,p,{0,0,-1}).z);return z;}
bool isContainer(const Clutter& c){if(c.traits&&c.traits->volume>=0)return c.model>=0&&c.traits->volume>0;return c.model>=0&&c.model<int(std::size(model::container))&&model::container[c.model][1]>0;}
Inner inner(const Clutter& c){
 const auto s=c.size();Inner in;
 const bool modelled=c.model>=0&&c.model<int(std::size(model::container))&&model::container[c.model][1]>0;
 if(modelled){const auto& k=model::container[c.model];const auto& own=model::propSize[c.model];
  auto scale=[&](int a){return own[a]>0?s[size_t(a)]/own[a]:1.f;};
  in.floor=k[0]*scale(2);in.rim=k[1]*scale(2);in.halfX=std::max(.01f,s[0]*.5f-k[2]*scale(0));in.halfY=std::max(.01f,s[1]*.5f-k[2]*scale(1));}
 else{in.floor=.02f;in.rim=s[2];in.halfX=std::max(.01f,s[0]*.5f-.02f);in.halfY=std::max(.01f,s[1]*.5f-.02f);}
 // Map format 2 volume= (litres): the rim stands where the opening holds that much.
 if(c.traits&&c.traits->volume>0)in.rim=std::clamp(in.floor+c.traits->volume*.001f/(4*in.halfX*in.halfY),in.floor+.02f,std::max(in.floor+.02f,s[2]));
 in.spill=std::clamp(std::atan2(in.rim-in.floor,2*std::min(in.halfX,in.halfY)),.21f,.87f);   // 12 to 50 degrees
 return in;
}
bool upright(const Clutter& c){return c.rotate(0,0,1)[2]>=std::cos(inner(c).spill);}
bool overInner(const Clutter& box,Vec2 p){const auto in=inner(box);const Vec2 l=toLocal(box,p);return std::fabs(l.x)<=in.halfX&&std::fabs(l.y)<=in.halfY;}
bool holds(const Clutter& box,const Clutter& item,bool ignoreBroken){
 if(&box==&item||!isContainer(box)||(box.broken&&!ignoreBroken)||item.model<0||item.broken||!upright(box))return false;
 const auto in=inner(box);if(!overInner(box,item.pos))return false;
 const auto s=item.size();if(std::min(s[0],s[1])*.5f>std::min(in.halfX,in.halfY)+.01f||std::max(s[0],s[1])*.5f>std::max(in.halfX,in.halfY)+.01f)return false;
 const float under=underside(item);return under>=box.z+in.floor-.04f&&under<=box.z+in.rim+.02f;
}
float supportAt(const Clutter& v,Vec2 p,float feet){
 // Over the opening: the inner floor. Over the walls: the rim (not a basket's handle or a box's flaps above it).
 if(isContainer(v)&&!v.broken&&upright(v)){const auto in=inner(v);const float s=v.z+(overInner(v,p)?in.floor:std::min(in.rim,v.supportHeight()));return s<=feet+.025f?s:-100.f;}
 const float top=v.z+v.supportHeight();return top<=feet+.025f?top:-100.f;
}
Snapshot capture(std::vector<Clutter>& list,int held,bool onlyHeld){
 Snapshot s;std::vector<int> boxes;std::vector<char> rode(list.size());
 // (onlyHeld: a container still on the move after a shove keeps carrying what rode in it till it stops.)
 auto carrying=[&](int i){const auto&c=list[size_t(i)];if(i==held||c.blockedTime>0)return true;if(lengthSq(c.velocity)+c.vz*c.vz<.04f)return false;
  for(const auto&v:list)if(v.riding&&holds(c,v))return true;return false;};
 std::vector<char> carries(list.size(),0);if(onlyHeld)for(int i=0;i<int(list.size());++i)if(isContainer(list[size_t(i)]))carries[size_t(i)]=carrying(i);
 for(int i=0;i<int(list.size());++i){rode[size_t(i)]=list[size_t(i)].riding;list[size_t(i)].riding=false;if(isContainer(list[size_t(i)])&&!list[size_t(i)].broken&&upright(list[size_t(i)])&&(!onlyHeld||carries[size_t(i)]))boxes.push_back(i);}
 if(boxes.empty())return s;
 std::vector<int> seen;
 for(int j=0;j<int(list.size());++j){if(j==held)continue;const auto& item=list[size_t(j)];if(item.model<0||item.broken)continue;
  int best=-1;float area=1e9f;
  for(int k:boxes){if(k==j||!holds(list[size_t(k)],item))continue;const auto in=inner(list[size_t(k)]);if(in.halfX*in.halfY<area){area=in.halfX*in.halfY;best=k;}}
  if(best<0)continue;const auto& box=list[size_t(best)];
  // A tray's lip holds nothing against a yank: jerked out from under it (faster than a push starts it, 1 m/s),
  // it stays behind, and does not catch on again until the tray has slowed to it.
  if(best!=held&&!rode[size_t(j)]){const auto in=inner(box);if(in.rim-in.floor<.05f&&length(box.velocity-item.velocity)>1.f){list[size_t(j)].sleeping=false;list[size_t(j)].restTime=0;continue;}}
  const auto axes=basis(box);const Point3 d=Point3{item.pos.x,item.pos.y,item.z}-centre(box);
  s.riders.push_back({best,j,{dot3(d,axes[0]),dot3(d,axes[1]),dot3(d,axes[2])},wrapAngle(item.yaw-box.yaw)});
  if(std::find(seen.begin(),seen.end(),best)==seen.end()){seen.push_back(best);s.boxes.push_back({best,box});}
 }
 return s;
}
void carry(std::vector<Clutter>& list,const Snapshot& snapshot,int held){
 if(snapshot.riders.empty())return;
 // A box carried in another box goes after it, so nested contents follow the box's new place.
 auto parent=[&](int box){for(const auto& r:snapshot.riders)if(r.item==box)return r.box;return -1;};
 auto depth=[&](int box){int d=0;for(int p=parent(box);p>=0&&d<8;p=parent(p))++d;return d;};
 auto order=snapshot.boxes;std::stable_sort(order.begin(),order.end(),[&](const auto& a,const auto& b){return depth(a.first)<depth(b.first);});
 for(const auto& [k,before]:order){if(size_t(k)>=list.size())continue;const auto& box=list[size_t(k)];
  auto each=[&](auto&& f){for(const auto& r:snapshot.riders)if(r.box==k&&size_t(r.item)<list.size())f(r,list[size_t(r.item)]);};
  if(box.broken){each([](const Rider&,Clutter& v){v.riding=false;v.sleeping=false;v.restTime=0;});continue;}
  if(!upright(box)){
   // Past the rim: what was in it pours out the open side, downhill.
   const auto up=box.rotate(0,0,1);Vec2 dir{up[0],up[1]};dir=lengthSq(dir)>1e-6f?normalized(dir):lengthSq(box.velocity)>1e-6f?normalized(box.velocity):Vec2{1,0};
   each([&](const Rider&,Clutter& v){v.riding=false;v.sleeping=false;v.restTime=0;v.projectile=false;v.velocity=box.velocity+dir*1.2f;v.vz=std::max(box.vz,.3f);});continue;}
  const bool moved=k==held||lengthSq(box.pos-before.pos)>1e-10f||std::fabs(box.z-before.z)>1e-5f||std::fabs(wrapAngle(box.yaw-before.yaw))>1e-5f||
   std::fabs(wrapAngle(box.pitch-before.pitch))>1e-5f||std::fabs(wrapAngle(box.roll-before.roll))>1e-5f;
  if(moved){const auto axes=basis(box);const Point3 c=centre(box);
   each([&](const Rider& r,Clutter& v){const Point3 p=c+axes[0]*r.local.x+axes[1]*r.local.y+axes[2]*r.local.z;
    v.pos={p.x,p.y};v.z=p.z;v.yaw=wrapAngle(box.yaw+r.yaw);v.velocity=box.velocity;v.vz=box.vz;v.spin=v.pitchSpeed=v.rollSpeed=0;
    v.riding=true;v.sleeping=false;v.restTime=0;v.projectile=false;});
   continue;}
  // At rest: the walls hold what moves in it.
  const auto in=inner(box);
  each([&](const Rider&,Clutter& v){if(v.sleeping)return;const Vec2 half=halfIn(box,v);Vec2 l=toLocal(box,v.pos);
   const float co=std::cos(box.yaw),si=std::sin(box.yaw);Vec2 lv{v.velocity.x*co+v.velocity.y*si,-v.velocity.x*si+v.velocity.y*co};bool hit=false;
   const float limX=std::max(0.f,in.halfX-half.x),limY=std::max(0.f,in.halfY-half.y);
   if(std::fabs(l.x)>limX){l.x=std::copysign(limX,l.x);if(lv.x*l.x>0)lv.x*=-.3f;hit=true;}
   if(std::fabs(l.y)>limY){l.y=std::copysign(limY,l.y);if(lv.y*l.y>0)lv.y*=-.3f;hit=true;}
   if(hit){v.pos=toWorld(box,l);v.velocity={lv.x*co-lv.y*si,lv.x*si+lv.y*co};}});
 }
}
void release(std::vector<Clutter>& list,size_t box){
 if(box>=list.size()||!isContainer(list[box]))return;const auto& b=list[box];
 for(auto& v:list)if(holds(b,v,true)){const Vec2 out=v.pos-b.pos;v.riding=false;v.sleeping=false;v.restTime=0;v.velocity+=lengthSq(out)>1e-6f?normalized(out)*.4f:Vec2{};v.vz=std::max(v.vz,.3f);}
}
void settleAuthored(std::vector<Clutter>& list){
 for(const auto& box:list){if(!isContainer(box)||box.broken||!upright(box))continue;const auto in=inner(box);
  for(auto& v:list){if(&v==&box||v.model<0||v.broken||!v.authored||!overInner(box,v.pos))continue;
   const auto s=v.size();if(std::min(s[0],s[1])*.5f>std::min(in.halfX,in.halfY)+.01f||std::max(s[0],s[1])*.5f>std::max(in.halfX,in.halfY)+.01f)continue;
   if(v.z>=box.z-.05f&&v.z<box.z+in.floor-.02f){v.z=box.z+in.floor;v.sleeping=true;v.velocity={};v.vz=0;}}}
}
}

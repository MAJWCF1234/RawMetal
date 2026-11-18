#pragma once
// Shapes as convex solids, and the faces buried inside them.
//
// Every structure the renderer draws as a solid (a box, a PRIM's curved or sloped
// shape) is also one or more convex pieces. When the map's geometry is built, each
// face of a structure is clipped against the pieces of the others it reaches into
// (Sutherland-Hodgman, one plane at a time): only the parts outside them are drawn.
// A face lying on a piece's face looking the other way (two solids touching) is
// buried; one lying on it looking the same way (two solids flush) is drawn once,
// by the later record. Each structure keeps its own material, so a building made of
// overlapping prims in several materials reads as one piece with clean seams.
// Collision is untouched: this is drawing only.
//
// The pieces of the shapes mirror Scene3D's drawShape (the old shapes) and the
// faces here (the new PRIM shapes), so a piece is the drawn solid itself.
// Header-only and free of the renderer: the lab's culling tube tests it directly.
#include "../world/Primitives.h"
#include "BoxSurfaces.h"
#include <memory>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace retro::cull {
inline Point3 crossP(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float dotP(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float lengthP(Point3 a){return std::sqrt(dotP(a,a));}

// ---- a structure's own frame: u across, v along its facing, from the middle of its box ----
struct ShapeFrame {
 float cx=0,cy=0,hw=0,hd=0,z0=0,z1=0,h=0;Vec2 forward{0,1},side{1,0};
 Point3 P(float u,float v,float z)const{return {cx+side.x*u+forward.x*v,cy+side.y*u+forward.y*v,z};}
};
inline ShapeFrame frameOf(const Structure& s){
 ShapeFrame f;const int q=s.facing&3;f.forward=q==0?Vec2{0,1}:q==1?Vec2{1,0}:q==2?Vec2{0,-1}:Vec2{-1,0};f.side={f.forward.y,-f.forward.x};
 f.cx=(s.x1+s.x2)*.5f;f.cy=(s.y1+s.y2)*.5f;f.hw=(q%2?s.y2-s.y1:s.x2-s.x1)*.5f;f.hd=(q%2?s.x2-s.x1:s.y2-s.y1)*.5f;f.z0=s.bottom;f.z1=s.top;f.h=s.top-s.bottom;return f;}

// ---- drawn faces of the new PRIM shapes ----
// A face is a convex polygon wound so its normal points out of the solid. A face with
// `mapped` carries its own texture coordinates in metres (round surfaces, unrolled);
// otherwise the drawer projects it onto its plane.
struct FaceVertex {Point3 p;float u=0,v=0;};
struct Face {std::vector<FaceVertex> v;bool mapped=false;};
// A convex piece of a shape: its corners (the solid is their hull) and the faces of it
// that are drawn (none for the old shapes, which Scene3D draws itself).
struct ShapePiece {std::vector<Point3> points;std::vector<Face> faces;};

inline bool newShape(int shape){return shape>=ShapeWedge&&shape<=ShapeArcWall;}

namespace detail {
inline Point3 centroid(const std::vector<Point3>& p){Point3 c{};for(auto q:p)c=c+q;return c*(1.f/std::max<size_t>(1,p.size()));}
inline Point3 newell(const std::vector<FaceVertex>& v){Point3 n{};for(size_t i=0;i<v.size();++i){const auto a=v[i].p,b=v[(i+1)%v.size()].p;n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}return n;}
// Turns each face of the piece to look out of it.
inline void orient(ShapePiece& piece){const Point3 c=centroid(piece.points);
 for(auto& f:piece.faces){if(f.v.size()<3)continue;Point3 mid{};for(auto& q:f.v)mid=mid+q.p;mid=mid*(1.f/f.v.size());
  if(dotP(newell(f.v),mid-c)<0)std::reverse(f.v.begin(),f.v.end());}}
inline Face plain(std::initializer_list<Point3> pts){Face f;for(auto p:pts)f.v.push_back({p});return f;}
}

// The convex pieces of a shaped structure; with faces, the faces of the new shapes to draw.
inline void shapePieces(const Structure& s,std::vector<ShapePiece>& out,bool faces=true){
 using detail::plain;const auto F=frameOf(s);const float hw=F.hw,hd=F.hd,z0=F.z0,z1=F.z1,h=F.h;
 auto P=[&](float u,float v,float z){return F.P(u,v,z);};
 auto piece=[&](std::vector<Point3> pts)->ShapePiece&{out.push_back({std::move(pts),{}});return out.back();};
 auto finish=[&](ShapePiece& p){if(faces)detail::orient(p);};
 constexpr float pi=3.14159265f;
 switch(s.shape){
 // ---- the old shapes: pieces only, as Scene3D draws them ----
 case ShapeRamp:piece({P(-hw,-hd,z0),P(hw,-hd,z0),P(-hw,hd,z0),P(hw,hd,z0),P(-hw,hd,z1),P(hw,hd,z1)});break;
 case ShapeColumn:{const float r=std::min(hw,hd);std::vector<Point3> pts;
  for(int i=0;i<10;++i){const float a=i*kTwoPi/10;for(float z:{z0,z1})pts.push_back({F.cx+r*std::cos(a),F.cy+r*std::sin(a),z});}piece(pts);break;}
 case ShapeDome:{constexpr int rings=6,sectors=16;
  auto at=[&](int ring,int sector){const float e=pi*.5f*ring/rings,a=kTwoPi*sector/sectors;return P(hw*std::cos(e)*std::cos(a),hd*std::cos(e)*std::sin(a),z0+h*std::sin(e));};
  for(int k=0;k<sectors;++k){std::vector<Point3> pts{P(0,0,z0)};for(int j=0;j<=rings;++j){pts.push_back(at(j,k));pts.push_back(at(j,k+1));}piece(pts);}break;}
 case ShapeStrut:{const float t=strutSection(hw*2,h);std::vector<Point3> pts;
  for(float u:{-hw,hw})for(auto q:{Point3{0,-hd,z0},Point3{0,-hd,z0+t},Point3{0,hd,z1-t},Point3{0,hd,z1}})pts.push_back(P(u,q.y,q.z));piece(pts);break;}
 case ShapeArch:case ShapeRoundel:case ShapeVault:{
  constexpr int segments=12;
  auto prism=[&](std::array<std::pair<float,float>,4> uz){std::vector<Point3> pts;for(float v:{-hd,hd})for(auto [u,z]:uz)pts.push_back(P(u,v,z));piece(pts);};
  auto block=[&](float u0,float u1){prism({std::pair{u0,z0},{u1,z0},{u1,z1},{u0,z1}});};
  if(s.shape==ShapeArch){const auto arch=archOpening(hw*2,h);block(-hw,-arch.half);block(arch.half,hw);
   for(int k=0;k<segments;++k){const float a0=pi*(1-float(k)/segments),a1=pi*(1-float(k+1)/segments);
    const float u0=arch.half*std::cos(a0),u1=arch.half*std::cos(a1),i0=z0+arch.spring+arch.half*std::sin(a0),i1=z0+arch.spring+arch.half*std::sin(a1);
    prism({std::pair{u0,i0},{u1,i1},{u1,z1},{u0,z1}});}}
  else if(s.shape==ShapeRoundel){const auto ring=roundelOpening(hw*2,h);const float r=ring.radius,c=z0+ring.centre;block(-hw,-r);block(r,hw);
   for(int k=0;k<segments;++k){const float a0=pi*(1-float(k)/segments),a1=pi*(1-float(k+1)/segments);
    const float u0=r*std::cos(a0),u1=r*std::cos(a1);
    prism({std::pair{u0,c+r*std::sin(a0)},{u1,c+r*std::sin(a1)},{u1,z1},{u0,z1}});prism({std::pair{u0,z0},{u1,z0},{u1,c-r*std::sin(a1)},{u0,c-r*std::sin(a0)}});}}
  else{const float iw=hw-VaultShell,ih=h-VaultShell;
   for(int k=0;k<segments;++k){const float a0=pi*(1-float(k)/segments),a1=pi*(1-float(k+1)/segments);
    prism({std::pair{hw*std::cos(a0),z0+h*std::sin(a0)},{hw*std::cos(a1),z0+h*std::sin(a1)},{iw*std::cos(a1),z0+ih*std::sin(a1)},{iw*std::cos(a0),z0+ih*std::sin(a0)}});}}
  break;}
 // ---- the new shapes: pieces with their faces ----
 case ShapeWedge:{auto& p=piece({P(-hw,-hd,z0),P(hw,-hd,z0),P(0,-hd,z1),P(-hw,hd,z0),P(hw,hd,z0),P(0,hd,z1)});
  if(faces){p.faces={plain({P(hw,-hd,z0),P(hw,hd,z0),P(0,hd,z1),P(0,-hd,z1)}),plain({P(-hw,hd,z0),P(-hw,-hd,z0),P(0,-hd,z1),P(0,hd,z1)}),
   plain({P(-hw,-hd,z0),P(hw,-hd,z0),P(0,-hd,z1)}),plain({P(hw,hd,z0),P(-hw,hd,z0),P(0,hd,z1)}),plain({P(-hw,-hd,z0),P(-hw,hd,z0),P(hw,hd,z0),P(hw,-hd,z0)})};}
  finish(p);break;}
 case ShapePyramid:{auto& p=piece({P(-hw,-hd,z0),P(hw,-hd,z0),P(hw,hd,z0),P(-hw,hd,z0),P(0,0,z1)});
  if(faces){const Point3 c[4]={P(-hw,-hd,z0),P(hw,-hd,z0),P(hw,hd,z0),P(-hw,hd,z0)};
   for(int i=0;i<4;++i)p.faces.push_back(plain({c[i],c[(i+1)%4],P(0,0,z1)}));p.faces.push_back(plain({c[3],c[2],c[1],c[0]}));}
  finish(p);break;}
 case ShapeCone:case ShapeOctagon:{
  // A cone on a 16-sided ellipse, or an upright octagonal prism (flat sides square to its frame).
  const bool cone=s.shape==ShapeCone;const int n=cone?16:8;std::vector<Point3> base,top;std::vector<float> around{0};
  for(int i=0;i<n;++i){const float a=(i+(cone?0.f:.5f))*kTwoPi/n;base.push_back(P(hw*std::cos(a),hd*std::sin(a),z0));top.push_back(P(hw*std::cos(a),hd*std::sin(a),z1));}
  for(int i=0;i<n;++i)around.push_back(around.back()+lengthP(base[(i+1)%n]-base[i]));
  std::vector<Point3> pts=base;if(cone)pts.push_back(P(0,0,z1));else pts.insert(pts.end(),top.begin(),top.end());
  auto& p=piece(pts);
  if(faces){const float slant=std::sqrt(h*h+std::max(hw,hd)*std::max(hw,hd));
   for(int i=0;i<n;++i){const int j=(i+1)%n;Face f;f.mapped=true;
    if(cone)f.v={{base[i],around[i],slant},{base[j],around[i+1],slant},{P(0,0,z1),(around[i]+around[i+1])*.5f,0}};
    else f.v={{base[i],around[i],h},{base[j],around[i+1],h},{top[j],around[i+1],0},{top[i],around[i],0}};
    p.faces.push_back(f);}
   Face bottom;for(int i=n-1;i>=0;--i)bottom.v.push_back({base[i]});p.faces.push_back(bottom);
   if(!cone){Face cap;for(int i=0;i<n;++i)cap.v.push_back({top[i]});p.faces.push_back(cap);}}
  finish(p);break;}
 case ShapeChamfer:{
  const float c=prim::chamferOf(hw*2,hd*2,h);
  auto ring=[&](float a,float b,float cut,float z){return std::vector<Point3>{P(-a+cut,-b,z),P(a-cut,-b,z),P(a,-b+cut,z),P(a,b-cut,z),P(a-cut,b,z),P(-a+cut,b,z),P(-a,b-cut,z),P(-a,-b+cut,z)};};
  const auto low=ring(hw,hd,c,z0),mid=ring(hw,hd,c,z1-c),high=ring(hw-c,hd-c,c,z1);
  std::vector<Point3> pts=low;pts.insert(pts.end(),mid.begin(),mid.end());pts.insert(pts.end(),high.begin(),high.end());auto& p=piece(pts);
  if(faces){for(int i=0;i<8;++i){const int j=(i+1)%8;p.faces.push_back(plain({low[i],low[j],mid[j],mid[i]}));p.faces.push_back(plain({mid[i],mid[j],high[j],high[i]}));}
   Face b,t;for(int i=7;i>=0;--i)b.v.push_back({low[i]});for(int i=0;i<8;++i)t.v.push_back({high[i]});p.faces.push_back(b);p.faces.push_back(t);}
  finish(p);break;}
 case ShapeButtress:{const float front=z0+h*.3f;
  auto& p=piece({P(-hw,-hd,z0),P(hw,-hd,z0),P(-hw,hd,z0),P(hw,hd,z0),P(-hw,-hd,z1),P(hw,-hd,z1),P(-hw,hd,front),P(hw,hd,front)});
  if(faces)p.faces={plain({P(-hw,-hd,z1),P(hw,-hd,z1),P(hw,hd,front),P(-hw,hd,front)}),plain({P(-hw,hd,z0),P(hw,hd,z0),P(hw,hd,front),P(-hw,hd,front)}),
   plain({P(-hw,-hd,z0),P(hw,-hd,z0),P(hw,-hd,z1),P(-hw,-hd,z1)}),plain({P(hw,-hd,z0),P(hw,hd,z0),P(hw,hd,front),P(hw,-hd,z1)}),
   plain({P(-hw,-hd,z0),P(-hw,hd,z0),P(-hw,hd,front),P(-hw,-hd,z1)}),plain({P(-hw,-hd,z0),P(hw,-hd,z0),P(hw,hd,z0),P(-hw,hd,z0)})};
  finish(p);break;}
 case ShapeSpiral:{
  const auto sp=prim::spiralOf(hw*2,hd*2,h);const int hand=s.skew<0?-1:1;
  {std::vector<Point3> pts,ring;for(int i=0;i<8;++i){const float a=(i+.5f)*kTwoPi/8;ring.push_back(P(sp.post*std::cos(a),sp.post*std::sin(a),z0));}
   for(auto q:ring){pts.push_back(q);pts.push_back({q.x,q.y,z1});}auto& post=piece(pts);
   if(faces){float run=0;for(int i=0;i<8;++i){const int j=(i+1)%8;const float len=lengthP(ring[j]-ring[i]);Face f;f.mapped=true;
     f.v={{ring[i],run,h},{ring[j],run+len,h},{Point3{ring[j].x,ring[j].y,z1},run+len,0},{Point3{ring[i].x,ring[i].y,z1},run,0}};run+=len;post.faces.push_back(f);}
    Face cap;for(auto q:ring)cap.v.push_back({Point3{q.x,q.y,z1}});post.faces.push_back(cap);}
   finish(post);}
  for(int i=0;i<sp.risers;++i){const auto [a0,a1]=prim::spiralTread(sp,i,hand);const float am=(a0+a1)*.5f,top=z0+sp.riser*(i+1),under=top-prim::SpiralTreadThickness;
   auto at=[&](float r,float a,float z){return P(r*std::cos(a),r*std::sin(a),z);};
   const float in=sp.post*.98f,R=sp.radius;
   std::vector<Point3> plan{at(in,a0,0),at(R,a0,0),at(R,am,0),at(R,a1,0),at(in,a1,0)};
   std::vector<Point3> pts;for(auto q:plan){pts.push_back({q.x,q.y,under});pts.push_back({q.x,q.y,top});}auto& tread=piece(pts);
   if(faces){Face t,b;for(auto q:plan){t.v.push_back({Point3{q.x,q.y,top}});b.v.push_back({Point3{q.x,q.y,under}});}tread.faces.push_back(t);tread.faces.push_back(b);
    for(int k=0;k<5;++k){const auto A=plan[size_t(k)],B=plan[size_t((k+1)%5)];tread.faces.push_back(plain({{A.x,A.y,under},{B.x,B.y,under},{B.x,B.y,top},{A.x,A.y,top}}));}}
   finish(tread);}
  break;}
 case ShapeCulvert:{
  const auto c=prim::culvertOf(hw*2,h,s.skew);const float cz=z0+c.centre,R=c.radius,r=c.radius-c.shell;
  const float a0=std::asin(std::clamp(-c.centre/R,-1.f,1.f)),b0=std::asin(std::clamp(-c.centre/r,-1.f,1.f));constexpr int n=14;
  auto ring=[&](float rad,float start,float t){const float a=start+(pi-2*start)*t;return std::pair{rad*std::cos(a),cz+rad*std::sin(a)};};
  for(int k=0;k<n;++k){const float t0=float(k)/n,t1=float(k+1)/n;const auto o0=ring(R,a0,t0),o1=ring(R,a0,t1),i0=ring(r,b0,t0),i1=ring(r,b0,t1);
   std::vector<Point3> pts;for(float v:{-hd,hd})for(auto [u,z]:{o0,o1,i1,i0})pts.push_back(P(u,v,z));auto& p=piece(pts);
   if(faces){const float so0=R*(pi-2*a0)*t0,so1=R*(pi-2*a0)*t1,si0=r*(pi-2*b0)*t0,si1=r*(pi-2*b0)*t1;Face outer,inner;outer.mapped=inner.mapped=true;
    outer.v={{P(o0.first,-hd,o0.second),so0,0},{P(o1.first,-hd,o1.second),so1,0},{P(o1.first,hd,o1.second),so1,2*hd},{P(o0.first,hd,o0.second),so0,2*hd}};
    inner.v={{P(i0.first,-hd,i0.second),si0,0},{P(i1.first,-hd,i1.second),si1,0},{P(i1.first,hd,i1.second),si1,2*hd},{P(i0.first,hd,i0.second),si0,2*hd}};
    p.faces={outer,inner};
    for(float v:{-hd,hd})p.faces.push_back(plain({P(o0.first,v,o0.second),P(o1.first,v,o1.second),P(i1.first,v,i1.second),P(i0.first,v,i0.second)}));}
   finish(p);}
  break;}
 case ShapeArcWall:{
  const auto a=prim::arcWallOf(hw*2,hd*2,s.skew);constexpr int n=12;const float ri=a.radius-a.thick;
  auto at=[&](float rad,float t,float z){return P(rad*std::sin(t),-a.centre+rad*std::cos(t),z);};
  for(int k=0;k<n;++k){const float t0=-a.half+2*a.half*k/n,t1=-a.half+2*a.half*(k+1)/n;
   std::vector<Point3> pts;for(float z:{z0,z1})for(auto [rad,t]:{std::pair{a.radius,t0},{a.radius,t1},{ri,t1},{ri,t0}})pts.push_back(at(rad,t,z));auto& p=piece(pts);
   if(faces){const float so0=a.radius*(t0+a.half),so1=a.radius*(t1+a.half),si0=ri*(t0+a.half),si1=ri*(t1+a.half);Face outer,inner;outer.mapped=inner.mapped=true;
    outer.v={{at(a.radius,t0,z0),so0,h},{at(a.radius,t1,z0),so1,h},{at(a.radius,t1,z1),so1,0},{at(a.radius,t0,z1),so0,0}};
    inner.v={{at(ri,t0,z0),si0,h},{at(ri,t1,z0),si1,h},{at(ri,t1,z1),si1,0},{at(ri,t0,z1),si0,0}};
    p.faces={outer,inner,plain({at(a.radius,t0,z1),at(a.radius,t1,z1),at(ri,t1,z1),at(ri,t0,z1)}),plain({at(a.radius,t0,z0),at(a.radius,t1,z0),at(ri,t1,z0),at(ri,t0,z0)})};
    if(k==0)p.faces.push_back(plain({at(a.radius,t0,z0),at(ri,t0,z0),at(ri,t0,z1),at(a.radius,t0,z1)}));
    if(k==n-1)p.faces.push_back(plain({at(a.radius,t1,z0),at(ri,t1,z0),at(ri,t1,z1),at(a.radius,t1,z1)}));}
   finish(p);}
  break;}
 default:break;}
}

// ---- convex pieces as half-spaces ----
struct Plane {Point3 n;float d=0;};
struct ConvexPiece {std::vector<Plane> planes;Point3 lo,hi;int owner=-1;bool box=false;};
// The hull of a handful of points, as the planes of its faces (all points on or behind each).
inline bool hullPiece(const std::vector<Point3>& pts,int owner,ConvexPiece& out){
 out={};out.owner=owner;if(pts.size()<4)return false;
 out.lo=out.hi=pts[0];for(auto p:pts){out.lo={std::min(out.lo.x,p.x),std::min(out.lo.y,p.y),std::min(out.lo.z,p.z)};out.hi={std::max(out.hi.x,p.x),std::max(out.hi.y,p.y),std::max(out.hi.z,p.z)};}
 const float scale=std::max({out.hi.x-out.lo.x,out.hi.y-out.lo.y,out.hi.z-out.lo.z});if(scale<1e-4f)return false;
 const float eps=std::max(1e-5f,scale*2e-5f);
 for(size_t i=0;i<pts.size();++i)for(size_t j=i+1;j<pts.size();++j)for(size_t k=j+1;k<pts.size();++k){
  Point3 n=crossP(pts[j]-pts[i],pts[k]-pts[i]);const float len=lengthP(n);if(len<scale*scale*1e-6f)continue;n=n*(1.f/len);float d=dotP(n,pts[i]);
  bool front=false,back=false;for(auto p:pts){const float e=dotP(n,p)-d;front|=e>eps;back|=e<-eps;if(front&&back)break;}
  if(front&&back)continue;if(front){n=n*-1.f;d=-d;}
  bool known=false;for(const auto& q:out.planes)known|=dotP(q.n,n)>1-1e-5f&&std::fabs(q.d-d)<eps*4;if(!known)out.planes.push_back({n,d});}
 return out.planes.size()>=4;}
inline ConvexPiece boxPiece(Point3 lo,Point3 hi,int owner){
 ConvexPiece p;p.lo=lo;p.hi=hi;p.owner=owner;p.box=true;
 p.planes={{{1,0,0},hi.x},{{-1,0,0},-lo.x},{{0,1,0},hi.y},{{0,-1,0},-lo.y},{{0,0,1},hi.z},{{0,0,-1},-lo.z}};return p;}

// ---- clipping a face ----
// A vertex of a face being clipped: where it is and two numbers carried along with it
// (a quad's own s, t across it, or a triangle's texture coordinates).
struct ClipVertex {Point3 p;float a=0,b=0;};
using Polygon=std::vector<ClipVertex>;
inline Point3 polygonNormal(const Polygon& v){Point3 n{};for(size_t i=0;i<v.size();++i){const auto a=v[i].p,b=v[(i+1)%v.size()].p;n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}
 const float l=lengthP(n);return l>1e-12f?n*(1.f/l):Point3{};}
inline float polygonArea(const Polygon& v){Point3 n{};for(size_t i=0;i<v.size();++i){const auto a=v[i].p,b=v[(i+1)%v.size()].p;n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}return lengthP(n)*.5f;}
inline constexpr float PlaneEpsilon=2e-4f;
// The parts of `face` (owner `owner`, unit normal `n`) outside `piece`, appended to `out`, and
// true, when some of it lies inside the piece; false (nothing appended) when none does.
inline bool clipAgainst(const Polygon& face,Point3 n,int owner,const ConvexPiece& piece,std::vector<Polygon>& out){
 Polygon inside=face;std::vector<float> dist;std::vector<Polygon> kept;
 for(const auto& plane:piece.planes){
  dist.resize(inside.size());bool front=false,back=false;
  for(size_t i=0;i<inside.size();++i){dist[i]=dotP(plane.n,inside[i].p)-plane.d;front|=dist[i]>PlaneEpsilon;back|=dist[i]<-PlaneEpsilon;}
  if(!front&&!back){
   // On this face of the piece: touching it back to back is buried; lying flush with it, the later record draws.
   const float c=dotP(plane.n,n);
   if(c<-.999f||(c>.999f&&piece.owner>owner))continue;
   return false;}
  if(!back)return false; // what is left lies outside: none of the face is in the piece
  if(!front)continue;
  Polygon keep,rest;
  for(size_t i=0;i<inside.size();++i){const size_t j=(i+1)%inside.size();const auto& A=inside[i];const auto& B=inside[j];const float da=dist[i],db=dist[j];
   if(da>PlaneEpsilon)keep.push_back(A);else if(da<-PlaneEpsilon)rest.push_back(A);else{keep.push_back(A);rest.push_back(A);}
   if((da>PlaneEpsilon&&db<-PlaneEpsilon)||(da<-PlaneEpsilon&&db>PlaneEpsilon)){const float t=da/(da-db);
    const ClipVertex X{A.p+(B.p-A.p)*t,A.a+(B.a-A.a)*t,A.b+(B.b-A.b)*t};keep.push_back(X);rest.push_back(X);}}
  if(rest.size()<3||polygonArea(rest)<1e-7f)return false;
  if(keep.size()>=3&&polygonArea(keep)>1e-7f)kept.push_back(std::move(keep));
  inside=std::move(rest);}
 // What is left lies inside every plane: buried.
 for(auto& k:kept)out.push_back(std::move(k));
 return true;}

// Splits `poly` by the plane n.p = d into the part in front and the part behind.
inline void splitPolygon(const Polygon& poly,Point3 n,float d,Polygon& front,Polygon& back){
 front.clear();back.clear();
 for(size_t i=0;i<poly.size();++i){const auto& A=poly[i];const auto& B=poly[(i+1)%poly.size()];const float da=dotP(n,A.p)-d,db=dotP(n,B.p)-d;
  if(da>=0)front.push_back(A);if(da<=0)back.push_back(A);
  if((da>0&&db<0)||(da<0&&db>0)){const float t=da/(da-db);const ClipVertex X{A.p+(B.p-A.p)*t,A.a+(B.a-A.a)*t,A.b+(B.b-A.b)*t};front.push_back(X);back.push_back(X);}}
 if(front.size()<3||polygonArea(front)<1e-8f)front.clear();if(back.size()<3||polygonArea(back)<1e-8f)back.clear();}
// `poly` cut into cells about `size` metres across in its own plane, so a big face is lit at
// as many points as Scene3D's wall grid lights it.
inline void gridSplit(const Polygon& poly,float size,std::vector<Polygon>& out){
 if(poly.size()<3)return;const Point3 n=polygonNormal(poly);
 size_t longest=0;float best=-1;for(size_t i=0;i<poly.size();++i){const float l=lengthP(poly[(i+1)%poly.size()].p-poly[i].p);if(l>best){best=l;longest=i;}}
 Point3 e1=poly[(longest+1)%poly.size()].p-poly[longest].p;if(best<1e-6f){out.push_back(poly);return;}e1=e1*(1.f/best);const Point3 e2=crossP(n,e1);
 std::vector<Polygon> strips;
 for(int pass=0;pass<2;++pass){const Point3 axis=pass?e2:e1;std::vector<Polygon> in=pass?std::move(strips):std::vector<Polygon>{poly},done;
  for(auto& p:in){float lo=1e9f,hi=-1e9f;for(auto& v:p){lo=std::min(lo,dotP(axis,v.p));hi=std::max(hi,dotP(axis,v.p));}
   const int cells=std::clamp(int(std::ceil((hi-lo)/size-1e-3f)),1,16);Polygon rest=p,front,back;
   for(int k=1;k<cells&&!rest.empty();++k){splitPolygon(rest,axis,lo+(hi-lo)*k/cells,front,back);if(!back.empty())done.push_back(back);rest=front;}
   if(!rest.empty())done.push_back(rest);}
  if(pass)out.insert(out.end(),done.begin(),done.end());else strips=std::move(done);}}

// ---- every solid of a map, found by where it is ----
struct CullIndex {
 std::vector<ConvexPiece> pieces;
 // Each owner's pieces are added together: [first, last) by owner.
 std::vector<std::pair<int,std::pair<int,int>>> owners;
 void add(ConvexPiece p){if(owners.empty()||owners.back().first!=p.owner)owners.push_back({p.owner,{int(pieces.size()),int(pieces.size())}});pieces.push_back(std::move(p));owners.back().second.second=int(pieces.size());}
 std::pair<int,int> ownerPieces(int owner)const{auto it=std::lower_bound(owners.begin(),owners.end(),owner,[](const auto& e,int o){return e.first<o;});return it!=owners.end()&&it->first==owner?it->second:std::pair{0,0};}
 bool hasOwner(int owner)const{const auto r=ownerPieces(owner);return r.second>r.first;}
 // A face's outward side: the face of its own solid it lies on, else its winding.
 Point3 outward(const Polygon& face,int owner)const{
  const auto r=ownerPieces(owner);
  for(int i=r.first;i<r.second;++i)for(const auto& plane:pieces[size_t(i)].planes){bool on=true;for(const auto& v:face)on&=std::fabs(dotP(plane.n,v.p)-plane.d)<PlaneEpsilon*2;if(on)return plane.n;}
  return polygonNormal(face);}
 float cell=2,ox=0,oy=0;int nx=0,ny=0;std::vector<std::vector<int>> grid;
 mutable std::vector<unsigned> stamp;mutable unsigned visit=0;
 void build(){
  grid.clear();nx=ny=0;stamp.assign(pieces.size(),0);visit=0;if(pieces.empty())return;
  float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;for(const auto& p:pieces){x0=std::min(x0,p.lo.x);y0=std::min(y0,p.lo.y);x1=std::max(x1,p.hi.x);y1=std::max(y1,p.hi.y);}
  ox=x0;oy=y0;nx=std::clamp(int((x1-x0)/cell)+1,1,512);ny=std::clamp(int((y1-y0)/cell)+1,1,512);cell=std::max(cell,std::max((x1-x0)/nx,(y1-y0)/ny)+1e-3f);
  grid.assign(size_t(nx)*ny,{});
  for(int i=0;i<int(pieces.size());++i){const auto& p=pieces[size_t(i)];
   for(int gy=cellOf(p.lo.y,oy,ny);gy<=cellOf(p.hi.y,oy,ny);++gy)for(int gx=cellOf(p.lo.x,ox,nx);gx<=cellOf(p.hi.x,ox,nx);++gx)grid[size_t(gy*nx+gx)].push_back(i);}}
 int cellOf(float v,float o,int n)const{return std::clamp(int(std::floor((v-o)/cell)),0,n-1);}
 template<class Visit> void query(Point3 lo,Point3 hi,Visit visitPiece)const{
  if(grid.empty())return;if(++visit==0){std::fill(stamp.begin(),stamp.end(),0u);visit=1;}
  constexpr float e=1e-4f;
  for(int gy=cellOf(lo.y-e,oy,ny);gy<=cellOf(hi.y+e,oy,ny);++gy)for(int gx=cellOf(lo.x-e,ox,nx);gx<=cellOf(hi.x+e,ox,nx);++gx)
   for(int i:grid[size_t(gy*nx+gx)]){if(stamp[size_t(i)]==visit)continue;stamp[size_t(i)]=visit;const auto& p=pieces[size_t(i)];
    if(p.hi.x<lo.x-e||p.lo.x>hi.x+e||p.hi.y<lo.y-e||p.lo.y>hi.y+e||p.hi.z<lo.z-e||p.lo.z>hi.z+e)continue;visitPiece(p);}}
 // The visible parts of `face`. False when no solid touches it (draw it whole); true with
 // the parts in `out` (none: it is buried). skipBoxes: boxes were cut away already.
 bool clip(const Polygon& face,int owner,bool skipBoxes,std::vector<Polygon>& out)const{
  out.clear();if(face.size()<3||pieces.empty())return false;
  const Point3 n=outward(face,owner);if(lengthP(n)<.5f)return false;
  Point3 lo=face[0].p,hi=face[0].p;for(const auto& v:face){lo={std::min(lo.x,v.p.x),std::min(lo.y,v.p.y),std::min(lo.z,v.p.z)};hi={std::max(hi.x,v.p.x),std::max(hi.y,v.p.y),std::max(hi.z,v.p.z)};}
  bool touched=false;std::vector<Polygon> parts{face},next;
  query(lo,hi,[&](const ConvexPiece& piece){
   if(piece.owner==owner||(skipBoxes&&piece.box)||parts.empty())return;
   next.clear();bool cut=false;for(const auto& part:parts){if(clipAgainst(part,n,owner,piece,next))cut=true;else next.push_back(part);}
   if(cut){touched=true;parts.swap(next);}});
  if(!touched)return false;out=std::move(parts);return true;}
};

// Whether a structure is drawn as a solid that hides what it covers (glass, mesh, rails, light
// shafts, collision proxies, hidden and water boxes, flooring laid on the land do not).
inline bool solidStructure(const Structure& s){
 return !s.rail&&!s.drape&&s.material!=1&&s.material!=6&&s.material!=9&&!seeThroughMaterial(s.material)&&s.material!=CollisionProxyMaterial&&
  s.material!=LightShaftMaterial&&s.x2>s.x1&&s.y2>s.y1&&s.top>s.bottom&&(s.shape<ShapeLip||newShape(s.shape));}
// The index Scene3D draws a map against: the boxes its face trimming already knows (wall tiles
// and solid boxes, exposedBoxFace's list), then every solid shape's pieces. Structure i is owner
// ownerBase + i; keep(i) leaves out more (breakable structures, drawn on their own).
template<class Keep> std::shared_ptr<CullIndex> buildIndex(const std::vector<SurfaceBox>& boxes,const std::vector<Structure>& list,int ownerBase,Keep keep){
 auto index=std::make_shared<CullIndex>();std::vector<ConvexPiece> all;std::vector<ShapePiece> shapes;
 for(const auto& b:boxes){if(b.owner>=ownerBase){const size_t i=size_t(b.owner-ownerBase);if(i<list.size()&&list[i].drape)continue;}all.push_back(boxPiece(b.low,b.high,b.owner));}
 for(size_t i=0;i<list.size();++i){const auto& st=list[i];if(st.shape==ShapeBox||!solidStructure(st)||!keep(i))continue;
  shapes.clear();shapePieces(st,shapes,false);for(const auto& piece:shapes){ConvexPiece hull;if(hullPiece(piece.points,ownerBase+int(i),hull))all.push_back(std::move(hull));}}
 std::stable_sort(all.begin(),all.end(),[](const ConvexPiece& a,const ConvexPiece& b){return a.owner<b.owner;});
 for(auto& p:all)index->add(std::move(p));index->build();return index;}
}

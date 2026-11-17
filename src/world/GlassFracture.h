#pragma once

#include "../core/Math.h"
#include <array>
#include <cstdint>
#include <vector>

namespace retro::glass {
// A pane's fracture, in its own plane: u runs 0..width along it, v 0..height
// up it. Everything is a pure function of the pane's size, where it was hit
// and a seed, so the shards that fall, the ones left lying on the floor and
// the teeth left in the frame agree across frames and after a reload.

inline float hashUnit(std::uint32_t a,std::uint32_t b=0,std::uint32_t c=0){
 std::uint32_t h=a*0x9e3779b1u^(b+0x7f4a7c15u)*0x85ebca6bu^(c+0x165667b1u)*0xc2b2ae35u;
 h^=h>>15;h*=0x2c1b3c6du;h^=h>>12;h*=0x297a2d39u;h^=h>>15;return float(h&0xffffffu)/float(0x1000000u);}

struct Polygon {std::array<Vec2,10> p{};int n=0;
 Vec2 centroid()const{Vec2 c{};for(int i=0;i<n;++i)c+=p[size_t(i)];return n?c*(1.f/float(n)):c;}
 float area()const{float a=0;for(int i=0;i<n;++i){auto q=p[size_t(i)],r=p[size_t((i+1)%n)];a+=q.x*r.y-r.x*q.y;}return std::fabs(a)*.5f;}};

// One loose piece: ring 0 is the crushed centre that the round blows out,
// higher rings let go later, so the pane comes apart from the hole outward.
struct Shard {Polygon shape;int ring=0;float delay=0;float spin=0;float tumble=0;float scatter=0;float yaw=0;};

inline Polygon clipToPane(const Polygon& in,float width,float height){
 Polygon poly=in;
 auto clip=[&](auto inside,auto cross){Polygon out;for(int i=0;i<poly.n;++i){Vec2 a=poly.p[size_t(i)],b=poly.p[size_t((i+1)%poly.n)];bool ia=inside(a),ib=inside(b);
   if(ia&&out.n<10)out.p[size_t(out.n++)]=a;if(ia!=ib&&out.n<10)out.p[size_t(out.n++)]=cross(a,b);}poly=out;};
 auto lerpAt=[](Vec2 a,Vec2 b,float t){return a+(b-a)*t;};
 clip([](Vec2 q){return q.x>=0;},[&](Vec2 a,Vec2 b){return lerpAt(a,b,a.x/(a.x-b.x));});
 clip([&](Vec2 q){return q.x<=width;},[&](Vec2 a,Vec2 b){return lerpAt(a,b,(a.x-width)/(a.x-b.x));});
 clip([](Vec2 q){return q.y>=0;},[&](Vec2 a,Vec2 b){return lerpAt(a,b,a.y/(a.y-b.y));});
 clip([&](Vec2 q){return q.y<=height;},[&](Vec2 a,Vec2 b){return lerpAt(a,b,(a.y-height)/(a.y-b.y));});
 return poly;}

// Radial cracks from the hit and rings round it, every band split so no
// piece runs much past 0.4 m: the spider-web a struck sheet of glass breaks into.
inline std::vector<Shard> fracture(float width,float height,Vec2 hit,std::uint32_t seed){
 std::vector<Shard> shards;if(width<=0||height<=0)return shards;
 hit={std::clamp(hit.x,0.f,width),std::clamp(hit.y,0.f,height)};
 const int rays=9+int(hashUnit(seed,1)*4.f);
 std::vector<float> bearing(static_cast<size_t>(rays));const float start=hashUnit(seed,2)*kTwoPi;
 for(int i=0;i<rays;++i)bearing[size_t(i)]=start+kTwoPi*(i+.5f*(hashUnit(seed,3,i)-.5f))/float(rays);
 const float reach=std::sqrt(width*width+height*height)+.05f;
 std::vector<float> rings{0};
 for(float r=.045f+.02f*hashUnit(seed,4);;){rings.push_back(r);if(r>=reach*1.2f)break;r=std::min(r*(1.5f+.25f*hashUnit(seed,5,std::uint32_t(rings.size()))),r+.38f);}
 // Each ring's corners sit between ray i and i+1 at the fractions its inner
 // band splits that gap into; the band outside splits it by a multiple of
 // that, finding its inner corners along the same edges, so pieces tile exactly.
 auto span=[&](int i){return (i+1<rays?bearing[size_t(i+1)]:bearing[0]+kTwoPi)-bearing[size_t(i)];};
 auto corner=[&](int ring,int i,int j,int parts){i%=rays;if(j==parts){i=(i+1)%rays;j=0;}
  if(!ring)return hit;const std::uint32_t key=j?std::uint32_t(i*4096+j*4096/parts):std::uint32_t(i*4096);
  const float a=bearing[size_t(i)]+span(i)*(float(j)+(j?.7f*(hashUnit(seed,60+ring,key)-.5f):0.f))/float(parts)+.12f*(hashUnit(seed,80+ring,key)-.5f);
  const float r=rings[size_t(ring)],gap=std::min(r-rings[size_t(ring-1)],ring+1<int(rings.size())?rings[size_t(ring+1)]-r:r);
  return hit+Vec2{std::cos(a),std::sin(a)}*(r+.6f*gap*(hashUnit(seed,6+ring,key)-.5f));};
 std::vector<int> inner(size_t(rays),1);
 for(int k=0;k+1<int(rings.size());++k)for(int i=0;i<rays;++i){
  const int coarse=inner[size_t(i)],split=coarse*std::max(1,int(std::ceil(span(i)*rings[size_t(k+1)]/.4f/float(coarse))));inner[size_t(i)]=split;
  auto along=[&](int s){const int per=split/coarse,j=s/per;const float t=float(s%per)/float(per);const Vec2 a=corner(k,i,j,coarse),b=corner(k,i,j+1,coarse);return a+(b-a)*t;};
  for(int s=0;s<split;++s){
   Polygon cell;
   if(k){cell.p[0]=along(s);cell.p[1]=corner(k+1,i,s,split);cell.p[2]=corner(k+1,i,s+1,split);cell.p[3]=along(s+1);cell.n=4;}
   else{cell.p[0]=hit;cell.p[1]=corner(1,i,s,split);cell.p[2]=corner(1,i,s+1,split);cell.n=3;}
   auto piece=clipToPane(cell,width,height);if(piece.n<3||piece.area()<4e-4f)continue;
   const std::uint32_t id=std::uint32_t(shards.size());Shard shard;shard.shape=piece;shard.ring=k;
   shard.delay=k<=1?0.f:std::min(1.3f,.06f+.1f*float(k-1)+.3f*hashUnit(seed,40,id));
   shard.spin=(hashUnit(seed,41,id)-.5f)*14.f;shard.tumble=(hashUnit(seed,42,id)-.5f)*10.f;shard.scatter=hashUnit(seed,43,id);shard.yaw=hashUnit(seed,44,id)*kTwoPi;
   shards.push_back(shard);}}
 return shards;}

// The jagged rim left standing in the frame: teeth along every edge, deepest
// along the bottom where the sill holds them.
inline std::vector<Polygon> teeth(float width,float height,std::uint32_t seed){
 std::vector<Polygon> out;if(width<=0||height<=0)return out;
 auto edge=[&](Vec2 from,Vec2 to,Vec2 inward,float deepest,std::uint32_t side){
  const float span=length(to-from);float at=0;int n=0;
  while(at<span-.01f){float step=std::min(span-at,.05f+.16f*hashUnit(seed,side,n));float depth=.012f+deepest*hashUnit(seed,side+10,n)*hashUnit(seed,side+20,n);
   Vec2 dir=(to-from)*(1.f/span),a=from+dir*at,b=from+dir*(at+step),tip=from+dir*(at+step*(.2f+.6f*hashUnit(seed,side+30,n)))+inward*depth;
   Polygon t;t.p[0]=a;t.p[1]=b;t.p[2]=tip;t.n=3;out.push_back(clipToPane(t,width,height));at+=step;++n;}};
 edge({0,0},{width,0},{0,1},std::min(.24f,height*.3f),100);
 edge({width,height},{0,height},{0,-1},std::min(.12f,height*.2f),200);
 edge({0,height},{0,0},{1,0},std::min(.1f,width*.2f),300);
 edge({width,0},{width,height},{-1,0},std::min(.1f,width*.2f),400);
 return out;}

// Spider-web cracks round one hit while the pane still stands: radial rays
// that lengthen as the pane weakens (strain 0..1) and broken rings joining them.
struct Crack {Vec2 a,b;float width;};
inline std::vector<Crack> cracks(Vec2 hit,float strain,std::uint32_t seed){
 std::vector<Crack> out;const int rays=6+int(hashUnit(seed,1)*5.f);const float start=hashUnit(seed,2)*kTwoPi;
 std::vector<std::array<Vec2,3>> knots;
 for(int i=0;i<rays;++i){float a=start+kTwoPi*(i+.6f*(hashUnit(seed,3,i)-.5f))/float(rays);
  float reach=(.05f+.12f*hashUnit(seed,4,i))*(1.f+5.f*std::clamp(strain,0.f,1.f));Vec2 p=hit;std::array<Vec2,3> ray{};
  const int segments=3+int(hashUnit(seed,5,i)*3.f);
  for(int s=0;s<segments;++s){a+=(hashUnit(seed,6+s,i)-.5f)*.7f;Vec2 q=p+Vec2{std::cos(a),std::sin(a)}*(reach/float(segments));
   out.push_back({p,q,.003f*(1.f-.5f*float(s)/float(segments))});if(s<3)ray[size_t(s)]=q;p=q;
   if(s>0&&hashUnit(seed,20+s,i)<.35f){float side=a+(hashUnit(seed,30+s,i)<.5f?.8f:-.8f);out.push_back({p,p+Vec2{std::cos(side),std::sin(side)}*reach*.25f,.0022f});}}
  knots.push_back(ray);}
 for(int ring=0;ring<2;++ring)for(int i=0;i<rays;++i)if(hashUnit(seed,50+ring,i)<.65f)out.push_back({knots[size_t(i)][size_t(ring)],knots[size_t((i+1)%rays)][size_t(ring)],.0026f});
 return out;}

// ---- A pane that breaks a piece at a time (as Half-Life 2's panes break) ----
// Until it comes down whole, a pane is a lattice of small jittered triangles about 9 cm
// across. Each blow is a Hit: a ragged hole of `radius` round where it struck (0: a star,
// no hole) and crack `strain` reaching out past the hole's rim. A triangle is gone when it
// lies in a hole, when the strain of the hits round it passes its toughness (the glass
// between close holes gives way), or when nothing joins it to the frame any more (a piece
// cut loose falls out). Laminated (holds) glass loses only what its holes take. All of it
// is a function of the pane's size and seed and its hits, so the hits are all a save keeps.

// The pane's own plane: u along it, v up it (a flat pane, a skylight or glass floor: u along
// x, v along y, at its top). The renderer's frame for an upright pane is the same.
struct Frame {Point3 origin{},along{},up{},normal{};float width=0,height=0;bool flat=false;
 Point3 at(Vec2 q,float off=0)const{return origin+along*q.x+up*q.y+normal*off;}
 Vec2 local(Point3 p)const{const Point3 d=p-origin;return {d.x*along.x+d.y*along.y+d.z*along.z,d.x*up.x+d.y*up.y+d.z*up.z};}
 float across(Point3 p)const{const Point3 d=p-origin;return d.x*normal.x+d.y*normal.y+d.z*normal.z;}
 Vec2 clamp(Vec2 q)const{return {std::clamp(q.x,0.f,width),std::clamp(q.y,0.f,height)};}};
inline Frame frame(float x1,float y1,float x2,float y2,float bottom,float top){
 const float dx=x2-x1,dy=y2-y1;
 if(top-bottom<std::min(dx,dy))return Frame{{x1,y1,top},{1,0,0},{0,1,0},{0,0,1},dx,dy,true};
 const Vec2 c{(x1+x2)*.5f,(y1+y2)*.5f};
 return dx>dy?Frame{{x1,c.y,bottom},{1,0,0},{0,0,1},{0,1,0},dx,top-bottom,false}:Frame{{c.x,y1,bottom},{0,1,0},{0,0,1},{1,0,0},dy,top-bottom,false};}
inline std::uint32_t paneSeed(int level,int index){return std::uint32_t(level*7919+index*104729+17);}

struct Hit {float u=0,v=0,radius=0,strain=0;};
// A hit as a save keeps it, and as the pane keeps it from the first (so a reload breaks
// exactly as the pane did): where, in 1/65535ths of the pane, the hole's radius in 4 mm
// steps (to 1.02 m) and the strain in 1/255ths.
struct PackedHit {std::uint16_t u=0,v=0;std::uint8_t radius=0,strain=0;};
inline PackedHit pack(const Hit& h,float width,float height){
 return {std::uint16_t(std::lround(std::clamp(h.u/std::max(width,1e-4f),0.f,1.f)*65535.f)),std::uint16_t(std::lround(std::clamp(h.v/std::max(height,1e-4f),0.f,1.f)*65535.f)),
  std::uint8_t(std::lround(std::clamp(h.radius,0.f,1.02f)*250.f)),std::uint8_t(std::lround(std::clamp(h.strain,0.f,1.f)*255.f))};}
inline Hit unpack(PackedHit p,float width,float height){return {float(p.u)/65535.f*width,float(p.v)/65535.f*height,float(p.radius)/250.f,float(p.strain)/255.f};}
inline Hit quantized(const Hit& h,float width,float height){return unpack(pack(h,width,height),width,height);}

// The triangles: a grid of quads, each vertex inside the pane jittered (those on its edges
// only along the edge), each quad split on a seeded diagonal.
struct Lattice {int nx=1,ny=1;float width=0,height=0;std::uint32_t seed=0;
 int cells()const{return nx*ny*2;}
 Vec2 vertex(int i,int j)const{Vec2 p{width*float(i)/float(nx),height*float(j)/float(ny)};
  if(i>0&&i<nx)p.x+=(hashUnit(seed,1000+std::uint32_t(i),std::uint32_t(j))*2-1)*.28f*width/float(nx);
  if(j>0&&j<ny)p.y+=(hashUnit(seed,2000+std::uint32_t(i),std::uint32_t(j))*2-1)*.28f*height/float(ny);return p;}
 Vec2 point(int id)const{return vertex(id%(nx+1),id/(nx+1));}
 std::array<int,3> corners(int c)const{const int q=c/2,i=q%nx,j=q/nx,a=j*(nx+1)+i,b=a+1,d=a+nx+1,e=d+1;
  if(hashUnit(seed,3000+std::uint32_t(i),std::uint32_t(j))<.5f)return c%2?std::array<int,3>{a,e,d}:std::array<int,3>{a,b,e};
  return c%2?std::array<int,3>{b,e,d}:std::array<int,3>{a,b,d};}
 std::array<Vec2,3> triangle(int c)const{const auto k=corners(c);return {point(k[0]),point(k[1]),point(k[2])};}
 Vec2 centre(int c)const{const auto t=triangle(c);return (t[0]+t[1]+t[2])*(1.f/3.f);}
 bool onFrame(int id)const{const int i=id%(nx+1),j=id/(nx+1);return i==0||i==nx||j==0||j==ny;}
 // A triangle with an edge on the frame is held by it.
 bool anchored(int c)const{int n=0;for(int id:corners(c))n+=onFrame(id);return n>=2;}
 int at(Vec2 q)const{const int i0=int(std::floor(q.x/std::max(width,1e-4f)*float(nx))),j0=int(std::floor(q.y/std::max(height,1e-4f)*float(ny)));
  auto inside=[](Vec2 p,const std::array<Vec2,3>& t){auto side=[&](Vec2 a,Vec2 b){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);};
   const float s0=side(t[0],t[1]),s1=side(t[1],t[2]),s2=side(t[2],t[0]);return (s0>=-1e-6f&&s1>=-1e-6f&&s2>=-1e-6f)||(s0<=1e-6f&&s1<=1e-6f&&s2<=1e-6f);};
  for(int r=0;r<2;++r)for(int dj=-r;dj<=r;++dj)for(int di=-r;di<=r;++di){const int i=i0+di,j=j0+dj;if(i<0||j<0||i>=nx||j>=ny)continue;
   for(int k=0;k<2;++k){const int c=(j*nx+i)*2+k;if(inside(q,triangle(c)))return c;}}
  return -1;}
 // Each triangle's neighbours across its three edges (-1: the frame).
 std::vector<std::array<int,3>> neighbours()const{std::vector<std::array<int,3>> out(size_t(cells()),{-1,-1,-1});
  struct Edge {std::uint32_t key;int cell,side;};std::vector<Edge> edges;edges.reserve(size_t(cells())*3);
  for(int c=0;c<cells();++c){const auto k=corners(c);for(int s=0;s<3;++s){const int a=k[size_t(s)],b=k[size_t((s+1)%3)];edges.push_back({std::uint32_t(std::min(a,b))*65536u+std::uint32_t(std::max(a,b)),c,s});}}
  std::sort(edges.begin(),edges.end(),[](const Edge& a,const Edge& b){return a.key<b.key;});
  for(size_t i=0;i+1<edges.size();++i)if(edges[i].key==edges[i+1].key){out[size_t(edges[i].cell)][size_t(edges[i].side)]=edges[i+1].cell;out[size_t(edges[i+1].cell)][size_t(edges[i+1].side)]=edges[i].cell;}
  return out;}};
inline Lattice lattice(float width,float height,std::uint32_t seed){Lattice l;l.width=std::max(width,.01f);l.height=std::max(height,.01f);l.seed=seed;
 const float step=std::max(.09f,std::max(l.width,l.height)/48.f);l.nx=std::clamp(int(std::lround(l.width/step)),1,64);l.ny=std::clamp(int(std::lround(l.height/step)),1,64);return l;}

// How far a hole reaches at the bearing of q: seven lobes seeded by where it is, so no hole is round.
inline float holeReach(const Hit& h,std::uint32_t seed,Vec2 q){if(h.radius<=0)return 0;
 const std::uint32_t at=std::uint32_t(std::lround(h.u*1000.f))*7919u+std::uint32_t(std::lround(h.v*1000.f));
 const float t=(std::atan2(q.y-h.v,q.x-h.u)+kPi)/kTwoPi*7.f;const int i=int(t)%7;const float f=t-std::floor(t);
 // A thing's own hole (0.2 m and more) is ragged less deeply, so what made it fits through it.
 const float least=h.radius>=.2f?.9f:.7f,spread=h.radius>=.2f?.3f:.55f;
 const float a=least+spread*hashUnit(seed,at,std::uint32_t(i)),b=least+spread*hashUnit(seed,at,std::uint32_t((i+1)%7));return h.radius*(a+(b-a)*f);}
// How far past its rim a hit's cracks carry strain.
inline float crackReach(const Hit& h){return .1f+1.6f*h.radius+.22f*h.strain;}
// The strain all the hits put on the glass at q (a laminated pane's measure of how far it is punched).
inline float strainAt(const std::vector<Hit>& hits,Vec2 q){float s=0;
 for(const auto& h:hits){const float d=std::max(0.f,length(q-Vec2{h.u,h.v})-h.radius);s+=h.strain*std::clamp(1.f-d/crackReach(h),0.f,1.f);}return s;}
inline bool inHole(const std::vector<Hit>& hits,Vec2 q,std::uint32_t seed){
 for(const auto& h:hits)if(h.radius>0&&length(q-Vec2{h.u,h.v})<holeReach(h,seed,q))return true;return false;}
inline float toughness(const Lattice& l,int c){return 1.f+.45f*hashUnit(l.seed,700,std::uint32_t(c));}

// Which triangles are gone (1) for these hits; returns how many.
inline int solve(const Lattice& l,const std::vector<Hit>& hits,bool laminated,std::vector<std::uint8_t>& gone){
 gone.assign(size_t(l.cells()),0);if(hits.empty())return 0;
 for(int c=0;c<l.cells();++c){const Vec2 m=l.centre(c);if(inHole(hits,m,l.seed)||(!laminated&&strainAt(hits,m)>=toughness(l,c)))gone[size_t(c)]=1;}
 // The triangle a round went through goes with it, however small its hole.
 for(const auto& h:hits)if(h.radius>0){const int c=l.at({h.u,h.v});if(c>=0)gone[size_t(c)]=1;}
 if(!laminated){
  // What no longer hangs from the frame falls out.
  const auto nb=l.neighbours();std::vector<std::uint8_t> held(gone.size(),0);std::vector<int> open;
  for(int c=0;c<l.cells();++c)if(!gone[size_t(c)]&&l.anchored(c)){held[size_t(c)]=1;open.push_back(c);}
  while(!open.empty()){const int c=open.back();open.pop_back();for(int n:nb[size_t(c)])if(n>=0&&!gone[size_t(n)]&&!held[size_t(n)]){held[size_t(n)]=1;open.push_back(n);}}
  for(int c=0;c<l.cells();++c)if(!held[size_t(c)])gone[size_t(c)]=1;}
 int n=0;for(auto g:gone)n+=g;return n;}

// The pieces a blow lets go (the triangles gone now that were not before), gathered into
// shards no more than about 0.3 m across. `blown`: from the hole itself (driven along the
// blow); otherwise loosened round it, they drop.
struct Piece {Vec2 centre{},low{},high{};float area=0;bool blown=false;int cells=0;};
inline std::vector<Piece> pieces(const Lattice& l,const std::vector<std::uint8_t>& before,const std::vector<std::uint8_t>& after,const Hit& hit){
 std::vector<Piece> out;if(after.size()!=size_t(l.cells()))return out;const auto nb=l.neighbours();std::vector<std::uint8_t> taken(after.size(),0);
 auto fresh=[&](int c){return after[size_t(c)]&&!(c<int(before.size())&&before[size_t(c)])&&!taken[size_t(c)];};
 for(int c=0;c<l.cells();++c)if(fresh(c)){Piece p;p.low={1e9f,1e9f};p.high={-1e9f,-1e9f};Vec2 sum{};std::vector<int> open{c};taken[size_t(c)]=1;
  while(!open.empty()){const int k=open.back();open.pop_back();const auto t=l.triangle(k);
   const float a=std::fabs((t[1].x-t[0].x)*(t[2].y-t[0].y)-(t[2].x-t[0].x)*(t[1].y-t[0].y))*.5f;sum+=l.centre(k)*a;p.area+=a;++p.cells;
   for(const auto& v:t){p.low={std::min(p.low.x,v.x),std::min(p.low.y,v.y)};p.high={std::max(p.high.x,v.x),std::max(p.high.y,v.y)};}
   if(p.cells>=6||std::max(p.high.x-p.low.x,p.high.y-p.low.y)>.3f)continue;
   for(int n:nb[size_t(k)])if(n>=0&&fresh(n)){taken[size_t(n)]=1;open.push_back(n);}}
  p.centre=p.area>0?sum*(1.f/p.area):l.centre(c);p.blown=length(p.centre-Vec2{hit.u,hit.v})<std::max(.06f,hit.radius*1.3f);out.push_back(p);}
 return out;}

// A whole pane coming down: which of fracture()'s pieces fall as shard props (1), which the
// renderer animates and leaves lying (0), and which were gone before it fell (2). The big
// outer pieces, at most `most`, are the props; both sides of the game ask this, so they agree.
inline std::vector<std::uint8_t> loosePieces(const std::vector<Shard>& shards,const Lattice* l=nullptr,const std::vector<std::uint8_t>* gone=nullptr,int most=16){
 std::vector<std::uint8_t> out(shards.size(),0);int taken=0;
 for(size_t i=0;i<shards.size();++i){const Vec2 c=shards[i].shape.centroid();
  if(l&&gone&&gone->size()==size_t(l->cells())){const int cell=l->at(c);if(cell>=0&&(*gone)[size_t(cell)]){out[i]=2;continue;}}
  if(taken<most&&shards[i].ring>=2&&shards[i].shape.area()>=.012f){out[i]=1;++taken;}}
 return out;}

// A glass shard prop's outline in its own unit square (x, y in -0.5..0.5), by its piece
// number: a sliver, a wedge, a blade or a broken quad, so a floor of them is not one shape.
inline int shardOutline(int piece,std::array<Vec2,5>& p){
 switch(piece&7){
  case 0:p={Vec2{-.5f,-.5f},{.5f,-.32f},{-.08f,.5f}};return 3;
  case 1:p={Vec2{-.5f,-.2f},{.5f,-.5f},{.38f,.5f},{-.3f,.35f}};return 4;
  case 2:p={Vec2{-.5f,.5f},{-.22f,-.5f},{.5f,-.1f}};return 3;
  case 3:p={Vec2{-.45f,-.5f},{.5f,-.42f},{.3f,.12f},{-.12f,.5f},{-.5f,.2f}};return 5;
  case 4:p={Vec2{-.5f,-.5f},{.5f,.5f},{-.35f,.3f}};return 3;
  case 5:p={Vec2{-.5f,-.35f},{.15f,-.5f},{.5f,.4f},{-.25f,.5f}};return 4;
  case 6:p={Vec2{-.3f,-.5f},{.5f,-.05f},{-.5f,.5f}};return 3;
  default:p={Vec2{-.5f,-.5f},{.45f,-.5f},{.5f,.3f},{-.1f,.5f},{-.5f,0.f}};return 5;}}
}

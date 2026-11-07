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
}

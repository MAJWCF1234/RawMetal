// Props through Vectra's queries (PropQueries.h).
#include "PropQueries.h"
#include <algorithm>
#include <cmath>
namespace retro::vectra {
int propBodies(const Clutter& c,std::array<Body,3>& out){
 if(c.model<0)return 0;
 if(!chained(c)){Body& b=out[0];b=Body{};const Shape& s=propShape(c);b.shape=&s;b.q=propOrientation(c);b.x=propCentre(c)+b.q.rotate(s.com);return 1;}
 // A chained pad: its three segments as the solver has them (RigidClutter.cpp's buildChain).
 const Chain ch=chainOf(c);const V3 ax=ch.along==0?V3{1,0,0}:V3{0,1,0};const float third=ch.length/3;
 for(int k=0;k<3;++k){Body& b=out[size_t(k)];b=Body{};b.shape=&segmentShape(c);b.q=ch.turn[size_t(k)];b.x=ch.frame[size_t(k)]+b.q.rotate(ax*(float(k-1)*third));}
 return 3;}
void propBounds(const Clutter& c,V3& centre,float& radius){
 const auto s=c.size();centre=propCentre(c);const float half=.5f*std::sqrt(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]);
 // A chained pad bends about its joints: its ends stay within its length of the middle.
 radius=(chained(c)?2*half:half)*1.05f+.01f;}
namespace {
// Over the prop's bodies, the nearest hit, after the sphere round it.
template<class Cast> bool castProp(const Clutter& c,V3 origin,V3 dir,float maxD,float reach,QueryHit& hit,Cast&& cast){
 if(c.model<0)return false;V3 centre;float radius;propBounds(c,centre,radius);radius+=reach;
 const V3 m=centre-origin;const float t=std::clamp(dot(m,dir),0.f,maxD);if(lenSq(m-dir*t)>radius*radius)return false;
 std::array<Body,3> bodies;const int n=propBodies(c,bodies);bool any=false;float best=maxD;
 for(int k=0;k<n;++k){QueryHit h;if(!cast(bodies[size_t(k)],best,h))continue;if(!any||h.distance<hit.distance){hit=h;hit.body=k;any=true;best=h.distance;}}
 return any;}
}
bool rayCastProp(const Clutter& c,V3 origin,V3 dir,float maxD,QueryHit& hit){
 return castProp(c,origin,dir,maxD,0,hit,[&](const Body& b,float best,QueryHit& h){return rayCast(b,origin,dir,best,h);});}
bool sphereCastProp(const Clutter& c,V3 origin,float radius,V3 dir,float maxD,QueryHit& hit){
 return castProp(c,origin,dir,maxD,radius,hit,[&](const Body& b,float best,QueryHit& h){return sphereCast(b,origin,radius,dir,best,h);});}
bool boxCastProp(const Clutter& c,V3 origin,V3 half,Quat turn,V3 dir,float maxD,QueryHit& hit){
 return castProp(c,origin,dir,maxD,len(half),hit,[&](const Body& b,float best,QueryHit& h){return boxCast(b,origin,half,turn,dir,best,h);});}
bool overlapSphereProp(const Clutter& c,V3 centre,float radius){
 if(c.model<0)return false;V3 middle;float reach;propBounds(c,middle,reach);if(lenSq(middle-centre)>(reach+radius)*(reach+radius))return false;
 std::array<Body,3> bodies;const int n=propBodies(c,bodies);for(int k=0;k<n;++k)if(overlapSphere(bodies[size_t(k)],centre,radius))return true;return false;}
}

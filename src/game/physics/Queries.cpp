// Vectra's queries (Queries.h): rays exactly against each part, shape casts and times of impact by
// conservative advancement on a GJK distance, and the continuous collision Space::continuous runs.
#include "Queries.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace retro::vectra {
namespace {
// ---- Support-mapped convex shapes ----------------------------------------------------------------
// A point (a sphere's centre, with its radius as a margin), a segment (a capsule's, half length h.z along its own z,
// its radius the margin), an oriented box, an upright cylinder or a convex hull in its own frame. GJK works on the
// core; the margin is taken off its distance.
struct Convex {
 enum Kind:std::uint8_t {Point,Box,Cylinder,Segment,Hull} kind=Point;
 V3 c{};Quat q{};V3 h{};float r=0;const ConvexHull* hull=nullptr;
 V3 support(V3 d)const{
  if(kind==Point)return c;
  const V3 l=q.unrotate(d);
  if(kind==Segment)return c+q.rotate({0,0,l.z<0?-h.z:h.z});
  if(kind==Hull){int best=0;float far=-1e30f;for(int i=0;i<hull->nv;++i){const float s=dot(hull->v[i],l);if(s>far){far=s;best=i;}}return c+q.rotate(hull->v[best]);}
  if(kind==Box)return c+q.rotate({l.x<0?-h.x:h.x,l.y<0?-h.y:h.y,l.z<0?-h.z:h.z});
  const float rho=std::sqrt(l.x*l.x+l.y*l.y);V3 s=rho>1e-9f?V3{l.x/rho*h.x,l.y/rho*h.x,0}:V3{};s.z=l.z<0?-h.z:h.z;return c+q.rotate(s);}
};
void frameOf(const Body& b,int part,V3& centre,Quat& turn){const auto& p=b.shape->parts[size_t(part)];centre=b.x+b.q.rotate(p.centre-b.shape->com);turn=b.q*p.turn;}
Convex convexOf(const Body& b,int part){const auto& p=b.shape->parts[size_t(part)];Convex k;frameOf(b,part,k.c,k.q);
 if(p.kind==PartKind::Sphere){k.kind=Convex::Point;k.r=p.half.x;}
 else if(p.kind==PartKind::Capsule){k.kind=Convex::Segment;k.r=p.half.x;k.h={0,0,p.half.z};}
 else if(p.kind==PartKind::Hull&&p.hull){k.kind=Convex::Hull;k.hull=p.hull;k.h=p.half;}
 else{k.kind=p.kind==PartKind::Cylinder?Convex::Cylinder:Convex::Box;k.h=p.half;}return k;}
float reachOf(const Convex& k){switch(k.kind){case Convex::Point:return k.r;case Convex::Box:return len(k.h);case Convex::Segment:return k.h.z+k.r;case Convex::Hull:return k.hull->reach;default:return std::sqrt(k.h.x*k.h.x+k.h.z*k.h.z);}}

// ---- GJK distance ---------------------------------------------------------------------------------
struct Vert {V3 w,a,b;};   // w = a - b: a point of the Minkowski difference and the two points it came from
struct Gjk {float distance=0;V3 a{},b{},normal{0,0,1};bool overlap=false;};   // core distance, nearest points, unit normal from A to B
// The nearest point of triangle abc to the origin as weights on a, b, c (Ericson's regions).
void closestTriangle(V3 a,V3 b,V3 c,float& u,float& v,float& w){
 const V3 ab=b-a,ac=c-a;const float d1=-dot(ab,a),d2=-dot(ac,a);
 if(d1<=0&&d2<=0){u=1;v=w=0;return;}
 const float d3=-dot(ab,b),d4=-dot(ac,b);
 if(d3>=0&&d4<=d3){v=1;u=w=0;return;}
 const float vc=d1*d4-d3*d2;
 if(vc<=0&&d1>=0&&d3<=0){const float t=d1/(d1-d3);u=1-t;v=t;w=0;return;}
 const float d5=-dot(ab,c),d6=-dot(ac,c);
 if(d6>=0&&d5<=d6){w=1;u=v=0;return;}
 const float vb=d5*d2-d1*d6;
 if(vb<=0&&d2>=0&&d6<=0){const float t=d2/(d2-d6);u=1-t;w=t;v=0;return;}
 const float va=d3*d6-d5*d4;
 if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){const float t=(d4-d3)/((d4-d3)+(d5-d6));v=1-t;w=t;u=0;return;}
 const float sum=va+vb+vc;
 if(std::fabs(sum)<1e-30f){ // degenerate (a sliver): the best of its edges
  auto seg=[](V3 p,V3 q,float& s,float& t){const V3 d=q-p;const float l=lenSq(d);t=l>1e-30f?std::clamp(-dot(p,d)/l,0.f,1.f):0.f;s=1-t;return lenSq(p+d*t);};
  float s1,t1,s2,t2,s3,t3;const float e1=seg(a,b,s1,t1),e2=seg(a,c,s2,t2),e3=seg(b,c,s3,t3);
  if(e1<=e2&&e1<=e3){u=s1;v=t1;w=0;}else if(e2<=e3){u=s2;w=t2;v=0;}else{v=s3;w=t3;u=0;}return;}
 const float den=1/sum;v=vb*den;w=vc*den;u=1-v-w;}
// Keep the simplex's vertices with weight; v the weighted point.
void keep(Vert* s,float* bc,int& n){int k=0;for(int i=0;i<n;++i)if(bc[i]>0){s[k]=s[i];bc[k]=bc[i];++k;}if(k==0){k=1;bc[0]=1;}n=k;}
// The nearest point of the simplex to the origin; reduces it to the feature holding that point. False when
// the origin is inside a tetrahedron.
bool solve(Vert* s,float* bc,int& n,V3& v){
 if(n==1){bc[0]=1;}
 else if(n==2){const V3 a=s[0].w,ab=s[1].w-a;const float l=lenSq(ab);const float t=l>1e-30f?std::clamp(-dot(a,ab)/l,0.f,1.f):0.f;bc[0]=1-t;bc[1]=t;keep(s,bc,n);}
 else if(n==3){closestTriangle(s[0].w,s[1].w,s[2].w,bc[0],bc[1],bc[2]);keep(s,bc,n);}
 else{
  static constexpr int faces[4][4]{{0,1,2,3},{0,1,3,2},{0,2,3,1},{1,2,3,0}};
  float best=1e30f,w[4]{0,0,0,0};bool outside=false;
  for(const auto& f:faces){const V3 a=s[f[0]].w,b=s[f[1]].w,c=s[f[2]].w,d=s[f[3]].w;const V3 nrm=cross(b-a,c-a);
   const float so=-dot(nrm,a),sd=dot(nrm,d-a);
   if(!(so*sd<0||std::fabs(sd)<1e-20f))continue;   // the origin is on the tetrahedron's side of this face
   outside=true;float u,vv,ww;closestTriangle(a,b,c,u,vv,ww);const float dd=lenSq(a*u+b*vv+c*ww);
   if(dd<best){best=dd;w[0]=w[1]=w[2]=w[3]=0;w[f[0]]=u;w[f[1]]=vv;w[f[2]]=ww;}}
  if(!outside)return false;
  for(int i=0;i<4;++i)bc[i]=w[i];keep(s,bc,n);}
 v={};for(int i=0;i<n;++i)v=v+s[i].w*bc[i];return true;}
Gjk gjk(const Convex& A,const Convex& B){
 Gjk out;Vert s[4];float bc[4]{1,0,0,0};int n=1;
 V3 d=A.c-B.c;if(lenSq(d)<1e-12f)d={1,0,0};
 auto vert=[&](V3 dir){Vert x;x.a=A.support(neg(dir));x.b=B.support(dir);x.w=x.a-x.b;return x;};
 s[0]=vert(d);V3 v=s[0].w;
 for(int it=0;it<48;++it){
  const float vv=lenSq(v);if(vv<1e-14f){out.overlap=true;break;}
  const Vert x=vert(v);
  if(vv-dot(v,x.w)<=1e-6f*vv+1e-12f)break;   // nothing of A-B lies nearer along v
  bool seen=false;for(int k=0;k<n;++k)seen|=lenSq(s[k].w-x.w)<1e-14f;if(seen)break;
  s[n++]=x;if(!solve(s,bc,n,v)){out.overlap=true;break;}
  if(lenSq(v)>=vv*(1-1e-7f))break;              // no progress (rounding)
 }
 V3 pa{},pb{};for(int i=0;i<n;++i){pa=pa+s[i].a*bc[i];pb=pb+s[i].b*bc[i];}
 out.a=pa;out.b=pb;out.distance=out.overlap?0.f:len(v);
 out.normal=out.distance>1e-7f?neg(v)*(1/out.distance):norm(B.c-A.c);
 return out;}
// With the margins: the signed distance between the surfaces, and their nearest points.
float distanceOf(const Convex& A,const Convex& B,V3& pa,V3& pb,V3& normal,bool& overlap){
 const Gjk g=gjk(A,B);normal=g.normal;
 if(g.overlap){overlap=true;pa=g.a;pb=g.b;return 0;}
 const float d=g.distance-A.r-B.r;overlap=d<=0;pa=g.a+g.normal*A.r;pb=g.b-g.normal*B.r;return d;}

// ---- Rays against a part (exact) ------------------------------------------------------------------
bool rayConvex(const Convex& k,V3 origin,V3 dir,float maxD,float& t,V3& normal,bool& inside){
 inside=false;
 if(k.kind==Convex::Point){const float r=k.r;const V3 m=origin-k.c;const float c=dot(m,m)-r*r;
  if(c<=0){inside=true;t=0;normal=neg(dir);return true;}
  const float b=dot(m,dir);if(b>0)return false;const float disc=b*b-c;if(disc<0)return false;
  t=-b-std::sqrt(disc);if(t>maxD)return false;normal=norm(m+dir*t);return true;}
 const V3 o=k.q.unrotate(origin-k.c),d=k.q.unrotate(dir);
 if(k.kind==Convex::Segment){   // a capsule: its round side between the ends, and a ball at each end
  const float r=k.r,hz=k.h.z;const float z=std::clamp(o.z,-hz,hz);const V3 off{o.x,o.y,o.z-z};
  if(lenSq(off)<=r*r){inside=true;t=0;normal=neg(dir);return true;}
  float best=1e30f;V3 n{};const float A=d.x*d.x+d.y*d.y;
  if(A>1e-12f){const float B=o.x*d.x+o.y*d.y,C=o.x*o.x+o.y*o.y-r*r,disc=B*B-A*C;
   if(disc>=0){const float s=(-B-std::sqrt(disc))/A;if(s>=0&&std::fabs(o.z+d.z*s)<=hz){best=s;const V3 p=o+d*s;n={p.x/r,p.y/r,0};}}}
  for(float side:{-1.f,1.f}){const V3 m=o-V3{0,0,side*hz};const float b=dot(m,d),c=dot(m,m)-r*r;if(c>0&&b>0)continue;const float disc=b*b-c;if(disc<0)continue;
   const float s=-b-std::sqrt(disc);if(s<0||s>=best)continue;best=s;n=norm(m+d*s);}
  if(best>maxD)return false;t=best;normal=k.q.rotate(n);return true;}
 if(k.kind==Convex::Hull){   // its faces' planes: the last way in before the first way out
  float t0=0,t1=maxD;int face=-1;const auto& H=*k.hull;bool in=true;
  for(int f=0;f<H.nf;++f){const V3 n=H.f[f].normal;const float a=dot(n,o)-H.f[f].d,b=dot(n,d);if(a>0)in=false;
   if(std::fabs(b)<1e-12f){if(a>0)return false;continue;}const float s=-a/b;if(b<0){if(s>t0){t0=s;face=f;}}else t1=std::min(t1,s);if(t0>t1)return false;}
  if(in){inside=true;t=0;normal=neg(dir);return true;}
  if(face<0)return false;t=t0;normal=k.q.rotate(H.f[face].normal);return true;}
 if(k.kind==Convex::Box){float t0=-1e30f,t1=1e30f;int axis=-1;float sign=0;
  for(int i=0;i<3;++i){const float oi=comp(o,i),di=comp(d,i),hi=comp(k.h,i);
   if(std::fabs(di)<1e-12f){if(std::fabs(oi)>hi)return false;continue;}
   float a=(-hi-oi)/di,b=(hi-oi)/di,s=-1;if(a>b){std::swap(a,b);s=1;}
   if(a>t0){t0=a;axis=i;sign=s;}t1=std::min(t1,b);if(t0>t1)return false;}
  if(t1<0||t0>maxD)return false;
  if(t0<0||axis<0){inside=true;t=0;normal=neg(dir);return true;}
  t=t0;V3 n{};if(axis==0)n.x=sign;else if(axis==1)n.y=sign;else n.z=sign;normal=k.q.rotate(n);return true;}
 // A cylinder about its own z.
 const float r=k.h.x,hz=k.h.z;
 if(o.x*o.x+o.y*o.y<=r*r&&std::fabs(o.z)<=hz){inside=true;t=0;normal=neg(dir);return true;}
 float best=1e30f;V3 n{};
 const float A=d.x*d.x+d.y*d.y;
 if(A>1e-12f){const float B=o.x*d.x+o.y*d.y,C=o.x*o.x+o.y*o.y-r*r,disc=B*B-A*C;
  if(disc>=0){const float s=(-B-std::sqrt(disc))/A;if(s>=0&&std::fabs(o.z+d.z*s)<=hz){best=s;const V3 p=o+d*s;n={p.x/r,p.y/r,0};}}}
 if(std::fabs(d.z)>1e-12f)for(float side:{-1.f,1.f}){const float s=(side*hz-o.z)/d.z;if(s<0||s>=best)continue;const V3 p=o+d*s;if(p.x*p.x+p.y*p.y<=r*r){best=s;n={0,0,side};}}
 if(best>maxD)return false;t=best;normal=k.q.rotate(n);return true;}

// ---- Linear casts by conservative advancement ------------------------------------------------------
constexpr float CastTolerance=2e-4f;
bool castConvex(Convex A,const Convex& B,V3 dir,float maxD,QueryHit& hit){
 const V3 start=A.c;float t=0;V3 n=dir;
 for(int it=0;it<castIterations;++it){
  A.c=start+dir*t;V3 pa,pb,nn;bool overlap;float d=distanceOf(A,B,pa,pb,nn,overlap);
  if(overlap){if(it==0){hit.distance=0;hit.point=A.c;hit.normal=neg(dir);hit.startedInside=true;return true;}d=0;nn=n;}
  if(d<=CastTolerance){hit.distance=t;hit.normal=neg(nn);hit.point=pb;hit.startedInside=false;return true;}
  n=nn;const float closing=dot(dir,n);if(closing<=1e-6f)return false;
  t+=d/closing;if(t>maxD)return false;}
 // Out of iterations without closing in (a grazing cast): a hit only if it is within reach where it got to.
 A.c=start+dir*t;V3 pa,pb,nn;bool overlap;const float d=distanceOf(A,B,pa,pb,nn,overlap);
 if(!overlap&&d>CastTolerance)return false;
 hit.distance=t;hit.normal=neg(overlap?n:nn);hit.point=pb;hit.startedInside=false;return true;}
Convex pointShape(V3 c,float r){Convex k;k.kind=Convex::Point;k.c=c;k.r=r;return k;}
Convex boxShape(V3 c,V3 h,Quat q){Convex k;k.kind=Convex::Box;k.c=c;k.h=h;k.q=q;return k;}
// Whether a cast of a shape of reach `radius` along the ray can come within reach of a sphere (centre c, radius r).
bool sphereNear(V3 origin,V3 dir,float maxD,V3 c,float r){const V3 m=c-origin;const float t=std::clamp(dot(m,dir),0.f,maxD);return lenSq(m-dir*t)<=r*r;}
template<class Each> bool castBody(const Body& b,float radius,V3 origin,V3 dir,float maxD,QueryHit& hit,Each&& each){
 if(!b.shape||!sphereNear(origin,dir,maxD,b.x,b.shape->bound+radius+1e-3f))return false;
 bool any=false;float best=maxD;
 for(int p=0;p<b.shape->count;++p){const Convex k=convexOf(b,p);if(!sphereNear(origin,dir,best,k.c,reachOf(k)+radius+1e-3f))continue;
  QueryHit h;if(!each(k,best,h))continue;if(!any||h.distance<hit.distance){hit=h;hit.part=p;any=true;best=h.distance;}}
 return any;}
template<class One> bool castList(const std::vector<Body>& bodies,const QueryFilter& filter,float maxD,QueryHit& hit,One&& one){
 bool any=false;float best=maxD;
 for(int i=0;i<int(bodies.size());++i){if(filter&&!filter(i))continue;QueryHit h;if(!one(bodies[size_t(i)],best,h))continue;if(!any||h.distance<hit.distance){hit=h;hit.body=i;any=true;best=h.distance;}}
 return any;}
}

// ---- One body ----
bool rayCastPart(const Body& b,int part,V3 origin,V3 dir,float maxD,QueryHit& hit){
 const Convex k=convexOf(b,part);float t;V3 n;bool inside;if(!rayConvex(k,origin,dir,maxD,t,n,inside))return false;
 hit.distance=t;hit.point=origin+dir*t;hit.normal=n;hit.part=part;hit.startedInside=inside;return true;}
bool rayCast(const Body& b,V3 origin,V3 dir,float maxD,QueryHit& hit){
 return castBody(b,0,origin,dir,maxD,hit,[&](const Convex& k,float best,QueryHit& h){float t;V3 n;bool inside;if(!rayConvex(k,origin,dir,best,t,n,inside))return false;h.distance=t;h.point=origin+dir*t;h.normal=n;h.startedInside=inside;return true;});}
bool sphereCast(const Body& b,V3 origin,float radius,V3 dir,float maxD,QueryHit& hit){
 return castBody(b,radius,origin,dir,maxD,hit,[&](const Convex& k,float best,QueryHit& h){
  if(k.kind==Convex::Point){Convex big=k;big.r+=radius;float t;V3 n;bool inside;if(!rayConvex(big,origin,dir,best,t,n,inside))return false;
   h.distance=t;h.normal=n;h.point=inside?origin:k.c+n*k.r;h.startedInside=inside;return true;}
  return castConvex(pointShape(origin,radius),k,dir,best,h);});}
bool boxCast(const Body& b,V3 origin,V3 half,Quat turn,V3 dir,float maxD,QueryHit& hit){
 return castBody(b,len(half),origin,dir,maxD,hit,[&](const Convex& k,float best,QueryHit& h){return castConvex(boxShape(origin,half,turn),k,dir,best,h);});}
namespace {
bool overlapBody(const Body& b,const Convex& probe,int* part){
 if(!b.shape||lenSq(probe.c-b.x)>(b.shape->bound+reachOf(probe))*(b.shape->bound+reachOf(probe)))return false;
 for(int p=0;p<b.shape->count;++p){const Convex k=convexOf(b,p);const float r=reachOf(k)+reachOf(probe);if(lenSq(k.c-probe.c)>r*r)continue;
  V3 pa,pb,n;bool overlap;distanceOf(probe,k,pa,pb,n,overlap);if(overlap){if(part)*part=p;return true;}}
 return false;}
}
bool overlapSphere(const Body& b,V3 centre,float radius,int* part){return overlapBody(b,pointShape(centre,radius),part);}
bool overlapBox(const Body& b,V3 centre,V3 half,Quat turn,int* part){return overlapBody(b,boxShape(centre,half,turn),part);}

// ---- Many bodies ----
bool rayCast(const std::vector<Body>& bodies,V3 origin,V3 dir,float maxD,QueryHit& hit,const QueryFilter& filter){
 return castList(bodies,filter,maxD,hit,[&](const Body& b,float best,QueryHit& h){return rayCast(b,origin,dir,best,h);});}
bool sphereCast(const std::vector<Body>& bodies,V3 origin,float radius,V3 dir,float maxD,QueryHit& hit,const QueryFilter& filter){
 return castList(bodies,filter,maxD,hit,[&](const Body& b,float best,QueryHit& h){return sphereCast(b,origin,radius,dir,best,h);});}
bool boxCast(const std::vector<Body>& bodies,V3 origin,V3 half,Quat turn,V3 dir,float maxD,QueryHit& hit,const QueryFilter& filter){
 return castList(bodies,filter,maxD,hit,[&](const Body& b,float best,QueryHit& h){return boxCast(b,origin,half,turn,dir,best,h);});}
int overlapSphere(const std::vector<Body>& bodies,V3 centre,float radius,std::vector<int>& out,const QueryFilter& filter){
 const size_t before=out.size();for(int i=0;i<int(bodies.size());++i)if((!filter||filter(i))&&overlapSphere(bodies[size_t(i)],centre,radius))out.push_back(i);return int(out.size()-before);}
int overlapBox(const std::vector<Body>& bodies,V3 centre,V3 half,Quat turn,std::vector<int>& out,const QueryFilter& filter){
 const size_t before=out.size();for(int i=0;i<int(bodies.size());++i)if((!filter||filter(i))&&overlapBox(bodies[size_t(i)],centre,half,turn))out.push_back(i);return int(out.size()-before);}
int rayCastAll(const std::vector<Body>& bodies,V3 origin,V3 dir,float maxD,std::vector<QueryHit>& out,const QueryFilter& filter){
 const size_t before=out.size();
 for(int i=0;i<int(bodies.size());++i){if(filter&&!filter(i))continue;QueryHit h;if(rayCast(bodies[size_t(i)],origin,dir,maxD,h)){h.body=i;out.push_back(h);}}
 std::stable_sort(out.begin()+std::ptrdiff_t(before),out.end(),[](const QueryHit& a,const QueryHit& b){return a.distance<b.distance;});return int(out.size()-before);}
bool rayCast(const Space& space,V3 origin,V3 dir,float maxD,QueryHit& hit,const QueryFilter& filter){
 const V3 inv{std::fabs(dir.x)>1e-12f?1/dir.x:1e30f,std::fabs(dir.y)>1e-12f?1/dir.y:1e30f,std::fabs(dir.z)>1e-12f?1/dir.z:1e30f};
 return castList(space.bodies,filter,maxD,hit,[&](const Body& b,float best,QueryHit& h){
  // The box its last step found, at the step's start: carried along by its motion since (dp) and grown by
  // how far its turn since (dq) can swing it (empty before the first step: then the bounding sphere alone).
  if(b.boundsHi.x>b.boundsLo.x){const float turned=2*std::acos(std::clamp(std::fabs(b.dq.w),0.f,1.f)),grow=turned*b.shape->bound+1e-3f;
   float t0=0,t1=best;for(int a=0;a<3;++a){const float shift=comp(b.dp,a);float lo=(std::min(comp(b.boundsLo,a),comp(b.boundsLo,a)+shift)-grow-comp(origin,a))*comp(inv,a),hi=(std::max(comp(b.boundsHi,a),comp(b.boundsHi,a)+shift)+grow-comp(origin,a))*comp(inv,a);if(lo>hi)std::swap(lo,hi);t0=std::max(t0,lo);t1=std::min(t1,hi);}
   if(t0>t1)return false;}
  return rayCast(b,origin,dir,best,h);});}

// ---- Distance and time of impact ----
float partDistance(const Body& a,int pa,const Body& b,int pb,V3& pointA,V3& pointB,bool* overlap){
 V3 n;bool o;const float d=distanceOf(convexOf(a,pa),convexOf(b,pb),pointA,pointB,n,o);if(overlap)*overlap=o;return d;}
float bodyDistance(const Body& a,const Body& b,V3& pointA,V3& pointB,V3& normal,bool* overlap){
 float best=1e30f;bool any=false,touching=false;
 for(int i=0;i<a.shape->count;++i){const Convex A=convexOf(a,i);const float ra=reachOf(A);
  for(int j=0;j<b.shape->count;++j){const Convex B=convexOf(b,j);if(len(B.c-A.c)-ra-reachOf(B)>best)continue;
   V3 p,q,n;bool o;const float d=distanceOf(A,B,p,q,n,o);if(!any||d<best){best=d;pointA=p;pointB=q;normal=n;any=true;touching=o;}}}
 if(overlap)*overlap=touching;return any?best:1e30f;}
void Motion::begin(){Quat d=q1*q0.conj();if(d.w<0)d={-d.w,-d.x,-d.y,-d.z};const float s=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
 angle=2*std::atan2(s,d.w);axis=s>1e-9f?V3{d.x/s,d.y/s,d.z/s}:V3{0,0,1};if(s<=1e-9f)angle=0;}
void Motion::at(float t,V3& x,Quat& q)const{x=x0+(x1-x0)*t;q=angle>0?(Quat::axisAngle(axis,angle*t)*q0).normalized():q0;}
bool timeOfImpact(const Body& a,const Motion& ma,const Body& b,const Motion& mb,float target,float& t,V3& point,V3& normal){
 const float ra=a.shape->bound,rb=b.shape->bound;const V3 la=ma.x1-ma.x0,lb=mb.x1-mb.x0;
 // The swept spheres first: their centres' nearest approach over the step.
 {const V3 m=mb.x0-ma.x0,d=lb-la;const float l=lenSq(d);const float s=l>1e-12f?std::clamp(-dot(m,d)/l,0.f,1.f):0.f;const float r=ra+rb+target;if(lenSq(m+d*s)>r*r)return false;}
 Body A=a,B=b;float at=0;V3 n{};float d=0;V3 pa{},pb{};
 for(int it=0;it<64;++it){
  ma.at(at,A.x,A.q);mb.at(at,B.x,B.q);bool overlap;d=bodyDistance(A,B,pa,pb,n,&overlap);
  if(it==0&&d<=target)return false;            // touching from the start: the contacts' business
  if(overlap)d=0;
  if(d<=target)break;
  const float closing=dot(la-lb,n)+ma.angle*ra+mb.angle*rb;if(closing<=1e-7f)return false;
  at+=(d-target*.5f)/closing;if(at>1)return false;}
 t=at;point=(pa+pb)*.5f;normal=n;return true;}
void edgeProbes(const Body& b,int part,float spacing,std::vector<V3>& out){
 const auto& p=b.shape->parts[size_t(part)];const V3 base=p.centre-b.shape->com;auto put=[&](V3 l){out.push_back(base+p.turn.rotate(l));};
 if(p.kind==PartKind::Box){const V3 h=p.half;
  for(int i=0;i<8;++i)put({i&1?h.x:-h.x,i&2?h.y:-h.y,i&4?h.z:-h.z});
  for(int axis=0;axis<3;++axis){const float L=2*comp(h,axis);const int n=int(std::ceil(L/spacing));if(n<2)continue;
   for(int e=0;e<4;++e){const float s1=e&1?1.f:-1.f,s2=e&2?1.f:-1.f;
    for(int k=1;k<n;++k){const float a=-comp(h,axis)+L*float(k)/float(n);V3 l=axis==0?V3{a,s1*h.y,s2*h.z}:axis==1?V3{s1*h.x,a,s2*h.z}:V3{s1*h.x,s2*h.y,a};put(l);}}}
  return;}
 if(p.kind==PartKind::Cylinder){const float r=p.half.x,hz=p.half.z;const int around=std::clamp(int(std::ceil(2*kPi*r/spacing)),8,32),along=std::max(1,int(std::ceil(2*hz/spacing)));
  for(int i=0;i<around;++i){const float a=2*kPi*float(i)/float(around);put({std::cos(a)*r,std::sin(a)*r,-hz});put({std::cos(a)*r,std::sin(a)*r,hz});}
  for(int i=0;i<8;++i){const float a=kPi/4*float(i);for(int k=1;k<along;++k)put({std::cos(a)*r,std::sin(a)*r,-hz+2*hz*float(k)/float(along)});}
  put({0,0,-hz});put({0,0,hz});return;}
 if(p.kind==PartKind::Capsule){const float r=p.half.x,hz=p.half.z;const int around=std::clamp(int(std::ceil(2*kPi*r/spacing)),8,32),along=std::max(1,int(std::ceil(2*hz/spacing)));
  for(int i=0;i<around;++i){const float a=2*kPi*float(i)/float(around);for(int k=0;k<=along;++k)put({std::cos(a)*r,std::sin(a)*r,-hz+2*hz*float(k)/float(along)});
   put({std::cos(a)*r*.7071f,std::sin(a)*r*.7071f,-hz-r*.7071f});put({std::cos(a)*r*.7071f,std::sin(a)*r*.7071f,hz+r*.7071f});}
  put({0,0,-hz-r});put({0,0,hz+r});return;}
 if(p.kind==PartKind::Hull&&p.hull){const auto& t=*p.hull;for(int i=0;i<t.nv;++i)put(t.v[i]);
  for(int e=0;e<t.ne;++e){const V3 a=t.v[t.e[e][0]],c=t.v[t.e[e][1]];const int n=int(std::ceil(len(c-a)/spacing));for(int k=1;k<n;++k)put(a+(c-a)*(float(k)/float(n)));}
  return;}
 const float r=p.half.x;for(int i=-1;i<=1;++i)for(int j=-1;j<=1;++j)for(int k=-1;k<=1;++k)if(i||j||k)put(norm(V3{float(i),float(j),float(k)})*r);}

// ---- Continuous collision (Space::continuous) -------------------------------------------------------
namespace {
constexpr float ProbeSpacing=.025f;  // edge probes this far apart: a 3 cm post cannot slip between two
constexpr float TunnelDepth=.02f;    // a body ending this deep in what it struck is past what its contacts push out
constexpr float SubTurn=.15f;        // a turn swept in pieces of at most this angle (a 0.6 m reach strays 1.7 mm from its arc)
constexpr float ArcStray=.002f;      // how far a probe's piece of chord may stray from its arc (sets its own pieces)
// What a fast body met first this step.
struct Event {float t=1;int kind=0;int other=-1;V3 point{},normal{};};   // kind 1: the world at a probe (point: in the body's frame), 2: a body
// A probe's way through the step in pieces (the body's poses at each piece's ends): the first piece that
// strikes the world, if from there it went through, or ends deep in it.
struct ProbeHit {float t,window;V3 r,n;};
// A body's edges for its probes (body frame, from the centre of mass): its corners with the outward normals of the faces
// round them, and its edges between corners, straight or round a circle (a cylinder's rim, a capsule's ring), with their
// faces' normals (a round edge's taken at each point along it).
struct Corner {V3 p{};std::array<V3,3> n{};int count=0;};
struct Edge {int a=0,b=0;V3 n0{},n1{};int count=2;
 bool round=false;V3 centre{};Quat turn{};float radius=0,from=0,to=0,z=0,cap=0;   // round: the circle (part frame), its angles, its cap's side (+-1, 0 none)
 float length(const std::vector<Corner>& c)const{return round?radius*std::fabs(to-from):len(c[size_t(b)].p-c[size_t(a)].p);}
 // The point at t of the way along it, and its faces' normals there.
 V3 at(const std::vector<Corner>& c,float t,std::array<V3,3>& n,int& nn)const{
  if(!round){nn=count;n[0]=n0;n[1]=n1;return c[size_t(a)].p+(c[size_t(b)].p-c[size_t(a)].p)*t;}
  const float g=from+(to-from)*t,cg=std::cos(g),sg=std::sin(g);n[0]=turn.rotate({cg,sg,0});nn=1;if(cap!=0){n[1]=turn.rotate({0,0,cap});nn=2;}
  return centre+turn.rotate({cg*radius,sg*radius,z});}};
void edgesOf(const Body& b,int part,std::vector<Corner>& corners,std::vector<Edge>& edges){
 const auto& p=b.shape->parts[size_t(part)];const V3 base=p.centre-b.shape->com;
 auto at=[&](V3 l){return base+p.turn.rotate(l);};auto dir=[&](V3 l){return p.turn.rotate(l);};
 const int c0=int(corners.size());
 auto corner=[&](V3 l,std::initializer_list<V3> normals){Corner c;c.p=at(l);for(V3 n:normals)if(c.count<3)c.n[size_t(c.count++)]=dir(n);corners.push_back(c);return int(corners.size())-1;};
 auto line=[&](int a,int b2,V3 n0,V3 n1,int count){Edge e;e.a=a;e.b=b2;e.n0=dir(n0);e.n1=dir(n1);e.count=count;edges.push_back(e);};
 if(p.kind==PartKind::Box){const V3 h=p.half;
  for(int i=0;i<8;++i){const float sx=i&1?1.f:-1.f,sy=i&2?1.f:-1.f,sz=i&4?1.f:-1.f;corner({sx*h.x,sy*h.y,sz*h.z},{V3{sx,0,0},V3{0,sy,0},V3{0,0,sz}});}
  for(int axis=0;axis<3;++axis){const int bit=1<<axis;for(int i=0;i<8;++i){if(i&bit)continue;const int o1=(axis+1)%3,o2=(axis+2)%3;
    auto unit=[&](int ax,int corner2){V3 v{};const float s=corner2&(1<<ax)?1.f:-1.f;if(ax==0)v.x=s;else if(ax==1)v.y=s;else v.z=s;return v;};
    line(c0+i,c0+(i|bit),unit(o1,i),unit(o2,i),2);}}
  return;}
 if(p.kind==PartKind::Cylinder||p.kind==PartKind::Capsule){const float r=p.half.x,hz=p.half.z;const bool cyl=p.kind==PartKind::Cylinder;constexpr int N=8;
  int ring[2][N];
  for(int s2=0;s2<2;++s2){const float z=s2?hz:-hz,cap=s2?1.f:-1.f;
   for(int k=0;k<N;++k){const float g=2*kPi*float(k)/float(N),cg=std::cos(g),sg=std::sin(g);
    ring[s2][k]=cyl?corner({cg*r,sg*r,z},{V3{cg,sg,0},V3{0,0,cap}}):corner({cg*r,sg*r,z},{V3{cg,sg,0}});}
   for(int k=0;k<N;++k){Edge e;e.a=ring[s2][k];e.b=ring[s2][(k+1)%N];e.round=true;e.centre=base;e.turn=p.turn;e.radius=r;e.from=2*kPi*float(k)/float(N);e.to=2*kPi*float(k+1)/float(N);e.z=z;e.cap=cyl?cap:0.f;edges.push_back(e);}
   if(cyl)corner({0,0,z},{V3{0,0,cap}});
   else{for(int k=0;k<N;++k){const float g=2*kPi*float(k)/float(N);const V3 d=norm(V3{std::cos(g)*.7071f,std::sin(g)*.7071f,cap*.7071f});corner({d.x*r,d.y*r,z+d.z*r},{d});}corner({0,0,z+cap*r},{V3{0,0,cap}});}}
  for(int k=0;k<N;++k){const float g=2*kPi*float(k)/float(N);line(ring[0][k],ring[1][k],{std::cos(g),std::sin(g),0},{},1);}
  return;}
 if(p.kind==PartKind::Hull&&p.hull){const auto& t=*p.hull;
  for(int i=0;i<t.nv;++i){Corner c;c.p=at(t.v[i]);corners.push_back(c);}
  for(int e=0;e<t.ne;++e){Edge E;E.a=c0+t.e[e][0];E.b=c0+t.e[e][1];E.count=0;
   for(int side=0;side<2;++side){const int f=t.ef[e][side];if(f<0)continue;const V3 n=dir(t.f[f].normal);(E.count==0?E.n0:E.n1)=n;++E.count;
    for(int v:{t.e[e][0],t.e[e][1]}){auto& c=corners[size_t(c0+v)];bool have=false;for(int k=0;k<c.count;++k)have|=dot(c.n[size_t(k)],n)>.999f;if(!have&&c.count<3)c.n[size_t(c.count++)]=n;}}
   edges.push_back(E);}
  return;}
 const float r=p.half.x;for(int i=-1;i<=1;++i)for(int j=-1;j<=1;++j)for(int k=-1;k<=1;++k)if(i||j||k){const V3 d=norm(V3{float(i),float(j),float(k)});corner(d*r,{d});}}
// The probes a fast body is swept by: its corners, and along each edge its middle, and every `fine` metres where the point
// leads (it goes out of a face round it, not into the body) and goes further than the body is thick (a thin thing could
// pass it whole; a nearer one would end inside it, the contacts' business). Points that lead nowhere or hardly move are
// left out. Each probe's pieces of the turn: as few as keep its chord within ArcStray of its arc (a power of two, at most
// K). Returns how many sweeps they ask (the sum of their pieces); `fineCount` and `fineCost` the share of those from `fine`.
struct Probe {V3 r;int pieces;};
int gatherProbes(const std::vector<Corner>& corners,const std::vector<Edge>& edges,const Motion& m,int K,float thick,float fine,std::vector<Probe>& out,int& fineCount,int& fineCost){
 out.clear();fineCount=0;fineCost=0;int cost=0;const V3 lin=m.x1-m.x0,omega=m.axis*m.angle;Quat qm;V3 xm;m.at(.5f,xm,qm);
 auto pieces=[&](V3 r){const float rr=std::max(len(r),1e-3f),most=std::sqrt(8*ArcStray/rr);int k=1;while(k<K&&float(k)*most<m.angle)k*=2;return k;};
 auto consider=[&](V3 r,const std::array<V3,3>& n,int nn,bool isFine)->bool{
  const V3 start=m.x0+m.q0.rotate(r),end=m.x1+m.q1.rotate(r);const float travel=len(end-start);
  if(travel<TunnelDepth*.5f)return false;   // it hardly moved: its contacts hold it
  if(isFine&&travel<=thick)return false;
  // Leading: going out of one of its faces at the step's start, middle or end.
  bool lead=nn==0;
  const Quat* poses[3]={&m.q0,&qm,&m.q1};for(const Quat* q:poses){if(lead)break;const V3 arm=q->rotate(r),v=lin+cross(omega,arm);for(int k=0;k<nn&&!lead;++k)lead=dot(v,q->rotate(n[size_t(k)]))>0;}
  if(!lead)return false;
  const int p=pieces(r);out.push_back({r,p});cost+=p;if(isFine){++fineCount;fineCost+=p;}return true;};
 for(const auto& c:corners)consider(c.p,c.n,c.count,false);
 std::array<V3,3> n{};int nn=0;
 for(const auto& e:edges){const float L=e.length(corners);
  consider(e.at(corners,.5f,n,nn),n,nn,false);
  if(fine<=0)continue;const int segs=int(std::ceil(L/fine));if(segs<2)continue;
  for(int k=1;k<segs;++k){if(2*k==segs)continue;const V3 r=e.at(corners,float(k)/float(segs),n,nn);consider(r,n,nn,true);}}
 return cost;}
bool worldProbes(Space& s,int i,const Motion& m,float thick,int share,std::vector<Probe>& probes,std::vector<ProbeHit>& hits,Event& best){
 Body& b=s.bodies[size_t(i)];probes.clear();hits.clear();
 const int cap=s.settings.ccdProbes;
 int K=std::clamp(int(std::ceil(m.angle/SubTurn)),1,8);
 if(cap<=0){   // every 2.5 cm along every edge, each through every piece (as before the bound)
  static thread_local std::vector<V3> all;all.clear();for(int p=0;p<b.shape->count;++p)edgeProbes(b,p,ProbeSpacing,all);for(const V3& r:all)probes.push_back({r,K});}
 else{
  {int k=1;while(k<K)k*=2;K=k;}   // (pieces by halves: a probe's own are every second, fourth... of the body's)
  static thread_local std::vector<Corner> corners;static thread_local std::vector<Edge> edges;corners.clear();edges.clear();
  for(int p=0;p<b.shape->count;++p)edgesOf(b,p,corners,edges);
  // Along its leading edges every 2.5 cm, then wider to keep to `cap` probes, and to its share of the step's sweeps.
  float fine=ProbeSpacing;int fines=0,fineCost=0;int cost=gatherProbes(corners,edges,m,K,thick,fine,probes,fines,fineCost);
  for(int tries=0;tries<4&&fines>0;++tries){const int base=int(probes.size())-fines;
   float grow=1;if(int(probes.size())>cap)grow=std::max(grow,float(fines)/float(std::max(1,cap-base)));
   if(share>0&&cost>share)grow=std::max(grow,float(fineCost)/float(std::max(1,share-(cost-fineCost))));
   if(grow<=1.001f)break;fine*=grow*1.05f;if(fine>2.f)fine=0;cost=gatherProbes(corners,edges,m,K,thick,fine,probes,fines,fineCost);}}
 std::array<V3,9> xs{};std::array<Quat,9> qs{};
 for(int k=0;k<=K;++k)m.at(float(k)/float(K),xs[size_t(k)],qs[size_t(k)]);
 auto sweep=[&](V3 a,V3 c,V3& n){++s.ccdSweeps;return s.sweep(s,i,a,c,n);};
 float first=best.t;
 for(const Probe& pr:probes){const V3 r=pr.r;const int stride=K/std::max(1,pr.pieces);
  const V3 start=xs[0]+qs[0].rotate(r),end=xs[size_t(K)]+qs[size_t(K)].rotate(r),way=end-start;const float travel=len(way);
  if(travel<TunnelDepth*.5f)continue;                                      // it hardly moved: its contacts hold it
  // A trailing point (a leading one meets things first): behind the body's way, when its going outruns its turn's
  // sweep there. (Its own chord is no guide: turned past 60 degrees a step, every point's chord points back.)
  {const V3 lin=xs[size_t(K)]-xs[0];const float l=len(lin);if(m.angle*len(r)<l&&dot(qs[0].rotate(r),lin)<-.5f*len(r)*l)continue;}
  for(int k=0;k<K;k+=stride){if(float(k)/float(K)>first+.05f)break;
   const V3 a=xs[size_t(k)]+qs[size_t(k)].rotate(r),c=xs[size_t(k+stride)]+qs[size_t(k+stride)].rotate(r);V3 n;const float f=sweep(a,c,n);
   if(f>=1)continue;if(f<=0)break;                                         // it starts in something: the contacts' business
   const float t=(float(k)+f*float(stride))/float(K);
   // From where it ends back to just off where it struck: something between, it went through; nothing, it
   // ends in what it struck, and only deep is it ours.
   const V3 struck=a+(c-a)*f;V3 n2;const float back=sweep(end,struck+n*.002f,n2);
   const bool through=back>0&&back<.999f,deep=back<=0&&dot(struck-end,n)>TunnelDepth;
   if(through||deep){hits.push_back({t,.01f/travel,r,n});first=std::min(first,t);}
   break;}}
 if(first>=best.t)return false;
 // Every point that met the same face within a centimetre of its way of the first: the blow lands at their
 // middle (a plank's face striking a post along its height is struck at its middle, not at one corner).
 V3 normal{};for(const auto& h:hits)if(h.t==first){normal=h.n;break;}
 V3 sum{};int count=0;for(const auto& h:hits)if(h.t<=first+h.window&&dot(h.n,normal)>.7f){sum=sum+h.r;++count;}
 best={first,1,-1,sum*(1.f/float(std::max(1,count))),normal};return true;}
float effective(const Body& b,V3 arm,V3 n){if(b.invMass<=0)return 0;const V3 rn=cross(arm,n);return b.invMass+dot(rn,b.invInertia*rn);}
// How far any point of b went through the step as a's frame sees it (a standing still): its centre's way there,
// and its turn there times its reach.
float relativeSweep(const Motion& a,const Motion& b,float reachB){
 const V3 p0=a.q0.unrotate(b.x0-a.x0),p1=a.q1.unrotate(b.x1-a.x1);
 Quat d=(a.q1.conj()*b.q1)*(a.q0.conj()*b.q0).conj();if(d.w<0)d={-d.w,-d.x,-d.y,-d.z};
 const float s=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);return len(p1-p0)+2*std::atan2(s,d.w)*reachB;}
}
void continuousCollision(Space& s,float dt,const std::vector<V3>& start){
 (void)dt;s.ccdSweeps=0;const int n=int(s.bodies.size());if(!n)return;
 // RM_CCD=off: none of it (a check's switch, to tell what continuous collision changes).
 static const bool off=[]{const char* v=std::getenv("RM_CCD");return v&&(std::strcmp(v,"off")==0||std::strcmp(v,"0")==0);}();
 if(off){for(auto& b:s.bodies)b.ccd=false;return;}
 // Bodies joined by joints that hold are swept as one island (a corpse's sections, a door on its hinge, a lamp and its
 // chain): the island's earliest time of impact, over every fast body in it, puts all of its bodies back to that moment
 // together, and the blow lands on the one that struck. Put back alone, one body would open its joints by as far as it
 // went (a crate landing on a bridge pulled its planks 7 cm off their pins); never swept, a thrown corpse or a slammed
 // door went through a thin wall or post within one step.
 // Pairs the step itself keeps from meeting, or meets its own way, are not the pair pass's either: a time of impact
 // between them would bounce apart what holds them together. Ruled out by the Space's filter or joined by a joint
 // that holds, met by the caller's own contacts (Space::pairContacts: a corpse's skin against its other sections),
 // or soft (a padded prop, a prop going through a corpse: a spring by design, never a wall).
 std::vector<int> root(static_cast<size_t>(n));for(int i=0;i<n;++i)root[size_t(i)]=i;
 auto findRoot=[&](int i){while(root[size_t(i)]!=i){root[size_t(i)]=root[size_t(root[size_t(i)])];i=root[size_t(i)];}return i;};
 std::vector<char> held(size_t(n),0);std::vector<std::pair<int,int>> joined;
 for(auto* c:s.constraints){if(auto* j=c->joint();j&&j->broken)continue;const int a=c->bodyA(),b=c->bodyB();
  for(int k:{a,b})if(k>=0&&k<n)held[size_t(k)]=1;
  if(a>=0&&b>=0&&a<n&&b<n&&a!=b){joined.push_back({std::min(a,b),std::max(a,b)});const int ra=findRoot(a),rb=findRoot(b);if(ra!=rb)root[size_t(std::max(ra,rb))]=std::min(ra,rb);}}
 std::sort(joined.begin(),joined.end());
 // Each island's bodies, in index order (a body no joint holds is an island of its own).
 std::vector<int> islandStart(size_t(n)+1,0),islandBodies(static_cast<size_t>(n));
 {for(int i=0;i<n;++i)++islandStart[size_t(findRoot(i))+1];for(int i=0;i<n;++i)islandStart[size_t(i)+1]+=islandStart[size_t(i)];
  std::vector<int> fill(islandStart.begin(),islandStart.end()-1);for(int i=0;i<n;++i)islandBodies[size_t(fill[size_t(findRoot(i))]++)]=i;}
 auto membersOf=[&](int i){const int r=findRoot(i);return std::pair<const int*,const int*>{islandBodies.data()+islandStart[size_t(r)],islandBodies.data()+islandStart[size_t(r)+1]};};
 std::vector<Manifold> scratch;
 auto apart=[&](int a,int b){if(a>b)std::swap(a,b);
  if(s.filter&&!s.filter(s,a,b))return true;
  if(std::binary_search(joined.begin(),joined.end(),std::make_pair(a,b)))return true;
  if(s.bodies[size_t(a)].softHertz>0||s.bodies[size_t(b)].softHertz>0)return true;
  if(s.pairContacts){scratch.clear();if(s.pairContacts(s,a,b,0.f,scratch))return true;}
  return false;};
 std::vector<Motion> motion(static_cast<size_t>(n));std::vector<char> fast(size_t(n),0),done(size_t(n),0);std::vector<float> thresholds(size_t(n),0.f),thick(size_t(n),0.f);
 for(int i=0;i<n;++i){auto& b=s.bodies[size_t(i)];auto& m=motion[size_t(i)];m.x1=b.x;m.q1=b.q;m.x0=size_t(i)<start.size()?start[size_t(i)]:b.x;
  // Where it began the step: its turn since (dq) undone.
  m.q0=b.movable()?(b.dq.conj()*b.q).normalized():b.q;m.begin();b.ccd=false;if(!b.shape)continue;
  // Fast for its size: it could cross something thinner than itself within this step, by going or by turning
  // (a plank's end sweeps far while its middle hardly moves).
  float small=1e30f;for(int p=0;p<b.shape->count;++p){const auto& part=b.shape->parts[size_t(p)];small=std::min(small,part.kind==PartKind::Box||part.kind==PartKind::Hull?std::min({part.half.x,part.half.y,part.half.z}):part.kind==PartKind::Capsule?part.half.x:std::min(part.half.x,part.kind==PartKind::Cylinder?part.half.z:part.half.x));}
  const float threshold=std::max(.02f,small*.5f);thresholds[size_t(i)]=threshold;thick[size_t(i)]=2*small;if(!b.movable())continue;
  b.ccd=len(m.x1-m.x0)>threshold||m.angle*b.shape->bound>threshold;fast[size_t(i)]=b.ccd;}
 const float target=2*s.settings.linearSlop;std::vector<Probe> probes;std::vector<ProbeHit> hits;
 // The step's sweeps against the world (Settings::ccdBudget): each fast body's share of what is left, so a blast of many
 // fast props thins each one's probes rather than the last ones' going without.
 int fastLeft=int(std::count(fast.begin(),fast.end(),char(1)));
 // Every body's swept box along x, sorted once: a fast body meets only those whose boxes reach its own (400 resting props
 // and 20 shards: 20 searches, not 8,000 box tests). A body met and put back keeps its way inside its first box.
 std::vector<std::pair<float,int>> byX;float widest=0;std::vector<int> near;
 if(std::find(fast.begin(),fast.end(),char(1))!=fast.end())for(int j=0;j<n;++j){const Body& o=s.bodies[size_t(j)];if(!o.shape)continue;const Motion& mo=motion[size_t(j)];const float r=o.shape->bound;
  const float lo=std::min(mo.x0.x,mo.x1.x)-r,hi=std::max(mo.x0.x,mo.x1.x)+r;byX.push_back({lo,j});widest=std::max(widest,hi-lo);}
 std::sort(byX.begin(),byX.end());
 // What body i meets first this step: the world at its edges, then the other bodies (the time two moving, turning shapes
 // first meet).
 auto earliest=[&](int i,Event& best){Body& b=s.bodies[size_t(i)];Motion& m=motion[size_t(i)];
  const int share=s.settings.ccdBudget>0?std::max(1,(s.settings.ccdBudget-s.ccdSweeps)/std::max(1,fastLeft)):0;--fastLeft;
  if(s.sweep)worldProbes(s,i,m,thick[size_t(i)],share,probes,hits,best);
  const float rb=b.shape->bound;
  const V3 sweptLo=V3{std::min(m.x0.x,m.x1.x),std::min(m.x0.y,m.x1.y),std::min(m.x0.z,m.x1.z)}-V3{rb,rb,rb},sweptHi=V3{std::max(m.x0.x,m.x1.x),std::max(m.x0.y,m.x1.y),std::max(m.x0.z,m.x1.z)}+V3{rb,rb,rb};
  near.clear();for(auto it=std::lower_bound(byX.begin(),byX.end(),std::make_pair(sweptLo.x-widest-.01f,-1));it!=byX.end()&&it->first<=sweptHi.x+.01f;++it)near.push_back(it->second);
  std::sort(near.begin(),near.end());   // (in index order, as before: the same first hit on a tie)
  for(int j:near){if(j==i)continue;const Body& o=s.bodies[size_t(j)];if(!o.shape||(b.group&&b.group==o.group))continue;
   const Motion& mo=motion[size_t(j)];const float r=o.shape->bound;
   if(std::min(mo.x0.x,mo.x1.x)-r>sweptHi.x||std::max(mo.x0.x,mo.x1.x)+r<sweptLo.x||std::min(mo.x0.y,mo.x1.y)-r>sweptHi.y||std::max(mo.x0.y,mo.x1.y)+r<sweptLo.y||std::min(mo.x0.z,mo.x1.z)-r>sweptHi.z||std::max(mo.x0.z,mo.x1.z)+r<sweptLo.z)continue;
   if(apart(i,j))continue;
   // A pair going together (a crate and the bottles in it, a stack shoved whole) meets by its contacts: only one that
   // went through the other's frame further than the thinner of them could keep it out of is swept.
   if(relativeSweep(m,mo,o.shape->bound)<=std::min(thresholds[size_t(i)],thresholds[size_t(j)]))continue;
   // (Touching when the step began: the contacts' business. Stopped by its speculative contacts: met at the step's
   // very end, closing no more, and so left as it is.)
   float t;V3 point,normal;if(!timeOfImpact(b,m,o,mo,target,t,point,normal)||t>=best.t)continue;
   best={t,2,j,point,normal};}};
 // Back to the moment t: every movable body of i's island together (each along its own way through the step, so what
 // joins them is as it was then), moved on by `shift` (off what was struck). Each one's way now ends there (later
 // bodies meet that).
 auto rewind=[&](int i,float t,V3 shift){const auto [first,last]=membersOf(i);
  for(const int* it=first;it!=last;++it){const int k=*it;Body& o=s.bodies[size_t(k)];if(!o.shape||!o.movable()||done[size_t(k)])continue;Motion& mo=motion[size_t(k)];
   mo.at(t,o.x,o.q);o.updateInertia();o.x=o.x+shift;mo.x1=o.x;mo.q1=o.q;mo.begin();done[size_t(k)]=1;}};
 auto finished=[&](int i){const auto [first,last]=membersOf(i);for(const int* it=first;it!=last;++it)done[size_t(*it)]=1;};
 for(int i=0;i<n;++i){if(!fast[size_t(i)]||done[size_t(i)])continue;
  // The island's first impact over its fast bodies (a body alone: its own).
  int si=i;Event best;
  {const auto [first,last]=membersOf(i);
   for(const int* it=first;it!=last;++it){const int k=*it;if(!fast[size_t(k)]||done[size_t(k)])continue;Event e;e.t=best.t;earliest(k,e);if(e.kind!=0&&e.t<best.t){best=e;si=k;}}}
  Body& b=s.bodies[size_t(si)];Motion& m=motion[size_t(si)];
  if(best.kind==0){
   // Nothing at its edges: its centre's way still (as before the probes): back to where the centre meets a
   // surface, resting its own reach off it, its way into it taken out. (A body a joint holds is left to its probes:
   // its centre put back alone would open its joints.)
   if(held[size_t(i)]){finished(i);continue;}
   if(!s.sweep||len(m.x1-m.x0)<=thresholds[size_t(i)])continue;V3 nn;const float t=s.sweep(s,i,m.x0,m.x1,nn);if(t>=1||t<=0)continue;   // (starting in something: the contacts' business)
   float extent=0;for(int p=0;p<b.shape->count;++p)extent=std::max(extent,dot(b.x-support(b,p,neg(nn)),nn));
   b.x=m.x0+(m.x1-m.x0)*t+nn*extent;const float vn=dot(b.v,nn);if(vn<0)b.v=b.v-nn*(vn*(1+b.material.restitution));
   m.x1=b.x;m.q1=b.q;m.begin();done[size_t(i)]=1;continue;}   // its way now ends where it was put (later bodies meet that)
  const float e=best.kind==1?b.material.restitution:Material::mix(b.material,s.bodies[size_t(best.other)].material).restitution;
  ContactEvent ev;ev.a=si;ev.b=best.kind==1?-1:best.other;
  if(best.kind==1){
   // Back to where the probe met the world, a millimetre off it (its island with it); its way into it taken out at that
   // point (a corner striking a post turns the body, not only stops it).
   const V3 nn=best.normal;rewind(si,best.t,nn*.001f);
   const V3 arm=b.q.rotate(best.point),at=b.x+arm;const float vn=dot(b.pointVelocity(at),nn),k=effective(b,arm,nn);float impulse=0;
   if(vn<0&&k>0){impulse=-(1+e)*vn/k;b.v=b.v+nn*(impulse*b.invMass);b.w=b.w+b.invInertia*cross(arm,nn*impulse);}
   ev.point=at;ev.normal=neg(nn);ev.approach=std::max(0.f,-vn);ev.impulse=impulse;ev.effectiveMass=k>0?1/k:0;}
  else{
   // Both back to where they meet (the other too when it was itself fast, with its island; a slow one moved too little
   // to matter), a hair apart, and the blow between them there.
   Body& o=s.bodies[size_t(best.other)];const V3 nn=best.normal;
   rewind(si,best.t,nn*-.0005f);
   if(fast[size_t(best.other)]&&!done[size_t(best.other)]){rewind(best.other,best.t,nn*.0005f);finished(best.other);}
   // (A body already done (best.other < i, or swept earlier) has had its way cut to end at its own hit: met here along
   // that shortened way, from where it began to where it was put, not along the path it truly took before its hit.)
   // A body a joint holds takes no blow here: its partners share it, which only the step's solver knows (struck alone, a
   // 2 kg plank of a bridge took a 20 kg crate's landing as if it were loose). Put back a hair apart, the two meet by their
   // contacts on the next step, its joints in the same solve.
   const V3 armA=best.point-b.x,armB=best.point-o.x;const float vn=dot(o.pointVelocity(best.point)-b.pointVelocity(best.point),nn);
   const float k=effective(b,armA,nn)+(o.invMass>0?effective(o,armB,nn):0.f);float impulse=0;
   if(vn<0&&k>0&&!held[size_t(si)]&&!held[size_t(best.other)]){impulse=-(1+e)*vn/k;const V3 P=nn*impulse;b.v=b.v-P*b.invMass;b.w=b.w-b.invInertia*cross(armA,P);
    if(o.invMass>0){o.sleeping=false;o.sleepTime=0;o.v=o.v+P*o.invMass;o.w=o.w+o.invInertia*cross(armB,P);}}
   ev.point=best.point;ev.normal=nn;ev.approach=std::max(0.f,-vn);ev.impulse=impulse;ev.effectiveMass=k>0?1/k:0;}
  // (The whole island is done: its fast bodies that struck nothing first were put back with it.)
  finished(si);
  if(ev.approach>.3f)s.events.push_back(ev);}
}
}

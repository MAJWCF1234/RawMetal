// Vectra's queries (Queries.h): rays exactly against each part, shape casts and times of impact by
// conservative advancement on a GJK distance, and the continuous collision Space::continuous runs.
#include "Queries.h"
#include <algorithm>
#include <cmath>
namespace retro::vectra {
namespace {
// ---- Support-mapped convex shapes ----------------------------------------------------------------
// A point (a sphere's centre, with its radius as a margin), an oriented box or an upright cylinder in its
// own frame. GJK works on the core; the margin is taken off its distance.
struct Convex {
 enum Kind:std::uint8_t {Point,Box,Cylinder} kind=Point;
 V3 c{};Quat q{};V3 h{};float r=0;
 V3 support(V3 d)const{
  if(kind==Point)return c;
  const V3 l=q.unrotate(d);
  if(kind==Box)return c+q.rotate({l.x<0?-h.x:h.x,l.y<0?-h.y:h.y,l.z<0?-h.z:h.z});
  const float rho=std::sqrt(l.x*l.x+l.y*l.y);V3 s=rho>1e-9f?V3{l.x/rho*h.x,l.y/rho*h.x,0}:V3{};s.z=l.z<0?-h.z:h.z;return c+q.rotate(s);}
};
void frameOf(const Body& b,int part,V3& centre,Quat& turn){const auto& p=b.shape->parts[size_t(part)];centre=b.x+b.q.rotate(p.centre-b.shape->com);turn=b.q*p.turn;}
Convex convexOf(const Body& b,int part){const auto& p=b.shape->parts[size_t(part)];Convex k;frameOf(b,part,k.c,k.q);
 if(p.kind==PartKind::Sphere){k.kind=Convex::Point;k.r=p.half.x;}else{k.kind=p.kind==PartKind::Box?Convex::Box:Convex::Cylinder;k.h=p.half;}return k;}
float reachOf(const Part& p){switch(p.kind){case PartKind::Box:return len(p.half);case PartKind::Cylinder:return std::sqrt(p.half.x*p.half.x+p.half.z*p.half.z);default:return p.half.x;}}
float reachOf(const Convex& k){return k.kind==Convex::Point?k.r:k.kind==Convex::Box?len(k.h):std::sqrt(k.h.x*k.h.x+k.h.z*k.h.z);}

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
 for(int it=0;it<64;++it){
  A.c=start+dir*t;V3 pa,pb,nn;bool overlap;float d=distanceOf(A,B,pa,pb,nn,overlap);
  if(overlap){if(it==0){hit.distance=0;hit.point=A.c;hit.normal=neg(dir);hit.startedInside=true;return true;}d=0;nn=n;}
  if(d<=CastTolerance){hit.distance=t;hit.normal=neg(nn);hit.point=pb;hit.startedInside=false;return true;}
  n=nn;const float closing=dot(dir,n);if(closing<=1e-6f)return false;
  t+=d/closing;if(t>maxD)return false;}
 A.c=start+dir*t;hit.distance=t;hit.normal=neg(n);hit.point=A.c;hit.startedInside=false;return true;}
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
 const float r=p.half.x;for(int i=-1;i<=1;++i)for(int j=-1;j<=1;++j)for(int k=-1;k<=1;++k)if(i||j||k)put(norm(V3{float(i),float(j),float(k)})*r);}

}

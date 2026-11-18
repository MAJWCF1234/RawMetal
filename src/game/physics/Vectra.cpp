// The rigid-body core (Vectra.h). Two halves:
//  - the narrow phase: a box and a cylinder are convex polyhedra (the cylinder a 12-sided prism scaled to
//    its true area), tested by the separating axis test (faces of each, crosses of edge directions), and
//    a face contact clips the incident face against the reference face's sides for up to four points,
//    as Gregorius describes for Valve's own solver; spheres meet everything exactly;
//  - the solver: TGS-soft (Catto, Solver2D and Box2D v3): one collision pass a step, then substeps of
//    integrate velocities, warm start, a soft solve (contacts as springs of `contactHertz`, critically
//    over-damped, push-out capped), integrate positions, relax without the soft bias; restitution last.
//    The separation of every contact is updated from the bodies' motion in each substep, so a stack
//    holds with four substeps and few iterations, and speculative contacts (positive separation) catch
//    what closes within the step.
#include "Vectra.h"
#include "Queries.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <iterator>
namespace retro::vectra {
M3 M3::inverse()const{
 const float a=m[0][0],b=m[0][1],c=m[0][2],d=m[1][0],e=m[1][1],f=m[1][2],g=m[2][0],h=m[2][1],i=m[2][2];
 const float A=e*i-f*h,B=-(d*i-f*g),C=d*h-e*g;const float det=a*A+b*B+c*C;
 if(std::fabs(det)<1e-20f)return zero();const float s=1/det;M3 r;
 r.m[0][0]=A*s;r.m[0][1]=-(b*i-c*h)*s;r.m[0][2]=(b*f-c*e)*s;
 r.m[1][0]=B*s;r.m[1][1]=(a*i-c*g)*s;r.m[1][2]=-(a*f-c*d)*s;
 r.m[2][0]=C*s;r.m[2][1]=-(a*h-b*g)*s;r.m[2][2]=(a*e-b*d)*s;return r;}
M3 Quat::matrix()const{M3 r;const float xx=x*x,yy=y*y,zz=z*z,xy=x*y,xz=x*z,yz=y*z,wx=w*x,wy=w*y,wz=w*z;
 r.m[0][0]=1-2*(yy+zz);r.m[0][1]=2*(xy-wz);r.m[0][2]=2*(xz+wy);
 r.m[1][0]=2*(xy+wz);r.m[1][1]=1-2*(xx+zz);r.m[1][2]=2*(yz-wx);
 r.m[2][0]=2*(xz-wy);r.m[2][1]=2*(yz+wx);r.m[2][2]=1-2*(xx+yy);return r;}
Quat Quat::fromEuler(float yaw,float pitch,float roll){
 const Quat qz{std::cos(yaw*.5f),0,0,std::sin(yaw*.5f)},qx{std::cos(roll*.5f),std::sin(roll*.5f),0,0},qy{std::cos(pitch*.5f),0,std::sin(pitch*.5f),0};
 return (qz*qx*qy).normalized();}
void Quat::toEuler(float& yaw,float& pitch,float& roll)const{
 // R = Rz(yaw) Rx(roll) Ry(pitch): row 2 is (-cr sp, sr, cr cp); column 1 is (-sy cr, cy cr, sr).
 const M3 r=matrix();const float cr=std::sqrt(r.m[2][0]*r.m[2][0]+r.m[2][2]*r.m[2][2]);
 roll=std::atan2(r.m[2][1],cr);
 if(cr>1e-5f){pitch=std::atan2(-r.m[2][0],r.m[2][2]);yaw=std::atan2(-r.m[0][1],r.m[1][1]);}
 else{pitch=0;yaw=std::atan2(r.m[1][0],r.m[0][0]);}
 // The same turn is also (yaw+pi, pitch+pi, pi-roll): keep the one with the smaller of the two largest, so the
 // angles say how far it is over (a chair 100 degrees over reads roll 100, not pitch 170 with roll 80).
 {const float p2=pitch>0?pitch-kPi:pitch+kPi,r2=roll>0?kPi-roll:-kPi-roll;
  if(std::max(std::fabs(p2),std::fabs(r2))<std::max(std::fabs(pitch),std::fabs(roll))-1e-4f){pitch=p2;roll=r2;yaw=yaw>0?yaw-kPi:yaw+kPi;}}}

// ---- Shapes ---------------------------------------------------------------------------------------
namespace {
float partVolume(const Part& p){switch(p.kind){case PartKind::Box:return 8*p.half.x*p.half.y*p.half.z;case PartKind::Cylinder:return kPi*p.half.x*p.half.x*2*p.half.z;default:return 4.f/3*kPi*p.half.x*p.half.x*p.half.x;}}
V3 partInertia(const Part& p){ // per kilogram, principal, about the part's own centre
 const V3 h=p.half;switch(p.kind){
  case PartKind::Box:return {(h.y*h.y+h.z*h.z)/3,(h.x*h.x+h.z*h.z)/3,(h.x*h.x+h.y*h.y)/3};
  case PartKind::Cylinder:{const float r2=h.x*h.x,l2=h.z*h.z;return {r2/4+l2/3,r2/4+l2/3,r2/2};}
  default:{const float s=.4f*h.x*h.x;return {s,s,s};}}}
float partReach(const Part& p){switch(p.kind){case PartKind::Box:return len(p.half);case PartKind::Cylinder:return std::sqrt(p.half.x*p.half.x+p.half.z*p.half.z);default:return p.half.x;}}
}
void Shape::finish(){
 float total=0;V3 centre{};for(int i=0;i<count;++i){const float v=std::max(1e-9f,partVolume(parts[size_t(i)]));total+=v;centre=centre+parts[size_t(i)].centre*v;}
 com=total>0?centre*(1/total):V3{};M3 sum=M3::zero();bound=0;
 for(int i=0;i<count;++i){const auto&p=parts[size_t(i)];const float share=std::max(1e-9f,partVolume(p))/total;
  const M3 r=p.turn.matrix();M3 local=r*M3::diagonal(partInertia(p))*r.transposed();
  const V3 d=p.centre-com;M3 shift=M3::diagonal({dot(d,d),dot(d,d),dot(d,d)});
  for(int a=0;a<3;++a)for(int b=0;b<3;++b)shift.m[a][b]-=comp(d,a)*comp(d,b);
  sum=sum+(local+shift)*share;bound=std::max(bound,len(d)+partReach(p));}
 inertia=sum;}

// ---- Polyhedra ------------------------------------------------------------------------------------
namespace {
constexpr int PrismSides=12;
struct Template {
 int nv=0;V3 v[2*PrismSides]{};
 struct Face {int n=0;int idx[PrismSides]{};V3 normal{};};int nf=0;Face f[PrismSides+2]{};
 int ne=0;int e[3*PrismSides][2]{};int edir[3*PrismSides]{};int nd=0;V3 dir[PrismSides]{};
};
Template build(const std::vector<V3>& verts,const std::vector<std::vector<int>>& faces){
 Template t;t.nv=int(verts.size());for(size_t i=0;i<verts.size();++i)t.v[i]=verts[i];
 V3 centre{};for(auto v:verts)centre=centre+v;centre=centre*(1.f/float(verts.size()));
 for(const auto& face:faces){auto&f=t.f[t.nf++];f.n=int(face.size());for(size_t k=0;k<face.size();++k)f.idx[k]=face[k];
  // Newell's normal; wind the face anticlockwise seen from outside.
  V3 n{},mid{};for(int k=0;k<f.n;++k){const V3 a=verts[size_t(f.idx[k])],b=verts[size_t(f.idx[(k+1)%f.n])];n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);mid=mid+a;}
  mid=mid*(1.f/float(f.n));n=norm(n);if(dot(n,mid-centre)<0){std::reverse(f.idx,f.idx+f.n);n=neg(n);}f.normal=n;
  for(int k=0;k<f.n;++k){int a=f.idx[k],b=f.idx[(k+1)%f.n];if(a>b)std::swap(a,b);bool seen=false;for(int j=0;j<t.ne;++j)if(t.e[j][0]==a&&t.e[j][1]==b)seen=true;
   if(seen)continue;t.e[t.ne][0]=a;t.e[t.ne][1]=b;const V3 d=norm(verts[size_t(b)]-verts[size_t(a)]);int found=-1;
   for(int j=0;j<t.nd;++j)if(lenSq(cross(t.dir[j],d))<1e-6f){found=j;break;}if(found<0){found=t.nd;t.dir[t.nd++]=d;}t.edir[t.ne++]=found;}}
 return t;}
const Template& boxTemplate(){static const Template t=[]{std::vector<V3> v;for(int i=0;i<8;++i)v.push_back({i&1?1.f:-1.f,i&2?1.f:-1.f,i&4?1.f:-1.f});
 return build(v,{{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}});}();return t;}
const Template& prismTemplate(){static const Template t=[]{std::vector<V3> v;
 // Vertices at the radius that gives the prism the cylinder's own cross-section area.
 const float r=std::sqrt(2*kPi/(PrismSides*std::sin(2*kPi/PrismSides)));
 for(int ring=0;ring<2;++ring)for(int i=0;i<PrismSides;++i){const float a=2*kPi*(float(i)+.5f)/PrismSides;v.push_back({std::cos(a)*r,std::sin(a)*r,ring?1.f:-1.f});}
 std::vector<std::vector<int>> f;std::vector<int> bottom,top;for(int i=0;i<PrismSides;++i){bottom.push_back(i);top.push_back(PrismSides+i);}f.push_back(bottom);f.push_back(top);
 for(int i=0;i<PrismSides;++i){const int j=(i+1)%PrismSides;f.push_back({i,j,PrismSides+j,PrismSides+i});}
 return build(v,f);}();return t;}
struct Hull {const Template* t=nullptr;V3 v[2*PrismSides];V3 n[PrismSides+2];float d[PrismSides+2];V3 dir[PrismSides];V3 centre;};
void partFrame(const Body& b,int part,V3& centre,Quat& turn){const auto&p=b.shape->parts[size_t(part)];centre=b.x+b.q.rotate(p.centre-b.shape->com);turn=b.q*p.turn;}
V3 scaleOf(const Part& p){return p.kind==PartKind::Cylinder?V3{p.half.x,p.half.x,p.half.z}:p.half;}
Hull hullOf(const Body& b,int part){
 Hull h;const auto&p=b.shape->parts[size_t(part)];h.t=p.kind==PartKind::Cylinder?&prismTemplate():&boxTemplate();Quat turn;partFrame(b,part,h.centre,turn);const V3 s=scaleOf(p);
 for(int i=0;i<h.t->nv;++i)h.v[i]=h.centre+turn.rotate(mulv(h.t->v[i],s));
 for(int f=0;f<h.t->nf;++f){h.n[f]=turn.rotate(h.t->f[f].normal);h.d[f]=dot(h.n[f],h.v[h.t->f[f].idx[0]]);}
 for(int i=0;i<h.t->nd;++i)h.dir[i]=turn.rotate(h.t->dir[i]);return h;}
struct FaceQuery {int index=-1;float separation=-1e30f;};
FaceQuery queryFaces(const Hull& a,const Hull& b){FaceQuery q;
 for(int f=0;f<a.t->nf;++f){float lo=1e30f;for(int i=0;i<b.t->nv;++i)lo=std::min(lo,dot(a.n[f],b.v[i]));const float s=lo-a.d[f];if(s>q.separation){q.separation=s;q.index=f;}}return q;}
struct EdgeQuery {int dirA=-1,dirB=-1;float separation=-1e30f;V3 axis{};};
EdgeQuery queryEdges(const Hull& a,const Hull& b){EdgeQuery q;const V3 between=b.centre-a.centre;
 for(int i=0;i<a.t->nd;++i)for(int j=0;j<b.t->nd;++j){V3 axis=cross(a.dir[i],b.dir[j]);const float l=len(axis);if(l<1e-3f)continue;axis=axis*(1/l);if(dot(axis,between)<0)axis=neg(axis);
  float hiA=-1e30f,loB=1e30f;for(int k=0;k<a.t->nv;++k)hiA=std::max(hiA,dot(axis,a.v[k]));for(int k=0;k<b.t->nv;++k)loB=std::min(loB,dot(axis,b.v[k]));
  const float s=loB-hiA;if(s>q.separation){q.separation=s;q.dirA=i;q.dirB=j;q.axis=axis;}}
 return q;}
// Keep up to four of `count` points: the deepest, the farthest from it, then the largest triangle and quad.
int reduce(V3* p,float* s,std::uint32_t* id,int count,V3 n){
 if(count<=4)return count;int pick[4]{0,-1,-1,-1};
 for(int i=1;i<count;++i)if(s[i]<s[pick[0]])pick[0]=i;
 float best=-1;for(int i=0;i<count;++i){const float d=lenSq(p[i]-p[pick[0]]);if(d>best){best=d;pick[1]=i;}}
 best=-1;for(int i=0;i<count;++i){if(i==pick[0]||i==pick[1])continue;const float a=std::fabs(dot(cross(p[pick[1]]-p[pick[0]],p[i]-p[pick[0]]),n));if(a>best){best=a;pick[2]=i;}}
 auto area=[&](V3 a,V3 b,V3 c){return std::fabs(dot(cross(b-a,c-a),n));};
 best=-1;const float tri=area(p[pick[0]],p[pick[1]],p[pick[2]]);
 for(int i=0;i<count;++i){if(i==pick[0]||i==pick[1]||i==pick[2])continue;const float a=area(p[pick[0]],p[pick[1]],p[i])+area(p[pick[1]],p[pick[2]],p[i])+area(p[pick[2]],p[pick[0]],p[i])-tri;if(a>best){best=a;pick[3]=i;}}
 V3 q[4];float t[4];std::uint32_t u[4];int k=0;for(int i:pick)if(i>=0){q[k]=p[i];t[k]=s[i];u[k]=id[i];++k;}
 for(int i=0;i<k;++i){p[i]=q[i];s[i]=t[i];id[i]=u[i];}return k;}
void setPoint(Manifold& m,const Body& a,const Body& b,V3 p,float separation,std::uint32_t feature){
 auto&c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=p;c.separation=separation;c.feature=feature;
 c.localA=a.q.unrotate(p-a.x);c.localB=b.q.unrotate(p-b.x);}
int faceContact(const Body& A,const Body& B,const Hull& ref,int rf,const Hull& inc,bool flip,float margin,Manifold& m){
 const V3 n=ref.n[rf];int inf=0;float lo=1e30f;for(int f=0;f<inc.t->nf;++f){const float d=dot(inc.n[f],n);if(d<lo){lo=d;inf=f;}}
 V3 poly[64],next[64];int count=inc.t->f[inf].n;std::uint32_t ids[64];for(int k=0;k<count;++k){poly[k]=inc.v[inc.t->f[inf].idx[k]];}
 const auto&F=ref.t->f[rf];
 for(int k=0;k<F.n&&count>0;++k){const V3 a=ref.v[F.idx[k]],b=ref.v[F.idx[(k+1)%F.n]];const V3 side=norm(cross(b-a,n));const float offset=dot(side,a);int out=0;
  for(int i=0;i<count;++i){const V3 p=poly[i],q=poly[(i+1)%count];const float dp=dot(side,p)-offset,dq=dot(side,q)-offset;
   if(dp<=0&&out<64)next[out++]=p;if(((dp<0&&dq>0)||(dp>0&&dq<0))&&out<64){   // strictly across: a vertex on the side is kept once, as itself
   const float t=dp/(dp-dq);next[out++]=p+(q-p)*t;}}
  count=out;for(int i=0;i<count;++i)poly[i]=next[i];}
 V3 pts[64];float seps[64];int kept=0;
 for(int i=0;i<count;++i){const float s=dot(n,poly[i])-ref.d[rf];if(s>margin)continue;pts[kept]=poly[i]-n*(s*.5f);seps[kept]=s;ids[kept]=std::uint32_t(rf<<16|inf<<8|i);++kept;}
 kept=reduce(pts,seps,ids,kept,n);m.normal=flip?neg(n):n;m.count=0;
 for(int i=0;i<kept;++i)setPoint(m,A,B,pts[i],seps[i],ids[i]);return m.count;}
void closestSegments(V3 p1,V3 q1,V3 p2,V3 q2,V3& c1,V3& c2){
 const V3 d1=q1-p1,d2=q2-p2,r=p1-p2;const float a=dot(d1,d1),e=dot(d2,d2),f=dot(d2,r);float s=0,t=0;
 if(a<=1e-12f&&e<=1e-12f){c1=p1;c2=p2;return;}
 if(a<=1e-12f){t=std::clamp(f/e,0.f,1.f);}
 else{const float c=dot(d1,r);if(e<=1e-12f)s=std::clamp(-c/a,0.f,1.f);
  else{const float b=dot(d1,d2),den=a*e-b*b;s=den>1e-12f?std::clamp((b*f-c*e)/den,0.f,1.f):0.f;t=(b*s+f)/e;
   if(t<0){t=0;s=std::clamp(-c/a,0.f,1.f);}else if(t>1){t=1;s=std::clamp((b-c)/a,0.f,1.f);}}}
 c1=p1+d1*s;c2=p2+d2*t;}
int hullHull(const Body& A,const Body& B,const Hull& a,const Hull& b,float margin,Manifold& m){
 const auto fa=queryFaces(a,b);if(fa.separation>margin)return 0;
 const auto fb=queryFaces(b,a);if(fb.separation>margin)return 0;
 const auto ed=queryEdges(a,b);if(ed.separation>margin)return 0;
 const float faceBest=std::max(fa.separation,fb.separation);
 // Faces are preferred (a full manifold holds a stack); an edge pair wins only when it is clearly the
 // shallower way out.
 if(ed.dirA>=0&&ed.separation>.95f*faceBest+.004f){
  float best=-1e30f;int ea=-1;for(int i=0;i<a.t->ne;++i)if(a.t->edir[i]==ed.dirA){const float d=dot(ed.axis,a.v[a.t->e[i][0]]+a.v[a.t->e[i][1]]);if(d>best){best=d;ea=i;}}
  best=1e30f;int eb=-1;for(int i=0;i<b.t->ne;++i)if(b.t->edir[i]==ed.dirB){const float d=dot(ed.axis,b.v[b.t->e[i][0]]+b.v[b.t->e[i][1]]);if(d<best){best=d;eb=i;}}
  if(ea>=0&&eb>=0){V3 c1,c2;closestSegments(a.v[a.t->e[ea][0]],a.v[a.t->e[ea][1]],b.v[b.t->e[eb][0]],b.v[b.t->e[eb][1]],c1,c2);
   m.normal=ed.axis;m.count=0;setPoint(m,A,B,(c1+c2)*.5f,dot(c2-c1,ed.axis),std::uint32_t(0x1000000|ea<<8|eb));return 1;}}
 if(fb.separation>.98f*fa.separation+.001f)return faceContact(A,B,b,fb.index,a,true,margin,m);
 return faceContact(A,B,a,fa.index,b,false,margin,m);}
// A point's nearest place on a part (sphere centre against box or cylinder): the surface point and the
// outward normal there, and whether the point is inside.
bool nearestOnPart(const Body& b,int part,V3 point,V3& surface,V3& normal,float& distance){
 const auto&p=b.shape->parts[size_t(part)];V3 centre;Quat turn;partFrame(b,part,centre,turn);const V3 l=turn.unrotate(point-centre);V3 q,n;bool inside;
 if(p.kind==PartKind::Box){const V3 h=p.half;q={std::clamp(l.x,-h.x,h.x),std::clamp(l.y,-h.y,h.y),std::clamp(l.z,-h.z,h.z)};inside=lenSq(q-l)<1e-12f;
  if(inside){const float dx=h.x-std::fabs(l.x),dy=h.y-std::fabs(l.y),dz=h.z-std::fabs(l.z);
   if(dx<=dy&&dx<=dz){n={l.x<0?-1.f:1.f,0,0};q.x=n.x*h.x;}else if(dy<=dz){n={0,l.y<0?-1.f:1.f,0};q.y=n.y*h.y;}else{n={0,0,l.z<0?-1.f:1.f};q.z=n.z*h.z;}}
  else n=norm(l-q);}
 else if(p.kind==PartKind::Cylinder){const float r=p.half.x,hz=p.half.z,rho=std::sqrt(l.x*l.x+l.y*l.y);const V3 radial=rho>1e-6f?V3{l.x/rho,l.y/rho,0}:V3{1,0,0};
  inside=rho<=r&&std::fabs(l.z)<=hz;
  if(inside){if(r-rho<hz-std::fabs(l.z)){q=radial*r;q.z=l.z;n=radial;}else{q={l.x,l.y,l.z<0?-hz:hz};n={0,0,l.z<0?-1.f:1.f};}}
  else{q=radial*std::min(rho,r);q.z=std::clamp(l.z,-hz,hz);n=norm(l-q,radial);}}
 else{const float r=p.half.x;n=norm(l,{0,0,1});q=n*r;inside=lenSq(l)<r*r;}
 surface=centre+turn.rotate(q);normal=turn.rotate(n);distance=inside?-len(q-l):len(l-q);return inside;}
}
bool nearestPoint(const Body& b,int part,V3 point,V3& surface,V3& normal,float& distance){return nearestOnPart(b,part,point,surface,normal,distance);}
void partBounds(const Body& b,int part,V3& lo,V3& hi){
 const auto&p=b.shape->parts[size_t(part)];V3 centre;Quat turn;partFrame(b,part,centre,turn);
 if(p.kind==PartKind::Sphere){const V3 r{p.half.x,p.half.x,p.half.x};lo=centre-r;hi=centre+r;return;}
 const M3 m=turn.matrix();const V3 s=scaleOf(p);V3 e{};
 for(int i=0;i<3;++i){const float v=std::fabs(m.m[i][0])*s.x+std::fabs(m.m[i][1])*s.y+std::fabs(m.m[i][2])*s.z;if(i==0)e.x=v;else if(i==1)e.y=v;else e.z=v;}
 if(p.kind==PartKind::Cylinder){ // a disc's extent along each world axis: r*sqrt(1-a_i^2) + h*|a_i|
  const V3 axis=m.column(2);e={p.half.x*std::sqrt(std::max(0.f,1-axis.x*axis.x))+p.half.z*std::fabs(axis.x),p.half.x*std::sqrt(std::max(0.f,1-axis.y*axis.y))+p.half.z*std::fabs(axis.y),p.half.x*std::sqrt(std::max(0.f,1-axis.z*axis.z))+p.half.z*std::fabs(axis.z)};}
 lo=centre-e;hi=centre+e;}
V3 support(const Body& b,int part,V3 d){
 const auto&p=b.shape->parts[size_t(part)];V3 centre;Quat turn;partFrame(b,part,centre,turn);const V3 l=turn.unrotate(d);
 if(p.kind==PartKind::Box)return centre+turn.rotate({l.x<0?-p.half.x:p.half.x,l.y<0?-p.half.y:p.half.y,l.z<0?-p.half.z:p.half.z});
 if(p.kind==PartKind::Cylinder){const float rho=std::sqrt(l.x*l.x+l.y*l.y);const V3 r=rho>1e-6f?V3{l.x/rho*p.half.x,l.y/rho*p.half.x,0}:V3{};return centre+turn.rotate({r.x,r.y,l.z<0?-p.half.z:p.half.z});}
 return centre+norm(d)*p.half.x;}
int supportFeature(const Body& b,int part,V3 d,std::array<V3,8>& out){
 const auto&p=b.shape->parts[size_t(part)];V3 centre;Quat turn;partFrame(b,part,centre,turn);const V3 l=norm(turn.unrotate(d));constexpr float flat=.12f;int n=0;
 if(p.kind==PartKind::Box){const bool fx=std::fabs(l.x)<flat,fy=std::fabs(l.y)<flat,fz=std::fabs(l.z)<flat;
  for(int i=0;i<8;++i){const float sx=i&1?1.f:-1.f,sy=i&2?1.f:-1.f,sz=i&4?1.f:-1.f;
   if(!fx&&(sx>0)!=(l.x>0))continue;if(!fy&&(sy>0)!=(l.y>0))continue;if(!fz&&(sz>0)!=(l.z>0))continue;
   if(n<8)out[size_t(n++)]=centre+turn.rotate({sx*p.half.x,sy*p.half.y,sz*p.half.z});}return n;}
 if(p.kind==PartKind::Cylinder){const float rho=std::sqrt(l.x*l.x+l.y*l.y),r=p.half.x,h=p.half.z;
  if(rho<flat){ // on a cap: four points round its rim
   const float z=l.z<0?-h:h;for(int k=0;k<4;++k){const float a=kPi*.5f*float(k)+kPi*.25f;out[size_t(n++)]=centre+turn.rotate({std::cos(a)*r,std::sin(a)*r,z});}return n;}
  const V3 radial{l.x/rho*r,l.y/rho*r,0};
  if(std::fabs(l.z)<flat){out[size_t(n++)]=centre+turn.rotate({radial.x,radial.y,-h});out[size_t(n++)]=centre+turn.rotate({radial.x,radial.y,h});
   if(h>.15f)out[size_t(n++)]=centre+turn.rotate({radial.x,radial.y,0});return n;}
  out[size_t(n++)]=centre+turn.rotate({radial.x,radial.y,l.z<0?-h:h});return n;}
 out[size_t(n++)]=centre+norm(d)*p.half.x;return n;}
void surfaceSamples(const Body& b,int part,float spacing,std::vector<V3>& out){
 const auto&p=b.shape->parts[size_t(part)];V3 centre;Quat turn;partFrame(b,part,centre,turn);
 auto put=[&](V3 l){out.push_back(centre+turn.rotate(l));};
 if(p.kind==PartKind::Box){const V3 h=p.half;const int nx=std::max(1,int(std::ceil(2*h.x/spacing))),ny=std::max(1,int(std::ceil(2*h.y/spacing))),nz=std::max(1,int(std::ceil(2*h.z/spacing)));
  // Every lattice point on the box's surface.
  for(int i=0;i<=nx;++i)for(int j=0;j<=ny;++j)for(int k=0;k<=nz;++k){if(i!=0&&i!=nx&&j!=0&&j!=ny&&k!=0&&k!=nz)continue;
   put({-h.x+2*h.x*float(i)/float(nx),-h.y+2*h.y*float(j)/float(ny),-h.z+2*h.z*float(k)/float(nz)});}
  return;}
 if(p.kind==PartKind::Cylinder){const float r=p.half.x,h=p.half.z;const int around=std::clamp(int(std::ceil(2*kPi*r/spacing)),8,24),along=std::max(1,int(std::ceil(2*h/spacing)));
  for(int k=0;k<=along;++k){const float z=-h+2*h*float(k)/float(along);for(int i=0;i<around;++i){const float a=2*kPi*float(i)/float(around);put({std::cos(a)*r,std::sin(a)*r,z});}}
  put({0,0,-h});put({0,0,h});if(r>spacing)for(int i=0;i<6;++i){const float a=kPi/3*float(i);put({std::cos(a)*r*.5f,std::sin(a)*r*.5f,-h});put({std::cos(a)*r*.5f,std::sin(a)*r*.5f,h});}
  return;}
 const float r=p.half.x;for(int i=-1;i<=1;++i)for(int j=-1;j<=1;++j)for(int k=-1;k<=1;++k)if(i||j||k)put(norm(V3{float(i),float(j),float(k)})*r);}
namespace {
// Two polyhedral parts whose hulls the caller already has (collide builds each part's once a step).
int collideBuilt(const Body& A,int pa,const Hull& ha,const Body& B,int pb,const Hull& hb,float margin,Manifold& m){m.count=0;m.a=-1;m.b=-1;m.partA=pa;m.partB=pb;return hullHull(A,B,ha,hb,margin,m);}
}
int collideParts(const Body& A,int pa,const Body& B,int pb,float margin,Manifold& m){
 m.count=0;m.a=-1;m.b=-1;m.partA=pa;m.partB=pb;
 const auto&a=A.shape->parts[size_t(pa)];const auto&b=B.shape->parts[size_t(pb)];
 if(a.kind==PartKind::Sphere||b.kind==PartKind::Sphere){
  const bool swap=a.kind!=PartKind::Sphere;const Body& S=swap?B:A;const Body& O=swap?A:B;const int ps=swap?pb:pa,po=swap?pa:pb;
  V3 sc;Quat st;partFrame(S,ps,sc,st);const float r=S.shape->parts[size_t(ps)].half.x;V3 n,point;float sep;
  if((swap?a:b).kind==PartKind::Sphere){V3 oc;Quat ot;partFrame(O,po,oc,ot);const float ro=O.shape->parts[size_t(po)].half.x;const V3 d=oc-sc;const float dist=len(d);
   n=dist>1e-6f?d*(1/dist):V3{0,0,1};sep=dist-r-ro;point=sc+n*(r+sep*.5f);}
  else{V3 surface,outward;float dist;nearestOnPart(O,po,sc,surface,outward,dist);
   // n from the sphere to the other part: against the other's outward normal.
   n=neg(outward);sep=dist-r;point=surface+outward*(sep*.5f);if(dist<0)sep=dist-r;}
  if(sep>margin)return 0;
  m.normal=swap?neg(n):n;setPoint(m,A,B,point,sep,0x2000000);return 1;}
 const Hull ha=hullOf(A,pa),hb=hullOf(B,pb);return hullHull(A,B,ha,hb,margin,m);}

// ---- The solver -----------------------------------------------------------------------------------
namespace {
struct Soft {float biasRate=0,massScale=1,impulseScale=0;};
Soft makeSoft(float hertz,float zeta,float h){if(hertz<=0)return {0,1,0};const float omega=2*kPi*hertz,a1=2*zeta+h*omega,a2=h*omega*a1,a3=1/(1+a2);return {omega/a1,a2*a3,a3};}
V3 velocityOf(const Body* b,V3 r,V3 surface){return b?b->v+cross(b->w,r):surface;}
}
void Space::wake(int i){auto&b=bodies[size_t(i)];if(b.sleeping){b.sleeping=false;b.sleepTime=0;}}
void Space::collide(float dt){
 manifolds.clear();pairsTested=0;worldManifolds=0;const int n=int(bodies.size());
 auto active=[&](const Body& b){return !b.sleeping&&(b.invMass>0||(b.kinematic&&(lenSq(b.v)>1e-8f||lenSq(b.w)>1e-8f)));};
 std::vector<float> reach(size_t(n),0.f);
 for(int i=0;i<n;++i){auto&b=bodies[size_t(i)];V3 lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
  if(!b.shape){b.boundsLo=lo;b.boundsHi=hi;continue;}   // no shape: nothing to meet (its bounds empty, never paired)
  for(int p=0;p<b.shape->count;++p){V3 l,h;partBounds(b,p,l,h);lo={std::min(lo.x,l.x),std::min(lo.y,l.y),std::min(lo.z,l.z)};hi={std::max(hi.x,h.x),std::max(hi.y,h.y),std::max(hi.z,h.z)};}
  b.boundsLo=lo;b.boundsHi=hi;reach[size_t(i)]=settings.speculative+(b.sleeping?0.f:std::min(1.f,(len(b.v)+len(b.w)*b.shape->bound)*dt));}
 // The world under and round each moving body.
 std::vector<char> grounded(size_t(n),0);
 auto ground=[&](int i){const auto&b=bodies[size_t(i)];if(!worldContacts||!b.shape||grounded[size_t(i)]||b.sleeping||b.invMass<=0)return;grounded[size_t(i)]=1;const size_t before=manifolds.size();worldContacts(*this,i,reach[size_t(i)],manifolds);worldManifolds+=int(manifolds.size()-before);};
 const auto t0=std::chrono::steady_clock::now();
 for(int i=0;i<n;++i)ground(i);
 const auto t1=std::chrono::steady_clock::now();worldMs=std::chrono::duration<double,std::milli>(t1-t0).count();
 // Pairs: sweep and prune along x.
 std::vector<int> order;order.reserve(size_t(n));for(int i=0;i<n;++i)order.push_back(i);
 std::sort(order.begin(),order.end(),[&](int a,int b){const float la=bodies[size_t(a)].boundsLo.x-reach[size_t(a)],lb=bodies[size_t(b)].boundsLo.x-reach[size_t(b)];return la!=lb?la<lb:a<b;});
 auto sweepPairs=[&](std::vector<std::pair<int,int>>& pairs){
 for(size_t s=0;s<order.size();++s){const int i=order[s];const auto&a=bodies[size_t(i)];const float hiX=a.boundsHi.x+reach[size_t(i)];
  for(size_t t=s+1;t<order.size();++t){const int j=order[t];const auto&b=bodies[size_t(j)];if(b.boundsLo.x-reach[size_t(j)]>hiX)break;
   if(!active(a)&&!active(b))continue;if(a.invMass<=0&&b.invMass<=0)continue;
   if(a.group&&a.group==b.group)continue;
   const float r=std::max(reach[size_t(i)],reach[size_t(j)]);
   if(a.boundsLo.y>b.boundsHi.y+r||b.boundsLo.y>a.boundsHi.y+r||a.boundsLo.z>b.boundsHi.z+r||b.boundsLo.z>a.boundsHi.z+r)continue;
   if(filter&&!filter(*this,i,j))continue;pairs.push_back({std::min(i,j),std::max(i,j)});}}
 std::sort(pairs.begin(),pairs.end());};
 std::vector<std::pair<int,int>> pairs;sweepPairs(pairs);
 // Each polyhedral part's hull, built once a step when a pair first needs it (a crate in a heap meets six).
 std::vector<int> first(size_t(n)+1,0);for(int i=0;i<n;++i)first[size_t(i)+1]=first[size_t(i)]+(bodies[size_t(i)].shape?bodies[size_t(i)].shape->count:0);
 std::vector<Hull> hulls(static_cast<size_t>(first.back()));std::vector<char> built(hulls.size(),0);
 auto hull=[&](int body,int part)->const Hull&{const size_t k=size_t(first[size_t(body)]+part);if(!built[k]){hulls[k]=hullOf(bodies[size_t(body)],part);built[k]=1;}return hulls[k];};
 int woken=0;std::vector<char> roused(size_t(n),0);   // woken by a touch this step: what it touches that sleeps wakes with it (a sleeper is a wall)
 auto meet=[&](const std::vector<std::pair<int,int>>& list){
 for(auto [i,j]:list){auto&a=bodies[size_t(i)];auto&b=bodies[size_t(j)];const float margin=std::max(reach[size_t(i)],reach[size_t(j)]);
  if(pairContacts&&pairContacts(*this,i,j,margin,manifolds)){++pairsTested;continue;}
  for(int pa=0;pa<a.shape->count;++pa)for(int pb=0;pb<b.shape->count;++pb){
   if(a.shape->count>1||b.shape->count>1){V3 l1,h1,l2,h2;partBounds(a,pa,l1,h1);partBounds(b,pb,l2,h2);
    if(l1.x>h2.x+margin||l2.x>h1.x+margin||l1.y>h2.y+margin||l2.y>h1.y+margin||l1.z>h2.z+margin||l2.z>h1.z+margin)continue;}
   ++pairsTested;Manifold m;
   const bool polyhedra=a.shape->parts[size_t(pa)].kind!=PartKind::Sphere&&b.shape->parts[size_t(pb)].kind!=PartKind::Sphere;
   if(!(polyhedra?collideBuilt(a,pa,hull(i,pa),b,pb,hull(j,pb),margin,m):collideParts(a,pa,b,pb,margin,m)))continue;m.a=i;m.b=j;m.material=Material::mix(a.material,b.material);
   {const float sa=a.softHertz,sb=b.softHertz;m.surface.hertz=sa>0&&sb>0?std::min(sa,sb):std::max(sa,sb);}
   manifolds.push_back(m);
   // A moving body touching a sleeping one wakes it, and what that touches wakes with it (the rounds below).
   float deepest=1e30f;for(int k=0;k<m.count;++k)deepest=std::min(deepest,m.points[size_t(k)].separation);
   if(deepest<margin){   // within reach this step: a sleeper treated as fixed would stop a blow dead
    auto rousing=[&](const Body& mover,const Body& other){const float speed=len(mover.v-other.v)+len(mover.w)*mover.shape->bound;return speed>2*settings.sleepLinear||(mover.kinematic&&speed>1e-3f);};
    if(a.sleeping&&!b.sleeping&&(rousing(b,a)||roused[size_t(j)])){wake(i);roused[size_t(i)]=1;++woken;}
    if(b.sleeping&&!a.sleeping&&(rousing(a,b)||roused[size_t(i)])){wake(j);roused[size_t(j)]=1;++woken;}}}}};
 meet(pairs);
 // Woken by a touch: its pairs with what still sleeps beside it were passed over as two sleepers'; met now, or
 // what woke it could drive it into them unopposed for a step.
 while(woken){woken=0;std::vector<std::pair<int,int>> more,fresh;sweepPairs(more);
  std::set_difference(more.begin(),more.end(),pairs.begin(),pairs.end(),std::back_inserter(fresh));if(fresh.empty())break;
  meet(fresh);std::vector<std::pair<int,int>> all;std::merge(pairs.begin(),pairs.end(),fresh.begin(),fresh.end(),std::back_inserter(all));pairs.swap(all);}
 pairMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t1).count();
 // Woken by a touch: the world under it this step too.
 for(int i=0;i<n;++i)ground(i);
}
void Space::prepare(float h){
 (void)h;
 // Warm start: the cache is sorted by key.
 for(auto&m:manifolds){
  const Body& A=bodies[size_t(m.a)];const Body* B=m.b>=0?&bodies[size_t(m.b)]:nullptr;
  const float mA=A.movable()?A.invMass:0,mB=B&&B->movable()?B->invMass:0;const M3 iA=A.movable()?A.invInertia:M3::zero(),iB=B&&B->movable()?B->invInertia:M3::zero();
  basis(m.normal,m.t1,m.t2);
  const auto key=keyOf(m);auto it=std::lower_bound(cache.begin(),cache.end(),key,[](const auto& e,std::uint64_t k){return e.first<k;});
  const Cached* old=it!=cache.end()&&it->first==key?&it->second:nullptr;m.cold=old==nullptr;
  if(old)for(int r=0;r<3;++r)m.rollingImpulse[r]=old->rolling[r];
  for(int k=0;k<m.count;++k){auto&c=m.points[size_t(k)];
   c.rA=c.p-A.x;c.rB=B?c.p-B->x:c.p;
   const V3 cnA=cross(c.rA,m.normal),cnB=cross(c.rB,m.normal);c.wnA=iA*cnA;c.wnB=B?iB*cnB:V3{};
   const float kn=mA+mB+dot(cnA,c.wnA)+(B?dot(cnB,c.wnB):0.f);c.normalMass=kn>0?1/kn:0;
   for(int t=0;t<2;++t){const V3 tan=t?m.t2:m.t1;const V3 ta=cross(c.rA,tan),tb=cross(c.rB,tan);c.wtA[t]=iA*ta;c.wtB[t]=B?iB*tb:V3{};const float kt=mA+mB+dot(ta,c.wtA[t])+(B?dot(tb,c.wtB[t]):0.f);c.tangentMass[t]=kt>0?1/kt:0;}
   const V3 dv=velocityOf(B,c.rB,m.surface.velocity)-(A.v+cross(A.w,c.rA));c.relativeVelocity=dot(dv,m.normal);
   if(old){float best=.02f*.02f;for(int j=0;j<old->count;++j){const auto&o=old->points[size_t(j)];const float d=lenSq(o.localA-c.localA);if(d<best){best=d;c.normalImpulse=o.normal;c.tangentImpulse[0]=o.tangent[0];c.tangentImpulse[1]=o.tangent[1];}}}
  }
  // The middle of the points (solve's impact), and the rolling and twisting axes.
  if(m.count>1){V3 mid{};for(int k=0;k<m.count;++k)mid=mid+m.points[size_t(k)].p;mid=mid*(1.f/float(m.count));
   m.midA=mid-A.x;m.midB=B?mid-B->x:mid;const V3 cA=cross(m.midA,m.normal),cB=cross(m.midB,m.normal);m.midWA=iA*cA;m.midWB=B?iB*cB:V3{};
   const float k=mA+dot(cA,m.midWA)+mB+(B?dot(cB,m.midWB):0.f);m.midMass=k>0?1/k:0;}
  for(int r=0;r<3;++r){const V3 axis=r==0?m.t1:r==1?m.t2:m.normal;m.rollWA[r]=iA*axis;m.rollWB[r]=B?iB*axis:V3{};const float k=dot(axis,m.rollWA[r])+dot(axis,m.rollWB[r]);m.rollMass[r]=k>0?1/k:0;}
  // Static or dynamic friction by how fast the surfaces slide when the step begins.
  float slide=0;for(int k=0;k<m.count;++k){const auto&c=m.points[size_t(k)];const V3 dv=velocityOf(B,c.rB,m.surface.velocity)-(A.v+cross(A.w,c.rA));slide=std::max(slide,len(dv-m.normal*dot(dv,m.normal)));}
  m.material.dynamicFriction=slide<.08f?m.material.staticFriction:m.material.dynamicFriction;
 }
}
void Space::warmStart(){
 for(auto&m:manifolds){Body& A=bodies[size_t(m.a)];Body* B=m.b>=0?&bodies[size_t(m.b)]:nullptr;const bool ma=A.movable(),mb=B&&B->movable();
  for(int k=0;k<m.count;++k){const auto&c=m.points[size_t(k)];const V3 P=m.normal*c.normalImpulse+m.t1*c.tangentImpulse[0]+m.t2*c.tangentImpulse[1];
   if(ma){A.v=A.v-P*A.invMass;A.w=A.w-(c.wnA*c.normalImpulse+c.wtA[0]*c.tangentImpulse[0]+c.wtA[1]*c.tangentImpulse[1]);}
   if(mb){B->v=B->v+P*B->invMass;B->w=B->w+(c.wnB*c.normalImpulse+c.wtB[0]*c.tangentImpulse[0]+c.wtB[1]*c.tangentImpulse[1]);}}
  // (Rolling impulses are stored along the contact's own axes.)
  if(ma)A.w=A.w-(m.rollWA[0]*m.rollingImpulse[0]+m.rollWA[1]*m.rollingImpulse[1]+m.rollWA[2]*m.rollingImpulse[2]);
  if(mb)B->w=B->w+(m.rollWB[0]*m.rollingImpulse[0]+m.rollWB[1]*m.rollingImpulse[1]+m.rollWB[2]*m.rollingImpulse[2]);}
 for(auto*c:constraints)c->warmStart(*this);
}
void Space::solve(float h,bool useBias,bool coldOnly){
 if(!coldOnly)for(auto*c:constraints)c->solve(*this,h,useBias);
 const float inv=1/h;
 const Soft soft=makeSoft(std::min(settings.contactHertz,.25f*inv),settings.contactDampingRatio,h),hard=makeSoft(std::min(2*settings.contactHertz,.25f*inv),settings.worldDampingRatio,h);
 for(auto&m:manifolds){if(coldOnly&&!m.cold)continue;Body& A=bodies[size_t(m.a)];Body* B=m.b>=0?&bodies[size_t(m.b)]:nullptr;const bool ma=A.movable(),mb=B&&B->movable();
  const Soft s=m.surface.hertz>0?makeSoft(std::min(m.surface.hertz,.25f*inv),1.f,h):B&&B->movable()?soft:hard;
  const V3 dpA=A.dp,dpB=B?B->dp:V3{};const Quat dqA=A.dq,dqB=B?B->dq:Quat{};
  float total=0;
  // A face meeting a face (two to four points): first the approach at the manifold's middle, shared evenly by
  // its points. Point by point alone, the first corner solved takes the whole blow and tips the body under the
  // rest: a basket landing flat on four corners came up spinning at 7 rad/s.
  if(m.count>1&&m.midMass>0){
   const V3 dv=velocityOf(B,m.midB,m.surface.velocity)-(A.v+cross(A.w,m.midA));const float vn=dot(dv,m.normal);
   // Still apart (a speculative manifold): only what would close past the nearest point's gap within the substep
   // is the impact's, as the points' own bias leaves it (all of the approach would stop it in the air, then bounce it).
   float gap=0;
   if(vn<-settings.middleThreshold){gap=1e30f;for(int j=0;j<m.count;++j){const auto&c=m.points[size_t(j)];
     const V3 d=(dpB-dpA)+(B?dqB.rotate(c.rB)-dqA.rotate(c.rA):V3{}-dqA.rotate(c.rA))-((B?c.rB:V3{})-c.rA);gap=std::min(gap,dot(d,m.normal)+c.separation);}
    gap=std::max(0.f,gap)*inv;}
   if(vn<-settings.middleThreshold&&vn+gap<0){const float lambda=-(vn+gap)*m.midMass;const V3 P=m.normal*lambda;   // impacts only: resting contacts are the points' own
    if(ma){A.v=A.v-P*A.invMass;A.w=A.w-m.midWA*lambda;}if(mb){B->v=B->v+P*B->invMass;B->w=B->w+m.midWB*lambda;}
    for(int j=0;j<m.count;++j){auto&c=m.points[size_t(j)];c.normalImpulse+=lambda/float(m.count);c.maxNormalImpulse=std::max(c.maxNormalImpulse,lambda/float(m.count));}}}
  for(int k=0;k<m.count;++k){auto&c=m.points[size_t(k)];
   // The gap now, from how far each body has moved and turned since the step began.
   const V3 prA=dqA.rotate(c.rA),prB=B?dqB.rotate(c.rB):c.rB;
   const V3 d=(dpB-dpA)+(B?prB-prA:V3{}-prA)-((B?c.rB:V3{})-c.rA);
   const float sep=dot(d,m.normal)+c.separation;
   float bias=0,massScale=1,impulseScale=0;
   if(sep>0)bias=sep*inv;   // speculative: close the gap within the substep, no more
   else if(useBias){bias=std::max(s.biasRate*std::min(0.f,sep+settings.linearSlop),-settings.pushout);massScale=s.massScale;impulseScale=s.impulseScale;}
   const V3 dv=velocityOf(B,c.rB,m.surface.velocity)-(A.v+cross(A.w,c.rA));const float vn=dot(dv,m.normal);
   float impulse=-c.normalMass*massScale*(vn+bias)-impulseScale*c.normalImpulse;
   const float next=std::max(c.normalImpulse+impulse,0.f);impulse=next-c.normalImpulse;c.normalImpulse=next;c.maxNormalImpulse=std::max(c.maxNormalImpulse,impulse);total+=next;
   const V3 P=m.normal*impulse;
   if(ma){A.v=A.v-P*A.invMass;A.w=A.w-c.wnA*impulse;}
   if(mb){B->v=B->v+P*B->invMass;B->w=B->w+c.wnB*impulse;}}
  // Friction: a cone on each point's two tangent impulses, by this step's normal impulse.
  const float mu=m.material.dynamicFriction;
  for(int k=0;k<m.count;++k){auto&c=m.points[size_t(k)];const V3 dv=velocityOf(B,c.rB,m.surface.velocity)-(A.v+cross(A.w,c.rA));
   const float o1=c.tangentImpulse[0],o2=c.tangentImpulse[1];float n1=o1-c.tangentMass[0]*dot(dv,m.t1),n2=o2-c.tangentMass[1]*dot(dv,m.t2);
   const float limit=mu*c.normalImpulse,mag=std::sqrt(n1*n1+n2*n2);if(mag>limit&&mag>0){n1*=limit/mag;n2*=limit/mag;}
   c.tangentImpulse[0]=n1;c.tangentImpulse[1]=n2;const float d1=n1-o1,d2=n2-o2;const V3 P=m.t1*d1+m.t2*d2;
   if(ma){A.v=A.v-P*A.invMass;A.w=A.w-(c.wtA[0]*d1+c.wtA[1]*d2);}
   if(mb){B->v=B->v+P*B->invMass;B->w=B->w+(c.wtB[0]*d1+c.wtB[1]*d2);}}
  // Rolling and twisting resistance: an angular friction on the relative spin, held to rolling * load.
  // (Run with no load too: a warm-started impulse is then taken back out, not left to spin a body in the air.)
  if(m.material.rolling>0||m.rollingImpulse[0]!=0||m.rollingImpulse[1]!=0||m.rollingImpulse[2]!=0){
   const float limit=m.material.rolling*total;   // the resisting torque: the load times the rolling resistance's length
   for(int r=0;r<3;++r){if(m.rollMass[r]<=0)continue;const V3 axis=r==0?m.t1:r==1?m.t2:m.normal;
    const V3 rel=(B?B->w:V3{})-A.w;const float old=m.rollingImpulse[r];float next=std::clamp(old-dot(rel,axis)*m.rollMass[r],-limit,limit);m.rollingImpulse[r]=next;const float d=next-old;
    if(ma)A.w=A.w-m.rollWA[r]*d;if(mb)B->w=B->w+m.rollWB[r]*d;}}
 }
 // The joints again after the contacts, so a load landing on a jointed body does not leave its joints open
 // (the contacts' last word would).
 if(!coldOnly&&!constraints.empty())for(auto*c:constraints)c->solve(*this,h,useBias);
}
void Space::restitution(){
 // Once per manifold, at the middle of its points weighted by what they took: a body landing flat on four
 // points bounces straight, where a point-by-point pass would twist it (each corner's fix turns the body
 // under the next) and hand it more than its restitution.
 for(auto&m:manifolds){if(m.material.restitution<=0||m.count==0)continue;Body& A=bodies[size_t(m.a)];Body* B=m.b>=0?&bodies[size_t(m.b)]:nullptr;const bool ma=A.movable(),mb=B&&B->movable();
  // The arms carried with each body through the step (c.p is where the points were when it began; the bodies
  // have moved since, and an arm from there to where a sliding body is now would spin it on the bounce).
  float weight=0,approach=0;V3 armA{},armB{};bool struck=false;
  for(int k=0;k<m.count;++k){const auto&c=m.points[size_t(k)];if(c.relativeVelocity>-settings.restitutionThreshold||c.maxNormalImpulse<=0)continue;
   const V3 pA=A.dq.rotate(c.rA),pB=B?B->dq.rotate(c.rB):c.rB;
   {const V3 d=((B?B->dp:V3{})-A.dp)+(B?pB-pA:V3{}-pA)-((B?c.rB:V3{})-c.rA);struck|=dot(d,m.normal)+c.separation<=settings.linearSlop;}
   weight+=c.maxNormalImpulse;armA=armA+pA*c.maxNormalImpulse;armB=armB+pB*c.maxNormalImpulse;approach+=c.relativeVelocity*c.maxNormalImpulse;}
  // Every point still apart when the step ends (a speculative contact that only slowed the approach): nothing has
  // struck yet, and it bounces when it lands, not in the air short of it.
  if(!struck)continue;
  if(weight<=0)continue;approach/=weight;
  const V3 rA=armA*(1/weight),rB=armB*(1/weight);const V3 cA=cross(rA,m.normal),cB=cross(rB,m.normal);
  const float k=(ma?A.invMass+dot(cA,A.invInertia*cA):0.f)+(mb?B->invMass+dot(cB,B->invInertia*cB):0.f);if(k<=0)continue;
  const V3 dv=velocityOf(B,rB,m.surface.velocity)-(A.v+cross(A.w,rA));const float vn=dot(dv,m.normal);
  const float impulse=std::max(0.f,-(vn+m.material.restitution*approach)/k);if(impulse<=0)continue;
  const V3 P=m.normal*impulse;if(ma){A.v=A.v-P*A.invMass;A.w=A.w-A.invInertia*cross(rA,P);}if(mb){B->v=B->v+P*B->invMass;B->w=B->w+B->invInertia*cross(rB,P);}
  for(int k2=0;k2<m.count;++k2){auto&c=m.points[size_t(k2)];c.normalImpulse+=impulse*(c.maxNormalImpulse/weight);c.maxNormalImpulse=std::max(c.maxNormalImpulse,c.normalImpulse);}}
}
void Space::storeImpulses(){
 std::vector<std::pair<std::uint64_t,Cached>> next;next.reserve(manifolds.size()+cache.size());
 for(const auto&m:manifolds){Cached c;c.count=m.count;for(int k=0;k<m.count;++k){const auto&p=m.points[size_t(k)];c.points[size_t(k)]={p.localA,p.normalImpulse,{p.tangentImpulse[0],p.tangentImpulse[1]}};}for(int r=0;r<3;++r)c.rolling[r]=m.rollingImpulse[r];next.push_back({keyOf(m),c});}
 // What a sleeping body held (its contacts are not built while it sleeps) is kept for when it wakes: a stack
 // woken by a shot then starts from the impulses it slept on, not from nothing.
 {std::vector<int> awake;for(int i=0;i<int(bodies.size());++i)if(!bodies[size_t(i)].sleeping)awake.push_back(idOf(i));std::sort(awake.begin(),awake.end());
  auto up=[&](int id){return id>=0&&std::binary_search(awake.begin(),awake.end(),id);};
  for(const auto&e:cache){const int idA=int(e.first>>40),idB=int((e.first>>20)&0xfffff)-1;if(!up(idA)&&!up(idB))next.push_back(e);}}
 std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.first<b.first;});
 // Two manifolds with one key (two world clusters a body could not tell apart): keep the first.
 next.erase(std::unique(next.begin(),next.end(),[](const auto& a,const auto& b){return a.first==b.first;}),next.end());
 cache.swap(next);
 for(auto*c:constraints)c->storeImpulses();
}
void Space::integrateVelocities(float h){
 for(auto&b:bodies){if(!b.movable())continue;
  b.v=b.v+(b.gravity+b.force*b.invMass)*h;b.w=b.w+(b.invInertia*b.torque)*h;
  // The gyroscopic torque, implicitly (one Newton step in the body frame): a long thin body spun off
  // its axis keeps its angular momentum instead of gaining energy from the turning inertia tensor.
  {const V3 w=b.q.unrotate(b.w);const M3& I=b.inertiaLocal;const V3 Iw=I*w;const V3 f=cross(w,Iw)*h;
   auto skew=[](V3 a){M3 m=M3::zero();m.m[0][1]=-a.z;m.m[0][2]=a.y;m.m[1][0]=a.z;m.m[1][2]=-a.x;m.m[2][0]=-a.y;m.m[2][1]=a.x;return m;};
   const M3 J=I+(skew(w)*I+skew(Iw)*-1.f)*h;const M3 Ji=J.inverse();b.w=b.q.rotate(w-Ji*f);}
  b.v=b.v*(1/(1+h*b.linearDamping));b.w=b.w*(1/(1+h*b.angularDamping));
  const float sv=len(b.v),sw=len(b.w);if(sv>settings.maxLinear)b.v=b.v*(settings.maxLinear/sv);if(sw>settings.maxAngular)b.w=b.w*(settings.maxAngular/sw);}
}
void Space::integratePositions(float h){
 for(auto&b:bodies){if(!b.movable())continue;
  b.x=b.x+b.v*h;b.q=integrate(b.q,b.w,h);b.dp=b.dp+b.v*h;b.dq=integrate(b.dq,b.w,h);}
}
// Fast bodies swept through the step, turning as well as going, against the world and each other (Queries.h).
void Space::continuous(float dt,const std::vector<V3>& start){continuousCollision(*this,dt,start);}
void Space::islands(float dt){
 const int n=int(bodies.size());std::vector<int> parent(static_cast<size_t>(n));for(int i=0;i<n;++i)parent[size_t(i)]=i;
 auto find=[&](int i){while(parent[size_t(i)]!=i){parent[size_t(i)]=parent[size_t(parent[size_t(i)])];i=parent[size_t(i)];}return i;};
 auto unite=[&](int a,int b){a=find(a);b=find(b);if(a!=b)parent[size_t(std::max(a,b))]=std::min(a,b);};
 std::vector<char> pinned(size_t(n),0);
 for(const auto&m:manifolds){if(m.b<0)continue;const auto&a=bodies[size_t(m.a)];const auto&b=bodies[size_t(m.b)];
  if(a.movable()&&b.movable())unite(m.a,m.b);
  // Touching a moving held body keeps it awake.
  if(a.kinematic&&(lenSq(a.v)>1e-6f||lenSq(a.w)>1e-6f))pinned[size_t(m.b)]=1;if(b.kinematic&&(lenSq(b.v)>1e-6f||lenSq(b.w)>1e-6f))pinned[size_t(m.a)]=1;}
 for(auto*c:constraints){const int a=c->bodyA(),b=c->bodyB();if(a>=0&&b>=0&&bodies[size_t(a)].movable()&&bodies[size_t(b)].movable())unite(a,b);}
 islandOf.assign(size_t(n),-1);std::vector<float> least(size_t(n),1e30f);
 for(int i=0;i<n;++i){auto&b=bodies[size_t(i)];if(!b.movable())continue;
  const float speed=std::max(len(b.v),len(b.w)*b.shape->bound);
  if(speed<settings.sleepLinear&&len(b.w)<settings.sleepAngular&&!pinned[size_t(i)])b.sleepTime+=dt;else b.sleepTime=0;
  const int r=find(i);islandOf[size_t(i)]=r;least[size_t(r)]=std::min(least[size_t(r)],b.sleepTime);}
 for(int i=0;i<n;++i){auto&b=bodies[size_t(i)];if(!b.movable())continue;if(least[size_t(find(i))]>=settings.sleepTime){b.sleeping=true;b.v={};b.w={};}}
}
void Space::drive(int i,V3 targetX,Quat targetQ,float linearHertz,float angularHertz,float dt,float maxForce,float maxTorque){
 auto&b=bodies[size_t(i)];if(b.invMass<=0||dt<=0)return;b.sleeping=false;b.sleepTime=0;
 const float wl=2*kPi*linearHertz,wa=2*kPi*angularHertz;
 // A critically damped spring, stepped implicitly: the velocity it asks for, held to maxForce.
 const V3 want=(targetX-b.x)*(wl/(1+wl*dt));V3 dv=want-b.v;const float cap=maxForce*b.invMass*dt,l=len(dv);if(l>cap&&l>0)dv=dv*(cap/l);b.v=b.v+dv;
 Quat d=targetQ*b.q.conj();if(d.w<0)d={-d.w,-d.x,-d.y,-d.z};const float s=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
 const V3 axis=s>1e-6f?V3{d.x/s,d.y/s,d.z/s}:V3{};const float angle=2*std::atan2(s,d.w);
 // The spin it asks for, held to maxTorque through the body's inertia (the angular impulse I dw over the step). Set
 // outright, a long plank pressed into a doorframe was levered by whatever the spring wanted, past what any contact
 // could answer in one step: it jittered against the frame or went through it.
 const V3 wantW=axis*(angle*wa/(1+wa*dt));const V3 dw=wantW-b.w;
 if(maxTorque<0)maxTorque=maxForce*(b.shape?std::max(b.shape->bound,.05f):.5f);
 b.updateInertia();const M3 I=b.q.matrix()*b.inertiaLocal*b.q.matrix().transposed();V3 L=I*dw;const float lc=maxTorque*dt,ll=len(L);
 if(ll>lc&&ll>0)L=L*(lc/ll);b.w=b.w+b.invInertia*L;}
void Space::step(float dt){
 events.clear();jointEvents.clear();if(dt<=0||bodies.empty()){manifolds.clear();return;}
 // A joint does not hold a moving body to a sleeping one: what is joined to something awake wakes (through
 // the whole chain).
 for(bool woke=true;woke;){woke=false;for(auto*c:constraints){const int a=c->bodyA(),b=c->bodyB();if(a<0||b<0)continue;if(auto*j=c->joint();j&&j->broken)continue;
   auto&A=bodies[size_t(a)];auto&B=bodies[size_t(b)];
   // A held (kinematic) body on the move wakes what it is joined to, as one touching it does (islands): its joint
   // solves nothing while the other sleeps, and it would pull away.
   auto moving=[](const Body& k){return k.kinematic&&(lenSq(k.v)>1e-6f||lenSq(k.w)>1e-6f);};
   if(moving(A)&&B.sleeping&&B.invMass>0){wake(b);woke=true;continue;}if(moving(B)&&A.sleeping&&A.invMass>0){wake(a);woke=true;continue;}
   if(A.invMass<=0||B.invMass<=0)continue;
   if(A.sleeping!=B.sleeping){wake(A.sleeping?a:b);woke=true;}}}
 // A motor at work keeps what it drives awake.
 for(auto*c:constraints)if(auto*j=c->joint();j&&j->driving(*this))for(int i:{j->a,j->b})if(i>=0&&bodies[size_t(i)].invMass>0){wake(i);bodies[size_t(i)].sleepTime=0;}
 const int sub=std::max(1,settings.substeps);const float h=dt/float(sub);
 for(auto&b:bodies){if(b.shape&&b.invMass>0)b.updateInertia();b.dp={};b.dq={};}
 collide(dt);
 const auto s0=std::chrono::steady_clock::now();
 prepare(h);for(auto*c:constraints)c->prepare(*this,h);
 std::vector<V3> start;start.reserve(bodies.size());for(const auto&b:bodies)start.push_back(b.x);
 std::vector<float> total(manifolds.size()*4,0.f);
 for(int s=0;s<sub;++s){
  integrateVelocities(h);warmStart();solve(h,true);
  if(s==0&&settings.coldIterations>0&&std::any_of(manifolds.begin(),manifolds.end(),[](const Manifold& m){return m.cold;}))for(int k=0;k<settings.coldIterations;++k)solve(h,true,true);
  integratePositions(h);
  for(int r=0;r<settings.relaxIterations;++r)solve(h,false);
  for(size_t m=0;m<manifolds.size();++m)for(int k=0;k<manifolds[m].count;++k)total[m*4+size_t(k)]+=manifolds[m].points[size_t(k)].normalImpulse;}
 restitution();storeImpulses();for(auto*c:constraints)c->finish(*this,h);solveMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s0).count();
 for(auto&b:bodies){b.force={};b.torque={};}
 // What struck what: the approach speed and the impulse it took (sounds, breaking, glass).
 for(size_t mi=0;mi<manifolds.size();++mi){const auto&m=manifolds[mi];float approach=0,impulse=0,mass=0;V3 at{};
  for(int k=0;k<m.count;++k){const auto&c=m.points[size_t(k)];impulse+=total[mi*4+size_t(k)];if(-c.relativeVelocity>approach){approach=-c.relativeVelocity;at=c.p+m.normal*(std::max(0.f,c.separation)*(m.b<0?1.f:.5f));mass=c.normalMass;}}   // where the surfaces met
  // A face landing flat brings the body's whole mass at the middle, not one corner's share.
  if(m.count>1&&m.midMass>mass)mass=m.midMass;
  if(approach>.3f)events.push_back({m.a,m.b,m.partA,m.partB,at,m.normal,approach,impulse,mass,m.surface});}
 continuous(dt,start);
 // What the substeps left open of a joint that asks for it (a ragdoll's), put back by position.
 for(int it=0;it<settings.projectIterations;++it)for(auto*c:constraints)c->project(*this);
 islands(dt);
 ++iterationsRun;
}
}

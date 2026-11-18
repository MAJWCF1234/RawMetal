#include "Carcass.h"
#include "../core/Tune.h"
#include "../game/physics/Vectra.h"
#include <deque>
#include <memory>
#include <string>
#include <algorithm>
#include <array>
#include <chrono>
#include <tuple>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <numeric>
#include <queue>
#include <unordered_map>
namespace retro {
namespace {
constexpr float Tick=1.f/60,Skin=.012f;
// How the dead go down and lie (Carcass::step). Each knob is read live in the lab (lab/tune.txt,
// carcass.NAME) and is the default given here everywhere else.
// Death tone (the pose a body died in held, fading, for its first moments) is built but off: held stiff,
// a man fell down a stair as a plank and went on tumbling at the bottom. A standing man still goes over
// from the hips, because his feet grip the floor. Joints turn against friction so a limb lands, not flops.
float ToneStart=0,ToneTime=.9f,JointFriction=.12f,TurnCap=.25f;
// Each step is cut into SubSteps, each solved in Passes; the floor grips (Coulomb, by position: a point the
// floor pushed back by `depth` does not move along it while that move is under StaticGrip*depth+Hold; past
// it, it slides, slowed by KineticGrip). Lying on the floor a section loses its motion at GroundDamp a
// second, and one barely moving (RestSpeed) stops. MaxLift: the most the floor lifts a point a pass (a
// step's edge is climbed, not leapt). CollideStep: the most two sections are pushed apart a pass. A body
// still moving FadeFrom seconds after it fell runs down at up to FadeDamp a second.
int Passes=4,SubSteps=7,VelocityRounds=3;
float QuietFor=.3f,SettleFeed=.008f,WallLook=.03f,StaticGrip=.9f,KineticGrip=.8f,Hold=.0015f,GroundDamp=8,ProbeStep=.01f,RestSpeed=.08f,CollideStep=.05f,MaxLift=.02f,FadeDamp=4,FadeFrom=2.5f;
void knobs(){ToneStart=tune("carcass.tone",0);ToneTime=tune("carcass.toneTime",.9f);JointFriction=tune("carcass.jointFriction",.12f);TurnCap=tune("carcass.turnCap",.25f);
 Passes=std::max(1,int(tune("carcass.passes",4)));SubSteps=std::max(1,int(tune("carcass.substeps",7)));VelocityRounds=std::max(1,int(tune("carcass.vrounds",3)));
 StaticGrip=tune("carcass.static",.9f);KineticGrip=tune("carcass.kinetic",.8f);Hold=tune("carcass.hold",.0015f);GroundDamp=tune("carcass.groundDamp",8);ProbeStep=tune("carcass.probe",.01f);
 RestSpeed=tune("carcass.rest",.08f);CollideStep=tune("carcass.collideStep",.05f);MaxLift=tune("carcass.maxLift",.02f);FadeDamp=tune("carcass.fade",4);FadeFrom=tune("carcass.fadeFrom",2.5f);WallLook=tune("carcass.wallLook",.03f);QuietFor=tune("carcass.quietFor",.3f);SettleFeed=tune("carcass.settleFeed",.008f);}
long long Steps=0;double StepMs=0;
}
// A body's state on Vectra (stepVectra): its sections' bodies, kept while it lasts, and its joints.
struct Carcass::VectraState {
 vectra::Space space;std::deque<vectra::Shape> shapes,solidShapes;std::vector<vectra::Joint> joints;std::vector<int> bodyOf;std::vector<char> paired;
 int sections=0,cuts=-1;bool built=false,kicked=false,wake=false;
 // Each section's outermost skin (rest coordinates): the vertices farthest out over many ways, which meet the floor and
 // the walls along with its points, so no corner of its mesh goes into them between the points.
 struct SkinPoint {Point3 rest{},probe{1e9f,1e9f,1e9f},safe{};float floor=-100;};std::vector<std::vector<SkinPoint>> skin;};
namespace {
// RM_CARCASS clears features for comparison: bit 1 no joint limits, 2 no death tone, 4 no self-collision,
// 8 the old straight seam blend, 16 no solids (props, movers), 32 no hull points, 64 no sweep of fast points and walls hold only points once clear of them by their reach (as before)
// (walls are looked at every carcass.wallLook, 3 cm), 128 no settling phase (settled in one frame). RM_CARCASS=248 and RM_CORPSE_ROUNDS=0 is the body as it was.
int switchedOff(){static const int off=[]{const char*v=std::getenv("RM_CARCASS");return v?std::atoi(v):0;}();return off;}
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross3(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float span(Point3 a){return std::sqrt(dot3(a,a));}
Point3 unit(Point3 a,Point3 fallback={0,0,1}){float s=span(a);return s>1e-6f?a*(1/s):fallback;}
float chance(unsigned& s){s=s*1664525u+1013904223u;return float((s>>8)&0xffffu)/65535.f;}
float either(unsigned& s){return chance(s)*2-1;}
void axesOf(const float q[4],Point3 r[3]){float w=q[0],x=q[1],y=q[2],z=q[3];
 r[0]={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)};r[1]={2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)};r[2]={2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)};}
// Quaternions as {w,x,y,z}.
void qmul(const float a[4],const float b[4],float o[4]){float r[4]{a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3],a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2],a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1],a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0]};for(int i=0;i<4;++i)o[i]=r[i];}
void qconj(const float a[4],float o[4]){o[0]=a[0];o[1]=-a[1];o[2]=-a[2];o[3]=-a[3];}
void qnorm(float q[4]){float s=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);if(!(s>1e-9f)||!std::isfinite(s)){q[0]=1;q[1]=q[2]=q[3]=0;return;}for(int i=0;i<4;++i)q[i]/=s;}
Point3 qrot(const float q[4],Point3 v){Point3 r[3];axesOf(q,r);return r[0]*v.x+r[1]*v.y+r[2]*v.z;}
// The rotation vector (axis times angle) of q, and back.
Point3 qlog(const float q[4]){float w=q[0];Point3 v{q[1],q[2],q[3]};if(w<0){w=-w;v=v*-1.f;}float s=span(v);if(s<1e-7f)return v*2.f;return v*(2.f*std::atan2(s,w)/s);}
void qexp(Point3 r,float o[4]){float a=span(r);if(a<1e-7f){o[0]=1;o[1]=r.x*.5f;o[2]=r.y*.5f;o[3]=r.z*.5f;qnorm(o);return;}float s=std::sin(a*.5f)/a;o[0]=std::cos(a*.5f);o[1]=r.x*s;o[2]=r.y*s;o[3]=r.z*s;}
// From a toward b by t (along the shorter way).
void qslerp(const float a[4],const float b[4],float t,float o[4]){float ia[4],d[4];qconj(a,ia);qmul(b,ia,d);qexp(qlog(d)*t,d);qmul(d,a,o);qnorm(o);}
// The rotation that best carries the points `from` onto `to` (Horn: the top eigenvector of his 4x4, by Jacobi).
void bestRotation(const std::vector<Point3>& from,const std::vector<Point3>& to,float q[4]){
 q[0]=1;q[1]=q[2]=q[3]=0;const size_t n=std::min(from.size(),to.size());if(n<3)return;Point3 cf{},ct{};for(size_t i=0;i<n;++i){cf=cf+from[i];ct=ct+to[i];}cf=cf*(1.f/float(n));ct=ct*(1.f/float(n));
 double S[3][3]{};for(size_t i=0;i<n;++i){Point3 a=from[i]-cf,b=to[i]-ct;const float A[3]{a.x,a.y,a.z},B[3]{b.x,b.y,b.z};for(int r=0;r<3;++r)for(int c=0;c<3;++c)S[r][c]+=double(A[r])*B[c];}
 double N[4][4]{{S[0][0]+S[1][1]+S[2][2],S[1][2]-S[2][1],S[2][0]-S[0][2],S[0][1]-S[1][0]},{S[1][2]-S[2][1],S[0][0]-S[1][1]-S[2][2],S[0][1]+S[1][0],S[2][0]+S[0][2]},
  {S[2][0]-S[0][2],S[0][1]+S[1][0],-S[0][0]+S[1][1]-S[2][2],S[1][2]+S[2][1]},{S[0][1]-S[1][0],S[2][0]+S[0][2],S[1][2]+S[2][1],-S[0][0]-S[1][1]+S[2][2]}};
 double E[4][4]{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
 for(int sweep=0;sweep<24;++sweep){double off=0;for(int p=0;p<4;++p)for(int r=p+1;r<4;++r)off+=N[p][r]*N[p][r];if(off<1e-18)break;
  for(int p=0;p<4;++p)for(int r=p+1;r<4;++r){if(std::fabs(N[p][r])<1e-30)continue;double th=(N[r][r]-N[p][p])/(2*N[p][r]),t=(th>=0?1:-1)/(std::fabs(th)+std::sqrt(th*th+1)),c=1/std::sqrt(t*t+1),s=t*c;
   for(int k=0;k<4;++k){double a=N[k][p],b=N[k][r];N[k][p]=c*a-s*b;N[k][r]=s*a+c*b;}for(int k=0;k<4;++k){double a=N[p][k],b=N[r][k];N[p][k]=c*a-s*b;N[r][k]=s*a+c*b;}
   for(int k=0;k<4;++k){double a=E[k][p],b=E[k][r];E[k][p]=c*a-s*b;E[k][r]=s*a+c*b;}}}
 int top=0;for(int k=1;k<4;++k)if(N[k][k]>N[top][top])top=k;for(int k=0;k<4;++k)q[k]=float(E[k][top]);qnorm(q);if(q[0]<0)for(int k=0;k<4;++k)q[k]=-q[k];}
struct Groups {std::vector<int> parent;explicit Groups(int n):parent(size_t(n)){std::iota(parent.begin(),parent.end(),0);}
 int find(int i){while(parent[size_t(i)]!=i){parent[size_t(i)]=parent[size_t(parent[size_t(i)])];i=parent[size_t(i)];}return i;}
 bool join(int a,int b){a=find(a);b=find(b);if(a==b)return false;parent[size_t(std::max(a,b))]=std::min(a,b);return true;}};
// A rotation's turn about `axis`, and what is left (the swing), as the bend about `bendAxis` and the
// sideways turn about `sideAxis`.
struct Turn {float bend=0,side=0,twist=0;};
Turn decompose(const float r[4],Point3 axis,Point3 bendAxis,Point3 sideAxis){
 const float along=r[1]*axis.x+r[2]*axis.y+r[3]*axis.z;float tw[4]{r[0],axis.x*along,axis.y*along,axis.z*along};
 Turn t;if(tw[0]*tw[0]+along*along<1e-12f){tw[0]=1;tw[1]=tw[2]=tw[3]=0;}else qnorm(tw);
 t.twist=2*std::atan2(tw[1]*axis.x+tw[2]*axis.y+tw[3]*axis.z,tw[0]);if(t.twist>kPi)t.twist-=kTwoPi;if(t.twist<-kPi)t.twist+=kTwoPi;
 float itw[4],sw[4];qconj(tw,itw);qmul(r,itw,sw);const Point3 v=qlog(sw);t.bend=dot3(v,bendAxis);t.side=dot3(v,sideAxis);return t;}
}
long long Carcass::stepsTaken(){return Steps;}
double Carcass::stepMilliseconds(){return StepMs;}
int Carcass::joints(bool seamsOnly)const{int n=0;for(auto&j:m_joints)if(!seamsOnly||j.seam)++n;return n;}
Point3 Carcass::carry(const Section& s,Point3 rest)const{Point3 d=rest-s.restCentre;return s.centre+s.axes[0]*d.x+s.axes[1]*d.y+s.axes[2]*d.z;}
bool Carcass::neighbours(int a,int b)const{for(auto&j:m_joints)if(!j.cut&&((j.a==a&&j.b==b)||(j.a==b&&j.b==a)))return true;return false;}

void Carcass::build(const std::vector<MeshTriangle>& posed,const World& world,Point3 push,float scale,unsigned seed,float reach,const Rig* rig){
 *this=Carcass{};if(posed.empty())return;
 if(rig&&(rig->part.size()!=posed.size()||rig->bind.size()!=posed.size()||rig->limits.empty()))rig=nullptr;
 m_triangles=posed;m_scale=scale;m_seed=seed*2654435761u|1u;m_reach=std::max(0.f,reach);m_rigged=rig!=nullptr;
 const int T=int(posed.size());m_corner.resize(size_t(T)*3);
 // Corners that share a place are one vertex, whatever the UV seams say.
 std::vector<Point3> bindAt;
 {std::unordered_map<std::uint64_t,int> cells;cells.reserve(size_t(T)*2);
  auto cell=[](Point3 p){auto q=[](float v){return std::uint64_t(std::int64_t(std::floor(v*500.f+.5f)))&0x1fffffu;};return q(p.x)|q(p.y)<<21|q(p.z)<<42;};
  for(int t=0;t<T;++t)for(int k=0;k<3;++k){auto p=posed[size_t(t)].v[k].p;auto [slot,fresh]=cells.try_emplace(cell(p),int(m_rest.size()));if(fresh){m_rest.push_back(p);if(rig)bindAt.push_back(rig->bind[size_t(t)].v[k].p);}m_corner[size_t(t*3+k)]=slot->second;}}
 const int V=int(m_rest.size());
 Point3 lo=m_rest[0],hi=lo;for(auto p:m_rest){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
 m_size=std::max(.05f,span(hi-lo));const Point3 mid=(lo+hi)*.5f;
 std::vector<std::vector<int>> around(static_cast<size_t>(V));
 for(int t=0;t<T;++t)for(int k=0;k<3;++k){int a=m_corner[size_t(t*3+k)],b=m_corner[size_t(t*3+(k+1)%3)];if(a!=b){around[size_t(a)].push_back(b);around[size_t(b)].push_back(a);}}
 // The part each vertex is, when the body was told.
 std::vector<int> vertexPart;
 if(rig){vertexPart.assign(size_t(V),-1);for(int t=0;t<T;++t)for(int k=0;k<3;++k){int&p=vertexPart[size_t(m_corner[size_t(t*3+k)])];if(p<0)p=rig->part[size_t(t)];}
  // The part most of the triangles round it are (the first of them on a tie): a corner where a thigh meets the hips
  // goes with the side that has more of it, so fewer triangles are pulled by a part they are not.
  if(tune("carcass.majorityHome",0)>.5f){const int P=int(rig->limits.size());std::vector<std::uint16_t> votes(size_t(V)*size_t(P),0);
   for(int t=0;t<T;++t){const int part=rig->part[size_t(t)];if(part<0||part>=P)continue;for(int k=0;k<3;++k)++votes[size_t(m_corner[size_t(t*3+k)])*size_t(P)+size_t(part)];}
   for(int v=0;v<V;++v){int best=vertexPart[size_t(v)];if(best<0||best>=P)continue;for(int q=0;q<P;++q)if(votes[size_t(v)*size_t(P)+size_t(q)]>votes[size_t(v)*size_t(P)+size_t(best)])best=q;vertexPart[size_t(v)]=best;}}}
 // The body is a cloud of points spread evenly over its skin.
 std::vector<int> seat;{std::vector<float> reachTo(size_t(V),1e30f);int pick=0;float far=-1;const int wanted=std::min(160,V);
  for(int v=0;v<V;++v){float d=span(m_rest[size_t(v)]-mid);if(d>far){far=d;pick=v;}}
  for(int i=0;i<wanted;++i){seat.push_back(pick);far=-1;int next=pick;const Point3 at=m_rest[size_t(pick)];
   for(int v=0;v<V;++v){float d=span(m_rest[size_t(v)]-at);if(d<reachTo[size_t(v)])reachTo[size_t(v)]=d;if(reachTo[size_t(v)]>far){far=reachTo[size_t(v)];next=v;}}pick=next;}
  // Told its parts, every part with skin enough gets points enough to turn on its own.
  if(rig)for(int p=0;p<int(rig->limits.size());++p){int have=0,skin=0;for(int s:seat)have+=vertexPart[size_t(s)]==p;for(int v=0;v<V;++v)skin+=vertexPart[size_t(v)]==p;if(skin<8)continue;
   while(have<5){int best=-1;float most=-1;for(int v=0;v<V;++v){if(vertexPart[size_t(v)]!=p)continue;float d=1e30f;for(int s:seat)if(vertexPart[size_t(s)]==p)d=std::min(d,span(m_rest[size_t(s)]-m_rest[size_t(v)]));if(have==0)d=span(m_rest[size_t(v)]-mid);if(d>most){most=d;best=v;}}
    if(best<0||most<=0)break;seat.push_back(best);++have;}}}
 const int N=int(seat.size());m_body=N;
 m_particles.resize(size_t(N));for(int i=0;i<N;++i){auto&p=m_particles[size_t(i)];p.rest=p.x=p.previous=m_rest[size_t(seat[size_t(i)])];}
 // Each vertex belongs to the point nearest it along the skin, so a leg never answers to the other leg.
 std::vector<int> near(size_t(V),-1);{std::vector<float> walk(size_t(V),1e30f);using Item=std::pair<float,int>;std::priority_queue<Item,std::vector<Item>,std::greater<Item>> open;
  for(int i=0;i<N;++i){int v=seat[size_t(i)];if(near[size_t(v)]>=0)continue;walk[size_t(v)]=0;near[size_t(v)]=i;open.push({0.f,v});}
  while(!open.empty()){auto [d,v]=open.top();open.pop();if(d>walk[size_t(v)])continue;
   for(int u:around[size_t(v)]){float nd=d+span(m_rest[size_t(u)]-m_rest[size_t(v)]);if(nd<walk[size_t(u)]){walk[size_t(u)]=nd;near[size_t(u)]=near[size_t(v)];open.push({nd,u});}}}
  for(int v=0;v<V;++v)if(near[size_t(v)]<0){float best=1e30f;for(int i=0;i<N;++i){float d=span(m_particles[size_t(i)].rest-m_rest[size_t(v)]);if(d<best){best=d;near[size_t(v)]=i;}}}}
 std::vector<char> linked(size_t(N)*size_t(N),0);auto link=[&](int a,int b)->char&{return linked[size_t(a)*size_t(N)+size_t(b)];};
 for(int v=0;v<V;++v)for(int u:around[size_t(v)]){int a=near[size_t(v)],b=near[size_t(u)];if(a!=b){link(a,b)=1;link(b,a)=1;}}
 // Shells: a model is often several skins that only touch.
 Groups shells(N);for(int a=0;a<N;++a)for(int b=a+1;b<N;++b)if(link(a,b))shells.join(a,b);
 std::vector<int> shellSize(size_t(N),0);for(int i=0;i<N;++i)++shellSize[size_t(shells.find(i))];
 int majors=0;for(int i=0;i<N;++i)if(shells.find(i)==i&&shellSize[size_t(i)]>=4)++majors;
 auto major=[&](int i){return majors==0||shellSize[size_t(shells.find(i))]>=4;};
 std::vector<int> region(size_t(N),-1);std::vector<int> partOfRegion;
 if(rig){
  // Told: each point is the part its vertex is. A part with too few points to turn steadily rides the
  // part it hangs from (a hand its forearm).
  const int P=int(rig->limits.size());std::vector<int> into(static_cast<size_t>(P));std::iota(into.begin(),into.end(),0);std::vector<int> count(size_t(P),0);
  for(int i=0;i<N;++i){int p=vertexPart[size_t(seat[size_t(i)])];if(p>=0&&p<P)++count[size_t(p)];}
  for(int p=P-1;p>=0;--p)if(count[size_t(p)]<4){int up=rig->limits[size_t(p)].parent;if(up>=0&&up<P){into[size_t(p)]=up;count[size_t(up)]+=count[size_t(p)];}}
  auto resolve=[&](int p){if(p<0||p>=P)return 0;for(int guard=0;guard<P&&into[size_t(p)]!=p;++guard)p=into[size_t(p)];return p;};
  std::vector<int> label(size_t(P),-1);
  for(int i=0;i<N;++i){int p=resolve(vertexPart[size_t(seat[size_t(i)])]);if(label[size_t(p)]<0){label[size_t(p)]=int(partOfRegion.size());partOfRegion.push_back(p);}region[size_t(i)]=label[size_t(p)];}
  for(int&p:vertexPart){p=resolve(p);if(label[size_t(p)]<0){label[size_t(p)]=-1;}}
  // A vertex whose part has no points rides the region of the point nearest it.
  for(int v=0;v<V;++v)vertexPart[size_t(v)]=label[size_t(vertexPart[size_t(v)])]>=0?label[size_t(vertexPart[size_t(v)])]:region[size_t(near[size_t(v)])];
 }else{
  // Sections grow outward along the skin from seeds set as far apart as they will go: the ends of
  // limbs first, then the trunk between them.
  std::vector<float> away(size_t(N),1e30f),d(size_t(N),0.f);std::vector<char> done(static_cast<size_t>(N));
  int wanted=std::min({16,std::clamp(N/14,3,12)+std::max(0,majors-1),std::max(1,N/4)});const float least=.085f*m_size;
  for(int s=0,made=0;s<wanted;++s){int from=-1;float far=-1;
   for(int i=0;i<N;++i)if(major(i)&&away[size_t(i)]>far){far=away[size_t(i)];from=i;}
   if(from<0||far<least)break;
   std::fill(d.begin(),d.end(),1e30f);std::fill(done.begin(),done.end(),char(0));d[size_t(from)]=0;
   for(;;){int best=-1;for(int i=0;i<N;++i)if(!done[size_t(i)]&&d[size_t(i)]<1e29f&&(best<0||d[size_t(i)]<d[size_t(best)]))best=i;if(best<0)break;done[size_t(best)]=1;
    for(int j=0;j<N;++j)if(link(best,j)){float nd=d[size_t(best)]+span(m_particles[size_t(j)].rest-m_particles[size_t(best)].rest);if(nd<d[size_t(j)])d[size_t(j)]=nd;}}
   for(int i=0;i<N;++i)if(d[size_t(i)]<away[size_t(i)]){away[size_t(i)]=d[size_t(i)];region[size_t(i)]=made;}
   ++made;}
  auto nearestOther=[&](int i,int avoid){int found=-1;float best=1e30f;for(int j=0;j<N;++j){if(region[size_t(j)]<0||region[size_t(j)]==avoid)continue;float dd=span(m_particles[size_t(j)].rest-m_particles[size_t(i)].rest);if(dd<best){best=dd;found=region[size_t(j)];}}return found;};
  for(int i=0;i<N;++i)if(region[size_t(i)]<0){int r=nearestOther(i,-1);region[size_t(i)]=r<0?0:r;}
  // A section too small to turn steadily joins the neighbour it touches most.
  for(int guard=0;guard<32;++guard){std::array<int,32> size{};for(int r:region)++size[size_t(r)];int live=0,small=-1;for(int r=0;r<32;++r){if(size[size_t(r)])++live;if(size[size_t(r)]&&size[size_t(r)]<4&&small<0)small=r;}
   if(small<0||live<2)break;std::array<int,32> touch{};for(int a=0;a<N;++a)if(region[size_t(a)]==small)for(int b=0;b<N;++b)if(link(a,b)&&region[size_t(b)]!=small)++touch[size_t(region[size_t(b)])];
   int target=int(std::max_element(touch.begin(),touch.end())-touch.begin());if(!touch[size_t(target)]){for(int a=0;a<N;++a)if(region[size_t(a)]==small){target=nearestOther(a,small);break;}}
   if(target<0||target==small)break;for(int&r:region)if(r==small)r=target;}
  int R=0;{std::array<int,32> label;label.fill(-1);for(int&r:region){if(label[size_t(r)]<0)label[size_t(r)]=R++;r=label[size_t(r)];}}
  partOfRegion.assign(size_t(R),-1);
 }
 const int R=int(partOfRegion.size());
 for(int i=0;i<N;++i)m_particles[size_t(i)].region=region[size_t(i)];
 m_binds.resize(size_t(V));for(int v=0;v<V;++v)m_binds[size_t(v)].home=rig?vertexPart[size_t(v)]:region[size_t(near[size_t(v)])];
 m_owner.resize(size_t(T));
 for(int t=0;t<T;++t){int h[3];for(int k=0;k<3;++k)h[k]=m_binds[size_t(m_corner[size_t(t*3+k)])].home;m_owner[size_t(t)]=h[1]==h[2]?h[1]:h[0];}
 // Seams: the edges where one section's triangles meet another's. Each is a joint while it holds
 // and the rim of a wound once it is cut.
 std::vector<Point3> seamSum(size_t(R)*size_t(R));std::vector<int> seamCount(size_t(R)*size_t(R),0);
 {std::unordered_map<std::uint64_t,std::array<int,2>> sides;sides.reserve(size_t(T)*2);std::vector<std::uint64_t> order;order.reserve(size_t(T)*2);
  for(int t=0;t<T;++t)for(int k=0;k<3;++k){int a=m_corner[size_t(t*3+k)],b=m_corner[size_t(t*3+(k+1)%3)];if(a==b)continue;std::uint64_t key=std::uint64_t(std::uint32_t(std::min(a,b)))<<32|std::uint32_t(std::max(a,b));
   auto [slot,fresh]=sides.try_emplace(key,std::array<int,2>{-1,-1});if(fresh){slot->second[0]=t;order.push_back(key);}else if(slot->second[1]<0)slot->second[1]=t;}
  for(auto key:order){auto pair=sides[key];int a=int(key>>32),b=int(key&0xffffffffu),first=m_owner[size_t(pair[0])];
   if(pair[1]<0){m_edges.push_back({a,b,first,-1});continue;}int second=m_owner[size_t(pair[1])];if(first==second)continue;
   m_edges.push_back({a,b,first,second});m_edges.push_back({a,b,second,first});size_t at=size_t(std::min(first,second))*size_t(R)+size_t(std::max(first,second));
   seamSum[at]=seamSum[at]+(m_rest[size_t(a)]+m_rest[size_t(b)])*.5f;++seamCount[at];}}
 Groups whole(R);
 for(int a=0;a<R;++a)for(int b=a+1;b<R;++b){size_t at=size_t(a)*size_t(R)+size_t(b);if(!seamCount[at])continue;Joint j;j.a=a;j.b=b;j.seam=true;j.rest=seamSum[at]*(1.f/float(seamCount[at]));m_joints.push_back(j);whole.join(a,b);}
 // Skins that only touch are pinned where they come closest.
 for(int guard=0;guard<R;++guard){float best=1e30f;int pa=-1,pb=-1;
  for(int a=0;a<N;++a)for(int b=a+1;b<N;++b){if(whole.find(region[size_t(a)])==whole.find(region[size_t(b)]))continue;float d=span(m_particles[size_t(a)].rest-m_particles[size_t(b)].rest);if(d<best){best=d;pa=a;pb=b;}}
  if(pa<0)break;Joint j;j.a=region[size_t(pa)];j.b=region[size_t(pb)];j.rest=(m_particles[size_t(pa)].rest+m_particles[size_t(pb)].rest)*.5f;m_joints.push_back(j);whole.join(j.a,j.b);}
 // Told its parts, a body is held only where its parts hang from one another: an arm that lay
 // against the belly is not stitched to it there, or it could not turn at the shoulder.
 if(rig){auto up=[&](int s){int want=rig->limits[size_t(partOfRegion[size_t(s)])].parent;
   for(int guard=0;guard<16&&want>=0;++guard){for(int o=0;o<R;++o)if(partOfRegion[size_t(o)]==want)return o;want=rig->limits[size_t(want)].parent;}return -1;};
  std::vector<Joint> kept;for(int s=0;s<R;++s){const int p=up(s);if(p<0)continue;bool found=false;
   for(const auto&j:m_joints)if(j.seam&&((j.a==s&&j.b==p)||(j.a==p&&j.b==s))){kept.push_back(j);found=true;break;}
   if(!found){float best=1e30f;int pa=-1,pb=-1;for(int a=0;a<N;++a){if(region[size_t(a)]!=s)continue;for(int b=0;b<N;++b){if(region[size_t(b)]!=p)continue;float d=span(m_particles[size_t(a)].rest-m_particles[size_t(b)].rest);if(d<best){best=d;pa=a;pb=b;}}}
    if(pa>=0){Joint j;j.a=p;j.b=s;j.rest=(m_particles[size_t(pa)].rest+m_particles[size_t(pb)].rest)*.5f;kept.push_back(j);}}}
  m_joints=kept;}
 m_sections.resize(size_t(R));
 for(int s=0;s<R;++s)m_sections[size_t(s)].part=partOfRegion[size_t(s)];
 for(int i=0;i<N;++i){auto&s=m_sections[size_t(region[size_t(i)])];s.members.push_back(i);s.weights.push_back(1.f);++s.count;}
 for(auto&j:m_joints){j.particle=int(m_particles.size());Particle p;p.rest=p.x=p.previous=j.rest;p.region=-1;m_particles.push_back(p);
  for(int s:{j.a,j.b}){auto&section=m_sections[size_t(s)];section.members.push_back(j.particle);section.weights.push_back(std::max(2.f,.35f*float(section.count)));}}
 // On a seam a vertex is carried half by each side, and less the farther it is from the seam: only
 // the skin over the joint bends; everything else is as rigid as its section.
 {const float band=std::max(.001f,tune("carcass.seamBand",1.f)*std::max(.012f,.022f*m_size));std::vector<std::vector<int>> seamVerts(m_joints.size());
  for(const auto&e:m_edges){if(e.other<0)continue;for(size_t j=0;j<m_joints.size();++j){const auto&joint=m_joints[j];if(!joint.seam||!((joint.a==e.owner&&joint.b==e.other)))continue;seamVerts[j].push_back(e.a);seamVerts[j].push_back(e.b);}}
  for(size_t j=0;j<m_joints.size();++j){auto&list=seamVerts[j];std::sort(list.begin(),list.end());list.erase(std::unique(list.begin(),list.end()),list.end());
   if(list.empty())continue;float rim=0;for(int v:list)rim=std::max(rim,span(m_rest[size_t(v)]-m_joints[j].rest));
   for(int v=0;v<V;++v){auto&b=m_binds[size_t(v)];const Joint&joint=m_joints[j];if(b.home!=joint.a&&b.home!=joint.b)continue;if(span(m_rest[size_t(v)]-joint.rest)>rim+band)continue;
    float d=1e30f;for(int u:list)d=std::min(d,span(m_rest[size_t(u)]-m_rest[size_t(v)]));const float w=.5f*std::max(0.f,1.f-d/band);
    if(w>b.blend){b.blend=w;b.other=b.home==joint.a?joint.b:joint.a;b.joint=int(j);}}}}
 Point3 middle{};for(int i=0;i<N;++i)middle=middle+m_particles[size_t(i)].rest;middle=middle*(1.f/float(N));
 // The kill tips it: the higher on the body, the harder. A body standing when it died and given no
 // push is tipped a little anyway, the way it was leaning; one already lying is let down as it lies.
 float low=1e30f,high=-1e30f;for(int i=0;i<N;++i){low=std::min(low,m_particles[size_t(i)].rest.z);high=std::max(high,m_particles[size_t(i)].rest.z);}
 const float upright=std::clamp(((high-low)/m_size-.35f)/.35f,0.f,1.f);
 Point3 shove=push;if(span(push)<.05f){float a=chance(m_seed)*kTwoPi;shove=Point3{std::cos(a),std::sin(a),0}*(.35f*upright);}
 std::vector<Point3> drift(static_cast<size_t>(R));for(auto&d:drift)d=Point3{either(m_seed)*.06f,either(m_seed)*.06f,either(m_seed)*.03f}*upright;
 for(auto&p:m_particles){Point3 v=shove*(.35f+.9f*(p.rest.z-low)/std::max(.05f,high-low));if(p.region>=0)v=v+drift[size_t(p.region)];p.previous=p.x-v*Tick;
  p.floor=world.supportBelow(p.x.x,p.x.y,std::max(p.x.z,middle.z)+.05f);p.roof=world.clearanceAbove(p.x.x,p.x.y,std::max(p.floor,p.x.z-.05f));p.probe=p.x;p.safe=p.x;
  // Free where it is (whatever its reach), walls hold it from the start: a man who died with his back to a wall
  // is kept out of it, not let into it until he is clear of it.
  if(!(switchedOff()&64))p.loose=world.fits(p.x.x,p.x.y,p.x.z+.01f,.04f,false);}
 fit();
 if(rig){m_root=0;for(int s=0;s<R;++s)if(rig->limits[size_t(partOfRegion[size_t(s)])].parent<0){m_root=s;break;}}
 else{float best=-1;for(int s=0;s<R;++s){float score=float(m_sections[size_t(s)].count)/(1.f+6.f*span(m_sections[size_t(s)].restCentre-middle)/m_size);if(score>best){best=score;m_root=s;}}}
 // Joints: each section hangs from one other, the trunk from nothing. Told, by the parts; found, by
 // the joints out from the trunk.
 {std::vector<int> order{m_root};std::vector<char> seen(size_t(R),0);seen[size_t(m_root)]=1;
  for(size_t head=0;head<order.size();++head){const int at=order[head];
   for(int j=0;j<int(m_joints.size());++j){const auto&joint=m_joints[size_t(j)];int other=joint.a==at?joint.b:joint.b==at?joint.a:-1;if(other<0||seen[size_t(other)])continue;
    if(rig){// Told: it hangs from the section its part's joint names, wherever that is.
     int want=rig->limits[size_t(partOfRegion[size_t(other)])].parent;bool ok=false;
     for(int guard=0;guard<16&&want>=0;++guard){for(int s=0;s<R;++s)if(partOfRegion[size_t(s)]==want){ok=s==at;break;}if(ok)break;bool found=false;for(int s=0;s<R;++s)if(partOfRegion[size_t(s)]==want){found=true;break;}if(found)break;want=rig->limits[size_t(want)].parent;}
     if(!ok)continue;}
    seen[size_t(other)]=1;m_sections[size_t(other)].parent=at;m_sections[size_t(other)].joint=j;order.push_back(other);}
   if(head+1==order.size())for(int s=0;s<R;++s)if(!seen[size_t(s)]){// anything the parts left unreached hangs from what it touches
    for(int j=0;j<int(m_joints.size());++j){const auto&joint=m_joints[size_t(j)];int other=joint.a==s?joint.b:joint.b==s?joint.a:-1;if(other>=0&&seen[size_t(other)]){seen[size_t(s)]=1;m_sections[size_t(s)].parent=other;m_sections[size_t(s)].joint=j;order.push_back(s);break;}}}}}
 // The rest pose each section's limits are measured from, and their axes.
 for(int s=0;s<R;++s){auto&section=m_sections[size_t(s)];Point3 bindCentre=section.restCentre,restCentre=section.restCentre;
  if(rig){std::vector<Point3> from,to;for(int v=0;v<V;++v)if(m_binds[size_t(v)].home==s){from.push_back(bindAt[size_t(v)]);to.push_back(m_rest[size_t(v)]);}
   bestRotation(from,to,section.bind);Point3 cf{},ct{};for(size_t i=0;i<from.size();++i){cf=cf+from[i];ct=ct+to[i];}if(!from.empty()){bindCentre=cf*(1.f/float(from.size()));restCentre=ct*(1.f/float(from.size()));}}
  if(section.parent<0||section.joint<0)continue;
  const Joint&joint=m_joints[size_t(section.joint)];float ib[4];qconj(section.bind,ib);
  const Point3 pivot=qrot(ib,joint.rest-restCentre)+bindCentre;Point3 meanRest{};for(int i:section.members)if(m_particles[size_t(i)].region>=0)meanRest=meanRest+m_particles[size_t(i)].rest;meanRest=meanRest*(1.f/float(std::max(1,section.count)));
  const Point3 far=qrot(ib,meanRest-restCentre)+bindCentre;
  section.twistAxis=unit(far-pivot,{0,0,1});
  const Point3 forward=rig?unit(rig->forward,{0,0,1}):unit(cross3(section.twistAxis,std::fabs(section.twistAxis.z)<.9f?Point3{0,0,1}:Point3{1,0,0}));
  Point3 h=cross3(section.twistAxis,forward);if(span(h)<1e-3f)h=cross3(section.twistAxis,std::fabs(section.twistAxis.x)<.9f?Point3{1,0,0}:Point3{0,1,0});h=unit(h);
  section.limit=rig?rig->limits[size_t(section.part)]:Limit{};
  if(dot3(cross3(h,section.twistAxis),forward)*(section.limit.flexForward?1.f:-1.f)<0)h=h*-1.f;
  section.bendAxis=h;section.sideAxis=unit(cross3(section.twistAxis,h));section.limited=true;}
 // The pose each joint died in (every section's rest turn known by now).
 // A pose the body died in is a pose it can hold: a limit it is already past is widened to take it.
 for(auto&section:m_sections)if(section.limited){relative(section,section.rel0);for(int k=0;k<4;++k)section.last[k]=section.rel0[k];
  const Turn at=decompose(section.rel0,section.twistAxis,section.bendAxis,section.sideAxis);constexpr float Margin=.09f;auto&l=section.limit;
  l.bendLo=std::min(l.bendLo,at.bend-Margin);l.bendHi=std::max(l.bendHi,at.bend+Margin);l.side=std::max(l.side,std::fabs(at.side)+Margin);l.twist=std::max(l.twist,std::fabs(at.twist)+Margin);}
 // Each section's body for the others to keep out of: its points' long axis, and the oval across it.
 for(auto&s:m_sections){std::vector<Point3> pts;for(int i:s.members)if(m_particles[size_t(i)].region>=0)pts.push_back(m_particles[size_t(i)].rest);if(pts.size()<3)continue;
  Point3 c{};for(auto p:pts)c=c+p;c=c*(1.f/float(pts.size()));float C[3][3]{};for(auto p:pts){Point3 d=p-c;const float a[3]{d.x,d.y,d.z};for(int r=0;r<3;++r)for(int k=0;k<3;++k)C[r][k]+=a[r]*a[k];}
  auto power=[&](Point3 v,Point3 avoid){for(int it=0;it<40;++it){v=v-avoid*dot3(v,avoid);Point3 n{C[0][0]*v.x+C[0][1]*v.y+C[0][2]*v.z,C[1][0]*v.x+C[1][1]*v.y+C[1][2]*v.z,C[2][0]*v.x+C[2][1]*v.y+C[2][2]*v.z};v=unit(n-avoid*dot3(n,avoid),v);}return v;};
  const Point3 U=power(unit(Point3{.6f,.5f,.62f}),{}),A=power(unit(cross3(U,std::fabs(U.z)<.9f?Point3{0,0,1}:Point3{1,0,0})),U),B=unit(cross3(U,A));
  float t0=1e30f,t1=-1e30f,sa=0,sb=0;for(auto p:pts){Point3 d=p-c;float t=dot3(d,U);t0=std::min(t0,t);t1=std::max(t1,t);sa+=dot3(d,A)*dot3(d,A);sb+=dot3(d,B)*dot3(d,B);}
  s.ru=.85f*std::sqrt(2.f*sa/float(pts.size()));s.rv=.85f*std::sqrt(2.f*sb/float(pts.size()));const float r=std::min(s.ru,s.rv);
  const float a=std::min(t0+r,(t0+t1)*.5f),b=std::max(t1-r,(t0+t1)*.5f);s.capA=c+U*a;s.capB=c+U*b;s.capU=A;s.capV=B;}
 // Each section's outermost skin, as hull points: the vertex farthest out each of 6 ways (the faces of
 // a cube, in the pose it died in), where no point of the cloud is within 4 cm of it.
 if(!(switchedOff()&32)){static const Point3 ways[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
  const float spacing=.04f*std::max(.5f,m_size/1.8f);
  for(int s=0;s<R;++s){auto&section=m_sections[size_t(s)];if(section.members.empty())continue;std::vector<int> picked;
   for(const auto&way:ways){int best=-1;float most=-1e30f;for(int v=0;v<V;++v){if(m_binds[size_t(v)].home!=s)continue;const float d=dot3(m_rest[size_t(v)],way);if(d>most){most=d;best=v;}}
    if(best<0||std::find(picked.begin(),picked.end(),best)!=picked.end())continue;const Point3 at=m_rest[size_t(best)];bool covered=false;
    for(int i:section.members){const auto&p=m_particles[size_t(i)];if(p.region>=0&&span(p.rest-at)<spacing){covered=true;break;}}
    for(int i:picked)if(span(m_rest[size_t(i)]-at)<spacing)covered=true;
    if(covered)continue;picked.push_back(best);
    Particle p;p.rest=p.x=p.previous=p.safe=p.probe=at;p.region=s;p.hull=true;p.floor=world.supportBelow(at.x,at.y,std::max(at.z,middle.z)+.05f);p.roof=world.clearanceAbove(at.x,at.y,std::max(p.floor,at.z-.05f));m_particles.push_back(p);}}}
 // What touches already at rest (an arm against the side it hung by) is let be.
 for(auto&p:m_particles){if(p.region<0)continue;for(int s=0;s<R&&s<32;++s){if(s==p.region)continue;const auto&o=m_sections[size_t(s)];if(o.ru<=0)continue;
   const Point3 axis=o.capB-o.capA;const float len=span(axis);const Point3 U=len>1e-6f?axis*(1/len):Point3{0,0,1};const float t=std::clamp(dot3(p.rest-o.capA,U),0.f,len);const Point3 off=p.rest-(o.capA+U*t);
   const float u=dot3(off,o.capU)/o.ru,v=dot3(off,o.capV)/o.rv,w=dot3(off,U)/std::min(o.ru,o.rv);if(u*u+v*v+w*w<1.21f)p.ignore|=1u<<s;}}
 if(std::getenv("RM_SEAM_DEBUG")&&m_rigged){static std::ofstream log("seam-debug.txt");std::vector<int> mixed(size_t(R),0),tris(size_t(R),0);std::vector<float> far(size_t(R),0.f);
  for(int t=0;t<T;++t){const int o=m_owner[size_t(t)];++tris[size_t(o)];for(int k=0;k<3;++k){const int v=m_corner[size_t(t*3+k)];const int h=m_binds[size_t(v)].home;if(h==o)continue;++mixed[size_t(o)];
    float best=1e9f;for(const auto&j:m_joints)if((j.a==h&&j.b==o)||(j.a==o&&j.b==h))best=std::min(best,span(m_rest[size_t(v)]-j.rest));far[size_t(o)]=std::max(far[size_t(o)],best);}}
  log<<"body R="<<R<<" T="<<T<<'\n';for(int s=0;s<R;++s)log<<"  section "<<s<<" part "<<m_sections[size_t(s)].part<<" tris "<<tris[size_t(s)]<<" foreign corners "<<mixed[size_t(s)]<<" farthest from a joint with its home "<<far[size_t(s)]<<'\n';log.flush();}
 regroup();measure();m_dirty=true;
}

void Carcass::relative(const Section& s,float out[4])const{
 // The child's turn from its parent, both from the rest pose: identity when it is as it was made.
 const auto&p=m_sections[size_t(s.parent)];float wp[4],wc[4],ip[4];qmul(p.q,p.bind,wp);qmul(s.q,s.bind,wc);qconj(wp,ip);qmul(ip,wc,out);qnorm(out);}

void Carcass::fit(){
 for(auto&s:m_sections){if(s.members.empty())continue;Point3 c{},rc{};float total=0;
  for(size_t k=0;k<s.members.size();++k){const auto&p=m_particles[size_t(s.members[k])];float w=s.weights[k];c=c+p.x*w;rc=rc+p.rest*w;total+=w;}
  c=c*(1/total);rc=rc*(1/total);Point3 a[3]{};
  for(size_t k=0;k<s.members.size();++k){const auto&p=m_particles[size_t(s.members[k])];Point3 d=(p.x-c)*s.weights[k],r=p.rest-rc;a[0]=a[0]+d*r.x;a[1]=a[1]+d*r.y;a[2]=a[2]+d*r.z;}
  // The rotation nearest the section's deformation, carried over from the last step.
  for(int i=0;i<6;++i){axesOf(s.q,s.axes);
   Point3 omega=(cross3(s.axes[0],a[0])+cross3(s.axes[1],a[1])+cross3(s.axes[2],a[2]))*(1.f/(std::fabs(dot3(s.axes[0],a[0])+dot3(s.axes[1],a[1])+dot3(s.axes[2],a[2]))+1e-9f));
   float angle=span(omega);if(!(angle>1e-6f))break;Point3 axis=omega*(1/angle);angle=std::min(angle,1.2f);float h=std::sin(angle*.5f),w=std::cos(angle*.5f);
   float d[4]{w,axis.x*h,axis.y*h,axis.z*h};qmul(d,s.q,s.q);qnorm(s.q);}
  axesOf(s.q,s.axes);s.centre=c;s.restCentre=rc;}
}

// Come to rest: no section lies in the floor or a wall. Each section is lifted by the most its own skin is
// in the floor; each piece is slid by the most it is in a wall, whole, so no section is pushed into the next.
void Carcass::settle(const World& world){
 const int V=int(m_binds.size());if(V==0)return;
 for(int round=0;round<6;++round){bool moved=false;
  std::vector<float> up(m_sections.size(),0.f),side(m_sections.size(),0.f);std::vector<Point3> slide(m_sections.size());
  skin();for(int c=0;c<int(m_corner.size());++c){const int v=m_corner[size_t(c)],t=c/3;const auto&b=m_binds[size_t(v)];const int owner=m_owner[size_t(t)];
   const int home=m_piece[size_t(b.home)]==m_piece[size_t(owner)]?b.home:owner;const Point3 at=m_triangles[size_t(t)].v[c%3].p;
   // Under what it lies on (to within a few millimetres): lifted onto it. In a wall or the side of a step: the
   // least move that frees it, out sideways or up onto it.
   Point3 best{};float least=1e30f;const float floor=world.supportBelow(at.x,at.y,at.z+.12f);if(floor>at.z+.003f&&floor-at.z<.12f){least=floor-at.z+.004f;best={0,0,least};}
   else if(world.fits(at.x,at.y,at.z+.03f,.001f,false))continue;
   if(least>1e29f)for(float r:{.01f,.02f,.035f,.05f,.075f,.1f}){if(r>=least)break;for(int k=0;k<8;++k){const float a=float(k)*kPi*.25f;const Point3 step{std::cos(a)*r,std::sin(a)*r,0};
     if(world.fits(at.x+step.x,at.y+step.y,at.z+.03f,.001f,false)){best=step*1.15f;least=r;break;}}}
   const size_t piece=size_t(m_piece[size_t(home)]);if(least>=1e29f)continue;
   if(best.z>0)up[size_t(home)]=std::max(up[size_t(home)],best.z);else if(least>side[piece]){side[piece]=least;slide[piece]=best;}}
  for(size_t s=0;s<m_sections.size();++s){const size_t piece=size_t(m_piece[s]);const Point3 fix=slide[piece]+Point3{0,0,up[s]};if(up[s]<=0&&side[piece]<=0)continue;moved=true;auto&sec=m_sections[s];sec.centre=sec.centre+fix;for(int i:sec.members){auto&p=m_particles[size_t(i)];if(p.region>=0){p.x=p.x+fix;p.previous=p.x;}}}
  // A section lifted into one that lies over it lifts that one too.
  for(int k=0;k<4;++k){std::vector<float> rise(m_sections.size(),0.f);bool any=false;
   for(const auto&p:m_particles){if(p.region<0)continue;for(int o=0;o<int(m_sections.size());++o){const float d=depthIn(p,o);if(d<=0)continue;
    const int upper=m_sections[size_t(o)].centre.z>=m_sections[size_t(p.region)].centre.z?o:p.region;rise[size_t(upper)]=std::max(rise[size_t(upper)],d+.003f);any=true;}}
   if(!any)break;moved=true;
   for(size_t s=0;s<m_sections.size();++s){if(rise[s]<=0)continue;auto&sec=m_sections[s];sec.centre.z+=rise[s];for(int i:sec.members){auto&p=m_particles[size_t(i)];if(p.region>=0){p.x.z+=rise[s];p.previous=p.x;}}}}
  if(!moved)break;}
 m_dirty=true;
}

// Each section is a rigid body (its points' mass and spread), stepped in SubSteps by position-based
// dynamics: everything moves only where a body is, and its velocity is what it did over the substep.
// Joints hold their two sides together and turn within their limits; sections keep out of one another;
// the floor holds them up and grips them (static friction where they are, dynamic as they slide).
void Carcass::bodies(){
 for(auto&s:m_sections){s.mass=0;s.comRest={};for(int i:s.members){const auto&p=m_particles[size_t(i)];if(p.region<0)continue;s.comRest=s.comRest+p.rest;s.mass+=1;}
  if(s.mass<=0)continue;s.comRest=s.comRest*(1/s.mass);
  double I[3][3]{};for(int i:s.members){const auto&p=m_particles[size_t(i)];if(p.region<0)continue;const Point3 r=p.rest-s.comRest;const double a[3]{r.x,r.y,r.z},rr=double(dot3(r,r));
   for(int u=0;u<3;++u)for(int v=0;v<3;++v)I[u][v]+=(u==v?rr:0.)-a[u]*a[v];}
  // Nothing outside this reach of its centre can be inside its capsule.
  s.capReach=std::max(span(s.capA-s.comRest),span(s.capB-s.comRest))+std::max(s.ru,s.rv)+.001f;
  s.reach=std::sqrt(float((I[0][0]+I[1][1]+I[2][2])*.5/double(s.mass)));
  // Points sit on the skin: a section is at least as hard to turn as a ball 3 cm across.
  for(int u=0;u<3;++u)I[u][u]+=double(s.mass)*.0004;
  const double det=I[0][0]*(I[1][1]*I[2][2]-I[1][2]*I[2][1])-I[0][1]*(I[1][0]*I[2][2]-I[1][2]*I[2][0])+I[0][2]*(I[1][0]*I[2][1]-I[1][1]*I[2][0]);
  for(int u=0;u<3;++u)for(int v=0;v<3;++v){const int u1=(v+1)%3,u2=(v+2)%3,v1=(u+1)%3,v2=(u+2)%3;s.invInertia[u*3+v]=float((I[u1][v1]*I[u2][v2]-I[u1][v2]*I[u2][v1])/det);}}
 m_bodies=true;}

namespace {
// A section's inverse inertia (world) times u.
Point3 turnBy(const Point3 axes[3],const float inv[9],Point3 u){const Point3 l{dot3(axes[0],u),dot3(axes[1],u),dot3(axes[2],u)};
 const Point3 m{inv[0]*l.x+inv[1]*l.y+inv[2]*l.z,inv[3]*l.x+inv[4]*l.y+inv[5]*l.z,inv[6]*l.x+inv[7]*l.y+inv[8]*l.z};return axes[0]*m.x+axes[1]*m.y+axes[2]*m.z;}
}

void Carcass::shift(int section,Point3 by){auto&s=m_sections[size_t(section)];s.centre=s.centre+by;s.com=s.com+by;
 for(auto&p:m_particles)if(p.region==section){p.x=p.x+by;p.previous=p.x;}}
void Carcass::step(const World& world){
 const auto started=std::chrono::steady_clock::now();
 if(m_glide){float most=0;for(const auto&e:m_ease)most=std::max(most,span(e));const float share=most<1e-6f?1.f:std::min(1.f,.015f/most);
  for(size_t s=0;s<m_ease.size()&&s<m_sections.size();++s){shift(int(s),m_ease[s]*share);m_ease[s]=m_ease[s]*(1-share);}
  for(auto&j:m_joints){if(j.cut)continue;auto&p=m_particles[size_t(j.particle)];p.x=(carry(m_sections[size_t(j.a)],j.rest)+carry(m_sections[size_t(j.b)],j.rest))*.5f;p.previous=p.x;}
  m_age+=Tick;m_dirty=true;m_motion=.001f;if(share>=1.f){m_ease.clear();m_glide=false;m_asleep=true;measure();}
  ++Steps;StepMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return;}
 // Settling toward sleep: what settling found is fed in a little a step (the most 1.2 cm, every section the
 // same share of its way) and the solver then runs as ever, joints, keeping apart and the world all held, so
 // the correction never leaves a limb in the trunk or a hand in the floor.
 const bool settling=!m_ease.empty();
 // Propped: a man's trunk steep (over 45 degrees) with his hips well off what is under them, a body leaning on a
 // rail or a bed's end as no dead weight stays. Until he is down his knees and feet lose their grip and nothing
 // drains his motion or lets him sleep, so he slides and folds down onto the floor or over onto what he leans on.
 bool propped=false;
 if(m_rigged){int pelvis=-1,chest=-1;for(int s=0;s<int(m_sections.size());++s){if(m_sections[size_t(s)].part==0)pelvis=s;if(m_sections[size_t(s)].part==2)chest=s;}
  if(pelvis>=0&&chest>=0&&m_piece[size_t(pelvis)]==m_piece[size_t(chest)]){const Point3 d=m_sections[size_t(chest)].centre-m_sections[size_t(pelvis)].centre;
   float hips=1e9f;for(int i:m_sections[size_t(pelvis)].members){const auto&p=m_particles[size_t(i)];if(p.region>=0)hips=std::min(hips,p.x.z-p.floor);}
   propped=std::atan2(d.z,std::hypot(d.x,d.y))>.785f&&hips>.15f&&hips<1e8f;}}
 struct Was {Point3 com;float q[4];};std::vector<Was> before;
 if(settling){before.resize(m_sections.size());for(size_t i=0;i<m_sections.size();++i){const auto&s=m_sections[i];before[i].com=carry(s,s.comRest);for(int k=0;k<4;++k)before[i].q[k]=s.q[k];}}
 if(settling){float most=0;for(const auto&e:m_ease)most=std::max(most,span(e));const float share=most<1e-6f?1.f:std::min(1.f,SettleFeed/most);
  for(size_t s=0;s<m_ease.size()&&s<m_sections.size();++s)if(span(m_ease[s])>0){shift(int(s),m_ease[s]*share);m_ease[s]=m_ease[s]*(1-share);}
  m_settling+=Tick;}
 if(!m_bodies)bodies();
 const int R=int(m_sections.size());
 // A point is clear where it is free, and, with a reach, free that far round it.
 auto clear=[&](Point3 at){if(!world.fits(at.x,at.y,at.z+.01f,.04f,false))return false;if(m_reach<=0)return true;
  for(auto [dx,dy]:{std::pair{m_reach,0.f},{-m_reach,0.f},{0.f,m_reach},{0.f,-m_reach}})if(!world.fits(at.x+dx,at.y+dy,at.z+.01f,.04f,false))return false;return true;};
 knobs();
 // The solids near enough to touch it this step.
 bool carried=false;struct Near {const Solid* solid;float c,s;};static thread_local std::vector<Near> near;near.clear();
 if(!(switchedOff()&16))for(const auto&o:m_solids){const float reach=m_radius+span(o.half)+.3f;const Point3 d=o.centre-m_centre;if(dot3(d,d)<reach*reach)near.push_back({&o,std::cos(o.yaw),std::sin(o.yaw)});}
 // RM_CARCASS clears features for comparison: bit 1 no joint limits, 2 no death tone, 4 no self-collision.
 const int off=switchedOff();
 const int subs=std::max(1,SubSteps);const float h=Tick/float(subs),hold=Hold/float(subs);
 auto live=[&](const Section&s){return s.mass>0&&!s.members.empty();};
 auto at=[&](const Section&s,Point3 rest){const Point3 d=rest-s.comRest;return s.com+s.axes[0]*d.x+s.axes[1]*d.y+s.axes[2]*d.z;};
 auto wasAt=[&](const Section&s,Point3 rest){const Point3 d=rest-s.comRest;return s.prevCom+s.prevAxes[0]*d.x+s.prevAxes[1]*d.y+s.prevAxes[2]*d.z;};
 auto spinOf=[&](Section&s,Point3 dw){const float d[4]{0,dw.x,dw.y,dw.z};float t[4];qmul(d,s.q,t);for(int k=0;k<4;++k)s.q[k]+=.5f*t[k];qnorm(s.q);axesOf(s.q,s.axes);};
 auto weightAt=[&](const Section&s,Point3 r,Point3 n){const Point3 rn=cross3(r,n);return 1/s.mass+dot3(rn,turnBy(s.axes,s.invInertia,rn));};
 auto push=[&](Section&s,Point3 r,Point3 p){s.com=s.com+p*(1/s.mass);spinOf(s,turnBy(s.axes,s.invInertia,cross3(r,p)));};
 // Pull the point `pa` of a toward `pb` of b (b may be the world: null).
 auto meet=[&](Section&a,Point3 pa,Section*b,Point3 pb,float most){Point3 dx=pb-pa;float c=span(dx);if(c<1e-7f)return 0.f;if(c>most){dx=dx*(most/c);c=most;}const Point3 n=dx*(1/c),ra=pa-a.com;
  const float wa=weightAt(a,ra,n),wb=b?weightAt(*b,pb-b->com,n):0.f,l=c/(wa+wb);push(a,ra,n*l);if(b)push(*b,pb-b->com,n*-l);return l;};
 // The velocities each section has, from its points (a strike or a blast pushes the points).
 for(auto&s:m_sections){if(!live(s))continue;axesOf(s.q,s.axes);s.com=carry(s,s.comRest);Point3 v{};for(int i:s.members){const auto&p=m_particles[size_t(i)];if(p.region>=0)v=v+(p.x-p.previous);}v=v*(1/(s.mass*Tick));
  Point3 L{};for(int i:s.members){const auto&p=m_particles[size_t(i)];if(p.region<0)continue;L=L+cross3(p.x-s.com,(p.x-p.previous)*(1/Tick)-v);}
  s.vel=v*.992f;s.spin=turnBy(s.axes,s.invInertia,L)*.97f;const float sp=span(s.vel);if(sp>18.f)s.vel=s.vel*(18.f/sp);const float w=span(s.spin);if(w>30.f)s.spin=s.spin*(30.f/w);}
 const float tone=off&2?0.f:ToneStart*std::clamp(1.f-m_age/ToneTime,0.f,1.f);
 // Death tone and joint friction pull so far a step (as four passes of the whole pull would): the same however finely the step is cut.
 const float toneSub=1-std::pow(1-tone,4.f/float(subs*Passes)),frictionSub=1-std::pow(1-JointFriction,4.f/float(subs*Passes));
 struct Contact {int section,particle;float press;bool held;Point3 carrier;};static thread_local std::vector<float> moverPush;static thread_local std::vector<Contact> contacts;static thread_local std::vector<int> touching;
 std::vector<char> next(size_t(R)*size_t(R),0);for(auto&j:m_joints)if(!j.cut){next[size_t(j.a)*size_t(R)+size_t(j.b)]=1;next[size_t(j.b)*size_t(R)+size_t(j.a)]=1;}
 // Sections keep out of one another: a point of one inside another's capsule is put back on its skin.
 auto keepApart=[&]{if(off&4)return;
  for(auto&pt:m_particles){if(pt.region<0||pt.hull)continue;auto&a=m_sections[size_t(pt.region)];if(!live(a))continue;Point3 p=at(a,pt.rest);
    for(int o=0;o<R;++o){if(o==pt.region||next[size_t(pt.region)*size_t(R)+size_t(o)]||(o<32&&(pt.ignore>>o&1u)))continue;auto&b=m_sections[size_t(o)];if(b.ru<=0||!live(b))continue;
     const Point3 d=p-b.com;if(dot3(d,d)>b.capReach*b.capReach)continue;const Point3 l=Point3{dot3(d,b.axes[0]),dot3(d,b.axes[1]),dot3(d,b.axes[2])}+b.comRest;
     const Point3 axis=b.capB-b.capA;const float len=span(axis);const Point3 U=len>1e-6f?axis*(1/len):Point3{0,0,1};const float tt=std::clamp(dot3(l-b.capA,U),0.f,len);const Point3 foot=b.capA+U*tt,offs=l-foot;
     const float rw=std::min(b.ru,b.rv),u=dot3(offs,b.capU)/b.ru,v=dot3(offs,b.capV)/b.rv,w=dot3(offs,U)/rw,rho2=u*u+v*v+w*w;if(rho2>=1)continue;
     const float rho=std::sqrt(rho2);const Point3 out=rho>1e-4f?foot+offs*(1/rho):foot+b.capU*b.ru;
     // The point goes out of b and b off the point, the lighter the more.
     const Point3 o2=at(b,out);meet(a,p,&b,o2,CollideStep);p=at(a,pt.rest);}}
 };
 // The fastest few points (over 1.5 m/s) are swept against the world after the step: a hand flung at a wall
 // that the points' own wall test, made only every few centimetres, would let through.
 struct Fast {int particle;Point3 from;float speed;};static thread_local std::vector<Fast> fast;fast.clear();
 if(!(off&64))for(int i=0;i<int(m_particles.size());++i){const auto&p=m_particles[size_t(i)];if(p.region<0||p.hull)continue;const float sp=span(p.x-p.previous)/Tick;if(sp<1.5f)continue;
  if(fast.size()<6)fast.push_back({i,p.x,sp});else{auto low=std::min_element(fast.begin(),fast.end(),[](const Fast&a,const Fast&b){return a.speed<b.speed;});if(low->speed<sp)*low={i,p.x,sp};}}
 for(int sub=0;sub<subs;++sub){
  for(auto&s:m_sections){if(!live(s))continue;s.prevCom=s.com;for(int k=0;k<4;++k)s.prevQ[k]=s.q[k];for(int k=0;k<3;++k)s.prevAxes[k]=s.axes[k];
   s.vel.z-=14.f*h;s.com=s.com+s.vel*h;spinOf(s,s.spin*h);}
  contacts.clear();touching.assign(m_particles.size(),-1);moverPush.assign(size_t(R),0.f);
  for(int pass=0;pass<Passes;++pass){const bool last=pass+1==Passes;
   // The floor, the roof and the walls; the floor grips what lies on it.
   for(int i=0;i<int(m_particles.size());++i){auto&pt=m_particles[size_t(i)];if(pt.region<0)continue;auto&s=m_sections[size_t(pt.region)];if(!live(s))continue;
    Point3 p=at(s,pt.rest);
    float dx=p.x-pt.probe.x,dy=p.y-pt.probe.y;
    if(dx*dx+dy*dy>ProbeStep*ProbeStep||std::fabs(p.z-pt.probe.z)>.1f){const float was=wasAt(s,pt.rest).z;pt.floor=world.supportBelow(p.x,p.y,std::max(p.z,was)+.08f);pt.roof=world.clearanceAbove(p.x,p.y,std::max(pt.floor,std::min(p.z,was)-.05f));pt.probe=p;}
    if(p.z>pt.roof-Skin&&pt.roof>pt.floor+.1f){meet(s,p,nullptr,Point3{p.x,p.y,pt.roof-Skin},1e9f);p=at(s,pt.rest);}
    // Deep under a floor it was not under a substep ago, it came in from the side (a step's riser): it goes back out
    // the way it came, not up onto the step.
    if(const Point3 came=wasAt(s,pt.rest);!pt.hull&&p.z<pt.floor-.03f&&world.supportBelow(came.x,came.y,came.z+.02f)<pt.floor-.02f){meet(s,p,nullptr,Point3{came.x,came.y,p.z},1e9f);p=at(s,pt.rest);pt.floor=world.supportBelow(p.x,p.y,p.z+.08f);pt.probe=p;}
    // The solids that are not the world (props, crates, whoever wades through): on top of one it is a floor
    // that may move under it; in its side it is put out of the nearest side, as hard as that thing shoves.
    float floor=pt.floor;Point3 carrier{};
    for(const auto&o:near){const Point3 d=p-o.solid->centre;if(o.solid->half.x<=0)continue;const float lx=d.x*o.c+d.y*o.s,ly=-d.x*o.s+d.y*o.c;
     const float hx=o.solid->half.x+Skin,hy=o.solid->half.y+Skin;if(std::fabs(lx)>=hx||std::fabs(ly)>=hy)continue;
     const float top=o.solid->centre.z+o.solid->half.z;const Point3 came=wasAt(s,pt.rest);
     if(p.z>=top-.06f||came.z>=top-.02f){if(top>floor&&top<=std::max(p.z,came.z)+.08f){floor=top;carrier=o.solid->velocity*o.solid->shove;}continue;}
     if(d.z<-o.solid->half.z-Skin)continue;
     const float ex=hx-std::fabs(lx),ey=hy-std::fabs(ly);const float sx=lx<0?-1.f:1.f,sy=ly<0?-1.f:1.f;
     const Point3 n=ex<ey?Point3{o.c*sx,o.s*sx,0}:Point3{-o.s*sy,o.c*sy,0};
     // Still, it is a wall; moving, it gives the body no more than its share of its speed (a light crate
     // does not bulldoze a man), so it may pass through what it cannot shift.
     // (A section already going its share of the way the thing comes at it is not pushed further: the thing
     // passes, as it would have slowed against him.)
     const float speed=span(o.solid->velocity),depth=std::min(ex,ey);
     if(speed<.05f)meet(s,p,nullptr,p+n*depth,2*CollideStep);
     else{const float share=o.solid->shove*std::max(0.f,dot3(o.solid->velocity,n)),going=dot3(s.com-s.prevCom,n)/h;
      float&used=moverPush[size_t(pt.region)];const float move=std::min(depth,(share-going)*h-used);if(move<=1e-5f)continue;used+=move;meet(s,p,nullptr,p+n*move,move);}
     p=at(s,pt.rest);}
    if(p.z<floor+Skin&&!(pt.hull&&p.z<floor-.03f)){const float depth=floor+Skin-p.z;meet(s,p,nullptr,Point3{p.x,p.y,floor+Skin},MaxLift);p=at(s,pt.rest);
     // Static friction: what it moved along the floor this substep is taken back while the floor holds it hard enough
     // (along what it lies on: on a moving crate, what it moved against the crate).
     const Point3 from=wasAt(s,pt.rest);const Point3 slid{p.x-from.x-carrier.x*h,p.y-from.y-carrier.y*h,0};const float moved=span(slid);
     const bool held=moved<=(propped?.1f:StaticGrip)*depth+hold;if(held&&moved>1e-7f){meet(s,p,nullptr,p-slid,1e9f);p=at(s,pt.rest);}
     // Every point the floor pushed this substep, in any pass, gives up what it is moving into or off the floor.
     if(dot3(carrier,carrier)>.01f)carried=true;
     if(int&k=touching[size_t(i)];k<0){k=int(contacts.size());contacts.push_back({pt.region,i,depth,held,carrier});}else{auto&c=contacts[size_t(k)];c.press=std::max(c.press,depth);c.held=held;c.carrier=carrier;}}
    dx=p.x-pt.safe.x;dy=p.y-pt.safe.y;
    if(const float look=off&64?.03f:WallLook;dx*dx+dy*dy>look*look){
     // A point that began inside a wall is let out before walls start holding it.
     if(clear(p)){pt.safe=p;pt.loose=true;}
     // Into a wall: back to where the substep found it, so the wall stops it rather than throws it (what is
     // left in a wall at rest, settle frees).
     else if(pt.loose){const Point3 came=wasAt(s,pt.rest);const Point3 to=came;
      meet(s,p,nullptr,Point3{to.x,to.y,p.z},1e9f);}}}
   keepApart();
   // The joints hold.
   for(auto&j:m_joints){if(j.cut)continue;auto&a=m_sections[size_t(j.a)];auto&b=m_sections[size_t(j.b)];if(!live(a)||!live(b))continue;meet(a,at(a,j.rest),&b,at(b,j.rest),1e9f);}
   // Joints turn toward the pose they died in while the body has tone, and against friction, and within their
   // limits (three sweeps: setting one joint moves the next).
   // The limits and keeping apart alternate (thrice on the last pass), so neither undoes the other.
   for(int cycle=0;cycle<(last?3:1);++cycle){
   for(int round=0;round<3;++round)for(int c=0;c<R;++c){auto&s=m_sections[size_t(c)];if(!s.limited||s.parent<0||s.joint<0||m_joints[size_t(s.joint)].cut||!live(s))continue;auto&p=m_sections[size_t(s.parent)];if(!live(p))continue;
    float rel[4];relative(s,rel);float want[4]{rel[0],rel[1],rel[2],rel[3]};
    if(cycle==0&&round==0&&toneSub>0)qslerp(want,s.rel0,toneSub,want);
    if(cycle==0&&round==0&&frictionSub>0)qslerp(want,s.last,frictionSub,want);
    Turn t=decompose(want,s.twistAxis,s.bendAxis,s.sideAxis);const Turn was=t;
    if(!(off&1)){t.bend=std::clamp(t.bend,s.limit.bendLo,s.limit.bendHi);t.side=std::clamp(t.side,-s.limit.side,s.limit.side);t.twist=std::clamp(t.twist,-s.limit.twist,s.limit.twist);}
    if(was.bend!=t.bend||was.side!=t.side||was.twist!=t.twist){float sw[4],tw[4];qexp(s.bendAxis*t.bend+s.sideAxis*t.side,sw);qexp(s.twistAxis*t.twist,tw);qmul(sw,tw,want);}
    float wp[4],wc[4],target[4],iwc[4],e[4];qmul(p.q,p.bind,wp);qmul(s.q,s.bind,wc);qmul(wp,want,target);qconj(wc,iwc);qmul(target,iwc,e);qnorm(e);
    Point3 turn=qlog(e);const float angle=span(turn);if(angle<1e-6f)continue;if(angle>TurnCap)turn=turn*(TurnCap/angle);const Point3 n=unit(turn);
    const Point3 ic=turnBy(s.axes,s.invInertia,n),ip=turnBy(p.axes,p.invInertia,n);const float l=span(turn)/(dot3(n,ic)+dot3(n,ip));
    spinOf(s,ic*l);spinOf(p,ip*-l);}
   keepApart();}
  }
  // The velocities are what the substep did.
  for(auto&s:m_sections){if(!live(s))continue;s.vel=(s.com-s.prevCom)*(1/h);float iq[4],d[4];qconj(s.prevQ,iq);qmul(s.q,iq,d);if(d[0]<0)for(float&k:d)k=-k;s.spin=Point3{d[1],d[2],d[3]}*(2/h);}
  // On the floor nothing goes into it or comes off it, and what slides is slowed (dynamic friction).
  for(int round=0;round<VelocityRounds;++round)for(const auto&c:contacts){auto&s=m_sections[size_t(c.section)];const auto&pt=m_particles[size_t(c.particle)];const Point3 r=at(s,pt.rest)-s.com;
   auto pointV=[&]{return s.vel+cross3(s.spin,r)-c.carrier;};auto kick=[&](Point3 p){s.vel=s.vel+p*(1/s.mass);s.spin=s.spin+turnBy(s.axes,s.invInertia,cross3(r,p));};
   Point3 v=pointV();{const Point3 n{0,0,1};kick(n*(-v.z/weightAt(s,r,n)));}
   v=pointV();const Point3 vt{v.x,v.y,0};const float sp=span(vt);if(sp<1e-6f)continue;const Point3 tdir=vt*(1/sp);
   const float cut=c.held?sp:std::min(sp,(propped?.2f:KineticGrip)*c.press/h/float(VelocityRounds));kick(tdir*(-cut/weightAt(s,r,tdir)));}
  // Flesh on the floor does not roll on: a section lying on it loses its motion as it rolls and slides, and
  // one barely moving stops.
  {const float keep=std::exp(-(propped?1.5f:GroundDamp)*h);std::vector<char> seen(m_sections.size(),0);for(const auto&c:contacts){if(seen[size_t(c.section)])continue;seen[size_t(c.section)]=1;auto&s=m_sections[size_t(c.section)];
   const Point3 own=s.vel-c.carrier;s.vel=c.carrier+Point3{own.x*keep,own.y*keep,own.z};s.spin=s.spin*keep;
   if(!propped&&m_age>ToneTime&&span(s.vel-c.carrier)<RestSpeed&&span(s.spin)*s.reach<RestSpeed){s.vel=c.carrier;s.spin={};}}}
  // A body still moving long after it fell runs down: what is left of its motion drains away faster the
  // longer it goes on (it still falls, only as through water).
  if(const float drain=propped?0.f:FadeDamp*std::clamp((m_age-m_woke-FadeFrom)/1.5f,0.f,1.f);drain>0){const float keep=std::exp(-drain*h);for(auto&s:m_sections){s.vel=s.vel*keep;s.spin=s.spin*keep;}}
  for(auto&s:m_sections)if(s.limited&&s.parent>=0&&live(s)&&live(m_sections[size_t(s.parent)]))relative(s,s.last);
 }
 // Settling, no part goes more than 1.5 cm a step: the step is cut back toward where it began, every section
 // the same share (its middle and its turn), so the solver's answer is kept, only slower.
 if(settling&&before.size()==m_sections.size()){float most=0;
  for(size_t i=0;i<m_sections.size();++i){const auto&s=m_sections[i];if(!live(s))continue;float iq[4],d[4];qconj(before[i].q,iq);qmul(s.q,iq,d);most=std::max(most,span(s.com-before[i].com)+span(qlog(d))*s.capReach);}
  if(most>.015f){const float k=.015f/most;for(size_t i=0;i<m_sections.size();++i){auto&s=m_sections[i];if(!live(s))continue;s.com=before[i].com+(s.com-before[i].com)*k;qslerp(before[i].q,s.q,k,s.q);axesOf(s.q,s.axes);s.vel=s.vel*k;s.spin=s.spin*k;}}}
 // The sweep: along each fast point's way this step, the first place it is in something solid (3 cm over
 // its foot, as the wall test looks, so lying on the floor is not "in" it); it is put back where it last
 // was free and its section stops going that way.
 for(const auto&f:fast){auto&pt=m_particles[size_t(f.particle)];if(pt.region<0)continue;auto&s=m_sections[size_t(pt.region)];if(!live(s))continue;
  const Point3 to=at(s,pt.rest);if(span(to-f.from)<.01f)continue;auto free=[&](Point3 q){return world.fits(q.x,q.y,q.z+.03f,.02f,false);};
  if(!free(f.from))continue;Point3 last=f.from;bool hit=false;
  for(int k=1;k<=8&&!hit;++k){const Point3 q=f.from+(to-f.from)*(float(k)/8.f);if(free(q))last=q;else hit=true;}
  if(!hit&&!world.rayClear({f.from.x,f.from.y},f.from.z+.03f,{to.x,to.y},to.z+.03f,false,false)){hit=true;last=f.from;}
  if(!hit)continue;meet(s,to,nullptr,Point3{last.x,last.y,std::max(last.z,to.z)},1e9f);const Point3 way=unit(to-f.from);const float into=dot3(s.vel,way);if(into>0)s.vel=s.vel-way*into;}
 // The points ride their sections; their velocity is their section's there.
 float motion=0;
 for(auto&s:m_sections){if(!live(s))continue;s.centre=s.com-(s.axes[0]*(s.comRest-s.restCentre).x+s.axes[1]*(s.comRest-s.restCentre).y+s.axes[2]*(s.comRest-s.restCentre).z);
  for(int i:s.members){auto&p=m_particles[size_t(i)];if(p.region<0)continue;p.x=at(s,p.rest);p.previous=p.x-(s.vel+cross3(s.spin,p.x-s.com))*Tick;p.grounded=p.x.z<p.floor+Skin+.004f;motion=std::max(motion,span(p.x-p.previous));}}
 for(auto&p:m_particles)if(p.hull&&p.region>=0){const auto&s=m_sections[size_t(p.region)];p.x=at(s,p.rest);p.previous=p.x;p.grounded=p.x.z<p.floor+Skin+.004f;}
 for(auto&j:m_joints){if(j.cut)continue;auto&p=m_particles[size_t(j.particle)];p.x=(carry(m_sections[size_t(j.a)],j.rest)+carry(m_sections[size_t(j.b)],j.rest))*.5f;p.previous=p.x;}
 // Every half second, how far any section has got since the last look: points that fidget get nowhere.
 // A big body (a Chimera) is still at the same turn of its parts that leaves a man still: its far points move more.
 const float big=std::max(1.f,m_size/1.9f);
 bool still=false;if(int(m_age/Tick+.5f)%30==29){float most=0;for(auto&s:m_sections){if(s.members.empty())continue;float iq[4],d[4];qconj(s.wasQ,iq);qmul(s.q,iq,d);most=std::max(most,span(s.centre-s.was)+span(qlog(d))*.25f);s.was=s.centre;for(int k=0;k<4;++k)s.wasQ[k]=s.q[k];}still=most<.012f*big&&m_age>.6f;}
 measure();m_age+=Tick;m_dirty=true;m_motion=motion;
 m_quiet=motion<.0024f*big?m_quiet+Tick:0;
 // Carried on something still moving it does not sleep.
 // What settling would still move, tried on a copy: kept when it is small, else fed in through the solver.
 auto trial=[&](float& most){const auto sections=m_sections;const auto particles=m_particles;std::vector<Point3> was(m_sections.size());
  for(size_t s=0;s<m_sections.size();++s)was[s]=m_sections[s].centre;settle(world);most=0;std::vector<Point3> fix(m_sections.size());
  for(size_t s=0;s<m_sections.size();++s){fix[s]=m_sections[s].centre-was[s];most=std::max(most,span(fix[s]));}
  return std::make_tuple(fix,sections,particles);};
 if(settling){float left=0;for(const auto&e:m_ease)left=std::max(left,span(e));
  if(left<1e-4f&&m_lastFeed){float most=0;auto [fix,sections,particles]=trial(most);m_lastFeed=false;
   if(most<.015f){m_ease.clear();m_asleep=true;measure();}else{m_sections=sections;m_particles=particles;m_ease=fix;m_glide=m_settling>1.f;}m_dirty=true;}
  else if(left<1e-4f){float most=0;auto [fix,sections,particles]=trial(most);
   // Settled (under 4 mm): kept. Nearly (under 1.5 cm): fed in once more through the solver, then what is left
   // (a few millimetres) kept as it sleeps. Not settled
   // in a second (the solver and settling disagree:
   // a heel on a stair's edge): the rest glides in, every section the same share, 1.5 cm a frame at most, then sleep.
   if(most<.004f){m_ease.clear();m_asleep=true;measure();}
   else if(most<.015f){m_sections=sections;m_particles=particles;m_ease=fix;m_lastFeed=true;}
   else{m_sections=sections;m_particles=particles;m_ease=fix;m_glide=m_settling>1.f;}m_dirty=true;}}
 else if(!carried&&!propped&&(m_quiet>QuietFor||still||m_age-m_woke>7.f)){
  if(off&128){m_asleep=true;settle(world);measure();}
  else{float most=0;auto [fix,sections,particles]=trial(most);
   if(most<.004f){m_asleep=true;measure();}else{m_sections=sections;m_particles=particles;m_ease=fix;m_settling=0;}m_dirty=true;}}
 ++Steps;StepMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
}

void Carcass::advance(const World& world,float toAge){
 if(!built())return;
 if(m_asleep){
  // Asleep it wakes only when something moves into it: a thrown crate, a creature walking through it.
  bool woken=false;
  if(!(switchedOff()&16))for(const auto&o:m_solids){if(dot3(o.velocity,o.velocity)<.15f*.15f)continue;const float c=std::cos(o.yaw),s=std::sin(o.yaw);
   {const Point3 d=m_centre-o.centre;const float reach=m_radius+span(o.half);if(dot3(d,d)>reach*reach)continue;}
   for(const auto&p:m_particles){if(p.region<0)continue;const Point3 d=p.x-o.centre;const float lx=d.x*c+d.y*s,ly=-d.x*s+d.y*c;
    if(std::fabs(lx)<o.half.x+.02f&&std::fabs(ly)<o.half.y+.02f&&std::fabs(d.z)<o.half.z+.02f){woken=true;break;}}
   if(woken)break;}
  if(!woken){m_age=std::max(m_age,toAge);return;}
  m_asleep=false;m_shoved=true;m_quiet=0;m_woke=m_age;m_dirty=true;if(m_vectra)m_vectra->wake=true;}
 int budget=300;const bool onVectra=vectraBodies();while(m_age+Tick*.5f<toAge&&budget-->0&&!m_asleep){if(onVectra)stepVectra(world);else step(world);}
 if(m_age<toAge&&(m_asleep||budget<=0))m_age=toAge;
}

void Carcass::measure(){Point3 sum{};int n=0;for(auto&p:m_particles)if(p.region!=-2){sum=sum+p.x;++n;}m_centre=sum*(1.f/float(std::max(1,n)));m_radius=.1f;for(auto&p:m_particles)if(p.region!=-2)m_radius=std::max(m_radius,span(p.x-m_centre));m_radius+=.08f*m_size;}

std::vector<Carcass::JointState> Carcass::jointStates()const{
 std::vector<JointState> out;
 for(size_t i=0;i<m_sections.size();++i){const auto&s=m_sections[i];if(!s.limited||s.parent<0)continue;JointState j;j.section=int(i);j.parent=s.parent;j.part=s.part;j.cut=s.joint<0||m_joints[size_t(s.joint)].cut;
  float rel[4];relative(s,rel);const Turn t=decompose(rel,s.twistAxis,s.bendAxis,s.sideAxis);j.bend=t.bend;j.side=t.side;j.twist=t.twist;j.bendLo=s.limit.bendLo;j.bendHi=s.limit.bendHi;j.sideMax=s.limit.side;j.twistMax=s.limit.twist;out.push_back(j);}
 return out;}

// How deep a point is inside section s's body (0 when it is not, or may be: its own, a neighbour, one it touched at rest).
float Carcass::depthIn(const Particle& p,int s)const{
 if(p.region<0||p.hull||s==p.region||neighbours(p.region,s)||(s<32&&(p.ignore>>s&1u)))return 0;const auto&o=m_sections[size_t(s)];if(o.ru<=0||o.members.empty())return 0;
 const Point3 d=p.x-o.centre,l=Point3{dot3(d,o.axes[0]),dot3(d,o.axes[1]),dot3(d,o.axes[2])}+o.restCentre;
 const Point3 axis=o.capB-o.capA;const float len=span(axis);const Point3 U=len>1e-6f?axis*(1/len):Point3{0,0,1};const float t=std::clamp(dot3(l-o.capA,U),0.f,len);const Point3 off=l-(o.capA+U*t);
 const float rw=std::min(o.ru,o.rv),u=dot3(off,o.capU)/o.ru,v=dot3(off,o.capV)/o.rv,w=dot3(off,U)/rw,rho=std::sqrt(u*u+v*v+w*w);return rho<1?(1-rho)*rw:0.f;}
float Carcass::overlap()const{
 float worst=0;const int R=int(m_sections.size());
 for(const auto&p:m_particles)for(int s=0;s<R;++s)worst=std::max(worst,depthIn(p,s));
 return worst;}

void Carcass::regroup(){
 const int R=int(m_sections.size());Groups pieces(R);for(auto&j:m_joints)if(!j.cut)pieces.join(j.a,j.b);
 m_piece.resize(size_t(R));m_pieces=0;for(int s=0;s<R;++s){m_piece[size_t(s)]=pieces.find(s);if(m_piece[size_t(s)]==s)++m_pieces;}
 m_loops.clear();if(m_pieces<2)return;
 // The rim of every wound: seam edges whose two sides have parted, and the open edge of a separate
 // skin that was pinned where the cut fell.
 std::vector<std::vector<int>> open(static_cast<size_t>(R));
 for(int e=0;e<int(m_edges.size());++e){const auto&edge=m_edges[size_t(e)];bool parted=false;
  if(edge.other>=0)parted=m_piece[size_t(edge.owner)]!=m_piece[size_t(edge.other)];
  else{Point3 mid=(m_rest[size_t(edge.a)]+m_rest[size_t(edge.b)])*.5f;for(auto&j:m_joints)if(j.cut&&!j.seam&&(edge.owner==j.a||edge.owner==j.b)&&span(mid-j.rest)<.12f*m_size){parted=true;break;}}
  if(parted)open[size_t(edge.owner)].push_back(e);}
 for(int s=0;s<R;++s){auto&list=open[size_t(s)];if(list.size()<3)continue;
  std::unordered_map<int,int> local;auto id=[&](int v){return local.try_emplace(v,int(local.size())).first->second;};for(int e:list){id(m_edges[size_t(e)].a);id(m_edges[size_t(e)].b);}
  Groups rims(int(local.size()));for(int e:list)rims.join(local[m_edges[size_t(e)].a],local[m_edges[size_t(e)].b]);
  std::unordered_map<int,size_t> made;
  for(int e:list){int rim=rims.find(local[m_edges[size_t(e)].a]);auto [slot,fresh]=made.try_emplace(rim,m_loops.size());if(fresh){Loop loop;loop.owner=s;m_loops.push_back(loop);}m_loops[slot->second].edges.push_back(e);}}
 for(auto&loop:m_loops){Point3 sum{};for(int e:loop.edges)sum=sum+(m_rest[size_t(m_edges[size_t(e)].a)]+m_rest[size_t(m_edges[size_t(e)].b)])*.5f;loop.centre=sum*(1.f/float(loop.edges.size()));
  Point3 body{};int n=0;for(int i:m_sections[size_t(loop.owner)].members)if(m_particles[size_t(i)].region>=0){body=body+m_particles[size_t(i)].rest;++n;}body=body*(1.f/float(std::max(1,n)));
  loop.out=unit(loop.centre-body);loop.across=unit(cross3(loop.out,std::fabs(loop.out.z)<.9f?Point3{0,0,1}:Point3{1,0,0}));loop.up=cross3(loop.out,loop.across);loop.reach=.01f;
  for(int e:loop.edges)for(int v:{m_edges[size_t(e)].a,m_edges[size_t(e)].b})loop.reach=std::max(loop.reach,span(m_rest[size_t(v)]-loop.centre));}
 m_loops.erase(std::remove_if(m_loops.begin(),m_loops.end(),[](const Loop&l){return l.edges.size()<3;}),m_loops.end());
}

// A vertex where its section has it; on a seam that still holds, part way to where the section across
// it has it. Seen from a piece it is not on (the far side of a cut), where that piece has it.
Point3 Carcass::place(int vertex,int viewer)const{
 const auto&b=m_binds[size_t(vertex)];const Point3 rest=m_rest[size_t(vertex)];
 if(m_piece[size_t(b.home)]!=m_piece[size_t(viewer)])return carry(m_sections[size_t(viewer)],rest);
 Point3 at=carry(m_sections[size_t(b.home)],rest);
 // Turned part of the way the section across it has turned, about the joint: a bent knee keeps its
 // thickness (a straight blend of the two places pinches the skin over a bent joint flat).
 if(b.other>=0&&b.blend>0&&b.joint>=0&&!m_joints[size_t(b.joint)].cut&&(switchedOff()&8))at=at+(carry(m_sections[size_t(b.other)],rest)-at)*b.blend;
 else if(b.other>=0&&b.blend>0&&b.joint>=0&&!m_joints[size_t(b.joint)].cut){const auto&home=m_sections[size_t(b.home)],&other=m_sections[size_t(b.other)];
  const Point3 jr=m_joints[size_t(b.joint)].rest,jh=carry(home,jr),jo=carry(other,jr);float ih[4],d[4],r[4];qconj(home.q,ih);qmul(other.q,ih,d);qexp(qlog(d)*b.blend,r);
  at=jh+(jo-jh)*b.blend+qrot(r,at-jh);}
 return at;
}

void Carcass::skin(){
 m_dirty=false;const int V=int(m_binds.size()),T=int(m_owner.size());m_posed.resize(size_t(V));
 for(int v=0;v<V;++v)m_posed[size_t(v)]=place(v,m_binds[size_t(v)].home);
 for(int t=0;t<T;++t)for(int k=0;k<3;++k){int v=m_corner[size_t(t*3+k)],home=m_binds[size_t(v)].home,owner=m_owner[size_t(t)];
  m_triangles[size_t(t)].v[k].p=m_pieces==1||m_piece[size_t(home)]==m_piece[size_t(owner)]?m_posed[size_t(v)]:place(v,owner);}
 m_caps.clear();
 for(const auto&loop:m_loops){const auto&s=m_sections[size_t(loop.owner)];MeshTriangle cap;cap.part=0;Point3 hub=carry(s,loop.centre+loop.out*(.012f*m_size));for(int i:s.members)if(m_particles[size_t(i)].region>=0){hub.z=std::max(hub.z,m_particles[size_t(i)].floor+.012f);break;}cap.v[2]={hub,.5f,.5f,1.f};
  for(int e:loop.edges){int ends[2]{m_edges[size_t(e)].a,m_edges[size_t(e)].b};
   for(int k=0;k<2;++k){Point3 d=m_rest[size_t(ends[k])]-loop.centre;cap.v[k]={place(ends[k],loop.owner),.5f+.48f*dot3(d,loop.across)/loop.reach,.5f+.48f*dot3(d,loop.up)/loop.reach,1.f};}
   m_caps.push_back(cap);}}
}
const std::vector<MeshTriangle>& Carcass::triangles(){if(m_dirty)skin();return m_triangles;}
const std::vector<MeshTriangle>& Carcass::caps(){if(m_dirty)skin();return m_caps;}

// The sections that come away if `section` is cut loose from the trunk. Struck in the trunk itself,
// it is the limb whose joint is nearest the wound.
std::vector<int> Carcass::limb(int section,Point3 hit)const{
 const int R=int(m_sections.size()),piece=m_piece[size_t(section)];int root=-1;
 if(m_piece[size_t(m_root)]==piece)root=m_root;else for(int s=0;s<R;++s)if(m_piece[size_t(s)]==piece&&(root<0||m_sections[size_t(s)].count>m_sections[size_t(root)].count))root=s;
 std::vector<int> parent(size_t(R),-1),order{root};parent[size_t(root)]=root;
 for(size_t head=0;head<order.size();++head){int at=order[head];for(auto&j:m_joints){if(j.cut)continue;int other=j.a==at?j.b:j.b==at?j.a:-1;if(other<0||parent[size_t(other)]>=0)continue;parent[size_t(other)]=at;order.push_back(other);}}
 int top=section;
 if(top==root){float best=1e30f;top=-1;for(auto&j:m_joints){if(j.cut)continue;int other=j.a==root?j.b:j.b==root?j.a:-1;if(other<0||parent[size_t(other)]!=root)continue;float d=span(m_particles[size_t(j.particle)].x-hit);if(d<best){best=d;top=other;}}if(top<0)return {};}
 std::vector<int> torn;for(int s:order){int at=s;while(at!=root&&at!=top)at=parent[size_t(at)];if(at==top)torn.push_back(s);}
 return torn;
}

void Carcass::sever(const std::vector<int>& torn,Point3 direction,float speed,std::vector<Event>& out){
 std::vector<char> in(m_sections.size(),0);Point3 middle{};int mass=0;for(int s:torn){in[size_t(s)]=1;middle=middle+m_sections[size_t(s)].centre*float(m_sections[size_t(s)].count);mass+=m_sections[size_t(s)].count;}
 middle=middle*(1.f/float(std::max(1,mass)));bool any=false;
 for(auto&j:m_joints){if(j.cut||in[size_t(j.a)]==in[size_t(j.b)])continue;j.cut=true;any=true;auto&pin=m_particles[size_t(j.particle)];pin.region=-2;
  for(int s:{j.a,j.b}){auto&section=m_sections[size_t(s)];for(size_t k=0;k<section.members.size();++k)if(section.members[k]==j.particle){section.members.erase(section.members.begin()+std::ptrdiff_t(k));section.weights.erase(section.weights.begin()+std::ptrdiff_t(k));break;}}
  if(out.size()<8)out.push_back({pin.x,unit(middle-pin.x,direction),1.2f,true});}
 if(!any)return;
 // It leaves the way the round was going, turning over as it goes.
 Point3 spin{either(m_seed)*4.f,either(m_seed)*4.f,either(m_seed)*3.f};
 for(int s:torn)for(int i:m_sections[size_t(s)].members){auto&p=m_particles[size_t(i)];Point3 v=direction*speed+Point3{0,0,1.2f}+cross3(spin,p.x-middle);p.previous=p.previous-v*Tick;}
 fit();regroup();measure();m_asleep=false;m_ease.clear();m_glide=false;m_lastFeed=false;m_quiet=0;m_woke=m_age;m_dirty=true;kickVectra();
}

bool Carcass::strike(const Ray& ray,std::vector<Event>& out,int* face){
 if(!built())return false;
 {Point3 to=m_centre-ray.origin;float along=dot3(to,ray.direction);Point3 off=to-ray.direction*along;float reach=m_radius+.1f;if(dot3(off,off)>reach*reach||along<-reach||along>ray.range+reach)return false;}
 const auto&faces=triangles();float best=ray.range;int hit=-1;
 for(int t=0;t<int(faces.size());++t){const auto&f=faces[size_t(t)];Point3 e1=f.v[1].p-f.v[0].p,e2=f.v[2].p-f.v[0].p,h=cross3(ray.direction,e2);float det=dot3(e1,h);if(std::fabs(det)<1e-9f)continue;
  float inv=1/det;Point3 s=ray.origin-f.v[0].p;float u=dot3(s,h)*inv;if(u<0||u>1)continue;Point3 q=cross3(s,e1);float v=dot3(ray.direction,q)*inv;if(v<0||u+v>1)continue;float along=dot3(e2,q)*inv;if(along>.02f&&along<best){best=along;hit=t;}}
 if(hit<0)return false;if(face)*face=hit;
 const Point3 point=ray.origin+ray.direction*best;const int section=m_owner[size_t(hit)];auto&s=m_sections[size_t(section)];
 const float weight=std::clamp(ray.damage/34.f,.4f,2.f);
 for(int i:s.members){auto&p=m_particles[size_t(i)];float near=std::exp(-span(p.x-point)/(.18f*m_size));p.previous=p.previous-ray.direction*(ray.impulse*weight*near*Tick);}
 m_asleep=false;m_ease.clear();m_glide=false;m_lastFeed=false;m_quiet=0;m_woke=m_age;m_dirty=true;kickVectra();
 out.push_back({point,ray.direction,std::clamp(weight,.5f,2.f),false});
 s.damage+=ray.damage;if(!ray.tear)return true;auto torn=limb(section,point);
 if(!torn.empty()){int mass=0;for(int t:torn)mass+=m_sections[size_t(t)].count;float need=std::clamp(30.f+220.f*float(mass)/float(std::max(1,m_body)),40.f,85.f);
  // The trunk takes more before it lets a limb go.
  if(std::find(torn.begin(),torn.end(),section)==torn.end())need*=1.6f;
  if(s.damage>=need){s.damage=0;sever(torn,ray.direction,1.5f+ray.impulse*.5f,out);}}
 return true;
}

void Carcass::blast(Point3 origin,float power,std::vector<Event>& out){
 if(!built())return;
 for(auto&j:m_joints){if(j.cut)continue;j.cut=true;auto&pin=m_particles[size_t(j.particle)];if(out.size()<6)out.push_back({pin.x,unit(pin.x-origin),1.3f,true});pin.region=-2;
  for(int s:{j.a,j.b}){auto&section=m_sections[size_t(s)];for(size_t k=0;k<section.members.size();++k)if(section.members[k]==j.particle){section.members.erase(section.members.begin()+std::ptrdiff_t(k));section.weights.erase(section.weights.begin()+std::ptrdiff_t(k));break;}}}
 for(auto&s:m_sections){Point3 out=s.centre-origin;out.z=out.z*.3f+.2f;Point3 away=unit(out),spin{either(m_seed)*7.f,either(m_seed)*7.f,either(m_seed)*5.f};float speed=power*(2.2f+2.2f*chance(m_seed)),lift=1.f+1.4f*chance(m_seed);
  for(int i:s.members){auto&p=m_particles[size_t(i)];Point3 v=away*speed+Point3{0,0,lift}+cross3(spin,p.x-s.centre);p.previous=p.previous-v*Tick;}}
 fit();regroup();measure();m_asleep=false;m_ease.clear();m_glide=false;m_lastFeed=false;m_quiet=0;m_woke=m_age;m_dirty=true;kickVectra();
}
bool Carcass::test(){
 World world(3);std::ofstream report("gore-test.txt");
 // A clear patch of floor to drop it on.
 Point3 spot{12,12,0};bool found=false;
 for(float y=3;y<21&&!found;y+=.5f)for(float x=3;x<21&&!found;x+=.5f){float floor=world.supportBelow(x,y,.5f);bool clear=std::fabs(floor)<.3f;
  for(float dy=-1.5f;dy<=1.5f&&clear;dy+=.5f)for(float dx=-1.5f;dx<=1.5f&&clear;dx+=.5f)clear=world.fits(x+dx,y+dy,floor+.02f,1.9f,false)&&std::fabs(world.supportBelow(x+dx,y+dy,floor+.5f)-floor)<.01f;
  if(clear){spot={x,y,floor};found=true;}}
 // A standing trunk with two arms that are skins of their own, as bought models often are.
 std::vector<MeshTriangle> body;auto face=[&](Point3 a,Point3 b,Point3 c){MeshTriangle t;t.v[0]={a+spot,0,0,1};t.v[1]={b+spot,1,0,1};t.v[2]={c+spot,0,1,1};body.push_back(t);};
 const int rings=15,sides=10;auto ring=[&](int i,int k){float a=kTwoPi*float(k%sides)/float(sides),z=.03f+1.6f*float(i)/float(rings-1),r=.17f-.05f*std::fabs(float(i)/float(rings-1)-.6f);return Point3{std::cos(a)*r,std::sin(a)*r,z};};
 for(int i=0;i+1<rings;++i)for(int k=0;k<sides;++k){face(ring(i,k),ring(i,k+1),ring(i+1,k+1));face(ring(i,k),ring(i+1,k+1),ring(i+1,k));}
 for(int k=0;k<sides;++k){face(ring(0,k+1),ring(0,k),{0,0,.03f});face(ring(rings-1,k),ring(rings-1,k+1),{0,0,1.63f});}
 for(float side:{-1.f,1.f})for(int i=0;i<4;++i){float x0=side*(.15f+.12f*float(i)),x1=side*(.15f+.12f*float(i+1));
  Point3 c[8];for(int n=0;n<8;++n)c[n]={n&1?x1:x0,n&2?.05f:-.05f,n&4?1.28f:1.18f};
  for(auto q:{std::array<int,4>{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6},{0,1,3,2},{4,5,7,6}}){if((i>0&&q==std::array<int,4>{0,2,6,4})||(i<3&&q==std::array<int,4>{1,3,7,5}))continue;face(c[q[0]],c[q[1]],c[q[2]]);face(c[q[0]],c[q[2]],c[q[3]]);}}
 auto over=[&](Point3 p){float support=world.supportBelow(p.x,p.y,p.z+.3f);return std::fabs(support-spot.z)>.5f?.05f:p.z-support;};
 auto lowest=[&](Carcass&c){float low=1e30f;for(auto&t:c.triangles())for(auto&v:t.v)low=std::min(low,over(v.p));for(auto&t:c.caps())for(auto&v:t.v)low=std::min(low,over(v.p));return low;};
 auto highest=[&](Carcass&c){float high=-1e30f;for(auto&t:c.triangles())for(auto&v:t.v)high=std::max(high,over(v.p));return high;};
 auto sound=[&](Carcass&c){for(auto&t:c.triangles())for(auto&v:t.v)if(!std::isfinite(v.p.x+v.p.y+v.p.z))return false;return true;};
 auto stretch=[&](Carcass&c){float worst=1;const auto&now=c.triangles();for(size_t t=0;t<now.size();++t)for(int k=0;k<3;++k){float before=span(body[t].v[(k+1)%3].p-body[t].v[k].p),after=span(now[t].v[(k+1)%3].p-now[t].v[k].p);if(before>.01f)worst=std::max(worst,after/before);}return worst;};
 bool pass=found;report<<"spot "<<spot.x<<','<<spot.y<<" floor "<<spot.z<<" found "<<found<<'\n';
 // It falls, folds and comes to rest on the floor.
 Carcass fallen;fallen.build(body,world,{1.6f,.4f,.3f},1,7);const int sections=fallen.sections();
 report<<"built: triangles "<<body.size()<<" sections "<<sections<<" joints "<<fallen.joints()<<" seams "<<fallen.joints(true)<<" pieces "<<fallen.pieces()<<'\n';
 pass&=sections>=4&&fallen.joints(true)>=2&&fallen.joints()>fallen.joints(true)&&fallen.pieces()==1;
 float settled=-1;for(int i=1;i<=480;++i){fallen.advance(world,float(i)/60);if(fallen.asleep()&&settled<0)settled=float(i)/60;if(i==30||i==60||i==120||i==240)report<<" t "<<float(i)/60<<" top "<<highest(fallen)<<" low "<<lowest(fallen)<<" motion "<<fallen.m_motion<<'\n';}
 report<<"fallen: settled at "<<settled<<" top "<<highest(fallen)<<" lowest "<<lowest(fallen)<<" stretch "<<stretch(fallen)<<" pieces "<<fallen.pieces()<<'\n';
 pass&=settled>0&&settled<7.5f&&highest(fallen)<.85f&&lowest(fallen)>-.005f&&sound(fallen)&&stretch(fallen)<2.2f&&fallen.pieces()==1&&fallen.caps().empty();
 // The same body, the same way, every time: the renderer may rebuild it at any age.
 Carcass again;again.build(body,world,{1.6f,.4f,.3f},1,7);again.advance(world,8);float drift=0;for(size_t t=0;t<body.size();++t)drift=std::max(drift,span(again.triangles()[t].v[0].p-fallen.triangles()[t].v[0].p));
 report<<"rebuilt drift "<<drift<<'\n';pass&=drift<.0001f;
 // Shot through the upper trunk until that section comes away, with a face over each side of the cut.
 Carcass shot;shot.build(body,world,{},1,11);std::vector<Event> events;int rounds=0,struck=-1;
 for(;rounds<8&&shot.pieces()==1;++rounds){shot.advance(world,float(rounds)*.02f);Ray ray{spot+Point3{-2,.01f,1.45f},{1,0,0},6,34,2.5f};if(!shot.strike(ray,events,&struck))break;}
 int stumps=0;for(auto&e:events)stumps+=e.stump;const size_t capFaces=shot.caps().size();
 report<<"shot: rounds "<<rounds<<" pieces "<<shot.pieces()<<" cap faces "<<capFaces<<" stump events "<<stumps<<" face "<<struck<<'\n';
 pass&=rounds>=2&&shot.pieces()==2&&capFaces>=6&&stumps>=1&&struck>=0;
 shot.advance(world,6);report<<"shot rests: asleep "<<shot.asleep()<<" lowest "<<lowest(shot)<<" top "<<highest(shot)<<'\n';pass&=lowest(shot)>-.005f&&sound(shot)&&highest(shot)<1.f;
 // An arm is a skin of its own: it still comes off at the shoulder.
 Carcass arm;arm.build(body,world,{},1,13);events.clear();int armRounds=0;
 for(;armRounds<8&&arm.pieces()==1;++armRounds){Ray ray{spot+Point3{.42f,-2,1.23f},{0,1,0},6,34,2.5f};if(!arm.strike(ray,events))break;}
 report<<"arm: rounds "<<armRounds<<" pieces "<<arm.pieces()<<'\n';pass&=arm.pieces()==2;
 // Blown apart: every section its own piece, every wound closed.
 Carcass blown;blown.build(body,world,{},1,17);events.clear();blown.blast(spot+Point3{.6f,0,.3f},1.6f,events);const size_t blownCaps=blown.caps().size();
 report<<"blown: pieces "<<blown.pieces()<<" of "<<blown.sections()<<" cap faces "<<blownCaps<<" events "<<events.size()<<'\n';pass&=blown.pieces()==blown.sections()&&blownCaps>=size_t(blown.joints(true))*6&&!events.empty();
 blown.advance(world,1);const float spread=blown.radius();blown.advance(world,8);
 report<<"blown rests: asleep "<<blown.asleep()<<" lowest "<<lowest(blown)<<" spread "<<spread<<" top "<<highest(blown)<<'\n';pass&=lowest(blown)>-.005f&&sound(blown)&&spread>1.f&&highest(blown)<.9f;
 report<<(pass?"PASS":"FAIL")<<'\n';return pass;
}

// ---- A body's pose for the save (physics layout 4) ----
Carcass::Pose Carcass::pose()const{
 Pose p;p.age=m_age;p.asleep=m_asleep;p.shoved=m_shoved;p.cut.reserve(m_joints.size());for(const auto&j:m_joints)p.cut.push_back(j.cut?1:0);
 p.sections.reserve(m_sections.size());
 for(const auto&s:m_sections)p.sections.push_back({s.centre.x,s.centre.y,s.centre.z,s.q[0],s.q[1],s.q[2],s.q[3],s.com.x,s.com.y,s.com.z,s.vel.x,s.vel.y,s.vel.z,s.spin.x,s.spin.y,s.spin.z});
 return p;}
bool Carcass::restorePose(const Pose& p){
 if(!built()||p.cut.size()!=m_joints.size()||p.sections.size()!=m_sections.size()||!std::isfinite(p.age)||p.age<0)return false;
 for(const auto&a:p.sections){for(float v:a)if(!std::isfinite(v)||std::fabs(v)>1e6f)return false;const float n=a[3]*a[3]+a[4]*a[4]+a[5]*a[5]+a[6]*a[6];if(n<.81f||n>1.21f)return false;}
 for(size_t i=0;i<m_joints.size();++i)if(m_joints[i].cut&&!p.cut[i])return false;   // a cut is never undone
 // The joints it lost, as a cut leaves them (the pin out of both sections), and the pieces and the caps again.
 bool cut=false;
 for(size_t i=0;i<m_joints.size();++i){auto&j=m_joints[i];if(j.cut||!p.cut[i])continue;j.cut=true;cut=true;auto&pin=m_particles[size_t(j.particle)];pin.region=-2;
  for(int s:{j.a,j.b}){auto&section=m_sections[size_t(s)];for(size_t k=0;k<section.members.size();++k)if(section.members[k]==j.particle){section.members.erase(section.members.begin()+std::ptrdiff_t(k));section.weights.erase(section.weights.begin()+std::ptrdiff_t(k));break;}}}
 if(cut){fit();regroup();}
 if(!m_bodies)bodies();
 for(size_t k=0;k<m_sections.size();++k){auto&s=m_sections[k];const auto&a=p.sections[k];
  s.centre={a[0],a[1],a[2]};s.q[0]=a[3];s.q[1]=a[4];s.q[2]=a[5];s.q[3]=a[6];s.com={a[7],a[8],a[9]};s.vel={a[10],a[11],a[12]};s.spin={a[13],a[14],a[15]};axesOf(s.q,s.axes);}
 for(auto&pt:m_particles){if(pt.region<0)continue;const auto&s=m_sections[size_t(pt.region)];pt.x=carry(s,pt.rest);pt.previous=pt.hull?pt.x:pt.x-(s.vel+cross3(s.spin,pt.x-s.com))*Tick;pt.probe={1e9f,1e9f,1e9f};}
 for(auto&j:m_joints){if(j.cut)continue;auto&pt=m_particles[size_t(j.particle)];pt.x=(carry(m_sections[size_t(j.a)],j.rest)+carry(m_sections[size_t(j.b)],j.rest))*.5f;pt.previous=pt.x;}
 for(auto&s:m_sections)if(s.limited&&s.parent>=0&&!s.members.empty()&&!m_sections[size_t(s.parent)].members.empty())relative(s,s.last);
 m_age=p.age;m_asleep=p.asleep;m_shoved=p.shoved;m_woke=m_age;m_quiet=0;m_ease.clear();m_glide=false;m_lastFeed=false;m_settling=0;m_vectra.reset();
 measure();m_dirty=true;return true;}

// ---- The body on Vectra (the default; RM_RAGDOLL=legacy, or carcass.vectra 0, for the position-based step) ----
// Each section is a Vectra body that lasts as long as the body does (its points' mass and inertia), each joint a Vectra
// ball at its pivot with the rig's limits on the swing-twist split the rig measures (Joint::swingLimited: a bend between
// its two stops, a side lean and a twist either way), held by position as well (Joint::positionCorrection), so a knee
// never shows past its stop. Sections not joined keep out of each other by their skin points against the other's
// capsule (Space::pairContacts, the very measure the lab checks). The floor and the walls meet the skin points; the
// props near it are kinematic boxes that shove it at their own share. It sleeps when Vectra's island sleeps, where it
// lies: nothing is moved on the way to sleep.
bool Carcass::vectraBodies(){
 static const int env=[]{const char* v=std::getenv("RM_RAGDOLL");if(!v)return -1;const std::string s(v);return s=="legacy"?0:s=="vectra"?1:-1;}();
 if(env>=0)return env==1;return tune("carcass.vectra",1)>.5f;}
namespace {
using vectra::Quat;
Quat quatOfAxes(Point3 x,Point3 y,Point3 z){
 const float m00=x.x,m10=x.y,m20=x.z,m01=y.x,m11=y.y,m21=y.z,m02=z.x,m12=z.y,m22=z.z,tr=m00+m11+m22;Quat q;
 if(tr>0){const float t=std::sqrt(tr+1)*2;q={.25f*t,(m21-m12)/t,(m02-m20)/t,(m10-m01)/t};}
 else if(m00>m11&&m00>m22){const float t=std::sqrt(1+m00-m11-m22)*2;q={(m21-m12)/t,.25f*t,(m01+m10)/t,(m02+m20)/t};}
 else if(m11>m22){const float t=std::sqrt(1+m11-m00-m22)*2;q={(m02-m20)/t,(m01+m10)/t,.25f*t,(m12+m21)/t};}
 else{const float t=std::sqrt(1+m22-m00-m11)*2;q={(m10-m01)/t,(m02+m20)/t,(m12+m21)/t,.25f*t};}
 return q.normalized();}
}
// A round, a blast or a cut pushed the points: the bodies take it on their next step.
void Carcass::kickVectra(){if(m_vectra){m_vectra->kicked=true;m_vectra->wake=true;}}
void Carcass::stepVectra(const World& world){
 using namespace vectra;
 const auto started=std::chrono::steady_clock::now();
 if(!m_bodies)bodies();knobs();
 if(!m_vectra)m_vectra=std::make_shared<VectraState>();auto& vs=*m_vectra;Space& space=vs.space;
 const int R=int(m_sections.size());auto live=[&](const Section&s){return s.mass>0&&!s.members.empty();};
 auto at=[&](const Section&s,Point3 rest){const Point3 d=rest-s.comRest;return s.com+s.axes[0]*d.x+s.axes[1]*d.y+s.axes[2]*d.z;};
 // A man weighs 75 kg (a body of another size by its volume), shared by the sections' points.
 float points=0;for(const auto&s:m_sections)if(live(s))points+=s.mass;const float kg=75.f*std::pow(std::max(.3f,m_size/1.9f),3.f)/std::max(1.f,points);
 const Material flesh{tune("carcass.vstatic",1.f),tune("carcass.vkinetic",.85f),0,tune("carcass.vroll",.03f)};
 // The velocities each section has, from its points (when a round or a blast pushed them, and at the start).
 auto fromPoints=[&](Section& s){axesOf(s.q,s.axes);s.com=carry(s,s.comRest);Point3 v{};for(int i:s.members){const auto&p=m_particles[size_t(i)];if(p.region>=0)v=v+(p.x-p.previous);}v=v*(1/(s.mass*Tick));
  Point3 L{};for(int i:s.members){const auto&p=m_particles[size_t(i)];if(p.region<0)continue;L=L+cross3(p.x-s.com,(p.x-p.previous)*(1/Tick)-v);}
  s.vel=v;s.spin=turnBy(s.axes,s.invInertia,L);const float sp=span(s.vel);if(sp>18.f)s.vel=s.vel*(18.f/sp);const float w=span(s.spin);if(w>30.f)s.spin=s.spin*(30.f/w);};
 // ---- The bodies, once.
 if(!vs.built){vs.built=true;vs.sections=R;vs.bodyOf.assign(size_t(R),-1);
  space.settings.substeps=std::max(1,int(tune("carcass.vsubsteps",8)));space.settings.jointIterations=std::max(1,int(tune("carcass.viterations",6)));space.settings.projectIterations=std::max(0,int(tune("carcass.vproject",3)));
  space.settings.sleepTime=tune("carcass.vsleepTime",.4f);space.settings.sleepLinear=tune("carcass.vsleepLinear",.05f);space.settings.sleepAngular=tune("carcass.vsleepAngular",.15f);
  for(int k=0;k<R;++k){auto&s=m_sections[size_t(k)];if(!live(s))continue;fromPoints(s);
   // Its box for the props it meets: the capsule's frame, as far as its skin reaches each way (the points and the hull).
   const Point3 axis=s.capB-s.capA;const float len=span(axis);const Point3 U=len>1e-6f?axis*(1/len):Point3{0,0,1};
   const Point3 a=unit(s.capU-U*dot3(s.capU,U),{1,0,0}),b=cross3(U,a);Point3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
   for(const auto&p:m_particles){if(p.region!=k)continue;const Point3 d=p.rest-s.comRest;const Point3 l{dot3(d,a),dot3(d,b),dot3(d,U)};lo={std::min(lo.x,l.x),std::min(lo.y,l.y),std::min(lo.z,l.z)};hi={std::max(hi.x,l.x),std::max(hi.y,l.y),std::max(hi.z,l.z)};}
   if(lo.x>hi.x){lo=Point3{-.03f,-.03f,-.03f};hi=Point3{.03f,.03f,.03f};}
   const Point3 mid=(lo+hi)*.5f,half{std::max(.02f,(hi.x-lo.x)*.5f),std::max(.02f,(hi.y-lo.y)*.5f),std::max(.02f,(hi.z-lo.z)*.5f)};
   vs.shapes.emplace_back();Shape& sh=vs.shapes.back();sh.add(PartKind::Box,s.comRest+a*mid.x+b*mid.y+U*mid.z,half,quatOfAxes(a,b,U));sh.finish();sh.com=s.comRest;sh.bound=span(sh.parts[0].centre-s.comRest)+span(half);
   Body body;body.shape=&sh;body.x=s.com;body.q=Quat{s.q[0],s.q[1],s.q[2],s.q[3]}.normalized();body.v=s.vel;body.w=s.spin;body.material=flesh;body.invMass=1/(s.mass*kg);
   M3 inv;for(int u=0;u<3;++u)for(int v=0;v<3;++v)inv.m[u][v]=s.invInertia[u*3+v]/kg;body.invInertiaLocal=inv;body.inertiaLocal=inv.inverse();body.updateInertia();
   body.linearDamping=tune("carcass.vdamp",.1f);body.angularDamping=tune("carcass.vspin",1.f);body.id=k;body.user=k;
   vs.bodyOf[size_t(k)]=int(space.bodies.size());space.bodies.push_back(body);}
  vs.sections=int(space.bodies.size());vs.kicked=false;
  // The skin each section meets the world by: its vertices farthest along 96 ways spread over the sphere.
  {vs.skin.assign(size_t(R),{});const int V=int(m_rest.size());constexpr int Ways=96;std::vector<Point3> ways;ways.reserve(Ways);
   for(int i=0;i<Ways;++i){const float z=1-2*(float(i)+.5f)/float(Ways),r=std::sqrt(std::max(0.f,1-z*z)),a=float(i)*2.39996323f;ways.push_back({r*std::cos(a),r*std::sin(a),z});}
   std::vector<std::vector<int>> mine(static_cast<size_t>(R));for(int v=0;v<V;++v){const int h=m_binds[size_t(v)].home;if(h>=0&&h<R)mine[size_t(h)].push_back(v);}
   for(int k=0;k<R;++k){std::vector<int> picked;for(const auto&way:ways){int best=-1;float most=-1e30f;for(int v:mine[size_t(k)]){const float d=dot3(m_rest[size_t(v)],way);if(d>most){most=d;best=v;}}
     if(best>=0&&std::find(picked.begin(),picked.end(),best)==picked.end())picked.push_back(best);}
    std::sort(picked.begin(),picked.end());for(int v:picked){VectraState::SkinPoint sp;sp.rest=m_rest[size_t(v)];sp.safe=sp.rest;vs.skin[size_t(k)].push_back(sp);}}}}
 const int sections=vs.sections;
 // ---- Pushed since the last step: from the points.
 if(vs.kicked){vs.kicked=false;for(int k=0;k<R;++k){auto&s=m_sections[size_t(k)];if(!live(s)||vs.bodyOf[size_t(k)]<0)continue;fromPoints(s);auto&b=space.bodies[size_t(vs.bodyOf[size_t(k)])];b.v=s.vel;b.w=s.spin;}}
 if(vs.wake){vs.wake=false;for(int k=0;k<sections;++k){auto&b=space.bodies[size_t(k)];b.sleeping=false;b.sleepTime=0;}}
 // ---- The joints, again whenever one is cut.
 {int cuts=0;for(const auto&j:m_joints)cuts+=j.cut;
  if(cuts!=vs.cuts){vs.cuts=cuts;vs.joints.clear();vs.joints.reserve(m_joints.size());vs.paired.assign(size_t(R)*size_t(R),0);
   const float lockHertz=tune("carcass.vlock",240.f);
   for(int j=0;j<int(m_joints.size());++j){const auto&jt=m_joints[size_t(j)];if(jt.cut||vs.bodyOf[size_t(jt.a)]<0||vs.bodyOf[size_t(jt.b)]<0)continue;
    vs.paired[size_t(jt.a)*size_t(R)+size_t(jt.b)]=vs.paired[size_t(jt.b)*size_t(R)+size_t(jt.a)]=1;
    // Which side is the child (its limits are about its own axes).
    int child=-1;for(int c:{jt.a,jt.b})if(m_sections[size_t(c)].joint==j&&m_sections[size_t(c)].parent==(c==jt.a?jt.b:jt.a))child=c;
    vectra::Joint J;J.kind=JointKind::Ball;J.key=std::uint64_t(j+1);J.positionCorrection=true;J.lockHertz=lockHertz;J.lockDamping=1;
    if(child>=0&&m_sections[size_t(child)].limited&&!(switchedOff()&1)){const auto&s=m_sections[size_t(child)];const auto&p=m_sections[size_t(s.parent)];
     J.a=vs.bodyOf[size_t(s.parent)];J.b=vs.bodyOf[size_t(child)];
     // The limits' frame in bind space (x the bend axis, y the side, z the twist), carried by each body's bind turn: the
     // turn of B's frame from A's is then exactly the turn Carcass::jointStates splits into bend, side and twist.
     const Quat R0=quatOfAxes(s.bendAxis,s.sideAxis,s.twistAxis);
     J.frameA=Quat{p.bind[0],p.bind[1],p.bind[2],p.bind[3]}*R0;J.frameB=Quat{s.bind[0],s.bind[1],s.bind[2],s.bind[3]}*R0;
     J.swingLimited=true;J.swingLower={s.limit.bendLo,-s.limit.side,0};J.swingUpper={s.limit.bendHi,s.limit.side,0};
     J.twistLimited=true;J.twistLower=-s.limit.twist;J.twistUpper=s.limit.twist;
     if(const float spine=tune("carcass.vspine",0)*.01745f;spine>0&&(s.part==1||s.part==2)){const Turn t0=decompose(s.rel0,s.twistAxis,s.bendAxis,s.sideAxis);
      J.swingLower={std::max(J.swingLower.x,t0.bend-spine),std::max(J.swingLower.y,t0.side-spine),0};J.swingUpper={std::min(J.swingUpper.x,t0.bend+spine),std::min(J.swingUpper.y,t0.side+spine),0};
      J.twistLower=std::max(J.twistLower,t0.twist-spine);J.twistUpper=std::min(J.twistUpper,t0.twist+spine);}
     J.anchorA=jt.rest-p.comRest;J.anchorB=jt.rest-s.comRest;
     // Friction at the joint: a share of what the limb's own weight turns it by, held level from the joint.
     J.frictionTorque=tune("carcass.vfriction",0)*s.mass*kg*14.f*span(s.comRest-jt.rest);}
    else{J.a=vs.bodyOf[size_t(jt.a)];J.b=vs.bodyOf[size_t(jt.b)];J.anchorA=jt.rest-m_sections[size_t(jt.a)].comRest;J.anchorB=jt.rest-m_sections[size_t(jt.b)].comRest;}
    vs.joints.push_back(J);}
   space.constraints.clear();for(auto&J:vs.joints)space.constraints.push_back(&J);}}
 // ---- The props near it: kinematic boxes, going at their share of their own speed (rebuilt each step).
 space.bodies.resize(size_t(sections));vs.solidShapes.clear();
 if(!(switchedOff()&16))for(const auto&o:m_solids){const float reach=m_radius+span(o.half)+.3f;const Point3 d=o.centre-m_centre;if(dot3(d,d)>reach*reach||o.half.x<=0)continue;
  vs.solidShapes.emplace_back();Shape& sh=vs.solidShapes.back();sh.add(PartKind::Box,{},o.half);sh.finish();Body body;body.shape=&sh;body.x=o.centre;body.q=Quat::axisAngle({0,0,1},o.yaw);
  body.kinematic=true;body.invMass=0;body.v=o.velocity*o.shove;
  // One going through it (a thrown crate) is a spring, not a wall: it shoves the body by its share, never presses one part into another.
  if(dot3(o.velocity,o.velocity)>.15f*.15f)body.softHertz=tune("carcass.vshoveHertz",12.f);
  body.material={.8f,.7f,0,.01f};body.id=1000+int(space.bodies.size());body.user=-1;space.bodies.push_back(body);}
 // ---- What meets what. Joined sections do not meet; the others meet by their skins (pairContacts).
 const auto& paired=vs.paired;
 space.filter=[&](const Space& sp,int a,int b){const int ua=sp.bodies[size_t(a)].user,ub=sp.bodies[size_t(b)].user;if(ua<0||ub<0)return true;if(switchedOff()&4)return false;return !paired[size_t(ua)*size_t(R)+size_t(ub)];};
 // The skin points of one against the other's capsule (Carcass::depthIn's measure), both ways: a manifold of one point
 // per skin point within reach, its normal out of the capsule.
 space.pairContacts=[&](Space& sp,int ia,int ib,float margin,std::vector<Manifold>& out){const Body& A=sp.bodies[size_t(ia)];const Body& B=sp.bodies[size_t(ib)];if(A.user<0||B.user<0)return false;
  auto oneWay=[&](int ip,int ic){const Body& P=sp.bodies[size_t(ip)];const Body& C=sp.bodies[size_t(ic)];const auto&own=m_sections[size_t(P.user)];const auto&o=m_sections[size_t(C.user)];if(o.ru<=0)return;
   const Point3 axis=o.capB-o.capA;const float len=span(axis);const Point3 U=len>1e-6f?axis*(1/len):Point3{0,0,1};const float rw=std::min(o.ru,o.rv);const float reach=o.capReach+margin;
   for(int i:own.members){const auto&pt=m_particles[size_t(i)];if(pt.region<0||pt.hull||(C.user<32&&(pt.ignore>>C.user&1u)))continue;
    const V3 w=P.toWorld(pt.rest);const V3 rel=w-C.x;if(dot(rel,rel)>reach*reach)continue;const Point3 l=C.q.unrotate(rel)+C.shape->com;
    const float t=std::clamp(dot3(l-o.capA,U),0.f,len);const Point3 off=l-(o.capA+U*t);const float u=dot3(off,o.capU)/o.ru,v=dot3(off,o.capV)/o.rv,ww=dot3(off,U)/rw;
    const float rho=std::sqrt(u*u+v*v+ww*ww);if(rho<1e-4f)continue;
    // The capsule's measure grows by |grad| a metre: its gap is (rho - 1) / |grad|, along grad.
    const Point3 g=(o.capU*(u/o.ru)+o.capV*(v/o.rv)+U*(ww/rw))*(1/rho);const float gl=span(g);if(gl<1e-6f)continue;const float sep=(rho-1)/gl;if(sep>margin)continue;
    Manifold m;m.a=ic;m.b=ip;m.partA=0;m.partB=i+1;m.normal=C.q.rotate(g*(1/gl));m.material=Material::mix(flesh,flesh);
    auto&c=m.points[0];c=ContactPoint{};c.p=w;c.separation=sep;c.localA=C.q.unrotate(w-C.x);c.localB=P.q.unrotate(w-P.x);c.feature=std::uint32_t(i);m.count=1;out.push_back(m);}};
  oneWay(ia,ib);oneWay(ib,ia);return true;};
 // ---- The world: each section's skin points against what is under them, and against walls.
 space.worldContacts=[&](Space& sp,int bi,float margin,std::vector<Manifold>& out){const Body& body=sp.bodies[size_t(bi)];if(body.user<0)return;auto&s=m_sections[size_t(body.user)];
  struct Hit {Point3 p;float sep;int particle;};static thread_local std::vector<Hit> floor,wall;static thread_local std::vector<Point3> wallN;floor.clear();wall.clear();wallN.clear();
  for(int i:s.members){auto&pt=m_particles[size_t(i)];if(pt.region<0)continue;const V3 w=body.toWorld(pt.rest);const Point3 p{w.x,w.y,w.z};
   float dx=p.x-pt.probe.x,dy=p.y-pt.probe.y;if(dx*dx+dy*dy>ProbeStep*ProbeStep||std::fabs(p.z-pt.probe.z)>.1f){pt.floor=world.supportBelow(p.x,p.y,p.z+.08f);pt.probe=p;}
   const float sep=p.z-pt.floor-Skin;if(sep<margin&&sep>-.25f)floor.push_back({p,sep,i});
   if(!world.fits(p.x,p.y,p.z+.03f,.001f,false)&&sep>.02f&&!pt.hull){Point3 n=p-pt.safe;n.z=0;const float l=span(n);if(l>1e-4f&&l<.3f){wall.push_back({p,-l,i});wallN.push_back(n*(1/l));}}
   else if(world.fits(p.x,p.y,p.z+.03f,.001f,false))pt.safe=p;}
  // Hull points ride their section but are not its members: they are listed by region.
  for(size_t i=size_t(m_body);i<m_particles.size();++i){auto&pt=m_particles[i];if(!pt.hull||pt.region!=body.user)continue;const V3 w=body.toWorld(pt.rest);const Point3 p{w.x,w.y,w.z};
   float dx=p.x-pt.probe.x,dy=p.y-pt.probe.y;if(dx*dx+dy*dy>ProbeStep*ProbeStep||std::fabs(p.z-pt.probe.z)>.1f){pt.floor=world.supportBelow(p.x,p.y,p.z+.08f);pt.probe=p;}
   const float sep=p.z-pt.floor-Skin;if(sep<margin&&sep>-.25f)floor.push_back({p,sep,int(i)});}
  // Its outermost skin: against the floor and the walls as its points are.
  {auto&skin=vs.skin[size_t(body.user)];for(size_t i=0;i<skin.size();++i){auto&pt=skin[i];const V3 w=body.toWorld(pt.rest);const Point3 p{w.x,w.y,w.z};
    float dx=p.x-pt.probe.x,dy=p.y-pt.probe.y;if(dx*dx+dy*dy>ProbeStep*ProbeStep||std::fabs(p.z-pt.probe.z)>.1f){pt.floor=world.supportBelow(p.x,p.y,p.z+.08f);pt.probe=p;}
    const float sep=p.z-pt.floor-Skin;if(sep<margin&&sep>-.25f)floor.push_back({p,sep,100000+int(i)});
    const bool clear=world.fits(p.x,p.y,p.z+.03f,.001f,false);
    if(!clear&&sep>.02f){Point3 n=p-pt.safe;n.z=0;const float l=span(n);if(l>1e-4f&&l<.3f){wall.push_back({p,-l,100000+int(i)});wallN.push_back(n*(1/l));}}else if(clear)pt.safe=p;}}
  auto emit=[&](std::vector<Hit>& hits,V3 normal,int cluster){if(hits.empty())return;std::sort(hits.begin(),hits.end(),[](const Hit&a,const Hit&b){return a.sep!=b.sep?a.sep<b.sep:a.particle<b.particle;});
   Manifold m;m.a=bi;m.b=-1;m.partA=0;m.partB=cluster;m.normal=normal;m.material=Material::mix(flesh,{.7f,.6f,0,.004f});
   // The deepest point, then the ones farthest from those already taken.
   std::vector<int> pick{0};while(int(pick.size())<4&&pick.size()<hits.size()){int best=-1;float widest=-1;for(int h=0;h<int(hits.size());++h){float closest=1e9f;for(int k:pick)closest=std::min(closest,span(hits[size_t(h)].p-hits[size_t(k)].p));if(closest>widest){widest=closest;best=h;}}if(widest<.02f)break;pick.push_back(best);}
   for(int k:pick){const auto&h=hits[size_t(k)];auto&c=m.points[size_t(m.count++)];c=ContactPoint{};c.p={h.p.x,h.p.y,h.p.z};c.separation=h.sep;c.localA=body.q.unrotate(c.p-body.x);c.localB=c.p;c.feature=std::uint32_t(h.particle);}
   out.push_back(m);};
  emit(floor,{0,0,-1},0);
  if(!wall.empty()){Point3 n{};for(auto w:wallN)n=n+w;n=unit(n);emit(wall,{n.x,n.y,0},1);}};
 space.step(Tick);
 space.pairContacts=nullptr;space.worldContacts=nullptr;space.filter=nullptr;
 // ---- Back to the sections and their points.
 float motion=0;
 for(int k=0;k<R;++k){auto&s=m_sections[size_t(k)];if(!live(s)||vs.bodyOf[size_t(k)]<0)continue;const Body& b=space.bodies[size_t(vs.bodyOf[size_t(k)])];
  s.com=b.x;s.q[0]=b.q.w;s.q[1]=b.q.x;s.q[2]=b.q.y;s.q[3]=b.q.z;qnorm(s.q);axesOf(s.q,s.axes);s.vel=b.v;s.spin=b.w;
  const Point3 off=s.comRest-s.restCentre;s.centre=s.com-(s.axes[0]*off.x+s.axes[1]*off.y+s.axes[2]*off.z);
  for(int i:s.members){auto&p=m_particles[size_t(i)];if(p.region<0)continue;const Point3 was=p.x;p.x=at(s,p.rest);p.previous=p.x-(s.vel+cross3(s.spin,p.x-s.com))*Tick;p.grounded=p.x.z<p.floor+Skin+.004f;motion=std::max(motion,span(p.x-was));}}
 for(auto&p:m_particles)if(p.hull&&p.region>=0){const auto&s=m_sections[size_t(p.region)];p.x=at(s,p.rest);p.previous=p.x;}
 for(auto&j:m_joints){if(j.cut)continue;auto&p=m_particles[size_t(j.particle)];p.x=(carry(m_sections[size_t(j.a)],j.rest)+carry(m_sections[size_t(j.b)],j.rest))*.5f;p.previous=p.x;}
 for(auto&s:m_sections)if(s.limited&&s.parent>=0&&live(s)&&live(m_sections[size_t(s.parent)]))relative(s,s.last);
 if(std::getenv("RM_VECTRA_DEBUG")){static std::ofstream log("vectra-debug.txt");log<<"age "<<m_age<<" gaps(mm):";for(const auto&j:m_joints){if(j.cut)continue;log<<' '<<span(carry(m_sections[size_t(j.a)],j.rest)-carry(m_sections[size_t(j.b)],j.rest))*1000;}
  log<<" manifolds "<<space.manifolds.size()<<'\n';for(const auto&J:vs.joints){if(!J.swingLimited)continue;const V3 f=J.swingTwist(space)*57.2958f;const float over=std::max({f.x-J.swingUpper.x*57.2958f,J.swingLower.x*57.2958f-f.x,f.y-J.swingUpper.y*57.2958f,J.swingLower.y*57.2958f-f.y,f.z-J.twistUpper*57.2958f,J.twistLower*57.2958f-f.z,0.f});
   if(over>.5f){log<<"  joint "<<J.key<<" over "<<over<<" bend "<<f.x<<" side "<<f.y<<" twist "<<f.z<<" sleepingA "<<space.bodies[size_t(J.a)].sleeping<<" B "<<space.bodies[size_t(J.b)].sleeping<<" proj "<<J.positionCorrection<<" iters "<<space.settings.projectIterations;
    vectra::Joint copy=J;Space probe=space;copy.project(probe);const V3 g=copy.swingTwist(probe)*57.2958f;log<<" after "<<g.x<<' '<<g.y<<' '<<g.z<<'\n';}}log.flush();}
 measure();m_age+=Tick;m_dirty=true;m_motion=motion;
 // Asleep when Vectra's island is (every section slow for its sleep time), or past the time a body may take.
 bool asleep=sections>0;for(int k=0;k<sections;++k)asleep&=space.bodies[size_t(k)].sleeping;
 if(asleep||m_age-m_woke>7.f){for(int k=0;k<sections;++k){auto&b=space.bodies[size_t(k)];b.sleeping=true;b.v={};b.w={};b.sleepTime=0;}m_asleep=true;}
 ++Steps;StepMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
}
}

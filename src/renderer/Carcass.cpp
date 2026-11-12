#include "Carcass.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <numeric>
#include <queue>
#include <unordered_map>
namespace retro {
namespace {
constexpr float Tick=1.f/60,Skin=.02f;
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross3(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float span(Point3 a){return std::sqrt(dot3(a,a));}
Point3 unit(Point3 a,Point3 fallback={0,0,1}){float s=span(a);return s>1e-6f?a*(1/s):fallback;}
float chance(unsigned& s){s=s*1664525u+1013904223u;return float((s>>8)&0xffffu)/65535.f;}
float either(unsigned& s){return chance(s)*2-1;}
void axesOf(const float q[4],Point3 r[3]){float w=q[0],x=q[1],y=q[2],z=q[3];
 r[0]={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)};r[1]={2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)};r[2]={2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)};}
struct Groups {std::vector<int> parent;explicit Groups(int n):parent(size_t(n)){std::iota(parent.begin(),parent.end(),0);}
 int find(int i){while(parent[size_t(i)]!=i){parent[size_t(i)]=parent[size_t(parent[size_t(i)])];i=parent[size_t(i)];}return i;}
 bool join(int a,int b){a=find(a);b=find(b);if(a==b)return false;parent[size_t(std::max(a,b))]=std::min(a,b);return true;}};
}
int Carcass::joints(bool seamsOnly)const{int n=0;for(auto&j:m_joints)if(!seamsOnly||j.seam)++n;return n;}
Point3 Carcass::carry(const Section& s,Point3 rest)const{Point3 d=rest-s.restCentre;return s.centre+s.axes[0]*d.x+s.axes[1]*d.y+s.axes[2]*d.z;}

void Carcass::build(const std::vector<MeshTriangle>& posed,const World& world,Point3 push,float scale,unsigned seed,float reach){
 *this=Carcass{};if(posed.empty())return;
 m_triangles=posed;m_scale=scale;m_seed=seed*2654435761u|1u;m_reach=std::max(0.f,reach);
 const int T=int(posed.size());m_corner.resize(size_t(T)*3);
 // Corners that share a place are one vertex, whatever the UV seams say.
 {std::unordered_map<std::uint64_t,int> cells;cells.reserve(size_t(T)*2);
  auto cell=[](Point3 p){auto q=[](float v){return std::uint64_t(std::int64_t(std::floor(v*500.f+.5f)))&0x1fffffu;};return q(p.x)|q(p.y)<<21|q(p.z)<<42;};
  for(int t=0;t<T;++t)for(int k=0;k<3;++k){auto p=posed[size_t(t)].v[k].p;auto [slot,fresh]=cells.try_emplace(cell(p),int(m_rest.size()));if(fresh)m_rest.push_back(p);m_corner[size_t(t*3+k)]=slot->second;}}
 const int V=int(m_rest.size());
 Point3 lo=m_rest[0],hi=lo;for(auto p:m_rest){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
 m_size=std::max(.05f,span(hi-lo));const Point3 mid=(lo+hi)*.5f;
 std::vector<std::vector<int>> around(static_cast<size_t>(V));
 for(int t=0;t<T;++t)for(int k=0;k<3;++k){int a=m_corner[size_t(t*3+k)],b=m_corner[size_t(t*3+(k+1)%3)];if(a!=b){around[size_t(a)].push_back(b);around[size_t(b)].push_back(a);}}
 // The body is a cloud of points spread evenly over its skin.
 const int N=std::min(160,V);m_body=N;
 std::vector<int> seat;{std::vector<float> reach(size_t(V),1e30f);int pick=0;float far=-1;
  for(int v=0;v<V;++v){float d=span(m_rest[size_t(v)]-mid);if(d>far){far=d;pick=v;}}
  for(int i=0;i<N;++i){seat.push_back(pick);far=-1;int next=pick;const Point3 at=m_rest[size_t(pick)];
   for(int v=0;v<V;++v){float d=span(m_rest[size_t(v)]-at);if(d<reach[size_t(v)])reach[size_t(v)]=d;if(reach[size_t(v)]>far){far=reach[size_t(v)];next=v;}}pick=next;}}
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
 // Sections grow outward along the skin from seeds set as far apart as they will go: the ends of
 // limbs first, then the trunk between them.
 std::vector<int> region(size_t(N),-1);{std::vector<float> away(size_t(N),1e30f),d(size_t(N),0.f);std::vector<char> done(static_cast<size_t>(N));
  int wanted=std::min({16,std::clamp(N/14,3,12)+std::max(0,majors-1),std::max(1,N/4)});const float least=.085f*m_size;
  for(int s=0,made=0;s<wanted;++s){int from=-1;float far=-1;
   for(int i=0;i<N;++i)if(major(i)&&away[size_t(i)]>far){far=away[size_t(i)];from=i;}
   if(from<0||far<least)break;
   std::fill(d.begin(),d.end(),1e30f);std::fill(done.begin(),done.end(),char(0));d[size_t(from)]=0;
   for(;;){int best=-1;for(int i=0;i<N;++i)if(!done[size_t(i)]&&d[size_t(i)]<1e29f&&(best<0||d[size_t(i)]<d[size_t(best)]))best=i;if(best<0)break;done[size_t(best)]=1;
    for(int j=0;j<N;++j)if(link(best,j)){float nd=d[size_t(best)]+span(m_particles[size_t(j)].rest-m_particles[size_t(best)].rest);if(nd<d[size_t(j)])d[size_t(j)]=nd;}}
   for(int i=0;i<N;++i)if(d[size_t(i)]<away[size_t(i)]){away[size_t(i)]=d[size_t(i)];region[size_t(i)]=made;}
   ++made;}}
 auto nearestOther=[&](int i,int avoid){int found=-1;float best=1e30f;for(int j=0;j<N;++j){if(region[size_t(j)]<0||region[size_t(j)]==avoid)continue;float d=span(m_particles[size_t(j)].rest-m_particles[size_t(i)].rest);if(d<best){best=d;found=region[size_t(j)];}}return found;};
 for(int i=0;i<N;++i)if(region[size_t(i)]<0){int r=nearestOther(i,-1);region[size_t(i)]=r<0?0:r;}
 // A section too small to turn steadily joins the neighbour it touches most.
 for(int guard=0;guard<32;++guard){std::array<int,32> size{};for(int r:region)++size[size_t(r)];int live=0,small=-1;for(int r=0;r<32;++r){if(size[size_t(r)])++live;if(size[size_t(r)]&&size[size_t(r)]<4&&small<0)small=r;}
  if(small<0||live<2)break;std::array<int,32> touch{};for(int a=0;a<N;++a)if(region[size_t(a)]==small)for(int b=0;b<N;++b)if(link(a,b)&&region[size_t(b)]!=small)++touch[size_t(region[size_t(b)])];
  int into=int(std::max_element(touch.begin(),touch.end())-touch.begin());if(!touch[size_t(into)]){for(int a=0;a<N;++a)if(region[size_t(a)]==small){into=nearestOther(a,small);break;}}
  if(into<0||into==small)break;for(int&r:region)if(r==small)r=into;}
 int R=0;{std::array<int,32> label;label.fill(-1);for(int&r:region){if(label[size_t(r)]<0)label[size_t(r)]=R++;r=label[size_t(r)];}}
 for(int i=0;i<N;++i)m_particles[size_t(i)].region=region[size_t(i)];
 // Skin: each vertex rides its own point and the nearest of that point's neighbours.
 m_binds.resize(size_t(V));const float soft=.02f*m_size;
 for(int v=0;v<V;++v){auto&b=m_binds[size_t(v)];const int first=near[size_t(v)];b.home=region[size_t(first)];float reach[3]{1e30f,1e30f,1e30f};
  auto offer=[&](int i){float d=span(m_particles[size_t(i)].rest-m_rest[size_t(v)]);for(int k=0;k<3;++k)if(d<reach[k]){for(int m=2;m>k;--m){reach[m]=reach[m-1];b.particle[m]=b.particle[m-1];}reach[k]=d;b.particle[k]=i;break;}};
  offer(first);for(int j=0;j<N;++j)if(link(first,j))offer(j);
  float total=0;for(int k=0;k<3;++k)if(b.particle[k]>=0){b.weight[k]=1.f/(reach[k]*reach[k]+soft*soft);total+=b.weight[k];}for(float&w:b.weight)w/=total;}
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
 m_sections.resize(size_t(R));
 for(int i=0;i<N;++i){auto&s=m_sections[size_t(region[size_t(i)])];s.members.push_back(i);s.weights.push_back(1.f);++s.count;}
 for(auto&j:m_joints){j.particle=int(m_particles.size());Particle p;p.rest=p.x=p.previous=j.rest;p.region=-1;m_particles.push_back(p);
  for(int s:{j.a,j.b}){auto&section=m_sections[size_t(s)];section.members.push_back(j.particle);section.weights.push_back(std::max(2.f,.35f*float(section.count)));}}
 Point3 middle{};for(int i=0;i<N;++i)middle=middle+m_particles[size_t(i)].rest;middle=middle*(1.f/float(N));
 // The kill tips it: the higher on the body, the harder.
 float low=1e30f,high=-1e30f;for(int i=0;i<N;++i){low=std::min(low,m_particles[size_t(i)].rest.z);high=std::max(high,m_particles[size_t(i)].rest.z);}
 Point3 shove=push;if(span(push)<.05f){float a=chance(m_seed)*kTwoPi;shove={std::cos(a)*.35f,std::sin(a)*.35f,0};}
 std::vector<Point3> drift(static_cast<size_t>(R));for(auto&d:drift)d={either(m_seed)*.3f,either(m_seed)*.3f,either(m_seed)*.12f};
 for(auto&p:m_particles){Point3 v=shove*(.35f+.9f*(p.rest.z-low)/std::max(.05f,high-low));if(p.region>=0)v=v+drift[size_t(p.region)];p.previous=p.x-v*Tick;
  p.floor=world.supportBelow(p.x.x,p.x.y,std::max(p.x.z,middle.z)+.05f);p.roof=world.clearanceAbove(p.x.x,p.x.y,std::max(p.floor,p.x.z-.05f));p.probe=p.x;p.safe=p.x;}
 fit();
 float best=-1;for(int s=0;s<R;++s){float score=float(m_sections[size_t(s)].count)/(1.f+6.f*span(m_sections[size_t(s)].restCentre-middle)/m_size);if(score>best){best=score;m_root=s;}}
 regroup();measure();m_dirty=true;
}

void Carcass::fit(){
 for(auto&s:m_sections){if(s.members.empty())continue;Point3 c{},rc{};float total=0;
  for(size_t k=0;k<s.members.size();++k){const auto&p=m_particles[size_t(s.members[k])];float w=s.weights[k];c=c+p.x*w;rc=rc+p.rest*w;total+=w;}
  c=c*(1/total);rc=rc*(1/total);Point3 a[3]{};
  for(size_t k=0;k<s.members.size();++k){const auto&p=m_particles[size_t(s.members[k])];Point3 d=(p.x-c)*s.weights[k],r=p.rest-rc;a[0]=a[0]+d*r.x;a[1]=a[1]+d*r.y;a[2]=a[2]+d*r.z;}
  // The rotation nearest the section's deformation, carried over from the last step.
  for(int i=0;i<6;++i){axesOf(s.q,s.axes);
   Point3 omega=(cross3(s.axes[0],a[0])+cross3(s.axes[1],a[1])+cross3(s.axes[2],a[2]))*(1.f/(std::fabs(dot3(s.axes[0],a[0])+dot3(s.axes[1],a[1])+dot3(s.axes[2],a[2]))+1e-9f));
   float angle=span(omega);if(!(angle>1e-6f))break;Point3 axis=omega*(1/angle);angle=std::min(angle,1.2f);float h=std::sin(angle*.5f),w=std::cos(angle*.5f);
   float d[4]{w,axis.x*h,axis.y*h,axis.z*h},q[4]{s.q[0],s.q[1],s.q[2],s.q[3]};
   s.q[0]=d[0]*q[0]-d[1]*q[1]-d[2]*q[2]-d[3]*q[3];s.q[1]=d[0]*q[1]+d[1]*q[0]+d[2]*q[3]-d[3]*q[2];s.q[2]=d[0]*q[2]-d[1]*q[3]+d[2]*q[0]+d[3]*q[1];s.q[3]=d[0]*q[3]+d[1]*q[2]-d[2]*q[1]+d[3]*q[0];
   float size=std::sqrt(s.q[0]*s.q[0]+s.q[1]*s.q[1]+s.q[2]*s.q[2]+s.q[3]*s.q[3]);if(!(size>1e-6f)||!std::isfinite(size)){s.q[0]=1;s.q[1]=s.q[2]=s.q[3]=0;break;}for(float&v:s.q)v/=size;}
  axesOf(s.q,s.axes);s.centre=c;s.restCentre=rc;}
}

void Carcass::step(const World& world){
 const size_t count=m_particles.size();
 // A point is clear where it is free, and, with a reach, free that far round it.
 auto clear=[&](Point3 at){if(!world.fits(at.x,at.y,at.z+.01f,.04f,false))return false;if(m_reach<=0)return true;
  for(auto [dx,dy]:{std::pair{m_reach,0.f},{-m_reach,0.f},{0.f,m_reach},{0.f,-m_reach}})if(!world.fits(at.x+dx,at.y+dy,at.z+.01f,.04f,false))return false;return true;};
 for(auto&p:m_particles){if(p.region==-2)continue;Point3 v=(p.x-p.previous)*.992f;float speed=span(v);if(speed>.3f)v=v*(.3f/speed);p.previous=p.x;p.x=p.x+v;p.x.z-=14.f*Tick*Tick;p.grounded=false;}
 static thread_local std::vector<Point3> goal;static thread_local std::vector<float> share;
 for(int pass=0;pass<3;++pass){
  // Every section is pulled back to its own shape; the joints they share hold them to one another.
  fit();goal.assign(count,Point3{});share.assign(count,0.f);
  for(auto&s:m_sections)for(size_t k=0;k<s.members.size();++k){size_t i=size_t(s.members[k]);goal[i]=goal[i]+carry(s,m_particles[i].rest)*s.weights[k];share[i]+=s.weights[k];}
  for(size_t i=0;i<count;++i)if(share[i]>0){auto&p=m_particles[i];p.x=p.x+(goal[i]*(1/share[i])-p.x)*.92f;}
  for(auto&p:m_particles){if(p.region==-2)continue;
   if(p.x.z>p.roof-Skin&&p.roof>p.floor+.1f){p.x.z=p.roof-Skin;p.previous.z=std::max(p.previous.z,p.x.z);}
   if(p.x.z<p.floor+Skin){p.x.z=p.floor+Skin;p.grounded=true;}
   float dx=p.x.x-p.safe.x,dy=p.x.y-p.safe.y;
   if(dx*dx+dy*dy>.03f*.03f){
    // A point that began inside a wall is let out before walls start holding it.
    if(clear(p.x)){p.safe=p.x;p.loose=true;}
    else if(p.loose){p.x.x=p.safe.x;p.x.y=p.safe.y;p.previous.x=p.x.x;p.previous.y=p.x.y;}
   }
   dx=p.x.x-p.probe.x;dy=p.x.y-p.probe.y;
   if(dx*dx+dy*dy>.04f*.04f||p.x.z>p.probe.z+.25f){p.floor=world.supportBelow(p.x.x,p.x.y,std::max(p.x.z,p.previous.z)+.08f);p.roof=world.clearanceAbove(p.x.x,p.x.y,std::max(p.floor,std::min(p.x.z,p.previous.z)-.05f));p.probe=p.x;if(p.x.z<p.floor+Skin){p.x.z=p.floor+Skin;p.grounded=true;}}
  }
 }
 float motion=0;
 for(auto&p:m_particles){if(p.region==-2)continue;if(p.grounded){p.previous.x=p.x.x-(p.x.x-p.previous.x)*.7f;p.previous.y=p.x.y-(p.x.y-p.previous.y)*.7f;p.previous.z=p.x.z;}motion=std::max(motion,span(p.x-p.previous));}
 fit();measure();m_age+=Tick;m_dirty=true;m_motion=motion;
 m_quiet=motion<.0024f?m_quiet+Tick:0;if(m_quiet>.5f||m_age-m_woke>7.f)m_asleep=true;
}

void Carcass::advance(const World& world,float toAge){
 if(!built())return;if(m_asleep){m_age=std::max(m_age,toAge);return;}
 int budget=300;while(m_age+Tick*.5f<toAge&&budget-->0&&!m_asleep)step(world);
 if(m_age<toAge&&(m_asleep||budget<=0))m_age=toAge;
}

void Carcass::measure(){Point3 sum{};int n=0;for(auto&p:m_particles)if(p.region!=-2){sum=sum+p.x;++n;}m_centre=sum*(1.f/float(std::max(1,n)));m_radius=.1f;for(auto&p:m_particles)if(p.region!=-2)m_radius=std::max(m_radius,span(p.x-m_centre));m_radius+=.08f*m_size;}

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

Point3 Carcass::place(int vertex,int viewer)const{
 const auto&b=m_binds[size_t(vertex)];Point3 sum{};float total=0,floor=-100;const Point3 rest=m_rest[size_t(vertex)];
 for(int k=0;k<3;++k){int i=b.particle[k];if(i<0)continue;const auto&p=m_particles[size_t(i)];if(m_piece[size_t(p.region)]!=m_piece[size_t(viewer)])continue;
  const auto&s=m_sections[size_t(p.region)];Point3 d=rest-p.rest;if(total==0)floor=p.floor;sum=sum+(p.x+s.axes[0]*d.x+s.axes[1]*d.y+s.axes[2]*d.z)*b.weight[k];total+=b.weight[k];}
 if(total<=1e-5f){const auto&s=m_sections[size_t(viewer)];Point3 at=carry(s,rest);if(!s.members.empty())at.z=std::max(at.z,m_particles[size_t(s.members[0])].floor+.012f);return at;}
 Point3 at=sum*(1/total);at.z=std::max(at.z,floor+.012f);return at;
}

void Carcass::skin(){
 m_dirty=false;const int V=int(m_binds.size()),T=int(m_owner.size());m_posed.resize(size_t(V));
 for(int v=0;v<V;++v)m_posed[size_t(v)]=place(v,m_binds[size_t(v)].home);
 for(int t=0;t<T;++t)for(int k=0;k<3;++k){int v=m_corner[size_t(t*3+k)],home=m_binds[size_t(v)].home,owner=m_owner[size_t(t)];
  m_triangles[size_t(t)].v[k].p=m_pieces==1||m_piece[size_t(home)]==m_piece[size_t(owner)]?m_posed[size_t(v)]:place(v,owner);}
 m_caps.clear();
 for(const auto&loop:m_loops){const auto&s=m_sections[size_t(loop.owner)];MeshTriangle cap;cap.part=0;Point3 hub=carry(s,loop.centre+loop.out*(.012f*m_size));if(!s.members.empty())hub.z=std::max(hub.z,m_particles[size_t(s.members[0])].floor+.012f);cap.v[2]={hub,.5f,.5f,1.f};
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
 fit();regroup();measure();m_asleep=false;m_quiet=0;m_woke=m_age;m_dirty=true;
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
 m_asleep=false;m_quiet=0;m_woke=m_age;m_dirty=true;
 out.push_back({point,ray.direction,std::clamp(weight,.5f,2.f),false});
 s.damage+=ray.damage;auto torn=limb(section,point);
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
 fit();regroup();measure();m_asleep=false;m_quiet=0;m_woke=m_age;m_dirty=true;
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
}

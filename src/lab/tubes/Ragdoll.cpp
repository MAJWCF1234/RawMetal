// Ragdoll falls (Carcass, humanCorpse): men and creatures killed in set ways, each body measured every
// frame against what dead weight does. A man dropped where he stands, shot from the front, from behind,
// from the side; one shot onto a table, down a stair, back against a wall; a headless man; a Brute and
// a Chimera. Every frame: no joint past its limit, no section in another, every section the shape it
// was (its rigid core within 1.5 cm after the best rigid fit), the volume kept, and at rest nothing in
// the floor or a wall. Once down (nothing of it 0.6 m above where his feet were) its middle moves less
// than 0.3 m: it does not slide. Strips from the front and the side at 30 fps for judging by eye.
//
// Fixture keys: scenario (a label), body N (0-5 civilians, 6.. the cleanup crew, 12 the Investigator),
// creature KIND, kill-frame, from DEG (where the shot comes from, off the way he faces: 0 in front,
// 180 behind, 90 his left), push 0 (no push: dropped where he stands), headless 1,
// box[1-6] X1 Y1 X2 Y2 BOTTOM TOP (furniture, steps, walls), topple 1 (a standing fall must go over).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include "renderer/Humanoid.h"
#include "game/EngineRegressionScene.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <unordered_map>
namespace retro::lab {
namespace {
constexpr std::uint32_t PartColours[]={0xffffff,0xc0c0c0,0x808080,0xffe040,0xff4040,0x4040ff,0xff9090,0x9090ff,0x40c040,0xc040c0,0xa0ffa0,0xffa0ff};
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross3(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float len3(Point3 a){return std::sqrt(dot3(a,a));}

// ------------------------------------------------------------------ rig picture
bool rigLabels(){
 auto storage=std::make_unique<SoftwareRenderer>(160,90);auto& r=*storage;std::vector<Image> tiles;std::ofstream out("rig-labels.txt");
 for(int m=0;m<LabAccess::humanMeshes();++m){Mesh& mesh=LabAccess::humanMesh(r,m);const auto& bind=mesh.bindTriangles();const auto& labels=LabAccess::humanParts(r,m);
  Image tile(400,220,0x202020);float y0=1e9f,y1=-1e9f,x0=1e9f,x1=-1e9f,z0=1e9f,z1=-1e9f;
  for(auto& t:bind)for(auto& v:t.v){y0=std::min(y0,v.p.y);y1=std::max(y1,v.p.y);x0=std::min(x0,v.p.x);x1=std::max(x1,v.p.x);z0=std::min(z0,v.p.z);z1=std::max(z1,v.p.z);}
  const float s=200.f/std::max({y1-y0,x1-x0,z1-z0});int counts[humanoid::Count]{};
  for(size_t t=0;t<bind.size();++t){Point3 c=(bind[t].v[0].p+bind[t].v[1].p+bind[t].v[2].p)*(1.f/3);const int p=labels.part[t];++counts[p];const auto col=PartColours[p];
   const float cx=c.x-(x0+x1)*.5f,cz=c.z-(z0+z1)*.5f,f=cx*labels.forward.x+cz*labels.forward.z,l=cx*labels.forward.z-cz*labels.forward.x;
   tile.put(int(100+l*s),int(210-(c.y-y0)*s),col);tile.put(int(300+f*s),int(210-(c.y-y0)*s),col);}
  drawText(tile,4,4,"MESH "+std::to_string(m)+(labels.ok?" OK":" NO ARMS"),0xffff60);tiles.push_back(tile);
  out<<"mesh "<<m<<" ok "<<labels.ok<<" forward "<<labels.forward.x<<','<<labels.forward.z<<" hip "<<labels.hip<<" knee "<<labels.knee<<" :";for(int p=0;p<humanoid::Count;++p)out<<' '<<humanoid::name(p)<<'='<<counts[p];out<<'\n';}
 std::vector<std::pair<std::string,const Image*>> sheet;for(size_t i=0;i<tiles.size();++i)sheet.push_back({"mesh "+std::to_string(i),&tiles[i]});
 savePng("rig-labels.png",contactSheet(sheet,4,400));return true;}
const bool labelsTube=add({.name="ragdoll-rig",.summary="the human rigs as Humanoid cuts them (rig-labels.png)",.standalone=rigLabels});

// ------------------------------------------------------------------ measuring a body
// The rotation that best carries `a` onto `b` (Horn, Jacobi), as axes.
void bestFit(const std::vector<Point3>& a,const std::vector<Point3>& b,Point3& ca,Point3& cb,Point3 axes[3]){
 const size_t n=a.size();ca={};cb={};for(size_t i=0;i<n;++i){ca=ca+a[i];cb=cb+b[i];}ca=ca*(1.f/float(n));cb=cb*(1.f/float(n));
 double S[3][3]{};for(size_t i=0;i<n;++i){Point3 p=a[i]-ca,q=b[i]-cb;const float A[3]{p.x,p.y,p.z},B[3]{q.x,q.y,q.z};for(int r=0;r<3;++r)for(int c=0;c<3;++c)S[r][c]+=double(A[r])*B[c];}
 double N[4][4]{{S[0][0]+S[1][1]+S[2][2],S[1][2]-S[2][1],S[2][0]-S[0][2],S[0][1]-S[1][0]},{S[1][2]-S[2][1],S[0][0]-S[1][1]-S[2][2],S[0][1]+S[1][0],S[2][0]+S[0][2]},
  {S[2][0]-S[0][2],S[0][1]+S[1][0],-S[0][0]+S[1][1]-S[2][2],S[1][2]+S[2][1]},{S[0][1]-S[1][0],S[2][0]+S[0][2],S[1][2]+S[2][1],-S[0][0]-S[1][1]+S[2][2]}};
 double E[4][4]{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
 for(int sweep=0;sweep<24;++sweep){double off=0;for(int p=0;p<4;++p)for(int r=p+1;r<4;++r)off+=N[p][r]*N[p][r];if(off<1e-18)break;
  for(int p=0;p<4;++p)for(int r=p+1;r<4;++r){if(std::fabs(N[p][r])<1e-30)continue;double th=(N[r][r]-N[p][p])/(2*N[p][r]),t=(th>=0?1:-1)/(std::fabs(th)+std::sqrt(th*th+1)),c=1/std::sqrt(t*t+1),s=t*c;
   for(int k=0;k<4;++k){double x=N[k][p],y=N[k][r];N[k][p]=c*x-s*y;N[k][r]=s*x+c*y;}for(int k=0;k<4;++k){double x=N[p][k],y=N[r][k];N[p][k]=c*x-s*y;N[r][k]=s*x+c*y;}
   for(int k=0;k<4;++k){double x=E[k][p],y=E[k][r];E[k][p]=c*x-s*y;E[k][r]=s*x+c*y;}}}
 int top=0;for(int k=1;k<4;++k)if(N[k][k]>N[top][top])top=k;float w=float(E[0][top]),x=float(E[1][top]),y=float(E[2][top]),z=float(E[3][top]);const float l=std::sqrt(w*w+x*x+y*y+z*z);w/=l;x/=l;y/=l;z/=l;
 axes[0]={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)};axes[1]={2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)};axes[2]={2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)};}
// A body under watch: the shape each corner had when it died, the group (part or section) of each
// triangle, how far each corner is from the seam to another group.
struct Watch {bool ready=false,human=false;int body=0;std::vector<Point3> rest;std::vector<int> group;std::vector<float> seam;int groups=0;double volume=0;Point3 feet{};float facing=0;
 // Per frame, the worst seen.
 float shapeCore=0,shapeAll=0,volumeDev=0,limitDeg=0,overlap=0,lowestMoving=1e9f;int insideMoving=0;float settled=-1,fell=-1;
 // Where its middle was once it had fallen (down within 0.6 m of the floor), and the farthest it went from there: the slide.
 Point3 down{};float slide=0;};
// Each group's own enclosed volume about its own middle, summed: a rigid turn of a part leaves it as it is, a squash does not.
double volumeOf(const std::vector<MeshTriangle>& t,const std::vector<int>& group){std::map<int,std::pair<Point3,int>> mid;for(size_t i=0;i<t.size();++i){auto& m=mid[group[i]];m.first=m.first+t[i].v[0].p;++m.second;}
 std::map<int,double> vol;for(size_t i=0;i<t.size();++i){const auto& m=mid[group[i]];const Point3 c=m.first*(1.f/float(m.second));vol[group[i]]+=dot3(t[i].v[0].p-c,cross3(t[i].v[1].p-c,t[i].v[2].p-c))/6.0;}double v=0;for(auto& [g,x]:vol)v+=std::fabs(x);return v;}
double volumeOld(const std::vector<MeshTriangle>& t){Point3 c{};for(auto& f:t)c=c+f.v[0].p;c=c*(1.f/float(std::max<size_t>(1,t.size())));double v=0;for(auto& f:t)v+=dot3(f.v[0].p-c,cross3(f.v[1].p-c,f.v[2].p-c))/6.0;return std::fabs(v);}
void prepare(Watch& w,const std::vector<Point3>& rest,const std::vector<int>& group){
 w.rest=rest;w.group=group;w.groups=0;for(int g:group)w.groups=std::max(w.groups,g+1);
 // Seam corners: a place shared by triangles of two groups.
 std::unordered_map<std::uint64_t,int> first;std::vector<Point3> seams;auto key=[](Point3 p){auto q=[](float v){return std::uint64_t(std::int64_t(std::floor(v*500.f+.5f)))&0x1fffffu;};return q(p.x)|q(p.y)<<21|q(p.z)<<42;};
 std::unordered_map<std::uint64_t,char> mixed;for(size_t c=0;c<rest.size();++c){auto [it,fresh]=first.try_emplace(key(rest[c]),group[c/3]);if(!fresh&&it->second!=group[c/3])mixed[it->first]=1;}
 for(size_t c=0;c<rest.size();++c)if(mixed.count(key(rest[c])))seams.push_back(rest[c]);
 w.seam.assign(rest.size(),1e9f);for(size_t c=0;c<rest.size();++c)for(auto s:seams)w.seam[c]=std::min(w.seam[c],len3(rest[c]-s));
 std::vector<MeshTriangle> t(rest.size()/3);for(size_t i=0;i<t.size();++i)for(int k=0;k<3;++k)t[i].v[k].p=rest[i*3+k];w.volume=volumeOf(t,w.group);w.ready=true;}
// The worst corner of each group's rigid core (over 6 cm from a seam) after the best rigid fit, and of every corner.
void shapeError(const Watch& w,const std::vector<MeshTriangle>& now,float& core,float& all,std::vector<float>* each=nullptr){
 core=0;all=0;if(each)each->assign(size_t(w.groups),0.f);std::vector<std::vector<int>> by(size_t(w.groups));for(size_t c=0;c<w.rest.size();++c)by[size_t(w.group[c/3])].push_back(int(c));
 for(auto& list:by){if(list.size()<9)continue;std::vector<Point3> a,b;for(int c:list){if(w.seam[size_t(c)]<.06f)continue;a.push_back(w.rest[size_t(c)]);b.push_back(now[size_t(c/3)].v[c%3].p);}
  if(a.size()<6){a.clear();b.clear();for(int c:list){a.push_back(w.rest[size_t(c)]);b.push_back(now[size_t(c/3)].v[c%3].p);}}
  Point3 ca,cb,R[3];bestFit(a,b,ca,cb,R);
  for(int c:list){Point3 d=w.rest[size_t(c)]-ca;Point3 at=cb+R[0]*d.x+R[1]*d.y+R[2]*d.z;const float e=len3(at-now[size_t(c/3)].v[c%3].p);all=std::max(all,e);if(w.seam[size_t(c)]>=.06f){core=std::max(core,e);if(each)(*each)[size_t(w.group[size_t(c/3)])]=std::max((*each)[size_t(w.group[size_t(c/3)])],e);}}}}
// Corners more than 1.5 cm inside the floor or anything solid, and the lowest gap over the floor under the body.
int insideCount(const std::vector<MeshTriangle>& t,const World& world,float& lowest,Point3* where=nullptr){
 int n=0;lowest=1e9f;for(auto& f:t)for(auto& v:f.v){const Point3 p=v.p;if(!world.fits(p.x,p.y,p.z+.015f,.001f,false)){++n;if(where)*where=p;}
  float floor=world.supportBelow(p.x,p.y,p.z+.06f);for(auto [dx,dy]:{std::pair{.05f,0.f},{-.05f,0.f},{0.f,.05f},{0.f,-.05f}})floor=std::min(floor,world.supportBelow(p.x+dx,p.y+dy,p.z+.06f));lowest=std::min(lowest,p.z-floor);}
 return n;}

// ------------------------------------------------------------------ strips
struct Strip {std::vector<Image> front,side;};
std::map<const Scene*,Strip>& strips(){static std::map<const Scene*,Strip> s;return s;}
Image grid(const std::vector<Image>& tiles,int columns,const std::string& title){
 if(tiles.empty())return Image(8,8);const int w=tiles[0].width,h=tiles[0].height,rows=int((tiles.size()+size_t(columns)-1)/size_t(columns));Image out(w*columns,h*rows+14,0x101010);
 for(size_t i=0;i<tiles.size();++i){const int x0=int(i%size_t(columns))*w,y0=14+int(i/size_t(columns))*h;for(int y=0;y<h;++y)for(int x=0;x<w;++x)out.at(x0+x,y0+y)=tiles[i].pixels[size_t(y)*w+x];
  drawText(out,x0+2,y0+2,fixed(double(i)/30.0,2)+"S",0xffff60);}
 drawText(out,2,2,title,0xffffff);return out;}

// ------------------------------------------------------------------ the tube
Vec2 facingOf(float yaw){return {std::sin(yaw),std::cos(yaw)};} // as the game lays a man's pose (Hunter.cpp)
std::map<const Scene*,Watch>& watches(){static std::map<const Scene*,Watch> w;return w;}
int killFrame(const Scene& s){return int(s.fixture->number("kill-frame",12));}
Carcass* bodyOf(Scene& s,Watch* w=nullptr){
 Game& g=s.g();
 if(s.fixture->value("creature").empty()){if(g.npcActors().empty()||!g.npcDeath(0).dead)return nullptr;auto* c=LabAccess::humanCorpse(*s.renderer,g,0);return c?&c->carcass:nullptr;}
 (void)w;for(size_t i=0;i<LabAccess::enemies(g).size();++i)if(!LabAccess::enemies(g)[i].alive)return LabAccess::remains(*s.renderer,g.level(),i);return nullptr;}
const bool fall=add({
 .name="ragdoll-fall",.summary="a man (or a creature) killed in a set way falls, lies and holds his shape: limits, no self-overlap, rigid sections, nothing in the floor or a wall (Carcass, humanCorpse)",
 .friends={"blood"},.fixtures={"ragdoll-drop","ragdoll-shot-front","ragdoll-shot-behind","ragdoll-shot-side","ragdoll-table","ragdoll-stairs","ragdoll-wall","ragdoll-headless","ragdoll-brute","ragdoll-chimera"},
 .author=[](Scene& s,AuthoredMapData& m){
  const auto& fx=*s.fixture;
  for(const char* k:{"box","box2","box3","box4","box5","box6"}){const auto v=fx.value(k);if(v.empty())continue;float a[6]{};if(std::sscanf(v.c_str(),"%f %f %f %f %f %f",a,a+1,a+2,a+3,a+4,a+5)==6)m.structures.push_back({a[0],a[1],a[2],a[3],a[4],a[5]});}
  if(!fx.value("creature").empty())return;
  if(m.actorTracks.empty()){auto man=regression::guard({12,12},{12,12},AiFaction::Raider);m.actorTracks.push_back(man);}
  const int body=int(fx.number("body",6));auto& t=m.actorTracks[0];
  t.visual=body<6?(body==0?ActorVisual::Worker:ActorVisual(int(ActorVisual::Scientist)+body-1)):body>=12?ActorVisual::Investigator:ActorVisual(int(ActorVisual::Cleanup)+body-6);
  t.scale=1.75f;t.ai.armed=false;for(auto& k:t.keys)k.z=fx.number("man-z",0);},
 .setup=[](Scene& s){LabAccess::setGod(s.g(),true);watches().erase(&s);strips().erase(&s);
  if(!s.fixture->value("creature").empty()){const auto at=s.fixture->value("creature-at","12 12");float x=12,y=12;std::sscanf(at.c_str(),"%f %f",&x,&y);
   static const char* names[]={"huntsman","wasp","brute","warden","mutant","snatcher","chimera","transformed","machine"};int kind=0;
   for(int i=0;i<9;++i)if(s.fixture->value("creature")==names[i])kind=i;
   CreatureSpawn c{CreatureKind(kind),{x,y},0};LabAccess::spawnCreature(s.g(),c);}},
 .step=[](Scene& s,float){
  if(s.frame!=killFrame(s))return;Game& g=s.g();const float from=s.fixture->number("from",0)*0.01745329f;const bool push=s.fixture->number("push",1)!=0;
  if(s.fixture->value("creature").empty()){if(g.npcActors().empty())return;const auto& n=g.npcActors()[0];const Vec2 f=facingOf(n.pose.yaw);
   // The shot comes from `from` off the way he faces; dropped (push 0): from straight above, no push.
   const Vec2 dir{f.x*std::cos(from)-f.y*std::sin(from),f.x*std::sin(from)+f.y*std::cos(from)};
   if(s.fixture->number("headless",0)!=0)LabAccess::behead(g,0);
   LabAccess::killNpc(g,0,push?n.pose.position+dir*3.f:n.pose.position);}
  else{auto& list=LabAccess::enemies(g);if(list.empty())return;auto& e=list[0];const Vec2 f{std::cos(e.heading),std::sin(e.heading)};const Vec2 dir{f.x*std::cos(from)-f.y*std::sin(from),f.x*std::sin(from)+f.y*std::cos(from)};
   LabAccess::damageEnemy(g,e,1e4f);e.deathPush=push?Point3{-dir.x*1.6f,-dir.y*1.6f,.3f}:Point3{};}},
 .capture=[](Scene& s,const Frame&){
  Game& g=s.g();auto& w=watches()[&s];Carcass* c=bodyOf(s);if(!c)return;const World& world=g.chunkWorld(g.level());
  if(!w.ready){
   const bool human=s.fixture->value("creature").empty();w.human=human;std::vector<Point3> rest;std::vector<int> group;
   if(human){const auto& n=g.npcActors()[0];const auto& track=n.track();auto* corpse=LabAccess::humanCorpse(*s.renderer,g,0);
    const int body=cleanupActorVisual(track.visual)?6+cleanupActorBody(track.visual):civilianActorBody(track.visual);w.body=body;Mesh& mesh=LabAccess::humanMesh(*s.renderer,body);
    mesh.poseCreature(4,g.npcDeath(0).from);const float scale=track.scale/std::max(.01f,mesh.maximum.y-mesh.minimum.y);const auto& labels=LabAccess::humanParts(*s.renderer,body);
    // As humanCorpse lays him into the world (a turn and a mirror): a rigid fit then finds his parts.
    const float cy=std::cos(n.pose.yaw),sy=std::sin(n.pose.yaw);
    for(int f:corpse->faces){for(int k=0;k<3;++k){const Point3 p=mesh.triangles[size_t(f)].v[k].p;rest.push_back(Point3{p.x*cy+p.z*sy,-p.x*sy+p.z*cy,p.y}*scale);}group.push_back(labels.part[size_t(f)]);}
    w.feet={n.pose.position.x,n.pose.position.y,n.pose.z};w.facing=n.pose.yaw;
    #ifdef RAWMETAL_CARCASS_RIG
    for(size_t t=0;t<group.size();++t)group[t]=std::max(0,c->sectionOf(int(t)));
    #endif
   }
   else{const auto& e=LabAccess::enemies(g)[0];const auto mesh=LabAccess::creatureCorpseMesh(*s.renderer,e);for(auto& t:mesh)for(auto& v:t.v)rest.push_back(v.p);
    #ifdef RAWMETAL_CARCASS_RIG
    for(size_t t=0;t<mesh.size();++t)group.push_back(std::max(0,c->sectionOf(int(t))));
    #else
    group.assign(mesh.size(),0);
    #endif
    w.feet={e.pos.x,e.pos.y,e.z};w.facing=std::atan2(e.pos.x,e.pos.y);}
   if(rest.size()!=c->triangles().size()*3)return;prepare(w,rest,group);}
  const auto& tri=c->triangles();float core=0,all=0;shapeError(w,tri,core,all);w.shapeCore=std::max(w.shapeCore,core);w.shapeAll=std::max(w.shapeAll,all);
  if(std::getenv("RM_RAGDOLL_DEBUG")){static std::ofstream log("ragdoll-debug.txt");log<<"frame "<<s.frame<<" age "<<c->age()<<" core "<<core<<" all "<<all<<" asleep "<<c->asleep()<<" motion "<<c->motion()<<" groups:";
   {std::vector<float> each;float a,b;shapeError(w,tri,a,b,&each);for(size_t g=0;g<each.size();++g)log<<' '<<(w.human?humanoid::name(int(g)):"s")<<'='<<fixed(each[g]*100,1);log<<'\n';}
   #ifdef RAWMETAL_CARCASS_RIG
   {static std::map<const Scene*,Watch> bySection;auto& v=bySection[&s];if(!v.ready){std::vector<int> g;for(size_t t=0;t<tri.size();++t)g.push_back(std::max(0,c->sectionOf(int(t))));prepare(v,w.rest,g);}
    std::vector<float> each;float a,b;shapeError(v,tri,a,b,&each);log<<"  by section:";for(size_t g=0;g<each.size();++g)log<<' '<<humanoid::name(c->partOf(int(g)))<<'='<<fixed(each[g]*100,1);log<<'\n';}
   #endif
   #ifdef RAWMETAL_CARCASS_RIG
   for(const auto& j:c->jointStates())log<<"  "<<humanoid::name(j.part)<<" from "<<humanoid::name(c->partOf(j.parent))<<" bend "<<j.bend*57.3f<<" ["<<j.bendLo*57.3f<<","<<j.bendHi*57.3f<<"] side "<<j.side*57.3f<<" /"<<j.sideMax*57.3f<<" twist "<<j.twist*57.3f<<" /"<<j.twistMax*57.3f<<'\n';
   #endif
   log.flush();}
  w.volumeDev=std::max(w.volumeDev,float(std::fabs(volumeOf(tri,w.group)/w.volume-1)));
  #ifdef RAWMETAL_CARCASS_RIG
  for(const auto& j:c->jointStates()){if(j.cut)continue;const float over=std::max({j.bend-j.bendHi,j.bendLo-j.bend,std::fabs(j.side)-j.sideMax,std::fabs(j.twist)-j.twistMax,0.f});w.limitDeg=std::max(w.limitDeg,over*57.2958f);}
  w.overlap=std::max(w.overlap,c->overlap());
  #endif
  if(!c->asleep()&&(s.frame-killFrame(s))%2==0){float low=0;w.insideMoving=std::max(w.insideMoving,insideCount(tri,world,low));w.lowestMoving=std::min(w.lowestMoving,low);}
  if(w.settled<0&&c->asleep())w.settled=c->age();
  if(w.fell<0){float top=-1e9f;for(auto& f:tri)for(auto& v:f.v)top=std::max(top,v.p.z);if(top-w.feet.z<.6f){w.fell=c->age();w.down=c->centre();}}
  else w.slide=std::max(w.slide,std::hypot(c->centre().x-w.down.x,c->centre().y-w.down.y));
  // The strips: the fall from the front and the side, every other frame (30 fps) for 2.5 s.
  const int since=s.frame-killFrame(s);
  if(since>=0&&since<150&&since%2==0){Player& cam=LabAccess::player(g);const Player was=cam;const Vec2 f=facingOf(w.facing),side{f.y,-f.x};const Point3 aim{w.feet.x,w.feet.y,w.feet.z+.45f};
   auto view=[&](Vec2 from,std::vector<Image>& out){const Vec2 at=Vec2{aim.x,aim.y}+from*3.f;const float eyeZ=aim.z+.6f;
    LabAccess::setCamera(g,at,eyeZ-cam.eye,std::atan2(aim.y-at.y,aim.x-at.x),-std::atan2(.6f,3.f)*140.f);s.renderer->render(g);
    out.push_back(scaled(Image::of(s.renderer->pixels(),s.renderer->width(),s.renderer->height()),240,135));};
   auto& st=strips()[&s];view(f,st.front);view(side,st.side);cam=was;s.renderer->render(g);}
  if(s.last()){auto& st=strips()[&s];const std::string n=s.fixture->value("scenario",s.fixture->name);
   if(!st.front.empty()){s.shots.push_back({});Image a=grid(st.front,15,"FRONT "+n);s.shots.back()={"strip front",a.width,a.height,a.pixels};
    Image b=grid(st.side,15,"SIDE "+n);s.shots.push_back({"strip side",b.width,b.height,b.pixels});}}},
 .metrics=[](Scene& s,Report& r){
  Game& g=s.g();Carcass* c=bodyOf(s);auto& w=watches()[&s];r.check("it dies and is a body",c!=nullptr&&w.ready);if(!c||!w.ready)return;const World& world=g.chunkWorld(g.level());
  const auto& tri=c->triangles();float low=0;Point3 where{};const int inside=insideCount(tri,world,low,&where);
  float top=-1e9f;for(auto& f:tri)for(auto& v:f.v)top=std::max(top,v.p.z);
  const Point3 ctr=c->centre();const float drift=std::hypot(ctr.x-w.feet.x,ctr.y-w.feet.y);
  r.metric("settle_s",w.settled,"s");r.metric("fell_s",w.fell,"s");r.metric("shape_core_cm",w.shapeCore*100,"cm");r.metric("shape_all_cm",w.shapeAll*100,"cm");r.metric("volume_dev_pct",w.volumeDev*100,"%");
  r.metric("inside_moving",w.insideMoving);r.metric("inside_rest",inside);r.metric("lowest_rest_m",low,"m");r.metric("lowest_moving_m",w.lowestMoving,"m");r.metric("top_rest_m",top-w.feet.z,"m");r.metric("centre_from_feet_m",drift,"m");r.metric("slide_after_fall_m",w.slide,"m");
  r.check("once down it does not slide (centre within 0.3 m of where it fell)",w.fell<0||w.slide<.3f,fixed(w.slide,2)+" m");
  #ifdef RAWMETAL_CARCASS_RIG
  r.metric("limit_excess_deg",w.limitDeg,"deg");r.metric("overlap_cm",w.overlap*100,"cm");r.metric("sections",c->sections());
  r.check("every joint within its limits every frame (2 deg)",w.limitDeg<=2.f,fixed(w.limitDeg,1)+" deg past at worst");
  r.check("no section inside another by more than 2 cm",w.overlap<=.02f,fixed(w.overlap*100,1)+" cm at worst");
  #endif
  r.check("every section keeps its shape: rigid core within 1.5 cm every frame",w.shapeCore<=.015f,fixed(w.shapeCore*100,2)+" cm at worst");
  r.check("the volume stays within 3%",w.volumeDev<=.03f,fixed(w.volumeDev*100,1)+"% at worst");
  r.check("it comes to rest within 4 s",w.settled>0&&w.settled<=4.f,"asleep at "+fixed(w.settled,2)+" s");
  r.check("at rest nothing of it is in the floor or anything solid (1.5 cm)",inside==0,std::to_string(inside)+" corners in"+(inside?" e.g. at "+fixed(where.x,2)+","+fixed(where.y,2)+","+fixed(where.z,2):""));
  r.check("at rest it lies on what is under it (6 cm)",low<=.06f&&low>=-.015f,"lowest "+fixed(low)+" m");
  if(w.human){
   // Over, not down: his head ends far from his feet.
   float hx=0,hy=0;int hn=0;const auto& labels=LabAccess::humanParts(*s.renderer,w.body);auto* corpse=LabAccess::humanCorpse(*s.renderer,g,0);
   for(size_t i=0;i<corpse->faces.size();++i)if(labels.part[size_t(corpse->faces[i])]==humanoid::Head||labels.part[size_t(corpse->faces[i])]==humanoid::Chest){hx+=tri[i].v[0].p.x;hy+=tri[i].v[0].p.y;++hn;}
   const float reach=hn?std::hypot(hx/float(hn)-w.feet.x,hy/float(hn)-w.feet.y):0;r.metric("upper_body_from_feet_m",reach,"m");
   if(s.fixture->number("topple",0)!=0)r.check("he goes over from the hips, not down in a heap (chest 0.7 m or more from where his feet were)",reach>=.7f,fixed(reach,2)+" m");
   // The same death made again lies the same.
   const auto first=tri;LabAccess::corpses(*s.renderer).clear();auto* again=LabAccess::humanCorpse(*s.renderer,g,0);bool same=again&&again->carcass.triangles().size()==first.size();
   if(same){const auto& t=again->carcass.triangles();for(size_t i=0;i<t.size()&&same;++i)for(int k=0;k<3;++k)same&=t[i].v[k].p.x==first[i].v[k].p.x&&t[i].v[k].p.y==first[i].v[k].p.y&&t[i].v[k].p.z==first[i].v[k].p.z;}
   r.check("the same death made again lies exactly the same",same);}},
 .reporter=[](Scene& s,Overlay& o){
  Carcass* c=bodyOf(s);if(!c)return;
  #ifdef RAWMETAL_CARCASS_RIG
  for(const auto& j:c->jointStates()){if(j.cut)continue;const float over=std::max({j.bend-j.bendHi,j.bendLo-j.bend,std::fabs(j.side)-j.sideMax,std::fabs(j.twist)-j.twistMax,0.f});(void)over;}
  #endif
  o.cross(c->centre(),.12f,c->asleep()?0x40ff60:0xffa020);o.label(c->centre()+Point3{0,0,.3f},c->asleep()?"ASLEEP":"AWAKE",0xffffff);},
 .teardown=[](Scene& s){watches().erase(&s);strips().erase(&s);}});
}
}

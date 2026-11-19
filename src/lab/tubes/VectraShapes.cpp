// Capsules and convex hulls (Vectra.h PartKind::Capsule, PartKind::Hull), checked on the bare core and in the game.
// vectra-shapes.txt says what each did.
//  mass: a hull of a box's corners weighs and turns as the box; a capsule's volume and inertia against the sum of its
//    cylinder and its two half balls worked by hand.
//  rolling: a pipe on its side sent rolling over a static slab (body against body) and over the floor, as the 12-sided
//    prism the cylinder was and as a capsule: the variance of its vertical speed and of its speed about its own steady
//    slowing (a polygon bumps over each edge; a round does not).
//  resting: a rock's hull (PropHulls.h, cave_rock_chunk) dropped turned onto a slab and onto the floor lies on one of its
//    own faces, flat (that face's corners all on what is under it), its middle over them, asleep.
//  gibs: each of the glass bottle's four shards, dropped with its broken face (its largest) tipped 25 degrees off the
//    floor, ends flat on that face.
//  rays: rays at the rock from every side hit its true surface (on a face's plane, inside every other), never its box;
//    rays through its box's corners past it miss; rays at a capsule hit its round ends where the hand-worked ball says.
//  game: a traffic cone (a hull from its mesh) and a bottle (a capsule on a disc foot) knocked over in a room settle
//    on the floor, the cone back on its base when set down upright.
//  drums: a drum on its side rolled over a slab as its 12-sided prism and as the true round (Settings::roundCylinders),
//    and stood on its end there; a sealed drum turning afloat on its side, its water cells round its axis and square.
//  cost: 120 rock hulls dropped as a heap, ms a step.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include "game/physics/PropBodies.h"
#include "game/physics/Queries.h"
#include "game/physics/Vectra.h"
#include "world/CampaignKit.h"
#include "world/GibSets.h"
#include "world/ModelIds.h"
#include "world/PropHulls.h"
#include "core/Tune.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
namespace retro::lab {
namespace {
using namespace vectra;
std::string f3(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
using Check=std::function<void(bool,const std::string&)>;
// A space with a floor at z = 0 (each part's lowest feature against it), or none.
struct World {
 Space space;std::deque<Shape> shapes;
 explicit World(bool floor=true){if(floor)space.worldContacts=[](Space& sp,int i,float margin,std::vector<Manifold>& out){const Body& body=sp.bodies[size_t(i)];
   for(int part=0;part<body.shape->count;++part){std::array<V3,8> f{};int k=supportFeature(body,part,{0,0,-1},f);Manifold m;m.a=i;m.b=-1;m.partA=part;m.normal={0,0,-1};m.material=Material::mix(body.material,{.6f,.5f,0,.004f});
    // Four of them as the game's world keeps four (RigidClutter keepFour): the deepest (the first within a third of a
    // millimetre of it), the farthest from it, the widest triangle and quad.
    std::array<int,8> order{};int kept=0;
    if(k>4){float least=1e30f;for(int j=0;j<k;++j)least=std::min(least,f[size_t(j)].z);int pick[4]{-1,-1,-1,-1};for(int j=0;j<k;++j)if(f[size_t(j)].z<=least+.0003f){pick[0]=j;break;}
     float best=-1;for(int j=0;j<k;++j){const float d=lenSq(f[size_t(j)]-f[size_t(pick[0])]);if(d>best){best=d;pick[1]=j;}}
     auto area=[&](V3 a,V3 b,V3 c){return std::fabs(cross(b-a,c-a).z);};best=-1;for(int j=0;j<k;++j){if(j==pick[0]||j==pick[1])continue;const float a=area(f[size_t(pick[0])],f[size_t(pick[1])],f[size_t(j)]);if(a>best){best=a;pick[2]=j;}}
     if(pick[2]>=0){best=-1;const float tri=area(f[size_t(pick[0])],f[size_t(pick[1])],f[size_t(pick[2])]);for(int j=0;j<k;++j){if(j==pick[0]||j==pick[1]||j==pick[2])continue;const V3 q=f[size_t(j)];const float a=area(f[size_t(pick[0])],f[size_t(pick[1])],q)+area(f[size_t(pick[1])],f[size_t(pick[2])],q)+area(f[size_t(pick[2])],f[size_t(pick[0])],q)-tri;if(a>best){best=a;pick[3]=j;}}}
     for(int j:pick)if(j>=0)order[size_t(kept++)]=j;}
    else for(int j=0;j<k;++j)order[size_t(kept++)]=j;
    for(int o=0;o<kept;++o){const int j=order[size_t(o)];const float sep=f[size_t(j)].z;if(sep>margin)continue;auto& c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=f[size_t(j)];c.separation=sep;c.localA=body.q.unrotate(c.p-body.x);c.localB=c.p;c.feature=std::uint32_t(j);}
    if(m.count)out.push_back(m);}};}
 int add(const Shape& s,V3 at,float mass,Quat q={},Material mat={.45f,.38f,.32f,.001f}){shapes.push_back(s);Body b;b.shape=&shapes.back();b.q=q;b.x=at+q.rotate(b.shape->com);b.setMass(mass);b.updateInertia();b.material=mat;b.id=int(space.bodies.size());space.bodies.push_back(b);return int(space.bodies.size())-1;}
 // A static slab whose top is at z = 0.
 int slab(){Shape s;s.add(PartKind::Box,{},{20,20,.25f});s.finish();const int i=add(s,{0,0,-.25f},0);space.bodies[size_t(i)].material={.6f,.5f,0,.004f};return i;}
};
Shape hullShape(int model,std::array<float,3> d){const auto* e=hulls::find(model);Shape s;if(!e)return s;std::vector<V3> pts;for(int k=0;k<e->count;++k)pts.push_back({e->p[k][0]*d[0],e->p[k][1]*d[1],e->p[k][2]*d[2]});
 auto h=std::make_shared<ConvexHull>();V3 shift{};if(!ConvexHull::build(pts,*h,&shift))return s;s.addHull(std::move(h),shift);s.finish();return s;}
// How a hull body lies: the face most nearly facing down, its tilt (rad), the gap of its corners over the floor at
// `floor` (the most and the least), and whether its middle is over that face.
struct Lie {int face=-1;float tilt=9,hi=9,lo=-9;bool over=false;};
// (face >= 0: how that face lies.)
Lie lieOf(const Body& b,float floor=0,int face=-1){Lie r;const Part& p=b.shape->parts[0];const ConvexHull& h=*p.hull;const Quat t=b.q*p.turn;const V3 c=b.x+b.q.rotate(p.centre-b.shape->com);
 float most=-2;for(int f=0;f<h.nf;++f){const float d=-t.rotate(h.f[f].normal).z;if(face>=0?f==face:d>most){most=d;r.face=f;}}
 r.tilt=std::acos(std::clamp(most,-1.f,1.f));const auto& F=h.f[r.face];r.hi=-1e9f;r.lo=1e9f;
 for(int k=0;k<F.n;++k){const float z=(c+t.rotate(h.v[F.idx[k]])).z-floor;r.hi=std::max(r.hi,z);r.lo=std::min(r.lo,z);}
 // The centre of mass over the face (its corners' 2D hull, in the floor's plane: every side has it inside).
 r.over=true;for(int k=0;k<F.n;++k){const V3 a=c+t.rotate(h.v[F.idx[k]]),e=c+t.rotate(h.v[F.idx[(k+1)%F.n]]);const V3 n=t.rotate(h.f[r.face].normal);
  if(dot(cross(e-a,n),b.x-a)>.002f)r.over=false;}
 return r;}
float faceArea(const ConvexHull& h,int f){const auto& F=h.f[f];V3 sum{};for(int k=1;k+1<F.n;++k)sum=sum+cross(h.v[F.idx[k]]-h.v[F.idx[0]],h.v[F.idx[k+1]]-h.v[F.idx[0]]);return len(sum)*.5f;}

void mass(std::ostream& out,const Check& check){
 // A hull of a 0.4 x 0.6 x 0.2 box's corners against the box itself.
 std::vector<V3> corners;for(int i=0;i<8;++i)corners.push_back({i&1?.2f:-.2f,i&2?.3f:-.3f,i&4?.1f:-.1f});
 ConvexHull h;const bool built=ConvexHull::build(corners,h);Shape hs;hs.addHull(std::make_shared<ConvexHull>(h),{});hs.finish();Shape bs;bs.add(PartKind::Box,{},{.2f,.3f,.1f});bs.finish();
 float worst=0;for(int i=0;i<3;++i)for(int j=0;j<3;++j)worst=std::max(worst,std::fabs(hs.inertia.m[i][j]-bs.inertia.m[i][j]));
 out<<"box hull: "<<h.nv<<" corners, "<<h.nf<<" faces, "<<h.ne<<" edges, volume "<<f3(h.volume,5)<<" (box 0.048), inertia off the box's by "<<f3(worst*1e6,3)<<" e-6 kg m^2 per kg\n";
 check(built&&h.nv==8&&h.nf==6&&h.ne==12&&std::fabs(h.volume-.048f)<1e-5f&&worst<1e-5f,"mass: a hull of a box's corners has the box's volume and inertia");
 // A capsule r 0.1, segment 0.4: volume pi r^2 0.4 + 4/3 pi r^3, inertia by hand.
 Shape cs;cs.addCapsule({},.1f,.2f);cs.finish();const double r=.1,H=.4,mc=kPi*r*r*H,ms=4.0/3*kPi*r*r*r,m=mc+ms;
 const double axis=(mc*r*r/2+ms*2*r*r/5)/m,across=(mc*(H*H/12+r*r/4)+ms*(2*r*r/5+H*H/4+3*H*r/8))/m;
 out<<"capsule: inertia across "<<f3(cs.inertia.m[0][0]*1e4,4)<<" e-4 (by hand "<<f3(across*1e4,4)<<"), about its axis "<<f3(cs.inertia.m[2][2]*1e4,4)<<" e-4 (by hand "<<f3(axis*1e4,4)<<")\n";
 check(std::fabs(cs.inertia.m[0][0]-across)<1e-6&&std::fabs(cs.inertia.m[2][2]-axis)<1e-6,"mass: a capsule's inertia is its cylinder's and its two half balls'");}

struct Roll {double vzVar=0,speedVar=0,speed0=0,speed1=0,wobble=0;};
// A pipe 0.99 m long, 0.135 m round, 8 kg, lying along y and rolled along x at 2 m/s.
Roll roll(bool capsule,bool onSlab){
 World w(!onSlab);if(onSlab)w.slab();Shape s;const float r=.135f,hx=.495f;const Quat along=Quat::axisAngle({1,0,0},kPi*.5f);
 w.space.settings.roundCylinders=false;   // (the prism it was; Settings::roundCylinders makes it round: drums below)
 if(capsule)s.addCapsule({},r,hx-r,along);else s.add(PartKind::Cylinder,{},{r,r,hx},along);s.finish();
 const int p=w.add(s,{0,0,r},8);Body& b0=w.space.bodies[size_t(p)];b0.v={2,0,0};b0.w={0,2/r,0};
 std::vector<double> t,sp,vz;const float dt=1.f/120;
 for(int i=0;i<240;++i){w.space.step(dt);const Body& b=w.space.bodies[size_t(p)];if(i<30)continue;const double s2=std::sqrt(double(b.v.x)*b.v.x+double(b.v.y)*b.v.y);if(s2<.2)break;t.push_back(i*dt);sp.push_back(s2);vz.push_back(b.v.z);}
 Roll out;if(t.size()<10)return out;
 // The speed about a straight line fitted through it (its steady slowing), and the vertical speed about none.
 double mt=0,ms=0;for(size_t i=0;i<t.size();++i){mt+=t[i];ms+=sp[i];}mt/=double(t.size());ms/=double(t.size());
 double sxy=0,sxx=0;for(size_t i=0;i<t.size();++i){sxy+=(t[i]-mt)*(sp[i]-ms);sxx+=(t[i]-mt)*(t[i]-mt);}const double k=sxx>0?sxy/sxx:0;
 for(size_t i=0;i<t.size();++i){const double e=sp[i]-(ms+k*(t[i]-mt));out.speedVar+=e*e;out.vzVar+=vz[i]*vz[i];}
 out.speedVar/=double(t.size());out.vzVar/=double(t.size());out.speed0=sp.front();out.speed1=sp.back();return out;}

void rolling(std::ostream& out,const Check& check){
 for(bool slab:{true,false}){const Roll prism=roll(false,slab),cap=roll(true,slab);const std::string on=slab?"over a static slab (body against body)":"over the floor";
  out<<"a pipe rolled at 2 m/s "<<on<<": prism vertical-speed variance "<<f3(prism.vzVar*1e6,3)<<" e-6 m^2/s^2, speed variance "<<f3(prism.speedVar*1e6,3)<<" e-6 ("<<f3(prism.speed0,2)<<" to "<<f3(prism.speed1,2)<<" m/s); capsule "<<f3(cap.vzVar*1e6,4)<<" e-6, "<<f3(cap.speedVar*1e6,4)<<" e-6 ("<<f3(cap.speed0,2)<<" to "<<f3(cap.speed1,2)<<" m/s)\n";
  if(slab)check(cap.vzVar<.05*prism.vzVar+1e-7&&cap.speedVar<.05*prism.speedVar+1e-7&&cap.speed1>.5,"rolling: a capsule pipe rolls over a prop with a twentieth of the prism's speed variance or less");
  else check(cap.vzVar<1e-5&&cap.speedVar<1e-5&&cap.speed1>.5,"rolling: a capsule pipe rolls smoothly over the floor");}}

void resting(std::ostream& out,const Check& check){
 const std::array<float,3> dims{1.09f,1.01f,.70f};const Shape rock=hullShape(model::cave_rock_chunk,dims);
 check(rock.count==1&&rock.parts[0].hull!=nullptr,"resting: cave_rock_chunk has a hull in PropHulls.h");if(!rock.count)return;
 const auto& h=*rock.parts[0].hull;out<<"cave_rock_chunk's hull: "<<h.nv<<" corners, "<<h.nf<<" faces, "<<f3(h.volume,3)<<" m^3 (its box "<<f3(dims[0]*dims[1]*dims[2],3)<<")\n";
 int flat=0,tries=0;
 for(bool slab:{true,false})for(int k=0;k<4;++k){World w(!slab);if(slab)w.slab();const Quat q=(Quat::axisAngle({0,0,1},.7f*float(k))*Quat::axisAngle({1,0,0},.45f+.3f*float(k))*Quat::axisAngle({0,1,0},.2f*float(k))).normalized();
  const int r=w.add(rock,{0,0,.9f},180,q,{.7f,.6f,.15f,.004f});float t=0;for(int i=0;i<600&&!w.space.bodies[size_t(r)].sleeping;++i){w.space.step(1.f/120);t+=1.f/120;}
  const Body& b=w.space.bodies[size_t(r)];const Lie l=lieOf(b);++tries;const bool ok=b.sleeping&&l.tilt<.035f&&l.lo>-.004f&&l.hi<.004f&&l.over;flat+=ok;
  out<<"  dropped turned ("<<k<<") onto "<<(slab?"a slab":"the floor")<<": "<<(b.sleeping?"asleep":"awake")<<" after "<<f3(t,2)<<" s on face "<<l.face<<" ("<<h.f[l.face].n<<" corners), tilt "<<f3(l.tilt*57.3,2)<<" deg, its corners "<<f3(l.lo*1000,2)<<" to "<<f3(l.hi*1000,2)<<" mm off it, middle "<<(l.over?"over it":"NOT over it")<<"\n";}
 check(flat==tries,"resting: a rock's hull dropped turned lies flat on one of its own faces, asleep, its middle over it ("+std::to_string(flat)+" of "+std::to_string(tries)+")");}

void gibs(std::ostream& out,const Check& check){
 // The bottle's shards at their own sizes (ModelIds.h), each with its largest face (the break) tipped 15 degrees off the
 // floor and let fall from 2 cm: onto a static slab (body against body), onto the floor (the core's own) and in the game.
 const int shards[4]{model::gib_glass_bottle_shard_1,model::gib_glass_bottle_shard_2,model::gib_glass_bottle_shard_3,model::gib_glass_bottle_shard_4};
 const std::array<float,3> sizes[4]{{.07f,.10f,.22f},{.06f,.09f,.17f},{.05f,.09f,.17f},{.07f,.10f,.12f}};
 int flat=0,tries=0;std::array<Quat,4> pose{};std::array<int,4> broken{};
 for(int g=0;g<4;++g){const Shape& s=propShape(shards[g],sizes[g]);if(s.count!=1||s.parts[0].kind!=PartKind::Hull){check(false,"gibs: shard "+std::to_string(g+1)+" is a hull");return;}
  const auto& h=*s.parts[0].hull;int big=0;for(int f=1;f<h.nf;++f)if(faceArea(h,f)>faceArea(h,big))big=f;broken[size_t(g)]=big;
  // Turn the big face to face down, then tip it 15 degrees.
  const V3 n=h.f[big].normal;const V3 axis=cross(n,{0,0,-1});const float a=std::acos(std::clamp(dot(n,{0,0,-1}),-1.f,1.f));
  const Quat down=len(axis)>1e-6f?Quat::axisAngle(axis,a):(n.z>0?Quat::axisAngle({1,0,0},kPi):Quat{});const Quat q=(Quat::axisAngle({1,0,0},.26f)*down).normalized();pose[size_t(g)]=q;
  for(bool slab:{true,false}){World w(!slab);if(slab)w.slab();V3 lo{1e9f,1e9f,1e9f};
   {Body probe;probe.shape=&s;probe.q=q;probe.x={};for(int p=0;p<s.count;++p)lo.z=std::min(lo.z,support(probe,p,{0,0,-1}).z);}
   const int b=w.add(s,{0,0,-lo.z+.02f},.12f,q,{.45f,.35f,.45f,.0015f});float t=0;static const bool trace=std::getenv("RM_SHAPES_TRACE")!=nullptr;
   if(trace){int open=0;for(int e=0;e<h.ne;++e)open+=h.ef[e][1]<0;out<<"    hull: "<<h.nv<<" v "<<h.nf<<" f "<<h.ne<<" e, "<<open<<" open edges; volume "<<f3(h.volume*1e6,2)<<" cm^3, com "<<f3(s.com.x,4)<<","<<f3(s.com.y,4)<<","<<f3(s.com.z,4)<<"\n";}
   for(int i=0;i<720&&!w.space.bodies[size_t(b)].sleeping;++i){w.space.step(1.f/120);t+=1.f/120;
    if(trace&&g==0&&(i%30==0||i<80)){const Body& B=w.space.bodies[size_t(b)];float low=1e9f;for(int p=0;p<B.shape->count;++p)low=std::min(low,support(B,p,{0,0,-1}).z);Manifold mm;const int got=slab?collideParts(w.space.bodies[0],0,B,0,.04f,mm):0;out<<"    t "<<f3(t,3)<<" low "<<f3(low*1000,2)<<" direct "<<got<<(got?" n "+f3(mm.normal.z,3)+" s "+f3(mm.points[0].separation*1000,2):std::string())<<" v "<<f3(B.v.x,3)<<","<<f3(B.v.y,3)<<","<<f3(B.v.z,3)<<" w "<<f3(B.w.x,2)<<","<<f3(B.w.y,2)<<","<<f3(B.w.z,2);
     for(const auto& m:w.space.manifolds)if(m.a==b||m.b==b){out<<" | m"<<(m.world()?"W":"B")<<" n "<<f3(m.normal.x,2)<<","<<f3(m.normal.y,2)<<","<<f3(m.normal.z,2)<<" pts "<<m.count<<":";for(int k=0;k<m.count;++k)out<<" "<<f3(m.points[size_t(k)].separation*1000,2)<<"@"<<std::hex<<m.points[size_t(k)].feature<<std::dec;}
     out<<"\n";}}
   const Body& B=w.space.bodies[size_t(b)];const Lie l=lieOf(B,0,big);const bool ok=B.sleeping&&l.tilt<.035f&&l.lo>-.003f&&l.hi<.003f;flat+=ok;++tries;
   out<<"  shard "<<g+1<<" ("<<h.nv<<" corners, "<<h.nf<<" faces; its break face "<<big<<", "<<f3(faceArea(h,big)*1e4,1)<<" cm^2) onto "<<(slab?"a slab":"the floor")<<": "<<(B.sleeping?"asleep":"awake")<<" after "<<f3(t,2)<<" s on face "<<l.face<<", tilt "<<f3(l.tilt*57.3,2)<<" deg, corners "<<f3(l.lo*1000,2)<<" to "<<f3(l.hi*1000,2)<<" mm, going "<<f3(len(B.v)*1000,2)<<" mm/s, turning "<<f3(len(B.w),3)<<" rad/s\n";}}
 // In the game: the same four as props, set down as above in a room.
 auto m=regression::room();m->definition.playerStart={2,2};
 for(int g=0;g<4;++g){float yaw,pitch,roll;pose[size_t(g)].toEuler(yaw,pitch,roll);kit::propPose(*m,shards[g],4.f+float(g),10,.02f,yaw,pitch,roll,1);}
 Game game(regression::campaign(m));LabAccess::player(game).pos={2,2};auto& list=LabAccess::clutter(game);
 for(auto& c:list)for(int g=0;g<4;++g)if(c.model==shards[g]){c.sleeping=false;c.restTime=0;}   // (a map's props start asleep where they are put)
 for(int i=0;i<480;++i)game.update({},1.f/60);
 for(int g=0;g<4;++g)for(const auto& c:list){if(c.model!=shards[g])continue;Body b;toBody(c,b);if(b.shape->count!=1||b.shape->parts[0].kind!=PartKind::Hull)continue;const Lie l=lieOf(b,0,broken[size_t(g)]);
  const bool ok=c.sleeping&&l.tilt<.035f&&l.lo>-.003f&&l.hi<.003f;flat+=ok;++tries;
  out<<"  shard "<<g+1<<" in the game: "<<(c.sleeping?"asleep":"awake")<<" on face "<<l.face<<" (its break "<<broken[size_t(g)]<<"), tilt "<<f3(l.tilt*57.3,2)<<" deg, corners "<<f3(l.lo*1000,2)<<" to "<<f3(l.hi*1000,2)<<" mm off the floor, going "<<f3(std::sqrt(c.velocity.x*c.velocity.x+c.velocity.y*c.velocity.y+c.vz*c.vz)*1000,2)<<" mm/s, turning "<<f3(len(propAngular(c)),3)<<" rad/s, rest "<<f3(c.restTime,2)<<" s\n";break;}
 check(flat==tries&&tries==12,"gibs: each bottle shard tipped onto its broken face comes to rest flat on it, on a slab, the floor and in the game ("+std::to_string(flat)+" of "+std::to_string(tries)+")");}

void rays(std::ostream& out,const Check& check){
 const Shape rock=hullShape(model::cave_rock_chunk,{1.09f,1.01f,.70f});if(!rock.count)return;World w(false);const int r=w.add(rock,{1,2,.5f},100,Quat::axisAngle(norm(V3{1,2,3}),.8f));const Body& b=w.space.bodies[size_t(r)];
 const Part& p=b.shape->parts[0];const ConvexHull& h=*p.hull;const Quat t=b.q*p.turn;const V3 c=b.x+b.q.rotate(p.centre-b.shape->com);
 auto offHull=[&](V3 at){float most=-1e9f;const V3 l=t.unrotate(at-c);for(int f=0;f<h.nf;++f)most=std::max(most,dot(h.f[f].normal,l)-h.f[f].d);return most;};
 int hits=0,onSurface=0,shorter=0;float worst=0;
 for(int i=0;i<200;++i){const float u=float(i)/200,phi=kPi*2*u*13,z=1-2*(float(i)+.5f)/200,rr=std::sqrt(1-z*z);const V3 dir{rr*std::cos(phi),rr*std::sin(phi),z};
  const V3 o=c+dir*3;QueryHit hit;if(!rayCast(b,o,neg(dir),6,hit))continue;++hits;const float off=offHull(hit.point);worst=std::max(worst,std::fabs(off));onSurface+=std::fabs(off)<2e-4f;
  // Against its box: the hull's surface is never nearer than the box's.
  Shape box;box.add(PartKind::Box,p.centre,p.half,p.turn);box.finish();Body bb=b;bb.shape=&box;bb.x=b.x-b.q.rotate(b.shape->com)+b.q.rotate(box.com);QueryHit bh;if(rayCast(bb,o,neg(dir),6,bh)&&bh.distance<hit.distance-.01f)++shorter;}
 // Rays through its box's corner regions, just past the hull: a box would be hit, the hull is not.
 int missed=0,cornerRays=0;
 for(int i=0;i<8;++i){const V3 corner=c+t.rotate({i&1?p.half.x:-p.half.x,i&2?p.half.y:-p.half.y,i&4?p.half.z:-p.half.z})*.97f;if(offHull(corner)<.02f)continue;++cornerRays;
  const V3 dir=norm(cross(corner-c,{0,0,1}),{1,0,0});QueryHit hit;if(!rayCast(b,corner-dir*2,dir,4,hit)||offHull(hit.point)>1e-3f)++missed;else if(hit.distance>2.05f)++missed;}
 out<<"rays at the rock from 200 ways: "<<hits<<" hit, "<<onSurface<<" on its true surface (worst "<<f3(worst*1000,4)<<" mm off it); its box would have stopped "<<shorter<<" of them sooner; "<<missed<<" of "<<cornerRays<<" rays across its box's empty corners pass it\n";
 check(hits==200&&onSurface==200&&shorter>20&&missed==cornerRays&&cornerRays>0,"rays: Queries meet a hull's true surface, not its box");
 // A capsule r 0.1, segment 0.6 along x: a ray down onto its end ball off its axis and one at its side.
 Shape cs;cs.addCapsule({},.1f,.3f,Quat::axisAngle({0,1,0},kPi*.5f));cs.finish();World w2(false);const int ci=w2.add(cs,{0,0,0},5,{});const Body& cb=w2.space.bodies[size_t(ci)];
 QueryHit end,side;const bool e=rayCast(cb,{.33f,.05f,2},{0,0,-1},5,end),s=rayCast(cb,{0,0,2},{0,0,-1},5,side);
 const float want=2-std::sqrt(.01f-.03f*.03f-.05f*.05f);
 out<<"capsule rays: onto its end ball "<<f3(e?end.distance:-1,5)<<" m (by hand "<<f3(want,5)<<"), onto its side "<<f3(s?side.distance:-1,5)<<" m (1.9)\n";
 check(e&&s&&std::fabs(end.distance-want)<1e-4f&&std::fabs(side.distance-1.9f)<1e-4f,"rays: a capsule's round ends and side are met exactly");
 // Sphere and box casts and an overlap reach the hull's surface too.
 QueryHit sc;const bool sCast=sphereCast(b,c+V3{0,0,3},.05f,{0,0,-1},6,sc);const float surfaceAt=offHull(sc.point);
 int part=-1;const bool inside=overlapSphere(b,c,.05f,&part),outside=overlapSphere(b,c+V3{0,0,3},.05f);
 out<<"a 5 cm ball cast down onto the rock stops with its contact "<<f3(surfaceAt*1000,3)<<" mm off the hull; an overlap at its middle "<<(inside?"meets it":"misses")<<", 3 m over it "<<(outside?"meets it":"misses")<<"\n";
 check(sCast&&std::fabs(surfaceAt)<.002f&&inside&&!outside,"queries: a sphere cast and overlaps meet the hull");}

void game(std::ostream& out,const Check& check){
 auto m=regression::room();m->definition.playerStart={2,2};
 kit::propPose(*m,model::urb_cone,6,6,0,0,0,0,1);     // set down upright
 kit::propPose(*m,model::urb_cone,8,6,.4f,0,1.3f,0,1); // dropped on its side
 kit::propPose(*m,model::glass_bottle,6,8,.3f,0,1.57f,0,1);  // a bottle dropped on its side
 kit::propPose(*m,model::glass_bottle,8,8,0,0,0,0,1);  // a bottle set down upright
 Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};auto& list=LabAccess::clutter(g);
 for(int i=0;i<300;++i)g.update({},1.f/60);
 int found=0,ok=0;
 for(const auto& c:list){if(c.model!=model::urb_cone&&c.model!=model::glass_bottle)continue;++found;Body b;toBody(c,b);float low=1e9f;for(int p=0;p<b.shape->count;++p)low=std::min(low,support(b,p,{0,0,-1}).z);
  const V3 up=b.q.rotate({0,0,1});const bool upright=up.z>.995f;const char* what=c.model==model::urb_cone?"cone":"bottle";const PartKind k=b.shape->parts[0].kind;
  const bool setUpright=(c.model==model::urb_cone&&c.pos.x<7)||(c.model==model::glass_bottle&&c.pos.x>7);   // the cone at 6,6 and the bottle at 8,8
  const bool good=c.sleeping&&std::fabs(low)<.005f&&(!setUpright||upright);ok+=good;
  out<<"  "<<what<<" at "<<f3(c.pos.x,1)<<","<<f3(c.pos.y,1)<<" ("<<(k==PartKind::Hull?"hull":k==PartKind::Capsule?"capsule":"other")<<"): "<<(c.sleeping?"asleep":"awake")<<", lowest point "<<f3(low*1000,2)<<" mm off the floor, its up "<<f3(std::acos(std::clamp(up.z,-1.f,1.f))*57.3,1)<<" deg from vertical\n";}
 check(found==4&&ok==4,"game: cones (hulls) and bottles (capsules) settle on the floor; set down upright they stay upright ("+std::to_string(ok)+" of "+std::to_string(found)+")");}

// Every authored gib piece that is a hull, let fall in the game in three turns: each comes to rest on the floor, asleep.
void pieces(std::ostream& out,const Check& check){
 auto m=regression::room();m->definition.playerStart={2,2};std::vector<int> models;
 for(const auto& p:gibs::pieces)if(!p.cylinder&&hulls::find(p.model)&&std::find(models.begin(),models.end(),p.model)==models.end())models.push_back(p.model);   // (a bottle's neck is a capsule)
 for(size_t i=0;i<models.size();++i)for(int k=0;k<3;++k)kit::propPose(*m,models[i],3.f+.7f*float(i%14),4.f+.8f*float(i/14)*3+.8f*float(k),.15f,.7f*float(k),.5f+.9f*float(k),.3f*float(k),1);
 Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};auto& list=LabAccess::clutter(g);for(auto& c:list){c.sleeping=false;c.restTime=0;}
 for(int i=0;i<720;++i)g.update({},1.f/60);
 int asleep=0,all=0;float lowest=1e9f;std::string which;
 for(const auto& c:list){if(std::find(models.begin(),models.end(),c.model)==models.end())continue;++all;asleep+=c.sleeping;Body b;toBody(c,b);for(int p=0;p<b.shape->count;++p)lowest=std::min(lowest,support(b,p,{0,0,-1}).z);
  if(!c.sleeping)which+=" "+std::to_string(c.model)+" ("+f3(std::sqrt(c.velocity.x*c.velocity.x+c.velocity.y*c.velocity.y+c.vz*c.vz),3)+" m/s, "+f3(len(propAngular(c)),3)+" rad/s)";}
 out<<models.size()<<" gib pieces with hulls, three turns each: "<<asleep<<" of "<<all<<" asleep after 12 s, the lowest point "<<f3(lowest*1000,2)<<" mm off the floor;"<<(which.empty()?" none awake":" awake:"+which)<<"\n";
 check(all==int(models.size())*3&&asleep==all&&lowest>-.004f,"gibs: every hull gib piece let fall in the game comes to rest on the floor, asleep");
 // The monitor's and the bottle's pieces dropped together in a heap, as a break leaves them: on each other, they sleep too.
 for(int set=0;set<2;++set){auto h=regression::room();h->definition.playerStart={2,2};int n=0;
  for(const auto& p:gibs::pieces){const bool mine=set==0?(p.model>=model::gib_crt_monitor_screen_1&&p.model<=model::gib_crt_monitor_stand)||p.model==model::gib_crt_monitor_casing:(p.model>=model::gib_glass_bottle_neck&&p.model<=model::gib_glass_bottle_shard_4);
   if(!mine)continue;for(int k=0;k<2;++k){kit::propPose(*h,p.model,8.f+.07f*float(n%3),8.f+.06f*float(n%2),.1f+.12f*float(n),.9f*float(n),.4f*float(n%4),.3f*float(k),1);++n;}}
  Game hg(regression::campaign(h));LabAccess::player(hg).pos={2,2};auto& hl=LabAccess::clutter(hg);for(auto& c:hl){c.sleeping=false;c.restTime=0;}
  for(int i=0;i<600;++i)hg.update({},1.f/60);
  int hs=0,ha=0;std::string hw;for(const auto& c:hl){if(c.model<0)continue;++ha;hs+=c.sleeping;if(!c.sleeping)hw+=" "+std::to_string(c.model)+" ("+f3(std::sqrt(c.velocity.x*c.velocity.x+c.velocity.y*c.velocity.y+c.vz*c.vz),3)+" m/s, "+f3(len(propAngular(c)),3)+" rad/s)";}
  out<<(set==0?"the monitor's":"the bottle's")<<" pieces, two of each, dropped in a heap: "<<hs<<" of "<<ha<<" asleep after 10 s"<<(hw.empty()?"":"; awake:"+hw)<<"\n";
  check(ha==n&&hs==ha,std::string("gibs: ")+(set==0?"the monitor's":"the bottle's")+" pieces dropped in a heap all come to rest, asleep");}}

void cost(std::ostream& out,const Check& check){
 const Shape rock=hullShape(model::cave_rock_chunk,{.36f,.33f,.23f});if(!rock.count)return;
 World w;for(int i=0;i<120;++i){const float x=float(i%6)*.45f,y=float((i/6)%5)*.45f,z=.3f+float(i/30)*.4f;w.add(rock,{x,y,z},12,Quat::axisAngle(norm(V3{float(i%3)+.3f,1,float(i%5)}),float(i)*.37f),{.7f,.6f,.15f,.004f});}
 double total=0,worst=0;int n=0;for(int i=0;i<360;++i){const auto t0=std::chrono::steady_clock::now();w.space.step(1.f/120);const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();total+=ms;worst=std::max(worst,ms);++n;}
 int asleep=0;for(const auto& b:w.space.bodies)asleep+=b.sleeping;
 out<<"120 rock hulls ("<<rock.parts[0].hull->nv<<" corners each) dropped as a heap: "<<f3(total/n,3)<<" ms a step over 3 s (worst "<<f3(worst,3)<<"; the last step's pairs "<<f3(w.space.pairMs,3)<<", solve "<<f3(w.space.solveMs,3)<<"), "<<asleep<<" asleep at the end\n";
 // The same heap of boxes the rocks' size, for scale.
 {World wb;Shape bx;bx.add(PartKind::Box,{},{.18f,.165f,.115f});bx.finish();for(int i=0;i<120;++i){const float x=float(i%6)*.45f,y=float((i/6)%5)*.45f,z=.3f+float(i/30)*.4f;wb.add(bx,{x,y,z},12,Quat::axisAngle(norm(V3{float(i%3)+.3f,1,float(i%5)}),float(i)*.37f));}
  double tb=0;for(int i=0;i<360;++i){const auto t0=std::chrono::steady_clock::now();wb.space.step(1.f/120);tb+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();}
  int sb=0;for(const auto& b:wb.space.bodies)sb+=b.sleeping;out<<"the same heap of boxes: "<<f3(tb/360,3)<<" ms a step, "<<sb<<" asleep at the end\n";}
 check(total/n<4,"cost: 120 hull rocks in a heap step in under 4 ms");}

// A drum (0.29 m round, 0.88 m long, 35 kg) on its side rolled at 2 m/s over a static slab, as its 12-sided prism and as
// the true round (Settings::roundCylinders); and stood on its end on the slab, leant 2 degrees.
Roll rollDrum(bool round){
 World w(false);w.space.settings.roundCylinders=round;w.slab();Shape s;const float r=.29f,hz=.44f;s.add(PartKind::Cylinder,{},{r,r,hz},Quat::axisAngle({1,0,0},kPi*.5f));s.finish();
 const int p=w.add(s,{0,0,r},35);Body& b0=w.space.bodies[size_t(p)];b0.v={2,0,0};b0.w={0,2/r,0};
 std::vector<double> t,sp,vz;const float dt=1.f/120;
 for(int i=0;i<240;++i){w.space.step(dt);const Body& b=w.space.bodies[size_t(p)];if(i<30)continue;const double s2=std::sqrt(double(b.v.x)*b.v.x+double(b.v.y)*b.v.y);if(s2<.2)break;t.push_back(i*dt);sp.push_back(s2);vz.push_back(b.v.z);}
 Roll out;if(t.size()<10)return out;
 double mt=0,ms=0;for(size_t i=0;i<t.size();++i){mt+=t[i];ms+=sp[i];}mt/=double(t.size());ms/=double(t.size());
 double sxy=0,sxx=0;for(size_t i=0;i<t.size();++i){sxy+=(t[i]-mt)*(sp[i]-ms);sxx+=(t[i]-mt)*(t[i]-mt);}const double k=sxx>0?sxy/sxx:0;
 for(size_t i=0;i<t.size();++i){const double e=sp[i]-(ms+k*(t[i]-mt));out.speedVar+=e*e;out.vzVar+=vz[i]*vz[i];}
 out.speedVar/=double(t.size());out.vzVar/=double(t.size());out.speed0=sp.front();out.speed1=sp.back();return out;}
struct Stand {bool asleep=false;float tilt=9,drift=9,sink=9;};
Stand standDrum(bool round){
 World w(false);w.space.settings.roundCylinders=round;w.slab();Shape s;const float r=.29f,hz=.44f;s.add(PartKind::Cylinder,{},{r,r,hz});s.finish();
 const Quat lean=Quat::axisAngle({1,0,0},2*kPi/180);const int p=w.add(s,{0,0,hz+.002f},35,lean);const V3 start=w.space.bodies[size_t(p)].x;
 for(int i=0;i<360;++i)w.space.step(1.f/120);const Body& b=w.space.bodies[size_t(p)];Stand out;out.asleep=b.sleeping;
 out.tilt=std::acos(std::clamp(b.q.rotate({0,0,1}).z,-1.f,1.f));out.drift=std::hypot(b.x.x-start.x,b.x.y-start.y);out.sink=std::max(0.f,-support(b,0,{0,0,-1}).z);return out;}
void drums(std::ostream& out,const Check& check){
 const Roll prism=rollDrum(false),round=rollDrum(true);
 out<<"a drum on its side rolled at 2 m/s over a static slab: the prism's vertical-speed variance "<<f3(prism.vzVar*1e6,3)<<" e-6 m^2/s^2, speed variance "<<f3(prism.speedVar*1e6,3)<<" e-6 ("<<f3(prism.speed0,2)<<" to "<<f3(prism.speed1,2)<<" m/s); the round's "<<f3(round.vzVar*1e6,4)<<" e-6, "<<f3(round.speedVar*1e6,4)<<" e-6 ("<<f3(round.speed0,2)<<" to "<<f3(round.speed1,2)<<" m/s)\n";
 check(round.vzVar<.05*prism.vzVar+1e-7&&round.speedVar<.05*prism.speedVar+1e-7&&round.speed1>.5,"rolling: a drum rolls over a prop as a true round, with a twentieth of its prism's speed variance or less");
 const Stand up=standDrum(true),upPrism=standDrum(false);
 out<<"a drum stood on its end on the slab, leant 2 degrees: "<<(up.asleep?"asleep":"awake")<<" after 3 s, "<<f3(up.tilt*57.3,3)<<" deg off upright, drifted "<<f3(up.drift*1000,2)<<" mm, "<<f3(up.sink*1000,2)<<" mm into the slab (the prism: "<<(upPrism.asleep?"asleep":"awake")<<", "<<f3(upPrism.tilt*57.3,3)<<" deg, "<<f3(upPrism.drift*1000,2)<<" mm)\n";
 check(up.asleep&&up.tilt<.01f&&up.sink<.003f,"standing: a drum stood leant on its end on a prop settles flat and upright on it and sleeps");}
// In the game's water (RigidClutter.cpp): a sealed drum floating on its side, turned about its own axis at 4 rad/s: how much
// it bobs (its vertical-speed variance over a second) with its cells round its axis (water.round_cells 1) and in the old
// square turned with it (0, from a tune file of this tube's own).
double drumBob(bool roundCells){std::string saved;{auto& tf=TuneFile::instance();std::lock_guard<std::mutex> hold(tf.lock);saved=tf.path;}
 if(!roundCells){std::ofstream("vectra-shapes-square-cells.txt")<<"water.round_cells 0\n";tuneFile("vectra-shapes-square-cells.txt");}
 constexpr float Bed=-1.2f,Surface=-.05f;auto m=regression::room();m->definition.playerStart={2,2};m->waterVolumes.push_back({3,6,11,14,Bed,Surface});
 forceLegacyProps(0);Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};
 Clutter c;c.kind=7;c.model=model::ind_barrel;c.dims=Game::propTraits(c.model).size;c.pos={7,10};c.z=Surface-.3f;c.roll=kPi*.5f;c.sleeping=false;
 {auto t=std::make_shared<EntityTraits>();t->density=220;t->floats=1;c.traits=t;}
 LabAccess::clutter(g)={c};for(int i=0;i<300;++i)g.update({},1.f/60);
 double var=0;int n=0;for(int i=0;i<60;++i){auto& d=LabAccess::clutter(g)[0];{Body b;toBody(d,b);setPropAngular(d,b.q.rotate({0,0,1})*4.f);}d.sleeping=false;g.update({},1.f/60);if(i>=10){var+=double(d.vz)*d.vz;++n;}}
 forceLegacyProps(-1);if(!roundCells)tuneFile(saved);return var/std::max(1,n);}
void drumWater(std::ostream& out,const Check& check){
 const double round=drumBob(true),square=drumBob(false);
 out<<"a sealed drum afloat on its side, turned about its axis at 4 rad/s: vertical-speed variance "<<f3(round*1e6,3)<<" e-6 m^2/s^2 with its water cells round its axis, "<<f3(square*1e6,3)<<" e-6 in the old square\n";
 check(round<.5*square,"water: a drum turning afloat on its side bobs half as much or less with its cells round its axis");}

const bool registered=add({.name="vectra-shapes",.summary="capsules and convex hulls: mass, a pipe rolling smoothly, a rock and gib shards resting on their true faces, rays on the true surface, cones and bottles in the game, the cost of a heap",.standalone=[]{
 std::ofstream out("vectra-shapes.txt");bool ok=true;
 auto check=[&](bool pass,const std::string& what){out<<(pass?"PASS ":"FAIL ")<<what<<'\n';out.flush();ok&=pass;};
 out<<"---- mass ----\n";mass(out,check);
 out<<"---- rolling ----\n";rolling(out,check);
 out<<"---- drums ----\n";drums(out,check);drumWater(out,check);
 out<<"---- resting ----\n";resting(out,check);
 out<<"---- gibs ----\n";gibs(out,check);
 out<<"---- rays ----\n";rays(out,check);
 out<<"---- game ----"<<std::endl;game(out,check);pieces(out,check);
 out<<"---- cost ----\n";cost(out,check);
 return ok;}});
}
}

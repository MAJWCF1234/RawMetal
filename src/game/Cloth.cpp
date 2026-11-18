// Curtains, straps and cables as they play (Cloth.h): built from the map's CLOTH records, stepped on Vectra.
#include "Cloth.h"
#include "Game.h"
#include "physics/PropBodies.h"
#include "../core/Tune.h"
#include <algorithm>
#include <cmath>
namespace retro {
namespace {
using namespace vectra;
constexpr float ClothStep=1.f/120;
Material clothMaterial(){return {.85f,.7f,.02f,.02f};}
V3 v3of(Point3 p){return {p.x,p.y,p.z};}
// A CLOTH record's beads and joints, hanging as the map lays it out.
std::shared_ptr<ClothRun> buildCloth(const ClothSpec& c){
 auto run=std::make_shared<ClothRun>();ClothRun& r=*run;r.spec=c;
 const float stiff=c.stiffness>0?c.stiffness:tune("cloth.hertz",120.f);   // (at 30 Hz a 1.2 m strap of 15 links stretched 9 cm under its own weight)
 const V3 a=v3of(c.at),b=v3of(c.to);std::vector<V3> pos;std::vector<char> pinned;float total=0;
 if(c.kind==ClothKind::Curtain){
  // Columns every 20 cm along the rail, rows every 20 cm down it; the top row on the rail.
  const float width=len(b-a),drop=c.drop>0?c.drop:1.6f;
  r.cols=std::clamp(int(std::lround(width/.2f))+1,3,14);r.rows=std::clamp(int(std::lround(drop/.2f))+1,3,12);r.bead=.03f;
  for(int row=0;row<r.rows;++row)for(int col=0;col<r.cols;++col){pos.push_back(a+(b-a)*(float(col)/float(r.cols-1))-V3{0,0,drop*float(row)/float(r.rows-1)});pinned.push_back(row==0);}
  total=c.mass>0?c.mass:.35f*width*drop;}
 else if(c.kind==ClothKind::Strap){
  const float length=c.length>0?c.length:1.f;r.cols=std::clamp(int(std::lround(length/.08f))+1,4,20);r.rows=1;
  r.bead=std::clamp((c.width>0?c.width:.05f)*.5f,.012f,.05f);
  for(int k=0;k<r.cols;++k){pos.push_back(a-V3{0,0,length*float(k)/float(r.cols-1)});pinned.push_back(k==0);}
  total=c.mass>0?c.mass:.25f;}
 else{
  // A cable: along a parabola from end to end as deep as its spare length asks (length = span + 8 d^2 / 3 span); loose,
  // laid straight toward where its free end lies.
  const float span=std::max(.05f,len(b-a)),length=std::max(span,c.length>0?c.length:1.15f*span);
  r.cols=std::clamp(int(std::lround(length/.08f))+1,5,32);r.rows=1;r.bead=std::max(.012f,c.radius>0?c.radius:.012f);
  const float sag=c.loose?0.f:std::sqrt(3*span*(length-span)/8);const V3 way=c.loose?norm(b-a)*length:b-a;
  for(int k=0;k<r.cols;++k){const float t=float(k)/float(r.cols-1);pos.push_back(a+way*t-V3{0,0,4*sag*t*(1-t)});pinned.push_back(k==0||(!c.loose&&k==r.cols-1));}
  total=c.mass>0?c.mass:.4f*length;}
 r.beads=int(pos.size());int free=0;for(char p:pinned)free+=!p;const float each=total/float(std::max(1,free));
 r.shapes.emplace_back();Shape& bead=r.shapes.back();bead.add(PartKind::Sphere,{},{r.bead,r.bead,r.bead});bead.finish();
 for(int i=0;i<r.beads;++i){Body body;body.shape=&bead;body.x=pos[size_t(i)];body.material=clothMaterial();body.kinematic=pinned[size_t(i)]!=0;body.setMass(each);body.updateInertia();
  body.linearDamping=tune("cloth.drag",1.2f);body.angularDamping=2;body.group=1;body.id=i;body.user=i;r.space.bodies.push_back(body);}
 r.safe.assign(size_t(r.beads),Point3{});for(int i=0;i<r.beads;++i)r.safe[size_t(i)]={pos[size_t(i)].x,pos[size_t(i)].y,pos[size_t(i)].z};
 r.floor.assign(size_t(r.beads),-1e9f);r.probe.assign(size_t(r.beads),Point3{1e9f,1e9f,1e9f});r.open.assign(size_t(r.beads),0);
 // Ropes between neighbours (held at most as far apart as they hang, slack nearer: it folds and drapes), and a weak spring
 // across every other bead along its length and down it (it bends rather than kinks).
 auto join=[&](JointKind kind,int i,int j,float hertz){if(pinned[size_t(i)]&&pinned[size_t(j)])return;r.joints.push_back(std::make_unique<Joint>());Joint& jt=*r.joints.back();jt.kind=kind;jt.a=i;jt.b=j;
  jt.attachEnds(r.space,pos[size_t(i)],pos[size_t(j)]);if(kind==JointKind::Spring){jt.springHertz=hertz;jt.springDamping=.6f;}else{jt.lockHertz=hertz;jt.lockDamping=1;}r.space.constraints.push_back(&jt);};
 auto id=[&](int col,int row){return row*r.cols+col;};
 for(int row=0;row<r.rows;++row)for(int col=0;col+1<r.cols;++col)join(JointKind::Rope,id(col,row),id(col+1,row),stiff);
 for(int row=0;row+1<r.rows;++row)for(int col=0;col<r.cols;++col)join(JointKind::Rope,id(col,row),id(col,row+1),stiff);
 // (A curtain has none: it hangs in folds, and its grid's ropes were already most of its cost.)
 const float bend=tune("cloth.bend",1.5f);
 if(r.rows==1)for(int col=0;col+2<r.cols;++col)join(JointKind::Spring,id(col,0),id(col+2,0),bend);
 // Two joint passes a substep (four substeps a step): a grid of ropes this light converges in two, at half the cost.
 r.space.settings.sleepTime=.4f;r.space.settings.jointIterations=2;
 r.spacing=r.rows>1?len(pos[size_t(id(1,0))]-pos[size_t(id(0,0))]):len(pos[1]-pos[0]);
 return run;}
// The capsule a person is (a standing man's height, a creature's), kept for the step.
const Shape& personShape(float radius,float height){static std::deque<std::pair<std::pair<int,int>,Shape>> cache;const std::pair<int,int> key{int(radius*1000),int(height*1000)};
 for(auto& [k,s]:cache)if(k==key)return s;cache.push_back({key,{}});Shape& s=cache.back().second;s.addCapsule({},radius,std::max(0.f,height*.5f-radius));s.finish();return s;}
}

void Game::stepCloth(float dt){
 const AuthoredMapData* data=m_world.mapData();
 if(data!=m_clothMap||m_level!=m_clothLevel){m_cloths.clear();m_clothMap=data;m_clothLevel=m_level;m_clothPlayerSeen=false;for(const auto& c:m_world.cloths())m_cloths.push_back(buildCloth(c));}
 if(m_cloths.empty()||dt<=0)return;
 // The player's own velocity (as the frames move him), for what he walks through.
 const Point3 feet{m_player.pos.x,m_player.pos.y,m_player.z};Point3 moved{};
 if(m_clothPlayerSeen&&dt>1e-5f)moved=(feet-m_clothPlayerAt)*(1/dt);m_clothPlayerAt=feet;m_clothPlayerSeen=true;
 if(std::sqrt(moved.x*moved.x+moved.y*moved.y+moved.z*moved.z)>15)moved={};   // (a teleport, not a walk)
 // Out under the sky the breeze the plants bend in (Foliage.cpp: turning and gusting, harder in the storm); none indoors.
 const bool open=m_world.zoneSky()||m_world.hasTerrain();const float now=m_elapsed;V3 breeze{};
 if(open){const float storm=std::clamp(stormIntensity(),0.f,1.f),gust=.5f+.5f*std::sin(now*.37f)*std::sin(now*.11f+1.f);
  const float speed=tune("plants.wind",1.f)*(1.f+.35f*std::sin(now*.21f)+.25f*std::sin(now*.57f+1.3f))*(1.f+2.5f*storm)*(.6f+.8f*gust),angle=.7f+.35f*std::sin(now*.031f);
  breeze=V3{std::cos(angle),std::sin(angle),0}*(speed*tune("cloth.wind",1.2f));}
 for(auto& run:m_cloths){ClothRun& r=*run;r.carry=std::min(r.carry+dt,4*ClothStep);
  while(r.carry>=ClothStep){r.carry-=ClothStep;
   Space& s=r.space;s.bodies.resize(size_t(r.beads));
   // Its box, and what moves in or near it: props, the player, creatures (kinematic bodies of their own shapes).
   V3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};for(int i=0;i<r.beads;++i){const V3 x=s.bodies[size_t(i)].x;lo={std::min(lo.x,x.x),std::min(lo.y,x.y),std::min(lo.z,x.z)};hi={std::max(hi.x,x.x),std::max(hi.y,x.y),std::max(hi.z,x.z)};}
   r.lo={lo.x,lo.y,lo.z};r.hi={hi.x,hi.y,hi.z};const float reach=.4f;
   auto inReach=[&](V3 c,float radius,float top,float bottom){return c.x+radius>lo.x-reach&&c.x-radius<hi.x+reach&&c.y+radius>lo.y-reach&&c.y-radius<hi.y+reach&&top>lo.z-reach&&bottom<hi.z+reach;};
   bool stirring=open;
   for(int i=0;i<int(m_clutter.size());++i){const Clutter& c=m_clutter[size_t(i)];if(c.model<0||c.broken)continue;const auto e=c.extent();const float rad=std::max(e[0],e[1])*1.5f;
    if(!inReach({c.pos.x,c.pos.y,0},rad,c.z+2*e[2],c.z))continue;Body b;vectra::toBody(c,b);b.kinematic=true;b.setMass(1);b.sleeping=false;b.group=0;b.id=100000+i;b.user=-1;b.updateInertia();
    if(!c.sleeping&&(lenSq(b.v)>1e-6f||lenSq(b.w)>1e-6f))stirring=true;else{b.v={};b.w={};}s.bodies.push_back(b);}
   auto person=[&](Vec2 at,float z,float radius,float height,V3 velocity,int id){if(!inReach({at.x,at.y,0},radius,z+height,z))return;Body b;b.shape=&personShape(radius,height);b.x={at.x,at.y,z+height*.5f};b.kinematic=true;b.setMass(1);b.v=velocity;b.group=0;b.id=id;b.user=-1;b.updateInertia();b.material={.12f,.08f,0,.002f};   // (cloth slides over clothes and skin: it parts round her, it is not carried along)
    s.bodies.push_back(b);if(lenSq(velocity)>1e-4f)stirring=true;};
   person(m_player.pos,m_player.z,.28f,m_player.hullHeight(),{moved.x,moved.y,moved.z},200000);
   for(int i=0;i<int(m_enemies.size());++i){const auto& e=m_enemies[size_t(i)];if(e.alive)person(e.pos,e.z,.35f,1.7f,{},300000+i);}
   // Asleep with nothing moving near and no wind: nothing to step.
   if(!stirring&&r.asleep()){s.bodies.resize(size_t(r.beads));continue;}
   if(open)for(int i=0;i<r.beads;++i){Body& b=s.bodies[size_t(i)];if(b.kinematic)continue;
    // Each bead's own share, flickering by its place (a cloth ripples in a gust, it does not swing as one board).
    const float flick=.75f+.25f*std::sin(now*2.3f+float(i)*1.7f);b.force=breeze*(flick/std::max(1e-6f,b.invMass)*1.5f);b.sleeping=false;b.sleepTime=0;}
   // The world: the floor under each bead and the walls round it (a bead in a wall meets it toward where it was last clear).
   s.worldContacts=[&](Space& sp,int bi,float margin,std::vector<Manifold>& out){if(bi>=r.beads)return;const Body& b=sp.bodies[size_t(bi)];const V3 x=b.x;const float rad=r.bead;
    // (Asked again only once it has gone 5 cm: the floor under it, and whether there is any wall within a bead and 5 cm.)
    Point3& probe=r.probe[size_t(bi)];if((x.x-probe.x)*(x.x-probe.x)+(x.y-probe.y)*(x.y-probe.y)>.0025f||std::fabs(x.z-probe.z)>.05f){r.floor[size_t(bi)]=m_world.supportBelow(x.x,x.y,x.z+.05f,true);probe={x.x,x.y,x.z};
     bool clear=true;const float d=rad+.05f;for(int k=0;k<4&&clear;++k)clear=m_world.fits(x.x+(k==0?d:k==1?-d:0),x.y+(k==2?d:k==3?-d:0),x.z-rad,2*rad,true,true);r.open[size_t(bi)]=clear&&m_world.fits(x.x,x.y,x.z-rad,2*rad,true,true);}
    auto one=[&](V3 normal,V3 at,float sep,int cluster){Manifold m;m.a=bi;m.b=-1;m.partA=0;m.partB=cluster;m.normal=normal;m.material=Material::mix(b.material,{.6f,.5f,0,.004f});auto& c=m.points[0];c=ContactPoint{};c.p=at;c.separation=sep;c.localA=b.q.unrotate(at-b.x);c.localB=at;m.count=1;out.push_back(m);};
    const float sep=x.z-rad-r.floor[size_t(bi)];if(sep<margin&&sep>-.06f)one({0,0,-1},{x.x,x.y,x.z-rad},sep,0);
    if(r.open[size_t(bi)]){r.safe[size_t(bi)]={x.x,x.y,x.z};return;}
    const bool clear=m_world.fits(x.x,x.y,x.z-rad*.5f,rad,true,true);
    if(clear){r.safe[size_t(bi)]={x.x,x.y,x.z};
     // Round it, a bead's width out: a wall there is met at no gap (it slides along it, not into it).
     for(int k=0;k<4;++k){const V3 d=k==0?V3{1,0,0}:k==1?V3{-1,0,0}:k==2?V3{0,1,0}:V3{0,-1,0};const V3 q=x+d*rad;if(!m_world.fits(q.x,q.y,x.z-rad*.5f,rad,true,true))one(d,q,0.f,1+k);}}
    else{const Point3 s0=r.safe[size_t(bi)];V3 n{x.x-s0.x,x.y-s0.y,0};const float l=len(n);if(l>1e-4f&&l<.5f)one(n*(1/l),x,-l,5);}};
   s.step(ClothStep);s.worldContacts=nullptr;s.bodies.resize(size_t(r.beads));}}}

void Game::shootCloth(Point3 origin,Point3 end){
 const V3 o=v3of(origin),e=v3of(end),way=e-o;const float l2=lenSq(way);if(l2<1e-8f)return;const V3 dir=way*(1/std::sqrt(l2));
 for(auto& run:m_cloths){ClothRun& r=*run;bool hit=false;
  for(int i=0;i<r.beads;++i){Body& b=r.space.bodies[size_t(i)];if(b.kinematic)continue;const float t=std::clamp(dot(b.x-o,way)/l2,0.f,1.f);const float d=len(o+way*t-b.x);
   const float reach=std::max(r.bead+.04f,r.spacing*.6f);if(d>reach)continue;
   // The round knocks the beads it passes (most the nearest), and on along its way.
   const float k=1-d/reach;b.v=b.v+dir*(tune("cloth.shot",4.f)*k);hit=true;}
  if(hit)for(int i=0;i<r.beads;++i)r.space.wake(i);}}

bool Game::clothBeads(int index,std::vector<Point3>& points,int& cols,int& rows,float& bead)const{
 if(index<0||index>=int(m_cloths.size()))return false;const ClothRun& r=*m_cloths[size_t(index)];cols=r.cols;rows=r.rows;bead=r.bead;points.resize(size_t(r.beads));
 for(int i=0;i<r.beads;++i){const V3 x=r.space.bodies[size_t(i)].x;points[size_t(i)]={x.x,x.y,x.z};}return true;}
const ClothRun* Game::clothRun(int index)const{return index>=0&&index<int(m_cloths.size())?m_cloths[size_t(index)].get():nullptr;}
}

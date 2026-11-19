// Model props on Vectra (physics/Vectra.h). simulateClutter still runs everything that happens to
// a prop from outside (the player's push, a walking body's kick, a swinging door, a hand), and the old
// point physics for junk and shards; this steps the model props themselves:
//  - at a fixed 120 Hz (two soft substeps each), whatever the frame rate, from the leftover time;
//  - against the world as the World answers for it: points over each part's surface (and its exact
//    lowest points, so a barrel rolls round) asked where the floor, shelf, ramp or terrain is under them
//    (supportBelow, its slope by differences), whether they are inside a wall, fixture or door (fits; the
//    face and its normal by a ray from the middle) and what is over them (clearanceAbove), grouped by
//    normal into manifolds of four;
//  - against each other with full manifolds; a held prop is kinematic (it pushes, it is not pushed);
//  - in water by the volume under the surface, point by point (so it floats level, or rights itself);
//  - and back: impact sounds and AI hearing from the contact's approach speed, breaking past a material's
//    break speed, glass from the energy the contact's effective mass brings into the pane, bodies struck.
#include "Game.h"
#include "Buoyancy.h"
#include "Containers.h"
#include "physics/PropBodies.h"
#include "../core/Tune.h"
#include "../world/PrimitiveKinds.h"
#include <chrono>
#include <deque>
#include <cstdio>
#include <cstdlib>
namespace retro {
namespace {
constexpr float RigidStep=1.f/120;constexpr int RigidMaxSteps=6;constexpr int PlayerBody=-2,ActorBody=-100;   // ActorBody-k: the k-th man or creature this step
// Panes, as Clutter.cpp's strike has them.
constexpr float PaneTap=2,PaneRearm=.3f;
float paneBreakEnergy(const Breakable& b){return b.material==17?(b.holds?450.f:150.f):b.material==22?40.f:300.f;}
float paneLimit(const Breakable& b){return b.holds?180.f:b.health>0?b.health:b.material==17?20.f:b.material==22?8.f:35.f;}
// The ground's material against a prop: a hard floor that takes no bounce of its own.
const vectra::Material Ground{.6f,.5f,.3f,.004f};   // a floor is rough: what rolls on it comes to rest; its bounce, times the prop's (Space::prepare)
// The floor's own material where a prop stands on it (the World's surface materials), mixed with the prop's in
// Space::prepare: painted ground on terrain (grass and mud give, gravel grips and drags what rolls), carpet, a metal
// deck, the map's floor finish, and over any of them water: a wet patch, most of all a puddle, is slick.
vectra::Material floorGrip(const World& w,float x,float y,float floor){
 vectra::Material m=Ground;
 const int finish=w.floorSurface();
 if(w.definition().floorMaterial==FloorMaterial::OfficeCarpet)m={.8f,.65f,.1f,.012f};
 else if(w.metalFloor(int(x),int(y))||finish==SurfaceCorrugated||finish==SurfacePressure)m={.45f,.35f,.35f,.002f};
 else if(finish==SurfaceClinicTile||finish==SurfaceLabPanel)m={.5f,.4f,.3f,.003f};
 else if(finish==SurfaceMineTimber)m={.62f,.5f,.25f,.004f};
 else if(finish==SurfaceRock||finish==SurfaceStone)m={.7f,.6f,.3f,.006f};
 if(w.hasTerrain()&&w.structureSupportBelow(x,y,.02f,floor+.05f)<floor-.03f){   // (not on a structure standing on it)
  const auto o=w.definition().origin;
  switch(w.groundKind(x+o.x,y+o.y)){
   case GroundShortGrass:case GroundLeafyGrass:case GroundClover:case GroundWild:m={.55f,.45f,.1f,.03f};break;
   case GroundDry:case GroundLitter:m={.65f,.55f,.15f,.012f};break;
   case GroundGravel:m={.7f,.6f,.15f,.02f};break;
   case GroundMud:m={.4f,.3f,.05f,.03f};break;
   default:break;}}
 float wet=0;for(const auto&p:w.wetPatches()){if(x<p.x1||x>p.x2||y<p.y1||y>p.y2)continue;
  if(p.round){const float cx=(p.x1+p.x2)*.5f,cy=(p.y1+p.y2)*.5f,rx=std::max(.01f,(p.x2-p.x1)*.5f),ry=std::max(.01f,(p.y2-p.y1)*.5f);if((x-cx)*(x-cx)/(rx*rx)+(y-cy)*(y-cy)/(ry*ry)>1)continue;}
  wet=std::max(wet,std::clamp(p.amount,0.f,1.f)*(p.puddle?1.f:.6f));}
 if(wet>0){const vectra::Material water{.22f,.15f,0,.002f};auto mixTo=[&](float a,float b){return a+(b-a)*wet;};
  m={mixTo(m.staticFriction,water.staticFriction),mixTo(m.dynamicFriction,water.dynamicFriction),mixTo(m.restitution,.1f),mixTo(m.rolling,water.rolling)};}
 return m;}
struct Hit {vectra::V3 p{},n{};float sep=0;int kind=0,part=0;};
// Up to four of the hits: the deepest, the farthest from it, the largest triangle, the largest quad. (The deepest is the
// first within a third of a millimetre of it: points on one resting face are all as deep, and the first stays first
// from step to step where the deepest by a hair would change.)
int keepFour(std::vector<Hit>& h,vectra::V3 n){
 using namespace vectra;if(h.size()<=4)return int(h.size());int pick[4]{0,-1,-1,-1};
 {float least=1e30f;for(const auto& x:h)least=std::min(least,x.sep);for(int i=0;i<int(h.size());++i)if(h[size_t(i)].sep<=least+.0003f){pick[0]=i;break;}}
 float best=-1;for(int i=0;i<int(h.size());++i){const float d=lenSq(h[size_t(i)].p-h[size_t(pick[0])].p);if(d>best){best=d;pick[1]=i;}}
 auto area=[&](V3 a,V3 b,V3 c){return std::fabs(dot(cross(b-a,c-a),n));};
 best=-1;for(int i=0;i<int(h.size());++i){if(i==pick[0]||i==pick[1])continue;const float a=area(h[size_t(pick[0])].p,h[size_t(pick[1])].p,h[size_t(i)].p);if(a>best){best=a;pick[2]=i;}}
 if(pick[2]>=0){best=-1;const float tri=area(h[size_t(pick[0])].p,h[size_t(pick[1])].p,h[size_t(pick[2])].p);
  for(int i=0;i<int(h.size());++i){if(i==pick[0]||i==pick[1]||i==pick[2])continue;const V3 p=h[size_t(i)].p;
   const float a=area(h[size_t(pick[0])].p,h[size_t(pick[1])].p,p)+area(h[size_t(pick[1])].p,h[size_t(pick[2])].p,p)+area(h[size_t(pick[2])].p,h[size_t(pick[0])].p,p)-tri;if(a>best){best=a;pick[3]=i;}}}
 std::vector<Hit> out;for(int i:pick)if(i>=0)out.push_back(h[size_t(i)]);h.swap(out);return int(h.size());}
}
void Game::simulateRigidProps(std::vector<Clutter>& list,int level,float dt,int held){
 using namespace vectra;
 auto& rp=m_rigidProps[level];
 if(level==m_level)updateBodyKnocks(dt);   // the men and creatures props knocked, stumbling on (once a frame)
 if(level==m_level){m_rigidStats.steps=0;m_rigidStats.ccdSweeps=0;m_rigidStats.ms=m_rigidStats.worldMs=m_rigidStats.pairMs=m_rigidStats.solveMs=m_rigidStats.buildMs=m_rigidStats.preMs=m_rigidStats.postMs=m_rigidStats.ccdMs=0;}
 const Vec2 shift=chunkOffset(level)-chunkOffset(m_level);
 // A point in this list's coordinates, in the world that holds it (p becomes that world's own coordinates).
 auto physicalWorld=[&](Vec2& p)->const World&{auto local=p-shift;if(local.x>=0&&local.x<24&&local.y>=0&&local.y<24){p=local;return level==m_level?m_world:m_chunks[level].world;}return worldAt(p);};
 auto inPlay=[&](int i){const auto&c=list[size_t(i)];return c.model>=0&&!c.broken&&!c.riding&&!dormantEntity(c.z);};
 bool any=false;for(int i=0;i<int(list.size());++i)if(inPlay(i)&&(!list[size_t(i)].sleeping||i==held)){any=true;break;}
 // A sleeping prop the moving player walks into is stepped too (his body in the solver wakes it).
 if(!any&&level==m_level&&lengthSq(m_velocity)>1e-4f)for(int i=0;i<int(list.size())&&!any;++i){if(!inPlay(i))continue;const auto&c=list[size_t(i)];const auto e=c.extent();
  any=std::fabs(c.pos.x-m_player.pos.x)<e[0]+.35f&&std::fabs(c.pos.y-m_player.pos.y)<e[1]+.35f&&c.z<m_player.z+m_player.hullHeight()&&c.z+c.height()>m_player.z;}
 // A motor at work on a joint keeps the list stepping (a winch reels in whatever sleeps on it).
 if(!any){const auto&jw=chunkWorld(level);for(size_t k=0;k<jw.joints().size()&&k<jw.jointStates().size()&&!any;++k){const auto&j=jw.joints()[k];const auto&st=jw.jointStates()[k];
   any=j.motor==1&&!st.broken&&st.motorOn&&st.speed!=0&&(!j.motorState||state(j.motorState))&&!(j.kind==PropJointKind::Winch&&(st.speed>0?st.length>=0&&st.length<=(j.limited?j.lower:.2f)+1e-3f:st.length>=(j.limited?j.upper:1e9f)-1e-3f));}}
 // The player stepping onto a jointed prop (a lift, a seesaw, a bridge) wakes it under his weight.
 if(!any&&level==m_level&&!m_noclip&&!m_inspectionCamera&&std::fabs(jointedPropTop(m_player.pos,m_player.z+.1f,.2f)-m_player.z)<.08f)any=true;
 // A VR hand reaching into or near a prop steps the list (its body pushes what it meets: VrHands.cpp).
 if(!any&&level==m_level&&vrHandsNear(list))any=true;
 rp.accumulator+=dt;if(!any){rp.accumulator=std::min(rp.accumulator,RigidStep);return;}
 int steps=0;while(rp.accumulator>=RigidStep-1e-6f&&steps<RigidMaxSteps){rp.accumulator-=RigidStep;++steps;}
 if(steps==RigidMaxSteps)rp.accumulator=std::min(rp.accumulator,RigidStep);
 if(!steps)return;
 const auto started=std::chrono::steady_clock::now();
 Space space;space.settings.substeps=int(tune("props.substeps",4.f));space.settings.contactHertz=tune("props.contact_hertz",space.settings.contactHertz);space.settings.linearSlop=tune("props.slop",space.settings.linearSlop);space.settings.sleepTime=tune("props.sleep_time",space.settings.sleepTime);space.settings.contactDampingRatio=tune("props.contact_damping",space.settings.contactDampingRatio);space.settings.pushout=tune("props.pushout",space.settings.pushout);space.settings.worldDampingRatio=tune("props.world_damping",space.settings.worldDampingRatio);space.settings.sleepLinear=tune("props.sleep_linear",space.settings.sleepLinear);space.settings.sleepAngular=tune("props.sleep_angular",space.settings.sleepAngular);space.settings.relaxIterations=int(tune("props.relax",1.f));space.settings.middleThreshold=tune("props.middle",1.f);space.settings.coldIterations=int(tune("props.cold",8.f));space.settings.shockNormal=tune("props.shock",space.settings.shockNormal);space.settings.ccdProbes=int(tune("props.ccd_probes",float(space.settings.ccdProbes)));space.settings.ccdBudget=int(tune("props.ccd_budget",float(space.settings.ccdBudget)));
 space.cache.swap(rp.cache);
 std::vector<V3> samples;std::vector<char> curved;std::vector<int> partOf;std::vector<Hit> hits;std::array<V3,8> feature{};
 struct WetCell {V3 r;float area;};std::vector<WetCell> wetCells;   // a body's cells under water this step (its arm to each, wet face area)
 if(rp.flat.size()<list.size())rp.flat.resize(list.size());
 // How every door of a world stands (the flat-floor memo is good while it is unchanged).
 std::vector<std::pair<const World*,float>> doorStates;
 auto doorSignature=[&](const World& w){for(const auto&[k,v]:doorStates)if(k==&w)return v;float sum=0;int i=0;for(const auto&d:w.doors()){sum+=(d.open+(d.opening?2.f:0.f))*float(1+(i++%7));}doorStates.push_back({&w,sum});return sum;};
 // Each world's ramps (ShapeRamp, not solid: the slope its step proxies stand for), found once a frame.
 std::vector<std::pair<const World*,std::vector<int>>> ramps;
 auto rampsOf=[&](const World& w)->const std::vector<int>&{for(const auto&[k,v]:ramps)if(k==&w)return v;std::vector<int> found;
  for(int i=0;i<int(w.structures().size());++i){const auto&st=w.structures()[size_t(i)];if(st.shape==ShapeRamp&&!st.solid)found.push_back(i);}ramps.push_back({&w,found});return ramps.back().second;};
 // Internal edges: a ramp's step proxies (solid boxes standing for its slope, each riser up to half a step over it) are
 // not the ramp. A prop meets the true slope; the risers' faces and corners would be edges inside the slope's one face,
 // and a crate slid onto the ramp's foot caught on the first and hopped (45 mm, 0.9 m/s up past the slope's own speed).
 // The true slope under (x, y) when it is over a ramp of w: its height and normal.
 auto rampAt=[&](const World& w,float x,float y,float& height,V3& normal)->bool{for(int r:rampsOf(w)){const auto&st=w.structures()[size_t(r)];if(x<st.x1||x>st.x2||y<st.y1||y>st.y2)continue;
   const int f=st.facing%4;const float run=f==0||f==2?st.y2-st.y1:st.x2-st.x1;if(run<=1e-3f)continue;
   const float t=f==0?(y-st.y1)/run:f==1?(x-st.x1)/run:f==2?(st.y2-y)/run:(st.x2-x)/run;const float k=(st.top-st.bottom)/run;
   const Vec2 up=f==0?Vec2{0,1}:f==1?Vec2{1,0}:f==2?Vec2{0,-1}:Vec2{-1,0};height=st.bottom+(st.top-st.bottom)*std::clamp(t,0.f,1.f);normal=norm(V3{-k*up.x,-k*up.y,1});return true;}
  return false;};
 // A structure that is one of a ramp's step proxies (CollisionProxyMaterial, within a ramp's footprint and under its top).
 auto rampProxy=[&](const World& w,const Structure& st){if(st.material!=CollisionProxyMaterial||st.shape==ShapeRamp)return false;
  for(int r:rampsOf(w)){const auto&rp=w.structures()[size_t(r)];if(st.x1>=rp.x1-1e-3f&&st.x2<=rp.x2+1e-3f&&st.y1>=rp.y1-1e-3f&&st.y2<=rp.y2+1e-3f&&st.top<=rp.top+1e-3f)return true;}return false;};
 // Solid at (x, y) from base up `height`, a ramp's proxies taken as its true slope (what they report up to half a step over
 // it is the slope's, met by the ramp's own contacts).
 auto solidAt=[&](const World& w,float x,float y,float base,float height)->bool{if(w.fits(x,y,base,height,true,true)&&!w.doorBlocks(x,y,base,height))return false;
  float slope=0;V3 n;if(!rampAt(w,x,y,slope,n)||base<slope-.02f)return true;const float above=std::max(base,slope+PrimRampStep*.55f);
  return !w.fits(x,y,above,height,true,true)||w.doorBlocks(x,y,above,height);};
 const float spacing=tune("props.sample_spacing",.12f);
 // The world's contacts hold a body within half a millimetre of its surfaces, not the solver's few millimetres
 // of slop: the World's own column tests (fits) are exact, and props were always kept out of walls by them.
 const float worldSlop=space.settings.linearSlop-.0005f;
 // ---- The world round a body ----
 space.worldContacts=[&](Space& sp,int bi,float margin,std::vector<Manifold>& out){
  const Body& b=sp.bodies[size_t(bi)];if(b.user<0)return;samples.clear();curved.clear();partOf.clear();hits.clear();
  Vec2 cq{b.x.x,b.x.y};cq=cq+shift;const World& cw=physicalWorld(cq);
  // The exact lowest points along the floor's normal (and a ramp's, under the body), and every part's
  // surface points. A curved part's sampled points meet only walls and ceilings: on a floor they would
  // be a polygon's corners, and a barrel would roll over them as if it were one.
  std::vector<V3> downs{V3{0,0,-1}};
  {const Vec2 k=cq-(Vec2{b.x.x,b.x.y}+shift);for(int r:rampsOf(cw)){const auto&st=cw.structures()[size_t(r)];
    if(b.boundsHi.x+shift.x+k.x<st.x1||b.boundsLo.x+shift.x+k.x>st.x2||b.boundsHi.y+shift.y+k.y<st.y1||b.boundsLo.y+shift.y+k.y>st.y2)continue;
    const int f=st.facing%4;const float run=f==0||f==2?st.y2-st.y1:st.x2-st.x1;if(run<=1e-3f)continue;const float g=(st.top-st.bottom)/run;
    const Vec2 up=f==0?Vec2{0,1}:f==1?Vec2{1,0}:f==2?Vec2{0,-1}:Vec2{-1,0};downs.push_back(neg(norm(V3{-g*up.x,-g*up.y,1})));}}
  const bool ceilingNear=cw.clearanceAbove(cq.x,cq.y,b.boundsLo.z,true)<b.boundsHi.z+margin+.3f;
  // The usual case, asked once: one flat floor under the whole footprint (no ramp) and every column round
  // and through it open from that floor to over the body. Then each point needs only that floor's height.
  bool flat=downs.size()==1;float flatFloor=0;const World* flatWorld=nullptr;
  if(flat){const float lo=b.boundsLo.z,hi=b.boundsHi.z;const float reachXY=margin-sp.settings.speculative+.02f,x0=b.boundsLo.x-reachXY,x1=b.boundsHi.x+reachXY,y0=b.boundsLo.y-reachXY,y1=b.boundsHi.y+reachXY;
   // Asked before for a region round this one (and no door has moved since): the same answer. A prop at rest
   // or sliding about keeps its answer; it is asked again each 4 cm it goes, or 2 cm it falls.
   Vec2 mq{(x0+x1)*.5f,(y0+y1)*.5f};mq=mq+shift;const World& cw2=physicalWorld(mq);const float doors=doorSignature(cw2);
   auto& memo=rp.flat[size_t(b.user)];
   if(memo.valid&&memo.world==&cw2&&memo.doors==doors&&x0>=memo.x0&&x1<=memo.x1&&y0>=memo.y0&&y1<=memo.y1&&std::fabs(lo-memo.lo)<.02f&&hi+margin<=memo.hi){flat=memo.flat;flatFloor=memo.floor;flatWorld=&cw2;}
   else{const float grow=.04f,X0=x0-grow,X1=x1+grow,Y0=y0-grow,Y1=y1+grow,top=hi+margin+.06f;
    const int nx=std::max(1,int(std::ceil((X1-X0)/.04f))),ny=std::max(1,int(std::ceil((Y1-Y0)/.04f)));   // every 4 cm round it, and every 10 cm across it: a fixture's corner poking into the region is found
    auto column=[&](float x,float y,bool first)->bool{Vec2 q{x,y};q=q+shift;const World& w=physicalWorld(q);if(first)flatWorld=&w;else if(&w!=flatWorld)return false;
     const float fs=w.supportBelow(q.x,q.y,lo+.06f,true);if(first)flatFloor=fs;else if(std::fabs(fs-flatFloor)>1e-4f)return false;
     return w.fits(q.x,q.y,flatFloor+.004f,std::max(.01f,top-flatFloor-.004f),true,true)&&!w.doorBlocks(q.x,q.y,flatFloor+.004f,std::max(.01f,top-flatFloor-.004f));};
    flat=column((X0+X1)*.5f,(Y0+Y1)*.5f,true);
    for(int i=0;i<=nx&&flat;++i){const float x=X0+(X1-X0)*float(i)/float(nx);flat=column(x,Y0,false)&&column(x,Y1,false);}
    for(int j=1;j<ny&&flat;++j){const float y=Y0+(Y1-Y0)*float(j)/float(ny);flat=column(X0,y,false)&&column(X1,y,false);}
    {const int ix=std::max(1,int(std::ceil((X1-X0)/.1f))),iy=std::max(1,int(std::ceil((Y1-Y0)/.1f)));for(int i=1;i<ix&&flat;++i)for(int j=1;j<iy&&flat;++j)flat=column(X0+(X1-X0)*float(i)/float(ix),Y0+(Y1-Y0)*float(j)/float(iy),false);}
    memo={&cw2,X0,Y0,X1,Y1,lo,top,flatFloor,doors,flat,flatWorld==&cw2};}}
  // On one flat floor with nothing over it, only the lowest points can touch anything (every column round is
  // open): those alone. Otherwise every part's surface points too, for the walls and what is overhead.
  const bool lowestOnly=flat&&!ceilingNear;
  for(int p=0;p<b.shape->count;++p){const PartKind kind=b.shape->parts[size_t(p)].kind;const bool round=kind==PartKind::Cylinder||kind==PartKind::Sphere||kind==PartKind::Capsule;   // (a hull is flat-faced: any of its points stands)
   // (A box's eight corners even so: the next to come down as it settles is a contact before it arrives. A hull's lowest
   // feature alone, as a round part's: its many corners and face middles on the face it rests on are all equally deep,
   // and the four kept would change from step to step.)
   if(!lowestOnly||(!round&&kind!=PartKind::Hull)){surfaceSamples(b,p,lowestOnly?100.f:spacing,samples);curved.resize(samples.size(),round?1:0);}
   for(V3 d:downs){const int n=supportFeature(b,p,d,feature);for(int k=0;k<n;++k){samples.push_back(feature[size_t(k)]);curved.push_back(0);}}
   if(lowestOnly){partOf.resize(samples.size(),p);continue;}
   for(V3 d:{V3{1,0,0},V3{-1,0,0},V3{0,1,0},V3{0,-1,0},V3{0,0,1}}){const int n=supportFeature(b,p,d,feature);for(int k=0;k<n;++k){samples.push_back(feature[size_t(k)]);curved.push_back(round?1:0);}}
   partOf.resize(samples.size(),p);}
  for(size_t si=0;si<samples.size();++si){const V3 p=samples[si];const bool round=curved[si]!=0;
   if(flat){Vec2 q{p.x,p.y};q=q+shift;const World& w=physicalWorld(q);const float s=flatFloor-softSinkAt(w.level(),q,flatFloor);
    if(p.z-s<margin&&!round)hits.push_back({p,{0,0,1},p.z-s,0,partOf[si]});
    if(ceilingNear&&p.z>b.x.z){const float cl=w.clearanceAbove(q.x,q.y,p.z-.05f,true);if(cl-p.z<margin)hits.push_back({p,{0,0,-1},cl-p.z,1});}
    continue;}
   Vec2 q{p.x,p.y};q=q+shift;const World& w=physicalWorld(q);const Vec2 k=q-(Vec2{p.x,p.y}+shift); // world-local = list + shift + k
   // A ramp collides in the World as 10 cm steps; a body meets its true slope (and not those steps).
   {bool onRamp=false;for(int r:rampsOf(w)){const auto&st=w.structures()[size_t(r)];if(q.x<st.x1||q.x>st.x2||q.y<st.y1||q.y>st.y2||p.z>st.top+.1f||p.z<st.bottom-.3f)continue;
     const int f=st.facing%4;const float run=f==0||f==2?st.y2-st.y1:st.x2-st.x1;if(run<=1e-3f)continue;
     const float t=f==0?(q.y-st.y1)/run:f==1?(q.x-st.x1)/run:f==2?(st.y2-q.y)/run:(st.x2-q.x)/run;const float k=(st.top-st.bottom)/run,slope=st.bottom+(st.top-st.bottom)*std::clamp(t,0.f,1.f);
     if(p.z>slope+.07f||p.z<slope-.3f)continue;onRamp=true;
     const Vec2 up=f==0?Vec2{0,1}:f==1?Vec2{1,0}:f==2?Vec2{0,-1}:Vec2{-1,0};const V3 n=norm(V3{-k*up.x,-k*up.y,1});
     if(p.z-slope<margin&&!round)hits.push_back({p,n,(p.z-slope)*n.z,3});break;}
    if(onRamp)continue;}
   float s=w.supportBelow(q.x,q.y,p.z+.06f,true);s-=softSinkAt(w.level(),q,s);
   // Only its underside stands on things: a point up its side that has gone a little into a fixture's top edge
   // (a tall crate shoved against a counter) meets the fixture's side, as a wall, and does not lift it onto it.
   // (A point well under what the World says supports it (6 cm) is inside that thing from its side: a wall.)
   const bool under=p.z<b.x.z+.01f&&p.z-s>-.06f;
   if(p.z-s<margin&&!round&&under)hits.push_back({p,{0,0,1},p.z-s,0,partOf[si]});
   // Every point is asked about walls too, from just over what it stands on: a thin thing on the floor
   // (a keyboard, a bottle on its side) is all floor points, and would otherwise roll through a wall.
   const float base=under?std::max(p.z-.005f,s+.004f):p.z-.005f;
   if(!w.fits(q.x,q.y,base,.01f,true,true)||w.doorBlocks(q.x,q.y,base,.01f)){
    // Inside a wall, a fixture or a door: its face from a ray out of the body's middle.
    const V3 from{b.x.x+shift.x+k.x,b.x.y+shift.y+k.y,under?std::max(b.x.z,s+.01f):b.x.z},to{q.x,q.y,under?std::max(p.z,s+.006f):p.z};V3 n;const float t=w.shotFraction(from,to,n);
    if(t<1&&dot(n,to-from)<0){const V3 at=from+(to-from)*t;hits.push_back({p,n,std::min(0.f,dot(to-at,n)),2});continue;}
    // The ray found no face (it began inside): the nearest open column round the point.
    bool found=false;for(float r:{.03f,.07f,.12f,.2f}){for(int a=0;a<8&&!found;++a){const float ang=kPi*.25f*float(a);const float x=q.x+std::cos(ang)*r,y=q.y+std::sin(ang)*r;
      if(w.fits(x,y,base,.01f,true,true)&&!w.doorBlocks(x,y,base,.01f)){hits.push_back({p,{std::cos(ang),std::sin(ang),0},-r,2});found=true;}}if(found)break;}
    continue;}
   if(ceilingNear&&p.z>b.x.z){const float cl=w.clearanceAbove(q.x,q.y,p.z-.05f,true);if(cl-p.z<margin)hits.push_back({p,{0,0,-1},cl-p.z,1});}
   // Clear now but in a solid by the end of the step at its own speed: a contact now, at the face the way meets
   // (speculative, for the walls as for the floor), so a fast thing stops at a wall rather than in it.
   {const V3 vp=b.v+cross(b.w,p-b.x);const V3 move=vp*RigidStep*1.5f;const float horiz=std::sqrt(move.x*move.x+move.y*move.y);
    if(horiz>.004f){const float tx=q.x+move.x,ty=q.y+move.y;const float fb=std::max(p.z+move.z-.005f,s+.004f);
     // Onto a ramp: its slope's face, met ahead as a speculative contact (the slope's plane carried out to the point), not
     // its proxies' risers.
     {float slope=0;V3 rn;if(!round&&rampAt(w,tx,ty,slope,rn)&&p.z<slope+.07f){const float planeZ=slope+(rn.x*(tx-q.x)+rn.y*(ty-q.y))/rn.z;if(p.z>=planeZ-.02f)hits.push_back({p,rn,std::max(0.f,(p.z-planeZ)*rn.z),3});}}   // (from its foot: beside it, its side is a wall)
     if(solidAt(w,tx,ty,fb,.01f)){const V3 from{q.x,q.y,std::max(p.z,s+.006f)},to{tx,ty,std::max(p.z,s+.006f)};V3 n;const float t=w.shotFraction(from,to,n);
      if(t<1&&dot(n,to-from)<0)hits.push_back({p,n,std::max(0.f,dot(from-(from+(to-from)*t),n)),2});
      // The ray met no face (a fixture a ray passes through, a corner between its steps): the way it is going is
      // blocked, so a contact now against that way, at no gap, stops it at the face rather than in it.
      else{const float l=horiz;hits.push_back({p,V3{-move.x/l,-move.y/l,0},0.f,2});}}}}
  }
  // The corners of the boxes round it (fixtures, structures) against its own shape: the world is asked only at
  // points over the body, so a counter's corner pushed into a crate's face between two of them went unseen.
  if(!flat){const Vec2 kk=cq-(Vec2{b.x.x,b.x.y}+shift);   // world-local = list + shift + kk
   const float lx0=b.boundsLo.x+shift.x+kk.x-margin,lx1=b.boundsHi.x+shift.x+kk.x+margin,ly0=b.boundsLo.y+shift.y+kk.y-margin,ly1=b.boundsHi.y+shift.y+kk.y+margin,lz0=b.boundsLo.z,lz1=b.boundsHi.z;
   auto corner=[&](float wx,float wy,float bottom,float top){if(wx<lx0||wx>lx1||wy<ly0||wy>ly1)return;const float z0=std::max(bottom,lz0),z1=std::min(top,lz1);if(z1<z0)return;
    for(float wz:{z0+.01f,(z0+z1)*.5f,z1-.01f}){const V3 p{wx-shift.x-kk.x,wy-shift.y-kk.y,wz};
     for(int part=0;part<b.shape->count;++part){V3 surface,normal;float d;nearestPoint(b,part,p,surface,normal,d);if(d<margin)hits.push_back({p,neg(normal),d,2,part});}}};
   for(const auto&f:cw.fixtures()){if(!f.solid||f.core<=0)continue;const float hx=f.width*f.core*.5f,hy=f.depth*f.core*.5f,r=std::sqrt(hx*hx+hy*hy);
    if(f.position.x+r<lx0||f.position.x-r>lx1||f.position.y+r<ly0||f.position.y-r>ly1)continue;const float bottom=cw.floorHeight(f.position.x,f.position.y)+f.base,top=bottom+f.height;if(top<lz0||bottom>lz1)continue;
    const float c=std::cos(f.yaw),s=std::sin(f.yaw);const Vec2 ax{c,-s},ay{s,c};for(int i=0;i<4;++i){const Vec2 q=f.position+ax*(i&1?hx:-hx)+ay*(i&2?hy:-hy);corner(q.x,q.y,bottom,top);}}
   for(const auto&st:cw.structures()){if(!st.solid||st.shape==ShapeRamp||rampProxy(cw,st))continue;if(st.x2<lx0||st.x1>lx1||st.y2<ly0||st.y1>ly1||st.top<lz0||st.bottom>lz1)continue;
    corner(st.x1,st.y1,st.bottom,st.top);corner(st.x2,st.y1,st.bottom,st.top);corner(st.x1,st.y2,st.bottom,st.top);corner(st.x2,st.y2,st.bottom,st.top);}}
  if(hits.empty())return;
  // Group: the floor, the ceiling, the walls by the way they face (eight ways round).
  // (The floor under each part of a compound is its own manifold: a chair on its back rests on its back and its
  // seat's edge at once, not on whichever four of their corners were picked this step, rocking between them.)
  auto groupOf=[](const Hit& h){if(h.kind==0)return h.part==0?0:40+h.part;if(h.kind<2)return h.kind;if(h.kind==3)return 30;const float a=std::atan2(h.n.y,h.n.x);return 2+(int(std::lround(a/(kPi*.25f)))+8)%8+(h.n.z>.5f?8:h.n.z<-.5f?16:0);};
  std::vector<int> groups;for(const auto&h:hits){const int g=groupOf(h);if(std::find(groups.begin(),groups.end(),g)==groups.end())groups.push_back(g);}
  std::sort(groups.begin(),groups.end());
  std::vector<Hit> group;
  for(int g:groups){group.clear();V3 n{};for(const auto&h:hits)if(groupOf(h)==g){group.push_back(h);n=n+h.n;}n=norm(n);
   // Only what is near touching: a point well above the deepest (a box's far edge as it tips) would crowd
   // a real contact out of the four (a barrel left on one rim).
   {float deepest=1e30f;for(const auto&h:group)deepest=std::min(deepest,h.sep);std::erase_if(group,[&](const Hit& h){return h.sep>deepest+.012f;});}
   keepFour(group,n);
   if((g==0||g>=40)&&!flat){ // the floor's slope at the points kept (a ramp, terrain), by differences of the support
    V3 sum{};for(auto&h:group){Vec2 q{h.p.x,h.p.y};q=q+shift;const World& w=physicalWorld(q);const float s=h.p.z-h.sep,d=.04f;
     // (Over a ramp, its true slope, not its step proxies' tops.)
     auto at=[&](float x,float y){float v=w.supportBelow(x,y,s+.06f,true);float slope=0;V3 rn;if(rampAt(w,x,y,slope,rn)&&std::fabs(v-slope)<PrimRampStep*.6f)v=slope;return std::fabs(v-s)<.06f?v:s;};
     // Across a crease (a floor meeting a ramp's foot, a ramp's crest meeting its landing) the two sides' slopes differ, and
     // their average is an edge's normal no face has: it threw a bottle back off a ramp's foot. The flatter side's, the
     // face the point stands on, is kept (internal edges filtered as on a mesh); on a smooth slope both sides agree.
     auto grade=[&](float minus,float here,float plus){const float back=(here-minus)/d,ahead=(plus-here)/d;return std::fabs(ahead-back)>.1f?(std::fabs(ahead)<std::fabs(back)?ahead:back):(plus-minus)/(2*d);};
     const float here=at(q.x,q.y),gx=grade(at(q.x-d,q.y),here,at(q.x+d,q.y)),gy=grade(at(q.x,q.y-d),here,at(q.x,q.y+d));h.n=norm(V3{-gx,-gy,1});sum=sum+h.n;}
    n=norm(sum);for(auto&h:group)h.sep*=n.z;}
   Manifold m;m.a=bi;m.b=-1;m.partA=0;m.partB=g;m.normal=neg(n);m.material=Material::mix(b.material,Ground);m.material.restitution=b.material.restitution;m.count=0;
   // A floor gives its own material (Space::prepare mixes it with the prop's): under the points' middle.
   if(g==0||g>=40||g==30){V3 mid{};for(const auto&h:group)mid=mid+h.p;mid=mid*(1.f/float(group.size()));Vec2 q{mid.x,mid.y};q=q+shift;const World& w=physicalWorld(q);float sep=0;for(const auto&h:group)sep+=h.sep;m.surface.grip(floorGrip(w,q.x,q.y,mid.z-sep/float(group.size())));}
   for(const auto&h:group){auto&c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=h.p;c.separation=h.sep-worldSlop;c.localA=b.q.unrotate(h.p-b.x);c.localB=h.p;}
   out.push_back(m);}
 };
 space.filter=[&](const Space& sp,int a,int b){const Body& A=sp.bodies[size_t(a)];const Body& B=sp.bodies[size_t(b)];if(A.user<0&&B.user<0)return false;
  // A VR hand meets props it can move (under 12 kg, loose); it stops at the rest (VrHands.cpp).
  if(A.user==VrHandBody||A.user==VrHandBody-1)return B.user>=0&&vrHandMeets(B);if(B.user==VrHandBody||B.user==VrHandBody-1)return A.user>=0&&vrHandMeets(A);
  // (The prop in the hand meets men by the carry's own sweep, pushBodiesFromCarried: it neither hurts nor knocks them.)
  if(A.user<=ActorBody||B.user<=ActorBody)return !(A.kinematic||B.kinematic);
  if(A.user==PlayerBody||B.user==PlayerBody){const Body& prop=A.user==PlayerBody?B:A;const Body& player=A.user==PlayerBody?A:B;(void)player;
   // Light props only: one of 8 kg and up stops him (Movement) and is shoved by the old rule's slow push.
   return prop.kinematic||prop.invMass<1/8.f?false:len(prop.v)<2.5f;}return true;};
 space.sweep=[&](Space&,int,V3 from,V3 to,V3& normal)->float{Vec2 q{from.x,from.y};q=q+shift;const World& w=physicalWorld(q);const Vec2 k=q-(Vec2{from.x,from.y}+shift);
  const V3 a{q.x,q.y,from.z},bb{to.x+shift.x+k.x,to.y+shift.y+k.y,to.z};
  // A ramp's step proxies are not met: a riser or a step's top over the true slope is passed, and the slope's own plane
  // met instead (a thrown bottle swept onto a ramp's foot bounced back off its first riser).
  float begin=0;V3 from2=a;
  for(int tries=0;tries<8;++tries){const float t=w.shotFraction(from2,bb,normal);if(t>=1)return 1.f;const V3 hit=from2+(bb-from2)*t;const float at=begin+(1-begin)*t;
   const V3 dir=norm(bb-a);float slope=0;V3 rn;
   if(!rampAt(w,hit.x+dir.x*.002f,hit.y+dir.y*.002f,slope,rn)||hit.z<slope-.01f)return at;
   // The slope's plane along the rest of the way, from where it entered the ramp.
   {const V3 d=bb-a;const float den=dot(d,rn);if(den<-1e-6f){const float h0=dot(a-V3{hit.x,hit.y,slope},rn),ts=-h0/den;
     if(ts>=0&&ts<=1){const V3 p=a+d*ts;float s2=0;V3 n2;if(rampAt(w,p.x,p.y,s2,n2)&&std::fabs(p.z-s2)<.02f){normal=rn;return ts;}}}}
   begin=std::min(1.f,at+.004f/std::max(len(bb-a),1e-6f));from2=hit+dir*.004f;if(begin>=1)return 1.f;}
  return 1.f;};
 // ---- A prop's own reactions (as the old step had them) ----
 // A blow's sound (and hearing) goes by the contact's own approach; breaking goes by the approach of the prop's
 // middle (`breaking`, the speed the materials' break speeds were set on: a tumbling bottle's tip is faster).
 auto impact=[&](int index,float speed,float breaking=-1){auto&c=list[size_t(index)];if(speed<.65f||c.broken)return;if(breaking<0)breaking=speed;
  const auto&t=propMaterialTraits(c.material());
  // (The cooldown keeps one blow's sound from repeating; it never spares a prop the blow that breaks it: a crate's
  // corner touching a mug a step before its weight lands on it.)
  if(c.impactCooldown>0){if(t.breakSpeed>0&&breaking>t.breakSpeed)breakClutter(level,size_t(index),{c.velocity.x,c.velocity.y,c.vz});return;}SoundEvent event{c.impactSound(),c.pos+shift,std::clamp(speed*.12f,.12f,.85f)*t.impactGain,t.impactPitch,true,17,c.z+c.height()*.5f};
  event.owner=c.soundOwner;event.aiRadius=std::clamp(2.f+std::sqrt(c.mass())*speed,2.f,12.f);
  if(t.breakSpeed>0&&breaking>t.breakSpeed)breakClutter(level,size_t(index),{c.velocity.x,c.velocity.y,c.vz});m_sounds.push_back(event);c.impactCooldown=.075f;};
 // A pane struck with `energy` (J): cracked, or broken (the share of its speed the prop keeps going on).
 auto strike=[&](int index,int paneLevel,int pane,float energy,Point3 at,Point3 dir)->float{auto&c=list[size_t(index)];auto&pw=paneLevel==m_level?m_world:m_chunks[paneLevel].world;if(!pw.breakableIntact(pane))return 1;
  const bool fresh=c.paneRearm<=0;c.paneRearm=PaneRearm;if(!fresh)return 0;const auto b=pw.breakables()[size_t(pane)];
  const float limit=paneLimit(b),left=b.material==17?paneResistance(paneLevel,pane,at):std::max(0.f,limit-pw.breakableDamage(pane)),damage=limit*std::max(0.f,energy-PaneTap)/paneBreakEnergy(b);
  const auto e=c.extent(),s=c.size();const float hole=std::min(1.f,std::min(std::sqrt(std::max(e[0],e[1])*std::max(e[0],e[1])+e[2]*e[2]),.5f*std::sqrt(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]))*1.1f+.04f);
  if(damage<=0||!damagePane(paneLevel,pane,damage,at,dir,{PaneBlow::Prop,hole}))return 0;c.paneRearm=0;return std::sqrt(std::clamp(1-(PaneTap+paneBreakEnergy(b)*left/limit)/energy,.1f,.9f));};
 // Bodies (the player, people, creatures) against a moving prop: struck, pushed along, stood on.
 auto bodyContacts=[&](int index){auto&c=list[size_t(index)];if(c.model<0||c.broken||c.sleeping)return;const auto e=c.extent();const float propHeight=e[2]*2,mass=c.mass();
  Vec2 ax{std::cos(c.yaw),std::sin(c.yaw)},ay{-ax.y,ax.x};float halfX,halfY;{auto local=c;local.yaw=0;const auto h=local.extent();halfX=h[0];halfY=h[1];}
  auto overlap=[&](Vec2 delta,float radius,Vec2& normal,float& penetration){penetration=1e30f;
   for(Vec2 axis:std::array<Vec2,4>{Vec2{1,0},Vec2{0,1},ax,ay}){const float along=dot(delta,axis);const float r=std::fabs(dot(ax,axis))*halfX+std::fabs(dot(ay,axis))*halfY+radius*(std::fabs(axis.x)+std::fabs(axis.y));
    const float depth=r-std::fabs(along);if(depth<=0)return false;if(depth<penetration){penetration=depth;normal=axis*(along<0?-1.f:1.f);}}return true;};
  AiTargetRef attacker;if(c.soundOwner>=0&&size_t(c.soundOwner)<m_friendlyActors.size())attacker={ShotKind::Npc,m_friendlyActors[size_t(c.soundOwner)].level,c.soundOwner};else if(c.projectile)attacker={ShotKind::Player,m_level,0};
  auto nearby=[&](Vec2 actor,float feet,float height,float radius){return std::fabs(actor.x-c.pos.x)<e[0]+radius&&std::fabs(actor.y-c.pos.y)<e[1]+radius&&c.z+std::min(0.f,c.vz*RigidStep)<feet+height&&c.z+propHeight-softGive(c)>feet;};
  auto sweepFits=[&](Vec2 from,Vec2 to){const float d=length(to-from);const int n=std::max(1,int(std::ceil(d/.08f)));for(int k=1;k<=n;++k){const Vec2 p=from+(to-from)*(float(k)/float(n));
   if(!c.everyOutlinePoint([&](Vec2 o){auto q=p+o+shift;const auto&w=physicalWorld(q);return w.fits(q.x,q.y,c.z+.02f,std::max(.01f,propHeight-.04f),true,true)&&!w.doorBlocks(q.x,q.y,c.z+.02f,std::max(.01f,propHeight-.04f));}))return false;}return true;};
  auto contact=[&](Vec2 actor,float feet,float height,float radius,float actorMass,Vec2 actorVelocity,auto&& move,auto&& hurt,auto&& reel){if(!nearby(actor,feet,height,radius))return;Vec2 n{};float penetration;
   if(!overlap(actor-c.pos,radius,n,penetration))return;
   // Closing only as far as the prop itself comes at the body (a body walking into a prop being pushed away
   // from it is not struck by it).
   float closing=std::min(std::max(0.f,dot(c.velocity-actorVelocity,n)),std::max(0.f,dot(c.velocity,n)));const bool falling=c.vz<0&&c.z>=feet+height-.12f;float speed=std::max(closing,falling?-c.vz:0.f);
   if(falling&&!c.everyOutlinePoint([&](Vec2 offset){auto p=c.pos+offset+shift;const auto&w=physicalWorld(p);return w.supportBelow(p.x,p.y,c.z+.025f,true)<feet+height;}))return;
   if(speed>=1.5f&&c.bodyImpactCooldown<=0){float damage=std::clamp((.5f*c.mass()*speed*speed-2.f)*.2f,0.f,80.f);if(damage>0){hurt(damage,n);c.bodyImpactCooldown=.25f;c.projectile=false;impact(index,speed);}}
   if(c.broken)return;if(falling){c.z=feet+height;c.vz=std::max(0.f,-c.vz*.12f);return;}
   // The blow's momentum shoves the body: the speed it takes from the prop (the prop's momentum over both masses),
   // carried through a quarter second of follow-through. A mug barely nudges a man; a 9 kg crate thrown at him
   // shoves him back a hand's width and staggers him; a rolling 35 kg drum shoves him a pace. A heavy prop also
   // takes its way out of him from the body (it is the one that does not give).
   float moved=0;if(closing>.001f){const float bodySpeed=1.15f*closing*mass/(mass+actorMass);auto delta=n*((mass>=8?penetration+.001f:0.f)+bodySpeed*.22f);
    if(dot(delta,n)>.002f)moved=std::max(0.f,dot(move(delta),n));if(bodySpeed>.15f)reel(bodySpeed,n);}
   // Out of the body only when the prop came into it: one the body walks into is the body's to stop (Movement),
   // or the player's own body in the solver pushes it; set out of his hull each step, a barrel would ride at a sprint.
   float left=std::max(0.f,penetration+.001f-moved);auto next=c.pos-n*left;if(left>0&&closing>.001f&&sweepFits(c.pos,next))c.pos=next;
   if(closing>.001f){float keep=(mass-.15f*actorMass)/(mass+actorMass);if(moved<penetration*.5f)keep=-.15f;c.velocity=c.velocity-n*(closing*(1-keep));c.sleeping=false;c.restTime=0;}};
  // A jointed prop he stands on (a bridge's plank, a lift) carries him; it does not strike or shove him.
  if(!onJointedTop(level,index,m_player.pos,m_player.z))contact(m_player.pos-shift,m_player.z,m_player.hullHeight(),.2f,70.f,m_velocity,[&](Vec2 delta){auto before=m_player.pos;tryMove(delta);return m_player.pos-before;},[&](float damage,Vec2){receiveDamage(damage,c.pos+shift,false,false);},[&](float dv,Vec2){m_verticalSpringVelocity-=std::min(1.f,dv*.4f);});
  // (Men and creatures are bodies in the solver (actor bodies, below): a prop meets them by contact.)
  if(false)for(size_t a=0;a<m_friendlyActors.size();++a){auto&npc=m_friendlyActors[a];const auto&track=npc.track();auto offset=chunkOffset(npc.level)-chunkOffset(level);auto point=npc.pose.position+offset;if(!nearby(point,npc.pose.z,track.scale,.2f))continue;if(npc.pose.clip==4||npcHeld(a)||!autonomousActor(track)||(track.ai.enableState&&!state(track.ai.enableState)))continue;float height=friendlyHullHeight(a,track);
   contact(point,npc.pose.z,height,.2f,70.f,Vec2{},[&](Vec2 delta){auto before=npc.pose.position;const int steps=std::max(1,int(std::ceil(length(delta)/.04f)));Vec2 reached=before;for(int i=1;i<=steps;++i){const auto next=before+delta*(float(i)/steps);if(!friendlyBodyFits(npc.level,next,npc.pose.z,height))break;reached=next;}npc.pose.position=reached;return npc.pose.position-before;},[&](float damage,Vec2){auto p=c.pos+shift;damageFriendly(a,damage,p+chunkOffset(m_level)-chunkOffset(npc.level),attacker);},[](float,Vec2){});}
  if(false)for(int target=0;target<chunkCount();++target)if(chunkResident(target)){auto offset=chunkOffset(target)-chunkOffset(level);auto&enemies=target==m_level?m_enemies:m_chunks[target].enemies;for(auto&enemy:enemies)if(enemy.alive){
   contact(enemy.pos+offset,enemy.bodyBottom(),enemy.bodyTop()-enemy.bodyBottom(),enemy.radius(),70.f*enemy.scale*enemy.scale*enemy.scale,Vec2{},[&](Vec2 delta){auto before=enemy.pos;const int steps=std::max(1,int(std::ceil(length(delta)/.04f)));Vec2 reached=before;for(int i=1;i<=steps;++i){const auto next=before+delta*(float(i)/steps);if(!hullFits(next+chunkOffset(target)-chunkOffset(m_level),enemy.z,enemy.hull(),enemy.radius(),false,false))break;reached=next;}enemy.pos=reached;return enemy.pos-before;},
    [&](float damage,Vec2){enemy.awareness=6;enemy.recognition=1;enemy.lastKnown=c.pos-offset;enemy.lastKnownZ=c.z;enemy.state=Enemy::State::Investigate;if(applyEnemyDamage(enemy,damage,false,false,target)){auto audible=enemy;audible.pos+=chunkOffset(target)-chunkOffset(m_level);enemySound(audible,2);}},
    // Staggered by a hard shove (half a metre a second taken from the prop): its blow is spoilt and it recovers.
    [&](float dv,Vec2){if(dv<.5f)return;enemy.windup=0;enemy.strike=0;enemy.attackCooldown=std::max(enemy.attackCooldown,.35f+dv*.5f);enemy.painFlash=std::max(enemy.painFlash,std::min(1.f,dv*.6f));});}}
 };
 if(level==m_level){m_rigidStats.events.clear();m_rigidStats.manifoldsSeen.clear();}m_rigidStats.worldMs=m_rigidStats.pairMs=m_rigidStats.solveMs=m_rigidStats.buildMs=m_rigidStats.preMs=m_rigidStats.postMs=0;
 using Clock=std::chrono::steady_clock;auto since=[](Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();};
 std::vector<int> bodyOf(list.size(),-1);std::vector<char> wasAsleep;std::vector<V3> before;
 // ---- Men and creatures: bodies in the solver, so a prop meets them by contact (it never passes into them)
 // and the impulse it gives them knocks them back by its momentum ----
 struct ActorRef {bool enemy=false;int level=0;size_t index=0;float mass=70,radius=.2f,feet=0,height=1.7f;Vec2 at{},velocity{};std::uint64_t key=0;Vec2 shove{},pushed{};float hurtSpeed=0;int hurtProp=-1;int body=-1;V3 start{};};
 std::vector<ActorRef> actors;std::deque<Shape> actorShapes;
 {std::vector<std::pair<std::uint64_t,Vec2>> seen;
  auto add=[&](ActorRef a){a.key=(a.enemy?1ull<<62:0ull)|(std::uint64_t(std::uint32_t(a.level))<<32)|std::uint64_t(a.index);Vec2 last=a.at;
   for(const auto&[k,p]:rp.actorLast)if(k==a.key){last=p;break;}a.velocity=dt>1e-4f?(a.at-last)*(1/dt):Vec2{};if(length(a.velocity)>6)a.velocity=normalized(a.velocity)*6.f;seen.push_back({a.key,a.at});actors.push_back(a);};
  for(size_t a=0;a<m_friendlyActors.size();++a){const auto&npc=m_friendlyActors[a];if(!chunkResident(npc.level))continue;const auto&track=npc.track();
   if(npc.pose.clip==4||npcHeld(a)||!autonomousActor(track)||(track.ai.enableState&&!state(track.ai.enableState))||(track.deadState&&state(track.deadState)))continue;
   ActorRef r;r.level=npc.level;r.index=a;r.at=npc.pose.position+chunkOffset(npc.level)-chunkOffset(level);r.feet=npc.pose.z;r.height=friendlyHullHeight(a,track);add(r);}
  for(int target=0;target<chunkCount();++target)if(chunkResident(target)){const auto&enemies=target==m_level?m_enemies:m_chunks[target].enemies;
   for(size_t e=0;e<enemies.size();++e){const auto&en=enemies[e];if(!en.alive)continue;ActorRef r;r.enemy=true;r.level=target;r.index=e;r.at=en.pos+chunkOffset(target)-chunkOffset(level);
    r.feet=en.bodyBottom();r.height=en.bodyTop()-en.bodyBottom();r.radius=en.radius();r.mass=70.f*en.scale*en.scale*en.scale;add(r);}}
  rp.actorLast.swap(seen);}
 // A creature's or a man's: hurt, staggered, knocked back.
 auto actorHurt=[&](const ActorRef& r,const Clutter& c,float damage){const Vec2 p=c.pos+shift;
  if(r.enemy){auto&enemy=(r.level==m_level?m_enemies:m_chunks[r.level].enemies)[r.index];const Vec2 offset=chunkOffset(r.level)-chunkOffset(level);
   enemy.awareness=6;enemy.recognition=1;enemy.lastKnown=c.pos-offset;enemy.lastKnownZ=c.z;enemy.state=Enemy::State::Investigate;
   if(applyEnemyDamage(enemy,damage,false,false,r.level)){auto audible=enemy;audible.pos+=chunkOffset(r.level)-chunkOffset(m_level);enemySound(audible,2);}}
  else{AiTargetRef attacker;if(c.soundOwner>=0&&size_t(c.soundOwner)<m_friendlyActors.size())attacker={ShotKind::Npc,m_friendlyActors[size_t(c.soundOwner)].level,c.soundOwner};else if(c.projectile)attacker={ShotKind::Player,m_level,0};
   damageFriendly(r.index,damage,p+chunkOffset(m_level)-chunkOffset(m_friendlyActors[r.index].level),attacker);}};
 // The prop in the hand spins as the hand turned it since last frame (a carried crate swung round carries
 // what stands on it by friction, and flings it off when jerked).
 V3 heldSpin{};
 if(held>=0&&held<int(list.size())){const Quat now=propOrientation(list[size_t(held)]);
  if(rp.heldIndex==held&&dt>1e-4f){Quat d=now*rp.heldTurn.conj();if(d.w<0)d={-d.w,-d.x,-d.y,-d.z};const float sn=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
   if(sn>1e-6f)heldSpin=V3{d.x/sn,d.y/sn,d.z/sn}*std::min(2*std::atan2(sn,d.w)/dt,20.f);}
  rp.heldIndex=held;rp.heldTurn=now;}else rp.heldIndex=-1;
 // ---- Chained props (mattresses, pads: PropBodies.h): three segments on springy hinges ----
 struct ChainRec {int prop=-1;std::array<int,3> body{};bool held=false;};std::vector<ChainRec> chains;std::deque<Hinge> hinges;std::vector<char> chainBody;
 auto setFrame=[](Clutter& c,Quat q,V3 centre){q=q.normalized();c.orient={q.w,q.x,q.y,q.z};q.toEuler(c.yaw,c.pitch,c.roll);c.orientEuler={c.yaw,c.pitch,c.roll};c.pos={centre.x,centre.y};c.z=centre.z-c.extent()[2];};
 auto buildChain=[&](int i,bool inHand){auto&c=list[size_t(i)];
  // The frame is the middle segment's, or while it is carried the end nearest the player (he holds it by that end).
  int root=1;if(inHand){root=c.boneRoot;if(root==1){const Chain ch=chainOf(c);const float along=c.size()[size_t(ch.along)]/3;const V3 ax=ch.along==0?V3{1,0,0}:V3{0,1,0};
    const V3 e0=ch.frame[0]+ch.turn[0].rotate(ax*-along),e2=ch.frame[2]+ch.turn[2].rotate(ax*along);const V3 eye{m_player.pos.x-shift.x,m_player.pos.y-shift.y,e0.z};root=lenSq(e0-eye)<=lenSq(V3{e2.x,e2.y,e0.z}-eye)?0:2;}}
  if(root!=c.boneRoot){const Chain ch=chainOf(c);c.boneRoot=root;setFrame(c,ch.turn[size_t(root)],ch.frame[size_t(root)]);}
  const Chain ch=chainOf(c);const V3 ax=ch.along==0?V3{1,0,0}:V3{0,1,0},wd=ch.along==0?V3{0,1,0}:V3{-1,0,0};const float third=ch.length/3;
  const V3 v{c.velocity.x,c.velocity.y,c.vz},w=propAngular(c),centre=ch.frame[size_t(root)];ChainRec rec;rec.prop=i;rec.held=inHand;
  for(int k=0;k<3;++k){Body b;b.shape=&segmentShape(c);b.q=ch.turn[size_t(k)];b.x=ch.frame[size_t(k)]+b.q.rotate(ax*(float(k-1)*third));
   b.material=propMaterialOf(c);b.softHertz=propSoftHertz(c);b.sleeping=c.sleeping;b.sleepTime=std::max(0.f,c.restTime);b.setMass(c.mass()/3);b.updateInertia();
   // Its velocity: the prop's, and its own turn about the joints between it and the root.
   V3 wk=w,vk=v+cross(w,b.x-centre);
   if(k!=root){const int step=k>root?1:-1;for(int j=root;j!=k;j+=step){const int joint=step>0?j:j-1;const float rate=c.bendSpeed[size_t(joint)]*float(step);
     const V3 axis=ch.turn[size_t(joint)].rotate(wd);const V3 J=ch.frame[size_t(joint)]+ch.turn[size_t(joint)].rotate(ax*((float(joint)-.5f)*third));wk=wk+axis*rate;vk=vk+cross(axis*rate,b.x-J);}}
   b.v=vk;b.w=wk;b.user=i;b.group=0x40000000u|std::uint32_t(i);b.id=k==root?i:i+(1<<17)*(k+(k<root?1:0));
   if(inHand&&k==root){b.kinematic=true;b.setMass(0);b.sleeping=false;b.w=heldSpin;b.v=carryVelocity();}
   if(inHand&&k!=root)b.sleeping=false;
   rec.body[size_t(k)]=int(space.bodies.size());if(k==root)bodyOf[size_t(i)]=int(space.bodies.size());space.bodies.push_back(b);}
  for(int k=0;k<2;++k){Hinge h;h.a=rec.body[size_t(k)];h.b=rec.body[size_t(k+1)];h.anchorA=ax*(third*.5f);h.anchorB=ax*(-third*.5f);h.axisA=wd;
   h.hertz=tune("pad.hertz",2.5f);h.damping=tune("pad.damping",.25f);h.limit=tune("pad.limit",1.5f);hinges.push_back(h);}
  chains.push_back(rec);};
 auto writeChain=[&](const ChainRec& rec){auto&c=list[size_t(rec.prop)];const auto&B=space.bodies;bool asleep=true;float rest=1e9f;
  for(int k:rec.body){asleep&=B[size_t(k)].sleeping||B[size_t(k)].kinematic;rest=std::min(rest,B[size_t(k)].sleepTime);}
  const Chain ch=chainOf(c);const V3 ax=ch.along==0?V3{1,0,0}:V3{0,1,0},wd=ch.along==0?V3{0,1,0}:V3{-1,0,0};const float third=ch.length/3;
  for(int j=0;j<2;++j){const auto&a=B[size_t(rec.body[size_t(j)])];const auto&b=B[size_t(rec.body[size_t(j+1)])];const auto&h=hinges[size_t(&rec-chains.data())*2+size_t(j)];
   c.bend[size_t(j)]=h.angle(space);c.bendSpeed[size_t(j)]=dot(b.w-a.w,a.q.rotate(wd));}
  const auto&r=B[size_t(rec.body[size_t(c.boneRoot)])];
  if(!rec.held){const V3 centre=r.x-r.q.rotate(ax*(float(c.boneRoot-1)*third));setFrame(c,r.q,centre);const V3 v=r.v+cross(r.w,centre-r.x);c.velocity={v.x,v.y};c.vz=v.z;setPropAngular(c,r.w);}
  c.sleeping=asleep&&!rec.held;c.restTime=rest;if(len(r.v)<.5f)c.projectile=false;};
 // Joints between props (PropJoints.cpp): each joint's props, so both are built whenever either is.
 const auto jointPairs=jointedProps(level,list);std::deque<Joint> propJoints;std::vector<int> propJointOf;std::vector<char> jointNeed;
 for(int step=0;step<steps;++step){
  auto mark=Clock::now();
  space.bodies.clear();std::fill(bodyOf.begin(),bodyOf.end(),-1);
  // What can be touched this step: the awake props (and the hand's, and the player's), a metre and a half
  // round. A sleeping prop out of all their reach is not built at all (500 asleep cost next to nothing).
  struct Reach {float x0,y0,x1,y1;};std::vector<Reach> reaches;
  for(int i=0;i<int(list.size());++i){if(!inPlay(i))continue;const auto&c=list[size_t(i)];if(c.sleeping&&i!=held)continue;const auto e=c.extent();const float r=1.5f+len(V3{c.velocity.x,c.velocity.y,c.vz})*RigidStep;
   reaches.push_back({c.pos.x-e[0]-r,c.pos.y-e[1]-r,c.pos.x+e[0]+r,c.pos.y+e[1]+r});}
  if(level==m_level)reaches.push_back({m_player.pos.x-1.5f,m_player.pos.y-1.5f,m_player.pos.x+1.5f,m_player.pos.y+1.5f});
  // A free VR hand reaches what it is about to touch, wherever the body stands (a sleeping prop it swats is built).
  if(level==m_level)for(int hand=0;hand<2;++hand)if(vrHandFree(hand)){const Point3 p=m_vrHandPosition[size_t(hand)];reaches.push_back({p.x-1.f,p.y-1.f,p.x+1.f,p.y+1.f});}
  auto reachable=[&](const Clutter& c){const auto e=c.extent();for(const auto&r:reaches)if(c.pos.x+e[0]>=r.x0&&c.pos.x-e[0]<=r.x1&&c.pos.y+e[1]>=r.y0&&c.pos.y-e[1]<=r.y1)return true;return false;};
  chains.clear();hinges.clear();space.constraints.clear();chainBody.assign(0,0);
  jointNeed.assign(list.size(),0);
  if(!jointPairs.empty()){auto built=[&](int i){if(i<0||!inPlay(i))return false;const auto&c=list[size_t(i)];return !c.sleeping||i==held||reachable(c)||jointNeed[size_t(i)];};
   for(bool more=true;more;){more=false;for(auto [a,b]:jointPairs)if((built(a)||built(b))&&(!jointNeed[size_t(a)]||(b>=0&&!jointNeed[size_t(b)]))){jointNeed[size_t(a)]=1;if(b>=0)jointNeed[size_t(b)]=1;more=true;}}}
  for(int i=0;i<int(list.size());++i){if(!inPlay(i))continue;const auto&c=list[size_t(i)];if(c.sleeping&&i!=held&&!reachable(c)&&!jointNeed[size_t(i)])continue;
   if(chained(c)){buildChain(i,i==held);continue;}
   Body b;toBody(c,b);b.user=i;
   if(i==held){b.kinematic=true;b.setMass(0);b.sleeping=false;b.w=heldSpin;
    b.v=carryVelocity();}
   if(c.material()==PropMaterial::Paper){b.linearDamping=3;b.angularDamping=3;}
   b.id=i;bodyOf[size_t(i)]=int(space.bodies.size());space.bodies.push_back(b);}
  // The player: a body at his own velocity (no gravity, no turning, not written back; 20 kg of push, so what
  // he pins against a wall is not squeezed into it). He shoves what
  // he walks into by contact, and what is pinned against a wall stops him in the solver rather than going
  // into the wall. What comes at him fast is left to bodyContacts (the blow, its damage).
  if(level==m_level&&!m_noclip&&!m_inspectionCamera){static Shape hull;static float hullHeight=-1;const float h=m_player.hullHeight();
   // From his step height up: he never stands on, or treads down, what his body pushes (the low things at his
   // feet are moved by the old rule, simulateClutter).
   const float body=std::max(.2f,h-.25f);
   if(hull.count==0||hullHeight!=body){hull=Shape{};hull.add(PartKind::Box,{},{.2f,.2f,body*.5f});hull.finish();hullHeight=body;}
   Body p;p.shape=&hull;p.x={m_player.pos.x,m_player.pos.y,m_player.z+.25f+body*.5f};p.v={m_velocity.x,m_velocity.y,0};p.gravity={};p.user=PlayerBody;
   p.invMass=1/tune("props.player_mass",20.f);p.invInertiaLocal=M3::zero();p.inertiaLocal=M3::zero();p.invInertia=M3::zero();p.material={.6f,.5f,0,0};p.linearDamping=0;p.angularDamping=0;
   // A shove, not a blow: his contacts are springs (a limited push), so a crate he walks into gets going over a
   // moment and what is in it goes along; one pinned against a wall gives under him instead of into the wall.
   p.softHertz=tune("props.player_hertz",5.f);p.sleepTime=1e9f;p.id=(1<<19)-1;   // never what keeps an island awake (his own speed still does)
   space.bodies.push_back(p);}
  if(level==m_level)addVrHandBodies(space,list,RigidStep);   // the VR hands that hold nothing (VrHands.cpp)
  // Each man and creature within reach of what moves: a box of his hull from just over his feet, his own
  // speed, his weight, no gravity and no turning; contacts on it are hard (a crate does not go into him).
  for(size_t k=0;k<actors.size();++k){const auto&r=actors[k];bool near=false;for(const auto&rc:reaches)near|=r.at.x+r.radius>=rc.x0&&r.at.x-r.radius<=rc.x1&&r.at.y+r.radius>=rc.y0&&r.at.y-r.radius<=rc.y1;if(!near)continue;
   const float lift=.04f,h=std::max(.3f,r.height-lift);actorShapes.emplace_back();Shape& hull=actorShapes.back();hull.add(PartKind::Box,{},{r.radius,r.radius,h*.5f});hull.finish();
   Body a;a.shape=&hull;a.x={r.at.x,r.at.y,r.feet+lift+h*.5f};a.v={r.velocity.x,r.velocity.y,0};a.gravity={};a.user=ActorBody-int(k);a.invMass=1/r.mass;a.invInertiaLocal=M3::zero();a.inertiaLocal=M3::zero();a.invInertia=M3::zero();
   a.material={.6f,.5f,0,0};a.linearDamping=0;a.angularDamping=0;a.sleepTime=1e9f;a.id=(1<<19)-2-int(k);actors[k].body=int(space.bodies.size());actors[k].start=a.x;space.bodies.push_back(a);}
  chainBody.resize(space.bodies.size(),0);for(const auto&ch:chains)for(int k=0;k<3;++k)chainBody[size_t(ch.body[size_t(k)])]=1;
  for(auto&h:hinges)space.constraints.push_back(&h);
  propJoints.clear();propJointOf.clear();addPropJoints(level,list,bodyOf,space,propJoints,propJointOf);
  m_rigidStats.buildMs+=since(mark);mark=Clock::now();
  wasAsleep.assign(space.bodies.size(),0);before.assign(space.bodies.size(),{});
  for(size_t bi=0;bi<space.bodies.size();++bi){wasAsleep[bi]=space.bodies[bi].sleeping;before[bi]=space.bodies[bi].v;}
  // ---- Before the step: water, and panes ahead of a fast prop ----
  for(size_t bi=0;bi<space.bodies.size();++bi){auto&b=space.bodies[bi];if(!b.movable()||b.user<0)continue;const int index=b.user;auto&c=list[size_t(index)];
   c.impactCooldown=std::max(0.f,c.impactCooldown-RigidStep);c.bodyImpactCooldown=std::max(0.f,c.bodyImpactCooldown-RigidStep);c.paneRearm=std::max(0.f,c.paneRearm-RigidStep);c.tipTime=std::max(0.f,c.tipTime-RigidStep);
   Vec2 q=c.pos+shift;const World& w=physicalWorld(q);
   const float surface=w.waterSurface(q.x,q.y);
   if(surface>-999.f){V3 lo,hi;b.boundsLo={1e30f,1e30f,1e30f};for(int p=0;p<b.shape->count;++p){partBounds(b,p,lo,hi);b.boundsLo.z=std::min(b.boundsLo.z,lo.z);}
    const float bottom=b.boundsLo.z;
    // Through the surface this step, falling: the splash (its base above the water now, under it after).
    if(b.v.z<-1.f&&bottom>=surface-.002f&&bottom+b.v.z*RigidStep*1.05f<surface&&c.impactCooldown<=0){raiseWaterSplash(w.level(),{q.x,q.y,surface},{b.v.x,b.v.y,b.v.z},buoyancy::splashPower(c.mass(),len(b.v)),WaterSplashKind::Entry);c.impactCooldown=.2f;}
    if(bottom<surface){
     // The share of its volume under the surface and that volume's middle: each part in 27 cells, each cell
     // under by how much of its own height is (so the share runs smoothly as it sinks or rocks).
     float under=0,all=0;V3 middle{};const M3 turn=b.q.matrix();wetCells.clear();
     for(int p=0;p<b.shape->count;++p){const auto&part=b.shape->parts[size_t(p)];V3 cellsAt,half;partVolumeBox(part,cellsAt,half);
      const float vol=half.x*half.y*half.z;const M3 r=turn*part.turn.matrix();
      // A round part's (a drum's, a pipe's) cells lie round its axis: one on it and eight on the ring that halves its section's
      // outer eight ninths, and a cell is as tall as its disc stands, so a drum rolling in the water rides on no corners (in a
      // square turned with it, its lift rose and fell four times a turn). water.round_cells 0: the square.
      const bool round=(part.kind==PartKind::Cylinder||part.kind==PartKind::Capsule)&&tune("water.round_cells",1.f)>.5f;const float ring=.722f*part.half.x;
      const float tall=round?(half.x*std::sqrt(std::max(0.f,1-r.m[2][2]*r.m[2][2]))+std::fabs(r.m[2][2])*half.z)*2.f/3.f
                            :(std::fabs(r.m[2][0])*half.x+std::fabs(r.m[2][1])*half.y+std::fabs(r.m[2][2])*half.z)*2.f/3.f;   // a cell's height, upright
      for(int i=-1;i<=1;++i)for(int j=-1;j<=1;++j)for(int k=-1;k<=1;++k){
       V3 o{half.x*float(i)*.667f,half.y*float(j)*.667f,half.z*float(k)*.667f};if(round){if(i||j){const float g=std::atan2(float(j),float(i));o.x=std::cos(g)*ring;o.y=std::sin(g)*ring;}else o.x=o.y=0;}
       const V3 l=cellsAt+part.turn.rotate(o);const V3 at=b.toWorld(l);
       const float wet=std::clamp((surface-(at.z-tall*.5f))/std::max(1e-4f,tall),0.f,1.f);all+=vol;if(wet>0){under+=vol*wet;middle=middle+at*(vol*wet);wetCells.push_back({at-b.x,wet*std::pow(vol*8.f/27.f,2.f/3.f)});}}}
     const float f=all>0?under/all:0.f;
     if(f>0){middle=middle*(1/under);const float rho=buoyancy::density(c),k=buoyancy::drag(c)*tune("water.drag",1.f);const bool floater=rho<buoyancy::WaterDensity;
      const float mass=b.invMass>0?1/b.invMass:1;const float height=std::max(.01f,c.height());
      // Lifted at the middle of what is under (so a thing half in tips as it should), drawn halfway to its own
      // middle (water.lever): a prop's mass is in its walls, not spread as a solid block's.
      b.applyForce({0,0,mass*buoyancy::Gravity*buoyancy::WaterDensity/rho*f},b.x+(middle-b.x)*tune("water.lever",.35f));
      const float horizontal=std::sqrt(b.v.x*b.v.x+b.v.y*b.v.y);const float hk=std::exp(-(1.1f+k*horizontal)*f*RigidStep);b.v.x*=hk;b.v.y*=hk;
      const float bob=floater?2*tune("water.bob_damping",.2f)*std::sqrt(buoyancy::Gravity*buoyancy::WaterDensity/(rho*height)):tune("water.sink_drag",2.6f);
      b.v.z*=std::exp(-(1.1f+bob+k*std::fabs(b.v.z))*f*RigidStep);b.w=b.w*std::exp(-2.6f*f*RigidStep);
      // The water against its turning, cell by cell: each wet cell's own speed through the water from the turn, resisted by
      // its square (half rho Cd A |u| u, water.turn_drag the Cd) at the cell, so the drag grows with the reach: a plank or a
      // log spun in the water slows far sooner than a mug, and a drum tipping over is checked as it goes, not flung.
      {b.updateInertia();V3 tau{};const float cd=tune("water.turn_drag",1.f)*.5f*buoyancy::WaterDensity;
       for(const auto&cell:wetCells){const V3 u=cross(b.w,cell.r);tau=tau+cross(cell.r,u*(-cd*cell.area*len(u)));}
       V3 dw=b.invInertia*tau*RigidStep;const float most=len(b.w),l=len(dw);if(l>most&&l>0)dw=dw*(most/l);b.w=b.w+dw;}
      const float bed=w.supportBelow(q.x,q.y,bottom+.025f,true);const bool afloat=floater&&surface-bed>height*rho/buoyancy::WaterDensity*.9f;
      if(afloat&&f<.999f){
       // Afloat: a prop's mass lies in its walls (a crate's planks, a box's card), so it rides on a face, not
       // on a corner as a solid block of its density would: the water rights it toward its nearest face it can float on.
       // A face is one when its waterplane's lever over what is under (BM, a^2 / 6 h s for a box of half extent h across
       // the face and a the lesser of the others, s its density over water's) beats the drop from the middle of what is
       // under to its own (BG, h (1 - s)), that drop taken at water.hollow of a solid block's (its mass in its walls):
       // a crate rides on any face; a plank, a log or a pipe only lying flat; a drum or a bottle on its side, so one
       // dropped upright bobs, tips over and settles. (Nearest of all faces, as before, when none is.)
       // A swell rocks it a few degrees and heaves it, a slow drift carries it (each on its own phase).
       const auto dims=c.size();const float sr=std::clamp(rho/buoyancy::WaterDensity,.05f,.95f),hollow=tune("water.hollow",.5f);
       auto floatsOn=[&](int a){const float h=std::max(.005f,dims[size_t(a)]*.5f),w=std::min(dims[size_t((a+1)%3)],dims[size_t((a+2)%3)])*.5f;return w*w/(6*h*sr)>hollow*h*(1-sr);};
       V3 axis{};float best=-1;for(int pass=0;pass<2&&best<0;++pass)for(int a=0;a<3;++a){if(pass==0&&!floatsOn(a))continue;const V3 v=turn.column(a);const float d=std::fabs(v.z);if(d>best){best=d;axis=v.z<0?neg(v):v;}}
       b.w=b.w+cross(axis,V3{0,0,1})*(tune("water.right",80.f)*RigidStep);
       const float phase=float(index%97)*.61f,swell=tune("water.swell",1.f);
       b.w=b.w+V3{std::cos(c.yaw),std::sin(c.yaw),0}*(1.5f*swell*std::sin(1.6f*m_elapsed+phase*1.7f+1.f)*RigidStep)+V3{-std::sin(c.yaw),std::cos(c.yaw),0}*(1.5f*swell*std::sin(1.9f*m_elapsed+phase)*RigidStep);
       b.v.z+=.45f*swell*std::sin(2.4f*m_elapsed+phase*2.3f)*RigidStep;const float drift=tune("water.drift",.03f);
       b.v.x+=std::cos(.23f*m_elapsed+phase)*drift*RigidStep;b.v.y+=std::sin(.19f*m_elapsed+phase*1.3f)*drift*RigidStep;b.sleepTime=0;}}}}
   // A fast prop's way ahead this step: the panes it would pass (a thrown thing meets glass at its speed).
   const float speed=len(b.v);if(speed*RigidStep>.01f){Vec2 q2{b.x.x,b.x.y};q2=q2+shift;const World& rw=physicalWorld(q2);const int paneLevel=rw.level();auto&pw=paneLevel==m_level?m_world:m_chunks[paneLevel].world;
    const auto e=c.extent();const V3 delta=b.v*RigidStep;const float distance=len(delta);const V3 from{q2.x,q2.y,b.x.z};const V3 travel=delta*(1+(std::max({e[0],e[1],e[2]})+space.settings.speculative+distance)/distance);   // as far ahead as its contacts reach
    V3 n;ShotTrace trace;float t=pw.shotFraction(from,from+travel,n,&trace);const float m=c.mass();
    const V3 arriving=b.v+b.gravity*RigidStep;   // the speed it meets the glass at, a step on
    for(int j=0;j<2;++j)if(trace.passed[j]>=0&&trace.passedAt[j]<t){const auto pn=trace.passedNormal[j];const float closing=std::fabs(dot(arriving,pn));
     const float keep=strike(index,paneLevel,trace.passed[j],.5f*m*closing*closing,from+travel*trace.passedAt[j],delta*(1/distance));if(keep>0){b.v=b.v*keep;}else break;}
    if(t<1&&trace.blocking>=0){const float closing=std::fabs(dot(arriving,n));const float keep=strike(index,paneLevel,trace.blocking,.5f*m*closing*closing,from+travel*t,delta*(1/distance));if(keep>0)b.v=b.v*keep;}}
  }
  // ---- The step ----
  m_rigidStats.preMs+=since(mark);space.step(RigidStep);mark=Clock::now();if(level==m_level&&m_vrInputActive){settleVrHands(space,held);m_vrHandStepped=true;}if(!propJoints.empty())afterPropJoints(level,list,space,propJoints,propJointOf);m_rigidStats.worldMs+=space.worldMs;if(level==m_level){m_rigidStats.ccdMs+=space.ccdMs;m_rigidStats.ccdSweeps+=space.ccdSweeps;}m_rigidStats.pairMs+=space.pairMs;m_rigidStats.solveMs+=space.solveMs;
  // ---- Back to the props ----
  for(const auto&rec:chains){bool still=true;for(int k:rec.body)still&=wasAsleep[size_t(k)]&&space.bodies[size_t(k)].sleeping;if(!still)writeChain(rec);}
  for(size_t bi=0;bi<space.bodies.size();++bi){const auto&b=space.bodies[bi];if(b.kinematic||b.user<0||chainBody[bi])continue;auto&c=list[size_t(b.user)];
   if(wasAsleep[bi]&&b.sleeping)continue;fromBody(b,c);c.island=b.sleeping?space.islandOf.empty()?-1:std::max(0,space.islandOf[bi]):-1;
   if(len(b.v)<.5f)c.projectile=false;}
  // Scrape and roll: each prop's loops (AudioEngine) ease toward the fastest slide and roll of the contacts it bears on
  // (Manifold::slideSpeed, rollSpeed), and fall silent the moment it is held, asleep or off everything.
  {std::vector<float> slide(list.size(),0.f),turn(list.size(),0.f);
   for(const auto&m:space.manifolds){float load=0;for(int k=0;k<m.count;++k)load+=m.points[size_t(k)].normalImpulse;if(load<=0)continue;
    for(int side=0;side<2;++side){const int bi=side?m.b:m.a;if(bi<0)continue;const Body& B=space.bodies[size_t(bi)];if(B.kinematic||B.user<0)continue;
     slide[size_t(B.user)]=std::max(slide[size_t(B.user)],m.slideSpeed);turn[size_t(B.user)]=std::max(turn[size_t(B.user)],m.rollSpeed);}}
   const float ease=std::min(1.f,RigidStep*14.f);
   for(size_t bi=0;bi<space.bodies.size();++bi){const auto&b=space.bodies[bi];if(b.user<0)continue;auto&c=list[size_t(b.user)];
    if(b.kinematic||b.sleeping||c.broken){c.scrapeLevel=c.rollLevel=0;continue;}
    const float s=std::clamp((slide[size_t(b.user)]-.06f)/1.2f,0.f,1.f),r=std::clamp((turn[size_t(b.user)]-.06f)/1.5f,0.f,1.f);
    c.scrapeLevel+=(s-c.scrapeLevel)*ease;c.rollLevel+=(r-c.rollLevel)*ease;if(c.scrapeLevel<.005f)c.scrapeLevel=0;if(c.rollLevel<.005f)c.rollLevel=0;}}
  // A prop leaning on him or pushed slowly into him moves him as far as it moved his body in the solver.
  for(auto&r:actors){if(r.body<0||size_t(r.body)>=space.bodies.size())continue;const V3 d=space.bodies[size_t(r.body)].x-r.start;r.pushed+=Vec2{d.x,d.y}-r.velocity*RigidStep;r.body=-1;}
  for(const auto&ev:space.events){if(ev.b<0)continue;const int ua=space.bodies[size_t(ev.a)].user,ub=space.bodies[size_t(ev.b)].user;const bool aActor=ua<=ActorBody,bActor=ub<=ActorBody;
   if(aActor==bActor||(aActor?ub:ua)<0)continue;const int propBody=aActor?ev.b:ev.a,prop=aActor?ub:ua;auto&r=actors[size_t(ActorBody-(aActor?ua:ub))];const V3 into=aActor?neg(ev.normal):ev.normal;   // from the prop into him
   // Knocked by what came at him (the prop's own way toward him before the blow), not by his walking into a prop.
   // The speed his body takes from the blow: the prop's momentum at its closing speed shared over both masses
   // (a fifth of it given back), the most of the frame's blows.
   const float coming=dot(before[size_t(propBody)],into);if(coming>.5f){const Vec2 dir{into.x,into.y};const float l=length(dir);const float m=list[size_t(prop)].mass();
    const float dv=1.2f*m*ev.approach/(m+r.mass);if(l>.3f&&dv>length(r.shove))r.shove=dir*(1/l)*dv;}
   if(ev.approach>r.hurtSpeed&&coming>.5f){r.hurtSpeed=ev.approach;r.hurtProp=prop;}}
  for(const auto&ev:space.events){m_rigidStats.events.push_back(ev);if(space.bodies[size_t(ev.a)].user<0||(ev.b>=0&&space.bodies[size_t(ev.b)].user<0))continue;
   const int a=space.bodies[size_t(ev.a)].user,bUser=ev.b>=0?space.bodies[size_t(ev.b)].user:-1;
   auto&ca=list[size_t(a)];
   // Whoever came in faster (before the blow) gives both its sound's owner (a guard's kick stays his).
   if(bUser>=0){auto&cb=list[size_t(bUser)];const int owner=len(before[size_t(ev.a)])>=len(before[size_t(ev.b)])?ca.soundOwner:cb.soundOwner;ca.soundOwner=cb.soundOwner=owner;}
   const bool aKinematic=space.bodies[size_t(ev.a)].kinematic,bKinematic=ev.b>=0&&space.bodies[size_t(ev.b)].kinematic;
   const float middles=std::max(0.f,dot(before[size_t(ev.a)]-(ev.b>=0?before[size_t(ev.b)]:V3{}),ev.normal));
   // A prop breaks by the speed it was struck at, or by the blow it took (the impulse over its own mass): a crate
   // dropped on a bottle crushes it however slowly the bottle was going. (The blow over (1 + restitution) is the
   // speed a drop on the floor would give the same blow.)
   auto blow=[&](const Clutter& v){return ev.approach<1?0.f:ev.impulse/std::max(.05f,v.mass())/(1+propMaterialOf(v).restitution);};   // (a crate set down gently crushes nothing)
   if(!aKinematic)impact(a,ev.approach,std::max(middles,blow(ca)));
   if(bUser>=0&&!bKinematic){auto&cb=list[size_t(bUser)];const float bs=propMaterialTraits(cb.material()).breakSpeed;if(bs>0&&std::max(middles,blow(cb))>bs)breakClutter(level,size_t(bUser),{ev.normal.x,ev.normal.y,ev.normal.z});else if(aKinematic)impact(bUser,ev.approach);}
   if(aKinematic)continue;
   // Against the world: was it a pane? It takes the energy the contact brought (its effective mass).
   if(ev.b<0&&!ca.broken){Vec2 q{ev.point.x,ev.point.y};q=q+shift;const World& pw=physicalWorld(q);const auto&panes=pw.breakables();
    for(int pi=0;pi<int(panes.size());++pi){const auto&pn=panes[size_t(pi)];if(!pw.breakableIntact(pi))continue;
     if(q.x<pn.x1-.04f||q.x>pn.x2+.04f||q.y<pn.y1-.04f||q.y>pn.y2+.04f||ev.point.z<pn.bottom-.04f||ev.point.z>pn.top+.04f)continue;
     const float meff=ev.effectiveMass>0?ev.effectiveMass:ca.mass();const float keep=strike(a,pw.level(),pi,.5f*meff*ev.approach*ev.approach,{q.x,q.y,ev.point.z},ev.normal);
     if(keep>0){const V3 v=before[size_t(ev.a)]*keep;ca.velocity={v.x,v.y};ca.vz=v.z;ca.sleeping=false;ca.restTime=0;}break;}}
  }
  // What his body pushes goes no faster than the old push had it (his pace, by 4 kg over its mass, and 0.2 m/s):
  // a sprint into a chair sends it off, not flying.
  {const float pace=length(m_velocity);
   for(const auto&m:space.manifolds){if(m.b<0)continue;const Body& A=space.bodies[size_t(m.a)];const Body& B=space.bodies[size_t(m.b)];const int prop=A.user==PlayerBody?B.user:B.user==PlayerBody?A.user:-2;if(prop<0)continue;
    auto&c=list[size_t(prop)];const float cap=pace*std::min(1.f,4/c.mass())+.2f,speed=length(c.velocity);if(speed>cap&&speed>0)c.velocity=c.velocity*(cap/speed);}}
  // What the player's body is shoving (a container so marked carries its contents along: Containers.h).
  for(const auto&m:space.manifolds){if(m.b<0)continue;const Body& A=space.bodies[size_t(m.a)];const Body& B=space.bodies[size_t(m.b)];
   if(A.user==PlayerBody&&B.user>=0)list[size_t(B.user)].blockedTime=std::max(list[size_t(B.user)].blockedTime,.25f);
   if(B.user==PlayerBody&&A.user>=0)list[size_t(A.user)].blockedTime=std::max(list[size_t(A.user)].blockedTime,.25f);}
  m_rigidStats.postMs+=since(mark);
  // Bodies struck by what moves.
  for(size_t bi=0;bi<space.bodies.size();++bi){const auto&b=space.bodies[bi];if(b.kinematic||b.sleeping||b.user<0||(chainBody[bi]&&bodyOf[size_t(b.user)]!=int(bi)))continue;bodyContacts(b.user);}
  for(int i=0;i<int(list.size());++i)if(list[size_t(i)].broken){auto&c=list[size_t(i)];c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;c.sleeping=true;c.restTime=0;}
 }
 // The blows on men and creatures: hurt by the energy past a tap (as a body was always hurt by a prop), knocked
 // back by the speed the prop's momentum gave him: a mug nudges a man a centimetre; a thrown crate sends him
 // stumbling a metre back, its blow spoilt; a drum rolled into him at a run knocks a man down.
 for(auto&r:actors){
  if(r.hurtProp>=0&&r.hurtSpeed>=1.5f){auto&c=list[size_t(r.hurtProp)];if(c.bodyImpactCooldown<=0){const float damage=std::clamp((.5f*c.mass()*r.hurtSpeed*r.hurtSpeed-2.f)*.2f,0.f,80.f);
    if(damage>0){actorHurt(r,c,damage);c.bodyImpactCooldown=.25f;c.projectile=false;}}}
  if(lengthSq(r.pushed)>1e-8f&&length(r.shove)<.15f){BodyKnock k;k.level=r.level;k.enemy=r.enemy;k.index=r.index;k.velocity=r.pushed*(1/std::max(dt,1e-3f));k.time=dt;m_bodyKnocks.push_back(k);}   // moved this frame (updateBodyKnocks sweeps it)
  const float dv=length(r.shove);if(dv<.02f)continue;const Vec2 dir=r.shove*(1/dv);
  BodyKnock k;k.level=r.level;k.enemy=r.enemy;k.index=r.index;
  if(dv<.15f){k.velocity=dir*(dv*.22f/.1f);k.time=.1f;}   // a nudge: his own footing takes it within a step
  else{const float reach=std::min(1.5f,1.6f*dv);k.velocity=dir*std::sqrt(2*4.f*reach);k.time=1;
   // A shove staggers him: reeling while he slides (to a stop at 4 m/s^2), then finding his feet.
   k.slide=length(k.velocity)/4.f;k.push=dir;k.stumble=k.slide+.45f;k.flinch=r.enemy?1.f:0.f;}
  bool merged=false;for(auto&o:m_bodyKnocks)if(o.enemy==k.enemy&&o.level==k.level&&o.index==k.index){if(lengthSq(k.velocity)>lengthSq(o.velocity))o.velocity=k.velocity;o.time=std::max(o.time,k.time);
   if(k.stumble>o.stumble-o.age){o.stumble=k.stumble;o.slide=k.slide;o.age=0;o.push=k.push;o.flinch=std::max(o.flinch,k.flinch);}   // a harder blow mid-stumble staggers him afresh
   merged=true;}   /* a prop still rolling into him keeps him going, no further */if(!merged)m_bodyKnocks.push_back(k);
  if(r.enemy){auto&enemy=(r.level==m_level?m_enemies:m_chunks[r.level].enemies)[r.index];if(dv>=.5f){enemy.windup=0;enemy.strike=0;enemy.attackCooldown=std::max(enemy.attackCooldown,.35f+dv*(dv>=1.3f?1.5f:.6f));enemy.painFlash=std::max(enemy.painFlash,std::min(1.f,dv*.6f));}}
  else if(dv>=1.3f&&!npcHeld(r.index)){auto&npc=m_friendlyActors[r.index];setState(friendlyKey(r.index,"restraint"),1);npc.pose.clip=4;npc.pose.phase=1;saveRestrainedPose(r.index);}   // knocked down: out cold where he falls
}
 rp.cache.swap(space.cache);
 int awake=0;for(const auto&b:space.bodies)awake+=b.movable()&&b.user>=0;
 m_rigidStats.bodies=int(space.bodies.size());m_rigidStats.awake=awake;m_rigidStats.manifolds=int(space.manifolds.size());m_rigidStats.worldManifolds=space.worldManifolds;m_rigidStats.steps=steps;
 if(level==m_level)m_rigidStats.manifoldsSeen=std::move(space.manifolds);
 m_rigidStats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
}
void Game::updateBodyKnocks(float dt){
 for(auto& k:m_bodyKnocks){if(k.time<=0)continue;const float speed=length(k.velocity);if(speed<.05f){k.time=0;continue;}
  const Vec2 delta=k.velocity*dt;const int steps=std::max(1,int(std::ceil(length(delta)/.04f)));
  if(k.enemy){if(!chunkResident(k.level)){k.time=0;continue;}auto&enemies=k.level==m_level?m_enemies:m_chunks[k.level].enemies;if(k.index>=enemies.size()||!enemies[k.index].alive){k.time=0;continue;}auto&enemy=enemies[k.index];
   const Vec2 before=enemy.pos;Vec2 reached=before;for(int i=1;i<=steps;++i){const auto next=before+delta*(float(i)/float(steps));if(!hullFits(next+chunkOffset(k.level)-chunkOffset(m_level),enemy.z,enemy.hull(),enemy.radius(),false,false))break;reached=next;}
   if(length(reached-before)<length(delta)*.5f)k.velocity=Vec2{};enemy.pos=reached;enemy.windup=0;}
  else{if(k.index>=m_friendlyActors.size()){k.time=0;continue;}auto&npc=m_friendlyActors[k.index];const float height=friendlyHullHeight(k.index,npc.track());
   const Vec2 before=npc.pose.position;Vec2 reached=before;for(int i=1;i<=steps;++i){const auto next=before+delta*(float(i)/float(steps));if(!friendlyBodyFits(npc.level,next,npc.pose.z,height))break;reached=next;}
   if(length(reached-before)<length(delta)*.5f)k.velocity=Vec2{};npc.pose.position=reached;k.at=reached;k.placed=true;}
  // He stumbles to a stop (4 m/s^2: his feet find the floor again).
  const float slowed=std::max(0.f,speed-4.f*dt);k.velocity=speed>0?k.velocity*(slowed/speed):Vec2{};k.time-=dt;}
 std::erase_if(m_bodyKnocks,[](const BodyKnock& k){return (k.time<=0||lengthSq(k.velocity)<.0025f)&&k.age>=k.stumble;});
}
// His lean: thrown with the shove from his feet within a tenth of a second (harder for a harder shove, 11 to
// 17 degrees), held while he slides, then straightened as he finds his feet, with a little sway back past
// upright at the last.
Vec2 Game::npcStumbleLean(size_t actor)const{
 for(const auto& k:m_bodyKnocks){if(k.enemy||k.index!=actor||k.stumble<=0||k.age>=k.stumble)continue;
  const auto& npc=m_friendlyActors[actor];if(npc.level!=k.level||npc.pose.clip!=3)return {};
  auto smooth=[](float a,float b,float x){x=std::clamp((x-a)/std::max(b-a,1e-4f),0.f,1.f);return x*x*(3-2*x);};
  const float peak=std::clamp(.14f+.06f*k.slide*4.f,.2f,.3f),settle=smooth(k.slide,k.stumble,k.age);
  const float amount=peak*smooth(0.f,.1f,k.age)*((1-settle)-.18f*std::sin(kPi*settle));
  return k.push*amount;}
 return {};
}
// Reeling while he slides (the hit clip's first half, quick at first), finding his feet after (the rest).
float Game::stumblePhase(float age,float slide,float stumble){
 const float reel=std::max(slide,.12f);
 if(age<reel){const float t=age/reel;return .5f*(1-(1-t)*(1-t));}
 return .5f+.5f*std::clamp((age-reel)/std::max(stumble-reel,.05f),0.f,1.f);
}
// After everyone has moved this frame (the guards' own minds included), the knocked play their stumble: a man
// his hit clip where the shove left him, his gun down till he has his feet; a creature its flinch.
void Game::poseBodyKnocks(float dt){
 for(auto& k:m_bodyKnocks){
  if(k.stumble>0&&k.age<k.stumble){
   if(k.enemy){if(k.flinch>0&&chunkResident(k.level)){auto&enemies=k.level==m_level?m_enemies:m_chunks[k.level].enemies;
     if(k.index<enemies.size()&&enemies[k.index].alive){auto&e=enemies[k.index];e.painFlash=std::max(e.painFlash,.84f*(1-k.age/k.stumble));}}}
   else if(k.index<m_friendlyActors.size()){auto&npc=m_friendlyActors[k.index];const auto&track=npc.track();
    if(npc.level==k.level&&humanActorVisual(track.visual)&&npc.pose.clip!=4&&!npcIncapacitated(k.index)&&!(track.deadState&&state(track.deadState))){
     if(k.placed)npc.pose.position=k.at;
     // On his feet again: back to standing (his mind takes him on from there; a frozen man stays as he is).
     if(k.age+dt>=k.stumble){if(npc.pose.clip==3){npc.pose.clip=0;npc.pose.phase=0;}}
     else{npc.pose.clip=3;npc.pose.phase=stumblePhase(k.age,k.slide,k.stumble);}
     if(track.ai.mode==ActorAiMode::Guard){const StateId recovery=friendlyKey(k.index,"weapon_recovery");setState(recovery,std::max(state(recovery),int(std::ceil((k.stumble-k.age)*1000))));}}}}
  k.age+=dt;}
 std::erase_if(m_bodyKnocks,[](const BodyKnock& k){return (k.time<=0||lengthSq(k.velocity)<.0025f)&&k.age>=k.stumble;});
}
}

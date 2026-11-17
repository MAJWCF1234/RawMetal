// Model props on the rigid core (physics/Rigid.h). simulateClutter still runs everything that happens to
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
#include <chrono>
namespace retro {
namespace {
constexpr float RigidStep=1.f/120;constexpr int RigidMaxSteps=6;constexpr int PlayerBody=-2;
// Panes, as Clutter.cpp's strike has them.
constexpr float PaneTap=2,PaneRearm=.3f;
float paneBreakEnergy(const Breakable& b){return b.material==17?(b.holds?450.f:150.f):b.material==22?40.f:300.f;}
float paneLimit(const Breakable& b){return b.holds?180.f:b.health>0?b.health:b.material==17?20.f:b.material==22?8.f:35.f;}
// The ground's material against a prop: a hard floor that takes no bounce of its own.
const rigid::Material Ground{.7f,.6f,0.f,.004f};   // a floor is rough: what rolls on it comes to rest
struct Hit {rigid::V3 p{},n{};float sep=0;int kind=0;};
// Up to four of the hits: the deepest, the farthest from it, the largest triangle, the largest quad.
int keepFour(std::vector<Hit>& h,rigid::V3 n){
 using namespace rigid;if(h.size()<=4)return int(h.size());int pick[4]{0,-1,-1,-1};
 for(int i=1;i<int(h.size());++i)if(h[size_t(i)].sep<h[size_t(pick[0])].sep)pick[0]=i;
 float best=-1;for(int i=0;i<int(h.size());++i){const float d=lenSq(h[size_t(i)].p-h[size_t(pick[0])].p);if(d>best){best=d;pick[1]=i;}}
 auto area=[&](V3 a,V3 b,V3 c){return std::fabs(dot(cross(b-a,c-a),n));};
 best=-1;for(int i=0;i<int(h.size());++i){if(i==pick[0]||i==pick[1])continue;const float a=area(h[size_t(pick[0])].p,h[size_t(pick[1])].p,h[size_t(i)].p);if(a>best){best=a;pick[2]=i;}}
 if(pick[2]>=0){best=-1;const float tri=area(h[size_t(pick[0])].p,h[size_t(pick[1])].p,h[size_t(pick[2])].p);
  for(int i=0;i<int(h.size());++i){if(i==pick[0]||i==pick[1]||i==pick[2])continue;const V3 p=h[size_t(i)].p;
   const float a=area(h[size_t(pick[0])].p,h[size_t(pick[1])].p,p)+area(h[size_t(pick[1])].p,h[size_t(pick[2])].p,p)+area(h[size_t(pick[2])].p,h[size_t(pick[0])].p,p)-tri;if(a>best){best=a;pick[3]=i;}}}
 std::vector<Hit> out;for(int i:pick)if(i>=0)out.push_back(h[size_t(i)]);h.swap(out);return int(h.size());}
}
void Game::simulateRigidProps(std::vector<Clutter>& list,int level,float dt,int held){
 using namespace rigid;
 auto& rp=m_rigidProps[level];
 const Vec2 shift=chunkOffset(level)-chunkOffset(m_level);
 // A point in this list's coordinates, in the world that holds it (p becomes that world's own coordinates).
 auto physicalWorld=[&](Vec2& p)->const World&{auto local=p-shift;if(local.x>=0&&local.x<24&&local.y>=0&&local.y<24){p=local;return level==m_level?m_world:m_chunks[level].world;}return worldAt(p);};
 auto inPlay=[&](int i){const auto&c=list[size_t(i)];return c.model>=0&&!c.broken&&!c.riding&&!dormantEntity(c.z);};
 bool any=false;for(int i=0;i<int(list.size());++i)if(inPlay(i)&&(!list[size_t(i)].sleeping||i==held)){any=true;break;}
 // A sleeping prop the moving player walks into is stepped too (his body in the solver wakes it).
 if(!any&&level==m_level&&lengthSq(m_velocity)>1e-4f)for(int i=0;i<int(list.size())&&!any;++i){if(!inPlay(i))continue;const auto&c=list[size_t(i)];const auto e=c.extent();
  any=std::fabs(c.pos.x-m_player.pos.x)<e[0]+.35f&&std::fabs(c.pos.y-m_player.pos.y)<e[1]+.35f&&c.z<m_player.z+m_player.hullHeight()&&c.z+c.height()>m_player.z;}
 rp.accumulator+=dt;if(!any){rp.accumulator=std::min(rp.accumulator,RigidStep);return;}
 int steps=0;while(rp.accumulator>=RigidStep-1e-6f&&steps<RigidMaxSteps){rp.accumulator-=RigidStep;++steps;}
 if(steps==RigidMaxSteps)rp.accumulator=std::min(rp.accumulator,RigidStep);
 if(!steps)return;
 const auto started=std::chrono::steady_clock::now();
 Space space;space.settings.substeps=int(tune("props.substeps",2.f));space.settings.contactHertz=tune("props.contact_hertz",30.f);
 space.cache.swap(rp.cache);
 std::vector<V3> samples;std::vector<char> curved;std::vector<Hit> hits;std::array<V3,8> feature{};
 // Each world's ramps (ShapeRamp, not solid: the slope its step proxies stand for), found once a frame.
 std::vector<std::pair<const World*,std::vector<int>>> ramps;
 auto rampsOf=[&](const World& w)->const std::vector<int>&{for(const auto&[k,v]:ramps)if(k==&w)return v;std::vector<int> found;
  for(int i=0;i<int(w.structures().size());++i){const auto&st=w.structures()[size_t(i)];if(st.shape==ShapeRamp&&!st.solid)found.push_back(i);}ramps.push_back({&w,found});return ramps.back().second;};
 const float spacing=tune("props.sample_spacing",.12f);
 // The world's contacts hold a body within half a millimetre of its surfaces, not the solver's few millimetres
 // of slop: the World's own column tests (fits) are exact, and props were always kept out of walls by them.
 const float worldSlop=space.settings.linearSlop-.0005f;
 // ---- The world round a body ----
 space.worldContacts=[&](Space& sp,int bi,float margin,std::vector<Manifold>& out){
  const Body& b=sp.bodies[size_t(bi)];if(b.user<0)return;samples.clear();curved.clear();hits.clear();
  Vec2 cq{b.x.x,b.x.y};cq=cq+shift;const World& cw=physicalWorld(cq);
  // The exact lowest points along the floor's normal (and a ramp's, under the body), and every part's
  // surface points. A curved part's sampled points meet only walls and ceilings: on a floor they would
  // be a polygon's corners, and a barrel would roll over them as if it were one.
  std::vector<V3> downs{V3{0,0,-1}};
  {const Vec2 k=cq-(Vec2{b.x.x,b.x.y}+shift);for(int r:rampsOf(cw)){const auto&st=cw.structures()[size_t(r)];
    if(b.boundsHi.x+shift.x+k.x<st.x1||b.boundsLo.x+shift.x+k.x>st.x2||b.boundsHi.y+shift.y+k.y<st.y1||b.boundsLo.y+shift.y+k.y>st.y2)continue;
    const int f=st.facing%4;const float run=f==0||f==2?st.y2-st.y1:st.x2-st.x1;if(run<=1e-3f)continue;const float g=(st.top-st.bottom)/run;
    const Vec2 up=f==0?Vec2{0,1}:f==1?Vec2{1,0}:f==2?Vec2{0,-1}:Vec2{-1,0};downs.push_back(neg(norm(V3{-g*up.x,-g*up.y,1})));}}
  for(int p=0;p<b.shape->count;++p){const bool round=b.shape->parts[size_t(p)].kind!=PartKind::Box;const size_t before=samples.size();surfaceSamples(b,p,spacing,samples);curved.resize(samples.size(),round?1:0);(void)before;
   for(V3 d:downs){const int n=supportFeature(b,p,d,feature);for(int k=0;k<n;++k){samples.push_back(feature[size_t(k)]);curved.push_back(0);}}
   for(V3 d:{V3{1,0,0},V3{-1,0,0},V3{0,1,0},V3{0,-1,0},V3{0,0,1}}){const int n=supportFeature(b,p,d,feature);for(int k=0;k<n;++k){samples.push_back(feature[size_t(k)]);curved.push_back(round?1:0);}}}
  const bool ceilingNear=cw.clearanceAbove(cq.x,cq.y,b.boundsLo.z,true)<b.boundsHi.z+margin+.3f;
  for(size_t si=0;si<samples.size();++si){const V3 p=samples[si];const bool round=curved[si]!=0;
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
   if(p.z-s<margin&&!round)hits.push_back({p,{0,0,1},p.z-s,0});
   // Every point is asked about walls too, from just over what it stands on: a thin thing on the floor
   // (a keyboard, a bottle on its side) is all floor points, and would otherwise roll through a wall.
   const float base=std::max(p.z-.005f,s+.004f);
   if(!w.fits(q.x,q.y,base,.01f,true,true)||w.doorBlocks(q.x,q.y,base,.01f)){
    // Inside a wall, a fixture or a door: its face from a ray out of the body's middle.
    const V3 from{b.x.x+shift.x+k.x,b.x.y+shift.y+k.y,std::max(b.x.z,s+.01f)},to{q.x,q.y,std::max(p.z,s+.006f)};V3 n;const float t=w.shotFraction(from,to,n);
    if(t<1&&dot(n,to-from)<0){const V3 at=from+(to-from)*t;hits.push_back({p,n,std::min(0.f,dot(to-at,n)),2});continue;}
    // The ray found no face (it began inside): the nearest open column round the point.
    bool found=false;for(float r:{.03f,.07f,.12f,.2f}){for(int a=0;a<8&&!found;++a){const float ang=kPi*.25f*float(a);const float x=q.x+std::cos(ang)*r,y=q.y+std::sin(ang)*r;
      if(w.fits(x,y,base,.01f,true,true)&&!w.doorBlocks(x,y,base,.01f)){hits.push_back({p,{std::cos(ang),std::sin(ang),0},-r,2});found=true;}}if(found)break;}
    continue;}
   if(ceilingNear&&p.z>b.x.z){const float cl=w.clearanceAbove(q.x,q.y,p.z-.05f,true);if(cl-p.z<margin)hits.push_back({p,{0,0,-1},cl-p.z,1});}
  }
  if(hits.empty())return;
  // Group: the floor, the ceiling, the walls by the way they face (eight ways round).
  auto groupOf=[](const Hit& h){if(h.kind<2)return h.kind;if(h.kind==3)return 30;const float a=std::atan2(h.n.y,h.n.x);return 2+(int(std::lround(a/(kPi*.25f)))+8)%8+(h.n.z>.5f?8:h.n.z<-.5f?16:0);};
  std::vector<int> groups;for(const auto&h:hits){const int g=groupOf(h);if(std::find(groups.begin(),groups.end(),g)==groups.end())groups.push_back(g);}
  std::sort(groups.begin(),groups.end());
  std::vector<Hit> group;
  for(int g:groups){group.clear();V3 n{};for(const auto&h:hits)if(groupOf(h)==g){group.push_back(h);n=n+h.n;}n=norm(n);
   // Only what is near touching: a point well above the deepest (a box's far edge as it tips) would crowd
   // a real contact out of the four (a barrel left on one rim).
   {float deepest=1e30f;for(const auto&h:group)deepest=std::min(deepest,h.sep);std::erase_if(group,[&](const Hit& h){return h.sep>deepest+.012f;});}
   keepFour(group,n);
   if(g==0){ // the floor's slope at the points kept (a ramp, terrain), by differences of the support
    V3 sum{};for(auto&h:group){Vec2 q{h.p.x,h.p.y};q=q+shift;const World& w=physicalWorld(q);const float s=h.p.z-h.sep,d=.04f;
     auto at=[&](float x,float y){const float v=w.supportBelow(x,y,s+.06f,true);return std::fabs(v-s)<.06f?v:s;};
     const float gx=(at(q.x+d,q.y)-at(q.x-d,q.y))/(2*d),gy=(at(q.x,q.y+d)-at(q.x,q.y-d))/(2*d);h.n=norm(V3{-gx,-gy,1});sum=sum+h.n;}
    n=norm(sum);for(auto&h:group)h.sep*=n.z;}
   Manifold m;m.a=bi;m.b=-1;m.partA=0;m.partB=g;m.normal=neg(n);m.material=Material::mix(b.material,Ground);m.material.restitution=b.material.restitution;m.count=0;
   for(const auto&h:group){auto&c=m.points[size_t(m.count++)];c=ContactPoint{};c.p=h.p;c.separation=h.sep-worldSlop;c.localA=b.q.unrotate(h.p-b.x);c.localB=h.p;}
   out.push_back(m);}
 };
 space.filter=[&](const Space& sp,int a,int b){const Body& A=sp.bodies[size_t(a)];const Body& B=sp.bodies[size_t(b)];
  if(A.user==PlayerBody||B.user==PlayerBody){const Body& prop=A.user==PlayerBody?B:A;const Body& player=A.user==PlayerBody?A:B;(void)player;return prop.kinematic?false:len(prop.v)<2.5f;}return true;};
 space.sweep=[&](Space&,int,V3 from,V3 to,V3& normal)->float{Vec2 q{from.x,from.y};q=q+shift;const World& w=physicalWorld(q);const Vec2 k=q-(Vec2{from.x,from.y}+shift);
  const V3 a{q.x,q.y,from.z},bb{to.x+shift.x+k.x,to.y+shift.y+k.y,to.z};return w.shotFraction(a,bb,normal);};
 // ---- A prop's own reactions (as the old step had them) ----
 auto impact=[&](int index,float speed){auto&c=list[size_t(index)];if(speed<.65f||c.impactCooldown>0||c.broken)return;
  const auto&t=propMaterialTraits(c.material());SoundEvent event{c.impactSound(),c.pos+shift,std::clamp(speed*.12f,.12f,.85f)*t.impactGain,t.impactPitch,true,17,c.z+c.height()*.5f};
  event.owner=c.soundOwner;event.aiRadius=std::clamp(2.f+std::sqrt(c.mass())*speed,2.f,12.f);
  if(t.breakSpeed>0&&speed>t.breakSpeed)breakClutter(level,size_t(index),{c.velocity.x,c.velocity.y,c.vz});m_sounds.push_back(event);c.impactCooldown=.075f;};
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
  auto contact=[&](Vec2 actor,float feet,float height,float radius,float actorMass,Vec2 actorVelocity,auto&& move,auto&& hurt){if(!nearby(actor,feet,height,radius))return;Vec2 n{};float penetration;
   if(!overlap(actor-c.pos,radius,n,penetration))return;
   float closing=std::max(0.f,dot(c.velocity-actorVelocity,n));const bool falling=c.vz<0&&c.z>=feet+height-.12f;float speed=std::max(closing,falling?-c.vz:0.f);
   if(falling&&!c.everyOutlinePoint([&](Vec2 offset){auto p=c.pos+offset+shift;const auto&w=physicalWorld(p);return w.supportBelow(p.x,p.y,c.z+.025f,true)<feet+height;}))return;
   if(speed>=1.5f&&c.bodyImpactCooldown<=0){float damage=std::clamp((.5f*c.mass()*speed*speed-2.f)*.2f,0.f,80.f);if(damage>0){hurt(damage,n);c.bodyImpactCooldown=.25f;c.projectile=false;impact(index,speed);}}
   if(c.broken)return;if(falling){c.z=feet+height;c.vz=std::max(0.f,-c.vz*.12f);return;}
   float moved=0;if(mass>=8&&closing>.001f){float bodySpeed=1.15f*closing*mass/(mass+actorMass);auto delta=n*(penetration+.001f+bodySpeed*.08f);moved=std::max(0.f,dot(move(delta),n));}
   float left=std::max(0.f,penetration+.001f-moved);auto next=c.pos-n*left;if(left>0&&sweepFits(c.pos,next))c.pos=next;
   if(closing>.001f){float keep=(mass-.15f*actorMass)/(mass+actorMass);if(moved<penetration*.5f)keep=-.15f;c.velocity=c.velocity-n*(closing*(1-keep));c.sleeping=false;c.restTime=0;}};
  contact(m_player.pos-shift,m_player.z,m_player.hullHeight(),.2f,70.f,m_velocity,[&](Vec2 delta){auto before=m_player.pos;tryMove(delta);return m_player.pos-before;},[&](float damage,Vec2){receiveDamage(damage,c.pos+shift,false,false);});
  for(size_t a=0;a<m_friendlyActors.size();++a){auto&npc=m_friendlyActors[a];const auto&track=npc.track();auto offset=chunkOffset(npc.level)-chunkOffset(level);auto point=npc.pose.position+offset;if(!nearby(point,npc.pose.z,track.scale,.2f))continue;if(npc.pose.clip==4||npcHeld(a)||!autonomousActor(track)||(track.ai.enableState&&!state(track.ai.enableState)))continue;float height=friendlyHullHeight(a,track);
   contact(point,npc.pose.z,height,.2f,70.f,Vec2{},[&](Vec2 delta){auto before=npc.pose.position;const int steps=std::max(1,int(std::ceil(length(delta)/.04f)));Vec2 reached=before;for(int i=1;i<=steps;++i){const auto next=before+delta*(float(i)/steps);if(!friendlyBodyFits(npc.level,next,npc.pose.z,height))break;reached=next;}npc.pose.position=reached;return npc.pose.position-before;},[&](float damage,Vec2){auto p=c.pos+shift;damageFriendly(a,damage,p+chunkOffset(m_level)-chunkOffset(npc.level),attacker);});}
  for(int target=0;target<chunkCount();++target)if(chunkResident(target)){auto offset=chunkOffset(target)-chunkOffset(level);auto&enemies=target==m_level?m_enemies:m_chunks[target].enemies;for(auto&enemy:enemies)if(enemy.alive){
   contact(enemy.pos+offset,enemy.bodyBottom(),enemy.bodyTop()-enemy.bodyBottom(),enemy.radius(),70.f*enemy.scale*enemy.scale*enemy.scale,Vec2{},[&](Vec2 delta){auto before=enemy.pos;const int steps=std::max(1,int(std::ceil(length(delta)/.04f)));Vec2 reached=before;for(int i=1;i<=steps;++i){const auto next=before+delta*(float(i)/steps);if(!hullFits(next+chunkOffset(target)-chunkOffset(m_level),enemy.z,enemy.hull(),enemy.radius(),false,false))break;reached=next;}enemy.pos=reached;return enemy.pos-before;},
    [&](float damage,Vec2){enemy.awareness=6;enemy.recognition=1;enemy.lastKnown=c.pos-offset;enemy.lastKnownZ=c.z;enemy.state=Enemy::State::Investigate;if(applyEnemyDamage(enemy,damage,false,false,target)){auto audible=enemy;audible.pos+=chunkOffset(target)-chunkOffset(m_level);enemySound(audible,2);}});}}
 };
 m_rigidStats.events.clear();m_rigidStats.manifoldsSeen.clear();
 std::vector<int> bodyOf(list.size(),-1);std::vector<char> wasAsleep;std::vector<V3> before;
 for(int step=0;step<steps;++step){
  space.bodies.clear();std::fill(bodyOf.begin(),bodyOf.end(),-1);
  for(int i=0;i<int(list.size());++i){if(!inPlay(i))continue;const auto&c=list[size_t(i)];Body b;toBody(c,b);b.user=i;
   if(i==held){b.kinematic=true;b.setMass(0);b.sleeping=false;b.w={};
    if(m_vrInputActive&&m_vrHandValid[m_vrCarryHand])b.v=m_vrHandVelocity[m_vrCarryHand];else b.v={m_carryHandVelocity.x,m_carryHandVelocity.y,m_carryHandVz};}
   if(c.material()==PropMaterial::Paper){b.linearDamping=3;b.angularDamping=3;}
   bodyOf[size_t(i)]=int(space.bodies.size());space.bodies.push_back(b);}
  // The player: a 70 kg body at his own velocity (no gravity, no turning, not written back). He shoves what
  // he walks into by contact, and what is pinned against a wall stops him in the solver rather than going
  // into the wall. What comes at him fast is left to bodyContacts (the blow, its damage).
  if(level==m_level&&!m_noclip&&!m_inspectionCamera){static Shape hull;static float hullHeight=-1;const float h=m_player.hullHeight();
   if(hull.count==0||hullHeight!=h){hull=Shape{};hull.add(PartKind::Box,{},{.2f,.2f,h*.5f});hull.finish();hullHeight=h;}
   Body p;p.shape=&hull;p.x={m_player.pos.x,m_player.pos.y,m_player.z+h*.5f};p.v={m_velocity.x,m_velocity.y,0};p.gravity={};p.user=PlayerBody;
   p.invMass=1/70.f;p.invInertiaLocal=M3::zero();p.inertiaLocal=M3::zero();p.invInertia=M3::zero();p.material={.6f,.5f,0,0};p.linearDamping=0;p.angularDamping=0;space.bodies.push_back(p);}
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
     float under=0,all=0;V3 middle{};const M3 turn=b.q.matrix();
     for(int p=0;p<b.shape->count;++p){const auto&part=b.shape->parts[size_t(p)];const V3 half=part.kind==PartKind::Cylinder?V3{part.half.x*.886f,part.half.x*.886f,part.half.z}:part.kind==PartKind::Sphere?V3{part.half.x*.806f,part.half.x*.806f,part.half.x*.806f}:part.half;
      const float vol=half.x*half.y*half.z;const M3 r=turn*part.turn.matrix();
      const float tall=(std::fabs(r.m[2][0])*half.x+std::fabs(r.m[2][1])*half.y+std::fabs(r.m[2][2])*half.z)*2.f/3.f;   // a cell's height, upright
      for(int i=-1;i<=1;++i)for(int j=-1;j<=1;++j)for(int k=-1;k<=1;++k){const V3 l=part.centre+part.turn.rotate(V3{half.x*float(i)*.667f,half.y*float(j)*.667f,half.z*float(k)*.667f});const V3 at=b.toWorld(l);
       const float wet=std::clamp((surface-(at.z-tall*.5f))/std::max(1e-4f,tall),0.f,1.f);all+=vol;if(wet>0){under+=vol*wet;middle=middle+at*(vol*wet);}}}
     const float f=all>0?under/all:0.f;
     if(f>0){middle=middle*(1/under);const float rho=buoyancy::density(c),k=buoyancy::drag(c)*tune("water.drag",1.f);const bool floater=rho<buoyancy::WaterDensity;
      const float mass=b.invMass>0?1/b.invMass:1;const float height=std::max(.01f,c.height());
      // Lifted at the middle of what is under (so a thing half in tips as it should), drawn halfway to its own
      // middle (water.lever): a prop's mass is in its walls, not spread as a solid block's.
      b.applyForce({0,0,mass*buoyancy::Gravity*buoyancy::WaterDensity/rho*f},b.x+(middle-b.x)*tune("water.lever",.5f));
      const float horizontal=std::sqrt(b.v.x*b.v.x+b.v.y*b.v.y);const float hk=std::exp(-(1.1f+k*horizontal)*f*RigidStep);b.v.x*=hk;b.v.y*=hk;
      const float bob=floater?2*tune("water.bob_damping",.2f)*std::sqrt(buoyancy::Gravity*buoyancy::WaterDensity/(rho*height)):tune("water.sink_drag",2.6f);
      b.v.z*=std::exp(-(1.1f+bob+k*std::fabs(b.v.z))*f*RigidStep);b.w=b.w*std::exp(-2.6f*f*RigidStep);
      const float bed=w.supportBelow(q.x,q.y,bottom+.025f,true);const bool afloat=floater&&surface-bed>height*rho/buoyancy::WaterDensity*.9f;
      if(afloat&&f<.999f){
       // Afloat: a prop's mass lies in its walls (a crate's planks, a box's card), so it rides on a face, not
       // on a corner as a solid block of its density would: the water rights it toward its nearest face.
       // A swell rocks it a few degrees and heaves it, a slow drift carries it (each on its own phase).
       V3 axis{};float best=-1;for(int a=0;a<3;++a){const V3 v=turn.column(a);const float d=std::fabs(v.z);if(d>best){best=d;axis=v.z<0?neg(v):v;}}
       b.w=b.w+cross(axis,V3{0,0,1})*(tune("water.right",60.f)*RigidStep);
       const float phase=float(index%97)*.61f,swell=tune("water.swell",1.f);
       b.w=b.w+V3{std::cos(c.yaw),std::sin(c.yaw),0}*(1.5f*swell*std::sin(1.6f*m_elapsed+phase*1.7f+1.f)*RigidStep)+V3{-std::sin(c.yaw),std::cos(c.yaw),0}*(1.5f*swell*std::sin(1.9f*m_elapsed+phase)*RigidStep);
       b.v.z+=.45f*swell*std::sin(2.4f*m_elapsed+phase*2.3f)*RigidStep;const float drift=tune("water.drift",.03f);
       b.v.x+=std::cos(.23f*m_elapsed+phase)*drift*RigidStep;b.v.y+=std::sin(.19f*m_elapsed+phase*1.3f)*drift*RigidStep;b.sleepTime=0;}}}}
   // A fast prop's way ahead this step: the panes it would pass (a thrown thing meets glass at its speed).
   const float speed=len(b.v);if(speed*RigidStep>.01f){Vec2 q2{b.x.x,b.x.y};q2=q2+shift;const World& rw=physicalWorld(q2);const int paneLevel=rw.level();auto&pw=paneLevel==m_level?m_world:m_chunks[paneLevel].world;
    const auto e=c.extent();const V3 delta=b.v*RigidStep;const float distance=len(delta);const V3 from{q2.x,q2.y,b.x.z};const V3 travel=delta*(1+std::max({e[0],e[1],e[2]})/distance);
    V3 n;ShotTrace trace;float t=pw.shotFraction(from,from+travel,n,&trace);const float m=c.mass();
    for(int j=0;j<2;++j)if(trace.passed[j]>=0&&trace.passedAt[j]<t){const auto pn=trace.passedNormal[j];const float closing=std::fabs(dot(b.v,pn));
     const float keep=strike(index,paneLevel,trace.passed[j],.5f*m*closing*closing,from+travel*trace.passedAt[j],delta*(1/distance));if(keep>0){b.v=b.v*keep;}else break;}
    if(t<1&&trace.blocking>=0){const float closing=std::fabs(dot(b.v,n));const float keep=strike(index,paneLevel,trace.blocking,.5f*m*closing*closing,from+travel*t,delta*(1/distance));if(keep>0)b.v=b.v*keep;}}
  }
  // ---- The step ----
  space.step(RigidStep);
  // ---- Back to the props ----
  for(size_t bi=0;bi<space.bodies.size();++bi){const auto&b=space.bodies[bi];if(b.kinematic||b.user<0)continue;auto&c=list[size_t(b.user)];
   if(wasAsleep[bi]&&b.sleeping)continue;fromBody(b,c);c.island=b.sleeping?space.islandOf.empty()?-1:std::max(0,space.islandOf[bi]):-1;
   if(len(b.v)<.5f)c.projectile=false;}
  for(const auto&ev:space.events){m_rigidStats.events.push_back(ev);if(space.bodies[size_t(ev.a)].user<0||(ev.b>=0&&space.bodies[size_t(ev.b)].user<0))continue;
   const int a=space.bodies[size_t(ev.a)].user,bUser=ev.b>=0?space.bodies[size_t(ev.b)].user:-1;
   auto&ca=list[size_t(a)];
   // Whoever came in faster (before the blow) gives both its sound's owner (a guard's kick stays his).
   if(bUser>=0){auto&cb=list[size_t(bUser)];const int owner=len(before[size_t(ev.a)])>=len(before[size_t(ev.b)])?ca.soundOwner:cb.soundOwner;ca.soundOwner=cb.soundOwner=owner;}
   const bool aKinematic=space.bodies[size_t(ev.a)].kinematic,bKinematic=ev.b>=0&&space.bodies[size_t(ev.b)].kinematic;
   if(!aKinematic)impact(a,ev.approach);
   if(bUser>=0&&!bKinematic){auto&cb=list[size_t(bUser)];const float bs=propMaterialTraits(cb.material()).breakSpeed;if(bs>0&&ev.approach>bs)breakClutter(level,size_t(bUser),{ev.normal.x,ev.normal.y,ev.normal.z});else if(aKinematic)impact(bUser,ev.approach);}
   if(aKinematic)continue;
   // Against the world: was it a pane? It takes the energy the contact brought (its effective mass).
   if(ev.b<0&&!ca.broken){Vec2 q{ev.point.x,ev.point.y};q=q+shift;const World& pw=physicalWorld(q);const auto&panes=pw.breakables();
    for(int pi=0;pi<int(panes.size());++pi){const auto&pn=panes[size_t(pi)];if(!pw.breakableIntact(pi))continue;
     if(q.x<pn.x1-.04f||q.x>pn.x2+.04f||q.y<pn.y1-.04f||q.y>pn.y2+.04f||ev.point.z<pn.bottom-.04f||ev.point.z>pn.top+.04f)continue;
     const float meff=ev.effectiveMass>0?ev.effectiveMass:ca.mass();const float keep=strike(a,pw.level(),pi,.5f*meff*ev.approach*ev.approach,{q.x,q.y,ev.point.z},ev.normal);
     if(keep>0){const V3 v=before[size_t(ev.a)]*keep;ca.velocity={v.x,v.y};ca.vz=v.z;ca.sleeping=false;ca.restTime=0;}break;}}
  }
  // Bodies struck by what moves.
  for(size_t bi=0;bi<space.bodies.size();++bi){const auto&b=space.bodies[bi];if(b.kinematic||b.sleeping||b.user<0)continue;bodyContacts(b.user);}
  for(int i=0;i<int(list.size());++i)if(list[size_t(i)].broken){auto&c=list[size_t(i)];c.velocity={};c.vz=c.spin=c.pitchSpeed=c.rollSpeed=0;c.sleeping=true;c.restTime=0;}
 }
 rp.cache.swap(space.cache);
 int awake=0;for(const auto&b:space.bodies)awake+=b.movable()&&b.user>=0;
 m_rigidStats.bodies=int(space.bodies.size());m_rigidStats.awake=awake;m_rigidStats.manifolds=int(space.manifolds.size());m_rigidStats.worldManifolds=space.worldManifolds;m_rigidStats.steps=steps;
 m_rigidStats.manifoldsSeen=space.manifolds;
 m_rigidStats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
}
}

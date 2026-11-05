#include "Electricity.h"
#include "Game.h"
#include "EngineRegressionScene.h"
#include "../world/CampaignKit.h"
#include "../world/ModelIds.h"
#include <cstring>
#include <mutex>
#include <ostream>
#include <unordered_map>

namespace retro::electric {
namespace {
unsigned mix(unsigned h){h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return h;}
unsigned bits(float v){unsigned u;std::memcpy(&u,&v,4);return u;}
struct Rng{unsigned s;explicit Rng(unsigned seed):s(mix(seed^0x9e3779b9u)){}float next(){s=mix(s+0x6d2b79f5u);return float(s>>8)*(1.f/16777216.f);}float signed1(){return next()*2-1;}};
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float span(Point3 v){return std::sqrt(dot3(v,v));}
Point3 unit(Point3 v,Point3 fallback={0,0,1}){const float l=span(v);return l>1e-6f?v*(1/l):fallback;}
// Any unit vector square to the unit `d`.
Point3 square(Point3 d,Rng& r){Point3 v{r.signed1(),r.signed1(),r.signed1()};v=v-d*dot3(v,d);float l=span(v);if(l<1e-3f){v=cross(d,std::fabs(d.z)<.9f?Point3{0,0,1}:Point3{1,0,0});l=span(v);}return v*(1/std::max(l,1e-6f));}
constexpr float Gravity=9.81f;
// A spark mid-flight: launched with its life and starting heat; sliding along a floor or settled.
struct Bead {Point3 p{},v{};float life=0,heat=0;bool ember=false,dead=false,sliding=false,resting=false;};
// A burst simulated so far: the state at step `step` and the one before it (frames between steps lerp).
struct Run {std::vector<Bead> previous,current;int step=0;float touched=0;};
std::mutex cacheMutex;
std::unordered_map<std::uint64_t,Run> runs;
std::unordered_map<std::uint64_t,Arc> lanes;
void launch(const Burst& b,std::vector<Bead>& out){
 Rng r(b.seed);const Point3 axis=unit(b.along);const Point3 t1=square(axis,r),t2=cross(axis,t1);out.clear();
 for(int i=0;i<b.count;++i){const bool ember=i>=b.count-b.embers;
  // Directions spread evenly over the cone's solid angle; speeds mostly middling with a few fast ones.
  const float theta=b.spread*std::sqrt(r.next()),phi=r.next()*kTwoPi,u=r.next();const Point3 d=axis*std::cos(theta)+(t1*std::cos(phi)+t2*std::sin(phi))*std::sin(theta);
  Bead s;s.p=b.at+d*.004f;s.v=d*(b.speed*(ember?.22f+.3f*u:.35f+1.15f*u*u+.15f*r.next()));s.ember=ember;
  s.life=ember?1.5f+1.5f*r.next():.32f+.55f*r.next();s.heat=b.heat*(ember?.6f+.2f*r.next():.86f+.14f*r.next());out.push_back(s);}
}
// Off a surface of normal `n`: the part into it turned back with `bounce`, the rest kept by `keep`.
void reflect(Bead& s,Point3 n,float bounce,float keep){const float into=dot3(s.v,n);if(into>=0)return;const Point3 along=s.v-n*into;s.v=along*keep-n*(into*bounce);}
void advance(const World& w,const Burst& b,Bead& s,float age){
 if(s.dead||s.resting)return;if(age>=s.life){s.dead=true;return;}
 constexpr float h=SimStep;const float bounce=s.ember?.28f:.34f,keep=s.ember?.8f:.68f;
 // A burning bead is tiny: air drag (quadratic in speed) takes most of its pace within a few tenths.
 const float speed=span(s.v);s.v=s.v*std::max(0.f,1-(s.ember?.7f:1.5f)*speed*h);
 if(s.sliding){const float slow=(s.ember?.16f:.55f)*Gravity*h,flat=std::sqrt(s.v.x*s.v.x+s.v.y*s.v.y);s.v.z=0;
  if(flat<=slow+.02f){s.v={};s.resting=true;return;}s.v=s.v*((flat-slow)/flat);}
 else s.v.z-=Gravity*h;
 Point3 from=s.p,to=from+s.v*h,n;
 // Walls, solids, ceilings and doors: the ballistic trace, so a spark never goes through one.
 const float f=w.shotFraction(from,to,n);
 if(f>1e-4f&&f<1&&dot3(n,s.v)<0){to=from+(to-from)*f+n*.002f;reflect(s,n,bounce,keep);}
 // The floor or whatever stands on it (the trace knows only a flat slab): land, bounce, skitter.
 const float floor=w.supportBelow(to.x,to.y,std::max(from.z,to.z)+.01f);
 if(s.sliding){if(floor<to.z-.03f)s.sliding=false;else to.z=floor+.0015f;}
 else if(to.z<floor+.0015f){to.z=floor+.0015f;reflect(s,{0,0,1},bounce,keep);if(s.v.z<.45f){s.v.z=0;s.sliding=true;}}
 // Into water it goes out.
 if(to.z<w.waterSurface(to.x,to.y)){s.dead=true;return;}
 // Off the prop it came from, out through the nearest face.
 if(b.bodied){const float depth=leadBodyDepth(b.body,to);if(depth>0){const auto d=to-b.body.centre;int face=0;float least=1e9f,side=1;
  for(int j=0;j<3;++j){const float v=dot3(d,b.body.axis[j]),room=b.body.half[j]-std::fabs(v);if(room<least){least=room;face=j;side=v<0?-1.f:1.f;}}
  const Point3 normal=b.body.axis[face]*side;to=to+normal*(least+.002f);reflect(s,normal,bounce,keep);if(normal.z>.7f&&s.v.z<.45f){s.v.z=0;s.sliding=true;}}}
 s.p=to;
}
float heatAt(const Bead& s,float age){const float t=std::clamp(age/s.life,0.f,1.f);
 // Sparks stay white-yellow most of their flight and redden fast at its end; embers glow orange down to dull red.
 return s.ember?s.heat*std::pow(1-t,.8f):s.heat*(1-std::pow(t,1.6f));}
std::uint64_t runKey(const Burst& b){return (std::uint64_t(b.seed)<<32)^bits(b.time)^(std::uint64_t(unsigned(b.count))<<20);}
}

float burstLife(const Burst& b){return b.embers>0?3.1f:.9f;}
void resetCaches(){std::lock_guard lock(cacheMutex);runs.clear();lanes.clear();}

int sparks(const World& w,const Burst& b,float now,std::vector<Spark>& out,int limit){
 const float age=now-b.time;if(age<0||age>burstLife(b)||limit<=0)return 0;
 std::lock_guard lock(cacheMutex);
 if(runs.size()>96)for(auto i=runs.begin();i!=runs.end();)i=std::fabs(i->second.touched-now)>6.f?runs.erase(i):std::next(i);
 auto&run=runs[runKey(b)];
 // Fixed steps from the burst's own time: asked for an earlier time than it has run to, it starts over.
 const int want=int(std::floor(age/SimStep))+1;
 if(run.current.empty()||run.step>want){launch(b,run.current);run.previous=run.current;run.step=0;}
 for(;run.step<want;++run.step){run.previous=run.current;for(auto&s:run.current)advance(w,b,s,float(run.step)*SimStep);}
 run.touched=now;const float blend=std::clamp((age-float(want-1)*SimStep)/SimStep,0.f,1.f);int added=0;
 for(size_t i=0;i<run.current.size()&&added<limit;++i){const auto&a=run.previous[i];const auto&c=run.current[i];if(c.dead&&a.dead)continue;if(age>=c.life)continue;
  const float heat=heatAt(c,age);if(heat<.02f)continue;
  Spark s;s.p=a.p+(c.p-a.p)*blend;s.v=c.dead?a.v:c.v;s.heat=heat;s.size=c.ember?1.f:.45f+.55f*heat;s.ember=c.ember;s.resting=c.resting||(c.sliding&&span(c.v)<.05f);out.push_back(s);++added;}
 return added;
}

float strike(const Arc& a,float now){
 // Each arc keeps its own phase, so neighbours never strike together; one strike in eight misses (the flicker).
 const float t=now*StrikeRate+float(mix(a.seed)&1023u)/1024.f,slot=std::floor(t),into=t-slot;
 const unsigned h=mix(a.seed^unsigned(std::int64_t(slot))*2654435761u);if((h&7u)==0)return 0.f;
 // A stroke flares as it strikes and sags through its slot.
 return (.55f+.45f*float((h>>8)&255u)/255.f)*(1-.55f*into);
}

void bolt(const Arc& a,float now,int detail,std::vector<Piece>& out){
 const float t=now*StrikeRate+float(mix(a.seed)&1023u)/1024.f;Rng r(a.seed*2654435761u^unsigned(std::int64_t(std::floor(t)))*40503u);detail=std::clamp(detail,2,6);
 // A fork leaves the channel at a midpoint, roughly onward and to one side, shorter and thinner, and dies in the air.
 auto fork=[&](auto& self,Point3 p,Point3 q,float offset,int depth,float w0,float w1)->void{
  if(depth<=0){out.push_back({p,q,(w0+w1)*.5f});return;}const Point3 d=unit(q-p);const Point3 m=(p+q)*.5f+square(d,r)*(offset*r.signed1());
  self(self,p,m,offset*.5f,depth-1,w0,(w0+w1)*.5f);self(self,m,q,offset*.5f,depth-1,(w0+w1)*.5f,w1);};
 // The main channel: each piece's midpoint pushed aside by half the last push, forks off the coarse levels.
 auto channel=[&](auto& self,Point3 p,Point3 q,float t0,float t1,float offset,int depth)->void{
  auto width=[](float t){return 1-.45f*t;};
  if(depth<=0){out.push_back({p,q,width((t0+t1)*.5f)});return;}
  const Point3 d=unit(q-p);const float length=span(q-p);const Point3 m=(p+q)*.5f+square(d,r)*(offset*r.signed1());const float tm=(t0+t1)*.5f;
  if(depth>=detail-2&&r.next()<.42f){const Point3 dir=unit(d*.75f+square(d,r)*.85f);fork(fork,m,m+dir*(length*(.45f+.6f*r.next())),length*.18f,std::max(1,depth-2),width(tm)*.5f,width(tm)*.12f);}
  self(self,p,m,t0,tm,offset*.5f,depth-1);self(self,m,q,tm,t1,offset*.5f,depth-1);};
 if(a.open){
  // Nothing in reach: a brush of current spitting off the end into the air.
  const int brush=3+int(r.next()*3.f);for(int i=0;i<brush;++i){const Point3 dir=unit(Point3{r.signed1(),r.signed1(),r.signed1()*.7f-.15f});const float length=(.035f+.07f*r.next())*a.width;
   fork(fork,a.from,a.from+dir*length,length*.22f,std::max(2,detail-2),.8f,.15f);}
  return;}
 const float length=span(a.to-a.from);if(length<1e-4f)return;
 // The channel bows: current crawling over a floor stands up off it in the middle.
 const Point3 middle=(a.from+a.to)*.5f+Point3{0,0,a.bow}+square(unit(a.to-a.from),r)*(length*.12f*r.signed1());
 channel(channel,a.from,middle,0,.5f,length*.11f,detail-1);channel(channel,middle,a.to,.5f,1,length*.11f,detail-1);
}

bool conductor(const World& w,Point3 from,float reach,unsigned seed,Point3& to){
 Rng r(seed);float best=reach;bool found=false;const float turn=r.next()*kTwoPi;
 // Rays all round (turned afresh each strike so the arc hunts), then the floor and water straight below.
 for(int i=0;i<14;++i){const float yaw=turn+float(i%8)*kTwoPi/8,lift=i<8?r.signed1()*.35f:i<11?.85f:-.85f;const float flat=std::sqrt(1-lift*lift);
  const Point3 dir{std::cos(yaw+float(i/8)*.7f)*flat,std::sin(yaw+float(i/8)*.7f)*flat,lift};Point3 n;const float f=w.shotFraction(from,from+dir*reach,n);
  const float d=f*reach*(1+.25f*r.next());if(f<1&&f*reach>.012f&&d<best){best=d;to=from+dir*(f*reach);found=true;}}
 float below=w.supportBelow(from.x,from.y,from.z);const float water=w.waterSurface(from.x,from.y);if(water>below&&water<=from.z)below=water;
 const float drop=from.z-below;if(drop>.012f&&drop<best){best=drop;to={from.x+r.signed1()*drop*.25f,from.y+r.signed1()*drop*.25f,below+.002f};found=true;}
 return found;
}

Flash flash(const Burst& b,float now){
 // A shower's pop: bright for a few hundredths of a second, warm, gone in a fifth.
 const float age=now-b.time;if(age<0||age>.2f)return {};
 static const Point3 warm=WorldLight::tone(2300,0,{1,1,1});
 return {b.at+b.along*.05f,b.heat*std::clamp(float(b.count)/28.f,.25f,1.3f)*std::exp(-age/.045f)*.9f,warm,4.5f};
}
Flash flash(const Arc& a,float now){
 const float lit=strike(a,now)*a.strength;if(lit<=0)return {};
 // Blue-white, a little violet off a cell of resonance ore.
 static const Point3 cold=WorldLight::tone(11000,0,{.82f,.92f,1}),violet=WorldLight::tone(11000,-.2f,{.86f,.84f,1});
 return {a.open?a.from:(a.from+a.to)*.5f+Point3{0,0,a.bow*.5f},lit*1.15f*std::min(1.f,a.width),a.resonance?violet:cold,7.f};
}

void hazardArcs(const World& w,const Hazard& h,int index,float now,std::vector<Arc>& out,std::vector<Burst>* sparks){
 const float area=(h.x2-h.x1)*(h.y2-h.y1);const bool pulsed=h.period>0;
 const int count=pulsed?std::clamp(int(area/4.f),2,8):std::clamp(int(area/6.f),1,4);
 const unsigned seed=mix(unsigned(int(h.x1*8))*73856093u^unsigned(int(h.y1*8))*19349663u^unsigned(index)*83492791u^unsigned(w.level()+1)*2654435761u);
 for(int lane=0;lane<count;++lane){const unsigned ls=mix(seed+unsigned(lane)*977u);
  // A lane hops to new contacts every spell. Live water crackles in sparse spells, so a wide basin reads
  // as water that bites rather than a bed of arcs; a small steady fault (a burst breaker) and a floor
  // live in pulses are busy while they are on.
  const float spell=pulsed?.28f:.42f,phase=float(ls&1023u)/1024.f;const int epoch=int(std::floor(now/spell+phase));const unsigned e=mix(ls^unsigned(epoch)*2654435761u);
  if((e&7u)>=(pulsed?7u:area<3?6u:3u))continue;
  // Where a spell's arc lands on metal it spits a few sparks back as it strikes.
  auto spit=[&](const Arc& a){if(!sparks||!a.solid)return;Burst b;b.at=a.to;b.along=unit(a.from-a.to+Point3{0,0,.3f});b.time=(float(epoch)-phase)*spell;b.count=6;b.embers=0;b.speed=2.2f;b.spread=1.1f;b.heat=.9f;b.seed=e;b.level=w.level();sparks->push_back(b);};
  const std::uint64_t key=(std::uint64_t(ls)<<32)^unsigned(epoch);Arc a;
  {std::lock_guard lock(cacheMutex);auto found=lanes.find(key);if(found!=lanes.end()){if(found->second.strength>0){out.push_back(found->second);spit(found->second);}continue;}}
  Rng r(e);a.seed=e;a.level=w.level();a.strength=0;
  auto surface=[&](float x,float y){float z=w.supportBelow(x,y,h.top);const float water=w.waterSurface(x,y);if(water>z&&water<h.top)z=water;return std::max(z,h.bottom)+.004f;};
  const float x=h.x1+(h.x2-h.x1)*(.1f+.8f*r.next()),y=h.y1+(h.y2-h.y1)*(.1f+.8f*r.next());a.from={x,y,surface(x,y)};bool metal=false;
  // Up to the metal beside it when there is some in reach...
  if(r.next()<.5f){float best=1.1f;const float turn=r.next()*kTwoPi;const Point3 start=a.from+Point3{0,0,.06f};
   for(int i=0;i<8;++i){const float yaw=turn+float(i)*kTwoPi/8,lift=.15f+.4f*r.next();const Point3 dir=unit({std::cos(yaw),std::sin(yaw),lift});Point3 n;const float f=w.shotFraction(start,start+dir*1.1f,n);
    if(f<1&&f*1.1f>.15f&&f*1.1f<best){best=f*1.1f;a.to=start+dir*(f*1.1f)+n*.003f;metal=true;}}}
  // ...or across to another point of the floor or water.
  if(!metal){const float yaw=r.next()*kTwoPi,reach=.35f+.6f*r.next();const float tx=std::clamp(x+std::cos(yaw)*reach,h.x1,h.x2),ty=std::clamp(y+std::sin(yaw)*reach,h.y1,h.y2);a.to={tx,ty,surface(tx,ty)};}
  const float length=span(a.to-a.from);
  if(length>=.18f&&std::fabs(a.to.z-a.from.z)<1.2f){a.bow=metal?.04f*length:.05f+.1f*length;a.width=1;a.strength=pulsed?1.f:.85f;a.solid=metal;}
  {std::lock_guard lock(cacheMutex);if(lanes.size()>4096)lanes.clear();lanes[key]=a;}
  if(a.strength>0){out.push_back(a);spit(a);}
 }
}
}

namespace retro {
using electric::Burst;using electric::Arc;using electric::mix;using electric::bits;using electric::unit;using electric::span;using electric::burstLife;
namespace {
// Showers in flight, oldest dropped first past the cap (each lives at most burstLife).
void record(std::vector<Burst>& bursts,const Burst& b){bursts.push_back(b);if(bursts.size()>96)bursts.erase(bursts.begin());}
}
void Game::sparkBurst(int level,Point3 at,Point3 along,float heat,int count,bool smoke){
 Burst b;b.at=at;b.along=along;b.time=m_elapsed;b.heat=std::clamp(heat,0.f,1.f);b.count=std::clamp(count,1,64);b.embers=std::max(1,b.count/8);b.smoke=smoke;b.level=level;
 b.seed=mix(unsigned(level+1)*0x9e3779b1u^bits(m_elapsed)^bits(at.x)*31u^bits(at.y)*17u^unsigned(m_sparkBursts.size()));record(m_sparkBursts,b);++m_electricEdits;
}
void Game::strikeArc(int level,Point3 from,Point3 to,float seconds,float strength,bool resonance){
 Arc a;a.from=from;a.to=to;a.until=m_elapsed+std::max(0.f,seconds);a.strength=strength;a.resonance=resonance;a.level=level;a.seed=mix(bits(from.x)^bits(to.y)*7u^bits(m_elapsed)*13u);
 m_timedArcs.push_back(a);if(m_timedArcs.size()>16)m_timedArcs.erase(m_timedArcs.begin());++m_electricEdits;
}
void Game::updateElectricity(float dt){
 const float now=m_elapsed;
 m_sparkBursts.erase(std::remove_if(m_sparkBursts.begin(),m_sparkBursts.end(),[&](const Burst&b){return now-b.time>burstLife(b)||b.time>now+1;}),m_sparkBursts.end());
 m_timedArcs.erase(std::remove_if(m_timedArcs.begin(),m_timedArcs.end(),[&](const Arc&a){return now>a.until;}),m_timedArcs.end());
 if(dt<=0||!electric::shown)return;
 // Where a hazard's arc lands on metal it spits sparks back as its spell strikes: recorded here as a
 // shower of its own, so every spark flies out its life though the spell (or the pulse) that threw it is over.
 {std::vector<Arc> arcs;std::vector<Burst> spits;
  for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&w=chunkWorld(level);
   for(int i=0;i<int(w.hazards().size());++i){const auto&h=w.hazards()[size_t(i)];if(h.kind!=Hazard::Kind::Electricity||!hazardActive(h))continue;
    arcs.clear();spits.clear();electric::hazardArcs(w,h,i,now,arcs,&spits);
    for(auto b:spits)if(b.time>now-dt-1e-5f&&b.time<=now+1e-5f&&std::none_of(m_sparkBursts.begin(),m_sparkBursts.end(),[&](const Burst&o){return o.seed==b.seed;})){b.level=level;record(m_sparkBursts,b);}}}}
 // Each burst of a torn live lead's lead_arc clip throws its shower from the end as it begins:
 // the zap off the clip (out of the face it came from), then the spits off the dangling end.
 for(int level=0;level<chunkCount();++level)if(chunkResident(level)){const auto&w=chunkWorld(level);
  for(size_t i=0;i<w.leads().size()&&i<w.leadRopes().size();++i){const auto&l=w.leads()[i];const auto&r=w.leadRopes()[i];if(!l.live||!r.torn||r.points.size()<2)continue;
   float since=0;const float age=now-r.tornAt,arc=leadArc(age,&since);if(arc<=0||since>=dt)continue;
   const float start=r.tornAt+(age-since);const unsigned seed=mix(unsigned(level+1)*0x85ebca77u^unsigned(i)*0xc2b2ae3du^bits(start));
   if(std::any_of(m_sparkBursts.begin(),m_sparkBursts.end(),[&](const Burst&b){return b.seed==seed;}))continue;
   const Point3 tip=r.points.back(),line=unit(tip-r.points[r.points.size()-2]);const bool zap=arc>=1;Burst b;b.at=tip;b.time=start;b.seed=seed;b.level=level;
   b.along=zap?unit(line*-1.f+Point3{0,0,.6f}):unit(line+Point3{0,0,.35f});b.spread=zap?1.05f:1.25f;b.speed=zap?3.4f:2.5f;b.heat=zap?1.f:.92f;b.count=zap?36:12;b.embers=zap?6:2;b.smoke=zap;
   if(const int k=leadProp(level,l,-1);k>=0){b.body=leadBody(chunkClutter(level)[size_t(k)]);b.bodied=true;}
   record(m_sparkBursts,b);}}
}
const electric::Scene& Game::electricNow(int level)const{
 // Keyed on the clock, where the player stands and every electric event since (showers, arcs, rounds
 // striking): any of them moving starts a new frame.
 std::uint64_t key=bits(m_elapsed);
 for(std::uint64_t v:{std::uint64_t(electric::shown),std::uint64_t(bits(m_player.pos.x)),std::uint64_t(bits(m_player.pos.y)),std::uint64_t(unsigned(m_level)),std::uint64_t(m_electricEdits),std::uint64_t(m_bulletImpacts.size()),std::uint64_t(m_sparkBursts.size())})key=(key^v)*0x100000001b3ull+0x9e3779b97f4a7c15ull;
 auto&f=m_electricFrame;const size_t chunks=size_t(std::max(0,chunkCount()));
 if(f.key!=key||f.scenes.size()!=chunks){f.key=key;f.scenes.resize(chunks);f.made.assign(chunks,0);f.lit=false;}
 static const electric::Scene none;if(level<0||size_t(level)>=chunks)return none;
 auto&scene=f.scenes[size_t(level)];
 if(!f.made[size_t(level)]){scene.bursts.clear();scene.arcs.clear();scene.flashes.clear();electricScene(level,scene);f.made[size_t(level)]=1;}
 return scene;
}
void Game::electricScene(int level,electric::Scene& out)const{
 if(!electric::shown||!chunkResident(level))return;const auto&w=chunkWorld(level);const float now=m_elapsed;
 for(const auto&b:m_sparkBursts)if(b.level==level&&now>=b.time&&now<b.time+burstLife(b))out.bursts.push_back(b);
 // A round striking a solid throws a short, fast shower back off it; its embers glow on where they land.
 for(const auto&mark:m_bulletImpacts){if(mark.level!=level||mark.blood||mark.tint!=0)continue;const float age=now-mark.time;if(age<0)continue;
  Burst b;b.at={mark.pos.x,mark.pos.y,mark.z};b.along=unit({mark.normal.x,mark.normal.y,mark.normalZ});b.time=mark.time;b.speed=4.2f;b.spread=.95f;b.heat=.95f;b.count=14;b.embers=2;b.level=level;
  if(age>burstLife(b))continue;b.seed=mix(bits(mark.pos.x)*3u^bits(mark.pos.y)*5u^bits(mark.z)*7u^bits(mark.time));out.bursts.push_back(b);}
 // A torn live lead arcs in time with its clip: from the end to the clip it came off while that is near,
 // else to the nearest thing that conducts, else a brush into the air.
 for(size_t i=0;i<w.leads().size()&&i<w.leadRopes().size();++i){const auto&l=w.leads()[i];const auto&r=w.leadRopes()[i];if(!l.live||!r.torn||r.points.size()<2)continue;
  const float arc=leadArc(now-r.tornAt);if(arc<=0)continue;
  Arc a;a.from=r.points.back();a.seed=mix(unsigned(level+1)*0x27d4eb2fu^unsigned(i)*0x165667b1u^bits(r.tornAt));a.strength=arc;a.width=arc>=1?1.f:.75f;a.level=level;bool found=false;const float reach=arc>=1?.55f:.38f;
  if(const int k=leadProp(level,l,-1);k>=0){const auto&c=chunkClutter(level)[size_t(k)];a.resonance=c.model==model::anl_cell_charged||c.model==model::anl_cell_core;
   const Point3 clip=leadClip(c,l.offset);const float gap=span(clip-a.from);if(gap>.012f&&gap<reach){a.to=clip;found=true;}}
  if(!found)found=electric::conductor(w,a.from,reach,a.seed^unsigned(std::int64_t(now*electric::StrikeRate)),a.to);
  if(!found)a.to=a.from;a.open=!found;out.arcs.push_back(a);}
 for(const auto&a:m_timedArcs)if(a.level==level&&now<=a.until)out.arcs.push_back(a);
 // Hazards' arcs; the sparks they spit are showers of their own (updateElectricity), already above.
 for(int i=0;i<int(w.hazards().size());++i){const auto&h=w.hazards()[size_t(i)];if(h.kind==Hazard::Kind::Electricity&&hazardActive(h))electric::hazardArcs(w,h,i,now,out.arcs);}
 for(const auto&b:out.bursts){auto f=electric::flash(b,now);if(f.strength>0)out.flashes.push_back(f);}
 for(const auto&a:out.arcs){auto f=electric::flash(a,now);if(f.strength>0)out.flashes.push_back(f);}
}
Game Game::electricInspection(float seconds,int view,float* tearAge){
 // A plain small room: a lead plugged into the west wall at 1.45 m, run to a charged cell on a desk.
 // Views 0 (the room) and 1 (close by): the cell is knocked along the desk away from the wall, the cable
 // draws taut and the clip tears. View 2: a round strikes the west wall beside the desk. View 3: a live
 // floor along the west wall, pulsed, its power switched on. `seconds` from the tear, the round or the power.
 auto m=regression::room();m->name="Electricity inspection";m->definition.ceiling=3.2f;m->definition.ambient=.16f;m->definition.playerStart={5.4f,6.1f};
 for(int y=0;y<24;++y){auto&row=m->layers[0].rows[size_t(y)];row=std::string(24,'#');if(y>=5&&y<=11)for(int x=1;x<=8;++x)row[size_t(x)]='.';}
 m->fixtures.push_back({model::desk,{2.75f,8.6f},0,1.4f,.7f,.75f,0,true});
 kit::prop(*m,model::anl_cell_charged,2.35f,8.6f,.75f,0);kit::lead(*m,{1.012f,8.6f,1.45f},0,{0,0,.41f},1.75f,8,true,.007f);
 m->lights.push_back({{5.2f,8.4f},3.1f,WorldLightMount::Ceiling,0,.85f,8});
 const StateId power=stateId("electric_inspection_power");
 // Pulsed 0.5 s in 1.2, its first pulse starting as the power comes on (tick 31).
 if(view==3){Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=1;h.x2=3;h.y1=5.2f;h.y2=11.8f;h.bottom=-1;h.top=2;h.enabledFlag=power;h.period=1.2f;h.onTime=.5f;h.phase=1.2f-31.f/120;m->hazards.push_back(h);}
 const auto campaign=regression::campaign(m);
 auto run=[&](int ticks,int* tore){Game g(campaign);g.m_weaponEquipped=false;g.m_holster=1;
  auto look=[&](Vec2 at,Point3 to,float pitch){g.m_player.pos=at;g.m_player.angle=std::atan2(to.y-at.y,to.x-at.x);g.m_player.pitch=pitch;};
  if(view==2)look({3.4f,6.6f},{1,7.6f,1},-35);else if(view==3)look({5.8f,8.5f},{1,8.5f,0},-42);else look(view==1?Vec2{3.9f,6.9f}:Vec2{5.4f,6.1f},{2.4f,8.55f,0},view==1?-14.f:-8.f);
  for(int t=0;t<ticks;++t){if(t==30&&view<2){auto&c=g.m_clutter[0];c.velocity={3.4f,.35f};c.vz=.6f;c.sleeping=false;}
   g.m_elapsed+=1.f/120;
   if(t==30&&view==2){g.m_bulletImpacts.push_back({{1.004f,7.6f},1.25f,{1,0},g.m_elapsed,0,false,0,0});}
   if(t==30&&view==3)g.setState(power,1);
   g.updateClutter({},1.f/120);if(tore&&g.m_world.leadRopes()[0].torn){*tore=t;break;}}
  return g;};
 int tore=30;if(view<2){tore=-1;run(600,&tore);if(tore<0)tore=300;}
 const int target=std::max(0,tore+int(std::lround(seconds*120)));auto g=run(target+1,nullptr);
 if(tearAge)*tearAge=view>=2?g.m_elapsed-31.f/120:g.m_world.leadRopes()[0].torn?g.m_elapsed-g.m_world.leadRopes()[0].tornAt:-1.f;
 return g;
}
}

namespace retro::electric {
bool test(std::ostream& report){
 bool passed=true;auto check=[&](bool ok,const char*name,const std::string&v){report<<(ok?"PASS ":"FAIL ")<<name<<": "<<v<<'\n';report.flush();passed&=ok;};
 auto m=regression::room();m->fixtures.push_back({model::desk,{8,12},0,1.4f,.7f,.75f,0,true});World w(0,m);resetCaches();
 // Thrown up off the middle of the floor: they fall, bounce, skitter and the embers come to rest, never below it.
 {Burst b;b.at={8,8,1.2f};b.along={0,0,1};b.count=60;b.embers=10;b.speed=3.5f;b.spread=1.2f;b.seed=7;float lowest=1e9f,highest=0;int landed=0,alive=0;std::vector<Spark> s;
  for(int frame=0;frame<=150;++frame){s.clear();sparks(w,b,frame/60.f,s,SparkBudget);for(const auto&p:s){lowest=std::min(lowest,p.p.z);highest=std::max(highest,p.p.z);}}
  s.clear();sparks(w,b,2.5f,s,SparkBudget);for(const auto&p:s){++alive;landed+=p.resting&&p.p.z<.01f&&p.p.z>=0;}
  check(lowest>=-.001f&&highest>1.3f&&alive>0&&landed==alive,"10a sparks fall to the floor and rest on it, never through it","lowest z "+std::to_string(lowest)+", highest "+std::to_string(highest)+", embers glowing at 2.5 s "+std::to_string(alive)+" of them at rest on the floor "+std::to_string(landed));}
 // Fired at the west wall (its face at x=1): none passes it.
 {Burst b;b.at={1.25f,8,1};b.along={-1,0,.2f};b.count=48;b.speed=5;b.spread=.6f;b.seed=11;float least=1e9f;int back=0;std::vector<Spark> s;
  for(int frame=0;frame<=60;++frame){s.clear();sparks(w,b,frame/60.f,s,SparkBudget);for(const auto&p:s){least=std::min(least,p.p.x);if(frame==30&&p.v.x>0)++back;}}
  check(least>=1-.001f&&back>4,"10b sparks thrown at a wall bounce off it and never pass it","least x "+std::to_string(least)+", flying back off it at .5 s "+std::to_string(back));}
 // Over a desk: they land on its top.
 {Burst b;b.at={8,12,1.1f};b.along={0,0,1};b.count=24;b.speed=1.2f;b.spread=.3f;b.seed=5;std::vector<Spark> s;float least=1e9f;
  for(int frame=0;frame<=90;++frame){s.clear();sparks(w,b,frame/60.f,s,SparkBudget);for(const auto&p:s)if(std::fabs(p.p.x-8)<.65f&&std::fabs(p.p.y-12)<.3f)least=std::min(least,p.p.z);}
  check(least>=.75f-.001f,"10c sparks over a desk land on its top","least z over it "+std::to_string(least));}
 // The same burst at the same time, however it was reached, is the same sparks.
 {Burst b;b.at={6,6,1};b.along={1,0,1};b.count=40;b.seed=99;b.time=2;std::vector<Spark> stepped,direct,back;
  for(int frame=0;frame<=30;++frame){stepped.clear();sparks(w,b,2+frame/60.f,stepped,SparkBudget);}
  resetCaches();sparks(w,b,2.5f,direct,SparkBudget);direct.clear();sparks(w,b,2.5f,direct,SparkBudget);
  {std::vector<Spark> later;sparks(w,b,3.f,later,SparkBudget);}sparks(w,b,2.5f,back,SparkBudget);
  auto same=[](const std::vector<Spark>&a,const std::vector<Spark>&c){if(a.size()!=c.size())return false;for(size_t i=0;i<a.size();++i)if(std::memcmp(&a[i].p,&c[i].p,sizeof(Point3))||a[i].heat!=c[i].heat)return false;return true;};
  check(!stepped.empty()&&same(stepped,direct)&&same(stepped,back),"10d a burst is the same sparks at a time whether stepped to, asked fresh or asked again after a later time",std::to_string(stepped.size())+" sparks at .5 s");
  std::vector<Spark> few;const int given=sparks(w,b,2.1f,few,5);check(given==5&&few.size()==5,"10e a burst never gives more sparks than it is allowed","asked for 5, given "+std::to_string(few.size()));}
 // Current jumps to the wall beside it, the floor below or the water; the bolt runs from the end to that point.
 {Point3 to;const bool wall=conductor(w,{1.3f,8,1.4f},.5f,3,to);const bool floor=conductor(w,{8,8,.2f},.5f,3,to)&&std::fabs(to.z)<.01f;Point3 none;const bool air=!conductor(w,{8,8,1.6f},.5f,3,none);
  Point3 onWall;conductor(w,{1.3f,8,1.4f},.5f,3,onWall);
  Arc a;a.from={1.3f,8,1.4f};a.to=onWall;a.seed=4;std::vector<Piece> p;bolt(a,.5f,5,p);float firstGap=1,lastGap=1;for(const auto&q:p){firstGap=std::min(firstGap,span(q.a-a.from));lastGap=std::min(lastGap,span(q.b-a.to));}
  check(wall&&std::fabs(onWall.x-1)<.004f&&floor&&air&&p.size()>=16&&firstGap<1e-5f&&lastGap<1e-5f,"10f an arc ends on what conducts: the wall in reach, the floor below, nothing in mid-air","wall point x "+std::to_string(onWall.x)+", pieces "+std::to_string(p.size())+", gaps "+std::to_string(firstGap)+"/"+std::to_string(lastGap));}
 // A live floor's arcs run between real surfaces in its box.
 {Hazard h;h.kind=Hazard::Kind::Electricity;h.x1=1.f;h.x2=4;h.y1=6;h.y2=10;h.bottom=-1;h.top=2;h.period=2;h.onTime=1;int arcs=0;bool grounded=true;
  for(int f=0;f<40;++f){std::vector<Arc> out;hazardArcs(w,h,0,f*.05f,out);for(const auto&a:out){++arcs;for(Point3 e:{a.from,a.to}){bool onFloor=std::fabs(e.z-.004f)<.006f;bool onWall=false;
     for(Point3 d:{Point3{1,0,0},Point3{-1,0,0},Point3{0,1,0},Point3{0,-1,0}}){Point3 n;onWall|=w.shotFraction(e,e+d*.02f,n)<1;}grounded&=onFloor||onWall;}}}
  check(arcs>20&&grounded,"10g a live floor's arcs run from its floor to the floor or the wall beside it",std::to_string(arcs)+" arcs over two seconds");}
 // Struck about StrikeRate times a second, dark on some strikes.
 {Arc a;a.seed=21;int lit=0,dark=0,changes=0;float last=-1;for(int i=0;i<480;++i){float s=strike(a,i/240.f);(s>0?lit:dark)++;if(last>=0&&(s>0)!=(last>0))++changes;last=s;}
  check(lit>dark*3&&dark>0&&changes>4,"10h an arc flickers: lit through most strikes, dark through some","lit "+std::to_string(lit)+" dark "+std::to_string(dark)+" of 480 samples over 2 s");}
 // A torn live lead lights the room blue-white while it arcs and not once it is dead.
 {float strongest=0,pop=0;Point3 tone{};for(float age:{0.f,.05f,.09f,.15f})for(const auto&f:Game::electricInspection(age).weaponFlashes()){if(f.tone.z>f.tone.x&&f.strength>strongest){strongest=f.strength;tone=f.tone;}if(f.tone.x>f.tone.z)pop=std::max(pop,f.strength);}
  float after=0;for(const auto&f:Game::electricInspection(2.8f).weaponFlashes())after=std::max(after,f.strength);
  check(strongest>.3f&&pop>.3f&&after==0,"10i a torn live lead's sparks pop warm and its arc lights the room blue-white, then nothing once it is dead","arc "+std::to_string(strongest)+" tone "+std::to_string(tone.x)+","+std::to_string(tone.y)+","+std::to_string(tone.z)+", pop "+std::to_string(pop)+"; at 2.8 s "+std::to_string(after));}
 // A round striking a wall: both its embers still glow 1.4 s on (they live 1.5-3 s), and nothing is left once they are out.
 {auto g=Game::electricInspection(1.4f,2);int embers=0,below=0,shower=0;
  for(const auto&b:g.electricNow(0).bursts)if(b.count==14){++shower;std::vector<Spark> s;sparks(g.world(),b,g.elapsed(),s,SparkBudget);for(const auto&p:s){embers+=p.ember;below+=p.p.z<-.001f;}}
  int left=0;{auto late=Game::electricInspection(3.3f,2);for(const auto&b:late.electricNow(0).bursts)left+=b.count==14;}
  check(shower==1&&embers==2&&below==0&&left==0,"10j a round's sparks off a wall leave both embers glowing 1.4 s on, and nothing once they are out",std::to_string(embers)+" embers at 1.4 s, "+std::to_string(below)+" below the floor; showers left at 3.3 s "+std::to_string(left));}
 // A pulsed live floor beside a wall: what its arcs spit off metal flies out its whole life, though the
 // spell (.28 s) and the pulse (.5 s) that threw it are over.
 {auto g=Game::electricInspection(0,3);const Hazard h=g.world().hazards()[0];std::unordered_map<unsigned,std::pair<float,float>> seen;int flyingOff=0;
  for(int i=0;i<300;++i){g.update({},1.f/60);const float now=g.elapsed();const bool on=g.hazardActive(h);
   for(const auto&b:g.electricNow(0).bursts)if(b.count==6&&b.embers==0){seen.try_emplace(b.seed,b.time,now).first->second.second=now;
    if(!on){std::vector<Spark> live;flyingOff+=sparks(g.world(),b,now,live,SparkBudget)>0;}}}
  const float end=g.elapsed();int whole=0,cut=0;for(const auto&[seed,s]:seen)if(s.first<end-1.2f)(s.second-s.first>=.88f?whole:cut)++;
  check(whole>=3&&cut==0&&flyingOff>0,"10k a pulsed floor's arcs spit sparks off metal that fly out their life after their spell and pulse are over",std::to_string(whole)+" showers flew their .9 s, "+std::to_string(cut)+" cut short; frames with sparks flying while the floor is off "+std::to_string(flyingOff));}
 // The frame's electricity is worked out once a frame, and anew when anything electric happens in it.
 {auto g=Game::electricInspection(.05f);const size_t before=g.electricNow(0).bursts.size();g.sparkBurst(0,{5,8,1},{0,0,1});const size_t after=g.electricNow(0).bursts.size();
  check(before>0&&after==before+1,"10l a shower thrown after the frame's electricity was asked for is in it when asked again",std::to_string(before)+" showers, then "+std::to_string(after));}
 return passed;
}
}

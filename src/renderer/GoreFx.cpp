#include "GoreFx.h"
#include "GpuRenderer.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace retro {
namespace {
using Texture=GoreFx::Texture;
constexpr int Materials=int(ImpactMaterial::Count);
const Point3 Up{0,0,1};
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float span(Point3 v){return std::sqrt(dot3(v,v));}
Point3 unit(Point3 v,Point3 fallback=Point3{0,0,1}){const float l=span(v);return l>1e-5f?v*(1.f/l):fallback;}
std::uint32_t pack(Point3 c,float a){auto q=[](float v){return std::uint32_t(std::clamp(v,0.f,1.f)*255.f+.5f);};return (q(a)<<24)|(q(c.x)<<16)|(q(c.y)<<8)|q(c.z);}
float grain(int x,int y,unsigned salt){unsigned h=unsigned(x)*73856093u^unsigned(y)*19349663u^salt*83492791u;h=(h^(h>>13))*1274126177u;return float((h>>16)&1023u)/1023.f;}
struct Rng {unsigned s;float next(){s=s*1664525u+1013904223u;return float((s>>8)&0xffffu)/65535.f;}float range(float a,float b){return a+(b-a)*next();}float sign(){return next()*2.f-1.f;}};
unsigned seedOf(const HitBurst&b){
 unsigned h=unsigned(std::lround(b.point.x*977.f))*73856093u^unsigned(std::lround(b.point.y*977.f))*19349663u^unsigned(std::lround(b.point.z*977.f))*83492791u^unsigned(std::lround(b.time*1000.f))*2654435761u;
 h=(h^(h>>13))*1274126177u;return h^(h>>16);}
Texture blank(int w,int h,bool transparent){Texture t{w,h,std::vector<std::uint32_t>(size_t(w*h))};t.clampEdges=true;t.transparent=transparent;return t;}

// What each material throws when a round opens it: the colour of its chips and of its dust, how many chips,
// their sizes, how hard they leave, how they fall, how big the puff is, and how long and thin they are.
struct Debris {Point3 tone,dust;int chips;float least,most,speed,gravity,puff,stretch;};
const Debris debris[Materials]={
 {{.62f,.60f,.57f},{.70f,.68f,.64f},6,.012f,.030f,2.6f,9.8f,.22f,0},     // concrete: grey chips, grey dust
 {{},{},0,0,0,0,9.8f,0,0},                                               // metal: sparks only (ElectricFx)
 {{.62f,.45f,.27f},{.60f,.50f,.38f},6,.008f,.016f,2.4f,9.8f,.10f,3.5f},  // wood: splinters
 {{.27f,.20f,.13f},{.40f,.32f,.22f},7,.014f,.036f,2.0f,9.8f,.28f,0},     // dirt: clods
 {{.36f,.34f,.32f},{.48f,.46f,.43f},6,.012f,.034f,2.4f,9.8f,.16f,0},     // rock
 {{.86f,.86f,.84f},{.80f,.80f,.78f},7,.010f,.028f,3.0f,9.8f,.14f,0},     // tile: bright shards
 {{.90f,.88f,.84f},{.88f,.86f,.82f},5,.010f,.024f,2.0f,9.8f,.30f,0},     // plaster: mostly cloud
 {{.75f,.85f,.88f},{.80f,.88f,.90f},6,.008f,.020f,2.6f,9.8f,.06f,1.6f},  // glass
 {{},{},0,0,0,0,9.8f,0,0},                                               // flesh: blood, below
 {{.50f,.48f,.45f},{.60f,.58f,.55f},4,.008f,.014f,.9f,1.6f,.09f,2.f},    // fabric: lint that drifts
};
constexpr int MistLevels=6,DustLevels=4;
struct Kit {
 std::array<std::array<Texture,MistLevels>,2> mist;
 std::array<Texture,2> drop,splat;
 std::array<std::array<Texture,DustLevels>,Materials> dust;
 std::array<Texture,Materials> chip;
};
// A soft round puff: full at its middle, nothing at its edge, a little uneven.
Texture puff(Point3 tone,float strength,unsigned salt){constexpr int n=24;Texture t=blank(n,n,true);
 for(int y=1;y<n-1;++y)for(int x=1;x<n-1;++x){const float dx=(x+.5f)/n*2-1,dy=(y+.5f)/n*2-1,fall=std::clamp(1-std::sqrt(dx*dx+dy*dy)*1.08f,0.f,1.f),noise=grain(x,y,salt);
  t.pixels[size_t(y*n+x)]=pack(tone*(.85f+.3f*noise),fall*fall*(3-2*fall)*strength*(.75f+.25f*noise));}
 return t;}
const Kit& kit(){static const Kit made=[]{Kit k;
 const Point3 blood[2]={{.42f,.02f,.03f},{.45f,.70f,.15f}};
 for(int tone=0;tone<2;++tone){
  for(int level=0;level<MistLevels;++level)k.mist[size_t(tone)][size_t(level)]=puff(blood[tone],float(level+1)/MistLevels*.8f,unsigned(tone*7+level));
  // A drop: a hard round bead, lighter where the room catches it.
  constexpr int n=12;auto&d=k.drop[size_t(tone)];d=blank(n,n,false);
  for(int y=0;y<n;++y)for(int x=0;x<n;++x){const float dx=(x+.5f)/n*2-1,dy=(y+.5f)/n*2-1,r=std::sqrt(dx*dx+dy*dy);if(r>=.8f)continue;
   const float shine=std::clamp(1-std::sqrt((dx+.3f)*(dx+.3f)+(dy+.3f)*(dy+.3f))*2.2f,0.f,1.f);
   d.pixels[size_t(y*n+x)]=pack(blood[tone]*(.8f+.25f*(1-r))+Point3{1,1,1}*(shine*.25f),1);}
  // Where a drop lands: a wet uneven spot of the same blood, thrown out into a few short lobes.
  constexpr int w=16;auto&s=k.splat[size_t(tone)];s=blank(w,w,false);
  for(int y=1;y<w-1;++y)for(int x=1;x<w-1;++x){const float dx=(x+.5f)/w*2-1,dy=(y+.5f)/w*2-1,r=std::sqrt(dx*dx+dy*dy),angle=std::atan2(dy,dx);
   if(r>=.5f+.2f*std::sin(angle*5.f+float(tone))+.13f*std::sin(angle*3.f+1.7f))continue;
   s.pixels[size_t(y*w+x)]=pack(blood[tone]*(.6f+.18f*grain(x,y,unsigned(61+tone))+.2f*r),1);}
 }
 for(int m=0;m<Materials;++m){const auto&kind=debris[m];if(kind.chips<=0)continue;
  for(int level=0;level<DustLevels;++level)k.dust[size_t(m)][size_t(level)]=puff(kind.dust,float(level+1)/DustLevels*.55f,unsigned(100+m*5+level));
  // A chip: an uneven flake of the stuff, or a sliver for what splinters.
  constexpr int n=8;auto&c=k.chip[size_t(m)];c=blank(n,n,false);
  for(int y=0;y<n;++y)for(int x=0;x<n;++x){const float dx=(x+.5f)/n*2-1,dy=(y+.5f)/n*2-1,r=std::sqrt(dx*dx+dy*dy),angle=std::atan2(dy,dx);
   const bool in=kind.stretch>0?std::fabs(dy)<.42f*(1-std::fabs(dx)*.85f):r<.62f+.26f*std::sin(angle*3.f+float(m));if(!in)continue;
   c.pixels[size_t(y*n+x)]=pack(kind.tone*(.85f+.3f*grain(x,y,unsigned(m+31))),1);}
 }
 return k;}();return made;}

// Where a thrown bit is `t` seconds after it left: in the air, down on the floor under the burst, or
// stopped by the wall the burst knows is ahead of it (the game marks that wall itself).
struct Fate {Point3 at{},velocity{};bool flying=false,gone=false;float since=0;};
Fate fly(const HitBurst&b,Point3 from,Point3 v,float t,float gravity){Fate f;if(t<0){f.gone=true;return f;}
 const float drop=std::max(0.f,from.z-b.floor),landing=(v.z+std::sqrt(v.z*v.z+2*gravity*drop))/gravity,along=dot3(v,b.direction);
 if(b.reach<2.55f&&along>.05f){const float wall=std::max(0.f,b.reach-.03f)/along;
  if(wall<std::min(t,landing)&&from.z+v.z*wall-.5f*gravity*wall*wall>b.floor+.12f){f.gone=true;return f;}}
 if(t<landing){f.flying=true;f.at=from+v*t;f.at.z-=.5f*gravity*t*t;f.velocity=v;f.velocity.z-=gravity*t;return f;}
 f.at=from+v*landing;f.at.z=b.floor;f.since=t-landing;return f;}
}

GoreStats GoreFx::draw(const SoftwareRenderer&,const std::vector<HitBurst>& bursts,int level,float now,Point3 eye,const Quad& quad,const Hidden& hidden){
 GoreStats stats;if(bursts.empty())return stats;const Kit&k=kit();
 auto put=[&](Point3 a,Point3 b,Point3 c,Point3 d,const Texture&t,float light){++stats.quads;quad(a,b,c,d,t,light);};
 // A card that faces the eye, one drawn out along the way it is moving, and a spot lying on the floor.
 auto card=[&](Point3 c,float half,float turn,const Texture&t,float light){const Point3 f=unit(c-eye,{1,0,0}),right=unit(cross(f,Up),{1,0,0}),up=cross(right,f);
  const float cs=std::cos(turn),sn=std::sin(turn);const Point3 u=(right*cs+up*sn)*half,v=(up*cs-right*sn)*half;put(c-u-v,c+u-v,c+u+v,c-u+v,t,light);};
 auto streak=[&](Point3 c,Point3 velocity,float half,float stretch,const Texture&t,float light){const Point3 f=unit(c-eye,{1,0,0}),along=velocity-f*dot3(velocity,f);const float l=span(along);
  if(l<.3f){card(c,half,0,t,light);return;}
  Point3 u=along*(1.f/l);const Point3 v=cross(f,u)*half;u=u*(half*(1+stretch));put(c-u-v,c+u-v,c+u+v,c-u+v,t,light);};
 auto spot=[&](Point3 c,float half,float turn,float aspect,const Texture&t,float light){const float cs=std::cos(turn),sn=std::sin(turn);
  const Point3 u{cs*half*aspect,sn*half*aspect,0},v{-sn*half,cs*half,0};put(c-u-v,c+u-v,c+u+v,c-u+v,t,light);};
 for(const auto&b:bursts){
  if(b.level!=level)continue;const float age=now-b.time;if(age<0||age>7.f)continue;
  const int m=int(b.material);if(m<0||m>=Materials||b.material==ImpactMaterial::Metal)continue;
  if(hidden&&hidden(Point3{b.point.x-2.2f,b.point.y-2.2f,std::min(b.floor,b.point.z)-.05f},Point3{b.point.x+2.2f,b.point.y+2.2f,b.point.z+1.2f}))continue;
  ++stats.bursts;Rng rng{seedOf(b)};
  if(b.material==ImpactMaterial::Flesh){
   const int species=b.species>=0&&b.species<int(SoftwareRenderer::creatureVisuals.size())?b.species:int(CreatureKind::Mutant);
   const auto&visual=SoftwareRenderer::creatureVisuals[size_t(species)];const size_t tone=visual.xenoblood?1u:0u;
   const Texture&stain=k.splat[tone];const float stainLight=1.f;
   const float power=std::clamp(b.power,.3f,2.5f),root=std::sqrt(power);
   const Point3 dir=unit(b.direction),side=unit(cross(dir,Up),{1,0,0}),lift=cross(side,dir);
   // A puff of mist: it swells, drifts and thins.
   auto mist=[&](Point3 from,Point3 drift,float size,float life,float born){const float u=(age-born)/life;if(u<0||u>=1)return;const float ease=1-(1-u)*(1-u);
    card(from+drift*(life*ease*.6f)+Up*(.05f*u),size*(.5f+1.1f*ease),0,k.mist[tone][size_t(std::clamp(int((1-u)*MistLevels),0,MistLevels-1))],1.f);};
   // A drop: a bead drawn out along its flight, then a spot on the floor that dries and goes.
   auto droplet=[&](int index,Point3 from,Point3 velocity,float born,float size,float mark,float turn){const Fate f=fly(b,from,velocity,age-born,9.8f);if(f.gone)return;
    if(f.flying){streak(f.at,f.velocity,size,std::min(2.5f,span(f.velocity)*.35f),k.drop[tone],1.f);return;}
    const float half=mark*std::min(1.f,f.since/.08f)*(1-std::clamp((age-5.2f)/1.f,0.f,1.f));if(half<.004f)return;
    spot({f.at.x,f.at.y,b.floor+.012f+float(index)*.0004f},half,turn,1,stain,stainLight);};
   if(b.stump){
    // A cut pumps: mist at once, then drops thrown on a falling pressure for most of a second.
    mist(b.point,dir*.5f+Up*.2f,.14f*root,.6f,0);
    for(int i=0;i<14;++i){const float born=rng.next()*.9f,pressure=1-born/.9f,speed=(.9f+2.4f*pressure)*rng.range(.7f,1.1f),a=rng.sign(),c=rng.sign(),rise=.5f+1.3f*pressure*rng.next(),size=rng.range(.009f,.018f),mark=rng.range(.06f,.14f),turn=rng.next()*kTwoPi;
     droplet(i,b.point,dir*speed+side*(a*.35f*speed)+lift*(c*.35f*speed)+Up*rise,born,size,mark,turn);}
    continue;}
   // Where the round went in: a small puff and a few drops thrown back at the gun.
   for(int i=0;i<2;++i){const float a=rng.sign(),size=rng.range(.9f,1.2f);mist(b.point-dir*.06f,dir*-.35f+side*(a*.15f)+Up*.1f,.12f*root*size,.4f,float(i)*.03f);}
   const int entry=4+int(3.f*power);
   for(int i=0;i<entry;++i){const float back=rng.range(1.2f,2.4f),a=rng.sign(),c=rng.sign(),rise=rng.range(.4f,1.6f),size=rng.range(.008f,.016f),mark=rng.range(.03f,.07f),turn=rng.next()*kTwoPi;
    droplet(i,b.point-dir*.04f,dir*-back+side*(a*.9f)+lift*(c*.6f)+Up*rise,0,size,mark,turn);}
   // Where it came out: a wider cloud and the spray that carries on behind.
   for(int i=0;i<3;++i){const float a=rng.sign(),c=rng.sign(),out=rng.range(.14f,.22f),size=rng.range(.85f,1.2f);
    mist(b.point+dir*out,dir*.9f+side*(a*.3f)+lift*(c*.2f),.16f*root*(b.kill?1.3f:1.f)*size,.5f,float(i)*.025f);}
   const int exits=std::min(20,int((6.f+4.f*power)*(b.kill?1.6f:1.f)));
   for(int i=0;i<exits;++i){const float on=rng.range(2.f,5.5f)*root,a=rng.sign(),c=rng.sign(),rise=rng.range(.2f,2.f),size=rng.range(.009f,.02f),mark=rng.range(.035f,.09f)*std::min(1.5f,power),turn=rng.next()*kTwoPi;
    droplet(entry+i,b.point+dir*.2f,dir*on+side*(a*1.1f)+lift*(c*.8f)+Up*rise,0,size,mark,turn);}
   continue;}
  const auto&kind=debris[m];if(kind.chips<=0)continue;
  const float power=std::clamp(b.power,.3f,2.5f);
  const Point3 n=unit(b.direction),side=unit(cross(n,Up),{1,0,0}),lift=cross(side,n);
  // Dust off the face: it rolls out and up, swelling as it thins.
  {const float life=.55f+kind.puff*1.4f;
   for(int i=0;i<2;++i){const float a=rng.sign(),c=rng.sign(),scale=i?.6f:1.f,born=float(i)*.04f,u=(age-born)/life;if(u<0||u>=1)continue;const float ease=1-(1-u)*(1-u);
    card(b.point+n*(.05f+kind.puff*.9f*ease*scale)+side*(a*kind.puff*.3f)+lift*(c*kind.puff*.2f)+Up*(kind.puff*.5f*u),kind.puff*scale*(.35f+.9f*ease)*std::min(1.3f,power),0,k.dust[size_t(m)][size_t(std::clamp(int((1-u)*DustLevels),0,DustLevels-1))],1.f);}}
  const int chips=int(float(kind.chips)*std::min(1.5f,power));
  for(int i=0;i<chips;++i){const float out=rng.range(.45f,1.f),a=rng.sign(),c=rng.sign(),rise=rng.range(0.f,1.6f),size=rng.range(kind.least,kind.most),turn=rng.next()*kTwoPi,spin=rng.sign()*14.f;
   const Fate f=fly(b,b.point+n*.02f,n*(kind.speed*out)+side*(a*kind.speed*.45f)+lift*(c*kind.speed*.45f)+Up*(rise*std::min(1.f,kind.gravity/9.8f+.2f)),age,kind.gravity);if(f.gone)continue;
   if(f.flying){if(kind.stretch>0)streak(f.at,f.velocity,size*.5f,kind.stretch,k.chip[size_t(m)],1.f);else card(f.at,size,turn+spin*age,k.chip[size_t(m)],1.f);continue;}
   // Down: it lies a few seconds, then is gone.
   const float half=size*(1-std::clamp((f.since-4.f)/.6f,0.f,1.f));if(half<.003f)continue;
   spot({f.at.x,f.at.y,b.floor+.006f+float(i)*.0005f},kind.stretch>0?half*.5f:half,turn,1+kind.stretch*.5f,k.chip[size_t(m)],1.f);}
 }
 return stats;
}

bool SoftwareRenderer::testGore(std::ostream& report){
 bool passed=true;auto check=[&](bool ok,const std::string&text){report<<(ok?"PASS":"FAIL")<<": "<<text<<'\n';report.flush();passed&=ok;};
 static const char*const names[]={"Huntsman","Wasp","Brute","Warden","Mutant","Snatcher","Chimera","Transformed"};
 constexpr int W=480,H=270,Kinds=int(CreatureKind::Transformed)+1;
 auto number=[](float v){char text[32];std::snprintf(text,sizeof text,"%.3f",double(v));return std::string(text);};
 {auto storage=std::make_unique<SoftwareRenderer>(W,H);auto&d=*storage;
  // 1. Every kind: its body is cut into sections, falls and lies on the floor; shot, it comes apart
  // and the cuts are capped; blown up, it comes apart at once and every piece comes to rest.
  for(int kind=0;kind<Kinds;++kind){const std::string name=names[kind];
   Game g=Game::validationScene(Enemy::Kind(kind),0);const Enemy e=g.chunkEnemies(g.level())[0];const auto&world=g.chunkWorld(g.level());
   std::vector<MeshTriangle> pose;d.poseCreatureMesh(e,0,true,pose);
   const float floor=world.supportBelow(e.pos.x,e.pos.y,e.z+.5f);
   struct Survey {bool finite=true;float lowest=1e9f,highest=-1e9f,reach=0;Point3 centre{};};
   auto survey=[&](const std::vector<MeshTriangle>&faces){Survey s;int count=0;
    for(const auto&f:faces)for(const auto&v:f.v){const Point3 p=v.p;if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){s.finite=false;continue;}
     s.centre=s.centre+p;++count;s.reach=std::max(s.reach,std::hypot(p.x-e.pos.x,p.y-e.pos.y));
     const float support=world.supportBelow(p.x,p.y,p.z+.3f);if(std::fabs(support-floor)>.5f)continue;
     s.lowest=std::min(s.lowest,p.z-support);s.highest=std::max(s.highest,p.z-floor);}
    if(count)s.centre=s.centre*(1.f/float(count));return s;};
   const Survey standing=survey(pose);
   Carcass body;const auto began=std::chrono::steady_clock::now();body.build(pose,world,Point3{1.5f,0,.4f},e.scale,unsigned(kind)*977u+11u);
   const float buildMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-began).count();
   report<<name<<": "<<pose.size()<<" faces, "<<body.sections()<<" sections, "<<body.joints()<<" joints ("<<body.joints(true)<<" seams), built in "<<buildMs<<" ms; stands "<<standing.highest<<" tall\n";
   check(!pose.empty()&&body.built()&&body.triangles().size()==pose.size(),name+": its body is built from every face of its mesh");
   check(body.sections()>=2&&body.joints(true)>=1,name+": it is cut into sections that can part ("+std::to_string(body.sections())+" sections, "+std::to_string(body.joints(true))+" seams)");
   bool finite=standing.finite;float lowest=1e9f,highest=-1e9f;Survey rest;
   const auto fallBegan=std::chrono::steady_clock::now();
   for(int step=1;step<=32;++step){body.advance(world,float(step)*.25f);rest=survey(body.triangles());finite&=rest.finite;lowest=std::min(lowest,rest.lowest);highest=std::max(highest,rest.highest);}
   const float fallMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-fallBegan).count();
   const Point3 moved=rest.centre-standing.centre;
   report<<"  fell: lowest "<<lowest<<", highest "<<highest<<", rests "<<rest.highest<<" tall, centre moved "<<span(moved)<<" (down "<<-moved.z<<"), asleep "<<body.asleep()<<", 8 s simulated in "<<fallMs<<" ms\n";
   check(finite,name+": every vertex stays finite as it falls");
   check(lowest>=-.06f,name+": it never sinks through the floor (lowest "+number(lowest)+")");
   check(highest<=standing.highest+.4f,name+": it is never thrown above where it stood (highest "+number(highest)+" against "+number(standing.highest)+")");
   check(span(moved)>.05f&&rest.highest<standing.highest-.02f,name+": it goes down and stays down (rests "+number(rest.highest)+" tall from "+number(standing.highest)+")");
   // Shoot it until something comes off.
   int shots=0,stumps=0;std::vector<Carcass::Event> events;
   while((body.pieces()<2||body.caps().empty())&&shots<60){const auto&faces=body.triangles();const auto&f=faces[size_t(shots*7919+13)%faces.size()];
    const Point3 c=(f.v[0].p+f.v[1].p+f.v[2].p)*(1.f/3.f),from=c+Point3{-2,.3f,1.2f};Point3 to=c-from;const float l=span(to);to=to*(1.f/l);
    events.clear();body.strike(Carcass::Ray{from,to,l+.5f,60.f,3.f},events);for(const auto&event:events)stumps+=event.stump?1:0;++shots;}
   body.advance(world,10.f);const Survey cut=survey(body.triangles());bool capsFinite=true;for(const auto&cap:body.caps())for(const auto&v:cap.v)capsFinite&=std::isfinite(v.p.x)&&std::isfinite(v.p.y)&&std::isfinite(v.p.z);
   report<<"  shot: "<<shots<<" shots, "<<body.pieces()<<" pieces, "<<stumps<<" stumps, "<<body.caps().size()<<" cap faces, lowest "<<cut.lowest<<"\n";
   check(body.pieces()>=2&&stumps>=1,name+": shooting it takes a section off ("+std::to_string(shots)+" shots, "+std::to_string(body.pieces())+" pieces)");
   check(!body.caps().empty()&&capsFinite,name+": the cut is closed with a computed cap ("+std::to_string(body.caps().size())+" faces)");
   check(cut.finite&&cut.lowest>=-.06f,name+": the pieces lie on the floor (lowest "+number(cut.lowest)+")");
   // And blow a fresh one apart.
   Carcass blown;blown.build(pose,world,Point3{},e.scale,unsigned(kind)*977u+12u);events.clear();blown.blast(Point3{e.pos.x-.4f,e.pos.y,e.z+.2f},1.f,events);const int thrown=blown.pieces();
   bool blownFinite=true;float blownLowest=1e9f,blownReach=0;
   for(int step=1;step<=24;++step){blown.advance(world,float(step)*.5f);const Survey s=survey(blown.triangles());blownFinite&=s.finite;blownLowest=std::min(blownLowest,s.lowest);blownReach=std::max(blownReach,s.reach);}
   report<<"  blown: "<<thrown<<" pieces, "<<events.size()<<" events, lowest "<<blownLowest<<", reach "<<blownReach<<", asleep "<<blown.asleep()<<"\n";
   check(thrown>=std::min(3,body.sections()),name+": a blast takes it apart ("+std::to_string(thrown)+" pieces)");
   check(blown.caps().size()>=size_t(blown.joints(true))*3,name+": every seam it parted at is capped ("+std::to_string(blown.caps().size())+" cap faces over "+std::to_string(blown.joints(true))+" seams)");
   check(blownFinite&&blownLowest>=-.06f&&blownReach<7.f,name+": the pieces land and stay in the room (lowest "+number(blownLowest)+", reach "+number(blownReach)+")");
  }
  // 2. The corpse's clock: whole for most of its time, then fading out, then gone.
  {Enemy e;e.alive=false;e.deathTime=Enemy::CorpseLifetime-Enemy::CorpseFade;const float start=e.dissolve();e.deathTime=Enemy::CorpseLifetime-Enemy::CorpseFade*.5f;const float half=e.dissolve();const bool there=e.visible();e.deathTime=Enemy::CorpseLifetime;
   check(start==0&&std::fabs(half-.5f)<.01f&&there&&e.dissolve()==1&&!e.visible(),"a corpse lies whole, fades over its last "+number(Enemy::CorpseFade)+" s and is gone at "+number(Enemy::CorpseLifetime)+" s");
   e.alive=true;check(e.dissolve()==0&&e.visible(),"nothing living fades");}
  // 3. What hits throw: every material but metal throws its own stuff, flesh bleeds, a kill bleeds more,
  // a stump pumps, and all of it is over in a few seconds.
  {bool finite=true;auto count=[&](const HitBurst&b,float age){std::vector<HitBurst> one{b};
    return GoreFx::draw(d,one,0,b.time+age,Point3{0,0,1.6f},[&](Point3 a,Point3 bb,Point3 c,Point3 dd,const Texture&,float){for(Point3 p:{a,bb,c,dd})finite&=std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}).quads;};
   static const char*const materials[]={"concrete","metal","wood","dirt","rock","tile","plaster","glass","flesh","fabric"};
   for(int m=0;m<Materials;++m){HitBurst b{{5,5,1},{0,0,1},10.f,0,ImpactMaterial(m),-1,1.f,false};b.floor=0;b.reach=9;if(m==int(ImpactMaterial::Flesh))b.direction={1,0,0};
    const int early=count(b,.1f),late=count(b,3.f),over=count(b,8.f);
    if(m==int(ImpactMaterial::Metal))check(early==0&&late==0,"metal throws only sparks");
    else check(early>0&&late>0&&over==0,std::string(materials[m])+" throws its own debris ("+std::to_string(early)+" quads at 0.1 s, "+std::to_string(late)+" lying at 3 s, none at 8 s)");}
   HitBurst hit{{5,5,1},{1,0,0},10.f,0,ImpactMaterial::Flesh,int(CreatureKind::Mutant),1.f,false};hit.floor=0;hit.reach=9;HitBurst kill=hit;kill.kill=true;kill.power=1.5f;HitBurst stump=hit;stump.stump=true;
   const int plain=count(hit,.15f),killing=count(kill,.15f),pumping=count(stump,.3f);
   check(killing>plain&&plain>0,"a killing round bleeds more than a wound ("+std::to_string(killing)+" against "+std::to_string(plain)+" quads)");
   check(pumping>0,"a cut stump pumps ("+std::to_string(pumping)+" quads at 0.3 s)");
   HitBurst walled=hit;walled.reach=.4f;check(count(walled,.5f)<count(hit,.5f),"a wall close behind stops the spray");
   check(finite,"every quad drawn is finite");}
 }
 // 4. On both back ends: the body is there when it falls, lies whole, fades by its clock and is gone; squibs
 // and scars draw; a creature killed by a real shot is left as a body with a wound on it.
 int step=24;
 auto differing=[&step](const std::vector<std::uint32_t>&a,const std::vector<std::uint32_t>&b){int n=0;for(size_t i=0;i<a.size()&&i<b.size();++i){bool differs=false;for(int shift:{0,8,16})differs|=std::abs(int((a[i]>>shift)&255u)-int((b[i]>>shift)&255u))>step;n+=differs?1:0;}return n;};
 // The software back end lights this room far darker than Vulkan does, so a body is told from the floor by a finer step there.
 for(bool vulkan:{false,true}){const std::string backend=vulkan?"Vulkan":"CPU";step=vulkan?24:3;
  auto storage=std::make_unique<SoftwareRenderer>(W,H);auto&d=*storage;if(vulkan&&!d.enableHardware()){check(false,"Vulkan starts");break;}
  d.setEnvironmentInspection(true);if(vulkan)d.m_gpu->setPostProcessing(false);
  auto frame=[&](const Game&g){d.render(g);return std::vector<std::uint32_t>(d.m_pixels.begin(),d.m_pixels.end());};
  auto body=[&](float age){d.m_remains.clear();d.m_goreBursts.clear();const auto with=frame(Game::goreInspection(Enemy::Kind::Mutant,0,age));const bool kept=!d.m_remains.empty()&&d.m_remains.begin()->second.carcass.built();
   const auto without=frame(Game::goreInspection(Enemy::Kind::Mutant,0,age,true));return std::pair<int,bool>{differing(with,without),kept};};
  {const auto a=frame(Game::goreInspection(Enemy::Kind::Mutant,0,3,true)),b=frame(Game::goreInspection(Enemy::Kind::Mutant,0,3,true));report<<backend<<": an empty frame against itself differs in "<<differing(a,b)<<" pixels\n";}
  const auto falling=body(.1f),down=body(1.f),lying=body(3.f),still=body(6.f),whole=body(11.f),fading=body(13.25f),nearlyGone=body(14.9f),gone=body(15.5f);
  report<<backend<<": body pixels at 0.1 s "<<falling.first<<", 1 s "<<down.first<<", 3 s "<<lying.first<<", 6 s "<<still.first<<", 11 s "<<whole.first<<", 13.25 s "<<fading.first<<", 14.9 s "<<nearlyGone.first<<", 15.5 s "<<gone.first<<"\n";
  check(falling.first>150&&falling.second,backend+": a creature that has just died is still there, as a body ("+std::to_string(falling.first)+" pixels)");
  check(down.first>100&&lying.first>100&&still.first>100&&whole.first>100,backend+": it lies there at full size for as long as its clock says ("+std::to_string(lying.first)+" pixels at 3 s, "+std::to_string(whole.first)+" at 11 s)");
  check(fading.first>whole.first/6&&fading.first<whole.first*17/20,backend+": half way through its fade about half of it shows ("+std::to_string(fading.first)+" of "+std::to_string(whole.first)+" pixels)");
  check(nearlyGone.first<whole.first/4,backend+": at the end of its fade almost none does ("+std::to_string(nearlyGone.first)+" pixels)");
  check(gone.first<=12&&!gone.second,backend+": then it is gone, and nothing of it is kept ("+std::to_string(gone.first)+" pixels)");
  // Squibs: blood from a hit, and the blood a stump throws, draw over the room.
  {d.m_remains.clear();d.m_goreBursts.clear();const Game empty=Game::goreInspection(Enemy::Kind::Mutant,0,1,true);const auto clear=frame(empty);const auto&world=empty.chunkWorld(empty.level());
   HitBurst burst{{6.f,4.5f,1.f},{1,0,0},empty.elapsed()-.12f,empty.level(),ImpactMaterial::Flesh,int(CreatureKind::Mutant),1.5f,true};burst.floor=world.supportBelow(6.f,4.5f,1.f);
   d.m_goreBursts.push_back(burst);const int squib=differing(frame(empty),clear);
   d.m_goreBursts.clear();burst.stump=true;d.m_goreBursts.push_back(burst);const int pump=differing(frame(empty),clear);d.m_goreBursts.clear();
   check(squib>12&&pump>4,backend+": a squib and a pumping stump draw ("+std::to_string(squib)+" and "+std::to_string(pump)+" pixels)");}
  // Materials: ten hits on the floor throw debris and leave ten scars.
  {d.m_remains.clear();const auto early=differing(frame(Game::goreInspection(Enemy::Kind::Mutant,3,.15f)),frame(Game::goreInspection(Enemy::Kind::Mutant,3,.15f,true)));
   const auto late=differing(frame(Game::goreInspection(Enemy::Kind::Mutant,3,6.9f)),frame(Game::goreInspection(Enemy::Kind::Mutant,3,6.9f,true)));
   check(early>60&&late>20,backend+": hits on each material throw debris and leave their scars ("+std::to_string(early)+" pixels in flight, "+std::to_string(late)+" once it has settled)");}
  // A real shot at each kind.
  for(int kind=0;kind<Kinds;++kind){const std::string name=names[kind];d.m_remains.clear();d.m_goreBursts.clear();
   Game g=Game::goreInspection(Enemy::Kind(kind),4,0);const auto&e=g.chunkEnemies(g.level())[0];
   bool burst=false;for(const auto&b:g.hitBursts())burst|=b.material==ImpactMaterial::Flesh&&b.kill;
   check(!e.alive&&burst&&!g.shotRays().empty(),backend+": a shot kills the "+name+" and throws a killing squib");
   d.render(g);const bool kept=!d.m_remains.empty()&&d.m_remains.begin()->second.carcass.built();const size_t wounds=d.m_remains.empty()?0:d.m_remains.begin()->second.wounds.size();
   check(kept&&wounds>=1,backend+": it is left as a body with the wound on it ("+std::to_string(wounds)+" wounds)");
   for(int i=0;i<30;++i)g.update(InputState{},1.f/60);d.render(g);
   const bool follows=!d.m_remains.empty()&&std::fabs(d.m_remains.begin()->second.carcass.age()-g.chunkEnemies(g.level())[0].deathTime)<.05f;
   check(follows&&!g.chunkEnemies(g.level())[0].alive,backend+": the "+name+"'s body keeps the game's clock");}
 }
 return passed;
}
}

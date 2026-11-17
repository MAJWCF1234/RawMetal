// Props, people and rounds meeting standing water (the testers: "props in water float or sink by their
// mass and volume"; "splashes and ripples when you jump or fall in, walk or wade, throw stuff in, and
// especially when shooting into the water").
//
// water-props (standalone): each prop dropped into a deep pool and stepped as play steps it
// (Game::update): where it rides (its draft against its density), its bob and tilt once settled, its
// drift, how fast a sinker goes down and that it lies still on the bed; a throw slowed by the water;
// a floating crate shoved by the player wading; a floating crate saved and loaded mid-bob.
// water-splashes (standalone): the player's jump in, steps and wake in shallow water, a creature and a
// man dying in water, the splash sounds by size, the AI stimulus, the spray and crown by energy and
// the rings on the surface (their spread, fade, overlap and the cut of the surface for drawing).
// water-ripples (fixture water-pool, the Starter Map's welcome basin): a fall, a round and a step
// raised on the real pool, filmed "before", "after" and "later".
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "game/Buoyancy.h"
#include "renderer/WaterRipples.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
constexpr float Bed=-1.2f,Surface=-.05f,ShallowSurface=-.03f;
// A plain room with a deep pool (x 3-11, y 6-14, 1.15 m) and a shallow one (x 13-22, y 4-13, 17 cm).
std::shared_ptr<CustomCampaign> pools(){auto m=regression::room();m->definition.playerStart={2,2};
 m->waterVolumes.push_back({3,6,11,14,Bed,Surface});m->waterVolumes.push_back({13,4,22,13,-.2f,ShallowSurface});return regression::campaign(m);}
Clutter make(int model,Vec2 at,float z){Clutter c;c.kind=7;c.model=model;c.dims=Game::propTraits(model).size;c.pos=at;c.z=z;c.sleeping=false;return c;}
float tilt(const Clutter& c){auto off=[](float a){const float r=std::fmod(std::fabs(a),kPi*.5f);return std::min(r,kPi*.5f-r);};return std::max(off(c.pitch),off(c.roll));}
int countKind(const Game& g,WaterSplashKind k){int n=0;for(const auto&s:g.waterSplashes())n+=s.kind==k;return n;}

struct Drop {std::string name;float density=0,height=0,draft=0,bobMm=0,tiltDeg=0,driftCm=0,sinkSpeed=0,bedTime=-1,restDriftMm=0,power=0,splashGain=0,splashAi=0;int splashes=0;bool floats=false,afloat=false,onBed=false,asleep=false;};
Drop drop(int model,const char* name){
 Game g(pools());LabAccess::player(g).pos={2,2};LabAccess::clutter(g)={make(model,{7,10},Surface+.45f)};Drop d;d.name=name;
 const auto& c0=LabAccess::clutter(g)[0];d.density=buoyancy::density(c0);d.floats=buoyancy::floats(c0);d.height=c0.extent()[2]*2;
 float zLo=1e9f,zHi=-1e9f,entered=-1;Vec2 at4{},atRest{};bool rested=false;float tiltMax=0;
 for(int i=0;i<600;++i){g.update({},1.f/60);const float t=(i+1)/60.f;const auto& c=LabAccess::clutter(g)[0];
  for(const auto&s:g.sounds())if(s.sound==Sound::WaterSplash){d.splashGain=std::max(d.splashGain,s.gain);d.splashAi=std::max(d.splashAi,s.aiRadius);}
  if(i==60)for(const auto&s:g.waterSplashes())if(s.kind==WaterSplashKind::Entry){++d.splashes;d.power=std::max(d.power,s.power);}
  if(entered<0&&c.z<Surface)entered=t;if(entered>=0&&t>=entered+.4f&&t<entered+.4f+1.f/60&&c.z>Bed+.05f)d.sinkSpeed=-c.vz;
  if(d.bedTime<0&&c.z<Bed+.03f)d.bedTime=t;
  if(t>=4&&t<4+1.f/60)at4=c.pos;
  if(t>=6){zLo=std::min(zLo,c.z);zHi=std::max(zHi,c.z);tiltMax=std::max(tiltMax,tilt(c));}
  if(c.sleeping&&!rested){rested=true;atRest=c.pos;}}
 const auto& c=LabAccess::clutter(g)[0];
 d.draft=(Surface-c.z)/(c.extent()[2]*2);d.bobMm=(zHi-zLo)*1000;d.tiltDeg=tiltMax*57.3f;d.driftCm=std::sqrt(lengthSq(c.pos-at4))*100;
 d.onBed=c.z<Bed+.03f;d.asleep=c.sleeping;d.afloat=c.z<Surface&&c.z+c.extent()[2]*2>Surface;
 if(rested)d.restDriftMm=std::sqrt(lengthSq(c.pos-atRest))*1000;return d;}

// A prop thrown flat at 6 m/s: its speed 0.3 s on, through water and through air.
float thrownSpeed(bool water){Game g(pools());auto c=make(model::packing_crate,{4.f,10},water?Surface-.2f:.6f);if(!water)c.pos={4,2.5f};
 c.velocity={6,0};c.vz=0;LabAccess::clutter(g)={c};for(int i=0;i<18;++i)g.update({},1.f/60);const auto& e=LabAccess::clutter(g)[0];return std::sqrt(lengthSq(e.velocity));}

const bool props=add({.name="water-props",.summary="props float or sink by density, bob and tilt afloat, drift, sink slowly to the bed and stay; water slows a throw; a wader shoves a float; a save keeps it",.standalone=+[]{
 std::ofstream report("water-props-test.txt");bool passed=true;auto check=[&](bool ok,const std::string& name,const std::string& detail=""){report<<(ok?"PASS ":"FAIL ")<<name<<(detail.empty()?"":"  ("+detail+")")<<'\n';report.flush();passed&=ok;};
 struct Case {int model;const char* name;};
 std::vector<Drop> drops;
 for(const auto& k:{Case{model::wooden_crate,"wooden_crate"},Case{model::packing_crate,"packing_crate"},Case{model::prop_cardboard_box,"cardboard_box"},Case{model::keyboard,"keyboard"},
                    Case{model::mine_hard_hat,"hard_hat"},Case{model::med_specimen_jar,"specimen_jar"},Case{model::mug,"mug"},Case{model::trash_can,"trash_can"},
                    Case{model::crt_monitor,"crt_monitor"},Case{model::ind_barrel,"ind_barrel"},Case{model::gas_cylinder,"gas_cylinder"}}){
  const auto d=drop(k.model,k.name);drops.push_back(d);
  report<<d.name<<": density "<<fixed(d.density,0)<<" floats "<<d.floats<<" height "<<fixed(d.height)<<" | draft "<<fixed(d.draft)<<" (expect "<<fixed(std::min(1.f,d.density/1000))<<") bob "<<fixed(d.bobMm,1)<<" mm tilt "<<fixed(d.tiltDeg,1)
        <<" deg drift(4-10s) "<<fixed(d.driftCm,1)<<" cm | sink max "<<fixed(d.sinkSpeed)<<" m/s bed at "<<fixed(d.bedTime)<<" s on bed "<<d.onBed<<" asleep "<<d.asleep<<" rest drift "<<fixed(d.restDriftMm,2)
        <<" mm | entry splashes "<<d.splashes<<" power "<<fixed(d.power)<<" sound gain "<<fixed(d.splashGain)<<" ai "<<fixed(d.splashAi,1)<<'\n';
  if(d.floats){check(d.afloat&&std::fabs(d.draft-d.density/1000)<.12f,std::string(d.name)+" floats at its density's draft","draft "+fixed(d.draft)+" density "+fixed(d.density,0));
   check(d.bobMm>.5f&&d.bobMm<40&&d.tiltDeg>.2f&&d.tiltDeg<9&&!d.asleep,std::string(d.name)+" bobs and tilts gently afloat and stays awake","bob "+fixed(d.bobMm,1)+" mm tilt "+fixed(d.tiltDeg,1)+" deg");
   check(d.driftCm>.5f&&d.driftCm<60,std::string(d.name)+" drifts slowly","drift "+fixed(d.driftCm,1)+" cm in 6 s");}
  else{check(d.onBed&&d.asleep&&d.restDriftMm<1,std::string(d.name)+" sinks to the bed and lies still","bed at "+fixed(d.bedTime)+" s, rest drift "+fixed(d.restDriftMm,2)+" mm");
   check(d.sinkSpeed>.2f&&d.sinkSpeed<1.4f,std::string(d.name)+" sinks slowly",fixed(d.sinkSpeed)+" m/s 0.4 s after going in");}
  check(d.splashes==1&&d.splashGain>0&&d.splashAi>=2.5f,std::string(d.name)+" falling in raises one splash, heard","splashes "+std::to_string(d.splashes));}
 {float light=1e9f,heavy=0;for(const auto&d:drops){if(d.name=="mug")light=d.power;if(d.name=="ind_barrel")heavy=d.power;}
  check(heavy>light*2,"a heavy fall splashes harder than a light one","mug "+fixed(light)+" barrel "+fixed(heavy));}
 {const float wet=thrownSpeed(true),dry=thrownSpeed(false);report<<"thrown 6 m/s, after 0.3 s: through water "<<fixed(wet)<<" m/s, through air "<<fixed(dry)<<" m/s\n";
  check(wet<dry*.5f,"water slows a thrown prop","water "+fixed(wet)+" air "+fixed(dry));}
 // A floating crate shoved by the player wading into it.
 {Game g(pools());LabAccess::clutter(g)={make(model::wooden_crate,{7.2f,10},Surface-.3f)};for(int i=0;i<180;++i)g.update({},1.f/60);
  auto& p=LabAccess::player(g);p.pos={5.6f,10};p.z=g.world().floorHeight(5.6f,10);p.angle=0;p.pitch=0;const Vec2 start=LabAccess::clutter(g)[0].pos;
  for(int i=0;i<90;++i){InputState in;in.forward=true;g.update(in,1.f/60);}for(int i=0;i<60;++i)g.update({},1.f/60);
  const auto& c=LabAccess::clutter(g)[0];const float moved=c.pos.x-start.x;report<<"wading player shoves a floating crate "<<fixed(moved)<<" m, it still floats "<<(c.z<Surface&&c.z+c.height()>Surface)<<", player at "<<fixed(p.pos.x)<<'\n';
  check(moved>.3f&&c.z<Surface&&c.z+c.height()>Surface,"a floating crate is pushed by the player wading","moved "+fixed(moved)+" m");}
 // Saved and loaded mid-bob: the same float goes on the same way.
 {Game a(pools());LabAccess::clutter(a)={make(model::wooden_crate,{7,10},Surface+.4f)};for(int i=0;i<50;++i)a.update({},1.f/60);
  const std::string data=LabAccess::encodeSave(a);Game b(pools());const bool loaded=LabAccess::decodeSave(b,data);
  float worst=0;for(int i=0;i<120;++i){a.update({},1.f/60);if(loaded)b.update({},1.f/60);}
  if(loaded&&!LabAccess::clutter(b).empty()){const auto&x=LabAccess::clutter(a)[0],&y=LabAccess::clutter(b)[0];worst=std::max({std::fabs(x.z-y.z),std::fabs(x.pos.x-y.pos.x),std::fabs(x.pos.y-y.pos.y)});}
  report<<"saved mid-bob, loaded, 2 s on: worst difference "<<fixed(worst*1000,3)<<" mm (loaded "<<loaded<<")\n";
  check(loaded&&worst<.001f,"a floating prop saved and loaded mid-bob goes on as it was","worst "+fixed(worst*1000,3)+" mm");}
 return passed;}});

// The cards a splash throws (Particles::draw over a still room), a moment after it.
int splashCards(WaterSplashKind kind,float power,float at=.15f){auto memory=makeParticleMemory();WaterSplash s;s.point={10,10,1};s.direction={0,0,-1};s.time=0;s.level=0;s.power=power;s.kind=kind;memory->extra.splashes.push_back(s);
 auto room=regression::room();Game clean(regression::campaign(room));Particles::View view;view.eye={6,10,2};view.forward={1,0,-.2f};view.level=0;view.now=at;int cards=0;
 Particles::draw(*memory,clean,view,[&](const MeshVertex*,const Particles::Texture&,float,const Particles::CardLook&){++cards;},[](Point3,Point3){return 1.f;},[](Point3,Point3){return true;});return cards;}

const bool splashes=add({.name="water-splashes",.summary="jumps, steps, wakes, bodies and throws into water splash by size, sound by size, alert the AI; spray, crown and rings scale with energy",.standalone=+[]{
 std::ofstream report("water-splashes-test.txt");bool passed=true;auto check=[&](bool ok,const std::string& name,const std::string& detail=""){report<<(ok?"PASS ":"FAIL ")<<name<<(detail.empty()?"":"  ("+detail+")")<<'\n';report.flush();passed&=ok;};
 // The player jumps in from a metre over the deep pool.
 {Game g(pools());auto& p=LabAccess::player(g);p.pos={7,10};p.z=1.f;p.grounded=false;p.verticalVelocity=0;int entries=0;float power=0,gain=0,ai=0;bool stimulus=false;
  for(int i=0;i<60;++i){g.update({},1.f/60);for(const auto&s:g.sounds())if(s.sound==Sound::WaterSplash){gain=std::max(gain,s.gain);ai=std::max(ai,s.aiRadius);}
   for(const auto&st:LabAccess::stimuli(g))stimulus|=std::sqrt(lengthSq(st.source-Vec2{7,10}))<.3f&&st.radius>=5;}
  for(const auto&s:g.waterSplashes())if(s.kind==WaterSplashKind::Entry){++entries;power=s.power;report<<"  entry at "<<fixed(s.time)<<" power "<<fixed(s.power)<<" point "<<fixed(s.point.x)<<","<<fixed(s.point.y)<<","<<fixed(s.point.z)<<'\n';}
  report<<"player jumps in: entry splashes "<<entries<<" power "<<fixed(power)<<" sound gain "<<fixed(gain)<<" ai radius "<<fixed(ai,1)<<" stimulus "<<stimulus<<'\n';
  check(entries==1&&power>2&&gain>.6f&&ai>=8&&stimulus,"the player jumping in splashes hard once, loud, and the AI hears it");}
 // Steps and the wake: walking through the shallow pool, then through the deep one.
 for(bool deep:{false,true}){Game g(pools());auto& p=LabAccess::player(g);p.pos=deep?Vec2{4.2f,10}:Vec2{14.2f,8};p.z=g.world().floorHeight(p.pos.x,p.pos.y);p.angle=0;p.pitch=0;
  int steps=0,wakes=0,stepSounds=0;float stepGain=0;for(int i=0;i<90;++i){InputState in;in.forward=true;g.update(in,1.f/60);
   for(const auto&s:g.sounds())if(s.sound==Sound::WaterSplash){++stepSounds;stepGain=std::max(stepGain,s.gain);}}
  steps=countKind(g,WaterSplashKind::Step);wakes=countKind(g,WaterSplashKind::Wake);
  report<<(deep?"deep":"shallow")<<" walk 1.5 s: step splashes "<<steps<<" wake splashes "<<wakes<<" splash sounds "<<stepSounds<<" loudest "<<fixed(stepGain)<<" walked to x "<<fixed(p.pos.x)<<'\n';
  check(steps>=2&&wakes>=4&&stepSounds>=2&&stepGain<.45f,std::string("walking through ")+(deep?"deep":"shallow")+" water slaps each step and leaves a wake","steps "+std::to_string(steps)+" wakes "+std::to_string(wakes));}
 // A creature killed standing in the pool: its body goes into the water a moment later.
 {Game g(pools());LabAccess::player(g).pos={2,2};LabAccess::spawnCreature(g,{CreatureKind::Huntsman,{7,10}});for(int i=0;i<5;++i)g.update({},1.f/60);
  auto& e=LabAccess::enemies(g).back();const float surface=g.world().waterSurface(e.pos.x,e.pos.y);LabAccess::damageEnemy(g,e,1e4f);int body=0,soundFrame=-1;bool dueLater=false;
  for(int i=0;i<60;++i){g.update({},1.f/60);for(const auto&s:g.waterSplashes())if(s.kind==WaterSplashKind::Body){body=std::max(body,1);dueLater|=!s.sounded;}
   for(const auto&s:g.sounds())if(s.sound==Sound::WaterSplash&&soundFrame<0)soundFrame=i;}
  report<<"creature killed in the pool (feet "<<fixed(e.z)<<", surface "<<fixed(surface)<<"): body splash "<<body<<" due later "<<dueLater<<" sound at frame "<<soundFrame<<'\n';
  check(body==1&&dueLater&&soundFrame>=15&&soundFrame<=30,"a creature dying in water falls in with a splash as its body lands","sound frame "+std::to_string(soundFrame));}
 // A man killed standing in the pool.
 {auto m=regression::room();m->waterVolumes.push_back({3,6,11,14,Bed,Surface});m->actorTracks.push_back(regression::guard({7,10},{7,10}));Game g(regression::campaign(m));LabAccess::player(g).pos={2,2};
  for(int i=0;i<10;++i)g.update({},1.f/60);LabAccess::killNpc(g,0,{2,2});int body=0;for(int i=0;i<60;++i){g.update({},1.f/60);body=std::max(body,countKind(g,WaterSplashKind::Body));}
  report<<"guard killed in the pool: body splashes "<<body<<" (pose clip "<<LabAccess::friendlies(g)[0].pose.clip<<", z "<<fixed(LabAccess::friendlies(g)[0].pose.z)<<")\n";
  check(body==1,"a man dying in water splashes down");}
 // A crate thrown in turns a creature 7 m off towards it.
 {auto m=regression::room();m->waterVolumes.push_back({3,6,11,14,Bed,Surface});m->structures.push_back({14,14,23,14.3f,0,8});m->structures.push_back({14,14,14.3f,23,0,8});Game g(regression::campaign(m));
  LabAccess::player(g).pos={20,20};LabAccess::spawnCreature(g,{CreatureKind::Huntsman,{7,3.2f}});for(int i=0;i<5;++i)g.update({},1.f/60);
  LabAccess::clutter(g)={make(model::wooden_crate,{7,10},Surface+.5f)};auto before=LabAccess::enemies(g).back().state;
  for(int i=0;i<40;++i)g.update({},1.f/60);const auto& e=LabAccess::enemies(g).back();const float off=std::sqrt(lengthSq(e.lastKnown-Vec2{7,10}));
  report<<"creature 6.8 m from a crate falling in: state "<<int(before)<<" -> "<<int(e.state)<<" last known "<<fixed(e.lastKnown.x)<<","<<fixed(e.lastKnown.y)<<" ("<<fixed(off)<<" m from the splash)\n";
  check(e.state==Enemy::State::Investigate&&off<1.2f,"a loud splash draws a creature to it","state "+std::to_string(int(e.state)));}
 // Spray and crown by energy.
 {const int shot=splashCards(WaterSplashKind::Shot,1),step=splashCards(WaterSplashKind::Step,.3f),wake=splashCards(WaterSplashKind::Wake,.5f),gentle=splashCards(WaterSplashKind::Entry,.5f),mid=splashCards(WaterSplashKind::Entry,1.5f),big=splashCards(WaterSplashKind::Entry,3),body=splashCards(WaterSplashKind::Body,2);
  report<<"cards at 0.15 s: shot "<<shot<<" step "<<step<<" wake "<<wake<<" entry p0.5 "<<gentle<<" p1.5 "<<mid<<" p3 "<<big<<" body p2 "<<body<<'\n';
  check(step<shot&&wake<shot&&gentle<mid&&mid<big&&big>shot*2,"spray and crown grow with the energy of what fell in","step "+std::to_string(step)+" < shot "+std::to_string(shot)+", entry "+std::to_string(gentle)+" < "+std::to_string(mid)+" < "+std::to_string(big));}
 // The rings.
 {std::vector<WaterSplash> list;WaterSplash a;a.point={5,5,0};a.time=0;a.power=2;a.kind=WaterSplashKind::Entry;list.push_back(a);
  auto peak=[&](float now,float& radius,float& slope){const auto f=ripples::gather(list,0,now);float h=0;radius=0;slope=0;for(float d=0;d<2.f;d+=.005f){const float v=std::fabs(f.height(5+d,5));if(v>h){h=v;radius=d;}slope=std::max(slope,std::sqrt(lengthSq(f.slope(5+d,5))));}return h;};
  float r1,s1,r2,s2,r3,s3;const float h1=peak(.3f,r1,s1),h2=peak(1.2f,r2,s2),h3=peak(2.45f,r3,s3);
  report<<"entry p2 rings: 0.3 s peak "<<fixed(h1*1000,2)<<" mm at "<<fixed(r1)<<" m slope "<<fixed(s1)<<"; 1.2 s "<<fixed(h2*1000,2)<<" mm at "<<fixed(r2)<<" m slope "<<fixed(s2)<<"; 2.45 s "<<fixed(h3*1000,3)<<" mm\n";
  check(r2>r1+.25f&&h2<h1&&h3<.0002f&&s1>.1f,"a splash's rings spread, fade and are gone in their time");
  WaterSplash b=a;b.point={6,5,0};b.time=.2f;b.kind=WaterSplashKind::Shot;b.power=1;list.push_back(b);WaterSplash c=a;c.point={5,6,0};c.time=.4f;c.kind=WaterSplashKind::Step;c.power=.3f;list.push_back(c);
  const auto f=ripples::gather(list,0,.6f);const auto cut=ripples::cut(f,3,3,9,9);float moving=0;for(const auto&m:cut.moving)moving+=(m.x2-m.x1)*(m.y2-m.y1);
  report<<"three splashes at 0.6 s: "<<f.rings.size()<<" rings, surface cut: "<<cut.still.size()<<" still cells, "<<cut.moving.size()<<" moving cells ("<<fixed(moving)<<" m2) at "<<cut.fine<<"x"<<cut.fine<<'\n';
  check(f.rings.size()==6&&!cut.moving.empty()&&moving<12,"several splashes ring at once, and only where they run is the surface cut fine");}
 return passed;}});

// The real pool: a fall, a round and a step raised on the Starter Map's welcome basin.
const bool rippled=add({.name="water-ripples",.summary="rings and splashes on the Starter Map's welcome pool, filmed before, after and later",.fixtures={"water-pool"},
 .step=[](Scene& s,float){if(s.frame==12){LabAccess::waterSplash(s.g(),{15.2f,15.3f,.42f},2.2f,WaterSplashKind::Entry);LabAccess::waterSplash(s.g(),{16.8f,14.7f,.42f},1.f,WaterSplashKind::Shot);LabAccess::waterSplash(s.g(),{14.1f,16.2f,.42f},.4f,WaterSplashKind::Step);}
  // A wader's wake across the near side: a ring every 32 cm, a tenth of a second apart.
  if(s.frame==40)for(int k=0;k<8;++k)LabAccess::waterSplash(s.g(),{13.8f+k*.32f,14.3f,.42f},.9f,WaterSplashKind::Wake,k*.09f);},
 .capture=[](Scene& s,const Frame& f){if(f.index==11)s.shoot("before");if(f.index==20)s.shoot("after");if(f.index==50)s.shoot("later");if(f.index==85)s.shoot("rings");},
 .metrics=[](Scene& s,Report& r){const auto f=ripples::gather(s.g().waterSplashes(),s.g().level(),s.g().elapsed());r.metric("rings",double(f.rings.size()));
  float hmax=0;for(float x=13.2f;x<17.8f;x+=.02f)for(float y=13.7f;y<17.3f;y+=.02f)hmax=std::max(hmax,std::fabs(f.height(x,y)));r.metric("peak_mm",hmax*1000,"mm");
  int floating=0,sunk=0;for(const auto&c:LabAccess::clutter(s.g()))if(c.model>=0&&c.pos.x>13&&c.pos.x<18&&c.pos.y>13.6f&&c.pos.y<17.4f){if(c.z>.05f)++floating;else ++sunk;}
  r.metric("props_afloat",floating);r.metric("props_sunk",sunk);
  r.check("rings still run on the pool",f.rings.size()>=3&&hmax>.0005f,"rings "+std::to_string(f.rings.size())+" peak "+fixed(hmax*1000)+" mm");},
 .reporter=[](Scene& s,Overlay& o){const auto f=ripples::gather(s.g().waterSplashes(),s.g().level(),s.g().elapsed());
  for(const auto&r:f.rings){const float age=f.now-r.born,rad=ripples::radius(r,age);for(int k=0;k<32;++k){const float a=k*kTwoPi/32,b=(k+1)*kTwoPi/32;o.line({r.x+std::cos(a)*rad,r.y+std::sin(a)*rad,.43f},{r.x+std::cos(b)*rad,r.y+std::sin(b)*rad,.43f},0x40c0ff);}}}});

// Every campaign map and Ashfall zone chunk with water a crate can float in: a wooden crate let go over
// the middle of each such WATER rides it, a metal can goes to its bed, and a round into it splashes.
const bool maps=add({.name="water-maps",.summary="in every campaign map and Ashfall chunk with water, a crate floats, a can sinks to the bed and a round splashes",.standalone=+[]{
 std::ofstream report("water-maps-test.txt");bool passed=true;int pools=0,floated=0,sank=0,splashed=0;
 for(WorldId id:{WorldId::Campaign,WorldId::Ashfall}){Game probe=Game::mapInspection({12,12},0,0,0,false,-999,false,id);const int count=probe.chunkCount();
  for(int level=0;level<count;++level){Game g=Game::mapInspection({12,12},0,0,level,false,-999,false,id);if(g.level()!=level)continue;
   const auto volumes=g.world().waterVolumes();int tried=0;
   for(const auto&v:volumes){const World& w=g.world();
    // Open water: a spot whose metre round it is clear from the bed to 2.4 m over the surface, with no
    // walkway or fixture standing in it (the bed under it is the pool's own).
    auto open=[&](float x,float y){for(float dx:{-.5f,0.f,.5f})for(float dy:{-.5f,0.f,.5f}){const float px=x+dx,py=y+dy;if(px<=v.x1||px>=v.x2||py<=v.y1||py>=v.y2)return false;
      if(w.waterSurface(px,py)<v.surface-.001f||w.supportBelow(px,py,v.surface+1.5f)>v.surface-.3f||!w.fits(px,py,v.surface+.05f,2.4f,true,true))return false;}return true;};
    float cx=-1,cy=-1;for(float y=v.y1+.6f;y<v.y2-.5f&&cx<0;y+=.5f)for(float x=v.x1+.6f;x<v.x2-.5f;x+=.5f)if(open(x,y)&&open(x+.9f,y)){cx=x;cy=y;break;}
    if(cx<0||tried>=2){if(id==WorldId::Ashfall)report<<"ashfall "<<level<<" water "<<fixed(v.x1,1)<<","<<fixed(v.y1,1)<<"-"<<fixed(v.x2,1)<<","<<fixed(v.y2,1)<<" surface "<<fixed(v.surface)<<": no open spot a crate floats in (bed "<<fixed(w.supportBelow((v.x1+v.x2)*.5f,(v.y1+v.y2)*.5f,v.surface+1.5f))<<")\n";continue;}
    ++tried;++pools;const float bed=w.supportBelow(cx,cy,v.surface+1.5f);
    LabAccess::setGod(g,true);auto& list=LabAccess::clutter(g);const size_t crate=list.size();list.push_back(make(model::wooden_crate,{cx,cy},v.surface+.4f));
    list.push_back(make(model::trash_can,{cx+.9f,cy},v.surface+.2f));
    for(int i=0;i<240;++i)LabAccess::stepProps(g,1.f/60);
    const auto& c=LabAccess::clutter(g)[crate];const auto& k=LabAccess::clutter(g)[crate+1];const float canBed=w.supportBelow(k.pos.x,k.pos.y,k.z+.1f,true);
    const bool afloat=c.z<v.surface&&c.z+c.height()>v.surface,onBed=k.z-canBed<.15f&&k.z<v.surface-.2f&&std::fabs(k.vz)<.05f; // the coast's seabed slopes under its outline
    const Vec2 aim{cx+.45f,cy-.45f};LabAccess::setCamera(g,aim,v.surface+.8f,0,0);const size_t before=g.waterSplashes().size();ShotHit hit;LabAccess::fireAt(g,{aim.x+.01f,aim.y,v.surface},&hit);const bool splash=g.waterSplashes().size()>before;
    floated+=afloat;sank+=onBed;splashed+=splash;
    report<<(id==WorldId::Ashfall?"ashfall ":"campaign ")<<level<<" water "<<fixed(v.x1,1)<<","<<fixed(v.y1,1)<<"-"<<fixed(v.x2,1)<<","<<fixed(v.y2,1)<<" at "<<fixed(cx,1)<<","<<fixed(cy,1)<<" surface "<<fixed(v.surface)<<" bed "<<fixed(bed)
          <<": crate z "<<fixed(c.z)<<" (afloat "<<afloat<<") can z "<<fixed(k.z)<<" bed under it "<<fixed(canBed)<<" (on bed "<<onBed<<") round splash "<<splash<<'\n';
    if(!afloat||!onBed||!splash)passed=false;}}}
 report<<(passed&&pools>0?"PASS":"FAIL")<<" "<<pools<<" pools: "<<floated<<" crates afloat, "<<sank<<" cans on the bed, "<<splashed<<" rounds splashed\n";
 return passed&&pools>0;}});
}
}

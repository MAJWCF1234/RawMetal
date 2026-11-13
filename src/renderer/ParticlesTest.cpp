#include "Particles.h"
#include "../game/EngineRegressionScene.h"
#include "../world/CampaignExport.h"
#include "../world/CampaignKit.h"
#include "../world/CampaignMaps.h"
#include "../world/CampaignParticles.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace retro {
namespace {
std::string number(float v){char text[32];std::snprintf(text,sizeof text,"%.3f",double(v));return text;}
// One of everything in a plain room: every kind of EMITTER and every hazard that shows in the air.
std::shared_ptr<AuthoredMapData> airRoom(){
 auto m=regression::room();m->name="Particles inspection";m->definition.ceiling=3.4f;m->definition.ambient=.2f;m->definition.playerStart={4.f,12.f};
 m->lights.push_back(kit::lamp({9.f,12.f},3.2f,1.4f,9.f,4000));m->lights.push_back(kit::lamp({16.f,9.f},3.2f,1.1f,8.f,3200));
 kit::steamLeak(*m,{8.f,14.5f},1.4f,{.9f,-.2f,.1f});kit::steamJet(*m,{8.5f,9.f},.9f,{0,0,1.8f});kit::smokeColumn(*m,{12.f,15.f},.1f,2.4f,.8f);
 kit::sparkShower(*m,{6.f,16.f},1.6f,{.3f,-.4f,-.2f},2.f,26);kit::dustMotes(*m,{9.f,12.f},.2f,1.f,1.f,2.6f,60);kit::dripping(*m,{13.5f,10.f},3.3f,1.2f);
 kit::waterSpray(*m,{14.f,13.f},.6f,{-.6f,.3f,.4f});kit::fire(*m,{15.5f,15.5f},0.f,.35f,.7f);kit::toxicPool(*m,{17.f,11.f},0.f,1.4f,1.f,.5f);
 kit::ceilingGrit(*m,{11.f,9.f},3.35f,10);kit::arcGlow(*m,{6.5f,9.f},.4f,.3f);
 kit::hazard(*m,Hazard::Kind::Steam,19.f,13.f,20.f,14.5f,-1,2.4f,10);kit::hazard(*m,Hazard::Kind::Fire,19.5f,16.f,20.5f,17.f,-1,1.5f,10);kit::hazard(*m,Hazard::Kind::Toxic,18.f,6.f,21.f,8.f,-1,1.f,4);
 kit::hazard(*m,Hazard::Kind::Electricity,5.f,6.f,7.f,7.f,-1,1.f,10);kit::hazard(*m,Hazard::Kind::Pressure,21.f,10.f,22.f,11.f,-1,2.4f,10);kit::hazard(*m,Hazard::Kind::FallingDebris,11.f,17.f,12.5f,18.5f,-1,3.4f,5);
 return m;}
struct Recorded {std::vector<float> values;int cards=0;};
// Which row of which atlas a card is cut from (for the report).
std::string cardKind(const MeshVertex* q,const Particles::Texture& t){if(&t==&Particles::glowAtlas())return "glow";static const char*const rows[]={"smoke","soot","steam","dust","toxic","mist","arc","solid"};return rows[std::clamp(int(q[2].v*8.f),0,7)];}
float lengthOf(Point3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
Point3 unitOf(Point3 v){const float l=lengthOf(v);return l>1e-6f?v*(1.f/l):Point3{1,0,0};}
// Looking from `at` toward the nearest of a map's emitters (level), or along +x when it has none.
Point3 faceToward(const std::vector<ParticleEmitter>& e,Point3 at){float best=1e9f;Point3 to{1,0,0};
 for(const auto&x:e){const Point3 d{x.position.x-at.x,x.position.y-at.y,0};const float l=lengthOf(d);if(l>.3f&&l<best){best=l;to=d;}}return unitOf(to);}
// Where to stand to see `target` from about `distance` away: on open floor, with a clear line to it.
struct Stand {int level=0;Vec2 eye{};float feet=0,yaw=0,pitch=0;bool found=false;};
Stand standFor(int level,Point3 target,float distance=2.8f,float above=4.5f){Stand s;s.level=level;
 Game g=Game::mapInspection({12,12},0,0,level,true,-999,true);const World&w=g.chunkWorld(g.level());
 for(float scale:{1.f,1.4f,.75f,1.9f,.55f})for(int k=0;k<16;++k){const float a=float(k)*kTwoPi/16+.3f;const Vec2 at{target.x+std::cos(a)*distance*scale,target.y+std::sin(a)*distance*scale};
  if(at.x<.5f||at.y<.5f||at.x>23.5f||at.y>23.5f)continue;const float floor=w.supportBelow(at.x,at.y,target.z+1.f);if(target.z-floor>above||floor-target.z>1.f)continue;
  const float eyeZ=floor+1.6f;if(!Particles::open(w,{at.x,at.y,eyeZ})||!w.fits(at.x,at.y,floor+.05f,1.5f,false,false))continue;
  if(!w.rayClear(at,eyeZ,{target.x,target.y},target.z+.3f,true,false))continue;
  s.eye=at;s.feet=floor;s.yaw=std::atan2(target.y-at.y,target.x-at.x)*180.f/kPi;const float run=std::hypot(target.x-at.x,target.y-at.y);
  s.pitch=std::atan2(target.z+.4f-eyeZ,run)*140.f;s.found=true;return s;}
 return s;}
void savePpm(const SoftwareRenderer& d,const std::filesystem::path& file,int w,int h){std::ofstream out(file,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";
 for(int k=0;k<w*h;++k){const auto px=d.pixels()[k];const char rgb[]={char(px>>16),char(px>>8),char(px)};out.write(rgb,3);}}
// Strips at 30 frames a second of a steam leak, a spark shower, a shot's dust and a grenade's smoke, on
// Vulkan (and the software rasteriser with RM_PARTICLE_SOFTWARE), and a map-shots list of places for
// the before and after sheet.
bool writeParticleStrips(const std::string& folder,std::ostream& report){
 namespace fs=std::filesystem;fs::create_directories(folder);
 const bool software=std::getenv("RM_PARTICLE_SOFTWARE")!=nullptr;
 auto storage=std::make_unique<SoftwareRenderer>(960,540);auto&d=*storage;if(!software&&!d.enableHardware())return false;d.setEnvironmentInspection(true);
 const std::string tag=software?"cpu-":"gpu-";
 // The first emitter of a kind (from map `first` on) that can be seen from somewhere to stand.
 auto findKind=[&](ParticleKind kind,int first,int last,int& level,Point3& at){for(int l=first;l<=last;++l){auto m=campaignMap(l);if(!m)continue;for(const auto&e:m->emitters)if(e.kind==kind){const Point3 p{e.position.x,e.position.y,e.z};if(!standFor(l,p,2.6f,2.4f).found)continue;level=l;at=p;return true;}}return false;};
 auto scene=[&](const Stand& s){return Game::mapInspection(s.eye,s.yaw*kPi/180.f,s.pitch,s.level,true,s.feet,true);};
 auto film=[&](Game& g,const std::string& name,int frames,int every=2){for(int i=0;i<frames;++i){for(int k=0;k<every;++k)g.update({},1.f/60);d.render(g);char file[64];std::snprintf(file,sizeof file,"%s%s-%03d.ppm",tag.c_str(),name.c_str(),i);savePpm(d,fs::path(folder)/file,960,540);}};
 std::ofstream places(fs::path(folder)/"places.txt");int strips=0;
 // RM_PARTICLE_ONLY=steam,room (a look round): only those strips.
 const std::string only=std::getenv("RM_PARTICLE_ONLY")?std::getenv("RM_PARTICLE_ONLY"):"";
 auto want=[&](const char* name){return only.empty()||only.find(name)!=std::string::npos;};
 if(want("steam")){int level=0;Point3 at;if(findKind(ParticleKind::Steam,34,67,level,at)){const auto s=standFor(level,at,2.4f,2.4f);if(s.found){Game g=scene(s);film(g,"steam",24);++strips;report<<"steam strip: map "<<level<<"\n";}}}
 if(want("sparks")){int level=0;Point3 at;if(findKind(ParticleKind::Sparks,34,109,level,at)){const auto s=standFor(level,at,2.6f,2.4f);if(s.found){Game g=scene(s);
   for(int i=0;i<60*20;++i){g.update({},1.f/60);bool shower=false;for(const auto&b:g.electricNow(g.level()).bursts)shower|=g.elapsed()-b.time<.03f&&std::fabs(b.at.x-at.x)<.3f&&std::fabs(b.at.y-at.y)<.3f;if(shower)break;}
   film(g,"sparks",24);++strips;report<<"spark strip: map "<<level<<"\n";}}}
 if(want("gunshot")){const Stand s{55,{12,12},0,0,0,false};Game g=Game::mapInspection({5.f,13.f},0,0,55,true,-999,true);const World&w=g.chunkWorld(g.level());
  // Face the nearest wall a few metres off, and fire into it.
  float best=0;for(int k=0;k<16;++k){const float a=float(k)*kTwoPi/16;float reach=0;for(float r=.5f;r<9.f;r+=.25f){if(!w.fits(5.f+std::cos(a)*r,13.f+std::sin(a)*r,w.supportBelow(5.f,13.f,g.player().z+1.f)+1.2f,.1f,false,false))break;reach=r;}if(reach>3.f&&reach<7.f&&(best==0||std::fabs(reach-4.5f)<std::fabs(best-4.5f))){best=reach;auto&p=const_cast<Player&>(g.player());p.angle=a;p.pitch=-6;}}
  (void)s;g.update({},1.f/60);g.equipWeapon(WeaponId::Shotgun);for(int i=0;i<40;++i)g.update({},1.f/60);InputState fire{};fire.fire=true;g.update(fire,.02f);film(g,"gunshot",60);++strips;report<<"gunshot strip: map 55, wall "<<best<<" m\n";}
 if(want("grenade")){Game g=Game::mapInspection({12.f,7.f},kPi*.5f,-8,86,true,-999,true);const World&w=g.chunkWorld(g.level());for(int i=0;i<3;++i)g.update({},1.f/60);
  const Vec2 at{12.f+std::cos(g.player().angle)*5.f,7.f+std::sin(g.player().angle)*5.f};
  // Thrown from beside the player: its trail as it flies and smokes where it lies, then the blast.
  const float floor=w.supportBelow(at.x,at.y,g.player().z+1.f),thrown=g.elapsed()+.02f,flight=.6f,fuse=1.f;
  const Point3 from{12.f+std::cos(g.player().angle+.5f)*.8f,7.f+std::sin(g.player().angle+.5f)*.8f,g.player().z+1.5f};
  const Point3 velocity{(at.x-from.x)/flight,(at.y-from.y)/flight,(floor+.06f-from.z+4.9f*flight*flight)/flight};
  d.particleMemory().extra.throws={{from,velocity,thrown,fuse,g.level()}};
  d.particleMemory().extra.blasts={{at,floor+.55f,thrown+fuse,g.level()}};film(g,"grenade-a",48);film(g,"grenade-b",24,15);d.particleMemory().extra.blasts.clear();d.particleMemory().extra.throws.clear();++strips;report<<"grenade strip: map 86\n";}
 // One of everything in the plain room (airRoom), seen from its door: steam, a jet, a smoke column,
 // dust in the lamp, drips, spray, fire, gas, grit and the glow round current, a couple of seconds of it.
 if(want("room")){Game g(regression::campaign(airRoom()));{auto&p=const_cast<Player&>(g.player());p.pos={3.2f,12.f};p.angle=0;p.pitch=4;}for(int i=0;i<120;++i)g.update({},1.f/60);film(g,"room",8,15);report<<"room strip: the air room\n";}
 // Places for the before and after sheet: the hero rooms, then the first of each dressed kind.
 auto place=[&](const std::string& name,int level,Point3 at,float distance){const auto s=standFor(level,at,distance);if(!s.found){report<<"no place for "<<name<<"\n";return;}
  places<<name<<' '<<level<<' '<<s.eye.x<<' '<<s.eye.y<<' '<<s.feet<<' '<<s.yaw<<' '<<s.pitch<<" wait 30\n";};
 for(int level:{0,49,51,86,102}){auto m=campaignMap(level);if(!m||m->emitters.empty())continue;size_t pick=0;float most=0;for(size_t i=0;i<m->emitters.size();++i){const auto&e=m->emitters[i];const float weight=float(e.count)*e.strength*(e.kind==ParticleKind::Sparks?.2f:1.f);if(weight>most){most=weight;pick=i;}}
  const auto&e=m->emitters[pick];place("hero-"+std::to_string(level),level,{e.position.x,e.position.y,e.z},3.4f);}
 for(auto [kind,name,first,last]:{std::tuple{ParticleKind::Steam,"steam",34,67},{ParticleKind::Dust,"dust",34,67},{ParticleKind::Smoke,"smoke",34,67},{ParticleKind::Drips,"drips",68,109},{ParticleKind::Steam,"vent",68,109}}){int level=0;Point3 at;if(findKind(kind,first,last,level,at))place(std::string(name)+"-"+std::to_string(level),level,at,2.8f);}
 return strips==4||!only.empty();}
}

bool testParticles(){
 std::ofstream report("particles-test.txt");bool passed=true;
 auto check=[&](bool ok,const std::string&text){report<<(ok?"PASS":"FAIL")<<": "<<text<<'\n';report.flush();passed&=ok;};
 // RM_PARTICLE_STRIPS_ONLY (with RM_PARTICLE_FRAMES): the strips alone, for a look round.
 if(const char*folder=std::getenv("RM_PARTICLE_FRAMES");folder&&std::getenv("RM_PARTICLE_STRIPS_ONLY")){passed=writeParticleStrips(folder,report);report<<(passed?"PASS":"FAIL")<<'\n';return passed;}
 const auto room=airRoom();const auto campaign=regression::campaign(room);
 Game g(campaign);{auto&p=const_cast<Player&>(g.player());p.pos={3.5f,12.f};p.angle=0;p.pitch=0;}for(int i=0;i<3;++i)g.update({},1.f/60);
 const Point3 eye{3.5f,12.f,1.6f};
 auto view=[&](float now,Point3 at=Point3{3.5f,12.f,1.6f},Point3 forward=Point3{1,0,0}){Particles::View v;v.eye=at;v.forward=forward;v.pixel=1.f/900;v.level=0;v.now=now;return v;};
 auto light=[](Point3 p,Point3 n){return .4f+.05f*p.z+.1f*n.z;};
 auto record=[&](ParticleMemory&memory,float now,const Particles::Extra*extra,ParticleStats*stats=nullptr){Recorded r;
  auto s=Particles::draw(memory,g,view(now),[&](const MeshVertex*q,const Particles::Texture&t,float l,const Particles::CardLook& look){++r.cards;r.values.push_back(l);r.values.push_back(float(&t==&Particles::glowAtlas()));
   r.values.push_back(look.opacity);r.values.push_back(look.soft);r.values.push_back(look.blend);r.values.push_back(look.step);
   for(int k=0;k<4;++k){r.values.push_back(q[k].p.x);r.values.push_back(q[k].p.y);r.values.push_back(q[k].p.z);r.values.push_back(q[k].u);r.values.push_back(q[k].v);r.values.push_back(q[k].light);}},light,[](Point3,Point3){return true;},extra);
  if(stats)*stats=s;return r;};
 Particles::Extra events;const float now=40.f;
 events.blasts.push_back({{14.f,8.f},.55f,now-1.5f,0});
 events.hits.push_back({{22.95f,12.f,1.3f},{-1,0,0},now-.4f,0,ImpactMaterial::Concrete,-1,1.f,false,0.f,9.f});
 events.hits.push_back({{12.f,12.f,3.39f},{0,0,-1},now-.3f,0,ImpactMaterial::Plaster,-1,1.f,false,0.f,9.f});
 events.shots.push_back({{3.5f,12.f,1.5f},{1,0,0},20.f,now-.5f,10.f,1.f,0});
 // 1. The same moment draws the same frame: asked in order, again after a later one, and fresh.
 {auto a=makeParticleMemory(),b=makeParticleMemory();ParticleStats stats;const auto first=record(*a,now,&events,&stats);record(*a,now+2.7f,&events);record(*a,now-6.f,&events);const auto again=record(*a,now,&events);const auto fresh=record(*b,now,&events);
  report<<"room at "<<now<<" s: "<<stats.sources<<" sources, "<<stats.blended<<" blended, "<<stats.glowing<<" glowing, "<<stats.thinned<<" thinned, "<<stats.culled<<" culled, "<<stats.inside<<" inside, "<<stats.clearanceProbes<<" clearance probes, "<<number(stats.ms)<<" ms\n";
  check(first.cards>200&&stats.blended>150&&stats.glowing>20,"one of everything draws ("+std::to_string(first.cards)+" cards: "+std::to_string(stats.blended)+" blended, "+std::to_string(stats.glowing)+" glowing)");
  for(size_t i=0;i<first.values.size()&&i<again.values.size()&&i<fresh.values.size();++i)if(first.values[i]!=again.values[i]||first.values[i]!=fresh.values[i]){
   report<<"  first difference at value "<<i<<" (card "<<i/30<<", slot "<<i%30<<"): "<<first.values[i]<<" / "<<again.values[i]<<" / "<<fresh.values[i]<<"\n";break;}
  check(first.values==again.values&&first.values==fresh.values,"the same moment draws the same cards asked again after other moments and asked fresh ("+std::to_string(first.cards)+", "+std::to_string(again.cards)+", "+std::to_string(fresh.cards)+")");
  bool finite=true;for(float v:first.values)finite&=std::isfinite(v);check(finite,"every corner, uv and light is finite");
  // A source left alone keeps drawing: the room a minute on still has its air.
  const auto later=record(*a,now+60.f,nullptr);check(later.cards>150,"a minute on the room's own sources still draw ("+std::to_string(later.cards)+" cards)");}
 // 2. Far to near: every blended card is drawn after the ones behind it.
 {auto memory=makeParticleMemory();float last=1e9f;bool ordered=true;int blended=0;
  Particles::draw(*memory,g,view(now),[&](const MeshVertex*q,const Particles::Texture&t,float,const Particles::CardLook&){if(&t!=&Particles::blendedAtlas())return;++blended;const Point3 c=(q[0].p+q[2].p)*.5f;const float d=c.x-eye.x;ordered&=d<=last+.35f;last=std::min(last,d);},light,[](Point3,Point3){return true;},&events);
  check(ordered&&blended>100,"blended cards come far to near ("+std::to_string(blended)+")");}
 // 3. The budget holds: sixty blasts and the room at once.
 {Particles::Extra storm=events;for(int i=0;i<60;++i)storm.blasts.push_back({{3.f+float(i%9)*2.2f,4.f+float(i/9)*2.3f},.55f,now-.2f-float(i%7)*.9f,0});
  auto memory=makeParticleMemory();ParticleStats stats;const auto started=std::chrono::steady_clock::now();record(*memory,now,&storm,&stats);
  const float ms=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-started).count();ParticleStats warm;
  const auto warmStart=std::chrono::steady_clock::now();record(*memory,now+.0333f,&storm,&warm);const float warmMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-warmStart).count();
  report<<"sixty blasts: "<<stats.blended<<" blended, "<<stats.glowing<<" glowing, "<<stats.thinned<<" thinned; first frame "<<number(ms)<<" ms (collision and clearance found), next "<<number(warmMs)<<" ms\n";
  check(stats.blended<=Particles::BlendedBudget&&stats.glowing<=Particles::GlowBudget&&stats.thinned>0,"sixty blasts at once stay in the budget ("+std::to_string(stats.blended)+" of "+std::to_string(Particles::BlendedBudget)+" blended, "+std::to_string(stats.glowing)+" of "+std::to_string(Particles::GlowBudget)+" glowing, "+std::to_string(stats.thinned)+" thinned)");
  check(warmMs<40.f,"and the next frame of it is worked out in "+number(warmMs)+" ms");}
 // 4. Distance thins: the same room seen from far away draws fewer.
 {auto a=makeParticleMemory(),b=makeParticleMemory();ParticleStats nearStats,farStats;
  Particles::draw(*a,g,view(now,{3.5f,12.f,1.6f}),[](const MeshVertex*,const Particles::Texture&,float,const Particles::CardLook&){},light,[](Point3,Point3){return true;},&events);nearStats=a->last;
  Particles::draw(*b,g,view(now,{-30.f,12.f,1.6f}),[](const MeshVertex*,const Particles::Texture&,float,const Particles::CardLook&){},light,[](Point3,Point3){return true;},&events);farStats=b->last;
  check(farStats.particles<nearStats.particles*.8f&&farStats.thinned>nearStats.thinned,"seen from 34 m the room thins ("+std::to_string(farStats.particles)+" against "+std::to_string(nearStats.particles)+" particles)");}
 // 5. The EMITTER record: every kind, its strength and its gate read, write back out the same, and the old record still reads.
 {auto text=[](const std::string& records){
   std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Air validation|0\nMAP|0|Air|0|0|3.5|12|0|0|4.5|0.7|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
   for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+(y==0||y==23?std::string(24,'#'):"#"+std::string(22,'.')+"#")+'\n';
   return t+records+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";};
  std::string records="EMITTER|0|6|12|0.5|0.1|0.7|0.45|0.7|0.05|0.4|7\n";
  for(int kind=0;kind<int(ParticleKind::Count);++kind)records+="EMITTER|0|"+std::to_string(4+kind)+"|10|1.2|0.3|-0.2|0.8|0.5|0.06|0.4|12|"+std::to_string(kind)+"|"+(kind%2?"1.5":"")+"|"+(kind%3==0?"air_test_state":"")+"|"+(kind%3==0?"2":"")+"|"+(kind==6?"1":"")+"|"+(kind==4?"6|1.5|0.5":"")+"\n";
  bool read=false,same=false,old=false;std::string why;
  try{auto c=loadCustomCampaignText(text(records),"air.rmc");const auto&e=c->maps[0]->emitters;read=e.size()==size_t(ParticleKind::Count)+1;
   if(read){old=e[0].kind==ParticleKind::Plume&&e[0].strength==1&&!e[0].enabledFlag&&e[0].count==7;
    for(int kind=0;kind<int(ParticleKind::Count);++kind){const auto&x=e[size_t(kind+1)];read&=x.kind==ParticleKind(kind)&&x.strength==(kind%2?1.5f:1.f)&&x.enabledFlag==(kind%3==0?stateId("air_test_state"):0)&&x.enabledValue==(kind%3==0?2:1)&&x.invertFlag==(kind==6)&&x.period==(kind==4?6.f:0.f)&&x.onTime==(kind==4?1.5f:0.f)&&x.phase==(kind==4?.5f:0.f)&&x.drift.y==-.2f&&x.rise==.8f;}}
   const auto exported=exportRmc("Air validation",{{c->maps[0],-1}},0,{});auto again=loadCustomCampaignText(exported.text,"air-again.rmc");
   const auto differences=compareAuthoredMaps(*c->maps[0],*again->maps[0],true);same=differences.empty()&&again->maps[0]->emitters.size()==e.size();if(!differences.empty())why=differences.front();}
  catch(const std::exception& x){why=x.what();}
  check(read&&old,"EMITTER reads every kind, its strength and its gate, and the old twelve-field record still reads as a plume"+(why.empty()?std::string():" ("+why+")"));
  check(same,"and exports to the same records"+(why.empty()?std::string():" ("+why+")"));
  auto refuses=[&](const std::string& r){try{loadCustomCampaignText(text(r),"bad.rmc");return false;}catch(const std::exception&){return true;}};
  check(refuses("EMITTER|0|6|12|1|0|0|1|0.5|0.1|0.4|8|11\n")&&refuses("EMITTER|0|6|12|1|0|0|1|0.5|0.1|0.4|8|1|0\n")&&refuses("EMITTER|0|6|12|1|0|0|1|0.5|0.1|0.4|0|1\n")&&refuses("EMITTER|0|6|12|1|0|0|1|0.5|0.1|0.4|8|1|1||||4|5\n")&&refuses("EMITTER|0|6|12|1|0|0|-1|0.5|0.1|0.4|8\n"),
   "an unknown kind, no strength, no particles, a window longer than its period and an old plume sinking are refused");}
 // 6. Sparks are the electric system's: a Sparks emitter throws showers through it, and they light the room.
 {int showers=0;float brightest=0;for(int i=0;i<60*5;++i){g.update({},1.f/60);showers=std::max(showers,int(g.electricNow(0).bursts.size()));for(const auto&f:g.weaponFlashes())brightest=std::max(brightest,f.strength);}
  check(showers>=1&&brightest>0,"a Sparks emitter throws showers through the electric system ("+std::to_string(showers)+" at once at most) that light the room (strength "+number(brightest)+")");}
 // 7. Chapters 1 and 2: every map has its air, within its limits, and nothing is drawn inside a wall from where the player stands.
 {int maps=0,dressed=0,over=0,sparkMaps=0,bad=0,shut=0,inside=0,cards=0,probes=0;std::string worst,limits;int most=0,least=1000;
  auto probe=[&](const World& w,Point3 p){++probes;return Particles::open(w,p);};
  for(int level:{0,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109}){
   const auto map=campaignMap(level);if(!map)continue;++maps;const int added=dressedAirCount(level);const auto&e=map->emitters;int sparks=0;for(const auto&x:e)sparks+=x.kind==ParticleKind::Sparks;
   dressed+=added>0;most=std::max(most,int(e.size()));least=std::min(least,int(e.size()));sparkMaps+=sparks>0;if(sparks>2||added>MostDressedEmitters){++over;limits="map "+std::to_string(level)+" has "+std::to_string(e.size())+" emitters, "+std::to_string(sparks)+" sparking";}
   {report<<"  map "<<level<<": "<<e.size()<<" emitters ("<<added<<" dressed):";for(const auto&x:e)report<<' '<<int(x.kind);report<<"\n";}
   const Vec2 start=map->definition.playerStart;Game m=Game::mapInspection(start,0,0,level,true,-999,true);const World&w=m.chunkWorld(m.level());
   for(size_t i=e.size()-size_t(std::min<int>(added,int(e.size())));i<e.size();++i){const auto&x=e[i];const bool fine=Particles::open(w,{x.position.x,x.position.y,x.z})&&validParticleEmitter(x);if(!fine){++bad;limits="map "+std::to_string(level)+" emitter "+std::to_string(i)+" kind "+std::to_string(int(x.kind))+" at "+number(x.position.x)+","+number(x.position.y)+","+number(x.z)+" is in a solid or out of range";}}
   // From the start and from beside each emitter, a blast at the start too.
   std::vector<Point3> eyes{{start.x,start.y,w.supportBelow(start.x,start.y,m.player().z+1.f)+1.6f}};
   for(const auto&x:e){for(Point3 d:{Point3{2.5f,0,0},{-2.5f,0,0},{0,2.5f,0},{0,-2.5f,0}}){const Point3 at{x.position.x+d.x,x.position.y+d.y,0};const float floor=w.supportBelow(at.x,at.y,x.z+1.f);if(Particles::open(w,{at.x,at.y,floor+1.6f})){eyes.push_back({at.x,at.y,floor+1.6f});break;}}if(eyes.size()>5)break;}
   Particles::Extra blast;blast.blasts.push_back({start,w.supportBelow(start.x,start.y,m.player().z+1.f)+.55f,38.5f,m.level()});
   auto memory=makeParticleMemory();
   for(const Point3 at:eyes){Particles::View v;v.eye=at;v.forward=faceToward(e,at);v.pixel=1.f/900;v.level=m.level();v.now=40.f;
    Particles::draw(*memory,m,v,[&](const MeshVertex*q,const Particles::Texture&t,float,const Particles::CardLook&){++cards;const Point3 c=(q[0].p+q[2].p)*.5f;if(!probe(w,c)){++inside;worst="map "+std::to_string(level)+": a "+cardKind(q,t)+" card's middle "+number(c.x)+","+number(c.y)+","+number(c.z)+" is in a solid";return;}
      if(&t!=&Particles::blendedAtlas()||q[0].v>=float(Particles::AtlasRows-1)/float(Particles::AtlasRows))return;
      // A card fades where it nears a surface, so it may lie into one; never deep.
      const Point3 hu=(q[1].p-q[0].p)*.15f,hv=(q[3].p-q[0].p)*.15f;for(Point3 d:{hu,hu*-1.f,hv,hv*-1.f})if(!probe(w,c+d)&&!probe(w,c+d+unitOf(d)*.12f)){++shut;worst="map "+std::to_string(level)+": a "+cardKind(q,t)+" card "+number(lengthOf(q[1].p-q[0].p))+" m across at "+number(c.x)+","+number(c.y)+","+number(c.z)+" cuts into a surface";break;}},
     [&](Point3 p,Point3 n){(void)n;return .5f+.02f*p.z;},[](Point3,Point3){return true;},&blast);}}
  report<<"chapters 1 and 2: "<<maps<<" maps, "<<dressed<<" dressed, "<<least<<" to "<<most<<" emitters a map, "<<sparkMaps<<" with sparks; "<<cards<<" cards drawn from "<<probes<<" probes, "<<inside<<" inside, "<<shut<<" cutting a surface\n";
  if(!worst.empty())report<<"  worst: "<<worst<<'\n';
  check(maps>=77&&dressed>=76&&over==0&&bad==0,"every Chapter 1 and 2 map (and the Foundry) has its air within its limits ("+std::to_string(dressed)+" of "+std::to_string(maps)+" dressed, "+std::to_string(least)+" to "+std::to_string(most)+" emitters, no more than two sparking"+(limits.empty()?"":"; "+limits)+")");
  check(inside==0&&shut==0&&cards>5000,"nothing is drawn inside a wall or cutting deep into a floor, wall or ceiling ("+std::to_string(cards)+" cards, "+std::to_string(inside)+" inside, "+std::to_string(shut)+" cutting"+(worst.empty()?"":"; "+worst)+")");}
 // 8. Both back ends: the room's air draws over the room, the same moment twice the same, and what it costs.
 for(bool vulkan:{true,false}){const std::string backend=vulkan?"Vulkan":"CPU";auto storage=std::make_unique<SoftwareRenderer>(480,270);auto&d=*storage;
  if(vulkan&&!d.enableHardware()){check(false,"Vulkan starts");continue;}d.setEnvironmentInspection(true);
  auto frame=[&](const Game&game){d.render(game);return std::vector<std::uint32_t>(d.pixels(),d.pixels()+480*270);};
  auto bare=room;auto stripped=std::make_shared<AuthoredMapData>(*room);stripped->emitters.clear();stripped->hazards.clear();
  Game plain(regression::campaign(stripped));{auto&p=const_cast<Player&>(plain.player());p.pos={3.5f,12.f};p.angle=0;p.pitch=0;}for(int i=0;i<3;++i)plain.update({},1.f/60);
  const auto without=frame(plain);std::vector<std::uint32_t> with;for(int i=0;i<3;++i)with=frame(g);const auto twice=frame(g);
  int changed=0,worst=0;for(size_t i=0;i<with.size();++i){int most=0;for(int s:{0,8,16})most=std::max(most,std::abs(int((with[i]>>s)&255)-int((without[i]>>s)&255)));changed+=most>10;for(int s:{0,8,16})worst=std::max(worst,std::abs(int((with[i]>>s)&255)-int((twice[i]>>s)&255)));}
  const auto stats=d.particleMemory().last;
  for(int i=0;i<4;++i){d.render(g);d.render(plain);}
  double total=0,bareTotal=0;const int runs=16;
  for(int i=0;i<runs;++i){auto started=std::chrono::steady_clock::now();d.render(g);total+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
   started=std::chrono::steady_clock::now();d.render(plain);bareTotal+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();}
  report<<backend<<": "<<changed<<" pixels changed by the air, repeat difference "<<worst<<"; frame "<<number(float(total/runs))<<" ms with it, "<<number(float(bareTotal/runs))<<" ms without; particles "<<stats.particles<<" ("<<stats.blended<<" blended, "<<stats.glowing<<" glowing) in "<<number(stats.ms)<<" ms\n";
  check(changed>480*270/12,backend+": the air draws over the room ("+std::to_string(changed)+" pixels)");
  check(worst<=(vulkan?2:0),backend+": the same moment draws the same frame twice (peak channel difference "+std::to_string(worst)+")");}
 // 9. Frame strips for inspection: RM_PARTICLE_FRAMES=<folder>.
 if(const char*folder=std::getenv("RM_PARTICLE_FRAMES"))passed&=writeParticleStrips(folder,report);
 report<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}

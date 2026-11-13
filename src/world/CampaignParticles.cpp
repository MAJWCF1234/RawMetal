#include "CampaignParticles.h"
#include "CampaignKit.h"
#include <algorithm>
#include <cmath>
#include <vector>
// The air of Chapters 1 and 2. Every map of the descent (chunks 34-109) is given EMITTER records, the
// same a custom map writes, from what stands in it:
//
//  - a steam main weeps at a joint, out into the room, below head height where it can be seen;
//  - a vent grille breathes a faint warm vapour;
//  - an opened breaker panel or a cable bundle spits sparks now and then (the electric system throws
//    them, so they bounce and light the room); a closed cabinet only rarely;
//  - a generator smokes a little from its exhaust, a burn barrel burns;
//  - a floor drain is dripped on from the ceiling over it, a sink's tap drips, water in a basin is
//    dripped into;
//  - in a tall room the strongest lamp has dust turning in its light;
//  - the deep levels shed grit from their ceilings.
//
// A few of each to a map at most, never two close together, faint unless the room is a set piece: it
// is atmosphere, and the stealth needs its sight lines. The set pieces get more by hand: the Foundry's
// pour, the Sump's wet well, the cooling towers' vapour and rain, the High-Energy Test Bay and the
// Breach Sector. An old plume beside a pulsing steam hazard is timed to it.
//
// It runs after the chapter files, so it never moves what they place.
namespace retro {
namespace {
using Map=AuthoredMapData;
std::array<int,CampaignChunkCount> dressedCounts{};
std::uint32_t mix32(std::uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return v;}
struct Air {
 Map& m;int chunk;const World& w;size_t before=0;int sparks=0;
 float dice(std::uint32_t salt)const{return float(mix32(std::uint32_t(chunk)*0x9e3779b9u^salt*0x85ebca6bu)>>8)*(1.f/16777216.f);}
 float ground()const{return m.layers.empty()?0.f:m.layers.front().elevation;}
 bool open(Point3 p)const{return p.x>.3f&&p.y>.3f&&p.x<23.7f&&p.y<23.7f&&w.fits(p.x,p.y,p.z,.01f,false,false);}
 // Open for `r` round about, and up.
 bool roomy(Point3 p,float r)const{if(!open(p))return false;for(Point3 d:{Point3{r,0,0},{-r,0,0},{0,r,0},{0,-r,0},{0,0,r}})if(!open(p+d))return false;return true;}
 float floorAt(Vec2 p,float z)const{return w.supportBelow(p.x,p.y,z+.05f);}
 float roofAt(Vec2 p,float z)const{return w.clearanceAbove(p.x,p.y,z);}
 bool apart(Vec2 p,float d)const{for(const auto&e:m.emitters)if(length(e.position-p)<d)return false;return true;}
 int count(ParticleKind kind)const{int n=0;for(size_t i=before;i<m.emitters.size();++i)n+=m.emitters[i].kind==kind;return n;}
 int added()const{return int(m.emitters.size()-before);}
 bool full()const{return added()>=MostDressedEmitters;}
 // The way out of something at `c` into the room: the level direction with the longest open run.
 bool outward(Point3 c,float from,Vec2& out)const{float best=0;
  for(Vec2 d:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){float run=0;for(float r=from;r<from+1.4f;r+=.2f){if(!open({c.x+d.x*r,c.y+d.y*r,c.z}))break;run=r-from+.2f;}
   if(run>best+.01f){best=run;out=d;}}
  return best>=.6f;}
 std::vector<const Fixture*> fixtures(std::initializer_list<int> models,std::uint32_t salt)const{std::vector<std::pair<float,const Fixture*>> found;
  for(const auto&f:m.fixtures)if(f.visible&&std::find(models.begin(),models.end(),f.model)!=models.end())found.push_back({dice(salt^std::uint32_t(&f-m.fixtures.data())*0x27d4eb2fu),&f});
  std::sort(found.begin(),found.end(),[](const auto&a,const auto&b){return a.first<b.first;});std::vector<const Fixture*> out;for(const auto&f:found)out.push_back(f.second);return out;}
 // ---- What stands in the room ----
 void pipeLeaks(int most,std::uint32_t salt=1){
  std::vector<std::pair<float,const PipeRun*>> runs;
  for(const auto&p:m.pipes){if(p.endZ>-999||p.radius<.05f)continue;const float run=length(p.end-p.start);if(run<1.5f)continue;runs.push_back({dice(salt^std::uint32_t(&p-m.pipes.data())*0x165667b1u),&p});}
  std::sort(runs.begin(),runs.end(),[](const auto&a,const auto&b){return a.first<b.first;});int placed=0;
  for(const auto&[order,pipe]:runs){if(placed>=most||full())break;const Vec2 along=(pipe->end-pipe->start)*(1.f/length(pipe->end-pipe->start)),side{-along.y,along.x};
   for(float t:{.35f+.3f*order,.5f,.25f,.75f}){const Vec2 at=pipe->start+(pipe->end-pipe->start)*t;const float floor=floorAt(at,pipe->z);const float height=pipe->z-floor;
    if(height<.6f||height>3.2f||roofAt(at,pipe->z+pipe->radius+.05f)-pipe->z<1.f)continue;bool done=false;
    for(float s:{1.f,-1.f}){const Vec2 out=side*s;const Point3 origin{at.x+out.x*(pipe->radius+.06f),at.y+out.y*(pipe->radius+.06f),pipe->z-pipe->radius*.3f};
     if(!open(origin)||!roomy({origin.x+out.x*.6f,origin.y+out.y*.6f,origin.z},.3f)||!apart({origin.x,origin.y},5.f))continue;
     kit::steamLeak(m,{origin.x,origin.y},origin.z,{out.x*.8f,out.y*.8f,-.05f},.5f+.25f*dice(salt+7u));done=true;break;}
    if(done){++placed;break;}}}}
 void vents(int most){int placed=0;
  for(const auto*f:fixtures({model::pwr_vent_grille,model::vent},3)){if(placed>=most||full())break;const Point3 c{f->position.x,f->position.y,ground()+f->base+f->height*.5f};Vec2 out;
   if(!outward(c,std::max(f->width,f->depth)*.5f+.05f,out))continue;const Point3 at{c.x+out.x*(f->depth*.5f+.08f),c.y+out.y*(f->depth*.5f+.08f),c.z};
   // Not one tucked under the ceiling: what it breathes would only flatten there.
   const float floor=floorAt({at.x,at.y},at.z),roof=roofAt({at.x,at.y},at.z);if(!open(at)||at.z-floor<.4f||roof-at.z<1.f||!apart({at.x,at.y},5.f))continue;
   // It breathes: a warm exhalation every few seconds, out into the room and up, then a lull.
   kit::pulsed(kit::airEmitter(m,ParticleKind::Steam,{at.x,at.y},at.z,{out.x*.75f,out.y*.75f,.3f},2.8f,34,.06f,.8f,.55f),4.2f,2.6f,4.2f*dice(500u+std::uint32_t(placed)));++placed;}}
 void sparking(int most){
  // An opened panel or a loose bundle sparks for sure; a shut cabinet only now and then.
  for(const auto*f:fixtures({model::pwr_breaker_panel_open,model::pwr_cable_bundle},5)){if(sparks>=most||full())break;place(*f);}
  if(sparks<most&&dice(13)<.22f)for(const auto*f:fixtures({model::pwr_switchgear_cabinet,model::pwr_step_transformer,model::transformer,model::anl_switch_cabinet_1,model::anl_switch_cabinet_2,model::pwr_breaker_panel,model::electrical_cabinet,model::pwr_tall_switch_cabinet},6)){if(sparks>=std::min(most,1)||full())break;place(*f);}}
 void place(const Fixture& f){const Point3 c{f.position.x,f.position.y,ground()+f.base+std::min(1.7f,f.height*.62f)};Vec2 out;
  if(!outward(c,std::max(f.width,f.depth)*.5f+.05f,out))return;
  float reach=.05f;while(reach<1.2f&&!open({c.x+out.x*reach,c.y+out.y*reach,c.z}))reach+=.03f;const Point3 at{c.x+out.x*(reach+.05f),c.y+out.y*(reach+.05f),c.z};
  if(!open(at)||!apart({at.x,at.y},4.f))return;
  kit::sparkShower(m,{at.x,at.y},at.z,{out.x*.7f,out.y*.7f,-.35f},3.5f+4.5f*dice(17u+std::uint32_t(sparks)),18+int(10*dice(19u+std::uint32_t(sparks))),.95f);++sparks;}
 void smoke(){
  for(const auto*f:fixtures({model::scrap_burn_barrel},7)){if(count(ParticleKind::Fire)>=2||full())break;const Point3 top{f->position.x,f->position.y,ground()+f->base+f->height-.12f};
   if(!open(top)||!roomy({top.x,top.y,top.z+.8f},.3f))continue;kit::fire(m,{top.x,top.y},top.z,.2f,.45f,.85f);}
  for(const auto*f:fixtures({model::generator,model::mine_portable_generator,model::pwr_generator_unit,model::ind_generator},8)){if(count(ParticleKind::Smoke)>=1||full())break;
   const Point3 top{f->position.x,f->position.y,ground()+f->base+f->height+.06f};if(!open(top)||!roomy({top.x,top.y,top.z+.6f},.25f))continue;kit::smokeColumn(m,{top.x,top.y},top.z,1.6f,.45f,.1f);}}
 void drips(){
  for(const auto*f:fixtures({model::med_floor_drain},9)){if(count(ParticleKind::Drips)>=1||full())break;const float floor=ground()+f->base,roof=roofAt(f->position,floor+.3f);
   if(roof-floor>6.f||roof-floor<1.8f)continue;const Point3 at{f->position.x,f->position.y,roof-.04f};if(!open(at)||!apart(f->position,3.f))continue;kit::dripping(m,f->position,at.z,.5f+.4f*dice(21));}
  for(const auto*f:fixtures({model::med_scrub_sink},10)){if(count(ParticleKind::Drips)>=2||full())break;const Point3 at{f->position.x,f->position.y,ground()+f->base+f->height+.16f};
   if(!open(at)||!apart(f->position,3.f))continue;kit::dripping(m,f->position,at.z,.35f+.3f*dice(23));}
  int basins=0;for(const auto&v:m.waterVolumes){if(basins>=2||full())break;
   for(int k=0;k<6&&basins<2;++k){const Vec2 at{v.x1+(v.x2-v.x1)*(.2f+.6f*dice(30u+std::uint32_t(k))),v.y1+(v.y2-v.y1)*(.2f+.6f*dice(40u+std::uint32_t(k)))};const float roof=roofAt(at,v.surface+.2f);
    if(roof-v.surface>9.f||!open({at.x,at.y,roof-.04f})||!apart(at,3.5f))continue;kit::dripping(m,at,roof-.04f,.7f+.6f*dice(50u+std::uint32_t(k)));++basins;}}}
 void shafts(int most,float tall){
  // The strongest lamps of a tall room: dust turning in the light under them.
  std::vector<std::pair<float,const WorldLight*>> lamps;for(const auto&l:m.lights)if(l.mount==WorldLightMount::Ceiling&&l.intensity>=.85f&&!l.cargoLift)lamps.push_back({-l.intensity-.2f*dice(60u+std::uint32_t(&l-m.lights.data())),&l});
  std::sort(lamps.begin(),lamps.end(),[](const auto&a,const auto&b){return a.first<b.first;});int placed=0;
  for(const auto&[order,l]:lamps){if(placed>=most||full())break;(void)order;const float floor=floorAt(l->position,l->z-.4f),top=l->z-.35f;if(top-floor<tall||top-floor>14.f)continue;
   const float half=.55f;bool clear=true;for(float z:{floor+.5f,(floor+top)*.5f,top-.1f})clear&=roomy({l->position.x,l->position.y,z},half);if(!clear||!apart(l->position,3.f))continue;
   kit::dustMotes(m,l->position,floor+.35f,half,half,top-floor-.45f,int(std::clamp((top-floor)*18.f,40.f,64.f)),1.f);++placed;}}
 void grit(){for(int k=0;k<20&&count(ParticleKind::Debris)<1&&!full();++k){const Vec2 at{2.f+20.f*dice(70u+std::uint32_t(k)),2.f+20.f*dice(80u+std::uint32_t(k))};const float floor=floorAt(at,ground()+3.f),roof=roofAt(at,floor+.3f);
  if(roof-floor<2.2f||roof-floor>7.f||!open({at.x,at.y,roof-.04f})||!roomy({at.x,at.y,floor+1.f},.4f)||!apart(at,4.f)||length(at-m.definition.playerStart)<4.f)continue;kit::ceilingGrit(m,at,roof-.03f,6.f,.65f);}}
 // An old plume next to a steam hazard that pulses breathes with it.
 void timeOldPlumes(){for(size_t i=0;i<before;++i){auto&e=m.emitters[i];if(e.kind!=ParticleKind::Plume||e.period>0||e.enabledFlag)continue;
  for(const auto&h:m.hazards)if(h.kind==Hazard::Kind::Steam&&h.period>0&&e.position.x>h.x1-.8f&&e.position.x<h.x2+.8f&&e.position.y>h.y1-.8f&&e.position.y<h.y2+.8f){
   e.kind=ParticleKind::Steam;e.count=std::min(64,e.count*3);e.period=h.period;e.onTime=h.onTime+.4f;e.phase=h.phase;e.enabledFlag=h.enabledFlag;e.enabledValue=h.enabledValue;e.invertFlag=h.invertFlag;break;}}}
 // Anywhere at all: the brightest lamp's dust, however low the room.
 // Anywhere at all: the brightest lamp's dust, however low the room and however near a wall; failing
 // that a pipe's leak, failing that dust hanging over the open floor where the player comes in.
 void atLeastOne(){if(added()>0)return;shafts(1,2.2f);if(added()>0)return;pipeLeaks(1,99);if(added()>0)return;
  std::vector<std::pair<float,const WorldLight*>> lamps;for(const auto&l:m.lights)if(!l.cargoLift&&l.intensity>.2f)lamps.push_back({-l.intensity,&l});
  std::sort(lamps.begin(),lamps.end(),[](const auto&a,const auto&b){return a.first<b.first;});
  for(const auto&[order,l]:lamps){(void)order;const float floor=floorAt(l->position,l->z-.3f),top=l->z-.3f;if(top-floor<1.6f||top-floor>14.f)continue;
   bool clear=true;for(float z:{floor+.4f,(floor+top)*.5f})clear&=roomy({l->position.x,l->position.y,z},.3f);if(!clear)continue;
   kit::dustMotes(m,l->position,floor+.35f,.32f,.32f,top-floor-.45f,24,.6f);return;}
  const Vec2 start=m.definition.playerStart;for(int k=0;k<24;++k){const Vec2 at{start.x+4.f*(dice(90u+std::uint32_t(k))-.5f)*2,start.y+4.f*(dice(95u+std::uint32_t(k))-.5f)*2};
   const float floor=floorAt(at,ground()+3.f);if(!roomy({at.x,at.y,floor+.6f},.4f)||!roomy({at.x,at.y,floor+1.6f},.4f))continue;
   kit::dustMotes(m,at,floor+.35f,.45f,.45f,1.6f,24,.5f);return;}}
};
// The set pieces, placed by hand.
void heroes(Air& a){Map& m=a.m;
 switch(a.chunk){
  case 0:{ // The Foundry: heat off the converter vessels, dust in the sodium high-bays.
   kit::smokeColumn(m,{12.f,12.6f},3.f,2.4f,.38f,.4f);
   a.shafts(2,2.5f);break;}
  case 49:{ // The Sump: the pump bridge drips into the wet well, cold mist lies on it.
   for(float x:{4.5f,8.2f,11.6f})if(a.open({x,8.f,-25.3f}))kit::dripping(m,{x,8.f},-25.3f,.9f+.3f*a.dice(std::uint32_t(x*10)));
   for(Vec2 at:{Vec2{5.f,10.8f},Vec2{10.5f,10.8f}}){for(const auto&v:m.waterVolumes)if(at.x>v.x1&&at.x<v.x2&&at.y>v.y1&&at.y<v.y2){
     kit::airEmitter(m,ParticleKind::Steam,at,v.surface+.15f,{.35f,.2f,.45f},5.f,12,.25f,.9f,.16f);break;}}
   break;}
  case 50:case 51:{ // The cooling towers: vapour off the warm basin and the fill raining into it.
   int plumes=0,rain=0;for(const auto&v:m.waterVolumes){
    for(int k=0;k<10&&plumes<3;++k){const Vec2 at{v.x1+(v.x2-v.x1)*(.15f+.7f*a.dice(300u+std::uint32_t(k))),v.y1+(v.y2-v.y1)*(.15f+.7f*a.dice(310u+std::uint32_t(k)))};
     // Fewer, bigger, slower billows: the vapour reads as one body rising, at a third the cards.
     if(!a.roomy({at.x,at.y,v.surface+1.2f},.8f)||!a.apart(at,4.5f))continue;kit::airEmitter(m,ParticleKind::Steam,at,v.surface+.2f,{.3f,.15f,2.4f},6.f,9,.5f,2.f,.34f);++plumes;}
    for(int k=0;k<14&&rain<4;++k){const Vec2 at{v.x1+(v.x2-v.x1)*(.1f+.8f*a.dice(320u+std::uint32_t(k))),v.y1+(v.y2-v.y1)*(.1f+.8f*a.dice(330u+std::uint32_t(k)))};const float roof=a.roofAt(at,v.surface+.3f);
     if(roof-v.surface>14.f||!a.open({at.x,at.y,roof-.05f})||!a.apart(at,2.5f))continue;kit::dripping(m,at,roof-.05f,2.2f+a.dice(340u+std::uint32_t(k)));++rain;}}
   break;}
  case 86:{ // The High-Energy Test Bay: current in the air round the transformers, cables that spit.
   int glows=0;for(const auto*f:a.fixtures({model::pwr_step_transformer,model::pwr_cable_bundle,model::lab_knife_switch},90)){if(glows>=2)break;
    const Point3 c{f->position.x,f->position.y,a.ground()+f->base+std::min(1.5f,f->height*.7f)};Vec2 out;if(!a.outward(c,std::max(f->width,f->depth)*.5f+.05f,out))continue;
    float reach=.05f;while(reach<1.2f&&!a.open({c.x+out.x*reach,c.y+out.y*reach,c.z}))reach+=.03f;const Point3 at{c.x+out.x*(reach+.2f),c.y+out.y*(reach+.2f),c.z};
    if(!a.roomy(at,.3f)||!a.apart({at.x,at.y},2.5f))continue;kit::arcGlow(m,{at.x,at.y},at.z,.3f,.8f);++glows;}
   a.sparking(2);a.shafts(2,2.8f);break;}
  case 102:{ // The Breach Sector: smoke from what burned, cables spitting, the ceiling coming down.
   for(const auto*f:a.fixtures({model::pwr_generator_unit,model::pwr_turbine_housing,model::mine_portable_generator},91)){const Point3 top{f->position.x,f->position.y,a.ground()+f->base+f->height+.08f};
    if(!a.open(top)||!a.roomy({top.x,top.y,top.z+.8f},.3f))continue;kit::smokeColumn(m,f->position,top.z,2.6f,.55f,.3f);break;}
   a.sparking(2);a.grit();a.pipeLeaks(1,102);
   // Something from the containment lying low in a corner of the floor: green, heavy, slow.
   for(int k=0;k<40;++k){const Vec2 at{3.f+18.f*a.dice(400u+std::uint32_t(k)),3.f+18.f*a.dice(410u+std::uint32_t(k))};const float floor=a.floorAt(at,a.ground()+2.f);
    if(length(at-m.definition.playerStart)<6.f||!a.apart(at,4.f))continue;bool clear=true;for(Vec2 d:{Vec2{-1.2f,-1.f},{1.2f,-1.f},{-1.2f,1.f},{1.2f,1.f},{0,0}})clear&=a.roomy({at.x+d.x,at.y+d.y,floor+.35f},.25f);
    if(!clear)continue;kit::toxicPool(m,at,floor,1.3f,1.f,.45f,.7f);break;}
   break;}
  default:break;}}
void dress(Map& m,int chunk){
 const World w(chunk,std::make_shared<const Map>(m));Air a{m,chunk,w,m.emitters.size()};
 a.timeOldPlumes();heroes(a);
 const bool plant=chunk<=52||chunk==63||chunk==65;     // Power, the Machine Hall, Ventilation, Water: steam everywhere
 const bool deep=chunk>=104&&chunk<=109;
 const bool hall=(chunk>=41&&chunk<=44)||chunk==50||chunk==51||chunk==86||chunk==107;
 if(chunk>=ChapterOneAirFirst){
  a.pipeLeaks(plant?2:(m.pipes.size()>=40?1:0));
  a.vents(1);a.sparking(chunk==92?2:1);a.smoke();a.drips();a.shafts(hall?2:1,3.f);
  if(deep)a.grit();
  a.atLeastOne();}
 dressedCounts[size_t(chunk)]=a.added();
}
}
int dressedAirCount(int chunk){return chunk>=0&&chunk<CampaignChunkCount?dressedCounts[size_t(chunk)]:0;}
void dressCampaignAir(std::array<std::shared_ptr<const AuthoredMapData>,CampaignChunkCount>& maps){
 for(int chunk=0;chunk<int(maps.size())&&chunk<=ChapterTwoAirLast;++chunk){if(!maps[size_t(chunk)]||(chunk>0&&chunk<ChapterOneAirFirst))continue;
  auto dressed=std::make_shared<Map>(*maps[size_t(chunk)]);dress(*dressed,chunk);maps[size_t(chunk)]=std::move(dressed);}
}
}

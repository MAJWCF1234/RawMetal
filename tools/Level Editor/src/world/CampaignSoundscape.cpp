#include "CampaignSoundscape.h"
#include "ModelIds.h"
#include "../audio/SoundBank.h"
#include <algorithm>
#include <cmath>
// Chapter 2's sound. Every map of the research levels (chunks 68-109) is given
// AMBIENT emitters from the sound bank, the same records a custom map writes:
//
//  - what stands in the room makes its own noise: a sink or a floor drain
//    drips, a fridge or a vending machine hums, a fluorescent fitting buzzes, a
//    vent breathes, a mainframe crackles, a test chamber works. A few of each
//    to a map, never two of a kind close together;
//  - each district has its bed and its now-and-then sounds, set down on open
//    floor well apart: a ward that cries somewhere, a morgue that breathes,
//    thick glass struck in Containment, the deep levels settling;
//  - a PA horn speaks the plant's announcement voice, rarely;
//  - the big rooms get their own REVERB; everywhere else the game measures
//    the space the player stands in.
//
// It runs after the chapter files, so it never changes what they place, and a
// map's own AMBIENT records (none yet) would simply be added to.
namespace retro {
namespace {
using Map=AuthoredMapData;
std::uint32_t mix32(std::uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return v;}
struct Dresser {
 Map& m;int chunk;
 bool clear(Vec2 at,Sound sound,float apart)const{for(const auto&a:m.ambients)if(a.sound==sound&&length(a.position-at)<apart)return false;return true;}
 void loop(Vec2 at,Sound sound,float gain,float radius,float z=-999){m.ambients.push_back({sound,at,z,gain,radius,AmbientEmitter::Loop});}
 void sometimes(Vec2 at,Sound sound,float gain,float radius,float every,float spread){m.ambients.push_back({sound,at,-999,gain,radius,AmbientEmitter::Random,every,spread});}
 // Whatever stands in the room that makes this sound: up to `most`, no two within `apart`.
 void byFixtures(std::initializer_list<int> models,Sound sound,float gain,float radius,int most,float apart,float lift=1.f){
  int placed=0;for(const auto&f:m.fixtures){if(placed>=most)break;if(!f.visible||std::find(models.begin(),models.end(),f.model)==models.end()||!clear(f.position,sound,apart))continue;
   loop(f.position,sound,gain,radius,-999);(void)lift;++placed;}
 }
 // Open floor on the ground layer, in an order fixed by the chunk: every tile around it floor too.
 std::vector<Vec2> openFloor(unsigned salt)const{
  std::vector<std::pair<std::uint32_t,Vec2>> found;if(m.layers.empty())return {};const auto&rows=m.layers.front().rows;
  auto floor=[&](int x,int y){return x>=0&&y>=0&&x<24&&y<24&&rows[size_t(y)].size()==24&&rows[size_t(y)][size_t(x)]=='.';};
  for(int y=1;y<23;++y)for(int x=1;x<23;++x){bool open=true;for(int dy=-1;dy<=1&&open;++dy)for(int dx=-1;dx<=1;++dx)open&=floor(x+dx,y+dy);
   if(open)found.push_back({mix32(std::uint32_t(chunk)*0x9e3779b9u^std::uint32_t(y*24+x)*0x85ebca6bu^salt),{x+.5f,y+.5f}});}
  std::sort(found.begin(),found.end(),[](const auto&a,const auto&b){return a.first<b.first;});
  std::vector<Vec2> out;for(const auto&f:found)out.push_back(f.second);return out;
 }
 // The next open spot at least `apart` from every emitter so far and from the map's start.
 bool spot(unsigned salt,float apart,Vec2& at)const{
  for(const auto&p:openFloor(salt)){bool far=length(p-m.definition.playerStart)>=5.f;for(const auto&a:m.ambients)far&=length(a.position-p)>=apart;if(far){at=p;return true;}}
  return false;
 }
 void bed(unsigned salt,Sound sound,float gain,float radius){Vec2 at;if(spot(salt,9.f,at))loop(at,sound,gain,radius);}
 void now(unsigned salt,Sound sound,float gain,float radius,float every,float spread){Vec2 at;if(spot(salt,7.f,at))sometimes(at,sound,gain,radius,every,spread);}
 // The horns speak these lines in turn, far apart in time.
 void announce(std::initializer_list<Sound> lines){
  int horns=0;for(const auto&f:m.fixtures){if(f.model!=model::lab_pa_horn||!f.visible||horns>=2)continue;int k=0;
   for(Sound line:lines){m.ambients.push_back({line,f.position,-999,.5f,18,AmbientEmitter::Random,95.f+31.f*float(k)+17.f*float(horns),80.f});++k;}++horns;}
 }
};
void dress(Map& m,int chunk){
 Dresser d{m,chunk};
 // What stands in the room.
 d.byFixtures({model::med_scrub_sink,model::med_floor_drain},Sound::AmbWaterDrip,.6f,8,2,6);
 d.byFixtures({model::lab_sample_fridge,model::vending_machine,model::metro_vending,model::water_cooler},Sound::AmbAirConditioner,.38f,7,3,6);
 d.byFixtures({model::med_fluoro_light},Sound::AmbFluorescent,.34f,7,3,8);
 d.byFixtures({model::vent,model::pwr_vent_grille,model::extraction_fan,model::pwr_air_handler},Sound::AmbVent,.5f,9,2,9);
 d.byFixtures({model::pwr_duct_fan_unit},Sound::AmbFan,.5f,10,1,10);
 // A mainframe hums with its fans, low; it used to crackle static, and a lab full of it grated.
 d.byFixtures({model::lab_mainframe,model::lab_analyzer_cabinet},Sound::AmbFan,.2f,6,1,9);
 d.byFixtures({model::lab_status_display,model::lab_scope_monitor},Sound::AmbLightBuzz,.26f,5,2,6);
 d.byFixtures({model::lab_test_chamber},Sound::AmbMechanical,.45f,11,1,10);
 // The district's own air.
 if(chunk<=70){ // Research Front: offices over a plant; the building settles.
  d.now(1,Sound::OneHouseCreak,.35f,14,38,45);d.announce({Sound::PaUnauthorized,Sound::PaAccessDenied});
 }else if(chunk<=78){ // Medical
  d.now(1,Sound::OneMetalCreak,.3f,16,44,50);
  if(chunk==75||chunk==76)d.bed(2,Sound::AmbCrying,.3f,14);                      // Ward C: someone, somewhere
  if(chunk==77)d.bed(2,Sound::AmbMetalTapping,.32f,10);                           // Surgery: instruments that should be still
  if(chunk==78){d.bed(2,Sound::AmbDrone,.4f,18);d.now(3,Sound::OneMonsterBreath,.4f,9,48,55);m.reverbWet=.3f;m.reverbSeconds=1.8f;} // the Morgue: tile and steel
  d.announce({Sound::PaCautionHazard,Sound::PaUnauthorized});
 }else if(chunk<=84){ // Analysis Laboratories
  d.bed(1,Sound::AmbMechanical,.34f,12);d.now(2,Sound::OneMetalMoan,.3f,16,52,60);d.announce({Sound::PaVentilationOffline,Sound::PaAccessDenied});
 }else if(chunk<=93){ // Energy Test Complex
  d.bed(1,Sound::AmbMechanical,.36f,12);d.now(2,Sound::OneMetalBang,.38f,20,46,60);
  if(chunk==86){d.bed(3,Sound::AmbDrone,.39f,20);m.reverbWet=.32f;m.reverbSeconds=2.2f;}                                          // the High-Energy Test Bay
  if(chunk>=87&&chunk<=90){m.reverbWet=.26f;m.reverbSeconds=1.6f;}                                                               // the Observation Ring
  d.announce({Sound::PaBackupGenerators,Sound::PaCautionHazard});
 }else if(chunk<=103){ // Containment
  d.bed(1,Sound::AmbHaunting,.38f,16);d.now(2,Sound::OneGlassBang,.48f,14,32,45);d.now(3,Sound::OneMonsterGroan,.42f,18,58,70);
  if(chunk>=100)d.now(4,Sound::OneGlassBang,.44f,14,41,50);
  if(chunk==102){d.now(5,Sound::OneMonsterGrowl,.45f,18,50,60);d.bed(6,Sound::AmbGasLeak,.3f,6);}                                  // the Breach Sector
  if(chunk==103)d.bed(6,Sound::AmbGasLeak,.26f,6);
  d.announce({Sound::PaContainmentBreach,Sound::PaEvacuate,Sound::PaSystemLockdown});
 }else{ // Deep Research: under everything
  d.bed(1,Sound::AmbDrone,.42f,18);d.now(2,Sound::OneDebris,.4f,12,42,50);d.now(3,Sound::OneDistantYell,.34f,24,85,90);
  if(chunk>=108)d.now(4,Sound::OneDistantYell2,.34f,24,110,90);
  if(chunk==107){m.reverbWet=.3f;m.reverbSeconds=2.f;}                                                                           // Energy Core Research
  d.announce({Sound::PaHostileDetected,Sound::PaSystemLockdown});
 }
}
}
void dressChapterTwoSound(std::array<std::shared_ptr<const AuthoredMapData>,CampaignChunkCount>& maps){
 for(int chunk=ChapterTwoSoundFirst;chunk<=ChapterTwoSoundLast&&chunk<int(maps.size());++chunk){if(!maps[size_t(chunk)])continue;
  auto dressed=std::make_shared<Map>(*maps[size_t(chunk)]);dress(*dressed,chunk);maps[size_t(chunk)]=std::move(dressed);}
}
}

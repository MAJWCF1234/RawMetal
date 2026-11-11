#include "Game.h"
#include "../audio/SoundBank.h"
#include "../world/CustomCampaign.h"
#include "../world/CampaignExport.h"
#include "../world/CampaignMaps.h"
#include "../world/CampaignSoundscape.h"
#include <cmath>
#include <fstream>
// Authored sound and the room's acoustics.
//
// A map places sounds from the bank (AMBIENT records, World::ambients): a loop
// is a bed the audio engine keeps running while its emitter's chunk is resident
// (AudioEngine::update); the other two modes are the game's, here. "Now and
// then" plays the sound at an uneven spacing, every..every+spread seconds, at a
// slightly different pitch each time; "on its state" plays it once each time
// the state takes its value (an announcement over a door, a stinger on a
// scripted beat). None of it is heard by the AI: it is the building, not a noise
// the player made. Timers are not saved; a load starts them afresh.
//
// The room's acoustics are what the audio engine's reverb runs on: the map's
// own REVERB record, or by default a measure of the space the player stands in
// (eight rays out to twelve metres and the ceiling over them), so a closet, a
// ward and a test bay each answer differently with no authoring at all.
namespace retro {
namespace {
std::uint64_t ambientKey(int level,size_t index){return (std::uint64_t(std::uint32_t(level))<<32)|std::uint64_t(index);}
std::uint32_t scramble32(std::uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return v;}
float unit(std::uint32_t seed){return float(scramble32(seed)&0xffffu)/65535.f;}
}
void Game::updateAmbience(float dt){
 (void)dt;
 for(int level=0;level<chunkCount();++level){if(!chunkResident(level))continue;const auto&world=level==m_level?m_world:m_chunks[size_t(level)].world;
  const auto&list=world.ambients();const Vec2 shift=chunkOffset(level)-chunkOffset(m_level);
  for(size_t i=0;i<list.size();++i){const auto&a=list[i];if(a.mode==AmbientEmitter::Loop||a.sound>=Sound::Count)continue;
   const auto key=ambientKey(level,i);const float feet=world.floorHeight(a.position.x,a.position.y),z=a.z<=-900?feet+1.5f:a.z;
   auto emit=[&](float pitch){SoundEvent e{a.sound,a.position+shift,a.gain,pitch,true,a.radius,z};e.aiRadius=-1;m_sounds.push_back(e);};
   if(a.mode==AmbientEmitter::OnState){
    // Once each time the state takes its value; never for a state that already has it on arrival.
    const bool on=a.state&&state(a.state)==a.value;auto found=m_ambientLatch.find(key);
    if(found==m_ambientLatch.end()){m_ambientLatch[key]=on;continue;}
    if(on&&!found->second)emit(1.f);found->second=on;continue;
   }
   if(a.state&&state(a.state)!=a.value){m_ambientNext.erase(key);continue;}
   auto next=m_ambientNext.find(key);const std::uint32_t seed=std::uint32_t(level)*0x9e3779b9u^std::uint32_t(i)*0x85ebca6bu;
   // The first one comes somewhere inside its first interval, so a room's sounds never start together.
   if(next==m_ambientNext.end()){m_ambientNext[key]={m_elapsed+(a.every+a.spread)*unit(seed)+.5f,0};continue;}
   if(m_elapsed<next->second.at)continue;
   const std::uint32_t turn=seed+std::uint32_t(++next->second.count)*0xc2b2ae35u;
   emit(.94f+.12f*unit(turn^0x1234567u));next->second.at=m_elapsed+std::max(.5f,a.every+a.spread*unit(turn));
  }
 }
}
// What the reverb should sound like where the player stands. Measured four
// times a second; the audio engine eases toward it.
Game::Acoustics Game::acoustics()const{
 if(m_elapsed-m_acoustics.at<.25f&&m_acoustics.at>=0&&m_acoustics.level==m_level)return m_acoustics.value;
 Acoustics out;const float eye=m_player.z+m_player.eye;
 if(hiddenLocker()>=0)out={.22f,.22f};       // a steel box
 else if(m_world.reverbWet()>=0)out={m_world.reverbWet(),m_world.reverbSeconds()>0?m_world.reverbSeconds():1.f};
 else if(m_world.outdoors()||m_world.zoneSky())out={.05f,.5f};
 else{
  float reach=0;
  for(int k=0;k<8;++k){const Vec2 d{std::cos(k*kPi*.25f),std::sin(k*kPi*.25f)};float lo=0,hi=12;
   for(int step=0;step<5;++step){const float mid=(lo+hi)*.5f;if(soundRayClear(m_player.pos,eye,m_player.pos+d*mid,eye))lo=mid;else hi=mid;}
   reach+=lo;}
  reach*=.125f;
  const float floor=m_world.floorHeight(m_player.pos.x,m_player.pos.y),height=std::clamp(m_world.clearanceHeight(m_player.pos.x,m_player.pos.y,m_player.z)-floor,2.f,14.f);
  const float size=std::cbrt(kPi*reach*reach*height); // metres, the cube root of the room's volume
  out={std::clamp(.08f+.022f*size,.08f,.34f),std::clamp(.3f+.13f*size,.35f,2.4f)};
 }
 m_acoustics={out,m_elapsed,m_level};return out;
}
bool Game::testAmbience(){
 std::ofstream report("ambience-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const std::string& text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 auto text=[](const std::string& records,int east=22){
  std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Ambience validation|0\nMAP|0|Sounds|0|0|3.5|12|0|0|4.5|0.7|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int y=0;y<24;++y)t+="ROW|0|0|"+std::to_string(y)+"|"+(y<8||y>16?std::string(24,'#'):"#"+std::string(size_t(east),'.')+std::string(size_t(23-east),'#'))+'\n';
  return t+"LIGHT|0|6|12|3.5|0|0|2|15|4000\n"+records+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";};
 const float frame=1.f/60;
 // 1. The record reads, with its defaults, and writes back out the same.
 {auto campaign=loadCustomCampaignText(text("AMBIENT|0|water_drip|6|12|1.2|0.5|7\nAMBIENT|0|metal_creak|10|12|-999|0.6|16|1|4|3\nAMBIENT|0|pa_doors_locked|12|12|2.4|0.7|16|2|||amb_test_door|1\nREVERB|0|0.3|1.6\n"),"ambience.rmc");
  const auto&a=campaign->maps[0]->ambients;
  check(a.size()==3&&a[0].sound==Sound::AmbWaterDrip&&a[0].mode==AmbientEmitter::Loop&&a[1].sound==Sound::OneMetalCreak&&a[1].mode==AmbientEmitter::Random&&a[1].every==4&&a[1].spread==3&&a[1].z<=-900&&
        a[2].mode==AmbientEmitter::OnState&&a[2].state==stateId("amb_test_door")&&a[2].value==1&&campaign->maps[0]->reverbWet==.3f&&campaign->maps[0]->reverbSeconds==1.6f,
   "AMBIENT (loop, now and then, on its state) and REVERB read from a map file");
  const auto exported=exportRmc("Ambience validation",{{campaign->maps[0],-1}},0,{});auto again=loadCustomCampaignText(exported.text,"ambience-again.rmc");const auto&b=again->maps[0]->ambients;
  bool same=b.size()==a.size()&&again->maps[0]->reverbWet==.3f&&again->maps[0]->reverbSeconds==1.6f;
  for(size_t i=0;same&&i<a.size();++i)same=a[i].sound==b[i].sound&&a[i].position.x==b[i].position.x&&a[i].z==b[i].z&&a[i].gain==b[i].gain&&a[i].radius==b[i].radius&&a[i].mode==b[i].mode&&a[i].every==b[i].every&&a[i].spread==b[i].spread&&a[i].state==b[i].state&&a[i].value==b[i].value;
  check(same&&exported.text.find("AMBIENT|0|water_drip|")!=std::string::npos,"and export to the same records");
  auto refuses=[&](const std::string& r){try{loadCustomCampaignText(text(r),"bad.rmc");return false;}catch(const std::exception&){return true;}};
  check(refuses("AMBIENT|0|no_such_sound|6|12|1|1|8\n")&&refuses("AMBIENT|0|water_drip|6|12|1|1|0\n")&&refuses("AMBIENT|0|water_drip|6|12|1|1|8|7\n")&&refuses("AMBIENT|0|metal_creak|6|12|1|1|8|2\n")&&refuses("REVERB|0|2|1\n"),
   "an unknown sound, a zero radius, an unknown mode, a state-less state emitter and a wild reverb are rejected");
  // 2. Now and then: within its spacing, at varied pitch, never for the AI; a loop is the audio engine's.
  Game g(campaign);int plays=0;float first=-1,last=-1,least=1e9f,most=0;bool varied=false,deaf=true,loopEvent=false;float pitch0=-1;
  for(int i=0;i<60*60;++i){g.update({},frame);for(const auto&e:g.sounds()){if(e.sound==Sound::AmbWaterDrip)loopEvent=true;if(e.sound!=Sound::OneMetalCreak)continue;
    const float now=g.elapsed();if(first<0)first=now;else{least=std::min(least,now-last);most=std::max(most,now-last);}last=now;++plays;deaf&=e.aiRadius<0&&e.spatial&&e.radius==16;
    if(pitch0<0)pitch0=e.pitch;else varied|=std::fabs(e.pitch-pitch0)>.005f;}}
  report<<"  "<<plays<<" creaks in 60 s, first at "<<first<<" s, spacing "<<least<<" to "<<most<<" s\n";
  check(plays>=8&&plays<=15&&first<=7.6f&&least>=3.99f&&most<=7.05f&&varied&&deaf&&!loopEvent,"A now-and-then emitter plays every 4 to 7 s at a varied pitch, unheard by the AI ("+std::to_string(plays)+" in a minute)");
  // 3. On its state: once when the state is set, again only after it clears.
  auto count=[&](int frames){int n=0;for(int i=0;i<frames;++i){g.update({},frame);for(const auto&e:g.sounds())n+=e.sound==Sound::PaDoorsLocked;}return n;};
  const int before=count(60);g.setState(stateId("amb_test_door"),1);const int set=count(120);g.setState(stateId("amb_test_door"),0);const int cleared=count(30);g.setState(stateId("amb_test_door"),1);const int again2=count(30);
  check(before==0&&set==1&&cleared==0&&again2==1,"A state emitter speaks once each time its state is set");
  // 4. The map's own reverb is what the engine is told.
  const auto told=g.acoustics();check(told.wet==.3f&&told.seconds==1.6f,"A REVERB record sets the room's acoustics");}
 // 5. With none, the room is measured: a hall rings longer than a closet.
 {Game hall(loadCustomCampaignText(text(""),"hall.rmc"));hall.update({},frame);const auto big=hall.acoustics();
  Game closet(loadCustomCampaignText(text("",5),"closet.rmc"));closet.update({},frame);const auto small=closet.acoustics();
  report<<"  hall wet "<<big.wet<<" for "<<big.seconds<<" s; closet wet "<<small.wet<<" for "<<small.seconds<<" s\n";
  check(big.seconds>small.seconds+.15f&&big.wet>small.wet&&small.seconds>=.35f&&big.seconds<=2.4f,"Unauthored, the space is measured: the hall answers longer and wetter than the closet");}
 // 6. Chapter 2 has its sound: every research-level map carries emitters from the bank, inside its walls, and the big rooms their reverb.
 {int least=1000,most=0,total=0,loops=0,outside=0,quiet=0;
  for(int chunk=ChapterTwoSoundFirst;chunk<=ChapterTwoSoundLast;++chunk){const auto map=campaignMap(chunk);const int n=int(map->ambients.size());least=std::min(least,n);most=std::max(most,n);total+=n;quiet+=n==0;
   for(const auto&a:map->ambients){loops+=a.mode==AmbientEmitter::Loop;const bool bad=a.position.x<0||a.position.y<0||a.position.x>24||a.position.y>24||!bankSound(a.sound)||a.gain>.6f||(a.mode==AmbientEmitter::Loop&&!bankSound(a.sound)->loop);outside+=bad;
    if(bad)report<<"  out of bounds: chunk "<<chunk<<" sound "<<int(a.sound)<<" at "<<a.position.x<<","<<a.position.y<<" gain "<<a.gain<<" mode "<<int(a.mode)<<(bankSound(a.sound)?"":" (not in bank)")<<'\n';}}
  const auto morgue=campaignMap(78),bay=campaignMap(86),foundry=campaignMap(0);
  bool breath=false,drone=false;for(const auto&a:morgue->ambients){breath|=a.sound==Sound::OneMonsterBreath&&a.mode==AmbientEmitter::Random;drone|=a.sound==Sound::AmbDrone;}
  report<<"  chunks 68-109: "<<total<<" emitters ("<<loops<<" beds), "<<least<<" to "<<most<<" a map\n";
  check(quiet==0&&most<=20&&outside==0&&loops>40&&total-loops>80,"Every Chapter 2 map has its sound ("+std::to_string(total)+" emitters, "+std::to_string(least)+" to "+std::to_string(most)+" a map), all inside the map and from the bank");
  check(breath&&drone&&morgue->reverbWet==.3f&&bay->reverbSeconds==2.2f&&foundry->ambients.empty()&&foundry->reverbWet<0,"The Morgue breathes over a drone and rings like tile; the test bay rings longer; Chapter 1 is untouched");
  // and the campaign map plays them
  Game g;g.startMainCampaign(78);g.m_god=true;int breaths=0;for(int i=0;i<60*150;++i){g.update({},1.f/60);for(const auto&e:g.sounds())breaths+=e.sound==Sound::OneMonsterBreath;}
  check(breaths>=1&&breaths<=4&&g.acoustics().wet==.3f,"In the Morgue the breath comes once or twice in two and a half minutes");}
 report<<(failed?"FAILED ":"PASSED ")<<checks-failed<<'/'<<checks<<'\n';return failed==0;
}
}

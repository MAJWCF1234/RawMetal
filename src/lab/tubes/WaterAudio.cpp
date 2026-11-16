// A short check of the new sound through the real headless mixer, independent of campaign sweeps.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "game/EngineRegressionScene.h"
#include <cmath>
#include <fstream>

namespace retro::lab {
namespace {
struct Window {double rms=0;int peak=0;};
Window measure(const std::vector<int16_t>& pcm,float first,float last,const std::vector<int16_t>* baseline=nullptr){Window out;double energy=0;size_t count=0;
 const size_t begin=size_t(first*44100)*2,end=std::min(pcm.size(),size_t(last*44100)*2);
 for(size_t i=begin;i<end;++i){const int value=int(pcm[i])-(baseline&&i<baseline->size()?int((*baseline)[i]):0);energy+=double(value)*value;out.peak=std::max(out.peak,std::abs(value));++count;}
 out.rms=count?std::sqrt(energy/count):0;return out;}
Game firedWater(float distance=3){auto room=regression::room();room->waterVolumes.push_back({10,10,14,14,-2,1});Game game(regression::campaign(room));
 LabAccess::setGod(game,true);LabAccess::equip(game,WeaponId::Shotgun);LabAccess::setCamera(game,{12-distance,12},0,0,0);LabAccess::playerCamera(game);
 for(int i=0;i<60;++i)game.update({},1.f/60);auto&p=LabAccess::player(game);p.pitch=std::atan2(1.f-p.z-p.eye,distance)*140.f;
 InputState fire{};fire.fire=true;game.update(fire,1.f/60);return game;}
struct Mixed {std::vector<int16_t> pcm;bool waterVoice=false,pump=false;float waterLevel=0;};
Mixed listen(Game& game,float distance,bool splashOnly){auto&p=LabAccess::player(game);p.pos={12-distance,12};p.z=0;p.angle=0;
 AudioEngine audio(false);audio.update(game,true);Mixed out;
 for(const auto& voice:LabAccess::held(audio))if(voice.sound==Sound::WaterSplash){out.waterVoice|=voice.spatial&&!voice.loop&&voice.level>0;out.waterLevel=std::max(out.waterLevel,voice.level);}
 if(splashOnly)LabAccess::onlySound(audio,Sound::WaterSplash);else LabAccess::onlyBus(audio,0);
 LabAccess::mix(audio,out.pcm,22050);return out;}
Mixed listenInPlay(Game& game,bool omitWater){AudioEngine audio(false);Mixed out;
 // Interleave the real game and mixer at 60 Hz, so the .12 s shotgun pump can mask the wet tail.
 for(int frame=0;frame<30;++frame){if(frame)game.update({},1.f/60);if(omitWater)LabAccess::omitSound(game,Sound::WaterSplash);
  for(const auto& event:game.sounds())out.pump|=event.sound==Sound::Pump;
  audio.update(game,true);LabAccess::onlyBus(audio,0);
  for(const auto& voice:LabAccess::held(audio))if(voice.sound==Sound::WaterSplash){out.waterVoice|=voice.spatial&&!voice.loop&&voice.level>0;out.waterLevel=std::max(out.waterLevel,voice.level);}
  LabAccess::mix(audio,out.pcm,735);
 }return out;}
const bool waterAudio=add({.name="water-audio",.summary="real water strikes remain audible at ten metres and leave a wet cue beneath the shotgun tail",.standalone=+[]{
 std::ofstream report("water-audio-test.txt");bool passed=true;
 { // Keep the original simple sample/mixer smoke check, separate from authored-shot audibility.
 Game game(regression::campaign(regression::room()));
 auto& player=LabAccess::player(game);player.pos={8,10};player.z=0;
 LabAccess::raise(game,{Sound::WaterSplash,{10,10},1,1,true,17,1});
 AudioEngine audio(false);audio.update(game,true);LabAccess::onlyBus(audio,0);
 bool started=false;for(const auto& voice:LabAccess::held(audio))started|=voice.sound==Sound::WaterSplash&&!voice.loop&&voice.spatial&&voice.level>0;
 std::vector<int16_t> pcm;LabAccess::mix(audio,pcm,44100);
 double early=0,late=0;int peak=0;
 for(size_t i=0;i<pcm.size();++i){const double value=pcm[i];peak=std::max(peak,std::abs(int(pcm[i])));if(i<17640)early+=value*value;if(i>=66150)late+=value*value;}
 const bool audible=early>17640*20.*20.&&peak>200;
 const bool faded=late<early*.1;
 report<<(started?"PASS":"FAIL")<<" spatial water-impact voice starts\n"
       <<(audible?"PASS":"FAIL")<<" real mixed splash is audible; peak "<<peak<<", early energy "<<early<<"\n"
       <<(faded?"PASS":"FAIL")<<" splash fades; late energy "<<late<<"\n";
 passed&=started&&audible&&faded;
 }
 // A real player shot supplies the event's current gain, radius and pitch. No fabricated loud event.
 {auto game=firedWater(10);bool gunshot=false;for(const auto& event:game.sounds())gunshot|=gunshotSound(event.sound);
  const auto mixed=listen(game,10,true);const auto audible=measure(mixed.pcm,.08f,.35f);const bool heard=mixed.waterVoice&&audible.rms>20&&audible.peak>100;
  report<<(gunshot&&game.waterSplashes().size()==1?"PASS":"FAIL")<<" real shotgun shot strikes the authored pool\n"
        <<(heard?"PASS":"FAIL")<<" water strike remains audible to a listener ten metres from its surface; voice "<<mixed.waterVoice<<", level "<<mixed.waterLevel<<", 80-350ms RMS "<<audible.rms<<", peak "<<audible.peak<<'\n';
  passed&=gunshot&&game.waterSplashes().size()==1&&heard;}
 {auto game=firedWater();const auto mixed=listen(game,3,true);const auto attack=measure(mixed.pcm,0,.12f),tail=measure(mixed.pcm,.12f,.35f);
  const bool wetTail=tail.rms>40&&tail.rms>=attack.rms*.25;
  report<<(wetTail?"PASS":"FAIL")<<" nearby splash keeps a wet tail after the opening impact; first120ms RMS "<<attack.rms<<", 120-350ms RMS "<<tail.rms<<", peak "<<tail.peak<<'\n';passed&=wetTail;}
 {auto complete=firedWater(),shotOnly=firedWater();
  const auto full=listenInPlay(complete,false),baseline=listenInPlay(shotOnly,true);const auto gunTail=measure(baseline.pcm,.15f,.35f),waterCue=measure(full.pcm,.15f,.35f,&baseline.pcm);
  const bool distinguishable=full.pump&&baseline.pump&&waterCue.rms>20&&waterCue.rms>=gunTail.rms*.2;
  report<<(distinguishable?"PASS":"FAIL")<<" wet cue emerges beneath the real shotgun and pump; pump heard "<<full.pump<<'/'<<baseline.pump<<", shot-only150-350ms RMS "<<gunTail.rms<<", added-water RMS "<<waterCue.rms<<", peak difference "<<waterCue.peak<<'\n';passed&=distinguishable;}
 return passed;
}});
}
}

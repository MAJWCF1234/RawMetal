#pragma once
#include "Sound.h"
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
namespace retro {
class Game;
class AudioEngine {
public:
 explicit AudioEngine(bool device=true);
 ~AudioEngine();
 AudioEngine(const AudioEngine&)=delete;
 AudioEngine& operator=(const AudioEngine&)=delete;
 void update(const Game& game, bool focused=true);
 bool available()const{return m_device!=nullptr;}
 static bool test();
 static bool testDevice();
 static bool testLiftMix();
private:
 struct Sample {std::vector<int16_t> pcm; int channels=1; size_t frames()const{return pcm.size()/channels;}};
 // NPC speech is coloured live by what the speaker wears over the mouth,
 // read from the actor's model: nothing, a face plate, a respirator, or a
 // sealed hazmat hood. Biquads (RBJ) plus a short dull reflection off the visor.
 struct Biquad {float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;float run(float x){float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}};
 struct MaskFilter {int kind=-1;std::array<Biquad,4> stages{};Biquad echoTone{};std::array<float,64> echo{};int cursor=0,delay=40;float echoGain=0,makeup=1;
  float run(float x){for(auto&stage:stages)x=stage.run(x);float reflected=echoTone.run(echo[size_t((cursor+64-delay)%64)]);echo[size_t(cursor)]=x;cursor=(cursor+1)%64;return (x+reflected*echoGain)*makeup;}};
 static void configureMask(MaskFilter& mask,int kind);
 static Biquad biquad(int type,float freq,float q,float gainDb=0);
 struct Voice {Sound sound; double cursor=0; float gain=1,pitch=1,left=1,right=1; Vec2 position{}; bool spatial=false,loop=false; std::int64_t emitter=0;float smoothLeft=0,smoothRight=0,occlusion=1,radius=17,z=std::numeric_limits<float>::quiet_NaN();int speech=-1;size_t speaker=0;int speechPriority=0;float filter=1,filterTarget=1,filtered=0;MaskFilter mask{};};
 static constexpr int Rate=44100, Block=512;
 std::array<Sample,size_t(Sound::Count)> m_samples;
 std::vector<Sample> m_speech;
 const Sample& sampleFor(const Voice& voice)const{return voice.speech>=0?m_speech.at(size_t(voice.speech)):m_samples[size_t(voice.sound)];}
 std::vector<Voice> m_voices;
 HWAVEOUT m_device=nullptr;
 HANDLE m_wake=nullptr;
 struct Buffer {WAVEHDR header{}; std::array<int16_t,Block*2> pcm{};};
 std::array<Buffer,4> m_buffers;
 std::thread m_thread;
 std::mutex m_mutex;
 std::atomic<bool> m_stop=false;
 float m_master=1,m_targetMaster=1,m_musicGain=.28f,m_lastTime=-1;
 float m_effectsGain=1;
 float m_mainBlend=1,m_reactorBlend=0,m_motorBlend=0;
 float m_mainTarget=1,m_reactorTarget=0,m_motorTarget=0;
 // Area score, one entry per MusicCue (0, the original track, is m_main*).
 static constexpr int AreaScores=9;
 std::array<float,AreaScores> m_areaBlend{},m_areaTarget{};
 float m_submerged=0,m_submergedTarget=0,m_underwaterLeft=0,m_underwaterRight=0;
 bool m_paused=false;
  int m_lastChunk=-1;
  int m_lastWorld=-1;
  unsigned m_lastRevision=0;
 void play(const SoundEvent& event,std::int64_t emitter=0,bool loop=false);
 void mix(int16_t* output,size_t frames);
 void run();
 void spatialize(Voice& voice,const Game& game,float dt=1.f/60.f);
};
}

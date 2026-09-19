#pragma once
#include "Sound.h"
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
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
 struct Voice {Sound sound; double cursor=0; float gain=1,pitch=1,left=1,right=1; Vec2 position{}; bool spatial=false,loop=false; int emitter=0;float smoothLeft=0,smoothRight=0,occlusion=1;};
 static constexpr int Rate=44100, Block=512;
 std::array<Sample,size_t(Sound::Count)> m_samples;
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
 bool m_paused=false;
 int m_lastChunk=-1;
 void play(const SoundEvent& event,int emitter=0,bool loop=false);
 void mix(int16_t* output,size_t frames);
 void run();
 void spatialize(Voice& voice,const Game& game,float dt=1.f/60.f);
};
}

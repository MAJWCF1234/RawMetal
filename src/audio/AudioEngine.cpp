#include "AudioEngine.h"
#include "../core/PackedResource.h"
#include "../game/Game.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <chrono>
namespace retro {
std::vector<int16_t> decodeMusic(const unsigned char* data,size_t bytes);
AudioEngine::AudioEngine(bool device){
 for(size_t i=0;i<m_samples.size();++i){
  auto resource=loadResource(int(200+i));auto data=resource.data();size_t size=resource.size();
  auto u16=[](const unsigned char*p){return unsigned(p[0])|(unsigned(p[1])<<8);};
  auto u32=[](const unsigned char*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
  if(size<12||std::memcmp(data,"RIFF",4)||std::memcmp(data+8,"WAVE",4)){
   m_samples[i].pcm=decodeMusic(data,size);m_samples[i].channels=2;continue;
  }
  const unsigned char* pcm=nullptr;size_t bytes=0;bool format=false;
  for(size_t offset=12;offset+8<=size;){size_t count=u32(data+offset+4);auto chunk=data+offset+8;
   if(count>size-offset-8)throw std::runtime_error("Truncated audio resource");
   if(!std::memcmp(data+offset,"fmt ",4)&&count>=16){auto channels=u16(chunk+2);format=u16(chunk)==1&&(channels==1||channels==2)&&u32(chunk+4)==Rate&&u16(chunk+14)==16;m_samples[i].channels=int(channels);}
   if(!std::memcmp(data+offset,"data",4)){pcm=chunk;bytes=count;}
   offset+=8+count+(count&1);
  }
  if(!format||!pcm||bytes<4||bytes%(2*m_samples[i].channels))throw std::runtime_error("Audio must be PCM16 at 44100 Hz");
  auto&sample=m_samples[i];sample.pcm.resize(bytes/2);std::memcpy(sample.pcm.data(),pcm,bytes);
  // A short boundary taper suppresses clicks when ambience wraps.
  if(i==size_t(Sound::Music)||i==size_t(Sound::Machine)||i==size_t(Sound::Wings)){
   size_t fade=std::min(size_t(220),sample.frames()/2);
   for(size_t f=0;f<fade;++f)for(int c=0;c<sample.channels;++c){sample.pcm[f*sample.channels+c]=int16_t(sample.pcm[f*sample.channels+c]*float(f)/float(fade));auto end=(sample.frames()-1-f)*sample.channels+c;sample.pcm[end]=int16_t(sample.pcm[end]*float(f)/float(fade));}
  }
 }
 play({Sound::Music,{},1,1,false},-1,true);
 if(!device)return;
 m_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=Rate;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=Rate*4;
 if(!m_wake||waveOutOpen(&m_device,WAVE_MAPPER,&format,reinterpret_cast<DWORD_PTR>(m_wake),0,CALLBACK_EVENT)!=MMSYSERR_NOERROR){m_device=nullptr;return;}
 for(auto&buffer:m_buffers){buffer.header.lpData=reinterpret_cast<LPSTR>(buffer.pcm.data());buffer.header.dwBufferLength=DWORD(buffer.pcm.size()*2);
  if(waveOutPrepareHeader(m_device,&buffer.header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR){waveOutReset(m_device);for(auto&b:m_buffers)if(b.header.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(m_device,&b.header,sizeof(WAVEHDR));waveOutClose(m_device);m_device=nullptr;return;}
 }
 m_thread=std::thread([this]{run();});
}
AudioEngine::~AudioEngine(){
 m_stop=true;if(m_wake)SetEvent(m_wake);if(m_thread.joinable())m_thread.join();
 if(m_device){waveOutReset(m_device);for(auto&buffer:m_buffers)waveOutUnprepareHeader(m_device,&buffer.header,sizeof(WAVEHDR));waveOutClose(m_device);}
 if(m_wake)CloseHandle(m_wake);
}
void AudioEngine::play(const SoundEvent& event,int emitter,bool loop){
 if(m_voices.size()>=48){auto voice=std::find_if(m_voices.begin(),m_voices.end(),[](auto&v){return !v.loop;});if(voice!=m_voices.end())m_voices.erase(voice);else return;}
 m_voices.push_back({event.sound,0,event.gain,event.pitch,1,1,event.position,event.spatial,loop,emitter});
 if(loop&&event.sound==Sound::Machine)m_voices.back().cursor=double((unsigned(-emitter)*7919u)%m_samples[size_t(event.sound)].frames());
}
void AudioEngine::spatialize(Voice& voice,const Game& game,float dt){
 if(!voice.spatial){voice.left=voice.right=1;return;}
 auto delta=voice.position-game.player().pos;float distance=length(delta);
 float gain=std::pow(std::max(0.f,1-distance/17.f),2.f);
 float pan=distance>.01f?dot(delta*(1/distance),Vec2{-std::sin(game.player().angle),std::cos(game.player().angle)}):0;
 pan=std::clamp(pan,-1.f,1.f); // Roundoff at full pan must never reach sqrt of a negative number.
 // Three rays and a slower occlusion envelope prevent abrupt volume pumping at corners.
 Vec2 side=distance>.001f?Vec2{-delta.y/distance,delta.x/distance}:Vec2{};float blocked=0;
 for(float offset:{-.18f,0.f,.18f})for(float t=.25f;t<distance-.25f;t+=.2f){auto p=game.player().pos+delta*(t/distance)+side*offset;
  if(voice.sound==Sound::Machine&&length(p-voice.position)<1.7f)continue;
  float z=game.player().z+game.player().eye;
  const auto&world=game.worldAt(p);if(world.supportHeight(p.x,p.y)>z||world.doorBlocks(p.x,p.y,z,.01f)){blocked+=1;break;}
 }
 float target=1-blocked*(.68f/3.f);voice.occlusion+=(target-voice.occlusion)*(1-std::exp(-dt*7.f));gain*=voice.occlusion;
 voice.left=gain*std::sqrt((1-pan)*.5f);voice.right=gain*std::sqrt((1+pan)*.5f);
}
void AudioEngine::update(const Game& game,bool focused){
 std::lock_guard lock(m_mutex);
 float dt=std::clamp(game.elapsed()-m_lastTime,0.f,.1f);
 if(game.elapsed()<m_lastTime){m_voices.clear();play({Sound::Music,{},1,1,false},-1,true);m_mainBlend=1;m_reactorBlend=m_motorBlend=0;}
 if(m_lastChunk>=0&&m_lastChunk!=game.level()){auto shift=Game::chunkOffset(m_lastChunk)-Game::chunkOffset(game.level());for(auto&voice:m_voices)if(voice.spatial)voice.position+=shift;}m_lastChunk=game.level();
 m_lastTime=game.elapsed();m_targetMaster=focused&&!game.audioMuted()?game.settings().master:0.f;
 m_musicGain=game.musicEnabled()?(game.dead()||game.won()?.16f:.28f)*game.settings().music/.75f:0;
 m_effectsGain=game.settings().effects;m_paused=game.paused()||game.consoleOpen();
 auto phase=game.world().liftPhase();bool shaft=game.level()==3;
 m_mainTarget=!shaft||phase==World::LiftPhase::Ready?1.f:0.f;
 m_reactorTarget=shaft&&phase==World::LiftPhase::Crashed?1.f:0.f;
 m_motorTarget=shaft&&phase==World::LiftPhase::Ascending?.8f:shaft&&phase==World::LiftPhase::Jammed?.25f:0.f;
 auto sceneLoop=[&](Sound cue,int id){if(std::none_of(m_voices.begin(),m_voices.end(),[&](auto&v){return v.emitter==id;}))play({cue,{},1,1,false},id,true);};
 if(m_motorTarget>0)sceneLoop(Sound::LiftMotor,-2);
 if(m_reactorTarget>0)sceneLoop(Sound::ReactorMusic,-3);
 for(const auto&event:game.sounds())play(event);
 auto loop=[&](int emitter,Sound sound,Vec2 pos,float gain){auto it=std::find_if(m_voices.begin(),m_voices.end(),[&](auto&v){return v.emitter==emitter;});
  if(it==m_voices.end()){play({sound,pos,gain,1,true},emitter,true);}else{it->position=pos;it->gain=gain;}
 };
 for(int level=0;level<Game::ChunkCount;++level){if(!game.chunkResident(level)){std::erase_if(m_voices,[&](auto&v){return (v.emitter>=100+level*100&&v.emitter<200+level*100)||(v.emitter<=-1000-level*100&&v.emitter>-1100-level*100);});continue;}auto chunk=game.chunkView(level);auto shift=Game::chunkOffset(level)-Game::chunkOffset(game.level());
  for(size_t i=0;i<chunk.enemies().size();++i){auto&e=chunk.enemies()[i];int id=100+level*100+int(i);
   if(e.alive&&e.kind==Enemy::Kind::Wasp)loop(id,Sound::Wings,e.pos+shift,.3f);else std::erase_if(m_voices,[&](auto&v){return v.emitter==id;});
  }
  int index=0;for(auto emitter:chunk.world().machines())loop(-1000-level*100-index++,Sound::Machine,emitter+shift,.30f);
 }
 for(auto&voice:m_voices)spatialize(voice,game,dt);
}
void AudioEngine::mix(int16_t* output,size_t frames){
 std::fill(output,output+frames*2,int16_t(0));
 for(size_t frame=0;frame<frames;++frame){float left=0,right=0;
  // Per-sample envelopes: main track fades over roughly 2 seconds, motor
  // takes over quickly, and the reactor bed arrives slowly after the impact.
  m_mainBlend+=(m_mainTarget-m_mainBlend)/(.65f*Rate);
  m_reactorBlend+=(m_reactorTarget-m_reactorBlend)/(1.2f*Rate);
  m_motorBlend+=(m_motorTarget-m_motorBlend)/(.18f*Rate);
  for(auto&v:m_voices){bool music=v.sound==Sound::Music||v.sound==Sound::ReactorMusic;
   if(m_paused&&!music)continue;auto&s=m_samples[size_t(v.sound)];size_t n=s.frames();if(v.cursor>=n){if(v.loop)v.cursor=std::fmod(v.cursor,double(n));else continue;}
   size_t a=size_t(v.cursor),b=a+1<n?a+1:v.loop?0:a;float frac=float(v.cursor-double(a));
   auto sample=[&](int channel){float value=s.pcm[a*s.channels+channel]*(1-frac)+s.pcm[b*s.channels+channel]*frac;
    if(v.loop&&(v.sound==Sound::Machine||v.sound==Sound::LiftMotor||v.sound==Sound::ReactorMusic)){size_t fade=std::min(size_t(2205),n/4);if(a>=n-fade){float blend=float(v.cursor-double(n-fade))/float(fade);size_t head=a-(n-fade);float incoming=s.pcm[head*s.channels+channel]*(1-frac)+s.pcm[(head+1)*s.channels+channel]*frac;value=value*(1-blend)+incoming*blend;}}
    return value/32768.f;};
   float blend=v.sound==Sound::Music?m_mainBlend:v.sound==Sound::ReactorMusic?m_reactorBlend:v.sound==Sound::LiftMotor?m_motorBlend:1.f;
   float gain=v.gain*(music?m_musicGain:m_effectsGain)*blend;
   v.smoothLeft+=(v.left*gain-v.smoothLeft)*.0015f;v.smoothRight+=(v.right*gain-v.smoothRight)*.0015f;
   left+=sample(0)*v.smoothLeft;right+=sample(s.channels-1)*v.smoothRight;v.cursor+=v.pitch;
   if(v.loop&&(v.sound==Sound::Machine||v.sound==Sound::LiftMotor||v.sound==Sound::ReactorMusic)&&v.cursor>=n)v.cursor=std::min(size_t(2205),n/4)+v.cursor-n;
  }
  m_master+=(m_targetMaster-m_master)*.002f;
  // Smooth limiter leaves headroom when footsteps, music and several attacks overlap.
  output[frame*2]=int16_t(std::tanh(left*.8f)*m_master*30000);output[frame*2+1]=int16_t(std::tanh(right*.8f)*m_master*30000);
 }
 std::erase_if(m_voices,[&](auto&v){return !v.loop&&v.cursor>=m_samples[size_t(v.sound)].frames();});
}
void AudioEngine::run(){
 while(!m_stop){for(auto&buffer:m_buffers)if(!(buffer.header.dwFlags&WHDR_INQUEUE)){
   {std::lock_guard lock(m_mutex);mix(buffer.pcm.data(),Block);}
   if(waveOutWrite(m_device,&buffer.header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR){m_stop=true;break;}
  }WaitForSingleObject(m_wake,10);
 }
}
bool AudioEngine::testLiftMix(){
 AudioEngine audio(false);auto game=Game::mapInspection({12,11},kPi*.5f,0,3,false,0,true);
 std::ofstream report("lift-audio-test.txt");std::vector<int16_t> output;
 auto block=[&](){audio.update(game);std::array<int16_t,1470> samples{};audio.mix(samples.data(),735);output.insert(output.end(),samples.begin(),samples.end());};
 for(int i=0;i<120;++i){game.update({},1.f/60);block();}
 game=Game::liftInspection(0);bool creak=false,snap=false,crash=false;
 for(int i=0;i<900;++i){game.update({},1.f/60);block();
  for(auto&e:game.sounds()){creak|=e.sound==Sound::LiftCreak;snap|=e.sound==Sound::LiftSnap;crash|=e.sound==Sound::LiftCrash;}
  if(i==240){if(audio.m_mainBlend>.01f||audio.m_motorBlend<.7f)return false;report<<"Main OST fades out; lift motor takes over: PASS\n";}
 }
 if(!creak||!snap||!crash||audio.m_reactorBlend<.95f||audio.m_motorBlend>.001f||audio.m_mainBlend>.001f)return false;
 report<<"Creak, snap, crash and sinister reactor music transition: PASS\n";
 size_t reactorVoices=0;for(auto&v:audio.m_voices)reactorVoices+=v.sound==Sound::ReactorMusic;
 if(reactorVoices!=1)return false;
 InputState muteMusic{};muteMusic.music=true;game.update(muteMusic,1.f/60);audio.update(game);
 if(audio.m_musicGain!=0||audio.m_effectsGain==0)return false;
 report<<"One reactor loop; music toggle preserves effects: PASS\n";
 game.restart();audio.update(game);if(audio.m_mainTarget!=1||audio.m_reactorTarget!=0||audio.m_motorTarget!=0)return false;
 report<<"Restart restores main OST: PASS\n";
 std::ofstream file("lift-audio-preview.wav",std::ios::binary);uint32_t bytes=uint32_t(output.size()*2),riff=bytes+36,fmt=16,rate=Rate,byteRate=Rate*4;uint16_t pcm=1,channels=2,align=4,bits=16;
 auto write=[&](auto v){file.write(reinterpret_cast<const char*>(&v),sizeof(v));};
 file.write("RIFF",4);write(riff);file.write("WAVEfmt ",8);write(fmt);write(pcm);write(channels);write(rate);write(byteRate);write(align);write(bits);file.write("data",4);write(bytes);file.write(reinterpret_cast<const char*>(output.data()),bytes);
 return true;
}
bool AudioEngine::test(){
 AudioEngine audio(false);auto game=Game::validationScene(Enemy::Kind::Brute);
 std::vector<int16_t> output;output.reserve(Rate*2*12);size_t maxVoices=0;double energy=0;
 for(int frame=0;frame<720;++frame){InputState input{};input.forward=frame<95;input.back=frame>=95&&frame<185;input.jump=frame==125;input.fire=frame==220||frame==280||frame==340||frame==400;game.update(input,1.f/60.f);audio.update(game);
  if(frame==450)audio.play({Sound::WaspAttack,game.player().pos+Vec2{0,2},.8f,1,true});
  if(frame==510)audio.play({Sound::SpiderDeath,game.player().pos+Vec2{0,-2},.8f,1,true});
  if(frame==560)audio.play({Sound::JunkMetal,game.player().pos+Vec2{0,2},.8f,1,true});
  if(frame==610)audio.play({Sound::JunkGlass,game.player().pos+Vec2{0,-2},.8f,1,true});
  if(frame==660)audio.play({Sound::JunkSoft,game.player().pos+Vec2{1,0},.8f,1,true});
  for(auto&v:audio.m_voices)audio.spatialize(v,game);
  maxVoices=std::max(maxVoices,audio.m_voices.size());std::array<int16_t,1470> block;audio.mix(block.data(),735);for(auto v:block)energy+=double(v)*v;output.insert(output.end(),block.begin(),block.end());
 }
 // Positional cues must favor the correct speaker and decay with distance.
 Voice right{Sound::WaspCall};right.spatial=true;right.position=game.player().pos+Vec2{0,2};audio.spatialize(right,game);
 Voice distantVoice=right;distantVoice.position=game.player().pos+Vec2{0,15};audio.spatialize(distantVoice,game);
 if(right.right<=right.left||distantVoice.right>=right.right||maxVoices<4||energy<1e8)return false;
 Voice machine{Sound::Machine};machine.spatial=true;machine.position={4.f,10.5f};float previous=-1,maxChange=0;
 for(int i=0;i<720;++i){float angle=i*kTwoPi/720;auto orbit=Game::mapInspection({4.f+std::cos(angle)*1.5f,10.5f+std::sin(angle)*1.5f},angle+kPi*.5f);
  audio.spatialize(machine,orbit);float total=machine.left+machine.right;
  if(!std::isfinite(total))return false;if(previous>=0)maxChange=std::max(maxChange,std::fabs(total-previous));previous=total;
 }if(maxChange>.08f)return false;
 auto machines=game.world().machines();for(size_t i=0;i<machines.size();++i)for(size_t j=i+1;j<machines.size();++j)if(length(machines[i]-machines[j])<1.01f)return false;
 std::ofstream("generator-audio-test.txt")<<"Generator orbit: finite stereo gains, no duplicate machine emitters, maximum gain change "<<maxChange<<"; 50 ms seamless loop crossfade enabled.\n";
 audio.m_targetMaster=0;std::vector<int16_t> mute(Rate*2);audio.mix(mute.data(),Rate);
 for(size_t i=mute.size()-200;i<mute.size();++i)if(mute[i]!=0)return false;
 std::ofstream file("audio-preview.wav",std::ios::binary);uint32_t bytes=uint32_t(output.size()*2),riff=bytes+36,fmtSize=16,rate=Rate,byteRate=Rate*4;uint16_t pcm=1,channels=2,align=4,bits=16;
 auto write=[&](auto value){file.write(reinterpret_cast<const char*>(&value),sizeof(value));};
 file.write("RIFF",4);write(riff);file.write("WAVEfmt ",8);write(fmtSize);write(pcm);write(channels);write(rate);write(byteRate);write(align);write(bits);file.write("data",4);write(bytes);file.write(reinterpret_cast<const char*>(output.data()),bytes);
 std::ofstream("audio-test.txt")<<"Loaded "<<audio.m_samples.size()<<" embedded samples.\n12 second gameplay mix; peak voices: "<<maxVoices<<"\nStereo direction, distance attenuation, overlap, mute and PCM output: PASS\n";
 return bool(file);
}
bool AudioEngine::testDevice(){
 AudioEngine audio;std::ofstream report("audio-device-test.txt");
 if(!audio.available()){report<<"No Windows audio output device available.\n";return false;}
 auto game=Game::validationScene(Enemy::Kind::Huntsman,3);
 for(int frame=0;frame<120;++frame){InputState input{};input.forward=frame<70;input.fire=frame==75;game.update(input,1.f/60.f);audio.update(game);std::this_thread::sleep_for(std::chrono::milliseconds(17));}
 MMTIME position{};position.wType=TIME_SAMPLES;auto result=waveOutGetPosition(audio.m_device,&position,sizeof(position));
 bool passed=result==MMSYSERR_NOERROR&&position.wType==TIME_SAMPLES&&position.u.sample>44100&&!audio.m_stop;
 report<<"Windows waveOut stereo PCM playback: "<<(passed?"PASS":"FAIL")<<"\nDevice-reported samples played: "<<position.u.sample<<"\n";return passed;
}
}


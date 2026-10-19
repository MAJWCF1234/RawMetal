#include "AudioEngine.h"
#include "NpcVoiceCatalog.h"
#include "../core/PackedResource.h"
#include "../game/Game.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <chrono>
namespace retro {
namespace {
enum class AmbientKind : std::uint64_t { Wings=1, Machine=2, Water=3 };
std::int64_t ambientEmitter(AmbientKind kind,int level,std::size_t index){
 return std::int64_t((std::uint64_t(kind)<<48)|(std::uint64_t(level)<<32)|std::uint64_t(index));
}
bool emitterInChunk(std::int64_t emitter,int level){return emitter>0&&int((std::uint64_t(emitter)>>32)&0xffffu)==level;}
bool writeVoicePreview(const std::vector<int16_t>& pcm,const char* path="npc-voice-preview.wav"){
 std::ofstream file(path,std::ios::binary);auto write=[&](auto value){file.write(reinterpret_cast<const char*>(&value),sizeof(value));};
 file.write("RIFF",4);write(uint32_t(pcm.size()*2+36));file.write("WAVEfmt ",8);write(uint32_t(16));write(uint16_t(1));write(uint16_t(2));
 write(uint32_t(44100));write(uint32_t(176400));write(uint16_t(4));write(uint16_t(16));file.write("data",4);write(uint32_t(pcm.size()*2));file.write(reinterpret_cast<const char*>(pcm.data()),pcm.size()*2);return bool(file);
}
}
std::vector<int16_t> decodeMusic(const unsigned char* data,size_t bytes);
AudioEngine::AudioEngine(bool device){
 for(size_t i=0;i<m_samples.size();++i){
  auto resource=loadResource(i==size_t(Sound::FreightMusic)?271:i==size_t(Sound::WorkerDying)?283:int(200+i));auto data=resource.data();size_t size=resource.size();
  auto u16=[](const unsigned char*p){return unsigned(p[0])|(unsigned(p[1])<<8);};
  auto u32=[](const unsigned char*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
  if(size<12||std::memcmp(data,"RIFF",4)||std::memcmp(data+8,"WAVE",4)){
   m_samples[i].pcm=decodeMusic(data,size);m_samples[i].channels=2;continue;
  }
  const unsigned char* pcm=nullptr;size_t bytes=0;bool format=false,compressed=false;
  for(size_t offset=12;offset+8<=size;){size_t count=u32(data+offset+4);auto chunk=data+offset+8;
   if(count>size-offset-8)throw std::runtime_error("Truncated audio resource");
   if(!std::memcmp(data+offset,"fmt ",4)&&count>=16){auto channels=u16(chunk+2);compressed=u16(chunk)!=1;format=u16(chunk)==1&&(channels==1||channels==2)&&u32(chunk+4)==Rate&&u16(chunk+14)==16;m_samples[i].channels=int(channels);}
   if(!std::memcmp(data+offset,"data",4)){pcm=chunk;bytes=count;}
   offset+=8+count+(count&1);
  }
  if(compressed){m_samples[i].pcm=decodeMusic(data,size);m_samples[i].channels=2;continue;}
  if(!format||!pcm||bytes<4||bytes%(2*m_samples[i].channels))throw std::runtime_error("Audio must be PCM16 at 44100 Hz");
  auto&sample=m_samples[i];sample.pcm.resize(bytes/2);std::memcpy(sample.pcm.data(),pcm,bytes);
  // A short boundary taper suppresses clicks when ambience wraps.
  if(i==size_t(Sound::Music)||i==size_t(Sound::Machine)||i==size_t(Sound::Wings)||i==size_t(Sound::WaterReturn)){
   size_t fade=std::min(size_t(220),sample.frames()/2);
   for(size_t f=0;f<fade;++f)for(int c=0;c<sample.channels;++c){sample.pcm[f*sample.channels+c]=int16_t(sample.pcm[f*sample.channels+c]*float(f)/float(fade));auto end=(sample.frames()-1-f)*sample.channels+c;sample.pcm[end]=int16_t(sample.pcm[end]*float(f)/float(fade));}
  }
 }
 auto bank=loadResource(347);auto word=[&](size_t offset){uint32_t v=0;if(offset+4>bank.size())throw std::runtime_error("Truncated NPC voice bank");std::memcpy(&v,bank.data()+offset,4);return v;};
 if(bank.size()<8||std::memcmp(bank.data(),"RVB1",4)||word(4)!=NpcVoiceClips.size())throw std::runtime_error("Invalid NPC voice bank");
 size_t offset=8;for(size_t i=0;i<NpcVoiceClips.size();++i){size_t count=word(offset);offset+=4;if(count>bank.size()-offset)throw std::runtime_error("Truncated NPC voice clip");
  Sample sample;sample.channels=2;sample.pcm=decodeMusic(bank.data()+offset,count);if(sample.frames()<Rate/5||sample.frames()>Rate*12)throw std::runtime_error("Invalid NPC voice duration");m_speech.push_back(std::move(sample));offset+=count;}
 if(offset!=bank.size())throw std::runtime_error("Trailing NPC voice data");
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
void AudioEngine::play(const SoundEvent& event,std::int64_t emitter,bool loop){
 if(event.speech>=0){if(size_t(event.speech)>=m_speech.size()||!event.speaker)return;
  std::erase_if(m_voices,[&](const auto&voice){return voice.speech>=0&&voice.speaker==event.speaker;});
  if(std::count_if(m_voices.begin(),m_voices.end(),[](const auto&v){return v.speech>=0;})>=2){
   auto victim=m_voices.end();for(auto it=m_voices.begin();it!=m_voices.end();++it)if(it->speech>=0&&(victim==m_voices.end()||it->speechPriority<victim->speechPriority))victim=it;
   if(victim!=m_voices.end()){if(victim->speechPriority>event.speechPriority)return;m_voices.erase(victim);}}
 }
 if(m_voices.size()>=48){auto voice=std::find_if(m_voices.begin(),m_voices.end(),[](auto&v){return !v.loop;});if(voice!=m_voices.end())m_voices.erase(voice);else return;}
 m_voices.push_back({event.sound,0,event.gain,event.pitch,1,1,event.position,event.spatial,loop,emitter});
 m_voices.back().radius=std::max(.1f,event.radius);
 m_voices.back().z=event.z;
 m_voices.back().speech=event.speech;m_voices.back().speaker=event.speaker;m_voices.back().speechPriority=event.speechPriority;
 if(loop&&(event.sound==Sound::Machine||event.sound==Sound::WaterReturn))m_voices.back().cursor=double((unsigned(-emitter)*7919u)%m_samples[size_t(event.sound)].frames());
}
void AudioEngine::spatialize(Voice& voice,const Game& game,float dt){
 if(!voice.spatial){voice.left=voice.right=1;voice.filterTarget=1;return;}
 auto delta=voice.position-game.player().pos;float planar=length(delta),listenerZ=game.player().z+game.player().eye;
 float sourceZ=std::isfinite(voice.z)?voice.z:listenerZ;
 float distance=std::sqrt(lengthSq(delta)+(sourceZ-listenerZ)*(sourceZ-listenerZ));
 float gain=std::pow(std::max(0.f,1-distance/voice.radius),2.f);
 // Inaudible voices need no geometry queries; keep their envelope for re-entry.
 if(gain<=0||voice.gain<=0){voice.left=voice.right=0;return;}
 float pan=planar>.01f?dot(delta*(1/planar),Vec2{-std::sin(game.player().angle),std::cos(game.player().angle)}):0;
 pan=std::clamp(pan,-1.f,1.f); // Roundoff at full pan must never reach sqrt of a negative number.
 // Three rays and a slower occlusion envelope prevent abrupt volume pumping at corners.
 Vec2 side=planar>.001f?Vec2{-delta.y/planar,delta.x/planar}:Vec2{};float blocked=0;
 // Generator origins sit inside their enclosures; retain the horizontal exemption.
 float farTrim=voice.sound==Sound::Machine&&planar>.001f?std::max(.25f,1.7f*distance/planar):.25f;
 if(distance>.5f)for(float offset:{-.18f,0.f,.18f})if(!game.soundRayClear(game.player().pos+side*offset,listenerZ,voice.position+side*offset,sourceZ,.25f,farTrim))blocked+=1;
 float target=1-blocked*(.68f/3.f);voice.occlusion+=(target-voice.occlusion)*(1-std::exp(-dt*7.f));gain*=voice.occlusion;
 // Walls absorb treble as well as level. Compute the pole on the game
 // thread, never exponentials in the per-sample spatial filter.
 float openness=std::clamp((voice.occlusion-.32f)/.68f,0.f,1.f);
 float cutoff=900.f+17100.f*openness*openness;
 voice.filterTarget=openness+(1-openness)*(1-std::exp(-kTwoPi*cutoff/Rate));
 voice.left=gain*std::sqrt((1-pan)*.5f);voice.right=gain*std::sqrt((1+pan)*.5f);
}
void AudioEngine::update(const Game& game,bool focused){
 std::lock_guard lock(m_mutex);
 float dt=std::clamp(game.elapsed()-m_lastTime,0.f,.1f);
 bool newSession=game.sessionRevision()!=m_lastRevision||int(game.worldId())!=m_lastWorld;
 m_lastWorld=int(game.worldId());
 if(game.elapsed()<m_lastTime||newSession){m_voices.clear();m_lastChunk=-1;play({Sound::Music,{},1,1,false},-1,true);m_mainBlend=1;m_reactorBlend=m_motorBlend=m_freightBlend=0;}m_lastRevision=game.sessionRevision();
 if(m_lastChunk>=0&&m_lastChunk!=game.level()){auto shift=game.chunkOffset(m_lastChunk)-game.chunkOffset(game.level());for(auto&voice:m_voices)if(voice.spatial)voice.position+=shift;}m_lastChunk=game.level();
 m_lastTime=game.elapsed();m_targetMaster=focused&&!game.audioMuted()?game.settings().master:0.f;
 m_submergedTarget=game.world().waterSurface(game.player().pos.x,game.player().pos.y)>game.player().z+game.player().eye+.03f?1.f:0.f;
 m_musicGain=game.musicEnabled()?(game.dead()||game.won()?.16f:.28f)*game.settings().music/.75f:0;
 m_effectsGain=game.settings().effects;m_paused=game.paused()||game.consoleOpen();
 auto phase=game.world().liftPhase();bool shaft=game.world().hasLift();
 m_freightTarget=game.world().definition().musicCue==MusicCue::Freight?1.f:0.f;
 m_mainTarget=m_freightTarget>0?0.f:!shaft||phase==World::LiftPhase::Ready?1.f:0.f;
 m_reactorTarget=shaft&&phase==World::LiftPhase::Crashed&&game.world().liftPhaseTime()>2?1.f:0.f;
 m_motorTarget=shaft?game.world().liftMotorGain():0.f;
 if(newSession){m_mainBlend=m_mainTarget;m_reactorBlend=m_reactorTarget;m_motorBlend=m_motorTarget;m_freightBlend=m_freightTarget;}
 auto sceneLoop=[&](Sound cue,int id){if(std::none_of(m_voices.begin(),m_voices.end(),[&](auto&v){return v.emitter==id;}))play({cue,{},1,1,false},id,true);};
 if(m_motorTarget>0)sceneLoop(Sound::LiftMotor,-2);
 for(auto&voice:m_voices)if(voice.emitter==-2)voice.pitch=m_motorTarget<.3f?.65f:game.world().liftPhaseTime()>24?.82f:1.f;
 if(m_freightTarget>0)sceneLoop(Sound::FreightMusic,-4);
 if(m_reactorTarget>0)sceneLoop(Sound::ReactorMusic,-3);
 std::erase_if(m_voices,[&](const auto&voice){if(voice.speech<0)return false;if(!voice.speaker||voice.speaker>game.npcActors().size())return true;
  size_t actor=voice.speaker-1;const auto&npc=game.npcActors()[actor];return !game.chunkResident(npc.level)||(voice.speechPriority<3&&(game.npcIncapacitated(actor)||game.state(npc.track().deadState)));});
 for(const auto&event:game.sounds()){
  if(event.speech>=0){if(!event.speaker||event.speaker>game.npcActors().size())continue;size_t actor=event.speaker-1;
   if(event.speechPriority<3&&(game.npcIncapacitated(actor)||game.state(game.npcActors()[actor].track().deadState)))continue;}
  play(event);
 }
 for(auto&voice:m_voices)if(voice.speech>=0&&voice.speaker&&voice.speaker<=game.npcActors().size()){
  const auto&npc=game.npcActors()[voice.speaker-1];voice.position=npc.pose.position+game.chunkOffset(npc.level)-game.chunkOffset(game.level());voice.z=npc.pose.z+npc.track().scale*.85f;}
 auto loop=[&](std::int64_t emitter,Sound sound,Vec2 pos,float gain,float z=std::numeric_limits<float>::quiet_NaN()){auto it=std::find_if(m_voices.begin(),m_voices.end(),[&](auto&v){return v.emitter==emitter;});
  if(it==m_voices.end()){play({sound,pos,gain,1,true,17,z},emitter,true);}else{it->position=pos;it->gain=gain;it->z=z;}
 };
 std::erase_if(m_voices,[&](auto&v){int level=int((std::uint64_t(v.emitter)>>32)&0xffffu);return v.emitter>0&&!game.chunkResident(level);});
 for(int level=0;level<game.chunkCount();++level){if(!game.chunkResident(level))continue;const auto&world=game.chunkWorld(level);const auto&enemies=game.chunkEnemies(level);auto shift=game.chunkOffset(level)-game.chunkOffset(game.level());
  for(size_t i=0;i<enemies.size();++i){auto&e=enemies[i];auto id=ambientEmitter(AmbientKind::Wings,level,i);
   if(e.alive&&e.kind==Enemy::Kind::Wasp)loop(id,Sound::Wings,e.pos+shift,.3f,e.z+.8f);else std::erase_if(m_voices,[&](auto&v){return v.emitter==id;});
  }
  float machineGain=.30f;if(world.hasLift()&&game.world().hasLift())machineGain/=1.f+(game.player().z+8)*(game.player().z+8)*.1f;
  size_t index=0;for(auto emitter:world.machines())loop(ambientEmitter(AmbientKind::Machine,level,index++),Sound::Machine,emitter+shift,machineGain);
  index=0;for(const auto& basin:world.waterVolumes())
   loop(ambientEmitter(AmbientKind::Water,level,index++),Sound::WaterReturn,Vec2{(basin.x1+basin.x2)*.5f,(basin.y1+basin.y2)*.5f}+shift,.18f,basin.surface);
 }
 for(auto&voice:m_voices)spatialize(voice,game,dt);
}
void AudioEngine::mix(int16_t* output,size_t frames){
 std::fill(output,output+frames*2,int16_t(0));
 for(size_t frame=0;frame<frames;++frame){float left=0,right=0;
  // Per-sample envelopes: main track fades over roughly 2 seconds, motor
  // takes over quickly, and the reactor bed arrives slowly after the impact.
  m_mainBlend+=(m_mainTarget-m_mainBlend)/(.65f*Rate);
  m_freightBlend+=(m_freightTarget-m_freightBlend)/(.65f*Rate);
  m_reactorBlend+=(m_reactorTarget-m_reactorBlend)/(1.2f*Rate);
  m_motorBlend+=(m_motorTarget-m_motorBlend)/(.18f*Rate);
  for(auto&v:m_voices){bool music=v.sound==Sound::Music||v.sound==Sound::FreightMusic||v.sound==Sound::ReactorMusic;
   if(m_paused&&!music)continue;const auto&s=sampleFor(v);size_t n=s.frames();if(v.cursor>=n){if(v.loop)v.cursor=std::fmod(v.cursor,double(n));else continue;}
   size_t a=size_t(v.cursor),b=a+1<n?a+1:v.loop?0:a;float frac=float(v.cursor-double(a));
   auto sample=[&](int channel){float value=s.pcm[a*s.channels+channel]*(1-frac)+s.pcm[b*s.channels+channel]*frac;
    if(v.loop&&(v.sound==Sound::Machine||v.sound==Sound::LiftMotor||v.sound==Sound::ReactorMusic||v.sound==Sound::FreightMusic||v.sound==Sound::WaterReturn)){size_t fade=std::min(size_t(2205),n/4);if(a>=n-fade){float blend=float(v.cursor-double(n-fade))/float(fade);size_t head=a-(n-fade);float incoming=s.pcm[head*s.channels+channel]*(1-frac)+s.pcm[(head+1)*s.channels+channel]*frac;value=value*(1-blend)+incoming*blend;}}
    return value/32768.f;};
   float blend=v.sound==Sound::FreightMusic?m_freightBlend:v.sound==Sound::Music?m_mainBlend:v.sound==Sound::ReactorMusic?m_reactorBlend:v.sound==Sound::LiftMotor?m_motorBlend:1.f;
   float gain=v.gain*(music?m_musicGain:m_effectsGain)*blend;
   v.smoothLeft+=(v.left*gain-v.smoothLeft)*.0015f;v.smoothRight+=(v.right*gain-v.smoothRight)*.0015f;
   if(v.spatial){
    // A positional source has one location; baked stereo must not pull it
    // into two ears independently. Preserve stereo only for listener beds.
    float mono=s.channels==1?sample(0):(sample(0)+sample(1))*.5f;
    v.filter+=(v.filterTarget-v.filter)*.0015f;
    v.filtered+=(mono-v.filtered)*v.filter;
    left+=v.filtered*v.smoothLeft;right+=v.filtered*v.smoothRight;
   }else{left+=sample(0)*v.smoothLeft;right+=sample(s.channels-1)*v.smoothRight;}
   v.cursor+=v.pitch;
   if(v.loop&&(v.sound==Sound::Machine||v.sound==Sound::LiftMotor||v.sound==Sound::ReactorMusic||v.sound==Sound::FreightMusic||v.sound==Sound::WaterReturn)&&v.cursor>=n)v.cursor=std::min(size_t(2205),n/4)+v.cursor-n;
  }
  m_master+=(m_targetMaster-m_master)*.002f;
  m_submerged+=(m_submergedTarget-m_submerged)*.0006f;
  m_underwaterLeft+=(left-m_underwaterLeft)*.055f;
  m_underwaterRight+=(right-m_underwaterRight)*.055f;
  left+=m_submerged*(m_underwaterLeft-left);
  right+=m_submerged*(m_underwaterRight-right);
  // Smooth limiter leaves headroom when footsteps, music and several attacks overlap.
  output[frame*2]=int16_t(std::tanh(left*.8f)*m_master*30000);output[frame*2+1]=int16_t(std::tanh(right*.8f)*m_master*30000);
 }
 std::erase_if(m_voices,[&](auto&v){return !v.loop&&v.cursor>=sampleFor(v).frames();});
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
 for(int i=0;i<3300;++i){game.update({},1.f/60);block();
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
 std::ofstream diagnostic("audio-validation.txt");auto failure=[&](const char* reason){diagnostic<<"FAIL: "<<reason<<'\n';return false;};
 // The old -2000-map*100 water IDs collided with machine IDs ten maps later.
 auto waterKey=ambientEmitter(AmbientKind::Water,5,0),machineKey=ambientEmitter(AmbientKind::Machine,15,0);
 if(waterKey==machineKey||emitterInChunk(waterKey,15)||!emitterInChunk(waterKey,5))return failure("ambient emitter namespace collision");
 AudioEngine audio(false);auto game=Game::validationScene(Enemy::Kind::Brute);
 if(audio.m_speech.size()!=NpcVoiceClips.size())return failure("NPC voice bank count");
 for(size_t i=0;i<audio.m_speech.size();++i){const auto&clip=audio.m_speech[i];
  if(std::fabs(double(clip.frames())/Rate-NpcVoiceClips[i].seconds)>.15||std::none_of(clip.pcm.begin(),clip.pcm.end(),[](int16_t p){return std::abs(int(p))>500;}))return failure("NPC voice silent or truncated");}
 {
  AudioEngine voices(false);voices.m_voices.clear();SoundEvent event{Sound::WorkerDying};event.speech=0;event.speaker=1;event.speechPriority=1;voices.play(event);voices.play(event);
  if(std::count_if(voices.m_voices.begin(),voices.m_voices.end(),[](const auto&v){return v.speech>=0;})!=1)return failure("same-speaker voice overlap");
  event.speaker=2;voices.play(event);event.speaker=3;event.speechPriority=2;voices.play(event);
  if(std::count_if(voices.m_voices.begin(),voices.m_voices.end(),[](const auto&v){return v.speech>=0;})!=2)return failure("NPC speech concurrency cap");
  std::vector<int16_t> mixed(Rate*2*3);voices.mix(mixed.data(),Rate*3);
  if(std::none_of(mixed.begin(),mixed.end(),[](int16_t p){return std::abs(int(p))>300;}))return failure("NPC speech mixer output");
  voices.m_voices.clear();std::vector<int16_t> audition;
  for(int role=1;role<=7;++role){size_t clip=0;while(clip<NpcVoiceClips.size()&&NpcVoiceClips[clip].role!=role)++clip;if(clip==NpcVoiceClips.size())return failure("missing voice role");
   event.speaker=size_t(role);event.speech=int(clip);event.speechPriority=1;voices.play(event);size_t frames=voices.m_speech[clip].frames()+Rate/3;
   std::vector<int16_t> rendered(frames*2);voices.mix(rendered.data(),frames);audition.insert(audition.end(),rendered.begin(),rendered.end());}
  if(!writeVoicePreview(audition))return failure("NPC voice audition export");
 }
 // Synthetic alternating stereo channels prove the real mixer collapses
 // positional stereo and that obstruction removes treble without muting bass.
 {
  AudioEngine probe(false);probe.m_voices.clear();probe.m_musicGain=0;
  auto& sample=probe.m_samples[size_t(Sound::JunkMetal)];sample.channels=2;
  sample.pcm.resize(Rate*2);
  auto render=[&](bool spatial,float pole,float frequency,bool opposing){
   for(int i=0;i<Rate;++i){auto value=int16_t(5000*std::sin(kTwoPi*frequency*i/Rate));sample.pcm[i*2]=value;sample.pcm[i*2+1]=opposing?-value:value;}
   probe.m_voices.clear();probe.play({Sound::JunkMetal,{},1,1,spatial});
   auto& v=probe.m_voices.back();v.filter=v.filterTarget=pole;
   std::vector<int16_t> output(Rate*2);probe.mix(output.data(),Rate);
   double energy=0;for(size_t i=Rate;i<output.size();++i)energy+=double(output[i])*output[i];return energy;
  };
  if(render(true,1,1000,true)!=0||render(false,1,1000,true)<1e8)return failure("positional stereo collapse or nonspatial stereo preservation");
  float blockedPole=1-std::exp(-kTwoPi*900.f/Rate);
  double low=render(true,blockedPole,120,false)/render(true,1,120,false);
  double high=render(true,blockedPole,8000,false)/render(true,1,8000,false);
  if(low<.9||high>.03)return failure("wall filter bass preservation or treble attenuation");
  diagnostic<<"Spatial stereo resolves to one emitter; nonspatial stereo preserved. Wall filter energy ratios bass "<<low<<" treble "<<high<<": PASS\n";
 }
 {
  AudioEngine demo(false);demo.m_voices.clear();demo.m_musicGain=0;
  std::vector<int16_t> audition;
  // Four two-second sections: clear left, blocked left, clear right,
  // blocked right. Same machine recording and level before wall attenuation.
  for(int section=0;section<4;++section){
   demo.m_voices.clear();demo.play({Sound::Machine,{},.4f,1,true},0,true);
   auto& v=demo.m_voices.back();bool blocked=section%2!=0;
   v.left=section<2?.9f:.1f;v.right=section<2?.1f:.9f;
   if(blocked){v.left*=.32f;v.right*=.32f;}
   v.filter=v.filterTarget=blocked?1-std::exp(-kTwoPi*900.f/Rate):1.f;
   std::vector<int16_t> part(Rate*4);demo.mix(part.data(),Rate*2);
   audition.insert(audition.end(),part.begin(),part.end());
  }
  if(!writeVoicePreview(audition,"spatial-wall-preview.wav"))return failure("spatial wall preview export");
  demo.m_voices.clear();for(int i=0;i<48;++i){demo.play({Sound::Machine,{},.01f,1,true},0,true);auto& v=demo.m_voices.back();v.filter=v.filterTarget=.15f;}
  std::array<int16_t,Block*2> block{};for(int i=0;i<16;++i)demo.mix(block.data(),Block);
  auto begin=std::chrono::steady_clock::now();for(int i=0;i<256;++i)demo.mix(block.data(),Block);
  double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/256;
  diagnostic<<"48 spatial loop mixer stress: "<<ms<<" ms per 512-frame block (11.61 ms output); wall demo exported.\n";
 }
 diagnostic<<"All "<<audio.m_speech.size()<<" NPC takes decode with expected duration and signal; mixer replacement and two-speaker cap: PASS\n";
 const auto& worker=audio.m_samples[size_t(Sound::WorkerDying)];
 if(worker.channels!=2||worker.frames()<Rate*9.3||worker.frames()>Rate*9.5||std::none_of(worker.pcm.begin(),worker.pcm.end(),[](int16_t p){return std::abs(int(p))>500;}))return failure("worker compressed WAV missing, silent or truncated");
 diagnostic<<"Original 22.05 kHz IMA ADPCM worker voice decodes to full 44.1 kHz mixer audio: PASS\n";
 // Small asset-free rooms isolate elevation from attenuation and chunk transforms.
 {
  auto scene=[](bool deck=false,bool joined=false,bool remoteWall=false,int gate=0){
   auto campaign=std::make_shared<CustomCampaign>();campaign->name="Audio height validation";campaign->key=781432;
   for(int level=0;level<(joined?2:1);++level){auto map=std::make_shared<AuthoredMapData>();map->name="Audio room";
    map->definition={{float(level*24),0},joined?Vec2{22.5f,6.5f}:Vec2{6.5f,6.5f},0,Environment::Interior,false,12,.3f,51};
    map->definition.spawnYaw=0;
    AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
    map->openEast=joined&&level==0;map->openWest=joined&&level==1;
    if(deck)map->structures.push_back({0,0,24,24,3,3.04f});
    if(remoteWall&&level==1)map->structures.push_back({1,4,1.1f,9,0,8});
    if(gate)map->doors.push_back({5,8,8,gate==2?1.f:0.f,gate==2});
    campaign->maps.push_back(map);
   }return Game(campaign);
  };
  AudioEngine spatial(false);auto room=scene();float eye=room.player().z+room.player().eye;
  Voice flat{Sound::JunkMetal};flat.spatial=true;flat.position=room.player().pos+Vec2{3,0};flat.z=eye;
  Voice legacy=flat;legacy.z=std::numeric_limits<float>::quiet_NaN();Voice elevated=flat;elevated.z=eye+6;
  spatial.spatialize(flat,room,1);spatial.spatialize(legacy,room,1);spatial.spatialize(elevated,room,1);
  float expected=std::pow(1-std::sqrt(45.f)/elevated.radius,2.f)*std::sqrt(.5f);
  diagnostic<<"Elevation fixture yaw "<<room.player().angle<<" eye "<<eye<<" flat "<<flat.left<<" legacy "<<legacy.left<<" elevated "<<elevated.left<<" expected "<<expected<<" occlusion "<<elevated.occlusion<<'\n';
  if(std::fabs(elevated.left-expected)>.0001f||elevated.left>=flat.left||std::fabs(legacy.left-flat.left)>.0001f)return failure("3D attenuation or legacy unspecified height");
  diagnostic<<"Elevated voices attenuate by true 3D distance; unspecified height retains planar attenuation: PASS\n";
  auto stacked=scene(true);Voice screened=elevated;screened.occlusion=1;spatial.spatialize(screened,stacked,1);
  diagnostic<<"Deck occlusion "<<screened.occlusion<<" screened gain "<<screened.left<<'\n';
  if(screened.occlusion>.33f||screened.left>=elevated.left*.34f)return failure("thin intervening floor does not occlude elevated sound");
  Voice overhead=elevated;overhead.position=room.player().pos;spatial.spatialize(overhead,room,1);
  if(!std::isfinite(overhead.left)||std::fabs(overhead.left-overhead.right)>.0001f)return failure("vertical-only sound pan");
  diagnostic<<"Four-centimetre floor occludes an elevated emitter; directly overhead stereo remains finite and centered: PASS\n";
  auto closed=scene(false,false,false,1),opened=scene(false,false,false,2);Voice door=flat;door.position={6.5f,10.5f};door.z=eye+.6f;
  spatial.spatialize(door,closed,1);Voice clearDoor=door;clearDoor.occlusion=1;spatial.spatialize(clearDoor,opened,1);
  diagnostic<<"Door occlusion closed "<<door.occlusion<<" opened "<<clearDoor.occlusion<<'\n';
  if(door.filterTarget>.14f||clearDoor.filterTarget<.99f)return failure("door spectral occlusion does not follow geometry");
  if(door.occlusion>.33f||clearDoor.occlusion<.99f)return failure("door state or interpolated height occlusion");
  diagnostic<<"Closed/open doors attenuate interpolated-height sound rays while preserving the smooth envelope: PASS\n";
  Voice inaudible=elevated;inaudible.z=eye+30;inaudible.occlusion=.7f;spatial.spatialize(inaudible,stacked,1);
  Voice silent=door;silent.gain=0;silent.occlusion=.7f;spatial.spatialize(silent,closed,1);
  if(inaudible.left!=0||inaudible.right!=0||inaudible.occlusion!=.7f||silent.left!=0||silent.right!=0||silent.occlusion!=.7f)return failure("inaudible spatial voices still trace occlusion geometry");
  diagnostic<<"Zero-gain and out-of-radius voices skip occlusion geometry without advancing the retained envelope: PASS\n";
  spatial.m_voices.clear();spatial.play({Sound::JunkMetal,flat.position,.8f,1,true,17,elevated.z});
  if(spatial.m_voices.back().z!=elevated.z)return failure("play drops captured sound height");
  Voice centered{Sound::JunkMetal};centered.z=elevated.z;spatial.spatialize(centered,room);
  if(centered.left!=1||centered.right!=1||centered.z!=elevated.z)return failure("nonspatial sound is no longer listener centered");
  auto joined=scene(false,true),blockedJoin=scene(false,true,true);Voice remote=flat;remote.position={27.5f,6.5f};remote.z=eye+3;
  spatial.spatialize(remote,joined,1);Voice occluded=remote;occluded.occlusion=1;spatial.spatialize(occluded,blockedJoin,1);
  diagnostic<<"Joined chunk occlusion open "<<remote.occlusion<<" blocked "<<occluded.occlusion<<'\n';
  if(remote.occlusion<.99f||occluded.occlusion>.33f)return failure("remote chunk geometry uses incorrect ray coordinates");
  spatial.update(joined);spatial.play({Sound::JunkMetal,remote.position,.8f,1,true,17,remote.z});
  auto next=joined.chunkView(1);spatial.update(next);auto emitted=std::find_if(spatial.m_voices.begin(),spatial.m_voices.end(),[](const auto&v){return v.sound==Sound::JunkMetal;});
  if(emitted==spatial.m_voices.end()||emitted->z!=remote.z||length(emitted->position-Vec2{3.5f,6.5f})>.001f)return failure("chunk handoff drops or offsets sound height");
  diagnostic<<"Playback keeps captured height, nonspatial cues remain centered, and chunk handoffs transform XY without changing Z: PASS\n";
  diagnostic<<"Remote chunk structures occlude interpolated-height rays in their own local coordinates: PASS\n";
  auto wasp=Game::validationScene(Enemy::Kind::Wasp);spatial.update(wasp);auto wings=std::find_if(spatial.m_voices.begin(),spatial.m_voices.end(),[](const auto&v){return v.sound==Sound::Wings;});
  if(wings==spatial.m_voices.end()||std::fabs(wings->z-(wasp.enemies()[0].z+.8f))>.001f)return failure("wasp loop loses body center elevation");
  for(const auto&v:spatial.m_voices)if(v.sound==Sound::Machine&&std::isfinite(v.z))return failure("legacy machine loop gains an invented elevation");
  diagnostic<<"Wasp loops use body-center elevation; static machine loops retain unspecified height: PASS\n";
 }
 // Authored music coverage and seam continuity: map IDs do not route music.
 {
  AudioEngine district(false);std::array<int16_t,Rate*2> block{};
  if(district.m_samples[size_t(Sound::FreightMusic)].frames()<Rate*20)return failure("freight track missing or truncated");
  double cursor=-1;
  for(int level=10;level<CampaignChunkCount;++level){auto view=game.chunkView(level);district.update(view);district.mix(block.data(),Rate);
   if(district.m_freightTarget!=1||district.m_mainTarget!=0)return failure("freight music coverage");
   int count=0;for(const auto&v:district.m_voices)if(v.sound==Sound::FreightMusic){++count;if(v.cursor<=cursor)return failure("freight soundtrack restarts at seam");cursor=v.cursor;}
   if(count!=1)return failure("duplicated freight music voice");
  }
  district.update(game);if(district.m_freightTarget!=0||district.m_mainTarget!=1)return failure("original soundtrack return");
  diagnostic<<"Freight music covers all 22 chunks, retains its cursor across seams, and returns to original OST: PASS\n";
 }
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
 diagnostic<<"Spatial gains "<<right.left<<' '<<right.right<<" distant "<<distantVoice.right<<" voices "<<maxVoices<<" energy "<<energy<<'\n';
 if(right.right<=right.left||distantVoice.right>=right.right||maxVoices<4||energy<1e8)return failure("spatial direction, attenuation, overlap or mix energy");
 Voice machine{Sound::Machine};machine.spatial=true;machine.position={4.f,10.5f};float previous=-1,maxChange=0;
 for(int i=0;i<720;++i){float angle=i*kTwoPi/720;auto orbit=Game::mapInspection({4.f+std::cos(angle)*1.5f,10.5f+std::sin(angle)*1.5f},angle+kPi*.5f);
  audio.spatialize(machine,orbit);float total=machine.left+machine.right;
  if(!std::isfinite(total))return failure("nonfinite machine spatial gain");if(previous>=0)maxChange=std::max(maxChange,std::fabs(total-previous));previous=total;
 }if(maxChange>.08f)return failure("machine orbit gain discontinuity");
 auto machines=game.world().machines();for(size_t i=0;i<machines.size();++i)for(size_t j=i+1;j<machines.size();++j)if(length(machines[i]-machines[j])<1.01f)return failure("duplicate machine emitters");
 auto coolant=Game::mapInspection({12,9},0,0,5,false,-9,true);audio.update(coolant);
 auto submerged=Game::mapInspection({8.5f,9.f},0,0,5,false,-9.4f,true);InputState duck{};duck.crouch=true;
 for(int i=0;i<30;++i)submerged.update(duck,1.f/60.f);
 audio.update(submerged);if(audio.m_submergedTarget<.5f)return failure("underwater fixture camera is above water");
 audio.update(coolant);if(audio.m_submergedTarget>.5f)return failure("dry camera remains submerged");
 int waterVoices=0;for(const auto& voice:audio.m_voices)waterVoices+=voice.sound==Sound::WaterReturn;
 for(const auto& voice:audio.m_voices)if(voice.sound==Sound::WaterReturn&&!std::isfinite(voice.z))return failure("water loop has no surface elevation");
 int expectedWaterVoices=0;for(int level=0;level<coolant.chunkCount();++level)if(coolant.chunkResident(level))expectedWaterVoices+=int(coolant.chunkView(level).world().waterVolumes().size());
 if(waterVoices!=expectedWaterVoices)return failure("water voice count differs from resident authored volumes");
 game=Game::validationScene(Enemy::Kind::Brute);audio.update(game);
 for(const auto& voice:audio.m_voices)if(voice.sound==Sound::WaterReturn)return failure("water voices survive world change");
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


// The dead are quiet: what people say as they are hit and as they die, and that nothing
// vocal comes from a body afterwards (the user on v1.4.2: "the guy keeps screaming when
// he's dead"). Both tubes run the game's own audio engine headless beside the scene, so
// they measure what the mixer plays, not only what the game raises.
//   death-voices   kill a squad, two of the staff, a scripted worker on a looping timer
//                  that screams at him, and a dormant transformed worker with a looping
//                  moan at its bed; keep shooting the bodies, save and load, wait:
//                  nothing vocal from any of them after the frame each one dies.
//   death-variety  ten people die two seconds apart, then four transformed workers:
//                  the cries differ, never twice running, and the mix is written as a WAV.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include "audio/NpcVoiceCatalog.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// Someone who can die in the tube: an autonomous NPC (0), a scripted actor track (1) or a creature (2).
struct Who {std::string name;int kind=0;size_t index=0;float diedAt=-1;int deathEvents=0,after=0;std::vector<int> takes;};
struct Rig {
 std::unique_ptr<AudioEngine> audio;std::vector<int16_t> pcm;std::vector<Who> who;
 int lingering=0,deadSpeech=0,loads=0,shots=0,radioFrames=0;std::vector<int> deathOrder; // each death's take: a speech clip, or -1-sound
 std::string log;
};
std::map<const Scene*,std::unique_ptr<Rig>> rigs;
Rig& rig(Scene& s){auto& r=rigs[&s];if(!r)r=std::make_unique<Rig>();return *r;}

ActorTrack person(Vec2 at,ActorVisual visual,ActorAiMode mode,int squad,const std::string& tag,int health=100){
 auto a=regression::guard(at,at,AiFaction::Neutral);a.visual=visual;a.ai.mode=mode;a.ai.squad=squad;a.health=health;
 a.damageState=stateId("lab_dv_damage_"+tag);a.deadState=stateId("lab_dv_dead_"+tag); // on the room's own clock (reg_guard_clock)
 a.keys={{0,at,0,0},{60000,at,0,0}};if(mode==ActorAiMode::Guard)a.ai.patrol={{{at.x,at.y},0,60000}};else a.ai.patrol.clear();return a;}
void loopCue(AuthoredMapData& m,const char* clock,int durationMs,std::initializer_list<std::pair<int,Sound>> cues,Vec2 at,const char* voice=""){
 TimedSequence q;q.timerState=stateId(clock);q.finishState=stateId(std::string(clock)+"_done");q.durationMs=durationMs;q.finishAtMs=durationMs;q.soundIntervalMs=0;q.soundUntilMs=0;q.loop=true;
 m.timedSequences.push_back(q);for(auto [t,sound]:cues){SequenceCue c;c.timerState=q.timerState;c.timeMs=t;c.sound=sound;c.position=at;c.gain=.8f;c.voice=voice;m.sequenceCues.push_back(c);}}
// A speech clip by role and cue (and a line key, or none).
int clipFor(int role,NpcVoiceCue cue,bool keyed){for(size_t i=0;i<NpcVoiceClips.size();++i)if(NpcVoiceClips[i].role==role&&NpcVoiceClips[i].cue==cue&&bool(*NpcVoiceClips[i].line)==keyed)return int(i);return -1;}

const World& level0(const Game& g){return g.chunkWorld(0);}
bool dead(const Game& g,const Who& w){
 if(w.kind==0){const auto& t=g.npcActors()[w.index].track();return t.deadState&&g.state(t.deadState);}
 if(w.kind==1){const auto& t=level0(g).actorTracks()[w.index];return t.deadState&&g.state(t.deadState);}
 auto& e=LabAccess::enemies(const_cast<Game&>(g))[w.index];return !e.alive;}
Point3 where(Game& g,const Who& w){
 if(w.kind==0){const auto& n=g.npcActors()[w.index];return {n.pose.position.x,n.pose.position.y,n.pose.z};}
 if(w.kind==1){const auto p=level0(g).actorPose(w.index);return {p.position.x,p.position.y,p.z};}
 const auto& e=LabAccess::enemies(g)[w.index];return {e.pos.x,e.pos.y,e.z};}
// A sound in a person's (or a transformed man's) voice: speech, the old scream and hurt, the Vox block, the big bodies' calls.
bool vocal(const SoundEvent& e){if(e.speech>=0&&!e.speaker){const int role=NpcVoiceClips[size_t(e.speech)].role;if(role==1||role==3||role==4||role==6||role==8||role==11||role==13)return false;}
 return e.speech>=0||e.sound==Sound::WorkerDying||e.sound==Sound::Hurt||int(e.sound)>int(Sound::MusicClinic)||
 e.sound==Sound::BruteCall||e.sound==Sound::BruteAttack||e.sound==Sound::BruteDeath;}
Who* owner(Game& g,Rig& r,const SoundEvent& e){
 if(e.speech>=0&&e.speaker){for(auto& w:r.who)if(w.kind==0&&w.index==e.speaker-1)return &w;return nullptr;}
 Who* best=nullptr;float reach=1.3f*1.3f;for(auto& w:r.who){const Point3 p=where(g,w);const float d=(p.x-e.position.x)*(p.x-e.position.x)+(p.y-e.position.y)*(p.y-e.position.y);if(d<reach){reach=d;best=&w;}}
 return best;}
bool at(const Scene& s,float t){return s.frame==int(std::lround(t/s.dt));}

void stage(Scene& s,bool variety){
 Game& g=s.g();Rig& r=rig(s);r=Rig{};LabAccess::setGod(g,true);LabAccess::playerCamera(g);LabAccess::mute(g,true);
 Player& p=LabAccess::player(g);p.pos={4,11.5f};p.angle=0;p.pitch=-6;
 r.audio=std::make_unique<AudioEngine>(false);
 for(size_t i=0;i<g.npcActors().size();++i)if(g.npcActors()[i].track().ai.mode!=ActorAiMode::Scripted)r.who.push_back({"npc"+std::to_string(i),0,i});
 const auto& tracks=level0(g).actorTracks();for(size_t i=0;i<tracks.size();++i)if(tracks[i].ai.mode==ActorAiMode::Scripted)r.who.push_back({"scripted"+std::to_string(i),1,i});
 for(size_t i=0;i<LabAccess::enemies(g).size();++i)r.who.push_back({"creature"+std::to_string(i),2,i});
 if(!variety){g.setState(stateId("lab_dv_clock_s"),1);g.setState(stateId("lab_dv_clock_t"),1);g.setState(stateId("lab_dv_clock_radio"),1);}}

// Kill someone as play does: an NPC by a lethal hit, a scripted person or a creature by shotgun rounds.
void kill(Scene& s,Who& w){Game& g=s.g();
 if(w.kind==0){LabAccess::killNpc(g,w.index,LabAccess::player(g).pos);return;}
 if(w.kind==2){auto& e=LabAccess::enemies(g)[w.index];if(e.hp>2)LabAccess::damageEnemy(g,e,e.hp-1);}
 for(int round=0;round<4&&!dead(g,w);++round){const Point3 p=where(g,w);LabAccess::fireAt(g,{p.x,p.y,p.z+.9f});}}

void capture(Scene& s,bool variety,const std::vector<int>& order,float first,float every){
 Game& g=s.g();Rig& r=rig(s);
 for(size_t k=0;k<order.size();++k)if(at(s,first+every*float(k))&&size_t(order[k])<r.who.size()&&!dead(g,r.who[size_t(order[k])]))kill(s,r.who[size_t(order[k])]);
 if(!variety){
  // Keep shooting the bodies: a round every 0.3 s into one of the dead, in turn.
  if(s.frame%18==0){std::vector<Who*> bodies;for(auto& w:r.who)if(w.diedAt>=0)bodies.push_back(&w);
   if(!bodies.empty()){const Point3 p=where(g,*bodies[size_t(s.frame/18)%bodies.size()]);LabAccess::fireAt(g,{p.x,p.y,p.z+.3f});++r.shots;}}
  // Save and load in the middle of it.
  if(at(s,13.f)){const std::string save=LabAccess::encodeSave(g);r.loads+=LabAccess::decodeSave(g,save)?1:0;}}
 for(auto& w:r.who)if(w.diedAt<0&&dead(g,w))w.diedAt=s.time;
 for(const auto& e:g.sounds()){if(!vocal(e))continue;Who* w=owner(g,r,e);if(!w||w->diedAt<0)continue;
  if(w->diedAt==s.time){++w->deathEvents;const int take=e.speech>=0?e.speech:-1-int(e.sound);w->takes.push_back(take);r.deathOrder.push_back(take);}
  else{++w->after;r.log+=w->name+" at "+fixed(s.time)+" s: "+(e.speech>=0?"speech "+std::to_string(e.speech):"sound "+std::to_string(int(e.sound)))+"\n";}}
 // A line a dead man was still owed (a conversation's next take, a call queued as he fell): the mixer must not start it.
 if(!variety&&at(s,6.5f)){const auto& n=g.npcActors()[1];const int talk=clipFor(1,NpcVoiceCue::Talk,true);
  if(talk>=0)LabAccess::raise(g,{Sound::WorkerDying,n.pose.position,.95f,1,true,22,n.pose.z+1.4f,talk,2,3});}
 // What the mixer plays: no line of a dead man's but his death take, and that only as long as it lasts.
 r.audio->update(g,true);LabAccess::mix(*r.audio,r.pcm,size_t(std::lround(s.dt*44100)));
 for(const auto& p:LabAccess::playing(*r.audio)){if(p.speech>=0&&!p.speaker)++r.radioFrames;if(p.speech<0||!p.speaker||p.speaker>g.npcActors().size())continue;
  (void)0;
  for(const auto& w:r.who)if(w.kind==0&&w.index==p.speaker-1&&w.diedAt>=0){const auto& clip=NpcVoiceClips[size_t(p.speech)];
   if(clip.cue!=NpcVoiceCue::Down&&clip.cue!=NpcVoiceCue::LastGasp)++r.deadSpeech;
   else if(s.time-w.diedAt>clip.seconds/.94f+.3f)++r.lingering;}}}

void writeWav(const std::string& path,const std::vector<int16_t>& pcm){
 std::error_code ec;std::filesystem::create_directories(std::filesystem::path(path).parent_path(),ec);
 std::ofstream f(path,std::ios::binary);auto u32=[&](uint32_t v){f.write(reinterpret_cast<const char*>(&v),4);};auto u16=[&](uint16_t v){f.write(reinterpret_cast<const char*>(&v),2);};
 const uint32_t bytes=uint32_t(pcm.size()*2);f.write("RIFF",4);u32(36+bytes);f.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(44100);u32(44100*4);u16(4);u16(16);f.write("data",4);u32(bytes);
 f.write(reinterpret_cast<const char*>(pcm.data()),std::streamsize(bytes));}
std::string takeName(int take){if(take>=0){const auto& c=NpcVoiceClips[size_t(take)];return "role"+std::to_string(c.role)+(c.cue==NpcVoiceCue::LastGasp?"-lastgasp#":"-down#")+std::to_string(take);}
 return "sound#"+std::to_string(-1-take);}

// ------------------------------------------------------------------ the dead are quiet
// Order: Cleanup, worker, woman, scripted worker, transformed, Cleanup, Heavy (indices into Rig::who).
const std::vector<int> QuietOrder={0,3,4,5,6,1,2};
const bool quiet=add({
 .name="death-voices",.summary="nothing vocal from the dead: shot bodies, a scripted scream loop, a transformed man's moan loop, radio and dialogue cut, a reload",
 .fixtures={"death-room"},
 .author=[](Scene&,AuthoredMapData& m){
  m.actorTracks.clear();m.creatureSpawns.clear();
  m.actorTracks.push_back(person({9,10},ActorVisual::Cleanup,ActorAiMode::Guard,1,"g0"));
  m.actorTracks.push_back(person({9,13},ActorVisual::Cleanup,ActorAiMode::Guard,1,"g1"));
  m.actorTracks.push_back(person({11,11.5f},ActorVisual::CleanupHeavy,ActorAiMode::Guard,1,"g2"));
  m.actorTracks.push_back(person({8,8},ActorVisual::Worker,ActorAiMode::Civilian,0,"w"));
  m.actorTracks.push_back(person({8,15},ActorVisual::FemaleScientist,ActorAiMode::Civilian,0,"f"));
  // The scripted worker the Receiving encounter is made of: his timer screams and cries at him, looping.
  auto scripted=person({12,8},ActorVisual::Worker,ActorAiMode::Scripted,0,"s",30);scripted.timerState=stateId("lab_dv_clock_s");m.actorTracks.push_back(scripted);
  loopCue(m,"lab_dv_clock_s",4000,{{1000,Sound::WorkerDying},{3000,Sound::Hurt}},{12,8});
  // The Medical Ward's bed: a dormant transformed worker and the moan the ward's clock plays at it.
  m.creatureSpawns.push_back({CreatureKind::Transformed,{12,16.5f},-999,1,0,stateId("lab_dv_never")});
  loopCue(m,"lab_dv_clock_t",6000,{{2000,Sound::WorkerDying}},{12,16.5f});
  // A radio on the wall reading a voiced Chapter 2 line (role 1, keyed) every five seconds: it is nobody's body, so it keeps talking.
  loopCue(m,"lab_dv_clock_radio",5000,{{500,Sound::Empty}},{15,4},"dr_core_relay_say_900");},
 .setup=[](Scene& s){stage(s,false);},
 .capture=[](Scene& s,const Frame&){capture(s,false,QuietOrder,1.f,1.f);},
 .metrics=[](Scene& s,Report& r){
  Rig& q=rig(s);int died=0,cries=0,after=0,silentDeaths=0;
  for(const auto& w:q.who){if(w.diedAt<0)continue;++died;cries+=w.deathEvents;after+=w.after;silentDeaths+=w.deathEvents==0;r.metric("after."+w.name,w.after);r.metric("cries."+w.name,w.deathEvents);}
  const double window=s.time-(1.f+float(QuietOrder.size()-1));
  r.metric("died",died);r.metric("corpse_shots",q.shots);r.metric("loads",q.loads);r.metric("after_window_s",window,"s");
  r.metric("voice_events_after_death",after);r.metric("voice_events_per_s_after_death",window>0?after/window:0);r.metric("dead_speech_frames",q.deadSpeech);r.metric("lingering_frames",q.lingering);
  r.check("everyone in the tube dies",died==int(q.who.size()),std::to_string(died)+" of "+std::to_string(q.who.size()));
  r.check("each death makes its cry once",cries==died&&silentDeaths==0,std::to_string(cries)+" cries for "+std::to_string(died)+" deaths");
  r.check("nothing vocal from any body after it dies (shots, loops, reload; "+fixed(window,1)+" s after the last)",after==0,q.log.empty()?"0 events":q.log.substr(0,400));
  r.check("the mixer plays no dead man's line but his death take, and that only to its end",q.deadSpeech==0&&q.lingering==0,
   std::to_string(q.deadSpeech)+" frames of a dead man's other lines, "+std::to_string(q.lingering)+" frames past a death take's end");
  r.check("the save loads mid-scene",q.loads==1);
  r.metric("cue_voice_frames",q.radioFrames);
  r.check("a voiced cue (a radio's line, nobody's body) is heard",q.radioFrames>0,std::to_string(q.radioFrames)+" frames of speaker-less speech");},
 .teardown=[](Scene& s){rigs.erase(&s);}});

// ------------------------------------------------------------------ variety
// Six Cleanup men, a Heavy and a Rusher in one squad, a worker and a woman of the staff; then four transformed workers.
const std::vector<int> RowOrder={0,8,1,6,2,9,3,7,4,5,10,11,12,13};
const bool variety=add({
 .name="death-variety",.summary="ten people die two seconds apart, then four transformed workers: distinct cries, no take twice running, the mix as a WAV",
 .fixtures={"death-row"},
 .author=[](Scene&,AuthoredMapData& m){
  m.actorTracks.clear();m.creatureSpawns.clear();
  const Vec2 spots[]={{8,9},{8.5f,10.5f},{9,12},{8.5f,13.5f},{8,15},{9.5f,9.5f},{10,11},{10,12.5f},{7.5f,10},{7.5f,14}};
  for(int i=0;i<6;++i)m.actorTracks.push_back(person(spots[i],ActorVisual::Cleanup,ActorAiMode::Guard,1,"c"+std::to_string(i)));
  m.actorTracks.push_back(person(spots[6],ActorVisual::CleanupHeavy,ActorAiMode::Guard,1,"h"));
  m.actorTracks.push_back(person(spots[7],ActorVisual::CleanupRusher,ActorAiMode::Guard,1,"r"));
  m.actorTracks.push_back(person(spots[8],ActorVisual::Worker,ActorAiMode::Civilian,0,"w"));
  m.actorTracks.push_back(person(spots[9],ActorVisual::FemaleScientist,ActorAiMode::Civilian,0,"f"));
  for(int i=0;i<4;++i)m.creatureSpawns.push_back({CreatureKind::Transformed,{12.f,9.f+2.f*float(i)},-999,1,0,stateId("lab_dv_never")});},
 .setup=[](Scene& s){stage(s,true);},
 .capture=[](Scene& s,const Frame&){capture(s,true,RowOrder,1.f,2.f);},
 .metrics=[](Scene& s,Report& r){
  Rig& q=rig(s);std::vector<int> people,creatures;for(const auto& w:q.who)if(w.diedAt>=0&&!w.takes.empty())(w.kind==2?creatures:people).push_back(w.takes.front());
  std::set<int> distinct(people.begin(),people.end()),distinctCreatures(creatures.begin(),creatures.end());int repeats=0,lastGasps=0;std::string order;
  for(size_t i=0;i<q.deathOrder.size();++i){repeats+=i&&q.deathOrder[i]==q.deathOrder[i-1];order+=(i?", ":"")+takeName(q.deathOrder[i]);}
  for(int t:people)lastGasps+=t>=0&&NpcVoiceClips[size_t(t)].cue==NpcVoiceCue::LastGasp;
  r.metric("people_died",double(people.size()));r.metric("distinct_people_takes",double(distinct.size()));r.metric("last_gasps",lastGasps);
  r.metric("creatures_died",double(creatures.size()));r.metric("distinct_creature_takes",double(distinctCreatures.size()));r.metric("immediate_repeats",repeats);
  r.check("ten people die, each with a cry",people.size()==10,std::to_string(people.size())+" cries");
  r.check("at least five different death takes among the ten",distinct.size()>=5,std::to_string(distinct.size())+" distinct: "+order);
  r.check("no take twice running",repeats==0,std::to_string(repeats)+" repeats");
  r.check("some of the squad die with last words on the radio",lastGasps>0,std::to_string(lastGasps)+" last gasps");
  r.check("the transformed die as men (four, at least three groans, none the Brute's)",creatures.size()==4&&distinctCreatures.size()>=3&&!distinctCreatures.count(-1-int(Sound::BruteDeath)),
   std::to_string(creatures.size())+" deaths, "+std::to_string(distinctCreatures.size())+" groans");
  const std::string path=s.fixture->value("wav",".build/lab-out/death-mix.wav");writeWav(path,q.pcm);
  r.metric("wav_seconds",double(q.pcm.size()/2)/44100.,"s");r.check("the mix is written",std::filesystem::exists(path),path);},
 .teardown=[](Scene& s){rigs.erase(&s);}});
}
}

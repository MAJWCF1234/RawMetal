// What the player hears: the game's own audio engine mixed headless beside a scripted walk
// and a fight in campaign maps (the user on v1.4.5: "all your audio is muted, including
// character foley; character sounds are missing in the sample storage area; foley should
// be heard everywhere, the music track not as much").
//   audio-mix   walk (sprint, turning off walls), then a shotgun fight with a creature,
//               then walk again: every one-shot the game raises must reach the mixer,
//               and foley must sit clearly above the score.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>
#include <map>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Rig {
 std::unique_ptr<AudioEngine> audio;std::vector<int16_t> pcm;
 // The same game mixed three more times, one bus each: one-shots, score, beds (LabAccess::onlyBus).
 std::array<std::unique_ptr<AudioEngine>,3> bus;std::array<std::vector<int16_t>,3> busPcm;
 int npcSteps=0,npcStepsStarted=0,raised=0,started=0,footRaised=0,footStarted=0,creatureRaised=0,creatureStarted=0,gunRaised=0,gunStarted=0,speechRaised=0,speechStarted=0;
 size_t maxVoices=0,maxLoops=0,maxSilentLoops=0;double foleySum=0;int foleyCount=0;double musicSum=0;int musicFrames=0;double occSum=0,filterSum=0;int occCount=0,muffled=0,clearMuffled=0,clearCount=0;std::string clearWhy;Vec2 last{};int stuck=0;std::string why;
};
std::map<const Scene*,std::unique_ptr<Rig>> rigs;
Rig& rig(Scene& s){auto& r=rigs[&s];if(!r)r=std::make_unique<Rig>();return *r;}
bool creatureSound(Sound x){return x>=Sound::SpiderCall&&x<=Sound::BruteDeath;}
Enemy* foe(Game& g){Enemy* best=nullptr;float d=1e9f;for(auto& e:LabAccess::enemies(g)){if(!e.alive)continue;const float q=lengthSq(e.pos-g.player().pos);if(q<d){d=q;best=&e;}}return best;}

const bool mixTube=add({
 .name="audio-mix",.summary="a walk and a fight in campaign maps with the audio engine mixed headless: every one-shot reaches the mixer, foley sits above the score",
 .fixtures={"audio-m80","audio-m79","audio-m72","audio-people"},
 // audio-people: a worker and a scientist (civilians: they run from the Huntsman) and a scripted
 // worker walking his keys across the room. Their steps are character foley too.
 .author=[](Scene& s,AuthoredMapData& m){if(s.fixture->value("people")!="on")return;m.actorTracks.clear();
  auto person=[&](Vec2 at,ActorVisual visual,ActorAiMode mode,const std::string& tag){auto a=regression::guard(at,at,AiFaction::Neutral);a.visual=visual;a.ai.mode=mode;a.ai.patrol.clear();
   a.damageState=stateId("lab_am_damage_"+tag);a.deadState=stateId("lab_am_dead_"+tag);a.keys={{0,at,0,0},{60000,at,0,0}};return a;};
  m.actorTracks.push_back(person({10,12},ActorVisual::Worker,ActorAiMode::Civilian,"w"));
  m.actorTracks.push_back(person({10,9},ActorVisual::FemaleScientist,ActorAiMode::Civilian,"f"));
  {TimedSequence q;q.timerState=stateId("lab_am_walk");q.finishState=stateId("lab_am_walk_done");q.durationMs=60000;q.finishAtMs=60000;q.soundIntervalMs=0;q.soundUntilMs=0;m.timedSequences.push_back(q);}
  auto walker=person({5,15},ActorVisual::Scientist,ActorAiMode::Scripted,"s");walker.timerState=stateId("lab_am_walk");
  walker.keys={{0,{5,15},0,kPi*.5f,1,0},{8000,{17,15},0,kPi*.5f,1,9.f},{8100,{17,15},0,kPi*.5f,0,0},{60000,{17,15},0,kPi*.5f,0,0}};m.actorTracks.push_back(walker);},
 .setup=[](Scene& s){Game& g=s.g();Rig& r=rig(s);r=Rig{};LabAccess::playerCamera(g);LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Shotgun);
  g.setState(stateId("lab_am_walk"),1);
  r.audio=std::make_unique<AudioEngine>(false);for(auto& b:r.bus)b=std::make_unique<AudioEngine>(false);r.last=g.player().pos;},
 .step=[](Scene& s,float){Game& g=s.g();Rig& r=rig(s);Player& p=LabAccess::player(g);
  const bool fight=s.time>=3.f&&s.time<7.f;
  if(fight){if(Enemy* e=foe(g)){p.angle=std::atan2(e->pos.y-p.pos.y,e->pos.x-p.pos.x);p.pitch=-8;}s.input.fire=s.frame%45==0;s.input.forward=s.frame%90<30;}
  else{s.input.forward=true;s.input.sprint=true;
   // Off a wall: turn a quarter.
   if(lengthSq(p.pos-r.last)<.0004f){if(++r.stuck>6){p.angle+=kPi*.5f;r.stuck=0;}}else r.stuck=0;}
  r.last=p.pos;},
 .capture=[](Scene& s,const Frame&){Game& g=s.g();Rig& r=rig(s);
  r.audio->update(g,true);
  // Why a sound across the room is walled off: the first point of the straight ray from the ear to the creature that the world calls solid.
  if(r.why.empty()&&s.time>4.f){if(Enemy* e=foe(g)){const Player& p=g.player();const World& w=g.world();const Point3 a{p.pos.x,p.pos.y,p.z+p.eye},b{e->pos.x,e->pos.y,(e->bodyBottom()+e->bodyTop())*.5f};
   const bool clear=g.soundRayClear({a.x,a.y},a.z,{b.x,b.y},b.z,.25f,.25f);r.why=std::string("ray ")+(clear?"clear":"blocked")+" water volumes "+std::to_string(w.waterVolumes().size())+" ear "+fixed(a.x)+","+fixed(a.y)+","+fixed(a.z)+" src "+fixed(b.x)+","+fixed(b.y)+","+fixed(b.z);
   for(int i=1;i<60;++i){const float t=float(i)/60;const float x=a.x+(b.x-a.x)*t,y=a.y+(b.y-a.y)*t,z=a.z+(b.z-a.z)*t;
    if(!w.fits(x,y,z,.015f,false,true)){r.why+=" | first solid at "+fixed(x)+","+fixed(y)+","+fixed(z)+" support "+fixed(w.supportHeight(x,y,false,true))+" clearance "+fixed(w.clearanceHeight(x,y,z))+" floor "+fixed(w.floorHeight(x,y));break;}
    if(w.doorBlocks(x,y,z,.015f,false)){r.why+=" | door at "+fixed(x)+","+fixed(y);break;}}}}
  const auto held=LabAccess::held(*r.audio);size_t loops=0,silent=0;double music=0;
  for(const auto& h:held){if(h.loop&&!h.music){++loops;silent+=h.level<=0;}if(h.music)music+=h.level;}
  r.maxVoices=std::max(r.maxVoices,held.size());r.maxLoops=std::max(r.maxLoops,loops);r.maxSilentLoops=std::max(r.maxSilentLoops,silent);r.musicSum+=music;++r.musicFrames;
  // Each one-shot the game raised this frame against the fresh voices the mixer now holds.
  std::map<std::pair<int,int>,int> fresh;for(const auto& h:held)if(h.fresh&&!h.loop){++fresh[{int(h.sound),h.speech}];if(!h.music&&h.speech<0){r.foleySum+=h.level;++r.foleyCount;}}
  // Every spatial one-shot sounding: how much the walls take (occlusion 1 open, .32 behind three walls; filter 1 full band).
  for(const auto& h:held)if(h.spatial&&!h.loop&&!h.music&&h.level>0){r.occSum+=h.occlusion;r.filterSum+=h.filter;++r.occCount;r.muffled+=h.filter<.5f;
   // In plain view of the ear (the straight ray clear) a sound must not come through as if behind walls.
   const Player& p=g.player();const float ez=p.z+p.eye,sz=std::isfinite(h.z)?h.z:ez;if(length(h.at-p.pos)>.6f&&g.soundRayClear(p.pos,ez,h.at,sz,.25f,.25f)){++r.clearCount;if(h.occlusion<.75f){++r.clearMuffled;if(r.clearWhy.size()<300)r.clearWhy+="sound "+std::to_string(int(h.sound))+" at "+fixed(h.at.x)+","+fixed(h.at.y)+","+fixed(sz)+" occ "+fixed(h.occlusion)+"; ";}}}
  for(const auto& e:g.sounds()){if(e.gain<=0)continue;auto& left=fresh[{int(e.sound),e.speech}];const bool ok=left>0;if(ok)--left;
   ++r.raised;r.started+=ok;
   if(footstepSound(e.sound)&&e.spatial){++r.npcSteps;r.npcStepsStarted+=ok;}
   if(footstepSound(e.sound)){++r.footRaised;r.footStarted+=ok;}
   else if(creatureSound(e.sound)){++r.creatureRaised;r.creatureStarted+=ok;}
   else if(gunshotSound(e.sound)){++r.gunRaised;r.gunStarted+=ok;}
   if(e.speech>=0){++r.speechRaised;r.speechStarted+=ok;}}
  LabAccess::mix(*r.audio,r.pcm,size_t(std::lround(s.dt*44100)));
  for(int b=0;b<3;++b){r.bus[size_t(b)]->update(g,true);LabAccess::onlyBus(*r.bus[size_t(b)],b);LabAccess::mix(*r.bus[size_t(b)],r.busPcm[size_t(b)],size_t(std::lround(s.dt*44100)));}},
 .metrics=[](Scene& s,Report& rep){Rig& r=rig(s);
  auto rmsOf=[](const std::vector<int16_t>& p){double a=0;for(auto v:p)a+=double(v)*v;return p.empty()?0.:std::sqrt(a/double(p.size()));};
  const double rms=rmsOf(r.pcm),scoreRms=rmsOf(r.busPcm[1]),bedRms=rmsOf(r.busPcm[2]);
  // One-shots: 50 ms windows of their bus; the loud ones (a step, a shot as it lands) at the 90th percentile of those that sound at all.
  std::vector<double> windows;const auto& o=r.busPcm[0];for(size_t at=0;at+4410<=o.size();at+=4410){double a=0;for(size_t k=at;k<at+4410;++k)a+=double(o[k])*o[k];a=std::sqrt(a/4410);if(a>30)windows.push_back(a);}
  std::sort(windows.begin(),windows.end());const double shotP90=windows.empty()?0:windows[windows.size()*9/10],shotP50=windows.empty()?0:windows[windows.size()/2];
  rep.metric("oneshot_window_rms_p50",shotP50);rep.metric("oneshot_window_rms_p90",shotP90);rep.metric("score_rms",scoreRms);rep.metric("beds_rms",bedRms);
  rep.check("diagnostic: the ear-to-creature ray",true,r.why);
  rep.metric("in_view_frames",r.clearCount);rep.metric("in_view_muffled_frames",r.clearMuffled);
  rep.check("a one-shot in plain view of the ear is not muffled as if behind a wall",r.clearMuffled*20<=std::max(1,r.clearCount),std::to_string(r.clearMuffled)+" of "+std::to_string(r.clearCount)+" frames: "+r.clearWhy);
  rep.metric("spatial_oneshot_frames",r.occCount);rep.metric("spatial_occlusion_mean",r.occCount?r.occSum/r.occCount:1);rep.metric("spatial_filter_mean",r.occCount?r.filterSum/r.occCount:1);rep.metric("spatial_muffled_share",r.occCount?double(r.muffled)/r.occCount:0);
  rep.metric("oneshot_p50_over_score",scoreRms>0?shotP50/scoreRms:99);
  rep.check("foley clearly over the score: one-shot windows (median) at least 2x the score's RMS",shotP50>=2*scoreRms,fixed(shotP50,0)+" vs score "+fixed(scoreRms,0)+" (beds "+fixed(bedRms,0)+")");
  const double foley=r.foleyCount?r.foleySum/r.foleyCount:0,music=r.musicFrames?r.musicSum/r.musicFrames:0;
  rep.metric("raised",r.raised);rep.metric("started",r.started);rep.metric("dropped",r.raised-r.started);
  rep.metric("footsteps_raised",r.footRaised);rep.metric("footsteps_started",r.footStarted);
  rep.metric("gunshots_raised",r.gunRaised);rep.metric("gunshots_started",r.gunStarted);
  rep.metric("creature_raised",r.creatureRaised);rep.metric("creature_started",r.creatureStarted);
  rep.metric("speech_raised",r.speechRaised);rep.metric("speech_started",r.speechStarted);
  rep.metric("max_voices",double(r.maxVoices));rep.metric("max_ambient_loops",double(r.maxLoops));rep.metric("max_silent_loops",double(r.maxSilentLoops));
  rep.metric("foley_level_mean",foley);rep.metric("music_level_mean",music);rep.metric("foley_over_music",music>0?foley/music:99);rep.metric("mix_rms",rms);
  rep.metric("people_steps_raised",r.npcSteps);rep.metric("people_steps_started",r.npcStepsStarted);
  if(s.fixture->value("people")=="on")rep.check("the people walking are heard walking (civilians running, a scripted walker)",r.npcSteps>=8&&r.npcStepsStarted==r.npcSteps,std::to_string(r.npcStepsStarted)+" of "+std::to_string(r.npcSteps)+" people's steps mixed");
  rep.check("the walk makes footsteps",r.footRaised>=6,std::to_string(r.footRaised));
  rep.check("the fight fires the shotgun",r.gunRaised>=3,std::to_string(r.gunRaised));
  rep.check("every one-shot the game raises reaches the mixer",r.started==r.raised,std::to_string(r.started)+" of "+std::to_string(r.raised)+" (footsteps "+std::to_string(r.footStarted)+"/"+std::to_string(r.footRaised)+
   ", gun "+std::to_string(r.gunStarted)+"/"+std::to_string(r.gunRaised)+", creature "+std::to_string(r.creatureStarted)+"/"+std::to_string(r.creatureRaised)+", speech "+std::to_string(r.speechStarted)+"/"+std::to_string(r.speechRaised)+"); loops held up to "+std::to_string(r.maxLoops));
  rep.check("the mix is audible",rms>200,fixed(rms,0)+" rms");},
 .teardown=[](Scene& s){rigs.erase(&s);}});
}
}

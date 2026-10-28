#include "../../../Game.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
// The alarm through the real Game update: the radio call a guard commits to,
// the ALERT that holds a moment past cover, EVASION's clock running down into
// Caution, the floods held through the hunt, what the player hears of being
// seen, the card over his head, and a body radioed in.
bool Game::testAlarm(){
 std::ofstream report("alarm-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const std::string&text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 struct Post{Vec2 at;float yaw;const char*squad;};
 // CautionTest's open room: a wall to hide behind in the far corner, unarmed guards at posts.
 auto scene=[](const std::vector<Post>&posts,bool armed=false){
  auto map=std::make_shared<AuthoredMapData>();map->name="Alarm room";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);map->structures.push_back({12,14,24,14.2f,0,8});
  TimedSequence timer;timer.timerState=stateId("alarm_clock");timer.finishState=stateId("alarm_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  for(size_t i=0;i<posts.size();++i){const auto&post=posts[i];ActorTrack t;t.timerState=timer.timerState;t.keys={{0,post.at,0,post.yaw},{30000,post.at,0,post.yaw}};t.scale=1.6f;t.health=70;
   t.damageState=stateId("alarm_damage_"+std::to_string(i));t.deadState=stateId("alarm_dead_"+std::to_string(i));t.ai.mode=ActorAiMode::Guard;t.ai.alertState=stateId("alarm_flag");
   t.ai.squad=post.squad?stateId(post.squad):0;t.ai.armed=armed;t.ai.patrol={{post.at,0,1000}};map->actorTracks.push_back(t);}
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Alarm validation";campaign->key=731911;campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int frames){for(int i=0;i<frames;++i)g.update({},1.f/60);};
 auto heard=[](const Game&g,Sound s){return int(std::count_if(g.m_sounds.begin(),g.m_sounds.end(),[&](const SoundEvent&e){return e.sound==s;}));};
 auto get=[](const Game&g,size_t i,const char*f){return g.state(g.friendlyKey(i,f));};
 const int callFrames=RadioCallMs*60/1000;
 // Guard 0 faces the player's way in; his squadmate faces a wall far off; guard 2 is another squad.
 const std::vector<Post> yard={{{6.5f,6.5f},kPi*.5f,"alarm_squad"},{{2.5f,20.5f},-kPi*.5f,"alarm_squad"},{{20.5f,2.5f},kPi*.5f,"alarm_other"}};
 const Vec2 near{8.5f,6.5f},hide{18.5f,18.5f};
 // Making him out: the '?' fills with his recognition, a tick as it first goes up, the sting on the '!'.
 {auto g=scene(yard);g.m_player.pos=near;int ticks=0,stings=0,frames=0;bool tracked=true,rising=true;float last=0;const int need=g.npcActors()[0].track().ai.recognitionMs;
  while(frames<600&&!get(g,0,"guard_confirmed")){tick(g,1);++frames;ticks+=heard(g,Sound::SuspicionTick);stings+=heard(g,Sound::DetectSting);
   auto card=g.guardCard(0);if(card.kind==GuardCardKind::Making){float want=std::max(.05f,get(g,0,"guard_recognition")/float(need));tracked&=std::fabs(card.fill-want)<.001f&&std::fabs(g.guardAwareness(0)-want)<.001f;rising&=card.fill>=last;last=card.fill;}}
  auto card=g.guardCard(0);
  check(frames>2&&ticks==1&&tracked&&rising&&last>.5f,"The '?' fills with his recognition over "+std::to_string(frames)+" frames (last "+std::to_string(last)+"), one rising tick as it first goes up");
  check(card.kind==GuardCardKind::Alert&&g.guardAwareness(0)==2&&stings==1&&card.bangMs<=20,"The '!' goes up with one detection sting, its pop just begun ("+std::to_string(card.bangMs)+" ms)");
  tick(g,4);int pop=g.guardCard(0).bangMs;tick(g,4);check(pop>50&&pop<100&&g.guardCard(0).bangMs>=100,"The '!' pops over its first ~6 frames ("+std::to_string(pop)+" ms after 5)");
  // The committed call: begun on the '!', it goes on when the player breaks his line.
  bool calling=g.guardCalling(0)&&g.guardCard(0).call>0&&!g.state("alarm_flag");
  g.m_player.pos=hide;int squelch=0,alerted=-1,stingsAfter=0;
  for(int f=0;f<callFrames+10;++f){tick(g,1);squelch+=heard(g,Sound::RadioSquelch);stingsAfter+=heard(g,Sound::DetectSting);if(alerted<0&&g.state("alarm_flag"))alerted=f;}
  check(calling&&g.guardState(0)!=GuardState::Alert&&!g.guardCalling(0)&&get(g,0,"guard_radio")==RadioCallMs&&alerted>=callFrames-14&&squelch==1,"His radio call survives the player breaking his line and lands "+std::to_string(alerted)+" frames on, with the squelch as it does");
  check(g.state("alarm_flag")==1,"The alarm flag (floods, klaxon) goes up only when the call lands");
  // ALERT holds ~2 s past the last sighting, then EVASION with its clock.
  auto latch=g.guardAlarm();int frames2=0;while(frames2<180&&g.guardAlarm()==AlarmAlert){tick(g,1);++frames2;}
  float held=(frames2+callFrames+10)/60.f;
  check(latch==AlarmAlert&&g.guardAlarm()==AlarmEvasion&&std::fabs(held-AlertLatchMs/1000.f)<.1f,"ALERT holds "+std::to_string(held)+" s past the player leaving his sight, then reads EVASION");
  int clock=g.guardEvasion(),lowest=clock,steps=0;bool down=true,flood=true,hunted=true;
  while(steps<60*30&&g.guardAlarm()==AlarmEvasion){tick(g,1);++steps;int now=g.guardEvasion();down&=now<=lowest||now==0;lowest=std::min(lowest,now?now:lowest);const bool still=g.guardAlarm()==AlarmEvasion;flood&=!still||g.state("alarm_flag")==1;hunted&=!still||g.guardCard(0).kind==GuardCardKind::Hunting;stingsAfter+=heard(g,Sound::DetectSting);}
  // A confirmed hunt is searched at least HuntSearchMs, so EVASION is never a blink (and never a
  // HUD-only number); the ALERT latch is the first two seconds of that same hunt.
  check(clock>=HuntSearchMs&&clock<=std::max(g.npcActors()[0].track().ai.searchMs,HuntSearchMs+AlertLatchMs)+8000&&down&&std::abs(steps*1000/60-clock)<1200,"EVASION counts down from "+std::to_string(clock)+" ms (at least 20 s, from where the HUD starts it) and runs out in "+std::to_string(steps*1000/60)+" ms");
  check(flood&&hunted,"Through the hunt the alarm flag stays up and he wears the red '?'");
  check(g.guardAlarm()>0&&g.guardAlarm()==g.guardCaution(0)&&g.guardCaution(0)>CautionMs-200&&!g.state("alarm_flag")&&g.guardState(0)==GuardState::Patrol,"When the hunt gives up the HUD reads CAUTION and the alarm flag drops");
  check(stingsAfter==0,"No second sting through the hunt");
  // A new detection is a new sting.
  // (In front of him, wherever his long search left him facing: he searched where he last had the player.)
  {const auto&p=g.npcActors()[0].pose;g.m_player.pos=p.position+Vec2{std::sin(p.yaw),std::cos(p.yaw)}*2.5f;}int again=0;for(int f=0;f<120;++f){tick(g,1);again+=heard(g,Sound::DetectSting);}
  check(again==1&&g.guardAlarm()==AlarmAlert,"Seen again after the hunt, the sting sounds once more");
 }
 // Line-of-sight flicker: in and out of cover, one sting, one call, no strobing HUD.
 {auto g=scene(yard);g.m_player.pos=near;int stings=0,squelch=0,frames=0;bool steady=true;
  while(frames<600&&!get(g,0,"guard_confirmed")){tick(g,1);++frames;stings+=heard(g,Sound::DetectSting);}
  for(int f=0;f<150;++f){g.m_player.pos=(f/10)%2?near:hide;tick(g,1);stings+=heard(g,Sound::DetectSting);squelch+=heard(g,Sound::RadioSquelch);steady&=g.guardAlarm()==AlarmAlert;}
  check(stings==1&&squelch<=2&&steady,"Ducking in and out of his sight: one sting, one call, ALERT steady ("+std::to_string(stings)+" stings, "+std::to_string(squelch)+" squelches)");
 }
 // A choke mid-call: the call dies with him and nothing is raised.
 {auto g=scene(yard);g.m_player.pos=near;int frames=0;while(frames<600&&!get(g,0,"guard_confirmed")){tick(g,1);++frames;}
  g.m_player.pos=hide;tick(g,callFrames/2);bool half=g.guardCalling(0);
  g.setState(g.friendlyKey(0,"restraint"),1);tick(g,callFrames);
  check(half&&!g.guardCalling(0)&&!get(g,0,"guard_radio")&&!g.state("alarm_flag")&&g.guardAlarm()==0&&g.guardCard(0).kind==GuardCardKind::None,"Choked mid-call, his call dies with him: no alarm flag, no ALERT or EVASION");
 }
 // A hit mid-call cuts it: he starts over while he still has the player.
 {auto g=scene(yard);g.m_player.pos=near;int frames=0;while(frames<600&&!get(g,0,"guard_confirmed")){tick(g,1);++frames;}
  tick(g,callFrames/2);int before=get(g,0,"guard_radio");g.damageFriendly(0,5,near,{ShotKind::Player,g.level(),0});tick(g,1);int after=get(g,0,"guard_radio");
  check(before>RadioCallMs/3&&after<=40&&g.guardCalling(0),"A hit cuts his call short ("+std::to_string(before)+" ms in); with the player still in sight he starts over");
 }
 // A body: the man who sees his squadmate fall radios it in, and the squad goes to Caution when it lands.
 const std::vector<Post> pair={{{6.5f,6.5f},kPi*.5f,"alarm_squad"},{{9.5f,6.5f},kPi*.5f,"alarm_squad"},{{20.5f,2.5f},kPi*.5f,"alarm_other"}};
 for(bool dropped:{false,true}){auto g=scene(pair);g.m_player.pos=hide;tick(g,2);
  g.damageFriendly(1,1000,{9.5f,6.5f});int squelch=heard(g,Sound::RadioSquelch);bool calling=g.guardCalling(0)&&get(g,0,"guard_call")==CallBody;
  if(dropped){tick(g,callFrames/2);g.setState(g.friendlyKey(0,"restraint"),1);}
  tick(g,callFrames+4);
  if(!dropped)check(calling&&squelch==1&&g.guardCaution(0)>CautionMs-500&&g.guardAlarm()==g.guardCaution(0)&&!g.state("alarm_flag")&&!g.guardCaution(2),"Seeing his squadmate fall he radios it in; when it lands his squad is on Caution and the HUD reads it ("+std::to_string(g.guardCaution(0))+" ms)");
  else check(calling&&!g.guardCaution(0)&&g.guardAlarm()==0&&!get(g,0,"guard_radio"),"Knocked out before the body report lands, it never does: no Caution");
 }
 // The card over a man checking a noise, and its fill read straight from recognition.
 {auto g=scene(yard);g.m_player.pos=hide;const int need=g.npcActors()[0].track().ai.recognitionMs;
  g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Suspicious));g.setState(g.friendlyKey(0,"guard_recognition"),need/2);auto half=g.guardCard(0);
  g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Search));g.setState(g.friendlyKey(0,"guard_recognition"),0);auto noise=g.guardCard(0);
  check(half.kind==GuardCardKind::Making&&std::fabs(half.fill-.5f)<.01f&&noise.kind==GuardCardKind::Checking&&g.guardAwareness(0)==.5f,"Half made out the '?' is half full; checking a noise it is the dim one");
 }
 // A noise heard at peace: he stops dead, and only after a beat turns to it with his '?' up.
 {auto g=scene(yard);g.m_player.pos=hide;tick(g,2);g.m_sounds.clear();g.m_sounds.push_back({Sound::JunkMetal,{8.5f,6.5f},1,1,true,8,.5f});g.updateFriendlyActors(1.f/60,true);g.m_sounds.clear();
  int card=-1;for(int f=1;f<60&&card<0;++f){if(g.guardCard(0).kind!=GuardCardKind::None)card=f;tick(g,1);}
  check(card>=12&&card<=18,"A noise heard at peace: his '?' goes up "+std::to_string(card*1000/60)+" ms after it, a beat, not on the next frame");
 }
 auto ms=[](int frames){return std::to_string(frames*1000/60);};
 auto clearShot=[](Game&g,size_t i){const auto&n=g.npcActors()[i];auto t=g.guardTarget(i);return g.guardState(i)==GuardState::Alert&&t.valid()&&g.guardCanSee(i,n.pose,t)&&g.guardShotMatches(i,g.guardShot(i,n.pose,g.npcWeaponPoseAt(i,n.pose,{t.position.x,t.position.y,t.feet+t.height*.6f},1)));};
 // ALERT means shots. Two armed men of one squad (the second its flanker) have
 // the player in the open 8 m off (inside the 9 m an armed man shoots at): each
 // fires within 3 s of his '!', and again within 3 s of a clear shot after the
 // player sprints out of reach and back.
 {auto g=scene({{{6.5f,4.5f},0,"alarm_armed"},{{8.5f,4.5f},0,"alarm_armed"}},true);
  g.m_player.pos={7.5f,12.5f};int seen[2]={-1,-1},fired[2]={-1,-1},flank[2]={},frame=0;
  for(;frame<60*12&&(fired[0]<0||fired[1]<0);++frame){tick(g,1);g.m_player.health=1e6f;for(size_t i=0;i<2;++i){flank[i]|=g.guardFlankSide(i);if(seen[i]<0&&get(g,i,"guard_confirmed"))seen[i]=frame;if(seen[i]>=0&&fired[i]<0&&get(g,i,"weapon_flash")>0)fired[i]=frame;}}
  g.m_player.pos=hide;tick(g,90);g.m_player.pos={13.5f,10.5f};int clear[2]={-1,-1},back[2]={-1,-1};
  for(frame=0;frame<60*12&&(back[0]<0||back[1]<0);++frame){tick(g,1);g.m_player.health=1e6f;for(size_t i=0;i<2;++i){if(clear[i]<0&&clearShot(g,i))clear[i]=frame;if(clear[i]>=0&&back[i]<0&&get(g,i,"weapon_flash")>0)back[i]=frame-clear[i];}}
  report<<"  armed pair (flank sides "<<flank[0]<<","<<flank[1]<<"): '!' at frames "<<seen[0]<<","<<seen[1]<<", first shots "<<fired[0]<<","<<fired[1]<<"; back in reach, clear from frames "<<clear[0]<<","<<clear[1]<<", shots "<<back[0]<<","<<back[1]<<" frames after\n";
  bool quick=true;for(int i=0;i<2;++i)quick&=seen[i]>=0&&fired[i]>=0&&fired[i]-seen[i]<=180&&back[i]>=0&&back[i]<=180;
  check(quick&&(flank[0]||flank[1]),"On ALERT with a clear shot at 10 m each man fires within 3 s, the flanker too ("+ms(fired[0]-seen[0])+", "+ms(fired[1]-seen[1])+" ms), and again within 3 s of a clear shot after the player sprints out of reach ("+ms(back[0])+", "+ms(back[1])+" ms)");
 }
 // The review's run in the campaign's yard (map 58): seen from the gate, the
 // player sprints back 50 frames the moment ALERT goes up, then stands. Every
 // man with the player in sight and a clear line fires within 3 s of having it,
 // and the player is hit within 4 s of the sprint (it was 22 s: the man on
 // ALERT walked in place against the fence, his Move never letting him raise).
 for(bool wait:{true,false}){auto g=mapInspection({16.5f,11.8f},200*kPi/180,-4,58,true,-999,true);g.stowWeapon();int alert=-1;
  for(int f=0;f<60*10&&alert<0;++f){g.update({},1.f/60);g.m_player.health=1e6f;if(g.guardAlarm()==AlarmAlert)alert=f;}
  InputState run{};run.sprint=true;run.back=true;for(int f=0;f<50&&!wait;++f){g.update(run,1.f/60);g.m_player.health=1e6f;}
  std::vector<int> clear(g.npcActors().size(),-1),shot(g.npcActors().size(),-1);int hit=-1;
  for(int f=0;f<60*10;++f){g.update({},1.f/60);if(hit<0&&g.m_player.health<1e6f)hit=f;g.m_player.health=1e6f;
   for(size_t i=0;i<g.npcActors().size();++i){if(g.npcActors()[i].level!=g.level())continue;if(clear[i]<0&&clearShot(g,i))clear[i]=f;if(clear[i]>=0&&shot[i]<0&&get(g,i,"weapon_flash")>0)shot[i]=f;}}
  int men=0,worst=0;for(size_t i=0;i<clear.size();++i)if(clear[i]>=0){++men;worst=std::max(worst,shot[i]<0?99999:shot[i]-clear[i]);report<<"  yard ("<<(wait?"standing":"after the sprint")<<"): guard "<<i<<" clear from frame "<<clear[i]<<", fires on "<<shot[i]<<"\n";}
  report<<"  yard: ALERT on frame "<<alert<<", the player first hit "<<hit<<" frames "<<(wait?"after it":"after the sprint")<<"\n";
  check(alert>=0&&men>0&&worst<=180&&hit>=0&&hit<=240,std::string("In the yard (map 58), ")+(wait?"standing in his sight":"after sprinting back out of reach at ALERT")+", every man with a clear line fires within 3 s of it (at worst "+ms(worst)+" ms) and the player is hit within 4 s ("+ms(hit)+" ms)");
 }
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

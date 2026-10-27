#include "../../../Game.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
// Caution through the real Game update: a guard who had the player and lost
// him puts his squad on a 45 s Caution when his search gives up.
bool Game::testCaution(){
 std::ofstream report("caution-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const std::string&text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 struct Post{Vec2 at;float yaw;const char*squad;std::vector<ActorPatrolPoint> beat;};
 // An open room, a wall to hide behind in the far corner, unarmed guards at posts.
 auto room=[](const std::vector<Post>&posts,float sight){
  auto map=std::make_shared<AuthoredMapData>();map->name="Caution room";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);map->structures.push_back({12,14,24,14.2f,0,8});
  TimedSequence timer;timer.timerState=stateId("caution_clock");timer.finishState=stateId("caution_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  for(size_t i=0;i<posts.size();++i){const auto&post=posts[i];ActorTrack t;t.timerState=timer.timerState;t.keys={{0,post.at,0,post.yaw},{30000,post.at,0,post.yaw}};t.scale=1.6f;t.health=70;
   t.damageState=stateId("caution_damage_"+std::to_string(i));t.deadState=stateId("caution_dead_"+std::to_string(i));t.ai.mode=ActorAiMode::Guard;t.ai.alertState=stateId("caution_alert");t.ai.sightRange=sight;
   t.ai.squad=post.squad?stateId(post.squad):0;t.ai.patrol=post.beat.empty()?std::vector<ActorPatrolPoint>{{post.at,0,1000}}:post.beat;map->actorTracks.push_back(t);}
  return map;
 };
 auto scene=[&](const std::vector<Post>&posts,float sight=11){auto campaign=std::make_shared<CustomCampaign>();campaign->name="Caution validation";campaign->key=731905;campaign->maps.push_back(room(posts,sight));return Game(campaign);};
 auto tick=[](Game&g,int frames){for(int i=0;i<frames;++i)g.update({},1.f/60);};
 // Frames until guard 0 has made the player out, from where they stand now.
 auto confirm=[&](Game&g){int frames=0;while(frames<600&&!g.state(g.friendlyKey(0,"guard_confirmed"))){tick(g,1);++frames;}return frames;};
 auto searchOut=[&](Game&g){int frames=0;while(frames<60*30&&!g.guardCaution(0)){tick(g,1);++frames;}return frames;};
 // Guard 0 faces the player's way in; his squadmate and a man of another squad face walls.
 const std::vector<Post> yard={{{6.5f,6.5f},kPi*.5f,"caution_squad",{}},{{2.5f,20.5f},-kPi*.5f,"caution_squad",{}},{{20.5f,2.5f},kPi*.5f,"caution_other",{}}};
 const Vec2 hide{18.5f,18.5f};
 {auto g=scene(yard);g.m_player.pos={8.5f,6.5f};confirm(g);
  check(g.guardState(0)==GuardState::Alert&&g.state("caution_alert")==1&&g.guardAlarm()==-1&&!g.guardCaution(0),"A confirmed sighting is an Alert: the alarm flag is up, the HUD reads ALERT, no Caution yet");
  g.m_player.pos=hide;int frames=searchOut(g);int caution=g.guardCaution(0);
  check(caution>=CautionMs-40&&caution<=CautionMs&&frames>60*4&&g.guardState(0)==GuardState::Patrol&&!g.state(g.friendlyKey(0,"guard_confirmed")),"Hiding until his search gives up puts him on about 45 s of Caution as he goes back to patrol ("+std::to_string(caution)+" ms after "+std::to_string(frames)+" frames)");
  check(g.guardCaution(1)==caution&&!g.guardCaution(2)&&g.guardState(1)==GuardState::Patrol,"Caution is the squad's: his squadmate shares the clock, another squad does not");
  check(g.guardAlarm()==caution&&!g.state("caution_alert"),"The HUD reads CAUTION and the alarm flag is down");
  Game loaded(g.m_customCampaign);check(loaded.decodeSave(g.encodeSave())&&loaded.guardCaution(0)==caution&&loaded.guardCaution(1)==caution&&!loaded.guardCaution(2),"Caution survives save/load");
  auto key=g.guardCautionKey(0);g.setState(key,CautionMs+5000);Game longer(g.m_customCampaign);bool cut=longer.decodeSave(g.encodeSave())&&longer.guardCaution(0)==CautionMs&&longer.guardCaution(1)==CautionMs;
  g.setState(key,-1);Game negative(g.m_customCampaign);bool refused=!negative.decodeSave(g.encodeSave());g.setState(key,600001);Game huge(g.m_customCampaign);refused&=!huge.decodeSave(g.encodeSave());g.setState(key,caution);
  check(cut&&refused,"A saved Caution longer than this build's clock loads cut to 45 s; a negative or absurd one is refused");
  tick(g,60*30);int later=g.guardCaution(0);check(std::abs(caution-later-30000)<=20&&g.guardCaution(1)==later,"Caution counts down in real time ("+std::to_string(caution-later)+" ms in 30 s)");
  int left=0;while(left<60*20&&g.guardCaution(0)){tick(g,1);++left;}float total=(CautionMs-caution)*.001f+30+left/60.f;
  check(!g.guardCaution(0)&&!g.guardCaution(1)&&std::fabs(total-45)<.1f&&g.guardAlarm()==0,"Caution runs out after 45 s and the HUD line goes ("+std::to_string(total)+" s)");
 }
 // Only losing the player he had starts it: a search after a glimpse he never
 // made out, or after a fight that was not with the player, gives up quietly.
 for(int sure:{0,1}){auto g=scene(yard);g.m_player.pos=hide;const auto&config=g.npcActors()[0].track().ai;int kind=int(sure?ShotKind::Npc:ShotKind::Player);
  g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Search));g.setState(g.friendlyKey(0,"guard_search"),config.searchMs);g.setState(g.friendlyKey(0,"guard_bearing"),int(std::lround(std::remainder(g.npcActors()[0].pose.yaw,2*kPi)*10000)));
  g.setState(g.friendlyKey(0,"guard_confirmed"),sure);g.setState(g.friendlyKey(0,"guard_recognition"),sure?config.recognitionMs:config.recognitionMs/2);g.setState(g.friendlyKey(0,"guard_target_kind"),kind);g.setState(g.friendlyKey(0,"guard_target_level"),1);g.setState(g.friendlyKey(0,"guard_target_index"),sure?3:1);
  int held=-1,confirmed=-1,frames=0;for(;frames<60*30&&g.guardState(0)==GuardState::Search;++frames){held=g.state(g.friendlyKey(0,"guard_target_kind"));confirmed=g.state(g.friendlyKey(0,"guard_confirmed"));tick(g,1);}
  check(frames>60&&g.guardState(0)==GuardState::Patrol&&held==kind&&confirmed==sure&&!g.guardCaution(0)&&!g.guardCaution(1)&&g.guardAlarm()==0,std::string(sure?"A confirmed search for someone other than the player":"A search after a glimpse of the player he never made out")+" gives up without Caution ("+std::to_string(frames)+" frames)");
 }
 {auto plain=scene(yard);plain.m_player.pos={8.5f,6.5f};int near=confirm(plain);
  auto wary=scene(yard);wary.startGuardCaution(0);wary.m_player.pos={8.5f,6.5f};int nearWary=confirm(wary);
  check(nearWary<near*.8f,"On Caution he makes the player out sooner close up too ("+std::to_string(nearWary)+" frames against "+std::to_string(near)+")");
  check(wary.guardState(0)==GuardState::Alert&&!wary.guardCaution(0)&&!wary.guardCaution(1)&&wary.state("caution_alert")==1&&wary.guardAlarm()==-1,"An Alert ends the squad's Caution and still throws the alarm flag");
  wary.m_player.pos=hide;searchOut(wary);check(wary.guardCaution(0)>=CautionMs-40&&wary.guardCaution(1)==wary.guardCaution(0),"When that search gives up too, Caution starts again");
 }
 {auto g=scene(yard);g.startGuardCaution(0);g.startGuardCaution(2);g.m_player.pos={1.2f,20.5f};int frames=0;
  while(frames<600&&g.guardState(1)!=GuardState::Alert){tick(g,1);++frames;}int other=g.guardCaution(2),expected=CautionMs-int(std::lround(frames*1000/60.));
  check(g.guardState(1)==GuardState::Alert&&g.guardState(0)!=GuardState::Alert&&!g.guardCaution(0)&&!g.guardCaution(1)&&std::abs(other-expected)<=40,"His squadmate's Alert ends the squad's Caution; another squad's clock runs on ("+std::to_string(other)+" ms)");
 }
 {const std::vector<Post> post={{{6.5f,6.5f},kPi*.5f,"caution_squad",{}}};
  auto plain=scene(post,16);plain.m_player.pos={14.5f,6.5f};int far=confirm(plain);
  auto wary=scene(post,16);wary.startGuardCaution(0);wary.m_player.pos={14.5f,6.5f};int farWary=confirm(wary);
  check(far<600&&farWary<far*.75f,"An 8 m sighting is confirmed sooner on Caution ("+std::to_string(farWary)+" frames against "+std::to_string(far)+")");
 }
 {auto g=scene({{{2.5f,6.5f},kPi*.5f,"caution_squad",{}}},16);g.m_playerExposure=1;auto pose=g.npcActors()[0].pose;
  auto seen=[&](float x){g.m_player.pos={x,6.5f};return g.guardCanSee(0,pose,g.resolveGuardTarget(0,{ShotKind::Player,g.level(),0}));};
  bool near=seen(17.5f),far=seen(19.5f);g.startGuardCaution(0);bool wary=seen(19.5f),beyond=seen(21.f);
  check(near&&!far&&wary&&!beyond,"Caution stretches his sight by 15% (16 m to 18.4 m) and no further");
 }
 for(bool wary:{false,true}){auto g=scene({{{6.5f,6.5f},kPi*.5f,"caution_squad",{}}});if(wary)g.startGuardCaution(0);
  float bearing=g.npcActors()[0].pose.yaw;g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Search));g.setState(g.friendlyKey(0,"guard_search"),4000);g.setState(g.friendlyKey(0,"guard_bearing"),int(std::lround(bearing*10000)));
  float widest=0;for(int i=0;i<90;++i){tick(g,1);widest=std::max(widest,std::fabs(std::remainder(g.npcActors()[0].pose.yaw-bearing,2*kPi)));}
  check(wary?widest>1.1f:(widest>.35f&&widest<.5f),(wary?"On Caution a sound-only search sweeps as wide as a confirmed one (":"A sound-only search stays on the noise bearing (")+std::to_string(widest)+" rad)");
 }
 {auto g=scene({{{6.5f,6.5f},kPi*.5f,"caution_squad",{}}});g.m_player.pos=hide;g.startGuardCaution(0);
  float bearing=g.npcActors()[0].pose.yaw;g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Search));g.setState(g.friendlyKey(0,"guard_search"),4000);g.setState(g.friendlyKey(0,"guard_bearing"),int(std::lround(bearing*10000)));
  auto off=[&]{return std::fabs(std::remainder(g.npcActors()[0].pose.yaw-bearing,2*kPi));};int frames=0;while(frames<120&&off()<1.1f){tick(g,1);++frames;}
  float before=g.npcActors()[0].pose.yaw;g.setState(g.guardCautionKey(0),0);tick(g,1);float step=std::fabs(std::remainder(g.npcActors()[0].pose.yaw-before,2*kPi));tick(g,30);
  check(frames<120&&step<.06f&&off()<.5f&&g.guardState(0)==GuardState::Search,"Caution ending mid-sweep turns him back to the narrow arc instead of snapping ("+std::to_string(step)+" rad in a frame)");
 }
 for(bool wary:{false,true}){auto g=scene({{{6.5f,6.5f},kPi*.5f,"caution_squad",{{{6.5f,6.5f},0,3000},{{9.5f,6.5f},0,3000}}}});if(wary)g.startGuardCaution(0);
  int arrived=-1,frame=0;for(;frame<60*20;++frame){tick(g,1);int point=g.state(g.friendlyKey(0,"guard_point"));if(arrived<0&&point==1&&g.state(g.friendlyKey(0,"guard_arrived")))arrived=frame;if(arrived>=0&&point==0)break;}
  float held=(frame-arrived)/60.f;check(arrived>=0&&std::fabs(held-(wary?1.8f:3.f))<.1f,(wary?"On Caution a patrol post gets 60% of its pause (":"A patrol post keeps its authored pause (")+std::to_string(held)+" s)");
 }
 {auto g=scene({{{6.5f,6.5f},kPi*.5f,nullptr,{}},{{2.5f,20.5f},-kPi*.5f,nullptr,{}}});g.startGuardCaution(0);tick(g,1);
  Game loaded(g.m_customCampaign);check(g.guardCaution(0)>CautionMs-40&&!g.guardCaution(1)&&loaded.decodeSave(g.encodeSave())&&loaded.guardCaution(0)==g.guardCaution(0)&&!loaded.guardCaution(1),"A guard with no squad keeps his own Caution, saved with him");
 }
 {auto map=room(yard,11);map->actorTracks[1].ai.faction=AiFaction::Facility;auto campaign=std::make_shared<CustomCampaign>();campaign->name="Caution allies";campaign->key=731907;campaign->maps.push_back(map);Game g(campaign);g.m_player.pos=hide;
  auto alert=[&](size_t actor,ShotKind kind,int index){g.setState(g.friendlyKey(actor,"guard_mode"),int(GuardState::Alert));g.setState(g.friendlyKey(actor,"guard_confirmed"),1);g.setState(g.friendlyKey(actor,"guard_target_kind"),int(kind));g.setState(g.friendlyKey(actor,"guard_target_level"),1);g.setState(g.friendlyKey(actor,"guard_target_index"),index+1);};
  g.startGuardCaution(1);bool allyCaution=g.guardCaution(1)==CautionMs&&!g.guardCaution(0)&&g.guardAlarm()==0;
  g.startGuardCaution(0);bool hostileCaution=g.guardAlarm()==CautionMs;
  alert(2,ShotKind::Npc,1);alert(1,ShotKind::Npc,2);bool fight=g.guardTarget(2).kind==ShotKind::Npc&&g.guardTarget(1).kind==ShotKind::Npc&&g.guardAlarm()==CautionMs;
  alert(0,ShotKind::Player,0);bool player=g.guardAlarm()==-1;
  check(allyCaution&&hostileCaution&&fight&&player,"The HUD counts only the player's enemies: an ally's Caution and a fight between guards show nothing, a man after the player shows ALERT");
 }
 {auto first=room({},11);first->openEast=true;auto second=room({{{18.5f,6.5f},kPi*.5f,"caution_squad",{}}},11);second->definition.origin={24,0};second->openWest=true;
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Caution across chunks";campaign->key=731906;campaign->maps={first,second};Game g(campaign);
  g.startGuardCaution(0);int start=g.guardCaution(0);tick(g,120);
  check(g.level()==0&&g.npcActors()[0].level==1&&std::abs(start-g.guardCaution(0)-2000)<=20&&g.guardAlarm()==0,"A squad's Caution counts down while the player is in another chunk, and the HUD shows only this chunk's squads");
  g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Alert));g.setState(g.friendlyKey(0,"guard_target_kind"),int(ShotKind::Player));g.setState(g.friendlyKey(0,"guard_target_level"),1);g.setState(g.friendlyKey(0,"guard_target_index"),1);
  check(g.chunkResident(1)&&g.guardAlarm()==-1,"A man over the seam in Alert against the player reads ALERT here");
 }
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}

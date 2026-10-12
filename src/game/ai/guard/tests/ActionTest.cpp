#include "../../../Game.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testGuardActions(){
 std::ofstream report("guard-action-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,const char* label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](bool suppression=false,bool blocker=false){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Guard action validation";campaign->key=882745;
  auto map=std::make_shared<AuthoredMapData>();map->name="Action and cover-edge room";map->definition={{0,0},{10.5f,6.5f},0,Environment::Interior,false,8,.3f,42};AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("action_clock");timer.finishState=stateId("action_done");timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{6.5f,6.5f},0,kPi*.5f},{60000,{6.5f,6.5f},0,kPi*.5f}};guard.scale=1.6f;guard.health=70;guard.damageState=stateId("action_guard_damage");guard.deadState=stateId("action_guard_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.armed=true;guard.ai.patrol={{{6.5f,6.5f},0,1000}};
  if(suppression){guard.ai.suppress=true;guard.ai.squad=stateId("action_squad");}map->actorTracks.push_back(guard);
  if(suppression){auto other=guard;other.ai.suppress=false;for(auto&key:other.keys)key.position={6.5f,10.5f};other.ai.patrol={{{6.5f,10.5f},0,1000}};other.damageState=stateId("action_flanker_damage");other.deadState=stateId("action_flanker_dead");map->actorTracks.push_back(other);map->structures.push_back({8,0,8.2f,24,0,1.3f});}
  if(blocker){auto worker=guard;worker.ai={};worker.health=0;for(auto&key:worker.keys)key.position={7.5f,6.5f};worker.damageState=stateId("action_bystander_damage");worker.deadState=stateId("action_bystander_dead");map->actorTracks.push_back(worker);}
  campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int frames,int rate){int shots=0;for(int i=0;i<frames;++i){g.update({},1.f/rate);for(auto&s:g.sounds())shots+=s.sound==Sound::Shot;}return shots;};
 auto combat=[](Game&g,float dt){g.m_sounds.clear();g.updateGuardCombat(dt);int shots=0,pumps=0;for(auto&s:g.sounds()){shots+=s.sound==Sound::Shot;pumps+=s.sound==Sound::Pump;}return std::pair{shots,pumps};};
 for(auto action:{GuardAction::Fire,GuardAction::Bash,GuardAction::Reload}){unsigned consumed=0;auto start=consumeGuardAction(action,0,consumed);auto crossed=consumeGuardAction(action,guardActionDuration(action),consumed);
  check(start==GuardActionStart&&(crossed&GuardActionWork)&&(crossed&GuardActionComplete)&&!(crossed&GuardActionStart),"A long action step consumes each crossed event once, including its completion");
  check(consumeGuardAction(action,guardActionDuration(action),consumed)==0&&validGuardAction(action,guardActionDuration(action),consumed),"Revisiting a saved action phase never repeats an already consumed event");
 }
 for(int rate:{60,120}){auto g=scene();int frames=0;while(g.player().health==100&&frames++<rate*3)tick(g,1,rate);
  check(g.player().health==82&&g.npcWeaponPose(0).action==GuardAction::Fire&&g.npcWeaponPose(0).actionProgress==0&&g.npcWeaponPose(0).flash>0,"Real firing starts the visible Fire action at the existing first-shot time");
  Game saved(g.m_customCampaign);bool decoded=saved.decodeSave(g.encodeSave());check(decoded,"The event mask and just-fired pose survive a save before the pump event");
  int pumps=0,loadedPumps=0;for(int j=0;j<rate/3;++j){g.update({},1.f/rate);for(auto&s:g.sounds())pumps+=s.sound==Sound::Pump;if(decoded){saved.update({},1.f/rate);for(auto&s:saved.sounds())loadedPumps+=s.sound==Sound::Pump;}}
  check(pumps==1&&g.npcWeaponPose(0).action==GuardAction::None&&g.player().health==82,"Fire action pumps exactly once and completes without a second shot during recovery");
  if(decoded)check(loadedPumps==pumps&&saved.player().health==g.player().health&&saved.state(saved.friendlyKey(0,"weapon_rounds"))==g.state(g.friendlyKey(0,"weapon_rounds")),"Restored firing consumes the same remaining event once at 60/120 Hz");
 }
 {auto g=scene();combat(g,1.f/60);g.setState(g.friendlyKey(0,"weapon_rounds"),0);combat(g,1.f/60);
  check(g.npcWeaponPose(0).action==GuardAction::Reload&&g.state(g.friendlyKey(0,"weapon_reload"))==1800,"Empty tube starts the actual Reload timeline with its unchanged 1800ms interval");
  auto first=combat(g,.65f);check(first.first==0&&first.second==1&&g.npcWeaponPose(0).reload>.35f&&g.state(g.friendlyKey(0,"weapon_rounds"))==0,"A large frame crosses one shell event without early refill or firing");
  Game saved(g.m_customCampaign);bool decoded=saved.decodeSave(g.encodeSave());check(decoded,"A reload between shell and chamber events can be saved");auto second=combat(g,.7f);auto loaded=decoded?combat(saved,.7f):std::pair{0,0};
  check(second.second==1&&second.first==0&&g.state(g.friendlyKey(0,"weapon_rounds"))==0,"The chamber event is consumed exactly once while the tube remains empty");
  if(decoded)check(loaded==second&&saved.state(saved.friendlyKey(0,"weapon_reload"))==g.state(g.friendlyKey(0,"weapon_reload")),"Restored reload keeps the same remaining chamber clock and event count");
  auto end=combat(g,.5f);check(end.first==0&&g.state(g.friendlyKey(0,"weapon_rounds"))==6&&!g.state(g.friendlyKey(0,"weapon_reloading"))&&g.npcWeaponPose(0).action==GuardAction::None,"Crossing reload completion replenishes once and closes the visible action");
  auto after=combat(g,.5f);check(after.second==0&&g.state(g.friendlyKey(0,"weapon_rounds"))==6,"Advancing after completion cannot replay a reload event or create extra rounds");
 }
 {auto g=scene();g.m_player.pos={7.5f,6.5f};g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Alert));combat(g,1.f/60);g.setState(g.friendlyKey(0,"weapon_raise"),1000);combat(g,.1f);
  check(g.npcWeaponPose(0).action==GuardAction::Bash&&g.state(g.friendlyKey(0,"weapon_bash"))>0&&g.player().health==100,"Close guard bash starts a visible windup without immediate damage");
  Game saved(g.m_customCampaign);bool decoded=saved.decodeSave(g.encodeSave());check(decoded,"A partially wound-up bash preserves its phase and event mask");auto strike=combat(g,.25f);auto savedStrike=decoded?combat(saved,.25f):std::pair{0,0};
  check(strike.first==0&&g.player().health==88&&g.state(g.friendlyKey(0,"weapon_rounds"))==6&&g.state(g.friendlyKey(0,"weapon_bash_pose"))==220,"The crossed bash impact deals damage once without firing or spending ammunition");
  if(decoded)check(savedStrike==strike&&saved.player().health==88,"Saved bash resumes its pending impact rather than replaying its windup");
  combat(g,.6f);check(g.player().health==88&&g.npcWeaponPose(0).action==GuardAction::None,"Bash recovery closes the pose and cannot repeat the previous strike");
 }
 {auto g=scene();g.m_player.pos={7.5f,6.5f};g.setState(g.friendlyKey(0,"guard_mode"),int(GuardState::Alert));combat(g,1.f/60);g.setState(g.friendlyKey(0,"weapon_raise"),1000);combat(g,.1f);g.m_player.pos={18.5f,18.5f};combat(g,.5f);
  check(g.player().health==100&&g.state(g.friendlyKey(0,"weapon_bash"))==0&&g.npcWeaponPose(0).action!=GuardAction::Bash,"Losing the live close-range line cancels pending bash damage and pose");
 }
 auto prepare=[&](Game&g){g.m_player.crouched=true;g.m_player.eye=Player::CrouchedEye;for(size_t actor=0;actor<2;++actor){AiTarget target;target.kind=ShotKind::Player;target.level=0;target.index=0;target.position={10.5f,6.5f};target.height=1.6f;target.eye=1.42f;target.faction=AiFaction::Player;g.rememberGuardTarget(actor,target);auto put=[&](const char*f,int value){g.setState(g.friendlyKey(actor,f),value);};put("guard_mode",int(GuardState::Investigate));put("guard_confirmed",1);put("guard_memory",1);put("guard_seen",0);put("guard_x",10500);put("guard_y",6500);put("guard_z",0);put("weapon_saved",1);put("weapon_rounds",6);put("weapon_raise",1000);put("weapon_target",1);put("weapon_x",10500);put("weapon_y",6500);put("weapon_z",960);put("weapon_target_kind",int(ShotKind::Player));put("weapon_target_level",1);put("weapon_target_index",1);put("guard_suppress_ms",1200);}g.m_friendlyActors[1].pose.clip=1;};
 {auto g=scene(true);prepare(g);Point3 cover;bool found=g.guardSuppressionPoint(0,cover);check(found&&g.guardShot(0,g.npcActors()[0].pose,g.npcWeaponPoseAt(0,g.npcActors()[0].pose,cover,1)).kind==ShotKind::Solid,"Authored anchor finds a solid cover edge while a same-target squadmate flanks");
  g.m_player.pos={12.5f,8.5f};Point3 moved;check(g.guardSuppressionPoint(0,moved)&&std::hypot(length(Vec2{moved.x-cover.x,moved.y-cover.y}),moved.z-cover.z)<.001f,"Suppression aim does not follow the unseen player's movement or crouched body");int shots=0;for(int j=0;j<45;++j)shots+=combat(g,1.f/60).first;
  check(shots==1&&g.player().health==100&&g.state(g.friendlyKey(0,"weapon_rounds"))==5&&!g.bulletImpacts().empty()&&g.state("action_flanker_damage")==0,"Suppression spends one actual round into cover with no concealed target or ally damage");
  for(int j=0;j<40;++j)shots+=combat(g,1.f/60).first;check(shots==1&&!g.guardSuppressionPoint(0,moved)&&g.state(g.friendlyKey(0,"guard_suppress_ms"))==0,"Suppression pressure expires after its bounded last-seen interval");
 }
 {auto g=scene(true,true);prepare(g);Point3 point;int shots=0;for(int j=0;j<90;++j)shots+=combat(g,1.f/60).first;check(!g.guardSuppressionPoint(0,point)&&shots==0&&g.state(g.friendlyKey(0,"weapon_rounds"))==6&&g.state("action_bystander_damage")==0,"An intervening neutral living body prevents every suppression shot and ammunition spend");}
 {auto g=scene(true);prepare(g);g.setState(g.friendlyKey(1,"guard_flank_done"),1);Point3 point;check(!g.guardSuppressionPoint(0,point)&&combat(g,.8f).first==0,"Finishing the teammate's flank cancels the anchor's suppression permission");}
 {auto g=scene(true);prepare(g);g.damageFriendly(1,100,g.player().pos);Point3 point;check(!g.guardSuppressionPoint(0,point)&&combat(g,.8f).first==0,"A dead flanker cannot grant suppression permission or hold an anchor's lease");}
 {auto g=scene();combat(g,1.f/60);g.setState(g.friendlyKey(0,"weapon_rounds"),0);combat(g,1.f/60);g.damageFriendly(0,100,g.player().pos);auto dead=combat(g,2.f);check(dead==std::pair{0,0}&&g.npcWeaponPose(0).action==GuardAction::None&&!g.state(g.friendlyKey(0,"weapon_reloading")),"Death cancels reload/action clocks without a late shell, attack or refill");}
 for(const char*field:{"weapon_action","weapon_action_ms","weapon_action_events","weapon_reload_events","guard_suppress_ms"}){auto g=scene();g.setState(g.friendlyKey(0,field),std::numeric_limits<int>::max());Game saved(g.m_customCampaign);check(!saved.decodeSave(g.encodeSave()),"Malformed saved action phases and suppression clocks are rejected safely");}
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}

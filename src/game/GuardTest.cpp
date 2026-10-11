#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testGuardAI(){
 if(!testGuardCombat())return false;
 if(!testGuardTactics())return false;
 if(!testGuardPolicy())return false;
 if(!testGuardDefense())return false;
 std::ofstream report("guard-ai-test.txt");bool passed=true;auto check=[&](bool ok,const char*label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;};
 auto scene=[](bool wall=false,bool gate=false){auto campaign=std::make_shared<CustomCampaign>();campaign->name="Guard validation";campaign->key=728193;
  auto map=std::make_shared<AuthoredMapData>();map->name="Guard room";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  if(wall)map->structures.push_back({7.4f,0,7.6f,24,0,8});
  TimedSequence timer;timer.timerState=stateId("guard_clock");timer.finishState=stateId("guard_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  ActorTrack actor;actor.timerState=timer.timerState;actor.keys={{0,{6.5f,6.5f},0,kPi*.5f},{30000,{6.5f,6.5f},0,kPi*.5f}};actor.scale=1.6f;actor.health=70;actor.damageState=stateId("guard_damage");actor.deadState=stateId("guard_dead");actor.ai.mode=ActorAiMode::Guard;actor.ai.alertState=stateId("guard_alert");actor.ai.enableState=gate?stateId("guard_enabled"):0;
  actor.ai.patrol={{{6.5f,6.5f},0,1000},{{9.5f,6.5f},0,600},{{9.5f,9.5f},0,600},{{6.5f,9.5f},0,600}};map->actorTracks.push_back(actor);campaign->maps.push_back(map);return Game(campaign);
 };
 auto tick=[](Game&g,int count,int rate=60){for(int i=0;i<count;++i)g.update({},1.f/rate);};
 for(int rate:{60,120}){auto g=scene();tick(g,rate/3,rate);check(lengthSq(g.npcActors()[0].pose.position-Vec2{6.5f,6.5f})<.001f,"Guard respects authored pause at 60/120 Hz");
  Game paused(g.m_customCampaign);check(paused.decodeSave(g.encodeSave())&&paused.state(paused.friendlyKey(0,"guard_pause"))==g.state(g.friendlyKey(0,"guard_pause")),"Save restores an interrupted patrol pause");
  tick(g,rate*7,rate);check(g.state(g.friendlyKey(0,"guard_point"))>0&&lengthSq(g.npcActors()[0].pose.position-Vec2{6.5f,6.5f})>1,"Guard follows authored patrol points and walk animation");
  g=scene();g.m_player.pos={8.5f,6.5f};tick(g,rate/5,rate);check(g.guardState(0)==GuardState::Suspicious&&!g.state("guard_alert"),"Visible player raises suspicion before recognition delay expires");
  Game suspicious(g.m_customCampaign);bool loaded=suspicious.decodeSave(g.encodeSave());check(loaded&&suspicious.state(suspicious.friendlyKey(0,"guard_recognition"))==g.state(g.friendlyKey(0,"guard_recognition")),"Save restores partial recognition without instant alert");
  if(loaded){tick(suspicious,rate/2,rate);check(suspicious.guardState(0)==GuardState::Alert&&suspicious.state("guard_alert"),"Sustained visibility confirms alert after the authored delay");
   auto remembered=Vec2{suspicious.state(suspicious.friendlyKey(0,"guard_x"))*.001f,suspicious.state(suspicious.friendlyKey(0,"guard_y"))*.001f};suspicious.m_player.pos={18.5f,18.5f};tick(suspicious,1,rate);
   check(suspicious.guardState(0)==GuardState::Investigate&&lengthSq(Vec2{suspicious.state(suspicious.friendlyKey(0,"guard_x"))*.001f,suspicious.state(suspicious.friendlyKey(0,"guard_y"))*.001f}-remembered)<.001f,"Lost sight investigates the last observed position without tracking hidden movement");
   Game investigating(g.m_customCampaign);check(investigating.decodeSave(suspicious.encodeSave())&&investigating.guardState(0)==GuardState::Investigate,"Save restores an active investigation");
   tick(investigating,rate*15,rate);check(investigating.guardState(0)==GuardState::Patrol,"Bounded investigation and search return to patrol");
  }
 }
 {auto g=scene();g.m_player.pos={8.5f,6.5f};tick(g,10);g.m_player.pos={18.5f,18.5f};tick(g,1);check(g.guardState(0)==GuardState::Patrol&&g.state(g.friendlyKey(0,"guard_recognition"))==0,"Brief glimpse does not become a delayed alert after the player hides");}
 {auto g=scene();g.m_player.pos={3.5f,6.5f};tick(g,45);check(g.guardState(0)==GuardState::Patrol,"Player behind guard's sight cone remains unrecognized");}
 {auto g=scene(true);g.m_player.pos={8.5f,6.5f};tick(g,45);check(g.guardState(0)==GuardState::Patrol,"Wall blocks recognition inside the sight cone");}
 for(bool crouch:{false,true}){auto g=scene();g.m_player.pos={3.7f,6.5f};g.m_player.angle=0;g.m_stepDistance=2;InputState input{};input.forward=true;input.crouch=crouch;g.update(input,1.f/60);
  check(crouch?g.guardState(0)==GuardState::Patrol:g.guardState(0)==GuardState::Investigate,"Real Game update hears walking and preserves quiet crouching in the same frame");
 }
 {auto g=scene();g.m_player.pos={3.7f,6.5f};g.m_player.angle=kPi;g.m_suppressFire=false;InputState input{};input.fire=true;g.update(input,1.f/60);
  check(g.guardState(0)==GuardState::Investigate&&std::fabs(g.state(g.friendlyKey(0,"guard_x"))*.001f-g.player().pos.x)<.001f,"Real Game update hears a gunshot emitted after mechanisms");
 }
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);map->structures.push_back({1,7.4f,5.5f,7.6f,0,3});campaign->maps[0]=map;Game g(campaign);
  g.m_player.pos={3.5f,8.5f};g.m_player.angle=0;g.m_player.pitch=-38;Clutter object;object.kind=3;object.pos=g.player().pos;g.m_clutter={object};g.m_heldClutter=0;g.m_suppressFire=false;
  InputState input{};input.fire=true;input.crouch=true;g.update(input,1.f/60);input.fire=false;bool heard=false;
  for(int i=0;i<120&&!heard;++i){g.update(input,1.f/60);for(const auto&sound:g.sounds())if(sound.sound==Sound::JunkMetal&&g.guardState(0)==GuardState::Investigate)heard=true;}
  check(heard&&lengthSq(Vec2{g.state(g.friendlyKey(0,"guard_x"))*.001f,g.state(g.friendlyKey(0,"guard_y"))*.001f}-g.player().pos)>1,"Crouched throw in real Game update distracts guard beyond cover without revealing player");
 }
 {auto g=scene();g.m_player.pos={7.5f,6.5f};g.m_player.angle=kPi;check(!g.commandFriendly(),"PC Follow/Wait cannot command a guard");g.setVrHand(0,{6.8f,6.5f,1.2f},{-1,0,0},true);g.m_vrInteractionHand=0;check(!g.commandFriendly(),"VR Follow/Wait cannot command a guard");}
 {auto g=scene(false,true);g.m_player.pos={8.5f,6.5f};tick(g,45);check(g.guardState(0)==GuardState::Patrol&&!g.state("guard_alert"),"Disabled guard preserves scripted control and cannot alert");g.setState(stateId("guard_enabled"),1);tick(g,45);check(g.guardState(0)==GuardState::Alert,"Author enable flag activates guard recognition");g.damageFriendly(0,100,g.player().pos);tick(g,60);check(g.npcActors()[0].pose.clip==4&&!g.state("guard_alert"),"Dead guard stops perception and clears its alert flag");}
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);map->structures.push_back({10,0,10.2f,24,0,8});auto actor=map->actorTracks[0];map->actorTracks.clear();
  for(int i=0;i<9;++i){auto guard=actor;for(auto&key:guard.keys)key.position={9.3f,4.5f+float(i)*.25f};guard.ai.patrol={{{16.5f,6.5f},0,1000}};map->actorTracks.push_back(guard);}campaign->maps[0]=map;Game g(campaign);tick(g,1);int planned=0;for(size_t i=0;i<9;++i)planned+=g.state(g.friendlyKey(i,"repath"))>0;
  check(planned==4&&g.m_npcNavBudget==0,"Guard navigation shares the four-search frame budget");tick(g,2);planned=0;for(size_t i=0;i<9;++i)planned+=g.state(g.friendlyKey(i,"repath"))>0;check(planned==9,"Unreachable guards do not starve later guards");
 }
 for(bool locked:{false,true}){auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);for(auto&key:map->actorTracks[0].keys){key.position={6.5f,6.5f};key.yaw=0;}map->actorTracks[0].ai.patrol={{{6.5f,11.5f},0,1000}};
  for(int x=0;x<24;++x)if(x!=6)map->layers[0].rows[8][x]='#';Door gate{6,7,8,0,false,false,false};if(locked)gate.requireState=stateId("guard_door_permission");map->doors.push_back(gate);campaign->maps[0]=map;Game g(campaign);tick(g,420);
  check(locked?(g.npcActors()[0].pose.position.y<8&&!g.world().doors()[0].opening):g.npcActors()[0].pose.position.y>8.5f,"Guard opens ordinary doors and preserves authored locks");
 }
 {auto g=scene();g.setState(g.friendlyKey(0,"guard_point"),99);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Invalid saved patrol index is rejected safely");}
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto home=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto neighbor=std::make_shared<AuthoredMapData>(*home);home->actorTracks.clear();neighbor->definition.origin={24,0};neighbor->actorTracks[0].ai.patrol={{{6.5f,11.5f},0,1000}};for(auto&key:neighbor->actorTracks[0].keys)key.yaw=0;
  for(int x=0;x<24;++x)if(x!=6)neighbor->layers[0].rows[8][x]='#';neighbor->doors.push_back({6,7,8,0,false,false,false});campaign->maps={home,neighbor};Game g(campaign);tick(g,420);
  check(g.level()==0&&g.npcActors()[0].level==1&&g.npcActors()[0].pose.position.y>8.5f&&g.m_chunks[1].world.doors()[0].open>.8f,"Guard passes an ordinary door in a resident neighboring chunk while player stays away");
  auto transfer=Door{6,7,24,0,true,true,false};neighbor->doors.push_back(transfer);World withTransfer(1,neighbor);withTransfer.openDoor(0);withTransfer.updateDoors(.5f,false);
  check(withTransfer.doors()[0].open>0&&withTransfer.doors()[1].open==0,"Neighbor ordinary-door animation does not double-advance transfer leaves");
 }
 {auto g=scene();g.setState(g.friendlyKey(0,"guard_mode"),99);Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Invalid saved guard state is rejected safely");}
 {auto g=scene();g.setState(g.friendlyKey(0,"guard_z"),std::numeric_limits<int>::min());Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Extreme saved investigation height is rejected without integer overflow");}
 {auto g=scene();g.setState(g.friendlyKey(0,"guard_step"),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);check(!restored.decodeSave(g.encodeSave()),"Extreme saved footstep accumulator is rejected without integer overflow");}
 {auto original=scene();auto campaign=std::make_shared<CustomCampaign>(*original.m_customCampaign);auto map=std::make_shared<AuthoredMapData>(*campaign->maps[0]);auto guard=map->actorTracks[0];guard.damageState=stateId("second_guard_damage");guard.deadState=stateId("second_guard_dead");for(auto&key:guard.keys){key.position={11.5f,6.5f};key.yaw=-kPi*.5f;}map->actorTracks.push_back(guard);campaign->maps[0]=map;Game g(campaign);g.m_player.pos={8.5f,6.5f};tick(g,45);g.damageFriendly(0,100,g.player().pos);check(g.state("guard_alert")==1,"Shared alert flag remains set while another living guard is alerted");g.damageFriendly(1,100,g.player().pos);check(g.state("guard_alert")==0,"Shared alert flag clears when the final alerted guard dies");}
 {auto original=scene();auto map=std::make_shared<AuthoredMapData>(*original.m_customCampaign->maps[0]);map->definition.environment=Environment::Outdoor;map->definition.ceiling=2.8f;map->layers[0].rows[6][7]='#';World world(0,map);
  check(std::fabs(world.wallHeight(7,6)-2.8f)<.001f&&!world.fits(7.5f,6.5f,0,1.6f)&&!world.lightRayClear({6.5f,6.5f},1.4f,{8.5f,6.5f},1.4f),"Authored outdoor grid walls retain height, hull collision and sight occlusion without terrain");
 }
 std::string records="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Guard authoring test|0\nMAP|0|Room|0|0|18.5|18.5|0|0|8|0.3||0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int y=0;y<24;++y)records+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";
 records+="TIMED_SEQUENCE|0|clock|done|0|0|24|24|-0.2|8|30000|30000|0|0|0|0|1|1\nACTOR|0|clock|0|1.6|0|0|0\nACTOR_KEY|0|0|0|6.5|6.5|0|1.57|0|0\nACTOR_KEY|0|0|30000|6.5|6.5|0|1.57|0|1\n";
 auto load=[&](const std::string&tail){std::ofstream file("guard-authoring-fixture.txt");file<<records<<tail<<"--- CUSTOM_CAMPAIGN_DATA_END ---\n";file.close();return loadCustomCampaignFile("guard-authoring-fixture.txt");};
 auto rejects=[&](const std::string&tail){try{load(tail);return false;}catch(const std::exception&){return true;}};
 const std::string ai="ACTOR_AI|0|0|3||1.4|2|6\n",guard="ACTOR_GUARD|0|0|11|0.65|600|4000|alert\n",point="ACTOR_PATROL|0|0|6.5|6.5|0|900\n";
 try{auto authored=load(guard+point+ai);const auto&config=authored->maps[0]->actorTracks[0].ai;check(config.mode==ActorAiMode::Guard&&config.recognitionMs==600&&config.patrol.size()==1&&config.patrol[0].pauseMs==900,"Guard configuration survives ACTOR_AI record ordering");}catch(const std::exception&){check(false,"Guard configuration survives ACTOR_AI record ordering");}
 try{auto authored=load("ACTOR_AI|0|0|2||1.4|2|6\n");check(authored->maps[0]->actorTracks[0].ai.mode==ActorAiMode::Escort,"Older escort records remain compatible");}catch(const std::exception&){check(false,"Older escort records remain compatible");}
 check(rejects(ai+guard),"Guard authoring rejects a missing patrol");
 check(rejects("ACTOR_AI|0|0|2||1.4|2|6\n"+guard+point),"Guard authoring rejects configuration on an escort");
 check(rejects(ai+"ACTOR_GUARD|0|0|11|0.65|99|4000|alert\n"+point),"Guard authoring bounds recognition delay");
 check(rejects(ai+guard+"ACTOR_PATROL|0|0|24|6.5|0|900\n"),"Guard authoring rejects patrol points outside the home chunk");
 try{auto authored=load("ACTOR_WEAPON|0|0|1|650|1000|9|18\n"+guard+point+ai);const auto&config=authored->maps[0]->actorTracks[0].ai;check(config.armed&&config.aimMs==650&&config.attackDamage==18,"Armed guard configuration survives record ordering");}catch(const std::exception&){check(false,"Armed guard configuration survives record ordering");}
 check(rejects(ai+guard+point+"ACTOR_WEAPON|0|0|2|650|1000|9|18\n"),"Guard weapon authoring rejects unsupported weapons");
 check(rejects("ACTOR_AI|0|0|2||1.4|2|6\nACTOR_WEAPON|0|0|1|650|1000|9|18\n"),"Weapon authoring rejects armed civilian/escort modes");
 check(rejects(ai+guard+point+"ACTOR_WEAPON|0|0|1|99|1000|9|18\n"),"Guard weapon authoring bounds reaction delay");
 check(rejects(ai+guard+point+"ACTOR_WEAPON|0|0|1|650|1000|9|101\n"),"Guard weapon authoring bounds damage");
 try{auto authored=load("ACTOR_FACTION|0|0|2|security\n"+guard+point+ai);const auto&config=authored->maps[0]->actorTracks[0].ai;check(config.faction==AiFaction::Facility&&config.squad==stateId("security"),"Faction and squad authoring survive record ordering");}catch(const std::exception&){check(false,"Faction and squad authoring survive record ordering");}
 check(rejects(ai+guard+point+"ACTOR_FACTION|0|0|6|\n"),"Faction authoring rejects unknown relationship groups");
 check(rejects("ACTOR_AI|0|0|2||1.4|2|6\nACTOR_FACTION|0|0|2|security\n"),"Squad authoring rejects non-guard members");
 std::string points;for(int i=0;i<65;++i)points+=point;check(rejects(ai+guard+points),"Guard authoring rejects oversized patrol routes");
 return passed;
}
Game Game::guardInspection(int stage){
 auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");Game g(campaign);if(g.npcActors().empty())throw std::runtime_error("Ash custom playground has no authored guard");
 auto pose=g.npcActors()[0].pose;
 if(stage==0){unsigned visited=0;for(int i=0;i<1800;++i){g.update({},1.f/60);visited|=1u<<g.state(g.friendlyKey(0,"guard_point"));if(g.guardState(0)!=GuardState::Patrol)throw std::runtime_error("Custom playground guard saw through hiding cover");}if((visited&15)!=15)throw std::runtime_error("Custom playground patrol did not visit all four points");}
 if(stage==1){g.m_player.pos=pose.position+Vec2{std::sin(pose.yaw),std::cos(pose.yaw)}*2;g.m_player.z=pose.z;for(int i=0;i<70;++i)g.update({},1.f/60);}
 if(stage==2){g.m_sounds={{Sound::JunkMetal,pose.position+Vec2{2,2},1,1,true,17,pose.z+.04f}};g.updateFriendlyActors(1.f/60,true);}
 pose=g.npcActors()[0].pose;Vec2 camera=pose.position+Vec2{1.25f,-2.5f};if(!g.hullFits(camera,pose.z,Player::StandingHeight))throw std::runtime_error("Guard inspection camera is inside map geometry");g.m_player.pos=camera;g.m_player.z=pose.z;g.m_player.angle=std::atan2(pose.position.y-camera.y,pose.position.x-camera.x);g.m_player.pitch=-8;
 return g;
}
}

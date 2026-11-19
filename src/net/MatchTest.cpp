#include "Match.h"
#include "Loopback.h"
#include "Wire.h"
#include "../game/Game.h"
#include "../world/CustomCampaign.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <ostream>
namespace retro::net {
// --net-test: Games in one process play over one LoopbackHub. The maps are written to a temp folder here (no
// untracked map is needed): a walled arena with a wall across its middle, two sleeping creatures and a box of
// shells. Deathmatch with a map download, interpolation, a kill, respawn, the frag limit and its restart, the
// host's hand-over; co-op with creatures, pickups and the host leaving; and a lossy, slow hub.
bool testMatch(std::ostream& report){return Match::selfTest(report);}

namespace {
std::string arenaText(bool padded){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Net Arena|0\nMAP|0|Arena|0|0|3.5|3.5|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row){std::string r(24,'.');r.front()=r.back()='#';if(row==0||row==23)r=std::string(24,'#');if(row==12)for(int x=8;x<16;++x)r[size_t(x)]='#';
  t+="ROW|0|0|"+std::to_string(row)+"|"+r+"\n";}
 t+="CREATURE|0|0|19.5|19.5|0||400|net_sleep_a\nCREATURE|0|0|19.5|15.5|0||5|net_sleep_b\nPICKUP|0|1|4.5|19.5\n";
 // Padding (comment lines) so the file takes several frames to arrive and the progress shows.
 if(padded)for(int i=0;i<400;++i)t+="# padding line "+std::to_string(i)+" so the map is sent in many chunks ----------------------\n";
 return t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";
}
std::string lobbyText(){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Lobby|0\nMAP|0|Lobby|0|0|3.5|3.5|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row)t+="ROW|0|0|"+std::to_string(row)+"|"+(row==0||row==23?std::string(24,'#'):"#......................#")+"\n";
 return t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";
}
}

bool Match::selfTest(std::ostream& report){
 const auto began=std::chrono::steady_clock::now();
 bool ok=true;auto check=[&](bool c,const std::string& what){report<<(c?"PASS ":"FAIL ")<<what<<'\n';ok&=c;return c;};
 namespace fs=std::filesystem;
 const fs::path root=fs::temp_directory_path()/("rawmetal-net-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{root};
 const fs::path hostMaps=root/"host",guestMaps=root/"guest",thirdMaps=root/"third";
 for(const auto& d:{hostMaps,guestMaps,thirdMaps})fs::create_directories(d);
 const std::string arenaFile="Net Arena.rmc";
 {std::ofstream(hostMaps/arenaFile,std::ios::binary)<<arenaText(true);std::ofstream(thirdMaps/arenaFile,std::ios::binary)<<arenaText(true);
  std::ofstream(guestMaps/"Lobby.rmc",std::ios::binary)<<lobbyText();}
 // The host's copy with its MULTIPLAYER settings and SPAWNs as the Builder writes them: four corners, frag limit 2.
 auto hostCampaign=[&]{auto c=std::make_shared<CustomCampaign>(*loadCustomCampaignFile(hostMaps/arenaFile));
  c->multiplayer.fragLimit=2;c->multiplayer.timeLimitMinutes=0;c->multiplayer.friendlyFire=false;
  c->multiplayer.spawns={{0,4.5f,4.5f,0,45,0},{0,19.5f,4.5f,0,135,0},{0,4.5f,10.5f,0,-45,0},{0,19.5f,10.5f,0,-135,0}};
  return std::shared_ptr<const CustomCampaign>(c);};
 auto makeHost=[&]{auto g=std::make_unique<Game>(hostCampaign());g->m_customMapDirectory=hostMaps.wstring();return g;};
 auto makeGuest=[&](const fs::path& folder,bool hasArena){auto g=std::make_unique<Game>(loadCustomCampaignFile(guestMaps/"Lobby.rmc"));g->m_customMapDirectory=folder.wstring();
  if(hasArena)g->m_customCampaigns.push_back(loadCustomCampaignFile(folder/arenaFile));return g;};
 const float dt=1.f/60;
 std::vector<Game*> playing;
 auto frames=[&](int n){for(int i=0;i<n;++i)for(auto* g:playing)g->update(InputState{},dt);};
 auto until=[&](int limit,auto done){for(int i=0;i<limit;++i){if(done())return true;frames(1);}return done();};
 auto remote=[](const Game& g,int slot)->const RemotePlayer*{for(const auto& r:g.m_remotePlayers)if(r.slot==slot)return &r;return nullptr;};
 auto here=[](const Game& g){return Point3{g.m_player.pos.x,g.m_player.pos.y,g.m_player.z};};
 auto gap=[](Point3 a,Point3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));};
 auto slotOfGame=[](const Game& g){return g.m_match?g.m_match->m_selfSlot:-1;};
 auto aim=[](Game& g,Point3 target){const Vec2 d{target.x-g.m_player.pos.x,target.y-g.m_player.pos.y};g.m_player.angle=std::atan2(d.y,d.x);
  g.m_player.pitch=std::atan2(target.z-(g.m_player.z+g.m_player.eye),std::max(.1f,std::hypot(d.x,d.y)))*140.f;};
 auto frags=[](const Game& g,const std::string& name){for(const auto& s:g.m_matchView.scores)if(s.name==name)return s.frags;return -99;};
 auto onSpawn=[](const Match& m,const Game& g){const Vec2 o=g.chunkOffset(g.m_level);for(const auto& s:m.m_spawns)if(std::hypot(s.world.x-(g.m_player.pos.x+o.x),s.world.y-(g.m_player.pos.y+o.y))<.05f)return true;return false;};
 // Walks a player at speed m/s along dir for seconds (a guest's own movement; the host's for the host).
 auto walk=[&](Game& g,Vec2 dir,float speed,float seconds,float* worst,Game* viewer,int slot){
  const int n=int(seconds/dt);for(int i=0;i<n;++i){g.m_player.pos=g.m_player.pos+dir*(speed*dt);g.m_velocity=dir*speed;frames(1);
   if(worst&&viewer){if(const auto* r=remote(*viewer,slot))*worst=std::max(*worst,gap(r->position,here(g)));else *worst=99;}}
  g.m_velocity={};};
 // The test sets players where it wants them: the host places itself; a guest is placed by the host (a correction).
 auto put=[&](Game& host,Game& g,Vec2 p){if(&g==&host)g.m_player.pos=p;else host.m_match->placePlayer(slotOfGame(g),g.m_level,{p.x,p.y,g.m_player.z});};
 auto killWith=[&](Game& shooter,Game& victim,int shots){shooter.m_player.pools[1]={72,6};
  for(int i=0;i<shots&&!victim.dead();++i){aim(shooter,{victim.m_player.pos.x,victim.m_player.pos.y,victim.m_player.z+1.f});shooter.shoot();frames(8);}};

 // ===== Deathmatch =====
 {
  LoopbackHub hub;auto sh=hub.connect("Hana"),sg=hub.connect("Gus"),st=hub.connect("Tam");
  auto host=makeHost(),guest=makeGuest(guestMaps,false),third=makeGuest(thirdMaps,true);
  host->setNetSession(sh.get());guest->setNetSession(sg.get());third->setNetSession(st.get());
  playing={host.get(),guest.get()};frames(1);
  HostOptions o;o.mode=Mode::Deathmatch;o.map=arenaFile;o.mapHash=mapFileHash(hostMaps/arenaFile);o.maxPlayers=8;sh->host(o);
  frames(1);
  check(host->m_match&&host->m_match->started()&&host->m_matchView.active&&host->m_matchView.host,"the host's lobby makes a match and the host plays the map");
  check(host->m_enemies.empty(),"deathmatch: the map's creatures are not spawned");
  check(onSpawn(*host->m_match,*host),"the host starts on one of the map's SPAWN points");
  sg->joinCode(sh->lobby().joinCode);
  bool progress=false;
  until(240,[&]{const auto& s=guest->m_matchView.status;progress|=s.find("Waiting for the map (")!=std::string::npos&&s.find("(100%)")==std::string::npos&&s.find("(0%)")==std::string::npos;return guest->m_match&&guest->m_match->started();});
  check(guest->m_match&&guest->m_match->started(),"a guest without the map joins and starts playing");
  check(progress,"while the map arrives the guest's status shows how much has come");
  {std::error_code e;check(fs::is_regular_file(guestMaps/"Downloaded"/arenaFile,e)&&mapFileHash(guestMaps/"Downloaded"/arenaFile)==o.mapHash,"the guest saved the host's map as custom maps/Downloaded/Net Arena.rmc, byte for byte");}
  check(guest->m_customCampaign&&guest->m_customCampaign->name=="Net Arena"&&guest->m_netScriptsOff,"the guest loaded it with the campaign loader and plays it (scripts left to the host)");
  frames(20);
  check(onSpawn(*host->m_match,*guest)||onSpawn(*guest->m_match,*guest),"the guest spawned on a SPAWN point the host chose");
  check(guest->m_enemies.empty(),"deathmatch: the guest has no creatures either");
  const int gs=slotOfGame(*guest);
  check(gs>0&&remote(*host,gs)&&remote(*guest,0)&&remote(*guest,0)->name=="Hana"&&remote(*host,gs)->name=="Gus","each sees the other, by name and slot");
  // Interpolation while both move, then at rest.
  host->m_player.pos={6.5f,6.5f};put(*host,*guest,{6.5f,9.5f});frames(30);
  float worstHost=0,worstGuest=0;
  for(int k=0;k<2;++k){walk(*host,{1,0},2.5f,.8f,&worstHost,guest.get(),0);walk(*guest,{1,0},2.5f,.8f,&worstGuest,host.get(),gs);}
  frames(30);
  const auto* hv=remote(*guest,0);const auto* gv=remote(*host,gs);
  const float restHost=hv?gap(hv->position,here(*host)):99,restGuest=gv?gap(gv->position,here(*guest)):99;
  check(worstHost<.6f&&worstGuest<.6f,"moving at 2.5 m/s each is drawn within 0.6 m of where they are (worst "+std::to_string(worstHost)+" / "+std::to_string(worstGuest)+" m)");
  check(restHost<.05f&&restGuest<.05f,"at rest each is drawn where they stand (error "+std::to_string(restHost)+" / "+std::to_string(restGuest)+" m)");
  check(hv&&std::fabs(std::remainder(hv->angle-host->m_player.angle,kTwoPi))<.01f,"the host's facing arrives");
  // A guest who jumps through the wall across the middle is put back.
  {put(*host,*guest,{11.5f,11.4f});frames(20);const Point3 before=here(*guest);guest->m_player.pos={11.5f,13.6f};frames(30);
   const auto* v=remote(*host,gs);
   check(guest->m_player.pos.y<12.f&&v&&v->position.y<12.f&&gap(here(*guest),before)<.3f,"a guest's move through a wall is refused and the guest is sent back");}
  // The kill: a guest's shots kill the host and the frag counts.
  host->m_player.pos={8.5f,5.5f};put(*host,*guest,{8.5f,8.5f});frames(30);
  killWith(*guest,*host,4);frames(10);
  check(host->dead(),"three shotgun rounds from the guest (traced by the host) kill the host");
  check(frags(*host,"Gus")==1&&frags(*guest,"Gus")==1&&host->m_matchView.scores.front().name=="Gus","the frag counts on both machines and leads the scores");
  check(!guest->m_matchView.feed.empty()&&guest->m_matchView.feed.back().killer=="Gus"&&guest->m_matchView.feed.back().victim=="Hana","the kill feed says who killed whom");
  check(host->m_matchView.respawnIn>0&&host->m_matchView.status.find("Respawning in")==0,"the dead host counts down to respawning");
  const Point3 guestAt{guest->m_player.pos.x,guest->m_player.pos.y,guest->m_player.z};
  frames(int(3.3f/dt));
  {const auto& m=*host->m_match;float farthest=-1;for(const auto& s:m.m_spawns)farthest=std::max(farthest,std::hypot(s.world.x-guestAt.x,s.world.y-guestAt.y));
   const Vec2 o=host->chunkOffset(host->m_level);const float mine=std::hypot(host->m_player.pos.x+o.x-guestAt.x,host->m_player.pos.y+o.y-guestAt.y);
   check(!host->dead()&&host->m_player.health==100&&onSpawn(m,*host)&&std::fabs(mine-farthest)<.05f,"after 3 s the host respawns, full health, on the spawn farthest from the living guest");
   check(host->m_player.pools[1].total==72&&host->m_player.pools[1].loaded==6,"respawn gives the default loadout");}
  // The frag limit (2) ends the round; it starts again 10 s later.
  host->m_player.pos={8.5f,5.5f};put(*host,*guest,{8.5f,8.5f});frames(30);
  killWith(*guest,*host,4);frames(20);
  check(host->m_matchView.over&&guest->m_matchView.over&&host->m_matchView.winner=="Gus"&&guest->m_matchView.winner=="Gus","the frag limit ends the round on both machines with the winner named");
  check(guest->m_matchView.status.find("Gus wins")==0,"the guest's status names the winner and the restart");
  frames(int(10.5f/dt));
  check(!host->m_matchView.over&&!guest->m_matchView.over&&frags(*host,"Gus")==0&&frags(*guest,"Gus")==0&&!host->dead(),"10 s later the host restarts the round: scores cleared, everyone respawned");
  // A match does not stop for the pause menu, and nothing is saved in it.
  {host->m_paused=true;host->m_menuFromTitle=false;const float before=host->m_elapsed;frames(30);
   check(host->m_paused&&host->m_elapsed>before+.4f,"the host's pause menu is open and its world runs on");host->m_paused=false;
   check(!host->saveSlot(0)&&!host->saveCheckpoint()&&!guest->loadSlot(0),"no saving or loading in a match");}
  // A third player who has the map joins without a download.
  playing.push_back(third.get());frames(1);st->joinCode(sh->lobby().joinCode);
  until(120,[&]{return third->m_match&&third->m_match->started();});
  {std::error_code e;check(third->m_match&&third->m_match->started()&&!fs::exists(thirdMaps/"Downloaded",e),"a guest who has the same map plays it without a download");}
  frames(30);
  const int ts=slotOfGame(*third);
  check(remote(*guest,ts)&&remote(*third,gs)&&remote(*third,0),"the third player sees both and both see them");
  // One more kill for the record, then the host leaves: deathmatch goes on under the guest.
  host->m_player.pos={8.5f,5.5f};put(*host,*guest,{8.5f,8.5f});put(*host,*third,{15.5f,18.5f});frames(30);
  killWith(*guest,*host,4);frames(20);
  traffic:{
   const auto snap=host->m_match->lastSnapshotBytes();
   report<<"INFO measured snapshot with 3 players: "<<snap<<" bytes\n";
  }
  sh->leave();playing={guest.get(),third.get(),host.get()};frames(40);
  check(!host->m_match&&host->m_titleScreen,"the host who left is back at the title");
  check(guest->m_match&&guest->m_match->m_host&&guest->m_matchView.host&&third->m_match&&!third->m_match->m_host,"the lobby passed to the guest, whose match now hosts");
  check(frags(*guest,"Gus")==1&&frags(*third,"Gus")==1&&frags(*guest,"Hana")==-99,"the scores carry over and the old host is gone from them");
  float worstThird=0;walk(*third,{0,-1},2.f,1.f,&worstThird,guest.get(),ts);frames(30);
  {const auto* v=remote(*guest,ts);check(v&&gap(v->position,here(*third))<.05f&&worstThird<.6f,"the new host follows the third player's moves (worst "+std::to_string(worstThird)+" m)");}
  guest->m_player.pos={12.5f,15.5f};put(*guest,*third,{12.5f,18.5f});frames(30);
  third->m_player.pools[1]={72,6};aim(*third,{guest->m_player.pos.x,guest->m_player.pos.y,guest->m_player.z+1.f});third->shoot();frames(10);
  check(guest->m_player.health<100&&!guest->dead(),"the new host traces the third player's shot and takes the damage");
  playing.clear();
 }
 // ===== Co-op =====
 {
  LoopbackHub hub;auto sh=hub.connect("Hana"),sg=hub.connect("Gus");
  auto host=makeHost(),guest=makeGuest(guestMaps,false);
  host->setNetSession(sh.get());guest->setNetSession(sg.get());host->m_god=true;
  playing={host.get(),guest.get()};frames(1);
  HostOptions o;o.mode=Mode::Coop;o.map=arenaFile;o.mapHash=mapFileHash(hostMaps/arenaFile);sh->host(o);frames(1);
  sg->joinCode(sh->lobby().joinCode);
  until(240,[&]{return guest->m_match&&guest->m_match->started();});
  frames(30);
  check(guest->m_match&&guest->m_match->started()&&guest->m_matchView.mode==Mode::Coop,"co-op: the guest (with the map downloaded last time) joins the host's world");
  check(host->m_enemies.size()==2&&guest->m_enemies.size()==2&&guest->m_netCreaturesRemote,"the host's creatures are on the guest too, their minds left to the host");
  const auto* hv=remote(*guest,0);
  check(hv&&gap(hv->position,here(*host))<1.5f+.05f&&remote(*host,slotOfGame(*guest))&&remote(*host,slotOfGame(*guest))->team==1,"the guest starts beside the host, both on the co-op team");
  // The host kills the weak creature: it dies on the guest.
  host->m_player.pos={19.5f,13.5f};put(*host,*guest,{16.5f,17.5f});frames(20);
  {auto& weak=host->m_enemies[1];aim(*host,{weak.pos.x,weak.pos.y,weak.z+.3f});host->shoot();frames(20);
   check(!host->m_enemies[1].alive,"the host's own shot (through the match) kills the creature on the host");
   check(!guest->m_enemies[1].alive,"the creature killed on the host is dead on the guest");}
  {auto& strong=host->m_enemies[0];const float before=strong.hp;aim(*guest,{guest->m_enemies[0].pos.x,guest->m_enemies[0].pos.y,guest->m_enemies[0].z+.3f});guest->shoot();frames(20);
   check(host->m_enemies[0].hp<before,"a guest's shot damages the creature on the host ("+std::to_string(int(before))+" -> "+std::to_string(int(host->m_enemies[0].hp))+")");
   check(guest->m_enemies[0].hp<before,"and the guest sees its wound");}
  {host->m_god=false;host->m_player.pos={10.5f,17.5f};put(*host,*guest,{10.5f,20.5f});frames(20);aim(*guest,{host->m_player.pos.x,host->m_player.pos.y,host->m_player.z+1.f});guest->shoot();frames(20);
   host->m_god=true;check(host->m_player.health==100,"friendly fire off: the guest's round does not hurt the host");}
  {put(*host,*guest,{4.5f,19.5f});frames(20);
   check(!host->m_pickups.empty()&&!host->m_pickups[0].active&&!guest->m_pickups[0].active,"a pickup the guest takes is gone on the host too");}
  sh->leave();frames(10);
  check(!guest->m_match&&guest->m_titleScreen&&guest->m_matchView.status.find("host left")!=std::string::npos,"co-op ends for the guest when the host leaves, with a plain line: \""+guest->m_matchView.status+"\"");
  playing.clear();
 }
 // ===== A lossy, slow hub =====
 {
  LoopbackHub hub;hub.unreliableLossPercent=20;hub.latencyTicks=6;
  auto sh=hub.connect("Hana"),sg=hub.connect("Gus");auto host=makeHost(),guest=makeGuest(guestMaps,false);
  host->setNetSession(sh.get());guest->setNetSession(sg.get());playing={host.get(),guest.get()};frames(1);
  HostOptions o;o.mode=Mode::Deathmatch;o.map=arenaFile;o.mapHash=mapFileHash(hostMaps/arenaFile);sh->host(o);frames(1);
  sg->joinCode(sh->lobby().joinCode);until(240,[&]{return guest->m_match&&guest->m_match->started();});frames(30);
  const int gs=slotOfGame(*guest);
  host->m_player.pos={6.5f,6.5f};put(*host,*guest,{6.5f,9.5f});frames(30);
  float worstHost=0,worstGuest=0;walk(*host,{1,0},2.5f,1.f,&worstHost,guest.get(),0);walk(*guest,{1,0},2.5f,1.f,&worstGuest,host.get(),gs);frames(40);
  const auto* hv=remote(*guest,0);const auto* gv=remote(*host,gs);
  check(hv&&gv&&gap(hv->position,here(*host))<.05f&&gap(gv->position,here(*guest))<.05f,"20% loss, 6-tick latency: both converge on where the other stands");
  check(worstHost<1.f&&worstGuest<1.f,"and while moving stay within 1 m (worst "+std::to_string(worstHost)+" / "+std::to_string(worstGuest)+" m)");
  host->m_player.pos={8.5f,5.5f};put(*host,*guest,{8.5f,8.5f});frames(40);
  killWith(*guest,*host,4);frames(30);
  check(host->dead()&&frags(*guest,"Gus")==1,"shots still kill and score over the lossy hub");
  playing.clear();
 }
 // ===== A VR player's state survives the wire =====
 {NetPlayerState s;s.level=0;s.world={5.25f,7.5f,.25f};s.angle=1.f;s.pitch=-.3f;s.vr=true;s.weapon=2;s.shots=7;s.life=3;s.health=64;
  s.pose.active=true;s.pose.head={.1f,-.05f,1.5f};s.pose.headYaw=1.1f;s.pose.headPitch=-.2f;s.pose.bodyYaw=.9f;s.pose.eye=1.48f;s.pose.inHand=3;s.pose.support=true;
  s.pose.hands[1].tracked=true;s.pose.hands[1].wrist={.3f,-.2f,1.1f};s.pose.hands[1].forward={1,0,0};s.pose.hands[1].curls={0,.5f,1,1,1};
  wire::Writer w;writePlayerState(w,s,{},true);wire::Reader r(w.b);NetPlayerState t;const bool read=readPlayerState(r,t,true);
  auto near=[](Point3 a,Point3 b,float e){return std::fabs(a.x-b.x)<e&&std::fabs(a.y-b.y)<e&&std::fabs(a.z-b.z)<e;};
  check(read&&r.left()==0&&near(t.world,s.world,.005f)&&std::fabs(std::remainder(t.angle-s.angle,kTwoPi))<.001f&&t.vr&&t.weapon==2&&t.shots==7&&t.life==3&&t.health==64
   &&near(t.pose.head,s.pose.head,.001f)&&t.pose.inHand==3&&t.pose.support&&t.pose.hands[1].tracked&&near(t.pose.hands[1].wrist,s.pose.hands[1].wrist,.001f)&&std::fabs(t.pose.hands[1].curls[1]-.5f)<.01f,
   "a VR player's state and avatar pose come through the wire ("+std::to_string(w.b.size())+" bytes)");}
 // ===== Sizes =====
 const auto snap8=snapshotBytes(8,false),snap8vr=snapshotBytes(8,true),up=guestStateBytes(false),upvr=guestStateBytes(true);
 report<<"INFO snapshot of 8 players: "<<snap8<<" bytes ("<<snap8vr<<" if all 8 are in VR); a guest's state "<<up<<" bytes ("<<upvr<<" in VR)\n";
 const float down=float(snap8)*20/1024,upload=float(up)*30/1024;
 report<<"INFO per guest at 8 players: "<<down<<" KB/s down (snapshots at 20 Hz), "<<upload<<" KB/s up (state at 30 Hz); the host sends "<<down*7<<" KB/s in all\n";
 check(down+upload<20,"a guest's traffic at 8 players stays under 20 KB/s");
 const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
 report<<"INFO the test took "<<seconds<<" s\n";
 return ok;
}
}

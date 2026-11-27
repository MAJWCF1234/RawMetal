#include "Match.h"
#include "Loopback.h"
#include "MatchProtocol.h"
#include "Wire.h"
#include "../game/Game.h"
#include "../game/physics/PropBodies.h"
#include "../lab/LabAccess.h"
#include "../world/CustomCampaign.h"
#include "../world/CampaignExport.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <ostream>

// The audio test includes Windows headers, whose legacy qualifiers collide with distance names below.
#undef near
#undef far
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
// Co-op's one world: a wall across the middle (a gap at x 11-12), a pad and a door to the north, a gathering spot
// that wants every player and a lever to the south, and a guard (off until guard_on) who walks the north side.
std::string coopText(){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nFORMAT|2\nCAMPAIGN|Net Coop|0\nMAP|0|Coop|0|0|3.5|16.5|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row){std::string r(24,'.');r.front()=r.back()='#';if(row==0||row==23)r=std::string(24,'#');if(row==12)for(int x=1;x<23;++x)if(x!=11&&x!=12)r[size_t(x)]='#';
  t+="ROW|0|0|"+std::to_string(row)+"|"+r+"\n";}
 t+="DOOR|0|17|19|6.5|0|0|0|0|0|-1\n";
 t+="SCRIPT_EVENT|0|pad|20|2|22|4|-10|10||1|0\nSCRIPT_ACTION|0|pad|STATE|pad_hit|1\nSCRIPT_ACTION|0|pad|MESSAGE|PAD PRESSED|3\nSCRIPT_ACTION|0|pad|RAW|4||0|0|0|0|0|0|0|0|\n";
 t+="SCRIPT_EVENT|0|gather|2|19|5|22|-10|10||1|0\nSCRIPT_TRIGGER|0|gather|box|all|0|players=all\nSCRIPT_ACTION|0|gather|STATE|gathered|1\n";
 t+="TERMINAL|0|12|20|0|0|Lever|Pull|it|lever_on|0\n";
 t+="TIMED_SEQUENCE|0|clock|done|0|0|24|24|-0.2|8|30000|30000|0|0|0|0|1|1\nACTOR|0|clock|4|1.6|0|0|0\n"
    "ACTOR_KEY|0|0|0|5.5|5.5|0|1.57|0|0\nACTOR_KEY|0|0|30000|5.5|5.5|0|1.57|0|1\nACTOR_AI|0|0|3|guard_on|1.4|2|6\nACTOR_GUARD|0|0|11|0.65|600|4000|alert\n"
    "ACTOR_PATROL|0|0|5.5|5.5|0|300\nACTOR_PATROL|0|0|18.5|5.5|0|300\nACTOR_HEALTH|0|0|90|g_damage|g_dead\nACTOR_WEAPON|0|0|2|650|1000|9|18\n";
 return t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";
}
// Several maps: West (0) and South (1) side by side on the grid with an open seam between (x 4-19 at y 24), the
// Annex (2) apart from both (the test then puts it on West's own origin: maps that do not share one grid), the Yard
// (3) far east with fifty crates. On West: a crate, a glass bottle of water and a small box; on the Annex a crate.
std::string mapsText(){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nFORMAT|2\nCAMPAIGN|Net Maps|0\n";
 const char* names[]={"West","South","Annex","Yard"};const int ox[]={0,0,48,96},oy[]={0,24,48,0};
 std::string seam(24,'#');for(int x=4;x<20;++x)seam[size_t(x)]='.';
 for(int m=0;m<4;++m){const std::string M=std::to_string(m);
  t+="MAP|"+M+"|"+names[m]+"|"+std::to_string(ox[m])+"|"+std::to_string(oy[m])+"|3.5|16.5|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|"+M+"|0|Floor|0|0\n";
  for(int row=0;row<24;++row){std::string r(24,'.');r.front()=r.back()='#';
   if(row==0)r=m==1?seam:std::string(24,'#');if(row==23)r=m==0?seam:std::string(24,'#');
   t+="ROW|"+M+"|0|"+std::to_string(row)+"|"+r+"\n";}}
 t+="CLUTTER|0|7|12|8|-999|0|17|0.6|0.6|0.6|0|0|#0\nCLUTTER|0|7|15|15|-999|0|384|0.1|0.1|0.36|0|0|#0|p.liquid=water|p.fill=0.8\nCLUTTER|0|7|9|12|-999|0|17|0.3|0.3|0.3|0|0|#0\n";
 t+="CLUTTER|2|7|12|8|-999|0|17|0.6|0.6|0.6|0|0|#0\n";
 t+="FIXTURE|2|"+std::to_string(skinbot::Model)+"|7|10|0|2.363|3.099|2.4|1.5708|0|skinbot.route=20,10;7,10\n";
 for(int k=0;k<50;++k)t+="CLUTTER|3|7|"+std::to_string(3.f+float(k%10)*1.8f)+"|"+std::to_string(5.f+float(k/10)*1.8f)+"|-999|0|17|0.4|0.4|0.4|0|0|#0\n";
 return t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";
}
std::string lobbyText(){
 std::string t="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Lobby|0\nMAP|0|Lobby|0|0|3.5|3.5|0|0|3.4|0.3|industrial_night|0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
 for(int row=0;row<24;++row)t+="ROW|0|0|"+std::to_string(row)+"|"+(row==0||row==23?std::string(24,'#'):"#......................#")+"\n";
 return t+"--- CUSTOM_CAMPAIGN_DATA_END ---\n";
}

bool writeFootstepPreview(const std::vector<int16_t>& closeGuest,const std::vector<int16_t>& distantGuest,const std::vector<int16_t>& closeHost){
 std::ofstream wav("net-footsteps-preview.wav",std::ios::binary);
 const auto bytes=uint32_t((closeGuest.size()+distantGuest.size()+closeHost.size())*sizeof(int16_t));
 if(!wav||!bytes)return false;
 auto number=[&](auto value){wav.write(reinterpret_cast<const char*>(&value),sizeof(value));};
 wav.write("RIFF",4);number(uint32_t(bytes+36));
 wav.write("WAVEfmt ",8);number(uint32_t(16));
 number(uint16_t(1));number(uint16_t(2));number(uint32_t(44100));
 number(uint32_t(44100*4));number(uint16_t(4));number(uint16_t(16));
 wav.write("data",4);number(bytes);
 for(const auto* clip:{&closeGuest,&distantGuest,&closeHost})
  wav.write(reinterpret_cast<const char*>(clip->data()),std::streamsize(clip->size()*sizeof(int16_t)));
 wav.close();
 return bool(wav);
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
 auto arm=[&](Game& g){g.m_shotgunOwned=true;g.m_player.pools[1]={72,6};g.equipWeapon(WeaponId::Shotgun);};
 auto killWith=[&](Game& shooter,Game& victim,int shots){arm(shooter);
  for(int i=0;i<shots&&!victim.dead();++i){aim(shooter,{victim.m_player.pos.x,victim.m_player.pos.y,victim.m_player.z+1.f});shooter.shoot();frames(8);}};

 // ===== Deathmatch =====
 {
  LoopbackHub hub;auto sh=hub.connect("Hana"),sg=hub.connect("Gus"),st=hub.connect("Tam");
  auto host=makeHost(),guest=makeGuest(guestMaps,false),third=makeGuest(thirdMaps,true);
  host->setNetSession(sh.get());guest->setNetSession(sg.get());third->setNetSession(st.get());
  guest->m_settings.character=2;   // the guest plays Leon (the wire's one byte carries 0 Claire, 1 the Limitator, 2 Leon)
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
  check(remote(*host,gs)&&remote(*host,gs)->character==2&&remote(*guest,0)&&remote(*guest,0)->character==0,"the character rides the wire: the host sees the guest as Leon (2), the guest sees the host as Claire (0)");
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
  // Footsteps: a remote walker is heard on the listener's machine, made from the replicated state (no packet per
  // step): one per stride, at the walker's feet, on the floor's sound, loud near and fading with distance (the
  // AudioEngine's spatial curve, gain*(1-d/radius)^2), quieter crouched and prone, louder sprinting, none at rest
  // and none echoed on the walker's own machine.
  {struct Heard{int steps=0,lands=0,echoes=0,offFloor=0,misplaced=0,pcmPeak=0;float level=0,peak=0;std::vector<int16_t> pcm;};
   AudioEngine audio(false);
   auto audible=[](const SoundEvent& e,const Game& ear){const float dx=e.position.x-ear.m_player.pos.x,dy=e.position.y-ear.m_player.pos.y,dz=e.z-(ear.m_player.z+ear.m_player.eye);
    const float d=std::sqrt(dx*dx+dy*dy+dz*dz);return e.gain*std::pow(std::max(0.f,1-d/e.radius),2.f);};
   // The walker walks (its own movement, the keys held) along dir for seconds in a stance (0 standing, 1 crouched,
   // 2 prone, 3 sprinting); the listener counts what it hears.
   auto stroll=[&](Game& walker,Game& ear,int walkerSlot,Vec2 dir,float seconds,int stance){
    Heard h;InputState in{};in.forward=true;in.crouch=stance==1||stance==2;in.sprint=stance==3;walker.m_player.angle=std::atan2(dir.y,dir.x);
    if(stance==2){InputState z{};z.prone=true;walker.update(z,dt);}   // the prone toggle (Z), then held low
    for(int i=0;i<int(seconds/dt);++i){
     for(auto* g:playing)g->update(g==&walker?in:InputState{},dt);
     for(const auto& e:walker.m_sounds)if(e.spatial&&(footstepSound(e.sound)||e.sound==Sound::Land))++h.echoes;
     const RemotePlayer* r=remote(ear,walkerSlot);
     for(const auto& e:ear.m_sounds){if(!e.spatial||!(footstepSound(e.sound)||e.sound==Sound::Land))continue;
      if(e.sound==Sound::Land){++h.lands;continue;}
      ++h.steps;const int base=int(Game::stepSoundAt(ear.m_world,e.position));h.offFloor+=int(e.sound)<base||int(e.sound)>base+3;
      h.misplaced+=!r||std::hypot(e.position.x-r->position.x,e.position.y-r->position.y)>.3f||std::fabs(e.z-(r->position.z+.04f))>.05f;
      if(h.steps==1){audio.update(ear);LabAccess::onlySound(audio,e.sound);LabAccess::mix(audio,h.pcm,44100);
       for(int16_t sample:h.pcm)h.pcmPeak=std::max(h.pcmPeak,std::abs(int(sample)));}
      const float a=audible(e,ear);h.level+=a;h.peak=std::max(h.peak,a);}}
    if(stance==2){InputState z{};z.prone=true;walker.update(z,dt);}
    frames(30);if(h.steps)h.level/=float(h.steps);return h;};
   auto line=[](const char* what,const Heard& h){return std::string(what)+" "+std::to_string(h.steps)+" steps, mean level "+std::to_string(h.level)+", peak "+std::to_string(h.peak)+", first step PCM peak "+std::to_string(h.pcmPeak);};
   host->m_player.pos={3.5f,5.5f};put(*host,*guest,{3.5f,7.5f});frames(40);
   const Heard near=stroll(*host,*guest,0,{1,0},2.5f,0);
   report<<"  "<<line("near walk (2-9 m):",near)<<'\n';
   check(near.steps>=6&&near.steps<=8,"the guest hears the host's walk: 2.5 s at 3.6 m/s (~8.5 m, a stride of 1.22 m) is "+std::to_string(near.steps)+" steps (6-8)");
   check(!near.misplaced&&!near.offFloor,"every remote step sounds at the walker's drawn feet, on the floor's own step sounds");
   check(!near.echoes,"the walker's own machine does not hear its steps again as a remote player's");
   {int quiet=0;for(int i=0;i<60;++i){frames(1);for(const auto& e:guest->m_sounds)quiet+=e.spatial&&footstepSound(e.sound);}
    check(!quiet,"a remote player standing still makes no footsteps");}
   host->m_player.pos={3.5f,20.5f};put(*host,*guest,{3.5f,5.5f});frames(40);
   const Heard far=stroll(*host,*guest,0,{1,0},2.5f,0);
   report<<"  "<<line("far walk (15-17 m):",far)<<'\n';
   check(far.steps==near.steps||std::abs(far.steps-near.steps)<=1,"a far walk makes as many steps (the stride does not depend on the listener)");
   check(near.level>.05f&&far.level<near.level*.1f,"the steps fade with distance: mean level "+std::to_string(near.level)+" at 2-9 m, "+std::to_string(far.level)+" at 15-17 m");
   host->m_player.pos={3.5f,5.5f};put(*host,*guest,{3.5f,7.5f});frames(40);
   const Heard crouched=stroll(*host,*guest,0,{1,0},3.f,1);
   report<<"  "<<line("crouched walk:",crouched)<<'\n';
   check(crouched.steps>=4&&crouched.level<near.level*.25f,"a crouched walk is heard in shorter strides and far quieter ("+std::to_string(crouched.level)+" against "+std::to_string(near.level)+")");
   host->m_player.pos={3.5f,5.5f};put(*host,*guest,{3.5f,7.5f});frames(40);
   const Heard prone=stroll(*host,*guest,0,{1,0},3.f,2);
   report<<"  "<<line("prone crawl:",prone)<<'\n';
   check(prone.steps>=3&&prone.peak<crouched.peak,"a crawl is quieter still");
   host->m_player.pos={3.5f,5.5f};put(*host,*guest,{3.5f,7.5f});frames(40);
   const Heard sprint=stroll(*host,*guest,0,{1,0},1.6f,3);
   report<<"  "<<line("sprint:",sprint)<<'\n';
   check(sprint.steps>=4&&sprint.level>near.level,"a sprint is heard louder than a walk");
   // The other way: the host hears the guest (the guest's own report, through the host).
   host->m_player.pos={3.5f,7.5f};put(*host,*guest,{3.5f,5.5f});frames(40);
   const Heard back=stroll(*guest,*host,gs,{1,0},2.5f,0);
   report<<"  "<<line("guest walking, host listening:",back)<<'\n';
   check(back.steps>=6&&back.steps<=8&&!back.misplaced&&!back.echoes,"the host hears the guest's steps the same way");
   check(near.pcmPeak>0&&back.pcmPeak>0&&far.pcmPeak<near.pcmPeak,"the embedded footstep samples produce positional PCM for both listeners and fade at distance");
   if(writeFootstepPreview(near.pcm,far.pcm,back.pcm))
    report<<"INFO net-footsteps-preview.wav: guest hears host close, guest hears host far, host hears guest close (one second each)\n";
   // A jump: up in the air, then down. The landing thumps on the listener's machine.
   {host->m_player.pos={3.5f,5.5f};put(*host,*guest,{3.5f,7.5f});frames(40);int lands=0;InputState j{};j.jump=true;
    for(int i=0;i<90;++i){for(auto* g:playing)g->update(g==host.get()&&i<3?j:InputState{},dt);for(const auto& e:guest->m_sounds)lands+=e.spatial&&e.sound==Sound::Land;}
    check(lands==1,"a remote player's landing after a jump is heard once ("+std::to_string(lands)+")");}
  }
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
   check(host->m_player.pools[1].total==0&&!host->ownsWeapon(WeaponId::Shotgun)&&!host->hasHandgun()&&!host->hasMachinePistol()&&host->unarmed(),"a deathmatch respawn gives nothing: fists, no gun, no rounds");}
  // The guns lie on the map: one picked up arms its taker and leaves every screen, and is back 30 s later.
  {const Point3 spot{host->m_player.pos.x,host->m_player.pos.y,0};const Vec2 home=host->m_player.pos;
   host->m_pickups.assign(1,Pickup{{home.x+1.5f,home.y},Pickup::Kind::Shotgun,true,-999});guest->m_pickups=host->m_pickups;
   host->m_player.pos={home.x+1.5f,home.y};frames(6);host->m_player.pos=home;
   check(host->ownsWeapon(WeaponId::Shotgun)&&host->m_player.pool(WeaponId::Shotgun).loaded>0&&(host->m_switchTarget==WeaponId::Shotgun||host->m_equippedWeapon==WeaponId::Shotgun),"picking up the shotgun arms the player: the gun, loaded, in hand");
   check(!host->m_pickups[0].active&&!guest->m_pickups[0].active,"the taken shotgun is gone from the other player's map too");
   frames(int(20.f/dt));check(!host->m_pickups[0].active&&!guest->m_pickups[0].active,"20 s on it is still gone");
   frames(int(12.f/dt));check(host->m_pickups[0].active&&guest->m_pickups[0].active,"30 s after it was taken the shotgun lies there again, on both machines");(void)spot;}
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
  // Other players' footsteps are heard: a player walking on another machine's map is heard stepping there, once a
  // stride, and a player standing still is not.
  {int heard=0;const auto count=[&](Game& g,Vec2 near){for(const auto& s:g.m_sounds)if((s.position.x-near.x)*(s.position.x-near.x)+(s.position.y-near.y)*(s.position.y-near.y)<4.f)++heard;};
   // The third player walks by real input (forward, facing north), as a player does: the game works out the velocity it sends.
   put(*guest,*third,{4.5f,12.5f});frames(20);third->m_player.angle=0;   // in open floor, facing east
   const int n=int(1.5f/dt);for(int i=0;i<n;++i){for(auto* g:playing){InputState in{};if(g==third.get())in.forward=true;g->update(in,dt);}count(*guest,Vec2{here(*third).x,here(*third).y});}
   const int walking=heard;heard=0;for(int i=0;i<int(1.f/dt);++i){frames(1);count(*guest,Vec2{here(*third).x,here(*third).y});}
   check(walking>=2,"another player's footsteps are heard as they walk ("+std::to_string(walking)+" in 1.5 s)");
   check(heard==0,"and none while they stand still ("+std::to_string(heard)+")");}
  guest->m_player.pos={12.5f,15.5f};put(*guest,*third,{12.5f,18.5f});frames(30);
  arm(*third);aim(*third,{guest->m_player.pos.x,guest->m_player.pos.y,guest->m_player.z+1.f});third->shoot();frames(10);
  check(guest->m_player.health<100&&!guest->dead(),"the new host traces the third player's shot and takes the damage");
  playing.clear();
 }
 // ===== Chat =====
 {
  LoopbackHub hub;auto sh=hub.connect("Hana"),sg=hub.connect("Gus"),st=hub.connect("Tam");
  auto host=makeHost(),guest=makeGuest(thirdMaps,true),third=makeGuest(thirdMaps,true);
  host->setNetSession(sh.get());guest->setNetSession(sg.get());third->setNetSession(st.get());
  playing={host.get(),guest.get(),third.get()};frames(1);
  HostOptions o;o.mode=Mode::Deathmatch;o.map=arenaFile;o.mapHash=mapFileHash(hostMaps/arenaFile);sh->host(o);frames(1);
  sg->joinCode(sh->lobby().joinCode);until(120,[&]{return guest->m_match&&guest->m_match->started();});frames(20);
  st->joinCode(sh->lobby().joinCode);until(120,[&]{return third->m_match&&third->m_match->started();});frames(20);
  auto said=[](const Game& g,const std::string& from,const std::string& text){for(const auto& c:g.m_matchView.chat)if(!c.notice&&c.from==from&&c.text==text)return true;return false;};
  auto told=[](const Game& g,const std::string& text){for(const auto& c:g.m_matchView.chat)if(c.notice&&c.text==text)return true;return false;};
  auto count=[](const Game& g,const std::string& prefix){int n=0;for(const auto& c:g.m_matchView.chat)n+=!c.notice&&c.text.rfind(prefix,0)==0;return n;};
  check(told(*host,"Gus joined the game")&&told(*host,"Tam joined the game")&&told(*guest,"Tam joined the game"),"join notices appear in the chat box (host and the earlier guest)");
  check(!told(*third,"Gus joined the game")&&!told(*third,"Tam joined the game"),"a newcomer is not told about those already down here, nor about itself");
  check(guest->sendChat("  hello   down there  "),"a guest's line is accepted");frames(4);
  check(said(*host,"Gus","hello down there")&&said(*third,"Gus","hello down there")&&said(*guest,"Gus","hello down there"),"a guest's line reaches the host and the other guest (and comes back to the sender), cleaned");
  {bool self=false;for(const auto& c:guest->m_matchView.chat)self|=c.self&&c.from=="Gus";check(self,"the sender's own line is marked as theirs");}
  guest->sendChat(std::string(200,'a'));frames(4);
  {size_t longest=0;for(const auto& c:host->m_matchView.chat)if(!c.notice)longest=std::max(longest,c.text.size());check(longest==Match::ChatMax,"a long line is cut to 120 characters ("+std::to_string(longest)+")");}
  frames(int(7.f/dt));   // the allowance refills
  host->sendChat("what the fuck is that shit, Scunthorpe");frames(4);
  guest->m_settings.chatFilter=false;frames(2);
  check(said(*third,"Hana","what the **** is that ****, Scunthorpe")&&said(*guest,"Hana","what the fuck is that shit, Scunthorpe"),"the chat filter masks listed words for whoever has it on (and leaves Scunthorpe alone)");
  third->setPlayerMuted("Gus",true);frames(15);
  check(!said(*third,"Gus","hello down there")&&said(*host,"Gus","hello down there"),"a muted sender's lines are hidden from the one who muted them, not from the others");
  check(st->mutedPeers.count(sg->self())&&st->mutedPeers[sg->self()],"muting by name mutes the player's voice in the session too");
  third->setPlayerMuted("Gus",false);frames(15);check(said(*third,"Gus","hello down there")&&!st->mutedPeers[sg->self()],"unmuting shows the lines again and lets the voice through");
  // Typing takes the keys: a chat line full of I and F neither opens the inventory nor works the flashlight.
  {Game& g=*third;const bool light=g.flashlightOn();
   InputState open{};open.chat=true;g.update(open,dt);g.update({},dt);
   check(g.chatOpen(),"T opens the chat line in a match");
   for(int i=0;i<4;++i){InputState typed{};typed.textInput="fi";typed.inventory=true;typed.flashlight=true;g.update(typed,dt);g.update({},dt);}
   check(g.chatOpen()&&!g.m_inventoryOpen&&g.flashlightOn()==light,"typing I and F into the chat line opens no inventory and leaves the flashlight be");
   InputState shut{};shut.escape=true;g.update(shut,dt);g.update({},dt);
   check(!g.chatOpen()&&!g.m_inventoryOpen,"Escape closes the chat line, the inventory still shut");}
  {third->m_settings.textChat=false;frames(2);int lines=0;for(const auto& c:third->m_matchView.chat)lines+=!c.notice;
   check(lines==0,"text chat off hides the players' lines");third->m_settings.textChat=true;}
  // The allowance: a burst of ten from a guest, by the guest's own count and then past it straight to the host.
  frames(int(7.f/dt));
  {int accepted=0;for(int i=0;i<10;++i)accepted+=guest->sendChat("spam "+std::to_string(i));frames(4);
   check(accepted==4&&count(*host,"spam ")==4&&count(*third,"spam ")==4,"a spam burst of 10 is cut to 4 lines (accepted "+std::to_string(accepted)+", the host shows "+std::to_string(count(*host,"spam "))+")");
   bool warned=false;for(const auto& c:guest->m_matchView.chat)warned|=c.notice&&c.text.find("Too fast")==0;check(warned,"the sender is told to wait");}
  frames(int(7.f/dt));
  {for(int i=0;i<10;++i){wire::Writer w;w.u8(protocol::ChatSay).str("burst "+std::to_string(i));st->send(sh->self(),Events,w.b.data(),w.b.size(),true);}frames(4);
   check(count(*host,"burst ")==4&&count(*guest,"burst ")==4,"the host holds a guest to the same allowance however the lines are sent ("+std::to_string(count(*host,"burst "))+" of 10 relayed)");}
  // A kill makes a notice.
  host->m_player.pos={8.5f,5.5f};put(*host,*guest,{8.5f,8.5f});frames(30);
  killWith(*host,*guest,4);frames(10);
  check(told(*third,"Hana killed Gus")&&told(*guest,"Hana killed Gus"),"a kill is a notice in the chat box");
  // Speaking: Gus holds push-to-talk; the others' scoreboards and name tags show it.
  {InputState talk{};talk.talk=true;for(int i=0;i<4;++i){host->update({},dt);guest->update(talk,dt);third->update({},dt);}
   bool board=false,tag=false;for(const auto& r:host->m_matchView.scores)board|=r.name=="Gus"&&r.speaking;for(const auto& r:host->m_remotePlayers)tag|=r.name=="Gus"&&r.speaking;
   bool selfRow=false;for(const auto& r:guest->m_matchView.scores)selfRow|=r.self&&r.speaking;
   check(board&&tag&&selfRow,"push-to-talk held: the speaker is marked on the others' scoreboard and name tag, and on their own row");
   frames(2);bool quiet=true;for(const auto& r:host->m_matchView.scores)quiet&=!r.speaking;check(quiet,"let go, nobody is marked speaking");}
  {third->m_settings.voiceMode=0;InputState talk{};talk.talk=true;for(int i=0;i<3;++i){host->update({},dt);guest->update(talk,dt);third->update({},dt);}
   bool heard=false;for(const auto& r:third->m_matchView.scores)heard|=r.name=="Gus"&&r.speaking;check(!heard&&st->voice().mode==VoiceMode::Off,"voice off: the settings reach the session and nobody is heard");third->m_settings.voiceMode=1;}
  {float volume=-1;frames(15);if(sh->peerVolumes.count(sg->self()))volume=sh->peerVolumes[sg->self()];check(volume==1.f,"deathmatch: voices are heard in full, whatever the distance");}
  st->leave();frames(10);
  check(told(*host,"Tam left the game")&&told(*guest,"Tam left the game"),"a leave notice appears");
  playing.clear();
 }
 // ===== Deathmatch: the sniper rifle (a head hit takes the head multiplier on a player too; a body hit leaves 5 of 100) =====
 {
  LoopbackHub hub;auto sh=hub.connect("Hana"),sg=hub.connect("Gus");
  auto host=makeHost(),guest=makeGuest(guestMaps,false);
  host->setNetSession(sh.get());guest->setNetSession(sg.get());
  playing={host.get(),guest.get()};frames(1);
  HostOptions o;o.mode=Mode::Deathmatch;o.map=arenaFile;o.mapHash=mapFileHash(hostMaps/arenaFile);o.maxPlayers=8;sh->host(o);frames(1);
  sg->joinCode(sh->lobby().joinCode);
  until(240,[&]{return guest->m_match&&guest->m_match->started();});frames(30);
  check(guest->m_match&&guest->m_match->started(),"sniper test: the guest joins the host's deathmatch");
  host->m_player.pos={8.5f,5.5f};put(*host,*guest,{8.5f,10.5f});frames(30);
  guest->grantSniper(5);guest->selectWeapon(WeaponId::Sniper);frames(90);check(guest->equippedWeapon()==WeaponId::Sniper,"the guest has the rifle drawn");
  // A round into the head: one is enough (100 health against 95 x 3).
  aim(*guest,{host->m_player.pos.x,host->m_player.pos.y,host->m_player.z+host->m_player.eye*.97f});guest->shoot();frames(10);
  check(host->dead(),"deathmatch: one sniper round into the head kills a full-health player (traced by the host)");
  frames(int(3.4f/dt));
  check(!host->dead()&&host->m_player.health==100,"the host respawns at full health");
  // A round into the body: 95 of 100.
  put(*host,*guest,{8.5f,10.5f});host->m_player.pos={8.5f,5.5f};frames(30);guest->m_aimBlend=1.f;
  aim(*guest,{host->m_player.pos.x,host->m_player.pos.y,host->m_player.z+host->m_player.eye*.5f});guest->shoot();frames(10);
  check(!host->dead()&&host->m_player.health>0&&host->m_player.health<=10,"a sniper round into the body leaves the player on "+std::to_string(int(host->m_player.health))+" of 100");
  sh->leave();frames(10);playing.clear();
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
  // Footsteps in co-op: the guest walking is heard on the host with its floor-and-gait reach for the minds (they hear
  // it as the local player's step: Game::aiStimuli), and the host walking is heard on the guest (no minds there).
  {host->m_player.pos={3.5f,13.5f};put(*host,*guest,{3.5f,15.5f});frames(30);
   // Only steps at the walker (the map's creatures walk too).
   auto walkBy=[&](Game& walker,Game& ear,int& steps,int& forMinds,int& stimuli){walker.m_player.angle=0;InputState in{};in.forward=true;const Point3 from=here(walker);
    for(int i=0;i<int(2.f/dt);++i){for(auto* g:playing)g->update(g==&walker?in:InputState{},dt);
     const Point3 at=here(walker);auto near=[&](Vec2 p){return std::hypot(p.x-at.x,p.y-at.y)<1.5f;};
     for(const auto& e:ear.m_sounds)if(e.spatial&&footstepSound(e.sound)&&near(e.position)){++steps;forMinds+=e.aiRadius>0;}
     for(const auto& a:ear.aiStimuli())stimuli+=a.step&&near(a.source);}
    report<<"  co-op walk: "<<gap(from,here(walker))<<" m walked, "<<steps<<" steps heard, "<<stimuli<<" step stimuli\n";
    frames(30);};
   int steps=0,forMinds=0,stimuli=0;walkBy(*guest,*host,steps,forMinds,stimuli);
   check(steps>=4&&forMinds==steps&&stimuli>=steps,"co-op: the host hears the guest's "+std::to_string(steps)+" steps, every one with its reach for the minds ("+std::to_string(stimuli)+" step stimuli)");
   int back=0,backMinds=0,backStimuli=0;host->m_player.pos={3.5f,15.5f};put(*host,*guest,{3.5f,13.5f});frames(30);walkBy(*host,*guest,back,backMinds,backStimuli);
   check(back>=3&&backMinds==0,"co-op: the guest hears the host's "+std::to_string(back)+" steps (for the ears only: the guest has no minds)");
   host->m_player.pos={3.5f,13.5f};put(*host,*guest,{3.5f,15.5f});frames(30);
   InputState forward{};forward.forward=true;guest->m_player.angle=0;
   for(int i=0;i<30;++i){host->update({},dt);guest->update(forward,dt);}
   hub.unreliableLossPercent=100;
   int staleSteps=0;
   for(int i=0;i<90;++i){host->update({},dt);guest->update(forward,dt);
    if(i>=30)for(const auto& e:host->m_sounds)staleSteps+=footstepSound(e.sound)&&e.aiRadius>0;}
   hub.unreliableLossPercent=0;frames(30);
   check(staleSteps==0,"co-op: a guest's frozen network position falls silent when movement reports stop");}
  // Proximity voice: each voice's volume follows the distance and the wall across the middle.
  {auto volume=[&]{frames(20);return sh->peerVolumes.count(sg->self())?sh->peerVolumes[sg->self()]:-1.f;};
   host->m_player.pos={12.5f,6.5f};put(*host,*guest,{13.5f,6.5f});const float near=volume();
   put(*host,*guest,{20.5f,6.5f});const float open=volume();
   put(*host,*guest,{12.5f,18.5f});const float walled=volume();
   host->m_settings.proximityVoice=false;const float off=volume();host->m_settings.proximityVoice=true;
   check(near==1.f&&open>.5f&&open<.9f&&walled>0&&walled<.25f&&off==1.f,"co-op proximity voice: beside "+std::to_string(near)+", 8 m in the open "+std::to_string(open)+", 12 m behind the wall "+std::to_string(walled)+", proximity off "+std::to_string(off));}
  sh->leave();frames(10);
  check(!guest->m_match&&guest->m_titleScreen&&guest->m_matchView.status.find("host left")!=std::string::npos,"co-op ends for the guest when the host leaves, with a plain line: \""+guest->m_matchView.status+"\"");
  playing.clear();
 }
 // ===== Co-op: one world for every player (scripts, creatures, guards over Game::players()) =====
 {
  const std::string coopFile="Net Coop.rmc";
  {auto parsed=loadCustomCampaignText(coopText(),"coop.rmc");const ScriptEvent* gather=nullptr;for(const auto& e:parsed->maps[0]->scriptEvents)if(e.id==stateId("gather"))gather=&e;
   std::string bad=coopText();bad.replace(bad.find("players=all"),11,"players=some");bool refused=false;try{loadCustomCampaignText(bad,"coop.rmc");}catch(const std::exception&){refused=true;}
   const auto text=exportRmc("Net Coop",{{parsed->maps[0],0}}).text;
   check(gather&&gather->allPlayers&&refused&&text.find("players=all")!=std::string::npos,"SCRIPT_TRIGGER players=all parses, players=some is refused, and the export writes it back");}
  {std::ofstream(hostMaps/coopFile,std::ios::binary)<<coopText();std::ofstream(guestMaps/coopFile,std::ios::binary)<<coopText();}
  LoopbackHub hub;auto sh=hub.connect("Hana"),sg=hub.connect("Gus");
  auto host=std::make_unique<Game>(loadCustomCampaignFile(hostMaps/coopFile));host->m_customMapDirectory=hostMaps.wstring();
  auto guest=makeGuest(guestMaps,false);guest->m_customCampaigns.push_back(loadCustomCampaignFile(guestMaps/coopFile));
  host->setNetSession(sh.get());guest->setNetSession(sg.get());host->m_god=true;
  playing={host.get(),guest.get()};frames(1);
  HostOptions o;o.mode=Mode::Coop;o.map=coopFile;o.mapHash=mapFileHash(hostMaps/coopFile);sh->host(o);frames(1);
  sg->joinCode(sh->lobby().joinCode);
  until(240,[&]{return guest->m_match&&guest->m_match->started();});frames(30);
  const int gs=slotOfGame(*guest);
  check(guest->m_match&&guest->m_match->started()&&gs>0&&host->players().size()==2&&guest->players().size()==1,
   "co-op world: the host's minds see two players (itself and the guest's body); single player and a guest see one");
  {const auto* gp=host->playerById(gs);check(gp&&!gp->local&&gap({gp->body->pos.x,gp->body->pos.y,gp->body->z},here(*guest))<.05f,"the guest's body on the host stands where the guest does");}
  // --- Scripts that know every player ---
  host->m_player.pos={6.5f,16.5f};put(*host,*guest,{21.f,3.f});frames(30);
  check(host->state(stateId("pad_hit"))==1&&!host->state(stateId("gathered")),"the guest stepping onto the pad fires its box trigger on the host (the host stands elsewhere)");
  check(guest->state(stateId("pad_hit"))==1,"the state it set reaches the guest");
  check(!host->world().doors().empty()&&host->world().doors()[0].opening&&!guest->world().doors().empty()&&guest->world().doors()[0].opening,"the door it opened opens on the guest too");
  check(guest->pickupNotice()=="PAD PRESSED","its caption shows on the guest (\""+guest->pickupNotice()+"\")");
  put(*host,*guest,{3.5f,20.5f});frames(30);
  const bool waited=!host->state(stateId("gathered"));
  host->m_player.pos={3.f,21.f};frames(30);
  check(waited&&host->state(stateId("gathered"))==1&&guest->state(stateId("gathered"))==1,"a players=all trigger waits for the second player, then fires (and the guest knows)");
  // A guest's hand on the lever: worked by the host, its switch known to both.
  {put(*host,*guest,{12.f,19.f});host->m_player.pos={6.5f,16.5f};frames(20);guest->m_player.angle=kPi*.5f;guest->m_player.pitch=0;
   InputState pull{};pull.use=true;guest->update(pull,dt);host->update(InputState{},dt);frames(30);
   check(host->state(stateId("lever_on"))==1&&guest->state(stateId("lever_on"))==1,"a guest pulling a lever runs it on the host, and the guest sees it thrown");}
  // --- Creatures that choose between players ---
  auto creature=[&](Vec2 at,float heading){Enemy e;e.kind=Enemy::Kind::Huntsman;e.pos=e.home=e.lastKnown=at;e.z=e.homeZ=0;e.heading=heading;e.hp=e.maxHp=5000;host->m_enemies.push_back(e);return host->m_enemies.size()-1;};
  host->m_enemies.clear();host->m_player.pos={12.f,18.f};put(*host,*guest,{6.5f,7.5f});frames(10);
  const float healthBefore=guest->m_player.health;
  {const size_t c=creature({6.5f,3.5f},kPi*.5f);bool chose=false;for(int i=0;i<30&&!chose;++i){frames(1);chose=host->m_enemies[c].target==gs;}
   check(chose,"a creature that can see only the guest (the host behind the wall) hunts the guest");
   until(int(6.f/dt),[&]{return guest->m_player.health<healthBefore-1;});
   check(guest->m_player.health<healthBefore-1&&std::fabs(host->m_match->m_slots[size_t(gs)].health-guest->m_player.health)<1.f,
    "its strike, resolved on the host, damages the guest (guest "+std::to_string(int(healthBefore))+" -> "+std::to_string(int(guest->m_player.health))+", the host's count agrees)");
   // Downed: dropped as a target.
   host->m_match->damage(gs,-1,1000,0);frames(10);
   check(guest->dead()&&host->m_enemies[c].target!=gs,"a downed guest is dropped as the creature's target");}
  frames(int(10.5f/dt));   // the guest comes back beside the host
  check(!guest->dead(),"the downed guest is back after 10 s");
  // Both in sight: the nearer is chosen, and small moves for a second do not flip it.
  {host->m_enemies.clear();host->m_player.pos={10.f,18.f};put(*host,*guest,{11.1f,19.6f});frames(10);
   const size_t c=creature({6.f,18.f},0.f);frames(2);const int first=host->m_enemies[c].target;int flips=0,last=first;
   for(int i=0;i<60;++i){const float s=(i/6)%2?.35f:-.35f;host->m_player.pos={10.f+s,18.f};frames(1);const int now=host->m_enemies[c].target;flips+=now!=last;last=now;}
   check(first==0&&flips==0,"with both in sight it takes the nearer (the host) and keeps it through a second of small moves ("+std::to_string(flips)+" flips)");}
  // A pack of two splits across the two players.
  {host->m_enemies.clear();host->m_player.pos={10.f,18.f};put(*host,*guest,{13.f,18.f});frames(10);
   const size_t a=creature({6.f,17.5f},0.f),b=creature({6.f,18.5f},0.f);frames(3);
   const int ta=host->m_enemies[a].target,tb=host->m_enemies[b].target;
   check(ta!=tb&&(ta==0||ta==gs)&&(tb==0||tb==gs),"a pack of two nearer the host splits: one on each player ("+std::to_string(ta)+", "+std::to_string(tb)+")");}
  host->m_enemies.clear();frames(5);
  // --- Guards driven by the host ---
  {host->m_player.pos={12.f,18.f};put(*host,*guest,{12.f,20.f});frames(10);
   host->setState(stateId("guard_on"),1);frames(20);
   check(!host->friendlyActors().empty()&&host->autonomousActor(host->friendlyActors()[0].track())&&!guest->friendlyActors().empty(),"the guard is on, on both machines");
   // Drift the guest's copy away: only the host's snapshots put him back.
   guest->m_friendlyActors[0].pose.position={20.f,20.f};
   const Vec2 start=host->friendlyActors()[0].pose.position;float worst=0;
   for(int i=0;i<int(3.f/dt);++i){frames(1);if(i>30)worst=std::max(worst,length(host->friendlyActors()[0].pose.position-guest->friendlyActors()[0].pose.position));}
   const float walked=length(host->friendlyActors()[0].pose.position-start);
   check(walked>1.f&&worst<.6f,"the guard walks his rounds on the host ("+std::to_string(walked).substr(0,4)+" m) and the guest's copy follows its snapshots (worst "+std::to_string(worst).substr(0,4)+" m)");}
  // The guard makes out the guest (the host is behind the wall) and his fire, resolved on the host, hurts them.
  {const auto& npc=host->friendlyActors()[0];const Vec2 ahead{std::sin(npc.pose.yaw),std::cos(npc.pose.yaw)};const Vec2 at=npc.pose.position+ahead*3.f;
   put(*host,*guest,{std::clamp(at.x,1.5f,22.5f),std::clamp(at.y,1.5f,10.5f)});const float before=guest->m_player.health;
   bool spotted=false;for(int i=0;i<int(4.f/dt)&&!spotted;++i){frames(1);const auto t=host->guardTarget(0);spotted=t.kind==ShotKind::Player&&t.index==gs;}
   check(spotted,"the guard sees the guest, not the host behind the wall, and takes the guest as his target");
   until(int(6.f/dt),[&]{return guest->m_player.health<before-1||guest->dead();});
   check(guest->m_player.health<before-1||guest->dead(),"his fire, traced on the host against the guest's body, wounds the guest ("+std::to_string(int(before))+" -> "+std::to_string(int(guest->m_player.health))+")");
   // Traffic while he fights: the co-op world's own bytes (scripts, state store, people) to this one guest.
   {const std::size_t bytes=host->m_match->m_coopBytes;frames(int(2.f/dt));
    report<<"INFO co-op world traffic to one guest with one guard in a fight: "<<float(host->m_match->m_coopBytes-bytes)/2.f/1024.f<<" KB/s\n";}
   host->setState(stateId("guard_on"),0);put(*host,*guest,{12.f,20.f});frames(int(11.f/dt));}
  // Traffic.
  {const std::size_t before=host->m_match->m_coopBytes;frames(int(2.f/dt));const float perSecond=float(host->m_match->m_coopBytes-before)/2.f;
   std::size_t creatures=0;{wire::Writer w;w.u8(12).u32(0).u8(0).u16(0);creatures=w.b.size()+20*8;}
   report<<"INFO co-op world traffic to one guest at rest (state store, clocks): "<<perSecond/1024<<" KB/s; creature snapshots (as before) ~"<<creatures<<" bytes for 8 at 15 Hz ("<<float(creatures)*15/1024<<" KB/s)\n";}
  sh->leave();frames(10);playing.clear();
 }
 // ===== Co-op on several maps at once, and props from the host =====
 {
  const std::string mapsFile="Net Maps.rmc";
  {std::ofstream(hostMaps/mapsFile,std::ios::binary)<<mapsText();std::ofstream(guestMaps/mapsFile,std::ios::binary)<<mapsText();}
  // West and South share an open seam (the Builder's open edges), as each machine loads them.
  auto opened=[](std::shared_ptr<const CustomCampaign> c){auto copy=std::make_shared<CustomCampaign>(*c);
   auto west=std::make_shared<AuthoredMapData>(*copy->maps[0]);west->openSouth=true;copy->maps[0]=west;
   auto south=std::make_shared<AuthoredMapData>(*copy->maps[1]);south->openNorth=true;copy->maps[1]=south;
   // The Annex on West's own origin: maps that do not share one grid (the loader keeps a file's apart; nothing else does).
   auto annex=std::make_shared<AuthoredMapData>(*copy->maps[2]);annex->definition.origin={0,0};copy->maps[2]=annex;return std::shared_ptr<const CustomCampaign>(copy);};
  auto mapsGuest=[&]{auto g=makeGuest(guestMaps,false);g->m_customCampaigns.push_back(opened(loadCustomCampaignFile(guestMaps/mapsFile)));return g;};
  auto pos=[](const Clutter* c){return c?Point3{c->pos.x,c->pos.y,c->z}:Point3{-99,-99,-99};};
  auto propOn=[](Game& g,int level,std::uint32_t id)->Clutter*{if(!g.chunkResident(level))return nullptr;auto& list=level==g.m_level?g.m_clutter:g.m_chunks[size_t(level)].clutter;for(auto& c:list)if(c.netId==id)return &c;return nullptr;};
  auto indexOf=[](Game& g,int level,const Clutter* c){auto& list=level==g.m_level?g.m_clutter:g.m_chunks[size_t(level)].clutter;return size_t(c-list.data());};
  auto turnGap=[](const Clutter* a,const Clutter* b){const auto qa=vectra::propOrientation(*a),qb=vectra::propOrientation(*b);return 1-std::fabs(qa.w*qb.w+qa.x*qb.x+qa.y*qb.y+qa.z*qb.z);};
  auto start=[&](LoopbackHub& hub,std::unique_ptr<Game>& host,std::vector<std::unique_ptr<Game>>& guests,std::vector<std::unique_ptr<LoopbackSession>>& sessions,int count){
   sessions.push_back(hub.connect("Hana"));host=std::make_unique<Game>(opened(loadCustomCampaignFile(hostMaps/mapsFile)));host->m_customMapDirectory=hostMaps.wstring();
   host->setNetSession(sessions[0].get());host->m_god=true;playing={host.get()};frames(1);
   HostOptions o;o.mode=Mode::Coop;o.map=mapsFile;o.mapHash=mapFileHash(hostMaps/mapsFile);o.maxPlayers=8;sessions[0]->host(o);frames(1);
   for(int k=0;k<count;++k){sessions.push_back(hub.connect(std::string("Guest")+char('A'+k)));guests.push_back(mapsGuest());guests.back()->setNetSession(sessions.back().get());playing.push_back(guests.back().get());
    frames(1);sessions.back()->joinCode(sessions[0]->lobby().joinCode);Game* g=guests.back().get();until(240,[&]{return g->m_match&&g->m_match->started();});}
   frames(30);};
  {
   LoopbackHub hub;std::unique_ptr<Game> host;std::vector<std::unique_ptr<Game>> guests;std::vector<std::unique_ptr<LoopbackSession>> sessions;start(hub,host,guests,sessions,1);
   Game& guest=*guests[0];const int gs=slotOfGame(guest);
   check(gs>0&&guest.m_match&&guest.m_match->started()&&host->m_level==0&&guest.m_level==0,"several maps: the guest joins on the start map");
   // --- Two maps at once: the host on West, the guest on the Annex (the same coordinates, not side by side) ---
   host->m_player.pos={6.f,6.f};host->m_match->placePlayer(gs,2,{6.f,6.f,0.f});frames(20);
   check(guest.m_level==2&&host->m_level==0&&host->m_match->m_slots[size_t(gs)].latest.level==2,"the host sends the guest to the Annex and stays on West");
   check(!remote(*host,gs)&&!remote(guest,0),"neither draws the other: they stand at the same coordinates of maps that are not side by side");
   check(host->players().size()==1&&host->m_netForeign.size()>2&&host->m_netForeign[2]&&host->m_netKeep[2],"the host's own minds see only the host; the Annex is kept and gets a frame of its own");
   const auto* initial=guest.skinbotWalker();
   auto* crate=propOn(guest,2,(3u<<16)|1u);
   const bool replicated=initial&&crate&&host->m_skinbots.contains(2)&&!host->skinbotWalker();
   check(replicated,"the guest receives Skinbot on the Annex while the host stays on West");
   if(replicated){
    put(*host,guest,{initial->x,initial->y+2.5f});
    guest.m_player.angle=-kPi*.5f;guest.m_player.pitch=0;
    frames(10);
    guest.m_heldClutter=int(indexOf(guest,2,crate));
    frames(12);
    guest.puntHeld({0,-1,.05f},.25f);
    float hostPeak=0,guestPeak=0,worstLag=0;
    bool hostCaught=false,guestCaught=false,guestCharge=false;
    int hit=0,surge=0;
    for(int tick=0;tick<165;++tick){
     frames(1);
     const auto& authoritative=host->m_skinbots.at(2).walker;
     const auto* drawn=guest.skinbotWalker();
     hostCaught|=authoritative.catching();
     if(!drawn)continue;
     guestCaught|=drawn->catching();
     if(authoritative.charging())hostPeak=std::max(hostPeak,std::hypot(authoritative.velocityX,authoritative.velocityY));
     if(drawn->charging()){
      guestCharge|=guestCaught;
      guestPeak=std::max(guestPeak,std::hypot(drawn->velocityX,drawn->velocityY));
      worstLag=std::max(worstLag,std::hypot(drawn->x-authoritative.x,drawn->y-authoritative.y));
     }
     for(const auto& sound:guest.sounds())if(sound.spatial){hit+=sound.sound==Sound::SkinbotHit;surge+=sound.sound==Sound::SkinbotSurge;}
    }
    check(hostCaught&&guestCaught&&guestCharge&&hostPeak>3.2f&&guestPeak>3.2f&&worstLag<.65f,
          "a guest's thrown crate makes Skinbot catch and sprint on both machines ("+
          std::to_string(hostPeak)+" / "+std::to_string(guestPeak)+" m/s, at most "+std::to_string(worstLag)+" m apart)");
    check(hit==1&&surge>=1,"the guest hears the single host impact and the charge on its own map");
    put(*host,guest,{6.f,6.f});frames(20);
   }
   auto creatureOn=[&](int level,Vec2 at,float heading){Enemy e;e.kind=Enemy::Kind::Huntsman;e.pos=e.home=e.lastKnown=at;e.z=e.homeZ=0;e.heading=heading;e.hp=e.maxHp=5000;
    auto& list=level==host->m_level?host->m_enemies:host->m_chunks[size_t(level)].enemies;list.push_back(e);return list.size()-1;};
   const size_t annex=creatureOn(2,{6.f,13.f},-kPi*.5f),west=creatureOn(0,{6.f,13.f},-kPi*.5f);
   const float guestBefore=guest.m_player.health;
   bool hunted=false;for(int i=0;i<int(3.f/dt)&&!hunted;++i){frames(1);hunted=host->m_chunks[2].enemies[annex].target==gs&&host->m_enemies[west].target==0;}
   check(hunted,"the Annex's creature takes the guest as its prey, West's the host: each hunts on its own map");
   until(int(8.f/dt),[&]{return guest.m_player.health<guestBefore-1;});
   check(guest.m_player.health<guestBefore-1,"the Annex's creature, run on the host, reaches and wounds the guest ("+std::to_string(int(guestBefore))+" -> "+std::to_string(int(guest.m_player.health))+")");
   check(length(host->m_enemies[west].pos-Vec2{6.f,13.f})>1.f,"West's creature ran after the host meanwhile ("+std::to_string(length(host->m_enemies[west].pos-Vec2{6.f,13.f})).substr(0,4)+" m)");
   {const auto& a=host->m_chunks[2].enemies[annex];bool seen=false;for(const auto& e:guest.m_enemies)seen|=e.alive&&length(e.pos-a.pos)<.6f;
    check(seen&&guest.m_enemies.size()==host->m_chunks[2].enemies.size(),"the guest is sent its own map's creatures, where the host has them, and not West's");}
   host->m_chunks[2].enemies.clear();host->m_enemies.clear();frames(5);
   // --- A guest leaves a map alone: over the seam onto South, nobody pulled ---
   host->m_match->placePlayer(gs,0,{12.f,22.f,0.f});frames(20);
   check(guest.m_level==0,"the guest is back on West, by the open seam");
   walk(guest,{0,1},2.5f,1.6f,nullptr,nullptr,0);frames(60);
   check(guest.m_level==1&&host->m_match->m_slots[size_t(gs)].latest.level==1&&host->m_level==0,"the guest walks over the seam onto South alone; the host stays on West");
   check(guest.m_player.pos.y>1.f&&guest.m_player.pos.y<9.f&&guest.hullFits(guest.m_player.pos,guest.m_player.z,guest.m_player.hullHeight()),"a second later the guest is where it walked to, in the open (not pulled back, not in a wall): "+std::to_string(guest.m_player.pos.x).substr(0,5)+", "+std::to_string(guest.m_player.pos.y).substr(0,5)+", z "+std::to_string(guest.m_player.z).substr(0,5));
   check(host->m_netKeep.size()>1&&host->m_netKeep[1]&&host->chunkResident(1)&&!host->m_netForeign.empty()&&host->m_netForeign[1],"the host keeps South resident and runs it");
   check(remote(*host,gs)&&remote(guest,0),"side by side on the grid, each still draws the other across the seam");
   // --- The host changes map: with follow off the guest stays; with COOPFOLLOW a guest on the map it left comes along ---
   host->m_match->placePlayer(0,2,{6.f,6.f,0.f});frames(30);
   check(host->m_level==2&&guest.m_level==1,"the host goes to the Annex; the guest stays on South");
   host->m_match->setFollowHost(true);host->m_match->placePlayer(gs,2,{8.f,6.f,0.f});frames(20);
   host->m_match->placePlayer(0,0,{6.f,9.f,0.f});frames(30);
   check(guest.m_level==0&&host->m_level==0&&gap(here(guest),here(*host))<3.f&&guest.hullFits(guest.m_player.pos,guest.m_player.z,guest.m_player.hullHeight()),"with COOPFOLLOW on, the guest on the map the host left comes along, beside it in the open (guest map "+std::to_string(guest.m_level)+", host map "+std::to_string(host->m_level)+", "+std::to_string(gap(here(guest),here(*host))).substr(0,4)+" m apart)");
   host->m_match->setFollowHost(false);
   // --- Props: the host's are the truth ---
   host->m_match->placePlayer(gs,0,{9.f,9.f,0.f});host->m_player.pos={6.f,9.f};frames(30);
   const std::uint32_t crateId=(1u<<16)|1u,bottleId=(1u<<16)|2u,boxId=(1u<<16)|3u;
   check(propOn(*host,0,crateId)&&propOn(guest,0,crateId)&&propOn(guest,0,bottleId)&&host->m_netPropsHost&&guest.m_netPropsRemote,"both machines name West's props alike");
   {Clutter* hc=propOn(*host,0,crateId);const Point3 home=pos(hc);host->knockClutter(0,indexOf(*host,0,hc),{hc->pos.x,hc->pos.y,hc->z+.55f},{14.f,0,3.f});frames(int(3.f/dt));
    hc=propOn(*host,0,crateId);const Clutter* gc=propOn(guest,0,crateId);const float moved=gap(pos(hc),home),err=gap(pos(hc),pos(gc));
    check(moved>.3f&&err<.03f&&turnGap(hc,gc)<1e-3f,"a crate the host knocks over moves on the guest: "+std::to_string(moved).substr(0,4)+" m, the guest's within "+std::to_string(err).substr(0,5)+" m and turned alike");}
   {Clutter* gb=propOn(guest,0,boxId);guest.m_heldClutter=int(indexOf(guest,0,gb));float worst=0;
    for(int i=0;i<60;++i){guest.m_player.angle+=.01f;frames(1);if(i>20)worst=std::max(worst,gap(pos(propOn(*host,0,boxId)),pos(propOn(guest,0,boxId))));}
    const Clutter* hb=propOn(*host,0,boxId);
    check(guest.holdingClutter()&&hb->netHolder==gs&&worst<.25f,"the guest picks up the box: its own hand moves it at once, the host's copy follows it (worst "+std::to_string(worst).substr(0,4)+" m behind)");
    const Point3 held=pos(hb);guest.m_suppressFire=false;guest.puntHeld({std::cos(guest.m_player.angle),std::sin(guest.m_player.angle),.35f},1.f);frames(int(4.f/dt));
    hb=propOn(*host,0,boxId);const Clutter* gb2=propOn(guest,0,boxId);const float flew=gap(pos(hb),held),err=gap(pos(hb),pos(gb2));
    check(!guest.holdingClutter()&&hb->netHolder<0&&flew>.5f&&err<.05f,"thrown, it flies on the host ("+std::to_string(flew).substr(0,4)+" m) and lands in the same place on both (within "+std::to_string(err).substr(0,5)+" m)");}
   {Clutter* hb=propOn(*host,0,bottleId);const bool wet=hb->liquid.holds();host->breakClutter(0,indexOf(*host,0,hb),{0,0,-1});frames(20);
    const Clutter* gb=propOn(guest,0,bottleId);
    check(gb&&gb->broken&&(!wet||!guest.liquidDrops().empty()||!guest.puddles().empty()),std::string("a bottle broken on the host breaks on the guest")+(wet?" and spills its water there":""));}
   {Clutter* gc=propOn(guest,0,crateId);const Point3 before=pos(propOn(*host,0,crateId));guest.knockClutter(0,indexOf(guest,0,gc),{gc->pos.x,gc->pos.y,gc->z+.3f},{0,-30.f,6.f});frames(int(3.f/dt));
    const float moved=gap(pos(propOn(*host,0,crateId)),before),err=gap(pos(propOn(*host,0,crateId)),pos(propOn(guest,0,crateId)));
    check(moved>.2f&&err<.03f,"a guest's blow on a crate is dealt on the host ("+std::to_string(moved).substr(0,4)+" m) and the guest sees where it went (within "+std::to_string(err).substr(0,5)+" m)");}
   // --- Fifty props awake: the traffic to one guest ---
   {host->m_match->placePlayer(gs,3,{96.f+12.f,14.f,0.f});frames(30);
    for(auto& c:host->m_chunks[3].clutter)if(c.netId&&c.netId!=Clutter::NetDebris)host->knockClutter(3,indexOf(*host,3,&c),{c.pos.x+.1f,c.pos.y,c.z+.35f},{12.f,7.f,30.f});
    const std::size_t before=host->m_match->m_propBytes;int awake=0;
    for(int i=0;i<int(1.f/dt);++i){frames(1);int now=0;for(const auto& c:host->m_chunks[3].clutter)now+=c.netId!=Clutter::NetDebris&&!c.sleeping;awake=std::max(awake,now);}
    const float perSecond=float(host->m_match->m_propBytes-before);
    report<<"INFO props: "<<awake<<" props awake on the guest's map: "<<perSecond/1024.f<<" KB/s to that guest (PropState at 15 Hz, ~13 bytes a prop)\n";
    frames(int(4.f/dt));float worst=0;for(const auto& c:host->m_chunks[3].clutter){const Clutter* g=propOn(guest,3,c.netId);worst=std::max(worst,g?gap(pos(&c),pos(g)):99.f);}
    check(awake>=40&&perSecond<16*1024&&worst<.03f,"fifty knocked crates come to rest on the guest where the host has them (worst "+std::to_string(worst).substr(0,5)+" m) at "+std::to_string(int(perSecond/1024))+" KB/s");}
   for(auto& s:sessions)s->leave();frames(5);playing.clear();
  }
  // --- The host's frame with 1, 2 and 4 maps occupied (each with six creatures hunting) ---
  {
   LoopbackHub hub;std::unique_ptr<Game> host;std::vector<std::unique_ptr<Game>> guests;std::vector<std::unique_ptr<LoopbackSession>> sessions;start(hub,host,guests,sessions,3);
   for(auto& g:guests)g->m_god=true;
   auto pack=[&](int level,Vec2 near){auto& list=level==host->m_level?host->m_enemies:host->m_chunks[size_t(level)].enemies;list.clear();
    for(int k=0;k<6;++k){Enemy e;e.kind=Enemy::Kind::Huntsman;e.pos=e.home=e.lastKnown=near+Vec2{-5.f+2.f*float(k),8.f};e.z=e.homeZ=0;e.heading=-kPi*.5f;e.hp=e.maxHp=5000;e.awareness=8;list.push_back(e);}};
   auto timeHost=[&](int n){double ms=0,worst=0;for(int i=0;i<n;++i){for(auto& g:guests)g->update(InputState{},dt);const auto t0=std::chrono::steady_clock::now();host->update(InputState{},dt);
     const double f=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();ms+=f;worst=std::max(worst,f);}return std::pair<double,double>{ms/n,worst};};
   const int levels[3][3]={{0,0,0},{2,0,0},{1,2,3}};double one=0;
   for(int c=0;c<3;++c){host->m_player.pos={12.f,4.f};
    for(int k=0;k<3;++k){const int l=levels[c][k];host->m_match->placePlayer(slotOfGame(*guests[size_t(k)]),l,{host->chunkOffset(l).x+6.f+3.f*float(k),host->chunkOffset(l).y+4.f,0.f});}
    frames(20);for(int l=0;l<4;++l)pack(l,{12.f,4.f});timeHost(30);const auto [mean,worst]=timeHost(120);if(c==0)one=mean;
    const int maps=c==0?1:c==1?2:4;
    report<<"INFO host frame with "<<maps<<" occupied map"<<(maps>1?"s":"")<<": "<<mean<<" ms mean, "<<worst<<" ms worst (6 hunting creatures a map, "<<(c==0?"all on West":c==1?"one guest on the Annex":"guests on South, the Annex and the Yard")<<")\n";
    if(c==2)check(mean<std::max(2*one,one+4.0),"four occupied maps do not halve the host's frame rate ("+std::to_string(one).substr(0,5)+" ms with one, "+std::to_string(mean).substr(0,5)+" ms with four)");}
   for(auto& s:sessions)s->leave();frames(5);playing.clear();
  }
  // --- Props over a lossy, slow hub ---
  {
   LoopbackHub hub;hub.unreliableLossPercent=20;hub.latencyTicks=6;std::unique_ptr<Game> host;std::vector<std::unique_ptr<Game>> guests;std::vector<std::unique_ptr<LoopbackSession>> sessions;start(hub,host,guests,sessions,1);
   Game& guest=*guests[0];const int gs=slotOfGame(guest);host->m_match->placePlayer(gs,0,{9.f,9.f,0.f});host->m_player.pos={6.f,9.f};frames(40);
   const std::uint32_t crateId=(1u<<16)|1u,boxId=(1u<<16)|3u;
   {Clutter* hc=propOn(*host,0,crateId);host->knockClutter(0,indexOf(*host,0,hc),{hc->pos.x,hc->pos.y,hc->z+.55f},{14.f,0,3.f});frames(int(4.f/dt));}
   const float knocked=gap(pos(propOn(*host,0,crateId)),pos(propOn(guest,0,crateId)));
   {Clutter* gb=propOn(guest,0,boxId);guest.m_heldClutter=int(indexOf(guest,0,gb));for(int i=0;i<40;++i){guest.m_player.angle+=.01f;frames(1);}
    guest.puntHeld({std::cos(guest.m_player.angle),std::sin(guest.m_player.angle),.35f},1.f);frames(int(5.f/dt));}
   const float thrown=gap(pos(propOn(*host,0,boxId)),pos(propOn(guest,0,boxId)));
   check(knocked<.03f&&thrown<.05f,"20% loss, 6-tick latency: a crate the host knocks and a box the guest throws end where the host has them ("+std::to_string(knocked).substr(0,5)+" / "+std::to_string(thrown).substr(0,5)+" m)");
   for(auto& s:sessions)s->leave();frames(5);playing.clear();
  }
  {wire::Writer w;w.u8(15).u32(0).u8(0).u8(50);const std::size_t bytes=w.b.size()+50*13;
   report<<"INFO PropState with 50 props: "<<bytes<<" bytes; at 15 Hz "<<float(bytes)*15/1024<<" KB/s per guest; PropCarry 15 bytes at 30 Hz up while carrying ("<<15*30/1024.f<<" KB/s); PropThrow 45, PropKnock 33, PropBreak 8, PropSpawn 23, PaneBreak 4 bytes\n";}
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
  s.pose.active=true;s.pose.head={.1f,-.05f,1.5f};s.pose.headYaw=1.1f;s.pose.headPitch=-.2f;s.pose.bodyYaw=.9f;s.pose.eye=1.48f;s.pose.inHand=4;s.pose.back=2;s.pose.hip=4;s.pose.support=true;
  s.pose.hands[1].tracked=true;s.pose.hands[1].wrist={.3f,-.2f,1.1f};s.pose.hands[1].forward={1,0,0};s.pose.hands[1].curls={0,.5f,1,1,1};
  wire::Writer w;writePlayerState(w,s,{},true);wire::Reader r(w.b);NetPlayerState t;const bool read=readPlayerState(r,t,true);
  auto near=[](Point3 a,Point3 b,float e){return std::fabs(a.x-b.x)<e&&std::fabs(a.y-b.y)<e&&std::fabs(a.z-b.z)<e;};
  check(read&&r.left()==0&&near(t.world,s.world,.005f)&&std::fabs(std::remainder(t.angle-s.angle,kTwoPi))<.001f&&t.vr&&t.weapon==2&&t.shots==7&&t.life==3&&t.health==64
   &&near(t.pose.head,s.pose.head,.001f)&&t.pose.inHand==4&&t.pose.back==2&&t.pose.hip==4&&t.pose.support&&t.pose.hands[1].tracked&&near(t.pose.hands[1].wrist,s.pose.hands[1].wrist,.001f)&&std::fabs(t.pose.hands[1].curls[1]-.5f)<.01f,
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

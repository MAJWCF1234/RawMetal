#include "Match.h"
#include "MatchProtocol.h"
#include "Wire.h"
#include "../game/Game.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <ostream>
namespace retro {
// The Session's frame, before the game's (docs/Systems/multiplayer.md). A Session in a lobby gets a Match; a
// Match whose Session has left, whose lobby closed or which ended itself is dropped and the title comes back.
// Quitting to the title from a match's pause menu leaves the lobby.
void Game::updateNet(){
 if(!m_netSession){if(m_match){m_match.reset();m_remotePlayers.clear();m_matchView={};m_netScriptsOff=m_netCreaturesRemote=false;m_netPropsRemote=m_netPropsHost=false;m_netForeign.clear();m_netKeep.clear();}return;}
 m_netSession->tick();
 auto inLobby=[&]{const auto s=m_netSession->state();return s==net::SessionState::InLobby||((s==net::SessionState::Busy||s==net::SessionState::Failed)&&!m_netSession->lobby().id.empty());};
 if(!m_match&&inLobby())m_match=std::make_shared<net::Match>(*this);
 if(!m_match)return;
 if(m_match->started()&&m_titleScreen&&inLobby())m_netSession->leave();
 if(inLobby()&&!m_match->finished())return;
 const bool started=m_match->played();
 std::string status=m_match->finished()?m_match->endStatus():std::string("You left the game");
 m_match.reset();m_remotePlayers.clear();m_matchView={};m_matchView.status=status;
 m_netScriptsOff=m_netCreaturesRemote=false;m_netBodies=nullptr;m_netPropsRemote=m_netPropsHost=false;m_netForeign.clear();m_netKeep.clear();
 if(inLobby())m_netSession->leave();
 if(started&&!m_titleScreen)showTitleScreen();
}
}
namespace retro::net {
using namespace protocol;

std::string contentHash(const std::uint8_t* data,std::size_t size){
 std::uint64_t h=0xCBF29CE484222325ull;for(std::size_t i=0;i<size;++i){h^=data[i];h*=0x100000001B3ull;}
 static const char hex[]="0123456789abcdef";std::string s(16,'0');for(int i=15;i>=0;--i){s[size_t(i)]=hex[h&15];h>>=4;}return s;
}
static bool readBytes(const std::filesystem::path& file,std::vector<std::uint8_t>& out){
 std::ifstream in(file,std::ios::binary);if(!in)return false;
 out.assign(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());return true;
}
std::string mapFileHash(const std::filesystem::path& file){std::vector<std::uint8_t> bytes;if(!readBytes(file,bytes))return {};return contentHash(bytes.data(),bytes.size());}
// Two names of one map: the same stem whatever its case (a .txt draft arrives as .rmc).
static bool sameMapName(const std::string& a,const std::string& b){
 auto stem=[](const std::string& s){std::string t=std::filesystem::path(s).stem().string();for(auto& c:t)c=char(std::tolower(static_cast<unsigned char>(c)));return t;};
 return !a.empty()&&stem(a)==stem(b);
}

Match::Match(Game& game):m_game(game),m_session(game.m_netSession){}
Match::~Match(){m_game.m_remoteBodies.clear();m_game.m_netHurts.clear();m_game.m_netUses.clear();m_game.m_scriptFx.clear();m_game.m_netPropIntents.clear();m_game.m_netPropEvents.clear();m_game.m_netForeign.clear();m_game.m_netKeep.clear();}

void Match::send(const PeerId& to,std::uint8_t channel,const wire::Writer& w,bool reliable){
 if(!m_session||to.empty()||to==m_self)return;
 if(m_session->send(to,channel,w.b.data(),w.b.size(),reliable))m_bytesSent+=w.b.size();
}
// The host to every guest in the game (ready: playing the map); a guest to the host.
void Match::broadcast(std::uint8_t channel,const wire::Writer& w,bool reliable,int exceptSlot){
 if(!m_host){send(m_hostId,channel,w,reliable);return;}
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(s.used&&s.ready&&i!=m_selfSlot&&i!=exceptSlot)send(s.peer,channel,w,reliable);}
}
int Match::slotOf(const PeerId& peer)const{for(int i=0;i<MaxSlots;++i)if(m_slots[size_t(i)].used&&m_slots[size_t(i)].peer==peer)return i;return -1;}
int Match::freeSlot(int hint)const{
 if(hint>=0&&hint<MaxSlots&&!m_slots[size_t(hint)].used)return hint;
 for(int i=0;i<MaxSlots;++i)if(!m_slots[size_t(i)].used)return i;return -1;
}
std::filesystem::path Match::mapsFolder()const{
 return m_game.m_customMapDirectory.empty()?std::filesystem::current_path()/L"custom maps":std::filesystem::path(m_game.m_customMapDirectory);
}
// Where this machine keeps a map's file: beside the others, or among the downloaded ones.
std::filesystem::path Match::localMapFile(const std::string& file)const{
 std::error_code ec;const auto name=std::filesystem::path(file).filename();if(name.empty())return {};
 for(auto p:{mapsFolder()/name,mapsFolder()/L"Downloaded"/name,mapsFolder()/L"Downloaded"/(name.stem().wstring()+L".rmc")})if(std::filesystem::is_regular_file(p,ec))return p;
 return {};
}
// A loaded custom campaign of that name whose file has that hash (any, when the hash is empty).
std::shared_ptr<const CustomCampaign> Match::findCampaign(const std::string& file,const std::string& hash){
 for(const auto& c:m_game.m_customCampaigns){if(!c||!sameMapName(c->sourceFile,file))continue;
  if(hash.empty())return c;
  const auto path=localMapFile(c->sourceFile);
  const std::string have=path.empty()?"builtin-"+std::to_string(c->key):mapFileHash(path);
  if(have==hash)return c;}
 // Downloaded on an earlier night but not listed (the folder's own maps are): load it from there.
 const auto path=mapsFolder()/L"Downloaded"/(std::filesystem::path(file).stem().wstring()+L".rmc");std::error_code ec;
 if(std::filesystem::is_regular_file(path,ec)&&(hash.empty()||mapFileHash(path)==hash))
  try{auto c=loadCustomCampaignFile(path);m_game.m_customCampaigns.push_back(c);return c;}catch(const std::exception&){}
 return nullptr;
}

void Match::end(const std::string& status){m_finished=true;m_phase=Phase::Ended;m_endStatus=status;}

void Match::begin(){
 m_begun=true;m_self=m_session->self();m_hostId=m_session->hostId();m_host=m_session->isHost();
 m_mode=m_session->lobby().mode;m_maxPlayers=std::clamp(m_session->lobby().maxPlayers,2,MaxSlots);
 if(m_host)hostBegin();else guestHello();
}

// The host: the lobby's map from the loaded custom campaigns, its deathmatch map (or start map in co-op).
void Match::hostBegin(){
 const auto& lobby=m_session->lobby();
 auto campaign=findCampaign(lobby.map,{});
 if(!campaign&&lobby.map.empty())for(const auto& c:m_game.m_customCampaigns)if(c&&!c->maps.empty()&&(m_mode==Mode::Coop?c->multiplayer.coop:c->multiplayer.deathmatch)){campaign=c;break;}
 if(!campaign||campaign->maps.empty()){end(lobby.map.empty()?"No custom map to play":"The map "+lobby.map+" is not installed");return;}
 const auto& rules=campaign->multiplayer;
 m_fragLimit=std::max(0,rules.fragLimit);m_timeLimit=std::max(0.f,rules.timeLimitMinutes)*60.f;m_friendlyFire=rules.friendlyFire;
 m_mapFile=campaign->sourceFile;
 const auto path=localMapFile(campaign->sourceFile);
 if(!path.empty()&&readBytes(path,m_mapBytes))m_mapHash=contentHash(m_mapBytes.data(),m_mapBytes.size());
 else{m_mapBytes.clear();m_mapHash="builtin-"+std::to_string(campaign->key);}
 const int last=int(campaign->maps.size())-1;
 m_mapLevel=std::clamp(m_mode==Mode::Deathmatch&&rules.deathmatchMap>=0?rules.deathmatchMap:campaign->startMap,0,last);
 m_selfSlot=0;auto& me=m_slots[0];me=Slot{};me.used=me.ready=me.spawned=me.host=true;me.peer=m_self;me.name=m_session->displayName();me.character=m_game.m_settings.character;
 startWorld(campaign,m_mapLevel);
 if(m_mode==Mode::Deathmatch)respawn(0);
 else{me.latest=localState();me.haveState=true;me.acceptedAt=m_clock;}
 m_rosterDirty=true;
}

void Match::guestHello(){
 wire::Writer w;w.u8(Hello).u16(ProtocolVersion).u8(unsigned(std::clamp(m_game.m_settings.character,0,255))).str(m_session->displayName())
  .u8(m_selfSlot>=0?unsigned(m_selfSlot):255u).u8(m_phase==Phase::Playing?1:0);
 send(m_hostId,Control,w,true);m_helloTimer=2.f;
}

// Deathmatch goes on under the new host, from what it knows: the roster, everyone's last state and its own.
void Match::becomeHost(){
 const auto members=m_session->members();
 std::string oldName;
 for(int i=0;i<MaxSlots;++i){auto& s=m_slots[size_t(i)];if(!s.used||i==m_selfSlot)continue;
  if(std::find(members.begin(),members.end(),s.peer)==members.end()){if(s.host)oldName=s.name;s=Slot{};continue;}
  s.host=false;s.ready=true;s.acceptedAt=-1;s.life=s.latest.life;if(!s.alive&&s.respawnIn<0)s.respawnIn=3;}
 m_host=true;m_hostId=m_self;
 if(m_selfSlot>=0){auto& me=m_slots[size_t(m_selfSlot)];me.used=me.ready=me.spawned=me.host=true;me.peer=m_self;me.health=m_game.m_player.health;me.alive=!m_game.dead();me.latest=localState();me.haveState=true;me.acceptedAt=m_clock;me.life=m_lifeLocal;
  if(!me.alive&&me.respawnIn<0)me.respawnIn=std::max(0.f,m_selfRespawnIn);}
 m_game.m_netScriptsOff=false;m_game.m_netCreaturesRemote=false;m_game.m_netPropsRemote=false;m_game.m_netPropsHost=true;clearProps();
 if(m_timeLimit>0&&m_timeLeft>=0)m_roundClock=m_timeLimit-m_timeLeft;
 const auto path=localMapFile(m_mapFile);if(path.empty()||!readBytes(path,m_mapBytes))m_mapBytes.clear();
 m_restartIn=m_over?10.f:-1.f;
 for(auto& s:m_slots)s.history.clear();   // their times were the old host's clock
 m_notice=(oldName.empty()?std::string("The host"):oldName)+" left. You are the host now.";m_noticeTime=5;
 m_rosterDirty=true;
}

// The world a match plays in: the campaign's map, its scripts and creatures as the role and mode want them.
void Match::startWorld(std::shared_ptr<const CustomCampaign> campaign,int level){
 Game& g=m_game;
 g.m_customCampaign=campaign;g.m_customCampaignKey=campaign->key;g.m_worldId=WorldId::Custom;
 g.m_level=std::clamp(level,0,int(campaign->maps.size())-1);
 g.m_titleScreen=g.m_menuFromTitle=g.m_customMapsOpen=g.m_paused=g.m_inventoryOpen=g.m_consoleOpen=false;
 g.restart();g.m_suppressFire=true;
 g.m_netScriptsOff=!m_host;g.m_netCreaturesRemote=!m_host&&m_mode==Mode::Coop;g.m_netPropsRemote=!m_host;g.m_netPropsHost=m_host;
 m_campaign=campaign;m_mapName=campaign->name;
 if(m_mode==Mode::Deathmatch)clearCreatures();
 m_spawns.clear();buildSpawns();
 m_pickupSeen.clear();m_doorSeen.clear();m_doorTouched.clear();m_doorLevel=-1;m_creatureTargets.clear();m_creatureLevel=-1;clearCoop();clearProps();
 m_lastLevel=g.m_level;m_phase=Phase::Playing;m_played=true;
}

void Match::finishDownload(){
 if(contentHash(m_download.data(),m_download.size())!=m_mapHash){end("The map did not arrive whole");return;}
 const auto folder=mapsFolder()/L"Downloaded";std::error_code ec;std::filesystem::create_directories(folder,ec);
 const auto path=folder/(std::filesystem::path(m_mapFile).stem().wstring()+L".rmc");
 {std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(m_download.data()),std::streamsize(m_download.size()));
  if(!out){end("Cannot save the map in "+folder.string());return;}}
 std::shared_ptr<const CustomCampaign> campaign;
 try{campaign=loadCustomCampaignFile(path);}catch(const std::exception& e){end(std::string("The host's map does not load: ")+e.what());return;}
 if(!campaign||campaign->maps.empty()){end("The host's map is empty");return;}
 auto& list=m_game.m_customCampaigns;
 list.erase(std::remove_if(list.begin(),list.end(),[&](const auto& c){return c&&sameMapName(c->sourceFile,m_mapFile);}),list.end());
 list.push_back(campaign);
 m_download.clear();m_download.shrink_to_fit();
 startWorld(campaign,m_mapLevel);
 wire::Writer w;w.u8(Ready).u8(0);send(m_hostId,Control,w,true);
}

void Match::handleControl(const Packet& p){
 wire::Reader r(p.data);const unsigned type=r.u8();
 if(m_host){
  if(type==Hello){
   const unsigned version=r.u16();const int character=int(r.u8());std::string name=r.str();const int hint=int(r.u8());const bool playing=r.u8()!=0;
   if(r.bad)return;
   if(version!=ProtocolVersion){wire::Writer w;w.u8(Refuse).str("The host plays version "+std::to_string(ProtocolVersion)+" of multiplayer; update the game");send(p.from,Control,w,true);return;}
   int slot=slotOf(p.from);
   if(slot<0){slot=freeSlot(hint);if(slot<0){wire::Writer w;w.u8(Refuse).str("The game is full");send(p.from,Control,w,true);return;}
    m_slots[size_t(slot)]=Slot{};m_slots[size_t(slot)].used=true;m_slots[size_t(slot)].peer=p.from;}
   auto& s=m_slots[size_t(slot)];s.name=name.empty()?"Player "+std::to_string(slot+1):name;s.character=character;
   if(!playing){s.ready=false;s.spawned=false;}
   wire::Writer w;w.u8(Welcome).u8(unsigned(slot)).u8(unsigned(m_mode)).u16(unsigned(m_fragLimit)).u16(unsigned(std::lround(m_timeLimit))).u8(m_friendlyFire?1:0).u8(unsigned(m_maxPlayers))
    .str(m_mapFile).str(m_mapHash).u8(unsigned(m_mapLevel)).u8(unsigned(m_round)).u32(std::uint32_t(m_clock*1000));
   send(p.from,Control,w,true);m_rosterDirty=true;
  }else if(type==MapRequest){
   if(slotOf(p.from)<0)return;
   if(m_mapBytes.empty()){wire::Writer w;w.u8(Refuse).str("The host cannot send this map (it is built into the game)");send(p.from,Control,w,true);return;}
   for(const auto& u:m_uploads)if(u.peer==p.from)return;
   m_uploads.push_back({p.from,0});
  }else if(type==Ready){
   const bool already=r.u8()!=0;const int slot=slotOf(p.from);if(slot<0||r.bad)return;
   auto& s=m_slots[size_t(slot)];s.ready=true;
   if(m_mode==Mode::Coop){
    // The pickups already taken, on every map.
    wire::Writer w;w.u8(PickupsInit);std::vector<std::pair<int,int>> taken;
    for(int level=0;level<m_game.chunkCount();++level)if(m_game.chunkResident(level)){const auto& list=m_game.chunkPickups(level);for(size_t i=0;i<list.size();++i)if(!list[i].active)taken.push_back({level,int(i)});}
    w.u16(unsigned(taken.size()));for(auto [level,i]:taken)w.u8(unsigned(level)).u16(unsigned(i));send(p.from,Events,w,true);
    sendStateSync(true,&p.from);   // the whole state store, as it stands (MatchCoop.cpp)
   }
   if(already&&s.haveState)s.spawned=true;else respawn(slot);
   if(m_over){wire::Writer w;w.u8(RoundOver).str(m_winner);send(p.from,Events,w,true);}
   m_rosterDirty=true;
  }
  return;
 }
 if(p.from!=m_hostId)return;
 if(type==Welcome){
  const int slot=int(r.u8());const Mode mode=Mode(r.u8());const int frags=int(r.u16());const float limit=float(r.u16());const bool ff=r.u8()!=0;const int maxPlayers=int(r.u8());
  std::string file=r.str(),hash=r.str();const int level=int(r.u8());const int round=int(r.u8());const std::uint32_t hostMs=r.u32();
  if(r.bad||slot>=MaxSlots)return;
  m_helloTimer=-1;m_selfSlot=slot;m_mode=mode==Mode::Coop?Mode::Coop:Mode::Deathmatch;m_fragLimit=frags;m_timeLimit=limit;m_friendlyFire=ff;m_maxPlayers=maxPlayers;m_round=round;
  if(!m_haveOffset){m_hostOffset=float(hostMs)/1000.f-m_clock;m_haveOffset=true;}
  if(m_phase==Phase::Playing&&sameMapName(file,m_mapFile)){wire::Writer w;w.u8(Ready).u8(1);send(m_hostId,Control,w,true);return;}   // a new host: play on
  const auto name=std::filesystem::path(file).filename();
  if(name.empty()||name.string()!=file||!isCustomCampaignFile(name)){end("The host offered a map this game cannot use");return;}
  m_mapFile=file;m_mapHash=hash;m_mapLevel=level;
  if(auto campaign=findCampaign(file,hash)){startWorld(campaign,level);wire::Writer w;w.u8(Ready).u8(0);send(m_hostId,Control,w,true);return;}
  m_phase=Phase::Downloading;m_download.clear();m_downloadSize=m_downloadHave=0;
  wire::Writer w;w.u8(MapRequest);send(m_hostId,Control,w,true);
 }else if(type==Refuse){end(r.str());}
 else if(type==Roster){
  const int count=int(r.u8());std::array<bool,MaxSlots> seen{};
  for(int k=0;k<count&&!r.bad;++k){const int slot=int(r.u8());PeerId peer=r.str();std::string name=r.str();const int character=int(r.u8());const int frags=r.i16(),deaths=r.i16(),ping=r.i16();const unsigned flags=r.u8();
   if(r.bad||slot>=MaxSlots)break;auto& s=m_slots[size_t(slot)];
   if(s.used&&s.peer!=peer)s=Slot{};
   s.used=true;s.peer=std::move(peer);s.name=std::move(name);s.character=character;s.frags=frags;s.deaths=deaths;s.pingMs=ping;s.host=flags&1;s.ready=(flags&4)!=0;
   if(slot!=m_selfSlot)s.alive=(flags&2)!=0;seen[size_t(slot)]=true;}
  if(!r.bad)for(int i=0;i<MaxSlots;++i)if(!seen[size_t(i)])m_slots[size_t(i)]=Slot{};
 }
}

void Match::handleMapData(const Packet& p){
 if(m_host||p.from!=m_hostId||m_phase!=Phase::Downloading)return;
 wire::Reader r(p.data);if(r.u8()!=MapChunk)return;const std::uint32_t total=r.u32(),offset=r.u32();
 if(r.bad||total==0||total>(64u<<20)){end("The host's map is too large");return;}
 const std::size_t n=r.left();if(offset+n>total)return;
 if(m_downloadSize!=total){m_download.assign(total,0);m_downloadSize=total;m_downloadHave=0;}
 std::copy(p.data.begin()+long(r.i),p.data.end(),m_download.begin()+long(offset));m_downloadHave+=n;
 if(m_downloadHave>=m_downloadSize)finishDownload();
}

// The map to each guest who asked for it, a few chunks a frame (the reliable channel queues the rest).
void Match::sendUploads(){
 for(auto it=m_uploads.begin();it!=m_uploads.end();){
  if(slotOf(it->peer)<0){it=m_uploads.erase(it);continue;}
  for(int k=0;k<24&&it->offset<m_mapBytes.size();++k){const std::size_t n=std::min(MapChunkBytes,m_mapBytes.size()-it->offset);
   wire::Writer w;w.u8(MapChunk).u32(std::uint32_t(m_mapBytes.size())).u32(std::uint32_t(it->offset)).bytes(m_mapBytes.data()+it->offset,n);
   send(it->peer,MapData,w,true);it->offset+=n;}
  if(it->offset>=m_mapBytes.size())it=m_uploads.erase(it);else ++it;
 }
}

void Match::sendRoster(){
 wire::Writer w;w.u8(Roster);int count=0;for(const auto& s:m_slots)count+=s.used;w.u8(unsigned(count));
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(!s.used)continue;
  w.u8(unsigned(i)).str(s.peer).str(s.name).u8(unsigned(std::clamp(s.character,0,255))).i16(s.frags).i16(s.deaths).i16(s.pingMs).u8((s.host?1u:0u)|(s.alive?2u:0u)|(s.ready?4u:0u));}
 broadcast(Control,w,true);m_rosterDirty=false;m_rosterTimer=2.f;
}

void Match::update(const InputState&,float dt){
 if(m_finished)return;
 m_session=m_game.m_netSession;if(!m_session)return;
 dt=std::clamp(dt,0.f,.25f);m_clock+=dt;
 if(!m_begun){if(m_session->self().empty()||m_session->hostId().empty())return;begin();if(m_finished)return;}
 // The lobby passed on: deathmatch carries on under the new host, co-op ends with its host.
 if(const PeerId h=m_session->hostId();!h.empty()&&h!=m_hostId){
  if(m_mode==Mode::Coop){end("The host left the game. Co-op ends with them.");return;}
  m_hostId=h;
  if(m_session->isHost()){if(m_phase==Phase::Playing)becomeHost();else{end("The host left before the game began");return;}}
  else{m_haveOffset=false;m_haveSnapshot=false;for(auto& s:m_slots)s.history.clear();guestHello();}
 }
 Packet p;
 while(m_session->receive(p)){
  m_bytesReceived+=p.data.size();if(p.data.empty())continue;
  if(p.channel==Control)handleControl(p);
  else if(p.channel==MapData)handleMapData(p);
  else if(p.channel==State)handleState(p);
  else if(p.channel==Events){if(p.data[0]==ChatSay||p.data[0]==ChatRelay)handleChat(p);else handleEvent(p);}
  if(m_finished)return;
 }
 if(m_host){if(m_phase==Phase::Playing)hostFrame(dt);sendUploads();if(m_rosterDirty||(m_rosterTimer-=dt)<=0)sendRoster();}
 else{
  if(m_phase==Phase::Joining&&m_helloTimer>0&&(m_helloTimer-=dt)<=0)guestHello();
  if(m_phase==Phase::Playing)guestFrame(dt);
 }
 chatNotices();
 publish(dt);
 publishComms(dt);
}

}

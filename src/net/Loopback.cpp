#include "Loopback.h"
#include <algorithm>
#include <cctype>
#include <ostream>
namespace retro::net {

static const char CodeAlphabet[]="ABCDEFGHJKMNPQRSTUVWXYZ23456789";   // no 0/O, 1/I/L
std::string makeJoinCode(std::uint64_t seed){
 std::uint64_t x=seed*0x9E3779B97F4A7C15ull+0xD1B54A32D192ED03ull;std::string code;
 for(int i=0;i<6;++i){x^=x>>29;x*=0xBF58476D1CE4E5B9ull;x^=x>>32;code+=CodeAlphabet[x%(sizeof CodeAlphabet-1)];if(i==3)code+='-';}
 return code;
}
std::string normaliseJoinCode(const std::string& typed){
 std::string out;
 for(char c:typed){if(c=='-'||c==' ')continue;c=char(std::toupper(static_cast<unsigned char>(c)));
  bool known=false;for(const char* a=CodeAlphabet;*a;++a)known|=*a==c;
  if(!known)return {};                                           // the alphabet never makes 0, O, 1, I or L
  out+=c;}
 if(out.size()!=6)return {};
 return out.substr(0,4)+"-"+out.substr(4);
}

std::unique_ptr<LoopbackSession> LoopbackHub::connect(const std::string& name){
 const PeerId id="loop-"+std::to_string(nextId++);
 auto s=std::make_unique<LoopbackSession>(*this,id,name);sessions[id]=s.get();return s;
}
LoopbackHub::Lobby* LoopbackHub::lobbyOf(const PeerId& peer){
 for(auto& [id,l]:lobbies)if(std::find(l.members.begin(),l.members.end(),peer)!=l.members.end())return &l;
 return nullptr;
}
void LoopbackHub::broadcastEvent(const Lobby& lobby,const SessionEvent& event,const PeerId& except){
 for(const auto& m:lobby.members)if(m!=except)if(auto it=sessions.find(m);it!=sessions.end())it->second->event(event);
}
void LoopbackHub::removeMember(const PeerId& peer){
 Lobby* l=lobbyOf(peer);if(!l)return;
 l->members.erase(std::remove(l->members.begin(),l->members.end(),peer),l->members.end());
 l->info.players=int(l->members.size());
 if(l->members.empty()){lobbies.erase(l->info.id);return;}
 broadcastEvent(*l,{SessionEvent::MemberLeft,peer,{}},peer);
 if(l->info.owner==peer){l->info.owner=l->members.front();broadcastEvent(*l,{SessionEvent::HostChanged,l->info.owner,{}},peer);}
}

LoopbackSession::~LoopbackSession(){leave();m_hub.sessions.erase(m_id);}
VoiceStatus LoopbackSession::voiceStatus()const{
 VoiceStatus s;s.available=m_signedIn;if(!m_signedIn){s.reason="not signed in yet";return s;}
 s.inRoom=!m_lobbyId.empty()||fakeVoiceRoom;
 s.transmitting=s.inRoom&&(m_voice.mode==VoiceMode::PushToTalk?m_talking:m_voice.mode==VoiceMode::OpenMic&&fakeMicLevel>=m_voice.openMicThreshold);
 return s;
}
bool LoopbackSession::peerSpeaking(const PeerId& peer)const{
 if(m_voice.mode==VoiceMode::Off||m_lobbyId.empty())return false;
 if(auto m=mutedPeers.find(peer);m!=mutedPeers.end()&&m->second)return false;
 const auto it=m_hub.sessions.find(peer);if(it==m_hub.sessions.end())return false;
 const LoopbackSession& other=*it->second;
 return other.m_lobbyId==m_lobbyId&&other.voiceStatus().transmitting;
}
void LoopbackSession::tick(){
 if(!m_signedIn){m_signedIn=true;m_state=SessionState::Ready;}
 ++m_hub.clock;
 for(auto it=m_hub.flight.begin();it!=m_hub.flight.end();){
  if(it->to==m_id&&it->due<=m_hub.clock){m_inbox.push_back(std::move(it->packet));it=m_hub.flight.erase(it);}else ++it;
 }
}
bool LoopbackSession::ready(const char* what){
 if(!m_signedIn){fail(std::string("not signed in yet: cannot ")+what);return false;}
 if(m_state==SessionState::Failed)m_state=m_lobbyId.empty()?SessionState::Ready:SessionState::InLobby;
 return true;
}
void LoopbackSession::fail(const std::string& why){m_state=SessionState::Failed;m_reason=why;m_events.push_back({SessionEvent::Error,{},why});}
void LoopbackSession::enter(LoopbackHub::Lobby& lobby,SessionEvent::Kind kind){
 if(lobby.info.players>=lobby.info.maxPlayers){fail("that game is full");return;}
 lobby.members.push_back(m_id);lobby.info.players=int(lobby.members.size());
 m_lobbyId=lobby.info.id;m_state=SessionState::InLobby;m_events.push_back({kind,m_id,{}});
 m_hub.broadcastEvent(lobby,{SessionEvent::MemberJoined,m_id,{}},m_id);
}
void LoopbackSession::host(const HostOptions& o){
 if(!ready("host"))return;leave();
 LoopbackHub::Lobby l;l.info.id="lobby-"+std::to_string(m_hub.nextId++);l.info.owner=m_id;
 l.info.name=o.name.empty()?m_name+"'s game":o.name;l.info.map=o.map;l.info.mapHash=o.mapHash;l.info.mode=o.mode;
 l.info.maxPlayers=std::clamp(o.maxPlayers,2,16);l.info.privateGame=o.privateGame;l.info.joinCode=makeJoinCode(m_hub.nextId*7919+m_hub.clock);l.info.pingMs=0;
 auto& slot=m_hub.lobbies[l.info.id]=l;enter(slot,SessionEvent::Hosted);
}
void LoopbackSession::search(const SearchFilter& f){
 if(!ready("search"))return;m_results.clear();
 for(const auto& [id,l]:m_hub.lobbies){
  if(l.info.privateGame)continue;
  if(!f.anyMode&&l.info.mode!=f.mode)continue;
  if(!f.map.empty()&&l.info.map!=f.map)continue;
  if(f.hideFull&&l.info.players>=l.info.maxPlayers)continue;
  LobbyInfo row=l.info;row.joinCode.clear();m_results.push_back(row);
  if(int(m_results.size())>=f.maxResults)break;
 }
 m_events.push_back({SessionEvent::SearchDone,{},{}});
}
void LoopbackSession::join(const std::string& lobbyId){
 if(!ready("join"))return;
 auto it=m_hub.lobbies.find(lobbyId);if(it==m_hub.lobbies.end()){fail("that game has ended");return;}
 if(it->second.info.id==m_lobbyId)return;leave();it=m_hub.lobbies.find(lobbyId);if(it==m_hub.lobbies.end()){fail("that game has ended");return;}
 enter(it->second,SessionEvent::Joined);
}
void LoopbackSession::joinCode(const std::string& typed){
 if(!ready("join"))return;const std::string code=normaliseJoinCode(typed);
 if(code.empty()){fail("that is not a join code: six letters and numbers, like K7QM-2X");return;}
 for(auto& [id,l]:m_hub.lobbies)if(l.info.joinCode==code){join(id);return;}
 fail("no game has the code "+code);
}
void LoopbackSession::quickMatch(Mode mode){
 if(!ready("find a game"))return;
 LoopbackHub::Lobby* best=nullptr;
 for(auto& [id,l]:m_hub.lobbies)if(!l.info.privateGame&&l.info.mode==mode&&l.info.players<l.info.maxPlayers&&(!best||l.info.players>best->info.players))best=&l;
 if(best){join(best->info.id);return;}
 HostOptions o;o.mode=mode;host(o);
}
void LoopbackSession::leave(){
 if(m_lobbyId.empty())return;
 m_hub.removeMember(m_id);m_lobbyId.clear();if(m_signedIn)m_state=SessionState::Ready;
 m_hub.flight.erase(std::remove_if(m_hub.flight.begin(),m_hub.flight.end(),[&](const LoopbackHub::InFlight& f){return f.to==m_id;}),m_hub.flight.end());
 m_inbox.clear();
}
bool LoopbackSession::isHost()const{auto it=m_hub.lobbies.find(m_lobbyId);return it!=m_hub.lobbies.end()&&it->second.info.owner==m_id;}
PeerId LoopbackSession::hostId()const{auto it=m_hub.lobbies.find(m_lobbyId);return it!=m_hub.lobbies.end()?it->second.info.owner:PeerId{};}
const LobbyInfo& LoopbackSession::lobby()const{auto it=m_hub.lobbies.find(m_lobbyId);return it!=m_hub.lobbies.end()?it->second.info:m_none;}
std::vector<PeerId> LoopbackSession::members()const{auto it=m_hub.lobbies.find(m_lobbyId);return it!=m_hub.lobbies.end()?it->second.members:std::vector<PeerId>{};}
void LoopbackSession::setLobbyPlayers(int count){auto it=m_hub.lobbies.find(m_lobbyId);if(it!=m_hub.lobbies.end()&&it->second.info.owner==m_id)it->second.info.players=count;}
bool LoopbackSession::send(const PeerId& to,std::uint8_t channel,const std::uint8_t* data,std::size_t size,bool reliable){
 auto it=m_hub.lobbies.find(m_lobbyId);if(it==m_hub.lobbies.end())return false;
 const auto& m=it->second.members;if(to==m_id||std::find(m.begin(),m.end(),to)==m.end())return false;
 if(!reliable&&m_hub.unreliableLossPercent>0){auto& x=m_hub.lossState;x^=x<<13;x^=x>>7;x^=x<<17;if(int(x%100)<m_hub.unreliableLossPercent)return true;}
 LoopbackHub::InFlight f;f.to=to;f.reliable=reliable;f.due=m_hub.clock+std::uint64_t(std::max(1,m_hub.latencyTicks));
 f.packet.from=m_id;f.packet.channel=channel;f.packet.data.assign(data,data+size);m_hub.flight.push_back(std::move(f));return true;
}
bool LoopbackSession::receive(Packet& out){if(m_inbox.empty())return false;out=std::move(m_inbox.front());m_inbox.pop_front();return true;}
bool LoopbackSession::pollEvent(SessionEvent& out){if(m_events.empty())return false;out=m_events.front();m_events.pop_front();return true;}

bool testLoopback(std::ostream& report){
 bool ok=true;auto check=[&](bool c,const char* what){report<<(c?"PASS ":"FAIL ")<<what<<'\n';ok&=c;};
 check(normaliseJoinCode("k7qm 2x")=="K7QM-2X","a typed code is read whatever its case and spacing");
 check(normaliseJoinCode("K7QM-2")==""&&normaliseJoinCode("K7QM-2X!")=="","a wrong length or stray character is no code");
 check(normaliseJoinCode(makeJoinCode(42))==makeJoinCode(42),"a made code reads back as itself");
 LoopbackHub hub;auto a=hub.connect("Ana"),b=hub.connect("Ben"),c=hub.connect("Cy");
 for(auto* s:{a.get(),b.get(),c.get()})s->tick();
 check(a->state()==SessionState::Ready&&!a->self().empty(),"a session is signed in after its first tick");
 HostOptions o;o.mode=Mode::Deathmatch;o.map="Sample Facility.rmc";o.maxPlayers=2;a->host(o);
 check(a->state()==SessionState::InLobby&&a->isHost()&&!a->lobby().joinCode.empty(),"hosting puts the host in its own lobby with a code");
 SearchFilter f;b->search(f);
 check(b->results().size()==1&&b->results()[0].joinCode.empty()&&b->results()[0].map=="Sample Facility.rmc","the browser lists the public game, without its code");
 b->joinCode(a->lobby().joinCode);
 check(b->state()==SessionState::InLobby&&b->hostId()==a->self()&&!b->isHost(),"a code joins the host's lobby");
 SessionEvent e;bool joined=false;while(a->pollEvent(e))joined|=e.kind==SessionEvent::MemberJoined&&e.peer==b->self();
 check(joined,"the host hears the guest join");
 b->search(f);check(b->results().empty(),"a full game is hidden from the browser");
 c->joinCode(a->lobby().joinCode);check(c->state()==SessionState::Failed,"a full game refuses a third player");
 const std::uint8_t hello[]={1,2,3};a->send(b->self(),Control,hello,3,true);
 Packet p;check(!b->receive(p),"a packet is not there in the same tick");
 b->tick();check(b->receive(p)&&p.from==a->self()&&p.data.size()==3&&p.channel==Control,"a packet arrives on the next tick, from its sender, on its channel");
 a->leave();
 bool handed=false;while(b->pollEvent(e))handed|=e.kind==SessionEvent::HostChanged&&e.peer==b->self();
 check(handed&&b->isHost(),"when the host leaves the lobby passes to the guest");
 c->quickMatch(Mode::Coop);check(c->isHost()&&c->lobby().mode==Mode::Coop,"quick match with no open game hosts one");
 a->quickMatch(Mode::Coop);check(a->hostId()==c->self(),"quick match joins the open game of its mode");
 HostOptions hidden;hidden.privateGame=true;b->host(hidden);a->search(f);bool listed=false;for(auto& r:a->results())listed|=r.id==b->lobby().id;
 check(!listed,"a private game is never listed");
 hub.unreliableLossPercent=100;check(c->send(a->self(),State,hello,3,false),"an unreliable send to a member reports sent");
 a->tick();check(!a->receive(p),"a lossy hub drops it");
 return ok;
}
}

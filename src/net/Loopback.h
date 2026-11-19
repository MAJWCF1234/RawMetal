#pragma once
// An in-memory Session for tests: several LoopbackSessions on one LoopbackHub talk as if over EOS, in one
// process, deterministically. Packets sent during a frame arrive on the receiver's next tick after the hub's
// latency in ticks; the hub can drop unreliable packets (loss) to prove the game copes. Lobbies, codes, quick
// match, host hand-over on leave and the browser's filters behave as Session promises.
#include "NetTypes.h"
#include <deque>
#include <map>
#include <memory>
namespace retro::net {

class LoopbackSession;

class LoopbackHub{
public:
 int latencyTicks=1;          // a packet sent on tick t arrives on the receiver's tick t+latencyTicks
 int unreliableLossPercent=0; // 0-100, deterministic
 std::unique_ptr<LoopbackSession> connect(const std::string& name);  // signed in on its first tick
 // Internals the sessions share.
 struct Lobby{LobbyInfo info;std::vector<PeerId> members;};
 struct InFlight{PeerId to;Packet packet;std::uint64_t due=0;bool reliable=true;};
 std::map<std::string,Lobby> lobbies;          // by lobby id
 std::deque<InFlight> flight;
 std::map<PeerId,LoopbackSession*> sessions;
 std::uint64_t clock=0,nextId=1,lossState=0x9E3779B97F4A7C15ull;
 Lobby* lobbyOf(const PeerId& peer);
 void removeMember(const PeerId& peer);
 void broadcastEvent(const Lobby& lobby,const SessionEvent& event,const PeerId& except);
};

class LoopbackSession final:public Session{
public:
 LoopbackSession(LoopbackHub& hub,PeerId id,std::string name):m_hub(hub),m_id(std::move(id)),m_name(std::move(name)){}
 ~LoopbackSession()override;
 void tick()override;
 SessionState state()const override{return m_state;}
 std::string reason()const override{return m_reason;}
 PeerId self()const override{return m_signedIn?m_id:PeerId{};}
 std::string displayName()const override{return m_name;}
 void host(const HostOptions& options)override;
 void search(const SearchFilter& filter)override;
 const std::vector<LobbyInfo>& results()const override{return m_results;}
 void join(const std::string& lobbyId)override;
 void joinCode(const std::string& code)override;
 void quickMatch(Mode mode)override;
 void leave()override;
 bool isHost()const override;
 PeerId hostId()const override;
 const LobbyInfo& lobby()const override;
 std::vector<PeerId> members()const override;
 void setLobbyPlayers(int count)override;
 bool send(const PeerId& to,std::uint8_t channel,const std::uint8_t* data,std::size_t size,bool reliable)override;
 bool receive(Packet& out)override;
 bool pollEvent(SessionEvent& out)override;
 // The hub's side.
 void deliver(Packet&& packet){m_inbox.push_back(std::move(packet));}
 void event(const SessionEvent& e){m_events.push_back(e);}
 void lobbyGone(){m_state=SessionState::Ready;m_lobbyId.clear();}
private:
 bool ready(const char* what);
 void fail(const std::string& why);
 void enter(LoopbackHub::Lobby& lobby,SessionEvent::Kind kind);
 LoopbackHub& m_hub;PeerId m_id;std::string m_name;
 bool m_signedIn=false;SessionState m_state=SessionState::SigningIn;std::string m_reason;
 std::string m_lobbyId;std::vector<LobbyInfo> m_results;
 std::deque<Packet> m_inbox;std::deque<SessionEvent> m_events;
 LobbyInfo m_none{};
};

// --net-loopback-test: the hub's lobbies, codes, quick match, hand-over and packets.
bool testLoopback(std::ostream& report);
}

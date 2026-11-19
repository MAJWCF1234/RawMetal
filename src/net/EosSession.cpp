#include "EosSession.h"
#include <ostream>
namespace retro::net {
// Placeholder until the EOS layer lands: always Offline.
namespace {
class OfflineSession final:public Session{
 std::vector<LobbyInfo> m_results;LobbyInfo m_none;
public:
 void tick()override{}
 SessionState state()const override{return SessionState::Offline;}
 std::string reason()const override{return "multiplayer is not built into this copy";}
 PeerId self()const override{return {};}
 std::string displayName()const override{return {};}
 void host(const HostOptions&)override{}
 void search(const SearchFilter&)override{}
 const std::vector<LobbyInfo>& results()const override{return m_results;}
 void join(const std::string&)override{}
 void joinCode(const std::string&)override{}
 void quickMatch(Mode)override{}
 void leave()override{}
 bool isHost()const override{return false;}
 PeerId hostId()const override{return {};}
 const LobbyInfo& lobby()const override{return m_none;}
 std::vector<PeerId> members()const override{return {};}
 void setLobbyPlayers(int)override{}
 bool send(const PeerId&,std::uint8_t,const std::uint8_t*,std::size_t,bool)override{return false;}
 bool receive(Packet&)override{return false;}
 bool pollEvent(SessionEvent&)override{return false;}
};
}
std::unique_ptr<Session> makeEosSession(){return std::make_unique<OfflineSession>();}
bool testEos(std::ostream& report){report<<"FAIL the EOS layer is not built yet\n";return false;}
}

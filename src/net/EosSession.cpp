#include "EosSession.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>
#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <ostream>
#include <span>
#include <string>
#include <utility>
#include <vector>
#if RAWMETAL_EOS
#include <windows.h>
#include <delayimp.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include "EosConfig.h"
#include <eos_sdk.h>
#include <eos_logging.h>
#include <eos_connect.h>
#include <eos_lobby.h>
#include <eos_p2p.h>
#include "../core/PackedResource.h"
#include "../core/AssetHash.h"
#endif

namespace retro::net {
namespace {

// ----- The wire: whole messages and fragments (no EOS here, so it is tested without the network) -----
// Every EOS packet starts with one byte. 'W': the rest is one whole message. 'F': a fragment of a reliable
// message, then u16 message number, u16 index, u16 count (little endian), then up to EosPacketMax-7 bytes.
// Reliable packets on one channel arrive in order, and one send() emits all of a message's fragments in a row,
// so fragments from one peer on one channel arrive consecutively; unreliable packets are always whole and may
// land between them without harm.
constexpr std::uint8_t WireWhole='W',WireFragment='F';
constexpr std::size_t FragmentHeader=7,FragmentPayload=EosPacketMax-FragmentHeader;
constexpr std::size_t MaxFragments=(EosReliableMax+FragmentPayload-1)/FragmentPayload;

// The packets for one message; empty when it cannot be sent (too big for its kind).
std::vector<std::vector<std::uint8_t>> splitMessage(const std::uint8_t* data,std::size_t size,bool reliable,std::uint16_t number){
 std::vector<std::vector<std::uint8_t>> out;
 if(size<=EosUnreliableMax){
  std::vector<std::uint8_t> p(size+1);p[0]=WireWhole;if(size)std::memcpy(p.data()+1,data,size);out.push_back(std::move(p));return out;
 }
 if(!reliable||size>EosReliableMax)return out;
 const std::size_t count=(size+FragmentPayload-1)/FragmentPayload;
 for(std::size_t i=0;i<count;++i){
  const std::size_t at=i*FragmentPayload,n=std::min(FragmentPayload,size-at);
  std::vector<std::uint8_t> p(FragmentHeader+n);p[0]=WireFragment;
  p[1]=std::uint8_t(number);p[2]=std::uint8_t(number>>8);p[3]=std::uint8_t(i);p[4]=std::uint8_t(i>>8);p[5]=std::uint8_t(count);p[6]=std::uint8_t(count>>8);
  std::memcpy(p.data()+FragmentHeader,data+at,n);out.push_back(std::move(p));
 }
 return out;
}

// Puts fragments back together, per peer and channel. A broken run (a gap, a stray index, a new message before
// the last one ended, an oversize total) is dropped rather than delivered wrong.
class Reassembler{
 struct Partial{std::uint16_t number=0,count=0,next=0;std::vector<std::uint8_t> bytes;};
 std::map<std::pair<PeerId,std::uint8_t>,Partial> m_partials;
public:
 // True when a whole message is ready in out.
 bool accept(const PeerId& from,std::uint8_t channel,const std::uint8_t* data,std::size_t size,std::vector<std::uint8_t>& out){
  if(size<1)return false;
  if(data[0]==WireWhole){out.assign(data+1,data+size);return true;}
  if(data[0]!=WireFragment||size<=FragmentHeader)return false;
  const std::uint16_t number=std::uint16_t(data[1]|data[2]<<8),index=std::uint16_t(data[3]|data[4]<<8),count=std::uint16_t(data[5]|data[6]<<8);
  const auto key=std::make_pair(from,channel);
  if(count<2||count>MaxFragments||index>=count||size-FragmentHeader>FragmentPayload){m_partials.erase(key);return false;}
  if(index==0){Partial& p=m_partials[key];p.number=number;p.count=count;p.next=0;p.bytes.clear();}
  auto it=m_partials.find(key);
  if(it==m_partials.end())return false;
  Partial& p=it->second;
  if(p.number!=number||p.count!=count||p.next!=index){m_partials.erase(it);return false;}
  // Every fragment but the last is full: anything else means the run is not what was sent.
  if(index+1<count&&size-FragmentHeader!=FragmentPayload){m_partials.erase(it);return false;}
  p.bytes.insert(p.bytes.end(),data+FragmentHeader,data+size);++p.next;
  if(p.bytes.size()>EosReliableMax){m_partials.erase(it);return false;}
  if(p.next<p.count)return false;
  out=std::move(p.bytes);m_partials.erase(it);return true;
 }
 void forget(const PeerId& peer){for(auto it=m_partials.begin();it!=m_partials.end();)if(it->first.first==peer)it=m_partials.erase(it);else ++it;}
 void clear(){m_partials.clear();}
 std::size_t pending()const{return m_partials.size();}
};

bool testWire(std::ostream& report){
 bool ok=true;auto check=[&](bool c,const std::string& what){report<<(c?"PASS ":"FAIL ")<<what<<'\n';ok&=c;};
 auto pattern=[](std::size_t n,unsigned seed){std::vector<std::uint8_t> v(n);for(std::size_t i=0;i<n;++i)v[i]=std::uint8_t((i*131+seed*7+(i>>8))^(i>>3));return v;};
 auto roundTrip=[&](std::size_t n,bool reliable)->bool{
  auto msg=pattern(n,unsigned(n));auto packets=splitMessage(msg.data(),msg.size(),reliable,std::uint16_t(n));
  if(packets.empty())return false;
  for(auto& p:packets)if(p.size()>EosPacketMax)return false;
  Reassembler r;std::vector<std::uint8_t> out;int whole=0;
  for(std::size_t i=0;i<packets.size();++i){bool done=r.accept("peer",Events,packets[i].data(),packets[i].size(),out);if(done){++whole;if(i+1!=packets.size())return false;}}
  return whole==1&&out==msg&&r.pending()==0;
 };
 bool sizes=true;std::string failed;
 for(std::size_t n:{std::size_t(0),std::size_t(1),EosUnreliableMax-1,EosUnreliableMax,EosUnreliableMax+1,FragmentPayload*2,FragmentPayload*2+1,std::size_t(20000),EosReliableMax-1,EosReliableMax})
  if(!roundTrip(n,true)){sizes=false;failed+=" "+std::to_string(n);}
 check(sizes,"reliable messages of 0 B to 64 KB come back whole and byte-exact, every packet within 1170 bytes"+(failed.empty()?std::string():" (failed:"+failed+")"));
 check(roundTrip(EosUnreliableMax,false),"an unreliable message of 1169 bytes goes as one packet");
 {auto m=pattern(EosUnreliableMax+1,1);check(splitMessage(m.data(),m.size(),false,1).empty(),"an unreliable message over the packet limit is refused");}
 {auto m=pattern(EosReliableMax+1,2);check(splitMessage(m.data(),m.size(),true,1).empty(),"a reliable message over 64 KB is refused");}
 {auto m=pattern(EosReliableMax,3);check(splitMessage(m.data(),m.size(),true,1).size()==MaxFragments,"64 KB takes "+std::to_string(MaxFragments)+" fragments");}
 {// Unreliable snapshots land between one message's fragments, and two peers' and two channels' runs interleave.
  auto a=pattern(5000,4),b=pattern(3000,5),c=pattern(2500,6),snap=pattern(600,7);
  auto pa=splitMessage(a.data(),a.size(),true,10),pb=splitMessage(b.data(),b.size(),true,11),pc=splitMessage(c.data(),c.size(),true,12),ps=splitMessage(snap.data(),snap.size(),false,0);
  Reassembler r;std::vector<std::uint8_t> out;std::vector<std::vector<std::uint8_t>> gotA,gotB,gotC,gotS;
  std::size_t ia=0,ib=0,ic=0;
  while(ia<pa.size()||ib<pb.size()||ic<pc.size()){
   if(ia<pa.size()&&r.accept("ana",Events,pa[ia].data(),pa[ia].size(),out))gotA.push_back(out);++ia;
   if(r.accept("ana",Events,ps[0].data(),ps[0].size(),out))gotS.push_back(out);
   if(ib<pb.size()&&r.accept("ben",Events,pb[ib].data(),pb[ib].size(),out))gotB.push_back(out);++ib;
   if(ic<pc.size()&&r.accept("ana",MapData,pc[ic].data(),pc[ic].size(),out))gotC.push_back(out);++ic;
  }
  check(gotA.size()==1&&gotA[0]==a&&gotB.size()==1&&gotB[0]==b&&gotC.size()==1&&gotC[0]==c&&gotS.size()==pa.size()&&gotS[0]==snap,
        "interleaved peers, channels and unreliable packets each reassemble whole");
 }
 {// A lost or reordered fragment drops that message only; the next one still arrives.
  auto a=pattern(4000,8),b=pattern(4000,9);auto pa=splitMessage(a.data(),a.size(),true,20),pb=splitMessage(b.data(),b.size(),true,21);
  Reassembler r;std::vector<std::uint8_t> out;int delivered=0;
  for(std::size_t i=0;i<pa.size();++i)if(i!=1&&r.accept("p",Events,pa[i].data(),pa[i].size(),out))++delivered;
  bool bWhole=false;for(auto& p:pb)if(r.accept("p",Events,p.data(),p.size(),out))bWhole=out==b;
  check(delivered==0&&bWhole&&r.pending()==0,"a gap drops the broken message and the next message still comes through");
 }
 {// Garbage: empty, unknown kind, short headers, impossible counts.
  Reassembler r;std::vector<std::uint8_t> out;bool any=false;
  const std::uint8_t unknown[]={'X',1,2},shortFrag[]={'F',1,0,0},badCount[]={'F',1,0,0,0,0x10,0x27,9},badIndex[]={'F',1,0,5,0,3,0,9};
  any|=r.accept("p",0,unknown,0,out);any|=r.accept("p",0,unknown,sizeof unknown,out);any|=r.accept("p",0,shortFrag,sizeof shortFrag,out);
  any|=r.accept("p",0,badCount,sizeof badCount,out);any|=r.accept("p",0,badIndex,sizeof badIndex,out);
  check(!any&&r.pending()==0,"malformed packets are dropped, never delivered");
  auto a=pattern(3000,10);auto pa=splitMessage(a.data(),a.size(),true,30);
  r.accept("p",0,pa[0].data(),pa[0].size(),out);r.forget("p");bool late=false;for(std::size_t i=1;i<pa.size();++i)late|=r.accept("p",0,pa[i].data(),pa[i].size(),out);
  check(!late&&r.pending()==0,"a peer who leaves mid-message leaves nothing half-built");
 }
 return ok;
}

class OfflineSession final:public Session{
 std::string m_reason;std::vector<LobbyInfo> m_results;LobbyInfo m_none;std::deque<SessionEvent> m_events;
 void refuse(){m_events.push_back({SessionEvent::Error,{},m_reason});}
public:
 explicit OfflineSession(std::string why):m_reason(std::move(why)){}
 void tick()override{}
 SessionState state()const override{return SessionState::Offline;}
 std::string reason()const override{return m_reason;}
 PeerId self()const override{return {};}
 std::string displayName()const override{return {};}
 void host(const HostOptions&)override{refuse();}
 void search(const SearchFilter&)override{refuse();}
 const std::vector<LobbyInfo>& results()const override{return m_results;}
 void join(const std::string&)override{refuse();}
 void joinCode(const std::string&)override{refuse();}
 void quickMatch(Mode)override{refuse();}
 void leave()override{}
 bool isHost()const override{return false;}
 PeerId hostId()const override{return {};}
 const LobbyInfo& lobby()const override{return m_none;}
 std::vector<PeerId> members()const override{return {};}
 void setLobbyPlayers(int)override{}
 bool send(const PeerId&,std::uint8_t,const std::uint8_t*,std::size_t,bool)override{return false;}
 bool receive(Packet&)override{return false;}
 bool pollEvent(SessionEvent& out)override{if(m_events.empty())return false;out=m_events.front();m_events.pop_front();return true;}
};

} // namespace
} // namespace retro::net

#if RAWMETAL_EOS
// ----- Loading the packed DLL -----
// The import library is delay-loaded (/DELAYLOAD:EOSSDK-Win64-Shipping.dll). The session unpacks the DLL and
// loads it by full path before the first EOS call; this hook hands that module to the delay-load helper.
namespace {
HMODULE g_eosModule=nullptr;
FARPROC WINAPI eosDelayHook(unsigned notify,PDelayLoadInfo info){
 if(notify==dliNotePreLoadLibrary&&g_eosModule&&info&&info->szDll&&_stricmp(info->szDll,"EOSSDK-Win64-Shipping.dll")==0)
  return reinterpret_cast<FARPROC>(g_eosModule);
 return nullptr;
}
}
extern "C" const PfnDliHook __pfnDliNotifyHook2=eosDelayHook;

namespace retro::net {
namespace {
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;

std::wstring exeFolder(){wchar_t path[MAX_PATH*4]{};DWORD n=GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));if(!n||n>=std::size(path))return L".";return fs::path(path).parent_path().wstring();}
std::wstring localAppData(){wchar_t buffer[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",buffer,32768);if(n>0&&n<32768)return buffer;wchar_t temp[MAX_PATH+1]{};GetTempPathW(MAX_PATH,temp);return temp;}
std::string utf8(const std::wstring& w){if(w.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(std::max(0,n)),'\0');if(n>0)WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}

// RawMetal-eos.txt beside the exe: warnings from the SDK and the session's own results. EOS may log from its
// own threads, hence the lock. The client secret is never written.
struct EosLog{
 std::mutex lock;std::ofstream file;Clock::time_point start=Clock::now();
 void open(){std::lock_guard<std::mutex> hold(lock);if(!file.is_open())file.open(fs::path(exeFolder())/"RawMetal-eos.txt",std::ios::trunc);}
 void line(const std::string& text){
  std::lock_guard<std::mutex> hold(lock);if(!file.is_open())return;
  char stamp[32];std::snprintf(stamp,sizeof stamp,"%8.3f ",std::chrono::duration<double>(Clock::now()-start).count());file<<stamp<<text<<'\n';file.flush();
 }
};
EosLog g_log;
void EOS_CALL onEosLog(const EOS_LogMessage* m){
 if(!m)return;const char* level=m->Level<=EOS_ELogLevel::EOS_LOG_Error?"error":"warning";
 g_log.line(std::string("[eos ")+level+" "+(m->Category?m->Category:"")+"] "+(m->Message?m->Message:""));
}

struct DllLoad{HMODULE module=nullptr;std::string error;std::wstring path;std::size_t packed=0,unpacked=0;bool reused=false;double ms=0;};
// Off the game's thread: find resource 5990, unpack it once per content hash, load it.
DllLoad loadPackedDll(){
 DllLoad r;const auto t0=Clock::now();
 try{
  HMODULE self=GetModuleHandleW(nullptr);
  HRSRC res=FindResourceW(self,MAKEINTRESOURCEW(RAWMETAL_EOS_DLL_RESOURCE),MAKEINTRESOURCEW(10));
  if(!res){r.error="this copy has no EOS library packed inside";return r;}
  const auto* bytes=static_cast<const unsigned char*>(LockResource(LoadResource(self,res)));const std::size_t size=SizeofResource(self,res);
  if(!bytes||size<12||std::memcmp(bytes,"RMZ1",4)){r.error="the packed EOS library is damaged";return r;}
  std::uint32_t count=0;std::memcpy(&count,bytes+4,4);r.packed=size;r.unpacked=count;
  const auto hash=assetHash(std::span<const unsigned char>(bytes,size));
  char hex[17];for(int i=0;i<8;++i)std::snprintf(hex+2*i,3,"%02x",hash[size_t(i)]);
  const fs::path folder=fs::path(localAppData())/L"RawMetal"/L"eos"/hex,path=folder/L"EOSSDK-Win64-Shipping.dll";
  r.path=path.wstring();
  std::error_code ec;
  if(fs::exists(path,ec)&&fs::file_size(path,ec)==count&&!ec)r.reused=true;
  else{
   const auto dll=loadResourceFromModule(self,RAWMETAL_EOS_DLL_RESOURCE);
   fs::create_directories(folder,ec);if(ec){r.error="cannot make "+utf8(folder.wstring())+": "+ec.message();return r;}
   const fs::path temporary=folder/(L"unpack-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
   {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(dll.data()),std::streamsize(dll.size()));if(!out){r.error="cannot write the EOS library to "+utf8(folder.wstring());return r;}}
   if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
    const DWORD why=GetLastError();fs::remove(temporary,ec);
    // Another copy of the game may have unpacked it in the same moment: take theirs if it is whole.
    if(!(fs::exists(path,ec)&&fs::file_size(path,ec)==count)){r.error="cannot place the EOS library (Windows error "+std::to_string(why)+")";return r;}
   }
  }
  r.module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if(!r.module)r.error="the EOS library would not load (Windows error "+std::to_string(GetLastError())+")";
 }catch(const std::exception& e){r.error=std::string("the EOS library could not be unpacked: ")+e.what();}
 r.ms=std::chrono::duration<double,std::milli>(Clock::now()-t0).count();
 return r;
}

std::string idText(EOS_ProductUserId id){
 if(!id)return {};char text[EOS_PRODUCTUSERID_MAX_LENGTH+1]{};int32_t n=sizeof text;
 return EOS_ProductUserId_ToString(id,text,&n)==EOS_EResult::EOS_Success?std::string(text):std::string();
}
std::string resultName(EOS_EResult r){const char* s=EOS_EResult_ToString(r);return s?s:"unknown";}
// An EOS result in words a player can act on.
std::string plain(EOS_EResult r){
 switch(r){
  case EOS_EResult::EOS_NoConnection:return "no connection to Epic's servers (is the internet up?)";
  case EOS_EResult::EOS_TimedOut:return "Epic's servers did not answer in time";
  case EOS_EResult::EOS_TooManyRequests:return "Epic's servers are busy; try again shortly";
  case EOS_EResult::EOS_InvalidCredentials:case EOS_EResult::EOS_InvalidAuth:case EOS_EResult::EOS_AccessDenied:
  case EOS_EResult::EOS_MissingPermissions:
   return "Epic refused this copy's multiplayer keys ("+resultName(r)+")";
  case EOS_EResult::EOS_Lobby_TooManyPlayers:return "that game is full";
  case EOS_EResult::EOS_NotFound:case EOS_EResult::EOS_Lobby_LobbyAlreadyExists:return "that game has ended";
  case EOS_EResult::EOS_ServiceFailure:return "Epic's lobby service failed; try again";
  default:return "Epic said "+resultName(r);
 }
}

const char* modeKey(Mode m){return m==Mode::Coop?"coop":"deathmatch";}
void* serialData(std::uint64_t serial){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(serial));}
std::uint64_t serialOf(void* data){return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));}

bool g_eosInitialized=false,g_eosShutDown=false;

class EosSession;
EosSession* g_live=nullptr;   // EOS is one per process, so is this session; callbacks find it here

class EosSession final:public Session{
public:
 EosSession(){
  g_live=this;
  wchar_t user[257]{};DWORD n=257;std::wstring name=GetUserNameW(user,&n)?std::wstring(user):std::wstring(L"Player");
  if(name.empty())name=L"Player";
  m_name=utf8(name);while(m_name.size()>EOS_CONNECT_USERLOGININFO_DISPLAYNAME_MAX_LENGTH&&!name.empty()){name.pop_back();m_name=utf8(name);}
 }
 ~EosSession()override{shutdown();if(g_live==this)g_live=nullptr;}

 void tick()override{
  if(m_phase==Phase::Dead)return;
  if(m_phase==Phase::Start){
   g_log.open();g_log.line("DepthWorks " RAWMETAL_EOS_PRODUCT_VERSION ": multiplayer starting; display name '"+m_name+"'");
   if(g_eosShutDown){goOffline("multiplayer was shut down in this run; restart the game to play online");return;}
   m_phase=Phase::Loading;
   m_loader=std::thread([this]{m_dll=loadPackedDll();m_loaded=true;});
   return;
  }
  if(m_phase==Phase::Loading){
   if(!m_loaded)return;
   m_loader.join();
   if(!m_dll.module){goOffline(m_dll.error);return;}
   g_log.line((m_dll.reused?"reused the EOS library at ":"unpacked the EOS library to ")+utf8(m_dll.path)+" ("+std::to_string(m_dll.packed)+" packed, "+std::to_string(m_dll.unpacked)+" bytes) in "+std::to_string(int(m_dll.ms))+" ms");
   bringUp();
   if(m_phase==Phase::Dead)return;
  }
  if(!m_platform)return;
  EOS_Platform_Tick(m_platform);
  pumpPackets();
  const auto now=Clock::now();
  if(signingIn()&&now-m_signInStarted>std::chrono::seconds(45))signInFailed("Epic's servers did not answer the sign-in");
  if(m_op!=Op::None&&now-m_opStarted>std::chrono::seconds(30)){++m_serial;fail("Epic's servers did not answer in time");}
 }
 SessionState state()const override{return m_state;}
 std::string reason()const override{return m_reason;}
 PeerId self()const override{return m_user?m_selfId:PeerId{};}
 std::string displayName()const override{return m_name;}

 void host(const HostOptions& options)override{
  if(!ready())return;
  leaveLobby();begin(Op::Host);startHost(options);
 }
 void search(const SearchFilter& filter)override{
  if(!ready())return;
  begin(Op::Search);m_filter=filter;startSearch();
 }
 const std::vector<LobbyInfo>& results()const override{return m_results;}
 void join(const std::string& lobbyId)override{
  if(!ready())return;
  if(!m_lobbyId.empty()&&lobbyId==m_lobbyId)return;
  leaveLobby();begin(Op::Join);
  EOS_Lobby_JoinLobbyByIdOptions o{};o.ApiVersion=EOS_LOBBY_JOINLOBBYBYID_API_LATEST;o.LobbyId=lobbyId.c_str();o.LocalUserId=m_user;o.bPresenceEnabled=EOS_FALSE;
  o.RTCRoomJoinActionType=EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin;
  EOS_Lobby_JoinLobbyById(m_lobbyApi,&o,serialData(m_serial),[](const EOS_Lobby_JoinLobbyByIdCallbackInfo* i){if(g_live)g_live->onJoined(serialOf(i->ClientData),i->ResultCode,i->LobbyId);});
 }
 void joinCode(const std::string& typed)override{
  if(!ready())return;
  const std::string code=normaliseJoinCode(typed);
  if(code.empty()){refuse("that is not a join code: six letters and numbers, like K7QM-2X");return;}
  if(!m_lobbyId.empty()&&m_lobby.joinCode==code)return;
  begin(Op::JoinCode);m_code=code;startSearch();
 }
 void quickMatch(Mode mode)override{
  if(!ready())return;
  begin(Op::QuickMatch);m_filter=SearchFilter{};m_filter.anyMode=false;m_filter.mode=mode;m_filter.hideFull=true;m_filter.maxResults=50;startSearch();
 }
 void leave()override{
  if(m_op!=Op::None){++m_serial;m_op=Op::None;}
  leaveLobby();
  if(m_phase==Phase::Ready)m_state=SessionState::Ready;
 }
 bool isHost()const override{return !m_lobbyId.empty()&&!m_selfId.empty()&&m_lobby.owner==m_selfId;}
 PeerId hostId()const override{return m_lobbyId.empty()?PeerId{}:m_lobby.owner;}
 const LobbyInfo& lobby()const override{return m_lobbyId.empty()?m_none:m_lobby;}
 std::vector<PeerId> members()const override{return m_lobbyId.empty()?std::vector<PeerId>{}:m_members;}
 void setLobbyPlayers(int count)override{
  if(!isHost()||count==m_publishedPlayers)return;
  m_lobby.players=count;m_publishedPlayers=count;
  EOS_HLobbyModification mod=nullptr;
  if(!modification(mod))return;
  addInt(mod,"players",count);
  EOS_Lobby_UpdateLobbyOptions u{};u.ApiVersion=EOS_LOBBY_UPDATELOBBY_API_LATEST;u.LobbyModificationHandle=mod;
  EOS_Lobby_UpdateLobby(m_lobbyApi,&u,nullptr,[](const EOS_Lobby_UpdateLobbyCallbackInfo* i){
   if(g_live){g_live->m_playersUpdates++;if(i->ResultCode!=EOS_EResult::EOS_Success)g_log.line("player count update failed: "+resultName(i->ResultCode));}});
  EOS_LobbyModification_Release(mod);
 }

 bool send(const PeerId& to,std::uint8_t channel,const std::uint8_t* data,std::size_t size,bool reliable)override{
  if(m_lobbyId.empty()||!m_user||to==m_selfId)return false;
  if(std::find(m_members.begin(),m_members.end(),to)==m_members.end())return false;
  if(size&&!data)return false;
  const auto packets=splitMessage(data,size,reliable,m_nextMessage);
  if(packets.empty())return false;
  if(packets.size()>1)++m_nextMessage;
  EOS_ProductUserId remote=EOS_ProductUserId_FromString(to.c_str());
  if(!remote)return false;
  for(const auto& p:packets){
   EOS_P2P_SendPacketOptions o{};o.ApiVersion=EOS_P2P_SENDPACKET_API_LATEST;o.LocalUserId=m_user;o.RemoteUserId=remote;o.SocketId=&m_socket;o.Channel=channel;
   o.DataLengthBytes=std::uint32_t(p.size());o.Data=p.data();o.bAllowDelayedDelivery=EOS_TRUE;
   o.Reliability=reliable?EOS_EPacketReliability::EOS_PR_ReliableOrdered:EOS_EPacketReliability::EOS_PR_UnreliableUnordered;o.bDisableAutoAcceptConnection=EOS_FALSE;
   const EOS_EResult r=EOS_P2P_SendPacket(m_p2pApi,&o);
   if(r!=EOS_EResult::EOS_Success){if(m_sendWarnings++<8)g_log.line("send to "+to+" failed: "+resultName(r));return false;}
  }
  return true;
 }
 bool receive(Packet& out)override{if(m_inbox.empty())return false;out=std::move(m_inbox.front());m_inbox.pop_front();return true;}
 bool pollEvent(SessionEvent& out)override{if(m_events.empty())return false;out=m_events.front();m_events.pop_front();return true;}

 // --eos-test only: run the join-code search without joining (the test is already in that lobby). The match,
 // with the code kept, lands in results() and SearchDone follows.
 void lookupCode(const std::string& typed){
  if(!ready())return;
  m_code=normaliseJoinCode(typed);if(m_code.empty()){refuse("not a join code");return;}
  begin(Op::LookupCode);startSearch();
 }
 int playersUpdates()const{return m_playersUpdates;}

private:
 enum class Phase{Start,Loading,DeviceId,Login,CreateUser,SignInFailed,Ready,Dead};
 enum class Op{None,Host,Search,Join,JoinCode,QuickMatch,LookupCode};
 Phase m_phase=Phase::Start;SessionState m_state=SessionState::SigningIn;std::string m_reason;
 std::thread m_loader;std::atomic<bool> m_loaded{false};DllLoad m_dll;
 EOS_HPlatform m_platform=nullptr;EOS_HConnect m_connectApi=nullptr;EOS_HLobby m_lobbyApi=nullptr;EOS_HP2P m_p2pApi=nullptr;
 EOS_NotificationId m_authExpiry=EOS_INVALID_NOTIFICATIONID,m_loginStatus=EOS_INVALID_NOTIFICATIONID,m_memberStatus=EOS_INVALID_NOTIFICATIONID,
                    m_lobbyUpdate=EOS_INVALID_NOTIFICATIONID,m_connectionRequest=EOS_INVALID_NOTIFICATIONID;
 EOS_ProductUserId m_user=nullptr;std::string m_selfId,m_name;bool m_refreshing=false;
 Clock::time_point m_signInStarted{},m_opStarted{};
 Op m_op=Op::None;std::uint64_t m_serial=1;
 HostOptions m_hostOptions;SearchFilter m_filter;std::string m_code;
 std::string m_lobbyId;LobbyInfo m_lobby,m_none;std::vector<PeerId> m_members;int m_publishedPlayers=-1;int m_playersUpdates=0;
 std::vector<LobbyInfo> m_results;
 std::deque<SessionEvent> m_events;std::deque<Packet> m_inbox;Reassembler m_reassembler;std::uint16_t m_nextMessage=1;int m_sendWarnings=0;
 EOS_P2P_SocketId m_socket{};
 bool m_leaving=false;

 bool signingIn()const{return m_phase==Phase::DeviceId||m_phase==Phase::Login||m_phase==Phase::CreateUser;}
 void settle(){m_state=m_lobbyId.empty()?SessionState::Ready:SessionState::InLobby;}
 void goOffline(const std::string& why){m_phase=Phase::Dead;m_state=SessionState::Offline;m_reason=why;g_log.line("offline: "+why);}
 void refuse(const std::string& why){m_events.push_back({SessionEvent::Error,{},why});g_log.line("refused: "+why);}
 void fail(const std::string& why){m_op=Op::None;m_state=SessionState::Failed;m_reason=why;m_events.push_back({SessionEvent::Error,{},why});g_log.line("failed: "+why);}
 void begin(Op op){m_op=op;++m_serial;m_opStarted=Clock::now();m_state=SessionState::Busy;}
 bool current(std::uint64_t serial,Op op)const{return serial==m_serial&&m_op==op;}
 bool ready(){
  if(m_phase==Phase::Dead){refuse(m_reason);return false;}
  if(m_phase==Phase::SignInFailed){startSignIn();refuse("signing in to Epic again; try again in a moment");return false;}
  if(m_phase!=Phase::Ready){refuse("still signing in to Epic; try again in a moment");return false;}
  if(m_op!=Op::None){refuse("another request is still under way");return false;}
  if(m_state==SessionState::Failed)settle();
  return true;
 }

 // ----- Start-up and sign-in -----
 void bringUp(){
  if(!g_eosInitialized){
   g_eosModule=m_dll.module;
   EOS_InitializeOptions init{};init.ApiVersion=EOS_INITIALIZE_API_LATEST;init.ProductName=RAWMETAL_EOS_PRODUCT_NAME;init.ProductVersion=RAWMETAL_EOS_PRODUCT_VERSION;
   const EOS_EResult r=EOS_Initialize(&init);
   if(r!=EOS_EResult::EOS_Success&&r!=EOS_EResult::EOS_AlreadyConfigured){goOffline("Epic's SDK would not start: "+resultName(r));return;}
   g_eosInitialized=true;
   EOS_Logging_SetCallback(&onEosLog);EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES,EOS_ELogLevel::EOS_LOG_Warning);
  }
  const fs::path cache=fs::path(localAppData())/L"RawMetal"/L"eos"/L"cache";std::error_code ec;fs::create_directories(cache,ec);
  const std::string cacheText=utf8(cache.wstring());
  static const unsigned char hidden[]=RAWMETAL_EOS_CLIENT_SECRET_XOR;
  std::string secret;for(std::size_t i=0;i+1<sizeof hidden;++i)secret+=char(hidden[i]^0x5A);
  EOS_Platform_Options p{};p.ApiVersion=EOS_PLATFORM_OPTIONS_API_LATEST;p.ProductId=RAWMETAL_EOS_PRODUCT_ID;p.SandboxId=RAWMETAL_EOS_SANDBOX_ID;p.DeploymentId=RAWMETAL_EOS_DEPLOYMENT_ID;
  p.ClientCredentials.ClientId=RAWMETAL_EOS_CLIENT_ID;p.ClientCredentials.ClientSecret=secret.c_str();p.bIsServer=EOS_FALSE;
  p.Flags=EOS_PF_DISABLE_OVERLAY;p.CacheDirectory=cacheText.c_str();
  const auto t0=Clock::now();
  m_platform=EOS_Platform_Create(&p);
  SecureZeroMemory(secret.data(),secret.size());
  if(!m_platform){goOffline("Epic's platform would not start (are this copy's multiplayer keys right?)");return;}
  g_log.line("platform created in "+std::to_string(int(std::chrono::duration<double,std::milli>(Clock::now()-t0).count()))+" ms");
  m_connectApi=EOS_Platform_GetConnectInterface(m_platform);m_lobbyApi=EOS_Platform_GetLobbyInterface(m_platform);m_p2pApi=EOS_Platform_GetP2PInterface(m_platform);
  m_socket.ApiVersion=EOS_P2P_SOCKETID_API_LATEST;std::strncpy(m_socket.SocketName,"DepthWorks",EOS_P2P_SOCKETID_SOCKETNAME_SIZE-1);
  EOS_P2P_SetRelayControlOptions relay{};relay.ApiVersion=EOS_P2P_SETRELAYCONTROL_API_LATEST;relay.RelayControl=EOS_ERelayControl::EOS_RC_AllowRelays;EOS_P2P_SetRelayControl(m_p2pApi,&relay);
  EOS_Connect_AddNotifyAuthExpirationOptions ae{};ae.ApiVersion=EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
  m_authExpiry=EOS_Connect_AddNotifyAuthExpiration(m_connectApi,&ae,nullptr,[](const EOS_Connect_AuthExpirationCallbackInfo*){if(g_live)g_live->refreshLogin();});
  EOS_Connect_AddNotifyLoginStatusChangedOptions ls{};ls.ApiVersion=EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;
  m_loginStatus=EOS_Connect_AddNotifyLoginStatusChanged(m_connectApi,&ls,nullptr,[](const EOS_Connect_LoginStatusChangedCallbackInfo* i){if(g_live)g_live->onLoginStatus(i->CurrentStatus);});
  startSignIn();
 }
 void startSignIn(){
  m_phase=Phase::DeviceId;m_state=SessionState::SigningIn;m_signInStarted=Clock::now();m_refreshing=false;
  EOS_Connect_CreateDeviceIdOptions o{};o.ApiVersion=EOS_CONNECT_CREATEDEVICEID_API_LATEST;o.DeviceModel="PC Windows";
  EOS_Connect_CreateDeviceId(m_connectApi,&o,nullptr,[](const EOS_Connect_CreateDeviceIdCallbackInfo* i){if(g_live)g_live->onDeviceId(i->ResultCode);});
 }
 void onDeviceId(EOS_EResult r){
  if(m_phase!=Phase::DeviceId)return;
  if(r!=EOS_EResult::EOS_Success&&r!=EOS_EResult::EOS_DuplicateNotAllowed){signInFailed("could not make this PC's device id: "+plain(r));return;}
  g_log.line(std::string("device id: ")+(r==EOS_EResult::EOS_Success?"created":"already here"));
  m_phase=Phase::Login;login();
 }
 void login(){
  EOS_Connect_Credentials c{};c.ApiVersion=EOS_CONNECT_CREDENTIALS_API_LATEST;c.Type=EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;c.Token=nullptr;
  EOS_Connect_UserLoginInfo info{};info.ApiVersion=EOS_CONNECT_USERLOGININFO_API_LATEST;info.DisplayName=m_name.c_str();
  EOS_Connect_LoginOptions o{};o.ApiVersion=EOS_CONNECT_LOGIN_API_LATEST;o.Credentials=&c;o.UserLoginInfo=&info;
  EOS_Connect_Login(m_connectApi,&o,nullptr,[](const EOS_Connect_LoginCallbackInfo* i){if(g_live)g_live->onLogin(i->ResultCode,i->ContinuanceToken,i->LocalUserId);});
 }
 void refreshLogin(){if(m_phase!=Phase::Ready||m_refreshing)return;m_refreshing=true;g_log.line("sign-in expiring: refreshing");login();}
 void onLogin(EOS_EResult r,EOS_ContinuanceToken token,EOS_ProductUserId user){
  if(m_refreshing&&m_phase==Phase::Ready){m_refreshing=false;g_log.line("sign-in refresh: "+resultName(r));return;}
  if(m_phase!=Phase::Login)return;
  if(r==EOS_EResult::EOS_InvalidUser&&token){
   m_phase=Phase::CreateUser;g_log.line("first sign-in from this PC: making its Epic user");
   EOS_Connect_CreateUserOptions o{};o.ApiVersion=EOS_CONNECT_CREATEUSER_API_LATEST;o.ContinuanceToken=token;
   EOS_Connect_CreateUser(m_connectApi,&o,nullptr,[](const EOS_Connect_CreateUserCallbackInfo* i){if(g_live)g_live->onCreateUser(i->ResultCode,i->LocalUserId);});
   return;
  }
  if(r!=EOS_EResult::EOS_Success||!user){signInFailed("could not sign in: "+plain(r));return;}
  signedIn(user);
 }
 void onCreateUser(EOS_EResult r,EOS_ProductUserId user){
  if(m_phase!=Phase::CreateUser)return;
  if(r!=EOS_EResult::EOS_Success||!user){signInFailed("could not make this PC's Epic user: "+plain(r));return;}
  signedIn(user);
 }
 void signedIn(EOS_ProductUserId user){
  m_user=user;m_selfId=idText(user);m_phase=Phase::Ready;settle();m_reason.clear();
  g_log.line("signed in as product user "+m_selfId+" in "+std::to_string(int(std::chrono::duration<double,std::milli>(Clock::now()-m_signInStarted).count()))+" ms");
  if(m_memberStatus==EOS_INVALID_NOTIFICATIONID){
   EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions ms{};ms.ApiVersion=EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;
   m_memberStatus=EOS_Lobby_AddNotifyLobbyMemberStatusReceived(m_lobbyApi,&ms,nullptr,[](const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* i){if(g_live)g_live->onMemberStatus(i->LobbyId,i->TargetUserId,i->CurrentStatus);});
   EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions lu{};lu.ApiVersion=EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST;
   m_lobbyUpdate=EOS_Lobby_AddNotifyLobbyUpdateReceived(m_lobbyApi,&lu,nullptr,[](const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* i){if(g_live&&i->LobbyId&&g_live->m_lobbyId==i->LobbyId)g_live->refreshLobby();});
  }
  if(m_connectionRequest!=EOS_INVALID_NOTIFICATIONID)EOS_P2P_RemoveNotifyPeerConnectionRequest(m_p2pApi,m_connectionRequest);
  EOS_P2P_AddNotifyPeerConnectionRequestOptions cr{};cr.ApiVersion=EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;cr.LocalUserId=m_user;cr.SocketId=&m_socket;
  m_connectionRequest=EOS_P2P_AddNotifyPeerConnectionRequest(m_p2pApi,&cr,nullptr,[](const EOS_P2P_OnIncomingConnectionRequestInfo* i){if(g_live)g_live->onConnectionRequest(i->RemoteUserId);});
 }
 void signInFailed(const std::string& why){
  m_phase=Phase::SignInFailed;m_state=SessionState::Failed;m_reason=why;m_events.push_back({SessionEvent::Error,{},why});g_log.line("sign-in failed: "+why);
 }
 void onLoginStatus(EOS_ELoginStatus status){
  if(status!=EOS_ELoginStatus::EOS_LS_NotLoggedIn||m_phase!=Phase::Ready)return;
  g_log.line("signed out by Epic: signing in again");
  if(!m_lobbyId.empty()){dropLobby();m_events.push_back({SessionEvent::LobbyClosed,{},"signed out"});}
  m_op=Op::None;++m_serial;m_user=nullptr;startSignIn();
 }

 // ----- Lobbies -----
 bool modification(EOS_HLobbyModification& mod){
  EOS_Lobby_UpdateLobbyModificationOptions o{};o.ApiVersion=EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;o.LocalUserId=m_user;o.LobbyId=m_lobbyId.c_str();
  const EOS_EResult r=EOS_Lobby_UpdateLobbyModification(m_lobbyApi,&o,&mod);
  if(r!=EOS_EResult::EOS_Success){g_log.line("cannot change the lobby: "+resultName(r));mod=nullptr;return false;}
  return true;
 }
 static void addText(EOS_HLobbyModification mod,const char* key,const std::string& value){
  EOS_Lobby_AttributeData a{};a.ApiVersion=EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;a.Key=key;a.Value.AsUtf8=value.c_str();a.ValueType=EOS_ELobbyAttributeType::EOS_AT_STRING;
  EOS_LobbyModification_AddAttributeOptions o{};o.ApiVersion=EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;o.Attribute=&a;o.Visibility=EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
  const EOS_EResult r=EOS_LobbyModification_AddAttribute(mod,&o);if(r!=EOS_EResult::EOS_Success)g_log.line(std::string("attribute ")+key+": "+resultName(r));
 }
 static void addInt(EOS_HLobbyModification mod,const char* key,std::int64_t value){
  EOS_Lobby_AttributeData a{};a.ApiVersion=EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;a.Key=key;a.Value.AsInt64=value;a.ValueType=EOS_ELobbyAttributeType::EOS_AT_INT64;
  EOS_LobbyModification_AddAttributeOptions o{};o.ApiVersion=EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;o.Attribute=&a;o.Visibility=EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
  const EOS_EResult r=EOS_LobbyModification_AddAttribute(mod,&o);if(r!=EOS_EResult::EOS_Success)g_log.line(std::string("attribute ")+key+": "+resultName(r));
 }
 static std::string attrText(EOS_HLobbyDetails d,const char* key){
  EOS_LobbyDetails_CopyAttributeByKeyOptions o{};o.ApiVersion=EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;o.AttrKey=key;EOS_Lobby_Attribute* a=nullptr;std::string v;
  if(EOS_LobbyDetails_CopyAttributeByKey(d,&o,&a)==EOS_EResult::EOS_Success&&a){if(a->Data&&a->Data->ValueType==EOS_ELobbyAttributeType::EOS_AT_STRING&&a->Data->Value.AsUtf8)v=a->Data->Value.AsUtf8;EOS_Lobby_Attribute_Release(a);}
  return v;
 }
 static std::int64_t attrInt(EOS_HLobbyDetails d,const char* key,std::int64_t otherwise){
  EOS_LobbyDetails_CopyAttributeByKeyOptions o{};o.ApiVersion=EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;o.AttrKey=key;EOS_Lobby_Attribute* a=nullptr;std::int64_t v=otherwise;
  if(EOS_LobbyDetails_CopyAttributeByKey(d,&o,&a)==EOS_EResult::EOS_Success&&a){if(a->Data&&a->Data->ValueType==EOS_ELobbyAttributeType::EOS_AT_INT64)v=a->Data->Value.AsInt64;EOS_Lobby_Attribute_Release(a);}
  return v;
 }
 // A lobby as LobbyInfo; members filled when asked. version: the host's game version.
 static LobbyInfo readDetails(EOS_HLobbyDetails d,std::vector<PeerId>* members,std::string* version){
  LobbyInfo info;EOS_LobbyDetails_Info* raw=nullptr;EOS_LobbyDetails_CopyInfoOptions ci{};ci.ApiVersion=EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
  if(EOS_LobbyDetails_CopyInfo(d,&ci,&raw)==EOS_EResult::EOS_Success&&raw){
   info.id=raw->LobbyId?raw->LobbyId:"";info.owner=idText(raw->LobbyOwnerUserId);info.maxPlayers=int(raw->MaxMembers);EOS_LobbyDetails_Info_Release(raw);
  }
  EOS_LobbyDetails_GetMemberCountOptions mc{};mc.ApiVersion=EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;const std::uint32_t count=EOS_LobbyDetails_GetMemberCount(d,&mc);
  if(members){members->clear();for(std::uint32_t i=0;i<count;++i){EOS_LobbyDetails_GetMemberByIndexOptions mi{};mi.ApiVersion=EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;mi.MemberIndex=i;
   const std::string id=idText(EOS_LobbyDetails_GetMemberByIndex(d,&mi));if(!id.empty())members->push_back(id);}}
  info.name=attrText(d,"name");info.map=attrText(d,"map");info.mapHash=attrText(d,"maphash");info.joinCode=attrText(d,"joincode");
  info.mode=attrText(d,"mode")=="coop"?Mode::Coop:Mode::Deathmatch;info.privateGame=attrInt(d,"private",0)!=0;
  const std::int64_t players=attrInt(d,"players",0);info.players=players>0?int(players):int(count);
  if(version)*version=attrText(d,"version");
  return info;
 }
 void refreshLobby(){
  if(m_lobbyId.empty())return;
  EOS_Lobby_CopyLobbyDetailsHandleOptions o{};o.ApiVersion=EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;o.LobbyId=m_lobbyId.c_str();o.LocalUserId=m_user;
  EOS_HLobbyDetails d=nullptr;
  if(EOS_Lobby_CopyLobbyDetailsHandle(m_lobbyApi,&o,&d)!=EOS_EResult::EOS_Success||!d)return;
  std::vector<PeerId> members;LobbyInfo info=readDetails(d,&members,nullptr);EOS_LobbyDetails_Release(d);
  info.pingMs=0;if(info.owner==m_selfId&&m_publishedPlayers>0)info.players=m_publishedPlayers;
  m_lobby=info;m_members=members;
  for(const auto& m:m_members)if(m!=m_selfId)accept(m);   // open the P2P path to every member early
 }
 void accept(const PeerId& peer){
  EOS_ProductUserId remote=EOS_ProductUserId_FromString(peer.c_str());if(!remote)return;
  EOS_P2P_AcceptConnectionOptions o{};o.ApiVersion=EOS_P2P_ACCEPTCONNECTION_API_LATEST;o.LocalUserId=m_user;o.RemoteUserId=remote;o.SocketId=&m_socket;
  EOS_P2P_AcceptConnection(m_p2pApi,&o);
 }
 void startHost(const HostOptions& options){
  m_hostOptions=options;m_hostOptions.maxPlayers=std::clamp(options.maxPlayers,2,16);
  if(m_hostOptions.name.empty())m_hostOptions.name=m_name+"'s game";
  EOS_Lobby_CreateLobbyOptions o{};o.ApiVersion=EOS_LOBBY_CREATELOBBY_API_LATEST;o.LocalUserId=m_user;o.MaxLobbyMembers=std::uint32_t(m_hostOptions.maxPlayers);
  o.PermissionLevel=EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;   // private games too: their code must find them
  o.bPresenceEnabled=EOS_FALSE;o.bAllowInvites=EOS_TRUE;o.BucketId=modeKey(m_hostOptions.mode);o.bDisableHostMigration=EOS_FALSE;o.bEnableRTCRoom=EOS_FALSE;
  o.bEnableJoinById=EOS_TRUE;o.bRejoinAfterKickRequiresInvite=EOS_FALSE;o.bCrossplayOptOut=EOS_FALSE;o.RTCRoomJoinActionType=EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin;
  EOS_Lobby_CreateLobby(m_lobbyApi,&o,serialData(m_serial),[](const EOS_Lobby_CreateLobbyCallbackInfo* i){if(g_live)g_live->onCreated(serialOf(i->ClientData),i->ResultCode,i->LobbyId);});
 }
 void onCreated(std::uint64_t serial,EOS_EResult r,const char* lobbyId){
  const bool wanted=current(serial,Op::Host)||current(serial,Op::QuickMatch);
  if(!wanted){if(r==EOS_EResult::EOS_Success&&lobbyId)abandon(lobbyId,true);return;}
  if(r!=EOS_EResult::EOS_Success||!lobbyId){fail("could not host: "+plain(r));return;}
  m_lobbyId=lobbyId;m_publishedPlayers=1;
  const std::uint64_t seed=std::uint64_t(Clock::now().time_since_epoch().count())^std::hash<std::string>{}(m_lobbyId);
  const std::string code=makeJoinCode(seed);
  EOS_HLobbyModification mod=nullptr;
  if(!modification(mod)){abandon(m_lobbyId,true);dropLobby();fail("could not publish the game");return;}
  addText(mod,"mode",modeKey(m_hostOptions.mode));addText(mod,"map",m_hostOptions.map);addText(mod,"maphash",m_hostOptions.mapHash);
  addText(mod,"name",m_hostOptions.name);addText(mod,"joincode",code);addInt(mod,"private",m_hostOptions.privateGame?1:0);
  addInt(mod,"players",1);addText(mod,"version",RAWMETAL_EOS_PRODUCT_VERSION);
  EOS_Lobby_UpdateLobbyOptions u{};u.ApiVersion=EOS_LOBBY_UPDATELOBBY_API_LATEST;u.LobbyModificationHandle=mod;
  EOS_Lobby_UpdateLobby(m_lobbyApi,&u,serialData(m_serial),[](const EOS_Lobby_UpdateLobbyCallbackInfo* i){if(g_live)g_live->onPublished(serialOf(i->ClientData),i->ResultCode);});
  EOS_LobbyModification_Release(mod);
 }
 void onPublished(std::uint64_t serial,EOS_EResult r){
  if(!(current(serial,Op::Host)||current(serial,Op::QuickMatch)))return;   // leave() already let the lobby go
  if(r!=EOS_EResult::EOS_Success){abandon(m_lobbyId,true);dropLobby();fail("could not publish the game: "+plain(r));return;}
  refreshLobby();m_op=Op::None;m_state=SessionState::InLobby;
  g_log.line("hosting "+std::string(modeName(m_lobby.mode))+" '"+m_lobby.name+"' on "+m_lobby.map+(m_lobby.privateGame?" (private)":"")+", code "+m_lobby.joinCode);
  m_events.push_back({SessionEvent::Hosted,m_selfId,{}});
 }
 void startSearch(){
  EOS_Lobby_CreateLobbySearchOptions o{};o.ApiVersion=EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
  const bool byCode=m_op==Op::JoinCode||m_op==Op::LookupCode;
  o.MaxResults=std::uint32_t(byCode?4:std::clamp(m_filter.maxResults,1,100));
  EOS_HLobbySearch search=nullptr;const EOS_EResult r=EOS_Lobby_CreateLobbySearch(m_lobbyApi,&o,&search);
  if(r!=EOS_EResult::EOS_Success||!search){fail("could not search: "+plain(r));return;}
  auto param=[&](const char* key,const std::string* text,std::int64_t number,EOS_EComparisonOp op){
   EOS_Lobby_AttributeData a{};a.ApiVersion=EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;a.Key=key;
   if(text){a.Value.AsUtf8=text->c_str();a.ValueType=EOS_ELobbyAttributeType::EOS_AT_STRING;}else{a.Value.AsInt64=number;a.ValueType=EOS_ELobbyAttributeType::EOS_AT_INT64;}
   EOS_LobbySearch_SetParameterOptions p{};p.ApiVersion=EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;p.Parameter=&a;p.ComparisonOp=op;
   const EOS_EResult pr=EOS_LobbySearch_SetParameter(search,&p);if(pr!=EOS_EResult::EOS_Success)g_log.line(std::string("search parameter ")+key+": "+resultName(pr));
  };
  const std::string version=RAWMETAL_EOS_PRODUCT_VERSION,bucket=modeKey(m_filter.mode);
  if(byCode)param("joincode",&m_code,0,EOS_EComparisonOp::EOS_CO_EQUAL);
  else{
   param("private",nullptr,0,EOS_EComparisonOp::EOS_CO_EQUAL);
   param("version",&version,0,EOS_EComparisonOp::EOS_CO_EQUAL);
   if(!m_filter.anyMode)param(EOS_LOBBY_SEARCH_BUCKET_ID,&bucket,0,EOS_EComparisonOp::EOS_CO_EQUAL);
   if(!m_filter.map.empty())param("map",&m_filter.map,0,EOS_EComparisonOp::EOS_CO_EQUAL);
   if(m_filter.hideFull)param(EOS_LOBBY_SEARCH_MINSLOTSAVAILABLE,nullptr,1,EOS_EComparisonOp::EOS_CO_GREATERTHANOREQUAL);
  }
  releaseSearch();m_searchHandle=search;   // an older, abandoned search lets go here
  EOS_LobbySearch_FindOptions f{};f.ApiVersion=EOS_LOBBYSEARCH_FIND_API_LATEST;f.LocalUserId=m_user;
  EOS_LobbySearch_Find(search,&f,serialData(m_serial),[](const EOS_LobbySearch_FindCallbackInfo* i){if(g_live)g_live->onFound(serialOf(i->ClientData),i->ResultCode);});
 }
 EOS_HLobbySearch m_searchHandle=nullptr;
 void releaseSearch(){if(m_searchHandle){EOS_LobbySearch_Release(m_searchHandle);m_searchHandle=nullptr;}}
 void onFound(std::uint64_t serial,EOS_EResult r){
  if(serial!=m_serial||!(m_op==Op::Search||m_op==Op::JoinCode||m_op==Op::QuickMatch||m_op==Op::LookupCode)){return;}
  EOS_HLobbySearch search=m_searchHandle;m_searchHandle=nullptr;
  struct Row{LobbyInfo info;std::string version;EOS_HLobbyDetails details;};std::vector<Row> rows;
  if(r==EOS_EResult::EOS_Success&&search){
   EOS_LobbySearch_GetSearchResultCountOptions co{};co.ApiVersion=EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
   const std::uint32_t count=EOS_LobbySearch_GetSearchResultCount(search,&co);
   for(std::uint32_t i=0;i<count;++i){
    EOS_LobbySearch_CopySearchResultByIndexOptions ci{};ci.ApiVersion=EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;ci.LobbyIndex=i;EOS_HLobbyDetails d=nullptr;
    if(EOS_LobbySearch_CopySearchResultByIndex(search,&ci,&d)!=EOS_EResult::EOS_Success||!d)continue;
    Row row;row.info=readDetails(d,nullptr,&row.version);row.details=d;rows.push_back(std::move(row));
   }
  }
  if(search)EOS_LobbySearch_Release(search);
  auto releaseRows=[&](EOS_HLobbyDetails keep){for(auto& row:rows)if(row.details!=keep)EOS_LobbyDetails_Release(row.details);};
  if(r!=EOS_EResult::EOS_Success&&r!=EOS_EResult::EOS_NotFound){releaseRows(nullptr);fail("could not search: "+plain(r));return;}
  const Op op=m_op;
  if(op==Op::Search||op==Op::LookupCode){
   m_results.clear();
   for(const auto& row:rows){
    if(op==Op::Search){
     if(row.info.privateGame||(!m_filter.anyMode&&row.info.mode!=m_filter.mode)||(!m_filter.map.empty()&&row.info.map!=m_filter.map))continue;
     if(m_filter.hideFull&&row.info.players>=row.info.maxPlayers)continue;
    }
    LobbyInfo info=row.info;if(op==Op::Search)info.joinCode.clear();m_results.push_back(info);
   }
   releaseRows(nullptr);m_op=Op::None;settle();
   g_log.line("search found "+std::to_string(m_results.size())+" game(s)");
   m_events.push_back({SessionEvent::SearchDone,{},{}});return;
  }
  if(op==Op::JoinCode){
   const Row* hit=nullptr;for(const auto& row:rows)if(row.info.joinCode==m_code){hit=&row;break;}
   if(!hit){releaseRows(nullptr);fail("no game has the code "+m_code);return;}
   if(hit->version!=RAWMETAL_EOS_PRODUCT_VERSION){const std::string v=hit->version;releaseRows(nullptr);fail("that game runs DepthWorks "+v+"; this copy is " RAWMETAL_EOS_PRODUCT_VERSION);return;}
   EOS_HLobbyDetails d=hit->details;releaseRows(d);leaveLobby();joinDetails(d);EOS_LobbyDetails_Release(d);return;
  }
  // Quick match: the fullest open game of the mode, else host one.
  const Row* best=nullptr;
  for(const auto& row:rows)if(!row.info.privateGame&&row.info.mode==m_filter.mode&&row.info.players<row.info.maxPlayers&&row.info.id!=m_lobbyId&&(!best||row.info.players>best->info.players))best=&row;
  if(best){EOS_HLobbyDetails d=best->details;releaseRows(d);leaveLobby();joinDetails(d);EOS_LobbyDetails_Release(d);return;}
  releaseRows(nullptr);leaveLobby();
  HostOptions o;o.mode=m_filter.mode;startHost(o);
 }
 void joinDetails(EOS_HLobbyDetails d){
  EOS_Lobby_JoinLobbyOptions o{};o.ApiVersion=EOS_LOBBY_JOINLOBBY_API_LATEST;o.LobbyDetailsHandle=d;o.LocalUserId=m_user;o.bPresenceEnabled=EOS_FALSE;
  o.RTCRoomJoinActionType=EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin;
  EOS_Lobby_JoinLobby(m_lobbyApi,&o,serialData(m_serial),[](const EOS_Lobby_JoinLobbyCallbackInfo* i){if(g_live)g_live->onJoined(serialOf(i->ClientData),i->ResultCode,i->LobbyId);});
 }
 void onJoined(std::uint64_t serial,EOS_EResult r,const char* lobbyId){
  const bool wanted=serial==m_serial&&(m_op==Op::Join||m_op==Op::JoinCode||m_op==Op::QuickMatch);
  if(!wanted){if(r==EOS_EResult::EOS_Success&&lobbyId)abandon(lobbyId,false);return;}
  if(r!=EOS_EResult::EOS_Success||!lobbyId){fail(r==EOS_EResult::EOS_Lobby_TooManyPlayers?std::string("that game is full"):"could not join: "+plain(r));return;}
  m_lobbyId=lobbyId;m_publishedPlayers=-1;refreshLobby();m_op=Op::None;m_state=SessionState::InLobby;
  g_log.line("joined '"+m_lobby.name+"' ("+std::to_string(m_members.size())+" members)");
  m_events.push_back({SessionEvent::Joined,m_selfId,{}});
 }
 // Let go of a lobby this session no longer wants (a late answer to a cancelled request, or leaving).
 void abandon(const std::string& lobbyId,bool owner){
  if(owner){
   EOS_Lobby_DestroyLobbyOptions o{};o.ApiVersion=EOS_LOBBY_DESTROYLOBBY_API_LATEST;o.LocalUserId=m_user;o.LobbyId=lobbyId.c_str();
   EOS_Lobby_DestroyLobby(m_lobbyApi,&o,nullptr,[](const EOS_Lobby_DestroyLobbyCallbackInfo* i){if(g_live){g_live->m_leaving=false;g_log.line("closed the lobby: "+resultName(i->ResultCode));}});
  }else{
   EOS_Lobby_LeaveLobbyOptions o{};o.ApiVersion=EOS_LOBBY_LEAVELOBBY_API_LATEST;o.LocalUserId=m_user;o.LobbyId=lobbyId.c_str();
   EOS_Lobby_LeaveLobby(m_lobbyApi,&o,nullptr,[](const EOS_Lobby_LeaveLobbyCallbackInfo* i){if(g_live){g_live->m_leaving=false;g_log.line("left the lobby: "+resultName(i->ResultCode));}});
  }
  m_leaving=true;
 }
 void leaveLobby(){
  if(m_lobbyId.empty())return;
  // A host alone closes the lobby; with others in it, leaving passes it on (EOS promotes a member).
  const bool alone=m_members.size()<=1;
  abandon(m_lobbyId,isHost()&&alone);
  g_log.line(std::string("leaving the lobby")+(isHost()&&!alone?" (handing it on)":""));
  dropLobby();
 }
 void dropLobby(){
  if(m_p2pApi&&m_user){EOS_P2P_CloseConnectionsOptions o{};o.ApiVersion=EOS_P2P_CLOSECONNECTIONS_API_LATEST;o.LocalUserId=m_user;o.SocketId=&m_socket;EOS_P2P_CloseConnections(m_p2pApi,&o);}
  m_lobbyId.clear();m_lobby=LobbyInfo{};m_members.clear();m_inbox.clear();m_reassembler.clear();m_publishedPlayers=-1;
 }
 void onMemberStatus(const char* lobbyId,EOS_ProductUserId target,EOS_ELobbyMemberStatus status){
  if(!lobbyId||m_lobbyId!=lobbyId)return;
  const PeerId who=idText(target);
  switch(status){
   case EOS_ELobbyMemberStatus::EOS_LMS_JOINED:
    refreshLobby();if(who!=m_selfId){accept(who);m_events.push_back({SessionEvent::MemberJoined,who,{}});g_log.line("member joined: "+who);}
    break;
   case EOS_ELobbyMemberStatus::EOS_LMS_LEFT:case EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED:case EOS_ELobbyMemberStatus::EOS_LMS_KICKED:
    if(who==m_selfId){lobbyClosed("removed from the game");break;}
    refreshLobby();m_reassembler.forget(who);
    m_members.erase(std::remove(m_members.begin(),m_members.end(),who),m_members.end());
    m_events.push_back({SessionEvent::MemberLeft,who,{}});g_log.line("member left: "+who);
    break;
   case EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED:
    refreshLobby();m_lobby.owner=who;m_events.push_back({SessionEvent::HostChanged,who,{}});g_log.line("new host: "+who);
    if(who==m_selfId){m_publishedPlayers=-1;setLobbyPlayers(int(m_members.size()));}
    break;
   case EOS_ELobbyMemberStatus::EOS_LMS_CLOSED:lobbyClosed("the host closed the game");break;
   default:break;
  }
 }
 void lobbyClosed(const std::string& why){
  dropLobby();if(m_op==Op::None)m_state=SessionState::Ready;
  m_events.push_back({SessionEvent::LobbyClosed,{},why});g_log.line("lobby closed: "+why);
 }

 // ----- Packets -----
 void onConnectionRequest(EOS_ProductUserId remote){
  const PeerId who=idText(remote);
  if(!m_lobbyId.empty()&&std::find(m_members.begin(),m_members.end(),who)!=m_members.end())accept(who);
  else g_log.line("ignored a connection from "+who+" (not in this game)");
 }
 void pumpPackets(){
  if(!m_user)return;
  std::uint8_t buffer[EosPacketMax];
  for(int i=0;i<4096;++i){
   EOS_P2P_GetNextReceivedPacketSizeOptions so{};so.ApiVersion=EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;so.LocalUserId=m_user;so.RequestedChannel=nullptr;
   std::uint32_t size=0;if(EOS_P2P_GetNextReceivedPacketSize(m_p2pApi,&so,&size)!=EOS_EResult::EOS_Success)break;
   EOS_P2P_ReceivePacketOptions ro{};ro.ApiVersion=EOS_P2P_RECEIVEPACKET_API_LATEST;ro.LocalUserId=m_user;ro.MaxDataSizeBytes=sizeof buffer;ro.RequestedChannel=nullptr;
   EOS_ProductUserId peer=nullptr;EOS_P2P_SocketId socket{};std::uint8_t channel=0;std::uint32_t written=0;
   if(EOS_P2P_ReceivePacket(m_p2pApi,&ro,&peer,&socket,&channel,buffer,&written)!=EOS_EResult::EOS_Success)break;
   if(std::strncmp(socket.SocketName,m_socket.SocketName,EOS_P2P_SOCKETID_SOCKETNAME_SIZE)!=0||m_lobbyId.empty())continue;
   const PeerId from=idText(peer);
   if(std::find(m_members.begin(),m_members.end(),from)==m_members.end())continue;
   std::vector<std::uint8_t> message;
   if(m_reassembler.accept(from,channel,buffer,written,message))m_inbox.push_back(Packet{from,channel,std::move(message)});
  }
 }

 // ----- Shut-down -----
 void shutdown(){
  if(m_loader.joinable())m_loader.join();
  if(m_platform){
   if(!m_lobbyId.empty()&&m_user){
    leaveLobby();
    // Let the leave reach Epic so the lobby passes on now rather than when the others time out.
    const auto until=Clock::now()+std::chrono::milliseconds(400);
    while(m_leaving&&Clock::now()<until){EOS_Platform_Tick(m_platform);Sleep(10);}
   }
   releaseSearch();
   if(m_connectionRequest!=EOS_INVALID_NOTIFICATIONID)EOS_P2P_RemoveNotifyPeerConnectionRequest(m_p2pApi,m_connectionRequest);
   if(m_memberStatus!=EOS_INVALID_NOTIFICATIONID)EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(m_lobbyApi,m_memberStatus);
   if(m_lobbyUpdate!=EOS_INVALID_NOTIFICATIONID)EOS_Lobby_RemoveNotifyLobbyUpdateReceived(m_lobbyApi,m_lobbyUpdate);
   if(m_authExpiry!=EOS_INVALID_NOTIFICATIONID)EOS_Connect_RemoveNotifyAuthExpiration(m_connectApi,m_authExpiry);
   if(m_loginStatus!=EOS_INVALID_NOTIFICATIONID)EOS_Connect_RemoveNotifyLoginStatusChanged(m_connectApi,m_loginStatus);
   EOS_Platform_Release(m_platform);m_platform=nullptr;
  }
  if(g_eosInitialized){EOS_Shutdown();g_eosInitialized=false;g_eosShutDown=true;g_log.line("shut down");}
 }
};

bool waitFor(EosSession& s,std::vector<SessionEvent>& seen,double seconds,const std::function<bool()>& done){
 const auto until=Clock::now()+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
 while(Clock::now()<until){s.tick();SessionEvent e;while(s.pollEvent(e))seen.push_back(e);if(done())return true;Sleep(20);}
 return done();
}
bool saw(const std::vector<SessionEvent>& seen,SessionEvent::Kind kind){for(const auto& e:seen)if(e.kind==kind)return true;return false;}
std::string lastError(const std::vector<SessionEvent>& seen){for(auto it=seen.rbegin();it!=seen.rend();++it)if(it->kind==SessionEvent::Error)return it->text;return {};}

} // namespace

std::unique_ptr<Session> makeEosSession(){
 if(g_live)return std::make_unique<OfflineSession>("another multiplayer session is already running");
 return std::make_unique<EosSession>();
}

bool testEos(std::ostream& report){
 bool ok=testWire(report);auto check=[&](bool c,const std::string& what){report<<(c?"PASS ":"FAIL ")<<what<<'\n';report.flush();ok&=c;return c;};
 if(g_live){check(false,"no other EOS session is running in this process");return false;}
 if(g_eosShutDown){check(false,"EOS has not been shut down earlier in this process (it cannot restart)");return false;}
 std::vector<SessionEvent> seen;
 {
  const auto t0=Clock::now();EosSession s;
  const double constructMs=std::chrono::duration<double,std::milli>(Clock::now()-t0).count();
  check(constructMs<5&&s.state()==SessionState::SigningIn,"making the session is instant ("+std::to_string(int(constructMs*1000))+" us) and touches nothing until its first tick");
  const auto t1=Clock::now();s.tick();
  const double firstTickMs=std::chrono::duration<double,std::milli>(Clock::now()-t1).count();
  check(firstTickMs<20,"the first tick only starts the unpack on a worker ("+std::to_string(int(firstTickMs))+" ms)");
  waitFor(s,seen,60,[&]{return s.state()!=SessionState::SigningIn;});
  if(!check(s.state()==SessionState::Ready&&!s.self().empty(),"signs in with this PC's device id as '"+s.displayName()+"'"+(s.state()==SessionState::Ready?"":": "+s.reason())))return false;
  report<<"     product user "<<s.self()<<'\n';

  char tag[16];std::snprintf(tag,sizeof tag,"%08lx",static_cast<unsigned long>(GetTickCount64()^GetCurrentProcessId()*2654435761u));
  const std::string map=std::string("eos-test-")+tag+".rmc";
  HostOptions o;o.mode=Mode::Deathmatch;o.name="eos-test "+std::string(tag);o.map=map;o.mapHash=tag;o.maxPlayers=4;o.privateGame=false;
  seen.clear();s.host(o);
  waitFor(s,seen,30,[&]{return saw(seen,SessionEvent::Hosted)||saw(seen,SessionEvent::Error);});
  const LobbyInfo hosted=s.lobby();
  if(!check(s.state()==SessionState::InLobby&&s.isHost()&&saw(seen,SessionEvent::Hosted)&&normaliseJoinCode(hosted.joinCode)==hosted.joinCode&&!hosted.joinCode.empty()&&hosted.map==map,
            "hosts a public deathmatch lobby with a join code"+(hosted.joinCode.empty()?": "+lastError(seen):" ("+hosted.joinCode+")")))return false;
  check(s.members().size()==1&&s.members()[0]==s.self()&&s.hostId()==s.self(),"the host is its lobby's only member and its host");

  SearchFilter f;f.anyMode=false;f.mode=Mode::Deathmatch;f.map=map;f.hideFull=true;
  const LobbyInfo* found=nullptr;
  for(int attempt=0;attempt<6&&!found;++attempt){
   if(attempt)waitFor(s,seen,2,[]{return false;});
   seen.clear();s.search(f);waitFor(s,seen,20,[&]{return saw(seen,SessionEvent::SearchDone)||saw(seen,SessionEvent::Error);});
   for(const auto& r:s.results())if(r.id==hosted.id)found=&r;
  }
  check(found&&found->joinCode.empty()&&found->map==map&&found->name==o.name&&found->mode==Mode::Deathmatch&&found->maxPlayers==4&&found->owner==s.self(),
        "the browser's search finds it (map, name, mode, size, host), without its code"+(found?std::string():": "+lastError(seen)));
  {SearchFilter coop=f;coop.mode=Mode::Coop;seen.clear();s.search(coop);waitFor(s,seen,20,[&]{return saw(seen,SessionEvent::SearchDone)||saw(seen,SessionEvent::Error);});
   bool listed=false;for(const auto& r:s.results())listed|=r.id==hosted.id;check(saw(seen,SessionEvent::SearchDone)&&!listed,"a co-op search does not list the deathmatch game");}

  bool resolved=false;
  for(int attempt=0;attempt<4&&!resolved;++attempt){
   if(attempt)waitFor(s,seen,2,[]{return false;});
   seen.clear();s.lookupCode(hosted.joinCode);waitFor(s,seen,20,[&]{return saw(seen,SessionEvent::SearchDone)||saw(seen,SessionEvent::Error);});
   for(const auto& r:s.results())resolved|=r.id==hosted.id&&r.joinCode==hosted.joinCode;
  }
  std::string lower=hosted.joinCode;for(char& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));
  check(resolved,"its join code ("+hosted.joinCode+") resolves to it");
  {seen.clear();s.joinCode(lower);waitFor(s,seen,2,[]{return false;});
   check(s.state()==SessionState::InLobby&&s.lobby().id==hosted.id&&!saw(seen,SessionEvent::Error),"typing its own code in lower case keeps the host where it is");}

  s.setLobbyPlayers(3);
  waitFor(s,seen,15,[&]{return s.playersUpdates()>0;});
  int players=-1;
  for(int attempt=0;attempt<6&&players!=3;++attempt){
   if(attempt)waitFor(s,seen,2,[]{return false;});
   seen.clear();s.search(f);waitFor(s,seen,20,[&]{return saw(seen,SessionEvent::SearchDone)||saw(seen,SessionEvent::Error);});
   for(const auto& r:s.results())if(r.id==hosted.id)players=r.players;
  }
  check(players==3&&s.lobby().players==3,"setLobbyPlayers(3) reaches the browser (it reads "+std::to_string(players)+")");

  const std::uint8_t hello[]={1,2,3};
  check(!s.send(s.self(),Control,hello,3,true),"a packet to oneself is refused");
  {std::vector<std::uint8_t> big(EosUnreliableMax+1,7);check(!s.send("not-a-member",State,big.data(),big.size(),false),"a packet to a stranger is refused");}

  s.leave();
  check(s.state()==SessionState::Ready&&s.lobby().id.empty()&&s.members().empty()&&!s.isHost(),"leaving returns the session to Ready");
  bool gone=false;
  for(int attempt=0;attempt<8&&!gone;++attempt){
   waitFor(s,seen,attempt?2:1,[]{return false;});
   seen.clear();s.search(f);waitFor(s,seen,20,[&]{return saw(seen,SessionEvent::SearchDone)||saw(seen,SessionEvent::Error);});
   gone=saw(seen,SessionEvent::SearchDone);for(const auto& r:s.results())gone&=r.id!=hosted.id;
  }
  check(gone,"after leaving, the lobby is gone from the browser");

  // A private game: never in the browser, yet its code still finds it.
  HostOptions hidden=o;hidden.privateGame=true;hidden.mode=Mode::Coop;hidden.name="eos-test private "+std::string(tag);
  seen.clear();s.host(hidden);waitFor(s,seen,30,[&]{return saw(seen,SessionEvent::Hosted)||saw(seen,SessionEvent::Error);});
  const LobbyInfo secret=s.lobby();
  if(check(s.state()==SessionState::InLobby&&secret.privateGame&&secret.mode==Mode::Coop&&!secret.joinCode.empty(),"hosts a private co-op game"+(secret.joinCode.empty()?": "+lastError(seen):""))){
   bool coded=false;
   for(int attempt=0;attempt<4&&!coded;++attempt){
    if(attempt)waitFor(s,seen,2,[]{return false;});
    seen.clear();s.lookupCode(secret.joinCode);waitFor(s,seen,20,[&]{return saw(seen,SessionEvent::SearchDone)||saw(seen,SessionEvent::Error);});
    for(const auto& r:s.results())coded|=r.id==secret.id;
   }
   SearchFilter any;any.map=map;seen.clear();s.search(any);waitFor(s,seen,20,[&]{return saw(seen,SessionEvent::SearchDone)||saw(seen,SessionEvent::Error);});
   bool listed=false;for(const auto& r:s.results())listed|=r.id==secret.id;
   check(coded&&saw(seen,SessionEvent::SearchDone)&&!listed,"the browser never lists the private game; its code finds it");
   s.leave();check(s.state()==SessionState::Ready,"leaving the private game");
  }
 }   // the session shuts EOS down here
 check(g_eosShutDown&&!g_live,"the session shuts EOS down cleanly");
 return ok;
}
}
#else
namespace retro::net {
std::unique_ptr<Session> makeEosSession(){return std::make_unique<OfflineSession>("this copy was built without multiplayer keys");}
bool testEos(std::ostream& report){
 const bool wire=testWire(report);
 report<<"FAIL sign in: this copy was built without multiplayer keys (src/ThirdParty/EOS/eos_config.json)\n";
 (void)wire;return false;
}
}
#endif

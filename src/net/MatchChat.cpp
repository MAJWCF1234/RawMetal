// Chat and the voice upkeep of a match (docs/Systems/multiplayer.md "Chat and voice").
//  - A line goes to the host on the reliable Events channel (ChatSay); the host cleans it, holds each sender to a
//    small allowance (ChatBurst lines, then one every ChatRefill seconds) and relays it to everyone, the sender too
//    (ChatRelay), so every machine shows the same lines in the same order.
//  - Notices (joined, left, a kill) are made on each machine from the roster and the kill feed: no traffic.
//  - What the chat box shows follows this machine's settings: text chat off hides players' lines (notices stay),
//    the word filter masks listed words with asterisks, a muted player's lines are dropped.
//  - Voice: muted players are muted in the session; the scoreboard and name tags carry who is speaking; in co-op
//    with proximity voice on, each player's volume follows the distance between the two and the walls between.
#include "Match.h"
#include "MatchProtocol.h"
#include "Wire.h"
#include "../game/Game.h"
#include <algorithm>
#include <cctype>
#include <cmath>
namespace retro::net {
using namespace protocol;

std::string Match::cleanChat(const std::string& text){
 std::string out;out.reserve(std::min(text.size(),ChatMax));
 for(char c:text){const unsigned char u=static_cast<unsigned char>(c);if(u<32||u>126)continue;if(c==' '&&(out.empty()||out.back()==' '))continue;out+=c;}
 while(!out.empty()&&out.back()==' ')out.pop_back();
 if(out.size()>ChatMax){out.resize(ChatMax);while(!out.empty()&&out.back()==' ')out.pop_back();}
 return out;
}

// A small list, matched on whole words: the exact words, and the stems that mask any word they begin.
std::string Match::maskChat(const std::string& text){
 static const char* exact[]={"ass","arse","asshole","arsehole","bastard","bastards","bitch","bitches","bollocks","cock","cocks","cunt","cunts","dick","dicks","dickhead",
  "fag","fags","faggot","fuck","fucks","fucker","fucking","motherfucker","nigger","nigga","piss","prick","pussy","retard","retarded","shit","shits","shitty","slut","sluts","twat","wanker","whore"};
 static const char* stems[]={"fuck","shit","cunt","nigg","fagg","bitch","whore","wank","motherf"};
 std::string out=text;
 for(std::size_t i=0;i<out.size();){
  if(!std::isalpha(static_cast<unsigned char>(out[i]))){++i;continue;}
  std::size_t j=i;std::string word;
  while(j<out.size()&&std::isalpha(static_cast<unsigned char>(out[j]))){word+=char(std::tolower(static_cast<unsigned char>(out[j])));++j;}
  bool bad=false;
  for(const char* w:exact)bad|=word==w;
  for(const char* s:stems)bad|=word.rfind(s,0)==0;
  if(bad)for(std::size_t k=i;k<j;++k)out[k]='*';
  i=j;
 }
 return out;
}

bool Match::takeChatToken(const PeerId& peer){
 auto [it,fresh]=m_chatBuckets.try_emplace(peer,ChatBurst,m_clock);
 auto& [tokens,stamp]=it->second;
 tokens=std::min(ChatBurst,tokens+(m_clock-stamp)/ChatRefill);stamp=m_clock;
 if(tokens<1)return false;
 tokens-=1;return true;
}

void Match::pushChat(const std::string& from,const std::string& text,bool notice,bool self){
 m_chat.push_back({from,text,notice,self,0});
 while(m_chat.size()>40)m_chat.pop_front();
}

// The host: one line from a slot to everyone, and into its own box.
void Match::relayChat(int slot,const std::string& text){
 if(slot<0||slot>=MaxSlots)return;
 const std::string& name=m_slots[size_t(slot)].name;
 wire::Writer w;w.u8(ChatRelay).u8(unsigned(slot)).str(name).str(text);
 broadcast(Events,w,true);
 pushChat(name,text,false,slot==m_selfSlot);
}

bool Match::sendChat(const std::string& raw){
 if(m_finished||m_phase!=Phase::Playing||m_selfSlot<0)return false;
 const std::string text=cleanChat(raw);if(text.empty())return false;
 if(!takeChatToken(m_self)){pushChat({},"Too fast. Wait a moment before the next line.",true,true);return false;}
 if(m_host){relayChat(m_selfSlot,text);return true;}
 wire::Writer w;w.u8(ChatSay).str(text);send(m_hostId,Events,w,true);
 return true;
}

void Match::handleChat(const Packet& p){
 wire::Reader r(p.data);const unsigned type=r.u8();
 if(type==ChatSay){
  if(!m_host)return;
  const int slot=slotOf(p.from);if(slot<0)return;
  const std::string text=cleanChat(r.str());if(r.bad||text.empty())return;
  if(!takeChatToken(p.from))return;   // over the allowance: dropped
  relayChat(slot,text);
 }else if(type==ChatRelay){
  if(m_host||p.from!=m_hostId)return;
  const int slot=int(r.u8());const std::string name=r.str(),text=cleanChat(r.str());
  if(r.bad||text.empty())return;
  pushChat(name,text,false,slot==m_selfSlot);
 }
}

// Before publish: kills pushed this frame (their age is still exactly 0), and who came and went since the last frame.
void Match::chatNotices(){
 for(auto& f:m_feed)if(f.age==0.f){
  pushChat({},f.killer.empty()?"The dark took "+f.victim:f.killer+" killed "+f.victim,true,false);
  f.age=1e-6f;   // told once
 }
 const bool selfIn=m_selfSlot>=0&&m_slots[size_t(m_selfSlot)].used;
 if(!m_chatPrimed){
  if(!selfIn)return;   // a guest learns who is here from its first roster: those already down here are no news
  for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];m_chatKnown[size_t(i)]=s.used?s.peer:PeerId{};m_chatKnownName[size_t(i)]=s.name;}
  m_chatPrimed=true;return;
 }
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];const PeerId now=s.used?s.peer:PeerId{};
  if(now!=m_chatKnown[size_t(i)]){
   if(!m_chatKnown[size_t(i)].empty())pushChat({},m_chatKnownName[size_t(i)]+" left the game",true,false);
   if(!now.empty()&&i!=m_selfSlot)pushChat({},s.name+" joined the game",true,false);
   m_chatKnown[size_t(i)]=now;}
  if(!now.empty())m_chatKnownName[size_t(i)]=s.name;
 }
}

// After publish: the chat box as this machine shows it, who is speaking, and every few frames the session's mutes
// and (co-op, proximity voice) each voice's volume.
void Match::publishComms(float dt){
 Game& g=m_game;auto& v=g.m_matchView;const Settings& set=g.m_settings;
 for(auto& c:m_chat)c.age+=dt;
 v.chat.clear();
 for(const auto& c:m_chat){
  if(!c.notice&&(!set.textChat||(!c.self&&set.muted(c.from))))continue;
  v.chat.push_back({c.from,!c.notice&&set.chatFilter?maskChat(c.text):c.text,c.notice,c.self,c.age});
 }
 if(v.chat.size()>12)v.chat.erase(v.chat.begin(),v.chat.end()-12);
 if(!m_session)return;
 const bool transmitting=m_session->voiceStatus().transmitting;
 for(auto& row:v.scores){const auto& s=m_slots[size_t(row.slot)];
  row.muted=!row.self&&set.muted(row.name);
  row.speaking=row.self?transmitting:!row.muted&&!s.peer.empty()&&m_session->peerSpeaking(s.peer);}
 for(auto& rp:g.m_remotePlayers){const auto& s=m_slots[size_t(rp.slot)];rp.speaking=!set.muted(rp.name)&&!s.peer.empty()&&m_session->peerSpeaking(s.peer);}
 if((m_voiceTimer-=dt)>0)return;
 m_voiceTimer=.2f;
 const bool proximity=m_mode==Mode::Coop&&set.proximityVoice;
 const Point3 me{g.m_player.pos.x,g.m_player.pos.y,g.m_player.z+1.5f};
 auto inside=[](float x,float y){return x>=0&&y>=0&&x<24&&y<24;};
 std::map<PeerId,bool> seen;
 for(int i=0;i<MaxSlots;++i){const auto& s=m_slots[size_t(i)];if(!s.used||i==m_selfSlot||s.peer.empty())continue;seen[s.peer]=true;
  const bool muted=set.muted(s.name);
  if(auto it=m_peerMuted.find(s.peer);it==m_peerMuted.end()||it->second!=muted){m_session->setPeerMuted(s.peer,muted);m_peerMuted[s.peer]=muted;}
  float volume=1;
  if(proximity){
   const RemotePlayer* rp=nullptr;for(const auto& r:g.m_remotePlayers)if(r.slot==i)rp=&r;
   if(!rp)volume=0;   // not in the world near here (another map, or not spawned): out of earshot
   else{
    const Point3 them{rp->position.x,rp->position.y,rp->position.z+1.5f};
    const float d=std::sqrt((them.x-me.x)*(them.x-me.x)+(them.y-me.y)*(them.y-me.y)+(them.z-me.z)*(them.z-me.z));
    // Full within 3 m, gone by 30 m; a wall between keeps a third of it, muffled by distance.
    volume=std::pow(std::clamp(1.f-(d-3.f)/27.f,0.f,1.f),1.5f);
    if(volume>0&&(!inside(me.x,me.y)||!inside(them.x,them.y)||!g.lineOfSight({me.x,me.y},{them.x,them.y})))volume*=.35f;
   }
  }
  if(auto it=m_peerVolume.find(s.peer);it==m_peerVolume.end()||std::fabs(it->second-volume)>.02f||(volume==0)!=(it->second==0)){m_session->setPeerVolume(s.peer,volume);m_peerVolume[s.peer]=volume;}
 }
 for(auto it=m_peerVolume.begin();it!=m_peerVolume.end();)if(!seen.count(it->first))it=m_peerVolume.erase(it);else ++it;
 for(auto it=m_peerMuted.begin();it!=m_peerMuted.end();)if(!seen.count(it->first))it=m_peerMuted.erase(it);else ++it;
}

}

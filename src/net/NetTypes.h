#pragma once
// Multiplayer's shared contract (docs/Systems/multiplayer.md). Three layers meet here:
//  - Session: who is playing together. Signing in, lobbies (host, browse, join code, quick match) and the
//    peer-to-peer packets between members. EosSession (Epic Online Services) is the real one; LoopbackSession
//    (Loopback.h) is an in-memory one for tests, several sessions in one process on one LoopbackHub.
//  - Match (Match.h): the game over a Session. The host's game is the truth; guests send what they do and
//    receive what is. Deathmatch and co-op rules, the map handed to a guest who lacks it.
//  - Presentation: the multiplayer menu, the scoreboard and kill feed, the other players drawn as Claire.
//    It reads Game::remotePlayers() and Game::matchView(), plain data filled by the Match.
// Nothing here knows about EOS, and nothing below Session knows about the game.
#include "../core/Math.h"
#include "../game/Avatar.h"
#include <cstdint>
#include <string>
#include <vector>
namespace retro::net {

enum class Mode:std::uint8_t{Deathmatch=1,Coop=2};
inline const char* modeName(Mode m){return m==Mode::Coop?"CO-OP":"DEATHMATCH";}

// A member of a session: the backend's own user id (EOS: the ProductUserId as text). Stable for the session.
using PeerId=std::string;

// Channels keep kinds of traffic apart (EOS P2P channels 0-255).
enum Channel:std::uint8_t{Control=0,  // hello, welcome, map offer, match rules, goodbye (reliable)
                          State=1,    // snapshots: players, creatures, doors (unreliable, newest wins)
                          Events=2,   // shots, hits, deaths, pickups, chat (reliable)
                          MapData=3}; // the map file in chunks (reliable)

struct Packet{PeerId from;std::uint8_t channel=0;std::vector<std::uint8_t> data;};

// What a host offers, as the browser lists it.
struct HostOptions{
 Mode mode=Mode::Deathmatch;
 std::string name;            // shown in the browser ("Maddie's game")
 std::string map;             // the custom campaign's file name, e.g. "Sample Facility.rmc"
 std::string mapHash;         // hex of the file's content hash; a guest with another file is sent the host's
 int maxPlayers=8;            // 2-16
 bool privateGame=false;      // never listed; joined only by its code
};

struct LobbyInfo{
 std::string id;              // the backend's lobby id
 PeerId owner;                // the host
 std::string name,map,mapHash;
 std::string joinCode;        // e.g. "K7QM-2X"; empty in a search result for a private game (they are not listed)
 Mode mode=Mode::Deathmatch;
 int players=0,maxPlayers=0;
 bool privateGame=false;
 int pingMs=-1;               // -1 unknown
};

struct SearchFilter{
 bool anyMode=true;Mode mode=Mode::Deathmatch;
 std::string map;             // empty: any map
 bool hideFull=true;
 int maxResults=50;
};

enum class SessionState:std::uint8_t{
 Offline,      // no backend (no credentials baked in, or the SDK could not load); reason() says why
 SigningIn,
 Ready,        // signed in, in no lobby
 Busy,         // a host, search or join is under way
 InLobby,      // a member of a lobby: packets flow between members
 Failed        // the last request failed; reason() says why, state returns to Ready on the next request
};

// ----- Voice (EOS RTC on the lobby's room; docs/Systems/multiplayer.md "Chat and voice") -----
enum class VoiceMode:std::uint8_t{Off=0,PushToTalk=1,OpenMic=2};
inline const char* voiceModeName(VoiceMode m){return m==VoiceMode::OpenMic?"OPEN MIC":m==VoiceMode::PushToTalk?"PUSH TO TALK":"OFF";}
struct AudioDevice{std::string id,name;bool isDefault=false;};
// What the player chose (Settings' COMMUNICATION page), handed to the session whenever it changes.
struct VoiceSettings{
 VoiceMode mode=VoiceMode::PushToTalk;
 float openMicThreshold=.25f;  // 0-1 of the mic level meter: open mic sends while the level is above it
 std::string inputDevice,outputDevice;   // device names; "" the system's default
 float inputVolume=1,outputVolume=1;     // 0-2, 1 as it comes
 bool operator==(const VoiceSettings&)const=default;
};
struct VoiceStatus{
 bool available=false;        // the session can carry voice (signed in, the RTC side started)
 bool inRoom=false;           // in a lobby's voice room now
 bool transmitting=false;     // the local mic is being sent this moment (push-to-talk held or open mic above threshold)
 std::string reason;          // when not available, or a device failed: why, in plain words
};

struct SessionEvent{
 enum Kind:std::uint8_t{MemberJoined,MemberLeft,HostChanged,LobbyClosed,SearchDone,Joined,Hosted,Error} kind=Error;
 PeerId peer;                 // the member concerned (MemberJoined/Left, HostChanged: the new host)
 std::string text;            // Error: why
};

// One session: one signed-in player, at most one lobby at a time. Every call returns at once; results arrive
// as events and state changes during tick(). Not thread-safe: call it from the game's thread.
class Session{
public:
 virtual ~Session()=default;
 virtual void tick()=0;                                   // pump the backend; once a frame
 virtual SessionState state()const=0;
 virtual std::string reason()const=0;                     // why Offline or Failed, in plain words
 virtual PeerId self()const=0;                            // empty until signed in
 virtual std::string displayName()const=0;

 virtual void host(const HostOptions& options)=0;         // -> Hosted (then InLobby, isHost) or Error
 virtual void search(const SearchFilter& filter)=0;       // -> SearchDone; results() holds them
 virtual const std::vector<LobbyInfo>& results()const=0;
 virtual void join(const std::string& lobbyId)=0;         // a browser row -> Joined or Error
 virtual void joinCode(const std::string& code)=0;        // a typed code (case and the dash do not matter)
 virtual void quickMatch(Mode mode)=0;                    // the fullest open game of the mode, else hosts one
 virtual void leave()=0;                                  // -> Ready; a host leaving hands the lobby on

 virtual bool isHost()const=0;
 virtual PeerId hostId()const=0;
 virtual const LobbyInfo& lobby()const=0;                 // valid InLobby
 virtual std::vector<PeerId> members()const=0;            // InLobby, self included
 virtual void setLobbyPlayers(int count)=0;               // the host keeps the browser's count true

 // Packets between lobby members. reliable: ordered and resent until delivered.
 virtual bool send(const PeerId& to,std::uint8_t channel,const std::uint8_t* data,std::size_t size,bool reliable)=0;
 virtual bool receive(Packet& out)=0;                     // false when none waits
 virtual bool pollEvent(SessionEvent& out)=0;             // false when none waits

 // Voice. Everything degrades to silence: a session without voice (Offline, no RTC, no devices) answers with
 // VoiceStatus::available false and a reason; none of these calls fail loudly or block.
 virtual void setVoice(const VoiceSettings&){}             // applies live
 virtual void setTalking(bool){}                           // the push-to-talk key, held or not
 virtual VoiceStatus voiceStatus()const{return {false,false,false,"voice needs a multiplayer session"};}
 virtual std::vector<AudioDevice> inputDevices()const{return {};}
 virtual std::vector<AudioDevice> outputDevices()const{return {};}
 virtual float micLevel()const{return 0;}                  // 0-1, the local mic in a voice room
 virtual bool peerSpeaking(const PeerId&)const{return false;}
 virtual void setPeerMuted(const PeerId&,bool){}           // a muted member is not heard at all
 virtual void setPeerVolume(const PeerId&,float){}         // 0-1 on top of the voice volume (proximity in co-op)
};

// Join codes: 6 characters from an alphabet without look-alikes (no 0/O, 1/I/L), shown as "XXXX-XX".
std::string makeJoinCode(std::uint64_t seed);
std::string normaliseJoinCode(const std::string& typed);   // upper case, dash and spaces gone; "" if not a code

// ----- What the Match fills and the presentation reads (plain data, copied each frame) -----

// Another player as this machine draws them.
struct RemotePlayer{
 std::uint8_t slot=0;         // the host's player number, 0-15 (the host is 0)
 std::string name;
 Point3 position{};           // the feet, game space (chunk-relative, z up)
 float angle=0,pitch=0;       // facing (radians, the map angle) and look pitch (radians, up positive)
 Vec2 velocity{};             // m/s, for the legs' clip and phase
 bool crouched=false,prone=false,grounded=true;
 int weapon=0;                // WeaponId: 0 fists, 1 shotgun, 2 handgun, 3 the MAC-11 (two bits on the wire)
 float shotAge=99;            // seconds since their last shot (muzzle flash, recoil)
 float reloadAge=99;          // seconds since a reload began (99 none)
 float health=100;
 bool alive=true;
 float deathAge=0;            // seconds since they died (the death clip, then hidden until respawn)
 bool vr=false;AvatarPose pose{}; // a VR player: draw the avatar solve from pose instead of the clips
 int team=0;                  // 0 none (deathmatch); co-op players are all team 1
 int character=0;             // Settings::character of that player: 0 Claire, 1 the Limitator
 int pingMs=-1;
 bool speaking=false;         // their voice is heard now (the name tag's mark)
};

struct ScoreRow{std::uint8_t slot=0;std::string name;int frags=0,deaths=0,pingMs=-1;bool host=false,self=false;
 bool speaking=false,muted=false;};    // speaking: their voice is heard now; muted: this player muted them (by name)
struct FeedLine{std::string killer,victim;int weapon=0;float age=0;};  // killer empty: the world (a fall, a creature)

// A line in the chat box: a player's words, or a notice (joined, left, a kill). Filtered (masked) and muted
// players dropped as this machine's settings ask, newest last.
struct ChatLine{std::string from,text;bool notice=false,self=false;float age=0;};

struct MatchView{
 bool active=false;
 Mode mode=Mode::Deathmatch;
 bool host=false;
 std::string map,joinCode,lobbyName;
 int fragLimit=0;float timeLeft=-1;     // -1 no time limit
 std::vector<ScoreRow> scores;          // sorted: most frags first
 std::vector<FeedLine> feed;            // newest last, the last few seconds
 std::string status;                    // one line: "Waiting for the map (42%)", "Respawning in 3", "Host left"
 float respawnIn=-1;                    // the local player's respawn countdown, -1 alive
 bool over=false;std::string winner;    // frag or time limit reached
 std::vector<ChatLine> chat;            // the last lines of chat and notices, newest last
};

}

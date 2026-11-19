#pragma once
// The game over a Session (docs/Systems/multiplayer.md): the host's game is the truth. Created by Game when its
// Session enters a lobby, dropped when it leaves. Fills Game::m_remotePlayers and Game::m_matchView every update.
//  - Match.cpp: joining (hello, welcome), the map handed to a guest who lacks it, starting, the host's hand-over.
//  - MatchPlay.cpp: player state both ways, shots traced by the host, deathmatch and co-op rules, creatures,
//    doors and pickups in co-op, what the presentation reads.
//  - MatchTest.cpp: --net-test, several Games in one process on one LoopbackHub.
#include "NetTypes.h"
#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>
namespace retro { class Game; struct InputState; struct CustomCampaign; class World; }
namespace retro::net {
namespace wire { struct Writer; struct Reader; }

constexpr std::uint16_t ProtocolVersion=2;      // 2: a player state carries torch and locker; co-op scripts, state, people
constexpr int MaxSlots=16;
constexpr std::size_t MapChunkBytes=1100;     // a MapData packet's file bytes at most

// The content hash a lobby advertises (HostOptions::mapHash) and a guest checks its own copy against: FNV-1a 64
// of the file's bytes as 16 hex digits. mapFileHash is "" when the file cannot be read.
std::string contentHash(const std::uint8_t* data,std::size_t size);
std::string mapFileHash(const std::filesystem::path& file);

// A player's state as the Match keeps it: feet in world space (the chunk offset added), angles in radians.
struct NetPlayerState{
 int level=0;Point3 world{};
 float angle=0,pitch=0;Vec2 velocity{};
 bool crouched=false,prone=false,grounded=true,vr=false,alive=true;
 bool torch=false,hidden=false;          // their flashlight is on; they are shut in a locker (what creatures and guards see)
 int weapon=1;std::uint8_t shots=0;     // shots: a counter that turns over with every shot fired
 float reloadAge=99,health=100;
 std::uint8_t life=0;                   // bumped by the host at each respawn or correction: older reports are stale
 AvatarPose pose{};                     // vr: positions relative to the feet
};
// Encodes / decodes one player state (the host adds health); exposed for the size report.
void writePlayerState(wire::Writer& w,const NetPlayerState& s,Vec2 levelOrigin,bool withHealth);
bool readPlayerState(wire::Reader& r,NetPlayerState& s,bool withHealth);   // s.world holds level-local x,y after

class Match{
public:
 explicit Match(Game& game);
 ~Match();
 // Once a frame from Game::update, after the game's own frame: reads the packets, runs the host's rules or the
 // guest's side, sends what is due, and fills remotePlayers and matchView.
 void update(const InputState& input,float dt);
 // Game::shoot in a match: the flash, sound and recoil are already done. The host traces it as it traces every
 // guest's shot; a guest sends it to the host. True: the match resolves it.
 bool localShot(Point3 origin,Point3 direction,int weapon);
 bool started()const{return m_phase==Phase::Playing;}
 bool played()const{return m_played;}          // it got as far as the map (the title comes back when it ends)
 bool finished()const{return m_finished;}
 const std::string& endStatus()const{return m_endStatus;}
 // Traffic: the last snapshot this host sent (bytes), and the payload bytes sent and received so far.
 std::size_t lastSnapshotBytes()const{return m_lastSnapshotBytes;}
 std::size_t bytesSent()const{return m_bytesSent;}
 std::size_t bytesReceived()const{return m_bytesReceived;}
 // The bytes of a snapshot of n players (none in VR), as the host builds it.
 static std::size_t snapshotBytes(int players,bool vr);
 static std::size_t guestStateBytes(bool vr);
 static bool selfTest(std::ostream& report);
private:
 enum class Phase{Joining,Downloading,Playing,Ended};
 struct Sample{float t=0;NetPlayerState s;};
 struct Slot{
  bool used=false,ready=false,spawned=false,host=false;
  PeerId peer;std::string name;int character=0,frags=0,deaths=0,pingMs=-1;
  std::deque<Sample> history;            // host clock seconds; the newest last
  NetPlayerState latest;bool haveState=false;
  float health=100;bool alive=true;float respawnIn=-1,deathAt=-1;std::uint8_t life=0;
  float acceptedAt=-1,correctCooldown=0,spawnGrace=0,lastShotAt=-99;std::uint8_t lastShots=0;
  std::uint16_t seq=0;bool seqSeen=false;
 };
 struct Spawn{int level=0;Point3 world{};float yaw=0;};
 struct Upload{PeerId peer;std::size_t offset=0;};
 struct CreatureTarget{bool set=false;Vec2 pos{};float z=0,heading=0;};

 // Match.cpp
 void begin();
 void hostBegin();
 void guestHello();
 void becomeHost();
 void end(const std::string& status);
 void startWorld(std::shared_ptr<const CustomCampaign> campaign,int level);
 std::shared_ptr<const CustomCampaign> findCampaign(const std::string& file,const std::string& hash);
 std::filesystem::path mapsFolder()const;
 std::filesystem::path localMapFile(const std::string& file)const;
 void finishDownload();
 void handleControl(const Packet& p);
 void handleMapData(const Packet& p);
 void sendUploads();
 void send(const PeerId& to,std::uint8_t channel,const wire::Writer& w,bool reliable);
 void broadcast(std::uint8_t channel,const wire::Writer& w,bool reliable,int exceptSlot=-1);
 void sendRoster();
 int slotOf(const PeerId& peer)const;
 int freeSlot(int hint)const;
 // MatchPlay.cpp
 void hostFrame(float dt);
 void guestFrame(float dt);
 void handleState(const Packet& p);
 void handleEvent(const Packet& p);
 NetPlayerState localState()const;
 void acceptGuestState(int slot,const NetPlayerState& s);
 void resolveShot(int shooter,int weapon,int level,Point3 origin,Point3 direction,float viewTime);
 void damage(int victim,int attacker,float amount,int weapon);
 void registerDeath(int victim,int killer,int weapon);
 void respawn(int slot);
 void applySpawn(int level,Point3 world,float yaw);
 void teleportLocal(int level,Point3 world);
 void placePlayer(int slot,int level,Point3 world);   // the host moves a player (a correction they cannot refuse)
 Spawn chooseSpawn(int slot);
 Spawn besideHost(int slot);
 void buildSpawns();
 void roundOver();
 void newRound();
 void sendSnapshot();
 void sendGuestState();
 void sendCreatures();
 void sendDoors();
 void trackPickups();
 void trackDoors(float dt);
 void smoothCreatures(float dt);
 void clearCreatures();
 void pushFeed(int killer,int victim,int weapon);
 bool sampleAt(const Slot& slot,float t,NetPlayerState& out)const;
 void publish(float dt);
 Vec2 origin(int level)const;
 int levelAt(Vec2 world)const;
 const World* worldOf(int level)const;
 float renderTime()const;
 float hostNow()const;
 // MatchCoop.cpp: co-op's one world. The host's minds see every player (Game::players(): the guests' bodies
 // are kept here), and what they do reaches the guests: script effects, the state store, the people.
 void coopHostFrame(float dt);
 void coopGuestFrame(float dt);
 void syncBodies(float dt);
 void sendStateSync(bool full,const PeerId* to);
 void sendScriptFx();
 void sendActors();
 void smoothActors(float dt);
 void clearCoop();
 bool handleCoopEvent(int slot,unsigned type,wire::Reader& r);   // slot: the guest it came from (host), -1 a guest
 bool handleCoopState(unsigned type,wire::Reader& r);
 bool clockState(std::uint32_t id)const;
 struct ActorTarget{bool set=false;int level=0;Vec2 pos{};float z=0,yaw=0;};
 std::vector<std::pair<std::uint32_t,int>> m_sentStates,m_sentObjectives,m_sentProps;
 std::vector<std::pair<std::uint32_t,float>> m_clockSent;   // a running clock's id and when it was last sent
 std::vector<std::uint32_t> m_clocks;                       // sorted ids of states that tick every frame
 float m_syncTimer=0,m_fullSyncTimer=0,m_actorTimer=0;std::uint32_t m_lastActorsMs=0;
 std::vector<ActorTarget> m_actorTargets;
 std::size_t m_coopBytes=0;   // co-op world traffic sent (scripts, state, people)

 Game& m_game;
 Session* m_session=nullptr;
 bool m_host=false;PeerId m_self,m_hostId;
 Mode m_mode=Mode::Deathmatch;
 Phase m_phase=Phase::Joining;
 bool m_finished=false,m_begun=false,m_played=false;std::string m_endStatus,m_notice;float m_noticeTime=0;
 int m_fragLimit=20;float m_timeLimit=600;bool m_friendlyFire=false;int m_maxPlayers=8;
 std::string m_mapFile,m_mapHash,m_mapName;int m_mapLevel=0;
 std::shared_ptr<const CustomCampaign> m_campaign;
 std::vector<std::uint8_t> m_mapBytes,m_download;std::size_t m_downloadSize=0,m_downloadHave=0;
 std::vector<Upload> m_uploads;
 std::array<Slot,MaxSlots> m_slots;int m_selfSlot=-1;
 float m_clock=0,m_helloTimer=0,m_hostOffset=0;bool m_haveOffset=false;
 std::uint32_t m_lastSnapshotMs=0;float m_lastSnapshotLocal=0;bool m_haveSnapshot=false;
 std::uint32_t m_lastCreaturesMs=0;
 float m_stateTimer=0,m_creatureTimer=0,m_doorTimer=0,m_rosterTimer=0;bool m_rosterDirty=false;
 std::uint16_t m_stateSeq=0;std::uint8_t m_shotCounter=0,m_lifeLocal=0;
 int m_round=0;float m_roundClock=0,m_timeLeft=-1,m_restartIn=-1,m_selfRespawnIn=-1;bool m_over=false;std::string m_winner;
 std::vector<FeedLine> m_feed;
 std::vector<Spawn> m_spawns;int m_spawnTurn=0;
 std::vector<std::vector<char>> m_pickupSeen;
 std::vector<char> m_doorSeen;std::vector<float> m_doorTouched;int m_doorLevel=-1;
 std::vector<CreatureTarget> m_creatureTargets;int m_creatureLevel=-1;
 int m_lastLevel=-1;
 std::size_t m_lastSnapshotBytes=0,m_bytesSent=0,m_bytesReceived=0;
 friend bool testMatch(std::ostream& report);
};
// --net-test: matches over a LoopbackHub, several Games in one process.
bool testMatch(std::ostream& report);
}

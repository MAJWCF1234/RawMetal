#include <optional>
#include <thread>
#include <chrono>
#include "Game.h"
#include "EngineRegressionScene.h"
#include "../world/CustomCampaign.h"
#include <windows.h>
#undef PlaySound
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <type_traits>
#include <stdexcept>
#include <mutex>
#include <cctype>

namespace retro {
namespace {
// A sanity cap, not a budget. It was 2 MB: a playthrough into Chapter 2 writes more than that
// (every chunk's props are saved, and broken ones leave their pieces), so a save written late
// in the game was refused by the very build that wrote it.
constexpr size_t MaxSaveBytes=64*1024*1024;
uint32_t checksum(const std::string& data){uint32_t hash=2166136261u;for(unsigned char c:data){hash^=c;hash*=16777619u;}return hash;}
struct Writer {
 static constexpr bool reading=false;
 std::ostringstream stream;
 Writer(){stream.imbue(std::locale::classic());stream<<std::setprecision(std::numeric_limits<float>::max_digits10);}
 // A number the reader would refuse (NaN, infinity, past ten million) is written as the
 // nearest it accepts, and counted: one glitched value must not make the whole save unloadable.
 int repaired=0;
 template<class T> void one(T& v){if constexpr(std::is_enum_v<T>)stream<<int(v);
  else if constexpr(std::is_floating_point_v<T>){T out=v;if(!std::isfinite(double(out))){out=0;++repaired;}else if(std::fabs(double(out))>9999999.){out=out<0?T(-9999999):T(9999999);++repaired;}stream<<out;}
  else stream<<v;stream<<' ';}
 template<class... T> void operator()(T&... v){(one(v),...);stream<<'\n';}
};
struct Reader {
 static constexpr bool reading=true;
 std::istringstream stream;
 explicit Reader(const std::string& s):stream(s){stream.imbue(std::locale::classic());}
 template<class T> void one(T& v){
  if constexpr(std::is_enum_v<T>||std::is_same_v<T,bool>){int n=0;if(!(stream>>n)||n<0||n>(std::is_same_v<T,bool>?1:16))throw std::runtime_error(std::is_same_v<T,bool>?"invalid flag "+std::to_string(n):"invalid enum value "+std::to_string(n));v=static_cast<T>(n);}
  else if constexpr(std::is_floating_point_v<T>){if(!(stream>>v)||!std::isfinite(double(v))||std::fabs(double(v))>10000000)throw std::runtime_error("invalid number");}
  else if(!(stream>>v))throw std::runtime_error("invalid integer");
 }
 template<class... T> void operator()(T&... v){(one(v),...);}
};
std::filesystem::path slotPath(const std::wstring& directory,int slot){return std::filesystem::path(directory)/("slot-"+std::to_string(slot+1)+".rms");}
std::filesystem::path checkpointPath(const std::wstring& directory){return std::filesystem::path(directory)/"checkpoint.rms";}
void writeAtomic(const std::filesystem::path& path,const std::string& data){auto temp=path;temp+=L".tmp";std::filesystem::create_directories(path.parent_path());{std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(data.data(),std::streamsize(data.size()));file.flush();if(!file)throw std::runtime_error("write failed");file.close();if(file.fail())throw std::runtime_error("close failed");}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("replace failed");}
// Why the last load on this thread was refused. Every refusal names its reason:
// the menu shows it short and RawMetal-load.txt keeps it whole.
thread_local std::string t_loadFailure;
bool refuse(const std::string& why){t_loadFailure=why.empty()?"unknown error":why;return false;}
// Sections the last format 28 load on this thread skipped (unknown to this build), and how many
// chunks its "game" section said it carries.
thread_local std::vector<std::string> t_skippedSections;
thread_local int t_sectionChunks=0;
// Appends one line, stamped, to RawMetal-load.txt beside the other RawMetal-*.txt reports.
void appendLoadLog(const std::string& line){
 try{std::ofstream log("RawMetal-load.txt",std::ios::app);SYSTEMTIME t;GetLocalTime(&t);char stamp[32];std::snprintf(stamp,sizeof stamp,"%04d-%02d-%02d %02d:%02d:%02d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
  log<<stamp<<' '<<line<<'\n';}catch(...){}
}
void logLoadFailure(const std::filesystem::path& path,const std::string& why){
 appendLoadLog("load refused: "+path.string()+" (this build reads save versions 1-"+std::to_string(Game::SaveVersion)+"): "+why);
}
// Writes a file that must not exist yet: a backup never replaces another file.
bool writeNew(const std::filesystem::path& path,const std::string& data){
 HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 DWORD written=0;const bool ok=WriteFile(file,data.data(),DWORD(data.size()),&written,nullptr)&&written==data.size()&&FlushFileBuffers(file);CloseHandle(file);
 if(!ok){std::error_code e;std::filesystem::remove(path,e);}return ok;
}
// A list in an archive: its count, bounded, then each entry.
template<class A,class V,class F> void archiveList(A& a,V& values,F visit,int limit=1024){int count=int(values.size());a(count);if(count<0||count>limit)throw std::runtime_error("invalid collection");if constexpr(A::reading)values.resize(size_t(count));for(auto&v:values)visit(v);}
// A section frame: name length and name, version, flags, body length, body checksum.
constexpr size_t MaxSectionName=48;
constexpr std::uint16_t SectionRequired=1;
bool validSectionName(std::string_view name){if(name.empty()||name.size()>MaxSectionName)return false;for(char c:name)if(!(std::isalnum(static_cast<unsigned char>(c))||c=='.'||c=='-'||c=='_'))return false;return true;}
template<class T> void putFixed(std::string& out,T v){for(size_t i=0;i<sizeof(T);++i)out.push_back(char((std::uint64_t(v)>>(8*i))&0xff));}
template<class T> T getFixed(const std::string& in,size_t at){std::uint64_t v=0;for(size_t i=0;i<sizeof(T);++i)v|=std::uint64_t(static_cast<unsigned char>(in[at+i]))<<(8*i);return T(v);}
struct SectionFrame {std::string name;int version=0;std::uint16_t flags=0;size_t offset=0,size=0;};
// Splits a format 28 payload into its sections, checking every frame and body. Throws the reason.
std::vector<SectionFrame> sectionFrames(const std::string& payload,std::uint32_t(*sum)(const std::string&,size_t,size_t)){
 std::vector<SectionFrame> frames;size_t at=0;
 while(at<payload.size()){
  const std::string which="section "+std::to_string(frames.size()+1);
  const size_t length=static_cast<unsigned char>(payload[at]);
  if(length==0||length>MaxSectionName)throw std::runtime_error(which+" has a bad name (damaged save)");
  if(payload.size()-at<1+length+12)throw std::runtime_error("truncated save: "+which+"'s frame is cut off");
  SectionFrame f;f.name=payload.substr(at+1,length);if(!validSectionName(f.name))throw std::runtime_error(which+" has a bad name (damaged save)");at+=1+length;
  f.version=getFixed<std::uint16_t>(payload,at);f.flags=getFixed<std::uint16_t>(payload,at+2);const auto size=getFixed<std::uint32_t>(payload,at+4),check=getFixed<std::uint32_t>(payload,at+8);at+=12;
  if(size>payload.size()-at)throw std::runtime_error("truncated save: section '"+f.name+"' holds "+std::to_string(size)+" bytes, "+std::to_string(payload.size()-at)+" left");
  f.offset=at;f.size=size;if(sum(payload,at,size)!=check)throw std::runtime_error("section '"+f.name+"' is damaged (checksum mismatch)");
  if(f.version<1)throw std::runtime_error("section '"+f.name+"' has version 0");
  for(const auto& other:frames)if(other.name==f.name)throw std::runtime_error("section '"+f.name+"' appears twice");
  frames.push_back(std::move(f));at+=size;
 }
 return frames;
}
std::uint32_t rangeChecksum(const std::string& data,size_t at,size_t size){uint32_t hash=2166136261u;for(size_t i=at;i<at+size;++i){hash^=static_cast<unsigned char>(data[i]);hash*=16777619u;}return hash;}
void appendSection(std::string& payload,const std::string& name,std::uint16_t version,std::uint16_t flags,const std::string& body){
 payload.push_back(char(name.size()));payload+=name;putFixed(payload,version);putFixed(payload,flags);putFixed(payload,std::uint32_t(body.size()));putFixed(payload,rangeChecksum(body,0,body.size()));payload+=body;
}
// The section registry: the built-in sections first, then those systems register.
struct SectionRegistry {std::mutex lock;std::vector<SaveSection> sections;bool ready=false;};
SectionRegistry& sectionRegistry(){static SectionRegistry registry;return registry;}
}
// What the rigid-body solver plugs into the "physics" section (SaveSections.h).
PhysicsSaveHooks& physicsSaveHooks(){static PhysicsSaveHooks hooks;return hooks;}
namespace {
// The menu line: the reason, upper case, short enough for the pause menu.
std::string loadFailureLine(const std::string& lead,const std::string& why){std::string line=lead+" / "+why;for(auto&c:line)c=char(std::toupper(static_cast<unsigned char>(c)));if(line.size()>58)line=line.substr(0,55)+"...";return line;}
std::string readFile(const std::filesystem::path& path){
 std::error_code error;auto size=std::filesystem::file_size(path,error);if(error||size>MaxSaveBytes)throw std::runtime_error("missing or oversized save");
 std::ifstream in(path,std::ios::binary);std::string data(size,'\0');if(!in.read(data.data(),std::streamsize(size)))throw std::runtime_error("unreadable save");return data;
}
}
// The campaign a save names (its world and custom campaign key), from those installed or built in.
bool Game::adoptSavedCampaign(){
 if(m_worldId==WorldId::Custom){
  auto found=std::find_if(m_customCampaigns.begin(),m_customCampaigns.end(),[&](const auto& campaign){return campaign&&campaign->key==m_customCampaignKey;});
  if(found!=m_customCampaigns.end())m_customCampaign=*found;
  else if(auto builtIn=builtInCampaignForKey(m_customCampaignKey))m_customCampaign=builtIn;
  else return false;
 }else {m_customCampaign.reset();m_customCampaignKey=0;}
 return true;
}
// The positional text archive of formats 1-27, frozen: it reads every older save, and tests write
// one in any of those versions (a field the version lacked is left out, or for physics props before
// 17, the props themselves). Format 28 is sections (encodeSections).
template<class A> int Game::archiveSave(A& a,int version){
 if(version<1||version>LegacySaveVersion)throw std::runtime_error("the positional archive holds versions 1-"+std::to_string(LegacySaveVersion));
 if(version>=9){
  a(m_worldId);
  bool known=m_worldId==WorldId::Campaign||m_worldId==WorldId::Ashfall||(version>=12&&m_worldId==WorldId::Custom);
  if(!known)throw std::runtime_error("Unknown saved world");
 }else if constexpr(A::reading)m_worldId=WorldId::Campaign;
 if(version>=12){
  a(m_customCampaignKey);
  if constexpr(A::reading)if(!adoptSavedCampaign())throw std::runtime_error("Custom campaign required by save is not installed");
 }else if constexpr(A::reading){m_customCampaign.reset();m_customCampaignKey=0;}
 auto vec=[&](Vec2& v){a(v.x,v.y);};
 a(m_level,m_elapsed,m_won,m_medkits,m_weaponEquipped,m_heldClutter);
 if(version>=4){a(m_hazmat.initialized,m_hazmat.sleeping,m_hazmat.quiet,m_hazmat.accumulator);
  for(auto&points:{&m_hazmat.p,&m_hazmat.previous})for(auto&p:*points){a(p.x,p.y,p.z);if(p.x<0||p.x>24||p.y<0||p.y>24||p.z<-12||p.z>20)throw std::runtime_error("Invalid hazmat pose");}
  if(m_hazmat.accumulator<0||m_hazmat.accumulator>.06f||m_hazmat.quiet<0||m_hazmat.quiet>10)throw std::runtime_error("Invalid hazmat simulation");
 }
 // Before version 19 the one ammo/loaded pair was the shotgun's.
 auto&p=m_player;auto&shells=p.pool(WeaponId::Shotgun);vec(p.pos);a(p.angle,p.pitch,p.health,shells.total);if(version>=2)a(shells.loaded);a(p.z,p.verticalVelocity,p.grounded,p.crouched,p.eye);vec(m_velocity);
 a(m_weaponKick,m_shotCooldown);if(version>=2)a(m_reloadTimer);a(m_shotAge,m_holster,m_punchAge,m_punchLeft,m_guarding,m_verticalSpring,m_verticalSpringVelocity,m_stepDistance,m_stepVariant,m_jumpBuffer,m_coyote);
 a(m_weaponMotion.yaw,m_weaponMotion.pitch,m_weaponMotion.bob,m_weaponMotion.back,m_weaponMotion.elbow,m_weaponMotion.bolt,m_weaponMotion.roll);vec(m_sway);vec(m_swayVelocity);a(m_elbowVelocity);
 for(int i=0;i<(version>=19?int(m_itemCells.size()):3);++i)a(m_itemCells[size_t(i)]);
 // Version 19: the handgun's pool, the firearm in hand, a switch under way and the reload's length.
 if(version>=19){auto&nine=p.pool(WeaponId::Handgun);a(nine.total,nine.loaded,m_equippedWeapon,m_switchTarget,m_switchRaise,m_reloadLength,m_emptyReload);
  if(!playerFirearm(m_equippedWeapon)||(m_switchTarget!=WeaponId::Fists&&!playerFirearm(m_switchTarget)))throw std::runtime_error("invalid saved weapon");}
 // Older: the upgrade step to 19 (refitLegacySave) sets the shotgun in hand and an empty 9 mm pool.
 auto list=[&](auto& values,auto visit,int limit=1024){int count=int(values.size());a(count);if(count<0||count>limit)throw std::runtime_error("invalid collection");if constexpr(A::reading)values.resize(count);for(auto&v:values)visit(v);};
 if(version>=6){
  list(m_states,[&](StateValue&v){a(v.id,v.value);if(v.id==0)throw std::runtime_error("invalid state id");},8192);
  if constexpr(A::reading){
   // Older saves kept insertion order. Normalize once before runtime lookups.
   std::sort(m_states.begin(),m_states.end(),[](const StateValue&left,const StateValue&right){return left.id<right.id;});
   if(std::adjacent_find(m_states.begin(),m_states.end(),[](const StateValue&left,const StateValue&right){return left.id==right.id;})!=m_states.end())throw std::runtime_error("duplicate saved state");
  }
  list(m_objectives,[&](StateValue&v){a(v.id,v.value);if(v.id==0||v.value<0||v.value>3)throw std::runtime_error("invalid objective");});
  list(m_questItems,[&](QuestItemStack&v){a(v.id,v.count);if(v.id==0||v.count<=0||v.count>99)throw std::runtime_error("invalid quest item");});
  list(m_firedEvents,[&](StateId&id){a(id);if(id==0)throw std::runtime_error("invalid event id");});
 }
 int archivedChunks=version>=12?chunkCount():version>=11?worldChunkCount(m_worldId):version>=10?(m_worldId==WorldId::Campaign?6:12):version>=7?6:4;
 if(version>=11){a(archivedChunks);int available=version>=12?chunkCount():worldChunkCount(m_worldId);if(archivedChunks<1||archivedChunks>available||archivedChunks>MaxChunks)throw std::runtime_error("Invalid chunk count");}
 for(int index=0;index<archivedChunks;++index)try{archiveChunk(a,version,index,ChunkAll);}catch(const std::exception& error){throw std::runtime_error("map "+std::to_string(index)+": "+error.what());}
 return archivedChunks;
}
// One chunk's parts. The positional archive (1-27) takes every part in this order; format 28 keeps
// each part in its own section ("world", "creatures", "pickups", "props"), every chunk in turn, with
// `version` the format (28: every field the positional archive gained up to 27).
template<class A> void Game::archiveChunk(A& a,int version,int index,unsigned parts){
 auto vec=[&](Vec2& v){a(v.x,v.y);};
 auto list=[&](auto& values,auto visit,int limit=1024){archiveList(a,values,visit,limit);};
 auto&c=m_chunks[index];if constexpr(A::reading)if(parts&ChunkWorld){c.world=makeWorld(index);if(loadProgressCounter)++*loadProgressCounter;}auto&w=c.world;
 if(parts&ChunkWorld){
  a(c.kills,c.resident,w.m_controlReleased,w.m_liftPhase,w.m_liftHeight,w.m_liftTimer,w.m_liftVelocity,w.m_liftCaught,w.m_reactorStage,w.m_reactorFault);
  if(version>=16)list(w.m_destroyedTiles,[&](std::pair<int,int>&t){a(t.first,t.second);if(t.first<0||t.second<0||t.first>=World::Width||t.second>=World::Height)throw std::runtime_error("invalid destroyed tile");});
  if(version>=17){list(w.m_brokenBreakables,[&](std::uint16_t&i){a(i);if(i>=w.breakables().size())throw std::runtime_error("invalid broken pane");});
   std::vector<std::pair<int,float>> damaged;if constexpr(!A::reading)for(int i=0;i<int(w.m_breakableDamage.size());++i)if(w.m_breakableDamage[i]>0&&w.breakableIntact(i))damaged.push_back({i,w.m_breakableDamage[i]});
   list(damaged,[&](std::pair<int,float>&v){a(v.first,v.second);if(v.first<0||v.first>=int(w.breakables().size())||v.second<0)throw std::runtime_error("invalid pane damage");});
   if constexpr(A::reading){for(auto [i,damage]:damaged)w.m_breakableDamage[i]=damage;auto broken=w.m_brokenBreakables;for(auto i:broken)w.removeBreakable(i);}
  }
  if(version>=18){std::vector<std::pair<int,float>> ages;if constexpr(!A::reading)for(int i=0;i<int(w.m_breakableAge.size());++i)if(w.breakables()[i].holds&&w.m_breakableDamage[i]>0)ages.push_back({i,w.m_breakableAge[i]});
   list(ages,[&](std::pair<int,float>&v){a(v.first,v.second);if(v.first<0||v.first>=int(w.breakables().size())||v.second<0||v.second>21)throw std::runtime_error("invalid pane age");});if constexpr(A::reading)for(auto [i,age]:ages)w.m_breakableAge[i]=age;
  }
  if constexpr(A::reading)if(!w.m_destroyedTiles.empty()){w.m_lightSolids.clear();w.m_lightNodes.clear();}
  int doors=int(w.m_doors.size());a(doors);if(doors<0||doors>128)throw std::runtime_error("invalid door count");
  if constexpr(A::reading){
   for(int door=0;door<doors;++door){float open=0;bool opening=false;a(open,opening);if(open<0||open>1)throw std::runtime_error("invalid door");
    // Version 24: where a hand left the leaf, and the battering it has taken.
    float rest=-1,damage=0;bool broken=false;if(version>=24){a(rest,damage,broken);if(rest>1||!(damage>=0&&damage<1e6f))throw std::runtime_error("invalid door");}
    if(door<int(w.m_doors.size())){auto&d=w.m_doors[door];d.open=open;d.opening=opening;d.rest=rest<0?-1.f:std::min(rest,d.maxOpen);d.damage=damage;d.broken=broken&&d.breakable();}}
  }else for(auto&d:w.m_doors){a(d.open,d.opening);if(d.open<0||d.open>1)throw std::runtime_error("invalid door");if(version>=24)a(d.rest,d.damage,d.broken);}
 }
 if(parts&ChunkCreatures)
  list(c.enemies,[&](Enemy&e){vec(e.pos);a(e.hp,e.attackCooldown,e.painFlash,e.alive,e.kind,e.maxHp,e.deathTime,e.windup,e.strike,e.heading,e.gait,e.moving,e.voiceTimer,e.idleTimer,e.z,e.awareness,e.searchTime,e.verticalVelocity,e.repathTimer,e.lastKnownZ);vec(e.waypoint);vec(e.home);vec(e.lastKnown);a(e.state);if(version>=5)a(e.stalkMode,e.stalkTimer,e.stalkSide);if(version>=14)a(e.scriptSpawned);if(version>=15)a(e.wakeState);
   if(version>=16){a(e.homeZ,e.scale,e.originLevel,e.surfaceMode);vec(e.originHome);vec(e.surfaceNormal);if(e.scale<.25f||e.scale>4||e.originLevel<-1||e.originLevel>=MaxChunks||e.surfaceMode<0||e.surfaceMode>2)throw std::runtime_error("invalid creature fields");}
   // Before version 20 the recognition meter was not kept: the upgrade step to 20 rebuilds it.
   if(version>=20)a(e.recognition);
   // Version 22: a recurring hunter's name and prey, and its walk in through a seam.
   if(version>=22){a(e.hunter,e.prey,e.arrive);if(e.prey<-1||e.prey>4096||!(e.arrive>=0&&e.arrive<=30))throw std::runtime_error("invalid creature fields");}
   if(int(e.kind)>(version>=23?int(Enemy::Kind::Machine):version>=16?int(Enemy::Kind::Transformed):version>=15?int(Enemy::Kind::Chimera):version>=13?int(Enemy::Kind::Mutant):version>=3?3:2)||int(e.state)>int(Enemy::State::Search)||int(e.stalkMode)>int(Enemy::StalkMode::Recover)||e.stalkTimer<0||e.stalkTimer>60||std::fabs(e.stalkSide)>1.01f||e.maxHp<=0||!(e.recognition>=0&&e.recognition<=1))throw std::runtime_error("invalid creature (kind "+std::to_string(int(e.kind))+", state "+std::to_string(int(e.state))+", stalk "+std::to_string(int(e.stalkMode))+" for "+std::to_string(e.stalkTimer)+" s, max hp "+std::to_string(e.maxHp)+", recognition "+std::to_string(e.recognition)+")");},4096);
 if(parts&ChunkPickups)
  list(c.pickups,[&](Pickup&v){vec(v.pos);a(v.kind,v.active);if(int(v.kind)>(version>=19?int(PickupKind::HandgunAmmo):int(PickupKind::Ammo)))throw std::runtime_error("invalid pickup");});
 if(parts&ChunkProps){
  // A save older than version 17 had no physics props: one written in that form leaves them out.
  std::vector<Clutter> olderClutter;
  decltype((c.clutter)) clutterList=[&]()->decltype((c.clutter)){if constexpr(!A::reading){if(version<17){olderClutter=c.clutter;std::erase_if(olderClutter,[](const Clutter&v){return v.kind==ClutterModelKind;});return olderClutter;}}return c.clutter;}();
  // Each read prop starts as a new one: resizing keeps the fresh game's props in place, and a format
  // before 17, which carries no model, would leave one of them a junk kind still holding its model.
  list(clutterList,[&](Clutter&v){if constexpr(A::reading)v=Clutter{};vec(v.pos);vec(v.velocity);a(v.z,v.vz,v.yaw,v.spin,v.kind,v.projectile,v.impactCooldown,v.pitch,v.roll,v.pitchSpeed,v.rollSpeed,v.restTime,v.sleeping);if(v.kind<0||v.kind>(version>=17?7:6))throw std::runtime_error("invalid clutter");if(version>=17){a(v.model,v.damage,v.authored,v.broken,v.homeZ);vec(v.home);for(auto&d:v.dims)a(d);if((v.kind==7)!=(v.model>=0)||v.model>=FacilityModelCount||v.damage<0||(v.kind==7&&std::any_of(v.dims.begin(),v.dims.end(),[](float d){return d<=0||d>3.f;})))throw std::runtime_error("invalid prop");}
   // Version 25: what a shard was broken from, so it is drawn as a piece of it (Clutter::shards).
   if(version>=25){a(v.shardOf,v.piece,v.shardModel);for(auto&d:v.from)a(d);
    if(int(v.shardOf)<0||int(v.shardOf)>=int(ShardOf::Count)||v.piece<0||v.piece>7||v.shardModel<-1||v.shardModel>=FacilityModelCount||std::any_of(v.from.begin(),v.from.end(),[](float d){return !(d>=0&&d<=30.f);})||(v.kind==6&&std::any_of(v.dims.begin(),v.dims.end(),[](float d){return !(d>=0&&d<=3.f);})))throw std::runtime_error("invalid shard");}},16384); // a map whose props were all broken holds thousands of pieces
 }
 if(parts&ChunkWorld){
  // Version 21: which leads have torn. The cables themselves are hung again on load.
  if(version>=21){std::vector<std::uint16_t> torn;if constexpr(!A::reading)for(size_t i=0;i<w.m_leadRopes.size();++i)if(w.m_leadRopes[i].torn)torn.push_back(std::uint16_t(i));
   list(torn,[&](std::uint16_t&i){a(i);if(i>=w.leads().size())throw std::runtime_error("invalid lead");});if constexpr(A::reading)for(auto i:torn)w.m_leadRopes[i].torn=true;}
  // Version 27: each pane's hits (glass that breaks a piece at a time; GlassFracture.h), one number a hit:
  // where on the pane (16 bits each way), the hole's radius and the strain (8 bits each). The gone glass is worked out again.
  if(version>=27){std::vector<std::pair<std::uint16_t,std::vector<std::uint64_t>>> panes;
   if constexpr(!A::reading)for(const auto&[i,hits]:w.packedPaneHits()){std::vector<std::uint64_t> packed;for(const auto&h:hits)packed.push_back(std::uint64_t(h.u)<<32|std::uint64_t(h.v)<<16|std::uint64_t(h.radius)<<8|h.strain);panes.push_back({i,packed});}
   list(panes,[&](std::pair<std::uint16_t,std::vector<std::uint64_t>>&pane){a(pane.first);if(pane.first>=w.breakables().size())throw std::runtime_error("invalid pane");
    list(pane.second,[&](std::uint64_t&h){a(h);if(h>>48)throw std::runtime_error("invalid pane hit");},64);},4096);
   if constexpr(A::reading){std::vector<std::pair<std::uint16_t,std::vector<glass::PackedHit>>> hits;
    for(const auto&[i,packed]:panes){std::vector<glass::PackedHit> one;for(auto h:packed)one.push_back({std::uint16_t(h>>32),std::uint16_t(h>>16),std::uint8_t(h>>8),std::uint8_t(h)});hits.push_back({i,one});}w.restorePaneHits(hits);}}
  if(int(w.m_liftPhase)>int(World::LiftPhase::Crashed)||int(w.m_reactorStage)>int(World::ReactorStage::Released)||w.m_liftHeight<-9||w.m_liftHeight>9||w.m_liftTimer<0||c.kills<0)throw std::runtime_error("invalid world state");
  if constexpr(A::reading){w.restoreLift(w);if(!c.resident)w.unloadGeometry();}
 }
}
std::string Game::encodeSave()const{
 // Each pool is clamped to its own magazine or tube.
 Game snapshot=*this;for(auto id:{WeaponId::Shotgun,WeaponId::Handgun}){auto&rounds=snapshot.m_player.pool(id);rounds.loaded=std::clamp(rounds.loaded,0,std::clamp(rounds.total,0,playerWeaponDef(id).cycle.tubeCapacity));}
 snapshot.m_player.pool(WeaponId::Fists)={};snapshot.stripSummons();snapshot.storeChunk();int repaired=0;auto payload=snapshot.encodeSections(repaired);
 if(repaired)try{std::ofstream log("RawMetal-load.txt",std::ios::app);log<<"save on map "<<m_level<<": "<<repaired<<" non-finite or out-of-range numbers written as the nearest a load accepts\n";}catch(...){}
 auto data="RAWMETAL_SAVE "+std::to_string(SaveVersion)+" "+std::to_string(checksum(payload))+"\n"+payload;
 // A save no build would load is not written: the slot keeps what it had.
 if(data.size()>MaxSaveBytes)throw std::runtime_error("save is larger than 64 MB");
 return data;
}
// ---- Format 28: sections ----
std::vector<SaveSection> Game::currentSaveSections(){
 auto& registry=sectionRegistry();std::lock_guard guard(registry.lock);
 if(!registry.ready){registry.sections=builtInSaveSections();registry.ready=true;}
 return registry.sections;
}
void Game::registerSaveSection(SaveSection section){
 if(!validSectionName(section.name)||section.version<1||!section.write||!section.read)throw std::invalid_argument("save section '"+section.name+"': a name of 1-48 letters, digits, '.', '-' or '_', a version from 1, write and read");
 auto& registry=sectionRegistry();std::lock_guard guard(registry.lock);
 if(!registry.ready){registry.sections=builtInSaveSections();registry.ready=true;}
 for(auto& existing:registry.sections)if(existing.name==section.name){existing=std::move(section);return;}
 registry.sections.push_back(std::move(section));
}
std::vector<std::string> Game::saveSectionNames(){std::vector<std::string> names;for(const auto& s:currentSaveSections())names.push_back(s.name);return names;}
std::string Game::encodeSections(int& repaired){
 std::string payload;
 for(const auto& section:currentSaveSections()){SaveOut out;section.write(*this,out);repaired+=out.repaired;
  if(out.bytes.empty()&&!section.core)continue;
  if(out.bytes.size()>MaxSaveBytes)throw std::runtime_error("save section '"+section.name+"' is larger than 64 MB");
  appendSection(payload,section.name,section.version,section.required?SectionRequired:0,out.bytes);}
 return payload;
}
int Game::decodeSections(const std::string& payload,std::uint32_t hash){
 const auto frames=sectionFrames(payload,rangeChecksum);
 if(checksum(payload)!=hash)throw std::runtime_error("checksum mismatch (file damaged or truncated)");
 const auto sections=currentSaveSections();t_skippedSections.clear();t_sectionChunks=0;
 auto known=[&](const SectionFrame& f)->const SaveSection*{for(const auto& s:sections)if(s.name==f.name&&f.version<=s.version)return &s;return nullptr;};
 for(const auto& f:frames)if(!known(f)){
  if(f.flags&SectionRequired)throw std::runtime_error("section '"+f.name+"' (version "+std::to_string(f.version)+") is needed and this build does not read it (a newer build wrote the save)");
  t_skippedSections.push_back(f.name+" v"+std::to_string(f.version));}
 for(const auto& section:sections){
  const SectionFrame* found=nullptr;for(const auto& f:frames)if(f.name==section.name)found=&f;
  if(found&&found->version>section.version){if(section.core)throw std::runtime_error("section '"+section.name+"' is version "+std::to_string(found->version)+"; this build reads up to "+std::to_string(section.version));found=nullptr;}
  if(!found){if(section.core)throw std::runtime_error("missing section '"+section.name+"'");if(section.absent)section.absent(*this);continue;}
  SaveIn in(payload.data()+found->offset,found->size);
  try{section.read(*this,in,found->version);if(in.left())throw std::runtime_error(std::to_string(in.left())+" bytes left unread");}
  catch(const std::exception& error){throw std::runtime_error("section '"+section.name+"': "+error.what());}
  if(found->version<section.version&&section.upgrade)section.upgrade(*this,found->version);
 }
 return t_sectionChunks;
}
// The sections this build writes, in the order they are read. A section that indexes another's
// lists comes after it (physics after props).
std::vector<SaveSection> Game::builtInSaveSections(){
 std::vector<SaveSection> list;
 auto add=[&](const char* name,bool core,auto body,std::uint16_t version=1){
  list.push_back({.name=name,.version=version,.core=core,
   .write=[body,version](Game& g,SaveOut& out){body(g,out,version);},
   .read=[body](Game& g,SaveIn& in,int version){body(g,in,version);}});};
 // The world the save belongs to, where in it, and how many chunks the chunk sections carry.
 add("game",true,[](Game& g,auto& a,int){
  a(g.m_worldId);if(int(g.m_worldId)>int(WorldId::Custom))throw std::runtime_error("Unknown saved world");
  a(g.m_customCampaignKey);if constexpr(std::decay_t<decltype(a)>::reading)if(!g.adoptSavedCampaign())throw std::runtime_error("Custom campaign required by save is not installed");
  int chunks=g.chunkCount();a(g.m_level,g.m_elapsed,g.m_won,chunks);
  if(chunks<1||chunks>g.chunkCount()||chunks>MaxChunks)throw std::runtime_error("Invalid chunk count");t_sectionChunks=chunks;});
 // The body, the camera, the weapon in hand and its motion, and the prop held.
 add("player",true,[](Game& g,auto& a,int){auto&p=g.m_player;
  a(p.pos.x,p.pos.y,p.angle,p.pitch,p.health,p.z,p.verticalVelocity,p.grounded,p.crouched,p.eye,g.m_velocity.x,g.m_velocity.y);
  a(g.m_weaponKick,g.m_shotCooldown,g.m_reloadTimer,g.m_shotAge,g.m_holster,g.m_punchAge,g.m_punchLeft,g.m_guarding,g.m_verticalSpring,g.m_verticalSpringVelocity,g.m_stepDistance,g.m_stepVariant,g.m_jumpBuffer,g.m_coyote);
  auto&m=g.m_weaponMotion;a(m.yaw,m.pitch,m.bob,m.back,m.elbow,m.bolt,m.roll,g.m_sway.x,g.m_sway.y,g.m_swayVelocity.x,g.m_swayVelocity.y,g.m_elbowVelocity);
  a(g.m_weaponEquipped,g.m_equippedWeapon,g.m_switchTarget,g.m_switchRaise,g.m_reloadLength,g.m_emptyReload,g.m_heldClutter);
  if(!playerFirearm(g.m_equippedWeapon)||(g.m_switchTarget!=WeaponId::Fists&&!playerFirearm(g.m_switchTarget)))throw std::runtime_error("invalid saved weapon");});
 // What she carries: first aid, each firearm's rounds, where each item sits in the grid, quest items.
 add("inventory",true,[](Game& g,auto& a,int){
  a(g.m_medkits);for(auto id:{WeaponId::Shotgun,WeaponId::Handgun}){auto&rounds=g.m_player.pool(id);a(rounds.total,rounds.loaded);}
  for(auto&cell:g.m_itemCells)a(cell);
  archiveList(a,g.m_questItems,[&](QuestItemStack&v){a(v.id,v.count);if(v.id==0||v.count<=0||v.count>99)throw std::runtime_error("invalid quest item");});});
 // The hung hazmat suit's simulation.
 add("hazmat",true,[](Game& g,auto& a,int){auto&h=g.m_hazmat;a(h.initialized,h.sleeping,h.quiet,h.accumulator);
  for(auto&points:{&h.p,&h.previous})for(auto&p:*points){a(p.x,p.y,p.z);if(p.x<0||p.x>24||p.y<0||p.y>24||p.z<-12||p.z>20)throw std::runtime_error("Invalid hazmat pose");}
  if(h.accumulator<0||h.accumulator>.06f||h.quiet<0||h.quiet>10)throw std::runtime_error("Invalid hazmat simulation");});
 // Script and story state: every state flag (NPCs, grenades and drones keep theirs here too),
 // objectives and the one-shot events that have fired.
 add("script",true,[](Game& g,auto& a,int){
  archiveList(a,g.m_states,[&](StateValue&v){a(v.id,v.value);if(v.id==0)throw std::runtime_error("invalid state id");},8192);
  // As the legacy reader does: states may arrive in any order (a store written before it was kept
  // sorted). Normalize once so indexed lookups work; only a repeated id is refused.
  if constexpr(std::decay_t<decltype(a)>::reading){
   std::sort(g.m_states.begin(),g.m_states.end(),[](const StateValue&l,const StateValue&r){return l.id<r.id;});
   if(std::adjacent_find(g.m_states.begin(),g.m_states.end(),[](const StateValue&l,const StateValue&r){return l.id==r.id;})!=g.m_states.end())throw std::runtime_error("duplicate saved state");}
  archiveList(a,g.m_objectives,[&](StateValue&v){a(v.id,v.value);if(v.id==0||v.value<0||v.value>3)throw std::runtime_error("invalid objective");});
  archiveList(a,g.m_firedEvents,[&](StateId&id){a(id);if(id==0)throw std::runtime_error("invalid event id");});});
 // Each chunk in turn: its world (lift and reactor, broken tiles and breakables, pane damage and
 // glass hits, doors, torn leads), its creatures, its pickups, its props and their pieces.
 auto chunks=[&](const char* name,unsigned part){add(name,true,[part](Game& g,auto& a,int){
  int count=std::decay_t<decltype(a)>::reading?0:g.chunkCount();a(count);if(count!=t_sectionChunks&&std::decay_t<decltype(a)>::reading)throw std::runtime_error("holds "+std::to_string(count)+" maps, the save "+std::to_string(t_sectionChunks));
  for(int index=0;index<count;++index)try{g.archiveChunk(a,SaveVersion,index,part);}catch(const std::exception& error){throw std::runtime_error("map "+std::to_string(index)+": "+error.what());}});};
 chunks("world",ChunkWorld);chunks("creatures",ChunkCreatures);chunks("pickups",ChunkPickups);chunks("props",ChunkProps);
 // Runtime changes to entities' properties (the map's named entities and their `p.` properties).
 add("entities",false,[](Game& g,auto& a,int){auto&store=g.m_saveStore.entities;if constexpr(!std::decay_t<decltype(a)>::reading)if(store.empty())return; // an empty store is not written
  int count=int(store.size());a(count);
  if(count<0||size_t(count)>SaveStore::MaxEntities)throw std::runtime_error("invalid entity count");
  if constexpr(std::decay_t<decltype(a)>::reading){for(int i=0;i<count;++i){int chunk=0;std::string name;a(chunk);a.text(name,SaveStore::MaxName);
    if(chunk<-1||chunk>=MaxChunks||name.empty())throw std::runtime_error("invalid entity");int properties=0;a(properties);if(properties<1||size_t(properties)>SaveStore::MaxProperties)throw std::runtime_error("invalid property count");
    auto [at,fresh]=store.try_emplace(SaveStore::Key{chunk,name});if(!fresh)throw std::runtime_error("entity '"+name+"' twice");
    for(int p=0;p<properties;++p){std::string key;a.text(key,SaveStore::MaxName);SaveValue value;a.value(value,SaveStore::MaxText);if(key.empty()||!at->second.try_emplace(key,std::move(value)).second)throw std::runtime_error("invalid property of '"+name+"'");}}}
  else for(auto& [key,properties]:store){int chunk=key.first;std::string name=key.second;a(chunk);a.text(name);int n=int(properties.size());a(n);for(auto& [property,value]:properties){std::string k=property;auto v=value;a.text(k);a.value(v);}}});
 // Named values scripts set, per map or for the campaign.
 add("variables",false,[](Game& g,auto& a,int){auto&store=g.m_saveStore.variables;if constexpr(!std::decay_t<decltype(a)>::reading)if(store.empty())return;
  int count=int(store.size());a(count);
  if(count<0||size_t(count)>SaveStore::MaxVariables)throw std::runtime_error("invalid variable count");
  if constexpr(std::decay_t<decltype(a)>::reading){for(int i=0;i<count;++i){int chunk=0;std::string name;a(chunk);a.text(name,SaveStore::MaxName);SaveValue value;a.value(value,SaveStore::MaxText);
    if(chunk<-1||chunk>=MaxChunks||name.empty()||!store.try_emplace(SaveStore::Key{chunk,name},std::move(value)).second)throw std::runtime_error("invalid variable '"+name+"'");}}
  else for(auto& [key,value]:store){int chunk=key.first;std::string name=key.second;auto v=value;a(chunk);a.text(name);a.value(v);}});
 // The rigid-body solver's state for props (PropBody: orientation, angular velocity, sleep
 // island), per chunk, by each prop's place in the chunk's props. Written only once the solver
 // sets physicsSaveHooks(); read before the reconciliation, which keeps the props' fields.
 // Layout 2 adds the chained props' segments after them (ChainPose: a mattress's bend, so a draped one
 // loads draped); a layout 1 section has none, and its mattresses load lying straight. Layout 3 adds the
 // joints between props after the chains.
 add("physics",false,[](Game& g,auto& a,int version){const auto& hooks=physicsSaveHooks();
  auto norm=[](const PropBody& b){return std::sqrt(b.orientation[0]*b.orientation[0]+b.orientation[1]*b.orientation[1]+b.orientation[2]*b.orientation[2]+b.orientation[3]*b.orientation[3]);};
  // Written: a unit quaternion (a degenerate one is the identity), spin clamped to 1000 rad/s.
  // Read: refused unless near unit and in range, then made exactly unit.
  auto body=[&](PropBody& b){
   if constexpr(!std::decay_t<decltype(a)>::reading){const float length=norm(b);if(std::isfinite(length)&&length>1e-6f){if(std::fabs(length-1)>2e-6f)for(auto&q:b.orientation)q/=length;}else b.orientation={0,0,0,1};   // (unit to a float's last bits is left as it is: a re-save writes the same bytes)
    for(auto&w:b.angularVelocity)w=std::isfinite(w)?std::clamp(w,-1000.f,1000.f):0.f;b.island=std::max(b.island,-1);}
   for(auto&q:b.orientation)a(q);for(auto&w:b.angularVelocity)a(w);a(b.island);
   if constexpr(std::decay_t<decltype(a)>::reading){const float length=norm(b);
    if(!(length>.9f&&length<1.1f)||std::any_of(b.angularVelocity.begin(),b.angularVelocity.end(),[](float w){return std::fabs(w)>1000.f;})||b.island<-1)throw std::runtime_error("invalid prop body");
    if(std::fabs(length-1)>2e-6f)for(auto&q:b.orientation)q/=length;}};
  // A chained prop's segments: written as captured; read refused unless every number is finite, the
  // root a segment, the bends within a half turn, the rates as the spins, the turns near unit.
  auto chain=[&](ChainPose& c){a(c.root);for(auto&v:c.bend)a(v);for(auto&v:c.rate)a(v);for(auto&m:c.middle)for(auto&v:m)a(v);for(auto&q:c.turn)for(auto&v:q)a(v);
   if constexpr(std::decay_t<decltype(a)>::reading){bool ok=c.root>=0&&c.root<=2;
    for(float v:c.bend)ok&=std::isfinite(v)&&std::fabs(v)<=3.1416f;for(float v:c.rate)ok&=std::isfinite(v)&&std::fabs(v)<=1000.f;
    for(const auto&m:c.middle)for(float v:m)ok&=std::isfinite(v)&&std::fabs(v)<1e6f;
    for(const auto&q:c.turn){const float n=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];ok&=std::isfinite(n)&&n>.81f&&n<1.21f;}
    if(!ok)throw std::runtime_error("invalid chained prop");}};
  // Per chunk, by each prop's place in its props: `what` reads or writes one entry and puts it on the prop.
  auto entries=[&](auto read,const char* kind){int chunks=0;a(chunks);if(chunks<0||chunks>t_sectionChunks)throw std::runtime_error(std::string("invalid ")+kind+" chunk count");int last=-1;
   for(int i=0;i<chunks;++i){int level=0,count=0;a(level,count);if(level<=last||level>=t_sectionChunks)throw std::runtime_error(std::string("invalid ")+kind+" chunk");last=level;auto&props=g.m_chunks[size_t(level)].clutter;
    if(count<1||size_t(count)>props.size())throw std::runtime_error("map "+std::to_string(level)+": invalid "+kind+" count");int lastProp=-1;
    for(int k=0;k<count;++k){int index=0;a(index);if(index<=lastProp||size_t(index)>=props.size())throw std::runtime_error("map "+std::to_string(level)+": invalid "+kind+" index");lastProp=index;read(props[size_t(index)]);}}};
  if constexpr(std::decay_t<decltype(a)>::reading){
   entries([&](Clutter& prop){PropBody b;body(b);if(hooks.restore)hooks.restore(prop,b);},"prop");
   if(version>=2)entries([&](Clutter& prop){ChainPose c;chain(c);if(hooks.restoreChain)hooks.restoreChain(prop,c);},"chained prop");}
  else{std::vector<std::pair<int,std::vector<std::pair<int,PropBody>>>> captured;std::vector<std::pair<int,std::vector<std::pair<int,ChainPose>>>> chains;
   if(hooks.capture)for(int level=0;level<g.chunkCount();++level){std::vector<std::pair<int,PropBody>> bodies;std::vector<std::pair<int,ChainPose>> poses;const auto&props=g.m_chunks[size_t(level)].clutter;
    for(size_t i=0;i<props.size();++i){PropBody b;if(hooks.capture(props[i],b))bodies.push_back({int(i),b});ChainPose c;if(version>=2&&hooks.captureChain&&hooks.captureChain(props[i],c))poses.push_back({int(i),c});}
    if(!bodies.empty())captured.push_back({level,std::move(bodies)});if(!poses.empty())chains.push_back({level,std::move(poses)});}
   bool joints=false;for(int level=0;level<g.chunkCount();++level)joints|=!g.m_chunks[size_t(level)].world.jointStates().empty();
   if(captured.empty()&&chains.empty()&&!joints&&!(version>=4&&!g.m_corpsePoses.empty()))return;
   {int count=int(captured.size());a(count);for(auto& [level,bodies]:captured){int l=level,n=int(bodies.size());a(l,n);for(auto& [index,b]:bodies){int i=index;a(i);body(b);}}}
   if(version>=2){int count=int(chains.size());a(count);for(auto& [level,poses]:chains){int l=level,n=int(poses.size());a(l,n);for(auto& [index,c]:poses){int i=index;a(i);chain(c);}}}}
  // Layout 3: the joints between props (World.h PropJointState), after the chains and only when a map has
  // joints, per chunk that has any, in the map's JOINT order: snapped, its motor on, a winch's reeled length
  // (-1 as authored), the motor's target and speed. A save from a map whose joints have since changed in
  // number keeps the map's.
  // Layout 4: the dead bodies' poses (SaveSections.h CorpsePose), after the joints (a joint chunk count of 0 when no
  // map has joints): their count, then each body's map, slot, signature, a creature's or not, its clock, asleep,
  // shoved, its joints' cuts (count, then each a flag) and its sections (count, then 16 numbers each). Written only
  // when there are any; a section of layout 1-3 loads with every body made again by its death, as before.
  auto corpse=[&](CorpsePose& c){a(c.level,c.slot,c.signature,c.creature,c.age,c.asleep,c.shoved);int joints=int(c.cut.size());a(joints);
   if constexpr(std::decay_t<decltype(a)>::reading){if(c.level<0||c.level>=t_sectionChunks||joints<0||joints>256||!(c.age>=0&&c.age<1e7f))throw std::runtime_error("invalid body");c.cut.resize(size_t(joints));}
   for(auto&k:c.cut){bool cutThere=k!=0;a(cutThere);k=cutThere?1:0;}
   int sections=int(c.sections.size());a(sections);
   if constexpr(std::decay_t<decltype(a)>::reading){if(sections<1||sections>64)throw std::runtime_error("invalid body section count");c.sections.resize(size_t(sections));}
   for(auto&part:c.sections)for(auto&v:part)a(v);
   if constexpr(std::decay_t<decltype(a)>::reading)for(const auto&part:c.sections){const float n=part[3]*part[3]+part[4]*part[4]+part[5]*part[5]+part[6]*part[6];if(!(n>.81f&&n<1.21f))throw std::runtime_error("invalid body section turn");}};
  if constexpr(std::decay_t<decltype(a)>::reading){if(version<3||!a.left())return;int chunks=0;a(chunks);if(chunks<0||chunks>t_sectionChunks)throw std::runtime_error("invalid joint chunk count");int last=-1;
   for(int i=0;i<chunks;++i){int level=0,count=0;a(level,count);if(level<=last||level>=t_sectionChunks||count<1||count>4096)throw std::runtime_error("invalid joint chunk");last=level;
    std::vector<PropJointState> read(static_cast<size_t>(count));for(auto& j:read){a(j.broken,j.motorOn,j.length,j.target,j.speed);
     if(!std::isfinite(j.length)||j.length<-1||j.length>100||!std::isfinite(j.target)||std::fabs(j.target)>1000||!std::isfinite(j.speed)||std::fabs(j.speed)>1000)throw std::runtime_error("map "+std::to_string(level)+": invalid joint");}
    auto& states=g.m_chunks[size_t(level)].world.m_jointStates;if(states.size()==read.size())states=std::move(read);}
   if(version<4||!a.left())return;int bodies=0;a(bodies);if(bodies<0||bodies>4096)throw std::runtime_error("invalid body count");
   for(int i=0;i<bodies;++i){CorpsePose c;corpse(c);c.loaded=true;const auto key=std::make_tuple(c.creature,int(c.level),c.slot);if(!g.m_corpsePoses.emplace(key,std::move(c)).second)throw std::runtime_error("body saved twice");}}
  else{std::vector<int> levels;for(int level=0;level<g.chunkCount();++level)if(!g.m_chunks[size_t(level)].world.jointStates().empty())levels.push_back(level);
   std::vector<CorpsePose> bodies;if(version>=4)for(const auto& [key,c]:g.m_corpsePoses)if(c.level>=0&&c.level<g.chunkCount()&&!c.sections.empty())bodies.push_back(c);
   if(levels.empty()&&bodies.empty())return;int chunks=int(levels.size());a(chunks);for(int level:levels){auto states=g.m_chunks[size_t(level)].world.jointStates();int l=level,count=int(states.size());a(l,count);
    for(auto& j:states){bool broken=j.broken,on=j.motorOn;float length=std::clamp(j.length,-1.f,100.f),target=j.target,speed=j.speed;a(broken,on,length,target,speed);}}
   if(!bodies.empty()){int n=int(bodies.size());a(n);for(auto&c:bodies)corpse(c);}}},4);
 return list;
}
// ---- The stores the "entities" and "variables" sections keep ----
namespace {
bool storeName(std::string_view name){return !name.empty()&&name.size()<=SaveStore::MaxName&&std::none_of(name.begin(),name.end(),[](char c){return static_cast<unsigned char>(c)<32;});}
bool storeValue(const SaveValue& value){if(auto* d=std::get_if<double>(&value))return std::isfinite(*d);if(auto* s=std::get_if<std::string>(&value))return s->size()<=SaveStore::MaxText;return true;}
}
// A save in an older positional format (1..LegacySaveVersion), as that build wrote it: for tests
// that an old save still loads (MapFormatTest.cpp).
std::string Game::legacySaveText(int version){storeChunk();Writer writer;archiveSave(writer,version);auto payload=writer.stream.str();return "RAWMETAL_SAVE "+std::to_string(version)+" "+std::to_string(checksum(payload))+"\n"+payload;}
bool Game::setEntityProperty(int chunk,std::string_view entity,std::string_view property,SaveValue value){
 if(chunk<-1||chunk>=MaxChunks||!storeName(entity)||!storeName(property)||!storeValue(value))return false;
 auto& store=m_saveStore.entities;auto at=store.find(SaveStore::Key{chunk,std::string(entity)});
 if(at==store.end()){if(store.size()>=SaveStore::MaxEntities)return false;at=store.emplace(SaveStore::Key{chunk,std::string(entity)},SaveStore::Properties{}).first;}
 auto& properties=at->second;auto found=properties.find(property);
 if(found!=properties.end()){found->second=std::move(value);return true;}
 if(properties.size()>=SaveStore::MaxProperties){if(properties.empty())store.erase(at);return false;}
 properties.emplace(std::string(property),std::move(value));return true;
}
const SaveStore::Properties* Game::entityProperties(int chunk,std::string_view entity)const{auto at=m_saveStore.entities.find(SaveStore::Key{chunk,std::string(entity)});return at==m_saveStore.entities.end()?nullptr:&at->second;}
const SaveValue* Game::entityProperty(int chunk,std::string_view entity,std::string_view property)const{
 const auto* properties=entityProperties(chunk,entity);if(!properties)return nullptr;auto found=properties->find(property);return found==properties->end()?nullptr:&found->second;}
bool Game::clearEntityProperty(int chunk,std::string_view entity,std::string_view property){
 auto at=m_saveStore.entities.find(SaveStore::Key{chunk,std::string(entity)});if(at==m_saveStore.entities.end())return false;
 auto found=at->second.find(property);if(found==at->second.end())return false;at->second.erase(found);if(at->second.empty())m_saveStore.entities.erase(at);return true;}
bool Game::setMapVariable(int chunk,std::string_view name,SaveValue value){
 if(chunk<-1||chunk>=MaxChunks||!storeName(name)||!storeValue(value))return false;
 auto& store=m_saveStore.variables;auto at=store.find(SaveStore::Key{chunk,std::string(name)});
 if(at!=store.end()){at->second=std::move(value);return true;}
 if(store.size()>=SaveStore::MaxVariables)return false;store.emplace(SaveStore::Key{chunk,std::string(name)},std::move(value));return true;}
const SaveValue* Game::mapVariable(int chunk,std::string_view name)const{auto at=m_saveStore.variables.find(SaveStore::Key{chunk,std::string(name)});return at==m_saveStore.variables.end()?nullptr:&at->second;}
bool Game::clearMapVariable(int chunk,std::string_view name){return m_saveStore.variables.erase(SaveStore::Key{chunk,std::string(name)})>0;}
// From 1.0.4 to 1.5.0 the game asked for L"\RawMetal\saves" and L"\RawMetal\settings.ini": "\R"
// and "\s" are no escapes, so it wrote <LOCALAPPDATA>RawMetalsaves and RawMetalsettings.ini.
// Each file there comes into the right place unless the right place holds one as new; one it
// would replace is kept beside it first as <file>.older.bak (-2, -3... never over a file).
// Copies keep their times, so a second start adopts nothing; the old folder is left as it was.
void Game::adoptMisplacedSaves(const std::wstring& localAppData){
 if(localAppData.empty())return;
 const std::filesystem::path root(localAppData);
 auto adopt=[](const std::filesystem::path& from,const std::filesystem::path& to){
  std::error_code error;if(!std::filesystem::is_regular_file(from,error))return;
  if(std::filesystem::exists(to,error)){
   if(std::filesystem::last_write_time(to,error)>=std::filesystem::last_write_time(from,error)||error)return;
   std::string older;try{older=readFile(to);}catch(const std::exception&){return;}
   bool kept=false;for(int n=1;n<100&&!kept;++n){auto backup=to;backup+=n>1?L".older-"+std::to_wstring(n)+L".bak":std::wstring(L".older.bak");if(!std::filesystem::exists(backup,error))kept=writeNew(backup,older);}
   if(!kept)return;
  }
  std::filesystem::create_directories(to.parent_path(),error);
  if(CopyFileW(from.c_str(),to.c_str(),FALSE))appendLoadLog("adopted "+from.string()+" as "+to.string()+" (the folder older builds wrote by mistake)");
 };
 std::error_code error;const std::filesystem::path wrongSaves(localAppData+L"RawMetalsaves");
 if(std::filesystem::is_directory(wrongSaves,error))for(const auto& entry:std::filesystem::directory_iterator(wrongSaves,error)){
  const auto extension=entry.path().extension();if(extension==L".rms"||extension==L".label"||extension==L".bak")adopt(entry.path(),root/L"RawMetal"/L"saves"/entry.path().filename());}
 adopt(std::filesystem::path(localAppData+L"RawMetalsettings.ini"),root/L"RawMetal"/L"settings.ini");
}
// Earlier builds represented a broken crate as four unrelated prop models (kinds 0-3) spawned
// together: that recognizable quartet becomes the current textured splinters (kind 6).
static void migrateCrateDebris(std::vector<Clutter>&items){std::vector<bool>used(items.size());
 for(size_t i=0;i<items.size();++i){if(used[i]||items[i].kind!=0)continue;size_t group[4]={i};bool complete=true;
  for(int kind=1;kind<4;++kind){float best=1.21f;size_t found=items.size();for(size_t j=0;j<items.size();++j){if(used[j]||int(j)==int(i)||items[j].kind!=kind||std::fabs(items[j].z-items[i].z)>.30f)continue;float d=lengthSq(items[j].pos-items[i].pos);if(d<best){best=d;found=j;}}
   if(found==items.size()){complete=false;break;}group[kind]=found;}
  if(!complete)continue;Vec2 center{};for(auto index:group)center+=items[index].pos*.25f;float spread=0;for(auto index:group)spread=std::max(spread,lengthSq(items[index].pos-center));
  bool outward=items[group[0]].pos.x>center.x+.01f&&items[group[1]].pos.y>center.y+.01f&&items[group[2]].pos.x<center.x-.01f&&items[group[3]].pos.y<center.y-.01f;
  if(spread>.0064f&&!outward)continue;
  for(int part=0;part<4;++part){auto&shard=items[group[part]];float centerZ=shard.z+shard.height()*.5f;shard.kind=6;shard.z=centerZ-shard.height()*.5f;shard.yaw=part*1.5707963f;shard.pitch=part%2?-.14f:.14f;shard.roll=part%2?.10f:-.10f;shard.spin=part%2?2.5f:-2.5f;shard.pitchSpeed=part%2?1.5f:-1.5f;shard.rollSpeed=part%2?-1.5f:1.5f;shard.sleeping=false;shard.restTime=0;if(lengthSq(shard.velocity)<.01f)shard.velocity={std::cos(part*1.5707963f)*1.25f,std::sin(part*1.5707963f)*1.25f};if(std::fabs(shard.vz)<.1f)shard.vz=.75f;used[group[part]]=true;}
 }
}
// ---- Formats 1-27: the upgrade chain ----
// A save of an older format is read by the positional archive, which leaves a field its version
// lacked at the fresh game's value. These steps then bring it to the current form, oldest first:
// each runs when the save predates the version it names. (The authored-map reconciliation in
// prepareLoad runs after them for every save; it also wakes, from version 15, creatures that
// were never disturbed, and gives a save before 8 the authored props by kind.)
void Game::refitLegacySave(int from){
 struct Step {int to;void(*apply)(Game&);};
 const Step steps[]={
  // 2: the shotgun's tube was not saved; it holds what it can of the rounds, not reloading.
  {2,[](Game& g){auto&shells=g.m_player.pool(WeaponId::Shotgun);shells.loaded=std::min(weaponDef(WeaponId::Shotgun).cycle.tubeCapacity,shells.total);g.m_reloadTimer=0;}},
  // 19: the one ammunition pair was the shotgun's. The shotgun is in hand, no switch under way, the
  // 9 mm pool empty, a reload under way the shotgun's; the 9 mm stack takes the first 1x2 cell clear of the others.
  {19,[](Game& g){g.m_equippedWeapon=WeaponId::Shotgun;g.m_switchTarget=WeaponId::Fists;g.m_switchRaise=false;g.m_player.pool(WeaponId::Handgun)={};
   g.m_reloadLength=g.m_reloadTimer>0?playerWeaponDef(WeaponId::Shotgun).cycle.reloadTime:0;g.m_emptyReload=false;
   auto taken=[&](int c){for(int i=0;i<3;++i){int at=g.m_itemCells[size_t(i)],w=i==0?4:i==2?2:1;for(int r=c/6;r<c/6+2;++r)if(r>=at/6&&r<at/6+2&&c%6>=at%6&&c%6<at%6+w)return true;}return false;};
   for(int c=0;c<24;++c)if(!taken(c)){g.m_itemCells[3]=c;break;}}},
  // 20: how far a creature had made the player out was not kept: one on a Chase has.
  {20,[](Game& g){for(auto&chunk:g.m_chunks)for(auto&e:chunk.enemies)e.recognition=e.state==Enemy::State::Chase&&e.awareness>0?1.f:0.f;}},
 };
 for(const auto& step:steps)if(from<step.to)step.apply(*this);
}
bool Game::decodeSave(const std::string& data){
 Game next;if(!prepareLoad(data,next))return false;
 *this=std::move(next);return true;
}
bool Game::prepareLoad(const std::string& data,Game& next)const{
 t_loadFailure.clear();t_skippedSections.clear();
 try {
  if(data.size()>MaxSaveBytes)return refuse("save is larger than 64 MB");auto split=data.find('\n');if(split==std::string::npos)return refuse("no header line (empty or truncated file)");
  std::istringstream header(data.substr(0,split));std::string magic;int version=0;uint32_t hash=0;
  if(!(header>>magic>>version>>hash)||magic!="RAWMETAL_SAVE")return refuse("not a RawMetal save (bad header)");if(version<1)return refuse("invalid save version "+std::to_string(version));if(version>SaveVersion)return refuse("save version "+std::to_string(version)+" is from a newer build (this build reads up to "+std::to_string(SaveVersion)+")");header>>std::ws;if(!header.eof())return refuse("extra text on the header line");
  auto payload=data.substr(split+1);
  next.m_customCampaigns=m_customCampaigns;next.m_customMapDirectory=m_customMapDirectory;
  int archived=0;
  if(version>LegacySaveVersion)archived=next.decodeSections(payload,hash);
  else{
   if(checksum(payload)!=hash)return refuse("checksum mismatch (file damaged or truncated)");
   Reader reader(payload);
   // A save made inside the prologue when it was its own custom world: its map
   // has since moved onto the campaign grid, so the night shift starts over.
   auto oldPrologue=[&]{return next.m_worldId==WorldId::Custom&&next.m_customCampaignKey==loadEmbeddedPrologueCampaign()->key;};
   try{archived=next.archiveSave(reader,version);}catch(const std::exception&){if(!oldPrologue())throw;}
   if(oldPrologue()){next=Game();next.startNewGame();adoptSession(next);return true;}
   reader.stream>>std::ws;if(!reader.stream.eof())return refuse("unread data after the version "+std::to_string(version)+" archive");
   next.refitLegacySave(version);
  }
  next.m_loadedSaveVersion=version;
  // Every load, as before the sections: a broken crate's old quartet of junk props becomes splinters.
  for(auto&chunk:next.m_chunks)migrateCrateDebris(chunk.clutter);
  for(int level=0;level<next.chunkCount();++level)next.restoreCrates(next.m_chunks[level].world,level);
  if(next.chunkCount()>3&&next.m_chunks[3].world.hasLift()){
   auto reactorStage=next.m_chunks[3].world.reactorStage();if(reactorStage==World::ReactorStage::DiskHeld&&!next.hasQuestItem(ReactorAuthDisk))next.giveQuestItem(ReactorAuthDisk);if(next.m_chunks[3].world.controlReleased())next.setState(stateId("reactor_bulkhead_released"),1);
  }

  // Saves store mutable gameplay state, but the executable owns the current
  // authored population. Reconcile old state onto today's baseline so content
  // added by an update appears in existing saves instead of being erased by
  // the older serialized vector lengths.
  next.seedScripts();Game authored=next.m_worldId==WorldId::Custom?Game(next.m_customCampaign):Game(next.m_worldId);
  auto samePosition=[](Vec2 a,Vec2 b){return lengthSq(a-b)<.0004f;};
  // Homes are in world XY, independent of which chunk currently owns a prop.
  // Snapshot the saved identities before reconciliation mutates any list.
  std::vector<Clutter> savedProps;for(int l=0;l<next.chunkCount();++l)for(const auto&c:next.m_chunks[l].clutter)if(c.model>=0&&c.authored)savedProps.push_back(c);
  auto sameProp=[&](const Clutter&a,const Clutter&b){return a.model==b.model&&samePosition(a.home,b.home)&&std::fabs(a.homeZ-b.homeZ)<.05f;};
  auto freshProp=[&](const Clutter&c)->const Clutter*{for(int l=0;l<authored.chunkCount();++l)for(const auto&spawn:authored.m_chunks[l].clutter)if(spawn.model>=0&&spawn.authored&&sameProp(spawn,c))return &spawn;return nullptr;};
  auto hadSavedProp=[&](const Clutter&c){return std::any_of(savedProps.begin(),savedProps.end(),[&](const Clutter&v){return sameProp(c,v);});};
  for(int level=0;level<next.chunkCount();++level){
   auto&saved=next.m_chunks[level];const auto&fresh=authored.m_chunks[level];
   // Chunks the save never carried (older saves, or maps a custom pack gained
   // since) are today's authored chunks, not the default Game's.
   if(level>=archived){saved=fresh;continue;}

   auto oldEnemies=std::move(saved.enemies);std::vector<bool> enemyUsed(oldEnemies.size(),false);saved.enemies.clear();saved.enemies.reserve(fresh.enemies.size());
   // An authored creature that followed the player to another chunk is matched
   // there: its spawn here stays empty instead of coming back at full health.
   auto followedElsewhere=[&](const Enemy&spawn){for(int other=0;other<next.chunkCount();++other){const auto&list=other==level?oldEnemies:next.m_chunks[other].enemies;
    for(const auto&enemy:list)if(enemy.originLevel==level&&enemy.kind==spawn.kind&&samePosition(enemy.originHome,spawn.home))return true;}return false;};
   for(const auto&spawn:fresh.enemies){
    int match=-1;for(int i=0;i<int(oldEnemies.size());++i)if(!enemyUsed[i]&&!oldEnemies[i].scriptSpawned&&oldEnemies[i].originLevel<0&&oldEnemies[i].kind==spawn.kind&&samePosition(oldEnemies[i].home,spawn.home)){match=i;break;}
    if(match>=0){enemyUsed[match]=true;auto state=oldEnemies[match];state.maxHp=spawn.maxHp;state.scale=spawn.scale;state.hunter=spawn.hunter;state.prey=spawn.prey;if(state.alive)state.hp=std::min(state.hp,state.maxHp);
     if(state.homeZ<=-999)state.homeZ=spawn.homeZ;
     state.adoptTraits(spawn); // map format 2 overrides are the map's, not the save's
     // Saves before dormancy kept no wake state: an untouched creature sleeps again.
     if(version<15&&state.alive&&state.hp>=state.maxHp&&state.awareness<=0)state.wakeState=spawn.wakeState;
     saved.enemies.push_back(state);}
    else if(!followedElsewhere(spawn))saved.enemies.push_back(spawn);
   }

   // Dynamic waves retain both living and dead members instead of vanishing
   // when content updates reconcile the authored population; so do creatures
   // that crossed into this chunk from another.
   for(const auto&enemy:oldEnemies)if(enemy.scriptSpawned||enemy.originLevel>=0)saved.enemies.push_back(enemy);

   auto oldPickups=std::move(saved.pickups);std::vector<bool> pickupUsed(oldPickups.size(),false);saved.pickups.clear();saved.pickups.reserve(fresh.pickups.size());
   for(const auto&spawn:fresh.pickups){
    int match=-1;for(int i=0;i<int(oldPickups.size());++i)if(!pickupUsed[i]&&oldPickups[i].kind==spawn.kind&&samePosition(oldPickups[i].pos,spawn.pos)){match=i;break;}
    // Elevation belongs to authored geometry, while collection belongs to the
    // save. This also upgrades old saves without changing the save format.
    auto pickup=spawn;
    if(match>=0){pickupUsed[match]=true;pickup.active=oldPickups[match].active;}
    saved.pickups.push_back(pickup);
   }

   // Current saves own their dynamic clutter population, including imported objects.
   if(version>=8){
    int held=level==next.m_level?next.m_heldClutter:-1,newHeld=-1;std::vector<Clutter> reconciled;
    for(int i=0;i<int(saved.clutter.size());++i){auto c=saved.clutter[i];bool keep=c.model<0||!c.authored||i==held;
     if(c.model>=0&&c.authored)if(const auto*spawn=freshProp(c)){c.breakState=spawn->breakState;c.traits=spawn->traits;keep=true;}
     if(keep){if(i==held)newHeld=int(reconciled.size());reconciled.push_back(c);}
    }
    for(const auto&spawn:fresh.clutter)if(spawn.model>=0&&!hadSavedProp(spawn))reconciled.push_back(spawn);
    saved.clutter=std::move(reconciled);if(level==next.m_level)next.m_heldClutter=newHeld;continue;
   }
   int heldOld=level==next.m_level?next.m_heldClutter:-1,heldNew=-1;
   auto oldClutter=std::move(saved.clutter);std::vector<bool> clutterUsed(oldClutter.size(),false);saved.clutter.clear();saved.clutter.reserve(std::max(fresh.clutter.size(),oldClutter.size()));
   for(const auto&spawn:fresh.clutter){
    int match=-1;for(int i=0;i<int(oldClutter.size());++i)if(!clutterUsed[i]&&oldClutter[i].kind==spawn.kind){match=i;break;}
    if(match>=0){clutterUsed[match]=true;if(match==heldOld)heldNew=int(saved.clutter.size());saved.clutter.push_back(oldClutter[match]);}
    else saved.clutter.push_back(spawn);
   }
   if(level==next.m_level)next.m_heldClutter=heldNew;
  }
  auto&p=next.m_player;
  // Each firearm's pool against its own magazine; the handgun's only when it is owned or its 9 mm waits for it.
  for(auto id:{WeaponId::Shotgun,WeaponId::Handgun}){const auto&rounds=p.pool(id);if(rounds.total<0||rounds.total>9999||rounds.loaded<0||rounds.loaded>playerWeaponDef(id).cycle.tubeCapacity||rounds.loaded>rounds.total)return refuse("invalid ammunition pool");}
  if(p.pool(WeaponId::Fists)!=AmmoPool{}||(p.pool(WeaponId::Handgun).loaded>0&&!next.hasHandgun())||!next.ownsWeapon(next.m_equippedWeapon)||!next.ownsWeapon(next.m_switchTarget)||next.m_reloadLength<0||next.m_reloadLength>2||next.m_reloadTimer>next.m_reloadLength+.001f)return refuse("invalid weapon or reload state");
  // A body saved somewhere no body can be (past the map's edge, far above or below it, as a
  // noclip flight or a glitch leaves it) is set back on the map's own start rather than refusing
  // the whole save; the camera's eye and pitch come back into range the same way.
  if(next.m_level>=0&&next.m_level<next.chunkCount()&&(p.pos.x<-2||p.pos.x>26||p.pos.y<-2||p.pos.y>26||p.z<-100||p.z>100)){
   const auto& d=next.m_chunks[next.m_level].world.definition();p.pos=d.playerStart;p.z=d.spawnHeight;p.verticalVelocity=0;p.grounded=true;next.m_velocity={};
   try{std::ofstream log("RawMetal-load.txt",std::ios::app);log<<"load: the player was saved off map "<<next.m_level<<"; set at its start\n";}catch(...){}}
  p.eye=std::clamp(p.eye,.1f,2.6f);p.pitch=std::clamp(p.pitch,-220.f,220.f);p.health=std::min(p.health,100.f);
  if(next.m_level<0||next.m_level>=next.chunkCount()||next.m_elapsed<0||next.m_reloadTimer<0||next.m_reloadTimer>8||p.health>100||p.pos.x<-2||p.pos.x>26||p.pos.y<-2||p.pos.y>26||p.z<-100||p.z>100||p.eye<.1f||p.eye>2.6f||std::fabs(p.pitch)>220.f||next.m_medkits<0)return refuse("player or map out of range (map "+std::to_string(next.m_level)+", health "+std::to_string(p.health)+", at "+std::to_string(p.pos.x)+","+std::to_string(p.pos.y)+","+std::to_string(p.z)+")");
  for(int i=0;i<int(next.m_itemCells.size());++i){int cell=next.m_itemCells[size_t(i)],width=i==0?4:i==2?2:1;if(cell<0||cell/6+2>5||cell%6+width>6)return refuse("invalid inventory cell");}
  next.ensureChunk(next.m_level);auto&c=next.m_chunks[next.m_level];next.m_world=c.world;next.m_enemies=c.enemies;next.m_pickups=c.pickups;next.m_clutter=c.clutter;next.m_kills=c.kills;
  if(next.m_world.hasLift()&&!next.m_hazmat.initialized)next.m_hazmat.seed(next.m_world);
  next.updateHungSuits(0);
  if(next.m_heldClutter<-1||next.m_heldClutter>=int(next.m_clutter.size())||(next.m_heldClutter>=0&&next.m_clutter[next.m_heldClutter].broken))return refuse("held prop index invalid or broken");
  adoptSession(next);
  next.updateStreaming(0);
  for(int level=0;level<next.chunkCount();++level)if(next.chunkResident(level))next.settleLeads(level);
  next.rebuildFriendlyActors();
  int heldCount=0;
  for(size_t actor=0;actor<next.m_friendlyActors.size();++actor){const auto&npc=next.m_friendlyActors[actor];
   if(next.npcIncapacitated(actor)&&npc.track().deadState&&next.state(npc.track().deadState))return refuse("NPC "+std::to_string(actor)+" is both restrained and dead");
   if(next.npcHeld(actor)&&(++heldCount>1||npc.level!=next.m_level))return refuse("NPC "+std::to_string(actor)+" held off the current map or more than one held");
  }
  next.validateTacticalGrenades();
  next.validateEngineerDrones();
  next.updateMechanisms(0);next.updateBreakables();
  // Ashfall saves from the flat-ground prototype keep their chunk-local X/Y,
  // but grounded feet now belong on the generated terrain surface.
  if(next.m_world.outdoors()&&p.grounded){
   p.z=next.m_world.floorHeight(p.pos.x,p.pos.y);p.verticalVelocity=0;
  }
  // Old megamap builds had non-colliding partitions. Only relocate a saved
  // player if the current authored geometry now overlaps their complete hull.
  // A player saved shut in a locker stays in it, half inside its steel,
  // unless an update has since blocked every way out: then they go free and
  // are set somewhere clear like anyone else.
  if(const int locker=next.hiddenLocker();locker>=0){Vec2 exit;float feet;if(!next.lockerExit(next.m_world.fixtures()[size_t(locker)],exit,feet,true))next.forgetLocker();}
  // A creature or a man pressed against the player is not geometry: only walls and props move him.
  // The clear place is looked for on the floor and on the decks and slabs near his own height (maps
  // 107-109 stand their floors on slabs over a deep layer, where the layer's floor is solid
  // everywhere: looking only there refused every such save). Nothing clear: the map's own start.
  if(next.m_world.campaign()&&next.m_level>=4&&next.hiddenLocker()<0&&!next.hullFits(p.pos,p.z,p.hullHeight(),.2f,false)){
   Vec2 safe{};float safeZ=0,best=std::numeric_limits<float>::max();
   for(float y=1.25f;y<23.75f;y+=.25f)for(float x=1.25f;x<23;x+=.25f){
    Vec2 candidate{x,y};const float planar=lengthSq(candidate-p.pos);if(planar>=best)continue;
    for(float z:{next.m_world.supportBelow(x,y,p.z+1.f),next.m_world.supportBelow(x,y,p.z+.3f),next.m_world.floorHeight(x,y)}){
     const float distance=planar+(z-p.z)*(z-p.z);
     if(std::isfinite(z)&&distance<best&&next.hullFits(candidate,z,p.hullHeight(),.2f,false)){safe=candidate;safeZ=z;best=distance;}}
   }
   if(best==std::numeric_limits<float>::max()){const auto& d=next.m_world.definition();safe=d.playerStart;safeZ=d.spawnHeight;
    try{std::ofstream log("RawMetal-load.txt",std::ios::app);log<<"load: no clear place near the saved player on map "<<next.m_level<<"; set at its start\n";}catch(...){}}
   p.pos=safe;p.z=safeZ;p.verticalVelocity=0;p.grounded=true;next.m_velocity={};
  }
  return true;
 }catch(const std::exception& error){return refuse(error.what());}
}
// What a loaded game keeps of the session that loads it: settings, the save
// folder and engine switches, and a fresh render revision.
void Game::adoptSession(Game& next)const{
 next.m_settings=m_settings;next.m_audioMuted=m_audioMuted;next.m_musicEnabled=m_musicEnabled;next.m_showFps=m_showFps;next.m_renderScale=m_renderScale;next.m_saveDirectory=m_saveDirectory;
 next.m_sessionRevision=m_sessionRevision+1;next.m_suppressFire=true;next.m_previousUse=true;next.m_previousSubdue=true;next.m_previousJump=true;next.m_previousEscape=true;next.m_menuPage=MenuPage::Settings;
 next.m_aiQueryCaching=m_aiQueryCaching;next.m_aiDecisionScheduling=m_aiDecisionScheduling;next.m_asyncLoads=m_asyncLoads;
 next.m_customCampaigns=m_customCampaigns;next.m_customMapDirectory=m_customMapDirectory;
}
// A save being built on a worker. The worker only writes `result`, `ok` and the
// counters; the main thread reads them after `done`, then joins.
struct LoadJob {
 std::thread worker;std::atomic<bool> done{false};std::atomic<int> progress{0};int expected=1,slot=-1;bool ok=false;std::optional<Game> result;std::string failure;std::filesystem::path path;
 Game::SaveRefit refit;
 ~LoadJob(){if(worker.joinable())worker.join();}
};
static std::string saveLabel(int level,float elapsed,float health){return "MAP "+std::to_string(level)+" / "+std::to_string(int(elapsed/60))+" MIN / HP "+std::to_string(int(health));}
static std::filesystem::path labelPath(const std::filesystem::path& save){auto path=save;path+=".label";return path;}
// The refit of an older save, run on the game just loaded from it (on the load's own thread).
// The original is kept beside it first, as <file>.v<old>.bak, or <file>.v<old>-2.bak and on when
// that name holds something else: a backup is never written over. Then the game is written in the
// current format, but only once that form has been loaded back; the swap is atomic. Anything
// failing leaves the original file in place, and the loaded game is never touched.
Game::SaveRefit Game::refitSaveFile(const std::filesystem::path& path,const std::string& original)const{
 SaveRefit refit;refit.from=m_loadedSaveVersion;refit.skipped=t_skippedSections;
 if(refit.from<=0||refit.from>=SaveVersion)return refit;
 try{
  const auto stem=path.filename().wstring()+L".v"+std::to_wstring(refit.from);
  for(int n=1;n<100&&refit.backup.empty();++n){
   const auto candidate=path.parent_path()/(stem+(n>1?L"-"+std::to_wstring(n):L"")+L".bak");std::error_code error;
   if(std::filesystem::exists(candidate,error)){try{if(readFile(candidate)==original)refit.backup=candidate;}catch(const std::exception&){}continue;}
   if(error)break;
   if(writeNew(candidate,original))refit.backup=candidate;else break;
  }
  if(refit.backup.empty()){refit.note="no backup could be written beside it";return refit;}
  const auto current=encodeSave();
  Game probe;if(!prepareLoad(current,probe)){refit.note="the rewritten save did not load back ("+t_loadFailure+")";t_loadFailure.clear();return refit;}
  writeAtomic(path,current);refit.rewritten=true;
  try{writeAtomic(labelPath(path),saveLabel(m_level,m_elapsed,m_player.health));}catch(const std::exception&){}
 }catch(const std::exception& error){refit.note=error.what();}
 return refit;
}
// After a load from a file: RawMetal-load.txt and the HUD notice say what the refit did.
void Game::noteRefit(const std::filesystem::path& path,const SaveRefit& refit){
 if(!refit.skipped.empty()){std::string names;for(const auto& s:refit.skipped)names+=(names.empty()?"":", ")+s;appendLoadLog("load: "+path.string()+" has sections this build does not read, skipped: "+names);}
 if(refit.from<=0||refit.from>=SaveVersion)return;
 if(refit.rewritten){
  appendLoadLog("save refit: "+path.string()+" was format "+std::to_string(refit.from)+", rewritten as format "+std::to_string(SaveVersion)+"; the original is kept as "+refit.backup.filename().string());
  m_pickupNotice="SAVE UPDATED TO THE NEW FORMAT / OLD COPY KEPT";m_pickupNoticeTime=5;
 }else{
  appendLoadLog("save refit skipped: "+path.string()+" (format "+std::to_string(refit.from)+"): "+(refit.note.empty()?std::string("unknown error"):refit.note)+". It loaded as it is and stays in its old format"+(refit.backup.empty()?std::string():"; a copy is kept as "+refit.backup.filename().string()));
  m_pickupNotice="OLD SAVE LOADED / NOT UPDATED (SEE RAWMETAL-LOAD.TXT)";m_pickupNoticeTime=5;
 }
}
bool Game::loadReady()const{return m_loadJob&&m_loadJob->done.load();}
float Game::loadProgress()const{if(!m_loadJob)return 1.f;return std::clamp(float(m_loadJob->progress.load())/float(m_loadJob->expected),0.f,.99f);}
bool Game::beginLoad(std::string data,int slot){
 if(m_loadJob)return false;
 auto job=std::make_shared<LoadJob>();job->slot=slot;job->path=slot>=0?slotPath(m_saveDirectory,slot):checkpointPath(m_saveDirectory);job->expected=std::max(1,chunkCount()*3);
 // The source session stays still while the worker reads its settings: update()
 // does nothing else until the job is done.
 job->worker=std::thread([job,this,data=std::move(data)]{loadProgressCounter=&job->progress;
  try{job->result.emplace();job->ok=prepareLoad(data,*job->result);if(!job->ok)job->failure=t_loadFailure;}catch(const std::exception& error){job->ok=false;job->failure=error.what();}catch(...){job->ok=false;job->failure="unknown error";}
  loadProgressCounter=nullptr;
  // An older save is refitted here, on the worker, while the loading bar still shows.
  if(job->ok)try{job->refit=job->result->refitSaveFile(job->path,data);}catch(...){}
  job->done.store(true);});
 m_loadJob=std::move(job);m_menuMessage.clear();return true;
}
void Game::finishLoad(){
 auto job=std::move(m_loadJob);m_loadJob.reset();if(job->worker.joinable())job->worker.join();
 if(job->ok){*this=std::move(*job->result);noteRefit(job->path,job->refit);return;}
 logLoadFailure(job->path,job->failure);
 if(job->slot>=0){m_menuMessage=loadFailureLine("LOAD FAILED",job->failure);m_menuPage=MenuPage::Load;m_menuSelection=job->slot;}
 else m_menuMessage=loadFailureLine("AUTOSAVE FAILED",job->failure);
}
const std::string& Game::lastLoadFailure(){return t_loadFailure;}
bool Game::saveSlot(int slot){
 if(slot<0||slot>=3||m_saveDirectory.empty()){m_menuMessage="SAVE LOCATION UNAVAILABLE";return false;}
 try {writeAtomic(slotPath(m_saveDirectory,slot),encodeSave());try{writeAtomic(labelPath(slotPath(m_saveDirectory,slot)),saveLabel(m_level,m_elapsed,m_player.health));}catch(const std::exception&){}refreshSaveSlots();m_menuMessage="GAME SAVED";return true;}catch(const std::exception&){m_menuMessage="SAVE FAILED / CHECK DISK AND FOLDER";return false;}
}
bool Game::saveCheckpoint(){if(m_saveDirectory.empty())return false;try{writeAtomic(checkpointPath(m_saveDirectory),encodeSave());try{writeAtomic(labelPath(checkpointPath(m_saveDirectory)),saveLabel(m_level,m_elapsed,m_player.health));}catch(const std::exception&){}refreshSaveSlots();return true;}catch(const std::exception&){return false;}}
bool Game::loadCheckpoint(){if(m_saveDirectory.empty()){m_menuMessage="AUTOSAVE UNAVAILABLE";return false;}
 const auto path=checkpointPath(m_saveDirectory);std::string why;
 try{auto data=readFile(path);if(m_asyncLoads)return beginLoad(std::move(data),-1);if(decodeSave(data)){noteRefit(path,refitSaveFile(path,data));return true;}why=t_loadFailure;}catch(const std::exception& error){why=error.what();}
 logLoadFailure(path,why);m_menuMessage=loadFailureLine("AUTOSAVE FAILED",why);return false;}
bool Game::loadSlot(int slot){
 if(slot<0||slot>=3||m_saveDirectory.empty()){m_menuMessage="SAVE LOCATION UNAVAILABLE";return false;}
 const auto path=slotPath(m_saveDirectory,slot);std::string why;std::error_code missing;
 if(!std::filesystem::exists(path,missing)){m_menuMessage="LOAD FAILED / EMPTY SLOT";return false;}
 try{auto data=readFile(path);if(m_asyncLoads)return beginLoad(std::move(data),slot);if(decodeSave(data)){noteRefit(path,refitSaveFile(path,data));return true;}why=t_loadFailure;}catch(const std::exception& error){why=error.what();}
 logLoadFailure(path,why);m_menuMessage=loadFailureLine("LOAD FAILED",why);return false;
}
// What a save's menu row says: its sidecar label when it is newer than the
// save; otherwise (a save from before sidecars) one full decode, after which the
// sidecar is written so it never happens again.
std::string Game::savedLabel(const std::filesystem::path& path,const char* invalid)const{
 std::error_code error;auto label=labelPath(path);
 if(std::filesystem::exists(label,error)&&std::filesystem::last_write_time(label,error)>=std::filesystem::last_write_time(path,error)&&!error)
  try{auto text=readFile(label);if(!text.empty()&&text.size()<64)return text;}catch(const std::exception&){}
 try{Game preview;if(prepareLoad(readFile(path),preview)){auto text=saveLabel(preview.level(),preview.elapsed(),preview.player().health);try{writeAtomic(label,text);}catch(const std::exception&){}return text;}return invalid;}catch(const std::exception&){return "UNREADABLE";}
}
void Game::refreshSaveSlots(){
 for(int slot=0;slot<3;++slot){auto&label=m_slotLabels[slot];label="SLOT "+std::to_string(slot+1)+" / ";
  if(m_saveDirectory.empty()){label+="UNAVAILABLE";continue;}
  auto path=slotPath(m_saveDirectory,slot);std::error_code error;if(!std::filesystem::exists(path,error)){label+=error?"UNAVAILABLE":"EMPTY";continue;}
  label+=savedLabel(path,"INVALID SAVE");
 }
 m_checkpointLabel="AUTOSAVE / ";
 if(m_saveDirectory.empty())m_checkpointLabel+="UNAVAILABLE";
 else {auto path=checkpointPath(m_saveDirectory);std::error_code error;if(!std::filesystem::exists(path,error))m_checkpointLabel+=error?"UNAVAILABLE":"EMPTY";else m_checkpointLabel+=savedLabel(path,"INVALID");}
}
bool Game::testEngineRegressionSaves(std::ostream& report){
 bool passed=true;
 auto check=[&](bool ok,const char* name,const std::string& v){report<<(ok?"PASS ":"FAIL ")<<name<<": "<<v<<'\n';report.flush();passed&=ok;};
 auto approx=[](float a,float b){return std::fabs(a-b)<.001f;};
 for(auto kind:{Enemy::Kind::Warden,Enemy::Kind::Snatcher}){
  auto m=regression::room();m->creatureSpawns.push_back({kind,{9,9},0});Game g(regression::campaign(m));
  auto&e=g.m_enemies[0];g.applyEnemyDamage(e,e.maxHp*.7f,false,false);bool retreat=e.stalkMode==Enemy::StalkMode::Retreat;
  g.applyEnemyDamage(e,1000,false,false);Game loaded(g.m_customCampaign);bool ok=loaded.decodeSave(g.encodeSave());
  int mode=ok&&!loaded.m_enemies.empty()?int(loaded.m_enemies[0].stalkMode):-1;bool dead=ok&&!loaded.m_enemies.empty()&&!loaded.m_enemies[0].alive;
  check(retreat&&ok&&dead&&mode==int(Enemy::StalkMode::Watch),"1a retreat corpse save",std::string(e.name())+" retreat="+std::to_string(retreat)+" decoded="+std::to_string(ok)+" dead="+std::to_string(dead)+" mode="+std::to_string(mode));
 }
 {auto m=regression::room();m->layers[0].rows[9][9]='C';Game g(regression::campaign(m));char before=g.m_world.tile(9,9);g.m_world.destroyTile(9,9);
  Game loaded(g.m_customCampaign);bool ok=loaded.decodeSave(g.encodeSave());char saved=ok?loaded.m_world.tile(9,9):'?';
  if(ok){loaded.storeChunk();loaded.m_chunks[0].world.unloadGeometry();loaded.m_chunks[0].resident=false;loaded.ensureChunk(0);}
  char rebuilt=ok?loaded.m_chunks[0].world.tile(9,9):'?';
  check(before=='C'&&saved=='.'&&rebuilt=='.',"1b destroyed crate load and rebuild",std::string("before=")+before+" decoded="+std::to_string(ok)+" loaded="+saved+" rebuilt="+rebuilt);
 }
 {Game g(regression::campaign(regression::room()));g.m_player.eye=2.2f;g.m_player.pitch=219;Game loaded(g.m_customCampaign);bool ok=loaded.decodeSave(g.encodeSave());
  check(ok&&approx(loaded.m_player.eye,2.2f)&&approx(loaded.m_player.pitch,219),"1c VR pose save","decoded="+std::to_string(ok)+" eye="+std::to_string(loaded.m_player.eye)+" pitch="+std::to_string(loaded.m_player.pitch));
 }
 {Game g(regression::campaign(regression::room()));g.m_player.health=63;g.storeChunk();Writer writer;g.archiveSave(writer,15);auto payload=writer.stream.str();Game loaded(g.m_customCampaign);
  bool ok=loaded.decodeSave("RAWMETAL_SAVE 15 "+std::to_string(checksum(payload))+"\n"+payload);
  check(ok&&loaded.m_player.health==63,"1d version 15 archive","decoded="+std::to_string(ok)+" health="+std::to_string(loaded.m_player.health));
 }
 {Game g(regression::campaign(regression::room()));CreatureSpawn s{Enemy::Kind::Huntsman,{9,9},0};s.scale=1.5f;g.spawnCreature(s,0,false);
  auto&e=g.m_enemies.back();e.scriptSpawned=true;e.homeZ=3.25f;e.originLevel=0;e.originHome={7.25f,8.75f};e.surfaceMode=2;e.surfaceNormal={.6f,.8f};
  Game loaded(g.m_customCampaign);bool ok=loaded.decodeSave(g.encodeSave());Enemy v{};int count=0;if(ok)for(const auto&x:loaded.m_enemies)if(x.scriptSpawned){v=x;++count;}
  std::ostringstream values;values<<"decoded="<<ok<<" count="<<count<<" homeZ="<<v.homeZ<<" scale="<<v.scale<<" origin="<<v.originLevel<<","<<v.originHome.x<<","<<v.originHome.y<<" surface="<<v.surfaceMode<<","<<v.surfaceNormal.x<<","<<v.surfaceNormal.y;
  check(ok&&count==1&&approx(v.homeZ,3.25f)&&approx(v.scale,1.5f)&&v.originLevel==0&&approx(v.originHome.x,7.25f)&&approx(v.originHome.y,8.75f)&&v.surfaceMode==2&&approx(v.surfaceNormal.x,.6f)&&approx(v.surfaceNormal.y,.8f),"1e enemy fields save",values.str());
 }
 // How far each creature has made the player out: one searching that had, one
 // that has only glimpsed, one chasing an NPC with the player half made out.
 // A version 19 save rebuilds it from the hunt (the Chase alone).
 {Game g(regression::campaign(regression::room()));const std::pair<Enemy::State,float> held[]={{Enemy::State::Search,1.f},{Enemy::State::Investigate,.4f},{Enemy::State::Chase,.25f}};
  for(int i=0;i<3;++i){g.spawnCreature({Enemy::Kind::Huntsman,{6.f+3*i,9},0},5,false);auto&e=g.m_enemies.back();e.scriptSpawned=true;e.state=held[i].first;e.recognition=held[i].second;}
  auto meters=[](const Game&l){std::array<float,3> r{-1,-1,-1};for(const auto&x:l.m_enemies)for(int i=0;i<3;++i)if(x.scriptSpawned&&lengthSq(x.home-Vec2{6.f+3*i,9})<.0001f)r[size_t(i)]=x.recognition;return r;};
  Game loaded(g.m_customCampaign);bool ok=loaded.decodeSave(g.encodeSave());const auto kept=meters(loaded);
  g.storeChunk();Writer writer;g.archiveSave(writer,19);auto payload=writer.stream.str();Game legacy(g.m_customCampaign);bool old=legacy.decodeSave("RAWMETAL_SAVE 19 "+std::to_string(checksum(payload))+"\n"+payload);const auto rebuilt=meters(legacy);
  std::ostringstream values;values<<"decoded="<<ok<<" search/investigate/chase kept="<<kept[0]<<'/'<<kept[1]<<'/'<<kept[2]<<"; version 19 decoded="<<old<<" rebuilt="<<rebuilt[0]<<'/'<<rebuilt[1]<<'/'<<rebuilt[2];
  check(ok&&approx(kept[0],1)&&approx(kept[1],.4f)&&approx(kept[2],.25f)&&old&&rebuilt[0]==0&&rebuilt[1]==0&&rebuilt[2]==1,"1f creature recognition save",values.str());
 }
 return passed;
}
bool Game::testSaves(){
 std::ofstream report("save-test.txt");auto check=[&](bool ok,const char* label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();return ok;};
 for(float pitch:{-210.f,210.f}){
  Game original,restored;original.m_player.pitch=pitch;
  if(!check(restored.decodeSave(original.encodeSave())&&std::fabs(restored.player().pitch-pitch)<.001f,"Full vertical camera range survives save/load"))return false;
 }
 for(float time:{0.f,7.f,21.f,34.f,39.5f,42.f,48.f}){
  auto original=liftInspection(time);original.m_player.health=63;original.m_player.pool(WeaponId::Shotgun).total=17;original.m_medkits=2;original.m_weaponEquipped=false;original.m_world.setDoor(0,.35f,true);
  // liftInspection() deliberately removes enemies from every chunk, so do not
  // index the stripped enemy vectors here. Enemy death persistence is covered
  // by the authored-content migration regression below.
  original.m_chunks[1].pickups[0].active=false;original.m_chunks[1].kills=1;
  if(!original.m_enemies.empty()){auto&stalker=original.m_enemies.back();if(stalker.kind==Enemy::Kind::Warden){stalker.stalkMode=Enemy::StalkMode::Flank;stalker.stalkTimer=.73f;stalker.stalkSide=-1.f;}}
  if(time==48){original.m_world.takeReactorDisk();original.m_world.useReactorTerminal(1);original.m_world.useReactorTerminal(2);}
  auto data=original.encodeSave();Game restored;restored.m_settings.master=.4f;
  // liftInspection() is intentionally scenery-only and strips authored enemies.
  // Loading now reconciles current authored content, so byte-for-byte equality is
  // not expected for this synthetic save. Verify the mutable state that this
  // test actually owns instead.
  if(!check(restored.decodeSave(data)&&restored.settings().master==.4f&&
      restored.player().health==original.player().health&&restored.player().pools==original.player().pools&&
      restored.medkits()==original.medkits()&&restored.weaponEquipped()==original.weaponEquipped()&&
      restored.world().liftPhase()==original.world().liftPhase()&&
      std::fabs(restored.world().liftHeight()-original.world().liftHeight())<.0001f,
      "Dynamic-state roundtrip / settings preserved"))return false;
  auto phase=original.world().liftPhase();restored.update({},.01f);original.update({},.01f);
  if(!check(restored.world().liftPhase()==original.world().liftPhase()&&std::fabs(restored.player().z-original.player().z)<.0001f,"Loaded lift resumes without moving the passenger incorrectly"))return false;
  if(time==39.5f&&!check(phase==World::LiftPhase::Caught,"Brake-catch save is covered"))return false;
  if(time==48){restored.m_world.useReactorTerminal(3);restored.m_world.useReactorTerminal(1);if(!check(restored.world().controlReleased(),"Loaded reactor puzzle can finish"))return false;}
 }
 // A normal current-build save with the complete authored population should
 // still be canonical and re-encode exactly.
 {Game current;auto data=current.encodeSave();Game restored;
  if(!check(restored.decodeSave(data)&&restored.encodeSave()==data,"Current authored save remains byte-exact"))return false;
 }
 {Game current(loadEmbeddedExclusionZoneCampaign());auto data=current.encodeSave();Game restored(current.m_customCampaign);
  bool decoded=restored.decodeSave(data);auto canonical=decoded?restored.encodeSave():std::string{};
  if(!decoded||canonical!=data){
   report<<"Exclusion canonical probe decoded="<<decoded<<" original_bytes="<<data.size()<<" restored_bytes="<<canonical.size()<<" original_states="<<current.m_states.size()<<" restored_states="<<restored.m_states.size()<<'\n';
   if(decoded){std::istringstream originalLines(data.substr(data.find('\n')+1)),restoredLines(canonical.substr(canonical.find('\n')+1));std::string originalLine,restoredLine;int line=0;
    while(std::getline(originalLines,originalLine)&&std::getline(restoredLines,restoredLine)){++line;if(originalLine==restoredLine)continue;report<<"First differing payload line="<<line<<" original="<<originalLine.substr(0,400)<<" restored="<<restoredLine.substr(0,400)<<'\n';break;}
    report<<"Original player="<<current.player().pos.x<<','<<current.player().pos.y<<','<<current.player().z<<" restored player="<<restored.player().pos.x<<','<<restored.player().pos.y<<','<<restored.player().z<<'\n';
   }
  }
  if(!check(decoded&&canonical==data,"Fresh embedded Exclusion save remains byte-exact"))return false;
  Game again(current.m_customCampaign);if(!check(again.decodeSave(restored.encodeSave())&&again.encodeSave()==data,"Repeated Exclusion restoration does not add machinery or NPC state"))return false;
  size_t heavy=current.m_friendlyActors.size();for(size_t actor=0;actor<current.m_friendlyActors.size();++actor)if(current.m_friendlyActors[actor].track().ai.role==GuardSquadRole::Heavy){heavy=actor;break;}
  if(!check(heavy<current.m_friendlyActors.size(),"Exclusion contains its authored Heavy for signed-pi save coverage"))return false;
  for(int endpoint:{-31416,31416}){auto source=current;source.setState(source.friendlyKey(heavy,"yaw"),endpoint);auto bytes=source.encodeSave();Game first(current.m_customCampaign),second(current.m_customCampaign);
   if(!check(first.decodeSave(bytes)&&first.encodeSave()==bytes&&second.decodeSave(first.encodeSave())&&second.encodeSave()==bytes,"Both saved signed-pi NPC endpoints remain byte-exact over repeated restoration"))return false;
  }
  bool rejects=true;
  for(int invalid:{0,1,2}){auto malformed=current;malformed.setState(malformed.friendlyKey(0,"restraint"),2);
   if(invalid==0)malformed.setState(malformed.friendlyKey(1,"restraint"),2);
   else if(invalid==1)malformed.setState(malformed.friendlyKey(0,"chunk"),2);
   else malformed.setState(malformed.npcActors()[0].track().deadState,1);
   Game rejected(current.m_customCampaign);rejects&=!rejected.decodeSave(malformed.encodeSave());
  }
  if(!check(rejects,"Multiple held guards, a remote held guard and dead restraint identities are rejected"))return false;
 }
 // Zero-time initialization restores machinery/poses, but one-shot authored
 // script events still belong to the live script pass, exactly once.
 {auto campaign=std::make_shared<CustomCampaign>();campaign->name="Save initialization fixture";campaign->key=883017;
  auto map=std::make_shared<AuthoredMapData>();map->name="Script room";map->definition={{0,0},{6.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence sequence;sequence.timerState=stateId("save_init_clock");sequence.finishState=stateId("save_init_done");sequence.soundIntervalMs=0;map->timedSequences.push_back(sequence);
  ScriptEvent event;event.id=stateId("save_init_once");event.x1=event.y1=6;event.x2=event.y2=7;
  ScriptAction item;item.type=ScriptAction::Type::GiveItem;item.id=stateId("save_init_item");event.actions.push_back(item);
  ScriptAction cue;cue.type=ScriptAction::Type::PlaySound;cue.sound=Sound::Pickup;event.actions.push_back(cue);map->scriptEvents.push_back(event);campaign->maps.push_back(map);
  Game current(campaign);
  if(!check(current.state(sequence.timerState)==1&&current.questItemCount(item.id)==0&&current.m_firedEvents.empty()&&current.sounds().empty(),"Fresh initialization seeds its timeline without executing a one-shot script or sound"))return false;
  current.updateScripts(0);current.m_sounds.clear();auto data=current.encodeSave();Game restored(campaign);
  if(!check(current.questItemCount(item.id)==1&&restored.decodeSave(data)&&restored.encodeSave()==data&&restored.questItemCount(item.id)==1&&restored.sounds().empty(),"Loading a fired one-shot preserves its single item award and silent canonical state"))return false;
  restored.updateScripts(0);
  if(!check(restored.questItemCount(item.id)==1&&restored.m_firedEvents.size()==1&&restored.sounds().empty(),"Restored one-shot events never replay an item award or sound"))return false;
 }
 // Existing saves are reconciled with newly authored content. Simulate an
 {Game legacy;legacy.loadLevel(5,false);legacy.m_player.health=63;legacy.m_world.m_doors.pop_back();legacy.storeChunk();Writer writer;legacy.archiveSave(writer,10);auto payload=writer.stream.str();
  auto data=std::string("RAWMETAL_SAVE 10 ")+std::to_string(checksum(payload))+"\n"+payload;Game restored;
  if(!check(restored.decodeSave(data)&&restored.level()==5&&restored.player().health==63&&restored.world().doors().back().transfer&&restored.m_chunks[6].enemies.size()==World(6).creatureSpawns().size()&&restored.m_chunks[9].clutter.size()==World(9).clutterSpawns().size(),"Six-chunk version 10 saves gain the four new chapters and transfer door"))return false;
 }
 // Existing saves are reconciled with newly authored content. Simulate an
 // older build that did not yet contain the last reactor creature, the final pickup,
 // one clutter item, or the newest gantry transfer door.
 {Game legacy;legacy.m_chunks[3].enemies[0].alive=false;legacy.m_chunks[3].enemies[0].hp=0;
  legacy.m_chunks[3].enemies.pop_back();legacy.m_chunks[3].pickups[0].active=false;legacy.m_chunks[3].pickups.pop_back();
  legacy.m_chunks[3].clutter[0].pos={9.25f,18.75f};legacy.m_chunks[3].clutter.pop_back();
  legacy.m_chunks[2].world.m_doors.pop_back();
  Writer writer;legacy.archiveSave(writer,5);auto payload=writer.stream.str();
  auto data=std::string("RAWMETAL_SAVE 5 ")+std::to_string(checksum(payload))+"\n"+payload;Game restored;
  if(!check(restored.decodeSave(data),"Older content save loads after authored additions"))return false;
  auto&reactor=restored.m_chunks[3];
  // The creature the older save predates comes back, alive, in its place.
  bool hasNewcomer=reactor.enemies.size()==Game{}.m_chunks[3].enemies.size()&&reactor.enemies.back().alive;
  bool oldDeathPreserved=!reactor.enemies.empty()&&!reactor.enemies[0].alive;
  bool collectedPreserved=!reactor.pickups.empty()&&!reactor.pickups[0].active;
  bool movedClutterPreserved=!reactor.clutter.empty()&&lengthSq(reactor.clutter[0].pos-Vec2{9.25f,18.75f})<.0001f;
  if(!check(hasNewcomer&&oldDeathPreserved&&collectedPreserved&&movedClutterPreserved&&reactor.pickups.size()==Game{}.m_chunks[3].pickups.size()&&reactor.clutter.size()==Game{}.m_chunks[3].clutter.size(),"New enemies/items spawn while old dynamic state survives"))return false;
  if(!check(restored.m_chunks[2].world.doors().size()==Game{}.m_chunks[2].world.doors().size(),"New doors use current map defaults instead of invalidating old saves"))return false;
 }
 // Version 6 files contained only the original four chunks. New chunks come
 // from the current authored baseline instead of making those saves unreadable.
 {Game legacy;Writer writer;legacy.archiveSave(writer,6);auto payload=writer.stream.str();
  auto data=std::string("RAWMETAL_SAVE 6 ")+std::to_string(checksum(payload))+"\n"+payload;Game restored;
  if(!check(restored.decodeSave(data)&&restored.m_chunks[4].world.level()==4&&restored.m_chunks[5].world.level()==5,
      "Version 6 saves gain the new authored megamap chunks"))return false;
 }
 // Version 4 saves remain loadable; new stalk state falls back to safe defaults.
 {Game legacy=mapInspection({18.6f,18.5f},0,0,3,true,-9,false);legacy.storeChunk();Writer writer;legacy.archiveSave(writer,4);auto payload=writer.stream.str();
  auto data=std::string("RAWMETAL_SAVE 4 ")+std::to_string(checksum(payload))+"\n"+payload;Game restored;
  if(!check(restored.decodeSave(data)&&!restored.m_enemies.empty()&&restored.m_enemies.back().stalkMode==Enemy::StalkMode::Watch&&restored.m_enemies.back().stalkTimer==0&&restored.m_enemies.back().stalkSide==1,"Version 4 saves load with default stalk state"))return false;
 }
 for(int level:{4,5}){
  Game legacy;legacy.loadLevel(level,false);legacy.m_player.pos=level==4?Vec2{8.9f,6}:Vec2{17.35f,22};legacy.m_player.z=-9;legacy.m_player.health=63;
  Game restored;if(!check(restored.decodeSave(legacy.encodeSave())&&restored.hullFits(restored.player().pos,restored.player().z,restored.player().hullHeight())&&restored.player().health==63&&length(restored.player().pos-legacy.player().pos)<1,"Old megamap saves escape corrected walls without losing health"))return false;
  restored.m_player.pos={12,21};restored.m_player.z=-9;Game roundtrip;
  if(!check(roundtrip.decodeSave(restored.encodeSave())&&lengthSq(roundtrip.player().pos-restored.player().pos)<.000001f&&roundtrip.player().z==restored.player().z,"Valid megamap positions remain unchanged on load"))return false;
 }
 Game game;auto pristine=game.encodeSave();auto corrupt=pristine;corrupt.back()='x';
 if(!check(!game.decodeSave(corrupt)&&!game.decodeSave(pristine.substr(0,pristine.size()/2))&&!game.decodeSave("RAWMETAL_SAVE 99 0\n")&&game.encodeSave()==pristine,"Corrupt / truncated / future saves leave the live game untouched"))return false;
 auto invalid=game;invalid.m_player.pool(WeaponId::Shotgun).total=-1;if(!check(!game.decodeSave(invalid.encodeSave()),"Valid-checksum invalid gameplay data rejected"))return false;
 game.m_heldClutter=0;game.m_clutter[0].projectile=true;game.m_clutter[0].velocity={2,3};Game held;if(!check(held.decodeSave(game.encodeSave())&&held.holdingClutter()&&held.m_clutter[0].velocity.x==2,"Held and moving clutter roundtrip"))return false;
 auto directory=std::filesystem::current_path()/("save-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));game.setSaveDirectory(directory.wstring());
 struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;for(int i=0;i<3;++i){auto p=slotPath(path.wstring(),i);std::filesystem::remove(p,e);p+=L".tmp";std::filesystem::remove(p,e);}auto c=checkpointPath(path.wstring());std::filesystem::remove(c,e);c+=L".tmp";std::filesystem::remove(c,e);std::filesystem::remove(path,e);}} cleanup{directory};
 InputState escape{};escape.escape=true;game.update(escape,.01f);game.update({},.01f);
 auto click=[&](int row){game.update({},.01f);InputState i{};i.pointerX=MenuLayout::X+30;i.pointerY=MenuLayout::RowTop+row*MenuLayout::RowHeight+7;i.fire=true;game.update(i,.01f);};
 click(9);if(!check(game.paused()&&game.menuPage()==MenuPage::Save,"Esc menu opens Save submenu"))return false;
 auto time=game.elapsed();click(0);if(!check(std::filesystem::exists(slotPath(directory.wstring(),0))&&game.elapsed()==time&&game.menuMessage()=="GAME SAVED","Save slot writes while simulation stays paused"))return false;
 auto before=readFile(slotPath(directory.wstring(),0));game.m_player.pool(WeaponId::Shotgun).total=5;click(0);if(!check(game.menuPage()==MenuPage::Overwrite,"Existing slot requires overwrite confirmation"))return false;
 click(0);if(!check(readFile(slotPath(directory.wstring(),0))==before,"Cancel preserves old save"))return false;
 click(0);click(1);if(!check(game.menuPage()==MenuPage::Save&&readFile(slotPath(directory.wstring(),0))!=before,"Confirmed overwrite replaces slot"))return false;
 {auto path=slotPath(directory.wstring(),0),temp=path;temp+=L".tmp";auto saved=readFile(path);std::filesystem::create_directory(temp);
  bool safe=!game.saveSlot(0)&&readFile(path)==saved;std::filesystem::remove(temp);if(!check(safe,"Failed temporary write preserves the previous slot"))return false;}
 game.update({},.01f);game.update(escape,.01f);if(!check(game.paused()&&game.menuPage()==MenuPage::Settings,"Esc returns from submenu without resuming"))return false;
 click(10);click(2);click(1);if(!check(game.paused()&&game.menuPage()==MenuPage::Load&&game.menuMessage().find("FAILED")!=std::string::npos,"Empty slot reports load error without losing progress"))return false;
 click(0);click(0);if(!check(game.menuPage()==MenuPage::Load,"Load confirmation can be cancelled"))return false;
 game.m_player.pool(WeaponId::Shotgun).total=1;auto revision=game.sessionRevision();click(0);click(1);
 if(!check(!game.paused()&&game.player().pool(WeaponId::Shotgun).total==5&&game.sessionRevision()>revision,"Confirmed load resumes and resets audio session"))return false;
 InputState fire{};fire.fire=true;game.update(fire,.01f);if(!check(game.player().pool(WeaponId::Shotgun).total==5,"Load click cannot fire the weapon"))return false;
 for(int slot=1;slot<3;++slot){if(!check(game.saveSlot(slot),"All three slots support independent saves"))return false;}
 game.setState(stateId("checkpoint_test"),37);game.giveQuestItem(stateId("test_fuse"));game.m_player.health=42;
 if(!check(game.saveCheckpoint(),"Autosave checkpoint writes independently of manual slots"))return false;
 game.setState(stateId("checkpoint_test"),0);game.takeQuestItem(stateId("test_fuse"));game.m_player.health=99;
 if(!check(game.loadCheckpoint()&&game.player().health==42&&game.state("checkpoint_test")==37&&game.hasQuestItem(stateId("test_fuse")),"Autosave restores flags, quest items and player state"))return false;
 {Game wave;ScriptAction spawn;spawn.type=ScriptAction::Type::SpawnEnemy;spawn.enemyKind=CreatureKind::Huntsman;spawn.position={6.5f,6.5f};spawn.z=wave.groundHeight(spawn.position);wave.spawnScriptEnemy(spawn);wave.m_enemies.back().hp=37;
  spawn.position={7.5f,6.5f};wave.spawnScriptEnemy(spawn);wave.m_enemies.back().alive=false;wave.m_enemies.back().hp=0;Game loaded;
  bool ok=loaded.decodeSave(wave.encodeSave());int scripted=0,dead=0;bool injured=false;for(const auto&e:loaded.enemies())if(e.scriptSpawned){++scripted;dead+=!e.alive;injured|=e.alive&&e.hp==37;}
  if(!check(ok&&scripted==2&&dead==1&&injured,"Version 14 retains living, injured and dead script-spawned creatures without resurrecting them"))return false;
 }
 return check(!game.saveSlot(-1)&&!game.loadSlot(3),"Invalid manual slots are rejected");
}
}

namespace retro {
bool Game::testPropSaves(){
 // A late save is big: past 1024 pieces in one map, and past the old 2 MB cap as the text archive
 // wrote it (format 28 writes the same in about two thirds of that), it still loads, byte-exact.
 {Game big(regression::campaign(regression::room()));for(int i=0;i<16000;++i){Clutter c{};c.pos={2.0123457f+float(i%180)*.1f,2.0123457f+float(i/180)*.25f};c.z=.0123457f;c.yaw=c.pitch=c.roll=.1234567f*float(i%7);c.restTime=1.2345678f;c.sleeping=true;big.m_clutter.push_back(c);}
  const auto bytes=big.encodeSave();Game text=big;text.storeChunk();Writer writer;text.archiveSave(writer,LegacySaveVersion);const size_t textBytes=writer.stream.str().size();
  Game loaded(big.m_customCampaign);if(textBytes<=2u*1024*1024||!loaded.decodeSave(bytes)||loaded.m_clutter.size()<16000||loaded.encodeSave()!=bytes){std::ofstream("prop-saves-big.txt")<<"bytes "<<bytes.size()<<" (text "<<textBytes<<") loaded "<<loaded.m_clutter.size()<<" "<<lastLoadFailure()<<std::endl;return false;}}
 auto map=regression::room();map->clutterSpawns.push_back({7,{8,8},0,0,27,.42f,.4f,.38f});auto campaign=regression::campaign(map);Game g(campaign);g.storeChunk();g.m_chunks[0].clutter.clear();g.m_heldClutter=-1;Writer writer;g.archiveSave(writer,16);auto payload=writer.stream.str();Game loaded(campaign);if(!loaded.decodeSave("RAWMETAL_SAVE 16 "+std::to_string(checksum(payload))+"\n"+payload)||loaded.m_clutter.size()!=1||loaded.m_clutter[0].model!=27)return false;
 loaded.m_heldClutter=0;auto bytes=loaded.encodeSave();Game held(campaign);if(!held.decodeSave(bytes)||held.m_heldClutter!=0||held.encodeSave()!=bytes)return false;
 // Version 21 keeps which leads have torn; a version 20 or 19 save opens on the map's own leads, clipped.
 {auto m=regression::room();m->structures.push_back({5.8f,6,6,10,0,3});m->clutterSpawns.push_back({7,{8,8},0,0,27,.42f,.4f,.38f});Lead lead;lead.anchor={6.012f,8,1.6f};lead.prop=0;lead.offset={0,0,.38f};lead.length=2.4f;lead.live=true;m->leads.push_back(lead);
  auto leads=regression::campaign(m);Game source(leads);if(source.world().leadRopes()[0].torn)return false;source.tearLead(0,0);
  auto raw=[&](int version){source.storeChunk();Writer writer;source.archiveSave(writer,version);auto payload=writer.stream.str();return "RAWMETAL_SAVE "+std::to_string(version)+" "+std::to_string(checksum(payload))+"\n"+payload;};
  Game current(leads),v20(leads),v19(leads);const auto now=source.encodeSave();
  if(!current.decodeSave(now)||!current.world().leadRopes()[0].torn||current.encodeSave()!=now)return false;
  if(!v20.decodeSave(raw(20))||v20.world().leadRopes()[0].torn||!v20.world().leadRopes()[0].sleeping||!v19.decodeSave(raw(19))||v19.world().leadRopes()[0].torn)return false;}
 return true;
}
// Version 19 keeps each firearm's pool, the gun in hand, a switch under way and
// the handgun's ownership; a version 18 save loads with its pair as the shotgun's.
bool Game::testHandgunSaves(std::ostream& report){
 bool passed=true;auto check=[&](bool ok,const char*label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';passed&=ok;};
 auto campaign=regression::campaign(regression::room());
 auto raw=[](Game g,int version){g.storeChunk();Writer writer;g.archiveSave(writer,version);auto payload=writer.stream.str();return "RAWMETAL_SAVE "+std::to_string(version)+" "+std::to_string(checksum(payload))+"\n"+payload;};
 {Game g(campaign);g.grantHandgun(12);g.m_player.pool(WeaponId::Shotgun)={40,3};g.m_player.pool(WeaponId::Handgun)={30,7};g.equipWeapon(WeaponId::Handgun);g.m_itemCells[3]=4;
  const auto bytes=g.encodeSave();Game loaded(campaign);const bool ok=loaded.decodeSave(bytes);
  check(ok&&bytes.rfind("RAWMETAL_SAVE "+std::to_string(SaveVersion)+" ",0)==0,("A current save is version "+std::to_string(SaveVersion)).c_str());
  check(ok&&loaded.player().pools==g.player().pools&&loaded.activeFirearm()==WeaponId::Handgun&&loaded.weaponEquipped()&&loaded.hasHandgun()&&loaded.itemCell(3)==4,"Version 19 round-trips both pools, the handgun in hand, its ownership and the 9 mm cell");
  check(ok&&loaded.encodeSave()==bytes,"The version 21 save is byte-exact on re-encoding");
  g.selectWeapon(WeaponId::Shotgun);g.m_holster=.4f;Game mid(campaign);check(mid.decodeSave(g.encodeSave())&&mid.weaponSwitchPhase()==1&&mid.weaponSwitchTarget()==WeaponId::Shotgun&&mid.activeFirearm()==WeaponId::Handgun,"A switch under way is saved");
  g.equipWeapon(WeaponId::Handgun);g.m_player.pool(WeaponId::Handgun)={20,0};g.reloadWeapon();g.m_reloadTimer=.7f;Game reloading(campaign);
  check(reloading.decodeSave(g.encodeSave())&&reloading.emptyReload()&&std::fabs(reloading.reloadDuration()-1.9f)<.001f&&std::fabs(reloading.reloadProgress()-(1-.7f/1.9f))<.01f,"An empty reload under way keeps its length and progress");}
 {Game g(campaign);g.m_player.pool(WeaponId::Shotgun)={23,5};const auto legacy=raw(g,18);Game loaded(campaign);const bool ok=loaded.decodeSave(legacy);
  check(ok&&loaded.player().pool(WeaponId::Shotgun)==AmmoPool{23,5}&&loaded.player().pool(WeaponId::Handgun)==AmmoPool{}&&loaded.activeFirearm()==WeaponId::Shotgun&&!loaded.hasHandgun(),"A version 18 save loads its ammo and tube into the shotgun pool, no handgun");
  check(ok&&loaded.itemCell(3)>=0&&loaded.itemCell(3)!=loaded.itemCell(1),"The 9 mm cell is placed clear of the older items");
  g.m_pickups.push_back({{9,9},Pickup::Kind::HandgunAmmo,true});bool refusedOld=false;try{Game refused(campaign);refusedOld=!refused.decodeSave(raw(g,18));}catch(const std::exception&){refusedOld=true;}check(refusedOld,"A 9 mm box cannot appear in a version 18 archive");}
 {Game g(campaign);g.grantHandgun(12);g.m_player.pool(WeaponId::Handgun)={20,13};Game loaded(campaign);check(!loaded.decodeSave(raw(g,19)),"A magazine holding more than twelve is refused");
  Game h(campaign);h.m_player.pool(WeaponId::Handgun)={12,4};Game unowned(campaign);check(!unowned.decodeSave(raw(h,19)),"A loaded handgun pool without the handgun is refused");
  Game k(campaign);k.m_player.pool(WeaponId::Handgun)={12,0};Game waiting(campaign);check(waiting.decodeSave(raw(k,19))&&waiting.player().pool(WeaponId::Handgun).total==12,"9 mm waiting for the pistol loads");
  Game s(campaign);s.grantHandgun(12);s.m_player.pool(WeaponId::Shotgun)={12,6};s.m_player.pool(WeaponId::Handgun)={12,12};Game clamp(campaign);check(clamp.decodeSave(s.encodeSave())&&clamp.player().pool(WeaponId::Handgun).loaded==12&&clamp.player().pool(WeaponId::Shotgun).loaded==6,"Each pool keeps its own capacity: twelve 9 mm and six shells");}
 return passed;
}
}
namespace retro {
// Saves written the way play writes them, on every Chapter 1 and Chapter 2 map:
// on entry, after a while, the moment men and creatures die and while their
// bodies settle, with props broken into pieces and barrels blown, mid-fight
// with guards shooting, the script checkpoints a walk sets off, and the seam
// autosave into the next map. Every one must load, load again after it was
// itself saved, and play on. RM_SAVE_MAPS="a-b" narrows the maps.
bool Game::testSavesInPlay(std::ostream& report){
 int first=0,last=109,stride=1;if(const char* range=std::getenv("RM_SAVE_MAPS")){std::sscanf(range,"%d-%d/%d",&first,&last,&stride);stride=std::max(1,stride);}
 const auto directory=std::filesystem::temp_directory_path()/("rm-saves-in-play-"+std::to_string(GetCurrentProcessId()));std::error_code ignored;std::filesystem::create_directories(directory,ignored);
 int saves=0,refused=0;bool passed=true;std::string lastCheckpoint;
 const bool verbose=std::getenv("RM_SAVE_VERBOSE")!=nullptr;
 auto load=[&](const std::string& data,int level,const std::string& what,bool deep=false){
  using clock=std::chrono::steady_clock;auto ms=[](clock::time_point a,clock::time_point b){return std::chrono::duration_cast<std::chrono::milliseconds>(b-a).count();};
  const auto t0=clock::now();++saves;Game loaded;const auto t1=clock::now();const bool ok=loaded.decodeSave(data);const auto t2=clock::now();
  if(!ok){++refused;passed=false;report<<"FAIL map "<<level<<" "<<what<<": "<<t_loadFailure<<'\n';report.flush();return;}
  if(!deep){if(verbose){report<<"  map "<<level<<" "<<what<<": decode "<<ms(t1,t2)<<" ms\n";report.flush();}return;}
  // The loaded game plays on and is saved again: that save must load too.
  for(int i=0;i<10;++i)loaded.update({},1.f/60);const auto t3=clock::now();
  const auto again=loaded.encodeSave();Game second;if(!second.decodeSave(again)){++refused;passed=false;report<<"FAIL map "<<level<<" "<<what<<" (saved again after loading): "<<t_loadFailure<<'\n';report.flush();}
  if(verbose){report<<"  map "<<level<<" "<<what<<": construct "<<ms(t0,t1)<<" ms, decode "<<ms(t1,t2)<<" ms, play "<<ms(t2,t3)<<" ms, again "<<ms(t3,clock::now())<<" ms\n";report.flush();}
 };
 auto probe=[&](const Game& g,const std::string& what,bool deep=false){load(g.encodeSave(),g.m_level,what,deep);};
 // A checkpoint the game wrote itself since the last look.
 auto checkpoints=[&](const Game& g,const std::string& what){std::error_code e;const auto path=checkpointPath(directory.wstring());if(!std::filesystem::exists(path,e))return;
  std::string data;try{data=readFile(path);}catch(const std::exception&){return;}if(data==lastCheckpoint)return;lastCheckpoint=data;load(data,g.m_level,"checkpoint during "+what);};
 auto run=[&](Game& g,int frames,const InputState& input,const std::string& what){for(int i=0;i<frames;++i){g.update(input,1.f/60);checkpoints(g,what);}};
 auto fresh=[&](int level){Game g;g.setSaveDirectory(directory.wstring());g.setAsyncLoads(false);g.startMainCampaign(level);g.m_god=true;return g;};
 // One game carried through every map in turn, as a playthrough carries it: what it gathers
 // (states, fired events, bodies, debris) must still fit a save at the end of Chapter 2.
 if(std::getenv("RM_SAVE_LONG")){Game g=fresh(first);
  for(int level=first;level<=last&&level<CampaignChunkCount;level+=stride){
   if(level!=g.m_level){g.storeChunk();g.loadLevel(level,true);g.updateStreaming(0);}
   InputState walk{};walk.forward=true;walk.fire=true;walk.mouseDx=20;run(g,45,walk,"a walk");
   for(size_t a=0;a<g.m_friendlyActors.size();++a){const auto&npc=g.m_friendlyActors[a];if(npc.level!=level||!npc.track().health||(npc.track().deadState&&g.state(npc.track().deadState)))continue;g.damageFriendly(a,1e4f,g.m_player.pos,{ShotKind::Player,0,0});}
   for(auto&e:g.m_enemies)if(e.alive)g.applyEnemyDamage(e,e.maxHp*3+100);
   for(size_t i=0;i<g.m_clutter.size();++i)if(g.m_clutter[i].kind==7&&!g.m_clutter[i].broken)g.breakClutter(g.m_level,i,{1,0,0});
   g.landShards();run(g,30,{},"after");
   size_t clutter=0,enemies=0;for(int l=0;l<g.chunkCount();++l){clutter=std::max(clutter,l==g.m_level?g.m_clutter.size():g.m_chunks[l].clutter.size());enemies=std::max(enemies,l==g.m_level?g.m_enemies.size():g.m_chunks[l].enemies.size());}
   report<<"  through map "<<level<<": "<<g.m_states.size()<<" states (limit 8192), "<<g.m_firedEvents.size()<<" fired events (limit 1024), most clutter in a map "<<clutter<<", most creatures "<<enemies<<", save "<<g.encodeSave().size()<<" bytes\n";report.flush();
   if((level-first)/stride%10==9||level+stride>last)probe(g,"after the walk through map "+std::to_string(level),true);
  }
  std::filesystem::remove_all(directory,ignored);report<<(passed?"PASS":"FAIL")<<" long walk: "<<saves<<" saves, "<<refused<<" refused\n";return passed;}
 // A physics glitch leaves a NaN spin on a shard and an infinite timer on a creature: the save still loads.
 {Game g=fresh(first);bool staged=false;for(auto&c:g.m_clutter){c.spin=std::numeric_limits<float>::quiet_NaN();c.vz=std::numeric_limits<float>::infinity();staged=true;break;}
  for(auto&e:g.m_enemies){e.voiceTimer=std::numeric_limits<float>::infinity();e.heading=std::numeric_limits<float>::quiet_NaN();staged=true;break;}
  g.m_shotAge=3e7f;probe(g,staged?"with a NaN spin, an infinite timer and a huge clock":"with a huge clock");}
 for(int level=first;level<=last&&level<CampaignChunkCount;level+=stride){
  const int before=refused;
  {Game g=fresh(level);run(g,90,{},"idle");
   // The men on this map die where they stand, and the creatures.
   int men=0,beasts=0;
   for(size_t a=0;a<g.m_friendlyActors.size();++a){const auto&npc=g.m_friendlyActors[a];if(npc.level!=level||!npc.track().health||(npc.track().deadState&&g.state(npc.track().deadState)))continue;g.damageFriendly(a,1e4f,g.m_player.pos,{ShotKind::Player,0,0});++men;}
   for(auto&e:g.m_enemies)if(e.alive){g.applyEnemyDamage(e,e.maxHp*3+100);++beasts;}
   if(men+beasts){run(g,6,{},"dying");probe(g,"0.1 s after the deaths");run(g,90,{},"bodies settling");probe(g,"1.6 s after the deaths",true);}
   // Props broken into pieces, a barrel blown.
   int broken=0;for(size_t i=0;i<g.m_clutter.size()&&broken<4;++i)if(g.m_clutter[i].kind==7&&!g.m_clutter[i].broken){g.breakClutter(g.m_level,i,{1,0,0});++broken;}
   g.landShards();
   bool blown=false;for(int y=0;y<World::Height&&!blown;++y)for(int x=0;x<World::Width&&!blown;++x)if(g.m_world.tile(x,y)=='B'){g.detonateBarrel(g.m_level,x,y);blown=true;}
   if(broken||blown){run(g,12,{},"debris flying");run(g,120,{},"debris settling");probe(g,"debris at rest");}
  }
  // Mid-fight: fire into the room while walking and turning, guards answering.
  {Game g=fresh(level);InputState fight{};fight.fire=true;fight.forward=true;
   for(int second=0;second<4;++second){fight.mouseDx=second%2?40.f:-25.f;run(g,60,fight,"a fight");if(second%2)probe(g,"mid-fight, "+std::to_string(second+1)+" s",second==3);fight.mouseDx=0;}
  }
  // The seam autosave: across each edge with a map beyond it.
  {const Vec2 edges[]={{12,24.3f},{12,-.3f},{24.3f,12},{-.3f,12}};
   for(const auto at:edges){Game g=fresh(level);const Vec2 global=at+g.chunkOffset(level);int beyond=-1;
    for(int other=0;other<g.chunkCount();++other){const Vec2 o=g.chunkOffset(other);if(other!=level&&global.x>=o.x&&global.x<o.x+World::Width&&global.y>=o.y&&global.y<o.y+World::Height){beyond=other;break;}}
    if(beyond<0)continue;g.m_player.pos=at;run(g,2,{},"crossing into "+std::to_string(beyond));if(g.m_level!=level)probe(g,"just across into map "+std::to_string(beyond));}
  }
  report<<(refused==before?"PASS":"FAIL")<<" map "<<level<<' '<<CampaignMapNames[level]<<'\n';report.flush();
 }
 std::filesystem::remove_all(directory,ignored);
 report<<(passed?"PASS":"FAIL")<<" saves in play: "<<saves<<" saves, "<<refused<<" refused\n";
 return passed;
}
}
namespace retro {
// The section container (format 28), the refit of formats 1-27, and the stores and hooks other
// systems use (docs/Systems/save-format.md). Real saves are copied into a temp folder first: the
// originals are only read, and checked unchanged at the end.
bool Game::testSaveFormat(std::ostream& report,int parts){
 bool passed=true;
 auto encode=[](const Game& g){try{return g.encodeSave();}catch(const std::exception& error){return std::string("<save failed: ")+error.what()+">";}};
 auto check=[&](bool ok,const std::string& label,const std::string& detail=""){report<<(ok?"PASS ":"FAIL ")<<label<<(detail.empty()?"":" ("+detail+")")<<'\n';report.flush();passed&=ok;return ok;};
 using clock=std::chrono::steady_clock;auto seconds=[](clock::time_point t){return std::chrono::duration<double>(clock::now()-t).count();};
 auto legacy=[](Game g,int version){g.storeChunk();Writer writer;g.archiveSave(writer,version);auto payload=writer.stream.str();return "RAWMETAL_SAVE "+std::to_string(version)+" "+std::to_string(checksum(payload))+"\n"+payload;};
 auto header=[](const std::string& data){return data.substr(0,data.find('\n'));};
 auto payloadOf=[](const std::string& data){return data.substr(data.find('\n')+1);};
 auto wrap=[](const std::string& payload){return "RAWMETAL_SAVE "+std::to_string(SaveVersion)+" "+std::to_string(checksum(payload))+"\n"+payload;};
 auto names=[&](const std::string& data){std::vector<std::string> n;for(const auto& f:sectionFrames(payloadOf(data),rangeChecksum))n.push_back(f.name);return n;};
 auto has=[&](const std::string& data,const std::string& name){const auto n=names(data);return std::find(n.begin(),n.end(),name)!=n.end();};
 // The save rebuilt a section at a time: edit may change a section's name, version, flags or body, or return false to drop it.
 auto rebuild=[&](const std::string& data,auto edit){const auto payload=payloadOf(data);std::string out;
  for(const auto& f:sectionFrames(payload,rangeChecksum)){std::string name=f.name;int version=f.version;std::uint16_t flags=f.flags;std::string body=payload.substr(f.offset,f.size);if(edit(name,version,flags,body))appendSection(out,name,std::uint16_t(version),flags,body);}
  return out;};
 auto contains=[](const std::string& text,const std::string& part){return text.find(part)!=std::string::npos;};
 auto put=[](const std::filesystem::path& p,const std::string& data){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(data.data(),std::streamsize(data.size()));};
 auto get=[](const std::filesystem::path& p){try{return readFile(p);}catch(const std::exception&){return std::string("<missing>");}};
 auto logText=[]{std::ifstream in("RawMetal-load.txt",std::ios::binary);return std::string((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());};
 const auto root=std::filesystem::temp_directory_path()/("rm-save-format-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 std::error_code ignored;std::filesystem::create_directories(root,ignored);

 auto refitMap=regression::room();refitMap->creatureSpawns.push_back({Enemy::Kind::Warden,{9,9},0});refitMap->pickupSpawns.push_back({{12,12},PickupKind::Ammo});refitMap->clutterSpawns.push_back({7,{8,8},0,0,27,.42f,.4f,.38f});
 const auto room=regression::campaign(refitMap);
 // ---- 1. Every positional version the archive can still write: load, refit with its backup, keep the game, re-save byte-exact ----
 // Formats 1-11 cannot hold a custom campaign, so they are the main campaign (a full load of it
 // takes seconds); 12-27 are a custom room with a creature, a pickup and a prop, and 19 and 27 the
 // campaign as well. RM_SAVE_FORMAT_ALL=1 runs every format on the campaign.
 const auto refitDir=root/"legacy";std::filesystem::create_directories(refitDir,ignored);int good=0,ran=0;
 auto refitVersions=[&](const std::string& world,std::function<Game()> fresh,int pickupChunk,int deadChunk,const std::vector<int>& versions){
  Game source=fresh();source.m_player.health=63;source.m_elapsed=754.5f;source.m_medkits=2;source.m_player.pool(WeaponId::Shotgun)={17,3};
  source.setState(stateId("refit_probe"),5);source.giveQuestItem(stateId("refit_fuse"));
  auto pickupsOf=[](Game& g,int c)->std::vector<Pickup>&{return c==g.m_level?g.m_pickups:g.m_chunks[size_t(c)].pickups;};
  auto enemiesOf=[](Game& g,int c)->std::vector<Enemy>&{return c==g.m_level?g.m_enemies:g.m_chunks[size_t(c)].enemies;};
  Pickup taken{};bool hasTaken=false;for(auto& p:pickupsOf(source,pickupChunk))if(int(p.kind)<=int(PickupKind::Ammo)){p.active=false;taken=p;hasTaken=true;break;}
  if(pickupChunk==source.m_level)source.m_kills=1;else source.m_chunks[size_t(pickupChunk)].kills=1;
  Enemy dead{};bool hasDead=false;if(!enemiesOf(source,deadChunk).empty()){auto&e=enemiesOf(source,deadChunk)[0];e.alive=false;e.hp=0;dead=e;hasDead=true;}
  source.storeChunk();
  {const auto encodeStart=clock::now();const auto sections=encode(source);const double encodeTime=seconds(encodeStart);
   Game probe=fresh();const auto t=clock::now();probe.decodeSave(sections);const double loadTime=seconds(t);
   const auto text=legacy(source,LegacySaveVersion);Game textProbe=fresh();const auto t27=clock::now();textProbe.decodeSave(text);const double textTime=seconds(t27);
   report<<"  "<<world<<": a save is "<<text.size()<<" bytes as format 27 text, "<<sections.size()<<" bytes as format 28 ("<<names(sections).size()<<" sections); writing it takes "<<std::fixed<<std::setprecision(2)<<encodeTime<<" s, a whole load "<<loadTime<<" s (format 27: "<<textTime<<" s)\n";}
  const auto dir=refitDir;
  for(int version:versions){const auto t0=clock::now();Game g=source;++ran;
   // What this version could not hold is left out: its reader would refuse it.
   const int maxKind=version>=23?int(Enemy::Kind::Machine):version>=16?int(Enemy::Kind::Transformed):version>=15?int(Enemy::Kind::Chimera):version>=13?int(Enemy::Kind::Mutant):version>=3?3:2;
   auto strip=[&](std::vector<Enemy>& enemies,std::vector<Pickup>& pickups){std::erase_if(enemies,[&](const Enemy&e){return int(e.kind)>maxKind;});if(version<19)std::erase_if(pickups,[](const Pickup&p){return int(p.kind)>int(PickupKind::Ammo);});};
   strip(g.m_enemies,g.m_pickups);for(auto&chunk:g.m_chunks)strip(chunk.enemies,chunk.pickups);
   const bool deadKept=hasDead&&std::any_of(g.m_chunks[size_t(deadChunk)].enemies.begin(),g.m_chunks[size_t(deadChunk)].enemies.end(),[&](const Enemy&e){return !e.alive&&e.kind==dead.kind&&lengthSq(e.home-dead.home)<.0004f;});
   const auto data=legacy(g,version);const auto path=dir/(world+"-v"+std::to_string(version)+".rms");put(path,data);
   Game loaded=fresh();const bool ok=loaded.decodeSave(data);const std::string why=ok?std::string():lastLoadFailure();
   const auto refit=ok?loaded.refitSaveFile(path,data):SaveRefit{};
   const auto backup=dir/(world+"-v"+std::to_string(version)+".rms.v"+std::to_string(version)+".bak");
   const auto rewritten=get(path);const auto first=ok?encode(loaded):std::string();
   Game second=fresh();const bool again=ok&&second.decodeSave(rewritten);const auto secondBytes=again?encode(second):std::string();
   const auto twice=again?second.refitSaveFile(path,rewritten):SaveRefit{};
   auto kept=[&](const Game& l){
    bool k=l.m_level==0&&l.m_player.health==63&&std::fabs(l.m_elapsed-754.5f)<.01f&&l.m_medkits==2&&l.m_player.pool(WeaponId::Shotgun).total==17&&
     l.m_player.pool(WeaponId::Shotgun).loaded==(version>=2?3:std::min(17,weaponDef(WeaponId::Shotgun).cycle.tubeCapacity))&&l.m_chunks[size_t(pickupChunk)].kills==1;
    if(version>=6)k&=l.state(stateId("refit_probe"))==5&&l.hasQuestItem(stateId("refit_fuse"));
    if(hasTaken)k&=std::any_of(l.m_chunks[size_t(pickupChunk)].pickups.begin(),l.m_chunks[size_t(pickupChunk)].pickups.end(),[&](const Pickup&p){return p.kind==taken.kind&&lengthSq(p.pos-taken.pos)<.0004f&&!p.active;});
    if(deadKept)k&=std::any_of(l.m_chunks[size_t(deadChunk)].enemies.begin(),l.m_chunks[size_t(deadChunk)].enemies.end(),[&](const Enemy&e){return e.kind==dead.kind&&lengthSq(e.home-dead.home)<.0004f&&!e.alive;});
    return k;};
   const bool keptFirst=ok&&kept(loaded),keptSecond=again&&kept(second),exact=again&&first==rewritten&&secondBytes==rewritten;
   const bool fine=ok&&loaded.loadedSaveVersion()==version&&refit.rewritten&&refit.backup==backup&&get(backup)==data&&header(rewritten).rfind("RAWMETAL_SAVE 28 ",0)==0&&exact&&keptFirst&&keptSecond&&second.loadedSaveVersion()==SaveVersion&&!twice.rewritten&&twice.backup.empty();
   good+=fine;std::ostringstream detail;detail<<"loaded "<<ok<<(why.empty()?"":" ["+why+"]")<<", rewritten "<<refit.rewritten<<(refit.note.empty()?"":" ["+refit.note+"]")<<", backup "<<(get(backup)==data)<<", state kept "<<keptFirst<<"/"<<keptSecond<<", byte-exact "<<exact<<", "<<data.size()<<" -> "<<rewritten.size()<<" bytes, "<<std::fixed<<std::setprecision(2)<<seconds(t0)<<" s";
   check(fine,world+": format "+std::to_string(version)+" save loads, is refitted to 28 beside its backup, keeps its game and re-saves byte-exact",detail.str());
  }
 };
 // A full campaign load takes seconds, so the campaign formats are split over two parts (each a
 // lab tube under its ten minutes): 1-6, then 7-11, 19 and 27 (with RM_SAVE_FORMAT_ALL, 1-13 and 14-27).
 {const bool everything=std::getenv("RM_SAVE_FORMAT_ALL")!=nullptr;std::vector<int> early,late,onRoom;
  for(int v=1;v<=LegacySaveVersion;++v){if(v<=6||(everything&&v<=13))early.push_back(v);else if(v<=11||v==19||v==27||everything)late.push_back(v);if(v>=12)onRoom.push_back(v);}
  if(parts&SaveTestCampaignEarly)refitVersions("campaign",[]{return Game();},1,3,early);
  if(parts&SaveTestCampaignLate)refitVersions("campaign",[]{return Game();},1,3,late);
  if(parts&SaveTestRoomFormats)refitVersions("room",[room]{return Game(room);},0,0,onRoom);
  if(ran)check(good==ran,"every positional format in this part refits",std::to_string(good)+" of "+std::to_string(ran));
 }

 // ---- 2. The load paths: a backup never written over, a failed rewrite, a second load, the worker ----
 if(parts&SaveTestCore){Game source(room);source.m_player.health=58;const auto data=legacy(source,19);
  {const auto dir=root/"ui";std::filesystem::create_directories(dir,ignored);const auto path=dir/"checkpoint.rms";put(path,data);
   auto taken=dir/"checkpoint.rms.v19.bak";put(taken,"someone else's file");
   Game ui(room);ui.setAsyncLoads(false);ui.setSaveDirectory(dir.wstring());const bool loaded=ui.loadCheckpoint();const auto now=get(path);
   check(loaded&&ui.player().health==58&&get(taken)=="someone else's file"&&get(dir/"checkpoint.rms.v19-2.bak")==data&&header(now).rfind("RAWMETAL_SAVE 28 ",0)==0&&ui.pickupNotice()=="SAVE UPDATED TO THE NEW FORMAT / OLD COPY KEPT"&&ui.pickupNoticeTime()>0,
    "Loading an old autosave rewrites it as 28, keeps the original as .v19-2.bak beside a .v19.bak it does not touch, and says so",ui.pickupNotice());
   check(contains(logText(),"save refit: "+path.string()+" was format 19, rewritten as format 28; the original is kept as checkpoint.rms.v19-2.bak"),"RawMetal-load.txt records the refit");
   Game again(room);again.setAsyncLoads(false);again.setSaveDirectory(dir.wstring());const bool reloaded=again.loadCheckpoint();
   check(reloaded&&get(path)==now&&!std::filesystem::exists(dir/"checkpoint.rms.v19-3.bak",ignored)&&again.pickupNotice().empty(),"A refitted save loads again with no second backup and no notice");
   put(dir/"slot-2.rms",data);Game slot(room);slot.setAsyncLoads(false);slot.setSaveDirectory(dir.wstring());
   check(slot.loadSlot(1)&&get(dir/"slot-2.rms.v19.bak")==data&&header(get(dir/"slot-2.rms")).rfind("RAWMETAL_SAVE 28 ",0)==0,"A manual slot is refitted the same way");}
  {const auto dir=root/"fails";std::filesystem::create_directories(dir,ignored);const auto path=dir/"checkpoint.rms";put(path,data);
   auto temp=path;temp+=L".tmp";std::filesystem::create_directories(temp,ignored); // the atomic write's temporary cannot be made
   Game ui(room);ui.setAsyncLoads(false);ui.setSaveDirectory(dir.wstring());const bool loaded=ui.loadCheckpoint();
   check(loaded&&ui.player().health==58&&get(path)==data&&get(dir/"checkpoint.rms.v19.bak")==data&&contains(ui.pickupNotice(),"NOT UPDATED")&&contains(logText(),"save refit skipped: "+path.string()),
    "A rewrite that fails leaves the original file, its backup and the loaded game intact, and says so",ui.pickupNotice());}
  {const auto dir=root/"worker";std::filesystem::create_directories(dir,ignored);const auto path=dir/"checkpoint.rms";put(path,data);
   Game ui(room);ui.setSaveDirectory(dir.wstring());ui.setAsyncLoads(true);const bool began=ui.loadCheckpoint();
   for(int i=0;i<12000&&ui.loading();++i){ui.update({},.001f);Sleep(5);}
   check(began&&!ui.loading()&&ui.player().health==58&&get(dir/"checkpoint.rms.v19.bak")==data&&header(get(path)).rfind("RAWMETAL_SAVE 28 ",0)==0&&ui.pickupNotice()=="SAVE UPDATED TO THE NEW FORMAT / OLD COPY KEPT","A load on the worker thread refits too and the notice shows when it lands",ui.pickupNotice());}
 }

 // ---- 3. Real saves from past releases: copied into a temp folder, never the originals ----
 if(parts&SaveTestRealSaves){std::vector<std::filesystem::path> sources;
  if(const char* extra=std::getenv("RM_REAL_SAVES")){std::stringstream list(extra);std::string one;while(std::getline(list,one,';'))if(!one.empty())sources.push_back(one);}
  wchar_t local[32768]{};const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
  if(length>0&&length<32768){sources.push_back(std::filesystem::path(local)/L"RawMetal"/L"saves");sources.push_back(std::filesystem::path(std::wstring(local)+L"RawMetalsaves"));}
  for(auto at=std::filesystem::current_path();;at=at.parent_path()){if(std::filesystem::is_directory(at/"lab"/"fixtures"/"saves",ignored)){sources.push_back(at/"lab"/"fixtures"/"saves");break;}if(at==at.parent_path())break;}
  struct Original {std::filesystem::path path;std::string bytes;std::filesystem::file_time_type time;};std::vector<Original> originals;std::vector<std::string> seen;
  const auto dir=root/"real";std::filesystem::create_directories(dir,ignored);int tested=0;
  for(const auto& source:sources){if(!std::filesystem::is_directory(source,ignored))continue;
   for(const auto& entry:std::filesystem::directory_iterator(source,ignored)){if(entry.path().extension()!=".rms"||!entry.is_regular_file(ignored))continue;
    const auto bytes=get(entry.path());originals.push_back({entry.path(),bytes,std::filesystem::last_write_time(entry.path(),ignored)});
    if(std::find(seen.begin(),seen.end(),bytes)!=seen.end())continue;seen.push_back(bytes);
    const auto copy=dir/(std::to_string(++tested)+"-"+entry.path().filename().string());std::filesystem::copy_file(entry.path(),copy,ignored);
    const auto data=get(copy);const auto t0=clock::now();Game loaded;const bool ok=loaded.decodeSave(data);const std::string why=ok?std::string():lastLoadFailure();
    const int from=ok?loaded.loadedSaveVersion():0;const auto refit=ok?loaded.refitSaveFile(copy,data):SaveRefit{};const auto now=get(copy);
    Game second;const bool again=ok&&second.decodeSave(now);const bool exact=again&&encode(second)==now;
    const bool same=again&&second.m_level==loaded.m_level&&second.m_player.health==loaded.m_player.health&&second.m_elapsed==loaded.m_elapsed&&second.m_player.pools==loaded.m_player.pools&&
     std::equal(second.m_states.begin(),second.m_states.end(),loaded.m_states.begin(),loaded.m_states.end(),[](const StateValue&a,const StateValue&b){return a.id==b.id&&a.value==b.value;})&&
     std::equal(second.m_questItems.begin(),second.m_questItems.end(),loaded.m_questItems.begin(),loaded.m_questItems.end(),[](const QuestItemStack&a,const QuestItemStack&b){return a.id==b.id&&a.count==b.count;});
    const bool refitted=from>=SaveVersion||(refit.rewritten&&get(refit.backup)==data);const bool good=ok&&refitted&&exact&&same;
    std::ostringstream detail;detail<<entry.path().string()<<", "<<header(data)<<", "<<data.size()<<" -> "<<now.size()<<" bytes, map "<<loaded.m_level<<", "<<loaded.m_states.size()<<" states, "<<std::fixed<<std::setprecision(2)<<seconds(t0)<<" s"<<(why.empty()?"":", refused: "+why)<<(refit.note.empty()?"":", "+refit.note);
    check(good,"Real save "+entry.path().filename().string()+" (format "+std::to_string(from)+") loads, refits, re-saves byte-exact and keeps its game",detail.str());}}
  check(tested>0,"Real saves were found to test",std::to_string(tested)+" distinct saves");
  bool untouched=true;for(const auto& o:originals){untouched&=get(o.path)==o.bytes&&std::filesystem::last_write_time(o.path,ignored)==o.time;
   for(int v=1;v<=SaveVersion;++v)untouched&=!std::filesystem::exists(std::filesystem::path(o.path.wstring()+L".v"+std::to_wstring(v)+L".bak"),ignored);}
  check(untouched,"The original real saves are unchanged and nothing was written beside them",std::to_string(originals.size())+" files read");
 }

 // ---- 4. Damaged sections are refused with the reason, the live game untouched ----
 if(parts&SaveTestCore){auto campaign=regression::campaign(regression::room());
 {Game g(campaign);g.m_player.health=71;g.setMapVariable(-1,"probe",std::int64_t(1));const auto data=encode(g);
  put("save-format-sample.rms",data); // beside the report, for tools/dev/savedump.py
  Game live(campaign);const bool base=live.decodeSave(data);const auto before=encode(live);
  auto refused=[&](const std::string& bytes,const std::string& expect,const std::string& label){const bool loaded=live.decodeSave(bytes);const auto why=loaded?std::string("(loaded)"):lastLoadFailure();check(!loaded&&contains(why,expect),label,why);};
  check(base&&before==data,"The probe save round-trips byte-exact");
  refused(data.substr(0,data.size()-1),"truncated save: section 'variables'","A save cut short inside its last section is refused");
  refused(data.substr(0,data.find('\n')+4),"truncated save","A save cut short inside a section frame is refused");
  refused(data.substr(0,data.find('\n')+1),"checksum mismatch","A save with nothing after its header is refused");
  {auto payload=payloadOf(data);for(const auto& f:sectionFrames(payload,rangeChecksum))if(f.name=="player"){payload[f.offset+2]^=0x40;break;}
   refused(wrap(payload),"section 'player' is damaged (checksum mismatch)","A section whose bytes changed is refused by name");}
  {const float nan=std::numeric_limits<float>::quiet_NaN();refused(wrap(rebuild(data,[&](std::string& name,int&,std::uint16_t&,std::string& body){if(name=="player")std::memcpy(body.data(),&nan,4);return true;})),"section 'player': invalid number","A section holding a value no game can have is refused by name");}
  refused(wrap(rebuild(data,[](std::string& name,int&,std::uint16_t&,std::string&){return name!="props";})),"missing section 'props'","A save without a core section is refused");
  refused(wrap(rebuild(data,[](std::string& name,int&,std::uint16_t&,std::string& body){if(name=="inventory")body.push_back('\0');return true;})),"section 'inventory': 1 bytes left unread","A section longer than its layout is refused");
  refused(wrap(rebuild(data,[](std::string& name,int& version,std::uint16_t&,std::string&){if(name=="props")version=9;return true;})),"this build reads up to 1","A core section newer than this build reads is refused");
  {auto payload=payloadOf(data);for(const auto& f:sectionFrames(payload,rangeChecksum))if(f.name=="player"){appendSection(payload,"player",1,0,payload.substr(f.offset,f.size));break;}
   refused(wrap(payload),"section 'player' appears twice","A section written twice is refused");}
  {auto payload=payloadOf(data);appendSection(payload,"zz.future",1,SectionRequired,"1234");refused(wrap(payload),"section 'zz.future' (version 1) is needed","An unknown section flagged required is refused: the save needs a newer build");}
  {auto payload=payloadOf(data);payload.push_back('\0');refused(wrap(payload),"bad name","A frame with no name is refused");}
  check(encode(live)==before,"Every refusal leaves the live game as it was");
  // ---- 5. Unknown sections are skipped ----
  {auto payload=payloadOf(data);appendSection(payload,"zz.future",3,0,std::string("\x01\x02\xff\x00 any bytes",14));const auto extended=wrap(payload);
   Game later(campaign);const bool loaded=later.decodeSave(extended);const auto refit=loaded?later.refitSaveFile(root/"none.rms",extended):SaveRefit{};
   check(loaded&&encode(later)==data&&refit.skipped==std::vector<std::string>{"zz.future v3"}&&!refit.rewritten,"An unknown section is skipped (and named for the load log); the game loads and re-saves without it",loaded?std::to_string(refit.skipped.size())+" skipped":lastLoadFailure());}
  {Game later(campaign);const bool loaded=later.decodeSave(wrap(rebuild(data,[](std::string& name,int& version,std::uint16_t&,std::string&){if(name=="variables")version=7;return true;})));
   check(loaded&&!later.mapVariable(-1,"probe")&&later.player().health==71,"A newer layout of an optional section is skipped like an unknown one",loaded?"":lastLoadFailure());}
 }

 // ---- 6. The section API: the entity and variable stores, a registered section, the physics hook ----
 {Game g(campaign);const auto plain=encode(g);
  check(!has(plain,"entities")&&!has(plain,"variables")&&!has(plain,"physics")&&has(plain,"props"),"Empty stores and an unhooked physics section are not written");
  const bool set=g.setEntityProperty(3,"door_lab","p.locked",true)&&g.setEntityProperty(3,"door_lab","p.code",std::int64_t(1234567890123))&&g.setEntityProperty(-1,entityIdKey("prop",17),"p.mass",2.5)
   &&g.setEntityProperty(0,"sign_\xc3\xbc","p.text",std::string("Ashfall \xc3\xbc\nline two"))&&g.setEntityProperty(0,"sign_\xc3\xbc","p.empty",std::string())
   &&g.setMapVariable(-1,"alarm_count",std::int64_t(-3))&&g.setMapVariable(0,"lights",std::string("off"))&&g.setMapVariable(0,"ratio",.125)&&g.setMapVariable(0,"done",false);
  const bool limits=!g.setMapVariable(0,"",true)&&!g.setMapVariable(0,std::string(129,'x'),true)&&!g.setMapVariable(0,"nan",std::numeric_limits<double>::quiet_NaN())&&!g.setMapVariable(-2,"x",true)&&!g.setMapVariable(0,"long",std::string(4097,'y'))
   &&!g.setEntityProperty(0,"bad\x01name","p",true)&&!g.mapVariable(0,"nan")&&g.setMapVariable(0,"temp",true)&&g.clearMapVariable(0,"temp")&&!g.mapVariable(0,"temp")
   &&g.setEntityProperty(1,"gone","p.x",true)&&g.clearEntityProperty(1,"gone","p.x")&&!g.entityProperties(1,"gone");
  check(set&&limits,"The stores take each kind of value and refuse empty, long, control-character and non-finite ones");
  const auto bytes=encode(g);Game back(campaign);const bool loaded=back.decodeSave(bytes);
  const auto* code=loaded?back.entityProperty(3,"door_lab","p.code"):nullptr;const auto* text=loaded?back.entityProperty(0,"sign_\xc3\xbc","p.text"):nullptr;
  check(loaded&&back.saveStore()==g.saveStore()&&encode(back)==bytes&&has(bytes,"entities")&&has(bytes,"variables")&&code&&std::get<std::int64_t>(*code)==1234567890123&&text&&std::get<std::string>(*text)=="Ashfall \xc3\xbc\nline two"&&back.entityProperties(3,"door_lab")->size()==2,
   "Entity properties and map variables round-trip through the save, byte-exact",std::to_string(g.saveStore().entities.size())+" entities, "+std::to_string(g.saveStore().variables.size())+" variables");
  // A system's own section, with an older layout upgraded.
  static int counter=0,upgradedFrom=0;static bool writing=false;
  auto counterSection=[]{return SaveSection{.name="test.counter",.version=2,
   .write=[](Game&,SaveOut& out){if(writing){int v=counter;out(v);}},.read=[](Game&,SaveIn& in,int version){int v=0;in(v);counter=version==1?v*10:v;},.upgrade=[](Game&,int from){upgradedFrom=from;}};};
  registerSaveSection(counterSection());registerSaveSection(counterSection());
  bool refusedName=false;try{registerSaveSection({.name="bad name!",.write=[](Game&,SaveOut&){},.read=[](Game&,SaveIn&,int){}});}catch(const std::invalid_argument&){refusedName=true;}
  const auto all=saveSectionNames();
  writing=true;counter=7;const auto withCounter=encode(g);counter=0;upgradedFrom=0;Game c1(campaign);const bool c1ok=c1.decodeSave(withCounter);const int afterCurrent=counter;
  counter=0;Game c2(campaign);const bool c2ok=c2.decodeSave(wrap(rebuild(withCounter,[](std::string& name,int& version,std::uint16_t&,std::string&){if(name=="test.counter")version=1;return true;})));
  writing=false;const bool dropped=!has(encode(g),"test.counter");
  check(refusedName&&std::count(all.begin(),all.end(),"test.counter")==1&&all.back()=="test.counter"&&has(withCounter,"test.counter")&&c1ok&&afterCurrent==7&&c2ok&&counter==70&&upgradedFrom==1&&dropped,
   "A registered section is written, read, upgraded from its older layout, replaced by name and left out when it writes nothing");
  // The physics section, through the hooks the rigid-body solver sets.
  {static std::vector<PropBody> restored;auto map=regression::room();map->clutterSpawns.push_back({7,{8,8},0,0,27,.42f,.4f,.38f});auto props=regression::campaign(map);Game p(props);
   int prop=-1;for(int i=0;i<int(p.m_clutter.size());++i)if(p.m_clutter[size_t(i)].model>=0){prop=i;break;}
   if(prop>=0){auto&c=p.m_clutter[size_t(prop)];c.yaw=.6f;c.spin=1.5f;c.sleeping=true;}
   const auto keptHooks=physicsSaveHooks();physicsSaveHooks()={[](const Clutter& c,PropBody& b){if(c.model<0)return false;b.orientation={0,0,std::sin(c.yaw*.5f),std::cos(c.yaw*.5f)};b.angularVelocity={0,0,c.spin};b.island=c.sleeping?3:-1;return true;},
                       [](Clutter&,const PropBody& b){restored.push_back(b);}};
   const auto bytes=encode(p);restored.clear();Game q(props);const bool ok=q.decodeSave(bytes);const auto again=ok?encode(q):std::string();
   const bool body=restored.size()==1&&std::fabs(restored[0].orientation[2]-std::sin(.3f))<1e-6f&&std::fabs(restored[0].orientation[3]-std::cos(.3f))<1e-6f&&restored[0].angularVelocity[2]==1.5f&&restored[0].island==3;
   const auto broken=wrap(rebuild(bytes,[](std::string& name,int&,std::uint16_t&,std::string& b){if(name=="physics"){const float w=2;std::memcpy(b.data()+b.size()-17,&w,4);}return true;}));
   Game bad(props);const bool badLoaded=bad.decodeSave(broken);const auto why=badLoaded?std::string("(loaded)"):lastLoadFailure();
   physicsSaveHooks()={};Game plainLoad(props);const bool plainOk=plainLoad.decodeSave(bytes);const bool unhooked=plainOk&&!has(encode(plainLoad),"physics");physicsSaveHooks()=keptHooks;
   check(prop>=0&&has(bytes,"physics")&&ok&&body&&again==bytes,"The physics section carries a prop's orientation, angular velocity and sleep island to the solver's restore hook",std::to_string(restored.size())+" bodies");
   check(!badLoaded&&contains(why,"section 'physics': invalid prop body"),"A prop body whose quaternion is not unit is refused",why);
   check(unhooked,"Without the solver's hooks a saved physics section is read, checked and dropped");
   {physicsSaveHooks()={nullptr,[](Clutter&,const PropBody& b){restored.push_back(b);}};restored.clear();Game old(props);const bool oldOk=old.decodeSave(wrap(rebuild(bytes,[](std::string& name,int& version,std::uint16_t&,std::string& b){if(name=="physics"){version=1;b.pop_back();}return true;}) /* layout 1: the bodies alone, without layout 2's chained-prop count (here none, one byte) */));physicsSaveHooks()=keptHooks;
    check(oldOk&&restored.size()==1,"A physics section from before joints (version 1) still loads",oldOk?std::to_string(restored.size())+" bodies":lastLoadFailure());}}
 }

 // ---- 7. The save folder 1.0.4-1.5.0 wrote by mistake is adopted, in a temp folder ----
 {const auto local=root/"local";const std::filesystem::path wrong(local.wstring()+L"RawMetalsaves"),wrongSettings(local.wstring()+L"RawMetalsettings.ini"),right=local/"RawMetal"/"saves",settings=local/"RawMetal"/"settings.ini";
  std::filesystem::create_directories(wrong,ignored);std::filesystem::create_directories(right,ignored);
  const auto now=std::filesystem::file_time_type::clock::now();auto at=[&](const std::filesystem::path& p,const std::string& data,int hoursAgo){put(p,data);std::filesystem::last_write_time(p,now-std::chrono::hours(hoursAgo),ignored);};
  at(wrong/"checkpoint.rms","newer checkpoint",1);at(right/"checkpoint.rms","older checkpoint",48);
  at(wrong/"slot-1.rms","only here",2);at(wrong/"slot-1.rms.label","MAP 3",2);
  at(wrong/"slot-2.rms","stale",72);at(right/"slot-2.rms","current",3);
  at(wrong/"checkpoint.rms.tmp","half written",1);
  at(wrongSettings,"0.5 0.5 0.5 1 0 1 1",1);
  adoptMisplacedSaves(local.wstring());
  const bool first=get(right/"checkpoint.rms")=="newer checkpoint"&&get(right/"checkpoint.rms.older.bak")=="older checkpoint"&&get(right/"slot-1.rms")=="only here"&&get(right/"slot-1.rms.label")=="MAP 3"
   &&get(right/"slot-2.rms")=="current"&&!std::filesystem::exists(right/"slot-2.rms.older.bak",ignored)&&!std::filesystem::exists(right/"checkpoint.rms.tmp",ignored)&&get(settings)=="0.5 0.5 0.5 1 0 1 1"
   &&get(wrong/"checkpoint.rms")=="newer checkpoint"&&get(wrong/"slot-2.rms")=="stale"&&get(wrongSettings)=="0.5 0.5 0.5 1 0 1 1";
  adoptMisplacedSaves(local.wstring());
  const bool second=get(right/"checkpoint.rms")=="newer checkpoint"&&!std::filesystem::exists(right/"checkpoint.rms.older-2.bak",ignored)&&get(right/"checkpoint.rms.older.bak")=="older checkpoint";
  check(first,"Saves and settings in the mistaken %LOCALAPPDATA%RawMetalsaves come into RawMetal\\saves; a newer file there is kept; one replaced is kept as .older.bak; the old folder is left as it was");
  check(second,"A second start adopts nothing more");
 }
 }
 std::filesystem::remove_all(root,ignored);
 report<<(passed?"PASS":"FAIL")<<" save format\n";
 return passed;
}
}

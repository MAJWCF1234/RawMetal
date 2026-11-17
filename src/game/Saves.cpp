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
  list(clutterList,[&](Clutter&v){vec(v.pos);vec(v.velocity);a(v.z,v.vz,v.yaw,v.spin,v.kind,v.projectile,v.impactCooldown,v.pitch,v.roll,v.pitchSpeed,v.rollSpeed,v.restTime,v.sleeping);if(v.kind<0||v.kind>(version>=17?7:6))throw std::runtime_error("invalid clutter");if(version>=17){a(v.model,v.damage,v.authored,v.broken,v.homeZ);vec(v.home);for(auto&d:v.dims)a(d);if((v.kind==7)!=(v.model>=0)||v.model>=FacilityModelCount||v.damage<0||(v.kind==7&&std::any_of(v.dims.begin(),v.dims.end(),[](float d){return d<=0||d>3.f;})))throw std::runtime_error("invalid prop");}
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
 auto add=[&](const char* name,bool core,auto body){
  list.push_back({.name=name,.version=1,.core=core,
   .write=[body](Game& g,SaveOut& out){body(g,out,1);},
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
  if constexpr(std::decay_t<decltype(a)>::reading)for(size_t i=1;i<g.m_states.size();++i)if(g.m_states[i-1].id>=g.m_states[i].id)throw std::runtime_error("unsorted or duplicate saved state");
  archiveList(a,g.m_objectives,[&](StateValue&v){a(v.id,v.value);if(v.id==0||v.value<0||v.value>3)throw std::runtime_error("invalid objective");});
  archiveList(a,g.m_firedEvents,[&](StateId&id){a(id);if(id==0)throw std::runtime_error("invalid event id");});});
 // Each chunk in turn: its world (lift and reactor, broken tiles and breakables, pane damage and
 // glass hits, doors, torn leads), its creatures, its pickups, its props and their pieces.
 auto chunks=[&](const char* name,unsigned part){add(name,true,[part](Game& g,auto& a,int){
  int count=std::decay_t<decltype(a)>::reading?0:g.chunkCount();a(count);if(count!=t_sectionChunks&&std::decay_t<decltype(a)>::reading)throw std::runtime_error("holds "+std::to_string(count)+" maps, the save "+std::to_string(t_sectionChunks));
  for(int index=0;index<count;++index)try{g.archiveChunk(a,SaveVersion,index,part);}catch(const std::exception& error){throw std::runtime_error("map "+std::to_string(index)+": "+error.what());}});};
 chunks("world",ChunkWorld);chunks("creatures",ChunkCreatures);chunks("pickups",ChunkPickups);chunks("props",ChunkProps);
 // Runtime changes to entities' properties (the map's named entities and their `p.` properties).
 add("entities",false,[](Game& g,auto& a,int){auto&store=g.m_saveStore.entities;int count=int(store.size());a(count);
  if(count<0||size_t(count)>SaveStore::MaxEntities)throw std::runtime_error("invalid entity count");
  if constexpr(std::decay_t<decltype(a)>::reading){for(int i=0;i<count;++i){int chunk=0;std::string name;a(chunk);a.text(name,SaveStore::MaxName);
    if(chunk<-1||chunk>=MaxChunks||name.empty())throw std::runtime_error("invalid entity");int properties=0;a(properties);if(properties<1||size_t(properties)>SaveStore::MaxProperties)throw std::runtime_error("invalid property count");
    auto [at,fresh]=store.try_emplace(SaveStore::Key{chunk,name});if(!fresh)throw std::runtime_error("entity '"+name+"' twice");
    for(int p=0;p<properties;++p){std::string key;a.text(key,SaveStore::MaxName);SaveValue value;a.value(value,SaveStore::MaxText);if(key.empty()||!at->second.try_emplace(key,std::move(value)).second)throw std::runtime_error("invalid property of '"+name+"'");}}}
  else for(auto& [key,properties]:store){int chunk=key.first;std::string name=key.second;a(chunk);a.text(name);int n=int(properties.size());a(n);for(auto& [property,value]:properties){std::string k=property;auto v=value;a.text(k);a.value(v);}}});
 // Named values scripts set, per map or for the campaign.
 add("variables",false,[](Game& g,auto& a,int){auto&store=g.m_saveStore.variables;int count=int(store.size());a(count);
  if(count<0||size_t(count)>SaveStore::MaxVariables)throw std::runtime_error("invalid variable count");
  if constexpr(std::decay_t<decltype(a)>::reading){for(int i=0;i<count;++i){int chunk=0;std::string name;a(chunk);a.text(name,SaveStore::MaxName);SaveValue value;a.value(value,SaveStore::MaxText);
    if(chunk<-1||chunk>=MaxChunks||name.empty()||!store.try_emplace(SaveStore::Key{chunk,name},std::move(value)).second)throw std::runtime_error("invalid variable '"+name+"'");}}
  else for(auto& [key,value]:store){int chunk=key.first;std::string name=key.second;auto v=value;a(chunk);a.text(name);a.value(v);}});
 // The rigid-body solver's state for props (PropBody: orientation, angular velocity, sleep
 // island), per chunk, by each prop's place in the chunk's props. Written only once the solver
 // sets physicsSaveHooks(); read before the reconciliation, which keeps the props' fields.
 add("physics",false,[](Game& g,auto& a,int){const auto& hooks=physicsSaveHooks();
  auto norm=[](const PropBody& b){return std::sqrt(b.orientation[0]*b.orientation[0]+b.orientation[1]*b.orientation[1]+b.orientation[2]*b.orientation[2]+b.orientation[3]*b.orientation[3]);};
  // Written: a unit quaternion (a degenerate one is the identity), spin clamped to 1000 rad/s.
  // Read: refused unless near unit and in range, then made exactly unit.
  auto body=[&](PropBody& b){
   if constexpr(!std::decay_t<decltype(a)>::reading){const float length=norm(b);if(std::isfinite(length)&&length>1e-6f)for(auto&q:b.orientation)q/=length;else b.orientation={0,0,0,1};
    for(auto&w:b.angularVelocity)w=std::isfinite(w)?std::clamp(w,-1000.f,1000.f):0.f;b.island=std::max(b.island,-1);}
   for(auto&q:b.orientation)a(q);for(auto&w:b.angularVelocity)a(w);a(b.island);
   if constexpr(std::decay_t<decltype(a)>::reading){const float length=norm(b);
    if(!(length>.9f&&length<1.1f)||std::any_of(b.angularVelocity.begin(),b.angularVelocity.end(),[](float w){return std::fabs(w)>1000.f;})||b.island<-1)throw std::runtime_error("invalid prop body");
    for(auto&q:b.orientation)q/=length;}};
  if constexpr(std::decay_t<decltype(a)>::reading){int chunks=0;a(chunks);if(chunks<0||chunks>t_sectionChunks)throw std::runtime_error("invalid chunk count");int last=-1;
   for(int i=0;i<chunks;++i){int level=0,count=0;a(level,count);if(level<=last||level>=t_sectionChunks)throw std::runtime_error("invalid chunk");last=level;auto&props=g.m_chunks[size_t(level)].clutter;
    if(count<1||size_t(count)>props.size())throw std::runtime_error("map "+std::to_string(level)+": invalid prop count");int lastProp=-1;
    for(int k=0;k<count;++k){int index=0;a(index);if(index<=lastProp||size_t(index)>=props.size())throw std::runtime_error("map "+std::to_string(level)+": invalid prop index");lastProp=index;PropBody b;body(b);if(hooks.restore)hooks.restore(props[size_t(index)],b);}}}
  else{if(!hooks.capture)return;std::vector<std::pair<int,std::vector<std::pair<int,PropBody>>>> captured;
   for(int level=0;level<g.chunkCount();++level){std::vector<std::pair<int,PropBody>> bodies;const auto&props=g.m_chunks[size_t(level)].clutter;
    for(size_t i=0;i<props.size();++i){PropBody b;if(hooks.capture(props[i],b))bodies.push_back({int(i),b});}if(!bodies.empty())captured.push_back({level,std::move(bodies)});}
   if(captured.empty())return;int chunks=int(captured.size());a(chunks);
   for(auto& [level,bodies]:captured){int l=level,count=int(bodies.size());a(l,count);for(auto& [index,b]:bodies){int i=index;a(i);body(b);}}}});
 return list;
}
// ---- The stores the "entities" and "variables" sections keep ----
namespace {
bool storeName(std::string_view name){return !name.empty()&&name.size()<=SaveStore::MaxName&&std::none_of(name.begin(),name.end(),[](char c){return static_cast<unsigned char>(c)<32;});}
bool storeValue(const SaveValue& value){if(auto* d=std::get_if<double>(&value))return std::isfinite(*d);if(auto* s=std::get_if<std::string>(&value))return s->size()<=SaveStore::MaxText;return true;}
}
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
  // 28: earlier builds represented a broken crate as four unrelated prop models (kinds 0-3) spawned
  // together. That recognizable quartet becomes the current textured splinters.
  {28,[](Game& g){for(auto&chunk:g.m_chunks)migrateCrateDebris(chunk.clutter);}},
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
     if(c.model>=0&&c.authored)if(const auto*spawn=freshProp(c)){c.breakState=spawn->breakState;keep=true;}
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
 // A late save is big: past the old 2 MB cap and past 1024 pieces in one map, it still loads.
 {Game big(regression::campaign(regression::room()));for(int i=0;i<16000;++i){Clutter c{};c.pos={2.0123457f+float(i%180)*.1f,2.0123457f+float(i/180)*.25f};c.z=.0123457f;c.yaw=c.pitch=c.roll=.1234567f*float(i%7);c.restTime=1.2345678f;c.sleeping=true;big.m_clutter.push_back(c);}
  const auto bytes=big.encodeSave();Game loaded(big.m_customCampaign);if(bytes.size()<=2u*1024*1024||!loaded.decodeSave(bytes)||loaded.m_clutter.size()<16000){std::ofstream("prop-saves-big.txt")<<"bytes "<<bytes.size()<<" loaded "<<loaded.m_clutter.size()<<" "<<lastLoadFailure()<<std::endl;return false;}}
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

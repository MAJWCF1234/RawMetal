#include "Game.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <type_traits>
#include <stdexcept>

namespace retro {
namespace {
constexpr size_t MaxSaveBytes=2*1024*1024;
uint32_t checksum(const std::string& data){uint32_t hash=2166136261u;for(unsigned char c:data){hash^=c;hash*=16777619u;}return hash;}
struct Writer {
 static constexpr bool reading=false;
 std::ostringstream stream;
 Writer(){stream.imbue(std::locale::classic());stream<<std::setprecision(std::numeric_limits<float>::max_digits10);}
 template<class T> void one(T& v){if constexpr(std::is_enum_v<T>)stream<<int(v);else stream<<v;stream<<' ';}
 template<class... T> void operator()(T&... v){(one(v),...);stream<<'\n';}
};
struct Reader {
 static constexpr bool reading=true;
 std::istringstream stream;
 explicit Reader(const std::string& s):stream(s){stream.imbue(std::locale::classic());}
 template<class T> void one(T& v){
  if constexpr(std::is_enum_v<T>||std::is_same_v<T,bool>){int n=0;if(!(stream>>n)||n<0||n>(std::is_same_v<T,bool>?1:16))throw std::runtime_error("invalid enum");v=static_cast<T>(n);}
  else {if(!(stream>>v)||!std::isfinite(double(v))||std::fabs(double(v))>10000000)throw std::runtime_error("invalid number");}
 }
 template<class... T> void operator()(T&... v){(one(v),...);}
};
std::filesystem::path slotPath(const std::wstring& directory,int slot){return std::filesystem::path(directory)/("slot-"+std::to_string(slot+1)+".rms");}
std::string readFile(const std::filesystem::path& path){
 std::error_code error;auto size=std::filesystem::file_size(path,error);if(error||size>MaxSaveBytes)throw std::runtime_error("missing or oversized save");
 std::ifstream in(path,std::ios::binary);std::string data(size,'\0');if(!in.read(data.data(),std::streamsize(size)))throw std::runtime_error("unreadable save");return data;
}
}
template<class A> void Game::archiveSave(A& a,int version){
 auto vec=[&](Vec2& v){a(v.x,v.y);};
 a(m_level,m_elapsed,m_won,m_medkits,m_weaponEquipped,m_heldClutter);
 if(version>=4){a(m_hazmat.initialized,m_hazmat.sleeping,m_hazmat.quiet,m_hazmat.accumulator);
  for(auto&points:{&m_hazmat.p,&m_hazmat.previous})for(auto&p:*points){a(p.x,p.y,p.z);if(p.x<0||p.x>24||p.y<0||p.y>24||p.z<-12||p.z>20)throw std::runtime_error("Invalid hazmat pose");}
  if(m_hazmat.accumulator<0||m_hazmat.accumulator>.06f||m_hazmat.quiet<0||m_hazmat.quiet>10)throw std::runtime_error("Invalid hazmat simulation");
 }
 auto&p=m_player;vec(p.pos);a(p.angle,p.pitch,p.health,p.ammo);if(version>=2)a(p.loaded);a(p.z,p.verticalVelocity,p.grounded,p.crouched,p.eye);vec(m_velocity);
 a(m_weaponKick,m_shotCooldown);if(version>=2)a(m_reloadTimer);a(m_shotAge,m_holster,m_punchAge,m_punchLeft,m_guarding,m_verticalSpring,m_verticalSpringVelocity,m_stepDistance,m_stepVariant,m_jumpBuffer,m_coyote);
 a(m_weaponMotion.yaw,m_weaponMotion.pitch,m_weaponMotion.bob,m_weaponMotion.back,m_weaponMotion.elbow,m_weaponMotion.bolt,m_weaponMotion.roll);vec(m_sway);vec(m_swayVelocity);a(m_elbowVelocity);
 for(auto& cell:m_itemCells)a(cell);
 auto list=[&](auto& values,auto visit){int count=int(values.size());a(count);if(count<0||count>1024)throw std::runtime_error("invalid collection");if constexpr(A::reading)values.resize(count);for(auto&v:values)visit(v);};
 for(int index=0;index<ChunkCount;++index){auto&c=m_chunks[index];if constexpr(A::reading)c.world=World(index);auto&w=c.world;
  a(c.kills,c.resident,w.m_controlReleased,w.m_liftPhase,w.m_liftHeight,w.m_liftTimer,w.m_liftVelocity,w.m_liftCaught,w.m_reactorStage,w.m_reactorFault);
  int doors=int(w.m_doors.size());a(doors);if(doors<0||doors>128)throw std::runtime_error("invalid door count");
  if constexpr(A::reading){
   for(int door=0;door<doors;++door){float open=0;bool opening=false;a(open,opening);if(open<0||open>1)throw std::runtime_error("invalid door");
    if(door<int(w.m_doors.size())){w.m_doors[door].open=open;w.m_doors[door].opening=opening;}}
  }else for(auto&d:w.m_doors){a(d.open,d.opening);if(d.open<0||d.open>1)throw std::runtime_error("invalid door");}
  list(c.enemies,[&](Enemy&e){vec(e.pos);a(e.hp,e.attackCooldown,e.painFlash,e.alive,e.kind,e.maxHp,e.deathTime,e.windup,e.strike,e.heading,e.gait,e.moving,e.voiceTimer,e.stepTimer,e.z,e.awareness,e.searchTime,e.verticalVelocity,e.repathTimer,e.lastKnownZ);vec(e.waypoint);vec(e.home);vec(e.lastKnown);a(e.state);if(version>=5)a(e.stalkMode,e.stalkTimer,e.stalkSide);if(int(e.kind)>(version>=3?3:2)||int(e.state)>3||int(e.stalkMode)>2||e.stalkTimer<0||e.stalkTimer>60||std::fabs(e.stalkSide)>1.01f||e.maxHp<=0)throw std::runtime_error("invalid enemy");});
  list(c.pickups,[&](Pickup&v){vec(v.pos);a(v.kind,v.active);if(int(v.kind)>1)throw std::runtime_error("invalid pickup");});
  list(c.clutter,[&](Clutter&v){vec(v.pos);vec(v.velocity);a(v.z,v.vz,v.yaw,v.spin,v.kind,v.projectile,v.impactCooldown,v.pitch,v.roll,v.pitchSpeed,v.rollSpeed,v.restTime,v.sleeping);if(v.kind<0||v.kind>5)throw std::runtime_error("invalid clutter");});
  if(int(w.m_liftPhase)>int(World::LiftPhase::Crashed)||int(w.m_reactorStage)>int(World::ReactorStage::Released)||w.m_liftHeight<-9||w.m_liftHeight>9||w.m_liftTimer<0||c.kills<0)throw std::runtime_error("invalid world state");
  if constexpr(A::reading){w.restoreLift(w);if(!c.resident)w.unloadGeometry();}
 }
}
std::string Game::encodeSave()const{
 Game snapshot=*this;snapshot.m_player.loaded=std::clamp(snapshot.m_player.loaded,0,std::clamp(snapshot.m_player.ammo,0,6));snapshot.storeChunk();Writer writer;snapshot.archiveSave(writer,5);auto payload=writer.stream.str();
 return "RAWMETAL_SAVE 5 "+std::to_string(checksum(payload))+"\n"+payload;
}
bool Game::decodeSave(const std::string& data){
 try {
  if(data.size()>MaxSaveBytes)return false;auto split=data.find('\n');if(split==std::string::npos)return false;
  std::istringstream header(data.substr(0,split));std::string magic;int version=0;uint32_t hash=0;
  if(!(header>>magic>>version>>hash)||magic!="RAWMETAL_SAVE"||(version<1||version>5))return false;header>>std::ws;if(!header.eof())return false;
  auto payload=data.substr(split+1);if(checksum(payload)!=hash)return false;
  Game next;Reader reader(payload);next.archiveSave(reader,version);if(version==1){next.m_player.loaded=std::min(6,next.m_player.ammo);next.m_reloadTimer=0;}reader.stream>>std::ws;if(!reader.stream.eof())return false;

  // Saves store mutable gameplay state, but the executable owns the current
  // authored population. Reconcile old state onto today's baseline so content
  // added by an update appears in existing saves instead of being erased by
  // the older serialized vector lengths.
  Game authored;
  auto samePosition=[](Vec2 a,Vec2 b){return lengthSq(a-b)<.0004f;};
  for(int level=0;level<ChunkCount;++level){
   auto&saved=next.m_chunks[level];const auto&fresh=authored.m_chunks[level];

   auto oldEnemies=std::move(saved.enemies);std::vector<bool> enemyUsed(oldEnemies.size(),false);saved.enemies.clear();saved.enemies.reserve(fresh.enemies.size());
   for(const auto&spawn:fresh.enemies){
    int match=-1;for(int i=0;i<int(oldEnemies.size());++i)if(!enemyUsed[i]&&oldEnemies[i].kind==spawn.kind&&samePosition(oldEnemies[i].home,spawn.home)){match=i;break;}
    if(match>=0){enemyUsed[match]=true;auto state=oldEnemies[match];state.maxHp=spawn.maxHp;if(state.alive)state.hp=std::min(state.hp,state.maxHp);saved.enemies.push_back(state);}
    else saved.enemies.push_back(spawn);
   }

   auto oldPickups=std::move(saved.pickups);std::vector<bool> pickupUsed(oldPickups.size(),false);saved.pickups.clear();saved.pickups.reserve(fresh.pickups.size());
   for(const auto&spawn:fresh.pickups){
    int match=-1;for(int i=0;i<int(oldPickups.size());++i)if(!pickupUsed[i]&&oldPickups[i].kind==spawn.kind&&samePosition(oldPickups[i].pos,spawn.pos)){match=i;break;}
    if(match>=0){pickupUsed[match]=true;saved.pickups.push_back(oldPickups[match]);}else saved.pickups.push_back(spawn);
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
  if(next.m_level<0||next.m_level>=ChunkCount||next.m_elapsed<0||p.ammo<0||p.loaded<0||p.loaded>6||p.loaded>p.ammo||next.m_reloadTimer<0||next.m_reloadTimer>2||p.health>100||p.pos.x<-2||p.pos.x>26||p.pos.y<-2||p.pos.y>26||p.z<-100||p.z>100||p.eye<.1f||p.eye>1.1f||std::fabs(p.pitch)>100||next.m_medkits<0)return false;
  for(int i=0;i<3;++i){int cell=next.m_itemCells[i],width=i==0?4:i==1?1:2;if(cell<0||cell/6+2>5||cell%6+width>6)return false;}
  next.ensureChunk(next.m_level);auto&c=next.m_chunks[next.m_level];next.m_world=c.world;next.m_enemies=c.enemies;next.m_pickups=c.pickups;next.m_clutter=c.clutter;next.m_kills=c.kills;
  if(next.m_level==3&&!next.m_hazmat.initialized)next.m_hazmat.seed(next.m_world);
  if(next.m_heldClutter<-1||next.m_heldClutter>=int(next.m_clutter.size()))return false;
  next.m_settings=m_settings;next.m_audioMuted=m_audioMuted;next.m_musicEnabled=m_musicEnabled;next.m_showFps=m_showFps;next.m_renderScale=m_renderScale;next.m_saveDirectory=m_saveDirectory;
  next.m_sessionRevision=m_sessionRevision+1;next.m_suppressFire=true;next.m_previousUse=true;next.m_previousJump=true;next.m_previousEscape=true;next.m_menuPage=MenuPage::Settings;
  next.updateStreaming(0);*this=std::move(next);return true;
 }catch(const std::exception&){return false;}
}
bool Game::saveSlot(int slot){
 if(slot<0||slot>=3||m_saveDirectory.empty()){m_menuMessage="SAVE LOCATION UNAVAILABLE";return false;}
 try {
  auto path=slotPath(m_saveDirectory,slot),temp=path;temp+=L".tmp";
  std::filesystem::create_directories(path.parent_path());auto data=encodeSave();
  {std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(data.data(),std::streamsize(data.size()));file.flush();if(!file)throw std::runtime_error("write failed");file.close();if(file.fail())throw std::runtime_error("close failed");}
  // Same-volume replacement preserves the old slot if writing or replacement fails.
  if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("replace failed");
  refreshSaveSlots();m_menuMessage="GAME SAVED";return true;
 }catch(const std::exception&){m_menuMessage="SAVE FAILED / CHECK DISK AND FOLDER";return false;}
}
bool Game::loadSlot(int slot){
 if(slot<0||slot>=3||m_saveDirectory.empty()){m_menuMessage="SAVE LOCATION UNAVAILABLE";return false;}
 try{if(decodeSave(readFile(slotPath(m_saveDirectory,slot))))return true;}catch(const std::exception&){}
 m_menuMessage="LOAD FAILED / EMPTY OR INVALID SAVE";return false;
}
void Game::refreshSaveSlots(){
 for(int slot=0;slot<3;++slot){auto&label=m_slotLabels[slot];label="SLOT "+std::to_string(slot+1)+" / ";
  if(m_saveDirectory.empty()){label+="UNAVAILABLE";continue;}
  auto path=slotPath(m_saveDirectory,slot);std::error_code error;if(!std::filesystem::exists(path,error)){label+=error?"UNAVAILABLE":"EMPTY";continue;}
  try{Game preview;if(preview.decodeSave(readFile(path))){int minutes=int(preview.elapsed()/60);label+="MAP "+std::to_string(preview.level())+" / "+std::to_string(minutes)+" MIN / HP "+std::to_string(int(preview.player().health));}else label+="INVALID SAVE";}catch(const std::exception&){label+="UNREADABLE";}
 }
}
bool Game::testSaves(){
 std::ofstream report("save-test.txt");auto check=[&](bool ok,const char* label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();return ok;};
 for(float time:{0.f,7.f,21.f,34.f,39.5f,42.f,48.f}){
  auto original=liftInspection(time);original.m_player.health=63;original.m_player.ammo=17;original.m_medkits=2;original.m_weaponEquipped=false;original.m_world.setDoor(1,.35f,true);
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
      restored.player().health==original.player().health&&restored.player().ammo==original.player().ammo&&
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
 // Existing saves are reconciled with newly authored content. Simulate an
 // older build that did not yet contain the Reactor Stalker, the final pickup,
 // one clutter item, or the newest gantry transfer door.
 {Game legacy;legacy.m_chunks[3].enemies[0].alive=false;legacy.m_chunks[3].enemies[0].hp=0;
  legacy.m_chunks[3].enemies.pop_back();legacy.m_chunks[3].pickups[0].active=false;legacy.m_chunks[3].pickups.pop_back();
  legacy.m_chunks[3].clutter[0].pos={9.25f,18.75f};legacy.m_chunks[3].clutter.pop_back();
  legacy.m_chunks[2].world.m_doors.pop_back();
  Writer writer;legacy.archiveSave(writer,5);auto payload=writer.stream.str();
  auto data=std::string("RAWMETAL_SAVE 5 ")+std::to_string(checksum(payload))+"\n"+payload;Game restored;
  if(!check(restored.decodeSave(data),"Older content save loads after authored additions"))return false;
  auto&reactor=restored.m_chunks[3];
  bool hasWarden=std::any_of(reactor.enemies.begin(),reactor.enemies.end(),[](const Enemy&e){return e.kind==Enemy::Kind::Warden&&e.alive;});
  bool oldDeathPreserved=!reactor.enemies.empty()&&!reactor.enemies[0].alive;
  bool collectedPreserved=!reactor.pickups.empty()&&!reactor.pickups[0].active;
  bool movedClutterPreserved=!reactor.clutter.empty()&&lengthSq(reactor.clutter[0].pos-Vec2{9.25f,18.75f})<.0001f;
  if(!check(hasWarden&&oldDeathPreserved&&collectedPreserved&&movedClutterPreserved&&reactor.pickups.size()==Game{}.m_chunks[3].pickups.size()&&reactor.clutter.size()==Game{}.m_chunks[3].clutter.size(),"New enemies/items spawn while old dynamic state survives"))return false;
  if(!check(restored.m_chunks[2].world.doors().size()==Game{}.m_chunks[2].world.doors().size(),"New doors use current map defaults instead of invalidating old saves"))return false;
 }
 // Version 4 saves remain loadable; new stalk state falls back to safe defaults.
 {Game legacy=stalkerInspection(0,0,0);legacy.storeChunk();Writer writer;legacy.archiveSave(writer,4);auto payload=writer.stream.str();
  auto data=std::string("RAWMETAL_SAVE 4 ")+std::to_string(checksum(payload))+"\n"+payload;Game restored;
  if(!check(restored.decodeSave(data)&&!restored.m_enemies.empty()&&restored.m_enemies.back().stalkMode==Enemy::StalkMode::Watch&&restored.m_enemies.back().stalkTimer==0&&restored.m_enemies.back().stalkSide==1,"Version 4 saves load with default stalk state"))return false;
 }
 Game game;auto pristine=game.encodeSave();auto corrupt=pristine;corrupt.back()='x';
 if(!check(!game.decodeSave(corrupt)&&!game.decodeSave(pristine.substr(0,pristine.size()/2))&&!game.decodeSave("RAWMETAL_SAVE 99 0\n")&&game.encodeSave()==pristine,"Corrupt / truncated / future saves leave the live game untouched"))return false;
 auto invalid=game;invalid.m_player.ammo=-1;if(!check(!game.decodeSave(invalid.encodeSave()),"Valid-checksum invalid gameplay data rejected"))return false;
 game.m_heldClutter=0;game.m_clutter[0].projectile=true;game.m_clutter[0].velocity={2,3};Game held;if(!check(held.decodeSave(game.encodeSave())&&held.holdingClutter()&&held.m_clutter[0].velocity.x==2,"Held and moving clutter roundtrip"))return false;
 auto directory=std::filesystem::current_path()/("save-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));game.setSaveDirectory(directory.wstring());
 struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;for(int i=0;i<3;++i){auto p=slotPath(path.wstring(),i);std::filesystem::remove(p,e);p+=L".tmp";std::filesystem::remove(p,e);}std::filesystem::remove(path,e);}} cleanup{directory};
 InputState escape{};escape.escape=true;game.update(escape,.01f);game.update({},.01f);
 auto click=[&](int row){game.update({},.01f);InputState i{};i.pointerX=MenuLayout::X+30;i.pointerY=MenuLayout::RowTop+row*MenuLayout::RowHeight+7;i.fire=true;game.update(i,.01f);};
 click(7);if(!check(game.paused()&&game.menuPage()==MenuPage::Save,"Esc menu opens Save submenu"))return false;
 auto time=game.elapsed();click(0);if(!check(std::filesystem::exists(slotPath(directory.wstring(),0))&&game.elapsed()==time&&game.menuMessage()=="GAME SAVED","Save slot writes while simulation stays paused"))return false;
 auto before=readFile(slotPath(directory.wstring(),0));game.m_player.ammo=5;click(0);if(!check(game.menuPage()==MenuPage::Overwrite,"Existing slot requires overwrite confirmation"))return false;
 click(0);if(!check(readFile(slotPath(directory.wstring(),0))==before,"Cancel preserves old save"))return false;
 click(0);click(1);if(!check(game.menuPage()==MenuPage::Save&&readFile(slotPath(directory.wstring(),0))!=before,"Confirmed overwrite replaces slot"))return false;
 {auto path=slotPath(directory.wstring(),0),temp=path;temp+=L".tmp";auto saved=readFile(path);std::filesystem::create_directory(temp);
  bool safe=!game.saveSlot(0)&&readFile(path)==saved;std::filesystem::remove(temp);if(!check(safe,"Failed temporary write preserves the previous slot"))return false;}
 game.update({},.01f);game.update(escape,.01f);if(!check(game.paused()&&game.menuPage()==MenuPage::Settings,"Esc returns from submenu without resuming"))return false;
 click(8);click(2);click(1);if(!check(game.paused()&&game.menuPage()==MenuPage::Load&&game.menuMessage().find("FAILED")!=std::string::npos,"Empty slot reports load error without losing progress"))return false;
 click(0);click(0);if(!check(game.menuPage()==MenuPage::Load,"Load confirmation can be cancelled"))return false;
 game.m_player.ammo=1;auto revision=game.sessionRevision();click(0);click(1);
 if(!check(!game.paused()&&game.player().ammo==5&&game.sessionRevision()>revision,"Confirmed load resumes and resets audio session"))return false;
 InputState fire{};fire.fire=true;game.update(fire,.01f);if(!check(game.player().ammo==5,"Load click cannot fire the weapon"))return false;
 for(int slot=1;slot<3;++slot){if(!check(game.saveSlot(slot),"All three slots support independent saves"))return false;}
 return check(!game.saveSlot(-1)&&!game.loadSlot(3),"Invalid slots are rejected");
}
}

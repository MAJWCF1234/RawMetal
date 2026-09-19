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
template<class A> void Game::archiveSave(A& a){
 auto vec=[&](Vec2& v){a(v.x,v.y);};
 a(m_level,m_elapsed,m_won,m_medkits,m_weaponEquipped,m_heldClutter);
 auto&p=m_player;vec(p.pos);a(p.angle,p.pitch,p.health,p.ammo,p.z,p.verticalVelocity,p.grounded,p.crouched,p.eye);vec(m_velocity);
 a(m_weaponKick,m_shotCooldown,m_shotAge,m_holster,m_punchAge,m_punchLeft,m_guarding,m_verticalSpring,m_verticalSpringVelocity,m_stepDistance,m_stepVariant,m_jumpBuffer,m_coyote);
 a(m_weaponMotion.yaw,m_weaponMotion.pitch,m_weaponMotion.bob,m_weaponMotion.back,m_weaponMotion.elbow,m_weaponMotion.bolt,m_weaponMotion.roll);vec(m_sway);vec(m_swayVelocity);a(m_elbowVelocity);
 for(auto& cell:m_itemCells)a(cell);
 auto list=[&](auto& values,auto visit){int count=int(values.size());a(count);if(count<0||count>1024)throw std::runtime_error("invalid collection");if constexpr(A::reading)values.resize(count);for(auto&v:values)visit(v);};
 for(int index=0;index<ChunkCount;++index){auto&c=m_chunks[index];if constexpr(A::reading)c.world=World(index);auto&w=c.world;
  a(c.kills,c.resident,w.m_controlReleased,w.m_liftPhase,w.m_liftHeight,w.m_liftTimer,w.m_liftVelocity,w.m_liftCaught,w.m_reactorStage,w.m_reactorFault);
  int doors=int(w.m_doors.size());a(doors);if(doors!=int(w.m_doors.size()))throw std::runtime_error("door schema mismatch");for(auto&d:w.m_doors){a(d.open,d.opening);if(d.open<0||d.open>1)throw std::runtime_error("invalid door");}
  list(c.enemies,[&](Enemy&e){vec(e.pos);a(e.hp,e.attackCooldown,e.painFlash,e.alive,e.kind,e.maxHp,e.deathTime,e.windup,e.strike,e.heading,e.gait,e.moving,e.voiceTimer,e.stepTimer,e.z,e.awareness,e.searchTime,e.verticalVelocity,e.repathTimer,e.lastKnownZ);vec(e.waypoint);vec(e.home);vec(e.lastKnown);a(e.state);if(int(e.kind)>2||int(e.state)>3||e.maxHp<=0)throw std::runtime_error("invalid enemy");});
  list(c.pickups,[&](Pickup&v){vec(v.pos);a(v.kind,v.active);if(int(v.kind)>1)throw std::runtime_error("invalid pickup");});
  list(c.clutter,[&](Clutter&v){vec(v.pos);vec(v.velocity);a(v.z,v.vz,v.yaw,v.spin,v.kind,v.projectile,v.impactCooldown,v.pitch,v.roll,v.pitchSpeed,v.rollSpeed,v.restTime,v.sleeping);if(v.kind<0||v.kind>5)throw std::runtime_error("invalid clutter");});
  if(int(w.m_liftPhase)>int(World::LiftPhase::Crashed)||int(w.m_reactorStage)>int(World::ReactorStage::Released)||w.m_liftHeight<-9||w.m_liftHeight>9||w.m_liftTimer<0||c.kills<0)throw std::runtime_error("invalid world state");
  if constexpr(A::reading){w.restoreLift(w);if(!c.resident)w.unloadGeometry();}
 }
}
std::string Game::encodeSave()const{
 Game snapshot=*this;snapshot.storeChunk();Writer writer;snapshot.archiveSave(writer);auto payload=writer.stream.str();
 return "RAWMETAL_SAVE 1 "+std::to_string(checksum(payload))+"\n"+payload;
}
bool Game::decodeSave(const std::string& data){
 try {
  if(data.size()>MaxSaveBytes)return false;auto split=data.find('\n');if(split==std::string::npos)return false;
  std::istringstream header(data.substr(0,split));std::string magic;int version=0;uint32_t hash=0;
  if(!(header>>magic>>version>>hash)||magic!="RAWMETAL_SAVE"||version!=1)return false;header>>std::ws;if(!header.eof())return false;
  auto payload=data.substr(split+1);if(checksum(payload)!=hash)return false;
  Game next;Reader reader(payload);next.archiveSave(reader);reader.stream>>std::ws;if(!reader.stream.eof())return false;
  auto&p=next.m_player;
  if(next.m_level<0||next.m_level>=ChunkCount||next.m_elapsed<0||p.ammo<0||p.health>100||p.pos.x<-2||p.pos.x>26||p.pos.y<-2||p.pos.y>26||p.z<-100||p.z>100||p.eye<.1f||p.eye>1.1f||std::fabs(p.pitch)>100||next.m_medkits<0)return false;
  for(int i=0;i<3;++i){int cell=next.m_itemCells[i],width=i==0?4:i==1?1:2;if(cell<0||cell/6+2>5||cell%6+width>6)return false;}
  next.ensureChunk(next.m_level);auto&c=next.m_chunks[next.m_level];next.m_world=c.world;next.m_enemies=c.enemies;next.m_pickups=c.pickups;next.m_clutter=c.clutter;next.m_kills=c.kills;
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
  original.m_chunks[1].enemies[0].alive=false;original.m_chunks[1].enemies[0].hp=0;original.m_chunks[1].pickups[0].active=false;original.m_chunks[1].kills=1;
  if(time==48){original.m_world.takeReactorDisk();original.m_world.useReactorTerminal(1);original.m_world.useReactorTerminal(2);}
  auto data=original.encodeSave();Game restored;restored.m_settings.master=.4f;
  if(!check(restored.decodeSave(data)&&restored.encodeSave()==data&&restored.settings().master==.4f,"Exact dynamic-state roundtrip / settings preserved"))return false;
  auto phase=original.world().liftPhase();restored.update({},.01f);original.update({},.01f);
  if(!check(restored.world().liftPhase()==original.world().liftPhase()&&std::fabs(restored.player().z-original.player().z)<.0001f,"Loaded lift resumes without moving the passenger incorrectly"))return false;
  if(time==39.5f&&!check(phase==World::LiftPhase::Caught,"Brake-catch save is covered"))return false;
  if(time==48){restored.m_world.useReactorTerminal(3);restored.m_world.useReactorTerminal(1);if(!check(restored.world().controlReleased(),"Loaded reactor puzzle can finish"))return false;}
 }
 Game game;auto pristine=game.encodeSave();auto corrupt=pristine;corrupt.back()='x';
 if(!check(!game.decodeSave(corrupt)&&!game.decodeSave(pristine.substr(0,pristine.size()/2))&&!game.decodeSave("RAWMETAL_SAVE 99 0\n")&&game.encodeSave()==pristine,"Corrupt / truncated / future saves leave the live game untouched"))return false;
 auto invalid=game;invalid.m_player.ammo=-1;if(!check(!game.decodeSave(invalid.encodeSave()),"Valid-checksum invalid gameplay data rejected"))return false;
 game.m_heldClutter=0;game.m_clutter[0].projectile=true;game.m_clutter[0].velocity={2,3};Game held;if(!check(held.decodeSave(game.encodeSave())&&held.holdingClutter()&&held.m_clutter[0].velocity.x==2,"Held and moving clutter roundtrip"))return false;
 auto directory=std::filesystem::current_path()/("save-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));game.setSaveDirectory(directory.wstring());
 struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;for(int i=0;i<3;++i){auto p=slotPath(path.wstring(),i);std::filesystem::remove(p,e);p+=L".tmp";std::filesystem::remove(p,e);}std::filesystem::remove(path,e);}} cleanup{directory};
 InputState escape{};escape.escape=true;game.update(escape,.01f);game.update({},.01f);
 auto click=[&](int row){game.update({},.01f);InputState i{};i.pointerX=MenuLayout::X+30;i.pointerY=MenuLayout::RowTop+row*MenuLayout::RowHeight+7;i.fire=true;game.update(i,.01f);};
 click(6);if(!check(game.paused()&&game.menuPage()==MenuPage::Save,"Esc menu opens Save submenu"))return false;
 auto time=game.elapsed();click(0);if(!check(std::filesystem::exists(slotPath(directory.wstring(),0))&&game.elapsed()==time&&game.menuMessage()=="GAME SAVED","Save slot writes while simulation stays paused"))return false;
 auto before=readFile(slotPath(directory.wstring(),0));game.m_player.ammo=5;click(0);if(!check(game.menuPage()==MenuPage::Overwrite,"Existing slot requires overwrite confirmation"))return false;
 click(0);if(!check(readFile(slotPath(directory.wstring(),0))==before,"Cancel preserves old save"))return false;
 click(0);click(1);if(!check(game.menuPage()==MenuPage::Save&&readFile(slotPath(directory.wstring(),0))!=before,"Confirmed overwrite replaces slot"))return false;
 {auto path=slotPath(directory.wstring(),0),temp=path;temp+=L".tmp";auto saved=readFile(path);std::filesystem::create_directory(temp);
  bool safe=!game.saveSlot(0)&&readFile(path)==saved;std::filesystem::remove(temp);if(!check(safe,"Failed temporary write preserves the previous slot"))return false;}
 game.update({},.01f);game.update(escape,.01f);if(!check(game.paused()&&game.menuPage()==MenuPage::Settings,"Esc returns from submenu without resuming"))return false;
 click(7);click(2);click(1);if(!check(game.paused()&&game.menuPage()==MenuPage::Load&&game.menuMessage().find("FAILED")!=std::string::npos,"Empty slot reports load error without losing progress"))return false;
 click(0);click(0);if(!check(game.menuPage()==MenuPage::Load,"Load confirmation can be cancelled"))return false;
 game.m_player.ammo=1;auto revision=game.sessionRevision();click(0);click(1);
 if(!check(!game.paused()&&game.player().ammo==5&&game.sessionRevision()>revision,"Confirmed load resumes and resets audio session"))return false;
 InputState fire{};fire.fire=true;game.update(fire,.01f);if(!check(game.player().ammo==5,"Load click cannot fire the weapon"))return false;
 for(int slot=1;slot<3;++slot){if(!check(game.saveSlot(slot),"All three slots support independent saves"))return false;}
 return check(!game.saveSlot(-1)&&!game.loadSlot(3),"Invalid slots are rejected");
}
}

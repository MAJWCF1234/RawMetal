#include "MapHealth.h"
#include "Game.h"
#include "../audio/SoundAssets.h"
#include <windows.h>
#include "../world/CustomCampaign.h"
#include "../world/ModelIds.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <string_view>

namespace retro {
namespace {
constexpr std::array Systems{"doors","creature-nav","collision","lights","decals","audio","controller","assets"};
std::string indexed(const char* label,size_t i){return std::string(label)+"["+std::to_string(i)+"]";}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool validRange(float value,float low,float high){return value>=low&&value<=high;}
std::string clean(std::string value){
 for(char& c:value)if(c=='\t'||c=='\r'||c=='\n')c=' ';
 return value;
}
std::string mapName(const Game& game){
 const char* name=game.world().customMapName();
 if(*name)return clean(name);
 if(game.worldId()==WorldId::Campaign&&game.level()>=0&&game.level()<CampaignChunkCount)
  return CampaignMapNames[size_t(game.level())];
 return "unnamed";
}
std::string auditSystem(std::string_view reason){
 if(reason.find("lamp")!=reason.npos||reason.find("LAMP")!=reason.npos||reason.find("LIGHT")!=reason.npos)return "lights";
 if(reason.find("door")!=reason.npos||reason.find("DOOR")!=reason.npos)return "doors";
 if(reason.starts_with("UNREACHABLE")||reason.starts_with("TRAP"))return "creature-nav";
 return "collision";
}
std::string auditObject(const std::string& reason){
 const size_t bracket=reason.find('['),end=reason.find(']',bracket);
 if(bracket!=reason.npos&&end!=reason.npos){
  const size_t space=reason.rfind(' ',bracket);
  return reason.substr(space==reason.npos?0:space+1,end-(space==reason.npos?0:space+1)+1);
 }
 // The fallback keeps named or coordinate-based authored objects findable.
 const auto at=reason.find(" at ");
 return reason.substr(0,at==reason.npos?reason.find(" ("):at);
}
void writeIssue(std::ostream& out,const MapHealthIssue& i){
 out<<"FAIL\t"<<clean(i.system)<<'\t'<<clean(i.object)<<'\t'<<clean(i.file)<<'\t'<<i.line<<'\t'<<clean(i.reason)<<'\n';
}
}

std::vector<MapHealthIssue> inspectMapHealth(const Game& game){
 const World& world=game.world();
 std::vector<MapHealthIssue> issues;
 auto flag=[&](const char* system,std::string object,const char* reason,Vec2 p={},float z=0,bool point=false,
               std::source_location source=std::source_location::current()){
  issues.push_back({system,std::move(object),reason,p,z,point&&finite(p)&&std::isfinite(z),"src/game/MapHealth.cpp",source.line()});
 };
 const auto& doors=world.doors();
 for(size_t i=0;i<doors.size();++i){
  const auto& d=doors[i];const Vec2 p=d.centre();
  const float z=world.floorHeight(p.x,p.y)+d.z;
  if(!finite(p)||!std::isfinite(d.width())||!std::isfinite(z)||d.width()<=0||!validRange(d.maxOpen,0,1)||!validRange(d.open,0,1))
   flag("doors",indexed("door",i),"invalid opening dimensions or leaf state",p,z,true);
  for(size_t j=0;j<i;++j){
   const auto& other=doors[j];
   if(d.alongY==other.alongY&&std::fabs(d.y-other.y)<.035f&&std::fabs(d.z-other.z)<.08f&&
      std::min(d.right,other.right)-std::max(d.left,other.left)>.055f){
    flag("doors",indexed("door",i),"closed leaf overlaps another door span",p,z+1.f,true);break;
   }
  }
 }
 const auto& creatures=world.creatureSpawns();
 for(size_t i=0;i<creatures.size();++i){
  const auto& s=creatures[i];
  if(!finite(s.position)||!std::isfinite(s.z)||!validRange(s.scale,.05f,8.f)){
   flag("creature-nav",indexed("creature",i),"invalid position or hull scale",s.position,s.z,true);continue;
  }
  const auto& t=creatureTraits(s.kind);
  const float z=s.z==-999?world.floorHeight(s.position.x,s.position.y):s.z;
  if(!t.dormant&&world.structureBlocksHull(s.position.x,s.position.y,t.radius*s.scale,z+.02f,t.hull*s.scale))
   flag("creature-nav",indexed("creature",i),"spawn hull intersects map collision",s.position,z,true);
 }
 const auto& spawn=world.definition();
 const Vec2 player=spawn.playerStart;
 const float foot=world.hasTerrain()?world.supportBelow(player.x,player.y,float(World::TerrainMaxZ+1)):spawn.spawnHeight;
 if(!finite(player)||!std::isfinite(foot)||!world.fits(player.x,player.y,foot+.02f,Player::ProneHeight,false)||
    world.structureBlocksHull(player.x,player.y,.2f,foot+.02f,Player::ProneHeight))
  flag("collision","player start","initial player hull is blocked",player,foot,true);
 const auto& structures=world.structures();
 for(size_t i=0;i<structures.size();++i){
  const auto& s=structures[i];
  if(s.solid&&(!finite({s.x1,s.y1})||!finite({s.x2,s.y2})||!std::isfinite(s.bottom)||!std::isfinite(s.top)||
     s.x2<=s.x1||s.y2<=s.y1||s.top<=s.bottom))
   flag("collision",indexed("structure",i),"invalid solid bounds",{(s.x1+s.x2)*.5f,(s.y1+s.y2)*.5f},s.bottom,true);
 }
 const auto& lights=world.lights();
 for(size_t i=0;i<lights.size();++i){
  const auto& l=lights[i];
  if(!finite(l.position)||!std::isfinite(l.z)||!std::isfinite(l.range)||l.range<=0||!std::isfinite(l.intensity)||l.intensity<0)
   flag("lights",indexed("light",i),"invalid light position, range or intensity",l.position,l.z,true);
  for(size_t j=0;j<i;++j){
   const auto& other=lights[j];
   if(l.mount!=WorldLightMount::Bare&&other.mount!=WorldLightMount::Bare&&
      lengthSq(l.position-other.position)<.0225f&&std::fabs(l.z-other.z)<.12f){
    flag("lights",indexed("light",i),"lamp housing shares another lamp's position",l.position,l.z,true);break;
   }
  }
  if(l.mount==WorldLightMount::Wall&&finite(l.position)&&std::isfinite(l.z)){
   const Vec2 back=l.position-Vec2{std::cos(l.yaw),std::sin(l.yaw)}*.16f;
   if(world.fits(back.x,back.y,l.z,.015f,false))
    flag("lights",indexed("light",i),"wall lamp has no backing within 16 cm",l.position,l.z,true);
  }
 }
 const auto& decals=game.bulletImpacts();
 for(size_t i=0;i<decals.size();++i){
  const auto& d=decals[i];
  if(d.level==game.level()&&(!finite(d.pos)||!std::isfinite(d.z)||!finite(d.normal)||
     !std::isfinite(d.normalZ)||d.door<-1||d.door>=int(doors.size())))
   flag("decals",indexed(d.blood?"blood":"bullet-impact",i),"invalid surface normal, position or door binding",d.pos,d.z,true);
 }
 const auto& audio=world.ambients();
 for(size_t i=0;i<audio.size();++i){
  const auto& a=audio[i];
  if(a.sound<Sound(0)||a.sound>=Sound::Count||!finite(a.position)||!std::isfinite(a.z)||
     !std::isfinite(a.gain)||a.gain<0||!std::isfinite(a.radius)||a.radius<=0||
     a.mode<AmbientEmitter::Loop||a.mode>AmbientEmitter::OnState)
   flag("audio",indexed("ambient",i),"invalid sound, attenuation, position or playback mode",a.position,a.z,a.z!=-999);
#ifndef RAWMETAL_LAB
  else if(!FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(soundResourceId(a.sound)),MAKEINTRESOURCEW(10)))
   flag("assets",indexed("ambient",i),"ambient sound resource missing from executable",a.position,a.z,a.z!=-999);
#endif
 }
 const auto& pad=game.settings().pad;
 if(!validRange(float(pad.look),0,8)||!validRange(float(pad.dead),0,4)||
    !validRange(float(pad.curve),0,2)||!validRange(float(pad.rumble),0,2))
  flag("controller","binding profile","input settings outside their supported range");
 const auto& fixtures=world.fixtures();
 for(size_t i=0;i<fixtures.size();++i){
  const auto& f=fixtures[i];
  if(f.visible&&(f.model<0||size_t(f.model)>=std::size(model::size)))
   flag("assets",indexed("fixture",i),"fixture model slot is missing from the model catalog",f.position,f.base,true);
 }
 return issues;
}

void Game::reportMapHealthLoad() const{
 const auto issues=inspectMapHealth(*this);
 std::ofstream out("RawMetal-map-health.log",std::ios::app);
 for(const char* system:Systems){
  const auto match=std::find_if(issues.begin(),issues.end(),[&](const auto& i){return i.system==system;});
  out<<(match==issues.end()?"OK":"FAIL")<<" | "<<system<<" | map "<<level()<<' '<<mapName(*this);
  if(match!=issues.end()){
   const auto count=std::count_if(match,issues.end(),[&](const auto& i){return i.system==system;});
   out<<" | "<<clean(match->object)<<" | "<<clean(match->reason);
   if(count>1)out<<" [x"<<count<<']';
  }
  else out<<" | map state | checks passed";
  out<<'\n';
 }
}

int Game::runMapHealth(const wchar_t* index,const std::wstring& campaignPath){
 try{
  auto campaign=campaignPath.empty()?std::shared_ptr<const CustomCampaign>{}:loadCustomCampaignFile(std::filesystem::path(campaignPath));
  const int count=campaign?int(campaign->maps.size()):worldChunkCount(WorldId::Campaign);
  if(!index){std::ofstream out("map-health-count.txt");out<<count<<'\n';return out?0:2;}
  size_t used=0;
  const int map=std::stoi(index,&used);
  if(index[used]||map<0||map>=count)return 2;
  Game game(campaign?WorldId::Custom:WorldId::Campaign,true);
  game.m_customCampaign=std::move(campaign);
  game.loadLevel(map,false,false);
  auto issues=inspectMapHealth(game);
  std::vector<std::pair<std::string,unsigned>> audit;
  auditMapScene(game,map,&audit);
  for(auto& [reason,line]:audit)
   issues.push_back({auditSystem(reason),auditObject(reason),std::move(reason),{},0,false,"src/game/MapAudit.cpp",line});
  std::ofstream out("map-health.tsv");
  out<<"MAP\t"<<map<<'\t'<<mapName(game)<<'\n';
  for(const auto& i:issues)writeIssue(out,i);
  out<<"END\n";
  return out?0:2;
 }catch(const std::exception& e){
  std::ofstream out("map-health.tsv");
  if(index){
   out<<"MAP\t"<<clean(std::filesystem::path(index).string())<<"\tunloaded\n";
   writeIssue(out,{"assets","map load",e.what(),{},0,false,"src/game/MapHealth.cpp",__LINE__});
   out<<"END\n";
  }
  return index&&out?0:2;
 }
}
}

#include "Game.h"
#include <sstream>
#include <cctype>
#include <fstream>
#include <charconv>
namespace retro {
void Game::executeConsole(std::string command){
 if(command.empty())return;
 m_consoleLog.push_back("> "+command);m_consoleHistory.push_back(command);
 if(m_consoleHistory.size()>32)m_consoleHistory.erase(m_consoleHistory.begin());m_consoleHistoryIndex=int(m_consoleHistory.size());
 for(auto&c:command)c=char(std::tolower(static_cast<unsigned char>(c)));
 std::istringstream stream(command);std::string verb,arg,extra;stream>>verb>>arg>>extra;
 if(verb=="help"||verb=="maps"){
  m_consoleLog.push_back("ASHFALL: MAP WASTELAND / MAP COAST TO VISIT THE NEW SHORE");
  for(int map=0;map<CampaignChunkCount;++map)m_consoleLog.push_back(std::to_string(map)+" "+CampaignMapNames[map]);
  m_consoleLog.push_back("MAP <ID> / FREIGHT / RECEIVING / WAREHOUSE / MANIFEST / YARD / PLATFORM / DEPOT / TUNNELS.");
  m_consoleLog.push_back("MAP REACTOR STARTS AFTER THE CRASH.");
  m_consoleLog.push_back("RELOAD / WHERE / FPS / R_SCALE 50|75|100 / GIVE FLASHLIGHT / CLEAR. ESC: CLOSE.");
 }else if(verb=="clear")m_consoleLog.clear();
 else if(verb=="map"&&(arg=="custom"||arg=="wasteland"||arg=="horror")){
  m_customCampaign.reset();m_customCampaignKey=0;m_worldId=WorldId::Ashfall;m_level=0;restart();m_paused=false;m_inventoryOpen=false;m_consoleLog.push_back("LOADED ASHFALL WASTELAND / 15 STITCHED SURFACE CHUNKS.");
 }
 else if(verb=="map"&&(arg=="coast"||arg=="shore")){
  m_customCampaign.reset();m_customCampaignKey=0;m_worldId=WorldId::Ashfall;m_level=13;restart();m_paused=false;m_inventoryOpen=false;m_consoleLog.push_back("LOADED ASHFALL COAST / WALK EAST TO THE WATER.");
 }
 else if(verb=="give"&&arg=="flashlight"&&extra.empty()){giveQuestItem(Flashlight);m_consoleLog.push_back("FLASHLIGHT ADDED. F TO TOGGLE.");}
 else if(verb=="fps"){m_showFps=!m_showFps;m_consoleLog.push_back(m_showFps?"FRAME-TIME DISPLAY ON":"FRAME-TIME DISPLAY OFF");}
 else if(verb=="r_scale"){
  if(arg=="50"||arg=="75"||arg=="100"){m_renderScale=arg=="50"?.5f:arg=="75"?.75f:1.f;m_consoleLog.push_back("3D RENDER SCALE "+arg+" PERCENT. HUD STAYS FULL RESOLUTION.");}
  else m_consoleLog.push_back("USE R_SCALE 50, 75 OR 100. 100 IS THE FULL-RES DEFAULT.");
 }
 else if(verb=="where"){
  std::ostringstream out;out<<"MAP "<<m_level<<" X "<<m_player.pos.x<<" Y "<<m_player.pos.y<<" Z "<<m_player.z;m_consoleLog.push_back(out.str());
 }else if(verb=="map"||verb=="reload"){
  int level=-1;bool reactor=arg=="reactor";
  if(verb=="reload")level=m_level;
  else if(arg=="0"||arg=="foundry")level=0;
  else if(arg=="1"||arg=="pressureworks"||arg=="pressure")level=1;
  else if(arg=="2"||arg=="gantry")level=2;
  else if(arg=="3"||arg=="lift"||arg=="surface"||reactor)level=3;
  else if(arg=="4"||arg=="gallery"||arg=="service")level=4;
  else if(arg=="6"||arg=="cable")level=6;
  else if(arg=="7"||arg=="annex")level=7;
  else if(arg=="8"||arg=="junction")level=8;
  else if(arg=="9"||arg=="waste")level=9;
  else if(arg=="5"||arg=="coolant"||arg=="return")level=5;
  else if(arg=="freight")level=10;
  else if(arg=="receiving")level=13;
  else if(arg=="warehouse")level=16;
  else if(arg=="manifest")level=20;
  else if(arg=="yard")level=22;
  else if(arg=="platform")level=25;
  else if(arg=="depot")level=27;
  else if(arg=="tunnels")level=30;
  else if(!arg.empty()){
   int candidate=-1;auto result=std::from_chars(arg.data(),arg.data()+arg.size(),candidate);
   if(result.ec==std::errc{}&&result.ptr==arg.data()+arg.size()&&candidate>=0&&candidate<CampaignChunkCount)level=candidate;
  }
  if(level<0||!extra.empty())m_consoleLog.push_back("UNKNOWN MAP. TYPE MAPS FOR VALID NAMES / IDS.");
  else{
   if(verb!="reload"){m_customCampaign.reset();m_customCampaignKey=0;m_worldId=WorldId::Campaign;}m_level=level;restart();m_paused=false;m_inventoryOpen=false;
   if(reactor){m_world.startLift();m_world.updateLift(World::LiftRideComplete);m_player.pos={12,15.5f};m_player.z=-9;m_player.angle=kPi*.5f;}
   m_consoleLog.push_back("LOADED "+(reactor?std::string("REACTOR"):std::to_string(level))+". PRESS ` OR ESC TO PLAY.");
  }
 }else m_consoleLog.push_back("UNKNOWN COMMAND. TYPE HELP.");
 if(m_consoleLog.size()>48)m_consoleLog.erase(m_consoleLog.begin(),m_consoleLog.end()-48);
}
void Game::updateConsole(const InputState& input){
 if(input.escape){m_consoleOpen=false;m_previousEscape=true;m_suppressFire=true;return;}
 for(char c:input.textInput){
  if(c=='\r'||c=='\n'){auto command=m_consoleLine;m_consoleLine.clear();executeConsole(command);}
  else if(c=='\b'){if(!m_consoleLine.empty())m_consoleLine.pop_back();}
  else if(c>=32&&c<127&&c!='`'&&c!='~'&&m_consoleLine.size()<120)m_consoleLine+=c;
 }
 if(input.menuUp&&!m_consoleUp&&!m_consoleHistory.empty()){m_consoleHistoryIndex=std::max(0,m_consoleHistoryIndex-1);m_consoleLine=m_consoleHistory[m_consoleHistoryIndex];}
 if(input.menuDown&&!m_consoleDown){m_consoleHistoryIndex=std::min(int(m_consoleHistory.size()),m_consoleHistoryIndex+1);m_consoleLine=m_consoleHistoryIndex<int(m_consoleHistory.size())?m_consoleHistory[m_consoleHistoryIndex]:"";}
 m_consoleUp=input.menuUp;m_consoleDown=input.menuDown;
}
bool Game::testConsole(){
 Game game;InputState toggle{};toggle.console=true;game.update(toggle,.02f);if(!game.consoleOpen())return false;
 auto p=game.player();float time=game.elapsed();InputState input{};input.forward=true;input.fire=true;input.mouseDx=30;
 game.update(input,.02f);if(game.elapsed()!=time||game.player().ammo!=p.ammo||game.player().angle!=p.angle)return false;
 for(int level=0;level<ChunkCount;++level){input={};input.textInput="map "+std::to_string(level)+"\r";game.update(input,.02f);if(game.level()!=level||!game.consoleOpen()||!game.world().fits(game.player().pos.x,game.player().pos.y,game.player().z,1))return false;}
 for(const auto& alias:std::array<std::pair<const char*,int>,8>{{{"freight",10},{"receiving",13},{"warehouse",16},{"manifest",20},{"yard",22},{"platform",25},{"depot",27},{"tunnels",30}}}){
  input.textInput=std::string("map ")+alias.first+"\r";game.update(input,.02f);if(game.level()!=alias.second||!game.world().campaign())return false;
 }
 for(const char* invalid:{"32","-1","10junk","999999999999999999999999"}){int current=game.level();input.textInput=std::string("map ")+invalid+"\r";game.update(input,.02f);if(game.level()!=current)return false;}
 input={};input.textInput="map receiving\r";game.update(input,.02f);if(game.state("receiving_worker_encounter_ms"))return false;
 input={};input.escape=true;game.update(input,.02f);game.update({},.02f);if(!game.state("receiving_worker_encounter_ms")||game.player().pitch>-40)return false;
 input={};input.console=true;game.update(input,.02f);if(!game.consoleOpen())return false;
 input.textInput="map reactor\r";game.update(input,.02f);if(game.world().liftPhase()!=World::LiftPhase::Crashed||game.player().z!=-9)return false;
 input.textInput="map 99\r";game.update(input,.02f);if(game.level()!=3||game.player().z!=-9)return false;
 input.textInput="map lift\r";game.update(input,.02f);if(game.world().liftPhase()!=World::LiftPhase::Ready||game.player().z!=0)return false;
 input={};input.textInput="map custom\r";game.update(input,.02f);
 if(!game.world().horrorMode()||game.level()!=0||!game.world().openSouthBoundary()||!game.world().openEastBoundary()||game.world().openNorthBoundary()||game.world().openWestBoundary()||!game.world().fits(game.player().pos.x,game.player().pos.y,game.player().z,game.player().hullHeight()))return false;
 for(int level=0;level<game.chunkCount();++level){auto&w=game.m_chunks[level].world;int col=level>=12?4:level%4,row=level>=12?level-12:level/4;bool north=row>0,south=row<2,west=col>0,east=col<4;
  if(!w.horrorMode()||w.openNorthBoundary()!=north||w.openSouthBoundary()!=south||w.openWestBoundary()!=west||w.openEastBoundary()!=east)return false;
 }
 input.textInput="map coast\r";game.update(input,.02f);
 if(game.level()!=13||!game.world().coast()||game.world().waterSurface(20,12)>0||game.world().floorHeight(20,12)>=game.world().waterSurface(20,12))return false;
 input.textInput="fps\r";game.update(input,.02f);if(!game.showFps())return false;
 input={};input.escape=true;game.update(input,.02f);if(game.consoleOpen()||game.paused())return false;
 std::ofstream("console-test.txt")<<"Backtick toggle; paused simulation; all campaign map IDs and reactor; Receiving teleport starts visible encounter after console closes; Ashfall boundaries and coast shortcut; invalid map; fresh lift; FPS; Esc closes: PASS\n";return true;
}
}

#include "Game.h"
#include "../renderer/SoftwareRenderer.h"
#include "../renderer/DebugLayer.h"
#include "../world/CustomCampaign.h"
#include "../world/CampaignExport.h"
#include "../lab/LabAccess.h"
#include "../net/Match.h"
#include <sstream>
#include <cctype>
#include <fstream>
#include <charconv>
#include <algorithm>
namespace retro {
void Game::executeConsole(std::string command){
 if(command.empty())return;
 m_consoleLog.push_back("> "+command);m_consoleHistory.push_back(command);
 if(m_consoleHistory.size()>32)m_consoleHistory.erase(m_consoleHistory.begin());m_consoleHistoryIndex=int(m_consoleHistory.size());
 for(auto&c:command)c=char(std::tolower(static_cast<unsigned char>(c)));
 std::istringstream stream(command);std::string verb,arg,extra;stream>>verb>>arg>>extra;
 // summon, ally/enemy/neutral, kill, killall, freeze, ai, list, help summon (Summon.cpp).
 if(consoleSummonCommand(command)){}
 else if(verb=="maps"&&arg=="engineering"&&extra.empty()){
  m_consoleLog.push_back("CHAPTER "+std::to_string(campaignChunkLabel(CampaignEngineeringEntry).chapter)+" / ENGINEERING");
  int first=-1,last=-1;
  std::string_view district;
  auto printDistrict=[&](){
   if(first>=0)m_consoleLog.push_back(std::to_string(first)+"-"+std::to_string(last)+" "+std::string(district));
  };
  for(int chunk:campaignChunksInPlayOrder()){
   const auto label=campaignChunkLabel(chunk);
   if(std::string_view(label.section)!=CampaignEngineeringSection)continue;
   if(district!=label.district){
    printDistrict();
    district=label.district;first=chunk;
   }
   last=chunk;
  }
  printDistrict();
  m_consoleLog.push_back("MAP ENGINEERING OPENS SERVICE SPINE (180), WEST OF DEEP ACCESS CONTROL.");
 }
 else if(verb=="help"||verb=="maps"){
  auto listMaps=[&](){
   m_consoleLog.push_back("ASHFALL: MAP WASTELAND / MAP COAST / MAP EXCLUSION");
   std::string_view section;
   for(int map=0;map<CampaignChunkCount;++map){
    // A district with its own section (Engineering) gets a heading line above its maps.
    const auto label=campaignChunkLabel(map);
    if(section!=label.section){
     section=label.section;
     if(!section.empty())m_consoleLog.push_back("-- CHAPTER "+std::to_string(label.chapter)+" / "+std::string(section)+" --");
    }
    m_consoleLog.push_back(std::to_string(map)+" "+CampaignMapNames[map]);
   }
  };
  // The console shows only its last lines: HELP ends on the commands, MAPS on the
  // maps, so the Engineering section is on screen after typing MAPS.
  if(verb=="help")listMaps();
  m_consoleLog.push_back("MAP <ID> / FREIGHT / RECEIVING / WAREHOUSE / MANIFEST / YARD / PLATFORM / DEPOT / TUNNELS.");
  m_consoleLog.push_back("MAP REACTOR STARTS AFTER THE CRASH.");
  m_consoleLog.push_back("MAPS ENGINEERING LISTS THE ENGINEERING DISTRICTS. MAP ENGINEERING OPENS SERVICE SPINE.");
  m_consoleLog.push_back("RELOAD / WHERE / GOD / NOCLIP / FPS / R_SCALE 50|75|100 / TEX_UP 1|2|4 / GIVE FLASHLIGHT|HANDGUN|9MM / CLEAR. ESC: CLOSE.");m_consoleLog.push_back("NOCLIP: FLY THROUGH WALLS WHERE YOU LOOK. JUMP RISES, CROUCH SINKS, SPRINT IS FASTER. NOT SAVED.");m_consoleLog.push_back("UNLOCK: OPENS THE DOOR OR TERMINAL YOU ARE LOOKING AT (PLAYTESTING; CHAPTER 2 STARTS AT MAP 68).");
  m_consoleLog.push_back("DEBUGLAYER: TOGGLE COLLIDERS, LIGHTS, CHUNKS, HAZARDS AND STATS.");
  m_consoleLog.push_back("GIVE 12 GA / 12GA / SHELLS: ADDS 16 SHOTGUN SHELLS.");
  m_consoleLog.push_back("SUMMON <KIND> [N] [ALLY|ENEMY|NEUTRAL] [AI=..] / ALLY / ENEMY / KILL / KILLALL / FREEZE / AI ON|OFF / LIST. HELP SUMMON.");
  if(verb=="maps")listMaps();
 }else if(verb=="clear")m_consoleLog.clear();
 else if(verb=="unlock"){
  // Unlock what the player is looking at for good, by setting the state it
  // waits for: a door's lock, or a terminal's prerequisite. For playtesting.
  if(int door=nearbyInteractionDoor();door>=0){auto&d=m_world.doors()[size_t(door)];
   if(d.requireState){setState(d.requireState,d.requireValue);m_consoleLog.push_back("DOOR UNLOCKED.");}else m_consoleLog.push_back("THAT DOOR HAS NO LOCK TO OPEN.");}
  else if(int terminal=nearbyTerminal();terminal>=0){auto&t=m_world.terminals()[size_t(terminal)];
   if(t.requireState){setState(t.requireState,1);m_consoleLog.push_back("TERMINAL UNLOCKED.");}else m_consoleLog.push_back("THAT TERMINAL IS NOT LOCKED.");}
  else m_consoleLog.push_back("LOOK AT A DOOR OR A TERMINAL, THEN TYPE UNLOCK.");
 }
 else if(verb=="map"&&(arg=="custom"||arg=="wasteland"||arg=="horror")){
  m_customCampaign.reset();m_customCampaignKey=0;m_worldId=WorldId::Ashfall;m_level=0;restart();m_paused=false;m_inventoryOpen=false;m_consoleLog.push_back("LOADED ASHFALL WASTELAND / 15 STITCHED SURFACE CHUNKS.");
 }
 else if(verb=="map"&&(arg=="exclusion"||arg=="facility")){
  if(m_customCampaigns.empty()&&!m_customMapDirectory.empty())loadCustomCampaignDirectory(m_customMapDirectory);
  auto found=std::find_if(m_customCampaigns.begin(),m_customCampaigns.end(),[](const auto&c){return c&&c->sourceFile=="RawMetal-Ashfall-Exclusion-Zone.txt";});
  if(found==m_customCampaigns.end()){try{m_customCampaigns.push_back(loadEmbeddedExclusionZoneCampaign());found=m_customCampaigns.end()-1;}catch(...){m_consoleLog.push_back("EXCLUSION ZONE PACK MISSING.");return;}}
  selectCustomCampaign(int(found-m_customCampaigns.begin()));m_paused=false;m_inventoryOpen=false;m_consoleLog.push_back("LOADED ASHFALL EXCLUSION ZONE / GENERATED WASTELAND TERRAIN + FORWARD FACILITY.");
 }
 else if(verb=="map"&&(arg=="coast"||arg=="shore")){
  m_customCampaign.reset();m_customCampaignKey=0;m_worldId=WorldId::Ashfall;m_level=13;restart();m_paused=false;m_inventoryOpen=false;m_consoleLog.push_back("LOADED ASHFALL COAST / WALK EAST TO THE WATER.");
 }
 else if(verb=="give"&&arg=="flashlight"&&extra.empty()){giveQuestItem(Flashlight);m_consoleLog.push_back("FLASHLIGHT ADDED. F TO TOGGLE.");}
 // The handgun with one magazine in it (another magazine if already carried), and 24 rounds of 9 mm.
 else if(verb=="give"&&arg=="handgun"&&extra.empty()){grantHandgun(playerWeaponDef(WeaponId::Handgun).cycle.tubeCapacity);m_consoleLog.push_back("HANDGUN ADDED. 3 OR THE WHEEL TO DRAW.");}
 else if(verb=="give"&&arg=="9mm"&&extra.empty()){m_player.pool(WeaponId::Handgun).total+=24;m_consoleLog.push_back("24 ROUNDS 9MM ADDED.");}
 // The MAC-11 with one magazine in it (another if already carried), and a box of .380.
 else if(verb=="give"&&(arg=="sniper"||arg=="rifle")&&extra.empty()){grantSniper(playerWeaponDef(WeaponId::Sniper).cycle.tubeCapacity);m_consoleLog.push_back("SNIPER RIFLE ADDED. 5 OR THE WHEEL TO DRAW.");}
 else if(verb=="give"&&(arg=="308"||arg==".308")&&extra.empty()){m_player.pool(WeaponId::Sniper).total+=playerWeaponDef(WeaponId::Sniper).cycle.pickupAmount;m_consoleLog.push_back("10 ROUNDS .308 ADDED.");}
 else if(verb=="give"&&(arg=="mac11"||arg=="mac-11"||arg=="mac")&&extra.empty()){grantMachinePistol(playerWeaponDef(WeaponId::MachinePistol).cycle.tubeCapacity);m_consoleLog.push_back("MAC-11 ADDED. 4 OR THE WHEEL TO DRAW.");}
 else if(verb=="give"&&(arg=="380"||arg==".380")&&extra.empty()){m_player.pool(WeaponId::MachinePistol).total+=playerWeaponDef(WeaponId::MachinePistol).cycle.pickupAmount;m_consoleLog.push_back("32 ROUNDS .380 ADDED.");}
  else if(verb=="give"&&((arg=="12"&&(extra=="ga"||extra=="gauge")&&(stream>>std::ws).eof())||((arg=="12ga"||arg=="shells")&&extra.empty()))){m_player.pool(WeaponId::Shotgun).total+=16;m_consoleLog.push_back("16 SHELLS 12 GA ADDED.");}
 else if(verb=="dev"){setDeveloperMode(!developerMode());m_consoleLog.push_back(developerMode()?"DEVELOPER MODE ON: NO DAMAGE, NO CREATURES, EVERY DOOR OPEN.":"DEVELOPER MODE OFF.");}
 else if(verb=="god"){m_god=!m_god;if(m_god)m_player.health=std::max(m_player.health,100.f);m_consoleLog.push_back(m_god?"GOD MODE ON: NO DAMAGE. DEV ALSO OPENS THE GATES THAT WANT A ROOM CLEARED.":"GOD MODE OFF.");}
 else if(verb=="noclip"){const bool on=arg=="on"||(arg.empty()&&!m_noclip);
  if(arg.empty()||arg=="on"||arg=="off"){setNoclip(on);m_consoleLog.push_back(on?"NOCLIP ON: FLY WHERE YOU LOOK. JUMP UP, CROUCH DOWN, SPRINT FAST.":"NOCLIP OFF.");}
  else m_consoleLog.push_back("USE NOCLIP, NOCLIP ON OR NOCLIP OFF.");}
 // TP <WHO> [<TO>]: in a match the host moves a player beside another (names by their first letters; no TO, or
 // ME: beside the host). UNSTUCK: out of a wall or a tight spot, to the nearest open floor.
 else if(verb=="tp"){if(!m_match)m_consoleLog.push_back("TP MOVES PLAYERS IN A MULTIPLAYER GAME.");
  else if(arg.empty())m_consoleLog.push_back("USE TP <NAME> [<NAME>]: THE FIRST PLAYER GOES TO THE SECOND (NO SECOND: TO YOU).");
  else m_consoleLog.push_back(m_match->consoleTeleport(arg,extra));}
 // COOPFOLLOW [ON|OFF]: a co-op host's change of map brings along the guests on the map it left (off: everyone's map runs).
 else if(verb=="coopfollow"){if(!m_match)m_consoleLog.push_back("COOPFOLLOW IS FOR A CO-OP HOST.");
  else{if(arg=="on"||arg=="off")m_match->setFollowHost(arg=="on");else if(arg.empty())m_match->setFollowHost(!m_match->followHost());
   m_consoleLog.push_back(m_match->followHost()?"COOPFOLLOW ON: GUESTS ON THE MAP YOU LEAVE COME WITH YOU.":"COOPFOLLOW OFF: EVERYONE STAYS ON THEIR OWN MAP.");}}
 else if(verb=="unstuck"){m_stuckTime=1;if(!hullFits(m_player.pos,m_player.z,m_player.hullHeight()))m_consoleLog.push_back(unstickPlayer()?"MOVED TO OPEN FLOOR.":"NO OPEN FLOOR NEARBY. ASK THE HOST TO TP YOU.");
  else m_consoleLog.push_back("YOU ARE NOT STUCK.");m_stuckTime=0;}
 else if(verb=="debuglayer"||verb=="debug_layer"){
  using namespace debuglayer;
  int selected=-1;
  if(extra.empty()){
   if(arg.empty())selected=visible?0:All;
   else if(arg=="off")selected=0;
   else if(arg=="on"||arg=="all")selected=All;
   else for(const auto& [name,layer]:{std::pair{"colliders",Colliders},{"lights",Lights},{"chunks",Chunks},{"hazards",Hazards},{"stats",Stats}})
    if(arg==name){selected=layer;break;}
  }
  if(selected<0)m_consoleLog.push_back("USE DEBUGLAYER [ON|OFF|COLLIDERS|LIGHTS|CHUNKS|HAZARDS|STATS].");
  else{
   visible=unsigned(selected);
   m_consoleLog.push_back(visible?"DEBUG LAYER ON. CLOSE THE CONSOLE TO INSPECT.":"DEBUG LAYER OFF.");
  }
 }
 else if(verb=="fps"){m_showFps=!m_showFps;m_consoleLog.push_back(m_showFps?"FRAME-TIME DISPLAY ON":"FRAME-TIME DISPLAY OFF");}
 else if(verb=="r_scale"){
  if(arg=="50"||arg=="75"||arg=="100"){m_renderScale=arg=="50"?.5f:arg=="75"?.75f:1.f;m_consoleLog.push_back("3D RENDER SCALE "+arg+" PERCENT. HUD STAYS FULL RESOLUTION.");}
  else m_consoleLog.push_back("USE R_SCALE 50, 75 OR 100. 100 IS THE FULL-RES DEFAULT.");
 }
 else if(verb=="tex_up"){
  if(arg=="1"||arg=="2"||arg=="4"){SoftwareRenderer::setTexturePresentScale(std::stoi(arg));m_consoleLog.push_back("TEXTURE PRESENT SCALE "+arg+"X (STORE-LOW / RUN-HIGH). ATLASES REBUILD ON NEXT DRAW.");}
  else m_consoleLog.push_back("USE TEX_UP 1, 2 OR 4. DEFAULT IS 1X NATIVE ATLASES.");
 }
 else if(verb=="r_post"){
  if(arg=="ao"){SoftwareRenderer::setPostProcessing(true,true);m_consoleLog.push_back("POST DEBUG: AMBIENT OCCLUSION ONLY.");}
  else if(arg=="on"||arg=="off"){SoftwareRenderer::setPostProcessing(arg=="on");m_consoleLog.push_back(arg=="on"?"POST ON: AMBIENT OCCLUSION, DEPTH HAZE, BLOOM, GRADE.":"POST OFF: RAW SCENE LIGHTING.");}
  else m_consoleLog.push_back("USE R_POST ON OR OFF. ON IS THE DEFAULT.");
 }
 else if(verb=="r_lens"||verb=="r_tiltshift"){
  if(arg=="on"||arg=="off"){SoftwareRenderer::setTiltShift(arg=="on");m_consoleLog.push_back(arg=="on"?"LENS ON: DEPTH OF FIELD, VIGNETTE, GRAIN.":"LENS OFF.");}
  else m_consoleLog.push_back("USE R_LENS ON OR OFF. ON IS THE DEFAULT.");
 }
 else if(verb=="r_taa"){
  if(arg=="on"||arg=="off"){SoftwareRenderer::setTemporalAA(arg=="on");m_consoleLog.push_back(arg=="on"?"TEMPORAL AA ON.":"TEMPORAL AA OFF: MSAA ONLY.");}
  else m_consoleLog.push_back("USE R_TAA ON OR OFF. ON IS THE DEFAULT.");
 }
 else if(verb=="r_pixel"){
  if(arg=="on"||arg=="off"){SoftwareRenderer::setPixelLighting(arg=="on");m_consoleLog.push_back(arg=="on"?"PIXEL LIGHTING ON: PER-PIXEL LAMPS WITH SHADOW MAPS.":"PIXEL LIGHTING OFF: BAKED VERTEX LAMPS.");}
  else m_consoleLog.push_back("USE R_PIXEL ON OR OFF. ON IS THE DEFAULT.");
 }
 else if(verb=="thirdperson"||verb=="cl_thirdperson"){
  const bool on=arg=="on"||(arg.empty()&&!SoftwareRenderer::thirdPerson());
  if(arg.empty()||arg=="on"||arg=="off"){SoftwareRenderer::setThirdPerson(on);m_consoleLog.push_back(on?"THIRD PERSON: THE CAMERA OVER CLAIRE'S SHOULDER.":"FIRST PERSON.");}
  else m_consoleLog.push_back("USE THIRDPERSON, THIRDPERSON ON OR THIRDPERSON OFF.");
 }
 else if(verb=="r_texels"){
  if(arg=="crisp"||arg=="smooth"){SoftwareRenderer::setCrispTexels(arg=="crisp");m_consoleLog.push_back(arg=="crisp"?"TEXELS CRISP: SQUARE PSX TEXELS, ANTIALIASED EDGES.":"TEXELS SMOOTH: PLAIN BILINEAR MAGNIFICATION.");}
  else m_consoleLog.push_back("USE R_TEXELS CRISP OR SMOOTH. CRISP IS THE DEFAULT.");
 }
 else if(verb=="labsnap"){ // the scene as a feature-lab fixture (src/lab/LabAccess.h, lab/README.md)
  const std::string path=LabAccess::snapshot(*this,arg);m_consoleLog.push_back(path.empty()?"LABSNAP FAILED.":"LAB FIXTURE "+path);
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
  else if(arg=="engineering")level=CampaignEngineeringEntry;
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
// noclip: typed in the console, the body flies through a wall where the camera
// looks, rises and sinks, ignores gravity, stays out of the save, and lands when off.
bool Game::testNoclip(std::ostream& report){
 bool passed=true;auto check=[&](bool ok,const std::string& label){report<<(ok?"PASS ":"FAIL ")<<label<<'\n';report.flush();passed&=ok;};
 Game g;g.startMainCampaign(95);g.m_god=true;g.update({},1.f/60);
 auto type=[&](const std::string& line){InputState open{};open.console=true;g.update(open,1.f/60);InputState text{};text.textInput=line+"\r";g.update(text,1.f/60);InputState close{};close.escape=true;g.update(close,1.f/60);g.update({},1.f/60);};
 type("help");bool listed=false;for(const auto& l:g.m_consoleLog)listed|=l.find("NOCLIP")!=std::string::npos;check(listed,"help lists NOCLIP");
 // Stand in the middle of the map facing a wall a metre or two away, with floor past it.
 Vec2 start=g.m_player.pos;float bestD=1e9f;float bestYaw=0;
 for(float y=6;y<=18&&bestD>9e8f;y+=.5f)for(float x=6;x<=18&&bestD>9e8f;x+=.5f){const float z=g.m_world.floorHeight(x,y);if(!g.hullFits({x,y},z,Player::StandingHeight))continue;
  for(int a=0;a<8&&bestD>9e8f;++a){const float yaw=a*kPi/4;const Vec2 f{std::cos(yaw),std::sin(yaw)};
   for(float d=1.f;d<2.5f;d+=.1f){const Vec2 p=start=Vec2{x,y};const Vec2 q=p+f*d;if(g.m_world.fits(q.x,q.y,z+.3f,1.4f,false))continue;
    const Vec2 past=p+f*(d+1.2f);if(g.hullFits(past,g.m_world.floorHeight(past.x,past.y),Player::StandingHeight)){bestD=d;bestYaw=yaw;g.m_player.pos=p;g.m_player.z=z;}break;}}}
 start=g.m_player.pos;
 g.m_player.angle=bestYaw;g.m_player.pitch=0;const Vec2 ahead{std::cos(bestYaw),std::sin(bestYaw)};
 {Game walk=g;InputState fwd{};fwd.forward=true;for(int i=0;i<120;++i)walk.update(fwd,1.f/60);check(walk.level()==g.level()&&dot(walk.m_player.pos-start,ahead)<bestD+.05f,"without noclip the wall "+std::to_string(bestD)+" m ahead stops the body");}
 type("noclip");check(g.noclip(),"noclip turns on");
 report<<"  start "<<start.x<<","<<start.y<<" yaw "<<bestYaw<<" now "<<g.m_player.pos.x<<","<<g.m_player.pos.y<<" angle "<<g.m_player.angle<<" level "<<g.level()<<"\n";g.m_player.angle=bestYaw;g.m_player.pitch=0;
 InputState fwd{};fwd.forward=true;for(int i=0;i<int(60*(bestD+1.5f)/6.f)+30;++i)g.update(fwd,1.f/60);
 const float through=dot(g.m_player.pos-start,ahead);report<<"  after the flight "<<g.m_player.pos.x<<","<<g.m_player.pos.y<<" angle "<<g.m_player.angle<<" level "<<g.level()<<"\n";
 check(through>bestD+.6f||g.level()!=95,"noclip flies through the wall ("+std::to_string(through)+" m along, wall at "+std::to_string(bestD)+")");
 const float z0=g.m_player.z;for(int i=0;i<60;++i)g.update({},1.f/60);check(std::fabs(g.m_player.z-z0)<1e-3f,"noclip holds height without gravity");
 InputState up{};up.jump=true;for(int i=0;i<30;++i)g.update(up,1.f/60);const float risen=g.m_player.z-z0;check(risen>2.5f&&risen<3.5f,"jump rises 6 m/s ("+std::to_string(risen)+" m in 0.5 s)");
 InputState down{};down.crouch=true;for(int i=0;i<30;++i)g.update(down,1.f/60);check(std::fabs(g.m_player.z-z0)<.2f,"crouch sinks back");
 InputState fast{};fast.forward=true;fast.sprint=true;g.m_player.pitch=0;const Vec2 before=g.m_player.pos;const int level=g.level();g.update(fast,.05f);if(g.level()==level)check(length(g.m_player.pos-before)>.8f,"sprint flies 18 m/s");
 g.m_player.pitch=140.f*.5f;const float zLook=g.m_player.z;for(int i=0;i<6;++i)g.update(fwd,1.f/60);check(g.m_player.z>zLook+.2f,"forward follows the look pitch upward");
 Game loaded;check(loaded.decodeSave(g.encodeSave())&&!loaded.noclip(),"a save taken in noclip loads without noclip");
 type("noclip");check(!g.noclip(),"noclip turns off");for(int i=0;i<240;++i)g.update({},1.f/60);check(g.m_player.grounded||g.m_player.verticalVelocity<0,"off, gravity takes the body again");
 return passed;
}
bool Game::testConsole(){
 std::ofstream report("console-test.txt");
 Game game;auto fail=[&](int line){report<<"FAIL at Console.cpp:"<<line<<" level="<<game.level()<<" z="<<game.player().z<<'\n';return false;};
 InputState toggle{};toggle.console=true;game.update(toggle,.02f);if(!game.consoleOpen())return fail(__LINE__);
 auto p=game.player();float time=game.elapsed();InputState input{};input.forward=true;input.fire=true;input.mouseDx=30;
 game.update(input,.02f);if(game.elapsed()!=time||game.player().pools!=p.pools||game.player().angle!=p.angle)return fail(__LINE__);
 // A shell grant must target the shotgun even with the handgun drawn, without
 // loading the tube, changing weapons, or accepting a partly valid command.
 game.grantHandgun(3);game.equipWeapon(WeaponId::Handgun);
 game.m_player.pool(WeaponId::Shotgun)={10,2};game.m_player.pool(WeaponId::Handgun)={11,3};
 bool givePassed=true;auto checkGive=[&](bool ok,const std::string& label){report<<(ok?"PASS ":"FAIL ")<<label<<'\n';report.flush();givePassed&=ok;};
 auto typeGive=[&](const std::string& command){input={};input.textInput=command+"\r";game.update(input,.02f);};
 for(const std::string command:{"give 12 GA","give 12ga","give shells","  GiVe   12   gA  ","give 12 gauge"}){
  const auto before=game.player().pools;const auto weapon=game.equippedWeapon();
  typeGive(command);
  checkGive(game.player().pool(WeaponId::Shotgun)==AmmoPool{before[size_t(WeaponId::Shotgun)].total+16,2},command+" adds sixteen reserve shells without loading");
  checkGive(game.player().pool(WeaponId::Handgun)==AmmoPool{11,3}&&game.player().pool(WeaponId::Fists)==before[size_t(WeaponId::Fists)]&&game.equippedWeapon()==weapon,command+" preserves the other ammo pools and drawn handgun");
 }
 for(const std::string command:{"give 12","give ga","give 12 gax","give 12 ga junk","give 12 gauge junk","give 12ga junk","give shells junk"}){
  const auto before=game.player().pools;typeGive(command);
  checkGive(game.player().pools==before&&game.equippedWeapon()==WeaponId::Handgun,command+" rejects malformed shell requests");
 }
 typeGive("give 9mm");checkGive(game.player().pool(WeaponId::Handgun)==AmmoPool{35,3},"give 9mm still adds twenty-four rounds");
 typeGive("give handgun");checkGive(game.hasHandgun()&&game.player().pool(WeaponId::Handgun)==AmmoPool{47,3},"give handgun still adds one magazine when owned");
 typeGive("help");bool shellHelp=false;for(const auto& line:game.m_consoleLog)shellHelp|=line.find("GIVE 12 GA")!=std::string::npos;
 checkGive(shellHelp,"help lists the displayed 12 GA caliber command");
 {typeGive("clear");typeGive("maps");
  const auto heading=std::find(game.m_consoleLog.begin(),game.m_consoleLog.end(),std::string("-- CHAPTER 2 / ENGINEERING --"));
  checkGive(heading!=game.m_consoleLog.end()&&heading+1!=game.m_consoleLog.end()&&*(heading+1)=="180 ENGINEERING / SERVICE SPINE","MAPS shows an Engineering heading above Service Spine");
  checkGive(heading!=game.m_consoleLog.end()&&game.m_consoleLog.end()-heading<=9,"the Engineering heading is in the console's nine visible lines");}
 typeGive("maps engineering");
 const std::array<std::string_view,2> engineeringRows={
  "CHAPTER 2 / ENGINEERING","180-181 DEEP RESEARCH SERVICES"};
 for(auto row:engineeringRows)
  checkGive(std::find(game.m_consoleLog.begin(),game.m_consoleLog.end(),row)!=game.m_consoleLog.end(),std::string(row));
 typeGive("map engineering");
 checkGive(game.level()==EngineeringServiceChunk&&game.world().campaign(),"MAP ENGINEERING opens the new service spine");
 auto walkSeam=[&](int from,Vec2 start,Vec2 direction,int to){
  game.m_level=from;game.restart();game.m_player.pos=start;game.m_player.z=-15.f;
  game.updateStreaming(.02f);
  for(int step=0;step<28&&game.level()==from;++step){
   game.tryMove(direction*.1f);
   game.crossChunkBoundary();
  }
  return game.level()==to;
 };
 checkGive(walkSeam(108,{1.5f,14.5f},{-1,0},EngineeringServiceChunk),"Deep Access Control opens west to the service spine");
 checkGive(walkSeam(EngineeringServiceChunk,{22.5f,14.5f},{1,0},108),"Engineering returns east to Deep Access Control");
 checkGive(walkSeam(EngineeringServiceChunk,{1.5f,14.5f},{-1,0},EngineeringIsolationChunk),"Service Spine connects west to Feed Isolation");
 checkGive(walkSeam(EngineeringIsolationChunk,{22.5f,14.5f},{1,0},EngineeringServiceChunk),"Feed Isolation returns east to Service Spine");
 checkGive(walkSeam(108,{12.f,22.5f},{0,1},109),"Deep Access Control still continues south to Extraction Interface");
 if(!givePassed)return false;
 for(int level=0;level<ChunkCount;level+=level<34?1:(level+7<ChunkCount?7:std::max(1,ChunkCount-1-level))){input={};input.textInput="map "+std::to_string(level)+"\r";game.update(input,.02f);if(game.level()!=level||!game.consoleOpen()||!game.world().fits(game.player().pos.x,game.player().pos.y,game.player().z,1))return fail(__LINE__);}
 for(const auto& alias:std::array<std::pair<const char*,int>,8>{{{"freight",10},{"receiving",13},{"warehouse",16},{"manifest",20},{"yard",22},{"platform",25},{"depot",27},{"tunnels",30}}}){
  input.textInput=std::string("map ")+alias.first+"\r";game.update(input,.02f);if(game.level()!=alias.second||!game.world().campaign())return fail(__LINE__);
 }
 for(const std::string invalid:{std::to_string(ChunkCount),std::string("-1"),std::string("10junk"),std::string("999999999999999999999999")}){int current=game.level();input.textInput="map "+invalid+"\r";game.update(input,.02f);if(game.level()!=current)return fail(__LINE__);}
 // Restart canonicalizes the visible encounter at t=1; the console must freeze it.
 input={};input.textInput="map receiving\r";game.update(input,.02f);if(game.state("receiving_worker_encounter_ms")!=1)return fail(__LINE__);
 input={};input.escape=true;game.update(input,.02f);game.update({},.02f);if(game.state("receiving_worker_encounter_ms")<=1||game.player().pitch>-40)return fail(__LINE__);
 input={};input.console=true;game.update(input,.02f);if(!game.consoleOpen())return fail(__LINE__);
 input.textInput="map reactor\r";game.update(input,.02f);if(game.world().liftPhase()!=World::LiftPhase::Crashed||game.player().z!=-9)return fail(__LINE__);
 input.textInput="map 9999\r";game.update(input,.02f);if(game.level()!=3||game.player().z!=-9)return fail(__LINE__);
 input.textInput="map lift\r";game.update(input,.02f);if(game.world().liftPhase()!=World::LiftPhase::Ready||game.player().z!=0)return fail(__LINE__);
 input={};input.textInput="map custom\r";game.update(input,.02f);
 if(!game.world().horrorMode()||game.level()!=0||!game.world().openSouthBoundary()||!game.world().openEastBoundary()||game.world().openNorthBoundary()||game.world().openWestBoundary()||!game.world().fits(game.player().pos.x,game.player().pos.y,game.player().z,game.player().hullHeight()))return fail(__LINE__);
 for(int level=0;level<game.chunkCount();++level){auto&w=game.m_chunks[level].world;int col=level>=12?4:level%4,row=level>=12?level-12:level/4;bool north=row>0,south=row<2,west=col>0,east=col<4;
  if(!w.horrorMode()||w.openNorthBoundary()!=north||w.openSouthBoundary()!=south||w.openWestBoundary()!=west||w.openEastBoundary()!=east)return fail(__LINE__);
 }
 input.textInput="map coast\r";game.update(input,.02f);
 if(game.level()!=13||!game.world().coast()||game.world().waterSurface(20,12)>0||game.world().floorHeight(20,12)>=game.world().waterSurface(20,12))return fail(__LINE__);
 input.textInput="fps\r";game.update(input,.02f);if(!game.showFps())return fail(__LINE__);
 input={};input.escape=true;game.update(input,.02f);if(game.consoleOpen()||game.paused())return fail(__LINE__);
 report<<"Backtick toggle; paused simulation; all campaign map IDs and reactor; Receiving teleport starts visible encounter after console closes; Ashfall boundaries and coast shortcut; invalid map; fresh lift; FPS; Esc closes: PASS\n";return true;
}
}

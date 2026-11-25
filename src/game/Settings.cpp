#include "Game.h"
#include "../world/CustomCampaign.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>
namespace retro {
int Game::titleRows()const{
 if(!m_customMapsOpen)return TitleMenuLayout::Rows;
 return std::min(6,std::max(1,customMenuItemCount()-m_customMenuOffset));
}
std::string Game::customMenuLabel(int row)const{
 int item=customMenuLogicalIndex(row);
 if(item>=0&&item<int(m_customCampaigns.size()))return m_customCampaigns[size_t(item)]->name;
 return "BACK TO TITLE";
}
void Game::startMainCampaign(int level){
 m_pendingMainCampaign=false;m_customCampaign.reset();m_customCampaignKey=0;m_worldId=WorldId::Campaign;m_titleScreen=false;m_menuFromTitle=false;m_customMapsOpen=false;m_level=level;restart();m_suppressFire=true;
}
// New Game: the night shift in the Security Office, on the campaign grid north
// of the Foundry; the crashed lift's shaft passage walks into Map 0.
void Game::startNewGame(){startMainCampaign(PrologueChunk);}
void Game::selectCustomCampaign(int index){
 if(index<0||index>=int(m_customCampaigns.size()))return;
 startCustomCampaign(m_customCampaigns[size_t(index)]);
}
void Game::startCustomCampaign(std::shared_ptr<const CustomCampaign> campaign){
 m_customCampaign=campaign;m_customCampaignKey=campaign->key;m_worldId=WorldId::Custom;
 m_level=std::clamp(m_customCampaign->startMap,0,int(m_customCampaign->maps.size())-1);
 m_titleScreen=false;m_menuFromTitle=false;m_customMapsOpen=false;restart();m_suppressFire=true;
}
void Game::updateTitle(const InputState& input){
 auto pressed=[](bool now,bool previous){return now&&!previous;};
 // The multiplayer pages have their own rows (MultiplayerMenu.cpp).
 if(m_titlePage!=TitlePage::Main&&!m_customMapsOpen){updateMultiplayerMenu(input);m_titlePrevious=input;return;}
 if(m_customMapsOpen&&pressed(input.escape,m_titlePrevious.escape)){m_customMapsOpen=false;m_titleSelection=TitleCustomMaps;m_titlePrevious=input;return;}
 int titleRowCount=titleRows();
 if(m_customMapsOpen){
  int total=customMenuItemCount();
  if(pressed(input.menuUp,m_titlePrevious.menuUp)){
   if(m_titleSelection>0)--m_titleSelection;
   else if(m_customMenuOffset>0)--m_customMenuOffset;
   else {m_customMenuOffset=std::max(0,total-6);m_titleSelection=titleRows()-1;}
  }
  if(pressed(input.menuDown,m_titlePrevious.menuDown)){
   if(m_titleSelection+1<titleRows())++m_titleSelection;
   else if(m_customMenuOffset+titleRows()<total)++m_customMenuOffset;
   else {m_customMenuOffset=0;m_titleSelection=0;}
  }
  titleRowCount=titleRows();
 }else{
  if(pressed(input.menuUp,m_titlePrevious.menuUp))m_titleSelection=(m_titleSelection+titleRowCount-1)%titleRowCount;
  if(pressed(input.menuDown,m_titlePrevious.menuDown))m_titleSelection=(m_titleSelection+1)%titleRowCount;
 }
 // The pointer: the archive's rows are evenly spaced; the main menu's come in groups (TitleMenuLayout::rowAt).
 const bool across=input.pointerX>=TitleMenuLayout::X-14&&input.pointerX<TitleMenuLayout::X+TitleMenuLayout::Width;
 int under=-1;
 if(across&&m_customMapsOpen){if(input.pointerY>=TitleMenuLayout::Y&&input.pointerY<TitleMenuLayout::Y+titleRowCount*TitleMenuLayout::RowHeight)under=(input.pointerY-TitleMenuLayout::Y)/TitleMenuLayout::RowHeight;}
 else if(across)under=TitleMenuLayout::rowAt(input.pointerY);
 const bool inside=under>=0&&under<titleRowCount;
 bool click=pressed(input.fire,m_titlePrevious.fire);
 if(inside&&(input.pointerX!=m_pointerX||input.pointerY!=m_pointerY||click))m_titleSelection=under;
 m_pointerX=input.pointerX;m_pointerY=input.pointerY;
 bool activate=pressed(input.menuAccept,m_titlePrevious.menuAccept)||(inside&&click);
 if(activate&&m_customMapsOpen){
  int item=customMenuLogicalIndex(m_titleSelection);
  if(item>=0&&item<int(m_customCampaigns.size()))selectCustomCampaign(item);
  else {m_customMapsOpen=false;m_customMenuOffset=0;m_titleSelection=TitleCustomMaps;}
 }else if(activate){
  if(m_titleSelection==TitleNewGame)startNewGame();
  else if(m_titleSelection==TitleCustomMaps){if(!m_customMapDirectory.empty())loadCustomCampaignDirectory(m_customMapDirectory);m_customMapsOpen=true;m_customMenuOffset=0;m_titleSelection=0;}
  else if(m_titleSelection==TitleLoadGame){m_titleScreen=false;m_menuFromTitle=true;m_paused=true;m_menuPage=MenuPage::Load;m_menuSelection=0;m_menuMessage.clear();refreshSaveSlots();m_menuPrevious=input;}
  else if(m_titleSelection==TitleMultiplayer){if(!m_customMapDirectory.empty())loadCustomCampaignDirectory(m_customMapDirectory);openTitlePage(TitlePage::Multiplayer);}
  else if(m_titleSelection==TitleSettings){m_titleScreen=false;m_menuFromTitle=true;m_paused=true;openSettings(m_vrInputActive?SettingsTab::Vr:SettingsTab::Audio);m_menuPrevious=input;}
  else if(m_titleSelection==TitleQuit)m_quitRequested=true;
 }
 m_titlePrevious=input;
}
// The pause menu's pages: PAUSE and SETTINGS (SettingsMenu.cpp), then the save and load slab and its confirmations.
void Game::updateMenu(const InputState& input){
 if(m_menuPage==MenuPage::Pause){updatePauseMenu(input);return;}
 if(m_menuPage==MenuPage::Options){updateSettingsMenu(input);return;}
 auto pressed=[](bool now,bool previous){return now&&!previous;};
 int rows=menuRows();
 if(pressed(input.menuUp,m_menuPrevious.menuUp))m_menuSelection=(m_menuSelection+rows-1)%rows;
 if(pressed(input.menuDown,m_menuPrevious.menuDown))m_menuSelection=(m_menuSelection+1)%rows;
 bool inside=input.pointerX>=MenuLayout::X+12&&input.pointerX<MenuLayout::X+MenuLayout::Width-12&&input.pointerY>=MenuLayout::RowTop&&input.pointerY<MenuLayout::RowTop+rows*MenuLayout::RowHeight;
 bool click=pressed(input.fire,m_menuPrevious.fire);
 if(inside&&(input.pointerX!=m_pointerX||input.pointerY!=m_pointerY||click))m_menuSelection=(input.pointerY-MenuLayout::RowTop)/MenuLayout::RowHeight;
 m_pointerX=input.pointerX;m_pointerY=input.pointerY;
 bool activate=pressed(input.menuAccept,m_menuPrevious.menuAccept)||(inside&&click);
 if(activate){
  if(m_menuPage==MenuPage::Save||m_menuPage==MenuPage::Load){
   bool saving=m_menuPage==MenuPage::Save;int backRow=saving?3:4;
   if(m_menuSelection==backRow){if(m_menuFromTitle){showTitleScreen();m_titlePrevious=input;}else {m_menuPage=MenuPage::Pause;m_menuSelection=saving?PauseSave:PauseLoad;m_menuMessage.clear();}}
   else if(!saving&&m_menuSelection==3){m_menuMessage.clear();loadCheckpoint();}
   else {m_pendingSlot=m_menuSelection;m_menuMessage.clear();
    if(saving){std::error_code error;bool exists=!m_saveDirectory.empty()&&std::filesystem::exists(std::filesystem::path(m_saveDirectory)/("slot-"+std::to_string(m_pendingSlot+1)+".rms"),error);
     if(exists){m_menuPage=MenuPage::Overwrite;m_menuSelection=0;}else saveSlot(m_pendingSlot);
    }else {m_menuPage=MenuPage::ConfirmLoad;m_menuSelection=0;}
   }
  }else {bool saving=m_menuPage==MenuPage::Overwrite;
   if(m_menuSelection==0){m_menuPage=saving?MenuPage::Save:MenuPage::Load;m_menuSelection=m_pendingSlot;}
   else if(saving){saveSlot(m_pendingSlot);m_menuPage=MenuPage::Save;m_menuSelection=m_pendingSlot;}
   else if(!loadSlot(m_pendingSlot)){m_menuPage=MenuPage::Load;m_menuSelection=m_pendingSlot;}
  }
 }
 m_menuPrevious=input;
}
// The settings file: whitespace-separated numbers, each later group appended so an older file still loads (its
// missing groups keep their defaults), then named lines for what can hold spaces (device and player names).
void Game::loadSettings(const std::wstring& path){
 std::ifstream file{std::filesystem::path(path)};Settings settings;int inverted=0;
 if(!(file>>settings.master>>settings.music>>settings.effects>>settings.sensitivity>>inverted))return;
 if(!std::isfinite(settings.master)||!std::isfinite(settings.music)||!std::isfinite(settings.effects)||!std::isfinite(settings.sensitivity))return;
 settings.master=std::clamp(settings.master,0.f,1.f);settings.music=std::clamp(settings.music,0.f,1.f);settings.effects=std::clamp(settings.effects,0.f,1.f);settings.sensitivity=std::clamp(settings.sensitivity,.2f,3.f);settings.invertMouse=inverted!=0;
 float renderScale=1;bool showFps=false;
 // Lens settings came later: older files keep the default look.
 float dof=1,lens=1;if(file>>dof>>lens&&std::isfinite(dof)&&std::isfinite(lens)){settings.depthOfField=std::clamp(dof,0.f,2.f);settings.lensEffects=std::clamp(lens,0.f,2.f);
  int highlight=1;if(file>>highlight)settings.pullHighlight=highlight?1:0;}
 // VR comfort came later still.
 float turn=1,vignette=.5f,scale=0;int seated=0,lefty=0,rate=0;
 if(file>>turn>>vignette>>seated>>lefty>>rate>>scale&&std::isfinite(turn)&&std::isfinite(vignette)&&std::isfinite(scale)){
  settings.vrTurnSpeed=std::clamp(turn,.25f,2.f);settings.vrVignette=std::clamp(vignette,0.f,1.f);settings.vrSeated=seated!=0;settings.vrLeftHanded=lefty!=0;settings.vrFrameRate=rate!=0;
  settings.vrRenderScale=scale<.6f?0.f:std::clamp(scale,.6f,1.f);
  // The multiplayer body came next (0 Claire, 1 the Limitator, 2 Leon).
  int character=0;if(file>>character){settings.character=std::clamp(character,0,2);
   // Then chat and voice, the 3D render scale and the frame-time readout; then the named lines.
   int chat=1,filter=1,mode=1,key=0x56,proximity=1,fps=0;float threshold=.25f,mic=1,voice=1,render=1;
   if(file>>chat>>filter>>mode>>key>>threshold>>mic>>voice>>proximity>>render>>fps&&std::isfinite(threshold)&&std::isfinite(mic)&&std::isfinite(voice)&&std::isfinite(render)){
    settings.textChat=chat!=0;settings.chatFilter=filter!=0;settings.voiceMode=std::clamp(mode,0,2);settings.pushToTalkKey=key>0&&key<256?key:0x56;
    settings.openMicThreshold=std::clamp(threshold,0.f,1.f);settings.micVolume=std::clamp(mic,0.f,2.f);settings.voiceVolume=std::clamp(voice,0.f,2.f);settings.proximityVoice=proximity!=0;
    renderScale=render<.6f?.5f:render<.9f?.75f:1.f;showFps=fps!=0;
    // The view distance came after (a number; the named lines that follow start with a letter).
    file>>std::ws;if(file.peek()>='0'&&file.peek()<='9'){int view=1;if(file>>view)settings.viewDistance=std::clamp(view,0,3);}
    for(std::string line;std::getline(file,line);){
     if(!line.empty()&&line.back()=='\r')line.pop_back();
     const auto space=line.find(' ');if(space==std::string::npos)continue;
     const std::string tag=line.substr(0,space),value=line.substr(space+1);
     if(tag=="name")settings.playerName=value.substr(0,24);
     else if(tag=="pad"){int v[9]={3,0,0,0,0,0,0,0,0};std::istringstream in(value);for(int& x:v)if(!(in>>x))break;
      settings.pad={std::clamp(v[0],0,6),std::clamp(v[1],0,3),std::clamp(v[2],0,3),std::clamp(v[3],0,3),v[4]!=0,v[5]!=0,v[6]!=0,v[7]!=0,v[8]!=0};}
     else if(tag=="mic")settings.micDevice=value;else if(tag=="speakers")settings.speakerDevice=value;
     else if(tag=="muted"&&!value.empty()&&settings.mutedPlayers.size()<256&&!settings.muted(value))settings.mutedPlayers.push_back(value);
    }}}}
 m_settings=settings;m_renderScale=renderScale;m_showFps=showFps;
}
void Game::saveSettings(const std::wstring& path)const{
 std::error_code error;auto filePath=std::filesystem::path(path);std::filesystem::create_directories(filePath.parent_path(),error);if(error)return;
 const auto& s=m_settings;
 std::ofstream file(filePath);file<<s.master<<' '<<s.music<<' '<<s.effects<<' '<<s.sensitivity<<' '<<int(s.invertMouse)<<' '<<s.depthOfField<<' '<<s.lensEffects<<' '<<s.pullHighlight
  <<' '<<s.vrTurnSpeed<<' '<<s.vrVignette<<' '<<int(s.vrSeated)<<' '<<int(s.vrLeftHanded)<<' '<<int(s.vrFrameRate)<<' '<<s.vrRenderScale<<' '<<s.character
  <<' '<<int(s.textChat)<<' '<<int(s.chatFilter)<<' '<<s.voiceMode<<' '<<s.pushToTalkKey<<' '<<s.openMicThreshold<<' '<<s.micVolume<<' '<<s.voiceVolume<<' '<<int(s.proximityVoice)
  <<' '<<m_renderScale<<' '<<int(m_showFps)<<' '<<s.viewDistance<<'\n';
 auto oneLine=[](std::string v){for(auto& c:v)if(c=='\r'||c=='\n')c=' ';return v;};
 file<<"pad "<<s.pad.look<<' '<<s.pad.dead<<' '<<s.pad.steady<<' '<<s.pad.filter<<' '<<int(s.pad.toggleAim)<<' '<<int(s.pad.toggleSprint)<<' '<<int(s.pad.toggleCrouch)<<' '<<int(s.pad.bumpers)<<' '<<int(s.pad.swapSticks)<<'\n';
 if(!s.playerName.empty())file<<"name "<<oneLine(s.playerName)<<'\n';
 if(!s.micDevice.empty())file<<"mic "<<oneLine(s.micDevice)<<'\n';
 if(!s.speakerDevice.empty())file<<"speakers "<<oneLine(s.speakerDevice)<<'\n';
 for(const auto& n:s.mutedPlayers)file<<"muted "<<oneLine(n)<<'\n';
}
// The VR page of SETTINGS (VrRuntime's --vr-test runs this): in VR the pause menu's SETTINGS opens on it; each row
// changes its setting, the values survive a save and load, an older settings file keeps the defaults, and Esc goes
// back through the page column to PAUSE.
bool Game::testComfortMenu(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const char* what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto game=validationScene(Enemy::Kind::Huntsman);game.m_vrInputActive=true;InputState escape{};escape.escape=true;game.update(escape,.02f);game.update({},.02f);
 auto key=[&](bool InputState::* k){InputState i{};i.*k=true;game.update(i,.02f);game.update({},.02f);};
 game.m_menuSelection=PauseSettings;key(&InputState::menuAccept);
 check(game.menuPage()==MenuPage::Options&&game.settingsTab()==SettingsTab::Vr,"VR: PAUSE's SETTINGS opens on the VR page");
 key(&InputState::menuRight);
 const auto rows=game.settingsRows();
 auto pick=[&](int row){game.m_menuSelection=row;key(&InputState::menuAccept);};
 check(rows.size()==8,"the VR page: turn speed, vignette, seated, height, left-handed, render scale, frame-rate readout, reset");
 game.m_menuSelection=0;key(&InputState::menuRight);check(std::fabs(game.settings().vrTurnSpeed-1.125f)<.001f,"turn speed steps up");
 {game.update({},.02f);InputState click{};click.fire=true;click.pointerX=SettingsLayout::SliderX;click.pointerY=SettingsLayout::rowY(1)+8;game.update(click,.02f);click.fire=false;game.update(click,.02f);}
 check(game.settings().vrVignette==0,"the vignette slider drags to off");
 pick(2);pick(4);pick(6);pick(5);pick(5);check(game.settings().vrSeated&&game.settings().vrLeftHanded&&game.settings().vrFrameRate&&std::fabs(game.settings().vrRenderScale-.9f)<.001f,"seated, left-handed, readout on, render scale AUTO -> 100 -> 90");
 pick(3);check(game.takeVrRecalibrate()&&!game.takeVrRecalibrate(),"recalibrate is taken once");
 {auto path=std::filesystem::temp_directory_path()/"rawmetal-comfort-test.txt";game.saveSettings(path.wstring());Game reloaded;reloaded.loadSettings(path.wstring());std::error_code error;std::filesystem::remove(path,error);
  const auto&r=reloaded.settings();check(r.vrSeated&&r.vrLeftHanded&&r.vrFrameRate&&r.vrVignette==0&&std::fabs(r.vrTurnSpeed-1.125f)<.001f&&std::fabs(r.vrRenderScale-.9f)<.001f,"comfort settings survive a save and load");
  std::ofstream(path)<<"1 .75 1 1 0 1 1\n";Game old;old.loadSettings(path.wstring());std::filesystem::remove(path,error);check(old.settings().vrTurnSpeed==1&&old.settings().vrVignette==.5f&&!old.settings().vrSeated,"an older settings file keeps the comfort defaults");}
 key(&InputState::escape);key(&InputState::escape);check(game.menuPage()==MenuPage::Pause&&game.menuSelection()==PauseSettings,"Esc, Esc returns to PAUSE on SETTINGS");
 return ok;
}
bool Game::testSettings(){
 {Game title;title.m_level=3;title.showTitleScreen();InputState accept{};accept.menuAccept=true;title.update(accept,.02f);
 if(title.titleScreen()||title.paused()||title.level()!=PrologueChunk||title.worldId()!=WorldId::Campaign)return false;
 }
 // Custom Maps lists installed campaigns and Back. An empty catalog must
 // return to the title, while an installed pack launches its own world.
 for(bool installed:{false,true}){Game title;
  std::shared_ptr<CustomCampaign> campaign;
  if(installed){campaign=std::make_shared<CustomCampaign>();campaign->name="Settings menu fixture";campaign->key=883016;
   auto map=std::make_shared<AuthoredMapData>();map->name="Menu room";map->definition={{0,0},{6.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
   AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);campaign->maps.push_back(map);title.m_customCampaigns.push_back(campaign);
  }
  title.showTitleScreen();InputState down{};down.menuDown=true;title.update(down,.02f);title.update({},.02f);InputState accept{};accept.menuAccept=true;title.update(accept,.02f);
  if(!title.titleScreen()||!title.customMapsOpen()||title.customMenuLabel(0)!=(installed?"Settings menu fixture":"BACK TO TITLE"))return false;
  title.update({},.02f);title.update(accept,.02f);
  if(installed){if(title.titleScreen()||title.worldId()!=WorldId::Custom||title.m_customCampaign!=campaign)return false;}
  else if(!title.titleScreen()||title.customMapsOpen())return false;
 }
 auto game=validationScene(Enemy::Kind::Huntsman);InputState escape{};escape.escape=true;game.update(escape,.02f);
 if(!game.paused()||game.quitRequested()||game.menuPage()!=MenuPage::Pause)return false;
 game.update(escape,.02f);if(!game.paused())return false;
 auto before=game.player();auto enemy=game.enemies()[0];float time=game.elapsed();
 InputState moving{};moving.forward=true;moving.fire=true;moving.mouseDx=100;
 for(int i=0;i<120;++i)game.update(moving,1.f/60.f);
 if(game.elapsed()!=time||game.player().pools!=before.pools||game.player().health!=before.health||game.player().angle!=before.angle||lengthSq(game.player().pos-before.pos)>0||lengthSq(game.enemies()[0].pos-enemy.pos)>0||!game.sounds().empty())return false;
 // PAUSE -> SETTINGS (AUDIO) -> the rows: left lowers the master volume a step; Esc, Esc, Esc resumes.
 auto key=[&](bool InputState::* k){InputState i{};i.*k=true;game.update(i,.02f);game.update({},.02f);};
 game.update({},.02f);key(&InputState::menuDown);if(game.menuSelection()!=PauseSettings)return false;
 key(&InputState::menuAccept);if(game.menuPage()!=MenuPage::Options)return false;
 key(&InputState::menuRight);key(&InputState::menuLeft);if(std::fabs(game.settings().master-.95f)>.001f)return false;
 key(&InputState::escape);key(&InputState::escape);if(game.menuPage()!=MenuPage::Pause||!game.paused())return false;
 game.update(escape,.02f);if(game.paused())return false;
 moving.forward=false;moving.mouseDx=0;game.update(moving,.02f);if(game.player().pools!=before.pools)return false;
 game.update({},.02f);game.update(moving,.02f);if(game.player().pool(WeaponId::Shotgun).total!=before.pool(WeaponId::Shotgun).total-1)return false;
 game.update(escape,.02f);if(!game.paused())return false;game.update({},.02f);
 // Into SETTINGS by the pointer: the music slider pressed at its left end.
 auto clickAt=[&](int x,int y){game.update({},.02f);InputState i{};i.pointerX=x;i.pointerY=y;i.fire=true;game.update(i,.02f);i.fire=false;game.update(i,.02f);};
 clickAt(PauseMenuLayout::X+40,PauseMenuLayout::rowY(PauseSettings,false)+8);if(game.menuPage()!=MenuPage::Options||game.settingsTab()!=SettingsTab::Audio)return false;
 InputState click{};click.pointerX=SettingsLayout::SliderX;click.pointerY=SettingsLayout::rowY(1)+8;click.fire=true;game.update(click,.02f);if(game.settings().music!=0)return false;
 auto settingsPath=(std::filesystem::current_path()/"settings-roundtrip.ini").wstring();game.saveSettings(settingsPath);Game restored;restored.loadSettings(settingsPath);
 if(std::fabs(restored.settings().master-.95f)>.001f||restored.settings().music!=0)return false;
 // Hold the handle, leave its row, then release: the original slider owns the drag.
 click.pointerX=SettingsLayout::SliderX+SettingsLayout::SliderWidth/2;click.pointerY=SettingsLayout::rowY(3)+4;game.update(click,.02f);
 if(std::fabs(game.settings().music-.5f)>.02f||game.menuSelection()!=1)return false;
 click.pointerX=SettingsLayout::SliderX+SettingsLayout::SliderWidth+40;game.update(click,.02f);if(game.settings().music!=1)return false;
 click.fire=false;game.update(click,.02f);click.pointerX=SettingsLayout::SliderX;game.update(click,.02f);if(game.settings().music!=1)return false;
 // Mouse settings must change actual aiming, independent of frame rate.
 auto normal=validationScene(Enemy::Kind::Huntsman,3),inverted=normal;inverted.m_settings.sensitivity=2;inverted.m_settings.invertMouse=true;
 InputState aim{};aim.mouseDx=10;aim.mouseDy=10;normal.update(aim,.02f);inverted.update(aim,.02f);
 if(std::fabs(inverted.player().angle-2*normal.player().angle)>.0001f||std::fabs(inverted.player().pitch+2*normal.player().pitch)>.0001f)return false;
 // Equal mouse counts must rotate both axes by the same angular distance.
 if(std::fabs(normal.player().angle+normal.player().pitch/140.f)>.0001f)return false;
 auto looking=validationScene(Enemy::Kind::Huntsman,3);
 aim.mouseDx=0;aim.mouseDy=-100000;looking.update(aim,.02f);
 if(std::fabs(looking.player().pitch-210.f)>.001f)return false;
 aim.mouseDy=100000;looking.update(aim,.02f);
 if(std::fabs(looking.player().pitch+210.f)>.001f)return false;
 // Each slider dragged from 15 px to 60 px along: AUDIO's three volumes, then CONTROLS' sensitivity.
 const float at60=60.f/SettingsLayout::SliderWidth;
 auto drag=[&](int row){game.update({},.02f);InputState d{};d.fire=true;d.pointerX=SettingsLayout::SliderX+15;d.pointerY=SettingsLayout::rowY(row)+8;game.update(d,.02f);
  d.pointerX=SettingsLayout::SliderX+60;d.pointerY+=50;game.update(d,.02f);const bool held=game.menuSelection()==row;d.fire=false;game.update(d,.02f);return held;};
 for(int row=0;row<3;++row){if(!drag(row))return false;const float value=row==0?game.settings().master:row==1?game.settings().music:game.settings().effects;if(std::fabs(value-at60)>.001f)return false;}
 clickAt(SettingsLayout::TabX+20,SettingsLayout::tabY(int(SettingsTab::Controls))+6);if(game.settingsTab()!=SettingsTab::Controls)return false;
 if(!drag(0)||std::fabs(game.settings().sensitivity-(.2f+2.8f*at60))>.001f)return false;
 // VIDEO's lens sliders run 0-200 %, and the values survive a settings save and load.
 clickAt(SettingsLayout::TabX+20,SettingsLayout::tabY(int(SettingsTab::Video))+6);
 for(int row:{1,2}){if(!drag(row))return false;const float value=row==1?game.settings().depthOfField:game.settings().lensEffects;if(std::fabs(value-2*at60)>.001f)return false;}
 {auto path=std::filesystem::temp_directory_path()/"rawmetal-settings-test.txt";game.saveSettings(path.wstring());Game reloaded;reloaded.loadSettings(path.wstring());
  std::error_code error;std::filesystem::remove(path,error);
  if(std::fabs(reloaded.settings().depthOfField-2*at60)>.001f||std::fabs(reloaded.settings().lensEffects-2*at60)>.001f)return false;}
 // GAMEPLAY: the pull highlight row toggles SUBTLE / NONE on a click, and NONE survives a save and load.
 clickAt(SettingsLayout::TabX+20,SettingsLayout::tabY(int(SettingsTab::Gameplay))+6);
 {clickAt(SettingsLayout::RowX+30,SettingsLayout::rowY(0)+6);
  if(game.settings().pullHighlight!=0||game.menuSelection()!=0)return false;
  auto path=std::filesystem::temp_directory_path()/"rawmetal-settings-highlight.txt";game.saveSettings(path.wstring());Game reloaded;reloaded.loadSettings(path.wstring());
  std::error_code error;std::filesystem::remove(path,error);if(reloaded.settings().pullHighlight!=0)return false;
  clickAt(SettingsLayout::RowX+30,SettingsLayout::rowY(0)+6);if(game.settings().pullHighlight!=1)return false;}
 // BACK to PAUSE; RESTART asks twice, then the game starts over.
 clickAt(SettingsLayout::TabX+20,SettingsLayout::tabY(int(SettingsTab::Count))+6);
 if(game.menuPage()!=MenuPage::Pause||!game.paused()||!game.takeSettingsSave())return false;
 game.m_player.health=40;
 clickAt(PauseMenuLayout::X+40,PauseMenuLayout::rowY(PauseRestart,false)+8);if(!game.paused()||game.menuArmed()!=PauseRestart||game.player().health!=40)return false;
 clickAt(PauseMenuLayout::X+40,PauseMenuLayout::rowY(PauseRestart,false)+8);if(game.paused()||game.player().health!=100)return false;
 // QUIT TO TITLE asks twice too.
 InputState reopen{};reopen.escape=true;game.update(reopen,.02f);game.update({},.02f);
 clickAt(PauseMenuLayout::X+40,PauseMenuLayout::rowY(PauseQuit,false)+8);if(game.titleScreen())return false;
 clickAt(PauseMenuLayout::X+40,PauseMenuLayout::rowY(PauseQuit,false)+8);
 if(!game.titleScreen()||game.quitRequested())return false;
 game.update({},.02f);game.m_titleSelection=TitleQuit;InputState quit{};quit.menuAccept=true;game.update(quit,.02f);if(!game.quitRequested())return false;
 {std::ofstream menuReport("settings-menu-test.txt");if(!testSettingsMenu(menuReport))return false;}
 std::ofstream("settings-test.txt")<<"Title new-game reset; Escape toggling, frozen gameplay, PAUSE and SETTINGS pages, restart and quit confirmations, keyboard/mouse controls, resume fire suppression, aiming settings, persistence, quit to title and explicit quit: PASS\n";
 return true;
}
}
namespace retro {
float Game::viewDistance()const{
 if(m_viewOverride>0)return std::clamp(m_viewOverride,20.f,1500.f);
 static constexpr float factor[4]={.7f,1.f,2.f,3.5f};
 // A map that names its own distance (MAP view: a megastructure across a gulf) may reach 1500 m.
 const float own=m_world.definition().viewDistance;
 float base=70.f;if(own>0)base=own;else if(const auto* shape=m_world.terrainShape();shape&&shape->viewDistance>0)base=shape->viewDistance;else if(m_world.hasTerrain()&&m_world.zoneSky())base=110.f;
 return std::clamp(base*factor[std::clamp(m_settings.viewDistance,0,3)],20.f,own>0?1500.f:400.f);
}
float Game::edgeReach(Vec2 eye,Vec2 dir,float limit)const{
 // Custom campaigns only: their maps are 24 m chunks laid out by origin. Built-in chapters are closed worlds.
 if(m_worldId!=WorldId::Custom||!m_customCampaign)return limit;
 const Vec2 base=chunkOffset(m_level);const int chunks=chunkCount();
 auto holder=[&](Vec2 p){for(int level=0;level<chunks;++level){if(level!=m_level&&!chunkResident(level))continue;const Vec2 o=chunkOffset(level);
   if(p.x>=o.x&&p.x<o.x+24&&p.y>=o.y&&p.y<o.y+24)return level;}return -1;};
 Vec2 p{base.x+eye.x,base.y+eye.y};int inside=holder(p);if(inside<0)return limit;
 float beyond=0;if(const auto* shape=m_world.terrainShape())beyond=shape->horizonDepth;   // a HORIZON strip is land past the edge
 for(float t=2;t<limit;t+=2){
  const Vec2 q{p.x+dir.x*t,p.y+dir.y*t};const int next=holder(q);if(next>=0){inside=next;continue;}
  // Out of the last map: through which side? A closed side is a wall, so the view stops there anyway.
  const World& w=chunkWorld(inside);const Vec2 o=chunkOffset(inside);const float lx=q.x-o.x,ly=q.y-o.y;
  const bool open=lx<0?w.openWestBoundary():lx>=24?w.openEastBoundary():ly<0?w.openNorthBoundary():w.openSouthBoundary();
  if(!open)return limit;
  return std::min(limit,t+beyond);
 }
 return limit;
}
Game::ViewFade Game::viewFade(Vec2 eye,float yaw,float halfFov)const{
 const float view=viewDistance();ViewFade fade;
 const bool own=m_world.definition().viewDistance>0;
 if((m_world.outdoors()||own)&&view>70.f){fade.start=view*.62f;fade.end=view*.97f;}
 // The nearest open edge with nothing past it across the view (five rays over the field of view).
 // Only where there is no sky to show past the edge (indoors, underground, or a map that names its own view):
 // under an open sky the land simply ends against the sky, and fading near walls of a small yard looked like fog.
 float reach=view;if(!m_world.outdoors()||own)for(int k=-2;k<=2;++k){const float a=yaw+halfFov*float(k)*.5f;reach=std::min(reach,edgeReach(eye,{std::cos(a),std::sin(a)},view));}
 const float target=reach<view?std::max(12.f,reach):-1.f;
 // Eased over about a second as the player turns or crosses into another map, so the fade never pops.
 const float now=m_elapsed,dt=std::clamp(now-m_edgeFadeTime,0.f,.25f);m_edgeFadeTime=now;
 const float goal=target>0?target:view;if(m_edgeFadeEnd<0)m_edgeFadeEnd=goal;else m_edgeFadeEnd+=(goal-m_edgeFadeEnd)*std::min(1.f,dt*3.f);
 if(m_edgeFadeEnd<view*.96f){fade.end=m_edgeFadeEnd;fade.start=std::min(fade.start>0?fade.start:fade.end*.62f,fade.end*.62f);}
 fade.voidTone=own||m_edgeFadeEnd<view*.96f;
 return fade;
}
}

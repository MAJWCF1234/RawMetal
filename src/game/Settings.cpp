#include "Game.h"
#include <filesystem>
#include <fstream>
#include <cmath>
namespace retro {
void Game::updateTitle(const InputState& input){
 auto pressed=[](bool now,bool previous){return now&&!previous;};
 if(pressed(input.menuUp,m_titlePrevious.menuUp))m_titleSelection=(m_titleSelection+TitleMenuLayout::Rows-1)%TitleMenuLayout::Rows;
 if(pressed(input.menuDown,m_titlePrevious.menuDown))m_titleSelection=(m_titleSelection+1)%TitleMenuLayout::Rows;
 bool inside=input.pointerX>=TitleMenuLayout::X&&input.pointerX<TitleMenuLayout::X+TitleMenuLayout::Width&&
             input.pointerY>=TitleMenuLayout::Y&&input.pointerY<TitleMenuLayout::Y+TitleMenuLayout::Rows*TitleMenuLayout::RowHeight;
 bool click=pressed(input.fire,m_titlePrevious.fire);
 if(inside&&(input.pointerX!=m_pointerX||input.pointerY!=m_pointerY||click))m_titleSelection=(input.pointerY-TitleMenuLayout::Y)/TitleMenuLayout::RowHeight;
 m_pointerX=input.pointerX;m_pointerY=input.pointerY;
 bool activate=pressed(input.menuAccept,m_titlePrevious.menuAccept)||(inside&&click);
 if(activate){
  if(m_titleSelection==0){m_titleScreen=false;m_menuFromTitle=false;m_level=0;restart();m_suppressFire=true;}
  else if(m_titleSelection==1){m_titleScreen=false;m_menuFromTitle=true;m_paused=true;m_menuPage=MenuPage::Load;m_menuSelection=0;m_menuMessage.clear();refreshSaveSlots();m_menuPrevious=input;}
  else if(m_titleSelection==2){m_titleScreen=false;m_menuFromTitle=true;m_paused=true;m_menuPage=MenuPage::Settings;m_menuSelection=1;m_menuMessage.clear();m_menuPrevious=input;}
  else if(m_titleSelection==3)m_quitRequested=true;
 }
 m_titlePrevious=input;
}
void Game::updateMenu(const InputState& input){
 auto pressed=[](bool now,bool previous){return now&&!previous;};
 int rows=menuRows();
 if(pressed(input.menuUp,m_menuPrevious.menuUp))m_menuSelection=(m_menuSelection+rows-1)%rows;
 if(pressed(input.menuDown,m_menuPrevious.menuDown))m_menuSelection=(m_menuSelection+1)%rows;
 bool inside=input.pointerX>=MenuLayout::X+12&&input.pointerX<MenuLayout::X+MenuLayout::Width-12&&input.pointerY>=MenuLayout::RowTop&&input.pointerY<MenuLayout::RowTop+rows*MenuLayout::RowHeight;
 bool click=pressed(input.fire,m_menuPrevious.fire);
 if(!input.fire)m_dragSlider=-1;
 if(m_dragSlider<0&&inside&&(input.pointerX!=m_pointerX||input.pointerY!=m_pointerY||click))m_menuSelection=(input.pointerY-MenuLayout::RowTop)/MenuLayout::RowHeight;
 if(m_menuPage==MenuPage::Settings&&click&&inside&&m_menuSelection>=1&&m_menuSelection<=4&&input.pointerX>=MenuLayout::SliderX-5&&input.pointerX<=MenuLayout::SliderX+MenuLayout::SliderWidth+5)m_dragSlider=m_menuSelection;
 if(m_dragSlider>=0)m_menuSelection=m_dragSlider;
 m_pointerX=input.pointerX;m_pointerY=input.pointerY;
 int direction=int(pressed(input.menuRight,m_menuPrevious.menuRight))-int(pressed(input.menuLeft,m_menuPrevious.menuLeft));
 bool activate=pressed(input.menuAccept,m_menuPrevious.menuAccept)||(inside&&click);
 if(m_menuPage!=MenuPage::Settings){
  if(m_menuPage==MenuPage::ConfirmRestart){
   if(activate){if(m_menuSelection==0){m_menuPage=MenuPage::Settings;m_menuSelection=6;}
    else {restart();m_paused=false;m_menuPage=MenuPage::Settings;m_menuSelection=0;m_suppressFire=true;}}
   m_menuPrevious=input;return;
  }
  if(activate){
   if(m_menuPage==MenuPage::Save||m_menuPage==MenuPage::Load){
    bool saving=m_menuPage==MenuPage::Save;
    if(m_menuSelection==3){if(m_menuFromTitle)showTitleScreen();else {m_menuPage=MenuPage::Settings;m_menuSelection=saving?7:8;m_menuMessage.clear();}}
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
  m_menuPrevious=input;return;
 }
 float* value=m_menuSelection==1?&m_settings.master:m_menuSelection==2?&m_settings.music:m_menuSelection==3?&m_settings.effects:m_menuSelection==4?&m_settings.sensitivity:nullptr;
 if(value){
  float low=m_menuSelection==4?.2f:0,high=m_menuSelection==4?3.f:1.f;
  if(direction)*value=std::clamp(*value+direction*(m_menuSelection==4?.1f:.05f),low,high);
  if(m_dragSlider>=0)*value=low+(high-low)*std::clamp(float(input.pointerX-MenuLayout::SliderX)/MenuLayout::SliderWidth,0.f,1.f);
  if(direction||m_dragSlider>=0){
   if(m_menuSelection==1)m_audioMuted=false;if(m_menuSelection==2)m_musicEnabled=true;
  }
 }
 if(m_menuSelection==5&&(activate||direction))m_settings.invertMouse=!m_settings.invertMouse;
 if(activate&&m_menuSelection==0){if(m_menuFromTitle)showTitleScreen();else {m_paused=false;m_suppressFire=true;}}
 if(activate&&m_menuSelection==6){m_menuPage=MenuPage::ConfirmRestart;m_menuSelection=0;m_menuMessage.clear();}
 if(activate&&(m_menuSelection==7||m_menuSelection==8)){m_menuPage=m_menuSelection==7?MenuPage::Save:MenuPage::Load;m_menuSelection=0;m_dragSlider=-1;m_menuMessage.clear();refreshSaveSlots();}
 if(activate&&m_menuSelection==9)m_quitRequested=true;
 m_menuPrevious=input;
}
void Game::loadSettings(const std::wstring& path){
 std::ifstream file{std::filesystem::path(path)};Settings settings;int inverted=0;
 if(!(file>>settings.master>>settings.music>>settings.effects>>settings.sensitivity>>inverted))return;
 if(!std::isfinite(settings.master)||!std::isfinite(settings.music)||!std::isfinite(settings.effects)||!std::isfinite(settings.sensitivity))return;
 settings.master=std::clamp(settings.master,0.f,1.f);settings.music=std::clamp(settings.music,0.f,1.f);settings.effects=std::clamp(settings.effects,0.f,1.f);settings.sensitivity=std::clamp(settings.sensitivity,.2f,3.f);settings.invertMouse=inverted!=0;m_settings=settings;
}
void Game::saveSettings(const std::wstring& path)const{
 std::error_code error;auto filePath=std::filesystem::path(path);std::filesystem::create_directories(filePath.parent_path(),error);if(error)return;
 std::ofstream file(filePath);file<<m_settings.master<<' '<<m_settings.music<<' '<<m_settings.effects<<' '<<m_settings.sensitivity<<' '<<int(m_settings.invertMouse)<<'\n';
}
bool Game::testSettings(){
 {Game title;title.m_level=3;title.showTitleScreen();InputState accept{};accept.menuAccept=true;title.update(accept,.02f);
  if(title.titleScreen()||title.paused()||title.level()!=0)return false;
 }
 auto game=validationScene(Enemy::Kind::Huntsman);InputState escape{};escape.escape=true;game.update(escape,.02f);
 if(!game.paused()||game.quitRequested())return false;
 game.update(escape,.02f);if(!game.paused())return false;
 auto before=game.player();auto enemy=game.enemies()[0];float time=game.elapsed();
 InputState moving{};moving.forward=true;moving.fire=true;moving.mouseDx=100;
 for(int i=0;i<120;++i)game.update(moving,1.f/60.f);
 if(game.elapsed()!=time||game.player().ammo!=before.ammo||game.player().health!=before.health||game.player().angle!=before.angle||lengthSq(game.player().pos-before.pos)>0||lengthSq(game.enemies()[0].pos-enemy.pos)>0||!game.sounds().empty())return false;
 game.update({},.02f);InputState down{};down.menuDown=true;game.update(down,.02f);game.update({},.02f);
 InputState left{};left.menuLeft=true;game.update(left,.02f);if(std::fabs(game.settings().master-.95f)>.001f)return false;
 game.update({},.02f);game.update(escape,.02f);if(game.paused())return false;
 moving.forward=false;moving.mouseDx=0;game.update(moving,.02f);if(game.player().ammo!=before.ammo)return false;
 game.update({},.02f);game.update(moving,.02f);if(game.player().ammo!=before.ammo-1)return false;
 game.update(escape,.02f);if(!game.paused())return false;game.update({},.02f);
 InputState click{};click.pointerX=MenuLayout::SliderX;click.pointerY=MenuLayout::RowTop+2*MenuLayout::RowHeight+5;click.fire=true;game.update(click,.02f);if(game.settings().music!=0)return false;
 auto settingsPath=(std::filesystem::current_path()/"settings-roundtrip.ini").wstring();game.saveSettings(settingsPath);Game restored;restored.loadSettings(settingsPath);
 if(std::fabs(restored.settings().master-.95f)>.001f||restored.settings().music!=0)return false;
 // Hold the handle, leave its row, then release: the original slider owns the drag.
 click.pointerX=MenuLayout::SliderX+MenuLayout::SliderWidth/2;click.pointerY=MenuLayout::RowTop+4*MenuLayout::RowHeight;game.update(click,.02f);
 if(std::fabs(game.settings().music-.493333f)>.02f||game.menuSelection()!=2)return false;
 click.pointerX=MenuLayout::SliderX+MenuLayout::SliderWidth+40;game.update(click,.02f);if(game.settings().music!=1)return false;
 click.fire=false;game.update(click,.02f);click.pointerX=MenuLayout::SliderX;game.update(click,.02f);if(game.settings().music!=1)return false;
 click.fire=true;
 // Mouse settings must change actual aiming, independent of frame rate.
 auto normal=validationScene(Enemy::Kind::Huntsman,3),inverted=normal;inverted.m_settings.sensitivity=2;inverted.m_settings.invertMouse=true;
 InputState aim{};aim.mouseDx=10;aim.mouseDy=10;normal.update(aim,.02f);inverted.update(aim,.02f);
 if(std::fabs(inverted.player().angle-2*normal.player().angle)>.0001f||std::fabs(inverted.player().pitch+2*normal.player().pitch)>.0001f)return false;
 for(int row=1;row<=4;++row){game.update({},.02f);click.fire=true;click.pointerX=MenuLayout::SliderX+15;click.pointerY=MenuLayout::RowTop+row*MenuLayout::RowHeight+9;game.update(click,.02f);
  click.pointerX=MenuLayout::SliderX+60;click.pointerY+=50;game.update(click,.02f);
  float value=row==1?game.settings().master:row==2?game.settings().music:row==3?game.settings().effects:game.settings().sensitivity;
  if(std::fabs(value-(row==4?2.44f:.8f))>.001f||game.menuSelection()!=row)return false;
  click.fire=false;game.update(click,.02f);
 }
 click.fire=true;
 game.update({},.02f);click.pointerX=MenuLayout::X+30;click.pointerY=MenuLayout::RowTop+6*MenuLayout::RowHeight+5;game.update(click,.02f);
 if(game.menuPage()!=MenuPage::ConfirmRestart||!game.paused())return false;
 game.update({},.02f);click.pointerY=MenuLayout::RowTop+0*MenuLayout::RowHeight+5;game.update(click,.02f);if(game.menuPage()!=MenuPage::Settings)return false;
 game.update({},.02f);click.pointerY=MenuLayout::RowTop+6*MenuLayout::RowHeight+5;game.update(click,.02f);game.update({},.02f);click.pointerY=MenuLayout::RowTop+1*MenuLayout::RowHeight+5;game.update(click,.02f);
 if(game.paused()||game.player().health!=100)return false;
 InputState reopen{};reopen.escape=true;game.update(reopen,.02f);game.update({},.02f);click.pointerY=MenuLayout::RowTop+9*MenuLayout::RowHeight+5;game.update(click,.02f);
 if(!game.quitRequested())return false;
 std::ofstream("settings-test.txt")<<"Title new-game reset; Escape toggling, frozen gameplay, restart confirmation, keyboard/mouse controls, resume fire suppression, aiming settings, persistence and explicit quit: PASS\n";
 return true;
}
}

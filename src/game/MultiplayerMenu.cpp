#include "Game.h"
#include "../net/Loopback.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string_view>
// The multiplayer pages on the title (docs/Systems/multiplayer.md): QUICK MATCH, BROWSE, HOST (then the lobby with
// its join code), JOIN BY CODE and CHARACTER. They call the Session directly and pump it while no Match does; once
// the session is InLobby the Match takes over and fills remotePlayers and matchView. The rows are built here as
// TitleItems (place, words, whether they can be used) and the renderer draws the same list (renderer/FrontEnd.cpp),
// so the pointer, the keys, a controller and the VR panel's laser all hit what is drawn.
namespace retro {
bool copyTextToClipboard(const std::string& text);
namespace {
enum Action:int{ActNone,ActQuick,ActBrowse,ActHost,ActJoin,ActCharacter,ActPlayerName,ActBack,ActQuickDeathmatch,ActQuickCoop,ActRefresh,ActMode,ActHideFull,
 ActLobby,ActHostMode,ActHostMap,ActHostPlayers,ActHostPrivate,ActHostName,ActHostGo,ActCopy,ActLeave,ActJoinField,ActJoinGo,ActClaire,ActLimitator,ActLeon,ActNameKey};
constexpr int BrowseTop=190,BrowseRow=18,BrowseRows=6;
constexpr int NameColumns=10,NameRows=4;
constexpr std::string_view NameLetters="1234567890abcdefghijklmnopqrstuvwxyz";
constexpr int NameSpace=int(NameLetters.size()),NameShift=NameSpace+1,NameDelete=NameSpace+2,NameDone=NameSpace+3;
static_assert(NameDone+1==NameColumns*NameRows);

void editName(std::string& name,std::string_view text){
 for(char c:text){
  if(c=='\b'){if(!name.empty())name.pop_back();continue;}
  if(c>=32&&c<127&&c!='|'&&name.size()<24)name+=c;
 }
}
// Game::update ticks the session itself (Match.cpp updateNet).
void tickUpdate(Game& g,const InputState& in,float dt){g.update(in,dt);}   // Game::update pumps the session itself
const char* characterName(int c){return c==2?"LEON":c==1?"THE LIMITATOR":"CLAIRE";}
// The map's content hash for HostOptions::mapHash: FNV-1a 64 of the .rmc file's bytes as 16 hex digits, the same
// as net::mapFileHash (Match.h), which a guest compares it with (switch to that one call once both are in one tree).
// A campaign with no file (the embedded Exclusion Zone) hashes its key instead.
std::string mapHashOf(const std::wstring& directory,const CustomCampaign& c){
 std::uint64_t h=1469598103934665603ull;bool read=false;
 if(!directory.empty()&&!c.sourceFile.empty()){std::ifstream in(std::filesystem::path(directory)/c.sourceFile,std::ios::binary);char buffer[4096];
  while(in.read(buffer,sizeof buffer)||in.gcount()>0){read=true;for(std::streamsize i=0;i<in.gcount();++i){h^=static_cast<unsigned char>(buffer[i]);h*=1099511628211ull;}if(!in)break;}}
 if(!read)h^=c.key;
 char text[17];std::snprintf(text,sizeof text,"%016llx",static_cast<unsigned long long>(h));return text;
}
}

std::string Game::sessionWord()const{
 if(!m_netSession)return "OFFLINE";
 switch(m_netSession->state()){
  case net::SessionState::Offline:return "OFFLINE";
  case net::SessionState::SigningIn:return "SIGNING IN";
  case net::SessionState::Ready:return "SIGNED IN";
  case net::SessionState::Busy:return "WORKING";
  case net::SessionState::InLobby:return "IN A LOBBY";
  case net::SessionState::Failed:return "SIGNED IN";
 }
 return "OFFLINE";
}
std::string Game::sessionReason()const{
 if(!m_netSession)return "This build carries no multiplayer session. The descent goes on alone.";
 const std::string why=m_netSession->reason();
 switch(m_netSession->state()){
  case net::SessionState::Offline:return why.empty()?"Epic Online Services cannot be reached. The descent goes on alone.":why;
  case net::SessionState::SigningIn:return "Signing in to Epic Online Services.";
  case net::SessionState::Ready:return "Signed in as "+m_netSession->displayName()+".";
  case net::SessionState::Busy:return "Waiting on Epic Online Services.";
  case net::SessionState::InLobby:return "In "+m_netSession->lobby().name+".";
  case net::SessionState::Failed:return why.empty()?"The last request failed.":why;
 }
 return why;
}
bool Game::sessionUsable()const{
 if(!m_netSession)return false;const auto s=m_netSession->state();
 return s==net::SessionState::Ready||s==net::SessionState::Failed;
}
std::vector<int> Game::hostableMaps(net::Mode mode)const{
 std::vector<int> out;
 for(size_t i=0;i<m_customCampaigns.size();++i){const auto& c=m_customCampaigns[i];if(!c||c->maps.empty())continue;
  if(mode==net::Mode::Coop?c->multiplayer.coop:c->multiplayer.deathmatch)out.push_back(int(i));}
 return out;
}
std::string Game::customCampaignName(int index)const{
 if(index<0||index>=int(m_customCampaigns.size())||!m_customCampaigns[size_t(index)])return {};
 return m_customCampaigns[size_t(index)]->name;
}

// The session's events belong to the menus (the Match never polls them): a failure, a closed lobby or a new host
// becomes the line of plain words under the multiplayer rows. Drained every update, in a match too, so none pile up.
void Game::drainSessionEvents(){
 if(!m_netSession)return;
 net::SessionEvent e;
 while(m_netSession->pollEvent(e)){
  if(e.kind==net::SessionEvent::Error)m_titleNotice=e.text.empty()?"The last request failed.":e.text;
  else if(e.kind==net::SessionEvent::LobbyClosed)m_titleNotice="The lobby closed.";
  else if(e.kind==net::SessionEvent::HostChanged&&e.peer!=m_netSession->self())m_titleNotice="The host left; the lobby passed to another player.";
  else if(e.kind==net::SessionEvent::Hosted||e.kind==net::SessionEvent::Joined)m_titleNotice.clear();}
}
void Game::openTitlePage(TitlePage page){
 m_titlePage=page;m_titleSelection=0;m_customMapsOpen=false;
 if(page==TitlePage::Main){m_titleSelection=TitleMultiplayer;return;}
 if(page==TitlePage::Browse){m_browseScroll=0;m_browseSearched=false;if(sessionUsable())browseRefresh();}
 if(page==TitlePage::Host){
  if(m_hostDraft.name.empty())m_hostDraft.name=m_netSession&&!m_netSession->displayName().empty()?m_netSession->displayName()+"'s game":"A game below";
  auto maps=hostableMaps(m_hostDraft.mode);if(std::find(maps.begin(),maps.end(),m_hostDraft.map)==maps.end())m_hostDraft.map=maps.empty()?-1:maps.front();}
 if(page==TitlePage::QuickMatch)m_quickSearching=false;
 if(page==TitlePage::Character)m_titleSelection=std::clamp(m_settings.character,0,2);
 // The first row that can be used.
 const auto items=titleItems();
 if(page!=TitlePage::Character)for(size_t i=0;i<items.size();++i)if(items[i].enabled){m_titleSelection=int(i);break;}
}
void Game::browseRefresh(){
 if(!sessionUsable())return;
 net::SearchFilter filter;filter.anyMode=m_browseMode==0;filter.mode=m_browseMode==2?net::Mode::Coop:net::Mode::Deathmatch;filter.hideFull=m_browseHideFull;
 m_netSession->search(filter);m_browseSearched=true;m_browseScroll=0;
}
void Game::leaveMatch(){
 if(m_netSession)m_netSession->leave();
 m_matchView={};m_remotePlayers.clear();
 showTitleScreen();openTitlePage(TitlePage::Multiplayer);
}

std::vector<TitleItem> Game::titleItems()const{
 std::vector<TitleItem> items;const bool usable=sessionUsable();
 constexpr int X=TitleMenuLayout::X,Y=TitleMenuLayout::Y,H=TitleMenuLayout::RowHeight,G=TitleMenuLayout::Gap,W=280;
 auto add=[&](TitleItem::Kind kind,int x,int y,int w,int h,std::string label,std::string value,bool enabled,int action,int lobby=-1){
  TitleItem t;t.kind=kind;t.x=x;t.y=y;t.w=w;t.h=h;t.label=std::move(label);t.value=std::move(value);t.enabled=enabled;t.action=action;t.lobby=lobby;items.push_back(std::move(t));};
 auto row=[&](int y,std::string label,std::string value,bool enabled,int action){add(TitleItem::Row,X,y,W,H,std::move(label),std::move(value),enabled,action);};
 auto back=[&](int y){add(TitleItem::Back,X,y,W,H,"BACK","",true,ActBack);};
 const bool busy=m_netSession&&m_netSession->state()==net::SessionState::Busy;
 switch(m_titlePage){
  case TitlePage::Main:break;
  case TitlePage::Multiplayer:
   row(Y,"QUICK MATCH","",usable,ActQuick);row(Y+H,"BROWSE GAMES","",usable,ActBrowse);row(Y+2*H,"HOST A GAME","",usable,ActHost);row(Y+3*H,"JOIN BY CODE","",usable,ActJoin);
   row(Y+4*H+G,"NAME",m_settings.playerName.empty()?(m_netSession?m_netSession->displayName():std::string()):m_settings.playerName+(m_titleSelection==4?"_":""),true,ActPlayerName);
   row(Y+5*H+G,"CHARACTER",characterName(m_settings.character),true,ActCharacter);back(Y+6*H+2*G);break;
  case TitlePage::PlayerName:
   for(int key=0;key<=NameDone;++key){
    std::string label;
    if(key<NameSpace){
     const char c=NameLetters[size_t(key)];
     label=m_nameUpper?char(std::toupper(static_cast<unsigned char>(c))):c;
    }else if(key==NameSpace)label="SPACE";
    else if(key==NameShift)label=m_nameUpper?"ABC":"abc";
    else if(key==NameDelete)label="DEL";
    else label="DONE";
    add(TitleItem::Chip,X+key%NameColumns*54,180+key/NameColumns*30,50,25,std::move(label),"",key!=NameDone||m_nameDraft.size()>=2,ActNameKey);
   }
   break;
  case TitlePage::QuickMatch:
   row(Y,"DEATHMATCH",busy&&m_quickSearching&&m_quickMode==1?"SEARCHING":"",usable,ActQuickDeathmatch);
   row(Y+H,"CO-OP",busy&&m_quickSearching&&m_quickMode==2?"SEARCHING":"",usable,ActQuickCoop);back(Y+2*H+G);break;
  case TitlePage::Browse:{
   add(TitleItem::Chip,X,Y,78,18,"REFRESH","",usable,ActRefresh);
   add(TitleItem::Chip,X+84,Y,128,18,"MODE",m_browseMode==0?"ANY":m_browseMode==1?"DEATHMATCH":"CO-OP",true,ActMode);
   add(TitleItem::Chip,X+218,Y,108,18,"HIDE FULL",m_browseHideFull?"ON":"OFF",true,ActHideFull);
   if(m_netSession){const auto& found=m_netSession->results();int shown=0,listed=0;
    // Nearest first (by ping; unknown last), as the browser promises.
    std::vector<size_t> order(found.size());for(size_t i=0;i<order.size();++i)order[i]=i;
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){const int pa=found[a].pingMs<0?1<<30:found[a].pingMs,pb=found[b].pingMs<0?1<<30:found[b].pingMs;return pa<pb;});
    for(size_t i:order){const auto& l=found[i];
     if(m_browseMode&&int(l.mode)!=m_browseMode)continue;
     const bool full=l.players>=l.maxPlayers;if(full&&m_browseHideFull)continue;
     if(listed++<m_browseScroll)continue;if(shown>=BrowseRows)continue;
     add(TitleItem::Lobby,X,BrowseTop+shown*BrowseRow,DisplayWidth-2*X,BrowseRow,l.name,l.map,usable&&!full,ActLobby,int(i));++shown;}}
   add(TitleItem::Back,X,BrowseTop+BrowseRows*BrowseRow+8,120,18,"BACK","",true,ActBack);break;}
  case TitlePage::Host:{
   const auto maps=hostableMaps(m_hostDraft.mode);const bool haveMap=std::find(maps.begin(),maps.end(),m_hostDraft.map)!=maps.end();
   row(Y,"MODE",net::modeName(m_hostDraft.mode),true,ActHostMode);
   row(Y+H,"MAP",haveMap?customCampaignName(m_hostDraft.map):"NONE ALLOWS THIS MODE",!maps.empty(),ActHostMap);
   row(Y+2*H,"PLAYERS",std::to_string(m_hostDraft.maxPlayers),true,ActHostPlayers);
   row(Y+3*H,"PRIVATE",m_hostDraft.privateGame?"ON":"OFF",true,ActHostPrivate);
   add(TitleItem::Field,X,Y+4*H,W,H,"NAME",m_hostDraft.name,true,ActHostName);
   row(Y+5*H+G,busy?"OPENING THE LOBBY":"HOST GAME","",usable&&haveMap&&!m_hostDraft.name.empty(),ActHostGo);back(Y+6*H+G);break;}
  case TitlePage::Lobby:{
   const bool code=m_netSession&&m_netSession->state()==net::SessionState::InLobby&&!m_netSession->lobby().joinCode.empty();
   row(Y+76,"COPY CODE",m_joinCopied>0?"COPIED":"",code,ActCopy);row(Y+76+H,"LEAVE","",true,ActLeave);break;}
  case TitlePage::JoinCode:
   add(TitleItem::Field,X,Y,220,34,"CODE",m_joinTyped,true,ActJoinField);
   row(Y+46,busy?"JOINING":"JOIN",net::normaliseJoinCode(m_joinTyped).empty()?"":net::normaliseJoinCode(m_joinTyped),usable&&!net::normaliseJoinCode(m_joinTyped).empty(),ActJoinGo);back(Y+46+H+G);break;
  case TitlePage::Character:
   row(Y,"CLAIRE",m_settings.character==0?"IN USE":"",true,ActClaire);row(Y+H,"THE LIMITATOR",m_settings.character==1?"IN USE":"",true,ActLimitator);row(Y+2*H,"LEON",m_settings.character==2?"IN USE":"",true,ActLeon);back(Y+3*H+G);break;
 }
 return items;
}

void Game::savePlayerName(std::string name){
 m_settings.playerName=std::move(name);
 if(m_netSession&&m_settings.playerName.size()>=2)m_netSession->setDisplayName(m_settings.playerName);
 m_settingsSaveDue=true;
}

void Game::updatePlayerNameKeyboard(const InputState& input){
 const auto pressed=[](bool now,bool previous){return now&&!previous;};
 auto close=[&](){openTitlePage(TitlePage::Multiplayer);m_titleSelection=4;};
 if(pressed(input.escape,m_titlePrevious.escape)){close();return;}
 editName(m_nameDraft,input.textInput);
 int row=m_titleSelection/NameColumns+int(pressed(input.menuDown,m_titlePrevious.menuDown))-int(pressed(input.menuUp,m_titlePrevious.menuUp));
 int column=m_titleSelection%NameColumns+int(pressed(input.menuRight,m_titlePrevious.menuRight))-int(pressed(input.menuLeft,m_titlePrevious.menuLeft));
 m_titleSelection=((row+NameRows)%NameRows)*NameColumns+(column+NameColumns)%NameColumns;

 const bool click=pressed(input.fire,m_titlePrevious.fire);
 int clicked=-1;
 if(click){
  const auto keys=titleItems();
  for(int i=0;i<int(keys.size());++i){
   const auto& key=keys[size_t(i)];
   if(input.pointerX>=key.x&&input.pointerX<key.x+key.w&&input.pointerY>=key.y&&input.pointerY<key.y+key.h){clicked=i;break;}
  }
  if(clicked>=0)m_titleSelection=clicked;
 }
 if(clicked<0&&!pressed(input.menuAccept,m_titlePrevious.menuAccept))return;

 const int key=m_titleSelection;
 if(key==NameDone){
  if(m_nameDraft.size()>=2){savePlayerName(m_nameDraft);close();}
 }else if(key==NameShift)m_nameUpper=!m_nameUpper;
 else if(key==NameDelete)editName(m_nameDraft,"\b");
 else if(key==NameSpace)editName(m_nameDraft," ");
 else{
  char c=NameLetters[size_t(key)];
  if(m_nameUpper)c=char(std::toupper(static_cast<unsigned char>(c)));
  editName(m_nameDraft,std::string_view(&c,1));
 }
}

void Game::updateMultiplayerMenu(const InputState& input){
 auto pressed=[&](bool now,bool previous){return now&&!previous;};
 // Game::update pumps the session (the Match layer) and drains its events into the notice; the menus read its state.
 if(m_joinCopied>0)m_joinCopied=std::max(0.f,m_joinCopied-1.f/60.f);
 const auto state=m_netSession?m_netSession->state():net::SessionState::Offline;
 if(state==net::SessionState::Failed&&m_titleNotice!=m_netSession->reason())m_titleNotice=m_netSession->reason();
 // In a lobby the page is the lobby's (its code to give out, LEAVE); out of one, the lobby page goes back.
 if(state==net::SessionState::InLobby&&m_titlePage!=TitlePage::Lobby){m_titleNotice.clear();openTitlePage(TitlePage::Lobby);}
 else if(m_titlePage==TitlePage::Lobby&&state!=net::SessionState::InLobby&&state!=net::SessionState::Busy)openTitlePage(TitlePage::Multiplayer);
 if(m_quickSearching&&state!=net::SessionState::Busy)m_quickSearching=false;
 if(m_titlePage==TitlePage::PlayerName){updatePlayerNameKeyboard(input);return;}
 auto items=titleItems();const int count=int(items.size());
 if(!count){openTitlePage(TitlePage::Multiplayer);return;}
 m_titleSelection=std::clamp(m_titleSelection,0,count-1);
 auto step=[&](int direction){for(int n=1;n<=count;++n){const int i=((m_titleSelection+direction*n)%count+count)%count;if(items[size_t(i)].enabled){m_titleSelection=i;return;}}};
 // Escape: the lobby page points at LEAVE (leaving is never a stray key); every other page goes back a step.
 if(pressed(input.escape,m_titlePrevious.escape)){
  if(m_titlePage==TitlePage::Lobby){for(int i=0;i<count;++i)if(items[size_t(i)].action==ActLeave)m_titleSelection=i;return;}
  if(m_titlePage==TitlePage::Multiplayer){m_titlePage=TitlePage::Main;m_titleSelection=TitleMultiplayer;return;}
  const TitlePage from=m_titlePage;openTitlePage(TitlePage::Multiplayer);
  m_titleSelection=from==TitlePage::QuickMatch?0:from==TitlePage::Browse?1:from==TitlePage::Host?2:from==TitlePage::JoinCode?3:5;return;}
 // The browser's list scrolls with the selection past its last shown row.
 if(m_titlePage==TitlePage::Browse&&m_netSession){
  int listed=0;for(const auto& l:m_netSession->results()){if(m_browseMode&&int(l.mode)!=m_browseMode)continue;if(l.players>=l.maxPlayers&&m_browseHideFull)continue;++listed;}
  const auto& at=items[size_t(m_titleSelection)];
  if(at.kind==TitleItem::Lobby){const int shown=(at.y-BrowseTop)/BrowseRow;
   if(pressed(input.menuDown,m_titlePrevious.menuDown)&&shown==BrowseRows-1&&m_browseScroll+BrowseRows<listed){++m_browseScroll;m_titlePrevious.menuDown=true;}
   if(pressed(input.menuUp,m_titlePrevious.menuUp)&&shown==0&&m_browseScroll>0){--m_browseScroll;m_titlePrevious.menuUp=true;}
   items=titleItems();}
 }
 const int countNow=int(items.size());m_titleSelection=std::clamp(m_titleSelection,0,countNow-1);
 if(pressed(input.menuUp,m_titlePrevious.menuUp))step(-1);
 if(pressed(input.menuDown,m_titlePrevious.menuDown))step(1);
 int direction=int(pressed(input.menuRight,m_titlePrevious.menuRight))-int(pressed(input.menuLeft,m_titlePrevious.menuLeft));
 // The browser's strip: left and right walk along it.
 if(direction&&items[size_t(m_titleSelection)].kind==TitleItem::Chip){const int to=m_titleSelection+direction;if(to>=0&&to<countNow&&items[size_t(to)].kind==TitleItem::Chip&&items[size_t(to)].enabled)m_titleSelection=to;direction=0;}
 // The pointer: hovering picks a usable row, a click on it uses it.
 int under=-1;for(int i=0;i<countNow;++i){const auto& t=items[size_t(i)];if(input.pointerX>=t.x-14&&input.pointerX<t.x+t.w&&input.pointerY>=t.y&&input.pointerY<t.y+t.h){under=i;break;}}
 const bool click=pressed(input.fire,m_titlePrevious.fire);
 if(under>=0&&items[size_t(under)].enabled&&(input.pointerX!=m_pointerX||input.pointerY!=m_pointerY||click))m_titleSelection=under;
 m_pointerX=input.pointerX;m_pointerY=input.pointerY;
 const TitleItem& item=items[size_t(m_titleSelection)];
 // Typing: a join code (its alphabet only, six characters), or the host's name while NAME is chosen.
 if(!input.textInput.empty()){
  if(m_titlePage==TitlePage::JoinCode){
   for(char c:input.textInput){if(c=='\b'){if(!m_joinTyped.empty())m_joinTyped.pop_back();continue;}
    c=char(std::toupper(static_cast<unsigned char>(c)));
    if(m_joinTyped.size()<6&&std::string_view("ABCDEFGHJKMNPQRSTUVWXYZ23456789").find(c)!=std::string_view::npos)m_joinTyped+=c;}}
  else if(m_titlePage==TitlePage::Multiplayer&&item.action==ActPlayerName){
   // The name others see: letters, digits, spaces and plain punctuation, 2-24 characters; saved at once.
   auto name=m_settings.playerName;editName(name,input.textInput);savePlayerName(std::move(name));}
  else if(m_titlePage==TitlePage::Host&&item.action==ActHostName){
   for(char c:input.textInput){if(c=='\b'){if(!m_hostDraft.name.empty())m_hostDraft.name.pop_back();continue;}
    if(c>=32&&c<127&&m_hostDraft.name.size()<24)m_hostDraft.name+=c;}}
 }
 const bool activate=(pressed(input.menuAccept,m_titlePrevious.menuAccept)||(click&&under==m_titleSelection))&&item.enabled;
 // Values that step: left and right, or a press, which steps forward.
 const int change=direction?direction:activate?1:0;
 switch(item.action){
  case ActHostMode:if(change){m_hostDraft.mode=m_hostDraft.mode==net::Mode::Coop?net::Mode::Deathmatch:net::Mode::Coop;
   const auto maps=hostableMaps(m_hostDraft.mode);if(std::find(maps.begin(),maps.end(),m_hostDraft.map)==maps.end())m_hostDraft.map=maps.empty()?-1:maps.front();}return;
  case ActHostMap:if(change){const auto maps=hostableMaps(m_hostDraft.mode);if(maps.empty())return;int at=0;for(size_t i=0;i<maps.size();++i)if(maps[i]==m_hostDraft.map)at=int(i);
   m_hostDraft.map=maps[size_t((at+change+int(maps.size()))%int(maps.size()))];}return;
  case ActHostPlayers:if(change){m_hostDraft.maxPlayers+=change;if(m_hostDraft.maxPlayers>16)m_hostDraft.maxPlayers=2;if(m_hostDraft.maxPlayers<2)m_hostDraft.maxPlayers=16;}return;
  case ActHostPrivate:if(change)m_hostDraft.privateGame=!m_hostDraft.privateGame;return;
  case ActMode:if(change){m_browseMode=(m_browseMode+(change>0?1:2))%3;browseRefresh();}return;
  case ActHideFull:if(change){m_browseHideFull=!m_browseHideFull;browseRefresh();}return;
  default:break;
 }
 if(!activate)return;
 m_titleNotice.clear();
 switch(item.action){
  case ActQuick:openTitlePage(TitlePage::QuickMatch);break;
  case ActBrowse:openTitlePage(TitlePage::Browse);break;
  case ActHost:openTitlePage(TitlePage::Host);break;
  case ActJoin:openTitlePage(TitlePage::JoinCode);break;
  case ActCharacter:openTitlePage(TitlePage::Character);break;
  case ActPlayerName:
   m_nameDraft=m_settings.playerName;
   m_nameUpper=false;
   openTitlePage(TitlePage::PlayerName);
   break;
  case ActBack:if(m_titlePage==TitlePage::Multiplayer){m_titlePage=TitlePage::Main;m_titleSelection=TitleMultiplayer;}
   else{const TitlePage from=m_titlePage;openTitlePage(TitlePage::Multiplayer);m_titleSelection=from==TitlePage::QuickMatch?0:from==TitlePage::Browse?1:from==TitlePage::Host?2:from==TitlePage::JoinCode?3:4;}break;
  case ActQuickDeathmatch:case ActQuickCoop:{const auto mode=item.action==ActQuickCoop?net::Mode::Coop:net::Mode::Deathmatch;
   m_quickMode=int(mode);m_quickSearching=true;m_netSession->quickMatch(mode);break;}
  case ActRefresh:browseRefresh();break;
  case ActLobby:if(item.lobby>=0&&item.lobby<int(m_netSession->results().size()))m_netSession->join(m_netSession->results()[size_t(item.lobby)].id);break;
  case ActHostGo:{const auto& campaign=m_customCampaigns[size_t(m_hostDraft.map)];net::HostOptions options;options.mode=m_hostDraft.mode;options.name=m_hostDraft.name;
   options.map=campaign->sourceFile.empty()?campaign->name:campaign->sourceFile;options.mapHash=mapHashOf(m_customMapDirectory,*campaign);
   options.maxPlayers=std::clamp(m_hostDraft.maxPlayers,2,16);options.privateGame=m_hostDraft.privateGame;m_netSession->host(options);break;}
  case ActCopy:if(m_netSession&&m_netSession->state()==net::SessionState::InLobby){copyTextToClipboard(m_netSession->lobby().joinCode);m_joinCopied=2.5f;}break;
  case ActLeave:if(m_netSession)m_netSession->leave();openTitlePage(TitlePage::Multiplayer);break;
  case ActJoinGo:{const auto code=net::normaliseJoinCode(m_joinTyped);if(!code.empty())m_netSession->joinCode(code);break;}
  case ActClaire:m_settings.character=0;m_settingsSaveDue=true;break;
  case ActLimitator:m_settings.character=1;m_settingsSaveDue=true;break;
  case ActLeon:m_settings.character=2;m_settingsSaveDue=true;break;
  default:break;
 }
}

// --multiplayer-menu-test (the multiplayer-ui lab tube runs it): the title's new row, every page reached and left by
// keys, a host over a loopback hub showing its code, another game's browser listing it and its join code joining it,
// the character kept through a settings save and load, and Offline greying out what needs a session.
bool Game::testMultiplayerMenu(std::ostream& report){
 bool ok=true;auto check=[&](bool pass,const std::string& what){report<<(pass?"PASS: ":"FAIL: ")<<what<<'\n';ok&=pass;};
 auto key=[](Game& g,auto field){InputState in{};in.*field=true;tickUpdate(g,in,1.f/60);tickUpdate(g,{},1.f/60);};
 auto campaign=std::make_shared<CustomCampaign>();campaign->name="Menu Arena";campaign->sourceFile="Menu Arena.rmc";campaign->key=4242;
 {auto map=std::make_shared<AuthoredMapData>();map->name="Arena";map->definition={{0,0},{6.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto& r:layer.rows)r=std::string(24,'.');map->layers.push_back(layer);campaign->maps.push_back(map);}
 auto coopOnly=std::make_shared<CustomCampaign>(*campaign);coopOnly->name="Co-op Only";coopOnly->key=4343;coopOnly->multiplayer.deathmatch=false;
 // Offline: no session. The page opens; what needs one is greyed and skipped.
 {Game g(campaign);g.showTitleScreen();for(int i=0;i<TitleMultiplayer;++i)key(g,&InputState::menuDown);
  check(g.titleSelection()==TitleMultiplayer,"MULTIPLAYER is the fourth row, between the campaign and SETTINGS");
  key(g,&InputState::menuAccept);check(g.titlePage()==TitlePage::Multiplayer,"accepting MULTIPLAYER opens its page");
  const auto items=g.titleItems();int usable=0;for(const auto& t:items)usable+=t.enabled;
  check(items.size()==7&&!items[0].enabled&&!items[3].enabled&&items[4].enabled&&items[4].label=="NAME"&&items[5].enabled&&items[6].enabled,"offline: QUICK MATCH to JOIN BY CODE greyed, NAME, CHARACTER and BACK usable");
  check(g.titleSelection()==4,"offline: the selection starts on the first usable row (CHARACTER)");
  check(g.sessionWord()=="OFFLINE"&&!g.sessionReason().empty(),"offline: the page says so and why");
  key(g,&InputState::escape);check(g.titlePage()==TitlePage::Main&&g.titleSelection()==TitleMultiplayer,"escape returns to the title on MULTIPLAYER");
  g.m_titleSelection=TitleQuit;key(g,&InputState::menuAccept);check(g.quitRequested(),"QUIT is still the last row and quits");}
 // Character: chosen on its page, kept through a settings file.
 {Game g(campaign);g.showTitleScreen();g.openTitlePage(TitlePage::Character);key(g,&InputState::menuDown);key(g,&InputState::menuAccept);
  check(g.settings().character==1,"CHARACTER: THE LIMITATOR chosen");
  const auto path=std::filesystem::temp_directory_path()/"rawmetal-character-test.txt";g.saveSettings(path.wstring());Game h(campaign);h.loadSettings(path.wstring());std::error_code e;std::filesystem::remove(path,e);
  check(h.settings().character==1,"the character survives a settings save and load");
  std::ofstream(path)<<"1 .75 1 1 0 1 1 1 1 .5 0 0 0 0\n";Game old(campaign);old.loadSettings(path.wstring());std::filesystem::remove(path,e);check(old.settings().character==0,"an older settings file keeps Claire");
  // Leon is the third row: chosen, kept through a save and load (an unknown number is clamped to the last body).
  Game l(campaign);l.showTitleScreen();l.openTitlePage(TitlePage::Character);key(l,&InputState::menuDown);key(l,&InputState::menuDown);key(l,&InputState::menuAccept);
  check(l.titleItems().size()==4&&l.titleItems()[2].label=="LEON"&&l.settings().character==2,"CHARACTER: three bodies and BACK, LEON chosen");
  l.saveSettings(path.wstring());Game m(campaign);m.loadSettings(path.wstring());check(m.settings().character==2,"Leon survives a settings save and load");
  std::ofstream(path)<<"1 .75 1 1 0 1 1 1 1 .5 0 0 0 0 9\n";Game wild(campaign);wild.loadSettings(path.wstring());std::filesystem::remove(path,e);check(wild.settings().character==2,"a character number past the last is clamped to Leon");}
 // MULTIPLAYER > NAME: typed on the page, used by the session at once, kept through a save and load; never the
 // Windows account name, which players did not want shown.
 {net::LoopbackHub names;auto session=names.connect("Diver-1234");Game g(campaign);g.setNetSession(session.get());g.showTitleScreen();g.openTitlePage(TitlePage::Multiplayer);
  check(g.titleItems().size()>4&&g.titleItems()[4].label=="NAME","MULTIPLAYER: a NAME row");
  g.m_titleSelection=4;{InputState typed{};typed.textInput="Nyx|Rider";tickUpdate(g,typed,1.f/60);tickUpdate(g,{},1.f/60);}
  check(g.settings().playerName=="NyxRider"&&session->displayName()=="NyxRider","NAME: typed (no | allowed) and used by the session ("+session->displayName()+")");
  {InputState typed{};typed.textInput="\b\b\b\b\bRider of Gloom";tickUpdate(g,typed,1.f/60);tickUpdate(g,{},1.f/60);}
  check(g.settings().playerName=="NyxRider of Gloom","NAME: backspace and spaces ("+g.settings().playerName+")");
  auto pressName=[&](bool InputState::*button){InputState input{};input.*button=true;tickUpdate(g,input,1.f/60);tickUpdate(g,{},1.f/60);};
  pressName(&InputState::menuAccept);
  check(g.titlePage()==TitlePage::PlayerName,"NAME: A opens the on-screen keyboard");
  pressName(&InputState::menuAccept);
  pressName(&InputState::escape);
  check(g.settings().playerName=="NyxRider of Gloom","NAME: B cancels changes");
  pressName(&InputState::menuAccept);
  pressName(&InputState::menuAccept);
  pressName(&InputState::menuUp);
  for(int step=0;step<3;++step)pressName(&InputState::menuLeft);
  pressName(&InputState::menuAccept); // Shift
  pressName(&InputState::menuRight);
  pressName(&InputState::menuAccept); // Delete the digit
  pressName(&InputState::menuDown);
  pressName(&InputState::menuDown);
  pressName(&InputState::menuRight);
  pressName(&InputState::menuAccept); // J
  pressName(&InputState::menuUp);
  pressName(&InputState::menuUp);
  pressName(&InputState::menuAccept); // Done
  check(g.titlePage()==TitlePage::Multiplayer&&g.settings().playerName=="NyxRider of GloomJ"&&session->displayName()==g.settings().playerName,"NAME: keyboard keys and Done update the session");
  g.savePlayerName("NyxRider of Gloom");
  tickUpdate(g,{},1.f/60);
  const auto path=std::filesystem::temp_directory_path()/"rawmetal-name-test.txt";g.saveSettings(path.wstring());Game h(campaign);h.loadSettings(path.wstring());std::error_code e;std::filesystem::remove(path,e);
  auto later=names.connect("Diver-5678");h.setNetSession(later.get());
  check(h.settings().playerName=="NyxRider of Gloom"&&later->displayName()=="NyxRider of Gloom","the name survives a save and load and is handed to the next session");}
 // Online over a loopback hub: a host, and a guest who finds it.
 net::LoopbackHub hub;auto hostSession=hub.connect("Maddie");auto guestSession=hub.connect("Ash");
 Game host(campaign);host.m_customCampaigns={campaign,coopOnly};host.setNetSession(hostSession.get());host.showTitleScreen();
 Game guest(campaign);guest.m_customCampaigns={campaign};guest.setNetSession(guestSession.get());guest.showTitleScreen();
 host.openTitlePage(TitlePage::Multiplayer);guest.openTitlePage(TitlePage::Multiplayer);
 tickUpdate(host,{},1.f/60);tickUpdate(guest,{},1.f/60);
 check(host.sessionUsable()&&host.sessionWord()=="SIGNED IN","signed in on the hub after a tick");
 host.m_titleSelection=2;key(host,&InputState::menuAccept);check(host.titlePage()==TitlePage::Host,"HOST A GAME opens the host page");
 check(host.hostableMaps(net::Mode::Deathmatch).size()==1&&host.hostableMaps(net::Mode::Coop).size()==2,"maps offered by what each campaign's MULTIPLAYER record allows");
 // Rows: MODE, MAP, PLAYERS, PRIVATE, NAME, HOST GAME, BACK. Players stepped to 6 by the left key.
 host.m_titleSelection=2;for(int i=0;i<2;++i)key(host,&InputState::menuLeft);check(host.hostDraft().maxPlayers==6,"PLAYERS steps with left and right (8 to 6)");
 host.m_titleSelection=4;{InputState typed{};typed.textInput="\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bThe Pit";tickUpdate(host,typed,1.f/60);tickUpdate(host,{},1.f/60);}
 check(host.hostDraft().name=="The Pit","the game's name is typed into NAME");
 host.m_titleSelection=5;key(host,&InputState::menuAccept);tickUpdate(host,{},1.f/60);
 for(int i=0;i<10;++i)tickUpdate(host,{},1.f/60);
 check(hostSession->state()==net::SessionState::InLobby&&!host.titleScreen()&&host.inMatch(),"HOST GAME opens a lobby and takes the host, who has the map, straight down into it");
 const std::string code=hostSession->lobby().joinCode;
 check(!code.empty()&&hostSession->lobby().maxPlayers==6&&hostSession->lobby().name=="The Pit"&&hostSession->lobby().map=="Menu Arena.rmc"&&hostSession->lobby().mapHash.size()==16,"the lobby has a code, six places, the name, the map file and its hash");
 // The guest's browser lists it; then a typed code joins it.
 guest.m_titleSelection=1;key(guest,&InputState::menuAccept);tickUpdate(guest,{},1.f/60);
 {const auto items=guest.titleItems();int lobbies=0;for(const auto& t:items)lobbies+=t.kind==TitleItem::Lobby;
  check(guest.titlePage()==TitlePage::Browse&&lobbies==1,"BROWSE lists the host's game ("+std::to_string(lobbies)+" row)");}
 guest.m_browseMode=2;guest.browseRefresh();tickUpdate(guest,{},1.f/60);
 {int lobbies=0;for(const auto& t:guest.titleItems())lobbies+=t.kind==TitleItem::Lobby;check(lobbies==0,"the mode filter hides a deathmatch when CO-OP is asked for");}
 key(guest,&InputState::escape);check(guest.titlePage()==TitlePage::Multiplayer&&guest.titleSelection()==1,"escape from BROWSE returns to MULTIPLAYER on BROWSE GAMES");
 guest.m_titleSelection=3;key(guest,&InputState::menuAccept);
 {InputState typed{};typed.textInput=code;for(auto& c:typed.textInput)c=char(std::tolower(static_cast<unsigned char>(c)));tickUpdate(guest,typed,1.f/60);tickUpdate(guest,{},1.f/60);}
 check(net::normaliseJoinCode(guest.joinCodeTyped())==code,"JOIN BY CODE: the typed code reads as the host's ("+guest.joinCodeTyped()+")");
 guest.m_titleSelection=1;key(guest,&InputState::menuAccept);tickUpdate(guest,{},1.f/60);
 check(guestSession->state()==net::SessionState::InLobby&&guestSession->lobby().id==hostSession->lobby().id&&guest.titlePage()==TitlePage::Lobby,"JOIN joins the host's lobby and waits on the lobby page");
 // LEAVE on the lobby page, while the welcome is still on its way, leaves.
 {const auto items=guest.titleItems();for(int i=0;i<int(items.size());++i)if(items[size_t(i)].action==ActLeave)guest.m_titleSelection=i;}
 key(guest,&InputState::menuAccept);check(guestSession->state()==net::SessionState::Ready&&guest.titlePage()==TitlePage::Multiplayer,"LEAVE leaves the lobby and returns to MULTIPLAYER");
 // Joining again, the guest (who has the map) is welcomed and goes down into the host's game.
 guestSession->joinCode(code);for(int i=0;i<40&&guest.titleScreen();++i){tickUpdate(host,{},1.f/60);tickUpdate(guest,{},1.f/60);}
 check(!guest.titleScreen()&&guest.inMatch(),"a guest who has the map is welcomed and plays");
 guest.leaveMatch();for(int i=0;i<3;++i)tickUpdate(guest,{},1.f/60);
 check(guestSession->state()==net::SessionState::Ready&&!guest.inMatch(),"LEAVE MATCH takes the guest out of the game");
 // In a match the pause menu has no saving: its third row is the join code, its last leaves.
 {host.m_titleScreen=false;host.m_matchView.active=true;host.m_paused=true;host.m_menuPage=MenuPage::Pause;host.m_menuFromTitle=false;host.m_menuSelection=PauseMatchCode;
  key(host,&InputState::menuAccept);check(host.paused()&&host.menuPage()==MenuPage::Pause&&host.menuRows()==4,"in a match PAUSE has no SAVE GAME (its third row is the join code)");
  host.m_menuSelection=PauseMatchLeave;key(host,&InputState::menuAccept);
  check(host.titleScreen()&&host.titlePage()==TitlePage::Multiplayer&&hostSession->state()==net::SessionState::Ready&&!host.inMatch(),"LEAVE MATCH leaves the lobby and shows MULTIPLAYER");}
 return ok;
}
}

// The join code to the clipboard (Win32), for COPY CODE.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
bool retro::copyTextToClipboard(const std::string& text){
 if(!OpenClipboard(nullptr))return false;EmptyClipboard();
 HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,text.size()+1);bool done=false;
 if(memory){if(void* p=GlobalLock(memory)){std::memcpy(p,text.c_str(),text.size()+1);GlobalUnlock(memory);done=SetClipboardData(CF_TEXT,memory)!=nullptr;}if(!done)GlobalFree(memory);}
 CloseClipboard();return done;
}
#else
bool retro::copyTextToClipboard(const std::string&){return false;}
#endif

#include "SoftwareRenderer.h"
#include "../net/NetTypes.h"
#include "../net/Match.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
// The front door and a match's HUD, in the game's own type and colours: bone on near-black, one red for what is
// chosen or wrong, amber for what is under way. The title (main menu, the custom map archive, the multiplayer pages
// built by Game::titleItems) and, in a match, the scoreboard, kill feed, status line and respawn count.
namespace retro {
namespace {
std::uint32_t rgb(int r,int g,int b){return 0xff000000u|(std::uint32_t(r)<<16)|(std::uint32_t(g)<<8)|std::uint32_t(b);}
// Words wrapped at spaces to `columns` characters, at most `limit` lines (the last cut with "...").
std::vector<std::string> wrapUiText(std::string remaining,size_t columns,size_t limit){
 std::vector<std::string> lines;columns=std::max(size_t(1),columns);
 while(!remaining.empty()&&lines.size()<limit){size_t count=std::min(columns,remaining.size());
  if(count<remaining.size()){auto space=remaining.rfind(' ',count);if(space!=std::string::npos&&space>0)count=space;}
  lines.push_back(remaining.substr(0,count));remaining.erase(0,count);while(!remaining.empty()&&remaining.front()==' ')remaining.erase(0,1);}
 if(!remaining.empty()&&!lines.empty()&&columns>=3){auto& last=lines.back();last.resize(std::min(last.size(),columns-3));last+="...";}
 return lines;
}
const char* weaponWord(int weapon){return weapon==1?"SHOTGUN":weapon==2?"9MM":"HANDS";}
std::string upper(std::string s){for(auto& c:s)c=char(std::toupper(static_cast<unsigned char>(c)));return s;}
}

void SoftwareRenderer::drawTitle(const Game& game){
 static unsigned tick=0;++tick;const bool blink=(tick/30)%2==0;
 if(!hardwarePresentsWindow())for(auto&p:m_pixels)p=shade(p,.17f);
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0),amber=rgb(232,160,60);
 const auto page=game.customMapsOpen()?Game::TitlePage::Main:game.titlePage();
 const bool main=page==Game::TitlePage::Main&&!game.customMapsOpen();
 // Smoke the left of the frame so the menu reads over the dimmed scene; a heavier veil on the wide pages.
 gradient(0,0,m_width*3/4,m_height,black,.94f,0.f,true);
 if(page==Game::TitlePage::Browse||page==Game::TitlePage::Lobby)fill(0,0,m_width,m_height,black,.45f);
 gradient(0,0,m_width,64,black,.75f,0.f,false);gradient(0,m_height-72,m_width,72,black,0.f,.9f,false);
 for(int y=0;y<m_height;y+=2)fill(0,y,m_width,1,black,.16f);
 for(int i=0;i<700;++i){unsigned h=unsigned(i)*2654435761u^tick*40503u;h^=h>>15;h*=2246822519u;h^=h>>13;blend(int(h%unsigned(m_width)),int((h>>10)%unsigned(m_height)),(h>>5)&1?bone:black,.09f);}
 // Header strip.
 fill(40,21,5,5,red,1);uiText(52,16,"DEPTHWORKS INDUSTRIAL  //  SITE 9 EXTRACTION COMPLEX",dim,0,11,1,1);
 const char* clock="SHIFT 03 / 03:14 AM";uiText(m_width-40-uiTextWidth(clock,3,11),16,clock,dim,3,11);
 fill(40,33,m_width-80,1,bone,.10f);
 // Wordmark: a red plate misregistered under bone type, torn every few seconds.
 const char* mark="DEPTHWORKS";const int markX=36,markY=38;
 uiText(markX+3,markY+2,mark,red,2,62,.85f,2);uiText(markX,markY,mark,bone,2,62,1,2);
 if(tick>60&&(tick/7)%23==0){unsigned h=tick*747796405u;int band=markY+10+int(h%42),shift=6+int((h>>8)%14);
  for(int y=band;y<std::min(m_height,band+4+int((h>>4)%6));++y){auto row=m_pixels.begin()+size_t(y)*m_width;std::rotate(row,row+m_width-shift,row+m_width);}}
 // The page's name in red and a line under it in the game's voice.
 struct Heading{const char* kicker;const char* line;};
 Heading heading{"CONTAINMENT FAILURE","Something broke loose below the foundry. Nobody is coming."};
 if(game.customMapsOpen())heading={"CUSTOM MAP ARCHIVE","Drop .rmc campaign files in /custom maps"};
 else switch(page){
  case Game::TitlePage::Multiplayer:heading={"MULTIPLAYER","Others came down after you. Not all of them came to help."};break;
  case Game::TitlePage::QuickMatch:heading={"QUICK MATCH","The fullest open game of its kind, or one of your own."};break;
  case Game::TitlePage::Browse:heading={"OPEN GAMES","Lamps in the dark. Each one is somebody's shift."};break;
  case Game::TitlePage::Host:heading={"HOST A GAME","Your map, your rules. They come down on your word."};break;
  case Game::TitlePage::Lobby:heading={"IN THE LOBBY","Give them the code. Then wait for footsteps."};break;
  case Game::TitlePage::JoinCode:heading={"JOIN BY CODE","Six characters, given in confidence."};break;
  case Game::TitlePage::Character:heading={"CHARACTER","Who the others see coming down the stair."};break;
  default:break;
 }
 fill(40,112,4,16,red,1);uiText(52,111,heading.kicker,red,0,15,1,3);
 uiText(52,130,heading.line,dim,1,13);
 if(game.customMapsOpen()){
  for(int row=0;row<game.titleRows();++row){
   const int y=TitleMenuLayout::Y+row*TitleMenuLayout::RowHeight;const bool selected=row==game.titleSelection();
   const std::string label=game.customMenuLabel(row);
   if(selected){gradient(TitleMenuLayout::X-14,y,TitleMenuLayout::Width+14,TitleMenuLayout::RowHeight-2,red,.55f,0.f,true);fill(TitleMenuLayout::X-14,y,3,TitleMenuLayout::RowHeight-2,red,1);}
   char index[4];std::snprintf(index,sizeof index,"%02d",row+1);
   uiText(TitleMenuLayout::X,y+6,index,selected?bone:dim,3,10,selected?1.f:.55f);
   uiText(TitleMenuLayout::X+24+(selected?3:0),y+1,label.c_str(),selected?bone:dim,0,17,selected?1.f:.8f,1);
   if(selected)uiText(TitleMenuLayout::X+TitleMenuLayout::Width-16,y+1,">",bone,0,17,.8f);}
 }else if(main){
  // Three groups: the campaign, MULTIPLAYER, then settings and quit, smaller. The chosen row is lit red from its
  // left edge, nudged right, and named again at the foot of the menu.
  static const char* labels[]={"NEW GAME","CUSTOM MAPS","LOAD GAME","MULTIPLAYER","SETTINGS","QUIT"};
  static const char* notes[]={"The night shift. The Security Office. The lift is already gone.","Descents other hands have dug. Drop .rmc files in /custom maps.",
   "Pick up where the dark last left you.","Others are down here too. Deathmatch or co-op, hosted by players.","Sound, sight and hands.","Go back up while you still can."};
  for(int row=0;row<TitleMenuLayout::Rows;++row){
   const int y=TitleMenuLayout::rowY(row);const bool selected=row==game.titleSelection(),big=row<=TitleMultiplayer;
   if(row==TitleMultiplayer||row==TitleSettings)fill(TitleMenuLayout::X,y-TitleMenuLayout::Gap/2-1,TitleMenuLayout::Width-24,1,bone,.08f);
   if(selected){gradient(TitleMenuLayout::X-14,y,TitleMenuLayout::Width+14,TitleMenuLayout::RowHeight-2,red,.55f,0.f,true);fill(TitleMenuLayout::X-14,y,3,TitleMenuLayout::RowHeight-2,red,1);}
   char index[4];std::snprintf(index,sizeof index,"%02d",row+1);
   uiText(TitleMenuLayout::X,y+(big?6:5),index,selected?bone:dim,3,10,selected?1.f:.5f);
   uiText(TitleMenuLayout::X+24+(selected?3:0),y+(big?1:3),labels[row],selected?bone:dim,0,big?18:14,selected?1.f:big?.85f:.7f,1);
   if(row==TitleMultiplayer){const std::string word=game.sessionWord();const bool ok=game.sessionUsable();
    uiText(TitleMenuLayout::X+TitleMenuLayout::Width-26-uiTextWidth(word.c_str(),3,9,1),y+7,word.c_str(),ok?dim:word=="OFFLINE"?red:amber,3,9,selected?1.f:.7f,1);}
   if(selected)uiText(TitleMenuLayout::X+TitleMenuLayout::Width-14,y+(big?1:2),">",bone,0,big?18:14,.8f);}
  const int noteY=TitleMenuLayout::rowY(TitleMenuLayout::Rows-1)+TitleMenuLayout::RowHeight+6;
  const int pick=std::clamp(game.titleSelection(),0,TitleMenuLayout::Rows-1);
  fill(TitleMenuLayout::X,noteY+7,10,1,red,1);uiText(TitleMenuLayout::X+16,noteY,notes[pick],dim,1,12);
 }else drawTitleItems(game);
 // The right of the frame: the facility on the main menu, the link on the multiplayer pages that leave room for it.
 if(main||game.customMapsOpen()){
  const int sx=m_width-218,sy=150;
  uiText(sx,sy,"FACILITY STATUS",dim,0,11,1,2);fill(sx,sy+16,178,1,bone,.14f);
  const char* keys[]={"MAIN POWER","SERVICE LIFT","CONTAINMENT","PERSONNEL"};const char* values[]={"DEGRADED","EMERGENCY","FAILED","UNACCOUNTED"};
  for(int i=0;i<4;++i){uiText(sx,sy+24+i*17,keys[i],dim,3,11,.85f);uiText(sx+178-uiTextWidth(values[i],3,11),sy+24+i*17,values[i],i<2?amber:red,3,11);}
  if(blink)fill(sx,sy+101,6,6,red,1);uiText(sx+12,sy+96,"ALARM ACTIVE / ALL LEVELS",red,0,11,blink?1.f:.55f,1);
 }else if(page==Game::TitlePage::Multiplayer||page==Game::TitlePage::QuickMatch||page==Game::TitlePage::JoinCode){
  const int sx=m_width-226,sy=150,w=186;const std::string word=game.sessionWord();const bool ok=game.sessionUsable();
  uiText(sx,sy,"THE LINK",dim,0,11,1,2);fill(sx,sy+16,w,1,bone,.14f);
  const auto tone=ok||word=="IN A LOBBY"?bone:word=="OFFLINE"?red:amber;
  if(!ok&&word!="OFFLINE"&&blink)fill(sx,sy+29,5,5,amber,1);
  uiText(sx+(ok||word=="OFFLINE"?0:10),sy+24,word.c_str(),tone,0,15,1,2);
  int y=sy+46;for(const auto& line:wrapUiText(game.sessionReason(),size_t(34),size_t(4))){uiText(sx,y,line.c_str(),dim,1,12);y+=15;}
  y=std::max(y+8,sy+112);fill(sx,y,w,1,bone,.08f);
  uiText(sx,y+7,"YOU ARE",dim,3,9,1,2);const char* who=game.settings().character==1?"THE LIMITATOR":"CLAIRE";uiText(sx+w-uiTextWidth(who,0,12,1),y+5,who,bone,0,12,1,1);
 }
 // Footer: what the keys and a controller do, and the build.
 fill(40,m_height-36,m_width-80,1,bone,.10f);
 int kx=40;auto cap=[&](const char*key,const char*what){int w=uiTextWidth(key,3,10)+10;frame(kx,m_height-27,w,15,bone,.35f);uiText(kx+5,m_height-26,key,bone,3,10);kx+=w+6;kx+=uiText(kx,m_height-27,what,dim,0,11,1,1)+14;};
 cap("ENTER / A","SELECT");cap("ESC / B","BACK");cap("ARROWS / D-PAD","MOVE");
 if(page==Game::TitlePage::Host||page==Game::TitlePage::Browse)cap("< >","CHANGE");
 const char* build="V1.6.0  CHAPTER 1";uiText(m_width-40-uiTextWidth(build,0,11,2),m_height-27,build,red,0,11,1,2);
}

// The multiplayer pages: Game::titleItems' rows where the game hit-tests them, and around them what each page shows.
void SoftwareRenderer::drawTitleItems(const Game& game){
 static unsigned tick=0;++tick;const bool caret=(tick/24)%2==0;
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0),amber=rgb(232,160,60),ink=rgb(10,8,8);
 const auto page=game.titlePage();const auto items=game.titleItems();const int selected=game.titleSelection();
 const net::Session* session=game.netSession();
 int number=0;
 for(size_t i=0;i<items.size();++i){const auto& t=items[i];const bool on=int(i)==selected&&t.enabled;const float a=t.enabled?1.f:.4f;
  switch(t.kind){
   case TitleItem::Row:case TitleItem::Back:{
    if(t.kind==TitleItem::Back||(page==Game::TitlePage::Multiplayer&&t.label=="CHARACTER"))fill(t.x,t.y-TitleMenuLayout::Gap/2-1,t.w-24,1,bone,.08f);
    if(on){gradient(t.x-14,t.y,t.w+14,t.h-2,red,.55f,0.f,true);fill(t.x-14,t.y,3,t.h-2,red,1);}
    const bool back=t.kind==TitleItem::Back;++number;
    char index[4];std::snprintf(index,sizeof index,"%02d",number);
    uiText(t.x,t.y+(back?5:6),back?"<":index,on?bone:dim,3,10,(on?1.f:.5f)*a);
    uiText(t.x+24+(on?3:0),t.y+(back?3:1),t.label.c_str(),on?bone:dim,0,back?14:17,(on?1.f:.85f)*a,1);
    if(!t.value.empty()){const bool live=t.value=="SEARCHING"||t.value=="COPIED";const int w=uiTextWidth(t.value.c_str(),3,11);
     uiText(t.x+t.w-22-w,t.y+5,t.value.c_str(),live?amber:on?bone:dim,3,11,a*(live&&!caret?.6f:1.f));
     if(page==Game::TitlePage::Host&&on&&t.enabled&&t.label!="HOST GAME"){uiText(t.x+t.w-34-w,t.y+5,"<",bone,3,11,.7f);uiText(t.x+t.w-14,t.y+5,">",bone,3,11,.7f);}}
    break;}
   case TitleItem::Chip:{
    if(on){fill(t.x,t.y,t.w,t.h,red,.6f);}else fill(t.x,t.y,t.w,t.h,ink,.6f);
    frame(t.x,t.y,t.w,t.h,on?bone:dim,on?.6f:.3f);
    std::string text=t.label;if(!t.value.empty())text+=": "+t.value;
    uiText(t.x+(t.w-uiTextWidth(text.c_str(),0,11,1))/2,t.y+3,text.c_str(),on?bone:dim,0,11,a,1);break;}
   case TitleItem::Lobby:{
    if(!session||t.lobby<0||t.lobby>=int(session->results().size()))break;const auto& l=session->results()[size_t(t.lobby)];
    if(((t.y-190)/18)%2)fill(t.x,t.y,t.w,t.h,bone,.03f);
    if(on){gradient(t.x-14,t.y,t.w+14,t.h,red,.55f,.05f,true);fill(t.x-14,t.y,3,t.h,red,1);}
    std::string name=l.name;while(name.size()>3&&uiTextWidth(name.c_str(),0,12)>212)name=name.substr(0,name.size()-4)+"...";
    std::string map=l.map;if(map.size()>4&&map.ends_with(".rmc"))map.resize(map.size()-4);while(map.size()>3&&uiTextWidth(map.c_str(),1,12)>100)map=map.substr(0,map.size()-4)+"...";
    const auto tone=on?bone:dim;
    uiText(t.x+6,t.y+2,name.c_str(),on?bone:rgb(206,198,186),0,12,a);
    uiText(t.x+222,t.y+3,net::modeName(l.mode),tone,3,10,a,1);
    uiText(t.x+300,t.y+2,map.c_str(),tone,1,12,a);
    char players[16];std::snprintf(players,sizeof players,"%d / %d",l.players,l.maxPlayers);uiText(t.x+410,t.y+3,players,l.players>=l.maxPlayers?red:tone,3,10,a);
    char ping[16];if(l.pingMs<0)std::snprintf(ping,sizeof ping,"--");else std::snprintf(ping,sizeof ping,"%d",l.pingMs);
    uiText(t.x+t.w-8-uiTextWidth(ping,3,10),t.y+3,ping,l.pingMs<0?dim:l.pingMs<90?tone:l.pingMs<160?amber:red,3,10,a);break;}
   case TitleItem::Field:{
    if(page==Game::TitlePage::JoinCode){
     // Six cells, the dash after the fourth: what is typed fills them, the caret waits in the next.
     const std::string& typed=game.joinCodeTyped();const int cell=28,gap=4;int x=t.x;
     for(int c=0;c<6;++c){if(c==4){uiText(x+2,t.y+7,"-",dim,0,18);x+=14;}
      const bool here=c==int(typed.size());fill(x,t.y,cell,t.h,ink,.75f);frame(x,t.y,cell,t.h,on&&here?red:bone,on&&here?.9f:.22f);
      if(c<int(typed.size())){const char g[2]={typed[size_t(c)],0};uiText(x+(cell-uiTextWidth(g,0,22))/2,t.y+5,g,bone,0,22);}
      else if(here&&on&&caret)fill(x+cell/2-5,t.y+t.h-8,10,2,red,1);
      x+=cell+gap;}
     break;}
    // A single line: its label, then the text with a caret while chosen.
    if(on){gradient(t.x-14,t.y,t.w+14,t.h-2,red,.4f,0.f,true);fill(t.x-14,t.y,3,t.h-2,red,1);}
    ++number;char index[4];std::snprintf(index,sizeof index,"%02d",number);uiText(t.x,t.y+6,index,on?bone:dim,3,10,on?1.f:.5f);
    uiText(t.x+24+(on?3:0),t.y+1,t.label.c_str(),on?bone:dim,0,17,on?1.f:.85f,1);
    const int fx=t.x+110,fw=t.w-122;fill(fx,t.y+2,fw,t.h-6,ink,.7f);frame(fx,t.y+2,fw,t.h-6,on?bone:dim,on?.5f:.2f);
    std::string text=t.value;while(text.size()>1&&uiTextWidth(text.c_str(),1,13)>fw-14)text.erase(0,1);
    const int w=uiText(fx+5,t.y+3,text.c_str(),bone,1,13);if(on&&caret)fill(fx+6+w,t.y+5,1,12,red,1);
    break;}
  }
 }
 // What each page shows beside its rows.
 const int X=TitleMenuLayout::X;
 if(page==Game::TitlePage::Browse){
  int listed=0,lobbies=0;for(const auto& t:items)lobbies+=t.kind==TitleItem::Lobby;
  if(session)for(const auto& l:session->results()){if(game.browseMode()&&int(l.mode)!=game.browseMode())continue;if(l.players>=l.maxPlayers&&game.browseHideFull())continue;++listed;}
  const int right=DisplayWidth-X;
  char count[32];std::snprintf(count,sizeof count,listed==1?"%d GAME":"%d GAMES",listed);uiText(right-uiTextWidth(count,3,10,1),153,count,dim,3,10,1,1);
  const int hy=176;uiText(X+6,hy,"NAME",dim,3,9,1,2);uiText(X+222,hy,"MODE",dim,3,9,1,2);uiText(X+300,hy,"MAP",dim,3,9,1,2);uiText(X+410,hy,"PLAYERS",dim,3,9,1,2);uiText(right-8-uiTextWidth("PING",3,9,2),hy,"PING",dim,3,9,1,2);
  fill(X,hy+11,right-X,1,bone,.14f);
  const bool busy=session&&session->state()==net::SessionState::Busy;
  if(!lobbies){const char* empty=!game.sessionUsable()&&!busy?"No link to the others. See why on the MULTIPLAYER page.":busy?"Listening for lamps below...":"No lamps lit. Refresh, or host one yourself.";
   uiText(X+(right-X-uiTextWidth(empty,1,13))/2,232,empty,busy?amber:dim,1,13);}
  if(game.browseScroll()>0)uiText(right-10,190,"^",dim,0,12);
  if(game.browseScroll()+6<listed)uiText(right-10,282,"v",dim,0,12);
 }else if(page==Game::TitlePage::Lobby&&session&&session->state()==net::SessionState::InLobby){
  const auto& l=session->lobby();const int y=TitleMenuLayout::Y;
  std::string name=l.name;while(name.size()>3&&uiTextWidth(name.c_str(),0,18,1)>250)name=name.substr(0,name.size()-4)+"...";
  uiText(X,y,name.c_str(),bone,0,18,1,1);
  std::string map=l.map;if(map.size()>4&&map.ends_with(".rmc"))map.resize(map.size()-4);
  const std::string line=std::string(net::modeName(l.mode))+"  /  "+upper(map);uiText(X,y+24,line.c_str(),dim,3,10,1,1);
  char players[48];std::snprintf(players,sizeof players,"%d OF %d DOWN HERE",l.players,l.maxPlayers);uiText(X,y+40,players,dim,3,10,1,1);
  const char* role=session->isHost()?"YOU HOLD THE LAMP":"WAITING ON THE HOST";uiText(X,y+56,role,session->isHost()?amber:dim,3,10,1,1);
  // The code, big, on the right: the wordmark's misregistered plate under it.
  const int px=m_width-292,py=146,pw=252,ph=126;fill(px,py,pw,ph,ink,.82f);frame(px,py,pw,ph,bone,.12f);fill(px,py,pw,2,red,1);
  uiText(px+14,py+12,"JOIN CODE",red,0,11,1,3);
  const char* privacy=l.privateGame?"PRIVATE  /  BY CODE ONLY":"LISTED  /  ANYONE MAY COME";uiText(px+pw-14-uiTextWidth(privacy,3,9,1),py+14,privacy,dim,3,9,1,1);
  const std::string code=l.joinCode.empty()?"------":l.joinCode;const int cw=uiTextWidth(code.c_str(),2,44,4),cx=px+(pw-cw)/2;
  uiText(cx+3,py+36,code.c_str(),red,2,44,.8f,4);uiText(cx,py+34,code.c_str(),bone,2,44,1,4);
  const float copied=game.joinCodeCopied();
  if(copied>0)uiText(px+(pw-uiTextWidth("COPIED TO THE CLIPBOARD",0,11,2))/2,py+96,"COPIED TO THE CLIPBOARD",amber,0,11,std::min(1.f,copied),2);
  else uiText(px+(pw-uiTextWidth("Give it only to those you trust.",1,12))/2,py+96,"Give it only to those you trust.",dim,1,12);
 }else if(page==Game::TitlePage::Host){
  const auto& d=game.hostDraft();const int px=m_width-246,py=150,pw=206;
  uiText(px,py,"THE GAME",dim,0,11,1,2);fill(px,py+16,pw,1,bone,.14f);
  std::string name=d.name.empty()?"(unnamed)":d.name;while(name.size()>3&&uiTextWidth(name.c_str(),0,15)>pw)name=name.substr(0,name.size()-4)+"...";
  uiText(px,py+24,name.c_str(),bone,0,15);
  std::string map=game.customCampaignName(d.map);if(map.empty())map="NO MAP";while(map.size()>3&&uiTextWidth(map.c_str(),1,13)>pw)map=map.substr(0,map.size()-4)+"...";
  uiText(px,py+44,map.c_str(),dim,1,13);
  auto line=[&](int y,const char* key,const std::string& value){uiText(px,y,key,dim,3,10,.9f,1);uiText(px+pw-uiTextWidth(value.c_str(),3,10),y,value.c_str(),bone,3,10);};
  line(py+66,"MODE",net::modeName(d.mode));line(py+82,"PLACES",std::to_string(d.maxPlayers));line(py+98,"LISTED",d.privateGame?"NO":"YES");
  if(const auto* rules=game.campaignMultiplayer(d.map)){
   if(d.mode==net::Mode::Deathmatch){line(py+114,"FRAG LIMIT",rules->fragLimit>0?std::to_string(rules->fragLimit):"NONE");
    line(py+130,"TIME LIMIT",rules->timeLimitMinutes>0?std::to_string(int(rules->timeLimitMinutes+.5f))+" MIN":"NONE");}
   else line(py+114,"FRIENDLY FIRE",rules->friendlyFire?"ON":"OFF");}
  if(game.hostableMaps(d.mode).empty()){int y=py+150;for(const auto& l:wrapUiText("No loaded campaign allows this mode. Its MULTIPLAYER record is set in the Campaign Builder.",size_t(36),size_t(3))){uiText(px,y,l.c_str(),amber,1,12);y+=15;}}
 }else if(page==Game::TitlePage::Character){
  const int c=std::clamp(game.titleSelection(),0,1);const int px=m_width-266,py=150,pw=226;
  uiText(px,py,c==1?"THE LIMITATOR":"CLAIRE",bone,0,18,1,2);fill(px,py+24,pw,1,bone,.14f);
  const char* about=c==1?"A heavier tread on the stair. He came down to set limits on what climbs back up.":"Security, unauthorised. Marked for elimination by the people who signed her shift. She knows the vents.";
  int y=py+32;for(const auto& l:wrapUiText(about,size_t(38),size_t(4))){uiText(px,y,l.c_str(),dim,1,13);y+=16;}
  if(c==1&&!characterBuilt(1)){y+=6;for(const auto& l:wrapUiText("His body is still being made: until it is in this build, the others see Claire.",size_t(40),size_t(3))){uiText(px,y,l.c_str(),amber,1,12);y+=15;}}
 }
 // A failed request, in plain words, under the rows.
 if(const std::string notice=game.titleNotice();!notice.empty()&&page!=Game::TitlePage::Browse){
  int y=0;for(const auto& t:items)y=std::max(y,t.y+t.h);y+=8;
  fill(X,y+6,10,1,red,1);int ly=y;for(const auto& l:wrapUiText(notice,size_t(46),size_t(2))){uiText(X+16,ly,l.c_str(),red,1,12);ly+=14;}}
 (void)black;
}

// A match on screen: the clock and limit at the top, the kill feed under the map at the top right, the status line,
// the respawn count, and while Tab (or back/view) is held, or the match is over, the scoreboard.
void SoftwareRenderer::drawMatchHud(const Game& game){
 const auto& m=game.matchView();if(!m.active)return;
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0),amber=rgb(232,160,60),ink=rgb(6,5,5);
 const int cx=m_width/2;
 // The clock: mode, time left, the limit.
 {char t[16]="--:--";if(m.timeLeft>=0){const int s=int(std::ceil(m.timeLeft));std::snprintf(t,sizeof t,"%d:%02d",s/60,s%60);}
  std::string line=net::modeName(m.mode);if(m.fragLimit>0&&m.mode==net::Mode::Deathmatch)line+="  /  FIRST TO "+std::to_string(m.fragLimit);
  const int tw=uiTextWidth(t,3,15,1),lw=uiTextWidth(line.c_str(),3,8,2),w=std::max(tw,lw);
  gradient(cx-w/2-26,6,w/2+26,32,black,0.f,.5f,true);gradient(cx,6,w/2+26,32,black,.5f,0.f,true);
  uiText(cx-tw/2,7,t,m.timeLeft>=0&&m.timeLeft<60?red:bone,3,15,1,1);uiText(cx-lw/2,26,line.c_str(),dim,3,8,1,2);}
 if(!m.status.empty()&&m.respawnIn<0){const std::string s=upper(m.status);const int w=uiTextWidth(s.c_str(),0,12,2);
  gradient(cx-w/2-34,42,w/2+34,18,black,0.f,.7f,true);gradient(cx,42,w/2+34,18,black,.7f,0.f,true);uiText(cx-w/2,44,s.c_str(),amber,0,12,1,2);}
 const bool board=game.scoreboardHeld()||m.over;
 // The kill feed: newest last, each line full for four seconds and gone by six (the scoreboard covers it).
 if(!board){const int right=m_width-14;int y=70;const size_t first=m.feed.size()>5?m.feed.size()-5:0;
  std::string self;for(const auto& r:m.scores)if(r.self)self=r.name;
  for(size_t i=first;i<m.feed.size();++i){const auto& f=m.feed[i];const float a=f.age<4?1.f:std::max(0.f,1.f-(f.age-4)/2);if(a<=0)continue;
   const std::string killer=f.killer.empty()?"THE DARK":upper(f.killer),victim=upper(f.victim),how=f.killer.empty()?"TOOK":weaponWord(f.weapon);
   const int wk=uiTextWidth(killer.c_str(),0,11),wh=uiTextWidth(how.c_str(),3,9,1),wv=uiTextWidth(victim.c_str(),0,11),total=wk+wh+wv+20;
   const bool mine=!self.empty()&&(upper(self)==killer||upper(self)==victim);
   gradient(right-total-28,y-2,total+34,16,black,0.f,.6f*a,true);
   if(mine)fill(right+2,y-1,2,14,red,a);
   int x=right-total;uiText(x,y,killer.c_str(),f.killer.empty()?red:bone,0,11,a);x+=wk+10;
   uiText(x,y+2,how.c_str(),dim,3,9,a,1);x+=wh+10;uiText(x,y,victim.c_str(),bone,0,11,a*.85f);y+=16;}}
 // Down: the frame dims and counts back in.
 if(m.respawnIn>=0&&!m.over){fill(0,0,m_width,m_height,black,.35f);
  const char* down="YOU ARE DOWN";uiText(cx-uiTextWidth(down,0,16,6)/2,m_height/2-46,down,red,0,16,1,6);
  const char* back="BACK IN";uiText(cx-uiTextWidth(back,3,10,3)/2,m_height/2-18,back,dim,3,10,1,3);
  char n[8];std::snprintf(n,sizeof n,"%d",int(std::ceil(m.respawnIn)));uiText(cx-uiTextWidth(n,2,40)/2,m_height/2-4,n,bone,2,40);}
 if(m.over){std::string title="MATCH OVER";std::string who=m.winner.empty()?"":upper(m.winner)+(m.mode==net::Mode::Coop?"":" TAKES IT");
  fill(0,0,m_width,m_height,black,.3f);uiText(cx-uiTextWidth(title.c_str(),0,13,6)/2,22,title.c_str(),red,0,13,1,6);
  if(!who.empty())uiText(cx-uiTextWidth(who.c_str(),2,26,2)/2,36,who.c_str(),bone,2,26,1,2);}
 drawChat(game);
 if(!board)return;
 // The scoreboard: the pause menu's slab, a row a player, yours lit.
 const int w=448,x=(m_width-w)/2,y=m.over?70:52,rowH=17,maxRows=std::max(1,(m_height-y-72)/rowH),rows=std::min(int(m.scores.size()),maxRows);
 const int h=58+std::max(1,rows)*rowH+24;
 fill(x,y,w,h,ink,.95f);frame(x,y,w,h,bone,.10f);fill(x,y,w,2,red,1);
 std::string name=m.lobbyName.empty()?"MATCH":upper(m.lobbyName);while(name.size()>3&&uiTextWidth(name.c_str(),0,16,2)>230)name=name.substr(0,name.size()-4)+"...";
 uiText(x+16,y+10,name.c_str(),bone,0,16,1,2);
 std::string map=m.map;if(map.size()>4&&map.ends_with(".rmc"))map.resize(map.size()-4);
 const std::string where=std::string(net::modeName(m.mode))+"  /  "+upper(map);uiText(x+w-16-uiTextWidth(where.c_str(),3,10,1),y+15,where.c_str(),red,3,10,1,1);
 const bool coop=m.mode==net::Mode::Coop;const int hy=y+38;
 const int cRank=x+16,cName=x+40,cFrags=x+w-190,cDeaths=x+w-124,cPing=x+w-16;
 uiText(cRank,hy,"#",dim,3,9,1,2);uiText(cName,hy,"NAME",dim,3,9,1,2);uiText(cFrags,hy,coop?"KILLS":"FRAGS",dim,3,9,1,2);uiText(cDeaths,hy,"DEATHS",dim,3,9,1,2);uiText(cPing-uiTextWidth("PING",3,9,2),hy,"PING",dim,3,9,1,2);
 fill(x+12,hy+12,w-24,1,bone,.14f);
 for(int i=0;i<rows;++i){const auto& r=m.scores[size_t(i)];const int ry=y+56+i*rowH;
  if(r.self){gradient(x+10,ry-1,w-20,rowH-1,red,.5f,.08f,true);fill(x+10,ry-1,2,rowH-1,red,1);}else if(i%2)fill(x+10,ry-1,w-20,rowH-1,bone,.03f);
  char rank[8];std::snprintf(rank,sizeof rank,"%d",i+1);uiText(cRank,ry+1,rank,r.self?bone:dim,3,10);
  std::string who=r.name;while(who.size()>3&&uiTextWidth(who.c_str(),0,12)>180)who=who.substr(0,who.size()-4)+"...";
  const int nw=uiText(cName,ry,who.c_str(),r.self?bone:rgb(206,198,186),0,12,r.muted?.5f:1.f);
  int tx=cName+nw+8;
  // Speaking: three bars, the middle tallest; muted: a red word; the row the arrows picked is framed.
  if(r.speaking){for(int k=0;k<3;++k){const int bh=k==1?9:6;fill(tx+k*3,ry+11-bh,2,bh,amber,1);}tx+=14;}
  if(r.host){tx+=uiText(tx,ry+2,"HOST",amber,3,8,1,1)+8;}
  if(r.muted)uiText(tx,ry+2,"MUTED",red,3,8,1,1);
  if(i==game.scoreboardPick())frame(x+10,ry-1,w-20,rowH-1,bone,.55f);
  uiText(cFrags,ry+1,std::to_string(r.frags).c_str(),bone,3,11);uiText(cDeaths,ry+1,std::to_string(r.deaths).c_str(),dim,3,11);
  char ping[12];if(r.pingMs<0)std::snprintf(ping,sizeof ping,"--");else std::snprintf(ping,sizeof ping,"%d",r.pingMs);
  uiText(cPing-uiTextWidth(ping,3,10),ry+1,ping,r.pingMs<0?dim:r.pingMs<90?dim:r.pingMs<160?amber:red,3,10);}
 if(m.scores.empty())uiText(cName,y+56,"Nobody else is down here yet.",dim,1,12);
 const int fy=y+h-18;fill(x+12,fy-4,w-24,1,bone,.08f);
 if(!m.joinCode.empty()){uiText(x+16,fy,"JOIN CODE",dim,3,9,1,2);uiText(x+16+uiTextWidth("JOIN CODE",3,9,2)+10,fy-1,m.joinCode.c_str(),bone,3,11,1,2);}
 const char* hint=m.over?"":game.scoreboardPick()>=0?"ENTER MUTES OR UNMUTES":"ARROWS PICK A PLAYER TO MUTE";if(*hint)uiText(x+w-16-uiTextWidth(hint,3,8,2),fy+1,hint,dim,3,8,.7f,2);
}

// The chat box, lower left above the HUD: the last few lines, each fading out after eight seconds; while typing,
// the last six whatever their age and the line being typed under them. Names in bone, words a shade softer,
// notices dim and italic-free; your own name in red.
void SoftwareRenderer::drawChat(const Game& game){
 const auto& m=game.matchView();
 const auto bone=rgb(236,228,214),soft=rgb(206,198,186),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0),ink=rgb(10,8,8);
 const bool typing=game.chatOpen();
 const int x=16,width=300,lineH=13,bottom=m_height-88;
 // Wraps one line to the box's width, the name taking the first line's start.
 struct Piece{std::string text;int indent=0;};
 auto wrap=[&](const std::string& text,int first,int font,int size){std::vector<Piece> out;std::string rest=text;int indent=first;
  while(!rest.empty()&&out.size()<4){size_t n=rest.size();while(n>1&&uiTextWidth(rest.substr(0,n).c_str(),font,size)>width-indent){size_t space=rest.rfind(' ',n-1);n=space!=std::string::npos&&space>0?space:n-1;}
   out.push_back({rest.substr(0,n),indent});rest.erase(0,n);while(!rest.empty()&&rest.front()==' ')rest.erase(0,1);indent=10;}
  return out;};
 struct Drawn{const net::ChatLine* line;std::vector<Piece> pieces;float alpha;int nameWidth;};
 std::vector<Drawn> lines;int rows=0;
 const size_t keep=typing?6:4;
 for(size_t i=m.chat.size();i-->0&&lines.size()<keep;){const auto& c=m.chat[i];
  const float alpha=typing?1.f:c.age<8?1.f:std::max(0.f,1.f-(c.age-8)/2);if(alpha<=0)break;
  std::string name=c.notice?std::string():upper(c.from)+"  ";
  const int nw=c.notice?0:uiTextWidth(name.c_str(),0,11,1);
  Drawn d{&c,wrap(c.text,nw,1,12),alpha,nw};rows+=int(d.pieces.size());lines.push_back(std::move(d));}
 const int fieldH=typing?18:0;
 if(lines.empty()&&!typing)return;
 int y=bottom-fieldH-rows*lineH;
 if(typing){gradient(0,y-6,x+width+40,bottom-y+12,black,.62f,0.f,true);}
 for(auto it=lines.rbegin();it!=lines.rend();++it){const auto& d=*it;const auto& c=*d.line;
  for(size_t k=0;k<d.pieces.size();++k){
   if(!typing)gradient(x-8,y-1,width/2+40,lineH,black,.5f*d.alpha,0.f,true);
   if(k==0&&!c.notice){std::string name=upper(c.from);uiText(x,y,name.c_str(),c.self?red:bone,0,11,d.alpha,1);}
   if(c.notice&&k==0)fill(x,y+6,5,1,dim,d.alpha);
   uiText(x+d.pieces[k].indent+(c.notice&&k==0?9:0),y,d.pieces[k].text.c_str(),c.notice?dim:soft,1,12,d.alpha);
   y+=lineH;}
 }
 if(typing){
  static unsigned tick=0;++tick;
  const int fy=bottom-fieldH+3;fill(x-4,fy,width+8,fieldH-2,ink,.85f);frame(x-4,fy,width+8,fieldH-2,bone,.25f);fill(x-4,fy,2,fieldH-2,red,1);
  const int lw=uiText(x+2,fy+3,"SAY",red,3,9,1,2)+10;
  std::string text=game.chatTyped();while(text.size()>1&&uiTextWidth(text.c_str(),1,12)>width-lw-12)text.erase(0,1);
  const int tw=uiText(x+2+lw,fy+2,text.c_str(),bone,1,12);
  if((tick/24)%2==0)fill(x+3+lw+tw,fy+4,1,11,red,1);
  char count[16];std::snprintf(count,sizeof count,"%d",int(net::Match::ChatMax-game.chatTyped().size()));
  uiText(x+width-uiTextWidth(count,3,8),fy+fieldH+1,count,dim,3,8,.8f);
  uiText(x,fy+fieldH+1,"ENTER SENDS  /  ESC DROPS IT",dim,3,8,.8f,1);
 }
}

// The keys a menu takes, in caps along the bottom (the title's footer).
void SoftwareRenderer::drawMenuFooter(bool change){
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128);
 fill(40,m_height-36,m_width-80,1,bone,.10f);
 int kx=40;auto cap=[&](const char*key,const char*what){int w=uiTextWidth(key,3,10)+10;frame(kx,m_height-27,w,15,bone,.35f);uiText(kx+5,m_height-26,key,bone,3,10);kx+=w+6;kx+=uiText(kx,m_height-27,what,dim,0,11,1,1)+14;};
 cap("ENTER / A","SELECT");cap("ESC / B","BACK");cap("ARROWS / D-PAD","MOVE");if(change)cap("< >","CHANGE");
}

// PAUSE: the scene held under smoke, the title's grouped rows and its red selection, a line saying what the chosen
// row does. RESTART and QUIT TO TITLE ask again in place.
void SoftwareRenderer::drawPauseMenu(const Game& game){
 if(!hardwarePresentsWindow())for(auto&p:m_pixels)p=shade(p,.3f);
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0),amber=rgb(232,160,60);
 const bool match=game.inMatch();
 fill(0,0,m_width,m_height,black,.3f);gradient(0,0,m_width*3/4,m_height,black,.92f,0.f,true);gradient(0,m_height-80,m_width,80,black,0.f,1.f,false);gradient(0,0,m_width,60,black,.8f,0.f,false);
 for(int y=0;y<m_height;y+=2)fill(0,y,m_width,1,black,.12f);
 fill(40,21,5,5,red,1);uiText(52,16,match?"A MATCH IS UNDER WAY  //  IT DOES NOT STOP":"DEPTHWORKS INDUSTRIAL  //  SITE 9 EXTRACTION COMPLEX",dim,0,11,1,1);
 fill(40,33,m_width-80,1,bone,.10f);
 fill(40,112,4,16,red,1);uiText(52,111,"PAUSED",red,0,15,1,3);
 uiText(52,130,match?"The match goes on without you. Nobody waits down here.":"The dark holds its breath. It will not hold it long.",dim,1,13);
 const int rows=PauseMenuLayout::rows(match);
 static const char* labels[]={"RESUME","SETTINGS","SAVE GAME","LOAD GAME","RESTART","QUIT TO TITLE"};
 static const char* notes[]={"Back into the dark.","Sound, sight, hands and voice.","Leave a mark to come back to.","Go back to a mark you left.",
  "Start this descent over. Unsaved progress is lost.","Climb back up to the title. Unsaved progress is lost."};
 const int armed=game.menuArmed();
 for(int row=0;row<rows;++row){
  const int y=PauseMenuLayout::rowY(row,match),X=PauseMenuLayout::X,W=PauseMenuLayout::Width,H=PauseMenuLayout::RowHeight;const bool selected=row==game.menuSelection();const bool big=row<2;
  if(row==2||row==(match?3:4))fill(X,y-PauseMenuLayout::Gap/2-1,W-24,1,bone,.08f);
  if(selected){gradient(X-14,y,W+14,H-2,red,.55f,0.f,true);fill(X-14,y,3,H-2,red,1);}
  char index[4];std::snprintf(index,sizeof index,"%02d",row+1);
  uiText(X,y+(big?6:5),index,selected?bone:dim,3,10,selected?1.f:.5f);
  std::string label=match?(row==0?"RESUME":row==1?"SETTINGS":row==2?"JOIN CODE":"LEAVE MATCH"):labels[row];
  if(armed==row)label=row==PauseRestart?"RESTART: PRESS AGAIN":"QUIT: PRESS AGAIN";
  uiText(X+24+(selected?3:0),y+(big?1:3),label.c_str(),armed==row?red:selected?bone:dim,0,big?18:14,selected?1.f:big?.85f:.7f,1);
  if(match&&row==PauseMatchCode){const std::string code=game.matchView().joinCode.empty()?"------":game.matchView().joinCode;
   uiText(X+W-26-uiTextWidth(code.c_str(),3,12,2),y+5,code.c_str(),selected?bone:dim,3,12,1,2);}
  if(selected)uiText(X+W-14,y+(big?1:2),">",bone,0,big?18:14,.8f);
 }
 const int noteY=PauseMenuLayout::rowY(rows-1,match)+PauseMenuLayout::RowHeight+6;const int pick=std::clamp(game.menuSelection(),0,rows-1);
 std::string note=match?(pick==0?notes[0]:pick==1?notes[1]:pick==2?"Give it to whoever should come down. Enter copies it.":"Leave the others to it. They will not wait."):notes[pick];
 if(armed==pick&&!match)note=pick==PauseRestart?"Press again to start over. Unsaved progress will be lost.":"Press again to go up to the title. Unsaved progress will be lost.";
 fill(PauseMenuLayout::X,noteY+7,10,1,red,1);uiText(PauseMenuLayout::X+16,noteY,note.c_str(),armed==pick?red:dim,1,12);
 if(!game.menuMessage().empty())uiText(PauseMenuLayout::X+16,noteY+16,game.menuMessage().c_str(),amber,0,11,1,2);
 // The right of the frame: where you are, as the title shows the facility.
 const int sx=m_width-218,sy=150;
 if(match){const auto& v=game.matchView();uiText(sx,sy,"THE MATCH",dim,0,11,1,2);fill(sx,sy+16,178,1,bone,.14f);
  std::string map=v.map;if(map.size()>4&&map.ends_with(".rmc"))map.resize(map.size()-4);
  const std::string players=std::to_string(v.scores.size());
  const char* keys[]={"MODE","MAP","PLAYERS"};const std::string values[]={net::modeName(v.mode),upper(map),players};
  for(int i=0;i<3;++i){std::string val=values[i];while(val.size()>3&&uiTextWidth(val.c_str(),3,11)>120)val=val.substr(0,val.size()-4)+"...";
   uiText(sx,sy+24+i*17,keys[i],dim,3,11,.85f);uiText(sx+178-uiTextWidth(val.c_str(),3,11),sy+24+i*17,val.c_str(),i==0?amber:bone,3,11);}}
 drawMenuFooter(false);
}

// SETTINGS: the page column on the left (the open page marked, the chosen one lit), the open page's rows on the
// right with their sliders, toggles and choices, and under them the chosen row's one plain line.
void SoftwareRenderer::drawSettingsMenu(const Game& game){
 if(!hardwarePresentsWindow())for(auto&p:m_pixels)p=shade(p,.22f);
 const auto bone=rgb(236,228,214),dim=rgb(146,138,128),red=rgb(214,32,26),black=rgb(0,0,0),amber=rgb(232,160,60),ink=rgb(10,8,8);
 fill(0,0,m_width,m_height,black,.8f);gradient(0,0,m_width*2/3,m_height,black,.6f,0.f,true);
 for(int y=0;y<m_height;y+=2)fill(0,y,m_width,1,black,.12f);
 const auto tab=game.settingsTab();const bool onTabs=game.settingsOnTabs();const int sel=game.menuSelection();
 // Header: the page's name and a line in the game's voice (COMMUNICATION: why voice is off, when it is).
 fill(40,24,4,16,red,1);uiText(52,23,"SETTINGS",red,0,15,1,3);
 const int kw=uiTextWidth("SETTINGS",0,15,3);uiText(52+kw+12,27,game.settingsMutedList()?"/  MUTED PLAYERS":(std::string("/  ")+Game::settingsTabName(tab)).c_str(),dim,0,11,1,2);
 std::string line;bool warn=false;
 switch(tab){
  case SettingsTab::Audio:line="What reaches you down here, and how loud.";break;
  case SettingsTab::Video:line="How the lens sees the dark.";break;
  case SettingsTab::Controls:line="Your hands, and what they answer to.";break;
  case SettingsTab::Gameplay:line="Small mercies, and the face the others see.";break;
  case SettingsTab::Communication:{const net::Session* s=game.netSession();const auto st=s?s->voiceStatus():net::VoiceStatus{false,false,false,"voice needs the online service"};
   if(game.settings().voiceMode==0)line="Voice is off. You neither speak nor hear the others.";
   else if(!st.available){line="Voice is off: "+st.reason;warn=true;}
   else if(st.inRoom)line=st.transmitting?"You are being heard.":"In the voice room. The others can hear you when you speak.";
   else line="Voices carry down here. So does yours.";
   break;}
  case SettingsTab::Vr:line=game.vrMenu()?"For a body that is not really here.":"For the headset. They change nothing on a screen.";break;
  default:break;
 }
 while(line.size()>3&&uiTextWidth(line.c_str(),1,13)>m_width-100)line=line.substr(0,line.size()-4)+"...";
 uiText(52,43,line.c_str(),warn?amber:dim,1,13);
 if(!game.menuMessage().empty()){const std::string msg=game.menuMessage();uiText(m_width-40-uiTextWidth(msg.c_str(),0,11,2),27,msg.c_str(),amber,0,11,1,2);}
 fill(40,62,m_width-80,1,bone,.10f);
 const auto items=game.settingsItems();
 std::string help;int rowIndex=0;
 for(size_t i=0;i<items.size();++i){const auto& m=items[i];
  if(m.kind==MenuItem::Tab||(m.kind==MenuItem::Back&&m.id>=1000)){
   const int t=m.id-1000;const bool chosen=onTabs&&sel==t;const bool back=m.kind==MenuItem::Back;
   if(back)fill(m.x,m.y-SettingsLayout::BackGap/2-1,m.w-10,1,bone,.08f);
   if(chosen){gradient(m.x-14,m.y,m.w+14,m.h-1,red,.55f,0.f,true);fill(m.x-14,m.y,3,m.h-1,red,1);help=m.help;}
   else if(m.current)fill(m.x-14,m.y+2,2,m.h-5,red,.9f);
   uiText(m.x+(chosen?3:0),m.y+(back?4:3),back?"< BACK":m.label.c_str(),chosen||m.current?bone:dim,0,back?12:13,chosen||m.current?1.f:.75f,1);
   continue;}
  // A row of the open page.
  const int r=rowIndex++;const bool chosen=!onTabs&&sel==r;const float a=m.enabled?1.f:.4f;
  const int y=m.y,X=m.x,W=m.w,H=m.h;
  if(chosen){gradient(X-12,y,W+12,H-1,red,.5f,0.f,true);fill(X-12,y,2,H-1,red,1);help=m.help;}
  else fill(X,y+H-1,W,1,bone,.05f);
  const bool back=m.kind==MenuItem::Back,reset=m.label.rfind("RESET",0)==0;
  if(reset&&!m.armed)fill(X,y-1,W,1,bone,.08f);
  std::string label=m.label;while(label.size()>3&&uiTextWidth(label.c_str(),0,13,1)>(m.kind==MenuItem::Slider?SettingsLayout::SliderX-X-30:190))label=label.substr(0,label.size()-4)+"...";
  uiText(X+8+(chosen?3:0),y+2,back?("< "+label).c_str():label.c_str(),m.armed?red:chosen?bone:dim,0,13,(chosen?1.f:.85f)*a,1);
  const int right=X+W-8;
  if(m.kind==MenuItem::Slider){
   const int sx=SettingsLayout::SliderX,sw=SettingsLayout::SliderWidth,knob=sx+int(m.fraction*sw+.5f);
   if(m.meter>=0){const int mw=int(std::clamp(m.meter,0.f,1.f)*sw);const bool over=m.meter>=m.fraction;
    fill(sx,y+5,sw,7,ink,.8f);fill(sx,y+5,mw,7,over?amber:dim,over?.85f:.6f);}
   fill(sx,y+8,sw,1,bone,.25f);fill(sx,y+7,knob-sx,3,chosen?red:bone,(chosen?1.f:.55f)*a);fill(knob-1,y+3,3,11,bone,a);
   uiText(right-uiTextWidth(m.value.c_str(),3,11),y+3,m.value.c_str(),bone,3,11,a);
  }else if(!m.value.empty()){
   std::string v=m.value;const int room=m.kind==MenuItem::Choice?(right-16)-(X+200):right-(X+200);
   while(v.size()>3&&uiTextWidth(v.c_str(),3,11)>room)v=v.substr(0,v.size()-4)+"...";
   const int vw=uiTextWidth(v.c_str(),3,11);const int vx=m.kind==MenuItem::Choice?right-12-vw:right-vw;
   const bool live=m.value=="LISTENING";
   uiText(vx,y+3,v.c_str(),m.warn?(live?amber:red):chosen?bone:rgb(206,198,186),3,11,a);
   if(m.kind==MenuItem::Choice&&chosen){uiText(vx-10,y+3,"<",bone,3,11,.7f);uiText(right-6,y+3,">",bone,3,11,.7f);}
   // TEST MY MIC listening: its level runs beside the word.
   if(m.meter>=0){const int mx=X+200,mw=std::max(0,vx-14-mx);fill(mx,y+6,mw,5,ink,.8f);fill(mx,y+6,int(std::clamp(m.meter,0.f,1.f)*mw),5,amber,.85f);}
  }else if(m.kind==MenuItem::Action&&chosen)uiText(right-uiTextWidth(">",0,13),y+2,">",bone,0,13,.8f);
 }
 // CONTROLS: the keys, read-only, in two columns under the rows.
 if(tab==SettingsTab::Controls&&!game.settingsMutedList()){
  const int ky=SettingsLayout::rowY(int(game.settingsRows().size()))+14;
  uiText(SettingsLayout::RowX+8,ky,"THE KEYS",dim,0,11,1,2);fill(SettingsLayout::RowX+8,ky+15,SettingsLayout::RowWidth-16,1,bone,.12f);
  std::string talk="V";{static const std::pair<int,const char*> keys[]={{0x56,"V"},{0x42,"B"},{0x58,"X"},{0x48,"H"},{0x14,"CAPS LOCK"},{0xA4,"LEFT ALT"},{0x05,"MOUSE 4"},{0x06,"MOUSE 5"}};
   for(const auto& [code,name]:keys)if(code==game.settings().pushToTalkKey)talk=name;}
  const std::pair<const char*,std::string> binds[]={{"MOVE","W A S D"},{"SPRINT","SHIFT"},{"JUMP","SPACE"},{"CROUCH","C / CTRL"},{"PRONE","Z"},{"FIRE","LEFT MOUSE"},
   {"GUARD","RIGHT MOUSE"},{"RELOAD","R"},{"USE","E"},{"SUBDUE","Q"},{"FLASHLIGHT","F"},{"PHYSGLOVES","G"},{"TURN A PROP","MIDDLE MOUSE"},{"WEAPONS","1 2 3 / WHEEL"},
   {"INVENTORY","I"},{"SCOREBOARD","TAB"},{"CHAT","T"},{"PUSH TO TALK",talk},{"SOUND / MUSIC","M / N"},{"PAUSE","ESC"}};
  const int colW=(SettingsLayout::RowWidth-16)/2;
  for(int i=0;i<20;++i){const int cx=SettingsLayout::RowX+8+(i/10)*(colW+8),cy=ky+22+(i%10)*13;
   uiText(cx,cy,binds[i].first,dim,0,10,.9f,1);const std::string& k=binds[i].second;uiText(cx+colW-12-uiTextWidth(k.c_str(),3,10),cy,k.c_str(),bone,3,10);}
 }
 if(!help.empty()){std::string h=help;while(h.size()>3&&uiTextWidth(h.c_str(),1,12)>m_width-80-16)h=h.substr(0,h.size()-4)+"...";
  fill(40,SettingsLayout::HelpY+7,10,1,red,1);uiText(56,SettingsLayout::HelpY,h.c_str(),dim,1,12);}
 drawMenuFooter(true);
}
}

// Chat, voice and the menus they live in (docs/Systems/multiplayer.md "Chat and voice"): the match's chat box in
// the middle of a conversation and while typing, the speaking marks on the scoreboard and over a head, PAUSE (alone
// and in a match, with QUIT asking again), and every page of SETTINGS, COMMUNICATION with a fake mic level and a
// device list from a LoopbackSession, and its muted-players list.
// Checks: the chat box draws its lines (the sender's own name in red) and the line being typed; the speaking bars
// show beside the speaker on the scoreboard and on the name tag; every row of PAUSE and of each SETTINGS page has
// its words where the layout puts them, the chosen one lit red, the rows end above the help line, the help line is
// drawn; the mic meter fills to the fake level; Game::testSettingsMenu passes.
#include <algorithm>
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include "net/Loopback.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
namespace retro::lab {
namespace {
std::string fixed(double v,int d=0){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
int count(const Image& im,int x0,int y0,int x1,int y1,auto test){int n=0;
 for(int y=std::max(0,y0);y<std::min(im.height,y1);++y)for(int x=std::max(0,x0);x<std::min(im.width,x1);++x){const auto p=im.pixels[size_t(y)*im.width+x];n+=test(int((p>>16)&255),int((p>>8)&255),int(p&255));}
 return n;}
int light(const Image& im,int x0,int y0,int x1,int y1,int floor=110){return count(im,x0,y0,x1,y1,[&](int r,int g,int b){return (r+g+b)/3>floor&&r-g<60;});}
int reddish(const Image& im,int x0,int y0,int x1,int y1){return count(im,x0,y0,x1,y1,[](int r,int g,int b){return r>60&&r>g*2&&r>b*2;});}
int amberish(const Image& im,int x0,int y0,int x1,int y1){return count(im,x0,y0,x1,y1,[](int r,int g,int b){return r>150&&g>90&&g<200&&b<110&&r-g>30;});}
net::MatchView conversation(){
 net::MatchView m;m.active=true;m.mode=net::Mode::Coop;m.host=false;m.map="Foundry Deep.rmc";m.joinCode="K7QM-2X";m.lobbyName="Down the Foundry stair";
 m.scores={{0,"Vesna",4,1,0,true,false},{1,"Maddie",3,0,38,false,true},{2,"Ash",2,2,71},{3,"Okonkwo",1,3,122},{4,"Rook",0,1,54}};
 m.scores[0].speaking=true;m.scores[2].speaking=true;m.scores[3].muted=true;
 m.chat={{"","Ash joined the game",true,false,14},{"Vesna","Stay off the catwalk. Something is up there with us.",false,false,11},
  {"Ash","I hear it. Left side, by the pump room.",false,false,7},{"Maddie","Two shells left. Anyone carrying spare?",false,true,4},
  {"","Rook was taken by the dark",true,false,3},{"Vesna","Back to the lift. Now. Don't run, it hears you run.",false,false,1.2f}};
 return m;
}
std::vector<net::RemotePlayer> speakers(){
 std::vector<net::RemotePlayer> out;
 auto add=[&](std::uint8_t slot,const char* name,float x,float y,float yaw,bool speaking){net::RemotePlayer r;r.slot=slot;r.name=name;r.position={x,y,0};r.angle=yaw*3.14159265f/180;r.weapon=1;r.team=1;r.speaking=speaking;out.push_back(r);};
 add(0,"Vesna",10.6f,11.f,-80,true);add(2,"Ash",12.9f,11.2f,-100,false);
 return out;
}
const bool tube=add({
 .name="communication",.summary="chat box (talking and typing), speaking marks on the scoreboard and a name tag, PAUSE alone and in a match, every SETTINGS page with a fake mic and device list",
 .fixtures={"multiplayer-room"},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Fists);LabAccess::remotePlayers(g)=speakers();LabAccess::matchView(g)=conversation();},
 .step=[](Scene& s,float){Game& g=s.g();LabAccess::remotePlayers(g)=speakers();const auto c=conversation();LabAccess::matchView(g).chat=c.chat;LabAccess::matchView(g).scores=c.scores;},
 .capture=[](Scene& s,const Frame& f){if(!s.last())return;
  Game& g=s.g();auto& r=*s.renderer;
  auto shot=[&](Game& game,const char* label){r.render(game);s.shoot(label);return Image::of(r.pixels(),r.width(),r.height());};
  const int W=r.width(),H=r.height();(void)f;
  // The chat box, talking; then typing.
  {const Image im=shot(g,"chat box mid-conversation, two speaking over their heads");
   s.board["chat.words"]=light(im,16,H-88-9*13,330,H-88,120);s.board["chat.self_red"]=reddish(im,16,H-88-9*13,80,H-88);
   for(const auto& rep:r.remoteBodyReports())if(rep.slot==0&&rep.tag){s.board["tag.amber"]=amberish(im,int(rep.tagX)-60,int(rep.tagY)-16,int(rep.tagX)+80,int(rep.tagY)+2);s.board["tag.seen"]=1;}
   for(const auto& rep:r.remoteBodyReports())if(rep.slot==2&&rep.tag)s.board["tag.quiet_amber"]=amberish(im,int(rep.tagX)+8,int(rep.tagY)-16,int(rep.tagX)+80,int(rep.tagY)+2);}
  LabAccess::chatTyping(g,"Coming. Keep the door open for me, I am right be");
  {const Image im=shot(g,"chat box while typing: the last six lines and the line being typed");
   s.board["typing.field"]=light(im,16,H-88-16,320,H-88,120);s.board["typing.red"]=reddish(im,10,H-88-16,60,H-88);s.board["typing.words"]=light(im,16,H-88-18-9*13,330,H-88-18,120);}
  LabAccess::chatTyping(g,"",false);
  // The scoreboard: speaking bars, a muted player, the arrows' pick.
  LabAccess::holdScoreboard(g,true);LabAccess::scoreboardPick(g,3);
  {const Image im=shot(g,"scoreboard: Vesna and Ash speaking, Okonkwo muted and picked");const int w=448,x=(W-w)/2,y=52;
   auto row=[&](int i){return y+56+i*17;};
   s.board["board.speaking0"]=amberish(im,x+40,row(0)-1,x+250,row(0)+15);s.board["board.speaking2"]=amberish(im,x+40,row(2)-1,x+250,row(2)+15);
   s.board["board.quiet4"]=amberish(im,x+40,row(4)-1,x+250,row(4)+15);s.board["board.muted"]=reddish(im,x+40,row(3)-1,x+260,row(3)+15);}
  LabAccess::holdScoreboard(g,false);LabAccess::scoreboardPick(g,-1);
  // A copy of the game out of any match for PAUSE and SETTINGS, with a loopback session faking the voice side.
  net::LoopbackHub hub;auto session=hub.connect("Maddie");session->tick();session->fakeVoiceRoom=true;session->fakeMicLevel=.62f;
  session->fakeInputs={{"in-1","Headset Microphone (SteelSeries Arctis 7)",true},{"in-2","Microphone Array (Realtek(R) Audio)",false}};
  session->fakeOutputs={{"out-1","Headset Earphone (SteelSeries Arctis 7)",true},{"out-2","Speakers (Realtek(R) Audio)",false}};
  Game menu=g;LabAccess::remotePlayers(menu).clear();LabAccess::matchView(menu)={};menu.setNetSession(session.get());
  auto& set=LabAccess::settings(menu);set.voiceMode=2;set.openMicThreshold=.45f;set.micDevice="Headset Microphone (SteelSeries Arctis 7)";set.mutedPlayers={"Okonkwo","Pale Rider"};
  auto rowsDrawn=[&](const std::string& key,const Image& im,const Game& game){
   int drawn=0,total=0,values=0,valued=0;int bottom=0;
   for(const auto& m:game.settingsItems()){++total;const bool row=m.kind!=MenuItem::Tab&&!(m.kind==MenuItem::Back&&m.id>=1000);
    drawn+=light(im,m.x,m.y,m.x+(row?200:m.w),m.y+m.h,m.enabled?100:40)+reddish(im,m.x,m.y,m.x+(row?200:m.w),m.y+m.h)>=8;
    if(row){bottom=std::max(bottom,m.y+m.h);if(!m.value.empty()){++values;valued+=light(im,m.x+200,m.y,m.x+m.w,m.y+m.h,m.enabled?100:40)+amberish(im,m.x+200,m.y,m.x+m.w,m.y+m.h)+reddish(im,m.x+200,m.y,m.x+m.w,m.y+m.h)>=6;}}}
   s.board[key+".items"]=total;s.board[key+".drawn"]=drawn;s.board[key+".values"]=values;s.board[key+".valued"]=valued;s.board[key+".bottom"]=bottom;
   s.board[key+".help"]=light(im,56,SettingsLayout::HelpY,W-40,SettingsLayout::HelpY+13,100);
   const auto items=game.settingsItems();int chosen=-1,r=0;for(size_t i=0;i<items.size();++i){const bool row=items[i].kind!=MenuItem::Tab&&!(items[i].kind==MenuItem::Back&&items[i].id>=1000);
    if(game.settingsOnTabs()?(!row&&int(i)==game.menuSelection()):(row&&r==game.menuSelection()))chosen=int(i);if(row)++r;}
   if(chosen>=0){const auto& m=items[size_t(chosen)];s.board[key+".chosen_red"]=reddish(im,m.x-14,m.y,m.x+m.w,m.y+m.h);}};
  // PAUSE alone, with QUIT asking again; PAUSE in a match.
  LabAccess::pause(menu,true);
  {const Image im=shot(menu,"PAUSE");int drawn=0;for(int row=0;row<6;++row){const int y=PauseMenuLayout::rowY(row,false);drawn+=light(im,PauseMenuLayout::X,y,PauseMenuLayout::X+PauseMenuLayout::Width,y+PauseMenuLayout::RowHeight,90)>=10;}
   s.board["pause.drawn"]=drawn;s.board["pause.chosen_red"]=reddish(im,PauseMenuLayout::X-14,PauseMenuLayout::rowY(0,false),PauseMenuLayout::X+PauseMenuLayout::Width,PauseMenuLayout::rowY(0,false)+PauseMenuLayout::RowHeight);}
  {Game armed=menu;LabAccess::menuArm(armed,PauseQuit);shot(armed,"PAUSE: QUIT TO TITLE asks again");}
  {Game inMatch=g;LabAccess::pause(inMatch,true);const Image im=shot(inMatch,"PAUSE in a match: the join code, LEAVE MATCH");int drawn=0;
   for(int row=0;row<4;++row){const int y=PauseMenuLayout::rowY(row,true);drawn+=light(im,PauseMenuLayout::X,y,PauseMenuLayout::X+PauseMenuLayout::Width,y+PauseMenuLayout::RowHeight,90)>=10;}
   s.board["pause_match.drawn"]=drawn;}
  // Every SETTINGS page; COMMUNICATION on its sensitivity row (the meter), then its muted list.
  static const char* keys[]={"audio","video","controls","gameplay","communication","vr"};
  for(int t=0;t<int(SettingsTab::Count);++t){const auto tab=SettingsTab(t);
   LabAccess::settings(menu,tab,tab==SettingsTab::Communication?4:t==0?-1:0);
   const std::string label=std::string("SETTINGS: ")+Game::settingsTabName(tab)+(t==0?" (the page column chosen)":tab==SettingsTab::Communication?" (open mic, the meter at 62% over the mark)":"");
   const Image im=shot(menu,label.c_str());rowsDrawn(std::string("settings.")+keys[t],im,menu);
   if(tab==SettingsTab::Communication){const int y=SettingsLayout::rowY(4);
    s.board["meter.fill"]=amberish(im,SettingsLayout::SliderX,y+5,SettingsLayout::SliderX+SettingsLayout::SliderWidth,y+12);
    s.board["meter.past"]=amberish(im,SettingsLayout::SliderX+int(SettingsLayout::SliderWidth*.7f),y+5,SettingsLayout::SliderX+SettingsLayout::SliderWidth,y+12);}}
  {LabAccess::settings(menu,SettingsTab::Communication,10);LabAccess::openMutedList(menu);const Image im=shot(menu,"SETTINGS: COMMUNICATION, the muted players");rowsDrawn("settings.muted",im,menu);}
  {LabAccess::settings(menu,SettingsTab::Audio,4);LabAccess::menuArm(menu,4);const Image im=shot(menu,"SETTINGS: AUDIO, RESET TO DEFAULTS asks again");rowsDrawn("settings.reset",im,menu);}
  LabAccess::pause(menu,false);
  // A full-size sheet of every picture, to judge the type and spacing by eye.
  {std::vector<std::pair<std::string,const Image*>> tiles;std::vector<Image> images;images.reserve(s.shots.size());
   for(const auto& sh:s.shots){Image i(sh.width,sh.height);i.pixels=sh.pixels;images.push_back(std::move(i));}
   for(size_t i=0;i<images.size();++i)tiles.push_back({s.shots[i].label,&images[i]});
   std::error_code e;const auto dir=std::filesystem::path(labRoot())/".build"/"lab-out";std::filesystem::create_directories(dir,e);
   savePng((dir/"communication-full-sheet.png").string(),contactSheet(tiles,2,640));
   for(size_t i=0;i<images.size();++i){char name[64];std::snprintf(name,sizeof name,"communication-%02d.png",int(i+1));savePng((dir/name).string(),images[i]);}}
  r.render(g);
 },
 .metrics=[](Scene& s,Report& r){
  auto b=[&](const std::string& k){return s.board.count(k)?s.board[k]:0.;};
  r.check("the chat box draws its lines",b("chat.words")>900,fixed(b("chat.words"))+" lit pixels");
  r.check("your own name in the chat box is red",b("chat.self_red")>20,fixed(b("chat.self_red"))+" red pixels");
  r.check("typing: the line being typed is drawn in its field, SAY and the caret in red",b("typing.field")>150&&b("typing.red")>20,fixed(b("typing.field"))+" lit, "+fixed(b("typing.red"))+" red");
  r.check("typing: the conversation stays readable above the field",b("typing.words")>900,fixed(b("typing.words"))+" lit pixels");
  r.check("the scoreboard marks both speakers",b("board.speaking0")>=8&&b("board.speaking2")>=8,fixed(b("board.speaking0"))+" / "+fixed(b("board.speaking2"))+" amber pixels");
  r.check("... and not the quiet",b("board.quiet4")<3,fixed(b("board.quiet4")));
  r.check("the muted player says MUTED in red",b("board.muted")>15,fixed(b("board.muted"))+" red pixels");
  r.check("the speaking player's name tag carries the bars",b("tag.seen")>0&&b("tag.amber")>=6,fixed(b("tag.amber"))+" amber pixels");
  r.check("PAUSE draws its six rows, RESUME lit",b("pause.drawn")==6&&b("pause.chosen_red")>300,fixed(b("pause.drawn"))+" rows, "+fixed(b("pause.chosen_red"))+" red");
  r.check("PAUSE in a match draws its four rows",b("pause_match.drawn")==4);
  for(const char* page:{"audio","video","controls","gameplay","communication","vr","muted","reset"}){const std::string k=std::string("settings.")+page;
   r.check(std::string("SETTINGS ")+page+": every tab and row has its words",b(k+".items")>0&&b(k+".drawn")==b(k+".items"),fixed(b(k+".drawn"))+" of "+fixed(b(k+".items")));
   r.check(std::string("SETTINGS ")+page+": every value is drawn",b(k+".valued")==b(k+".values"),fixed(b(k+".valued"))+" of "+fixed(b(k+".values")));
   r.check(std::string("SETTINGS ")+page+": the chosen row is lit red",b(k+".chosen_red")>150,fixed(b(k+".chosen_red"))+" red");
   r.check(std::string("SETTINGS ")+page+": the rows end above the help line, which is drawn",b(k+".bottom")<=SettingsLayout::HelpY-2&&b(k+".help")>40,"rows end at "+fixed(b(k+".bottom"))+", help "+fixed(b(k+".help"))+" lit");}
  r.check("the mic meter fills to the fake level, past the mark",b("meter.fill")>80&&b("meter.past")<3,fixed(b("meter.fill"))+" amber pixels, "+fixed(b("meter.past"))+" past 70%");
  {std::ostringstream o;const bool ok=Game::testSettingsMenu(o);std::string failed;std::istringstream in(o.str());for(std::string line;std::getline(in,line);)if(line.rfind("FAIL",0)==0)failed+=line+"; ";
   r.check("the menus' flow: PAUSE, SETTINGS pages and rows, resets, the muted list, VR, the title, a match, the settings file (Game::testSettingsMenu)",ok,failed.empty()?"all pass":failed);}
 }});
}
}

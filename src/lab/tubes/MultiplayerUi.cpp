// Multiplayer as the player sees it (docs/Systems/multiplayer.md): other players in the world and a match's HUD,
// filled by hand into Game's remotePlayers and matchView (no network), and the title's multiplayer pages over a
// LoopbackHub. Pictures: six other players (standing with the shotgun, running with the pistol, aiming, crouched,
// dying, and one VR-posed) under the kill feed; the scoreboard; the respawn count; the pause menu in a match; the
// title with MULTIPLAYER chosen; the multiplayer page signed in and offline; the browser with games listed; the
// host page; the lobby with its join code; join by code; character.
// Checks: each drawn body changes the frame where it stands (with and without the other players) and stands on
// its feet there; a player dead long enough is not drawn; name tags over the living; every row of the title and of
// each page puts its words where Game::titleItems says it is, the chosen row lit red; Game::testMultiplayerMenu's
// flow (hosting, browsing, a join code, leaving, the character saved) passes.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include "net/Loopback.h"
#include "world/CustomCampaign.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
namespace retro::lab {
namespace {
constexpr float Deg=3.14159265f/180.f;
// Tests stand in for Game::update's own session pump (the Match layer's): tick, then update.
void tickUpdate(Game& g,const InputState& in,float dt){g.update(in,dt);}   // Game::update pumps the session itself
std::string fixed(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
AvatarPose vrPose(Point3 feet,float angle){
 AvatarPose p;p.active=true;const Point3 f{std::cos(angle),std::sin(angle),0},right{-f.y,f.x,0},left{f.y,-f.x,0};
 p.head=feet+Point3{0,0,1.42f}+f*.04f;p.headYaw=angle;p.headPitch=-.12f;p.bodyYaw=angle;p.root=feet;p.eye=1.42f;
 p.hands[0].tracked=true;p.hands[0].wrist=feet+f*.28f+left*.2f+Point3{0,0,1.02f};p.hands[0].forward=f;p.hands[0].palm=right;p.hands[0].curls={.3f,.3f,.35f,.35f,.4f};
 p.hands[1].tracked=true;p.hands[1].wrist=feet+f*.42f+right*.1f+Point3{0,0,1.24f};p.hands[1].forward=f;p.hands[1].palm=left;p.hands[1].curls={.8f,.9f,.9f,.9f,.9f};
 p.gunSide=1;p.inHand=3;p.back=2;return p;
}
// The cast: slot, name, where, facing (degrees), and what they are doing.
std::vector<net::RemotePlayer> cast(){
 std::vector<net::RemotePlayer> out;
 auto add=[&](std::uint8_t slot,const char* name,float x,float y,float yaw){net::RemotePlayer r;r.slot=slot;r.name=name;r.position={x,y,0};r.angle=yaw*Deg;out.push_back(r);return &out.back();};
 {auto* r=add(1,"Ash",9.2f,11.f,-70);r->weapon=1;}                                                  // standing, the shotgun at rest
 {auto* r=add(2,"Vesna",10.3f,11.3f,0);r->weapon=2;r->velocity={3.4f,0};}                            // running across with the pistol
 {auto* r=add(3,"Rook",11.6f,11.f,-105);r->weapon=1;r->shotAge=1.f;r->pitch=.06f;}                   // aiming the shotgun
 {auto* r=add(4,"Okonkwo",12.8f,11.f,-125);r->weapon=2;r->crouched=true;r->character=1;}             // crouched with the pistol (the Limitator: Claire stands in)
 {auto* r=add(5,"Tamsin",13.9f,11.4f,-80);r->weapon=1;r->alive=false;r->deathAge=1.6f;r->health=0;}  // falling dead
 {auto* r=add(6,"Halloran",14.8f,11.f,-95);r->vr=true;r->pose=vrPose(r->position,r->angle);r->weapon=2;} // in VR, the pistol in the right hand
 {auto* r=add(7,"Wren",12.9f,17.f,-90);r->alive=false;r->deathAge=7.f;r->health=0;}                // dead long ago: not drawn
 for(auto& r:out)if(!r.alive)r.weapon=0;
 return out;
}
net::MatchView match(){
 net::MatchView m;m.active=true;m.mode=net::Mode::Deathmatch;m.host=true;m.map="Menu Arena.rmc";m.joinCode="K7QM-2X";m.lobbyName="The Pit";m.fragLimit=20;m.timeLeft=431.4f;
 m.scores={{0,"Maddie",12,3,0,true,true},{1,"Ash",9,5,42},{2,"Vesna",7,6,88},{3,"Rook",5,8,131},{4,"Okonkwo",3,4,172},{5,"Tamsin",2,9,64},{6,"Halloran",1,2,-1},{7,"Wren",0,7,55}};
 m.feed={{"Okonkwo","Wren",2,5.2f},{"Ash","Vesna",1,3.1f},{"","Tamsin",0,1.9f},{"Maddie","Rook",1,.6f}};
 return m;
}
std::shared_ptr<CustomCampaign> arena(const char* name,std::uint64_t key,bool deathmatch,bool coop){
 auto c=std::make_shared<CustomCampaign>();c->name=name;c->sourceFile=std::string(name)+".rmc";c->key=key;c->multiplayer.deathmatch=deathmatch;c->multiplayer.coop=coop;
 auto map=std::make_shared<AuthoredMapData>();map->name="Arena";map->definition={{0,0},{6.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
 AuthoredLayerData layer;for(auto& r:layer.rows)r=std::string(24,'.');map->layers.push_back(layer);c->maps.push_back(map);return c;
}
// Words on the frame: pixels lighter than the smoke and not the red of a highlight; the red of a chosen row.
int light(const Image& im,int x0,int y0,int x1,int y1,int floor=100){int n=0;
 for(int y=std::max(0,y0);y<std::min(im.height,y1);++y)for(int x=std::max(0,x0);x<std::min(im.width,x1);++x){const auto p=im.pixels[size_t(y)*im.width+x];const int r=(p>>16)&255,g=(p>>8)&255,b=p&255;n+=(r+g+b)/3>floor&&r-g<60;}
 return n;}
int reddish(const Image& im,int x0,int y0,int x1,int y1){int n=0;
 for(int y=std::max(0,y0);y<std::min(im.height,y1);++y)for(int x=std::max(0,x0);x<std::min(im.width,x1);++x){const auto p=im.pixels[size_t(y)*im.width+x];const int r=(p>>16)&255,g=(p>>8)&255,b=p&255;n+=r>60&&r>g*2&&r>b*2;}
 return n;}
double moved(const Image& a,const Image& b,int x0,int y0,int x1,int y1){size_t n=0,m=0;
 for(int y=std::max(0,y0);y<std::min(a.height,y1);++y)for(int x=std::max(0,x0);x<std::min(a.width,x1);++x){const auto p=a.pixels[size_t(y)*a.width+x],q=b.pixels[size_t(y)*a.width+x];int d=0;for(int k=0;k<24;k+=8)d=std::max(d,std::abs(int((p>>k)&255)-int((q>>k)&255)));m+=d>30;++n;}
 return n?double(m)/double(n):0;}
// Every row of a page drawn where titleItems puts it: words in each (dimmer for a greyed row), red on the chosen.
void rows(Scene& s,const std::string& page,const Image& im,const Game& g){
 const auto items=g.titleItems();int drawn=0;
 for(size_t i=0;i<items.size();++i){const auto& t=items[i];const int words=light(im,t.x,t.y,t.x+t.w,t.y+t.h,t.enabled?100:32);drawn+=words>=10;}
 s.board["mp.page."+page+".rows"]=double(items.size());s.board["mp.page."+page+".drawn"]=drawn;
 if(!items.empty()){const auto& t=items[size_t(std::clamp(g.titleSelection(),0,int(items.size())-1))];s.board["mp.page."+page+".chosen_red"]=t.kind==TitleItem::Field?999:reddish(im,t.x-14,t.y,t.x+t.w,t.y+t.h);}
}
const bool tube=add({
 .name="multiplayer-ui",.summary="other players drawn where they stand (standing, running, aiming, crouched, dying, VR), the match HUD, and the title's multiplayer pages over a loopback hub",
 .friends={"claire"},.fixtures={"multiplayer-room"},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::equip(g,WeaponId::Fists);LabAccess::remotePlayers(g)=cast();LabAccess::matchView(g)=match();},
 .step=[](Scene& s,float){Game& g=s.g();LabAccess::remotePlayers(g)=cast();auto m=match();LabAccess::matchView(g).feed=m.feed;},
 .capture=[](Scene& s,const Frame& f){if(!s.last())return;
  Game& g=s.g();auto& r=*s.renderer;
  const Image with=Image::of(f.pixels,f.width,f.height);s.shoot("six players, the kill feed (one dead long ago is gone)");
  const auto reports=r.remoteBodyReports();const auto players=LabAccess::remotePlayers(g);
  // The same moment without them: what each body changed, inside its own box on screen.
  LabAccess::remotePlayers(g).clear();r.render(g);const Image without=Image::of(r.pixels(),r.width(),r.height());LabAccess::remotePlayers(g)=players;
  for(const auto& p:players){const std::string k="mp.body."+std::to_string(p.slot);const SoftwareRenderer::RemoteBodyReport* rep=nullptr;for(const auto& x:reports)if(x.slot==p.slot)rep=&x;
   s.board[k+".drawn"]=rep&&rep->drawn;s.board[k+".tag"]=rep&&rep->tag;
   Point3 lo=rep&&rep->drawn?rep->low:Point3{p.position.x-.4f,p.position.y-.4f,p.position.z},hi=rep&&rep->drawn?rep->high:Point3{p.position.x+.4f,p.position.y+.4f,p.position.z+1.8f};
   float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;int seen=0;
   for(int c=0;c<8;++c){const Point3 q{c&1?hi.x:lo.x,c&2?hi.y:lo.y,c&4?hi.z:lo.z};const auto pr=project(s,q);if(!pr.visible)continue;++seen;x0=std::min(x0,pr.x);y0=std::min(y0,pr.y);x1=std::max(x1,pr.x);y1=std::max(y1,pr.y);}
   s.board[k+".moved"]=seen?moved(with,without,int(x0),int(y0),int(x1)+1,int(y1)+1):0;
   if(rep&&rep->drawn){s.board[k+".offset"]=std::hypot((lo.x+hi.x)*.5f-p.position.x,(lo.y+hi.y)*.5f-p.position.y);s.board[k+".floor"]=lo.z-p.position.z;s.board[k+".height"]=hi.z-lo.z;}}
  // The scoreboard while Tab is held; the respawn count; the pause menu in a match.
  LabAccess::holdScoreboard(g,true);r.render(g);s.shoot("scoreboard (Tab held)");
  {const Image im=Image::of(r.pixels(),r.width(),r.height());const int w=448,x=(im.width-w)/2;s.board["mp.scoreboard.words"]=light(im,x,52+56,x+w,52+56+8*17);s.board["mp.scoreboard.self_red"]=reddish(im,x+10,52+55,x+w-10,52+55+17);}
  LabAccess::holdScoreboard(g,false);
  LabAccess::matchView(g).respawnIn=2.4f;r.render(g);s.shoot("down: the respawn count");LabAccess::matchView(g).respawnIn=-1;
  LabAccess::pause(g,true);r.render(g);s.shoot("pause menu in a match: LEAVE MATCH, the join code");LabAccess::pause(g,false);
  // The title and its multiplayer pages, on a copy of the game (no match), over a loopback hub with five games open.
  net::LoopbackHub hub;std::vector<std::unique_ptr<net::LoopbackSession>> hosts;
  struct Open{const char* who;const char* name;const char* map;net::Mode mode;int players,max,ping;bool priv;};
  const Open open[]={{"Ash","Night shift on the Pit","Menu Arena.rmc",net::Mode::Deathmatch,5,8,38,false},{"Vesna","Down the Foundry stair","Foundry Deep.rmc",net::Mode::Coop,2,4,71,false},
   {"Rook","Lamps out, knives in","Menu Arena.rmc",net::Mode::Deathmatch,8,8,54,false},{"Okonkwo","Waste Handling","Waste Handling.rmc",net::Mode::Deathmatch,3,12,122,false},
   {"Tamsin","Sample Facility, slow","Sample Facility.rmc",net::Mode::Coop,1,6,188,false},{"Wren","Not for you","Menu Arena.rmc",net::Mode::Deathmatch,2,8,20,true}};
  for(const auto& o:open){auto h=hub.connect(o.who);h->tick();net::HostOptions opt;opt.name=o.name;opt.map=o.map;opt.mode=o.mode;opt.maxPlayers=o.max;opt.privateGame=o.priv;h->host(opt);
   for(auto& [id,l]:hub.lobbies)if(l.info.owner==h->self()){l.info.players=o.players;l.info.pingMs=o.ping;}hosts.push_back(std::move(h));}
  auto guestSession=hub.connect("Maddie");guestSession->tick();auto hostSession=hub.connect("Maddie");hostSession->tick();
  Game title=g;LabAccess::remotePlayers(title).clear();LabAccess::matchView(title)={};
  LabAccess::customCampaigns(title)={arena("Menu Arena",11,true,true),arena("Foundry Deep",12,false,true),arena("Waste Handling",13,true,false)};
  title.setNetSession(guestSession.get());title.showTitleScreen();LabAccess::titleSelection(title)=TitleMultiplayer;tickUpdate(title,{},1.f/60);
  auto shot=[&](Game& game,const char* label){r.render(game);s.shoot(label);return Image::of(r.pixels(),r.width(),r.height());};
  {const Image im=shot(title,"title: MULTIPLAYER chosen");int drawn=0;
   for(int row=0;row<TitleMenuLayout::Rows;++row){const int y=TitleMenuLayout::rowY(row);drawn+=light(im,TitleMenuLayout::X,y,TitleMenuLayout::X+TitleMenuLayout::Width,y+TitleMenuLayout::RowHeight,90)>=10;}
   s.board["mp.title.drawn"]=drawn;const int y=TitleMenuLayout::rowY(TitleMultiplayer);s.board["mp.title.chosen_red"]=reddish(im,TitleMenuLayout::X-14,y,TitleMenuLayout::X+TitleMenuLayout::Width,y+TitleMenuLayout::RowHeight);
   s.board["mp.title.other_red"]=reddish(im,TitleMenuLayout::X-14,TitleMenuLayout::rowY(0),TitleMenuLayout::X+TitleMenuLayout::Width,TitleMenuLayout::rowY(0)+TitleMenuLayout::RowHeight);}
  {InputState accept{};accept.menuAccept=true;tickUpdate(title,accept,1.f/60);tickUpdate(title,{},1.f/60);}
  s.board["mp.page.multiplayer.open"]=title.titlePage()==Game::TitlePage::Multiplayer;
  rows(s,"multiplayer",shot(title,"MULTIPLAYER, signed in"),title);
  {Game offline=title;offline.setNetSession(nullptr);offline.openTitlePage(Game::TitlePage::Multiplayer);rows(s,"offline",shot(offline,"MULTIPLAYER, offline: greyed, and why"),offline);
   int greyed=0;for(const auto& t:offline.titleItems())greyed+=!t.enabled;s.board["mp.page.offline.greyed"]=greyed;}
  auto press=[&](Game& game,int selection){LabAccess::titleSelection(game)=selection;InputState accept{};accept.menuAccept=true;tickUpdate(game,accept,1.f/60);tickUpdate(game,{},1.f/60);};
  auto lobbyRows=[&](const Game& game){int n=0;for(const auto& t:game.titleItems())n+=t.kind==TitleItem::Lobby;return n;};
  title.openTitlePage(Game::TitlePage::Browse);tickUpdate(title,{},1.f/60);
  press(title,2);   // HIDE FULL off: the full game shows, greyed
  LabAccess::titleSelection(title)=3;   // the nearest game
  {const Image im=shot(title,"BROWSE: open games (the full one greyed, the private one unlisted)");rows(s,"browse",im,title);s.board["mp.page.browse.lobbies"]=lobbyRows(title);}
  press(title,1);press(title,2);   // MODE: DEATHMATCH, HIDE FULL on
  s.board["mp.page.browse.deathmatch_lobbies"]=lobbyRows(title);shot(title,"BROWSE: deathmatch only, full games hidden");
  title.openTitlePage(Game::TitlePage::JoinCode);{InputState typed{};typed.textInput="k7qm2";tickUpdate(title,typed,1.f/60);tickUpdate(title,{},1.f/60);}
  rows(s,"join",shot(title,"JOIN BY CODE, typing"),title);
  title.openTitlePage(Game::TitlePage::Character);{InputState down{};down.menuDown=true;tickUpdate(title,down,1.f/60);tickUpdate(title,{},1.f/60);}
  rows(s,"character",shot(title,"CHARACTER: the Limitator selected"),title);
  Game host=title;host.setNetSession(hostSession.get());host.openTitlePage(Game::TitlePage::Host);
  rows(s,"host",shot(host,"HOST A GAME"),host);
  {LabAccess::titleSelection(host)=5;InputState accept{};accept.menuAccept=true;tickUpdate(host,accept,1.f/60);tickUpdate(host,{},1.f/60);tickUpdate(host,{},1.f/60);}
  // A host who has the map goes straight down into it (the Match starts at once); the lobby page is the waiting
  // room of a guest whose welcome (and perhaps the map) has not arrived yet.
  s.board["mp.page.host.dropped_in"]=hostSession->state()==net::SessionState::InLobby&&!host.titleScreen();
  Game guest=title;guest.setNetSession(guestSession.get());guestSession->joinCode(hostSession->lobby().joinCode);tickUpdate(guest,{},1.f/60);
  s.board["mp.page.lobby.open"]=guest.titlePage()==Game::TitlePage::Lobby&&guestSession->state()==net::SessionState::InLobby;
  {const Image im=shot(guest,"LOBBY: a guest waiting on the host, the code big and copyable");rows(s,"lobby",im,guest);
   s.board["mp.page.lobby.code_words"]=light(im,im.width-292,146+30,im.width-40,146+84,120);s.board["mp.page.lobby.code_length"]=double(guestSession->state()==net::SessionState::InLobby?guestSession->lobby().joinCode.size():0);}
  // A full-size sheet of every picture, to judge the type and spacing by eye.
  {std::vector<std::pair<std::string,const Image*>> tiles;std::vector<Image> images;images.reserve(s.shots.size());
   for(const auto& sh:s.shots){Image i(sh.width,sh.height);i.pixels=sh.pixels;images.push_back(std::move(i));}
   for(size_t i=0;i<images.size();++i)tiles.push_back({s.shots[i].label,&images[i]});
   std::error_code e;const auto dir=std::filesystem::path(labRoot())/".build"/"lab-out";std::filesystem::create_directories(dir,e);
   savePng((dir/"multiplayer-ui-full-sheet.png").string(),contactSheet(tiles,2,640));}
  // The tube's own scene is the last thing drawn (the repeat check compares it).
  r.render(g);
 },
 .metrics=[](Scene& s,Report& r){
  auto b=[&](const std::string& k){return s.board.count(k)?s.board[k]:0.;};
  static const char* doing[]={"","standing with the shotgun","running with the pistol","aiming the shotgun","crouched with the pistol (the Limitator's slot, Claire standing in)","falling dead","posed from VR"};
  for(int slot=1;slot<=6;++slot){const std::string k="mp.body."+std::to_string(slot);
   r.metric("body"+std::to_string(slot)+".moved",b(k+".moved"),"share");
   r.check(std::string("a remote player is drawn ")+doing[slot],b(k+".drawn")>0);
   r.check(std::string("... where it stands on screen: ")+doing[slot],b(k+".moved")>.08,fixed(b(k+".moved")*100,1)+"% of its box changes with it drawn");
   if(slot!=5)r.check(std::string("... centred on its place and on its feet: ")+doing[slot],b(k+".offset")<.5&&std::fabs(b(k+".floor"))<.12&&b(k+".height")>(slot==4?.8:1.3),
    "box centre "+fixed(b(k+".offset"),2)+" m off, lowest point "+fixed(b(k+".floor"),3)+" m, "+fixed(b(k+".height"),2)+" m tall");
   else r.check("... the dying body lies on the floor where it fell",std::fabs(b(k+".floor"))<.15&&b(k+".offset")<1.2,"lowest point "+fixed(b(k+".floor"),3)+" m, "+fixed(b(k+".offset"),2)+" m from its place");
   if(slot!=5)r.check(std::string("... with its name over its head: ")+doing[slot],b(k+".tag")>0);}
  r.check("a player dead longer than the death clip is not drawn",b("mp.body.7.drawn")==0&&b("mp.body.7.moved")<.01,fixed(b("mp.body.7.moved")*100,2)+"% of where it lay changes");
  r.check("the scoreboard draws its rows, yours lit",b("mp.scoreboard.words")>400&&b("mp.scoreboard.self_red")>200,fixed(b("mp.scoreboard.words"),0)+" lit pixels, "+fixed(b("mp.scoreboard.self_red"),0)+" red in your row");
  r.check("the title draws all six rows",b("mp.title.drawn")==6,fixed(b("mp.title.drawn"),0)+" of 6");
  r.check("the title's chosen row (MULTIPLAYER) is lit red, the others not",b("mp.title.chosen_red")>300&&b("mp.title.other_red")<40,fixed(b("mp.title.chosen_red"),0)+" red pixels vs "+fixed(b("mp.title.other_red"),0));
  r.check("accepting MULTIPLAYER opens its page",b("mp.page.multiplayer.open")>0);
  for(const char* page:{"multiplayer","offline","browse","join","character","host","lobby"}){const std::string k=std::string("mp.page.")+page;
   r.check(std::string("the ")+page+" page draws every row where it is",b(k+".rows")>0&&b(k+".drawn")==b(k+".rows"),fixed(b(k+".drawn"),0)+" of "+fixed(b(k+".rows"),0));
   r.check(std::string("the ")+page+" page lights its chosen row",b(k+".chosen_red")>150,fixed(b(k+".chosen_red"),0)+" red pixels");}
  r.check("offline, the four rows that need a session are greyed",b("mp.page.offline.greyed")==4);
  r.check("the browser lists the five listed games, not the private one",b("mp.page.browse.lobbies")==5,fixed(b("mp.page.browse.lobbies"),0)+" rows");
  r.check("the browser's mode filter (DEATHMATCH) with full games hidden leaves the open deathmatches",b("mp.page.browse.deathmatch_lobbies")==2,fixed(b("mp.page.browse.deathmatch_lobbies"),0)+" rows");
  r.check("HOST GAME takes a host who has the map straight down into it",b("mp.page.host.dropped_in")>0);
  r.check("a joining guest waits on the lobby page with the code big",b("mp.page.lobby.open")>0&&b("mp.page.lobby.code_length")==7&&b("mp.page.lobby.code_words")>500,fixed(b("mp.page.lobby.code_words"),0)+" lit pixels in the code");
  {std::ostringstream o;const bool ok=Game::testMultiplayerMenu(o);std::string failed;std::istringstream in(o.str());for(std::string line;std::getline(in,line);)if(line.rfind("FAIL",0)==0)failed+=line+"; ";
   r.check("the menus' flow: offline greying, host, browse, join code, leave, LEAVE MATCH, the character saved (Game::testMultiplayerMenu)",ok,failed.empty()?"all pass":failed);}
 },
 .reporter=[](Scene& s,Overlay& o){
  for(const auto& rep:s.renderer->remoteBodyReports()){if(!rep.drawn)continue;o.box(rep.low,rep.high,rep.vr?0x40c0ff:0x40ff60);o.cross(rep.feet,.15f,0xffff60);
   o.label(rep.head,"SLOT "+std::to_string(rep.slot)+(rep.vr?" VR":"")+" CLIP "+std::to_string(rep.lower)+"/"+std::to_string(rep.layer),0xffff60);}
 }});
}
}

// The crawlspace kits (tools/campaign/kits.py: Wall vent, Vent duct, Vent corner, Vent grate, Vent drop, Crawl
// tunnel, Big pipe) as the Starter Map's Kits Gallery 4 sets them down (make_starter_map.py vent_run): a vent run
// on a deck and legs 1.2 m up, and a tunnel and a pipe on the ground.
//
// crawl-kits: the player
//  - walks up to the wall vent standing and is stopped at its mouth;
//  - crouches and is laid prone by the game as she crawls in, along the duct, round the corner, over the grate
//    (which holds her: she never drops through it), along the second duct and down the drop to the ground;
//    she never stands or kneels anywhere in the run's ducts;
//  - walks into the big pipe standing and is stopped; crouched she gets through it kneeling, never prone;
//  - walks into the crawl tunnel standing and is stopped; crouched she is laid prone and crawls through.
// And at points in every piece: a standing (1.6 m) and kneeling (1.05 m) hull do not fit, a prone (0.58 m) one
// does; in the pipe a kneeling one fits and a standing one does not.
// Pictures: the run from outside, its collision, under the grate looking up, the wall vent, the tunnel and pipe,
// and her own view along the way. The fixture crawl-kits-film also writes every second frame to film-out (PNG, captioned)
// for the testers' video.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "lab/LabImage.h"
#include "renderer/DebugLayer.h"
#include "world/CustomCampaign.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
namespace retro::lab {
namespace {
constexpr float Duct=1.25f;   // the run's duct floor: the deck's 1.2 m and the duct's 5 cm sheet
std::shared_ptr<const CustomCampaign> starter(){static auto c=loadCustomCampaignFile(std::filesystem::path(labRoot())/"custom maps/Starter Map.rmc");return c;}
int levelNamed(const char* name){const auto c=starter();if(!c)return -1;for(size_t i=0;i<c->maps.size();++i)if(c->maps[i]->name=="Starter Map / "+std::string(name))return int(i);return -1;}
std::string fixed(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
// In the run's ducts, up on the deck's height (the drop's shaft, where she may kneel, is not in it).
bool inRunDuct(Vec2 p,float z){return z>1.1f&&((p.x>5.55f&&p.x<6.45f&&p.y>12.75f&&p.y<16.9f)||(p.y>12.75f&&p.y<13.65f&&p.x>5.55f&&p.x<12.45f));}
bool overGrate(Vec2 p){return p.y>12.75f&&p.y<13.65f&&p.x>6.6f&&p.x<8.4f;}
bool inPipe(Vec2 p){return std::fabs(p.x-18.5f)<.9f&&p.y>15.2f&&p.y<18.8f;}
bool inTunnel(Vec2 p){return std::fabs(p.x-18.5f)<.5f&&p.y>3.2f&&p.y<6.8f;}
void steer(Scene& s,Vec2 to){Player& p=LabAccess::player(s.g());p.angle=std::atan2(to.y-p.pos.y,to.x-p.pos.x);s.input.forward=true;}
void place(Scene& s,Vec2 at,float yaw){Player& p=LabAccess::player(s.g());p.pos=at;p.angle=yaw;p.pitch=0;p.z=LabAccess::groundHeight(s.g(),at);p.crouched=p.prone=false;LabAccess::velocity(s.g())={};}
const bool tube=add({
 .name="crawl-kits",.summary="the crawlspace kits: a vent run, a crawl tunnel and a big pipe stop a standing player and let her crawl (or crouch) through",
 .fixtures={"crawl-kits"},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::setGod(g,true);LabAccess::playerCamera(g);debuglayer::visible=0;
  place(s,{6.f,19.2f},-1.5707963f);
  // Each piece at a point inside it: what fits there.
  struct Probe {const char* key;Vec2 at;float feet;bool kneel;};
  const Probe probes[]={{"vent mouth",{6,16.85f},Duct,false},{"duct",{6,15.2f},Duct,false},{"corner",{6,13.2f},Duct,false},{"grate",{7.5f,13.2f},Duct,false},
   {"second duct",{10,13.2f},Duct,false},{"drop, upper",{12.f,13.2f},Duct,false},{"drop, lower mouth",{13.3f,13.2f},.05f,false},{"tunnel",{18.5f,5.f},.04f,false},
   {"pipe",{18.5f,17.f},-1,true}};
  int good=0;std::string bad;
  // The yard's ground: the kits stand on it, so every height in them is over it.
  const float base=LabAccess::groundHeight(g,{2.f,23.f});s.board["base"]=base;std::string diag="ground "+fixed(base)+"\n";
  for(float y=21.f;y>=13.f;y-=.5f){const Vec2 at{18.5f,y};const float f=LabAccess::groundHeight(g,at,base+.4f);
   diag+="pipe y "+fixed(y,1)+" floor "+fixed(f)+" stand "+std::to_string(LabAccess::hullFits(g,at,f,Player::StandingHeight))+" kneel "+std::to_string(LabAccess::hullFits(g,at,f,Player::CrouchedHeight))+"\n";}
  for(const auto& pr:probes){const float feet=pr.feet<0?LabAccess::groundHeight(g,pr.at,base+.4f):base+pr.feet;
   const bool stand=LabAccess::hullFits(g,pr.at,feet,Player::StandingHeight),kneel=LabAccess::hullFits(g,pr.at,feet,Player::CrouchedHeight),prone=LabAccess::hullFits(g,pr.at,feet,Player::ProneHeight);
   const bool ok=pr.kneel?(!stand&&kneel):(!stand&&!kneel&&prone);good+=ok;
   if(!ok)bad+=std::string(" ")+pr.key+" (stand "+std::to_string(stand)+" kneel "+std::to_string(kneel)+" prone "+std::to_string(prone)+" at "+fixed(feet)+")";}
  s.board["probes.good"]=good;s.board["probes.count"]=int(std::size(probes));s.board["phase"]=0;s.board["leg"]=0;s.board["mark"]=0;s.board["duct.hull"]=0;s.board["grate.minz"]=99;s.board["pipe.prone"]=0;s.board["tunnel.notprone"]=0;
  s.board["tunnel.inside"]=0;s.board["pipe.inside"]=0;s.board["run.inside"]=0;
  std::ofstream(std::filesystem::path(labRoot())/".build"/"crawl-kits-probes.txt")<<(bad.empty()?"all probes fit as designed":bad)<<'\n'<<diag;},
 .step=[](Scene& s,float){
  Game& g=s.g();Player& p=LabAccess::player(g);int phase=int(s.board["phase"]);const int since=s.frame-int(s.board["mark"]);
  auto next=[&]{s.board["phase"]=++phase;s.board["mark"]=s.frame;s.board["leg"]=0;};
  switch(phase){
  case 0:   // standing, up to the wall vent
   steer(s,{6,12});if(since==100){s.board["vent.blocked"]=p.pos.y;next();}break;
  case 1:{  // crouched, the whole run: north, round the corner, east over the grate, down the drop, out
   const Vec2 route[]={{6,13.2f},{12.1f,13.2f},{15.5f,13.2f}};int leg=int(s.board["leg"]);s.input.crouch=true;
   if(leg<3){if(std::hypot(route[leg].x-p.pos.x,route[leg].y-p.pos.y)<.2f)s.board["leg"]=++leg;}
   if(leg<3)steer(s,route[leg]);
   if(inRunDuct(p.pos,p.z-float(s.board["base"]))){s.board["run.inside"]+=1;s.board["duct.hull"]=std::max(s.board["duct.hull"],double(p.hullHeight()));}
   if(overGrate(p.pos)&&p.z-s.board["base"]>.6)s.board["grate.minz"]=std::min(s.board["grate.minz"],p.z-s.board["base"]);
   if(overGrate(p.pos)&&p.z-s.board["base"]>.6)s.board["grate.crossed"]=1;
   if(leg>=3){s.board["run.end.x"]=p.pos.x;s.board["run.end.z"]=p.z-s.board["base"];s.board["run.s"]=since/60.;next();}
   else if(since>60*40){s.board["run.end.x"]=p.pos.x;s.board["run.end.z"]=p.z-s.board["base"];next();}
   break;}
  case 2:   // the big pipe: standing first
   if(since==1)place(s,{18.5f,21.f},-1.5707963f);else steer(s,{18.5f,12});if(since==90){s.board["pipe.blocked"]=p.pos.y;next();}break;
  case 3:   // crouched through it
   s.input.crouch=true;steer(s,{18.5f,13.5f});if(inPipe(p.pos)){s.board["pipe.inside"]+=1;if(p.prone)s.board["pipe.prone"]+=1;}
   if(p.pos.y<14.4f||since>60*15){s.board["pipe.end"]=p.pos.y;next();}break;
  case 4:   // the crawl tunnel: standing first
   if(since==1)place(s,{18.5f,8.6f},-1.5707963f);else steer(s,{18.5f,1});if(since==90){s.board["tunnel.blocked"]=p.pos.y;next();}break;
  case 5:   // crouched through it
   s.input.crouch=true;steer(s,{18.5f,1.5f});if(inTunnel(p.pos)){s.board["tunnel.inside"]+=1;if(!p.prone)s.board["tunnel.notprone"]+=1;}
   if(p.pos.y<2.2f||since>60*20){s.board["tunnel.end"]=p.pos.y;next();}break;
  default:break;}
  // Her view along the way: down through the grate as she crosses it, level otherwise.
  p.pitch=phase==0&&since>50?-55.f:phase==1&&overGrate(p.pos)&&p.pos.x>7.1f&&p.pos.x<8.2f?-110.f:0.f;},
 .capture=[](Scene& s,const Frame& f){
  const auto out=s.fixture->value("film-out");
  if(!out.empty()&&s.frame%2==0&&!(s.board["phase"]>=6&&s.frame-s.board["mark"]>60)){const int n=s.frame/2;const auto dir=std::filesystem::path(labRoot())/out;std::filesystem::create_directories(dir);
   // A caption for the testers: what she is doing and the stance the game gave her.
   const auto& q=s.g().player();const int ph=int(s.board["phase"]);
   static const char* what[]={"STANDING: UP TO THE WALL VENT","CROUCH: INTO THE VENT RUN","STANDING: UP TO THE BIG PIPE","CROUCH: THROUGH THE BIG PIPE",
    "STANDING: UP TO THE CRAWL TUNNEL","CROUCH: THROUGH THE CRAWL TUNNEL","DONE"};
   const std::string stance=q.prone?"PRONE 0.58 M":q.crouched?"KNEELING 1.05 M":"STANDING 1.6 M";
   Image img=Image::of(f.pixels,f.width,f.height);drawText(img,24,70,what[std::min(ph,6)],0xffffff,3);drawText(img,24,110,stance,0xffd060,3);
   char name[32];std::snprintf(name,sizeof name,"%05d.png",n);savePng((dir/name).string(),img);}
  const auto& p=s.g().player();const int phase=int(s.board["phase"]);
  auto once=[&](const char* key,bool when,const char* label){if(when&&!s.board.count(key)){s.board[key]=1;s.shoot(label);}};
  once("shot.vent",phase==0&&s.frame==90,"her view: stopped standing at the wall vent");
  once("shot.duct",phase==1&&inRunDuct(p.pos,p.z-float(s.board["base"]))&&p.pos.y<15.f,"her view: prone in the duct");
  once("shot.corner",phase==1&&int(s.board["leg"])==1&&p.pos.x>6.2f,"her view: round the corner, the grate ahead");
  once("shot.grate",phase==1&&overGrate(p.pos)&&p.pos.x>7.4f,"her view: looking down through the grate");
  once("shot.drop",phase==1&&p.pos.x>12.6f&&p.z-s.board["base"]<.3,"her view: down the drop, the lower mouth ahead");
  once("shot.pipe",phase==3&&p.pos.y<17.5f,"her view: kneeling in the big pipe");
  once("shot.tunnel",phase==5&&p.pos.y<5.2f,"her view: prone in the crawl tunnel");
  if(!s.last())return;
  // The pieces from outside, from a fresh game of the same map (so the tube's own map is drawn last).
  const int level=levelNamed("Kits Gallery 4");if(level<0)return;
  auto look=[&](const char* label,Vec2 at,float z,float yawDeg,float pitch,unsigned layer){auto g=std::make_unique<Game>(starter());LabAccess::loadLevel(*g,level);
   LabAccess::setCamera(*g,at,z,yawDeg*kPi/180.f,pitch);debuglayer::visible=layer;s.renderer->render(*g);s.shoot(label);debuglayer::visible=0;};
  const float base=float(s.board["base"]);
  look("the vent run from outside",{1.8f,21.5f},base+3.4f,-42,-28,0);
  look("the vent run from the east",{15.5f,17.5f},base+2.6f,-150,-22,0);
  look("under the grate, looking up",{7.5f,14.6f},base+.35f,-90,70,0);
  look("the wall vent and its grille",{6.f,19.4f},base+1.9f,-90,-30,0);
  look("the drop's lower mouth",{15.4f,14.3f},base+.9f,-150,-14,0);
  look("the crawl tunnel",{18.5f,9.6f},base+1.3f,-90,-14,0);
  look("the big pipe",{18.5f,22.8f},base+1.4f,-90,-8,0);},
 .metrics=[](Scene& s,Report& r){auto b=[&](const char* k){return s.board.count(k)?s.board[k]:-1.;};
  r.metric("probes fitting as designed",b("probes.good"));r.metric("run seconds",b("run.s"));
  r.check("every piece: standing and kneeling do not fit, prone does (the pipe: kneeling fits, standing does not)",b("probes.good")==b("probes.count"),
   fixed(b("probes.good"),0)+" of "+fixed(b("probes.count"),0)+"; .build/crawl-kits-probes.txt");
  r.check("standing, she is stopped at the wall vent",b("vent.blocked")>16.95,"stopped at y "+fixed(b("vent.blocked")));
  r.check("crouched, she crawls the whole run down to the ground",b("run.end.x")>15.2&&std::fabs(b("run.end.z"))<.05,"out at x "+fixed(b("run.end.x"))+", z "+fixed(b("run.end.z")));
  r.check("in the run's ducts she is always prone",b("run.inside")>200&&std::fabs(b("duct.hull")-Player::ProneHeight)<1e-3,
   fixed(b("run.inside"),0)+" frames inside, tallest hull "+fixed(b("duct.hull")));
  r.check("the grate holds her",b("grate.crossed")>0&&b("grate.minz")>1.2,"lowest "+fixed(b("grate.minz")));
  r.check("standing, she is stopped at the big pipe",b("pipe.blocked")>18.9,"stopped at y "+fixed(b("pipe.blocked")));
  r.check("crouched, she gets through the big pipe kneeling",b("pipe.end")<14.5&&b("pipe.inside")>60&&b("pipe.prone")==0,
   "out at y "+fixed(b("pipe.end"))+", prone "+fixed(b("pipe.prone"),0)+" of "+fixed(b("pipe.inside"),0)+" frames");
  r.check("standing, she is stopped at the crawl tunnel",b("tunnel.blocked")>6.9,"stopped at y "+fixed(b("tunnel.blocked")));
  r.check("crouched, she crawls the tunnel prone",b("tunnel.end")<2.3&&b("tunnel.inside")>60&&b("tunnel.notprone")==0,
   "out at y "+fixed(b("tunnel.end"))+", not prone "+fixed(b("tunnel.notprone"),0)+" of "+fixed(b("tunnel.inside"),0)+" frames");},
 .teardown=[](Scene&){debuglayer::visible=0;}
});
}
}

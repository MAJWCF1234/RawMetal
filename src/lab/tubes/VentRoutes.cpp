// Creature routes by size: a run through a concrete culvert (bld_culvert, its collision from its own mesh, at any
// scale) that the player and small creatures fit through and big ones do not. The player crawls from the north room
// through the culvert into the south room; every creature is told where he is each frame (as a director's hint
// would), so what is measured is only how each one gets to him.
//   vent-crawl         a 2 m culvert (1.56 m inside, crouch): the Huntsman (1.05 m hull) follows him through it;
//                      the Brute (1.85 m) takes the long way round by the east gap and comes to him there.
//   vent-crawl-sealed  the same with the east gap walled up: the Brute goes to the culvert mouth he crawled into
//                      and waits there facing in, without wandering off or grinding into the opening.
//   vent-crawl-prone   a 1 m culvert (0.78 m inside, prone): a Huntsman at 0.55 scale (0.58 m) follows him through
//                      it; a full-size Huntsman and the Brute both go round.
// Fixture keys: `vent-x X` and `vent-width W` (the culvert's centre and width; its mouths at y 9.95 and 14.05),
// `runner-N KIND SCALE through|around X Y` (each creature and what it must do), `sealed 1` (no way round).
// The last frame is a view from straight above with each one's route drawn.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
struct Runner {CreatureKind kind=CreatureKind::Huntsman;float scale=1;bool through=false;Vec2 at{};std::string name;std::uint32_t rgb=0;};
std::vector<Runner> runners(const Fixture& f){
 std::vector<Runner> list;const std::uint32_t colours[]={0xff3030,0xffa020,0xff40ff,0x40c0ff};
 for(int i=1;i<=4;++i){const auto t=f.value("runner-"+std::to_string(i));if(t.empty())continue;std::istringstream in(t);std::string kind,what;Runner r;
  in>>kind>>r.scale>>what>>r.at.x>>r.at.y;static const char* kinds[]={"Huntsman","Wasp","Brute","Warden","Mutant","Snatcher","Chimera","Transformed","Machine"};
  for(int k=0;k<CreatureKindCount;++k)if(kind==kinds[k])r.kind=CreatureKind(k);r.through=what=="through";r.rgb=colours[i-1];
  for(auto& c:kind)c=char(std::toupper(unsigned(c)));r.name=kind+" "+fixed(creatureTraits(r.kind).hull*r.scale)+" M";list.push_back(r);}
 return list;}
Enemy* runnerBody(Game& g,const Runner& r){for(auto& e:LabAccess::enemies(g))if(e.kind==r.kind&&std::fabs(e.scale-r.scale)<.001f)return &e;return nullptr;}
float ventX(const Scene& s){return s.fixture->number("vent-x",5.f);}
float ventHalf(const Scene& s){return s.fixture->number("vent-width",2.f)*.5f;}
bool sealed(const Scene& s){return s.fixture->number("sealed",0)>0;}
bool inCulvert(const Scene& s,Vec2 p){return std::fabs(p.x-ventX(s))<ventHalf(s)+.1f&&p.y>10.3f&&p.y<13.7f;}
bool inBypass(Vec2 p){return p.x>11.8f&&p.y>10.f&&p.y<14.f;}
void trail(Scene& s,const std::string& key,Vec2 p,float z){auto& t=s.points[key];if(t.empty()||std::hypot(t.back().x-p.x,t.back().y-p.y)>.12f)t.push_back({p.x,p.y,z});}
const bool vents=add({
 .name="vent-routes",.summary="creatures route by their own size: small ones follow the player through a culvert, big ones go round, or wait at the mouth when there is no way round",
 .fixtures={"vent-crawl","vent-crawl-sealed","vent-crawl-prone"},
 .author=[](Scene& s,AuthoredMapData& m){for(const auto& r:runners(*s.fixture)){CreatureSpawn c{r.kind,r.at,0};c.scale=r.scale;m.creatureSpawns.push_back(c);}},
 .setup=[](Scene& s){Game& g=s.g();LabAccess::playerCamera(g);LabAccess::setGod(g,true);
  Player& p=LabAccess::player(g);p.pos={ventX(s),6.f};p.angle=1.5707963f;p.pitch=0;s.board["route.leg"]=0;
  for(size_t i=0;i<runners(*s.fixture).size();++i){const std::string k="r"+std::to_string(i);s.board[k+".culvert"]=0;s.board[k+".bypass"]=0;}},
 .step=[](Scene& s,float){
  Game& g=s.g();Player& p=LabAccess::player(g);
  // Two seconds to settle, then the crawl: to the mouth, through (crouched; the game drops him prone where only that
  // fits), then up and on to the south room's middle.
  const Vec2 route[]={{ventX(s),8.f},{ventX(s),16.5f},{8.5f,19.f}};
  int leg=int(s.board["route.leg"]);
  if(s.time>2.f&&leg<3){const Vec2 to=route[leg]-p.pos;
   if(std::hypot(to.x,to.y)<.3f)s.board["route.leg"]=++leg;
   else{p.angle=std::atan2(to.y,to.x);s.input.forward=true;}}
  s.input.crouch=p.pos.y>8.4f&&p.pos.y<15.6f;
  if(p.pos.y>14.1f&&!s.board.count("player.through_s"))s.board["player.through_s"]=s.time;
  // The hint: each creature knows where the player is.
  for(auto& e:LabAccess::enemies(g))if(e.alive){e.awareness=std::max(e.awareness,6.f);e.recognition=1;e.lastKnown=p.pos;e.lastKnownZ=p.z;
   if(e.state==Enemy::State::Idle||e.state==Enemy::State::Search)e.state=Enemy::State::Investigate;}},
 .capture=[](Scene& s,const Frame&){
  Game& g=s.g();Player& p=LabAccess::player(g);trail(s,"route.player",p.pos,p.z);
  const auto list=runners(*s.fixture);const Vec2 mouth{ventX(s),9.95f};
  for(size_t i=0;i<list.size();++i){const Enemy* e=runnerBody(g,list[i]);if(!e)continue;const std::string k="r"+std::to_string(i);
   trail(s,"route."+k,e->pos,e->z);
   if(inCulvert(s,e->pos))s.board[k+".culvert"]+=1;
   if(inBypass(e->pos))s.board[k+".bypass"]+=1;
   if(e->pos.y>14.1f&&!s.board.count(k+".south_s"))s.board[k+".south_s"]=s.time;
   s.board[k+".gap"]=std::hypot(e->pos.x-p.pos.x,e->pos.y-p.pos.y);
   s.board[k+".mouth"]=std::hypot(e->pos.x-mouth.x,e->pos.y-mouth.y);
   // How far it moved over its last two seconds, and whether it faces the player's way.
   auto& last=s.points[k+".recent"];last.push_back({e->pos.x,e->pos.y,0});if(last.size()>120)last.erase(last.begin());
   float spread=0;for(const auto& q:last)spread=std::max(spread,std::hypot(q.x-e->pos.x,q.y-e->pos.y));s.board[k+".settle"]=spread;
   const Vec2 face{std::cos(e->heading),std::sin(e->heading)},in=normalized(p.pos-e->pos);s.board[k+".facing"]=face.x*in.x+face.y*in.y;}
  // The last frame: a view from straight above the map (over its ceiling, north up, east right), for the routes;
  // the reporter outlines the walls, since the rooms themselves are not drawn from up there.
  if(s.last()){p.pos={7.f,12.f};p.z=24.f-p.stanceEye();p.angle=-1.5707963f;p.pitch=-218.f;s.renderer->render(g);}},
 .metrics=[](Scene& s,Report& r){
  auto b=[&](const std::string& k){return s.board.count(k)?s.board[k]:-1.;};
  r.metric("player_through_s",b("player.through_s"),"s");
  r.check("the player crawls through the culvert",b("player.through_s")>0,"through at "+fixed(float(b("player.through_s")))+" s");
  const auto list=runners(*s.fixture);
  for(size_t i=0;i<list.size();++i){const auto& run=list[i];const std::string k="r"+std::to_string(i),n=run.name;
   for(const char* m:{"culvert","bypass","south_s","gap","mouth","settle","facing"})r.metric(k+"_"+m,b(k+"."+m));
   if(run.through){
    r.check(n+" follows him through the culvert",b(k+".culvert")>0&&b(k+".south_s")>0,fixed(float(b(k+".culvert")),0)+" frames inside, south at "+fixed(float(b(k+".south_s")))+" s");
    r.check(n+" never by the long way",b(k+".bypass")<=0,fixed(float(b(k+".bypass")),0)+" frames in the east gap");
    r.check(n+" ends at the player",b(k+".gap")<2.5,fixed(float(b(k+".gap")))+" m off");
   }else{
    r.check(n+" never squeezes into the culvert",b(k+".culvert")<=0,fixed(float(b(k+".culvert")),0)+" frames inside");
    if(!sealed(s)){
     r.check(n+" takes the long way round",b(k+".bypass")>0&&b(k+".south_s")>0,fixed(float(b(k+".bypass")),0)+" frames in the east gap, south at "+fixed(float(b(k+".south_s")))+" s");
     r.check(n+" comes to the player",b(k+".gap")<2.5,fixed(float(b(k+".gap")))+" m off");
    }else{
     r.check(n+", with no way round, waits at the mouth the player went in by",b(k+".mouth")<2.f,fixed(float(b(k+".mouth")))+" m from the north mouth");
     r.check(n+" holds there (no wandering in its last two seconds)",b(k+".settle")<.35,"moved within "+fixed(float(b(k+".settle")))+" m");
     r.check(n+" faces the player's way",b(k+".facing")>.5,"facing dot "+fixed(float(b(k+".facing"))));
    }}}},
 .reporter=[](Scene& s,Overlay& o){
  {const World& w=s.g().world();   // the walls' outline: every edge between a solid cell and an open one
   for(int y=0;y<24;++y)for(int x=0;x<24;++x){if(!w.solid(x+.5f,y+.5f))continue;
    if(x<23&&!w.solid(x+1.5f,y+.5f))o.line({x+1.f,float(y),0},{x+1.f,y+1.f,0},0xa0a0a0);
    if(x>0&&!w.solid(x-.5f,y+.5f))o.line({float(x),float(y),0},{float(x),y+1.f,0},0xa0a0a0);
    if(y<23&&!w.solid(x+.5f,y+1.5f))o.line({float(x),y+1.f,0},{x+1.f,y+1.f,0},0xa0a0a0);
    if(y>0&&!w.solid(x+.5f,y-.5f))o.line({float(x),float(y),0},{x+1.f,float(y),0},0xa0a0a0);}}
  auto draw=[&](const std::string& key,std::uint32_t rgb,const std::string& name){const auto& t=s.points[key];
   for(size_t i=1;i<t.size();++i)o.line({t[i-1].x,t[i-1].y,t[i-1].z+.05f},{t[i].x,t[i].y,t[i].z+.05f},rgb);
   if(!t.empty()){o.point({t.back().x,t.back().y,t.back().z+.05f},rgb,5);o.label({t.back().x+.2f,t.back().y,t.back().z+.05f},name,rgb);
    o.cross({t.front().x,t.front().y,t.front().z+.05f},.25f,rgb);}};
  draw("route.player",0x40ff60,"PLAYER");
  const auto list=runners(*s.fixture);for(size_t i=0;i<list.size();++i)draw("route.r"+std::to_string(i),list[i].rgb,list[i].name);
  const float x=ventX(s),h=ventHalf(s);o.box({x-h,9.95f,0},{x+h,14.05f,2*h},0x60c0ff);
  o.label({x+h+.2f,12.f,0},"CULVERT "+fixed(.78f*2*h)+" M INSIDE",0x60c0ff);
  o.text(6,6,sealed(s)?"NO WAY ROUND: EAST GAP WALLED UP":"LONG WAY ROUND: EAST GAP (RIGHT)",0xffffff);}});
}
}

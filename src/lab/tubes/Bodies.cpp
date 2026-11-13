// What a death leaves: the human ragdoll (Carcass), blood decals per fluid, and
// R-117 (the Chimera) that kills a man with a bite and feeds on him. Three
// features that are friends: in one tube the Chimera makes the body, the body
// the pool, and each must still pass its own checks with the others in the room.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/EngineRegressionScene.h"
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fixed(float v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,double(v));return b;}
// How a body lies (as --each-test=human-ragdoll measures it): its lowest point over the floor under it,
// the deepest it goes into that floor, how many points are under it, its top.
struct Lie {float lowest=1e9f,deepest=1e9f,top=-1e9f;int under=0,count=0;bool finite=true;Point3 low{},lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};};
Lie lie(Carcass& c,const World& w){Lie l;for(const auto& t:c.triangles())for(const auto& v:t.v){
  if(!std::isfinite(v.p.x+v.p.y+v.p.z)){l.finite=false;continue;}
  const float own=w.supportBelow(v.p.x,v.p.y,v.p.z+.06f);float floor=own;
  for(auto [dx,dy]:{std::pair{.05f,0.f},{-.05f,0.f},{0.f,.05f},{0.f,-.05f}})floor=std::min(floor,w.supportBelow(v.p.x+dx,v.p.y+dy,v.p.z+.06f));
  const float gap=v.p.z-floor;if(gap<l.lowest){l.lowest=gap;l.low=v.p;}l.deepest=std::min(l.deepest,v.p.z-own);l.under+=gap<-.01f;++l.count;l.top=std::max(l.top,v.p.z);
  l.lo={std::min(l.lo.x,v.p.x),std::min(l.lo.y,v.p.y),std::min(l.lo.z,v.p.z)};l.hi={std::max(l.hi.x,v.p.x),std::max(l.hi.y,v.p.y),std::max(l.hi.z,v.p.z)};}
 return l;}
std::vector<size_t> deadMen(const Game& g){std::vector<size_t> out;for(size_t i=0;i<g.npcActors().size();++i)if(g.npcDeath(i).dead)out.push_back(i);return out;}
// The species whose fluid each blood mark shows (SoftwareRenderer::creatureVisuals): goo, ichor, blood, oil.
constexpr int FluidSpecies[]={int(CreatureKind::Huntsman),int(CreatureKind::Brute),int(CreatureKind::Mutant),int(CreatureKind::Machine)};
constexpr const char* FluidNames[]={"goo","ichor","blood","oil"};
constexpr std::uint32_t FluidColours[]={0x80ff40,0xc060ff,0xff3030,0xffc040};
const StateId ChimeraWake=stateId("lab_chimera_wake"),ChimeraHunter=stateId("lab_chimera_hunter");
Vec2 manAt(const AuthoredMapData& m){return m.actorTracks.empty()?Vec2{12,4.5f}:m.actorTracks[0].keys[0].position;}

// ------------------------------------------------------------------ ragdoll
const bool ragdoll=add({
 .name="ragdoll",.summary="a man killed beside a step falls, folds over its edge and comes to rest on the floor (Carcass, humanCorpse)",
 .friends={"blood","chimera"},.fixtures={"feed-room"},
 .author=[](Scene& s,AuthoredMapData& m){
  if(m.actorTracks.empty()){auto man=regression::guard({12,4.5f},{12,4.5f},AiFaction::Raider);m.actorTracks.push_back(man);}
  // The edge: a step just east of him, as high as the fixture says (tune ragdoll.edge to try others).
  const Vec2 at=manAt(m);const float h=tuned("ragdoll.edge",s.fixture->number("edge-height",.45f));
  if(h>0)m.structures.push_back({at.x+.35f,at.y-1.4f,at.x+2.4f,at.y+1.4f,0.f,h});},
 .step=[](Scene& s,float){
  if(s.has("chimera"))return; // the Chimera kills him in that tube
  if(s.frame==int(s.fixture->number("kill-frame",10)))for(size_t i=0;i<s.g().npcActors().size();++i){const auto& n=s.g().npcActors()[i];LabAccess::killNpc(s.g(),i,{n.pose.position.x-3.f,n.pose.position.y});}},
 .capture=[](Scene& s,const Frame&){
  Game& g=s.g();std::vector<Point3> bodies;
  for(size_t i:deadMen(g)){auto* c=LabAccess::humanCorpse(*s.renderer,g,int(i));if(!c)continue;const Lie l=lie(c->carcass,g.world());bodies.push_back(c->carcass.centre());
   const std::string k="ragdoll.fell_s."+std::to_string(i);if(!s.board.count(k)&&l.top-g.npcActors()[i].pose.z<.6f)s.board[k]=s.time;}
  s.points["bodies"]=bodies;},
 .metrics=[](Scene& s,Report& r){
  Game& g=s.g();const auto dead=deadMen(g);r.metric("bodies",double(dead.size()));
  r.check("a man dies in the tube",!dead.empty(),std::to_string(dead.size())+" dead");
  int asleep=0,down=0,whole=0;float worst=0;
  for(size_t i:dead){auto* c=LabAccess::humanCorpse(*s.renderer,g,int(i));if(!c)continue;auto& k=c->carcass;const Lie l=lie(k,g.world());const std::string n=std::to_string(i);
   r.metric("lowest_m."+n,l.lowest,"m");r.metric("deepest_m."+n,l.deepest,"m");r.metric("under."+n,l.under);r.metric("top_m."+n,l.top-g.npcActors()[i].pose.z,"m");r.metric("asleep."+n,k.asleep());
   r.metric("sections."+n,k.sections());r.metric("pieces."+n,k.pieces());r.metric("age_s."+n,k.age(),"s");
   if(s.board.count("ragdoll.fell_s."+n))r.metric("fell_s."+n,s.board["ragdoll.fell_s."+n],"s");
   asleep+=k.asleep();down+=l.finite&&l.lowest<=.06f&&l.lowest>=-.01f;whole+=l.finite&&l.under==0;worst=std::max(worst,std::fabs(l.lowest));}
  const int n=int(dead.size());
  r.check("every body comes to rest",asleep==n,std::to_string(asleep)+" of "+std::to_string(n)+" asleep");
  r.check("each lies with its lowest point on the floor under it (6 cm)",down==n,"worst "+fixed(worst)+" m");
  r.check("nothing of any body goes through the floor",whole==n,std::to_string(whole)+" of "+std::to_string(n)+" clean");},
 .reporter=[](Scene& s,Overlay& o){
  Game& g=s.g();for(size_t i:deadMen(g)){auto* c=LabAccess::humanCorpse(*s.renderer,g,int(i));if(!c)continue;auto& k=c->carcass;const Lie l=lie(k,g.world());
   const std::uint32_t col=k.asleep()?0x40ff60:0xffa020;o.box(l.lo,l.hi,col);o.cross(k.centre(),.12f,col);o.point(l.low,0xff2020,3);
   o.label({l.hi.x,l.hi.y,l.hi.z+.05f},std::string(k.asleep()?"ASLEEP":"AWAKE")+" "+std::to_string(k.sections())+" SECT "+std::to_string(k.joints())+" JNT LOW "+fixed(l.lowest,2),col);}},
 .with={{"blood",[](Scene& s,Report& r){
  // The pool under a body lies 3 cm over the floor (Scene3D): a body whose underside sinks below that
  // plane inside the pool has the pool painted over it. Count the body points under the pool plane.
  Game& g=s.g();int under=0,total=0;
  for(size_t i:deadMen(g)){auto* c=LabAccess::humanCorpse(*s.renderer,g,int(i));if(!c)continue;const Point3 ctr=c->carcass.centre();const float rad=std::max(.35f,c->carcass.radius()*.6f);
   for(const auto& t:c->carcass.triangles())for(const auto& v:t.v){if(std::hypot(v.p.x-ctr.x,v.p.y-ctr.y)>rad)continue;++total;under+=v.p.z<g.world().supportBelow(v.p.x,v.p.y,v.p.z+.3f)+.03f;}}
  const double share=total?double(under)/total:0;r.metric("under_pool_share",share);
  r.check("the pool lies under the body, not over it",share<.15,fixed(float(share*100),1)+"% of the body near its centre is below the pool's plane");}}}});

// ------------------------------------------------------------------ chimera
const bool chimera=add({
 .name="chimera",.summary="R-117 hunts the man it was set on, kills him with a bite that takes his head, and feeds (Hunter.cpp)",
 .friends={"ragdoll","blood","doors","chimera-shot"},.fixtures={"feed-room"},
 .author=[](Scene& s,AuthoredMapData& m){
  if(m.actorTracks.empty()){auto man=regression::guard({12,4.5f},{12,4.5f},AiFaction::Raider);m.actorTracks.push_back(man);}
  const auto at=s.fixture->value("chimera-at","4 4");float x=4,y=4;std::sscanf(at.c_str(),"%f %f",&x,&y);
  CreatureSpawn c{CreatureKind::Chimera,{x,y},0};c.wakeState=ChimeraWake;c.hunter=ChimeraHunter;c.prey=0;m.creatureSpawns.push_back(c);},
 .setup=[](Scene& s){s.g().setState(ChimeraWake,1);LabAccess::setGod(s.g(),true);},
 .capture=[](Scene& s,const Frame&){
  Game& g=s.g();if(!s.board.count("chimera.kill_s")&&!g.npcActors().empty()&&g.npcDeath(0).dead)s.board["chimera.kill_s"]=s.time;
  for(const auto& e:g.enemies())if(e.kind==CreatureKind::Chimera&&e.feeding>0)s.board["chimera.fed"]=1;},
 .metrics=[](Scene& s,Report& r){
  Game& g=s.g();const bool dead=!g.npcActors().empty()&&g.npcDeath(0).dead,headless=dead&&g.npcHeadless(0);
  const Enemy* beast=nullptr;for(const auto& e:g.enemies())if(e.kind==CreatureKind::Chimera)beast=&e;
  r.metric("kill_s",s.board.count("chimera.kill_s")?s.board["chimera.kill_s"]:-1,"s");r.metric("headless",headless);r.metric("fed",s.board.count("chimera.fed")?1:0);
  r.check("the Chimera is in the room",beast!=nullptr);
  r.check("it kills the man it hunts",dead,dead?"dead at "+fixed(float(s.board["chimera.kill_s"]),2)+" s":"alive after "+fixed(s.time,1)+" s");
  r.check("the bite takes his head",headless);
  r.check("it feeds on him",s.board.count("chimera.fed")>0);},
 .reporter=[](Scene& s,Overlay& o){
  Game& g=s.g();for(const auto& e:g.enemies()){if(e.kind!=CreatureKind::Chimera)continue;const Point3 p{e.pos.x,e.pos.y,e.z+.6f};
   o.point(p,0xff40ff,4);o.line(p,{p.x+std::cos(e.heading)*.8f,p.y+std::sin(e.heading)*.8f,p.z},0xff40ff);
   if(!g.npcActors().empty()){const auto& n=g.npcActors()[0].pose;o.line(p,{n.position.x,n.position.y,n.z+.9f},0x804080);}
   o.label({p.x,p.y,p.z+.5f},e.feeding>0?"FEEDING "+fixed(e.feeding,1)+"S":e.alive?"HUNTING":"DEAD",0xff80ff);}},
 .with={{"ragdoll",[](Scene& s,Report& r){
  // The handoff: the man it killed becomes a body that lies down, and it feeds at that body.
  Game& g=s.g();if(g.npcActors().empty()||!g.npcDeath(0).dead){r.check("its kill becomes a body",false,"no kill");return;}
  auto* c=LabAccess::humanCorpse(*s.renderer,g,0);const Enemy* beast=nullptr;for(const auto& e:g.enemies())if(e.kind==CreatureKind::Chimera)beast=&e;
  const float gap=c&&beast?std::hypot(beast->pos.x-c->carcass.centre().x,beast->pos.y-c->carcass.centre().y):99.f;r.metric("feed_gap_m",gap,"m");
  r.check("its kill becomes a body that lies down",c&&c->carcass.built());
  r.check("it feeds at the body, not where the man stood",gap<2.2f,fixed(gap,2)+" m from the body's centre");}}}});

// ------------------------------------------------------------------ blood
const bool blood=add({
 .name="blood",.summary="blood decals for each fluid (goo, ichor, blood, oil): splats and pools on the floor, tinted per creature (Scene3D marks, m_bloodDecals)",
 .friends={"ragdoll","chimera"},.fixtures={"blood-floor"},
 .setup=[](Scene& s){
  // blood-row off: no row of fluid marks (a tube where only the pools under bodies matter).
  s.board["blood.row"]=s.fixture->value("blood-row","on")=="off"?0:4;if(s.board["blood.row"]==0)return;
  Game& g=s.g();const Player& p=g.player();const float d=s.fixture->number("blood-distance",2.2f),spread=tuned("blood.spread",.75f);
  const Vec2 fwd{std::cos(p.angle),std::sin(p.angle)},side{-std::sin(p.angle),std::cos(p.angle)};std::vector<Point3> marks;
  for(int f=0;f<4;++f){const Vec2 at=p.pos+fwd*d+side*((float(f)-1.5f)*spread);const float z=g.world().floorHeight(at.x,at.y);
   BulletImpact b{};b.pos=at;b.z=z;b.time=g.elapsed();b.level=g.level();b.blood=true;b.tint=FluidSpecies[f];b.size=f%2?.42f:.3f;LabAccess::impacts(g).push_back(b);marks.push_back({at.x,at.y,z});}
  s.points["blood"]=marks;},
 .step=[](Scene& s,float){
  // With a body in the tube: when it has come to rest, a pool under it (as a shot man bleeds).
  if(!s.has("ragdoll")||s.board.count("blood.pooled")||!s.points.count("bodies"))return;
  Game& g=s.g();bool rest=!deadMen(g).empty();for(size_t i:deadMen(g)){auto* c=LabAccess::humanCorpse(*s.renderer,g,int(i));rest&=c&&c->carcass.asleep();}
  if(!rest)return;s.board["blood.pooled"]=s.time;
  for(const auto& b:s.points["bodies"]){BulletImpact m{};m.pos={b.x,b.y};m.z=g.world().supportBelow(b.x,b.y,b.z+.3f);m.time=g.elapsed();m.level=g.level();m.blood=true;m.tint=int(CreatureKind::Mutant);m.size=.5f;LabAccess::impacts(g).push_back(m);s.points["blood"].push_back({b.x,b.y,m.z});}},
 .capture=[](Scene& s,const Frame& f){
  if(!s.last())return;Game& g=s.g();auto& impacts=LabAccess::impacts(g);const auto kept=impacts;
  const std::vector<std::uint32_t> with(f.pixels,f.pixels+size_t(f.width)*f.height);
  std::erase_if(impacts,[](const BulletImpact& b){return b.blood;});s.renderer->render(g);s.shoot("without the blood");
  const auto* without=s.renderer->pixels();const auto& pts=s.points["blood"];
  for(size_t i=0;i<pts.size()&&i<size_t(s.board["blood.row"]);++i){const auto p=project(s,pts[i]);if(!p.visible)continue;
   const float half=i%2?.42f:.3f;const auto edge=project(s,{pts[i].x+half,pts[i].y,pts[i].z});const float rad=std::max(2.f,edge.visible?std::hypot(edge.x-p.x,edge.y-p.y)*.6f:3.f);
   double diff=0,c[3]{};int n=0;
   for(int y=int(p.y-rad);y<=int(p.y+rad);++y)for(int x=int(p.x-rad);x<=int(p.x+rad);++x){if(x<0||y<0||x>=f.width||y>=f.height||std::hypot(x-p.x,y-p.y)>rad)continue;const auto a=with[size_t(y)*f.width+x],b=without[size_t(y)*f.width+x];
    for(int k=0;k<3;++k){const int sh=16-8*k;diff+=std::abs(int((a>>sh)&255)-int((b>>sh)&255));c[k]+=(a>>sh)&255;}++n;}
   const std::string k=std::string("blood.seen.")+FluidNames[i];s.board[k]=n?diff/(3.0*n):0;s.board[k+".r"]=n?c[0]/n:0;s.board[k+".g"]=n?c[1]/n:0;s.board[k+".b"]=n?c[2]/n:0;}
  impacts=kept;s.renderer->render(g);},
 .metrics=[](Scene& s,Report& r){
  Game& g=s.g();int sets=0;for(int f=0;f<int(SoftwareRenderer::Fluid::Count);++f)for(int j=0;j<int(SoftwareRenderer::BloodJob::Count);++j)sets+=LabAccess::bloodDecalCount(*s.renderer,SoftwareRenderer::Fluid(f),SoftwareRenderer::BloodJob(j))>0;
  r.metric("decal_sets",sets);r.check("every fluid has every decal job loaded",sets==int(SoftwareRenderer::Fluid::Count)*int(SoftwareRenderer::BloodJob::Count),std::to_string(sets)+" sets");
  int marks=0;for(const auto& b:LabAccess::impacts(g))marks+=b.blood;r.metric("marks",marks);
  // Seen: inside each mark, the frame against the same moment drawn without any blood (capture keeps both).
  const auto& pts=s.points["blood"];int seen=0;std::vector<std::array<double,3>> tones;
  for(size_t i=0;i<pts.size()&&i<size_t(s.board["blood.row"]);++i){const std::string k=std::string("blood.seen.")+FluidNames[i];const double contrast=s.board.count(k)?s.board[k]:0;
   r.metric(std::string("contrast.")+FluidNames[i],contrast);seen+=contrast>4;
   tones.push_back({s.board[k+".r"],s.board[k+".g"],s.board[k+".b"]});}
  r.check("each fluid's mark shows on the floor (against the same frame without blood)",seen>=int(std::min<size_t>(size_t(s.board["blood.row"]),pts.size())),std::to_string(seen)+" of 4 change their spot by more than 4 levels");
  double closest=1e9;for(size_t a=0;a<tones.size();++a)for(size_t b=a+1;b<tones.size();++b){double d=0;for(int c=0;c<3;++c)d+=std::fabs(tones[a][c]-tones[b][c]);closest=std::min(closest,d/3);}
  if(tones.size()>1)r.metric("closest_tones",closest);if(tones.size()>1)r.check("no two fluids look alike",closest>3,"closest pair differs by "+fixed(float(closest),1)+" levels");},
 .reporter=[](Scene& s,Overlay& o){
  const auto& pts=s.points["blood"];for(size_t i=0;i<pts.size();++i){const float h=i<4?(i%2?.42f:.3f):.5f;const std::uint32_t col=i<4?FluidColours[i]:0xff3030;const Point3 c=pts[i];
   o.line({c.x-h,c.y-h,c.z+.03f},{c.x+h,c.y-h,c.z+.03f},col);o.line({c.x+h,c.y-h,c.z+.03f},{c.x+h,c.y+h,c.z+.03f},col);o.line({c.x+h,c.y+h,c.z+.03f},{c.x-h,c.y+h,c.z+.03f},col);o.line({c.x-h,c.y+h,c.z+.03f},{c.x-h,c.y-h,c.z+.03f},col);
   o.label({c.x,c.y,c.z+.1f},i<4?FluidNames[i]:"POOL",col);}},
 .with={{"chimera",[](Scene& s,Report& r){
  // A headless man bleeds a pool from the neck (Scene3D): it must be drawn on the floor under him.
  Game& g=s.g();const bool headless=!g.npcActors().empty()&&g.npcDeath(0).dead&&g.npcHeadless(0);
  r.metric("neck_pool",headless);r.check("the bite leaves the neck pool to draw",headless||g.npcActors().empty(),headless?"headless":"not headless");}}}});
}
}

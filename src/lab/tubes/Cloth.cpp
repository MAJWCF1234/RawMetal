// Curtains, straps and cables (CLOTH records, game/Cloth.cpp).
//  cloth (bare rooms; cloth.txt says what each did):
//   a curtain hangs from its rail to its drop and sleeps; walked through it parts and sways, then settles back where it
//     hung and sleeps again;
//   a cable strung over a crate drapes over it (its middle rests on the crate's top, where without the crate it sags lower);
//   a strap hangs straight down; a round through it knocks it swinging, and it settles;
//   the CLOTH record goes out and back in (the exporter and the reader keep its fields);
//   the cost: ms a frame awake and asleep.
//  cloth-room (the Starter Map's SHOWCASE: PHYSICS, fixture cloth-room): its curtain, strap and cable hang, settle and sleep;
//   the player walks through the curtain; pictures of each.
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "game/Cloth.h"
#include "game/EngineRegressionScene.h"
#include "world/CampaignKit.h"
#include "world/CampaignExport.h"
#include "world/CustomCampaign.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <string>
namespace retro::lab {
namespace {
std::string f3(double v,int d=3){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
using Check=std::function<void(bool,const std::string&)>;
float spanOf(Point3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
struct Beads {std::vector<Point3> p;int cols=0,rows=0;float bead=0;};
Beads beads(const Game& g,int i){Beads b;g.clothBeads(i,b.p,b.cols,b.rows,b.bead);return b;}
bool asleep(const Game& g,int i){const auto* r=g.clothRun(i);return r&&r->asleep();}
void run(Game& g,float seconds,float hz=60){for(int i=0;i<int(seconds*hz);++i)g.update({},1.f/hz);}

void curtain(std::ostream& out,const Check& check){
 auto m=regression::room();m->definition.playerStart={3,3};ClothSpec c;c.kind=ClothKind::Curtain;c.at={5,8,2.4f};c.to={6.4f,8,2.4f};c.drop=1.8f;m->cloths.push_back(c);
 Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};g.update({},1.f/60);   // (built on its first frame)
 check(g.clothCount()==1,"curtain: the CLOTH record plays as one cloth");if(g.clothCount()!=1)return;
 float t=0;while(t<4&&!asleep(g,0)){run(g,1.f/60);t+=1.f/60;}
 const Beads rest=beads(g,0);float bottom=1e9f,lean=0;for(int col=0;col<rest.cols;++col){const Point3 top=rest.p[size_t(col)],foot=rest.p[size_t((rest.rows-1)*rest.cols+col)];bottom=std::min(bottom,foot.z);lean=std::max(lean,std::hypot(foot.x-top.x,foot.y-top.y));}
 out<<"a curtain 1.4 m wide, 1.8 m drop ("<<rest.cols<<" x "<<rest.rows<<" beads): asleep after "<<f3(t,2)<<" s, its foot "<<f3(bottom,3)<<" m up (rail 2.4), each column off plumb by "<<f3(lean*1000,1)<<" mm at most\n";
 check(asleep(g,0)&&std::fabs(bottom-.6f)<.08f&&lean<.03f,"curtain: it hangs from its rail to its drop, plumb, and sleeps");
 // Walked through at 1.5 m/s, across its middle.
 float parted=0,fastest=0;LabAccess::player(g).pos={5.7f,6.5f};LabAccess::player(g).z=0;run(g,.2f);
 for(int i=0;i<120;++i){LabAccess::player(g).pos.y+=1.5f/60;g.update({},1.f/60);const Beads b=beads(g,0);
  for(size_t k=0;k<b.p.size();++k){parted=std::max(parted,std::hypot(b.p[k].x-rest.p[k].x,b.p[k].y-rest.p[k].y));}
  if(const auto* r=g.clothRun(0))for(int k=0;k<r->beads;++k){const auto& v=r->space.bodies[size_t(k)].v;fastest=std::max(fastest,std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z));}}
 const bool woke=!asleep(g,0);
 out<<"walked through at 1.5 m/s: the cloth parted by "<<f3(parted,3)<<" m, beads up to "<<f3(fastest,2)<<" m/s, "<<(woke?"awake":"asleep")<<" as she leaves\n";
 check(parted>.15f&&fastest>.3f,"curtain: walked through, it parts and sways");
 LabAccess::player(g).pos={3,3};t=0;while(t<8&&!asleep(g,0)){run(g,1.f/60);t+=1.f/60;}
 const Beads after=beads(g,0);float drift=0;for(size_t k=0;k<after.p.size();++k)drift=std::max(drift,spanOf(after.p[k]-rest.p[k]));
 out<<"and settles: asleep "<<f3(t,2)<<" s after she left, every bead within "<<f3(drift*1000,1)<<" mm of where it hung\n";
 check(asleep(g,0)&&drift<.05f,"curtain: it settles back where it hung and sleeps");}

void cable(std::ostream& out,const Check& check){
 // A cable 2.6 m long strung 2 m between two walls' fixings 1.1 m up, once over a crate 0.6 m tall under its middle, once bare.
 float middle[2]{},lowest[2]{};bool slept[2]{};float crateTop=0;
 for(int withCrate=0;withCrate<2;++withCrate){auto m=regression::room();m->definition.playerStart={3,3};ClothSpec c;c.kind=ClothKind::Cable;c.at={8,6,1.1f};c.to={8,8,1.1f};c.length=2.6f;c.radius=.015f;m->cloths.push_back(c);
  if(withCrate)kit::prop(*m,model::wooden_crate,8,7,0,.6f,.6f,.6f,0);
  Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};for(auto& p:LabAccess::clutter(g))crateTop=std::max(crateTop,p.z+p.height());
  float t=0;while(t<6&&!asleep(g,0)){run(g,1.f/60);t+=1.f/60;}slept[withCrate]=asleep(g,0);
  const Beads b=beads(g,0);middle[withCrate]=b.p[size_t(b.cols/2)].z;lowest[withCrate]=1e9f;for(const auto& p:b.p)lowest[withCrate]=std::min(lowest[withCrate],p.z);}
 out<<"a cable 2.6 m long over 2 m: bare its middle sags to "<<f3(middle[0],3)<<" m; over a crate whose top is "<<f3(crateTop,3)<<" m up it drapes over it, its middle at "<<f3(middle[1],3)<<" m (lowest bead "<<f3(lowest[1],3)<<"), "<<(slept[1]?"asleep":"awake")<<"\n";
 check(slept[0]&&slept[1]&&middle[0]<crateTop-.1f&&std::fabs(middle[1]-(crateTop+.015f))<.03f,"cable: it drapes over a crate (its middle on the crate's top, where bare it sags below it), and sleeps");}

void strap(std::ostream& out,const Check& check){
 auto m=regression::room();m->definition.playerStart={3,3};ClothSpec c;c.kind=ClothKind::Strap;c.at={10,6,2.2f};c.length=1.2f;m->cloths.push_back(c);
 Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};float t=0;while(t<4&&!asleep(g,0)){run(g,1.f/60);t+=1.f/60;}
 Beads b=beads(g,0);const Point3 foot=b.p.back();
 out<<"a strap 1.2 m long hung at 2.2 m: its end at "<<f3(foot.z,3)<<" m, "<<f3(std::hypot(foot.x-10,foot.y-6)*1000,1)<<" mm off plumb, "<<(asleep(g,0)?"asleep":"awake")<<"\n";
 check(asleep(g,0)&&std::fabs(foot.z-1.f)<.06f&&std::hypot(foot.x-10,foot.y-6)<.02f,"strap: it hangs straight down its length and sleeps");
 g.shootCloth({8,6,1.5f},{12,6,1.5f});float swing=0;for(int i=0;i<60;++i){run(g,1.f/60);b=beads(g,0);swing=std::max(swing,std::hypot(b.p.back().x-10,b.p.back().y-6));}
 t=0;while(t<10&&!asleep(g,0)){run(g,1.f/60);t+=1.f/60;}
 out<<"shot through 0.7 m up: its end swung "<<f3(swing,3)<<" m out, asleep again "<<f3(t,2)<<" s later\n";
 check(swing>.05f&&asleep(g,0),"strap: a round through it knocks it swinging, and it settles");}

void record(std::ostream& out,const Check& check){
 // The record out and back in: written by the exporter, read by the campaign reader, its fields kept.
 auto m=regression::room();ClothSpec c;c.kind=ClothKind::Cable;c.at={1,2,3};c.to={4,5,3.5f};m->cloths.push_back(c);
 m->recordFields[recordFieldKey("CLOTH","",0)]={{"length","4.5"},{"loose","1"}};
 std::string text;bool ok=false;
 try{text=exportRmc("cloth",{{m,-1}}).text;const auto back=loadCustomCampaignText(text,"cloth-lab");const auto& cl=back->maps.at(0)->cloths;
  ok=cl.size()==1&&cl[0].kind==ClothKind::Cable&&std::fabs(cl[0].to.y-5)<1e-4f&&std::fabs(cl[0].length-4.5f)<1e-4f&&cl[0].loose;
  out<<"exported: "<<text.substr(text.find("CLOTH|"),text.find('\n',text.find("CLOTH|"))-text.find("CLOTH|"))<<"\n";}
 catch(const std::exception& e){out<<"the round trip threw: "<<e.what()<<"\n";}
 check(ok,"record: a CLOTH record goes out and back in with its fields");}

void cost(std::ostream& out,const Check& check){
 auto m=regression::room();m->definition.playerStart={3,3};for(int k=0;k<4;++k){ClothSpec c;c.kind=ClothKind::Curtain;c.at={4.f+float(k)*1.6f,10,2.4f};c.to={5.4f+float(k)*1.6f,10,2.4f};c.drop=1.8f;m->cloths.push_back(c);}
 Game g(regression::campaign(m));LabAccess::player(g).pos={3,3};
 auto frame=[&](){const auto t0=std::chrono::steady_clock::now();g.update({},1.f/60);return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();};
 double awake=0;for(int i=0;i<30;++i)awake+=frame();awake/=30;
 for(int i=0;i<360;++i)frame();double still=0;for(int i=0;i<30;++i)still+=frame();still/=30;
 int sleeping=0;for(int i=0;i<g.clothCount();++i)sleeping+=asleep(g,i);
 const auto* r0=g.clothRun(0);const auto beadsEach=r0?r0->beads:0;
 for(int i=0;i<g.clothCount();++i)g.shootCloth({3.f+1.6f*float(i),9,1.5f},{6.f+1.6f*float(i),11,1.5f});double struck=0;for(int i=0;i<10;++i)struck+=frame();struck/=10;
 out<<"four curtains ("<<beadsEach<<" beads each): a frame "<<f3(awake,3)<<" ms while they fall, "<<f3(struck,3)<<" ms shot and swaying (the last step's world "<<(r0?f3(r0->space.worldMs,3):"")<<", pairs "<<(r0?f3(r0->space.pairMs,3):"")<<", solve "<<(r0?f3(r0->space.solveMs,3):"")<<" ms each), "<<f3(still,3)<<" ms once "<<sleeping<<" of 4 sleep\n";
 check(sleeping==4&&awake<6&&struck<6&&still<.3,"cost: four curtains falling or swaying cost under 6 ms a frame (1.5 ms each), asleep under 0.3");}

const bool bare=add({.name="cloth",.summary="curtains, straps and cables (CLOTH): a curtain hangs, parts when walked through and settles; a cable drapes over a crate; a strap swings when shot; the record round trip; the cost",.standalone=[]{
 std::ofstream out("cloth.txt");bool ok=true;auto check=[&](bool pass,const std::string& what){out<<(pass?"PASS ":"FAIL ")<<what<<'\n';out.flush();ok&=pass;};
 out<<"---- curtain ----\n";curtain(out,check);
 out<<"---- cable ----\n";cable(out,check);
 out<<"---- strap ----\n";strap(out,check);
 out<<"---- record ----\n";record(out,check);
 out<<"---- cost ----\n";cost(out,check);
 return ok;}});

// The Starter Map's cloth corner, played (fixture cloth-room).
struct Watch {float parted=0;bool sleptBefore=false;std::string awake;std::vector<Point3> rest;int curtain=-1;Point3 way{};};
std::map<const Scene*,Watch>& watches(){static std::map<const Scene*,Watch> w;return w;}
const bool room=add({.name="cloth-room",.summary="the Starter Map's curtain, strap and loose cable in SHOWCASE: PHYSICS: they hang, settle and sleep, and the curtain parts when she walks through it",
 .fixtures={"cloth-room"},
 .setup=[](Scene& s){watches().erase(&s);watches()[&s];LabAccess::setGod(s.g(),true);},
 // (The cloths are built on the first frame.) At 4.5 s: all asleep? Then she walks through the curtain's middle at 1.5 m/s,
 // from 0.9 m on the camera's side of it.
 .step=[](Scene& s,float){auto& w=watches()[&s];Game& g=s.g();
  if(w.curtain<0)for(int i=0;i<g.clothCount();++i)if(g.clothRun(i)->spec.kind==ClothKind::Curtain)w.curtain=i;if(w.curtain<0)return;
  const auto& spec=g.clothRun(w.curtain)->spec;const Point3 mid=(spec.at+spec.to)*.5f,rail=spec.to-spec.at;const float l=std::max(.01f,spanOf(rail));
  if(s.frame==270){w.sleptBefore=true;for(int i=0;i<g.clothCount();++i)if(!g.clothRun(i)->asleep()){w.sleptBefore=false;{float v=0;const auto* r=g.clothRun(i);for(int k=0;k<r->beads;++k)v=std::max(v,vectra::len(r->space.bodies[size_t(k)].v));w.awake+=std::string(clothWord(r->spec.kind))+" (fastest bead "+f3(v,3)+" m/s) ";}}
   w.rest=beads(g,w.curtain).p;
   // Across the rail, toward the camera's side (the camera stands south of it, at lower y).
   Point3 n{-rail.y/l,rail.x/l,0};const auto cam=LabAccess::player(g).pos;if((cam.x-mid.x)*n.x+(cam.y-mid.y)*n.y>0)n=n*-1.f;w.way=n*-1.f;
   LabAccess::player(g).pos={mid.x+n.x*.9f,mid.y+n.y*.9f};}
  if(s.frame>270&&s.frame<=390){auto& p=LabAccess::player(g).pos;p.x+=w.way.x*1.5f/60;p.y+=w.way.y*1.5f/60;
   const Beads b=beads(g,w.curtain);for(size_t k=0;k<b.p.size()&&k<w.rest.size();++k)w.parted=std::max(w.parted,std::hypot(b.p[k].x-w.rest[k].x,b.p[k].y-w.rest[k].y));}},
 .capture=[](Scene& s,const Frame& f){if(f.index==269)s.shoot("hanging");if(f.index==302)s.shoot("walked-through");},
 .metrics=[](Scene& s,Report& r){auto& w=watches()[&s];Game& g=s.g();
  int kinds[3]{0,0,0};for(int i=0;i<g.clothCount();++i)++kinds[int(g.clothRun(i)->spec.kind)];
  r.check("the Starter Map's physics room has a curtain, a strap and a cable",kinds[0]>=1&&kinds[1]>=1&&kinds[2]>=1,std::to_string(kinds[0])+" curtains, "+std::to_string(kinds[1])+" straps, "+std::to_string(kinds[2])+" cables");
  r.check("they hang and sleep before she comes",w.sleptBefore,w.awake.empty()?"":"awake: "+w.awake);
  r.metric("curtain_parted",w.parted,"m");r.check("the curtain parts as she walks through it",w.parted>.12f,f3(w.parted)+" m");}});
}
}

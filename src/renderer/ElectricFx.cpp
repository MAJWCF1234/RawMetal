#include "ElectricFx.h"
#include "GpuRenderer.h"
#include "Projection.h"
#include "../game/EngineRegressionScene.h"
#include <algorithm>
#include <cstring>
#include <ostream>

namespace retro {
namespace {
using Texture=ElectricFx::Texture;
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float span(Point3 v){return std::sqrt(dot3(v,v));}
Point3 unit(Point3 v,Point3 fallback){const float l=span(v);return l>1e-6f?v*(1/l):fallback;}
std::uint32_t pack(Point3 c,float a){auto q=[](float v){return unsigned(std::clamp(v,0.f,1.f)*255.f+.5f);};return (q(a)<<24)|(q(c.x)<<16)|(q(c.y)<<8)|q(c.z);}
Point3 blend(Point3 a,Point3 b,float t){return a+(b-a)*t;}
Texture blank(int w,int h,bool additive=false){Texture t{w,h,std::vector<std::uint32_t>(size_t(w*h))};t.clampEdges=true;if(additive)t.additive=true;else t.transparent=true;return t;}
// Molten steel as it cools: white-hot, straw, yellow, orange, cherry, dull red, out.
Point3 glowColour(float heat){
 static const float stops[][4]={{0,.16f,.02f,.01f},{.15f,.42f,.05f,.01f},{.3f,.75f,.16f,.03f},{.45f,.95f,.36f,.06f},{.62f,1,.6f,.2f},{.8f,1,.84f,.5f},{1,1,.97f,.88f}};
 heat=std::clamp(heat,0.f,1.f);for(int i=1;i<7;++i)if(heat<=stops[i][0]){const float t=(heat-stops[i-1][0])/(stops[i][0]-stops[i-1][0]);return blend({stops[i-1][1],stops[i-1][2],stops[i-1][3]},{stops[i][1],stops[i][2],stops[i][3]},t);}
 return {1,.98f,.92f};
}
constexpr int Heats=12;
// A spark's streak by heat: along u from its tail (faint) to its head (bright), a hot core down its middle.
const std::array<Texture,Heats>& streaks(){static const auto made=[]{std::array<Texture,Heats> out;
 for(int k=0;k<Heats;++k){auto&t=out[size_t(k)];t=blank(32,8);const float heat=(k+.5f)/Heats;const Point3 tint=glowColour(heat);
  for(int y=0;y<8;++y)for(int x=0;x<32;++x){const float along=(x+.5f)/32,across=((y+.5f)/8-.5f)*2,halo=std::exp(-across*across*3.2f),core=std::exp(-across*across*26.f);
   const float head=std::pow(along,1.4f)*(along>.88f?1.15f:1.f);
   t.pixels[size_t(y*32+x)]=pack(blend(tint,{1,1,1},core*(.2f+.8f*heat*heat)),std::clamp((halo*.75f+core*.55f)*head*(.55f+.45f*std::min(1.f,heat*2.2f)),0.f,1.f));}}
 return out;}();return made;}
// The soft warm halo round a hot spark or ember (additive: the renderer warms it further).
const std::array<Texture,4>& glows(){static const auto made=[]{std::array<Texture,4> out;
 for(int k=0;k<4;++k){auto&t=out[size_t(k)];t=blank(32,32,true);const float level=(k+1)/4.f;
  for(int y=0;y<32;++y)for(int x=0;x<32;++x){const float dx=(x-15.5f)/15.5f,dy=(y-15.5f)/15.5f,r=std::min(1.f,std::sqrt(dx*dx+dy*dy)),fall=(1-r)*(1-r);
   t.pixels[size_t(y*32+x)]=pack({1,.93f,.82f},fall*fall*level*.9f+fall*.1f*level);}}
 return out;}();return made;}
// A bolt's core: a hard white line with a thin blue-white edge.
const Texture& boltCore(){static const Texture made=[]{auto t=blank(4,16);
 for(int y=0;y<16;++y)for(int x=0;x<4;++x){const float c=((y+.5f)/16-.5f)*2,core=std::exp(-c*c*30.f),mid=std::exp(-c*c*6.f);
  t.pixels[size_t(y*4+x)]=pack(blend({.74f,.84f,1},{1,1,1},core),std::clamp(core+.5f*mid,0.f,1.f));}
 return t;}();return made;}
// The glow round a bolt, blue-white, or violet at its fringe off a cell of resonance ore; it thins
// over the middle so the core shows through whichever is drawn last.
const std::array<std::array<Texture,3>,2>& boltGlows(){static const auto made=[]{std::array<std::array<Texture,3>,2> out;
 for(int fringe=0;fringe<2;++fringe)for(int k=0;k<3;++k){auto&t=out[size_t(fringe)][size_t(k)];t=blank(4,32);const float level=k==0?.5f:k==1?.75f:1.f;
  for(int y=0;y<32;++y)for(int x=0;x<4;++x){const float c=((y+.5f)/32-.5f)*2,halo=std::exp(-c*c*4.f)*(1-c*c),dip=1-.75f*std::exp(-c*c*60.f);
   const Point3 inner{.5f,.65f,1},outer=fringe?Point3{.45f,.25f,1}:Point3{.25f,.4f,.95f};
   t.pixels[size_t(y*4+x)]=pack(blend(inner,outer,std::fabs(c)),halo*dip*level);}}
 return out;}();return made;}
// Where a bolt leaves and lands: a white-hot point in a blue halo.
const Texture& hotSpot(){static const Texture made=[]{auto t=blank(32,32);
 for(int y=0;y<32;++y)for(int x=0;x<32;++x){const float dx=(x-15.5f)/15.5f,dy=(y-15.5f)/15.5f,r2=dx*dx+dy*dy,core=std::exp(-r2*28.f),halo=std::exp(-r2*5.f)*std::max(0.f,1-std::sqrt(r2));
  t.pixels[size_t(y*32+x)]=pack(blend({.35f,.5f,1},{1,1,1},core),std::clamp(core+.6f*halo,0.f,1.f));}
 return t;}();return made;}
// Ozone and burnt insulation: a thin grey wisp, by how much of it is left.
const std::array<Texture,4>& wisps(){static const auto made=[]{std::array<Texture,4> out;
 for(int k=0;k<4;++k){auto&t=out[size_t(k)];t=blank(32,32);const float level=(k+1)/4.f;
  for(int y=0;y<32;++y)for(int x=0;x<32;++x){const float dx=(x-15.5f)/15.5f,dy=(y-15.5f)/15.5f,r=std::sqrt(dx*dx+dy*dy);
   unsigned h=unsigned(x)*2654435761u^unsigned(y)*2246822519u^0x2c1b3c6du;h=(h^(h>>15))*3266489917u;const float noise=float((h>>24)&255u)/255.f;
   const float swirl=.5f+.5f*std::sin(dx*5.f+std::sin(dy*4.f)*1.7f);
   t.pixels[size_t(y*32+x)]=pack({.56f,.56f,.6f},std::clamp((.92f-r)*1.4f+(noise-.5f)*.2f,0.f,1.f)*(.55f+.45f*swirl)*.5f*level);}}
 return out;}();return made;}
unsigned hashOf(unsigned h){h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return h;}
}

ElectricStats ElectricFx::draw(const Game& game,int level,Point3 eye,float pixel,const Quad& quad,const Hidden& hidden){
 ElectricStats stats;const auto&frame=game.electricNow(level);if(frame.bursts.empty()&&frame.arcs.empty())return stats;
 // A copy to sort: the frame's own is shared by every eye and the lights.
 electric::Scene scene;scene.bursts=frame.bursts;scene.arcs=frame.arcs;
 const auto&w=game.chunkWorld(level);const float now=game.elapsed();
 auto emit=[&](Point3 a,Point3 b,Point3 c,Point3 d,const Texture&t,float light){++stats.quads;quad(a,b,c,d,t,light);};
 auto distance=[&](Point3 p){return span(p-eye);};
 auto unseen=[&](Point3 lo,Point3 hi){return hidden&&hidden(lo-Point3{.05f,.05f,.05f},hi+Point3{.05f,.05f,.05f});};
 // A strip from a to b turned to face the eye, run past each end by half its width so pieces join.
 auto ribbon=[&](Point3 a,Point3 b,float width,const Texture&t,float light){const Point3 d=b-a;const float l=span(d);if(l<1e-6f)return;const Point3 dir=d*(1/l);
  const Point3 side=unit(cross(dir,eye-(a+b)*.5f),{0,0,1})*width;a=a-dir*(width*.5f);b=b+dir*(width*.5f);emit(a-side,b-side,b+side,a+side,t,light);};
 // A card facing the eye, turned by `turn` so no two read as one stamp.
 auto card=[&](Point3 c,float radius,const Texture&t,float light,float turn,float tall=1){const Point3 view=unit(c-eye,{1,0,0});const Point3 right=unit(cross(view,{0,0,1}),{1,0,0}),up=cross(right,view);
  const float cs=std::cos(turn),sn=std::sin(turn);const Point3 u=(right*cs+up*sn)*radius,v=(right*-sn+up*cs)*(radius*tall);emit(c-u-v,c+u-v,c+u+v,c-u+v,t,light);};
 // Nearest first: the budgets let the far ones go.
 std::sort(scene.bursts.begin(),scene.bursts.end(),[&](const auto&a,const auto&b){return distance(a.at)<distance(b.at);});
 std::sort(scene.arcs.begin(),scene.arcs.end(),[&](const auto&a,const auto&b){return distance(a.from)<distance(b.from);});
 std::vector<electric::Spark> sparks;int left=electric::SparkBudget;
 for(const auto&b:scene.bursts){if(distance(b.at)>40.f)continue;
  // Everywhere its sparks could be (drag holds the fastest to about a metre and a half, embers roll a
  // little further) down to a floor well below: all of it out of sight, it is not simulated this frame.
  {const float range=b.embers>0?2.6f:1.7f;if(unseen(b.at-Point3{range,range,3.f},b.at+Point3{range,range,1.2f}))continue;}
  if(b.smoke){const float age=now-b.time;
   // Ozone off the break: three wisps rising, spreading and thinning over two seconds. Smoke, not light:
   // the room lights it (and the flash while it lasts), so it is gone in the dark.
   if(age>=0&&age<2.f)for(int i=0;i<3;++i){const unsigned h=hashOf(b.seed+unsigned(i)*7919u);const float r1=float(h&255u)/255.f,r2=float((h>>8)&255u)/255.f,r3=float((h>>16)&255u)/255.f;
    const Point3 at=b.at+Point3{(r1-.5f)*.12f*age,(r2-.5f)*.12f*age,.03f+.22f*age*(.8f+.4f*r3)};const float remain=1-age/2.f;
    card(at,.012f+.05f*age*(.8f+.4f*r1),wisps()[size_t(std::clamp(int(remain*3.f),0,3))],.9f,(r2-.5f)*.5f+std::sin(age*2.3f+r3*6.f)*.25f,2.2f);}}
  // The break flares as it lets go: a warm bloom over its first few hundredths of a second.
  if(const float age=now-b.time;age>=0&&age<.07f)card(b.at,std::max(.07f*b.heat*std::sqrt(float(b.count)/24.f),pixel*distance(b.at)*10.f)*(1-age/.07f*.5f),glows()[3],1.5f+b.heat,0);
  if(left<=0)continue;sparks.clear();const int n=electric::sparks(w,b,now,sparks,left);left-=n;stats.sparks+=n;if(!n)continue;
  {Point3 lo=sparks[0].p,hi=lo;for(const auto&s:sparks){lo={std::min(lo.x,s.p.x),std::min(lo.y,s.p.y),std::min(lo.z,s.p.z)};hi={std::max(hi.x,s.p.x),std::max(hi.y,s.p.y),std::max(hi.z,s.p.z)};}if(unseen(lo,hi))continue;}
  for(const auto&s:sparks){const float px=pixel*distance(s.p),width=std::max(.0013f*s.size*(s.ember?1.5f:1.f),px*1.4f),speed=span(s.v);
   const auto&streak=streaks()[size_t(std::clamp(int(s.heat*Heats),0,Heats-1))];const float light=1.5f+.7f*s.heat;
   // Stretched along its flight by about a 30th of a second, as the eye sees a spark; settled, a bead.
   if(speed>.08f&&!s.resting){const Point3 dir=s.v*(1/speed);ribbon(s.p-dir*(std::clamp(speed/30.f,.004f,.16f)+width),s.p,width,streak,light);}
   else{const Point3 right=unit(cross(unit(s.p-eye,{1,0,0}),{0,0,1}),{1,0,0});const float r=std::max(width*1.3f,px*1.3f);ribbon(s.p-right*r,s.p+right*r,r,streak,light);}
   if(s.heat>.3f||s.ember)card(s.p,std::max((.008f+.022f*s.heat)*(s.ember?.7f:1.f),px*4.5f),glows()[size_t(std::clamp(int(s.heat*4.f),0,3))],1.5f,0);}}
 std::vector<electric::Piece> pieces;
 for(const auto&a:scene.arcs){if(stats.bolts>=electric::BoltBudget)break;const float lit=electric::strike(a,now)*a.strength;if(lit<=0)continue;const float d=distance(a.from);if(d>45.f)continue;
  pieces.clear();electric::bolt(a,now,d<4?5:d<10?4:3,pieces);if(pieces.empty())continue;
  {Point3 lo=pieces[0].a,hi=lo;for(const auto&p:pieces)for(Point3 q:{p.a,p.b}){lo={std::min(lo.x,q.x),std::min(lo.y,q.y),std::min(lo.z,q.z)};hi={std::max(hi.x,q.x),std::max(hi.y,q.y),std::max(hi.z,q.z)};}if(unseen(lo,hi))continue;}
  ++stats.bolts;stats.pieces+=int(pieces.size());
  const auto&glow=boltGlows()[a.resonance?1:0][size_t(lit>.75f?2:lit>.45f?1:0)];
  for(const auto&p:pieces){const float px=pixel*distance((p.a+p.b)*.5f);
   ribbon(p.a,p.b,std::max(.03f*p.width*a.width,px*7.f)*(.6f+.4f*lit),glow,1.5f+.6f*lit);
   ribbon(p.a,p.b,std::max(.0016f*p.width*a.width,px)*(.75f+.25f*lit),boltCore(),1.8f+1.2f*lit);}
  const float turn=float(hashOf(a.seed)&1023u)/1024.f*kTwoPi;
  card(a.from,std::max(.024f*(.5f+lit)*a.width,pixel*d*6.f),hotSpot(),1.6f+lit,turn);
  if(!a.open)card(a.to,std::max(.02f*(.5f+lit)*a.width,pixel*distance(a.to)*6.f),hotSpot(),1.6f+lit,turn+1.f);}
 return stats;
}

bool SoftwareRenderer::testElectricRendering(std::ostream& report){
 bool passed=true;auto check=[&](bool ok,const std::string&text){report<<(ok?"PASS":"FAIL")<<": "<<text<<'\n';report.flush();passed&=ok;};
 constexpr int W=480,H=270;
 for(bool vulkan:{true,false}){const char*backend=vulkan?"Vulkan":"CPU";
  auto storage=std::make_unique<SoftwareRenderer>(W,H);auto&d=*storage;if(vulkan&&!d.enableHardware())return false;d.setEnvironmentInspection(true);if(vulkan)d.m_gpu->setPostProcessing(false);
  auto luminance=[&](Point3 at){auto c=d.cameraPoint(at,d.m_view);const float f=Projection{float(W)/H}.focal(float(W));int cx=int(W*.5f+c.x*f/c.z),cy=int(H*.5f-c.y*f/c.z);double sum=0;int n=0;
   for(int y=cy-3;y<=cy+3;++y)for(int x=cx-3;x<=cx+3;++x)if(c.z>.1f&&x>=0&&y>=0&&x<W&&y<H){auto px=d.m_pixels[size_t(y*W+x)];sum+=((px>>16)&255)*.2126+((px>>8)&255)*.7152+(px&255)*.0722;++n;}return float(n?sum/n:-1);};
  // The west wall beside the socket: before the tear, its brightest while the clip arcs, and once the lead is dead.
  const Point3 probe{1.002f,7.55f,.75f};electric::resetCaches();
  d.render(Game::electricInspection(-.2f));const float before=luminance(probe);float peak=0;
  for(float age:{0.f,.02f,.04f,.06f,.08f,.1f,.12f}){d.render(Game::electricInspection(age));peak=std::max(peak,luminance(probe));}
  d.render(Game::electricInspection(3.f));const float after=luminance(probe);
  check(before>=0&&peak>before*1.2f+3&&std::fabs(after-before)<=std::max(2.f,before*.08f),std::string(backend)+": a torn live lead's arc lights the wall beside it (luminance "+std::to_string(before)+" before, "+std::to_string(peak)+" at its brightest), and the wall falls back once it is dead ("+std::to_string(after)+")");
  // The same moment of the same tear draws the same frame: asked in order, asked again after a later one, asked fresh.
  auto frame=[&](float age){d.render(Game::electricInspection(age));return std::vector<std::uint32_t>(d.m_pixels.begin(),d.m_pixels.end());};
  electric::resetCaches();auto first=frame(.2f);frame(.6f);auto again=frame(.2f);electric::resetCaches();auto fresh=frame(.2f);
  int worst=0;for(size_t i=0;i<first.size();++i)for(int shift:{0,8,16})worst=std::max({worst,std::abs(int((first[i]>>shift)&255)-int((again[i]>>shift)&255)),std::abs(int((first[i]>>shift)&255)-int((fresh[i]>>shift)&255))});
  check(worst<=(vulkan?1:0),std::string(backend)+": the tear 0.2 s in renders the same frame asked again after a later one and asked fresh (peak channel difference "+std::to_string(worst)+")");
  // A live floor along the wall, its first pulse: its arcs light the wall beside it, and several strikes
  // at once never burn the wall (or the frame) to white -- the CPU has no tone curve to hold them.
  {auto white=[&]{size_t n=0;for(auto p:d.m_pixels)n+=(p&0xffffffu)==0xffffffu;return float(n)/float(d.m_pixels.size());};
   const Point3 wall{1.002f,7.f,.55f};d.render(Game::electricInspection(-.2f,3));const float off=luminance(wall),offWhite=white();float peak=0,worstWhite=0;
   size_t arcs=0;float light=0;
   for(int i=0;i<=15;++i){auto g=Game::electricInspection(float(i)/30,3);d.render(g);peak=std::max(peak,luminance(wall));worstWhite=std::max(worstWhite,white());arcs=std::max(arcs,g.electricNow(0).arcs.size());for(const auto&f:g.weaponFlashes())light=std::max(light,f.strength);}
   check(off>=0&&peak>off*1.15f+2&&peak<235&&worstWhite<=offWhite+.01f,std::string(backend)+": a live floor's arcs light the wall beside it (luminance "+std::to_string(off)+" off, "+std::to_string(peak)+" at its brightest; up to "+std::to_string(arcs)+" arcs, light "+std::to_string(light)+") and never burn it white ("+std::to_string(worstWhite*100)+"% of the frame pure white at worst, "+std::to_string(offWhite*100)+"% with the power off)");}
  // The ozone wisp is smoke, not light: a second after a shower it shows above the break in a lit room,
  // and in the same room dark it is not there at all (a self-lit wisp glowed in the black).
  {float seen[2]{};
   for(int dark=0;dark<2;++dark){auto m=regression::room();m->definition.ceiling=3.2f;if(dark)m->definition.ambient=0;else m->lights.push_back({{8,7.2f},3.1f,WorldLightMount::Ceiling,0,.85f,8});
    float with=0,without=0;
    for(int smoke=0;smoke<2;++smoke){Game g(regression::campaign(m));auto&p=const_cast<Player&>(g.player());p.pos={8,6.2f};p.angle=kPi*.5f;p.pitch=0;
     if(smoke)g.sparkBurst(0,{8,8.5f,1.2f},{0,0,1},.3f,1,true);for(int i=0;i<60;++i)g.update({},1.f/60);d.render(g);(smoke?with:without)=luminance({8,8.5f,1.45f});}
    seen[dark]=std::fabs(with-without);}
   // Dark here is unlit, not the game's darkness: Vulkan keeps its light floor, which lights the wisp as it lights the wall
   // behind (a self-lit wisp changed as much in the dark as in the light).
   check(seen[0]>1.5f&&seen[1]<std::max(.5f,seen[0]*.1f),std::string(backend)+": the ozone wisp shows in a lit room (luminance changed by "+std::to_string(seen[0])+") and is lit by it: unlit it all but goes (changed by "+std::to_string(seen[1])+")");}
 }
 // Forty showers at once: the frame draws no more than its budget of sparks.
 {auto g=Game::electricInspection(-.5f);for(int i=0;i<40;++i)g.sparkBurst(0,{2+i*.1f,7,1.2f},{0,0,1},1,48,true);int quads=0;
  auto stats=ElectricFx::draw(g,0,{5,6,1.6f},1.f/Projection{16.f/9}.focal(1280.f),[&](Point3,Point3,Point3,Point3,const Texture&,float){++quads;});
  electric::Scene fresh;g.electricScene(0,fresh);std::vector<electric::Spark> first;if(!fresh.bursts.empty())electric::sparks(g.world(),fresh.bursts[0],g.elapsed(),first,electric::SparkBudget);
  check(stats.sparks==electric::SparkBudget&&quads<=electric::SparkBudget*2+40*4,"forty showers of 48 sparks draw "+std::to_string(stats.sparks)+" sparks (budget "+std::to_string(electric::SparkBudget)+") in "+std::to_string(quads)+" quads (showers in the frame "+std::to_string(g.electricNow(0).bursts.size())+", worked out afresh "+std::to_string(fresh.bursts.size())+", the first's sparks "+std::to_string(first.size())+")");}
 return passed;
}
}

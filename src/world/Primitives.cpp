#include "Primitives.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace retro {
namespace {
constexpr float MaxRiser=.17f,Tread=.28f,RampStep=.1f;
static_assert(MaxRiser<PlayerStep&&RampStep<PlayerStep);
struct Defaults {float w,d,h;};
struct Kind {std::string_view name;Defaults defaults;};
// Sizes a player reads as right: a 1.2 m stair at 0.28 m treads, a 2.2 m door,
// a 1 m rail, a 3 m storey.
constexpr Kind kinds[]={
 {"box",{1,1,1}},{"pillar",{.5f,.5f,3}},{"stairs",{1.2f,0,1}},{"ramp",{1.2f,0,.6f}},
 {"column",{.6f,.6f,3}},{"arch",{2.4f,.6f,3.4f}},{"vault",{4,4,3}},{"doorframe",{1.2f,.3f,2.2f}},
 {"window",{3,.3f,1}},{"fins",{3,.4f,3}},{"canopy",{3,1.5f,.25f}},{"railing",{2,.06f,GuardHeight}},
 {"corbel",{.6f,.6f,.6f}},{"plinth",{2,2,.45f}},
 // Openings through thick walls: each is the wall panel with its opening left open.
 {"slit",{1.2f,.4f,3}},{"clerestory",{3,.4f,3}},{"embrasure",{1.6f,.8f,3}},{"roundel",{2,.4f,3}},
 {"niche",{1,.5f,1.6f}},{"hood",{1,.5f,1.6f}},
 // Masses and structure.
 {"batter",{3,1,2.5f}},{"waffle",{4,4,.6f}},{"pilotis",{.6f,.6f,3.5f}},{"megastep",{3,0,1.35f}},
 {"lightwell",{4,4,GuardHeight}},{"bridge",{3,8,3}},{"core",{4,4,8}},{"dome",{4,4,2}},{"strut",{.35f,3,3}},
 {"girder",{.5f,4,.6f}},{"quadpier",{1,1,3}},
 // Detailing.
 {"band",{3,.08f,.25f}},{"spout",{.25f,.8f,.2f}},{"chase",{.8f,.2f,3}},{"lattice",{2,.2f,2.4f}}};

// The primitive's frame: u across it, v along its facing, both from its centre.
struct Frame {
 float x,y;int facing;
 Vec2 forward()const{constexpr Vec2 f[]={{0,1},{1,0},{0,-1},{-1,0}};return f[facing];}
 Vec2 across()const{auto f=forward();return {f.y,-f.x};}
 Vec2 at(float u,float v)const{auto a=across(),f=forward();return {x+a.x*u+f.x*v,y+a.y*u+f.y*v};}
};
}

void expandPrimitive(const Primitive& p,std::vector<Structure>& out){
 const Kind* kind=nullptr;for(const auto& k:kinds)if(k.name==p.kind)kind=&k;
 if(!kind)throw std::runtime_error("unknown primitive kind '"+std::string(p.kind)+"'");
 if(p.facing<0||p.facing>3)throw std::runtime_error("primitive facing "+std::to_string(p.facing)+" must be 0-3");
 float w=p.w>0?p.w:kind->defaults.w,h=p.h>0?p.h:kind->defaults.h,d=p.d;
 const int risers=int(std::ceil(h/MaxRiser-1e-4f));
 const int megasteps=int(std::ceil(h/.45f-1e-4f));
 if(d<=0)d=p.kind=="stairs"?risers*Tread:p.kind=="megastep"?megasteps*.9f:p.kind=="ramp"?std::max(1.f,h*4):kind->defaults.d;
 if(p.w<0||p.d<0||p.h<0||w<=0||d<=0||h<=0)throw std::runtime_error("primitive "+std::string(p.kind)+" sizes must be positive");
 const Frame frame{p.x,p.y,p.facing};const float hw=w*.5f,hd=d*.5f,z=p.z;
 auto box=[&](float u0,float u1,float v0,float v1,float bottom,float top,int material,bool solid,bool rail=false,std::uint8_t shape=ShapeBox){
  if(u1<=u0||v1<=v0||top<=bottom)return;
  auto a=frame.at(u0,v0),b=frame.at(u1,v1);
  out.push_back({std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y),bottom,top,rail,material,solid,shape,std::uint8_t(p.facing)});};
 auto part=[&](float u0,float u1,float v0,float v1,float bottom,float top){box(u0,u1,v0,v1,bottom,top,p.material,p.solid);};
 auto proxy=[&](float u0,float u1,float v0,float v1,float bottom,float top){if(p.solid)box(u0,u1,v0,v1,bottom,top,CollisionProxyMaterial,true);};
 auto shape=[&](std::uint8_t s){box(-hw,hw,-hd,hd,z,z+h,p.material,false,false,s);};
 const auto& k=p.kind;
 if(k=="box"||k=="pillar")part(-hw,hw,-hd,hd,z,z+h);
 else if(k=="stairs"){
  // Rises toward its facing in equal risers no taller than 0.17 m.
  for(int i=0;i<risers;++i)part(-hw,hw,-hd+d*i/risers,-hd+d*(i+1)/risers,z,z+h*(i+1)/risers);
 }else if(k=="ramp"){
  // Collides as 0.1 m steps at the slope's mid height, inside the step-up limit.
  shape(ShapeRamp);const int steps=int(std::ceil(h/RampStep-1e-4f));
  for(int i=0;i<steps;++i)proxy(-hw,hw,-hd+d*i/steps,-hd+d*(i+1)/steps,z,z+h*(i+.5f)/steps);
 }else if(k=="column"){
  shape(ShapeColumn);const float r=std::min(hw,hd);
  proxy(-r*.92f,r*.92f,-r*.38f,r*.38f,z,z+h);proxy(-r*.38f,r*.38f,-r*.92f,r*.92f,z,z+h);proxy(-r*.7f,r*.7f,-r*.7f,r*.7f,z,z+h);
 }else if(k=="arch"){
  const auto arch=archOpening(w,h);
  if(arch.spring<0||archClearance(arch)<PlayerHeadroom)throw std::runtime_error("arch "+std::to_string(w)+" x "+std::to_string(h)+" is too low to walk through: make it taller or wider");
  shape(ShapeArch);proxy(-hw,-arch.half,-hd,hd,z,z+h);proxy(arch.half,hw,-hd,hd,z,z+h);
  constexpr int slices=8;
  for(int i=0;i<slices;++i){const float u0=-arch.half+2*arch.half*i/slices,u1=-arch.half+2*arch.half*(i+1)/slices,m=(u0+u1)*.5f;
   proxy(u0,u1,-hd,hd,z+arch.spring+std::sqrt(std::max(0.f,arch.half*arch.half-m*m)),z+h);}
 }else if(k=="vault"){
  if(hw<=VaultShell||h<=VaultShell)throw std::runtime_error("vault must be wider and taller than its 0.3 m shell");
  shape(ShapeVault);constexpr int slices=10;const float inner=hw-VaultShell;
  for(int i=0;i<slices;++i){const float u0=-hw+w*i/slices,u1=-hw+w*(i+1)/slices,m=(u0+u1)*.5f;
   const float outer=h*std::sqrt(std::max(0.f,1-m*m/(hw*hw))),under=std::fabs(m)<inner?(h-VaultShell)*std::sqrt(std::max(0.f,1-m*m/(inner*inner))):0.f;
   proxy(u0,u1,-hd,hd,z+under,z+std::max(outer,under+.05f));}
 }else if(k=="doorframe"){
  // Jambs and head around a w x h opening in a wall d thick, proud of both faces.
  constexpr float frameWidth=.12f,proud=.04f;
  part(-hw-frameWidth,-hw,-hd-proud,hd+proud,z,z+h+frameWidth);part(hw,hw+frameWidth,-hd-proud,hd+proud,z,z+h+frameWidth);
  part(-hw,hw,-hd-proud,hd+proud,z+h,z+h+frameWidth);
 }else if(k=="window"){
  // A ribbon window from sill (z) to head: projecting sill, head, mullions, glass.
  part(-hw-.1f,hw+.1f,-hd,hd+.12f,z-.08f,z);part(-hw,hw,-hd,hd,z+h,z+h+.1f);
  const int panes=std::max(1,int(std::round(w/1.2f)));
  for(int i=0;i<=panes;++i){const float u=-hw+w*i/panes;part(std::max(-hw,u-.03f),std::min(hw,u+.03f),-.05f,.05f,z,z+h);}
  box(-hw,hw,-.015f,.015f,z,z+h,17,p.solid);
 }else if(k=="fins"){
  // 0.1 m fins, the gaps between them narrower than a player: a screen you see through.
  const int count=std::max(2,int(std::ceil((w-.1f)/(.1f+2*PlayerRadius*.95f)))+1);
  for(int i=0;i<count;++i){const float u=-hw+.05f+(w-.1f)*i/(count-1);part(u-.05f,u+.05f,-hd,hd,z,z+h);}
 }else if(k=="canopy"){
  // A cantilevered slab whose underside is z, with a fascia lip at the front.
  part(-hw,hw,-hd,hd,z,z+h);part(-hw,hw,hd-.08f,hd,z-.15f,z);
 }else if(k=="railing")box(-hw,hw,-hd,hd,z,z+h,p.material,p.solid,true);
 else if(k=="corbel"){
  // Three steps, deepening upward from the wall at the back.
  for(int i=0;i<3;++i)part(-hw,hw,-hd,-hd+d*(i+1)/3,z+h*i/3,z+h*(i+1)/3);
 } else if(k=="plinth"){part(-hw+.06f,hw-.06f,-hd+.06f,hd-.06f,z,z+h-.08f);part(-hw,hw,-hd,hd,z+h-.08f,z+h);}
 // Openings: the panel around a hole you can see and shoot through.
 else if(k=="slit"){const float o=std::min(.125f,hw*.4f);part(-hw,-o,-hd,hd,z,z+h);part(o,hw,-hd,hd,z,z+h);part(-o,o,-hd,hd,z,z+.6f);part(-o,o,-hd,hd,z+h-.4f,z+h);}
 else if(k=="clerestory"){const float e=std::min(.3f,hw*.3f),s0=std::max(z,z+h-.8f),s1=z+h-.4f;
  part(-hw,hw,-hd,hd,z,s0);part(-hw,hw,-hd,hd,s1,z+h);part(-hw,-hw+e,-hd,hd,s0,s1);part(hw-e,hw,-hd,hd,s0,s1);}
 else if(k=="embrasure"){const float o=std::min(.4f,hw*.5f),s0=z+std::min(1.f,h*.35f),s1=std::min(z+h-.2f,s0+2*o);
  part(-hw,-o,-hd,hd,z,z+h);part(o,hw,-hd,hd,z,z+h);part(-o,o,-hd,hd,z,s0);part(-o,o,-hd,hd,s1,z+h);}
 else if(k=="roundel"){
  const auto ring=roundelOpening(w,h);const float r=ring.radius,c=z+ring.centre;
  shape(ShapeRoundel);proxy(-hw,-r,-hd,hd,z,z+h);proxy(r,hw,-hd,hd,z,z+h);
  constexpr int slices=8;
  for(int i=0;i<slices;++i){const float u0=-r+2*r*i/slices,u1=-r+2*r*(i+1)/slices,m=(u0+u1)*.5f,half=std::sqrt(std::max(0.f,r*r-m*m));
   proxy(u0,u1,-hd,hd,z,c-half);proxy(u0,u1,-hd,hd,c+half,z+h);}
 }else if(k=="niche"||k=="hood"){
  // An alcove standing proud of the wall behind it (its back at -d/2), dark at the back.
  constexpr float t=.15f;
  part(-hw,-hw+t,-hd,hd,z,z+h);part(hw-t,hw,-hd,hd,z,z+h);part(-hw+t,hw-t,-hd,hd,z,z+t);part(-hw+t,hw-t,-hd,hd,z+h-t,z+h);
  box(-hw+t,hw-t,-hd,-hd+.03f,z+t,z+h-t,8,p.solid);
  if(k=="hood"){part(-hw-.1f,hw+.1f,-hd,hd+.35f,z+h,z+h+.15f);part(-hw-.1f,hw+.1f,hd+.27f,hd+.35f,z+h-.2f,z+h);}
 }
 // Masses and structure.
 else if(k=="batter"){
  // A retaining mass whose face leans back from its facing; 0.5 m proxy steps, too tall to climb.
  box(-hw,hw,-hd,hd,z,z+h,p.material,false,false,ShapeRamp);out.back().facing=std::uint8_t((p.facing+2)%4);
  const int steps=int(std::ceil(h/.5f-1e-4f));
  for(int i=0;i<steps;++i)proxy(-hw,hw,-hd,hd-d*(i+.5f)/steps,z,z+h*(i+1)/steps);
 }else if(k=="waffle"){
  // A coffered slab: the deck on top, ribs every metre under it; z is the ribs' underside.
  const float deck=std::min(.2f,h*.4f);part(-hw,hw,-hd,hd,z+h-deck,z+h);
  const int nu=std::max(1,int(std::round(w))),nv=std::max(1,int(std::round(d)));
  for(int i=0;i<=nu;++i){const float u=-hw+w*i/nu;part(std::max(-hw,u-.075f),std::min(hw,u+.075f),-hd,hd,z,z+h-deck);}
  for(int j=0;j<=nv;++j){const float v=-hd+d*j/nv;part(-hw,hw,std::max(-hd,v-.075f),std::min(hd,v+.075f),z,z+h-deck);}
 }else if(k=="pilotis"){
  // A round column lifting the floor above, under a stepped square capital.
  const float r=std::min(hw,hd),cap=std::min(.4f,h*.2f);
  box(-r,r,-r,r,z,z+h-cap,p.material,false,false,ShapeColumn);
  proxy(-r*.92f,r*.92f,-r*.38f,r*.38f,z,z+h-cap);proxy(-r*.38f,r*.38f,-r*.92f,r*.92f,z,z+h-cap);proxy(-r*.7f,r*.7f,-r*.7f,r*.7f,z,z+h-cap);
  part(-r*1.3f,r*1.3f,-r*1.3f,r*1.3f,z+h-cap,z+h-cap*.5f);part(-r*1.7f,r*1.7f,-r*1.7f,r*1.7f,z+h-cap*.5f,z+h);
 }else if(k=="megastep"){
  // Oversized treads to sit on: 0.45 m risers, a jump each.
  for(int i=0;i<megasteps;++i)part(-hw,hw,-hd+d*i/megasteps,-hd+d*(i+1)/megasteps,z,z+h*(i+1)/megasteps);
 }else if(k=="lightwell"){
  // The rim round a shaft: a parapet h high, 0.25 m thick, the middle open.
  constexpr float t=.25f;
  part(-hw,hw,-hd,-hd+t,z,z+h);part(-hw,hw,hd-t,hd,z,z+h);part(-hw,-hw+t,-hd+t,hd-t,z,z+h);part(hw-t,hw,-hd+t,hd-t,z,z+h);
 }else if(k=="bridge"){
  // A walk-through span along its facing: deck, side walls slit at eye height, roof.
  constexpr float t=.3f,wall=.25f;const float eye0=z+t+1.2f,eye1=std::min(z+h-t,z+t+1.8f);
  part(-hw,hw,-hd,hd,z,z+t);part(-hw,hw,-hd,hd,z+h-t,z+h);
  for(float u:{-hw,hw-wall}){part(u,u+wall,-hd,hd,z+t,eye0);part(u,u+wall,-hd,hd,eye1,z+h-t);}
 }else if(k=="core"){
  // A tower shaft: four 0.3 m walls with a 1.2 x 2.2 m door in the front one.
  constexpr float t=.3f;const float door=std::min(.6f,hw-t-.05f);
  part(-hw,hw,-hd,-hd+t,z,z+h);part(-hw,-hw+t,-hd+t,hd,z,z+h);part(hw-t,hw,-hd+t,hd,z,z+h);
  part(-hw+t,-door,hd-t,hd,z,z+h);part(door,hw-t,hd-t,hd,z,z+h);part(-door,door,hd-t,hd,z+std::min(2.2f,h-.2f),z+h);
 }else if(k=="dome"){
  // A half ellipsoid on its footprint, collided as stacked crossing boxes.
  shape(ShapeDome);constexpr int layers=6;
  for(int j=0;j<layers;++j){const float top=float(j+1)/layers,s=std::sqrt(std::max(0.f,1-top*top*.92f));
   const float ru=hw*s,rv=hd*s,b=z+h*j/layers,t=z+h*top;
   proxy(-ru*.92f,ru*.92f,-rv*.38f,rv*.38f,b,t);proxy(-ru*.38f,ru*.38f,-rv*.92f,rv*.92f,b,t);proxy(-ru*.7f,ru*.7f,-rv*.7f,rv*.7f,b,t);}
 }else if(k=="strut"){
  // A diagonal brace rising toward its facing, collided as slices along it.
  shape(ShapeStrut);const float s=strutSection(w,h);const int slices=std::max(2,int(std::ceil(d/.25f)));
  for(int i=0;i<slices;++i){const float t0=float(i)/slices,t1=float(i+1)/slices;
   proxy(-hw,hw,-hd+d*t0,-hd+d*t1,z+(h-s)*t0,z+(h-s)*t1+s);}
 }else if(k=="girder"){
  // An I-beam along its facing: flanges w wide, a 0.06 m web; z is its underside.
  const float flange=std::min(.06f,h*.2f);
  part(-hw,hw,-hd,hd,z,z+flange);part(-hw,hw,-hd,hd,z+h-flange,z+h);part(-.03f,.03f,-hd,hd,z+flange,z+h-flange);
 }else if(k=="quadpier"){
  const float t=std::min(.15f,std::min(hw,hd)*.6f),i=.05f;
  for(float u:{-hw+i,hw-i-t})for(float v:{-hd+i,hd-i-t})part(u,u+t,v,v+t,z,z+h);
 }
 // Detailing.
 else if(k=="band")part(-hw,hw,-hd,hd,z,z+h);
 else if(k=="spout"){part(-hw,hw,-hd,hd,z,z+h);part(-hw,hw,hd-.05f,hd,z-.12f,z);}
 else if(k=="chase"){
  // A vertical service recess: pilasters either side of a dark back.
  const float t=std::min(.15f,hw*.4f);
  part(-hw,-hw+t,-hd,hd,z,z+h);part(hw-t,hw,-hd,hd,z,z+h);box(-hw+t,hw-t,-hd,-hd+.02f,z,z+h,8,p.solid);
 }else if(k=="lattice"){
  // A cobogó screen: a frame and a 0.3 m grid of 0.06 m bars.
  constexpr float bar=.06f;
  part(-hw,hw,-hd,hd,z,z+bar);part(-hw,hw,-hd,hd,z+h-bar,z+h);part(-hw,-hw+bar,-hd,hd,z,z+h);part(hw-bar,hw,-hd,hd,z,z+h);
  const int nu=std::max(1,int(std::round(w/.3f))),nz=std::max(1,int(std::round(h/.3f)));
  for(int i=1;i<nu;++i){const float u=-hw+w*i/nu;part(u-bar*.5f,u+bar*.5f,-hd*.6f,hd*.6f,z+bar,z+h-bar);}
  for(int j=1;j<nz;++j){const float zz=z+h*j/nz;part(-hw+bar,hw-bar,-hd*.6f,hd*.6f,zz-bar*.5f,zz+bar*.5f);}
 }
}

bool testPrimitives(){
 std::ostringstream report;bool ok=true;
 auto check=[&](bool pass,const std::string& label){report<<(pass?"PASS ":"FAIL ")<<label<<'\n';ok&=pass;};
 for(int facing=0;facing<4;++facing){
  std::vector<Structure> stairs;expandPrimitive({"stairs",10,10,0,1.2f,0,1,facing,40},stairs);
  bool risers=stairs.size()==6,rising=true;float last=0;
  for(const auto& s:stairs){risers&=s.top-last<=MaxRiser+1e-4f;last=s.top;}
  const Vec2 forward=Frame{0,0,facing}.forward();
  for(size_t i=1;i<stairs.size();++i){const float a=(stairs[i-1].x1+stairs[i-1].x2)*forward.x+(stairs[i-1].y1+stairs[i-1].y2)*forward.y,b=(stairs[i].x1+stairs[i].x2)*forward.x+(stairs[i].y1+stairs[i].y2)*forward.y;rising&=b>a;}
  const float across=facing%2?stairs[0].y2-stairs[0].y1:stairs[0].x2-stairs[0].x1;
  check(risers&&rising&&std::fabs(across-1.2f)<1e-4f,"stairs facing "+std::to_string(facing)+": six risers of 0.17 m or less, rising forward, 1.2 m across");
 }
 std::vector<Structure> ramp;expandPrimitive({"ramp",5,5,0,1.2f,0,.6f,1,41},ramp);
 bool walkable=!ramp.empty()&&ramp[0].shape==ShapeRamp&&!ramp[0].solid&&ramp[0].facing==1;float last=0;
 for(size_t i=1;i<ramp.size();++i){walkable&=ramp[i].material==CollisionProxyMaterial&&ramp[i].solid&&ramp[i].top-last<=RampStep+1e-4f;last=ramp[i].top;}
 check(walkable&&ramp.size()==7&&ramp[0].x2-ramp[0].x1>2.3f,"ramp: true wedge over proxy steps no taller than 0.1 m, 2.4 m long by default");
 std::vector<Structure> arch;expandPrimitive({"arch",0,0,0,2.4f,.6f,3.4f,0,40},arch);const auto opening=archOpening(2.4f,3.4f);
 bool clear=true;for(size_t i=1;i<arch.size();++i){const auto& s=arch[i];if(s.x2>-opening.half+.01f&&s.x1<opening.half-.01f&&s.bottom<opening.spring-.01f)clear=false;}
 check(arch[0].shape==ShapeArch&&clear&&opening.spring>2.2f,"arch: a walkable opening at least 2.2 m to the springline");
 std::vector<Structure> column;expandPrimitive({"column",0,0,0,0,0,0,0,42},column);
 check(column.size()==4&&column[0].shape==ShapeColumn&&std::fabs(column[0].top-3)<1e-4f&&std::fabs(column[0].x2-column[0].x1-.6f)<1e-4f,"column: 0 sizes take the 0.6 m by 3 m default");
 std::vector<Structure> vault;expandPrimitive({"vault",0,0,3,4,4,2,0,40},vault);bool shell=vault.size()==11;
 for(size_t i=1;i<vault.size();++i)shell&=vault[i].top<=3+2+1e-3f&&vault[i].bottom>=3-1e-4f;
 check(shell&&vault[0].shape==ShapeVault,"vault: shell proxies stay inside its rise");
 {
  // Every kind expands at its defaults, facing each way, inside its own footprint.
  bool all=true;std::string bad;
  for(const auto& kind:kinds)for(int facing=0;facing<4;++facing){
   std::vector<Structure> o;try{expandPrimitive({kind.name,0,0,0,0,0,0,facing,40},o);}catch(const std::exception&){all=false;bad=std::string(kind.name);continue;}
   for(const auto& s:o)if(!(s.x2>s.x1&&s.y2>s.y1&&s.top>s.bottom&&s.x1>-12&&s.x2<12&&s.y1>-12&&s.y2<12&&s.bottom>-1&&s.top<12)){all=false;bad=std::string(kind.name);}
   if(o.empty()){all=false;bad=std::string(kind.name);}}
  check(all,"all "+std::to_string(std::size(kinds))+" kinds expand at their defaults"+(bad.empty()?"":" (failed: "+bad+")"));
  std::vector<Structure> mega;expandPrimitive({"megastep",0,0,0,0,0,0,0,40},mega);
  check(mega.size()==3&&mega[0].top>.215f,"megastep: 0.45 m risers you jump, not walk");
  std::vector<Structure> batter;expandPrimitive({"batter",0,0,0,0,0,0,0,40},batter);bool climbable=false;float last=0;
  for(size_t i=1;i<batter.size();++i){climbable|=batter[i].top-last<=.215f;last=batter[i].top;}
  check(batter[0].shape==ShapeRamp&&batter[0].facing==2&&!climbable,"batter: a face leaning back from its facing, too steep to walk up");
  std::vector<Structure> slit;expandPrimitive({"slit",0,0,0,0,0,0,0,40},slit);bool open=true;
  for(const auto& s:slit)if(s.x1<0&&s.x2>0&&s.bottom<1.5f&&s.top>1.5f)open=false;
  check(open,"slit: the opening is left open through the wall");
  std::vector<Structure> roundel;expandPrimitive({"roundel",0,0,0,0,0,0,0,40},roundel);bool hole=true;
  for(size_t i=1;i<roundel.size();++i){const auto& s=roundel[i];if(s.x1<0&&s.x2>0&&s.bottom<1&&s.top>1)hole=false;}
  check(roundel[0].shape==ShapeRoundel&&hole,"roundel: its centre is open");
  std::vector<Structure> core;expandPrimitive({"core",0,0,0,0,0,0,0,40},core);bool door=true;
  for(const auto& s:core)if(s.x1<0&&s.x2>0&&s.y2>1.8f&&s.bottom<1.f)door=false;
  check(door,"core: a doorway through its front wall");
 }
 {
  // Scaled to the player: what you walk through clears her, screens stop her,
  // guard heights need a crouch-jump, and what you jump onto is over her step.
  auto make=[](std::string_view kind){std::vector<Structure> o;expandPrimitive({kind,0,0,0,0,0,0,0,40},o);return o;};
  auto opening=[](const std::vector<Structure>& parts,float from,float to,float height,float y=0){
   // The widest run along x on the line y, below `height`, that no solid part blocks.
   float best=0,run=0;
   for(float x=from;x<=to;x+=.01f){bool blocked=false;
    for(const auto& s:parts)if(s.solid&&x>s.x1&&x<s.x2&&s.y1<y&&s.y2>y&&s.bottom<height&&s.top>.05f)blocked=true;
    run=blocked?0:run+.01f;best=std::max(best,run);}
   return best;};
  check(archClearance(archOpening(2.4f,3.4f))>=PlayerHeadroom&&opening(make("arch"),-1.2f,1.2f,PlayerHeadroom)>=2*PlayerRadius,"arch: a standing player walks through with 0.15 m to spare");
  const auto vault=make("vault");float walk=0;
  for(float u=0;u<2;u+=.01f){const float inner=(3-VaultShell)*std::sqrt(std::max(0.f,1-u*u/((2-VaultShell)*(2-VaultShell))));if(inner>=PlayerHeadroom)walk=2*u;}
  check(walk>=2.f&&vault[0].top-vault[0].bottom==3,"vault: 3 m high, at least 2 m of it walkable under");
  check(opening(make("core"),-2,2,2.15f,1.85f)>=1.15f,"core: a 1.2 x 2.2 m door in its front wall, the size of the game's doors");
  check(opening(make("fins"),-1.5f,1.5f,2)<2*PlayerRadius&&opening(make("lattice"),-1,1,2)<2*PlayerRadius&&opening(make("slit"),-.6f,.6f,2.5f)<2*PlayerRadius,
        "fins, lattice and slit: gaps you see through but cannot pass");
  check(opening(make("quadpier"),-.5f,.5f,2,-.38f)>=2*PlayerRadius,"quadpier: room to walk between its piers");
  check(make("railing")[0].top>=GuardHeight&&make("lightwell")[0].top>=GuardHeight,"railing and lightwell: guard height 1.1 m, cleared only by a crouch-jump");
  check(make("plinth").back().top>PlayerStep&&make("megastep")[0].top>PlayerStep,"plinth and megastep: seat heights you jump onto");
  const auto roundel=roundelOpening(2,3);
  check(std::fabs(roundel.centre-1.5f)<.01f&&roundel.centre-roundel.radius<1.42f&&roundel.centre+roundel.radius>1.42f,"roundel: the opening frames the player's eye height");
 }
 std::vector<Structure> ghost;expandPrimitive({"ramp",0,0,0,1,2,.4f,0,40,false},ghost);
 check(ghost.size()==1,"a non-solid primitive adds no collision proxies");
 auto throws=[](Primitive p){try{std::vector<Structure> o;expandPrimitive(p,o);}catch(const std::runtime_error&){return true;}return false;};
 check(throws({"spaceship"})&&throws({"box",0,0,0,1,1,1,4})&&throws({"box",0,0,0,-1})&&throws({"arch",0,0,0,4,0,2}),"unknown kinds, facings outside 0-3, negative sizes and low arches are rejected");
 std::ofstream("primitives-test.txt")<<report.str();return ok;
}
}

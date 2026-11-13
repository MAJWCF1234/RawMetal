#include "Particles.h"
#include "Projection.h"
#include "../core/FrameProfile.h"
#include "LookTuning.h"
#include "../game/EngineRegressionScene.h"
#include "../world/CampaignExport.h"
#include "../world/CampaignMaps.h"
#include "../world/CampaignParticles.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

namespace retro {
namespace {
using Texture=Particles::Texture;
const Point3 Up{0,0,1};
float dot3(Point3 a,Point3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point3 cross(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float span(Point3 v){return std::sqrt(dot3(v,v));}
Point3 unit(Point3 v,Point3 fallback=Point3{0,0,1}){const float l=span(v);return l>1e-6f?v*(1.f/l):fallback;}
float clamp01(float v){return std::clamp(v,0.f,1.f);}
float smooth(float a,float b,float x){const float t=clamp01((x-a)/(b-a));return t*t*(3-2*t);}
std::uint32_t mix32(std::uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return v;}
float rnd(std::uint32_t id,std::uint32_t k){return float(mix32(id^mix32(k*0x9e3779b9u+0x632be5abu))>>8)*(1.f/16777216.f);}
std::uint32_t bitsOf(float v){std::uint32_t b;std::memcpy(&b,&v,4);return b;}
std::uint32_t seedAt(Point3 p,float time,std::uint32_t salt){return mix32(bitsOf(std::round(p.x*512.f))*3u^bitsOf(std::round(p.y*512.f))*5u^bitsOf(std::round(p.z*512.f))*7u^bitsOf(std::round(time*240.f))*11u^salt);}

// Smooth value noise with its gradient, and the curl of it: air that swirls without bunching up.
struct Grad {float v=0;Point3 g{};};
float corner(int x,int y,int z,std::uint32_t salt){return float(mix32(std::uint32_t(x)*0x8da6b343u^std::uint32_t(y)*0xd8163841u^std::uint32_t(z)*0xcb1ab31fu^salt)>>8)*(2.f/16777216.f)-1.f;}
Grad noise(Point3 p,std::uint32_t salt){
 const float fx0=std::floor(p.x),fy0=std::floor(p.y),fz0=std::floor(p.z);const int ix=int(fx0),iy=int(fy0),iz=int(fz0);const float fx=p.x-fx0,fy=p.y-fy0,fz=p.z-fz0;
 const float u=fx*fx*(3-2*fx),v=fy*fy*(3-2*fy),w=fz*fz*(3-2*fz),du=6*fx*(1-fx),dv=6*fy*(1-fy),dw=6*fz*(1-fz);
 const float a=corner(ix,iy,iz,salt),b=corner(ix+1,iy,iz,salt),c=corner(ix,iy+1,iz,salt),d=corner(ix+1,iy+1,iz,salt),e=corner(ix,iy,iz+1,salt),f=corner(ix+1,iy,iz+1,salt),g=corner(ix,iy+1,iz+1,salt),h=corner(ix+1,iy+1,iz+1,salt);
 const float k1=b-a,k2=c-a,k3=e-a,k4=a-b-c+d,k5=a-c-e+g,k6=a-b-e+f,k7=-a+b+c-d+e-f-g+h;
 Grad r;r.v=a+k1*u+k2*v+k3*w+k4*u*v+k5*v*w+k6*w*u+k7*u*v*w;
 r.g={du*(k1+k4*v+k6*w+k7*v*w),dv*(k2+k5*w+k4*u+k7*w*u),dw*(k3+k6*u+k5*v+k7*u*v)};return r;}
Point3 curl(Point3 p,std::uint32_t salt){const Grad a=noise(p,salt),b=noise(p*2.03f+Point3{5.2f,1.3f,7.7f},salt^0x51ed27u);
 return {(a.g.y+.5f*b.g.y)*.7f,(-a.g.x-.5f*b.g.x)*.7f,.5f*(a.v+.5f*b.v)};}
float fbm(Point3 p,std::uint32_t salt,int octaves=4){float sum=0,amp=.55f;for(int o=0;o<octaves;++o){sum+=noise(p,salt+std::uint32_t(o)*977u).v*amp;p=p*2.07f+Point3{1.7f,9.2f,3.1f};amp*=.5f;}return sum;}
std::uint32_t pack(Point3 c,float a){auto q=[](float v){return std::uint32_t(std::clamp(v,0.f,1.f)*255.f+.5f);};return (q(a)<<24)|(q(c.x)<<16)|(q(c.y)<<8)|q(c.z);}

// The blended atlas: seven kinds of air and a fireball's burn, each in four shapes, each shape in six
// frames of its life (a card moves through them as it ages, blended frame to frame, so a puff rolls
// and opens out rather than only growing), and a row of solid bits. Alpha is the air's own density;
// how thick a card is laid on is its own opacity (CardLook), never a cell.
enum Row {RowSmoke,RowSoot,RowSteam,RowDust,RowToxic,RowMist,RowArc,RowFire,RowSolid,Rows};
static_assert(Rows==Particles::AtlasRows);
constexpr int Cell=Particles::AtlasCell,Frames=6,Variants=4,AtlasW=Cell*Variants*Frames,AtlasH=Cell*Rows;
enum Solid {SolidChunk0,SolidChunk3=3,SolidDrop,SolidGrit=6,SolidMote=7};
// The glow atlas: four flame tongues (64 x 128), an ember and a hot halo.
constexpr int GlowW=320,GlowH=128;
struct Rect {float u0,v0,u1,v1;};
Rect cellRect(int row,int column){const float x0=float(column*Cell)+.5f,y0=float(row*Cell)+.5f;return {x0/AtlasW,y0/AtlasH,(x0+Cell-1.f)/AtlasW,(y0+Cell-1.f)/AtlasH};}
Rect puffRect(int row,int variant,int frame){return cellRect(row,variant*Frames+frame);}
Rect glowRect(int which){ // 0-3 flames, 4 ember, 5 halo
 if(which<4){const float x0=float(which*64)+.5f;return {x0/GlowW,.5f/GlowH,(x0+63.f)/GlowW,127.5f/GlowH};}
 const float y0=which==4?.5f:64.5f;return {256.5f/GlowW,y0/GlowH,319.5f/GlowW,(y0+63.f)/GlowH};}

// A fireball's colour by its heat: soot, a deep red, orange, yellow, a white-yellow core.
Point3 burn(float heat){
 static const Point3 ramp[5]={{.10f,.09f,.085f},{.42f,.09f,.03f},{1.f,.42f,.08f},{1.f,.76f,.3f},{1.f,.95f,.78f}};
 const float x=std::clamp(heat,0.f,1.f)*4.f;const int i=std::min(3,int(x));const float t=x-float(i);return ramp[i]*(1-t)+ramp[i+1]*t;}

Texture makeBlended(){
 // (The look's values, LookTuning.h: how fine a steam card's threads are, how much thread and body it
 // has, how sharp; how far a billow is pushed about and eaten into as it ages; how deep its shadow.)
 const float steamFine=lookTune("steamFine",1.8f),steamThread=lookTune("steamThread",.75f),steamBody=lookTune("steamBody",.6f),steamSharp=lookTune("steamSharp",1.8f);
 const float billowPush=lookTune("billowPush",.35f),billowEat=lookTune("billowEat",.45f),billowShade=lookTune("billowShade",.6f),fireCool=lookTune("fireCool",1.3f);
 const float fireFolds=lookTune("fireFolds",.9f),fireCore=lookTune("fireCore",.45f);
 Texture t{AtlasW,AtlasH,std::vector<std::uint32_t>(size_t(AtlasW*AtlasH),0u)};t.clampEdges=true;t.transparent=true;t.soft=true;
 // (The gas a sickly yellow-green, greyed: a pure green read as a neon smear.)
 const Point3 tone[RowSolid]={{.5f,.5f,.51f},{.21f,.2f,.19f},{.92f,.93f,.95f},{.62f,.57f,.5f},{.56f,.66f,.42f},{.84f,.89f,.95f},{.62f,.76f,1.f},{1,1,1}};
 std::vector<float> density(size_t(Cell*Cell)),heat(size_t(Cell*Cell));
 for(int row=0;row<RowSolid;++row)for(int variant=0;variant<Variants;++variant){
  const std::uint32_t salt=std::uint32_t(row*16+variant)*0x9e3779b9u;
  // Each shape's lobes (a puff is a few rounded masses run together, not one disc), placed by its dice.
  Point3 lobe[5];float lobeSize[5];
  for(int k=0;k<5;++k){const float a=float(k)*1.2566f+rnd(salt,k)*1.1f,d=k==0?.05f:.26f+.16f*rnd(salt,10+k);lobe[k]={std::cos(a)*d,std::sin(a)*d*.85f-.04f,0};lobeSize[k]=k==0?.5f:.3f+.14f*rnd(salt,20+k);}
  for(int frame=0;frame<Frames;++frame){const float age=float(frame)/(Frames-1);
   for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x){const float dx=(x+.5f)/Cell*2-1,dy=(y+.5f)/Cell*2-1,r=std::sqrt(dx*dx+dy*dy);
    const Point3 p{dx,dy,float(variant)*7.3f+float(row)*3.1f};float d=0,h=0;
    if(row==RowArc)d=(std::exp(-r*r*14.f)+.3f*std::exp(-r*r*3.f))*(.85f+.15f*fbm(p*2.f,31u,2));
    else if(row==RowSteam||row==RowMist){
     // Vapour: fine filaments drawn out along the card's height (a jet is laid along its flight), the
     // envelope widening and the threads curling, thinning and parting as it ages.
     const float wide=row==RowSteam?.42f+.36f*age:.6f+.2f*age,tall=row==RowSteam?.95f:.8f;
     const float envelope=std::exp(-(dx*dx/(wide*wide)+dy*dy/(tall*tall))*1.7f);
     const Point3 warp{fbm(p*1.3f+Point3{0,age*.9f,0},41u,2),fbm(p*1.3f+Point3{3.7f,age*.9f,1.1f},43u,2),0};
     // (Threads a few to a card, soft-edged: finer, they read as hair once a card is a metre across.)
     const Point3 q{(dx+warp.x*(.25f+.55f*age))*(row==RowSteam?steamFine:2.f),(dy+warp.y*(.2f+.4f*age))*(row==RowSteam?.8f:1.5f),p.z+age*.6f};
     const float threads=1.f-std::fabs(fbm(q,47u,3)*1.6f),body=.5f+fbm(q*.6f,53u,3);
     const float thread=std::pow(std::clamp(threads,0.f,1.f),row==RowSteam?steamSharp+1.2f*age:1.6f);
     d=envelope*((row==RowSteam?steamBody:.65f)*std::clamp(body,0.f,1.f)*(1-.45f*age)+(row==RowSteam?steamThread:.45f)*thread)*(1-.4f*age);}
    else if(row==RowDust){
     // Dust: a soft, even drift with a fine grain in it.
     const float envelope=std::exp(-r*r*(2.4f-.8f*age));const float grain=.5f+fbm(p*3.4f,59u,4);
     d=envelope*(.5f+.6f*std::clamp(grain,0.f,1.f))*(1-.35f*age);}
    else{
     // Billows (smoke, soot, gas, a fireball): rounded lobes run together and pushed about by the air,
     // their edges eaten into as it ages, so a puff opens out into rags.
     const Point3 warp{fbm(p*1.7f+Point3{age*.7f,0,0},61u,3),fbm(p*1.7f+Point3{5.1f,age*.7f,2.3f},67u,3),0};
     const float push=.1f+billowPush*age,spread=1.f+.35f*age;const Point3 w{dx+warp.x*push,dy+warp.y*push,0};
     float empty=1.f;for(int k=0;k<5;++k){const Point3 c=lobe[k]*spread;const float ex=w.x-c.x,ey=w.y-c.y,s=lobeSize[k]*(1.f+.12f*age);empty*=1.f-std::exp(-(ex*ex+ey*ey)/(s*s)*1.6f);}
     const float mass=1.f-empty,detail=.5f+fbm(Point3{w.x*2.6f,w.y*2.6f,p.z+age*.8f},71u,4);
     const float eaten=.06f+billowEat*age*age+(row==RowFire?.12f*age:0.f);
     d=std::clamp(mass*(.62f+.75f*(detail-.5f)*(1.f+age))-eaten,0.f,1.f)*(1-.25f*age);
     d=std::pow(d,row==RowToxic?.8f:.9f);
     if(row==RowFire){
      // A fireball burns from the core out and down its life: white-yellow at the heart, orange
      // through its body, deep red at the rims and in its last frames soot, embers in the folds.
      const float folds=.5f+fbm(Point3{w.x*4.f,w.y*4.f,p.z+1.7f},79u,3);
      h=std::clamp(mass*(1.1f-fireCool*age)+(folds-.5f)*(fireFolds+.3f*age)-r*(fireCore+.35f*age)+.12f-.3f*age,0.f,1.f);
      d=std::clamp(d*(1.15f-.1f*age),0.f,1.f);}}
    d*=1-smooth(.82f,.98f,r);
    density[size_t(y*Cell+x)]=clamp01(d);heat[size_t(y*Cell+x)]=h;}
   // Lit as a body of air: its own thickness gives it a surface, lit from above and a little to the
   // side, rims thin and bright (light comes through them), the heart of a thick puff shadowed.
   const int x0=(variant*Frames+frame)*Cell,y0=row*Cell;
   for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x){const size_t i=size_t(y*Cell+x);
    auto at=[&](int ax,int ay){return density[size_t(std::clamp(ay,0,Cell-1)*Cell+std::clamp(ax,0,Cell-1))];};
    const float gx=(at(x+2,y)-at(x-2,y))*2.2f,gy=(at(x,y+2)-at(x,y-2))*2.2f;const Point3 n=unit(Point3{-gx,-gy,1.f});
    const float key=std::max(0.f,dot3(n,unit(Point3{-.35f,-.8f,.5f}))),dd=density[i];
    float shade;Point3 c;
    if(row==RowArc){shade=1.f;c=tone[row];}
    else if(row==RowFire){c=burn(heat[i]);shade=.75f+.35f*key;}
    else{const float thick=row==RowSteam||row==RowMist?.12f:row==RowSoot?.3f:billowShade;shade=(.62f+.42f*key)*(1.f-thick*dd*dd)+.14f*(1.f-dd);c=tone[row];}
    // Dithered by a texel of noise: a faint card's few alpha steps would otherwise show as contour rings.
    const float dither=(float(mix32(std::uint32_t((y0+y)*AtlasW+x0+x)*0x9e3779b9u)>>8)*(1.f/16777216.f)-.5f)/255.f;
    t.pixels[size_t((y0+y)*AtlasW+x0+x)]=pack(c*std::min(shade,1.25f),dd+(dd>.002f?dither:0.f));}}}
 // The solid row: four chips of rubble, two drops, a streak of falling grit, a speck of dust.
 for(int cell=0;cell<8;++cell){const int x0=cell*Cell,y0=RowSolid*Cell;
  for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x){const float dx=(x+.5f)/Cell*2-1,dy=(y+.5f)/Cell*2-1,r=std::sqrt(dx*dx+dy*dy),angle=std::atan2(dy,dx);Point3 c{};float a=0;
   if(cell<=SolidChunk3){const float edge=.62f+.18f*std::sin(angle*3.f+float(cell)*1.9f)+.1f*std::sin(angle*5.f+float(cell)*.7f);a=clamp01((edge-r)*14.f);
    const float grain=fbm(Point3{dx*4.f,dy*4.f,float(cell)},61u,3);c=Point3{.36f,.34f,.31f}*(.8f+.35f*grain+.2f*(-dy));}
   else if(cell<SolidGrit){const float bead=clamp01((.55f-r)*10.f);const float shine=clamp01(1-std::sqrt((dx+.25f)*(dx+.25f)+(dy+.3f)*(dy+.3f))*2.6f);
    a=bead*(cell==SolidDrop?.8f:.6f);c=Point3{.62f,.68f,.72f}*(.75f+.25f*(1-r))+Point3{1,1,1}*(shine*.45f);}
   else if(cell==SolidGrit){const float across=std::exp(-dx*dx*30.f),along=clamp01(1-std::fabs(dy));a=across*along*.85f;c={.62f,.58f,.5f};}
   else{a=std::exp(-r*r*5.f)*clamp01((1.f-r)*3.f);c={.95f,.9f,.8f};}
   t.pixels[size_t((y0+y)*AtlasW+x0+x)]=pack(c,a);}}
 return t;}
Texture makeGlow(){
 Texture t{GlowW,GlowH,std::vector<std::uint32_t>(size_t(GlowW*GlowH),0u)};t.clampEdges=true;t.additive=true;
 for(int f=0;f<4;++f)for(int y=0;y<128;++y)for(int x=0;x<64;++x){
  // A tongue of flame, its base on the last row (cards map their bottom edge to v=1): white-yellow at the
  // root, orange up its body, a dark red lick at the tip, the edge torn by noise.
  const float v=1-(y+.5f)/128,u=((x+.5f)/64-.5f)*2;const float tear=fbm(Point3{u*2.2f,v*3.4f,float(f)*5.f},71u,3);
  const float width=std::pow(1-v,.7f)*.9f+.06f+.18f*tear;float body=clamp01(1-std::fabs(u)/std::max(.05f,width));body=body*body*(3-2*body)*std::pow(clamp01(1-v),.5f)*clamp01(v*9.f);
  const float heat=clamp01(body*1.3f-.15f+.15f*tear-v*.35f);
  t.pixels[size_t(y*GlowW+f*64+x)]=pack({.55f+.45f*heat,.12f+.76f*heat*heat,.04f+.5f*heat*heat*heat},body);}
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){const float dx=(x-31.5f)/31.5f,dy=(y-31.5f)/31.5f,r=std::min(1.f,std::sqrt(dx*dx+dy*dy));
  const float core=std::exp(-r*r*14.f),halo=(1-r)*(1-r);
  t.pixels[size_t(y*GlowW+256+x)]=pack({1,.78f+.2f*core,.45f+.5f*core},clamp01(core+.35f*halo));
  t.pixels[size_t((y+64)*GlowW+256+x)]=pack({1,.86f,.66f},halo*halo*.8f);}
 return t;}

// What each kind of particle does in the air.
// Blaze is a fireball's burn (self-lit, burning down to soot); Haze the faint dust hanging in a lamp's light.
enum class Species:std::uint8_t {Smoke,Soot,Steam,Mote,Cloud,Grit,Chunk,Drop,Mist,Flame,Ember,Toxic,Ion,Blaze,Haze,Count};
struct Behaviour {
 float drag,lift,swirl,swirlScale,spin,fadeIn,fadeOut,peak,thin,scatter;int row;
 bool glow,ballistic,streak,hug;float cone;
};
const Behaviour& behaviour(Species s){
 //                         drag  lift  swirl scale spin  in    out   peak  thin  scatter row        glow  ball  streak hug  cone
 static const Behaviour table[int(Species::Count)]={
  /*Smoke*/ {.7f, .3f, .55f,.8f, .2f, .1f, .5f, 1.f, .5f, .8f, RowSmoke,false,false,false,false,.45f},
  /*Soot*/  {1.1f,.22f,.7f, .6f, .15f,.04f,.45f,1.f, .45f,.45f,RowSoot, false,false,false,false,.9f},
  /*Steam*/ {2.4f,1.f, .3f, 1.6f,.45f,.02f,.22f,.84f,.9f, .75f,RowSteam,false,false,false,false,.2f},
  /*Mote*/  {4.f, 0.f, .22f,.7f, 0.f, .25f,.65f,1.f, 0.f, 2.4f,RowDust, false,false,false,false,3.2f},
  /*Cloud*/ {2.2f,.06f,.35f,1.1f,.25f,.03f,.35f,.8f, .7f, .6f, RowDust, false,false,false,false,1.f},
  /*Grit*/  {3.f, -9.8f,0.f,1.f, 0.f, 0.f, .75f,.9f, 0.f, .5f, RowSolid,false,true, true, false,.4f},
  /*Chunk*/ {.2f, -9.8f,0.f,1.f, 9.f, 0.f, .9f, 1.f, 0.f, 0.f, RowSolid,false,true, false,false,1.1f},
  /*Drop*/  {.1f, -9.8f,0.f,1.f, 0.f, 0.f, .95f,.9f, 0.f, .6f, RowSolid,false,true, true, false,.15f},
  /*Mist*/  {3.f, .15f,.25f,2.f, .3f, .02f,.3f, .6f, 1.f, 1.6f,RowMist, false,false,false,false,.25f},
  /*Flame*/ {1.8f,2.2f,.12f,2.5f,.6f, .08f,.45f,1.f, 0.f, 0.f, 0,       true, false,false,false,.35f},
  /*Ember*/ {1.3f,1.1f,.4f, 1.8f,0.f, .02f,.6f, 1.f, 0.f, 0.f, 0,       true, false,false,false,.6f},
  /*Toxic*/ {1.6f,-.05f,.45f,.5f,.1f, .15f,.6f, .55f,.4f, .9f, RowToxic,false,false,false,true, 1.4f},
  /*Ion*/   {2.f, .2f, .3f, 2.f, .4f, .1f, .5f, .7f, 0.f, 0.f, RowArc,  false,false,false,false,3.2f},
  /*Blaze*/ {3.2f,1.1f,.3f, 1.4f,.5f, .02f,.45f,1.f, .1f, 0.f, RowFire, false,false,false,false,1.3f},
  /*Haze*/  {5.f, .02f,.12f,.45f,.04f,.3f, .7f, .55f,0.f, 2.2f,RowDust, false,false,false,false,3.2f}};
 return table[int(s)];}

// Each kind by name, for the look's values (LookTuning.h).
const char*const speciesNames[int(Species::Count)]={"smoke","soot","steam","mote","cloud","grit","chunk","drop","mist","flame","ember","toxic","ion","blaze","haze"};
// How thick each kind is laid on, over its table's peak (the look's values, LookTuning.h: peak.steam...).
float peakScale(Species s){
 static const auto scales=[]{std::array<float,int(Species::Count)> out{};for(int i=0;i<int(Species::Count);++i)out[size_t(i)]=lookTune((std::string("peak.")+speciesNames[i]).c_str(),1.f);return out;}();
 return scales[size_t(s)];}
// And how far each kind swells over its life, over its source's own (grow.steam...): vapour to well
// under half what a leak or a jet asks, so it frays into threads rather than banking up into a cloud.
float growScale(Species s){
 static const auto scales=[]{std::array<float,int(Species::Count)> out{};for(int i=0;i<int(Species::Count);++i)out[size_t(i)]=lookTune((std::string("grow.")+speciesNames[i]).c_str(),Species(i)==Species::Steam?.42f:1.f);return out;}();
 return scales[size_t(s)];}
// A source of particles for this frame: a map's EMITTER or hazard, or something the game threw.
struct Gate {StateId flag=0;int value=1;bool invert=false;float period=0,onTime=0,phase=0;std::uint64_t key=0;bool used=false;};
struct Source {
 Species species=Species::Smoke;std::uint32_t seed=1;
 Point3 at{},box{};      // where particles are born: the middle of the box, and its half sizes
 Point3 travel{0,0,1};   // drifting kinds: where one goes over its life on drag alone; ballistic kinds: how fast it leaves
 float cone=-1;          // half angle of the spread round `travel` (radians; -1 the kind's own)
 bool radial=false;      // thrown out level in every direction (a blast's ring along the floor)
 float size=.1f,grow=.3f,life=2,strength=1;
 float rate=0;int count=0;float t0=0,spread=0,until=1e30f; // a stream of `rate` a second (from t0 until `until`), or a burst of `count` over `spread` s
 float floor=-1e9f;
 float tall=1;           // the cards' height to their width (a dust ring's are low and wide)
 const std::vector<ParticleCrumb>* path=nullptr; // born along a thrown thing's flight instead of in the box
 Gate gate;
};
}

std::shared_ptr<ParticleMemory> makeParticleMemory(){return std::make_shared<ParticleMemory>();}
const std::vector<BarrelExplosion>& particleExtraBlasts(const ParticleMemory& memory){return memory.extra.blasts;}
ParticleMemory& SoftwareRenderer::particleMemory(){if(!m_particleMemory)m_particleMemory=makeParticleMemory();return *m_particleMemory;}

bool Particles::open(const World& w,Point3 p){
 if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.x<.02f||p.y<.02f||p.x>23.98f||p.y>23.98f)return false;
 return w.fits(p.x,p.y,p.z,.01f,false,false);}
const Texture& Particles::blendedAtlas(){static const Texture made=makeBlended();return made;}
const Texture& Particles::glowAtlas(){static const Texture made=makeGlow();return made;}

namespace {
// Which map, as built: its content's revision (a live edit bumps it), never where its moving parts are
// this frame (a lift or a swinging fixture would forget every patch of air each frame; a card that
// brushes one fades where it meets it).
std::uint64_t worldSignature(const Game& game,const World& w){std::uint64_t h=1469598103934665603ull;auto add=[&](std::uint64_t v){h^=v;h*=1099511628211ull;};
 add(w.structures().size());add(w.fixtures().size());add(w.layers().size());add(std::uint64_t(w.level()));add(game.staticRevision(w.level()));
 add(w.particleEmitters().size());add(w.hazards().size());
 add(std::uint64_t(reinterpret_cast<std::uintptr_t>(w.definitionIdentity().map)));
 return h;}
// How far the air round `p` reaches before something solid, along the axes and the level diagonals;
// -1 inside a solid. Kept by 25 cm cell (taken at its middle, less the cell's own reach). Probed only as
// far as `need` asks: a cell remembers how far it has been looked and whether a solid stopped the look,
// so a bigger card later looks further, and the answer is the same whichever card asked first.
struct Probe {float value=0;bool settled=false;};
float clearance(ParticleMemory& memory,const World& w,std::uint64_t signature,Point3 p,float need,ParticleStats& stats){
 auto&level=memory.clearance[w.level()];
 if(level.signature!=signature){level.cells.clear();level.signature=signature;level.boxes.clear();level.bins.assign(24*24,{});
  auto add=[&](float x1,float y1,float z1,float x2,float y2,float z2){if(x2<=x1||y2<=y1||z2<=z1)return;const auto index=std::uint32_t(level.boxes.size());level.boxes.push_back({x1,y1,z1,x2,y2,z2});
   for(int y=std::max(0,int(std::floor(y1)));y<=std::min(23,int(std::floor(y2)));++y)for(int x=std::max(0,int(std::floor(x1)));x<=std::min(23,int(std::floor(x2)));++x)level.bins[size_t(y*24+x)].push_back(index);};
  for(const auto&b:w.structures())if(b.solid&&b.shape==ShapeBox)add(b.x1,b.y1,b.bottom,b.x2,b.y2,b.top);
  for(const auto&layer:w.layers())if(layer.thickness>0)for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(size_t(x)<layer.rows[size_t(y)].size()&&layer.rows[size_t(y)][size_t(x)]=='=')
   add(float(x),float(y),layer.elevation-layer.thickness,float(x+1),float(y+1),layer.elevation);
  for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(w.tile(x,y)=='#')add(float(x),float(y),w.floorHeight(x+.5f,y+.5f)-1.f,float(x+1),float(y+1),w.wallHeight(x,y));
  for(const auto&f:w.fixtures())if(f.solid&&f.visible){const float c=std::fabs(std::cos(f.yaw)),sn=std::fabs(std::sin(f.yaw)),hx=.5f*(f.width*c+f.depth*sn)*f.core,hy=.5f*(f.width*sn+f.depth*c)*f.core,base=w.floorHeight(f.position.x,f.position.y)+f.base;
   add(f.position.x-hx,f.position.y-hy,base,f.position.x+hx,f.position.y+hy,base+f.height);}}
 // The exact distance from `at` to the nearest of those boxes within `limit` (0 inside one).
 auto boxRoom=[&](Point3 at,float limit){float best=limit;
  for(int y=std::max(0,int(std::floor(at.y-limit)));y<=std::min(23,int(std::floor(at.y+limit)));++y)for(int x=std::max(0,int(std::floor(at.x-limit)));x<=std::min(23,int(std::floor(at.x+limit)));++x)
   for(auto index:level.bins[size_t(y*24+x)]){const auto&b=level.boxes[index];const float dx=std::max({b[0]-at.x,0.f,at.x-b[3]}),dy=std::max({b[1]-at.y,0.f,at.y-b[4]}),dz=std::max({b[2]-at.z,0.f,at.z-b[5]});
    best=std::min(best,std::sqrt(dx*dx+dy*dy+dz*dz));}
  return best;};
 // A big card asks a big question of a coarse cell (50 cm): smoke that swells through a hall would
 // otherwise probe every 25 cm of it.
 const bool coarse=need>.4f;const float CellSize=coarse?.5f:.25f,Reach=CellSize*.87f;constexpr float Most=2.2f;const int cx=int(std::floor(p.x/CellSize)),cy=int(std::floor(p.y/CellSize)),cz=int(std::floor(p.z/CellSize));
 const std::uint64_t key=(coarse?1ull<<48:0ull)|(std::uint64_t(std::uint16_t(cx+512))<<32)|(std::uint64_t(std::uint16_t(cy+512))<<16)|std::uint64_t(std::uint16_t(cz+32768));
 static const float steps[]={.06f,.12f,.2f,.3f,.42f,.58f,.8f,1.08f,1.6f,2.2f};
 const float want=std::min(Most,need+Reach);
 auto found=level.cells.find(key);float value=0,box=-1;bool settled=false;
 if(found!=level.cells.end()){value=found->second.value;settled=found->second.settled;box=found->second.box;}
 if(found==level.cells.end()||(!settled&&value<want)){
  const Point3 q{(cx+.5f)*CellSize,(cy+.5f)*CellSize,(cz+.5f)*CellSize};
  if(found==level.cells.end()&&!Particles::open(w,q)){value=-1;settled=true;}
  else{static const auto directions=[]{std::array<Point3,26> out{};int n=0;for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)if(x||y||z)out[size_t(n++)]=unit(Point3{float(x),float(y),float(z)});return out;}();
   const float from=found==level.cells.end()?0.f:value;float free=from;settled=false;
   for(float d:steps){if(d<=from)continue;bool blocked=false;for(Point3 dir:directions){++stats.clearanceProbes;if(!Particles::open(w,q+dir*d)){blocked=true;break;}}
    if(blocked){settled=true;break;}free=d;if(d>=want)break;}
   value=free;if(value>=Most)settled=true;}
  if(level.cells.size()>400000)level.cells.clear();level.cells[key]={value,settled,box};}
 if(value<0)return Particles::open(w,p)?0.f:-1.f;
 // The boxes: how far the nearest is from the cell's middle is kept with the cell; only a card that
 // could reach one from where it is asks the exact distance (most of the air is nowhere near one).
 if(box<0){box=boxRoom({(cx+.5f)*CellSize,(cy+.5f)*CellSize,(cz+.5f)*CellSize},Most+Reach);level.cells[key].box=box;}
 return std::min(std::max(0.f,value-Reach),box-Reach>=need?need:boxRoom(p,need));}

// Where a thing thrown at `v0` from `p0` (accelerated by `g`, slowed by `k`) is `t` seconds on.
Point3 motion(Point3 p0,Point3 v0,Point3 g,float k,float t){
 if(k<1e-4f)return p0+v0*t+g*(.5f*t*t);
 const float e=std::exp(-k*t),a=(1-e)/k;return p0+v0*a+g*((t-a)/k);}
Point3 velocityAt(Point3 v0,Point3 g,float k,float t){if(k<1e-4f)return v0+g*t;const float e=std::exp(-k*t);return v0*e+g*((1-e)/k);}

// The flight of something thrown: off a wall it comes back weakly, off the floor it bounces once (a chip)
// or splashes (a drop) or stops (grit), then lies where it came to rest. Worked out once, then kept.
const ParticleMemory::Path& flight(ParticleMemory& memory,const World& w,std::uint64_t key,Species sp,Point3 p0,Point3 v0,Point3 g,float k,float life){
 if(auto found=memory.paths.find(key);found!=memory.paths.end())return found->second;
 if(memory.paths.size()>60000)memory.paths.clear();
 ParticleMemory::Path path;path.from[0]=p0;path.velocity[0]=v0;path.start[0]=0;path.arcs=1;
 auto blocked=[&](Point3 q){if(!Particles::open(w,q))return true;const float water=w.waterSurface(q.x,q.y);return water>-100&&q.z<water;};
 float t=0;int arc=0;
 while(arc<3){const Point3 from=path.from[size_t(arc)],v=path.velocity[size_t(arc)];const float begin=path.start[size_t(arc)];
  float hit=-1;float last=0;constexpr float Step=.04f;
  if(blocked(from)){path.rest=from;path.restAt=begin;break;}
  for(float s=Step;begin+s<=life+Step;s+=Step){if(blocked(motion(from,v,g,k,s))){hit=s;break;}last=s;}
  if(hit<0){path.restAt=1e9f;break;}
  float lo=last,hi=hit;for(int i=0;i<6;++i){const float mid=(lo+hi)*.5f;if(blocked(motion(from,v,g,k,mid)))hi=mid;else lo=mid;}
  const Point3 at=motion(from,v,g,k,lo),moving=velocityAt(v,g,k,lo);t=begin+lo;
  const bool floor=moving.z<0&&blocked(at+Point3{0,0,-.06f})&&!blocked(at+Point3{0,0,.04f});
  if(floor){
   if(sp==Species::Drop){path.rest=at;path.restAt=t;path.splash=true;break;}
   if(sp!=Species::Chunk||arc>=2||std::fabs(moving.z)<1.2f){path.rest=at+Point3{0,0,.012f};path.restAt=t;break;}
   ++arc;path.arcs=arc+1;path.from[size_t(arc)]=at+Point3{0,0,.01f};path.velocity[size_t(arc)]={moving.x*.4f,moving.y*.4f,-moving.z*.28f};path.start[size_t(arc)]=t;continue;}
  // A wall: back off it the way it came, weakly, and fall.
  Point3 n{0,0,0};for(Point3 d:{Point3{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}})if(blocked(at+d*.06f))n=n-d;
  n=unit(n,unit(Point3{-moving.x,-moving.y,0},Point3{1,0,0}));
  if(arc>=2){path.rest=at;path.restAt=t;break;}
  const float into=dot3(moving,n);Point3 out=moving-n*(1.3f*into);out=Point3{out.x*.3f,out.y*.3f,std::min(out.z,0.f)*.5f};
  ++arc;path.arcs=arc+1;path.from[size_t(arc)]=at+n*.02f;path.velocity[size_t(arc)]=out;path.start[size_t(arc)]=t;}
 return memory.paths.emplace(key,path).first->second;}

struct Live {Species sp;std::uint32_t id;Point3 c{},velocity{};float r=0,alpha=0,turn=0,u=0,light=1,depth=0,tall=1;int variant=0,cell=-1;bool streak=false,resting=false,flat=false;};
// Where along a thrown thing's flight it was at `time` (held at the ends).
Point3 alongPath(const std::vector<ParticleCrumb>& path,float time){
 if(path.empty())return {};if(time<=path.front().time)return path.front().at;if(time>=path.back().time)return path.back().at;
 const auto next=std::upper_bound(path.begin(),path.end(),time,[](float t,const ParticleCrumb& c){return t<c.time;});const auto&b=*next,&a=*(next-1);
 const float t=(time-a.time)/std::max(1e-5f,b.time-a.time);return a.at+(b.at-a.at)*t;}

struct Frame {
 ParticleMemory& memory;const Game& game;const World& w;const Particles::View& view;ParticleStats& stats;
 std::vector<Live> blended,glowing;
 bool gateOpen(const Gate& g,float born){
  if(!g.used)return true;
  if(g.period>0&&!(std::fmod(std::fmod(born+g.phase,g.period)+g.period,g.period)<g.onTime))return false;
  if(!g.flag)return true;
  const auto found=memory.gates.find(g.key);if(found==memory.gates.end())return false;const auto&m=found->second;
  return m.open?born>=m.openedAt:(born>=m.earlier&&born<m.closedAt);}
 void noteGate(Gate& g){
  if(!g.used||!g.flag)return;bool open=game.state(g.flag)==g.value;if(g.invert)open=!open;auto&m=memory.gates[g.key];const float now=view.now;
  if(!m.seen){m.seen=true;m.open=open;m.openedAt=-1e9f;m.closedAt=open?-1e9f:-1e9f;return;}
  if(open&&!m.open){m.open=true;m.openedAt=now;}
  else if(!open&&m.open){m.open=false;m.earlier=m.openedAt;m.closedAt=now;}}
 // Every particle of `s` alive now, `keep` of them (the rest thinned by their own dice).
 void evaluate(const Source& s,float keep){
  const Behaviour& b=behaviour(s.species);const float now=view.now;
  auto one=[&](std::uint32_t id,float born){
   const float life=s.life*(.75f+.5f*rnd(id,2)),age=now-born;if(age<0||age>=life)return;
   if(!gateOpen(s.gate,born))return;
   if(rnd(id,3)>=keep){++stats.thinned;return;}
   const float u=age/life;
   // Its own way out of the source: round `travel` within the cone, or level round the compass.
   Point3 base=s.radial?Point3{std::cos(rnd(id,4)*kTwoPi),std::sin(rnd(id,4)*kTwoPi),.04f}:unit(s.travel,Up);
   const float cone=s.cone>=0?s.cone:b.cone,theta=cone*std::sqrt(rnd(id,5)),phi=rnd(id,6)*kTwoPi;
   const Point3 t1=unit(std::fabs(base.z)<.9f?cross(base,Up):cross(base,Point3{1,0,0}),{1,0,0}),t2=cross(base,t1);
   const Point3 dir=unit(base*std::cos(theta)+(t1*std::cos(phi)+t2*std::sin(phi))*std::sin(theta),base);
   const float reach=span(s.travel)*(.7f+.6f*rnd(id,7));
   // (Born along a flight, a puff leaves within a few centimetres of where the thing was; the box is then
   // only the flight's reach.)
   const Point3 within=s.path?Point3{.025f,.025f,.025f}:s.box;
   const Point3 spawn=(s.path?alongPath(*s.path,born):s.at)+Point3{within.x*(2*rnd(id,8)-1),within.y*(2*rnd(id,9)-1),within.z*(2*rnd(id,10)-1)};
   Live p;p.sp=s.species;p.id=id;p.u=u;p.variant=int(rnd(id,11)*Variants)%Variants;p.turn=rnd(id,12)*kTwoPi+b.spin*(rnd(id,13)*2-1)*age;p.tall=s.tall;p.flat=s.radial;
   // (A big billow, a cooling tower's vapour, keeps the swell it was given: only a small fast leak or
   // jet is held to a few times its own width.)
   const float swell=std::min(s.grow,std::max(s.grow*growScale(s.species),s.size*2.5f));
   p.r=std::max(.002f,(s.size+swell*(1-(1-u)*(1-u)))*(.8f+.4f*rnd(id,14)));
   p.alpha=b.peak*peakScale(s.species)*s.strength*smooth(0,std::max(1e-3f,b.fadeIn),u)*(1-smooth(b.fadeOut,1,u))*(1-b.thin*.45f*u);
   // Thinned (far off, or the budget short), what is left is drawn larger and thicker: the air keeps
   // its body with fewer cards.
   if(keep<1&&!b.ballistic&&!b.glow){p.r*=std::pow(1.f/std::max(keep,.12f),.3f);p.alpha*=std::min(1.9f,std::pow(1.f/std::max(keep,.12f),.5f));}
   if(b.ballistic){
    const Point3 g{0,0,b.lift},v0=dir*reach;
    const auto&path=flight(memory,w,(std::uint64_t(s.seed)<<32)|id,s.species,spawn,v0,g,b.drag,life);
    if(age>=path.restAt){
     if(path.splash){const float since=age-path.restAt;if(since>.3f)return;
      // A drop lands: a breath of spray and two beads thrown up out of it.
      Live mist=p;mist.sp=Species::Mist;mist.c=path.rest+Point3{0,0,.03f+.08f*since};mist.r=.015f+.06f*since;mist.alpha=.3f*(1-since/.3f)*s.strength;mist.variant=p.variant;blended.push_back(mist);
      for(int k=0;k<2;++k){const float a=rnd(id,40+k)*kTwoPi;Live bead=p;bead.c=path.rest+Point3{std::cos(a)*.25f*since,std::sin(a)*.25f*since,1.4f*since-4.9f*since*since+.01f};if(bead.c.z<path.rest.z)continue;
       bead.r=p.r*.6f;bead.velocity={std::cos(a)*.25f,std::sin(a)*.25f,1.4f-9.8f*since};bead.streak=true;bead.cell=SolidDrop+1;blended.push_back(bead);}
      return;}
     if(s.species==Species::Grit)return;
     p.c=path.rest+Point3{0,0,p.r*.45f};p.resting=true;p.turn=rnd(id,12)*kTwoPi;p.alpha*=1-smooth(.85f,1.f,u);}
    else{int arc=0;while(arc+1<path.arcs&&age>=path.start[size_t(arc+1)])++arc;const float since=age-path.start[size_t(arc)];
     p.c=motion(path.from[size_t(arc)],path.velocity[size_t(arc)],g,b.drag,since);p.velocity=velocityAt(path.velocity[size_t(arc)],g,b.drag,since);}
    p.streak=b.streak&&!p.resting;
    p.cell=s.species==Species::Chunk?int(rnd(id,15)*4)%4:s.species==Species::Drop?SolidDrop:SolidGrit;
    blended.push_back(p);return;}
   // Drifting: thrown along `dir` and slowed by the air, lifted by its heat (or sinking), turned by the swirl.
   const float k=b.drag,nominal=s.life;Point3 v0=k>1e-4f?dir*(reach*k/std::max(1e-3f,1-std::exp(-k*nominal))):dir*(reach/nominal);
   const Point3 g{0,0,b.lift};Point3 c=motion(spawn,v0,g,k,age);
   c=c+curl(c*b.swirlScale+Point3{0,0,-now*.22f},s.seed&0xffu)*(b.swirl*std::pow(u,.7f));
   p.velocity=velocityAt(v0,g,k,age);
   if(b.hug&&s.floor>-1e8f)c.z=std::max(c.z,s.floor+.5f*p.r);
   p.c=c;
   if(b.glow){
    // Fire flickers; an ember cools from white-yellow to a dull red dot.
    const float flicker=.78f+.22f*std::sin(now*(19.f+7.f*rnd(id,16))+rnd(id,17)*9.f);
    if(s.species==Species::Flame){p.cell=p.variant;p.alpha*=flicker;p.r*=1.f-.35f*u;}
    else{p.cell=4;p.alpha*=(1-u*.7f)*(.6f+.4f*flicker);}
    glowing.push_back(p);return;}
   blended.push_back(p);};
  if(s.rate>0){const float R=s.rate,from=std::max(now-s.life*1.3f,s.t0),to=std::min(now,s.until);if(to<from)return;
   const auto first=std::int64_t(std::floor(from*R))-1,last=std::int64_t(std::floor(to*R))+1;
   for(auto n=first;n<=last;++n){const std::uint32_t id=mix32(s.seed^std::uint32_t(n)*0x9e3779b9u^std::uint32_t(std::uint64_t(n)>>32)*0x85ebca6bu);
    const float born=(float(n)+.8f*(rnd(id,1)-.5f))/R;if(born<s.t0||born>s.until)continue;one(id,born);}}
  else for(int i=0;i<s.count;++i){const std::uint32_t id=mix32(s.seed^std::uint32_t(i)*0x9e3779b9u^0x2545f491u);one(id,s.t0+rnd(id,1)*s.spread);}}
};

// How many of `s` are alive at once, about.
float alive(const Source& s){return s.rate>0?s.rate*s.life:float(s.count);}
// The box everything of `s` can reach.
void reachOf(const Source& s,Point3& lo,Point3& hi){
 const Behaviour& b=behaviour(s.species);const float t=span(s.travel)*(b.ballistic?.9f:1.3f)+std::max(0.f,s.size+s.grow)+b.swirl+std::max(0.f,b.lift)*s.life*.6f+.2f;
 const float down=(b.ballistic||b.lift<0)&&s.floor>-1e8f?std::max(t,s.at.z-s.floor):t;lo=s.at-s.box-Point3{t,t,down};hi=s.at+s.box+Point3{t,t,t};
 if(b.ballistic&&s.floor>-1e8f)lo.z=std::min(lo.z,s.floor-.1f);}

// The map's own sources, the same every frame (its EMITTER records and its hazards; their gates are
// read as each particle is born): worked out once a map and kept (ParticleMemory::fixed), since
// finding their floors and ceilings in a map of a thousand structures cost a millisecond a frame.
void gatherFixed(const World& w,int level,std::vector<Source>& out){
 const std::uint32_t levelSalt=std::uint32_t(level+1)*0x27d4eb2fu;
 auto floorUnder=[&](Point3 p){return w.supportBelow(p.x,p.y,p.z+.05f);};
 // The map's own emitters.
 for(size_t i=0;i<w.particleEmitters().size();++i){const auto&e=w.particleEmitters()[i];if(e.kind==ParticleKind::Sparks)continue;
  Source s;s.seed=mix32(levelSalt^std::uint32_t(i)*0x165667b1u^std::uint32_t(e.kind)*0x9e37u);s.at={e.position.x,e.position.y,e.z};s.travel={e.drift.x,e.drift.y,e.rise};
  s.size=e.radius;s.grow=e.growth;s.life=1.f/std::max(.02f,e.rate);s.strength=e.strength;s.rate=float(e.count)/s.life;s.t0=-1e30f;s.floor=floorUnder(s.at);
  s.gate={e.enabledFlag,e.enabledValue,e.invertFlag,e.period,e.onTime,e.phase,(std::uint64_t(level+1)<<40)|std::uint64_t(i),e.enabledFlag||e.period>0};
  switch(e.kind){
   case ParticleKind::Plume:s.species=w.zoneSky()?Species::Smoke:Species::Steam;s.rate*=3.f;s.strength*=w.zoneSky()?.8f:.75f;out.push_back(s);break;
   case ParticleKind::Smoke:s.species=Species::Smoke;out.push_back(s);break;
   case ParticleKind::Steam:s.species=Species::Steam;out.push_back(s);break;
   case ParticleKind::Dust:{
    // Dust fills a box: driftX, driftY and rise are its half width, half depth and height; it barely moves.
    s.species=Species::Mote;s.box={std::fabs(e.drift.x),std::fabs(e.drift.y),std::fabs(e.rise)*.5f};s.at.z+=std::fabs(e.rise)*.5f;s.travel={.05f,.03f,.02f};out.push_back(s);
    // And a faint haze through the same box, a few big slow cards that glow where a lamp stands behind them.
    Source haze=s;haze.species=Species::Haze;haze.seed^=0x3c6ef372u;haze.box=s.box*.7f;haze.travel={.08f,.05f,.06f};haze.size=std::clamp(std::min(s.box.x,s.box.y)*.9f,.3f,.9f);haze.grow=.3f;haze.tall=1.7f;
    haze.life=9;haze.rate=std::clamp(s.box.z*1.6f,3.f,8.f)/haze.life;haze.strength=e.strength*.8f;out.push_back(haze);break;}
   case ParticleKind::Debris:{
    // A crack that lets go: grit pours, now and then a chip, and dust where it lands.
    Source grit=s;grit.species=Species::Grit;grit.travel={e.drift.x,e.drift.y,0};grit.size=std::max(.004f,e.radius*.3f);grit.grow=0;grit.box={.12f,.12f,0};out.push_back(grit);
    Source chip=s;chip.species=Species::Chunk;chip.seed^=0x5bd1e995u;chip.rate=std::max(.15f,s.rate*.06f);chip.life=std::max(3.f,s.life*2.f);chip.size=std::max(.02f,e.radius);chip.grow=0;chip.travel={e.drift.x,e.drift.y,-.3f};out.push_back(chip);
    Source dust=s;dust.species=Species::Cloud;dust.seed^=0x68e31da4u;dust.at.z=s.floor+.15f;dust.rate=std::max(.4f,s.rate*.12f);dust.life=3;dust.size=.08f;dust.grow=.5f;dust.travel={0,0,.25f};dust.strength*=.6f;out.push_back(dust);break;}
   case ParticleKind::Drips:{s.species=Species::Drop;s.travel={e.drift.x,e.drift.y,0};s.size=std::max(.006f,e.radius);s.grow=0;s.life=std::max(s.life,1.5f);out.push_back(s);break;}
   case ParticleKind::Spray:{Source mist=s;mist.species=Species::Mist;out.push_back(mist);
    Source drops=s;drops.species=Species::Drop;drops.seed^=0x1b873593u;drops.travel=s.travel*(2.2f/std::max(.2f,s.life));drops.rate=s.rate*.6f;drops.size=.008f;drops.grow=0;drops.life=1.6f;out.push_back(drops);break;}
   case ParticleKind::Fire:{
    // radius is the fire's half width, growth how tall its flames stand.
    const float half=std::max(.08f,e.radius),tall=std::max(.2f,e.growth);
    Source flame=s;flame.species=Species::Flame;flame.box={half*.7f,half*.7f,.02f};flame.travel={e.drift.x*.2f,e.drift.y*.2f,tall*.6f};flame.size=std::min(.45f,half*.5f+tall*.12f);flame.grow=-flame.size*.2f;flame.life=.75f;flame.rate=std::clamp(float(e.count)*2.f,8.f,60.f);out.push_back(flame);
    Source ember=s;ember.species=Species::Ember;ember.seed^=0x85ebca6bu;ember.box=flame.box;ember.travel={e.drift.x,e.drift.y,tall*1.4f+.6f};ember.size=.012f;ember.grow=-.004f;ember.life=2.4f;ember.rate=std::clamp(float(e.count)*.4f,2.f,14.f);out.push_back(ember);
    Source smoke=s;smoke.species=Species::Soot;smoke.seed^=0xc2b2ae35u;smoke.at.z+=tall*.9f;smoke.box={half*.4f,half*.4f,.05f};smoke.travel={e.drift.x,e.drift.y,2.4f+tall};smoke.size=half*.5f;smoke.grow=1.f+half;smoke.life=6;smoke.rate=std::clamp(float(e.count)*.12f,1.f,6.f);smoke.strength*=.5f;out.push_back(smoke);break;}
   case ParticleKind::Toxic:{
    // Gas lies in a pool: driftX and driftY its half width and depth, rise how deep the layer is.
    s.species=Species::Toxic;s.box={std::fabs(e.drift.x),std::fabs(e.drift.y),.03f};s.at.z=s.floor+.08f;s.travel={.25f,.1f,std::max(.05f,std::fabs(e.rise)*.4f)};s.cone=2.6f;out.push_back(s);break;}
   case ParticleKind::ArcGlow:{s.species=Species::Ion;s.box={e.radius*2,e.radius*2,e.radius};s.size=std::max(.02f,e.radius*.22f);s.grow=std::min(s.grow,.05f);out.push_back(s);
    Source ozone=s;ozone.species=Species::Smoke;ozone.seed^=0x94d049bbu;ozone.rate=std::max(.3f,s.rate*.15f);ozone.life=4;ozone.size=.08f;ozone.grow=.5f;ozone.travel={0,0,.8f};ozone.strength*=.3f;out.push_back(ozone);break;}
   default:break;}}
 // The hazards show what they are, in their own boxes, while they are on. A field laid as many boxes
 // of one kind on one switch (a flooded hall's live floor, tile by tile) is shown as the few
 // rectangles it makes, its air by the whole field's area: a live floor of seventy tiles drew seventy
 // sources' worth of ions and ozone, a thousand cards and five milliseconds a frame.
 const auto&hazards=w.hazards();std::vector<Hazard> fields;std::vector<size_t> firstOf;std::vector<float> areas;std::vector<char> taken(hazards.size(),0);
 auto sameSwitch=[](const Hazard&a,const Hazard&b){return a.kind==b.kind&&a.enabledFlag==b.enabledFlag&&a.enabledValue==b.enabledValue&&a.invertFlag==b.invertFlag&&a.period==b.period&&a.onTime==b.onTime&&a.phase==b.phase&&std::fabs(a.bottom-b.bottom)<.3f&&std::fabs(a.top-b.top)<.3f;};
 for(size_t i=0;i<hazards.size();++i){if(taken[i])continue;taken[i]=1;Hazard box=hazards[i];float area=(box.x2-box.x1)*(box.y2-box.y1);
  for(bool grew=true;grew;){grew=false;
   for(size_t j=i+1;j<hazards.size();++j){const auto&o=hazards[j];if(taken[j]||!sameSwitch(o,box)||o.x1>box.x2+.1f||o.x2<box.x1-.1f||o.y1>box.y2+.1f||o.y2<box.y1-.1f)continue;
    const float x1=std::min(box.x1,o.x1),y1=std::min(box.y1,o.y1),x2=std::max(box.x2,o.x2),y2=std::max(box.y2,o.y2),sum=area+(o.x2-o.x1)*(o.y2-o.y1);
    if((x2-x1)*(y2-y1)>sum*1.3f)continue;box.x1=x1;box.y1=y1;box.x2=x2;box.y2=y2;area=sum;taken[j]=1;grew=true;}}
  fields.push_back(box);firstOf.push_back(i);areas.push_back(area);}
 for(size_t f=0;f<fields.size();++f){const auto&h=fields[f];const size_t i=firstOf[f];
  const Point3 centre{(h.x1+h.x2)*.5f,(h.y1+h.y2)*.5f,0};const float floor=w.supportBelow(centre.x,centre.y,std::min(h.top,w.clearanceHeight(centre.x,centre.y)-.05f));
  const float hw=(h.x2-h.x1)*.5f,hd=(h.y2-h.y1)*.5f,area=areas[f],bottom=std::max(h.bottom,floor),top=std::min(h.top,floor+3.2f);
  Source s;s.seed=mix32(levelSalt^std::uint32_t(i)*0x2c1b3c6du^0x7f4a7c15u);s.t0=-1e30f;s.floor=floor;
  s.gate={h.enabledFlag,h.enabledValue,h.invertFlag,h.period,h.onTime,h.phase,(std::uint64_t(level+1)<<40)|(1ull<<32)|std::uint64_t(i),true};
  using K=Hazard::Kind;
  if(h.kind==K::Steam||h.kind==K::Pressure){const bool pressure=h.kind==K::Pressure;
   s.species=Species::Steam;s.at={centre.x,centre.y,bottom+.25f};s.box={hw*.45f,hd*.45f,.1f};s.travel={0,0,std::clamp(top-bottom,.8f,pressure?3.f:2.2f)};s.size=.1f;s.grow=pressure?1.3f:.9f;
   s.life=pressure?.9f:1.4f;s.rate=std::clamp(area*(pressure?16.f:10.f),8.f,pressure?60.f:40.f);s.strength=pressure?1.1f:.9f;out.push_back(s);}
  else if(h.kind==K::Fire){const float half=std::min(hw,hd);
   Source flame=s;flame.species=Species::Flame;flame.at={centre.x,centre.y,floor+.05f};flame.box={hw*.6f,hd*.6f,.02f};flame.travel={0,0,.45f};flame.size=std::min(.4f,.12f+half*.4f);flame.grow=-flame.size*.2f;flame.life=.7f;flame.rate=std::clamp(area*30.f,14.f,48.f);out.push_back(flame);
   Source ember=s;ember.species=Species::Ember;ember.seed^=0x85ebca6bu;ember.at=flame.at;ember.box=flame.box;ember.travel={0,0,1.6f};ember.size=.012f;ember.grow=-.004f;ember.life=2.4f;ember.rate=std::clamp(area*6.f,4.f,12.f);out.push_back(ember);
   Source smoke=s;smoke.species=Species::Soot;smoke.seed^=0xc2b2ae35u;smoke.at={centre.x,centre.y,floor+1.f};smoke.box={hw*.35f,hd*.35f,.05f};smoke.travel={0,0,2.6f};smoke.size=.3f;smoke.grow=1.4f;smoke.life=6;smoke.rate=3;smoke.strength=.5f;out.push_back(smoke);}
  else if(h.kind==K::Toxic){s.species=Species::Toxic;s.at={centre.x,centre.y,floor+.08f};s.box={hw*.85f,hd*.85f,.03f};s.travel={.2f,.1f,.25f};s.cone=2.6f;s.size=.3f;s.grow=.9f;s.life=7;s.rate=std::clamp(area*1.3f,4.f,24.f);s.strength=.85f;out.push_back(s);}
  else if(h.kind==K::FallingDebris){const float roof=std::min(h.top,w.clearanceAbove(centre.x,centre.y,floor+.1f)-.05f);
   Source grit=s;grit.species=Species::Grit;grit.at={centre.x,centre.y,roof};grit.box={hw*.6f,hd*.6f,0};grit.travel={0,0,-.2f};grit.size=.006f;grit.grow=0;grit.life=2.5f;grit.rate=std::clamp(area*10.f,10.f,40.f);out.push_back(grit);
   Source chip=s;chip.species=Species::Chunk;chip.seed^=0x5bd1e995u;chip.at=grit.at;chip.box=grit.box;chip.travel={0,0,-.4f};chip.size=.035f;chip.grow=0;chip.life=4;chip.rate=std::clamp(area*.5f,.6f,3.f);out.push_back(chip);
   Source dust=s;dust.species=Species::Cloud;dust.seed^=0x68e31da4u;dust.at={centre.x,centre.y,floor+.2f};dust.box={hw*.6f,hd*.6f,.05f};dust.travel={0,0,.35f};dust.size=.15f;dust.grow=.8f;dust.life=3.5f;dust.rate=std::clamp(area*.8f,1.f,5.f);dust.strength=.6f;out.push_back(dust);}
  else if(h.kind==K::Electricity){s.species=Species::Ion;s.at={centre.x,centre.y,floor+.25f};s.box={hw*.8f,hd*.8f,.2f};s.travel={0,0,.3f};s.size=.025f;s.grow=.04f;s.life=1.2f;s.rate=std::clamp(area*2.f,3.f,24.f);s.strength=.8f;out.push_back(s);
   Source ozone=s;ozone.species=Species::Smoke;ozone.seed^=0x94d049bbu;ozone.travel={0,0,1.f};ozone.size=.12f;ozone.grow=.7f;ozone.life=4;ozone.rate=std::clamp(area*.25f,1.2f,4.f);ozone.strength=.25f;out.push_back(ozone);}}
}
// What the game throws this frame: rounds' dust, gun smoke, blasts and thrown grenades' trails.
void gather(const Game& game,const World& w,const Particles::View& view,const Particles::Extra* extra,const std::vector<std::vector<ParticleCrumb>>& flights,std::vector<Source>& out){
 const int level=view.level;const float now=view.now;
 auto floorUnder=[&](Point3 p){return w.supportBelow(p.x,p.y,p.z+.05f);};
 // What rounds throw off a solid: dust that rolls out and hangs, and grit that falls.
 auto hits=[&](const std::vector<HitBurst>& list){for(const auto&b:list){if(b.level!=level||b.material==ImpactMaterial::Flesh)continue;const float age=now-b.time;if(age<0||age>6.f)continue;
  static const float strengths[]={1.f,.35f,.6f,1.25f,.95f,.8f,1.4f,.3f,0.f,.5f};const float strength=strengths[std::clamp(int(b.material),0,9)]*std::clamp(b.power,.4f,2.f);
  const Point3 n=unit(b.direction);const std::uint32_t seed=seedAt(b.point,b.time,0x51u);
  Source cloud;cloud.species=b.material==ImpactMaterial::Metal?Species::Smoke:Species::Cloud;cloud.seed=seed;cloud.at=b.point+n*.05f;cloud.travel=n*.7f+Up*.12f;cloud.cone=1.1f;cloud.size=.06f;cloud.grow=.7f*std::sqrt(strength);
  cloud.life=3.4f;cloud.count=b.material==ImpactMaterial::Metal?4:10;cloud.t0=b.time;cloud.spread=.08f;cloud.strength=std::min(1.4f,strength*1.25f);cloud.floor=b.floor;out.push_back(cloud);
  if(b.material==ImpactMaterial::Metal||b.material==ImpactMaterial::Glass)continue;
  Source grit;grit.species=Species::Grit;grit.seed=seed^0x2b3c4d5eu;grit.at=b.point+n*.03f;grit.travel=n*1.2f+Up*.4f;grit.cone=.7f;grit.size=.005f;grit.grow=0;grit.life=2.f;grit.count=8;grit.t0=b.time;grit.spread=.25f;grit.strength=std::min(1.f,strength);grit.floor=b.floor;out.push_back(grit);
  // Struck overhead, the ceiling sheds for a second or two.
  if(n.z<-.6f){Source pour=grit;pour.seed^=0x77u;pour.at=b.point+Point3{0,0,-.03f};pour.box={.08f,.08f,0};pour.travel={0,0,-.2f};pour.count=0;pour.rate=16;pour.until=b.time+1.4f;out.push_back(pour);}}};
 hits(game.hitBursts());if(extra)hits(extra->hits);
 // Gun smoke: a shot leaves a wisp in front of the muzzle that drifts on and hangs (a shotgun's pellets are one shot).
 auto shots=[&](const std::vector<ShotRay>& list){float lastTime=-1e9f;for(const auto&r:list){if(r.level!=level)continue;const float age=now-r.time;if(age<0||age>3.f||std::fabs(r.time-lastTime)<1e-4f)continue;lastTime=r.time;
  const Point3 d=unit(r.direction,{1,0,0});Source s;s.species=Species::Smoke;s.seed=seedAt(r.origin,r.time,0x61u);s.at=r.origin+d*.85f-Up*.1f;s.travel=d*.45f+Up*.12f;s.cone=.5f;s.size=.03f;s.grow=.34f;s.life=2.4f;s.count=5;s.t0=r.time;s.spread=.05f;s.strength=.5f;out.push_back(s);}};
 shots(game.shotRays());if(extra)shots(extra->shots);
 // A blast: thick smoke that rolls up and hangs, a ring of dust racing out along the floor, chunks thrown
 // and bouncing, embers, and the ceiling over it shaken loose.
 auto blasts=[&](const std::vector<BarrelExplosion>& list){for(const auto&x:list){if(x.level!=level)continue;const float age=now-x.time;if(age<0||age>16.f)continue;
  const Point3 c{x.pos.x,x.pos.y,x.z};const float floor=floorUnder(c),roof=w.clearanceAbove(c.x,c.y,floor+.1f);const std::uint32_t seed=seedAt(c,x.time,0x71u);
  // The fireball: a burst of burning billows thrown up and out that slow at once, white-yellow at the
  // heart, rolling orange and burning down to soot inside a second, a hotter core inside it.
  Source blaze;blaze.species=Species::Blaze;blaze.seed=seed^0x5u;blaze.at=c+Up*.3f;blaze.box={.22f,.22f,.12f};blaze.travel=Up*std::min(1.2f,std::max(.4f,roof-c.z-.8f));blaze.cone=1.65f;blaze.size=.3f;blaze.grow=1.25f;blaze.life=.85f;blaze.count=16;blaze.t0=x.time;blaze.spread=.06f;blaze.floor=floor;out.push_back(blaze);
  Source core=blaze;core.seed=seed^0x6u;core.box={.08f,.08f,.08f};core.travel=Up*.35f;core.cone=2.4f;core.size=.42f;core.grow=.6f;core.life=.45f;core.count=7;core.spread=.03f;out.push_back(core);
  // Its soot rolls out of it as it burns down and climbs to hang under the ceiling.
  Source soot;soot.species=Species::Soot;soot.seed=seed;soot.at=c+Up*.35f;soot.box={.3f,.3f,.2f};soot.travel=Up*std::min(1.9f,std::max(.6f,roof-c.z-.6f));soot.cone=1.3f;soot.size=.38f;soot.grow=2.f;soot.life=12;soot.count=34;soot.t0=x.time+.12f;soot.spread=.7f;soot.floor=floor;out.push_back(soot);
  Source pall=soot;pall.species=Species::Smoke;pall.seed=seed^0x1u;pall.travel=Up*std::min(2.6f,std::max(.8f,roof-c.z-.4f));pall.cone=.9f;pall.size=.45f;pall.grow=2.3f;pall.life=14;pall.count=16;pall.t0=x.time+.3f;pall.spread=1.f;pall.strength=.7f;out.push_back(pall);
  // The blast's dust: a low sheet racing out along the floor and spreading as it slows.
  Source ring;ring.species=Species::Cloud;ring.seed=seed^0x2u;ring.at={c.x,c.y,floor+.12f};ring.radial=true;ring.cone=.1f;ring.travel={3.4f,0,0};ring.size=.28f;ring.grow=1.9f;ring.tall=.38f;ring.life=6;ring.count=28;ring.t0=x.time;ring.spread=.1f;ring.strength=.65f;ring.floor=floor;out.push_back(ring);
  Source chunks;chunks.species=Species::Chunk;chunks.seed=seed^0x3u;chunks.at=c+Up*.25f;chunks.box={.15f,.15f,.1f};chunks.travel=Up*5.f;chunks.cone=1.25f;chunks.size=.035f;chunks.grow=0;chunks.life=7;chunks.count=18;chunks.t0=x.time;chunks.spread=.02f;chunks.floor=floor;out.push_back(chunks);
  Source embers;embers.species=Species::Ember;embers.seed=seed^0x4u;embers.at=c+Up*.3f;embers.box={.2f,.2f,.15f};embers.travel=Up*2.2f;embers.cone=1.4f;embers.size=.014f;embers.grow=-.006f;embers.life=2.6f;embers.count=22;embers.t0=x.time;embers.spread=.1f;out.push_back(embers);
  if(roof-floor<8.f){
   // The ceiling over it lets go of what it held: streams of grit and a slow fall of dust.
   for(int k=0;k<6;++k){const float a=rnd(seed,200+k)*kTwoPi,d=.6f+2.6f*rnd(seed,210+k);const Point3 at{c.x+std::cos(a)*d,c.y+std::sin(a)*d,0};const float top=w.clearanceAbove(at.x,at.y,floor+.1f)-.05f;
    if(top-floor>8.f||!Particles::open(w,{at.x,at.y,top-.02f}))continue;
    Source pour;pour.species=Species::Grit;pour.seed=seed^(0x100u+std::uint32_t(k));pour.at={at.x,at.y,top};pour.box={.1f,.1f,0};pour.travel={0,0,-.2f};pour.size=.005f;pour.grow=0;pour.life=2.2f;pour.rate=18;pour.t0=x.time+.05f+.25f*rnd(seed,220+k);pour.until=pour.t0+.9f+.6f*rnd(seed,230+k);pour.floor=floor;out.push_back(pour);
    Source dust;dust.species=Species::Cloud;dust.seed=seed^(0x200u+std::uint32_t(k));dust.at={at.x,at.y,top-.15f};dust.travel={0,0,-.6f};dust.cone=.6f;dust.size=.12f;dust.grow=.9f;dust.life=5;dust.count=2;dust.t0=pour.t0;dust.spread=.4f;dust.strength=.55f;dust.floor=floor;out.push_back(dust);}}}};
 blasts(game.barrelExplosions());if(extra)blasts(extra->blasts);
 // A thrown grenade trails smoke from the moment it leaves the hand, and smokes where it lies.
 for(size_t i=0;i<flights.size();++i){const auto&f=flights[i];if(f.empty()||f.front().time>now||now-f.back().time>4.f)continue;
  Point3 lo=f.front().at,hi=lo;for(const auto&c:f){lo={std::min(lo.x,c.at.x),std::min(lo.y,c.at.y),std::min(lo.z,c.at.z)};hi={std::max(hi.x,c.at.x),std::max(hi.y,c.at.y),std::max(hi.z,c.at.z)};}
  Source trail;trail.species=Species::Smoke;trail.seed=seedAt(f.front().at,f.front().time,0x81u);trail.path=&f;trail.at=(lo+hi)*.5f;trail.box=(hi-lo)*.5f;trail.travel=Up*.3f;trail.cone=1.1f;
  trail.size=.03f;trail.grow=.26f;trail.life=1.9f;trail.rate=34;trail.t0=f.front().time;trail.until=f.back().time;trail.strength=.42f;out.push_back(trail);}
}
}

ParticleStats Particles::draw(ParticleMemory& memory,const Game& game,const View& view,const Card& card,const Light& light,const Visible& visible,const Extra* extraIn){
 perf::Scope profile(perf::Air);const auto began=std::chrono::steady_clock::now();ParticleStats stats;
 if(!game.chunkResident(view.level)){memory.last=stats;return stats;}
 const World& w=game.chunkWorld(view.level);
 const Extra* extra=extraIn?extraIn:(memory.extra.blasts.empty()&&memory.extra.hits.empty()&&memory.extra.shots.empty()&&memory.extra.throws.empty()?nullptr:&memory.extra);
 // The game's grenades in flight, as this frame sees them: each one's trail is the crumbs it has left
 // (a later frame at the same moment adds none, so the same moment draws the same trail).
 auto&trails=memory.trails[view.level];
 for(const auto&g:game.tacticalGrenades())if(g.level==view.level){std::vector<ParticleCrumb>* best=nullptr;float nearest=1.2f;
  for(auto&t:trails){const auto&last=t.back();if(view.now<last.time-1e-4f||view.now-last.time>.5f)continue;const float d=span(last.at-g.position);if(d<nearest){nearest=d;best=&t;}}
  if(!best){trails.push_back({});best=&trails.back();}
  if(best->empty()||view.now>best->back().time+1e-4f)best->push_back({view.now,g.position});}
 trails.erase(std::remove_if(trails.begin(),trails.end(),[&](const auto&t){return t.empty()||view.now-t.back().time>10.f;}),trails.end());
 if(trails.size()>12)trails.erase(trails.begin(),trails.begin()+std::ptrdiff_t(trails.size()-12));
 // And a test's throws, flown here: the arc to the floor, then lying there until the fuse.
 std::vector<std::vector<ParticleCrumb>> flights=trails;
 if(extra)for(const auto&t:extra->throws){if(t.level!=view.level)continue;std::vector<ParticleCrumb> f;Point3 p=t.from,v=t.velocity;
  for(float s=0;s<=t.fuse+1e-4f;s+=1.f/30){f.push_back({t.time+s,p});const float floor=w.supportBelow(p.x,p.y,p.z+.05f)+.06f;
   p=p+v*(1.f/30);v.z-=9.8f/30;if(p.z<floor){p.z=floor;v={0,0,0};}}
  flights.push_back(std::move(f));}
 const std::uint64_t signature=worldSignature(game,w);
 auto&fixed=memory.fixed[view.level];
 if(!fixed.second||fixed.first!=signature){auto made=std::make_shared<std::vector<Source>>();gatherFixed(w,view.level,*made);fixed={signature,made};}
 std::vector<Source> sources=*std::static_pointer_cast<const std::vector<Source>>(fixed.second);gather(game,w,view,extra,flights,sources);
 Frame frame{memory,game,w,view,stats,{},{}};
 // Thinned by distance, then held to the budgets nearest first.
 struct Plan {size_t index;float distance,keep,count;bool glow;};std::vector<Plan> plans;plans.reserve(sources.size());
 for(size_t i=0;i<sources.size();++i){auto&s=sources[i];frame.noteGate(s.gate);Point3 lo,hi;reachOf(s,lo,hi);
  const Point3 nearest{std::clamp(view.eye.x,lo.x,hi.x),std::clamp(view.eye.y,lo.y,hi.y),std::clamp(view.eye.z,lo.z,hi.z)};const float d=span(nearest-view.eye);
  if(d>55.f||(visible&&!visible(lo,hi))){++stats.culled;continue;}
  const float keep=1.f-.65f*smooth(10.f,40.f,d);plans.push_back({i,d,keep,alive(s),behaviour(s.species).glow});}
 std::stable_sort(plans.begin(),plans.end(),[](const Plan&a,const Plan&b){return a.distance<b.distance;});
 float spent[2]{0,0};const float budget[2]={float(BlendedBudget),float(GlowBudget)};
 for(auto&p:plans){float&used=spent[p.glow?1:0];const float want=p.count*p.keep;if(used+want>budget[p.glow?1:0]*.92f)p.keep=std::max(0.f,(budget[p.glow?1:0]*.92f-used)/std::max(1.f,p.count));used+=p.count*p.keep;}
 for(const auto&p:plans){if(p.keep<=0){stats.thinned+=int(p.count);continue;}++stats.sources;frame.evaluate(sources[p.index],p.keep);}
 // Held off every surface: a card that would cut into a wall, floor or ceiling is drawn smaller and
 // thinner (lifted, if it lies on the floor), and one whose middle is in a solid is not drawn at all.
 auto settle=[&](std::vector<Live>& list){std::vector<Live> kept;kept.reserve(list.size());
  for(auto&p:list){
   // How much room a card needs (its radius, or a pixel and a half: a speck never shrinks below that, so it
   // does not crawl); the room is asked only that far and held to it, so the answer never depends on what
   // was asked before.
   const float depth=dot3(p.c-view.eye,view.forward);if(depth<.12f){++stats.culled;continue;}
   const float least=view.pixel*depth*.75f,need=std::max(p.r,least)*.4f;
   float room=clearance(memory,w,signature,p.c,need,stats);if(room<0){++stats.inside;continue;}room=std::min(room,need);
   const Behaviour& b=behaviour(p.sp);const bool solid=b.ballistic||p.sp==Species::Drop;
   // A card fades where it nears what lies behind it, so it may lie a good way into a wall or the floor.
   if(!solid&&room<p.r*.4f){const float fits=room/.4f;if(fits<p.r*.3f&&fits<.03f){++stats.culled;continue;}p.alpha*=.55f+.45f*fits/p.r;p.r=fits;}
   p.r=std::max(p.r,solid?least:std::min(least,room/.4f));p.depth=depth;kept.push_back(p);}
  list.swap(kept);};
 settle(frame.blended);settle(frame.glowing);
 if(int(frame.blended.size())>BlendedBudget)frame.blended.resize(size_t(BlendedBudget));
 if(int(frame.glowing.size())>GlowBudget)frame.glowing.resize(size_t(GlowBudget));
 // Far to near (same depth: by identity, so the order never depends on how they were gathered).
 std::sort(frame.blended.begin(),frame.blended.end(),[](const Live&a,const Live&b){return a.depth!=b.depth?a.depth>b.depth:a.id<b.id;});
 // The room's light in each patch of air, toward the eye and from behind (by 90 cm cell for the frame). A
 // frame asks the room for at most LightCells of them, nearest first, as the cards come; past that a card
 // takes the mean of what was asked (its light only, never where or how big it is drawn).
 constexpr int LightCells=120;
 std::unordered_map<std::uint64_t,std::pair<float,float>> lit;double frontSum=0,backSum=0;
 auto lightAt=[&](Point3 p,float scatter){
  constexpr float Size=.9f;const int cx=int(std::floor(p.x/Size)),cy=int(std::floor(p.y/Size)),cz=int(std::floor(p.z/Size));const std::uint64_t key=(std::uint64_t(std::uint16_t(cx+4096))<<32)|(std::uint64_t(std::uint16_t(cy+4096))<<16)|std::uint64_t(std::uint16_t(cz+4096));
  auto found=lit.find(key);std::pair<float,float> value;
  if(found!=lit.end())value=found->second;
  else if(int(lit.size())<LightCells){const Point3 q{(cx+.5f)*Size,(cy+.5f)*Size,(cz+.5f)*Size};const Point3 toEye=unit(view.eye-q,{1,0,0});
   value={light?light(q,toEye):1.f,light?light(q,toEye*-1.f):1.f};lit.emplace(key,value);frontSum+=value.first;backSum+=value.second;}
  else value={float(frontSum/double(lit.size())),float(backSum/double(lit.size()))};
  return std::clamp(value.first+scatter*std::max(0.f,value.second-value.first*.55f),0.f,3.f);};
 // The nearest cards ask for their light first.
 for(auto p=frame.blended.rbegin();p!=frame.blended.rend();++p){if(behaviour(p->sp).row==RowSolid||p->sp==Species::Ion)continue;lightAt(p->c,0);}
 const Texture&atlas=blendedAtlas();const Texture&glow=glowAtlas();
 // (The look's values: a card's soft edge, a fireball's heat, a speck of dust's glint, how much a lamp behind a card lights it.)
 static const float cardSoft=lookTune("cardSoft",1.f),blazeHeat=lookTune("blazeHeat",6.f),moteGlint=lookTune("moteGlint",1.6f),cardScatter=lookTune("cardScatter",1.f);
 auto corners=[&](const Live&p,float tall,Point3& u,Point3& v){
  const Point3 f=unit(p.c-view.eye,view.forward);
  if(p.streak){const Point3 along=p.velocity-f*dot3(p.velocity,f);const float l=span(along);
   if(l>.25f){const Point3 a=along*(1.f/l);const float stretch=1.f+std::min(6.f,l*.06f/std::max(.002f,p.r));v=a*(p.r*stretch);u=unit(cross(f,a),{1,0,0})*p.r*.6f;return;}}
  // Vapour on the move is laid along its flight and drawn out by its speed: a jet's threads run the
  // way it goes.
  if(p.sp==Species::Steam||p.sp==Species::Mist){const Point3 along=p.velocity-f*dot3(p.velocity,f);const float l=span(along);
   if(l>.08f){const Point3 a=along*(1.f/l);const float stretch=1.f+std::min(1.3f,l*.5f)*(1.f-smooth(.25f,.6f,p.r));v=a*(p.r*stretch);u=unit(cross(f,a),{1,0,0})*(p.r/std::sqrt(stretch));return;}}
  // A sheet along the floor stays level, only rocked a little.
  const float turn=p.flat?std::sin(p.turn)*.12f:p.turn;
  const Point3 right=unit(cross(f,Up),{1,0,0}),up=cross(right,f);const float cs=std::cos(turn),sn=std::sin(turn);
  u=(right*cs+up*sn)*p.r;v=(up*cs-right*sn)*(p.r*tall*p.tall);};
 for(const auto&p:frame.blended){const Behaviour& b=behaviour(p.sp);Point3 u,v;corners(p,1,u,v);
  Rect r;float scale=1.f;bool selfLit=false;CardLook look;
  if(b.row==RowSolid||p.sp==Species::Mote){r=cellRect(RowSolid,p.sp==Species::Mote||p.cell<0?SolidMote:p.cell);look.opacity=std::clamp(p.alpha,0.f,1.f);
   if(b.row==RowSolid&&p.alpha<.15f)continue;}
  else{
   // Its frame by its age (smoke and fire change fastest young), blended on toward the next; its own
   // opacity; a soft edge of about its size where it nears a surface.
   const float age=p.sp==Species::Blaze?std::pow(p.u,.7f):p.sp==Species::Smoke||p.sp==Species::Soot?std::pow(p.u,.8f):p.u;
   const float at=std::clamp(age,0.f,1.f)*float(Frames-1);const int frameIndex=std::min(Frames-1,int(at));
   r=puffRect(b.row,p.variant,frameIndex);look.blend=frameIndex<Frames-1?at-float(frameIndex):0.f;look.step=float(Cell)/float(AtlasW);
   look.opacity=std::clamp(p.alpha,0.f,1.f);look.soft=std::clamp(p.r*.7f,.04f,.7f)*cardSoft;if(look.opacity<.01f)continue;
   selfLit=p.sp==Species::Ion;}
  MeshVertex q[4]={{p.c-u-v,r.u0,r.v1,1},{p.c+u-v,r.u1,r.v1,1},{p.c+u+v,r.u1,r.v0,1},{p.c-u+v,r.u0,r.v0,1}};
  if(selfLit){const float flicker=.8f+.2f*std::sin(view.now*31.f+float(p.id&255u));scale=1.6f*flicker;}
  else if(p.sp==Species::Blaze){
   // A fireball lights itself, fiercely at first; burnt down to soot it takes the room's light.
   const float cooled=smooth(.5f,1.f,p.u),room=lightAt(p.c,0);scale=.35f+blazeHeat*(1-p.u)*(1-p.u)*(1-cooled)+cooled*.65f;for(auto&c:q)c.light=1.f+(room-1.f)*cooled;}
  else if(p.r<.8f){const float l=lightAt(p.c,b.scatter*cardScatter);for(auto&c:q)c.light=l;
   // A speck of dust glints where the light catches it.
   if(p.sp==Species::Mote)scale=moteGlint;}
  else for(auto&c:q)c.light=lightAt(c.p,b.scatter*cardScatter);
  card(q,atlas,scale,look);++stats.blended;}
 for(const auto&p:frame.glowing){Point3 u,v;const bool flame=p.sp==Species::Flame;corners(p,flame?2.f:1.f,u,v);
  if(flame){const Point3 f=unit(p.c-view.eye,view.forward),right=unit(cross(f,Up),{1,0,0});u=right*p.r;v=Up*(p.r*2.f);}
  const Rect r=glowRect(flame?p.cell:4);const Point3 base=flame?p.c+v*.6f:p.c;
  if(flame&&!open(w,base))continue;
  const MeshVertex q[4]={{base-u-v,r.u0,r.v1,1},{base+u-v,r.u1,r.v1,1},{base+u+v,r.u1,r.v0,1},{base-u+v,r.u0,r.v0,1}};
  const float brightness=std::clamp(p.alpha,0.f,1.f)*(flame?2.2f:2.6f);if(brightness<.02f)continue;card(q,glow,brightness,{});++stats.glowing;
  // A hot ember carries a soft halo.
  if(!flame&&p.u<.5f){const Rect h=glowRect(5);const Point3 hu=u*4.f,hv=v*4.f;const MeshVertex halo[4]={{p.c-hu-hv,h.u0,h.v1,1},{p.c+hu-hv,h.u1,h.v1,1},{p.c+hu+hv,h.u1,h.v0,1},{p.c-hu+hv,h.u0,h.v0,1}};card(halo,glow,brightness*.35f,{});}}
 stats.particles=stats.blended+stats.glowing;
 stats.ms=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-began).count();memory.last=stats;return stats;
}
}

#include "Wetness.h"
#include "world/ModelIds.h"
#include <algorithm>
#include <cmath>

namespace retro {
namespace {
std::uint32_t hashOf(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
}

std::array<float,4> WetnessMask::at(float x,float y)const{
 if(texels.empty())return {0,0,0,-1e9f};
 const float fx=std::clamp(x/Texel-.5f,0.f,Size-1.f),fy=std::clamp(y/Texel-.5f,0.f,Size-1.f);
 const int x0=int(fx),y0=int(fy),x1=std::min(x0+1,Size-1),y1=std::min(y0+1,Size-1);const float tx=fx-x0,ty=fy-y0;
 std::array<float,4> out{};
 for(int c=0;c<4;++c){auto v=[&](int xx,int yy){return float((texels[size_t(yy*Size+xx)]>>(c*8))&255u)/255.f;};
  out[size_t(c)]=(v(x0,y0)*(1-tx)+v(x1,y0)*tx)*(1-ty)+(v(x0,y1)*(1-tx)+v(x1,y1)*tx)*ty;}
 out[3]=originZ+out[3]*25.5f;return out;}
float WetnessMask::floorWet(float x,float y,float z)const{
 if(texels.empty())return 0;const auto m=at(x,y);return m[0]*(1-smooth((std::fabs(z-m[3])-.35f)/.25f));}

float outdoorRain(const World& w){
 if(!w.outdoors())return 0;return rainOverride()>=0?std::clamp(rainOverride(),0.f,1.f):std::clamp(w.wetness(),0.f,1.f);}

WetnessMask bakeWetness(const World& w,float originZ){
 constexpr int N=WetnessMask::Size;constexpr float T=WetnessMask::Texel;
 WetnessMask mask;mask.originZ=originZ;
 std::vector<float> wet(size_t(N*N),0.f),puddle(size_t(N*N),0.f),streak(size_t(N*N),0.f),floor(size_t(N*N),0.f);
 for(int y=0;y<N;++y)for(int x=0;x<N;++x)floor[size_t(y*N+x)]=w.floorHeight((x+.5f)*T,(y+.5f)*T);
 const float damp=std::clamp(w.wetness(),0.f,1.f);
 // A damp map's patches reach further and run wetter; a dry one keeps only its sources' own.
 const float reach=.7f+.8f*damp,gain=std::min(1.f,.6f+.6f*damp);
 auto solid=[&](float x,float y){const int tx=int(std::floor(x)),ty=int(std::floor(y));return tx<0||ty<0||tx>=World::Width||ty>=World::Height||w.tile(tx,ty)=='#';};
 // Within ~0.7 m of a wall: a source there runs down the wall too.
 auto nearWall=[&](float x,float y){for(float dx:{-.7f,0.f,.7f})for(float dy:{-.7f,0.f,.7f})if((dx!=0||dy!=0)&&solid(x+dx,y+dy))return true;return false;};
 // A round source: wet, puddle and streak falling off smoothly to its radius.
 auto spot=[&](float cx,float cy,float radius,float amount,float pud,float str){
  const int x0=std::max(0,int((cx-radius)/T)),x1=std::min(N-1,int((cx+radius)/T)),y0=std::max(0,int((cy-radius)/T)),y1=std::min(N-1,int((cy+radius)/T));
  for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){const float d=std::hypot((x+.5f)*T-cx,(y+.5f)*T-cy);if(d>=radius)continue;const float f=smooth(1-d/radius);const size_t i=size_t(y*N+x);
   wet[i]=std::max(wet[i],amount*f);puddle[i]=std::max(puddle[i],pud*f*f);streak[i]=std::max(streak[i],str*f);}};
 // Water: the floor round a pool is splashed and tracked through; under it is the water itself.
 for(const auto& v:w.waterVolumes()){const float r=.9f*reach;
  const int x0=std::max(0,int((v.x1-r)/T)),x1=std::min(N-1,int((v.x2+r)/T)),y0=std::max(0,int((v.y1-r)/T)),y1=std::min(N-1,int((v.y2+r)/T));
  for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){const float px=(x+.5f)*T,py=(y+.5f)*T;const float dx=std::max({v.x1-px,0.f,px-v.x2}),dy=std::max({v.y1-py,0.f,py-v.y2}),d=std::hypot(dx,dy);
   if(d>=r)continue;const float f=smooth(1-d/r);const size_t i=size_t(y*N+x);wet[i]=std::max(wet[i],f);puddle[i]=std::max(puddle[i],.7f*f*f);}}
 // Leaks: drips wet the floor under them (a puddle at the middle), spray a wider fan along its
 // throw, steam and a plume indoors condense round them.
 for(const auto& e:w.particleEmitters()){
  const float x=e.position.x,y=e.position.y;const bool wall=nearWall(x,y);
  if(e.kind==ParticleKind::Drips)spot(x,y,(.6f+e.radius*2)*reach,1.f,.9f,wall?.8f:0.f);
  else if(e.kind==ParticleKind::Spray)spot(x+e.drift.x*.5f,y+e.drift.y*.5f,1.4f*reach,.9f,.6f,wall?.8f:0.f);
  else if(e.kind==ParticleKind::Steam||(e.kind==ParticleKind::Plume&&!w.outdoors()))spot(x,y,1.f*reach,.45f,.2f,wall?.5f:0.f);}
 for(const auto& h:w.hazards())if(h.kind==Hazard::Kind::Steam){const float cx=(h.x1+h.x2)*.5f,cy=(h.y1+h.y2)*.5f;
  spot(cx,cy,(std::hypot(h.x2-h.x1,h.y2-h.y1)*.5f+.5f)*reach,.45f,.2f,nearWall(cx,cy)?.5f:0.f);}
 // Floor drains: the water runs to them.
 for(const auto& f:w.fixtures())if(f.model==model::med_floor_drain)spot(f.position.x,f.position.y,1.1f*reach,.7f,1.f,0.f);
 // Authored wet patches (WET records): their own amount, unscaled.
 for(const auto& p:w.wetPatches()){const float cx=(p.x1+p.x2)*.5f,cy=(p.y1+p.y2)*.5f;
  if(p.round){spot(cx,cy,std::max(.05f,(p.x2-p.x1)*.5f),p.amount/gain,p.puddle?1.f:0.f,0.f);continue;}
  const int x0=std::max(0,int(p.x1/T)),x1=std::min(N-1,int(p.x2/T)),y0=std::max(0,int(p.y1/T)),y1=std::min(N-1,int(p.y2/T));
  for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){const float px=(x+.5f)*T,py=(y+.5f)*T;const float edge=std::min({px-p.x1,p.x2-px,py-p.y1,p.y2-py});if(edge<0)continue;
   const float f=smooth(edge/.4f);const size_t i=size_t(y*N+x);wet[i]=std::max(wet[i],p.amount/gain*f);if(p.puddle)puddle[i]=std::max(puddle[i],f);}}
 // A damp map seeps: a few patches at the foot of its walls, picked by a fixed hash so every bake of
 // the map wets the same places, apart from each other.
 if(damp>=.25f&&!w.outdoors()){const int wanted=int(std::lround(damp*8));std::vector<std::pair<float,float>> picked;
  std::vector<std::pair<std::uint32_t,int>> order;for(int t=0;t<World::Width*World::Height;++t)order.push_back({hashOf(std::uint32_t(t)*2654435761u+std::uint32_t(w.level())*97u),t});
  std::sort(order.begin(),order.end());
  for(const auto& [h,t]:order){if(int(picked.size())>=wanted)break;const int tx=t%World::Width,ty=t/World::Width;if(w.tile(tx,ty)=='#')continue;
   const float x=tx+.5f,y=ty+.5f;if(!nearWall(x,y))continue;bool apart=true;for(auto [px,py]:picked)if(std::hypot(px-x,py-y)<4.f)apart=false;if(!apart)continue;
   picked.push_back({x,y});spot(x,y,(.8f+1.2f*float(h&255u)/255.f)*reach,.25f+.55f*damp,.5f,.35f);}}
 // Low spots hold water that is already there: puddles gather where the floor dips.
 for(int y=0;y<N;++y)for(int x=0;x<N;++x){double sum=0;int n=0;for(int dy=-6;dy<=6;dy+=3)for(int dx=-6;dx<=6;dx+=3){const int xx=x+dx,yy=y+dy;if(xx<0||yy<0||xx>=N||yy>=N||solid((xx+.5f)*T,(yy+.5f)*T))continue;sum+=floor[size_t(yy*N+xx)];++n;}
  const float dip=float(sum/std::max(1,n))-floor[size_t(y*N+x)];if(dip>.02f&&dip<.5f)puddle[size_t(y*N+x)]=std::max(puddle[size_t(y*N+x)],smooth(dip/.08f));}
 mask.texels.resize(size_t(N*N));
 for(size_t i=0;i<mask.texels.size();++i){
  auto byte=[](float v){return std::uint32_t(std::clamp(int(std::lround(v*255.f)),0,255));};
  const std::uint32_t z=std::uint32_t(std::clamp(int(std::lround((floor[i]-originZ)/.1f)),0,255));
  mask.texels[i]=byte(std::min(1.f,wet[i]*gain))|(byte(puddle[i])<<8)|(byte(std::min(1.f,streak[i]*gain))<<16)|(z<<24);}
 return mask;
}
}

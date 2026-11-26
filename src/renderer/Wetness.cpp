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

float FilmShape::at(float x,float y)const{
 const float u=(x-x0)/Cell-.5f,v=(y-y0)/Cell-.5f;const int i=int(std::floor(u)),j=int(std::floor(v));if(i<0||j<0||i+1>=width||j+1>=height)return -1.f;
 const float fx=u-i,fy=v-j;auto s=[&](int a,int b){return inside[size_t(b*width+a)];};
 return (s(i,j)*(1-fx)+s(i+1,j)*fx)*(1-fy)+(s(i,j+1)*(1-fx)+s(i+1,j+1)*fx)*fy;}
std::vector<FilmShape> shapeFilms(const World& w){
 std::vector<FilmShape> out;if(w.waterVolumes().empty())return out;
 constexpr float C=FilmShape::Cell,Reach=.6f;const float ground=w.layers().front().elevation;
 // Smooth noise over the floor, the same on every bake: the waterline wanders by it.
 auto wander=[](float x,float y){const int xi=int(std::floor(x)),yi=int(std::floor(y));const float fx=smooth(x-xi),fy=smooth(y-yi);
  auto at=[](int a,int b){return float(hashOf(std::uint32_t(a)*2654435761u^std::uint32_t(b)*40503u)&1023u)/1023.f*2.f-1.f;};
  return (at(xi,yi)*(1-fx)+at(xi+1,yi)*fx)*(1-fy)+(at(xi,yi+1)*(1-fx)+at(xi+1,yi+1)*fx)*fy;};
 std::vector<float> levels;
 for(const auto& v:w.waterVolumes())if(waterFilm(v,ground)){bool known=false;for(float z:levels)known|=std::fabs(z-v.surface)<.03f;if(!known)levels.push_back(v.surface);}
 for(float level:levels){
  auto here=[&](const WaterVolume& v){return std::fabs(v.surface-level)<.03f;};
  float ax=1e9f,ay=1e9f,bx=-1e9f,by=-1e9f;
  for(const auto& v:w.waterVolumes())if(here(v)&&waterFilm(v,ground)){ax=std::min(ax,v.x1-Reach);ay=std::min(ay,v.y1-Reach);bx=std::max(bx,v.x2+Reach);by=std::max(by,v.y2+Reach);}
  float murk=0;for(const auto& v:w.waterVolumes())if(here(v)&&waterFilm(v,ground))murk=std::max(murk,v.murk);
  FilmShape film;film.murk=murk;film.surface=level;film.x0=std::floor(ax/C)*C;film.y0=std::floor(ay/C)*C;
  const int W=std::max(2,int(std::ceil((bx-film.x0)/C))),H=std::max(2,int(std::ceil((by-film.y0)/C)));film.width=W;film.height=H;
  std::vector<float> cover(size_t(W*H),0.f);
  auto fill=[&](float x1,float y1,float x2,float y2){
   for(int j=std::max(0,int((y1-film.y0)/C));j<H&&film.y0+(j+.5f)*C<y2;++j)for(int i=std::max(0,int((x1-film.x0)/C));i<W&&film.x0+(i+.5f)*C<x2;++i)
    if(film.x0+(i+.5f)*C>=x1&&film.y0+(j+.5f)*C>=y1)cover[size_t(j*W+i)]=1.f;};
  for(const auto& v:w.waterVolumes())if(here(v))fill(v.x1,v.y1,v.x2,v.y2);
  // What stands out of the water next to a film counts as water, so the waterline runs into it and not short of it.
  for(const auto& v:w.waterVolumes()){if(!here(v)||!waterFilm(v,ground))continue;
   for(const auto& b:w.structures())if(b.bottom<=level&&b.top>level+.05f){const float x1=std::max(b.x1,v.x1-.2f),y1=std::max(b.y1,v.y1-.2f),x2=std::min(b.x2,v.x2+.2f),y2=std::min(b.y2,v.y2+.2f);if(x1<x2&&y1<y2)fill(x1,y1,x2,y2);}
   for(int ty=int(std::floor(v.y1-.2f));ty<=int(std::floor(v.y2+.2f));++ty)for(int tx=int(std::floor(v.x1-.2f));tx<=int(std::floor(v.x2+.2f));++tx)
    if(tx>=0&&ty>=0&&tx<World::Width&&ty<World::Height&&w.tile(tx,ty)=='#')fill(std::max(float(tx),v.x1-.2f),std::max(float(ty),v.y1-.2f),std::min(tx+1.f,v.x2+.2f),std::min(ty+1.f,v.y2+.2f));}
  auto blur=[&](const std::vector<float>& in,int r){std::vector<float> mid(in.size()),res(in.size());
   for(int j=0;j<H;++j)for(int i=0;i<W;++i){float sum=0;for(int d=-r;d<=r;++d)sum+=in[size_t(j*W+std::clamp(i+d,0,W-1))];mid[size_t(j*W+i)]=sum/float(2*r+1);}
   for(int j=0;j<H;++j)for(int i=0;i<W;++i){float sum=0;for(int d=-r;d<=r;++d)sum+=mid[size_t(std::clamp(j+d,0,H-1)*W+i)];res[size_t(j*W+i)]=sum/float(2*r+1);}
   return res;};
  const auto shape=blur(blur(cover,2),2);   // corners round off, seams between patches vanish
  film.inside.resize(size_t(W*H));
  for(int j=0;j<H;++j)for(int i=0;i<W;++i){const float px=film.x0+(i+.5f)*C,py=film.y0+(j+.5f)*C,f=shape[size_t(j*W+i)];
   // The waterline wanders where the field is between wet and dry; well inside and well outside it cannot.
   const float ragged=(.7f*wander(px*3.3f,py*3.3f)+.3f*wander(px*8.7f+11.f,py*8.7f+5.f))*.5f*4.f*f*(1-f);
   film.inside[size_t(j*W+i)]=(i==0||j==0||i==W-1||j==H-1)?-1.f:f+ragged-.5f;}
  out.push_back(std::move(film));}
 return out;
}
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
 const float ground=w.layers().front().elevation;
 // A film of water: its own shape is wet and glossy, with a damp rim round it.
 for(const auto& film:shapeFilms(w))
  for(int y=0;y<N;++y)for(int x=0;x<N;++x){const float px=(x+.5f)*T,py=(y+.5f)*T;
   if(px<film.x0||py<film.y0||px>film.x0+film.width*FilmShape::Cell||py>film.y0+film.height*FilmShape::Cell)continue;
   float rim=0;for(float dx:{-.3f,0.f,.3f})for(float dy:{-.3f,0.f,.3f})rim+=film.at(px+dx,py+dy)>0?1.f/9.f:0.f;
   const bool water=film.at(px,py)>0;const size_t i=size_t(y*N+x);wet[i]=std::max(wet[i],water?1.f:.75f*smooth(rim*2.f));if(water)puddle[i]=1.f;}
 for(const auto& v:w.waterVolumes()){
  if(waterFilm(v,ground))continue;
  const float r=.9f*reach;
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

#include "WaterRipples.h"
#include "../core/Tune.h"
#include <algorithm>
#include <cmath>
namespace retro::ripples {
namespace {
constexpr float Band=2.6f; // the train's reach, in its widths (exp(-6.8): nothing left)
float smooth(float a,float b,float x){const float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
float strength(const Ring& r,float age){if(age<0||age>=r.life)return 0;const float fade=1-age/r.life;return r.amplitude*fade*fade*smooth(0,.05f,age)/(1+2.5f*radius(r,age));}
}
float radius(const Ring& r,float age){return .02f+r.speed*std::max(0.f,age);}
float halfWidth(const Ring&,float age){return .03f+.05f*std::max(0.f,age);}
float Field::height(float x,float y)const{float h=0;
 for(const auto&r:rings){const float age=now-r.born,a=strength(r,age);if(a==0)continue;
  const float d=std::sqrt((x-r.x)*(x-r.x)+(y-r.y)*(y-r.y)),off=d-radius(r,age),s=halfWidth(r,age);if(std::fabs(off)>Band*s)continue;
  h+=a*std::cos(kTwoPi*off/r.wavelength)*std::exp(-(off/s)*(off/s));}
 return h;}
Vec2 Field::slope(float x,float y)const{Vec2 g{};
 for(const auto&r:rings){const float age=now-r.born,a=strength(r,age);if(a==0)continue;
  const float dx=x-r.x,dy=y-r.y,d=std::sqrt(dx*dx+dy*dy);if(d<1e-4f)continue;const float off=d-radius(r,age),s=halfWidth(r,age);if(std::fabs(off)>Band*s)continue;
  const float k=kTwoPi/r.wavelength,e=std::exp(-(off/s)*(off/s)),along=a*e*(-k*std::sin(k*off)-2*off/(s*s)*std::cos(k*off));
  g+=Vec2{dx/d,dy/d}*along;}
 return g;}
bool Field::touches(float x1,float y1,float x2,float y2)const{
 for(const auto&r:rings){const float age=now-r.born;if(strength(r,age)==0)continue;const float in=radius(r,age),b=Band*halfWidth(r,age);
  const float cx=std::clamp(r.x,x1,x2)-r.x,cy=std::clamp(r.y,y1,y2)-r.y,fx=std::max(std::fabs(x1-r.x),std::fabs(x2-r.x)),fy=std::max(std::fabs(y1-r.y),std::fabs(y2-r.y));
  if(std::sqrt(cx*cx+cy*cy)<=in+b&&std::sqrt(fx*fx+fy*fy)>=in-b)return true;}
 return false;}
bool Field::bounds(float& x1,float& y1,float& x2,float& y2)const{bool any=false;
 for(const auto&r:rings){const float age=now-r.born;if(strength(r,age)==0)continue;const float reach=radius(r,age)+Band*halfWidth(r,age);
  if(!any){x1=r.x-reach;y1=r.y-reach;x2=r.x+reach;y2=r.y+reach;any=true;}
  else{x1=std::min(x1,r.x-reach);y1=std::min(y1,r.y-reach);x2=std::max(x2,r.x+reach);y2=std::max(y2,r.y+reach);}}
 return any;}
Field gather(const std::vector<WaterSplash>& splashes,int level,float now,size_t most){
 Field f;f.now=now;const float gain=tune("water.ripple",1.4f);
 for(auto it=splashes.rbegin();it!=splashes.rend()&&f.rings.size()<most;++it){const auto&s=*it;if(s.level!=level)continue;const float p=std::clamp(s.power,.1f,3.f);
  auto add=[&](float delay,float amplitude,float speed,float wavelength,float life){Ring r;r.x=s.point.x;r.y=s.point.y;r.born=s.time+delay;r.amplitude=amplitude*gain;r.speed=speed;r.wavelength=wavelength;r.life=life;
   if(now-r.born<r.life&&now>=s.time)f.rings.push_back(r);};
  switch(s.kind){
   case WaterSplashKind::Shot:add(0,.007f*p,.38f,.13f,1.9f);add(.1f,.004f*p,.34f,.11f,1.6f);break;
   case WaterSplashKind::Step:add(0,.0055f*std::min(1.f,p/.4f),.3f,.12f,1.5f);break;
   case WaterSplashKind::Wake:add(0,.0045f*std::min(p,1.2f),.28f,.12f,1.4f);break;
   default:{const float q=std::pow(std::min(p,2.5f),.7f);add(0,.013f*q,.46f+.05f*p,.16f+.03f*p,2.4f);add(.12f,.009f*q,.42f+.04f*p,.13f+.02f*p,2.2f);add(.27f,.006f*q,.38f,.13f,2.f);}}}
 return f;}
Cut cut(const Field& field,float left,float near,float right,float far,float coarse,int fine,int budget){
 Cut out;out.fine=fine;float x1,y1,x2,y2;
 if(right<=left||far<=near)return out;
 if(!field.bounds(x1,y1,x2,y2)||x2<=left||x1>=right||y2<=near||y1>=far){out.still.push_back({left,near,right,far});return out;}
 // Snapped to the coarse grid laid from the basin's corner, inside the basin.
 const float gx1=std::max(left,left+std::floor((x1-left)/coarse)*coarse),gy1=std::max(near,near+std::floor((y1-near)/coarse)*coarse);
 const float gx2=std::min(right,left+std::ceil((x2-left)/coarse)*coarse),gy2=std::min(far,near+std::ceil((y2-near)/coarse)*coarse);
 if(gy1>near)out.still.push_back({left,near,right,gy1});if(gy2<far)out.still.push_back({left,gy2,right,far});
 if(gx1>left)out.still.push_back({left,gy1,gx1,gy2});if(gx2<right)out.still.push_back({gx2,gy1,right,gy2});
 for(float y=gy1;y<gy2-1e-4f;y+=coarse)for(float x=gx1;x<gx2-1e-4f;x+=coarse){const Cut::Cell c{x,y,std::min(x+coarse,gx2),std::min(y+coarse,gy2)};
  (field.touches(c.x1,c.y1,c.x2,c.y2)?out.moving:out.still).push_back(c);}
 if(!out.moving.empty()&&int(out.moving.size())*fine*fine>budget)out.fine=std::max(3,int(std::sqrt(float(budget)/float(out.moving.size()))));
 return out;}
}

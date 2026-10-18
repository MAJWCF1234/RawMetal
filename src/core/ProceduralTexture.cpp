#include "ProceduralTexture.h"
#include "../ThirdParty/openktg/gentexture.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>
#include <stdexcept>
namespace retro {
namespace {
float smooth(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
float sharp(float a,float b,float x,float power=1.4f){return std::pow(smooth(a,b,x),power);}
uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
std::array<float,3> color(uint32_t rgb){return {float((rgb>>16)&255),float((rgb>>8)&255),float(rgb&255)};}
}
std::vector<unsigned char> generateProceduralTexture(const std::vector<unsigned char>& recipe){
 if(recipe.size()!=32||std::memcmp(recipe.data(),"RMP1",4))throw std::runtime_error("Invalid procedural texture recipe");
 std::array<uint32_t,8> p{};std::memcpy(p.data(),recipe.data(),32);
 const uint32_t kind=p[1],w=p[2],h=p[3],seed=p[4];
 if(kind<1||kind>5||w<64||h<64||w>1024||h>1024||(w&(w-1))||(h&(h-1))||(p[5]|p[6]|p[7])>0xffffffu)
  throw std::runtime_error("Unsupported procedural texture recipe");
 static std::once_flag initialized;std::call_once(initialized,[]{InitTexgen();});
 GenTexture gradient(256,1);for(int i=0;i<256;++i)gradient.Data[i].Init(sU8(i),sU8(i),sU8(i),255);
 // Cloud/detail = soft macro structure. grit/sand = bandlimited mid crunch.
 // sand/white = photographic grit the PNGs have and Perlin alone cannot.
 // roll = mild horizontal sheet grain; drip = mild vertical runoff.
 GenTexture cloud{int(w),int(h)},detail{int(w),int(h)},grit{int(w),int(h)};
 GenTexture sand{int(w),int(h)},roll{int(w),int(h)},drip{int(w),int(h)};
 constexpr int mode=GenTexture::NoiseBandlimit|GenTexture::NoiseNormalize;
 constexpr int white=GenTexture::NoiseWhite|GenTexture::NoiseNormalize;
 cloud.Noise(gradient,1,1,5,.55f,int(seed&0x7fffffffu),mode);
 detail.Noise(gradient,4,4,4,.48f,int((seed^0x519abu)&0x7fffffffu),mode);
 grit.Noise(gradient,12,12,3,.42f,int((seed^0xa3c1u)&0x7fffffffu),mode);
 // freq == log2(resolution) → one white-noise cell per texel (OpenKTG freq is log2 cell count).
 sand.Noise(gradient,sand.ShiftX,sand.ShiftY,1,0.f,int((seed^0x2f4du)&0x7fffffffu),white);
 roll.Noise(gradient,2,9,3,.50f,int((seed^0x9173u)&0x7fffffffu),mode);
 drip.Noise(gradient,6,2,3,.50f,int((seed^0x6e21u)&0x7fffffffu),mode);
 const auto base=color(p[5]),oxide=color(p[6]),highlight=color(p[7]);
 std::vector<unsigned char> out(12+size_t(w)*h*4);std::memcpy(out.data(),"RMT1",4);std::memcpy(out.data()+4,&w,4);std::memcpy(out.data()+8,&h,4);
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
  size_t i=size_t(y)*w+x;
  float n=cloud.Data[i].r/65535.f,d=detail.Data[i].r/65535.f,g=grit.Data[i].r/65535.f;
  float s=sand.Data[i].r/65535.f,ro=roll.Data[i].r/65535.f,dr=drip.Data[i].r/65535.f;
  uint32_t random=hash(seed^hash(x*73856093u)^hash(y*19349663u));
  float grain=float(random&255)/255.f-.5f;
  float value=0,wear=0,dark=0,edgeLight=0,tint[3]{0,0,0};
  // Irregular pits: hash-chosen centers, not a visible lattice.
  uint32_t cell=16+(hash(seed^0x51u)&15u),cx=x/cell,cy=y/cell;
  uint32_t pitId=hash(seed^(cx*374761393u)^(cy*668265263u));
  float px=float(x%cell)-float(pitId%cell),py=float(y%cell)-float((pitId>>8)%cell);
  float pr=px*px+py*py;bool hasPit=(pitId%5u)<(kind==4||kind==1?2u:1u);
  if(kind==1){
   // Concrete: per-texel sand + warm mottling; soft pits, not stamped circles.
   value=(n-.5f)*30+(d-.5f)*18+(g-.5f)*16+(s-.5f)*42+grain*14;
   wear=sharp(.48f,.84f,n*.7f+d*.3f)*.16f;
   dark=(random%199u<4?16.f:0.f)+(s<.18f?10.f:0.f);
   tint[0]+=wear*8+((s>.65f)?6.f:0.f);tint[1]+=wear*5+((s>.65f)?4.f:0.f);tint[2]+=wear*2;
   if(hasPit&&pr<2.4f)dark+=pr<1.0f?22.f:9.f;
   if(random%47u==0){tint[0]+=11;tint[1]+=8;tint[2]+=3;}
  }
  if(kind==2){
   // Sign rust: hard oxide islands, less sand static than other kinds.
   float mask=n*.62f+d*.28f+g*.10f;
   wear=sharp(.28f,.62f,mask,2.2f)*.80f;
   value=(n-.5f)*12+(d-.5f)*14+(g-.5f)*12+(s-.5f)*14+grain*8-(1.f-wear)*7.f;
   dark=sharp(.48f,.82f,1.f-mask,1.4f)*16.f+(random%131u<3?18.f:0.f);
   if(hasPit&&pr<2.2f)dark+=14.f;
   if(wear>.5f&&s>.8f)edgeLight+=6.f;
  }
  if(kind==3){
   // Near-black panel: faint macro mottling + light sand, mild roll grain.
   value=(n-.5f)*8+(d-.5f)*7+(g-.5f)*8+(s-.5f)*10+grain*3.f+(ro-.5f)*4.f;
   wear=0;if(s>.88f&&(random&15u)==0)edgeLight+=3.f;
  }
  if(kind==4){
   // Service floor: 3x3 tiles (PNG seams ~84/170), thin grout, speck pits.
   uint32_t tile=w/3,tx=x%tile,ty=y%(h/3),distance=std::min({tx,ty,tile-1-tx,h/3-1-ty});
   float cellVar=float(hash((x/tile)+(y/(h/3))*19+seed)&255)/255.f-.5f;
   float stain=sharp(.28f,.60f,n*.45f+d*.55f,1.7f);
   value=(n-.5f)*20+(d-.5f)*16+(g-.5f)*14+(s-.5f)*40+cellVar*14+grain*8;
   dark=distance==0?32.f:distance==1?8.f:0.f;
   if(distance==0)edgeLight+=((tx+ty)&1)?2.f:0.f;
   dark+=stain*36.f+std::fabs(cellVar)*10.f;
   if(s<.16f)dark+=18.f;
   // Speck pits from sand thresholds beat regular circular stamps.
   if(s<.08f&&(random&3u)==0)dark+=28.f;
   if(hasPit&&pr<2.0f&&(pitId&1u))dark+=pr<0.9f?26.f:10.f;
   if((random%67u)<3)dark+=34.f;
   wear=stain*.18f;
   if(s>.78f&&(random&3u)==0)edgeLight+=6.f;
  }
  if(kind==5){
   // Weathered metal: organic oxide blooms, mild drip, no scrape lattice.
   float mask=n*.55f+d*.25f+dr*.20f;
   wear=sharp(.24f,.66f,mask,1.45f)*.72f;
   value=(n-.5f)*34+(d-.5f)*20+(g-.5f)*12+(s-.5f)*26+(ro-.5f)*10+(dr-.5f)*16+grain*8;
   value+=(wear-.28f)*24.f;
   dark=sharp(.40f,.78f,1.f-mask)*20.f+(random%149u<3?22.f:0.f);
   if(s>.84f)edgeLight+=10.f+wear*10.f;
   if(wear<.22f){tint[0]-=5;tint[1]+=2;tint[2]+=6;}
   if(wear>.55f){tint[0]+=6;tint[1]-=2;tint[2]-=4;}
   if(hasPit&&pr<2.0f)dark+=pr<0.9f?20.f:8.f;
  }
  for(size_t c=0;c<3;++c){
   float oxideMix=wear;
   if(kind==2)oxideMix=std::clamp(wear*(.88f+d*.2f),0.f,1.f);
   if(kind==5)oxideMix=std::clamp(wear*(.8f+n*.25f),0.f,1.f);
   float v=base[c]+(oxide[c]-base[c])*oxideMix+value-dark+edgeLight+tint[c];
   if(kind==1&&random%53u==0)v=v*.68f+highlight[c]*.32f;
   if(kind==2&&random%89u==0)v=v*.80f+highlight[c]*.20f;
   if(kind==4&&random%47u==0)v=v*.72f+highlight[c]*.28f;
   if(kind==5&&random%59u==0)v=v*.70f+highlight[c]*.30f;
   out[12+i*4+c]=static_cast<unsigned char>(std::clamp(std::lround(v),0l,255l));
  }
  out[12+i*4+3]=255;
 }
 return out;
}
}

#include "GpuRenderer.h"
#include "../core/FrameProfile.h"

#include <sstream>
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include "../vr/VrRuntime.h"
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <limits>
#include <bit>
#include <cmath>
#include <algorithm>
#include <chrono>
namespace retro {
namespace {
void check(VkResult result,const char* operation){if(result!=VK_SUCCESS)throw std::runtime_error(std::string(operation)+" (Vulkan "+std::to_string(result)+")");}
const uint32_t vertexShader[]=
#include "scene.vert.inc"
;
const uint32_t actorVertexShader[]=
#include "actor.vert.inc"
;
const uint32_t fragmentShader[]=
#include "scene.frag.inc"
;
const uint32_t compositeVertexShader[]=
#include "composite.vert.inc"
;
const uint32_t compositeFragmentShader[]=
#include "composite.frag.inc"
;
const uint32_t postFragmentShader[]=
#include "post.frag.inc"
;
const uint32_t occlusionFragmentShader[]=
#include "occlusion.frag.inc"
;
const uint32_t shadowFragmentShader[]=
#include "shadow.frag.inc"
;
const uint32_t lampShadowVertexShader[]=
#include "lampshadow.vert.inc"
;
const uint32_t lampShadowFragmentShader[]=
#include "lampshadow.frag.inc"
;
struct Vertex {float position[4],uv[2],lighting[4],light0[4],light1[4],surface[4],worldNormal[4];};
// Keep every position/UV/light at full precision. The vertex shader derives
// edges, UV differences and outward vectors instead of streaming duplicates.
using ActorTriangle=GpuRenderer::ActorTriangle;
static_assert(sizeof(ActorTriangle)==92&&sizeof(Vertex)*3==312);
struct ActorLighting {std::array<std::array<float,4>,6> normalBrightness,light0,light1,surfaceNormal;std::array<float,4> originEye;};
static_assert(sizeof(ActorLighting)==400);
constexpr size_t MaxGpuActors=1024;
struct Buffer {VkBuffer handle=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void* mapped=nullptr;VkDeviceSize size=0;};
struct Image {VkImage handle=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;};
struct EmissionLevel {int width,height;std::vector<std::array<float,4>> pixels;};
// Store-low / run-high: cart atlases stay small; Vulkan presents a bilinear-
// upsampled working set so trimming source resolution can be judged live.
std::vector<uint32_t> upsamplePixels(const std::vector<uint32_t>& source,int width,int height,int scale){
 if(scale<=1||width<1||height<1||source.size()!=size_t(width)*height)return source;
 int outW=width*scale,outH=height*scale;std::vector<uint32_t> out(size_t(outW)*outH);
 auto sample=[&](float x,float y){
  x=std::clamp(x,0.f,float(width-1));y=std::clamp(y,0.f,float(height-1));
  int x0=int(x),y0=int(y),x1=std::min(width-1,x0+1),y1=std::min(height-1,y0+1);float fx=x-x0,fy=y-y0;
  auto mix=[](uint32_t a,uint32_t b,float t){uint32_t o=0;for(int c=0;c<4;++c){int s=c*8;auto av=(a>>s)&255u,bv=(b>>s)&255u;o|=unsigned(av*(1-t)+bv*t)<<s;}return o;};
  auto a=mix(source[size_t(y0*width+x0)],source[size_t(y0*width+x1)],fx);auto b=mix(source[size_t(y1*width+x0)],source[size_t(y1*width+x1)],fx);return mix(a,b,fy);
 };
 for(int y=0;y<outH;++y)for(int x=0;x<outW;++x)out[size_t(y*outW+x)]=sample((x+.5f)/scale-.5f,(y+.5f)/scale-.5f);
 return out;
}
std::vector<uint8_t> upsampleRelief(const std::vector<uint8_t>& source,int width,int height,int scale){
 if(scale<=1||width<1||height<1||source.size()!=size_t(width)*height)return source;
 int outW=width*scale,outH=height*scale;std::vector<uint8_t> out(size_t(outW)*outH);
 for(int y=0;y<outH;++y)for(int x=0;x<outW;++x){
  float sx=(x+.5f)/scale-.5f,sy=(y+.5f)/scale-.5f;sx=std::clamp(sx,0.f,float(width-1));sy=std::clamp(sy,0.f,float(height-1));
  int x0=int(sx),y0=int(sy),x1=std::min(width-1,x0+1),y1=std::min(height-1,y0+1);float fx=sx-x0,fy=sy-y0;
  float a=source[size_t(y0*width+x0)]*(1-fx)+source[size_t(y0*width+x1)]*fx;float b=source[size_t(y1*width+x0)]*(1-fx)+source[size_t(y1*width+x1)]*fx;
  out[size_t(y*outW+x)]=uint8_t(std::lround(a*(1-fy)+b*fy));
 }
 return out;
}
std::vector<Point3> upsampleNormals(const std::vector<Point3>& source,int width,int height,int scale){
 if(scale<=1||width<1||height<1||source.size()!=size_t(width)*height)return source;
 int outW=width*scale,outH=height*scale;std::vector<Point3> out(size_t(outW)*outH);
 auto at=[&](int x,int y)->Point3{return source[size_t(std::clamp(y,0,height-1)*width+std::clamp(x,0,width-1))];};
 for(int y=0;y<outH;++y)for(int x=0;x<outW;++x){
  float sx=(x+.5f)/scale-.5f,sy=(y+.5f)/scale-.5f;int x0=int(std::floor(sx)),y0=int(std::floor(sy));float fx=sx-x0,fy=sy-y0;
  auto a=at(x0,y0)*(1-fx)+at(x0+1,y0)*fx,b=at(x0,y0+1)*(1-fx)+at(x0+1,y0+1)*fx,n=a*(1-fy)+b*fy;
  float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);out[size_t(y*outW+x)]=len>.0001f?n*(1.f/len):Point3{0,0,1};
 }
 return out;
}
float srgbLinear(float encoded){return encoded<=.04045f?encoded/12.92f:std::pow((encoded+.055f)/1.055f,2.4f);}
uint8_t srgbByteSlow(float linear){float encoded=linear<=.0031308f?linear*12.92f:1.055f*std::pow(linear,1.f/2.4f)-.055f;return uint8_t(std::lround(std::clamp(encoded,0.f,1.f)*255.f));}
struct SrgbEncoding {std::array<uint8_t,4096> candidates;std::array<float,255> upper;};
const SrgbEncoding&srgbEncoding(){
 static const auto result=[] {SrgbEncoding table{};
  for(size_t i=0;i<table.candidates.size();++i)table.candidates[i]=srgbByteSlow(float(i)/float(table.candidates.size()-1));
  for(size_t code=0;code<table.upper.size();++code){
   // The mathematical half-byte boundary seeds the search. Find the first
   // float whose reference encoding rounds upward, retaining the old encoder's
   // exact behaviour even at floating-point ties around that boundary.
   double encoded=(double(code)+.5)/255.;double linear=encoded<=.04045?encoded/12.92:std::pow((encoded+.055)/1.055,2.4);
   uint32_t seed=std::bit_cast<uint32_t>(float(linear)),lower=0,upper=std::bit_cast<uint32_t>(1.f);
   if(srgbByteSlow(std::bit_cast<float>(seed))>code)upper=seed;else lower=seed;
   while(upper-lower>1){uint32_t middle=lower+(upper-lower)/2;if(srgbByteSlow(std::bit_cast<float>(middle))>code)upper=middle;else lower=middle;}
   table.upper[code]=std::bit_cast<float>(upper);
  }
  return table;
 }();return result;
}
uint8_t srgbByte(float linear){
 if(!(linear>0))return 0;if(linear>=1)return 255;const auto&table=srgbEncoding();
 unsigned code=table.candidates[size_t(linear*float(table.candidates.size()-1))];
 // Bins select a nearby code; exact monotonic cutpoints correct the candidate.
 // There are no per-texel powers and no approximate uploaded colour values.
 while(code<255&&linear>=table.upper[code])++code;
 while(code>0&&linear<table.upper[code-1])--code;
 return uint8_t(code);
}
void completeRadianceMips(std::vector<EmissionLevel>&result){
 while(result.back().width>1||result.back().height>1){
  const auto&previous=result.back();int w=previous.width,h=previous.height,nw=std::max(1,w/2),nh=std::max(1,h/2);EmissionLevel next{nw,nh,std::vector<std::array<float,4>>(size_t(nw)*nh)};
  // Equal-area footprints include odd last rows/columns. Integer overlap
  // weights avoid boundary rounding and preserve the mean at every level.
  for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){std::array<double,4> sum{};int left=x*w,right=(x+1)*w,top=y*h,bottom=(y+1)*h;
   for(int sy=top/nh;sy<(bottom+nh-1)/nh;++sy)for(int sx=left/nw;sx<(right+nw-1)/nw;++sx){
    int wx=std::min((sx+1)*nw,right)-std::max(sx*nw,left),wy=std::min((sy+1)*nh,bottom)-std::max(sy*nh,top);double weight=double(wx)*wy;
    const auto&sample=previous.pixels[size_t(sy*w+sx)];for(size_t c=0;c<sum.size();++c)sum[c]+=sample[c]*weight;
   }
   auto&sample=next.pixels[size_t(y*nw+x)];for(size_t c=0;c<sum.size();++c)sample[c]=float(sum[c]/(double(w)*h));
  }
  result.push_back(std::move(next));
 }
}
std::vector<EmissionLevel> diffuseMips(int width,int height,const std::vector<uint32_t>&pixels){
 if(width<1||height<1||pixels.size()!=size_t(width)*height)throw std::runtime_error("Invalid diffuse atlas dimensions");
 static const auto linear=[] {std::array<float,256> values{};for(size_t i=0;i<values.size();++i)values[i]=srgbLinear(float(i)/255.f);return values;}();
 std::vector<EmissionLevel> result;result.push_back({width,height,{}});auto&base=result.back().pixels;base.reserve(pixels.size());
 for(auto p:pixels)base.push_back({linear[(p>>16)&255],linear[(p>>8)&255],linear[p&255],float(p>>24)/255.f});
 completeRadianceMips(result);return result;
}
std::vector<std::vector<uint8_t>> diffuseBytes(int width,int height,const std::vector<uint32_t>&pixels){
 const auto radiance=diffuseMips(width,height,pixels);std::vector<std::vector<uint8_t>> result;result.reserve(radiance.size());
 // Level zero remains the exact source atlas, including alpha and atlas seams.
 result.emplace_back(pixels.size()*4);std::memcpy(result.back().data(),pixels.data(),result.back().size());
 for(size_t level=1;level<radiance.size();++level){const auto&source=radiance[level].pixels;std::vector<uint8_t> bytes(source.size()*4);
  for(size_t i=0;i<source.size();++i){bytes[i*4]=srgbByte(source[i][2]);bytes[i*4+1]=srgbByte(source[i][1]);bytes[i*4+2]=srgbByte(source[i][0]);bytes[i*4+3]=uint8_t(std::lround(std::clamp(source[i][3],0.f,1.f)*255.f));}
  result.push_back(std::move(bytes));
 }
 return result;
}
// Decode before filtering: averaging encoded phosphor values darkens small CRT
// text, and decoding after bilinear interpolation does not conserve radiance.
std::vector<EmissionLevel> emissionMips(int width,int height,const std::vector<uint32_t>& pixels){
 if(width<1||height<1||pixels.size()!=size_t(width)*height)throw std::runtime_error("Invalid emission atlas dimensions");
 static const auto linear=[] {std::array<float,256> values{};for(size_t i=0;i<values.size();++i)values[i]=std::pow(float(i)/255.f,2.2f);return values;}();
 std::vector<EmissionLevel> result;result.push_back({width,height,{}});auto&base=result.back().pixels;base.reserve(pixels.size());
 for(auto p:pixels)base.push_back({linear[(p>>16)&255],linear[(p>>8)&255],linear[p&255],float(p>>24)/255.f});
 completeRadianceMips(result);return result;
}
std::vector<EmissionLevel> normalMips(int width,int height,const std::vector<Point3>&normals){
 if(width<1||height<1||normals.size()!=size_t(width)*height)throw std::runtime_error("Invalid normal atlas dimensions");
 std::vector<EmissionLevel> result;result.push_back({width,height,{}});auto&base=result.back().pixels;base.reserve(normals.size());
 // Keep unnormalized first moments through all mip reductions. Renormalizing
 // intermediate levels biases later directions toward incoherent texel groups.
 for(auto n:normals)base.push_back({n.x,n.y,n.z,1.f});completeRadianceMips(result);return result;
}
std::vector<std::vector<uint8_t>> normalBytes(int width,int height,const std::vector<Point3>&normals){
 const auto moments=normalMips(width,height,normals);std::vector<std::vector<uint8_t>> result;result.reserve(moments.size());
 for(const auto&level:moments){std::vector<uint8_t> bytes(level.pixels.size()*8);
  for(size_t i=0;i<level.pixels.size();++i){auto sample=level.pixels[i];float length=std::sqrt(sample[0]*sample[0]+sample[1]*sample[1]+sample[2]*sample[2]);std::array<int16_t,4> encoded{};
   // Filtering interpolates the true first moment directly, so cancellation
   // and unequal coherence between neighbouring mip texels remain correct.
   for(size_t c=0;c<3;++c)encoded[c]=int16_t(std::clamp(sample[c],-1.f,1.f)*32767.f);
   encoded[3]=int16_t(std::clamp(length,0.f,1.f)*32767.f);std::memcpy(bytes.data()+i*8,encoded.data(),8);
  }
  result.push_back(std::move(bytes));
 }
 return result;
}
uint16_t emissionHalf(float value){
 uint32_t bits=std::bit_cast<uint32_t>(value),sign=(bits>>16)&0x8000u,mantissa=bits&0x7fffffu;int exponent=int((bits>>23)&255)-127+15;
 if(exponent<=0){if(exponent<-10)return uint16_t(sign);mantissa|=0x800000u;int shift=14-exponent;uint32_t rounded=mantissa>>shift,remainder=mantissa&((1u<<shift)-1),tie=1u<<(shift-1);if(remainder>tie||(remainder==tie&&(rounded&1)))++rounded;return uint16_t(sign|rounded);}
 if(exponent>=31)return uint16_t(sign|0x7c00u|(mantissa?0x200u:0u));
 uint32_t rounded=mantissa>>13,remainder=mantissa&0x1fffu;if(remainder>0x1000u||(remainder==0x1000u&&(rounded&1)))++rounded;
 if(rounded==0x400u){rounded=0;++exponent;}return uint16_t(sign|(uint32_t(exponent)<<10)|rounded);
}
float emissionFloat(uint16_t bits){
 float value=(bits&0x7c00u)==0?std::ldexp(float(bits&0x3ffu),-24):std::ldexp(1.f+float(bits&0x3ffu)/1024.f,int((bits>>10)&31)-15);return (bits&0x8000u)?-value:value;
}
std::vector<std::vector<uint8_t>> emissionBytes(const std::vector<EmissionLevel>& levels){
 std::vector<std::vector<uint8_t>> result;result.reserve(levels.size());
 for(const auto&level:levels){std::vector<uint8_t> bytes(level.pixels.size()*8);for(size_t i=0;i<level.pixels.size();++i)for(size_t c=0;c<4;++c){auto half=emissionHalf(level.pixels[i][c]);std::memcpy(bytes.data()+i*8+c*2,&half,2);}result.push_back(std::move(bytes));}
 return result;
}
}
bool GpuRenderer::testDiffuseMips(){
 // Dense samples, bin edges and adjacent floats at every byte transition must
 // match the retained slow encoding, including its float tie behaviour.
 for(uint32_t i=0;i<=1048576;++i){float value=float(i)/1048576.f;if(srgbByte(value)!=srgbByteSlow(value))return false;}
 const auto&encoding=srgbEncoding();
 for(size_t code=0;code<encoding.upper.size();++code){float boundary=encoding.upper[code];for(float value:{std::nextafter(boundary,0.f),boundary,std::nextafter(boundary,1.f)})if(srgbByte(value)!=srgbByteSlow(value))return false;
  if(srgbByte(std::nextafter(boundary,0.f))!=code||srgbByte(boundary)!=code+1)return false;
 }
 for(size_t i=0;i<encoding.candidates.size();++i){float boundary=float(i)/float(encoding.candidates.size()-1);for(float value:{std::nextafter(boundary,0.f),boundary,std::nextafter(boundary,1.f)})if(srgbByte(value)!=srgbByteSlow(value))return false;}
 // Validate the linear intermediate and the quantized sRGB upload separately.
 for(auto size:{std::array<int,2>{1,1},{3,5},{5,3},{1,7},{7,1},{9,7},{32,32},{348,307}}){
  int w=size[0],h=size[1];std::vector<uint32_t> source(size_t(w)*h);std::array<double,4> expected{};
  for(size_t i=0;i<source.size();++i){uint32_t r=uint32_t((i*73+17)%256),g=uint32_t((i*19+63)%256),b=uint32_t((i*131+241)%256),a=uint32_t((i*7+29)%256);source[i]=(a<<24)|(r<<16)|(g<<8)|b;
   expected[0]+=srgbLinear(float(r)/255.f);expected[1]+=srgbLinear(float(g)/255.f);expected[2]+=srgbLinear(float(b)/255.f);expected[3]+=double(a)/255.;
  }
  for(auto&channel:expected)channel/=source.size();auto levels=diffuseMips(w,h,source);auto bytes=diffuseBytes(w,h,source);
  if(bytes[0].size()!=source.size()*4||std::memcmp(bytes[0].data(),source.data(),bytes[0].size())!=0)return false;
  for(size_t mip=0;mip<levels.size();++mip){const auto&level=levels[mip];if(level.width!=std::max(1,w>>mip)||level.height!=std::max(1,h>>mip)||bytes[mip].size()!=level.pixels.size()*4)return false;
   std::array<double,4> average{},uploaded{};
   for(size_t i=0;i<level.pixels.size();++i)for(size_t c=0;c<4;++c){float value=level.pixels[i][c];uint8_t encoded=bytes[mip][i*4+(c==3?3:2-c)];float decoded=c==3?float(encoded)/255.f:srgbLinear(float(encoded)/255.f);
    if(!std::isfinite(value)||value<0||value>1||std::fabs(value-decoded)>(c==3?.00197f:.0045f))return false;average[c]+=value;uploaded[c]+=decoded;
   }
   for(size_t c=0;c<4;++c)if(std::fabs(average[c]/level.pixels.size()-expected[c])>.000002||std::fabs(uploaded[c]/level.pixels.size()-expected[c])>(c==3?.00197:.0045))return false;
  }
  if(levels.back().width!=1||levels.back().height!=1)return false;
 }
 std::vector<uint32_t> checker(32*32);for(int y=0;y<32;++y)for(int x=0;x<32;++x)checker[size_t(y*32+x)]=((x+y)&1)?0xffffffffu:0xff000000u;
 auto checkerLevels=diffuseMips(32,32,checker);auto checkerBytes=diffuseBytes(32,32,checker);
 for(size_t level=1;level<checkerLevels.size();++level)for(size_t i=0;i<checkerLevels[level].pixels.size();++i){auto sample=checkerLevels[level].pixels[i];if(sample[0]!=.5f||sample[1]!=.5f||sample[2]!=.5f||sample[3]!=1)return false;auto offset=i*4;if(checkerBytes[level][offset]!=188||checkerBytes[level][offset+1]!=188||checkerBytes[level][offset+2]!=188||checkerBytes[level][offset+3]!=255)return false;}
 auto neutral=diffuseBytes(7,5,std::vector<uint32_t>(35,0x49757575u));for(const auto&level:neutral)for(size_t i=0;i<level.size();i+=4)if(level[i]!=117||level[i+1]!=117||level[i+2]!=117||level[i+3]!=73)return false;
 std::vector<uint32_t> edge(15,0xff000000u);edge.back()=0xffffffffu;for(const auto&level:diffuseMips(3,5,edge)){double mean=0;for(const auto&sample:level.pixels)mean+=sample[0];if(std::fabs(mean/level.pixels.size()-1./15.)>.000001)return false;}
 return true;
}
bool GpuRenderer::testGameDiffuseEncoding(const std::vector<const SoftwareRenderer::Texture*>&textures,std::array<uint64_t,4>&statistics,std::array<double,2>&milliseconds){
 statistics={0,0,14695981039346656037ull,14695981039346656037ull};milliseconds={};srgbByte(.5f);bool equal=true;
 for(const auto*texture:textures){if(!texture||texture->additive||texture->width<1||texture->height<1||texture->pixels.empty())continue;
  auto levels=diffuseMips(texture->width,texture->height,texture->pixels);size_t count=0;for(size_t mip=1;mip<levels.size();++mip)count+=levels[mip].pixels.size()*3;std::array<std::vector<uint8_t>,2> encoded;
  for(int reference=0;reference<2;++reference){auto&bytes=encoded[reference];bytes.resize(count);size_t cursor=0;auto start=std::chrono::steady_clock::now();
   if(reference){for(size_t mip=1;mip<levels.size();++mip)for(const auto&pixel:levels[mip].pixels)for(size_t channel=0;channel<3;++channel)bytes[cursor++]=srgbByteSlow(pixel[channel]);}
   else{for(size_t mip=1;mip<levels.size();++mip)for(const auto&pixel:levels[mip].pixels)for(size_t channel=0;channel<3;++channel)bytes[cursor++]=srgbByte(pixel[channel]);}
   milliseconds[reference]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   for(auto byte:bytes){statistics[reference+2]^=byte;statistics[reference+2]*=1099511628211ull;}
  }
  equal&=encoded[0]==encoded[1];++statistics[0];statistics[1]+=count;
 }
 return equal;
}
bool GpuRenderer::testNormalMips(){
 for(auto size:{std::array<int,2>{1,1},{3,5},{5,3},{1,7},{7,1},{9,7},{32,32}}){
  int w=size[0],h=size[1];std::vector<Point3> source(size_t(w)*h);std::array<double,3> expected{};
  for(size_t i=0;i<source.size();++i){float x=float(int((i*73+17)%201)-100)/125.f,y=float(int((i*19+63)%201)-100)/125.f,z=.4f;float inv=1.f/std::sqrt(x*x+y*y+z*z);source[i]={x*inv,y*inv,z*inv};expected[0]+=source[i].x;expected[1]+=source[i].y;expected[2]+=source[i].z;}
  for(auto&channel:expected)channel/=source.size();auto levels=normalMips(w,h,source);auto bytes=normalBytes(w,h,source);
  for(size_t mip=0;mip<levels.size();++mip){const auto&level=levels[mip];if(level.width!=std::max(1,w>>mip)||level.height!=std::max(1,h>>mip)||bytes[mip].size()!=level.pixels.size()*8)return false;std::array<double,3> average{};
   for(size_t i=0;i<level.pixels.size();++i){const auto&moment=level.pixels[i];std::array<int16_t,4> encoded{};std::memcpy(encoded.data(),bytes[mip].data()+i*8,8);float alpha=float(encoded[3])/32767.f;
    if(alpha<0||alpha>1)return false;
    for(size_t c=0;c<3;++c){average[c]+=moment[c];float recovered=float(encoded[c])/32767.f;if(!std::isfinite(recovered)||std::fabs(recovered-moment[c])>.000065f)return false;}
   }
   for(size_t c=0;c<3;++c)if(std::fabs(average[c]/level.pixels.size()-expected[c])>.000002)return false;
  }
 }
 // An incoherent group has a short mean vector, and must not have the same
 // influence on the next direction as three coherent groups of equal area.
 std::vector<Point3> pathological(16,{.6f,0,.8f});float x=std::sqrt(.96f);
 pathological[0]={x,0,.2f};pathological[1]={-x,0,.2f};pathological[4]={x,0,.2f};pathological[5]={-x,0,.2f};
 auto moments=normalMips(4,4,pathological);auto bytes=normalBytes(4,4,pathological);auto mean=moments.back().pixels[0];
 if(std::fabs(mean[0]-.45f)>.000001f||std::fabs(mean[1])>.000001f||std::fabs(mean[2]-.65f)>.000001f)return false;
 std::array<int16_t,4> encoded{};std::memcpy(encoded.data(),bytes.back().data(),8);float length=std::sqrt(.45f*.45f+.65f*.65f);
 if(std::fabs(float(encoded[0])/32767.f-.45f)>.00004f||std::fabs(float(encoded[2])/32767.f-.65f)>.00004f||std::fabs(float(encoded[3])/32767.f-length)>.00004f)return false;
 // Bilinear samples of groups with different coherence retain their weighted
 // direction; unit RGB plus a separate interpolated length cannot do this.
 std::array<int16_t,4> left{},right{};std::memcpy(left.data(),bytes[1].data(),8);std::memcpy(right.data(),bytes[1].data()+8,8);
 float xMoment=float(left[0]+right[0])/(2.f*32767.f),zMoment=float(left[2]+right[2])/(2.f*32767.f);float bilinearLength=std::sqrt(xMoment*xMoment+zMoment*zMoment);
 if(std::fabs(xMoment-.3f)>.00004f||std::fabs(zMoment-.5f)>.00004f||std::fabs(xMoment/bilinearLength-.51449575f)>.0001f||std::fabs(zMoment/bilinearLength-.85749293f)>.0001f)return false;
 // Cancellation stays finite and fully rough, and an odd border survives.
 auto cancelled=normalBytes(2,1,{{1,0,0},{-1,0,0}});std::memcpy(encoded.data(),cancelled.back().data(),8);if(encoded[0]!=0||encoded[1]!=0||encoded[2]!=0||encoded[3]!=0)return false;
 std::vector<Point3> edge(15,{0,0,1});edge.back()={1,0,0};auto last=normalMips(3,5,edge).back().pixels[0];if(std::fabs(last[0]-1.f/15)>.000001f||std::fabs(last[2]-14.f/15)>.000001f)return false;
 return true;
}
bool GpuRenderer::testEmissionMips(){
 for(auto size:{std::array<int,2>{1,1},{3,5},{5,3},{1,7},{7,1},{9,7},{32,32},{348,307}}){
  int w=size[0],h=size[1];std::vector<uint32_t> source(size_t(w)*h);std::array<double,4> expected{};
  for(size_t i=0;i<source.size();++i){uint32_t r=uint32_t((i*73+17)%256),g=uint32_t((i*19+63)%256),b=uint32_t((i*131+241)%256),a=uint32_t((i*7+29)%256);source[i]=(a<<24)|(r<<16)|(g<<8)|b;
   expected[0]+=std::pow(double(r)/255.,2.2);expected[1]+=std::pow(double(g)/255.,2.2);expected[2]+=std::pow(double(b)/255.,2.2);expected[3]+=double(a)/255.;
  }
  for(auto&channel:expected)channel/=source.size();auto levels=emissionMips(w,h,source);auto bytes=emissionBytes(levels);
  for(size_t mip=0;mip<levels.size();++mip){const auto&level=levels[mip];if(level.width!=std::max(1,w>>mip)||level.height!=std::max(1,h>>mip)||level.pixels.size()!=size_t(level.width)*level.height||bytes[mip].size()!=level.pixels.size()*8)return false;
   std::array<double,4> average{},uploaded{};
   for(size_t i=0;i<level.pixels.size();++i)for(size_t c=0;c<4;++c){float value=level.pixels[i][c];uint16_t half=0;std::memcpy(&half,bytes[mip].data()+i*8+c*2,2);float decoded=emissionFloat(half);
    if(!std::isfinite(value)||value<0||value>1||!std::isfinite(decoded)||std::fabs(value-decoded)>.00025f)return false;average[c]+=value;uploaded[c]+=decoded;
   }
   for(size_t c=0;c<4;++c)if(std::fabs(average[c]/level.pixels.size()-expected[c])>.000002||std::fabs(uploaded[c]/level.pixels.size()-expected[c])>.00025)return false;
  }
  if(levels.back().width!=1||levels.back().height!=1)return false;
 }
 // An odd edge texel must survive all the way to 1x1 instead of being dropped.
 std::vector<uint32_t> edge(15,0xff000000u);edge.back()=0xffffffffu;auto odd=emissionMips(3,5,edge);
 for(const auto&level:odd){double mean=0;for(const auto&p:level.pixels)mean+=p[0];if(std::fabs(mean/level.pixels.size()-1./15.)>.000001)return false;}
 auto black=emissionMips(7,5,std::vector<uint32_t>(35,0xff000000u));for(const auto&level:black)for(const auto&p:level.pixels)if(p[0]!=0||p[1]!=0||p[2]!=0||p[3]!=1)return false;
 // Include IEEE half subnormals and round-to-nearest-even ties used by dim logs.
 if(emissionHalf(0)!=0||emissionHalf(1)!=0x3c00||emissionHalf(std::ldexp(1.f,-24))!=1||emissionHalf(std::ldexp(1.f,-25))!=0||emissionHalf(1.f+std::ldexp(1.f,-11))!=0x3c00||emissionHalf(1.f+3*std::ldexp(1.f,-11))!=0x3c02)return false;
 return true;
}
struct GpuRenderer::Impl {
 VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;uint32_t family=0;
 HWND hwnd=nullptr;VkSurfaceKHR surface=VK_NULL_HANDLE;VkSwapchainKHR swapchain=VK_NULL_HANDLE;VkFormat swapFormat=VK_FORMAT_UNDEFINED;VkExtent2D swapExtent{};std::vector<VkImage> swapImages;std::vector<VkImageView> swapViews;std::vector<VkFramebuffer> swapFrames;std::vector<VkSemaphore> readySemaphores;VkRenderPass compositePass=VK_NULL_HANDLE;VkDescriptorSetLayout compositeSetLayout=VK_NULL_HANDLE;VkPipelineLayout compositeLayout=VK_NULL_HANDLE;VkPipeline compositePipeline=VK_NULL_HANDLE;VkDescriptorPool compositeDescriptors=VK_NULL_HANDLE;Image overlay;bool overlayInitialized=false;int overlayWidth=0,overlayHeight=0;
 int surfaceClientWidth=0,surfaceClientHeight=0;
 VkExtent2D mirrorExtent{};VkRenderPass mirrorPass=VK_NULL_HANDLE;VkPipeline mirrorPipeline=VK_NULL_HANDLE;std::vector<VkImageView> mirrorViews;std::vector<VkFramebuffer> mirrorFrames;std::uint64_t mirrorPresented=0;
 bool vrMode=false;int vrEye=0;std::array<Image,2> vrOutputs{};std::array<float,24> vrProjection{};VkDescriptorSetLayout projectionLayout=VK_NULL_HANDLE;VkDescriptorPool projectionPool=VK_NULL_HANDLE;std::array<VkDescriptorSet,2> projectionSets{};std::array<Buffer,2> projectionBuffers{};
 VkCommandPool pool=VK_NULL_HANDLE;
 // Two frames in flight: CPU prepares frame N while GPU renders frame N-1.
 // Each slot owns its command buffer, fence, dynamic vertex buffer, overlay
 // staging buffer and swapchain-acquire semaphore so neither frame races
 // the other for write access to these mutable resources.
 static constexpr int FrameCount=2;
 int frameIndex=0;
 size_t frameVertexCapacity=0,frameActorCapacity=0;
 std::array<VkCommandBuffer,FrameCount> commands{};
 std::array<VkFence,FrameCount> fences{};
 std::array<bool,FrameCount> frameInFlight{};
 std::array<Buffer,FrameCount> vertexBuffers{};
 std::array<Buffer,FrameCount> actorVertexBuffers{};
 std::array<std::vector<Buffer>,FrameCount> retiredBuffers{};
 std::array<Buffer,FrameCount> overlayBuffers{};
 std::array<VkSemaphore,FrameCount> acquiredSems{};
 // Per-frame composite descriptor sets (each frame references the same shared
 // render target images, but we need separate sets so we can update them while
 // the other frame's set is still in use by the GPU).
 std::array<VkDescriptorSet,FrameCount> compositeSets{};
 // Dedicated fence + command buffer for bounded batches of cold atlases.
 // Flushes wait before staging reuse; warmed frames submit no upload work.
 VkCommandBuffer uploadCommand=VK_NULL_HANDLE;VkFence uploadFence=VK_NULL_HANDLE;
 static constexpr size_t UploadBatchLimit=16*1024*1024;
 struct PendingUpload {Image* image;std::vector<std::vector<uint8_t>> levels;std::vector<VkBufferImageCopy> copies;};
 std::vector<PendingUpload> pendingUploads;size_t pendingUploadBytes=0;Buffer atlasStaging;std::array<std::uint64_t,4> uploadStats{};
 VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;VkDescriptorPool descriptors=VK_NULL_HANDLE;
 VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;VkRenderPass renderPass=VK_NULL_HANDLE;VkPipeline opaque=VK_NULL_HANDLE,additive=VK_NULL_HANDLE,transparent=VK_NULL_HANDLE;
 VkPipeline actorOpaque=VK_NULL_HANDLE,actorAdditive=VK_NULL_HANDLE,actorTransparent=VK_NULL_HANDLE;
 VkSampler wrap=VK_NULL_HANDLE,clamp=VK_NULL_HANDLE,sceneSampler=VK_NULL_HANDLE,pointSampler=VK_NULL_HANDLE;VkFramebuffer framebuffer=VK_NULL_HANDLE;
 Image target,sceneColor,depth,blackEmission;Buffer readback,dynamicUpload;
 // Post pass (HDR, non-VR): reads scene RGB + planar depth in alpha, writes the
 // final linear image that both readback and the window composite consume.
 struct FullscreenStage{bool lampAccess=false;Image image;VkRenderPass pass=VK_NULL_HANDLE;VkFramebuffer framebuffer=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;
  VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;VkDescriptorPool pool=VK_NULL_HANDLE;std::array<VkDescriptorSet,FrameCount> sets{};uint32_t samplers=1;};
 // Occlusion stage: AO + one-bounce light. Compose stage: denoise, haze,
 // volumetric lamps, bloom, grade. Both read scene RGB + planar depth (alpha).
 FullscreenStage occlusionStage,shadowStage,composeStage;
 bool postAvailable=false,postEnabled=true,postDebugOcclusion=false;bool postRan=false;
 Image& finalImage(){return postRan?composeStage.image:target;}
 void destroyStageTarget(FullscreenStage& stage){if(stage.framebuffer)vkDestroyFramebuffer(device,stage.framebuffer,nullptr);stage.framebuffer=VK_NULL_HANDLE;destroy(stage.image);}
 void destroyStage(FullscreenStage& stage){destroyStageTarget(stage);if(stage.pipeline)vkDestroyPipeline(device,stage.pipeline,nullptr);if(stage.layout)vkDestroyPipelineLayout(device,stage.layout,nullptr);if(stage.setLayout)vkDestroyDescriptorSetLayout(device,stage.setLayout,nullptr);if(stage.pool)vkDestroyDescriptorPool(device,stage.pool,nullptr);if(stage.pass)vkDestroyRenderPass(device,stage.pass,nullptr);}
 struct Material {Image color,normal,emission,relief;VkDescriptorSet set=VK_NULL_HANDLE;bool additive=false,transparent=false;uint32_t sortKey=0,colorLevels=1;};
 std::unordered_map<uint64_t,Material> materials;
 int presentScaleCached=0;
 const void* lastPixels=nullptr;uint64_t lastGeneration=0,lastMaterial=0;uint32_t lastSort=0;
 void clearMaterials(){
  vkDeviceWaitIdle(device);
  for(auto&entry:materials){destroy(entry.second.color);destroy(entry.second.normal);destroy(entry.second.emission);destroy(entry.second.relief);}
  materials.clear();lastPixels=nullptr;lastGeneration=0;lastMaterial=0;lastSort=0;
  for(auto&entry:staticCaches){destroy(entry.second.buffer);destroy(entry.second.shadowIndices);}staticCaches.clear();std::fill(lampDirty.begin(),lampDirty.end(),uint8_t(2));
  // Replayed actor batches carry material keys; stale keys would throw at draw.
  actorEmitCaches.clear();actorEmitHits=0;
  if(descriptors)vkResetDescriptorPool(device,descriptors,0);
 }
 struct Batch {uint64_t material;uint32_t start,count;bool clear=false,cache=false;int cacheSlot=-1;uint32_t sortKey=0;bool actor=false;};
 // Opaque static triangles grouped by 4 m cell and material for lamp shadow maps.
 struct ShadowRun {uint64_t material;uint32_t first,count;float lo[3],hi[3];};
 struct StaticCache {uint64_t key=0;Buffer buffer;std::vector<Vertex> vertices;std::vector<Batch> batches;Buffer shadowIndices;std::vector<ShadowRun> shadowRuns;std::array<float,6> bounds{};};
 // ── Per-pixel lamps ──────────────────────────────────────────────────────
 // Static lamps light the static world per pixel. Each lamp owns six faces of
 // a radial-depth shadow atlas, drawn from the static caches and reused until
 // geometry near it is rebuilt or the resident lamp set changes.
 static constexpr int MaxLamps=256,LampsPerCell=16,MaxLampCells=16384;static constexpr uint32_t ShadowSize=256;
 // eye/basis: the scene camera, rewritten every frame for the post stages.
 struct LampHeader {float origin[4];int32_t size[4];int32_t flags[4];float eye[4];float basis[4];};
 static constexpr size_t LampBufferBytes=sizeof(LampHeader)+size_t(MaxLamps)*2*16+size_t(MaxLampCells)*LampsPerCell*4;
 std::array<Buffer,FrameCount> lampBuffers{};std::array<uint64_t,FrameCount> lampUploaded{};
 std::vector<uint8_t> lampData;uint64_t lampGeneration=1,lampKey=0;bool pixelLighting=false;
 // lampDirty: 0 current, 1 stale (geometry nearby rebuilt), 2 never drawn.
 std::vector<std::array<float,5>> lamps;std::vector<uint8_t> lampDirty;std::vector<std::array<float,6>> shadowDirtyBounds;
 Image shadowAtlas;std::vector<VkImageView> shadowLayerViews;std::vector<VkFramebuffer> shadowFrames;int shadowCapacity=0;bool shadowLayoutReady=false;
 VkRenderPass shadowPass=VK_NULL_HANDLE;VkPipeline shadowPipeline=VK_NULL_HANDLE;VkPipelineLayout shadowLayout=VK_NULL_HANDLE;VkSampler shadowSampler=VK_NULL_HANDLE;
 std::uint64_t shadowFacesDrawn=0;
 void destroyShadowAtlas(){
  for(auto frame:shadowFrames)vkDestroyFramebuffer(device,frame,nullptr);shadowFrames.clear();
  for(auto view:shadowLayerViews)vkDestroyImageView(device,view,nullptr);shadowLayerViews.clear();
  destroy(shadowAtlas);shadowCapacity=0;
 }
 void createShadowAtlas(int capacity){
  vkDeviceWaitIdle(device);destroyShadowAtlas();
  uint32_t layers=uint32_t(capacity)*6;
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=VK_FORMAT_D16_UNORM;info.extent={ShadowSize,ShadowSize,1};info.mipLevels=1;info.arrayLayers=layers;info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateImage(device,&info,nullptr,&shadowAtlas.handle),"Create lamp shadow atlas");
  VkMemoryRequirements req;vkGetImageMemoryRequirements(device,shadowAtlas.handle,&req);VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  check(vkAllocateMemory(device,&alloc,nullptr,&shadowAtlas.memory),"Allocate lamp shadow atlas");check(vkBindImageMemory(device,shadowAtlas.handle,shadowAtlas.memory,0),"Bind lamp shadow atlas");
  VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=shadowAtlas.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;view.format=VK_FORMAT_D16_UNORM;view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,layers};
  check(vkCreateImageView(device,&view,nullptr,&shadowAtlas.view),"Create lamp shadow view");
  for(uint32_t layer=0;layer<layers;++layer){
   view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,layer,1};VkImageView face=VK_NULL_HANDLE;check(vkCreateImageView(device,&view,nullptr,&face),"Create lamp shadow face");shadowLayerViews.push_back(face);
   VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fb.renderPass=shadowPass;fb.attachmentCount=1;fb.pAttachments=&face;fb.width=fb.height=ShadowSize;fb.layers=1;VkFramebuffer frame=VK_NULL_HANDLE;check(vkCreateFramebuffer(device,&fb,nullptr,&frame),"Create lamp shadow framebuffer");shadowFrames.push_back(frame);
  }
  shadowCapacity=capacity;shadowLayoutReady=false;std::fill(lampDirty.begin(),lampDirty.end(),uint8_t(2));
  VkDescriptorImageInfo image{shadowSampler,shadowAtlas.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  for(auto set:projectionSets){VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set;write.dstBinding=3;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;vkUpdateDescriptorSets(device,1,&write,0,nullptr);}
 }
 void renderLampShadows(VkCommandBuffer cmd){
  if(!pixelLighting||!shadowPipeline||!shadowCapacity)return;
  if(!shadowLayoutReady){
   VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=shadowAtlas.handle;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,uint32_t(shadowCapacity)*6};b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);shadowLayoutReady=true;
   // Faces not drawn yet read as "nothing blocks the lamp": clear the atlas to far.
   VkImageMemoryBarrier toClear=b;toClear.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;toClear.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;toClear.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;toClear.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&toClear);
   VkClearDepthStencilValue farDepth{1.f,0};VkImageSubresourceRange all{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,uint32_t(shadowCapacity)*6};vkCmdClearDepthStencilImage(cmd,shadowAtlas.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&farDepth,1,&all);
   VkImageMemoryBarrier toRead=b;toRead.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;toRead.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;toRead.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;toRead.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&toRead);
  }
  // A rebuilt cache stales only the lamps whose range reaches it.
  for(const auto&bounds:shadowDirtyBounds)for(size_t i=0;i<lamps.size();++i){
   const auto&lamp=lamps[i];float d2=0;for(int k=0;k<3;++k){float c=std::clamp(lamp[size_t(k)],bounds[size_t(k)],bounds[size_t(k)+3])-lamp[size_t(k)];d2+=c*c;}
   if(d2<lamp[3]*lamp[3]&&!lampDirty[i])lampDirty[i]=1;
  }
  shadowDirtyBounds.clear();
  // A streamed chunk can bring dozens of lamps at once; six faces each in
  // one frame spiked the lift fall past 50 ms. Draw the nearest dirty lamps
  // first, a bounded number per frame; undrawn faces read as unblocked.
  // A cold start (nothing drawn yet) takes the whole set in one frame, so the
  // first frame of a map is complete; the budget covers lamps streamed in later.
  bool coldStart=true;for(size_t i=0;i<std::min(lamps.size(),size_t(shadowCapacity));++i)if(lampDirty[i]!=2){coldStart=false;break;}
  const int budget=coldStart?int(lamps.size()):10;std::vector<std::pair<float,size_t>> queue;
  for(size_t i=0;i<std::min(lamps.size(),size_t(shadowCapacity));++i)if(lampDirty[i]){float dx=lamps[i][0]-viewState[0],dy=lamps[i][1]-viewState[1],dz=lamps[i][2]-viewState[2];queue.push_back({dx*dx+dy*dy+dz*dz,i});}
  if(queue.empty())return;
  if(int(queue.size())>budget){std::nth_element(queue.begin(),queue.begin()+budget,queue.end());queue.resize(size_t(budget));}
  VkViewport viewport{0,0,float(ShadowSize),float(ShadowSize),0,1};VkRect2D scissor{{0,0},{ShadowSize,ShadowSize}};
  VkClearValue clear{};clear.depthStencil={1,0};
  for(const auto&entry:queue){size_t i=entry.second;lampDirty[i]=0;
   const auto&lamp=lamps[i];float range2=lamp[3]*lamp[3];
   for(int face=0;face<6;++face){
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=shadowPass;begin.framebuffer=shadowFrames[i*6+size_t(face)];begin.renderArea.extent={ShadowSize,ShadowSize};begin.clearValueCount=1;begin.pClearValues=&clear;
    vkCmdBeginRenderPass(cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowPipeline);
    static constexpr float faces[6][8]={{1,0,0,0,0,0,1,0},{-1,0,0,0,0,0,1,0},{0,1,0,0,0,0,1,0},{0,-1,0,0,0,0,1,0},{0,0,1,0,0,1,0,0},{0,0,-1,0,0,1,0,0}};
    float push[12]={lamp[0],lamp[1],lamp[2],lamp[3]};std::memcpy(push+4,faces[face],sizeof(faces[face]));vkCmdPushConstants(cmd,shadowLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),push);
    int axis=face/2;float sign=face%2?-1.f:1.f;
    for(auto&entry:staticCaches){auto&cache=entry.second;if(!cache.buffer.handle||!cache.shadowIndices.handle)continue;
     VkDeviceSize offset=0;vkCmdBindVertexBuffers(cmd,0,1,&cache.buffer.handle,&offset);vkCmdBindIndexBuffer(cmd,cache.shadowIndices.handle,0,VK_INDEX_TYPE_UINT32);
     uint64_t bound=~0ull;
     for(const auto&run:cache.shadowRuns){
      float d2=0;for(int k=0;k<3;++k){float c=std::clamp(lamp[size_t(k)],run.lo[k],run.hi[k])-lamp[size_t(k)];d2+=c*c;}
      if(d2>=range2)continue;
      // Skip runs wholly behind this face.
      if((sign>0?run.hi[axis]-lamp[size_t(axis)]:lamp[size_t(axis)]-run.lo[axis])<.02f)continue;
      if(run.material!=bound){auto found=materials.find(run.material);if(found==materials.end())continue;vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowLayout,0,1,&found->second.set,0,nullptr);bound=run.material;}
      vkCmdDrawIndexed(cmd,run.count,1,run.first,0,0);
     }
    }
    vkCmdEndRenderPass(cmd);++shadowFacesDrawn;
   }
  }
 }
 std::unordered_map<int,StaticCache> staticCaches;int captureSlot=-1;bool capturing=false,renderingViewmodel=false;std::array<float,32> viewState{};std::uint64_t cacheHits=0;
 std::vector<Vertex> vertices;std::vector<Batch> batches;
 std::vector<ActorTriangle> actorVertices;
 std::array<Buffer,FrameCount> actorBuffers{};
 std::vector<ActorLighting> actorLights;
 std::uint64_t actorTriangles=0;
 struct ActorEmitBatch {uint64_t material;uint32_t sortKey,start,count;};
 struct ActorEmitCache {uint64_t identity=0;std::vector<ActorTriangle> triangles;std::vector<ActorEmitBatch> batches;};
 std::unordered_map<uint64_t,ActorEmitCache> actorEmitCaches;bool actorEmitCaching=true;std::uint64_t actorEmitHits=0;
 std::vector<Vertex> sortedVertices;std::vector<Batch> sortedBatches;
 int width=0,height=0;VkFormat sceneFormat=VK_FORMAT_B8G8R8A8_UNORM;VkSampleCountFlagBits sceneSamples=VK_SAMPLE_COUNT_1_BIT;bool hdrScene=false;std::string name;
 ~Impl(){
  if(device){vkDeviceWaitIdle(device);if(vrMode&&VrRuntime::active())VrRuntime::active()->shutdownGraphics();if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
   for(auto f:swapFrames)vkDestroyFramebuffer(device,f,nullptr);for(auto v:swapViews)vkDestroyImageView(device,v,nullptr);for(auto&i:vrOutputs){i.view=VK_NULL_HANDLE;destroy(i);}if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);
   for(auto f:mirrorFrames)vkDestroyFramebuffer(device,f,nullptr);for(auto v:mirrorViews)vkDestroyImageView(device,v,nullptr);if(mirrorPipeline)vkDestroyPipeline(device,mirrorPipeline,nullptr);if(mirrorPass)vkDestroyRenderPass(device,mirrorPass,nullptr);
   for(auto&b:projectionBuffers)destroy(b);for(auto&b:actorBuffers)destroy(b);if(projectionPool)vkDestroyDescriptorPool(device,projectionPool,nullptr);if(projectionLayout)vkDestroyDescriptorSetLayout(device,projectionLayout,nullptr);
   destroyStage(occlusionStage);destroyStage(shadowStage);destroyStage(composeStage);
   destroy(target);destroy(sceneColor);destroy(depth);destroy(blackEmission);destroy(overlay);destroy(readback);destroy(dynamicUpload);destroy(atlasStaging);
   for(int i=0;i<FrameCount;++i){destroy(vertexBuffers[i]);destroy(actorVertexBuffers[i]);destroy(overlayBuffers[i]);}
   for(auto&entry:staticCaches){destroy(entry.second.buffer);destroy(entry.second.shadowIndices);}
   destroyShadowAtlas();for(auto&b:lampBuffers)destroy(b);
   if(shadowPipeline)vkDestroyPipeline(device,shadowPipeline,nullptr);if(shadowLayout)vkDestroyPipelineLayout(device,shadowLayout,nullptr);if(shadowPass)vkDestroyRenderPass(device,shadowPass,nullptr);if(shadowSampler)vkDestroySampler(device,shadowSampler,nullptr);
   for(auto&slot:retiredBuffers)for(auto&buffer:slot)destroy(buffer);
   for(auto&entry:materials){destroy(entry.second.color);destroy(entry.second.normal);destroy(entry.second.emission);destroy(entry.second.relief);}
   if(transparent)vkDestroyPipeline(device,transparent,nullptr);if(opaque)vkDestroyPipeline(device,opaque,nullptr);if(additive)vkDestroyPipeline(device,additive,nullptr);
   if(actorTransparent)vkDestroyPipeline(device,actorTransparent,nullptr);if(actorOpaque)vkDestroyPipeline(device,actorOpaque,nullptr);if(actorAdditive)vkDestroyPipeline(device,actorAdditive,nullptr);
   if(compositePipeline)vkDestroyPipeline(device,compositePipeline,nullptr);if(compositePass)vkDestroyRenderPass(device,compositePass,nullptr);if(compositeLayout)vkDestroyPipelineLayout(device,compositeLayout,nullptr);if(compositeSetLayout)vkDestroyDescriptorSetLayout(device,compositeSetLayout,nullptr);if(compositeDescriptors)vkDestroyDescriptorPool(device,compositeDescriptors,nullptr);if(renderPass)vkDestroyRenderPass(device,renderPass,nullptr);if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
   if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);if(setLayout)vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
   if(wrap)vkDestroySampler(device,wrap,nullptr);if(clamp)vkDestroySampler(device,clamp,nullptr);if(sceneSampler)vkDestroySampler(device,sceneSampler,nullptr);if(pointSampler)vkDestroySampler(device,pointSampler,nullptr);
   for(auto sem:readySemaphores)vkDestroySemaphore(device,sem,nullptr);
   for(int i=0;i<FrameCount;++i){if(acquiredSems[i])vkDestroySemaphore(device,acquiredSems[i],nullptr);if(fences[i])vkDestroyFence(device,fences[i],nullptr);}
   if(uploadFence)vkDestroyFence(device,uploadFence,nullptr);
   if(pool)vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);
  }if(surface&&instance)vkDestroySurfaceKHR(instance,surface,nullptr);if(instance)vkDestroyInstance(instance,nullptr);
 }
 void destroy(Buffer&b){if(b.mapped)vkUnmapMemory(device,b.memory);if(b.handle)vkDestroyBuffer(device,b.handle,nullptr);if(b.memory)vkFreeMemory(device,b.memory,nullptr);b={};}
 // Retire geometry with this frame slot, never while the previous frame reads it.
 void retire(Buffer&b){if(b.handle){retiredBuffers[frameIndex].push_back(b);b={};}}
 void destroy(Image&i){if(i.view)vkDestroyImageView(device,i.view,nullptr);if(i.handle)vkDestroyImage(device,i.handle,nullptr);if(i.memory)vkFreeMemory(device,i.memory,nullptr);i={};}
 uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags flags,VkMemoryPropertyFlags preferred=0){VkPhysicalDeviceMemoryProperties props;vkGetPhysicalDeviceMemoryProperties(physical,&props);if(preferred)for(uint32_t i=0;i<props.memoryTypeCount;++i)if((bits&(1u<<i))&&(props.memoryTypes[i].propertyFlags&(flags|preferred))==(flags|preferred))return i;for(uint32_t i=0;i<props.memoryTypeCount;++i)if((bits&(1u<<i))&&(props.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("No compatible Vulkan memory type");}
 void makeBuffer(Buffer&b,VkDeviceSize size,VkBufferUsageFlags usage){
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;check(vkCreateBuffer(device,&info,nullptr,&b.handle),"Create buffer");
  VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,b.handle,&req);VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,(usage&VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)?VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT:(usage&VK_BUFFER_USAGE_TRANSFER_DST_BIT)?VK_MEMORY_PROPERTY_HOST_CACHED_BIT:0); // CPU-read buffers want cached memory
  check(vkAllocateMemory(device,&alloc,nullptr,&b.memory),"Allocate mapped buffer");check(vkBindBufferMemory(device,b.handle,b.memory,0),"Bind buffer");check(vkMapMemory(device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped),"Map buffer");b.size=size;
 }
 void makeImage(Image&i,int w,int h,int levels,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect,VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT){
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={uint32_t(w),uint32_t(h),1};info.mipLevels=levels;info.arrayLayers=1;info.samples=samples;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateImage(device,&info,nullptr,&i.handle),"Create image");VkMemoryRequirements req;vkGetImageMemoryRequirements(device,i.handle,&req);VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  check(vkAllocateMemory(device,&alloc,nullptr,&i.memory),"Allocate image");check(vkBindImageMemory(device,i.handle,i.memory,0),"Bind image");
  VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=i.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;view.subresourceRange={aspect,0,uint32_t(levels),0,1};check(vkCreateImageView(device,&view,nullptr,&i.view),"Create image view");
 }
 // ── Per-frame command recording ─────────────────────────────────────────────
 void startCommands(){
  auto cmd=commands[frameIndex];
  check(vkResetCommandBuffer(cmd,0),"Reset command buffer");
  VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(cmd,&info),"Begin commands");
 }
 // Submit current frame async — no wait. GPU runs in parallel with next frame's CPU work.
 void submitCommandsAsync(VkSemaphore waitSem=VK_NULL_HANDLE,VkSemaphore signalSem=VK_NULL_HANDLE){
  auto cmd=commands[frameIndex];auto f=fences[frameIndex];
  check(vkEndCommandBuffer(cmd),"End commands");
  check(vkResetFences(device,1,&f),"Reset fence");
  VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&cmd;
  VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  if(waitSem!=VK_NULL_HANDLE){info.waitSemaphoreCount=1;info.pWaitSemaphores=&waitSem;info.pWaitDstStageMask=&waitStage;}
  if(signalSem!=VK_NULL_HANDLE){info.signalSemaphoreCount=1;info.pSignalSemaphores=&signalSem;}
  check(vkQueueSubmit(queue,1,&info,f),"Submit graphics work");
  frameInFlight[frameIndex]=true;
 }
 // ── One-shot blocking upload (texture streaming only) ────────────────────
 void startUploadCommands(){
  check(vkResetCommandBuffer(uploadCommand,0),"Reset upload command buffer");
  VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(uploadCommand,&info),"Begin upload commands");
 }
 void submitUploadCommands(){
  check(vkEndCommandBuffer(uploadCommand),"End upload commands");
  check(vkResetFences(device,1,&uploadFence),"Reset upload fence");
  VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&uploadCommand;
  check(vkQueueSubmit(queue,1,&info,uploadFence),"Submit upload work");
  check(vkWaitForFences(device,1,&uploadFence,VK_TRUE,5000000000ull),"Wait for upload");
 }
 // ── Barrier helper — works on whichever command buffer is active ──────────
 void barrier(Image&i,VkImageLayout from,VkImageLayout to,VkAccessFlags source,VkAccessFlags dest,VkPipelineStageFlags sourceStage,VkPipelineStageFlags destStage,uint32_t levels=1,VkCommandBuffer cmd=VK_NULL_HANDLE){
  if(cmd==VK_NULL_HANDLE)cmd=commands[frameIndex];
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=source;b.dstAccessMask=dest;b.oldLayout=from;b.newLayout=to;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=i.handle;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,1};
  vkCmdPipelineBarrier(cmd,sourceStage,destStage,0,0,nullptr,0,nullptr,1,&b);
 }
 // Atlas payloads are owned until this synchronous flush completes. Material
 // entries stay stable across unordered_map rehashes, so their image pointers
 // remain valid while subsequent materials join the same transfer batch.
 void flushUploads(){
  if(pendingUploads.empty())return;
  if(atlasStaging.size<pendingUploadBytes){destroy(atlasStaging);makeBuffer(atlasStaging,std::max(size_t(65536),std::bit_ceil(pendingUploadBytes)),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);++uploadStats[2];}
  for(const auto&upload:pendingUploads)for(size_t level=0;level<upload.levels.size();++level)std::memcpy(static_cast<char*>(atlasStaging.mapped)+upload.copies[level].bufferOffset,upload.levels[level].data(),upload.levels[level].size());
  startUploadCommands();
  std::vector<VkImageMemoryBarrier> transitions;transitions.reserve(pendingUploads.size());
  for(const auto&upload:pendingUploads){VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=upload.image->handle;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,uint32_t(upload.levels.size()),0,1};transitions.push_back(b);}
  vkCmdPipelineBarrier(uploadCommand,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,uint32_t(transitions.size()),transitions.data());
  for(const auto&upload:pendingUploads)vkCmdCopyBufferToImage(uploadCommand,atlasStaging.handle,upload.image->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,uint32_t(upload.copies.size()),upload.copies.data());
  for(auto&b:transitions){b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;}
  vkCmdPipelineBarrier(uploadCommand,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,uint32_t(transitions.size()),transitions.data());
  submitUploadCommands();++uploadStats[1];pendingUploads.clear();pendingUploadBytes=0;
 }
 void upload(Image&i,int w,int h,VkFormat format,std::vector<std::vector<uint8_t>> levels,int pixelSize){
  if(w<1||h<1||levels.empty()||levels.size()>std::bit_width(unsigned(std::max(w,h))))throw std::runtime_error("Invalid atlas upload dimensions");
  size_t alignment=size_t(std::max(4,pixelSize));auto padded=[&](size_t offset){return (offset+alignment-1)&~(alignment-1);};
  auto extent=[&](size_t level){return VkExtent3D{uint32_t(std::max(1,w>>int(level))),uint32_t(std::max(1,h>>int(level))),1};};
  size_t end=pendingUploadBytes;for(size_t level=0;level<levels.size();++level){auto size=extent(level);if(levels[level].size()!=size_t(size.width)*size.height*pixelSize)throw std::runtime_error("Invalid atlas mip payload");end=padded(end)+levels[level].size();}
  if(!pendingUploads.empty()&&end>UploadBatchLimit)flushUploads();
  makeImage(i,w,h,int(levels.size()),format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);PendingUpload pending{&i,std::move(levels),{}};
  for(size_t level=0;level<pending.levels.size();++level){pendingUploadBytes=padded(pendingUploadBytes);VkBufferImageCopy copy{};copy.bufferOffset=pendingUploadBytes;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,uint32_t(level),0,1};copy.imageExtent=extent(level);pending.copies.push_back(copy);pendingUploadBytes+=pending.levels[level].size();uploadStats[3]+=pending.levels[level].size();}
  pendingUploads.push_back(std::move(pending));++uploadStats[0];if(pendingUploadBytes>=UploadBatchLimit)flushUploads();
 }
 uint64_t material(const SoftwareRenderer::Texture&t){
  int scale=SoftwareRenderer::texturePresentScale();if(scale!=1&&scale!=2&&scale!=4)scale=1;
  if(presentScaleCached&&presentScaleCached!=scale)clearMaterials();
  presentScaleCached=scale;
  // Plain constant colours share one image. Atlas keys use a lifetime token,
  // never an allocator address that can later belong to different maps.
  // Constant keys are odd and generation keys even, so the domains cannot alias.
  // Present scale lives in high bits so store-low/run-high retunes invalidate cleanly.
  uint64_t base=t.pixels.size()==1&&t.normalLevels.empty()&&t.emission.empty()&&t.relief.empty()&&!t.additive&&!t.transparent?(uint64_t(t.pixels[0])<<32)|0xffffffffu:t.generation.value<<1;
  uint64_t key=base^(uint64_t(unsigned(scale))<<56);auto found=materials.find(key);if(found!=materials.end())return key;
  auto&mat=materials[key];mat.additive=t.additive;mat.transparent=t.transparent;
  mat.sortKey=(mat.transparent?2u:mat.additive?1u:0u)<<24|uint32_t(materials.size()&0xffffffu);
  auto bytes=[](const auto&values){std::vector<uint8_t> result(values.size()*sizeof(values[0]));std::memcpy(result.data(),values.data(),result.size());return result;};
  int presentW=std::max(1,t.width*scale),presentH=std::max(1,t.height*scale);
  auto color=upsamplePixels(t.pixels,t.width,t.height,scale);
  auto emission=upsamplePixels(t.emission,t.width,t.height,scale);
  // sRGB formats decode before hardware bilinear/trilinear filtering. The
  // software reference keeps its original atlases and mip representation.
  std::vector<std::vector<uint8_t>> levels;
  if(t.additive){levels.push_back(bytes(color));if(scale==1)for(const auto&level:t.mips)levels.push_back(bytes(level));}
  else levels=diffuseBytes(presentW,presentH,color);
  mat.colorLevels=uint32_t(levels.size());
  upload(mat.color,presentW,presentH,t.additive?VK_FORMAT_B8G8R8A8_UNORM:VK_FORMAT_B8G8R8A8_SRGB,std::move(levels),4);
  // Store raw first moments so hardware interpolation also preserves variance.
  // Generate this GPU chain from the original normals, leaving CPU source
  // levels unchanged and including odd last rows/columns at every reduction.
  if(!t.normalLevels.empty()){auto normals=upsampleNormals(t.normalLevels.front(),t.width,t.height,scale);upload(mat.normal,presentW,presentH,VK_FORMAT_R16G16B16A16_SNORM,normalBytes(presentW,presentH,normals),8);}
  if(!t.emission.empty())upload(mat.emission,presentW,presentH,VK_FORMAT_R16G16B16A16_SFLOAT,emissionBytes(emissionMips(presentW,presentH,emission)),8);
  else if(!blackEmission.view)upload(blackEmission,1,1,VK_FORMAT_R16G16B16A16_SFLOAT,emissionBytes(emissionMips(1,1,{0xff000000u})),8);
  if(!t.relief.empty()){auto relief=upsampleRelief(t.relief,t.width,t.height,scale);upload(mat.relief,presentW,presentH,VK_FORMAT_R8_UNORM,{bytes(relief)},1);}
  VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};alloc.descriptorPool=descriptors;alloc.descriptorSetCount=1;alloc.pSetLayouts=&setLayout;check(vkAllocateDescriptorSets(device,&alloc,&mat.set),"Allocate material descriptors");
  VkDescriptorImageInfo images[]={{t.clampEdges?clamp:wrap,mat.color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{wrap,mat.normal.view?mat.normal.view:mat.color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{t.clampEdges?clamp:wrap,mat.emission.view?mat.emission.view:blackEmission.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{wrap,mat.relief.view?mat.relief.view:mat.color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
  VkWriteDescriptorSet writes[4]{};for(uint32_t n=0;n<4;++n){writes[n].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[n].dstSet=mat.set;writes[n].dstBinding=n;writes[n].descriptorCount=1;writes[n].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[n].pImageInfo=&images[n];}vkUpdateDescriptorSets(device,4,writes,0,nullptr);return key;
 }
 // The desktop spectator uses the existing right-eye HDR target and HUD.
 // This is one fullscreen GPU composite, with no third scene render or readback.
 void initMirror(){
  check(vkDeviceWaitIdle(device),"Wait for spectator resize");
  for(auto f:mirrorFrames)vkDestroyFramebuffer(device,f,nullptr);mirrorFrames.clear();for(auto v:mirrorViews)vkDestroyImageView(device,v,nullptr);mirrorViews.clear();for(auto sem:readySemaphores)vkDestroySemaphore(device,sem,nullptr);readySemaphores.clear();swapImages.clear();if(swapchain){vkDestroySwapchainKHR(device,swapchain,nullptr);swapchain=VK_NULL_HANDLE;}
  VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps),"Query spectator surface");uint32_t n=0;check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,nullptr),"Query spectator formats");std::vector<VkSurfaceFormatKHR> formats(n);check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,formats.data()),"Query spectator formats");auto chosen=formats.front();for(auto f:formats)if(f.format==VK_FORMAT_B8G8R8A8_UNORM){chosen=f;break;}
  RECT rect{};GetClientRect(hwnd,&rect);surfaceClientWidth=rect.right;surfaceClientHeight=rect.bottom;mirrorExtent=caps.currentExtent.width!=UINT32_MAX?caps.currentExtent:VkExtent2D{uint32_t(std::max<LONG>(1,rect.right)),uint32_t(std::max<LONG>(1,rect.bottom))};mirrorExtent.width=std::clamp(mirrorExtent.width,caps.minImageExtent.width,caps.maxImageExtent.width);mirrorExtent.height=std::clamp(mirrorExtent.height,caps.minImageExtent.height,caps.maxImageExtent.height);
  uint32_t imageCount=std::max(caps.minImageCount+1,2u);if(caps.maxImageCount)imageCount=std::min(imageCount,caps.maxImageCount);
  uint32_t count=0;vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&count,nullptr);std::vector<VkPresentModeKHR> modes(count);vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&count,modes.data());VkPresentModeKHR mode=VK_PRESENT_MODE_FIFO_KHR;for(auto value:modes)if(value==VK_PRESENT_MODE_MAILBOX_KHR)mode=value;
  VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.surface=surface;sc.minImageCount=imageCount;sc.imageFormat=chosen.format;sc.imageColorSpace=chosen.colorSpace;sc.imageExtent=mirrorExtent;sc.imageArrayLayers=1;sc.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;sc.preTransform=caps.currentTransform;sc.compositeAlpha=(caps.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)?VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:VkCompositeAlphaFlagBitsKHR(1u<<std::countr_zero(caps.supportedCompositeAlpha));sc.presentMode=mode;sc.clipped=VK_TRUE;check(vkCreateSwapchainKHR(device,&sc,nullptr,&swapchain),"Create spectator swapchain");
  if(!mirrorPass){VkAttachmentDescription color{};color.format=chosen.format;color.samples=VK_SAMPLE_COUNT_1_BIT;color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;color.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;color.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ref;VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&color;rp.subpassCount=1;rp.pSubpasses=&sub;check(vkCreateRenderPass(device,&rp,nullptr,&mirrorPass),"Create spectator render pass");}
  check(vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr),"Get spectator images");swapImages.resize(n);check(vkGetSwapchainImagesKHR(device,swapchain,&n,swapImages.data()),"Get spectator images");
  for(auto image:swapImages){VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=image;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=chosen.format;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView view;check(vkCreateImageView(device,&v,nullptr,&view),"Create spectator view");mirrorViews.push_back(view);VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};f.renderPass=mirrorPass;f.attachmentCount=1;f.pAttachments=&mirrorViews.back();f.width=mirrorExtent.width;f.height=mirrorExtent.height;f.layers=1;VkFramebuffer frame;check(vkCreateFramebuffer(device,&f,nullptr,&frame),"Create spectator framebuffer");mirrorFrames.push_back(frame);VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VkSemaphore ready;check(vkCreateSemaphore(device,&sem,nullptr,&ready),"Create spectator semaphore");readySemaphores.push_back(ready);}
 }
 void initPresentation(){
  if(vrMode&&surface)initMirror();
  VkExtent2D extent{};if(vrMode){auto size=VrRuntime::active()->extent();extent={uint32_t(size.first),uint32_t(size.second)};swapExtent=extent;swapFormat=VK_FORMAT_R8G8B8A8_UNORM;for(auto&i:vrOutputs){makeImage(i,extent.width,extent.height,1,swapFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);swapViews.push_back(i.view);}}else{
  VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps),"Query surface capabilities");uint32_t n=0;check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,nullptr),"Query surface formats");std::vector<VkSurfaceFormatKHR> formats(n);check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,formats.data()),"Query surface formats");
  VkSurfaceFormatKHR chosen=formats.front();for(auto f:formats)if(f.format==VK_FORMAT_B8G8R8A8_UNORM){chosen=f;break;}if(chosen.format!=VK_FORMAT_B8G8R8A8_UNORM&&chosen.format!=VK_FORMAT_R8G8B8A8_UNORM)throw std::runtime_error("Display has no supported 8-bit Vulkan surface format");swapFormat=chosen.format;
  RECT rect{};GetClientRect(hwnd,&rect);surfaceClientWidth=rect.right;surfaceClientHeight=rect.bottom;extent=caps.currentExtent.width!=UINT32_MAX?caps.currentExtent:VkExtent2D{uint32_t(std::max<LONG>(1,rect.right)),uint32_t(std::max<LONG>(1,rect.bottom))};extent.width=std::clamp(extent.width,caps.minImageExtent.width,caps.maxImageExtent.width);extent.height=std::clamp(extent.height,caps.minImageExtent.height,caps.maxImageExtent.height);swapExtent=extent;
  uint32_t imageCount=std::max(caps.minImageCount+1,2u);if(caps.maxImageCount&&imageCount>caps.maxImageCount)imageCount=caps.maxImageCount;
  uint32_t modeCount=0;vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,nullptr);std::vector<VkPresentModeKHR> modes(modeCount);vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,modes.data());
  // Preference order: IMMEDIATE (uncapped) → MAILBOX (low-latency vsync) → FIFO (vsync fallback).
  VkPresentModeKHR mode=VK_PRESENT_MODE_FIFO_KHR;
  // Mailbox renders uncapped without tearing; immediate tore visibly while
  // turning. FIFO (vsync) is the fallback.
  for(auto x:modes)if(x==VK_PRESENT_MODE_MAILBOX_KHR){mode=x;break;}
  VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.surface=surface;sc.minImageCount=imageCount;sc.imageFormat=swapFormat;sc.imageColorSpace=chosen.colorSpace;sc.imageExtent=extent;sc.imageArrayLayers=1;sc.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;sc.preTransform=caps.currentTransform;sc.compositeAlpha=(caps.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)?VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:VkCompositeAlphaFlagBitsKHR(1u<<std::countr_zero(caps.supportedCompositeAlpha));sc.presentMode=mode;sc.clipped=VK_TRUE;check(vkCreateSwapchainKHR(device,&sc,nullptr,&swapchain),"Create Win32 swapchain");
  check(vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr),"Get swapchain images");swapImages.resize(n);check(vkGetSwapchainImagesKHR(device,swapchain,&n,swapImages.data()),"Get swapchain images");
  for(auto image:swapImages){VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=image;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=swapFormat;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView view;check(vkCreateImageView(device,&v,nullptr,&view),"Create swapchain view");swapViews.push_back(view);}
  }
  VkAttachmentDescription color{};color.format=swapFormat;color.samples=VK_SAMPLE_COUNT_1_BIT;color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;color.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;color.finalLayout=vrMode?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ref;VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&color;rp.subpassCount=1;rp.pSubpasses=&sub;check(vkCreateRenderPass(device,&rp,nullptr,&compositePass),"Create presentation render pass");
  for(auto view:swapViews){VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};f.renderPass=compositePass;f.attachmentCount=1;f.pAttachments=&view;f.width=extent.width;f.height=extent.height;f.layers=1;VkFramebuffer frame;check(vkCreateFramebuffer(device,&f,nullptr,&frame),"Create presentation framebuffer");swapFrames.push_back(frame);}
  VkDescriptorSetLayoutBinding bindings[2]{};for(uint32_t i=0;i<2;++i){bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;}VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=2;sl.pBindings=bindings;check(vkCreateDescriptorSetLayout(device,&sl,nullptr,&compositeSetLayout),"Create composite descriptor layout");VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT,0,8*sizeof(float)};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&compositeSetLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;check(vkCreatePipelineLayout(device,&pl,nullptr,&compositeLayout),"Create composite pipeline layout");
  VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,uint32_t(2*FrameCount)};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=uint32_t(FrameCount);dp.poolSizeCount=1;dp.pPoolSizes=&ps;check(vkCreateDescriptorPool(device,&dp,nullptr,&compositeDescriptors),"Create composite pool");
  std::array<VkDescriptorSetLayout,FrameCount> layouts;layouts.fill(compositeSetLayout);
  VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=compositeDescriptors;da.descriptorSetCount=uint32_t(FrameCount);da.pSetLayouts=layouts.data();check(vkAllocateDescriptorSets(device,&da,compositeSets.data()),"Allocate composite descriptors");
  VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for(int i=0;i<FrameCount;++i)check(vkCreateSemaphore(device,&sem,nullptr,&acquiredSems[i]),"Create acquire semaphore");
  for(size_t i=0;!vrMode&&i<swapImages.size();++i){VkSemaphore ready;check(vkCreateSemaphore(device,&sem,nullptr,&ready),"Create presentation semaphore");readySemaphores.push_back(ready);}
  VkShaderModule vert=VK_NULL_HANDLE,frag=VK_NULL_HANDLE;try{VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(compositeVertexShader);sm.pCode=compositeVertexShader;check(vkCreateShaderModule(device,&sm,nullptr,&vert),"Create composite vertex shader");sm.codeSize=sizeof(compositeFragmentShader);sm.pCode=compositeFragmentShader;check(vkCreateShaderModule(device,&sm,nullptr,&frag),"Create composite fragment shader");VkPipelineShaderStageCreateInfo stages[2]{};stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr};stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,frag,"main",nullptr};VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=0xf;VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&ba;VkDynamicState dyns[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dyn.dynamicStateCount=2;dyn.pDynamicStates=dyns;VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&input;pi.pInputAssemblyState=&assembly;pi.pViewportState=&viewport;pi.pRasterizationState=&raster;pi.pMultisampleState=&ms;pi.pColorBlendState=&blend;pi.pDynamicState=&dyn;pi.layout=compositeLayout;pi.renderPass=compositePass;check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&compositePipeline),"Create composite pipeline");if(mirrorPass){pi.renderPass=mirrorPass;check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&mirrorPipeline),"Create spectator pipeline");}}catch(...){if(vert)vkDestroyShaderModule(device,vert,nullptr);if(frag)vkDestroyShaderModule(device,frag,nullptr);throw;}vkDestroyShaderModule(device,vert,nullptr);vkDestroyShaderModule(device,frag,nullptr);
 }
 void recreatePresentation(){
  vkDeviceWaitIdle(device);
  for(auto f:swapFrames)vkDestroyFramebuffer(device,f,nullptr);for(auto v:swapViews)vkDestroyImageView(device,v,nullptr);for(auto&i:vrOutputs){i.view=VK_NULL_HANDLE;destroy(i);}if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);
  for(auto sem:readySemaphores)vkDestroySemaphore(device,sem,nullptr);
  for(int i=0;i<FrameCount;++i){if(acquiredSems[i]){vkDestroySemaphore(device,acquiredSems[i],nullptr);acquiredSems[i]=VK_NULL_HANDLE;}}
  if(compositePipeline)vkDestroyPipeline(device,compositePipeline,nullptr);if(compositePass)vkDestroyRenderPass(device,compositePass,nullptr);if(compositeLayout)vkDestroyPipelineLayout(device,compositeLayout,nullptr);if(compositeSetLayout)vkDestroyDescriptorSetLayout(device,compositeSetLayout,nullptr);if(compositeDescriptors)vkDestroyDescriptorPool(device,compositeDescriptors,nullptr);
  swapFrames.clear();swapViews.clear();swapImages.clear();readySemaphores.clear();swapchain=VK_NULL_HANDLE;
  compositeSets={};compositePipeline=VK_NULL_HANDLE;compositePass=VK_NULL_HANDLE;compositeLayout=VK_NULL_HANDLE;compositeSetLayout=VK_NULL_HANDLE;compositeDescriptors=VK_NULL_HANDLE;
  initPresentation();
 }
};
GpuRenderer::GpuRenderer(void* nativeWindow):m(std::make_unique<Impl>()){
 HWND window=static_cast<HWND>(nativeWindow);m->hwnd=window;
 VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="RawMetal";app.apiVersion=VK_API_VERSION_1_0;
 m->vrMode=VrRuntime::active()!=nullptr;std::vector<std::string> extensionNames;if(window){extensionNames={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME};}if(m->vrMode){std::istringstream stream(VrRuntime::active()->instanceExtensions().c_str());for(std::string name;stream>>name;)if(std::find(extensionNames.begin(),extensionNames.end(),name)==extensionNames.end())extensionNames.push_back(name);}std::vector<const char*> extensions;for(auto&name:extensionNames)extensions.push_back(name.c_str());VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};instance.pApplicationInfo=&app;instance.enabledExtensionCount=uint32_t(extensions.size());instance.ppEnabledExtensionNames=extensions.data();check(vkCreateInstance(&instance,nullptr,&m->instance),"Create Vulkan instance");
 if(window){VkWin32SurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};info.hwnd=window;info.hinstance=GetModuleHandleW(nullptr);check(vkCreateWin32SurfaceKHR(m->instance,&info,nullptr,&m->surface),"Create Win32 Vulkan surface");}
 uint32_t count=0;check(vkEnumeratePhysicalDevices(m->instance,&count,nullptr),"Enumerate Vulkan adapters");std::vector<VkPhysicalDevice> adapters(count);check(vkEnumeratePhysicalDevices(m->instance,&count,adapters.data()),"Enumerate Vulkan adapters");int score=-1;
 auto vrDevice=m->vrMode?VrRuntime::active()->outputDevice(m->instance):VK_NULL_HANDLE;
 if(m->vrMode&&!vrDevice)throw std::runtime_error("SteamVR did not select a Vulkan adapter");
 for(auto adapter:adapters){if(m->vrMode&&adapter!=vrDevice)continue;VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(adapter,&props);if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
  uint32_t families=0;vkGetPhysicalDeviceQueueFamilyProperties(adapter,&families,nullptr);std::vector<VkQueueFamilyProperties> queues(families);vkGetPhysicalDeviceQueueFamilyProperties(adapter,&families,queues.data());
  for(uint32_t i=0;i<families;++i)if(queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){VkBool32 present=true;if(m->surface)check(vkGetPhysicalDeviceSurfaceSupportKHR(adapter,i,m->surface,&present),"Check Vulkan presentation support");if(!present)continue;int candidate=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?2:1;if(candidate>score){score=candidate;m->physical=adapter;m->family=i;m->name=props.deviceName;}break;}
 }
 if(!m->physical)throw std::runtime_error("No Vulkan hardware graphics adapter available");
 // Keep scene radiance linear and high range until the display composite.
 for(VkFormat format:{VK_FORMAT_R16G16B16A16_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT}){
  VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties(m->physical,format,&properties);
  constexpr VkFormatFeatureFlags required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  if((properties.optimalTilingFeatures&required)==required){m->sceneFormat=format;m->hdrScene=true;break;}
 }
  VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(m->physical,&properties);VkFormatProperties colorProperties{},depthProperties{};vkGetPhysicalDeviceFormatProperties(m->physical,m->sceneFormat,&colorProperties);vkGetPhysicalDeviceFormatProperties(m->physical,VK_FORMAT_D32_SFLOAT,&depthProperties);VkSampleCountFlags sampleCounts=properties.limits.framebufferColorSampleCounts&properties.limits.framebufferDepthSampleCounts;colorProperties.optimalTilingFeatures&=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;depthProperties.optimalTilingFeatures&=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;if(colorProperties.optimalTilingFeatures&&depthProperties.optimalTilingFeatures){VkImageFormatProperties cp{},dp{};if(vkGetPhysicalDeviceImageFormatProperties(m->physical,m->sceneFormat,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,0,&cp)==VK_SUCCESS&&vkGetPhysicalDeviceImageFormatProperties(m->physical,VK_FORMAT_D32_SFLOAT,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,0,&dp)==VK_SUCCESS)sampleCounts&=cp.sampleCounts&dp.sampleCounts;else sampleCounts=VK_SAMPLE_COUNT_1_BIT;}else sampleCounts=VK_SAMPLE_COUNT_1_BIT;
  for(auto candidate:{VK_SAMPLE_COUNT_4_BIT,VK_SAMPLE_COUNT_2_BIT})if(sampleCounts&candidate){m->sceneSamples=candidate;break;}
  m->name+=m->hdrScene?" / linear HDR":" / LDR";
  if(m->sceneSamples!=VK_SAMPLE_COUNT_1_BIT)m->name+=" / "+std::to_string(int(m->sceneSamples))+"x MSAA";
  VkPhysicalDeviceFeatures supported{};vkGetPhysicalDeviceFeatures(m->physical,&supported);VkPhysicalDeviceFeatures enabled{};enabled.samplerAnisotropy=supported.samplerAnisotropy;
 VkPhysicalDeviceProperties deviceProperties{};vkGetPhysicalDeviceProperties(m->physical,&deviceProperties);
 float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=m->family;queue.queueCount=1;queue.pQueuePriorities=&priority;std::vector<std::string> deviceNames;if(m->surface)deviceNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);if(m->vrMode){std::istringstream stream(VrRuntime::active()->deviceExtensions(m->physical).c_str());for(std::string name;stream>>name;)deviceNames.push_back(name);}std::vector<const char*> deviceExtensions;for(auto&name:deviceNames)deviceExtensions.push_back(name.c_str());VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.queueCreateInfoCount=1;device.pQueueCreateInfos=&queue;device.pEnabledFeatures=&enabled;device.enabledExtensionCount=uint32_t(deviceExtensions.size());device.ppEnabledExtensionNames=deviceExtensions.data();check(vkCreateDevice(m->physical,&device,nullptr,&m->device),"Create Vulkan device");vkGetDeviceQueue(m->device,m->family,0,&m->queue);
 VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool.queueFamilyIndex=m->family;check(vkCreateCommandPool(m->device,&pool,nullptr,&m->pool),"Create graphics command pool");
 // Allocate all per-frame command buffers plus the dedicated upload buffer in one call.
 VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cmdAlloc.commandPool=m->pool;cmdAlloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
 cmdAlloc.commandBufferCount=uint32_t(Impl::FrameCount);check(vkAllocateCommandBuffers(m->device,&cmdAlloc,m->commands.data()),"Allocate frame command buffers");
 cmdAlloc.commandBufferCount=1;check(vkAllocateCommandBuffers(m->device,&cmdAlloc,&m->uploadCommand),"Allocate upload command buffer");
 VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
 for(int i=0;i<Impl::FrameCount;++i)check(vkCreateFence(m->device,&fenceInfo,nullptr,&m->fences[i]),"Create frame fence");
 check(vkCreateFence(m->device,&fenceInfo,nullptr,&m->uploadFence),"Create upload fence");
 VkDescriptorSetLayoutBinding bindings[4]{};for(uint32_t i=0;i<4;++i){bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;}
 VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};set.bindingCount=4;set.pBindings=bindings;check(vkCreateDescriptorSetLayout(m->device,&set,nullptr,&m->setLayout),"Create material layout");VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,8192};VkDescriptorPoolCreateInfo descriptors{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};descriptors.maxSets=2048;descriptors.poolSizeCount=1;descriptors.pPoolSizes=&poolSize;check(vkCreateDescriptorPool(m->device,&descriptors,nullptr,&m->descriptors),"Create descriptor pool");
 VkPushConstantRange viewPush{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,32*sizeof(float)};
 VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
 VkDescriptorSetLayoutBinding projectionBindings[]={
  {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
  {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr},
  {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
  {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
 VkDescriptorSetLayoutCreateInfo projectionInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};projectionInfo.bindingCount=4;projectionInfo.pBindings=projectionBindings;
 check(vkCreateDescriptorSetLayout(m->device,&projectionInfo,nullptr,&m->projectionLayout),"Create view and actor layout");
 VkDescriptorPoolSize projectionSizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2}};
 VkDescriptorPoolCreateInfo projectionPool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};projectionPool.maxSets=2;projectionPool.poolSizeCount=3;projectionPool.pPoolSizes=projectionSizes;
 check(vkCreateDescriptorPool(m->device,&projectionPool,nullptr,&m->projectionPool),"Create view and actor pool");
 VkDescriptorSetLayout projectionLayouts[]={m->projectionLayout,m->projectionLayout};VkDescriptorSetAllocateInfo projectionAllocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};projectionAllocate.descriptorPool=m->projectionPool;projectionAllocate.descriptorSetCount=2;projectionAllocate.pSetLayouts=projectionLayouts;
 check(vkAllocateDescriptorSets(m->device,&projectionAllocate,m->projectionSets.data()),"Allocate view and actor descriptors");
 for(int i=0;i<Impl::FrameCount;++i){
  m->makeBuffer(m->projectionBuffers[i],sizeof(m->vrProjection),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  m->makeBuffer(m->actorBuffers[i],MaxGpuActors*sizeof(ActorLighting),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  VkDescriptorBufferInfo buffers[]={{m->projectionBuffers[i].handle,0,sizeof(m->vrProjection)},{m->actorBuffers[i].handle,0,MaxGpuActors*sizeof(ActorLighting)}};
  VkWriteDescriptorSet writes[2]{};for(int binding=0;binding<2;++binding){auto&write=writes[binding];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;write.dstSet=m->projectionSets[i];write.dstBinding=binding;write.descriptorCount=1;write.descriptorType=binding?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&buffers[binding];}
  vkUpdateDescriptorSets(m->device,2,writes,0,nullptr);
  m->makeBuffer(m->lampBuffers[i],Impl::LampBufferBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);std::memset(m->lampBuffers[i].mapped,0,Impl::LampBufferBytes);
  VkDescriptorBufferInfo lampInfo{m->lampBuffers[i].handle,0,Impl::LampBufferBytes};VkWriteDescriptorSet lampWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};lampWrite.dstSet=m->projectionSets[i];lampWrite.dstBinding=2;lampWrite.descriptorCount=1;lampWrite.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;lampWrite.pBufferInfo=&lampInfo;vkUpdateDescriptorSets(m->device,1,&lampWrite,0,nullptr);
 }
 {
  // Lamp shadow atlas: depth-only faces sampled with hardware comparison.
  VkFormatProperties depthFormat{};vkGetPhysicalDeviceFormatProperties(m->physical,VK_FORMAT_D16_UNORM,&depthFormat);
  VkFilter shadowFilter=(depthFormat.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)?VK_FILTER_LINEAR:VK_FILTER_NEAREST;
  VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=shadowFilter;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler.compareEnable=VK_TRUE;sampler.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL;sampler.maxLod=0.f;
  check(vkCreateSampler(m->device,&sampler,nullptr,&m->shadowSampler),"Create lamp shadow sampler");
  VkAttachmentDescription attachment{};attachment.format=VK_FORMAT_D16_UNORM;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkAttachmentReference reference{0,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.pDepthStencilAttachment=&reference;
  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;dependencies[0].dstSubpass=0;dependencies[0].srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;dependencies[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT;dependencies[0].dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;dependencies[0].dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].srcSubpass=0;dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;dependencies[1].srcStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;dependencies[1].srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;dependencies[1].dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;dependencies[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};passInfo.attachmentCount=1;passInfo.pAttachments=&attachment;passInfo.subpassCount=1;passInfo.pSubpasses=&subpass;passInfo.dependencyCount=2;passInfo.pDependencies=dependencies;
  check(vkCreateRenderPass(m->device,&passInfo,nullptr,&m->shadowPass),"Create lamp shadow pass");
  VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,12*sizeof(float)};VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layoutInfo.setLayoutCount=1;layoutInfo.pSetLayouts=&m->setLayout;layoutInfo.pushConstantRangeCount=1;layoutInfo.pPushConstantRanges=&push;
  check(vkCreatePipelineLayout(m->device,&layoutInfo,nullptr,&m->shadowLayout),"Create lamp shadow layout");
  VkShaderModule shadowVert=VK_NULL_HANDLE,shadowFrag=VK_NULL_HANDLE;
  try{
   VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};module.codeSize=sizeof(lampShadowVertexShader);module.pCode=lampShadowVertexShader;check(vkCreateShaderModule(m->device,&module,nullptr,&shadowVert),"Create lamp shadow vertex shader");
   module.codeSize=sizeof(lampShadowFragmentShader);module.pCode=lampShadowFragmentShader;check(vkCreateShaderModule(m->device,&module,nullptr,&shadowFrag),"Create lamp shadow fragment shader");
   VkPipelineShaderStageCreateInfo stages[2]{};for(auto&stage:stages){stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stage.pName="main";}stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=shadowVert;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=shadowFrag;
   VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription attributes[]={{0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,16}};
   VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;input.vertexAttributeDescriptionCount=2;input.pVertexAttributeDescriptions=attributes;
   VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewportState.viewportCount=viewportState.scissorCount=1;
   VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
   VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
   VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};depthState.depthTestEnable=depthState.depthWriteEnable=VK_TRUE;depthState.depthCompareOp=VK_COMPARE_OP_LESS;
   VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
   VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=states;
   VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&input;pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&viewportState;pipeline.pRasterizationState=&raster;pipeline.pMultisampleState=&multisample;pipeline.pDepthStencilState=&depthState;pipeline.pColorBlendState=&blending;pipeline.pDynamicState=&dynamic;pipeline.layout=m->shadowLayout;pipeline.renderPass=m->shadowPass;
   check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->shadowPipeline),"Create lamp shadow pipeline");
  }catch(...){if(shadowVert)vkDestroyShaderModule(m->device,shadowVert,nullptr);if(shadowFrag)vkDestroyShaderModule(m->device,shadowFrag,nullptr);throw;}
  vkDestroyShaderModule(m->device,shadowVert,nullptr);vkDestroyShaderModule(m->device,shadowFrag,nullptr);
  m->createShadowAtlas(16);
 }
 VkDescriptorSetLayout pipelineSets[]={m->setLayout,m->projectionLayout};layout.setLayoutCount=2;layout.pSetLayouts=pipelineSets;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&viewPush;check(vkCreatePipelineLayout(m->device,&layout,nullptr,&m->pipelineLayout),"Create graphics pipeline layout");
  const bool multisampled=m->sceneSamples!=VK_SAMPLE_COUNT_1_BIT;uint32_t depthIndex=multisampled?2u:1u;VkAttachmentDescription attachments[3]{};attachments[0].format=m->sceneFormat;attachments[0].samples=m->sceneSamples;attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[0].storeOp=multisampled?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE;attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachments[0].finalLayout=multisampled?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  if(multisampled){attachments[1]=attachments[0];attachments[1].samples=VK_SAMPLE_COUNT_1_BIT;attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[1].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;}
 attachments[depthIndex]=attachments[0];attachments[depthIndex].format=VK_FORMAT_D32_SFLOAT;attachments[depthIndex].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[depthIndex].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[depthIndex].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
 VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},resolve{1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth{depthIndex,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;subpass.pResolveAttachments=multisampled?&resolve:nullptr;subpass.pDepthStencilAttachment=&depth;
 VkSubpassDependency deps[2]{};deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;deps[0].srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;deps[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;deps[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;deps[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_SHADER_READ_BIT;
 VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pass.attachmentCount=depthIndex+1;pass.pAttachments=attachments;pass.subpassCount=1;pass.pSubpasses=&subpass;pass.dependencyCount=2;pass.pDependencies=deps;check(vkCreateRenderPass(m->device,&pass,nullptr,&m->renderPass),"Create render pass");
 VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;sampler.anisotropyEnable=enabled.samplerAnisotropy;sampler.maxAnisotropy=enabled.samplerAnisotropy?std::min(8.f,deviceProperties.limits.maxSamplerAnisotropy):1.f;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;sampler.maxLod=VK_LOD_CLAMP_NONE;check(vkCreateSampler(m->device,&sampler,nullptr,&m->wrap),"Create texture sampler");sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;check(vkCreateSampler(m->device,&sampler,nullptr,&m->clamp),"Create decal sampler");
 sampler.anisotropyEnable=VK_FALSE;sampler.maxAnisotropy=1.f;sampler.maxLod=0.f;check(vkCreateSampler(m->device,&sampler,nullptr,&m->sceneSampler),"Create scene presentation sampler");
 sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;check(vkCreateSampler(m->device,&sampler,nullptr,&m->pointSampler),"Create HUD and depth sampler");
 VkShaderModule vert=VK_NULL_HANDLE,frag=VK_NULL_HANDLE,actorVert=VK_NULL_HANDLE;
 try{
  VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof(vertexShader);shader.pCode=vertexShader;check(vkCreateShaderModule(m->device,&shader,nullptr,&vert),"Create vertex shader");shader.codeSize=sizeof(fragmentShader);shader.pCode=fragmentShader;check(vkCreateShaderModule(m->device,&shader,nullptr,&frag),"Create fragment shader");
  VkPipelineShaderStageCreateInfo stages[2]{};for(auto&s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vert;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=frag;
  VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription attrs[]={{0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,16},{2,0,VK_FORMAT_R32G32B32A32_SFLOAT,24},{3,0,VK_FORMAT_R32G32B32A32_SFLOAT,40},{4,0,VK_FORMAT_R32G32B32A32_SFLOAT,56},{5,0,VK_FORMAT_R32G32B32A32_SFLOAT,72},{6,0,VK_FORMAT_R32G32B32A32_SFLOAT,88}};
  VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;input.vertexAttributeDescriptionCount=7;input.pVertexAttributeDescriptions=attrs;VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
  VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};samples.rasterizationSamples=m->sceneSamples;VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};depthState.depthTestEnable=depthState.depthWriteEnable=VK_TRUE;depthState.depthCompareOp=VK_COMPARE_OP_LESS;
  VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=0xf;blend.srcColorBlendFactor=blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;blend.srcAlphaBlendFactor=blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.colorBlendOp=blend.alphaBlendOp=VK_BLEND_OP_ADD;VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blending.attachmentCount=1;blending.pAttachments=&blend;
  VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=states;
  VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&input;pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&viewport;pipeline.pRasterizationState=&raster;pipeline.pMultisampleState=&samples;pipeline.pDepthStencilState=&depthState;pipeline.pColorBlendState=&blending;pipeline.pDynamicState=&dynamic;pipeline.layout=m->pipelineLayout;pipeline.renderPass=m->renderPass;
  check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->opaque),"Create opaque Vulkan pipeline");blend.blendEnable=VK_TRUE;blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;depthState.depthWriteEnable=VK_FALSE;check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->additive),"Create additive Vulkan pipeline");blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->transparent),"Create water blend pipeline");
  shader.codeSize=sizeof(actorVertexShader);shader.pCode=actorVertexShader;check(vkCreateShaderModule(m->device,&shader,nullptr,&actorVert),"Create instanced actor shader");stages[0].module=actorVert;
  binding={0,sizeof(ActorTriangle),VK_VERTEX_INPUT_RATE_INSTANCE};
  VkVertexInputAttributeDescription actorAttrs[]={
   {0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32A32_SFLOAT,16},{2,0,VK_FORMAT_R32G32B32A32_SFLOAT,32},
   {3,0,VK_FORMAT_R32G32_SFLOAT,48},{4,0,VK_FORMAT_R32G32_SFLOAT,56},{5,0,VK_FORMAT_R32G32_SFLOAT,64},
   {6,0,VK_FORMAT_R32G32B32A32_SFLOAT,72},{7,0,VK_FORMAT_R32_SFLOAT,88}};
  input.vertexAttributeDescriptionCount=8;input.pVertexAttributeDescriptions=actorAttrs;
  blend.blendEnable=VK_FALSE;blend.srcColorBlendFactor=blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;depthState.depthWriteEnable=VK_TRUE;
  check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->actorOpaque),"Create opaque actor pipeline");
  blend.blendEnable=VK_TRUE;depthState.depthWriteEnable=VK_FALSE;check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->actorAdditive),"Create additive actor pipeline");
  blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->actorTransparent),"Create transparent actor pipeline");
 }catch(...){if(vert)vkDestroyShaderModule(m->device,vert,nullptr);if(frag)vkDestroyShaderModule(m->device,frag,nullptr);if(actorVert)vkDestroyShaderModule(m->device,actorVert,nullptr);throw;}vkDestroyShaderModule(m->device,vert,nullptr);vkDestroyShaderModule(m->device,frag,nullptr);vkDestroyShaderModule(m->device,actorVert,nullptr);
 // Post stages: fullscreen draws reading the resolved scene (and earlier stages).
 if(m->hdrScene&&!m->vrMode){
  auto buildStage=[&](Impl::FullscreenStage& stage,const uint32_t* code,size_t codeSize,uint32_t samplers,const char* name,bool lampAccess=false){
   stage.samplers=samplers;stage.lampAccess=lampAccess;
   VkAttachmentDescription a{};a.format=m->sceneFormat;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sp{};sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sp.colorAttachmentCount=1;sp.pColorAttachments=&ref;
   VkSubpassDependency pd[2]{};pd[0].srcSubpass=VK_SUBPASS_EXTERNAL;pd[0].dstSubpass=0;pd[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;pd[0].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_SHADER_READ_BIT;pd[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;pd[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT;
   pd[1].srcSubpass=0;pd[1].dstSubpass=VK_SUBPASS_EXTERNAL;pd[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;pd[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;pd[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;pd[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_SHADER_READ_BIT;
   VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&a;rp.subpassCount=1;rp.pSubpasses=&sp;rp.dependencyCount=2;rp.pDependencies=pd;check(vkCreateRenderPass(m->device,&rp,nullptr,&stage.pass),name);
   VkDescriptorSetLayoutBinding pb[5]{};for(uint32_t i=0;i<samplers;++i){pb[i].binding=i;pb[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;pb[i].descriptorCount=1;pb[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;}
   // Lamp access: the lamp grid and the cube shadow atlas follow the images.
   if(lampAccess){pb[samplers]={samplers,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};pb[samplers+1]={samplers+1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};}
   VkDescriptorSetLayoutCreateInfo psl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};psl.bindingCount=samplers+(lampAccess?2u:0u);psl.pBindings=pb;check(vkCreateDescriptorSetLayout(m->device,&psl,nullptr,&stage.setLayout),name);
   VkDescriptorPoolSize ps[2]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,uint32_t(Impl::FrameCount)*(samplers+(lampAccess?1u:0u))},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,uint32_t(Impl::FrameCount)}};VkDescriptorPoolCreateInfo pp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pp.maxSets=uint32_t(Impl::FrameCount);pp.poolSizeCount=lampAccess?2u:1u;pp.pPoolSizes=ps;check(vkCreateDescriptorPool(m->device,&pp,nullptr,&stage.pool),name);
   std::array<VkDescriptorSetLayout,Impl::FrameCount> setLayouts;setLayouts.fill(stage.setLayout);VkDescriptorSetAllocateInfo pa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};pa.descriptorPool=stage.pool;pa.descriptorSetCount=uint32_t(Impl::FrameCount);pa.pSetLayouts=setLayouts.data();check(vkAllocateDescriptorSets(m->device,&pa,stage.sets.data()),name);
   VkPushConstantRange postPush{VK_SHADER_STAGE_FRAGMENT_BIT,0,32*sizeof(float)};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&stage.setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&postPush;check(vkCreatePipelineLayout(m->device,&pl,nullptr,&stage.layout),name);
   VkShaderModule pv=VK_NULL_HANDLE,pf=VK_NULL_HANDLE;
   try{
    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(compositeVertexShader);sm.pCode=compositeVertexShader;check(vkCreateShaderModule(m->device,&sm,nullptr,&pv),name);sm.codeSize=codeSize;sm.pCode=code;check(vkCreateShaderModule(m->device,&sm,nullptr,&pf),name);
    VkPipelineShaderStageCreateInfo st[2]{};for(auto&s:st){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}st[0].stage=VK_SHADER_STAGE_VERTEX_BIT;st[0].module=pv;st[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;st[1].module=pf;
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState cb{};cb.colorWriteMask=0xf;VkPipelineColorBlendStateCreateInfo cbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cbs.attachmentCount=1;cbs.pAttachments=&cb;
    VkDynamicState ds[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dyn.dynamicStateCount=2;dyn.pDynamicStates=ds;
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.stageCount=2;gp.pStages=st;gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pColorBlendState=&cbs;gp.pDynamicState=&dyn;gp.layout=stage.layout;gp.renderPass=stage.pass;
    check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&gp,nullptr,&stage.pipeline),name);
   }catch(...){if(pv)vkDestroyShaderModule(m->device,pv,nullptr);if(pf)vkDestroyShaderModule(m->device,pf,nullptr);throw;}
   vkDestroyShaderModule(m->device,pv,nullptr);vkDestroyShaderModule(m->device,pf,nullptr);
  };
  buildStage(m->occlusionStage,occlusionFragmentShader,sizeof(occlusionFragmentShader),1,"Create occlusion post stage");
  buildStage(m->shadowStage,shadowFragmentShader,sizeof(shadowFragmentShader),1,"Create shadow post stage",true);
  buildStage(m->composeStage,postFragmentShader,sizeof(postFragmentShader),3,"Create compose post stage");
  m->postAvailable=true;
 }
 if(m->surface||m->vrMode)m->initPresentation();
 m->vertices.reserve(150000);m->actorVertices.reserve(40000);m->batches.reserve(4096);
}
GpuRenderer::~GpuRenderer()=default;
const std::string& GpuRenderer::adapter()const{return m->name;}
bool GpuRenderer::hasSurface()const{return m->surface!=VK_NULL_HANDLE;}
std::pair<int,int> GpuRenderer::surfaceExtent()const{
 if(m->vrMode)return VrRuntime::active()->extent();
 if(!m->surface||!m->swapExtent.width||!m->swapExtent.height)return {m->width,m->height};
 RECT client{};GetClientRect(m->hwnd,&client);
 if(client.right<=0||client.bottom<=0)return {int(m->swapExtent.width),int(m->swapExtent.height)};
 if(client.right==m->surfaceClientWidth&&client.bottom==m->surfaceClientHeight)return {int(m->swapExtent.width),int(m->swapExtent.height)};
 VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m->physical,m->surface,&caps),"Query render surface");
 VkExtent2D extent=caps.currentExtent.width!=UINT32_MAX?caps.currentExtent:VkExtent2D{uint32_t(client.right),uint32_t(client.bottom)};
 extent.width=std::clamp(extent.width,caps.minImageExtent.width,caps.maxImageExtent.width);
 extent.height=std::clamp(extent.height,caps.minImageExtent.height,caps.maxImageExtent.height);
 return {int(extent.width),int(extent.height)};
}
void GpuRenderer::captureVrEye(std::vector<std::uint32_t>&pixels){if(!m->vrMode)throw std::runtime_error("Eye capture requires VR");Buffer capture;try{m->flushUploads();m->makeBuffer(capture,VkDeviceSize(m->swapExtent.width)*m->swapExtent.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);m->startUploadCommands();VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={m->swapExtent.width,m->swapExtent.height,1};vkCmdCopyImageToBuffer(m->uploadCommand,m->vrOutputs[m->vrEye].handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,capture.handle,1,&copy);VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(m->uploadCommand,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);m->submitUploadCommands();pixels.resize(size_t(m->swapExtent.width)*m->swapExtent.height);auto source=static_cast<const uint8_t*>(capture.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(uint32_t(source[i*4])<<16)|(uint32_t(source[i*4+1])<<8)|source[i*4+2];m->destroy(capture);}catch(...){m->destroy(capture);throw;}}
void GpuRenderer::captureScene(std::vector<std::uint32_t>&pixels,int&width,int&height){
 if(!m->surface)throw std::runtime_error("Scene capture requires a window");
 vkDeviceWaitIdle(m->device);for(int i=0;i<Impl::FrameCount;++i)m->frameInFlight[i]=false;
 width=m->width;height=m->height;auto&image=m->finalImage();
 size_t bytes=size_t(width)*height*(m->sceneFormat==VK_FORMAT_R16G16B16A16_SFLOAT?8:m->sceneFormat==VK_FORMAT_R32G32B32A32_SFLOAT?16:4);
 if(m->readback.size<bytes){m->destroy(m->readback);m->makeBuffer(m->readback,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);}
 m->startUploadCommands();
 // present() left the image in shader-read layout for the composite.
 m->barrier(image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,1,m->uploadCommand);
 VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(width),uint32_t(height),1};
 vkCmdCopyImageToBuffer(m->uploadCommand,image.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,m->readback.handle,1,&copy);
 VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=m->readback.handle;host.size=VK_WHOLE_SIZE;
 vkCmdPipelineBarrier(m->uploadCommand,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
 m->barrier(image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,m->uploadCommand);
 m->submitUploadCommands();
 pixels.resize(size_t(width)*height);
 auto half=[](uint16_t bits){float value;if((bits&0x7c00u)==0x7c00u)value=(bits&0x03ffu)?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity();else if((bits&0x7c00u)==0)value=std::ldexp(float(bits&0x03ffu),-24);else value=std::ldexp(1.f+float(bits&0x03ffu)/1024.f,int((bits>>10)&31)-15);return (bits&0x8000u)?-value:value;};
 auto display=[](float value){value=std::max(0.f,value)*1.05f;float mapped=std::clamp((value*(2.51f*value+.03f))/(value*(2.43f*value+.59f)+.14f),0.f,1.f);return uint32_t(std::clamp(int(std::lround(std::pow(mapped,1.f/2.2f)*255.f)),0,255));};
 if(m->sceneFormat==VK_FORMAT_R16G16B16A16_SFLOAT){auto*source=static_cast<const uint16_t*>(m->readback.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(display(half(source[i*4]))<<16)|(display(half(source[i*4+1]))<<8)|display(half(source[i*4+2]));}
 else if(m->sceneFormat==VK_FORMAT_R32G32B32A32_SFLOAT){auto*source=static_cast<const float*>(m->readback.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(display(source[i*4])<<16)|(display(source[i*4+1])<<8)|display(source[i*4+2]);}
 else std::memcpy(pixels.data(),m->readback.mapped,pixels.size()*4);
}
void GpuRenderer::updateDynamic(const SoftwareRenderer::Texture&texture){
 auto key=m->material(texture);m->flushUploads();auto&material=m->materials.at(key);auto&image=material.color;
 int scale=SoftwareRenderer::texturePresentScale();if(scale!=1&&scale!=2&&scale!=4)scale=1;
 int presentW=std::max(1,texture.width*scale),presentH=std::max(1,texture.height*scale);
 auto color=upsamplePixels(texture.pixels,texture.width,texture.height,scale);
 std::vector<std::vector<uint8_t>> levels;
 if(texture.additive){auto bytes=[](const auto&pixels){std::vector<uint8_t> result(pixels.size()*4);std::memcpy(result.data(),pixels.data(),result.size());return result;};levels.push_back(bytes(color));if(scale==1)for(const auto&level:texture.mips)levels.push_back(bytes(level));}
 else levels=diffuseBytes(presentW,presentH,color);
 if(levels.size()!=material.colorLevels)throw std::runtime_error("Dynamic atlas topology changed after upload");
 VkDeviceSize size=0;std::vector<VkBufferImageCopy> copies;copies.reserve(levels.size());
 for(size_t level=0;level<levels.size();++level){VkBufferImageCopy copy{};copy.bufferOffset=size;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,uint32_t(level),0,1};copy.imageExtent={uint32_t(std::max(1,presentW>>int(level))),uint32_t(std::max(1,presentH>>int(level))),1};copies.push_back(copy);size+=levels[level].size();}
 if(m->dynamicUpload.size<size){m->destroy(m->dynamicUpload);m->makeBuffer(m->dynamicUpload,size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);}
 for(size_t level=0;level<levels.size();++level)std::memcpy(static_cast<char*>(m->dynamicUpload.mapped)+copies[level].bufferOffset,levels[level].data(),levels[level].size());
 m->startUploadCommands();auto cmd=m->uploadCommand;
 m->barrier(image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,material.colorLevels,cmd);
 vkCmdCopyBufferToImage(cmd,m->dynamicUpload.handle,image.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,uint32_t(copies.size()),copies.data());
 m->barrier(image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,material.colorLevels,cmd);m->submitUploadCommands();
}
void GpuRenderer::prepare(const SoftwareRenderer::Texture&texture){m->material(texture);}
void GpuRenderer::flushUploads(){m->flushUploads();}
std::array<std::uint64_t,4> GpuRenderer::uploadStatistics()const{return m->uploadStats;}
void GpuRenderer::begin(int width,int height){
 // Advance to the next frame slot.
 m->frameIndex=(m->frameIndex+1)%Impl::FrameCount;
 // If this slot was in flight (submitted two frames ago), wait for it to finish
 // before we overwrite its command buffer and vertex buffer.
 if(m->frameInFlight[m->frameIndex]){
  auto waitStart=std::chrono::steady_clock::now();
  check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for frame fence");
  perf::costs[perf::FenceWait]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-waitStart).count();
  m->frameInFlight[m->frameIndex]=false;
 }
 for(auto&buffer:m->retiredBuffers[m->frameIndex])m->destroy(buffer);m->retiredBuffers[m->frameIndex].clear();
 m->vertices.clear();m->actorVertices.clear();m->batches.clear();m->actorLights.clear();m->actorTriangles=0;m->capturing=false;m->captureSlot=-1;m->renderingViewmodel=false;
 m->frameVertexCapacity=m->vertices.capacity();m->frameActorCapacity=m->actorVertices.capacity();
 if(width==m->width&&height==m->height)return;
 // Render target size changed — must drain GPU fully before resize.
 vkDeviceWaitIdle(m->device);for(int i=0;i<Impl::FrameCount;++i)m->frameInFlight[i]=false;
 m->destroyStageTarget(m->occlusionStage);m->destroyStageTarget(m->shadowStage);m->destroyStageTarget(m->composeStage);
 if(m->framebuffer){vkDestroyFramebuffer(m->device,m->framebuffer,nullptr);m->framebuffer=VK_NULL_HANDLE;}m->destroy(m->target);m->destroy(m->sceneColor);m->destroy(m->depth);m->destroy(m->readback);
 m->makeImage(m->target,width,height,1,m->sceneFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);if(m->sceneSamples!=VK_SAMPLE_COUNT_1_BIT)m->makeImage(m->sceneColor,width,height,1,m->sceneFormat,VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,VK_IMAGE_ASPECT_COLOR_BIT,m->sceneSamples);m->makeImage(m->depth,width,height,1,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,m->sceneSamples);m->makeBuffer(m->readback,VkDeviceSize(width)*height*(m->sceneFormat==VK_FORMAT_R16G16B16A16_SFLOAT?8:m->sceneFormat==VK_FORMAT_R32G32B32A32_SFLOAT?16:4),VK_BUFFER_USAGE_TRANSFER_DST_BIT);
 VkImageView views[]={m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?m->target.view:m->sceneColor.view,m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?m->depth.view:m->target.view,m->depth.view};VkFramebufferCreateInfo frame{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};frame.renderPass=m->renderPass;frame.attachmentCount=m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?2u:3u;frame.pAttachments=views;frame.width=width;frame.height=height;frame.layers=1;check(vkCreateFramebuffer(m->device,&frame,nullptr,&m->framebuffer),"Create frame target");m->width=width;m->height=height;
 if(m->postAvailable)for(auto* stage:{&m->occlusionStage,&m->shadowStage,&m->composeStage}){
  m->makeImage(stage->image,width,height,1,m->sceneFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
  VkFramebufferCreateInfo pfb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};pfb.renderPass=stage->pass;pfb.attachmentCount=1;pfb.pAttachments=&stage->image.view;pfb.width=width;pfb.height=height;pfb.layers=1;check(vkCreateFramebuffer(m->device,&pfb,nullptr,&stage->framebuffer),"Create post stage target");
 }
}
void GpuRenderer::setView(float x,float y,float z,float yaw,float pitch,float aspect,bool flashlight,float muzzleFlash,float elapsed){
 float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
 m->viewState={x,y,z,cy,sy,cp,sp,aspect,flashlight?1.f:0.f,muzzleFlash,elapsed,m->hdrScene?1.f:0.f};
}
void GpuRenderer::setPostProcessing(bool enabled,bool occlusionView){m->postEnabled=enabled;m->postDebugOcclusion=occlusionView;}
bool GpuRenderer::postProcessing()const{return m->postAvailable&&m->postEnabled;}
void GpuRenderer::setCrispTexels(bool crisp){m->vrProjection[17]=crisp?1.f:0.f;}
void GpuRenderer::setFogLights(const std::array<float,16>& lights,const std::array<float,4>&powers){std::copy(lights.begin(),lights.end(),m->viewState.begin()+12);std::copy(powers.begin(),powers.end(),m->vrProjection.begin()+20);}
void GpuRenderer::setAtmosphere(const std::array<float,4>& atmosphere){std::copy(atmosphere.begin(),atmosphere.end(),m->viewState.begin()+28);}
bool GpuRenderer::beginStaticCache(int slot,std::uint64_t key){
 auto found=m->staticCaches.find(slot);
 if(found!=m->staticCaches.end()&&found->second.key==key){++m->cacheHits;Impl::Batch marker{};marker.cache=true;marker.cacheSlot=slot;m->batches.push_back(marker);return false;}
 auto&cache=m->staticCaches[slot];if(cache.shadowIndices.handle)m->shadowDirtyBounds.push_back(cache.bounds);m->retire(cache.buffer);m->retire(cache.shadowIndices);cache={};cache.key=key;m->captureSlot=slot;m->capturing=true;return true;
}
void GpuRenderer::endStaticCache(){
 if(!m->capturing||m->captureSlot<0)return;
 int slot=m->captureSlot;auto&cache=m->staticCaches.at(slot);std::vector<Vertex> sorted;std::vector<Impl::Batch> batches;sorted.reserve(cache.vertices.size());
 std::stable_sort(cache.batches.begin(),cache.batches.end(),[](const Impl::Batch&a,const Impl::Batch&b){return a.sortKey<b.sortKey;});
 for(auto batch:cache.batches){auto oldStart=batch.start;batch.start=uint32_t(sorted.size());sorted.insert(sorted.end(),cache.vertices.begin()+oldStart,cache.vertices.begin()+oldStart+batch.count);if(!batches.empty()&&batches.back().material==batch.material)batches.back().count+=batch.count;else batches.push_back(batch);}
 cache.vertices.swap(sorted);cache.batches.swap(batches);
 m->capturing=false;m->captureSlot=-1;
 if(!cache.vertices.empty()){size_t bytes=cache.vertices.size()*sizeof(Vertex);m->makeBuffer(cache.buffer,bytes,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);std::memcpy(cache.buffer.mapped,cache.vertices.data(),bytes);
  // Shadow casters: opaque triangles sorted by 4 m cell, then material, so a
  // lamp face draws only the runs inside its range.
  struct Caster {uint64_t cell,material;uint32_t first;};std::vector<Caster> casters;casters.reserve(cache.vertices.size()/3);
  std::array<float,6> bounds{1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
  for(const auto&batch:cache.batches){const auto&mat=m->materials.at(batch.material);bool casts=!mat.transparent&&!mat.additive;
   for(uint32_t v=batch.start;v+2<batch.start+batch.count;v+=3){const Vertex*t=&cache.vertices[v];float center[3]{};
    for(int c=0;c<3;++c)for(int k=0;k<3;++k){float value=t[c].position[k];bounds[size_t(k)]=std::min(bounds[size_t(k)],value);bounds[size_t(k)+3]=std::max(bounds[size_t(k)+3],value);center[k]+=value/3.f;}
    if(!casts)continue;
    auto cellOf=[](float value){return uint64_t(std::clamp(int(std::floor(value/4.f))+32768,0,65535));};
    casters.push_back({(cellOf(center[0])<<32)|(cellOf(center[1])<<16)|cellOf(center[2]),batch.material,v});
   }
  }
  std::sort(casters.begin(),casters.end(),[](const Caster&a,const Caster&b){return a.cell!=b.cell?a.cell<b.cell:a.material!=b.material?a.material<b.material:a.first<b.first;});
  std::vector<uint32_t> indices;indices.reserve(casters.size()*3);uint64_t runCell=~0ull;
  for(const auto&caster:casters){
   if(cache.shadowRuns.empty()||runCell!=caster.cell||cache.shadowRuns.back().material!=caster.material){cache.shadowRuns.push_back({caster.material,uint32_t(indices.size()),0,{1e30f,1e30f,1e30f},{-1e30f,-1e30f,-1e30f}});runCell=caster.cell;}
   auto&run=cache.shadowRuns.back();
   for(uint32_t c=0;c<3;++c){indices.push_back(caster.first+c);for(int k=0;k<3;++k){float value=cache.vertices[caster.first+c].position[k];run.lo[k]=std::min(run.lo[k],value);run.hi[k]=std::max(run.hi[k],value);}}
   run.count+=3;
  }
  if(!indices.empty()){m->makeBuffer(cache.shadowIndices,indices.size()*4,VK_BUFFER_USAGE_INDEX_BUFFER_BIT);std::memcpy(cache.shadowIndices.mapped,indices.data(),indices.size()*4);}
  cache.bounds=bounds;m->shadowDirtyBounds.push_back(bounds);
 }
 Impl::Batch marker{};marker.cache=true;marker.cacheSlot=slot;m->batches.push_back(marker);
}
void GpuRenderer::setLamps(const std::vector<std::array<float,5>>&input,bool enabled,int detail){
 size_t count=std::min(input.size(),size_t(Impl::MaxLamps));
 uint64_t key=1469598103934665603ull;auto mix=[&](uint32_t value){key^=value;key*=1099511628211ull;};mix(enabled?1u:0u);mix(uint32_t(detail));mix(uint32_t(count));
 for(size_t i=0;i<count;++i)for(float value:input[i])mix(std::bit_cast<uint32_t>(value));
 if(key==m->lampKey)return;
 m->lampKey=key;m->pixelLighting=enabled;m->lamps.assign(input.begin(),input.begin()+std::ptrdiff_t(count));m->lampDirty.assign(count,uint8_t(2));
 if(enabled&&int(count)>m->shadowCapacity)m->createShadowAtlas(int((count+15)/16*16));
 // World-space grid; each cell keeps the 16 lamps that can light it most.
 Impl::LampHeader header{};header.flags[0]=enabled?1:0;header.flags[1]=detail;header.size[3]=int(count);
 std::vector<uint32_t> cells;
 if(count){
  float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
  for(size_t i=0;i<count;++i)for(int k=0;k<3;++k){lo[k]=std::min(lo[k],input[i][size_t(k)]-input[i][3]);hi[k]=std::max(hi[k],input[i][size_t(k)]+input[i][3]);}
  float cell=4.f;int dims[3];
  for(;;){for(int k=0;k<3;++k)dims[k]=std::max(1,int(std::ceil((hi[k]-lo[k])/cell)));if(int64_t(dims[0])*dims[1]*dims[2]<=Impl::MaxLampCells)break;cell*=1.25f;}
  for(int k=0;k<3;++k){header.origin[k]=lo[k];header.size[k]=dims[k];}header.origin[3]=cell;
  std::vector<std::vector<std::pair<float,uint32_t>>> candidates(size_t(dims[0])*dims[1]*dims[2]);
  for(size_t i=0;i<count;++i){const auto&lamp=input[i];float range=lamp[3];int first[3],last[3];
   for(int k=0;k<3;++k){first[k]=std::clamp(int(std::floor((lamp[size_t(k)]-range-lo[k])/cell)),0,dims[k]-1);last[k]=std::clamp(int(std::floor((lamp[size_t(k)]+range-lo[k])/cell)),0,dims[k]-1);}
   for(int z=first[2];z<=last[2];++z)for(int y=first[1];y<=last[1];++y)for(int x=first[0];x<=last[0];++x){
    int c[3]={x,y,z};float d2=0;for(int k=0;k<3;++k){float a=lo[k]+c[k]*cell,b=a+cell,v=std::clamp(lamp[size_t(k)],a,b)-lamp[size_t(k)];d2+=v*v;}
    if(d2>=range*range)continue;float t=1.f-d2/(range*range);
    candidates[size_t((z*dims[1]+y)*dims[0]+x)].push_back({lamp[4]*3.2f*t*t/(1.f+d2*.12f),uint32_t(i+1)});
   }
  }
  cells.assign(candidates.size()*Impl::LampsPerCell,0);
  for(size_t c=0;c<candidates.size();++c){auto&list=candidates[c];
   std::sort(list.begin(),list.end(),[](const auto&a,const auto&b){return a.first>b.first;});
   for(size_t k=0;k<list.size()&&k<size_t(Impl::LampsPerCell);++k)cells[c*Impl::LampsPerCell+k]=list[k].second;
  }
 }
 m->lampData.assign(sizeof(header)+size_t(Impl::MaxLamps)*32+cells.size()*4,0);
 std::memcpy(m->lampData.data(),&header,sizeof(header));
 for(size_t i=0;i<count;++i){float record[8]={input[i][0],input[i][1],input[i][2],input[i][3],input[i][4],enabled&&int(i)<m->shadowCapacity?float(i):-1.f,0,0};std::memcpy(m->lampData.data()+sizeof(header)+i*32,record,32);}
 if(!cells.empty())std::memcpy(m->lampData.data()+sizeof(header)+size_t(Impl::MaxLamps)*32,cells.data(),cells.size()*4);
 ++m->lampGeneration;
}
std::uint64_t GpuRenderer::shadowFacesDrawn()const{return m->shadowFacesDrawn;}
void GpuRenderer::clearStaticCaches(){for(auto&entry:m->staticCaches){if(entry.second.shadowIndices.handle)m->shadowDirtyBounds.push_back(entry.second.bounds);m->retire(entry.second.buffer);m->retire(entry.second.shadowIndices);}m->staticCaches.clear();m->capturing=false;m->captureSlot=-1;}
std::uint64_t GpuRenderer::staticCacheHits()const{return m->cacheHits;}
void GpuRenderer::clearDepth(){m->batches.push_back({0,0,0,true});m->renderingViewmodel=true;}
int GpuRenderer::beginActor(const std::array<Point3,6>& normals,const std::array<float,6>& brightness,const std::array<SoftwareRenderer::NormalLighting,6>& lights,Vec2 origin,Vec2 eye,Point3 center){
 if(m->capturing||m->actorLights.size()>=MaxGpuActors)return -1;
 ActorLighting record{};record.originEye={origin.x,origin.y,eye.x,eye.y};
 for(size_t side=0;side<6;++side){auto n=normals[side];record.normalBrightness[side]={n.x,n.y,n.z,brightness[side]};
  const auto&source=lights[side];auto a=source.directions[0],b=source.directions[1],normal=source.surfaceNormal;
  record.light0[side]={a.x,a.y,a.z,source.weights[0]};record.light1[side]={b.x,b.y,b.z,source.weights[1]};record.surfaceNormal[side]={normal.x,normal.y,normal.z,0};
 }
 // Lighting reads only the XYZ surface normal. Its spare W lanes retain the
 // full-float body center without changing the ordinary scene shader's ABI.
 record.surfaceNormal[0][3]=center.x;record.surfaceNormal[1][3]=center.y;record.surfaceNormal[2][3]=center.z;
 int index=int(m->actorLights.size());m->actorLights.push_back(record);return index;
}
std::array<std::uint64_t,3> GpuRenderer::actorStatistics()const{return {m->actorLights.size(),m->actorTriangles,m->actorEmitHits};}
void GpuRenderer::setActorEmitCaching(bool enabled){m->actorEmitCaching=enabled;if(!enabled){m->actorEmitCaches.clear();m->actorEmitHits=0;}}
void GpuRenderer::clearActorEmitCache(){m->actorEmitCaches.clear();m->actorEmitHits=0;}
bool GpuRenderer::replayActorEmit(std::uint64_t slot,std::uint64_t identity,int actor){
 if(!m->actorEmitCaching||actor<0)return false;
 auto found=m->actorEmitCaches.find(slot);if(found==m->actorEmitCaches.end()||found->second.identity!=identity||found->second.triangles.empty())return false;
 uint32_t base=uint32_t(m->actorVertices.size());
 m->actorVertices.resize(base+found->second.triangles.size());
 std::memcpy(m->actorVertices.data()+base,found->second.triangles.data(),found->second.triangles.size()*sizeof(ActorTriangle));
 for(size_t i=0;i<found->second.triangles.size();++i)m->actorVertices[base+i].actor=float(actor);
 for(const auto&batch:found->second.batches){
  uint32_t start=base+batch.start;
  if(!m->batches.empty()&&!m->batches.back().clear&&!m->batches.back().cache&&m->batches.back().actor&&m->batches.back().material==batch.material)m->batches.back().count+=batch.count;
  else m->batches.push_back({batch.material,start,batch.count,false,false,-1,batch.sortKey,true});
 }
 m->actorTriangles+=found->second.triangles.size();++m->actorEmitHits;return true;
}
void GpuRenderer::commitActorEmit(std::uint64_t slot,std::uint64_t identity,std::uint32_t startTriangle){
 if(!m->actorEmitCaching||startTriangle>m->actorVertices.size())return;
 if(startTriangle==m->actorVertices.size()){m->actorEmitCaches.erase(slot);return;}
 if(m->actorEmitCaches.size()>=128&&!m->actorEmitCaches.count(slot))m->actorEmitCaches.clear();
 auto&entry=m->actorEmitCaches[slot];
 // Moving poses cannot replay their predecessor. Admit only after the same
 // identity repeats; preserve vector capacity instead of allocating/copying
 // a full body on every frame of an animation that never hits this cache.
 if(entry.identity!=identity){entry.identity=identity;entry.triangles.clear();entry.batches.clear();return;}
 entry.batches.clear();
 entry.triangles.assign(m->actorVertices.begin()+startTriangle,m->actorVertices.end());
 for(auto&tri:entry.triangles)tri.actor=0;
 for(const auto&batch:m->batches){
  if(!batch.actor||batch.clear||batch.cache||batch.start+batch.count<=startTriangle)continue;
  uint32_t begin=batch.start<startTriangle?startTriangle:batch.start;
  uint32_t end=batch.start+batch.count;if(end<=begin)continue;
  entry.batches.push_back({batch.material,batch.sortKey,begin-startTriangle,end-begin});
 }
}
GpuRenderer::ActorMaterial GpuRenderer::prepareActorMaterial(const SoftwareRenderer::Texture&texture,float emissionScale){
 uint64_t key;uint32_t sortKey;
 if(texture.pixels.size()>1&&m->lastPixels==texture.pixels.data()&&m->lastGeneration==texture.generation.value){key=m->lastMaterial;sortKey=m->lastSort;}
 else{key=m->material(texture);sortKey=m->materials.at(key).sortKey;m->lastPixels=texture.pixels.data();m->lastGeneration=texture.generation.value;m->lastMaterial=key;m->lastSort=sortKey;}
 return {key,sortKey,texture.emission.empty()?0.f:emissionScale,!texture.normalLevels.empty()?1.f:texture.glossStrength>0?0.f:-1.f,texture.parallaxScale,(texture.transparent?2.f:texture.additive?1.f:0.f)+texture.glossStrength};
}
void GpuRenderer::submitActor(const MeshVertex&a,const MeshVertex&b,const MeshVertex&c,int actor,const ActorMaterial&material){
 uint32_t start=uint32_t(m->actorVertices.size());
 auto&common=m->actorVertices.emplace_back();
 common.settings[0]=material.emission;common.settings[1]=material.normal;common.settings[2]=material.parallax;common.settings[3]=material.surface;common.actor=float(actor);
 const MeshVertex* points[]={&a,&b,&c};
 for(size_t i=0;i<3;++i){const auto&v=*points[i];auto&out=common.position[i];out[0]=v.p.x;out[1]=v.p.y;out[2]=v.p.z;out[3]=v.light;common.uv[i][0]=v.u;common.uv[i][1]=v.v;}
 if(!m->batches.empty()&&!m->batches.back().clear&&!m->batches.back().cache&&m->batches.back().actor&&m->batches.back().material==material.key)++m->batches.back().count;
 else m->batches.push_back({material.key,start,1,false,false,-1,material.sortKey,true});
 ++m->actorTriangles;
}
GpuRenderer::ActorStream GpuRenderer::beginActorStream(size_t maximum){
 uint32_t start=uint32_t(m->actorVertices.size());m->actorVertices.resize(size_t(start)+maximum);
 return {std::span<ActorTriangle>(m->actorVertices.data()+start,maximum),start};
}
void GpuRenderer::actorStreamRun(uint32_t start,uint32_t count,const ActorMaterial&material){
 if(!count)return;
 if(!m->batches.empty()&&!m->batches.back().clear&&!m->batches.back().cache&&m->batches.back().actor&&m->batches.back().material==material.key)m->batches.back().count+=count;
 else m->batches.push_back({material.key,start,count,false,false,-1,material.sortKey,true});
}
void GpuRenderer::endActorStream(const ActorStream&stream,uint32_t count){
 m->actorVertices.resize(size_t(stream.start)+count);m->actorTriangles+=count;
}
void GpuRenderer::submit(MeshVertex a,MeshVertex b,MeshVertex c,const SoftwareRenderer::Texture&texture,float light,const std::array<Point3,2>&directions,const std::array<float,2>&weights,float flatResponse,bool normals,float emissionScale){
 uint64_t key;uint32_t sortKey;
 if(texture.pixels.size()>1&&m->lastPixels==texture.pixels.data()&&m->lastGeneration==texture.generation.value){key=m->lastMaterial;sortKey=m->lastSort;}
 else{key=m->material(texture);sortKey=m->materials.at(key).sortKey;m->lastPixels=texture.pixels.data();m->lastGeneration=texture.generation.value;m->lastMaterial=key;m->lastSort=sortKey;}
 auto&target=m->capturing?m->staticCaches.at(m->captureSlot).vertices:m->vertices;uint32_t start=uint32_t(target.size());
 auto worldPoint=[&](Point3 p){float cy=m->viewState[3],sy=m->viewState[4],cp=m->viewState[5],sp=m->viewState[6];float forward=p.z*cp-p.y*sp;return Point3{m->viewState[0]-p.x*sy+forward*cy,m->viewState[1]+p.x*cy+forward*sy,m->viewState[2]+p.y*cp+p.z*sp};};
 Point3 worldNormal{};if(m->capturing){auto e1=b.p-a.p,e2=c.p-a.p;Point3 n{e1.y*e2.z-e1.z*e2.y,e1.z*e2.x-e1.x*e2.z,e1.x*e2.y-e1.y*e2.x};float nl=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);if(nl>.00001f)n=n*(1.f/nl);worldNormal={-n.x*m->viewState[4]-n.y*m->viewState[6]*m->viewState[3]+n.z*m->viewState[5]*m->viewState[3],n.x*m->viewState[3]-n.y*m->viewState[6]*m->viewState[4]+n.z*m->viewState[5]*m->viewState[4],n.y*m->viewState[5]+n.z*m->viewState[6]};}
 for(auto v:{a,b,c}){Vertex out{};if(m->capturing){auto p=worldPoint(v.p);out.position[0]=p.x;out.position[1]=p.y;out.position[2]=p.z;out.position[3]=1.f;}else{out.position[0]=v.p.x*1.3f;out.position[1]=-v.p.y*1.3f*float(m->width)/m->height;out.position[2]=v.p.z-.06f;out.position[3]=v.p.z;}
  out.uv[0]=v.u;out.uv[1]=v.v;out.lighting[0]=light;out.lighting[1]=v.light;out.lighting[2]=flatResponse;out.lighting[3]=normals?1.f:0.f;
  for(int i=0;i<2;++i){auto*dest=i?out.light1:out.light0;dest[0]=directions[i].x;dest[1]=directions[i].y;dest[2]=directions[i].z;dest[3]=weights[i];}out.surface[0]=v.p.z;out.surface[1]=texture.emission.empty()?0.f:emissionScale;out.surface[2]=(texture.transparent?2.f:texture.additive?1.f:0.f)+texture.glossStrength;out.surface[3]=m->capturing?1.f:m->renderingViewmodel?2.f:0.f;out.worldNormal[0]=worldNormal.x;out.worldNormal[1]=worldNormal.y;out.worldNormal[2]=worldNormal.z;out.worldNormal[3]=texture.parallaxScale;target.push_back(out);
 }
 if(m->capturing){auto&batches=m->staticCaches.at(m->captureSlot).batches;if(!batches.empty()&&!batches.back().clear&&batches.back().material==key)batches.back().count+=3;else batches.push_back({key,start,3,false,false,-1,sortKey});}
 else if(!m->batches.empty()&&!m->batches.back().clear&&!m->batches.back().cache&&!m->batches.back().actor&&m->batches.back().material==key)m->batches.back().count+=3;else m->batches.push_back({key,start,3,false,false,-1,sortKey});
}
void GpuRenderer::finish(std::vector<std::uint32_t>&pixels){
 if(m->lampUploaded[m->frameIndex]!=m->lampGeneration&&!m->lampData.empty()){std::memcpy(m->lampBuffers[m->frameIndex].mapped,m->lampData.data(),std::min(m->lampData.size(),Impl::LampBufferBytes));m->lampUploaded[m->frameIndex]=m->lampGeneration;}
 {const auto&v=m->viewState;float camera[8]={v[0],v[1],v[2],0,v[3],v[4],v[5],v[6]};std::memcpy(static_cast<uint8_t*>(m->lampBuffers[m->frameIndex].mapped)+offsetof(Impl::LampHeader,eye),camera,sizeof(camera));}
 if(!m->actorLights.empty())std::memcpy(m->actorBuffers[m->frameIndex].mapped,m->actorLights.data(),m->actorLights.size()*sizeof(ActorLighting));
 // Complete atlas initialization before any descriptor is used by a draw.
 m->flushUploads();
 // Batch opaque geometry by material, then additive effects, independently for
 // world and view-model depth ranges. Thousands of small modules become tens
 // of hardware draw calls without clipping the visible world.
 auto& vb=m->vertexBuffers[m->frameIndex];
 size_t bytes=m->vertices.size()*sizeof(Vertex);
 if(bytes>vb.size){auto allocationStart=std::chrono::steady_clock::now();m->destroy(vb);m->makeBuffer(vb,std::max(bytes*2,size_t(1048576)),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);++perf::gpuBufferGrowths;perf::costs[perf::GpuAllocation]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-allocationStart).count();}
 Vertex* dest=static_cast<Vertex*>(vb.mapped);uint32_t vertexOffset=0;
 auto& ab=m->actorVertexBuffers[m->frameIndex];size_t actorBytes=m->actorVertices.size()*sizeof(ActorTriangle);
 if(actorBytes>ab.size){auto allocationStart=std::chrono::steady_clock::now();m->destroy(ab);m->makeBuffer(ab,std::max(actorBytes*2,size_t(1048576)),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);++perf::gpuBufferGrowths;perf::costs[perf::GpuAllocation]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-allocationStart).count();}
 if(m->frameVertexCapacity!=m->vertices.capacity()||m->frameActorCapacity!=m->actorVertices.capacity())++perf::cpuGrowthFrames;
 auto* actorDest=static_cast<ActorTriangle*>(ab.mapped);uint32_t actorOffset=0;
 m->sortedBatches.clear();
 for(size_t begin=0;begin<m->batches.size();){
  if(m->batches[begin].clear||m->batches[begin].cache){m->sortedBatches.push_back(m->batches[begin++]);continue;}
  size_t end=begin;while(end<m->batches.size()&&!m->batches[end].clear&&!m->batches[end].cache)++end;
  std::stable_sort(m->batches.begin()+begin,m->batches.begin()+end,[](const auto&a,const auto&b){return a.sortKey<b.sortKey;});
  for(size_t i=begin;i<end;++i){auto batch=m->batches[i];uint32_t start=vertexOffset;
   if(batch.actor){start=actorOffset;if(actorDest)std::memcpy(actorDest+actorOffset,m->actorVertices.data()+batch.start,batch.count*sizeof(ActorTriangle));actorOffset+=batch.count;}
   else{if(dest)std::memcpy(dest+vertexOffset,m->vertices.data()+batch.start,batch.count*sizeof(Vertex));vertexOffset+=batch.count;}
   if(!m->sortedBatches.empty()&&!m->sortedBatches.back().clear&&!m->sortedBatches.back().cache&&m->sortedBatches.back().actor==batch.actor&&m->sortedBatches.back().material==batch.material)m->sortedBatches.back().count+=batch.count;
   else m->sortedBatches.push_back({batch.material,start,batch.count,false,false,-1,batch.sortKey,batch.actor});
  }begin=end;
 }
 m->batches.swap(m->sortedBatches);
 m->startCommands();
 m->renderLampShadows(m->commands[m->frameIndex]);
  VkClearValue clear[3]{};clear[0].color=m->hdrScene?VkClearColorValue{{std::pow(12/255.f,2.2f),std::pow(16/255.f,2.2f),std::pow(18/255.f,2.2f),1000}}:VkClearColorValue{{12/255.f,16/255.f,18/255.f,1}};uint32_t depthIndex=m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?1u:2u;clear[depthIndex].depthStencil={1,0};
 VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=m->renderPass;pass.framebuffer=m->framebuffer;pass.renderArea.extent={uint32_t(m->width),uint32_t(m->height)};pass.clearValueCount=depthIndex+1;pass.pClearValues=clear;
 auto cmd=m->commands[m->frameIndex];
 vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
 VkViewport viewport{0,0,float(m->width),float(m->height),0,1};VkRect2D scissor{{0,0},{uint32_t(m->width),uint32_t(m->height)}};
 vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
 VkDeviceSize offset=0;VkBuffer currentBuffer=VK_NULL_HANDLE;
 if(bytes){currentBuffer=vb.handle;vkCmdBindVertexBuffers(cmd,0,1,&currentBuffer,&offset);}
 std::memcpy(m->projectionBuffers[m->frameIndex].mapped,m->vrProjection.data(),sizeof(m->vrProjection));vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,1,1,&m->projectionSets[m->frameIndex],0,nullptr);
 vkCmdPushConstants(cmd,m->pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(m->viewState),m->viewState.data());
 VkPipeline previous=VK_NULL_HANDLE;
 struct TransparentDraw{VkBuffer buffer;VkDescriptorSet material;uint32_t count,start;bool actor=false;};std::vector<TransparentDraw> transparentDraws;
 auto flushTransparent=[&]{for(const auto& draw:transparentDraws){if(currentBuffer!=draw.buffer){vkCmdBindVertexBuffers(cmd,0,1,&draw.buffer,&offset);currentBuffer=draw.buffer;}auto pipeline=draw.actor?m->actorTransparent:m->transparent;if(previous!=pipeline){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);previous=pipeline;}vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,0,1,&draw.material,0,nullptr);if(draw.actor)vkCmdDraw(cmd,3,draw.count,0,draw.start);else vkCmdDraw(cmd,draw.count,1,draw.start,0);}transparentDraws.clear();};
 for(auto&batch:m->batches){
  if(batch.clear){flushTransparent();VkClearAttachment attachment{};attachment.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;attachment.clearValue.depthStencil={1,0};VkClearRect rect{scissor,0,1};vkCmdClearAttachments(cmd,1,&attachment,1,&rect);continue;}
  if(batch.cache){auto found=m->staticCaches.find(batch.cacheSlot);if(found==m->staticCaches.end()||!found->second.buffer.handle)continue;auto buffer=found->second.buffer.handle;if(currentBuffer!=buffer){vkCmdBindVertexBuffers(cmd,0,1,&buffer,&offset);currentBuffer=buffer;}for(auto&cached:found->second.batches){auto&mat=m->materials.at(cached.material);if(mat.transparent){transparentDraws.push_back({buffer,mat.set,cached.count,cached.start});continue;}auto pipeline=mat.additive?m->additive:m->opaque;if(pipeline!=previous){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);previous=pipeline;}vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,0,1,&mat.set,0,nullptr);vkCmdDraw(cmd,cached.count,1,cached.start,0);}continue;}
  auto buffer=batch.actor?ab.handle:vb.handle;if(currentBuffer!=buffer){vkCmdBindVertexBuffers(cmd,0,1,&buffer,&offset);currentBuffer=buffer;}
  auto&mat=m->materials.at(batch.material);if(mat.transparent){transparentDraws.push_back({buffer,mat.set,batch.count,batch.start,batch.actor});continue;}auto pipeline=batch.actor?(mat.additive?m->actorAdditive:m->actorOpaque):(mat.additive?m->additive:m->opaque);
  if(pipeline!=previous){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);previous=pipeline;}
  vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,0,1,&mat.set,0,nullptr);
  if(batch.actor)vkCmdDraw(cmd,3,batch.count,0,batch.start);else vkCmdDraw(cmd,batch.count,1,batch.start,0);
 }
 flushTransparent();vkCmdEndRenderPass(cmd);
 m->postRan=false;
 if(m->postAvailable&&m->postEnabled&&m->composeStage.framebuffer){
  // Scene RGB is linear radiance and alpha is planar camera depth.
  m->barrier(m->target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);
  const auto& v=m->viewState;auto linear=[](float c){return std::pow(std::max(0.f,c),2.2f);};
  float params[32]={m->postDebugOcclusion?-1.f:1.f,v[7],1.3f,.7f, linear(v[28]),linear(v[29]),linear(v[30]),v[31]*.9f, 1.f/float(m->width),1.f/float(m->height),.09f,1.08f, .004f,.05f,.004f,900.f};
  // The four nearest ceiling lamps (world) become view-space scatter sources.
  for(int i=0;i<4;++i){float dx=v[12+i*4]-v[0],dy=v[13+i*4]-v[1],dz=v[14+i*4]-.1f-v[2];
   float right=-dx*v[4]+dy*v[3],forward=dx*v[3]+dy*v[4];
   params[16+i*4]=right;params[17+i*4]=dz*v[5]-forward*v[6];params[18+i*4]=forward*v[5]+dz*v[6];params[19+i*4]=m->vrProjection[20+size_t(i)];}
  auto runStage=[&](Impl::FullscreenStage& stage,std::initializer_list<VkImageView> sources){
   VkDescriptorImageInfo images[4]{};VkWriteDescriptorSet writes[5]{};uint32_t count=0;
   for(auto view:sources){images[count]={m->sceneSampler,view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};auto&w=writes[count];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=stage.sets[m->frameIndex];w.dstBinding=count;w.descriptorCount=1;w.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w.pImageInfo=&images[count];++count;}
   VkDescriptorBufferInfo lampInfo{m->lampBuffers[m->frameIndex].handle,0,Impl::LampBufferBytes};
   if(stage.lampAccess){
    auto&grid=writes[count];grid.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;grid.dstSet=stage.sets[m->frameIndex];grid.dstBinding=count;grid.descriptorCount=1;grid.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;grid.pBufferInfo=&lampInfo;
    images[count]={m->shadowSampler,m->shadowAtlas.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};auto&atlas=writes[count+1];atlas.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;atlas.dstSet=stage.sets[m->frameIndex];atlas.dstBinding=count+1;atlas.descriptorCount=1;atlas.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;atlas.pImageInfo=&images[count];count+=2;
   }
   vkUpdateDescriptorSets(m->device,count,writes,0,nullptr);
   VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=stage.pass;begin.framebuffer=stage.framebuffer;begin.renderArea.extent={uint32_t(m->width),uint32_t(m->height)};
   vkCmdBeginRenderPass(cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
   vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,stage.pipeline);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,stage.layout,0,1,&stage.sets[m->frameIndex],0,nullptr);
   vkCmdPushConstants(cmd,stage.layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(params),params);vkCmdDraw(cmd,6,1,0,0);vkCmdEndRenderPass(cmd);
  };
  runStage(m->occlusionStage,{m->target.view});
  m->barrier(m->occlusionStage.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);
  runStage(m->shadowStage,{m->target.view});
  m->barrier(m->shadowStage.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);
  runStage(m->composeStage,{m->target.view,m->occlusionStage.image.view,m->shadowStage.image.view});
  m->postRan=true;
 }
 if(m->surface||m->vrMode){
  // Windowed path: leave command buffer open — present() will append the
  // composite pass and submit everything together in one async submit.
  return;
 }
 // Headless screenshot path: copy to readback buffer then wait (blocking is fine here).
 VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(m->width),uint32_t(m->height),1};
 vkCmdCopyImageToBuffer(cmd,m->finalImage().handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,m->readback.handle,1,&copy);
 VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=m->readback.handle;host.size=VK_WHOLE_SIZE;
 vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
 // Blocking submit for headless — we need pixels immediately.
 m->submitCommandsAsync();
 check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for headless frame");
 m->frameInFlight[m->frameIndex]=false;
 pixels.resize(size_t(m->width*m->height));
 auto half=[](uint16_t bits){float value;if((bits&0x7c00u)==0x7c00u)value=(bits&0x03ffu)?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity();else if((bits&0x7c00u)==0)value=std::ldexp(float(bits&0x03ffu),-24);else value=std::ldexp(1.f+float(bits&0x03ffu)/1024.f,int((bits>>10)&31)-15);return (bits&0x8000u)?-value:value;};
 auto display=[](float value){value=std::max(0.f,value)*1.05f;float mapped=std::clamp((value*(2.51f*value+.03f))/(value*(2.43f*value+.59f)+.14f),0.f,1.f);return uint32_t(std::clamp(int(std::lround(std::pow(mapped,1.f/2.2f)*255.f)),0,255));};
 if(m->sceneFormat==VK_FORMAT_R16G16B16A16_SFLOAT){auto*source=static_cast<const uint16_t*>(m->readback.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(display(half(source[i*4]))<<16)|(display(half(source[i*4+1]))<<8)|display(half(source[i*4+2]));}
 else if(m->sceneFormat==VK_FORMAT_R32G32B32A32_SFLOAT){auto*source=static_cast<const float*>(m->readback.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(display(source[i*4])<<16)|(display(source[i*4+1])<<8)|display(source[i*4+2]);}
 else std::memcpy(pixels.data(),m->readback.mapped,pixels.size()*4);
}
void GpuRenderer::present(const std::uint32_t* overlayPixels,int overlayWidth,int overlayHeight,bool underwater,float sceneDim,float damageFlash,float shotKick,float alarm){
 if(!m->surface&&!m->vrMode)return;
 RECT client{};if(m->vrMode){client.right=m->swapExtent.width;client.bottom=m->swapExtent.height;}else GetClientRect(m->hwnd,&client);if(client.right<=0||client.bottom<=0){m->submitCommandsAsync();return;}
 if(!m->vrMode&&(client.right!=m->surfaceClientWidth||client.bottom!=m->surfaceClientHeight))m->recreatePresentation();
 // Resize overlay staging buffer for this frame slot if needed.
 auto& ob=m->overlayBuffers[m->frameIndex];
 if(m->overlayWidth!=overlayWidth||m->overlayHeight!=overlayHeight){
  // Must drain before destroying shared overlay image.
  vkDeviceWaitIdle(m->device);for(int i=0;i<Impl::FrameCount;++i)m->frameInFlight[i]=false;
  m->destroy(m->overlay);for(int i=0;i<Impl::FrameCount;++i)m->destroy(m->overlayBuffers[i]);
  m->makeImage(m->overlay,overlayWidth,overlayHeight,1,VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
  for(int i=0;i<Impl::FrameCount;++i)m->makeBuffer(m->overlayBuffers[i],VkDeviceSize(overlayWidth)*overlayHeight*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  m->overlayWidth=overlayWidth;m->overlayHeight=overlayHeight;m->overlayInitialized=false;
 }
 // Write HUD pixels into this slot's staging buffer (safe — GPU is reading previous slot's).
 std::memcpy(ob.mapped,overlayPixels,size_t(overlayWidth)*overlayHeight*4);
 // Append overlay upload + composite pass to the still-open command buffer from finish().
 auto cmd=m->commands[m->frameIndex];
 VkImageLayout oldOverlay=m->overlayInitialized?(m->vrMode?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL):VK_IMAGE_LAYOUT_UNDEFINED;
 VkAccessFlags srcAccess=m->overlayInitialized?(m->vrMode?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_SHADER_READ_BIT):0;
 VkPipelineStageFlags srcStage=m->overlayInitialized?(m->vrMode?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT):VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
 m->barrier(m->overlay,oldOverlay,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,srcAccess,VK_ACCESS_TRANSFER_WRITE_BIT,srcStage,VK_PIPELINE_STAGE_TRANSFER_BIT,1,cmd);
 VkBufferImageCopy upload{};upload.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};upload.imageExtent={uint32_t(overlayWidth),uint32_t(overlayHeight),1};
 vkCmdCopyBufferToImage(cmd,ob.handle,m->overlay.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&upload);
 m->barrier(m->overlay,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);
 m->overlayInitialized=true;
 m->barrier(m->finalImage(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);
 // Update this frame slot's composite descriptor set.
 VkDescriptorImageInfo images[2]={{m->sceneSampler,m->finalImage().view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{m->pointSampler,m->overlay.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
 VkWriteDescriptorSet writes[2]{};for(uint32_t i=0;i<2;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=m->compositeSets[m->frameIndex];writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[i].pImageInfo=&images[i];}
 vkUpdateDescriptorSets(m->device,2,writes,0,nullptr);
 // Acquire swapchain image — use this slot's semaphore.
 uint32_t imageIndex=m->vrMode?uint32_t(m->vrEye):0;
 VkResult acquire=m->vrMode?VK_SUCCESS:vkAcquireNextImageKHR(m->device,m->swapchain,UINT64_MAX,m->acquiredSems[m->frameIndex],VK_NULL_HANDLE,&imageIndex);
 if(acquire==VK_ERROR_OUT_OF_DATE_KHR){m->submitCommandsAsync();m->recreatePresentation();return;}
 // A suboptimal image is still valid and its acquire semaphore must be
 // consumed. Present it, then rebuild if presentation also requests it.
 if(acquire!=VK_SUBOPTIMAL_KHR)check(acquire,"Acquire swapchain image");
 VkClearValue clear{};clear.color={{0,0,0,1}};
 VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=m->compositePass;pass.framebuffer=m->swapFrames[imageIndex];pass.renderArea.extent=m->swapExtent;pass.clearValueCount=1;pass.pClearValues=&clear;
 vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
 VkViewport viewport{0,0,float(m->swapExtent.width),float(m->swapExtent.height),0,1};VkRect2D scissor{{0,0},m->swapExtent};
 vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
 vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->compositePipeline);
 vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->compositeLayout,0,1,&m->compositeSets[m->frameIndex],0,nullptr);
 float params[8]={underwater?1.f:0.f,sceneDim,damageFlash,shotKick,m->hdrScene?1.f:0.f,m->vrMode?1.f:0.f,alarm,m->postRan?1.f:0.f};
 vkCmdPushConstants(cmd,m->compositeLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(params),params);
 vkCmdDraw(cmd,6,1,0,0);
 vkCmdEndRenderPass(cmd);
 uint32_t mirrorIndex=0;bool mirrorAcquired=false;
 if(m->vrMode&&m->surface&&m->vrEye==1){
  RECT rect{};GetClientRect(m->hwnd,&rect);
  if(rect.right>0&&rect.bottom>0){
   if(rect.right!=m->surfaceClientWidth||rect.bottom!=m->surfaceClientHeight)m->initMirror();
   // Do not stall headset rendering waiting for a desktop refresh.
   VkResult result=vkAcquireNextImageKHR(m->device,m->swapchain,0,m->acquiredSems[m->frameIndex],VK_NULL_HANDLE,&mirrorIndex);
   if(result==VK_SUCCESS||result==VK_SUBOPTIMAL_KHR){mirrorAcquired=true;pass.renderPass=m->mirrorPass;pass.framebuffer=m->mirrorFrames[mirrorIndex];pass.renderArea.extent=m->mirrorExtent;vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
    float scale=std::min(float(m->mirrorExtent.width)/overlayWidth,float(m->mirrorExtent.height)/overlayHeight);if(scale>=1)scale=std::floor(scale);float w=overlayWidth*scale,h=overlayHeight*scale;viewport={float(m->mirrorExtent.width-w)*.5f,float(m->mirrorExtent.height-h)*.5f,w,h,0,1};scissor={{0,0},m->mirrorExtent};vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->mirrorPipeline);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->compositeLayout,0,1,&m->compositeSets[m->frameIndex],0,nullptr);params[5]=VrRuntime::active()->titlePanel()?0.f:1.f;vkCmdPushConstants(cmd,m->compositeLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(params),params);vkCmdDraw(cmd,6,1,0,0);vkCmdEndRenderPass(cmd);
   }else if(result==VK_ERROR_OUT_OF_DATE_KHR)m->initMirror();else if(result!=VK_NOT_READY&&result!=VK_TIMEOUT)check(result,"Acquire spectator image");
  }
 }
 if(m->vrMode){m->barrier(m->overlay,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);m->barrier(m->vrOutputs[m->vrEye],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);m->submitCommandsAsync(mirrorAcquired?m->acquiredSems[m->frameIndex]:VK_NULL_HANDLE,mirrorAcquired?m->readySemaphores[mirrorIndex]:VK_NULL_HANDLE);check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for VR eye");m->frameInFlight[m->frameIndex]=false;if(m->vrEye==1)VrRuntime::active()->submitPanel(m->instance,m->physical,m->device,m->queue,m->family,m->overlay.handle,overlayWidth,overlayHeight);VrRuntime::active()->submit(m->vrEye,m->instance,m->physical,m->device,m->queue,m->family,m->vrOutputs[m->vrEye].handle,m->swapExtent.width,m->swapExtent.height);if(mirrorAcquired){VkPresentInfoKHR spectator{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};spectator.waitSemaphoreCount=1;spectator.pWaitSemaphores=&m->readySemaphores[mirrorIndex];spectator.swapchainCount=1;spectator.pSwapchains=&m->swapchain;spectator.pImageIndices=&mirrorIndex;auto result=vkQueuePresentKHR(m->queue,&spectator);if(result==VK_SUCCESS||result==VK_SUBOPTIMAL_KHR)++m->mirrorPresented;else if(result!=VK_ERROR_OUT_OF_DATE_KHR)check(result,"Present spectator");if(result==VK_SUBOPTIMAL_KHR||result==VK_ERROR_OUT_OF_DATE_KHR)m->initMirror();}return;}
 // Submit: wait on image-acquired semaphore, signal ready semaphore, NO CPU wait.
 m->submitCommandsAsync(m->acquiredSems[m->frameIndex],m->readySemaphores[imageIndex]);
 // Present immediately.
 VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&m->readySemaphores[imageIndex];present.swapchainCount=1;present.pSwapchains=&m->swapchain;present.pImageIndices=&imageIndex;
 VkResult result=vkQueuePresentKHR(m->queue,&present);
 if(result==VK_ERROR_OUT_OF_DATE_KHR||result==VK_SUBOPTIMAL_KHR||acquire==VK_SUBOPTIMAL_KHR){
  // Need to drain before recreating swapchain.
  check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for present fence");
  m->frameInFlight[m->frameIndex]=false;
  m->recreatePresentation();return;
 }
 if(result!=VK_SUCCESS)check(result,"Present swapchain");
 // Do NOT wait here — that's the whole point. The fence for this slot will
 // be waited on at the top of begin() when this slot comes around again.
}
}

namespace retro {std::uint64_t GpuRenderer::mirrorFrames()const{return m->mirrorPresented;}bool GpuRenderer::vrActive()const{return m->vrMode;}void GpuRenderer::setVrEye(int eye,const std::array<float,16>& clip){m->vrEye=eye;std::copy(clip.begin(),clip.end(),m->vrProjection.begin());m->vrProjection[16]=1;}}

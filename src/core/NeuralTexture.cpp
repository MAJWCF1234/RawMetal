#include "NeuralTexture.h"
#include "../ThirdParty/libwebp/src/webp/decode.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <xmmintrin.h>
namespace retro {
namespace {
using Bytes=std::vector<unsigned char>;
std::atomic<uint64_t> completedReconstructions{0};
uint32_t word(const Bytes&b,size_t at){uint32_t v;if(at>b.size()||b.size()-at<4)throw std::runtime_error("Truncated neural texture");std::memcpy(&v,b.data()+at,4);return v;}
struct Shape{uint32_t w,h,lw,lh,model,scale;};
Shape inspect(const Bytes&p){
 if(p.size()<33||std::memcmp(p.data(),"NTX1",4))throw std::runtime_error("Invalid neural texture header");
 Shape s{word(p,4),word(p,8),word(p,12),word(p,16),word(p,20),word(p,24)};
 if(s.w<4||s.h<4||s.w>2048||s.h>2048||s.lw<2||s.lh<2||s.lw>1024||s.lh>1024||(s.scale!=2&&s.scale!=4)||s.lw!=(s.w+s.scale-1)/s.scale||s.lh!=(s.h+s.scale-1)/s.scale||s.model==0||s.model>65535||word(p,28)!=0)throw std::runtime_error("Invalid neural texture dimensions/model");
 return s;
}
struct Layer{uint32_t in,out,k;std::vector<float> values;};
std::array<Layer,3> readModel(const Bytes&m){
 if(m.size()!=2204||std::memcmp(m.data(),"NTM1",4)||word(m,4)!=1||word(m,8)!=8||word(m,12)!=2||word(m,16)!=1084||word(m,20)!=0)throw std::runtime_error("Invalid neural texture model");
 std::array<Layer,3> layers{{{1,8,5,{}},{8,8,3,{}},{8,4,3,{}}}};size_t at=24;
 for(auto&layer:layers){float scale;std::memcpy(&scale,m.data()+at,4);at+=4;if(!std::isfinite(scale)||scale<=0||scale>1)throw std::runtime_error("Invalid neural weight scale");size_t count=size_t(layer.in)*layer.out*layer.k*layer.k+layer.out;layer.values.resize(count);for(auto&value:layer.values){int16_t q;std::memcpy(&q,m.data()+at,2);at+=2;value=q*scale;if(!std::isfinite(value)||std::fabs(value)>16)throw std::runtime_error("Invalid neural weight");}}
 return layers;
}
std::vector<float> convolve(const std::vector<float>&input,uint32_t w,uint32_t h,const Layer&layer,bool relu){
 std::vector<float> result(size_t(w)*h*layer.out);const size_t weights=size_t(layer.in)*layer.out*layer.k*layer.k;const int radius=int(layer.k/2);
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
  __m128 a=_mm_loadu_ps(layer.values.data()+weights),b=layer.out==8?_mm_loadu_ps(layer.values.data()+weights+4):_mm_setzero_ps();size_t at=0;
  for(uint32_t ky=0;ky<layer.k;++ky){uint32_t sy=uint32_t(std::clamp(int(y)+int(ky)-radius,0,int(h)-1));for(uint32_t kx=0;kx<layer.k;++kx){uint32_t sx=uint32_t(std::clamp(int(x)+int(kx)-radius,0,int(w)-1));size_t source=(size_t(sy)*w+sx)*layer.in;for(uint32_t c=0;c<layer.in;++c){auto value=_mm_set1_ps(input[source+c]);a=_mm_add_ps(a,_mm_mul_ps(value,_mm_loadu_ps(layer.values.data()+at)));if(layer.out==8)b=_mm_add_ps(b,_mm_mul_ps(value,_mm_loadu_ps(layer.values.data()+at+4)));at+=layer.out;}}}
  if(relu){a=_mm_max_ps(a,_mm_setzero_ps());b=_mm_max_ps(b,_mm_setzero_ps());}size_t dest=(size_t(y)*w+x)*layer.out;_mm_storeu_ps(result.data()+dest,a);if(layer.out==8)_mm_storeu_ps(result.data()+dest+4,b);
 }
 return result;
}
Bytes enlarge2(const Bytes&rgba,uint32_t w,uint32_t h,const std::array<Layer,3>&layers){
 std::vector<float> input(size_t(w)*h);for(size_t i=0;i<input.size();++i)input[i]=(rgba[i*4]*.2126f+rgba[i*4+1]*.7152f+rgba[i*4+2]*.0722f)/255.f-.5f;
 auto first=convolve(input,w,h,layers[0],true);auto second=convolve(first,w,h,layers[1],true);first.clear();first.shrink_to_fit();auto residual=convolve(second,w,h,layers[2],false);
 uint32_t ow=w*2,oh=h*2;Bytes result(size_t(ow)*oh*4,255);
 for(uint32_t y=0;y<oh;++y){float sy=(y+.5f)*.5f-.5f;int iy=int(std::floor(sy));float fy=sy-iy;uint32_t y0=uint32_t(std::clamp(iy,0,int(h)-1)),y1=uint32_t(std::clamp(iy+1,0,int(h)-1));
  for(uint32_t x=0;x<ow;++x){float sx=(x+.5f)*.5f-.5f;int ix=int(std::floor(sx));float fx=sx-ix;uint32_t x0=uint32_t(std::clamp(ix,0,int(w)-1)),x1=uint32_t(std::clamp(ix+1,0,int(w)-1));float delta=residual[(size_t(y/2)*w+x/2)*4+(y%2)*2+x%2]*255.f;
   for(int c=0;c<3;++c){float top=rgba[(size_t(y0)*w+x0)*4+c]*(1-fx)+rgba[(size_t(y0)*w+x1)*4+c]*fx,bottom=rgba[(size_t(y1)*w+x0)*4+c]*(1-fx)+rgba[(size_t(y1)*w+x1)*4+c]*fx;float value=std::clamp(top*(1-fy)+bottom*fy+delta,0.f,255.f);result[(size_t(y)*ow+x)*4+c]=static_cast<unsigned char>(value+.5f);}
  }
 }
 return result;
}
}
uint32_t neuralTextureModelResource(const Bytes&packet){return inspect(packet).model;}
uint64_t neuralTextureReconstructionCount(){return completedReconstructions.load(std::memory_order_relaxed);}
Bytes reconstructNeuralTexture(const Bytes&packet,const Bytes&model){
 auto s=inspect(packet);auto layers=readModel(model);WebPBitstreamFeatures features{};
 if(WebPGetFeatures(packet.data()+32,packet.size()-32,&features)!=VP8_STATUS_OK||features.format!=2||features.has_animation||features.has_alpha||features.width!=int(s.lw)||features.height!=int(s.lh))throw std::runtime_error("Invalid neural source image");
 Bytes pixels(size_t(s.lw)*s.lh*4);if(!WebPDecodeRGBAInto(packet.data()+32,packet.size()-32,pixels.data(),pixels.size(),s.lw*4))throw std::runtime_error("Cannot decode neural source image");
 for(uint32_t factor=1;factor<s.scale;factor*=2){pixels=enlarge2(pixels,s.lw,s.lh,layers);s.lw*=2;s.lh*=2;}
 // Odd-sized source atlases are edge-padded by the encoder. Crop only the
 // padded right/bottom texels; retain the authored dimensions and UV layout.
 Bytes raw(12+size_t(s.w)*s.h*4);std::memcpy(raw.data(),"RMT1",4);std::memcpy(raw.data()+4,&s.w,4);std::memcpy(raw.data()+8,&s.h,4);
 for(uint32_t y=0;y<s.h;++y)std::memcpy(raw.data()+12+size_t(y)*s.w*4,pixels.data()+size_t(y)*s.lw*4,size_t(s.w)*4);
 completedReconstructions.fetch_add(1,std::memory_order_relaxed);return raw;
}
}

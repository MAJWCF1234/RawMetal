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
struct Shape{uint32_t w,h,lw,lh,model,scale,flags;bool v2;};
// NTX2 flags: the stored low-res colour is lossy VP8 (else lossless VP8L);
// an exact full-size alpha plane follows the colour stream.
constexpr uint32_t LossyColour=1,AlphaPlane=2;
Shape inspect(const Bytes&p){
 if(p.size()<33||(std::memcmp(p.data(),"NTX1",4)&&std::memcmp(p.data(),"NTX2",4)))throw std::runtime_error("Invalid neural texture header");
 Shape s{word(p,4),word(p,8),word(p,12),word(p,16),word(p,20),word(p,24),word(p,28),p[3]=='2'};
 // 4096 lets paid sky panoramas ship as neural packets (x2 from 2048x512).
 if(s.w<4||s.h<4||s.w>4096||s.h>4096||s.lw<2||s.lh<2||s.lw>2048||s.lh>2048||(s.scale!=2&&s.scale!=4)||s.lw!=(s.w+s.scale-1)/s.scale||s.lh!=(s.h+s.scale-1)/s.scale||s.model==0||s.model>65535)throw std::runtime_error("Invalid neural texture dimensions/model");
 if(s.v2?(s.flags&~(LossyColour|AlphaPlane))!=0:s.flags!=0)throw std::runtime_error("Invalid neural texture flags");
 return s;
}
float weightScale(const Bytes&m,size_t at){float scale;std::memcpy(&scale,m.data()+at,4);if(!std::isfinite(scale)||scale<=0||scale>1)throw std::runtime_error("Invalid neural weight scale");return scale;}
void readValues(const Bytes&m,size_t&at,std::vector<float>&values,float scale){for(auto&value:values){int16_t q;std::memcpy(&q,m.data()+at,2);at+=2;value=q*scale;if(!std::isfinite(value)||std::fabs(value)>16)throw std::runtime_error("Invalid neural weight");}}

// ---- NTM1: one 1,084-parameter x2 luma model, run twice for x4 ----------
struct Layer{uint32_t in,out,k;std::vector<float> values;};
std::array<Layer,3> readModel(const Bytes&m){
 if(m.size()!=2204||std::memcmp(m.data(),"NTM1",4)||word(m,4)!=1||word(m,8)!=8||word(m,12)!=2||word(m,16)!=1084||word(m,20)!=0)throw std::runtime_error("Invalid neural texture model");
 std::array<Layer,3> layers{{{1,8,5,{}},{8,8,3,{}},{8,4,3,{}}}};size_t at=24;
 for(auto&layer:layers){float scale=weightScale(m,at);at+=4;layer.values.resize(size_t(layer.in)*layer.out*layer.k*layer.k+layer.out);readValues(m,at,layer.values,scale);}
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
// Adds a pixel-shuffled luma residual (factor x factor phases per low texel,
// in 0..1 units) to the bilinear enlargement of the low-res RGB.
Bytes shuffleOnto(const Bytes&rgba,uint32_t w,uint32_t h,uint32_t f,const std::vector<float>&residual){
 uint32_t ow=w*f,oh=h*f;Bytes result(size_t(ow)*oh*4,255);const float inv=1.f/float(f);
 for(uint32_t y=0;y<oh;++y){float sy=(y+.5f)*inv-.5f;int iy=int(std::floor(sy));float fy=sy-iy;uint32_t y0=uint32_t(std::clamp(iy,0,int(h)-1)),y1=uint32_t(std::clamp(iy+1,0,int(h)-1));
  for(uint32_t x=0;x<ow;++x){float sx=(x+.5f)*inv-.5f;int ix=int(std::floor(sx));float fx=sx-ix;uint32_t x0=uint32_t(std::clamp(ix,0,int(w)-1)),x1=uint32_t(std::clamp(ix+1,0,int(w)-1));float delta=residual[(size_t(y/f)*w+x/f)*f*f+(y%f)*f+x%f]*255.f;
   for(int c=0;c<3;++c){float top=rgba[(size_t(y0)*w+x0)*4+c]*(1-fx)+rgba[(size_t(y0)*w+x1)*4+c]*fx,bottom=rgba[(size_t(y1)*w+x0)*4+c]*(1-fx)+rgba[(size_t(y1)*w+x1)*4+c]*fx;float value=std::clamp(top*(1-fy)+bottom*fy+delta,0.f,255.f);result[(size_t(y)*ow+x)*4+c]=static_cast<unsigned char>(value+.5f);}
  }
 }
 return result;
}
Bytes enlarge2(const Bytes&rgba,uint32_t w,uint32_t h,const std::array<Layer,3>&layers){
 std::vector<float> input(size_t(w)*h);for(size_t i=0;i<input.size();++i)input[i]=(rgba[i*4]*.2126f+rgba[i*4+1]*.7152f+rgba[i*4+2]*.0722f)/255.f-.5f;
 auto first=convolve(input,w,h,layers[0],true);auto second=convolve(first,w,h,layers[1],true);first.clear();first.shrink_to_fit();auto residual=convolve(second,w,h,layers[2],false);
 return shuffleOnto(rgba,w,h,2,residual);
}

// ---- NTM2: per-factor residual heads (3x3 convs, residual blocks) ---------
// "NTM2" u32 version(2) u32 heads; per head u32 factor, inputs(3), channels,
// blocks, parameters, then layers conv_in, (a,b) x blocks, conv_out, each an
// f32 scale and int16 [ky][kx][in][out] weights followed by [out] biases.
struct Conv{uint32_t in,out;std::vector<float> values;};
struct Head{uint32_t factor=0,channels=0;std::vector<Conv> layers;};
Head readHead(const Bytes&m,uint32_t factor){
 if(m.size()<12||std::memcmp(m.data(),"NTM2",4)||word(m,4)!=2||word(m,8)<1||word(m,8)>4)throw std::runtime_error("Invalid neural texture model");
 size_t at=12;Head chosen;
 for(uint32_t i=0,count=word(m,8);i<count;++i){
  uint32_t f=word(m,at),inputs=word(m,at+4),channels=word(m,at+8),blocks=word(m,at+12),parameters=word(m,at+16);at+=20;
  if((f!=2&&f!=4)||inputs!=3||channels<4||channels>32||channels%4||blocks>8)throw std::runtime_error("Invalid neural model head");
  Head head{f,channels,{}};head.layers.push_back({3,channels,{}});for(uint32_t b=0;b<blocks*2;++b)head.layers.push_back({channels,channels,{}});head.layers.push_back({channels,f*f,{}});
  size_t total=0;for(auto&layer:head.layers)total+=size_t(9)*layer.in*layer.out+layer.out;
  if(total!=parameters)throw std::runtime_error("Invalid neural model parameter count");
  for(auto&layer:head.layers){if(m.size()-at<4+2*(size_t(9)*layer.in*layer.out+layer.out)||at>m.size())throw std::runtime_error("Truncated neural model");float scale=weightScale(m,at);at+=4;layer.values.resize(size_t(9)*layer.in*layer.out+layer.out);readValues(m,at,layer.values,scale);}
  if(f==factor){if(chosen.factor)throw std::runtime_error("Duplicate neural model head");chosen=std::move(head);}
 }
 if(at!=m.size())throw std::runtime_error("Invalid neural model length");
 if(!chosen.factor)throw std::runtime_error("Neural model has no head for this factor");
 return chosen;
}
// 3x3 replicate-padded convolution over HWC floats; N vectors of 4 outputs.
template<int N>void conv3(const std::vector<float>&padded,uint32_t w,uint32_t h,uint32_t in,const Conv&layer,float*out,bool relu,const float*add){
 const size_t pw=w+2;const float*bias=layer.values.data()+size_t(9)*in*layer.out;
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
  __m128 acc[N];for(int n=0;n<N;++n)acc[n]=_mm_loadu_ps(bias+4*n);const float*wt=layer.values.data();
  for(uint32_t ky=0;ky<3;++ky){const float*row=padded.data()+((size_t(y)+ky)*pw+x)*in;for(uint32_t t=0;t<3*in;++t){__m128 v=_mm_set1_ps(row[t]);for(int n=0;n<N;++n)acc[n]=_mm_add_ps(acc[n],_mm_mul_ps(v,_mm_loadu_ps(wt+4*n)));wt+=4*N;}}
  float*dest=out+(size_t(y)*w+x)*4*N;
  for(int n=0;n<N;++n){if(relu)acc[n]=_mm_max_ps(acc[n],_mm_setzero_ps());if(add)acc[n]=_mm_add_ps(acc[n],_mm_loadu_ps(add+(size_t(y)*w+x)*4*N+4*n));_mm_storeu_ps(dest+4*n,acc[n]);}
 }
}
void pad(const std::vector<float>&input,uint32_t w,uint32_t h,uint32_t c,std::vector<float>&padded){
 const size_t pw=w+2;padded.resize(pw*(h+2)*c);
 for(uint32_t y=0;y<h+2;++y){uint32_t sy=uint32_t(std::clamp(int(y)-1,0,int(h)-1));for(uint32_t x=0;x<w+2;++x){uint32_t sx=uint32_t(std::clamp(int(x)-1,0,int(w)-1));std::memcpy(padded.data()+(size_t(y)*pw+x)*c,input.data()+(size_t(sy)*w+sx)*c,c*sizeof(float));}}
}
void runConv(const std::vector<float>&input,uint32_t w,uint32_t h,const Conv&layer,std::vector<float>&out,bool relu,const std::vector<float>*add,std::vector<float>&scratch){
 pad(input,w,h,layer.in,scratch);out.resize(size_t(w)*h*layer.out);const float*a=add?add->data():nullptr;
 switch(layer.out/4){case 1:conv3<1>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;case 2:conv3<2>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;case 3:conv3<3>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;case 4:conv3<4>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;
  case 5:conv3<5>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;case 6:conv3<6>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;case 7:conv3<7>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;case 8:conv3<8>(scratch,w,h,layer.in,layer,out.data(),relu,a);break;
  default:throw std::runtime_error("Unsupported neural layer width");}
}
Bytes enlargeHead(const Bytes&rgba,uint32_t w,uint32_t h,const Head&head){
 std::vector<float> input(size_t(w)*h*3),state,inner,residual,scratch;
 for(size_t i=0;i<size_t(w)*h;++i)for(int c=0;c<3;++c)input[i*3+c]=rgba[i*4+c]/255.f-.5f;
 runConv(input,w,h,head.layers[0],state,true,nullptr,scratch);input.clear();input.shrink_to_fit();
 for(size_t b=1;b+1<head.layers.size();b+=2){runConv(state,w,h,head.layers[b],inner,true,nullptr,scratch);std::vector<float> next;runConv(inner,w,h,head.layers[b+1],next,false,&state,scratch);state.swap(next);}
 runConv(state,w,h,head.layers.back(),residual,false,nullptr,scratch);
 return shuffleOnto(rgba,w,h,head.factor,residual);
}
Bytes decodeWebp(const unsigned char*data,size_t size,uint32_t w,uint32_t h,int format){
 WebPBitstreamFeatures features{};
 if(WebPGetFeatures(data,size,&features)!=VP8_STATUS_OK||features.format!=format||features.has_animation||features.has_alpha||features.width!=int(w)||features.height!=int(h))throw std::runtime_error("Invalid neural source image");
 Bytes pixels(size_t(w)*h*4);if(!WebPDecodeRGBAInto(data,size,pixels.data(),pixels.size(),int(w)*4))throw std::runtime_error("Cannot decode neural source image");
 return pixels;
}
}
uint32_t neuralTextureModelResource(const Bytes&packet){return inspect(packet).model;}
uint64_t neuralTextureReconstructionCount(){return completedReconstructions.load(std::memory_order_relaxed);}
Bytes reconstructNeuralTexture(const Bytes&packet,const Bytes&model){
 auto s=inspect(packet);Bytes pixels,alpha;
 if(!s.v2){
  auto layers=readModel(model);pixels=decodeWebp(packet.data()+32,packet.size()-32,s.lw,s.lh,2);
  for(uint32_t factor=1;factor<s.scale;factor*=2){pixels=enlarge2(pixels,s.lw,s.lh,layers);s.lw*=2;s.lh*=2;}
 }else{
  // NTX2 body: u32 colour length, colour WebP; with AlphaPlane, u32 alpha
  // length and a lossless full-size WebP whose green channel is the alpha.
  size_t at=32;uint32_t colour=word(packet,at);at+=4;if(!colour||colour>packet.size()-at)throw std::runtime_error("Truncated neural colour stream");
  auto head=readHead(model,s.scale);
  pixels=decodeWebp(packet.data()+at,colour,s.lw,s.lh,(s.flags&LossyColour)?1:2);at+=colour;
  if(s.flags&AlphaPlane){uint32_t length=word(packet,at);at+=4;if(!length||length>packet.size()-at)throw std::runtime_error("Truncated neural alpha stream");alpha=decodeWebp(packet.data()+at,length,s.w,s.h,2);at+=length;}
  if(at!=packet.size())throw std::runtime_error("Invalid neural texture length");
  pixels=enlargeHead(pixels,s.lw,s.lh,head);s.lw*=s.scale;s.lh*=s.scale;
 }
 // Odd-sized source atlases are edge-padded by the encoder. Crop only the
 // padded right/bottom texels; retain the authored dimensions and UV layout.
 Bytes raw(12+size_t(s.w)*s.h*4);std::memcpy(raw.data(),"RMT1",4);std::memcpy(raw.data()+4,&s.w,4);std::memcpy(raw.data()+8,&s.h,4);
 for(uint32_t y=0;y<s.h;++y)std::memcpy(raw.data()+12+size_t(y)*s.w*4,pixels.data()+size_t(y)*s.lw*4,size_t(s.w)*4);
 if(!alpha.empty())for(size_t i=0;i<size_t(s.w)*s.h;++i)raw[12+i*4+3]=alpha[i*4+1];
 completedReconstructions.fetch_add(1,std::memory_order_relaxed);return raw;
}
}

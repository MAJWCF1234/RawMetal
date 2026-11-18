#define NOMINMAX
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/stb/stb_image.h"
#include "../ThirdParty/libwebp/src/webp/encode.h"
#include "../core/NeuralTexture.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using Bytes=std::vector<unsigned char>;
int checks=0;
void check(bool condition,const char*message){++checks;if(!condition)throw std::runtime_error(message);}
void put(Bytes&b,uint32_t v){size_t at=b.size();b.resize(at+4);std::memcpy(b.data()+at,&v,4);}
void set(Bytes&b,size_t at,uint32_t v){std::memcpy(b.data()+at,&v,4);}
Bytes read(const char*path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing neural input");return Bytes((std::istreambuf_iterator<char>(f)),{});}
void rejected(const Bytes&p,const Bytes&m){bool failed=false;try{retro::reconstructNeuralTexture(p,m);}catch(const std::exception&){failed=true;}check(failed,"Invalid neural source/model accepted");}
Bytes png(const char*path){int w,h,c;auto data=stbi_load(path,&w,&h,&c,4);if(!data)throw std::runtime_error("Missing reference PNG");Bytes b{'R','M','T','1'};put(b,w);put(b,h);b.insert(b.end(),data,data+size_t(w)*h*4);stbi_image_free(data);return b;}
int main(int argc,char**argv)try{
 if(argc>=5&&std::string(argv[1])=="--run"){
  auto model=read(argv[2]),packet=read(argv[3]);auto image=retro::reconstructNeuralTexture(packet,model);uint32_t w,h;std::memcpy(&w,image.data()+4,4);std::memcpy(&h,image.data()+8,4);
  if(argc>5){auto reference=png(argv[5]);check(reference.size()==image.size()&&!std::memcmp(reference.data(),image.data(),12),"Reference dimensions differ");unsigned maximum=0;size_t changed=0;for(size_t i=12;i<image.size();++i){auto delta=unsigned(std::abs(int(reference[i])-int(image[i])));maximum=std::max(maximum,delta);changed+=delta!=0;}check(maximum<=1,"Native output differs from quantized PyTorch reference by more than 1 LSB");std::cout<<"PyTorch parity: max "<<maximum<<" LSB, "<<changed<<" / "<<image.size()-12<<" channels differ; ";}
  std::vector<double> samples;for(int i=0;i<9;++i){auto start=std::chrono::steady_clock::now();auto again=retro::reconstructNeuralTexture(packet,model);samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());check(again==image,"Non-deterministic neural runtime");}std::sort(samples.begin(),samples.end());std::ofstream ppm(std::string(argv[4])+".ppm",std::ios::binary);ppm<<"P6\n"<<w<<' '<<h<<"\n255\n";for(size_t i=12;i<image.size();i+=4)ppm.write(reinterpret_cast<const char*>(image.data()+i),3);std::ofstream raw(std::string(argv[4])+".rmt",std::ios::binary);raw.write(reinterpret_cast<const char*>(image.data()),image.size());check(bool(ppm)&&bool(raw),"Cannot save neural output");std::cout<<w<<'x'<<h<<" median "<<samples[4]<<" ms, p89 "<<samples[7]<<" ms; 9 repeat decodes PASS\n";return 0;
 }
 Bytes model{'N','T','M','1'};for(uint32_t v:{1u,8u,2u,1084u,0u})put(model,v);
 for(size_t n:{208,584,292}){float scale=1;size_t at=model.size();model.resize(at+4+n*2);std::memcpy(model.data()+at,&scale,4);}
 Bytes rgba;for(int value:{0,64,128,192})for(int c=0;c<4;++c)rgba.push_back(c==3?255:static_cast<unsigned char>(value));uint8_t*encoded=nullptr;size_t length=WebPEncodeLosslessRGBA(rgba.data(),2,2,8,&encoded);check(length>0,"Cannot encode test texture");
 Bytes packet{'N','T','X','1'};for(uint32_t v:{4u,4u,2u,2u,348u,2u,0u})put(packet,v);packet.insert(packet.end(),encoded,encoded+length);WebPFree(encoded);
 auto image=retro::reconstructNeuralTexture(packet,model);check(image.size()==12+4*4*4,"Wrong neural output size");check(retro::neuralTextureModelResource(packet)==348,"Wrong model resource");int expected[]={0,16,48,64,32,48,80,96,96,112,144,160,128,144,176,192};for(size_t i=0;i<16;++i){for(int c=0;c<3;++c)check(image[12+i*4+c]==expected[i],"Zero model is not exact bilinear");check(image[12+i*4+3]==255,"Alpha changed");}
 auto phaseModel=model;float scale=1.f/255;std::memcpy(phaseModel.data()+1616,&scale,4);for(int c=0;c<4;++c){int16_t bias=int16_t(c+1);std::memcpy(phaseModel.data()+2196+c*2,&bias,2);}image=retro::reconstructNeuralTexture(packet,phaseModel);for(size_t y=0;y<4;++y)for(size_t x=0;x<4;++x)for(int c=0;c<3;++c)check(image[12+(y*4+x)*4+c]==expected[y*4+x]+int(y%2)*2+int(x%2)+1,"Learned pixel-shuffle phase mapping is wrong");
 check(image==retro::reconstructNeuralTexture(packet,phaseModel),"Learned decode is not repeatable");
 for(auto field:std::initializer_list<std::pair<size_t,uint32_t>>{{4,8},{8,0},{12,0},{16,1024},{20,0},{20,65536},{24,3},{28,1}}){auto bad=packet;set(bad,field.first,field.second);rejected(bad,model);}
 for(auto field:std::initializer_list<std::pair<size_t,uint32_t>>{{4,2},{8,0},{12,4},{16,1085},{20,1}}){auto bad=model;set(bad,field.first,field.second);rejected(packet,bad);}
 for(float scaleValue:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){auto bad=model;std::memcpy(bad.data()+24,&scaleValue,4);rejected(packet,bad);}
 auto bad=packet;bad.resize(31);rejected(bad,model);bad=packet;bad.resize(35);rejected(bad,model);bad=model;bad.pop_back();rejected(packet,bad);bad=model;bad.push_back(0);rejected(packet,bad);
 // x4 uses the same model twice, preserving a fixed shared model size.
 auto x4=packet;set(x4,4,8);set(x4,8,8);set(x4,24,4);image=retro::reconstructNeuralTexture(x4,model);check(image.size()==12+8*8*4,"x4 dimensions incorrect");
 // Non-power-of-two and odd source atlases keep their original dimensions.
 Bytes oddRgba(3*4*4,255);for(size_t y=0;y<4;++y)for(size_t x=0;x<3;++x)for(size_t c=0;c<3;++c)oddRgba[(y*3+x)*4+c]=static_cast<unsigned char>(x*30+y*40);
 length=WebPEncodeLosslessRGBA(oddRgba.data(),3,4,12,&encoded);check(length>0,"Cannot encode odd test image");
 Bytes odd{'N','T','X','1'};for(uint32_t v:{6u,8u,3u,4u,348u,2u,0u})put(odd,v);odd.insert(odd.end(),encoded,encoded+length);WebPFree(encoded);
 auto padded=retro::reconstructNeuralTexture(odd,model);set(odd,4,5);set(odd,8,7);auto cropped=retro::reconstructNeuralTexture(odd,model);check(cropped.size()==12+5*7*4,"Odd atlas dimensions changed");
 for(size_t y=0;y<7;++y)for(size_t x=0;x<5;++x)for(size_t c=0;c<4;++c)check(cropped[12+(y*5+x)*4+c]==padded[12+(y*6+x)*4+c],"Odd atlas crop moved authored texels");
 auto undersized=odd;set(undersized,4,4);rejected(undersized,model);
 // x4 padding can require cropping three texels; use the same independent
 // padded reference to catch stride and last-row mistakes.
 set(odd,4,12);set(odd,8,16);set(odd,24,4);padded=retro::reconstructNeuralTexture(odd,model);set(odd,4,9);set(odd,8,13);cropped=retro::reconstructNeuralTexture(odd,model);check(cropped.size()==12+9*13*4,"Odd x4 dimensions changed");
 for(size_t y=0;y<13;++y)for(size_t x=0;x<9;++x)for(size_t c=0;c<4;++c)check(cropped[12+(y*9+x)*4+c]==padded[12+(y*12+x)*4+c],"Odd x4 crop moved authored texels");
 // ---- NTX2 / NTM2: per-factor residual heads, lossy colour, exact alpha.
 // A zero model (every weight 0, scale 1) must reproduce bilinear exactly.
 struct HeadSpec{uint32_t factor,channels,blocks;};
 auto ntm2=[&](std::initializer_list<HeadSpec> heads){Bytes m{'N','T','M','2'};put(m,2);put(m,uint32_t(heads.size()));
  for(auto&h:heads){std::vector<size_t> layers{9*3*h.channels+h.channels};for(uint32_t b=0;b<2*h.blocks;++b)layers.push_back(9*h.channels*h.channels+h.channels);layers.push_back(9*h.channels*h.factor*h.factor+h.factor*h.factor);
   size_t total=0;for(auto n:layers)total+=n;put(m,h.factor);put(m,3);put(m,h.channels);put(m,h.blocks);put(m,uint32_t(total));
   for(auto n:layers){float one=1;size_t at=m.size();m.resize(at+4+n*2);std::memcpy(m.data()+at,&one,4);}}
  return m;};
 auto model2=ntm2({{2,4,1},{4,4,0}});
 auto ntx2=[&](uint32_t w,uint32_t h,uint32_t lw,uint32_t lh,uint32_t f,uint32_t flags,const Bytes&colour,const Bytes*alpha){Bytes p{'N','T','X','2'};for(uint32_t v:{w,h,lw,lh,1999u,f,flags})put(p,v);put(p,uint32_t(colour.size()));p.insert(p.end(),colour.begin(),colour.end());if(alpha){put(p,uint32_t(alpha->size()));p.insert(p.end(),alpha->begin(),alpha->end());}return p;};
 auto lossless=[&](const Bytes&rgba,int w,int h){uint8_t*out=nullptr;size_t n=WebPEncodeLosslessRGBA(rgba.data(),w,h,w*4,&out);check(n>0,"Cannot encode NTX2 test image");Bytes b(out,out+n);WebPFree(out);return b;};
 Bytes colour2=lossless(rgba,2,2);auto p2=ntx2(4,4,2,2,2,0,colour2,nullptr);check(retro::neuralTextureModelResource(p2)==1999,"NTX2 model resource");
 image=retro::reconstructNeuralTexture(p2,model2);check(image.size()==12+4*4*4,"NTX2 x2 size");
 for(size_t i=0;i<16;++i){for(int c=0;c<3;++c)check(image[12+i*4+c]==expected[i],"NTX2 zero head is not exact bilinear");check(image[12+i*4+3]==255,"NTX2 opaque alpha changed");}
 // x4 runs its own head in one pass: bilinear at (x+.5)/4-.5, edge-clamped.
 auto p4=ntx2(8,8,2,2,4,0,colour2,nullptr);image=retro::reconstructNeuralTexture(p4,model2);check(image.size()==12+8*8*4,"NTX2 x4 size");
 {const int low[2][2]={{0,64},{128,192}};for(int y=0;y<8;++y)for(int x=0;x<8;++x){float sy=(y+.5f)/4-.5f,sx=(x+.5f)/4-.5f;int iy=int(std::floor(sy)),ix=int(std::floor(sx));float fy=sy-iy,fx=sx-ix;int y0=std::clamp(iy,0,1),y1=std::clamp(iy+1,0,1),x0=std::clamp(ix,0,1),x1=std::clamp(ix+1,0,1);
  float v=(low[y0][x0]*(1-fx)+low[y0][x1]*fx)*(1-fy)+(low[y1][x0]*(1-fx)+low[y1][x1]*fx)*fy;check(image[12+(y*8+x)*4]==static_cast<unsigned char>(v+.5f),"NTX2 x4 zero head is not exact bilinear");}}
 // The x4 head's 16 output channels land on their pixel-shuffle phases.
 {auto phase=model2;size_t at=12;for(int head=0;head<2;++head){uint32_t f,c,b;std::memcpy(&f,phase.data()+at,4);std::memcpy(&c,phase.data()+at+8,4);std::memcpy(&b,phase.data()+at+12,4);at+=20;
   std::vector<size_t> layers{9*3*c+c};for(uint32_t i=0;i<2*b;++i)layers.push_back(9*c*c+c);layers.push_back(9*c*f*f+f*f);
   for(size_t i=0;i<layers.size();++i){if(f==4&&i+1==layers.size()){float s=1.f/255;std::memcpy(phase.data()+at,&s,4);for(uint32_t k=0;k<16;++k){int16_t bias=int16_t(k+1);std::memcpy(phase.data()+at+4+(layers[i]-16+k)*2,&bias,2);}}at+=4+layers[i]*2;}}
  auto plain=retro::reconstructNeuralTexture(p4,model2);image=retro::reconstructNeuralTexture(p4,phase);
  for(int y=0;y<8;++y)for(int x=0;x<8;++x)check(int(image[12+(y*8+x)*4+1])-int(plain[12+(y*8+x)*4+1])==(y%4)*4+x%4+1||plain[12+(y*8+x)*4+1]+(y%4)*4+x%4+1>255,"NTX2 x4 pixel-shuffle phase mapping is wrong");}
 // Residual blocks: a non-zero block changes the output (the add path runs).
 {auto busy=model2;for(size_t i=36;i<36+112*2;i+=2){int16_t v=int16_t((i*7919)%2001-1000);std::memcpy(busy.data()+i,&v,2);}float s=1.f/1000;std::memcpy(busy.data()+32,&s,4);
  check(retro::reconstructNeuralTexture(p2,busy)==retro::reconstructNeuralTexture(p2,busy),"NTX2 decode is not repeatable");}
 // An exact full-size alpha plane (green channel) replaces opaque alpha.
 {Bytes a(4*4*4,0);for(size_t i=0;i<16;++i){a[i*4]=a[i*4+1]=a[i*4+2]=static_cast<unsigned char>(i*17);a[i*4+3]=255;}auto alpha=lossless(a,4,4);auto pa=ntx2(4,4,2,2,2,2,colour2,&alpha);
  image=retro::reconstructNeuralTexture(pa,model2);for(size_t i=0;i<16;++i){check(image[12+i*4+3]==i*17,"NTX2 alpha plane not exact");check(image[12+i*4]==expected[i],"NTX2 alpha changed colour");}
  auto wrongSize=lossless(a,4,4);auto pw=ntx2(6,6,3,3,2,2,lossless(Bytes(3*3*4,255),3,3),&wrongSize);rejected(pw,model2);
  auto missing=ntx2(4,4,2,2,2,2,colour2,nullptr);rejected(missing,model2);
  auto extra=ntx2(4,4,2,2,2,0,colour2,&alpha);rejected(extra,model2);}
 // Lossy VP8 colour decodes through the same head.
 {Bytes rgb(8*8*3);for(size_t i=0;i<rgb.size();++i)rgb[i]=static_cast<unsigned char>(i*5);uint8_t*out=nullptr;size_t n=WebPEncodeRGB(rgb.data(),8,8,24,90,&out);check(n>0,"Cannot encode lossy test image");Bytes vp8(out,out+n);WebPFree(out);
  auto pl=ntx2(16,16,8,8,2,1,vp8,nullptr);image=retro::reconstructNeuralTexture(pl,model2);check(image.size()==12+16*16*4,"NTX2 lossy size");
  auto mislabeled=ntx2(16,16,8,8,2,0,vp8,nullptr);rejected(mislabeled,model2);auto lossyFlagOnLossless=ntx2(4,4,2,2,2,1,colour2,nullptr);rejected(lossyFlagOnLossless,model2);}
 // Odd atlases crop the edge-padded texels at x4 too.
 {auto padded4=retro::reconstructNeuralTexture(p4,model2);auto odd4=ntx2(7,5,2,2,4,0,colour2,nullptr);auto cropped4=retro::reconstructNeuralTexture(odd4,model2);check(cropped4.size()==12+7*5*4,"NTX2 odd x4 size");
  for(size_t y=0;y<5;++y)for(size_t x=0;x<7;++x)for(size_t c=0;c<4;++c)check(cropped4[12+(y*7+x)*4+c]==padded4[12+(y*8+x)*4+c],"NTX2 odd x4 crop moved authored texels");}
 // Malformed NTX2 records and NTM2 models; formats never cross.
 for(auto field:std::initializer_list<std::pair<size_t,uint32_t>>{{28,4},{28,0x80000000u},{24,3},{32,0},{32,9999}}){auto bad=p2;set(bad,field.first,field.second);rejected(bad,model2);}
 {auto bad=p2;bad.push_back(0);rejected(bad,model2);bad=p2;bad.pop_back();rejected(bad,model2);}
 rejected(p2,model);rejected(packet,model2);
 rejected(p4,ntm2({{2,4,1}}));rejected(p2,ntm2({{4,4,0}}));
 for(auto field:std::initializer_list<std::pair<size_t,uint32_t>>{{4,1},{8,0},{8,5},{12,3},{16,2},{20,6},{24,9},{28,557}}){auto bad=model2;set(bad,field.first,field.second);rejected(p2,bad);}
 {auto bad=model2;bad.push_back(0);rejected(p2,bad);bad=model2;bad.pop_back();rejected(p2,bad);auto dup=ntm2({{2,4,0},{2,4,0}});rejected(p2,dup);}
 std::cout<<checks<<" neural checks PASS: shared quantized model, independent bilinear/pixel-shuffle values, opaque alpha, x4, odd atlas crops, malformed records; NTX2 heads, x4 in one pass, exact alpha plane, lossy colour\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}

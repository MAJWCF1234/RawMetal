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
 std::cout<<checks<<" neural checks PASS: shared quantized model, independent bilinear/pixel-shuffle values, opaque alpha, x4, odd atlas crops, malformed records\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}

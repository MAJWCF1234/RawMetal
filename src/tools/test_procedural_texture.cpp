#include "../core/ProceduralTexture.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <array>
#include <algorithm>
#include <stdexcept>
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("Usage: texture-test source-directory preview-directory");
 std::filesystem::create_directories(argv[2]);
 const char*names[]={"concrete-floor","sign-rust","panel-metal","service-floor","weathered-metal"};
 for(auto name:names){
  auto path=std::filesystem::path(argv[1])/"assets/materials/procedural"/(std::string(name)+".rmp");std::ifstream file(path,std::ios::binary);
  std::vector<unsigned char> recipe((std::istreambuf_iterator<char>(file)),{});auto raw=retro::generateProceduralTexture(recipe);
  if(raw!=retro::generateProceduralTexture(recipe))throw std::runtime_error("Non-deterministic generation");
  uint32_t w,h;std::memcpy(&w,raw.data()+4,4);std::memcpy(&h,raw.data()+8,4);
  if(raw.size()!=12+size_t(w)*h*4)throw std::runtime_error("Invalid output size");
  unsigned low=255,high=0;for(size_t i=12;i<raw.size();i+=4){if(raw[i+3]!=255)throw std::runtime_error("Unexpected transparency");low=std::min(low,unsigned(raw[i]));high=std::max(high,unsigned(raw[i]));}
  if(high-low<12)throw std::runtime_error("Flat procedural material");
  std::ofstream image(std::filesystem::path(argv[2])/(std::string(name)+".ppm"),std::ios::binary);image<<"P6\n"<<w<<' '<<h<<"\n255\n";for(size_t i=12;i<raw.size();i+=4)image.write(reinterpret_cast<const char*>(raw.data()+i),3);
  for(int invalid=0;invalid<5;++invalid){auto bad=recipe;if(invalid==0)bad.resize(31);else if(invalid==1)bad[0]='X';else{uint32_t word=invalid==2?99:invalid==3?0:257;std::memcpy(bad.data()+(invalid==2?4:8),&word,4);}
   bool rejected=false;try{retro::generateProceduralTexture(bad);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Malformed recipe accepted");}
  std::cout<<name<<": deterministic opaque "<<w<<'x'<<h<<" range "<<low<<'-'<<high<<"; malformed recipes rejected PASS\n";
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#pragma once
#include <cstdlib>
#include <string>
#include <unordered_map>
namespace retro {
// Look tuning for captures and tests: RM_LOOK="name=value,name=value" (read once, at the first ask)
// overrides a look value's default, so one build can try several settings. Play never sets it; every
// default is the value chosen by eye. The shaders' share rides in the frame's lamp header
// (GpuRenderer::writeProbeHeader, the last reflection probe's record).
inline float lookTune(const char* name,float fallback){
 static const std::unordered_map<std::string,float> values=[]{std::unordered_map<std::string,float> out;
  if(const char* text=std::getenv("RM_LOOK")){std::string all(text);size_t at=0;
   while(at<all.size()){size_t end=all.find(',',at);if(end==std::string::npos)end=all.size();const std::string pair=all.substr(at,end-at);const size_t eq=pair.find('=');
    if(eq!=std::string::npos)out[pair.substr(0,eq)]=float(std::atof(pair.c_str()+eq+1));at=end+1;}}
  return out;}();
 const auto found=values.find(name);return found==values.end()?fallback:found->second;}
}

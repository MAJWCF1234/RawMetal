#pragma once
#include <vector>
#include <cstdint>
#include <cstring>

namespace retro {
// Reversible preprocessing, not audio transcoding. Even WAV headers and an odd
// trailing byte survive exactly; no sample rate/channel/precision changes.
inline std::vector<unsigned char> audioPredict(const std::vector<unsigned char>& input,
                                               bool secondOrder,bool planar,bool decode){
 auto output=input;const size_t words=input.size()/2;uint16_t previous=0,previousDelta=0;
 for(size_t i=0;i<words;++i){
  uint16_t value;
  if(decode&&planar)value=uint16_t(input[i])|uint16_t(input[words+i])<<8;
  else std::memcpy(&value,input.data()+i*2,2);
  uint16_t result;
  if(decode){
   uint16_t delta=secondOrder?uint16_t(value+previousDelta):value;
   result=uint16_t(previous+delta);previous=result;previousDelta=delta;
  }else{
   uint16_t delta=uint16_t(value-previous);previous=value;
   result=secondOrder?uint16_t(delta-previousDelta):delta;previousDelta=delta;
  }
  if(!decode&&planar){output[i]=static_cast<unsigned char>(result);output[words+i]=static_cast<unsigned char>(result>>8);}
  else std::memcpy(output.data()+i*2,&result,2);
 }
 return output;
}
}

#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace retro {
struct AnimationPredictionLayout {
 size_t prefix=0,payload=0,frames=0,vertices=0;
 std::vector<uint32_t> samples;
};
// RMA2 uses a fixed sample count; RMA3 stores one count per clip. Only the
// quantized XYZ frame payload is transformed. Topology and all joint/timing
// trailers remain byte-identical, including future opaque trailers.
inline bool animationPredictionLayout(const std::vector<unsigned char>&input,AnimationPredictionLayout&layout){
 if(input.size()<20)return false;
 bool compact=!std::memcmp(input.data(),"RMA3",4);
 if(!compact&&std::memcmp(input.data(),"RMA2",4))return false;
 uint32_t header[4];std::memcpy(header,input.data()+4,16);
 auto [corners,frames,clips,vertices]=header;
 if(!corners||corners>1000000||!vertices||vertices>65535||!clips||clips>32)return false;
 size_t prefix=20+size_t(corners)*2+(compact?size_t(clips)*4:0);
 if(prefix>input.size())return false;
 AnimationPredictionLayout candidate;candidate.prefix=prefix;candidate.vertices=vertices;
 for(uint32_t clip=0;clip<clips;++clip){uint32_t count=frames;
  if(compact)std::memcpy(&count,input.data()+20+size_t(corners)*2+size_t(clip)*4,4);
  if(count<2||count>512)return false;
  candidate.samples.push_back(count);candidate.frames+=count;
 }
 if(compact&&candidate.frames!=frames)return false;
 const size_t stride=candidate.vertices*6;
 if(candidate.frames>(input.size()-prefix)/stride)return false;
 candidate.payload=candidate.frames*stride;
 layout=std::move(candidate);return true;
}
// Modulo-u16 differences predict the same vertex/axis in the preceding
// frame. Six axis/byte planes cluster small motion deltas for LZMS. Every
// clip resets independently: a new clip's first frame is its absolute pose.
inline std::vector<unsigned char> animationPredict(const std::vector<unsigned char>&input,bool decode){
 AnimationPredictionLayout layout;
 if(!animationPredictionLayout(input,layout))throw std::runtime_error("Invalid predicted animation layout");
 auto output=input;const size_t plane=layout.frames*layout.vertices;size_t frameOffset=0;
 for(uint32_t count:layout.samples){
  for(size_t frame=0;frame<count;++frame)for(size_t vertex=0;vertex<layout.vertices;++vertex)for(size_t axis=0;axis<3;++axis){
   size_t sample=frameOffset+frame,index=sample*layout.vertices+vertex;
   size_t position=layout.prefix+index*6+axis*2;
   uint16_t previous=0,value;
   if(frame)std::memcpy(&previous,(decode?output:input).data()+position-layout.vertices*6,2);
   if(decode){
    value=uint16_t(input[layout.prefix+(axis*2)*plane+index])|uint16_t(input[layout.prefix+(axis*2+1)*plane+index])<<8;
    value=uint16_t(value+previous);std::memcpy(output.data()+position,&value,2);
   }else{
    std::memcpy(&value,input.data()+position,2);value=uint16_t(value-previous);
    output[layout.prefix+(axis*2)*plane+index]=static_cast<unsigned char>(value);
    output[layout.prefix+(axis*2+1)*plane+index]=static_cast<unsigned char>(value>>8);
   }
  }
  frameOffset+=count;
 }
 return output;
}
}

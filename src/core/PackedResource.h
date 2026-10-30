#pragma once
#include <windows.h>
#include <compressapi.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "TextureCodec.h"
#include "AudioPredictor.h"
#include "BinaryPredictor.h"
#include "AnimationPredictor.h"
#include "NeuralTexture.h"

#include "../ThirdParty/libwebp/src/webp/decode.h"
namespace retro {
// Format 2 is the lossless packing of a PNG; format 1 is a screened lossy
// release texture (VP8 colour, exact alpha). Both decode to RMT1 pixels.
inline std::vector<unsigned char> decodeWebpTexture(const std::vector<unsigned char>& input,int format){
 WebPBitstreamFeatures features{};
 if(WebPGetFeatures(input.data(),input.size(),&features)!=VP8_STATUS_OK||features.format!=format||features.has_animation||features.width<=0||features.height<=0||uint64_t(features.width)*features.height*4>128*1024*1024)
  throw std::runtime_error("Invalid WebP texture");
 std::vector<unsigned char> raw(12+size_t(features.width)*features.height*4);
 std::memcpy(raw.data(),"RMT1",4);std::memcpy(raw.data()+4,&features.width,4);std::memcpy(raw.data()+8,&features.height,4);
 if(!WebPDecodeRGBAInto(input.data(),input.size(),raw.data()+12,raw.size()-12,features.width*4))throw std::runtime_error("WebP texture decode failed");
 return raw;
}
inline std::vector<unsigned char> decodeLosslessTexture(const std::vector<unsigned char>& input){return decodeWebpTexture(input,2);}
inline std::vector<unsigned char> decodeLossyTexture(const std::vector<unsigned char>& input){return decodeWebpTexture(input,1);}
inline std::vector<unsigned char> loadResourceFromModule(HMODULE module,int id,int aliasDepth=0,int neuralDepth=0) {
 auto res=FindResourceW(module,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));
 if(!res)throw std::runtime_error("Missing embedded resource");
 auto bytes=static_cast<const unsigned char*>(LockResource(LoadResource(module,res)));size_t size=SizeofResource(module,res);
 if(bytes&&size==8&&!std::memcmp(bytes,"RMR1",4)){
  uint32_t target=0;std::memcpy(&target,bytes+4,4);
  // References always point directly to an ordinary packed resource. Reject
  // self references/chains, including cycles in a malformed executable.
  if(aliasDepth||!target||target>65535||target==uint32_t(id))throw std::runtime_error("Invalid resource reference");
  return loadResourceFromModule(module,int(target),1,neuralDepth);
 }
 if(!bytes||size<12||std::memcmp(bytes,"RMZ1",4))throw std::runtime_error("Invalid packed resource");
 uint32_t count=0,transform=0;std::memcpy(&count,bytes+4,4);std::memcpy(&transform,bytes+8,4);
 if(!count||count>128*1024*1024||transform>15||transform==11||transform==12)throw std::runtime_error("Invalid resource size or encoding");
 std::vector<unsigned char> decoded(count);DECOMPRESSOR_HANDLE decoder=nullptr;
 if(!CreateDecompressor(COMPRESS_ALGORITHM_LZMS,nullptr,&decoder))throw std::runtime_error("Cannot create resource decoder");
 SIZE_T actual=0;bool ok=Decompress(decoder,bytes+12,size-12,decoded.data(),count,&actual)!=FALSE;CloseDecompressor(decoder);
 if(!ok||actual!=count)throw std::runtime_error("Resource decompression failed");
 if(transform==1){uint16_t previous=0;for(size_t i=0;i+1<decoded.size();i+=2){uint16_t word;std::memcpy(&word,decoded.data()+i,2);word=uint16_t(word+previous);std::memcpy(decoded.data()+i,&word,2);previous=word;}}
 if(transform==2){if(count<12||std::memcmp(decoded.data(),"RMT1",4)||(count-12)%4)throw std::runtime_error("Invalid texture encoding");size_t pixels=(count-12)/4;auto planar=decoded;for(size_t c=0;c<4;++c){unsigned char previous=0;for(size_t i=0;i<pixels;++i){previous=static_cast<unsigned char>(previous+planar[12+c*pixels+i]);decoded[12+4*i+c]=previous;}}}
 if(transform==3)decoded=texturePredict(decoded,true);
 if(transform==4)decoded=decodeLosslessTexture(decoded);
 if(transform>=5&&transform<=7)decoded=audioPredict(decoded,transform!=6,transform!=5,true);
 if(transform==8||transform==9)decoded=binaryPredict(decoded,transform==9,true);
 if(transform==10)decoded=animationPredict(decoded,true);
 if(transform==14)decoded=animationGridPredict(decoded,true);
 if(transform==15)decoded=decodeLossyTexture(decoded);
 if(transform==13){
  if(neuralDepth)throw std::runtime_error("Neural texture model recursion");
  uint32_t modelId=neuralTextureModelResource(decoded);
  if(modelId==uint32_t(id))throw std::runtime_error("Neural texture references itself");
  auto model=loadResourceFromModule(module,int(modelId),0,1);
  decoded=reconstructNeuralTexture(decoded,model);
 }

 return decoded;
}
inline std::vector<unsigned char> loadResource(int id){return loadResourceFromModule(GetModuleHandleW(nullptr),id);}
}

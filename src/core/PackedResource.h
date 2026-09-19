#pragma once
#include <windows.h>
#include <compressapi.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "TextureCodec.h"
namespace retro {
inline std::vector<unsigned char> loadResource(int id) {
 auto module=GetModuleHandleW(nullptr);auto res=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
 if(!res)throw std::runtime_error("Missing embedded resource");
 auto bytes=static_cast<const unsigned char*>(LockResource(LoadResource(module,res)));size_t size=SizeofResource(module,res);
 if(!bytes||size<12||std::memcmp(bytes,"RMZ1",4))throw std::runtime_error("Invalid packed resource");
 uint32_t count=0,transform=0;std::memcpy(&count,bytes+4,4);std::memcpy(&transform,bytes+8,4);
 if(!count||count>128*1024*1024||transform>3)throw std::runtime_error("Invalid resource size or encoding");
 std::vector<unsigned char> decoded(count);DECOMPRESSOR_HANDLE decoder=nullptr;
 if(!CreateDecompressor(COMPRESS_ALGORITHM_LZMS,nullptr,&decoder))throw std::runtime_error("Cannot create resource decoder");
 SIZE_T actual=0;bool ok=Decompress(decoder,bytes+12,size-12,decoded.data(),count,&actual)!=FALSE;CloseDecompressor(decoder);
 if(!ok||actual!=count)throw std::runtime_error("Resource decompression failed");
 if(transform==1){uint16_t previous=0;for(size_t i=0;i+1<decoded.size();i+=2){uint16_t word;std::memcpy(&word,decoded.data()+i,2);word=uint16_t(word+previous);std::memcpy(decoded.data()+i,&word,2);previous=word;}}
 if(transform==2){if(count<12||std::memcmp(decoded.data(),"RMT1",4)||(count-12)%4)throw std::runtime_error("Invalid texture encoding");size_t pixels=(count-12)/4;auto planar=decoded;for(size_t c=0;c<4;++c){unsigned char previous=0;for(size_t i=0;i<pixels;++i){previous=static_cast<unsigned char>(previous+planar[12+c*pixels+i]);decoded[12+4*i+c]=previous;}}}
 if(transform==3)decoded=texturePredict(decoded,true);
 return decoded;
}
}

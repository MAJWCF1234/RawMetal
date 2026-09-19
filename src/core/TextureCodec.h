#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
namespace retro {
// Reversible colour decorrelation + 2D prediction; no quantization or resizing.
inline std::vector<unsigned char> texturePredict(const std::vector<unsigned char>& input,bool decode){
 if(input.size()<12||std::memcmp(input.data(),"RMT1",4))throw std::runtime_error("Invalid predicted texture");
 uint32_t w,h;std::memcpy(&w,input.data()+4,4);std::memcpy(&h,input.data()+8,4);
 if(!w||!h||uint64_t(w)*h*4+12!=input.size())throw std::runtime_error("Invalid texture dimensions");
 size_t count=size_t(w)*h;auto out=input;std::vector<unsigned char> plane(count);
 for(size_t c=0;c<4;++c){
  for(size_t i=0;i<count;++i){int a=i%w?plane[i-1]:0,b=i>=w?plane[i-w]:0,d=i>=w&&i%w?plane[i-w-1]:0;
   int p=a+b-d,pa=std::abs(p-a),pb=std::abs(p-b),pc=std::abs(p-d),prediction=pa<=pb&&pa<=pc?a:pb<=pc?b:d;
   if(decode){plane[i]=static_cast<unsigned char>(input[12+c*count+i]+prediction);out[12+i*4+c]=plane[i];}
   else{auto value=static_cast<unsigned char>(input[12+i*4+c]-((c==0||c==2)?input[12+i*4+1]:0));plane[i]=value;out[12+c*count+i]=static_cast<unsigned char>(value-prediction);}
  }
 }
 if(decode)for(size_t i=0;i<count;++i){out[12+i*4]=static_cast<unsigned char>(out[12+i*4]+out[12+i*4+1]);out[12+i*4+2]=static_cast<unsigned char>(out[12+i*4+2]+out[12+i*4+1]);}
 return out;
}
}

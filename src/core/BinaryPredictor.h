#pragma once
#include <vector>
#include <cstddef>
namespace retro {
// Separate interleaved byte lanes so float/integer exponent and high bytes
// cluster together. Optional XOR prediction is reversible for every byte;
// no quantization, changed model topology or animation resampling occurs.
inline std::vector<unsigned char> binaryPredict(const std::vector<unsigned char>& input,bool predict,bool decode){
 std::vector<unsigned char> output(input.size());size_t cursor=0;
 for(size_t lane=0;lane<4;++lane){unsigned char previous=0;
  for(size_t i=lane;i<input.size();i+=4){
   if(decode){auto value=static_cast<unsigned char>(input[cursor++]^(predict?previous:0));output[i]=value;previous=value;}
   else{auto value=input[i];output[cursor++]=static_cast<unsigned char>(value^(predict?previous:0));previous=value;}
  }
 }
 return output;
}
}

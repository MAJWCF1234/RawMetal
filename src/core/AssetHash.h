#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <span>
#include <stdexcept>
namespace retro {
inline std::array<unsigned char,32> assetHash(std::span<const unsigned char> bytes){
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::array<unsigned char,32> result{};
 if(bytes.size()>0xffffffffu||BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("Cannot initialize asset integrity check");
 bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),0)>=0&&BCryptFinishHash(hash,result.data(),ULONG(result.size()),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
 if(!ok)throw std::runtime_error("Asset integrity check failed");return result;
}
}

#pragma once
// Admin unlock for sealed campaign levels in a public Campaign Builder.
// A whitelisted name and PIN, checked against a salted SHA-256 of
// salt + name + PIN; the PIN itself is in no file. A four-digit PIN is a soft
// gate only (it can be brute-forced), which is all it is meant to be. The unlock
// is remembered per Windows user (HKCU\Software\DepthWorks\Campaign Builder), and
// the game and the builder both read it there.
#include "AssetHash.h"
#include <windows.h>
#include <cwctype>
#include <span>
#include <string>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

namespace retro::admin {
inline constexpr const char* Salt="DepthWorks/CampaignBuilder/admin/v1:";
struct Admin {const char* name;const char* hash;};
// SHA-256(Salt + name + PIN), lower-case hex.
inline constexpr Admin Whitelist[]={{"Kara","a1d7fe9299e5ca86de5b1ec56c7f4e7fe69d740da706eb1e0151f1fdd5a771b3"}};
inline constexpr const wchar_t* Key=L"Software\\DepthWorks\\Campaign Builder";

inline std::string hash(const std::string& name,const std::string& pin){
 const std::string text=std::string(Salt)+name+pin;
 const auto digest=assetHash(std::span<const unsigned char>(reinterpret_cast<const unsigned char*>(text.data()),text.size()));
 static const char* hex="0123456789abcdef";std::string out;for(auto b:digest){out+=hex[b>>4];out+=hex[b&15];}return out;
}
// The whitelisted spelling of a typed name (any case), or null.
inline const Admin* find(std::string name){
 while(!name.empty()&&name.back()==' ')name.pop_back();while(!name.empty()&&name.front()==' ')name.erase(name.begin());
 for(const auto& a:Whitelist){std::string w=a.name;if(w.size()!=name.size())continue;bool same=true;
  for(size_t i=0;i<w.size();++i)same&=std::towlower(wchar_t(static_cast<unsigned char>(w[i])))==std::towlower(wchar_t(static_cast<unsigned char>(name[i])));if(same)return &a;}
 return nullptr;
}
inline std::wstring read(const wchar_t* value){
 wchar_t buffer[256]{};DWORD size=sizeof(buffer);
 if(RegGetValueW(HKEY_CURRENT_USER,Key,value,RRF_RT_REG_SZ,nullptr,buffer,&size)!=ERROR_SUCCESS)return {};
 return buffer;
}
inline std::string narrow(const std::wstring& w){std::string s;for(wchar_t c:w)s+=c<128?char(c):'?';return s;}
// The admin this Windows user unlocked as, or empty.
inline std::string unlocked(){
 const auto name=narrow(read(L"Admin")),key=narrow(read(L"AdminKey"));
 const auto* a=find(name);return a&&key==a->hash?std::string(a->name):std::string();
}
// Checks a name and PIN; on a match remembers the unlock and returns true.
inline bool unlock(const std::string& name,const std::string& pin){
 const auto* a=find(name);if(!a||hash(a->name,pin)!=a->hash)return false;
 const std::wstring wname(a->name,a->name+std::char_traits<char>::length(a->name)),wkey(a->hash,a->hash+64);
 return RegSetKeyValueW(HKEY_CURRENT_USER,Key,L"Admin",REG_SZ,wname.c_str(),DWORD((wname.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS&&
        RegSetKeyValueW(HKEY_CURRENT_USER,Key,L"AdminKey",REG_SZ,wkey.c_str(),DWORD((wkey.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;
}
inline void lock(){RegDeleteKeyValueW(HKEY_CURRENT_USER,Key,L"Admin");RegDeleteKeyValueW(HKEY_CURRENT_USER,Key,L"AdminKey");}
}

#pragma once
// Live tuning knobs: retro::tune("particles.steamRate",1.f) is the default
// until a tune file names it. The file is RM_TUNE=path (the game, any build) or
// lab/tune.txt (the feature lab, which sets the path itself), one knob a line:
//   particles.steamRate 1.4        # a float
//   blood.tint #8a1010             # a colour (tuneColour), or three floats
// It is re-read when it changes (checked at most four times a second), so a
// running lab or game picks up an edit on its next frame. With no tune file the
// default is returned and no file is ever touched: release behaviour is the same.
// Read a knob where it is used (a hash lookup); don't cache it in a static if
// it should move live.
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
namespace retro {
struct TuneFile {
 std::mutex lock;
 std::string path;bool configured=false;
 std::unordered_map<std::string,std::string> values;
 std::filesystem::file_time_type stamp{};std::chrono::steady_clock::time_point checked{};
 std::uint64_t generation=0; // bumped on every reload that changed the file
 static TuneFile& instance(){static TuneFile file;return file;}
 void use(const std::string& file){std::lock_guard<std::mutex> hold(lock);path=file;configured=true;stamp={};checked={};values.clear();}
 void refresh(){ // under lock
  if(!configured){configured=true;if(const char* env=std::getenv("RM_TUNE"))path=env;}
  if(path.empty())return;
  const auto now=std::chrono::steady_clock::now();if(checked.time_since_epoch().count()&&now-checked<std::chrono::milliseconds(250))return;checked=now;
  std::error_code ec;const auto t=std::filesystem::last_write_time(path,ec);if(ec||t==stamp)return;stamp=t;
  std::ifstream in(path);std::string line;values.clear();
  while(std::getline(in,line)){const auto hash=line.find('#');const auto first=line.find_first_not_of(" \t");
   if(first==std::string::npos||line[first]=='#')continue;
   // a colour's '#' follows a space: only a '#' at a word start that is not a hex colour ends the line
   std::istringstream words(line);std::string name;words>>name;std::string rest;std::getline(words,rest);
   const auto start=rest.find_first_not_of(" \t");rest=start==std::string::npos?"":rest.substr(start);
   const auto comment=rest.find(" #");if(comment!=std::string::npos)rest=rest.substr(0,comment);
   while(!rest.empty()&&(rest.back()==' '||rest.back()=='\t'||rest.back()=='\r'))rest.pop_back();
   (void)hash;if(!name.empty())values[name]=rest;}
  ++generation;
 }
 bool find(const char* name,std::string& out){std::lock_guard<std::mutex> hold(lock);refresh();const auto it=values.find(name);if(it==values.end())return false;out=it->second;return true;}
};
// Point the knobs at a file (the lab does; the game reads RM_TUNE on first use).
inline void tuneFile(const std::string& path){TuneFile::instance().use(path);}
// How many times the knobs have been (re)loaded: a frame can tell they moved.
inline std::uint64_t tuneGeneration(){auto& f=TuneFile::instance();std::lock_guard<std::mutex> hold(f.lock);f.refresh();return f.generation;}
inline float tune(const char* name,float fallback){
 std::string text;if(!TuneFile::instance().find(name,text)||text.empty())return fallback;
 char* end=nullptr;const float v=std::strtof(text.c_str(),&end);return end==text.c_str()?fallback:v;}
// A colour knob: "#rrggbb" or "r g b" (0..1 floats). Returns 0xRRGGBB.
inline std::uint32_t tuneColour(const char* name,std::uint32_t fallback){
 std::string text;if(!TuneFile::instance().find(name,text)||text.empty())return fallback;
 if(text[0]=='#'&&text.size()>=7)return std::uint32_t(std::strtoul(text.c_str()+1,nullptr,16))&0xffffffu;
 std::istringstream in(text);float r=-1,g=-1,b=-1;in>>r>>g>>b;if(b<0)return fallback;
 auto byte=[](float v){return std::uint32_t(v<0?0:v>1?255:int(v*255+.5f));};return (byte(r)<<16)|(byte(g)<<8)|byte(b);}
}

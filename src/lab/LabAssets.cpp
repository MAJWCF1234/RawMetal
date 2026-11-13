// Loose assets for the lab: every resource assets.rc names is read from its
// source file and decoded the way the packer's round trip promises the packed
// one decodes (tools/pack_assets.cpp): a PNG to RMT1 pixels, a lossy WebP to its
// pixels, a neural texture reconstructed with its model, a gridded animation
// quantised to its release grid; everything else byte for byte. No packing, no
// embedding: an edited texture is in the next lab run.
//
// Roots, searched in order for each path: RM_ASSET_ROOT (';'-separated), this
// source tree, and the main checkout's src when this tree is a worktree under
// .claude/worktrees (for the gitignored library/ the worktree lacks).
#include "lab/LabAssets.h"
#include "core/PackedResource.h"
#include "core/AnimationPredictor.h"
#include "core/NeuralTexture.h"
#include "ThirdParty/stb/stb_image.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <unordered_map>
namespace fs=std::filesystem;
namespace retro::lab {
namespace {
struct Manifest {std::unordered_map<int,std::string> paths;std::vector<fs::path> roots;std::mutex lock;AssetStats stats;};
Manifest& manifest(){static Manifest m;return m;}
std::vector<unsigned char> readFile(const fs::path& p){std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("lab: cannot read asset "+p.string());return {std::istreambuf_iterator<char>(in),{}};}
std::vector<unsigned char> rawTexture(const std::vector<unsigned char>& png){
 int w=0,h=0,channels=0;auto pixels=stbi_load_from_memory(png.data(),int(png.size()),&w,&h,&channels,4);if(!pixels)throw std::runtime_error("lab: PNG decode failed");
 std::vector<unsigned char> raw(12+size_t(w)*h*4);std::memcpy(raw.data(),"RMT1",4);std::memcpy(raw.data()+4,&w,4);std::memcpy(raw.data()+8,&h,4);std::memcpy(raw.data()+12,pixels,raw.size()-12);stbi_image_free(pixels);return raw;}
std::vector<unsigned char> loose(int id){
 auto& m=manifest();fs::path file;std::string relative;
 {std::lock_guard<std::mutex> hold(m.lock);auto it=m.paths.find(id);if(it==m.paths.end())throw std::runtime_error("Missing embedded resource "+std::to_string(id)+" (not in assets.rc)");relative=it->second;
  for(const auto& root:m.roots){std::error_code ec;if(fs::exists(root/relative,ec)){file=root/relative;break;}}}
 if(file.empty())throw std::runtime_error("lab: asset "+std::to_string(id)+" "+relative+" is in no asset root (set RM_ASSET_ROOT)");
 const auto started=std::chrono::steady_clock::now();
 auto bytes=readFile(file);const auto ext=file.extension().string();
 if(ext==".png")bytes=rawTexture(bytes);
 else if(ext==".webp")bytes=decodeLossyTexture(bytes);
 else if(ext==".ntx")bytes=reconstructNeuralTexture(bytes,loose(int(neuralTextureModelResource(bytes))));
 else if(ext==".bin"){AnimationPredictionLayout grid;if(animationPredictionLayout(bytes,grid))bytes=animationQuantize(bytes);}
 std::lock_guard<std::mutex> hold(m.lock);++m.stats.loads;m.stats.last=std::to_string(id)+" "+relative;if(std::getenv("RM_LAB_ASSET_LOG"))std::fprintf(stderr,"asset %d %s %zu\n",id,relative.c_str(),bytes.size());m.stats.bytes+=bytes.size();m.stats.ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 return bytes;
}
}
bool useLooseAssets(std::string& error){
 auto& m=manifest();
 if(const char* env=std::getenv("RM_ASSET_ROOT")){std::string all=env;size_t at=0;while(at<=all.size()){size_t end=all.find(';',at);if(end==std::string::npos)end=all.size();if(end>at)m.roots.emplace_back(all.substr(at,end-at));at=end+1;}}
 const fs::path source=RAWMETAL_LAB_SOURCE;m.roots.push_back(source);
 const std::string s=source.generic_string();const auto cut=s.find("/.claude/worktrees/");
 if(cut!=std::string::npos)m.roots.push_back(fs::path(s.substr(0,cut))/"src");
 std::ifstream rc(source/"assets.rc");if(!rc){error="cannot read "+(source/"assets.rc").string();return false;}
 const std::regex pattern(R"rc(^(\d+)\s+\w+\s+"([^"]+)")rc");std::string line;
 while(std::getline(rc,line)){std::smatch match;if(std::regex_search(line,match,pattern))m.paths[std::stoi(match[1].str())]=match[2].str();}
 if(m.paths.empty()){error="assets.rc names no resources";return false;}
 g_looseResourceLoader=&loose;return true;
}
AssetStats assetStats(){auto& m=manifest();std::lock_guard<std::mutex> hold(m.lock);auto s=m.stats;s.manifest=int(m.paths.size());return s;}
std::vector<std::string> assetRoots(){std::vector<std::string> out;for(const auto& r:manifest().roots)out.push_back(r.string());return out;}
}

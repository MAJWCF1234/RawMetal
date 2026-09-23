#include <windows.h>
#include <compressapi.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "../core/AssetHash.h"
#include "../core/TextureCodec.h"
#include "../core/PackedResource.h"
#include "../ThirdParty/libwebp/src/webp/encode.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/stb/stb_image.h"
namespace fs=std::filesystem;
using Bytes=std::vector<unsigned char>;
void publish(const fs::path& temporary,const fs::path& destination){
 if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
  throw std::runtime_error("Cannot publish completed asset output");
}
Bytes rawTexture(const Bytes& png){
 int w,h,channels;auto pixels=stbi_load_from_memory(png.data(),int(png.size()),&w,&h,&channels,4);if(!pixels)throw std::runtime_error("PNG decode failed");
 Bytes raw(12+size_t(w)*h*4);std::memcpy(raw.data(),"RMT1",4);std::memcpy(raw.data()+4,&w,4);std::memcpy(raw.data()+8,&h,4);std::memcpy(raw.data()+12,pixels,raw.size()-12);stbi_image_free(pixels);return raw;
}
Bytes losslessTexture(const Bytes& raw){
 WebPConfig config;WebPPicture picture;WebPMemoryWriter writer;
 if(!WebPConfigInit(&config)||!WebPConfigLosslessPreset(&config,9)||!WebPPictureInit(&picture))throw std::runtime_error("Lossless encoder initialization failed");
 config.lossless=1;config.near_lossless=100;config.exact=1;config.thread_level=1;
 picture.use_argb=1;std::memcpy(&picture.width,raw.data()+4,4);std::memcpy(&picture.height,raw.data()+8,4);
 WebPMemoryWriterInit(&writer);picture.writer=WebPMemoryWrite;picture.custom_ptr=&writer;
 bool ok=WebPPictureImportRGBA(&picture,raw.data()+12,picture.width*4)&&WebPEncode(&config,&picture);
 Bytes encoded;if(ok)encoded.assign(writer.mem,writer.mem+writer.size);WebPMemoryWriterClear(&writer);WebPPictureFree(&picture);
 if(!ok||retro::decodeLosslessTexture(encoded)!=raw)throw std::runtime_error("Pixel-exact WebP round-trip failed");return encoded;
}
Bytes compress(const Bytes& input){
 COMPRESSOR_HANDLE encoder=nullptr;if(!CreateCompressor(COMPRESS_ALGORITHM_LZMS,nullptr,&encoder))throw std::runtime_error("Compressor creation failed");
 SIZE_T required=0;Compress(encoder,input.data(),input.size(),nullptr,0,&required);Bytes output(required);
 bool ok=Compress(encoder,input.data(),input.size(),output.data(),output.size(),&required)!=FALSE;CloseCompressor(encoder);
 if(!ok)throw std::runtime_error("Compression failed");output.resize(required);return output;
}
void verify(const Bytes& packed,const Bytes& original){
 DECOMPRESSOR_HANDLE decoder=nullptr;CreateDecompressor(COMPRESS_ALGORITHM_LZMS,nullptr,&decoder);Bytes decoded(original.size());SIZE_T actual=0;
 bool ok=Decompress(decoder,packed.data(),packed.size(),decoded.data(),decoded.size(),&actual)!=FALSE;CloseDecompressor(decoder);
 if(!ok||actual!=original.size()||decoded!=original)throw std::runtime_error("Lossless round-trip failed");
}
int main(int argc,char** argv){try{
 // Exercise wraparound and odd lengths before packing or verifying real files.
 for(size_t length:{size_t(0),size_t(1),size_t(2),size_t(3),size_t(255),size_t(4096)}){
  Bytes probe(length);for(size_t i=0;i<length;++i)probe[i]=static_cast<unsigned char>(i*i*37+i*113);
  for(bool second:{false,true})for(bool planar:{false,true})
   if(retro::audioPredict(retro::audioPredict(probe,second,planar,false),second,planar,true)!=probe)
    throw std::runtime_error("Audio predictor round-trip failed");
 }
 if(argc==4&&std::string(argv[1])=="--verify-exe"){
  // Load PE resources as data, without executing the binary or running DLL code.
  auto module=LoadLibraryExW(fs::absolute(argv[3]).c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);if(!module)throw std::runtime_error("Cannot inspect executable resources");
  struct Close{HMODULE module;~Close(){FreeLibrary(module);}} close{module};
  std::ifstream manifest(fs::path(argv[2])/"assets.rc");if(!manifest)throw std::runtime_error("Missing asset manifest");
  std::regex pattern(R"rc((\d+)\s+\w+\s+"([^"]+)")rc");std::string line;int count=0;
  while(std::getline(manifest,line)){std::smatch match;if(!std::regex_search(line,match,pattern))continue;
   auto path=fs::path(argv[2])/match[2].str();std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Missing source asset");Bytes original((std::istreambuf_iterator<char>(file)),{});
   auto decoded=retro::loadResourceFromModule(module,std::stoi(match[1].str()));
   if(path.extension()==".png"){original=rawTexture(original);if(decoded.size()<12||std::memcmp(decoded.data(),"RMT1",4))decoded=rawTexture(decoded);}
   if(decoded!=original)throw std::runtime_error("Packaged resource mismatch: "+path.string());++count;
  }
  if(!count)throw std::runtime_error("Empty asset manifest");std::cout<<count<<" packaged resources match source: exact RGBA pixels/dimensions, byte-identical audio/models/animation\n";return 0;
 }
 if(argc!=4)throw std::runtime_error("Usage: pack_assets source-dir output-dir bundle-path");fs::path source=argv[1],out=argv[2];fs::create_directories(out);
 struct Entry{uint32_t id;Bytes bytes;};std::vector<Entry> entries;
 std::ifstream manifest(source/"assets.rc");std::ofstream rc(out/"assets-packed.rc.tmp"),report(out/"compression-report.txt.tmp");
 std::regex pattern(R"rc((\d+)\s+\w+\s+"([^"]+)")rc");std::string line;size_t total=0,originalTotal=0;
 while(std::getline(manifest,line)){std::smatch match;if(!std::regex_search(line,match,pattern))continue;
  fs::path path=source/match[2].str();std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read asset");Bytes original((std::istreambuf_iterator<char>(file)),{});
  Bytes chosen=original,packed=compress(original);uint32_t encoding=0;
  auto candidate=[&](const Bytes& data,uint32_t method){auto compressed=compress(data);if(compressed.size()<packed.size()){chosen=data;packed=std::move(compressed);encoding=method;}};
  if(path.extension()==".wav"){
   Bytes delta=original;uint16_t previous=0;for(size_t i=0;i+1<delta.size();i+=2){uint16_t word;std::memcpy(&word,original.data()+i,2);auto difference=uint16_t(word-previous);std::memcpy(delta.data()+i,&difference,2);previous=word;}candidate(delta,1);
   for(uint32_t method:{5u,6u,7u}){
    auto predicted=retro::audioPredict(original,method!=6,method!=5,false);
    if(retro::audioPredict(predicted,method!=6,method!=5,true)!=original)throw std::runtime_error("Audio prediction changed source bytes");
    candidate(predicted,method);
   }
  }
  if(path.extension()==".png"){
   Bytes raw=rawTexture(original);size_t count=(raw.size()-12)/4;
   candidate(raw,0);auto planar=raw;
   for(size_t c=0;c<4;++c){unsigned char previous=0;for(size_t i=0;i<count;++i){auto value=raw[12+4*i+c];planar[12+c*count+i]=static_cast<unsigned char>(value-previous);previous=value;}}
   candidate(planar,2);
   auto predicted=retro::texturePredict(raw,false);if(retro::texturePredict(predicted,true)!=raw)throw std::runtime_error("Texture predictor round-trip failed");candidate(predicted,3);
   candidate(losslessTexture(raw),4);
  }
  verify(packed,chosen);fs::path destination=out/(match[1].str()+".rmz"),temporary=destination.wstring()+L".tmp";std::ofstream asset(temporary,std::ios::binary);
  uint32_t size=uint32_t(chosen.size());asset.write("RMZ1",4);asset.write(reinterpret_cast<char*>(&size),4);asset.write(reinterpret_cast<char*>(&encoding),4);asset.write(reinterpret_cast<char*>(packed.data()),packed.size());
  asset.close();if(!asset)throw std::runtime_error("Cannot write packed resource");publish(temporary,destination);
  Bytes record(12+packed.size());std::memcpy(record.data(),"RMZ1",4);std::memcpy(record.data()+4,&size,4);std::memcpy(record.data()+8,&encoding,4);std::memcpy(record.data()+12,packed.data(),packed.size());entries.push_back({uint32_t(std::stoul(match[1].str())),std::move(record)});
  rc<<match[1].str()<<" RCDATA \""<<destination.generic_string()<<"\"\n";
  report<<match[1].str()<<" "<<match[2].str()<<": "<<original.size()<<" -> "<<packed.size()+12<<" bytes; transform "<<encoding<<"; round-trip PASS\n";
  total+=packed.size()+12;originalTotal+=original.size();
  std::cout<<match[1].str()<<": "<<packed.size()+12<<" bytes, encoding "<<encoding<<std::endl;
 }
 Bytes bundle(8+entries.size()*12);std::memcpy(bundle.data(),"RMA1",4);uint32_t entryCount=uint32_t(entries.size());std::memcpy(bundle.data()+4,&entryCount,4);
 for(size_t i=0;i<entries.size();++i){auto&e=entries[i];uint32_t offset=uint32_t(bundle.size()),size=uint32_t(e.bytes.size());std::memcpy(bundle.data()+8+i*12,&e.id,4);std::memcpy(bundle.data()+12+i*12,&offset,4);std::memcpy(bundle.data()+16+i*12,&size,4);bundle.insert(bundle.end(),e.bytes.begin(),e.bytes.end());}
 {std::ofstream file(argv[3],std::ios::binary);file.write(reinterpret_cast<const char*>(bundle.data()),bundle.size());if(!file)throw std::runtime_error("Cannot write asset bundle");}
 auto hash=retro::assetHash(bundle);std::ofstream header(out/"AssetManifest.h");header<<"#pragma once\n#include <array>\nnamespace retro { inline constexpr unsigned AssetBundleSize="<<bundle.size()<<"; inline constexpr std::array<unsigned char,32> AssetBundleHash={";for(auto b:hash)header<<unsigned(b)<<',';header<<"}; }\n";if(!header)throw std::runtime_error("Cannot write asset manifest");
 report<<"Total: "<<originalTotal<<" -> "<<total<<" bytes\nSeparate bundle: "<<bundle.size()<<" bytes; SHA-256 pinned in executable\n";
 rc.close();report.close();if(!rc||!report)throw std::runtime_error("Cannot write asset manifest/report");
 publish(out/"compression-report.txt.tmp",out/"compression-report.txt");
 // Publish the build output last. An interrupted pack must never leave a
 // truncated RC file that an incremental build mistakes for a finished output.
 publish(out/"assets-packed.rc.tmp",out/"assets-packed.rc");
 std::cout<<"Packed assets: "<<total<<" bytes\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

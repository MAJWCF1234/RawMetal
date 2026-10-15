#include "../core/AnimationPredictor.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>

namespace fs=std::filesystem;
using Bytes=std::vector<unsigned char>;
namespace {
size_t checks=0;
void require(bool condition,const std::string&message){
 ++checks;if(!condition)throw std::runtime_error(message);
}
uint32_t u32(const Bytes&bytes,size_t offset){
 return uint32_t(bytes.at(offset))|(uint32_t(bytes.at(offset+1))<<8)|
        (uint32_t(bytes.at(offset+2))<<16)|(uint32_t(bytes.at(offset+3))<<24);
}
uint16_t u16(const Bytes&bytes,size_t offset){return uint16_t(bytes.at(offset))|(uint16_t(bytes.at(offset+1))<<8);}
void put32(Bytes&bytes,size_t offset,uint32_t value){for(unsigned b=0;b<4;++b)bytes.at(offset+b)=static_cast<unsigned char>(value>>(b*8));}
void put16(Bytes&bytes,size_t offset,uint16_t value){bytes.at(offset)=static_cast<unsigned char>(value);bytes.at(offset+1)=static_cast<unsigned char>(value>>8);}
struct Fixture {Bytes bytes;size_t prefix=0,vertices=0;std::vector<uint32_t> counts;};
Fixture fixture(bool compact,const std::vector<uint32_t>&counts,size_t vertices){
 Fixture value;value.counts=counts;value.vertices=vertices;
 const size_t corners=vertices*3,frames=std::accumulate(counts.begin(),counts.end(),size_t(0));
 value.prefix=20+corners*2+(compact?counts.size()*4:0);
 value.bytes.resize(value.prefix+frames*vertices*6);
 std::copy_n(compact?"RMA3":"RMA2",4,value.bytes.begin());
 put32(value.bytes,4,uint32_t(corners));put32(value.bytes,8,uint32_t(compact?frames:counts.front()));
 put32(value.bytes,12,uint32_t(counts.size()));put32(value.bytes,16,uint32_t(vertices));
 for(size_t corner=0;corner<corners;++corner)put16(value.bytes,20+corner*2,uint16_t((corner*7)%vertices));
 if(compact)for(size_t clip=0;clip<counts.size();++clip)put32(value.bytes,20+corners*2+clip*4,counts[clip]);
 const std::array<uint16_t,10> edges={0,65535,32767,32768,1,65534,255,256,65280,257};
 for(size_t word=0;word<frames*vertices*3;++word){
  uint16_t sample=word<edges.size()?edges[word]:uint16_t(word*1009+word*word*37);
  put16(value.bytes,value.prefix+word*2,sample);
 }
 // A deliberately opaque, odd-sized trailer exercises exact preservation.
 const unsigned char tail[]={'R','M','L','1',0,255,17,0,128,'R','M','D','1',9,0,31,255};
 value.bytes.insert(value.bytes.end(),std::begin(tail),std::end(tail));return value;
}
// Independent oracle: emit each whole plane by walking each vertex's original
// samples, with explicit byte reads and int arithmetic. It does not use the
// production layout parser, output indexing, or decoder.
Bytes reference(const Fixture&value){
 const size_t frames=std::accumulate(value.counts.begin(),value.counts.end(),size_t(0));
 Bytes expected(value.bytes.begin(),value.bytes.begin()+value.prefix);
 for(size_t axis=0;axis<3;++axis)for(unsigned byte=0;byte<2;++byte){
  size_t first=0;
  for(uint32_t count:value.counts){
   for(size_t sample=0;sample<count;++sample)for(size_t vertex=0;vertex<value.vertices;++vertex){
    const size_t offset=value.prefix+((first+sample)*value.vertices+vertex)*6+axis*2;
    const int current=u16(value.bytes,offset);
    const int previous=sample?u16(value.bytes,offset-value.vertices*6):0;
    const uint32_t delta=uint32_t(current-previous+65536)&65535u;
    expected.push_back(static_cast<unsigned char>(delta>>(byte*8)));
   }
   first+=count;
  }
 }
 expected.insert(expected.end(),value.bytes.begin()+value.prefix+frames*value.vertices*6,value.bytes.end());
 return expected;
}
void exercise(const Fixture&value,const std::string&label){
 const auto expected=reference(value),encoded=retro::animationPredict(value.bytes,false);
 require(encoded==expected,label+": encoder differs from independent plane oracle");
 require(retro::animationPredict(expected,true)==value.bytes,label+": decoder differs from original bytes");
 require(retro::animationPredict(encoded,true)==value.bytes,label+": round trip changed bytes");
 require(encoded.size()==value.bytes.size(),label+": byte count changed");
 require(std::equal(value.bytes.begin(),value.bytes.begin()+value.prefix,encoded.begin()),label+": prefix changed");
 const size_t frames=std::accumulate(value.counts.begin(),value.counts.end(),size_t(0));
 const size_t tail=value.prefix+frames*value.vertices*6;
 require(std::equal(value.bytes.begin()+tail,value.bytes.end(),encoded.begin()+tail),label+": trailer changed");
}
void rejects(const Bytes&bytes,const std::string&label){
 retro::AnimationPredictionLayout layout;
 require(!retro::animationPredictionLayout(bytes,layout),label+": invalid layout accepted");
 for(bool decode:{false,true}){
  bool threw=false;try{(void)retro::animationPredict(bytes,decode);}catch(const std::runtime_error&){threw=true;}
  require(threw,label+": invalid input did not throw");
 }
}
void synthetic(){
 auto known=fixture(true,{2,2},1);
 const std::array<uint16_t,12> positions={0,32767,65535,65535,32768,0,32768,65535,0,32767,0,65535};
 for(size_t word=0;word<positions.size();++word)put16(known.bytes,known.prefix+word*2,positions[word]);
 const Bytes planes={0,255,0,255, 0,255,128,255, 255,1,255,1, 127,0,255,0, 255,1,0,255, 255,0,0,255};
 auto encoded=retro::animationPredict(known.bytes,false);
 require(std::equal(planes.begin(),planes.end(),encoded.begin()+known.prefix),"known six byte planes / wraparound / clip reset");
 exercise(known,"known RMA3");
 exercise(fixture(true,{2,4,3},3),"unequal clips / multiple vertices");
 exercise(fixture(false,{2,2},3),"RMA2 fixed counts");
 exercise(fixture(true,{512},2),"maximum samples per clip");
 exercise(fixture(true,std::vector<uint32_t>(32,2),1),"maximum clip count");
 exercise(fixture(true,{2},65535),"maximum vertex count");
 auto base=fixture(true,{2,4,3},3);
 const size_t countTable=20+base.vertices*3*2;
 for(size_t size:{size_t(0),size_t(3),size_t(19),size_t(20),countTable+11}){
  auto bytes=base.bytes;bytes.resize(size);rejects(bytes,"truncated header/index/sample table "+std::to_string(size));
 }
 auto bytes=base.bytes;bytes[0]='X';rejects(bytes,"unknown magic");
 for(const auto&change:std::vector<std::pair<size_t,uint32_t>>{{4,0},{4,1000001},{4,0xffffffffu},{8,0},{8,10},{8,0xffffffffu},{12,0},{12,33},{16,0},{16,65536},{16,0xffffffffu},{countTable,0},{countTable,1},{countTable,513}}){
  bytes=base.bytes;put32(bytes,change.first,change.second);rejects(bytes,"invalid field "+std::to_string(change.first)+"="+std::to_string(change.second));
 }
 const size_t payload=9*base.vertices*6;
 bytes=base.bytes;bytes.resize(base.prefix+payload-1);rejects(bytes,"truncated quantized payload");
 bytes=base.bytes;bytes.resize(base.prefix);rejects(bytes,"missing quantized payload");
 auto fixed=fixture(false,{2,2},3);
 for(uint32_t count:{0u,1u,513u,0xffffffffu}){bytes=fixed.bytes;put32(bytes,8,count);rejects(bytes,"RMA2 invalid fixed count");}
 // Maximum allowed dimensions require over 6 GiB, so a tiny stream must fail
 // before touching payload memory or allocating a predicted output buffer.
 bytes=fixture(true,std::vector<uint32_t>(32,512),1).bytes;
 put32(bytes,16,65535);bytes.resize(20+6+32*4);rejects(bytes,"oversized payload dimensions");
}
Fixture bank(const fs::path&path){
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read "+path.string());
 Fixture value;value.bytes.assign(std::istreambuf_iterator<char>(file),{});
 require(value.bytes.size()>=20,"source bank header is truncated: "+path.string());
 const bool compact=std::equal(value.bytes.begin(),value.bytes.begin()+4,"RMA3");
 require(compact||std::equal(value.bytes.begin(),value.bytes.begin()+4,"RMA2"),"source bank magic: "+path.string());
 const size_t corners=u32(value.bytes,4),clips=u32(value.bytes,12);
 value.vertices=u32(value.bytes,16);value.prefix=20+corners*2+(compact?clips*4:0);
 for(size_t clip=0;clip<clips;++clip)value.counts.push_back(compact?u32(value.bytes,20+corners*2+clip*4):u32(value.bytes,8));
 return value;
}
}
int main(int argc,char**argv){try{
 synthetic();
 fs::path source=argc>1?fs::path(argv[1]):fs::path(RAWMETAL_SOURCE_DIRECTORY);
 if(fs::is_directory(source/"src/assets"))source/= "src";
 if(!fs::is_directory(source/"assets"))throw std::runtime_error("Expected repository root or src directory: "+source.string());
 std::vector<fs::path> banks;
 for(const auto&entry:fs::recursive_directory_iterator(source/"assets")){
  if(!entry.is_regular_file()||entry.path().extension()!=".bin")continue;
  std::ifstream file(entry.path(),std::ios::binary);char magic[4]{};file.read(magic,4);
  if(file.gcount()==4&&(!std::memcmp(magic,"RMA2",4)||!std::memcmp(magic,"RMA3",4)))banks.push_back(entry.path());
 }
 std::sort(banks.begin(),banks.end());require(!banks.empty(),"No real animation banks found");
 for(const auto&path:banks){auto value=bank(path);exercise(value,path.filename().string());std::cout<<"PASS "<<fs::relative(path,source).generic_string()<<" ("<<value.bytes.size()<<" bytes)\n";}
 std::cout<<"Animation codec: "<<checks<<" checks passed; "<<banks.size()<<" source banks matched independent planes and full bytes\n";return 0;
 }catch(const std::exception&error){std::cerr<<"Animation codec test failed: "<<error.what()<<'\n';return 1;}}

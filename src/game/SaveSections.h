#pragma once
// The save container (format 28) and its section API. docs/Systems/save-format.md has the whole story.
//
// A save is a text header line, `RAWMETAL_SAVE <format> <checksum>`, then a run of sections. Each
// section is framed as
//     u8 name length, name (ASCII), u16 section version, u16 flags, u32 body length, u32 body checksum
// (little-endian) and then its body. A body is written with SaveOut and read with SaveIn: compact,
// little-endian and deterministic (the same game always writes the same bytes).
//
// A reader reads the sections it knows in its own registration order, whatever order the file has
// them in, and skips the rest: a later build can add a section and an earlier build of the same
// format still loads the save. A section flagged Required (in the file) makes a build that does
// not know it refuse the save instead.
//
// Any system adds its own section with Game::registerSaveSection (see SaveSection below).
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace retro {
class Game;
struct Clutter;

// A named value a script, the map or a system sets at runtime: a flag, a whole number, a real
// number or text. Real numbers must be finite.
using SaveValue=std::variant<bool,std::int64_t,double,std::string>;

// One section body being written.
struct SaveOut {
 static constexpr bool reading=false;
 std::string bytes;
 // A number a load would refuse (NaN, infinity, past ten million) is written as the nearest it
 // accepts, and counted: one glitched value must not make the whole save unloadable.
 int repaired=0;
 void byte(std::uint8_t b){bytes.push_back(char(b));}
 void varint(std::uint64_t v){while(v>=0x80){byte(std::uint8_t(v|0x80));v>>=7;}byte(std::uint8_t(v));}
 void fixed(const void* data,size_t size){bytes.append(static_cast<const char*>(data),size);} // little-endian hosts only (x64)
 template<class T> void one(T& v){
  if constexpr(std::is_same_v<T,bool>)byte(v?1:0);
  else if constexpr(std::is_enum_v<T>){const std::int64_t n=std::int64_t(v);varint(std::uint64_t((n<<1)^(n>>63)));}
  else if constexpr(std::is_floating_point_v<T>){T out=v;if(!std::isfinite(double(out))){out=0;++repaired;}else if(std::fabs(double(out))>9999999.){out=out<0?T(-9999999):T(9999999);++repaired;}fixed(&out,sizeof out);}
  else if constexpr(std::is_signed_v<T>){const std::int64_t n=std::int64_t(v);varint(std::uint64_t((n<<1)^(n>>63)));}
  else varint(std::uint64_t(v));
 }
 template<class... T> void operator()(T&... v){(one(v),...);}
 void text(std::string& s){varint(s.size());bytes.append(s);}
 // A store value: its kind, then the value. A real number is written whole (8 bytes).
 void value(SaveValue& v){byte(std::uint8_t(v.index()));
  if(auto* b=std::get_if<bool>(&v))one(*b);else if(auto* i=std::get_if<std::int64_t>(&v))one(*i);
  else if(auto* d=std::get_if<double>(&v)){double x=std::isfinite(*d)?*d:0;if(!std::isfinite(*d))++repaired;fixed(&x,sizeof x);}
  else text(std::get<std::string>(v));}
};

// One section body being read. Every value is checked; a bad one throws with the reason.
struct SaveIn {
 static constexpr bool reading=true;
 const unsigned char* at=nullptr;const unsigned char* end=nullptr;
 SaveIn(const char* data,size_t size):at(reinterpret_cast<const unsigned char*>(data)),end(at+size){}
 size_t left()const{return size_t(end-at);}
 [[noreturn]] static void fail(const std::string& why){throw std::runtime_error(why);}
 std::uint8_t byte(){if(at>=end)fail("section ends early");return *at++;}
 std::uint64_t varint(){std::uint64_t v=0;for(int shift=0;shift<64;shift+=7){const auto b=byte();v|=std::uint64_t(b&0x7f)<<shift;if(!(b&0x80)){if(shift==63&&b>1)break;return v;}}fail("invalid number encoding");}
 std::int64_t signedVarint(){const auto u=varint();return std::int64_t(u>>1)^-std::int64_t(u&1);}
 void fixed(void* data,size_t size){if(left()<size)fail("section ends early");std::memcpy(data,at,size);at+=size;}
 template<class T> void one(T& v){
  if constexpr(std::is_same_v<T,bool>){const auto n=byte();if(n>1)fail("invalid flag "+std::to_string(n));v=n!=0;}
  else if constexpr(std::is_enum_v<T>){const auto n=signedVarint();if(n<0||n>16)fail("invalid enum value "+std::to_string(n));v=static_cast<T>(n);}
  else if constexpr(std::is_floating_point_v<T>){fixed(&v,sizeof v);if(!std::isfinite(double(v))||std::fabs(double(v))>10000000)fail("invalid number");}
  else if constexpr(std::is_signed_v<T>){const auto n=signedVarint();if(n<std::int64_t(std::numeric_limits<T>::min())||n>std::int64_t(std::numeric_limits<T>::max()))fail("invalid integer");v=T(n);}
  else{const auto n=varint();if(n>std::uint64_t(std::numeric_limits<T>::max()))fail("invalid integer");v=T(n);}
 }
 template<class... T> void operator()(T&... v){(one(v),...);}
 void text(std::string& s,size_t limit=4096){const auto n=varint();if(n>limit||n>left())fail("invalid text length");s.assign(reinterpret_cast<const char*>(at),size_t(n));at+=n;}
 void value(SaveValue& v,size_t limit=4096){switch(byte()){
  case 0:{bool b=false;one(b);v=b;break;}
  case 1:{std::int64_t i=0;one(i);v=i;break;}
  case 2:{double d=0;fixed(&d,sizeof d);if(!std::isfinite(d))fail("invalid number");v=d;break;}
  case 3:{std::string s;text(s,limit);v=std::move(s);break;}
  default:fail("invalid value kind");}}
};

// A save section: what a system keeps in a save, under its own name.
//  - `name`: its id in the file (1-48 characters: letters, digits, '.', '-', '_'). Never reuse a
//    retired name for different content.
//  - `version`: the newest layout this build writes. Raise it when the layout changes and read the
//    older layouts by `version` in `read`; `upgrade` then brings what an older layout lacked up to
//    date. A save whose section is newer than this build knows is handled as an unknown section.
//  - `core`: a save without it is refused (reader side). Off: `absent` runs instead.
//  - `required`: written into the file. A build that does not know this section (or not this
//    version of it) refuses the save rather than load it without it. Leave it off when an older
//    build may safely load the save without the section.
//  - `write` writes the body; a body left empty is not written at all (unless `core`).
//  - `read` reads a body of layout `version` (1..this build's). It throws std::runtime_error with
//    the reason on a bad value: the load is refused with "section 'name': reason".
// Sections are read after the core sections they follow in registration order, so a section
// registered after "props" sees every chunk's props (Game::chunkClutter) when it is read; the
// authored-map reconciliation runs after every section has been read.
struct SaveSection {
 std::string name;
 std::uint16_t version=1;
 bool core=false,required=false;
 std::function<void(Game&,SaveOut&)> write;
 std::function<void(Game&,SaveIn&,int version)> read;
 std::function<void(Game&,int fromVersion)> upgrade;
 std::function<void(Game&)> absent;
};

// Values a save keeps by name, for the map and script systems. `chunk` is the map (chunk index)
// they belong to; -1 is the whole campaign. Names are 1-128 bytes, text values up to 4096 bytes.
struct SaveStore {
 static constexpr size_t MaxEntities=65536,MaxProperties=256,MaxVariables=65536,MaxName=128,MaxText=4096;
 using Key=std::pair<int,std::string>;
 using Properties=std::map<std::string,SaveValue,std::less<>>;
 // Runtime changes to an entity's properties: per (map, entity name or id key), property -> value.
 std::map<Key,Properties> entities;
 // Named values scripts set: per (map or -1, name).
 std::map<Key,SaveValue> variables;
 bool operator==(const SaveStore&)const=default;
};

// The rigid-body solver's state for one prop, as the "physics" section keeps it.
struct PropBody {
 std::array<float,4> orientation{0,0,0,1}; // quaternion x, y, z, w (unit)
 std::array<float,3> angularVelocity{};    // radians per second, world axes
 std::int32_t island=-1;                    // sleep island the prop rests in; -1 awake / none
 bool operator==(const PropBody&)const=default;
};
// How the "physics" section reads and writes props. `capture` fills `body` for a prop that has
// solver state and returns true; `restore` puts it back onto the loaded prop (it runs before the
// authored-map reconciliation, which keeps the prop's fields). Both null: the section is not
// written and a saved one is read, checked and dropped. The rigid-body solver sets these.
struct PhysicsSaveHooks {
 bool(*capture)(const Clutter& prop,PropBody& body)=nullptr;
 void(*restore)(Clutter& prop,const PropBody& body)=nullptr;
};
PhysicsSaveHooks& physicsSaveHooks();
}

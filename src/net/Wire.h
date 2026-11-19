#pragma once
// Bytes on the wire for the Match (Match.cpp): little-endian, fixed-point where a float would waste bytes.
// A Reader never reads past its end: a short or garbled packet sets bad and reads zeros from then on.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace retro::net::wire {

struct Writer{
 std::vector<std::uint8_t> b;
 Writer& u8(unsigned v){b.push_back(std::uint8_t(v&255u));return *this;}
 Writer& u16(unsigned v){u8(v);u8(v>>8);return *this;}
 Writer& u32(std::uint32_t v){u16(v&0xFFFFu);u16(v>>16);return *this;}
 Writer& i8(int v){return u8(unsigned(std::uint8_t(std::int8_t(std::clamp(v,-127,127)))));}
 Writer& i16(int v){return u16(unsigned(std::uint16_t(std::int16_t(std::clamp(v,-32767,32767)))));}
 Writer& f32(float v){std::uint32_t x=0;std::memcpy(&x,&v,4);return u32(x);}
 Writer& str(const std::string& s){const std::size_t n=std::min<std::size_t>(s.size(),255);u8(unsigned(n));b.insert(b.end(),s.begin(),s.begin()+long(n));return *this;}
 Writer& bytes(const std::uint8_t* p,std::size_t n){b.insert(b.end(),p,p+n);return *this;}
 // Fixed point: v*scale rounded, clamped to the field.
 Writer& q8(float v,float scale){return i8(int(std::lround(std::isfinite(v)?v*scale:0.f)));}
 Writer& q16(float v,float scale){return i16(int(std::lround(std::isfinite(v)?std::clamp(v*scale,-32767.f,32767.f):0.f)));}
 Writer& unit8(float v){return u8(unsigned(std::lround(std::clamp(std::isfinite(v)?v:0.f,0.f,1.f)*255.f)));}
 // An angle in radians, any turn, as 1/65536 of a turn.
 Writer& angle(float radians){const float turn=std::isfinite(radians)?radians/6.2831853f:0.f;return u16(unsigned(std::lround((turn-std::floor(turn))*65536.f))&0xFFFFu);}
};

struct Reader{
 const std::uint8_t* p=nullptr;std::size_t n=0,i=0;bool bad=false;
 Reader(const std::uint8_t* data,std::size_t size):p(data),n(size){}
 explicit Reader(const std::vector<std::uint8_t>& v):p(v.data()),n(v.size()){}
 bool need(std::size_t k){if(bad||i+k>n){bad=true;return false;}return true;}
 std::size_t left()const{return bad?0:n-i;}
 unsigned u8(){if(!need(1))return 0;return p[i++];}
 unsigned u16(){const unsigned a=u8();const unsigned b=u8();return a|(b<<8);}
 std::uint32_t u32(){const std::uint32_t a=u16();const std::uint32_t b=u16();return a|(b<<16);}
 int i8(){return int(std::int8_t(std::uint8_t(u8())));}
 int i16(){return int(std::int16_t(std::uint16_t(u16())));}
 float f32(){const std::uint32_t x=u32();float v=0;std::memcpy(&v,&x,4);return std::isfinite(v)?v:0.f;}
 std::string str(){const unsigned k=u8();if(!need(k))return {};std::string s(reinterpret_cast<const char*>(p+i),k);i+=k;return s;}
 float q8(float scale){return float(i8())/scale;}
 float q16(float scale){return float(i16())/scale;}
 float unit8(){return float(u8())/255.f;}
 float angle(){return float(u16())/65536.f*6.2831853f;}
};

}

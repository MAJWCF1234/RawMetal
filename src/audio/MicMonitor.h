#pragma once
// TEST MY MIC on the COMMUNICATION page (docs/Systems/multiplayer.md "Chat and voice"): listens on a microphone with
// Windows waveIn, only while the test is on, and turns what it hears into the same 0-1 level the voice room's meter
// uses (micLevelOf). Nothing is recorded or sent. No microphone: open() is false and error() says so.
#include <cstdint>
#include <string>
#include <vector>
namespace retro {
// A block of 16-bit samples as a 0-1 meter reading: its peak on a 60 dB scale (-60 dBFS and below reads 0).
float micLevelOf(const std::int16_t* samples,std::size_t count);
class MicMonitor{
public:
 explicit MicMonitor(const std::string& deviceName);   // "" the system default; else the first device named like it
 ~MicMonitor();
 MicMonitor(const MicMonitor&)=delete;MicMonitor& operator=(const MicMonitor&)=delete;
 bool open()const{return m_device!=nullptr;}
 const std::string& error()const{return m_error;}
 const std::string& deviceName()const{return m_name;}  // what it asked for
 void poll();                                          // collect what was heard since the last call (once a frame)
 float level()const{return m_level;}                   // 0-1, falls back over a fraction of a second
private:
 void* m_device=nullptr;std::string m_name,m_error;float m_level=0;
 struct Buffer;std::vector<Buffer*> m_buffers;
};
}

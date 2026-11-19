#include "MicMonitor.h"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace retro {
float micLevelOf(const std::int16_t* samples,std::size_t count){
 int peak=0;for(std::size_t i=0;i<count;++i)peak=std::max(peak,std::abs(int(samples[i])));
 if(peak<=0)return 0;
 const float db=20.f*std::log10(float(peak)/32768.f);
 return std::clamp((db+60.f)/60.f,0.f,1.f);
}
struct MicMonitor::Buffer{WAVEHDR header{};std::vector<std::int16_t> samples;};
namespace {
constexpr int Rate=16000,BufferSamples=800,BufferCount=4;   // 50 ms blocks
std::string narrow(const wchar_t* w){std::string s;for(;*w;++w)s+=*w<128?char(*w):'?';return s;}
}
MicMonitor::MicMonitor(const std::string& deviceName):m_name(deviceName){
 const UINT count=waveInGetNumDevs();
 if(count==0){m_error="No microphone is connected";return;}
 UINT id=WAVE_MAPPER;
 if(!deviceName.empty())for(UINT i=0;i<count;++i){WAVEINCAPSW caps{};if(waveInGetDevCapsW(i,&caps,sizeof caps)!=MMSYSERR_NOERROR)continue;
  const std::string name=narrow(caps.szPname);   // waveIn cuts names at 31 characters
  if(!name.empty()&&deviceName.compare(0,name.size(),name)==0){id=i;break;}}
 WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=Rate;format.wBitsPerSample=16;
 format.nBlockAlign=2;format.nAvgBytesPerSec=Rate*2;
 HWAVEIN device=nullptr;
 if(waveInOpen(&device,id,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR||!device){m_error="The microphone could not be opened";return;}
 m_device=device;
 for(int i=0;i<BufferCount;++i){auto* b=new Buffer;b->samples.assign(BufferSamples,0);b->header.lpData=reinterpret_cast<LPSTR>(b->samples.data());b->header.dwBufferLength=BufferSamples*2;
  if(waveInPrepareHeader(device,&b->header,sizeof b->header)==MMSYSERR_NOERROR)waveInAddBuffer(device,&b->header,sizeof b->header);
  m_buffers.push_back(b);}
 if(waveInStart(device)!=MMSYSERR_NOERROR){m_error="The microphone would not start";}
}
MicMonitor::~MicMonitor(){
 if(m_device){const auto device=static_cast<HWAVEIN>(m_device);waveInReset(device);
  for(auto* b:m_buffers){if(b->header.dwFlags&WHDR_PREPARED)waveInUnprepareHeader(device,&b->header,sizeof b->header);}
  waveInClose(device);}
 for(auto* b:m_buffers)delete b;
}
void MicMonitor::poll(){
 m_level=std::max(0.f,m_level-.06f);
 if(!m_device)return;
 const auto device=static_cast<HWAVEIN>(m_device);
 for(auto* b:m_buffers)if(b->header.dwFlags&WHDR_DONE){
  m_level=std::max(m_level,micLevelOf(b->samples.data(),std::min<std::size_t>(b->header.dwBytesRecorded/2,b->samples.size())));
  b->header.dwFlags&=~WHDR_DONE;b->header.dwBytesRecorded=0;waveInAddBuffer(device,&b->header,sizeof b->header);}
}
}

// Use the already bundled miniaudio MP3 decoder; playback remains in our mixer.
#define MA_NO_DEVICE_IO
#define MA_NO_ENCODING
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MINIAUDIO_IMPLEMENTATION
#include "../ThirdParty/miniaudio/miniaudio.h"
#include <vector>
#include <cstdint>
#include <stdexcept>
namespace retro {
std::vector<int16_t> decodeMusic(const unsigned char* data,size_t bytes){
 auto config=ma_decoder_config_init(ma_format_s16,2,44100);ma_uint64 frames=0;void* pcm=nullptr;
 if(ma_decode_memory(data,bytes,&config,&frames,&pcm)!=MA_SUCCESS||!pcm||!frames)throw std::runtime_error("Invalid compressed music resource");
 std::vector<int16_t> samples(static_cast<int16_t*>(pcm),static_cast<int16_t*>(pcm)+frames*2);ma_free(pcm,nullptr);return samples;
}
}

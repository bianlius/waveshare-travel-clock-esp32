// Passport's integer decoder avoids software floating-point work on ESP32-C3.
// The Waveshare target keeps its existing minimp3 implementation.
extern "C" {
#include "mp3_player.h"
}
#include "micro_mp3/mp3_decoder.h"
#include <atomic>

namespace {
micro_mp3::Mp3Decoder decoder;
const uint8_t *file_start;
size_t file_size, byte_position;
int16_t pcm[micro_mp3::MP3_MIN_OUTPUT_BUFFER_BYTES / sizeof(int16_t)];
size_t pcm_count, pcm_position;
std::atomic<bool> active{false};
bool looping;

bool next_frame() {
    unsigned wraps=0;
    // Bound corrupt-input recovery, including the decoder's metadata probe.
    for(unsigned attempt=0;attempt<16;++attempt) {
        if(byte_position>=file_size) {
            if(!looping || wraps++)return false;
            byte_position=0;decoder.reset();
        }
        size_t consumed=0,samples=0;
        const size_t available=file_size-byte_position;
        auto result=decoder.decode(file_start+byte_position,available,
            reinterpret_cast<uint8_t *>(pcm),sizeof(pcm),consumed,samples);
        if(consumed>available)return false;
        byte_position+=consumed;
        if(result==micro_mp3::MP3_STREAM_INFO_READY ||
           result==micro_mp3::MP3_STREAM_INFO_CHANGED) {
            if(decoder.get_channels()!=1 || decoder.get_sample_rate()!=12000)return false;
            continue;
        }
        if(result<0 && result!=micro_mp3::MP3_DECODE_ERROR)return false;
        if(samples) {
            if(decoder.get_channels()!=1 || decoder.get_sample_rate()!=12000 ||
               samples>sizeof(pcm)/sizeof(pcm[0]))return false;
            pcm_count=samples;pcm_position=0;return true;
        }
        if(!consumed)return false;
    }
    return false;
}
bool start(const uint8_t *file,size_t size,bool repeat) {
    mp3_player_stop();
    if(!file || size<4 || file[0]!=0xff || (file[1]&0xe0)!=0xe0 ||
       (file[1]&0x06)!=0x02)return false;
    file_start=file;file_size=size;looping=repeat;
    active.store(true);return true;
}
}

void mp3_player_stop(void) {
    active.store(false);decoder.reset();file_start=nullptr;
    file_size=byte_position=pcm_count=pcm_position=0;
}
bool mp3_player_start(const uint8_t *file,size_t size){return start(file,size,true);}
bool mp3_player_start_once(const uint8_t *file,size_t size){return start(file,size,false);}
bool mp3_player_is_active(void){return active.load();}
int16_t mp3_player_render(uint8_t volume) {
    if(!active.load())return 0;
    if(pcm_position>=pcm_count && !next_frame()) {mp3_player_stop();return 0;}
    return static_cast<int16_t>(static_cast<int32_t>(pcm[pcm_position++])*volume/100);
}

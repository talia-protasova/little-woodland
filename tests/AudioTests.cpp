#include "audio/Audio.h"
#include "miniaudio.h"
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
using namespace woodland;
// Generate tiny PCM16/extensible-PCM24 fixtures locally so public-source tests
// exercise decoding even when none of the Git-ignored Mixkit WAVs are installed.
void syntheticWav(bool extensible) {
    const auto file=std::filesystem::temp_directory_path()/(std::string(extensible?"woodland-test-pcm24-":"woodland-test-pcm16-")+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".wav");
    std::vector<unsigned char> bytes;
    const auto text=[&](const char* str){for(int i=0;i<4;++i)bytes.push_back(str[i]);};
    const auto word=[&](unsigned value,int n){for(int i=0;i<n;++i)bytes.push_back((value>>(i*8))&255);};
    const int width=extensible?3:2, formatSize=extensible?40:16;
    text("RIFF");word(4+8+formatSize+8+width*4,4);text("WAVE");text("fmt ");word(formatSize,4);
    word(extensible?65534:1,2);word(2,2);word(44100,4);word(44100*width*2,4);word(width*2,2);word(width*8,2);
    // The extensible format uses a PCM subformat GUID and stereo channel mask;
    // a legacy-format decoder alone would miss this supplied delivery-file format.
    if(extensible){word(22,2);word(24,2);word(3,4);word(1,4);word(0,2);word(0x10,2);word(0x80,2);word(0x00aa00,3);word(0x719b38,3);}
    text("data");word(width*4,4);
    // Include positive full-scale, zero, negative full-scale and a half-scale
    // sample. Signed conversion failures are detectable without listening.
    word(extensible?8388607:32767,width);word(0,width);word(extensible?0x800000:0x8000,width);word(extensible?4194304:16384,width);
    {std::ofstream output(file,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    auto config=ma_decoder_config_init(ma_format_f32,2,44100);ma_uint64 frames=0;float* raw=nullptr;
    const auto status=ma_decode_file(file.string().c_str(),&config,&frames,reinterpret_cast<void**>(&raw));
    std::filesystem::remove(file);
    std::unique_ptr<float,void(*)(float*)> data(raw,[](float*p){ma_free(p,nullptr);});
    if(status!=MA_SUCCESS || frames!=2 || std::abs(data.get()[0]-1)>0.001f || std::abs(data.get()[2]+1)>0.001f)
        throw std::runtime_error("Synthetic WAV PCM16/extensible PCM24 decoding failed");
}
/**
 * Checks synthetic decoder fixtures on every run, and additionally checks any
 * local optional WAVs for finite, nonzero decoded samples. No downloaded sound is
 * required by public-source tests. An extra argument enables device probing and
 * mute checks, whose success still does not constitute an audible listening test.
 */
int main(int argc,char**){try{
    syntheticWav(false);syntheticWav(true);
    int checked=0;
    const auto assets=std::filesystem::path(WOODLAND_ASSET_DIR);
    // Existing optional clips get an additional real-file decode/finite-sample
    // check. Absence is expected in a public checkout and must not fail the build.
    for(const auto* name:{"tile_click.wav","success_tile_match.wav","tiles_falling.wav","letter_delivered.wav","victory.wav"}){
        const auto file=assets/"audio/mixkit"/name;if(!std::filesystem::exists(file))continue;
        auto config=ma_decoder_config_init(ma_format_f32,2,44100);ma_uint64 frames=0;float* raw=nullptr;
        if(ma_decode_file(file.string().c_str(),&config,&frames,reinterpret_cast<void**>(&raw))!=MA_SUCCESS)
            throw std::runtime_error(std::string("WAV decode failed: ")+name);
        std::unique_ptr<float,void(*)(float*)> data(raw,[](float*p){ma_free(p,nullptr);});
        if(frames==0)throw std::runtime_error("Empty WAV");
        bool nonzero=false;for(ma_uint64 i=0;i<frames*2;++i){if(!std::isfinite(data.get()[i]))throw std::runtime_error("Non-finite WAV samples");nonzero|=data.get()[i]!=0;}
        if(!nonzero)throw std::runtime_error("Silent source WAV");++checked;
    }
    // A deliberately absent tree must produce a safe silent Audio object even
    // when the machine happens to have a working default playback device.
    Audio missing(assets/"not-present");if(missing.status()!=SoundStatus::Unavailable)throw std::runtime_error("Missing audio not graceful");
    missing.toggleMute();missing.play(SoundCue::Victory);missing.stop();
    // Device probing is opt-in; a headless test normally needs no audio hardware.
    // A null sink can exercise the API path, but cannot establish audible playback.
    if(argc>1){
        Audio device(assets);
        if(device.status()==SoundStatus::On){
            device.play(SoundCue::Selection);device.toggleMute();
            if(device.status()!=SoundStatus::Muted)throw std::runtime_error("Device mute failed");
            device.stop();if(device.status()!=SoundStatus::Muted)throw std::runtime_error("Stop reset mute");
            device.toggleMute();if(device.status()!=SoundStatus::On)throw std::runtime_error("Unmute failed");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout<<"Audio device initialized; mute/stop/unmute passed (not a listening test)\n";
        }else std::cout<<"No audio device; graceful silent fallback passed\n";
    }
    std::cout<<checked<<" optional WAVs decoded (including extensible PCM24 when present); missing assets handled\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

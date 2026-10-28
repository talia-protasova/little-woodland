#include "Audio.h"
#include "Mixer.h"
#include "miniaudio.h"
#include <iostream>
#include <mutex>

namespace woodland {
/**
 * Keeps device details out of the public header. The mutex protects voice changes
 * between the main thread and device callback; clips are loaded before startup.
 * The destructor stops/uninitializes the device while mixer and mutex still exist.
 */
struct Audio::Impl {
    Mixer mixer;
    std::mutex mutex;
    ma_device device{};
    bool ready=false;
    ~Impl(){ if(ready)ma_device_uninit(&device); }
};
Audio::Audio(const std::filesystem::path& assets) : impl_(std::make_unique<Impl>()) {
    const std::array<const char*,5> names{{"tile_click.wav","success_tile_match.wav","tiles_falling.wav","letter_delivered.wav","victory.wav"}};
    bool any=false;
    // Missing clips are independent optional failures, not a reason to discard
    // clips that did load. Decode to one mixer format before starting any callback.
    for(std::size_t i=0;i<names.size();++i) {
        const auto path=assets/"audio/mixkit"/names[i];
        ma_uint64 frames=0;
        float* data=nullptr;
        auto config=ma_decoder_config_init(ma_format_f32,2,44100);
        const auto result=ma_decode_file(path.string().c_str(),&config,&frames,reinterpret_cast<void**>(&data));
        if(result!=MA_SUCCESS) {std::clog<<"Optional sound unavailable: "<<path<<" ("<<ma_result_description(result)<<")\n";continue;}
        // miniaudio owns the decode allocation until explicitly freed. Copy into
        // persistent vectors while an RAII guard covers both success and exceptions.
        std::unique_ptr<float,void(*)(float*)> owned(data,[](float* p){ma_free(p,nullptr);});
        impl_->mixer.clips[i].assign(data,data+frames*2);
        any|=frames>0;
    }
    if(!any){std::clog<<"Audio disabled: no optional WAV files loaded\n";return;}
    auto config=ma_device_config_init(ma_device_type_playback);
    config.playback.format=ma_format_f32;
    config.playback.channels=2;
    config.sampleRate=44100;
    config.pUserData=impl_.get();
    // The backend requests output on its own audio thread. No file access or new
    // sample buffers are needed here; main-thread play/mute/stop uses the same mutex.
    config.dataCallback=[](ma_device* device,void* output,const void*,ma_uint32 frames) {
        auto& state=*static_cast<Impl*>(device->pUserData);
        std::lock_guard<std::mutex> lock(state.mutex);
        state.mixer.render(static_cast<float*>(output),frames);
    };
    // Do not silently select miniaudio's null backend: the UI must report no device.
    const ma_backend backends[]={ma_backend_pulseaudio,ma_backend_alsa};
    auto result=ma_device_init_ex(backends,2,nullptr,&config,&impl_->device);
    if(result!=MA_SUCCESS){std::clog<<"Audio disabled: no supported device ("<<ma_result_description(result)<<")\n";return;}
    // An initialized device that cannot start must be released immediately. ready
    // is set only after success, so Impl will not uninitialize that failure twice.
    result=ma_device_start(&impl_->device);
    if(result!=MA_SUCCESS){ma_device_uninit(&impl_->device);std::clog<<"Audio disabled: device start failed\n";return;}
    impl_->ready=true;
}
Audio::~Audio()=default;
void Audio::play(SoundCue cue){if(!impl_->ready)return;std::lock_guard<std::mutex> lock(impl_->mutex);impl_->mixer.play(cue);}
void Audio::stop(){std::lock_guard<std::mutex> lock(impl_->mutex);impl_->mixer.stop();}
void Audio::toggleMute(){std::lock_guard<std::mutex> lock(impl_->mutex);impl_->mixer.mute(!impl_->mixer.muted());}
SoundStatus Audio::status()const {return !impl_->ready?SoundStatus::Unavailable:impl_->mixer.muted()?SoundStatus::Muted:SoundStatus::On;}
}

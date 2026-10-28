#pragma once
#include "presentation/Feedback.h"
#include <algorithm>
#include <array>
#include <vector>

namespace woodland {
/**
 * Mixes cached, interleaved stereo float clips into a fixed eight-voice pool.
 *
 * Owns sample vectors and voice cursors, but no thread, file or audio device. The
 * caller must serialize play/render/mute/stop access and finish loading clips before
 * starting callbacks. Clip indices match SoundCue values; sample vectors must hold
 * complete stereo frames and remain unchanged while voices reference them.
 */
class Mixer {
public:
    using Samples = std::vector<float>;
    /// Persistent decoded samples indexed by SoundCue; empty clips are optional silence.
    std::array<Samples,5> clips;
    /**
     * Drops active cursors without touching cached samples, serial ordering or mute.
     */
    void stop() { for(auto& voice:voices_) voice.active=false; }
    /**
     * Changes mute and discards voices when entering silence, preventing stale tails
     * from resuming when the user enables sound again.
     */
    void mute(bool value) { muted_=value; if(value) stop(); }
    bool muted() const { return muted_; }
    /**
     * Starts an effect at sample zero, subject to two voices per cue and eight total.
     * Victory clears all prior voices; delivery clears match/fall tails. A fresh event
     * can replace an older instance, but repeated rendering never restarts a voice.
     * @param cue Valid semantic clip index supplied by the presentation layer.
     */
    void play(SoundCue cue) {
        const auto index=static_cast<std::size_t>(cue);
        if(muted_ || clips[index].empty()) return;
        // Objective feedback takes precedence over noisy cascade tails; victory
        // begins cleanly, while delivery specifically removes match/fall competition.
        if(cue==SoundCue::Victory) stop();
        else if(cue==SoundCue::Delivery)
            for(auto& voice:voices_) if(voice.cue==SoundCue::Falling || voice.cue==SoundCue::Match) voice.active=false;
        // At most two voices per cue; the oldest instance yields to the new event.
        Voice* slot=nullptr;
        int same=0;
        for(auto& voice:voices_) {
            if(voice.active && voice.cue==cue) { ++same; if(!slot || voice.serial<slot->serial)slot=&voice; }
        }
        if(same<2) for(auto& voice:voices_) if(!voice.active) {slot=&voice;break;}
        if(!slot) slot=&*std::min_element(voices_.begin(),voices_.end(),[](auto&a,auto&b){return a.serial<b.serial;});
        *slot={cue,0,true,++serial_};
    }
    /**
     * Overwrites the device buffer with mixed stereo samples, advancing each cursor
     * and retiring voices at the end of their clip. Applies per-cue gains, a master
     * gain and an amplitude ceiling. Performs no allocations, decoding or file reads.
     * @param output Writable buffer containing space for frames * 2 floats.
     * @param frames Number of stereo frames requested by the device callback.
     */
    void render(float* output, std::size_t frames) {
        std::fill(output,output+frames*2,0.f);
        if(muted_) return;
        constexpr std::array<float,5> gains{{0.10f,0.12f,0.055f,0.22f,0.20f}};
        for(auto& voice:voices_) if(voice.active) {
            const auto i=static_cast<std::size_t>(voice.cue);
            const auto& data=clips[i];
            // Device requests may cross the end of a clip. Mix only available
            // samples, leave the rest of the cleared output silent, then retire it.
            const auto count=std::min(frames*2,data.size()-voice.offset);
            for(std::size_t n=0;n<count;++n) output[n]+=data[voice.offset+n]*gains[i];
            voice.offset+=count;
            if(voice.offset==data.size())voice.active=false;
        }
        // Conservative master gain and a final safety ceiling, independent of voice count.
        for(std::size_t n=0;n<frames*2;++n) output[n]=std::clamp(output[n]*0.6f,-0.7f,0.7f);
    }
private:
    /**
     * Tracks a clip's next interleaved sample offset and its replacement age.
     * Serial values order voice reuse; offsets are sample counts, not stereo frame counts.
     */
    struct Voice { SoundCue cue=SoundCue::Selection; std::size_t offset=0; bool active=false; unsigned serial=0; };
    std::array<Voice,8> voices_{};
    unsigned serial_=0;
    bool muted_=false;
};
}

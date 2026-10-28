#pragma once
#include "presentation/Feedback.h"
#include <filesystem>
#include <memory>
namespace woodland {
/**
 * The sound-free browser implementation preserves this facade and reports
 * Unavailable without opening a device. Native implementation:
 * Owns decoded optional WAV clips and a miniaudio playback device through Impl.
 *
 * The application calls this facade on its main thread; the device callback mixes
 * cached float32 stereo samples on the audio thread under Impl's mutex. Missing
 * clips are skipped, and missing clips/devices can leave a usable silent object.
 * It has no Board dependency: BoardPlayback emits semantic cues for main to forward.
 * Mute is session state, independent of level restart.
 */
class Audio {
public:
    /**
     * Decodes the five known optional sounds once, then starts PulseAudio/ALSA output
     * only if at least one nonempty clip loaded. Common decoder/device failures are
     * logged and do not throw a gameplay error; unavailable effects remain silent.
     * @param assets Root containing the optional audio/mixkit directory.
     */
    explicit Audio(const std::filesystem::path& assets);
    /**
     * Uninitializes an active device before destroying the cached samples and mutex,
     * so the callback cannot access resources after their owners are released.
     */
    ~Audio();
    Audio(const Audio&)=delete;
    Audio& operator=(const Audio&)=delete;
    /**
     * Queues a semantic effect into the bounded mixer without disk I/O. Unavailable
     * audio, missing clips and muted playback are harmless no-ops. Call on the main
     * thread; the callback shares voice state through the implementation mutex.
     */
    void play(SoundCue cue);
    /**
     * Clears active software voices while preserving mute; used when restarting levels.
     * Already-buffered backend/device samples cannot be retracted by this operation.
     */
    void stop();
    /**
     * Toggles session mute under the callback mutex and clears voices when muting.
     * Unmuting does not resume old effects. Device availability is unchanged.
     */
    void toggleMute();
    /**
     * Reads main-thread UI state, distinguishing an unavailable backend from user mute.
     * Unavailable takes precedence even if the silent session's mute flag was toggled.
     */
    SoundStatus status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

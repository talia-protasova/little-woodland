#include "Audio.h"
namespace woodland {
// Browser releases currently omit licensed WAVs and device initialization. Keep
// semantic cue consumption and session mute behind the existing audio facade.
struct Audio::Impl { bool muted = false; };
Audio::Audio(const std::filesystem::path&) : impl_(std::make_unique<Impl>()) {}
Audio::~Audio() = default;
void Audio::play(SoundCue) {}
void Audio::stop() {}
void Audio::toggleMute() { impl_->muted = !impl_->muted; }
SoundStatus Audio::status() const { return SoundStatus::Unavailable; }
}

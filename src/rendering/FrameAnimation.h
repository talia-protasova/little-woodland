#pragma once
#include "Texture.h"
#include "presentation/FrameTiming.h"
#include <memory>

namespace woodland {
/**
 * Owns a prepared sequence of full-canvas PNG textures and its pure timing table.
 *
 * The development converter handles GIF disposal/composition; runtime only reads
 * frames.txt and PNGs. Every frame loads once into an RAII Texture, so choosing a
 * frame performs no disk reads or GPU uploads. Destruction requires the GL context
 * to remain current. Renderer construction catches failures and uses a static PNG.
 */
class FrameAnimation {
public:
    /**
     * Reads an LWAN1 manifest and numbered frame_0000.png files from one directory.
     * The format records 256x256 canvas dimensions, frame count, total plays (zero
     * for infinite looping), then one duration in seconds per frame. At most 1000
     * frames are accepted; partial loads unwind their Texture owners on failure.
     * @throws std::runtime_error For manifest/read/canvas/texture errors.
     * @throws std::invalid_argument If FrameTiming rejects the duration sequence.
     */
    explicit FrameAnimation(const std::filesystem::path& directory);
    /**
     * Borrows the preloaded texture selected for elapsed result-page seconds.
     * The reference remains owned by this animation; finite loops hold the last frame.
     * @throws std::invalid_argument For negative/nonfinite elapsed time.
     */
    const Texture& frame(double seconds) const { return *frames_[timing_->frame(seconds)]; }
private:
    std::vector<std::unique_ptr<Texture>> frames_;
    std::unique_ptr<FrameTiming> timing_;
};
}

#include "FrameAnimation.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace woodland {
/**
 * Loads immutable GPU frame storage; elapsed-time selection stays in FrameTiming.
 * The renderer supplies BoardPlayback result seconds and catches loading errors
 * for its PNG fallback. This constructor neither advances nor resets a clock.
 */
FrameAnimation::FrameAnimation(const std::filesystem::path& directory) {
    std::ifstream manifest(directory / "frames.txt");
    std::string magic;
    int width=0, height=0, count=0;
    unsigned plays=0;
    // The manifest fixes the canvas and frame count before image loading.
    // Reject impossible counts/dimensions rather than exposing a partial animation
    // whose frame indices would no longer match the authored timing sequence.
    if (!(manifest >> magic >> width >> height >> count >> plays) || magic!="LWAN1"
        || width!=256 || height!=256 || count<=0 || count>1000)
        throw std::runtime_error("Invalid animation manifest: " + directory.string());
    std::vector<double> durations;
    // Each duration belongs to one numbered PNG, in seconds rather than frame
    // counts. Unequal GIF delays therefore survive variable rendering frame rates.
    for(int i=0;i<count;++i) {
        double duration=0;
        if (!(manifest>>duration)) throw std::runtime_error("Missing animation duration: " + directory.string());
        durations.push_back(duration);
    }
    // Validate all authored durations before allocating frame textures. This keeps
    // malformed timing manifests from beginning an expensive partial GPU load.
    timing_ = std::make_unique<FrameTiming>(durations, plays);
    for(int i=0;i<count;++i) {
        std::ostringstream name;
        name << "frame_" << std::setw(4) << std::setfill('0') << i << ".png";
        // Each file contains the already-composed full canvas, so switching frames
        // never needs runtime GIF disposal handling or changes to the character bounds.
        auto texture=std::make_unique<Texture>(directory / name.str());
        if(texture->width()!=width || texture->height()!=height)
            throw std::runtime_error("Animation canvas mismatch: " + directory.string());
        // Transfer exclusive ownership only after the canvas check succeeds.
        // If a later frame fails, member destruction releases all earlier textures;
        // no per-frame image upload is required during subsequent frame lookup.
        frames_.push_back(std::move(texture));
    }
}
}

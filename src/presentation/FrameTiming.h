#pragma once
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace woodland {
/**
 * Maps elapsed seconds to an authored sprite-frame index without owning textures
 * or an update clock. FrameAnimation supplies its positive per-frame durations;
 * BoardPlayback supplies time since the result page appeared. This separation lets
 * frame selection be tested without OpenGL and remain independent of frame rate.
 */
class FrameTiming {
public:
    /**
     * Stores durations in seconds and precomputes one cycle's length.
     * @param plays Total cycle count; zero means repeat forever.
     * @throws std::invalid_argument For an empty sequence or nonpositive/nonfinite duration.
     */
    FrameTiming(std::vector<double> durations, unsigned plays = 0)
        : durations_(std::move(durations)), plays_(plays) {
        if (durations_.empty()) throw std::invalid_argument("Empty frame animation");
        for (double duration : durations_) {
            if (!std::isfinite(duration) || duration <= 0) throw std::invalid_argument("Invalid frame duration");
            length_ += duration;
        }
    }
    /**
     * Selects the active frame from total elapsed time; infinite loops wrap, while
     * finite animations hold their final frame after the requested cycle count.
     * A small boundary tolerance prevents rounding from delaying a frame transition.
     * @throws std::invalid_argument If seconds is negative or nonfinite.
     */
    std::size_t frame(double seconds) const {
        if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid animation time");
        if (plays_ && seconds >= length_ * plays_) return durations_.size()-1;
        seconds = std::fmod(seconds, length_);
        for (std::size_t i=0; i<durations_.size(); ++i) {
            if (seconds + 1e-10 < durations_[i]) return i;
            seconds -= durations_[i];
        }
        return 0;
    }
private:
    std::vector<double> durations_;
    unsigned plays_;
    double length_ = 0;
};
}

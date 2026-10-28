#include "BoardPlayback.h"

#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace woodland {

/**
 * Assigns time to recorded visual phases without simulating puzzle rules.
 * A normal move repeats highlight, removal, gravity/delivery and refill for each
 * cascade. Rejection uses SwapBack; terminal UI stages begin only after resolution.
 */
double BoardPlayback::duration(Board::Stage stage) {
    switch (stage) {
    case Board::Stage::Swap: case Board::Stage::SwapBack: return 0.15;
    case Board::Stage::Match: return 0.15;
    case Board::Stage::Removal: return 0.10;
    case Board::Stage::Gravity: return 0.22;
    case Board::Stage::Refill: return 0.18;
    case Board::Stage::Reshuffle: return 0.65;
    case Board::Stage::Delivery: return 0.5;
    case Board::Stage::Victory: case Board::Stage::Defeat: return 0.6;
    }
    throw std::logic_error("Unknown board animation stage");
}

void BoardPlayback::click(Board& board, std::optional<Cell> cell) {
    if (!acceptsInput()) return; // Discard, never defer inputs made during resolution.
    if (!cell) { selected_.reset(); return; }
    if (!selected_) { selected_ = cell; soundCues_.push_back(SoundCue::Selection); return; }
    if (selected_->row == cell->row && selected_->column == cell->column) {
        selected_.reset();
        return;
    }
    // The Board commits a legal result immediately, but the trace retains every
    // visible intermediate. Rendering the final Board now would skip the swap,
    // removal and falling motion that explains how the player caused the result.
    Board::Trace trace;
    board.trySwap(*selected_, *cell, &trace);
    if (!trace.empty()) play(std::move(trace));
    else { selected_ = cell; soundCues_.push_back(SoundCue::Selection); }
}

void BoardPlayback::click(Level& level, std::optional<Cell> cell) {
    // Playback locks transient phases; Level also locks completed results and
    // pending turns. Neither kind of rejected click is buffered for a later move.
    if (!acceptsInput() || !level.acceptsInput()) return;
    if (!cell || level.board().at(cell->row, cell->column) == TileType::Letter) {
        selected_.reset(); return;
    }
    if (!selected_) { selected_ = cell; soundCues_.push_back(SoundCue::Selection); return; }
    if (selected_->row == cell->row && selected_->column == cell->column) {
        selected_.reset(); return;
    }
    // Level adds the budget/outcome handshake to the same recorded Board rules.
    // A nonempty rejected-swap trace still plays out and back without spending a
    // move; a pre-swap rejection can instead select the newly clicked ordinary cell.
    Board::Trace trace;
    level.trySwap(*selected_, *cell, &trace);
    if (!trace.empty()) play(std::move(trace));
    else { selected_ = cell; soundCues_.push_back(SoundCue::Selection); }
}

void BoardPlayback::update(Level& level, double seconds) {
    // Only this overload completes the Level handshake. The bare-clock overload
    // remains useful for tests and does not decide game outcomes.
    const auto previous = level.state();
    update(seconds);
    // Acknowledge only after the trace cursor is exhausted, including terminal
    // UI timing. Accepted moves remain pending and board input stays locked before it.
    if (acceptsInput()) level.finishTurn();
    if (previous != GameState::Won && level.state() == GameState::Won)
        soundCues_.push_back(SoundCue::Victory);
}

std::optional<GameState> BoardPlayback::result(const Level& level) const {
    // Delivery can be committed in the model long before it reaches the screen.
    // While a trace is active, only its terminal UI stage can expose a result.
    if (const auto* step = current()) {
        if (step->stage == Board::Stage::Victory) return GameState::Won;
        if (step->stage == Board::Stage::Defeat) return GameState::Lost;
        return std::nullopt;
    }
    if (level.state() != GameState::Playing) return level.state();
    return std::nullopt;
}

/**
 * Separates outcome knowledge from page visibility. The terminal stage first
 * fades gameplay away; renderer and control hit testing switch pages together
 * when its outgoing fraction has elapsed.
 */
bool BoardPlayback::showingResult(const Level& level) const {
    return result(level).has_value() && (!current() || progress() >= resultPageStart);
}

int BoardPlayback::pendingDeliveries() const {
    int count = 0;
    // Include the current delivery until its animation finishes, so progress
    // advances with the visible exit effect instead of with immediate rule execution.
    for (std::size_t i = index_; i < steps_.size(); ++i)
        if (steps_[i].stage == Board::Stage::Delivery)
            count += static_cast<int>(steps_[i].matched.count());
    return count;
}

void BoardPlayback::play(Board::Trace steps) {
    if (!acceptsInput()) throw std::logic_error("Cannot replace active board playback");
    // Each new trace owns its cursor and visual selection reset. Sound cues
    // already queued by selection stay queued until the main loop drains them.
    steps_ = std::move(steps);
    index_ = 0;
    elapsed_ = 0;
    selected_.reset();
    // A fresh turn starts with no result animation time. Restart replaces the
    // whole playback object, clearing these clocks and the sound queue together.
    resultSeconds_ = 0;
    resultClockRunning_ = false;
    enterStage();
}

void BoardPlayback::enterStage() {
    if (!current()) return;
    switch (current()->stage) {
    // Removal entry follows match highlighting. Cues live here, not in draw(),
    // so multiple rendered frames of a stage cannot replay the same sound.
    case Board::Stage::Removal: soundCues_.push_back(SoundCue::Match); break;
    case Board::Stage::Gravity: soundCues_.push_back(SoundCue::Falling); break;
    case Board::Stage::Delivery:
        for (std::size_t i=0;i<current()->matched.count();++i) soundCues_.push_back(SoundCue::Delivery);
        break;
    default: break;
    }
}

std::vector<SoundCue> BoardPlayback::takeSoundCues() {
    std::vector<SoundCue> result;
    result.swap(soundCues_);
    return result;
}

void BoardPlayback::update(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid animation elapsed time");
    if (acceptsInput()) {
        // Once settled, no rule snapshots remain. Only the visible result
        // character continues to consume elapsed seconds for its authored loop.
        if (resultClockRunning_) resultSeconds_ += seconds;
        return;
    }
    // Spend only the portion of this frame belonging to each stage, carrying the
    // remainder into following stages. A slow frame must still emit every entry cue.
    while (!acceptsInput()) {
        const auto stage = steps_[index_].stage;
        const double stageDuration = duration(stage);
        // Consume at most the active stage's remaining time; one update can
        // finish several stages, but partial progress belongs only to the last one.
        const double consumed = std::min(seconds, std::max(0.0, stageDuration-elapsed_));
        const double before = elapsed_;
        elapsed_ += consumed;
        seconds -= consumed;
        if (stage == Board::Stage::Victory || stage == Board::Stage::Defeat) {
            // The outgoing board fade belongs to the terminal stage, but not to
            // the character loop. Count only time after the result page appears.
            const double start = stageDuration * resultPageStart;
            resultSeconds_ += std::max(0.0, elapsed_ - std::max(before, start));
            if (elapsed_ >= start) resultClockRunning_ = true;
        }
        // A tiny tolerance avoids a boundary sticking because of floating-point
        // subtraction; this does not change the configured stage durations.
        if (elapsed_ + 1e-12 < stageDuration) break;
        // Enter the next recorded stage at its exact boundary. Entry emits
        // feedback once even when the remaining frame time is already zero.
        elapsed_ = 0;
        ++index_;
        enterStage();
    }
    if (acceptsInput()) {
        // A long frame may finish the trace with time left. Preserve it for the
        // looping result character even after stage snapshots are released.
        if (resultClockRunning_) resultSeconds_ += seconds;
        steps_.clear(); index_ = 0; elapsed_ = 0;
    }
}

// The active pointer borrows owned trace storage. Clearing a completed trace
// at the end of the timeline update makes nullptr the unambiguous idle representation.
const Board::Step* BoardPlayback::current() const {
    return acceptsInput() ? nullptr : &steps_[index_];
}

float BoardPlayback::progress() const {
    return acceptsInput() ? 1.0f : static_cast<float>(elapsed_ / duration(steps_[index_].stage));
}

} // namespace woodland

#pragma once

#include "puzzle/Level.h"
#include "Feedback.h"
#include <optional>

namespace woodland {

/**
 * Presents an already-resolved Board trace using elapsed time and a selection state.
 *
 * The Board remains authoritative and can already contain the final tiles while
 * current() still exposes intermediate snapshots for drawing. This class owns the
 * trace, stage cursor, queued sound cues and result-character clock; it never draws
 * RNG or owns GPU/audio resources. The Level overloads also enforce outcome gates.
 * Replacing this object on restart clears all pending visual and sound feedback.
 */
class BoardPlayback {
public:
    /**
     * Handles selection and traced swaps for a bare Board, mainly for rule/playback tests.
     * Busy input is discarded rather than replayed later. std::nullopt clears selection.
     * Callers supply valid board cells; this overload has no level budget/outcome gate.
     */
    void click(Board& board, std::optional<Cell> cell);
    /**
     * Handles gameplay selection through Level's input gate. Letters clear selection
     * without a cue; choosing a selectable cell emits one selection cue. Traced swaps
     * clear selection and lock further board clicks until their playback completes.
     * @param cell A valid zero-based cell from hit testing, or std::nullopt off-board.
     */
    void click(Level& level, std::optional<Cell> cell);
    /**
     * Advances the timeline, acknowledges a fully completed turn and queues Victory
     * only on the transition into Won. Defeat intentionally has no dedicated cue.
     * @throws std::invalid_argument If seconds is negative or not finite.
     */
    void update(Level& level, double seconds);
    /**
     * Counts letters in delivery stages not yet finished, including the current stage.
     * Subtract this from the committed Board count to avoid showing progress too early.
     */
    int pendingDeliveries() const;
    /**
     * Drains queued cues in timeline order. The application forwards each once to Audio;
     * drawing repeated frames must never replay effects or inspect the queue itself.
     */
    std::vector<SoundCue> takeSoundCues();
    /**
     * Returns time since the result page became visible, excluding the outgoing fade.
     * Continues after the terminal trace ends so authored character frames can loop.
     */
    double resultSeconds() const { return resultSeconds_; }
    /**
     * Exposes the terminal UI-stage outcome, or the settled Level outcome when idle.
     * Returns no outcome during ordinary gameplay steps, even if Board delivery counts
     * already satisfy the objective. A returned outcome may precede Level publication.
     */
    std::optional<GameState> result(const Level& level) const;
    /**
     * Reports when result-page drawing and hit testing should replace gameplay.
     * The first portion of the terminal UI stage still fades the outgoing board.
     */
    bool showingResult(const Level& level) const;
    /// Fraction of the terminal UI stage reserved for fading out gameplay.
    static constexpr float resultPageStart = 1.f / 3.f;
    /**
     * Takes ownership of a trace, resets selection/result timing and enters its first stage.
     * Existing queued selection cues are retained for the application to drain.
     * @throws std::logic_error If another trace is still active.
     */
    void play(Board::Trace steps);
    /**
     * Consumes elapsed seconds across as many stages as needed, queuing every entry cue.
     * Also advances the result clock, but does not acknowledge Level outcomes itself.
     * @throws std::invalid_argument If seconds is negative or not finite.
     */
    void update(double seconds);
    /**
     * Reports only whether the trace is idle. Callers must also consult Level for
     * win/loss/pending restrictions; an idle result page does not permit board input.
     */
    bool acceptsInput() const { return index_ == steps_.size(); }
    /**
     * Borrows the active snapshot, or nullptr when idle. Do not retain the pointer
     * across update, play or restart because the owned trace may be replaced/cleared.
     */
    const Board::Step* current() const;
    /**
     * Returns the uneased fraction of the active stage, or 1 while idle.
     * The renderer applies its own interpolation without affecting rule timing.
     */
    float progress() const;
    std::optional<Cell> selected() const { return selected_; }
    /**
     * Defines presentation seconds per stage, independent of rule results or frame rate.
     * @throws std::logic_error For an unsupported stage value.
     */
    static double duration(Board::Stage stage);

private:
    /**
     * Queues effects at removal/gravity/delivery entry, not per rendered frame.
     * A delivery mask can represent several letters and emits one cue for each.
     */
    void enterStage();
    std::vector<SoundCue> soundCues_;
    double resultSeconds_ = 0;
    bool resultClockRunning_ = false;
    Board::Trace steps_;
    std::size_t index_ = 0;
    double elapsed_ = 0;
    std::optional<Cell> selected_;
};

} // namespace woodland

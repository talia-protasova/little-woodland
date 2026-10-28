#include "presentation/BoardPlayback.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace woodland;
using Stage = Board::Stage;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// This oracle intentionally covers ordinary-board traces only. Letter delivery
// and terminal result stages have separate LevelTests/ResultTests coverage.
Board::Matches triples(const Board::Tiles& tiles) {
    Board::Matches matched;
    for (int r = 0; r < 7; ++r) for (int c = 0; c < 7; ++c) {
        const int i = r * 7 + c;
        if (tiles[i] == TileType::Empty) continue;
        for (int delta : {1, 7}) {
            if ((delta == 1 && c > 4) || (delta == 7 && r > 4)) continue;
            if (tiles[i] == tiles[i + delta] && tiles[i] == tiles[i + 2 * delta])
                for (int k = 0; k < 3; ++k) matched.set(i + k * delta);
        }
    }
    return matched;
}

/**
 * Chooses the first ordinary-tile match-producing exchange using copied cells and
 * the independent triple oracle. It does not mutate the board or its RNG merely
 * to choose the next deterministic replay input.
 */
std::pair<Cell, Cell> legalPair(const Board& board) {
    for (int r = 0; r < 7; ++r) for (int c = 0; c < 7; ++c) {
        for (Cell delta : {Cell{0, 1}, Cell{1, 0}}) {
            Cell second{r + delta.row, c + delta.column};
            if (second.row >= 7 || second.column >= 7) continue;
            auto tiles = board.tiles();
            std::swap(tiles[r * 7 + c], tiles[second.row * 7 + second.column]);
            if (triples(tiles).any()) return {{r, c}, second};
        }
    }
    throw std::runtime_error("No legal pair found");
}

bool sameCell(Cell a, Cell b) { return a.row == b.row && a.column == b.column; }

/**
 * Compares ordered snapshots and each recorded piece path. Equal final tiles are
 * not enough: a divergent trace could still display a wrong swap or survivor path.
 */
void equalTrace(const Board::Trace& a, const Board::Trace& b) {
    check(a.size() == b.size(), "Replay step count diverged");
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto& x = a[i]; const auto& y = b[i];
        check(x.stage == y.stage && x.before == y.before && x.after == y.after
              && x.matched == y.matched && x.wave == y.wave && x.motions.size() == y.motions.size(),
              "Replay snapshots diverged");
        for (std::size_t j = 0; j < x.motions.size(); ++j)
            check(x.motions[j].tile == y.motions[j].tile && sameCell(x.motions[j].from, y.motions[j].from)
                  && sameCell(x.motions[j].to, y.motions[j].to), "Replay motions diverged");
    }
}

// Reconstruct each rule stage independently, then verify continuity and visible
// movement endpoints. This catches plausible-looking traces detached from Board.
void validate(const Board::Trace& trace, Board::Tiles initial, const Board& final) {
    int wave = 0;
    bool removed = false;
    for (std::size_t n = 0; n < trace.size(); ++n) {
        const auto& step = trace[n];
        check(step.before == initial, "Discontinuous snapshots");
        auto expected = initial;
        switch (step.stage) {
        case Stage::Swap: case Stage::SwapBack: {
            check(step.motions.size() == 2, "Swap lacks two tile paths");
            const auto& first = step.motions[0]; const auto& second = step.motions[1];
            check(sameCell(first.from, second.to) && sameCell(first.to, second.from), "Swap paths are not reversed");
            check(std::abs(first.from.row - first.to.row) + std::abs(first.from.column - first.to.column) == 1,
                  "Swap path not adjacent");
            std::swap(expected[first.from.row * 7 + first.from.column], expected[first.to.row * 7 + first.to.column]);
            break;
        }
        case Stage::Match:
            check(step.wave == ++wave && step.matched == triples(initial) && step.matched.any(), "Incorrect match/cascade wave");
            check(n + 1 < trace.size() && trace[n + 1].stage == Stage::Removal, "Highlight not followed by removal");
            break;
        case Stage::Removal:
            check(n > 0 && trace[n - 1].stage == Stage::Match && step.matched == trace[n - 1].matched,
                  "Removal detached from detected match");
            for (int i = 0; i < 49; ++i) if (step.matched[i]) expected[i] = TileType::Empty;
            removed = true;
            break;
        case Stage::Gravity: {
            check(removed || step.wave == 0, "Gravity happened before removal");
            std::size_t motionCount = 0;
            for (int c = 0; c < 7; ++c) {
                std::vector<std::pair<int, TileType>> survivors;
                for (int r = 0; r < 7; ++r)
                    if (initial[r * 7 + c] != TileType::Empty) survivors.push_back({r, initial[r * 7 + c]});
                for (int r = 0; r < 7; ++r) expected[r * 7 + c] = TileType::Empty;
                for (std::size_t i = 0; i < survivors.size(); ++i) {
                    const int target = 7 - static_cast<int>(survivors.size()) + static_cast<int>(i);
                    expected[target * 7 + c] = survivors[i].second;
                    if (target == survivors[i].first) continue;
                    ++motionCount;
                    check(std::count_if(step.motions.begin(), step.motions.end(), [&](const Board::Motion& m) {
                        return m.tile == survivors[i].second && sameCell(m.from, {survivors[i].first, c}) && sameCell(m.to, {target, c});
                    }) == 1, "Gravity lost/reordered a survivor path");
                }
            }
            check(step.motions.size() == motionCount, "Extra gravity tile introduced");
            break;
        }
        case Stage::Refill: {
            std::size_t count = 0;
            for (int i = 0; i < 49; ++i) {
                if (initial[i] != TileType::Empty) continue;
                ++count;
                const int row = i / 7, column = i % 7;
                int emptyCount = 0;
                for (int r = 0; r < 7; ++r) if (initial[r * 7 + column] == TileType::Empty) ++emptyCount;
                check(static_cast<unsigned>(step.after[i]) < 5, "Refill not full/valid");
                check(std::count_if(step.motions.begin(), step.motions.end(), [&](const Board::Motion& m) {
                    return sameCell(m.to, {row, column}) && sameCell(m.from, {row - emptyCount, column})
                           && m.from.row < 0 && m.tile == step.after[i];
                }) == 1, "Replacement not spawned above its column");
                expected[i] = step.after[i];
            }
            check(step.motions.size() == count, "Refill changed existing tile");
            break;
        }
        case Stage::Reshuffle:
            check(!Board(initial).hasLegalSwap() && triples(initial).none(), "Reshuffle not dead-board recovery");
            check(Board(step.after).hasLegalSwap() && triples(step.after).none(), "Reshuffle not playable/stable");
            check(n + 1 == trace.size(), "Recovery not final stage");
            expected = step.after; // Only this explicit transition can globally replace tiles.
            break;
        }
        check(expected == step.after, "Step changed tiles outside its rule");
        // Reconstruct movement at t=1, exactly as the renderer does, independently of rule result.
        if (!step.motions.empty()) {
            auto reconstructed = step.before;
            for (const auto& motion : step.motions)
                if (motion.from.row >= 0) reconstructed[motion.from.row * 7 + motion.from.column] = TileType::Empty;
            for (const auto& motion : step.motions)
                reconstructed[motion.to.row * 7 + motion.to.column] = motion.tile;
            check(reconstructed == step.after, "Animation endpoints disagree with snapshot");
        }
        initial = step.after;
    }
    check(initial == final.tiles(), "Final animated state differs from Board");
}

/**
 * Compares unrecorded rules with two recorded twins over 750 accepted moves.
 * The seed range and deterministic helper keep failures reproducible, while the
 * stage oracle validates continuity, clearing and reconstructed motion endpoints.
 */
void replayTests() {
    bool cascades = false;
    // Compare unrecorded execution and two independently recorded twins across
    // cascades. Producing trace snapshots must not change any future RNG outcome.
    for (unsigned seed = 0; seed < 30; ++seed) {
        auto plain = Board::generate(seed), recorded = plain, twin = plain;
        for (int move = 0; move < 25; ++move) {
            const auto pair = legalPair(plain);
            const auto before = plain.tiles();
            Board::Trace trace, repeated;
            const auto a = plain.trySwap(pair.first, pair.second);
            const auto b = recorded.trySwap(pair.first, pair.second, &trace);
            twin.trySwap(pair.first, pair.second, &repeated);
            check(a.valid && b.valid && a.resolution.waves == b.resolution.waves
                  && a.resolution.reshuffled == b.resolution.reshuffled, "Recording changed resolution");
            check(plain.tiles() == recorded.tiles() && plain.moves() == recorded.moves(), "Recording changed RNG or rules");
            check(trace.front().stage == Stage::Swap, "First stage is not swap");
            validate(trace, before, recorded);
            equalTrace(trace, repeated);
            cascades |= b.resolution.waves > 1;
        }
    }
    check(cascades, "No cascades covered by traces");
}

/**
 * Covers wave-zero setup holes and a standalone dead-board reshuffle. These are
 * not normal swap waves, but still need explicit snapshots and a finite input lock
 * whose playback must not change rule RNG compared with unrecorded execution.
 */
void setupAndRecoveryTests() {
    Board::Tiles tiles;
    for (int r = 0; r < 7; ++r) for (int c = 0; c < 7; ++c) tiles[r * 7 + c] = static_cast<TileType>((r + c) % 5);
    Board dead(tiles, 73), twin = dead;
    Board::Trace trace;
    dead.resolve(&trace); twin.resolve();
    check(trace.size() == 1 && trace.front().stage == Stage::Reshuffle, "Dead board lacks explicit reshuffle");
    validate(trace, tiles, dead);
    check(dead.tiles() == twin.tiles(), "Recovery trace changed RNG");
    BoardPlayback recovery;
    recovery.play(trace);
    check(!recovery.acceptsInput() && recovery.current()->stage == Stage::Reshuffle, "Reshuffle not input-locked");
    recovery.update(0.65);
    check(recovery.acceptsInput(), "Recovery failed to unlock input");

    // Setup holes exercise the initial wave-zero gravity/refill path, which ordinary
    // accepted swaps on a full board usually omit from their trace.
    tiles[0] = tiles[14] = tiles[35] = TileType::Empty;
    Board holes(tiles, 991), plain = holes;
    trace.clear();
    holes.resolve(&trace); plain.resolve();
    validate(trace, tiles, holes);
    check(holes.tiles() == plain.tiles(), "Setup resolution trace changed RNG");
}

/**
 * Injects input throughout each animation and checks that authoritative tiles stay
 * fixed while the visual cursor advances. Also covers reversal traces, idle input
 * restoration and equivalent partial-versus-single elapsed-time updates.
 */
void playbackAndInputTests() {
    auto board = Board::generate(42397);
    const auto original = board.tiles();
    const auto pair = legalPair(board);
    BoardPlayback playback;
    check(playback.acceptsInput() && !playback.current(), "Initial input locked");
    playback.click(board, pair.first); playback.click(board, pair.second);
    check(!playback.acceptsInput() && !playback.selected() && board.moves() == 1, "Move did not start playback/input lock");
    check(playback.current()->before == original, "Playback starts at final board");
    const auto authoritative = board.tiles();
    int count = 0;
    while (!playback.acceptsInput()) {
        const auto* step = playback.current();
        // Inject clicks during every active phase; ignored input must neither
        // mutate the already-committed Board nor become a deferred next move.
        playback.click(board, Cell{0, 0}); playback.click(board, Cell{0, 1}); playback.click(board, std::nullopt);
        check(playback.current() == step && board.tiles() == authoritative && board.moves() == 1
              && !playback.selected(), "Input changed board or selection while locked");
        const double duration = BoardPlayback::duration(step->stage);
        playback.update(duration / 2);
        check(!playback.acceptsInput() && std::abs(playback.progress() - 0.5f) < 0.0001f, "Elapsed-time progress incorrect");
        playback.update(duration / 2 + 1e-10);
        check(++count < 500, "Playback stuck");
    }
    check(!playback.current() && board.tiles() == authoritative, "Playback final state incorrect");
    playback.click(board, Cell{0, 0});
    check(playback.selected().has_value(), "Input did not resume");

    // Invalid adjacent swaps animate out and back, without consuming moves or random state.
    auto rejected = Board::generate(42397), clean = rejected;
    Board::Trace trace;
    check(!rejected.trySwap({0, 0}, {0, 1}, &trace).valid && trace.size() == 2
          && trace[1].stage == Stage::SwapBack, "Rejected swap lacks reversal");
    validate(trace, rejected.tiles(), rejected);
    auto split = BoardPlayback{}, single = BoardPlayback{};
    split.play(trace); single.play(trace);
    split.update(0.04); split.update(0.06); single.update(0.10);
    check(split.current()->stage == single.current()->stage && std::abs(split.progress() - single.progress()) < 1e-6,
          "Animation depends on frame partition");
    split.update(10); single.update(10);
    check(split.acceptsInput() && single.acceptsInput(), "Long frame did not finish playback");
    const auto next = legalPair(clean);
    rejected.trySwap(next.first, next.second); clean.trySwap(next.first, next.second);
    check(rejected.tiles() == clean.tiles() && rejected.moves() == clean.moves(), "Rejected animation consumed RNG");
    trace.clear();
    check(!clean.trySwap({0, 0}, {0, 2}, &trace).valid && trace.empty(), "Non-adjacent swap animated");
}

int main() {
    try {
        replayTests(); setupAndRecoveryTests(); playbackAndInputTests();
        std::cout << "Resolution snapshots, 750 traced moves, elapsed-time playback and input locking passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}

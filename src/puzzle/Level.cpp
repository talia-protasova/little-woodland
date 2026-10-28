#include "Level.h"
#include <stdexcept>
#include <utility>

namespace woodland {
LevelDefinition LevelDefinition::demo(std::uint32_t seed) {
    LevelDefinition result;
    result.seed = seed;
    // Copy only generated tiles into the definition. makeBoard initializes a fresh
    // RNG with this seed for gameplay; it does not resume the generation RNG stream.
    result.tiles = Board::generate(seed).tiles();
    result.letters = {{1, 3}};
    result.exits.set(3);
    return result;
}

/**
 * Builds the ordinary board and special placements in separate validation phases.
 * Configuration checks reject unreachable exit placement and invalid budgets, but
 * do not claim arbitrary content is solvable within that budget.
 */
Board Level::makeBoard(const LevelDefinition& d) {
    // A target cannot exceed the supplied letter count because ordinary refill
    // never generates letters. An exit and a positive move budget are required
    // before any cell data is inspected.
    if (d.moveLimit <= 0 || d.requiredLetters <= 0
        || d.requiredLetters > static_cast<int>(d.letters.size()) || d.exits.none())
        throw std::invalid_argument("Invalid level budget/objective/exits");
    // Work on a copy so overlaying special pieces never mutates the definition
    // needed for deterministic restart. The caller must initialize every base cell.
    auto tiles = d.tiles;
    for (auto tile : tiles)
        if (tile == TileType::Letter || tile == TileType::Empty)
            throw std::invalid_argument("Level requires ordinary full board plus explicit letters");
    // Ordinary base cells and special placements stay separate. Since letters
    // never move sideways, each needs an exit in the same column below its start.
    Board::Matches placed;
    for (auto cell : d.letters) {
        if (cell.row < 0 || cell.row >= Board::size || cell.column < 0 || cell.column >= Board::size
            || !d.exits[cell.column] || cell.row == Board::size - 1)
            throw std::invalid_argument("Letter needs a reachable exit below it");
        const int i = cell.row * Board::size + cell.column;
        // Reject duplicate placements instead of quietly replacing one objective
        // letter twice and leaving fewer deliverable pieces than the target assumes.
        if (placed[i]) throw std::invalid_argument("Duplicate letter position");
        placed.set(i);
        tiles[i] = TileType::Letter;
    }
    Board board(tiles, d.seed, d.exits);
    if (board.hasMatches()) throw std::invalid_argument("Level starts with matches");
    // Overlaying a letter can remove an otherwise legal ordinary swap. Recover
    // without moving the letter, rather than presenting a dead opening to the player.
    board.recoverIfNoMoves();
    return board;
}

Level::Level(LevelDefinition definition)
    : definition_(std::move(definition)), board_(makeBoard(definition_)) {}

SwapResult Level::trySwap(Cell first, Cell second, Board::Trace* trace) {
    if (trace) trace->clear();
    // Board does not know level budgets or result states. This gate rejects
    // further attempts during a pending turn and after either final outcome.
    if (!acceptsInput()) return {};
    auto result = board_.trySwap(first, second, trace);
    if (result.valid) {
        // The Board already holds final tiles, but presentation must acknowledge
        // this turn before its outcome unlocks input or becomes a settled game state.
        pending_ = true;
        if (trace && (board_.delivered() >= definition_.requiredLetters || remainingMoves() == 0)) {
            const auto stage = board_.delivered() >= definition_.requiredLetters
                ? Board::Stage::Victory : Board::Stage::Defeat;
            // Terminal stages contain identical tiles: they transition the UI,
            // not the rules. Their placement at the end preserves every cascade
            // and delivery before the renderer may introduce the result page.
            trace->push_back({stage, board_.tiles(), board_.tiles(), {}, {}, 0});
        }
    }
    return result;
}

void Level::finishTurn() {
    // Only accepted moves create pending work. Repeated idle updates must not
    // reevaluate or republish an outcome as though a new move had completed.
    if (!pending_) return;
    pending_ = false;
    // The objective wins even when its final delivery uses the last available move.
    if (board_.delivered() >= definition_.requiredLetters) state_ = GameState::Won;
    else if (remainingMoves() == 0) state_ = GameState::Lost;
}

void Level::restart() {
    // Reconstruct first so a construction failure does not discard the old level.
    // Reusing the stored seed resets both visible content and future RNG behavior.
    auto initial = makeBoard(definition_);
    board_ = std::move(initial);
    state_ = GameState::Playing;
    // Presentation and sound own separate session state. The application resets
    // BoardPlayback and stops voices alongside this rule reset, without unmuting.
    pending_ = false;
}
}

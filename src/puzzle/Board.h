#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <random>
#include <vector>

namespace woodland {

/**
 * Identifies ordinary matchable pieces, temporary holes and nonmatchable letters.
 * Only Berry through Star belong to the refill palette; Letter is never randomized.
 */
enum class TileType { Empty = -1, Berry, Acorn, Leaf, Mushroom, Star, Letter };

/**
 * Zero-based board coordinates, with row zero at the top and column zero at the left.
 * Negative rows are used only for the off-board source of refill animation paths.
 */
struct Cell { int row; int column; };

/**
 * Reports how many simultaneous match waves were cleared and whether dead-board
 * recovery ran. These counters describe rule execution, not animation progress.
 */
struct Resolution {
    int waves = 0;
    bool reshuffled = false;
};

/**
 * Distinguishes an accepted player move from a rejected attempt.
 * Resolution details are meaningful only when valid is true.
 */
struct SwapResult {
    bool valid = false;
    Resolution resolution;
};

/**
 * Owns the deterministic 7x7 puzzle state, random generator and accepted-move count.
 *
 * Rules are independent of OpenGL and presentation timing. A successful swap resolves
 * on a candidate board before committing; optional traces describe those same rule
 * transitions without consuming extra randomness. Letters obey gravity but cannot
 * match or be swapped by the player. Level supplies exit columns and interprets
 * delivery counts as objectives. Explicit test layouts may contain holes or matches.
 */
class Board {
public:
    static constexpr int size = 7;
    static constexpr int tileTypeCount = 5;
    using Tiles = std::array<TileType, size * size>;
    using Matches = std::bitset<size * size>;

    /**
     * Names visible rule transitions recorded in a trace. Victory and Defeat are
     * UI-only terminal stages appended by Level after all Board resolution steps.
     */
    enum class Stage { Swap, SwapBack, Match, Removal, Gravity, Refill, Reshuffle, Delivery, Victory, Defeat };
    /**
     * One surviving or entering piece moving between cells. Refill sources may be
     * above the board; coordinates are interpolated by the renderer, not by rules.
     */
    struct Motion {
        TileType tile;
        Cell from;
        Cell to;
    };
    /**
     * A self-contained presentation snapshot. Adjacent steps join through after/before;
     * matched identifies highlighted, removed or delivered cells as appropriate.
     * Motion paths describe gravity/refill/swaps without inferring piece identity
     * from equal tile colors. The snapshots are owned values, not live Board views.
     */
    struct Step {
        Stage stage;
        Tiles before;
        Tiles after;
        Matches matched;
        std::vector<Motion> motions;
        int wave = 0; // One-based match/cascade wave; zero for swap, setup or terminal UI.
    };
    using Trace = std::vector<Step>;

    /**
     * Constructs an explicit layout without resolving it or recovering dead boards.
     * @param tiles Row-major cells; Empty and Letter are valid explicit values.
     * @param seed Initializes the persistent generator used by refill and recovery.
     * @param exits Columns whose bottom-row cells deliver letters during resolution.
     * @throws std::invalid_argument If a tile is outside the supported enum values.
     */
    explicit Board(Tiles tiles, std::uint32_t seed = 42397, std::bitset<size> exits = {});
    /**
     * Generates a full board without initial matches and with at least one legal swap.
     * The seed and subsequent accepted input sequence determine future random draws.
     * @throws std::runtime_error If bounded generation cannot produce a playable board.
     */
    static Board generate(std::uint32_t seed);
    /**
     * Reads a cell using zero-based coordinates.
     * @throws std::out_of_range If either coordinate lies outside the board.
     */
    TileType at(int row, int column) const;
    /**
     * Borrows the current row-major state; callers must not retain assumptions about
     * its contents across a mutating operation. Traces own separate snapshots.
     */
    const Tiles& tiles() const { return tiles_; }
    std::uint64_t moves() const { return moves_; }
    int delivered() const { return delivered_; }
    const std::bitset<size>& exits() const { return exits_; }
    /**
     * Returns the union of cells in horizontal or vertical runs of three or more.
     * Longer runs and intersections share one mask, so each matched cell clears once.
     * Empty cells and letters cannot contribute to a match.
     */
    Matches findMatches() const;
    /**
     * Reports whether any ordinary run is ready to clear; no state or RNG is changed.
     */
    bool hasMatches() const;
    /**
     * Searches right/down adjacent ordinary pairs without drawing randomness.
     * Returns false for incomplete or already-matched boards, which are not ready
     * for player input. A legal exchange must create a match at one swapped cell.
     */
    bool hasLegalSwap() const;
    /**
     * Attempts an orthogonally adjacent ordinary-tile exchange on a full, stable board.
     *
     * Out-of-bounds, diagonal, letter and nonmatching swaps spend no move and leave
     * tiles, delivery count and RNG unchanged. A legal move resolves all cascades
     * and recovery before incrementing moves once and committing the candidate.
     * @param first First zero-based selected cell.
     * @param second Second zero-based selected cell.
     * @param trace Optional output, cleared first. Eligible rejected adjacent swaps
     * record Swap/SwapBack; attempts rejected before an exchange leave it empty.
     * @return Validity and the completed rule-resolution summary.
     * @throws std::runtime_error If candidate resolution reaches a safety limit;
     * the original Board remains unchanged.
     */
    SwapResult trySwap(Cell first, Cell second, Trace* trace = nullptr);

    /**
     * Clears the complete match mask simultaneously, without moving or refilling tiles.
     * @return Number of distinct cells changed to Empty. Does not count a player move.
     */
    int removeMatches();
    /**
     * Compacts each column toward the bottom, preserving the order of all survivors,
     * including letters. Remaining holes occupy the top; no delivery/refill occurs here.
     * @param motions Optional output, cleared first and filled only for moved survivors.
     */
    void applyGravity(std::vector<Motion>* motions = nullptr);
    /**
     * Fills holes with ordinary tiles in row-major RNG order; survivors stay unchanged.
     * Refill can create matches for the next cascade and never creates letters.
     * @param motions Optional paths from above the destination column; presentation
     * assumes gravity has already gathered holes at the top. Cleared before use.
     */
    void refill(std::vector<Motion>* motions = nullptr);
    /**
     * Settles holes/deliveries/refill, then repeatedly clears complete match waves,
     * applies gravity, drains exits and refills until no ordinary match remains.
     * Finally recovers a full dead board, without spending a move.
     * @param trace Optional transitions appended to the caller's existing trace.
     * @throws std::runtime_error If cascade or generation limits are exceeded.
     * This low-level operation mutates in place; trySwap provides candidate rollback.
     */
    Resolution resolve(Trace* trace = nullptr);
    /**
     * Recovers a stable board with no legal swap, keeping letters anchored in place.
     * Tries bounded ordinary-tile shuffles first, then regenerates ordinary cells
     * if their multiset cannot yield a playable board. Recovery spends no move.
     * @return Whether recovery was necessary.
     * @throws std::logic_error If the board contains holes or existing matches.
     * @throws std::runtime_error If bounded fallback generation also fails.
     */
    bool recoverIfNoMoves();

private:
    /**
     * Checks coordinates before any indexing; off-board refill sources are not cells.
     */
    static bool contains(Cell cell);
    /**
     * Checks for holes only; a full layout still needs a separate match-stability check.
     */
    bool full() const;
    /**
     * Counts matching neighbors in both axes through one ordinary cell.
     * Used both for global detection and for proving that an exchanged cell matches.
     */
    bool matchesAt(int row, int column) const;
    /**
     * Draws an unbiased value below a positive bound with explicit rejection sampling.
     * Avoiding STL distributions makes the draw mapping stable across implementations.
     */
    unsigned randomBelow(unsigned bound);
    /**
     * Fills ordinary cells in row-major order while preserving any existing letters.
     * Rejects local triples and retries a bounded number of complete layouts until
     * a legal swap exists. Used for initial generation and recovery fallback.
     */
    void generatePlayable();
    /**
     * Drains bottom-row letters only in exit columns, marks their source cells and
     * updates the lifetime delivery count. Does not draw RNG or compact the new holes.
     */
    int deliver(Matches& delivered);
    Tiles tiles_;
    std::bitset<size> exits_;
    int delivered_ = 0;
    std::mt19937 random_;
    std::uint64_t moves_ = 0;
};

} // namespace woodland

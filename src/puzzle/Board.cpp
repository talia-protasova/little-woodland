#include "Board.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace woodland {

Board::Board(Tiles tiles, std::uint32_t seed, std::bitset<size> exits) : tiles_(tiles), exits_(exits), random_(seed) {
    // Explicit fixtures may be incomplete or matched, so validate enum values only.
    // Level validation and resolution impose the stronger ready-for-play conditions.
    for (const auto tile : tiles_) {
        if (tile != TileType::Empty && tile != TileType::Letter && static_cast<unsigned>(tile) >= tileTypeCount) {
            throw std::invalid_argument("Invalid board tile type");
        }
    }
}

TileType Board::at(int row, int column) const {
    if (row < 0 || row >= size || column < 0 || column >= size) {
        throw std::out_of_range("Board cell outside 7x7 grid");
    }
    return tiles_[row * size + column];
}

bool Board::matchesAt(int row, int column) const {
    const auto tile = at(row, column);
    if (tile == TileType::Empty || tile == TileType::Letter) return false;
    // Count outward from this cell rather than stopping at triples. This includes
    // every member of a long run and naturally recognizes both axes at a crossing.
    int horizontal = 1;
    int vertical = 1;
    for (int c = column - 1; c >= 0 && at(row, c) == tile; --c) ++horizontal;
    for (int c = column + 1; c < size && at(row, c) == tile; ++c) ++horizontal;
    for (int r = row - 1; r >= 0 && at(r, column) == tile; --r) ++vertical;
    for (int r = row + 1; r < size && at(r, column) == tile; ++r) ++vertical;
    return horizontal >= 3 || vertical >= 3;
}

Board::Matches Board::findMatches() const {
    // A union mask is the removal unit: an intersecting tile must not clear twice
    // or let gravity change one run before the other has been recorded.
    Matches result;
    for (int r = 0; r < size; ++r)
        for (int c = 0; c < size; ++c)
            if (matchesAt(r, c)) result.set(r * size + c);
    return result;
}

bool Board::hasMatches() const { return findMatches().any(); }

/**
 * Tests hypothetical exchanges on one reusable copy. The input must already be
 * full and stable, so checking only the exchanged cells is sufficient to detect
 * a newly created match; no cascade or refill is needed just to establish legality.
 */
bool Board::hasLegalSwap() const {
    if (!full() || hasMatches()) return false;
    // Only right/down neighbors are needed to cover every unordered adjacent pair.
    // Undo each probe so the next one sees the original stable layout and no RNG draws.
    Board candidate = *this;
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            for (const Cell offset : {Cell{0, 1}, Cell{1, 0}}) {
                const int nr = r + offset.row;
                const int nc = c + offset.column;
                if (nr >= size || nc >= size || at(r, c) == at(nr, nc)
                    || at(r, c) == TileType::Letter || at(nr, nc) == TileType::Letter) continue;
                auto& first = candidate.tiles_[r * size + c];
                auto& second = candidate.tiles_[nr * size + nc];
                std::swap(first, second);
                const bool legal = candidate.matchesAt(r, c) || candidate.matchesAt(nr, nc);
                std::swap(first, second);
                if (legal) return true;
            }
        }
    }
    return false;
}

bool Board::contains(Cell cell) {
    return cell.row >= 0 && cell.row < size && cell.column >= 0 && cell.column < size;
}

bool Board::full() const {
    return std::find(tiles_.begin(), tiles_.end(), TileType::Empty) == tiles_.end();
}

unsigned Board::randomBelow(unsigned bound) {
    // Explicit rejection sampling keeps replay independent of STL distributions.
    // The accepted integer range has a size divisible by bound; a modulo reduction
    // therefore cannot favor some palette values over others. All callers use bound > 0.
    const auto limit = std::mt19937::max() - (std::mt19937::max() % bound);
    std::mt19937::result_type value;
    do { value = random_(); } while (value >= limit);
    return static_cast<unsigned>(value % bound);
}

/**
 * Seeds one Board-owned generator before generation. Successful setup retains its
 * advanced RNG state for later Board refills; LevelDefinition separately copies
 * the generated tiles and constructs its gameplay Board from the definition seed.
 */
Board Board::generate(std::uint32_t seed) {
    Board board(Tiles{}, seed);
    board.generatePlayable();
    return board;
}

void Board::generatePlayable() {
    // A rejected full layout is generated again with the same advancing RNG,
    // not reseeded. The bounded outer loop guarantees a visible failure instead
    // of blocking forever if anchored letters make ordinary moves impossible.
    for (int attempt = 0; attempt < 1024; ++attempt) {
        for (int r = 0; r < size; ++r) {
            for (int c = 0; c < size; ++c) {
                // Recovery keeps special pieces anchored. In row-major order only
                // left/upper predecessors are final, so checking them prevents triples
                // without needing to resolve or consume refill draws afterward.
                if (at(r, c) == TileType::Letter) continue;
                TileType tile;
                do {
                    tile = static_cast<TileType>(randomBelow(tileTypeCount));
                } while ((c >= 2 && at(r, c - 1) == tile && at(r, c - 2) == tile)
                      || (r >= 2 && at(r - 1, c) == tile && at(r - 2, c) == tile));
                tiles_[r * size + c] = tile;
            }
        }
        // A stable but dead layout would strand the player before a turn can begin.
        if (hasLegalSwap()) return;
    }
    throw std::runtime_error("Unable to generate a playable board after 1024 attempts");
}

SwapResult Board::trySwap(Cell first, Cell second, Trace* trace) {
    if (trace) trace->clear();
    // Manhattan distance one permits horizontal/vertical neighbors only. Check
    // bounds before indexing; holes, pending matches and letters cannot be player swaps.
    if (!contains(first) || !contains(second)
        || std::abs(first.row - second.row) + std::abs(first.column - second.column) != 1
        || !full() || hasMatches()
        || at(first.row, first.column) == TileType::Letter
        || at(second.row, second.column) == TileType::Letter) return {};

    // Resolve on a copy that includes the RNG state, not just the cells. A rejected
    // attempt or failed candidate resolution must not alter a later deterministic refill.
    Board candidate = *this;
    Trace recorded;
    std::swap(candidate.tiles_[first.row * size + first.column],
              candidate.tiles_[second.row * size + second.column]);
    // Record both piece paths from the original layout before testing the match.
    // Even a rejected adjacent exchange needs a forward and reverse visual path,
    // while the authoritative Board remains unchanged throughout that rejection.
    if (trace) recorded.push_back({Stage::Swap, tiles_, candidate.tiles_, {},
        {{at(first.row, first.column), first, second},
         {at(second.row, second.column), second, first}}, 0});
    // Exchanging equal pieces cannot create a new run. Since the input was stable,
    // any genuinely new match must pass through one of the two exchanged positions.
    if (at(first.row, first.column) == at(second.row, second.column)
        || (!candidate.matchesAt(first.row, first.column)
            && !candidate.matchesAt(second.row, second.column))) {
        if (trace) {
            recorded.push_back({Stage::SwapBack, candidate.tiles_, tiles_, {},
                {{at(first.row, first.column), second, first},
                 {at(second.row, second.column), first, second}}, 0});
            *trace = std::move(recorded);
        }
        return {};
    }
    // Only the candidate resolves here. A complete trace follows actual rule
    // snapshots; recording must never simulate a second refill or draw extra RNG.
    const auto resolution = candidate.resolve(trace ? &recorded : nullptr);
    // Cascades and reshuffles belong to this one accepted move, so increment only
    // after the entire candidate reaches its stable, playable final state.
    ++candidate.moves_;
    *this = std::move(candidate);
    if (trace) *trace = std::move(recorded);
    return {true, resolution};
}

int Board::removeMatches() {
    // Compute the full union before changing cells; gravity/refill happen later.
    const auto matched = findMatches();
    for (int i = 0; i < size * size; ++i)
        if (matched.test(i)) tiles_[i] = TileType::Empty;
    return static_cast<int>(matched.count());
}

void Board::applyGravity(std::vector<Motion>* motions) {
    if (motions) motions->clear();
    // Columns are independent. Scanning bottom-up means the next surviving piece
    // fills the lowest hole without overwriting a source that has not been read.
    for (int c = 0; c < size; ++c) {
        // target is the lowest unfilled survivor slot, not the next source row.
        // It stays at or below the scan position because only holes are skipped.
        int target = size - 1;
        for (int r = size - 1; r >= 0; --r) {
            const auto tile = at(r, c);
            if (tile != TileType::Empty) {
                // Stationary pieces need no path: the renderer keeps them in the
                // before snapshot and draws only genuinely moving pieces separately.
                if (motions && target != r) motions->push_back({tile, {r, c}, {target, c}});
                tiles_[target-- * size + c] = tile;
            }
        }
        // Everything above the packed survivors becomes a hole for a later refill.
        // Letters use the same survivor path, so their relative order is preserved.
        while (target >= 0) tiles_[target-- * size + c] = TileType::Empty;
    }
}

void Board::refill(std::vector<Motion>* motions) {
    if (motions) motions->clear();
    std::array<int, size> emptyCounts{};
    // After gravity, emptyCounts is the height of each column's incoming stack.
    // Off-board source rows give refill a continuous entrance without changing RNG order.
    if (motions) {
        for (int r = 0; r < size; ++r)
            for (int c = 0; c < size; ++c)
                if (at(r, c) == TileType::Empty) ++emptyCounts[c];
    }
    // Row-major order is part of the deterministic replay policy.
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            auto& tile = tiles_[r * size + c];
            // Existing pieces, especially letters, must keep their identity.
            // Only holes consume a draw from the five ordinary tile types.
            if (tile == TileType::Empty) {
                tile = static_cast<TileType>(randomBelow(tileTypeCount));
                if (motions) motions->push_back({tile, {r - emptyCounts[c], c}, {r, c}});
            }
        }
    }
}

/**
 * Settles a complete turn as repeated simultaneous clears followed by compaction,
 * exit draining and refill. Falling or new ordinary tiles can create another wave,
 * so stability is established only after the entire cycle has been checked.
 */
Resolution Board::resolve(Trace* trace) {
    Resolution result;
    // Capture the output of the same rule functions used without presentation.
    const auto fallAndRefill = [&](int wave) {
        // Drain all letters currently at exit cells in one stage. Compact again
        // before refill so a higher letter in the same column can reach the exit
        // in its own subsequent delivery stage; it must not be blocked by new tiles.
        for (;;) {
            const auto before = tiles_;
            std::vector<Motion> falling;
            applyGravity(trace ? &falling : nullptr);
            if (trace && before != tiles_)
                trace->push_back({Stage::Gravity, before, tiles_, {}, std::move(falling), wave});
            const auto landed = tiles_;
            Matches delivered;
            if (!deliver(delivered)) break;
            if (trace) trace->push_back({Stage::Delivery, landed, tiles_, delivered, {}, wave});
        }
        const auto fallen = tiles_;
        std::vector<Motion> entering;
        refill(trace ? &entering : nullptr);
        // Omit unchanged steps rather than inventing motion for already-full
        // columns. Every recorded after snapshot is the actual rule result.
        if (trace && fallen != tiles_)
            trace->push_back({Stage::Refill, fallen, tiles_, {}, std::move(entering), wave});
    };
    // Explicit layouts may begin with holes or letters already at exits. Settle
    // those first; a normal full swap generally has nothing to record at wave zero.
    fallAndRefill(0);
    while (hasMatches()) {
        // Refill may form another match, so stability is checked after every wave.
        // Bound pathological cascades rather than leaving the application stuck.
        if (result.waves == 1024) throw std::runtime_error("Cascade resolution exceeded 1024 waves");
        ++result.waves;
        const auto before = tiles_;
        const auto matched = findMatches();
        if (trace) trace->push_back({Stage::Match, before, before, matched, {}, result.waves});
        // Highlight the full mask before clearing any cell. Clearing the union
        // first prevents an overlapping run from being broken by early gravity.
        removeMatches();
        if (trace) trace->push_back({Stage::Removal, before, tiles_, matched, {}, result.waves});
        fallAndRefill(result.waves);
    }
    // Recovery runs only after all matches are gone; the trace distinguishes this
    // global rearrangement from ordinary survivor movement.
    const auto beforeRecovery = tiles_;
    result.reshuffled = recoverIfNoMoves();
    if (trace && result.reshuffled)
        trace->push_back({Stage::Reshuffle, beforeRecovery, tiles_, {}, {}, result.waves});
    return result;
}

bool Board::recoverIfNoMoves() {
    if (!full() || hasMatches())
        throw std::logic_error("Dead-board recovery requires a full, stable board");
    // A playable stable board is a no-op, including its RNG state. Recovery
    // randomness must be consumed only when the player actually has no legal move.
    if (hasLegalSwap()) return false;
    const auto original = tiles_;
    for (int attempt = 0; attempt < 1024; ++attempt) {
        // Each trial starts from the original multiset and fixed letter anchors.
        // Trial acceptance requires both no automatic matches and a legal next move.
        tiles_ = original;
        // Shuffle indices for ordinary cells only. Including letters would silently
        // move the objective sideways or bypass the player's supporting-tile work.
        std::vector<int> ordinary;
        for (int i = 0; i < size * size; ++i)
            if (tiles_[i] != TileType::Letter) ordinary.push_back(i);
        // Fisher-Yates chooses among the remaining slots using the same seeded RNG.
        for (unsigned i = ordinary.size(); i > 1; --i)
            std::swap(tiles_[ordinary[i - 1]], tiles_[ordinary[randomBelow(i)]]);
        if (!hasMatches() && hasLegalSwap()) return true;
    }
    // Some ordinary multisets cannot make a stable playable board. Regenerate only
    // ordinary cells after the shuffle limit; generatePlayable still preserves letters
    // and has its own attempt limit for impossible anchored layouts.
    generatePlayable();
    return true;
}

int Board::deliver(Matches& delivered) {
    int count = 0;
    for (int c = 0; c < size; ++c) {
        const int i = (size - 1) * size + c;
        // Reaching the bottom is insufficient without this column's exit flag.
        // Clearing creates another hole; the caller compacts again before refill,
        // allowing stacked letters to arrive as separate visible delivery stages.
        if (exits_[c] && tiles_[i] == TileType::Letter) {
            tiles_[i] = TileType::Empty;
            delivered.set(i);
            ++count;
        }
    }
    // Count only letters removed from real exits, never ordinary cleared cells.
    // Level later compares this accumulated count with its delivery target.
    delivered_ += count;
    return count;
}

} // namespace woodland

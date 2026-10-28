#include "puzzle/Board.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace woodland;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Begin from a stable, dead Latin-style pattern so each hand-placed run or hole
// is intentional rather than an unrelated automatic match from a random fixture.
Board::Tiles base() {
    Board::Tiles tiles;
    for (int r = 0; r < 7; ++r)
        for (int c = 0; c < 7; ++c) tiles[r * 7 + c] = static_cast<TileType>((r + c) % 5);
    return tiles;
}

// Independent triple oracle; count each cell once, including intersections.
Board::Matches oracle(const Board::Tiles& tiles) {
    Board::Matches result;
    for (int r = 0; r < 7; ++r) {
        for (int c = 0; c < 7; ++c) {
            const int i = r * 7 + c;
            if (tiles[i] == TileType::Empty) continue;
            for (const int step : {1, 7}) {
                if ((step == 1 && c > 4) || (step == 7 && r > 4)) continue;
                if (tiles[i] == tiles[i + step] && tiles[i] == tiles[i + 2 * step]) {
                    result.set(i); result.set(i + step); result.set(i + 2 * step);
                }
            }
        }
    }
    return result;
}

/**
 * Asserts the final invariants of this ordinary-only suite: no matches, no holes
 * or letters, and a legal next swap. Uses the independent triple oracle rather
 * than trusting the production detector for both execution and verification.
 */
void stable(const Board& board) {
    require(oracle(board.tiles()).none(), "Final board contains matches");
    require(board.hasLegalSwap(), "Final board has no moves");
    for (const auto tile : board.tiles()) require(static_cast<unsigned>(tile) < 5, "Final board has invalid/empty cells");
}

// Returns the first legal pair using only the ordinary-tile oracle; it does not
// execute a production swap or advance RNG merely to choose the tested input.
std::pair<Cell, Cell> legalPair(const Board& board) {
    for (int r = 0; r < 7; ++r) {
        for (int c = 0; c < 7; ++c) {
            for (const Cell delta : {Cell{0, 1}, Cell{1, 0}}) {
                Cell other{r + delta.row, c + delta.column};
                if (other.row >= 7 || other.column >= 7) continue;
                auto tiles = board.tiles();
                std::swap(tiles[r * 7 + c], tiles[other.row * 7 + other.column]);
                if (oracle(tiles).any()) return {{r, c}, other};
            }
        }
    }
    throw std::runtime_error("Oracle found no legal move");
}

/**
 * Exercises complete horizontal/vertical runs of lengths three through seven,
 * then an intersection. Exact mask-based removal protects simultaneous clearing
 * from regressions that remove only a triple or clear a shared center twice.
 */
void detectionAndRemoval() {
    for (const bool vertical : {false, true}) {
        for (int length = 3; length <= 7; ++length) {
            auto tiles = base();
            for (int i = 0; i < length; ++i) tiles[vertical ? i * 7 : i] = TileType::Star;
            Board board(tiles);
            require(board.findMatches() == oracle(tiles), "Long horizontal/vertical match detection failed");
            const auto mask = board.findMatches();
            require(mask.count() >= static_cast<unsigned>(length), "Long run incomplete");
            require(board.removeMatches() == static_cast<int>(mask.count()), "Removal count incorrect");
            for (int i = 0; i < 49; ++i)
                require(board.tiles()[i] == (mask[i] ? TileType::Empty : tiles[i]), "Removal changed unmatched tile");
        }
    }
    // Intersect two three-runs. Their shared center belongs to one removal mask,
    // so exactly five cells clear simultaneously rather than six sequential clears.
    auto tiles = base();
    for (int i = 1; i <= 3; ++i) {
        tiles[2 * 7 + i] = TileType::Berry;
        tiles[i * 7 + 2] = TileType::Berry;
    }
    Board cross(tiles);
    require(cross.findMatches() == oracle(tiles) && cross.findMatches().count() == 5, "Overlapping matches not merged");
    require(cross.removeMatches() == 5 && !cross.hasMatches(), "Overlap not removed simultaneously");
    tiles.fill(TileType::Empty);
    require(!Board(tiles).hasMatches(), "Empty cells treated as matches");
}

/**
 * Uses holes at separated heights, a fully empty column and intact neighbors to
 * isolate compaction order. Twin boards with the same RNG state then prove refill
 * changes only holes and consumes identical draws; a second refill is a no-op.
 */
void gravityAndRefill() {
    auto tiles = base();
    for (const int r : {0, 2, 5, 6}) tiles[r * 7] = TileType::Empty;
    for (int r = 0; r < 7; ++r) tiles[r * 7 + 1] = TileType::Empty;
    Board board(tiles, 91);
    board.applyGravity();
    for (int r = 0; r < 4; ++r) require(board.at(r, 0) == TileType::Empty, "Gravity left internal gap");
    for (int i = 0; i < 3; ++i) require(board.at(4 + i, 0) == tiles[(i == 0 ? 1 : i + 2) * 7], "Gravity changed survivor order");
    for (int r = 0; r < 7; ++r) {
        require(board.at(r, 1) == TileType::Empty, "Empty column gravity failed");
        for (int c = 2; c < 7; ++c) require(board.at(r,c) == tiles[r * 7 + c], "Gravity changed intact column");
    }
    // Copy after compaction but before refill: equal tile/RNG state must consume
    // equal draws, while existing survivors must never be regenerated.
    auto twin = board;
    const auto fallen = board.tiles();
    board.refill(); twin.refill();
    require(board.tiles() == twin.tiles(), "Refill not reproducible");
    for (int i = 0; i < 49; ++i) {
        require(static_cast<unsigned>(board.tiles()[i]) < 5, "Refill left empty cell");
        if (fallen[i] != TileType::Empty) require(board.tiles()[i] == fallen[i], "Refill changed survivor");
    }
    const auto filled = board.tiles();
    board.refill();
    require(board.tiles() == filled && board.moves() == 0, "Refill changed full board or moves");
}

/**
 * Builds one known legal exchange and transposes it to cover both axes. The fixed
 * seed pins post-clear refill, while legality follows the explicit starting cells.
 * A separate equal-piece pair protects rejection without mutation.
 */
void explicitSwaps() {
    // Swap a berry into the middle of a horizontal pair; transpose for vertical.
    for (const bool transpose : {false, true}) {
        auto tiles = base();
        const auto index = [=](int r, int c) { return transpose ? c * 7 + r : r * 7 + c; };
        tiles[index(0, 0)] = tiles[index(0, 2)] = tiles[index(1, 1)] = TileType::Berry;
        tiles[index(0, 1)] = TileType::Acorn;
        Board board(tiles, 42);
        require(oracle(tiles).none(), "Swap fixture starts matched");
        const Cell first = transpose ? Cell{1, 0} : Cell{0, 1};
        const Cell second{1, 1};
        require(board.trySwap(first, second).valid, "Explicit horizontal/vertical swap failed");
        require(board.moves() == 1, "Explicit valid swap not counted once");
        stable(board);
    }
    auto tiles = base();
    tiles[0] = tiles[1] = TileType::Berry;
    Board equal(tiles);
    require(!equal.trySwap({0,0},{0,1}).valid && equal.tiles() == tiles,
            "Identical-tile swap accepted or changed state");
}

/**
 * Runs the same accepted sequence on seeded twins while only one receives invalid
 * attempts. Equality after each resolved turn exposes RNG consumption or hidden
 * state changes caused by rejection, including cascades and recovery outcomes.
 */
void swapsAndReplays() {
    bool cascadeSeen = false;
    for (unsigned seed = 0; seed < 40; ++seed) {
        auto board = Board::generate(seed);
        auto replay = Board::generate(seed);
        for (int move = 0; move < 40; ++move) {
            const auto before = board.tiles();
            const auto count = board.moves();
            for (const auto pair : {std::pair<Cell, Cell>{{0,0},{0,2}}, {{0,0},{1,1}},
                                    {{0,0},{0,0}}, {{-1,0},{0,0}}, {{6,6},{7,6}}}) {
                require(!board.trySwap(pair.first,pair.second).valid, "Invalid coordinates/adjacency accepted");
                require(board.tiles() == before && board.moves() == count, "Invalid swap mutated board");
            }
            // Find an adjacent pair that does not match and verify exact rollback.
            bool rejected = false;
            for (int r = 0; r < 7 && !rejected; ++r) {
                for (int c = 0; c < 6 && !rejected; ++c) {
                    auto swapped = before;
                    std::swap(swapped[r * 7 + c], swapped[r * 7 + c + 1]);
                    if (oracle(swapped).none()) {
                        require(!board.trySwap({r,c},{r,c+1}).valid, "Nonmatching swap accepted");
                        require(board.tiles() == before && board.moves() == count, "Nonmatching swap changed board");
                        rejected = true;
                    }
                }
            }
            // Only one twin receives rejected attempts. Matching the next accepted
            // result proves that rejection preserves the random stream as well as tiles.
            const auto pair = legalPair(board);
            const auto result = board.trySwap(pair.first,pair.second);
            const auto again = replay.trySwap(pair.first,pair.second);
            require(result.valid && again.valid, "Legal adjacent swap rejected");
            require(board.moves() == count + 1, "Valid move count incorrect");
            require(board.tiles() == replay.tiles() && board.moves() == replay.moves(), "Replay/RNG diverged after rejected moves");
            require(result.resolution.waves == again.resolution.waves && result.resolution.reshuffled == again.resolution.reshuffled, "Resolution replay diverged");
            require(result.resolution.waves >= 1, "Swap failed to resolve initial match");
            cascadeSeen |= result.resolution.waves > 1;
            stable(board);
        }
    }
    require(cascadeSeen, "Replay coverage did not exercise cascades");
}

/**
 * Starts from a stable dead pattern, checks deterministic recovery and its normal
 * tile multiset preservation, then confirms recovery is skipped when playable.
 * The final fixture forces a cascade through survivors rather than refill luck.
 */
void recovery() {
    auto tiles = base();
    Board board(tiles, 73), twin(tiles, 73);
    require(!board.hasLegalSwap(), "Dead fixture is playable");
    require(board.recoverIfNoMoves() && twin.recoverIfNoMoves(), "Dead-board recovery did not run");
    require(board.tiles() == twin.tiles() && board.moves() == 0, "Recovery is not deterministic or spent a move");
    auto original = tiles;
    auto shuffled = board.tiles();
    std::sort(original.begin(), original.end());
    std::sort(shuffled.begin(), shuffled.end());
    require(original == shuffled, "Normal reshuffle changed tile multiset");
    stable(board);
    const auto before = board.tiles();
    require(!board.recoverIfNoMoves() && board.tiles() == before, "Recovery changed a playable board");
    const auto pair = legalPair(board);
    board.trySwap(pair.first,pair.second); twin.trySwap(pair.first,pair.second);
    require(board.tiles() == twin.tiles(), "RNG diverged after recovery");

    Board automatic(tiles, 73);
    const auto recoveryResult = automatic.resolve();
    require(recoveryResult.reshuffled && recoveryResult.waves == 0 && automatic.moves() == 0,
            "Resolution did not automatically recover a dead board");
    stable(automatic);

    // Force a second match via falling survivors, independently of refill luck.
    auto falling = base();
    falling[4 * 7] = falling[5 * 7] = falling[6 * 7] = TileType::Star;
    falling[3 * 7] = TileType::Berry;
    falling[6 * 7 + 1] = falling[6 * 7 + 2] = TileType::Berry;
    Board cascade(falling, 123);
    require(cascade.resolve().waves >= 2, "Falling survivors failed to cascade");
    stable(cascade);
    require(cascade.moves() == 0, "Standalone resolution spent a player move");
}

int main() {
    try {
        detectionAndRemoval(); gravityAndRefill(); explicitSwaps(); swapsAndReplays(); recovery();
        std::cout << "Gameplay fixtures and 1600 deterministic moves passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}

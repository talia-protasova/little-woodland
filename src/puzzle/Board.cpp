#include "Board.h"

#include <random>
#include <stdexcept>
#include <utility>

namespace woodland {

Board::Board(Tiles tiles) : tiles_(tiles) {
    for (const auto tile : tiles_) {
        if (static_cast<unsigned>(tile) >= tileTypeCount) {
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
    int horizontal = 1;
    int vertical = 1;
    for (int c = column - 1; c >= 0 && at(row, c) == tile; --c) ++horizontal;
    for (int c = column + 1; c < size && at(row, c) == tile; ++c) ++horizontal;
    for (int r = row - 1; r >= 0 && at(r, column) == tile; --r) ++vertical;
    for (int r = row + 1; r < size && at(r, column) == tile; ++r) ++vertical;
    return horizontal >= 3 || vertical >= 3;
}

bool Board::hasMatches() const {
    for (int r = 0; r < size; ++r)
        for (int c = 0; c < size; ++c)
            if (matchesAt(r, c)) return true;
    return false;
}

bool Board::hasLegalSwap() const {
    Board candidate = *this;
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            for (const Cell offset : {Cell{0, 1}, Cell{1, 0}}) {
                const int nr = r + offset.row;
                const int nc = c + offset.column;
                if (nr >= size || nc >= size || at(r, c) == at(nr, nc)) continue;
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

Board Board::generate(std::uint32_t seed) {
    std::mt19937 random(seed);
    // Define the bounded sampling explicitly for reproducibility across STL versions.
    const auto limit = std::mt19937::max() - (std::mt19937::max() % tileTypeCount);
    for (int attempt = 0; attempt < 1024; ++attempt) {
        Board board(Tiles{});
        for (int r = 0; r < size; ++r) {
            for (int c = 0; c < size; ++c) {
                TileType tile;
                do {
                    std::mt19937::result_type value;
                    do { value = random(); } while (value >= limit);
                    tile = static_cast<TileType>(value % tileTypeCount);
                } while ((c >= 2 && board.at(r, c - 1) == tile && board.at(r, c - 2) == tile)
                      || (r >= 2 && board.at(r - 1, c) == tile && board.at(r - 2, c) == tile));
                board.tiles_[r * size + c] = tile;
            }
        }
        if (board.hasLegalSwap()) return board;
    }
    throw std::runtime_error("Unable to generate a playable board after 1024 attempts");
}

} // namespace woodland

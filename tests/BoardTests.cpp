#include "puzzle/Board.h"
#include "rendering/BoardLayout.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace woodland;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Independent oracle: enumerate triples after every adjacent exchange.
bool matches(const Board::Tiles& tiles) {
    for (int r = 0; r < Board::size; ++r) {
        for (int c = 0; c < Board::size; ++c) {
            const int i = r * Board::size + c;
            if (c < 5 && tiles[i] == tiles[i + 1] && tiles[i] == tiles[i + 2]) return true;
            if (r < 5 && tiles[i] == tiles[i + 7] && tiles[i] == tiles[i + 14]) return true;
        }
    }
    return false;
}

bool legalSwap(Board::Tiles tiles) {
    for (int r = 0; r < 7; ++r) {
        for (int c = 0; c < 7; ++c) {
            const int i = r * 7 + c;
            for (int j : {c < 6 ? i + 1 : -1, r < 6 ? i + 7 : -1}) {
                if (j < 0) continue;
                std::swap(tiles[i], tiles[j]);
                const bool found = matches(tiles);
                std::swap(tiles[i], tiles[j]);
                if (found) return true;
            }
        }
    }
    return false;
}

int main() {
    try {
        static_assert(Board::size == 7 && Board::tileTypeCount == 5);
        for (std::uint32_t index = 0; index < 2000; ++index) {
            const auto seed = index == 1999 ? UINT32_MAX : index;
            const auto first = Board::generate(seed);
            const auto second = Board::generate(seed);
            Board::Tiles tiles{};
            for (int r = 0; r < 7; ++r) {
                for (int c = 0; c < 7; ++c) {
                    const auto tile = first.at(r, c);
                    check(static_cast<unsigned>(tile) < 5, "Tile type outside palette");
                    check(tile == second.at(r, c), "Seed is not reproducible");
                    tiles[r * 7 + c] = tile;
                }
            }
            check(!matches(tiles) && !first.hasMatches(), "Generated automatic match");
            check(legalSwap(tiles) && first.hasLegalSwap(), "Generated dead board");
        }
        Board::Tiles dead{};
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < 7; ++c)
                dead[r * 7 + c] = static_cast<TileType>((r + c) % 5);
        check(!Board(dead).hasMatches() && !Board(dead).hasLegalSwap(), "Dead-board fixture failed");
        auto horizontal = dead;
        horizontal[0] = horizontal[1] = horizontal[2] = TileType::Star;
        check(Board(horizontal).hasMatches(), "Horizontal match not detected");
        auto vertical = dead;
        vertical[0] = vertical[7] = vertical[14] = TileType::Star;
        check(Board(vertical).hasMatches(), "Vertical match not detected");
        bool threw = false;
        try { Board(dead).at(0, 7); } catch (const std::out_of_range&) { threw = true; }
        check(threw, "Invalid coordinates accepted");
        for (const auto dimensions : {Cell{480, 800}, Cell{800, 480}, Cell{960, 1600}, Cell{1, 1}}) {
            const auto layout = BoardLayout::fit(dimensions.row, dimensions.column);
            for (int r = 0; r < 7; ++r)
                for (int c = 0; c < 7; ++c) {
                    const auto cell = layout.cellAt(layout.left + (c + 0.5) * layout.cellSize(),
                                                    layout.top + (r + 0.5) * layout.cellSize());
                    check(cell && cell->row == r && cell->column == c, "Cell center mapping failed");
                }
            check(!layout.cellAt(layout.left - 1, layout.top), "Outside click accepted");
            check(!layout.cellAt(layout.left + layout.extent, layout.top), "Right edge accepted");
            check(!layout.cellAt(layout.left, layout.top + layout.extent), "Bottom edge accepted");
        }
        check(!BoardLayout::fit(0, 0).cellAt(0, 0), "Minimized layout accepted input");
        std::cout << "Board invariants passed for 2000 seeds; rule fixtures and layout checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

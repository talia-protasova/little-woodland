#include "puzzle/Board.h"
#include "rendering/BoardLayout.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace woodland;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Ordinary-tile-only oracle for generated boards: scan triples independently of
// Board::findMatches, so a bug in the production detector cannot validate itself.
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

// Probe adjacent exchanges with the independent oracle, undoing each one before
// the next. Generated fixtures contain no letters or holes, by design.
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

/**
 * Checks ordinary-board generation independently of Board detection helpers, then
 * checks coordinate and control-layout invariants without OpenGL. A broad fixed
 * seed sweep makes accidental matches/dead openings reproducible regressions.
 */
int main() {
    try {
        static_assert(Board::size == 7 && Board::tileTypeCount == 5);
        // Sweep many deterministic layouts, including the maximum seed, rather
        // than relying on one favorable opening for generation invariants.
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
        // A diagonal five-color pattern is stable but cannot make an adjacent
        // three-match, separating dead-board detection from existing-match detection.
        Board::Tiles dead{};
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < 7; ++c)
                dead[r * 7 + c] = static_cast<TileType>((r + c) % 5);
        check(!Board(dead).hasMatches() && !Board(dead).hasLegalSwap(), "Dead-board fixture failed");
        // Derive each directional run from the same known-stable pattern, so a
        // detection failure cannot be hidden by an unrelated initial match.
        auto horizontal = dead;
        horizontal[0] = horizontal[1] = horizontal[2] = TileType::Star;
        check(Board(horizontal).hasMatches(), "Horizontal match not detected");
        auto vertical = dead;
        vertical[0] = vertical[7] = vertical[14] = TileType::Star;
        check(Board(vertical).hasMatches(), "Vertical match not detected");
        bool threw = false;
        try { Board(dead).at(0, 7); } catch (const std::out_of_range&) { threw = true; }
        check(threw, "Invalid coordinates accepted");
        // Test cell centers and exclusive edges across portrait, landscape, 2x
        // dimensions and a tiny viewport; picking must follow the drawn grid.
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
        // The visible restart button and board share one responsive hit-test layout.
        for (const auto dimensions : {Cell{480,800}, Cell{800,480}, Cell{960,1600}, Cell{1600,960}, Cell{320,480}}) {
            const auto ui = GameLayout::fit(dimensions.row, dimensions.column);
            const auto& button = ui.restart;
            check(button.contains(button.x + button.width/2, button.y + button.height/2), "Restart center misses hit area");
            check(!button.contains(button.x - 1, button.y) && !button.contains(button.x + button.width, button.y),
                  "Restart accepts outside/edge clicks");
            check(!ui.board.cellAt(button.x + button.width/2, button.y + button.height/2),
                  "Restart overlaps the interactive board");
            check(ui.board.left >= 0 && ui.board.top >= 0
                  && ui.board.left + ui.board.extent <= dimensions.row
                  && ui.board.top + ui.board.extent <= dimensions.column, "Board outside resized window");
        }
        // Doubling both dimensions models a uniformly scaled framebuffer without
        // needing real high-DPI hardware or changing the logical picking contract.
        const auto normal = GameLayout::fit(480,800), retina = GameLayout::fit(960,1600);
        check(retina.board.left == normal.board.left*2 && retina.board.top == normal.board.top*2
              && retina.restart.x == normal.restart.x*2 && retina.restart.y == normal.restart.y*2,
              "Window and framebuffer hit areas disagree at 2x scale");
        check(!GameLayout::fit(0,0).restart.contains(0,0), "Minimized window accepts restart click");
        check(!BoardLayout::fit(0, 0).cellAt(0, 0), "Minimized layout accepted input");
        std::cout << "Board invariants passed for 2000 seeds; rule fixtures and layout checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#pragma once

#include <array>
#include <cstdint>

namespace woodland {

enum class TileType { Berry, Acorn, Leaf, Mushroom, Star };

struct Cell {
    int row;
    int column;
};

class Board {
public:
    static constexpr int size = 7;
    static constexpr int tileTypeCount = 5;
    using Tiles = std::array<TileType, size * size>;

    // Explicit layouts are also useful for future level definitions.
    explicit Board(Tiles tiles);
    static Board generate(std::uint32_t seed);
    TileType at(int row, int column) const;
    bool hasMatches() const;
    bool hasLegalSwap() const;

private:
    bool matchesAt(int row, int column) const;
    Tiles tiles_;
};

} // namespace woodland

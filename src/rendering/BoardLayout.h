#pragma once

#include "puzzle/Board.h"
#include <algorithm>
#include <optional>

namespace woodland {

// Coordinates use a top-left origin, in either window or framebuffer pixels.
// Identical proportions in both spaces support high-DPI mouse input.
struct BoardLayout {
    float left;
    float top;
    float extent;
    float cellSize() const { return extent / Board::size; }

    static BoardLayout fit(int width, int height) {
        const float extent = std::max(0.0f, 0.875f * std::min(width, height));
        return {(width - extent) / 2, (height - extent) / 2, extent};
    }

    std::optional<Cell> cellAt(double x, double y) const {
        if (extent <= 0 || x < left || y < top || x >= left + extent || y >= top + extent)
            return std::nullopt;
        return Cell{static_cast<int>((y - top) / cellSize()),
                    static_cast<int>((x - left) / cellSize())};
    }
};

} // namespace woodland

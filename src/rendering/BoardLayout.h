#pragma once

#include "puzzle/Board.h"
#include <algorithm>
#include <optional>

namespace woodland {

/**
 * Top-left-origin rectangle in whichever pixel space the caller supplies.
 * Right/bottom edges are exclusive, matching board hit testing.
 */
struct UiRect {
    float x, y, width, height;
    /**
     * Tests a point against a nonempty rectangle; zero-size minimized controls reject it.
     */
    bool contains(double px, double py) const {
        return width > 0 && height > 0 && px >= x && py >= y && px < x + width && py < y + height;
    }
};

/**
 * Describes the square board using top-left-origin pixel coordinates.
 *
 * The application computes this layout with GLFW window dimensions for mouse input
 * and framebuffer dimensions for rendering. Identical aspect-preserving proportions
 * keep picking aligned under uniform high-DPI scaling; the two coordinate spaces
 * must not be mixed in one calculation. Extent divided by seven preserves square cells.
 */
struct BoardLayout {
    float left;
    float top;
    float extent;
    float cellSize() const { return extent / Board::size; }
    /**
     * Returns the board portion of GameLayout so drawing and picking cannot drift apart.
     */
    static BoardLayout fit(int width, int height);
    /**
     * Maps a point to a zero-based cell by subtracting the board origin and dividing
     * by cell size. Off-board points and empty/minimized layouts return std::nullopt.
     * @param x Horizontal coordinate in the same units as this layout.
     * @param y Vertical coordinate, increasing downward.
     */
    std::optional<Cell> cellAt(double x, double y) const {
        if (extent <= 0 || x < left || y < top || x >= left + extent || y >= top + extent)
            return std::nullopt;
        return Cell{static_cast<int>((y - top) / cellSize()),
                    static_cast<int>((x - left) / cellSize())};
    }
};

/**
 * Places gameplay on a centered portrait page or a landscape board/info arrangement.
 * Owns only calculated rectangles and scale; it stores no window or game state.
 */
struct GameLayout {
    BoardLayout board;
    UiRect restart;
    float scale;
    float x, y; // Origin of the portrait page or the landscape information column.
    bool wide;
    /**
     * Fits an entire 480x800 or 800x480 reference page without stretching it.
     * Unused space becomes margins; the wide breakpoint selects a side information
     * column. Nonpositive dimensions return zero-size, noninteractive rectangles.
     */
    static GameLayout fit(int width, int height) {
        if (width <= 0 || height <= 0) return {{0,0,0}, {0,0,0,0}, 0, 0, 0, false};
        if (width > height * 1.15f) {
            const float s = std::min(width / 800.f, height / 480.f);
            const float ox = (width - 800*s) / 2;
            const float oy = (height - 480*s) / 2;
            return {{ox + 28*s, oy + 38*s, 392*s},
                    {ox + 493*s, oy + 385*s, 214*s, 40*s},
                    s, ox + 448*s, oy, true};
        }
        const float s = std::min(width / 480.f, height / 800.f);
        const float ox = (width - 480*s) / 2;
        const float oy = (height - 800*s) / 2;
        return {{ox + 30*s, oy + 192*s, 420*s},
                {ox + 330*s, oy + 720*s, 118*s, 38*s}, s, ox, oy, false};
    }
};

/**
 * Calculates the dedicated result page's illustration, single character and replay
 * button bounds. Both renderer and input routing use these rectangles so result
 * controls stay aligned after resizing and high-DPI scaling.
 */
struct ResultLayout {
    UiRect illustration;
    UiRect character;
    UiRect restart;
    float scale, x, y;
    bool wide;
    /**
     * Centers and uniformly scales the portrait or landscape result composition.
     * Character/illustration boxes are fitted proportionally by the renderer; this
     * method only defines their available space and the exact clickable button.
     */
    static ResultLayout fit(int width, int height) {
        if (width <= 0 || height <= 0) return {{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, 0, 0, 0, false};
        const bool wide = width > height * 1.15f;
        const float s = wide ? std::min(width / 800.f, height / 480.f)
                             : std::min(width / 480.f, height / 800.f);
        const float x = (width - (wide ? 800 : 480)*s)/2;
        const float y = (height - (wide ? 480 : 800)*s)/2;
        if (wide)
            return {{x+84*s,y+106*s,256*s,256*s}, {x+262*s,y+254*s,126*s,126*s},
                    {x+452*s,y+334*s,248*s,44*s}, s,x,y,true};
        return {{x+112*s,y+122*s,256*s,256*s}, {x+288*s,y+277*s,126*s,126*s},
                {x+105*s,y+660*s,270*s,48*s}, s,x,y,false};
    }
};

/**
 * Returns the session sound control's shared render/input bounds.
 * The resultPage argument is currently unused because both pages place this
 * control at the same reference-page corner; it does not imply different geometry.
 */
inline UiRect soundToggleBounds(int width,int height,bool /*resultPage*/) {
    const auto page=ResultLayout::fit(width,height);
    return {page.x+(page.wide?710:390)*page.scale,page.y+8*page.scale,80*page.scale,24*page.scale};
}

inline BoardLayout BoardLayout::fit(int width, int height) {
    return GameLayout::fit(width, height).board;
}

} // namespace woodland

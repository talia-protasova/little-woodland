#pragma once

#include "ShaderProgram.h"
#include "Texture.h"
#include "FrameAnimation.h"
#include "BoardLayout.h"
#include "presentation/BoardPlayback.h"
#include <array>
#include <memory>
#include <optional>

namespace woodland {

/**
 * Draws puzzle snapshots and the storybook UI without changing gameplay state.
 *
 * Owns the shader, reusable vertex buffer/array, tile/font textures and optional
 * character/scene animations. BoardPlayback provides the intermediate states and
 * clocks; the renderer interpolates positions/scales and preserves painter order.
 * Required tile/font failures abort construction; optional art failures are logged
 * and use available PNG fallbacks or omit the illustration. All resources must be
 * created and destroyed with the application's GL context current.
 */
class BoardRenderer {
public:
    /**
     * Loads assets from the installed/development root resolved by woodland::assetRoot
     * in platform/Assets.h.
     * @throws std::runtime_error If asset discovery or required GPU/image setup fails.
     */
    BoardRenderer();
    /**
     * Loads resources once from an explicit root containing tiles, fonts, characters
     * and backgrounds. Geometry bindings match the interleaved Vertex format.
     * @param assetRoot Asset directory, useful for installed and test configurations.
     * @throws std::runtime_error If required shaders/textures or geometry cannot be created.
     */
    explicit BoardRenderer(const std::filesystem::path& assetRoot);
    /**
     * Deletes geometry handles, then member RAII owners release textures/programs.
     * The GLFW window and its current GL context must outlive this destructor.
     */
    ~BoardRenderer();
    BoardRenderer(const BoardRenderer&) = delete;
    BoardRenderer& operator=(const BoardRenderer&) = delete;
    /**
     * Builds geometry for either gameplay or the currently visible result page.
     * Draws the active trace snapshot when present, not the already-committed final
     * Board; rendering itself never advances animation, consumes cues or draws RNG.
     * @param width Framebuffer width, not logical GLFW window width.
     * @param height Framebuffer height; nonpositive dimensions skip drawing.
     * @param level Optional level HUD/outcome data; nullptr shows a bare Board view.
     * @param restartHovered Hover calculated with the same page layout in window units.
     * @param sound Session audio status supplied by the application, not inferred here.
     * @param soundHovered Whether the shared sound-control hit area is hovered.
     * @throws std::runtime_error If generated geometry exceeds the fixed buffer capacity.
     * GL blend state, program, vertex bindings and texture unit zero are set for drawing
     * and are not restored; this renderer owns the application's frame draw stream.
     */
    void draw(const Board& board, const BoardPlayback& playback, int width, int height, const Level* level = nullptr, bool restartHovered = false, SoundStatus sound = SoundStatus::Unavailable, bool soundHovered = false) const;

private:
    ShaderProgram shader_;
    std::array<Texture, Board::tileTypeCount + 1> tiles_;
    Texture white_;
    Texture serif_;
    Texture sans_;
    std::array<std::unique_ptr<Texture>, 5> hedgehogs_;
    std::unique_ptr<Texture> postOffice_;
    std::unique_ptr<FrameAnimation> happy_, worried_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};

} // namespace woodland

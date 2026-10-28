#include "BoardRenderer.h"
#include "FontMetrics.h"
#include "platform/Assets.h"
#include <cmath>

#include <array>
#include <cstddef>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>

namespace woodland {
namespace {
// Geometry is converted to clip space on the CPU. The shader carries tint, UVs
// and an optional illustration-edge feather through the same sprite/font pipeline.
#ifdef __EMSCRIPTEN__
#define WOODLAND_GLSL "#version 300 es\nprecision highp float;\n"
#else
#define WOODLAND_GLSL "#version 330 core\n"
#endif
const char* vertexSource = WOODLAND_GLSL R"(
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in float feather;
out vec4 tileColor;
out float edgeFeather;
out vec2 uv;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    tileColor = color;
    uv = texCoord;
    edgeFeather = feather;
}
)";
// Textures already contain premultiplied RGB. Premultiply only the tint opacity
// here, so page fades preserve the blend convention instead of darkening edges.
const char* fragmentSource = WOODLAND_GLSL R"(
in vec4 tileColor;
in float edgeFeather;
in vec2 uv;
uniform sampler2D sprite;
out vec4 FragColor;
void main() {
    float edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float vignette = mix(1.0, smoothstep(0.0, 0.08, edge), edgeFeather);
    FragColor = texture(sprite, uv) * vec4(tileColor.rgb * tileColor.a, tileColor.a) * vignette;
}
)";
/** Straight tint/opacity; the fragment shader premultiplies its alpha modulation. */
struct Color { float r, g, b; float a = 1; };
/** Interleaved clip-space position, tint, atlas UV and illustration-feather weight. */
struct Vertex { float x, y; Color color; float u, v, feather; };
/** Consecutive vertices sharing a texture, kept in their original painter order. */
struct Batch { GLuint texture; int first; int count; };
// Capacity covers board sprites, paper flecks, font glyphs and UI rectangles.
constexpr int maxVertices = 5000 * 6;

constexpr Color paper{0.956f, 0.930f, 0.871f};
constexpr Color ink{0.22f, 0.27f, 0.20f};
constexpr Color muted{0.44f, 0.44f, 0.36f};
constexpr Color sage{0.80f, 0.83f, 0.72f};
constexpr Color rust{0.63f, 0.36f, 0.25f};
} // namespace

BoardRenderer::BoardRenderer() : BoardRenderer(assetRoot()) {}

BoardRenderer::BoardRenderer(const std::filesystem::path& assetRoot)
    : shader_(vertexSource, fragmentSource),
      tiles_{{Texture(assetRoot / "tiles/berry.png"), Texture(assetRoot / "tiles/acorn.png"),
              Texture(assetRoot / "tiles/leaf.png"), Texture(assetRoot / "tiles/mushroom.png"),
              Texture(assetRoot / "tiles/star.png"), Texture(assetRoot / "tiles/letter.png")}},
      serif_(assetRoot / "fonts/serif.png"), sans_(assetRoot / "fonts/sans.png") {
    // Member initialization loads required tiles/fonts first. Optional artwork can
    // fail independently; a partial animation constructor releases its loaded frames
    // before these catches select a PNG fallback or omit missing scene art.
    const std::array<const char*,5> poses{{"neutral", "happy", "worried", "celebrating", "letter"}};
    for (std::size_t i = 0; i < poses.size(); ++i) {
        const auto path = assetRoot / "characters" / (std::string("hedgehog_") + poses[i] + ".png");
        try { hedgehogs_[i] = std::make_unique<Texture>(path); }
        catch (const std::exception& error) { std::clog << "Optional character omitted: " << error.what() << '\n'; }
    }
    try { postOffice_ = std::make_unique<Texture>(assetRoot / "backgrounds/post_office.png"); }
    catch (const std::exception& error) { std::clog << "Optional post office omitted: " << error.what() << '\n'; }
    try { happy_ = std::make_unique<FrameAnimation>(assetRoot / "characters/animations/happy"); }
    catch(const std::exception& error){std::clog<<"Happy animation unavailable; using PNG: "<<error.what()<<'\n';}
    try { worried_ = std::make_unique<FrameAnimation>(assetRoot / "characters/animations/worried"); }
    catch(const std::exception& error){std::clog<<"Worried animation unavailable; using PNG: "<<error.what()<<'\n';}
    // One reusable buffer holds this frame's ordered quads. The vertex array records
    // the offsets once, so each draw only needs to replace the populated buffer range.
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    if (!vao_ || !vbo_) {
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("Failed to create board geometry");
    }
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, maxVertices * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, feather)));
    glEnableVertexAttribArray(3);
    // Sampler uniforms default to texture unit zero; draw binds that unit explicitly.
}

BoardRenderer::~BoardRenderer() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void BoardRenderer::draw(const Board& board, const BoardPlayback& playback,
                         int width, int height, const Level* level, bool restartHovered, SoundStatus sound, bool soundHovered) const {
    // A minimized framebuffer has no meaningful cell size or clip-space divisor.
    // Return before layout, geometry construction or any per-frame GPU upload.
    if (width <= 0 || height <= 0) return;
    const auto ui = GameLayout::fit(width, height);
    const auto layout = ui.board;
    const float s = ui.scale;
    std::array<Vertex, maxVertices> vertices{};
    int count = 0;
    float opacity = 1;
    std::vector<Batch> batches;
    batches.reserve(128);
    // Merge only adjacent uses of the same texture. Globally sorting sprites would
    // break transparent layering between selection, illustrations and UI text.
    const auto quad = [&](float x, float y, float w, float h, Color color, GLuint texture,
                          float u0, float v0, float u1, float v1, float feather = 0) {
        if (w <= 0 || h <= 0) return;
        color.a *= opacity;
        // UI coordinates start at the top-left; clip space starts at the center and
        // points upward. Converting in one place keeps every primitive consistent.
        const float left = 2 * x / width - 1;
        const float right = 2 * (x + w) / width - 1;
        const float top = 1 - 2 * y / height;
        const float bottom = 1 - 2 * (y + h) / height;
        if (count + 6 > maxVertices) throw std::runtime_error("Board geometry capacity exceeded");
        // Quads need six triangle vertices. Keep each batch tied to its exact
        // first/count range; changing a texture may not reorder translucent content.
        if (batches.empty() || batches.back().texture != texture)
            batches.push_back({texture, count, 0});
        batches.back().count += 6;
        for (const Vertex vertex : {Vertex{left, top, color, u0, v0, feather}, Vertex{left, bottom, color, u0, v1, feather},
             Vertex{right, bottom, color, u1, v1, feather}, Vertex{left, top, color, u0, v0, feather},
             Vertex{right, bottom, color, u1, v1, feather}, Vertex{right, top, color, u1, v0, feather}})
            vertices[count++] = vertex;
    };
    // The white texel lets flat UI geometry use the same shader and batch logic.
    const auto rectangle = [&](float x, float y, float w, float h, Color color) {
        quad(x, y, w, h, color, white_.id(), 0, 0, 1, 1);
    };
    const auto sprite = [&](const Texture& texture, float x, float y, float w, float h, bool clip, bool feather = false) {
        const float left = clip ? std::max(x, layout.left) : x;
        const float top = clip ? std::max(y, layout.top) : y;
        const float right = clip ? std::min(x + w, layout.left + layout.extent) : x + w;
        const float bottom = clip ? std::min(y + h, layout.top + layout.extent) : y + h;
        if (w <= 0 || h <= 0 || right <= left || bottom <= top) return;
        // Crop UVs with geometry; incoming sprites reveal naturally at the board edge.
        quad(left, top, right - left, bottom - top, {1, 1, 1}, texture.id(),
             (left - x) / w, (top - y) / h, (right - x) / w, (bottom - y) / h, feather ? 1.f : 0.f);
    };
    // FontMetrics.h is generated alongside the two atlases, not edited by hand.
    // Rectangles/bearings/advances use the baked 64px font scale: UVs choose the ink,
    // left/top place that ink relative to a baseline, and advance moves the pen.
    // Centering measures advances (including spaces), not just visible glyph widths.
    // This is printable ASCII layout without kerning/shaping; unsupported bytes use ?.
    const auto text = [&](const std::string& value, float x, float baseline, float size,
                          Color color, bool heading, bool centered) {
        const auto& glyphs = heading ? serifGlyphs : sansGlyphs;
        const auto& atlas = heading ? serif_ : sans_;
        // Metrics use baked pixel units; UI size and framebuffer scale determine
        // the final pen advance and ink placement, not a fixed-width glyph cell.
        const float ratio = size / 64.f;
        float advance = 0;
        for (unsigned char ch : value) advance += glyphs[(ch >= 32 && ch < 127 ? ch : '?') - 32].advance * ratio;
        if (centered) x -= advance / 2;
        for (unsigned char ch : value) {
            const auto& g = glyphs[(ch >= 32 && ch < 127 ? ch : '?') - 32];
            quad(x + g.left*ratio, baseline - g.top*ratio, g.width*ratio, g.height*ratio,
                 color, atlas.id(), g.x / atlas.width(), g.y / atlas.height(),
                 (g.x + g.width) / atlas.width(), (g.y + g.height) / atlas.height());
            x += g.advance * ratio;
        }
    };
    const auto drawSound = [&](bool resultPage) {
        const auto bounds=soundToggleBounds(width,height,resultPage);
        const auto oldOpacity=opacity;
        // Sound is a session control, so its availability stays readable throughout
        // the outgoing/incoming page fade instead of disappearing with scene artwork.
        opacity=1;
        const bool enabled=sound!=SoundStatus::Unavailable;
        rectangle(bounds.x,bounds.y,bounds.width,bounds.height,
                  enabled && soundHovered ? sage : paper);
        text(sound==SoundStatus::On ? "Sound On" : sound==SoundStatus::Muted ? "Muted" : "No audio",
             bounds.x+bounds.width/2,bounds.y+bounds.height*.65f,10*s,
             enabled ? ink : muted,false,true);
        opacity=oldOpacity;
    };
    const auto flush = [&] {
        shader_.use();
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        // Upload only this frame's generated geometry. Texture pixels and authored
        // character frames remain resident; animation never reuploads their images.
        glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(Vertex), vertices.data());
        glEnable(GL_BLEND);
        // RGB already contains alpha from Texture and shader tint modulation.
        // Multiplying by source alpha again would produce dark sprite/font fringes.
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        for (const auto& batch : batches) {
            glBindTexture(GL_TEXTURE_2D, batch.texture);
            glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
        }
    };
    // Draw paper/flecks first; the opaque gameplay board covers the flecks beneath it.
    rectangle(0, 0, width, height, paper);
    for (int i = 0; i < 300; ++i) {
        const float x = std::fmod(i * 137.37f, static_cast<float>(width));
        const float y = std::fmod(i * 79.73f, static_cast<float>(height));
        rectangle(x, y, std::max(0.5f, s * 0.8f), std::max(0.5f, s * 0.6f), {0.925f, 0.896f, 0.833f});
    }
    const auto* step = playback.current();
    const float progress = playback.progress();
    // Smooth interpolation affects visual positions/scales only. Stage durations
    // and sound entries still come from the uneased playback timeline.
    const float eased = progress * progress * (3 - 2 * progress);
    const bool reshuffling = step && step->stage == Board::Stage::Reshuffle;
    const bool victory = level && (level->state() == GameState::Won || (step && step->stage == Board::Stage::Victory));
    const bool defeat = level && (level->state() == GameState::Lost || (step && step->stage == Board::Stage::Defeat));
    const bool locked = !playback.acceptsInput() || (level && !level->acceptsInput());
    const auto result = level ? playback.result(*level) : std::nullopt;
    // A dedicated result composition replaces gameplay rather than layering a
    // second scene/character over it. Input uses the same page-visibility predicate.
    if (level && playback.showingResult(*level)) {
        const auto page = ResultLayout::fit(width, height);
        const float r = page.scale;
        const bool won = *result == GameState::Won;
        if (step) {
            const float t = (progress - BoardPlayback::resultPageStart) / (1 - BoardPlayback::resultPageStart);
            opacity = t*t*(3-2*t);
        }
        const auto pageText = [&](const std::string& value, float x, float baseline, float size,
                                  Color color = ink, bool heading = false, bool centered = false) {
            text(value, page.x + x*r, page.y + baseline*r, size*r, color, heading, centered);
        };
        const auto fitPicture = [&](const Texture& image, UiRect box, bool feather) {
            // Fit inside both box dimensions, then center the spare margin.
            // Illustration and character canvases keep their own aspect ratios.
            const float fit = std::min(box.width / image.width(), box.height / image.height());
            const float w = image.width()*fit, h = image.height()*fit;
            sprite(image, box.x + (box.width-w)/2, box.y + (box.height-h)/2, w,h,false,feather);
        };
        if (postOffice_) fitPicture(*postOffice_, page.illustration, true);
        const int pose = won ? 1 : 2;
        // Prefer authored frame textures, then the corresponding happy/worried PNG,
        // then neutral. Only the selected texture is drawn in the character box.
        const auto* animation=won ? happy_.get() : worried_.get();
        const auto* character=animation ? &animation->frame(playback.resultSeconds())
            : hedgehogs_[pose] ? hedgehogs_[pose].get() : hedgehogs_[0].get();
        if (character) {
            auto bounds = page.character;
            if (!postOffice_) {
                bounds = page.illustration;
                bounds.x += bounds.width*0.125f;
                bounds.width *= 0.75f;
            }
            // The authored GIF supplies all motion; its full canvas keeps the same placement.
            fitPicture(*character, bounds, false);
        }
        const float center = page.wide ? 576 : 240;
        pageText(won ? "C H A P T E R   C O M P L E T E" : "T H E   S T O R Y   C O N T I N U E S",
                 center, page.wide ? 61 : 47, 10, muted, false, true);
        pageText("Chapter 1  /  Woodland Post", center, page.wide ? 88 : 77, 12, muted, false, true);
        if (page.wide) pageText("Woodland Post", 212, 66, 23, ink, true, true);
        pageText(won ? "Safe in the post!" : "One more try?", center, page.wide ? 153 : 459,
                 page.wide ? 29 : 34, ink, true, true);
        pageText(won ? "Every letter has found its way home." : "Some letters still need a little help.",
                 center, page.wide ? 189 : 496, page.wide ? 12 : 14, muted, false, true);
        rectangle(page.x + (center-38)*r, page.y + (page.wide ? 212 : 530)*r, 76*r,r,
                  {0.69f,0.68f,0.55f});
        const float left = page.wide ? 442 : 84;
        const float right = page.wide ? 710 : 396;
        const float firstLine = page.wide ? 252 : 573;
        pageText("Letters delivered", left, firstLine, 14, muted);
        pageText(std::to_string(board.delivered()) + " / " + std::to_string(level->definition().requiredLetters),
                 right, firstLine+2, 23, ink, true, true);
        pageText("Moves used", left, firstLine+41, 14, muted);
        pageText(std::to_string(board.moves()), right, firstLine+43, 23, ink, true, true);
        const auto& button = page.restart;
        rectangle(button.x,button.y,button.width,button.height,restartHovered ? ink : sage);
        text(won ? "Play Again" : "Try Again", button.x + button.width/2,
             button.y + button.height*0.64f,16*r,restartHovered ? paper : ink,false,true);
        pageText(won ? "A small kindness, safely delivered." : "There is always another path home.",
                 center,page.wide ? 411 : 746,12,muted,false,true);
        pageText("R to restart  /  Esc to close",center,page.wide ? 451 : 781,10,muted,false,true);
        drawSound(true);
        flush();
        return;
    }
    // Fade the last settled gameplay view into the warm page before showing results.
    if (result) {
        const float t = progress / BoardPlayback::resultPageStart;
        opacity = 1 - t*t*(3-2*t);
    }
    rectangle(layout.left - 6*s, layout.top - 6*s, layout.extent + 12*s, layout.extent + 12*s,
              {0.68f, 0.71f, 0.59f});
    rectangle(layout.left - 4*s, layout.top - 4*s, layout.extent + 8*s, layout.extent + 8*s,
              {0.88f, 0.88f, 0.79f});
    rectangle(layout.left, layout.top, layout.extent, layout.extent,
              reshuffling ? Color{0.83f, 0.80f, 0.66f} : Color{0.85f, 0.87f, 0.77f});
    const float cell = layout.cellSize();
    for (int r = 0; r < Board::size; ++r)
        for (int c = 0; c < Board::size; ++c)
            rectangle(layout.left + c*cell + s, layout.top + r*cell + s, cell - 2*s, cell - 2*s,
                      (r+c)%2 ? Color{0.89f,0.90f,0.82f} : Color{0.91f,0.91f,0.84f});
    // Clip incoming tiles at the board edge so they never cover the HUD.
    const auto clipped = [&](float x, float y, float extent, Color color) {
        const float left = std::max(x, layout.left);
        const float top = std::max(y, layout.top);
        const float right = std::min(x + extent, layout.left + layout.extent);
        const float bottom = std::min(y + extent, layout.top + layout.extent);
        rectangle(left, top, right - left, bottom - top, color);
    };
    const auto tile = [&](TileType type, float row, float column, float scale, bool highlight) {
        if (type == TileType::Empty || scale <= 0) return;
        // Rows/columns may be fractional during a recorded motion. Convert the
        // cell center to framebuffer pixels before fitting the sprite canvas.
        const float centerX = layout.left + (column + 0.5f) * cell;
        const float centerY = layout.top + (row + 0.5f) * cell;
        // Draw the border and backing first so selection remains visible around
        // transparent artwork without changing the sprite or its logical cell.
        if (highlight) {
            const float border = cell * 0.96f * scale;
            clipped(centerX - border / 2, centerY - border / 2, border, rust);
            const float inner = cell * 0.88f * scale;
            clipped(centerX - inner / 2, centerY - inner / 2, inner, {0.98f, 0.93f, 0.77f});
        }
        // Texture slots follow TileType enum order, including the nonmatchable
        // letter after the ordinary palette. Empty was rejected before indexing.
        const auto& texture = tiles_[static_cast<int>(type)];
        // Use the larger source dimension to fit the whole image within padded
        // square cells. Uniform scaling preserves rectangular PNG proportions too.
        const float fit = cell * 0.82f * scale / std::max(texture.width(), texture.height());
        const float w = texture.width() * fit;
        const float h = texture.height() * fit;
        sprite(texture, centerX - w / 2, centerY - h / 2, w, h, true);
    };
    // Start from the step's BEFORE snapshot. Suppress each moving source so the
    // same piece is not drawn both at its old cell and along its interpolated path.
    // Negative refill sources are already outside the board and need no mask entry.
    Board::Matches movingSources;
    if (step) {
        for (const auto& motion : step->motions)
            if (motion.from.row >= 0) movingSources.set(motion.from.row * Board::size + motion.from.column);
    }
    // Reshuffle changes layouts at its zero-scale midpoint. Ordinary stages keep
    // their before snapshot visible while movement/removal communicates the change.
    const auto& visible = step ? (reshuffling && progress >= 0.5f ? step->after : step->before) : board.tiles();
    for (int r = 0; r < Board::size; ++r) {
        for (int c = 0; c < Board::size; ++c) {
            const int i = r * Board::size + c;
            if (movingSources.test(i)) continue;
            const bool matched = step && step->matched.test(i);
            float scale = 1;
            // Highlighted matched/delivered cells shrink out before subsequent
            // gravity/refill steps reveal their after snapshots.
            if (step && (step->stage == Board::Stage::Removal || step->stage == Board::Stage::Delivery) && matched) scale = 1 - eased;
            // Recovery hides its discrete layout change at the shrink midpoint;
            // ordinary removal/delivery shrink only the cells marked by that step.
            if (reshuffling) scale = progress < 0.5f ? 1 - 2 * progress : 2 * progress - 1;
            const auto selected = playback.selected();
            const bool highlight = matched || (selected && selected->row == r && selected->column == c);
            tile(visible[i], static_cast<float>(r), static_cast<float>(c), scale, highlight);
        }
    }
    // Moving pieces are drawn after stationary cells from the same snapshot.
    // Swap/return paths, compacted survivors and off-board refill sources all use
    // the same interpolation; sprite clipping reveals incoming art at the edge.
    if (step) {
        for (const auto& motion : step->motions) {
            // These paths were recorded during rule execution, preserving survivor
            // order even when several equal-looking pieces occupy the same column.
            const float row = motion.from.row + (motion.to.row - motion.from.row) * eased;
            const float column = motion.from.column + (motion.to.column - motion.from.column) * eased;
            tile(motion.tile, row, column, 1, false);
        }
    }
    for (int c = 0; c < Board::size; ++c) {
        if (!board.exits()[c]) continue;
        const float x = layout.left + c * cell;
        const float y = layout.top + layout.extent;
        // Exit feedback follows the current delivery mask, not the already-final
        // model count, so one visible flash accompanies each recorded exit stage.
        const bool delivering = step && step->stage == Board::Stage::Delivery
            && step->matched[(Board::size - 1) * Board::size + c];
        rectangle(x + cell * 0.08f, y, cell * 0.84f, cell * 0.23f,
                  delivering ? Color{0.94f, 0.70f, 0.19f} : Color{0.58f, 0.32f, 0.16f});
        rectangle(x + cell * 0.18f, y + cell * 0.06f, cell * 0.64f, cell * 0.05f,
                  {0.98f, 0.92f, 0.72f});
        if (delivering) {
            const float spread = cell * eased;
            for (int i = -2; i <= 2; ++i)
                rectangle(x + cell / 2 + i * spread * 0.3f, y + spread * 0.5f,
                          cell * 0.07f * (1 - eased), cell * 0.07f * (1 - eased), {0.94f, 0.70f, 0.19f});
        }
    }
    for (int c = 0; c < Board::size; ++c)
        if (board.exits()[c])
            text("Post slot", layout.left + (c + 0.5f)*cell,
                 layout.top + layout.extent + 27*s, 9*s, rust, false, true);
    const auto label = [&](const std::string& value, float x, float y, float size,
                           Color color = ink, bool heading = false, bool center = false) {
        text(value, ui.x + x*s, ui.y + y*s, size*s, color, heading, center);
    };
    const float center = ui.wide ? 160.f : 240.f;
    label("L I T T L E   W O O D L A N D", center, ui.wide ? 36 : 27, 10, muted, false, true);
    label("Woodland Post", center, ui.wide ? 79 : 72, ui.wide ? 31 : 37, ink, true, true);
    const float lineY = ui.y + (ui.wide ? 96 : 90)*s;
    rectangle(ui.x + (center-48)*s, lineY, 96*s, s, {0.69f,0.68f,0.55f});
    label("Chapter 1  /  The first letter", center, ui.wide ? 122 : 117, 13, muted, false, true);

    const int moves = level ? level->remainingMoves() : static_cast<int>(board.moves());
    // Model deliveries may be final already; hide those whose exit animation has
    // not completed so the persistent HUD explains the visible progress change.
    const int delivered = board.delivered() - playback.pendingDeliveries();
    const int target = level ? level->definition().requiredLetters : 1;
    if (ui.wide) {
        label(level ? "Moves Left" : "Moves Made", 26, 161, 14, muted);
        label(std::to_string(moves), 278, 166, 29, moves <= 3 ? rust : ink, true, true);
    } else {
        label(level ? "Moves Left" : "Moves Made", 32, 162, 15, muted);
        label(std::to_string(moves), 149, 166, 28, moves <= 3 ? rust : ink, true);
        label(locked ? (victory ? "Delivered!" : defeat ? "Out of moves" : "Resolving...") : "Ready to match", 359, 160, 12, muted, false, true);
    }

    // A persistent objective remains visible, even while the delivery animates.
    const float objectiveX = ui.wide ? ui.x + 18*s : layout.left;
    const float objectiveY = ui.y + (ui.wide ? 189 : 645)*s;
    const float objectiveW = ui.wide ? 284*s : layout.extent;
    rectangle(objectiveX, objectiveY, objectiveW, 53*s, {0.93f,0.90f,0.82f});
    sprite(tiles_[static_cast<int>(TileType::Letter)], objectiveX + 6*s, objectiveY + 5*s, 44*s, 44*s, false);
    text("Letters delivered", objectiveX + 58*s, objectiveY + 23*s, 16*s, ink, true, false);
    text(std::to_string(delivered) + " / " + std::to_string(target), objectiveX + objectiveW - 31*s,
         objectiveY + 26*s, 23*s, ink, true, true);
    text(victory ? "Every letter found its way home." : "Guide the letter to the post slot.",
         objectiveX + 58*s, objectiveY + 43*s, (ui.wide ? 10.f : 12.f)*s, muted, false, false);

    int pose = victory ? 3 : defeat ? 2 : (step && step->stage == Board::Stage::Delivery) ? 1
        : level && level->remainingMoves() <= 3 ? 2 : 4;
    const Texture* character = hedgehogs_[pose] ? hedgehogs_[pose].get() : hedgehogs_[0].get();
    const float charX = ui.x + (ui.wide ? 18 : 32)*s;
    const float charY = ui.y + (ui.wide ? 271 : 707)*s;
    const float charBox = (ui.wide ? 91 : 76)*s;
    if (character) {
        const float fit = charBox / std::max(character->width(), character->height());
        sprite(*character, charX, charY, character->width()*fit, character->height()*fit, false);
    }
    if (ui.wide) {
        label(victory ? "A lovely delivery!" : defeat ? "Try another route?" : "A little help", 121, 304, 18, ink, true);
        label(victory ? "Thank you, dear friend." : "Match beneath the letter.", 121, 327, 11, muted);
        label(locked && !victory && !defeat ? "Please wait for the tiles." : "The forest is counting on you.", 160, 366, 11, muted, false, true);
    } else {
        label(victory ? "A lovely delivery!" : defeat ? "Try another route?" : "Every letter has a home.", 120, 735, 15, ink, true);
        label(victory ? "Thank you, dear friend." : "Match beneath the envelope.", 120, 755, 11, muted);
    }

    // A quiet, flat paper button; restart is intentionally available during playback.
    const auto& button = ui.restart;
    rectangle(button.x, button.y, button.width, button.height, restartHovered ? ink : sage);
    text("Restart", button.x + button.width/2, button.y + button.height*0.65f,
         14*s, restartHovered ? paper : ink, false, true);
    label("R to restart  /  Esc to close", center, ui.wide ? 455 : 791, 10, muted, false, true);

    drawSound(false);
    flush();
}

} // namespace woodland

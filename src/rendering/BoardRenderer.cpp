#include "BoardRenderer.h"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace woodland {
namespace {
const char* vertexSource = R"(#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
out vec3 tileColor;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    tileColor = color;
}
)";
const char* fragmentSource = R"(#version 330 core
in vec3 tileColor;
out vec4 FragColor;
void main() { FragColor = vec4(tileColor, 1.0); }
)";
struct Color { float r, g, b; };
struct Vertex { float x, y; Color color; };
constexpr std::array<Color, Board::tileTypeCount> palette{{
    {0.73f, 0.20f, 0.32f}, {0.58f, 0.32f, 0.16f}, {0.25f, 0.53f, 0.32f},
    {0.45f, 0.36f, 0.70f}, {0.94f, 0.70f, 0.19f}
}};
constexpr int maxVertices = (Board::size * Board::size + 2) * 6;
}

BoardRenderer::BoardRenderer() : shader_(vertexSource, fragmentSource) {
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
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);
}

BoardRenderer::~BoardRenderer() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void BoardRenderer::draw(const Board& board, std::optional<Cell> selected,
                         int width, int height) const {
    if (width <= 0 || height <= 0) return;
    const auto layout = BoardLayout::fit(width, height);
    std::array<Vertex, maxVertices> vertices{};
    int count = 0;
    const auto rectangle = [&](float x, float y, float w, float h, Color color) {
        const float left = 2 * x / width - 1;
        const float right = 2 * (x + w) / width - 1;
        const float top = 1 - 2 * y / height;
        const float bottom = 1 - 2 * (y + h) / height;
        for (const Vertex vertex : {Vertex{left, top, color}, Vertex{left, bottom, color},
             Vertex{right, bottom, color}, Vertex{left, top, color},
             Vertex{right, bottom, color}, Vertex{right, top, color}})
            vertices[count++] = vertex;
    };
    rectangle(layout.left, layout.top, layout.extent, layout.extent, {0.16f, 0.23f, 0.18f});
    const float cell = layout.cellSize();
    for (int r = 0; r < Board::size; ++r) {
        for (int c = 0; c < Board::size; ++c) {
            const float x = layout.left + c * cell;
            const float y = layout.top + r * cell;
            if (selected && selected->row == r && selected->column == c)
                rectangle(x + cell * 0.02f, y + cell * 0.02f,
                          cell * 0.96f, cell * 0.96f, {1.0f, 0.98f, 0.86f});
            rectangle(x + cell * 0.09f, y + cell * 0.09f, cell * 0.82f, cell * 0.82f,
                      palette[static_cast<int>(board.at(r, c))]);
        }
    }
    shader_.use();
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(Vertex), vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, count);
}

} // namespace woodland

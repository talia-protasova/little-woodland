#pragma once

#include "ShaderProgram.h"
#include "BoardLayout.h"
#include <optional>

namespace woodland {

class BoardRenderer {
public:
    BoardRenderer();
    ~BoardRenderer();
    BoardRenderer(const BoardRenderer&) = delete;
    BoardRenderer& operator=(const BoardRenderer&) = delete;
    void draw(const Board& board, std::optional<Cell> selected, int width, int height) const;

private:
    ShaderProgram shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};

} // namespace woodland

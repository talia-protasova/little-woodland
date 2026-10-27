#pragma once

#include <glad/gl.h>

class ShaderProgram {
public:
    ShaderProgram(
        const char* vertexSource,
        const char* fragmentSource
    );

    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&&) = delete;
    ShaderProgram& operator=(ShaderProgram&&) = delete;

    void use() const;

private:
    GLuint programId_ = 0;
};
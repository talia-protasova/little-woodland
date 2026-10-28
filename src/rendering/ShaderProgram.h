#pragma once

#include <glad/gl.h>

/**
 * Compiles, links and exclusively owns one OpenGL shader program.
 *
 * Temporary shader objects are released after linking; the linked program survives
 * until destruction. Copying and moving are disabled to keep ownership and context
 * lifetime explicit. Construction, use and destruction require a current compatible
 * OpenGL context, so the application destroys this owner before its GLFW window.
 */
class ShaderProgram {
public:
    /**
     * Builds a complete program from vertex/fragment GLSL source strings.
     * Compilation/link failures include driver logs and clean up allocated GL objects.
     * @throws std::runtime_error If shader/program allocation, compilation or linking fails.
     */
    ShaderProgram(        const char* vertexSource,
        const char* fragmentSource
    );

    /**
     * Releases the linked program while its OpenGL context is still valid.
     */
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&&) = delete;
    ShaderProgram& operator=(ShaderProgram&&) = delete;

    /**
     * Makes this program active for subsequent draw calls in the current context.
     * The renderer supplies matching vertex attributes and texture-unit bindings.
     */
    void use() const;

private:
    GLuint programId_ = 0;
};
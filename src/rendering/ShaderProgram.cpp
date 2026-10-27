#include "ShaderProgram.h"

#include <stdexcept>
#include <string>

namespace {

std::string getShaderLog(GLuint shader) {
    GLint length = 0;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length <= 1) {
        return "No additional information available.";
    }

    std::string log(length, '\0');
    GLsizei written = 0;

    glGetShaderInfoLog(
        shader,
        length,
        &written,
        log.data()
    );

    log.resize(written);

    return log;
}

std::string getProgramLog(GLuint program) {
    GLint length = 0;

    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    if (length <= 1) {
        return "No additional information available.";
    }

    std::string log(length, '\0');
    GLsizei written = 0;

    glGetProgramInfoLog(
        program,
        length,
        &written,
        log.data()
    );

    log.resize(written);

    return log;
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);

    if (shader == 0) {
        throw std::runtime_error(
            "Failed to create shader."
        );
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;

    glGetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &success
    );

    if (success == GL_FALSE) {
        const std::string log = getShaderLog(shader);

        glDeleteShader(shader);

        const std::string shaderType =
            type == GL_VERTEX_SHADER ? "Vertex" : "Fragment";

        throw std::runtime_error(
            shaderType + " shader compilation failed:\n" + log
        );
    }

    return shader;
}

} // namespace

ShaderProgram::ShaderProgram(
    const char* vertexSource,
    const char* fragmentSource
) {
    const GLuint vertexShader = compileShader(
        GL_VERTEX_SHADER,
        vertexSource
    );

    GLuint fragmentShader = 0;

    try {
        fragmentShader = compileShader(
            GL_FRAGMENT_SHADER,
            fragmentSource
        );

        programId_ = glCreateProgram();

        if (programId_ == 0) {
            throw std::runtime_error(
                "Failed to create shader program."
            );
        }

        glAttachShader(programId_, vertexShader);
        glAttachShader(programId_, fragmentShader);

        glLinkProgram(programId_);

        GLint success = GL_FALSE;

        glGetProgramiv(
            programId_,
            GL_LINK_STATUS,
            &success
        );

        if (success == GL_FALSE) {
            throw std::runtime_error(
                "Shader program linking failed:\n" +
                getProgramLog(programId_)
            );
        }

    } catch (...) {
        if (programId_ != 0) {
            glDeleteProgram(programId_);
            programId_ = 0;
        }

        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }

        glDeleteShader(vertexShader);

        throw;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

ShaderProgram::~ShaderProgram() {
    if (programId_ != 0) {
        glDeleteProgram(programId_);
    }
}

void ShaderProgram::use() const {
    glUseProgram(programId_);
}
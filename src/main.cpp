#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <exception>
#include <iostream>

#include "rendering/ShaderProgram.h"

// Vertex shader
const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPosition;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

// Fragment shader
const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(0.76, 0.37, 0.27, 1.0);
}
)";


int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        480,
        800,
        "Little Woodland",
        nullptr,
        nullptr
    );

    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Load OpenGL functions
    int version = gladLoadGL(glfwGetProcAddress);

    if (version == 0) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL version: "
              << GLAD_VERSION_MAJOR(version) << "."
              << GLAD_VERSION_MINOR(version) << '\n';

    glfwSwapInterval(1);

    // Configure viewport
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glfwSetFramebufferSizeCallback(
        window,
        [](GLFWwindow*, int width, int height) {
            glViewport(0, 0, width, height);
        }
    );

    // Triangle vertices
    float vertices[] = {
        -0.65f, -0.40f,
         0.65f, -0.40f,
         0.00f,  0.45f
    };

    try {
    ShaderProgram shaderProgram(
        vertexShaderSource,
        fragmentShaderSource
    );

    // Create vertex objects
    GLuint VAO, VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(float),
        nullptr
    );

    glEnableVertexAttribArray(0);

    // Game loop
    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        glClearColor(0.91f, 0.95f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw triangle
        shaderProgram.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

      // Cleanup geometry
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);

    } catch (const std::exception& error) {
        std::cerr << "Rendering error:\n"
                  << error.what() << '\n';

        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <iostream>

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

// Compile a shader and check for errors
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success != GL_TRUE) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);

        std::cerr << "Shader compilation failed:\n"
                  << infoLog << '\n';

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

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

    // Compile shaders
    GLuint vertexShader = compileShader(
        GL_VERTEX_SHADER,
        vertexShaderSource
    );

    GLuint fragmentShader = compileShader(
        GL_FRAGMENT_SHADER,
        fragmentShaderSource
    );

    if (!vertexShader || !fragmentShader) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);

        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Create shader program
    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint linked = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (linked != GL_TRUE) {
        char infoLog[1024];
        glGetProgramInfoLog(
            shaderProgram,
            sizeof(infoLog),
            nullptr,
            infoLog
        );

        std::cerr << "Shader linking failed:\n"
                  << infoLog << '\n';

        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

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
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
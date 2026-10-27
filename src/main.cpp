#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Configure OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create game window
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
    glfwSwapInterval(1);

    // Game loop
    while (!glfwWindowShouldClose(window)) {

        // Close with Escape
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Set background color
        glClearColor(0.91f, 0.95f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Display the frame
        glfwSwapBuffers(window);

        // Process input events
        glfwPollEvents();
    }

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
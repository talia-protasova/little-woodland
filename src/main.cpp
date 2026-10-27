#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "puzzle/Board.h"
#include "rendering/BoardRenderer.h"

namespace {
struct GlfwLifetime {
    GlfwLifetime() {
        if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    }
    ~GlfwLifetime() { glfwTerminate(); }
    GlfwLifetime(const GlfwLifetime&) = delete;
    GlfwLifetime& operator=(const GlfwLifetime&) = delete;
};

std::uint32_t parseSeed(int argc, char** argv) {
    if (argc == 1) return 42397;
    if (argc != 3 || std::string_view(argv[1]) != "--seed")
        throw std::invalid_argument("Usage: little_woodland [--seed <uint32>]");
    const std::string_view text(argv[2]);
    std::uint32_t seed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), seed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::invalid_argument("Seed must be an unsigned 32-bit decimal integer");
    return seed;
}
}

int main(int argc, char** argv) {
    try {
        const auto seed = parseSeed(argc, argv);
        const auto board = woodland::Board::generate(seed);
        GlfwLifetime glfw;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window(
            glfwCreateWindow(480, 800, "Little Woodland", nullptr, nullptr), glfwDestroyWindow);
        if (!window) throw std::runtime_error("Failed to create window");
        glfwMakeContextCurrent(window.get());
        const int version = gladLoadGL(glfwGetProcAddress);
        if (version == 0) throw std::runtime_error("Failed to initialize GLAD");
        std::cout << "OpenGL version: " << GLAD_VERSION_MAJOR(version) << '.'
                  << GLAD_VERSION_MINOR(version) << "\nBoard seed: " << seed << std::endl;
        glfwSwapInterval(1);

        std::optional<woodland::Cell> selected;
        glfwSetWindowUserPointer(window.get(), &selected);
        glfwSetMouseButtonCallback(window.get(), [](GLFWwindow* window, int button, int action, int) {
            if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
            double x, y;
            int width, height;
            glfwGetCursorPos(window, &x, &y);
            glfwGetWindowSize(window, &width, &height);
            auto& selection = *static_cast<std::optional<woodland::Cell>*>(glfwGetWindowUserPointer(window));
            selection = woodland::BoardLayout::fit(width, height).cellAt(x, y);
        });

        // GPU objects die before the window/context and GLFW lifetime objects.
        woodland::BoardRenderer renderer;
        while (!glfwWindowShouldClose(window.get())) {
            glfwPollEvents();
            if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
            int width, height;
            glfwGetFramebufferSize(window.get(), &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.91f, 0.95f, 0.85f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.draw(board, selected, width, height);
            glfwSwapBuffers(window.get());
        }
    } catch (const std::exception& error) {
        std::cerr << "Little Woodland error:\n" << error.what() << '\n';
        return 1;
    }
    return 0;
}

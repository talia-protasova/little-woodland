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
#include <vector>

#include "puzzle/Board.h"
#include "audio/Audio.h"
#include "platform/Assets.h"
#include "rendering/BoardRenderer.h"

namespace {
/**
 * An ordered mouse intention captured in GLFW window coordinates. Session controls
 * remain usable during board playback, while ordinary board clicks are discarded.
 */
struct Click { std::optional<woodland::Cell> cell; bool restart = false; bool sound = false; };
/**
 * Bridges GLFW's C callback to the main loop without throwing across a C boundary.
 * Owns queued clicks and an exception to rethrow later; Level/BoardPlayback pointers
 * are non-owning and stay valid because their stack objects outlive callback use.
 */
struct MouseInput {
    std::vector<Click> clicks;
    std::exception_ptr error;
    const woodland::BoardPlayback* playback = nullptr;
    const woodland::Level* level = nullptr;
};

/**
 * Balances GLFW initialization/termination and outlives all windows and GL owners.
 * Stack unwinding follows the same cleanup order after an exception.
 */
struct GlfwLifetime {
    GlfwLifetime() {
        if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    }
    ~GlfwLifetime() { glfwTerminate(); }
    GlfwLifetime(const GlfwLifetime&) = delete;
    GlfwLifetime& operator=(const GlfwLifetime&) = delete;
};

/**
 * Accepts the default seed or one complete decimal --seed uint32 argument.
 * Rejects partial parses, signs/out-of-range values and unexpected options before
 * creating platform resources. @throws std::invalid_argument For invalid CLI input.
 */
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

/**
 * Creates a seeded level and platform resources, then drives input, presentation,
 * feedback and rendering. Rules remain in Level/Board; callbacks queue intentions
 * and the main loop serializes their effects. Top-level exceptions print a useful
 * error and unwind RAII owners before returning failure.
 */
int main(int argc, char** argv) {
    try {
        const auto seed = parseSeed(argc, argv);
        // Construct deterministic rules before platform setup; tests use this same
        // level path without a window, renderer or audio device.
        woodland::Level level(woodland::LevelDefinition::demo(seed));
        // Initialize GLFW before requesting the OpenGL 3.3 core-profile window.
        GlfwLifetime glfw;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window(
            glfwCreateWindow(480, 800, "Little Woodland", nullptr, nullptr), glfwDestroyWindow);
        if (!window) throw std::runtime_error("Failed to create window");
        glfwMakeContextCurrent(window.get());
        // Driver entry points are valid only after the window's context is current.
        const int version = gladLoadGL(glfwGetProcAddress);
        if (version == 0) throw std::runtime_error("Failed to initialize GLAD");
        std::cout << "OpenGL version: " << GLAD_VERSION_MAJOR(version) << '.'
                  << GLAD_VERSION_MINOR(version) << "\nBoard seed: " << seed << std::endl;
        glfwSwapInterval(1);

        // Resolve assets once. Audio owns optional CPU/device resources; the renderer
        // later creates required GPU resources while this context is still current.
        const auto assets=woodland::assetRoot();
        woodland::Audio audio(assets);
        woodland::BoardPlayback playback;
        MouseInput mouse;
        mouse.playback = &playback;
        mouse.level = &level;
        mouse.clicks.reserve(8);
        glfwSetWindowUserPointer(window.get(), &mouse);
        glfwSetMouseButtonCallback(window.get(), [](GLFWwindow* window, int button, int action, int) {
            if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
            double x, y;
            int width, height;
            glfwGetCursorPos(window, &x, &y);
            glfwGetWindowSize(window, &width, &height);
            auto& mouse = *static_cast<MouseInput*>(glfwGetWindowUserPointer(window));

            // Keep ordered clicks even when multiple events arrive in one frame.
            // Exceptions must not cross GLFW's C callback boundary.
            try {
                const auto layout = woodland::GameLayout::fit(width, height);
                // Derive visible controls in window units, not framebuffer pixels.
                // This matches renderer proportions while supporting high-DPI input.
                const bool resultPage = mouse.playback->showingResult(*mouse.level);
                const auto restartBounds = resultPage ? woodland::ResultLayout::fit(width, height).restart : layout.restart;
                const bool restart = restartBounds.contains(x, y);
                const bool sound=woodland::soundToggleBounds(width,height,resultPage).contains(x,y);
                // Controls remain available during a turn. Gameplay clicks are
                // dropped at capture time as well as at the presentation/level gates.
                if (!restart && !sound && !mouse.playback->acceptsInput()) return;
                mouse.clicks.push_back({layout.board.cellAt(x,y),restart,sound});
            } catch (...) {
                mouse.error = std::current_exception();
            }
        });

        // Reverse stack destruction releases renderer textures/program/geometry
        // before the window/context, and stops the audio callback before its data
        // owners and GLFW disappear. Keep this ordering when adding new resources.
        woodland::BoardRenderer renderer;
        std::unique_ptr<GLFWcursor, decltype(&glfwDestroyCursor)> pointer(
            glfwCreateStandardCursor(GLFW_HAND_CURSOR), glfwDestroyCursor);
        bool restartHeld = false;
        bool muteHeld = false;
        double previousTime = glfwGetTime();
        while (!glfwWindowShouldClose(window.get())) {
            // Poll while the previous playback state is still locked, then advance its clock.
            // A click made during the last animation frame must not leak into the next move.
            glfwPollEvents();
            // Elapsed seconds drive every crossed stage and the result-character
            // clock; neither motion nor sound cue counts depend on render frequency.
            const double now = glfwGetTime();
            playback.update(level, now - previousTime);
            previousTime = now;
            if (mouse.error) std::rethrow_exception(mouse.error);
            for (const auto& clicked : mouse.clicks) {
                if (clicked.sound) { audio.toggleMute(); }
                else if (clicked.restart) {
                    // Reset rules, selection, cues and character time together.
                    // Audio persists as session state, so stop voices without unmuting.
                    audio.stop();
                    level.restart();
                    playback = woodland::BoardPlayback{};
                } else playback.click(level, clicked.cell);
            }
            mouse.clicks.clear();
            // Edge detection makes R/M session actions discrete even when a key is
            // held; Escape can remain a level-triggered request to leave the loop.
            const bool restart = glfwGetKey(window.get(), GLFW_KEY_R) == GLFW_PRESS;
            if (restart && !restartHeld) {
                audio.stop();
                level.restart();
                playback = woodland::BoardPlayback{};
                mouse.clicks.clear();
            }
            restartHeld = restart;
            const bool mute=glfwGetKey(window.get(),GLFW_KEY_M)==GLFW_PRESS;
            if(mute && !muteHeld)audio.toggleMute();
            muteHeld=mute;
            // Consume stage/selection events once after input handling. A restart
            // replaces playback first, so cues from the old level cannot leak through.
            for(const auto cue:playback.takeSoundCues())audio.play(cue);
            if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
            int width, height;
            // Drawing uses physical framebuffer pixels; cursor hit testing below
            // uses logical window pixels with the same aspect-preserving layout.
            glfwGetFramebufferSize(window.get(), &width, &height);
            glViewport(0, 0, width, height);
            double cursorX, cursorY;
            int windowWidth, windowHeight;
            glfwGetCursorPos(window.get(), &cursorX, &cursorY);
            glfwGetWindowSize(window.get(), &windowWidth, &windowHeight);
            const auto restartBounds = playback.showingResult(level)
                ? woodland::ResultLayout::fit(windowWidth, windowHeight).restart
                : woodland::GameLayout::fit(windowWidth, windowHeight).restart;
            const bool restartHovered = restartBounds.contains(cursorX, cursorY);
            const bool soundHovered=woodland::soundToggleBounds(windowWidth,windowHeight,playback.showingResult(level)).contains(cursorX,cursorY);
            glfwSetCursor(window.get(), (restartHovered || (soundHovered && audio.status()!=woodland::SoundStatus::Unavailable)) ? pointer.get() : nullptr);
            glClearColor(0.956f, 0.930f, 0.871f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.draw(level.board(), playback, width, height, &level, restartHovered, audio.status(), soundHovered);
            // Present only after the complete read-only scene has been drawn.
            glfwSwapBuffers(window.get());
        }
    } catch (const std::exception& error) {
        // This also receives callback errors rethrown by the main loop; callback
        // exceptions themselves never escape through GLFW's C interface.
        std::cerr << "Little Woodland error:\n" << error.what() << '\n';
        return 1;
    }
    return 0;
}

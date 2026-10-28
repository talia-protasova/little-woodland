#include "rendering/GL.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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
 * are non-owning and stay valid because their application owner outlives callback use.
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

/** Owns the session across browser callbacks. Member order releases GPU/audio
 * resources before the current window/context and GLFW on every shutdown path. */
class Application {
    woodland::Level level;
    GlfwLifetime glfw;
    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window{nullptr, glfwDestroyWindow};
    std::unique_ptr<woodland::Audio> audioOwner;
    woodland::BoardPlayback playback;
    MouseInput mouse;
    std::unique_ptr<woodland::BoardRenderer> rendererOwner;
    std::unique_ptr<GLFWcursor, decltype(&glfwDestroyCursor)> pointer{nullptr, glfwDestroyCursor};
    bool restartHeld = false, muteHeld = false;
    double previousTime = 0;
public:
    explicit Application(std::uint32_t seed) : level(woodland::LevelDefinition::demo(seed)) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
#ifdef __EMSCRIPTEN__
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#else
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
        window.reset(glfwCreateWindow(480, 800, "Little Woodland", nullptr, nullptr));
        if (!window) throw std::runtime_error("Failed to create graphics window (WebGL 2 required)");
        glfwMakeContextCurrent(window.get());
#ifndef __EMSCRIPTEN__
        const int version = gladLoadGL(glfwGetProcAddress);
        if (!version) throw std::runtime_error("Failed to initialize GLAD");
#endif
        std::cout << "Graphics: " << glGetString(GL_VERSION) << "\nBoard seed: " << seed << std::endl;
#ifndef __EMSCRIPTEN__
        glfwSwapInterval(1);
#endif
        audioOwner = std::make_unique<woodland::Audio>(woodland::assetRoot());
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

        rendererOwner = std::make_unique<woodland::BoardRenderer>();
#ifndef __EMSCRIPTEN__
        pointer.reset(glfwCreateStandardCursor(GLFW_HAND_CURSOR));
#endif
        previousTime = glfwGetTime();
    }
    bool running() const { return !glfwWindowShouldClose(window.get()); }
    void frame() {
        auto& audio = *audioOwner;
        auto& renderer = *rendererOwner;
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
#ifndef __EMSCRIPTEN__
        glfwSetCursor(window.get(), (restartHovered || (soundHovered && audio.status()!=woodland::SoundStatus::Unavailable)) ? pointer.get() : nullptr);
#endif
        glClearColor(0.956f, 0.930f, 0.871f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.draw(level.board(), playback, width, height, &level, restartHovered, audio.status(), soundHovered);
        // Present only after the complete read-only scene has been drawn.
        glfwSwapBuffers(window.get());
    }
};

#ifdef __EMSCRIPTEN__
// main returns to the browser; this owner keeps every callback target alive.
static std::unique_ptr<Application> application;
EM_JS(void, webReady, (), { if (Module.gameReady) Module.gameReady(); });
EM_JS(void, webStopped, (), { if (Module.gameStopped) Module.gameStopped(); });
EM_JS(void, webError, (const char* message), {
    if (Module.gameError) Module.gameError(UTF8ToString(message));
});
static void browserFrame(void*) {
    try {
        application->frame();
        if (application->running()) return;
        emscripten_cancel_main_loop();
        application.reset();
        webStopped();
    } catch (const std::exception& error) {
        // Never throw through a browser callback. Clean GPU owners up first.
        const std::string message = error.what();
        emscripten_cancel_main_loop();
        application.reset();
        webError(message.c_str());
    }
}
#endif

int main(int argc, char** argv) {
    try {
#ifdef __EMSCRIPTEN__
        application = std::make_unique<Application>(parseSeed(argc, argv));
        webReady();
        emscripten_set_main_loop_arg(browserFrame, nullptr, 0, false);
#else
        Application application(parseSeed(argc, argv));
        while (application.running()) application.frame();
#endif
    } catch (const std::exception& error) {
        std::cerr << "Little Woodland error:\n" << error.what() << '\n';
#ifdef __EMSCRIPTEN__
        webError(error.what());
#endif
        return 1;
    }
    return 0;
}

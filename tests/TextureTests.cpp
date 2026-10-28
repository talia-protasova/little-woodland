#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "rendering/Texture.h"
#include "rendering/FrameAnimation.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using woodland::Texture;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct GlfwLifetime {
    ~GlfwLifetime() { glfwTerminate(); }
};
// Keep synthetic files isolated and clean them up without throwing from a destructor,
// including when the test exits early after a decoder assertion.
struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("woodland-textures-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directory(path); }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

/**
 * Checks each real tile upload for full canvas size, retained alpha coverage,
 * premultiplied RGB and mipmap/filter configuration. Each Texture dies while its
 * GL context is still active so handle deletion is independently observable.
 */
void loadAssets() {
    for (const auto* name : {"berry", "acorn", "leaf", "mushroom", "star", "letter"}) {
        GLuint id = 0;
        {
            Texture texture(std::filesystem::path(WOODLAND_ASSET_DIR) / "tiles" / (std::string(name) + ".png"));
            id = texture.id();
            check(glIsTexture(id), "Texture not allocated");
            check(texture.width() == 256 && texture.height() == 256, "Asset dimensions changed");
            glBindTexture(GL_TEXTURE_2D, id);
            std::vector<unsigned char> pixels(256 * 256 * 4);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            bool transparent = false, opaque = false, partial = false;
            for (std::size_t i = 0; i < pixels.size(); i += 4) {
                const auto alpha = pixels[i + 3];
                transparent |= alpha == 0;
                opaque |= alpha == 255;
                partial |= alpha > 0 && alpha < 255;
                check(pixels[i] <= alpha && pixels[i + 1] <= alpha && pixels[i + 2] <= alpha,
                      "RGBA is not premultiplied");
            }
            check(transparent && opaque && partial, "Illustration lost its transparent/antialiased pixels");
            GLint filter = 0, mipWidth = 0;
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &mipWidth);
            check(filter == GL_LINEAR_MIPMAP_LINEAR && mipWidth == 128, "Downscale filtering/mipmaps missing");
        }
        // Leave the context alive after the owner dies so deletion is observable
        // rather than merely relying on the driver to clean up at context shutdown.
        check(!glIsTexture(id), "Texture was not deleted by RAII");
    }
}

/**
 * Uses an asymmetric 2x2 RGBA fixture to distinguish row reversal, missing alpha
 * and incorrect premultiplication in uploaded bytes. Missing/corrupt PNGs must
 * identify their paths, and the white UI texel must remain opaque white.
 */
void fixtureAndErrors() {
    TemporaryDirectory temporary;
    const auto fixture = temporary.path / "orientation-alpha.png";
    // Original 2x2 test data: top red/half-alpha green; bottom blue/transparent white.
    constexpr unsigned char png[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,23,73,68,65,84,120,156,5,193,1,1,0,0,0,130,32,166,247,220,64,36,67,85,14,67,211,8,121,46,66,165,54,0,0,0,0,73,69,78,68,174,66,96,130};
    {
        std::ofstream file(fixture, std::ios::binary);
        file.write(reinterpret_cast<const char*>(png), sizeof(png));
    }
    {
        Texture texture(fixture);
        check(texture.width() == 2 && texture.height() == 2, "Fixture dimensions wrong");
        std::array<unsigned char, 16> pixels{};
        glBindTexture(GL_TEXTURE_2D, texture.id());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        const std::array<unsigned char, 16> expected{{255,0,0,255, 0,128,0,128, 0,0,255,255, 0,0,0,0}};
        check(pixels == expected, "PNG row orientation, RGBA conversion or alpha premultiplication wrong");
    }
    const auto invalid = temporary.path / "invalid.png";
    { std::ofstream file(invalid); file << "This is not a PNG"; }
    for (const auto& path : {temporary.path / "missing.png", invalid}) {
        bool failed = false;
        try { Texture texture(path); }
        catch (const std::runtime_error& error) {
            failed = std::string(error.what()).find(path.string()) != std::string::npos;
        }
        check(failed, "Failed load did not name the missing/invalid file");
    }
    Texture white;
    std::array<unsigned char, 4> pixel{};
    glBindTexture(GL_TEXTURE_2D, white.id());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    check(pixel == std::array<unsigned char,4>{{255,255,255,255}}, "Solid-geometry texture wrong");
}

/**
 * Creates the hidden GL context required by Texture/FrameAnimation tests. No
 * display is a declared CTest skip; once GLFW initializes, context or asset errors
 * are failures. Prepared-frame checks verify lookup timing and handle reuse.
 */
int main() {
    // CTest treats code 77 as a skip only when no display can initialize GLFW.
    // Once a display exists, context/decoder/GL failures are real test failures.
    if (!glfwInit()) {
        std::cout << "No graphical display; run with xvfb-run -a ctest --test-dir build\n";
        return 77;
    }
    GlfwLifetime glfw;
    try {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window(
            glfwCreateWindow(64, 64, "Texture tests", nullptr, nullptr), glfwDestroyWindow);
        check(static_cast<bool>(window), "Cannot create OpenGL 3.3 test context");
        glfwMakeContextCurrent(window.get());
        check(gladLoadGL(glfwGetProcAddress), "Cannot load OpenGL");
        loadAssets();
        fixtureAndErrors();
        // Frame lookup must return existing texture handles at authored boundaries,
        // not reload/upload a texture each time the result character is drawn.
        for(const auto* pose:{"happy","worried"}) {
            woodland::FrameAnimation animation(std::filesystem::path(WOODLAND_ASSET_DIR)/"characters/animations"/pose);
            const auto first=animation.frame(0).id();
            check(first!=animation.frame(.03).id(),"Prepared frame durations ignored");
            check(first==animation.frame(5.03).id(),"Prepared GIF loop timing wrong");
            check(first==animation.frame(0).id(),"Repeated access reloaded textures");
            check(animation.frame(.07).width()==256 && animation.frame(.07).height()==256,"Animation canvas cropped");
        }
        check(glGetError() == GL_NO_ERROR, "OpenGL texture test error");
        std::cout << "PNG assets, RGBA alpha, row orientation, mipmaps, errors and RAII cleanup passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

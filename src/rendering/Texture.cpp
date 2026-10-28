#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace woodland {

/**
 * Creates the same four-channel texture resource used for ordinary images, but
 * with a single white texel so flat UI quads can be tinted by the sprite shader.
 */
Texture::Texture() {
    const unsigned char white[] = {255, 255, 255, 255};
    upload(white);
}

Texture::Texture(const std::filesystem::path& path) {
    int channels = 0;
    using Pixels = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;
    // Request four channels for both RGB artwork and transparent PNGs. A decoder
    // owner is established before conversion/upload so every exit frees CPU pixels.
    Pixels pixels(stbi_load(path.string().c_str(), &width_, &height_, &channels, STBI_rgb_alpha),
                  stbi_image_free);
    // Do not substitute an unrelated image here. Required assets fail with their
    // exact path; optional callers decide whether to omit art or use a PNG fallback.
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error("Cannot load PNG texture '" + path.string() + "': "
                                 + (reason ? reason : "unknown decoder error"));
    }
    // Premultiplication keeps transparent RGB out of bilinear samples and mipmaps,
    // preventing dark/colored fringes. Pair with GL_ONE, GL_ONE_MINUS_SRC_ALPHA.
    const auto bytes = static_cast<std::size_t>(width_) * height_ * 4;
    for (std::size_t i = 0; i < bytes; i += 4)
        for (int c = 0; c < 3; ++c)
            pixels.get()[i + c] = (pixels.get()[i + c] * pixels.get()[i + 3] + 127) / 255;
    try {
        upload(pixels.get());
    } catch (const std::exception& error) {
        // Keep the image identity when adding GPU-limit/allocation diagnostics,
        // since multiple assets are loaded during one renderer construction.
        throw std::runtime_error("Cannot upload PNG texture '" + path.string() + "': " + error.what());
    }
}

void Texture::upload(const unsigned char* pixels) {
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (width_ <= 0 || height_ <= 0 || width_ > maxSize || height_ > maxSize)
        throw std::runtime_error("Image dimensions exceed the OpenGL texture limit");
    // Validate dimensions before allocating a handle; an oversized image must
    // not leave a partially initialized resource for later draw calls to bind.
    glGenTextures(1, &id_);
    if (!id_) throw std::runtime_error("Failed to allocate an OpenGL texture");
    glBindTexture(GL_TEXTURE_2D, id_);
    // Transparent edges must not sample the opposite side of the image. Mipmaps
    // keep the 256px illustrations smooth when cells become much smaller on screen.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Minification uses prefiltered levels, while magnification interpolates the
    // original pixels. Both depend on premultiplication having happened on the CPU.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // RGBA rows are always multiples of four; no global pixel unpack changes needed.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    // A constructor that throws will not run this Texture's destructor, so any
    // allocated handle must be deleted here before propagating an upload error.
    const auto error = glGetError();
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &id_);
        id_ = 0;
        throw std::runtime_error("OpenGL texture upload failed (error " + std::to_string(error) + ")");
    }
}

Texture::~Texture() {
    // The GL handle names context-owned memory, not the already-freed CPU pixels.
    // Destruction must precede GLFW/context teardown so deletion reaches its driver.
    if (id_) glDeleteTextures(1, &id_);
}

} // namespace woodland

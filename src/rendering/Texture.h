#pragma once

#include <glad/gl.h>
#include <filesystem>

namespace woodland {

/**
 * Loads one PNG into an exclusively owned RGBA OpenGL texture with mipmaps.
 *
 * The decoder preserves top-to-bottom file rows; renderer quads use v=0 at the top
 * instead of globally flipping images. RGB is premultiplied by alpha before upload
 * and mipmap filtering; rendering must use GL_ONE / GL_ONE_MINUS_SRC_ALPHA.
 * Copying/moving are disabled so texture handles have one clear lifetime. A current
 * compatible GL context is required for construction, GPU use and destruction.
 */
class Texture {
public:
    /**
     * Creates an opaque white texel so colored geometry can share the sprite shader
     * and ordered draw stream without introducing a separate rendering pipeline.
     */
    Texture();
    /**
     * Decodes a PNG once, requests RGBA even for RGB input, premultiplies pixels and
     * uploads them. CPU decoder memory is freed after upload, including error paths.
     * @param path Required image path; optional callers implement their own fallback.
     * @throws std::runtime_error With the path and decoder/upload reason on failure.
     */
    explicit Texture(const std::filesystem::path& path);
    /**
     * Deletes the GPU texture; callers must keep its OpenGL context alive until then.
     */
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

    /**
     * Borrows a GPU handle for binding or inspection. The caller must not delete it
     * or retain it after the Texture owner has been destroyed.
     */
    GLuint id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    /**
     * Uploads width_ by height_ premultiplied RGBA bytes using the default four-byte
     * unpack alignment. Clamped edges prevent wrapping; mipmaps/trilinear minification
     * smooth illustrations at board size. A failed upload deletes the new handle.
     * @throws std::runtime_error For invalid dimensions, allocation or GL upload failure.
     */
    void upload(const unsigned char* pixels);
    GLuint id_ = 0;
    int width_ = 1;
    int height_ = 1;
};

} // namespace woodland

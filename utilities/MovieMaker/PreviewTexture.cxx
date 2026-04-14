#include "PreviewTexture.hxx"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace Solstice::MovieMaker {

void PreviewTextureRgba::Destroy() {
    if (glTexture != 0) {
        glDeleteTextures(1, &glTexture);
        glTexture = 0;
    }
    width = 0;
    height = 0;
}

void PreviewTextureRgba::SetSizeUpload(SDL_Window* window, uint32_t w, uint32_t h, const std::byte* rgbaTopDown,
    size_t byteCount) {
    (void)window;
    if (w == 0 || h == 0 || rgbaTopDown == nullptr) {
        return;
    }
    const size_t expected = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    if (byteCount < expected) {
        return;
    }
    if (glTexture == 0) {
        glGenTextures(1, &glTexture);
    }
    if (width != w || height != h) {
        width = w;
        height = h;
        glBindTexture(GL_TEXTURE_2D, glTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGBA,
            GL_UNSIGNED_BYTE, nullptr);
    } else {
        glBindTexture(GL_TEXTURE_2D, glTexture);
    }
    // OpenGL expects bottom-first rows; raster is top-down.
    std::vector<unsigned char> flipped(expected);
    const auto* src = reinterpret_cast<const unsigned char*>(rgbaTopDown);
    const size_t row = static_cast<size_t>(w) * 4;
    for (uint32_t y = 0; y < h; ++y) {
        const size_t srcRow = static_cast<size_t>(y) * row;
        const size_t dstRow = static_cast<size_t>(h - 1 - y) * row;
        std::memcpy(flipped.data() + dstRow, src + srcRow, row);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE,
        flipped.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace Solstice::MovieMaker

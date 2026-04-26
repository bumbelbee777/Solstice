#include "LibUI/Graphics/PreviewTexture.hxx"

#include "LibUI/Tools/OpenGlDebugBase.hxx"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace LibUI::Graphics {

namespace {

constexpr uint32_t kMaxEdgePx = 8192u;
constexpr size_t kMaxRgbaBytes = 256u * 1024u * 1024u; // 256 MiB

bool ValidateSize(uint32_t w, uint32_t h, size_t byteCount, size_t& outExpected) {
    if (w == 0 || h == 0 || w > kMaxEdgePx || h > kMaxEdgePx) {
        return false;
    }
    const uint64_t px = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (px > static_cast<uint64_t>(std::numeric_limits<size_t>::max() / 4u)) {
        return false;
    }
    outExpected = static_cast<size_t>(px) * 4u;
    if (outExpected > kMaxRgbaBytes || byteCount < outExpected) {
        return false;
    }
    return true;
}

} // namespace

void PreviewTextureRgba::Destroy() {
    if (glTexture != 0) {
        glDeleteTextures(1, &glTexture);
        LibUI::Tools::GlFlushErrors("PreviewTextureRgba::Destroy glDeleteTextures");
        glTexture = 0;
    }
    width = 0;
    height = 0;
}

bool PreviewTextureRgba::SetSizeUpload(SDL_Window* window, uint32_t w, uint32_t h, const std::byte* rgbaTopDown,
    size_t byteCount) {
    (void)window;
    size_t expected = 0;
    if (!rgbaTopDown || !ValidateSize(w, h, byteCount, expected)) {
        return false;
    }

    if (glTexture == 0) {
        glGenTextures(1, &glTexture);
        LibUI::Tools::GlFlushErrors("PreviewTextureRgba::SetSizeUpload glGenTextures");
        if (glTexture == 0) {
            return false;
        }
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
        LibUI::Tools::GlFlushErrors("PreviewTextureRgba::SetSizeUpload glTexImage2D");
    } else {
        glBindTexture(GL_TEXTURE_2D, glTexture);
    }

    const auto* src = reinterpret_cast<const unsigned char*>(rgbaTopDown);
    const size_t rowBytes = static_cast<size_t>(w) * 4u;

    std::vector<unsigned char> oneRow;
    try {
        oneRow.resize(rowBytes);
    } catch (const std::bad_alloc&) {
        return false;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (uint32_t j = 0; j < h; ++j) {
        const size_t srcRow = static_cast<size_t>(h - 1u - j) * rowBytes;
        std::memcpy(oneRow.data(), src + srcRow, rowBytes);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, static_cast<GLint>(j), static_cast<GLsizei>(w), 1, GL_RGBA, GL_UNSIGNED_BYTE,
            oneRow.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    LibUI::Tools::GlFlushErrors("PreviewTextureRgba::SetSizeUpload glTexSubImage2D");

    return true;
}

} // namespace LibUI::Graphics

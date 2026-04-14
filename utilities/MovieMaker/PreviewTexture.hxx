#pragma once

#include <cstddef>
#include <cstdint>

#include <imgui.h>

struct SDL_Window;

namespace Solstice::MovieMaker {

/// OpenGL 2D RGBA texture for ImGui; resizes/reuploads as needed.
struct PreviewTextureRgba {
    unsigned glTexture = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    void Destroy();
    /// Ensures texture exists and matches dimensions; uploads RGBA8 row-major (top-down to match raster).
    void SetSizeUpload(SDL_Window* window, uint32_t w, uint32_t h, const std::byte* rgbaTopDown, size_t byteCount);

    ImTextureID ImGuiTexId() const { return static_cast<ImTextureID>(glTexture); }
    bool Valid() const { return glTexture != 0 && width > 0 && height > 0; }
};

} // namespace Solstice::MovieMaker

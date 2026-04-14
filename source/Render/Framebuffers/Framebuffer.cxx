#include "Framebuffer.hxx"
#include <Core/Debug/Debug.hxx>
#include <cstdint>
#include <algorithm>

namespace Solstice::Render {

Framebuffer::Framebuffer(int Width, int Height) : Width(Width), Height(Height) {
    // Create BGFX framebuffer
    uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    bgfx::TextureHandle textures[2];
    uint8_t numTextures = 0;

    textures[numTextures++] = bgfx::createTexture2D(
        (uint16_t)Width, (uint16_t)Height, false, 1, bgfx::TextureFormat::RGBA8, flags
    );

    // Depth
    textures[numTextures++] = bgfx::createTexture2D(
        (uint16_t)Width, (uint16_t)Height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL
    );

    Handle = bgfx::createFrameBuffer(numTextures, textures, true);
}

Framebuffer::~Framebuffer() {
    if (bgfx::isValid(Handle)) {
        bgfx::destroy(Handle);
    }
}

// Add these methods to the Framebuffer class
void Framebuffer::Resize(int NewWidth, int NewHeight) {
    if (NewWidth == Width && NewHeight == Height) {
        return;
    }

    if (NewWidth <= 0 || NewHeight <= 0)
        return;

    Width = NewWidth;
    Height = NewHeight;

    if (bgfx::isValid(Handle)) {
        bgfx::destroy(Handle);
    }

    uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    bgfx::TextureHandle textures[2];
    uint8_t numTextures = 0;

    textures[numTextures++] = bgfx::createTexture2D(
        (uint16_t)Width, (uint16_t)Height, false, 1, bgfx::TextureFormat::RGBA8, flags
    );

    textures[numTextures++] = bgfx::createTexture2D(
        (uint16_t)Width, (uint16_t)Height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL
    );

    Handle = bgfx::createFrameBuffer(numTextures, textures, true);
}

} // namespace Solstice::Render

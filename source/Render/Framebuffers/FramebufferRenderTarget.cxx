#include <bgfx/bgfx.h>
#include <Render/Context/Context.hxx>
#include <stdexcept>

namespace Solstice::Render {

FramebufferRenderTarget::FramebufferRenderTarget(Math::Vec2 Size, bool HasDepth)
    : m_Size(Size), m_HasDepth(HasDepth) {
    CreateFramebuffer();
}

FramebufferRenderTarget::~FramebufferRenderTarget() {
    DeleteFramebuffer();
}

void FramebufferRenderTarget::CreateFramebuffer() {
    // Create framebuffer
    // BGFX texture flags
    uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

    bgfx::TextureHandle textures[2];
    uint8_t numTextures = 0;

    // Color attachment
    textures[numTextures++] = bgfx::createTexture2D(
        (uint16_t)m_Size.x, (uint16_t)m_Size.y, false, 1, bgfx::TextureFormat::RGBA8, flags
    );

    // Depth attachment
    if (m_HasDepth) {
        textures[numTextures++] = bgfx::createTexture2D(
            (uint16_t)m_Size.x, (uint16_t)m_Size.y, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL
        );
    }

    m_Handle = bgfx::createFrameBuffer(numTextures, textures, true);

    if (!bgfx::isValid(m_Handle)) {
        throw std::runtime_error("Failed to create BGFX framebuffer!");
    }
}

void FramebufferRenderTarget::DeleteFramebuffer() {
    if (bgfx::isValid(m_Handle)) {
        bgfx::destroy(m_Handle);
        m_Handle = BGFX_INVALID_HANDLE;
    }
}

void FramebufferRenderTarget::Bind() const {
    // In BGFX, binding a framebuffer is usually done per view.
    // This method might need to take a view ID or set a global state if that's how the engine works.
    // For now, we'll assume view 0 or just leave it as a no-op if the renderer handles view assignment.
    // However, to match previous behavior, we might want to set this framebuffer for the current view.
    // Since we don't have the view ID here, we can't do much.
    // But typically, you set the framebuffer for a view: bgfx::setViewFrameBuffer(viewId, m_Handle);
}

void FramebufferRenderTarget::Unbind() const {
    // No-op in BGFX immediate mode, handled by view setup
}

void FramebufferRenderTarget::Resize(Math::Vec2 NewSize) {
    if (NewSize.x <= 0 || NewSize.y <= 0) {
        return;
    }

    if (NewSize.x != m_Size.x || NewSize.y != m_Size.y) {
        m_Size = NewSize;
        DeleteFramebuffer();
        CreateFramebuffer();
    }
}

} // namespace Solstice::Render

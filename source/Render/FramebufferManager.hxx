#pragma once

#include "../Solstice.hxx"
#include <bgfx/bgfx.h>
#include <memory>

namespace Solstice::Render {

// Centralized framebuffer management for all render targets
class SOLSTICE_API FramebufferManager {
public:
    FramebufferManager();
    ~FramebufferManager();

    // Delete copy constructor and assignment operator
    FramebufferManager(const FramebufferManager&) = delete;
    FramebufferManager& operator=(const FramebufferManager&) = delete;

    // Initialize framebuffers
    void Initialize(uint32_t width, uint32_t height, uint32_t shadowMapSize = 1024);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);

    // Shadow map framebuffer
    bgfx::FrameBufferHandle GetShadowFramebuffer() const { return m_shadowFB; }
    bgfx::TextureHandle GetShadowMap() const { return m_shadowMap; }

    // Scene framebuffer (color + depth)
    bgfx::FrameBufferHandle GetSceneFramebuffer() const { return m_sceneFB; }
    bgfx::TextureHandle GetSceneColor() const { return m_sceneColor; }
    bgfx::TextureHandle GetSceneDepth() const { return m_sceneDepth; }

    // Getters for dimensions
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    uint32_t GetShadowMapSize() const { return m_shadowMapSize; }

private:
    void CreateShadowFramebuffer();
    void CreateSceneFramebuffer();
    void DestroyShadowFramebuffer();
    void DestroySceneFramebuffer();

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_shadowMapSize = 1024;

    // Shadow map resources
    bgfx::FrameBufferHandle m_shadowFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_shadowMap = BGFX_INVALID_HANDLE;

    // Scene framebuffer resources
    bgfx::FrameBufferHandle m_sceneFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_sceneColor = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_sceneDepth = BGFX_INVALID_HANDLE;
};

} // namespace Solstice::Render

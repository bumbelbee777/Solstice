#pragma once

#include "../Solstice.hxx"
#include <bgfx/bgfx.h>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>

namespace Solstice::Render {

class SOLSTICE_API PostProcessing {
public:
    PostProcessing();
    ~PostProcessing();

    void Initialize(uint32_t width, uint32_t height);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);

    // Call this before rendering the shadow map pass
    void BeginShadowPass();
    // Call this before rendering the main scene
    void BeginScenePass();
    // Call this after rendering the main scene
    void EndScenePass();
    
    // Apply post processing and render to backbuffer
    void Apply(bgfx::ViewId viewId);

    // Getters for integration
    bgfx::FrameBufferHandle GetShadowFramebuffer() const { return m_shadowFB; }
    bgfx::TextureHandle GetShadowMap() const { return m_shadowMap; }
    
    // Shadow matrix for shaders
    const Math::Matrix4& GetShadowViewProj() const { return m_shadowViewProj; }
    
    // Shader Access
    bgfx::ProgramHandle GetShadowProgram() const { return m_progShadow; }
    
    // Set/Get settings
    void SetShadowQuality(int size) { m_shadowMapSize = size; } // Requires re-init if changed
    
    // Constants for View IDs
    static constexpr bgfx::ViewId VIEW_SHADOW = 1; // 0 is reserved/main
    static constexpr bgfx::ViewId VIEW_SCENE = 2;  // Main scene color
    
private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    
    // Shadow Resources
    uint32_t m_shadowMapSize = 1024;
    bgfx::FrameBufferHandle m_shadowFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_shadowMap = BGFX_INVALID_HANDLE;
    Math::Matrix4 m_shadowViewProj;
    
    // Scene Resources (HDR Color + Depth)
    bgfx::FrameBufferHandle m_sceneFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_sceneColor = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_sceneDepth = BGFX_INVALID_HANDLE;
    
    // Post Processing Programs
    bgfx::ProgramHandle m_progPost = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_progShadow = BGFX_INVALID_HANDLE; // Simple shadow caster shader
    
    bgfx::UniformHandle u_shadowParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texShadow = BGFX_INVALID_HANDLE;
    
    // Fullscreen Quad
    bgfx::VertexLayout m_layout;
    
    void CreateResources();
    void DestroyResources();
};

} // namespace Solstice::Render
#pragma once

#include "../Solstice.hxx"
#include <bgfx/bgfx.h>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <unordered_map>

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
    bgfx::FrameBufferHandle GetSceneFramebuffer() const { return m_sceneFB; }
    bgfx::TextureHandle GetShadowMap() const { return m_shadowMap; }
    bgfx::TextureHandle GetSceneColor() const { return m_sceneColor; }

    // Shadow matrix for shaders
    const Math::Matrix4& GetShadowViewProj() const { return m_shadowViewProj; }

    // Shader Access
    bgfx::ProgramHandle GetShadowProgram() const { return m_progShadow; }

    // Set/Get settings
    void SetShadowQuality(int size) { m_shadowMapSize = size; } // Requires re-init if changed

    // Set camera position for shadow following
    void SetCameraPosition(const Math::Vec3& pos) { m_cameraPos = pos; }

    // Set light direction for shadow calculation
    void SetLightDirection(const Math::Vec3& direction);

    // Raytracing texture integration
    void SetRaytracingTextures(bgfx::TextureHandle shadowTexture, bgfx::TextureHandle aoTexture);

    // HDR settings
    void SetHDRExposure(float exposure) { m_hdrExposure = exposure; }
    float GetHDRExposure() const { return m_hdrExposure; }

    // Motion blur settings
    enum class MotionBlurQuality {
        Low,
        Medium,
        High
    };

    struct MotionBlurSettings {
        bool Enabled = false;
        float Strength = 1.0f;
        int SampleCount = 12;
        float DepthScale = 0.5f;
        MotionBlurQuality Quality = MotionBlurQuality::Medium;
    };

    void SetMotionBlurSettings(const MotionBlurSettings& settings) { m_motionBlurSettings = settings; }
    const MotionBlurSettings& GetMotionBlurSettings() const { return m_motionBlurSettings; }
    void SetMotionBlurQuality(MotionBlurQuality quality);
    MotionBlurQuality GetMotionBlurQuality() const { return m_motionBlurSettings.Quality; }
    void SetPreviousViewProj(const Math::Matrix4& prevViewProj) { m_previousViewProj = prevViewProj; }

    // Bloom settings (HDR glow extraction)
    struct BloomSettings {
        bool Enabled = false;
        float Threshold = 1.0f;    // HDR brightness threshold
        float Intensity = 0.5f;    // Bloom intensity multiplier
        float Radius = 5.0f;       // Blur radius
    };

    void SetBloomSettings(const BloomSettings& Settings) { m_BloomSettings = Settings; }
    const BloomSettings& GetBloomSettings() const { return m_BloomSettings; }
    void SetBloomEnabled(bool Enabled) { m_BloomSettings.Enabled = Enabled; }
    void SetBloomThreshold(float Threshold) { m_BloomSettings.Threshold = Threshold; }
    void SetBloomIntensity(float Intensity) { m_BloomSettings.Intensity = Intensity; }

    // God ray / volumetric settings
    struct GodRaySettings {
        bool Enabled = false;
        float Density = 0.8f;      // Ray density
        float Decay = 0.96f;       // Falloff per sample
        float Exposure = 0.3f;     // Final exposure multiplier
    };

    void SetGodRaySettings(const GodRaySettings& Settings) { m_GodRaySettings = Settings; }
    const GodRaySettings& GetGodRaySettings() const { return m_GodRaySettings; }
    void SetGodRayEnabled(bool Enabled) { m_GodRaySettings.Enabled = Enabled; }
    void SetVolumetricTexture(bgfx::TextureHandle Texture) { m_VolumetricTexture = Texture; }

    // Velocity buffer pass
    void BeginVelocityPass();
    void EndVelocityPass();
    bgfx::FrameBufferHandle GetVelocityFramebuffer() const { return m_velocityFB; }
    bgfx::TextureHandle GetVelocityBuffer() const { return m_velocityBuffer; }

    // Per-object velocity tracking
    void UpdateObjectVelocity(uint32_t objectId, const Math::Matrix4& currentTransform);
    Math::Vec3 GetObjectVelocity(uint32_t objectId) const;

    // Constants for View IDs
    static constexpr bgfx::ViewId VIEW_SHADOW = 1; // 0 is reserved/main
    static constexpr bgfx::ViewId VIEW_SCENE = 2;  // Main scene color
    static constexpr bgfx::ViewId VIEW_VELOCITY = 3; // Velocity buffer pass

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Shadow Resources
    uint32_t m_shadowMapSize = 1024;
    bgfx::FrameBufferHandle m_shadowFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_shadowMap = BGFX_INVALID_HANDLE;
    Math::Matrix4 m_shadowViewProj;
    Math::Vec3 m_cameraPos = Math::Vec3(0.0f, 0.0f, 0.0f);
    Math::Vec3 m_lightDirection = Math::Vec3(0.5f, 1.0f, -0.5f).Normalized(); // Camera position for shadow following

    // Scene Resources (HDR Color + Depth + Velocity)
    bgfx::FrameBufferHandle m_sceneFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_sceneColor = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_sceneDepth = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_velocityBuffer = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_velocityFB = BGFX_INVALID_HANDLE;

    // Per-object velocity tracking
    std::unordered_map<uint32_t, Math::Matrix4> m_previousTransforms;
    std::unordered_map<uint32_t, Math::Vec3> m_objectVelocities;

    // Post Processing Programs
    bgfx::ProgramHandle m_progPost = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_progShadow = BGFX_INVALID_HANDLE; // Simple shadow caster shader

    bgfx::UniformHandle u_shadowParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texShadow = BGFX_INVALID_HANDLE;

    // Raytracing textures (set externally)
    bgfx::TextureHandle m_raytraceShadowTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_raytraceAOTexture = BGFX_INVALID_HANDLE;

    // HDR settings
    float m_hdrExposure = 1.0f;
    bgfx::UniformHandle u_hdrExposure = BGFX_INVALID_HANDLE;

    // Motion blur settings
    MotionBlurSettings m_motionBlurSettings;
    Math::Matrix4 m_previousViewProj;
    bgfx::UniformHandle u_motionBlurParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_prevViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texDepth = BGFX_INVALID_HANDLE;

    // Bloom settings
    BloomSettings m_BloomSettings;
    bgfx::UniformHandle u_bloomParams = BGFX_INVALID_HANDLE;

    // God ray / volumetric settings
    GodRaySettings m_GodRaySettings;
    bgfx::TextureHandle m_VolumetricTexture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_godRayParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texVolumetric = BGFX_INVALID_HANDLE;

    // Fullscreen Quad
    bgfx::VertexLayout m_layout;

    void CreateResources();
    void DestroyResources();
};

} // namespace Solstice::Render

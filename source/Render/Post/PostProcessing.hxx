#pragma once

#include <Solstice.hxx>
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
    bgfx::FrameBufferHandle GetShadowFramebuffer() const { return m_ShadowFB; }
    bgfx::FrameBufferHandle GetSceneFramebuffer() const { return m_SceneFB; }
    bgfx::TextureHandle GetShadowMap() const { return m_ShadowMap; }
    bgfx::TextureHandle GetSceneColor() const { return m_SceneColor; }

    // Shadow matrix for shaders
    const Math::Matrix4& GetShadowViewProj() const { return m_ShadowViewProj; }

    // Shader Access
    bgfx::ProgramHandle GetShadowProgram() const { return m_ProgShadow; }

    // Set/Get settings
    void SetShadowQuality(int size) { m_ShadowMapSize = size; } // Requires re-init if changed

    // Set camera position for shadow following
    void SetCameraPosition(const Math::Vec3& pos) { m_CameraPos = pos; }

    // Set light direction for shadow calculation
    void SetLightDirection(const Math::Vec3& direction);

    // Raytracing texture integration
    void SetRaytracingTextures(bgfx::TextureHandle shadowTexture, bgfx::TextureHandle aoTexture);

    // Reflection probe integration
    void SetReflectionProbeTexture(bgfx::TextureHandle probeTexture) { m_ReflectionProbeTexture = probeTexture; }
    void SetCameraMatrices(const Math::Matrix4& view, const Math::Matrix4& jitteredProj,
                           const Math::Matrix4& unjitteredProj, const Math::Vec3& cameraPos,
                           const Math::Vec2& jitterNdc);

    struct ReflectionSettings {
        float Intensity = 0.4f;
        int MaxSteps = 12;
        float Thickness = 0.15f;
        float Stride = 0.3f;
    };
    void SetReflectionSettings(const ReflectionSettings& settings) { m_ReflectionSettings = settings; }
    const ReflectionSettings& GetReflectionSettings() const { return m_ReflectionSettings; }

    // HDR settings
    void SetHDRExposure(float exposure) { m_HDRExposure = exposure; }
    float GetHDRExposure() const { return m_HDRExposure; }

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

    void SetMotionBlurSettings(const MotionBlurSettings& settings) { m_MotionBlurSettings = settings; }
    const MotionBlurSettings& GetMotionBlurSettings() const { return m_MotionBlurSettings; }
    void SetMotionBlurQuality(MotionBlurQuality quality);
    MotionBlurQuality GetMotionBlurQuality() const { return m_MotionBlurSettings.Quality; }
    void SetPreviousViewProj(const Math::Matrix4& prevViewProj) { m_PreviousViewProj = prevViewProj; }

    struct TAASettings {
        bool Enabled = true;
        float BlendFactor = 0.10f;
        float ClampStrength = 1.25f;
        float Sharpen = 0.10f;
    };
    void SetTAASettings(const TAASettings& settings) { m_TAASettings = settings; }
    const TAASettings& GetTAASettings() const { return m_TAASettings; }
    void InvalidateTAAHistory();

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
    bgfx::FrameBufferHandle GetVelocityFramebuffer() const { return m_VelocityFB; }
    bgfx::TextureHandle GetVelocityBuffer() const { return m_VelocityBuffer; }
    bgfx::TextureHandle GetTAAHistoryTexture() const { return m_TAAHistoryColor[m_TAAHistoryReadIndex]; }
    const Math::Matrix4& GetCurrentUnjitteredViewProj() const { return m_ViewProjUnjittered; }
    const Math::Matrix4& GetPreviousUnjitteredViewProj() const { return m_PreviousViewProjUnjittered; }

    // Per-object velocity tracking
    void UpdateObjectVelocity(uint32_t objectId, const Math::Matrix4& currentTransform);
    Math::Vec3 GetObjectVelocity(uint32_t objectId) const;

    // Constants for View IDs
    static constexpr bgfx::ViewId VIEW_SHADOW = 1; // 0 is reserved/main
    static constexpr bgfx::ViewId VIEW_SCENE = 2;  // Main scene color
    static constexpr bgfx::ViewId VIEW_VELOCITY = 3; // Velocity buffer pass

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    // Shadow Resources
    uint32_t m_ShadowMapSize = 1024;
    bgfx::FrameBufferHandle m_ShadowFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_ShadowMap = BGFX_INVALID_HANDLE;
    Math::Matrix4 m_ShadowViewProj;
    Math::Vec3 m_CameraPos = Math::Vec3(0.0f, 0.0f, 0.0f);
    Math::Vec3 m_LightDirection = Math::Vec3(0.5f, 1.0f, -0.5f).Normalized(); // Camera position for shadow following

    // Scene Resources (HDR Color + Depth + Velocity)
    bgfx::FrameBufferHandle m_SceneFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_SceneColor = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_SceneDepth = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_VelocityBuffer = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_VelocityFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_TAAHistoryColor[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    uint8_t m_TAAHistoryReadIndex = 0;
    bool m_TAAHistoryValid = false;

    // Per-object velocity tracking
    std::unordered_map<uint32_t, Math::Matrix4> m_PreviousTransforms;
    std::unordered_map<uint32_t, Math::Vec3> m_ObjectVelocities;

    // Post Processing Programs
    bgfx::ProgramHandle m_ProgPost = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ProgShadow = BGFX_INVALID_HANDLE; // Simple shadow caster shader

    bgfx::UniformHandle u_ShadowParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TexColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TexShadow = BGFX_INVALID_HANDLE;

    // Raytracing textures (set externally)
    bgfx::TextureHandle m_RaytraceShadowTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_RaytraceAOTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_ReflectionProbeTexture = BGFX_INVALID_HANDLE;

    // Reflection uniforms
    bgfx::UniformHandle u_ReflectionParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ReflectionViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ReflectionInvViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_CameraPos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TexReflectionProbe = BGFX_INVALID_HANDLE;
    ReflectionSettings m_ReflectionSettings;
    Math::Matrix4 m_ViewProj;
    Math::Matrix4 m_InvViewProj;
    Math::Vec3 m_CameraPosWorld = Math::Vec3(0.0f, 0.0f, 0.0f);

    // HDR settings
    float m_HDRExposure = 1.0f;
    bgfx::UniformHandle u_HDRExposure = BGFX_INVALID_HANDLE;

    // Motion blur settings
    MotionBlurSettings m_MotionBlurSettings;
    Math::Matrix4 m_PreviousViewProj;
    Math::Matrix4 m_ViewProjUnjittered;
    Math::Matrix4 m_PreviousViewProjUnjittered;
    Math::Vec2 m_CurrentJitterNdc = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 m_PreviousJitterNdc = Math::Vec2(0.0f, 0.0f);
    bgfx::UniformHandle u_MotionBlurParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_PrevViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TexDepth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TexVelocity = BGFX_INVALID_HANDLE;

    // TAA settings
    TAASettings m_TAASettings;
    bgfx::UniformHandle s_TexHistory = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_TAAParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_TAAJitter = BGFX_INVALID_HANDLE;

    // Bloom settings
    BloomSettings m_BloomSettings;
    bgfx::UniformHandle u_BloomParams = BGFX_INVALID_HANDLE;

    // God ray / volumetric settings
    GodRaySettings m_GodRaySettings;
    bgfx::TextureHandle m_VolumetricTexture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_GodRayParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TexVolumetric = BGFX_INVALID_HANDLE;

    // Fullscreen Quad
    bgfx::VertexLayout m_Layout;

    void CreateResources();
    void DestroyResources();
};

} // namespace Solstice::Render

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
        float BlendFactor = 0.085f;
        float ClampStrength = 0.70f;
        float Sharpen = 0.20f;
    };
    void SetTAASettings(const TAASettings& settings) { m_TAASettings = settings; }
    const TAASettings& GetTAASettings() const { return m_TAASettings; }
    void InvalidateTAAHistory();

    /// Per-sample MSAA for the main HDR color/depth pass (1, 2, 4, 8). Resolved before TAA/post.
    void SetSceneMsaaSamples(uint8_t samples);
    uint8_t GetSceneMsaaSamples() const { return m_SceneMsaaSamples; }

    // Fast approximate anti-aliasing (HDR linear, before tone map). Helps edges when TAA is off or thin geometry.
    struct FXAASettings {
        bool Enabled = true;
        float Strength = 0.85f;
    };
    void SetFXAASettings(const FXAASettings& settings) { m_FXAASettings = settings; }
    const FXAASettings& GetFXAASettings() const { return m_FXAASettings; }

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

    /// Optional per-frame post tunables (see `fs_post.sc` `u_postChain`). Not persisted in scene assets.
    struct PostChainTunables {
        /// HDR pre-TAA fullscreen blur mix (0 = off). Games use for pause overlays, depth haze, etc.
        float PrenormBlurMix{0.0f};
        float Reserved0{0.0f};
        float Reserved1{0.0f};
        float Reserved2{0.0f};
    };

    void SetPostChainTunables(const PostChainTunables& Tunables) { m_PostChainTunables = Tunables; }
    const PostChainTunables& GetPostChainTunables() const { return m_PostChainTunables; }

    /// Expanding ring UV refraction in `fs_post` (screen-space; pairs with `u_fog` for action FX).
    struct ShockwaveSettings {
        float CenterX{0.5f};
        float CenterY{0.5f};
        float RingRadius{0.35f}; // length in aspect-corrected UV space from center
        float Strength{0.0f};   // 0 = disabled
    };
    void SetShockwaveSettings(const ShockwaveSettings& s) { m_ShockwaveSettings = s; }
    const ShockwaveSettings& GetShockwaveSettings() const { return m_ShockwaveSettings; }

    /// Cheap exponential distance + height fog in post (linear HDR, before tone map). Complements god-ray texture.
    struct ScreenFogSettings {
        bool Enabled{false};
        float DistanceDensity{0.0f};
        float HeightFalloff{0.0f};
        float HeightAnchorY{0.0f};
        float Mix{1.0f};
        float FogR{0.55f};
        float FogG{0.6f};
        float FogB{0.7f};
    };
    void SetScreenFogSettings(const ScreenFogSettings& s) { m_ScreenFogSettings = s; }
    const ScreenFogSettings& GetScreenFogSettings() const { return m_ScreenFogSettings; }

    /// SMM / editor preview: chromatic (depth-scaled), frame smear, height fog dither. Games may use the same path.
    struct CinematicViewState {
        /// Radial RGB split strength (0 = off). Tuned for ~0.002–0.02 in preview.
        float ChromaticAberrationStrength{0.0f};
        /// 0 = uniform; 1 = more aberration on distant pixels (uses hardware depth).
        float ChromaticAberrationDepthScale{0.7f};
        float ChromaticCenterU{0.5f};
        float ChromaticCenterV{0.5f};
        /// Blends in extra TAA history (0–1) for motion-graphic / film smear.
        float SmearFrameStrength{0.0f};
        /// 0 = off; 0.02–0.12 breaks fog banding (screen-space fog in `fs_post`).
        float ScreenFogDither{0.0f};
    };

    void SetCinematicViewState(const CinematicViewState& s) { m_CinematicView = s; }
    const CinematicViewState& GetCinematicViewState() const { return m_CinematicView; }

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
    /// Scene HDR MSAA: 1 = off, 2/4/8 = bgfx MSAA FBO; combined with TAA on resolved color.
    uint8_t m_SceneMsaaSamples = 8;

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

    FXAASettings m_FXAASettings;
    bgfx::UniformHandle u_FXAAParams = BGFX_INVALID_HANDLE;

    // Bloom settings
    BloomSettings m_BloomSettings;
    bgfx::UniformHandle u_BloomParams = BGFX_INVALID_HANDLE;

    // God ray / volumetric settings
    GodRaySettings m_GodRaySettings;
    bgfx::TextureHandle m_VolumetricTexture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_GodRayParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TexVolumetric = BGFX_INVALID_HANDLE;

    PostChainTunables m_PostChainTunables{};
    bgfx::UniformHandle u_PostChain = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ShockwaveParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ScreenFog = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_FogColor = BGFX_INVALID_HANDLE;
    ShockwaveSettings m_ShockwaveSettings{};
    ScreenFogSettings m_ScreenFogSettings{};
    CinematicViewState m_CinematicView{};

    bgfx::UniformHandle u_ChromaticParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_SmearFrame = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_FogDither = BGFX_INVALID_HANDLE;

    // Fullscreen Quad
    bgfx::VertexLayout m_Layout;

    void CreateResources();
    void DestroyResources();
};

} // namespace Solstice::Render

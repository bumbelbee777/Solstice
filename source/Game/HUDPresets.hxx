#pragma once

#include "../Solstice.hxx"
#include "../Math/Vector.hxx"
#include <imgui.h>
#include <cstdint>

namespace Solstice::Game {

// Bodycam HUD configuration
struct BodycamHUDConfig {
    // Flicker settings
    float FlickerRate{2.0f}; // Average seconds between flickers
    float FlickerIntensity{0.15f}; // Maximum brightness variation (0.0-1.0)
    float ColorShiftIntensity{0.05f}; // Maximum color temperature shift (0.0-1.0)
    float ScanlineIntensity{0.1f}; // Scanline artifact intensity (0.0-1.0)
    float ChromaticAberrationIntensity{0.02f}; // Chromatic aberration at edges (0.0-1.0)

    // Timing
    float FadeInDuration{2.0f}; // Duration of fade-in from black
    float MicroStutterChance{0.1f}; // Chance of micro-stutter per frame (0.0-1.0)
    float MicroStutterDuration{0.016f}; // Duration of micro-stutter (typically 1 frame)

    // Vignette
    float VignetteStrength{0.15f}; // Corner darkening strength (0.0-1.0)
    float VignetteSize{0.1f}; // Size of vignette area (0.0-1.0)

    // Random seed for deterministic effects
    uint32_t Seed{12345};
};

// Bodycam HUD preset - implements realistic camera flicker effects
class SOLSTICE_API BodycamHUD {
public:
    BodycamHUD();
    explicit BodycamHUD(const BodycamHUDConfig& Config);
    ~BodycamHUD() = default;

    // Update HUD state (call each frame)
    void Update(float DeltaTime);

    // Render HUD effects (call after scene rendering, before UI)
    void Render();

    // Configuration
    void SetConfig(const BodycamHUDConfig& Config) { m_Config = Config; }
    const BodycamHUDConfig& GetConfig() const { return m_Config; }

    // State queries
    bool IsInitialized() const { return m_Initialized; }
    float GetBrightnessMultiplier() const { return m_CurrentBrightness; }
    Math::Vec3 GetColorShift() const { return m_CurrentColorShift; }

private:
    // Generate deterministic random value
    float Random(uint32_t Seed) const;

    // Update flicker state
    void UpdateFlicker(float DeltaTime);

    // Update color temperature shift
    void UpdateColorShift(float DeltaTime);

    // Render brightness variation overlay
    void RenderBrightnessOverlay();

    // Render color temperature shift
    void RenderColorShift();

    // Render scanline artifacts
    void RenderScanlines();

    // Render chromatic aberration
    void RenderChromaticAberration();

    // Render vignette
    void RenderVignette();

    // Render fade-in
    void RenderFadeIn();

    BodycamHUDConfig m_Config;

    // State
    bool m_Initialized{false};
    float m_Time{0.0f};
    float m_FadeInTime{0.0f};

    // Flicker state
    float m_LastFlickerTime{0.0f};
    float m_FlickerDuration{0.0f};
    float m_CurrentBrightness{1.0f};
    float m_TargetBrightness{1.0f};

    // Color shift state
    float m_ColorShiftTime{0.0f};
    Math::Vec3 m_CurrentColorShift{1.0f, 1.0f, 1.0f};
    Math::Vec3 m_TargetColorShift{1.0f, 1.0f, 1.0f};

    // Scanline state
    float m_ScanlineOffset{0.0f};
    float m_ScanlineTime{0.0f};

    // Micro-stutter state
    float m_MicroStutterTime{0.0f};
    bool m_InMicroStutter{false};

    // Random state
    uint32_t m_RandomState{0};
};

} // namespace Solstice::Game


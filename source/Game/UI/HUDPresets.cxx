#include "UI/HUDPresets.hxx"
#include "../../Core/Debug/Debug.hxx"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace Solstice::Game {

BodycamHUD::BodycamHUD() : m_Config() {
    m_RandomState = m_Config.Seed;
    m_Initialized = false; // Start uninitialized for fade-in
    m_FadeInTime = 0.0f;
}

BodycamHUD::BodycamHUD(const BodycamHUDConfig& Config) : m_Config(Config) {
    m_RandomState = Config.Seed;
    m_Initialized = false; // Start uninitialized for fade-in
    m_FadeInTime = 0.0f;
}

float BodycamHUD::Random(uint32_t Seed) const {
    // Simple hash-based random
    uint32_t H = Seed ^ (Seed << 13);
    H ^= (H >> 17);
    H ^= (H << 5);
    return static_cast<float>(H & 0x7FFFFFFF) / 2147483647.0f;
}

void BodycamHUD::Update(float DeltaTime) {
    // Safety check - ensure DeltaTime is valid
    if (DeltaTime <= 0.0f || DeltaTime > 1.0f) {
        return; // Skip invalid delta times
    }

    m_Time += DeltaTime;

    // Update fade-in
    if (!m_Initialized) {
        m_FadeInTime += DeltaTime;
        // Force initialization after max 5 seconds (safety timeout)
        if (m_FadeInTime >= m_Config.FadeInDuration || m_FadeInTime >= 5.0f) {
            m_Initialized = true;
            m_FadeInTime = m_Config.FadeInDuration; // Clamp to prevent overflow
        }
    }

    // Update flicker
    UpdateFlicker(DeltaTime);

    // Update color shift
    UpdateColorShift(DeltaTime);

    // Update scanlines
    m_ScanlineTime += DeltaTime;
    m_ScanlineOffset = std::sin(m_ScanlineTime * 10.0f) * 0.5f + 0.5f;

    // Update micro-stutter
    if (m_InMicroStutter) {
        m_MicroStutterTime += DeltaTime;
        if (m_MicroStutterTime >= m_Config.MicroStutterDuration) {
            m_InMicroStutter = false;
            m_MicroStutterTime = 0.0f;
        }
    } else {
        // Random chance for micro-stutter
        float Rand = Random(static_cast<uint32_t>(m_Time * 1000.0f) + m_RandomState);
        if (Rand < m_Config.MicroStutterChance * DeltaTime * 60.0f) {
            m_InMicroStutter = true;
            m_MicroStutterTime = 0.0f;
        }
    }
}

void BodycamHUD::UpdateFlicker(float DeltaTime) {
    m_LastFlickerTime += DeltaTime;

    // Check if it's time for a new flicker
    if (m_LastFlickerTime >= m_Config.FlickerRate) {
        // Random chance for flicker (not every interval)
        float Rand = Random(static_cast<uint32_t>(m_Time * 1000.0f) + m_RandomState + 1000);
        if (Rand < 0.3f) { // 30% chance
            // Generate flicker
            float FlickerRand = Random(static_cast<uint32_t>(m_Time * 1000.0f) + m_RandomState + 2000);
            m_FlickerDuration = 0.1f + FlickerRand * 0.1f; // 0.1-0.2s flicker
            m_TargetBrightness = 1.0f + (FlickerRand - 0.5f) * m_Config.FlickerIntensity * 2.0f;
            m_LastFlickerTime = 0.0f;
        } else {
            m_LastFlickerTime = 0.0f; // Reset timer
        }
    }

    // Update current brightness (smooth interpolation)
    if (m_FlickerDuration > 0.0f) {
        m_FlickerDuration -= DeltaTime;
        if (m_FlickerDuration < 0.0f) {
            m_FlickerDuration = 0.0f;
            m_TargetBrightness = 1.0f;
        }

        // Smooth interpolation to target brightness
        float LerpSpeed = 5.0f; // How fast to interpolate
        m_CurrentBrightness += (m_TargetBrightness - m_CurrentBrightness) * LerpSpeed * DeltaTime;
    } else {
        // Return to normal brightness
        float LerpSpeed = 2.0f;
        m_CurrentBrightness += (1.0f - m_CurrentBrightness) * LerpSpeed * DeltaTime;
    }

    // Clamp brightness
    m_CurrentBrightness = std::max(0.0f, std::min(2.0f, m_CurrentBrightness));
}

void BodycamHUD::UpdateColorShift(float DeltaTime) {
    m_ColorShiftTime += DeltaTime;

    // Update color shift every 0.5 seconds
    if (m_ColorShiftTime >= 0.5f) {
        float RandR = Random(static_cast<uint32_t>(m_Time * 1000.0f) + m_RandomState + 3000);
        float RandG = Random(static_cast<uint32_t>(m_Time * 1000.0f) + m_RandomState + 4000);
        float RandB = Random(static_cast<uint32_t>(m_Time * 1000.0f) + m_RandomState + 5000);

        // Shift towards blue (cool) or amber (warm)
        float Shift = (RandR - 0.5f) * m_Config.ColorShiftIntensity;
        m_TargetColorShift = Math::Vec3(
            1.0f - Shift * 0.3f, // Red decreases for cool, increases for warm
            1.0f - Shift * 0.1f,
            1.0f + Shift * 0.2f  // Blue increases for cool, decreases for warm
        );
        m_ColorShiftTime = 0.0f;
    }

    // Smooth interpolation
    float LerpSpeed = 2.0f;
    m_CurrentColorShift.x += (m_TargetColorShift.x - m_CurrentColorShift.x) * LerpSpeed * DeltaTime;
    m_CurrentColorShift.y += (m_TargetColorShift.y - m_CurrentColorShift.y) * LerpSpeed * DeltaTime;
    m_CurrentColorShift.z += (m_TargetColorShift.z - m_CurrentColorShift.z) * LerpSpeed * DeltaTime;
}

void BodycamHUD::Render() {
    // Safety check - ensure ImGui is initialized
    if (!ImGui::GetCurrentContext()) {
        return;
    }

    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;

    // Safety check - ensure screen size is valid
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) {
        return;
    }

    // Render fade-in (only if not initialized and fade-in time is less than duration)
    // Once fade-in completes (m_Initialized = true), we stop rendering it
    if (!m_Initialized && m_FadeInTime < m_Config.FadeInDuration) {
        RenderFadeIn();
    }

    // Render brightness variation
    if (std::abs(m_CurrentBrightness - 1.0f) > 0.01f) {
        RenderBrightnessOverlay();
    }

    // Render color shift
    if (m_Config.ColorShiftIntensity > 0.0f) {
        RenderColorShift();
    }

    // Render scanlines
    if (m_Config.ScanlineIntensity > 0.0f) {
        RenderScanlines();
    }

    // Render chromatic aberration
    if (m_Config.ChromaticAberrationIntensity > 0.0f) {
        RenderChromaticAberration();
    }

    // Render vignette
    if (m_Config.VignetteStrength > 0.0f) {
        RenderVignette();
    }
}

void BodycamHUD::RenderBrightnessOverlay() {
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ScreenSize);
    ImGui::Begin("BodycamBrightness", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // Calculate brightness overlay color
    float BrightnessDelta = m_CurrentBrightness - 1.0f;
    uint8_t Alpha = static_cast<uint8_t>(std::abs(BrightnessDelta) * 50.0f);

    if (BrightnessDelta > 0.0f) {
        // Brighten
        DrawList->AddRectFilled(
            ImVec2(0, 0),
            ScreenSize,
            IM_COL32(255, 255, 255, Alpha)
        );
    } else {
        // Darken
        DrawList->AddRectFilled(
            ImVec2(0, 0),
            ScreenSize,
            IM_COL32(0, 0, 0, Alpha)
        );
    }

    ImGui::End();
}

void BodycamHUD::RenderColorShift() {
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ScreenSize);
    ImGui::Begin("BodycamColorShift", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // Apply color temperature shift as a subtle overlay
    Math::Vec3 Shift = m_CurrentColorShift;
    Shift.x = std::max(0.0f, std::min(1.0f, Shift.x));
    Shift.y = std::max(0.0f, std::min(1.0f, Shift.y));
    Shift.z = std::max(0.0f, std::min(1.0f, Shift.z));

    uint8_t Alpha = static_cast<uint8_t>(m_Config.ColorShiftIntensity * 30.0f);
    DrawList->AddRectFilled(
        ImVec2(0, 0),
        ScreenSize,
        IM_COL32(
            static_cast<uint8_t>(Shift.x * 255.0f),
            static_cast<uint8_t>(Shift.y * 255.0f),
            static_cast<uint8_t>(Shift.z * 255.0f),
            Alpha
        )
    );

    ImGui::End();
}

void BodycamHUD::RenderScanlines() {
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ScreenSize);
    ImGui::Begin("BodycamScanlines", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // Draw subtle horizontal scanlines
    float LineSpacing = 4.0f;
    float Alpha = m_Config.ScanlineIntensity * 20.0f * m_ScanlineOffset;
    uint8_t LineAlpha = static_cast<uint8_t>(Alpha);

    for (float Y = 0.0f; Y < ScreenSize.y; Y += LineSpacing) {
        DrawList->AddLine(
            ImVec2(0, Y),
            ImVec2(ScreenSize.x, Y),
            IM_COL32(0, 0, 0, LineAlpha),
            1.0f
        );
    }

    ImGui::End();
}

void BodycamHUD::RenderChromaticAberration() {
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    // Chromatic aberration at edges - subtle red/blue shift
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ScreenSize);
    ImGui::Begin("BodycamChromatic", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // Draw subtle edge color shifts (red on left, blue on right)
    float EdgeWidth = ScreenSize.x * 0.05f;
    float Alpha = m_Config.ChromaticAberrationIntensity * 40.0f;
    uint8_t EdgeAlpha = static_cast<uint8_t>(Alpha);

    // Left edge (red shift)
    DrawList->AddRectFilledMultiColor(
        ImVec2(0, 0),
        ImVec2(EdgeWidth, ScreenSize.y),
        IM_COL32(255, 0, 0, EdgeAlpha),
        IM_COL32(255, 0, 0, 0),
        IM_COL32(255, 0, 0, 0),
        IM_COL32(255, 0, 0, EdgeAlpha)
    );

    // Right edge (blue shift)
    DrawList->AddRectFilledMultiColor(
        ImVec2(ScreenSize.x - EdgeWidth, 0),
        ScreenSize,
        IM_COL32(0, 0, 255, 0),
        IM_COL32(0, 0, 255, EdgeAlpha),
        IM_COL32(0, 0, 255, EdgeAlpha),
        IM_COL32(0, 0, 255, 0)
    );

    ImGui::End();
}

void BodycamHUD::RenderVignette() {
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ScreenSize);
    ImGui::Begin("BodycamVignette", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    float VignetteSize = ScreenSize.x * m_Config.VignetteSize;
    uint8_t VignetteAlpha = static_cast<uint8_t>(m_Config.VignetteStrength * 255.0f);

    // Draw corner darkening
    DrawList->AddRectFilledMultiColor(
        ImVec2(0, 0),
        ImVec2(VignetteSize, ScreenSize.y),
        IM_COL32(0, 0, 0, VignetteAlpha),
        IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, VignetteAlpha)
    );

    DrawList->AddRectFilledMultiColor(
        ImVec2(ScreenSize.x - VignetteSize, 0),
        ScreenSize,
        IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, VignetteAlpha),
        IM_COL32(0, 0, 0, VignetteAlpha),
        IM_COL32(0, 0, 0, 0)
    );

    ImGui::End();
}

void BodycamHUD::RenderFadeIn() {
    if (!ImGui::GetCurrentContext()) return;
    ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    if (ScreenSize.x <= 0.0f || ScreenSize.y <= 0.0f) return;

    // Calculate fade alpha (1.0 = fully black, 0.0 = fully transparent)
    float FadeAlpha = 1.0f;
    if (m_Config.FadeInDuration > 0.0f) {
        FadeAlpha = 1.0f - (m_FadeInTime / m_Config.FadeInDuration);
    }
    FadeAlpha = std::max(0.0f, std::min(1.0f, FadeAlpha));

    // Don't render if fully transparent (already faded in)
    if (FadeAlpha <= 0.0f) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ScreenSize);
    ImGui::Begin("BodycamFadeIn", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->AddRectFilled(
        ImVec2(0, 0),
        ScreenSize,
        IM_COL32(0, 0, 0, static_cast<int>(FadeAlpha * 255.0f))
    );

    ImGui::End();
}

} // namespace Solstice::Game


#pragma once

#include <string>
#include <imgui.h>

namespace Solstice::UI::TextEffects {

    // Shake effect - vibration/jitter animation
    void ShakeText(const std::string& Text, float Time, float Intensity = 1.0f, float Speed = 1.0f);
    
    // Bounce effect - elastic bounce animation
    void BounceText(const std::string& Text, float Time, float Amplitude = 10.0f, float Speed = 1.0f);
    
    // Rotation effect - spinning text animation
    void RotateText(const std::string& Text, float Time, float Speed = 1.0f, bool Oscillate = false);
    
    // Scale effect - size pulsing animation
    void ScaleText(const std::string& Text, float Time, float MinScale = 0.8f, float MaxScale = 1.2f, float Speed = 1.0f);
    
    // Wave effect - sinusoidal wave motion across text
    void WaveText(const std::string& Text, float Time, float Amplitude = 5.0f, float Speed = 1.0f, float Frequency = 1.0f);
    
    // Glow effect - outline/shadow glow with configurable intensity
    void GlowText(const std::string& Text, const ImVec4& Color, float Time = 0.0f, float Intensity = 1.0f, float Speed = 1.0f);

} // namespace Solstice::UI::TextEffects

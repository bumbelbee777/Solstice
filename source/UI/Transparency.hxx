#pragma once

#include <imgui.h>

namespace Solstice::UI::Transparency {

    // Blend mode enumeration
    enum class BlendMode {
        Normal,      // Standard alpha blending
        Additive,    // Additive blending
        Multiply,    // Multiply blending
        Screen       // Screen blending
    };

    // Gradient direction for transparency gradients
    enum class GradientDirection {
        TopToBottom,
        BottomToTop,
        LeftToRight,
        RightToLeft,
        DiagonalTopLeft,
        DiagonalTopRight
    };

    // Push per-widget transparency (alpha value 0.0 to 1.0)
    void PushAlpha(float Alpha);
    
    // Pop per-widget transparency
    void PopAlpha();
    
    // Push gradient transparency
    void PushGradientAlpha(float StartAlpha, float EndAlpha, GradientDirection Direction);
    
    // Pop gradient transparency
    void PopGradientAlpha();
    
    // Push animated transparency (time-based alpha transition)
    void PushAnimatedAlpha(float Time, float Speed = 1.0f, float MinAlpha = 0.0f, float MaxAlpha = 1.0f);
    
    // Pop animated transparency
    void PopAnimatedAlpha();
    
    // Push blend mode
    void PushBlendMode(BlendMode Mode);
    
    // Pop blend mode
    void PopBlendMode();
    
    // Combined transparency and blend mode push
    void PushTransparency(float Alpha, BlendMode Mode = BlendMode::Normal);
    
    // Combined pop
    void PopTransparency();

} // namespace Solstice::UI::Transparency

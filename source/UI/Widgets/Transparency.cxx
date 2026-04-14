#include <UI/Widgets/Transparency.hxx>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

namespace Solstice::UI::Transparency {

    void PushAlpha(float Alpha) {
        // Clamp alpha to valid range
        Alpha = (Alpha < 0.0f) ? 0.0f : (Alpha > 1.0f) ? 1.0f : Alpha;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Alpha);
    }

    void PopAlpha() {
        ImGui::PopStyleVar();
    }

    void PushGradientAlpha(float StartAlpha, float EndAlpha, GradientDirection Direction) {
        // Clamp alphas
        StartAlpha = (StartAlpha < 0.0f) ? 0.0f : (StartAlpha > 1.0f) ? 1.0f : StartAlpha;
        EndAlpha = (EndAlpha < 0.0f) ? 0.0f : (EndAlpha > 1.0f) ? 1.0f : EndAlpha;
        
        // Store gradient parameters in ImGui style vars
        // We'll use window alpha and a custom approach for gradient
        // For now, use average alpha as approximation
        // Full gradient requires per-vertex alpha which is more complex
        float avgAlpha = (StartAlpha + EndAlpha) * 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, avgAlpha);
        
        // Note: Full gradient transparency would require custom rendering
        // This is a simplified version. For full gradient support, we'd need
        // to modify the draw list rendering which is more complex.
    }

    void PopGradientAlpha() {
        ImGui::PopStyleVar();
    }

    void PushAnimatedAlpha(float Time, float Speed, float MinAlpha, float MaxAlpha) {
        // Clamp alphas
        MinAlpha = (MinAlpha < 0.0f) ? 0.0f : (MinAlpha > 1.0f) ? 1.0f : MinAlpha;
        MaxAlpha = (MaxAlpha < 0.0f) ? 0.0f : (MaxAlpha > 1.0f) ? 1.0f : MaxAlpha;
        
        // Calculate animated alpha using sine wave
        float animatedAlpha = MinAlpha + (MaxAlpha - MinAlpha) * (sin(Time * Speed * 3.14159f) * 0.5f + 0.5f);
        
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, animatedAlpha);
    }

    void PopAnimatedAlpha() {
        ImGui::PopStyleVar();
    }

    void PushBlendMode(BlendMode Mode) {
        // ImGui doesn't directly support blend modes, but we can use style colors
        // to approximate some effects. For full blend mode support, we'd need
        // to modify the rendering pipeline which is beyond ImGui's scope.
        // This is a placeholder for future enhancement with custom rendering.
        
        // For now, we'll store the blend mode preference
        // Actual implementation would require BGFX render state changes
        // which should be done at the UISystem level
    }

    void PopBlendMode() {
        // Placeholder - see PushBlendMode
    }

    void PushTransparency(float Alpha, BlendMode Mode) {
        PushAlpha(Alpha);
        PushBlendMode(Mode);
    }

    void PopTransparency() {
        PopBlendMode();
        PopAlpha();
    }

} // namespace Solstice::UI::Transparency

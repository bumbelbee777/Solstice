#pragma once

#include "../Solstice.hxx"
#include <UI/Animation.hxx>
#include <imgui.h>
#include <bgfx/bgfx.h>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <vector>

namespace Solstice::UI {

enum class ShadowType {
    DropShadow,      // Standard drop shadow
    InnerShadow,     // Inner shadow (inset)
    ColoredShadow,   // Shadow with custom color
    Glow,            // Glow effect (soft shadow)
    Multiple         // Multiple shadow layers
};

struct ShadowParams {
    ImVec2 Offset{2.0f, 2.0f};           // Shadow offset (x, y)
    float BlurRadius{4.0f};               // Blur amount (approximated)
    float Spread{0.0f};                   // Shadow spread
    ImU32 Color{IM_COL32(0, 0, 0, 128)};  // Shadow color (with alpha)
    ShadowType Type{ShadowType::DropShadow};
    bool Inset{false};                    // For inner shadows
};

enum class BlurType {
    Gaussian,        // Gaussian blur
    Box,            // Box blur (faster)
    Motion,         // Motion blur (directional)
    Background      // Background blur (frosted glass effect)
};

struct BlurParams {
    float Radius{5.0f};                   // Blur radius
    BlurType Type{BlurType::Gaussian};
    ImVec2 Direction{1.0f, 0.0f};         // For motion blur
    float Strength{1.0f};                 // Blur strength (0.0 to 1.0)
    bool PreserveAlpha{true};             // Preserve alpha channel
};

struct MotionBlurData {
    ImVec2 Velocity;                       // Velocity vector (pixels per frame)
    float Strength{1.0f};                 // Motion blur strength
    int Samples{8};                        // Number of samples for motion blur
};

class SOLSTICE_API VisualEffects {
public:
    // Shadow effects (using ImDrawList - no render-to-texture needed for simple shadows)
    static void DrawDropShadow(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                              const ShadowParams& Params, float Rounding = 0.0f);

    static void DrawInnerShadow(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                               const ShadowParams& Params, float Rounding = 0.0f);

    static void DrawGlow(ImDrawList* DrawList, const ImVec2& Center, float Radius,
                        const ShadowParams& Params, uint32_t Segments = 32);

    static void DrawMultipleShadows(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                    const std::vector<ShadowParams>& Shadows, float Rounding = 0.0f);

    // Blur effects (requires render-to-texture for advanced blur)
    // For now, we'll provide a simple approximation using multiple layers
    static void DrawBlurredRect(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                               ImU32 BaseColor, const BlurParams& Params);
    
    // Motion blur for UI elements
    static void DrawMotionBlurredRect(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                      ImU32 Color, const MotionBlurData& MotionData);

    // Combined effects
    static void DrawWidgetWithEffects(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                     ImU32 FillColor, const ShadowParams* Shadow = nullptr,
                                     const BlurParams* Blur = nullptr, float Rounding = 0.0f);

    // World-space effects
    static void DrawWorldSpaceShadow(ImDrawList* DrawList, const Math::Vec3& WorldPos,
                                    const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                                    int ScreenWidth, int ScreenHeight, const ShadowParams& Params);
};

// Enhanced transparency system (extends existing Transparency namespace)
namespace Transparency {
    // Additional transparency effects
    SOLSTICE_API void PushBackdropBlur(float BlurRadius, float Alpha = 1.0f);
    SOLSTICE_API void PopBackdropBlur();

    SOLSTICE_API void PushShadow(const ShadowParams& Params);
    SOLSTICE_API void PopShadow();

    SOLSTICE_API void PushBlur(const BlurParams& Params);
    SOLSTICE_API void PopBlur();

    // Animated transparency
    SOLSTICE_API void PushAnimatedBlur(float Time, float Speed, float MinBlur, float MaxBlur);
    SOLSTICE_API void PopAnimatedBlur();
}

} // namespace Solstice::UI

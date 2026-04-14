#include <UI/Motion/VisualEffects.hxx>
#include <UI/Viewport/ViewportUI.hxx>
#include <cmath>
#include <algorithm>

namespace Solstice::UI {

// Helper to extract alpha from color
static uint8_t GetAlpha(ImU32 Color) {
    return (Color >> IM_COL32_A_SHIFT) & 0xFF;
}

static ImU32 SetAlpha(ImU32 Color, uint8_t Alpha) {
    return (Color & ~IM_COL32_A_MASK) | (Alpha << IM_COL32_A_SHIFT);
}

// Draw drop shadow
void VisualEffects::DrawDropShadow(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                  const ShadowParams& Params, float Rounding) {
    if (!DrawList) return;

    // Create shadow by drawing multiple offset rectangles with decreasing alpha
    int layers = static_cast<int>(Params.BlurRadius) + 1;
    layers = std::max(1, std::min(layers, 8)); // Limit layers for performance

    for (int i = layers; i >= 1; --i) {
        float offsetScale = static_cast<float>(i) / static_cast<float>(layers);
        ImVec2 offset(Params.Offset.x * offsetScale, Params.Offset.y * offsetScale);

        uint8_t alpha = GetAlpha(Params.Color);
        alpha = static_cast<uint8_t>(alpha * (1.0f / static_cast<float>(layers)));
        ImU32 shadowColor = SetAlpha(Params.Color, alpha);

        ImVec2 shadowMin(Min.x + offset.x, Min.y + offset.y);
        ImVec2 shadowMax(Max.x + offset.x, Max.y + offset.y);

        DrawList->AddRectFilled(shadowMin, shadowMax, shadowColor, Rounding);
    }
}

// Draw inner shadow
void VisualEffects::DrawInnerShadow(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                   const ShadowParams& Params, float Rounding) {
    if (!DrawList) return;

    // Inner shadow is drawn as a gradient from the edges
    int layers = static_cast<int>(Params.BlurRadius) + 1;
    layers = std::max(1, std::min(layers, 8));

    for (int i = 0; i < layers; ++i) {
        float inset = static_cast<float>(i) * 2.0f;
        ImVec2 innerMin(Min.x + inset, Min.y + inset);
        ImVec2 innerMax(Max.x - inset, Max.y - inset);

        if (innerMin.x >= innerMax.x || innerMin.y >= innerMax.y) break;

        uint8_t alpha = GetAlpha(Params.Color);
        alpha = static_cast<uint8_t>(alpha * (1.0f - static_cast<float>(i) / static_cast<float>(layers)));
        ImU32 shadowColor = SetAlpha(Params.Color, alpha);

        // Draw as border
        DrawList->AddRect(innerMin, innerMax, shadowColor, Rounding, 0, 1.0f);
    }
}

// Draw glow effect
void VisualEffects::DrawGlow(ImDrawList* DrawList, const ImVec2& Center, float Radius,
                             const ShadowParams& Params, uint32_t Segments) {
    if (!DrawList || Segments < 3) return;

    // Draw multiple circles with increasing radius and decreasing alpha
    int layers = static_cast<int>(Params.BlurRadius) + 1;
    layers = std::max(1, std::min(layers, 10));

    for (int i = layers; i >= 1; --i) {
        float layerRadius = Radius + static_cast<float>(i) * 2.0f;
        uint8_t alpha = GetAlpha(Params.Color);
        alpha = static_cast<uint8_t>(alpha * (1.0f / static_cast<float>(layers)));
        ImU32 glowColor = SetAlpha(Params.Color, alpha);

        DrawList->AddCircleFilled(Center, layerRadius, glowColor, Segments);
    }
}

// Draw multiple shadows
void VisualEffects::DrawMultipleShadows(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                       const std::vector<ShadowParams>& Shadows, float Rounding) {
    if (!DrawList) return;

    for (const auto& shadow : Shadows) {
        switch (shadow.Type) {
            case ShadowType::DropShadow:
                DrawDropShadow(DrawList, Min, Max, shadow, Rounding);
                break;
            case ShadowType::InnerShadow:
                DrawInnerShadow(DrawList, Min, Max, shadow, Rounding);
                break;
            case ShadowType::Glow:
                // For glow on rectangle, use center and approximate radius
                {
                    ImVec2 center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
                    float radius = std::max((Max.x - Min.x), (Max.y - Min.y)) * 0.5f;
                    DrawGlow(DrawList, center, radius, shadow);
                }
                break;
            default:
                DrawDropShadow(DrawList, Min, Max, shadow, Rounding);
                break;
        }
    }
}

// Draw blurred rect (simplified approximation)
void VisualEffects::DrawBlurredRect(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                   ImU32 BaseColor, const BlurParams& Params) {
    if (!DrawList) return;

    // Simple blur approximation using multiple semi-transparent layers
    int layers = static_cast<int>(Params.Radius * Params.Strength) + 1;
    layers = std::max(1, std::min(layers, 5));

    uint8_t baseAlpha = GetAlpha(BaseColor);

    for (int i = 0; i < layers; ++i) {
        float offset = static_cast<float>(i) * 1.0f;
        ImVec2 blurMin(Min.x - offset, Min.y - offset);
        ImVec2 blurMax(Max.x + offset, Max.y + offset);

        uint8_t alpha = static_cast<uint8_t>(baseAlpha * (1.0f / static_cast<float>(layers + 1)));
        ImU32 blurColor = SetAlpha(BaseColor, alpha);

        DrawList->AddRectFilled(blurMin, blurMax, blurColor, 0.0f);
    }
}

// Draw motion blurred rect
void VisualEffects::DrawMotionBlurredRect(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                         ImU32 Color, const MotionBlurData& MotionData) {
    if (!DrawList) return;
    
    // Calculate velocity magnitude
    float velMag = std::sqrt(MotionData.Velocity.x * MotionData.Velocity.x + 
                            MotionData.Velocity.y * MotionData.Velocity.y);
    
    if (velMag < 0.1f) {
        // No motion, just draw normally
        DrawList->AddRectFilled(Min, Max, Color, 0.0f);
        return;
    }
    
    // Normalize velocity direction
    ImVec2 velDir(MotionData.Velocity.x / velMag, MotionData.Velocity.y / velMag);
    
    // Scale velocity by strength
    float scaledVel = velMag * MotionData.Strength;
    
    // Limit samples for performance
    int samples = std::max(1, std::min(MotionData.Samples, 16));
    
    uint8_t baseAlpha = GetAlpha(Color);
    
    // Draw motion blur trail along velocity direction
    for (int i = 0; i <= samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples);
        float offset = scaledVel * t;
        
        ImVec2 offsetVec(velDir.x * offset, velDir.y * offset);
        ImVec2 blurMin(Min.x + offsetVec.x, Min.y + offsetVec.y);
        ImVec2 blurMax(Max.x + offsetVec.x, Max.y + offsetVec.y);
        
        // Alpha decreases with distance
        float alphaFactor = 1.0f - (t * 0.7f); // Fade out to 30% alpha
        uint8_t alpha = static_cast<uint8_t>(baseAlpha * alphaFactor);
        ImU32 blurColor = SetAlpha(Color, alpha);
        
        DrawList->AddRectFilled(blurMin, blurMax, blurColor, 0.0f);
    }
}

// Draw widget with effects
void VisualEffects::DrawWidgetWithEffects(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                                          ImU32 FillColor, const ShadowParams* Shadow,
                                          const BlurParams* Blur, float Rounding) {
    if (!DrawList) return;

    // Draw shadow first (behind)
    if (Shadow) {
        DrawDropShadow(DrawList, Min, Max, *Shadow, Rounding);
    }

    // Draw blur (if specified, draw as background)
    if (Blur) {
        DrawBlurredRect(DrawList, Min, Max, FillColor, *Blur);
    }

    // Draw main widget
    DrawList->AddRectFilled(Min, Max, FillColor, Rounding);
}

// Draw world-space shadow
void VisualEffects::DrawWorldSpaceShadow(ImDrawList* DrawList, const Math::Vec3& WorldPos,
                                        const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                                        int ScreenWidth, int ScreenHeight, const ShadowParams& Params) {
    if (!DrawList) return;

    // Project world position to screen
    Math::Vec2 screenPos = ViewportUI::ProjectToScreen(WorldPos, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

    // Draw shadow at screen position
    float shadowSize = Params.BlurRadius * 2.0f;
    ImVec2 center(screenPos.x, screenPos.y);
    DrawGlow(DrawList, center, shadowSize, Params);
}

// Enhanced transparency namespace
namespace Transparency {

    // Stack for backdrop blur
    static std::vector<float> s_BackdropBlurStack;
    static std::vector<float> s_BackdropBlurAlphaStack;

    void PushBackdropBlur(float BlurRadius, float Alpha) {
        s_BackdropBlurStack.push_back(BlurRadius);
        s_BackdropBlurAlphaStack.push_back(Alpha);
        // Note: Actual backdrop blur requires render-to-texture, this is a placeholder
    }

    void PopBackdropBlur() {
        if (!s_BackdropBlurStack.empty()) {
            s_BackdropBlurStack.pop_back();
            s_BackdropBlurAlphaStack.pop_back();
        }
    }

    // Stack for shadow
    static std::vector<ShadowParams> s_ShadowStack;

    void PushShadow(const ShadowParams& Params) {
        s_ShadowStack.push_back(Params);
    }

    void PopShadow() {
        if (!s_ShadowStack.empty()) {
            s_ShadowStack.pop_back();
        }
    }

    // Stack for blur
    static std::vector<BlurParams> s_BlurStack;

    void PushBlur(const BlurParams& Params) {
        s_BlurStack.push_back(Params);
    }

    void PopBlur() {
        if (!s_BlurStack.empty()) {
            s_BlurStack.pop_back();
        }
    }

    // Animated blur
    static std::vector<float> s_AnimatedBlurTimeStack;
    static std::vector<float> s_AnimatedBlurSpeedStack;
    static std::vector<float> s_AnimatedBlurMinStack;
    static std::vector<float> s_AnimatedBlurMaxStack;

    void PushAnimatedBlur(float Time, float Speed, float MinBlur, float MaxBlur) {
        s_AnimatedBlurTimeStack.push_back(Time);
        s_AnimatedBlurSpeedStack.push_back(Speed);
        s_AnimatedBlurMinStack.push_back(MinBlur);
        s_AnimatedBlurMaxStack.push_back(MaxBlur);

        // Calculate current blur value
        float currentBlur = MinBlur + (MaxBlur - MinBlur) * (std::sin(Time * Speed * 3.14159f) * 0.5f + 0.5f);

        BlurParams params;
        params.Radius = currentBlur;
        params.Type = BlurType::Gaussian;
        PushBlur(params);
    }

    void PopAnimatedBlur() {
        if (!s_AnimatedBlurTimeStack.empty()) {
            s_AnimatedBlurTimeStack.pop_back();
            s_AnimatedBlurSpeedStack.pop_back();
            s_AnimatedBlurMinStack.pop_back();
            s_AnimatedBlurMaxStack.pop_back();
            PopBlur();
        }
    }

} // namespace Transparency

} // namespace Solstice::UI

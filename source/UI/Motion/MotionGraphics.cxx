#include <UI/Motion/MotionGraphics.hxx>
#include <UI/Motion/Primitives.hxx>
#include <UI/Motion/VisualEffects.hxx>
#include <imgui.h>
#include <algorithm>
#include <map>

namespace Solstice::UI::MotionGraphics {

// Velocity tracking for UI elements
static std::map<std::string, ImVec2> g_previousPositions;

// Helper to calculate velocity
static ImVec2 CalculateVelocity(const std::string& elementId, const ImVec2& currentPos) {
    auto it = g_previousPositions.find(elementId);
    if (it != g_previousPositions.end()) {
        ImVec2 velocity(
            currentPos.x - it->second.x,
            currentPos.y - it->second.y
        );
        g_previousPositions[elementId] = currentPos;
        return velocity;
    } else {
        g_previousPositions[elementId] = currentPos;
        return ImVec2(0.0f, 0.0f);
    }
}

// Animated button
bool AnimatedButton(const std::string& Label, const Solstice::UI::Animation::AnimationClip& Animation, const std::function<void()>& OnClick) {
    ImVec2 position, scale;
    float rotation, alpha;
    ImVec4 color;

    // Get current time from ImGui (approximate)
    static float currentTime = 0.0f;
    currentTime += ImGui::GetIO().DeltaTime;
    if (currentTime > Animation.GetDuration()) {
        currentTime = 0.0f;
    }

    Animation.Evaluate(currentTime, position, scale, rotation, color, alpha);
    
    // Calculate velocity for motion blur
    ImVec2 velocity = CalculateVelocity("AnimatedButton_" + Label, position);

    // Apply animation
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, color.w));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, color.w));

    bool clicked = ImGui::Button(Label.c_str());

    if (clicked && OnClick) {
        OnClick();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    return clicked;
}

// Animated text
void AnimatedText(const std::string& Text, const Solstice::UI::Animation::AnimationClip& Animation) {
    ImVec2 position, scale;
    float rotation, alpha;
    ImVec4 color;

    static float currentTime = 0.0f;
    currentTime += ImGui::GetIO().DeltaTime;
    if (currentTime > Animation.GetDuration()) {
        currentTime = 0.0f;
    }

    Animation.Evaluate(currentTime, position, scale, rotation, color, alpha);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::TextColored(color, "%s", Text.c_str());
    ImGui::PopStyleVar();
}

// Animated image
void AnimatedImage(ImTextureID Texture, const ImVec2& Size, const Solstice::UI::Animation::AnimationClip& Animation) {
    ImVec2 position, scale;
    float rotation, alpha;
    ImVec4 color;

    static float currentTime = 0.0f;
    currentTime += ImGui::GetIO().DeltaTime;
    if (currentTime > Animation.GetDuration()) {
        currentTime = 0.0f;
    }

    Animation.Evaluate(currentTime, position, scale, rotation, color, alpha);

    ImVec2 finalSize(Size.x * scale.x, Size.y * scale.y);
    ImVec4 tintCol = ImVec4(color.x, color.y, color.z, color.w * alpha);
    ImVec4 borderCol = ImVec4(0, 0, 0, 0);
    ImGui::Image(Texture, finalSize, ImVec2(0, 0), ImVec2(1, 1), tintCol, borderCol);
}

// Transitioning window
bool TransitioningWindow(const std::string& Title, bool* Open, Transition& Transition) {
    if (!Open || !*Open) {
        Transition.Reset();
        return false;
    }

    Transition.Update(ImGui::GetIO().DeltaTime);

    float alpha = 1.0f;
    if (auto* fadeTransition = dynamic_cast<FadeTransition*>(&Transition)) {
        fadeTransition->Apply(alpha);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    bool result = ImGui::Begin(Title.c_str(), Open);
    ImGui::PopStyleVar();

    return result;
}

// Transitioning panel
void TransitioningPanel(const std::string& Id, Transition& Transition, const std::function<void()>& Content) {
    Transition.Update(ImGui::GetIO().DeltaTime);

    float alpha = 1.0f;
    if (auto* fadeTransition = dynamic_cast<FadeTransition*>(&Transition)) {
        fadeTransition->Apply(alpha);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    if (ImGui::BeginChild(Id.c_str())) {
        if (Content) {
            Content();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Animated world-space dialog
void AnimatedWorldSpaceDialog(ViewportUI::WorldSpaceDialog& Dialog, const Solstice::UI::Animation::AnimationClip& Animation, float DeltaTime) {
    // Note: This would require storing animation state in the dialog
    // For now, this is a placeholder that shows the pattern
    static float currentTime = 0.0f;
    currentTime += DeltaTime;

    ImVec2 position, scale;
    float rotation, alpha;
    ImVec4 color;
    Animation.Evaluate(currentTime, position, scale, rotation, color, alpha);

    // Apply animation to dialog (would need to extend WorldSpaceDialog)
    // Dialog.SetPosition(...);
    // Dialog.SetSize(...);
}

// Animated world-space label
void AnimatedWorldSpaceLabel(ViewportUI::WorldSpaceLabel& Label, const Animation::AnimationClip& Animation, float DeltaTime) {
    static float currentTime = 0.0f;
    currentTime += DeltaTime;

    ImVec2 position, scale;
    float rotation, alpha;
    ImVec4 color;
    Animation.Evaluate(currentTime, position, scale, rotation, color, alpha);

    // Apply animation (would need to extend WorldSpaceLabel)
}

// Animated world-space button
void AnimatedWorldSpaceButton(ViewportUI::WorldSpaceButton& Button, const Solstice::UI::Animation::AnimationClip& Animation, float DeltaTime) {
    static float currentTime = 0.0f;
    currentTime += DeltaTime;

    ImVec2 position, scale;
    float rotation, alpha;
    ImVec4 color;
    Animation.Evaluate(currentTime, position, scale, rotation, color, alpha);

    // Apply animation (would need to extend WorldSpaceButton)
}

// Render sprite
void RenderSprite(const Solstice::UI::Sprite& Sprite, ImDrawList* DrawList) {
    if (!DrawList) {
        DrawList = ImGui::GetWindowDrawList();
    }
    const_cast<Solstice::UI::Sprite&>(Sprite).Render(DrawList);
}

// Render sprite world-space
void RenderSpriteWorldSpace(const Solstice::UI::Sprite& Sprite, const Math::Vec3& WorldPos,
                           const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                           int ScreenWidth, int ScreenHeight, ImDrawList* DrawList) {
    const_cast<Solstice::UI::Sprite&>(Sprite).RenderWorldSpace(WorldPos, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight, DrawList);
}

// Draw animated polygon
void DrawAnimatedPolygon(ImDrawList* DrawList, const ImVec2& Center, const Animation::AnimationTrack<float>& RadiusAnimation,
                        uint32_t Sides, ImU32 Color, float CurrentTime) {
    if (!DrawList) return;

    float radius = RadiusAnimation.Evaluate(CurrentTime);
    Primitives::DrawPolygonFilled(DrawList, Center, radius, Sides, Color);
}

// Draw animated gradient
void DrawAnimatedGradient(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                         const Solstice::UI::Animation::AnimationTrack<ImU32>& ColorStartAnimation,
                         const Solstice::UI::Animation::AnimationTrack<ImU32>& ColorEndAnimation,
                         float CurrentTime) {
    if (!DrawList) return;

    ImU32 colorStart = ColorStartAnimation.Evaluate(CurrentTime);
    ImU32 colorEnd = ColorEndAnimation.Evaluate(CurrentTime);

    Primitives::DrawLinearGradientRect(DrawList, Min, Max, colorStart, colorEnd);
}

// Button with shadow
bool ButtonWithShadow(const std::string& Label, const ShadowParams& Shadow, const std::function<void()>& OnClick) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 buttonSize = ImGui::CalcTextSize(Label.c_str());
    buttonSize.x += ImGui::GetStyle().FramePadding.x * 2.0f;
    buttonSize.y += ImGui::GetStyle().FramePadding.y * 2.0f;

    ImVec2 min = cursorPos;
    ImVec2 max = ImVec2(cursorPos.x + buttonSize.x, cursorPos.y + buttonSize.y);

    // Draw shadow
    VisualEffects::DrawDropShadow(drawList, min, max, Shadow, ImGui::GetStyle().FrameRounding);

    // Draw button
    bool clicked = ImGui::Button(Label.c_str());

    if (clicked && OnClick) {
        OnClick();
    }

    return clicked;
}

// Panel with blur
void PanelWithBlur(const std::string& Id, const BlurParams& Blur, const std::function<void()>& Content) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    if (ImGui::BeginChild(Id.c_str())) {
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max = ImVec2(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());

        // Draw blur background
        ImU32 blurColor = IM_COL32(255, 255, 255, 200);
        VisualEffects::DrawBlurredRect(drawList, min, max, blurColor, Blur);

        if (Content) {
            Content();
        }
    }
    ImGui::EndChild();
}

// Window with effects
void WindowWithEffects(const std::string& Title, bool* Open, const ShadowParams* Shadow, const BlurParams* Blur) {
    if (ImGui::Begin(Title.c_str(), Open)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max = ImVec2(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());

        // Draw effects
        if (Shadow) {
            VisualEffects::DrawDropShadow(drawList, min, max, *Shadow, ImGui::GetStyle().WindowRounding);
        }

        if (Blur) {
            ImU32 blurColor = IM_COL32(50, 50, 50, 200);
            VisualEffects::DrawBlurredRect(drawList, min, max, blurColor, *Blur);
        }
    }
    ImGui::End();
}

// Animated glow
void AnimatedGlow(ImDrawList* DrawList, const ImVec2& Center, float Radius, const Solstice::UI::Animation::AnimationTrack<float>& BlurAnimation,
                 const Solstice::UI::Animation::AnimationTrack<ImU32>& ColorAnimation, float CurrentTime) {
    if (!DrawList) return;

    float blurRadius = BlurAnimation.Evaluate(CurrentTime);
    ImU32 color = ColorAnimation.Evaluate(CurrentTime);

    ShadowParams params;
    params.BlurRadius = blurRadius;
    params.Color = color;
    params.Type = ShadowType::Glow;

    VisualEffects::DrawGlow(DrawList, Center, Radius, params);
}

// Animated shadow
void AnimatedShadow(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                   const Solstice::UI::Animation::AnimationTrack<ShadowParams>& ShadowAnimation, float CurrentTime) {
    if (!DrawList) return;

    ShadowParams params = ShadowAnimation.Evaluate(CurrentTime);
    VisualEffects::DrawDropShadow(DrawList, Min, Max, params);
}

} // namespace Solstice::UI::MotionGraphics

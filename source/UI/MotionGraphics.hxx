#pragma once

#include "../Solstice.hxx"
#include <UI/Animation.hxx>
#include <UI/Transition.hxx>
#include <UI/Sprite.hxx>
#include <UI/VisualEffects.hxx>
#include <UI/ViewportUI.hxx>
#include <imgui.h>
#include <functional>
#include <string>

namespace Solstice::UI::MotionGraphics {

using ShadowParams = Solstice::UI::ShadowParams;
using BlurParams = Solstice::UI::BlurParams;

// Animated widgets
SOLSTICE_API bool AnimatedButton(const std::string& Label, const Solstice::UI::Animation::AnimationClip& Animation, const std::function<void()>& OnClick = nullptr);
SOLSTICE_API void AnimatedText(const std::string& Text, const Solstice::UI::Animation::AnimationClip& Animation);
SOLSTICE_API void AnimatedImage(ImTextureID Texture, const ImVec2& Size, const Solstice::UI::Animation::AnimationClip& Animation);

// Transitioning widgets
SOLSTICE_API bool TransitioningWindow(const std::string& Title, bool* Open, Transition& Transition);
SOLSTICE_API void TransitioningPanel(const std::string& Id, Transition& Transition, const std::function<void()>& Content);

// World-space animated elements
SOLSTICE_API void AnimatedWorldSpaceDialog(ViewportUI::WorldSpaceDialog& Dialog, const Solstice::UI::Animation::AnimationClip& Animation, float DeltaTime);
SOLSTICE_API void AnimatedWorldSpaceLabel(ViewportUI::WorldSpaceLabel& Label, const Solstice::UI::Animation::AnimationClip& Animation, float DeltaTime);
SOLSTICE_API void AnimatedWorldSpaceButton(ViewportUI::WorldSpaceButton& Button, const Solstice::UI::Animation::AnimationClip& Animation, float DeltaTime);

// Sprite rendering
SOLSTICE_API void RenderSprite(const Sprite& Sprite, ImDrawList* DrawList);
SOLSTICE_API void RenderSpriteWorldSpace(const Sprite& Sprite, const Math::Vec3& WorldPos,
                            const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                            int ScreenWidth, int ScreenHeight, ImDrawList* DrawList);

// Primitive rendering with animations
SOLSTICE_API void DrawAnimatedPolygon(ImDrawList* DrawList, const ImVec2& Center, const Solstice::UI::Animation::AnimationTrack<float>& RadiusAnimation,
                        uint32_t Sides, ImU32 Color, float CurrentTime);
SOLSTICE_API void DrawAnimatedGradient(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                         const Solstice::UI::Animation::AnimationTrack<ImU32>& ColorStartAnimation,
                         const Solstice::UI::Animation::AnimationTrack<ImU32>& ColorEndAnimation,
                         float CurrentTime);

// Widgets with visual effects
SOLSTICE_API bool ButtonWithShadow(const std::string& Label, const ShadowParams& Shadow, const std::function<void()>& OnClick = nullptr);
SOLSTICE_API void PanelWithBlur(const std::string& Id, const BlurParams& Blur, const std::function<void()>& Content);
SOLSTICE_API void WindowWithEffects(const std::string& Title, bool* Open, const ShadowParams* Shadow, const BlurParams* Blur);

// Animated effects
SOLSTICE_API void AnimatedGlow(ImDrawList* DrawList, const ImVec2& Center, float Radius, const Solstice::UI::Animation::AnimationTrack<float>& BlurAnimation,
                 const Solstice::UI::Animation::AnimationTrack<ImU32>& ColorAnimation, float CurrentTime);
SOLSTICE_API void AnimatedShadow(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max,
                   const Solstice::UI::Animation::AnimationTrack<ShadowParams>& ShadowAnimation, float CurrentTime);

} // namespace Solstice::UI::MotionGraphics

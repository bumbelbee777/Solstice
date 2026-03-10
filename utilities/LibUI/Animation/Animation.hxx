#pragma once

#include "LibUI/Core/Core.hxx"
#include <functional>
#include <vector>
#include <memory>

namespace LibUI::Animation {

// Easing function types
enum class EasingFunction {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Bounce,
    Elastic,
    Back
};

// Easing functions
LIBUI_API float EaseLinear(float t);
LIBUI_API float EaseIn(float t);
LIBUI_API float EaseOut(float t);
LIBUI_API float EaseInOut(float t);
LIBUI_API float EaseBounce(float t);
LIBUI_API float EaseElastic(float t);
LIBUI_API float EaseBack(float t);
LIBUI_API float ApplyEasing(EasingFunction func, float t);

// Animator class for value interpolation
class LIBUI_API Animator {
public:
    Animator();
    ~Animator() = default;

    // Update animation with delta time
    void Update(float deltaTime);

    // Get current value
    float GetValue() const { return m_CurrentValue; }

    // Set target value and duration
    void SetTarget(float target, float duration, EasingFunction easing = EasingFunction::EaseInOut);

    // Set current value immediately
    void SetValue(float value);

    // Check if animation is complete
    bool IsComplete() const { return m_Time >= m_Duration; }

    // Reset animation
    void Reset();

    // Set easing function
    void SetEasing(EasingFunction easing) { m_Easing = easing; }

    // Get/set callbacks
    using OnCompleteCallback = std::function<void()>;
    void SetOnCompleteCallback(OnCompleteCallback callback) { m_OnCompleteCallback = callback; }

private:
    float m_StartValue{0.0f};
    float m_TargetValue{0.0f};
    float m_CurrentValue{0.0f};
    float m_Duration{0.0f};
    float m_Time{0.0f};
    EasingFunction m_Easing{EasingFunction::EaseInOut};
    bool m_IsAnimating{false};
    OnCompleteCallback m_OnCompleteCallback;
};

// Keyframe for keyframe animation
struct Keyframe {
    float Time{0.0f};
    float Value{0.0f};
    EasingFunction Easing{EasingFunction::Linear};
};

// Keyframe animation class
class LIBUI_API KeyframeAnimation {
public:
    KeyframeAnimation();
    ~KeyframeAnimation() = default;

    // Add keyframe
    void AddKeyframe(float time, float value, EasingFunction easing = EasingFunction::Linear);

    // Update animation with delta time
    void Update(float deltaTime);

    // Get current value
    float GetValue() const { return m_CurrentValue; }

    // Set time directly
    void SetTime(float time);

    // Get current time
    float GetTime() const { return m_Time; }

    // Get duration (time of last keyframe)
    float GetDuration() const;

    // Reset animation
    void Reset();

    // Check if complete
    bool IsComplete() const;

    // Set looping
    void SetLooping(bool looping) { m_Looping = looping; }
    bool IsLooping() const { return m_Looping; }

    // Set callbacks
    using OnCompleteCallback = std::function<void()>;
    void SetOnCompleteCallback(OnCompleteCallback callback) { m_OnCompleteCallback = callback; }

private:
    std::vector<Keyframe> m_Keyframes;
    float m_Time{0.0f};
    float m_CurrentValue{0.0f};
    bool m_Looping{false};
    OnCompleteCallback m_OnCompleteCallback;

    void UpdateValue();
};

// Common animation helpers
LIBUI_API void FadeIn(Animator& animator, float duration = 1.0f);
LIBUI_API void FadeOut(Animator& animator, float duration = 1.0f);
LIBUI_API void SlideIn(Animator& animator, float start_pos, float end_pos, float duration = 1.0f);
LIBUI_API void SlideOut(Animator& animator, float start_pos, float end_pos, float duration = 1.0f);
LIBUI_API void Scale(Animator& animator, float start_scale, float end_scale, float duration = 1.0f);

} // namespace LibUI::Animation


#include "LibUI/Animation/Animation.hxx"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace LibUI::Animation {

// Easing functions
float EaseLinear(float t) {
    return t;
}

float EaseIn(float t) {
    return t * t;
}

float EaseOut(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

float EaseInOut(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float EaseBounce(float t) {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
        float t2 = t - 1.5f / 2.75f;
        return 7.5625f * t2 * t2 + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        float t2 = t - 2.25f / 2.75f;
        return 7.5625f * t2 * t2 + 0.9375f;
    } else {
        float t2 = t - 2.625f / 2.75f;
        return 7.5625f * t2 * t2 + 0.984375f;
    }
}

float EaseElastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;

    float p = 0.3f;
    float s = p / 4.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - s) * (2.0f * 3.14159f) / p) + 1.0f;
}

float EaseBack(float t) {
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}

float ApplyEasing(EasingFunction func, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    switch (func) {
        case EasingFunction::Linear: return EaseLinear(t);
        case EasingFunction::EaseIn: return EaseIn(t);
        case EasingFunction::EaseOut: return EaseOut(t);
        case EasingFunction::EaseInOut: return EaseInOut(t);
        case EasingFunction::Bounce: return EaseBounce(t);
        case EasingFunction::Elastic: return EaseElastic(t);
        case EasingFunction::Back: return EaseBack(t);
        default: return t;
    }
}

// Animator implementation
Animator::Animator() {
}

void Animator::Update(float deltaTime) {
    if (!m_IsAnimating || m_Duration <= 0.0f) {
        return;
    }

    m_Time += deltaTime;

    if (m_Time >= m_Duration) {
        m_Time = m_Duration;
        m_CurrentValue = m_TargetValue;
        m_IsAnimating = false;

        if (m_OnCompleteCallback) {
            m_OnCompleteCallback();
        }
    } else {
        float t = m_Time / m_Duration;
        float eased_t = ApplyEasing(m_Easing, t);
        m_CurrentValue = m_StartValue + (m_TargetValue - m_StartValue) * eased_t;
    }
}

void Animator::SetTarget(float target, float duration, EasingFunction easing) {
    m_StartValue = m_CurrentValue;
    m_TargetValue = target;
    m_Duration = std::max(0.0f, duration);
    m_Time = 0.0f;
    m_Easing = easing;
    m_IsAnimating = (m_Duration > 0.0f && m_StartValue != m_TargetValue);
}

void Animator::SetValue(float value) {
    m_CurrentValue = value;
    m_StartValue = value;
    m_TargetValue = value;
    m_Time = 0.0f;
    m_IsAnimating = false;
}

void Animator::Reset() {
    m_Time = 0.0f;
    m_CurrentValue = m_StartValue;
    m_IsAnimating = false;
}

// KeyframeAnimation implementation
KeyframeAnimation::KeyframeAnimation() {
}

void KeyframeAnimation::AddKeyframe(float time, float value, EasingFunction easing) {
    Keyframe keyframe;
    keyframe.Time = time;
    keyframe.Value = value;
    keyframe.Easing = easing;
    m_Keyframes.push_back(keyframe);

    // Sort by time
    std::sort(m_Keyframes.begin(), m_Keyframes.end(),
        [](const Keyframe& a, const Keyframe& b) {
            return a.Time < b.Time;
        });
}

void KeyframeAnimation::Update(float deltaTime) {
    if (m_Keyframes.empty()) {
        return;
    }

    float duration = GetDuration();
    if (duration <= 0.0f) {
        m_CurrentValue = m_Keyframes[0].Value;
        return;
    }

    m_Time += deltaTime;

    if (m_Looping && m_Time > duration) {
        m_Time = std::fmod(m_Time, duration);
    } else if (m_Time > duration) {
        m_Time = duration;
        if (m_OnCompleteCallback) {
            m_OnCompleteCallback();
        }
    }

    UpdateValue();
}

void KeyframeAnimation::SetTime(float time) {
    float duration = GetDuration();
    if (m_Looping && duration > 0.0f) {
        m_Time = std::fmod(time, duration);
    } else {
        m_Time = std::max(0.0f, std::min(time, duration));
    }
    UpdateValue();
}

float KeyframeAnimation::GetDuration() const {
    if (m_Keyframes.empty()) {
        return 0.0f;
    }
    return m_Keyframes.back().Time;
}

void KeyframeAnimation::Reset() {
    m_Time = 0.0f;
    UpdateValue();
}

bool KeyframeAnimation::IsComplete() const {
    if (m_Looping) {
        return false;
    }
    return m_Time >= GetDuration();
}

void KeyframeAnimation::UpdateValue() {
    if (m_Keyframes.empty()) {
        m_CurrentValue = 0.0f;
        return;
    }

    if (m_Keyframes.size() == 1) {
        m_CurrentValue = m_Keyframes[0].Value;
        return;
    }

    // Find the two keyframes to interpolate between
    size_t next_idx = 0;
    for (size_t i = 0; i < m_Keyframes.size(); i++) {
        if (m_Keyframes[i].Time >= m_Time) {
            next_idx = i;
            break;
        }
    }

    if (next_idx == 0) {
        m_CurrentValue = m_Keyframes[0].Value;
        return;
    }

    if (next_idx >= m_Keyframes.size()) {
        m_CurrentValue = m_Keyframes.back().Value;
        return;
    }

    // Interpolate between prev and next keyframe
    const Keyframe& prev = m_Keyframes[next_idx - 1];
    const Keyframe& next = m_Keyframes[next_idx];

    float time_range = next.Time - prev.Time;
    if (time_range <= 0.0f) {
        m_CurrentValue = next.Value;
        return;
    }

    float local_t = (m_Time - prev.Time) / time_range;
    float eased_t = ApplyEasing(next.Easing, local_t);
    m_CurrentValue = prev.Value + (next.Value - prev.Value) * eased_t;
}

// Common animation helpers
void FadeIn(Animator& animator, float duration) {
    animator.SetValue(0.0f);
    animator.SetTarget(1.0f, duration, EasingFunction::EaseInOut);
}

void FadeOut(Animator& animator, float duration) {
    animator.SetTarget(0.0f, duration, EasingFunction::EaseInOut);
}

void SlideIn(Animator& animator, float start_pos, float end_pos, float duration) {
    animator.SetValue(start_pos);
    animator.SetTarget(end_pos, duration, EasingFunction::EaseOut);
}

void SlideOut(Animator& animator, float start_pos, float end_pos, float duration) {
    animator.SetValue(start_pos);
    animator.SetTarget(end_pos, duration, EasingFunction::EaseIn);
}

void Scale(Animator& animator, float start_scale, float end_scale, float duration) {
    animator.SetValue(start_scale);
    animator.SetTarget(end_scale, duration, EasingFunction::EaseInOut);
}

} // namespace LibUI::Animation


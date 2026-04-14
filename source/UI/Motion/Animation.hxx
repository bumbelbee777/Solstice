#pragma once

#include "../../Solstice.hxx"
#include <MinGfx/EasingFunction.hxx>
#include <imgui.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <type_traits>

namespace Solstice::UI {

// Forward declaration
struct ShadowParams;

namespace Animation {

// Easing types and functions from MinGfx (canonical API: MinGfx::EasingType, MinGfx::Ease)
using EasingType = MinGfx::EasingType;

inline float Ease(float T, EasingType Type, float Strength = 1.0f) {
    return MinGfx::Ease(T, static_cast<MinGfx::EasingType>(Type), Strength);
}

inline float EaseBezier(float T, float P1X, float P1Y, float P2X, float P2Y) {
    return MinGfx::EaseBezier(T, P1X, P1Y, P2X, P2Y);
}

// Keyframe template
template<typename T>
struct Keyframe {
    float Time{0.0f};
    T Value{};
    EasingType Easing{EasingType::Linear};

    Keyframe() = default;
    Keyframe(float Time, const T& Value, EasingType Easing = EasingType::Linear)
        : Time(Time), Value(Value), Easing(Easing) {}
};

// Animation track template
template<typename T>
class AnimationTrack {
public:
    AnimationTrack() = default;
    ~AnimationTrack() = default;

    void AddKeyframe(const Keyframe<T>& Kf) {
        m_Keyframes.push_back(Kf);
        std::sort(m_Keyframes.begin(), m_Keyframes.end(),
                 [](const Keyframe<T>& a, const Keyframe<T>& b) -> bool {
                     return a.Time < b.Time;
                 });

        if (!m_Keyframes.empty()) {
            m_Duration = m_Keyframes.back().Time;
        }
    }

    T Evaluate(float Time) const {
        if (m_Keyframes.empty()) {
            return T{};
        }

        if (m_Loop && m_Duration > 0.0f) {
            Time = std::fmod(Time, m_Duration);
        }

        // Clamp time
        Time = std::max(0.0f, std::min(Time, m_Duration));

        // Find surrounding keyframes
        if (Time <= m_Keyframes[0].Time) {
            return m_Keyframes[0].Value;
        }

        if (Time >= m_Keyframes.back().Time) {
            return m_Keyframes.back().Value;
        }

        // Find the two keyframes to interpolate between
        for (size_t i = 0; i < m_Keyframes.size() - 1; ++i) {
            if (Time >= m_Keyframes[i].Time && Time <= m_Keyframes[i + 1].Time) {
                float t0 = m_Keyframes[i].Time;
                float t1 = m_Keyframes[i + 1].Time;
                float t = (Time - t0) / (t1 - t0);

                // Apply easing
                float easedT = MinGfx::Ease(t, static_cast<MinGfx::EasingType>(m_Keyframes[i].Easing));

                // Interpolate
                return Lerp(m_Keyframes[i].Value, m_Keyframes[i + 1].Value, easedT);
            }
        }

        return m_Keyframes.back().Value;
    }

    float GetDuration() const { return m_Duration; }
    void SetLoop(bool Loop) { m_Loop = Loop; }
    bool GetLoop() const { return m_Loop; }

    size_t GetKeyframeCount() const { return m_Keyframes.size(); }
    const Keyframe<T>& GetKeyframe(size_t Index) const { return m_Keyframes[Index]; }

private:
    // Helper function for interpolation (specialized for different types)
    // Use SFINAE to exclude types that have custom overloads
    template<typename U>
    auto Lerp(const U& A, const U& B, float Alpha) const -> typename std::enable_if<
        !std::is_same<U, ImVec2>::value &&
        !std::is_same<U, ImVec4>::value &&
        !std::is_same<U, ShadowParams>::value, U>::type {
        return A + (B - A) * Alpha;
    }

    // Specialization for ImVec2
    ImVec2 Lerp(const ImVec2& A, const ImVec2& B, float Alpha) const {
        return ImVec2(A.x + (B.x - A.x) * Alpha, A.y + (B.y - A.y) * Alpha);
    }

    // Specialization for ImVec4
    ImVec4 Lerp(const ImVec4& A, const ImVec4& B, float Alpha) const {
        return ImVec4(A.x + (B.x - A.x) * Alpha, A.y + (B.y - A.y) * Alpha, A.z + (B.z - A.z) * Alpha, A.w + (B.w - A.w) * Alpha);
    }

    // Specialization for ShadowParams (forward declared, implementation in .cxx)
    ShadowParams Lerp(const ShadowParams& A, const ShadowParams& B, float Alpha) const;

    std::vector<Keyframe<T>> m_Keyframes;
    float m_Duration{0.0f};
    bool m_Loop{false};
};

// Animation clip for multi-property animations
class SOLSTICE_API AnimationClip {
public:
    AnimationClip() = default;
    ~AnimationClip() = default;

    AnimationTrack<ImVec2>& GetPositionTrack() { return m_PositionTrack; }
    AnimationTrack<ImVec2>& GetScaleTrack() { return m_ScaleTrack; }
    AnimationTrack<float>& GetRotationTrack() { return m_RotationTrack; }
    AnimationTrack<ImVec4>& GetColorTrack() { return m_ColorTrack; }
    AnimationTrack<float>& GetAlphaTrack() { return m_AlphaTrack; }

    const AnimationTrack<ImVec2>& GetPositionTrack() const { return m_PositionTrack; }
    const AnimationTrack<ImVec2>& GetScaleTrack() const { return m_ScaleTrack; }
    const AnimationTrack<float>& GetRotationTrack() const { return m_RotationTrack; }
    const AnimationTrack<ImVec4>& GetColorTrack() const { return m_ColorTrack; }
    const AnimationTrack<float>& GetAlphaTrack() const { return m_AlphaTrack; }

    float GetDuration() const {
        float maxDuration = 0.0f;
        maxDuration = std::max(maxDuration, m_PositionTrack.GetDuration());
        maxDuration = std::max(maxDuration, m_ScaleTrack.GetDuration());
        maxDuration = std::max(maxDuration, m_RotationTrack.GetDuration());
        maxDuration = std::max(maxDuration, m_ColorTrack.GetDuration());
        maxDuration = std::max(maxDuration, m_AlphaTrack.GetDuration());
        return maxDuration;
    }

    void Evaluate(float Time, ImVec2& Position, ImVec2& Scale, float& Rotation, ImVec4& Color, float& Alpha) const {
        Position = m_PositionTrack.Evaluate(Time);
        Scale = m_ScaleTrack.Evaluate(Time);
        Rotation = m_RotationTrack.Evaluate(Time);
        Color = m_ColorTrack.Evaluate(Time);
        Alpha = m_AlphaTrack.Evaluate(Time);
    }

private:
    AnimationTrack<ImVec2> m_PositionTrack;
    AnimationTrack<ImVec2> m_ScaleTrack;
    AnimationTrack<float> m_RotationTrack;
    AnimationTrack<ImVec4> m_ColorTrack;
    AnimationTrack<float> m_AlphaTrack;
};

// Animation player
class SOLSTICE_API AnimationPlayer {
public:
    AnimationPlayer() = default;
    ~AnimationPlayer() = default;

    void SetClip(const AnimationClip& Clip) { m_Clip = Clip; }
    const AnimationClip& GetClip() const { return m_Clip; }

    void Play() { m_IsPlaying = true; }
    void Pause() { m_IsPlaying = false; }
    void Stop() { m_IsPlaying = false; m_CurrentTime = 0.0f; }
    void Seek(float Time) { m_CurrentTime = Time; }

    void Update(float DeltaTime) {
        if (!m_IsPlaying) {
            return;
        }

        m_CurrentTime += DeltaTime;

        float duration = m_Clip.GetDuration();
        if (duration > 0.0f) {
            if (m_Loop) {
                m_CurrentTime = std::fmod(m_CurrentTime, duration);
            } else {
                m_CurrentTime = std::min(m_CurrentTime, duration);
                if (m_CurrentTime >= duration) {
                    m_IsPlaying = false;
                }
            }
        }
    }

    bool IsPlaying() const { return m_IsPlaying; }
    float GetCurrentTime() const { return m_CurrentTime; }
    void SetLoop(bool Loop) { m_Loop = Loop; }
    bool GetLoop() const { return m_Loop; }

private:
    AnimationClip m_Clip;
    float m_CurrentTime{0.0f};
    bool m_IsPlaying{false};
    bool m_Loop{false};
};

/** Adds a short overshoot “pop” on scale (AE-style motion preset). */
inline void AddOvershootScalePulse(AnimationClip& clip, float duration, float overshootScale,
                                  float peakNormalizedTime = 0.35f) {
    if (duration <= 0.0f) {
        return;
    }
    const float peakT = std::clamp(peakNormalizedTime, 0.05f, 0.95f) * duration;
    clip.GetScaleTrack().AddKeyframe(Keyframe<ImVec2>(0.0f, ImVec2(1.0f, 1.0f)));
    clip.GetScaleTrack().AddKeyframe(
        Keyframe<ImVec2>(peakT, ImVec2(overshootScale, overshootScale), EasingType::EaseOut));
    clip.GetScaleTrack().AddKeyframe(Keyframe<ImVec2>(duration, ImVec2(1.0f, 1.0f), EasingType::EaseIn));
}

/** Ping-pong time in [0,1] for a repeating segment of length `period` seconds. */
inline float PingPongNormalized(float timeSeconds, float periodSeconds) {
    if (periodSeconds <= 0.0f) {
        return 0.0f;
    }
    const float p = std::fmod(timeSeconds, periodSeconds * 2.0f);
    return p < periodSeconds ? (p / periodSeconds) : ((periodSeconds * 2.0f - p) / periodSeconds);
}

} // namespace Animation

} // namespace Solstice::UI

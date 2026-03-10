#pragma once

#include "EasingFunction.hxx"
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <vector>
#include <algorithm>
#include <cmath>

namespace Solstice::MinGfx {

// Interpolation mode for keyframe segments
enum class InterpolationMode {
    LINEAR,
    STEP,
    CUBIC
};

// Time-indexed keyframe value; supports interpolation mode and optional easing (when LINEAR).
template<typename T>
struct Keyframe {
    float Time{0.0f};
    T Value{};
    InterpolationMode Mode{InterpolationMode::LINEAR};
    EasingType Easing{EasingType::Linear};

    Keyframe() = default;
    Keyframe(float Time, const T& Value, InterpolationMode ModeParam = InterpolationMode::LINEAR, EasingType EasingParam = EasingType::Linear)
        : Time(Time), Value(Value), Mode(ModeParam), Easing(EasingParam) {}
};

// Collection of keyframes for one property
template<typename T>
class KeyframeTrack {
public:
    KeyframeTrack() = default;

    void AddKeyframe(const Keyframe<T>& KeyframeParam) {
        m_Keyframes.push_back(KeyframeParam);
        std::sort(m_Keyframes.begin(), m_Keyframes.end(),
            [](const Keyframe<T>& A, const Keyframe<T>& B) { return A.Time < B.Time; });
    }

    [[nodiscard]] T Evaluate(float Time) const {
        if (m_Keyframes.empty()) { return T{}; }
        if (Time <= m_Keyframes.front().Time) { return m_Keyframes.front().Value; }
        if (Time >= m_Keyframes.back().Time) { return m_Keyframes.back().Value; }
        for (size_t I = 0; I < m_Keyframes.size() - 1; ++I) {
            if (Time >= m_Keyframes[I].Time && Time <= m_Keyframes[I + 1].Time) {
                return Interpolate(m_Keyframes[I], m_Keyframes[I + 1], Time);
            }
        }
        return m_Keyframes.back().Value;
    }

    [[nodiscard]] const std::vector<Keyframe<T>>& GetKeyframes() const { return m_Keyframes; }
    [[nodiscard]] float GetDuration() const {
        if (m_Keyframes.empty()) { return 0.0f; }
        return m_Keyframes.back().Time;
    }

private:
    [[nodiscard]] T Interpolate(const Keyframe<T>& A, const Keyframe<T>& B, float Time) const {
        if (A.Mode == InterpolationMode::STEP) { return A.Value; }
        float TParam = (Time - A.Time) / (B.Time - A.Time);
        TParam = std::max(0.0f, std::min(1.0f, TParam));
        if (A.Mode == InterpolationMode::LINEAR) {
            TParam = Ease(TParam, A.Easing);
        } else if (A.Mode == InterpolationMode::CUBIC) {
            float T2 = TParam * TParam;
            float T3 = T2 * TParam;
            // Simplified cubic (no tangents)
            TParam = (2.0f * T3) - (3.0f * T2) + TParam;
        }
        return Lerp(A.Value, B.Value, TParam);
    }

    [[nodiscard]] T Lerp(const T& A, const T& B, float TParamValue) const {
        return A + ((B - A) * TParamValue);
    }

    std::vector<Keyframe<T>> m_Keyframes;
};

template<>
inline Math::Vec3 KeyframeTrack<Math::Vec3>::Lerp(const Math::Vec3& A, const Math::Vec3& B, float T) const {
    return Math::Vec3::Lerp(A, B, T);
}

template<>
inline Math::Quaternion KeyframeTrack<Math::Quaternion>::Lerp(const Math::Quaternion& A, const Math::Quaternion& B, float T) const {
    return Math::Quaternion::Slerp(A, B, T);
}

} // namespace Solstice::MinGfx

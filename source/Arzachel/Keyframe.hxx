#pragma once

#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <vector>
#include <algorithm>
#include <cmath>

namespace Solstice::Arzachel {

// Interpolation mode
enum class InterpolationMode {
    Linear,
    Step,
    Cubic
};

// Time-indexed keyframe value
template<typename T>
struct Keyframe {
    float Time;
    T Value;
    InterpolationMode Mode;

    Keyframe() : Time(0.0f), Mode(InterpolationMode::Linear) {}
    Keyframe(float Time, const T& Value, InterpolationMode ModeParam = InterpolationMode::Linear)
        : Time(Time), Value(Value), Mode(ModeParam) {}
};

// Collection of keyframes for one property
template<typename T>
class KeyframeTrack {
public:
    KeyframeTrack() = default;

    void AddKeyframe(const Keyframe<T>& KeyframeParam) {
        Keyframes.push_back(KeyframeParam);
        // Keep sorted by time
        std::sort(Keyframes.begin(), Keyframes.end(),
            [](const Keyframe<T>& A, const Keyframe<T>& B) {
                return A.Time < B.Time;
            });
    }

    T Evaluate(float Time) const {
        if (Keyframes.empty()) {
            return T{};
        }

        // Clamp to range
        if (Time <= Keyframes.front().Time) {
            return Keyframes.front().Value;
        }
        if (Time >= Keyframes.back().Time) {
            return Keyframes.back().Value;
        }

        // Find surrounding keyframes
        for (size_t I = 0; I < Keyframes.size() - 1; ++I) {
            if (Time >= Keyframes[I].Time && Time <= Keyframes[I + 1].Time) {
                return Interpolate(Keyframes[I], Keyframes[I + 1], Time);
            }
        }

        return Keyframes.back().Value;
    }

    const std::vector<Keyframe<T>>& GetKeyframes() const { return Keyframes; }
    float GetDuration() const {
        if (Keyframes.empty()) return 0.0f;
        return Keyframes.back().Time;
    }

private:
    T Interpolate(const Keyframe<T>& A, const Keyframe<T>& B, float Time) const {
        if (A.Mode == InterpolationMode::Step) {
            return A.Value;
        }

        float TParam = (Time - A.Time) / (B.Time - A.Time);
        TParam = std::max(0.0f, std::min(1.0f, TParam)); // Clamp

        if (A.Mode == InterpolationMode::Cubic) {
            // Hermite interpolation (simplified)
            float T2 = TParam * TParam;
            float T3 = T2 * TParam;
            float H1 = 2.0f * T3 - 3.0f * T2 + 1.0f;
            float H2 = -2.0f * T3 + 3.0f * T2;
            // Simplified - would need tangents for proper cubic
            return Lerp(A.Value, B.Value, TParam);
        }

        // Linear
        return Lerp(A.Value, B.Value, TParam);
    }

    T Lerp(const T& A, const T& B, float TParamValue) const {
        // This will be specialized for Vec3, Quaternion, etc.
        return A + (B - A) * TParamValue;
    }

    std::vector<Keyframe<T>> Keyframes;
};

// Specializations for common types
template<>
inline Math::Vec3 KeyframeTrack<Math::Vec3>::Lerp(const Math::Vec3& A, const Math::Vec3& B, float T) const {
    return Math::Vec3::Lerp(A, B, T);
}

template<>
inline Math::Quaternion KeyframeTrack<Math::Quaternion>::Lerp(const Math::Quaternion& A, const Math::Quaternion& B, float T) const {
    return Math::Quaternion::Slerp(A, B, T);
}

} // namespace Solstice::Arzachel

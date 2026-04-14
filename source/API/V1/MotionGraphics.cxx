#include "SolsticeAPI/V1/MotionGraphics.h"
#include "MinGfx/EasingFunction.hxx"
#include <UI/Motion/Animation.hxx>
#include <algorithm>

namespace {

Solstice::MinGfx::EasingType ToMinGfxEasing(SolsticeV1_MotionEasingType type) {
    switch (type) {
        case SolsticeV1_MotionEaseLinear: return Solstice::MinGfx::EasingType::Linear;
        case SolsticeV1_MotionEaseIn: return Solstice::MinGfx::EasingType::EaseIn;
        case SolsticeV1_MotionEaseOut: return Solstice::MinGfx::EasingType::EaseOut;
        case SolsticeV1_MotionEaseInOut: return Solstice::MinGfx::EasingType::EaseInOut;
        case SolsticeV1_MotionEaseBounce: return Solstice::MinGfx::EasingType::Bounce;
        case SolsticeV1_MotionEaseElastic: return Solstice::MinGfx::EasingType::Elastic;
        case SolsticeV1_MotionEaseBack: return Solstice::MinGfx::EasingType::Back;
        case SolsticeV1_MotionEaseCirc: return Solstice::MinGfx::EasingType::Circ;
        case SolsticeV1_MotionEaseExpo: return Solstice::MinGfx::EasingType::Expo;
        case SolsticeV1_MotionEaseQuad: return Solstice::MinGfx::EasingType::Quad;
        case SolsticeV1_MotionEaseCubic: return Solstice::MinGfx::EasingType::Cubic;
        case SolsticeV1_MotionEaseQuart: return Solstice::MinGfx::EasingType::Quart;
        case SolsticeV1_MotionEaseQuint: return Solstice::MinGfx::EasingType::Quint;
        case SolsticeV1_MotionEaseBezierCurve: return Solstice::MinGfx::EasingType::Bezier;
        default: return Solstice::MinGfx::EasingType::Linear;
    }
}

} // namespace

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionEase(
    float T,
    SolsticeV1_MotionEasingType Type,
    float Strength,
    float* OutValue) {
    if (!OutValue) {
        return SolsticeV1_ResultFailure;
    }
    const float tc = std::clamp(T, 0.0f, 1.0f);
    *OutValue = Solstice::MinGfx::Ease(tc, ToMinGfxEasing(Type), Strength);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionEaseBezier(
    float T,
    float P1X,
    float P1Y,
    float P2X,
    float P2Y,
    float* OutValue) {
    if (!OutValue) {
        return SolsticeV1_ResultFailure;
    }
    const float tc = std::clamp(T, 0.0f, 1.0f);
    *OutValue = Solstice::MinGfx::EaseBezier(tc, P1X, P1Y, P2X, P2Y);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionPingPongNormalized(
    float TimeSeconds,
    float PeriodSeconds,
    float* OutValue) {
    if (!OutValue) {
        return SolsticeV1_ResultFailure;
    }
    *OutValue = Solstice::UI::Animation::PingPongNormalized(TimeSeconds, PeriodSeconds);
    return SolsticeV1_ResultSuccess;
}

namespace {

float SmoothDampScalar(float current, float target, float& vel, float smoothTime, float deltaTime) {
    smoothTime = std::max(0.0001f, smoothTime);
    const float omega = 2.0f / smoothTime;
    const float x = omega * deltaTime;
    const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    const float change = current - target;
    const float temp = (vel + omega * change) * deltaTime;
    vel = (vel - omega * temp) * exp;
    return target + (change + temp) * exp;
}

} // namespace

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionSmoothDampFloat(
    float Current,
    float Target,
    float* VelocityInOut,
    float SmoothTime,
    float DeltaTime,
    float* OutValue) {
    if (!VelocityInOut || !OutValue) {
        return SolsticeV1_ResultFailure;
    }
    *OutValue = SmoothDampScalar(Current, Target, *VelocityInOut, SmoothTime, DeltaTime);
    return SolsticeV1_ResultSuccess;
}

} // extern "C"

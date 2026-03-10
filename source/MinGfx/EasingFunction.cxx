#include "EasingFunction.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::MinGfx {

namespace {

float EaseOutBounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    }
    if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    }
    if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

float EaseInQuad(float t) { return t * t; }
float EaseOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
float EaseInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float EaseInCubic(float t) { return t * t * t; }
float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }
float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

float EaseInQuart(float t) { return t * t * t * t; }
float EaseOutQuart(float t) { return 1.0f - std::pow(1.0f - t, 4.0f); }
float EaseInOutQuart(float t) {
    return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;
}

float EaseInQuint(float t) { return t * t * t * t * t; }
float EaseOutQuint(float t) { return 1.0f - std::pow(1.0f - t, 5.0f); }
float EaseInOutQuint(float t) {
    return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) / 2.0f;
}

float EaseInExpo(float t) { return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f)); }
float EaseOutExpo(float t) { return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t); }
float EaseInOutExpo(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return t < 0.5f
        ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f
        : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
}

float EaseInCirc(float t) { return 1.0f - std::sqrt(1.0f - t * t); }
float EaseOutCirc(float t) { return std::sqrt(1.0f - (t - 1.0f) * (t - 1.0f)); }
float EaseInOutCirc(float t) {
    return t < 0.5f
        ? (1.0f - std::sqrt(1.0f - 4.0f * t * t)) / 2.0f
        : (std::sqrt(1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) + 1.0f) / 2.0f;
}

float EaseInBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}
float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}
float EaseInOutBack(float t) {
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;
    return t < 0.5f
        ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
        : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
}

float EaseInElastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return -std::pow(2.0f, 10.0f * (t - 1.0f)) * std::sin((t - 1.1f) * 5.0f * 3.14159f);
}
float EaseOutElastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - 0.1f) * 5.0f * 3.14159f) + 1.0f;
}
float EaseInOutElastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return t < 0.5f
        ? -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * 3.14159f / 4.5f)) / 2.0f
        : (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * 3.14159f / 4.5f)) / 2.0f + 1.0f;
}

float EaseInOutBounce(float t) {
    return t < 0.5f
        ? (1.0f - EaseOutBounce(1.0f - 2.0f * t)) / 2.0f
        : (1.0f + EaseOutBounce(2.0f * t - 1.0f)) / 2.0f;
}

} // namespace

float Ease(float T, EasingType Type, float Strength) {
    T = std::max(0.0f, std::min(1.0f, T));
    float result = 0.0f;
    switch (Type) {
        case EasingType::Linear: result = T; break;
        case EasingType::EaseIn: result = EaseInQuad(T); break;
        case EasingType::EaseOut: result = EaseOutQuad(T); break;
        case EasingType::EaseInOut: result = EaseInOutQuad(T); break;
        case EasingType::Quad: result = EaseInOutQuad(T); break;
        case EasingType::Cubic: result = EaseInOutCubic(T); break;
        case EasingType::Quart: result = EaseInOutQuart(T); break;
        case EasingType::Quint: result = EaseInOutQuint(T); break;
        case EasingType::Expo: result = EaseInOutExpo(T); break;
        case EasingType::Circ: result = EaseInOutCirc(T); break;
        case EasingType::Back: result = EaseInOutBack(T); break;
        case EasingType::Elastic: result = EaseInOutElastic(T); break;
        case EasingType::Bounce: result = EaseInOutBounce(T); break;
        case EasingType::Bezier: result = EaseBezier(T, 0.42f, 0.0f, 0.58f, 1.0f); break;
    }
    if (Strength != 1.0f && (Type == EasingType::Elastic || Type == EasingType::Bounce)) {
        result = T + (result - T) * Strength;
    }
    return result;
}

float EaseBezier(float T, float P1X, float P1Y, float P2X, float P2Y) {
    T = std::max(0.0f, std::min(1.0f, T));
    float low = 0.0f;
    float high = 1.0f;
    float mid = 0.5f;
    for (int i = 0; i < 10; ++i) {
        mid = (low + high) * 0.5f;
        float u = 1.0f - mid;
        float uu = u * u;
        float uuu = uu * u;
        float x = 3.0f * uu * mid * P1X + 3.0f * u * mid * mid * P2X + mid * mid * mid * 1.0f;
        if (x < T) low = mid;
        else high = mid;
    }
    float u = 1.0f - mid;
    float uu = u * u;
    float uuu = uu * u;
    return uuu * 0.0f + 3.0f * uu * mid * P1Y + 3.0f * u * mid * mid * P2Y + mid * mid * mid * 1.0f;
}

} // namespace Solstice::MinGfx

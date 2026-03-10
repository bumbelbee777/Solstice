#pragma once

namespace Solstice::MinGfx {

enum class EasingType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Bounce,
    Elastic,
    Back,
    Circ,
    Expo,
    Quad,
    Cubic,
    Quart,
    Quint,
    Bezier
};

// Apply easing to normalized time T in [0, 1]
float Ease(float T, EasingType Type, float Strength = 1.0f);

// Cubic bezier easing: P0=(0,0), P3=(1,1), P1=(P1X,P1Y), P2=(P2X,P2Y)
float EaseBezier(float T, float P1X, float P1Y, float P2X, float P2Y);

} // namespace Solstice::MinGfx

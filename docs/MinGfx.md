# MinGfx Module

## Overview

MinGfx (`Solstice::MinGfx`) provides shared graphics utilities used by Arzachel (keyframe tracks for animation) and UI (easing for animations and transitions). It is the single home for easing types/functions and for a unified keyframe/track type that supports both interpolation modes (linear, step, cubic) and optional easing.

## Easing

- **EasingType**: Linear, EaseIn, EaseOut, EaseInOut, Bounce, Elastic, Back, Circ, Expo, Quad, Cubic, Quart, Quint, Bezier.
- **Ease(T, Type, Strength)**: Apply easing to normalized time T in [0, 1].
- **EaseBezier(T, P1X, P1Y, P2X, P2Y)**: Cubic bezier easing (control points P1, P2; P0=(0,0), P3=(1,1)).

Headers: `MinGfx/EasingFunction.hxx`, implementation in `MinGfx/EasingFunction.cxx`.

## Keyframes

- **InterpolationMode**: LINEAR, STEP, CUBIC.
- **Keyframe<T>**: Time, Value, Mode (InterpolationMode), Easing (EasingType; used when Mode is LINEAR).
- **KeyframeTrack<T>**: `AddKeyframe`, `Evaluate(Time)`, `GetKeyframes`, `GetDuration`. Supports `Vec3` and `Quaternion` (Lerp/Slerp specializations).

Header: `MinGfx/Keyframe.hxx` (header-only for Keyframe/KeyframeTrack).

## Consumers

- **Arzachel**: Uses `MinGfx::Keyframe`, `MinGfx::KeyframeTrack`, `MinGfx::InterpolationMode` for bone animation tracks (Translation, Rotation, Scale). See [Arzachel.md](Arzachel.md).
- **UI**: Uses `MinGfx::EasingType`, `MinGfx::Ease`, `MinGfx::EaseBezier`; `UI::Animation` exposes type aliases and wrappers. UI keeps its own Keyframe/AnimationTrack for ImGui types (ImVec2, ImVec4, ShadowParams). See [MotionGraphics.md](MotionGraphics.md).

#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SolsticeV1_MotionEasingType {
    SolsticeV1_MotionEaseLinear = 0,
    SolsticeV1_MotionEaseIn = 1,
    SolsticeV1_MotionEaseOut = 2,
    SolsticeV1_MotionEaseInOut = 3,
    SolsticeV1_MotionEaseBounce = 4,
    SolsticeV1_MotionEaseElastic = 5,
    SolsticeV1_MotionEaseBack = 6,
    SolsticeV1_MotionEaseCirc = 7,
    SolsticeV1_MotionEaseExpo = 8,
    SolsticeV1_MotionEaseQuad = 9,
    SolsticeV1_MotionEaseCubic = 10,
    SolsticeV1_MotionEaseQuart = 11,
    SolsticeV1_MotionEaseQuint = 12,
    SolsticeV1_MotionEaseBezierCurve = 13,
} SolsticeV1_MotionEasingType;

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionEase(
    float T,
    SolsticeV1_MotionEasingType Type,
    float Strength,
    float* OutValue);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionEaseBezier(
    float T,
    float P1X,
    float P1Y,
    float P2X,
    float P2Y,
    float* OutValue);

/** Ping-pong interpolation in [0,1] over `PeriodSeconds` (then repeats). */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionPingPongNormalized(
    float TimeSeconds,
    float PeriodSeconds,
    float* OutValue);

/**
 * Critically damped smooth toward target (scalar). On success, `VelocityInOut` is updated.
 * `SmoothTime` is approximate time to reach target (seconds), similar to common game-engine smoothDamp.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_MotionSmoothDampFloat(
    float Current,
    float Target,
    float* VelocityInOut,
    float SmoothTime,
    float DeltaTime,
    float* OutValue);

#ifdef __cplusplus
}
#endif

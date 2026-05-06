#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioPlayMusic(const char* Path, int Loops);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioStopMusic(void);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioPlaySound(const char* Path, int Loops);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetMasterVolume(float Volume);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioUpdate(float Dt);
SOLSTICE_V1_API void SolsticeV1_AudioSetHRTFEnabled(int Enabled);
SOLSTICE_V1_API int SolsticeV1_AudioIsHRTFEnabled(void);
SOLSTICE_V1_API void SolsticeV1_AudioLoadHRTFDatabase(const char* Path);
SOLSTICE_V1_API void SolsticeV1_AudioSetHeadRadius(float Radius);
SOLSTICE_V1_API void SolsticeV1_AudioSetWaveTracingEnabled(int Enabled);
SOLSTICE_V1_API int SolsticeV1_AudioIsWaveTracingEnabled(void);
SOLSTICE_V1_API void SolsticeV1_AudioSetMaxRays(int PerSource);
SOLSTICE_V1_API void SolsticeV1_AudioSetMaxBounces(int Bounces);
SOLSTICE_V1_API void SolsticeV1_AudioSetAmbisonicOrder(int Order);
SOLSTICE_V1_API int SolsticeV1_AudioGetAmbisonicOrder(void);
SOLSTICE_V1_API void SolsticeV1_AudioSetFluidCouplingEnabled(int Enabled);
SOLSTICE_V1_API int SolsticeV1_AudioIsFluidCouplingEnabled(void);
SOLSTICE_V1_API void SolsticeV1_AudioSetPortalTransform(const float* Matrix, int Enabled);
SOLSTICE_V1_API void SolsticeV1_AudioClearPortalTransform(void);
SOLSTICE_V1_API void SolsticeV1_AudioSetMLSpatializationEnabled(int Enabled);
SOLSTICE_V1_API int SolsticeV1_AudioIsMLSpatializationEnabled(void);
SOLSTICE_V1_API void SolsticeV1_AudioTrainMLModel(void);
SOLSTICE_V1_API void SolsticeV1_AudioSaveMLWeights(const char* Path);
SOLSTICE_V1_API void SolsticeV1_AudioLoadMLWeights(const char* Path);

typedef uint64_t SolsticeV1_AudioEmitterHandle;

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioCreateEmitter(
    const char* Path,
    float X, float Y, float Z,
    float MaxDistance,
    SolsticeV1_Bool Loop,
    SolsticeV1_AudioEmitterHandle* OutHandle
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioUpdateEmitterTransform(
    SolsticeV1_AudioEmitterHandle Handle,
    float X, float Y, float Z
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterVolume(
    SolsticeV1_AudioEmitterHandle Handle,
    float Volume
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterOcclusion(
    SolsticeV1_AudioEmitterHandle Handle,
    float Occlusion
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterRolloff(
    SolsticeV1_AudioEmitterHandle Handle,
    float MinDistance,
    float MaxDistance,
    float RolloffFactor,
    int DistanceModel
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterDirection(
    SolsticeV1_AudioEmitterHandle Handle,
    float Dx, float Dy, float Dz
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterCone(
    SolsticeV1_AudioEmitterHandle Handle,
    float InnerAngleDeg,
    float OuterAngleDeg,
    float OuterGain
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterDoppler(
    SolsticeV1_AudioEmitterHandle Handle,
    float DopplerFactor,
    float SpeedOfSound
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterVelocity(
    SolsticeV1_AudioEmitterHandle Handle,
    float Vx, float Vy, float Vz,
    SolsticeV1_Bool Manual
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioClearEmitterVelocity(
    SolsticeV1_AudioEmitterHandle Handle
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterFocus(
    SolsticeV1_AudioEmitterHandle Handle,
    float FocusFactor,
    float NearFieldGain,
    float MaxGain
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterImmersion(
    SolsticeV1_AudioEmitterHandle Handle,
    float ImmersionFactor,
    float DistanceReverbFactor,
    float RearReverbFactor
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterDiffraction(
    SolsticeV1_AudioEmitterHandle Handle,
    float DiffractionFactor,
    float OcclusionPitchDamping
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterMotionAdaptation(
    SolsticeV1_AudioEmitterHandle Handle,
    float AdaptiveMotionFactor
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterAirAbsorption(
    SolsticeV1_AudioEmitterHandle Handle,
    float AirAbsorptionFactor
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterPitchVariance(
    SolsticeV1_AudioEmitterHandle Handle,
    float PitchVariance
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterFlags(
    SolsticeV1_AudioEmitterHandle Handle,
    SolsticeV1_Bool IsDialogue,
    SolsticeV1_Bool IsCriticalCue,
    int Priority
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioApplySpatialProfile(
    SolsticeV1_AudioEmitterHandle Handle,
    int Profile
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioDestroyEmitter(
    SolsticeV1_AudioEmitterHandle Handle
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetListener(
    float Px, float Py, float Pz,
    float Fx, float Fy, float Fz,
    float Ux, float Uy, float Uz
);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetReverbPreset(int Preset);

#ifdef __cplusplus
}
#endif

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

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

#ifdef __cplusplus
}
#endif

#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerSetEnabled(SolsticeV1_Bool Enabled);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerBeginFrame(void);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerEndFrame(void);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerBeginScope(const char* Name);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerEndScope(const char* Name);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerSetCounter(const char* Name, int64_t Value);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerGetCounter(const char* Name, int64_t* OutValue);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerGetLastFrame(float* OutFrameMs, float* OutFps);

#ifdef __cplusplus
}
#endif

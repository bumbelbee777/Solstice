#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_CoreInitialize(void);
SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_CoreReinitialize(void);
SOLSTICE_V1_API void SolsticeV1_CoreShutdown(void);

/** Returns a static string; valid for the lifetime of the DLL. */
SOLSTICE_V1_API void SolsticeV1_CoreGetVersionString(const char** OutVersion);
/** Returns the build git commit string; valid for the lifetime of the DLL. */
SOLSTICE_V1_API void SolsticeV1_CoreGetBuildCommit(const char** OutCommit);

#ifdef __cplusplus
}
#endif

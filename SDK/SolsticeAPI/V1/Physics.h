#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to an engine-managed ECS-backed physics world created for C API usage. */
typedef struct SolsticeV1_PhysicsOpaque SolsticeV1_PhysicsOpaque;
typedef SolsticeV1_PhysicsOpaque* SolsticeV1_PhysicsWorldHandle;

/**
 * Starts physics with an internal ECS registry managed by the engine.
 * Requires SolsticeV1_CoreInitialize() to have completed successfully.
 * Returns SolsticeV1_ResultSuccess on success and stores a stable opaque handle in OutHandle when non-null.
 * Subsequent calls are idempotent and return success.
 */
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PhysicsStart(SolsticeV1_PhysicsWorldHandle* OutHandle);

/** Stops physics and destroys the internal C API world handle (if started). */
SOLSTICE_V1_API void SolsticeV1_PhysicsStop(void);

/** Advances physics by Dt when a world has been started; no-op otherwise. */
SOLSTICE_V1_API void SolsticeV1_PhysicsUpdate(float Dt);

#ifdef __cplusplus
}
#endif

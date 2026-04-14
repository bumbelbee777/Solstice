#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SolsticeV1_SpritePhysicsOpaque SolsticeV1_SpritePhysicsOpaque;
typedef SolsticeV1_SpritePhysicsOpaque* SolsticeV1_SpritePhysicsWorldHandle;

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsCreate(SolsticeV1_SpritePhysicsWorldHandle* OutWorld);
SOLSTICE_V1_API void SolsticeV1_SpritePhysicsDestroy(SolsticeV1_SpritePhysicsWorldHandle World);

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsSetGravity(SolsticeV1_SpritePhysicsWorldHandle World, float Gx, float Gy);
SOLSTICE_V1_API void SolsticeV1_SpritePhysicsSetRestitution(SolsticeV1_SpritePhysicsWorldHandle World, float E);

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsSetBounds(
    SolsticeV1_SpritePhysicsWorldHandle World, float MinX, float MinY, float MaxX, float MaxY);
SOLSTICE_V1_API void SolsticeV1_SpritePhysicsClearBounds(SolsticeV1_SpritePhysicsWorldHandle World);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsAddBody(
    SolsticeV1_SpritePhysicsWorldHandle World,
    float CenterX,
    float CenterY,
    float HalfExtentX,
    float HalfExtentY,
    float Mass,
    SolsticeV1_Bool Dynamic,
    uint32_t* OutBodyId);

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsRemoveBody(SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId);
SOLSTICE_V1_API void SolsticeV1_SpritePhysicsStep(SolsticeV1_SpritePhysicsWorldHandle World, float DeltaTime);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsGetCenter(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float* OutX, float* OutY);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsSetCenter(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float X, float Y);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsGetVelocity(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float* OutVx, float* OutVy);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsSetVelocity(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float Vx, float Vy);

#ifdef __cplusplus
}
#endif

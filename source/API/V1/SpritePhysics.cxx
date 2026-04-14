#include "SolsticeAPI/V1/UI.h"
#include <UI/Motion/SpritePhysics2D.hxx>
#include <imgui.h>

struct SolsticeV1_SpritePhysicsOpaque {
    Solstice::UI::SpritePhysicsWorld World;
};

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsCreate(SolsticeV1_SpritePhysicsWorldHandle* OutWorld) {
    if (!OutWorld) {
        return SolsticeV1_ResultFailure;
    }
    *OutWorld = new SolsticeV1_SpritePhysicsOpaque();
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsDestroy(SolsticeV1_SpritePhysicsWorldHandle World) {
    delete World;
}

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsSetGravity(SolsticeV1_SpritePhysicsWorldHandle World, float Gx, float Gy) {
    if (!World) {
        return;
    }
    World->World.SetGravity(ImVec2(Gx, Gy));
}

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsSetRestitution(SolsticeV1_SpritePhysicsWorldHandle World, float E) {
    if (!World) {
        return;
    }
    World->World.SetRestitution(E);
}

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsSetBounds(
    SolsticeV1_SpritePhysicsWorldHandle World, float MinX, float MinY, float MaxX, float MaxY) {
    if (!World) {
        return;
    }
    World->World.SetBounds(ImVec2(MinX, MinY), ImVec2(MaxX, MaxY));
}

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsClearBounds(SolsticeV1_SpritePhysicsWorldHandle World) {
    if (!World) {
        return;
    }
    World->World.ClearBounds();
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsAddBody(
    SolsticeV1_SpritePhysicsWorldHandle World,
    float CenterX,
    float CenterY,
    float HalfExtentX,
    float HalfExtentY,
    float Mass,
    SolsticeV1_Bool Dynamic,
    uint32_t* OutBodyId) {
    if (!World || !OutBodyId) {
        return SolsticeV1_ResultFailure;
    }
    *OutBodyId = World->World.AddAabbBody(
        ImVec2(CenterX, CenterY), ImVec2(HalfExtentX, HalfExtentY), Mass, Dynamic != SolsticeV1_False);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsRemoveBody(SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId) {
    if (!World) {
        return;
    }
    World->World.RemoveBody(BodyId);
}

SOLSTICE_V1_API void SolsticeV1_SpritePhysicsStep(SolsticeV1_SpritePhysicsWorldHandle World, float DeltaTime) {
    if (!World) {
        return;
    }
    World->World.Step(DeltaTime);
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsGetCenter(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float* OutX, float* OutY) {
    if (!World || !OutX || !OutY) {
        return SolsticeV1_ResultFailure;
    }
    if (!World->World.IsValid(BodyId)) {
        return SolsticeV1_ResultFailure;
    }
    const ImVec2 c = World->World.GetBodyCenter(BodyId);
    *OutX = c.x;
    *OutY = c.y;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsSetCenter(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float X, float Y) {
    if (!World) {
        return SolsticeV1_ResultFailure;
    }
    if (!World->World.IsValid(BodyId)) {
        return SolsticeV1_ResultFailure;
    }
    World->World.SetBodyCenter(BodyId, ImVec2(X, Y));
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsGetVelocity(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float* OutVx, float* OutVy) {
    if (!World || !OutVx || !OutVy) {
        return SolsticeV1_ResultFailure;
    }
    if (!World->World.IsValid(BodyId)) {
        return SolsticeV1_ResultFailure;
    }
    const ImVec2 v = World->World.GetBodyVelocity(BodyId);
    *OutVx = v.x;
    *OutVy = v.y;
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SpritePhysicsSetVelocity(
    SolsticeV1_SpritePhysicsWorldHandle World, uint32_t BodyId, float Vx, float Vy) {
    if (!World) {
        return SolsticeV1_ResultFailure;
    }
    if (!World->World.IsValid(BodyId)) {
        return SolsticeV1_ResultFailure;
    }
    World->World.SetBodyVelocity(BodyId, ImVec2(Vx, Vy));
    return SolsticeV1_ResultSuccess;
}

} // extern "C"

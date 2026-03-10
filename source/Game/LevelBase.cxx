#include "LevelBase.hxx"
#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include "../Math/Quaternion.hxx"
#include "../Math/Vector.hxx"

namespace Solstice::Game {

LevelBase::LevelBase() {
}

void LevelBase::AddObjectToScene(
    Solstice::Render::Scene* Scene,
    uint32_t MeshID,
    const Solstice::Math::Vec3& Position,
    const Solstice::Math::Quaternion& Rotation,
    const Solstice::Math::Vec3& Scale
) {
    if (Scene) {
        Scene->AddObject(
            MeshID,
            Position,
            Rotation,
            Scale,
            Solstice::Render::ObjectType_Static
        );
    }
}

} // namespace Solstice::Game


#pragma once

#include "../Solstice.hxx"
#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include "../Core/Material.hxx"
#include "../Physics/PhysicsSystem.hxx"
#include "../Entity/Registry.hxx"
#include "../Arzachel/Seed.hxx"
#include <string>
#include <memory>

// Forward declarations
namespace Solstice::Render {
    class MeshLibrary;
}
namespace Solstice::Core {
    class MaterialLibrary;
}

namespace Solstice::Game {

// Base level interface for procedural level generation
class SOLSTICE_API LevelBase {
public:
    LevelBase();
    virtual ~LevelBase() = default;

    // Level initialization
    virtual void Initialize(
        Solstice::Render::Scene* Scene,
        Solstice::Render::MeshLibrary* MeshLibrary,
        Solstice::Core::MaterialLibrary* MaterialLibrary,
        Solstice::ECS::Registry* Registry,
        const Solstice::Arzachel::Seed& Seed
    ) = 0;

    // Level name
    virtual std::string GetName() const = 0;
    virtual std::string GetDescription() const = 0;

    // Level state
    virtual void Update(float DeltaTime) {}
    virtual void Shutdown() {}

protected:
    // Helper methods for common level operations
    void AddObjectToScene(
        Solstice::Render::Scene* Scene,
        uint32_t MeshID,
        const Solstice::Math::Vec3& Position,
        const Solstice::Math::Quaternion& Rotation = Solstice::Math::Quaternion(),
        const Solstice::Math::Vec3& Scale = Solstice::Math::Vec3(1, 1, 1)
    );

    // Scene and library references (set during Initialize)
    Solstice::Render::Scene* m_Scene{nullptr};
    Solstice::Render::MeshLibrary* m_MeshLibrary{nullptr};
    Solstice::Core::MaterialLibrary* m_MaterialLibrary{nullptr};
    Solstice::ECS::Registry* m_Registry{nullptr};
    Solstice::Arzachel::Seed m_Seed;
};

} // namespace Solstice::Game


#pragma once

#include <Game/App/GameBase.hxx>
#include <UI/Core/Window.hxx>
#include <Render/DefaultRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Scheduler.hxx>
#include <Core/DeterministicStream.hxx>
#include <memory>
#include <vector>
#include <cstdint>

namespace Solstice::Render {
    class MeshLibrary;
    class Skybox;
}
namespace Solstice::Core {
    class MaterialLibrary;
}

namespace Solstice::MazeExp {

class MazeGame : public Game::GameBase {
public:
    explicit MazeGame(std::uint64_t masterSeed = 42ULL);
    ~MazeGame();

protected:
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    void InitializeWindow();
    void ConfigureScheduler();
    void SpawnPlayer(const Math::Vec3& position);
    void BuildWorld();
    void GenerateMaze();
    void InstanciateGeometry();

    void AddStaticBox(const Math::Vec3& center, const Math::Vec3& halfExtents, uint32_t materialId);
    void PlaceSanctumDecor(const Math::Vec3& roomCenter);
    void PlaceShortcutPortals();
    void PlaceFinaleLoop(const Math::Vec3& roomCenter, float roomInnerHeight);
    void UpdatePianoKeys(float dt);

    Math::Vec3 CellCenterWorld(int gi, int gj) const;

    std::unique_ptr<Render::SoftwareRenderer> m_Renderer;
    Render::Scene m_Scene;
    std::unique_ptr<Render::MeshLibrary> m_MeshLibrary;
    std::unique_ptr<Core::MaterialLibrary> m_MaterialLibrary;
    ECS::Registry m_Registry;
    ECS::PhaseScheduler m_Scheduler;
    ECS::EntityId m_PlayerEntity{0};

    Render::Camera m_Camera{};
    bool m_MouseLocked{false};
    float m_MouseX{0};
    float m_MouseY{0};
    double m_MouseAccumX{0};
    double m_MouseAccumY{0};

    std::vector<Physics::LightSource> m_Lights;

    uint32_t m_CubeMeshId{0};
    uint32_t m_PlaneMeshId{0};
    uint32_t m_CylinderMeshId{0};
    uint32_t m_MatWall{0};
    uint32_t m_MatWallAlt{0};
    uint32_t m_MatFloor{0};
    uint32_t m_MatFloorAlt{0};
    uint32_t m_MatCeil{0};
    uint32_t m_MatCouch{0};
    uint32_t m_MatWood{0};
    uint32_t m_MatWoodDark{0};
    uint32_t m_MatLamp{0};
    uint32_t m_MatWindow{0};
    uint32_t m_MatKey{0};
    uint32_t m_MatKeyBlack{0};
    uint32_t m_MatTrim{0};
    uint32_t m_MatRune{0};
    uint32_t m_MatGold{0};
    uint32_t m_MatTileA{0};
    uint32_t m_MatTileB{0};
    uint32_t m_MatPlaster{0};

    int m_GridW{0};
    int m_GridH{0};
    std::vector<uint8_t> m_GridCarve{};
    std::pair<int, int> m_StartCell{1, 1};
    std::pair<int, int> m_FarCell{1, 1};
    Math::Vec3 m_SanctumWorldCenter{};

    Core::DeterministicStream m_RngPortal{42,
        Core::StreamTagFourCC('P', 'R', 'T', 'L')};
    Core::DeterministicStream m_RngProp{42,
        Core::StreamTagFourCC('P', 'R', 'P', 'S')};
    uint64_t m_MasterSeed{42};

    static constexpr float kCell = 4.0f;
    static constexpr float kRoomHeight = 3.8f;

    float m_GameplayTime{0};
    std::vector<Render::SceneObjectID> m_PianoKeyObjects{};
};

} // namespace Solstice::MazeExp

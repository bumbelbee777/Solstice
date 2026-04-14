#pragma once

#include "../../Solstice.hxx"
#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include "../../Material/Material.hxx"
#include "../../Physics/Integration/PhysicsSystem.hxx"
#include "../../Physics/Dynamics/RigidBody.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Arzachel/TerrainGenerator.hxx"
#include "../../Arzachel/StructureGenerator.hxx"
#include "../../Arzachel/AssetPresets.hxx"
#include "../../Arzachel/AssetBuilder.hxx"
#include "../../Math/Quaternion.hxx"
#include <memory>
#include <vector>
#include <cstdint>
#include <random>

namespace Solstice::Game {

// Terrain configuration
struct TerrainConfig {
    uint32_t Resolution{128};
    float TerrainSize{200.0f};
    float HeightScale{40.0f};
    uint32_t Seed{12345};
    uint32_t LakeMaterialID{0xFFFFFFFF}; // UINT32_MAX
    uint32_t TerrainMaterialID{0xFFFFFFFF}; // UINT32_MAX
    bool CreatePhysicsBody{true};
    std::string CachePath; // Optional: path to cache file (empty = use default "cache/terrain_heightmap.json")
    bool EnableCaching{true}; // Whether to use caching
};

// Hill configuration
struct HillConfig {
    uint32_t Count{5};
    float TerrainSize{200.0f};
    uint32_t Seed{12345};
    uint32_t MaterialID{0xFFFFFFFF}; // UINT32_MAX
    bool CreatePhysicsBody{true};
};

// Structure configuration
struct StructureConfig {
    uint32_t Count{3};
    float TerrainSize{200.0f};
    uint32_t Seed{54321};
    uint32_t MaterialID{0xFFFFFFFF}; // UINT32_MAX
    bool CreatePhysicsBody{false};
};

// Tree configuration
struct TreeConfig {
    uint32_t Count{10};
    float TerrainSize{200.0f};
    uint32_t Seed{12345};
    uint32_t MaterialID{0xFFFFFFFF}; // UINT32_MAX
    float MinHeight{3.0f};
    float MaxHeight{8.0f};
    bool SnowCovered{true};
    bool CreatePhysicsBody{false};
};

// Building configuration
struct BuildingConfig {
    enum class Type {
        SwissHouse,
        SwissChurch,
        SwissTownHall,
        SwissShop
    };
    Type BuildingType{Type::SwissHouse};
    Math::Vec3 Position{0, 0, 0};
    Math::Quaternion Rotation{};
    int Floors{2}; // For houses
    uint32_t MaterialID{0xFFFFFFFF}; // UINT32_MAX
    bool CreatePhysicsBody{false};
};

// Road segment configuration
struct RoadSegment {
    Math::Vec3 Start{0, 0, 0};
    Math::Vec3 End{10, 0, 0};
    float Width{4.0f};
};

// Road configuration
struct RoadConfig {
    std::vector<RoadSegment> Segments;
    uint32_t MaterialID{0xFFFFFFFF}; // UINT32_MAX
};

// Ground vegetation configuration
struct GroundVegetationConfig {
    enum class Type {
        GrassPatch,
        SmallBush,
        Rock,
        SnowDrift
    };
    Type VegetationType{Type::GrassPatch};
    uint32_t Count{20};
    Math::Vec3 AreaMin{-50, 0, -50};
    Math::Vec3 AreaMax{50, 0, 50};
    float MinSize{0.5f};
    float MaxSize{2.0f};
    uint32_t Seed{12345};
    uint32_t MaterialID{0xFFFFFFFF}; // UINT32_MAX
    std::vector<Math::Vec3> ExcludeAreas; // Areas to avoid (e.g., roads, buildings)
    float ExcludeRadius{5.0f}; // Radius around exclude areas
};

// Scene builder for common scene setup patterns
class SOLSTICE_API SceneBuilder {
public:
    SceneBuilder();
    ~SceneBuilder() = default;

    // Add terrain to scene
    void AddTerrain(
        Render::Scene* Scene,
        Render::MeshLibrary* MeshLibrary,
        Core::MaterialLibrary* MaterialLibrary,
        ECS::Registry* Registry,
        const TerrainConfig& Config);

    // Add hills to scene
    void AddHills(
        Render::Scene* Scene,
        Render::MeshLibrary* MeshLibrary,
        Core::MaterialLibrary* MaterialLibrary,
        ECS::Registry* Registry,
        const HillConfig& Config);

    // Add structures to scene
    void AddStructures(
        Render::Scene* Scene,
        Render::MeshLibrary* MeshLibrary,
        Core::MaterialLibrary* MaterialLibrary,
        const StructureConfig& Config);

    // Add trees to scene
    void AddTrees(
        Render::Scene* Scene,
        Render::MeshLibrary* MeshLibrary,
        Core::MaterialLibrary* MaterialLibrary,
        ECS::Registry* Registry,
        const TreeConfig& Config);

    // Add buildings to scene
    void AddBuildings(
        Render::Scene* Scene,
        Render::MeshLibrary* MeshLibrary,
        Core::MaterialLibrary* MaterialLibrary,
        ECS::Registry* Registry,
        const std::vector<BuildingConfig>& Buildings);

    // Add roads to scene
    void AddRoads(
        Render::Scene* Scene,
        Render::MeshLibrary* MeshLibrary,
        Core::MaterialLibrary* MaterialLibrary,
        ECS::Registry* Registry,
        const RoadConfig& Config);

    // Add ground vegetation to scene
    void AddGroundVegetation(
        Render::Scene* Scene,
        Render::MeshLibrary* MeshLibrary,
        Core::MaterialLibrary* MaterialLibrary,
        ECS::Registry* Registry,
        const GroundVegetationConfig& Config);

private:
    // Terrain data cache
    std::vector<float> m_TerrainHeightmap;
    uint32_t m_TerrainResolution{0};
    float m_TerrainSize{0.0f};

    // Terrain heightmap caching functions
    bool SaveTerrainHeightmap(const std::string& Path, const std::vector<float>& Heightmap, uint32_t Resolution);
    bool LoadTerrainHeightmap(const std::string& Path, std::vector<float>& OutHeightmap, uint32_t& OutResolution);
};

} // namespace Solstice::Game

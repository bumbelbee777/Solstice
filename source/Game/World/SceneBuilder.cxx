#include "World/SceneBuilder.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Core/Serialization/JSON.hxx"
#include "../../Arzachel/TerrainGenerator.hxx"
#include "../../Arzachel/StructureGenerator.hxx"
#include "../../Arzachel/AssetPresets.hxx"
#include "../../Arzachel/AssetBuilder.hxx"
#include "../../Arzachel/Seed.hxx"
#include <Render/Assets/Mesh.hxx>
#include "../../Physics/Integration/PhysicsSystem.hxx"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <cmath>
#include <algorithm>

namespace Solstice::Game {

SceneBuilder::SceneBuilder() {
}

void SceneBuilder::AddTerrain(
    Render::Scene* Scene,
    Render::MeshLibrary* MeshLibrary,
    Core::MaterialLibrary* MaterialLibrary,
    ECS::Registry* Registry,
    const TerrainConfig& Config) {

    if (!Scene || !MeshLibrary || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: SceneBuilder::AddTerrain - null pointers");
        return;
    }

    using namespace Solstice::Arzachel;

    // Determine cache path
    std::string HeightmapCachePath;
    if (!Config.CachePath.empty()) {
        HeightmapCachePath = Config.CachePath;
    } else {
        // Try to find existing cache in common locations
        std::vector<std::filesystem::path> CachePathCandidates = {
            "cache/terrain_heightmap.json",
            "out/build/x64-release/bin/cache/terrain_heightmap.json",
            "../cache/terrain_heightmap.json",
            "../../cache/terrain_heightmap.json"
        };

        bool FoundCachePath = false;
        for (const auto& Candidate : CachePathCandidates) {
            if (std::filesystem::exists(Candidate)) {
                HeightmapCachePath = Candidate.string();
                FoundCachePath = true;
                break;
            }
        }

        if (!FoundCachePath) {
            HeightmapCachePath = "cache/terrain_heightmap.json";
            std::filesystem::create_directories("cache");
        }
    }

    // Load or generate heightmap
    std::vector<float> HeightMap;
    uint32_t LoadedResolution = 0;
    bool LoadedFromCache = false;

    // Try loading from cache if caching is enabled
    if (Config.EnableCaching && std::filesystem::exists(HeightmapCachePath)) {
        LoadedFromCache = LoadTerrainHeightmap(HeightmapCachePath, HeightMap, LoadedResolution);

        // Validate cached data matches config
        if (LoadedFromCache && (LoadedResolution != Config.Resolution)) {
            SIMPLE_LOG("WARNING: Cached heightmap resolution (" + std::to_string(LoadedResolution) +
                      ") doesn't match config (" + std::to_string(Config.Resolution) + "), regenerating...");
            LoadedFromCache = false;
            HeightMap.clear();
        }
    }

    if (!LoadedFromCache) {
        HeightMap = Arzachel::GenerateTerrainHeightmap(Config.Resolution, Config.HeightScale, Config.Seed);

        // Save to cache if caching is enabled
        if (Config.EnableCaching) {
            SaveTerrainHeightmap(HeightmapCachePath, HeightMap, Config.Resolution);
        }
    }

    // Store for later use
    m_TerrainHeightmap = HeightMap;
    m_TerrainResolution = Config.Resolution;
    m_TerrainSize = Config.TerrainSize;

    // Create mesh (cache loading would be handled by TerrainGenerator in future)
    std::unique_ptr<Render::Mesh> TerrainMesh = Arzachel::CreateTerrainMesh(
        HeightMap, Config.Resolution, Config.TerrainSize);

    if (!TerrainMesh || TerrainMesh->Vertices.empty() || TerrainMesh->Indices.empty()) {
        SIMPLE_LOG("ERROR: Failed to create terrain mesh");
        return;
    }

    // Optimize terrain mesh
    Arzachel::OptimizeTerrainMesh(TerrainMesh.get(), HeightMap, Config.Resolution, Config.TerrainSize);

    // Add terrain to scene
    uint32_t TerrainMeshID = MeshLibrary->AddMesh(std::move(TerrainMesh));

    // Set materials
    Render::Mesh* MeshPtr = MeshLibrary->GetMesh(TerrainMeshID);
    if (MeshPtr && MeshPtr->SubMeshes.size() >= 2) {
        if (Config.LakeMaterialID != UINT32_MAX) {
            MeshPtr->SubMeshes[0].MaterialID = Config.LakeMaterialID;
        }
        if (Config.TerrainMaterialID != UINT32_MAX) {
            MeshPtr->SubMeshes[1].MaterialID = Config.TerrainMaterialID;
        }
    } else if (MeshPtr && !MeshPtr->SubMeshes.empty()) {
        if (Config.TerrainMaterialID != UINT32_MAX) {
            MeshPtr->SubMeshes[0].MaterialID = Config.TerrainMaterialID;
        }
    }

    // Add terrain to scene
    auto TerrainObjID = Scene->AddObject(
        TerrainMeshID,
        Math::Vec3(0, 0, 0),
        Math::Quaternion(),
        Math::Vec3(1, 1, 1),
        Render::ObjectType_Static
    );

    // Create terrain collision if requested
    if (Config.CreatePhysicsBody && Registry) {
        auto TerrainEntityID = Registry->Create();
        auto& TerrainRB = Registry->Add<Physics::RigidBody>(TerrainEntityID);

        // Position terrain collision at ground level (flat terrain)
        // For flat terrain, the box top should be at Y=0 (ground level)
        // Make it thick enough to prevent falling through
        // Thick slab: reduces edge tunneling; top face stays at Y=0
        float TerrainHeight = 5.0f;
        float TerrainCenterY = -TerrainHeight * 0.5f; // Center below ground so top is at Y=0

        TerrainRB.Position = Math::Vec3(0, TerrainCenterY, 0);
        TerrainRB.Rotation = Math::Quaternion();
        TerrainRB.IsStatic = true;
        TerrainRB.SetMass(0.0f);
        TerrainRB.Type = Physics::ColliderType::Box;
        // Make box cover terrain area - top of box at Y=0 (ground level)
        // Box extends from Y=-1.0 to Y=0.0
        TerrainRB.HalfExtents = Math::Vec3(Config.TerrainSize * 0.5f, TerrainHeight * 0.5f, Config.TerrainSize * 0.5f);
        TerrainRB.Friction = 0.6f;
        TerrainRB.Restitution = 0.0f;
        TerrainRB.RenderObjectID = TerrainObjID;

        // CRITICAL: Force sync terrain collision to ReactPhysics3D immediately
        // This ensures the terrain exists in the physics world before the player is created
        // Only sync if PhysicsSystem is initialized
        try {
            Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
            Bridge.SyncToReactPhysics3D();
        } catch (...) {
            // Physics system not initialized yet - will sync on next physics update
        }
    }
}

void SceneBuilder::AddHills(
    Render::Scene* Scene,
    Render::MeshLibrary* MeshLibrary,
    Core::MaterialLibrary* MaterialLibrary,
    ECS::Registry* Registry,
    const HillConfig& Config) {

    if (!Scene || !MeshLibrary || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: SceneBuilder::AddHills - null pointers");
        return;
    }

    using namespace Solstice::Arzachel;

    // Use stored terrain data if available
    if (m_TerrainHeightmap.empty() || m_TerrainResolution == 0 || m_TerrainSize == 0.0f) {
        SIMPLE_LOG("WARNING: Terrain not initialized, using default size");
    }

    float TerrainSize = (m_TerrainSize > 0.0f) ? m_TerrainSize : Config.TerrainSize;

    // Generate hills
    std::vector<HillData> Hills = Arzachel::GenerateHills(Config.Count, TerrainSize, Config.Seed);

    // Create and add hills
    for (const auto& Hill : Hills) {
        // Validate hill dimensions - use more reasonable limits to prevent huge hills
        if (Hill.Radius <= 0.0f || Hill.Height <= 0.0f ||
            Hill.Radius > 20.0f || Hill.Height > 15.0f) {
            SIMPLE_LOG("WARNING: Skipping invalid hill (radius: " + std::to_string(Hill.Radius) + ", height: " + std::to_string(Hill.Height) + ")");
            continue;
        }

        auto HillMesh = Arzachel::CreateHillMesh(Hill.Radius, Hill.Height);
        if (!HillMesh || HillMesh->Vertices.empty() || HillMesh->Indices.empty()) {
            SIMPLE_LOG("ERROR: Failed to create hill mesh - skipping");
            continue;
        }

        uint32_t HillMeshID = MeshLibrary->AddMesh(std::move(HillMesh));

        // Set material
        Render::Mesh* MeshPtr = MeshLibrary->GetMesh(HillMeshID);
        if (MeshPtr && !MeshPtr->SubMeshes.empty() && Config.MaterialID != 0xFFFFFFFF) { // UINT32_MAX
            MeshPtr->SubMeshes[0].MaterialID = Config.MaterialID;
        }

        Math::Vec3 HillPos = Hill.Position;

        // Place hill on terrain - ellipsoid extends from -Height to +Height, so offset upward
        // Position the bottom of the ellipsoid at terrain height
        float TerrainHeight = 0.0f;
        if (!m_TerrainHeightmap.empty() && m_TerrainResolution > 0 && m_TerrainSize > 0.0f) {
            // Calculate terrain height at hill position
            float TerrainX = (HillPos.x / m_TerrainSize + 0.5f) * m_TerrainResolution;
            float TerrainZ = (HillPos.z / m_TerrainSize + 0.5f) * m_TerrainResolution;
            int X = static_cast<int>(std::max(0.0f, std::min(static_cast<float>(m_TerrainResolution - 1), TerrainX)));
            int Z = static_cast<int>(std::max(0.0f, std::min(static_cast<float>(m_TerrainResolution - 1), TerrainZ)));
            int Index = Z * m_TerrainResolution + X;
            if (Index >= 0 && Index < static_cast<int>(m_TerrainHeightmap.size())) {
                TerrainHeight = m_TerrainHeightmap[Index];
            }
        }
        // Offset hill upward by its height so bottom of ellipsoid sits on terrain
        // Ellipsoid is centered at HillPos.y, so bottom is at HillPos.y - Height
        // We want bottom at TerrainHeight, so: HillPos.y - Height = TerrainHeight
        // Therefore: HillPos.y = TerrainHeight + Height
        HillPos.y = TerrainHeight + Hill.Height;

        auto HillObjID = Scene->AddObject(
            HillMeshID,
            HillPos,
            Math::Quaternion(),
            Math::Vec3(1, 1, 1),
            Render::ObjectType_Static
        );

        // Create hill collision if requested
        if (Config.CreatePhysicsBody && Registry) {
            auto HillEntityID = Registry->Create();
            auto& HillRB = Registry->Add<Physics::RigidBody>(HillEntityID);
            HillRB.Position = HillPos;
            HillRB.Rotation = Math::Quaternion();
            HillRB.IsStatic = true;
            HillRB.SetMass(0.0f);
            HillRB.Type = Physics::ColliderType::Sphere;
            HillRB.Radius = Hill.Radius;
            HillRB.Friction = 0.6f;
            HillRB.Restitution = 0.0f;
            HillRB.RenderObjectID = HillObjID;
        }
    }
}

void SceneBuilder::AddStructures(
    Render::Scene* Scene,
    Render::MeshLibrary* MeshLibrary,
    Core::MaterialLibrary* MaterialLibrary,
    ECS::Registry* Registry,
    const StructureConfig& Config) {

    if (!Scene || !MeshLibrary || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: SceneBuilder::AddStructures - null pointers");
        return;
    }

    using namespace Solstice::Arzachel;
    using Solstice::Math::Matrix4;
    using Solstice::Math::Vec3;
    using Solstice::Math::Vec4;

    // Use stored terrain size if available
    float TerrainSize = (m_TerrainSize > 0.0f) ? m_TerrainSize : Config.TerrainSize;

    std::vector<StructureData> Structures = Arzachel::GenerateStructures(Config.Count, TerrainSize, Config.Seed);

    for (size_t I = 0; I < Structures.size(); ++I) {
        const auto& Struct = Structures[I];
        Arzachel::Seed InstanceSeed = Arzachel::Seed(Config.Seed).Derive(static_cast<uint32_t>(I));
        Arzachel::Generator<Arzachel::MeshData> Gen = (Struct.StructureType == StructureData::Type::Shack) ?
            Arzachel::Shack(InstanceSeed) :
            Arzachel::Cabin(InstanceSeed);

        Arzachel::MeshData Data = Gen(InstanceSeed);
        Data.CalculateBounds();
        auto StructMesh = Arzachel::ConvertToRenderMesh(Data);
        uint32_t MeshID = MeshLibrary->AddMesh(std::move(StructMesh));

        // Set material
        Render::Mesh* MeshPtr = MeshLibrary->GetMesh(MeshID);
        if (MeshPtr && Config.MaterialID != 0xFFFFFFFF) { // UINT32_MAX
            for (auto& Sub : MeshPtr->SubMeshes) {
                Sub.MaterialID = Config.MaterialID;
            }
        }

        Vec3 PlacedPosition = Struct.Position;
        float TerrainY = 0.0f;
        if (!m_TerrainHeightmap.empty() && m_TerrainResolution > 0 && m_TerrainSize > 0.0f) {
            float TerrainX = (Struct.Position.x / m_TerrainSize + 0.5f) * static_cast<float>(m_TerrainResolution);
            float TerrainZ = (Struct.Position.z / m_TerrainSize + 0.5f) * static_cast<float>(m_TerrainResolution);
            int X = static_cast<int>(std::max(0.0f, std::min(static_cast<float>(m_TerrainResolution - 1), TerrainX)));
            int Z = static_cast<int>(std::max(0.0f, std::min(static_cast<float>(m_TerrainResolution - 1), TerrainZ)));
            int Index = Z * static_cast<int>(m_TerrainResolution) + X;
            if (Index >= 0 && Index < static_cast<int>(m_TerrainHeightmap.size())) {
                TerrainY = m_TerrainHeightmap[Index];
            }
        }
        PlacedPosition.y = TerrainY - (Data.BoundsMin.y * Struct.Scale.y);

        Render::SceneObjectID ObjId = Scene->AddObject(
            MeshID,
            PlacedPosition,
            Struct.Rotation,
            Struct.Scale,
            Render::ObjectType_Static
        );

        if (Config.CreatePhysicsBody && Registry) {
            Vec3 BMin(Data.BoundsMin.x * Struct.Scale.x, Data.BoundsMin.y * Struct.Scale.y, Data.BoundsMin.z * Struct.Scale.z);
            Vec3 BMax(Data.BoundsMax.x * Struct.Scale.x, Data.BoundsMax.y * Struct.Scale.y, Data.BoundsMax.z * Struct.Scale.z);
            Vec3 LocalCenter = (BMin + BMax) * 0.5f;
            Vec3 LocalHalf = (BMax - BMin) * 0.5f;
            const float kPad = 0.12f;
            LocalHalf = Vec3(
                std::max(LocalHalf.x + kPad, 0.25f),
                std::max(LocalHalf.y + kPad, 0.25f),
                std::max(LocalHalf.z + kPad, 0.25f));

            Matrix4 RotM = Struct.Rotation.ToMatrix();
            Vec4 Rc(LocalCenter.x, LocalCenter.y, LocalCenter.z, 0.0f);
            Vec4 Rw = RotM * Rc;
            Vec3 WorldCenter = PlacedPosition + Vec3(Rw.x, Rw.y, Rw.z);

            auto SEntity = Registry->Create();
            auto& SRB = Registry->Add<Physics::RigidBody>(SEntity);
            SRB.Position = WorldCenter;
            SRB.Rotation = Struct.Rotation;
            SRB.IsStatic = true;
            SRB.SetMass(0.0f);
            SRB.Type = Physics::ColliderType::Box;
            SRB.HalfExtents = LocalHalf;
            SRB.Friction = 0.6f;
            SRB.Restitution = 0.0f;
            SRB.RenderObjectID = ObjId;
        }
    }

    if (Config.CreatePhysicsBody && Registry) {
        try {
            Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();
        } catch (...) {
        }
    }
}

bool SceneBuilder::SaveTerrainHeightmap(const std::string& Path, const std::vector<float>& Heightmap, uint32_t Resolution) {
    try {
        // Ensure directory exists
        std::filesystem::path FilePath(Path);
        if (FilePath.has_parent_path()) {
            std::filesystem::create_directories(FilePath.parent_path());
        }

        Core::JSONObject Obj;
        Obj["Resolution"] = static_cast<double>(Resolution);
        Obj["Count"] = static_cast<double>(Heightmap.size());

        // Store heightmap as array
        Core::JSONArray HeightArray;
        for (float Height : Heightmap) {
            HeightArray.push_back(Core::JSONValue(static_cast<double>(Height)));
        }
        Obj["Heightmap"] = Core::JSONValue(std::move(HeightArray));

        Core::JSONValue JSON(std::move(Obj));

        std::ofstream File(Path);
        if (!File.is_open()) {
            SIMPLE_LOG("ERROR: Failed to open file for writing: " + Path);
            return false;
        }

        File << JSON.Stringify(true);
        File.close();

        if (!File.good() && File.bad()) {
            SIMPLE_LOG("ERROR: Failed to write terrain heightmap to: " + Path);
            return false;
        }

        SIMPLE_LOG("Saved terrain heightmap to: " + Path);
        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Exception saving terrain heightmap: " + std::string(e.what()));
        return false;
    } catch (...) {
        SIMPLE_LOG("ERROR: Unknown exception saving terrain heightmap");
        return false;
    }
}

bool SceneBuilder::LoadTerrainHeightmap(const std::string& Path, std::vector<float>& OutHeightmap, uint32_t& OutResolution) {
    try {
        std::filesystem::path PathObj(Path);
        if (!std::filesystem::exists(PathObj)) {
            SIMPLE_LOG("Terrain heightmap cache not found: " + Path);
            // Log absolute path for debugging
            try {
                std::filesystem::path AbsPath = std::filesystem::absolute(PathObj);
                SIMPLE_LOG("  Absolute path checked: " + AbsPath.string());
            } catch (...) {
                // Ignore if absolute path resolution fails
            }
            return false;
        }

        std::ifstream File(Path);
        if (!File.is_open()) {
            SIMPLE_LOG("ERROR: Failed to open terrain heightmap cache: " + Path);
            return false;
        }

        std::stringstream Buffer;
        Buffer << File.rdbuf();
        File.close();

        Core::JSONValue JSON = Core::JSONParser::Parse(Buffer.str());
        if (!JSON.IsObject()) {
            SIMPLE_LOG("ERROR: Invalid terrain heightmap cache format: " + Path);
            return false;
        }

        const Core::JSONObject& Obj = JSON.AsObject();

        if (Obj.find("Resolution") == Obj.end() || Obj.find("Heightmap") == Obj.end()) {
            SIMPLE_LOG("ERROR: Terrain heightmap cache missing required fields: " + Path);
            return false;
        }

        OutResolution = static_cast<uint32_t>(Obj.at("Resolution").AsDouble());
        const Core::JSONArray& HeightArray = Obj.at("Heightmap").AsArray();

        OutHeightmap.clear();
        OutHeightmap.reserve(HeightArray.size());
        for (const auto& Val : HeightArray) {
            OutHeightmap.push_back(static_cast<float>(Val.AsDouble()));
        }

        // Validate resolution matches heightmap size
        uint32_t ExpectedSize = OutResolution * OutResolution;
        if (OutHeightmap.size() != ExpectedSize) {
            SIMPLE_LOG("ERROR: Terrain heightmap cache size mismatch (expected " +
                      std::to_string(ExpectedSize) + ", got " + std::to_string(OutHeightmap.size()) + "): " + Path);
            OutHeightmap.clear();
            OutResolution = 0;
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Exception loading terrain heightmap: " + std::string(e.what()));
        OutHeightmap.clear();
        OutResolution = 0;
        return false;
    } catch (...) {
        SIMPLE_LOG("ERROR: Unknown exception loading terrain heightmap");
        OutHeightmap.clear();
        OutResolution = 0;
        return false;
    }
}

void SceneBuilder::AddTrees(
    Render::Scene* Scene,
    Render::MeshLibrary* MeshLibrary,
    Core::MaterialLibrary* MaterialLibrary,
    ECS::Registry* Registry,
    const TreeConfig& Config) {

    if (!Scene || !MeshLibrary || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: SceneBuilder::AddTrees - null pointers");
        return;
    }

    using namespace Solstice::Arzachel;

    float TerrainSize = (m_TerrainSize > 0.0f) ? m_TerrainSize : Config.TerrainSize;
    Seed TreeSeed(Config.Seed);

    for (uint32_t i = 0; i < Config.Count; ++i) {
        Seed InstanceSeed = TreeSeed.Derive(i);

        // Random position within terrain
        float X = (Arzachel::NextFloat(InstanceSeed.Derive(0)) - 0.5f) * TerrainSize;
        float Z = (Arzachel::NextFloat(InstanceSeed.Derive(1)) - 0.5f) * TerrainSize;
        float TreeHeight = Config.MinHeight + (Config.MaxHeight - Config.MinHeight) * Arzachel::NextFloat(InstanceSeed.Derive(2));

        // Generate tree mesh
        auto TreeGen = PineTree(InstanceSeed, TreeHeight, Config.SnowCovered);
        MeshData TreeData = TreeGen(InstanceSeed);
        auto TreeMesh = ConvertToRenderMesh(TreeData);

        if (!TreeMesh || TreeMesh->Vertices.empty()) {
            continue;
        }

        uint32_t TreeMeshID = MeshLibrary->AddMesh(std::move(TreeMesh));

        // Set material
        Render::Mesh* MeshPtr = MeshLibrary->GetMesh(TreeMeshID);
        if (MeshPtr && !MeshPtr->SubMeshes.empty() && Config.MaterialID != 0xFFFFFFFF) {
            MeshPtr->SubMeshes[0].MaterialID = Config.MaterialID;
        }

        // Random rotation and slight scale variation
        float RotationY = Arzachel::NextFloat(InstanceSeed.Derive(3)) * 360.0f;
        float Scale = 0.8f + Arzachel::NextFloat(InstanceSeed.Derive(4)) * 0.4f; // 0.8 to 1.2

        Math::Quaternion Rotation = Math::Quaternion::FromEuler(0, RotationY * 3.14159f / 180.0f, 0);
        Math::Vec3 ScaleVec(Scale, Scale, Scale);

        auto TreeObjID = Scene->AddObject(
            TreeMeshID,
            Math::Vec3(X, 0, Z),
            Rotation,
            ScaleVec,
            Render::ObjectType_Static
        );
    }
}

void SceneBuilder::AddBuildings(
    Render::Scene* Scene,
    Render::MeshLibrary* MeshLibrary,
    Core::MaterialLibrary* MaterialLibrary,
    ECS::Registry* Registry,
    const std::vector<BuildingConfig>& Buildings) {

    if (!Scene || !MeshLibrary || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: SceneBuilder::AddBuildings - null pointers");
        return;
    }

    using namespace Solstice::Arzachel;

    for (size_t i = 0; i < Buildings.size(); ++i) {
        const auto& Building = Buildings[i];
        Seed BuildingSeed(static_cast<uint32_t>(i + 1000));

        Generator<MeshData> BuildingGen;
        switch (Building.BuildingType) {
            case BuildingConfig::Type::SwissHouse:
                BuildingGen = SwissHouse(BuildingSeed, Building.Floors);
                break;
            case BuildingConfig::Type::SwissChurch:
                BuildingGen = SwissChurch(BuildingSeed);
                break;
            case BuildingConfig::Type::SwissTownHall:
                BuildingGen = SwissTownHall(BuildingSeed);
                break;
            case BuildingConfig::Type::SwissShop:
                BuildingGen = SwissShop(BuildingSeed);
                break;
        }

        MeshData BuildingData = BuildingGen(BuildingSeed);
        auto BuildingMesh = ConvertToRenderMesh(BuildingData);

        if (!BuildingMesh || BuildingMesh->Vertices.empty()) {
            continue;
        }

        uint32_t BuildingMeshID = MeshLibrary->AddMesh(std::move(BuildingMesh));

        // Set material
        Render::Mesh* MeshPtr = MeshLibrary->GetMesh(BuildingMeshID);
        if (MeshPtr && !MeshPtr->SubMeshes.empty() && Building.MaterialID != 0xFFFFFFFF) {
            MeshPtr->SubMeshes[0].MaterialID = Building.MaterialID;
        }

        auto BuildingObjID = Scene->AddObject(
            BuildingMeshID,
            Building.Position,
            Building.Rotation,
            Math::Vec3(1, 1, 1),
            Render::ObjectType_Static
        );

        // Add physics body if requested
        if (Building.CreatePhysicsBody && Registry) {
            auto BuildingEntityID = Registry->Create();
            auto& BuildingRB = Registry->Add<Physics::RigidBody>(BuildingEntityID);
            BuildingRB.Position = Building.Position;
            BuildingRB.Rotation = Building.Rotation;
            BuildingRB.IsStatic = true;
            BuildingRB.SetMass(0.0f);
            BuildingRB.Type = Physics::ColliderType::Box;
            // Estimate size from mesh bounds
            if (MeshPtr) {
                Math::Vec3 Size = MeshPtr->BoundsMax - MeshPtr->BoundsMin;
                BuildingRB.HalfExtents = Size * 0.5f;
            }
            BuildingRB.RenderObjectID = BuildingObjID;
        }
    }
}

void SceneBuilder::AddRoads(
    Render::Scene* Scene,
    Render::MeshLibrary* MeshLibrary,
    Core::MaterialLibrary* MaterialLibrary,
    ECS::Registry* Registry,
    const RoadConfig& Config) {

    if (!Scene || !MeshLibrary || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: SceneBuilder::AddRoads - null pointers");
        return;
    }

    using namespace Solstice::Arzachel;

    for (size_t i = 0; i < Config.Segments.size(); ++i) {
        const auto& Segment = Config.Segments[i];
        Seed RoadSeed(static_cast<uint32_t>(i + 2000));

        // Calculate road length and direction
        Math::Vec3 Direction = Segment.End - Segment.Start;
        float Length = Direction.Magnitude();
        if (Length < 0.1f) continue;

        Direction = Direction.Normalized();

        // Generate road mesh
        auto RoadGen = Road(RoadSeed, Length, Segment.Width);
        MeshData RoadData = RoadGen(RoadSeed);
        auto RoadMesh = ConvertToRenderMesh(RoadData);

        if (!RoadMesh || RoadMesh->Vertices.empty()) {
            continue;
        }

        uint32_t RoadMeshID = MeshLibrary->AddMesh(std::move(RoadMesh));

        // Set material
        Render::Mesh* MeshPtr = MeshLibrary->GetMesh(RoadMeshID);
        if (MeshPtr && !MeshPtr->SubMeshes.empty() && Config.MaterialID != 0xFFFFFFFF) {
            MeshPtr->SubMeshes[0].MaterialID = Config.MaterialID;
        }

        // Calculate rotation to align road with direction
        // Calculate Y rotation from direction vector
        float Yaw = std::atan2(Direction.x, Direction.z);
        Math::Quaternion Rotation = Math::Quaternion::FromEuler(0, Yaw, 0);
        Math::Vec3 Center = (Segment.Start + Segment.End) * 0.5f;
        Center.y = 0.01f; // Slightly above terrain

        auto RoadObjID = Scene->AddObject(
            RoadMeshID,
            Center,
            Rotation,
            Math::Vec3(1, 1, 1),
            Render::ObjectType_Static
        );
    }
}

void SceneBuilder::AddGroundVegetation(
    Render::Scene* Scene,
    Render::MeshLibrary* MeshLibrary,
    Core::MaterialLibrary* MaterialLibrary,
    ECS::Registry* Registry,
    const GroundVegetationConfig& Config) {

    if (!Scene || !MeshLibrary || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: SceneBuilder::AddGroundVegetation - null pointers");
        return;
    }

    using namespace Solstice::Arzachel;

    Seed VegSeed(Config.Seed);
    Math::Vec3 AreaSize = Config.AreaMax - Config.AreaMin;

    for (uint32_t i = 0; i < Config.Count; ++i) {
        Seed InstanceSeed = VegSeed.Derive(i);

        // Random position within area
        float X = Config.AreaMin.x + Arzachel::NextFloat(InstanceSeed.Derive(0)) * AreaSize.x;
        float Z = Config.AreaMin.z + Arzachel::NextFloat(InstanceSeed.Derive(1)) * AreaSize.z;
        Math::Vec3 Position(X, 0, Z);

        // Check if position is in exclude area
        bool InExcludeArea = false;
        for (const auto& ExcludePos : Config.ExcludeAreas) {
            Math::Vec3 Diff = Position - ExcludePos;
            Diff.y = 0; // Only check XZ distance
            if (Diff.Magnitude() < Config.ExcludeRadius) {
                InExcludeArea = true;
                break;
            }
        }
        if (InExcludeArea) continue;

        // Random size
        float Size = Config.MinSize + (Config.MaxSize - Config.MinSize) * Arzachel::NextFloat(InstanceSeed.Derive(2));

        // Generate vegetation mesh based on type
        Generator<MeshData> VegGen;
        switch (Config.VegetationType) {
            case GroundVegetationConfig::Type::GrassPatch:
                VegGen = GrassPatch(InstanceSeed, Size);
                break;
            case GroundVegetationConfig::Type::SmallBush:
                VegGen = SmallBush(InstanceSeed, Size);
                break;
            case GroundVegetationConfig::Type::Rock:
                VegGen = Rock(InstanceSeed, Size);
                break;
            case GroundVegetationConfig::Type::SnowDrift:
                VegGen = SnowDrift(InstanceSeed, Size, Size * 0.3f);
                break;
        }

        MeshData VegData = VegGen(InstanceSeed);
        auto VegMesh = ConvertToRenderMesh(VegData);

        if (!VegMesh || VegMesh->Vertices.empty()) {
            continue;
        }

        uint32_t VegMeshID = MeshLibrary->AddMesh(std::move(VegMesh));

        // Set material
        Render::Mesh* MeshPtr = MeshLibrary->GetMesh(VegMeshID);
        if (MeshPtr && !MeshPtr->SubMeshes.empty() && Config.MaterialID != 0xFFFFFFFF) {
            MeshPtr->SubMeshes[0].MaterialID = Config.MaterialID;
        }

        // Random rotation
        float RotationY = Arzachel::NextFloat(InstanceSeed.Derive(3)) * 360.0f;
        Math::Quaternion Rotation = Math::Quaternion::FromEuler(0, RotationY * 3.14159f / 180.0f, 0);

        auto VegObjID = Scene->AddObject(
            VegMeshID,
            Position,
            Rotation,
            Math::Vec3(1, 1, 1),
            Render::ObjectType_Static
        );
    }
}

} // namespace Solstice::Game

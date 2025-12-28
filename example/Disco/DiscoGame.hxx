#pragma once

#include <Game/GameBase.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/VolumetricLighting.hxx>
#include <Render/Scene.hxx>
#include <Render/Camera.hxx>
#include <Core/Audio.hxx>
#include <Core/Material.hxx>
#include <Core/Debug.hxx>
#include <Core/SIMD.hxx>
#include <Render/Mesh.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <UI/UISystem.hxx>
#include <UI/VisualEffects.hxx>
#include <UI/Primitives.hxx>
#include <UI/Animation.hxx>
#include <imgui.h>
#include "BassDrop.hxx"
#include <Arzachel/MeshFactory.hxx>
#include <vector>
#include <array>
#include <cmath>
#include <random>

namespace DiscoDemo {

using namespace Solstice;

namespace DiscoColors {
    inline Math::Vec3 HotPink() { return Math::Vec3(1.0f, 0.1f, 0.5f); }
    inline Math::Vec3 ElectricCyan() { return Math::Vec3(0.0f, 1.0f, 0.9f); }
    inline Math::Vec3 VividPurple() { return Math::Vec3(0.6f, 0.0f, 1.0f); }
    inline Math::Vec3 NeonYellow() { return Math::Vec3(1.0f, 1.0f, 0.0f); }
    inline Math::Vec3 LaserGreen() { return Math::Vec3(0.0f, 1.0f, 0.2f); }
    inline Math::Vec3 DeepBlue() { return Math::Vec3(0.1f, 0.1f, 0.4f); }

    inline Math::Vec3 GetColorByIndex(int Index) {
        switch (Index % 5) {
            case 0: return HotPink();
            case 1: return ElectricCyan();
            case 2: return VividPurple();
            case 3: return NeonYellow();
            case 4: return LaserGreen();
            default: return HotPink();
        }
    }
}

// ============================================================================
// SIMD-Optimized Floor Tile Batch (4 tiles per batch)
// ============================================================================
struct FloorTileBatch {
    Core::SIMD::Vec4 EmissiveR;
    Core::SIMD::Vec4 EmissiveG;
    Core::SIMD::Vec4 EmissiveB;
    Core::SIMD::Vec4 Intensity;
    std::array<uint32_t, 4> MatIDs;

    void Update(float Time, int BatchIndex) {
        float Vals[4];
        for (int I = 0; I < 4; I++) {
            float Offset = static_cast<float>(BatchIndex * 4 + I) * 0.3f;
            // Faster, more dramatic wave pattern for neon glow
            float Wave = std::sin((Time * 3.0f) + Offset) * 0.5f + 0.5f;
            // Add a second wave for more complex pattern
            float Wave2 = std::sin((Time * 1.5f) + Offset * 2.0f) * 0.3f + 0.7f;
            float CombinedWave = Wave * Wave2;

            int ColorIdx = (BatchIndex * 4 + I) % 5;
            Math::Vec3 Color = DiscoColors::GetColorByIndex(ColorIdx);

            // Store in temporary arrays
            float R[4], G[4], B[4], Inten[4];
            EmissiveR.Store(R);
            EmissiveG.Store(G);
            EmissiveB.Store(B);
            Intensity.Store(Inten);

            // Brighter, more saturated colors for neon effect
            R[I] = Color.x * CombinedWave;
            G[I] = Color.y * CombinedWave;
            B[I] = Color.z * CombinedWave;
            Inten[I] = CombinedWave * 4.0f + 1.0f; // Higher base intensity for visible glow

            EmissiveR = Core::SIMD::Vec4::Load(R);
            EmissiveG = Core::SIMD::Vec4::Load(G);
            EmissiveB = Core::SIMD::Vec4::Load(B);
            Intensity = Core::SIMD::Vec4::Load(Inten);
        }
    }

    void Apply(Core::MaterialLibrary& MatLib, float EmissionMultiplier) {
        float R[4], G[4], B[4], Inten[4];
        EmissiveR.Store(R);
        EmissiveG.Store(G);
        EmissiveB.Store(B);
        Intensity.Store(Inten);

        for (int I = 0; I < 4; I++) {
            auto* Mat = MatLib.GetMaterial(MatIDs[I]);
            if (Mat) {
                // Increase emission intensity for visible neon glow
                Mat->SetEmission(Math::Vec3(R[I], G[I], B[I]), Inten[I] * EmissionMultiplier);
            }
        }
    }
};

struct ConfettiSystem {
    static constexpr size_t MAX_PARTICLES = 2048;
    static constexpr size_t SIMD_BATCH = 4;

    // Structure of Arrays for SIMD
    alignas(16) float PosX[MAX_PARTICLES];
    alignas(16) float PosY[MAX_PARTICLES];
    alignas(16) float PosZ[MAX_PARTICLES];
    alignas(16) float VelX[MAX_PARTICLES];
    alignas(16) float VelY[MAX_PARTICLES];
    alignas(16) float VelZ[MAX_PARTICLES];
    alignas(16) float Rotation[MAX_PARTICLES];
    alignas(16) float RotationSpeed[MAX_PARTICLES];
    alignas(16) float Life[MAX_PARTICLES];
    alignas(16) float Size[MAX_PARTICLES];
    ImU32 Colors[MAX_PARTICLES];

    size_t ActiveCount = 0;
    bool Active = false;
    float SpawnTimer = 0.0f;

    void Spawn(size_t Count, const Math::Vec3& Origin) {
        std::mt19937 Rng(static_cast<uint32_t>(Origin.x * 1000));
        std::uniform_real_distribution<float> DistX(-8.0f, 8.0f);
        std::uniform_real_distribution<float> DistVelX(-2.0f, 2.0f);
        std::uniform_real_distribution<float> DistVelY(-1.0f, 0.5f);
        std::uniform_real_distribution<float> DistRot(-3.14f, 3.14f);
        std::uniform_real_distribution<float> DistRotSpeed(-5.0f, 5.0f);
        std::uniform_real_distribution<float> DistSize(0.1f, 0.3f);
        std::uniform_int_distribution<int> DistColor(0, 4);

        for (size_t I = 0; I < Count && ActiveCount < MAX_PARTICLES; I++) {
            size_t Idx = ActiveCount++;

            PosX[Idx] = Origin.x + DistX(Rng);
            PosY[Idx] = Origin.y;
            PosZ[Idx] = Origin.z + DistX(Rng);
            VelX[Idx] = DistVelX(Rng);
            VelY[Idx] = DistVelY(Rng) - 2.0f; // Mostly downward
            VelZ[Idx] = DistVelX(Rng);
            Rotation[Idx] = DistRot(Rng);
            RotationSpeed[Idx] = DistRotSpeed(Rng);
            Life[Idx] = 5.0f + DistSize(Rng) * 3.0f;
            Size[Idx] = DistSize(Rng);

            Math::Vec3 Color = DiscoColors::GetColorByIndex(DistColor(Rng));
            Colors[Idx] = IM_COL32(
                static_cast<int>(Color.x * 255),
                static_cast<int>(Color.y * 255),
                static_cast<int>(Color.z * 255),
                255
            );
        }
    }

    void Update(float Dt) {
        if (!Active && ActiveCount == 0) return;

        const float Gravity = -9.8f * Dt;
        const float Drag = 0.98f;

        // Process in SIMD batches
        size_t BatchCount = (ActiveCount + SIMD_BATCH - 1) / SIMD_BATCH;

        for (size_t Batch = 0; Batch < BatchCount; Batch++) {
            size_t Base = Batch * SIMD_BATCH;

            // Load velocities
            Core::SIMD::Vec4 VX = Core::SIMD::Vec4::Load(&VelX[Base]);
            Core::SIMD::Vec4 VY = Core::SIMD::Vec4::Load(&VelY[Base]);
            Core::SIMD::Vec4 VZ = Core::SIMD::Vec4::Load(&VelZ[Base]);

            // Apply gravity and drag
            Core::SIMD::Vec4 GravVec(Gravity, Gravity, Gravity, Gravity);
            Core::SIMD::Vec4 DragVec(Drag, Drag, Drag, Drag);

            VY = VY + GravVec;
            VX = VX * DragVec;
            VY = VY * DragVec;
            VZ = VZ * DragVec;

            // Store velocities
            VX.Store(&VelX[Base]);
            VY.Store(&VelY[Base]);
            VZ.Store(&VelZ[Base]);

            // Load and update positions
            Core::SIMD::Vec4 PX = Core::SIMD::Vec4::Load(&PosX[Base]);
            Core::SIMD::Vec4 PY = Core::SIMD::Vec4::Load(&PosY[Base]);
            Core::SIMD::Vec4 PZ = Core::SIMD::Vec4::Load(&PosZ[Base]);
            Core::SIMD::Vec4 DtVec(Dt, Dt, Dt, Dt);

            PX = PX + VX * DtVec;
            PY = PY + VY * DtVec;
            PZ = PZ + VZ * DtVec;

            PX.Store(&PosX[Base]);
            PY.Store(&PosY[Base]);
            PZ.Store(&PosZ[Base]);
        }

        // Update life and rotation (scalar loop)
        size_t WriteIdx = 0;
        for (size_t I = 0; I < ActiveCount; I++) {
            Life[I] -= Dt;
            Rotation[I] += RotationSpeed[I] * Dt;

            // Remove dead particles by compacting
            if (Life[I] > 0.0f && PosY[I] > -5.0f) {
                if (WriteIdx != I) {
                    PosX[WriteIdx] = PosX[I];
                    PosY[WriteIdx] = PosY[I];
                    PosZ[WriteIdx] = PosZ[I];
                    VelX[WriteIdx] = VelX[I];
                    VelY[WriteIdx] = VelY[I];
                    VelZ[WriteIdx] = VelZ[I];
                    Rotation[WriteIdx] = Rotation[I];
                    RotationSpeed[WriteIdx] = RotationSpeed[I];
                    Life[WriteIdx] = Life[I];
                    Size[WriteIdx] = Size[I];
                    Colors[WriteIdx] = Colors[I];
                }
                WriteIdx++;
            }
        }
        ActiveCount = WriteIdx;
    }

    void Render(ImDrawList* DrawList, const Render::Camera& Cam, int ScreenW, int ScreenH) {
        if (ActiveCount == 0) return;

        Math::Matrix4 View = Cam.GetViewMatrix();
        float Aspect = static_cast<float>(ScreenW) / static_cast<float>(ScreenH);
        Math::Matrix4 Proj = Math::Matrix4::Perspective(Cam.GetZoom() * 0.0174533f, Aspect, 0.1f, 100.0f);
        Math::Matrix4 ViewProj = Proj * View;

        for (size_t I = 0; I < ActiveCount; I++) {
            Math::Vec4 WorldPos(PosX[I], PosY[I], PosZ[I], 1.0f);
            Math::Vec4 ClipPos = ViewProj * WorldPos;

            if (ClipPos.w <= 0.01f) continue;

            float NdcX = ClipPos.x / ClipPos.w;
            float NdcY = ClipPos.y / ClipPos.w;

            float ScreenX = (NdcX * 0.5f + 0.5f) * ScreenW;
            float ScreenY = (1.0f - (NdcY * 0.5f + 0.5f)) * ScreenH;

            float ScreenSize = (Size[I] * 50.0f) / ClipPos.w;
            ScreenSize = std::max(2.0f, std::min(20.0f, ScreenSize));

            // Draw rotated square (confetti)
            float Angle = Rotation[I];
            float C = std::cos(Angle);
            float S = std::sin(Angle);

            ImVec2 Points[4];
            float HalfSize = ScreenSize * 0.5f;
            float Corners[4][2] = {{-1,-1}, {1,-1}, {1,1}, {-1,1}};

            for (int J = 0; J < 4; J++) {
                float LX = Corners[J][0] * HalfSize;
                float LY = Corners[J][1] * HalfSize * 0.5f; // Flatten
                Points[J] = ImVec2(ScreenX + LX * C - LY * S, ScreenY + LX * S + LY * C);
            }

            DrawList->AddConvexPolyFilled(Points, 4, Colors[I]);
        }
    }
};

// ============================================================================
// Disco Ball with Facet Reflections
// ============================================================================
struct DiscoBall {
    Render::SceneObjectID ObjectID = 0;
    uint32_t MaterialID = 0;
    float RotationAngle = 0.0f;

    static constexpr int NUM_REFLECTED_LIGHTS = 16;
    std::array<Physics::LightSource, NUM_REFLECTED_LIGHTS> ReflectedLights;
    int ActiveReflections = 0;

    void Initialize(Render::Scene& Scene, Core::MaterialLibrary& MatLib, Render::MeshLibrary& MeshLib, uint32_t IcosphereMeshID) {
        // Create disco ball material (highly reflective and emissive)
        Core::Material BallMat;
        BallMat.SetAlbedoColor({0.9f, 0.9f, 0.95f}, 0.1f); // Very low roughness
        BallMat.Metallic = 255; // Full metallic
        BallMat.SetEmission({1.0f, 1.0f, 1.0f}, 3.0f); // Bright white emission for light rays (will be updated from UI)
        MaterialID = MatLib.AddMaterial(BallMat);

        // Add to scene at top-center using icosphere mesh
        ObjectID = Scene.AddObject(IcosphereMeshID, {0, 8, 0}, Math::Quaternion(), {1.5f, 1.5f, 1.5f});
        Scene.SetMaterial(ObjectID, MaterialID);
    }

    void Update(float Dt, const Math::Vec3& BallPos, const std::vector<Physics::LightSource>& SpotLights) {
        RotationAngle += Dt * 0.5f;

        // Generate reflected lights from spotlights hitting the ball
        ActiveReflections = 0;
        float FacetAngle = RotationAngle;

        for (int I = 0; I < NUM_REFLECTED_LIGHTS && I < static_cast<int>(SpotLights.size() * 4); I++) {
            // Simulate facet reflection
            float Angle = FacetAngle + static_cast<float>(I) * (6.28f / NUM_REFLECTED_LIGHTS);
            float Pitch = std::sin(Angle * 3.0f) * 0.5f - 0.3f; // Mostly downward

            Math::Vec3 Dir(std::cos(Angle) * std::cos(Pitch),
                          std::sin(Pitch),
                          std::sin(Angle) * std::cos(Pitch));

            // Ray from ball position
            Math::Vec3 HitPos = BallPos + Dir * 15.0f;

            // Clamp to floor/walls
            if (HitPos.y < -0.5f) {
                float T = (BallPos.y + 0.5f) / (BallPos.y - HitPos.y);
                HitPos = BallPos + Dir * 15.0f * T;
            }

            ReflectedLights[ActiveReflections].Type = Physics::LightSource::LightType::Point;
            ReflectedLights[ActiveReflections].Position = HitPos;
            ReflectedLights[ActiveReflections].Color = DiscoColors::GetColorByIndex(I % 5);
            ReflectedLights[ActiveReflections].Intensity = 3.0f;
            ReflectedLights[ActiveReflections].Range = 5.0f;
            ReflectedLights[ActiveReflections].Attenuation = 0.5f;

            ActiveReflections++;
        }
    }
};

// ============================================================================
// Main Disco Game Class
// ============================================================================
class DiscoGame : public Game::GameBase {
public:
    std::unique_ptr<Render::SoftwareRenderer> m_Renderer;
    std::unique_ptr<Render::VolumetricLighting> m_Volumetrics;
    Render::Scene m_Scene;
    Render::Camera m_Camera;
    Core::MaterialLibrary m_MaterialLibrary;
    Render::MeshLibrary m_MeshLibrary;

    std::vector<Physics::LightSource> m_Lights;
    float m_Time = 0.0f;

    // === SCENE OBJECTS ===
    DiscoBall m_DiscoBall;
    std::array<std::array<Render::SceneObjectID, 8>, 8> m_FloorTiles;
    std::array<FloorTileBatch, 16> m_FloorTileBatches;
    ConfettiSystem m_Confetti;

    // Sweeping spotlights
    std::array<Physics::LightSource, 4> m_SpotLights;
    std::array<float, 4> m_SpotAngles = {0.0f, 1.57f, 3.14f, 4.71f};

    // Materials
    uint32_t m_FloorMatID = 0;
    uint32_t m_WallMatID = 0;
    uint32_t m_IcosphereMeshID = 0;

    // Bass drop state
    bool m_BassDropActive = false;
    float m_BassDropTimer = 0.0f;
    float m_CameraShake = 0.0f;
    Math::Vec3 m_CameraShakeOffset;
    float m_FlashIntensity = 0.0f;

    // UI-controllable parameters
    float m_VolumetricDensity = 1.2f;
    float m_VolumetricExposure = 0.6f;
    float m_GodRayDensity = 1.0f;
    float m_GodRayDecay = 0.94f;
    float m_GodRayExposure = 3.0f;
    float m_DiscoBallEmission = 3.0f;
    float m_DiscoBallLightIntensity = 10.0f;
    float m_FloorTileEmissionMultiplier = 2.0f;
    float m_CameraFOV = 60.0f;
    float m_SpotlightIntensity = 5.0f;
    bool m_ShowDebugPanel = true;

    void CreateCubeMesh() {
        auto Mesh = std::make_unique<Render::Mesh>();

        auto AddQuad = [&](Math::Vec3 P1, Math::Vec3 P2, Math::Vec3 P3, Math::Vec3 P4, Math::Vec3 N) {
            uint32_t Start = static_cast<uint32_t>(Mesh->Vertices.size());
            Mesh->AddVertex(P1, N, {0, 0});
            Mesh->AddVertex(P2, N, {1, 0});
            Mesh->AddVertex(P3, N, {1, 1});
            Mesh->AddVertex(P4, N, {0, 1});
            Mesh->AddTriangle(Start, Start + 1, Start + 2);
            Mesh->AddTriangle(Start, Start + 2, Start + 3);
        };

        AddQuad({-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f});
        AddQuad({0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f});
        AddQuad({-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f});
        AddQuad({0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});
        AddQuad({-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f});
        AddQuad({-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f});

        Mesh->AddSubMesh(0, 0, static_cast<uint32_t>(Mesh->Indices.size()));
        Mesh->CalculateBounds();

        m_MeshLibrary.AddMesh(std::move(Mesh));
    }

    void CreateIcosphereMesh() {
        auto Mesh = Solstice::Arzachel::MeshFactory::CreateIcosphere(1.0f, 2);
        m_IcosphereMeshID = m_MeshLibrary.AddMesh(std::move(Mesh));
    }

    void CreateFloorTiles() {
        // Create 8x8 floor tiles with individual materials
        int BatchIdx = 0;
        int TileInBatch = 0;

        for (int Z = 0; Z < 8; Z++) {
            for (int X = 0; X < 8; X++) {
                float TileX = (X - 3.5f) * 2.5f;
                float TileZ = (Z - 3.5f) * 2.5f;

                // Create unique material for each tile
                Core::Material TileMat;
                Math::Vec3 BaseColor = DiscoColors::GetColorByIndex((X + Z) % 5);
                TileMat.SetAlbedoColor(BaseColor * 0.3f, 0.3f);
                // Start with higher base emission that will be animated
                TileMat.SetEmission(BaseColor, 2.0f);
                uint32_t MatID = m_MaterialLibrary.AddMaterial(TileMat);

                // Add tile to scene
                m_FloorTiles[Z][X] = m_Scene.AddObject(0, {TileX, -0.5f, TileZ}, Math::Quaternion(), {2.4f, 0.2f, 2.4f});
                m_Scene.SetMaterial(m_FloorTiles[Z][X], MatID);

                // Add to batch
                m_FloorTileBatches[BatchIdx].MatIDs[TileInBatch] = MatID;
                TileInBatch++;

                if (TileInBatch >= 4) {
                    TileInBatch = 0;
                    BatchIdx++;
                }
            }
        }
    }

    void CreateSpotLights() {
        // 4 corner spotlights
        Math::Vec3 Corners[4] = {
            {-10.0f, 10.0f, -10.0f},
            { 10.0f, 10.0f, -10.0f},
            { 10.0f, 10.0f,  10.0f},
            {-10.0f, 10.0f,  10.0f}
        };

        for (int I = 0; I < 4; I++) {
            m_SpotLights[I].Type = Physics::LightSource::LightType::Spot;
            m_SpotLights[I].Position = Corners[I];
            m_SpotLights[I].Color = DiscoColors::GetColorByIndex(I);
            m_SpotLights[I].Intensity = 5.0f;
            m_SpotLights[I].Range = 30.0f;
            m_SpotLights[I].Attenuation = 0.1f;
        }
    }

    void Initialize() override {
        if (!m_Window) return;
        auto Size = m_Window->GetFramebufferSize();

        try {
            m_Renderer = std::make_unique<Render::SoftwareRenderer>(Size.first, Size.second, 32, m_Window->NativeWindow());
            if (!m_Renderer) {
                SIMPLE_LOG("ERROR: Failed to create renderer");
                return;
            }
        } catch (const std::exception& E) {
            SIMPLE_LOG("ERROR: Exception creating renderer: " + std::string(E.what()));
            return;
        }

        m_Scene.SetMaterialLibrary(&m_MaterialLibrary);
        m_Scene.SetMeshLibrary(&m_MeshLibrary);

        // Create cube mesh
        CreateCubeMesh();

        // Update scene transforms to ensure bounds are calculated
        m_Scene.UpdateTransforms();

        // Create icosphere mesh for disco ball
        CreateIcosphereMesh();

        // Initialize volumetric lighting
        m_Volumetrics = std::make_unique<Render::VolumetricLighting>();
        m_Volumetrics->Initialize(Size.first, Size.second,
                                  Math::Vec3(-20, -5, -20),
                                  Math::Vec3(20, 15, 20));
        m_Volumetrics->SetDensity(m_VolumetricDensity);
        m_Volumetrics->SetExposure(m_VolumetricExposure);

        // Initialize UI
        UI::UISystem::Instance().Initialize(m_Window->NativeWindow());
        void* ImguiContext = UI::UISystem::Instance().GetImGuiContext();
        if (ImguiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ImguiContext));
        }

        // Camera setup - start inside the disco room
        // Room bounds: x: -12 to 12, z: -12 to 12 (approximately)
        m_Camera.Position = {0.0f, 8.0f, -8.0f}; // Start inside, closer to center
        m_Camera.WorldUp = Math::Vec3(0.0f, 1.0f, 0.0f);
        m_Camera.Up = Math::Vec3(0.0f, 1.0f, 0.0f);
        m_Camera.Zoom = m_CameraFOV; // Wider field of view
        m_Camera.Yaw = 90.0f;
        m_Camera.Pitch = -15.0f;
        m_Camera.ProcessMouseMovement(0, 0);

        // Create materials
        {
            Core::Material FloorMat;
            FloorMat.SetAlbedoColor({0.1f, 0.1f, 0.15f}, 0.2f);
            FloorMat.Metallic = static_cast<uint8_t>(0.9f * 255.0f);
            m_FloorMatID = m_MaterialLibrary.AddMaterial(FloorMat);

            Core::Material WallMat;
            WallMat.SetAlbedoColor({0.05f, 0.05f, 0.1f}, 0.6f);
            m_WallMatID = m_MaterialLibrary.AddMaterial(WallMat);
        }

        // Create floor tiles
        CreateFloorTiles();

        // Create walls
        {
            Render::SceneObjectID BackWall = m_Scene.AddObject(0, {0, 5, 12}, Math::Quaternion(), {25, 12, 1});
            m_Scene.SetMaterial(BackWall, m_WallMatID);

            Render::SceneObjectID LeftWall = m_Scene.AddObject(0, {-12, 5, 0}, Math::Quaternion(), {1, 12, 25});
            m_Scene.SetMaterial(LeftWall, m_WallMatID);

            Render::SceneObjectID RightWall = m_Scene.AddObject(0, {12, 5, 0}, Math::Quaternion(), {1, 12, 25});
            m_Scene.SetMaterial(RightWall, m_WallMatID);
        }

        // Initialize disco ball (pass icosphere mesh ID)
        m_DiscoBall.Initialize(m_Scene, m_MaterialLibrary, m_MeshLibrary, m_IcosphereMeshID);

        // Update all transforms to ensure proper bounds calculation for culling
        m_Scene.UpdateTransforms();

        // Create spotlights
        CreateSpotLights();

        // Ambient light (dark club atmosphere)
        Physics::LightSource Ambient;
        Ambient.Type = Physics::LightSource::LightType::Directional;
        Ambient.Position = Math::Vec3(0.5f, -1.0f, 0.5f).Normalized();
        Ambient.Color = DiscoColors::DeepBlue();
        Ambient.Intensity = 0.3f;
        m_Lights.push_back(Ambient);

        // Add spotlights to main light list
        for (int I = 0; I < 4; I++) {
            m_Lights.push_back(m_SpotLights[I]);
        }

        // Audio
        BassDrop::Initialize();

        SIMPLE_LOG("DiscoGame: Initialized with " + std::to_string(m_Scene.GetObjectCount()) + " objects");
    }

    void UpdateFloorTilesBatched(float Time) {
        for (int I = 0; I < 16; I++) {
            m_FloorTileBatches[I].Update(Time, I);
            m_FloorTileBatches[I].Apply(m_MaterialLibrary, m_FloorTileEmissionMultiplier);
        }
    }

    void UpdateSpotlightsBatched(float Time) {
        for (int I = 0; I < 4; I++) {
            m_SpotAngles[I] += 0.02f;

            float Angle = m_SpotAngles[I];
            float SweepX = std::sin(Angle) * 8.0f;
            float SweepZ = std::cos(Angle * 0.7f) * 8.0f;

            // Update spotlight target
            Math::Vec3 Target(SweepX, 0.0f, SweepZ);
            Math::Vec3 Dir = (Target - m_SpotLights[I].Position).Normalized();

            // Pulse color
            float Pulse = std::sin(Time * 3.0f + I * 1.57f) * 0.5f + 0.5f;
            m_SpotLights[I].Color = DiscoColors::GetColorByIndex(I) * (0.5f + Pulse * 0.5f);
            m_SpotLights[I].Intensity = m_SpotlightIntensity * (0.8f + Pulse * 0.4f); // Use UI-controlled base intensity

            // Update in main lights array
            if (I + 1 < static_cast<int>(m_Lights.size())) {
                m_Lights[I + 1] = m_SpotLights[I];
            }
        }
    }

    void TriggerBassDrop() {
        BassDrop::Play();

        m_BassDropActive = true;
        m_BassDropTimer = 0.0f;
        m_CameraShake = 1.0f;
        m_FlashIntensity = 1.0f;

        // Spawn confetti
        m_Confetti.Active = true;
        m_Confetti.Spawn(500, Math::Vec3(0, 10, 0));

        // Flash all floor tiles white
        for (auto& Batch : m_FloorTileBatches) {
            for (int I = 0; I < 4; I++) {
                auto* Mat = m_MaterialLibrary.GetMaterial(Batch.MatIDs[I]);
                if (Mat) {
                    Mat->SetEmission({1.0f, 1.0f, 1.0f}, 10.0f);
                }
            }
        }

        // Flash all lights
        for (auto& Light : m_Lights) {
            Light.Intensity *= 5.0f;
        }
    }

    void UpdateBassDrop(float Dt) {
        if (!m_BassDropActive) return;

        m_BassDropTimer += Dt;

        // Phase 1: Flash (0-0.3s)
        if (m_BassDropTimer < 0.3f) {
            m_FlashIntensity = 1.0f - (m_BassDropTimer / 0.3f);
        }
        // Phase 2: Settle (0.3-2.0s)
        else if (m_BassDropTimer < 2.0f) {
            m_FlashIntensity = 0.0f;

            // Continue spawning confetti
            m_Confetti.SpawnTimer += Dt;
            if (m_Confetti.SpawnTimer > 0.05f) {
                m_Confetti.Spawn(20, Math::Vec3(0, 10, 0));
                m_Confetti.SpawnTimer = 0.0f;
            }
        }
        // Phase 3: Done
        else {
            m_BassDropActive = false;
            m_FlashIntensity = 0.0f;
            m_Confetti.Active = false;
        }

        // Camera shake decay
        if (m_CameraShake > 0.01f) {
            float ShakeStr = m_CameraShake * 0.3f;
            m_CameraShakeOffset = Math::Vec3(
                std::sin(m_Time * 50.0f) * ShakeStr,
                std::cos(m_Time * 40.0f) * ShakeStr * 0.5f,
                0.0f
            );
            m_CameraShake *= 0.92f;
        } else {
            m_CameraShakeOffset = Math::Vec3(0, 0, 0);
        }
    }

    void Update(float DeltaTime) override {
        m_Time += DeltaTime;

        // Camera orbit - oscillate back and forth inside the disco room
        // Room bounds: x: -12 to 12, z: -12 to 12, so use smaller radius
        // Use a clamped angle that oscillates between -60° and +60° (120° total range)
        float Angle = std::sin(m_Time * 0.15f) * 1.047f; // ±60 degrees in radians
        float Radius = 10.0f; // Smaller radius to keep camera inside room
        float CamX = std::sin(Angle) * Radius;
        float CamZ = std::cos(Angle) * Radius;
        // Clamp to room bounds to ensure we stay inside
        CamX = std::max(-10.0f, std::min(10.0f, CamX));
        CamZ = std::max(-10.0f, std::min(10.0f, CamZ));
        m_Camera.Position = Math::Vec3(CamX, 10.0f, CamZ) + m_CameraShakeOffset;

        Math::Vec3 Direction = (Math::Vec3(0, 2, 0) - m_Camera.Position).Normalized();
        m_Camera.Pitch = std::asin(Direction.y) * 57.29578f;
        m_Camera.Yaw = std::atan2(Direction.z, Direction.x) * 57.29578f;
        m_Camera.ProcessMouseMovement(0, 0);

        // Update floor tiles with SIMD batching
        UpdateFloorTilesBatched(m_Time);

        // Update spotlights
        UpdateSpotlightsBatched(m_Time);

        // Update disco ball material emission from UI
        auto* DiscoBallMat = m_MaterialLibrary.GetMaterial(m_DiscoBall.MaterialID);
        if (DiscoBallMat) {
            DiscoBallMat->SetEmission({1.0f, 1.0f, 1.0f}, m_DiscoBallEmission);
        }

        // Update disco ball
        m_DiscoBall.Update(DeltaTime, Math::Vec3(0, 8, 0),
                          std::vector<Physics::LightSource>(m_SpotLights.begin(), m_SpotLights.end()));

        // Add disco ball reflected lights to main lights
        size_t BaseReflectedIdx = 5; // After ambient + 4 spots
        for (int I = 0; I < m_DiscoBall.ActiveReflections; I++) {
            if (BaseReflectedIdx + I < m_Lights.size()) {
                m_Lights[BaseReflectedIdx + I] = m_DiscoBall.ReflectedLights[I];
            } else if (m_Lights.size() < 32) {
                m_Lights.push_back(m_DiscoBall.ReflectedLights[I]);
            }
        }

        // Update confetti
        m_Confetti.Update(DeltaTime);

        // Update bass drop
        UpdateBassDrop(DeltaTime);

        // Update volumetric lighting
        if (m_Volumetrics) {
            m_Volumetrics->BuildOcclusionGrid(m_Scene);
            auto Size = m_Window->GetFramebufferSize();
            float Aspect = static_cast<float>(Size.first) / static_cast<float>(Size.second);
            Math::Matrix4 View = m_Camera.GetViewMatrix();
            Math::Matrix4 Proj = Math::Matrix4::Perspective(m_CameraFOV * 0.0174533f, Aspect, 0.1f, 100.0f);
            Math::Matrix4 ViewProj = Proj * View;

            // Add disco ball as a light source for volumetric lighting
            std::vector<Physics::LightSource> VolumetricLights = m_Lights;
            Physics::LightSource DiscoBallLight;
            DiscoBallLight.Type = Physics::LightSource::LightType::Point;
            DiscoBallLight.Position = Math::Vec3(0, 8, 0); // Disco ball position
            DiscoBallLight.Color = Math::Vec3(1.0f, 1.0f, 1.0f); // White light
            DiscoBallLight.Intensity = m_DiscoBallLightIntensity;
            DiscoBallLight.Range = 30.0f; // Wide range
            DiscoBallLight.Attenuation = 0.1f; // Lower attenuation for stronger light
            VolumetricLights.push_back(DiscoBallLight);

            // Update volumetric settings from UI
            m_Volumetrics->SetDensity(m_VolumetricDensity);
            m_Volumetrics->SetExposure(m_VolumetricExposure);

            m_Volumetrics->TraceVolumetricRays(VolumetricLights, m_Camera.Position, ViewProj);
        }

        // Update camera FOV
        m_Camera.Zoom = m_CameraFOV;

        // Update spotlight intensities
        for (int I = 0; I < 4; I++) {
            m_SpotLights[I].Intensity = m_SpotlightIntensity;
            if (I + 1 < static_cast<int>(m_Lights.size())) {
                m_Lights[I + 1].Intensity = m_SpotlightIntensity;
            }
        }
    }

    void RenderUI() {
        auto Size = m_Window->GetFramebufferSize();
        float CenterX = Size.first * 0.5f;
        float BottomY = Size.second - 130.0f;

        ImDrawList* DrawList = ImGui::GetBackgroundDrawList();

        // Glassmorphism panel
        ImVec2 PanelMin(CenterX - 180, BottomY);
        ImVec2 PanelMax(CenterX + 180, BottomY + 110);

        // Panel background with rounded corners
        DrawList->AddRectFilled(PanelMin, PanelMax, IM_COL32(20, 20, 40, 200), 15.0f);

        // Neon border glow
        float GlowPulse = std::sin(m_Time * 3.0f) * 0.3f + 0.7f;
        ImU32 GlowColor = IM_COL32(255, 0, 200, static_cast<int>(150 * GlowPulse));

        for (int I = 0; I < 3; I++) {
            float Offset = I * 2.0f;
            DrawList->AddRect(
                ImVec2(PanelMin.x - Offset, PanelMin.y - Offset),
                ImVec2(PanelMax.x + Offset, PanelMax.y + Offset),
                IM_COL32(255, 0, 200, static_cast<int>(50 * GlowPulse / (I + 1))),
                15.0f + Offset, 0, 2.0f
            );
        }

        // Title
        const char* Title = "DISCO INFERNO";
        ImVec2 TitleSize = ImGui::CalcTextSize(Title);
        DrawList->AddText(ImVec2(CenterX - TitleSize.x * 0.5f, BottomY + 10),
                         IM_COL32(255, 100, 200, 255), Title);

        // Bass drop button
        ImGui::SetNextWindowPos(ImVec2(CenterX - 140, BottomY + 40));
        ImGui::SetNextWindowSize(ImVec2(280, 60));
        ImGui::Begin("##DiscoPanelBtn", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_NoScrollbar);

        // Animated button glow
        ImVec2 BtnMin = ImGui::GetCursorScreenPos();
        ImVec2 BtnMax(BtnMin.x + 280, BtnMin.y + 50);

        // Button glow effect
        float BtnGlow = std::sin(m_Time * 5.0f) * 0.5f + 0.5f;
        for (int I = 0; I < 5; I++) {
            float Offset = I * 3.0f;
            DrawList->AddRect(
                ImVec2(BtnMin.x - Offset, BtnMin.y - Offset),
                ImVec2(BtnMax.x + Offset, BtnMax.y + Offset),
                IM_COL32(0, 255, 255, static_cast<int>(100 * BtnGlow / (I + 1))),
                10.0f + Offset
            );
        }

        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 100, 150, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 150, 200, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 200, 255, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

        if (ImGui::Button("INITIATE BASS DROP", ImVec2(280, 50))) {
            TriggerBassDrop();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::End();

        // Flash overlay
        if (m_FlashIntensity > 0.01f) {
            ImU32 FlashColor = IM_COL32(255, 200, 255, static_cast<int>(200 * m_FlashIntensity));
            DrawList->AddRectFilled(ImVec2(0, 0), ImVec2(Size.first, Size.second), FlashColor);
        }
    }

    void RenderDebugPanel() {
        if (!m_ShowDebugPanel) return;

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 600), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Lighting & Rendering Controls", &m_ShowDebugPanel)) {
            if (ImGui::CollapsingHeader("Volumetric Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Density", &m_VolumetricDensity, 0.0f, 3.0f, "%.2f");
                ImGui::SliderFloat("Exposure", &m_VolumetricExposure, 0.0f, 2.0f, "%.2f");
            }

            if (ImGui::CollapsingHeader("God Rays", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("God Ray Density", &m_GodRayDensity, 0.0f, 2.0f, "%.2f");
                ImGui::SliderFloat("God Ray Decay", &m_GodRayDecay, 0.8f, 1.0f, "%.3f");
                ImGui::SliderFloat("God Ray Exposure", &m_GodRayExposure, 0.0f, 10.0f, "%.2f");
            }

            if (ImGui::CollapsingHeader("Disco Ball", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Emission Intensity", &m_DiscoBallEmission, 0.0f, 10.0f, "%.2f");
                ImGui::SliderFloat("Light Intensity", &m_DiscoBallLightIntensity, 0.0f, 50.0f, "%.1f");
            }

            if (ImGui::CollapsingHeader("Floor Tiles", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Emission Multiplier", &m_FloorTileEmissionMultiplier, 0.0f, 5.0f, "%.2f");
            }

            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Field of View", &m_CameraFOV, 30.0f, 120.0f, "%.1f");
            }

            if (ImGui::CollapsingHeader("Spotlights", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Intensity", &m_SpotlightIntensity, 0.0f, 20.0f, "%.1f");
            }

            ImGui::Separator();
            if (ImGui::Button("Reset to Defaults")) {
                m_VolumetricDensity = 1.2f;
                m_VolumetricExposure = 0.6f;
                m_GodRayDensity = 1.0f;
                m_GodRayDecay = 0.94f;
                m_GodRayExposure = 3.0f;
                m_DiscoBallEmission = 3.0f;
                m_DiscoBallLightIntensity = 10.0f;
                m_FloorTileEmissionMultiplier = 2.0f;
                m_CameraFOV = 60.0f;
                m_SpotlightIntensity = 5.0f;
            }
        }
        ImGui::End();
    }

    void Render() override {
        if (!m_Renderer || !m_Window) return;

        UI::UISystem::Instance().NewFrame();

        m_Renderer->Clear({0.02f, 0.02f, 0.05f, 1.0f});

        // Set HDR exposure
        // m_Renderer->SetHDRExposure(1.5f + m_FlashIntensity * 2.0f);

        // Pass volumetric texture to post-processing and enable godrays (do this every frame)
        if (m_Volumetrics && m_Renderer) {
            bgfx::TextureHandle volTex = m_Volumetrics->GetVolumetricTexture();
            Render::PostProcessing* postProc = m_Renderer->GetPostProcessing();
            if (postProc) {
                if (bgfx::isValid(volTex)) {
                    postProc->SetVolumetricTexture(volTex);
                }
                postProc->SetGodRayEnabled(true);
                Render::PostProcessing::GodRaySettings godRaySettings;
                godRaySettings.Enabled = true;
                godRaySettings.Density = m_GodRayDensity;
                godRaySettings.Decay = m_GodRayDecay;
                godRaySettings.Exposure = m_GodRayExposure;
                postProc->SetGodRaySettings(godRaySettings);
            }
        }

        // Render scene with all lights
        if (m_Scene.GetObjectCount() > 0) {
            m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);
        }

        // Render confetti overlay
        if (m_Confetti.ActiveCount > 0) {
            auto Size = m_Window->GetFramebufferSize();
            m_Confetti.Render(ImGui::GetBackgroundDrawList(), m_Camera, Size.first, Size.second);
        }

        // Render UI
        RenderUI();

        // Render debug/control panel
        RenderDebugPanel();

        UI::UISystem::Instance().Render();
        m_Renderer->Present();
    }

    void Shutdown() override {
        UI::UISystem::Instance().Shutdown();
        Core::Audio::AudioManager::Instance().Shutdown();

        if (m_Volumetrics) {
            m_Volumetrics->Shutdown();
            m_Volumetrics.reset();
        }

        m_Renderer.reset();
    }
};

} // namespace DiscoDemo

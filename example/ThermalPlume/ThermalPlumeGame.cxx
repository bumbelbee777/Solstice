#include "ThermalPlumeGame.hxx"
#include <UI/Core/UISystem.hxx>
#include <Solstice.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Arzachel/MeshFactory.hxx>
#include <Arzachel/GeometryOps.hxx>
#include <Arzachel/Polyhedra.hxx>
#include <Arzachel/MaterialPresets.hxx>
#include <Core/System/Async.hxx>
#include <Core/Audio/Audio.hxx>
#include <Core/Debug/Debug.hxx>
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace Solstice;
using namespace Solstice::Render;
using namespace Solstice::UI;
using namespace Solstice::Math;

namespace MeshFactory = Solstice::Arzachel::MeshFactory;
using Solstice::Arzachel::Seed;
using Solstice::Arzachel::MeshGenerator;
using Solstice::Arzachel::MeshData;

namespace Solstice::ThermalPlume {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ThermalPlumeGame::ThermalPlumeGame() = default;

ThermalPlumeGame::~ThermalPlumeGame() {
    Shutdown();
}

// ---------------------------------------------------------------------------
// GameBase overrides
// ---------------------------------------------------------------------------

void ThermalPlumeGame::Initialize() {
    Solstice::Initialize();

    InitializeWindow();

    auto* window = GetWindow();
    if (!window) {
        SIMPLE_LOG("ERROR: Window not set");
        return;
    }
    auto fbSize = window->GetFramebufferSize();
    m_Renderer = std::make_unique<SoftwareRenderer>(
        fbSize.first, fbSize.second, 16, window->NativeWindow());

    UISystem::Instance().Initialize(m_Window->NativeWindow());
    if (void* ctx = UISystem::Instance().GetImGuiContext()) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx));
    }

    m_MeshLibrary = std::make_unique<Render::MeshLibrary>();
    m_MaterialLibrary = std::make_unique<Core::MaterialLibrary>();
    m_Scene.SetMeshLibrary(m_MeshLibrary.get());
    m_Scene.SetMaterialLibrary(m_MaterialLibrary.get());

    Physics::PhysicsSystem::Instance().Start(m_Registry);

    InitializeScene();
    InitializeFluidSim();
    InitializeParticles();
    InitializeLighting();

    // Default camera: slightly offset, looking at the nozzle area
    m_Camera = Camera(Vec3(0.0f, 2.0f, 4.5f), Vec3(0.0f, 1.0f, 0.0f), -90.0f, -5.0f);

    SIMPLE_LOG("ThermalPlume initialized");
}

void ThermalPlumeGame::Shutdown() {
    if (m_ExhaustParticles) {
        m_ExhaustParticles->Shutdown();
        m_ExhaustParticles.reset();
    }
    m_VolumeViz.Shutdown();
    if (m_Fluid) {
        Physics::PhysicsSystem::Instance().UnregisterFluidSimulation(m_Fluid.get());
        m_Fluid.reset();
    }
    Physics::PhysicsSystem::Instance().Stop();
    UISystem::Instance().Shutdown();
    Solstice::Shutdown();
}

void ThermalPlumeGame::Update(float DeltaTime) {
    DeltaTime = std::min(DeltaTime, 0.05f);

    UpdateCamera(DeltaTime);

    if (m_SimState == SimState::Running) {
        // Nozzle injection before physics step
        InjectNozzle(DeltaTime);

        m_PhysicsAccumulator += DeltaTime;
        int steps = 0;
        while (m_PhysicsAccumulator >= kPhysicsStep && steps < kMaxSubSteps) {
            Physics::PhysicsSystem::Instance().Update(kPhysicsStep);
            m_PhysicsAccumulator -= kPhysicsStep;
            ++steps;
        }
        if (m_PhysicsAccumulator > kPhysicsStep * kMaxSubSteps) {
            m_PhysicsAccumulator = 0.0f;
        }
    }

    UpdateEmissiveMaterial();

    if (m_ExhaustParticles && m_SimState == SimState::Running) {
        m_ExhaustParticles->SetThrottle(m_Firing ? m_Throttle : 0.0f);
        m_ExhaustParticles->SetFiring(m_Firing && m_SimState == SimState::Running);
        m_ExhaustParticles->SetExhaustVelocity(m_ExhaustVelocity * 0.04f);
        m_ExhaustParticles->SetTurbulenceIntensity(m_TurbulenceIntensity);
        m_ExhaustParticles->SetNozzleRadius(m_NozzleDiameter * 0.5f);
        m_ExhaustParticles->Update(DeltaTime, m_Camera.Position);
    }

    // Handle pending resize
    if (m_PendingResize) {
        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(now - m_LastResizeEvent).count();
        if (elapsed > 0.15f) {
            m_PendingResize = false;
            if (m_Renderer) {
                m_Renderer->Resize(m_PendingWidth, m_PendingHeight);
            }
        }
    }
}

void ThermalPlumeGame::Render() {
    if (!m_Renderer) return;

    UISystem::Instance().NewFrame();

    m_Renderer->Clear(Vec4(0.04f, 0.05f, 0.08f, 1.0f));
    m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);

    // Fluid volume overlay
    if (m_ShowVolume && m_Fluid) {
        int vw = m_Renderer->GetFramebufferWidth();
        int vh = m_Renderer->GetFramebufferHeight();
        auto dm = FluidVolumeVisualizer::DrawMode::Raymarch;
        auto fm = FluidVolumeVisualizer::FieldMode::Temperature;
        if (m_DrawMode == 0) {
            dm = FluidVolumeVisualizer::DrawMode::Raymarch;
            fm = FluidVolumeVisualizer::FieldMode::Temperature;
        } else if (m_DrawMode == 1) {
            dm = FluidVolumeVisualizer::DrawMode::Raymarch;
            fm = FluidVolumeVisualizer::FieldMode::Schlieren;
        } else if (m_DrawMode == 2) {
            dm = FluidVolumeVisualizer::DrawMode::IsoSurface;
        } else {
            dm = FluidVolumeVisualizer::DrawMode::VelocityLines;
        }
        m_VolumeViz.DrawOverlay(m_Camera, vw, vh, *m_Fluid, dm, fm,
                                m_IsoLevel, m_VelStride, m_VelScale, 0);
    }

    // Exhaust particles
    if (m_ShowParticles && m_ExhaustParticles && m_ExhaustParticles->GetActiveParticleCount() > 0) {
        float aspect = static_cast<float>(m_Renderer->GetFramebufferWidth()) /
                       std::max(1.0f, static_cast<float>(m_Renderer->GetFramebufferHeight()));
        Matrix4 view = m_Camera.GetViewMatrix();
        Matrix4 proj = Matrix4::Perspective(m_Camera.GetZoom() * 0.0174533f, aspect, 0.1f, 500.0f);
        if (m_Camera.UseOrthographic) {
            proj = m_Camera.GetProjectionMatrix(aspect, 0.1f, 500.0f);
        }
        Matrix4 viewProj = proj * view;
        m_ExhaustParticles->Render(2, viewProj, m_Camera.Right, m_Camera.Up);
    }

    RenderUI();

    UISystem::Instance().Render();
    m_Renderer->Present();
}

void ThermalPlumeGame::HandleInput() {
    auto* window = GetWindow();
    if (!window) return;

    const float camSpeed = m_CameraSpeed;
    float dt = GetDeltaTime();

    if (m_MouseLocked && m_CameraMode == CameraMode::FreeFly) {
        if (window->IsKeyScanPressed(26) || window->IsKeyScanPressed(82))
            m_Camera.Position += m_Camera.Front * camSpeed * dt;
        if (window->IsKeyScanPressed(22) || window->IsKeyScanPressed(81))
            m_Camera.Position -= m_Camera.Front * camSpeed * dt;
        if (window->IsKeyScanPressed(4) || window->IsKeyScanPressed(80))
            m_Camera.Position -= m_Camera.Right * camSpeed * dt;
        if (window->IsKeyScanPressed(7) || window->IsKeyScanPressed(79))
            m_Camera.Position += m_Camera.Right * camSpeed * dt;
        if (window->IsKeyScanPressed(44))
            m_Camera.Position.y += camSpeed * dt;
        if (window->IsKeyScanPressed(225) || window->IsKeyScanPressed(224))
            m_Camera.Position.y -= camSpeed * dt;
    }
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

void ThermalPlumeGame::InitializeWindow() {
    auto window = std::make_unique<Window>(1280, 720, "Solstice — Rocket Plume Lab");

    window->SetResizeCallback([this](int w, int h) { HandleWindowResize(w, h); });
    window->SetKeyCallback([this](int key, int sc, int action, int mods) {
        HandleKeyInput(key, sc, action, mods);
    });
    window->SetMouseButtonCallback([this](int btn, int action, int mods) {
        HandleMouseButton(btn, action, mods);
    });
    window->SetCursorPosCallback([this](double dx, double dy) {
        HandleCursorPos(dx, dy);
    });

    SetWindow(std::move(window));
}

void ThermalPlumeGame::HandleWindowResize(int W, int H) {
    m_PendingResize = true;
    m_PendingWidth = W;
    m_PendingHeight = H;
    m_LastResizeEvent = std::chrono::high_resolution_clock::now();
}

void ThermalPlumeGame::HandleKeyInput(int /*Key*/, int Scancode, int Action, int /*Mods*/) {
    if (Action != 1) return; // press only

    // Space: toggle Start / Pause
    if (Scancode == 44) {
        if (m_SimState == SimState::Idle || m_SimState == SimState::Paused) {
            m_SimState = SimState::Running;
        } else {
            m_SimState = SimState::Paused;
        }
    }

    // R: reset
    if (Scancode == 21) {
        ResetSimulation();
    }

    // I: toggle ignition
    if (Scancode == 12) {
        m_Firing = !m_Firing;
    }

    // C: cycle camera mode
    if (Scancode == 6) {
        int next = (static_cast<int>(m_CameraMode) + 1) % 3;
        m_CameraMode = static_cast<CameraMode>(next);
    }

    // F: toggle fullscreen
    if (Scancode == 9) {
        auto* win = GetWindow();
        if (win) {
            m_IsFullscreen = !m_IsFullscreen;
            win->SetFullscreen(m_IsFullscreen);
        }
    }

    // Escape: close
    if (Scancode == 41) {
        RequestClose();
    }
}

void ThermalPlumeGame::HandleMouseButton(int Button, int Action, int /*Mods*/) {
    if (Button == 2 && Action == 1) {
        m_MouseLocked = !m_MouseLocked;
        auto* win = GetWindow();
        if (win) {
            win->SetRelativeMouse(m_MouseLocked);
            win->SetCursorGrab(m_MouseLocked);
            win->ShowCursor(!m_MouseLocked);
        }
        if (m_Renderer) {
            m_Renderer->SetShowCrosshair(m_MouseLocked);
        }
    }
}

void ThermalPlumeGame::HandleCursorPos(double Dx, double Dy) {
    if (!m_MouseLocked) return;
    if (m_CameraMode == CameraMode::FreeFly) {
        m_Camera.ProcessMouseMovement(static_cast<float>(Dx), static_cast<float>(-Dy));
    }
}

// ---------------------------------------------------------------------------
// Scene construction — Arzachel-sculpted geometry + procedural materials
// ---------------------------------------------------------------------------

void ThermalPlumeGame::InitializeScene() {
    Seed seed = m_Seed;

    // --- Materials ---

    // Ground: dark concrete
    auto groundMat = Core::Materials::CreateDefault();
    groundMat.SetAlbedoColor(Vec3(0.18f, 0.18f, 0.2f), 0.85f);
    uint32_t groundMatID = m_MaterialLibrary->AddMaterial(groundMat);

    // Stand: metallic gray
    auto standMat = Core::Materials::CreateMetal(Vec3(0.45f, 0.45f, 0.5f), 0.4f);
    uint32_t standMatID = m_MaterialLibrary->AddMaterial(standMat);

    // Nozzle bell: bright metal
    auto nozzleMat = Core::Materials::CreateMetal(Vec3(0.7f, 0.7f, 0.75f), 0.25f);
    uint32_t nozzleMatID = m_MaterialLibrary->AddMaterial(nozzleMat);

    // Chamber: dark metal
    auto chamberMat = Core::Materials::CreateMetal(Vec3(0.35f, 0.33f, 0.32f), 0.35f);
    uint32_t chamberMatID = m_MaterialLibrary->AddMaterial(chamberMat);

    // Nozzle emissive (updated per frame based on throttle)
    auto emissiveMat = Core::Materials::CreateEmissive(Vec3(1.0f, 0.5f, 0.1f), 0.0f);
    m_NozzleEmissiveMatID = m_MaterialLibrary->AddMaterial(emissiveMat);

    // Confinement wall: translucent glass-like
    auto wallMat = Core::Materials::CreateDefault();
    wallMat.SetAlbedoColor(Vec3(0.3f, 0.4f, 0.5f), 0.1f);
    wallMat.Opacity = 60;
    wallMat.AlphaMode = static_cast<uint8_t>(Core::AlphaMode::Blend);
    wallMat.Flags = Core::MaterialFlag_Transparent;
    uint32_t wallMatID = m_MaterialLibrary->AddMaterial(wallMat);

    // --- Meshes ---

    // Ground plane
    auto groundMesh = MeshFactory::CreatePlane(25.0f, 25.0f);
    uint32_t groundMeshID = m_MeshLibrary->AddMesh(std::move(groundMesh));
    if (auto* m = m_MeshLibrary->GetMesh(groundMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = groundMatID;
    }
    m_Scene.AddObject(groundMeshID, Vec3(0, 0, 0), Quaternion(), Vec3(1, 1, 1), ObjectType_Static);

    // Test stand platform (wide low box)
    auto platformMesh = MeshFactory::CreateCube(1.0f);
    uint32_t platformMeshID = m_MeshLibrary->AddMesh(std::move(platformMesh));
    if (auto* m = m_MeshLibrary->GetMesh(platformMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = standMatID;
    }
    m_Scene.AddObject(platformMeshID, Vec3(0, 0.06f, 0), Quaternion(),
                      Vec3(1.8f, 0.12f, 1.8f), ObjectType_Static);

    // Stand legs — four cylinders at corners
    auto legMesh = MeshFactory::CreateCylinder(0.04f, 1.0f, 12);
    uint32_t legMeshID = m_MeshLibrary->AddMesh(std::move(legMesh));
    if (auto* m = m_MeshLibrary->GetMesh(legMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = standMatID;
    }
    const float legOff = 0.6f;
    const float legY = 0.12f + kStandHeight * 0.5f;
    const Vec3 legScale(1.0f, kStandHeight, 1.0f);
    m_Scene.AddObject(legMeshID, Vec3(-legOff, legY, -legOff), Quaternion(), legScale, ObjectType_Static);
    m_Scene.AddObject(legMeshID, Vec3( legOff, legY, -legOff), Quaternion(), legScale, ObjectType_Static);
    m_Scene.AddObject(legMeshID, Vec3(-legOff, legY,  legOff), Quaternion(), legScale, ObjectType_Static);
    m_Scene.AddObject(legMeshID, Vec3( legOff, legY,  legOff), Quaternion(), legScale, ObjectType_Static);

    // Upper platform (mounting ring)
    m_Scene.AddObject(platformMeshID, Vec3(0, kStandHeight + 0.12f, 0), Quaternion(),
                      Vec3(1.4f, 0.08f, 1.4f), ObjectType_Static);

    // Combustion chamber — cylinder sitting on upper platform
    auto chamberMesh = MeshFactory::CreateCylinder(0.18f, 0.5f, 16);
    uint32_t chamberMeshID = m_MeshLibrary->AddMesh(std::move(chamberMesh));
    if (auto* m = m_MeshLibrary->GetMesh(chamberMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = chamberMatID;
    }
    float chamberY = kStandHeight + 0.16f + 0.25f;
    m_ChamberID = m_Scene.AddObject(chamberMeshID, Vec3(0, chamberY, 0), Quaternion(),
                                    Vec3(1, 1, 1), ObjectType_Static);

    // Nozzle bell (convergent-divergent) — two merged cylinders: throat + flared exit
    // Throat section (narrower, upper)
    auto throatMesh = MeshFactory::CreateCylinder(0.12f, 0.3f, 20);
    uint32_t throatMeshID = m_MeshLibrary->AddMesh(std::move(throatMesh));
    if (auto* m = m_MeshLibrary->GetMesh(throatMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = nozzleMatID;
    }
    float throatY = chamberY - 0.25f - 0.15f;
    m_Scene.AddObject(throatMeshID, Vec3(0, throatY, 0), Quaternion(), Vec3(1, 1, 1), ObjectType_Static);

    // Bell exit (wider, lower) — the divergent section
    auto bellMesh = MeshFactory::CreateCylinder(0.15f, 0.4f, 20);
    uint32_t bellMeshID = m_MeshLibrary->AddMesh(std::move(bellMesh));
    if (auto* m = m_MeshLibrary->GetMesh(bellMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = nozzleMatID;
    }
    float bellY = throatY - 0.15f - 0.2f;
    m_NozzleBellID = m_Scene.AddObject(bellMeshID, Vec3(0, bellY, 0), Quaternion(),
                                       Vec3(1.6f, 1.0f, 1.6f), ObjectType_Static);

    // Nozzle interior glow ring (emissive, visible when firing)
    auto glowMesh = MeshFactory::CreateCylinder(0.10f, 0.08f, 16);
    uint32_t glowMeshID = m_MeshLibrary->AddMesh(std::move(glowMesh));
    if (auto* m = m_MeshLibrary->GetMesh(glowMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = m_NozzleEmissiveMatID;
    }
    float glowY = bellY - 0.2f + 0.04f;
    m_Scene.AddObject(glowMeshID, Vec3(0, glowY, 0), Quaternion(),
                      Vec3(2.0f, 1.0f, 2.0f), ObjectType_Static);

    // Confinement walls (thesis channel) — thin tall slabs, hidden by default
    auto wallMesh = MeshFactory::CreateCube(1.0f);
    uint32_t wallMeshID = m_MeshLibrary->AddMesh(std::move(wallMesh));
    if (auto* m = m_MeshLibrary->GetMesh(wallMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = wallMatID;
    }
    float wallH = kGridNy * kCellSize;
    m_WallLeftID = m_Scene.AddObject(wallMeshID,
        Vec3(-kGridNx * kCellSize * 0.5f - 0.05f, wallH * 0.5f, 0),
        Quaternion(), Vec3(0.02f, wallH, kGridNz * kCellSize), ObjectType_Static);
    m_WallRightID = m_Scene.AddObject(wallMeshID,
        Vec3(kGridNx * kCellSize * 0.5f + 0.05f, wallH * 0.5f, 0),
        Quaternion(), Vec3(0.02f, wallH, kGridNz * kCellSize), ObjectType_Static);

    // Cross-brace pipe between two front legs for visual detail
    auto braceMesh = MeshFactory::CreateCylinder(0.025f, 1.0f, 8);
    uint32_t braceMeshID = m_MeshLibrary->AddMesh(std::move(braceMesh));
    if (auto* m = m_MeshLibrary->GetMesh(braceMeshID)) {
        if (!m->SubMeshes.empty()) m->SubMeshes[0].MaterialID = standMatID;
    }
    float braceY = 1.0f;
    auto braceRot = Quaternion::FromEuler(0.0f, 0.0f, 1.5708f);
    m_Scene.AddObject(braceMeshID, Vec3(0, braceY, legOff), braceRot,
                      Vec3(1.0f, legOff * 2.0f, 1.0f), ObjectType_Static);
    m_Scene.AddObject(braceMeshID, Vec3(0, braceY, -legOff), braceRot,
                      Vec3(1.0f, legOff * 2.0f, 1.0f), ObjectType_Static);
}

// ---------------------------------------------------------------------------
// Fluid simulation
// ---------------------------------------------------------------------------

void ThermalPlumeGame::InitializeFluidSim() {
    m_Fluid = std::make_unique<Physics::FluidSimulation>(
        kGridNx, kGridNy, kGridNz, kCellSize, kCellSize, kCellSize, 1e-5f, 1e-4f);

    // Center grid under nozzle exit
    float gridHalfX = 0.5f * kGridNx * kCellSize;
    float gridHalfZ = 0.5f * kGridNz * kCellSize;
    m_Fluid->SetGridOrigin(Vec3(-gridHalfX, 0.0f, -gridHalfZ));

    auto& tune = m_Fluid->GetTuning();
    tune.useSparseRaymarch = false;
    tune.enableMacCormack = true;
    tune.useSpectralDiffusion = false;
    tune.parallelAnisotropicLinearSolve = true;
    // Keep reprojection active but lighter for better real-time throughput.
    tune.temporalReprojectWeight = 0.12f;

    auto& th = m_Fluid->GetThermal();
    th.enableBoussinesq = true;
    th.buoyancyAxis = 1;
    th.thermalWallAxis = 1;
    th.useLocalizedHotWall = true;

    // Localized hot wall matching nozzle exit footprint
    float nozzleR = m_NozzleDiameter * 0.5f;
    float pad = 0.5f * kCellSize;
    th.hotWallUMin = -nozzleR - pad;
    th.hotWallUMax =  nozzleR + pad;
    th.hotWallVMin = -nozzleR - pad;
    th.hotWallVMax =  nozzleR + pad;

    th.THot = 1.0f;
    th.TCold = 0.0f;
    th.TReference = 0.5f;
    th.buoyancyStrength = m_Gravity;
    th.Prandtl = 0.71f;
    th.timeVaryingForcing = false;

    Physics::PhysicsSystem::Instance().RegisterFluidSimulation(m_Fluid.get());

    ResetSimulation();
}

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------

void ThermalPlumeGame::InitializeParticles() {
    ExhaustParticleConfig cfg;
    cfg.MaxParticles = 5000;
    cfg.SpawnRate = 300.0f;
    cfg.MaxDistance = 80.0f;
    cfg.NozzlePosition = Vec3(0.0f, kNozzleExitY, 0.0f);
    cfg.NozzleRadius = kNozzleRadius;
    cfg.ExhaustVelocity = m_ExhaustVelocity * 0.04f;
    cfg.TurbulenceIntensity = m_TurbulenceIntensity;

    m_ExhaustParticles = std::make_unique<ExhaustParticleSystem>(cfg);
    m_ExhaustParticles->SetFluidSimulation(m_Fluid.get());
    m_ExhaustParticles->SetFiring(false);
    m_ExhaustParticles->SetThrottle(0.0f);
}

// ---------------------------------------------------------------------------
// Lighting
// ---------------------------------------------------------------------------

void ThermalPlumeGame::InitializeLighting() {
    Physics::LightSource keyLight;
    keyLight.Position = Vec3(5.0f, 8.0f, 5.0f);
    keyLight.Color = Vec3(1.0f, 0.95f, 0.9f);
    keyLight.Intensity = 1.2f;
    keyLight.Hue = 0.0f;
    keyLight.Attenuation = 0.01f;
    keyLight.Type = Physics::LightSource::LightType::Point;
    m_Lights.push_back(keyLight);

    Physics::LightSource fillLight;
    fillLight.Position = Vec3(-4.0f, 3.0f, -2.0f);
    fillLight.Color = Vec3(0.4f, 0.5f, 0.7f);
    fillLight.Intensity = 0.6f;
    fillLight.Hue = 0.0f;
    fillLight.Attenuation = 0.02f;
    fillLight.Type = Physics::LightSource::LightType::Point;
    m_Lights.push_back(fillLight);
}

// ---------------------------------------------------------------------------
// Nozzle injection — per-frame forcing of velocity + temperature into fluid grid
// ---------------------------------------------------------------------------

void ThermalPlumeGame::InjectNozzle(float /*Dt*/) {
    if (!m_Fluid || !m_Firing || m_Throttle < 0.01f) return;

    const int nx = m_Fluid->GetNx();
    const int ny = m_Fluid->GetNy();
    const int nz = m_Fluid->GetNz();
    const Vec3 go = m_Fluid->GetGridOrigin();

    // Nozzle exit mapped to grid: top few rows, within nozzle radius
    float nozzleR = m_NozzleDiameter * 0.5f;
    float velMag = m_ExhaustVelocity * m_Throttle * 0.02f;
    float tHot = std::clamp(m_ChamberTemperature / 4000.0f, 0.0f, 1.0f);

    // Inject into the top ~3 cell layers
    int yStart = std::max(0, ny - 4);
    int yEnd = ny - 1;

    for (int z = 1; z < nz - 1; ++z) {
        for (int y = yStart; y <= yEnd; ++y) {
            for (int x = 1; x < nx - 1; ++x) {
                float wx = go.x + (static_cast<float>(x) + 0.5f) * kCellSize;
                float wz = go.z + (static_cast<float>(z) + 0.5f) * kCellSize;
                float dist = std::sqrt(wx * wx + wz * wz);
                if (dist > nozzleR) continue;

                float radialFalloff = 1.0f - (dist / nozzleR);
                radialFalloff *= radialFalloff;

                float pert = m_NoiseDist(m_Rng) * m_TurbulenceIntensity * 0.003f;

                m_Fluid->AddVelocity(x, y, z, pert, -velMag * radialFalloff, pert);
                m_Fluid->SetTemperatureLogical(x, y, z, tHot * radialFalloff);
            }
        }
    }

    // Combustion heat source: a few cells below nozzle exit
    int heatY = std::max(0, yStart - 3);
    for (int z = 1; z < nz - 1; ++z) {
        for (int x = 1; x < nx - 1; ++x) {
            float wx = go.x + (static_cast<float>(x) + 0.5f) * kCellSize;
            float wz = go.z + (static_cast<float>(z) + 0.5f) * kCellSize;
            float dist = std::sqrt(wx * wx + wz * wz);
            if (dist > nozzleR * 1.2f) continue;
            float heatAmount = tHot * m_FuelFlowRate * 0.3f * m_Throttle;
            for (int y = heatY; y < yStart; ++y) {
                m_Fluid->AddDensity(x, y, z, heatAmount * 0.1f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Camera modes
// ---------------------------------------------------------------------------

void ThermalPlumeGame::UpdateCamera(float Dt) {
    switch (m_CameraMode) {
    case CameraMode::FreeFly:
        m_Camera.UseOrthographic = false;
        break;

    case CameraMode::SideOrtho:
        m_Camera.UseOrthographic = true;
        m_Camera.OrthoHalfExtentY = 2.5f;
        m_Camera.Position = Vec3(0.0f, kStandHeight * 0.5f + 0.5f, 5.0f);
        m_Camera.Yaw = -90.0f;
        m_Camera.Pitch = 0.0f;
        break;

    case CameraMode::PlumeChase: {
        m_Camera.UseOrthographic = false;
        m_OrbitAngle += Dt * 0.3f;
        float orbR = 3.5f;
        float orbY = kNozzleExitY - 1.5f;
        m_Camera.Position = Vec3(
            std::cos(m_OrbitAngle) * orbR,
            orbY,
            std::sin(m_OrbitAngle) * orbR);
        // Look at plume center
        Vec3 target(0.0f, kNozzleExitY - 0.8f, 0.0f);
        Vec3 dir = (target - m_Camera.Position).Normalized();
        m_Camera.Yaw = std::atan2(dir.z, dir.x) * 57.2958f;
        m_Camera.Pitch = std::asin(dir.y) * 57.2958f;
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Dynamic emissive material update
// ---------------------------------------------------------------------------

void ThermalPlumeGame::UpdateEmissiveMaterial() {
    float glow = (m_Firing && m_SimState == SimState::Running) ? m_Throttle : 0.0f;
    auto* mat = m_MaterialLibrary->GetMaterial(m_NozzleEmissiveMatID);
    if (mat) {
        mat->SetEmission(Vec3(1.0f, 0.5f + glow * 0.3f, 0.1f), glow);
        if (glow > 0.01f) {
            mat->Flags = Core::MaterialFlag_HasEmission | Core::MaterialFlag_Unlit;
        } else {
            mat->Flags = Core::MaterialFlag_Unlit;
        }
    }
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void ThermalPlumeGame::ResetSimulation() {
    m_SimState = SimState::Idle;
    m_Firing = false;
    m_PhysicsAccumulator = 0.0f;

    if (m_Fluid) {
        m_Fluid->ResetFlowFields();
        m_Fluid->ResetSimulationClock();

        const int nx = m_Fluid->GetNx();
        const int ny = m_Fluid->GetNy();
        const int nz = m_Fluid->GetNz();
        auto& th = m_Fluid->GetThermal();

        std::mt19937 rng{42u};
        std::uniform_real_distribution<float> noise(-0.004f, 0.004f);
        for (int z = 0; z < nz; ++z) {
            for (int y = 0; y < ny; ++y) {
                for (int x = 0; x < nx; ++x) {
                    float fy = static_cast<float>(y) / static_cast<float>(std::max(ny - 1, 1));
                    float base = th.THot + fy * (th.TCold - th.THot);
                    m_Fluid->SetTemperatureLogical(x, y, z, base + noise(rng));
                }
            }
        }
    }

    if (m_ExhaustParticles) {
        m_ExhaustParticles->KillAllParticles();
        m_ExhaustParticles->SetFiring(false);
        m_ExhaustParticles->SetThrottle(0.0f);
    }
}

// ---------------------------------------------------------------------------
// ImGui UI
// ---------------------------------------------------------------------------

void ThermalPlumeGame::RenderUI() {
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Rocket Plume Lab")) {
        ImGui::End();
        return;
    }

    RenderSimControls();
    ImGui::Separator();
    RenderEngineControls();
    ImGui::Separator();
    RenderEnvironmentControls();
    ImGui::Separator();
    RenderVisualizationControls();
    ImGui::Separator();
    RenderStatsPanel();

    ImGui::End();
}

void ThermalPlumeGame::RenderSimControls() {
    ImGui::Text("SIMULATION");

    // State indicator
    switch (m_SimState) {
    case SimState::Idle:
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "STATE: IDLE");
        break;
    case SimState::Running:
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "STATE: RUNNING");
        break;
    case SimState::Paused:
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "STATE: PAUSED");
        break;
    }

    if (ImGui::Button("Start", ImVec2(100, 28))) {
        m_SimState = SimState::Running;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause", ImVec2(100, 28))) {
        if (m_SimState == SimState::Running) {
            m_SimState = SimState::Paused;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(100, 28))) {
        ResetSimulation();
    }

    ImGui::Text("Space=Start/Pause  R=Reset  I=Ignite");
}

void ThermalPlumeGame::RenderEngineControls() {
    ImGui::Text("ENGINE");

    ImGui::SliderFloat("Throttle", &m_Throttle, 0.0f, 1.0f);
    ImGui::SliderFloat("Chamber Temp (K)", &m_ChamberTemperature, 500.0f, 4000.0f, "%.0f");
    ImGui::SliderFloat("Exhaust Vel (m/s)", &m_ExhaustVelocity, 50.0f, 500.0f, "%.0f");
    ImGui::SliderFloat("Nozzle Dia (m)", &m_NozzleDiameter, 0.1f, 0.6f);
    ImGui::SliderFloat("Expansion Ratio", &m_ExpansionRatio, 1.5f, 12.0f);
    ImGui::SliderFloat("Fuel Flow", &m_FuelFlowRate, 0.0f, 1.0f);

    if (m_Firing) {
        if (ImGui::Button("Shutdown Engine", ImVec2(-1, 28))) {
            m_Firing = false;
        }
    } else {
        if (ImGui::Button("Ignite", ImVec2(-1, 28))) {
            m_Firing = true;
        }
    }
}

void ThermalPlumeGame::RenderEnvironmentControls() {
    ImGui::Text("ENVIRONMENT");

    if (ImGui::SliderFloat("Gravity (m/s2)", &m_Gravity, 0.0f, 20.0f)) {
        if (m_Fluid) {
            m_Fluid->GetThermal().buoyancyStrength = m_Gravity;
        }
    }
    ImGui::SliderFloat("Ambient Temp (K)", &m_AmbientTemperature, 200.0f, 500.0f, "%.0f");
    ImGui::SliderFloat("Turbulence", &m_TurbulenceIntensity, 0.0f, 2.0f);
}

void ThermalPlumeGame::RenderVisualizationControls() {
    ImGui::Text("VISUALIZATION");

    const char* modes[] = {"Raymarch (temp)", "Raymarch (schlieren)", "Isosurface (T)", "Velocity lines"};
    ImGui::Combo("Draw Mode", &m_DrawMode, modes, IM_ARRAYSIZE(modes));
    ImGui::SliderFloat("Iso Level", &m_IsoLevel, 0.05f, 0.95f);
    ImGui::SliderInt("Vel Stride", &m_VelStride, 1, 6);
    ImGui::SliderFloat("Vel Scale", &m_VelScale, 0.05f, 1.0f);
    ImGui::Checkbox("Particles", &m_ShowParticles);
    ImGui::SameLine();
    ImGui::Checkbox("Volume", &m_ShowVolume);
    ImGui::SameLine();
    ImGui::Checkbox("Walls", &m_ShowWalls);

    const char* camModes[] = {"Free Fly", "Side Ortho", "Plume Chase"};
    int camIdx = static_cast<int>(m_CameraMode);
    if (ImGui::Combo("Camera", &camIdx, camModes, IM_ARRAYSIZE(camModes))) {
        m_CameraMode = static_cast<CameraMode>(camIdx);
    }
}

void ThermalPlumeGame::RenderStatsPanel() {
    ImGui::Text("STATS");

    float thrust = m_FuelFlowRate * m_ExhaustVelocity * m_Throttle * (m_Firing ? 1.0f : 0.0f);
    ImGui::Text("Thrust: %.1f N", thrust);

    // Scan fluid grid for max temperature and plume height
    float maxTemp = 0.0f;
    float maxPlumeY = 0.0f;
    if (m_Fluid) {
        const auto& T = m_Fluid->GetTemperature();
        const int nx = m_Fluid->GetNx();
        const int ny = m_Fluid->GetNy();
        const int nz = m_Fluid->GetNz();
        const int sx = nx + 2;
        const int sxy = sx * (ny + 2);
        const float threshold = 0.15f;
        for (int z = 1; z <= nz; ++z) {
            for (int y = 1; y <= ny; ++y) {
                for (int x = 1; x <= nx; ++x) {
                    int idx = x + sx * y + sxy * z;
                    if (idx >= 0 && idx < static_cast<int>(T.size())) {
                        float t = T[idx];
                        if (t > maxTemp) maxTemp = t;
                        if (t > threshold) {
                            float worldY = static_cast<float>(y - 1) * kCellSize;
                            if (worldY > maxPlumeY) maxPlumeY = worldY;
                        }
                    }
                }
            }
        }
    }

    ImGui::Text("Max Plume Temp: %.3f", maxTemp);
    ImGui::Text("Plume Height: %.2f m", maxPlumeY);

    float Re = 1000.0f * m_ExhaustVelocity * m_NozzleDiameter / 1.8e-5f;
    ImGui::Text("Re (approx): %.0f", Re * m_Throttle);
    ImGui::Text("FPS: %.1f", GetCurrentFPS());
    ImGui::Text("Grid: %dx%dx%d (%.2fm)", kGridNx, kGridNy, kGridNz, kCellSize);

    if (m_ExhaustParticles) {
        ImGui::Text("Particles: %u / %u",
            m_ExhaustParticles->GetActiveParticleCount(),
            m_ExhaustParticles->GetMaxParticles());
    }
}

} // namespace Solstice::ThermalPlume

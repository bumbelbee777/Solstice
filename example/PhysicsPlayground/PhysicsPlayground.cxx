#include "PhysicsPlayground.hxx"
#include "ObjectSpawner.hxx"
#include "SelectionSystem.hxx"
#include "UIManager.hxx"
#include "TerminalHub.hxx"
#include <UI/UISystem.hxx>
#include <Render/Mesh.hxx>
#include <Core/Material.hxx>
#include <Render/PhysicsBridge.hxx>
#include <Render/Skybox.hxx>
#include <Arzachel/ProceduralTexture.hxx>
#include <Solstice.hxx>
#include <Render/ParticlePresets.hxx>
#include <Physics/PhysicsSystem.hxx>
#include <Physics/RigidBody.hxx>
#include <Physics/ConvexHullFactory.hxx>
#include <Physics/LightSource.hxx>
#include <Physics/Fluid.hxx>
#include <Game/InputManager.hxx>
#include <Game/ScriptManager.hxx>
#include <Core/Async.hxx>
#include <Core/Audio.hxx>
#include <Scripting/BytecodeVM.hxx>
#include <Scripting/Compiler.hxx>
#include <Scripting/ScriptBindings.hxx>
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <random>

#include <Arzachel/MeshFactory.hxx>

// Key constants
static constexpr int KEY_ESCAPE = 27;
static constexpr int MOD_CTRL = 0x0040;
static constexpr int MOD_ALT  = 0x0100;

using namespace Solstice;
using namespace Solstice::Render;
using namespace Solstice::UI;
using namespace Solstice::Math;

namespace Solstice::PhysicsPlayground {

PhysicsPlayground::PhysicsPlayground() {
}

PhysicsPlayground::~PhysicsPlayground() {
    this->Shutdown();
}

// Run() method removed - using inherited GameBase::Run()

void PhysicsPlayground::Initialize() {
    // Initialize unified engine systems
    Solstice::Initialize();

    // Initialize window in fullscreen
    InitializeWindow();

    std::cout << "=== Solstice Engine - Physics Playground ===" << std::endl;
    std::cout << "Note: Using ReactPhysics3D for physics simulation" << std::endl;

    // Test scripting (optional)
    Solstice::Scripting::BytecodeVM vm;
    Solstice::Scripting::RegisterScriptBindings(vm, nullptr);

    Solstice::Scripting::Compiler compiler;
    std::string script = R"(
        @Entry {
            print("Starting Moonwalk Script...");
            let x = 10;
            let i = 0;
            while (i < 3) {
                print("Loop iteration", i);
                i = i + 1;
            }
            if (x == 10) {
                print("X is 10!");
            }
            print("Script finished.");
        }
    )";

    try {
        auto prog = compiler.Compile(script);
        vm.LoadProgram(prog);
        vm.Run();
    } catch (const std::exception& e) {
        std::cerr << "Script Error: " << e.what() << std::endl;
    }

    std::cout << "Controls:" << std::endl;
    std::cout << "  E - Pick up/Drop object" << std::endl;
    std::cout << "  Right Mouse - Lock camera" << std::endl;
    std::cout << "  WASD - Move camera (when locked)" << std::endl;
    std::cout << "  F - Toggle fullscreen/maximized" << std::endl;
    std::cout << std::endl;

    // Create renderer
    SIMPLE_LOG("Creating SoftwareRenderer...");
    auto* window = GetWindow();
    if (!window) {
        SIMPLE_LOG("ERROR: Window not set!");
        return;
    }
    auto fbSize = window->GetFramebufferSize();
    m_Renderer = std::make_unique<SoftwareRenderer>(fbSize.first, fbSize.second, 16, window->NativeWindow());
    m_Renderer->SetWireframe(false);
    m_Renderer->SetPhysicsRegistry(&m_Registry);
    SIMPLE_LOG("Renderer created");

    // Initialize UI System
    SIMPLE_LOG("Initializing UISystem...");
    UISystem::Instance().Initialize(m_Window->NativeWindow());
    SIMPLE_LOG("UISystem initialized");

    // Share ImGui context
    void* imguiContext = UISystem::Instance().GetImGuiContext();
    if (imguiContext) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
        SIMPLE_LOG("ImGui context shared from DLL to executable");
    }

    // Create scene and libraries
    m_MeshLibrary = std::make_unique<Render::MeshLibrary>();
    m_MaterialLibrary = std::make_unique<Core::MaterialLibrary>();
    m_Scene.SetMeshLibrary(m_MeshLibrary.get());
    m_Scene.SetMaterialLibrary(m_MaterialLibrary.get());

    // Initialize material presets
    InitializeMaterialPresets();

    // Store gray material ID for use in CreateInitialObjects (use a default material)
    uint32_t GrayMat = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(GrayMat)->SetAlbedoColor(Vec3(0.5f, 0.5f, 0.5f), 0.5f);
    m_GrayMaterialID = GrayMat;

    // Create ground material (darker concrete-like material)
    // Will be assigned texture in CreateInitialObjects after textures are generated
    m_GroundMaterialID = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    Core::Material* groundMat = m_MaterialLibrary->GetMaterial(m_GroundMaterialID);
    groundMat->SetAlbedoColor(Vec3(0.3f, 0.3f, 0.35f), 0.7f); // Dark gray, rough (concrete-like)

    // Initialize skybox and procedural textures
    InitializeSkybox();
    InitializeProceduralTextures();

    // Initialize Physics
    SIMPLE_LOG("Initializing Physics System...");
    Physics::PhysicsSystem::Instance().Start(m_Registry);

    // Create initial objects
    CreateInitialObjects();

    // Initialize systems
    m_ObjectSpawner = std::make_unique<ObjectSpawner>(m_Scene, *m_MeshLibrary, *m_MaterialLibrary, m_Registry);
    m_SelectionSystem = std::make_unique<SelectionSystem>(*m_Renderer);
    m_UIManager = std::make_unique<UIManager>(*m_ObjectSpawner, m_Camera);

    // Set spawn callback
    m_UIManager->SetSpawnCallback([this](ObjectType type) {
        SpawnObjectFromUI(type);
    });

    // Initialize InputManager
    m_InputManager = std::make_unique<Game::InputManager>();
    if (m_Window) {
        m_InputManager->Update(m_Window.get());
    }

    // Initialize ScriptManager
    m_ScriptManager = std::make_unique<Game::ScriptManager>();
    if (!m_ScriptManager->Initialize("scripts", &m_Registry, &m_Scene, &Physics::PhysicsSystem::Instance(), &m_Camera)) {
        SIMPLE_LOG("WARNING: ScriptManager initialization failed - scripts may not work");
    }

    // Initialize TerminalHub
    m_TerminalHub = std::make_unique<TerminalHub>(m_Scene, *m_MeshLibrary, *m_MaterialLibrary, m_Registry, m_Skybox.get(), m_Camera);

    // Set terminal callbacks
    m_TerminalHub->SetParticleSpawnCallback([this](Render::ParticlePresetType type) {
        // Spawn particle system at camera position
        try {
            auto particleSystem = Render::ParticlePresets::CreateParticleSystem(type, Render::ParticlePresetConfig());
            if (particleSystem) {
                particleSystem->Initialize(3000, "vs_particle.bin", "fs_particle.bin");
                m_ParticleSystems.push_back(std::move(particleSystem));
                SIMPLE_LOG("Spawned particle system of type " + std::to_string(static_cast<int>(type)));
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Failed to spawn particle system: " + std::string(e.what()));
        }
    });

    m_TerminalHub->SetFluidSpawnCallback([this](const Math::Vec3& position) {
        SpawnFluidContainer(position);
    });

    m_TerminalHub->SetScriptExecuteCallback([this](const std::string& script, std::string& error) -> bool {
        try {
            Scripting::Compiler compiler;
            auto prog = compiler.Compile(script);

            // Create temporary VM for execution
            Scripting::BytecodeVM vm;
            Scripting::RegisterScriptBindings(vm, &m_Registry, &m_Scene, &Physics::PhysicsSystem::Instance(), &m_Camera);
            vm.LoadProgram(prog);
            vm.Run();

            error = "Script executed successfully!";
            return true;
        } catch (const std::exception& e) {
            error = std::string("Error: ") + e.what();
            return false;
        }
    });

    // Initialize camera
    m_Camera.Position = Vec3(0, 3, 8);
    m_Camera.Up = Vec3(0, 1, 0);
    m_Camera.Zoom = 45.0f;
    m_Camera.Yaw = -90.0f;
    m_Camera.ProcessMouseMovement(0, 0);

    // Create sun light source for raytracing
    Physics::LightSource sunLight(
        Math::Vec3(0.5f, 1.0f, -0.5f).Normalized(), // Direction (sun coming from upper-right)
        Math::Vec3(1.0f, 0.95f, 0.9f), // Warm white color
        1.5f, // Intensity
        0.0f, // Hue
        0.001f // Very low attenuation (directional light)
    );
    m_Lights.push_back(sunLight);
    SIMPLE_LOG("Created sun light source for raytracing");

    // Initialize mouse position
    m_MouseX = window->GetFramebufferSize().first * 0.5f;
    m_MouseY = window->GetFramebufferSize().second * 0.5f;
}

void PhysicsPlayground::Shutdown() {
    // Shutdown unified engine systems (handles UISystem, PhysicsSystem, Audio, JobSystem)
    Solstice::Shutdown();
}

void PhysicsPlayground::InitializeWindow() {
    // Create window in fullscreen mode
    auto window = std::make_unique<Window>(1280, 720, "Solstice Engine - Physics Playground");
    window->SetFullscreen(true);
    m_IsFullscreen = true;

    // Set up callbacks
    window->SetResizeCallback([this](int w, int h) {
        HandleWindowResize(w, h);
    });

    window->SetKeyCallback([this](int key, int scancode, int action, int mods) {
        HandleKeyInput(key, scancode, action, mods);
    });

    window->SetMouseButtonCallback([this](int button, int action, int mods) {
        HandleMouseButton(button, action, mods);
    });

    window->SetCursorPosCallback([this](double dx, double dy) {
        HandleCursorPos(dx, dy);
    });

    // Set window via GameBase
    SetWindow(std::move(window));
}

void PhysicsPlayground::HandleInput() {
    // Update InputManager
    if (m_InputManager && m_Window) {
        m_InputManager->Update(m_Window.get());
    }

    // Input is also handled through callbacks (HandleKeyInput, HandleMouseButton, HandleCursorPos)
    // This method can be used for any non-callback input processing if needed
}

void PhysicsPlayground::HandleWindowResize(int w, int h) {
    m_PendingResize = true;
    m_PendingWidth = w;
    m_PendingHeight = h;
    m_LastResizeEvent = std::chrono::high_resolution_clock::now();
}

void PhysicsPlayground::HandleKeyInput(int key, int scancode, int action, int mods) {
    if (action == 1) { // Key press
        auto* window = GetWindow();
        if (!window) return;

        // F - Toggle fullscreen/maximized
        if (key == 'f' || key == 'F') {
            if (m_IsFullscreen) {
                window->SetFullscreen(false);
                window->SetMaximized(true);
                window->SetResizable(false);
                m_IsFullscreen = false;
                m_IsMaximized = true;
                auto fb = window->GetFramebufferSize();
                m_PendingResize = true;
                m_PendingWidth = fb.first;
                m_PendingHeight = fb.second;
                m_LastResizeEvent = std::chrono::high_resolution_clock::now();
            } else {
                window->SetFullscreen(true);
                window->SetResizable(true);
                m_IsFullscreen = true;
                m_IsMaximized = false;
                auto fb = window->GetFramebufferSize();
                m_PendingResize = true;
                m_PendingWidth = fb.first;
                m_PendingHeight = fb.second;
                m_LastResizeEvent = std::chrono::high_resolution_clock::now();
            }
        }

        // ESC - Release mouse lock
        if (key == KEY_ESCAPE) {
            if (m_MouseLocked) {
                m_MouseLocked = false;
                window->SetRelativeMouse(false);
                window->SetCursorGrab(false);
                window->ShowCursor(true);
                m_Renderer->SetShowCrosshair(false);
            }
        }

        // E - Pick up/Drop
        if (key == 'e' || key == 'E') {
            if (m_GrabbedEntity == 0) {
                // Find closest object in front of camera
                float minDist = 10.0f;
                ECS::EntityId closest = 0;

                m_Registry.ForEach<Physics::RigidBody>([&](ECS::EntityId eid, const Physics::RigidBody& rb) {
                    // Fix: Exclude static objects (like the infinite plane) from being grabbed
                    if (rb.IsStatic) return;

                    Vec3 toObj = rb.Position - m_Camera.Position;
                    float dist = toObj.Magnitude();
                    Vec3 forward = m_Camera.Front;
                    float dot = toObj.Normalized().Dot(forward);

                    if (dot > 0.7f && dist < minDist) {
                        minDist = dist;
                        closest = eid;
                    }
                });

                if (closest != 0) {
                    m_GrabbedEntity = closest;
                    m_GrabbedDistance = minDist;
                    m_GrabbedOffsets.clear();

                    // If grabbed object is selected, grab everything else selected too
                    auto* rbGrabbed = &m_Registry.Get<Physics::RigidBody>(m_GrabbedEntity);
                    if (m_SelectionSystem && m_SelectionSystem->IsSelected(rbGrabbed->RenderObjectID)) {
                        Vec3 basePos = rbGrabbed->Position;
                        m_Registry.ForEach<Physics::RigidBody>([&](ECS::EntityId eid, Physics::RigidBody& rb) {
                            if (!rb.IsStatic && m_SelectionSystem->IsSelected(rb.RenderObjectID)) {
                                m_GrabbedOffsets[eid] = rb.Position - basePos;
                                rb.IsGrabbed = true; // Use the IsGrabbed flag we found in RigidBody.hxx
                            }
                        });
                    } else {
                        m_GrabbedOffsets[m_GrabbedEntity] = Vec3(0, 0, 0);
                        rbGrabbed->IsGrabbed = true;
                    }
                    SIMPLE_LOG("Grabbed " + std::to_string(m_GrabbedOffsets.size()) + " objects!");
                }
            } else {
                // Drop all objects
                for (auto& [eid, offset] : m_GrabbedOffsets) {
                    auto* rb = m_Registry.Has<Physics::RigidBody>(eid) ? &m_Registry.Get<Physics::RigidBody>(eid) : nullptr;
                    if (rb) {
                        rb->IsGrabbed = false;
                        const float impulseStrength = 3.0f * rb->Mass;
                        rb->ApplyImpulse(m_Camera.Front * impulseStrength);
                    }
                }
                m_GrabbedEntity = 0;
                m_GrabbedOffsets.clear();
                SIMPLE_LOG("Dropped objects");
            }
        }

        // Ctrl+Alt - Lock mouse
        if ((mods & (MOD_CTRL | MOD_ALT)) == (MOD_CTRL | MOD_ALT)) {
            auto* window = GetWindow();
            if (window) {
                m_MouseLocked = true;
                window->SetRelativeMouse(true);
                window->SetCursorGrab(true);
                window->ShowCursor(false);
                m_Renderer->SetShowCrosshair(true);
            }
        }

        // P - Play test sound
        if (key == 'p' || key == 'P') {
            SIMPLE_LOG("Playing test sound at (0,0,0)...");
            Core::Audio::AudioManager::Instance().PlaySound3D("assets/test.wav", {0, 0, 0}, 50.0f);
        }
    }
}

void PhysicsPlayground::HandleMouseButton(int button, int action, int mods) {
    auto* window = GetWindow();
    if (!window) return;

    if (button == 2 && action == 1) { // Right mouse button
        m_MouseLocked = !m_MouseLocked;
        window->SetRelativeMouse(m_MouseLocked);
        window->SetCursorGrab(m_MouseLocked);
        window->ShowCursor(!m_MouseLocked);
        m_Renderer->SetShowCrosshair(m_MouseLocked);
    }

    if (button == 1 && action == 1 && !m_MouseLocked && m_SelectionSystem) { // Left mouse button, not locked
        // Perform object picking
        auto fbSize = window->GetFramebufferSize();
        Render::SceneObjectID picked = m_SelectionSystem->PickObject(m_MouseX, m_MouseY, m_Camera, m_Scene, fbSize.first, fbSize.second);

        if (picked != Render::InvalidObjectID) {
            // Toggle selection
            m_SelectionSystem->ToggleSelection(picked);
        } else {
            // Clicked on nothing - clear selection
            m_SelectionSystem->ClearSelection();
        }
    }
}

void PhysicsPlayground::HandleCursorPos(double dx, double dy) {
    auto* window = GetWindow();
    if (!window) return;

    if (m_MouseLocked) {
        m_Camera.ProcessMouseMovement(static_cast<float>(dx), static_cast<float>(-dy));
        // Update InputManager mouse delta
        if (m_InputManager) {
            m_InputManager->UpdateMouseDelta(static_cast<float>(dx), static_cast<float>(-dy));
        }
    } else {
        // Update mouse position for hover detection
        auto fbSize = window->GetFramebufferSize();
        m_MouseX = static_cast<float>(dx);
        m_MouseY = static_cast<float>(dy);

        // Update InputManager mouse position
        if (m_InputManager) {
            m_InputManager->SetMousePosition(m_MouseX, m_MouseY);
        }

        // Update hover object
        if (m_SelectionSystem) {
            Render::SceneObjectID hovered = m_SelectionSystem->PickObject(m_MouseX, m_MouseY, m_Camera, m_Scene, fbSize.first, fbSize.second);
            m_SelectionSystem->SetHoveredObject(hovered);
        }
    }
}

void PhysicsPlayground::InitializeMaterialPresets() {
    using namespace Core::Materials;
    using namespace Solstice::Math;

    // Metallic materials
    m_MaterialPresets.ChromeMat = m_MaterialLibrary->AddMaterial(
        CreateMetal(Vec3(0.9f, 0.9f, 0.95f), 0.1f));

    m_MaterialPresets.GoldMat = m_MaterialLibrary->AddMaterial(
        CreateMetal(Vec3(1.0f, 0.84f, 0.0f), 0.2f));

    m_MaterialPresets.SilverMat = m_MaterialLibrary->AddMaterial(
        CreateMetal(Vec3(0.95f, 0.95f, 0.97f), 0.15f));

    m_MaterialPresets.CopperMat = m_MaterialLibrary->AddMaterial(
        CreateMetal(Vec3(0.72f, 0.45f, 0.20f), 0.25f));

    // Glass materials
    m_MaterialPresets.ClearGlassMat = m_MaterialLibrary->AddMaterial(
        CreateGlass(Vec3(1.0f, 1.0f, 1.0f), 0.05f, 0.7f));

    m_MaterialPresets.TintedGlassMat = m_MaterialLibrary->AddMaterial(
        CreateGlass(Vec3(0.8f, 0.9f, 1.0f), 0.05f, 0.6f));

    m_MaterialPresets.FrostedGlassMat = m_MaterialLibrary->AddMaterial(
        CreateGlass(Vec3(1.0f, 1.0f, 1.0f), 0.3f, 0.8f));

    // Textured materials (will have procedural textures applied later)
    m_MaterialPresets.CheckerboardMat = m_MaterialLibrary->AddMaterial(
        CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MaterialPresets.CheckerboardMat)->SetAlbedoColor(
        Vec3(0.9f, 0.9f, 0.9f), 0.4f);

    m_MaterialPresets.NoiseMat = m_MaterialLibrary->AddMaterial(
        CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MaterialPresets.NoiseMat)->SetAlbedoColor(
        Vec3(0.7f, 0.7f, 0.7f), 0.5f);

    m_MaterialPresets.StripesMat = m_MaterialLibrary->AddMaterial(
        CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MaterialPresets.StripesMat)->SetAlbedoColor(
        Vec3(0.8f, 0.8f, 0.8f), 0.3f);

    // Standard colored materials
    m_MaterialPresets.RedMat = m_MaterialLibrary->AddMaterial(CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MaterialPresets.RedMat)->SetAlbedoColor(Vec3(1.0f, 0.2f, 0.2f), 0.3f);

    m_MaterialPresets.GreenMat = m_MaterialLibrary->AddMaterial(CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MaterialPresets.GreenMat)->SetAlbedoColor(Vec3(0.2f, 1.0f, 0.2f), 0.3f);

    m_MaterialPresets.BlueMat = m_MaterialLibrary->AddMaterial(CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MaterialPresets.BlueMat)->SetAlbedoColor(Vec3(0.2f, 0.2f, 1.0f), 0.3f);

    SIMPLE_LOG("PhysicsPlayground: Initialized material presets");
}

void PhysicsPlayground::InitializeSkybox() {
    m_Skybox = std::make_unique<Render::Skybox>();
    m_Skybox->Initialize(512); // 512x512 cubemap faces

    // Set skybox in renderer
    if (m_Renderer) {
        m_Renderer->SetSkybox(m_Skybox.get());
    }

    SIMPLE_LOG("PhysicsPlayground: Initialized skybox");
}

void PhysicsPlayground::InitializeProceduralTextures() {
    using namespace Render;

    // Generate various procedural textures
    m_ProceduralTextures.push_back(
        Arzachel::ProceduralTexture::UploadToGPU(
            Arzachel::ProceduralTexture::GenerateNoise(512, 4, 4.0f, Arzachel::Seed(12345))));

    m_ProceduralTextures.push_back(
        Arzachel::ProceduralTexture::UploadToGPU(
            Arzachel::ProceduralTexture::GenerateCheckerboard(512, 64,
                Math::Vec3(0.9f, 0.9f, 0.9f), Math::Vec3(0.1f, 0.1f, 0.1f))));

    m_ProceduralTextures.push_back(
        Arzachel::ProceduralTexture::UploadToGPU(
            Arzachel::ProceduralTexture::GenerateStripes(512, 32, true,
                Math::Vec3(0.8f, 0.8f, 0.8f), Math::Vec3(0.2f, 0.2f, 0.2f))));

    m_ProceduralTextures.push_back(
        Arzachel::ProceduralTexture::UploadToGPU(
            Arzachel::ProceduralTexture::GenerateRoughness(512, 0.1f, 0.9f, Arzachel::Seed(54321))));

    // Generate concrete/stone texture for ground (higher scale for concrete-like pattern)
    m_ProceduralTextures.push_back(
        Arzachel::ProceduralTexture::UploadToGPU(
            Arzachel::ProceduralTexture::GenerateNoise(512, 4, 8.0f, Arzachel::Seed(9999))));

    SIMPLE_LOG("PhysicsPlayground: Generated " + std::to_string(m_ProceduralTextures.size()) + " procedural textures");

    // Register textures with renderer and assign to materials
    if (m_Renderer) {
        // Register textures and get their indices
        uint16_t noiseTexIndex = m_Renderer->GetTextureRegistry().Register(m_ProceduralTextures[0]);
        uint16_t checkerboardTexIndex = m_Renderer->GetTextureRegistry().Register(m_ProceduralTextures[1]);
        uint16_t stripesTexIndex = m_Renderer->GetTextureRegistry().Register(m_ProceduralTextures[2]);
        // Roughness texture (index 3) is not used as albedo, so we don't assign it to a material
        uint16_t concreteTexIndex = m_Renderer->GetTextureRegistry().Register(m_ProceduralTextures[4]); // Index 4 is concrete texture

        // Assign textures to materials
        if (m_MaterialLibrary) {
            // Assign noise texture to NoiseMat
            Core::Material* noiseMat = m_MaterialLibrary->GetMaterial(m_MaterialPresets.NoiseMat);
            if (noiseMat) {
                noiseMat->AlbedoTexIndex = noiseTexIndex;
                SIMPLE_LOG("PhysicsPlayground: Assigned noise texture (index " + std::to_string(noiseTexIndex) + ") to NoiseMat");
            }

            // Assign checkerboard texture to CheckerboardMat
            Core::Material* checkerboardMat = m_MaterialLibrary->GetMaterial(m_MaterialPresets.CheckerboardMat);
            if (checkerboardMat) {
                checkerboardMat->AlbedoTexIndex = checkerboardTexIndex;
                SIMPLE_LOG("PhysicsPlayground: Assigned checkerboard texture (index " + std::to_string(checkerboardTexIndex) + ") to CheckerboardMat");
            }

            // Assign stripes texture to StripesMat
            Core::Material* stripesMat = m_MaterialLibrary->GetMaterial(m_MaterialPresets.StripesMat);
            if (stripesMat) {
                stripesMat->AlbedoTexIndex = stripesTexIndex;
                SIMPLE_LOG("PhysicsPlayground: Assigned stripes texture (index " + std::to_string(stripesTexIndex) + ") to StripesMat");
            }

            // Assign concrete texture to ground material
            Core::Material* groundMat = m_MaterialLibrary->GetMaterial(m_GroundMaterialID);
            if (groundMat) {
                groundMat->AlbedoTexIndex = concreteTexIndex;
                SIMPLE_LOG("PhysicsPlayground: Assigned concrete texture (index " + std::to_string(concreteTexIndex) + ") to ground material");
            }
        }
    }
}

void PhysicsPlayground::CreateInitialObjects() {
    // Create ObjectSpawner temporarily for initial objects
    ObjectSpawner spawner(m_Scene, *m_MeshLibrary, *m_MaterialLibrary, m_Registry);

    // Create plane - static ground
    uint32_t PlaneMeshID = m_MeshLibrary->AddMesh(Solstice::Arzachel::MeshFactory::CreatePlane(1000.0f, 1000.0f));
    auto PlaneObjID = m_Scene.AddObject(PlaneMeshID, Vec3(0, 0, 0), Quaternion(), Vec3(1, 1, 1), ObjectType_Static);

    // Apply ground material to plane (darker concrete material with texture)
    Mesh* PlaneMesh = m_MeshLibrary->GetMesh(PlaneMeshID);
    if (PlaneMesh && !PlaneMesh->SubMeshes.empty()) {
        PlaneMesh->SubMeshes[0].MaterialID = m_GroundMaterialID;
    }

    // Create static ground RigidBody
    auto groundEntityID = m_Registry.Create();
    auto& groundRB = m_Registry.Add<Physics::RigidBody>(groundEntityID);
    groundRB.Position = Vec3(0, -0.1f, 0);
    groundRB.Rotation = Quaternion();
    groundRB.IsStatic = true;
    groundRB.SetMass(0.0f);
    groundRB.Type = Physics::ColliderType::Box;
    groundRB.HalfExtents = Vec3(500.0f, 0.1f, 500.0f);
    groundRB.Friction = 0.6f;
    groundRB.Restitution = 0.0f;
    groundRB.RenderObjectID = PlaneObjID;

    // Force sync to create ground body
    Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();

    // Create some initial dynamic objects with varied materials
    // Mix of metallic, glassy, and textured materials
    m_DynamicObjects.push_back(spawner.SpawnObject(ObjectType::Sphere, Vec3(-6, 8, -8), m_MaterialPresets.ChromeMat));
    m_DynamicObjects.push_back(spawner.SpawnObject(ObjectType::Cube, Vec3(0, 10, -8), m_MaterialPresets.ClearGlassMat));
    m_DynamicObjects.push_back(spawner.SpawnObject(ObjectType::Tetrahedron, Vec3(6, 12, -8), m_MaterialPresets.GoldMat));
    m_DynamicObjects.push_back(spawner.SpawnObject(ObjectType::Cylinder, Vec3(-3, 14, -8), m_MaterialPresets.TintedGlassMat));

    // Stack of boxes with varied materials
    uint32_t boxMaterials[] = {
        m_MaterialPresets.SilverMat,
        m_MaterialPresets.CheckerboardMat,
        m_MaterialPresets.FrostedGlassMat,
        m_MaterialPresets.CopperMat,
        m_MaterialPresets.NoiseMat
    };
    for (int i = 0; i < 5; ++i) {
        float yPos = 2.0f + i * 1.1f;
        uint32_t matID = boxMaterials[i % 5];
        m_DynamicObjects.push_back(spawner.SpawnObject(ObjectType::Cube, Vec3(3, yPos, -8), matID));
    }

    // Add more objects with different materials
    m_DynamicObjects.push_back(spawner.SpawnObject(ObjectType::Sphere, Vec3(-9, 6, -8), m_MaterialPresets.StripesMat));
    m_DynamicObjects.push_back(spawner.SpawnObject(ObjectType::Icosphere, Vec3(9, 6, -8), m_MaterialPresets.SilverMat));

    SIMPLE_LOG("Physics playground created with " + std::to_string(m_DynamicObjects.size()) + " dynamic objects");
}

void PhysicsPlayground::Update(float DeltaTime) {
    auto* window = GetWindow();
    if (!window) return;

    // Update InputManager
    if (m_InputManager) {
        m_InputManager->Update(window);
    }

    // Camera movement (use InputManager if available, fallback to window)
    if (m_MouseLocked) {
        bool moveForward = false, moveBackward = false, moveLeft = false, moveRight = false, moveUp = false, moveDown = false;

        if (m_InputManager) {
            moveForward = m_InputManager->IsActionPressed(Game::InputAction::MoveForward);
            moveBackward = m_InputManager->IsActionPressed(Game::InputAction::MoveBackward);
            moveLeft = m_InputManager->IsActionPressed(Game::InputAction::MoveLeft);
            moveRight = m_InputManager->IsActionPressed(Game::InputAction::MoveRight);
            moveUp = m_InputManager->IsKeyPressed(44); // Space
            moveDown = m_InputManager->IsKeyPressed(225) || m_InputManager->IsKeyPressed(224); // Shift or Ctrl
        } else {
            // Fallback to window
            moveForward = window->IsKeyScanPressed(26) || window->IsKeyScanPressed(82);
            moveBackward = window->IsKeyScanPressed(22) || window->IsKeyScanPressed(81);
            moveLeft = window->IsKeyScanPressed(4) || window->IsKeyScanPressed(80);
            moveRight = window->IsKeyScanPressed(7) || window->IsKeyScanPressed(79);
            moveUp = window->IsKeyScanPressed(44);
            moveDown = window->IsKeyScanPressed(225) || window->IsKeyScanPressed(224);
        }

        if (moveForward) {
            m_Camera.Position += m_Camera.Front * m_CameraSpeed * DeltaTime;
        }
        if (moveBackward) {
            m_Camera.Position -= m_Camera.Front * m_CameraSpeed * DeltaTime;
        }
        if (moveLeft) {
            m_Camera.Position -= m_Camera.Right * m_CameraSpeed * DeltaTime;
        }
        if (moveRight) {
            m_Camera.Position += m_Camera.Right * m_CameraSpeed * DeltaTime;
        }
        if (moveUp) {
            m_Camera.Position.y += m_CameraSpeed * DeltaTime;
        }
        if (moveDown) {
            m_Camera.Position.y -= m_CameraSpeed * DeltaTime;
        }
    }

    // Update Audio Listener
    Core::Audio::Listener listener;
    listener.Position = m_Camera.Position;
    listener.Forward = m_Camera.Front;
    listener.Up = m_Camera.Up;
    Core::Audio::AudioManager::Instance().SetListener(listener);

    // Handle resize
    if (m_PendingResize) {
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration<float, std::milli>(now - m_LastResizeEvent).count() > 120.0f) {
            m_PendingWidth = std::max(m_PendingWidth, 800);
            m_PendingHeight = std::max(m_PendingHeight, 600);
            m_Renderer->Resize(m_PendingWidth, m_PendingHeight);
            m_PendingResize = false;
        }
    }

    // Update grabbed object (pass DeltaTime for smooth interpolation)
    UpdateGrabbedObject(DeltaTime);

    // Physics update
    m_PhysicsAccumulator += DeltaTime;
    const float physicsStep = 1.0f / 60.0f;
    const int maxSubSteps = 5;
    int steps = 0;

    while (m_PhysicsAccumulator >= physicsStep && steps < maxSubSteps) {
        Physics::PhysicsSystem::Instance().Update(physicsStep);
        m_PhysicsAccumulator -= physicsStep;
        steps++;
    }

    // Sync physics to scene
    SyncPhysicsToScene();

    // Update TerminalHub
    if (m_TerminalHub && m_InputManager) {
        m_TerminalHub->Update(DeltaTime, m_Camera, *m_InputManager);
    }

    // Update particle systems
    for (auto& particleSystem : m_ParticleSystems) {
        if (particleSystem) {
            particleSystem->Update(DeltaTime, m_Camera.Position);
        }
    }

    // Update fluid simulations
    for (auto& fluidSim : m_FluidSimulations) {
        if (fluidSim) {
            fluidSim->Step(DeltaTime);
        }
    }

    // Update ScriptManager
    if (m_ScriptManager) {
        m_ScriptManager->Update(DeltaTime);
    }
}

void PhysicsPlayground::UpdateGrabbedObject(float deltaTime) {
    if (m_GrabbedEntity == 0 || m_GrabbedOffsets.empty()) return;

    Vec3 baseTargetPos = m_Camera.Position + m_Camera.Front * m_GrabbedDistance;
    // Increased lerp speed for responsiveness, but pure velocity control will be smoother
    const float lerpSpeed = 25.0f; 

    for (auto& [eid, offset] : m_GrabbedOffsets) {
        auto* rb = m_Registry.Has<Physics::RigidBody>(eid) ? &m_Registry.Get<Physics::RigidBody>(eid) : nullptr;
        if (!rb || rb->IsStatic) continue;

        Vec3 targetPos = baseTargetPos + offset;
        Vec3 currentPos = rb->Position;
        Vec3 dir = targetPos - currentPos;
        float dist = dir.Magnitude();

        if (dist > 0.001f) {
            // Velocity-based control only
            // P-controller for velocity: V_target = Error * Gain
            Vec3 targetVel = dir * lerpSpeed;
            
            // Limit max velocity to prevent instability
            float maxGrabVel = 20.0f;
            if (targetVel.Magnitude() > maxGrabVel) {
                targetVel = targetVel.Normalized() * maxGrabVel;
            }
            
            // Apply a critical damping-like effect by blending current velocity
            // This prevents overshoot/oscillations aka "stuttering"
            rb->Velocity = targetVel;
            
            // Do NOT manually set Position - let the physics engine integrate it
            // rb->Position = currentPos + moveVel * deltaTime; <--- REMOVED
        } else {
            rb->Velocity = Vec3(0, 0, 0);
             // Only snap if VERY close and effectively stopped
            if (dist < 0.01f) {
                 rb->Position = targetPos;
            }
        }

        // Heavy damping on rotation to keep object stable while held
        rb->AngularVelocity = rb->AngularVelocity * 0.5f;

        // No manual Scene sync needed here, the physics system will do it in SyncPhysicsToScene
    }
}

void PhysicsPlayground::SyncPhysicsToScene() {
    m_Registry.ForEach<Physics::RigidBody>([&](ECS::EntityId eid, Physics::RigidBody& rb) {
        if (rb.RenderObjectID != Render::InvalidObjectID) {
            m_Scene.SetPosition(rb.RenderObjectID, rb.Position);
            m_Scene.SetRotation(rb.RenderObjectID, rb.Rotation);
        }
    });
}

void PhysicsPlayground::Render() {
    // Start new ImGui frame
    UISystem::Instance().NewFrame();

    // Render scene with sun light for raytracing
    m_Renderer->Clear(Vec4(0.4f, 0.6f, 0.9f, 1.0f));
    m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);

    // Render particle systems
    auto* window = GetWindow();
    if (window) {
        auto fbSize = window->GetFramebufferSize();
        Math::Matrix4 viewMatrix = m_Camera.GetViewMatrix();
        Math::Matrix4 projMatrix = Math::Matrix4::Perspective(
            m_Camera.GetZoom() * 0.0174533f,
            static_cast<float>(fbSize.first) / static_cast<float>(fbSize.second),
            0.1f,
            1000.0f
        );
        Math::Matrix4 viewProj = projMatrix * viewMatrix;

        // Use view ID 1 for particles (scene is typically 0)
        bgfx::ViewId particleViewId = 1;
        for (auto& particleSystem : m_ParticleSystems) {
            if (particleSystem) {
                particleSystem->Render(particleViewId, viewProj);
            }
        }
    }

    // Render TerminalHub
    if (m_TerminalHub && window) {
        auto fbSize = window->GetFramebufferSize();
        m_TerminalHub->Render(m_Camera, fbSize.first, fbSize.second);
    }

    // Render UI via UIManager
    if (m_UIManager) {
        // Update UI manager with current stats
        float frameTime = GetDeltaTime() * 1000.0f; // Convert to milliseconds
        m_UIManager->SetFPS(GetCurrentFPS());
        m_UIManager->SetFrameTime(frameTime);
        m_UIManager->SetObjectCount(m_DynamicObjects.size());
        m_UIManager->SetGrabbedObject(m_GrabbedEntity != 0);
        m_UIManager->Render();
    }

    // Render ImGui
    UISystem::Instance().Render();

    // Present frame
    m_Renderer->Present();

    // Update window title with FPS (GameBase handles FPS calculation)
    if (window) {
        window->SetTitle("Solstice Physics Playground | " + std::to_string(static_cast<int>(GetCurrentFPS())) + " FPS");
    }
}

void PhysicsPlayground::SpawnFluidContainer(const Math::Vec3& position) {
    if (!m_ObjectSpawner) return;

    // Create a box container (1x1x1m)
    float containerSize = 1.0f;
    uint32_t boxMeshID = m_MeshLibrary->AddMesh(Solstice::Arzachel::MeshFactory::CreateCube(containerSize));

    // Position container slightly above ground
    Math::Vec3 containerPos = position;
    containerPos.y = std::max(containerPos.y, 1.0f);

    auto containerObjID = m_Scene.AddObject(boxMeshID, containerPos, Math::Quaternion(), Math::Vec3(1, 1, 1), Render::ObjectType_Static);

    // Create physics entity for container
    auto containerEntityID = m_Registry.Create();
    auto& containerRB = m_Registry.Add<Physics::RigidBody>(containerEntityID);
    containerRB.Position = containerPos;
    containerRB.Rotation = Math::Quaternion();
    containerRB.IsStatic = true;
    containerRB.SetMass(0.0f);
    containerRB.Type = Physics::ColliderType::Box;
    containerRB.HalfExtents = Math::Vec3(containerSize * 0.5f, containerSize * 0.5f, containerSize * 0.5f);
    containerRB.RenderObjectID = containerObjID;

    // Create fluid simulation for the container
    auto fluidSim = std::make_unique<Physics::FluidSimulation>(32, 0.0f, 0.0f); // 32x32x32 grid
    Physics::PhysicsSystem::Instance().RegisterFluidSimulation(fluidSim.get());
    m_FluidSimulations.push_back(std::move(fluidSim));
    m_FluidContainers.push_back(containerEntityID);

    // Spawn some test objects that will float
    for (int i = 0; i < 3; ++i) {
        Math::Vec3 spawnPos = containerPos + Math::Vec3(
            (i - 1) * 0.3f,
            0.5f,
            0.0f
        );
        PhysicsObject obj = m_ObjectSpawner->SpawnObject(ObjectType::Sphere, spawnPos, m_MaterialPresets.BlueMat);
        m_DynamicObjects.push_back(obj);

        // Add Fluid component to make objects float
        auto& testRB = m_Registry.Get<Physics::RigidBody>(obj.EntityID);
        auto& fluid = m_Registry.Add<Physics::Fluid>(obj.EntityID);
        fluid.Density = 500.0f; // Less dense than water (1000), so it floats
        fluid.Viscosity = 0.1f;
    }

    SIMPLE_LOG("Spawned fluid container at (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
}

void PhysicsPlayground::SpawnObjectFromUI(ObjectType type) {
    if (!m_ObjectSpawner) return;

    Math::Vec3 spawnPos = m_Camera.Position + m_Camera.Front * 5.0f;

    // Randomly assign material from presets (30% metallic, 20% glassy, 50% textured/standard)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);
    int roll = dis(gen);

    uint32_t materialID = 0;
    if (roll < 30) {
        // Metallic
        uint32_t metallicMats[] = {
            m_MaterialPresets.ChromeMat,
            m_MaterialPresets.GoldMat,
            m_MaterialPresets.SilverMat,
            m_MaterialPresets.CopperMat
        };
        materialID = metallicMats[roll % 4];
    } else if (roll < 50) {
        // Glassy
        uint32_t glassMats[] = {
            m_MaterialPresets.ClearGlassMat,
            m_MaterialPresets.TintedGlassMat,
            m_MaterialPresets.FrostedGlassMat
        };
        materialID = glassMats[(roll - 30) % 3];
    } else {
        // Textured/Standard
        uint32_t texturedMats[] = {
            m_MaterialPresets.CheckerboardMat,
            m_MaterialPresets.NoiseMat,
            m_MaterialPresets.StripesMat,
            m_MaterialPresets.RedMat,
            m_MaterialPresets.GreenMat,
            m_MaterialPresets.BlueMat
        };
        materialID = texturedMats[(roll - 50) % 6];
    }

    PhysicsObject obj = m_ObjectSpawner->SpawnObject(type, spawnPos, materialID, Math::Quaternion());
    m_DynamicObjects.push_back(obj);

    SIMPLE_LOG("Spawned object of type " + std::to_string(static_cast<int>(type)) + " with material " + std::to_string(materialID));
}

} // namespace Solstice::PhysicsPlayground

// Main entry point
int main() {
    Solstice::PhysicsPlayground::PhysicsPlayground playground;
    // Window will be created in Initialize() via InitializeWindow()
    return playground.Run();
}

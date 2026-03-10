#include <UI/Window.hxx>
#include <UI/UISystem.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Render/Particle/ParticlePresets.hxx>
#include <Render/Particle/ParticleSystem.hxx>
#include <Core/Material.hxx>
#include <Arzachel/MeshFactory.hxx>
#include <Render/PhysicsBridge.hxx>
#include <Physics/PhysicsSystem.hxx>
#include <Physics/RigidBody.hxx>
#include <Entity/Registry.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Core/Debug.hxx>
#include <Core/Async.hxx>
#include <Core/Audio.hxx>
#include <UI/ViewportUI.hxx> // For world-space UI
#include <bgfx/bgfx.h>

#include <imgui.h>
#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <random>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <utility>
#include <stdexcept>
#include <exception>

using namespace Solstice;
using namespace Solstice::Render;
using namespace Solstice::UI;
using namespace Solstice::Math;
using namespace Solstice::Core;

namespace MeshFactory = Solstice::Arzachel::MeshFactory;

// Simple structure to track falling objects
struct Debris {
    ECS::EntityId entityID;
    std::string label;
    float spawnTime{0.0f};
    float lastY{0.0f};
    bool exploded{false};
};

struct ExplosionFX {
    Math::Vec3 Position{};
    float Age{0.0f};
    float Life{2.0f};
    float BurstDuration{0.25f};
    float SmokeDelay{0.1f};
    float Intensity{1.0f};
    bool Active{false};
    std::unique_ptr<Render::ParticleSystem> Fire;
    std::unique_ptr<Render::ParticleSystem> Smoke;
    std::unique_ptr<Render::ParticleSystem> Sparks;
};

int main() {
    try {
        std::set_terminate([]() {
            SIMPLE_LOG("Avalanche: std::terminate invoked");
            std::abort();
        });
        Core::JobSystem::Instance().Initialize();
        Core::Audio::AudioManager::Instance().Initialize();
        Core::Audio::AudioManager::Instance().SetMasterVolume(0.9f);
        Core::Audio::AudioManager::Instance().SetSoundVolume(0.85f);

        // Resolve example asset paths from the build output directory
        std::filesystem::path assetRoot = std::filesystem::weakly_canonical(
            std::filesystem::current_path() / ".." / ".." / ".." / ".." / "example" / "Avalanche" / "assets"
        );

        Window window(1280, 720, "Solstice - Avalanche Demo");

        SoftwareRenderer Renderer(1280, 720, 16, window.NativeWindow());
        Renderer.SetWireframe(false);
        if (auto* post = Renderer.GetPostProcessing()) {
            post->SetGodRayEnabled(false);
        }

        struct UiShutdownGuard {
            bool Enabled{false};
            ~UiShutdownGuard() {
                if (Enabled && UISystem::Instance().IsInitialized()) {
                    UISystem::Instance().Shutdown();
                }
            }
        } uiShutdownGuard;

        constexpr bool kEnableUI = true;
        constexpr bool kEnableParticles = true;
        constexpr bool kEnableCheckerTexture = false;
        if (kEnableUI) {
            SDL_Window* nativeWindow = window.NativeWindow();
            if (!nativeWindow) {
                throw std::runtime_error("Avalanche: SDL window handle is null");
            }
            UISystem::Instance().Initialize(nativeWindow);
            if (!UISystem::Instance().IsInitialized()) {
                throw std::runtime_error("Avalanche: UISystem failed to initialize");
            }
        }

        // Share ImGui
        if (kEnableUI) {
            void* imguiContext = UISystem::Instance().GetImGuiContext();
            if (!imguiContext) {
                throw std::runtime_error("Avalanche: ImGui context is null after UI init");
            }
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
            if (ImGui::GetCurrentContext() != imguiContext) {
                throw std::runtime_error("Avalanche: Failed to set ImGui context");
            }
            SIMPLE_LOG("Avalanche: ImGui context successfully set");
            uiShutdownGuard.Enabled = true;
        }

        // Scene Setup
        Scene DemoScene;
        MeshLibrary MeshLib;
        MaterialLibrary MatLib;
        DemoScene.SetMeshLibrary(&MeshLib);
        DemoScene.SetMaterialLibrary(&MatLib);

        SIMPLE_LOG("Avalanche: Scene created");

        // Materials (vibrant palette for demo flair)
        SIMPLE_LOG("Avalanche: Materials setup start");
        uint16_t checkerTexIndex = 0xFFFF;
        bool checkerTexturePending = kEnableCheckerTexture;
        if (!kEnableCheckerTexture) {
            SIMPLE_LOG("Avalanche: Checker texture disabled (bgfx::copy crash guard)");
        }
        std::vector<uint32_t> checkerMaterialIds;
        checkerMaterialIds.reserve(12);

        SIMPLE_LOG("Avalanche: Materials - creating ground");
        Core::Material groundMat = Materials::CreateUnlit(Vec3(0.25f, 0.26f, 0.3f));
        uint32_t GroundMat = MatLib.AddMaterial(groundMat);
        if (MatLib.GetMaterialCount() == 0 || GroundMat >= MatLib.GetMaterialCount()) {
            throw std::runtime_error("Avalanche: Failed to register ground material");
        }
        checkerMaterialIds.push_back(GroundMat);

        SIMPLE_LOG("Avalanche: Materials - creating obstacle");
        Core::Material obstacleMat = Materials::CreateUnlit(Vec3(0.45f, 0.4f, 0.35f));
        uint32_t ObstacleMat = MatLib.AddMaterial(obstacleMat);
        if (ObstacleMat >= MatLib.GetMaterialCount()) {
            throw std::runtime_error("Avalanche: Failed to register obstacle material");
        }
        checkerMaterialIds.push_back(ObstacleMat);
        SIMPLE_LOG("Avalanche: Materials - creating scorch");
        uint32_t ScorchMat = MatLib.AddMaterial(Materials::CreateEmissive(Vec3(0.9f, 0.2f, 0.05f), 1.2f));
        if (ScorchMat >= MatLib.GetMaterialCount()) {
            throw std::runtime_error("Avalanche: Failed to register scorch material");
        }

        std::vector<uint32_t> debrisMaterials;
        debrisMaterials.reserve(12);

        auto addDebrisMat = [&](const Core::Material& mat, bool usesChecker) {
            uint32_t id = MatLib.AddMaterial(mat);
            debrisMaterials.push_back(id);
            if (usesChecker) {
                checkerMaterialIds.push_back(id);
            }
        };

        Core::Material matte = Materials::CreateUnlit(Vec3(0.78f, 0.8f, 0.86f));
        addDebrisMat(matte, true);

        Core::Material slate = Materials::CreateUnlit(Vec3(0.2f, 0.4f, 0.7f));
        addDebrisMat(slate, true);

        Core::Material crimson = Materials::CreateUnlit(Vec3(0.85f, 0.2f, 0.2f));
        addDebrisMat(crimson, true);

        Core::Material neon = Materials::CreateEmissive(Vec3(0.4f, 0.9f, 1.0f), 1.6f);
        addDebrisMat(neon, false);

        Core::Material gold = Materials::CreateUnlit(Vec3(0.95f, 0.72f, 0.2f));
        addDebrisMat(gold, true);

        Core::Material cobalt = Materials::CreateUnlit(Vec3(0.2f, 0.65f, 1.0f));
        addDebrisMat(cobalt, true);

        Core::Material steel = Materials::CreateUnlit(Vec3(0.6f, 0.6f, 0.65f));
        addDebrisMat(steel, true);

        Core::Material aqua = Materials::CreateUnlit(Vec3(0.45f, 0.85f, 1.0f));
        addDebrisMat(aqua, true);
        addDebrisMat(Materials::CreateEmissive(Vec3(1.0f, 0.55f, 0.1f), 1.4f), false);
        addDebrisMat(Materials::CreateEmissive(Vec3(0.75f, 0.35f, 1.0f), 1.3f), false);

        SIMPLE_LOG("Avalanche: Materials created");

        // Meshes
        auto sphereMesh = MeshFactory::CreateSphere(0.4f, 12);
        sphereMesh->IsStatic = true;
        uint32_t SphereID = MeshLib.AddMesh(std::move(sphereMesh));

        auto cubeMesh = MeshFactory::CreateCube(0.8f);
        cubeMesh->IsStatic = true;
        uint32_t CubeID = MeshLib.AddMesh(std::move(cubeMesh));

        auto planeMesh = MeshFactory::CreatePlane(100.0f, 100.0f);
        planeMesh->IsStatic = true;
        uint32_t PlaneID = MeshLib.AddMesh(std::move(planeMesh));

        SIMPLE_LOG("Avalanche: Meshes created");

        // Physics
        ECS::Registry Registry;
        Physics::PhysicsSystem::Instance().Start(Registry);

        // Ground Plane (Slope)
        {
            auto renderID = DemoScene.AddObject(PlaneID, Vec3(0, -5, 0), Quaternion(), Vec3(1,1,1), ObjectType_Static);
            DemoScene.SetMaterial(renderID, GroundMat);
            auto entityID = Registry.Create();
            auto& rb = Registry.Add<Physics::RigidBody>(entityID);
            rb.Position = Vec3(0, -5, 0);
            rb.IsStatic = true;
            rb.Type = Physics::ColliderType::Box; // Infinite plane approximation
            rb.HalfExtents = Vec3(100, 0.1f, 100);
            rb.Friction = 0.85f;
            rb.Restitution = 0.05f;
            rb.RenderObjectID = renderID;
        }

        // Funnel / Obstacles
        for(int i=0; i<5; ++i) {
             auto entityID = Registry.Create();
             auto& rb = Registry.Add<Physics::RigidBody>(entityID);
             float x = (i % 2 == 0) ? -5.0f : 5.0f;
             float y = i * 4.0f;
             rb.Position = Vec3(x, y, 0);
             rb.IsStatic = true;
             rb.Type = Physics::ColliderType::Box;
             rb.HalfExtents = Vec3(4.0f, 0.5f, 4.0f);
            float angleRad = (i % 2 == 0 ? -15.0f : 15.0f) * 3.14159f / 180.0f;
            rb.Rotation = Quaternion::FromEuler(0.0f, 0.0f, angleRad);
            rb.Friction = 0.85f;
            rb.Restitution = 0.05f;

             // Add corresponding visual? (Skipping for brevity, invisible colliders are fun too or just debug draw)
             // Let's add simple visual boxes
             auto boxMesh = MeshFactory::CreateCube(1.0f);
             boxMesh->IsStatic = true;
             uint32_t BoxMesh = MeshLib.AddMesh(std::move(boxMesh));
             // Scale visual to match physics
             auto renderID = DemoScene.AddObject(BoxMesh, rb.Position, rb.Rotation, rb.HalfExtents * 2.0f, ObjectType_Static);
             DemoScene.SetMaterial(renderID, ObstacleMat);
             rb.RenderObjectID = renderID;
        }

        Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();

        // Object Spawning State
        std::vector<Debris> debrisList;
        int spawnCounter = 0;
        int maxObjects = 2000;
        float spawnRate = 0.05f; // Initial spawn interval
        float spawnTimer = 0.0f;
        debrisList.reserve(static_cast<size_t>(maxObjects));

        // Random generators
        std::mt19937 rng(static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::uniform_real_distribution<float> rand01(0.0f, 1.0f);
        std::uniform_int_distribution<size_t> debrisMatPick(0, debrisMaterials.size() - 1);

        // Explosion SFX pool (mix it up)
        const std::array<std::string, 3> explosionSfx = {
            (assetRoot / "explosion01.wav").string(),
            (assetRoot / "explosion02.wav").string(),
            (assetRoot / "explosion03.wav").string()
        };
        std::uniform_int_distribution<int> explosionSfxPick(0, static_cast<int>(explosionSfx.size()) - 1);

        // Explosion particle pool
        const size_t explosionPoolSize = 16;
        std::vector<ExplosionFX> explosions(explosionPoolSize);
        size_t explosionCursor = 0;
        constexpr float kDormantSpawnRate = 0.01f;
        constexpr float kDormantDensity = 0.001f;

        if (kEnableParticles) {
            Render::FireParticleConfig fireConfig;
            fireConfig.MaxParticles = 700;
            fireConfig.SpawnRate = 900.0f;
            fireConfig.LifeMin = 0.25f;
            fireConfig.LifeMax = 0.9f;
            fireConfig.BaseSpawnRadius = 0.8f;
            fireConfig.SpawnHeight = 0.4f;
            fireConfig.SizeMin = 0.2f;
            fireConfig.SizeMax = 0.6f;
            fireConfig.UpwardVelocity = 4.0f;
            fireConfig.Turbulence = 1.1f;
            fireConfig.ColorStart = Vec3(1.0f, 0.3f, 0.1f);
            fireConfig.ColorMid = Vec3(1.0f, 0.7f, 0.15f);
            fireConfig.ColorEnd = Vec3(1.0f, 1.0f, 0.45f);
            fireConfig.SizeGrowthRate = 1.7f;

            Render::SmokeParticleConfig smokeConfig;
            smokeConfig.MaxParticles = 500;
            smokeConfig.SpawnRate = 180.0f;
            smokeConfig.LifeMin = 1.4f;
            smokeConfig.LifeMax = 3.2f;
            smokeConfig.BaseSpawnRadius = 1.4f;
            smokeConfig.SpawnHeight = 0.6f;
            smokeConfig.SizeMin = 0.4f;
            smokeConfig.SizeMax = 1.2f;
            smokeConfig.ColorStart = Vec3(0.08f, 0.08f, 0.1f);
            smokeConfig.ColorEnd = Vec3(0.35f, 0.35f, 0.38f);
            smokeConfig.DensityOpacity = 0.45f;
            smokeConfig.UpwardDrift = 0.6f;

            Render::ElectricityParticleConfig sparkConfig;
            sparkConfig.MaxParticles = 200;
            sparkConfig.SpawnRate = 400.0f;
            sparkConfig.LifeMin = 0.05f;
            sparkConfig.LifeMax = 0.35f;
            sparkConfig.BaseSpawnRadius = 0.6f;
            sparkConfig.SpawnHeight = 0.2f;
            sparkConfig.Speed = 12.0f;
            sparkConfig.Color = Vec3(1.0f, 0.9f, 0.6f);
            sparkConfig.ColorAlt = Vec3(0.6f, 0.8f, 1.0f);
            sparkConfig.Alpha = 0.95f;

            for (auto& fx : explosions) {
                fx.Fire = Render::ParticlePresets::CreateParticleSystem(Render::ParticlePresetType::Fire, fireConfig);
                fx.Smoke = Render::ParticlePresets::CreateParticleSystem(Render::ParticlePresetType::Smoke, smokeConfig);
                fx.Sparks = Render::ParticlePresets::CreateParticleSystem(Render::ParticlePresetType::Electricity, sparkConfig);

                if (fx.Fire) {
                    fx.Fire->SetSpawnRate(kDormantSpawnRate);
                    fx.Fire->SetDensity(kDormantDensity);
                    fx.Fire->SetMaxDistance(120.0f);
                }
                if (fx.Smoke) {
                    fx.Smoke->SetSpawnRate(kDormantSpawnRate);
                    fx.Smoke->SetDensity(kDormantDensity);
                    fx.Smoke->SetMaxDistance(120.0f);
                }
                if (fx.Sparks) {
                    fx.Sparks->SetSpawnRate(kDormantSpawnRate);
                    fx.Sparks->SetDensity(kDormantDensity);
                    fx.Sparks->SetMaxDistance(120.0f);
                }
            }
        }

        // Camera
        Camera Cam;
        Cam.Position = Vec3(0, 5, 25);
        Cam.ProcessMouseMovement(0, 0);

        auto lastTime = std::chrono::high_resolution_clock::now();

        // UI Animation State
        float counterFontSize = 1.0f;
        float simTime = 0.0f;
        int totalExplosions = 0;

        auto spawnExplosion = [&](const Vec3& position, float impactSpeed) {
            if (!kEnableParticles) {
                return;
            }
            if (explosions.empty()) return;
            ExplosionFX& fx = explosions[explosionCursor];
            explosionCursor = (explosionCursor + 1) % explosions.size();

            fx.Position = position;
            fx.Age = 0.0f;
            fx.BurstDuration = 0.22f + rand01(rng) * 0.08f;
            fx.SmokeDelay = 0.08f + rand01(rng) * 0.06f;
            fx.Life = 2.2f + rand01(rng) * 0.4f;
            fx.Intensity = std::clamp(impactSpeed / 10.0f, 0.7f, 1.8f);
            fx.Active = true;

            if (fx.Fire) {
                fx.Fire->SetSpawnRate(1200.0f * fx.Intensity);
                fx.Fire->SetDensity(1.0f);
            }
            if (fx.Sparks) {
                fx.Sparks->SetSpawnRate(700.0f * fx.Intensity);
                fx.Sparks->SetDensity(1.0f);
            }
            if (fx.Smoke) {
                fx.Smoke->SetSpawnRate(220.0f * fx.Intensity);
                fx.Smoke->SetDensity(1.0f);
            }

            auto source = Core::Audio::AudioManager::Instance().PlaySound3D(
                explosionSfx[explosionSfxPick(rng)].c_str(),
                position,
                80.0f,
                false
            );
            Core::Audio::AudioManager::Instance().UpdateAudioSource(source);
            totalExplosions++;
        };

        while (!window.ShouldClose()) {
            window.PollEvents();
            if (kEnableUI) {
                void* imguiContext = UISystem::Instance().GetImGuiContext();
                if (!imguiContext) {
                    throw std::runtime_error("Avalanche: ImGui context missing before NewFrame");
                }
                if (ImGui::GetCurrentContext() != imguiContext) {
                    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
                }
                UISystem::Instance().NewFrame();
            }

            if (checkerTexturePending) {
                SIMPLE_LOG("Avalanche: Checker texture - begin");
                const uint8_t checkerPixels[4 * 4 * 4] = {
                    30, 30, 30, 255, 200, 200, 200, 255, 30, 30, 30, 255, 200, 200, 200, 255,
                    200, 200, 200, 255, 30, 30, 30, 255, 200, 200, 200, 255, 30, 30, 30, 255,
                    30, 30, 30, 255, 200, 200, 200, 255, 30, 30, 30, 255, 200, 200, 200, 255,
                    200, 200, 200, 255, 30, 30, 30, 255, 200, 200, 200, 255, 30, 30, 30, 255
                };
                SIMPLE_LOG("Avalanche: Checker texture - before copy");
                const bgfx::Memory* mem = bgfx::copy(checkerPixels, sizeof(checkerPixels));
                SIMPLE_LOG("Avalanche: Checker texture - after copy");
                if (!mem) {
                    throw std::runtime_error("Avalanche: BGFX memory copy failed for checker texture");
                }
                SIMPLE_LOG("Avalanche: Checker texture - before createTexture2D");
                bgfx::TextureHandle checkerTexture = bgfx::createTexture2D(
                    4, 4, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem
                );
                SIMPLE_LOG("Avalanche: Checker texture - after createTexture2D");
                if (bgfx::isValid(checkerTexture)) {
                    SIMPLE_LOG("Avalanche: Checker texture - before register");
                    checkerTexIndex = Renderer.GetTextureRegistry().Register(checkerTexture);
                    SIMPLE_LOG("Avalanche: Checker texture registered at index " + std::to_string(checkerTexIndex));
                    for (uint32_t materialId : checkerMaterialIds) {
                        if (auto* mat = MatLib.GetMaterial(materialId)) {
                            mat->AlbedoTexIndex = checkerTexIndex;
                        }
                    }
                    checkerTexturePending = false;
                    SIMPLE_LOG("Avalanche: Checker texture applied to materials");
                } else {
                    SIMPLE_LOG("Avalanche: Failed to create checker texture");
                }
                SIMPLE_LOG("Avalanche: Checker texture - end");
            }

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;
            simTime += dt;

            Core::Audio::Listener listener;
            listener.Position = Cam.Position;
            listener.Forward = Cam.Front;
            listener.Up = Cam.Up;
            listener.CurrentReverb = {0.0f, 0.0f, 1.0f};
            listener.TargetReverb = Core::Audio::ReverbPresetType::None;
            Core::Audio::AudioManager::Instance().SetListener(listener);
            Core::Audio::AudioManager::Instance().Update(dt);

            // Spawning Logic
            if (debrisList.size() < maxObjects) {
                spawnTimer += dt;
                // Adaptive spawn pacing to keep the demo smooth under load
                if (dt > 1.0f / 50.0f) {
                    spawnRate = std::min(spawnRate + dt * 0.03f, 0.09f);
                } else if (dt < 1.0f / 90.0f) {
                    spawnRate = std::max(spawnRate - dt * 0.004f, 0.012f);
                }

                if (spawnTimer >= spawnRate) {
                    // Spawn Batch
                    int batchSize = 1 + (int)(debrisList.size() / 100); // Increase batch size as we go

                    for(int b=0; b<batchSize && debrisList.size() < maxObjects; ++b) {
                        float rX = (rand01(rng) * 10.0f) - 5.0f;
                        float rZ = (rand01(rng) * 10.0f) - 5.0f;
                        Vec3 pos(rX, 25.0f + b*2.0f, rZ);
                        float scale = 0.7f + rand01(rng) * 1.2f;
                        Vec3 renderScale(scale, scale, scale);

                        uint32_t mesh = (rand01(rng) > 0.5f) ? SphereID : CubeID;
                        uint32_t mat = debrisMaterials[debrisMatPick(rng)];

                        float yaw = rand01(rng) * 6.28318f;
                        float pitch = (rand01(rng) - 0.5f) * 0.6f;
                        float roll = (rand01(rng) - 0.5f) * 0.6f;
                        Quaternion rot = Quaternion::FromEuler(pitch, yaw, roll);

                        auto renderID = DemoScene.AddObject(mesh, pos, rot, renderScale, ObjectType_Dynamic);
                        DemoScene.SetMaterial(renderID, mat);

                        auto entityID = Registry.Create();
                        auto& rb = Registry.Add<Physics::RigidBody>(entityID);
                        rb.Position = pos;
                        rb.Rotation = rot;
                        rb.SetMass(4.0f * scale);
                        rb.Type = mesh == SphereID ? Physics::ColliderType::Sphere : Physics::ColliderType::Box;
                        if (mesh == SphereID) rb.Radius = 0.4f * scale;
                        else rb.HalfExtents = Vec3(0.4f * scale, 0.4f * scale, 0.4f * scale);
                        rb.Friction = 0.7f;
                        rb.Restitution = 0.15f;
                        rb.RenderObjectID = renderID;
                        rb.LinearDamping = 0.6f;
                        rb.AngularDrag = 0.6f;

                        std::string label = "OBJ-" + std::to_string(spawnCounter++);
                        debrisList.push_back({entityID, label, simTime, pos.y, false});

                        counterFontSize = 1.5f;
                    }
                    spawnTimer = 0.0f;
                }
            }

            if (counterFontSize > 1.0f) counterFontSize -= dt * 1.0f;

            // Physics Update (Fixed Step)
            const float physicsStep = 1.0f/60.0f;
            static float accumulator = 0.0f;
            accumulator += dt;
            int steps = 0;
            while(accumulator >= physicsStep && steps < 5) {
                Physics::PhysicsSystem::Instance().Update(physicsStep);
                accumulator -= physicsStep;
                steps++;
            }
            SyncPhysicsToScene(Registry, DemoScene);
            DemoScene.UpdateTransforms();

            // Impact-driven explosions + impulse shockwave
            const float impactY = -4.6f;
            const float impactVelocity = -6.0f;
            const float explosionRadius = 6.0f;
            const float explosionImpulse = 38.0f;
            int explosionsThisFrame = 0;
            const int maxExplosionsPerFrame = 3;

            for (auto& d : debrisList) {
                if (!Registry.Has<Physics::RigidBody>(d.entityID)) {
                    continue;
                }

                auto& rb = Registry.Get<Physics::RigidBody>(d.entityID);
                bool crossedGround = (d.lastY > impactY && rb.Position.y <= impactY);
                if (!d.exploded && (simTime - d.spawnTime) > 0.2f && crossedGround && rb.Velocity.y < impactVelocity) {
                    if (explosionsThisFrame < maxExplosionsPerFrame && rand01(rng) > 0.15f) {
                        float impactSpeed = std::fabs(rb.Velocity.y);
                        spawnExplosion(rb.Position, impactSpeed);
                        explosionsThisFrame++;

                        for (auto& other : debrisList) {
                            if (!Registry.Has<Physics::RigidBody>(other.entityID)) continue;
                            auto& otherRb = Registry.Get<Physics::RigidBody>(other.entityID);
                            if (otherRb.IsStatic) continue;

                            Vec3 delta = otherRb.Position - rb.Position;
                            float dist = delta.Magnitude();
                            if (dist > 0.01f && dist < explosionRadius) {
                                float strength = explosionImpulse * (1.0f - (dist / explosionRadius));
                                otherRb.ApplyImpulse(delta / dist * strength);
                            }
                        }
                    }

                    d.exploded = true;
                    if (rb.RenderObjectID != Render::InvalidObjectID) {
                        DemoScene.SetMaterial(rb.RenderObjectID, ScorchMat);
                    }
                }

                d.lastY = rb.Position.y;
            }

            // Update explosion particle systems
            if (kEnableParticles) {
                for (auto& fx : explosions) {
                if (!fx.Fire && !fx.Smoke && !fx.Sparks) continue;

                bool hasLiveParticles = false;
                if (fx.Fire) hasLiveParticles |= fx.Fire->GetActiveParticleCount() > 0;
                if (fx.Smoke) hasLiveParticles |= fx.Smoke->GetActiveParticleCount() > 0;
                if (fx.Sparks) hasLiveParticles |= fx.Sparks->GetActiveParticleCount() > 0;

                if (!fx.Active && !hasLiveParticles) {
                    continue;
                }

                if (fx.Active) {
                    fx.Age += dt;

                    float fireRate = (fx.Age <= fx.BurstDuration) ? 1200.0f * fx.Intensity : kDormantSpawnRate;
                    float fireDensity = (fx.Age <= fx.BurstDuration) ? 1.0f : kDormantDensity;
                    float sparkRate = (fx.Age <= fx.BurstDuration * 0.6f) ? 700.0f * fx.Intensity : kDormantSpawnRate;
                    float sparkDensity = (fx.Age <= fx.BurstDuration * 0.6f) ? 1.0f : kDormantDensity;
                    float smokeRate = (fx.Age >= fx.SmokeDelay && fx.Age <= fx.Life) ? 220.0f * fx.Intensity : kDormantSpawnRate;
                    float smokeDensity = (fx.Age >= fx.SmokeDelay && fx.Age <= fx.Life) ? 1.0f : kDormantDensity;

                    if (fx.Fire) {
                        fx.Fire->SetSpawnRate(fireRate);
                        fx.Fire->SetDensity(fireDensity);
                    }
                    if (fx.Sparks) {
                        fx.Sparks->SetSpawnRate(sparkRate);
                        fx.Sparks->SetDensity(sparkDensity);
                    }
                    if (fx.Smoke) {
                        fx.Smoke->SetSpawnRate(smokeRate);
                        fx.Smoke->SetDensity(smokeDensity);
                    }

                    if (fx.Age > fx.Life + 0.6f) {
                        fx.Active = false;
                    }
                }

                    if (fx.Fire) fx.Fire->Update(dt, fx.Position);
                    if (fx.Smoke) fx.Smoke->Update(dt, fx.Position);
                    if (fx.Sparks) fx.Sparks->Update(dt, fx.Position);
                }
            }

            // Render
            Renderer.Clear(Vec4(0.1f, 0.1f, 0.15f, 1.0f));
            Renderer.RenderScene(DemoScene, Cam);

            auto [uiWidth, uiHeight] = window.GetFramebufferSize();
            Math::Matrix4 viewMatrix = Cam.GetViewMatrix();
            float aspect = uiHeight > 0 ? static_cast<float>(uiWidth) / static_cast<float>(uiHeight) : 1.0f;
            Math::Matrix4 projMatrix = Math::Matrix4::Perspective(Cam.GetZoom() * 0.0174533f, aspect, 0.1f, 2000.0f);
            Math::Matrix4 viewProj = projMatrix * viewMatrix;

            if (kEnableParticles) {
                // Render explosion particles into the main scene view
                bgfx::ViewId particleViewId = Renderer.GetSceneViewId();
                for (auto& fx : explosions) {
                    bool hasParticles = false;
                    if (fx.Fire) hasParticles |= fx.Fire->GetActiveParticleCount() > 0;
                    if (fx.Smoke) hasParticles |= fx.Smoke->GetActiveParticleCount() > 0;
                    if (fx.Sparks) hasParticles |= fx.Sparks->GetActiveParticleCount() > 0;
                    if (!fx.Active && !hasParticles) continue;

                    if (fx.Fire) fx.Fire->Render(particleViewId, viewProj, Cam.Right, Cam.Up);
                    if (fx.Smoke) fx.Smoke->Render(particleViewId, viewProj, Cam.Right, Cam.Up);
                    if (fx.Sparks) fx.Sparks->Render(particleViewId, viewProj, Cam.Right, Cam.Up);
                }
            }

            // === Motion Graphics / UI ===

            if (kEnableUI) {
                // 1. World Space Labels (adaptive budget to keep UI memory stable)
                size_t labelBudget = 6;
                if (debrisList.size() < 200) labelBudget = 16;
                else if (debrisList.size() < 600) labelBudget = 10;
                if (dt < 1.0f / 90.0f) labelBudget += 4;
                if (dt > 1.0f / 50.0f) labelBudget = std::min<size_t>(labelBudget, 6);
                if (labelBudget > debrisList.size()) labelBudget = debrisList.size();
                size_t labelStride = labelBudget > 0 ? std::max<size_t>(1, debrisList.size() / labelBudget) : debrisList.size();

                size_t labelsDrawn = 0;
                ImDrawList* drawList = ImGui::GetForegroundDrawList();
                for (size_t i = 0; i < debrisList.size() && labelsDrawn < labelBudget; i += labelStride) {
                    auto& d = debrisList[i];
                    if (!Registry.Has<Physics::RigidBody>(d.entityID)) {
                        continue;
                    }

                    auto& rb = Registry.Get<Physics::RigidBody>(d.entityID);
                    Vec3 pos = rb.Position + Vec3(0, 1.0f, 0); // Above object
                    Vec3 toObj = pos - Cam.Position;
                    float dist = toObj.Magnitude();
                    if (toObj.Dot(Cam.Front) <= 0 || dist > 40.0f) {
                        continue;
                    }

                    auto screenPos = UI::ViewportUI::ProjectToScreen(pos, viewMatrix, projMatrix, uiWidth, uiHeight);
                    ImVec2 textSize = ImGui::CalcTextSize(d.label.c_str());
                    if (screenPos.x < -textSize.x || screenPos.x > uiWidth + textSize.x ||
                        screenPos.y < -textSize.y || screenPos.y > uiHeight + textSize.y) {
                        continue;
                    }

                    if (drawList) {
                        if (labelsDrawn < 6) {
                            drawList->AddRectFilled(
                                ImVec2(screenPos.x - textSize.x * 0.5f - 4, screenPos.y - textSize.y * 0.5f - 4),
                                ImVec2(screenPos.x + textSize.x * 0.5f + 4, screenPos.y + textSize.y * 0.5f + 4),
                                IM_COL32(0, 0, 0, 170), 4.0f
                            );
                        }
                        drawList->AddText(
                            ImVec2(screenPos.x - textSize.x * 0.5f, screenPos.y - textSize.y * 0.5f),
                            IM_COL32(255, 255, 255, 210), d.label.c_str()
                        );
                    }
                    labelsDrawn++;
                }

                int activeExplosionFx = 0;
                uint32_t activeParticles = 0;
                for (auto& fx : explosions) {
                    if (fx.Active) {
                        activeExplosionFx++;
                    }
                    if (fx.Fire) activeParticles += fx.Fire->GetActiveParticleCount();
                    if (fx.Smoke) activeParticles += fx.Smoke->GetActiveParticleCount();
                    if (fx.Sparks) activeParticles += fx.Sparks->GetActiveParticleCount();
                }

                // 2. HUD Counter with Shake
                ImGui::SetNextWindowPos(ImVec2(50, 50));
                ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);

                ImGui::SetWindowFontScale(counterFontSize);
                ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "AVALANCHE DEBRIS: %d", (int)debrisList.size());
                ImGui::SetWindowFontScale(1.0f);

                ImGui::Text("FPS: %.1f", 1.0f/dt);
                ImGui::Text("Explosions: %d (Active FX: %d)", totalExplosions, activeExplosionFx);
                ImGui::Text("Active Particles: %u", activeParticles);
                ImGui::End();

                UISystem::Instance().Render();
            }
            Renderer.Present();
        }

        if (kEnableUI) {
            UISystem::Instance().Shutdown();
        }
        Physics::PhysicsSystem::Instance().Stop();
        Core::Audio::AudioManager::Instance().Shutdown();
        Core::JobSystem::Instance().Shutdown();

    } catch (const std::exception& e) {
        SIMPLE_LOG(std::string("Avalanche: Unhandled exception: ") + e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        SIMPLE_LOG("Avalanche: Unhandled non-std exception");
        std::cerr << "Error: Unhandled non-std exception" << std::endl;
        return 1;
    }
    return 0;
}

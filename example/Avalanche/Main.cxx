#include <UI/Window.hxx>
#include <UI/UISystem.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene.hxx>
#include <Render/Camera.hxx>
#include <Render/Mesh.hxx>
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

#include <imgui.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>

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
};

int main() {
    try {
        Core::JobSystem::Instance().Initialize();
        Core::Audio::AudioManager::Instance().Initialize();

        Window window(1280, 720, "Solstice - Avalanche Demo");
        
        SoftwareRenderer Renderer(1280, 720, 16, window.NativeWindow());
        Renderer.SetWireframe(false);
        
        UISystem::Instance().Initialize(window.NativeWindow());

        // Share ImGui
        void* imguiContext = UISystem::Instance().GetImGuiContext();
        if (imguiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
        }

        // Scene Setup
        Scene DemoScene;
        MeshLibrary MeshLib;
        MaterialLibrary MatLib;
        DemoScene.SetMeshLibrary(&MeshLib);
        DemoScene.SetMaterialLibrary(&MatLib);

        // Materials
        uint32_t WhiteMat = MatLib.AddMaterial(Materials::CreateDefault());
        uint32_t RedMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(RedMat)->SetAlbedoColor(Vec3(0.9f, 0.2f, 0.2f), 0.3f);
        uint32_t BlueMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(BlueMat)->SetAlbedoColor(Vec3(0.2f, 0.2f, 0.9f), 0.3f);
        uint32_t YellowMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(YellowMat)->SetAlbedoColor(Vec3(0.9f, 0.9f, 0.1f), 0.3f);

        // Meshes
        uint32_t SphereID = MeshLib.AddMesh(MeshFactory::CreateSphere(0.4f, 12));
        uint32_t CubeID = MeshLib.AddMesh(MeshFactory::CreateCube(0.8f));
        uint32_t PlaneID = MeshLib.AddMesh(MeshFactory::CreatePlane(100.0f, 100.0f));

        // Physics
        ECS::Registry Registry;
        Physics::PhysicsSystem::Instance().Start(Registry);

        // Ground Plane (Slope)
        {
            auto renderID = DemoScene.AddObject(PlaneID, Vec3(0, -5, 0), Quaternion(), Vec3(1,1,1), ObjectType_Static);
            auto entityID = Registry.Create();
            auto& rb = Registry.Add<Physics::RigidBody>(entityID);
            rb.Position = Vec3(0, -5, 0);
            rb.IsStatic = true;
            rb.Type = Physics::ColliderType::Box; // Infinite plane approximation
            rb.HalfExtents = Vec3(100, 0.1f, 100);
            rb.Friction = 0.3f;
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
             rb.Rotation = Quaternion::AngleAxis((i%2==0 ? -15.0f : 15.0f) * 3.14159f/180.0f, Vec3(0,0,1));
             
             // Add corresponding visual? (Skipping for brevity, invisible colliders are fun too or just debug draw)
             // Let's add simple visual boxes
             uint32_t BoxMesh = MeshLib.AddMesh(MeshFactory::CreateCube(1.0f)); 
             // Scale visual to match physics
             auto renderID = DemoScene.AddObject(BoxMesh, rb.Position, rb.Rotation, rb.HalfExtents * 2.0f, ObjectType_Static);
             rb.RenderObjectID = renderID;
        }
        
        Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();

        // Object Spawning State
        std::vector<Debris> debrisList;
        int spawnCounter = 0;
        int maxObjects = 1500;
        float spawnRate = 0.05f; // Initial spawn interval
        float spawnTimer = 0.0f;

        // Camera
        Camera Cam;
        Cam.Position = Vec3(0, 5, 25);
        Cam.ProcessMouseMovement(0, 0);

        // Viewport UI for World Labels
        ViewportUI viewportUI;
        viewportUI.Initialize(window.NativeWindow());

        auto lastTime = std::chrono::high_resolution_clock::now();

        // UI Animation State
        float counterShake = 0.0f;
        float counterFontSize = 1.0f;

        while (!window.ShouldClose()) {
            window.PollEvents();
            UISystem::Instance().NewFrame();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            // Spawning Logic
            if (debrisList.size() < maxObjects) {
                spawnTimer += dt;
                // Accelerate spawn rate over time
                if (spawnRate > 0.005f) spawnRate -= dt * 0.001f;

                if (spawnTimer >= spawnRate) {
                    // Spawn Batch
                    int batchSize = 1 + (int)(debrisList.size() / 100); // Increase batch size as we go
                    
                    for(int b=0; b<batchSize && debrisList.size() < maxObjects; ++b) {
                        float rX = ((rand() % 100) / 10.0f) - 5.0f; 
                        float rZ = ((rand() % 100) / 10.0f) - 5.0f;
                        Vec3 pos(rX, 25.0f + b*2.0f, rZ);

                        uint32_t mesh = (rand() % 2 == 0) ? SphereID : CubeID;
                        uint32_t mat = (rand() % 3 == 0) ? RedMat : (rand() % 2 == 0 ? BlueMat : YellowMat);
                        
                        auto renderID = DemoScene.AddObject(mesh, pos, Quaternion(), Vec3(1,1,1), ObjectType_Dynamic);
                        DemoScene.GetObject(renderID)->MaterialID = mat;

                        auto entityID = Registry.Create();
                        auto& rb = Registry.Add<Physics::RigidBody>(entityID);
                        rb.Position = pos;
                        rb.SetMass(5.0f); // Heavy objects
                        rb.Type = mesh == SphereID ? Physics::ColliderType::Sphere : Physics::ColliderType::Box;
                        if (mesh == SphereID) rb.Radius = 0.4f;
                        else rb.HalfExtents = Vec3(0.4f, 0.4f, 0.4f);
                        rb.Restitution = 0.6f;
                        rb.RenderObjectID = renderID;
                        rb.LinearDamping = 0.1f;
                        rb.AngularDrag = 0.1f;

                        std::string label = "OBJ-" + std::to_string(spawnCounter++);
                        debrisList.push_back({entityID, label});

                        // Trigger UI Shake
                        counterShake = 5.0f;
                        counterFontSize = 1.5f;
                    }
                    spawnTimer = 0.0f;
                }
            }

            // Decay Shake
            if (counterShake > 0.0f) counterShake -= dt * 20.0f;
            if (counterShake < 0.0f) counterShake = 0.0f;
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

            // Render
            Renderer.Clear(Vec4(0.1f, 0.1f, 0.15f, 1.0f));
            Renderer.RenderScene(DemoScene, Cam);

            // === Motion Graphics / UI ===
            
            // 1. World Space Labels (on every 50th object to save visual clutter)
            for (size_t i = 0; i < debrisList.size(); i += 50) {
                auto& d = debrisList[i];
                if (Registry.Has<Physics::RigidBody>(d.entityID)) {
                    auto& rb = Registry.Get<Physics::RigidBody>(d.entityID);
                    Vec3 pos = rb.Position + Vec3(0, 1.0f, 0); // Above object
                    
                    // Simple Billboard check (in front of camera)
                    Vec3 toObj = pos - Cam.Position;
                    if (toObj.Dot(Cam.Front) > 0) {
                        viewportUI.DrawLabel3D(pos, d.label.c_str(), Vec4(1,1,1,0.8f));
                    }
                }
            }

            // 2. HUD Counter with Shake
            ImGui::SetNextWindowPos(ImVec2(50 + ((rand()%10 - 5) * (counterShake>0)), 
                                         50 + ((rand()%10 - 5) * (counterShake>0))));
            ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
            
            ImGui::SetWindowFontScale(counterFontSize + (counterShake * 0.1f));
            ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "AVALANCHE DEBRIS: %d", (int)debrisList.size());
            ImGui::SetWindowFontScale(1.0f);
            
            ImGui::Text("FPS: %.1f", 1.0f/dt);
            ImGui::End();

            UISystem::Instance().Render();
            Renderer.Present();
        }

        UISystem::Instance().Shutdown();
        Physics::PhysicsSystem::Instance().Stop();
        Core::Audio::AudioManager::Instance().Shutdown();
        Core::JobSystem::Instance().Shutdown();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

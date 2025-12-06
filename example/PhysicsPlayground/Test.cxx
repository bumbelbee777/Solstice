#include <UI/Window.hxx>
#include <UI/UISystem.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene.hxx>
#include <Render/Camera.hxx>
#include <Render/Mesh.hxx>
#include <Render/Material.hxx>
#include <Render/PhysicsBridge.hxx>
#include <Physics/PhysicsSystem.hxx>
#include <Physics/RigidBody.hxx>
#include <Entity/Registry.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Core/Debug.hxx>
#include <Core/Async.hxx>
#include <Core/Audio.hxx>
#include <Scripting/BytecodeVM.hxx>
#include <Scripting/Compiler.hxx>

#include <imgui.h>

#include <iostream>
#include <chrono>
#include <string>
#include <bgfx/bgfx.h>

// Key constants
static constexpr int KEY_ESCAPE = 27;
static constexpr int KEY_E = 'e';
static constexpr int MOD_CTRL = 0x0040;
static constexpr int MOD_ALT  = 0x0100;

using namespace Solstice;
using namespace Solstice::Render;
using namespace Solstice::UI;
using namespace Solstice::Math;

int main() {
    try {
        // Initialize job system for async operations
        Core::JobSystem::Instance().Initialize();
        
        // Initialize Audio
        Core::Audio::AudioManager::Instance().Initialize();
        
        // Create window
        Window window(1280, 720, "Solstice Engine - Physics Playground");

        std::cout << "=== Solstice Engine - Physics Playground ===" << std::endl;
        // Initialize Moonwalk scripting VM
        Solstice::Scripting::BytecodeVM vm;
        // Register native WriteLn function
        vm.RegisterNative("WriteLn", [](const std::vector<Solstice::Scripting::Value>& args) -> Solstice::Scripting::Value {
            if (!args.empty() && std::holds_alternative<std::string>(args[0])) {
                std::cout << std::get<std::string>(args[0]) << std::endl;
            }
            return Solstice::Scripting::Value();
        });
        // Create a simple program that prints a message
        Solstice::Scripting::Program prog;
        prog.Add(Solstice::Scripting::OpCode::PUSH_CONST, std::string("Hello, Moonwalk!"));
        prog.Add(Solstice::Scripting::OpCode::PUSH_CONST, int64_t(1)); // arg count
        prog.Add(Solstice::Scripting::OpCode::CALL, std::string("WriteLn"));
        prog.Add(Solstice::Scripting::OpCode::HALT);
        vm.LoadProgram(prog);
        vm.Run();
        std::cout << "Controls:" << std::endl;
        std::cout << "  E - Pick up/Drop object" << std::endl;
        std::cout << "  Right Mouse - Lock camera" << std::endl;
        std::cout << "  WASD - Move camera (when locked)" << std::endl;
        std::cout << std::endl;

        // Create renderer first (initializes BGFX)
        SIMPLE_LOG("Creating SoftwareRenderer...");
        SoftwareRenderer Renderer(1280, 720, 16, window.NativeWindow());
        Renderer.SetWireframe(false);
        SIMPLE_LOG("Renderer created");
        
        // Initialize UI System AFTER renderer (requires BGFX)
        SIMPLE_LOG("Initializing UISystem...");
        UISystem::Instance().Initialize(window.NativeWindow());
        SIMPLE_LOG("UISystem initialized");
        
        // CRITICAL: Share ImGui context from DLL to executable
        // This fixes the DLL/EXE boundary issue where each module has its own ImGui static data
        void* imguiContext = UISystem::Instance().GetImGuiContext();
        if (imguiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
            SIMPLE_LOG("ImGui context shared from DLL to executable");
        } else {
            SIMPLE_LOG("WARNING: Failed to get ImGui context from DLL!");
        }
        
        // Create scene + libraries
        Scene DemoScene;
        MeshLibrary MeshLib;
        MaterialLibrary MatLib;
        DemoScene.SetMeshLibrary(&MeshLib);
        DemoScene.SetMaterialLibrary(&MatLib);
        
        // Create materials with different colors
        uint32_t RedMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(RedMat)->SetAlbedoColor(Vec3(1.0f, 0.2f, 0.2f), 0.3f);
        
        uint32_t GreenMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(GreenMat)->SetAlbedoColor(Vec3(0.2f, 1.0f, 0.2f), 0.3f);
        
        uint32_t BlueMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(BlueMat)->SetAlbedoColor(Vec3(0.2f, 0.2f, 1.0f), 0.3f);
        
        uint32_t YellowMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(YellowMat)->SetAlbedoColor(Vec3(1.0f, 1.0f, 0.2f), 0.3f);
        
        uint32_t GrayMat = MatLib.AddMaterial(Materials::CreateDefault());
        MatLib.GetMaterial(GrayMat)->SetAlbedoColor(Vec3(0.5f, 0.5f, 0.5f), 0.5f);
        
        // Create meshes
        SIMPLE_LOG("Creating meshes...");
        uint32_t PlaneMeshID = MeshLib.AddMesh(MeshFactory::CreatePlane(1000.0f, 1000.0f)); // Much larger plane
        uint32_t CubeMeshID = MeshLib.AddMesh(MeshFactory::CreateCube(1.0f));
        uint32_t SphereMeshID = MeshLib.AddMesh(MeshFactory::CreateSphere(0.5f, 16));
        uint32_t TetraMeshID = MeshLib.AddMesh(MeshFactory::CreateTetrahedron(0.5f));
        uint32_t CylinderMeshID = MeshLib.AddMesh(MeshFactory::CreateCylinder(0.5f, 1.0f, 16));
        
        // Initialize Physics + ECS
        SIMPLE_LOG("Initializing Physics System...");
        ECS::Registry Registry;
        Physics::PhysicsSystem::Instance().Start(Registry);
        
        // Create plane - static ground
        auto PlaneObjID = DemoScene.AddObject(PlaneMeshID, Vec3(0, 0, 0), Quaternion(), Vec3(1, 1, 1), ObjectType_Static);
        
        // Apply gray material to plane
        Mesh* PlaneMesh = MeshLib.GetMesh(PlaneMeshID);
        if (PlaneMesh && !PlaneMesh->SubMeshes.empty()) {
            PlaneMesh->SubMeshes[0].MaterialID = GrayMat;
        }
        
        // Create dynamic objects with physics
        struct PhysicsObject {
            SceneObjectID renderID;
            ECS::EntityId entityID;
        };
        
        std::vector<PhysicsObject> dynamicObjects;
        
        // Sphere
        {
            auto renderID = DemoScene.AddObject(SphereMeshID, Vec3(-6, 8, -8), Quaternion(), Vec3(1, 1, 1), ObjectType_Dynamic);
            auto entityID = Registry.Create();
            auto& rb = Registry.Add<Physics::RigidBody>(entityID);
            rb.Position = Vec3(-6, 8, -8);
            rb.SetMass(1.0f);
            rb.Restitution = 0.6f;
            rb.Type = Physics::ColliderType::Sphere;
            rb.Radius = 0.5f;
            rb.RenderObjectID = renderID;
            dynamicObjects.push_back({renderID, entityID});
        }
        
        // Cube
        {
            auto renderID = DemoScene.AddObject(CubeMeshID, Vec3(0, 10, -8), Quaternion(), Vec3(1, 1, 1), ObjectType_Dynamic);
            auto entityID = Registry.Create();
            auto& rb = Registry.Add<Physics::RigidBody>(entityID);
            rb.Position = Vec3(0, 10, -8);
            rb.SetBoxInertia(1.2f, Vec3(0.5f, 0.5f, 0.5f));
            rb.Restitution = 0.4f;
            rb.Type = Physics::ColliderType::Box;
            rb.HalfExtents = Vec3(0.5f, 0.5f, 0.5f);
            rb.RenderObjectID = renderID;
            dynamicObjects.push_back({renderID, entityID});
        }
        
        // Tetrahedron
        {
            auto renderID = DemoScene.AddObject(TetraMeshID, Vec3(6, 12, -8), Quaternion(), Vec3(1, 1, 1), ObjectType_Dynamic);
            auto entityID = Registry.Create();
            auto& rb = Registry.Add<Physics::RigidBody>(entityID);
            rb.Position = Vec3(6, 12, -8);
            rb.SetBoxInertia(0.8f, Vec3(0.5f, 0.5f, 0.5f)); // Use box inertia instead of sphere
            rb.Restitution = 0.1f; // Much lower restitution to prevent excessive bouncing
            rb.Type = Physics::ColliderType::Box; // Use box collider for stable face resting
            rb.HalfExtents = Vec3(0.5f, 0.5f, 0.5f); // Approximate bounding box
            rb.RenderObjectID = renderID;
            dynamicObjects.push_back({renderID, entityID});
        }
        
        // Cylinder
        {
            auto renderID = DemoScene.AddObject(CylinderMeshID, Vec3(-3, 14, -8), Quaternion(), Vec3(1, 1, 1), ObjectType_Dynamic);
            auto entityID = Registry.Create();
            auto& rb = Registry.Add<Physics::RigidBody>(entityID);
            rb.Position = Vec3(-3, 14, -8);
            rb.SetBoxInertia(1.5f, Vec3(0.5f, 0.5f, 0.5f)); // Approx as box inertia
            rb.Restitution = 0.3f;
            rb.Type = Physics::ColliderType::Box; // Approx as box
            rb.HalfExtents = Vec3(0.5f, 0.5f, 0.5f);
            rb.RenderObjectID = renderID;
            dynamicObjects.push_back({renderID, entityID});
        }

        // Stack of Boxes
        for (int i = 0; i < 5; ++i) {
            float yPos = 2.0f + i * 1.1f; // Stack them up
            auto renderID = DemoScene.AddObject(CubeMeshID, Vec3(3, yPos, -8), Quaternion(), Vec3(1, 1, 1), ObjectType_Dynamic);
            auto entityID = Registry.Create();
            auto& rb = Registry.Add<Physics::RigidBody>(entityID);
            rb.Position = Vec3(3, yPos, -8);
            rb.SetBoxInertia(1.0f, Vec3(0.5f, 0.5f, 0.5f));
            rb.Restitution = 0.1f; // Low restitution for stacking
            rb.Friction = 0.6f;
            rb.Type = Physics::ColliderType::Box;
            rb.HalfExtents = Vec3(0.5f, 0.5f, 0.5f);
            rb.RenderObjectID = renderID;
            dynamicObjects.push_back({renderID, entityID});
        }
        
        SIMPLE_LOG("Physics playground created with " + std::to_string(dynamicObjects.size()) + " dynamic objects");
        
        // Create camera
        Camera Cam;
        Cam.Position = Vec3(0, 3, 8);
        Cam.Up = Vec3(0, 1, 0);
        Cam.Zoom = 45.0f;
        Cam.Yaw = -90.0f;
        Cam.ProcessMouseMovement(0, 0); // Init camera vectors

        // Interaction state
        bool pendingResize = false;
        int pendingW = 1280, pendingH = 720;
        auto lastResizeEvent = std::chrono::high_resolution_clock::now();
        bool mouseLocked = false;
        
        ECS::EntityId grabbedEntity = 0;
        float grabbedDistance = 3.0f;

        // Callbacks
        window.SetResizeCallback([&](int w, int h){
            pendingResize = true;
            pendingW = w; pendingH = h;
            lastResizeEvent = std::chrono::high_resolution_clock::now();
        });
        
        window.SetKeyCallback([&](int key, int scancode, int action, int mods){
            if (action == 1 && (key == 'f' || key == 'F')) {
                bool fs = window.IsFullscreen();
                window.SetFullscreen(!fs);
                auto fb = window.GetFramebufferSize();
                pendingResize = true;
                pendingW = fb.first; pendingH = fb.second;
                lastResizeEvent = std::chrono::high_resolution_clock::now();
            }
            
            // ESC - release mouse lock
            if (action == 1 && key == KEY_ESCAPE) {
                if (mouseLocked) {
                    mouseLocked = false;
                    window.SetRelativeMouse(false);
                    window.SetCursorGrab(false);
                    window.ShowCursor(true);
                    Renderer.SetShowCrosshair(false);
                }
            }
            
            // E - Pick up/Drop
            if (action == 1 && (key == 'e' || key == 'E')) {
                if (grabbedEntity == 0) {
                    // Find closest object in front of camera
                    float minDist = 10.0f;
                    ECS::EntityId closest = 0;
                    
                    Registry.ForEach<Physics::RigidBody>([&](ECS::EntityId eid, const Physics::RigidBody& rb) {
                        Vec3 toObj = rb.Position - Cam.Position;
                        float dist = toObj.Magnitude();
                        Vec3 forward = Cam.Front;
                        float dot = toObj.Normalized().Dot(forward);
                        
                        if (dot > 0.7f && dist < minDist) {
                            minDist = dist;
                            closest = eid;
                        }
                    });
                    
                    if (closest != 0) {
                        grabbedEntity = closest;
                        grabbedDistance = minDist;
                        SIMPLE_LOG("Grabbed object!");
                    }
                } else {
                    // Drop object
                    auto* rb = Registry.Has<Physics::RigidBody>(grabbedEntity) ? &Registry.Get<Physics::RigidBody>(grabbedEntity) : nullptr;
                    if (rb) {
                        // Give it a small forward impulse (safer than directly setting velocity)
                        const float impulseStrength = 3.0f * rb->Mass;
                        rb->ApplyImpulse(Cam.Front * impulseStrength);
                    }
                    grabbedEntity = 0;
                    SIMPLE_LOG("Dropped object");
                }
            }
            
            // Ctrl+Alt - lock mouse
            if (action == 1 && (mods & (MOD_CTRL|MOD_ALT)) == (MOD_CTRL|MOD_ALT)) {
                mouseLocked = true;
                window.SetRelativeMouse(true);
                window.SetCursorGrab(true);
                window.ShowCursor(false);
                Renderer.SetShowCrosshair(true);
            }

            // P - Play test sound
            if (action == 1 && (key == 'p' || key == 'P')) {
                SIMPLE_LOG("Playing test sound at (0,0,0)...");
                Core::Audio::AudioManager::Instance().PlaySound3D("assets/test.wav", {0, 0, 0}, 50.0f);
            }
        });
        
        window.SetCursorPosCallback([&](double dx, double dy){
            if (!mouseLocked) return;
            Cam.ProcessMouseMovement(static_cast<float>(dx), static_cast<float>(-dy));
        });
        
        window.SetMouseButtonCallback([&](int button, int action, int mods){
            if (button == 2 && action == 1) {
                mouseLocked = !mouseLocked;
                window.SetRelativeMouse(mouseLocked);
                window.SetCursorGrab(mouseLocked);
                window.ShowCursor(!mouseLocked);
                Renderer.SetShowCrosshair(mouseLocked);
            }
        });

        // Main loop
        auto last = std::chrono::high_resolution_clock::now();
        auto fpsLast = last;
        int frameCount = 0;
        float currentFPS = 0.0f;
        
        // Camera movement speed
        const float camSpeed = 5.0f;
        
        while (!window.ShouldClose()) {
            window.PollEvents();
            
            // Start new ImGui frame
            UISystem::Instance().NewFrame();
            
            auto nowTs = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(nowTs - last).count();
            dt = std::min(dt, 0.033f); // Cap at 30 FPS to avoid instability
            last = nowTs;
            
            // Camera movement (WASD or ZQSD depending on layout, plus arrow keys)
            // Use scancodes (position-based) instead of keycodes (character-based)
            if (mouseLocked) {
                // Forward: W key (scancode 26) or Up Arrow (scancode 82)
                if (window.IsKeyScanPressed(26) || window.IsKeyScanPressed(82)) {
                    Cam.Position += Cam.Front * camSpeed * dt;
                }
                // Backward: S key (scancode 22) or Down Arrow (scancode 81)
                if (window.IsKeyScanPressed(22) || window.IsKeyScanPressed(81)) {
                    Cam.Position -= Cam.Front * camSpeed * dt;
                }
                // Left: A key (scancode 4) or Left Arrow (scancode 80)
                if (window.IsKeyScanPressed(4) || window.IsKeyScanPressed(80)) {
                    Cam.Position -= Cam.Right * camSpeed * dt;
                }
                // Right: D key (scancode 7) or Right Arrow (scancode 79)
                if (window.IsKeyScanPressed(7) || window.IsKeyScanPressed(79)) {
                    Cam.Position += Cam.Right * camSpeed * dt;
                }
                // Up: Space (scancode 44)
                if (window.IsKeyScanPressed(44)) {
                    Cam.Position.y += camSpeed * dt;
                }
                // Down: Left Shift (scancode 225) or Left Ctrl (scancode 224)
                if (window.IsKeyScanPressed(225) || window.IsKeyScanPressed(224)) {
                    Cam.Position.y -= camSpeed * dt;
                }
            }
            
            // Update Audio Listener
            Core::Audio::Listener listener;
            listener.Position = Cam.Position;
            listener.Forward = Cam.Front;
            listener.Up = Cam.Up;
            Core::Audio::AudioManager::Instance().SetListener(listener);
            
            // Handle resize
            if (pendingResize) {
                if (std::chrono::duration<float, std::milli>(nowTs - lastResizeEvent).count() > 120.0f) {
                    Renderer.Resize(pendingW, pendingH);
                    pendingResize = false;
                }
            }
            
            // Update grabbed object position
            if (grabbedEntity != 0) {
                auto* rb = Registry.Has<Physics::RigidBody>(grabbedEntity) ? &Registry.Get<Physics::RigidBody>(grabbedEntity) : nullptr;
                if (rb) {
                    Vec3 targetPos = Cam.Position + Cam.Front * grabbedDistance;
                    Vec3 dir = targetPos - rb->Position;
                    float dist = dir.Magnitude();
                    
                    // Use a P-controller for velocity to move towards target
                    // v = (target - current) * kp, but clamp to avoid injecting large energy
                    const float kp = 6.0f;
                    Vec3 desired = dir * kp;
                    const float maxSpeed = 8.0f;
                    float speed = desired.Magnitude();
                    if (speed > maxSpeed) desired = desired * (maxSpeed / speed);
                    rb->Velocity = desired;
                    
                    // Dampen angular velocity to prevent spinning while held
                    rb->AngularVelocity = rb->AngularVelocity * 0.9f;
                    
                    // Wake up the body if it was sleeping (not implemented yet, but good practice)
                    rb->IsStatic = false; 
                }
            }
            
            // Fixed timestep accumulator
            static float accumulator = 0.0f;
            accumulator += dt;
            
            const float physicsStep = 1.0f / 60.0f;
            const int maxSubSteps = 5; // Prevent spiral of death
            int steps = 0;
            
            while (accumulator >= physicsStep && steps < maxSubSteps) {
                Physics::PhysicsSystem::Instance().Update(physicsStep);
                accumulator -= physicsStep;
                steps++;
            }
            
            // Sync physics positions to render scene
            SyncPhysicsToScene(Registry, DemoScene);
            
            // Render scene
            Renderer.Clear(Vec4(0.1f, 0.1f, 0.2f, 1.0f));
            Renderer.RenderScene(DemoScene, Cam);
            
            // Render Dear ImGui debug overlay
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Debug Info", nullptr, ImGuiWindowFlags_NoResize)) {
                ImGui::Text("Solstice Engine - Physics Playground");
                ImGui::Separator();
                
                ImGui::Text("FPS: %.1f", currentFPS);
                ImGui::Text("Frame Time: %.2f ms", dt * 1000.0f);
                ImGui::Separator();
                
                ImGui::Text("Camera Position:");
                ImGui::Text("  X: %.2f  Y: %.2f  Z: %.2f", Cam.Position.x, Cam.Position.y, Cam.Position.z);
                ImGui::Text("Camera Rotation:");
                ImGui::Text("  Yaw: %.1f  Pitch: %.1f", Cam.Yaw, Cam.Pitch);
                ImGui::Separator();
                
                ImGui::Text("Physics Objects: %zu", dynamicObjects.size());
                if (grabbedEntity != 0) {
                    ImGui::Text("Object Grabbed: Yes");
                } else {
                    ImGui::Text("Object Grabbed: No");
                }
                ImGui::Separator();
                
                ImGui::Text("Controls:");
                ImGui::BulletText("E - Pick up/Drop object");
                ImGui::BulletText("Right Mouse - Lock camera");
                ImGui::BulletText("WASD - Move camera");
            }
            ImGui::End();
            
            // Render ImGui
            UISystem::Instance().Render();
            
            // Present frame
            Renderer.Present();
            
            // FPS counter
            frameCount++;
            float secElapsed = std::chrono::duration<float>(nowTs - fpsLast).count();
            if (secElapsed >= 1.0f) {
                currentFPS = frameCount / secElapsed;
                frameCount = 0;
                fpsLast = nowTs;
                window.SetTitle("Solstice Physics Playground | " + std::to_string(static_cast<int>(currentFPS)) + " FPS");
            }
        }

        std::cout << "\nShutting down..." << std::endl;
        UISystem::Instance().Shutdown();
        Physics::PhysicsSystem::Instance().Stop();
        Core::Audio::AudioManager::Instance().Shutdown();
        Core::JobSystem::Instance().Shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}

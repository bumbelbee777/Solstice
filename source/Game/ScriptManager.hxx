#pragma once

#include "../Solstice.hxx"
#include "../Core/Async.hxx"
#include "../Scripting/BytecodeVM.hxx"
#include "../Scripting/Compiler.hxx"
#include "../Scripting/ScriptBindings.hxx"
#include "../Entity/Registry.hxx"
#include "../Render/Scene.hxx"
#include "../Render/Camera.hxx"
#include "../Physics/PhysicsSystem.hxx"
#include <functional>
#include <vector>
#include <string>
#include <filesystem>

namespace Solstice::Game {

// Deferred execution task
struct DeferredExecution {
    std::function<bool()> Condition;
    std::string ModuleName;
    uint32_t TimeoutMs;
    bool Executed{false};
};

// Script manager for seamless scripting integration
class SOLSTICE_API ScriptManager {
public:
    ScriptManager();
    ~ScriptManager();

    // Initialize script manager
    // ScriptDirectory: Directory containing .mw script files (tries multiple paths)
    // Registry, Scene, PhysicsSystem, Camera: Pointers for script bindings
    bool Initialize(
        const std::string& ScriptDirectory,
        ECS::Registry* Registry,
        Render::Scene* Scene,
        Physics::PhysicsSystem* PhysicsSystem,
        Render::Camera* Camera);

    // Register game-specific native function
    void RegisterNative(const std::string& Name, Scripting::BytecodeVM::NativeFunc Function);

    // Check if module is loaded
    bool HasModule(const std::string& ModuleName) const;

    // Execute module immediately (with timeout)
    bool ExecuteModule(const std::string& ModuleName, uint32_t TimeoutMs = 5000);

    // Execute module when condition is met (checked each Update())
    void ExecuteModuleWhen(
        std::function<bool()> Condition,
        const std::string& ModuleName,
        uint32_t TimeoutMs = 5000);

    // Update deferred executions (call from Update())
    void Update(float DeltaTime);

    // Shutdown and cleanup
    void Shutdown();

private:
    Scripting::BytecodeVM m_ScriptVM;
    Core::ExecutionGuard m_ExecutionGuard;
    mutable Core::Spinlock m_VMLock;

    std::vector<DeferredExecution> m_DeferredExecutions;

    ECS::Registry* m_Registry{nullptr};
    Render::Scene* m_Scene{nullptr};
    Physics::PhysicsSystem* m_PhysicsSystem{nullptr};
    Render::Camera* m_Camera{nullptr};
};

} // namespace Solstice::Game

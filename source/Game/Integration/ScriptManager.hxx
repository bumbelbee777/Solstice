#pragma once

#include "../../Solstice.hxx"
#include "../../Core/System/Async.hxx"
#include "../../Scripting/VM/BytecodeVM.hxx"
#include "../../Scripting/Compiler/Compiler.hxx"
#include "../../Scripting/Bindings/ScriptBindings.hxx"
#include "../../Entity/Registry.hxx"
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include "../../Physics/Integration/PhysicsSystem.hxx"
#include <functional>
#include <vector>
#include <string>
#include <filesystem>

namespace Solstice::Game {

class MatchScoreState;
class Leaderboard;
class AchievementState;
class ScoreMultiplierState;

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

    /// Optional: FFA / match `MatchScoreState` and `Leaderboard` for Moonwalk natives (`Match_*`, `Leaderboard_*`). Call
    /// before `Initialize` if scripts should use scores on first run; may be set later for natives that read pointers each call.
    void SetMatchContext(MatchScoreState* Scores, Leaderboard* Board);

    /// Optional achievements / score multiplier for `Achievement_*`, `ScoreMult_*`, and `Match_AddScore` scaling. May be set before `Initialize`.
    void SetProgressionContext(AchievementState* Achievements, ScoreMultiplierState* ScoreMult);

    /// Per-frame delta (seconds) for `Time_FrameDelta` and UI scripts; set from the game loop (e.g. `Update` / before `RunModuleExport`).
    void SetScriptTimeDelta(float DeltaSeconds);

    /// Run a named export from a compiled module (e.g. `OnFrame` for UI). Restores the previously active program. Thread-safe with the script VM lock.
    bool RunModuleExport(const std::string& ModuleName, const std::string& ExportName);

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

    // Start a coroutine (script function at entryIP in program). Called from script via Coroutine.Start.
    void StartCoroutine(const Scripting::Program& program, size_t entryIP, const std::vector<Scripting::Value>& args);

    // Update deferred executions and resume due coroutines (call from game Update())
    void Update(float DeltaTime);

    // Shutdown and cleanup
    void Shutdown();

    // Access VM for event/callback registration (e.g. from bindings)
    Scripting::BytecodeVM& GetVM() { return m_ScriptVM; }
    const Scripting::BytecodeVM& GetVM() const { return m_ScriptVM; }

private:
    void RegisterMatchScriptBindings();
    void RegisterProgressionAndUiScriptBindings();

    Scripting::BytecodeVM m_ScriptVM;
    Core::ExecutionGuard m_ExecutionGuard;
    mutable Core::Spinlock m_VMLock;

    std::vector<DeferredExecution> m_DeferredExecutions;

    // Coroutines (suspended VM state, resumed each frame when due)
    std::vector<Scripting::CoroutineState> m_Coroutines;
    double m_GameTime{0.0};
    double m_ScriptFrameDelta{0.0};
    uint64_t m_FrameCount{0};

    ECS::Registry* m_Registry{nullptr};
    Render::Scene* m_Scene{nullptr};
    Physics::PhysicsSystem* m_PhysicsSystem{nullptr};
    Render::Camera* m_Camera{nullptr};

    MatchScoreState* m_MatchScores{nullptr};
    Leaderboard* m_Leaderboard{nullptr};

    AchievementState* m_Achievements{nullptr};
    ScoreMultiplierState* m_ScoreMult{nullptr};
};

} // namespace Solstice::Game

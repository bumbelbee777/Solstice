#include "Integration/ScriptManager.hxx"
#include "Dialogue/NarrativeScriptBindings.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Scripting/Bindings/NativeBinding.hxx"
#include <Plugin/SubsystemHooks.hxx>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

namespace Solstice::Game {

ScriptManager::ScriptManager() {
}

ScriptManager::~ScriptManager() {
    Shutdown();
}

bool ScriptManager::Initialize(
    const std::string& ScriptDirectory,
    ECS::Registry* Registry,
    Render::Scene* Scene,
    Physics::PhysicsSystem* PhysicsSystem,
    Render::Camera* Camera) {

    m_Registry = Registry;
    m_Scene = Scene;
    m_PhysicsSystem = PhysicsSystem;
    m_Camera = Camera;

    // Register standard script bindings
    Scripting::RegisterScriptBindings(m_ScriptVM, Registry, Scene, PhysicsSystem, Camera);
    RegisterNarrativeScriptBindings(m_ScriptVM);
    m_ScriptVM.EnableJIT();
    SIMPLE_LOG("ScriptManager: Script bindings registered");

    // Coroutine and wait natives (require ScriptManager to start coroutines / request yield)
    Scripting::NativeBinding::Register<void, int64_t>(m_ScriptVM, "WaitFrames", [this](int64_t frames) {
        m_ScriptVM.RequestYieldFrames(frames);
    });
    Scripting::NativeBinding::Register<void, double>(m_ScriptVM, "WaitSeconds", [this](double seconds) {
        m_ScriptVM.RequestYieldSeconds(seconds);
    });
    Scripting::NativeBinding::Register<void, Scripting::Value>(m_ScriptVM, "Coroutine.Start", [this](const Scripting::Value& value) {
        if (!std::holds_alternative<Scripting::ScriptFunc>(value)) {
            return;
        }
        const auto& sf = std::get<Scripting::ScriptFunc>(value);
        StartCoroutine(m_ScriptVM.GetProgram(), sf.entryIP, {});
    });
    Scripting::NativeBinding::Register<void, Scripting::Value>(m_ScriptVM, "WaitUntil", [this](const Scripting::Value& value) {
        if (!std::holds_alternative<Scripting::ScriptFunc>(value)) {
            return;
        }
        m_ScriptVM.RequestYieldUntil(std::get<Scripting::ScriptFunc>(value));
    });

    // Try multiple possible script directory paths
    std::vector<std::filesystem::path> ScriptDirCandidates = {
        ScriptDirectory,
        "scripts",
        "example/Blizzard/scripts",
        "../example/Blizzard/scripts",
        "../../example/Blizzard/scripts"
    };

    std::filesystem::path ScriptDir;
    bool Found = false;
    for (const auto& Candidate : ScriptDirCandidates) {
        if (std::filesystem::exists(Candidate)) {
            ScriptDir = Candidate;
            Found = true;
            SIMPLE_LOG("Found script directory at: " + ScriptDir.string());
            break;
        }
    }

    if (!Found) {
        SIMPLE_LOG("ERROR: Script directory not found. Tried:");
        for (const auto& Candidate : ScriptDirCandidates) {
            SIMPLE_LOG("  - " + Candidate.string());
        }
        return false;
    }

    // Compile and load scripts
    Scripting::Compiler Compiler;
    try {
        auto Modules = Compiler.BatchCompile(ScriptDir);
        SIMPLE_LOG("BatchCompile returned " + std::to_string(Modules.size()) + " modules");

        if (Modules.empty()) {
            SIMPLE_LOG("WARNING: No modules were compiled. Check for syntax errors in .mw files.");
            return false;
        }

        for (const auto& [Name, Prog] : Modules) {
            m_ScriptVM.AddModule(Name, Prog);
            std::string ExportList = "exports: ";
            for (const auto& [ExpName, ExpAddr] : Prog.Exports) {
                ExportList += ExpName + " ";
            }
            SIMPLE_LOG("Loaded Moonwalk module: " + Name + " (has " + std::to_string(Prog.Exports.size()) + " " + ExportList + ")");
        }

        SIMPLE_LOG("Total modules registered in VM: " + std::to_string(Modules.size()));
        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: BatchCompile failed: " + std::string(e.what()));
        return false;
    } catch (...) {
        SIMPLE_LOG("ERROR: Unknown exception during BatchCompile");
        return false;
    }
}

void ScriptManager::RegisterNative(const std::string& Name, Scripting::BytecodeVM::NativeFunc Function) {
    Core::LockGuard Guard(m_VMLock);
    m_ScriptVM.RegisterNative(Name, Function);
}

bool ScriptManager::HasModule(const std::string& ModuleName) const {
    Core::LockGuard Guard(m_VMLock);
    return m_ScriptVM.HasModule(ModuleName);
}

bool ScriptManager::ExecuteModule(const std::string& ModuleName, uint32_t TimeoutMs) {
    if (!m_ExecutionGuard.TryExecute(TimeoutMs)) {
        SIMPLE_LOG("WARNING: Script execution already in progress or timed out, skipping " + ModuleName);
        return false;
    }

    try {
        Core::LockGuard Guard(m_VMLock);
        if (!m_ScriptVM.HasModule(ModuleName)) {
            SIMPLE_LOG("WARNING: Module " + ModuleName + " not found in VM");
            m_ExecutionGuard.Release();
            return false;
        }

        SIMPLE_LOG("Executing module: " + ModuleName);
        m_ScriptVM.LoadProgram(m_ScriptVM.GetModule(ModuleName));
        Scripting::CoroutineState state;
        Scripting::RunResult r = m_ScriptVM.Run(&state);
        if (r == Scripting::RunResult::Yielded) {
            if (state.yieldRequest.type == Scripting::YieldRequest::Type::Seconds)
                state.yieldRequest.resumeAtTime = m_GameTime + state.yieldRequest.secondsDelay;
            m_Coroutines.push_back(std::move(state));
        }
        SIMPLE_LOG("Module " + ModuleName + " executed successfully");
        m_ExecutionGuard.Release();
        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Exception in script execution (" + ModuleName + "): " + std::string(e.what()));
        m_ExecutionGuard.Release();
        return false;
    } catch (...) {
        SIMPLE_LOG("ERROR: Unknown exception in script execution (" + ModuleName + ")");
        m_ExecutionGuard.Release();
        return false;
    }
}

void ScriptManager::StartCoroutine(const Scripting::Program& program, size_t entryIP, const std::vector<Scripting::Value>& args) {
    Scripting::CoroutineState state;
    state.program = program;
    state.IP = entryIP;
    state.stack = args;
    state.callStack.clear();
    state.registers = {};
    state.yieldRequest = Scripting::YieldRequest{};
    Core::LockGuard Guard(m_VMLock);
    m_Coroutines.push_back(std::move(state));
}

void ScriptManager::ExecuteModuleWhen(
    std::function<bool()> Condition,
    const std::string& ModuleName,
    uint32_t TimeoutMs) {

    DeferredExecution Task;
    Task.Condition = Condition;
    Task.ModuleName = ModuleName;
    Task.TimeoutMs = TimeoutMs;
    Task.Executed = false;

    m_DeferredExecutions.push_back(Task);
    SIMPLE_LOG("Scheduled deferred execution for module: " + ModuleName);
}

void ScriptManager::Update(float DeltaTime) {
    m_GameTime += static_cast<double>(DeltaTime);
    m_FrameCount++;

    // Tick coroutines (decrement frame counters)
    for (auto& s : m_Coroutines)
        s.tick(m_FrameCount, m_GameTime);

    // Resume due coroutines
    for (auto it = m_Coroutines.begin(); it != m_Coroutines.end(); ) {
        bool due = it->isDue(m_FrameCount, m_GameTime);
        if (!due && it->yieldRequest.type == Scripting::YieldRequest::Type::Condition) {
            try {
                Core::LockGuard Guard(m_VMLock);
                due = m_ScriptVM.RunCondition(it->program, it->yieldRequest.conditionFunc);
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: WaitUntil condition exception: " + std::string(e.what()));
                it = m_Coroutines.erase(it);
                continue;
            }
        }
        if (!due) {
            ++it;
            continue;
        }
        Scripting::RunResult r = Scripting::RunResult::Completed;
        try {
            Core::LockGuard Guard(m_VMLock);
            r = m_ScriptVM.RunFromState(*it);
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Coroutine exception: " + std::string(e.what()));
            it = m_Coroutines.erase(it);
            continue;
        }
        if (r == Scripting::RunResult::Completed)
            it = m_Coroutines.erase(it);
        else {
            if (r == Scripting::RunResult::Yielded && it->yieldRequest.type == Scripting::YieldRequest::Type::Seconds)
                it->yieldRequest.resumeAtTime = m_GameTime + it->yieldRequest.secondsDelay;
            ++it;
        }
    }

    // Check deferred execution conditions
    for (auto& Task : m_DeferredExecutions) {
        if (!Task.Executed && Task.Condition()) {
            ExecuteModule(Task.ModuleName, Task.TimeoutMs);
            Task.Executed = true;
        }
    }

    // Remove executed tasks
    m_DeferredExecutions.erase(
        std::remove_if(m_DeferredExecutions.begin(), m_DeferredExecutions.end(),
            [](const DeferredExecution& T) { return T.Executed; }),
        m_DeferredExecutions.end());

    Solstice::Plugin::SubsystemHooks::Instance().Invoke(Solstice::Plugin::SubsystemHookKind::ScriptingPostUpdate, DeltaTime);
}

void ScriptManager::Shutdown() {
    // Wait for any ongoing script execution
    if (m_ExecutionGuard.IsExecuting()) {
        SIMPLE_LOG("ScriptManager: Waiting for script execution to complete...");
        int MaxWait = 100; // 10 seconds
        while (m_ExecutionGuard.IsExecuting() && MaxWait > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            MaxWait--;
        }
        if (m_ExecutionGuard.IsExecuting()) {
            SIMPLE_LOG("WARNING: Script execution did not complete, forcing release");
            m_ExecutionGuard.Release();
        }
    }

    m_DeferredExecutions.clear();
    m_Coroutines.clear();
    SIMPLE_LOG("ScriptManager: Shutdown complete");
}

} // namespace Solstice::Game

#include "Integration/ScriptManager.hxx"
#include "Dialogue/NarrativeScriptBindings.hxx"
#include "Match/Leaderboard.hxx"
#include "Match/MatchScoreState.hxx"
#include "Progression/AchievementState.hxx"
#include "Progression/ScoreMultiplierState.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Scripting/Bindings/NativeBinding.hxx"
#include <Plugin/SubsystemHooks.hxx>
#include <imgui.h>
#include <cmath>
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
    Scripting::NativeBinding::Register<int64_t, std::string, std::string>(m_ScriptVM, "Coroutine.StartExport",
        [this](const std::string& moduleName, const std::string& exportName) -> int64_t {
            if (!m_ScriptVM.HasModule(moduleName)) {
                return 0;
            }
            const Scripting::Program& mod = m_ScriptVM.GetModule(moduleName);
            const auto it = mod.Exports.find(exportName);
            if (it == mod.Exports.end()) {
                return 0;
            }
            StartCoroutine(mod, it->second, {});
            return 1;
        });
    Scripting::NativeBinding::Register<int64_t>(m_ScriptVM, "Coroutine.ActiveCount", [this]() -> int64_t {
        return static_cast<int64_t>(m_Coroutines.size());
    });
    Scripting::NativeBinding::Register<void>(m_ScriptVM, "Coroutine.StopAll", [this]() {
        m_Coroutines.clear();
    });
    Scripting::NativeBinding::Register<void, Scripting::Value>(m_ScriptVM, "WaitUntil", [this](const Scripting::Value& value) {
        if (!std::holds_alternative<Scripting::ScriptFunc>(value)) {
            return;
        }
        m_ScriptVM.RequestYieldUntil(std::get<Scripting::ScriptFunc>(value));
    });

    RegisterMatchScriptBindings();
    RegisterProgressionAndUiScriptBindings();

    // Try multiple possible script directory paths
    std::vector<std::filesystem::path> ScriptDirCandidates = {
        ScriptDirectory,
        "scripts",
        "example/Blizzard/scripts",
        "../example/Blizzard/scripts",
        "../../example/Blizzard/scripts",
        "example/WarsOfHeaven/scripts",
        "../example/WarsOfHeaven/scripts",
        "../../example/WarsOfHeaven/scripts"
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
    m_ScriptFrameDelta = static_cast<double>(DeltaTime);
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

void ScriptManager::SetMatchContext(MatchScoreState* Scores, Leaderboard* Board) {
    m_MatchScores = Scores;
    m_Leaderboard = Board;
}

void ScriptManager::SetProgressionContext(AchievementState* Achievements, ScoreMultiplierState* ScoreMult) {
    m_Achievements = Achievements;
    m_ScoreMult = ScoreMult;
}

void ScriptManager::SetScriptTimeDelta(float DeltaSeconds) {
    m_ScriptFrameDelta = static_cast<double>(DeltaSeconds);
}

bool ScriptManager::RunModuleExport(const std::string& ModuleName, const std::string& ExportName) {
    Core::LockGuard Guard(m_VMLock);
    if (!m_ScriptVM.HasModule(ModuleName)) {
        return false;
    }
    const Scripting::Program& mod = m_ScriptVM.GetModule(ModuleName);
    const auto expIt = mod.Exports.find(ExportName);
    if (expIt == mod.Exports.end()) {
        return false;
    }
    const Scripting::Program saved = m_ScriptVM.GetProgram();
    try {
        m_ScriptVM.LoadProgram(mod);
        (void)m_ScriptVM.RunFunctionSlice(expIt->second, {});
    } catch (const std::exception& e) {
        m_ScriptVM.LoadProgram(saved);
        SIMPLE_LOG(std::string("RunModuleExport exception: ") + e.what());
        return false;
    } catch (...) {
        m_ScriptVM.LoadProgram(saved);
        SIMPLE_LOG("RunModuleExport: unknown exception");
        return false;
    }
    m_ScriptVM.LoadProgram(saved);
    return true;
}

void ScriptManager::RegisterMatchScriptBindings() {
    using namespace Scripting::NativeBinding;
    Register<int64_t, int64_t>(m_ScriptVM, "Match_GetScore", [this](int64_t pid) -> int64_t {
        if (!m_MatchScores) {
            return 0;
        }
        return static_cast<int64_t>(m_MatchScores->GetScore(static_cast<std::uint32_t>(pid)));
    });
    Register<void, int64_t, int64_t>(m_ScriptVM, "Match_SetScore", [this](int64_t pid, int64_t score) {
        if (m_MatchScores) {
            m_MatchScores->SetScore(static_cast<std::uint32_t>(pid), score);
        }
    });
    Register<void, int64_t, int64_t>(m_ScriptVM, "Match_AddScore", [this](int64_t pid, int64_t delta) {
        if (!m_MatchScores) {
            return;
        }
        std::int64_t add = delta;
        if (m_ScoreMult) {
            const double scaled = static_cast<double>(delta) * static_cast<double>(m_ScoreMult->GetEffective());
            add = static_cast<std::int64_t>(std::llround(scaled));
        }
        m_MatchScores->AddScore(static_cast<std::uint32_t>(pid), add);
    });
    Register<void, int64_t, int64_t>(m_ScriptVM, "Match_SetPlayerTeam", [this](int64_t pid, int64_t teamId) {
        if (m_MatchScores) {
            m_MatchScores->SetPlayerTeam(static_cast<std::uint32_t>(pid), static_cast<std::uint8_t>(teamId));
        }
    });
    Register<int64_t, int64_t>(m_ScriptVM, "Match_GetPlayerTeam", [this](int64_t pid) -> int64_t {
        if (!m_MatchScores) {
            return 0;
        }
        return static_cast<int64_t>(m_MatchScores->GetPlayerTeam(static_cast<std::uint32_t>(pid)));
    });
    Register<int64_t, int64_t>(m_ScriptVM, "Match_GetTeamScore", [this](int64_t teamId) -> int64_t {
        if (!m_MatchScores) {
            return 0;
        }
        return m_MatchScores->GetTeamScore(static_cast<std::uint8_t>(teamId));
    });
    Register<double, std::string>(m_ScriptVM, "Match_CaptureGetProgress", [this](const std::string& zone) -> double {
        if (!m_MatchScores) {
            return 0.0;
        }
        CaptureZoneProgress p{};
        if (!m_MatchScores->GetZoneProgress(zone, p)) {
            return 0.0;
        }
        return static_cast<double>(p.Progress01);
    });
    Register<int64_t, std::string>(m_ScriptVM, "Match_CaptureGetLeader", [this](const std::string& zone) -> int64_t {
        if (!m_MatchScores) {
            return 0;
        }
        CaptureZoneProgress p{};
        if (!m_MatchScores->GetZoneProgress(zone, p)) {
            return 0;
        }
        return static_cast<int64_t>(p.LeadingPlayerId);
    });
    Register<void, std::string, int64_t, double>(m_ScriptVM, "Match_CaptureSet", [this](const std::string& zone, int64_t lead,
                                                                                     double progr) {
        if (!m_MatchScores) {
            return;
        }
        CaptureZoneProgress p;
        p.ZoneId = zone;
        p.LeadingPlayerId = static_cast<std::uint32_t>(lead);
        p.Progress01 = static_cast<float>(progr);
        m_MatchScores->SetZoneProgress(p);
    });
    Register<void>(m_ScriptVM, "Leaderboard_Refresh", [this]() {
        if (m_MatchScores && m_Leaderboard) {
            m_Leaderboard->Refresh(*m_MatchScores);
        }
    });
    Register<int64_t>(m_ScriptVM, "Leaderboard_Size", [this]() -> int64_t {
        if (!m_Leaderboard) {
            return 0;
        }
        return static_cast<int64_t>(m_Leaderboard->GetRows().size());
    });
    Register<int64_t, int64_t>(m_ScriptVM, "Leaderboard_RowPlayerId", [this](int64_t index) -> int64_t {
        if (!m_Leaderboard) {
            return 0;
        }
        const auto& r = m_Leaderboard->GetRows();
        if (index < 0 || static_cast<std::size_t>(index) >= r.size()) {
            return 0;
        }
        return static_cast<int64_t>(r[static_cast<std::size_t>(index)].MatchPlayerId);
    });
    Register<int64_t, int64_t>(m_ScriptVM, "Leaderboard_RowScore", [this](int64_t index) -> int64_t {
        if (!m_Leaderboard) {
            return 0;
        }
        const auto& r = m_Leaderboard->GetRows();
        if (index < 0 || static_cast<std::size_t>(index) >= r.size()) {
            return 0;
        }
        return r[static_cast<std::size_t>(index)].Score;
    });
}

void ScriptManager::RegisterProgressionAndUiScriptBindings() {
    using namespace Scripting::NativeBinding;
    Register<double>(m_ScriptVM, "Time_FrameDelta", [this]() { return m_ScriptFrameDelta; });
    Register<double>(m_ScriptVM, "Time_GameTime", [this]() { return m_GameTime; });
    Register<int64_t>(m_ScriptVM, "Time_FrameCount", [this]() { return static_cast<int64_t>(m_FrameCount); });

    Register<int64_t, std::string>(m_ScriptVM, "Events.HandlerCount", [this](const std::string& eventName) -> int64_t {
        return static_cast<int64_t>(m_ScriptVM.GetEventHandlerCount(eventName));
    });
    Register<void, std::string>(m_ScriptVM, "Events.Clear", [this](const std::string& eventName) {
        m_ScriptVM.ClearEventHandlers(eventName);
    });
    Register<void>(m_ScriptVM, "Events.ClearAll", [this]() {
        m_ScriptVM.ClearAllEventHandlers();
    });

    Register<void, std::string, int64_t>(m_ScriptVM, "Achievement_Configure", [this](const std::string& id, int64_t target) {
        if (m_Achievements) {
            m_Achievements->Configure(id, target);
        }
    });
    Register<void, std::string, int64_t>(m_ScriptVM, "Achievement_SetProgress", [this](const std::string& id, int64_t value) {
        if (m_Achievements) {
            m_Achievements->SetProgress(id, value);
        }
    });
    Register<void, std::string, int64_t>(m_ScriptVM, "Achievement_AddProgress", [this](const std::string& id, int64_t delta) {
        if (m_Achievements) {
            m_Achievements->AddProgress(id, delta);
        }
    });
    Register<void, std::string>(m_ScriptVM, "Achievement_Unlock", [this](const std::string& id) {
        if (m_Achievements) {
            m_Achievements->Unlock(id);
        }
    });
    Register<int64_t, std::string>(m_ScriptVM, "Achievement_IsUnlocked", [this](const std::string& id) -> int64_t {
        if (!m_Achievements) {
            return 0;
        }
        return m_Achievements->IsUnlocked(id) ? 1 : 0;
    });
    Register<int64_t, std::string>(m_ScriptVM, "Achievement_GetProgress", [this](const std::string& id) -> int64_t {
        if (!m_Achievements) {
            return 0;
        }
        return m_Achievements->GetProgress(id);
    });
    Register<int64_t, std::string>(m_ScriptVM, "Achievement_GetTarget", [this](const std::string& id) -> int64_t {
        if (!m_Achievements) {
            return 0;
        }
        return m_Achievements->GetTarget(id);
    });

    Register<void, double>(m_ScriptVM, "ScoreMult_SetBase", [this](double base) {
        if (m_ScoreMult) {
            m_ScoreMult->SetBase(static_cast<float>(base));
        }
    });
    Register<void, double>(m_ScriptVM, "ScoreMult_SetSession", [this](double session) {
        if (m_ScoreMult) {
            m_ScoreMult->SetSession(static_cast<float>(session));
        }
    });
    Register<double>(m_ScriptVM, "ScoreMult_GetEffective", [this]() {
        if (!m_ScoreMult) {
            return 1.0;
        }
        return static_cast<double>(m_ScoreMult->GetEffective());
    });
    Register<void>(m_ScriptVM, "ScoreMult_Clear", [this]() {
        if (m_ScoreMult) {
            m_ScoreMult->Clear();
        }
    });

    Register<void, std::string, double, double>(m_ScriptVM, "UI_Begin", [](const std::string& title, double w, double h) {
        if (!ImGui::GetCurrentContext()) {
            return;
        }
        if (const ImGuiViewport* vp = ImGui::GetMainViewport()) {
            const ImVec2 c = vp->GetWorkCenter();
            ImGui::SetNextWindowPos(c, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        }
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_NoCollapse);
    });
    Register<void>(m_ScriptVM, "UI_End", []() {
        if (ImGui::GetCurrentContext()) {
            ImGui::End();
        }
    });
    Register<void, std::string>(m_ScriptVM, "UI_Text", [](const std::string& s) {
        if (ImGui::GetCurrentContext()) {
            ImGui::TextUnformatted(s.c_str());
        }
    });
    Register<int64_t, std::string>(m_ScriptVM, "UI_Button", [](const std::string& label) -> int64_t {
        if (!ImGui::GetCurrentContext()) {
            return 0;
        }
        return ImGui::Button(label.c_str()) ? 1 : 0;
    });
}

} // namespace Solstice::Game

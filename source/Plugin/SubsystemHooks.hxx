#pragma once

#include <Solstice.hxx>

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace Solstice::Plugin {

/**
 * Well-known integration points for native plugins and in-process tools.
 * Renderer hooks receive DeltaTime 0 when the render path does not track frame dt.
 * ECS: EcsPreTick/EcsPostTick wrap PhaseScheduler::ExecuteAll; per-phase hooks wrap ExecutePhase.
 * Networking: PrePoll/PostPoll wrap NetworkingSystem::Poll (delta 0 if not available).
 * Profiling: PreFrame/PostFrame wrap Core::Profiler BeginFrame/EndFrame in GameBase (frame delta time).
 */
enum class SubsystemHookKind : std::uint32_t {
    RendererPreFrame = 0,
    RendererPostFrame,
    GamePreUpdate,
    GamePostUpdate,
    GamePreRender,
    GamePostRender,
    PhysicsPreStep,
    PhysicsPostStep,
    NetworkingPrePoll,
    NetworkingPostPoll,
    ProfilingPreFrame,
    ProfilingPostFrame,
    ScriptingPreUpdate,
    ScriptingPostUpdate,
    EcsPreTick,
    EcsPostTick,
    EcsPrePhaseInput,
    EcsPostPhaseInput,
    EcsPrePhaseSimulation,
    EcsPostPhaseSimulation,
    EcsPrePhasePresentation,
    EcsPostPhasePresentation,
    EcsPrePhaseLate,
    EcsPostPhaseLate,
    Count
};

class SOLSTICE_API SubsystemHooks {
public:
    static SubsystemHooks& Instance();

    SubsystemHooks(const SubsystemHooks&) = delete;
    SubsystemHooks& operator=(const SubsystemHooks&) = delete;

    using HookFn = std::function<void(float /*deltaTime*/)>;

    /** Opaque handle for Unregister. */
    using HookHandle = std::uint64_t;

    /**
     * Register a callback for a subsystem hook. Thread-safe.
     * Returns 0 on failure (invalid kind or empty function).
     */
    HookHandle Register(SubsystemHookKind kind, HookFn fn);

    /** Remove a previously registered hook. Thread-safe. */
    void Unregister(HookHandle handle);

    void Clear();

    /** Invoke all hooks for `kind` in registration order. Safe if callbacks register/unregister others (next frame). */
    void Invoke(SubsystemHookKind kind, float deltaTime);

private:
    SubsystemHooks() = default;

    struct Entry {
        HookHandle Id{0};
        HookFn Fn;
    };

    mutable std::mutex m_Mutex;
    HookHandle m_NextId{1};
    std::vector<Entry> m_Buckets[static_cast<std::size_t>(SubsystemHookKind::Count)];
};

} // namespace Solstice::Plugin

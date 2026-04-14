#include <Entity/Scheduler.hxx>
#include <Plugin/SubsystemHooks.hxx>

namespace Solstice::ECS {

namespace {

Solstice::Plugin::SubsystemHookKind PhasePreKind(SystemPhase phase) {
    using SK = Solstice::Plugin::SubsystemHookKind;
    switch (phase) {
    case SystemPhase::Input:
        return SK::EcsPrePhaseInput;
    case SystemPhase::Simulation:
        return SK::EcsPrePhaseSimulation;
    case SystemPhase::Presentation:
        return SK::EcsPrePhasePresentation;
    case SystemPhase::Late:
        return SK::EcsPrePhaseLate;
    default:
        return SK::EcsPrePhaseSimulation;
    }
}

Solstice::Plugin::SubsystemHookKind PhasePostKind(SystemPhase phase) {
    using SK = Solstice::Plugin::SubsystemHookKind;
    switch (phase) {
    case SystemPhase::Input:
        return SK::EcsPostPhaseInput;
    case SystemPhase::Simulation:
        return SK::EcsPostPhaseSimulation;
    case SystemPhase::Presentation:
        return SK::EcsPostPhasePresentation;
    case SystemPhase::Late:
        return SK::EcsPostPhaseLate;
    default:
        return SK::EcsPostPhaseSimulation;
    }
}

} // namespace

void PhaseScheduler::ExecutePhase(SystemPhase phase, Registry& registry, float deltaTime) {
    auto& hooks = Solstice::Plugin::SubsystemHooks::Instance();
    hooks.Invoke(PhasePreKind(phase), deltaTime);
    for (Entry& entry : m_Entries) {
        if (entry.Phase == phase && entry.Execute) {
            entry.Execute(registry, deltaTime);
        }
    }
    hooks.Invoke(PhasePostKind(phase), deltaTime);
}

void PhaseScheduler::ExecuteAll(Registry& registry, float deltaTime) {
    auto& hooks = Solstice::Plugin::SubsystemHooks::Instance();
    hooks.Invoke(Solstice::Plugin::SubsystemHookKind::EcsPreTick, deltaTime);
    ExecutePhase(SystemPhase::Input, registry, deltaTime);
    ExecutePhase(SystemPhase::Simulation, registry, deltaTime);
    ExecutePhase(SystemPhase::Presentation, registry, deltaTime);
    ExecutePhase(SystemPhase::Late, registry, deltaTime);
    hooks.Invoke(Solstice::Plugin::SubsystemHookKind::EcsPostTick, deltaTime);
}

} // namespace Solstice::ECS

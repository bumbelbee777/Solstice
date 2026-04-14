#include "Entity/Registry.hxx"
#include "Entity/Kind.hxx"
#include "Entity/Name.hxx"
#include "Entity/Transform.hxx"
#include "Entity/PlayerFactory.hxx"
#include "Entity/PlayerTag.hxx"
#include "Entity/Scheduler.hxx"
#include "Physics/Dynamics/RigidBody.hxx"

#include <cstdio>

namespace {

int Fail(int code, const char* message) {
    std::fprintf(stderr, "[ECSRegistryTest] FAIL (%d): %s\n", code, message);
    return code;
}

} // namespace

int main() {
    Solstice::ECS::Registry registry;

    const Solstice::ECS::EntityId e1 = registry.Create();
    const Solstice::ECS::EntityId e2 = registry.Create();
    if (!registry.Valid(e1) || !registry.Valid(e2)) {
        return Fail(1, "Created entities should be valid");
    }

    registry.Add<Solstice::ECS::Transform>(e1);
    registry.Add<Solstice::ECS::Name>(e1, Solstice::ECS::Name{"EntityOne"});
    registry.Add<Solstice::ECS::Kind>(e1, Solstice::ECS::Kind{Solstice::ECS::EntityKind::Player});

    registry.Add<Solstice::ECS::Transform>(e2);
    registry.Add<Solstice::ECS::Name>(e2, Solstice::ECS::Name{"EntityTwo"});
    registry.Add<Solstice::Physics::RigidBody>(e2);

    if (!registry.HasAll<Solstice::ECS::Transform, Solstice::ECS::Name>(e1)) {
        return Fail(2, "HasAll failed for entity one");
    }
    if (!registry.HasAny<Solstice::Physics::RigidBody, Solstice::ECS::Kind>(e2)) {
        return Fail(3, "HasAny failed for entity two");
    }

    auto* maybeName = registry.TryGet<Solstice::ECS::Name>(e1);
    if (!maybeName || maybeName->Value != "EntityOne") {
        return Fail(4, "TryGet should return entity name");
    }

    int dualCount = 0;
    registry.ForEach<Solstice::ECS::Transform, Solstice::ECS::Name>([&](Solstice::ECS::EntityId id, Solstice::ECS::Transform&, Solstice::ECS::Name& name) {
        if ((id == e1 && name.Value == "EntityOne") || (id == e2 && name.Value == "EntityTwo")) {
            ++dualCount;
        }
    });
    if (dualCount != 2) {
        return Fail(5, "Variadic ForEach should enumerate both entities");
    }

    int filteredCount = 0;
    registry.ForEachFiltered<Solstice::ECS::Transform, Solstice::ECS::Name>(
        Solstice::ECS::Registry::Exclude<Solstice::Physics::RigidBody>{},
        [&](Solstice::ECS::EntityId id, Solstice::ECS::Transform&, Solstice::ECS::Name&) {
            if (id == e1) {
                ++filteredCount;
            }
        });
    if (filteredCount != 1) {
        return Fail(6, "ForEachFiltered should exclude rigid body entities");
    }

    Solstice::ECS::PhaseScheduler scheduler;
    int phaseOrder = 0;
    scheduler.Register(Solstice::ECS::SystemPhase::Input, "InputPhase", [&](Solstice::ECS::Registry&, float) {
        if (phaseOrder == 0) {
            phaseOrder = 1;
        }
    });
    scheduler.Register(Solstice::ECS::SystemPhase::Simulation, "SimulationPhase", [&](Solstice::ECS::Registry&, float) {
        if (phaseOrder == 1) {
            phaseOrder = 2;
        }
    });
    scheduler.Register(Solstice::ECS::SystemPhase::Late, "LatePhase", [&](Solstice::ECS::Registry&, float) {
        if (phaseOrder == 2) {
            phaseOrder = 3;
        }
    });
    scheduler.ExecuteAll(registry, 0.016f);
    if (phaseOrder != 3) {
        return Fail(9, "PhaseScheduler should execute phases in deterministic order");
    }

    const Solstice::ECS::EntityId defaultPlayer = Solstice::ECS::EnsureDefaultPlayer(registry);
    if (!registry.Valid(defaultPlayer)
        || !registry.HasAll<Solstice::ECS::Transform, Solstice::ECS::Name, Solstice::ECS::Kind, Solstice::ECS::PlayerTag>(defaultPlayer)) {
        return Fail(7, "PlayerFactory should build pure ECS default player");
    }

    registry.Destroy(e1);
    if (registry.Valid(e1) || registry.TryGet<Solstice::ECS::Name>(e1) != nullptr) {
        return Fail(8, "Destroy should invalidate entity and remove components");
    }

    std::printf("[ECSRegistryTest] PASS\n");
    return 0;
}

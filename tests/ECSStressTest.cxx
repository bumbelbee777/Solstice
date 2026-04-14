#include "TestHarness.hxx"
#include "Entity/Registry.hxx"
#include "Entity/Transform.hxx"
#include "Entity/Name.hxx"
#include "Entity/Kind.hxx"
#include "Physics/Dynamics/RigidBody.hxx"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace {

bool EnvFlag(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

int EnvInt(const char* name, int defaultValue) {
    const char* e = std::getenv(name);
    if (!e || e[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    const long v = std::strtol(e, &end, 10);
    if (end == e || v < 1) {
        return defaultValue;
    }
    return static_cast<int>(v);
}

bool FiniteVec3(const Solstice::Math::Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool RunEcsStress() {
    const bool torture = EnvFlag("SOLSTICE_STRESS_TORTURE");
    const int entities = EnvInt("SOLSTICE_ECS_STRESS_ENTITIES", torture ? 65536 : 8192);
    const int rounds = EnvInt("SOLSTICE_ECS_STRESS_ROUNDS", torture ? 16 : 4);

    Solstice::ECS::Registry registry;
    std::mt19937 rng(42);

    for (int r = 0; r < rounds; ++r) {
        std::vector<Solstice::ECS::EntityId> ids;
        ids.reserve(static_cast<size_t>(entities));
        for (int i = 0; i < entities; ++i) {
            const Solstice::ECS::EntityId e = registry.Create();
            ids.push_back(e);
            auto& t = registry.Add<Solstice::ECS::Transform>(e);
            t.Position = Solstice::Math::Vec3(
                std::generate_canonical<float, 10>(rng) * 100.0f - 50.0f,
                std::generate_canonical<float, 10>(rng) * 100.0f - 50.0f,
                std::generate_canonical<float, 10>(rng) * 100.0f - 50.0f);
            if ((i & 3) == 0) {
                registry.Add<Solstice::ECS::Name>(e, Solstice::ECS::Name{"Stress"});
            }
            if ((i & 7) == 1) {
                registry.Add<Solstice::ECS::Kind>(e, Solstice::ECS::Kind{Solstice::ECS::EntityKind::Environment});
            }
            if ((i & 15) == 2) {
                auto& rb = registry.Add<Solstice::Physics::RigidBody>(e);
                rb.Type = Solstice::Physics::ColliderType::Sphere;
                rb.Radius = 0.25f;
                rb.SetMass(1.0f);
            }
        }

        int foreachCount = 0;
        registry.ForEach<Solstice::ECS::Transform>([&](Solstice::ECS::EntityId, Solstice::ECS::Transform& tr) {
            SOLSTICE_TEST_ASSERT_VOID(FiniteVec3(tr.Position), "Transform position must stay finite");
            ++foreachCount;
        });
        SOLSTICE_TEST_ASSERT(foreachCount == entities, "ForEach Transform count mismatch");

        for (size_t i = 0; i < ids.size(); i += 3) {
            registry.Destroy(ids[i]);
        }

        int remaining = 0;
        registry.ForEach<Solstice::ECS::Transform>([&](Solstice::ECS::EntityId id, Solstice::ECS::Transform&) {
            (void)id;
            ++remaining;
        });
        const int expectedLeft = entities - static_cast<int>((ids.size() + 2) / 3);
        SOLSTICE_TEST_ASSERT(remaining == expectedLeft, "partial destroy entity count");
        SOLSTICE_TEST_ASSERT(remaining == expectedLeft, "Entity count after destroy wave");

        registry.ForEach<Solstice::ECS::Transform>([&](Solstice::ECS::EntityId eid, Solstice::ECS::Transform&) {
            if ((eid & 1u) == 0u) {
                registry.Remove<Solstice::ECS::Name>(eid);
            }
        });

        registry.ForEach<Solstice::ECS::Transform>([&](Solstice::ECS::EntityId eid, Solstice::ECS::Transform& tr) {
            tr.Position.x += 0.001f;
            SOLSTICE_TEST_ASSERT_VOID(FiniteVec3(tr.Position), "Nudge must stay finite");
        });

        for (const Solstice::ECS::EntityId id : ids) {
            if (registry.Valid(id)) {
                registry.Destroy(id);
            }
        }
    }

    SOLSTICE_TEST_PASS("ECS stress rounds completed");
    return true;
}

} // namespace

int main() {
    if (!RunEcsStress()) {
        return 1;
    }
    return SolsticeTestMainResult("ECSStressTest");
}

#include "TestHarness.hxx"
#include "Solstice.hxx"
#include "Entity/Registry.hxx"
#include "Physics/Dynamics/RigidBody.hxx"
#include "Physics/Integration/PhysicsSystem.hxx"

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

bool RunPhysicsStress() {
    if (!Solstice::Initialize()) {
        SOLSTICE_TEST_FAIL_MSG("Solstice::Initialize failed");
        return false;
    }

    const bool torture = EnvFlag("SOLSTICE_STRESS_TORTURE");
    const int bodies = EnvInt("SOLSTICE_PHYSICS_STRESS_BODIES", torture ? 512 : 128);
    const int steps = EnvInt("SOLSTICE_PHYSICS_STRESS_STEPS", torture ? 600 : 180);

    Solstice::ECS::Registry registry;
    Solstice::Physics::PhysicsSystem::Instance().Start(registry);

    std::mt19937 rng(1337);
    for (int i = 0; i < bodies; ++i) {
        const Solstice::ECS::EntityId e = registry.Create();
        auto& rb = registry.Add<Solstice::Physics::RigidBody>(e);
        rb.Position = Solstice::Math::Vec3(
            std::generate_canonical<float, 10>(rng) * 40.0f - 20.0f,
            std::generate_canonical<float, 10>(rng) * 40.0f - 20.0f,
            std::generate_canonical<float, 10>(rng) * 40.0f - 20.0f);
        rb.Velocity = Solstice::Math::Vec3(
            std::generate_canonical<float, 10>(rng) * 4.0f - 2.0f,
            std::generate_canonical<float, 10>(rng) * 4.0f - 2.0f,
            std::generate_canonical<float, 10>(rng) * 4.0f - 2.0f);
        if ((i & 1) == 0) {
            rb.Type = Solstice::Physics::ColliderType::Sphere;
            rb.Radius = 0.35f + std::generate_canonical<float, 10>(rng) * 0.4f;
            rb.SetMass(0.5f + std::generate_canonical<float, 10>(rng) * 2.0f);
        } else {
            rb.Type = Solstice::Physics::ColliderType::Box;
            rb.HalfExtents = Solstice::Math::Vec3(0.3f, 0.35f, 0.4f);
            rb.SetBoxInertia(1.0f, rb.HalfExtents);
        }
    }

    const float dt = 1.0f / 120.0f;
    for (int s = 0; s < steps; ++s) {
        if ((s & 127) == 0 && s > 0 && bodies > 8) {
            std::vector<Solstice::ECS::EntityId> toDestroy;
            registry.ForEach<Solstice::Physics::RigidBody>([&](Solstice::ECS::EntityId id, Solstice::Physics::RigidBody&) {
                if (toDestroy.size() < 3 && (id & 7u) == 0u) {
                    toDestroy.push_back(id);
                }
            });
            for (Solstice::ECS::EntityId id : toDestroy) {
                registry.Destroy(id);
            }
        }
        Solstice::Physics::PhysicsSystem::Instance().Update(dt);
    }

    registry.ForEach<Solstice::Physics::RigidBody>([&](Solstice::ECS::EntityId, Solstice::Physics::RigidBody& rb) {
        SOLSTICE_TEST_ASSERT_VOID(FiniteVec3(rb.Position), "body position finite");
        SOLSTICE_TEST_ASSERT_VOID(FiniteVec3(rb.Velocity), "body velocity finite");
        SOLSTICE_TEST_ASSERT_VOID(FiniteVec3(rb.AngularVelocity), "body angular velocity finite");
    });

    Solstice::Physics::PhysicsSystem::Instance().Stop();
    Solstice::Shutdown();

    SOLSTICE_TEST_PASS("Physics stress simulation completed");
    return true;
}

} // namespace

int main() {
    if (!RunPhysicsStress()) {
        return 1;
    }
    return SolsticeTestMainResult("PhysicsStressTest");
}

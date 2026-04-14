#include "SolsticeAPI/V1/Physics.h"
#include "Solstice.hxx"
#include "Entity/Registry.hxx"
#include "Physics/Integration/PhysicsSystem.hxx"
#include <memory>

extern "C" {

namespace {
std::unique_ptr<Solstice::ECS::Registry> g_CApiPhysicsRegistry;
bool g_CApiPhysicsStarted = false;
} // namespace

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_PhysicsStart(SolsticeV1_PhysicsWorldHandle* OutHandle) {
    if (OutHandle) {
        *OutHandle = nullptr;
    }

    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }

    if (!g_CApiPhysicsRegistry) {
        g_CApiPhysicsRegistry = std::make_unique<Solstice::ECS::Registry>();
    }

    if (!g_CApiPhysicsStarted) {
        Solstice::Physics::PhysicsSystem::Instance().Start(*g_CApiPhysicsRegistry);
        g_CApiPhysicsStarted = true;
    }

    if (OutHandle) {
        *OutHandle = reinterpret_cast<SolsticeV1_PhysicsWorldHandle>(g_CApiPhysicsRegistry.get());
    }
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API void SolsticeV1_PhysicsStop(void) {
    if (!g_CApiPhysicsStarted) {
        return;
    }
    Solstice::Physics::PhysicsSystem::Instance().Stop();
    g_CApiPhysicsStarted = false;
    g_CApiPhysicsRegistry.reset();
}

SOLSTICE_V1_API void SolsticeV1_PhysicsUpdate(float Dt) {
    if (!g_CApiPhysicsStarted) {
        return;
    }
    Solstice::Physics::PhysicsSystem::Instance().Update(Dt);
}

} // extern "C"

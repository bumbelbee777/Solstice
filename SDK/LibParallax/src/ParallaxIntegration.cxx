#include <Parallax/ParallaxScene.hxx>

namespace Solstice::Parallax {

void RestorePhysicsSnapshot(const ParallaxScene& scene, uint64_t timeTicks, Solstice::Physics::PhysicsSystem& physics,
                            Solstice::ECS::Registry& registry) {
    (void)scene;
    (void)timeTicks;
    (void)physics;
    (void)registry;
}

void RestoreScriptSnapshot(const ParallaxScene& scene, uint64_t timeTicks, Solstice::Scripting::BytecodeVM& vm) {
    (void)scene;
    (void)timeTicks;
    (void)vm;
}

} // namespace Solstice::Parallax

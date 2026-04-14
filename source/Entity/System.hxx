#pragma once

namespace Solstice::ECS {
class Registry;

// Lightweight registry-driven system contract used by ECS-based subsystems.
// Systems that do not need polymorphism can keep static Update-style functions.
struct ISystem {
    virtual ~ISystem() = default;
    virtual void Update(Registry& R, float DeltaTime) = 0;
};
}

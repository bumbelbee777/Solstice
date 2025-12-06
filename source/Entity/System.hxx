#pragma once

namespace Solstice::ECS {
class Registry;

struct ISystem {
    virtual ~ISystem() = default;
    virtual void Update(Registry& R, float DeltaTime) = 0;
};
}

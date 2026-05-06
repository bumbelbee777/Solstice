#pragma once

#include "../../Solstice.hxx"
#include <Entity/System.hxx>

namespace Solstice::ECS {
class Registry;
}

namespace Solstice::Game {

class SOLSTICE_API PortalSystem final : public ECS::ISystem {
public:
    void Update(ECS::Registry& Registry, float DeltaTime) override;
};

} // namespace Solstice::Game

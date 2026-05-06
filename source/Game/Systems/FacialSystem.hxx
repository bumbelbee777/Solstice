#pragma once

#include <Entity/System.hxx>

namespace Solstice::ECS { class Registry; }

namespace Solstice::Game {

class FacialSystem : public ECS::ISystem {
public:
    void Update(ECS::Registry& registry, float deltaTime) override;
};

} // namespace Solstice::Game

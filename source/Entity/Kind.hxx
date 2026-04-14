#pragma once

namespace Solstice::ECS {
enum class EntityKind {
    NeutralNPC,
    HostileNPC,
    Player,
    OtherPlayer,
    Item,
    Environment,
    Other
};

struct Kind {
    EntityKind Value;
};
}

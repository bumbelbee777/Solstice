#pragma once

#include "Registry.hxx"
#include "PlayerTag.hxx"
#include "CurrentPlayer.hxx"
#include "Transform.hxx"
#include "Name.hxx"
#include "Kind.hxx"

namespace Solstice::ECS {

inline EntityId CreateDefaultPlayer(Registry& R) {
    const EntityId e = R.Create();

    R.Add<Transform>(e, Transform{ {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} });
    R.Add<Name>(e, Name{ "Player" });
    R.Add<Kind>(e, Kind{ EntityKind::Player });
    R.Add<PlayerTag>(e, PlayerTag{});
    
    SetCurrentPlayer(e);
    return e;
}

inline EntityId EnsureDefaultPlayer(Registry& R) {
    auto id = GetCurrentPlayer();
    if (id != 0 && R.Valid(id)) return id;
    return CreateDefaultPlayer(R);
}

} // namespace Solstice::ECS

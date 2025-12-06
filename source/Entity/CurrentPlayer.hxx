#pragma once

#include "EntityId.hxx"
#include "Registry.hxx"

namespace Solstice::ECS {

inline EntityId& CurrentPlayerIdRef() {
    static EntityId id = 0;
    return id;
}

inline EntityId GetCurrentPlayer() {
    return CurrentPlayerIdRef();
}

inline void SetCurrentPlayer(EntityId id) {
    CurrentPlayerIdRef() = id;
}

inline EntityId EnsureCurrentPlayer(Registry& R) {
    auto id = GetCurrentPlayer();
    if (id != 0 && R.Valid(id)) return id;
    return 0; // Will be created by factory if needed
}

} // namespace Solstice::ECS

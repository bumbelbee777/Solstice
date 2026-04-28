#pragma once

#include <Arzachel/MapSerializer.hxx>

#include <Smf/SmfMap.hxx>

namespace Solstice::Game {

/// Pushes **SMAL v1** map gameplay into engine systems (audio, lights, sky, fluids, etc.). Match-only authoring is **not** in `SmfMap`; set that on `MatchGameplayAuthoringCache` from the game.
inline void ApplySmfGameplayToEngine(const Solstice::Smf::SmfMap& map) {
    Solstice::Arzachel::MapSerializer::ApplyGameplayFromSmfMap(map);
}

} // namespace Solstice::Game

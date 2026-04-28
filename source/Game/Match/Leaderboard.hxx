#pragma once

#include "../../Solstice.hxx"

#include "Match/MatchScoreState.hxx"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Solstice::Game {

/// Sorted FFA view over `MatchScoreState` (higher score first; ties broken by lower player id).
class SOLSTICE_API Leaderboard {
public:
    struct Row {
        std::uint32_t MatchPlayerId{0};
        std::int64_t Score{0};
    };

    /// Rebuild ordering from current scores (call after score mutations or each frame).
    void Refresh(const MatchScoreState& scores);

    const std::vector<Row>& GetRows() const { return m_Rows; }

    /// Fixed-capacity snapshot for UI or network (padding with zero ids if fewer entries).
    void BuildSnapshot(std::vector<Row>& out, std::size_t maxEntries) const;

private:
    std::vector<Row> m_Rows;
};

} // namespace Solstice::Game

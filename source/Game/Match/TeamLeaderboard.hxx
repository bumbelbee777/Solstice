#pragma once

#include "../../Solstice.hxx"

#include "Match/MatchScoreState.hxx"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Solstice::Game {

/// High-first ordering over **non-zero** team ids present in `MatchScoreState` player team map.
class SOLSTICE_API TeamLeaderboard {
public:
    struct Row {
        std::uint8_t TeamId{0};
        std::int64_t Score{0};
    };

    void Refresh(const MatchScoreState& scores);

    const std::vector<Row>& GetRows() const { return m_Rows; }

private:
    std::vector<Row> m_Rows;
};

} // namespace Solstice::Game

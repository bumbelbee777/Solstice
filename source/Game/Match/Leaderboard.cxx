#include "Match/Leaderboard.hxx"

#include <algorithm>

namespace Solstice::Game {

void Leaderboard::Refresh(const MatchScoreState& scores) {
    m_Rows.clear();
    for (const auto& [pid, sc] : scores.GetAllScores()) {
        m_Rows.push_back(Row{pid, sc});
    }
    std::sort(m_Rows.begin(), m_Rows.end(), [](const Row& a, const Row& b) {
        if (a.Score != b.Score) {
            return a.Score > b.Score;
        }
        return a.MatchPlayerId < b.MatchPlayerId;
    });
}

void Leaderboard::BuildSnapshot(std::vector<Row>& out, std::size_t maxEntries) const {
    out.clear();
    if (maxEntries == 0) {
        return;
    }
    out.reserve(maxEntries);
    for (std::size_t i = 0; i < maxEntries; ++i) {
        if (i < m_Rows.size()) {
            out.push_back(m_Rows[i]);
        } else {
            out.push_back(Row{0, 0});
        }
    }
}

} // namespace Solstice::Game

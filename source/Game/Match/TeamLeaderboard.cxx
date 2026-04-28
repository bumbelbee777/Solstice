#include "Match/TeamLeaderboard.hxx"

#include <algorithm>
#include <unordered_set>

namespace Solstice::Game {

void TeamLeaderboard::Refresh(const MatchScoreState& scores) {
    m_Rows.clear();
    std::unordered_set<std::uint8_t> seen;
    for (const auto& [pid, team] : scores.GetPlayerTeams()) {
        (void)pid;
        if (team == 0) {
            continue;
        }
        if (seen.insert(team).second) {
            Row r;
            r.TeamId = team;
            r.Score = scores.GetTeamScore(team);
            m_Rows.push_back(r);
        }
    }
    std::sort(m_Rows.begin(), m_Rows.end(), [](const Row& a, const Row& b) {
        if (a.Score != b.Score) {
            return a.Score > b.Score;
        }
        return a.TeamId < b.TeamId;
    });
}

} // namespace Solstice::Game

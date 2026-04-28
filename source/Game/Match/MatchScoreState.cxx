#include "Match/MatchScoreState.hxx"

namespace Solstice::Game {

void MatchScoreState::Clear() {
    m_Scores.clear();
    m_Zones.clear();
    m_PlayerTeams.clear();
}

void MatchScoreState::SetScore(std::uint32_t matchPlayerId, std::int64_t score) {
    m_Scores[matchPlayerId] = score;
}

std::int64_t MatchScoreState::GetScore(std::uint32_t matchPlayerId) const {
    const auto it = m_Scores.find(matchPlayerId);
    if (it == m_Scores.end()) {
        return 0;
    }
    return it->second;
}

void MatchScoreState::AddScore(std::uint32_t matchPlayerId, std::int64_t delta) {
    m_Scores[matchPlayerId] = GetScore(matchPlayerId) + delta;
}

void MatchScoreState::SetZoneProgress(const CaptureZoneProgress& zone) {
    m_Zones[zone.ZoneId] = zone;
}

bool MatchScoreState::GetZoneProgress(const std::string& zoneId, CaptureZoneProgress& out) const {
    const auto it = m_Zones.find(zoneId);
    if (it == m_Zones.end()) {
        return false;
    }
    out = it->second;
    return true;
}

void MatchScoreState::SetPlayerTeam(std::uint32_t matchPlayerId, std::uint8_t teamId) {
    if (teamId == 0) {
        m_PlayerTeams.erase(matchPlayerId);
    } else {
        m_PlayerTeams[matchPlayerId] = teamId;
    }
}

std::uint8_t MatchScoreState::GetPlayerTeam(std::uint32_t matchPlayerId) const {
    const auto it = m_PlayerTeams.find(matchPlayerId);
    if (it == m_PlayerTeams.end()) {
        return 0;
    }
    return it->second;
}

std::int64_t MatchScoreState::GetTeamScore(std::uint8_t teamId) const {
    if (teamId == 0) {
        return 0;
    }
    std::int64_t sum{0};
    for (const auto& [pid, score] : m_Scores) {
        const auto it = m_PlayerTeams.find(pid);
        if (it != m_PlayerTeams.end() && it->second == teamId) {
            sum += score;
        }
    }
    return sum;
}

} // namespace Solstice::Game

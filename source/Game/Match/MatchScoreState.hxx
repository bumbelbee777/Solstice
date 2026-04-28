#pragma once

#include "../../Solstice.hxx"

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace Solstice::Game {

/// Per–capture-zone progress (neutral-flag style, FFA: any `MatchPlayerId` can lead).
struct CaptureZoneProgress {
    std::string ZoneId;
    std::uint32_t LeadingPlayerId{0};
    /// 0..1 capture level for the leading player.
    float Progress01{0.0f};
};

/// Authoritative-ish match scores: FFA or **team-aggregated** table + optional per-zone capture state.
class SOLSTICE_API MatchScoreState {
public:
    void Clear();

    void SetScore(std::uint32_t matchPlayerId, std::int64_t score);
    std::int64_t GetScore(std::uint32_t matchPlayerId) const;
    void AddScore(std::uint32_t matchPlayerId, std::int64_t delta);

    /// Team `0` = none (FFA: score does not roll into a team total).
    void SetPlayerTeam(std::uint32_t matchPlayerId, std::uint8_t teamId);
    std::uint8_t GetPlayerTeam(std::uint32_t matchPlayerId) const;
    /// Sum of all **player** scores whose team assignment is `teamId` (ignores FFA / team 0).
    std::int64_t GetTeamScore(std::uint8_t teamId) const;

    void SetZoneProgress(const CaptureZoneProgress& zone);
    bool GetZoneProgress(const std::string& zoneId, CaptureZoneProgress& out) const;

    const std::unordered_map<std::uint32_t, std::int64_t>& GetAllScores() const { return m_Scores; }
    const std::unordered_map<std::string, CaptureZoneProgress>& GetAllZones() const { return m_Zones; }
    const std::unordered_map<std::uint32_t, std::uint8_t>& GetPlayerTeams() const { return m_PlayerTeams; }

private:
    std::unordered_map<std::uint32_t, std::int64_t> m_Scores;
    std::unordered_map<std::string, CaptureZoneProgress> m_Zones;
    std::unordered_map<std::uint32_t, std::uint8_t> m_PlayerTeams;
};

} // namespace Solstice::Game

#pragma once

#include "../../Solstice.hxx"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Solstice::Game {

/// Per-achievement state for local / steam-style unlock tracking (game supplies ids).
struct AchievementRecord {
    bool Unlocked{false};
    std::int64_t Progress{0};
    std::int64_t Target{0};
};

class SOLSTICE_API AchievementState {
public:
    void Clear();

    void Configure(const std::string& id, std::int64_t target);
    void SetProgress(const std::string& id, std::int64_t value);
    void Unlock(const std::string& id);
    /// Marks complete when progress reaches target (if target > 0).
    void AddProgress(const std::string& id, std::int64_t delta);

    bool IsUnlocked(const std::string& id) const;
    std::int64_t GetProgress(const std::string& id) const;
    std::int64_t GetTarget(const std::string& id) const;

    const std::unordered_map<std::string, AchievementRecord>& GetAll() const { return m_Records; }

private:
    std::unordered_map<std::string, AchievementRecord> m_Records;
};

} // namespace Solstice::Game

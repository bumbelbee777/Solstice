#include "Progression/AchievementState.hxx"

namespace Solstice::Game {

void AchievementState::Clear() {
    m_Records.clear();
}

void AchievementState::Configure(const std::string& id, std::int64_t target) {
    AchievementRecord& r = m_Records[id];
    r.Target = target;
}

void AchievementState::SetProgress(const std::string& id, std::int64_t value) {
    AchievementRecord& r = m_Records[id];
    r.Progress = value;
    if (r.Target > 0 && r.Progress >= r.Target) {
        r.Unlocked = true;
    }
}

void AchievementState::Unlock(const std::string& id) {
    m_Records[id].Unlocked = true;
}

void AchievementState::AddProgress(const std::string& id, std::int64_t delta) {
    AchievementRecord& r = m_Records[id];
    r.Progress += delta;
    if (r.Target > 0 && r.Progress >= r.Target) {
        r.Unlocked = true;
    }
}

bool AchievementState::IsUnlocked(const std::string& id) const {
    const auto it = m_Records.find(id);
    if (it == m_Records.end()) {
        return false;
    }
    return it->second.Unlocked;
}

std::int64_t AchievementState::GetProgress(const std::string& id) const {
    const auto it = m_Records.find(id);
    if (it == m_Records.end()) {
        return 0;
    }
    return it->second.Progress;
}

std::int64_t AchievementState::GetTarget(const std::string& id) const {
    const auto it = m_Records.find(id);
    if (it == m_Records.end()) {
        return 0;
    }
    return it->second.Target;
}

} // namespace Solstice::Game

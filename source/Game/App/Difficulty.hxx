#pragma once

#include "../../Solstice.hxx"
#include <string>
#include <unordered_map>

namespace Solstice::Game {

// Difficulty levels
enum class DifficultyLevel {
    Easy,
    Normal,
    Hard,
    Custom
};

// Difficulty parameters
struct DifficultyParameters {
    float EnemyHealthMultiplier{1.0f};
    float EnemyDamageMultiplier{1.0f};
    float EnemySpawnRateMultiplier{1.0f};
    float PlayerDamageMultiplier{1.0f};
    float ResourceMultiplier{1.0f};
    float ExperienceMultiplier{1.0f};
    float LootDropRate{1.0f};
};

// Difficulty manager
class SOLSTICE_API DifficultyManager {
public:
    DifficultyManager();
    ~DifficultyManager() = default;

    // Set difficulty level
    void SetDifficulty(DifficultyLevel Level);
    DifficultyLevel GetDifficulty() const { return m_CurrentDifficulty; }

    // Get current parameters
    const DifficultyParameters& GetParameters() const { return m_CurrentParameters; }
    DifficultyParameters& GetParameters() { return m_CurrentParameters; }

    // Custom difficulty
    void SetCustomParameters(const DifficultyParameters& Parameters);
    const DifficultyParameters& GetCustomParameters() const { return m_CustomParameters; }

    // Presets for different game types
    void SetFPSPreset(DifficultyLevel Level);
    void SetStrategyPreset(DifficultyLevel Level);
    void SetSandboxPreset(DifficultyLevel Level);

    // Apply difficulty to systems
    void ApplyToEnemySystem();
    void ApplyToHealthSystem();
    void ApplyToSpawnSystem();

private:
    DifficultyLevel m_CurrentDifficulty{DifficultyLevel::Normal};
    DifficultyParameters m_CurrentParameters;
    DifficultyParameters m_CustomParameters;

    // Preset parameters
    std::unordered_map<DifficultyLevel, DifficultyParameters> m_FPSPresets;
    std::unordered_map<DifficultyLevel, DifficultyParameters> m_StrategyPresets;
    std::unordered_map<DifficultyLevel, DifficultyParameters> m_SandboxPresets;

    void InitializePresets();
    void UpdateCurrentParameters();
};

} // namespace Solstice::Game

#include "Difficulty.hxx"
#include "../Core/Debug.hxx"

namespace Solstice::Game {

DifficultyManager::DifficultyManager() {
    InitializePresets();
    SetDifficulty(DifficultyLevel::Normal);
}

void DifficultyManager::InitializePresets() {
    // FPS presets
    DifficultyParameters easyFPS;
    easyFPS.EnemyHealthMultiplier = 0.7f;
    easyFPS.EnemyDamageMultiplier = 0.7f;
    easyFPS.EnemySpawnRateMultiplier = 0.8f;
    easyFPS.PlayerDamageMultiplier = 1.3f;
    easyFPS.LootDropRate = 1.2f;
    m_FPSPresets[DifficultyLevel::Easy] = easyFPS;

    DifficultyParameters normalFPS;
    normalFPS.EnemyHealthMultiplier = 1.0f;
    normalFPS.EnemyDamageMultiplier = 1.0f;
    normalFPS.EnemySpawnRateMultiplier = 1.0f;
    normalFPS.PlayerDamageMultiplier = 1.0f;
    normalFPS.LootDropRate = 1.0f;
    m_FPSPresets[DifficultyLevel::Normal] = normalFPS;

    DifficultyParameters hardFPS;
    hardFPS.EnemyHealthMultiplier = 1.5f;
    hardFPS.EnemyDamageMultiplier = 1.5f;
    hardFPS.EnemySpawnRateMultiplier = 1.3f;
    hardFPS.PlayerDamageMultiplier = 0.8f;
    hardFPS.LootDropRate = 0.7f;
    m_FPSPresets[DifficultyLevel::Hard] = hardFPS;

    // Strategy presets
    DifficultyParameters easyStrategy;
    easyStrategy.EnemyHealthMultiplier = 0.8f;
    easyStrategy.EnemyDamageMultiplier = 0.8f;
    easyStrategy.ResourceMultiplier = 1.5f;
    easyStrategy.ExperienceMultiplier = 1.2f;
    m_StrategyPresets[DifficultyLevel::Easy] = easyStrategy;

    DifficultyParameters normalStrategy;
    normalStrategy.EnemyHealthMultiplier = 1.0f;
    normalStrategy.EnemyDamageMultiplier = 1.0f;
    normalStrategy.ResourceMultiplier = 1.0f;
    normalStrategy.ExperienceMultiplier = 1.0f;
    m_StrategyPresets[DifficultyLevel::Normal] = normalStrategy;

    DifficultyParameters hardStrategy;
    hardStrategy.EnemyHealthMultiplier = 1.3f;
    hardStrategy.EnemyDamageMultiplier = 1.3f;
    hardStrategy.ResourceMultiplier = 0.7f;
    hardStrategy.ExperienceMultiplier = 0.8f;
    m_StrategyPresets[DifficultyLevel::Hard] = hardStrategy;

    // Sandbox presets (mostly affect resource generation)
    DifficultyParameters easySandbox;
    easySandbox.ResourceMultiplier = 2.0f;
    easySandbox.ExperienceMultiplier = 1.5f;
    m_SandboxPresets[DifficultyLevel::Easy] = easySandbox;

    DifficultyParameters normalSandbox;
    normalSandbox.ResourceMultiplier = 1.0f;
    normalSandbox.ExperienceMultiplier = 1.0f;
    m_SandboxPresets[DifficultyLevel::Normal] = normalSandbox;

    DifficultyParameters hardSandbox;
    hardSandbox.ResourceMultiplier = 0.5f;
    hardSandbox.ExperienceMultiplier = 0.7f;
    m_SandboxPresets[DifficultyLevel::Hard] = hardSandbox;
}

void DifficultyManager::SetDifficulty(DifficultyLevel Level) {
    m_CurrentDifficulty = Level;
    UpdateCurrentParameters();
    SIMPLE_LOG("DifficultyManager: Set difficulty to " + std::to_string(static_cast<int>(Level)));
}

void DifficultyManager::UpdateCurrentParameters() {
    if (m_CurrentDifficulty == DifficultyLevel::Custom) {
        m_CurrentParameters = m_CustomParameters;
    } else {
        // Use FPS preset by default (can be changed with SetFPSPreset, etc.)
        auto it = m_FPSPresets.find(m_CurrentDifficulty);
        if (it != m_FPSPresets.end()) {
            m_CurrentParameters = it->second;
        } else {
            // Default to normal if preset not found
            m_CurrentParameters = DifficultyParameters();
        }
    }
}

void DifficultyManager::SetCustomParameters(const DifficultyParameters& Parameters) {
    m_CustomParameters = Parameters;
    if (m_CurrentDifficulty == DifficultyLevel::Custom) {
        m_CurrentParameters = m_CustomParameters;
    }
}

void DifficultyManager::SetFPSPreset(DifficultyLevel Level) {
    auto it = m_FPSPresets.find(Level);
    if (it != m_FPSPresets.end()) {
        m_CurrentParameters = it->second;
    }
}

void DifficultyManager::SetStrategyPreset(DifficultyLevel Level) {
    auto it = m_StrategyPresets.find(Level);
    if (it != m_StrategyPresets.end()) {
        m_CurrentParameters = it->second;
    }
}

void DifficultyManager::SetSandboxPreset(DifficultyLevel Level) {
    auto it = m_SandboxPresets.find(Level);
    if (it != m_SandboxPresets.end()) {
        m_CurrentParameters = it->second;
    }
}

void DifficultyManager::ApplyToEnemySystem() {
    // This would integrate with EnemySystem to apply difficulty multipliers
    SIMPLE_LOG("DifficultyManager: Applying difficulty to EnemySystem");
}

void DifficultyManager::ApplyToHealthSystem() {
    // This would integrate with HealthSystem to apply difficulty multipliers
    SIMPLE_LOG("DifficultyManager: Applying difficulty to HealthSystem");
}

void DifficultyManager::ApplyToSpawnSystem() {
    // This would integrate with spawn systems to apply difficulty multipliers
    SIMPLE_LOG("DifficultyManager: Applying difficulty to SpawnSystem");
}

} // namespace Solstice::Game

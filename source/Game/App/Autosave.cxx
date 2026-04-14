#include "App/Autosave.hxx"
#include "../../Core/Debug/Debug.hxx"
#include <filesystem>
#include <algorithm>

namespace Solstice::Game {

AutosaveManager::AutosaveManager() {
    m_LastAutosaveTime = std::chrono::high_resolution_clock::now();

    // Create autosave directory
    try {
        if (!std::filesystem::exists(m_AutosaveDirectory)) {
            std::filesystem::create_directories(m_AutosaveDirectory);
        }
    } catch (const std::exception& e) {
        SIMPLE_LOG("AutosaveManager: Failed to create autosave directory: " + std::string(e.what()));
    }
}

void AutosaveManager::Update(float DeltaTime, GameState& GameState) {
    if (!m_AutosaveEnabled) return;

    // Only autosave during gameplay
    if (GameState.GetCurrentState() != GameStateType::Playing) {
        return;
    }

    m_TimeSinceLastAutosave += DeltaTime;

    if (m_TimeSinceLastAutosave >= m_AutosaveInterval) {
        TriggerAutosave();
        m_TimeSinceLastAutosave = 0.0f;
    }
}

bool AutosaveManager::TriggerAutosave() {
    if (!m_AutosaveEnabled) return false;
    if (!m_SaveDataCallback) {
        SIMPLE_LOG("AutosaveManager: No save data callback set");
        return false;
    }

    return PerformAutosave();
}

bool AutosaveManager::PerformAutosave() {
    try {
        // Rotate slots if needed
        RotateAutosaveSlots();

        // Get save data from callback
        Core::SaveData saveData = m_SaveDataCallback();

        // Save to slot 0 (most recent)
        std::string slotPath = GetAutosaveSlotPath(0);
        if (m_SaveManager.Save(slotPath, saveData)) {
            m_LastAutosaveTime = std::chrono::high_resolution_clock::now();
            SIMPLE_LOG("AutosaveManager: Autosave completed to " + slotPath);
            return true;
        } else {
            SIMPLE_LOG("AutosaveManager: Failed to save autosave");
            return false;
        }
    } catch (const std::exception& e) {
        SIMPLE_LOG("AutosaveManager: Exception during autosave: " + std::string(e.what()));
        return false;
    }
}

void AutosaveManager::OnCheckpointReached() {
    TriggerAutosave();
}

void AutosaveManager::OnLevelTransition() {
    TriggerAutosave();
}

void AutosaveManager::OnPlayerDeath() {
    // Don't autosave on death, but could save a checkpoint before death
}

std::vector<std::string> AutosaveManager::GetAutosaveSlots() const {
    std::vector<std::string> slots;
    for (int i = 0; i < m_MaxAutosaveSlots; ++i) {
        std::string slotPath = GetAutosaveSlotPath(i);
        if (std::filesystem::exists(slotPath)) {
            slots.push_back(slotPath);
        }
    }
    return slots;
}

bool AutosaveManager::LoadAutosave(int SlotIndex, Core::SaveData& OutData) {
    if (SlotIndex < 0 || SlotIndex >= m_MaxAutosaveSlots) {
        return false;
    }

    std::string slotPath = GetAutosaveSlotPath(SlotIndex);
    return m_SaveManager.Load(slotPath, OutData);
}

std::string AutosaveManager::GetAutosaveSlotPath(int SlotIndex) const {
    return m_AutosaveDirectory + "/autosave_" + std::to_string(SlotIndex) + ".sav";
}

void AutosaveManager::RotateAutosaveSlots() {
    // Shift all autosave slots down by one
    // This keeps the most recent autosave at slot 0
    for (int i = m_MaxAutosaveSlots - 2; i >= 0; --i) {
        std::string oldPath = GetAutosaveSlotPath(i);
        std::string newPath = GetAutosaveSlotPath(i + 1);

        if (std::filesystem::exists(oldPath)) {
            try {
                if (std::filesystem::exists(newPath)) {
                    std::filesystem::remove(newPath);
                }
                std::filesystem::rename(oldPath, newPath);
            } catch (const std::exception& e) {
                SIMPLE_LOG("AutosaveManager: Failed to rotate slot " + std::to_string(i) + ": " + std::string(e.what()));
            }
        }
    }
}

} // namespace Solstice::Game

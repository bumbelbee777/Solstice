#pragma once

#include "../Solstice.hxx"
#include "../Core/Save.hxx"
#include "GameState.hxx"
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace Solstice::Game {

// Autosave manager extending SaveManager
class SOLSTICE_API AutosaveManager {
public:
    AutosaveManager();
    ~AutosaveManager() = default;

    // Configuration
    void SetAutosaveEnabled(bool Enabled) { m_AutosaveEnabled = Enabled; }
    bool IsAutosaveEnabled() const { return m_AutosaveEnabled; }
    void SetAutosaveInterval(float IntervalSeconds) { m_AutosaveInterval = IntervalSeconds; }
    float GetAutosaveInterval() const { return m_AutosaveInterval; }
    void SetMaxAutosaveSlots(int Slots) { m_MaxAutosaveSlots = Slots; }

    // Update (call each frame)
    void Update(float DeltaTime, GameState& GameState);

    // Manual autosave trigger
    bool TriggerAutosave();

    // Save data preparation
    using SaveDataCallback = std::function<Core::SaveData()>;
    void SetSaveDataCallback(SaveDataCallback Callback) { m_SaveDataCallback = Callback; }

    // Event-based triggers
    void OnCheckpointReached();
    void OnLevelTransition();
    void OnPlayerDeath();

    // Get autosave slots
    std::vector<std::string> GetAutosaveSlots() const;

    // Load from autosave
    bool LoadAutosave(int SlotIndex, Core::SaveData& OutData);

private:
    bool m_AutosaveEnabled{true};
    float m_AutosaveInterval{300.0f}; // 5 minutes default
    int m_MaxAutosaveSlots{10};

    float m_TimeSinceLastAutosave{0.0f};
    std::chrono::high_resolution_clock::time_point m_LastAutosaveTime;

    Core::SaveManager m_SaveManager;
    SaveDataCallback m_SaveDataCallback;

    std::string m_AutosaveDirectory{"saves/autosave"};

    // Internal methods
    std::string GetAutosaveSlotPath(int SlotIndex) const;
    void RotateAutosaveSlots();
    bool PerformAutosave();
};

} // namespace Solstice::Game

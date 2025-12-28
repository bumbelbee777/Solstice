#pragma once

#include "../Solstice.hxx"
#include "Level.hxx"
#include "../UI/Widgets.hxx"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Solstice::Game {

// Level metadata
struct LevelMetadata {
    std::string Name;
    std::string Description;
    std::string ThumbnailPath;
    bool IsCompleted{false};
    bool IsUnlocked{true};
    float CompletionPercent{0.0f};
    float BestTime{0.0f};
    int Difficulty{1}; // 1=easy, 2=normal, 3=hard
};

// View mode for level selector
enum class LevelSelectorViewMode {
    Grid,
    List
};

// Level selector class
class SOLSTICE_API LevelSelector {
public:
    LevelSelector();
    ~LevelSelector() = default;

    // Add/remove levels
    void AddLevel(const LevelMetadata& Metadata);
    void RemoveLevel(const std::string& LevelName);
    LevelMetadata* GetLevel(const std::string& LevelName);
    const std::vector<LevelMetadata>& GetAllLevels() const { return m_Levels; }

    // Selection
    void SetSelectedLevel(const std::string& LevelName);
    const std::string& GetSelectedLevel() const { return m_SelectedLevel; }
    LevelMetadata* GetSelectedLevelMetadata();

    // View mode
    void SetViewMode(LevelSelectorViewMode Mode) { m_ViewMode = Mode; }
    LevelSelectorViewMode GetViewMode() const { return m_ViewMode; }

    // Render
    void Render(int ScreenWidth, int ScreenHeight);

    // Update
    void Update(float DeltaTime);

    // Callbacks
    using LevelSelectedCallback = std::function<void(const std::string& LevelName)>;
    void SetLevelSelectedCallback(LevelSelectedCallback Callback) { m_LevelSelectedCallback = Callback; }

    // Unlock system
    void UnlockLevel(const std::string& LevelName);
    void LockLevel(const std::string& LevelName);
    bool IsLevelUnlocked(const std::string& LevelName) const;

private:
    std::vector<LevelMetadata> m_Levels;
    std::unordered_map<std::string, size_t> m_LevelIndexMap;
    std::string m_SelectedLevel;
    LevelSelectorViewMode m_ViewMode{LevelSelectorViewMode::Grid};

    LevelSelectedCallback m_LevelSelectedCallback;

    // UI state
    float m_ScrollPosition{0.0f};
    int m_GridColumns{3};

    // Internal rendering
    void RenderGridView(int ScreenWidth, int ScreenHeight);
    void RenderListView(int ScreenWidth, int ScreenHeight);
    void RenderLevelCard(const LevelMetadata& Level, const ImVec2& Position, const ImVec2& Size, bool IsSelected);
};

} // namespace Solstice::Game

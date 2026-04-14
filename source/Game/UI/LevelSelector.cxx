#include "UI/LevelSelector.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../UI/Core/UISystem.hxx"
#include <imgui.h>
#include <algorithm>

namespace Solstice::Game {

LevelSelector::LevelSelector() {
}

void LevelSelector::AddLevel(const LevelMetadata& Metadata) {
    m_Levels.push_back(Metadata);
    m_LevelIndexMap[Metadata.Name] = m_Levels.size() - 1;
}

void LevelSelector::RemoveLevel(const std::string& LevelName) {
    auto it = m_LevelIndexMap.find(LevelName);
    if (it != m_LevelIndexMap.end()) {
        size_t index = it->second;
        m_Levels.erase(m_Levels.begin() + index);
        m_LevelIndexMap.erase(it);

        // Rebuild index map
        m_LevelIndexMap.clear();
        for (size_t i = 0; i < m_Levels.size(); ++i) {
            m_LevelIndexMap[m_Levels[i].Name] = i;
        }
    }
}

LevelMetadata* LevelSelector::GetLevel(const std::string& LevelName) {
    auto it = m_LevelIndexMap.find(LevelName);
    if (it != m_LevelIndexMap.end()) {
        return &m_Levels[it->second];
    }
    return nullptr;
}

void LevelSelector::SetSelectedLevel(const std::string& LevelName) {
    if (GetLevel(LevelName)) {
        m_SelectedLevel = LevelName;
        if (m_LevelSelectedCallback) {
            m_LevelSelectedCallback(LevelName);
        }
    }
}

LevelMetadata* LevelSelector::GetSelectedLevelMetadata() {
    return GetLevel(m_SelectedLevel);
}

void LevelSelector::UnlockLevel(const std::string& LevelName) {
    LevelMetadata* level = GetLevel(LevelName);
    if (level) {
        level->IsUnlocked = true;
    }
}

void LevelSelector::LockLevel(const std::string& LevelName) {
    LevelMetadata* level = GetLevel(LevelName);
    if (level) {
        level->IsUnlocked = false;
    }
}

bool LevelSelector::IsLevelUnlocked(const std::string& LevelName) const {
    auto it = m_LevelIndexMap.find(LevelName);
    if (it != m_LevelIndexMap.end()) {
        return m_Levels[it->second].IsUnlocked;
    }
    return false;
}

void LevelSelector::Update(float DeltaTime) {
    (void)DeltaTime;
}

void LevelSelector::Render(int ScreenWidth, int ScreenHeight) {
    float menuWidth = static_cast<float>(ScreenWidth) * 0.8f;
    float menuHeight = static_cast<float>(ScreenHeight) * 0.8f;
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);
    ImVec2 menuPos(center.x - menuWidth * 0.5f, center.y - menuHeight * 0.5f);

    ImGui::SetNextWindowPos(menuPos);
    ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight));

    ImGui::Begin("Level Selector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // View mode toggle
    if (ImGui::Button("Grid View")) {
        m_ViewMode = LevelSelectorViewMode::Grid;
    }
    ImGui::SameLine();
    if (ImGui::Button("List View")) {
        m_ViewMode = LevelSelectorViewMode::List;
    }
    ImGui::Separator();

    // Render based on view mode
    if (m_ViewMode == LevelSelectorViewMode::Grid) {
        RenderGridView(ScreenWidth, ScreenHeight);
    } else {
        RenderListView(ScreenWidth, ScreenHeight);
    }

    ImGui::Separator();
    if (ImGui::Button("Back", ImVec2(100, 30))) {
        // Back button - returns to main menu
        // Use empty string to indicate "back" action
        if (m_LevelSelectedCallback) {
            m_LevelSelectedCallback(""); // Empty string signals back action
        }
    }

    ImGui::End();
}

void LevelSelector::RenderGridView(int ScreenWidth, int ScreenHeight) {
    float cardWidth = 200.0f;
    float cardHeight = 250.0f;
    float spacing = 20.0f;
    float padding = 20.0f;

    int columns = m_GridColumns;
    int totalRows = (static_cast<int>(m_Levels.size()) + columns - 1) / columns;
    float contentHeight = totalRows * (cardHeight + spacing) - spacing + padding * 2.0f;
    float contentWidth = columns * (cardWidth + spacing) - spacing + padding * 2.0f;

    // Use BeginChild with scrolling to ensure all cards are visible
    ImGui::BeginChild("LevelGridScroll", ImVec2(0, -40), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Calculate starting position to center content if needed
    float startX = padding;
    float startY = padding;

    int row = 0;
    int col = 0;

    for (const auto& level : m_Levels) {
        if (col >= columns) {
            col = 0;
            row++;
        }

        float x = startX + col * (cardWidth + spacing);
        float y = startY + row * (cardHeight + spacing);

        RenderLevelCard(level, ImVec2(x, y), ImVec2(cardWidth, cardHeight), level.Name == m_SelectedLevel);

        col++;
    }

    ImGui::EndChild();
}

void LevelSelector::RenderListView(int ScreenWidth, int ScreenHeight) {
    (void)ScreenWidth;
    (void)ScreenHeight;

    for (const auto& level : m_Levels) {
        bool isSelected = (level.Name == m_SelectedLevel);

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        std::string buttonText = level.Name;
        if (level.IsCompleted) {
            buttonText += " [Completed]";
        }
        if (!level.IsUnlocked) {
            buttonText += " [Locked]";
        }

        if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 60.0f))) {
            if (level.IsUnlocked) {
                SetSelectedLevel(level.Name);
            }
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
    }
}

void LevelSelector::RenderLevelCard(const LevelMetadata& Level, const ImVec2& Position, const ImVec2& Size, bool IsSelected) {
    // Use screen position for drawing
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 min(windowPos.x + Position.x, windowPos.y + Position.y);
    ImVec2 max(min.x + Size.x, min.y + Size.y);

    // Background
    ImU32 bgColor = IsSelected ?
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.5f, 0.8f, 1.0f)) :
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

    if (!Level.IsUnlocked) {
        bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    }

    drawList->AddRectFilled(min, max, bgColor);

    // Border
    drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));

    // Level name
    ImVec2 textPos(min.x + 10.0f, min.y + 10.0f);
    drawList->AddText(textPos, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), Level.Name.c_str());

    // Completion status
    if (Level.IsCompleted) {
        ImVec2 checkPos(min.x + Size.x - 30.0f, min.y + 10.0f);
        drawList->AddCircleFilled(checkPos, 10.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));
    }

    // Click detection using invisible button
    ImGui::SetCursorPos(Position);
    std::string buttonId = "LevelCard_" + Level.Name;
    if (ImGui::InvisibleButton(buttonId.c_str(), Size)) {
        if (Level.IsUnlocked) {
            SetSelectedLevel(Level.Name);
        }
    }

    // Double-click to load level
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (Level.IsUnlocked && m_LevelSelectedCallback) {
            m_LevelSelectedCallback(Level.Name);
        }
    }
}

} // namespace Solstice::Game

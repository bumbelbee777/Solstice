#include "UI/MainMenu.hxx"
#include "App/GamePreferences.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Core/Serialization/Save.hxx"
#include "../../UI/Core/UISystem.hxx"
#include <imgui.h>

namespace Solstice::Game {

// Helper function to draw a gradient within a rounded rectangle
static void DrawRoundedRectGradient(ImDrawList* drawList, const ImVec2& min, const ImVec2& max,
                                     float rounding, ImU32 topColor, ImU32 bottomColor, int steps = 50) {
    // Draw many horizontal slices, each with appropriate rounding for the top
    for (int i = 0; i < steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps - 1);
        float nextT = static_cast<float>(i + 1) / static_cast<float>(steps - 1);

        // Interpolate color
        ImVec4 topCol = ImGui::ColorConvertU32ToFloat4(topColor);
        ImVec4 bottomCol = ImGui::ColorConvertU32ToFloat4(bottomColor);
        ImVec4 col1 = ImVec4(
            topCol.x + (bottomCol.x - topCol.x) * t,
            topCol.y + (bottomCol.y - topCol.y) * t,
            topCol.z + (bottomCol.z - topCol.z) * t,
            topCol.w + (bottomCol.w - topCol.w) * t
        );
        ImVec4 col2 = ImVec4(
            topCol.x + (bottomCol.x - topCol.x) * nextT,
            topCol.y + (bottomCol.y - topCol.y) * nextT,
            topCol.z + (bottomCol.z - topCol.z) * nextT,
            topCol.w + (bottomCol.w - topCol.w) * nextT
        );
        ImU32 avgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
            (col1.x + col2.x) * 0.5f,
            (col1.y + col2.y) * 0.5f,
            (col1.z + col2.z) * 0.5f,
            (col1.w + col2.w) * 0.5f
        ));

        if (col1.w < 0.01f && col2.w < 0.01f) continue; // Skip transparent slices

        // Calculate Y positions
        float y1 = min.y + (max.y - min.y) * t;
        float y2 = min.y + (max.y - min.y) * nextT;

        // Apply rounding only to top slices (where rounded corners are visible)
        float sliceRounding = 0.0f;
        if (t < 0.2f) {
            // Smooth interpolation: full rounding at top, zero at 20% down
            float roundingFactor = 1.0f - (t / 0.2f);
            sliceRounding = rounding * roundingFactor;
        }

        // Draw this slice
        ImVec2 sliceMin(min.x, y1);
        ImVec2 sliceMax(max.x, y2);
        drawList->AddRectFilled(sliceMin, sliceMax, avgColor, sliceRounding);
    }
}

MainMenu::MainMenu() {
}

void MainMenu::Show() {
    // Only reset submenu states if menu was previously hidden
    // This prevents resetting submenu state when Show() is called every frame
    bool wasHidden = !m_IsVisible;
    m_IsVisible = true;

    if (wasHidden) {
        // Only reset submenu states when menu was actually hidden
        m_SelectedOption = 0;
        m_ShowLoadGameMenu = false;
        m_ShowSettingsMenu = false;
        m_ShowLevelSelectorMenu = false;
    }
}

void MainMenu::Hide() {
    m_IsVisible = false;
}

void MainMenu::Update(float DeltaTime, InputManager& InputManager, GameState& GameState) {
    (void)DeltaTime;

    if (!m_IsVisible) return;

    HandleInput(InputManager, GameState);
}

void MainMenu::HandleInput(InputManager& InputManager, GameState& GameState) {
    if (m_ShowLoadGameMenu || m_ShowSettingsMenu || m_ShowLevelSelectorMenu) {
        // Handle back button
        if (InputManager.IsKeyJustPressed(27)) { // ESC
            m_ShowLoadGameMenu = false;
            m_ShowSettingsMenu = false;
            m_ShowLevelSelectorMenu = false;
        }
        return;
    }

    // Handle arrow keys for navigation
    if (InputManager.IsKeyJustPressed(82)) { // Up arrow
        m_SelectedOption = (m_SelectedOption - 1 + static_cast<int>(m_MenuOptions.size())) % static_cast<int>(m_MenuOptions.size());
    }
    if (InputManager.IsKeyJustPressed(81)) { // Down arrow
        m_SelectedOption = (m_SelectedOption + 1) % static_cast<int>(m_MenuOptions.size());
    }

    // Handle enter/return to select
    if (InputManager.IsKeyJustPressed(40)) { // Enter/Return
        ExecuteOption(m_SelectedOption);
    }
}

void MainMenu::ExecuteOption(int Option) {
    switch (Option) {
        case 0: // New Game
            if (m_ShowLevelSelector && m_LevelSelector) {
                // Show level selector instead of hiding menu
                m_ShowLevelSelectorMenu = true;
                // Also call the callback if set (for any additional setup)
                if (m_NewGameCallback) {
                    m_NewGameCallback();
                }
            } else {
                // No level selector - just call callback and hide menu
                if (m_NewGameCallback) {
                    m_NewGameCallback();
                }
                Hide();
            }
            break;
        case 1: // Load Game
            m_ShowLoadGameMenu = true;
            if (m_LoadGameCallback) {
                m_LoadGameCallback();
            }
            break;
        case 2: // Settings
            m_ShowSettingsMenu = true;
            if (m_SettingsCallback) {
                m_SettingsCallback();
            }
            break;
        case 3: // Quit
            if (m_QuitCallback) {
                m_QuitCallback();
            }
            break;
    }
}

void MainMenu::Render(int ScreenWidth, int ScreenHeight, float DeltaTime) {
    (void)DeltaTime;
    if (!m_IsVisible) {
        return;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);

    // 1. Sleek Frutiger Aero Background (Dark Grey Gradient)
    ImU32 primaryColor = ImGui::ColorConvertFloat4ToU32(ImVec4(m_PrimaryColor[0], m_PrimaryColor[1], m_PrimaryColor[2], m_PrimaryColor[3]));
    ImU32 secondaryColor = ImGui::ColorConvertFloat4ToU32(ImVec4(m_SecondaryColor[0], m_SecondaryColor[1], m_SecondaryColor[2], m_SecondaryColor[3]));
    ImU32 accentColor = ImGui::ColorConvertFloat4ToU32(ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], m_AccentColor[3]));

    drawList->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight)),
        primaryColor, primaryColor, secondaryColor, secondaryColor);

    // Subtle orange glow orbs
    drawList->AddCircleFilled(ImVec2(static_cast<float>(ScreenWidth) * 0.85f, static_cast<float>(ScreenHeight) * 0.15f), 400.0f,
        ImGui::GetColorU32(ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], 0.04f)), 64);
    drawList->AddCircleFilled(ImVec2(static_cast<float>(ScreenWidth) * 0.1f, static_cast<float>(ScreenHeight) * 0.9f), 300.0f,
        ImGui::GetColorU32(ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], 0.03f)), 64);

    if (m_ShowLoadGameMenu) {
        RenderLoadGameMenu(ScreenWidth, ScreenHeight);
        return;
    }

    // If settings or level selector is shown, skip the menu buttons but keep the background visible
    if (m_ShowSettingsMenu || m_ShowLevelSelectorMenu) {
        // Return early to skip menu buttons, but background was already rendered above
        return;
    }

    // Create a transparent fullscreen window for interactive elements
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); // Transparent background
    ImGui::Begin("MainMenuOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground);

    // 2. Glassy Menu Panel (Left Aligned for futuristic feel)
    float menuWidth = 480.0f;
    float menuHeight = 620.0f;
    ImVec2 menuPos(80.0f, center.y - menuHeight * 0.5f);

    // Glass background
    drawList->AddRectFilled(menuPos, ImVec2(menuPos.x + menuWidth, menuPos.y + menuHeight),
        ImGui::GetColorU32(ImVec4(m_PrimaryColor[0], m_PrimaryColor[1], m_PrimaryColor[2], 0.7f)), 24.0f);
    drawList->AddRect(menuPos, ImVec2(menuPos.x + menuWidth, menuPos.y + menuHeight), IM_COL32(120, 120, 160, 60), 24.0f, 0, 2.0f);

    // Glass gloss highlight - use custom rounded rectangle gradient function
    float highlightInset = 10.0f;
    float highlightHeight = 110.0f;
    float highlightRounding = 14.0f;

    // Calculate the desired final bounds (rounded rectangle)
    ImVec2 highlightMin(menuPos.x + highlightInset, menuPos.y + highlightInset);
    ImVec2 highlightMax(menuPos.x + menuWidth - highlightInset, menuPos.y + highlightInset + highlightHeight);

    // Draw gradient with rounded corners using our custom function
    ImU32 topColor = IM_COL32(255, 255, 255, 30);  // White with 30 alpha at top
    ImU32 bottomColor = IM_COL32(255, 255, 255, 0); // Transparent at bottom
    DrawRoundedRectGradient(drawList, highlightMin, highlightMax, highlightRounding, topColor, bottomColor, 60);

    // 3. Title (Sleek Accent Color) - Use drawList for text with larger size
    ImVec2 titlePos(menuPos.x + 50.0f, menuPos.y + 50.0f);
    // Calculate scaled font size for title
    float baseFontSize = ImGui::GetFontSize();
    float titleFontSize = baseFontSize * 3.2f;
    // Use ImGui's default font with calculated size
    // Note: Some ImGui versions may not support font parameter in AddText
    // Fallback to rendering multiple times or using default size
    ImFont* font = ImGui::GetFont();
    if (font) {
        // Try the font version - if it fails at compile time, we'll need to use a different approach
        // For now, render at default size but make it stand out with color
        drawList->AddText(titlePos, accentColor, m_Title.c_str());
    } else {
        drawList->AddText(titlePos, accentColor, m_Title.c_str());
    }

    if (!m_Subtitle.empty()) {
        ImVec2 subtitlePos(menuPos.x + 55.0f, menuPos.y + 120.0f);
        ImU32 subtitleColor = IM_COL32(179, 179, 204, 179); // 0.7f alpha
        drawList->AddText(nullptr, 20.0f, subtitlePos, subtitleColor, m_Subtitle.c_str());
    }

    // 4. Interactive Menu Options (Skeuomorphic Buttons)
    float startY = menuPos.y + 200.0f;
    for (size_t i = 0; i < m_MenuOptions.size(); ++i) {
        bool isSelected = (static_cast<int>(i) == m_SelectedOption);

        ImVec2 btnPos(menuPos.x + 50.0f, startY + i * 85.0f);
        ImVec2 btnSize(menuWidth - 100.0f, 65.0f);

        // Button Shadow (behind button)
        drawList->AddRectFilled(ImVec2(btnPos.x + 3, btnPos.y + 5), ImVec2(btnPos.x + btnSize.x + 3, btnPos.y + btnSize.y + 5), IM_COL32(0, 0, 0, 80), 12.0f);

        // Button Base - fill entire area with no gaps
        ImU32 btnBaseColor = isSelected ?
            ImGui::GetColorU32(ImVec4(m_SecondaryColor[0] * 1.5f, m_SecondaryColor[1] * 1.5f, m_SecondaryColor[2] * 1.5f, 1.0f)) :
            ImGui::GetColorU32(ImVec4(m_SecondaryColor[0], m_SecondaryColor[1], m_SecondaryColor[2], 1.0f));
        // Use AddRectFilled with proper rounding to ensure complete fill
        ImVec2 btnMin = btnPos;
        ImVec2 btnMax = ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y);
        drawList->AddRectFilled(btnMin, btnMax, btnBaseColor, 12.0f);

        // Button Border/Glow - draw on top of filled area
        ImU32 borderColor = isSelected ? accentColor : IM_COL32(100, 100, 130, 120);
        drawList->AddRect(btnMin, btnMax, borderColor, 12.0f, 0, isSelected ? 2.5f : 1.5f);

        // Button Glossy Highlight - ensure it doesn't create gaps
        // Make highlight slightly inset but ensure it covers the full width
        float highlightInset = 1.0f;
        drawList->AddRectFilledMultiColor(
            ImVec2(btnPos.x + highlightInset, btnPos.y + highlightInset),
            ImVec2(btnPos.x + btnSize.x - highlightInset, btnPos.y + btnSize.y * 0.45f),
            IM_COL32(255, 255, 255, 50), IM_COL32(255, 255, 255, 50),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

        // ImGui invisible button for selection/hover (needs window context)
        // Use screen coordinates since btnPos is in screen space
        ImGui::SetCursorScreenPos(btnPos);
        std::string btnId = "MenuBtn_" + std::to_string(i) + "_" + m_MenuOptions[i];
        if (ImGui::InvisibleButton(btnId.c_str(), btnSize)) {
            // Button clicked - execute the option
            ExecuteOption(static_cast<int>(i));
        }

        if (ImGui::IsItemHovered()) {
            m_SelectedOption = static_cast<int>(i);
            // Subtle hover glow
            drawList->AddRect(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y),
                ImGui::GetColorU32(ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], 0.4f)), 12.0f, 0, 4.0f);
        }

        // Button Text - Use drawList with screen coordinates
        ImVec2 textSize = ImGui::CalcTextSize(m_MenuOptions[i].c_str());
        ImVec2 textPos(btnPos.x + 30.0f, btnPos.y + (btnSize.y - textSize.y) * 0.5f);
        ImU32 textColor = isSelected ? accentColor : IM_COL32(220, 220, 240, 230);
        drawList->AddText(nullptr, 24.0f, textPos, textColor, m_MenuOptions[i].c_str());
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // 5. Build Info - Use drawList for text
    ImVec2 buildInfoPos(static_cast<float>(ScreenWidth) - 200.0f, static_cast<float>(ScreenHeight) - 50.0f);
    ImU32 buildInfoColor = IM_COL32(128, 128, 153, 102); // 0.4f alpha
    char buildText[256];
    snprintf(buildText, sizeof(buildText), "Development Build (Solstive v%s)", SOLSTICE_VERSION);
    drawList->AddText(nullptr, 14.0f, buildInfoPos, buildInfoColor, buildText);
}

void MainMenu::RenderLoadGameMenu(int ScreenWidth, int ScreenHeight) {
    float menuWidth = 600.0f;
    float menuHeight = 500.0f;
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);
    ImVec2 menuPos(center.x - menuWidth * 0.5f, center.y - menuHeight * 0.5f);

    ImGui::SetNextWindowPos(menuPos);
    ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight));

    ImGui::Begin("Load Game", nullptr, ImGuiWindowFlags_NoResize);

    ImGui::Text("Select Save Slot:");
    ImGui::Separator();

    // Get save slots
    Core::SaveManager saveManager;
    auto saveSlots = saveManager.GetSaveSlots();

    if (saveSlots.empty()) {
        ImGui::Text("No save files found.");
    } else {
        for (size_t i = 0; i < saveSlots.size(); ++i) {
            if (ImGui::Button(("Slot " + std::to_string(i + 1) + ": " + saveSlots[i]).c_str(),
                             ImVec2(menuWidth - 40.0f, 40.0f))) {
                // Load this save slot
                // This would trigger the load callback
            }
            ImGui::Spacing();
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Back", ImVec2(100, 30))) {
        m_ShowLoadGameMenu = false;
    }

    ImGui::End();
}

void MainMenu::RenderSaveSlots(int ScreenWidth, int ScreenHeight) {
    (void)ScreenWidth;
    (void)ScreenHeight;
    // Implementation for save slot display
}

} // namespace Solstice::Game

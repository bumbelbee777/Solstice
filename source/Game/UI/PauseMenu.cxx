#include "UI/PauseMenu.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../UI/Core/UISystem.hxx"
#include <imgui.h>

namespace Solstice::Game {

PauseMenu::PauseMenu() {
}

void PauseMenu::Show() {
    m_IsVisible = true;
    m_SelectedOption = 0;
}

void PauseMenu::Hide() {
    m_IsVisible = false;
}

void PauseMenu::Toggle() {
    if (m_IsVisible) {
        Hide();
    } else {
        Show();
    }
}

void PauseMenu::Update(float DeltaTime, InputManager& InputManager, GameState& GameState) {
    (void)DeltaTime;

    if (!m_IsVisible) return;

    HandleInput(InputManager, GameState);
}

void PauseMenu::HandleInput(InputManager& InputManager, GameState& GameState) {
    // Handle escape key to resume
    if (InputManager.IsKeyJustPressed(27)) { // ESC key
        ExecuteOption(0); // Resume
    }

    // Handle arrow keys for navigation
    if (InputManager.IsKeyJustPressed(82)) { // Up arrow (SDL_SCANCODE_UP)
        m_SelectedOption = (m_SelectedOption - 1 + static_cast<int>(m_MenuOptions.size())) % static_cast<int>(m_MenuOptions.size());
    }
    if (InputManager.IsKeyJustPressed(81)) { // Down arrow (SDL_SCANCODE_DOWN)
        m_SelectedOption = (m_SelectedOption + 1) % static_cast<int>(m_MenuOptions.size());
    }

    // Handle enter/return to select
    if (InputManager.IsKeyJustPressed(40)) { // Enter/Return
        ExecuteOption(m_SelectedOption);
    }
}

void PauseMenu::ExecuteOption(int Option) {
    switch (Option) {
        case 0: // Resume
            if (m_ResumeCallback) {
                m_ResumeCallback();
            }
            Hide();
            break;
        case 1: // Settings
            if (m_SettingsCallback) {
                m_SettingsCallback();
            }
            break;
        case 2: // Save
            if (m_SaveCallback) {
                m_SaveCallback();
            }
            break;
        case 3: // Quit
            if (m_QuitCallback) {
                m_QuitCallback();
            }
            break;
    }
}

void PauseMenu::Render(int ScreenWidth, int ScreenHeight) {
    if (!m_IsVisible) return;

    // Always draw semi-transparent background when pause menu is visible
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 min(0, 0);
    ImVec2 max(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));
    drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, m_BackgroundAlpha)));

    // Don't render pause menu panel if settings are being shown
    if (m_SettingsOpen) return;

    // Center the menu
    float menuWidth = 300.0f;
    float menuHeight = 400.0f;
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);
    ImVec2 menuPos(center.x - menuWidth * 0.5f, center.y - menuHeight * 0.5f);

    ImGui::SetNextWindowPos(menuPos);
    ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight));
    ImGui::SetNextWindowBgAlpha(0.9f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("PauseMenu", nullptr, flags);

    // Title
    ImVec2 titleSize = ImGui::CalcTextSize(m_Title.c_str());
    ImGui::SetCursorPosX((menuWidth - titleSize.x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("%s", m_Title.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Menu options
    for (size_t i = 0; i < m_MenuOptions.size(); ++i) {
        bool isSelected = (static_cast<int>(i) == m_SelectedOption);

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
        }

        float buttonWidth = menuWidth - 40.0f;
        ImGui::SetCursorPosX(20.0f);
        if (ImGui::Button(m_MenuOptions[i].c_str(), ImVec2(buttonWidth, 40.0f))) {
            ExecuteOption(static_cast<int>(i));
        }

        if (isSelected) {
            ImGui::PopStyleColor(2);
        }

        ImGui::Spacing();
    }

    // Back button at bottom
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX(20.0f);
    if (ImGui::Button("Back", ImVec2(menuWidth - 40.0f, 40.0f))) {
        // If settings are open, close them first
        if (m_SettingsOpen && m_SettingsCallback) {
            // Close settings by toggling them (settings callback should handle this)
            // Actually, we need a close settings callback
            ExecuteOption(0); // For now, just resume
        } else {
            ExecuteOption(0); // Resume/Back
        }
    }

    ImGui::End();
}

} // namespace Solstice::Game

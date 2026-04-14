#pragma once

#include "../../Solstice.hxx"
#include "App/GameState.hxx"
#include "App/InputManager.hxx"
#include "UI/LevelSelector.hxx"
#include "../../UI/Widgets/Widgets.hxx"
#include <string>
#include <functional>
#include <vector>

namespace Solstice::Game {

// Main menu class
class SOLSTICE_API MainMenu {
public:
    MainMenu();
    ~MainMenu() = default;

    // Show/hide menu
    void Show();
    void Hide();
    bool IsVisible() const { return m_IsVisible; }

    // Render (call each frame when visible)
    void Render(int ScreenWidth, int ScreenHeight, float DeltaTime);

    // Update (call each frame)
    void Update(float DeltaTime, InputManager& InputManager, GameState& GameState);

    // Menu callbacks
    using MenuCallback = std::function<void()>;
    void SetNewGameCallback(MenuCallback Callback) { m_NewGameCallback = Callback; }
    void SetLoadGameCallback(MenuCallback Callback) { m_LoadGameCallback = Callback; }
    void SetSettingsCallback(MenuCallback Callback) { m_SettingsCallback = Callback; }
    void SetQuitCallback(MenuCallback Callback) { m_QuitCallback = Callback; }

    // Customization
    void SetTitle(const std::string& Title) { m_Title = Title; }
    void SetSubtitle(const std::string& Subtitle) { m_Subtitle = Subtitle; }
    void SetShowLevelSelector(bool Show) { m_ShowLevelSelector = Show; }

    // Color customization
    void SetPrimaryColor(float R, float G, float B, float A = 1.0f) { m_PrimaryColor[0] = R; m_PrimaryColor[1] = G; m_PrimaryColor[2] = B; m_PrimaryColor[3] = A; }
    void SetSecondaryColor(float R, float G, float B, float A = 1.0f) { m_SecondaryColor[0] = R; m_SecondaryColor[1] = G; m_SecondaryColor[2] = B; m_SecondaryColor[3] = A; }
    void SetAccentColor(float R, float G, float B, float A = 1.0f) { m_AccentColor[0] = R; m_AccentColor[1] = G; m_AccentColor[2] = B; m_AccentColor[3] = A; }

    // Level selector integration
    void SetLevelSelector(LevelSelector* Selector) { m_LevelSelector = Selector; }
    void CloseLevelSelector() { m_ShowLevelSelectorMenu = false; }
    bool IsLevelSelectorMenuVisible() const { return m_ShowLevelSelectorMenu; }

    // Settings menu integration
    void CloseSettingsMenu() { m_ShowSettingsMenu = false; }

private:
    bool m_IsVisible{true}; // Main menu is visible by default
    std::string m_Title{"SOLSTICE ENGINE"};
    std::string m_Subtitle{"Main Menu"};
    bool m_ShowLevelSelector{false};

    // Menu state
    int m_SelectedOption{0};
    std::vector<std::string> m_MenuOptions{"New Game", "Load Game", "Settings", "Quit"};

    // Sub-menus
    bool m_ShowLoadGameMenu{false};
    bool m_ShowSettingsMenu{false};
    bool m_ShowLevelSelectorMenu{false};
    LevelSelector* m_LevelSelector{nullptr};

    // Callbacks
    MenuCallback m_NewGameCallback;
    MenuCallback m_LoadGameCallback;
    MenuCallback m_SettingsCallback;
    MenuCallback m_QuitCallback;

    // Customization
    float m_PrimaryColor[4]{0.04f, 0.04f, 0.06f, 1.0f};   // Black/Dark Grey
    float m_SecondaryColor[4]{0.12f, 0.12f, 0.15f, 1.0f}; // Grey
    float m_AccentColor[4]{1.0f, 0.55f, 0.0f, 1.0f};      // Orange

    // Internal methods
    void HandleInput(InputManager& InputManager, GameState& GameState);
    void ExecuteOption(int Option);
    void RenderLoadGameMenu(int ScreenWidth, int ScreenHeight);
    void RenderSaveSlots(int ScreenWidth, int ScreenHeight);
};

} // namespace Solstice::Game

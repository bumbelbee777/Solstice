#pragma once

#include "../Solstice.hxx"
#include "GameState.hxx"
#include "InputManager.hxx"
#include "../UI/Widgets.hxx"
#include <string>
#include <functional>
#include <vector>

namespace Solstice::Game {

// Pause menu class
class SOLSTICE_API PauseMenu {
public:
    PauseMenu();
    ~PauseMenu() = default;

    // Show/hide menu
    void Show();
    void Hide();
    bool IsVisible() const { return m_IsVisible; }
    void Toggle();

    // Render (call each frame when visible)
    void Render(int ScreenWidth, int ScreenHeight);

    // Update (call each frame)
    void Update(float DeltaTime, InputManager& InputManager, GameState& GameState);

    // Menu options callbacks
    using MenuCallback = std::function<void()>;
    void SetResumeCallback(MenuCallback Callback) { m_ResumeCallback = Callback; }
    void SetSettingsCallback(MenuCallback Callback) { m_SettingsCallback = Callback; }
    void SetSaveCallback(MenuCallback Callback) { m_SaveCallback = Callback; }
    void SetQuitCallback(MenuCallback Callback) { m_QuitCallback = Callback; }

    // Customization
    void SetTitle(const std::string& Title) { m_Title = Title; }
    void SetBackgroundAlpha(float Alpha) { m_BackgroundAlpha = Alpha; }

    // Settings state tracking
    void SetSettingsOpen(bool Open) { m_SettingsOpen = Open; }
    bool IsSettingsOpen() const { return m_SettingsOpen; }

private:
    bool m_IsVisible{false};
    std::string m_Title{"PAUSED"};
    float m_BackgroundAlpha{0.8f};

    // Menu state
    int m_SelectedOption{0};
    std::vector<std::string> m_MenuOptions{"Resume", "Settings", "Save", "Quit"};

    // Callbacks
    MenuCallback m_ResumeCallback;
    MenuCallback m_SettingsCallback;
    MenuCallback m_SaveCallback;
    MenuCallback m_QuitCallback;

    // Settings state
    bool m_SettingsOpen{false};

    // Internal methods
    void HandleInput(InputManager& InputManager, GameState& GameState);
    void ExecuteOption(int Option);
};

} // namespace Solstice::Game

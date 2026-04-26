#pragma once

#include "../../Solstice.hxx"
#include "../../UI/Widgets/Widgets.hxx"
#include <string>
#include <unordered_map>
#include <functional>

namespace Solstice::Render {
class SoftwareRenderer;
}

namespace Solstice::Game {

// Settings categories
enum class SettingsCategory {
    Graphics,
    Audio,
    Controls,
    Gameplay
};

// Graphics settings
struct GraphicsSettings {
    int ResolutionWidth{1920};
    int ResolutionHeight{1080};
    bool Fullscreen{false};
    bool VSync{true};
    int MSAA{8}; // 0, 2, 4, 8 — drives HDR scene MSAA; TAA runs on the resolved buffer
    float RenderDistance{1000.0f};
    float FOV{75.0f};
    bool Shadows{true};
    int ShadowQuality{2}; // 0=low, 1=medium, 2=high
    bool Bloom{true};
    bool MotionBlur{false};
};

// Audio settings
struct AudioSettings {
    float MasterVolume{1.0f};
    float MusicVolume{0.7f};
    float SFXVolume{1.0f};
    float VoiceVolume{1.0f};
    bool MuteOnFocusLoss{false};
};

// Controls settings
struct ControlsSettings {
    float MouseSensitivity{1.0f};
    bool InvertMouseY{false};
    bool InvertMouseX{false};
    int KeyForward{26}; // W
    int KeyBackward{22}; // S
    int KeyLeft{4}; // A
    int KeyRight{7}; // D
    int KeyJump{44}; // Space
    int KeyCrouch{29}; // Ctrl
    int KeySprint{225}; // Shift
    int KeyInteract{8}; // E
    int KeyReload{21}; // R
    int KeyPause{41}; // ESC
};

// Gameplay settings
struct GameplaySettings {
    float Difficulty{1.0f}; // 0.5=easy, 1.0=normal, 1.5=hard
    bool AutoSave{true};
    float AutoSaveInterval{300.0f}; // seconds
    bool ShowCrosshair{true};
    bool ShowHUD{true};
    bool ShowMinimap{true};
    bool ShowDamageNumbers{true};
    float SubtitlesSize{1.0f};
    bool SubtitlesEnabled{true};
};

// Game preferences manager
class SOLSTICE_API GamePreferences {
public:
    GamePreferences();
    ~GamePreferences() = default;

    // Load/Save preferences
    bool Load(const std::string& FilePath = "preferences.json");
    bool Save(const std::string& FilePath = "preferences.json");

    // Settings access
    GraphicsSettings& GetGraphicsSettings() { return m_GraphicsSettings; }
    AudioSettings& GetAudioSettings() { return m_AudioSettings; }
    ControlsSettings& GetControlsSettings() { return m_ControlsSettings; }
    GameplaySettings& GetGameplaySettings() { return m_GameplaySettings; }

    const GraphicsSettings& GetGraphicsSettings() const { return m_GraphicsSettings; }
    const AudioSettings& GetAudioSettings() const { return m_AudioSettings; }
    const ControlsSettings& GetControlsSettings() const { return m_ControlsSettings; }
    const GameplaySettings& GetGameplaySettings() const { return m_GameplaySettings; }

    // Apply settings to systems
    void ApplyGraphicsSettings();

    /// Push MSAA / VSync from `m_GraphicsSettings` to the renderer (call after `SoftwareRenderer` exists).
    void SyncRenderer(Solstice::Render::SoftwareRenderer& renderer);
    void ApplyAudioSettings();
    void ApplyControlsSettings();

    // Reset to defaults
    void ResetToDefaults();

    // UI rendering
    void RenderSettingsMenu(int ScreenWidth, int ScreenHeight);

    // Callbacks
    using SettingsChangedCallback = std::function<void()>;
    void SetSettingsChangedCallback(SettingsChangedCallback Callback) { m_SettingsChangedCallback = Callback; }
    
    using SettingsClosedCallback = std::function<void()>;
    void SetSettingsClosedCallback(SettingsClosedCallback Callback) { m_SettingsClosedCallback = Callback; }

private:
    GraphicsSettings m_GraphicsSettings;
    AudioSettings m_AudioSettings;
    ControlsSettings m_ControlsSettings;
    GameplaySettings m_GameplaySettings;

    std::string m_PreferencesPath{"preferences.json"};
    SettingsChangedCallback m_SettingsChangedCallback;
    SettingsClosedCallback m_SettingsClosedCallback;
    SettingsCategory m_CurrentCategory{SettingsCategory::Graphics};

    // Internal serialization
    std::string SerializeToJSON() const;
    bool DeserializeFromJSON(const std::string& JSON);

    // UI rendering helpers
    void RenderGraphicsSettings();
    void RenderAudioSettings();
    void RenderControlsSettings();
    void RenderGameplaySettings();
};

} // namespace Solstice::Game

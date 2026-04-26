#include "App/GamePreferences.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Core/Audio/Audio.hxx"
#include "../../UI/Core/UISystem.hxx"
#include <Render/SoftwareRenderer.hxx>
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace Solstice::Game {

GamePreferences::GamePreferences() {
    // Try to load existing preferences
    Load();
}

bool GamePreferences::Load(const std::string& FilePath) {
    m_PreferencesPath = FilePath;

    std::ifstream file(FilePath);
    if (!file.is_open()) {
        SIMPLE_LOG("GamePreferences: Could not open preferences file, using defaults");
        ResetToDefaults();
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    if (DeserializeFromJSON(json)) {
        SIMPLE_LOG("GamePreferences: Loaded preferences from " + FilePath);
        ApplyGraphicsSettings();
        ApplyAudioSettings();
        ApplyControlsSettings();
        return true;
    } else {
        SIMPLE_LOG("GamePreferences: Failed to parse preferences, using defaults");
        ResetToDefaults();
        return false;
    }
}

bool GamePreferences::Save(const std::string& FilePath) {
    std::string path = FilePath.empty() ? m_PreferencesPath : FilePath;

    std::ofstream file(path);
    if (!file.is_open()) {
        SIMPLE_LOG("GamePreferences: Could not open preferences file for writing: " + path);
        return false;
    }

    std::string json = SerializeToJSON();
    file << json;
    file.close();

    SIMPLE_LOG("GamePreferences: Saved preferences to " + path);
    return true;
}

void GamePreferences::ResetToDefaults() {
    m_GraphicsSettings = GraphicsSettings();
    m_AudioSettings = AudioSettings();
    m_ControlsSettings = ControlsSettings();
    m_GameplaySettings = GameplaySettings();
}

void GamePreferences::ApplyGraphicsSettings() {
    SIMPLE_LOG("GamePreferences: Applying graphics settings (use SyncRenderer when a renderer is available)");
}

void GamePreferences::SyncRenderer(Solstice::Render::SoftwareRenderer& renderer) {
    uint8_t samples = 1;
    if (m_GraphicsSettings.MSAA >= 8) {
        samples = 8;
    } else if (m_GraphicsSettings.MSAA >= 4) {
        samples = 4;
    } else if (m_GraphicsSettings.MSAA >= 2) {
        samples = 2;
    }
    renderer.SetSceneMsaaSamples(samples);
    renderer.SetVSync(m_GraphicsSettings.VSync);
}

void GamePreferences::ApplyAudioSettings() {
    // Apply audio settings
    auto& audioManager = Core::Audio::AudioManager::Instance();
    audioManager.SetMusicVolume(m_AudioSettings.MusicVolume * m_AudioSettings.MasterVolume);
    audioManager.SetSoundVolume(m_AudioSettings.SFXVolume * m_AudioSettings.MasterVolume);
    SIMPLE_LOG("GamePreferences: Applying audio settings");
}

void GamePreferences::ApplyControlsSettings() {
    // Apply control settings
    // This would integrate with InputManager
    SIMPLE_LOG("GamePreferences: Applying controls settings");
}

void GamePreferences::RenderSettingsMenu(int ScreenWidth, int ScreenHeight) {
    (void)ScreenWidth;
    (void)ScreenHeight;

    float menuWidth = 600.0f;
    float menuHeight = 500.0f;
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);
    ImVec2 menuPos(center.x - menuWidth * 0.5f, center.y - menuHeight * 0.5f);

    ImGui::SetNextWindowPos(menuPos);
    ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight));

    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Category tabs
    if (ImGui::Button("Graphics")) { m_CurrentCategory = SettingsCategory::Graphics; }
    ImGui::SameLine();
    if (ImGui::Button("Audio")) { m_CurrentCategory = SettingsCategory::Audio; }
    ImGui::SameLine();
    if (ImGui::Button("Controls")) { m_CurrentCategory = SettingsCategory::Controls; }
    ImGui::SameLine();
    if (ImGui::Button("Gameplay")) { m_CurrentCategory = SettingsCategory::Gameplay; }
    ImGui::Separator();

    // Render category-specific settings
    switch (m_CurrentCategory) {
        case SettingsCategory::Graphics:
            RenderGraphicsSettings();
            break;
        case SettingsCategory::Audio:
            RenderAudioSettings();
            break;
        case SettingsCategory::Controls:
            RenderControlsSettings();
            break;
        case SettingsCategory::Gameplay:
            RenderGameplaySettings();
            break;
    }

    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(100, 30))) {
        Save();
        if (m_SettingsChangedCallback) {
            m_SettingsChangedCallback();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 30))) {
        Load(); // Reload to discard changes
    }
    ImGui::SameLine();
    if (ImGui::Button("Defaults", ImVec2(100, 30))) {
        ResetToDefaults();
    }
    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(100, 30))) {
        // Back button - close settings menu
        if (m_SettingsClosedCallback) {
            m_SettingsClosedCallback();
        }
    }

    ImGui::End();
}

void GamePreferences::RenderGraphicsSettings() {
    ImGui::Text("Graphics Settings");
    ImGui::Separator();

    ImGui::Checkbox("Fullscreen", &m_GraphicsSettings.Fullscreen);
    ImGui::Checkbox("VSync", &m_GraphicsSettings.VSync);

    const char* msaaOptions[] = {"Off", "2x", "4x", "8x"};
    int msaaIndex = 0;
    if (m_GraphicsSettings.MSAA == 2) msaaIndex = 1;
    else if (m_GraphicsSettings.MSAA == 4) msaaIndex = 2;
    else if (m_GraphicsSettings.MSAA == 8) msaaIndex = 3;

    if (ImGui::Combo("MSAA", &msaaIndex, msaaOptions, 4)) {
        m_GraphicsSettings.MSAA = (msaaIndex == 0) ? 0 : (1 << msaaIndex);
    }

    ImGui::SliderFloat("FOV", &m_GraphicsSettings.FOV, 60.0f, 120.0f);
    ImGui::SliderFloat("Render Distance", &m_GraphicsSettings.RenderDistance, 100.0f, 5000.0f);

    ImGui::Checkbox("Shadows", &m_GraphicsSettings.Shadows);
    ImGui::SliderInt("Shadow Quality", &m_GraphicsSettings.ShadowQuality, 0, 2);

    ImGui::Checkbox("Bloom", &m_GraphicsSettings.Bloom);
    ImGui::Checkbox("Motion Blur", &m_GraphicsSettings.MotionBlur);
}

void GamePreferences::RenderAudioSettings() {
    ImGui::Text("Audio Settings");
    ImGui::Separator();

    ImGui::SliderFloat("Master Volume", &m_AudioSettings.MasterVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("Music Volume", &m_AudioSettings.MusicVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("SFX Volume", &m_AudioSettings.SFXVolume, 0.0f, 1.0f);
    ImGui::SliderFloat("Voice Volume", &m_AudioSettings.VoiceVolume, 0.0f, 1.0f);
    ImGui::Checkbox("Mute on Focus Loss", &m_AudioSettings.MuteOnFocusLoss);
}

void GamePreferences::RenderControlsSettings() {
    ImGui::Text("Controls Settings");
    ImGui::Separator();

    ImGui::SliderFloat("Mouse Sensitivity", &m_ControlsSettings.MouseSensitivity, 0.1f, 5.0f);
    ImGui::Checkbox("Invert Mouse Y", &m_ControlsSettings.InvertMouseY);
    ImGui::Checkbox("Invert Mouse X", &m_ControlsSettings.InvertMouseX);

    ImGui::Text("Key Bindings:");
    ImGui::Text("Forward: %d", m_ControlsSettings.KeyForward);
    ImGui::Text("Backward: %d", m_ControlsSettings.KeyBackward);
    ImGui::Text("Left: %d", m_ControlsSettings.KeyLeft);
    ImGui::Text("Right: %d", m_ControlsSettings.KeyRight);
    ImGui::Text("Jump: %d", m_ControlsSettings.KeyJump);
    ImGui::Text("Crouch: %d", m_ControlsSettings.KeyCrouch);
    ImGui::Text("Sprint: %d", m_ControlsSettings.KeySprint);
}

void GamePreferences::RenderGameplaySettings() {
    ImGui::Text("Gameplay Settings");
    ImGui::Separator();

    ImGui::SliderFloat("Difficulty", &m_GameplaySettings.Difficulty, 0.5f, 1.5f);
    ImGui::Checkbox("Auto Save", &m_GameplaySettings.AutoSave);
    ImGui::SliderFloat("Auto Save Interval (seconds)", &m_GameplaySettings.AutoSaveInterval, 60.0f, 600.0f);

    ImGui::Checkbox("Show Crosshair", &m_GameplaySettings.ShowCrosshair);
    ImGui::Checkbox("Show HUD", &m_GameplaySettings.ShowHUD);
    ImGui::Checkbox("Show Minimap", &m_GameplaySettings.ShowMinimap);
    ImGui::Checkbox("Show Damage Numbers", &m_GameplaySettings.ShowDamageNumbers);

    ImGui::Checkbox("Subtitles Enabled", &m_GameplaySettings.SubtitlesEnabled);
    ImGui::SliderFloat("Subtitles Size", &m_GameplaySettings.SubtitlesSize, 0.5f, 2.0f);
}

std::string GamePreferences::SerializeToJSON() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "{\n";
    ss << "  \"graphics\": {\n";
    ss << "    \"resolutionWidth\": " << m_GraphicsSettings.ResolutionWidth << ",\n";
    ss << "    \"resolutionHeight\": " << m_GraphicsSettings.ResolutionHeight << ",\n";
    ss << "    \"fullscreen\": " << (m_GraphicsSettings.Fullscreen ? "true" : "false") << ",\n";
    ss << "    \"vsync\": " << (m_GraphicsSettings.VSync ? "true" : "false") << ",\n";
    ss << "    \"msaa\": " << m_GraphicsSettings.MSAA << ",\n";
    ss << "    \"renderDistance\": " << m_GraphicsSettings.RenderDistance << ",\n";
    ss << "    \"fov\": " << m_GraphicsSettings.FOV << ",\n";
    ss << "    \"shadows\": " << (m_GraphicsSettings.Shadows ? "true" : "false") << ",\n";
    ss << "    \"shadowQuality\": " << m_GraphicsSettings.ShadowQuality << ",\n";
    ss << "    \"bloom\": " << (m_GraphicsSettings.Bloom ? "true" : "false") << ",\n";
    ss << "    \"motionBlur\": " << (m_GraphicsSettings.MotionBlur ? "true" : "false") << "\n";
    ss << "  },\n";

    ss << "  \"audio\": {\n";
    ss << "    \"masterVolume\": " << m_AudioSettings.MasterVolume << ",\n";
    ss << "    \"musicVolume\": " << m_AudioSettings.MusicVolume << ",\n";
    ss << "    \"sfxVolume\": " << m_AudioSettings.SFXVolume << ",\n";
    ss << "    \"voiceVolume\": " << m_AudioSettings.VoiceVolume << ",\n";
    ss << "    \"muteOnFocusLoss\": " << (m_AudioSettings.MuteOnFocusLoss ? "true" : "false") << "\n";
    ss << "  },\n";

    ss << "  \"controls\": {\n";
    ss << "    \"mouseSensitivity\": " << m_ControlsSettings.MouseSensitivity << ",\n";
    ss << "    \"invertMouseY\": " << (m_ControlsSettings.InvertMouseY ? "true" : "false") << ",\n";
    ss << "    \"invertMouseX\": " << (m_ControlsSettings.InvertMouseX ? "true" : "false") << "\n";
    ss << "  },\n";

    ss << "  \"gameplay\": {\n";
    ss << "    \"difficulty\": " << m_GameplaySettings.Difficulty << ",\n";
    ss << "    \"autoSave\": " << (m_GameplaySettings.AutoSave ? "true" : "false") << ",\n";
    ss << "    \"autoSaveInterval\": " << m_GameplaySettings.AutoSaveInterval << ",\n";
    ss << "    \"showCrosshair\": " << (m_GameplaySettings.ShowCrosshair ? "true" : "false") << ",\n";
    ss << "    \"showHUD\": " << (m_GameplaySettings.ShowHUD ? "true" : "false") << ",\n";
    ss << "    \"showMinimap\": " << (m_GameplaySettings.ShowMinimap ? "true" : "false") << ",\n";
    ss << "    \"showDamageNumbers\": " << (m_GameplaySettings.ShowDamageNumbers ? "true" : "false") << ",\n";
    ss << "    \"subtitlesSize\": " << m_GameplaySettings.SubtitlesSize << ",\n";
    ss << "    \"subtitlesEnabled\": " << (m_GameplaySettings.SubtitlesEnabled ? "true" : "false") << "\n";
    ss << "  }\n";
    ss << "}";

    return ss.str();
}

bool GamePreferences::DeserializeFromJSON(const std::string& JSON) {
    // Simple JSON parsing (in production, use a proper JSON library)
    // For now, just return true and use defaults
    // This is a placeholder - a full implementation would parse the JSON properly
    (void)JSON;
    return true;
}

} // namespace Solstice::Game

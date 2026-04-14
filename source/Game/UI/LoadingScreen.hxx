#pragma once

#include "../../Solstice.hxx"
#include "../../UI/Widgets/Widgets.hxx"
#include <string>
#include <functional>

namespace Solstice::Game {

// Loading screen styles
enum class LoadingScreenStyle {
    Minimal,
    Animated,
    ProgressBar,
    SplashScreen
};

// Loading screen class
class SOLSTICE_API LoadingScreen {
public:
    LoadingScreen();
    ~LoadingScreen() = default;

    // Show/hide loading screen
    void Show();
    void Hide();
    bool IsVisible() const { return m_IsVisible; }

    // Progress management
    void SetProgress(float Progress); // 0.0 to 1.0
    float GetProgress() const { return m_Progress; }
    void SetProgressText(const std::string& Text);
    const std::string& GetProgressText() const { return m_ProgressText; }

    // Style configuration
    void SetStyle(LoadingScreenStyle Style) { m_Style = Style; }
    LoadingScreenStyle GetStyle() const { return m_Style; }

    // Customization
    void SetBackgroundColor(float R, float G, float B, float A = 1.0f);
    void SetTextColor(float R, float G, float B, float A = 1.0f);
    void SetAccentColor(float R, float G, float B, float A = 1.0f);
    void SetTitle(const std::string& Title);
    void SetSubtitle(const std::string& Subtitle);

    // Render (call each frame when visible)
    void Render(int ScreenWidth, int ScreenHeight, float DeltaTime);

    // Callbacks
    using ProgressCallback = std::function<void(float)>;
    void SetProgressCallback(ProgressCallback Callback) { m_ProgressCallback = Callback; }

private:
    bool m_IsVisible{false};
    float m_Progress{0.0f};
    std::string m_ProgressText;
    LoadingScreenStyle m_Style{LoadingScreenStyle::ProgressBar};

    // Visual customization
    float m_BackgroundColor[4]{0.04f, 0.04f, 0.06f, 1.0f};
    float m_TextColor[4]{0.7f, 0.7f, 0.8f, 1.0f};
    float m_AccentColor[4]{1.0f, 0.55f, 0.0f, 1.0f};
    std::string m_Title;
    std::string m_Subtitle;

    // Animation
    float m_AnimationTime{0.0f};
    float m_SpinnerRotation{0.0f};

    // Callbacks
    ProgressCallback m_ProgressCallback;

    // Internal rendering methods
    void RenderMinimal(int ScreenWidth, int ScreenHeight);
    void RenderAnimated(int ScreenWidth, int ScreenHeight, float DeltaTime);
    void RenderProgressBar(int ScreenWidth, int ScreenHeight);
    void RenderSplashScreen(int ScreenWidth, int ScreenHeight, float DeltaTime);
};

} // namespace Solstice::Game

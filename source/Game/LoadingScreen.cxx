#include "LoadingScreen.hxx"
#include "../Core/Debug.hxx"
#include "../UI/UISystem.hxx"
#include <imgui.h>
#include <cmath>

namespace Solstice::Game {

LoadingScreen::LoadingScreen() {
    m_Title = "GAME NAME";
    m_Subtitle = "LOADING GAME...";
    SetBackgroundColor(0.04f, 0.04f, 0.06f, 1.0f); // Sleek Black
    m_Style = LoadingScreenStyle::ProgressBar; // Default to the enhanced progress bar style
    m_IsVisible = true; // Make visible by default during initialization
}

void LoadingScreen::Show() {
    m_IsVisible = true;
    m_Progress = 0.0f;
    m_AnimationTime = 0.0f;
    m_SpinnerRotation = 0.0f;
}

void LoadingScreen::Hide() {
    m_IsVisible = false;
}

void LoadingScreen::SetProgress(float Progress) {
    m_Progress = std::max(0.0f, std::min(1.0f, Progress));
    if (m_ProgressCallback) {
        m_ProgressCallback(m_Progress);
    }
}

void LoadingScreen::SetProgressText(const std::string& Text) {
    m_ProgressText = Text;
}

void LoadingScreen::SetBackgroundColor(float R, float G, float B, float A) {
    m_BackgroundColor[0] = R;
    m_BackgroundColor[1] = G;
    m_BackgroundColor[2] = B;
    m_BackgroundColor[3] = A;
}

void LoadingScreen::SetTextColor(float R, float G, float B, float A) {
    m_TextColor[0] = R;
    m_TextColor[1] = G;
    m_TextColor[2] = B;
    m_TextColor[3] = A;
}

void LoadingScreen::SetAccentColor(float R, float G, float B, float A) {
    m_AccentColor[0] = R;
    m_AccentColor[1] = G;
    m_AccentColor[2] = B;
    m_AccentColor[3] = A;
}

void LoadingScreen::SetTitle(const std::string& Title) {
    m_Title = Title;
}

void LoadingScreen::SetSubtitle(const std::string& Subtitle) {
    m_Subtitle = Subtitle;
}

void LoadingScreen::Render(int ScreenWidth, int ScreenHeight, float DeltaTime) {
    if (!m_IsVisible) return;

    m_AnimationTime += DeltaTime;
    m_SpinnerRotation += DeltaTime * 360.0f; // Rotate 360 degrees per second

    // Set fullscreen window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight)));
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("LoadingScreen", nullptr, flags);

    // Default Background (Black/Dark Grey)
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 min = ImVec2(0, 0);
    ImVec2 max = ImVec2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));
    drawList->AddRectFilledMultiColor(min, max,
        IM_COL32(10, 10, 15, 255), IM_COL32(10, 10, 15, 255),
        IM_COL32(25, 25, 30, 255), IM_COL32(25, 25, 30, 255));

    // Render based on style
    switch (m_Style) {
        case LoadingScreenStyle::Minimal:
            RenderMinimal(ScreenWidth, ScreenHeight);
            break;
        case LoadingScreenStyle::Animated:
            RenderAnimated(ScreenWidth, ScreenHeight, DeltaTime);
            break;
        case LoadingScreenStyle::ProgressBar:
            RenderProgressBar(ScreenWidth, ScreenHeight);
            break;
        case LoadingScreenStyle::SplashScreen:
            RenderSplashScreen(ScreenWidth, ScreenHeight, DeltaTime);
            break;
    }

    ImGui::End();
}

void LoadingScreen::RenderMinimal(int ScreenWidth, int ScreenHeight) {
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);

    ImGui::SetCursorPos(ImVec2(center.x - 100.0f, center.y - 20.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_TextColor[0], m_TextColor[1], m_TextColor[2], m_TextColor[3]));
    ImGui::Text("%s", m_Title.c_str());
    ImGui::PopStyleColor();
}

void LoadingScreen::RenderAnimated(int ScreenWidth, int ScreenHeight, float DeltaTime) {
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);

    // Draw spinner (Orange)
    float radius = 40.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int i = 0; i < 12; ++i) {
        float angle = (m_SpinnerRotation + i * 30.0f) * (3.14159f / 180.0f);
        float x1 = center.x + std::cos(angle) * radius;
        float y1 = center.y + std::sin(angle) * radius;

        float alpha = 1.0f - (static_cast<float>(i) / 12.0f);
        ImU32 dotColor = IM_COL32(255, 140, 0, static_cast<int>(alpha * 255.0f));
        drawList->AddCircleFilled(ImVec2(x1, y1), 5.0f + (1.0f - alpha) * 2.0f, dotColor);

        // Add subtle glow
        drawList->AddCircle(ImVec2(x1, y1), 7.0f + (1.0f - alpha) * 2.0f, IM_COL32(255, 140, 0, static_cast<int>(alpha * 50.0f)), 0, 1.5f);
    }

    // Title
    ImGui::SetCursorPos(ImVec2(center.x - 150.0f, center.y + 80.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.0f, 1.0f));
    ImGui::SetWindowFontScale(2.0f);
    ImGui::Text("%s", m_Title.c_str());
    ImGui::SetWindowFontScale(1.0f);
    if (!m_ProgressText.empty()) {
        ImGui::SetCursorPos(ImVec2(center.x - 100.0f, center.y + 130.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.9f, 0.8f));
        ImGui::Text("%s", m_ProgressText.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();
}

void LoadingScreen::RenderProgressBar(int ScreenWidth, int ScreenHeight) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);

    // 1. Glossy Background Gradient
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(m_BackgroundColor[0], m_BackgroundColor[1], m_BackgroundColor[2], m_BackgroundColor[3]));
    ImU32 accentColor = ImGui::ColorConvertFloat4ToU32(ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], m_AccentColor[3]));

    ImVec2 bgMin(0, 0);
    ImVec2 bgMax(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));
    drawList->AddRectFilledMultiColor(bgMin, bgMax,
        bgColor, bgColor,
        IM_COL32(30, 30, 40, 255), IM_COL32(30, 30, 40, 255));

    // 2. Frutiger Aero Glossy Header
    float headerHeight = 150.0f;
    drawList->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(static_cast<float>(ScreenWidth), headerHeight),
        IM_COL32(60, 60, 80, 80), IM_COL32(60, 60, 80, 80),
        ImGui::GetColorU32(ImVec4(m_BackgroundColor[0], m_BackgroundColor[1], m_BackgroundColor[2], 0.0f)),
        ImGui::GetColorU32(ImVec4(m_BackgroundColor[0], m_BackgroundColor[1], m_BackgroundColor[2], 0.0f)));

    // 3. Main Title (Sleek Accent Color)
    ImGui::SetCursorPos(ImVec2(60.0f, 50.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], m_AccentColor[3]));
    ImGui::SetWindowFontScale(2.5f);
    ImGui::Text("%s", m_Title.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    if (!m_Subtitle.empty()) {
        ImGui::SetCursorPos(ImVec2(65.0f, 110.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_TextColor[0], m_TextColor[1], m_TextColor[2], m_TextColor[3]));
        ImGui::Text("%s", m_Subtitle.c_str());
        ImGui::PopStyleColor();
    }

    // 4. Progress bar (Glassy Skeuomorphic)
    float barWidth = 600.0f;
    float barHeight = 28.0f;
    ImVec2 barPos(center.x - barWidth * 0.5f, center.y + 100.0f);
    ImVec2 barMax(barPos.x + barWidth, barPos.y + barHeight);

    // Bar glass background
    drawList->AddRectFilled(barPos, barMax, IM_COL32(40, 40, 55, 180), 14.0f);
    drawList->AddRect(barPos, barMax, IM_COL32(120, 120, 150, 100), 14.0f, 0, 2.0f);

    // Progress fill (Vibrant Accent Color with glow)
    float fillWidth = barWidth * m_Progress;
    if (fillWidth > 14.0f) {
        ImVec2 fillMax(barPos.x + fillWidth, barMax.y);

        // Accent gradient
        ImU32 accentBright = ImGui::GetColorU32(ImVec4(
            std::min(1.0f, m_AccentColor[0] * 1.2f),
            std::min(1.0f, m_AccentColor[1] * 1.2f),
            std::min(1.0f, m_AccentColor[2] * 1.2f), 1.0f));

        drawList->AddRectFilledMultiColor(barPos, fillMax,
            accentColor, accentBright, accentBright, accentColor);

        // Top highlight (Gloss)
        drawList->AddRectFilledMultiColor(
            ImVec2(barPos.x + 4, barPos.y + 2),
            ImVec2(barPos.x + fillWidth - 4, barPos.y + barHeight * 0.45f),
            IM_COL32(255, 255, 255, 120), IM_COL32(255, 255, 255, 120),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

        // Inner glow
        drawList->AddRect(barPos, fillMax, ImGui::GetColorU32(ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], 0.6f)), 14.0f, 0, 1.0f);
    }

    // 5. "Built using Solstice (TM) technology" (Fading effect)
    float techAlpha = (std::sin(m_AnimationTime * 1.5f) * 0.4f + 0.6f);
    std::string techText = "Built using Solstice (TM) technology";
    ImVec2 techTextSize = ImGui::CalcTextSize(techText.c_str());
    ImGui::SetCursorPos(ImVec2(center.x - techTextSize.x * 0.5f, ScreenHeight - 80.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_TextColor[0] * 0.7f, m_TextColor[1] * 0.7f, m_TextColor[2] * 0.7f, techAlpha * 0.5f));
    ImGui::Text("%s", techText.c_str());
    ImGui::PopStyleColor();

    // 6. Operation info
    if (!m_ProgressText.empty()) {
        ImGui::SetCursorPos(ImVec2(barPos.x + 10.0f, barPos.y - 35.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_TextColor[0], m_TextColor[1], m_TextColor[2], 0.8f));
        ImGui::Text("%s", m_ProgressText.c_str());
        ImGui::PopStyleColor();
    }

    // 7. Percentage (Right aligned above bar)
    std::string percentText = std::to_string(static_cast<int>(m_Progress * 100.0f)) + "%";
    ImVec2 percentSize = ImGui::CalcTextSize(percentText.c_str());
    ImGui::SetCursorPos(ImVec2(barMax.x - percentSize.x, barPos.y - 35.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_AccentColor[0], m_AccentColor[1], m_AccentColor[2], 0.9f));
    ImGui::Text("%s", percentText.c_str());
    ImGui::PopStyleColor();
}

void LoadingScreen::RenderSplashScreen(int ScreenWidth, int ScreenHeight, float DeltaTime) {
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);

    // Fade in/out effect
    float fadeTime = 1.0f;
    float alpha = 1.0f;
    if (m_AnimationTime < fadeTime) {
        alpha = m_AnimationTime / fadeTime;
    } else if (m_Progress >= 1.0f && m_AnimationTime > fadeTime) {
        alpha = 1.0f - ((m_AnimationTime - fadeTime) / fadeTime);
        alpha = std::max(0.0f, alpha);
    }

    // Title with fade
    ImGui::SetCursorPos(ImVec2(center.x - 200.0f, center.y - 50.0f));
    ImVec4 titleColor(m_TextColor[0], m_TextColor[1], m_TextColor[2], m_TextColor[3] * alpha);
    ImGui::PushStyleColor(ImGuiCol_Text, titleColor);
    ImGui::Text("%s", m_Title.c_str());
    ImGui::PopStyleColor();

    // Subtitle
    if (!m_Subtitle.empty()) {
        ImGui::SetCursorPos(ImVec2(center.x - 150.0f, center.y + 20.0f));
        ImVec4 subtitleColor(m_TextColor[0], m_TextColor[1], m_TextColor[2], m_TextColor[3] * alpha * 0.7f);
        ImGui::PushStyleColor(ImGuiCol_Text, subtitleColor);
        ImGui::Text("%s", m_Subtitle.c_str());
        ImGui::PopStyleColor();
    }

    // Progress bar at bottom
    float barWidth = 400.0f;
    float barHeight = 4.0f;
    float barY = static_cast<float>(ScreenHeight) - 100.0f;
    ImVec2 barMin(center.x - barWidth * 0.5f, barY);
    ImVec2 barMax(center.x + barWidth * 0.5f, barY + barHeight);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(barMin, barMax, ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.2f, 0.2f, alpha)));

    ImVec2 fillMax(barMin.x + barWidth * m_Progress, barMax.y);
    drawList->AddRectFilled(barMin, fillMax, ImGui::ColorConvertFloat4ToU32(ImVec4(
        m_TextColor[0], m_TextColor[1], m_TextColor[2], alpha)));
}

} // namespace Solstice::Game

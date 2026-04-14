#include "Dialogue/VisualNovelPresets.hxx"

namespace Solstice::Game {

VisualNovelConfig VisualNovelPresets::GetClassicPreset() {
    VisualNovelConfig Config;
    Config.BoxAnchor = UI::ViewportUI::Anchor::BottomCenter;
    Config.BoxWidth = 800.0f;
    Config.BoxHeight = 180.0f;
    Config.BoxOffsetX = 0.0f;
    Config.BoxOffsetY = 20.0f;
    Config.NameBoxHeight = 28.0f;
    Config.NameBoxOffsetY = -8.0f;
    Config.TypewriterCharsPerSecond = DEFAULT_TYPEWRITER_CHARS_PER_SECOND;
    Config.SkipMode = false;
    Config.BackgroundAlpha = 0.9f;
    Config.BackgroundColor = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
    Config.TextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    Config.NameColor = ImVec4(0.95f, 0.9f, 0.75f, 1.0f);
    return Config;
}

VisualNovelConfig VisualNovelPresets::GetMinimalPreset() {
    VisualNovelConfig Config;
    Config.BoxAnchor = UI::ViewportUI::Anchor::BottomCenter;
    Config.BoxWidth = 700.0f;
    Config.BoxHeight = 140.0f;
    Config.BoxOffsetX = 0.0f;
    Config.BoxOffsetY = 16.0f;
    Config.NameBoxHeight = 0.0f;
    Config.NameBoxOffsetY = 0.0f;
    Config.TypewriterCharsPerSecond = 40.0f;
    Config.SkipMode = false;
    Config.BackgroundAlpha = 0.85f;
    Config.BackgroundColor = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    Config.TextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    Config.NameColor = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    return Config;
}

} // namespace Solstice::Game

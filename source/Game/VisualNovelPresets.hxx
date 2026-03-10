#pragma once

#include "../Solstice.hxx"
#include "../UI/ViewportUI.hxx"
#include <imgui.h>

namespace Solstice::Game {

// Default typewriter speed: characters per second
constexpr float DEFAULT_TYPEWRITER_CHARS_PER_SECOND = 30.0f;

// Visual novel / dialogue box configuration
struct VisualNovelConfig {
    // Box layout (anchor and size)
    UI::ViewportUI::Anchor BoxAnchor{UI::ViewportUI::Anchor::BottomCenter};
    float BoxWidth{800.0f};
    float BoxHeight{180.0f};
    float BoxOffsetX{0.0f};
    float BoxOffsetY{20.0f};

    // Name box (optional; if height > 0, show speaker name above or inside box)
    float NameBoxHeight{0.0f};  // 0 = no separate name box
    float NameBoxOffsetY{0.0f};

    // Text
    float TypewriterCharsPerSecond{DEFAULT_TYPEWRITER_CHARS_PER_SECOND};
    bool SkipMode{false};  // If true, show full text instantly

    // Appearance
    float BackgroundAlpha{0.85f};
    ImVec4 BackgroundColor{0.1f, 0.1f, 0.15f, 1.0f};
    ImVec4 TextColor{1.0f, 1.0f, 1.0f, 1.0f};
    ImVec4 NameColor{0.9f, 0.85f, 0.7f, 1.0f};
};

// Named presets for visual novel / dialogue box
class SOLSTICE_API VisualNovelPresets {
public:
    static VisualNovelConfig GetClassicPreset();
    static VisualNovelConfig GetMinimalPreset();
};

} // namespace Solstice::Game

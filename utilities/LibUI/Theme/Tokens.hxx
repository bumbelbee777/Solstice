#pragma once

#include "LibUI/Core/Core.hxx"

#include <imgui.h>

namespace LibUI::Theme {

struct Tokens {
    ImVec4 windowBg{0.10f, 0.11f, 0.12f, 1.00f};
    ImVec4 panelBg{0.13f, 0.14f, 0.16f, 1.00f};
    ImVec4 accent{0.33f, 0.57f, 0.90f, 1.00f};
    float windowRounding{6.0f};
    float frameRounding{4.0f};
    float itemSpacingX{8.0f};
    float itemSpacingY{6.0f};
};

} // namespace LibUI::Theme


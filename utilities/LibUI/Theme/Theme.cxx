#include "LibUI/Theme/Theme.hxx"

#include <imgui.h>

namespace LibUI::Theme {

Tokens SfmInspiredDefaultTokens() {
    Tokens tokens;
    tokens.windowBg = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
    tokens.panelBg = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
    tokens.accent = ImVec4(0.15f, 0.50f, 0.95f, 1.0f);
    tokens.windowRounding = 2.0f;
    tokens.frameRounding = 2.0f;
    tokens.itemSpacingX = 8.0f;
    tokens.itemSpacingY = 5.0f;
    return tokens;
}

void ApplyTokens(const Tokens& tokens) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = tokens.windowRounding;
    style.FrameRounding     = tokens.frameRounding;
    style.ChildRounding     = tokens.frameRounding;
    style.PopupRounding     = tokens.frameRounding;
    style.ScrollbarRounding = tokens.frameRounding;
    style.GrabRounding      = tokens.frameRounding;
    style.TabRounding       = 2.0f;
    style.ItemSpacing       = ImVec2(tokens.itemSpacingX, tokens.itemSpacingY);
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    const ImVec4 bg0   = tokens.windowBg;                            // deepest bg
    const ImVec4 bg1   = tokens.panelBg;                             // panel bg
    const ImVec4 bg2   = ImVec4(0.17f, 0.17f, 0.18f, 1.0f);         // slightly lighter
    const ImVec4 brd   = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);         // border / separator
    const ImVec4 acc   = tokens.accent;                               // primary accent
    const ImVec4 accHv = ImVec4(acc.x + 0.08f, acc.y + 0.06f, acc.z + 0.04f, 1.0f); // accent hovered
    const ImVec4 accAc = ImVec4(acc.x - 0.05f, acc.y - 0.05f, acc.z - 0.05f, 1.0f); // accent active
    const ImVec4 txt   = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);         // main text
    const ImVec4 txtDi = ImVec4(0.50f, 0.52f, 0.55f, 1.0f);         // disabled text
    const ImVec4 scrl  = ImVec4(0.25f, 0.25f, 0.27f, 1.0f);         // scrollbar grab

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                  = txt;
    c[ImGuiCol_TextDisabled]          = txtDi;
    c[ImGuiCol_WindowBg]              = bg0;
    c[ImGuiCol_ChildBg]               = bg1;
    c[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.10f, 0.11f, 0.98f);
    c[ImGuiCol_Border]                = brd;
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.24f, 0.24f, 0.26f, 1.0f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.12f, 0.20f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.06f, 0.07f, 0.75f);
    c[ImGuiCol_MenuBarBg]             = ImVec4(0.07f, 0.07f, 0.08f, 1.0f);
    c[ImGuiCol_ScrollbarBg]           = bg1;
    c[ImGuiCol_ScrollbarGrab]         = scrl;
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]   = acc;
    c[ImGuiCol_CheckMark]             = acc;
    c[ImGuiCol_SliderGrab]            = acc;
    c[ImGuiCol_SliderGrabActive]      = accHv;
    c[ImGuiCol_Button]                = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
    c[ImGuiCol_ButtonHovered]         = accHv;
    c[ImGuiCol_ButtonActive]          = accAc;
    c[ImGuiCol_Header]                = ImVec4(acc.x, acc.y, acc.z, 0.35f);
    c[ImGuiCol_HeaderHovered]         = ImVec4(acc.x, acc.y, acc.z, 0.55f);
    c[ImGuiCol_HeaderActive]          = acc;
    c[ImGuiCol_Separator]             = brd;
    c[ImGuiCol_SeparatorHovered]      = accHv;
    c[ImGuiCol_SeparatorActive]       = acc;
    c[ImGuiCol_ResizeGrip]            = brd;
    c[ImGuiCol_ResizeGripHovered]     = accHv;
    c[ImGuiCol_ResizeGripActive]      = acc;
    c[ImGuiCol_Tab]                   = bg2;
    c[ImGuiCol_TabHovered]            = accHv;
    c[ImGuiCol_TabSelected]           = ImVec4(acc.x, acc.y, acc.z, 0.85f);
    c[ImGuiCol_TabSelectedOverline]   = acc;
    c[ImGuiCol_TabDimmed]             = bg1;
    c[ImGuiCol_TabDimmedSelected]     = bg2;
    #ifdef ImGuiCol_DockingPreview
    c[ImGuiCol_DockingPreview]        = ImVec4(acc.x, acc.y, acc.z, 0.5f);
    #endif
    #ifdef ImGuiCol_DockingEmptyBg
    c[ImGuiCol_DockingEmptyBg]        = bg0;
    #endif
    c[ImGuiCol_PlotLines]             = acc;
    c[ImGuiCol_PlotLinesHovered]      = accHv;
    c[ImGuiCol_PlotHistogram]         = acc;
    c[ImGuiCol_PlotHistogramHovered]  = accHv;
    c[ImGuiCol_TableHeaderBg]         = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    c[ImGuiCol_TableBorderStrong]     = brd;
    c[ImGuiCol_TableBorderLight]      = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.03f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(acc.x, acc.y, acc.z, 0.35f);
    c[ImGuiCol_DragDropTarget]        = ImVec4(1.0f, 0.75f, 0.0f, 0.9f);
    c[ImGuiCol_NavHighlight]          = acc;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.35f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.55f);
}

} // namespace LibUI::Theme


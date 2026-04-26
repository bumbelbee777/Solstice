#include "Icons.hxx"

#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace LibUI::Icons {

namespace {

ImFont* s_IconFont = nullptr;
bool s_IconFontMergedIntoDefault = false;

std::string Utf8FromCodepoint(unsigned cp) {
    std::string s;
    if (cp < 0x80) {
        s.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return s;
}

// Phosphor (regular) Unicode codepoints from @phosphor-icons/core (must match bundled Phosphor.ttf).
static const unsigned kPackedCp[] = {
    57930, // folder
    57904, // file
    58558, // upload (import)
    60144, // export
    57928, // floppy-disk (save)
    57942, // folder-open (open)
    58320, // play
    58324, // plus (new)
    58774, // puzzle-piece (plugins)
    57968, // gear (settings)
    58062, // info (about)
    58886, // seal-check (validate)
    58072, // keyboard (shortcuts)
    58639, // arrow-clockwise (reload)
    58316, // copy (duplicate)
    58218, // trash (remove)
    57944, // caret-left (prev)
    57945, // caret-right (next)
    58390, // cube (mesh)
};

static_assert(static_cast<int>(Id::COUNT) == static_cast<int>(sizeof(kPackedCp) / sizeof(kPackedCp[0])), "icon map");

static const ImWchar kIconGlyphRanges[] = {
    static_cast<ImWchar>(57904), static_cast<ImWchar>(57904), // file
    static_cast<ImWchar>(57928), static_cast<ImWchar>(57930), // save/folder
    static_cast<ImWchar>(57942), static_cast<ImWchar>(57942), // folder-open
    static_cast<ImWchar>(57968), static_cast<ImWchar>(57968), // gear
    static_cast<ImWchar>(58062), static_cast<ImWchar>(58062), // info
    static_cast<ImWchar>(58072), static_cast<ImWchar>(58072), // keyboard
    static_cast<ImWchar>(58320), static_cast<ImWchar>(58324), // play/plus
    static_cast<ImWchar>(58390), static_cast<ImWchar>(58390), // cube
    static_cast<ImWchar>(58558), static_cast<ImWchar>(58558), // upload
    static_cast<ImWchar>(58639), static_cast<ImWchar>(58639), // arrow-clockwise
    static_cast<ImWchar>(58774), static_cast<ImWchar>(58774), // puzzle-piece
    static_cast<ImWchar>(58886), static_cast<ImWchar>(58886), // seal-check
    static_cast<ImWchar>(60144), static_cast<ImWchar>(60144), // export
    static_cast<ImWchar>(58218), static_cast<ImWchar>(58218), // trash
    static_cast<ImWchar>(58316), static_cast<ImWchar>(58316), // copy
    static_cast<ImWchar>(57944), static_cast<ImWchar>(57945), // carets
    0,
};

} // namespace

bool TryLoadIconFontPackFromFile(const char* pathTtf, float sizePixels) {
    if (!pathTtf || pathTtf[0] == '\0' || !std::filesystem::exists(pathTtf)) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig mergeCfg{};
    mergeCfg.MergeMode = true;
    mergeCfg.PixelSnapH = true;
    s_IconFontMergedIntoDefault = io.Fonts->AddFontFromFileTTF(pathTtf, sizePixels, &mergeCfg, kIconGlyphRanges) != nullptr;

    ImFontConfig iconCfg{};
    s_IconFont = io.Fonts->AddFontFromFileTTF(pathTtf, sizePixels, &iconCfg, kIconGlyphRanges);
    return s_IconFontMergedIntoDefault || s_IconFont != nullptr;
}

void RefreshFontGpuTexture() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    ImGui_ImplOpenGL3_CreateDeviceObjects();
}

bool TryLoadIconAtlasFromFiles(const char* pngPath, const char* manifestJsonPath) {
    (void)pngPath;
    (void)manifestJsonPath;
    return false;
}

ImFont* GetIconFont() {
    return s_IconFont;
}

const char* Glyph(Id icon) {
    switch (icon) {
    case Id::Folder:
        return "[+] ";
    case Id::File:
        return "[f] ";
    case Id::Import:
        return "<< ";
    case Id::Export:
        return ">> ";
    case Id::Save:
        return "Sv ";
    case Id::Open:
        return "Op ";
    case Id::Play:
        return "|> ";
    case Id::New:
        return "+ ";
    case Id::Plugins:
        return "Pg ";
    case Id::Settings:
        return "* ";
    case Id::About:
        return "i ";
    case Id::Validate:
        return "V ";
    case Id::Shortcuts:
        return "Kb ";
    case Id::Reload:
        return "Re ";
    case Id::Duplicate:
        return "Du ";
    case Id::Remove:
        return "Rm ";
    case Id::Prev:
        return "< ";
    case Id::Next:
        return "> ";
    case Id::Mesh:
        return "M ";
    default:
        return "";
    }
}

static bool ButtonWithIconBase(bool small, Id icon, const char* label) {
    char buf[256];
    const int ii = static_cast<int>(icon);
    if (s_IconFont && ii >= 0 && ii < static_cast<int>(Id::COUNT)) {
        const std::string g = Utf8FromCodepoint(kPackedCp[ii]);
        std::snprintf(buf, sizeof(buf), "%s%s", g.c_str(), label ? label : "");
        ImGui::PushFont(s_IconFont);
        const bool r = small ? ImGui::SmallButton(buf) : ImGui::Button(buf);
        ImGui::PopFont();
        return r;
    }
    std::snprintf(buf, sizeof(buf), "%s%s", Glyph(icon), label ? label : "");
    return small ? ImGui::SmallButton(buf) : ImGui::Button(buf);
}

bool ToolbarButton(Id icon, const char* label) {
    return ButtonWithIconBase(false, icon, label);
}

bool SmallButtonWithIcon(Id icon, const char* label) {
    return ButtonWithIconBase(true, icon, label);
}

bool MenuItemWithIcon(Id icon, const char* label, const char* shortcut, bool selected, bool enabled) {
    char buf[384];
    const int ii = static_cast<int>(icon);
    if (s_IconFontMergedIntoDefault && ii >= 0 && ii < static_cast<int>(Id::COUNT)) {
        const std::string g = Utf8FromCodepoint(kPackedCp[ii]);
        std::snprintf(buf, sizeof(buf), "%s %s", g.c_str(), label ? label : "");
    } else {
        std::snprintf(buf, sizeof(buf), "%s%s", Glyph(icon), label ? label : "");
    }
    return ImGui::MenuItem(buf, shortcut, selected, enabled);
}

} // namespace LibUI::Icons

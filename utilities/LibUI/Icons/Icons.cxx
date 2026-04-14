#include "Icons.hxx"

#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace LibUI::Icons {

namespace {

ImFont* s_IconFont = nullptr;

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

// Font Awesome 4–style PUA codepoints (when using `SOLSTICE_ICON_FONT` = fontawesome-webfont.ttf or similar).
static const unsigned kPackedCp[] = {
    0xf07b, // folder
    0xf15b, // file-text
    0xf093, // upload
    0xf0ab, // sort / export-ish (visual)
    0xf0c7, // save
    0xf115, // keyboard-o / open
    0xf04b, // play
};

} // namespace

bool TryLoadIconFontPackFromFile(const char* pathTtf, float sizePixels) {
    if (!pathTtf || pathTtf[0] == '\0' || !std::filesystem::exists(pathTtf)) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg{};
    s_IconFont = io.Fonts->AddFontFromFileTTF(pathTtf, sizePixels, &cfg);
    return s_IconFont != nullptr;
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
    default:
        return "";
    }
}

bool ToolbarButton(Id icon, const char* label) {
    char buf[160];
    const int ii = static_cast<int>(icon);
    if (s_IconFont && ii >= 0 && ii < static_cast<int>(Id::COUNT)) {
        const std::string g = Utf8FromCodepoint(kPackedCp[ii]);
        std::snprintf(buf, sizeof(buf), "%s%s", g.c_str(), label ? label : "");
        ImGui::PushFont(s_IconFont);
        const bool r = ImGui::Button(buf);
        ImGui::PopFont();
        return r;
    }
    std::snprintf(buf, sizeof(buf), "%s%s", Glyph(icon), label ? label : "");
    return ImGui::Button(buf);
}

} // namespace LibUI::Icons

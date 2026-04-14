#pragma once

#include "../Core/Core.hxx"
#include <imgui.h>

namespace LibUI::Icons {

/// Stable ids for toolbar / chrome (UTF-8 or icon-font codepoints when a pack is loaded).
enum class Id : int {
    Folder = 0,
    File,
    Import,
    Export,
    Save,
    Open,
    Play,
    COUNT
};

/// Optional Font Awesome–style `.ttf` (or any font with glyphs in the private-use block used below).
LIBUI_API bool TryLoadIconFontPackFromFile(const char* pathTtf, float sizePixels);

/// Rebuild atlas + OpenGL font texture after adding fonts (call once after `TryLoadIconFontPackFromFile`).
LIBUI_API void RefreshFontGpuTexture();

/// Future: PNG + JSON UV manifest; not implemented yet (returns false).
LIBUI_API bool TryLoadIconAtlasFromFiles(const char* pngPath, const char* manifestJsonPath);

LIBUI_API ImFont* GetIconFont();

LIBUI_API const char* Glyph(Id icon);

/// Toolbar button with icon + label
LIBUI_API bool ToolbarButton(Id icon, const char* label);

} // namespace LibUI::Icons

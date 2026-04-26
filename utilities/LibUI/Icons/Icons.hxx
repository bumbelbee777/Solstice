#pragma once

#include "../Core/Core.hxx"
#include <imgui.h>

namespace LibUI::Icons {

/// Stable ids for toolbar / chrome (Phosphor codepoints when ``Phosphor.ttf`` is loaded).
enum class Id : int {
    Folder = 0,
    File,
    Import,
    Export,
    Save,
    Open,
    Play,
    New,
    Plugins,
    Settings,
    About,
    Validate,
    Shortcuts,
    Reload,
    Duplicate,
    Remove,
    Prev,
    Next,
    Mesh,
    COUNT
};

/// Optional Font Awesome–style `.ttf` or Phosphor (private-use codepoints in ``Icons.cxx``).
LIBUI_API bool TryLoadIconFontPackFromFile(const char* pathTtf, float sizePixels);

/// Rebuild atlas + OpenGL font texture after adding fonts (call once after ``TryLoadIconFontPackFromFile``).
LIBUI_API void RefreshFontGpuTexture();

/// Future: PNG + JSON UV manifest; not implemented yet (returns false).
LIBUI_API bool TryLoadIconAtlasFromFiles(const char* pngPath, const char* manifestJsonPath);

LIBUI_API ImFont* GetIconFont();

LIBUI_API const char* Glyph(Id icon);

/// Toolbar button with icon + label (uses icon font for the whole label when loaded).
LIBUI_API bool ToolbarButton(Id icon, const char* label);

LIBUI_API bool SmallButtonWithIcon(Id icon, const char* label);

LIBUI_API bool MenuItemWithIcon(Id icon, const char* label, const char* shortcut = nullptr, bool selected = false,
    bool enabled = true);

} // namespace LibUI::Icons

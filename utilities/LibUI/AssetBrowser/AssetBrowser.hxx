#pragma once

#include "../Core/Core.hxx"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace LibUI::AssetBrowser {

struct Entry {
    std::string DisplayName;
    uint64_t Hash{0};
};

// Simple selectable list of imported assets (name + hash preview).
LIBUI_API void DrawPanel(const char* id, std::vector<Entry>& entries, int* selectedIndex);

} // namespace LibUI::AssetBrowser

#include "AssetBrowser.hxx"

#include <imgui.h>
#include <cstdio>

namespace LibUI::AssetBrowser {

void DrawPanel(const char* id, std::vector<Entry>& entries, int* selectedIndex) {
    if (ImGui::BeginChild(id, ImVec2(0, 200), true)) {
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            char label[256];
            snprintf(label, sizeof(label), "%s  (0x%016llX)", entries[i].DisplayName.c_str(),
                     static_cast<unsigned long long>(entries[i].Hash));
            if (ImGui::Selectable(label, selectedIndex && *selectedIndex == i)) {
                if (selectedIndex) {
                    *selectedIndex = i;
                }
            }
        }
    }
    ImGui::EndChild();
}

} // namespace LibUI::AssetBrowser

#include "LibUI/Tools/RecentPathsUi.hxx"

#include <imgui.h>

namespace LibUI::Tools {

void DrawRecentPathsCollapsible(const char* headerLabel, ImGuiTreeNodeFlags treeFlags, void* userData,
    void (*onOpenPath)(void* userData, const char* pathUtf8)) {
    if (LibUI::Core::RecentPathGetCount() <= 0) {
        return;
    }
    if (!ImGui::CollapsingHeader(headerLabel, treeFlags)) {
        return;
    }
    const int rc = LibUI::Core::RecentPathGetCount();
    for (int ri = 0; ri < rc; ++ri) {
        const char* rp = LibUI::Core::RecentPathGet(ri);
        if (!rp || !rp[0]) {
            continue;
        }
        ImGui::PushID(ri);
        if (ImGui::SmallButton("Open")) {
            if (onOpenPath) {
                onOpenPath(userData, rp);
            }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(rp);
        ImGui::PopID();
    }
}

} // namespace LibUI::Tools

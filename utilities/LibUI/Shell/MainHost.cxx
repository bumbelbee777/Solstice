#include "LibUI/Shell/MainHost.hxx"

namespace LibUI::Shell {

ImGuiWindowFlags MainHostFlags_MenuBarNoTitle() {
    return ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_MenuBar;
}

ImGuiWindowFlags MainHostFlags_NoDecoration() {
    return ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
}

ImGuiWindowFlags MainHostFlags_SharponEditor() {
    return ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
}

void BeginMainHostWindow(const char* imguiId, ImGuiWindowFlags flags) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin(imguiId, nullptr, flags);
}

void EndMainHostWindow() {
    ImGui::End();
}

} // namespace LibUI::Shell

#include "LibUI/Input/CommandRegistry.hxx"

#include <imgui.h>

namespace LibUI::Input {

void CommandRegistry::Register(Command command) {
    if (command.id.empty() || command.label.empty()) {
        return;
    }
    for (const Command& existing : m_commands) {
        if (existing.id == command.id) {
            return;
        }
    }
    m_commands.push_back(std::move(command));
}

bool CommandRegistry::Trigger(std::string_view id) const {
    for (const Command& command : m_commands) {
        if (command.id == id && command.execute) {
            command.execute();
            return true;
        }
    }
    return false;
}

const std::vector<Command>& CommandRegistry::Commands() const {
    return m_commands;
}

void DrawCommandPalette(const char* title, bool* visible, const CommandRegistry& registry) {
    if (visible && !(*visible)) {
        return;
    }
    if (!ImGui::Begin((title && title[0]) ? title : "Command Palette", visible)) {
        ImGui::End();
        return;
    }
    for (const Command& command : registry.Commands()) {
        const bool clicked = ImGui::Selectable(command.label.c_str());
        if (!command.shortcut.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", command.shortcut.c_str());
        }
        if (clicked && command.execute) {
            command.execute();
        }
    }
    if (registry.Commands().empty()) {
        ImGui::TextDisabled("No commands registered.");
    }
    ImGui::End();
}

} // namespace LibUI::Input


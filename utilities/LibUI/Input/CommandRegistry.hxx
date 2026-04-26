#pragma once

#include "LibUI/Core/Core.hxx"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace LibUI::Input {

struct Command {
    std::string id;
    std::string label;
    std::string shortcut;
    std::function<void()> execute;
};

class LIBUI_API CommandRegistry {
public:
    void Register(Command command);
    bool Trigger(std::string_view id) const;
    const std::vector<Command>& Commands() const;

private:
    std::vector<Command> m_commands;
};

LIBUI_API void DrawCommandPalette(const char* title, bool* visible, const CommandRegistry& registry);

} // namespace LibUI::Input


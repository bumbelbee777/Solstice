#pragma once

#include <Solstice.hxx>
#include <cstdint>
#include <string>

namespace Solstice::Plugin {

enum class PluginPermissions : std::uint32_t {
    None = 0,
    Read = 1u << 0,
    Write = 1u << 1,
    Execute = 1u << 2,
    All = Read | Write | Execute
};

inline PluginPermissions operator|(PluginPermissions a, PluginPermissions b) {
    return static_cast<PluginPermissions>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

enum class PluginState {
    Unloaded,
    Loaded,
    Initialized,
    Running,
    Stopping,
    Stopped,
    Crashed
};

/**
 * Optional host-facing interface for plugins implemented in C++ inside the same process.
 * Native DLL modules loaded via PluginManager use DynamicLibrary only unless they export a factory.
 */
class SOLSTICE_API IPlugin {
public:
    std::string Name;
    std::string Description;
    std::string Author;
    PluginPermissions Permissions{PluginPermissions::None};
    PluginState State{PluginState::Unloaded};

    IPlugin() = default;
    virtual ~IPlugin();

    virtual void Initialize() = 0;
    virtual void Reload() = 0;
    virtual void Shutdown() = 0;
};

} // namespace Solstice::Plugin

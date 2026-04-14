#pragma once

#include <string>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <functional>
#include "../Compiler/Compiler.hxx"
#include "../VM/BytecodeVM.hxx"

namespace Solstice::Scripting {

class HotReloader {
public:
    using ReloadCallback = std::function<void(const std::string& moduleName, const Program& prog)>;

    HotReloader(Compiler& compiler, BytecodeVM& vm);

    void AddWatch(const std::filesystem::path& path);
    void Update();

    void OnReload(ReloadCallback callback) { m_Callback = callback; }

private:
    Compiler& m_Compiler;
    BytecodeVM& m_VM;
    ReloadCallback m_Callback;

    struct WatchInfo {
        std::filesystem::file_time_type LastWrite;
        std::string ModuleName;
    };

    std::unordered_map<std::filesystem::path, WatchInfo> m_Watches;
};

}

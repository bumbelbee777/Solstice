#include "HotReloader.hxx"
#include <fstream>
#include <iostream>

namespace Solstice::Scripting {

HotReloader::HotReloader(Compiler& compiler, BytecodeVM& vm) 
    : m_Compiler(compiler), m_VM(vm) {}

void HotReloader::AddWatch(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        m_Watches[path] = {
            std::filesystem::last_write_time(path),
            path.stem().string()
        };
    }
}

void HotReloader::Update() {
    for (auto& [path, info] : m_Watches) {
        if (!std::filesystem::exists(path)) continue;

        auto currentWrite = std::filesystem::last_write_time(path);
        if (currentWrite > info.LastWrite) {
            info.LastWrite = currentWrite;
            std::cout << "[HotReloader] Reloading " << path << std::endl;

            try {
                std::ifstream file(path);
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                Program prog = m_Compiler.Compile(content);
                
                m_VM.ReloadProgram(prog);
                if (m_Callback) {
                    m_Callback(info.ModuleName, prog);
                }
            } catch (const std::exception& e) {
                std::cerr << "[HotReloader] Error reloading " << path << ": " << e.what() << std::endl;
            }
        }
    }
}

}

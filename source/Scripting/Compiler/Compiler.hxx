#pragma once
#include "../VM/BytecodeVM.hxx"
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include "../../Solstice.hxx"

namespace Solstice::Scripting {

class SOLSTICE_API Compiler {
public:
    Program Compile(const std::string& source);

    // Batch compile all Moonwalk files in a directory
    std::unordered_map<std::string, Program> BatchCompile(const std::filesystem::path& directory);

    // Optimization
    static Program OptimizeProgram(const Program& program);
};

}

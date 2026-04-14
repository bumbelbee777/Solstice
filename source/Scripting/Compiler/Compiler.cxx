#include "Compiler.hxx"
#include "Parser.hxx"
#include "../Analysis/MemoryAnalysis.hxx"
#include "../Analysis/StaticTypes.hxx"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace Solstice::Scripting {

Program Compiler::Compile(const std::string& source) {
    Program prog = ParseProgramSource(source);

    prog = Compiler::OptimizeProgram(prog);

    std::vector<MemoryIssue> issues = MemoryAnalyzer::AnalyzeProgram(prog);
    if (!issues.empty()) {
        const MemoryIssue& issue = issues.front();
        std::string kindStr =
            (issue.kind == MemoryIssue::Kind::UseAfterFree) ? "Use-after-free" : "Double-free";
        std::string msg = "Memory analysis error (" + kindStr + ") at instruction " +
                          std::to_string(issue.instructionIndex);
        if (!issue.functionName.empty()) {
            msg += " in " + issue.functionName;
        }
        throw std::runtime_error(msg);
    }

    std::vector<TypeIssue> typeIssues = StaticTypeChecker::CheckProgram(prog);
    if (!typeIssues.empty()) {
        const TypeIssue& t = typeIssues.front();
        throw std::runtime_error("Type check: " + t.message);
    }

    return prog;
}

std::unordered_map<std::string, Program> Compiler::BatchCompile(const std::filesystem::path& directory) {
    std::unordered_map<std::string, Program> programs;

    if (!std::filesystem::exists(directory)) return programs;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.path().extension() == ".mw") {
            std::string moduleName = entry.path().stem().string();
            std::filesystem::path cachePath = entry.path();
            cachePath.replace_extension(".mwc");

            bool useCache = false;
            if (std::filesystem::exists(cachePath)) {
                auto mwTime = std::filesystem::last_write_time(entry.path());
                auto mwcTime = std::filesystem::last_write_time(cachePath);
                if (mwcTime >= mwTime) {
                    useCache = true;
                }
            }

            if (useCache) {
                std::ifstream cfile(cachePath, std::ios::binary);
                if (!cfile.is_open()) {
                    useCache = false;
                } else {
                    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(cfile)),
                                                std::istreambuf_iterator<char>());
                    if (buffer.empty()) {
                        useCache = false;
                    } else {
                        try {
                            programs[moduleName] = Program::Deserialize(buffer);
                            if (programs[moduleName].Exports.empty()) {
                                std::cerr << "WARNING: Cached module " << moduleName
                                          << " has no exports, recompiling..." << std::endl;
                                useCache = false;
                                programs.erase(moduleName);
                            }
                        } catch (...) {
                            std::cerr << "WARNING: Failed to deserialize cache for " << moduleName
                                      << ", recompiling..." << std::endl;
                            useCache = false;
                            programs.erase(moduleName);
                        }
                    }
                }
            }

            if (!useCache) {
                std::ifstream file(entry.path());
                if (!file.is_open()) {
                    continue;
                }
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                try {
                    Program prog = Compile(content);
                    programs[moduleName] = prog;

                    std::vector<uint8_t> buffer;
                    prog.Serialize(buffer);
                    std::ofstream cfile(cachePath, std::ios::binary);
                    if (cfile.is_open()) {
                        cfile.write((const char*)buffer.data(), buffer.size());
                    }
                } catch (const std::exception& e) {
                    std::cerr << "ERROR: Failed to compile " << entry.path().string() << ": " << e.what()
                              << std::endl;
                    throw;
                }
            }
        }
    }
    return programs;
}

} // namespace Solstice::Scripting

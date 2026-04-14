#pragma once

#include <Solstice.hxx>
#include <string>
#include <vector>
#include "../VM/BytecodeVM.hxx"

namespace Solstice::Scripting {

    struct MemoryIssue {
        enum class Kind {
            UseAfterFree,
            DoubleFree
        };

        Kind kind;
        std::string functionName;
        size_t instructionIndex = 0;
    };

    struct SOLSTICE_API MemoryAnalyzer {
        // Analyze a compiled Program for Ptr<T> lifetime issues using a
        // per-function basic-block CFG and merged abstract states (Unknown /
        // Live / Freed) at joins.
        static std::vector<MemoryIssue> AnalyzeProgram(const Program& program);
    };

} // namespace Solstice::Scripting


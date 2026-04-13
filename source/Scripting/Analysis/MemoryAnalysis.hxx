#pragma once

#include "BytecodeVM.hxx"

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

    class MemoryAnalyzer {
    public:
        // Analyze a compiled and optimized Program for common pointer lifetime
        // mistakes involving Ptr<T> operations. The current implementation is
        // intentionally conservative and focuses on simple linear patterns
        // involving PTR_NEW / PTR_RESET / PTR_GET.
        static std::vector<MemoryIssue> AnalyzeProgram(const Program& program);
    };

} // namespace Solstice::Scripting


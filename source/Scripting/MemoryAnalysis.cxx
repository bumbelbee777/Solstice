#include "MemoryAnalysis.hxx"

namespace Solstice::Scripting {

    namespace {

        enum class PtrState {
            Unknown,
            Live,
            Freed
        };

    } // namespace

    std::vector<MemoryIssue> MemoryAnalyzer::AnalyzeProgram(const Program& program) {
        std::vector<MemoryIssue> issues;

        // Very simple, intraprocedural scan:
        // Track a single abstract pointer state as we walk instructions in
        // order. This is conservative but still catches the most common local
        // patterns like:
        //   PTR_RESET ... PTR_RESET   (double free)
        //   PTR_RESET ... PTR_GET     (use-after-free)
        PtrState state = PtrState::Unknown;

        for (size_t ip = 0; ip < program.Instructions.size(); ++ip) {
            const Instruction& inst = program.Instructions[ip];
            switch (inst.Op) {
                case OpCode::PTR_NEW:
                    state = PtrState::Live;
                    break;

                case OpCode::PTR_RESET: {
                    if (state == PtrState::Freed) {
                        MemoryIssue issue;
                        issue.kind = MemoryIssue::Kind::DoubleFree;
                        issue.functionName = ""; // Function name mapping not yet available
                        issue.instructionIndex = ip;
                        issues.push_back(issue);
                    }
                    // Once reset, the abstract pointer is considered freed.
                    state = PtrState::Freed;
                    break;
                }

                case OpCode::PTR_GET: {
                    if (state == PtrState::Freed || state == PtrState::Unknown) {
                        MemoryIssue issue;
                        issue.kind = MemoryIssue::Kind::UseAfterFree;
                        issue.functionName = "";
                        issue.instructionIndex = ip;
                        issues.push_back(issue);
                    }
                    break;
                }

                default:
                    // Other instructions do not change the abstract state in
                    // this initial implementation.
                    break;
            }
        }

        return issues;
    }

} // namespace Solstice::Scripting


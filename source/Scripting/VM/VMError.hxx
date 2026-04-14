#pragma once

#include "../../Solstice.hxx"
#include <string>
#include <vector>
#include <stdexcept>

namespace Solstice::Scripting {

class SOLSTICE_API VMError : public std::runtime_error {
public:
    struct StackFrame {
        size_t InstructionIndex;
        std::string FunctionName;
        std::string SourceFile;
        size_t LineNumber;
    };

    VMError(const std::string& message, size_t instructionIndex = 0);

    void AddStackFrame(const StackFrame& frame);
    const std::vector<StackFrame>& GetStackFrames() const { return m_StackFrames; }

    size_t GetInstructionIndex() const { return m_InstructionIndex; }
    void SetInstructionIndex(size_t index) { m_InstructionIndex = index; }

private:
    size_t m_InstructionIndex;
    std::vector<StackFrame> m_StackFrames;
};

} // namespace Solstice::Scripting


#include "VMError.hxx"
#include <sstream>

namespace Solstice::Scripting {

VMError::VMError(const std::string& message, size_t instructionIndex)
    : std::runtime_error(message)
    , m_InstructionIndex(instructionIndex)
{
}

void VMError::AddStackFrame(const StackFrame& frame) {
    m_StackFrames.push_back(frame);
}

} // namespace Solstice::Scripting


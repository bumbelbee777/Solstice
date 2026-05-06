#pragma once

#include <string>
#include "../VM/BytecodeVM.hxx"

namespace Solstice::Scripting::Diagnostics {

std::string FormatInstructionDiagnostic(
    const Program& program,
    size_t instructionIndex,
    const std::string& headline,
    const std::string& detail);

}


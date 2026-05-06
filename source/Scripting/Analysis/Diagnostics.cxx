#include "Diagnostics.hxx"
#include <sstream>

namespace Solstice::Scripting::Diagnostics {

std::string FormatInstructionDiagnostic(
    const Program& program,
    size_t instructionIndex,
    const std::string& headline,
    const std::string& detail) {
    std::ostringstream oss;
    oss << headline << " at instruction " << instructionIndex;

    auto it = program.InstructionSourceMap.find(instructionIndex);
    if (it == program.InstructionSourceMap.end()) {
        if (!detail.empty()) {
            oss << "\nDetails: " << detail;
        }
        return oss.str();
    }

    const Program::InstructionSourceInfo& src = it->second;
    oss << "\nLocation: ";
    if (!src.SourceFile.empty()) {
        oss << src.SourceFile;
    } else {
        oss << "<script>";
    }
    if (src.Line != 0) {
        oss << ":" << src.Line;
        if (src.Column != 0) {
            oss << ":" << src.Column;
        }
    }
    if (!detail.empty()) {
        oss << "\nDetails: " << detail;
    }
    if (!src.SourceLine.empty()) {
        oss << "\nCode: " << src.SourceLine;
        if (src.Column > 0) {
            oss << "\n      ";
            for (size_t i = 1; i < src.Column; ++i) {
                oss << ' ';
            }
            oss << '^';
        }
    }
    return oss.str();
}

}


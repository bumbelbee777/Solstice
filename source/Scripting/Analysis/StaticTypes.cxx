#include "StaticTypes.hxx"

namespace Solstice::Scripting {

std::vector<TypeIssue> StaticTypeChecker::CheckProgram(const Program& program) {
    if (program.PtrOperandRegs.empty()) {
        return {};
    }

    std::vector<TypeIssue> out;
    for (const auto& [ip, reg] : program.PtrOperandRegs) {
        auto tr = program.RegisterTypes.find(reg);
        if (tr == program.RegisterTypes.end()) {
            out.push_back({"Ptr.* call at instruction " + std::to_string(ip) + " uses register " +
                           std::to_string((int)reg) + " without a type hint (expected Ptr<...>)"});
        } else if (tr->second.rfind("Ptr", 0) != 0) {
            out.push_back({"Ptr.* call at instruction " + std::to_string(ip) + " expected Ptr type, got " + tr->second});
        }
    }
    return out;
}

} // namespace Solstice::Scripting

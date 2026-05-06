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
            TypeIssue issue;
            issue.message = "Ptr.* call uses register " + std::to_string((int)reg) +
                            " without a type hint (expected Ptr<...>)";
            issue.instructionIndex = ip;
            out.push_back(std::move(issue));
        } else if (tr->second.rfind("Ptr", 0) != 0) {
            TypeIssue issue;
            issue.message = "Ptr.* call expected Ptr type, got " + tr->second;
            issue.instructionIndex = ip;
            out.push_back(std::move(issue));
        } else if (tr->second == "Ptr") {
            TypeIssue issue;
            issue.message = "Ptr.* call uses bare Ptr type; prefer Ptr<T> so pointee intent is explicit";
            issue.instructionIndex = ip;
            out.push_back(std::move(issue));
        }
    }
    return out;
}

} // namespace Solstice::Scripting

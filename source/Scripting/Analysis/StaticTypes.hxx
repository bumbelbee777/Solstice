#pragma once

#include "../VM/BytecodeVM.hxx"
#include <string>
#include <vector>

namespace Solstice::Scripting {

struct TypeIssue {
    std::string message;
};

class StaticTypeChecker {
public:
    static std::vector<TypeIssue> CheckProgram(const Program& program);
};

} // namespace Solstice::Scripting

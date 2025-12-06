#pragma once
#include "BytecodeVM.hxx"
#include <string>

namespace Solstice::Scripting {

class Compiler {
public:
    Program Compile(const std::string& source);
};

}

#pragma once
#include "BytecodeVM.hxx"
#include <string>
#include "../Solstice.hxx"

namespace Solstice::Scripting {

class SOLSTICE_API Compiler {
public:
    Program Compile(const std::string& source);
};

}

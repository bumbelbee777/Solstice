#pragma once

#include "../VM/BytecodeVM.hxx"
#include <string>

namespace Solstice::Scripting {

Program ParseProgramSource(const std::string& source);
Program ParseProgramSourceWithName(const std::string& source, const std::string& sourceName);

}

#pragma once

#include "../../Solstice.hxx"

namespace Solstice::Scripting {
class BytecodeVM;
}

namespace Solstice::Game {

SOLSTICE_API void RegisterNarrativeScriptBindings(Scripting::BytecodeVM& VM);

} // namespace Solstice::Game

#pragma once

#include "../Solstice.hxx"

namespace Solstice::Render { class Scene; }

namespace Solstice::Scripting {
    class BytecodeVM;
    
    // Registers native functions for UI, Render, etc.
    SOLSTICE_API void RegisterScriptBindings(BytecodeVM& vm, Solstice::Render::Scene* scene = nullptr);
}

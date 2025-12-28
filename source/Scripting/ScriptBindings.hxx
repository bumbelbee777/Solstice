#pragma once

#include "../Solstice.hxx"

#include "BytecodeVM.hxx"

namespace Solstice::Render { class Scene; class Camera; }
namespace Solstice::ECS { class Registry; }
namespace Solstice::Physics { class PhysicsSystem; }

namespace Solstice::Scripting {
    class BytecodeVM;

    // Helper to extract values from Value
    SOLSTICE_API float GetFloat(const Value& v);
    SOLSTICE_API int64_t GetInt(const Value& v);
    SOLSTICE_API std::string GetString(const Value& v);

    // Registers native functions for UI, Render, ECS, Physics, etc.
    SOLSTICE_API void RegisterScriptBindings(
        BytecodeVM& vm,
        ECS::Registry* registry,
        Render::Scene* scene = nullptr,
        Physics::PhysicsSystem* physicsSystem = nullptr,
        Render::Camera* camera = nullptr
    );
}

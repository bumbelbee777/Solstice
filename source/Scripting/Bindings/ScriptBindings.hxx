#pragma once

#include "../../Solstice.hxx"

#include "../VM/BytecodeVM.hxx"
#include "NativeBinding.hxx"

namespace Solstice::Render { class Scene; class Camera; }
namespace Solstice::ECS { class Registry; }
namespace Solstice::Physics { class PhysicsSystem; }

namespace Solstice::Scripting {
    class BytecodeVM;

    // Helper to extract values from Value
    SOLSTICE_API float GetFloat(const Value& v);
    SOLSTICE_API int64_t GetInt(const Value& v);
    SOLSTICE_API std::string GetString(const Value& v);

    // Typed native registration helpers (AngelScript-style ergonomics on top of Value VM).
    namespace Typed {
        using NativeBinding::Bind;
        using NativeBinding::BindMethod;
        using NativeBinding::Register;
    }

    // Registers native functions for UI, Render, ECS, Physics, etc.
    SOLSTICE_API void RegisterScriptBindings(
        BytecodeVM& vm,
        ECS::Registry* registry,
        Render::Scene* scene = nullptr,
        Physics::PhysicsSystem* physicsSystem = nullptr,
        Render::Camera* camera = nullptr
    );
}

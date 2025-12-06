#pragma once

#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>

namespace Solstice::ECS {
struct Transform {
    Math::Vec3 Position{};
    Math::Vec3 Scale{1.0f, 1.0f, 1.0f};
    Math::Matrix4 Matrix{};
};
}

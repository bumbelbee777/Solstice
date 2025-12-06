#pragma once

#include <Math/Vector.hxx>

namespace Solstice::Physics {
struct Fluid {
    float Density{1000.0f};
    float Viscosity{0.001f};
    Math::Vec3 Velocity{};
    Math::Vec3 Volume{1.0f, 1.0f, 1.0f}; // Dimensions of the fluid volume
};
}
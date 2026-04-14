#pragma once

#include "Vector.hxx"
#include "../Core/ML/SIMD.hxx"

namespace Solstice::Math {

// Inline helper functions for SIMD-accelerated Vec3 operations

/// Convert Vec3 to SIMD Vec4 (w=0)
inline Core::SIMD::Vec4 ToSIMD(const Vec3& v) {
    return Core::SIMD::Vec4(v.x, v.y, v.z, 0.0f);
}

/// Convert SIMD Vec4 to Vec3 (ignore w)
inline Vec3 FromSIMD(const Core::SIMD::Vec4& v) {
    return Vec3(v.X(), v.Y(), v.Z());
}

/// SIMD-accelerated dot product for Vec3
inline float DotSIMD(const Vec3& a, const Vec3& b) {
    return ToSIMD(a).Dot(ToSIMD(b));
}

/// SIMD-accelerated cross product for Vec3
inline Vec3 CrossSIMD(const Vec3& a, const Vec3& b) {
    return FromSIMD(ToSIMD(a).Cross(ToSIMD(b)));
}

/// SIMD-accelerated magnitude for Vec3
inline float MagnitudeSIMD(const Vec3& v) {
    return ToSIMD(v).Magnitude3();
}

/// SIMD-accelerated normalization for Vec3
inline Vec3 NormalizeSIMD(const Vec3& v) {
    return FromSIMD(ToSIMD(v).Normalized3());
}

/// SIMD-accelerated vector addition
inline Vec3 AddSIMD(const Vec3& a, const Vec3& b) {
    return FromSIMD(ToSIMD(a) + ToSIMD(b));
}

/// SIMD-accelerated vector subtraction
inline Vec3 SubSIMD(const Vec3& a, const Vec3& b) {
    return FromSIMD(ToSIMD(a) - ToSIMD(b));
}

/// SIMD-accelerated scalar multiplication
inline Vec3 MulSIMD(const Vec3& v, float s) {
    return FromSIMD(ToSIMD(v) * s);
}

} // namespace Solstice::Math

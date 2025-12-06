#pragma once

#include "../SolsticeExport.hxx"
#include <cmath>
#include <iostream>

// Forward declaration to avoid circular dependency
namespace Solstice::Math {
    struct Matrix4;
}

namespace Solstice::Math {
struct SOLSTICE_API Quaternion {
    float w, x, y, z;

    Quaternion() : w(1), x(0), y(0), z(0) {}
    Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}
    Quaternion(const Quaternion& Other) : w(Other.w), x(Other.x), y(Other.y), z(Other.z) {}

    Quaternion operator+(const Quaternion& Other) const {
        return Quaternion(w + Other.w, x + Other.x, y + Other.y, z + Other.z);
    }

    Quaternion operator-() const {
        return Quaternion(-w, -x, -y, -z);
    }

    Quaternion operator-(const Quaternion& Other) const {
        return Quaternion(w - Other.w, x - Other.x, y - Other.y, z - Other.z);
    }

    Quaternion operator*(float s) const {
        return Quaternion(w * s, x * s, y * s, z * s);
    }

    Quaternion operator/(float s) const {
        return Quaternion(w / s, x / s, y / s, z / s);
    }

    Quaternion operator*(const Quaternion& r) const {
        return Quaternion(
            w * r.w - x * r.x - y * r.y - z * r.z,
            w * r.x + x * r.w + y * r.z - z * r.y,
            w * r.y - x * r.z + y * r.w + z * r.x,
            w * r.z + x * r.y - y * r.x + z * r.w
        );
    }

    float Dot(const Quaternion& Other) const {
        return w * Other.w + x * Other.x + y * Other.y + z * Other.z;
    }

    float Magnitude() const {
        return std::sqrt(w * w + x * x + y * y + z * z);
    }

    Quaternion Normalized() const {
        float Mag = Magnitude();
        return (Mag == 0) ? Quaternion() : *this / Mag;
    }

    Quaternion Conjugate() const {
        return Quaternion(w, -x, -y, -z);
    }

    static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t) {
        return (a * (1.0f - t) + b * t).Normalized();
    }

    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t) {
        float CosTheta = a.Dot(b);
        Quaternion End = b;

        // Use the shortest path
        if (CosTheta < 0.0f) {
            End = b * -1.0f;
            CosTheta = -CosTheta;
        }

        static constexpr float Thresh = 0.9995f;
        if (CosTheta > Thresh) {
            // If very close, fallback to linear interpolation
            return Lerp(a, End, t);
        }

        float Theta = std::acos(CosTheta);
        float SinTheta = std::sqrt(1.0f - CosTheta * CosTheta);

        float aFactor = std::sin((1.0f - t) * Theta) / SinTheta;
        float bFactor = std::sin(t * Theta) / SinTheta;

        return a * aFactor + End * bFactor;
    }

    Matrix4 ToMatrix() const;

    void Print() const {
        std::cout << "Quaternion(" << w << ", " << x << ", " << y << ", " << z << ")\n";
    }
};
}
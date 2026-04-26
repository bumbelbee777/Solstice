#pragma once

#include "../Solstice.hxx"
#include <cmath>
#include <iostream>

// Forward declaration to avoid circular dependency
namespace Solstice::Math {
    struct Matrix4;
}

namespace Solstice::Math {
/* Inline ctors must not be class-dllexported: static libs (e.g. LibParallax) + SolsticeEngine.dll
 * otherwise both provide identical MSVC-exported symbols and utilities link both (LNK2005). */
struct Quaternion {
    float w, x, y, z;

    Quaternion() : w(1), x(0), y(0), z(0) {}
    Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}
    Quaternion(const Quaternion& Other)  = default;

    Quaternion operator+(const Quaternion& Other) const {
        return {w + Other.w, x + Other.x, y + Other.y, z + Other.z};
    }

    Quaternion operator-() const {
        return {-w, -x, -y, -z};
    }

    Quaternion operator-(const Quaternion& Other) const {
        return {w - Other.w, x - Other.x, y - Other.y, z - Other.z};
    }

    Quaternion operator*(float S) const {
        return {w * S, x * S, y * S, z * S};
    }

    Quaternion operator/(float S) const {
        return {w / S, x / S, y / S, z / S};
    }

    Quaternion operator*(const Quaternion& R) const {
        return {
            (w * R.w) - (x * R.x) - (y * R.y) - (z * R.z),
            (w * R.x) + (x * R.w) + (y * R.z) - (z * R.y),
            (w * R.y) - (x * R.z) + (y * R.w) + (z * R.x),
            (w * R.z) + (x * R.y) - (y * R.x) + (z * R.w)
        };
    }

    bool operator==(const Quaternion& Rhs) const {
        return w == Rhs.w && x == Rhs.x && y == Rhs.y && z == Rhs.z;
    }

    bool operator!=(const Quaternion& Rhs) const {
        return !(*this == Rhs);
    }

    [[nodiscard]] float Dot(const Quaternion& Other) const {
        return (w * Other.w) + (x * Other.x) + (y * Other.y) + (z * Other.z);
    }

    [[nodiscard]] float Magnitude() const {
        return std::sqrt((w * w) + (x * x) + (y * y) + (z * z));
    }

    [[nodiscard]] Quaternion Normalized() const {
        float mag = Magnitude();
        return (mag == 0) ? Quaternion() : *this / mag;
    }

    [[nodiscard]] Quaternion Conjugate() const {
        return {w, -x, -y, -z};
    }

    static Quaternion Lerp(const Quaternion& A, const Quaternion& B, float T) {
        return (A * (1.0f - T) + B * T).Normalized();
    }

    static Quaternion Slerp(const Quaternion& A, const Quaternion& B, float T) {
        float CosTheta = A.Dot(B);
        Quaternion End = B;

        // Use the shortest path
        if (CosTheta < 0.0f) {
            End = B * -1.0f;
            CosTheta = -CosTheta;
        }

        static constexpr float THRESH = 0.9995f;
        if (CosTheta > THRESH) {
            // If very close, fallback to linear interpolation
            return Lerp(A, End, T);
        }

        float Theta = std::acos(CosTheta);
        float SinTheta = std::sqrt(1.0f - (CosTheta * CosTheta));

        float AFactor = std::sin((1.0f - T) * Theta) / SinTheta;
        float BFactor = std::sin(T * Theta) / SinTheta;

        return A * AFactor + End * BFactor;
    }

    static Quaternion FromEuler(float Pitch, float Yaw, float Roll) {
        float CP = std::cos(Pitch * 0.5f);
        float SP = std::sin(Pitch * 0.5f);
        float CY = std::cos(Yaw * 0.5f);
        float SY = std::sin(Yaw * 0.5f);
        float CR = std::cos(Roll * 0.5f);
        float SR = std::sin(Roll * 0.5f);

        Quaternion Q;
        Q.w = CP * CY * CR + SP * SY * SR;
        Q.x = SP * CY * CR - CP * SY * SR;
        Q.y = CP * SY * CR + SP * CY * SR;
        Q.z = CP * CY * SR - SP * SY * CR;
        return Q;
    }

    [[nodiscard]] SOLSTICE_API Matrix4 ToMatrix() const;

    void Print() const {
        std::cout << "Quaternion(" << w << ", " << x << ", " << y << ", " << z << ")\n";
    }
};
}

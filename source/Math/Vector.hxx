#pragma once

#include <iostream>
#include <cmath>

namespace Solstice::Math {
struct Vec2 {
	float x, y;

	Vec2() : x(0), y(0) {}
	Vec2(float x, float y) : x(x), y(y) {}
	Vec2(const Vec2& Other)  = default;

	[[nodiscard]] float Magnitude() const {
		return std::sqrt((x * x) + (y * y));
	}

	[[nodiscard]] float Dot(const Vec2& Other) const {
		return (x * Other.x) + (y * Other.y);
	}

	Vec2 Normalized() const {
		float Mag = Magnitude();
		return (Mag == 0) ? Vec2(0, 0) : *this / Mag;
	}

	Vec2 operator+(const Vec2& Other) const { return {x + Other.x, y + Other.y}; }
	Vec2 operator-() const { return {-x, -y}; }
	Vec2 operator-(const Vec2& Other) const { return {x - Other.x, y - Other.y}; }
	Vec2 operator*(float Scalar) const { return {x * Scalar, y * Scalar}; }
	Vec2 operator/(float Scalar) const { return {x / Scalar, y / Scalar}; }

    Vec2& operator+=(const Vec2& Other) { x += Other.x; y += Other.y; return *this; }
    Vec2& operator-=(const Vec2& Other) { x -= Other.x; y -= Other.y; return *this; }
    Vec2& operator*=(float Scalar) { x *= Scalar; y *= Scalar; return *this; }
    Vec2& operator/=(float Scalar) { x /= Scalar; y /= Scalar; return *this; }

    bool operator==(const Vec2& Rhs) const { return x == Rhs.x && y == Rhs.y; }
    bool operator!=(const Vec2& Rhs) const { return !(*this == Rhs); }

    static Vec2 Lerp(const Vec2& A, const Vec2& B, float T) {
        return A + (B - A) * T;
    }

    [[nodiscard]] float DistanceSquared(const Vec2& Other) const {
        float Dx = x - Other.x;
        float Dy = y - Other.y;
        return (Dx * Dx) + (Dy * Dy);
    }

    [[nodiscard]] float Distance(const Vec2& Other) const {
        return std::sqrt(DistanceSquared(Other));
    }

    [[nodiscard]] Vec2 Project(const Vec2& Other) const {
        float DotP = Dot(Other);
        float Len2 = Other.Dot(Other);
        return (Len2 == 0.0f) ? Vec2() : Other * (DotP / Len2);
    }

    [[nodiscard]] Vec2 Reflect(const Vec2& Normal) const {
        return *this - Normal * 2.0f * Dot(Normal);
    }
};

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(const Vec3& Other)  = default;

    [[nodiscard]] float Magnitude() const {
        return std::sqrt((x * x) + (y * y) + (z * z));
    }

    [[nodiscard]] float Length() const { return Magnitude(); }

    float operator[](int I) const {
        if (I == 0) { return x;
}
        if (I == 1) { return y;
}
        return z;
    }

    float& operator[](int I) {
        if (I == 0) { return x;
}
        if (I == 1) { return y;
}
        return z;
    }

    [[nodiscard]] float Dot(const Vec3& Other) const {
        return (x * Other.x) + (y * Other.y) + (z * Other.z);
    }

    [[nodiscard]] Vec3 Cross(const Vec3& Other) const {
        return {
            (y * Other.z) - (z * Other.y),
            (z * Other.x) - (x * Other.z),
            (x * Other.y) - (y * Other.x)
        };
    }

    [[nodiscard]] Vec3 Normalized() const {
        float Mag = Magnitude();
        return (Mag == 0) ? Vec3() : *this / Mag;
    }

    Vec3 operator+(const Vec3& Other) const { return {x + Other.x, y + Other.y, z + Other.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator-(const Vec3& Other) const { return {x - Other.x, y - Other.y, z - Other.z}; }
    Vec3 operator*(float Scalar) const { return {Scalar * x, Scalar * y, Scalar * z}; }
    Vec3 operator/(float Scalar) const { return {x / Scalar, y / Scalar, z / Scalar}; }

    Vec3& operator+=(const Vec3& Other) { x += Other.x; y += Other.y; z += Other.z; return *this; }
    Vec3& operator-=(const Vec3& Other) { x -= Other.x; y -= Other.y; z -= Other.z; return *this; }
    Vec3& operator*=(float Scalar) { x *= Scalar; y *= Scalar; z *= Scalar; return *this; }
    Vec3& operator/=(float Scalar) { x /= Scalar; y /= Scalar; z /= Scalar; return *this; }

    bool operator==(const Vec3& Rhs) const { return x == Rhs.x && y == Rhs.y && z == Rhs.z; }
    bool operator!=(const Vec3& Rhs) const { return !(*this == Rhs); }

    static Vec3 Lerp(const Vec3& A, const Vec3& B, float T) { return A + (B - A) * T; }

    static Vec3 Slerp(const Vec3& A, const Vec3& B, float T) {
        float DotP = A.Normalized().Dot(B.Normalized());
        DotP = std::fmax(std::fmin(DotP, 1.0f), -1.0f);
        float Theta = std::acos(DotP) * T;
        Vec3 Relative = (B - A * DotP).Normalized();
        return A * std::cos(Theta) + Relative * std::sin(Theta);
    }

    [[nodiscard]] float DistanceSquared(const Vec3& Other) const {
        float Dx = x - Other.x;
        float Dy = y - Other.y;
        float Dz = z - Other.z;
        return (Dx * Dx) + (Dy * Dy) + (Dz * Dz);
    }

    [[nodiscard]] float Distance(const Vec3& Other) const {
        return std::sqrt(DistanceSquared(Other));
    }

    [[nodiscard]] Vec3 Project(const Vec3& Other) const {
        float DotP = Dot(Other);
        float Len2 = Other.Dot(Other);
        return (Len2 == 0.0f) ? Vec3() : Other * (DotP / Len2);
    }

    [[nodiscard]] Vec3 Reflect(const Vec3& Normal) const {
        return *this - Normal * 2.0f * Dot(Normal);
    }

    [[nodiscard]] Vec3 Extrude(const Vec3& Normal, float Distance) const {
        return *this + Normal.Normalized() * Distance;
    }
};

struct Vec4 {
    float x, y, z, w;

    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec4& Other)  = default;

    [[nodiscard]] float Magnitude() const {
        return std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
    }

    [[nodiscard]] Vec4 Normalized() const {
        float Mag = Magnitude();
        return (Mag == 0.0f) ? Vec4() : *this / Mag;
    }

    [[nodiscard]] float Dot(const Vec4& Other) const {
        return (x * Other.x) + (y * Other.y) + (z * Other.z) + (w * Other.w);
    }

    Vec4 operator+(const Vec4& Rhs) const { return {x + Rhs.x, y + Rhs.y, z + Rhs.z, w + Rhs.w}; }
    Vec4 operator-(const Vec4& Rhs) const { return {x - Rhs.x, y - Rhs.y, z - Rhs.z, w - Rhs.w}; }
    Vec4 operator-() const { return {-x, -y, -z, -w}; }
    Vec4 operator*(float Scalar) const { return {x * Scalar, y * Scalar, z * Scalar, w * Scalar}; }
    Vec4 operator/(float Scalar) const { return {x / Scalar, y / Scalar, z / Scalar, w / Scalar}; }

    Vec4& operator+=(const Vec4& Rhs) { x += Rhs.x; y += Rhs.y; z += Rhs.z; w += Rhs.w; return *this; }
    Vec4& operator-=(const Vec4& Rhs) { x -= Rhs.x; y -= Rhs.y; z -= Rhs.z; w -= Rhs.w; return *this; }
    Vec4& operator*=(float Scalar) { x *= Scalar; y *= Scalar; z *= Scalar; w *= Scalar; return *this; }
    Vec4& operator/=(float Scalar) { x /= Scalar; y /= Scalar; z /= Scalar; w /= Scalar; return *this; }

    bool operator==(const Vec4& Rhs) const { return x == Rhs.x && y == Rhs.y && z == Rhs.z && w == Rhs.w; }
    bool operator!=(const Vec4& Rhs) const { return !(*this == Rhs); }

    static Vec4 Lerp(const Vec4& A, const Vec4& B, float T) { return A + (B - A) * T; }

    [[nodiscard]] float DistanceSquared(const Vec4& Other) const {
        float Dx = x - Other.x;
        float Dy = y - Other.y;
        float Dz = z - Other.z;
        float Dw = w - Other.w;
        return (Dx*Dx) + (Dy*Dy) + (Dz*Dz) + (Dw*Dw);
    }

    [[nodiscard]] float Distance(const Vec4& Other) const {
        return std::sqrt(DistanceSquared(Other));
    }
};

inline std::ostream& operator<<(std::ostream& Os, const Vec2& V) { return Os << "(" << V.x << ", " << V.y << ")"; }
inline std::ostream& operator<<(std::ostream& Os, const Vec3& V) { return Os << "(" << V.x << ", " << V.y << ", " << V.z << ")"; }
inline std::ostream& operator<<(std::ostream& Os, const Vec4& V) { return Os << "(" << V.x << ", " << V.y << ", " << V.z << ", " << V.w << ")"; }

inline Vec2 operator*(float Scalar, const Vec2& V) { return V * Scalar; }
inline Vec3 operator*(float Scalar, const Vec3& V) { return V * Scalar; }
inline Vec4 operator*(float Scalar, const Vec4& V) { return V * Scalar; }

} // namespace Solstice::Math

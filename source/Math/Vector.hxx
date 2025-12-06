#pragma once

#include <iostream>
#include <cmath>

namespace Solstice::Math {
struct Vec2 {
	float x, y;

	Vec2() : x(0), y(0) {}
	Vec2(float x, float y) : x(x), y(y) {}
	Vec2(const Vec2& Other) : x(Other.x), y(Other.y) {}

	float Magnitude() {
		return std::sqrt(x * x + y * y);
	}

	float Dot(const Vec2& Other) const {
		return x * Other.x + y * Other.y;
	}

	Vec2 Normalized() {
		float Mag = Magnitude();
		return (Mag == 0) ? Vec2(0, 0) : *this / Mag;
	}

	Vec2 operator+(const Vec2& Other) const { return Vec2(x + Other.x, y + Other.y); }
	Vec2 operator-() const { return Vec2(-x, -y); }
	Vec2 operator-(const Vec2& Other) const { return Vec2(x - Other.x, y - Other.y); }
	Vec2 operator*(float Scalar) const { return Vec2(x * Scalar, y * Scalar); }
	Vec2 operator/(float Scalar) const { return Vec2(x / Scalar, y / Scalar); }

    Vec2& operator+=(const Vec2& Other) { x += Other.x; y += Other.y; return *this; }
    Vec2& operator-=(const Vec2& Other) { x -= Other.x; y -= Other.y; return *this; }
    Vec2& operator*=(float Scalar) { x *= Scalar; y *= Scalar; return *this; }
    Vec2& operator/=(float Scalar) { x /= Scalar; y /= Scalar; return *this; }

    bool operator==(const Vec2& rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(const Vec2& rhs) const { return !(*this == rhs); }

    static Vec2 Lerp(const Vec2& a, const Vec2& b, float t) {
        return a + (b - a) * t;
    }

    float DistanceSquared(const Vec2& Other) const {
        float dx = x - Other.x, dy = y - Other.y;
        return dx * dx + dy * dy;
    }

    float Distance(const Vec2& Other) const {
        return std::sqrt(DistanceSquared(Other));
    }

    Vec2 Project(const Vec2& Other) const {
        float DotP = Dot(Other);
        float Len2 = Other.Dot(Other);
        return (Len2 == 0.0f) ? Vec2() : Other * (DotP / Len2);
    }

    Vec2 Reflect(const Vec2& Normal) const {
        return *this - Normal * 2.0f * Dot(Normal);
    }
};

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(const Vec3& Other) : x(Other.x), y(Other.y), z(Other.z) {}

    float Magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    float Dot(const Vec3& Other) const {
        return x * Other.x + y * Other.y + z * Other.z;
    }

    Vec3 Cross(const Vec3& Other) const {
        return Vec3(
            y * Other.z - z * Other.y,
            z * Other.x - x * Other.z,
            x * Other.y - y * Other.x
        );
    }

    Vec3 Normalized() const {
        float Mag = Magnitude();
        return (Mag == 0) ? Vec3() : *this / Mag;
    }

    Vec3 operator+(const Vec3& Other) const { return Vec3(x + Other.x, y + Other.y, z + Other.z); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3 operator-(const Vec3& Other) const { return Vec3(x - Other.x, y - Other.y, z - Other.z); }
    Vec3 operator*(float Scalar) const { return Vec3(Scalar * x, Scalar * y, Scalar * z); }
    Vec3 operator/(float Scalar) const { return Vec3(x / Scalar, y / Scalar, z / Scalar); }

    Vec3& operator+=(const Vec3& Other) { x += Other.x; y += Other.y; z += Other.z; return *this; }
    Vec3& operator-=(const Vec3& Other) { x -= Other.x; y -= Other.y; z -= Other.z; return *this; }
    Vec3& operator*=(float Scalar) { x *= Scalar; y *= Scalar; z *= Scalar; return *this; }
    Vec3& operator/=(float Scalar) { x /= Scalar; y /= Scalar; z /= Scalar; return *this; }

    bool operator==(const Vec3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
    bool operator!=(const Vec3& rhs) const { return !(*this == rhs); }

    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

    static Vec3 Slerp(const Vec3& a, const Vec3& b, float t) {
        float DotP = a.Normalized().Dot(b.Normalized());
        DotP = std::fmax(std::fmin(DotP, 1.0f), -1.0f);
        float Theta = std::acos(DotP) * t;
        Vec3 Relative = (b - a * DotP).Normalized();
        return a * std::cos(Theta) + Relative * std::sin(Theta);
    }

    float DistanceSquared(const Vec3& Other) const {
        float dx = x - Other.x, dy = y - Other.y, dz = z - Other.z;
        return dx * dx + dy * dy + dz * dz;
    }

    float Distance(const Vec3& Other) const {
        return std::sqrt(DistanceSquared(Other));
    }

    Vec3 Project(const Vec3& Other) const {
        float DotP = Dot(Other);
        float Len2 = Other.Dot(Other);
        return (Len2 == 0.0f) ? Vec3() : Other * (DotP / Len2);
    }

    Vec3 Reflect(const Vec3& Normal) const {
        return *this - Normal * 2.0f * Dot(Normal);
    }

    Vec3 Extrude(const Vec3& Normal, float Distance) const {
        return *this + Normal.Normalized() * Distance;
    }
};

struct Vec4 {
    float x, y, z, w;

    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec4& Other) : x(Other.x), y(Other.y), z(Other.z), w(Other.w) {}

    float Magnitude() const {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }

    Vec4 Normalized() const {
        float Mag = Magnitude();
        return (Mag == 0.0f) ? Vec4() : *this / Mag;
    }

    float Dot(const Vec4& Other) const {
        return x * Other.x + y * Other.y + z * Other.z + w * Other.w;
    }

    Vec4 operator+(const Vec4& rhs) const { return Vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w); }
    Vec4 operator-(const Vec4& rhs) const { return Vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w); }
    Vec4 operator-() const { return Vec4(-x, -y, -z, -w); }
    Vec4 operator*(float Scalar) const { return Vec4(x * Scalar, y * Scalar, z * Scalar, w * Scalar); }
    Vec4 operator/(float Scalar) const { return Vec4(x / Scalar, y / Scalar, z / Scalar, w / Scalar); }

    Vec4& operator+=(const Vec4& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
    Vec4& operator-=(const Vec4& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; return *this; }
    Vec4& operator*=(float Scalar) { x *= Scalar; y *= Scalar; z *= Scalar; w *= Scalar; return *this; }
    Vec4& operator/=(float Scalar) { x /= Scalar; y /= Scalar; z /= Scalar; w /= Scalar; return *this; }

    bool operator==(const Vec4& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }
    bool operator!=(const Vec4& rhs) const { return !(*this == rhs); }

    static Vec4 Lerp(const Vec4& a, const Vec4& b, float t) { return a + (b - a) * t; }

    float DistanceSquared(const Vec4& Other) const {
        float dx = x - Other.x;
        float dy = y - Other.y;
        float dz = z - Other.z;
        float dw = w - Other.w;
        return dx*dx + dy*dy + dz*dz + dw*dw;
    }

    float Distance(const Vec4& Other) const {
        return std::sqrt(DistanceSquared(Other));
    }
};

inline std::ostream& operator<<(std::ostream& os, const Vec2& V) { return os << "(" << V.x << ", " << V.y << ")"; }
inline std::ostream& operator<<(std::ostream& os, const Vec3& V) { return os << "(" << V.x << ", " << V.y << ", " << V.z << ")"; }
inline std::ostream& operator<<(std::ostream& os, const Vec4& V) { return os << "(" << V.x << ", " << V.y << ", " << V.z << ", " << V.w << ")"; }

inline Vec2 operator*(float Scalar, const Vec2& V) { return V * Scalar; }
inline Vec3 operator*(float Scalar, const Vec3& V) { return V * Scalar; }
inline Vec4 operator*(float Scalar, const Vec4& V) { return V * Scalar; }

} // namespace Solstice::Math
#pragma once

#include <Math/Vector.hxx>
#include <iostream>
#include <cmath>
#include <cassert>

namespace Solstice::Math {
struct Matrix2 {
    float M[2][2];

    Matrix2() {
        M[0][0] = 1.0f; M[0][1] = 0.0f;
        M[1][0] = 0.0f; M[1][1] = 1.0f;
    }

    static Matrix2 Identity() { return {}; }

    void Print() const {
        for (const auto *I : M) {
            for (int J = 0; J < 2; ++J) {
                std::cout << I[J] << " ";
}
            std::cout << std::endl;
        }
    }

    Matrix2 operator+(const Matrix2& Other) const {
        Matrix2 Result;
        for (int I = 0; I < 2; ++I) {
            for (int J = 0; J < 2; ++J) {
                Result.M[I][J] = M[I][J] + Other.M[I][J];
}
}
        return Result;
    }

    Matrix2 operator-(const Matrix2& Other) const {
        Matrix2 Result;
        for (int I = 0; I < 2; ++I) {
            for (int J = 0; J < 2; ++J) {
                Result.M[I][J] = M[I][J] - Other.M[I][J];
            }
        }
        return Result;
    }

    Matrix2 operator*(float Scalar) const {
        Matrix2 Result;
        for (int I = 0; I < 2; ++I) {
            for (int J = 0; J < 2; ++J) {
                Result.M[I][J] = M[I][J] * Scalar;
            }
        }
        return Result;
    }

    Matrix2 operator*(const Matrix2& Other) const {
        Matrix2 Result;
        for (int I = 0; I < 2; ++I) {
            for (int J = 0; J < 2; ++J) {
                Result.M[I][J] = M[I][0] * Other.M[0][J] + M[I][1] * Other.M[1][J];
            }
        }
        return Result;
    }

    Matrix2& operator+=(const Matrix2& O) { *this = *this + O; return *this; }
    Matrix2& operator-=(const Matrix2& O) { *this = *this - O; return *this; }
    Matrix2& operator*=(const Matrix2& O) { *this = *this * O; return *this; }
    Matrix2& operator*=(float S) { *this = *this * S; return *this; }

    bool operator==(const Matrix2& Rhs) const {
        for (int I = 0; I < 2; ++I) {
            for (int J = 0; J < 2; ++J) {
                if (M[I][J] != Rhs.M[I][J]) { return false;
}
}
}
        return true;
    }

    bool operator!=(const Matrix2& Rhs) const {
        return !(*this == Rhs);
    }

    [[nodiscard]] Matrix2 Transposed() const {
        Matrix2 Result;
        Result.M[0][0] = M[0][0]; Result.M[0][1] = M[1][0];
        Result.M[1][0] = M[0][1]; Result.M[1][1] = M[1][1];
        return Result;
    }

    [[nodiscard]] float Determinant() const {
        return (M[0][0] * M[1][1]) - (M[0][1] * M[1][0]);
    }

    [[nodiscard]] Matrix2 Inverse() const {
        float Det = Determinant();
        assert(Det != 0.0f);
        Matrix2 Result;
        Result.M[0][0] = M[1][1] / Det;
        Result.M[0][1] = -M[0][1] / Det;
        Result.M[1][0] = -M[1][0] / Det;
        Result.M[1][1] = M[0][0] / Det;
        return Result;
    }
};

struct Matrix3 {
    float M[3][3];

    Matrix3() {
        for (int I = 0; I < 3; ++I) {
            for (int J = 0; J < 3; ++J) {
                M[I][J] = (I == J) ? 1.0f : 0.0f;
}
}
    }

    static Matrix3 Identity() { return {}; }

    void Print() const {
        for (const auto *I : M) {
            for (int J = 0; J < 3; ++J) {
                std::cout << I[J] << " ";
}
            std::cout << std::endl;
        }
    }

    Matrix3 operator+(const Matrix3& Other) const {
        Matrix3 Result;
        for (int I = 0; I < 3; ++I) {
            for (int J = 0; J < 3; ++J) {
                Result.M[I][J] = M[I][J] + Other.M[I][J];
}
}
        return Result;
    }

    Matrix3 operator-(const Matrix3& Other) const {
        Matrix3 Result;
        for (int I = 0; I < 3; ++I) {
            for (int J = 0; J < 3; ++J) {
                Result.M[I][J] = M[I][J] - Other.M[I][J];
}
}
        return Result;
    }

    Matrix3 operator*(float Scalar) const {
        Matrix3 Result;
        for (int I = 0; I < 3; ++I) {
            for (int J = 0; J < 3; ++J) {
                Result.M[I][J] = M[I][J] * Scalar;
}
}
        return Result;
    }

    Matrix3 operator*(const Matrix3& Other) const {
        Matrix3 Result;
        for (int I = 0; I < 3; ++I) {
            for (int J = 0; J < 3; ++J) {
                Result.M[I][J] = M[I][0] * Other.M[0][J] + M[I][1] * Other.M[1][J] + M[I][2] * Other.M[2][J];
            }
        }
        return Result;
    }

    Matrix3& operator+=(const Matrix3& O) { *this = *this + O; return *this; }
    Matrix3& operator-=(const Matrix3& O) { *this = *this - O; return *this; }
    Matrix3& operator*=(const Matrix3& O) { *this = *this * O; return *this; }
    Matrix3& operator*=(float S) { *this = *this * S; return *this; }

    bool operator==(const Matrix3& Rhs) const {
        for (int I = 0; I < 3; ++I) {
            for (int J = 0; J < 3; ++J) {
                if (M[I][J] != Rhs.M[I][J]) { return false;
}
}
}
        return true;
    }

    bool operator!=(const Matrix3& Rhs) const {
        return !(*this == Rhs);
    }

    [[nodiscard]] Matrix3 Transposed() const {
        Matrix3 Result;
        for (int I = 0; I < 3; ++I) {
            for (int J = 0; J < 3; ++J) {
                Result.M[I][J] = M[J][I];
}
}
        return Result;
    }

    Vec3 operator*(const Vec3& V) const {
        return {
            (M[0][0] * V.x) + (M[0][1] * V.y) + (M[0][2] * V.z),
            (M[1][0] * V.x) + (M[1][1] * V.y) + (M[1][2] * V.z),
            (M[2][0] * V.x) + (M[2][1] * V.y) + (M[2][2] * V.z)
        };
    }
};

struct Matrix4 {
    float M[4][4];

    Matrix4() {
        for (int I = 0; I < 4; ++I) {
            for (int J = 0; J < 4; ++J) {
                M[I][J] = (I == J) ? 1.0f : 0.0f;
}
}
    }

    static Matrix4 Identity() { return {}; }

    [[nodiscard]] Vec3 GetTranslation() const {
        return {M[0][3], M[1][3], M[2][3]};
    }

    void Print() const {
        for (const auto *I : M) {
            for (int J = 0; J < 4; ++J) {
                std::cout << I[J] << " ";
}
            std::cout << std::endl;
        }
    }

    Matrix4 operator+(const Matrix4& Other) const {
        Matrix4 Result;
        for (int I = 0; I < 4; ++I) {
            for (int J = 0; J < 4; ++J) {
                Result.M[I][J] = M[I][J] + Other.M[I][J];
}
}
        return Result;
    }

    Matrix4 operator-(const Matrix4& Other) const {
        Matrix4 Result;
        for (int I = 0; I < 4; ++I) {
            for (int J = 0; J < 4; ++J) {
                Result.M[I][J] = M[I][J] - Other.M[I][J];
}
}
        return Result;
    }

    Matrix4 operator*(float Scalar) const {
        Matrix4 Result;
        for (int I = 0; I < 4; ++I) {
            for (int J = 0; J < 4; ++J) {
                Result.M[I][J] = M[I][J] * Scalar;
}
}
        return Result;
    }

    Matrix4 operator*(const Matrix4& Other) const {
        Matrix4 Result;
        for (int I = 0; I < 4; ++I) {
            for (int J = 0; J < 4; ++J) {
                Result.M[I][J] = 0.0f;
                for (int K = 0; K < 4; ++K) {
                    Result.M[I][J] += M[I][K] * Other.M[K][J];
}
            }
        }
        return Result;
    }

    Matrix4& operator+=(const Matrix4& O) { *this = *this + O; return *this; }
    Matrix4& operator-=(const Matrix4& O) { *this = *this - O; return *this; }
    Matrix4& operator*=(const Matrix4& O) { *this = *this * O; return *this; }
    Matrix4& operator*=(float S) { *this = *this * S; return *this; }

    bool operator==(const Matrix4& Rhs) const {
        for (int I = 0; I < 4; ++I) {
            for (int J = 0; J < 4; ++J) {
                if (M[I][J] != Rhs.M[I][J]) { return false;
}
}
}
        return true;
    }

    bool operator!=(const Matrix4& Rhs) const {
        return !(*this == Rhs);
    }

    [[nodiscard]] Matrix4 Transposed() const {
        Matrix4 Result;
        for (int I = 0; I < 4; ++I) {
            for (int J = 0; J < 4; ++J) {
                Result.M[I][J] = M[J][I];
}
}
        return Result;
    }

    Vec4 operator*(const Vec4& V) const {
        return {
            (M[0][0] * V.x) + (M[0][1] * V.y) + (M[0][2] * V.z) + (M[0][3] * V.w),
            (M[1][0] * V.x) + (M[1][1] * V.y) + (M[1][2] * V.z) + (M[1][3] * V.w),
            (M[2][0] * V.x) + (M[2][1] * V.y) + (M[2][2] * V.z) + (M[2][3] * V.w),
            (M[3][0] * V.x) + (M[3][1] * V.y) + (M[3][2] * V.z) + (M[3][3] * V.w)
        };
    }

    static Matrix4 Translation(const Vec3& T) {
        Matrix4 Result = Identity();
        Result.M[0][3] = T.x;
        Result.M[1][3] = T.y;
        Result.M[2][3] = T.z;
        return Result;
    }

    static Matrix4 Scale(const Vec3& S) {
        Matrix4 Result = Identity();
        Result.M[0][0] = S.x;
        Result.M[1][1] = S.y;
        Result.M[2][2] = S.z;
        return Result;
    }

    static Matrix4 RotationX(float Angle) {
        Matrix4 Result = Identity();
        float C = std::cos(Angle);
        float S = std::sin(Angle);
        Result.M[1][1] = C; Result.M[1][2] = -S;
        Result.M[2][1] = S; Result.M[2][2] = C;
        return Result;
    }

    static Matrix4 RotationY(float Angle) {
        Matrix4 Result = Identity();
        float C = std::cos(Angle);
        float S = std::sin(Angle);
        Result.M[0][0] = C; Result.M[0][2] = S;
        Result.M[2][0] = -S; Result.M[2][2] = C;
        return Result;
    }

    static Matrix4 RotationZ(float Angle) {
        Matrix4 Result = Identity();
        float C = std::cos(Angle);
        float S = std::sin(Angle);
        Result.M[0][0] = C; Result.M[0][1] = -S;
        Result.M[1][0] = S; Result.M[1][1] = C;
        return Result;
    }

    static Matrix4 RotationAxis(const Vec3& Axis, float Angle) {
        Matrix4 Result = Identity();
        float C = std::cos(Angle);
        float S = std::sin(Angle);
        float T = 1.0f - C;
        Vec3 A = Axis.Normalized();

        Result.M[0][0] = T * A.x * A.x + C;
        Result.M[0][1] = T * A.x * A.y - S * A.z;
        Result.M[0][2] = T * A.x * A.z + S * A.y;

        Result.M[1][0] = T * A.x * A.y + S * A.z;
        Result.M[1][1] = T * A.y * A.y + C;
        Result.M[1][2] = T * A.y * A.z - S * A.x;

        Result.M[2][0] = T * A.x * A.z - S * A.y;
        Result.M[2][1] = T * A.y * A.z + S * A.x;
        Result.M[2][2] = T * A.z * A.z + C;

        return Result;
    }

    static Matrix4 Perspective(float FovY, float Aspect, float NearPlane, float FarPlane) {
        Matrix4 Result{};
        float F = 1.0f / std::tan(FovY * 0.5f);
        Result.M[0][0] = F / Aspect;
        Result.M[1][1] = F;
        Result.M[2][2] = (FarPlane + NearPlane) / (NearPlane - FarPlane);
        Result.M[2][3] = (2 * FarPlane * NearPlane) / (NearPlane - FarPlane);
        Result.M[3][2] = -1.0f;
        Result.M[3][3] = 0.0f;
        return Result;
    }

    // Asymmetric frustum projection (for VR)
    static Matrix4 Frustum(float Left, float Right, float Bottom, float Top, float NearPlane, float FarPlane) {
        Matrix4 Result{};
        float InvWidth = 1.0f / (Right - Left);
        float InvHeight = 1.0f / (Top - Bottom);
        float InvDepth = 1.0f / (FarPlane - NearPlane);

        Result.M[0][0] = 2.0f * NearPlane * InvWidth;
        Result.M[0][1] = 0.0f;
        Result.M[0][2] = 0.0f;
        Result.M[0][3] = 0.0f;

        Result.M[1][0] = 0.0f;
        Result.M[1][1] = 2.0f * NearPlane * InvHeight;
        Result.M[1][2] = 0.0f;
        Result.M[1][3] = 0.0f;

        Result.M[2][0] = (Right + Left) * InvWidth;
        Result.M[2][1] = (Top + Bottom) * InvHeight;
        Result.M[2][2] = -(FarPlane + NearPlane) * InvDepth;
        Result.M[2][3] = -1.0f;

        Result.M[3][0] = 0.0f;
        Result.M[3][1] = 0.0f;
        Result.M[3][2] = -2.0f * FarPlane * NearPlane * InvDepth;
        Result.M[3][3] = 0.0f;

        return Result;
    }

    static Matrix4 Orthographic(float Left, float Right, float Bottom, float Top, float NearPlane, float FarPlane) {
        Matrix4 Result = Identity();
        Result.M[0][0] = 2.0f / (Right - Left);
        Result.M[1][1] = 2.0f / (Top - Bottom);
        Result.M[2][2] = -2.0f / (FarPlane - NearPlane);
        Result.M[0][3] = -(Right + Left) / (Right - Left);
        Result.M[1][3] = -(Top + Bottom) / (Top - Bottom);
        Result.M[2][3] = -(FarPlane + NearPlane) / (FarPlane - NearPlane);
        return Result;
    }

    static Matrix4 LookAt(const Vec3& Eye, const Vec3& Target, const Vec3& Up) {
        Vec3 Z = (Eye - Target).Normalized();
        Vec3 X = Up.Cross(Z).Normalized();
        Vec3 Y = Z.Cross(X);

        Matrix4 Result = Identity();
        Result.M[0][0] = X.x; Result.M[0][1] = X.y; Result.M[0][2] = X.z; Result.M[0][3] = -X.Dot(Eye);
        Result.M[1][0] = Y.x; Result.M[1][1] = Y.y; Result.M[1][2] = Y.z; Result.M[1][3] = -Y.Dot(Eye);
        Result.M[2][0] = Z.x; Result.M[2][1] = Z.y; Result.M[2][2] = Z.z; Result.M[2][3] = -Z.Dot(Eye);
        return Result;
    }

    [[nodiscard]] Vec3 TransformPoint(const Vec3& Point) const {
        Vec4 V(Point.x, Point.y, Point.z, 1.0f);
        Vec4 R = *this * V;
        return {R.x, R.y, R.z};
    }

    [[nodiscard]] Vec3 TransformVector(const Vec3& Vector) const {
        Vec4 V(Vector.x, Vector.y, Vector.z, 0.0f);
        Vec4 R = *this * V;
        return {R.x, R.y, R.z};
    }

    [[nodiscard]] Matrix4 Inverse() const {
        float S0 = (M[0][0] * M[1][1]) - (M[1][0] * M[0][1]);
        float S1 = (M[0][0] * M[1][2]) - (M[1][0] * M[0][2]);
        float S2 = (M[0][0] * M[1][3]) - (M[1][0] * M[0][3]);
        float S3 = (M[0][1] * M[1][2]) - (M[1][1] * M[0][2]);
        float S4 = (M[0][1] * M[1][3]) - (M[1][1] * M[0][3]);
        float S5 = (M[0][2] * M[1][3]) - (M[1][2] * M[0][3]);

        float C5 = (M[2][2] * M[3][3]) - (M[3][2] * M[2][3]);
        float C4 = (M[2][1] * M[3][3]) - (M[3][1] * M[2][3]);
        float C3 = (M[2][1] * M[3][2]) - (M[3][1] * M[2][2]);
        float C2 = (M[2][0] * M[3][3]) - (M[3][0] * M[2][3]);
        float C1 = (M[2][0] * M[3][2]) - (M[3][0] * M[2][2]);
        float C0 = (M[2][0] * M[3][1]) - (M[3][0] * M[2][1]);

        float Det = ((S0 * C5) - (S1 * C4) + (S2 * C3) + (S3 * C2) - (S4 * C1) + (S5 * C0));
        if (std::abs(Det) < 1e-6f) { return Identity();
}

        float InvDet = 1.0f / Det;

        Matrix4 Result;
        Result.M[0][0] = ( M[1][1] * C5 - M[1][2] * C4 + M[1][3] * C3) * InvDet;
        Result.M[0][1] = (-M[0][1] * C5 + M[0][2] * C4 - M[0][3] * C3) * InvDet;
        Result.M[0][2] = ( M[3][1] * S5 - M[3][2] * S4 + M[3][3] * S3) * InvDet;
        Result.M[0][3] = (-M[2][1] * S5 + M[2][2] * S4 - M[2][3] * S3) * InvDet;

        Result.M[1][0] = (-M[1][0] * C5 + M[1][2] * C2 - M[1][3] * C1) * InvDet;
        Result.M[1][1] = ( M[0][0] * C5 - M[0][2] * C2 + M[0][3] * C1) * InvDet;
        Result.M[1][2] = (-M[3][0] * S5 + M[3][2] * S2 - M[3][3] * S1) * InvDet;
        Result.M[1][3] = ( M[2][0] * S5 - M[2][2] * S2 + M[2][3] * S1) * InvDet;

        Result.M[2][0] = ( M[1][0] * C4 - M[1][1] * C2 + M[1][3] * C0) * InvDet;
        Result.M[2][1] = (-M[0][0] * C4 + M[0][1] * C2 - M[0][3] * C0) * InvDet;
        Result.M[2][2] = ( M[3][0] * S4 - M[3][1] * S2 + M[3][3] * S0) * InvDet;
        Result.M[2][3] = (-M[2][0] * S4 + M[2][1] * S2 - M[2][3] * S0) * InvDet;

        Result.M[3][0] = (-M[1][0] * C3 + M[1][1] * C1 - M[1][2] * C0) * InvDet;
        Result.M[3][1] = ( M[0][0] * C3 - M[0][1] * C1 + M[0][2] * C0) * InvDet;
        Result.M[3][2] = (-M[3][0] * S3 + M[3][1] * S1 - M[3][2] * S0) * InvDet;
        Result.M[3][3] = ( M[2][0] * S3 - M[2][1] * S1 + M[2][2] * S0) * InvDet;

        return Result;
    }

    [[nodiscard]] float Determinant() const {
        float S0 = (M[0][0] * M[1][1]) - (M[1][0] * M[0][1]);
        float S1 = (M[0][0] * M[1][2]) - (M[1][0] * M[0][2]);
        float S2 = (M[0][0] * M[1][3]) - (M[1][0] * M[0][3]);
        float S3 = (M[0][1] * M[1][2]) - (M[1][1] * M[0][2]);
        float S4 = (M[0][1] * M[1][3]) - (M[1][1] * M[0][3]);
        float S5 = (M[0][2] * M[1][3]) - (M[1][2] * M[0][3]);

        float C5 = (M[2][2] * M[3][3]) - (M[3][2] * M[2][3]);
        float C4 = (M[2][1] * M[3][3]) - (M[3][1] * M[2][3]);
        float C3 = (M[2][1] * M[3][2]) - (M[3][1] * M[2][2]);
        float C2 = (M[2][0] * M[3][3]) - (M[3][0] * M[2][3]);
        float C1 = (M[2][0] * M[3][2]) - (M[3][0] * M[2][2]);
        float C0 = (M[2][0] * M[3][1]) - (M[3][0] * M[2][1]);

        return ((S0 * C5) - (S1 * C4) + (S2 * C3) + (S3 * C2) - (S4 * C1) + (S5 * C0));
    }
};

inline Matrix2 operator*(float Scalar, const Matrix2& M) { return M * Scalar; }
inline Matrix3 operator*(float Scalar, const Matrix3& M) { return M * Scalar; }
inline Matrix4 operator*(float Scalar, const Matrix4& M) { return M * Scalar; }

}

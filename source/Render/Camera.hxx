#pragma once

#include "../SolsticeExport.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <vector>

namespace Solstice::Render {

struct SOLSTICE_API Frustum {
    Math::Vec4 Planes[6]; // Left, Right, Bottom, Top, Near, Far

    // Normalize planes
    void Normalize() {
        for (int i = 0; i < 6; ++i) {
            float length = Math::Vec3(Planes[i].x, Planes[i].y, Planes[i].z).Magnitude();
            if (length > 0.0f) Planes[i] /= length;
        }
    }

    // Check if AABB is visible
    bool IsBoxVisible(const Math::Vec3& Min, const Math::Vec3& Max) const {
        for (int i = 0; i < 6; ++i) {
            // Find the point on the AABB closest to the plane normal
            Math::Vec3 P;
            P.x = (Planes[i].x > 0) ? Max.x : Min.x;
            P.y = (Planes[i].y > 0) ? Max.y : Min.y;
            P.z = (Planes[i].z > 0) ? Max.z : Min.z;

            // If the closest point is behind the plane, the box is outside
            if (Planes[i].Dot(Math::Vec4(P.x, P.y, P.z, 1.0f)) < 0.0f) {
                return false;
            }
        }
        return true;
    }
};

struct SOLSTICE_API Camera {
    Camera(Math::Vec3 Position = Math::Vec3(0.0f, 0.0f, 3.0f),
           Math::Vec3 Up = Math::Vec3(0.0f, 1.0f, 0.0f),
           float Yaw = -90.0f,
           float Pitch = 0.0f);

    Camera(float PosX, float PosY, float PosZ,
           float UpX, float UpY, float UpZ,
           float Yaw, float Pitch);

    Math::Matrix4 GetViewMatrix() const;
    Frustum GetFrustum(float Aspect, float FOV, float Near, float Far) const;

    void ProcessKeyboard(Math::Vec3 Direction, float DeltaTime);
    void ProcessMouseMovement(float XOffset, float YOffset, bool ConstrainPitch = true);
    void ProcessMouseScroll(float YOffset);

    Math::Vec3 GetPosition() const { return Position; }
    Math::Vec3 GetFront() const { return Front; }
    float GetZoom() const { return Zoom; }

    Math::Vec3 Position, Front, Up, Right, WorldUp;
    
    float Yaw, Pitch;
    float MovementSpeed, MouseSensitivity, Zoom;
    
private:
    // Calculates the front vector from the Camera's (updated) Euler angles
    void UpdateCameraVectors();
};

} // namespace Solstice::Render
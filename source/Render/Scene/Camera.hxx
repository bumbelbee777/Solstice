#pragma once

#include <Solstice.hxx>
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

// VR Camera configuration for stereo rendering
struct SOLSTICE_API VRCameraConfig {
    float IPD = 0.064f; // Inter-pupillary distance in meters (64mm default)
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;
    float FOV = 90.0f; // Vertical FOV in degrees

    // Asymmetric frustum parameters (for VR)
    struct AsymmetricFrustum {
        float Left;
        float Right;
        float Bottom;
        float Top;
    } LeftEye, RightEye;

    bool IsVR = false;
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
    /// Perspective (from Zoom FOV) or orthographic from OrthoHalfExtentY; must match between scene and fluid overlay.
    Math::Matrix4 GetProjectionMatrix(float aspectRatio, float nearPlane = 0.1f, float farPlane = 2000.0f) const;
    Frustum GetFrustum(float Aspect, float FOV, float Near, float Far) const;

    // VR stereo camera support
    Math::Matrix4 GetViewMatrixVR(bool leftEye) const;
    Math::Matrix4 GetProjectionMatrixVR(bool leftEye, float aspect) const;
    Frustum GetFrustumVR(bool leftEye, float aspect) const;

    // Set VR configuration
    void SetVRConfig(const VRCameraConfig& config) { m_VRConfig = config; }
    const VRCameraConfig& GetVRConfig() const { return m_VRConfig; }
    bool IsVR() const { return m_VRConfig.IsVR; }

    void ProcessKeyboard(Math::Vec3 Direction, float DeltaTime);
    void ProcessMouseMovement(float XOffset, float YOffset, bool ConstrainPitch = true);
    void ProcessMouseScroll(float YOffset);

    // Update camera with interpolation (call each frame)
    void Update(float DeltaTime);

    // Set target position for interpolation
    void SetTargetPosition(const Math::Vec3& TargetPos) { m_TargetPosition = TargetPos; }
    Math::Vec3 GetTargetPosition() const { return m_TargetPosition; }

    // Set interpolation speed (higher = faster, default 8.0)
    void SetInterpolationSpeed(float Speed) { m_InterpolationSpeed = Speed; }
    float GetInterpolationSpeed() const { return m_InterpolationSpeed; }

    Math::Vec3 GetPosition() const { return Position; }
    Math::Vec3 GetFront() const { return Front; }
    float GetZoom() const { return Zoom; }

    Math::Vec3 Position, Front, Up, Right, WorldUp;

    float Yaw, Pitch;
    float MovementSpeed, MouseSensitivity, Zoom;

    /// When true, GetProjectionMatrix uses orthographic Y half-extent (world-aligned with view Up); width follows aspect.
    bool UseOrthographic = false;
    float OrthoHalfExtentY = 1.0f;

private:
    // Calculates the front vector from the Camera's (updated) Euler angles
    void UpdateCameraVectors();

    // VR configuration
    VRCameraConfig m_VRConfig;

    // Interpolation state
    Math::Vec3 m_TargetPosition;
    float m_InterpolationSpeed{20.0f}; // Higher default for more responsive movement
};

} // namespace Solstice::Render

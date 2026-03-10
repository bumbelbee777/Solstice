#include <Render/Scene/Camera.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Render {

// Constructor with vectors
Camera::Camera(Math::Vec3 Position, Math::Vec3 Up, float Yaw, float Pitch)
    : Front(Math::Vec3(0.0f, 0.0f, -1.0f)),
      MovementSpeed(2.5f),
      MouseSensitivity(0.2f),
      Zoom(45.0f),
      m_TargetPosition(Position),
      m_InterpolationSpeed(20.0f) {
    this->Position = Position;
    WorldUp = Up;
    this->Yaw = Yaw;
    this->Pitch = Pitch;
    UpdateCameraVectors();
}

// Constructor with scalar values
Camera::Camera(float PosX, float PosY, float PosZ, float UpX, float UpY, float UpZ, float Yaw, float Pitch)
    : Front(Math::Vec3(0.0f, 0.0f, -1.0f)),
      MovementSpeed(2.5f),
      MouseSensitivity(0.2f),
      Zoom(45.0f),
      m_TargetPosition(Math::Vec3(PosX, PosY, PosZ)),
      m_InterpolationSpeed(20.0f) {
    Position = Math::Vec3(PosX, PosY, PosZ);
    WorldUp = Math::Vec3(UpX, UpY, UpZ);
    this->Yaw = Yaw;
    this->Pitch = Pitch;
    UpdateCameraVectors();
}

// Returns the view matrix calculated using Euler Angles and the LookAt Matrix
Math::Matrix4 Camera::GetViewMatrix() const {
    Math::Vec3 eye = Position;
    if (std::fabs(eye.x) < 1e-6f && std::fabs(eye.y) < 1e-6f && std::fabs(eye.z) < 1e-6f) {
        eye = Math::Vec3(0.0f, 0.0f, 2.0f);
    }
    return Math::Matrix4::LookAt(eye, eye + Front, Up);
}

// VR stereo view matrix (offset by IPD)
Math::Matrix4 Camera::GetViewMatrixVR(bool leftEye) const {
    Math::Vec3 eye = Position;
    if (std::fabs(eye.x) < 1e-6f && std::fabs(eye.y) < 1e-6f && std::fabs(eye.z) < 1e-6f) {
        eye = Math::Vec3(0.0f, 0.0f, 2.0f);
    }
    
    // Offset eye position by half IPD in the right direction
    float eyeOffset = m_VRConfig.IPD * 0.5f;
    Math::Vec3 eyePos = eye + Right * (leftEye ? -eyeOffset : eyeOffset);
    
    return Math::Matrix4::LookAt(eyePos, eyePos + Front, Up);
}

// VR asymmetric frustum projection
Math::Matrix4 Camera::GetProjectionMatrixVR(bool leftEye, float aspect) const {
    const auto& frustum = leftEye ? m_VRConfig.LeftEye : m_VRConfig.RightEye;
    float near = m_VRConfig.NearPlane;
    float far = m_VRConfig.FarPlane;
    
    // Create asymmetric perspective projection
    // Using OpenGL-style frustum (left, right, bottom, top, near, far)
    return Math::Matrix4::Frustum(frustum.Left, frustum.Right, 
                                  frustum.Bottom, frustum.Top, 
                                  near, far);
}

// VR frustum culling
Frustum Camera::GetFrustumVR(bool leftEye, float aspect) const {
    Frustum frustum;
    Math::Matrix4 view = GetViewMatrixVR(leftEye);
    Math::Matrix4 proj = GetProjectionMatrixVR(leftEye, aspect);
    Math::Matrix4 viewProj = proj * view;
    
    // Extract frustum planes (same as regular GetFrustum)
    frustum.Planes[0].x = viewProj.M[3][0] + viewProj.M[0][0];
    frustum.Planes[0].y = viewProj.M[3][1] + viewProj.M[0][1];
    frustum.Planes[0].z = viewProj.M[3][2] + viewProj.M[0][2];
    frustum.Planes[0].w = viewProj.M[3][3] + viewProj.M[0][3];
    
    frustum.Planes[1].x = viewProj.M[3][0] - viewProj.M[0][0];
    frustum.Planes[1].y = viewProj.M[3][1] - viewProj.M[0][1];
    frustum.Planes[1].z = viewProj.M[3][2] - viewProj.M[0][2];
    frustum.Planes[1].w = viewProj.M[3][3] - viewProj.M[0][3];
    
    frustum.Planes[2].x = viewProj.M[3][0] + viewProj.M[1][0];
    frustum.Planes[2].y = viewProj.M[3][1] + viewProj.M[1][1];
    frustum.Planes[2].z = viewProj.M[3][2] + viewProj.M[1][2];
    frustum.Planes[2].w = viewProj.M[3][3] + viewProj.M[1][3];
    
    frustum.Planes[3].x = viewProj.M[3][0] - viewProj.M[1][0];
    frustum.Planes[3].y = viewProj.M[3][1] - viewProj.M[1][1];
    frustum.Planes[3].z = viewProj.M[3][2] - viewProj.M[1][2];
    frustum.Planes[3].w = viewProj.M[3][3] - viewProj.M[1][3];
    
    frustum.Planes[4].x = viewProj.M[3][0] + viewProj.M[2][0];
    frustum.Planes[4].y = viewProj.M[3][1] + viewProj.M[2][1];
    frustum.Planes[4].z = viewProj.M[3][2] + viewProj.M[2][2];
    frustum.Planes[4].w = viewProj.M[3][3] + viewProj.M[2][3];
    
    frustum.Planes[5].x = viewProj.M[3][0] - viewProj.M[2][0];
    frustum.Planes[5].y = viewProj.M[3][1] - viewProj.M[2][1];
    frustum.Planes[5].z = viewProj.M[3][2] - viewProj.M[2][2];
    frustum.Planes[5].w = viewProj.M[3][3] - viewProj.M[2][3];
    
    frustum.Normalize();
    return frustum;
}

Frustum Camera::GetFrustum(float Aspect, float FOV, float Near, float Far) const {
    Frustum frustum;
    Math::Matrix4 view = GetViewMatrix();
    Math::Matrix4 proj = Math::Matrix4::Perspective(Math::Vec3(FOV * (3.14159f / 180.0f), 0, 0).x, Aspect, Near, Far);
    Math::Matrix4 viewProj = proj * view; // Note: Check matrix multiplication order in Math lib (Row vs Column major)

    // Assuming Row-Major memory layout but Column-Vector multiplication (OpenGL style)
    // If Math::Matrix4 is standard, M[row][col]
    // Gribb/Hartmann method for plane extraction

    // Left plane
    frustum.Planes[0].x = viewProj.M[3][0] + viewProj.M[0][0];
    frustum.Planes[0].y = viewProj.M[3][1] + viewProj.M[0][1];
    frustum.Planes[0].z = viewProj.M[3][2] + viewProj.M[0][2];
    frustum.Planes[0].w = viewProj.M[3][3] + viewProj.M[0][3];

    // Right plane
    frustum.Planes[1].x = viewProj.M[3][0] - viewProj.M[0][0];
    frustum.Planes[1].y = viewProj.M[3][1] - viewProj.M[0][1];
    frustum.Planes[1].z = viewProj.M[3][2] - viewProj.M[0][2];
    frustum.Planes[1].w = viewProj.M[3][3] - viewProj.M[0][3];

    // Bottom plane
    frustum.Planes[2].x = viewProj.M[3][0] + viewProj.M[1][0];
    frustum.Planes[2].y = viewProj.M[3][1] + viewProj.M[1][1];
    frustum.Planes[2].z = viewProj.M[3][2] + viewProj.M[1][2];
    frustum.Planes[2].w = viewProj.M[3][3] + viewProj.M[1][3];

    // Top plane
    frustum.Planes[3].x = viewProj.M[3][0] - viewProj.M[1][0];
    frustum.Planes[3].y = viewProj.M[3][1] - viewProj.M[1][1];
    frustum.Planes[3].z = viewProj.M[3][2] - viewProj.M[1][2];
    frustum.Planes[3].w = viewProj.M[3][3] - viewProj.M[1][3];

    // Near plane
    frustum.Planes[4].x = viewProj.M[3][0] + viewProj.M[2][0];
    frustum.Planes[4].y = viewProj.M[3][1] + viewProj.M[2][1];
    frustum.Planes[4].z = viewProj.M[3][2] + viewProj.M[2][2];
    frustum.Planes[4].w = viewProj.M[3][3] + viewProj.M[2][3];

    // Far plane
    frustum.Planes[5].x = viewProj.M[3][0] - viewProj.M[2][0];
    frustum.Planes[5].y = viewProj.M[3][1] - viewProj.M[2][1];
    frustum.Planes[5].z = viewProj.M[3][2] - viewProj.M[2][2];
    frustum.Planes[5].w = viewProj.M[3][3] - viewProj.M[2][3];

    frustum.Normalize();
    return frustum;
}

// Processes input received from any keyboard-like input system
void Camera::ProcessKeyboard(Math::Vec3 Direction, float DeltaTime) {
    float Velocity = MovementSpeed * DeltaTime;
    Position += Direction * Velocity;
}

// Processes input received from a mouse input system
void Camera::ProcessMouseMovement(float XOffset, float YOffset, bool ConstrainPitch) {
    XOffset *= MouseSensitivity;
    YOffset *= MouseSensitivity;

    Yaw += XOffset;
    Pitch += YOffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (ConstrainPitch) {
        Pitch = std::clamp(Pitch, -89.0f, 89.0f);
    }

    // Update Front, Right and Up Vectors using the updated Euler angles
    UpdateCameraVectors();
}

// Processes input received from a mouse scroll-wheel event
void Camera::ProcessMouseScroll(float YOffset) {
    Zoom -= (float)YOffset;
    Zoom = std::clamp(Zoom, 1.0f, 90.0f);
}

// Update camera with interpolation
void Camera::Update(float DeltaTime) {
    // Smooth interpolation using exponential smoothing
    // This provides frame-rate independent smooth movement
    // Higher interpolation speed = faster response, less smoothing
    // Clamp delta time to prevent large jumps
    float clampedDelta = std::min(DeltaTime, 0.033f); // Cap at ~30fps minimum
    
    // Calculate distance to target
    Math::Vec3 diff = m_TargetPosition - Position;
    float distance = diff.Magnitude();
    
    // If very close to target, snap to it to avoid micro-stuttering
    if (distance < 0.001f) {
        Position = m_TargetPosition;
        return;
    }
    
    // Adaptive interpolation speed - faster when far away
    float adaptiveSpeed = m_InterpolationSpeed;
    if (distance > 1.0f) {
        // Increase speed for large distances to reduce visible lag
        adaptiveSpeed *= (1.0f + distance * 0.5f);
    }
    
    float t = 1.0f - std::exp(-adaptiveSpeed * clampedDelta);
    Position = Position + diff * t;
}

// Calculates the front vector from the Camera's (updated) Euler Angles
void Camera::UpdateCameraVectors() {
    // Calculate the new Front vector
    Math::Vec3 FrontVec;
    float radYaw = Yaw * (3.14159f / 180.0f);
    float radPitch = Pitch * (3.14159f / 180.0f);

    FrontVec.x = std::cos(radYaw) * std::cos(radPitch);
    FrontVec.y = std::sin(radPitch);
    FrontVec.z = std::sin(radYaw) * std::cos(radPitch);
    Front = FrontVec.Normalized();

    // For FPS camera: always use WorldUp to prevent rolling/tilting
    // Calculate Right vector from WorldUp and Front
    Right = WorldUp.Cross(Front).Normalized();
    // Always use WorldUp directly (prevents camera roll)
    Up = WorldUp;
}

} // namespace Solstice::Render

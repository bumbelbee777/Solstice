#pragma once

#include <algorithm>

#include "WohShipTypes.hxx"

#include <Render/Scene/Camera.hxx>
#include <cmath>

namespace Solstice::WarsOfHeaven {

/// Third-person offset from ship: **behind** along heading, **up** for horizon line. Camera looks at the ship.
inline void ApplyChaseCamera(Render::Camera& cam, const Math::Vec3& shipPos, const WohShipKinematics& kin, float chaseBack,
                             float chaseUp) {
    const float yrad = kin.YawDeg * 0.0174532924f;
    const float c = std::cos(yrad);
    const float s = std::sin(yrad);
    const Math::Vec3 forward{c, 0.0f, s};
    cam.Position = shipPos - forward * chaseBack + Math::Vec3(0.0f, chaseUp, 0.0f);
    const Math::Vec3 toShip = shipPos - cam.Position;
    const float mag = toShip.Magnitude();
    if (mag > 1e-4f) {
        const Math::Vec3 d = toShip * (1.0f / mag);
        const float pitchRad = std::asin((std::clamp)(d.y, -1.0f, 1.0f));
        const float yawRad = std::atan2(d.z, d.x);
        cam.Pitch = pitchRad * 57.2957795f;
        cam.Yaw = yawRad * 57.2957795f;
    }
    cam.ProcessMouseMovement(0.0f, 0.0f, true);
}

} // namespace Solstice::WarsOfHeaven

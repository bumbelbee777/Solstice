#include "ShapeHelpers.hxx"

namespace Solstice::Physics {

void GetBoxCorners(const Math::Vec3& halfExtents, Math::Vec3 corners[8]) {
    corners[0] = Math::Vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z);
    corners[1] = Math::Vec3( halfExtents.x, -halfExtents.y, -halfExtents.z);
    corners[2] = Math::Vec3( halfExtents.x,  halfExtents.y, -halfExtents.z);
    corners[3] = Math::Vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z);
    corners[4] = Math::Vec3(-halfExtents.x, -halfExtents.y,  halfExtents.z);
    corners[5] = Math::Vec3( halfExtents.x, -halfExtents.y,  halfExtents.z);
    corners[6] = Math::Vec3( halfExtents.x,  halfExtents.y,  halfExtents.z);
    corners[7] = Math::Vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z);
}

Math::Vec3 TransformPoint(const Math::Vec3& p, const Math::Vec3& pos, const Math::Quaternion& rot) {
    float x = rot.x, y = rot.y, z = rot.z, w = rot.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    Math::Vec3 result;
    result.x = p.x * (1.0f - yy - zz) + p.y * (xy - wz) + p.z * (xz + wy) + pos.x;
    result.y = p.x * (xy + wz) + p.y * (1.0f - xx - zz) + p.z * (yz - wx) + pos.y;
    result.z = p.x * (xz - wy) + p.y * (yz + wx) + p.z * (1.0f - xx - yy) + pos.z;
    return result;
}

void GetBoxTestPoints(const Math::Vec3& halfExtents, Math::Vec3 points[9]) {
    points[0] = Math::Vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z);
    points[1] = Math::Vec3( halfExtents.x, -halfExtents.y, -halfExtents.z);
    points[2] = Math::Vec3( halfExtents.x,  halfExtents.y, -halfExtents.z);
    points[3] = Math::Vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z);
    points[4] = Math::Vec3(-halfExtents.x, -halfExtents.y,  halfExtents.z);
    points[5] = Math::Vec3( halfExtents.x, -halfExtents.y,  halfExtents.z);
    points[6] = Math::Vec3( halfExtents.x,  halfExtents.y,  halfExtents.z);
    points[7] = Math::Vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z);
    points[8] = Math::Vec3(0, 0, 0); // Center
}

Math::Vec3 TransformPointToLocal(const Math::Vec3& worldPoint, const Math::Vec3& position, const Math::Quaternion& rotation) {
    // Transform: local = q_inv * (world - pos) * q
    Math::Vec3 relPos = worldPoint - position;
    Math::Quaternion qInv = rotation.Conjugate();

    // Rotate using quaternion: v_local = q_inv * v_world * q
    float x = qInv.x, y = qInv.y, z = qInv.z, w = qInv.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    Math::Vec3 result;
    result.x = relPos.x * (1.0f - yy - zz) + relPos.y * (xy - wz) + relPos.z * (xz + wy);
    result.y = relPos.x * (xy + wz) + relPos.y * (1.0f - xx - zz) + relPos.z * (yz - wx);
    result.z = relPos.x * (xz - wy) + relPos.y * (yz + wx) + relPos.z * (1.0f - xx - yy);
    return result;
}

} // namespace Solstice::Physics

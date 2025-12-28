#pragma once

#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"

namespace Solstice::Physics {

/// Geometry helper functions for shape transformations and queries

/// Get all 8 corners of a box in local space
void GetBoxCorners(const Math::Vec3& halfExtents, Math::Vec3 corners[8]);

/// Transform a point from local space to world space using position and rotation
Math::Vec3 TransformPoint(const Math::Vec3& localPoint, const Math::Vec3& position, const Math::Quaternion& rotation);

/// Get test points of a box (8 corners + center)
void GetBoxTestPoints(const Math::Vec3& halfExtents, Math::Vec3 points[9]);

/// Transform a point from world space to local space using position and rotation
Math::Vec3 TransformPointToLocal(const Math::Vec3& worldPoint, const Math::Vec3& position, const Math::Quaternion& rotation);

} // namespace Solstice::Physics

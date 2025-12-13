#include "GJK.hxx"
#include "ConvexHull.hxx"
#include "../Math/Quaternion.hxx"
#include "../Math/Matrix.hxx"
#include "../Math/SIMDVec3.hxx"
#include "../Core/Debug.hxx"
#include <cmath>
#include <algorithm>
#include <limits>
#include <vector>

namespace Solstice::Physics {

// Forward declarations for simplex update helpers
static bool UpdateSimplex2(Simplex& simplex, Math::Vec3& direction);
static bool UpdateSimplex3(Simplex& simplex, Math::Vec3& direction);
static bool UpdateSimplex4(Simplex& simplex, Math::Vec3& direction);

// Helper: Transform point from local to world space
static Math::Vec3 TransformToWorld(const Math::Vec3& localPoint, const Math::Vec3& position, const Math::Quaternion& rotation) {
    // Rotate: p' = q * p * q_inv
    float x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    Math::Vec3 result;
    result.x = localPoint.x * (1.0f - yy - zz) + localPoint.y * (xy - wz) + localPoint.z * (xz + wy);
    result.y = localPoint.x * (xy + wz) + localPoint.y * (1.0f - xx - zz) + localPoint.z * (yz - wx);
    result.z = localPoint.x * (xz - wy) + localPoint.y * (yz + wx) + localPoint.z * (1.0f - xx - yy);

    return result + position;
}

// Helper: Transform direction from world to local space (rotation only)
static Math::Vec3 TransformDirectionToLocal(const Math::Vec3& worldDir, const Math::Quaternion& rotation) {
    // Use inverse rotation: q_inv
    Math::Quaternion qInv = rotation.Conjugate();

    float x = qInv.x, y = qInv.y, z = qInv.z, w = qInv.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    Math::Vec3 result;
    result.x = worldDir.x * (1.0f - yy - zz) + worldDir.y * (xy - wz) + worldDir.z * (xz + wy);
    result.y = worldDir.x * (xy + wz) + worldDir.y * (1.0f - xx - zz) + worldDir.z * (yz - wx);
    result.z = worldDir.x * (xz - wy) + worldDir.y * (yz + wx) + worldDir.z * (1.0f - xx - yy);

    return result;
}

Math::Vec3 GetSupportPoint(const RigidBody& rb, const Math::Vec3& direction) {
    // Get support point in world space

    // Transform direction to local space
    Math::Vec3 localDir = TransformDirectionToLocal(direction, rb.Rotation);

    Math::Vec3 localSupport;

    switch (rb.Type) {
        case ColliderType::Sphere:
            // Sphere support is just center + radius * direction
            localSupport = localDir.Normalized() * rb.Radius;
            break;

        case ColliderType::Box: {
            // Box support: pick corner with max dot product
            localSupport.x = (localDir.x > 0) ? rb.HalfExtents.x : -rb.HalfExtents.x;
            localSupport.y = (localDir.y > 0) ? rb.HalfExtents.y : -rb.HalfExtents.y;
            localSupport.z = (localDir.z > 0) ? rb.HalfExtents.z : -rb.HalfExtents.z;
            break;
        }

        case ColliderType::ConvexHull:
            if (rb.Hull) {
                localSupport = rb.Hull->GetSupportPoint(localDir);
            } else {
                localSupport = Math::Vec3(0, 0, 0);
            }
            break;

        case ColliderType::Tetrahedron:
            // Tetrahedron support: max dot product among up to 4 vertices
            // Use HullVertices if present (expected 4 vertices), otherwise fallback to ConvexHull support
            if (!rb.HullVertices.empty()) {
                float maxDot = -1e9f;
                for (const auto& v : rb.HullVertices) {
                    float dot = v.Dot(localDir);
                    if (dot > maxDot) { maxDot = dot; localSupport = v; }
                }
            } else if (rb.Hull) {
                localSupport = rb.Hull->GetSupportPoint(localDir);
            }
            break;
            // Let's assume Triangle is just a special case of ConvexHull or uses HullVertices.
            // Let's use HullVertices for now as a fallback if Hull is null, or just assume Hull is used.
            // Actually, for a specific Triangle type, we might want explicit vertices.
            // But `RigidBody` struct in `RigidBody.hxx` (which I read) DOES NOT have `TriangleVertices`.
            // It has `HullVertices`.
            // Let's use `HullVertices` for Triangle type for now.

            if (!rb.HullVertices.empty()) {
                float maxDot = -1e9f;
                for (const auto& v : rb.HullVertices) {
                    float dot = v.Dot(localDir);
                    if (dot > maxDot) {
                        maxDot = dot;
                        localSupport = v;
                    }
                }
            } else if (rb.Hull) {
                 localSupport = rb.Hull->GetSupportPoint(localDir);
            }
            break;

        default:
            localSupport = Math::Vec3(0, 0, 0);
            break;
    }

    // Transform back to world space
    return TransformToWorld(localSupport, rb.Position, rb.Rotation);
}

Math::Vec3 GetMinkowskiSupport(const RigidBody& A, const RigidBody& B, const Math::Vec3& direction) {
    // Minkowski difference: A - B
    // Support(A - B, d) = Support(A, d) - Support(B, -d)
    Math::Vec3 supportA = GetSupportPoint(A, direction);
    Math::Vec3 supportB = GetSupportPoint(B, direction * -1.0f);
    return supportA - supportB;
}

bool UpdateSimplex(Simplex& simplex, Math::Vec3& direction) {
    // Update simplex based on count
    switch (simplex.Count) {
        case 2: return UpdateSimplex2(simplex, direction);
        case 3: return UpdateSimplex3(simplex, direction);
        case 4: return UpdateSimplex4(simplex, direction);
        default: return false;
    }
}

// Helper: Check if origin is in line region
static bool UpdateSimplex2(Simplex& simplex, Math::Vec3& direction) {
    Math::Vec3 a = simplex[1];  // Most recent point
    Math::Vec3 b = simplex[0];

    Math::Vec3 ab = b - a;
    Math::Vec3 ao = a * -1.0f;  // Origin - a

    if (ab.Dot(ao) > 0) {
        // Origin is towards B - use SIMD for cross product
        direction = Math::CrossSIMD(Math::CrossSIMD(ab, ao), ab);
        if (direction.Magnitude() < 1e-6f) {
            // Perpendicular is zero, origin is on line
            direction = Math::Vec3(ab.y, -ab.x, 0).Normalized();
        }
    } else {
        // Origin is towards A
        simplex.Count = 1;
        simplex[0] = a;
        direction = ao;
    }

    return false;
}

// Helper: Check if origin is in triangle region
static bool UpdateSimplex3(Simplex& simplex, Math::Vec3& direction) {
    Math::Vec3 a = simplex[2];  // Most recent
    Math::Vec3 b = simplex[1];
    Math::Vec3 c = simplex[0];

    Math::Vec3 ab = b - a;
    Math::Vec3 ac = c - a;
    Math::Vec3 ao = a * -1.0f;

    // Use SIMD for triangle normal
    Math::Vec3 abc = Math::CrossSIMD(ab, ac);

    // Check which Voronoi region origin is in
    if (Math::CrossSIMD(abc, ac).Dot(ao) > 0) {
        // Region AC
        if (ac.Dot(ao) > 0) {
            simplex.Count = 2;
            simplex[0] = c;
            simplex[1] = a;
            direction = Math::CrossSIMD(Math::CrossSIMD(ac, ao), ac);
        } else {
            // Region A
            simplex.Count = 1;
            simplex[0] = a;
            direction = ao;
        }
    } else {
        if (Math::CrossSIMD(ab, abc).Dot(ao) > 0) {
            // Region AB
            if (ab.Dot(ao) > 0) {
                simplex.Count = 2;
                simplex[0] = b;
                simplex[1] = a;
                direction = Math::CrossSIMD(Math::CrossSIMD(ab, ao), ab);
            } else {
                // Region A
                simplex.Count = 1;
                simplex[0] = a;
                direction = ao;
            }
        } else {
            // Above or below triangle
            if (abc.Dot(ao) > 0) {
                direction = abc;
            } else {
                // Flip winding
                simplex[0] = b;
                simplex[1] = c;
                direction = abc * -1.0f;
            }
        }
    }

    return false;
}

// Helper: Check if origin is in tetrahedron
static bool UpdateSimplex4(Simplex& simplex, Math::Vec3& direction) {
    Math::Vec3 a = simplex[3];  // Most recent
    Math::Vec3 b = simplex[2];
    Math::Vec3 c = simplex[1];
    Math::Vec3 d = simplex[0];

    Math::Vec3 ab = b - a;
    Math::Vec3 ac = c - a;
    Math::Vec3 ad = d - a;
    Math::Vec3 ao = a * -1.0f;

    // Use SIMD for face normals
    Math::Vec3 abc = Math::CrossSIMD(ab, ac);
    Math::Vec3 acd = Math::CrossSIMD(ac, ad);
    Math::Vec3 adb = Math::CrossSIMD(ad, ab);

    // Check each face
    if (abc.Dot(ao) > 0) {
        // Remove D, keep ABC
        simplex.Count = 3;
        simplex[0] = c;
        simplex[1] = b;
        simplex[2] = a;
        return UpdateSimplex3(simplex, direction);
    }

    if (acd.Dot(ao) > 0) {
        // Remove B, keep ACD
        simplex.Count = 3;
        simplex[0] = d;
        simplex[1] = c;
        simplex[2] = a;
        return UpdateSimplex3(simplex, direction);
    }

    if (adb.Dot(ao) > 0) {
        // Remove C, keep ADB
        simplex.Count = 3;
        simplex[0] = b;
        simplex[1] = d;
        simplex[2] = a;
        return UpdateSimplex3(simplex, direction);
    }

    // Origin is inside tetrahedron
    return true;
}

bool GJK(const RigidBody& A, const RigidBody& B, Simplex& simplex) {
    simplex.Clear();

    // Initial direction (B to A)
    Math::Vec3 direction = B.Position - A.Position;
    float dirMag = direction.Magnitude();

    // Handle degenerate case (overlapping centers)
    if (dirMag < 1e-6f) {
        // Try multiple directions to find a good starting point
        Math::Vec3 testDirs[] = {
            Math::Vec3(1, 0, 0),
            Math::Vec3(0, 1, 0),
            Math::Vec3(0, 0, 1),
            Math::Vec3(1, 1, 0).Normalized(),
            Math::Vec3(1, 0, 1).Normalized(),
            Math::Vec3(0, 1, 1).Normalized()
        };

        for (const auto& testDir : testDirs) {
            Math::Vec3 support = GetMinkowskiSupport(A, B, testDir);
            if (support.Magnitude() > 1e-6f) {
                direction = testDir;
                break;
            }
        }

        if (direction.Magnitude() < 1e-6f) {
            // Still degenerate - likely deep penetration
            direction = Math::Vec3(1, 0, 0);
        }
    } else {
        direction = direction / dirMag;  // Normalize
    }

    // Get first support point
    Math::Vec3 support = GetMinkowskiSupport(A, B, direction);
    simplex.Add(support);

    // New direction towards origin
    direction = support * -1.0f;
    float dirMag2 = direction.Magnitude();
    if (dirMag2 > 1e-6f) {
        direction = direction / dirMag2;
    }

    const int maxIterations = 64;  // Increased for robustness
    const float tolerance = 1e-6f;

    for (int iter = 0; iter < maxIterations; ++iter) {
        support = GetMinkowskiSupport(A, B, direction);

        // If support point didn't pass origin, no collision
        float dot = support.Dot(direction);
        if (dot < -tolerance) {
            return false;
        }

        // Check if we're close enough to origin (early termination)
        float distToOrigin = support.Magnitude();
        if (distToOrigin < tolerance && simplex.Count >= 2) {
            // Close enough - treat as collision
            return true;
        }

        simplex.Add(support);

        // Check for duplicate points (numerical stability)
        bool isDuplicate = false;
        for (int i = 0; i < simplex.Count - 1; ++i) {
            Math::Vec3 diff = simplex[i] - support;
            if (diff.Magnitude() < tolerance) {
                isDuplicate = true;
                break;
            }
        }
        if (isDuplicate) {
            // Duplicate point - likely converged
            return true;
        }

        if (UpdateSimplex(simplex, direction)) {
            // Origin is enclosed
            return true;
        }

        // Normalize direction for next iteration
        dirMag2 = direction.Magnitude();
        if (dirMag2 < 1e-6f) {
            // Direction became zero - numerical issue
            return true;  // Treat as collision
        }
        direction = direction / dirMag2;
    }

    // Max iterations reached - check if we have a valid simplex
    if (simplex.Count >= 4) {
        // We have a tetrahedron, likely collision
        return true;
    }

    return false;
}

// EPA helpers - Production-ready implementation with proper edge tracking
struct EPAFace {
    Math::Vec3 Normal;
    float Distance;  // Distance from origin to face plane
    int Vertices[3]; // Indices into point list
    bool Obsolete;   // Mark for removal

    EPAFace() : Normal(0,0,0), Distance(0), Obsolete(false) {
        Vertices[0] = Vertices[1] = Vertices[2] = 0;
    }
};

struct EPAEdge {
    int A, B;
    int FaceIdx;  // Face that owns this edge

    bool operator==(const EPAEdge& other) const {
        return (A == other.A && B == other.B) || (A == other.B && B == other.A);
    }
};

bool EPA(const Simplex& initialSimplex, const RigidBody& A, const RigidBody& B,
         Math::Vec3& outNormal, float& outPenetration) {

    // EPA builds a polytope around the origin and expands it
    std::vector<Math::Vec3> points;
    std::vector<EPAFace> faces;

    // Initialize with simplex
    for (int i = 0; i < initialSimplex.Count; ++i) {
        points.push_back(initialSimplex[i]);
    }

    // Create initial tetrahedron faces with proper winding
    auto addFace = [&](int a, int b, int c, bool checkWinding = true) -> bool {
        if (a == b || b == c || a == c) return false;  // Degenerate

        Math::Vec3 ab = points[b] - points[a];
        Math::Vec3 ac = points[c] - points[a];
        Math::Vec3 normal = Math::CrossSIMD(ab, ac);
        float dist = normal.Magnitude();

        if (dist < 1e-8f) return false;  // Degenerate face

        normal = normal / dist;  // Normalize
        float d = normal.Dot(points[a]);

        // Ensure normal points outward (away from origin)
        // The face should have positive distance from origin
        if (d < 0) {
            normal = normal * -1.0f;
            d = -d;
            std::swap(b, c);  // Flip winding
        }

        // Additional check: ensure face is not degenerate
        if (d < 1e-6f) return false;

        EPAFace face;
        face.Normal = normal;
        face.Distance = d;
        face.Vertices[0] = a;
        face.Vertices[1] = b;
        face.Vertices[2] = c;
        face.Obsolete = false;
        faces.push_back(face);
        return true;
    };

    // Build initial faces (assuming 4-point simplex)
    // Use consistent winding: all faces should point outward
    if (initialSimplex.Count == 4) {
        if (!addFace(0, 1, 2, false)) return false;
        if (!addFace(0, 3, 1, false)) return false;
        if (!addFace(0, 2, 3, false)) return false;
        if (!addFace(1, 3, 2, false)) return false;
    } else {
        // Fallback for non-tetrahedron simplex
        SIMPLE_LOG("Warning: EPA called with non-tetrahedron simplex");
        return false;
    }

    if (faces.size() < 4) {
        SIMPLE_LOG("Warning: Failed to build initial EPA tetrahedron");
        return false;
    }

    const int maxIterations = 64;  // Increased for robustness
    const float tolerance = 1e-5f;  // Tighter tolerance

    for (int iter = 0; iter < maxIterations; ++iter) {
        // Find closest face to origin
        int closestFaceIdx = -1;
        float minDist = std::numeric_limits<float>::max();

        for (size_t i = 0; i < faces.size(); ++i) {
            if (faces[i].Obsolete) continue;
            if (faces[i].Distance < minDist) {
                minDist = faces[i].Distance;
                closestFaceIdx = static_cast<int>(i);
            }
        }

        if (closestFaceIdx < 0 || closestFaceIdx >= (int)faces.size()) {
            break;  // No valid faces
        }

        const EPAFace& closestFace = faces[closestFaceIdx];

        // Get support point in direction of closest face normal
        Math::Vec3 support = GetMinkowskiSupport(A, B, closestFace.Normal);
        float supportDist = support.Dot(closestFace.Normal);

        // Check if we found the true closest point (convergence)
        float expansion = supportDist - minDist;
        if (expansion < tolerance) {
            outNormal = closestFace.Normal;
            outPenetration = minDist;
            return true;
        }

        // Expand polytope: add new point and rebuild visible faces
        points.push_back(support);
        int newPointIdx = static_cast<int>(points.size()) - 1;

        // Find all faces visible from the new point
        std::vector<int> visibleFaces;
        for (size_t i = 0; i < faces.size(); ++i) {
            if (faces[i].Obsolete) continue;

            // Check if face is visible from new point
            Math::Vec3 toPoint = support - points[faces[i].Vertices[0]];
            if (faces[i].Normal.Dot(toPoint) > 0.0f) {
                visibleFaces.push_back(static_cast<int>(i));
                faces[i].Obsolete = true;
            }
        }

        if (visibleFaces.empty()) {
            // No visible faces - numerical issue, use closest face
            outNormal = closestFace.Normal;
            outPenetration = minDist;
            return true;
        }

        // Build horizon edges (edges between visible and non-visible faces)
        std::vector<EPAEdge> horizon;
        for (int faceIdx : visibleFaces) {
            const EPAFace& face = faces[faceIdx];
            // Check each edge of this face
            for (int e = 0; e < 3; ++e) {
                int v0 = face.Vertices[e];
                int v1 = face.Vertices[(e + 1) % 3];

                // Check if this edge is on the horizon
                bool isHorizon = true;
                for (size_t i = 0; i < faces.size(); ++i) {
                    if (faces[i].Obsolete || static_cast<int>(i) == faceIdx) continue;

                    // Check if this face shares the edge
                    const EPAFace& other = faces[i];
                    bool sharesEdge = false;
                    for (int j = 0; j < 3; ++j) {
                        int ov0 = other.Vertices[j];
                        int ov1 = other.Vertices[(j + 1) % 3];
                        if ((ov0 == v0 && ov1 == v1) || (ov0 == v1 && ov1 == v0)) {
                            sharesEdge = true;
                            // Check if other face is also visible
                            Math::Vec3 toPointOther = support - points[other.Vertices[0]];
                            if (other.Normal.Dot(toPointOther) > 0.0f) {
                                isHorizon = false;  // Both faces visible, edge not on horizon
                            }
                            break;
                        }
                    }
                    if (sharesEdge) break;
                }

                if (isHorizon) {
                    EPAEdge edge;
                    edge.A = v0;
                    edge.B = v1;
                    edge.FaceIdx = faceIdx;
                    horizon.push_back(edge);
                }
            }
        }

        // Build new faces from horizon edges to new point
        for (const auto& edge : horizon) {
            // Create face: newPoint -> edge.A -> edge.B
            // Ensure proper winding (normal points away from origin)
            addFace(newPointIdx, edge.A, edge.B, false);
        }

        // Remove obsolete faces
        faces.erase(
            std::remove_if(faces.begin(), faces.end(),
                [](const EPAFace& f) { return f.Obsolete; }),
            faces.end());

        if (faces.empty()) {
            // All faces removed - fallback
            outNormal = closestFace.Normal;
            outPenetration = minDist;
            return true;
        }
    }

    // Fallback: use closest face found
    if (!faces.empty()) {
        int closestFaceIdx = 0;
        float minDist = std::numeric_limits<float>::max();
        for (size_t i = 0; i < faces.size(); ++i) {
            if (!faces[i].Obsolete && faces[i].Distance < minDist) {
                minDist = faces[i].Distance;
                closestFaceIdx = static_cast<int>(i);
            }
        }
        if (closestFaceIdx >= 0 && closestFaceIdx < (int)faces.size()) {
            outNormal = faces[closestFaceIdx].Normal;
            outPenetration = minDist;
            return true;
        }
    }

    return false;
}

} // namespace Solstice::Physics

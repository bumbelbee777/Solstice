#include <Physics/Collision/Narrowphase/GJK.hxx>
#include <Physics/Collision/Narrowphase/ConvexHull.hxx>
#include <Math/Quaternion.hxx>
#include <Math/Matrix.hxx>
#include <Math/SIMDVec3.hxx>
#include <Core/ML/SIMD.hxx>
#include <Core/Debug/Debug.hxx>
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
            // Box support: pick corner with max dot product (SIMD optimized)
            using Solstice::Core::SIMD::Vec4;
            Vec4 dirVec(localDir.x, localDir.y, localDir.z, 0.0f);
            Vec4 extents(rb.HalfExtents.x, rb.HalfExtents.y, rb.HalfExtents.z, 0.0f);

            // Create sign mask: (dir > 0) ? 1.0 : -1.0
            // Use comparison and blend (simplified with scalar for now, but structure allows SIMD)
            Vec4 signs = dirVec; // Will use sign check
            float dirArr[4], extArr[4];
            dirVec.Store(dirArr);
            extents.Store(extArr);

            localSupport.x = (dirArr[0] > 0) ? extArr[0] : -extArr[0];
            localSupport.y = (dirArr[1] > 0) ? extArr[1] : -extArr[1];
            localSupport.z = (dirArr[2] > 0) ? extArr[2] : -extArr[2];
            break;
        }

        case ColliderType::ConvexHull:
            if (rb.Hull) {
                localSupport = rb.Hull->GetSupportPoint(localDir);
            } else {
                localSupport = Math::Vec3(0, 0, 0);
            }
            break;

        case ColliderType::Capsule: {
            // Capsule = sphere-swept line segment along local Y axis
            // Support point is the furthest point on the capsule surface along direction
            float halfHeight = rb.CapsuleHeight * 0.5f;
            float radius = rb.CapsuleRadius;

            // Project direction onto Y axis
            float yProj = localDir.y;
            float xyMag = std::sqrt(localDir.x * localDir.x + localDir.z * localDir.z);

            if (xyMag < 1e-6f) {
                // Direction is along Y axis - return top or bottom cap center
                localSupport = Math::Vec3(0, (yProj > 0) ? (halfHeight + radius) : -(halfHeight + radius), 0);
            } else {
                // Normalize XY direction
                float invXYMag = 1.0f / xyMag;
                Math::Vec3 xyDir(localDir.x * invXYMag, 0, localDir.z * invXYMag);

                // Check if we're in the cylindrical section or caps
                // The cylindrical section extends from -halfHeight to +halfHeight
                // The caps are hemispheres at y = ±halfHeight
                float yAbs = std::abs(yProj);

                if (yAbs * xyMag < halfHeight * xyMag) {
                    // Cylindrical section - support is on the side
                    localSupport = Math::Vec3(xyDir.x * radius, (yProj > 0) ? halfHeight : -halfHeight, xyDir.z * radius);
                } else {
                    // Spherical cap - support is on the hemisphere
                    Math::Vec3 capCenter(0, (yProj > 0) ? halfHeight : -halfHeight, 0);
                    // Direction from cap center to support point
                    Math::Vec3 capDir(localDir.x, localDir.y, localDir.z);
                    capDir = capDir.Normalized();
                    localSupport = capCenter + capDir * radius;
                }
            }
            break;
        }

        case ColliderType::Cylinder: {
            // Oriented cylinder - support point on cylinder surface (caps + side)
            // Cylinder axis is along local Y axis
            float halfHeight = rb.CylinderHeight * 0.5f;
            float radius = rb.CylinderRadius;

            // Project direction onto Y axis
            float yProj = localDir.y;
            float xyMag = std::sqrt(localDir.x * localDir.x + localDir.z * localDir.z);

            if (xyMag < 1e-6f) {
                // Direction is along Y axis - return point on cap edge
                // For a cylinder, when direction is purely vertical, we want a point on the cap edge
                // Use an arbitrary direction in XZ plane (e.g., X axis)
                localSupport = Math::Vec3(radius, (yProj > 0) ? halfHeight : -halfHeight, 0);
            } else {
                // Normalize XY direction for side/cap calculations
                float invXYMag = 1.0f / xyMag;
                Math::Vec3 xyDir(localDir.x * invXYMag, 0, localDir.z * invXYMag);

                // Determine if we're hitting the cap or the side
                // For a cylinder, the support point maximizes d · p where p is on the cylinder surface
                // On the side: p = (r*dx/|dxz|, y, r*dz/|dxz|) with -h/2 ≤ y ≤ h/2
                // Dot product = r*|dxz| + dy*y, maximized when y = sign(dy) * h/2
                // On the cap: p = (r*dx/|dxz|, ±h/2, r*dz/|dxz|)
                // Dot product = r*|dxz| + dy*(±h/2)

                // Compare: side gives r*|dxz| + |dy|*h/2, cap gives r*|dxz| + |dy|*h/2
                // They're equal at the boundary! So we need to check which region we're in.
                // The boundary occurs when the direction's angle from horizontal equals the cap's angle.
                // Actually, the correct check: we're on the cap if the direction vector, when projected
                // onto the cap plane, would hit the cap edge. This happens when |dy|/|dxz| > h/(2r)

                float yRatio = std::abs(yProj) / xyMag;
                float capAngle = halfHeight / radius; // tan of cap angle

                // Use a small bias to prefer side when close to boundary for stability
                const float bias = 0.01f;
                if (yRatio > capAngle + bias) {
                    // Hitting the cap (top or bottom flat disk)
                    // Y is fixed at ±halfHeight, X/Z are on the disk edge
                    localSupport = Math::Vec3(xyDir.x * radius, (yProj > 0) ? halfHeight : -halfHeight, xyDir.z * radius);
                } else {
                    // Hitting the side (vertical cylindrical surface)
                    // Maximize dot product: r*|dxz| + dy*y where y ∈ [-h/2, h/2]
                    // Maximum occurs at y = sign(dy) * h/2
                    float yCoord = (yProj > 0) ? halfHeight : -halfHeight;
                    localSupport = Math::Vec3(xyDir.x * radius, yCoord, xyDir.z * radius);
                }
            }
            break;
        }

        case ColliderType::Tetrahedron:
            // Tetrahedron support: max dot product among up to 4 vertices
            // Use HullVertices if present (expected 4 vertices), otherwise fallback to ConvexHull support
            if (!rb.HullVertices.empty()) {
                // Use SIMD to compute max dot product among vertices
                using Solstice::Core::SIMD::Vec4;
                if (rb.HullVertices.size() >= 4) {
                    // Load 4 vertices
                    Vec4 v0(rb.HullVertices[0].x, rb.HullVertices[0].y, rb.HullVertices[0].z, 0);
                    Vec4 v1(rb.HullVertices[1].x, rb.HullVertices[1].y, rb.HullVertices[1].z, 0);
                    Vec4 v2(rb.HullVertices[2].x, rb.HullVertices[2].y, rb.HullVertices[2].z, 0);
                    Vec4 v3(rb.HullVertices[3].x, rb.HullVertices[3].y, rb.HullVertices[3].z, 0);
                    Vec4 dir(localDir.x, localDir.y, localDir.z, 0);

                    float dots[4];
                    dots[0] = v0.Dot(dir);
                    dots[1] = v1.Dot(dir);
                    dots[2] = v2.Dot(dir);
                    dots[3] = v3.Dot(dir);

                    int maxIdx = 0;
                    float maxDot = dots[0];
                    for (int i = 1; i < 4; ++i) {
                        if (dots[i] > maxDot) {
                            maxDot = dots[i];
                            maxIdx = i;
                        }
                    }
                    localSupport = rb.HullVertices[maxIdx];
                } else {
                    // Fallback to scalar for fewer vertices
                    float maxDot = -1e9f;
                    for (const auto& v : rb.HullVertices) {
                        float dot = v.Dot(localDir);
                        if (dot > maxDot) {
                            maxDot = dot;
                            localSupport = v;
                        }
                    }
                }
            } else if (rb.Hull) {
                localSupport = rb.Hull->GetSupportPoint(localDir);
            } else {
                localSupport = Math::Vec3(0, 0, 0);
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

    const int maxIterations = 32;  // Reduced to prevent long hangs
    const float tolerance = 1e-6f;
    const float convergenceThreshold = 1e-5f; // Threshold for detecting convergence

    // Initialize for convergence detection
    Math::Vec3 prevSupport = support;
    Math::Vec3 prevDirection = direction;
    int oscillationCount = 0;
    float bestDistToOrigin = support.Magnitude();
    int noImprovementCount = 0;

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

        // Track best distance to origin
        if (distToOrigin < bestDistToOrigin) {
            bestDistToOrigin = distToOrigin;
            noImprovementCount = 0;
        } else {
            noImprovementCount++;
            // If we haven't improved in several iterations, we're likely stuck
            if (noImprovementCount > 5 && iter > 5) {
                // No improvement - likely converged or stuck, treat as collision if close
                if (bestDistToOrigin < 0.1f) {
                    return true;
                }
            }
        }

        // Check for convergence: support point hasn't changed significantly
        Math::Vec3 supportDiff = support - prevSupport;
        if (supportDiff.Magnitude() < convergenceThreshold && iter > 2) {
            // Support point converged - likely collision
            return true;
        }

        // Check for oscillation: direction is alternating
        float dirDot = direction.Dot(prevDirection);
        if (dirDot < -0.9f) {
            oscillationCount++;
            if (oscillationCount > 2) {
                // Oscillating - likely numerical issue, treat as collision if close to origin
                if (bestDistToOrigin < 0.1f) {
                    return true;
                }
            }
        } else {
            oscillationCount = 0;
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

        // Store for next iteration
        prevSupport = support;
        prevDirection = direction;
    }

    // Max iterations reached - check if we have a valid simplex
    // Log warning about GJK not converging (can be enabled for debugging)
    // SIMPLE_LOG("Warning: GJK reached max iterations");

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

    const int maxIterations = 32;  // Reduced from 64 to prevent long hangs
    const float tolerance = 1e-5f;  // Tighter tolerance
    const float convergenceThreshold = 1e-4f; // Threshold for detecting convergence

    float prevMinDist = std::numeric_limits<float>::max();
    int noProgressCount = 0;
    Math::Vec3 prevSupport(0, 0, 0);
    int duplicateSupportCount = 0;
    float bestDist = std::numeric_limits<float>::max();
    Math::Vec3 bestNormal(0, 1, 0);

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

        // Check for duplicate support points (numerical stability)
        if (iter > 0) {
            Math::Vec3 supportDiff = support - prevSupport;
            if (supportDiff.Magnitude() < tolerance * 10.0f) {
                duplicateSupportCount++;
                if (duplicateSupportCount > 2) {
                    // Same support point multiple times - converged
                    outNormal = closestFace.Normal;
                    outPenetration = minDist;
                    return true;
                }
            } else {
                duplicateSupportCount = 0;
            }
        }
        prevSupport = support;

        // Check for convergence: distance hasn't changed significantly
        if (iter > 2) {
            float distChange = std::abs(minDist - prevMinDist);
            if (distChange < convergenceThreshold) {
                noProgressCount++;
                if (noProgressCount > 3) {
                    // No progress for several iterations - converged
                    outNormal = closestFace.Normal;
                    outPenetration = minDist;
                    return true;
                }
            } else {
                noProgressCount = 0;
            }
        }
        prevMinDist = minDist;

        // Track best result so far
        if (minDist < bestDist) {
            bestDist = minDist;
            bestNormal = closestFace.Normal;
        }

        // Check if we found the true closest point (convergence)
        float expansion = supportDist - minDist;
        if (expansion < tolerance) {
            outNormal = closestFace.Normal;
            outPenetration = minDist;
            return true;
        }

        // Safety check: if expansion is extremely small or negative, we might be stuck
        if (expansion < tolerance * 0.1f && iter > 5) {
            // Very small expansion - likely converged
            outNormal = closestFace.Normal;
            outPenetration = minDist;
            return true;
        }

        // Early exit if expansion is negative (shouldn't happen, but numerical issues)
        if (expansion < -tolerance && iter > 3) {
            // Negative expansion - numerical error, use best result
            outNormal = bestNormal;
            outPenetration = bestDist;
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

        // Safety check: if we've added too many points, something is wrong
        if (points.size() > 50) {
            // Too many points - likely numerical issue, use best result so far
            outNormal = bestNormal;
            outPenetration = bestDist;
            return true;
        }
    }

    // Max iterations reached - use best result found
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

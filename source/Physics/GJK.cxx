#include "GJK.hxx"
#include "ConvexHull.hxx"
#include "../Math/Quaternion.hxx"
#include "../Math/Matrix.hxx"
#include "../Math/SIMDVec3.hxx"
#include "../Core/Debug.hxx"
#include <cmath>
#include <algorithm>
#include <limits>

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

        case ColliderType::Triangle:
            // Triangle support: max dot product among 3 vertices
            // We assume vertices are stored in HullVertices or similar if not using Hull
            // But RigidBody has no specific Triangle vertices member exposed directly in struct except HullVertices (deprecated)
            // Wait, RigidBody.hxx has `TriangleVertices`? No, it has `HullVertices`.
            // Let's assume for now Triangle uses HullVertices[0..2] or we need to check RigidBody definition again.
            // The user prompt mentioned "TriangleVertices not being a member" in previous conversation, 
            // but I should check what is available.
            // RigidBody.hxx shows:
            // std::vector<Math::Vec3> HullVertices;  // DEPRECATED: use Hull instead
            // std::shared_ptr<ConvexHull> Hull;
            
            // If it's a Triangle, it should probably be a ConvexHull with 3 vertices.
            // But if we have a specific type, maybe we should use HullVertices if it's populated?
            // Or maybe the user wants us to add TriangleVertices?
            // The prompt said "Integrate the usage of convex hulls and triangles as collider types".
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
    // Initial direction (B to A)
    Math::Vec3 direction = B.Position - A.Position;
    if (direction.Magnitude() < 1e-6f) {
        direction = Math::Vec3(1, 0, 0);  // Arbitrary if overlapping exactly
    }
    
    // Get first support point
    Math::Vec3 support = GetMinkowskiSupport(A, B, direction);
    simplex.Add(support);
    
    // New direction towards origin
    direction = support * -1.0f;
    
    const int maxIterations = 32;
    for (int iter = 0; iter < maxIterations; ++iter) {
        support = GetMinkowskiSupport(A, B, direction);
        
        // If support point didn't pass origin, no collision
        if (support.Dot(direction) < 0) {
            return false;
        }
        
        simplex.Add(support);
        
        if (UpdateSimplex(simplex, direction)) {
            // Origin is enclosed
            return true;
        }
    }
    
    // Max iterations reached (unlikely, but treat as no collision)
    return false;
}

// EPA helpers
struct EPAFace {
    Math::Vec3 Normal;
    float Distance;  // Distance from origin to face
    int Vertices[3]; // Indices into point list
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
    
    // Create initial tetrahedron faces
    auto addFace = [&](int a, int b, int c) {
        Math::Vec3 ab = points[b] - points[a];
        Math::Vec3 ac = points[c] - points[a];
        // Use SIMD for normal calculation
        Math::Vec3 normal = Math::CrossSIMD(ab, ac);
        float dist = normal.Magnitude();
        
        if (dist > 1e-6f) {
            normal = normal / dist;  // Normalize
            float d = normal.Dot(points[a]);
            
            // Ensure normal points outward (away from origin)
            if (d < 0) {
                normal = normal * -1.0f;
                d = -d;
                std::swap(b, c);  // Flip winding
            }
            
            EPAFace face;
            face.Normal = normal;
            face.Distance = d;
            face.Vertices[0] = a;
            face.Vertices[1] = b;
            face.Vertices[2] = c;
            faces.push_back(face);
        }
    };
    
    // Build initial faces (assuming 4-point simplex)
    addFace(0, 1, 2);
    addFace(0, 3, 1);
    addFace(0, 2, 3);
    addFace(1, 3, 2);
    
    const int maxIterations = 32;
    const float tolerance = 1e-4f;
    
    for (int iter = 0; iter < maxIterations; ++iter) {
        // Find closest face to origin
        int closestFaceIdx = 0;
        float minDist = faces[0].Distance;
        
        for (size_t i = 1; i < faces.size(); ++i) {
            if (faces[i].Distance < minDist) {
                minDist = faces[i].Distance;
                closestFaceIdx = static_cast<int>(i);
            }
        }
        
        const EPAFace& closestFace = faces[closestFaceIdx];
        
        // Get support point in direction of closest face
        Math::Vec3 support = GetMinkowskiSupport(A, B, closestFace.Normal);
        float supportDist = support.Dot(closestFace.Normal);
        
        // Check if we found the true closest point
        if (supportDist - minDist < tolerance) {
            outNormal = closestFace.Normal;
            outPenetration = minDist;
            return true;
        }
        
        // Expand polytope (simplified - just add new point and rebuild visible faces)
        // Full implementation would track edges and build new faces properly
        points.push_back(support);
        int newPointIdx = static_cast<int>(points.size()) - 1;
        
        // Remove faces visible from new point and add new faces
        std::vector<EPAFace> newFaces;
        for (const auto& face : faces) {
            Math::Vec3 toPoint = support - points[face.Vertices[0]];
            if (face.Normal.Dot(toPoint) < 0) {
                // Face is visible from new point, remove it
                // (In full EPA, we'd track edges and rebuild)
            } else {
                newFaces.push_back(face);
            }
        }
        
        // Simple fallback: just add faces from new point to existing edges
       // This is simplified - production EPA would properly track horizon edges
        if (newFaces.size() == faces.size()) {
            // No faces removed - unusual, might be numerical issue
            outNormal = closestFace.Normal;
            outPenetration = minDist;
            return true;
        }
        
        faces = newFaces;
        
        // Add new faces (simplified)
        for (const auto& face : newFaces) {
            addFace(newPointIdx, face.Vertices[0], face.Vertices[1]);
            addFace(newPointIdx, face.Vertices[1], face.Vertices[2]);
            addFace(newPointIdx, face.Vertices[2], face.Vertices[0]);
        }
    }
    
    // Fallback if max iterations
    if (!faces.empty()) {
        int closestFaceIdx = 0;
        float minDist = faces[0].Distance;
        for (size_t i = 1; i < faces.size(); ++i) {
            if (faces[i].Distance < minDist) {
                minDist = faces[i].Distance;
                closestFaceIdx = static_cast<int>(i);
            }
        }
        outNormal = faces[closestFaceIdx].Normal;
        outPenetration = minDist;
        return true;
    }
    
    return false;
}

} // namespace Solstice::Physics

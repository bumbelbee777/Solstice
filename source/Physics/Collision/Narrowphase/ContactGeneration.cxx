#include <Physics/Collision/Narrowphase/ContactGeneration.hxx>
#include <Physics/Collision/Narrowphase/ShapeHelpers.hxx>
#include <Physics/Collision/Narrowphase/ConvexHull.hxx>
#include <Math/Matrix.hxx>
#include <Core/ML/SIMD.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Physics {
namespace ContactGeneration {

bool GenerateGroundContacts(RigidBody& body, const Math::Vec3& groundNormal, float groundDistance,
                            CollisionResolution::ContactManifold& outManifold) {
    outManifold.BodyA = &body;
    outManifold.BodyB = nullptr; // Ground (static infinite mass)
    outManifold.Friction = body.Friction;
    outManifold.Restitution = body.Restitution;

    if (body.Type == ColliderType::Sphere) {
        float penetration = (groundDistance + body.Radius) - body.Position.y;
        if (penetration > 0.0f) {
            CollisionResolution::ContactPoint contact;
            contact.Position = body.Position - Math::Vec3(0, body.Radius, 0);
            contact.Normal = groundNormal;
            contact.Penetration = penetration;
            outManifold.AddContact(contact);
            return true;
        }
    } else if (body.Type == ColliderType::Box) {
        Math::Vec3 corners[8];
        GetBoxCorners(body.HalfExtents, corners);

        for (int i = 0; i < 8; ++i) {
            Math::Vec3 worldPos = TransformPoint(corners[i], body.Position, body.Rotation);
            float pen = groundDistance - worldPos.y;
            if (pen > 0.0f) {
                CollisionResolution::ContactPoint contact;
                contact.Position = worldPos;
                contact.Normal = groundNormal;
                contact.Penetration = pen;
                outManifold.AddContact(contact);
            }
        }
        return !outManifold.Contacts.empty();
    } else if (body.Type == ColliderType::Tetrahedron) {
        // Special handling for tetrahedron-ground: enforce face snapping and prevent edge balancing
        // Check all vertices to find which ones are below ground
        if (!body.HullVertices.empty() || (body.Hull && !body.Hull->Vertices.empty())) {
            const std::vector<Math::Vec3>* verts = body.HullVertices.empty() ? &body.Hull->Vertices : &body.HullVertices;

            if (verts->size() >= 3) {
                std::vector<Math::Vec3> worldVerts;
                std::vector<int> belowGroundIndices;

                // Transform all vertices to world space and find those below ground
                // MUCH more aggressive threshold to force face contacts
                const float groundThreshold = 0.5f; // Very large threshold to catch all potential face contacts
                for (size_t i = 0; i < verts->size(); ++i) {
                    Math::Vec3 worldPos = TransformPoint((*verts)[i], body.Position, body.Rotation);
                    worldVerts.push_back(worldPos);
                    // Count ANY vertex that's even close to ground level
                    if (worldPos.y <= groundDistance + groundThreshold) {
                        belowGroundIndices.push_back(static_cast<int>(i));
                    }
                }

                // Source/UE3 style: Force face contacts when 2+ vertices are below ground
                // EXTREMELY aggressive - force face contact even with 2 vertices to prevent edge balancing
                if (belowGroundIndices.size() >= 2) {
                    // On a face - generate contacts for all vertices on that face
                    // Use consistent penetration for all face contacts to prevent jitter
                    float maxPenetration = 0.0f;
                    float minPenetration = 1e9f;
                    for (int idx : belowGroundIndices) {
                        float pen = groundDistance - worldVerts[idx].y;
                        if (pen > maxPenetration) {
                            maxPenetration = pen;
                        }
                        if (pen > 0.0f && pen < minPenetration) {
                            minPenetration = pen;
                        }
                    }

                    // Source/UE3: Use average penetration but clamp to prevent excessive corrections
                    float avgPenetration = 0.0f;
                    int validCount = 0;
                    for (int idx : belowGroundIndices) {
                        float pen = groundDistance - worldVerts[idx].y;
                        if (pen > 0.0f) {
                            avgPenetration += pen;
                            validCount++;
                        }
                    }
                    if (validCount > 0) {
                        avgPenetration /= static_cast<float>(validCount);
                    }

                    // UE3: Clamp penetration to reasonable values to prevent sinking
                    const float MAX_TETRA_PENETRATION = 0.05f; // Tighter clamp to prevent sinking
                    avgPenetration = std::min(avgPenetration, MAX_TETRA_PENETRATION);

                    // Force minimum penetration to ensure contact is always valid
                    avgPenetration = std::max(avgPenetration, 0.001f);

                    // Generate contacts for all face vertices with consistent penetration
                    // Use ALL vertices that are below ground, not just 4
                    for (int idx : belowGroundIndices) {
                        if (outManifold.Contacts.size() >= 4) break;
                        CollisionResolution::ContactPoint contact;
                        contact.Position = worldVerts[idx];
                        contact.Normal = groundNormal;
                        contact.Penetration = avgPenetration; // Use consistent penetration
                        outManifold.AddContact(contact);
                    }

                    // Source/UE3: Maximum friction for face contacts to force stability
                    outManifold.Friction = std::max(outManifold.Friction, 1.0f);
                } else if (belowGroundIndices.size() == 1) {
                    // Single vertex - treat as edge case and force it to settle
                    Math::Vec3 v0 = worldVerts[belowGroundIndices[0]];
                    float pen = groundDistance - v0.y;
                    pen = std::min(pen, 0.05f); // Clamp
                    pen = std::max(pen, 0.001f); // Ensure valid

                    CollisionResolution::ContactPoint contact;
                    contact.Position = v0;
                    contact.Normal = groundNormal;
                    contact.Penetration = pen;
                    outManifold.AddContact(contact);

                    // Very high friction to force settling
                    outManifold.Friction = std::max(outManifold.Friction, 0.95f);
                } else if (belowGroundIndices.size() == 0) {
                    // No vertices below - but tetrahedron might still be touching
                    // Check if center is close to ground
                    Math::Vec3 center = body.Position;
                    if (center.y < 0.2f && center.y > -0.1f) {
                        // Force a contact at center to prevent floating
                        CollisionResolution::ContactPoint contact;
                        contact.Position = Math::Vec3(center.x, 0.0f, center.z);
                        contact.Normal = groundNormal;
                        contact.Penetration = 0.01f; // Small penetration to ensure contact
                        outManifold.AddContact(contact);
                        outManifold.Friction = std::max(outManifold.Friction, 0.9f);
                    }
                } else {
                    // Fallback to GJK/EPA if no vertices clearly below ground
                    RigidBody groundPlane;
                    groundPlane.Type = ColliderType::Box;
                    groundPlane.Position = Math::Vec3(0, groundDistance, 0);
                    groundPlane.Rotation = Math::Quaternion(1, 0, 0, 0);
                    groundPlane.HalfExtents = Math::Vec3(1000.0f, 0.01f, 1000.0f);
                    groundPlane.IsStatic = true;

                    Simplex simplex;
                    if (GJK(body, groundPlane, simplex)) {
                        Math::Vec3 normal;
                        float penetration;
                        if (EPA(simplex, body, groundPlane, normal, penetration)) {
                            if (normal.y < 0) normal = normal * -1.0f;
                            Math::Vec3 pBody = GetSupportPoint(body, normal * -1.0f);
                            Math::Vec3 pGround = GetSupportPoint(groundPlane, normal);
                            CollisionResolution::ContactPoint contact;
                            contact.Position = (pBody + pGround) * 0.5f;
                            contact.Normal = groundNormal;
                            contact.Penetration = penetration;
                            outManifold.AddContact(contact);
                        }
                    }
                }
            }
        }
        return !outManifold.Contacts.empty();
    } else if (body.Type == ColliderType::Cylinder) {
        // Source engine style: Special handling for cylinder-ground with better tilt support
        float halfHeight = body.CylinderHeight * 0.5f;
        float radius = body.CylinderRadius;

        // Source engine approach: Sample more points for tilted cylinders
        // Use adaptive sampling based on cylinder orientation
        const int numBaseContacts = 12; // Increased for better tilt coverage
        std::vector<Math::Vec3> contactPositions;
        std::vector<float> penetrations;

        // Get cylinder's local up direction in world space to detect tilt
        Math::Vec3 localUp(0, 1, 0);
        Math::Vec3 worldUp = TransformPoint(localUp, Math::Vec3(0,0,0), body.Rotation);
        float tiltAngle = std::acos(std::max(-1.0f, std::min(1.0f, worldUp.Dot(Math::Vec3(0,1,0)))));
        bool isTilted = tiltAngle > 0.1f; // More than ~6 degrees

        // Sample points around the bottom cap circle
        // For tilted cylinders, also sample along the edge
        for (int i = 0; i < numBaseContacts; ++i) {
            float angle = i * 2.0f * 3.14159f / numBaseContacts;
            Math::Vec3 localOffset(radius * std::cos(angle), -halfHeight, radius * std::sin(angle));
            Math::Vec3 worldPos = TransformPoint(localOffset, body.Position, body.Rotation);
            float penetration = groundDistance - worldPos.y;
            if (penetration > 0.0f) {
                contactPositions.push_back(worldPos);
                penetrations.push_back(penetration);
            }
        }

        // For tilted cylinders, also sample along the cylinder edge
        if (isTilted) {
            const int numEdgeSamples = 6;
            for (int i = 0; i <= numEdgeSamples; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(numEdgeSamples); // 0 to 1
                float y = -halfHeight + t * body.CylinderHeight; // Along cylinder height

                // Sample around the circle at this height
                for (int j = 0; j < 4; ++j) {
                    float angle = j * 2.0f * 3.14159f / 4.0f;
                    Math::Vec3 localOffset(radius * std::cos(angle), y, radius * std::sin(angle));
                    Math::Vec3 worldPos = TransformPoint(localOffset, body.Position, body.Rotation);
                    float penetration = groundDistance - worldPos.y;
                    if (penetration > 0.0f) {
                        contactPositions.push_back(worldPos);
                        penetrations.push_back(penetration);
                    }
                }
            }
        }

        // Also check center of bottom cap
        Math::Vec3 bottomCenterLocal(0, -halfHeight, 0);
        Math::Vec3 bottomCenterWorld = TransformPoint(bottomCenterLocal, body.Position, body.Rotation);
        float centerPenetration = groundDistance - bottomCenterWorld.y;
        if (centerPenetration > 0.0f) {
            contactPositions.push_back(bottomCenterWorld);
            penetrations.push_back(centerPenetration);
        }

        // Source engine style: Use consistent penetration and select best contacts
        if (!contactPositions.empty()) {
            float maxPenetration = 0.0f;
            for (float pen : penetrations) {
                if (pen > maxPenetration) {
                    maxPenetration = pen;
                }
            }

            float avgPenetration = 0.0f;
            for (float pen : penetrations) {
                avgPenetration += pen;
            }
            avgPenetration /= static_cast<float>(penetrations.size());
            avgPenetration = std::max(0.0f, avgPenetration);

            // Source/UE3: More conservative clamping for stability, prevent sinking
            avgPenetration = std::min(avgPenetration, maxPenetration * 0.7f);

            // UE3: Clamp cylinder penetration to prevent sinking into ground
            const float MAX_CYLINDER_PENETRATION = 0.08f; // Maximum allowed penetration
            avgPenetration = std::min(avgPenetration, MAX_CYLINDER_PENETRATION);

            // Select best 4 contacts using Source-style heuristics
            // Prefer contacts with good distribution and deeper penetration
            std::vector<std::pair<size_t, float>> indexedPenetrations;
            for (size_t i = 0; i < penetrations.size(); ++i) {
                indexedPenetrations.push_back({i, penetrations[i]});
            }
            std::sort(indexedPenetrations.begin(), indexedPenetrations.end(),
                [](const std::pair<size_t, float>& a, const std::pair<size_t, float>& b) {
                    return a.second > b.second;
                });

            // Greedy selection: pick contacts that are well-distributed
            std::vector<bool> selected(contactPositions.size(), false);
            std::vector<CollisionResolution::ContactPoint> selectedContacts;

            // Always include deepest contact
            if (!indexedPenetrations.empty()) {
                size_t deepestIdx = indexedPenetrations[0].first;
                CollisionResolution::ContactPoint contact;
                contact.Position = contactPositions[deepestIdx];
                contact.Normal = groundNormal;
                contact.Penetration = avgPenetration;
                selectedContacts.push_back(contact);
                selected[deepestIdx] = true;
            }

            // Select remaining contacts that are furthest from selected ones
            while (selectedContacts.size() < 4 && selectedContacts.size() < contactPositions.size()) {
                float maxMinDist = -1.0f;
                size_t bestIdx = 0;

                for (size_t i = 0; i < contactPositions.size(); ++i) {
                    if (selected[i]) continue;

                    float minDist = 1e9f;
                    for (const auto& selContact : selectedContacts) {
                        Math::Vec3 diff = contactPositions[i] - selContact.Position;
                        float dist = diff.Magnitude();
                        if (dist < minDist) {
                            minDist = dist;
                        }
                    }

                    // Weight by penetration
                    float score = minDist * (1.0f + penetrations[i] * 10.0f);
                    if (score > maxMinDist) {
                        maxMinDist = score;
                        bestIdx = i;
                    }
                }

                if (maxMinDist > 0.0f) {
                    CollisionResolution::ContactPoint contact;
                    contact.Position = contactPositions[bestIdx];
                    contact.Normal = groundNormal;
                    contact.Penetration = avgPenetration;
                    selectedContacts.push_back(contact);
                    selected[bestIdx] = true;
                } else {
                    break;
                }
            }

            // Add selected contacts to manifold
            for (const auto& contact : selectedContacts) {
                outManifold.AddContact(contact);
            }
        }

        // If no contacts generated from base check, use GJK/EPA
        if (outManifold.Contacts.empty()) {
            RigidBody groundPlane;
            groundPlane.Type = ColliderType::Box;
            groundPlane.Position = Math::Vec3(0, groundDistance, 0);
            groundPlane.Rotation = Math::Quaternion(1, 0, 0, 0);
            groundPlane.HalfExtents = Math::Vec3(1000.0f, 0.01f, 1000.0f);
            groundPlane.IsStatic = true;

            Simplex simplex;
            if (GJK(body, groundPlane, simplex)) {
                Math::Vec3 normal;
                float penetration;
                if (EPA(simplex, body, groundPlane, normal, penetration)) {
                    if (penetration <= 0.0f || penetration > 10.0f) return false;

                    Math::Vec3 pBody = GetSupportPoint(body, groundNormal * -1.0f);
                    Math::Vec3 pGround = GetSupportPoint(groundPlane, groundNormal);
                    CollisionResolution::ContactPoint contact;
                    contact.Position = (pBody + pGround) * 0.5f;
                    contact.Normal = groundNormal;
                    contact.Penetration = penetration;
                    outManifold.AddContact(contact);
                }
            }
        }
        return !outManifold.Contacts.empty();
    } else if (body.Type == ColliderType::Capsule ||
               body.Type == ColliderType::ConvexHull || body.Type == ColliderType::Triangle) {
        // Use GJK/EPA for other complex shapes vs ground plane
        RigidBody groundPlane;
        groundPlane.Type = ColliderType::Box;
        groundPlane.Position = Math::Vec3(0, groundDistance, 0);
        groundPlane.Rotation = Math::Quaternion(1, 0, 0, 0);
        groundPlane.HalfExtents = Math::Vec3(1000.0f, 0.01f, 1000.0f);
        groundPlane.IsStatic = true;

        Simplex simplex;
        if (GJK(body, groundPlane, simplex)) {
            Math::Vec3 normal;
            float penetration;
            if (EPA(simplex, body, groundPlane, normal, penetration)) {
                if (penetration <= 0.0f || penetration > 10.0f) return false;

                Math::Vec3 pBody = GetSupportPoint(body, groundNormal * -1.0f);
                Math::Vec3 pGround = GetSupportPoint(groundPlane, groundNormal);

                CollisionResolution::ContactPoint contact;
                contact.Position = (pBody + pGround) * 0.5f;
                contact.Normal = groundNormal;
                contact.Penetration = penetration;
                outManifold.AddContact(contact);
                return true;
            }
        }
    }

    return false;
}

// Helper function for sphere-box collision
static void GenerateSphereBoxContacts(RigidBody* sphere, RigidBody* box, CollisionResolution::ContactManifold& outManifold) {
    Math::Quaternion invRot = box->Rotation.Conjugate();
    Math::Vec3 relPos = sphere->Position - box->Position;
    Math::Matrix4 mInv = invRot.ToMatrix();
    Math::Vec3 localPos;
    localPos.x = mInv.M[0][0]*relPos.x + mInv.M[1][0]*relPos.y + mInv.M[2][0]*relPos.z;
    localPos.y = mInv.M[0][1]*relPos.x + mInv.M[1][1]*relPos.y + mInv.M[2][1]*relPos.z;
    localPos.z = mInv.M[0][2]*relPos.x + mInv.M[1][2]*relPos.y + mInv.M[2][2]*relPos.z;

    Math::Vec3 closest;
    closest.x = (localPos.x < -box->HalfExtents.x) ? -box->HalfExtents.x : ((localPos.x > box->HalfExtents.x) ? box->HalfExtents.x : localPos.x);
    closest.y = (localPos.y < -box->HalfExtents.y) ? -box->HalfExtents.y : ((localPos.y > box->HalfExtents.y) ? box->HalfExtents.y : localPos.y);
    closest.z = (localPos.z < -box->HalfExtents.z) ? -box->HalfExtents.z : ((localPos.z > box->HalfExtents.z) ? box->HalfExtents.z : localPos.z);

    Math::Vec3 delta = localPos - closest;
    float dist2 = delta.Dot(delta);

    if (dist2 > sphere->Radius * sphere->Radius) return;

    float dist = std::sqrt(dist2);
    Math::Vec3 localNormal;
    float penetration;

    if (dist < 1e-6f) {
        float dx = box->HalfExtents.x - std::abs(localPos.x);
        float dy = box->HalfExtents.y - std::abs(localPos.y);
        float dz = box->HalfExtents.z - std::abs(localPos.z);

        if (dx < dy && dx < dz) {
            penetration = dx + sphere->Radius;
            localNormal = Math::Vec3((localPos.x > 0) ? 1.0f : -1.0f, 0, 0);
        } else if (dy < dz) {
            penetration = dy + sphere->Radius;
            localNormal = Math::Vec3(0, (localPos.y > 0) ? 1.0f : -1.0f, 0);
        } else {
            penetration = dz + sphere->Radius;
            localNormal = Math::Vec3(0, 0, (localPos.z > 0) ? 1.0f : -1.0f);
        }
    } else {
        localNormal = delta / dist;
        penetration = sphere->Radius - dist;
    }

    Math::Matrix4 mRot = box->Rotation.ToMatrix();
    Math::Vec3 normal;
    normal.x = mRot.M[0][0]*localNormal.x + mRot.M[1][0]*localNormal.y + mRot.M[2][0]*localNormal.z;
    normal.y = mRot.M[0][1]*localNormal.x + mRot.M[1][1]*localNormal.y + mRot.M[2][1]*localNormal.z;
    normal.z = mRot.M[0][2]*localNormal.x + mRot.M[1][2]*localNormal.y + mRot.M[2][2]*localNormal.z;

    Math::Vec3 contactPoint = sphere->Position - normal * sphere->Radius;

    CollisionResolution::ContactPoint contact;
    contact.Position = contactPoint;
    contact.Normal = normal;
    contact.Penetration = penetration;
    outManifold.AddContact(contact);
}

// Helper function for tetrahedron-box collision with Source-style clustering
static void GenerateTetraBoxContacts(RigidBody* tetra, RigidBody* box, const Math::Vec3& normal, float penetration,
                                     CollisionResolution::ContactManifold& outManifold) {
    // Generate multiple contact points for stability with Source-style approach
    if (!tetra->HullVertices.empty() || (tetra->Hull && !tetra->Hull->Vertices.empty())) {
        const std::vector<Math::Vec3>* verts = tetra->HullVertices.empty() ? &tetra->Hull->Vertices : &tetra->HullVertices;

        // Transform tetrahedron vertices to world space
        std::vector<Math::Vec3> worldVerts;
        for (const auto& v : *verts) {
            worldVerts.push_back(TransformPoint(v, tetra->Position, tetra->Rotation));
        }

        // Find vertices closest to the contact surface
        Math::Vec3 contactPlanePoint = GetSupportPoint(*tetra, normal * -1.0f);

        // Collect candidate contacts (Source engine style: collect all valid candidates first)
        std::vector<CollisionResolution::ContactPoint> candidates;
        const float contactPlaneTolerance = penetration * 0.6f; // Slightly more lenient for better coverage
        const float margin = 0.1f;

        for (const auto& v : worldVerts) {
            // Check if vertex is near contact plane
            Math::Vec3 toVertex = v - contactPlanePoint;
            float distToPlane = toVertex.Dot(normal);

            // If vertex is close to contact plane
            if (std::abs(distToPlane) < contactPlaneTolerance) {
                // Check if this point is inside or near the box
                Math::Quaternion invRot = box->Rotation.Conjugate();
                Math::Vec3 relPos = v - box->Position;
                Math::Matrix4 mInv = invRot.ToMatrix();
                Math::Vec3 localPos;
                localPos.x = mInv.M[0][0]*relPos.x + mInv.M[1][0]*relPos.y + mInv.M[2][0]*relPos.z;
                localPos.y = mInv.M[0][1]*relPos.x + mInv.M[1][1]*relPos.y + mInv.M[2][1]*relPos.z;
                localPos.z = mInv.M[0][2]*relPos.x + mInv.M[1][2]*relPos.y + mInv.M[2][2]*relPos.z;

                // If point is inside or near box bounds, add as candidate
                if (localPos.x >= -box->HalfExtents.x - margin && localPos.x <= box->HalfExtents.x + margin &&
                    localPos.y >= -box->HalfExtents.y - margin && localPos.y <= box->HalfExtents.y + margin &&
                    localPos.z >= -box->HalfExtents.z - margin && localPos.z <= box->HalfExtents.z + margin) {

                    CollisionResolution::ContactPoint contact;
                    contact.Position = v;
                    contact.Normal = normal;
                    contact.Penetration = penetration;
                    candidates.push_back(contact);
                }
            }
        }

        // Source engine style: Select best contacts using distance-based clustering
        // Prefer contacts that are furthest apart for stability
        if (candidates.size() <= 4) {
            // Use all candidates if we have 4 or fewer
            for (const auto& c : candidates) {
                outManifold.AddContact(c);
            }
        } else {
            // Select 4 contacts that are furthest apart (Source engine heuristic)
            std::vector<bool> selected(candidates.size(), false);

            // Always include the contact with deepest penetration
            size_t deepestIdx = 0;
            float deepestPen = candidates[0].Penetration;
            for (size_t i = 1; i < candidates.size(); ++i) {
                if (candidates[i].Penetration > deepestPen) {
                    deepestPen = candidates[i].Penetration;
                    deepestIdx = i;
                }
            }
            outManifold.AddContact(candidates[deepestIdx]);
            selected[deepestIdx] = true;

            // Greedily select remaining contacts that are furthest from selected ones
            while (outManifold.Contacts.size() < 4) {
                float maxMinDist = -1.0f;
                size_t bestIdx = 0;

                for (size_t i = 0; i < candidates.size(); ++i) {
                    if (selected[i]) continue;

                    // Find minimum distance to any selected contact
                    float minDist = 1e9f;
                    for (const auto& selectedContact : outManifold.Contacts) {
                        Math::Vec3 diff = candidates[i].Position - selectedContact.Position;
                        float dist = diff.Magnitude();
                        if (dist < minDist) {
                            minDist = dist;
                        }
                    }

                    // Weight by penetration depth (prefer deeper contacts)
                    float score = minDist * (1.0f + candidates[i].Penetration * 5.0f);
                    if (score > maxMinDist) {
                        maxMinDist = score;
                        bestIdx = i;
                    }
                }

                if (maxMinDist > 0.0f) {
                    outManifold.AddContact(candidates[bestIdx]);
                    selected[bestIdx] = true;
                } else {
                    break; // No more valid contacts
                }
            }
        }
    }

    // If no contacts generated, fall back to single contact
    if (outManifold.Contacts.empty()) {
        Math::Vec3 pA = GetSupportPoint(*tetra, normal * -1.0f);
        Math::Vec3 pB = GetSupportPoint(*box, normal);
        CollisionResolution::ContactPoint contact;
        contact.Position = (pA + pB) * 0.5f;
        contact.Normal = normal;
        contact.Penetration = penetration;
        outManifold.AddContact(contact);
    }
}

bool GenerateBodyContacts(RigidBody* A, RigidBody* B, CollisionResolution::ContactManifold& outManifold) {
    if (!A || !B) return false;
    if (A->IsStatic && B->IsStatic) return false;

    outManifold.BodyA = A;
    outManifold.BodyB = B;
    outManifold.Friction = std::min(A->Friction, B->Friction);
    outManifold.Restitution = std::min(A->Restitution, B->Restitution);

    // Dispatch based on types
    if (A->Type == ColliderType::Sphere && B->Type == ColliderType::Sphere) {
        // Sphere-Sphere (SIMD optimized)
        using Solstice::Core::SIMD::Vec4;
        Vec4 posA(A->Position.x, A->Position.y, A->Position.z, 0.0f);
        Vec4 posB(B->Position.x, B->Position.y, B->Position.z, 0.0f);
        Vec4 delta = posB - posA;

        float dist2 = delta.Dot(delta);
        float r = A->Radius + B->Radius;
        if (dist2 > r*r || dist2 < 1e-12f) return false;

        float dist = std::sqrt(dist2);
        Vec4 nVec = (dist > 1e-6f) ? (delta * (1.0f / dist)) : Vec4(0.0f, 1.0f, 0.0f, 0.0f);
        float nArr[4];
        nVec.Store(nArr);
        Math::Vec3 n(nArr[0], nArr[1], nArr[2]);
        float penetration = r - dist;

        CollisionResolution::ContactPoint contact;
        contact.Position = A->Position + n * (A->Radius - penetration * 0.5f);
        contact.Normal = n;
        contact.Penetration = penetration;
        outManifold.AddContact(contact);
        return true;
    }
    else if (A->Type == ColliderType::Box && B->Type == ColliderType::Box) {
        // Box-Box with SAT
        Math::Vec3 normal;
        float penetration;
        std::vector<CollisionResolution::ContactPoint> contacts;

        if (TestSATBoxVsBox(*A, *B, normal, penetration, contacts)) {
            for (const auto& c : contacts) {
                outManifold.AddContact(c);
            }
            return true;
        }
    }
    else if ((A->Type == ColliderType::Capsule && B->Type == ColliderType::Capsule) ||
             (A->Type == ColliderType::Capsule && B->Type == ColliderType::Sphere) ||
             (A->Type == ColliderType::Sphere && B->Type == ColliderType::Capsule)) {
        // Capsule-Capsule or Capsule-Sphere (use GJK/EPA)
        Simplex simplex;
        if (GJK(*A, *B, simplex)) {
            Math::Vec3 normal;
            float penetration;
            if (EPA(simplex, *A, *B, normal, penetration)) {
                if (penetration <= 0.0f || penetration > 10.0f) return false;

                normal = normal * -1.0f;
                Math::Vec3 pA = GetSupportPoint(*A, normal * -1.0f);
                Math::Vec3 pB = GetSupportPoint(*B, normal);
                CollisionResolution::ContactPoint contact;
                contact.Position = (pA + pB) * 0.5f;
                contact.Normal = normal;
                contact.Penetration = penetration;
                outManifold.AddContact(contact);
                return true;
            }
        }
    }
    else if ((A->Type == ColliderType::Capsule && B->Type == ColliderType::Box) ||
             (A->Type == ColliderType::Box && B->Type == ColliderType::Capsule)) {
        // Capsule-Box (use GJK/EPA)
        Simplex simplex;
        if (GJK(*A, *B, simplex)) {
            Math::Vec3 normal;
            float penetration;
            if (EPA(simplex, *A, *B, normal, penetration)) {
                if (penetration <= 0.0f || penetration > 10.0f) return false;

                normal = normal * -1.0f;
                Math::Vec3 pA = GetSupportPoint(*A, normal * -1.0f);
                Math::Vec3 pB = GetSupportPoint(*B, normal);
                CollisionResolution::ContactPoint contact;
                contact.Position = (pA + pB) * 0.5f;
                contact.Normal = normal;
                contact.Penetration = penetration;
                outManifold.AddContact(contact);
                return true;
            }
        }
    }
    else if (A->Type == ColliderType::Cylinder || B->Type == ColliderType::Cylinder) {
        // Cylinder-* collisions - use simple approach for ALL combinations to prevent freezes
        bool useSimpleApproach = true; // Always use simple approach to prevent GJK/EPA freezes
        if ((A->Type == ColliderType::Cylinder && B->Type == ColliderType::Box) ||
            (A->Type == ColliderType::Box && B->Type == ColliderType::Cylinder) ||
            (A->Type == ColliderType::Cylinder && B->Type == ColliderType::Tetrahedron) ||
            (A->Type == ColliderType::Tetrahedron && B->Type == ColliderType::Cylinder) ||
            (A->Type == ColliderType::Cylinder && B->Type == ColliderType::Sphere) ||
            (A->Type == ColliderType::Sphere && B->Type == ColliderType::Cylinder)) {
            useSimpleApproach = true;
        }

        if (useSimpleApproach) {
            // Approximate cylinder as box for collision detection
            RigidBody* cylinder = (A->Type == ColliderType::Cylinder) ? A : B;
            RigidBody* other = (A == cylinder) ? B : A;

            // Create temporary box approximation of cylinder
            RigidBody tempBox = *cylinder;
            tempBox.Type = ColliderType::Box;
            float r = cylinder->CylinderRadius;
            float h = cylinder->CylinderHeight;
            tempBox.HalfExtents = Math::Vec3(r, h * 0.5f, r);

            if (other->Type == ColliderType::Box) {
                // Box-box collision
                Math::Vec3 normal;
                float penetration;
                std::vector<CollisionResolution::ContactPoint> contacts;
                RigidBody* boxA = (A == cylinder) ? &tempBox : A;
                RigidBody* boxB = (B == cylinder) ? &tempBox : B;

                if (TestSATBoxVsBox(*boxA, *boxB, normal, penetration, contacts)) {
                    for (const auto& c : contacts) {
                        outManifold.AddContact(c);
                    }
                    return true;
                }
            } else if (other->Type == ColliderType::Tetrahedron) {
                // Tetrahedron-box (approximated cylinder)
                RigidBody* tetra = other;
                RigidBody* box = &tempBox;

                Simplex simplex;
                if (GJK(*box, *tetra, simplex)) {
                    Math::Vec3 normal;
                    float penetration;
                    if (EPA(simplex, *box, *tetra, normal, penetration)) {
                        if (penetration <= 0.0f || penetration > 10.0f) return false;

                        normal = normal * -1.0f;
                        GenerateTetraBoxContacts(tetra, box, normal, penetration, outManifold);
                        return true;
                    }
                }
            } else if (other->Type == ColliderType::Sphere) {
                // Sphere-box (approximated cylinder) - use sphere-box collision
                RigidBody* sphere = other;
                RigidBody* box = &tempBox;
                GenerateSphereBoxContacts(sphere, box, outManifold);
                return !outManifold.Contacts.empty();
            }
        } else {
            // Use GJK/EPA for other cylinder combinations
            Simplex simplex;
            if (GJK(*A, *B, simplex)) {
                Math::Vec3 normal;
                float penetration;
                if (EPA(simplex, *A, *B, normal, penetration)) {
                    if (penetration <= 0.0f || penetration > 10.0f) return false;

                    normal = normal * -1.0f;
                    Math::Vec3 pA = GetSupportPoint(*A, normal * -1.0f);
                    Math::Vec3 pB = GetSupportPoint(*B, normal);

                    float contactDist = (pA - pB).Magnitude();
                    if (contactDist > 5.0f) return false;

                    CollisionResolution::ContactPoint contact;
                    contact.Position = (pA + pB) * 0.5f;
                    contact.Normal = normal;
                    contact.Penetration = penetration;
                    outManifold.AddContact(contact);
                    return true;
                }
            }
        }
    }
    else if ((A->Type == ColliderType::Sphere && B->Type == ColliderType::Box) ||
             (A->Type == ColliderType::Box && B->Type == ColliderType::Sphere)) {
        // Sphere-Box
        RigidBody* sphere = (A->Type == ColliderType::Sphere) ? A : B;
        RigidBody* box = (A->Type == ColliderType::Sphere) ? B : A;
        GenerateSphereBoxContacts(sphere, box, outManifold);
        return !outManifold.Contacts.empty();
    }
    else if ((A->Type == ColliderType::Tetrahedron && B->Type == ColliderType::Box) ||
             (A->Type == ColliderType::Box && B->Type == ColliderType::Tetrahedron)) {
        // Tetrahedron-Box: Generate multiple contact points for stability
        RigidBody* tetra = (A->Type == ColliderType::Tetrahedron) ? A : B;
        RigidBody* box = (A->Type == ColliderType::Box) ? A : B;

        Simplex simplex;
        if (GJK(*A, *B, simplex)) {
            Math::Vec3 normal;
            float penetration;
            if (EPA(simplex, *A, *B, normal, penetration)) {
                if (penetration <= 0.0f || penetration > 10.0f) return false;

                normal = normal * -1.0f;
                GenerateTetraBoxContacts(tetra, box, normal, penetration, outManifold);
                return true;
            }
        }
    }
    else {
        // Generic Convex Collision (GJK/EPA)
        Simplex simplex;
        if (GJK(*A, *B, simplex)) {
            Math::Vec3 normal;
            float penetration;
            if (EPA(simplex, *A, *B, normal, penetration)) {
                if (penetration <= 0.0f || penetration > 10.0f) return false;

                normal = normal * -1.0f;
                Math::Vec3 pA = GetSupportPoint(*A, normal * -1.0f);
                Math::Vec3 pB = GetSupportPoint(*B, normal);
                CollisionResolution::ContactPoint contact;
                contact.Position = (pA + pB) * 0.5f;
                contact.Normal = normal;
                contact.Penetration = penetration;
                outManifold.AddContact(contact);
                return true;
            }
        }
    }

    return false;
}

bool TestSATBoxVsBox(const RigidBody& A, const RigidBody& B,
                     Math::Vec3& outNormal, float& outPenetration,
                     std::vector<CollisionResolution::ContactPoint>& outContacts) {
    // Separating Axis Theorem for Oriented Bounding Boxes
    // Test 15 axes: 3 face normals from A, 3 from B, 9 edge-edge combinations

    // Get rotation matrices
    Math::Matrix4 matA = A.Rotation.ToMatrix();
    Math::Matrix4 matB = B.Rotation.ToMatrix();

    // Extract axes
    Math::Vec3 axesA[3] = {
        Math::Vec3(matA.M[0][0], matA.M[0][1], matA.M[0][2]),
        Math::Vec3(matA.M[1][0], matA.M[1][1], matA.M[1][2]),
        Math::Vec3(matA.M[2][0], matA.M[2][1], matA.M[2][2])
    };

    Math::Vec3 axesB[3] = {
        Math::Vec3(matB.M[0][0], matB.M[0][1], matB.M[0][2]),
        Math::Vec3(matB.M[1][0], matB.M[1][1], matB.M[1][2]),
        Math::Vec3(matB.M[2][0], matB.M[2][1], matB.M[2][2])
    };

    Math::Vec3 centerDelta = B.Position - A.Position;

    float minPen = 1e9f;
    Math::Vec3 minAxis;
    int minAxisIndex = -1; // 0-2: A, 3-5: B, 6-14: Edge

    // Helper to test an axis
    auto TestAxis = [&](const Math::Vec3& axis, int index) -> bool {
        float lenSq = axis.Dot(axis);
        if (lenSq < 1e-8f) return true; // Skip near-zero axes

        // Normalize
        float invLen = 1.0f / std::sqrt(lenSq);
        Math::Vec3 normAxis = axis * invLen;

        // Project centers
        float centerProj = centerDelta.Dot(normAxis);

        // Project extents of A
        float projA = std::abs(A.HalfExtents.x * axesA[0].Dot(normAxis)) +
                      std::abs(A.HalfExtents.y * axesA[1].Dot(normAxis)) +
                      std::abs(A.HalfExtents.z * axesA[2].Dot(normAxis));

        // Project extents of B
        float projB = std::abs(B.HalfExtents.x * axesB[0].Dot(normAxis)) +
                      std::abs(B.HalfExtents.y * axesB[1].Dot(normAxis)) +
                      std::abs(B.HalfExtents.z * axesB[2].Dot(normAxis));

        float overlap = projA + projB - std::abs(centerProj);

        if (overlap < 0.0f) {
            return false; // Separating axis found
        }

        // Track minimum penetration
        if (overlap < minPen) {
            minPen = overlap;
            minAxis = (centerProj < 0.0f) ? normAxis : (normAxis * -1.0f);
            minAxisIndex = index;
        }

        return true;
    };

    // Test face normals of A
    for (int i = 0; i < 3; ++i) {
        if (!TestAxis(axesA[i], i)) return false;
    }

    // Test face normals of B
    for (int i = 0; i < 3; ++i) {
        if (!TestAxis(axesB[i], i + 3)) return false;
    }

    // Test edge-edge axes
    int edgeIdx = 6;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Math::Vec3 edgeAxis = axesA[i].Cross(axesB[j]);
            if (!TestAxis(edgeAxis, edgeIdx++)) return false;
        }
    }

    // Collision detected
    outNormal = minAxis;
    outPenetration = minPen;

    // --- Contact Point Generation ---

    // Case 1: Face-Face (Axis is from A or B)
    if (minAxisIndex < 6) {
        const RigidBody* Ref = (minAxisIndex < 3) ? &A : &B;
        const RigidBody* Inc = (minAxisIndex < 3) ? &B : &A;

        Math::Vec3 refNormal = (minAxisIndex < 3) ? (minAxis * -1.0f) : minAxis;

        // Find corners of Incident body
        Math::Vec3 corners[8];
        GetBoxCorners(Inc->HalfExtents, corners);

        // Transform corners to world space
        for (int i = 0; i < 8; ++i) {
            corners[i] = TransformPoint(corners[i], Inc->Position, Inc->Rotation);
        }

        // Find the "deepest" points
        float minProj = 1e9f;
        float projections[8];

        for (int i = 0; i < 8; ++i) {
            projections[i] = corners[i].Dot(refNormal);
            if (projections[i] < minProj) {
                minProj = projections[i];
            }
        }

        // Keep all corners within a small epsilon of the deepest point
        const float epsilon = 0.05f;
        std::vector<std::pair<int, float>> candidates;

        for (int i = 0; i < 8; ++i) {
            if (projections[i] < minProj + epsilon) {
                candidates.push_back({i, projections[i]});
            }
        }

        // Limit to 4 contacts
        if (candidates.size() > 4) {
            candidates.resize(4);
        }

        for (const auto& c : candidates) {
            CollisionResolution::ContactPoint contact;
            contact.Position = corners[c.first];
            contact.Normal = outNormal;
            contact.Penetration = minPen;
            outContacts.push_back(contact);
        }
    }
    // Case 2: Edge-Edge
    else {
        // Fallback to simple center point for edge-edge
        CollisionResolution::ContactPoint contact;
        contact.Position = (A.Position + B.Position) * 0.5f;
        contact.Normal = outNormal;
        contact.Penetration = minPen;
        outContacts.push_back(contact);
    }

    return true;
}

} // namespace ContactGeneration
} // namespace Solstice::Physics

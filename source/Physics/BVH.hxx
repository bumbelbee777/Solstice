#pragma once

#include <Math/Matrix.hxx>
#include <Math/Quaternion.hxx>
#include "RigidBody.hxx"
#include "ConvexHull.hxx"
#include <vector>
#include <algorithm>
#include <cmath>
#include <Core/SIMD.hxx>

namespace Solstice::Physics {

// Helper: Transform point using quaternion rotation (static inline to avoid ODR violations)
static inline Math::Vec3 TransformPointBVH(const Math::Vec3& p, const Math::Vec3& pos, const Math::Quaternion& rot) {
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

struct BVHNode {
    AABB Bounds;
    int LeftChild{-1};  // -1 if leaf
    int RightChild{-1}; // -1 if leaf
    int BodyIndex{-1};  // -1 if internal node

    bool IsLeaf() const { return BodyIndex != -1; }
};

// Morton Code calculation
// Expands 10-bit integer to 30-bit integer by inserting 2 zeros after each bit.
inline uint32_t ExpandBits(uint32_t v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

inline uint32_t CalculateMortonCode(const Math::Vec3& pos, const AABB& worldBounds) {
    // Normalize position to [0, 1] within world bounds
    float x = (pos.x - worldBounds.Min.x) / (worldBounds.Max.x - worldBounds.Min.x);
    float y = (pos.y - worldBounds.Min.y) / (worldBounds.Max.y - worldBounds.Min.y);
    float z = (pos.z - worldBounds.Min.z) / (worldBounds.Max.z - worldBounds.Min.z);

    x = (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x);
    y = (y < 0.0f) ? 0.0f : ((y > 1.0f) ? 1.0f : y);
    z = (z < 0.0f) ? 0.0f : ((z > 1.0f) ? 1.0f : z);

    // Expand to 10 bits (1024 cells)
    uint32_t xx = ExpandBits(static_cast<uint32_t>(x * 1023.0f));
    uint32_t yy = ExpandBits(static_cast<uint32_t>(y * 1023.0f));
    uint32_t zz = ExpandBits(static_cast<uint32_t>(z * 1023.0f));

    return xx | (yy << 1) | (zz << 2);
}

    struct SortEntry {
        uint32_t Code;
        int BodyIndex;
    };

    class BVH {
    public:
        std::vector<BVHNode> Nodes;
        std::vector<int> SortedBodyIndices; // Maps leaf index to original body index if needed, or just store ptrs

        void Build(const std::vector<RigidBody*>& bodies) {
            Nodes.clear();
            if (bodies.empty()) return;

            // 1. Compute World Bounds
            AABB worldBounds;
            worldBounds.Min = Math::Vec3(1e9f, 1e9f, 1e9f);
            worldBounds.Max = Math::Vec3(-1e9f, -1e9f, -1e9f);

            for (const auto* rb : bodies) {
                // Approximate AABB for the body
                Math::Vec3 extents;
                if (rb->Type == ColliderType::Box) {
                    // If rotated, AABB is larger. For now, just use max dimension or rotate corners.
                    // Simple max dimension approximation for Morton code center is fine.
                    // But for Bounds, we need to be conservative.
                    // Let's compute AABB of the rotated box.
                    // Rotated extents = |R * (w,0,0)| + |R * (0,h,0)| + |R * (0,0,d)|
                    // For now, let's just use a safe radius for Morton center, and compute actual AABB for the node.
                    extents = rb->HalfExtents; // This is local.
                    // TODO: Rotate extents.
                    // For Morton code, we use Center.
                } else {
                    extents = Math::Vec3(rb->Radius, rb->Radius, rb->Radius);
                }

                worldBounds.Min.x = std::min(worldBounds.Min.x, rb->Position.x);
                worldBounds.Min.y = std::min(worldBounds.Min.y, rb->Position.y);
                worldBounds.Min.z = std::min(worldBounds.Min.z, rb->Position.z);
                worldBounds.Max.x = std::max(worldBounds.Max.x, rb->Position.x);
                worldBounds.Max.y = std::max(worldBounds.Max.y, rb->Position.y);
                worldBounds.Max.z = std::max(worldBounds.Max.z, rb->Position.z);
            }

            // Add some margin
            worldBounds.Min = worldBounds.Min - Math::Vec3(1,1,1);
            worldBounds.Max = worldBounds.Max + Math::Vec3(1,1,1);

            // 2. Calculate Morton Codes and Sort
            std::vector<SortEntry> entries(bodies.size());

            for (size_t i = 0; i < bodies.size(); ++i) {
                entries[i].Code = CalculateMortonCode(bodies[i]->Position, worldBounds);
                entries[i].BodyIndex = static_cast<int>(i);
            }

            std::sort(entries.begin(), entries.end(), [](const SortEntry& a, const SortEntry& b) {
                return a.Code < b.Code;
            });

            // 3. Build Tree (LBVH)
            // For N primitives, we have 2N-1 nodes.
            Nodes.resize(2 * bodies.size() - 1);

            // Leaf nodes are at indices [N-1, 2N-2]? No, standard LBVH layout is usually internal nodes first or mixed.
            // Let's use a recursive builder on the sorted list for simplicity, or the Karras method.
            // Karras method is parallelizable but recursive is easier to write correctly first.

            BuildRecursive(0, 0, static_cast<int>(bodies.size()) - 1, entries, bodies);
        }

        // Returns index of the node created
        int BuildRecursive(int nodeIdx, int start, int end, const std::vector<SortEntry>& entries, const std::vector<RigidBody*>& bodies) {
            BVHNode& node = Nodes[nodeIdx];

            // Compute AABB for this range
            node.Bounds.Min = Math::Vec3(1e9f, 1e9f, 1e9f);
            node.Bounds.Max = Math::Vec3(-1e9f, -1e9f, -1e9f);

            // Note: Recomputing bounds O(N) at each level is O(N log N) total. Acceptable for small N.
            // For optimization, we could precompute leaf AABBs and merge up.

            if (start == end) {
                // Leaf
                node.LeftChild = -1;
                node.RightChild = -1;
                node.BodyIndex = entries[start].BodyIndex;

                // Compute precise AABB for the leaf body
                const RigidBody* rb = bodies[node.BodyIndex];
                Math::Vec3 center = rb->Position;
                Math::Vec3 halfSize;

                if (rb->Type == ColliderType::Sphere) {
                    halfSize = Math::Vec3(rb->Radius, rb->Radius, rb->Radius);
                } else if (rb->Type == ColliderType::Box) {
                    // Box: Rotate half extents
                    // AABB half-extents = Sum(|R_col * e_local|)
                    Math::Matrix4 rotMat = rb->Rotation.ToMatrix();
                    Math::Vec3 right(rotMat.M[0][0], rotMat.M[0][1], rotMat.M[0][2]);
                    Math::Vec3 up(rotMat.M[1][0], rotMat.M[1][1], rotMat.M[1][2]);
                    Math::Vec3 fwd(rotMat.M[2][0], rotMat.M[2][1], rotMat.M[2][2]);

                    Math::Vec3 he = rb->HalfExtents;
                    halfSize.x = std::abs(right.x)*he.x + std::abs(up.x)*he.y + std::abs(fwd.x)*he.z;
                    halfSize.y = std::abs(right.y)*he.x + std::abs(up.y)*he.y + std::abs(fwd.y)*he.z;
                    halfSize.z = std::abs(right.z)*he.x + std::abs(up.z)*he.y + std::abs(fwd.z)*he.z;
                } else if (rb->Type == ColliderType::Capsule) {
                    // Capsule: AABB = center ± (radius, height/2 + radius, radius) rotated
                    float halfHeight = rb->CapsuleHeight * 0.5f;
                    Math::Vec3 localHalfSize(rb->CapsuleRadius, halfHeight + rb->CapsuleRadius, rb->CapsuleRadius);

                    Math::Matrix4 rotMat = rb->Rotation.ToMatrix();
                    Math::Vec3 right(rotMat.M[0][0], rotMat.M[0][1], rotMat.M[0][2]);
                    Math::Vec3 up(rotMat.M[1][0], rotMat.M[1][1], rotMat.M[1][2]);
                    Math::Vec3 fwd(rotMat.M[2][0], rotMat.M[2][1], rotMat.M[2][2]);

                    halfSize.x = std::abs(right.x)*localHalfSize.x + std::abs(up.x)*localHalfSize.y + std::abs(fwd.x)*localHalfSize.z;
                    halfSize.y = std::abs(right.y)*localHalfSize.x + std::abs(up.y)*localHalfSize.y + std::abs(fwd.y)*localHalfSize.z;
                    halfSize.z = std::abs(right.z)*localHalfSize.x + std::abs(up.z)*localHalfSize.y + std::abs(fwd.z)*localHalfSize.z;
                } else if (rb->Type == ColliderType::Cylinder) {
                    // Cylinder: AABB = center ± (radius, height/2, radius) rotated
                    float halfHeight = rb->CylinderHeight * 0.5f;
                    Math::Vec3 localHalfSize(rb->CylinderRadius, halfHeight, rb->CylinderRadius);

                    Math::Matrix4 rotMat = rb->Rotation.ToMatrix();
                    Math::Vec3 right(rotMat.M[0][0], rotMat.M[0][1], rotMat.M[0][2]);
                    Math::Vec3 up(rotMat.M[1][0], rotMat.M[1][1], rotMat.M[1][2]);
                    Math::Vec3 fwd(rotMat.M[2][0], rotMat.M[2][1], rotMat.M[2][2]);

                    halfSize.x = std::abs(right.x)*localHalfSize.x + std::abs(up.x)*localHalfSize.y + std::abs(fwd.x)*localHalfSize.z;
                    halfSize.y = std::abs(right.y)*localHalfSize.x + std::abs(up.y)*localHalfSize.y + std::abs(fwd.y)*localHalfSize.z;
                    halfSize.z = std::abs(right.z)*localHalfSize.x + std::abs(up.z)*localHalfSize.y + std::abs(fwd.z)*localHalfSize.z;
                } else if (rb->Type == ColliderType::Tetrahedron) {
                    // Tetrahedron: Compute AABB from vertices
                    if (!rb->HullVertices.empty()) {
                        Math::Vec3 minV(1e9f, 1e9f, 1e9f);
                        Math::Vec3 maxV(-1e9f, -1e9f, -1e9f);

                        for (const auto& v : rb->HullVertices) {
                            Math::Vec3 worldV = TransformPointBVH(v, rb->Position, rb->Rotation);
                            minV.x = std::min(minV.x, worldV.x);
                            minV.y = std::min(minV.y, worldV.y);
                            minV.z = std::min(minV.z, worldV.z);
                            maxV.x = std::max(maxV.x, worldV.x);
                            maxV.y = std::max(maxV.y, worldV.y);
                            maxV.z = std::max(maxV.z, worldV.z);
                        }

                        Math::Vec3 center = (minV + maxV) * 0.5f;
                        halfSize = (maxV - minV) * 0.5f;
                        // Adjust center to match rb->Position if needed, but for AABB we use computed center
                        node.Bounds.Min = minV;
                        node.Bounds.Max = maxV;
                        return nodeIdx;
                    } else {
                        // Fallback to sphere approximation
                        halfSize = Math::Vec3(rb->Radius, rb->Radius, rb->Radius);
                    }
                } else {
                    // ConvexHull, Triangle, etc. - use bounding sphere or compute from vertices
                    if (rb->Hull && !rb->Hull->Vertices.empty()) {
                        Math::Vec3 minV(1e9f, 1e9f, 1e9f);
                        Math::Vec3 maxV(-1e9f, -1e9f, -1e9f);

                        for (const auto& v : rb->Hull->Vertices) {
                            Math::Vec3 worldV = TransformPointBVH(v, rb->Position, rb->Rotation);
                            minV.x = std::min(minV.x, worldV.x);
                            minV.y = std::min(minV.y, worldV.y);
                            minV.z = std::min(minV.z, worldV.z);
                            maxV.x = std::max(maxV.x, worldV.x);
                            maxV.y = std::max(maxV.y, worldV.y);
                            maxV.z = std::max(maxV.z, worldV.z);
                        }

                        halfSize = (maxV - minV) * 0.5f;
                        center = (minV + maxV) * 0.5f;
                    } else {
                        // Fallback to sphere approximation
                        halfSize = Math::Vec3(rb->Radius, rb->Radius, rb->Radius);
                    }
                }

                node.Bounds.Min = center - halfSize;
                node.Bounds.Max = center + halfSize;

                return nodeIdx;
            }

            // Internal node
            // Split based on Morton codes (highest differing bit)
            // Or just simple median split since we sorted by Morton code (Spatial Median)
            int split = start + (end - start) / 2;
            // Ideally we use the bit difference to split, but median of sorted Morton codes is a good approximation of spatial median.

            int leftIdx = nodeIdx + 1;
            // Right child index is unknown until left subtree is built.
            // Actually, with a pre-allocated array, we need to manage indices.
            // Let's use a simple counter if we weren't doing parallel.
            // Since we are recursive, we can just append to vector?
            // But I resized it.
            // Let's change to append-based for simplicity.

            // RESTART with append strategy
            return -1; // Placeholder
        }

        // Simplified Append-based Build
        void BuildAppend(const std::vector<RigidBody*>& bodies) {
            Nodes.clear();
            Nodes.reserve(2 * bodies.size());
            if (bodies.empty()) return;

            // 1. World Bounds & 2. Morton Sort (Same as above)
            // ... (Copy paste logic)
             AABB worldBounds;
            worldBounds.Min = Math::Vec3(1e9f, 1e9f, 1e9f);
            worldBounds.Max = Math::Vec3(-1e9f, -1e9f, -1e9f);
            for (const auto* rb : bodies) {
                 worldBounds.Min.x = std::min(worldBounds.Min.x, rb->Position.x);
                 worldBounds.Min.y = std::min(worldBounds.Min.y, rb->Position.y);
                 worldBounds.Min.z = std::min(worldBounds.Min.z, rb->Position.z);
                 worldBounds.Max.x = std::max(worldBounds.Max.x, rb->Position.x);
                 worldBounds.Max.y = std::max(worldBounds.Max.y, rb->Position.y);
                 worldBounds.Max.z = std::max(worldBounds.Max.z, rb->Position.z);
            }
            worldBounds.Min = worldBounds.Min - Math::Vec3(1,1,1);
            worldBounds.Max = worldBounds.Max + Math::Vec3(1,1,1);

            std::vector<SortEntry> entries(bodies.size());
            for (size_t i = 0; i < bodies.size(); ++i) {
                entries[i].Code = CalculateMortonCode(bodies[i]->Position, worldBounds);
                entries[i].BodyIndex = static_cast<int>(i);
            }
            std::sort(entries.begin(), entries.end(), [](const SortEntry& a, const SortEntry& b) { return a.Code < b.Code; });

            BuildRecursiveAppend(0, static_cast<int>(bodies.size()) - 1, entries, bodies);
        }

        int BuildRecursiveAppend(int start, int end, const std::vector<SortEntry>& entries, const std::vector<RigidBody*>& bodies) {
            int nodeIdx = static_cast<int>(Nodes.size());
            Nodes.emplace_back();

            if (start == end) {
                // Leaf
                BVHNode& node = Nodes[nodeIdx];
                node.LeftChild = -1;
                node.RightChild = -1;
                node.BodyIndex = entries[start].BodyIndex;

                const RigidBody* rb = bodies[node.BodyIndex];
                Math::Vec3 center = rb->Position;
                Math::Vec3 halfSize;

                if (rb->Type == ColliderType::Sphere) {
                    halfSize = Math::Vec3(rb->Radius, rb->Radius, rb->Radius);
                } else {
                    Math::Matrix4 rotMat = rb->Rotation.ToMatrix();
                    Math::Vec3 right(rotMat.M[0][0], rotMat.M[0][1], rotMat.M[0][2]);
                    Math::Vec3 up(rotMat.M[1][0], rotMat.M[1][1], rotMat.M[1][2]);
                    Math::Vec3 fwd(rotMat.M[2][0], rotMat.M[2][1], rotMat.M[2][2]);

                    Math::Vec3 he = rb->HalfExtents;
                    halfSize.x = std::abs(right.x)*he.x + std::abs(up.x)*he.y + std::abs(fwd.x)*he.z;
                    halfSize.y = std::abs(right.y)*he.x + std::abs(up.y)*he.y + std::abs(fwd.y)*he.z;
                    halfSize.z = std::abs(right.z)*he.x + std::abs(up.z)*he.y + std::abs(fwd.z)*he.z;
                }
                node.Bounds.Min = center - halfSize;
                node.Bounds.Max = center + halfSize;
                return nodeIdx;
            }

        int split = start + (end - start) / 2;
        int left = BuildRecursiveAppend(start, split, entries, bodies);
        int right = BuildRecursiveAppend(split + 1, end, entries, bodies);

        BVHNode& node = Nodes[nodeIdx]; // Re-fetch because vector might have reallocated
        node.LeftChild = left;
        node.RightChild = right;
        node.BodyIndex = -1;

        // Merge bounds
        const AABB& l = Nodes[left].Bounds;
        const AABB& r = Nodes[right].Bounds;

        node.Bounds.Min.x = std::min(l.Min.x, r.Min.x);
        node.Bounds.Min.y = std::min(l.Min.y, r.Min.y);
        node.Bounds.Min.z = std::min(l.Min.z, r.Min.z);
        node.Bounds.Max.x = std::max(l.Max.x, r.Max.x);
        node.Bounds.Max.y = std::max(l.Max.y, r.Max.y);
        node.Bounds.Max.z = std::max(l.Max.z, r.Max.z);

        return nodeIdx;
    }

    // Query
    template<typename Func>
    void Query(const AABB& queryBounds, Func&& callback) {
        if (Nodes.empty()) return;
        QueryRecursive(0, queryBounds, callback);
    }

    template<typename Func>
    void QueryRecursive(int nodeIdx, const AABB& queryBounds, Func&& callback) {
        const BVHNode& node = Nodes[nodeIdx];
        if (!node.Bounds.Intersects(queryBounds)) return;

        if (node.IsLeaf()) {
            callback(node.BodyIndex);
        } else {
            if (node.LeftChild != -1) QueryRecursive(node.LeftChild, queryBounds, callback);
            if (node.RightChild != -1) QueryRecursive(node.RightChild, queryBounds, callback);
        }
    }

    // Find all pairs
    std::vector<std::pair<int, int>> FindPotentialCollisions() {
        std::vector<std::pair<int, int>> pairs;
        if (Nodes.empty()) return pairs;

        // Self-collision of the tree
        // We can do this by traversing. For each leaf, query the tree? O(N log N).
        // Or traverse node vs node.
        FindPairsRecursive(0, 0, pairs);
        return pairs;
    }

    void FindPairsRecursive(int nodeA, int nodeB, std::vector<std::pair<int, int>>& pairs) {
        if (nodeA == -1 || nodeB == -1) return;

        const BVHNode& A = Nodes[nodeA];
        const BVHNode& B = Nodes[nodeB];

        if (!A.Bounds.Intersects(B.Bounds)) return;

        if (A.IsLeaf() && B.IsLeaf()) {
            if (A.BodyIndex < B.BodyIndex) { // Avoid duplicates
                pairs.emplace_back(A.BodyIndex, B.BodyIndex);
            }
            return;
        }

        if (A.IsLeaf()) {
            FindPairsRecursive(nodeA, B.LeftChild, pairs);
            FindPairsRecursive(nodeA, B.RightChild, pairs);
        } else if (B.IsLeaf()) {
            FindPairsRecursive(A.LeftChild, nodeB, pairs);
            FindPairsRecursive(A.RightChild, nodeB, pairs);
        } else {
            // Both internal
            // Split the larger node or descend both?
            // Standard strategy: Descend the one with larger volume?
            // Simple strategy:
            FindPairsRecursive(A.LeftChild, B.LeftChild, pairs);
            FindPairsRecursive(A.LeftChild, B.RightChild, pairs);
            FindPairsRecursive(A.RightChild, B.LeftChild, pairs);
            FindPairsRecursive(A.RightChild, B.RightChild, pairs);
            // This is O(N^2) in worst case (all overlap).
            // Better:
            // If A == B (self check), then:
            //   Check(Left, Left) -> Self check left
            //   Check(Right, Right) -> Self check right
            //   Check(Left, Right) -> Cross check
        }
    }

    // Optimized Self-Collision
    void FindSelfCollisions(std::vector<std::pair<int, int>>& pairs) {
        if (Nodes.empty()) return;
        CheckNodeSelf(0, pairs);
    }

    void CheckNodeSelf(int nodeIdx, std::vector<std::pair<int, int>>& pairs) {
        const BVHNode& node = Nodes[nodeIdx];
        if (node.IsLeaf()) return;

        // Check children against each other
        CheckNodes(node.LeftChild, node.RightChild, pairs);

        // Recurse
        CheckNodeSelf(node.LeftChild, pairs);
        CheckNodeSelf(node.RightChild, pairs);
    }

    void CheckNodes(int nodeA, int nodeB, std::vector<std::pair<int, int>>& pairs) {
        if (nodeA == -1 || nodeB == -1) return;
        const BVHNode& A = Nodes[nodeA];
        const BVHNode& B = Nodes[nodeB];

        if (!A.Bounds.Intersects(B.Bounds)) return;

        if (A.IsLeaf() && B.IsLeaf()) {
            // Found pair
            int idx1 = A.BodyIndex;
            int idx2 = B.BodyIndex;
            if (idx1 > idx2) std::swap(idx1, idx2);
            pairs.emplace_back(idx1, idx2);
            return;
        }

        if (A.IsLeaf()) {
            CheckNodes(nodeA, B.LeftChild, pairs);
            CheckNodes(nodeA, B.RightChild, pairs);
        } else if (B.IsLeaf()) {
            CheckNodes(A.LeftChild, nodeB, pairs);
            CheckNodes(A.RightChild, nodeB, pairs);
        } else {
            CheckNodes(A.LeftChild, B.LeftChild, pairs);
            CheckNodes(A.LeftChild, B.RightChild, pairs);
            CheckNodes(A.RightChild, B.LeftChild, pairs);
            CheckNodes(A.RightChild, B.RightChild, pairs);
        }
    }
};

}

#pragma once

#include <Math/Vector.hxx>
#include <vector>
#include <memory>
#include <array>

namespace Solstice::Core {

struct OctreeNode {
    Math::Vec3 Min;
    Math::Vec3 Max;
    
    // 8 Children
    std::array<std::unique_ptr<OctreeNode>, 8> Children;
    
    // Data stored in this node (e.g., object IDs)
    std::vector<uint32_t> ObjectIndices;
    
    bool IsLeaf() const { return Children[0] == nullptr; }
    
    OctreeNode(const Math::Vec3& min, const Math::Vec3& max) : Min(min), Max(max) {}
};

class Octree {
public:
    Octree(const Math::Vec3& WorldMin, const Math::Vec3& WorldMax, int MaxDepth = 5)
        : m_MaxDepth(MaxDepth) {
        m_Root = std::make_unique<OctreeNode>(WorldMin, WorldMax);
    }
    
    void Insert(uint32_t ObjectIndex, const Math::Vec3& ObjMin, const Math::Vec3& ObjMax) {
        InsertRecursive(m_Root.get(), ObjectIndex, ObjMin, ObjMax, 0);
    }
    
    // Query objects within a frustum or box (simplified to box for now)
    void Query(const Math::Vec3& BoxMin, const Math::Vec3& BoxMax, std::vector<uint32_t>& Results) const {
        QueryRecursive(m_Root.get(), BoxMin, BoxMax, Results);
    }

private:
    std::unique_ptr<OctreeNode> m_Root;
    int m_MaxDepth;

    bool Intersects(const Math::Vec3& MinA, const Math::Vec3& MaxA, const Math::Vec3& MinB, const Math::Vec3& MaxB) const {
        return (MinA.x <= MaxB.x && MaxA.x >= MinB.x) &&
               (MinA.y <= MaxB.y && MaxA.y >= MinB.y) &&
               (MinA.z <= MaxB.z && MaxA.z >= MinB.z);
    }

    void InsertRecursive(OctreeNode* Node, uint32_t ID, const Math::Vec3& ObjMin, const Math::Vec3& ObjMax, int Depth) {
        if (Depth >= m_MaxDepth) {
            Node->ObjectIndices.push_back(ID);
            return;
        }

        // If not leaf, try to push down to children
        if (!Node->IsLeaf()) {
            bool inserted = false;
            for (auto& child : Node->Children) {
                if (Intersects(child->Min, child->Max, ObjMin, ObjMax)) {
                    InsertRecursive(child.get(), ID, ObjMin, ObjMax, Depth + 1);
                    inserted = true;
                }
            }
            // If object overlaps multiple children or doesn't fit perfectly, store here?
            // For simple octree, we usually store in the smallest node that fully contains it.
            // But here we are doing a loose insertion where it goes into all intersecting nodes.
            return;
        }

        // Split if needed (simple heuristic: always split if capacity reached, but here just split if not at max depth)
        // For simplicity in this skeleton, we only split if we are strictly adding logic for it.
        // Let's implement lazy splitting:
        if (Node->ObjectIndices.size() > 8) { // Capacity threshold
            Split(Node);
            // Re-insert existing objects
            std::vector<uint32_t> oldIndices = std::move(Node->ObjectIndices);
            Node->ObjectIndices.clear();
            for (uint32_t oldID : oldIndices) {
                // We need the object bounds here to re-insert correctly. 
                // This implies we need a lookup for object bounds, or we store them.
                // For this skeleton, we'll just keep them in the parent if we can't look them up.
                // This is a limitation of this simple implementation.
                Node->ObjectIndices.push_back(oldID); 
            }
            // Insert new object
             InsertRecursive(Node, ID, ObjMin, ObjMax, Depth); // Retry
        } else {
            Node->ObjectIndices.push_back(ID);
        }
    }

    void Split(OctreeNode* Node) {
        Math::Vec3 center = (Node->Min + Node->Max) * 0.5f;
        
        // Create 8 children
        for (int i = 0; i < 8; ++i) {
            Math::Vec3 newMin, newMax;
            newMin.x = (i & 1) ? center.x : Node->Min.x;
            newMax.x = (i & 1) ? Node->Max.x : center.x;
            newMin.y = (i & 2) ? center.y : Node->Min.y;
            newMax.y = (i & 2) ? Node->Max.y : center.y;
            newMin.z = (i & 4) ? center.z : Node->Min.z;
            newMax.z = (i & 4) ? Node->Max.z : center.z;
            
            Node->Children[i] = std::make_unique<OctreeNode>(newMin, newMax);
        }
    }

    void QueryRecursive(const OctreeNode* Node, const Math::Vec3& BoxMin, const Math::Vec3& BoxMax, std::vector<uint32_t>& Results) const {
        if (!Intersects(Node->Min, Node->Max, BoxMin, BoxMax)) return;

        for (uint32_t id : Node->ObjectIndices) {
            Results.push_back(id);
        }

        if (!Node->IsLeaf()) {
            for (const auto& child : Node->Children) {
                QueryRecursive(child.get(), BoxMin, BoxMax, Results);
            }
        }
    }
};

} // namespace Solstice::Core
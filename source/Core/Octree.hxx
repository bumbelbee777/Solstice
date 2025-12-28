#pragma once

#include <Math/Vector.hxx>
#include <vector>
#include <memory>
#include <array>
#include <cmath>
#include <algorithm>

namespace Solstice::Core {

struct OctreeNode {
    Math::Vec3 Min;
    Math::Vec3 Max;

    // 8 Children
    std::array<std::unique_ptr<OctreeNode>, 8> Children;

    // Data stored in this node (e.g., object IDs)
    std::vector<uint32_t> ObjectIndices;

    bool IsLeaf() const { return Children[0] == nullptr; }

    OctreeNode(const Math::Vec3& MinParam, const Math::Vec3& MaxParam) : Min(MinParam), Max(MaxParam) {}
};

class Octree {
public:
    Octree(const Math::Vec3& WorldMin, const Math::Vec3& WorldMax, int MaxDepth = 5)
        : MMaxDepth(MaxDepth) {
        MRoot = std::make_unique<OctreeNode>(WorldMin, WorldMax);
    }

    void Insert(uint32_t ObjectIndex, const Math::Vec3& ObjMin, const Math::Vec3& ObjMax) {
        InsertRecursive(MRoot.get(), ObjectIndex, ObjMin, ObjMax, 0);
    }

    // Query objects within a box
    void Query(const Math::Vec3& BoxMin, const Math::Vec3& BoxMax, std::vector<uint32_t>& Results) const {
        QueryRecursive(MRoot.get(), BoxMin, BoxMax, Results);
    }

    // Query objects within a sphere
    void QuerySphere(const Math::Vec3& Center, float Radius, std::vector<uint32_t>& Results) const {
        QuerySphereRecursive(MRoot.get(), Center, Radius, Results);
    }

    // Ray intersection test for the octree structure itself
    bool IntersectRay(const Math::Vec3& Origin, const Math::Vec3& Direction, float& T) const {
        return IntersectRayNode(MRoot.get(), Origin, Direction, T);
    }

private:
    std::unique_ptr<OctreeNode> MRoot;
    int MMaxDepth;

    bool Intersects(const Math::Vec3& MinA, const Math::Vec3& MaxA, const Math::Vec3& MinB, const Math::Vec3& MaxB) const {
        return (MinA.x <= MaxB.x && MaxA.x >= MinB.x) &&
               (MinA.y <= MaxB.y && MaxA.y >= MinB.y) &&
               (MinA.z <= MaxB.z && MaxA.z >= MinB.z);
    }

    bool IntersectsSphere(const Math::Vec3& MinA, const Math::Vec3& MaxA, const Math::Vec3& Center, float Radius) const {
        float D2 = 0;
        if (Center.x < MinA.x) D2 += std::pow(Center.x - MinA.x, 2);
        else if (Center.x > MaxA.x) D2 += std::pow(Center.x - MaxA.x, 2);
        if (Center.y < MinA.y) D2 += std::pow(Center.y - MinA.y, 2);
        else if (Center.y > MaxA.y) D2 += std::pow(Center.y - MaxA.y, 2);
        if (Center.z < MinA.z) D2 += std::pow(Center.z - MinA.z, 2);
        else if (Center.z > MaxA.z) D2 += std::pow(Center.z - MaxA.z, 2);
        return D2 <= std::pow(Radius, 2);
    }

    void InsertRecursive(OctreeNode* Node, uint32_t ID, const Math::Vec3& ObjMin, const Math::Vec3& ObjMax, int Depth) {
        if (Depth >= MMaxDepth) {
            Node->ObjectIndices.push_back(ID);
            return;
        }

        if (!Node->IsLeaf()) {
            for (auto& Child : Node->Children) {
                if (Intersects(Child->Min, Child->Max, ObjMin, ObjMax)) {
                    InsertRecursive(Child.get(), ID, ObjMin, ObjMax, Depth + 1);
                }
            }
            return;
        }

        if (Node->ObjectIndices.size() > 8) {
            Split(Node);
            std::vector<uint32_t> OldIndices = std::move(Node->ObjectIndices);
            Node->ObjectIndices.clear();
            // In this simple implementation, we don't have object bounds for re-insertion.
            // Keeping them in the node for now if they can't be pushed down.
            Node->ObjectIndices = OldIndices;
            InsertRecursive(Node, ID, ObjMin, ObjMax, Depth);
        } else {
            Node->ObjectIndices.push_back(ID);
        }
    }

    void Split(OctreeNode* Node) {
        Math::Vec3 Center = (Node->Min + Node->Max) * 0.5f;
        for (int I = 0; I < 8; ++I) {
            Math::Vec3 NewMin, NewMax;
            NewMin.x = (I & 1) ? Center.x : Node->Min.x;
            NewMax.x = (I & 1) ? Node->Max.x : Center.x;
            NewMin.y = (I & 2) ? Center.y : Node->Min.y;
            NewMax.y = (I & 2) ? Node->Max.y : Center.y;
            NewMin.z = (I & 4) ? Center.z : Node->Min.z;
            NewMax.z = (I & 4) ? Node->Max.z : Center.z;
            Node->Children[I] = std::make_unique<OctreeNode>(NewMin, NewMax);
        }
    }

    void QueryRecursive(const OctreeNode* Node, const Math::Vec3& BoxMin, const Math::Vec3& BoxMax, std::vector<uint32_t>& Results) const {
        if (!Intersects(Node->Min, Node->Max, BoxMin, BoxMax)) return;
        for (uint32_t ID : Node->ObjectIndices) Results.push_back(ID);
        if (!Node->IsLeaf()) {
            for (const auto& Child : Node->Children) QueryRecursive(Child.get(), BoxMin, BoxMax, Results);
        }
    }

    void QuerySphereRecursive(const OctreeNode* Node, const Math::Vec3& Center, float Radius, std::vector<uint32_t>& Results) const {
        if (!IntersectsSphere(Node->Min, Node->Max, Center, Radius)) return;
        for (uint32_t ID : Node->ObjectIndices) Results.push_back(ID);
        if (!Node->IsLeaf()) {
            for (const auto& Child : Node->Children) QuerySphereRecursive(Child.get(), Center, Radius, Results);
        }
    }

    bool IntersectRayNode(const OctreeNode* Node, const Math::Vec3& Origin, const Math::Vec3& Direction, float& T) const {
        float Tmin = -INFINITY, Tmax = INFINITY;
        for (int I = 0; I < 3; ++I) {
            float T1 = (Node->Min[I] - Origin[I]) / Direction[I];
            float T2 = (Node->Max[I] - Origin[I]) / Direction[I];
            Tmin = std::max(Tmin, std::min(T1, T2));
            Tmax = std::min(Tmax, std::max(T1, T2));
        }
        if (Tmax >= Tmin && Tmax >= 0) {
            T = Tmin;
            return true;
        }
        return false;
    }
};

} // namespace Solstice::Core

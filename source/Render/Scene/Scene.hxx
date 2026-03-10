#pragma once

#include <vector>
#include <memory>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Math/Matrix.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Core/Material.hxx>
#include <Render/Scene/Camera.hxx>
#include <Core/BSP.hxx>
#include <Core/Octree.hxx>

namespace Solstice::Render {

// Handle to a scene object
using SceneObjectID = uint32_t;
static constexpr SceneObjectID InvalidObjectID = 0xFFFFFFFF;

// Object type flags
enum ObjectType : uint8_t {
    ObjectType_Static = 0,      // Never moves, can be baked into BSP
    ObjectType_Dynamic = 1,     // Moves frequently
    ObjectType_Skinned = 2,     // Has skeletal animation
    ObjectType_Particle = 3,    // Particle system
};

// Visibility flags
enum VisibilityFlags : uint8_t {
    Visible_None = 0,
    Visible_Camera = 1 << 0,
    Visible_Shadow = 1 << 1,
    Visible_Reflection = 1 << 2,
};

// Structure of Arrays (SoA) for Transform data
struct TransformData {
    std::vector<float> PosX, PosY, PosZ;
    std::vector<float> RotW, RotX, RotY, RotZ;
    std::vector<float> ScaleX, ScaleY, ScaleZ;
    std::vector<bool> IsDirty;

    // Derived data (cached matrices)
    std::vector<Math::Matrix4> LocalMatrices;
    std::vector<Math::Matrix4> WorldMatrices;

    void Resize(size_t Size) {
        PosX.resize(Size); PosY.resize(Size); PosZ.resize(Size);
        RotW.resize(Size); RotX.resize(Size); RotY.resize(Size); RotZ.resize(Size);
        ScaleX.resize(Size); ScaleY.resize(Size); ScaleZ.resize(Size);
        IsDirty.resize(Size, true);
        LocalMatrices.resize(Size);
        WorldMatrices.resize(Size);
    }

    void Reserve(size_t Capacity) {
        PosX.reserve(Capacity); PosY.reserve(Capacity); PosZ.reserve(Capacity);
        RotW.reserve(Capacity); RotX.reserve(Capacity); RotY.reserve(Capacity); RotZ.reserve(Capacity);
        ScaleX.reserve(Capacity); ScaleY.reserve(Capacity); ScaleZ.reserve(Capacity);
        IsDirty.reserve(Capacity);
        LocalMatrices.reserve(Capacity);
        WorldMatrices.reserve(Capacity);
    }
};

// SoA for Axis-Aligned Bounding Boxes
struct BoundingBoxData {
    std::vector<float> MinX, MinY, MinZ;
    std::vector<float> MaxX, MaxY, MaxZ;

    void Resize(size_t Size) {
        MinX.resize(Size); MinY.resize(Size); MinZ.resize(Size);
        MaxX.resize(Size); MaxY.resize(Size); MaxZ.resize(Size);
    }

    void Reserve(size_t Capacity) {
        MinX.reserve(Capacity); MinY.reserve(Capacity); MinZ.reserve(Capacity);
        MaxX.reserve(Capacity); MaxY.reserve(Capacity); MaxZ.reserve(Capacity);
    }

    Math::Vec3 GetMin(size_t Index) const {
        return Math::Vec3(MinX[Index], MinY[Index], MinZ[Index]);
    }

    Math::Vec3 GetMax(size_t Index) const {
        return Math::Vec3(MaxX[Index], MaxY[Index], MaxZ[Index]);
    }

    void Set(size_t Index, const Math::Vec3& Min, const Math::Vec3& Max) {
        MinX[Index] = Min.x; MinY[Index] = Min.y; MinZ[Index] = Min.z;
        MaxX[Index] = Max.x; MaxY[Index] = Max.y; MaxZ[Index] = Max.z;
    }
};

// SoA for Mesh Instances
struct MeshInstanceData {
    std::vector<uint32_t> MeshIDs;
    std::vector<uint32_t> MaterialIDs;
    std::vector<uint8_t> ObjectTypes;
    std::vector<uint8_t> VisibilityFlags;
    std::vector<uint8_t> CurrentLOD;

    void Resize(size_t Size) {
        MeshIDs.resize(Size);
        MaterialIDs.resize(Size);
        ObjectTypes.resize(Size);
        VisibilityFlags.resize(Size);
        CurrentLOD.resize(Size);
    }

    void Reserve(size_t Capacity) {
        MeshIDs.reserve(Capacity);
        MaterialIDs.reserve(Capacity);
        ObjectTypes.reserve(Capacity);
        VisibilityFlags.reserve(Capacity);
        CurrentLOD.reserve(Capacity);
    }
};

// Scene object descriptor
struct SceneObject {
    SceneObjectID ID;
    uint32_t MeshID;
    uint32_t MaterialID;
    ObjectType Type;
    uint8_t VisibilityMask;

    SceneObject() : ID(InvalidObjectID), MeshID(0), MaterialID(0), Type(ObjectType_Static), VisibilityMask(Visible_Camera) {}
};

class Scene {
public:
    Scene() {
        // Initialize with reasonable capacity
        Reserve(1024);

        // Create default octree for dynamic objects
        m_Octree = std::make_unique<Core::Octree>(
            Math::Vec3(-1000, -1000, -1000),
            Math::Vec3(1000, 1000, 1000),
            5 // Max depth
        );
    }

    ~Scene() = default;

    // Object Management
    SceneObjectID AddObject(uint32_t MeshID, const Math::Vec3& Position,
                           const Math::Quaternion& Rotation = Math::Quaternion(),
                           const Math::Vec3& Scale = Math::Vec3(1, 1, 1),
                           ObjectType Type = ObjectType_Static) {
        SceneObjectID ID = static_cast<SceneObjectID>(m_Count++);

        // Resize if needed
        if (m_Count > m_Transforms.PosX.size()) {
            size_t NewSize = m_Transforms.PosX.size() == 0 ? 128 : m_Transforms.PosX.size() * 2;
            Resize(NewSize);
        }

        // Set transform
        SetTransform(ID, Position, Rotation, Scale);

        // Set mesh instance data
        m_MeshInstances.MeshIDs[ID] = MeshID;
        m_MeshInstances.MaterialIDs[ID] = 0; // Default material
        m_MeshInstances.ObjectTypes[ID] = static_cast<uint8_t>(Type);
        m_MeshInstances.VisibilityFlags[ID] = Visible_Camera | Visible_Shadow;
        m_MeshInstances.CurrentLOD[ID] = 0;

        // Update AABB (would need mesh bounds)
        UpdateObjectBounds(ID);

        // Add to spatial structure if dynamic
        if (Type == ObjectType_Dynamic) {
            Math::Vec3 Min = m_AABBs.GetMin(ID);
            Math::Vec3 Max = m_AABBs.GetMax(ID);
            m_Octree->Insert(ID, Min, Max);
        }

        return ID;
    }

    void RemoveObject(SceneObjectID ID) {
        if (ID >= m_Count) return;

        // Swap with last and pop (invalidates IDs but keeps data compact)
        if (ID != m_Count - 1) {
            SwapObjects(ID, m_Count - 1);
        }
        m_Count--;
    }

    // Material Access
    void SetMaterial(SceneObjectID ID, uint32_t MaterialID) {
        if (ID >= m_Count) return;
        m_MeshInstances.MaterialIDs[ID] = MaterialID;
    }

    uint32_t GetMaterial(SceneObjectID ID) const {
        if (ID >= m_Count) return 0;
        return m_MeshInstances.MaterialIDs[ID];
    }

    // Transform Access
    void SetTransform(SceneObjectID ID, const Math::Vec3& Pos,
                     const Math::Quaternion& Rot, const Math::Vec3& Scale) {
        if (ID >= m_Count) return;

        m_Transforms.PosX[ID] = Pos.x;
        m_Transforms.PosY[ID] = Pos.y;
        m_Transforms.PosZ[ID] = Pos.z;

        m_Transforms.RotW[ID] = Rot.w;
        m_Transforms.RotX[ID] = Rot.x;
        m_Transforms.RotY[ID] = Rot.y;
        m_Transforms.RotZ[ID] = Rot.z;

        m_Transforms.ScaleX[ID] = Scale.x;
        m_Transforms.ScaleY[ID] = Scale.y;
        m_Transforms.ScaleZ[ID] = Scale.z;

        m_Transforms.IsDirty[ID] = true;
    }

    void SetPosition(SceneObjectID ID, const Math::Vec3& Pos) {
        if (ID >= m_Count) return;
        m_Transforms.PosX[ID] = Pos.x;
        m_Transforms.PosY[ID] = Pos.y;
        m_Transforms.PosZ[ID] = Pos.z;
        m_Transforms.IsDirty[ID] = true;
    }

    void SetRotation(SceneObjectID ID, const Math::Quaternion& Rot) {
        if (ID >= m_Count) return;
        m_Transforms.RotW[ID] = Rot.w;
        m_Transforms.RotX[ID] = Rot.x;
        m_Transforms.RotY[ID] = Rot.y;
        m_Transforms.RotZ[ID] = Rot.z;
        m_Transforms.IsDirty[ID] = true;
    }

    Math::Vec3 GetPosition(SceneObjectID ID) const {
        if (ID >= m_Count) return Math::Vec3();
        return Math::Vec3(m_Transforms.PosX[ID], m_Transforms.PosY[ID], m_Transforms.PosZ[ID]);
    }

    Math::Quaternion GetRotation(SceneObjectID ID) const {
        if (ID >= m_Count) return Math::Quaternion();
        return Math::Quaternion(m_Transforms.RotW[ID], m_Transforms.RotX[ID],
                               m_Transforms.RotY[ID], m_Transforms.RotZ[ID]);
    }

    Math::Vec3 GetScale(SceneObjectID ID) const {
        if (ID >= m_Count) return Math::Vec3(1, 1, 1);
        return Math::Vec3(m_Transforms.ScaleX[ID], m_Transforms.ScaleY[ID], m_Transforms.ScaleZ[ID]);
    }

    const Math::Matrix4& GetWorldMatrix(SceneObjectID ID) const {
        return m_Transforms.WorldMatrices[ID];
    }

    // Update Logic
    void UpdateTransforms() {
        // SIMD candidate loop
        for (size_t i = 0; i < m_Count; ++i) {
            if (m_Transforms.IsDirty[i]) {
                // Reconstruct matrix: T * R * S
                Math::Vec3 Pos(m_Transforms.PosX[i], m_Transforms.PosY[i], m_Transforms.PosZ[i]);
                Math::Vec3 Scale(m_Transforms.ScaleX[i], m_Transforms.ScaleY[i], m_Transforms.ScaleZ[i]);
                Math::Quaternion Rot(m_Transforms.RotW[i], m_Transforms.RotX[i],
                                    m_Transforms.RotY[i], m_Transforms.RotZ[i]);

                Math::Matrix4 T = Math::Matrix4::Translation(Pos);
                Math::Matrix4 R = Rot.ToMatrix();
                Math::Matrix4 S = Math::Matrix4::Scale(Scale);

                m_Transforms.LocalMatrices[i] = T * R * S;
                m_Transforms.WorldMatrices[i] = m_Transforms.LocalMatrices[i];
                m_Transforms.IsDirty[i] = false;

                // Update AABB
                UpdateObjectBounds(i);
            }
        }
    }

    // Culling and Visibility
    void FrustumCull(const Camera& Cam, std::vector<SceneObjectID>& VisibleObjects, float AspectRatio = 16.0f / 9.0f) {
        VisibleObjects.clear();

        // Get frustum from camera with extended far plane for vast terrain (Phase 7)
        // Far plane extended to 2000.0f to see distant landmarks
        Frustum Frust = Cam.GetFrustum(AspectRatio, Cam.GetZoom(), 0.1f, 2000.0f);

        // Test all objects
        for (size_t i = 0; i < m_Count; ++i) {
            if (!(m_MeshInstances.VisibilityFlags[i] & Visible_Camera)) continue;

            Math::Vec3 Min = m_AABBs.GetMin(i);
            Math::Vec3 Max = m_AABBs.GetMax(i);

            if (Frust.IsBoxVisible(Min, Max)) {
                VisibleObjects.push_back(static_cast<SceneObjectID>(i));
            }
        }
    }

    // LOD Selection
    void UpdateLODs(const Math::Vec3& CameraPos) {
        for (size_t i = 0; i < m_Count; ++i) {
            Math::Vec3 ObjPos(m_Transforms.PosX[i], m_Transforms.PosY[i], m_Transforms.PosZ[i]);
            float Distance = ObjPos.Distance(CameraPos);

            // Enhanced distance-based LOD for vast terrain (Phase 7)
            // Near (0-150 units): detailed, high poly
            // Mid (150-300 units): medium detail
            // Far (300-500+ units): low detail, silhouette-focused
            if (Distance < 150.0f) {
                m_MeshInstances.CurrentLOD[i] = 0; // High detail
            } else if (Distance < 300.0f) {
                m_MeshInstances.CurrentLOD[i] = 1; // Medium detail
            } else if (Distance < 500.0f) {
                m_MeshInstances.CurrentLOD[i] = 2; // Low detail
            } else {
                // Beyond 500 units, don't render (distance culling)
                m_MeshInstances.VisibilityFlags[i] &= ~Visible_Camera;
                continue;
            }
            // Ensure object is visible if within render distance
            m_MeshInstances.VisibilityFlags[i] |= Visible_Camera;
        }
    }

    // Spatial Queries
    void QueryOctree(const Math::Vec3& Min, const Math::Vec3& Max, std::vector<uint32_t>& Results) {
        m_Octree->Query(Min, Max, Results);
    }

    // Accessors
    const TransformData& GetTransforms() const { return m_Transforms; }
    const BoundingBoxData& GetBoundingBoxes() const { return m_AABBs; }
    const MeshInstanceData& GetMeshInstances() const { return m_MeshInstances; }
    size_t GetObjectCount() const { return m_Count; }

    uint32_t GetMeshID(SceneObjectID ID) const {
        if (ID >= m_Count) return 0;
        return m_MeshInstances.MeshIDs[ID];
    }

    // Mesh and Material Libraries
    void SetMeshLibrary(MeshLibrary* Library) { m_MeshLibrary = Library; }
    void SetMaterialLibrary(Core::MaterialLibrary* Library) { m_MaterialLibrary = Library; }

    MeshLibrary* GetMeshLibrary() { return m_MeshLibrary; }
    Core::MaterialLibrary* GetMaterialLibrary() { return m_MaterialLibrary; }

    // BSP for static geometry
    void BuildBSP(const std::vector<Core::Triangle>& StaticGeometry) {
        m_BSP = std::make_unique<Core::BSP>();
        m_BSP->Build(StaticGeometry, 20, 4);
    }

    const Core::BSP* GetBSP() const { return m_BSP.get(); }

private:
    void Reserve(size_t Capacity) {
        m_Transforms.Reserve(Capacity);
        m_AABBs.Reserve(Capacity);
        m_MeshInstances.Reserve(Capacity);
    }

    void Resize(size_t NewSize) {
        m_Transforms.Resize(NewSize);
        m_AABBs.Resize(NewSize);
        m_MeshInstances.Resize(NewSize);
    }

    void SwapObjects(SceneObjectID A, SceneObjectID B) {
        // Swap all SoA data
        std::swap(m_Transforms.PosX[A], m_Transforms.PosX[B]);
        std::swap(m_Transforms.PosY[A], m_Transforms.PosY[B]);
        std::swap(m_Transforms.PosZ[A], m_Transforms.PosZ[B]);
        // ... swap all other fields
        std::swap(m_MeshInstances.MeshIDs[A], m_MeshInstances.MeshIDs[B]);
        std::swap(m_MeshInstances.MaterialIDs[A], m_MeshInstances.MaterialIDs[B]);
        std::swap(m_AABBs.MinX[A], m_AABBs.MinX[B]);
        // etc.
    }

    void UpdateObjectBounds(SceneObjectID ID) {
        // Get mesh bounds and transform them
        if (m_MeshLibrary) {
            Mesh* MeshPtr = m_MeshLibrary->GetMesh(m_MeshInstances.MeshIDs[ID]);
            if (MeshPtr) {
                // Transform mesh bounds by world matrix
                Math::Vec3 MeshMin = MeshPtr->BoundsMin;
                Math::Vec3 MeshMax = MeshPtr->BoundsMax;

                // Simple AABB transform (not perfect but fast)
                const Math::Matrix4& WorldMat = m_Transforms.WorldMatrices[ID];
                Math::Vec3 Center = (MeshMin + MeshMax) * 0.5f;
                Math::Vec3 Extent = (MeshMax - MeshMin) * 0.5f;
                float scaleX = std::fabs(m_Transforms.ScaleX[ID]);
                float scaleY = std::fabs(m_Transforms.ScaleY[ID]);
                float scaleZ = std::fabs(m_Transforms.ScaleZ[ID]);
                float maxScale = std::max(scaleX, std::max(scaleY, scaleZ));
                Extent *= maxScale;

                Math::Vec3 WorldCenter = WorldMat.TransformPoint(Center);

                // Conservative bounds
                float MaxExtent = std::max(std::max(Extent.x, Extent.y), Extent.z);
                Math::Vec3 WorldMin = WorldCenter - Math::Vec3(MaxExtent, MaxExtent, MaxExtent);
                Math::Vec3 WorldMax = WorldCenter + Math::Vec3(MaxExtent, MaxExtent, MaxExtent);

                m_AABBs.Set(ID, WorldMin, WorldMax);
            }
        }
    }

    size_t m_Count{0};
    TransformData m_Transforms;
    BoundingBoxData m_AABBs;
    MeshInstanceData m_MeshInstances;

    // Spatial structures
    std::unique_ptr<Core::BSP> m_BSP;
    std::unique_ptr<Core::Octree> m_Octree;

    // Resource references
    MeshLibrary* m_MeshLibrary{nullptr};
    Core::MaterialLibrary* m_MaterialLibrary{nullptr};
};

} // namespace Solstice::Render

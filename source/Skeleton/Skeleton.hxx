#pragma once

#include <Math/Matrix.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <utility>
#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include <unordered_map>

namespace Solstice::Skeleton {

// Strongly-typed bone identifier
struct BoneID {
    uint32_t Value;

    BoneID() : Value(0xFFFFFFFF) {} // Invalid sentinel
    explicit BoneID(uint32_t Val) : Value(Val) {}

    bool operator==(const BoneID& Other) const { return Value == Other.Value; }
    bool operator!=(const BoneID& Other) const { return Value != Other.Value; }
    bool operator<(const BoneID& Other) const { return Value < Other.Value; }

    [[nodiscard]] bool IsValid() const { return Value != 0xFFFFFFFF; }
};

// Hash for BoneID
inline std::size_t HashBoneID(const BoneID& ID) {
    return std::hash<uint32_t>{}(ID.Value);
}

// Immutable bone data
struct Bone {
    BoneID ID;
    std::string Name;
    BoneID Parent;
    Math::Matrix4 LocalTransform;
    Math::Matrix4 InverseBindMatrix;

    Bone() = default;
    Bone(BoneID ID, std::string Name, BoneID Parent, const Math::Matrix4& LocalTransform, const Math::Matrix4& InverseBindMatrix)
        : ID(ID), Name(std::move(Name)), Parent(Parent), LocalTransform(LocalTransform), InverseBindMatrix(InverseBindMatrix) {}
};

// Immutable skeleton tree structure
class Skeleton {
public:
    Skeleton() = default;

    // Construct from bones
    explicit Skeleton(const std::vector<Bone>& Bones, BoneID RootBone)
        : m_Bones(Bones), m_Root(RootBone) {
        BuildIndex();
    }

    // Find bone by ID
    const Bone* FindBone(BoneID ID) const {
        auto It = m_MBoneIndex.find(ID.Value);
        if (It != m_MBoneIndex.end()) {
            return &m_Bones[It->second];
        }
        return nullptr;
    }

    // Find bone by name
    const Bone* FindBoneByName(const std::string& Name) const {
        auto It = m_MNameIndex.find(Name);
        if (It != m_MNameIndex.end()) {
            return &m_Bones[It->second];
        }
        return nullptr;
    }

    // Get children of a bone
    std::vector<BoneID> GetChildren(BoneID ID) const {
        std::vector<BoneID> Children;
        for (const auto& BoneObj : m_Bones) {
            if (BoneObj.Parent.Value == ID.Value) {
                Children.push_back(BoneObj.ID);
            }
        }
        return Children;
    }

    // Accessors
    const std::vector<Bone>& GetBones() const { return m_Bones; }
    BoneID GetRoot() const { return m_Root; }
    size_t GetBoneCount() const { return m_Bones.size(); }

private:
    void BuildIndex() {
        m_MBoneIndex.clear();
        m_MNameIndex.clear();
        for (size_t I = 0; I < m_Bones.size(); ++I) {
            m_MBoneIndex[m_Bones[I].ID.Value] = static_cast<uint32_t>(I);
            if (!m_Bones[I].Name.empty()) {
                m_MNameIndex[m_Bones[I].Name] = static_cast<uint32_t>(I);
            }
        }
    }

    std::vector<Bone> m_Bones;
    BoneID m_Root;

    mutable std::unordered_map<uint32_t, uint32_t> m_MBoneIndex;
    mutable std::unordered_map<std::string, uint32_t> m_MNameIndex;
};

// Bone transform at a point in time
struct BoneTransform {
    Math::Vec3 Translation;
    Math::Quaternion Rotation;
    Math::Vec3 Scale;

    BoneTransform()
        : Translation(0, 0, 0)
        , Rotation(1, 0, 0, 0)
        , Scale(1, 1, 1) {}

    BoneTransform(const Math::Vec3& T, const Math::Quaternion& R, const Math::Vec3& S)
        : Translation(T), Rotation(R), Scale(S) {}

    [[nodiscard]] Math::Matrix4 ToMatrix() const {
        Math::Matrix4 TMat = Math::Matrix4::Translation(Translation);
        Math::Matrix4 RMat = Rotation.ToMatrix();
        Math::Matrix4 SMat = Math::Matrix4::Scale(Scale);
        return TMat * RMat * SMat;
    }
};

// Pose - bone transforms at a point in time
class Pose {
public:
    Pose() = default;

    // Get transform for a bone (returns identity if not found)
    [[nodiscard]] BoneTransform GetTransform(BoneID ID) const {
        auto It = m_BoneTransformsMap.find(ID);
        if (It != m_BoneTransformsMap.end()) {
            return It->second;
        }
        return BoneTransform{};
    }

    // Get world transform for a bone (accumulates parent transforms)
    [[nodiscard]] Math::Matrix4 GetWorldTransform(BoneID ID, const Skeleton& SkeletonParam) const {
        Math::Matrix4 Result = Math::Matrix4::Identity();

        BoneID CurrentID = ID;
        while (CurrentID.IsValid()) {
            const Bone* BonePtr = SkeletonParam.FindBone(CurrentID);
            if (BonePtr == nullptr) { break; }

            BoneTransform TransformObj = GetTransform(CurrentID);
            Math::Matrix4 LocalMatrix = TransformObj.ToMatrix();
            Result = LocalMatrix * Result;

            CurrentID = BonePtr->Parent;
        }

        return Result;
    }

    // Set transform for a bone
    void SetTransform(BoneID ID, const BoneTransform& TransformObj) {
        m_BoneTransformsMap[ID] = TransformObj;
    }

    [[nodiscard]] const std::map<BoneID, BoneTransform>& GetBoneTransforms() const { return m_BoneTransformsMap; }

private:
    std::map<BoneID, BoneTransform> m_BoneTransformsMap;
};

} // namespace Solstice::Skeleton

// Hash specialization for BoneID
namespace std {
    template<>
    struct hash<Solstice::Skeleton::BoneID> {
        std::size_t operator()(const Solstice::Skeleton::BoneID& Id) const {
            return Solstice::Skeleton::HashBoneID(Id);
        }
    };
}

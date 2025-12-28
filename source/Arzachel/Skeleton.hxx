#pragma once

#include <Math/Matrix.hxx>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace Solstice::Arzachel {

// Strongly-typed bone identifier
struct BoneID {
    uint32_t Value;

    BoneID() : Value(0xFFFFFFFF) {} // Invalid sentinel
    explicit BoneID(uint32_t Val) : Value(Val) {}

    bool operator==(const BoneID& Other) const { return Value == Other.Value; }
    bool operator!=(const BoneID& Other) const { return Value != Other.Value; }
    bool operator<(const BoneID& Other) const { return Value < Other.Value; }

    bool IsValid() const { return Value != 0xFFFFFFFF; }
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

    Bone() : Parent(BoneID{}) {}
    Bone(BoneID ID, const std::string& Name, BoneID Parent, const Math::Matrix4& LocalTransform, const Math::Matrix4& InverseBindMatrix)
        : ID(ID), Name(Name), Parent(Parent), LocalTransform(LocalTransform), InverseBindMatrix(InverseBindMatrix) {}
};

// Immutable skeleton tree structure
class Skeleton {
public:
    Skeleton() : Root(BoneID{}) {}

    // Construct from bones
    explicit Skeleton(const std::vector<Bone>& Bones, BoneID RootBone)
        : Bones(Bones), Root(RootBone) {
        BuildIndex();
    }

    // Find bone by ID
    const Bone* FindBone(BoneID ID) const {
        auto It = MBoneIndex.find(ID.Value);
        if (It != MBoneIndex.end()) {
            return &Bones[It->second];
        }
        return nullptr;
    }

    // Find bone by name
    const Bone* FindBoneByName(const std::string& Name) const {
        auto It = MNameIndex.find(Name);
        if (It != MNameIndex.end()) {
            return &Bones[It->second];
        }
        return nullptr;
    }

    // Get children of a bone
    std::vector<BoneID> GetChildren(BoneID ID) const {
        std::vector<BoneID> Children;
        for (const auto& BoneObj : Bones) {
            if (BoneObj.Parent.Value == ID.Value) {
                Children.push_back(BoneObj.ID);
            }
        }
        return Children;
    }

    // Accessors
    const std::vector<Bone>& GetBones() const { return Bones; }
    BoneID GetRoot() const { return Root; }
    size_t GetBoneCount() const { return Bones.size(); }

private:
    void BuildIndex() {
        MBoneIndex.clear();
        MNameIndex.clear();
        for (size_t I = 0; I < Bones.size(); ++I) {
            MBoneIndex[Bones[I].ID.Value] = static_cast<uint32_t>(I);
            if (!Bones[I].Name.empty()) {
                MNameIndex[Bones[I].Name] = static_cast<uint32_t>(I);
            }
        }
    }

    std::vector<Bone> Bones;
    BoneID Root;

    // Indices for fast lookup
    mutable std::unordered_map<uint32_t, uint32_t> MBoneIndex;
    mutable std::unordered_map<std::string, uint32_t> MNameIndex;
};

} // namespace Solstice::Arzachel

// Hash specialization for BoneID
namespace std {
    template<>
    struct hash<Solstice::Arzachel::BoneID> {
        std::size_t operator()(const Solstice::Arzachel::BoneID& Id) const {
            return Solstice::Arzachel::HashBoneID(Id);
        }
    };
}

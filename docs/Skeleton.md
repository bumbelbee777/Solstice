# Skeleton Module

## Overview

The Skeleton module (`Solstice::Skeleton`) provides generalized skeleton and posing types: bone trees, bone IDs, and poses (bone transforms at a point in time). It is used by Arzachel for animation clips and rigging, and by Core for asset loading (e.g. glTF skins).

## Types

- **BoneID**: Strongly-typed bone identifier (uint32_t; invalid sentinel 0xFFFFFFFF).
- **Bone**: Immutable bone data (ID, Name, Parent, LocalTransform, InverseBindMatrix).
- **Skeleton**: Immutable skeleton tree (construct from bones; FindBone, FindBoneByName, GetChildren, GetBones, GetRoot, GetBoneCount).
- **BoneTransform**: TRS at a point in time (Translation, Rotation, Scale; ToMatrix()).
- **Pose**: Map of bone IDs to BoneTransform; GetTransform, SetTransform, GetWorldTransform(ID, Skeleton), GetBoneTransforms.

## Posing Helpers

- **BlendPoses(A, B, Weight)**: Blend two poses by weight (slerp rotation, lerp translation/scale).
- **SolveIK(CurrentPose, TipBone, TargetWorldPos, Skeleton)**: Placeholder for CCD IK.

## Example

```cpp
using namespace Solstice::Skeleton;

std::vector<Bone> bones = { /* ... */ };
BoneID root(0);
Skeleton skel(bones, root);

Pose pose;
pose.SetTransform(boneId, BoneTransform(translation, rotation, scale));
Math::Matrix4 world = pose.GetWorldTransform(boneId, skel);

// Blend two poses (e.g. from Arzachel animation clips)
Pose mixed = BlendPoses(poseA, poseB, 0.5f);
```

## Integration

- **Arzachel**: AnimationClip evaluates to `Skeleton::Pose`; RiggedMesh, SkinWeights, RiggingOps (MergeSkeletons, RemapWeights) use Skeleton types. See [Arzachel.md](Arzachel.md) Animation & Rigging.
- **Core**: AssetLoader::ConvertSkin returns `std::unique_ptr<Skeleton::Skeleton>`.

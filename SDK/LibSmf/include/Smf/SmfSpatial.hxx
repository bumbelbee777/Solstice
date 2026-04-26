#pragma once

#include "SmfTypes.hxx"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Solstice::Smf {

/// Per-face UV / scale / rotation for BSP face textures (authoring; optional on-disk tail after BPEX slab).
struct SmfBspFaceTextureXform {
    float ShiftU{0.f};
    float ShiftV{0.f};
    float ScaleU{1.f};
    float ScaleV{1.f};
    float RotateDeg{0.f};
};

/// Authoring-time BSP (axis-aligned split planes, indexed children). Not runtime CSG.
/// On disk, the spatial **section** stays at format **1**; optional per-node texture/slab fields follow a **BPEX** tag after the 28-byte core (see `SmfBinary.cxx`).
struct SmfAuthoringBspNode {
    SmfVec3 PlaneNormal{0.f, 1.f, 0.f};
    float PlaneD{0.f};
    int32_t FrontChild{-1};
    int32_t BackChild{-1};
    /// 0xFFFFFFFF = non-leaf / unused leaf slot.
    uint32_t LeafId{0xFFFFFFFFu};
    /// Optional RELIC path or filesystem path for viewport / tooling (front = positive half-space side of the plane).
    std::string FrontTexturePath;
    /// Optional texture for the negative half-space side.
    std::string BackTexturePath;
    /// When set, **SlabMin** / **SlabMax** bound the editor visualization (finite split volume); otherwise the plane is unbounded in the overlay.
    bool SlabValid{false};
    SmfVec3 SlabMin{};
    SmfVec3 SlabMax{};
    /// Optional texture alignment (see ``SmfBspFaceTextureXform``); persisted when non-default via **BXT1** tail.
    bool HasFrontTextureXform{false};
    SmfBspFaceTextureXform FrontTextureXform{};
    bool HasBackTextureXform{false};
    SmfBspFaceTextureXform BackTextureXform{};
    /// When set, author-time tools may skip auto-fit / bulk UV edits (optional **TXL1** tail).
    bool FrontTextureXformLocked{false};
    bool BackTextureXformLocked{false};
};

struct SmfAuthoringBsp {
    std::vector<SmfAuthoringBspNode> Nodes;
    uint32_t RootIndex{0};
};

/// Linear octree nodes (8 children each, -1 = empty).
struct SmfAuthoringOctreeNode {
    SmfVec3 Min{};
    SmfVec3 Max{1.f, 1.f, 1.f};
    std::array<int32_t, 8> Children{};
    uint32_t LeafId{0xFFFFFFFFu};

    SmfAuthoringOctreeNode() { Children.fill(-1); }
};

struct SmfAuthoringOctree {
    std::vector<SmfAuthoringOctreeNode> Nodes;
    uint32_t RootIndex{0};
};

} // namespace Solstice::Smf

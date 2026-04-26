#pragma once

#include <Smf/SmfSpatial.hxx>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Jackhammer::Spatial {

using Solstice::Smf::SmfAuthoringBsp;
using Solstice::Smf::SmfAuthoringBspNode;
using Solstice::Smf::SmfVec3;

Solstice::Smf::SmfVec3 NormalizeSmfVec3(const Solstice::Smf::SmfVec3& v);
void OrthoBasisFromNormal(const Solstice::Smf::SmfVec3& nUnit, Solstice::Smf::SmfVec3& u, Solstice::Smf::SmfVec3& v);

void SmfAabbCanonical(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a, const SmfVec3& b);
bool SmfAabbIntersect(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1, const SmfVec3& b0,
    const SmfVec3& b1);
void SmfAabbUnion(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1, const SmfVec3& b0,
    const SmfVec3& b1);

float BspPlaneSignedAnchorD(const SmfAuthoringBspNode& pl);
void ReflectSmfVec3AcrossBspPlane(SmfVec3& o, const SmfVec3& p, const SmfAuthoringBspNode& pl);
void MirrorSmfAabbAcrossBspPlane(SmfVec3& oMin, SmfVec3& oMax, const SmfVec3& iMin, const SmfVec3& iMax,
    const SmfAuthoringBspNode& mirrorPlane);

void ExpandSmfAabb(SmfVec3& ioMin, SmfVec3& ioMax, float margin);
bool InsetSmfAabb(SmfVec3& ioMin, SmfVec3& ioMax, float margin);
float SmfAabbVolume(const SmfVec3& a0, const SmfVec3& a1);
void SnapSmfAabbToGrid(SmfVec3& ioMin, SmfVec3& ioMax, float grid);

/// Largest face-corridor of A outside B (existing Jackhammer CSG heuristic).
bool SmfAabbLargestFaceStripOutsideB(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1,
    const SmfVec3& b0, const SmfVec3& b1);

/// Partitions **A \\ B** into up to six axis-aligned pieces and returns the **largest** (volume). Disjoint AABB only;
/// interior of the result does not intersect **B**'s interior (epsilon margins). Returns false if empty.
bool SmfAabbBooleanSubtractLargestPiece(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1,
    const SmfVec3& b0, const SmfVec3& b1);

/// **Full** axis-aligned subtract: every non-empty box in the standard six-region partition of **A \\ B** (0–6 pieces).
void SmfAabbBooleanSubtractAllPieces(std::vector<std::pair<SmfVec3, SmfVec3>>& outPieces, const SmfVec3& a0,
    const SmfVec3& a1, const SmfVec3& b0, const SmfVec3& b1);

/// Exact **A ∪ B** as a **small set of AABBs**: all pieces of **A \\ B** plus **B** (order: subtract pieces, then B).
void SmfAabbBooleanUnionAllPieces(std::vector<std::pair<SmfVec3, SmfVec3>>& outPieces, const SmfVec3& a0,
    const SmfVec3& a1, const SmfVec3& b0, const SmfVec3& b1);

/// **Symmetric difference** of two AABBs: (A \ B) ∪ (B \ A) as separate boxes (0–12 pieces for axis-aligned inputs).
void SmfAabbBooleanXorAllPieces(std::vector<std::pair<SmfVec3, SmfVec3>>& outPieces, const SmfVec3& a0, const SmfVec3& a1,
    const SmfVec3& b0, const SmfVec3& b1);

/// If ``PlaneNormal`` is axis-aligned, set ``PlaneD`` to match the corresponding **slab** face (box-brush convention).
void JhSyncAxisAlignedPlaneDFromSlab(SmfAuthoringBspNode& nd);

/// World-space width/height on the BSP face for texture **fit** (axis-aligned slab + plane only). Returns false if not axis-aligned.
bool JhBspSlabFaceWorldSize(const SmfAuthoringBspNode& nd, float& outWidth, float& outHeight);

/// Extrudes an AABB by moving one side: faces **0–5** = +X, −X, +Y, −Y, +Z, −Z (same as box-brush indices).
bool PushSlabFace(SmfVec3& ioMin, SmfVec3& ioMax, int faceIndex, float delta);

SmfAuthoringBspNode MakeJhBspBoxPlaneNode(const SmfVec3& normal, float planeD, const SmfVec3& slabMin,
    const SmfVec3& slabMax, const std::string& frontTexture, const std::string& backTexture);
void BuildJhBspBoxBrush(SmfAuthoringBsp& outBsp, const SmfVec3& a, const SmfVec3& b, const std::string& frontTexture,
    const std::string& backTexture);
bool JhBspIsCanonicalBoxBrush(const SmfAuthoringBsp& b);
void SyncJhBspBoxBrushFromAabb(SmfAuthoringBsp& b, const SmfVec3& a, const SmfVec3& c);

const char* JhBspFaceLabel(const SmfAuthoringBspNode& nd);
SmfVec3 JhAabbCorner(const SmfVec3& a, const SmfVec3& b, int cornerIndex);
void JhSetAabbCorner(SmfVec3& ioMin, SmfVec3& ioMax, int cornerIndex, const SmfVec3& p);

int FindBspParentIndex(const SmfAuthoringBsp& b, int childIdx);
int JhBspSubtreeHeight(const SmfAuthoringBsp& b, int idx, int guard);
bool JhBspGraphContainsDirectedCycle(const SmfAuthoringBsp& b);

std::string JhExportBspNodeText(const SmfAuthoringBspNode& node, int nodeIndex);

/// Moves one face of a canonical six-plane box brush by `delta` world units (positive expands outward on +X/+Y/+Z faces).
/// Returns false if the brush is not canonical or the edit would collapse the box.
bool PushCanonicalBoxBrushFace(SmfAuthoringBsp& b, int faceIndex, float delta);

SmfVec3 SmfDirectionUnit(const SmfVec3& d);

} // namespace Jackhammer::Spatial

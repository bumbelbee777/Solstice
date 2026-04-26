#include "LibUI/Tools/ViewportSpatialPick.hxx"

#include "LibUI/Viewport/ViewportMath.hxx"

namespace LibUI::Tools {

bool PickClosestAxisAlignedBoxAlongRay(float rayOriginX, float rayOriginY, float rayOriginZ, float rayDirX, float rayDirY,
    float rayDirZ, const AxisAlignedBox3* boxes, int boxCount, int& outIndex, float& outT) {
    outIndex = -1;
    outT = 0.f;
    if (!boxes || boxCount <= 0) {
        return false;
    }
    float bestT = 1.0e30f;
    int best = -1;
    for (int i = 0; i < boxCount; ++i) {
        const AxisAlignedBox3& b = boxes[i];
        if (b.minX > b.maxX || b.minY > b.maxY || b.minZ > b.maxZ) {
            continue;
        }
        float th = 0.f;
        if (!LibUI::Viewport::IntersectRayAxisAlignedBox(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ,
                b.minX, b.minY, b.minZ, b.maxX, b.maxY, b.maxZ, th)) {
            continue;
        }
        if (th < bestT) {
            bestT = th;
            best = i;
        }
    }
    if (best < 0) {
        return false;
    }
    outIndex = best;
    outT = bestT;
    return true;
}

} // namespace LibUI::Tools

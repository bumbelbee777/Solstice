#include "JackhammerBspTextureOps.hxx"
#include "JackhammerSpatial.hxx"

#include "LibUI/Viewport/ViewportMath.hxx"

#include <cmath>
#include <cstring>
#include <algorithm>

namespace {

using LibUI::Viewport::Mat4Col;
using Solstice::Smf::SmfAuthoringBspNode;
using Solstice::Smf::SmfBspFaceTextureXform;
using Solstice::Smf::SmfVec3;

SmfVec3 SmfScale(const SmfVec3& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}

float SmfDot3(const SmfVec3& a, const SmfVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

SmfVec3 SmfSub(const SmfVec3& a, const SmfVec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

void CameraRightUpWorld(const float viewCol4x4[16], SmfVec3& rW, SmfVec3& uW) {
    Mat4Col invV{};
    Mat4Col viewM{};
    std::memcpy(viewM.data(), viewCol4x4, 16u * sizeof(float));
    if (!LibUI::Viewport::InverseMat4(viewM, invV)) {
        rW = {1.f, 0.f, 0.f};
        uW = {0.f, 1.f, 0.f};
        return;
    }
    rW = {invV[0], invV[1], invV[2]};
    uW = {invV[4], invV[5], invV[6]};
    const float rl = 1.0f / std::max(1.0e-8f, std::sqrt(SmfDot3(rW, rW)));
    const float ul = 1.0f / std::max(1.0e-8f, std::sqrt(SmfDot3(uW, uW)));
    rW = SmfScale(rW, rl);
    uW = SmfScale(uW, ul);
}

void PlaneTangentBasis(const SmfAuthoringBspNode& node, int faceSide, SmfVec3& nOut, SmfVec3& uOut, SmfVec3& vOut) {
    nOut = Jackhammer::Spatial::NormalizeSmfVec3(node.PlaneNormal);
    if (faceSide != 0) {
        nOut = SmfScale(nOut, -1.f);
    }
    Jackhammer::Spatial::OrthoBasisFromNormal(nOut, uOut, vOut);
}

float DegAtan2YOverXInBasis(const SmfVec3& u, const SmfVec3& v, const SmfVec3& dirOnPlane) {
    const float x = SmfDot3(u, dirOnPlane);
    const float y = SmfDot3(v, dirOnPlane);
    return 180.0f * (1.0f / 3.14159265f) * std::atan2(y, x);
}

} // namespace

namespace Jackhammer::BspTex {

void ApplyTextureAlignToDefaultFace(Solstice::Smf::SmfBspFaceTextureXform& outXf) {
    outXf = {};
    outXf.ScaleU = 1.f;
    outXf.ScaleV = 1.f;
    outXf.RotateDeg = 0.f;
    outXf.ShiftU = 0.5f;
    outXf.ShiftV = 0.5f;
}

void ApplyTextureAlignToWorld(Solstice::Smf::SmfBspFaceTextureXform& outXf, const SmfAuthoringBspNode& node, int faceSide,
    const SmfVec3& worldHintAxis) {
    SmfVec3 n, u, v;
    PlaneTangentBasis(node, faceSide, n, u, v);
    SmfVec3 d = worldHintAxis;
    d = SmfSub(d, SmfScale(n, SmfDot3(d, n)));
    const float dl = std::max(1.0e-8f, std::sqrt(SmfDot3(d, d)));
    d = SmfScale(d, 1.0f / dl);
    outXf.RotateDeg = DegAtan2YOverXInBasis(u, v, d);
    if (!std::isfinite(outXf.RotateDeg)) {
        outXf.RotateDeg = 0.f;
    }
    // Preserve scale/shift: user may want only rotation change from align — keep current shift if already used
    if (outXf.ScaleU < 1.0e-6f) {
        outXf.ScaleU = 1.f;
    }
    if (outXf.ScaleV < 1.0e-6f) {
        outXf.ScaleV = 1.f;
    }
}

void ApplyTextureAlignToView(Solstice::Smf::SmfBspFaceTextureXform& outXf, const SmfAuthoringBspNode& node, int faceSide,
    const float viewColMajor4x4[16]) {
    SmfVec3 n, u, v;
    PlaneTangentBasis(node, faceSide, n, u, v);
    SmfVec3 rW, sW;
    CameraRightUpWorld(viewColMajor4x4, rW, sW);
    rW = SmfSub(rW, SmfScale(n, SmfDot3(rW, n)));
    sW = SmfSub(sW, SmfScale(n, SmfDot3(sW, n)));
    // Blend screen "horizontal" and "vertical" in plane for stable axis
    SmfVec3 d = rW;
    const float rl = std::max(1.0e-8f, std::sqrt(SmfDot3(d, d)));
    d = SmfScale(d, 1.0f / rl);
    outXf.RotateDeg = DegAtan2YOverXInBasis(u, v, d);
    if (!std::isfinite(outXf.RotateDeg)) {
        outXf.RotateDeg = 0.f;
    }
    if (outXf.ScaleU < 1.0e-6f) {
        outXf.ScaleU = 1.f;
    }
    if (outXf.ScaleV < 1.0e-6f) {
        outXf.ScaleV = 1.f;
    }
}

} // namespace Jackhammer::BspTex

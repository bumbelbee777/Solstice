#include "LibUI/Viewport/ViewportMath.hxx"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace LibUI::Viewport {

static void SetIdentity(Mat4Col& m) {
    m.fill(0.f);
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

static void LookAtRHColMajor(float eyeX, float eyeY, float eyeZ, float cx, float cy, float cz, float upX, float upY,
    float upZ, Mat4Col& out) {
    float fx = cx - eyeX;
    float fy = cy - eyeY;
    float fz = cz - eyeZ;
    float len = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (len < 1e-6f) {
        SetIdentity(out);
        return;
    }
    fx /= len;
    fy /= len;
    fz /= len;
    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    len = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (len > 1e-6f) {
        sx /= len;
        sy /= len;
        sz /= len;
    }
    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    // Column-major: columns are right, up, -forward, translation
    out[0] = sx;
    out[1] = ux;
    out[2] = -fx;
    out[3] = 0.f;
    out[4] = sy;
    out[5] = uy;
    out[6] = -fy;
    out[7] = 0.f;
    out[8] = sz;
    out[9] = uz;
    out[10] = -fz;
    out[11] = 0.f;
    out[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    out[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    out[14] = -(-fx * eyeX + -fy * eyeY + -fz * eyeZ);
    out[15] = 1.f;
}

static void PerspectiveRHColMajor(float fovYRad, float aspect, float zNear, float zFar, Mat4Col& out) {
    float tanHalf = std::tan(fovYRad * 0.5f);
    float n = zNear;
    float f = zFar;
    float range = f - n;
    if (range <= 1e-6f || aspect <= 1e-6f) {
        SetIdentity(out);
        return;
    }
    float a = 1.f / (tanHalf * aspect);
    float b = 1.f / tanHalf;
    float c = -(f + n) / range;
    float d = -(2.f * f * n) / range;
    out.fill(0.f);
    out[0] = a;
    out[5] = b;
    out[10] = c;
    out[11] = -1.f;
    out[14] = d;
    out[15] = 0.f;
}

static void OrthographicRHColMajor(float left, float right, float bottom, float top, float zNear, float zFar,
    Mat4Col& out) {
    const float rl = right - left;
    const float tb = top - bottom;
    const float fn = zFar - zNear;
    if (std::fabs(rl) < 1e-8f || std::fabs(tb) < 1e-8f || std::fabs(fn) < 1e-8f) {
        SetIdentity(out);
        return;
    }
    out.fill(0.f);
    out[0] = 2.f / rl;
    out[5] = 2.f / tb;
    out[10] = -2.f / fn;
    out[12] = -(right + left) / rl;
    out[13] = -(top + bottom) / tb;
    out[14] = -(zFar + zNear) / fn;
    out[15] = 1.f;
}

Mat4Col MultiplyMat4(const Mat4Col& a, const Mat4Col& b) {
    Mat4Col r{};
    for (int c = 0; c < 4; ++c) {
        for (int ridx = 0; ridx < 4; ++ridx) {
            float s = 0.f;
            for (int k = 0; k < 4; ++k) {
                s += a[k * 4 + ridx] * b[c * 4 + k];
            }
            r[c * 4 + ridx] = s;
        }
    }
    return r;
}

void TransformPoint4(const Mat4Col& m, float x, float y, float z, float w, float& ox, float& oy, float& oz, float& ow) {
    ox = m[0] * x + m[4] * y + m[8] * z + m[12] * w;
    oy = m[1] * x + m[5] * y + m[9] * z + m[13] * w;
    oz = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
    ow = m[3] * x + m[7] * y + m[11] * z + m[15] * w;
}

static void ComputeOrbitOrthoViewProjectionColMajor(OrbitProjectionMode mode, const OrbitPanZoomState& state,
    float targetX, float targetY, float targetZ, float aspect, float zNear, float zFar, Mat4Col& outView,
    Mat4Col& outProj) {
    (void)zNear;
    (void)zFar;
    const float cx = targetX + state.pan_x;
    const float cy = targetY + state.pan_y;
    const float cz = targetZ;
    const float he = std::max(state.distance, 1e-4f);
    const float asp = std::max(aspect, 1e-4f);
    const float halfW = he * asp;
    const float halfH = he;

    float eyeX = cx;
    float eyeY = cy;
    float eyeZ = cz;
    float upX = 0.f;
    float upY = 1.f;
    float upZ = 0.f;

    if (mode == OrbitProjectionMode::OrthoTop) {
        eyeY = cy + 1000.f;
        upX = 0.f;
        upY = 0.f;
        upZ = 1.f;
    } else if (mode == OrbitProjectionMode::OrthoFront) {
        eyeZ = cz + 1000.f;
        upX = 0.f;
        upY = 1.f;
        upZ = 0.f;
    } else if (mode == OrbitProjectionMode::OrthoSide) {
        eyeX = cx + 1000.f;
        upX = 0.f;
        upY = 1.f;
        upZ = 0.f;
    }

    LookAtRHColMajor(eyeX, eyeY, eyeZ, cx, cy, cz, upX, upY, upZ, outView);

    struct W {
        float x, y, z;
    };
    W corners[4]{};
    if (mode == OrbitProjectionMode::OrthoTop) {
        corners[0] = {cx - halfW, cy, cz - halfH};
        corners[1] = {cx + halfW, cy, cz - halfH};
        corners[2] = {cx - halfW, cy, cz + halfH};
        corners[3] = {cx + halfW, cy, cz + halfH};
    } else if (mode == OrbitProjectionMode::OrthoFront) {
        corners[0] = {cx - halfW, cy - halfH, cz};
        corners[1] = {cx + halfW, cy - halfH, cz};
        corners[2] = {cx - halfW, cy + halfH, cz};
        corners[3] = {cx + halfW, cy + halfH, cz};
    } else {
        corners[0] = {cx, cy - halfH, cz - halfW};
        corners[1] = {cx, cy + halfH, cz - halfW};
        corners[2] = {cx, cy - halfH, cz + halfW};
        corners[3] = {cx, cy + halfH, cz + halfW};
    }

    float minvx = 1e30f;
    float maxvx = -1e30f;
    float minvy = 1e30f;
    float maxvy = -1e30f;
    float minvz = 1e30f;
    float maxvz = -1e30f;

    for (int i = 0; i < 4; ++i) {
        float vx, vy, vz, vw;
        TransformPoint4(outView, corners[i].x, corners[i].y, corners[i].z, 1.f, vx, vy, vz, vw);
        if (std::fabs(vw) > 1e-8f) {
            vx /= vw;
            vy /= vw;
            vz /= vw;
        }
        minvx = std::min(minvx, vx);
        maxvx = std::max(maxvx, vx);
        minvy = std::min(minvy, vy);
        maxvy = std::max(maxvy, vy);
        minvz = std::min(minvz, vz);
        maxvz = std::max(maxvz, vz);
    }

    const float depthPad = 2048.f;
    W depthA{};
    W depthB{};
    if (mode == OrbitProjectionMode::OrthoTop) {
        depthA = {cx, cy - depthPad, cz};
        depthB = {cx, cy + depthPad, cz};
    } else if (mode == OrbitProjectionMode::OrthoFront) {
        depthA = {cx, cy, cz - depthPad};
        depthB = {cx, cy, cz + depthPad};
    } else {
        depthA = {cx - depthPad, cy, cz};
        depthB = {cx + depthPad, cy, cz};
    }
    for (int di = 0; di < 2; ++di) {
        const W& p = (di == 0) ? depthA : depthB;
        float vx, vy, vz, vw;
        TransformPoint4(outView, p.x, p.y, p.z, 1.f, vx, vy, vz, vw);
        if (std::fabs(vw) > 1e-8f) {
            vx /= vw;
            vy /= vw;
            vz /= vw;
        }
        minvz = std::min(minvz, vz);
        maxvz = std::max(maxvz, vz);
        minvx = std::min(minvx, vx);
        maxvx = std::max(maxvx, vx);
        minvy = std::min(minvy, vy);
        maxvy = std::max(maxvy, vy);
    }

    const float pad = 1e-3f;
    OrthographicRHColMajor(minvx - pad, maxvx + pad, minvy - pad, maxvy + pad, minvz - pad, maxvz + pad, outProj);
}

void ComputeOrbitViewProjectionColMajor(const OrbitPanZoomState& state, float targetX, float targetY, float targetZ,
    float fovYDeg, float aspect, float zNear, float zFar, Mat4Col& outView, Mat4Col& outProj) {
    (void)zNear;
    (void)zFar;
    if (state.projection != OrbitProjectionMode::Perspective) {
        ComputeOrbitOrthoViewProjectionColMajor(state.projection, state, targetX, targetY, targetZ, aspect, zNear, zFar,
            outView, outProj);
        return;
    }
    const float yaw = state.yaw;
    const float pitch = state.pitch;
    const float dist = std::max(state.distance, 1e-4f);
    float fx = std::cos(pitch) * std::sin(yaw);
    float fy = std::sin(pitch);
    float fz = std::cos(pitch) * std::cos(yaw);
    float eyeX = targetX - fx * dist;
    float eyeY = targetY - fy * dist;
    float eyeZ = targetZ - fz * dist;
    float tx = targetX + state.pan_x;
    float ty = targetY + state.pan_y;
    float tz = targetZ;
    LookAtRHColMajor(eyeX, eyeY, eyeZ, tx, ty, tz, 0.f, 1.f, 0.f, outView);
    PerspectiveRHColMajor(fovYDeg * 0.017453292f, std::max(aspect, 1e-4f), zNear, zFar, outProj);
}

bool InverseMat4(const Mat4Col& m, Mat4Col& invOut) {
    float a[4][8]{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            a[row][col] = m[col * 4 + row];
        }
        a[row][row + 4] = 1.f;
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        float maxAbs = std::fabs(a[col][col]);
        for (int row = col + 1; row < 4; ++row) {
            const float v = std::fabs(a[row][col]);
            if (v > maxAbs) {
                maxAbs = v;
                pivot = row;
            }
        }
        if (maxAbs < 1e-12f) {
            return false;
        }
        if (pivot != col) {
            for (int j = 0; j < 8; ++j) {
                std::swap(a[col][j], a[pivot][j]);
            }
        }
        const float invPivot = 1.f / a[col][col];
        for (int j = 0; j < 8; ++j) {
            a[col][j] *= invPivot;
        }
        for (int row = 0; row < 4; ++row) {
            if (row == col) {
                continue;
            }
            const float f = a[row][col];
            for (int j = 0; j < 8; ++j) {
                a[row][j] -= f * a[col][j];
            }
        }
    }
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            invOut[col * 4 + row] = a[row][col + 4];
        }
    }
    return true;
}

bool ScreenToWorldRay(const Mat4Col& view, const Mat4Col& proj, const ImVec2& panel_min, const ImVec2& panel_max,
    const ImVec2& screen_pos, float& originX, float& originY, float& originZ, float& dirX, float& dirY, float& dirZ) {
    const float pw = panel_max.x - panel_min.x;
    const float ph = panel_max.y - panel_min.y;
    if (pw <= 1e-6f || ph <= 1e-6f) {
        return false;
    }
    const float ndc_x = 2.f * (screen_pos.x - panel_min.x) / pw - 1.f;
    const float ndc_y = 1.f - 2.f * (screen_pos.y - panel_min.y) / ph;

    Mat4Col vp = MultiplyMat4(proj, view);
    Mat4Col invVp{};
    if (!InverseMat4(vp, invVp)) {
        return false;
    }

    float p0x, p0y, p0z, p0w;
    float p1x, p1y, p1z, p1w;
    TransformPoint4(invVp, ndc_x, ndc_y, -1.f, 1.f, p0x, p0y, p0z, p0w);
    TransformPoint4(invVp, ndc_x, ndc_y, 1.f, 1.f, p1x, p1y, p1z, p1w);
    if (std::fabs(p0w) < 1e-12f || std::fabs(p1w) < 1e-12f) {
        return false;
    }
    p0x /= p0w;
    p0y /= p0w;
    p0z /= p0w;
    p1x /= p1w;
    p1y /= p1w;
    p1z /= p1w;
    float dx = p1x - p0x;
    float dy = p1y - p0y;
    float dz = p1z - p0z;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-12f) {
        return false;
    }
    originX = p0x;
    originY = p0y;
    originZ = p0z;
    dirX = dx / len;
    dirY = dy / len;
    dirZ = dz / len;
    return true;
}

bool IntersectRayPlane(float originX, float originY, float originZ, float dirX, float dirY, float dirZ, float planeNx,
    float planeNy, float planeNz, float planeD, float& outT) {
    const float denom = planeNx * dirX + planeNy * dirY + planeNz * dirZ;
    if (std::fabs(denom) < 1e-12f) {
        return false;
    }
    const float t = (planeD - (planeNx * originX + planeNy * originY + planeNz * originZ)) / denom;
    outT = t;
    return true;
}

bool ScreenToXZPlane(const Mat4Col& view, const Mat4Col& proj, const ImVec2& panel_min, const ImVec2& panel_max,
    const ImVec2& screen_pos, float planeY, float& outX, float& outZ) {
    float ox, oy, oz, dx, dy, dz;
    if (!ScreenToWorldRay(view, proj, panel_min, panel_max, screen_pos, ox, oy, oz, dx, dy, dz)) {
        return false;
    }
    float t = 0.f;
    if (!IntersectRayPlane(ox, oy, oz, dx, dy, dz, 0.f, 1.f, 0.f, planeY, t)) {
        return false;
    }
    outX = ox + t * dx;
    outZ = oz + t * dz;
    return true;
}

bool ScreenToWorldOnPlane(const Mat4Col& view, const Mat4Col& proj, const ImVec2& panel_min,
    const ImVec2& panel_max, const ImVec2& screen_pos, float planeNx, float planeNy, float planeNz, float planeD,
    float& outX, float& outY, float& outZ) {
    float ox, oy, oz, dx, dy, dz;
    if (!ScreenToWorldRay(view, proj, panel_min, panel_max, screen_pos, ox, oy, oz, dx, dy, dz)) {
        return false;
    }
    float t = 0.f;
    if (!IntersectRayPlane(ox, oy, oz, dx, dy, dz, planeNx, planeNy, planeNz, planeD, t)) {
        return false;
    }
    if (t < 0.f) {
        return false;
    }
    outX = ox + t * dx;
    outY = oy + t * dy;
    outZ = oz + t * dz;
    return true;
}

bool WorldDeltaOnHorizontalPlane(const Mat4Col& view, const Mat4Col& proj, const ImVec2& panel_min,
    const ImVec2& panel_max, const ImVec2& prev_screen, const ImVec2& cur_screen, float planeY, float& outDeltaX,
    float& outDeltaZ) {
    float px, pz, cx, cz;
    if (!ScreenToXZPlane(view, proj, panel_min, panel_max, prev_screen, planeY, px, pz)) {
        return false;
    }
    if (!ScreenToXZPlane(view, proj, panel_min, panel_max, cur_screen, planeY, cx, cz)) {
        return false;
    }
    outDeltaX = cx - px;
    outDeltaZ = cz - pz;
    return true;
}

bool IntersectRayAxisAlignedBox(float originX, float originY, float originZ, float dirX, float dirY, float dirZ,
    float bminX, float bminY, float bminZ, float bmaxX, float bmaxY, float bmaxZ, float& outT) {
    float t0 = 0.f;
    float t1 = 1.0e30f;
    const float eps = 1e-8f;

    auto slab = [&](float o, float d, float bmin, float bmax) -> bool {
        if (std::fabs(d) < eps) {
            if (o < bmin || o > bmax) {
                return false;
            }
            return true;
        }
        const float inv = 1.f / d;
        float ta = (bmin - o) * inv;
        float tb = (bmax - o) * inv;
        if (ta > tb) {
            std::swap(ta, tb);
        }
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        return true;
    };

    if (!slab(originX, dirX, bminX, bmaxX)) {
        return false;
    }
    if (!slab(originY, dirY, bminY, bmaxY)) {
        return false;
    }
    if (!slab(originZ, dirZ, bminZ, bmaxZ)) {
        return false;
    }

    if (t0 > t1 || t1 < 0.f) {
        return false;
    }
    outT = (t0 < 0.f) ? 0.f : t0;
    return true;
}

static bool ProjectToScreen(const Mat4Col& vp, float wx, float wy, float wz, const ImVec2& panel_min,
    const ImVec2& panel_max, ImVec2& out) {
    float cx, cy, cz, cw;
    TransformPoint4(vp, wx, wy, wz, 1.f, cx, cy, cz, cw);
    if (std::fabs(cw) < 1e-6f) {
        return false;
    }
    float ndcX = cx / cw;
    float ndcY = cy / cw;
    float ndcZ = cz / cw;
    (void)ndcZ;
    if (ndcZ < -1.f || ndcZ > 1.f) {
        return false;
    }
    float pw = panel_max.x - panel_min.x;
    float ph = panel_max.y - panel_min.y;
    out.x = panel_min.x + (ndcX * 0.5f + 0.5f) * pw;
    out.y = panel_min.y + (1.f - (ndcY * 0.5f + 0.5f)) * ph;
    return true;
}

bool WorldToScreen(const Mat4Col& view, const Mat4Col& proj, float wx, float wy, float wz,
    const ImVec2& panel_min, const ImVec2& panel_max, ImVec2& out) {
    Mat4Col vp = MultiplyMat4(proj, view);
    return ProjectToScreen(vp, wx, wy, wz, panel_min, panel_max, out);
}

void DrawWorldCrossXZ(ImDrawList* dl, const ImVec2& panel_min, const ImVec2& panel_max, const Mat4Col& view,
    const Mat4Col& proj, float x, float y, float z, float extent, ImU32 col) {
    if (!dl || extent <= 0.f) {
        return;
    }
    Mat4Col vp = MultiplyMat4(proj, view);
    ImVec2 a, b, c, d;
    if (ProjectToScreen(vp, x - extent, y, z, panel_min, panel_max, a)
        && ProjectToScreen(vp, x + extent, y, z, panel_min, panel_max, b)) {
        dl->AddLine(a, b, col, 2.f);
    }
    if (ProjectToScreen(vp, x, y, z - extent, panel_min, panel_max, c)
        && ProjectToScreen(vp, x, y, z + extent, panel_min, panel_max, d)) {
        dl->AddLine(c, d, col, 2.f);
    }
}

void DrawXZGrid(ImDrawList* dl, const ImVec2& panel_min, const ImVec2& panel_max, const Mat4Col& view,
    const Mat4Col& proj, float cellWorld, ImU32 color, int halfCount) {
    if (!dl || cellWorld <= 1e-6f) {
        return;
    }
    Mat4Col vp = MultiplyMat4(proj, view);
    dl->PushClipRect(panel_min, panel_max, true);
    const int n = std::max(1, std::min(halfCount, 256));
    for (int i = -n; i <= n; ++i) {
        float x = static_cast<float>(i) * cellWorld;
        ImVec2 a, b;
        if (ProjectToScreen(vp, x, 0.f, static_cast<float>(-n) * cellWorld, panel_min, panel_max, a)
            && ProjectToScreen(vp, x, 0.f, static_cast<float>(n) * cellWorld, panel_min, panel_max, b)) {
            dl->AddLine(a, b, color, 1.f);
        }
    }
    for (int i = -n; i <= n; ++i) {
        float z = static_cast<float>(i) * cellWorld;
        ImVec2 a, b;
        if (ProjectToScreen(vp, static_cast<float>(-n) * cellWorld, 0.f, z, panel_min, panel_max, a)
            && ProjectToScreen(vp, static_cast<float>(n) * cellWorld, 0.f, z, panel_min, panel_max, b)) {
            dl->AddLine(a, b, color, 1.f);
        }
    }
    dl->PopClipRect();
}

} // namespace LibUI::Viewport

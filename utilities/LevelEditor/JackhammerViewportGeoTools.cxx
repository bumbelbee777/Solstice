#include "JackhammerViewportGeoTools.hxx"

#include "JackhammerMeshOps.hxx"

#include <imgui.h>

#include <cmath>
#include <cstdio>

namespace Jackhammer::ViewportGeo {

namespace {

void SnapXz(float& x, float& z, float g) {
    if (g > 1.0e-6f) {
        x = std::round(x / g) * g;
        z = std::round(z / g) * g;
    }
}

} // namespace

float MeasureWorldDistanceOrNeg(const MeasureState& s) {
    if (!s.hasA || !s.hasB) {
        return -1.f;
    }
    const float dx = s.bx - s.ax;
    const float dy = s.by - s.ay;
    const float dz = s.bz - s.az;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void DrawMeasureOverlay(ImDrawList* drawList, const LibUI::Viewport::Mat4Col& viewM, const LibUI::Viewport::Mat4Col& projM, ImVec2 panelMin, ImVec2 panelMax, const MeasureState& m) {
    if (!drawList || !m.hasA) {
        return;
    }
    ImVec2 sa{}, sb{};
    if (LibUI::Viewport::WorldToScreen(viewM, projM, m.ax, m.ay, m.az, panelMin, panelMax, sa)) {
        drawList->AddCircleFilled(sa, 5.f, IM_COL32(80, 220, 120, 255), 12);
    }
    if (m.hasB) {
        if (LibUI::Viewport::WorldToScreen(viewM, projM, m.bx, m.by, m.bz, panelMin, panelMax, sb)) {
            drawList->AddCircleFilled(sb, 5.f, IM_COL32(120, 200, 255, 255), 12);
        }
        if (LibUI::Viewport::WorldToScreen(viewM, projM, m.ax, m.ay, m.az, panelMin, panelMax, sa)
            && LibUI::Viewport::WorldToScreen(viewM, projM, m.bx, m.by, m.bz, panelMin, panelMax, sb)) {
            drawList->AddLine(sa, sb, IM_COL32(255, 255, 120, 220), 2.f);
        }
    }
    if (m.hasA && m.hasB) {
        const float d = MeasureWorldDistanceOrNeg(m);
        char buf[96];
        std::snprintf(buf, sizeof(buf), " dist %.4f  Δx %.3f  Δy %.3f  Δz %.3f", d, m.bx - m.ax, m.by - m.ay, m.bz - m.az);
        drawList->AddText(ImVec2(panelMin.x + 6.f, panelMin.y + 6.f), IM_COL32(255, 255, 200, 255), buf);
    }
}

void ProcessMeasureClicks(
    int geoToolIndex, const ImGuiIO& io, bool engineViewportHovered, const LibUI::Viewport::Mat4Col& viewM, const LibUI::Viewport::Mat4Col& projM,
    ImVec2 panelMin, ImVec2 panelMax, float groundPlaneY, float placeGridSnap, MeasureState& m, std::string& status) {
    if (geoToolIndex != 2 || !engineViewportHovered) {
        return;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyCtrl) {
        float hitX = 0.f, hitZ = 0.f;
        if (LibUI::Viewport::ScreenToXZPlane(
                viewM, projM, panelMin, panelMax, ImGui::GetMousePos(), groundPlaneY, hitX, hitZ)) {
            SnapXz(hitX, hitZ, placeGridSnap);
            if (!m.hasA) {
                m.ax = hitX;
                m.ay = groundPlaneY;
                m.az = hitZ;
                m.hasA = true;
                m.hasB = false;
                status = "Measure: A set (set B on next click, or use Reset in panel).";
            } else if (!m.hasB) {
                m.bx = hitX;
                m.by = groundPlaneY;
                m.bz = hitZ;
                m.hasB = true;
                const float d = MeasureWorldDistanceOrNeg(m);
                char buf[160];
                std::snprintf(buf, sizeof(buf), "Measure: B set — dist %.4f wu.", d);
                status = buf;
            } else {
                m.ax = hitX;
                m.ay = groundPlaneY;
                m.az = hitZ;
                m.hasB = false;
                status = "Measure: reset B; A moved.";
            }
        }
    }
}

void ProcessTerrainSculpt(
    int geoToolIndex, const ImGuiIO& io, bool engineViewportHovered, const LibUI::Viewport::Mat4Col& viewM, const LibUI::Viewport::Mat4Col& projM, ImVec2 panelMin, ImVec2 panelMax, float groundPlaneY, float placeGridSnap, float brushRadiusWorld, float raisePerFrame,
    Jackhammer::MeshOps::JhTriangleMesh& mesh, std::string& status) {
    if (geoToolIndex != 3 || !engineViewportHovered || brushRadiusWorld <= 0.f || std::abs(raisePerFrame) < 1.0e-8f) {
        return;
    }
    if (mesh.positions.empty() || !ImGui::IsMouseDown(ImGuiMouseButton_Left) || io.KeyCtrl) {
        return;
    }
    float hitX = 0.f, hitZ = 0.f;
    if (!LibUI::Viewport::ScreenToXZPlane(
            viewM, projM, panelMin, panelMax, ImGui::GetMousePos(), groundPlaneY, hitX, hitZ)) {
        return;
    }
    SnapXz(hitX, hitZ, placeGridSnap);
    const float r2 = brushRadiusWorld * brushRadiusWorld;
    int changed = 0;
    for (auto& p : mesh.positions) {
        const float dx = p.x - hitX;
        const float dz = p.z - hitZ;
        if (dx * dx + dz * dz <= r2) {
            p.y += raisePerFrame;
            ++changed;
        }
    }
    if (changed > 0) {
        Jackhammer::MeshOps::RecalculateNormals(mesh);
        static int s_terrSt = 0;
        if ((++s_terrSt & 7) == 0) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Terrain: adjusted %d verts (mesh workshop).", changed);
            status = buf;
        }
    }
    (void)io;
}

} // namespace Jackhammer::ViewportGeo

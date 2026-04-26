#pragma once

#include "JackhammerMeshOps.hxx"

#include "LibUI/Viewport/ViewportMath.hxx"

#include <string>

struct ImDrawList;
struct ImGuiIO;
struct ImVec2;

namespace Jackhammer::ViewportGeo {

struct MeasureState {
    bool hasA{false};
    bool hasB{false};
    float ax{};
    float ay{};
    float az{};
    float bx{};
    float by{};
    float bz{};
};

/// Returns distance when both endpoints are set, else -1. Uses Euclidean distance in world space.
float MeasureWorldDistanceOrNeg(const MeasureState& s);

void DrawMeasureOverlay(
    ImDrawList* drawList, const LibUI::Viewport::Mat4Col& viewM, const LibUI::Viewport::Mat4Col& projM, ImVec2 panelMin, ImVec2 panelMax, const MeasureState& m);

void ProcessMeasureClicks(
    int geoToolIndex, // 2 = measure
    const ImGuiIO& io, bool engineViewportHovered, const LibUI::Viewport::Mat4Col& viewM, const LibUI::Viewport::Mat4Col& projM,
    ImVec2 panelMin, ImVec2 panelMax, float groundPlaneY, float placeGridSnap, MeasureState& m, std::string& status);

void ProcessTerrainSculpt(
    int geoToolIndex, // 3 = terrain
    const ImGuiIO& io, bool engineViewportHovered, const LibUI::Viewport::Mat4Col& viewM, const LibUI::Viewport::Mat4Col& projM,
    ImVec2 panelMin, ImVec2 panelMax, float groundPlaneY, float placeGridSnap, float brushRadiusWorld, float raisePerFrame,
    Jackhammer::MeshOps::JhTriangleMesh& mesh, std::string& status);

} // namespace Jackhammer::ViewportGeo

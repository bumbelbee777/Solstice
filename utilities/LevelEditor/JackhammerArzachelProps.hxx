#pragma once

#include "JackhammerMeshOps.hxx"

#include <cstddef>

namespace Jackhammer::ArzachelProps {

/// Same shapes as the Arzachel Polyhedra set, built in-process for the mesh workshop (no static Arzachel link; avoids
/// duplicate Core symbols with SolsticeEngine).
enum class PropKind {
    ArzachelCube,
    UvSphere,
    Icosphere,
    Cylinder,
    Torus,
    GroundPlane,
    Tetrahedron,
    SquarePyramid,
};

struct PropBuildParams {
    float uniformScale{1.f};
    int i0{12};
    int i1{8};
    int i2{2};
};

void AddArzachelProp(PropKind kind, const PropBuildParams& p, Jackhammer::MeshOps::JhTriangleMesh& out, char* statusLine,
    size_t statusLineMax);

} // namespace Jackhammer::ArzachelProps

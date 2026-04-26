#pragma once

#include <Smf/SmfTypes.hxx>

#include <cstdint>
#include <string>
#include <vector>

namespace Jackhammer::MeshOps {

using Solstice::Smf::SmfVec2;
using Solstice::Smf::SmfVec3;

/// Indexed triangle list (positions + triangle corner indices). Optional per-vertex normals / UVs
/// (same cardinality as ``positions`` when non-empty).
struct JhTriangleMesh {
    std::vector<SmfVec3> positions;
    std::vector<uint32_t> indices;
    std::vector<SmfVec3> normals;
    std::vector<SmfVec2> uvs;
};

enum class JhSewPositionPolicy : std::uint8_t {
    KeepA, ///< Keep vertex from the first chain after merge (canonical vertex id is min(a,b) per pair).
    KeepB,
    Midpoint,
};

/// Axis-aligned unit cube, 8 verts, 12 triangles (outward CCW per face in +X,+Y,+Z space).
void MakeUnitCube(JhTriangleMesh& out);

/// Two axis-aligned quads in XZ (Y=0 and Y=1), each split into two triangles, 8 verts / 4 tris.
/// Boundary edge loops are the four corners of each quad — useful to exercise ``SewBorderChains``.
void MakeTwoStackedSquaresDemo(JhTriangleMesh& out);

bool IsValidTriangleMesh(const JhTriangleMesh& m);

/// Replace all index occurrences of ``drop`` with ``keep``, remove vertex ``drop``, compact arrays.
/// Optional attributes (normals, uvs) shrink with the vertex array. ``keep`` must differ from ``drop``.
void MergeVertexPair(JhTriangleMesh& m, uint32_t keep, uint32_t drop);

/// Maya-style **sew**: pair-wise merge of boundary vertices. ``chainA`` and ``chainB`` must have the
/// same length; for each *i*, vertices ``chainA[i]`` and ``chainB[i]`` are merged (order-stable:
/// surviving index is the smaller of the two before each merge). Chains must describe vertices on the
/// mesh boundary (each interior step must be a **single** triangle edge on the boundary).
bool SewBorderChains(JhTriangleMesh& m, const std::vector<uint32_t>& chainA, const std::vector<uint32_t>& chainB,
    JhSewPositionPolicy policy, std::string* errOut);

/// Greedy weld: any two vertices closer than ``epsilon`` (world units) are merged (lower index wins).
/// Repeats until stable. Returns number of merge rounds that changed the mesh (0 = none).
std::uint32_t WeldVerticesByDistance(JhTriangleMesh& m, float epsilon);

void FlipTriangleWinding(JhTriangleMesh& m);

void RemoveDegenerateTriangles(JhTriangleMesh& m);

/// Per-vertex normals from adjacent face normals (area-weighted). Clears then fills ``m.normals``.
void RecalculateNormals(JhTriangleMesh& m);

/// Loop subdivision–style one step: each triangle split into four (midpoints on edges; shared midpoints).
void SubdivideTrianglesMidpoint(JhTriangleMesh& m);

/// Uniformly scale positions about the mesh centroid (for quick preview sizing).
void ScaleMeshAboutCentroid(JhTriangleMesh& m, float uniformScale);

/// Mild Laplacian smooth: each vertex moves toward the average of its **unique** edge neighbors.
/// ``factor`` in [0,1] blends old and neighbor average; ``iterations`` repeats (default workflow: 1–4).
void LaplacianSmoothUniform(JhTriangleMesh& m, float factor, int iterations);

/// Summarize triangle/vertex counts for UI.
void MeshCounts(const JhTriangleMesh& m, std::uint32_t& outVerts, std::uint32_t& outTris);

/// Axis-aligned box with faces outward (CCW from outside, +X/+Y/+Z right-handed), min < max.
void MakeAxisAlignedBox(JhTriangleMesh& out, const SmfVec3& aabbMin, const SmfVec3& aabbMax);

/// Right circular cylinder, Y-up, from y=0 to y=height, centered on xz origin.
void MakeCylinder(JhTriangleMesh& out, float radius, float height, int radialSegments, int heightSegments);

/// UV-sphere, center origin, radius ``radius``; rings include poles when latSegments >= 2.
void MakeSphere(JhTriangleMesh& out, float radius, int latSegments, int longSegments);

/// Standard torus in XZ, Y-up. Major loop in XZ, tube cross-section in the radial plane.
void MakeTorus(JhTriangleMesh& out, float majorRadius, float minorRadius, int majorSegments, int minorSegments);

/// Barrel arch: chord ``width`` on y=0, central angle ``curveDeg`` (5–180), extruded 0..depth in Z, scaled so sagitta matches ``height``.
void MakeArch(JhTriangleMesh& out, float width, float height, float depth, int alongArcSegments, float curveDeg);

// --- Sub-object editing (indexed mesh) ---

void SetVertexPosition(JhTriangleMesh& m, std::uint32_t vertex, const SmfVec3& p);

/// Removes one triangle; does not remove isolated vertices.
bool DeleteTriangle(JhTriangleMesh& m, std::uint32_t triIndex);

/// Collapse undirected edge (a,b) to the midpoint, merging vertices. Returns false if not an edge.
bool CollapseUndirectedEdge(JhTriangleMesh& m, std::uint32_t a, std::uint32_t b, std::string* errOut);

/// Insert midpoint on the edge, splitting each incident triangle. Internal edges (two tris) stay manifold.
bool SplitUndirectedEdge(JhTriangleMesh& m, std::uint32_t a, std::uint32_t b, std::string* errOut);

/// Tight world-space AABB of vertex positions. False if the mesh has no positions.
bool ComputeAxisAlignedBounds(const JhTriangleMesh& m, SmfVec3& outMin, SmfVec3& outMax);

/// Regular **XZ** heightfield, Y-up: spans ``[originX, originX+sizeX] × [originZ, originZ+sizeZ]`` with ``cellCountX`` / ``cellCountZ``
/// **quad** cells. All vertices start at Y = ``baseY`` (sculpt in the **Terrain** tool or move verts in the workshop).
void MakeHeightfieldGrid(JhTriangleMesh& out, float originX, float originZ, float sizeX, float sizeZ, int cellCountX, int cellCountZ,
    float baseY);

/// Snaps every vertex to the nearest world grid (same step on X, Y, Z). ``grid`` > 0 required.
void SnapAllVerticesToGrid(JhTriangleMesh& m, float grid);

/// Hammer-style **displacement** scaffold: a regular grid in **XZ** with ``(2^power+1)²`` vertices on plane ``Y = baseY``,
/// each quad split into two triangles (``power`` in 1‥4 → 2‥32 segments per side). Sculpt with the Terrain tool or sub-object.
void MakeHammerDisplacementPatch(
    JhTriangleMesh& out, float originX, float originZ, float sizeX, float sizeZ, float baseY, int power);

} // namespace Jackhammer::MeshOps

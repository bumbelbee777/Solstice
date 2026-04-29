#include "TestHarness.hxx"

#include <Render/Assets/MeshletBuilder.hxx>
#include <Render/Assets/Mesh.hxx>

static bool Run() {
    using namespace Solstice::Render;

    std::vector<QuantizedVertex> vertices;
    vertices.push_back(QuantizedVertex::FromFloat({0, 0, 0}, {0, 1, 0}, {0, 0}, {}, {}));
    vertices.push_back(QuantizedVertex::FromFloat({1, 0, 0}, {0, 1, 0}, {1, 0}, {}, {}));
    vertices.push_back(QuantizedVertex::FromFloat({0, 1, 0}, {0, 1, 0}, {0, 1}, {}, {}));
    vertices.push_back(QuantizedVertex::FromFloat({1, 1, 0}, {0, 1, 0}, {1, 1}, {}, {}));
    std::vector<uint32_t> indices{0, 1, 2, 1, 3, 2};

    MeshletBuilder builder;
    MeshletLibrary library = builder.BuildMeshlets(vertices, indices, 64, 6);
    SOLSTICE_TEST_ASSERT(!library.UniqueMeshlets.empty(), "meshlet list generated");
    SOLSTICE_TEST_ASSERT(library.UniqueMeshlets[0].IndexCount > 0, "meshlet index count");
    SOLSTICE_TEST_ASSERT(library.UniqueMeshlets[0].Radius >= 0.0f, "meshlet radius sane");
    SOLSTICE_TEST_PASS("Meshlet build + cull primitives");
    return true;
}

int main() {
    if (!Run()) {
        return 1;
    }
    return SolsticeTestMainResult("MeshletCullingTest");
}

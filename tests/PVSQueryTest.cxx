#include "TestHarness.hxx"

#include <Render/Scene/SpatialIndex.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>

static bool Run() {
    using namespace Solstice::Render;
    using namespace Solstice::Math;

    MeshLibrary meshLibrary;
    auto mesh = std::make_unique<Mesh>();
    mesh->BoundsMin = Vec3(-0.5f, -0.5f, -0.5f);
    mesh->BoundsMax = Vec3(0.5f, 0.5f, 0.5f);
    const uint32_t meshId = meshLibrary.AddMesh(std::move(mesh));

    Scene scene;
    scene.SetMeshLibrary(&meshLibrary);
    scene.AddObject(meshId, Vec3(0.0f, 0.0f, 0.0f));
    scene.UpdateTransforms();

    SpatialIndex spatial;
    spatial.SetScene(&scene);
    spatial.BuildPVS();
    std::vector<SceneObjectID> visible;
    Camera cam(Vec3(0.0f, 0.0f, 3.0f));
    spatial.QueryVisible(cam, visible);

    SOLSTICE_TEST_ASSERT(!visible.empty(), "spatial query returns visible object");
    SOLSTICE_TEST_PASS("PVS/spatial visibility query");
    return true;
}

int main() {
    if (!Run()) {
        return 1;
    }
    return SolsticeTestMainResult("PVSQueryTest");
}

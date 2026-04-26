#include "TestHarness.hxx"

#include <Core/AuthoringSkyboxBus.hxx>

#include <cmath>
#include <cstring>

static bool Run() {
    using namespace Solstice::Core;

    AuthoringSkyboxState a{};
    a.Enabled = true;
    a.Brightness = 1.25f;
    a.YawDegrees = 33.f;
    a.FacePaths[0] = "a/px.png";
    a.FacePaths[5] = "a/nz.png";
    PublishAuthoringSkyboxState(a);

    AuthoringSkyboxState g = GetAuthoringSkyboxState();
    SOLSTICE_TEST_ASSERT(g.Enabled, "sky enabled");
    SOLSTICE_TEST_ASSERT(g.Revision == 1, "revision 1");
    SOLSTICE_TEST_ASSERT(std::fabs(g.Brightness - 1.25f) < 1e-5f, "brightness");
    SOLSTICE_TEST_ASSERT(std::fabs(g.YawDegrees - 33.f) < 1e-5f, "yaw");
    SOLSTICE_TEST_ASSERT(g.FacePaths[0] == "a/px.png", "face0");
    SOLSTICE_TEST_ASSERT(g.FacePaths[5] == "a/nz.png", "face5");

    AuthoringSkyboxState b = a;
    b.Brightness = 0.5f;
    PublishAuthoringSkyboxState(b);
    g = GetAuthoringSkyboxState();
    SOLSTICE_TEST_ASSERT(g.Revision == 2, "revision 2");
    SOLSTICE_TEST_ASSERT(std::fabs(g.Brightness - 0.5f) < 1e-5f, "brightness 2");

    SOLSTICE_TEST_PASS("AuthoringSkyboxBus revision + fields");
    return true;
}

int main() {
    if (!Run()) {
        return 1;
    }
    return SolsticeTestMainResult("AuthoringSkyboxBusTest");
}

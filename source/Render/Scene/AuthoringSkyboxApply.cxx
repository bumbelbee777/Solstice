#include <Render/Scene/AuthoringSkyboxApply.hxx>

#include <Core/AuthoringSkyboxBus.hxx>
#include <Render/Scene/Skybox.hxx>

namespace Solstice::Render {

bool ApplyAuthoringSkyboxBusToSkybox(Skybox& sky, std::string* errOut) {
    Core::AuthoringSkyboxState s = Core::GetAuthoringSkyboxState();
    if (!s.Enabled) {
        return false;
    }
    if (sky.GetAppliedAuthoringRevision() == s.Revision) {
        return false;
    }
    std::string err;
    if (!sky.LoadImageCubemapFromFacePaths(s.FacePaths, s.Brightness, &err)) {
        if (errOut) {
            *errOut = err;
        }
        return false;
    }
    sky.SetAuthoringImageYawDegrees(s.YawDegrees);
    sky.SetAppliedAuthoringRevision(s.Revision);
    return true;
}

} // namespace Solstice::Render

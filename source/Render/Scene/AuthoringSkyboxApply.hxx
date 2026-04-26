#pragma once

#include <Solstice.hxx>
#include <string>

namespace Solstice::Render {

class Skybox;

/// Applies [`Core::GetAuthoringSkyboxState`] to `sky` when revision or paths change. Returns true if GPU cubemap was (re)built.
bool SOLSTICE_API ApplyAuthoringSkyboxBusToSkybox(Skybox& sky, std::string* errOut = nullptr);

} // namespace Solstice::Render

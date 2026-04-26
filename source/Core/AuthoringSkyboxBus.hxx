#pragma once

#include <Solstice.hxx>
#include <array>
#include <cstdint>
#include <string>

namespace Solstice::Core {

/// Thread-safe last-published six-face sky authoring from SMF / Parallax (no Render dependency).
struct SOLSTICE_API AuthoringSkyboxState {
    bool Enabled{false};
    float Brightness{1.f};
    float YawDegrees{0.f};
    std::array<std::string, 6> FacePaths{};
    /// Increments on each publish; consumers compare against last applied on `Skybox`.
    uint64_t Revision{0};
};

void SOLSTICE_API PublishAuthoringSkyboxState(const AuthoringSkyboxState& state);
AuthoringSkyboxState SOLSTICE_API GetAuthoringSkyboxState();

} // namespace Solstice::Core

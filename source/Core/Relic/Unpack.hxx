#pragma once

#include "Solstice.hxx"
#include <filesystem>

namespace Solstice::Core::Relic {

/// Extract each manifest asset to `outDir`. If `preferPathTableNames`, uses RELIC path table entries when
/// present and safe; otherwise files are named `{hash:016X}.bin`.
SOLSTICE_API bool UnpackRelic(const std::filesystem::path& relicPath, const std::filesystem::path& outDir,
    bool preferPathTableNames);

} // namespace Solstice::Core::Relic

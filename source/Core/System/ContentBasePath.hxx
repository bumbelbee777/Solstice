#pragma once

#include <Solstice.hxx>

#include <filesystem>

namespace Solstice::Core {

/// Directory containing the running executable (no trailing separator), or empty on failure.
/// Used so `game.data.relic` resolves next to the shipped `.exe` instead of the process CWD.
[[nodiscard]] SOLSTICE_API std::filesystem::path GetExecutableDirectory();

} // namespace Solstice::Core

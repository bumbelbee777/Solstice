#pragma once

namespace Solstice::Utilities {

/// Product version for Sharpon, Jackhammer (LevelEditor), and SMM (MovieMaker) — user-visible strings.
inline constexpr const char* kProductVersion = "1.0";
/// Tech preview release tag (UI / marketing).
inline constexpr const char* kReleaseTag = "TP1";

/// Single-line suffix, e.g. "v1.0 (TP1)".
inline constexpr const char* kVersionDisplaySuffix = "v1.0 (TP1)";

/// About-window headline (semver + release tag).
inline constexpr const char* kAboutHeadline = "1.0 (TP1)";

} // namespace Solstice::Utilities

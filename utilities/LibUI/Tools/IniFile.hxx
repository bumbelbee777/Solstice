#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace LibUI::Ini {

/// Line-oriented reader: `[section]`, `key=value`, `;` and `#` as comments (see below for `#` vs `#include`).
struct Document {
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> Sections;
};

struct ParseOptions {
    /// If true, a trailing `\` (after trim) continues the key/value line on the next physical line.
    bool lineContinuation{true};
    /// If true, `#include "file.ini"` / `@include "file"` lines are processed relative to the current file.
    bool allowInclude{true};
    int maxIncludeDepth{32};
};

/// Parses one `.ini` file. Merged keys from later lines or later `#include` order override earlier values in the
/// same section. `#include` is recognized **before** the generic `#` comment rule.
bool ParseFile(const std::filesystem::path& path, Document& out, std::string* errOut, const ParseOptions* options = nullptr);

const std::string* Get(const Document& doc, std::string_view section, std::string_view key);

} // namespace LibUI::Ini

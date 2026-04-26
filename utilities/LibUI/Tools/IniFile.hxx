#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace LibUI::Ini {

/// Minimal `.ini` reader: `[section]` headers, `key=value` lines, `#` / `;` line comments.
/// No escape rules beyond trimming surrounding whitespace on keys and values.
struct Document {
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> Sections;
};

bool ParseFile(const std::filesystem::path& path, Document& out, std::string* errOut);

const std::string* Get(const Document& doc, std::string_view section, std::string_view key);

} // namespace LibUI::Ini

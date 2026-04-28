#include "SmmTimelineRangePresets.hxx"

#include <LibUI/Tools/IniFile.hxx>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Smm::Timeline {
namespace {

static void ParseOneIniFile(const std::filesystem::path& path, std::vector<RangePreset>& out) {
    LibUI::Ini::Document doc;
    std::string err;
    if (!LibUI::Ini::ParseFile(path, doc, &err)) {
        return;
    }
    const std::string pfx = "TimelineRangePreset:";
    for (const auto& sec : doc.Sections) {
        if (sec.first.size() <= pfx.size() || sec.first.compare(0, pfx.size(), pfx) != 0) {
            continue;
        }
        const auto& m = sec.second;
        const auto take = [&](const char* k, const char* dflt) -> std::string {
            auto it = m.find(k);
            if (it != m.end()) {
                return it->second;
            }
            return dflt;
        };
        RangePreset p;
        p.Id = sec.first.substr(pfx.size());
        p.DisplayName = take("DisplayName", "");
        if (p.DisplayName.empty()) {
            p.DisplayName = p.Id;
        }
        p.Author = take("Author", "");
        p.Description = take("Description", "");
        p.Tags = take("Tags", "");
        try {
            p.StartTick = static_cast<uint64_t>(std::stoull(take("StartTick", "0")));
        } catch (...) {
            p.StartTick = 0;
        }
        try {
            p.EndTick = static_cast<uint64_t>(std::stoull(take("EndTick", "1")));
        } catch (...) {
            p.EndTick = 1;
        }
        if (p.EndTick <= p.StartTick) {
            p.EndTick = p.StartTick + 1;
        }
        out.push_back(std::move(p));
    }
}

} // namespace

void ScanRangePresetsFromRoots(const std::vector<std::filesystem::path>& roots, std::vector<RangePreset>& out) {
    out.clear();
    std::unordered_map<std::string, RangePreset> byId;
    for (const std::filesystem::path& root : roots) {
        if (root.empty()) {
            continue;
        }
        std::error_code ec;
        const std::filesystem::path sub = root / "Timeline";
        if (!std::filesystem::is_directory(sub, ec)) {
            continue;
        }
        try {
            for (const std::filesystem::directory_entry& e : std::filesystem::recursive_directory_iterator(sub)) {
                if (!e.is_regular_file()) {
                    continue;
                }
                if (e.path().extension() != ".ini") {
                    continue;
                }
                std::vector<RangePreset> chunk;
                ParseOneIniFile(e.path(), chunk);
                for (RangePreset& p : chunk) {
                    std::string id = p.Id;
                    byId[std::move(id)] = std::move(p);
                }
            }
        } catch (...) {
            continue;
        }
    }
    out.reserve(byId.size());
    for (auto& e : byId) {
        out.push_back(std::move(e.second));
    }
    std::sort(out.begin(), out.end(), [](const RangePreset& a, const RangePreset& b) { return a.Id < b.Id; });
}

} // namespace Smm::Timeline

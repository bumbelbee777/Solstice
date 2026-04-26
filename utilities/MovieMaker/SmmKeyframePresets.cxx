#include "SmmKeyframePresets.hxx"

#include <LibUI/Tools/IniFile.hxx>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace Smm::Keyframe {
namespace {

static bool ParseU8Clamped(const std::string& s, int maxVal, uint8_t& o) {
    int v = 0;
    try {
        v = std::stoi(s);
    } catch (...) {
        return false;
    }
    v = (std::clamp)(v, 0, maxVal);
    o = static_cast<uint8_t>(v);
    return true;
}

static void ParseOneIniFile(const std::filesystem::path& path, std::vector<KeyframeCurvePreset>& out) {
    LibUI::Ini::Document doc;
    std::string err;
    if (!LibUI::Ini::ParseFile(path, doc, &err)) {
        return;
    }
    const std::string pfx = "KeyframeCurvePreset:";
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
        KeyframeCurvePreset p;
        p.Id = sec.first.substr(pfx.size());
        p.DisplayName = take("DisplayName", "");
        if (p.DisplayName.empty()) {
            p.DisplayName = p.Id;
        }
        {
            std::string s = take("EaseIn", "0");
            if (!ParseU8Clamped(s, 13, p.EaseIn)) {
                p.EaseIn = 0;
            }
        }
        {
            int v = 255;
            try {
                v = std::stoi(take("EaseOut", "255"));
            } catch (...) {
            }
            v = (std::clamp)(v, 0, 255);
            p.EaseOut = static_cast<uint8_t>(v);
        }
        {
            uint8_t i = 0;
            (void)ParseU8Clamped(take("Interp", "0"), 3, i);
            p.Interp = i;
        }
        {
            const std::string a = take("TangentIn", "0.333333");
            p.TangentIn = std::strtof(a.c_str(), nullptr);
        }
        {
            const std::string a = take("TangentOut", "0.333333");
            p.TangentOut = std::strtof(a.c_str(), nullptr);
        }
        p.TangentIn = (std::clamp)(p.TangentIn, 0.02f, 0.99f);
        p.TangentOut = (std::clamp)(p.TangentOut, 0.02f, 0.99f);
        (void)path; // may tag source later
        out.push_back(std::move(p));
    }
}

} // namespace

void ScanCurvePresetsFromRoots(const std::vector<std::filesystem::path>& roots, std::vector<KeyframeCurvePreset>& out) {
    out.clear();
    for (const std::filesystem::path& root : roots) {
        if (root.empty()) {
            continue;
        }
        std::error_code ec;
        const std::filesystem::path kf = root / "Keyframe";
        if (!std::filesystem::is_directory(kf, ec)) {
            continue;
        }
        for (const std::filesystem::directory_entry& e : std::filesystem::directory_iterator(kf, ec)) {
            if (!e.is_regular_file()) {
                continue;
            }
            if (e.path().extension() != ".ini") {
                continue;
            }
            ParseOneIniFile(e.path(), out);
        }
    }
    std::sort(out.begin(), out.end(), [](const KeyframeCurvePreset& a, const KeyframeCurvePreset& b) { return a.Id < b.Id; });
    auto last = std::unique(out.begin(), out.end(), [](const KeyframeCurvePreset& a, const KeyframeCurvePreset& b) {
        return a.Id == b.Id;
    });
    out.erase(last, out.end());
}

} // namespace Smm::Keyframe

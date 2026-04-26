#include "IniFile.hxx"

#include <fstream>

namespace LibUI::Ini {
namespace {

static void TrimInPlace(std::string& s) {
    while (!s.empty() && (static_cast<unsigned char>(s.front()) <= 32)) {
        s.erase(s.begin());
    }
    while (!s.empty() && (static_cast<unsigned char>(s.back()) <= 32)) {
        s.pop_back();
    }
}

} // namespace

bool ParseFile(const std::filesystem::path& path, Document& out, std::string* errOut) {
    out.Sections.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (errOut) {
            *errOut = "Could not open: " + path.string();
        }
        return false;
    }
    std::string line;
    std::string curSection;
    while (std::getline(f, line)) {
        TrimInPlace(line);
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            curSection = line.substr(1, line.size() - 2);
            TrimInPlace(curSection);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        TrimInPlace(key);
        TrimInPlace(val);
        if (key.empty()) {
            continue;
        }
        out.Sections[curSection][std::move(key)] = std::move(val);
    }
    if (errOut) {
        errOut->clear();
    }
    return true;
}

const std::string* Get(const Document& doc, std::string_view section, std::string_view key) {
    const std::string sec(section);
    auto itS = doc.Sections.find(sec);
    if (itS == doc.Sections.end()) {
        return nullptr;
    }
    const std::string k(key);
    auto itK = itS->second.find(k);
    if (itK == itS->second.end()) {
        return nullptr;
    }
    return &itK->second;
}

} // namespace LibUI::Ini

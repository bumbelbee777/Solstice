#include "IniFile.hxx"

#include <fstream>
#include <istream>
#include <unordered_set>

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

static void MergeInto(Document& into, const Document& from) {
    for (const auto& sec : from.Sections) {
        for (const auto& kv : sec.second) {
            into.Sections[sec.first][kv.first] = kv.second;
        }
    }
}

static bool ReadLogicalLine(std::istream& in, std::string& out, const ParseOptions& opt) {
    out.clear();
    if (!opt.lineContinuation) {
        if (!std::getline(in, out)) {
            return false;
        }
        if (!out.empty() && out.back() == '\r') {
            out.pop_back();
        }
        return true;
    }
    std::string part;
    while (std::getline(in, part)) {
        if (!part.empty() && part.back() == '\r') {
            part.pop_back();
        }
        bool cont = !part.empty() && part.back() == '\\';
        if (cont) {
            part.pop_back();
            TrimInPlace(part);
            out.append(part);
            continue;
        }
        out.append(part);
        return true;
    }
    return !out.empty();
}

static bool IsIncludeLine(std::string& line, std::string& outPath) {
    TrimInPlace(line);
    if (line.size() < 10) {
        return false;
    }
    size_t pfxLen = 0;
    if (line.compare(0, 8, "#include") == 0) {
        pfxLen = 8;
    } else if (line.compare(0, 8, "@include") == 0) {
        pfxLen = 8;
    } else {
        return false;
    }
    size_t i = pfxLen;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
    }
    if (i >= line.size()) {
        return false;
    }
    const char a = line[i];
    if (a != '"' && a != '<') {
        return false;
    }
    const char close = (a == '"') ? '"' : '>';
    const size_t start = i + 1;
    const size_t j = line.find(close, start);
    if (j == std::string::npos) {
        return false;
    }
    outPath = line.substr(start, j - start);
    TrimInPlace(outPath);
    return !outPath.empty();
}

static bool ParseFileAt(const std::filesystem::path& path, Document& out, std::string* errOut, const ParseOptions& opt, int depth,
    std::unordered_set<std::string>& openStack);

static bool LoadInclude(
    const std::filesystem::path& baseFile, const std::string& rel, Document& out, std::string* errOut, const ParseOptions& opt, int depth,
    std::unordered_set<std::string>& openStack) {
    if (depth > opt.maxIncludeDepth) {
        if (errOut) {
            *errOut = "INI #include: max include depth (" + std::to_string(opt.maxIncludeDepth) + "): " + baseFile.string();
        }
        return false;
    }
    std::filesystem::path p = (baseFile.empty() || baseFile == "-") ? std::filesystem::path(rel) : (baseFile.parent_path() / rel);
    p = p.lexically_normal();
    return ParseFileAt(p, out, errOut, opt, depth, openStack);
}

static bool ParseFileAt(const std::filesystem::path& path, Document& out, std::string* errOut, const ParseOptions& opt, int depth,
    std::unordered_set<std::string>& openStack) {
    std::string canon;
    {
        std::error_code fe;
        if (!std::filesystem::is_regular_file(path, fe) || fe) {
            if (errOut) {
                *errOut = "INI: not a file: " + path.string();
            }
            return false;
        }
    }
    {
        std::error_code ec;
        const std::filesystem::path absP = std::filesystem::absolute(path, ec);
        const std::filesystem::path base = ec ? path : absP;
        std::filesystem::path n = std::filesystem::weakly_canonical(base, ec);
        canon = (ec ? base.lexically_normal() : n).string();
    }
    if (openStack.count(canon) != 0) {
        if (errOut) {
            *errOut = "INI: circular #include: " + path.string();
        }
        return false;
    }
    openStack.insert(canon);

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        openStack.erase(canon);
        if (errOut) {
            *errOut = "Could not open: " + path.string();
        }
        return false;
    }

    std::string curSection;
    std::string line;
    while (ReadLogicalLine(f, line, opt)) {
        TrimInPlace(line);
        if (line.empty()) {
            continue;
        }
        if (opt.allowInclude) {
            std::string inc;
            if (IsIncludeLine(line, inc)) {
                Document sub;
                if (!LoadInclude(path, inc, sub, errOut, opt, depth + 1, openStack)) {
                    openStack.erase(canon);
                    return false;
                }
                MergeInto(out, sub);
                continue;
            }
        }
        if (line[0] == ';') {
            continue;
        }
        if (line[0] == '#') {
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
    openStack.erase(canon);
    return true;
}

} // namespace

bool ParseFile(const std::filesystem::path& path, Document& out, std::string* errOut, const ParseOptions* options) {
    static const ParseOptions kDefault;
    const ParseOptions& opt = options ? *options : kDefault;
    out.Sections.clear();
    std::unordered_set<std::string> open;
    return ParseFileAt(path, out, errOut, opt, 0, open);
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
